#pragma once
// Diagnostic-only terrain call accounting; emit through the existing log_event sink.
// No new hooks, game calls, files, worker threads or settings. CPU is current-thread
// kernel+user time (100 ns units); cycles are also kept because short CPU times can quantize to zero.
#if !defined(NO_RUNTIME_LOG) && !defined(SPLIT_COMBAT)
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <utility>
namespace terrain_profile {
#define TERRAIN_PROFILE_KEYS(X) \
    X(FoamCreate,"foam_create_hook") X(FoamCreateNative,"foam_create_E17CA0") \
    X(FoamComplete,"foam_complete_hook") X(FoamCompleteNative,"foam_complete_E1B580") \
    X(FoamDestroy,"foam_destroy_hook") X(FoamDestroyNative,"foam_destroy_E18280") X(FoamTime,"foam_time_hook") \
    X(FoamTimeNative,"foam_time_E1B7B0") X(FoamRender,"foam_render_hook") \
    X(FoamRenderNative,"foam_render_E19690") X(FoamObserve,"foam_observe") \
    X(FoamPlan,"foam_plan") X(FoamApply,"foam_apply") \
    X(FoamPosition,"foam_pos_E1AAE0") X(FoamRotation,"foam_rot_E1ADD0") X(FoamScale,"foam_scale_E1B160") \
    X(TextureHook,"texture_hook") X(TextureNative,"texture_create_D3D11") \
    X(RenderDevice,"render_on_device") X(RenderUpload,"render_on_present") \
    X(UploadNative,"upload_UpdateSubresource") X(Readback,"render_readback") \
    X(MapNative,"readback_Map") X(CopyNative,"readback_Copy") \
    X(HeightCapture,"height_capture") X(HeightNative,"height_A07B70") \
    X(HeightCell,"height_cell") X(CpuCell,"height_cpu_cell") \
    X(MaskFit,"mask_fit") X(TreesApply,"trees_apply") X(TreesUpdate,"trees_update_hook") \
    X(TreesNative,"trees_BB10F0") X(TreesSet,"trees_B91330") \
    X(TerrainSync,"terrain_sync") X(TerrainWorld,"terrain_check_world") \
    X(PresentHook,"present_hook") X(PresentNative,"present_DXGI") X(SeasonFrame,"season_bake_frame")
#define TERRAIN_ENUM(id,name) id,
enum class Id : unsigned {TERRAIN_PROFILE_KEYS(TERRAIN_ENUM) Count};
#undef TERRAIN_ENUM
#define TERRAIN_NAME(id,name) name,
inline const char *const names[]={TERRAIN_PROFILE_KEYS(TERRAIN_NAME)};
#undef TERRAIN_NAME
#undef TERRAIN_PROFILE_KEYS
constexpr unsigned Count=static_cast<unsigned>(Id::Count);
using Emit=void (*)(const char *,unsigned,unsigned,unsigned);
struct Stamp {uint64_t wall,cpu,cycles;bool cpu_ok,cycles_ok;};
inline uint64_t filetime(FILETIME f) {return (uint64_t(f.dwHighDateTime)<<32)|f.dwLowDateTime;}
inline Stamp read_stamp() {
    FILETIME c={},e={},k={},u={};ULONG64 cycles=0;LARGE_INTEGER q={};
    bool cpu=GetThreadTimes(GetCurrentThread(),&c,&e,&k,&u)!=0;
    bool cyc=QueryThreadCycleTime(GetCurrentThread(),&cycles)!=0;
    QueryPerformanceCounter(&q);return {uint64_t(q.QuadPart),filetime(k)+filetime(u),cycles,cpu,cyc};
}
#ifdef TERRAIN_CALL_PROFILE_FIXTURE
inline Stamp (*fixture_stamp)();
#endif
inline Stamp sample() {
#ifdef TERRAIN_CALL_PROFILE_FIXTURE
    if(fixture_stamp)return fixture_stamp();
#endif
    return read_stamp();
}
// local268 (user 2026-11-19 "지형그리기 쪽도", Fable advice): the local267 run spent about 3 us per foam call in this profiler
// (two scopes, six clock syscalls, one global exclusive lock shared by the foam threads and the render thread), about 31 s
// of a 9.6 minute run, and its CPU column read above wall. Now:
//   - rows live in kStripes stripes picked by thread id, each with its own lock; a report merges them;
//   - hot keys (hot() below, millions of calls) are measured on one call in 2^sample_shift: a measured call counts with that
//     weight, its nested scopes inherit it, and the calls between are not timed at all (their nested scopes neither);
//     a hot key nested in a weight-1 scope is sampled the same way and adds its weighted time to the parent's children;
//   - a weighted call reads QPC only (its CPU/cycle columns stay empty); weight-1 scopes keep the thread CPU and cycles;
//   - self time is a signed sum, so sampled children never clip a parent's self time to zero.
// Counts and times of hot keys are estimates (weight x sampled); maxima come from the sampled calls.
struct Row {
    uint64_t calls,wall,cpu,cycles;int64_t self_wall,self_cpu;uint64_t max_wall,max_cpu,max_cycles,peak_cpu;
    unsigned cpu_samples,cycle_samples,first_frame,last_frame,peak_frame,thread_id,peak_thread,peak_start;
};
inline SRWLOCK report_guard=SRWLOCK_INIT;
constexpr unsigned kStripes=8;
struct Stripe {SRWLOCK lock;Row rows[Count];};
inline Stripe stripes[kStripes];
inline unsigned sample_shift=5;   // hot keys: one call in 32 measured (the fixture sets 0 to time every call)
inline bool hot(Id id) {
    switch(id) {
    case Id::FoamTime:case Id::FoamRender:case Id::FoamPosition:case Id::FoamRotation:case Id::FoamScale:case Id::UploadNative:return true;
    default:return false;
    }
}
inline __declspec(thread) unsigned thread_stripe,hot_counter,suppressed;
inline Stripe &my_stripe() {
    if(!thread_stripe)thread_stripe=1+(GetCurrentThreadId()>>2)%kStripes;
    return stripes[thread_stripe-1];
}
inline void merge_row(Row &out,const Row &r) {
    if(!r.calls)return;
    if(!out.calls) {out=r;return;}
    out.calls+=r.calls;out.wall+=r.wall;out.cpu+=r.cpu;out.cycles+=r.cycles;out.self_wall+=r.self_wall;out.self_cpu+=r.self_cpu;
    out.cpu_samples+=r.cpu_samples;out.cycle_samples+=r.cycle_samples;
    if(r.max_wall>=out.max_wall) {out.max_wall=r.max_wall;out.peak_cpu=r.peak_cpu;out.peak_frame=r.peak_frame;out.peak_thread=r.peak_thread;out.peak_start=r.peak_start;}
    if(r.max_cpu>out.max_cpu)out.max_cpu=r.max_cpu;
    if(r.max_cycles>out.max_cycles)out.max_cycles=r.max_cycles;
    if(r.first_frame<out.first_frame)out.first_frame=r.first_frame;
    if(r.last_frame>out.last_frame)out.last_frame=r.last_frame;
    if(out.thread_id!=r.thread_id)out.thread_id=0xFFFFFFFFu;
}
inline Row row(Id id) {   // merged view of one key (reports, fixture)
    Row out{};
    for(Stripe &s:stripes) {AcquireSRWLockShared(&s.lock);merge_row(out,s.rows[static_cast<unsigned>(id)]);ReleaseSRWLockShared(&s.lock);}
    return out;
}
inline bool running;
inline Emit emit;
inline uint64_t frequency,previous_wall,previous_process,report_wall,report_process,previous_thread;
inline volatile LONG frame=1;
inline unsigned previous_thread_id;
inline bool previous_thread_valid;
inline unsigned field(uint64_t v) {return v>0xFFFFFFFFu?0xFFFFFFFFu:static_cast<unsigned>(v);}
inline uint64_t us(uint64_t ticks) {return frequency?ticks*1000000/frequency:0;}
inline bool process_cpu(uint64_t &out) {
    FILETIME c={},e={},k={},u={};
    if(!GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u))return false;
    out=filetime(k)+filetime(u);return true;
}
inline void initialize(bool on,Emit sink) {
    // Called once before callbacks publish; an inactive profiler performs no clock queries.
    emit=sink;LARGE_INTEGER f={};
    if(!on || !sink || !QueryPerformanceFrequency(&f) || f.QuadPart<=0){running=false;return;}
    frequency=uint64_t(f.QuadPart);Stamp now=read_stamp();uint64_t cpu=0;
    if(!process_cpu(cpu)){running=false;return;}
    previous_wall=report_wall=now.wall;previous_process=report_process=cpu;
    previous_thread_id=0;previous_thread_valid=false;running=true;
    SYSTEM_INFO info={};GetSystemInfo(&info);
    emit("terrain_profile_on",info.dwNumberOfProcessors,75000,1000000);
}
struct Scope;
inline __declspec(thread) Scope *current;
inline Stamp quick() {   // weighted calls: wall clock only
#ifdef TERRAIN_CALL_PROFILE_FIXTURE
    if(fixture_stamp) {Stamp s=fixture_stamp();s.cpu=s.cycles=0;s.cpu_ok=s.cycles_ok=false;return s;}
#endif
    LARGE_INTEGER q={};QueryPerformanceCounter(&q);return {uint64_t(q.QuadPart),0,0,false,false};
}
struct Scope {
    Id id;Scope *parent=nullptr;Stamp begin={};int64_t child_wall=0,child_cpu=0;unsigned start_frame=0,weight=1;
    bool active=false,quiet=false;
    explicit Scope(Id key):id(key) {
        if(!running || static_cast<unsigned>(id)>=Count || suppressed)return;
        Scope *p=current;
        if(p && p->weight>1)weight=p->weight;   // inside a measured hot call: same weight
        else if(hot(id) && sample_shift) {
            if((++hot_counter&((1u<<sample_shift)-1))!=0) {quiet=true;++suppressed;return;}
            weight=1u<<sample_shift;
        }
        active=true;start_frame=static_cast<unsigned>(InterlockedCompareExchange(&frame,0,0));
        parent=p;current=this;begin=weight>1?quick():sample();
    }
    Scope(const Scope &)=delete;Scope &operator=(const Scope &)=delete;
    ~Scope() {
        if(quiet) {--suppressed;return;}
        if(!active)return;
        Stamp end=weight>1?quick():sample();uint64_t wall=end.wall>=begin.wall?end.wall-begin.wall:0;
        bool cpu_ok=begin.cpu_ok && end.cpu_ok && end.cpu>=begin.cpu;
        bool cyc_ok=begin.cycles_ok && end.cycles_ok && end.cycles>=begin.cycles;
        uint64_t cpu=cpu_ok?end.cpu-begin.cpu:0,cycles=cyc_ok?end.cycles-begin.cycles:0;
        current=parent;
        if(parent) {   // in the parent's units: a sampled child stands for weight / parent weight calls
            const int64_t scale=parent->weight?static_cast<int64_t>(weight/parent->weight):1;
            parent->child_wall+=static_cast<int64_t>(wall)*scale;parent->child_cpu+=static_cast<int64_t>(cpu)*scale;
        }
        const uint64_t w=weight;const unsigned tid=GetCurrentThreadId();
        Stripe &s=my_stripe();
        AcquireSRWLockExclusive(&s.lock);
        Row &r=s.rows[static_cast<unsigned>(id)];
        if(!r.calls){r.first_frame=start_frame;r.thread_id=tid;}
        else if(r.thread_id!=tid)r.thread_id=0xFFFFFFFFu;
        r.calls+=w;r.wall+=wall*w;r.cpu+=cpu*w;r.cycles+=cycles*w;
        r.cpu_samples+=cpu_ok?1u:0u;r.cycle_samples+=cyc_ok?1u:0u;
        r.self_wall+=(static_cast<int64_t>(wall)-child_wall)*static_cast<int64_t>(w);
        r.self_cpu+=cpu_ok?(static_cast<int64_t>(cpu)-child_cpu)*static_cast<int64_t>(w):0;
        r.last_frame=static_cast<unsigned>(frame);if(wall>=r.max_wall){
            r.max_wall=wall;r.peak_cpu=cpu;r.peak_frame=static_cast<unsigned>(frame);
            r.peak_thread=tid;r.peak_start=start_frame;
        }
        if(cpu>r.max_cpu)r.max_cpu=cpu;if(cycles>r.max_cycles)r.max_cycles=cycles;
        ReleaseSRWLockExclusive(&s.lock);
    }
};
template<class F,class... A> inline decltype(auto) invoke(Id id,F &&fn,A &&... args) {
    Scope scope(id);return std::forward<F>(fn)(std::forward<A>(args)...);
}
// A completion is attributed to the current Present-to-Present window. Spanning calls
// retain first/last frame IDs; per-key maxima need not occur at the same call.
inline void checkpoint() {
    if(!running || !TryAcquireSRWLockExclusive(&report_guard))return;
    Stamp now=read_stamp();uint64_t cpu=0;if(!process_cpu(cpu)){ReleaseSRWLockExclusive(&report_guard);return;}
    uint64_t gap=now.wall>=previous_wall?now.wall-previous_wall:0;
    uint64_t process_delta=cpu>=previous_process?cpu-previous_process:0;
    unsigned tid=GetCurrentThreadId();bool thread_ok=now.cpu_ok && previous_thread_valid && tid==previous_thread_id && now.cpu>=previous_thread;
    uint64_t thread_delta=thread_ok?now.cpu-previous_thread:0;
    previous_wall=now.wall;previous_process=cpu;previous_thread=now.cpu;previous_thread_id=tid;previous_thread_valid=now.cpu_ok;
    static Row snapshot[Count];unsigned completed;   // under report_guard
    completed=static_cast<unsigned>(InterlockedIncrement(&frame))-1;
    bool report=us(gap)>=75000 || us(now.wall-report_wall)>=1000000;
    if(report) {
        for(unsigned i=0;i<Count;++i)snapshot[i]={};
        for(Stripe &s:stripes) {
            AcquireSRWLockExclusive(&s.lock);
            for(unsigned i=0;i<Count;++i) {merge_row(snapshot[i],s.rows[i]);s.rows[i]={};}
            ReleaseSRWLockExclusive(&s.lock);
        }
    }
    if(!report){ReleaseSRWLockExclusive(&report_guard);return;}
    emit("terrain_frame",completed,field(us(gap)),field(us(now.wall-report_wall)));
    emit("terrain_frame_cpu",completed,field(process_delta/10),thread_ok?field(thread_delta/10):0xFFFFFFFFu);
    emit("terrain_cpu_window",completed,field(cpu>=report_process?(cpu-report_process)/10:0),field(us(now.wall-report_wall)));
    report_wall=now.wall;report_process=cpu;
    for(unsigned i=0;i<Count;++i){const Row &r=snapshot[i];if(!r.calls)continue;
        char key[96];
#define TERRAIN_EMIT(prefix,b,c) do {_snprintf_s(key,sizeof(key),_TRUNCATE,prefix "%s",names[i]);emit(key,completed,field(b),field(c));}while(0)
        TERRAIN_EMIT("terrain_wall_",us(r.wall),us(r.max_wall));
        TERRAIN_EMIT("terrain_cpu_",r.cpu/10,r.max_cpu/10);
        TERRAIN_EMIT("terrain_self_",us(r.self_wall>0?static_cast<uint64_t>(r.self_wall):0),r.self_cpu>0?static_cast<uint64_t>(r.self_cpu)/10:0);
        TERRAIN_EMIT("terrain_calls_",r.calls,r.thread_id);
        TERRAIN_EMIT("terrain_cycles_",r.cycles/1000,r.max_cycles/1000);
        TERRAIN_EMIT("terrain_range_",r.first_frame,r.last_frame);
        _snprintf_s(key,sizeof(key),_TRUNCATE,"terrain_peak_%s",names[i]);
        emit(key,r.peak_frame,field(us(r.max_wall)),field(r.peak_cpu/10));
        _snprintf_s(key,sizeof(key),_TRUNCATE,"terrain_peak_thread_%s",names[i]);
        emit(key,r.peak_frame,r.peak_thread,r.peak_start);
        TERRAIN_EMIT("terrain_valid_",r.cpu_samples,r.cycle_samples);
#undef TERRAIN_EMIT
    }
    ReleaseSRWLockExclusive(&report_guard);
}
inline __declspec(thread) unsigned present_depth;
struct Frame {
    bool entered,active;
    explicit Frame(bool real):entered(real && running),active(entered && ++present_depth==1){}
    ~Frame(){if(entered)--present_depth;if(active)checkpoint();}
};
// local266 diagnostics (user 2026-11-19 "다른 모드 최적화 할 수 있는것도"): who calls a hook. Each call adds its return address
// (exe rva; SAN14 DLL 0x80000000 | dll << 24 | offset with 1 combat, 2 convenience, 3 layout, 4 ccz, as path_caller_*;
// any other module 0xFFFFFFFF) and its wall time; every `period` calls the eight busiest are logged as <name>
// (code, calls, us) and the table restarts.
struct CallerTally {
    static constexpr unsigned kSlots=32;
    struct Slot {unsigned code,calls;uint64_t ticks;};
    const char *name;unsigned period;
    Slot slots[kSlots]={};unsigned used=0,total=0;SRWLOCK lock=SRWLOCK_INIT;
    static unsigned code(void *address) {
        HMODULE m=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(address),&m) || !m)return 0xFFFFFFFFu;
        const uintptr_t offset=reinterpret_cast<uintptr_t>(address)-reinterpret_cast<uintptr_t>(m);
        if(m==GetModuleHandleW(nullptr))return static_cast<unsigned>(offset);
        const wchar_t *dlls[4]={L"SAN14_CombatAI.dll",L"SAN14_Convenience.dll",L"SAN14_Layout.dll",L"SAN14_CCZ.dll"};
        for(unsigned i=0;i<4;++i)if(m==GetModuleHandleW(dlls[i]))return 0x80000000u|((i+1)<<24)|static_cast<unsigned>(offset&0xFFFFFF);
        return 0xFFFFFFFFu;
    }
    void add(void *address,uint64_t ticks) {
        const unsigned c=code(address);
        AcquireSRWLockExclusive(&lock);
        unsigned i=0;while(i<used && slots[i].code!=c)++i;
        if(i==used && used<kSlots)slots[used++]={c,0,0};
        if(i<used) {++slots[i].calls;slots[i].ticks+=ticks;}
        if(++total>=period) {   // logged through the terrain profile sink (emit), present while terrain_profile_on
            LARGE_INTEGER hz;QueryPerformanceFrequency(&hz);
            for(unsigned rank=0;emit && rank<8 && rank<used;++rank) {
                unsigned best=rank;for(unsigned k=rank+1;k<used;++k)if(slots[k].ticks>slots[best].ticks)best=k;
                std::swap(slots[rank],slots[best]);
                emit(name,slots[rank].code,slots[rank].calls,static_cast<unsigned>(slots[rank].ticks*1000000/static_cast<uint64_t>(hz.QuadPart)));
            }
            used=total=0;
        }
        ReleaseSRWLockExclusive(&lock);
    }
};
struct CallerScope {
    CallerTally &tally;void *address;LARGE_INTEGER start;
    CallerScope(CallerTally &t,void *a):tally(t),address(a) {QueryPerformanceCounter(&start);}
    ~CallerScope() {LARGE_INTEGER end;QueryPerformanceCounter(&end);tally.add(address,static_cast<uint64_t>(end.QuadPart-start.QuadPart));}
};
}
#include <intrin.h>
#define TERRAIN_JOIN_(a,b) a##b
#define TERRAIN_JOIN(a,b) TERRAIN_JOIN_(a,b)
#define TERRAIN_SCOPE(id) terrain_profile::Scope TERRAIN_JOIN(terrain_scope_,__COUNTER__)(terrain_profile::Id::id)
// One per hook function: a fixed name keeps the unity and unit preprocessed texts equal (tools/unit_equivalence.py).
#define TERRAIN_CALLER(tally) terrain_profile::CallerScope terrain_caller_scope(tally,_ReturnAddress())
#else
#include <utility>
namespace terrain_profile {
// Names remain available to forwarding wrappers; all measurement/logging code is absent.
enum class Id {FoamCreateNative,FoamCompleteNative,FoamPosition,FoamRotation,FoamScale,TextureNative,UploadNative,MapNative,CopyNative,TreesSet,HeightNative};
template<class F,class... A> inline decltype(auto) invoke(Id,F &&fn,A &&... args) {return std::forward<F>(fn)(std::forward<A>(args)...);}
}
#define TERRAIN_SCOPE(id) do {} while(0)
#define TERRAIN_CALLER(tally) do {} while(0)
#endif

namespace terrain_profile {
template<class F,Id Key> struct Function {
    F target;
    operator F() const {return target;}
    Function &operator=(F value){target=value;return *this;}
    template<class... A> decltype(auto) operator()(A &&... args) const {
        return invoke(Key,target,std::forward<A>(args)...);
    }
};
}
