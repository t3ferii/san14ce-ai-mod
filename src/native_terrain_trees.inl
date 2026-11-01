// User/Claude 2026-10-29: ResourceMap edits on the native placement callback only.
#pragma once
#define SAN14_TERRAIN_TREES_INCLUDED 1
#include <vector>
#include <cmath>
namespace terrain_trees {
#ifndef NO_RUNTIME_LOG
// Completion counters are sampled/reset by Present, without taking the tree lock.
alignas(8) static volatile LONG64 perf_calls,perf_total_us,perf_frame_us,perf_max_us;
static const LONGLONG perf_frequency=[] {LARGE_INTEGER f={};QueryPerformanceFrequency(&f);return f.QuadPart?f.QuadPart:1;}();
#endif
static constexpr unsigned Slot=0x1694958,UpdateRva=0xBB10F0,SetterRva=0xB91330;
static constexpr unsigned Size=1024,HexCount=48400,FrameLimit=64;
static constexpr unsigned char update_bytes[16]={0x40,0x55,0x53,0x56,0x41,0x55,0x41,0x56,0x48,0x8D,0xAC,0x24,0x10,0xE7,0xFF,0xFF};
static constexpr unsigned char setter_bytes[16]={0x48,0x89,0x5C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x48,0x89,0x54,0x24,0x10,0x57};
// Decoded EXE SHA256 1fbe6be7... (2026-10-29), BB10F0..BB1412:
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
        !memcmp(image+UpdateRva,update_bytes,sizeof(update_bytes)) &&
        !memcmp(image+SetterRva,setter_bytes,sizeof(setter_bytes));
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
    if(!readable(s,0x70D8) || pointer(s,0)!=image+0x1694928 ||
       *reinterpret_cast<unsigned *>(s+0x7084)!=Size || *reinterpret_cast<unsigned *>(s+0x7088)!=Size ||
       *reinterpret_cast<unsigned *>(s+0x708C)!=128 || *reinterpret_cast<unsigned *>(s+0x7090)!=128 || s[0x70B4]!=1)return false;
    r=pointer(s,0x70D0);
    if(!readable(r,0x98) || pointer(r,0)!=image+0x1692770 ||
       *reinterpret_cast<unsigned *>(r+0x14)!=Size || *reinterpret_cast<unsigned *>(r+0x18)!=Size)return false;
    data=pointer(r,0x58);return readable(data,static_cast<size_t>(Size)*Size);
}
// Only the original placement callback calls this function. Keep its SRW lock
// through native setters so UI goals cannot change halfway through a hex.
static void apply_pending(Obj s) {
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
            fn<Set>(SetterRva)(r,static_cast<uint64_t>(static_cast<unsigned>(x))|
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
static uint64_t __fastcall update(Obj s,Obj second,Obj context,Obj view,Obj fifth,unsigned sixth,unsigned seventh,unsigned eighth) { COST_SCOPE("terrain_trees::update");
#ifndef NO_RUNTIME_LOG
    static volatile LONG thread_logged=0;
    if(!InterlockedCompareExchange(&thread_logged,1,0))log_event("terrain_trees_thread",GetCurrentThreadId(),0,0);
    LARGE_INTEGER perf_start;QueryPerformanceCounter(&perf_start);
#endif
    apply_pending(s);
#ifndef NO_RUNTIME_LOG
    LARGE_INTEGER perf_end;QueryPerformanceCounter(&perf_end);
    LONG64 perf_us=(perf_end.QuadPart-perf_start.QuadPart)*1000000/perf_frequency;
    InterlockedIncrement64(&perf_calls);InterlockedAdd64(&perf_total_us,perf_us);InterlockedAdd64(&perf_frame_us,perf_us);
    LONG64 perf_old=InterlockedCompareExchange64(&perf_max_us,0,0);
    while(perf_us>perf_old){LONG64 seen=InterlockedCompareExchange64(&perf_max_us,perf_us,perf_old);if(seen==perf_old)break;perf_old=seen;}
#endif
    return fn<Update>(UpdateRva)(s,second,context,view,fifth,sixth,seventh,eighth);
}
}
static bool terrain_trees_guards_match() {return terrain_trees::guards_match();}
