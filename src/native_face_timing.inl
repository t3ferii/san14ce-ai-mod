// [diagnostics] cost_profile only (2026-10-25, user: 0-day wait after 전략중 in 영웅난무). Times the
// original face setter 1B91E0(face node, person) at each of its 7 direct call sites; results go to the
// function_cost CSV as face_1B91E0_<site> (calls, total, max per 1 s window). No behaviour change.
// Evidence (REA carrier REA_Analysis/turnstart_20261025, (근거: 1a4a447d 이전 기록, docs/HISTORY.md)): 1B91E0 sets five face
// images (sizes 7/0x10/0x18/0x1E/0x20) through 9307B0; CPU samples of the 0-day wait put ~0.9 s of
// 4.7 s in its 9307B0->930840->872170->8A6DE0->8AB113 path. Army flag panels reach it via 1B3523.
// Sites (caller): 1990EF/199203 (198FA0), 199727 (1995E0), 19A0A1/19A0C1 (19A080), 1A95CB (1A957D
// chunk), 1B3523 (1B3450). Args: RCX, RDX only (1B91E0 prologue); callers ignore RAX.
#include "game_addresses.h"
#include <tlhelp32.h>
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2
#endif
#include <psapi.h>
namespace face_timing {
static const unsigned kFace=GA(GA_1BA4E0),kSites[7]={GA(GA_19A3BF),GA(GA_19A4D3),GA(GA_19A9F7),GA(GA_19B371),GA(GA_19B391),GA(GA_1AA8CB),GA(GA_1B4823)};
using Face=void(__fastcall *)(Obj,Obj);
static void __fastcall s0(Obj f,Obj p) {COST_SCOPE("face_1B91E0_1990EF");fn<Face>(kFace)(f,p);}
static void __fastcall s1(Obj f,Obj p) {COST_SCOPE("face_1B91E0_199203");fn<Face>(kFace)(f,p);}
static void __fastcall s2(Obj f,Obj p) {COST_SCOPE("face_1B91E0_199727");fn<Face>(kFace)(f,p);}
static void __fastcall s3(Obj f,Obj p) {COST_SCOPE("face_1B91E0_19A0A1");fn<Face>(kFace)(f,p);}
static void __fastcall s4(Obj f,Obj p) {COST_SCOPE("face_1B91E0_19A0C1");fn<Face>(kFace)(f,p);}
static void __fastcall s5(Obj f,Obj p) {COST_SCOPE("face_1B91E0_1A95CB");fn<Face>(kFace)(f,p);}
static void __fastcall s6(Obj f,Obj p) {COST_SCOPE("face_1B91E0_1B3523");fn<Face>(kFace)(f,p);}
static CallLink links[7];
// local159 (user 2026-11-14 "내정때문이라면 내정쪽 우리 모드에도 호출기달고 원작에도 달아서 원인 알아보자"): the 순-boundary stutter is
// one long original day frame (speed_frame:native 89-122 ms, nearly all outside the mod's scopes). Each case call of the day frame
// 44DEE0 (San14Sim/docs/orig/rules_time.md 2절) and the per-순 / month calls (305390 children, 2593E0, 42FDC0) get a timing wrapper
// at its E8 site, so the function_cost CSV and the long-frame record (native_cost_profile.h) name the original stage. The wrappers
// pass eight integer arguments and RAX through unchanged (no target takes float arguments or more than five arguments; 2BE030,
// with more, is left out). A site that is not the expected E8 (another hook owns it, e.g. 44E50D speed_buttons, 44E7D0 the
// season engine) is skipped and logged. Diagnostic builds with [diagnostics] cost_profile=1 only; no behaviour change.
struct DaySite {unsigned site,target;};
static const DaySite kDay[]={
    {GA(GA_44E332),GA(GA_2035D0)},{GA(GA_44E34D),GA(GA_4048D0)},{GA(GA_44E36A),GA(GA_405A40)},{GA(GA_44E37D),GA(GA_457C90)},
    {GA(GA_44E38F),GA(GA_458520)},{GA(GA_44E399),GA(GA_2E38E0)},{GA(GA_44E3A3),GA(GA_2E3BC0)},{GA(GA_44E3B6),GA(GA_2CCC60)},
    {GA(GA_44E3F7),GA(GA_2B2390)},{GA(GA_44E4B0),GA(GA_2DC550)},{GA(GA_44E4F6),GA(GA_43C9F0)},{GA(GA_44E517),GA(GA_303020)},
    {GA(GA_44E52A),GA(GA_2EA1D0)},{GA(GA_44E545),GA(GA_17B9E0)},{GA(GA_44E560),GA(GA_18F9D0)},{GA(GA_44E56A),GA(GA_28A890)},
    {GA(GA_44E57D),GA(GA_2EABE0)},{GA(GA_44E587),GA(GA_2FF450)},{GA(GA_44E59B),GA(GA_272720)},{GA(GA_44E5C8),GA(GA_2BDFB0)},
    {GA(GA_44E647),GA(GA_2A3CF0)},{GA(GA_44E650),GA(GA_2A3510)},{GA(GA_44E6E2),GA(GA_2E6BD0)},{GA(GA_44E6F4),GA(GA_18FB90)},
    {GA(GA_44E706),GA(GA_43E660)},{GA(GA_44E715),GA(GA_43F4C0)},{GA(GA_44E71C),GA(GA_4512A0)},{GA(GA_44E723),GA(GA_2901A0)},
    {GA(GA_44E728),GA(GA_301AF0)},{GA(GA_44E72F),GA(GA_2902C0)},{GA(GA_44E736),GA(GA_276250)},{GA(GA_44E743),GA(GA_2FEE70)},
    {GA(GA_44E74A),GA(GA_2D4630)},
    {GA(GA_431824),GA(GA_305390)},{GA(GA_30554D),GA(GA_297070)},{GA(GA_305555),GA(GA_27BF90)},{GA(GA_30555F),GA(GA_303240)},
    {GA(GA_3055A9),GA(GA_2E9640)},{GA(GA_44CC1D),GA(GA_2593E0)},{GA(GA_44ECFF),GA(GA_42FDC0)}};
constexpr unsigned kDayCount=sizeof(kDay)/sizeof(kDay[0]);
using Any8=uint64_t(__fastcall *)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
static volatile DWORD logic_thread;   // thread of the day frame 44DEE0 (render-stall sampler role 2)
#define DAY_SITE(i,name) static uint64_t __fastcall d##i(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f,uint64_t g,uint64_t h) \
    {COST_SCOPE(name);logic_thread=GetCurrentThreadId();return reinterpret_cast<Any8>(image+kDay[i].target)(a,b,c,d,e,f,g,h);}
DAY_SITE(0,"day_c00_events_2035D0") DAY_SITE(1,"day_c01_pre_4048D0") DAY_SITE(2,"day_c01_turn_block_405A40") DAY_SITE(3,"day_c02_status_457C90")
DAY_SITE(4,"day_c02_extra_458520") DAY_SITE(5,"day_c03_duel_cmd_2E38E0") DAY_SITE(6,"day_c04_orders_2E3BC0") DAY_SITE(7,"day_c05_duel_2CCC60")
DAY_SITE(8,"day_c07_collect_2B2390") DAY_SITE(9,"day_c08_move_2DC550") DAY_SITE(10,"day_c09_appear_43C9F0") DAY_SITE(11,"day_c0b_paint_303020")
DAY_SITE(12,"day_c0c_take_2EA1D0") DAY_SITE(13,"day_c0d_combat_17B9E0") DAY_SITE(14,"day_c0e_facility_18F9D0") DAY_SITE(15,"day_c0f_build_start_28A890")
DAY_SITE(16,"day_c10_build_2EABE0") DAY_SITE(17,"day_c11_traits_2FF450") DAY_SITE(18,"day_c12_morale_272720") DAY_SITE(19,"day_c13_wounded_2BDFB0")
DAY_SITE(20,"day_c14_fire_2A3CF0") DAY_SITE(21,"day_c14_fire_2A3510") DAY_SITE(22,"day_c15_demolish_2E6BD0") DAY_SITE(23,"day_c16_attack_18FB90")
DAY_SITE(24,"day_c17_fire_damage_43E660") DAY_SITE(25,"day_c18_poison_43F4C0") DAY_SITE(26,"day_c19_capture_4512A0") DAY_SITE(27,"day_c1a_list_2901A0")
DAY_SITE(28,"day_c1a_list_301AF0") DAY_SITE(29,"day_c1b_cleanup_2902C0") DAY_SITE(30,"day_c1c_options_276250") DAY_SITE(31,"day_c1d_traits_2FEE70")
DAY_SITE(32,"day_c1e_misc_2D4630")
DAY_SITE(33,"xun_city_305390") DAY_SITE(34,"xun_district_297070") DAY_SITE(35,"xun_27BF90") DAY_SITE(36,"xun_training_303240")
DAY_SITE(37,"xun_repair_2E9640") DAY_SITE(38,"xun_orders_2593E0") DAY_SITE(39,"month_events_42FDC0")
#undef DAY_SITE
static_assert(kDayCount==40,"one wrapper per day site");
static CallLink day_links[kDayCount];
static void install_day_sites() {
    static const Any8 wrappers[kDayCount]={d0,d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12,d13,d14,d15,d16,d17,d18,d19,
        d20,d21,d22,d23,d24,d25,d26,d27,d28,d29,d30,d31,d32,d33,d34,d35,d36,d37,d38,d39};
    bool use[kDayCount]={};unsigned wanted=0;
    for(unsigned i=0;i<kDayCount;++i) {const unsigned site=kDay[i].site,target=kDay[i].target;
        if(!site || !target || image[site]!=0xE8 || site+5+*reinterpret_cast<int *>(image+site+1)!=target) {log_event("day_timing_site_skipped",site,target,i);continue;}
        use[i]=true;++wanted;}
    if(!wanted)return;
    Obj relay=allocate_near(image+kDay[2].site);if(!relay) {log_event("day_timing_relay_rejected");return;}
    DWORD old;bool ok=true;
    for(unsigned i=0;i<kDayCount && ok;++i)if(use[i]) {absolute_jump(relay+32*i,reinterpret_cast<void *>(wrappers[i]));ok=call_prepare(day_links[i],kDay[i].site,relay+32*i);}
    if(!ok || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,kDayCount*32)) {
        VirtualFree(relay,0,MEM_RELEASE);log_event("day_timing_prepare_rejected");return;
    }
    unsigned published=0;
    for(unsigned i=0;i<kDayCount;++i)if(use[i]) {if(call_publish(day_links[i]))++published;else {use[i]=false;log_event("day_timing_publish_rejected",kDay[i].site,i);}}
    log_event("day_timing_installed",published,wanted,kDayCount);
}
// local203 (user 2026-11-16 "응, 진단판 만들어"): render-stall sampler. The 10-day hitch is one 140-235 ms gap between two Present
// entries during the computer forces' strategy turns (development/USER_REQUESTS.md 2026-11-16, docs/MOD_DEV_LESSONS.md D4); our
// own draw is 0.2 ms of it, so the render thread waits on something in the original. A thread of ours watches the Present clock
// (costprofile::present_qpc, written by unified_overlay's frame_perf). When a gap reaches 50 ms it lists this process's threads
// and their CPU time; at 60 ms and then every 30 ms (local265; before 80 / 40 ms; at most 5 samples per gap, gaps that end under
// 100 ms are not written) it reads the context and the top 48 KB of the
// stack of the render thread, the day-frame thread, the window thread and every thread that used >= 2 ms CPU since the previous
// reading. While a thread is suspended the sampler only calls GetThreadContext and copies stack memory into a fixed buffer (no
// allocation, lock, log or game call); return-address candidates (values in a module's code that follow a CALL) are picked after
// ResumeThread. Rows go to data/performance/render_stall_<pid>.csv when the gap ends; tools/render_stall_report.py summarizes
// them. Reads only: no game call, no game memory write. Diagnostic builds with [diagnostics] cost_profile=1 only.
namespace stall_sampler {
constexpr unsigned kStackBytes=48*1024,kFrames=28,kSamplesPerGap=5,kThreadsPerSample=12,kMaxGaps=600,kMaxThreads=512,kMaxModules=192;
enum : unsigned {kRender=1,kLogic=2,kWindow=4,kBusy=8};
struct Range {uintptr_t begin,end;};
struct Module {uintptr_t base,end;Range code[4];unsigned code_count;char name[24];};
static Module modules[kMaxModules];static unsigned module_count;static ULONGLONG modules_tick;
static uintptr_t lowest=~uintptr_t(0),highest;
static void load_modules() {
    HMODULE list[kMaxModules];DWORD need=0;HANDLE self=GetCurrentProcess();
    if(!EnumProcessModules(self,list,sizeof(list),&need))return;
    unsigned n=need/sizeof(HMODULE);if(n>kMaxModules)n=kMaxModules;
    const HMODULE exe=GetModuleHandleW(nullptr);
    unsigned count=0;uintptr_t lo=~uintptr_t(0),hi=0;
    for(unsigned i=0;i<n;++i) {
        MODULEINFO info={};if(!GetModuleInformation(self,list[i],&info,sizeof(info)))continue;
        Module &m=modules[count];m.base=reinterpret_cast<uintptr_t>(info.lpBaseOfDll);m.end=m.base+info.SizeOfImage;m.code_count=0;
        char name[MAX_PATH]={};
        if(list[i]==exe)strcpy_s(name,"exe");else if(!GetModuleBaseNameA(self,list[i],name,MAX_PATH))strcpy_s(name,"?");
        for(char *c=name;*c;++c)if(*c>='A' && *c<='Z')*c=static_cast<char>(*c+32);
        if(char *dot=strrchr(name,'.'))if(!strcmp(dot,".dll"))*dot=0;
        strncpy_s(m.name,name,_TRUNCATE);
        const auto *dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(m.base);
        const auto *nt=reinterpret_cast<const IMAGE_NT_HEADERS64 *>(m.base+static_cast<uintptr_t>(dos->e_lfanew));
        const IMAGE_SECTION_HEADER *section=IMAGE_FIRST_SECTION(nt);
        for(unsigned k=0;k<nt->FileHeader.NumberOfSections && m.code_count<4;++k)
            if((section[k].Characteristics&IMAGE_SCN_MEM_EXECUTE) && section[k].Misc.VirtualSize>8)   // +8: a CALL before the return is readable
                m.code[m.code_count++]={m.base+section[k].VirtualAddress+8,m.base+section[k].VirtualAddress+section[k].Misc.VirtualSize};
        if(m.base<lo)lo=m.base;
        if(m.end>hi)hi=m.end;
        ++count;
    }
    module_count=count;lowest=lo;highest=hi;modules_tick=GetTickCount64();
}
static const Module *find(uintptr_t a) {
    if(a<lowest || a>=highest)return nullptr;
    for(unsigned i=0;i<module_count;++i)if(a>=modules[i].base && a<modules[i].end)return &modules[i];
    return nullptr;
}
static bool in_code(const Module *m,uintptr_t a) {for(unsigned i=0;i<m->code_count;++i)if(a>=m->code[i].begin && a<m->code[i].end)return true;return false;}
// p = return address: the bytes before it end a CALL (rel32, [rip+disp32], register, [reg], [reg+disp8/32], REX.B forms).
static bool after_call(const unsigned char *p) {
    if(p[-5]==0xE8 || (p[-6]==0xFF && p[-5]==0x15))return true;
    if(p[-2]==0xFF && ((p[-1]&0xF8)==0xD0 || ((p[-1]&0xF8)==0x10 && (p[-1]&7)!=4 && (p[-1]&7)!=5)))return true;
    if(p[-3]==0x41 && p[-2]==0xFF && ((p[-1]&0xF8)==0xD0 || (p[-1]&0xF8)==0x10))return true;
    if(p[-3]==0xFF && (p[-2]&0xF8)==0x50 && (p[-2]&7)!=4)return true;
    if(p[-4]==0xFF && p[-3]==0x54)return true;
    if(p[-4]==0x41 && p[-3]==0xFF && (p[-2]&0xF8)==0x50)return true;
    if(p[-6]==0xFF && (p[-5]&0xF8)==0x90 && (p[-5]&7)!=4)return true;
    if(p[-7]==0xFF && p[-6]==0x94)return true;
    if(p[-7]==0x41 && p[-6]==0xFF && (p[-5]&0xF8)==0x90)return true;
    return false;
}
struct BasicInfo {LONG status;void *teb;void *process,*thread;ULONG_PTR affinity;LONG priority,base;};   // THREAD_BASIC_INFORMATION
using QueryThread=LONG(NTAPI *)(HANDLE,int,void *,ULONG,ULONG *);
static QueryThread query_thread;
static uintptr_t stack_base(HANDLE h) {
    BasicInfo info={};
    if(!query_thread || query_thread(h,0,&info,sizeof(info),nullptr)<0 || !info.teb)return 0;
    return *reinterpret_cast<const uintptr_t *>(static_cast<const char *>(info.teb)+8);   // NT_TIB.StackBase
}
static ULONGLONG cpu_time(HANDLE h) {
    FILETIME created,exited,kernel,user;if(!GetThreadTimes(h,&created,&exited,&kernel,&user))return 0;
    ULARGE_INTEGER k,u;k.LowPart=kernel.dwLowDateTime;k.HighPart=kernel.dwHighDateTime;u.LowPart=user.dwLowDateTime;u.HighPart=user.dwHighDateTime;
    return k.QuadPart+u.QuadPart;   // 100 ns
}
struct Slot {DWORD id;HANDLE handle;uintptr_t stack;ULONGLONG cpu;unsigned roles,used_us;};
static Slot slots[kMaxThreads];static unsigned slot_count;
static void close_threads() {for(unsigned i=0;i<slot_count;++i)CloseHandle(slots[i].handle);slot_count=0;}
static void open_threads() {
    close_threads();
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snap==INVALID_HANDLE_VALUE)return;
    THREADENTRY32 entry={};entry.dwSize=sizeof(entry);
    const DWORD pid=GetCurrentProcessId(),me=GetCurrentThreadId();
    for(BOOL ok=Thread32First(snap,&entry);ok && slot_count<kMaxThreads;entry.dwSize=sizeof(entry),ok=Thread32Next(snap,&entry)) {
        if(entry.th32OwnerProcessID!=pid || entry.th32ThreadID==me)continue;
        HANDLE h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,entry.th32ThreadID);
        if(h)slots[slot_count++]={entry.th32ThreadID,h,stack_base(h),cpu_time(h),0,0};
    }
    CloseHandle(snap);
}
struct Sample {DWORD thread;unsigned roles,sample,at_ms,cpu_us,count;uintptr_t rip,frames[kFrames];};
static Sample samples[kSamplesPerGap*kThreadsPerSample];static unsigned sample_count;
alignas(16) static CONTEXT context;
static unsigned char stack_copy[kStackBytes];
static bool copy_stack(const void *from,size_t bytes) {__try {memcpy(stack_copy,from,bytes);return true;} __except(EXCEPTION_EXECUTE_HANDLER) {return false;}}
static bool capture(const Slot &t,Sample &s) {
    if(SuspendThread(t.handle)==static_cast<DWORD>(-1))return false;
    // Suspended: GetThreadContext and a copy into the fixed buffer only.
    context.ContextFlags=CONTEXT_CONTROL;
    const bool got=GetThreadContext(t.handle,&context)!=0;
    size_t bytes=0;
    if(got && t.stack>context.Rsp) {bytes=static_cast<size_t>(t.stack-context.Rsp);if(bytes>kStackBytes)bytes=kStackBytes;if(!copy_stack(reinterpret_cast<const void *>(context.Rsp),bytes))bytes=0;}
    ResumeThread(t.handle);
    if(!got)return false;
    s.rip=context.Rip;s.count=0;
    for(size_t at=0;at+8<=bytes && s.count<kFrames;at+=8) {
        uintptr_t value;memcpy(&value,stack_copy+at,8);
        const Module *m=find(value);
        if(m && in_code(m,value) && after_call(reinterpret_cast<const unsigned char *>(value)))s.frames[s.count++]=value;
    }
    return true;
}
static DWORD window_thread;
static BOOL CALLBACK find_window(HWND h,LPARAM) {
    DWORD pid=0;const DWORD tid=GetWindowThreadProcessId(h,&pid);
    if(pid==GetCurrentProcessId() && IsWindowVisible(h) && !GetWindow(h,GW_OWNER)) {window_thread=tid;return FALSE;}
    return TRUE;
}
static void take(unsigned sample,unsigned at_ms) {
    const DWORD render=static_cast<DWORD>(InterlockedCompareExchange(&costprofile::present_thread,0,0)),logic=logic_thread;
    for(unsigned i=0;i<slot_count;++i) {
        Slot &t=slots[i];const ULONGLONG cpu=cpu_time(t.handle),used=cpu>t.cpu?cpu-t.cpu:0;t.cpu=cpu;t.used_us=static_cast<unsigned>(used/10);
        t.roles=(t.id==render?kRender:0)|(t.id==logic?kLogic:0)|(t.id==window_thread?kWindow:0)|(used>=20000?kBusy:0);
    }
    unsigned taken=0;
    for(unsigned pass=0;pass<2;++pass)   // named roles first, then busy threads, up to kThreadsPerSample
        for(unsigned i=0;i<slot_count && taken<kThreadsPerSample && sample_count<kSamplesPerGap*kThreadsPerSample;++i) {
            const Slot &t=slots[i];
            if(pass==0?!(t.roles&(kRender|kLogic|kWindow)):(t.roles!=kBusy))continue;
            Sample &s=samples[sample_count];s.thread=t.id;s.roles=t.roles;s.sample=sample;s.at_ms=at_ms;s.cpu_us=t.used_us;
            if(capture(t,s)) {++sample_count;++taken;}
        }
}
static wchar_t out_path[MAX_PATH];
static void label(char *out,size_t size,uintptr_t a) {
    if(const Module *m=find(a))_snprintf_s(out,size,_TRUNCATE,"%s+%llX",m->name,static_cast<unsigned long long>(a-m->base));
    else _snprintf_s(out,size,_TRUNCATE,"?%llX",static_cast<unsigned long long>(a));
}
static void append(const char *text,size_t size) {
    HANDLE file=CreateFileW(out_path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    DWORD written=0;WriteFile(file,text,static_cast<DWORD>(size),&written,nullptr);CloseHandle(file);
}
static void write_gap(unsigned gap,ULONGLONG start_tick,unsigned total) {
    static char text[1<<16];size_t used=0;
    for(unsigned i=0;i<sample_count;++i) {
        const Sample &s=samples[i];char a[48];label(a,sizeof(a),s.rip);
        int n=_snprintf_s(text+used,sizeof(text)-used,_TRUNCATE,"%llu,%u,%u,%u,%u,%lu,%u,%u,%s",start_tick,gap,total,s.sample,s.at_ms,s.thread,s.roles,s.cpu_us,a);
        if(n<0)break;
        used+=static_cast<size_t>(n);
        for(unsigned k=0;k<s.count;++k) {label(a,sizeof(a),s.frames[k]);n=_snprintf_s(text+used,sizeof(text)-used,_TRUNCATE,",%s",a);if(n<0)break;used+=static_cast<size_t>(n);}
        if(used+2>=sizeof(text))break;
        text[used++]='\r';text[used++]='\n';
    }
    append(text,used);
    log_event("render_stall_sampled",total,sample_count,gap);
}
static DWORD WINAPI run(void *) {
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);
    LARGE_INTEGER hz;QueryPerformanceFrequency(&hz);
    LONGLONG gap_from=0;ULONGLONG start_tick=0;unsigned gap_id=0,next_ms=0,taken=0;bool armed=false;
    for(;;) {
        Sleep(5);
        const LONGLONG last=InterlockedCompareExchange64(&costprofile::present_qpc,0,0);
        if(!last)continue;
        if(armed && last!=gap_from) {   // the gap ended at the Present entry 'last'
            // local265: samples start at 60 ms (every 30 ms) so the 100-150 ms hitches get stacks too; gaps under 100 ms are dropped.
            const unsigned gap_ms=static_cast<unsigned>((last-gap_from)*1000/hz.QuadPart);
            if(sample_count && gap_ms>=100)write_gap(++gap_id,start_tick,gap_ms);
            close_threads();armed=false;sample_count=0;
            continue;
        }
        LARGE_INTEGER now;QueryPerformanceCounter(&now);
        const LONGLONG gap=(now.QuadPart-last)*1000/hz.QuadPart;
        if(!armed) {
            if(gap<50 || gap>=1000 || gap_id>=kMaxGaps)continue;
            if(!module_count || GetTickCount64()-modules_tick>10000)load_modules();
            if(!window_thread)EnumWindows(find_window,0);
            open_threads();armed=true;gap_from=last;start_tick=GetTickCount64()-static_cast<ULONGLONG>(gap);next_ms=60;taken=0;sample_count=0;
            continue;
        }
        if(taken<kSamplesPerGap && gap>=next_ms) {take(taken++,static_cast<unsigned>(gap));next_ms+=30;}
    }
}
static void start() {
    if(!costprofile::file_path[0])return;
    wcscpy_s(out_path,costprofile::file_path);
    wchar_t *slash=wcsrchr(out_path,L'\\');if(!slash)return;
    swprintf_s(slash+1,MAX_PATH-static_cast<size_t>(slash+1-out_path),L"render_stall_%lu.csv",GetCurrentProcessId());
    query_thread=reinterpret_cast<QueryThread>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryInformationThread"));
    static const char header[]="# gap_start_tick,gap,gap_ms,sample,at_ms,thread,roles(1 render 2 day-frame 4 window 8 busy),cpu_us_since_previous,"
                               "rip,return-address candidates innermost first (module+rva; exe = game)\r\n";
    append(header,sizeof(header)-1);
    HANDLE thread=CreateThread(nullptr,0,run,nullptr,0,nullptr);
    log_event("render_stall_sampler",thread?1u:0u,query_thread?1u:0u,0);
    if(thread)CloseHandle(thread);
}
}
static bool install_face();
bool install() {install_face();install_day_sites();stall_sampler::start();return true;}
static bool install_face() {
    // 1B3523 belongs to face_keep when it is installed (its wrapper keeps the same timing key).
    const unsigned count=face_keep::installed?6:7;
    for(unsigned i=0;i<count;++i) {const unsigned site=kSites[i];
        if(image[site]!=0xE8 || site+5+*reinterpret_cast<int *>(image+site+1)!=kFace) {log_event("face_timing_guard_rejected",site);return false;}}
    Obj relay=allocate_near(image+kFace);if(!relay)return false;
    const Face wrappers[7]={s0,s1,s2,s3,s4,s5,s6};
    DWORD old;bool ok=true;
    for(unsigned i=0;i<count && ok;++i) {absolute_jump(relay+32*i,reinterpret_cast<void *>(wrappers[i]));ok=call_prepare(links[i],kSites[i],relay+32*i);}
    if(!ok || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,count*32)) {
        VirtualFree(relay,0,MEM_RELEASE);log_event("face_timing_prepare_rejected");return false;
    }
    for(unsigned i=0;i<count;++i)if(!call_publish(links[i])) {log_event("face_timing_publish_rejected",i);while(i)call_restore(links[--i]);return false;}
    log_event("face_timing_installed");return true;
}
}
