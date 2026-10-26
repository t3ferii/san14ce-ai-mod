// Loads two real fixture DLLs against a synthetic image, never the live game.
#include "../src/split_module_api.h"
#undef SPLIT_MODULE_FIXTURE
#define TACTICS_FIXTURE
#include "../src/native_adapter.cpp"
static void *fixture_function(unsigned) {return nullptr;}
#include "../src/split_module_api.h"
static unsigned checks;
static void check(bool value,const char *label) {++checks;if(!value){fprintf(stderr,"%s\n",label);exit(1);}}
static unsigned expert_population_calls;
static unsigned expert_power_calls;
static float __fastcall mock_expert_power(Obj army,Obj city,int parts,int kind) {
    check(army==image+32 && city==image+48 && parts==7 && kind==9,
          "native unit power gate preserves both pointers and both integer arguments");
    ++expert_power_calls;return 123.75f;
}
static void __fastcall mock_expert_population(Obj layout,Obj second,Obj third,Obj fourth) {
    check(!layout && second==image+32 && third==image+48 && fourth==image+64,
          "expert list trampoline preserves all four native arguments");
    ++expert_population_calls;
}
static const SplitFixtureApi *combat_fixture;
static uint64_t __fastcall mock_departure(Obj,Obj,Obj) {
    check(combat_fixture->scope()==1,"base departure wrapper shares combat DLL TLS scope");return 77;
}
static void seed_pointers() {
    auto put=[](unsigned slot,unsigned original){*reinterpret_cast<Obj *>(image+slot)=image+original;};
    put(0x1458848,0x7DF540);put(0x1458830,0x7D9E50);
    put(0x14751D8,0x894420);put(0x14751C8,0x87E3D0);put(0x14751F0,0x8782E0);
    put(0x13AC588,0x2FB440);put(0x13AC578,0x256970);
    put(0x13AC5B0,0x258170);
    put(0x14AB2D0,0x2FB440);put(0x14AB2C0,0x8C6150);put(0x14AB2E8,0x8FD320);put(0x14AB548,0x8DF7E0);put(0x14AB510,0x5CC120);
    for(auto slot:{0x1344518u,0x1343C98u,0x134B4F8u,0x134AF00u,0x1346E70u,0x134C8F8u})
        put(slot,slot==0x1344518?0x201E0:slot==0x1343C98?0x1F3A0:slot==0x134B4F8?0x68CE0:slot==0x134AF00?0x61120:slot==0x1346E70?0x36320:0x7A110);
    put(0x13512A0,0x8E7C0);   // CVariableGetPersonInfoNode (combat: ai_max_aggression)
    put(0x134D248,0x7DC00);put(0x13517A8,0x91030);   // attack_recheck_fix nodes
    for(const auto &x:recruit_ui_links)put(x.slot,x.original);
    for(const auto &x:recruit_ai_links)put(x.slot,x.original);
    for(const auto &x:injury_links)put(x.slot,x.original);
    put(0x13491B0,0x535E0);put(0x134C6E8,0x333320);
    put(0x13E69A0,0x50B670);put(0x13E60F8,0x509580);put(0x13A6BB0,0x2390B0);
    put(0x1413600,0x908820);put(0x1413488,0x878810);
    put(0x141BE88,0x5A73A0);put(0x141BCC8,0x5B9110);
    // 2026-10-23 user: adjacency preset buttons follow native row teardown.
    put(0x141BCF8,0x5A7450);put(0x141BD20,0x6475C0);
    put(0x141BD18,0x5F12B0); // 2026-10-24: create registered preset nodes during native row init.
    put(0x14296B8,0x65E810);put(0x14296F0,0x65F890);put(0x1429798,0x65E8C0);
    put(0x1429728,0x65EDD0); // 2026-10-23: radio choices use list event 1.
    // User 2026-10-26: incoming diplomacy has its own compatibility boundary.
    for(const auto &s:diplomacy_intel::slots)put(s.rva,s.original);
    for(const auto &s:advisor_briefing::slots)put(s.rva,s.original);
}
static bool unified_slots_native() {
    return pointer(image,0x1458848)==image+0x7DF540 && pointer(image,0x1458830)==image+0x7D9E50 &&
           pointer(image,0x14751D8)==image+0x894420 && pointer(image,0x14751C8)==image+0x87E3D0 && pointer(image,0x14751F0)==image+0x8782E0 &&
           pointer(image,0x13AC588)==image+0x2FB440 && pointer(image,0x13AC578)==image+0x256970 && pointer(image,0x13AC5B0)==image+0x258170 &&
           image[0x2C0820]==0x40 && !memcmp(image+0x8DF900,"\xE8\x1B\xC8\xCE\xFF",5);
}
static bool unified_slots_owned() {
    return pointer(image,0x1458848)!=image+0x7DF540 && pointer(image,0x1458830)!=image+0x7D9E50 &&
           pointer(image,0x14751D8)!=image+0x894420 && pointer(image,0x14751C8)==image+0x87E3D0 && pointer(image,0x14751F0)!=image+0x8782E0 &&
           pointer(image,0x13AC588)!=image+0x2FB440 && pointer(image,0x13AC578)!=image+0x256970 && pointer(image,0x13AC5B0)!=image+0x258170;
}
int main(int argc,char **argv) {
    check(argc==2 || argc==3,"captured original text supplied");
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x29FF000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    check(image!=nullptr,"synthetic image allocated");
    FILE *file=nullptr;check(fopen_s(&file,argv[1],"rb")==0,"captured text opened");
    check(fread(image+0x1000,1,kTextSize,file)==kTextSize,"complete original text read");fclose(file);
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER *>(image);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+0x80);pe->Signature=IMAGE_NT_SIGNATURE;pe->OptionalHeader.SizeOfImage=0x29FF000;
    seed_pointers();check(code_ready(),"original captured text CRC matches");
    check(diplomacy_intel_guards_match(),"diplomacy original code, calls and all screen slots match");
    check(advisor_briefing_guards_match(),"adviser briefing original functions and exact informational slots match");
    for(const auto &g:diplomacy_intel::guards)for(unsigned offset:{0u,g.size-1}) {
        image[g.rva+offset]^=1;
        check(!diplomacy_intel_guards_match(),"foreign diplomacy code rejects its feature");
        image[g.rva+offset]^=1;
    }
    for(const auto &s:diplomacy_intel::slots) {
        image[s.rva]^=1;check(!diplomacy_intel_guards_match(),"foreign diplomacy slot rejected");image[s.rva]^=1;
    }
    check(diplomacy_intel_guards_match() && code_ready(),"diplomacy guard testing preserves original text");
    // User/Claude 2026-10-25: 183A30 per-segment path_fast and 184485 body patch.
    check(path_fast_guards_match(),"path fast entry, all helpers, buffer owners and debug address match");
    for(const auto &g:path_fast::guards)for(unsigned offset:{0u,g.size-1}) {
        image[g.rva+offset]^=1;
        check(!path_fast_guards_match(),"each changed path function independently rejects path fast");
        image[g.rva+offset]^=1;
    }
    // Simulate the already-published combat entry and distance kernel. Their
    // whole 16-byte atomic blocks must not invalidate convenience guards.
    for(unsigned entry:{0x1843E0u,0x2B0290u,0x2AA850u}) {
        unsigned char saved[16];memcpy(saved,image+entry,16);
        memset(image+entry,0xCC,16);
        check(path_fast_guards_match(),"existing combat/progress entry patches accepted");
        memcpy(image+entry,saved,16);
    }
    for(unsigned offset:{0u,1u}) {
        image[path_fast::clear_site+offset]^=1;
        check(!path_fast_guards_match(),"changed node clear site rejects only path_fast");
        check(area_index_guards_match(),"path clear mismatch preserves independent area guard");
        image[path_fast::clear_site+offset]^=1;
    }
    check(path_fast_guards_match() && code_ready(),"path guard checks preserve original code");
    // User decision 2026-10-25: area_index has an independent compatibility guard.
    check(area_index_guards_match(),"area index five fingerprints and three E8 callers match");
    for(const auto &g:area_index::guards)for(unsigned offset:{0u,g.size-1}) {
        image[g.rva+offset]^=1;
        check(!area_index_guards_match(),"each changed area function rejects its guard");
        image[g.rva+offset]^=1;
    }
    for(unsigned site:area_index::callers)for(unsigned offset:{0u,1u,4u}) {
        image[site+offset]^=1;
        check(!area_index_guards_match(),"each foreign area caller opcode or displacement rejected");
        image[site+offset]^=1;
    }
    check(area_index_guards_match() && code_ready(),"area guard checks leave original code intact");
    // User decision 2026-10-25: strategy_batch and its guard cases removed (screen corruption).
    HMODULE base=LoadLibraryW(L"SplitBaseFixture.dll"),combat=LoadLibraryW(L"SplitCombatFixture.dll");
    check(base && combat,"both real fixture DLLs loaded");
    auto getter=reinterpret_cast<GetSplitCombatApi>(GetProcAddress(combat,"SAN14CombatGetApi"));
    check(getter && getter(99)==nullptr,"unknown cross-module ABI rejected");
    const SplitCombatApi *api=getter(split_module_abi);check(api && api->size==sizeof(SplitCombatApi),"combat API size and version");
    using Getter=const SplitFixtureApi *(__cdecl *)();
    auto base_get=reinterpret_cast<Getter>(GetProcAddress(base,"SAN14SplitFixtureGetApi"));
    auto combat_get=reinterpret_cast<Getter>(GetProcAddress(combat,"SAN14SplitFixtureGetApi"));
    check(base_get && combat_get,"test-only fixture exports present");
    const SplitFixtureApi *base_api=base_get();combat_fixture=combat_get();
    // User decision 2026-10-25: append compat coverage; retain every strict assertion below.
    if(argc==3 && strcmp(argv[2],"--failure")) {
        const char *mode=argv[2];
        unsigned all=(1u<<(sizeof(decode_probes)/sizeof(decode_probes[0])))-1;
        CodeReadiness ready={};
        check(code_unpacked_ready(ready) && text_exact,"strict decoding remains immediate");
        for(const auto &p:decode_probes)check(!memcmp(image+p.rva,p.bytes,5),"each decoding probe matches captured current-build text");
        ready={};
        check(!code_sample_ready(ready,1,all>>4) && !code_sample_ready(ready,1,all>>4),"encrypted majority never passes readiness");
        ready={};unsigned threshold=all>>3;
        check(!code_sample_ready(ready,1,threshold),"three-quarter match requires a second poll");
        check(!code_sample_ready(ready,2,threshold),"changing CRC resets stability");
        check(!code_sample_ready(ready,2,all^7u),"changing match mask resets stability even at the same count");
        check(code_sample_ready(ready,2,all^7u) && !text_exact,"stable three-quarter match accepts compat");
        ready={};
        for(unsigned i=0;i<code_poll_limit;++i)check(!code_sample_ready(ready,1,0),"encrypted polling stays blocked until timeout");
        check(!code_sample_ready(ready,1,all) && !code_sample_ready(ready,1,all),"timeout cannot accept later decoded text");
        unsigned char originals[12];
        for(unsigned i=0;i<12;++i) {originals[i]=image[decode_probes[i].rva];if(i<4)image[decode_probes[i].rva]^=0xFF;}
        ready={};check(!code_unpacked_ready(ready),"actual encrypted image rejected");Sleep(250);
        check(!code_unpacked_ready(ready),"stable encrypted image still rejected");
        image[decode_probes[3].rva]=originals[3];ready={};
        check(!code_unpacked_ready(ready),"actual 9-of-12 image waits");Sleep(250);
        check(code_unpacked_ready(ready) && !text_exact,"actual stable 9-of-12 image accepted");
        for(unsigned i=0;i<12;++i)image[decode_probes[i].rva]=originals[i];
        image[0x1000]^=1; // unrelated decoded code change; all hook signatures remain original
        using Install=bool(__cdecl *)(Obj,const SplitCombatApi *);
        using Features=unsigned(__cdecl *)();using Disable=void(__cdecl *)(unsigned);
        auto install_fixture=reinterpret_cast<Install>(GetProcAddress(base,"SAN14SplitFixtureInstall"));
        auto features=reinterpret_cast<Features>(GetProcAddress(base,"SAN14SplitFixtureFeatures"));
        auto disable=reinterpret_cast<Disable>(GetProcAddress(base,"SAN14SplitFixtureDisable"));
        check(install_fixture && features && disable,"compat fixture controls found");
        bool unified=!strcmp(mode,"--unified"),merit=!strcmp(mode,"--merit"),save=!strcmp(mode,"--save");
        bool off=!strcmp(mode,"--off"),empty=!strcmp(mode,"--empty"),combat_guard=!strcmp(mode,"--combat-guard");
        bool base_failure=!strcmp(mode,"--base-failure"),combat_failure=!strcmp(mode,"--combat-failure");
        bool base_prepare=!strcmp(mode,"--base-prepare"),both=!strcmp(mode,"--both-failure");
        bool recheck=!strcmp(mode,"--recheck");
        bool briefing=!strcmp(mode,"--briefing"),briefing_guard=!strcmp(mode,"--briefing-guard");
        if(briefing || briefing_guard)disable(1|8); // 2026-10-26: HUD can serve briefing with unified personnel OFF.
        if(briefing_guard)image[0x930D20]^=1;
        if(unified || off || recheck)*reinterpret_cast<Obj *>(image+0x1458848)=image+0x1234;
        if(merit || off)image[0x232EE0]^=1;
        if(save)image[0x3F7E32]^=1;
        if(off)disable(3);
        if(empty)disable(4);
        if(combat_guard || both)image[0xD2486]^=1;
        if(base_failure)base_api->fail_after(3);
        if(combat_failure)combat_fixture->fail_after(1);
        if(base_prepare || both)base_api->fail_after(1000);
        if(recheck) {
            // First prepare with native pointers, then change one owner before publication.
            *reinterpret_cast<Obj *>(image+0x1458848)=image+0x7DF540;
            check(combat_fixture->prepare(image,nullptr) && base_api->prepare(image,api),"compat modules prepared for second guard pass");
            *reinterpret_cast<Obj *>(image+0x1458848)=image+0x1234;
            check(combat_fixture->publish() && base_api->publish(),"publish-time mismatch only disables its feature");
        } else check(install_fixture(image,api)!=both,"independent coordinator returns success iff at least one owner succeeds");
        bool combat_owned=image[0x1749A0]==0xE9;
        bool base_owned=image[0x2B07C0]==0xE9;
        if(briefing || briefing_guard) {
            check(!(features()&1),"briefing does not implicitly enable unified personnel");
            check((pointer(image,0x14751D8)!=image+0x894420)==briefing,"shared HUD enabled only for a compatible briefing");
            check(pointer(image,0x1458848)==image+0x7DF540 && pointer(image,0x13AC588)==image+0x2FB440,"briefing leaves unrelated personnel and tactic slots native");
            check(base_owned && combat_owned,"briefing guard failure preserves independent convenience and combat features");
        }
        check(combat_owned==!(combat_guard || combat_failure || both),"combat failure affects combat ownership only");
        check(base_owned==!(empty || base_failure || base_prepare || both),"convenience failure or empty set leaves combat intact");
        if(unified || off || recheck) {
            check(!(features()&1) && pointer(image,0x1458848)==image+0x1234 && pointer(image,0x14751D8)==image+0x894420,"foreign unified slot preserved; other unified slots skipped");
        }
        if(merit || off || save)check(!(features()&2) && image[0x232EE0]!=0xE9,"merit mismatch/disabled switch skips merit hooks");
        if(unified || merit || off || recheck)check((features()&28)==28 && base_owned && combat_owned,"unrelated convenience and combat features survive");
        if(merit || off)check(image[0x3F7E32]==0xE8 && pointer(image,0x13E69A0)!=image+0x50B670,"guarded save sidecar remains independent of disabled merit gameplay");
        if(save)check(image[0x3F7E32]!=0xE8 && pointer(image,0x13E69A0)==image+0x50B670 && base_owned && combat_owned,"bad save guard skips sidecar and preserves both owners");
        if(combat_failure || combat_guard)check(pointer(image,0x1344518)==image+0x201E0 && base_owned,"failed combat restores its own partial writes");
        if(base_failure || base_prepare)check(pointer(image,0x1346E70)==image+0x36320 && combat_owned,"failed convenience restores its own partial writes");
        base_api->rollback();combat_fixture->rollback();
        check(image[0x1749A0]!=0xE9 && image[0x2B07C0]!=0xE9,"each owner rolls back its own hooks");
        printf("{\"status\":\"split_compat_fixture_passed\",\"case\":\"%s\",\"checks\":%u,\"gameplay_verified\":false}\n",mode,checks);
        return 0;
    }
    check(combat_fixture->prepare(image,nullptr) && base_api->prepare(image,api),"both modules prepare against unchanged original code");
    check(code_ready(),"preparation does not mutate game code");
    check(combat_fixture->publish(),"combat hooks published");
    check(unified_slots_native(),"combat leaves all personnel HUD and player tactic selection slots native");
    check(pointer(image,0x1344518)!=image+0x201E0 && pointer(image,0x1346E70)==image+0x36320,"combat leaves domestic slots native");
    check(image[0x1749A0]==0xE9 && image[0x279190]==0xE9 && image[0x2B07C0]!=0xE9 && image[0x302980]!=0xE9,"combat owns damage observation and building, excludes adjacency and population");
    check(image[0x6566E0]==0x40 && image[0x2A8D40]==0x48 && image[0x173B44]==0xE8 &&
          pointer(image,0x14296B8)==image+0x65E810 && pointer(image,0x141BCF8)==image+0x5A7450 &&
          pointer(image,0x141BD20)==image+0x6475C0 && pointer(image,0x1429728)==image+0x65EDD0,
          "combat module leaves expert settings UI and input callbacks untouched");
    if(argc==3) {
        base_api->fail_after(3);
        check(!base_api->publish(),"failure after partial base publication reported");
        check(unified_slots_native(),"failed convenience publication restores personnel and tactic slots");
        check(image[0x2B07C0]!=0xE9 && image[0x295160]!=0xE9 && pointer(image,0x1346E70)==image+0x36320,"partially published base hooks rolled back");
        combat_fixture->rollback();check(code_ready(),"coordinated failure restores original complete code CRC");
        printf("{\"status\":\"split_module_failure_fixture_passed\",\"checks\":%u}\n",checks);return 0;
    }
    check(base_api->publish(),"other module publishes after combat without guard conflicts");
    check(unified_slots_owned(),"convenience exclusively owns personnel HUD and player tactic selection");
    check(image[0x8DF900]==0xE8 && memcmp(image+0x8DF900,"\xE8\x1B\xC8\xCE\xFF",5) && image[0x2C0820]==0xE9,"officer roster setup call and list builder entry published");
    check(pointer(image,0x1346E70)!=image+0x36320 && image[0x2B07C0]==0xE9 && image[0x302980]==0xE9,"other module owns domestic adjacency and population");
    check(image[0x1EC890]==0xE9 && image[0x295160]==0xE9 && pointer(image,0x1346AF8)!=image+0x34F10,"sortie plus independent injury protection coexist");
    check(image[0x6566E0]==0xE9 && image[0x6566E5]==0x90 && pointer(image,0x14296B8)!=image+0x65E810 &&
          pointer(image,0x141BCC8)!=image+0x5B9110 && pointer(image,0x141BCF8)!=image+0x5A7450 &&
          pointer(image,0x141BD20)!=image+0x6475C0 && pointer(image,0x1429728)!=image+0x65EDD0,
          "convenience alone publishes expert UI and 2026-10-23 preset selection/lifetime hooks");
    unsigned char expert_tail[32];memcpy(expert_tail,image+0x6566E6,sizeof(expert_tail));
    const unsigned char restore_pushes[]={0x41,0x5F,0x41,0x5E,0x5F};
    memcpy(image+0x6566E6,restore_pushes,sizeof(restore_pushes));
    absolute_jump(image+0x6566EB,reinterpret_cast<void *>(&mock_expert_population));
    reinterpret_cast<ExpertPopulate>(image+0x6566E0)(nullptr,image+32,image+48,image+64);
    check(expert_population_calls==1,"actual expert entry jump and relocated three-push gate return safely");
    memcpy(image+0x6566E6,expert_tail,sizeof(expert_tail));
    check(image[0x2A8D40]==0xE9 && image[0x2A8D45]==0x90 && image[0x2A8D46]==0x90,
          "convenience publishes the seven-byte native unit power entry guard");
    unsigned char power_tail[16];memcpy(power_tail,image+0x2A8D47,sizeof(power_tail));
    absolute_jump(image+0x2A8D47,reinterpret_cast<void *>(&mock_expert_power));
    check(reinterpret_cast<ExpertUnitPower>(image+0x2A8D40)(image+32,image+48,7,9)==123.75f && expert_power_calls==1,
          "actual native unit power entry and relocated home-space instructions return safely");
    memcpy(image+0x2A8D47,power_tail,sizeof(power_tail));
    // With policies still disabled, execute only our synthetic original tail.
    unsigned char original[16];memcpy(original,image+0x34F10,16);
    absolute_jump(image+0x34F10,reinterpret_cast<void *>(&mock_departure));
    check(base_api->departure()==77 && combat_fixture->scope()==0,"cross-DLL scope restored after original departure returns");
    memcpy(image+0x34F10,original,16);
    check(!base_api->publish() && !combat_fixture->publish(),"duplicate publication rejected");
    combat_fixture->rollback();
    check(unified_slots_owned(),"combat rollback preserves convenience personnel and tactic selection");
    check(pointer(image,0x1344518)==image+0x201E0 && pointer(image,0x13512A0)==image+0x8E7C0 && pointer(image,0x134D248)==image+0x7DC00 && pointer(image,0x13517A8)==image+0x91030 && image[0x1EC890]!=0xE9,"combat rollback restores its owned sites");
    check(pointer(image,0x1346E70)!=image+0x36320 && image[0x2B07C0]==0xE9,"combat rollback preserves other module");
    base_api->rollback();check(code_ready(),"combined rollback restores exact original text CRC");
    check(unified_slots_native(),"convenience rollback restores all four original personnel and tactic slots");
    check(pointer(image,0x1346AF8)==image+0x34F10 && pointer(image,0x1346E70)==image+0x36320,"other rollback restores injury and domestic slots");
    check(pointer(image,0x141BE88)==image+0x5A73A0 && pointer(image,0x141BCC8)==image+0x5B9110 &&
          pointer(image,0x14296B8)==image+0x65E810 && pointer(image,0x14296F0)==image+0x65F890 &&
          pointer(image,0x1429798)==image+0x65E8C0 && pointer(image,0x141BCF8)==image+0x5A7450 &&
          pointer(image,0x141BD20)==image+0x6475C0 && pointer(image,0x1429728)==image+0x65EDD0,
          "rollback restores every expert UI vtable including 2026-10-23 preset selection/lifetime slots");
    printf("{\"status\":\"split_module_fixture_passed\",\"checks\":%u,\"gameplay_verified\":false}\n",checks);
    // Modules remain loaded until process exit; no gate is unloaded in flight.
    return 0;
}
