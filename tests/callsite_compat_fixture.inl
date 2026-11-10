// 사용자 결정 2026-10-30: 통합패치·발탁장수 공존을 위해 첫머리 훅을 호출 자리로 이동
static unsigned compat_calls;
static uint64_t __fastcall compat_plan(Obj a,Obj b,Obj c) {++compat_calls;return reinterpret_cast<uint64_t>(a)^reinterpret_cast<uint64_t>(b)^reinterpret_cast<uint64_t>(c);}
static float __fastcall compat_move(Obj a,Obj b,int c,int d,Obj t) {++compat_calls;return a==image+32 && b==image+48 && c==7 && d==13 && t==image+64?3.75f:-1.0f;}
static int __fastcall compat_search(SortieNativePath *,Obj c,unsigned f,unsigned *t,int a,int b,int d) {++compat_calls;return c==image+48 && f==2 && t && *t==3?a+b+d:-1;}
static void compat_set_call(unsigned site,Obj target) {
    intptr_t delta=reinterpret_cast<intptr_t>(target)-reinterpret_cast<intptr_t>(image+site+5);
    check(delta>=INT32_MIN && delta<=INT32_MAX,"fixture target reachable by rel32");
    int value=static_cast<int>(delta);image[site]=0xE8;memcpy(image+site+1,&value,4);
}
static Obj compat_target(unsigned site) {int delta;memcpy(&delta,image+site+1,4);return image+site+5+delta;}
// 사용자 결정 2026-10-30: 통합패치·발탁장수 공존을 위해 첫머리 훅을 호출 자리로 이동
// Claude review: retain BOTH capture users through the restored 301BD9 thunk.
static unsigned capture_updates,capture_searches;
static int __fastcall capture_valid(Obj object) {return object!=nullptr;}
static unsigned __fastcall capture_army_id(Obj) {return 0;}
static int __fastcall capture_distance(int a,int b) {return abs(a-b);}
static void __fastcall capture_clear(Obj) {}
static void __fastcall capture_mode(Obj army,int mode) {army[0x37]=static_cast<unsigned char>(mode);}
static void __fastcall capture_target(Obj army,int kind,int target) {
    army[0x38]=static_cast<unsigned char>(kind);unsigned short value=static_cast<unsigned short>(target);memcpy(army+0x3A,&value,2);
}
static void __fastcall capture_waypoint(Obj army,int index,unsigned hex) {unsigned short value=static_cast<unsigned short>(hex);memcpy(army+0x2C+index*2,&value,2);}
static int __fastcall capture_search(SortieNativePath *path,Obj,unsigned from,unsigned *to,int a,int b,int c) {
    ++capture_searches;
    check(native_search_depth==1 && native_search_from==from && native_search_flags==static_cast<unsigned>(b) &&
        native_search_buffer==static_cast<unsigned>(c) && !strcmp(native_phase,"native_path_search"),
        "restored deep caller records search depth phase and native_search fields");
    check(a==1 && b==11 && c==13 && to && from==2 && *to==4,"restored deep caller forwards every search argument");
    static int cells[]={2,3,4};path->data=cells;path->count=3;return 1;
}
static void __fastcall capture_update(Obj army) {
    ++capture_updates;SortieNativePath path{};unsigned to=word(army,0x3A);
    check(reinterpret_cast<SortieSearch>(compat_target(0x303FC9))(&path,image+48,word(army,0x2A),&to,1,11,13)==1,
        "26C940 mock command update invokes the actual installed 301BD9 caller thunk");
    unsigned short next=3;memcpy(army+0x48,&next,2);
}
static void *callsite_capture_function(unsigned rva) {
    switch(rva) {
    case 0x331780:return reinterpret_cast<void *>(&capture_valid);
    case 0x22F4A0:return reinterpret_cast<void *>(&capture_army_id);
    case 0x2ACB80:return reinterpret_cast<void *>(&capture_distance);
    case 0xA3370:return reinterpret_cast<void *>(&capture_clear);
    case 0x2ECEA0:return reinterpret_cast<void *>(&capture_mode);
    case 0x2ECF00:return reinterpret_cast<void *>(&capture_target);
    case 0x2F05A0:return reinterpret_cast<void *>(&capture_waypoint);
    case 0x26E260:return reinterpret_cast<void *>(&capture_update);
    default:return nullptr;
    }
}
static void restored_capture_tests() {
    unsigned index=0;while(index<kSortieSearchCount && sortie_search_sites[index]!=0x303FC9)++index;
    check(index<kSortieSearchCount,"301BD9 is present in the graph-derived caller table");
    SortieSearch saved_next=sortie_search_nexts[index];sortie_search_nexts[index]=capture_search;
    Obj saved_world=world(),root=static_cast<Obj>(calloc(1,0x90000));check(root!=nullptr,"capture fixture world allocated");
    *reinterpret_cast<Obj *>(image+0x2780F60)=root;
    unsigned char army[0x200]={},tiles[3][0x30]={};unsigned short from=2;memcpy(army+0x2A,&from,2);
    for(unsigned i=0;i<3;++i)*reinterpret_cast<Obj *>(root+0xE270+(i+2)*8)=tiles[i];
    capture_target(army,9,4);capture_updates=capture_searches=0;callsite_capture_active=true;
    SortieOrder saved=sortie_order(army);SortieTrace trace;
    check(sortie_trace_path(army,4,trace) && trace.count==3 && trace.cells==std::vector<unsigned>({2,3,4}),
        "sortie_trace_path recovers the native path through restored 301BD9");
    check(!memcmp(army+0x2C,saved.bytes,sizeof(saved.bytes)),"sortie trace restores native command fields");
    auto route=campaign_safe_route(army,4,false,true);
    check(route.cells==std::vector<unsigned>({2,3,4}),"campaign_safe_route recovers the native path through restored 301BD9");
    check(!memcmp(army+0x2C,saved.bytes,sizeof(saved.bytes)),"campaign trace restores native command fields");
    check(capture_updates==3 && capture_searches==3 && !sortie_trace_output && !campaign_native_trace && !native_search_depth,
        "both capture paths execute original-next and clean up all trace and search scopes");
    callsite_capture_active=false;*reinterpret_cast<Obj *>(image+0x2780F60)=saved_world;free(root);sortie_search_nexts[index]=saved_next;
}
static void callsite_sortie_tests() {
    const LONG saved_enabled=enabled;enabled=0;
    const LONG saved_logging=diagnostic_logging;
    unsigned sites[kSortieCallCount];unsigned char saved[kSortieCallCount][5],entries[8][64];
    for(unsigned k=0;k<kSortieCallCount;++k) {sites[k]=k<5?sortie_plan_sites[k]:k<kSortieSearchEnd?sortie_search_sites[k-5]:sortie_movement_sites[k-kSortieSearchEnd];memcpy(saved[k],image+sites[k],5);}
    for(unsigned k=0;k<8;++k)memcpy(entries[k],image+sortie_entries[k].rva,64);
    Obj relay=allocate_near(image+sites[0],8192),foreign=allocate_near(image+sites[0]);
    check(relay && foreign,"fixture relays allocated");
    for(unsigned k=0;k<kSortieCallCount;++k)absolute_jump(foreign+k*16,k<5?reinterpret_cast<void *>(&compat_plan):k<kSortieSearchEnd?reinterpret_cast<void *>(&compat_search):reinterpret_cast<void *>(&compat_move));
    DWORD old=0;check(VirtualProtect(foreign,4096,PAGE_EXECUTE_READ,&old)!=0,"foreign module surrogate executable");
    for(unsigned scenario=0;scenario<4;++scenario) {
        for(unsigned k=0;k<kSortieCallCount;++k)memcpy(image+sites[k],saved[k],5);
        if(scenario==1)for(unsigned k=0;k<kSortieCallCount;++k)compat_set_call(sites[k],foreign+k*16);
        if(scenario==2) {image[sites[0]]=0xCC;compat_set_call(sites[1],image+0x1000);image[sites[5]]=0x90;compat_set_call(sites[kSortieSearchEnd],image+0x1000);image[0x1EDC30]=0xCC;}
        if(scenario==3)for(unsigned k=0;k<5;++k)image[sites[k]]=0xCC;
        SortieCode code{};
        check(sortie_guards_match()==(scenario!=3),"guard fails only when all five planner callers fail");
        // Empty fixture log_path prevents file output; the actual common writer
        // still counts each invocation through log_perf::Sample.
        diagnostic_logging=1;LONG64 logged=log_perf::calls;
        check(prepare_sortie(code,relay)==(scenario!=3),"prepare keeps individual mismatches local");
        check(log_perf::calls-logged==(scenario>=2?5:0),"every skipped caller invokes the common log_event writer");
        diagnostic_logging=saved_logging;
        if(scenario==3) {memcpy(image+0x1EDC30,entries[5],64);continue;}
        check(VirtualProtect(relay,8192,PAGE_EXECUTE_READWRITE,&old)!=0,"fixture relays executable");
        check(publish_sortie(code),"valid remaining caller hooks published");
        check(path_fast_guards_match(),"combat caller hooks preserve path_fast fingerprints");
        if(scenario==1)check(path_fast::movement(image+32,image+48,7,13,image+64)==3.75f,
            "path_fast follows the actual 183CC9 combat thunk and external next");
        if(scenario==1)restored_capture_tests();
        for(unsigned k=0;k<kSortieCallCount;++k) {
            bool skipped=scenario==2 && (k==0 || k==1 || k==5 || k==kSortieSearchEnd);
            check(code.calls[k].published==!skipped,"every caller independently installed or skipped");
            if(skipped)continue;
            if(scenario==1) {
                Obj next=foreign+k*16;
                check(k<5?reinterpret_cast<Obj>(original_sortie_planners[k])==next:k<kSortieSearchEnd?reinterpret_cast<Obj>(sortie_search_nexts[k-5])==next:reinterpret_cast<Obj>(sortie_movement_nexts[k-kSortieSearchEnd])==next,"each site retains its own distinct external next pointer");
                unsigned before=compat_calls;
                if(k<5)check(reinterpret_cast<SortiePlanner>(compat_target(sites[k]))(image+32,image+48,image+64)==compat_plan(image+32,image+48,image+64),"planner caller ABI and external chain");
                else if(k<kSortieSearchEnd) {SortieNativePath path{};unsigned to=3;check(reinterpret_cast<SortieSearch>(compat_target(sites[k]))(&path,image+48,2,&to,7,11,13)==31,"search caller forwards seven arguments");}
                else check(reinterpret_cast<SortieMovement>(compat_target(sites[k]))(image+32,image+48,7,13,image+64)==3.75f,"movement caller forwards fifth pointer and float return");
                check(compat_calls>before,"external next actually executed");
            }
        }
        for(unsigned k:{0u,1u,2u,3u,4u,6u,7u})check(!memcmp(image+sortie_entries[k].rva,entries[k],64),"all seven native 64-byte prefixes unchanged");
        // A subsequent owner replaces one opcode and one rel32; preserve both.
        image[sites[3]]=0xCC;compat_set_call(sites[4],foreign+16);
        unsigned char later[2][5];memcpy(later[0],image+sites[3],5);memcpy(later[1],image+sites[4],5);
        // Neighbouring bytes must also survive restore of an aligned CALL block.
        image[sites[2]-1]^=1;unsigned char neighbour=image[sites[2]-1];
        restore_sortie(code);
        check(!memcmp(image+sites[3],later[0],5) && !memcmp(image+sites[4],later[1],5),"restore preserves later opcode and destination owners");
        check(image[sites[2]-1]==neighbour,"restore preserves unrelated neighbouring bytes");image[sites[2]-1]^=1;
        for(unsigned k=0;k<kSortieCallCount;++k)if(k!=3 && k!=4 && !(scenario==2 && (k==0 || k==1 || k==5 || k==kSortieSearchEnd)))
            check(compat_target(sites[k])==(scenario==1?foreign+k*16:image+sites[k]+5+*reinterpret_cast<int *>(saved[k]+1)),"restored caller points to its exact original or external next");
        memcpy(image+0x1EDC30,entries[5],64);
    }
    for(unsigned k=0;k<kSortieCallCount;++k)memcpy(image+sites[k],saved[k],5);
    for(unsigned k=0;k<8;++k)memcpy(image+sortie_entries[k].rva,entries[k],64);
    for(unsigned site:path_fast::movement_calls) {
        unsigned char bytes[5];memcpy(bytes,image+site,5);
        image[site]=0xCC;check(!path_fast_guards_match(),"path fingerprint rejects foreign CALL opcode");
        compat_set_call(site,image+0x1000);check(!path_fast_guards_match(),"path fingerprint rejects wrong in-image target");
        compat_set_call(site,foreign);check(path_fast_guards_match(),"path fingerprint permits independently validated external CALL");
        memcpy(image+site,bytes,5);
    }
    enabled=saved_enabled;VirtualFree(relay,0,MEM_RELEASE);VirtualFree(foreign,0,MEM_RELEASE);
    check(code_ready(),"callsite fixtures restore complete captured text CRC");
}
