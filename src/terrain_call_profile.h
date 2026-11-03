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
    X(PresentHook,"present_hook") X(PresentNative,"present_DXGI")
#define TERRAIN_ENUM(id,name) id,
enum class Id : unsigned {TERRAIN_PROFILE_KEYS(TERRAIN_ENUM) Count};
#undef TERRAIN_ENUM
#define TERRAIN_NAME(id,name) name,
static const char *const names[]={TERRAIN_PROFILE_KEYS(TERRAIN_NAME)};
#undef TERRAIN_NAME
#undef TERRAIN_PROFILE_KEYS
constexpr unsigned Count=static_cast<unsigned>(Id::Count);
using Emit=void (*)(const char *,unsigned,unsigned,unsigned);
struct Stamp {uint64_t wall,cpu,cycles;bool cpu_ok,cycles_ok;};
static uint64_t filetime(FILETIME f) {return (uint64_t(f.dwHighDateTime)<<32)|f.dwLowDateTime;}
static Stamp read_stamp() {
    FILETIME c={},e={},k={},u={};ULONG64 cycles=0;LARGE_INTEGER q={};
    bool cpu=GetThreadTimes(GetCurrentThread(),&c,&e,&k,&u)!=0;
    bool cyc=QueryThreadCycleTime(GetCurrentThread(),&cycles)!=0;
    QueryPerformanceCounter(&q);return {uint64_t(q.QuadPart),filetime(k)+filetime(u),cycles,cpu,cyc};
}
#ifdef TERRAIN_CALL_PROFILE_FIXTURE
static Stamp (*fixture_stamp)();
#endif
static Stamp sample() {
#ifdef TERRAIN_CALL_PROFILE_FIXTURE
    if(fixture_stamp)return fixture_stamp();
#endif
    return read_stamp();
}
struct Row {
    uint64_t calls,wall,cpu,cycles,self_wall,self_cpu,max_wall,max_cpu,max_cycles,peak_cpu;
    unsigned cpu_samples,cycle_samples,first_frame,last_frame,peak_frame,thread_id,peak_thread,peak_start;
};
static SRWLOCK guard=SRWLOCK_INIT;
static SRWLOCK report_guard=SRWLOCK_INIT;
static Row rows[Count];
static bool running;
static Emit emit;
static uint64_t frequency,previous_wall,previous_process,report_wall,report_process,previous_thread;
static volatile LONG frame=1;
static unsigned previous_thread_id;
static bool previous_thread_valid;
static unsigned field(uint64_t v) {return v>0xFFFFFFFFu?0xFFFFFFFFu:static_cast<unsigned>(v);}
static uint64_t us(uint64_t ticks) {return frequency?ticks*1000000/frequency:0;}
static bool process_cpu(uint64_t &out) {
    FILETIME c={},e={},k={},u={};
    if(!GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u))return false;
    out=filetime(k)+filetime(u);return true;
}
static void initialize(bool on,Emit sink) {
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
static __declspec(thread) Scope *current;
struct Scope {
    Id id;Scope *parent=nullptr;Stamp begin={};uint64_t child_wall=0,child_cpu=0;unsigned start_frame=0;bool active=false;
    explicit Scope(Id key):id(key) {
        if(!running || static_cast<unsigned>(id)>=Count)return;
        active=true;start_frame=static_cast<unsigned>(InterlockedCompareExchange(&frame,0,0));
        parent=current;current=this;begin=sample();
    }
    Scope(const Scope &)=delete;Scope &operator=(const Scope &)=delete;
    ~Scope() {
        if(!active)return;
        Stamp end=sample();uint64_t wall=end.wall>=begin.wall?end.wall-begin.wall:0;
        bool cpu_ok=begin.cpu_ok && end.cpu_ok && end.cpu>=begin.cpu;
        bool cyc_ok=begin.cycles_ok && end.cycles_ok && end.cycles>=begin.cycles;
        uint64_t cpu=cpu_ok?end.cpu-begin.cpu:0,cycles=cyc_ok?end.cycles-begin.cycles:0;
        current=parent;if(parent){parent->child_wall+=wall;parent->child_cpu+=cpu;}
        AcquireSRWLockExclusive(&guard);
        Row &r=rows[static_cast<unsigned>(id)];
        if(!r.calls){r.first_frame=start_frame;r.thread_id=GetCurrentThreadId();}
        else if(r.thread_id!=GetCurrentThreadId())r.thread_id=0xFFFFFFFFu;
        ++r.calls;r.wall+=wall;r.cpu+=cpu;r.cycles+=cycles;
        r.cpu_samples+=cpu_ok?1u:0u;r.cycle_samples+=cyc_ok?1u:0u;
        r.self_wall+=wall>child_wall?wall-child_wall:0;r.self_cpu+=cpu>child_cpu?cpu-child_cpu:0;
        r.last_frame=static_cast<unsigned>(frame);if(wall>=r.max_wall){
            r.max_wall=wall;r.peak_cpu=cpu;r.peak_frame=static_cast<unsigned>(frame);
            r.peak_thread=GetCurrentThreadId();r.peak_start=start_frame;
        }
        if(cpu>r.max_cpu)r.max_cpu=cpu;if(cycles>r.max_cycles)r.max_cycles=cycles;
        ReleaseSRWLockExclusive(&guard);
    }
};
template<class F,class... A> static decltype(auto) invoke(Id id,F &&fn,A &&... args) {
    Scope scope(id);return std::forward<F>(fn)(std::forward<A>(args)...);
}
// A completion is attributed to the current Present-to-Present window. Spanning calls
// retain first/last frame IDs; per-key maxima need not occur at the same call.
static void checkpoint() {
    if(!running || !TryAcquireSRWLockExclusive(&report_guard))return;
    Stamp now=read_stamp();uint64_t cpu=0;if(!process_cpu(cpu)){ReleaseSRWLockExclusive(&report_guard);return;}
    uint64_t gap=now.wall>=previous_wall?now.wall-previous_wall:0;
    uint64_t process_delta=cpu>=previous_process?cpu-previous_process:0;
    unsigned tid=GetCurrentThreadId();bool thread_ok=now.cpu_ok && previous_thread_valid && tid==previous_thread_id && now.cpu>=previous_thread;
    uint64_t thread_delta=thread_ok?now.cpu-previous_thread:0;
    previous_wall=now.wall;previous_process=cpu;previous_thread=now.cpu;previous_thread_id=tid;previous_thread_valid=now.cpu_ok;
    Row snapshot[Count]={};unsigned completed;
    AcquireSRWLockExclusive(&guard);completed=static_cast<unsigned>(frame);InterlockedIncrement(&frame);
    bool report=us(gap)>=75000 || us(now.wall-report_wall)>=1000000;
    if(report){for(unsigned i=0;i<Count;++i){snapshot[i]=rows[i];rows[i]={};}}
    ReleaseSRWLockExclusive(&guard);
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
        TERRAIN_EMIT("terrain_self_",us(r.self_wall),r.self_cpu/10);
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
static __declspec(thread) unsigned present_depth;
struct Frame {
    bool entered,active;
    explicit Frame(bool real):entered(real && running),active(entered && ++present_depth==1){}
    ~Frame(){if(entered)--present_depth;if(active)checkpoint();}
};
}
#define TERRAIN_JOIN_(a,b) a##b
#define TERRAIN_JOIN(a,b) TERRAIN_JOIN_(a,b)
#define TERRAIN_SCOPE(id) terrain_profile::Scope TERRAIN_JOIN(terrain_scope_,__COUNTER__)(terrain_profile::Id::id)
#else
#include <utility>
namespace terrain_profile {
// Names remain available to forwarding wrappers; all measurement/logging code is absent.
enum class Id {FoamCreateNative,FoamCompleteNative,FoamPosition,FoamRotation,FoamScale,TextureNative,UploadNative,MapNative,CopyNative,TreesSet,HeightNative};
template<class F,class... A> static decltype(auto) invoke(Id,F &&fn,A &&... args) {return std::forward<F>(fn)(std::forward<A>(args)...);}
}
#define TERRAIN_SCOPE(id) do {} while(0)
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
