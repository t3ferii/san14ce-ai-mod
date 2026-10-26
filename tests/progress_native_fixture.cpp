// Private executable pages only. Never attaches to or patches the game.
#define PROGRESS_NATIVE_FIXTURE
#define TACTICS_FIXTURE
#include "../src/native_adapter.cpp"
static Obj phase_manager,phase_subject;
static unsigned phase_count;
static void __fastcall phase_mock(Obj manager,Obj subject) {phase_manager=manager;phase_subject=subject;++phase_count;}
static void *fixture_function(unsigned rva) {
    if(rva==0xD3D50 || rva==0xD3CE0 || rva==0xD3C70)return reinterpret_cast<void *>(&phase_mock);
    abort();
}
static unsigned checks;
static void check(bool value) {if(!value) {fprintf(stderr,"Progress native check failed: %u\n",checks+1);exit(1);}++checks;}
static void __fastcall nested_ai_mock(Obj marker) {
    ProgressSample *current=progress_sample;check(current!=nullptr);
    progress_force(marker,marker);check(current->calls[0]==1);
    if(marker)progress_native_ai(nullptr);
    check(progress_sample==current && current->calls[0]==1);
}
static void __fastcall exception_ai_mock(Obj) {RaiseException(0xE0123456,0,0,nullptr);}
static void verify_exception_restore() {
    bool caught=false;
    __try {progress_native_ai(nullptr);}
    __except(GetExceptionCode()==0xE0123456?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH) {caught=true;}
    check(caught);
}
static void writable(Obj memory,SIZE_T bytes) {DWORD old;check(VirtualProtect(memory,bytes,PAGE_EXECUTE_READWRITE,&old)!=0);}
struct Pair {int a,b;};
static volatile uint64_t benchmark_sink;
struct Worker {ProgressDistance original,patched;uint32_t seed;bool matched;};
static DWORD WINAPI worker(LPVOID data) {
    auto task=static_cast<Worker *>(data);uint32_t random=task->seed;task->matched=true;
    for(unsigned i=0;i<50000;++i) {
        random=random*1664525u+1013904223u;int a=static_cast<int>(random%48400);
        random=random*1664525u+1013904223u;int b=static_cast<int>(random%48400);
        if(task->patched(a,b)!=task->original(a,b)) {task->matched=false;break;}
    }
    return 0;
}
static double measure(ProgressDistance function,const std::vector<Pair> &pairs) {
    LARGE_INTEGER begin,end,frequency;QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&begin);
    uint64_t sum=0;for(unsigned repeat=0;repeat<20;++repeat)for(const Pair &p:pairs)sum+=static_cast<unsigned>(function(p.a,p.b));
    QueryPerformanceCounter(&end);benchmark_sink=sum;
    return static_cast<double>(end.QuadPart-begin.QuadPart)*1000.0/static_cast<double>(frequency.QuadPart);
}
static void verify_logging_policy() {
    wchar_t temporary[MAX_PATH],config[MAX_PATH];
    check(GetTempPathW(MAX_PATH,temporary)>0);
    check(GetTempFileNameW(temporary,L"s14",0,config)!=0);
    check(wcscpy_s(log_path,config)==0 && wcscat_s(log_path,L".log")==0);
    load_diagnostic_settings(config);check(!diagnostic_logging);
    unsigned argument=0;log_event("normal_off",++argument);check(argument==0);
    log_bonus("normal_bonus_off",1,2,30,1.0f,1.0f);
    check(observe_native_exception(nullptr)==EXCEPTION_CONTINUE_SEARCH);
    check(GetFileAttributesW(log_path)==INVALID_FILE_ATTRIBUTES);
    check(WritePrivateProfileStringW(L"diagnostics",L"enabled",L"1",config)!=0);
    load_diagnostic_settings(config);check(diagnostic_logging==1);
    log_event("development_on",++argument);check(argument==1);
    log_bonus("development_bonus_on",1,2,30,1.0f,1.0f);
    HANDLE file=CreateFileW(log_path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(file!=INVALID_HANDLE_VALUE);LARGE_INTEGER before={},after={};check(GetFileSizeEx(file,&before)!=0 && before.QuadPart>0);
    char content[1024]={};DWORD read=0;check(ReadFile(file,content,sizeof(content)-1,&read,nullptr)!=0);
    check(strstr(content,"development_on") && strstr(content,"development_bonus_on") && !strstr(content,"normal_off"));
    check(WritePrivateProfileStringW(L"diagnostics",L"enabled",L"0",config)!=0);
    load_diagnostic_settings(config);check(!diagnostic_logging);
    log_event("disabled_again",++argument);log_bonus("disabled_bonus",1,2,30,1.0f,1.0f);
    check(argument==1 && GetFileSizeEx(file,&after)!=0 && before.QuadPart==after.QuadPart);
    CloseHandle(file);log_append.close();check(DeleteFileW(log_path)!=0);check(DeleteFileW(config)!=0);log_path[0]=0;
}
int main() {
    verify_logging_policy();
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x450000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));check(image!=nullptr);
    Obj relay=static_cast<Obj>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));check(relay!=nullptr);
    Obj baseline=static_cast<Obj>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));check(baseline!=nullptr);
    memcpy(image+0x2AA850,progress_distance_bytes,sizeof(progress_distance_bytes));
    memcpy(baseline,progress_distance_bytes,sizeof(progress_distance_bytes));
    memcpy(image+0x43E030,progress_ai_entry_bytes,sizeof(progress_ai_entry_bytes));
    memcpy(image+0x43E0A0,progress_force_call_bytes,16);
    memcpy(image+0x43E120,progress_district_call_bytes,16);
    memcpy(image+0x43E4C0,progress_army_call_bytes,16);
    // Finish the three shadow-space saves from the verified original prolog,
    // increment a scalar fixture context and unwind its actual stack frame.
    const unsigned char ai_tail[]={0x89,0x58,0x10,0x48,0x89,0x70,0x18,0x48,0x89,0x78,0x20,
        0xFF,0x01,0x48,0x81,0xC4,0,1,0,0,0x41,0x5F,0x41,0x5E,0x41,0x5D,0x41,0x5C,0x5D,0xC3};
    memcpy(image+0x43E050,ai_tail,sizeof(ai_tail));
    progress_grid.initialize();check(QueryPerformanceFrequency(&progress_clock_frequency)!=0);
    ProgressNativeCode code;check(progress_native_guards_match());check(prepare_progress_native(code,relay));
    check(code.distance.modified[5]==0x90 && code.distance.modified[7]==0x90 && code.ai.modified[5]==0x90);
    DWORD old;check(VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old)!=0);
    check(VirtualProtect(baseline,4096,PAGE_EXECUTE_READ,&old)!=0);
    check(VirtualProtect(image,0x450000,PAGE_EXECUTE_READ,&old)!=0);
    FlushInstructionCache(GetCurrentProcess(),image,0x450000);FlushInstructionCache(GetCurrentProcess(),relay,280);
    auto runtime=reinterpret_cast<RUNTIME_FUNCTION *>(relay+256);
    check(RtlAddFunctionTable(runtime,1,reinterpret_cast<DWORD64>(relay))!=0);
    DWORD64 function_base=0;
    check(RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(relay+230),&function_base,nullptr)==runtime);
    DWORD64 stack[]={0x123456,0x789ABC,0x102030};CONTEXT context_unwind={};context_unwind.ContextFlags=CONTEXT_FULL;
    context_unwind.Rip=reinterpret_cast<DWORD64>(relay+230);context_unwind.Rsp=reinterpret_cast<DWORD64>(stack);
    void *handler_data=nullptr;DWORD64 establisher=0;
    RtlVirtualUnwind(0,function_base,context_unwind.Rip,runtime,&context_unwind,&handler_data,&establisher,nullptr);
    check(context_unwind.R12==stack[0] && context_unwind.Rbp==stack[1] && context_unwind.Rip==stack[2]);
    check(context_unwind.Rsp==reinterpret_cast<DWORD64>(stack+3));
    context_unwind.Rip=reinterpret_cast<DWORD64>(relay+240);context_unwind.Rsp=reinterpret_cast<DWORD64>(stack);
    RtlVirtualUnwind(0,function_base,context_unwind.Rip,runtime,&context_unwind,&handler_data,&establisher,nullptr);
    check(context_unwind.R12==stack[0] && context_unwind.Rbp==stack[1] && context_unwind.Rip==stack[2]);
    check(context_unwind.Rsp==reinterpret_cast<DWORD64>(stack+3));
    auto original=reinterpret_cast<ProgressDistance>(baseline);
    auto patched=reinterpret_cast<ProgressDistance>(image+0x2AA850);
    auto ai=reinterpret_cast<ProgressAi>(image+0x43E030);
    unsigned context=0;progress_native_profile=false;ai(reinterpret_cast<Obj>(&context));check(context==1);
    check(original_progress_distance(0,48399)==original(0,48399));
    // Force rejection of the second publication after the first has succeeded.
    progress_native_profile=true;
    code.ai.original[15]^=1;check(!publish_progress_native(code));
    check(!progress_native_active && !memcmp(image+0x2AA850,progress_distance_bytes,16));
    check(!memcmp(image+0x43E030,progress_ai_entry_bytes,16));code.ai.original[15]^=1;
    for(unsigned phase=0;phase<3;++phase) {
        code.phases[phase].original[15]^=1;check(!publish_progress_native(code));
        check(!progress_native_active && !memcmp(image+0x2AA850,progress_distance_bytes,16));
        check(!memcmp(image+0x43E030,progress_ai_entry_bytes,16));
        for(unsigned j=0;j<3;++j)check(!memcmp(image+code.phases[j].block,code.phases[j].original,15));
        code.phases[phase].original[15]^=1;
    }
    check(publish_progress_native(code));check(progress_native_active==1);
    ProgressSample phase_sample={};progress_sample=&phase_sample;progress_native_profile=true;
    for(unsigned phase=0;phase<3;++phase) {
        auto phase_entry=reinterpret_cast<ProgressPhase>(relay+288+phase*32);
        phase_entry(reinterpret_cast<Obj>(uintptr_t(0xABC0+phase)),reinterpret_cast<Obj>(uintptr_t(0xDEF0+phase)));
        check(phase_manager==reinterpret_cast<Obj>(uintptr_t(0xABC0+phase)) && phase_subject==reinterpret_cast<Obj>(uintptr_t(0xDEF0+phase)));
        check(phase_sample.calls[phase]==1 && phase_sample.ticks[phase]>=0);
        int32_t displacement=0;unsigned offset=code.phases[phase].rva-code.phases[phase].block;
        memcpy(&displacement,image+code.phases[phase].rva+1,4);
        check(image+code.phases[phase].rva+5+displacement==relay+288+phase*32);
        for(unsigned j=0;j<16;++j)if(j<offset || j>=offset+5)check(image[code.phases[phase].block+j]==code.phases[phase].original[j]);
    }
    progress_native_profile=false;progress_force(nullptr,nullptr);check(phase_count==4 && phase_sample.calls[0]==1);
    progress_native_profile=true;progress_native_ai(reinterpret_cast<Obj>(&context));check(progress_sample==&phase_sample && context==2);
    progress_sample=nullptr;progress_native_profile=false;
    ProgressAi saved_ai=original_progress_ai;original_progress_ai=nested_ai_mock;progress_native_profile=true;
    progress_native_ai(reinterpret_cast<Obj>(uintptr_t(1)));check(progress_sample==nullptr);
    original_progress_ai=exception_ai_mock;progress_sample=&phase_sample;verify_exception_restore();check(progress_sample==&phase_sample);
    progress_sample=nullptr;original_progress_ai=saved_ai;progress_native_profile=false;
    uint64_t comparisons=0;
    for(unsigned a=0;a<48400;++a)for(int b:{0,1,219,220,221,48399,24200,24201}) {
        check(patched(static_cast<int>(a),b)==original(static_cast<int>(a),b));++comparisons;
    }
    for(int a:{-1,48400,INT32_MIN,INT32_MAX})for(int b:{-1,0,48399,48400,INT32_MIN,INT32_MAX})check(patched(a,b)==original(a,b));
    InterlockedExchange(&progress_native_active,0);check(patched(31,48213)==original(31,48213));
    InterlockedExchange(&progress_native_active,1);ai(reinterpret_cast<Obj>(&context));check(context==3);
    progress_native_profile=true;ai(reinterpret_cast<Obj>(&context));check(context==4 && progress_sample==nullptr);progress_native_profile=false;
    Worker workers[8];HANDLE handles[8];
    for(unsigned i=0;i<8;++i) {workers[i]={original,patched,0xCA7832Fu+i,false};handles[i]=CreateThread(nullptr,0,worker,&workers[i],0,nullptr);check(handles[i]!=nullptr);}
    check(WaitForMultipleObjects(8,handles,TRUE,10000)==WAIT_OBJECT_0);
    for(unsigned i=0;i<8;++i) {check(workers[i].matched);CloseHandle(handles[i]);}
    comparisons+=400000;
    printf("{\"status\":\"progress_native_fixture_passed\",\"native_comparisons\":%llu,\"patched_entry_benchmarks\":[",static_cast<unsigned long long>(comparisons));
    for(unsigned mode=0;mode<3;++mode) {
        std::vector<Pair> pairs;uint32_t random=0x7EA142F9;
        for(unsigned i=0;i<100000;++i) {
            random=random*1664525u+1013904223u;int a=static_cast<int>(random%48400);
            random=random*1664525u+1013904223u;int b=mode==0?static_cast<int>(random%48400):mode==1?24200:(a+1)%48400;
            pairs.push_back({a,b});
        }
        measure(original,pairs);measure(patched,pairs);std::vector<double> before,after;
        for(unsigned trial=0;trial<7;++trial) {
            if(trial&1) {after.push_back(measure(patched,pairs));before.push_back(measure(original,pairs));}
            else {before.push_back(measure(original,pairs));after.push_back(measure(patched,pairs));}
        }
        std::sort(before.begin(),before.end());std::sort(after.begin(),after.end());
        printf("%s{\"mode\":%u,\"original_ms\":%.3f,\"patched_ms\":%.3f,\"ratio\":%.3f}",mode?",":"",mode,before[3],after[3],before[3]/after[3]);
    }
    restore_progress_native(code);check(!progress_native_active);
    check(!memcmp(image+0x2AA850,progress_distance_bytes,sizeof(progress_distance_bytes)));
    check(!memcmp(image+0x43E030,progress_ai_entry_bytes,sizeof(progress_ai_entry_bytes)));
    for(unsigned phase=0;phase<3;++phase)check(!memcmp(image+code.phases[phase].block,code.phases[phase].original,16));
    check(patched(101,202)==original(101,202));ai(reinterpret_cast<Obj>(&context));check(context==5);
    progress_native_geometry=false;progress_native_profile=true;check(publish_progress_native(code));
    check(!memcmp(image+0x2AA850,progress_distance_bytes,sizeof(progress_distance_bytes)));
    ai(reinterpret_cast<Obj>(&context));check(context==6);restore_progress_native(code);progress_native_geometry=true;
    progress_native_profile=false;check(publish_progress_native(code));
    check(!memcmp(image+0x43E030,progress_ai_entry_bytes,sizeof(progress_ai_entry_bytes)));
    for(unsigned phase=0;phase<3;++phase)check(!memcmp(image+code.phases[phase].block,code.phases[phase].original,16));
    restore_progress_native(code);
    unsigned log_argument=0;log_event("disabled_log_argument",++log_argument);check(log_argument==0);
    writable(image+0x2AA850,4096);image[0x2AA850+173]^=1;check(!progress_native_guards_match());
    image[0x2AA850+173]^=1;
    printf("],\"checks\":%u,\"rollback_and_gates_verified\":true,\"gameplay_verified\":false}\n",checks);
    RtlDeleteFunctionTable(runtime);VirtualFree(relay,0,MEM_RELEASE);VirtualFree(baseline,0,MEM_RELEASE);VirtualFree(image,0,MEM_RELEASE);return 0;
}
