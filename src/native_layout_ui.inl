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
// Original ABI used here ((근거: 1a4a447d 이전 기록, docs/HISTORY.md), ghidra_full C + callers, see docs/LAYOUT_P4_DIAG.md for the evidence table):
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
struct Created {void *obj;unsigned kind;void *owner;char feature[24];bool radio;};   // feature = owner feature of a UI-ownership scope (ui_begin), empty for event-callback objects
// 2026-11-05 local3 (docs/HISTORY.md 9-10): the live table was a 512-entry linear array that filled during a normal session
// (12,736 builds, 1,395 matched destroys). It is now an open-addressing hash table (linear probing, backward-shift erase) keyed by
// the layout address; a power of two, bounded probing, never grows. Overflow is still counted and logged once (table_full).
static const unsigned kLiveMax=8192,kPendingMax=64,kSubMax=32,kCreatedMax=256;
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
static thread_local int scope_depth;          // ui_begin/ui_end: a published feature owns this thread's UI calls (tactic_priority UI ownership)
static thread_local char scope_owner[24];
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
static const uint32_t kBuiltSite=GA(GA_8706E8),kBuiltOriginal=GA(GA_92D550),kDestroyEntry=GA(GA_8678B0);
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
static const Screen kScreens[]={{GA(GA_1476E58),"speed",1},{GA(GA_146E078),"lord_footer",2},{GA(GA_141EF18),"expert",3},{GA(GA_13E7D08),"ccz_title",4}};
struct NameDump {unsigned hash;unsigned d[16];bool used;};
static NameDump name_dumps[48];
static unsigned res_seq,screen_seen[5],ktid_match_logs,ktid_stat_logs,same_total,diff_total,same_eq[16],diff_eq[16];
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
            if(!guarded([&] {control=gf<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(table,static_cast<int>(node));}) || !control)continue;
            unsigned key=(s.id<<16)|node;
            emit("layout_p4_node",key,static_cast<unsigned>(reinterpret_cast<uintptr_t>(control)),static_cast<unsigned>(reinterpret_cast<uintptr_t>(control)>>32));
            emit("layout_p4_node_xy",key,rd32(control,0x20),rd32(control,0x24));   // ints: setPosition 874600 stores +0x20/+0x24
            emit("layout_p4_node_wh",key,rd32(control,0x28),rd32(control,0x2C));
        }
        return;
    }
}
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
[[maybe_unused]] static unsigned long long to_ns(LONGLONG ticks) {
    if(!qpc_frequency.QuadPart)QueryPerformanceFrequency(&qpc_frequency);
    return static_cast<unsigned long long>(ticks)*1000000000ull/static_cast<unsigned long long>(qpc_frequency.QuadPart);
}
[[maybe_unused]] static void raise_max(volatile LONG64 *slot,LONG64 value) {
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
    present_retry_tick();   // review F-06: bounded re-arm of a transient Present probe failure
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
static bool in_event() {return dispatch_depth>0 || scope_depth>0;}
static void track(void *object,unsigned kind,void *owner) {
    AcquireSRWLockExclusive(&ui_lock);
    if(created_count<kCreatedMax) {
        Created &c=created[created_count++];
        c.obj=object;c.kind=kind;c.owner=owner;c.radio=false;
        strcpy_s(c.feature,scope_depth>0?scope_owner:"");
    }
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
        pane=gf<Obj(__fastcall *)(size_t)>(GA(GA_3F90E0))(0x178);
        if(!pane)return;
        memset(pane,0,0x178);
        gf<Obj(__fastcall *)(Obj)>(GA(GA_866FA0))(pane);
        *reinterpret_cast<Obj *>(pane)=image+GA(GA_1345530);
        gf<Obj(__fastcall *)(Obj)>(GA(GA_92D020))(pane+0x130);
        *reinterpret_cast<Obj *>(pane)=image+GA(GA_13E7F10);
        loaded=gf<unsigned long long(__fastcall *)(Obj,int,int,Obj,const char *,int,unsigned long long)>(GA(GA_870470))(
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
        gf<void(__fastcall *)(Obj,int,Obj,int,int,int)>(GA(GA_930640))(table,node,static_cast<Obj>(child),8,1,0);
        ok=gf<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(table,node)==child;
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
    if(!guarded([&] {control=gf<Obj(__fastcall *)(Obj,int,int,int,int,int)>(GA(GA_9309C0))(table,node,style,kind,1,0);})) {
        emit("layout_ui_fault",2,static_cast<unsigned>(node),0);return nullptr;
    }
    if(control) {
        track(control,CK_BUTTON,layout);
        if(kind==1) {AcquireSRWLockExclusive(&ui_lock);int i=find_created_locked(control);if(i>=0)created[i].radio=true;ReleaseSRWLockExclusive(&ui_lock);}
    }
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
        label=gf<Obj(__fastcall *)(size_t)>(GA(GA_3F90E0))(0x188);
        if(!label)return;
        label=gf<Obj(__fastcall *)(Obj)>(GA(GA_84D4B0))(label);
        if(!label)return;
        unsigned long long position=gf<unsigned long long(__fastcall *)(Obj)>(GA(GA_92F1B0))(node_ptr);
        unsigned long long size=gf<unsigned long long(__fastcall *)(Obj)>(GA(GA_92F4F0))(node_ptr);
        reinterpret_cast<void(__fastcall *)(Obj,int,int,int,int,Obj,int)>(vslot(label,0x138))(
            label,static_cast<int>(position&0xFFFFFFFFu),static_cast<int>(size>>32),static_cast<int>(size&0xFFFFFFFFu),
            static_cast<int>(size>>32),static_cast<Obj>(parent),0);
        gf<void(__fastcall *)(Obj,int,Obj,int,int,int)>(GA(GA_930640))(table,node,label,0xC,1,0);
        ok=gf<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(table,node)==label;
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
        else gf<void(__fastcall *)(Obj,const wchar_t *)>(GA(GA_85ED10))(static_cast<Obj>(control),text);
    });
    return ok?0:-1;
}
static int svc_set_visible(void *control,int on) {
    if(!in_event()) {reject("set_visible",1);return -1;}
    if(!control || !game_object(control))return -1;
    return guarded([&] {gf<void(__fastcall *)(Obj,int)>(GA(GA_876580))(static_cast<Obj>(control),on?1:0);})?0:-1;
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
static void __fastcall click_delete(ClickCallable *self,bool heap) {COST_SCOPE("layout_click_delete");if(heap)gf<void(__fastcall *)(void *)>(GA(GA_3F9170))(self);}
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
        gf<void(__fastcall *)(Obj,int,void *)>(GA(GA_1A4FB0))(button,0x7FFFFFFE,local);   // copies, then destroys local
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
    guarded([&] {gf<void(__fastcall *)(Obj,int)>(GA(GA_876580))(static_cast<Obj>(object),0);});
    return LAYOUT_DESTROY_DEFERRED;
}
// ---- UI ownership for published features (user 2026-11-07: the Layout DLL owns every UI entry point of a mod feature) ----
// The feature module keeps behaviour only (what a click does, labels, data). It brackets its UI calls with ui_begin(owner)/ui_end():
// ui_begin succeeds only while the owner's registry hooks are installed/chained, so an unchecked, refused or unpublished feature
// never gets a UI object and the original screen stays intact. Everything created inside a scope is tagged with the owner;
// ui_release(owner,next) removes it with the original destroy sequence (vt+0x40, vt+0x28, scalar deleting destructor).
static int svc_ui_begin(const char *owner) {
    if(!owner || !*owner || !image || strlen(owner)>=sizeof(scope_owner))return 0;
    if(scope_depth>0) {if(strcmp(owner,scope_owner))return 0;++scope_depth;return 1;}   // nested scope of the same owner
    const int state=api_hook_owner_state(owner);
    if(state!=LAYOUT_STATE_INSTALLED && state!=LAYOUT_STATE_CHAINED) {reject("ui_begin",static_cast<unsigned>(state));return 0;}
    strcpy_s(scope_owner,owner);scope_depth=1;return 1;
}
static void svc_ui_end() {if(scope_depth>0 && --scope_depth==0)scope_owner[0]=0;}
static void *svc_ui_create_window(void *parent,int x,int y,int w,int h,const void *desc,int count) {
    if(!in_event() || scope_depth<=0 || !desc || count<=0 || count>=256 || !image)return nullptr;
    if(parent && !game_object(parent))return nullptr;
    Obj pane=nullptr;bool ok=false;
    bool clean=guarded([&] {
        // same assembly as the original sub-panes (6F3DC0): allocate 0x178, 866FA0, vt 1345530 -> table ctor -> final vt, then 870410.
        pane=gf<Obj(__fastcall *)(size_t)>(GA(GA_3F90E0))(0x178);
        if(!pane)return;
        memset(pane,0,0x178);
        gf<Obj(__fastcall *)(Obj)>(GA(GA_866FA0))(pane);
        *reinterpret_cast<Obj *>(pane)=image+GA(GA_1345530);
        gf<Obj(__fastcall *)(Obj)>(GA(GA_92D020))(pane+0x130);
        *reinterpret_cast<Obj *>(pane)=image+GA(GA_13E7F10);
        gf<unsigned long long(__fastcall *)(Obj,int,int,int,int,Obj,const void *,int)>(GA(GA_870410))(pane,x,y,w,h,static_cast<Obj>(parent),desc,count);
        ok=rd32(pane+0x130,0x10)==static_cast<unsigned>(count);
    });
    if(!clean) {emit("layout_ui_fault",4,static_cast<unsigned>(count),0);return nullptr;}   // partially built pane is leaked on purpose
    if(!pane)return nullptr;
    if(!ok) {emit("layout_ui_window_failed",static_cast<unsigned>(count),rd32(pane+0x130,0x10),0);return nullptr;}
    track(pane,CK_PANE,parent);
    return pane;
}
// vt+0x38 = 869210: registers the layout with the UI manager's layer lists (92D3B0/92D330 on its table, 869120). Every original
// construction (7DDD00 enter, 6F3DC0) calls it once the controls exist; without it the pane is neither drawn nor updated.
static int svc_ui_activate(void *pane) {
    if(!in_event() || scope_depth<=0 || kind_of(pane)!=CK_PANE)return -1;
    return guarded([&] {reinterpret_cast<void(__fastcall *)(Obj)>(vslot(static_cast<Obj>(pane),0x38))(static_cast<Obj>(pane));})?0:-1;
}
static void *svc_ui_node(void *layout,int node) {   // read-only lookup, allowed without a scope (idle-frame checks)
    if(!layout || node<0 || !game_object(layout))return nullptr;
    Obj table=static_cast<Obj>(layout)+0x130;
    if(node>=static_cast<int>(rd32(table,0x10)))return nullptr;
    Obj control=nullptr;
    if(!guarded([&] {control=gf<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(table,node);}))return nullptr;
    return control;
}
static int svc_ui_set_node_text(void *layout,int node,const wchar_t *text) {
    if(!in_event() || !layout || !text || node<0 || !game_object(layout))return -1;
    Obj table=static_cast<Obj>(layout)+0x130;
    if(node>=static_cast<int>(rd32(table,0x10)))return -1;
    return guarded([&] {gf<void(__fastcall *)(Obj,int,const wchar_t *,int)>(GA(GA_9332B0))(table,node,text,1);})?0:-1;
}
static int svc_ui_set_node_visible(void *layout,int node,int on) {
    if(!in_event() || !layout || node<0 || !game_object(layout))return -1;
    Obj table=static_cast<Obj>(layout)+0x130;
    if(node>=static_cast<int>(rd32(table,0x10)))return -1;
    return guarded([&] {gf<void(__fastcall *)(Obj,unsigned,int)>(GA(GA_931DA0))(table,static_cast<unsigned>(node),on?1:0);})?0:-1;
}
static int svc_ui_select(void *radio,int on) {
    if(!in_event() || !radio)return -1;
    bool is_radio=false;
    AcquireSRWLockShared(&ui_lock);
    int i=find_created_locked(radio);
    if(i>=0)is_radio=created[i].radio;
    ReleaseSRWLockShared(&ui_lock);
    if(!is_radio)return -1;   // 85AD20 writes +0x198, only radios (kind 1) have that field
    return guarded([&] {gf<void(__fastcall *)(Obj,int)>(GA(GA_85D050))(static_cast<Obj>(radio),on?1:0);})?0:-1;
}
// Visibility/alpha of `own` follow `source` (86F250 needs both low bits; native_ccz_title.inl sync).
static int svc_ui_sync_visibility(void *own,void *source) {
    if(!in_event() || !own || !source || !game_object(own) || !game_object(source))return -1;
    return guarded([&] {
        Obj o=static_cast<Obj>(own),s=static_cast<Obj>(source);
        const unsigned before=rd32(o,0x40),after=(before&~0xBu)|(rd32(s,0x40)&0xBu);
        if(before!=after) {
            *reinterpret_cast<unsigned *>(o+0x40)=after;
            gf<void(__fastcall *)(Obj,int)>(GA(GA_875720))(o,1);
        }
        if(rd32(o,0x30)!=rd32(s,0x30))gf<void(__fastcall *)(Obj,int)>(GA(GA_875800))(o,static_cast<int>(rd32(s,0x30)));
    })?0:-1;
}
// Disabled look of a control made by a scope (user 2026-11-07, tactic priority: the remember toggle is not usable without an officer).
// Original mechanism (0086.c 860830 = CCmnButton / CCmnRadioButton vtable slot 2, the per-frame update; 0085.c 85CE40/85CF70 set the same bit on
// composite widgets): a widget is enabled while input flag +0x40 bit 4 is set. The button update 860830 reads it every frame; with the bit
// clear it calls vt+0x1C0(button, 4 / 5 / 6) = the disabled states (disable / disable+focus / disable+selected), i.e. the original greyed
// look, and the mouse press handler 873260 (requires the bit) does nothing. So only that bit is changed: no own dimming, no own skin.
static int svc_ui_enable(void *control,int on) {
    if(!in_event() || scope_depth<=0 || !control || !game_object(control))return -1;
    Obj o=static_cast<Obj>(control);
    AcquireSRWLockExclusive(&ui_lock);
    const int i=find_created_locked(control);
    const bool ours=i>=0 && created[i].kind==CK_BUTTON;
    ReleaseSRWLockExclusive(&ui_lock);
    if(!ours)return -1;
    const bool done=guarded([&] {
        unsigned *flags=reinterpret_cast<unsigned *>(o+0x40);
        *flags=on?(*flags|4u):(*flags&~4u);
    });
    emit("layout_ui_enable",on?1u:0u,done?1u:0u,rd32(o,0x40));
    return done?0:-1;
}
// Mouse-over balloon for the owner (same CSimpleBalloonLayout assembly and show sequence as native_trait_hover.inl make_tip/show_impl:
// 3F6BC0(0x180), 864C50, 92AB70, vt 139C540, 89C320, vt+0xC8, 8A18C0(0xFF), vt+0x70(6); show = 8A2140 text, vt+0x38 once, 8A5D60 with the
// offset block, vt+0xC8(1)). One balloon per process, owned by the scope owner, destroyed by svc_ui_release.
struct TipState {Obj tip;bool registered,failed;wchar_t text[1024];char owner[24];};
static TipState tip_state;
static void tip_hide() {
    if(tip_state.tip)guarded([&] {reinterpret_cast<void(__fastcall *)(Obj,int)>(vslot(tip_state.tip,0xC8))(tip_state.tip,0);});
    tip_state.text[0]=0;
}
static bool tip_make() {
    if(tip_state.tip)return true;
    if(tip_state.failed)return false;
    Obj tip=nullptr;bool ok=false;
    const bool clean=guarded([&] {
        tip=gf<Obj(__fastcall *)(size_t)>(GA(GA_3F90E0))(0x180);
        if(!tip)return;
        gf<Obj(__fastcall *)(Obj)>(GA(GA_866FA0))(tip);
        gf<void(__fastcall *)(Obj)>(GA(GA_92D020))(tip+0x130);
        *reinterpret_cast<Obj *>(tip)=image+GA(GA_139F580);
        *reinterpret_cast<Obj *>(tip+0x178)=nullptr;
        tip_state.tip=tip;
        gf<void(__fastcall *)(Obj,int)>(GA(GA_89E550))(tip,0);
        reinterpret_cast<void(__fastcall *)(Obj,int)>(vslot(tip,0xC8))(tip,0);
        if(rdptr(tip,0x138)) {
            tip[0x40]|=8;
            gf<void(__fastcall *)(Obj,int)>(GA(GA_8A3AC0))(tip,0xFF);
            reinterpret_cast<void(__fastcall *)(Obj,int)>(vslot(tip,0x70))(tip,6);
            ok=true;
        }
    });
    emit("layout_ui_tip_built",clean&&ok?1u:0u,tip?1u:0u,0);
    if(!clean || !ok) {tip_state.failed=true;return false;}
    tip_state.registered=false;
    return true;
}
static int svc_ui_tooltip(const wchar_t *text) {
    if(!in_event() || scope_depth<=0 || !image)return -1;
    if(!text || !*text) {tip_hide();return 0;}
    if(!tip_make())return -1;
    strcpy_s(tip_state.owner,scope_owner);
    Obj tip=tip_state.tip;
    const bool changed=wcsncmp(tip_state.text,text,1023)!=0;
    const bool ok=guarded([&] {
        if(changed) {
            gf<void(__fastcall *)(Obj,const wchar_t *)>(GA(GA_8A4380))(tip,text);
            wcsncpy_s(tip_state.text,text,_TRUNCATE);
        }
        if(!tip_state.registered) {reinterpret_cast<void(__fastcall *)(Obj)>(vslot(tip,0x38))(tip);tip_state.registered=true;}
        alignas(16) unsigned char m[0x40]={};
        *reinterpret_cast<Obj *>(m+0x10)=tip;
        *reinterpret_cast<int *>(m+0x38)=20;*reinterpret_cast<int *>(m+0x3C)=20;
        gf<void(__fastcall *)(void *)>(GA(GA_8A7F60))(m);
        reinterpret_cast<void(__fastcall *)(Obj,int)>(vslot(tip,0xC8))(tip,1);
    });
    return ok?0:-1;
}
// ui_release: the balloon goes first, with the same destroy sequence as trait_hover release_impl (hide, vt+0x40, vt+0x28(obj,next), vt[0](obj,1)).
static void tip_release(const char *owner,void *next) {
    if(!tip_state.tip || strcmp(tip_state.owner,owner))return;
    Obj tip=tip_state.tip;
    guarded([&] {
        reinterpret_cast<void(__fastcall *)(Obj,int)>(vslot(tip,0xC8))(tip,0);
        if(tip_state.registered)reinterpret_cast<void(__fastcall *)(Obj)>(vslot(tip,0x40))(tip);
        reinterpret_cast<void(__fastcall *)(Obj,Obj)>(vslot(tip,0x28))(tip,static_cast<Obj>(next));
        reinterpret_cast<void(__fastcall *)(Obj,int)>(vslot(tip,0))(tip,1);
    });
    emit("layout_ui_tip_released",0,0,0);
    memset(&tip_state,0,sizeof(tip_state));
}
// Original teardown of a layout owned by its creator (7DC200 leave, 933BA0): vt+0x40 (unregister from the UI manager, 86BBD0),
// vt+0x28(obj,next) (table controls, own detach from the parent through parent vt+0xE0, 87A690) then vt[0](obj,1) (428DE0, frees 0x178).
// `next` is the second argument of the screen's leave. Returns the number of panes released.
static int svc_ui_release(const char *owner,void *next) {
    if(!owner || !in_event() || scope_depth<=0 || strcmp(owner,scope_owner))return -1;
    tip_release(owner,next);
    void *panes[32];unsigned n=0;
    AcquireSRWLockExclusive(&ui_lock);
    for(unsigned i=created_count;i>0 && n<32;--i)
        if(created[i-1].kind==CK_PANE && !strcmp(created[i-1].feature,owner))panes[n++]=created[i-1].obj;
    // forget everything the owner made inside scopes (panes, their controls, the footer button of the screen that is about to be destroyed)
    for(unsigned i=created_count;i>0;--i)
        if(!strcmp(created[i-1].feature,owner)) {created[i-1]=created[created_count-1];--created_count;}
    ReleaseSRWLockExclusive(&ui_lock);
    int done=0;
    for(unsigned k=n;k>0;--k) {   // newest first
        Obj p=static_cast<Obj>(panes[k-1]);
        if(guarded([&] {
            reinterpret_cast<void(__fastcall *)(Obj)>(vslot(p,0x40))(p);
            reinterpret_cast<void(__fastcall *)(Obj,Obj)>(vslot(p,0x28))(p,static_cast<Obj>(next));
            reinterpret_cast<void(__fastcall *)(Obj,int)>(vslot(p,0))(p,1);
        }))++done;
        else emit("layout_ui_fault",5,0,0);
    }
    if(n)emit("layout_ui_released",n,static_cast<unsigned>(done),hash32(owner));
    return done;
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
static int api_ui_begin(const char *owner) {return svc_ui_begin(owner);}
static void api_ui_end() {svc_ui_end();}
static void *api_ui_create_window(void *parent,int x,int y,int w,int h,const void *desc,int count) {return svc_ui_create_window(parent,x,y,w,h,desc,count);}
static int api_ui_activate(void *pane) {return svc_ui_activate(pane);}
static void *api_ui_node(void *layout,int node) {return svc_ui_node(layout,node);}
static int api_ui_set_node_text(void *layout,int node,const wchar_t *text) {return svc_ui_set_node_text(layout,node,text);}
static int api_ui_set_node_visible(void *layout,int node,int on) {return svc_ui_set_node_visible(layout,node,on);}
static int api_ui_select(void *radio,int on) {return svc_ui_select(radio,on);}
static int api_ui_sync_visibility(void *own,void *source) {return svc_ui_sync_visibility(own,source);}
static int api_ui_release(const char *owner,void *next) {return svc_ui_release(owner,next);}
static int api_ui_enable(void *control,int on) {return svc_ui_enable(control,on);}
static int api_ui_tooltip(const wchar_t *text) {return svc_ui_tooltip(text);}
}
