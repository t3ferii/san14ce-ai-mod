// SAN14_Layout.dll P4 (user exception 2026-11-05, design docs/LAYOUT_DLL_COMPAT_DESIGN.md 4.4/4.5): layout lifetime
// events + runtime UI services + the one-build diagnostic probes (docs/LAYOUT_P4_DIAG.md).
//
// Observation points (neither is an entry patch of a function other mods fingerprint; no vtable slot is replaced):
//  * built: the E8 inside 86E320 (86E338, `lea rcx,[rbx+130h]; mov rdx,rbx; call 92B0A0`). 92B0A0 has no other caller, so
//    the call fires exactly when the layout vtable +0x140 (86E320, shared by 642 vtables) finished building the control table.
//    No reference DLL keeps the 86E320 body or this call window; IC only keeps vtable images (slot/value pairs) holding it.
//  * destroyed: entry of 865560 (~CUILayout body: every layout destructor ends in it, 200 E8 callers) through hook_block with a
//    6-byte trampoline (40 57 48 83 EC 30). No reference DLL keeps that function's body (256-bit window scan of 175 DLLs).
// The hooks are registered only when somebody subscribes or the diagnostic probes are compiled in (no behaviour change for a
// production build without subscribers).
//
// Original ABI used here (unpacked 1fbe6be7..., ghidra_full C + callers, see docs/LAYOUT_P4_DIAG.md for the evidence table):
//  3F6BC0(size) alloc | 864C50(obj) CUILayout ctor | 92AB70(obj+0x130) table ctor | vtable 1342530 then pane 13E4E80
//  86E0C0(obj,int x,int y,parent,name,0,async) load (86C570 sets obj+0x90 job, +0x98=job+0x20, +0x124/+0x128 = x/y, +0x12C=1)
//  92CEE0(table,node) control by node | 92E430(table,node,style,kind,1,0) control from an unbound node (table+8 [node] != 0)
//  92E0B0(table,node,obj,kind,1,0) attach | vt+0x1D8(button,text) | 85C9E0(label,text) | 8741D0(obj,on) visible | vt+0x78(obj,int x,int y)
//  1A3CB0(button,0x7FFFFFFE,&functor) click | 84B150(obj) label ctor + vt+0x138(label,x,y,w,h,parent,0) | vt[0](obj,1) destructor
#include "game_addresses.h"
namespace layout {
using Obj=Bytes *;
// ---- game function access (fixtures replace the functions; nothing here runs without the original UI thread) ----
#ifdef LAYOUT_FIXTURE
static void *(*game_resolver)(unsigned rva);
static bool fixture_probe;
#endif
static void *game_fn(unsigned rva) {
#ifdef LAYOUT_FIXTURE
    if(game_resolver)if(void *f=game_resolver(rva))return f;
#endif
    return image+rva;
}
template<class T> static T gf(unsigned rva) {return reinterpret_cast<T>(game_fn(rva));}
template<class F> static bool guarded(F &&body) {
    __try {body();return true;} __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static bool copy_safe(void *to,const void *from,size_t n) {
    __try {memcpy(to,from,n);return true;} __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static unsigned rd32(const void *base,unsigned off) {unsigned v=0;copy_safe(&v,static_cast<const Bytes *>(base)+off,sizeof(v));return v;}
static Obj rdptr(const void *base,unsigned off) {Obj v=nullptr;copy_safe(&v,static_cast<const Bytes *>(base)+off,sizeof(v));return v;}
static bool game_object(const void *p) {   // a pointer whose vtable lies inside the game image
    if(!p || !image)return false;
    Obj vt=rdptr(p,0);
    return vt>=image && vt<image+image_size;
}
static void *vslot(Obj object,unsigned offset) {Obj vt=rdptr(object,0);return vt?rdptr(vt,offset):nullptr;}
static int to_int(float v) {return v!=v?0:static_cast<int>(v+(v>=0.0f?0.5f:-0.5f));}
static unsigned name_hash31(const char *text) {   // 86C570: h*=31; sum+=(signed char)c*h
    unsigned h=1,sum=0;
    for(;text && *text;++text) {h*=31u;sum+=static_cast<unsigned>(static_cast<int>(static_cast<signed char>(*text))*static_cast<int>(h));}
    return sum;
}
// Offset of the KTID inside the layout resource handle (*(obj+0x90)). UNVERIFIED until the P4 diagnostic build reports it
// (layout_p4_ktid_cand); ~0u = unknown, KTID filters then never match. Fixtures set it.
static unsigned ktid_offset=~0u;
static unsigned read_ktid(Obj layout) {
    if(ktid_offset==~0u)return 0;
    Obj handle=rdptr(layout,0x90);
    return handle?rd32(handle,ktid_offset):0;
}
// ---- tracking ----
struct Live {Obj obj;unsigned ktid,name_hash;char name[48];};
struct Pending {Obj obj;unsigned hash;char name[48];int async;};
struct LayoutSub {void *cb;void *ctx;unsigned ktid;char name[48];bool destroyed;};
enum CreatedKind : unsigned {CK_PANE=1,CK_BUTTON=2,CK_LABEL=3};
struct Created {void *obj;unsigned kind;void *owner;};
// 2026-11-05 local3 (INSTALL_LOCAL2_TRIAGE 9-10): the live table was a 512-entry linear array that filled during a normal session
// (12,736 builds, 1,395 matched destroys). It is now an open-addressing hash table (linear probing, backward-shift erase) keyed by
// the layout address; a power of two, bounded probing, never grows. Overflow is still counted and logged once (table_full).
static const unsigned kLiveMax=8192,kPendingMax=64,kSubMax=32,kCreatedMax=128;
static Live live[kLiveMax];
static unsigned live_count;
static unsigned live_hash(const void *object) {
    return static_cast<unsigned>((static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(object)>>4)*0x9E3779B97F4A7C15ull)>>40)&(kLiveMax-1);
}
static int live_find_locked(const void *object) {
    unsigned i=live_hash(object);
    for(unsigned n=0;n<kLiveMax;++n,i=(i+1)&(kLiveMax-1)) {
        if(live[i].obj==object)return static_cast<int>(i);
        if(!live[i].obj)return -1;
    }
    return -1;
}
static bool live_insert_locked(const Live &info) {
    int at=live_find_locked(info.obj);
    if(at>=0) {live[at]=info;return true;}
    if(live_count>=kLiveMax)return false;
    unsigned i=live_hash(info.obj);
    while(live[i].obj)i=(i+1)&(kLiveMax-1);
    live[i]=info;++live_count;return true;
}
static bool live_erase_locked(const void *object,Live *out) {
    int at=live_find_locked(object);
    if(at<0)return false;
    if(out)*out=live[at];
    unsigned hole=static_cast<unsigned>(at),j=hole;
    live[hole]=Live{};--live_count;
    for(;;) {   // backward shift: later entries of the probe run move into the hole unless their home slot lies after it
        j=(j+1)&(kLiveMax-1);
        if(!live[j].obj)break;
        unsigned home=live_hash(live[j].obj);
        bool after_hole=hole<=j?(hole<home && home<=j):(hole<home || home<=j);
        if(after_hole)continue;
        live[hole]=live[j];live[j]=Live{};hole=j;
    }
    return true;
}
static Pending pending[kPendingMax];
static unsigned pending_next;
static LayoutSub layout_subs[kSubMax];
static Created created[kCreatedMax];
static unsigned created_count;
static SRWLOCK ui_lock=SRWLOCK_INIT;
static thread_local int dispatch_depth;
static volatile LONG pending_count;
static volatile LONG nested_dropped,async_loads,async_built,sync_built,destroyed_events,table_full,other_thread_calls,ui_rejected;
static volatile LONG first_thread;
struct DepthGuard {DepthGuard() {++dispatch_depth;} ~DepthGuard() {--dispatch_depth;}};
static void reject(const char *what,unsigned reason) {
    if(InterlockedIncrement(&ui_rejected)<=32)emit("layout_ui_rejected",hash32(what),reason,0);   // bounded: normal use never gets here
}
static int find_created_locked(const void *object) {
    for(unsigned i=0;i<created_count;++i)if(created[i].obj==object)return static_cast<int>(i);
    return -1;
}
static bool is_ours(const void *object) {
    AcquireSRWLockShared(&ui_lock);
    bool ours=find_created_locked(object)>=0;
    ReleaseSRWLockShared(&ui_lock);
    return ours;
}
// ---- observation hooks (state shared with the manager in native_layout_core.inl) ----
static void *volatile next_built;
static void *destroy_trampoline;
static int built_handle,destroy_handle;
static volatile LONG observation_registered;
static volatile LONG subscriber_total;
static const uint32_t kBuiltSite=GA(GA_86E338),kBuiltOriginal=GA(GA_92B0A0),kDestroyEntry=GA(GA_865560);
static const uint8_t kDestroyExpect[6]={0x40,0x57,0x48,0x83,0xEC,0x30};   // push rdi; sub rsp,30h (instruction boundary at 6)
static bool probe_enabled() {
#ifdef NO_RUNTIME_LOG
    return false;
#elif defined(LAYOUT_FIXTURE)
    return fixture_probe;
#else
    return true;
#endif
}
static bool observation_wanted() {return InterlockedCompareExchange(&subscriber_total,0,0)>0 || probe_enabled();}
static bool destroy_observed() {
    if(destroy_handle<=0 || static_cast<unsigned>(destroy_handle)>hook_total)return false;
    LONG state=InterlockedCompareExchange(&hooks[destroy_handle-1].state,0,0);
    return state==LAYOUT_STATE_INSTALLED || state==LAYOUT_STATE_CHAINED;
}
static void __fastcall built_hook(Obj table,Obj layout);
static void __fastcall destroy_hook(Obj self,unsigned long long a,unsigned long long b,unsigned long long c);
static Bytes *pool_alloc(unsigned bytes) {
    if(!pool) {pool=allocate_near(image,kPoolSize);pool_used=0;}
    bytes=(bytes+15u)&~15u;
    if(!pool || pool_used+bytes>kPoolSize)return nullptr;
    Bytes *at=pool+pool_used;pool_used+=bytes;return at;
}
// Registers the two observation hooks once (published: installed at once, otherwise by publish_hooks).
static void register_observation() {
    if(!image || InterlockedCompareExchange(&observation_registered,1,0)!=0)return;
    next_built=image+kBuiltOriginal;
    built_handle=add_hook("layout_events",kCall,kBuiltSite,kBuiltOriginal,reinterpret_cast<void *>(&built_hook),
        const_cast<void **>(&next_built),LAYOUT_CHAIN_OK,0,nullptr,nullptr,nullptr);
    if(image_size>kDestroyEntry+16 && !ga::mcmp_at(image,kDestroyEntry,kDestroyExpect,sizeof(kDestroyExpect))) {
        // trampoline = displaced instructions + absolute jump behind them (must be built before the entry is patched)
        if(Bytes *t=pool_alloc(32)) {
            memcpy(t,kDestroyExpect,sizeof(kDestroyExpect));
            t[6]=0xFF;t[7]=0x25;memset(t+8,0,4);
            *reinterpret_cast<void **>(t+12)=image+kDestroyEntry+sizeof(kDestroyExpect);
            FlushInstructionCache(GetCurrentProcess(),t,32);
            destroy_trampoline=t;
            destroy_handle=add_hook("layout_events_destroy",kBlock,kDestroyEntry,0,reinterpret_cast<void *>(&destroy_hook),
                nullptr,LAYOUT_REFUSE_IF_FOREIGN,sizeof(kDestroyExpect),kDestroyExpect,nullptr,nullptr);
        } else emit("layout_events_no_trampoline",kDestroyEntry,0,0);
    } else emit("layout_events_destroy_bytes",kDestroyEntry,0,0);
    emit("layout_events_registered",static_cast<unsigned>(built_handle),static_cast<unsigned>(destroy_handle),0);
}
// ---- subscriptions ----
static int add_sub(bool destroyed,unsigned ktid,const char *name,void *cb,void *ctx) {
    if(!cb)return -1;
    if(name && strlen(name)>=sizeof(layout_subs[0].name))return -1;
    int result=-3;
    AcquireSRWLockExclusive(&ui_lock);
    for(unsigned i=0;i<kSubMax && result==-3;++i) {
        const LayoutSub &s=layout_subs[i];
        if(s.cb==cb && s.ctx==ctx && s.destroyed==destroyed && s.ktid==ktid && !strcmp(s.name,name?name:""))result=static_cast<int>(i+1);
    }
    for(unsigned i=0;i<kSubMax && result==-3;++i) {
        if(layout_subs[i].cb)continue;
        LayoutSub &s=layout_subs[i];
        memset(&s,0,sizeof(s));s.ctx=ctx;s.ktid=ktid;s.destroyed=destroyed;
        if(name)strcpy_s(s.name,name);
        MemoryBarrier();s.cb=cb;result=static_cast<int>(i+1);
        InterlockedIncrement(&subscriber_total);
    }
    ReleaseSRWLockExclusive(&ui_lock);
    if(result>0 && InterlockedCompareExchange(&observation_registered,0,0)==0)register_observation();
    return result;
}
static int api_on_layout_built(uint32_t ktid,const char *name,void (*cb)(void *,void *),void *ctx) {
    return add_sub(false,ktid,name,reinterpret_cast<void *>(cb),ctx);
}
static int api_on_layout_destroyed(uint32_t ktid,void (*cb)(void *,void *),void *ctx) {
    return add_sub(true,ktid,nullptr,reinterpret_cast<void *>(cb),ctx);
}
static int api_layout_unsubscribe(int handle) {
    if(handle<=0 || static_cast<unsigned>(handle)>kSubMax)return -1;
    int result=-1;
    AcquireSRWLockExclusive(&ui_lock);
    LayoutSub &s=layout_subs[handle-1];
    if(s.cb) {s.cb=nullptr;InterlockedDecrement(&subscriber_total);result=0;}
    ReleaseSRWLockExclusive(&ui_lock);
    return result;
}
static bool call_callback(void *cb,Obj layout,void *ctx) {
    return guarded([&] {reinterpret_cast<void (*)(void *,void *)>(cb)(layout,ctx);});
}
static void dispatch(bool destroyed,const Live &info) {
    LayoutSub todo[kSubMax];unsigned n=0;
    AcquireSRWLockShared(&ui_lock);
    for(unsigned i=0;i<kSubMax;++i) {
        const LayoutSub &s=layout_subs[i];
        if(!s.cb || s.destroyed!=destroyed)continue;
        if(s.ktid && s.ktid!=info.ktid)continue;
        if(s.name[0] && strcmp(s.name,info.name))continue;
        todo[n++]=s;
    }
    ReleaseSRWLockShared(&ui_lock);
    for(unsigned i=0;i<n;++i)
        if(!call_callback(todo[i].cb,info.obj,todo[i].ctx))emit("layout_event_callback_fault",destroyed?1u:0u,info.ktid,info.name_hash);
}
// ---- 86C570 name notes ----
static void api_note_layout_load(void *layout,const char *name,int async) {
    if(!layout || !name)return;
    if(async)InterlockedIncrement(&async_loads);
    AcquireSRWLockExclusive(&ui_lock);
    unsigned slot=pending_next%kPendingMax;
    for(unsigned i=0;i<kPendingMax;++i)if(pending[i].obj==layout)slot=i;
    if(slot==pending_next%kPendingMax)++pending_next;
    Pending &p=pending[slot];
    if(!p.obj)InterlockedIncrement(&pending_count);
    p.obj=static_cast<Obj>(layout);p.async=async;p.hash=name_hash31(name);
    strncpy_s(p.name,sizeof(p.name),name,_TRUNCATE);
    ReleaseSRWLockExclusive(&ui_lock);
}
// ---- diagnostics (diagnostic builds only; fixtures switch them on explicitly) ----
#ifndef NO_RUNTIME_LOG
struct Screen {unsigned vtable;const char *tag;unsigned id;};
static const Screen kScreens[]={{GA(GA_1473DC8),"speed",1},{GA(GA_146AFE8),"lord_footer",2},{GA(GA_141BE88),"expert",3},{GA(GA_13E4C78),"ccz_title",4}};
struct NameDump {unsigned hash;unsigned d[16];bool used;};
static NameDump name_dumps[48];
static unsigned res_seq,screen_seen[5],ktid_match_logs,ktid_stat_logs,same_total,diff_total,same_eq[16],diff_eq[16];
static bool life_done;
static const struct {const char *name;unsigned ktid;} kKnownKtid[]={{"cmn_footer_re_cn",0xD8913AB2u}};
static void probe_resource(Obj layout,const Live &info) {
    Obj handle=rdptr(layout,0x90);
    if(!handle)return;
    unsigned head[16]={},body[16]={};
    copy_safe(head,handle,sizeof(head));
    Obj inner=rdptr(handle,0x20);
    if(inner)copy_safe(body,inner,sizeof(body));
    unsigned seq=++res_seq;
    if(seq<=12) {
        emit("layout_p4_res_head",seq,info.name_hash,static_cast<unsigned>(reinterpret_cast<uintptr_t>(handle)));
        for(unsigned g=0;g<8;++g)emit("layout_p4_res_raw",(seq<<8)|g,head[g*2],head[g*2+1]);
        if(inner)for(unsigned g=0;g<8;++g)emit("layout_p4_res_inner",(seq<<8)|g,body[g*2],body[g*2+1]);
    }
    if(!info.name[0])return;
    for(unsigned i=0;i<16 && ktid_match_logs<64;++i) {   // against the name hash and the known name->KTID pairs (64 lines at most)
        if(info.name_hash && head[i]==info.name_hash) {++ktid_match_logs;emit("layout_p4_ktid_match",i*4,1,seq);}
        if(info.name_hash && body[i]==info.name_hash) {++ktid_match_logs;emit("layout_p4_ktid_match",i*4,4,seq);}
        for(const auto &k:kKnownKtid)if(!strcmp(k.name,info.name)) {
            if(head[i]==k.ktid) {++ktid_match_logs;emit("layout_p4_ktid_match",i*4,2,seq);}
            if(body[i]==k.ktid) {++ktid_match_logs;emit("layout_p4_ktid_match",i*4,3,seq);}
        }
    }
    // equal for the same name, different across names => KTID offset candidates (bounded: 40 stat lines)
    NameDump *same=nullptr,*other=nullptr;
    for(auto &d:name_dumps) {if(d.used && d.hash==info.name_hash)same=&d;else if(d.used)other=&d;}
    if(same) {
        ++same_total;for(unsigned i=0;i<16;++i)if(same->d[i]==head[i])++same_eq[i];
        if(other) {++diff_total;for(unsigned i=0;i<16;++i)if(other->d[i]==head[i])++diff_eq[i];}
        if(ktid_stat_logs<40) {
            ++ktid_stat_logs;
            unsigned same_mask=0,diff_zero=0;
            for(unsigned i=0;i<16;++i) {if(same_eq[i]==same_total)same_mask|=1u<<i;if(diff_eq[i]==0)diff_zero|=1u<<i;}
            emit("layout_p4_ktid_cand",same_mask&(diff_total?diff_zero:0u),same_total,diff_total);   // bit i = dword offset i*4
        }
        memcpy(same->d,head,sizeof(head));
    } else for(auto &d:name_dumps)if(!d.used) {d.used=true;d.hash=info.name_hash;memcpy(d.d,head,sizeof(head));break;}
}
static void probe_track_screen(Obj layout,unsigned id);   // timing sampler, defined with the other local3 diagnostics below
static void probe_screen(Obj layout,unsigned thread) {
    Obj vt=rdptr(layout,0);
    for(const Screen &s:kScreens) {
        if(vt!=image+s.vtable)continue;
        if(screen_seen[s.id]>=2)return;
        ++screen_seen[s.id];
        probe_track_screen(layout,s.id);
        Obj table=layout+0x130;
        unsigned count=rd32(table,0x10);
        emit("layout_p4_screen",s.id,count,thread);
        emit("layout_p4_layout_xy",s.id,rd32(layout,0x124),rd32(layout,0x128));
        for(unsigned node=0;node<count && node<64;++node) {
            Obj control=nullptr;
            if(!guarded([&] {control=gf<Obj(__fastcall *)(Obj,int)>(GA(GA_92CEE0))(table,static_cast<int>(node));}) || !control)continue;
            unsigned key=(s.id<<16)|node;
            emit("layout_p4_node",key,static_cast<unsigned>(reinterpret_cast<uintptr_t>(control)),static_cast<unsigned>(reinterpret_cast<uintptr_t>(control)>>32));
            emit("layout_p4_node_xy",key,rd32(control,0x20),rd32(control,0x24));   // ints: setPosition 874600 stores +0x20/+0x24
            emit("layout_p4_node_wh",key,rd32(control,0x28),rd32(control,0x2C));
        }
        return;
    }
}
static void probe_lifetime();
// 2026-11-05 local3 (triage 10-4): destroys of layouts the live table does not know. count + the vtable RVA of the object (the first
// 24 and every 512th); the vtable shows which layout classes escape the build observation.
static volatile LONG destroy_unmatched;
static void note_destroy_unmatched(Obj layout) {
    LONG n=InterlockedIncrement(&destroy_unmatched);
    if(n>24 && n%512)return;
    Obj vt=rdptr(layout,0);
    unsigned rva=vt>=image && vt<image+image_size?static_cast<unsigned>(vt-image):0u;
    emit("layout_p4_destroy_unmatched",static_cast<unsigned>(n),rva,live_count);
}
// 2026-11-05 local3 (UI_RESOURCE_LIFECYCLE 8, runtime part): the extra RDB nodes of a feature that is not installed can only be
// hidden once their controls exist; at the build observation (86E338) none exist yet. This sampler (diagnostic builds) finds WHEN:
// per frame (Present callback thread, memory reads only, no game call) it counts the nodes of every tracked screen whose scene
// element is still unbound (table[1][node] != 0) and logs layout_p4_node_timing(screen, frames since build, unbound<<16|count)
// whenever the count changes (at most 12 lines per layout object).
struct ScreenTrack {Obj obj;unsigned id,built_frame,last_unbound,logged;};
static ScreenTrack screen_track[8];
static volatile LONG screen_frames,timing_subscribed;
static unsigned count_unbound(Obj layout,unsigned *count) {
    Obj table=layout+0x130;
    unsigned n=rd32(table,0x10);
    Obj nodes=rdptr(table,8);
    *count=n;
    if(!nodes || n>256)return 0xFFFFu;
    unsigned unbound=0;
    for(unsigned node=0;node<n;++node)if(rdptr(nodes,node*8))++unbound;
    return unbound;
}
static void __cdecl timing_overlay(void *,void *) {
    LONG frame=InterlockedIncrement(&screen_frames);
    ScreenTrack todo[8];unsigned n=0;
    AcquireSRWLockShared(&ui_lock);
    for(const ScreenTrack &t:screen_track)if(t.obj && t.logged<12)todo[n++]=t;
    ReleaseSRWLockShared(&ui_lock);
    for(unsigned i=0;i<n;++i) {
        unsigned count=0,unbound=count_unbound(todo[i].obj,&count);
        if(unbound==todo[i].last_unbound)continue;
        emit("layout_p4_node_timing",todo[i].id,static_cast<unsigned>(frame)-todo[i].built_frame,(unbound<<16)|count);
        AcquireSRWLockExclusive(&ui_lock);
        for(ScreenTrack &t:screen_track)if(t.obj==todo[i].obj) {t.last_unbound=unbound;++t.logged;}
        ReleaseSRWLockExclusive(&ui_lock);
    }
}
static void probe_track_screen(Obj layout,unsigned id) {
    unsigned count=0,unbound=count_unbound(layout,&count);
    AcquireSRWLockExclusive(&ui_lock);
    ScreenTrack *slot=nullptr;
    for(ScreenTrack &t:screen_track)if(t.obj==layout) {slot=&t;break;}
    if(!slot)for(ScreenTrack &t:screen_track)if(!t.obj) {slot=&t;break;}
    if(slot)*slot={layout,id,static_cast<unsigned>(screen_frames),unbound,0};
    ReleaseSRWLockExclusive(&ui_lock);
    emit("layout_p4_node_timing",id,0,(unbound<<16)|count);   // state at the build observation
    if(slot && InterlockedCompareExchange(&timing_subscribed,1,0)==0)api_overlay_subscribe(&timing_overlay,nullptr);
}
static void probe_forget_screen(Obj layout) {
    AcquireSRWLockExclusive(&ui_lock);
    for(ScreenTrack &t:screen_track)if(t.obj==layout)t={};
    ReleaseSRWLockExclusive(&ui_lock);
}
#endif
// ---- stats ----
static volatile LONG64 built_calls,built_orig_ns,built_hook_ns,built_orig_max,built_hook_max,destroy_calls,destroy_ns,destroy_max;
static LARGE_INTEGER qpc_frequency;
static unsigned long long to_ns(LONGLONG ticks) {
    if(!qpc_frequency.QuadPart)QueryPerformanceFrequency(&qpc_frequency);
    return static_cast<unsigned long long>(ticks)*1000000000ull/static_cast<unsigned long long>(qpc_frequency.QuadPart);
}
static void raise_max(volatile LONG64 *slot,LONG64 value) {
    for(LONG64 seen=*slot;value>seen;seen=*slot)if(InterlockedCompareExchange64(slot,value,seen)==seen)break;
}
// ---- built / destroyed ----
static bool track_live(const Live &info) {
    AcquireSRWLockExclusive(&ui_lock);
    bool stored=live_insert_locked(info);
    ReleaseSRWLockExclusive(&ui_lock);
    if(!stored && InterlockedIncrement(&table_full)==1)emit("layout_events_table_full",kLiveMax,0,0);
    return stored;
}
static void on_built(Obj layout) {
    if(!layout)return;
    unsigned thread=GetCurrentThreadId();
    if(InterlockedCompareExchange(&first_thread,static_cast<LONG>(thread),0)!=0 && first_thread!=static_cast<LONG>(thread))
        InterlockedIncrement(&other_thread_calls);
    if(dispatch_depth>0 || is_ours(layout)) {   // reentrancy: events raised by our own callbacks/created panes are dropped
        InterlockedIncrement(&nested_dropped);return;
    }
    Live info={};
    info.obj=layout;
    int async=0;
    AcquireSRWLockExclusive(&ui_lock);
    for(unsigned i=0;i<kPendingMax;++i)if(pending[i].obj==layout) {
        info.name_hash=pending[i].hash;strcpy_s(info.name,pending[i].name);async=pending[i].async;pending[i].obj=nullptr;InterlockedDecrement(&pending_count);break;
    }
    if(!info.name[0]) {
        int at=live_find_locked(layout);
        if(at>=0) {info.name_hash=live[at].name_hash;strcpy_s(info.name,live[at].name);}
    }
    ReleaseSRWLockExclusive(&ui_lock);
    info.ktid=read_ktid(layout);
    if(async)InterlockedIncrement(&async_built);else InterlockedIncrement(&sync_built);
    if(destroy_observed())track_live(info);
    DepthGuard guard;
#ifndef NO_RUNTIME_LOG
    if(probe_enabled()) {
        probe_resource(layout,info);
        probe_screen(layout,thread);
        if(!life_done && rdptr(layout,0)==image+GA(GA_146AFE8)) {life_done=true;probe_lifetime();}
    }
#endif
    dispatch(false,info);
}
static void purge_created_for(const void *owner) {   // the owner's table frees the controls made inside it
    if(!created_count)return;
    AcquireSRWLockExclusive(&ui_lock);
    for(unsigned i=created_count;i>0;--i)
        if(created[i-1].owner==owner && created[i-1].kind!=CK_PANE) {created[i-1]=created[created_count-1];--created_count;}
    ReleaseSRWLockExclusive(&ui_lock);
}
static void on_destroyed(Obj layout) {
    if(!layout)return;
    if(created_count)purge_created_for(layout);
    if(InterlockedCompareExchange(&pending_count,0,0)>0) {   // a load note for an object that is dying unbuilt (failed async load) must not name a later object
        AcquireSRWLockExclusive(&ui_lock);
        for(unsigned i=0;i<kPendingMax;++i)if(pending[i].obj==layout) {pending[i].obj=nullptr;InterlockedDecrement(&pending_count);}
        ReleaseSRWLockExclusive(&ui_lock);
    }
    Live info={};bool found=false;
    if(live_count) {
        AcquireSRWLockExclusive(&ui_lock);
        found=live_erase_locked(layout,&info);
        ReleaseSRWLockExclusive(&ui_lock);
    }
#ifndef NO_RUNTIME_LOG
    if(probe_enabled()) {
        probe_forget_screen(layout);
        if(!found)note_destroy_unmatched(layout);
    }
#endif
    if(!found)return;
    InterlockedIncrement(&destroyed_events);
    DepthGuard guard;
    dispatch(true,info);
}
static void __fastcall built_hook(Obj table,Obj layout) {
    COST_SCOPE("layout_built_hook");
#ifdef NO_RUNTIME_LOG
    reinterpret_cast<void (__fastcall *)(Obj,Obj)>(next_built)(table,layout);   // original 92B0A0 (or the next mod in the chain)
    on_built(layout);
#else
    LARGE_INTEGER t0={},t1={},t2={};
    QueryPerformanceCounter(&t0);
    reinterpret_cast<void (__fastcall *)(Obj,Obj)>(next_built)(table,layout);
    QueryPerformanceCounter(&t1);
    on_built(layout);
    QueryPerformanceCounter(&t2);
    if(probe_enabled()) {   // 86E320 hook cost: original 92B0A0 vs our work (recording + probes + callbacks), per call
        LONG64 orig=static_cast<LONG64>(to_ns(t1.QuadPart-t0.QuadPart)),ours=static_cast<LONG64>(to_ns(t2.QuadPart-t1.QuadPart));
        LONG64 n=InterlockedIncrement64(&built_calls);
        InterlockedAdd64(&built_orig_ns,orig);InterlockedAdd64(&built_hook_ns,ours);
        raise_max(&built_orig_max,orig);raise_max(&built_hook_max,ours);
        if(n%64==0) {
            emit("layout_p4_built_cost",static_cast<unsigned>(n),static_cast<unsigned>(built_hook_ns/n),static_cast<unsigned>(built_hook_max));
            emit("layout_p4_orig_cost",static_cast<unsigned>(n),static_cast<unsigned>(built_orig_ns/n),static_cast<unsigned>(built_orig_max));
            emit("layout_p4_counts",static_cast<unsigned>(sync_built),static_cast<unsigned>(async_loads),static_cast<unsigned>(async_built));
            emit("layout_p4_state",static_cast<unsigned>(destroyed_events),static_cast<unsigned>(nested_dropped),
                static_cast<unsigned>(live_count)|(static_cast<unsigned>(other_thread_calls)<<16));
        }
    }
#endif
}
static void __fastcall destroy_hook(Obj self,unsigned long long a,unsigned long long b,unsigned long long c) {
    COST_SCOPE("layout_destroy_hook");
#ifndef NO_RUNTIME_LOG
    LARGE_INTEGER t0={},t1={};
    if(probe_enabled())QueryPerformanceCounter(&t0);
#endif
    on_destroyed(self);
#ifndef NO_RUNTIME_LOG
    if(probe_enabled()) {
        QueryPerformanceCounter(&t1);
        LONG64 ours=static_cast<LONG64>(to_ns(t1.QuadPart-t0.QuadPart));
        LONG64 n=InterlockedIncrement64(&destroy_calls);
        InterlockedAdd64(&destroy_ns,ours);raise_max(&destroy_max,ours);
        if(n%1024==0)emit("layout_p4_destroy_cost",static_cast<unsigned>(n),static_cast<unsigned>(destroy_ns/n),static_cast<unsigned>(destroy_max));
    }
#endif
    reinterpret_cast<void (__fastcall *)(Obj,unsigned long long,unsigned long long,unsigned long long)>(destroy_trampoline)(self,a,b,c);
}
// ---- runtime UI services (only inside an event callback = game UI thread) ----
static bool in_event() {return dispatch_depth>0;}
static void track(void *object,unsigned kind,void *owner) {
    AcquireSRWLockExclusive(&ui_lock);
    if(created_count<kCreatedMax)created[created_count++]={object,kind,owner};
    ReleaseSRWLockExclusive(&ui_lock);
}
static unsigned kind_of(const void *object,void **owner=nullptr) {
    AcquireSRWLockShared(&ui_lock);
    int i=find_created_locked(object);
    unsigned kind=i>=0?created[i].kind:0;
    if(i>=0 && owner)*owner=created[i].owner;
    ReleaseSRWLockShared(&ui_lock);
    return kind;
}
static void raw_destroy(Obj object) {   // scalar deleting destructor, as native code does for layouts it owns
    guarded([&] {reinterpret_cast<void (__fastcall *)(Obj,int)>(vslot(object,0))(object,1);});
}
static void *svc_load_sublayout(void *parent,const char *name,float x,float y) {
    if(!in_event()) {reject("load_sublayout",1);return nullptr;}
    if(!name || !*name || !image)return nullptr;
    Obj pane=nullptr;bool ok=false;
    unsigned long long loaded=0;   // 86E0C0 return value (diagnostic: layout_ui_load_detail)
    bool clean=guarded([&] {
        pane=gf<Obj(__fastcall *)(size_t)>(GA(GA_3F6BC0))(0x178);
        if(!pane)return;
        memset(pane,0,0x178);
        gf<Obj(__fastcall *)(Obj)>(GA(GA_864C50))(pane);
        *reinterpret_cast<Obj *>(pane)=image+GA(GA_1342530);
        gf<Obj(__fastcall *)(Obj)>(GA(GA_92AB70))(pane+0x130);
        *reinterpret_cast<Obj *>(pane)=image+GA(GA_13E4E80);
        loaded=gf<unsigned long long(__fastcall *)(Obj,int,int,Obj,const char *,int,unsigned long long)>(GA(GA_86E0C0))(
            pane,to_int(x),to_int(y),static_cast<Obj>(parent),name,0,0);
        ok=rd32(pane,0x12C)==1 && rdptr(pane,0x98)!=nullptr;
    });
    if(!clean) {emit("layout_ui_fault",1,hash32(name),0);return nullptr;}   // partially built object is leaked on purpose
    if(!pane)return nullptr;
    if(!ok) {
        // triage 10-3: why the load check failed (return value of 86E0C0, dword at +0x12C, bit0 = +0x98 set, bit1 = +0x90 set).
        emit("layout_ui_load_detail",static_cast<unsigned>(loaded),rd32(pane,0x12C),(rdptr(pane,0x98)?1u:0u)|(rdptr(pane,0x90)?2u:0u));
        raw_destroy(pane);emit("layout_ui_load_failed",hash32(name),0,0);return nullptr;
    }
    track(pane,CK_PANE,parent);
    return pane;
}
static int svc_attach(void *parent,int node,void *child) {
    if(!in_event()) {reject("attach",1);return -1;}
    if(!parent || !child || node<0 || !game_object(parent) || kind_of(child)!=CK_PANE)return -1;
    Obj table=static_cast<Obj>(parent)+0x130;
    if(node>=static_cast<int>(rd32(table,0x10)))return -1;
    bool ok=false;
    if(!guarded([&] {
        gf<void(__fastcall *)(Obj,int,Obj,int,int,int)>(GA(GA_92E0B0))(table,node,static_cast<Obj>(child),8,1,0);
        ok=gf<Obj(__fastcall *)(Obj,int)>(GA(GA_92CEE0))(table,node)==child;
    }))return -1;
    if(ok) {
        AcquireSRWLockExclusive(&ui_lock);
        int i=find_created_locked(child);
        if(i>=0)created[i].owner=parent;
        ReleaseSRWLockExclusive(&ui_lock);
    }
    return ok?0:-1;
}
static void *svc_clone_control(void *layout,int node,int style,int kind) {
    if(!in_event()) {reject("clone_control",1);return nullptr;}
    if(!layout || node<0 || !game_object(layout))return nullptr;
    Obj table=static_cast<Obj>(layout)+0x130;
    Obj nodes=rdptr(table,8);
    // 92E430 only builds a control from a node whose scene element is still unbound (it clears table[1][node] afterwards).
    if(node>=static_cast<int>(rd32(table,0x10)) || !nodes || !rdptr(nodes,static_cast<unsigned>(node)*8)) {
        emit("layout_ui_clone_unavailable",static_cast<unsigned>(node),nodes?1u:0u,0);return nullptr;
    }
    Obj control=nullptr;
    if(!guarded([&] {control=gf<Obj(__fastcall *)(Obj,int,int,int,int,int)>(GA(GA_92E430))(table,node,style,kind,1,0);})) {
        emit("layout_ui_fault",2,static_cast<unsigned>(node),0);return nullptr;
    }
    if(control)track(control,CK_BUTTON,layout);
    return control;
}
static void *svc_create_label(void *parent,int node) {
    if(!in_event()) {reject("create_label",1);return nullptr;}
    if(!parent || node<0 || !game_object(parent))return nullptr;
    Obj table=static_cast<Obj>(parent)+0x130;
    Obj nodes=rdptr(table,8);
    Obj node_ptr=node<static_cast<int>(rd32(table,0x10)) && nodes?rdptr(nodes,static_cast<unsigned>(node)*8):nullptr;
    if(!node_ptr) {emit("layout_ui_label_unavailable",static_cast<unsigned>(node),0,0);return nullptr;}
    Obj label=nullptr;bool ok=false;
    bool clean=guarded([&] {
        // same sequence as the native 1A0A40 label loop: 3F6BC0(0x188), 84B150, geometry from the node, vt+0x138, 92E0B0 (kind 0xC)
        label=gf<Obj(__fastcall *)(size_t)>(GA(GA_3F6BC0))(0x188);
        if(!label)return;
        label=gf<Obj(__fastcall *)(Obj)>(GA(GA_84B150))(label);
        if(!label)return;
        unsigned long long position=gf<unsigned long long(__fastcall *)(Obj)>(GA(GA_92CD00))(node_ptr);
        unsigned long long size=gf<unsigned long long(__fastcall *)(Obj)>(GA(GA_92D040))(node_ptr);
        reinterpret_cast<void(__fastcall *)(Obj,int,int,int,int,Obj,int)>(vslot(label,0x138))(
            label,static_cast<int>(position&0xFFFFFFFFu),static_cast<int>(size>>32),static_cast<int>(size&0xFFFFFFFFu),
            static_cast<int>(size>>32),static_cast<Obj>(parent),0);
        gf<void(__fastcall *)(Obj,int,Obj,int,int,int)>(GA(GA_92E0B0))(table,node,label,0xC,1,0);
        ok=gf<Obj(__fastcall *)(Obj,int)>(GA(GA_92CEE0))(table,node)==label;
    });
    if(!clean) {emit("layout_ui_fault",3,static_cast<unsigned>(node),0);return nullptr;}
    if(!label)return nullptr;
    if(!ok) {raw_destroy(label);return nullptr;}   // never attached: no table owns it
    track(label,CK_LABEL,parent);
    return label;
}
static int svc_set_text(void *control,const wchar_t *text) {
    if(!in_event()) {reject("set_text",1);return -1;}
    if(!control || !text)return -1;
    unsigned kind=kind_of(control);
    if(kind!=CK_BUTTON && kind!=CK_LABEL)return -1;   // only controls we made
    bool ok=guarded([&] {
        if(kind==CK_BUTTON)reinterpret_cast<void(__fastcall *)(Obj,const wchar_t *)>(vslot(static_cast<Obj>(control),0x1D8))(static_cast<Obj>(control),text);
        else gf<void(__fastcall *)(Obj,const wchar_t *)>(GA(GA_85C9E0))(static_cast<Obj>(control),text);
    });
    return ok?0:-1;
}
static int svc_set_visible(void *control,int on) {
    if(!in_event()) {reject("set_visible",1);return -1;}
    if(!control || !game_object(control))return -1;
    return guarded([&] {gf<void(__fastcall *)(Obj,int)>(GA(GA_8741D0))(static_cast<Obj>(control),on?1:0);})?0:-1;
}
static int svc_set_position(void *control,int x,int y) {
    if(!in_event()) {reject("set_position",1);return -1;}
    if(!control || !game_object(control))return -1;
    return guarded([&] {reinterpret_cast<void(__fastcall *)(Obj,int,int)>(vslot(static_cast<Obj>(control),0x78))(static_cast<Obj>(control),x,y);})?0:-1;
}
static int svc_get_position(void *control,int *x,int *y) {
    if(!control || !x || !y || !game_object(control))return -1;
    *x=static_cast<int>(rd32(control,0x20));*y=static_cast<int>(rd32(control,0x24));return 0;
}
// std::function-style callable as the game lays it out (vtable: copy, move, call, type, delete, get; storage 0x38 bytes)
struct ClickCallable {void **vtable;void (*callback)(void *,void *);void *ctx;};
static void *__fastcall click_copy(ClickCallable *self,ClickCallable *where) {COST_SCOPE("layout_click_copy");if(where)*where=*self;return where;}
static void __fastcall click_call(ClickCallable *self,Obj *arg) {COST_SCOPE("layout_click_call");self->callback(arg?*arg:nullptr,self->ctx);}
static void *__fastcall click_type(ClickCallable *) {COST_SCOPE("layout_click_type");return nullptr;}
static void __fastcall click_delete(ClickCallable *self,bool heap) {COST_SCOPE("layout_click_delete");if(heap)gf<void(__fastcall *)(void *)>(GA(GA_3F6C50))(self);}
static void *__fastcall click_get(ClickCallable *self) {COST_SCOPE("layout_click_get");return &self->callback;}
static void *click_vtable[6]={reinterpret_cast<void *>(&click_copy),reinterpret_cast<void *>(&click_copy),reinterpret_cast<void *>(&click_call),
    reinterpret_cast<void *>(&click_type),reinterpret_cast<void *>(&click_delete),reinterpret_cast<void *>(&click_get)};
static int svc_bind_click(void *control,void (*cb)(void *,void *),void *ctx) {
    if(!in_event()) {reject("bind_click",1);return -1;}
    if(!control || !cb || kind_of(control)!=CK_BUTTON)return -1;
    alignas(16) unsigned char local[0x40]={};
    auto *callable=reinterpret_cast<ClickCallable *>(local);
    callable->vtable=click_vtable;callable->callback=cb;callable->ctx=ctx;
    *reinterpret_cast<void **>(local+0x38)=local;
    Obj button=static_cast<Obj>(control);
    return guarded([&] {
        *reinterpret_cast<unsigned *>(button+0x84)=1;   // 92E430 already stored the node id at +0x80
        gf<void(__fastcall *)(Obj,int,void *)>(GA(GA_1A3CB0))(button,0x7FFFFFFE,local);   // copies, then destroys local
    })?0:-1;
}
static int svc_destroy(void *object) {
    if(!object)return LAYOUT_DESTROY_REJECTED;
    void *owner=nullptr;
    unsigned kind=kind_of(object,&owner);
    if(!kind)return LAYOUT_DESTROY_REJECTED;                       // not created by us: never touch it
    if(!in_event()) {reject("destroy",1);return LAYOUT_DESTROY_REJECTED;}
    if(kind==CK_PANE) {
        // children (controls in its table) first, newest first; the pane's own destructor frees them, then the pane goes
        AcquireSRWLockExclusive(&ui_lock);
        for(unsigned i=created_count;i>0;--i)
            if(created[i-1].owner==object) {created[i-1]=created[created_count-1];--created_count;}
        int self=find_created_locked(object);
        if(self>=0) {created[self]=created[created_count-1];--created_count;}
        ReleaseSRWLockExclusive(&ui_lock);
        raw_destroy(static_cast<Obj>(object));
        return LAYOUT_DESTROY_DONE;
    }
    // controls registered in an owner's table are freed with that table: hide now, forget when the owner dies
    guarded([&] {gf<void(__fastcall *)(Obj,int)>(GA(GA_8741D0))(static_cast<Obj>(object),0);});
    return LAYOUT_DESTROY_DEFERRED;
}
static void *api_load_sublayout(void *parent,const char *name,float x,float y) {return svc_load_sublayout(parent,name,x,y);}
static int api_attach(void *parent,int node,void *child) {return svc_attach(parent,node,child);}
static void *api_clone_control(void *layout,int node,int style,int kind) {return svc_clone_control(layout,node,style,kind);}
static void *api_create_label(void *parent,int node) {return svc_create_label(parent,node);}
static int api_set_text(void *control,const wchar_t *text) {return svc_set_text(control,text);}
static int api_set_visible(void *control,int on) {return svc_set_visible(control,on);}
static int api_bind_click(void *control,void (*cb)(void *,void *),void *ctx) {return svc_bind_click(control,cb,ctx);}
static int api_destroy(void *object) {return svc_destroy(object);}
static int api_set_position(void *control,int x,int y) {return svc_set_position(control,x,y);}
static int api_get_position(void *control,int *x,int *y) {return svc_get_position(control,x,y);}
// ---- one-build lifetime test (diagnostic builds): load our own instance of the footer layout, clone node 14, move, label, destroy ----
#ifndef NO_RUNTIME_LOG
static void probe_click(void *,void *) {}
static void probe_lifetime() {
    // Never touches the live footer: the footer's own nodes are bound by native 85FCE0 later. Everything lives in a private pane
    // that is destroyed before this function returns; each stage logs layout_p4_life(stage,status,detail).
    void *pane=svc_load_sublayout(nullptr,"cmn_footer_re_cn",0.0f,0.0f);
    emit("layout_p4_life",1,pane?1u:0u,pane?rd32(static_cast<Obj>(pane)+0x130,0x10):0u);   // 1 load_sublayout: ok, node count
    if(!pane)return;
    emit("layout_p4_life_xy",1,rd32(pane,0x124),rd32(pane,0x128));
    void *button=svc_clone_control(pane,14,2,0);
    emit("layout_p4_life",2,button?1u:0u,0);                                                // 2 clone_control(node 14, hanyou, kind 0)
    if(button) {
        int x=0,y=0,x2=0,y2=0;
        svc_get_position(button,&x,&y);
        emit("layout_p4_life_xy",2,static_cast<unsigned>(x),static_cast<unsigned>(y));       // position before
        emit("layout_p4_life_wh",2,rd32(button,0x28),rd32(button,0x2C));
        int moved=svc_set_position(button,x+29,y);                                           // 3 set_position (vt+0x78)
        svc_get_position(button,&x2,&y2);
        emit("layout_p4_life",3,moved==0?1u:0u,static_cast<unsigned>(x2-x));
        emit("layout_p4_life_xy",3,static_cast<unsigned>(x2),static_cast<unsigned>(y2));
        emit("layout_p4_life",4,svc_set_text(button,L"P4")==0?1u:0u,0);                      // 4 set_text
        emit("layout_p4_life",5,svc_bind_click(button,&probe_click,nullptr)==0?1u:0u,0);     // 5 bind_click
        emit("layout_p4_life",6,svc_set_visible(button,0)==0?1u:0u,0);                       // 6 set_visible
    }
    void *label=svc_create_label(pane,17);
    emit("layout_p4_life",7,label?1u:0u,label?(rdptr(label,0x178)?1u:0u):2u);               // 7 create_label (node 0x11), text node found
    if(label)emit("layout_p4_life",8,svc_set_text(label,L"P4")==0?1u:0u,0);                  // 8 label text
    int result=svc_destroy(pane);
    emit("layout_p4_life",9,static_cast<unsigned>(result),created_count);                    // 9 destroy(pane): result, tracked entries left
}
#endif
}
