// Loads two real fixture DLLs against a synthetic image, never the live game.
#include "../src/split_module_api.h"
#undef SPLIT_MODULE_FIXTURE
#define TACTICS_FIXTURE
#include "../src/native_adapter.cpp"
#define CCZ_GUARD_FIXTURE
#include "../src/native_ccz_bridge.inl"
#undef CCZ_GUARD_FIXTURE
// Exercise the actual speed frame wrapper through simulated native functions.
static Obj __fastcall mock_speed_manager();
static Obj __fastcall mock_speed_settings();
static int __fastcall mock_native_speed(Obj);
static void __fastcall mock_speed_frame(Obj);
static bool callsite_capture_active;
static void *callsite_capture_function(unsigned);
static void *fixture_function(unsigned rva) {
    if(callsite_capture_active)if(void *function=callsite_capture_function(rva))return function;
    switch(rva) {
        case 0x11040:return reinterpret_cast<void *>(&mock_speed_manager);
        case 0x3EC1A0:return reinterpret_cast<void *>(&mock_speed_settings);
        case 0x3EC3A0:return reinterpret_cast<void *>(&mock_native_speed);
        case 0x44DEE0:return reinterpret_cast<void *>(&mock_speed_frame);
        default:return nullptr;
    }
}
static unsigned speed_skip_logs,speed_log_reason,speed_log_delta;
static void speed_fixture_log(const char *kind,unsigned reason=0,unsigned delta=0,unsigned=0) {
    if(!strcmp(kind,"speed_second_skip")) {++speed_skip_logs;speed_log_reason=reason;speed_log_delta=delta;}
}
#pragma push_macro("log_event")
#undef log_event
#define log_event(...) speed_fixture_log(__VA_ARGS__)
#include "../src/native_speed_buttons.inl"
#pragma pop_macro("log_event")
#include "../src/split_module_api.h"
static unsigned checks;
static void check(bool value,const char *label) {++checks;if(!value){fprintf(stderr,"%s\n",label);exit(1);}}
static Obj speed_state_manager;
static unsigned speed_calls,speed_action;
static unsigned long long speed_pending_change;
static int speed_native=2;
static Obj __fastcall mock_speed_manager() {return speed_state_manager;}
static Obj __fastcall mock_speed_settings() {return nullptr;}
static int __fastcall mock_native_speed(Obj) {return speed_native;}
static void __fastcall mock_speed_frame(Obj speed_progress) {
    ++speed_calls;
    if(speed_calls!=1)return;
    if(speed_action==1 || speed_action==2) // pop/push both enqueue in manager+30
        *reinterpret_cast<unsigned long long *>(speed_state_manager+0x30)+=speed_pending_change;
    if(speed_action==3)*reinterpret_cast<Obj *>(pointer(speed_state_manager,0x20)+8)=nullptr;
    if(speed_action==4)*reinterpret_cast<unsigned long long *>(speed_state_manager+0x10)=0;
    if(speed_action==5)*reinterpret_cast<unsigned long long *>(speed_state_manager+0x30)-=1;
    if(speed_action==6)*reinterpret_cast<unsigned *>(speed_progress+0x484)=1;
    if(speed_action==7)speed_buttons::mode=0;
    if(speed_action==8)enabled=0;
    if(speed_action==9)speed_native=1;
    if(speed_action==10) { // 571EF0 yield: next 570840 update drains the queue before resuming.
        *reinterpret_cast<unsigned long long *>(speed_state_manager+0x30)=0;
        *reinterpret_cast<unsigned long long *>(speed_state_manager+0x30)+=1; // resumed frame queues pop
    }
}
static void speed_second_start_tests() {
    // User/Claude 2026-10-28: idle after a queued turn-end pop must not
    // restart CProgressState. 110E0 Obj(), 12540 +30 queue increment and
    // 570840's four-slot state+50 pool are verified in verify_speed_buttons.py.
    alignas(8) unsigned char state[0x50]={},speed_progress[0x500]={};
    Obj stack[2]={nullptr,speed_progress};speed_state_manager=state;
    const int saved_interval=*reinterpret_cast<int *>(image+speed_buttons::kInterval);
    const LONG saved_enabled=enabled;
    auto reset=[&](unsigned action,LONG mode=2) {
        memset(speed_progress,0,sizeof(speed_progress));stack[1]=speed_progress;
        *reinterpret_cast<unsigned long long *>(state+0x10)=2;
        *reinterpret_cast<Obj *>(state+0x20)=reinterpret_cast<Obj>(stack);
        *reinterpret_cast<unsigned long long *>(state+0x30)=0;
        *reinterpret_cast<int *>(image+speed_buttons::kInterval)=2;
        speed_buttons::mode=mode;speed_buttons::accumulator=0;
        speed_calls=0;speed_action=action;speed_pending_change=1;speed_native=2;enabled=1;
    };
    speed_buttons::second_skips[0]=speed_buttons::second_skips[1]=0;
    speed_buttons::second_skip_last=0;speed_skip_logs=0;
    for(LONG mode:{1L,2L}) {
        reset(1,mode);speed_buttons::frame(speed_progress);
        check(speed_calls==1,"queued turn-end pop blocks second native frame at x4 and x8");
        check(speed_buttons::accumulator==1.0,"pop skip keeps unused accumulator credit");
    }
    check(speed_skip_logs==1 && speed_log_reason==1 && speed_log_delta==1,"same pending skip reason logs once, with delta one");
    reset(2);speed_buttons::frame(speed_progress);
    check(speed_calls==1 && speed_buttons::accumulator==1.0,"queued push also blocks second frame and keeps credit");
    check(speed_buttons::second_skips[0]==3,"pending skip counter includes repeated pop and push");
    reset(0);speed_buttons::frame(speed_progress);
    // Claude review 2026-10-28 supersedes the old nonempty-queue allowance:
    // only an empty queue plus actual stack top may retain the second start.
    check(speed_calls==2 && speed_buttons::accumulator==0.0,"empty queue and actual stack top retain second start");
    check(speed_skip_logs==1 && speed_buttons::second_skip_last==0,"normal frame resets diagnostic reason without a skip log");
    reset(3);speed_buttons::frame(speed_progress);
    check(speed_calls==1 && speed_buttons::accumulator==1.0,"progress below stack top skips second start and keeps credit");
    check(speed_log_reason==2 && speed_log_delta==0 && speed_skip_logs==2,"top change logs its distinct reason");
    reset(4);speed_buttons::frame(speed_progress);
    check(speed_calls==1 && speed_buttons::second_skips[1]==2,"empty stack skips safely without count underflow");
    reset(1);speed_pending_change=1ull<<32;speed_buttons::frame(speed_progress);
    check(speed_calls==1 && speed_buttons::accumulator==1.0,"pending comparison uses all 64 bits");
    reset(5);*reinterpret_cast<unsigned long long *>(state+0x30)=2;speed_buttons::frame(speed_progress);
    check(speed_calls==1 && speed_buttons::accumulator==1.0,"decreased but still nonzero pending count blocks second start");
    // Carry the skipped credit into a lower-rate next frame (4/8 interval).
    speed_calls=0;speed_action=0;speed_buttons::mode=1;
    *reinterpret_cast<int *>(image+speed_buttons::kInterval)=8;
    speed_buttons::frame(speed_progress);
    check(speed_calls==1 && speed_buttons::accumulator==0.5,"skipped credit remains usable on the next frame");
    for(LONG mode:{1L,2L}) {
        reset(10,mode);*reinterpret_cast<unsigned long long *>(state+0x30)=1;
        speed_buttons::frame(speed_progress);
        check(speed_calls==1 && speed_buttons::accumulator==1.0,"yield then queue drain 1->0 and resumed pop 0->1 blocks second start at x4/x8");
        check(speed_log_reason==3 && speed_log_delta==0,"equal before/after counts are diagnostic only and do not permit restart");
    }
    reset(0);*reinterpret_cast<unsigned long long *>(state+0x30)=1;
    speed_buttons::frame(speed_progress);
    check(speed_calls==1 && speed_buttons::accumulator==1.0,"another state's unchanged pending work defers acceleration once");
    *reinterpret_cast<unsigned long long *>(state+0x30)=0;speed_calls=0;
    speed_buttons::frame(speed_progress);
    check(speed_calls==2 && speed_buttons::accumulator==0.0,"second start resumes normally after other state's work drains");
    reset(5);*reinterpret_cast<unsigned long long *>(state+0x30)=1;speed_buttons::frame(speed_progress);
    check(speed_calls==2 && speed_buttons::accumulator==0.0,"drained queue permits second start even when before differs");
    for(unsigned action:{6u,7u,8u,9u}) {
        reset(action);speed_buttons::frame(speed_progress);
        check(speed_calls==1 && speed_buttons::accumulator==1.0,"existing idle/mode/enabled/native-speed condition still blocks second start");
    }
    reset(0);speed_buttons::mode=0;speed_buttons::frame(speed_progress);
    check(speed_calls==1 && speed_buttons::accumulator==0.0,"native mode remains one unchanged frame");
    *reinterpret_cast<int *>(image+speed_buttons::kInterval)=saved_interval;
    enabled=saved_enabled;speed_buttons::mode=0;speed_buttons::accumulator=0;
    speed_state_manager=nullptr;
}
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
    put(terrain_trees::Slot,terrain_trees::UpdateRva);
    put(0x145B8D8,0x7E18E0);put(0x145B8C0,0x7DC200);
    put(0x1478268,0x896710);put(0x14751C8,0x87E3D0);put(0x1478280,0x87A690);
    put(0x13AF618,0x2FD830);put(0x13AF608,0x2583C0);
    put(0x13AF640,0x259B10);
    put(0x14AB2D0,0x2FD830);put(0x14AB2C0,0x8C6150);put(0x14AB2E8,0x8FD320);put(0x14AB548,0x8E1C70);put(0x14AB510,0x5CC120);
    for(auto slot:{0x1347518u,0x1346C98u,0x134E518u,0x134DF20u,0x1349E98u,0x134F918u})
        put(slot,slot==0x1347518?0x201C0:slot==0x1346C98?0x1F380:slot==0x134E518?0x69AB0:slot==0x134DF20?0x61C90:slot==0x1349E98?0x36E80:0x7B0B0);
    put(0x13542C0,0x8F810);   // CVariableGetPersonInfoNode (combat: ai_max_aggression)
    put(0x1350268,0x7EBA0);put(0x13547C8,0x92080);   // attack_recheck_fix nodes
    for(const auto &x:recruit_ui_links)put(x.slot,x.original);
    for(const auto &x:recruit_ai_links)put(x.slot,x.original);
    for(const auto &x:injury_links)put(x.slot,x.original);
    put(0x134C1D0,0x54250);put(0x134F708,0x3355A0);
    put(0x13E9A30,0x50DD30);put(0x13E9188,0x50BD20);put(0x13A9BF0,0x23AA60);
    put(0x1416690,0x90ACC0);put(0x1416518,0x87ABC0);
    put(0x141EF18,0x5A9950);put(0x141ED58,0x5BBA10);
    // 2026-10-23 user: adjacency preset buttons follow native row teardown.
    put(0x141ED88,0x5A9A60);put(0x141EDB0,0x649CD0);
    put(0x141BD18,0x5F3B10); // 2026-10-24: create registered preset nodes during native row init.
    put(0x142C748,0x660E20);put(0x142C780,0x661ED0);put(0x142C828,0x660F00);
    put(0x142C7B8,0x661470); // 2026-10-23: radio choices use list event 1.
    // User 2026-10-26: incoming diplomacy has its own compatibility boundary.
    for(const auto &s:diplomacy_intel::slots)put(s.rva,s.original);
    for(const auto &s:advisor_briefing::slots)put(s.rva,s.original);
}
static void chained_pointer_tests(HMODULE base) {
    (void)base;
    const unsigned slots[]={0x13AF640,0x13A9BF0}, originals[]={0x259B10,0x23AA60};
    for(unsigned i=0;i<2;++i) {
        auto slot=reinterpret_cast<void **>(image+slots[i]);void *foreign=image+originals[i]+0x10;
        *slot=foreign;
        check(!patch_pointer(slots[i],originals[i],image+0x100),"foreign owner rejects patch");
        check(*slot==foreign,"rejection preserves foreign owner");
        *slot=image+originals[i];
    }
}
static bool shared_slots_native() {
    return pointer(image,0x145B8D8)==image+0x7E18E0 && pointer(image,0x145B8C0)==image+0x7DC200 &&
           pointer(image,0x1478268)==image+0x896710 && pointer(image,0x14751C8)==image+0x87E3D0 && pointer(image,0x1478280)==image+0x87A690 &&
           pointer(image,0x13AF618)==image+0x2FD830 && pointer(image,0x13AF608)==image+0x2583C0 && pointer(image,0x13AF640)==image+0x259B10 &&
           image[0x2C2BA0]==0x40 && !memcmp(image+0x8E1D90,"\xE8\x4B\xCC\xCE\xFF",5);
}
static bool ccz_dialog_slots_owned() {
    return pointer(image,0x145B8D8)==image+0x7E18E0 && pointer(image,0x145B8C0)==image+0x7DC200 &&
           pointer(image,0x1478268)==image+0x896710 && pointer(image,0x14751C8)==image+0x87E3D0 && pointer(image,0x1478280)==image+0x87A690 &&
           pointer(image,0x13AF618)!=image+0x2FD830 && pointer(image,0x13AF608)!=image+0x2583C0 && pointer(image,0x13AF640)!=image+0x259B10;
}
static bool advisor_hud_owned() {
    return pointer(image,0x145B8D8)==image+0x7E18E0 && pointer(image,0x145B8C0)==image+0x7DC200 &&
           pointer(image,0x1478268)!=image+0x896710 && pointer(image,0x1478280)!=image+0x87A690 &&
           pointer(image,0x13AF618)==image+0x2FD830 && pointer(image,0x13AF608)==image+0x2583C0 && pointer(image,0x13AF640)==image+0x259B10;
}
// User/Claude 2026-10-29: tree slot/start mismatches isolate only placement edits.
static void terrain_trees_guard_tests() {
    check(terrain_trees_guards_match(),"tree slot and both native 16-byte starts match captured EXE");
    for(unsigned site:{terrain_trees::Slot,terrain_trees::UpdateRva,terrain_trees::SetterRva}) {
        unsigned length=site==terrain_trees::Slot?8:16;
        for(unsigned offset=0;offset<length;++offset) {
            image[site+offset]^=1;
            check(!terrain_trees_guards_match(),"every tree slot/start byte detects mismatch");
            image[site+offset]^=1;
        }
    }
    check(terrain_trees_guards_match() && code_ready(),"tree guard tests preserve native code and slot");
}
static HRESULT STDMETHODCALLTYPE terrain_foreign_texture(ID3D11Device *,const D3D11_TEXTURE2D_DESC *,const D3D11_SUBRESOURCE_DATA *,ID3D11Texture2D **) {return E_NOTIMPL;}
static void terrain_render_early_real_tests() {
    // User 2026-10-29 R1b: real windowless hardware probe in this fixture only.
    const LONG saved_enabled=enabled;auto saved_features=mod_features;
    enabled=1;mod_features.convenience=mod_features.terrain_edit=true;
    terrain_render::install_early();
    check(terrain_render::early_library && terrain_render::early_table && terrain_render_guards_match(),"real early system hardware device installs guarded slot before overlay");
    check(!terrain_render::slots[0].keep,"real early slot retains no temporary device");
    auto create=reinterpret_cast<decltype(&D3D11CreateDevice)>(GetProcAddress(terrain_render::early_library,"D3D11CreateDevice"));
    check(create!=nullptr,"early slot retains system implementation module");
    ID3D11Device *device=nullptr;
    check(SUCCEEDED(create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr)) && device,"second real hardware device created without a window or swapchain");
    auto table=*reinterpret_cast<void ***>(device);terrain_render::on_device(device);
    check(table[5]==reinterpret_cast<void *>(&terrain_render::create_texture_2d) && terrain_render_guards_match(),"real game-like hardware table reuses or joins early hook");
    unsigned owned=0;for(const auto &s:terrain_render::slots)if(s.published)++owned;
    terrain_render::on_device(device);
    unsigned after=0;for(const auto &s:terrain_render::slots)if(s.published)++after;
    check(owned==after,"repeated real-device observation does not publish another slot");
    terrain_render::restore();
    for(const auto &s:terrain_render::slots)if(s.table)check(s.table[5]==reinterpret_cast<void *>(s.original) && !s.keep,"real early and late tables restore without probe device references");
    device->Release();enabled=saved_enabled;mod_features=saved_features;
}
static void terrain_render_owner_tests(HMODULE base,HMODULE combat) {
    // User/Claude 2026-10-29: a real Windows WARP device, in this fixture process
    // only. No game process, original game calls, game memory or texture writes.
    using Test=unsigned(__cdecl *)(unsigned,ID3D11Device *);
    auto test=reinterpret_cast<Test>(GetProcAddress(base,"SAN14TerrainRenderFixture"));
    check(test && !GetProcAddress(combat,"SAN14TerrainRenderFixture"),"terrain device slot has convenience owner only");
    check(test(0,nullptr)==5,"registered runtime slot is CreateTexture2D index five");
    HMODULE d3d=LoadLibraryW(L"d3d11.dll");check(d3d!=nullptr,"Windows D3D11 loaded for isolated owner test");
    auto create=reinterpret_cast<decltype(&D3D11CreateDevice)>(GetProcAddress(d3d,"D3D11CreateDevice"));
    check(create!=nullptr,"D3D11 device entry found");ID3D11Device *device=nullptr;
    check(SUCCEEDED(create(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr)) && device,"isolated WARP device created");
    auto table=*reinterpret_cast<void ***>(device);void *original=table[5];
    check(test(1,device)==1 && table[5]!=original,"registered owner accepts original D3D11 implementation and bytes");
    void *hook=table[5];check(test(1,device)==1 && table[5]==hook,"same runtime table is not installed twice");
    DWORD prior=0;check(VirtualProtect(table+5,sizeof(void *),PAGE_READWRITE,&prior)!=0,"fixture slot writable");
    InterlockedExchangePointer(table+5,reinterpret_cast<void *>(&terrain_foreign_texture));
    DWORD ignored=0;VirtualProtect(table+5,sizeof(void *),prior,&ignored);
    check(test(2,device)==7,"foreign slot disables renderer only, preserving terrain edit and convenience");
    check(test(3,device)==1 && table[5]==reinterpret_cast<void *>(&terrain_foreign_texture),"split rollback preserves foreign runtime owner");
    check(VirtualProtect(table+5,sizeof(void *),PAGE_READWRITE,&prior)!=0,"fixture slot restoration writable");
    InterlockedExchangePointer(table+5,hook);VirtualProtect(table+5,sizeof(void *),prior,&ignored);
    // Re-arm only the ownership bit in the fixture DLL, then exercise rollback
    // of our actual hook. This is distinct from overwriting a foreign owner.
    check(test(4,device)==1 && table[5]==original,"split rollback restores its registered runtime hook");
    device->Release();FreeLibrary(d3d);
}
#include "callsite_compat_fixture.inl"
int main(int argc,char **argv) {
    check(argc==2 || argc==3,"captured original text supplied");
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x2A0B000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    check(image!=nullptr,"synthetic image allocated");
    FILE *file=nullptr;check(fopen_s(&file,argv[1],"rb")==0,"captured text opened");
    check(fread(image+0x1000,1,kTextSize,file)==kTextSize,"complete original text read");fclose(file);
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER *>(image);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+0x80);pe->Signature=IMAGE_NT_SIGNATURE;pe->OptionalHeader.SizeOfImage=0x2A0B000;
    seed_pointers();check(code_ready(),"original captured text CRC matches");
    callsite_sortie_tests();
    speed_second_start_tests();
    check(diplomacy_intel_guards_match(),"diplomacy original code, calls and all screen slots match");
    // User 2026-11-02: never place the deferred UI gate over a foreign tail.
    for(unsigned offset=0;offset<5;++offset) {
        image[diplomacy_intel::final_tail+offset]^=1;
        check(!diplomacy_intel_guards_match(),"changed final-tail instruction disables diplomacy feature");
        image[diplomacy_intel::final_tail+offset]^=1;
    }
    check(advisor_briefing_guards_match(),"adviser briefing original functions and exact informational slots match");
    // User 2026-11-02: friendly/ally rejection must use a verified native ABI.
    check(ccz_combat::guards_match(),"CCZ combat call and native relation helpers match");
    for(unsigned rva:{0x22E690u,0x2CC630u})for(unsigned offset:{0u,0x2Fu}) {
        image[rva+offset]^=1;check(!ccz_combat::guards_match(),"foreign relation helper prevents CCZ combat publication");image[rva+offset]^=1;
    }
    // User 2026-11-02: headless report ABI and its caller must fail closed.
    for(const auto &g:advisor_briefing::guards)for(unsigned offset:{0u,g.size-1}) {
        image[g.rva+offset]^=1;
        bool yielded_scene=(offset==0 && (g.rva==0x6C6B10 || g.rva==0x6C6DD0));
        check(advisor_briefing_guards_match()==yielded_scene,"only foreign scene-entry prologues yield; every other adviser guard remains strict");
        if(yielded_scene)check(advisor_briefing::scene_yield,"foreign scene prologue sets the scoped yield flag");
        image[g.rva+offset]^=1;
    }
    check(advisor_briefing_guards_match(),"scene guard mutation restores the original briefing guard state");
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
    // Simulate the already-published combat entry and distance kernel. Their five patched bytes (CodeLink keeps the other 11 bytes of the
    // atomic block; in 1a4a447d the distance guard of GA_2B255F starts at block offset 0xF) must not invalidate convenience guards.
    for(unsigned entry:{0x185710u,0x2B2550u,0x2ACB80u}) {
        unsigned char saved[16];memcpy(saved,image+entry,16);
        memset(image+entry,0xCC,5);
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
    terrain_trees_guard_tests();
    terrain_render_early_real_tests();
    HMODULE base=LoadLibraryW(L"SplitBaseFixture.dll"),combat=LoadLibraryW(L"SplitCombatFixture.dll");
    check(base && combat,"both real fixture DLLs loaded");
    chained_pointer_tests(base);
    terrain_render_owner_tests(base,combat);
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
        // W5 standalone-DLL scenarios: each root DLL (built without the other's modules since the W5 include lists) prepares, publishes and
        // rolls back ALONE, with nothing of the other DLL present (the loader passes no combat API when the combat DLL is absent).
        if(!strcmp(mode,"--combat-alone")) {
            check(combat_fixture->prepare(image,nullptr) && combat_fixture->publish(),"combat DLL prepares and publishes with no convenience DLL");
            check(image[0x175950]==0xE9 && image[0x27AAC0]==0xE9 && pointer(image,0x1347518)!=image+0x201C0,"combat DLL owns damage, build permission and the AI nodes alone");
            check(image[0x2B2A90]!=0xE9 && image[0x297070]!=0xE9 && image[0x304D70]!=0xE9 && image[0x658DF0]==0x40 && pointer(image,0x1349E98)==image+0x36E80,
                  "no convenience hook exists when the convenience DLL is absent");
            check(shared_slots_native(),"combat alone leaves every shared UI slot native");
            combat_fixture->rollback();
            check(image[0x175950]!=0xE9 && pointer(image,0x1347518)==image+0x201C0,"combat alone rolls back its own hooks");
            printf("{\"status\":\"split_compat_fixture_passed\",\"case\":\"%s\",\"checks\":%u,\"gameplay_verified\":false}\n",mode,checks);
            return 0;
        }
        if(!strcmp(mode,"--base-alone")) {
            check(base_api->prepare(image,nullptr) && base_api->publish(),"convenience DLL prepares and publishes with no combat DLL");
            check(image[0x2B2A90]==0xE9 && image[0x304D70]==0xE9 && image[0x297070]==0xE9 && pointer(image,0x1349E98)!=image+0x36E80,"convenience DLL owns adjacency, population and the domestic tick alone");
            check(image[0x175950]!=0xE9 && image[0x27AAC0]!=0xE9 && pointer(image,0x1347518)==image+0x201C0 && pointer(image,0x1346C98)==image+0x1F380,
                  "no combat hook exists when the combat DLL is absent");
            base_api->rollback();
            check(image[0x2B2A90]!=0xE9 && image[0x304D70]!=0xE9 && pointer(image,0x1349E98)==image+0x36E80,"convenience alone rolls back its own hooks");
            printf("{\"status\":\"split_compat_fixture_passed\",\"case\":\"%s\",\"checks\":%u,\"gameplay_verified\":false}\n",mode,checks);
            return 0;
        }
        bool ccz=!strcmp(mode,"--ccz"),ccz_guard=!strcmp(mode,"--ccz-guard"),merit=!strcmp(mode,"--merit"),save=!strcmp(mode,"--save");
        bool off=!strcmp(mode,"--off"),empty=!strcmp(mode,"--empty"),combat_guard=!strcmp(mode,"--combat-guard");
        bool base_failure=!strcmp(mode,"--base-failure"),combat_failure=!strcmp(mode,"--combat-failure");
        bool base_prepare=!strcmp(mode,"--base-prepare"),both=!strcmp(mode,"--both-failure");
        bool recheck=!strcmp(mode,"--recheck");
        // 2026-11-05 local3 (user: a refused optional feature switches only itself off): "--opt-<feature>" makes that ONE optional
        // publication step fail after it really wrote its hooks (fail_after(2000+step), src/native_split_install.inl). The module
        // must still publish, the failed feature must be taken back and off, every other feature must stay published and on.
        // Core failures stay whole-module rollbacks (--base-failure above: fail_after(3) fails core link 3).
        struct Optional {const char *mode;unsigned step,bit;};
        static const Optional optionals[]={{"--opt-merit",3,2},{"--opt-recruit",4,64},{"--opt-recruit-ai",6,1024},{"--opt-injury",7,128},
            {"--opt-awards",8,512},{"--opt-loyalty",9,256},{"--opt-expert",10,16}};
        int optional=-1;
        for(unsigned i=0;i<sizeof(optionals)/sizeof(optionals[0]);++i)if(!strcmp(mode,optionals[i].mode))optional=static_cast<int>(i);
        unsigned char loyalty_before[4];memcpy(loyalty_before,image+0x34CB96,sizeof(loyalty_before));
        bool briefing=!strcmp(mode,"--briefing"),briefing_guard=!strcmp(mode,"--briefing-guard"),briefing_pointer_guard=!strcmp(mode,"--briefing-pointer-guard");
        if(briefing || briefing_guard)disable(1|8); // 2026-10-26: HUD can serve briefing with unified personnel OFF.
        if(ccz || ccz_guard || recheck)disable(16);
        if(briefing_guard)image[0x9332B0]^=1;
        if(ccz_guard)*reinterpret_cast<Obj *>(image+0x13AF618)=image+0x1234;
        if(briefing_pointer_guard){disable(8);*reinterpret_cast<Obj *>(image+0x1478268)=image+0x1234;}
        if(merit || off)image[0x234880]^=1;
        if(save)image[0x3FA342]^=1;
        if(off)disable(3);
        if(empty)disable(4);
        if(combat_guard || both)image[0xD3516]^=1;
        if(base_failure)base_api->fail_after(3);
        if(optional>=0)base_api->fail_after(2000+optionals[optional].step);
        if(combat_failure)combat_fixture->fail_after(1);
        if(base_prepare || both)base_api->fail_after(1000);
        if(recheck) {
            // First prepare with native pointers, then change one owner before publication.
            *reinterpret_cast<Obj *>(image+0x13AF618)=image+0x2FD830;
            check(combat_fixture->prepare(image,nullptr) && base_api->prepare(image,api),"compat modules prepared for second guard pass");
            *reinterpret_cast<Obj *>(image+0x13AF618)=image+0x1234;
            check(combat_fixture->publish() && base_api->publish(),"publish-time mismatch only disables its feature");
        } else check(install_fixture(image,api)!=both,"independent coordinator returns success iff at least one owner succeeds");
        bool combat_owned=image[0x175950]==0xE9;
        // expert failure takes battle_rules (pair scope + adjacency, core link 1) back as one feature: its own marker is the domestic tick
        bool base_owned=image[optional==6?0x297070:0x2B2A90]==0xE9;
        if(briefing || briefing_guard || briefing_pointer_guard) {
            check((features()&1)==0,"user decision 2026-11-03: removed unified diagnostic bit remains zero");
            if(!briefing_pointer_guard)check((pointer(image,0x1478268)!=image+0x896710)==briefing,"advisor HUD enabled only for a compatible briefing");
            if(briefing)check(advisor_hud_owned(),"advisor briefing owns HUD slots without enabling CCZ dialogs or tactic selection");
            else if(briefing_pointer_guard)check(pointer(image,0x1478268)==image+0x1234 && (features()&32)==0,"user decision 2026-11-03: advisor HUD pointer mismatch preserves foreign owner and disables advisor only");
            else if(briefing_guard)check(pointer(image,0x1478268)==image+0x896710 && (features()&32)==0,"advisor byte guard disables only briefing and leaves HUD slots native");
            check(pointer(image,0x145B8D8)==image+0x7E18E0 && pointer(image,0x13AF618)==image+0x2FD830,"briefing leaves removed tactic and CCZ dialog slots native");
            check(base_owned && combat_owned,"briefing guard failure preserves independent convenience and combat features");
        }
        if(ccz || ccz_guard) {
            check((features()&1)==0,"user decision 2026-11-03: removed unified diagnostic bit remains zero");
            if(ccz)check(ccz_dialog_slots_owned(),"all CCZ shared dialog slots are owned only when ccz_sprites is enabled");
            if(ccz_guard)check(pointer(image,0x13AF618)==image+0x1234 && (features()&1)==0,"CCZ foreign pointer is preserved and ccz_sprites alone is disabled");
            check(pointer(image,0x1478268)==image+0x896710 && pointer(image,0x1478280)==image+0x87A690,"CCZ dialog owner does not claim advisor HUD");
            if(ccz_guard)check(base_owned && combat_owned,"CCZ pointer mismatch preserves independent convenience and combat features");
        }
        if(optional>=0) {
            // published(0..6): merit, recruit, recruit_ai, injury, awards, loyalty, expert hooks of the synthetic image
            auto published=[&](unsigned which)->bool {
                switch(which) {
                case 0:return image[0x234880]==0xE9;
                case 1:return pointer(image,0x1444070)!=image+0x6F3FF0;
                case 2:return pointer(image,0x134FE18)!=image+0x7C300;
                case 3:return pointer(image,0x1349B20)!=image+0x35A70;
                case 4:return pointer(image,0x134C1D0)!=image+0x54250;
                case 5:return memcmp(image+0x34CB96,loyalty_before,sizeof(loyalty_before))!=0;
                default:return image[0x658DF0]==0xE9;
                }
            };
            const unsigned flags=features();
            check(base_owned && combat_owned && image[0x304D70]==0xE9,"optional feature failure keeps the shared core links and the combat module published");
            for(unsigned which=0;which<7;++which) {
                // merit failure also takes the awards warehouse sidecar dependency off (awards need the merit save hosts)
                const bool expect_off=which==static_cast<unsigned>(optional) || (optional==0 && which==4);
                check(published(which)==!expect_off,expect_off?"failed optional feature is taken back":"other optional features stay published");
                check(((flags&optionals[which].bit)!=0)==!expect_off,expect_off?"failed optional feature is switched off":"other optional features stay on");
            }
            if(optional==0)check(pointer(image,0x13E9A30)==image+0x50DD30,"failed merit takes its save sidecar host back");
            if(optional==6)check(image[0x2B2A90]!=0xE9 && pointer(image,0x142C748)==image+0x660E20,"failed expert takes its core link and UI slots back");
        }
        check(combat_owned==!(combat_guard || combat_failure || both),"combat failure affects combat ownership only");
        check(base_owned==!(empty || base_failure || base_prepare || both),"convenience failure or empty set leaves combat intact");
        if(base_failure)check(image[0x297070]!=0xE9 && pointer(image,0x1349E98)==image+0x36E80,"core link failure rolls the whole convenience module back");
        if(ccz_guard || off || recheck) {
            check((features()&1)==0 && pointer(image,0x145B8D8)==image+0x7E18E0 && pointer(image,0x1478268)==image+0x896710,"removed tactic slots remain native and foreign CCZ/HUD owners are preserved");
        }
        if(merit || off || save)check(!(features()&2) && image[0x234880]!=0xE9,"merit mismatch/disabled switch skips merit hooks");
        if(ccz || ccz_guard || merit || off || recheck)check((features()&28)==28 && base_owned && combat_owned,"unrelated convenience and combat features survive");
        if(merit || off)check(image[0x3FA342]==0xE8 && pointer(image,0x13E9A30)!=image+0x50DD30,"guarded save sidecar remains independent of disabled merit gameplay");
        if(save)check(image[0x3FA342]!=0xE8 && pointer(image,0x13E9A30)==image+0x50DD30 && base_owned && combat_owned,"bad save guard skips sidecar and preserves both owners");
        if(combat_failure || combat_guard)check(pointer(image,0x1347518)==image+0x201C0 && base_owned,"failed combat restores its own partial writes");
        if(base_failure || base_prepare)check(pointer(image,0x1349E98)==image+0x36E80 && combat_owned,"failed convenience restores its own partial writes");
        base_api->rollback();combat_fixture->rollback();
        check(image[0x175950]!=0xE9 && image[0x2B2A90]!=0xE9,"each owner rolls back its own hooks");
        printf("{\"status\":\"split_compat_fixture_passed\",\"case\":\"%s\",\"checks\":%u,\"gameplay_verified\":false}\n",mode,checks);
        return 0;
    }
    check(combat_fixture->prepare(image,nullptr) && base_api->prepare(image,api),"both modules prepare against unchanged original code");
    check(code_ready(),"preparation does not mutate game code");
    check(combat_fixture->publish(),"combat hooks published");
    check(shared_slots_native(),"combat leaves advisor HUD, CCZ dialogs and removed tactic slots native");
    check(pointer(image,0x1347518)!=image+0x201C0 && pointer(image,0x1349E98)==image+0x36E80,"combat leaves domestic slots native");
    check(image[0x175950]==0xE9 && image[0x27AAC0]==0xE9 && image[0x2B2A90]!=0xE9 && image[0x304D70]!=0xE9,"combat owns damage observation and building, excludes adjacency and population");
    check(image[0x658DF0]==0x40 && image[0x2AAF30]==0x48 && image[0x174AF4]==0xE8 &&
          pointer(image,0x142C748)==image+0x660E20 && pointer(image,0x141ED88)==image+0x5A9A60 &&
          pointer(image,0x141EDB0)==image+0x649CD0 && pointer(image,0x142C7B8)==image+0x661470,
          "combat module leaves expert settings UI and input callbacks untouched");
    if(argc==3) {
        base_api->fail_after(3);
        check(!base_api->publish(),"failure after partial base publication reported");
        check(shared_slots_native(),"failed convenience publication restores advisor HUD, CCZ dialogs and removed tactic slots");
        check(image[0x2B2A90]!=0xE9 && image[0x297070]!=0xE9 && pointer(image,0x1349E98)==image+0x36E80,"partially published base hooks rolled back");
        combat_fixture->rollback();check(code_ready(),"coordinated failure restores original complete code CRC");
        printf("{\"status\":\"split_module_failure_fixture_passed\",\"checks\":%u}\n",checks);return 0;
    }
    check(base_api->publish(),"other module publishes after combat without guard conflicts");
    check(shared_slots_native(),"default feature set leaves optional HUD and CCZ dialog slots native");
    check(pointer(image,0x1349E98)!=image+0x36E80 && image[0x2B2A90]==0xE9 && image[0x304D70]==0xE9,"other module owns domestic adjacency and population");
    check(image[0x1EDC30]==0xE9 && image[0x297070]==0xE9 && pointer(image,0x1349B20)!=image+0x35A70,"sortie plus independent injury protection coexist");
    check(image[0x658DF0]==0xE9 && image[0x658DF5]==0x90 && pointer(image,0x142C748)!=image+0x660E20 &&
          pointer(image,0x141ED58)!=image+0x5BBA10 && pointer(image,0x141ED88)!=image+0x5A9A60 &&
          pointer(image,0x141EDB0)!=image+0x649CD0 && pointer(image,0x142C7B8)!=image+0x661470,
          "convenience alone publishes expert UI and 2026-10-23 preset selection/lifetime hooks");
    unsigned char expert_tail[32];memcpy(expert_tail,image+0x658DF6,sizeof(expert_tail));
    const unsigned char restore_pushes[]={0x41,0x5F,0x41,0x5E,0x5F};
    memcpy(image+0x658DF6,restore_pushes,sizeof(restore_pushes));
    absolute_jump(image+0x658DFB,reinterpret_cast<void *>(&mock_expert_population));
    reinterpret_cast<ExpertPopulate>(image+0x658DF0)(nullptr,image+32,image+48,image+64);
    check(expert_population_calls==1,"actual expert entry jump and relocated three-push gate return safely");
    memcpy(image+0x658DF6,expert_tail,sizeof(expert_tail));
    check(image[0x2AAF30]==0xE9 && image[0x2AAF35]==0x90 && image[0x2AAF36]==0x90,
          "convenience publishes the seven-byte native unit power entry guard");
    unsigned char power_tail[16];memcpy(power_tail,image+0x2AAF37,sizeof(power_tail));
    absolute_jump(image+0x2AAF37,reinterpret_cast<void *>(&mock_expert_power));
    check(reinterpret_cast<ExpertUnitPower>(image+0x2AAF30)(image+32,image+48,7,9)==123.75f && expert_power_calls==1,
          "actual native unit power entry and relocated home-space instructions return safely");
    memcpy(image+0x2AAF37,power_tail,sizeof(power_tail));
    // With policies still disabled, execute only our synthetic original tail.
    unsigned char original[16];memcpy(original,image+0x35A70,16);
    absolute_jump(image+0x35A70,reinterpret_cast<void *>(&mock_departure));
    check(base_api->departure()==77 && combat_fixture->scope()==0,"cross-DLL scope restored after original departure returns");
    memcpy(image+0x35A70,original,16);
    check(!base_api->publish() && !combat_fixture->publish(),"duplicate publication rejected");
    combat_fixture->rollback();
    check(shared_slots_native(),"combat rollback preserves convenience independence and removed feature slots");
    check(pointer(image,0x1347518)==image+0x201C0 && pointer(image,0x13542C0)==image+0x8F810 && pointer(image,0x1350268)==image+0x7EBA0 && pointer(image,0x13547C8)==image+0x92080 && image[0x1EDC30]!=0xE9,"combat rollback restores its owned sites");
    check(pointer(image,0x1349E98)!=image+0x36E80 && image[0x2B2A90]==0xE9,"combat rollback preserves other module");
    base_api->rollback();check(code_ready(),"combined rollback restores exact original text CRC");
    check(shared_slots_native(),"convenience rollback restores advisor HUD, CCZ dialogs and removed tactic slots");
    check(pointer(image,0x1349B20)==image+0x35A70 && pointer(image,0x1349E98)==image+0x36E80,"other rollback restores injury and domestic slots");
    check(pointer(image,0x141EF18)==image+0x5A9950 && pointer(image,0x141ED58)==image+0x5BBA10 &&
          pointer(image,0x142C748)==image+0x660E20 && pointer(image,0x142C780)==image+0x661ED0 &&
          pointer(image,0x142C828)==image+0x660F00 && pointer(image,0x141ED88)==image+0x5A9A60 &&
          pointer(image,0x141EDB0)==image+0x649CD0 && pointer(image,0x142C7B8)==image+0x661470,
          "rollback restores every expert UI vtable including 2026-10-23 preset selection/lifetime slots");
    printf("{\"status\":\"split_module_fixture_passed\",\"checks\":%u,\"gameplay_verified\":false}\n",checks);
    // Modules remain loaded until process exit; no gate is unloaded in flight.
    return 0;
}
