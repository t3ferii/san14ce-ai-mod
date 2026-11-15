// User/Claude 2026-10-29: ResourceMap edits on the native placement callback only.
#pragma once
#include "game_addresses.h"
#define SAN14_TERRAIN_TREES_INCLUDED 1
#include <vector>
#include <cmath>
#include "terrain_call_profile.h"
#include <tlhelp32.h>
namespace terrain_trees {
#ifndef NO_RUNTIME_LOG
// Completion counters are sampled/reset by Present, without taking the tree lock.
alignas(8) static volatile LONG64 perf_calls,perf_total_us,perf_frame_us,perf_max_us;
static const LONGLONG perf_frequency=[] {LARGE_INTEGER f={};QueryPerformanceFrequency(&f);return f.QuadPart?f.QuadPart:1;}();
#endif
static const unsigned Slot=GA(GA_1697960),UpdateRva=GA(GA_BB3AC0),SetterRva=GA(GA_B93A90);
static constexpr unsigned Size=1024,HexCount=48400,FrameLimit=64;
static constexpr unsigned char update_bytes[16]={0x40,0x55,0x53,0x56,0x41,0x55,0x41,0x56,0x48,0x8D,0xAC,0x24,0x10,0xE7,0xFF,0xFF};
static constexpr unsigned char setter_bytes[16]={0x48,0x89,0x5C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x48,0x89,0x54,0x24,0x10,0x57};
// Decoded EXE (근거: 1a4a447d 이전 기록, docs/HISTORY.md) (2026-10-29), BB10F0..BB1412:
// rbp = entry_rsp-0x1918. +1940/+1948/+1950/+1958 are arguments 5..8.
// BB9F0B..BB9F4B and common dispatch D67C34..D67C45 supply a pointer and
// three u32 masks at caller rsp+20/+28/+30/+38, with no argument beyond 8.
// Keep opaque RAX as well, although those callers do not consume it.
using Update=uint64_t(__fastcall *)(Obj,Obj,Obj,Obj,Obj,unsigned,unsigned,unsigned);
using Set=void(__fastcall *)(Obj,uint64_t,unsigned char);
static bool permitted=true;
static SRWLOCK lock=SRWLOCK_INIT;
struct Goal {unsigned char terrain,original;};
static Goal goals[HexCount]={};
static bool has_goal[HexCount]={},queued[HexCount]={},world_transition,resync_needed;
// User 2026-10-29: validating S/R (VirtualQuery over the 1MB map) cost ~19ms per
// frame. The callback validates only when work is queued or S changed after a goal.
static volatile LONG work_pending,any_goal;
static unsigned short dirty_hex[HexCount]={};
static unsigned dirty_head,dirty_count;
static Obj captured_s,captured_r,captured_data; // Identity only outside the callback.
static std::vector<unsigned char> original_resource;
static bool active() {return permitted && enabled && mod_features.convenience && mod_features.terrain_edit;}
static bool guards_match() {
    return pointer(image,Slot)==image+UpdateRva &&
        !ga::mcmp_at(image,UpdateRva,update_bytes,sizeof(update_bytes)) &&
        !ga::mcmp_at(image,SetterRva,setter_bytes,sizeof(setter_bytes));
}
static void clear_queue_locked() {memset(queued,0,sizeof(queued));dirty_head=dirty_count=0;}
static void enqueue_locked(unsigned hex) {
    InterlockedExchange(&work_pending,1);
    if(!queued[hex]) {
        dirty_hex[(dirty_head+dirty_count)%HexCount]=static_cast<unsigned short>(hex);
        queued[hex]=true;++dirty_count;
    }
}
static void retain_pending_locked() {
    // A second load/resync can precede the callback that restores missing
    // sidecar goals. Preserve those pending restorations when clearing a queue.
    for(unsigned i=0;i<dirty_count;++i)has_goal[dirty_hex[(dirty_head+i)%HexCount]]=true;
}
// UI/load host: numbers only; no game object access or native call.
static void mark(unsigned hex,unsigned terrain,unsigned original) {
    if(hex>=HexCount || terrain>255 || original>255 || !active())return;
    AcquireSRWLockExclusive(&lock);
    // local142: a change between non-forest terrains (season flood / ice, roads...) has no trees to place or remove, unless
    // an earlier forest edit of this cell is still pending.
    const bool forest_side=terrain==5 || terrain==6 || original==5 || original==6;
    if(!forest_side && !has_goal[hex]) {ReleaseSRWLockExclusive(&lock);return;}
    if(!(world_transition && terrain==original)) {
        goals[hex]={static_cast<unsigned char>(terrain),static_cast<unsigned char>(original)};
        has_goal[hex]=true;InterlockedExchange(&any_goal,1);enqueue_locked(hex);
    }
    ReleaseSRWLockExclusive(&lock);
}
static void before_world_change() {
    AcquireSRWLockExclusive(&lock);
    world_transition=true;retain_pending_locked();clear_queue_locked();resync_needed=true;
    ReleaseSRWLockExclusive(&lock);
}
// Resync restores missing old goals first, then terrain_edit replays its sidecar.
// Renderer capture may independently require the same full replay.
static bool take_ui_work(bool force) {
    AcquireSRWLockExclusive(&lock);
    bool resync=resync_needed || force;world_transition=false;
    if(resync) {
        resync_needed=false;retain_pending_locked();clear_queue_locked();
        for(unsigned hex=0;hex<HexCount;++hex)if(has_goal[hex]) {
            goals[hex].terrain=goals[hex].original;has_goal[hex]=false;enqueue_locked(hex);
        }
    }
    ReleaseSRWLockExclusive(&lock);return resync;
}
struct Center {double x,z;};
static Center center(unsigned hex) {
    constexpr double h=810.5997779422346;
    unsigned row=hex/220,col=hex%220;
    return {37594.0+col*702.0,13335.3+row*h+((col%2==0)?h/2:0)};
}
static bool owns_cell(unsigned hex,unsigned cx,unsigned cz) {
    Center c=center(hex);double x=cx*200.0+100,z=cz*200.0+100;
    double nearest=(x-c.x)*(x-c.x)+(z-c.z)*(z-c.z);
    int row=static_cast<int>(hex/220),col=static_cast<int>(hex%220);
    for(int r=row-1;r<=row+1;++r)for(int k=col-1;k<=col+1;++k) {
        if(r<0 || r>=220 || k<0 || k>=220)continue;
        unsigned other=static_cast<unsigned>(r*220+k);if(other==hex)continue;
        Center n=center(other);double d=(x-n.x)*(x-n.x)+(z-n.z)*(z-n.z);
        if(d<nearest || (d==nearest && other<hex))return false;
    }
    return true;
}
static bool forest(unsigned terrain) {return terrain==5 || terrain==6;}
static unsigned char nearby_value(unsigned hex,unsigned terrain) {
    Center c=center(hex);int cx=static_cast<int>(std::floor(c.x/200)),cz=static_cast<int>(std::floor(c.z/200));
    unsigned counts[7]={};
    for(int z=cz-12;z<=cz+12;++z)for(int x=cx-12;x<=cx+12;++x) {
        if(x<0 || z<0 || x>=static_cast<int>(Size) || z>=static_cast<int>(Size) ||
           (x-cx)*(x-cx)+(z-cz)*(z-cz)>144)continue;
        unsigned value=original_resource[static_cast<size_t>(z)*Size+x];
        if(value==0 || value==5 || (terrain==5 && (value==4 || value==6)))++counts[value];
    }
    // Stable ties prefer the documented fallback, then lower resource values.
    unsigned best=terrain==6?5:0;
    for(unsigned value:{0u,4u,5u,6u})
        if((terrain==5 || value==0 || value==5) && counts[value]>counts[best])best=value;
    return static_cast<unsigned char>(best);
}
static bool readable(Obj object,size_t bytes) {
    uintptr_t at=reinterpret_cast<uintptr_t>(object);
    if(!at || at+bytes<at)return false;
    uintptr_t end=at+bytes;
    while(at<end) {
        MEMORY_BASIC_INFORMATION m={};
        if(!VirtualQuery(reinterpret_cast<void *>(at),&m,sizeof(m)) || m.State!=MEM_COMMIT ||
           (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
           !(m.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
        uintptr_t next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;
        if(next<=at)return false;at=next;
    }
    return true;
}
static bool resource(Obj s,Obj &r,Obj &data) {
    if(!readable(s,0x70D8) || pointer(s,0)!=image+GA(GA_1697930) ||
       *reinterpret_cast<unsigned *>(s+0x7084)!=Size || *reinterpret_cast<unsigned *>(s+0x7088)!=Size ||
       *reinterpret_cast<unsigned *>(s+0x708C)!=128 || *reinterpret_cast<unsigned *>(s+0x7090)!=128 || s[0x70B4]!=1)return false;
    r=pointer(s,0x70D0);
    if(!readable(r,0x98) || pointer(r,0)!=image+GA(GA_16955C0) ||
       *reinterpret_cast<unsigned *>(r+0x14)!=Size || *reinterpret_cast<unsigned *>(r+0x18)!=Size)return false;
    data=pointer(r,0x58);return readable(data,static_cast<size_t>(Size)*Size);
}
// Only the original placement callback calls this function. Keep its SRW lock
// through native setters so UI goals cannot change halfway through a hex.
static void apply_pending(Obj s) {
    TERRAIN_SCOPE(TreesApply);
    if(!active())return;
    // s is the live callback object; its R slot is read before R itself, so a
    // replaced R or data buffer still takes the validating path below.
    if(!InterlockedCompareExchange(&work_pending,0,0) && (!InterlockedCompareExchange(&any_goal,0,0) ||
       (s && s==captured_s && pointer(s,0x70D0)==captured_r && pointer(captured_r,0x58)==captured_data)))return;
    Obj r=nullptr,data=nullptr;
    if(!resource(s,r,data))return;
    unsigned cells=0,writes=0;bool captured=false;
    AcquireSRWLockExclusive(&lock);
    if(s!=captured_s || r!=captured_r || data!=captured_data) {
        try {original_resource.assign(data,data+static_cast<size_t>(Size)*Size);}
        catch(...) {ReleaseSRWLockExclusive(&lock);return;}
        captured_s=s;captured_r=r;captured_data=data;captured=true;resync_needed=true;
        for(unsigned hex=0;hex<HexCount;++hex)if(has_goal[hex])enqueue_locked(hex);
    }
    while(!world_transition && dirty_count && cells<FrameLimit) {
        unsigned hex=dirty_hex[dirty_head];dirty_head=(dirty_head+1)%HexCount;--dirty_count;queued[hex]=false;
        Goal goal=goals[hex];Center c=center(hex);
        int cx=static_cast<int>(std::floor(c.x/200)),cz=static_cast<int>(std::floor(c.z/200));
        bool restore=goal.terrain==goal.original || (!forest(goal.terrain) && !forest(goal.original));
        unsigned char value=forest(goal.terrain) && !restore?nearby_value(hex,goal.terrain):255;
        for(int z=cz-4;z<=cz+4;++z)for(int x=cx-4;x<=cx+4;++x) {
            if(x<0 || z<0 || x>=static_cast<int>(Size) || z>=static_cast<int>(Size) ||
               !owns_cell(hex,static_cast<unsigned>(x),static_cast<unsigned>(z)))continue;
            size_t at=static_cast<size_t>(z)*Size+x;
            unsigned char target=restore?original_resource[at]:value;
            if(data[at]!=target)++writes;
            terrain_profile::invoke(terrain_profile::Id::TreesSet,fn<Set>(SetterRva),r,static_cast<uint64_t>(static_cast<unsigned>(x))|
                (static_cast<uint64_t>(static_cast<unsigned>(z))<<32),target);
        }
        ++cells;
    }
    if(!dirty_count && !world_transition)InterlockedExchange(&work_pending,0);
    ReleaseSRWLockExclusive(&lock);
#ifndef NO_RUNTIME_LOG
    if(captured)log_event("terrain_trees_capture",static_cast<unsigned>(reinterpret_cast<uintptr_t>(s)),1,0);
    if(cells)log_event("terrain_trees_apply",cells,writes,0);
#else
    (void)captured;(void)writes;
#endif
}
#ifndef NO_RUNTIME_LOG
// local145 (user 2026-11-14 "나무 끊김 155 ms 이거 원작을 조사하고 최적화하자"): the 140-155 ms frames are inside the original update
// (BB3AC0 -> BBA0C0, 15 KB; ours 0.3 ms) about every 110 s. A read-only sampler finds where: once an original call has run
// 20 ms, a worker suspends the calling thread every millisecond, reads its RIP and the first two EXE return addresses on its stack,
// and resumes it (no game memory written, no game function called, no allocation while suspended). A call over 50 ms logs its
// top places as tree_spike_at (army = EXE RVA or 0xF0000000|module-relative for other modules, value = samples, hex = caller RVA).
namespace spike {
static HANDLE thread_handle,worker;static volatile LONG active;static volatile LONG64 started;
static constexpr unsigned Max=400;static uint64_t rip[Max],ret[Max];static volatile LONG count;
static uintptr_t lo,hi;
static void bounds() {
    if(lo)return;
    const IMAGE_DOS_HEADER *dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(image);
    const IMAGE_NT_HEADERS64 *nt=reinterpret_cast<const IMAGE_NT_HEADERS64 *>(image+dos->e_lfanew);
    lo=reinterpret_cast<uintptr_t>(image);hi=lo+nt->OptionalHeader.SizeOfImage;
}
// local147: B38C00/B38E60/B390A0 are the original job scheduler's steal/wait loop, so the tree job waits for jobs on other worker
// threads. During a long call the worker also samples every other thread of the process (opened once per long call, closed after),
// keeping only samples whose RIP is inside the EXE (idle threads sit in system waits): tree_spike_job army = RIP RVA, value =
// samples, hex = caller RVA. A main-thread place outside the EXE also logs tree_spike_mod (army = first four letters of the module).
static constexpr unsigned OMax=4000,TMax=96;static uint64_t orip[OMax],oret[OMax];static volatile LONG ocount;
static HANDLE others_h[TMax];static unsigned others_n;static bool others_open;
static void close_others() {for(unsigned i=0;i<others_n;++i)CloseHandle(others_h[i]);others_n=0;others_open=false;}
static void open_others() {
    others_open=true;const DWORD pid=GetCurrentProcessId(),self=GetCurrentThreadId(),main=GetThreadId(thread_handle);
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snap==INVALID_HANDLE_VALUE)return;
    THREADENTRY32 te={};te.dwSize=sizeof(te);
    for(BOOL ok=Thread32First(snap,&te);ok && others_n<TMax;ok=Thread32Next(snap,&te)) {
        if(te.th32OwnerProcessID!=pid || te.th32ThreadID==self || te.th32ThreadID==main)continue;
        HANDLE h=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT,FALSE,te.th32ThreadID);if(h)others_h[others_n++]=h;
    }
    CloseHandle(snap);
}
static void others() {
    if(!others_open)open_others();
    for(unsigned i=0;i<others_n;++i) {
        const LONG at=InterlockedCompareExchange(&ocount,0,0);if(at>=static_cast<LONG>(OMax))return;
        if(SuspendThread(others_h[i])==static_cast<DWORD>(-1))continue;
        CONTEXT c={};c.ContextFlags=CONTEXT_CONTROL;uint64_t r=0,caller=0;
        if(GetThreadContext(others_h[i],&c) && c.Rip>lo && c.Rip<hi) {
            r=c.Rip;MEMORY_BASIC_INFORMATION mbi;const uint64_t *sp=reinterpret_cast<const uint64_t *>(c.Rsp);
            if(VirtualQuery(sp,&mbi,sizeof(mbi)) && mbi.State==MEM_COMMIT) {
                const uintptr_t end=reinterpret_cast<uintptr_t>(mbi.BaseAddress)+mbi.RegionSize;
                for(unsigned k=0;k<64 && reinterpret_cast<uintptr_t>(sp+k+1)<=end;++k)if(sp[k]>lo && sp[k]<hi){caller=sp[k];break;}
            }
        }
        ResumeThread(others_h[i]);
        if(r){orip[at]=r;oret[at]=caller;InterlockedIncrement(&ocount);}
    }
}
static DWORD WINAPI run(void *) {
    for(;;) {
        if(!InterlockedCompareExchange(&active,0,0)){if(others_open)close_others();Sleep(2);continue;}
        LARGE_INTEGER now;QueryPerformanceCounter(&now);
        if((now.QuadPart-started)*1000<perf_frequency*20){Sleep(1);continue;}
        LONG at=InterlockedCompareExchange(&count,0,0);
        if(at>=static_cast<LONG>(Max)){Sleep(1);continue;}
        if(SuspendThread(thread_handle)==static_cast<DWORD>(-1)){Sleep(5);continue;}
        CONTEXT c={};c.ContextFlags=CONTEXT_CONTROL;uint64_t r=0,caller=0;
        if(GetThreadContext(thread_handle,&c) && InterlockedCompareExchange(&active,0,0)) {
            r=c.Rip;
            const uint64_t *sp=reinterpret_cast<const uint64_t *>(c.Rsp);unsigned found=0;
            for(unsigned i=0;i<96 && found<2;++i) {
                MEMORY_BASIC_INFORMATION mbi;
                if(i==0 && (!VirtualQuery(sp,&mbi,sizeof(mbi)) || mbi.State!=MEM_COMMIT))break;
                const uint64_t v=sp[i];
                if(v>lo && v<hi) {caller=v;++found;if(r>lo && r<hi)break;}   // inside the EXE: the nearest caller is enough
            }
        }
        ResumeThread(thread_handle);
        if(r){rip[at]=r;ret[at]=caller;InterlockedIncrement(&count);}
        others();
        Sleep(1);
    }
}
static void enter() {
    if(!thread_handle) {
        bounds();
        DuplicateHandle(GetCurrentProcess(),GetCurrentThread(),GetCurrentProcess(),&thread_handle,THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,0);
        if(thread_handle)worker=CreateThread(nullptr,0,run,nullptr,0,nullptr);
    }
    if(!worker)return;
    LARGE_INTEGER now;QueryPerformanceCounter(&now);started=now.QuadPart;InterlockedExchange(&count,0);InterlockedExchange(&ocount,0);InterlockedExchange(&active,1);
}
static uint32_t place(uint64_t a) {
    if(a>lo && a<hi)return static_cast<uint32_t>(a-lo);
    HMODULE m=nullptr;
    if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(a),&m) && m)
        return 0xF0000000u|static_cast<uint32_t>((a-reinterpret_cast<uintptr_t>(m))&0x0FFFFFFFu);
    return 0xFFFFFFFFu;
}
static void leave(LONG64 us) {
    if(!worker)return;
    InterlockedExchange(&active,0);
    if(us<50000)return;
    const LONG n=InterlockedCompareExchange(&count,0,0);
    // group by (rip rounded to 64 bytes, caller); log the top six
    struct Bin {uint32_t at,caller;unsigned n;} bins[Max];unsigned used=0;
    for(LONG i=0;i<n;++i) {
        const uint32_t a=place(rip[i])&~0x3Fu,c=ret[i]?place(ret[i]):0;unsigned k=0;
        for(;k<used;++k)if(bins[k].at==a && bins[k].caller==c){++bins[k].n;break;}
        if(k==used && used<Max)bins[used++]={a,c,1};
    }
    log_event("tree_spike",static_cast<unsigned>(us/1000),static_cast<unsigned>(n),used);
    for(unsigned t=0;t<6 && used;++t) {
        unsigned best=0;for(unsigned k=1;k<used;++k)if(bins[k].n>bins[best].n)best=k;
        if(!bins[best].n)break;
        log_event("tree_spike_at",bins[best].at,bins[best].n,bins[best].caller);bins[best].n=0;
    }
    // modules outside the EXE the main thread sat in (up to two): army = first four letters packed, value = samples, hex = offset
    HMODULE mods[2]={};unsigned mod_n[2]={};uint32_t mod_off[2]={};
    for(LONG i=0;i<n;++i) {
        if(rip[i]>lo && rip[i]<hi)continue;HMODULE m=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(rip[i]),&m) || !m)continue;
        for(unsigned k=0;k<2;++k){if(!mods[k]){mods[k]=m;mod_off[k]=static_cast<uint32_t>(rip[i]-reinterpret_cast<uintptr_t>(m));}if(mods[k]==m){++mod_n[k];break;}}
    }
    for(unsigned k=0;k<2;++k) {
        if(!mods[k])continue;wchar_t path[MAX_PATH]={};GetModuleFileNameW(mods[k],path,MAX_PATH);
        const wchar_t *b=path;for(const wchar_t *q=path;*q;++q)if(*q==L'\\' || *q==L'/')b=q+1;
        uint32_t tag=0;for(unsigned j=0;j<4 && b[j];++j)tag|=static_cast<uint32_t>(static_cast<unsigned char>(b[j]))<<(8*j);
        log_event("tree_spike_mod",tag,mod_n[k],mod_off[k]);
    }
    // other threads' EXE places during the call: the jobs the tree job waits for
    const LONG on=InterlockedCompareExchange(&ocount,0,0);
    static Bin obins[OMax];unsigned oused=0;
    for(LONG i=0;i<on;++i) {
        const uint32_t a=static_cast<uint32_t>(orip[i]-lo)&~0x3Fu,c=oret[i]?static_cast<uint32_t>(oret[i]-lo):0;unsigned k=0;
        for(;k<oused;++k)if(obins[k].at==a && obins[k].caller==c){++obins[k].n;break;}
        if(k==oused && oused<OMax)obins[oused++]={a,c,1};
    }
    log_event("tree_spike_jobs",static_cast<unsigned>(on),oused,others_n);
    for(unsigned t=0;t<10 && oused;++t) {
        unsigned best=0;for(unsigned k=1;k<oused;++k)if(obins[k].n>obins[best].n)best=k;
        if(!obins[best].n)break;
        log_event("tree_spike_job",obins[best].at,obins[best].n,obins[best].caller);obins[best].n=0;
    }
}
}
#endif
static uint64_t __fastcall update(Obj s,Obj second,Obj context,Obj view,Obj fifth,unsigned sixth,unsigned seventh,unsigned eighth) { COST_SCOPE("terrain_trees::update");
    TERRAIN_SCOPE(TreesUpdate);
#ifndef NO_RUNTIME_LOG
    static volatile LONG thread_logged=0;
    if(!InterlockedCompareExchange(&thread_logged,1,0))log_event("terrain_trees_thread",GetCurrentThreadId(),0,0);
    LARGE_INTEGER perf_start;QueryPerformanceCounter(&perf_start);
#endif
    {COST_SCOPE("terrain_trees:apply_pending");apply_pending(s);}   // local144 probe: ours vs the original update below
#ifndef NO_RUNTIME_LOG
    LARGE_INTEGER perf_end;QueryPerformanceCounter(&perf_end);
    LONG64 perf_us=(perf_end.QuadPart-perf_start.QuadPart)*1000000/perf_frequency;
    InterlockedIncrement64(&perf_calls);InterlockedAdd64(&perf_total_us,perf_us);InterlockedAdd64(&perf_frame_us,perf_us);
    LONG64 perf_old=InterlockedCompareExchange64(&perf_max_us,0,0);
    while(perf_us>perf_old){LONG64 seen=InterlockedCompareExchange64(&perf_max_us,perf_us,perf_old);if(seen==perf_old)break;perf_old=seen;}
#endif
    TERRAIN_SCOPE(TreesNative);
    COST_SCOPE("terrain_trees:native");
#ifndef NO_RUNTIME_LOG
    if(diagnostic_logging) {
        spike::enter();
        LARGE_INTEGER a,b;QueryPerformanceCounter(&a);
        const uint64_t result=fn<Update>(UpdateRva)(s,second,context,view,fifth,sixth,seventh,eighth);
        QueryPerformanceCounter(&b);spike::leave((b.QuadPart-a.QuadPart)*1000000/perf_frequency);
        return result;
    }
#endif
    return fn<Update>(UpdateRva)(s,second,context,view,fifth,sixth,seventh,eighth);
}
}
static bool terrain_trees_guards_match() {return terrain_trees::guards_match();}
