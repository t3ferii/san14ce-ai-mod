/* GPU pass-timing diagnostic (SAN14_Convenience.dll, [diagnostics] gpu_profile=1).
 * Observation only. Inside the game's own D3D11 calls, on its render thread, timestamp queries are issued at every
 * render-target change and read back a few frames later (DONOTFLUSH).
 * Frame edges come from SAN14_Layout.dll, the owner of the shared Present hook (user 2026-11-05): this unit subscribes to
 * its overlay callback, which runs on the render thread just before each real Present. At the first edge the swap
 * chain's device gives the immediate context, whose OM target changes, draws and dispatches are vtable-patched (one
 * vtable per slot; a different class is logged and left alone).
 * Output: AI_TacticsMod\data\diagnostics\gpu_passes.csv, one block per 5 s.
 *
 * local205 added three [diagnostics] switches (user goal: 120 fps uncapped on the laptop), all off = original:
 *   frame_profile=1  frame_pacing.csv: frame-time percentiles, time inside Present, the render thread's CPU per frame,
 *                    the game's update/render counts per second, display refresh rate and timer resolution.
 *   present_uncap=1  the game's Present SyncInterval (device +0x305C, 1 = vsync) set to 0.
 *   fps_120=1        the game's frame-rate table (30/60 frames per second, 1/30 and 1/60 s per frame, FPS_SETTING 0/1)
 *                    says 120 and 1/120 for setting 1. Without present_uncap the game then runs at half speed under vsync 60.
 * local206 (start-up crash of local205): local205 patched the DXGI factory and Present vtable slots directly. Right after
 * Layout's probe created a second swap chain, our Present hook and the Steam overlay's called each other until the stack
 * overflowed (crash dumps 2026-11-16: 4936 alternations above the game's 956F20). Inferred, not visible in the dumps (no
 * code memory): the overlay hooked the function then in the Present slot (ours) as its original. It also made Layout refuse its own
 * Present hook (layout_refused 3), so every Layout overlay was off. Now no DXGI slot is written here: Layout's overlay
 * callback is the frame edge, subscribed only after the game's swap chain exists (so the probe never precedes it), the
 * game's own end-of-Present clock (+0x32F0) closes the Present interval, and uncap writes the device field the game
 * reads. The D3D11 context hooks are installed only for gpu_profile=1. */
#define CINTERFACE
#define COBJMACROS
#define D3D11_NO_HELPERS   // C++ helper structs call C++ interface methods
#include "gpu_profile.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <intrin.h>
#include <algorithm>
#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG)
extern "C" void gpu_profile_crumb(const char *,bool);
struct GpuCrumbScope {const char *name;explicit GpuCrumbScope(const char *n):name(n){gpu_profile_crumb(name,true);}~GpuCrumbScope(){gpu_profile_crumb(name,false);}};
#define COST_SCOPE(name) GpuCrumbScope gpu_crumb_scope(name)
#else
#define COST_SCOPE(name) do {} while(0)
#endif
// Bridge in native_adapter.cpp (this unit has no address table or log): 0 = frame-rate table, 1 = application variable.
// Bridge in native_adapter.cpp (this unit has no address table or log): 0 = frame-rate table, 1 = application variable,
// 2 = the variable that holds the game's render device object (94E8F0 stores it).
extern "C" unsigned char *gpu_profile_game_data(unsigned which);
extern "C" void gpu_profile_event(const char *kind,unsigned a,unsigned b,unsigned c);
// Layout API (SAN14_Layout.dll): on_layout_built(any) and overlay_subscribe; -100 = Layout missing, nothing installed.
extern "C" int gpu_profile_layout_watch(void (*built)(void *,void *));
extern "C" int gpu_profile_overlay_subscribe(void (*draw)(void *,void *));

namespace {
constexpr unsigned kSlots=4,kSegs=192,kAgg=320,kInherited=0xFFFF;
constexpr double kWindowSeconds=5.0;
struct Target {UINT w,h,fmt,samples;};
struct Key {Target rt,ds;UINT rts,uav;};
struct Seg {Key key;UINT draws,dispatches;};
struct Slot {ID3D11Query *disjoint;ID3D11Query *ts[kSegs+1];Seg seg[kSegs];UINT n;bool pending;};
struct Agg {Key key;UINT occ,frames;double ms,max_ms,pos;unsigned long long draws,dispatches;};

SRWLOCK lock=SRWLOCK_INIT;
HANDLE file=INVALID_HANDLE_VALUE;
LARGE_INTEGER frequency,origin,window_start;
IDXGISwapChain *chain;UINT chain_area;HWND window;
ID3D11Device *query_device;
ID3D11DeviceContext *volatile context;   // game's immediate context; no reference held
Slot slots[kSlots];Slot *current;unsigned slot_index;
const void *bound[3];bool bound_valid,broken;
Agg agg[kAgg];UINT agg_count,resolved,dropped,presents;double frame_ms,frame_max;
char text[128*1024];
bool passes_on,pacing_on,uncap_on;              // gpu_profile / frame_profile / present_uncap
volatile LONG imm_draws,other_draws,exec_lists; // draw tallies (context hooks, gpu_profile=1 only), drained every window
bool context_patched;
volatile LONG subscribed,reentry_logged;
void pacing_note(const char *format,...);

void write_text(const char *s,size_t n) {
    if(file==INVALID_HANDLE_VALUE || !n)return;
    DWORD written;WriteFile(file,s,static_cast<DWORD>(n),&written,nullptr);
}
void note(const char *what,const void *a=nullptr,const void *b=nullptr) {
    if(file==INVALID_HANDLE_VALUE) {pacing_note("%s %p %p",what,a,b);return;}   // pacing-only sessions keep the notes too
    char line[160];LARGE_INTEGER now;QueryPerformanceCounter(&now);
    int n=snprintf(line,sizeof line,"# %.1f %s %p %p\r\n",
        double(now.QuadPart-origin.QuadPart)/double(frequency.QuadPart),what,a,b);
    if(n>0)write_text(line,static_cast<size_t>(n));
}

// One vtable per hooked slot; the original pointer is saved before the swap.
struct Patch {void **vtable;void *real;};
bool patch(Patch &p,void *object,size_t offset,void *hook,const char *name) {
    void **vtable=*reinterpret_cast<void ***>(object);
    if(p.vtable)return p.vtable==vtable || (note(name,vtable,p.vtable),false);
    void **slot=vtable+offset/sizeof(void *);
    DWORD old=0;
    if(!VirtualProtect(slot,sizeof(void *),PAGE_READWRITE,&old)) {note(name,vtable);return false;}
    p.real=*slot;p.vtable=vtable;
    InterlockedExchangePointer(slot,hook);
    VirtualProtect(slot,sizeof(void *),old,&old);
    return true;
}
#define REAL(p,member,vtbl) reinterpret_cast<decltype(vtbl::member)>(p.real)

Patch p_set,p_set_uav,p_draw[7],p_dispatch[2],p_execute;

void describe(Key &k,UINT n,ID3D11RenderTargetView *const *rtv,ID3D11DepthStencilView *dsv,UINT uav) {
    k.rts=n;k.uav=uav;
    if(n && rtv && rtv[0]) {
        D3D11_RENDER_TARGET_VIEW_DESC vd;ID3D11RenderTargetView_GetDesc(rtv[0],&vd);
        ID3D11Resource *r=nullptr;ID3D11RenderTargetView_GetResource(rtv[0],&r);
        ID3D11Texture2D *t=nullptr;
        if(r && SUCCEEDED(ID3D11Resource_QueryInterface(r,IID_ID3D11Texture2D,reinterpret_cast<void **>(&t))) && t) {
            D3D11_TEXTURE2D_DESC d;ID3D11Texture2D_GetDesc(t,&d);
            k.rt={d.Width,d.Height,static_cast<UINT>(vd.Format),d.SampleDesc.Count};ID3D11Texture2D_Release(t);
        }
        if(r)ID3D11Resource_Release(r);
    }
    if(dsv) {
        D3D11_DEPTH_STENCIL_VIEW_DESC vd;ID3D11DepthStencilView_GetDesc(dsv,&vd);
        ID3D11Resource *r=nullptr;ID3D11DepthStencilView_GetResource(dsv,&r);
        ID3D11Texture2D *t=nullptr;
        if(r && SUCCEEDED(ID3D11Resource_QueryInterface(r,IID_ID3D11Texture2D,reinterpret_cast<void **>(&t))) && t) {
            D3D11_TEXTURE2D_DESC d;ID3D11Texture2D_GetDesc(t,&d);
            k.ds={d.Width,d.Height,static_cast<UINT>(vd.Format),d.SampleDesc.Count};ID3D11Texture2D_Release(t);
        }
        if(r)ID3D11Resource_Release(r);
    }
}
void boundary(ID3D11DeviceContext *c,UINT n,ID3D11RenderTargetView *const *rtv,ID3D11DepthStencilView *dsv,UINT uav) {
    if(!passes_on || c!=context)return;
    AcquireSRWLockExclusive(&lock);
    Slot *f=current;
    const void *id[3]={n && rtv?rtv[0]:nullptr,dsv,reinterpret_cast<const void *>(static_cast<UINT_PTR>(n | uav<<16))};
    if(f && f->n<kSegs && !(bound_valid && !memcmp(id,bound,sizeof id))) {
        memcpy(bound,id,sizeof id);bound_valid=true;
        ID3D11DeviceContext_End(c,reinterpret_cast<ID3D11Asynchronous *>(f->ts[f->n]));
        Seg &s=f->seg[f->n++];s={};describe(s.key,n,rtv,dsv,uav);
    }
    ReleaseSRWLockExclusive(&lock);
}
inline void count(ID3D11DeviceContext *c,bool dispatch) {
    // Deferred contexts reach these hooks only when they share the immediate context's class.
    if(pacing_on && !dispatch)InterlockedIncrement(c==context?&imm_draws:&other_draws);
    if(!passes_on)return;
    Slot *f=current;   // slots are static; a racing frame edge can only misattribute one count
    if(c!=context || !f || !f->n)return;
    Seg &s=f->seg[f->n-1];
    if(dispatch)++s.dispatches;else ++s.draws;
}
// Re-entry of a context hook on one thread is logged once at depth 2 (may be legitimate: another module's hook drawing inside
// OMSetRenderTargets) and once at depth 32 (a call cycle).
thread_local int hook_depth;
struct DepthGuard {
    DepthGuard() {
        const int d=++hook_depth;
        if(d==2 && !InterlockedCompareExchange(&reentry_logged,1,0))gpu_profile_event("gpu_profile_reentry",2,0,0);
        if(d==32 && InterlockedCompareExchange(&reentry_logged,2,1)==1)gpu_profile_event("gpu_profile_reentry",32,0,0);
    }
    ~DepthGuard() {--hook_depth;}
};

void STDMETHODCALLTYPE set_targets(ID3D11DeviceContext *c,UINT n,ID3D11RenderTargetView *const *rtv,ID3D11DepthStencilView *dsv) { COST_SCOPE("gpu_profile::set_targets"); DepthGuard depth_guard;
    boundary(c,n,rtv,dsv,0);
    REAL(p_set,OMSetRenderTargets,ID3D11DeviceContextVtbl)(c,n,rtv,dsv);
}
void STDMETHODCALLTYPE set_targets_uav(ID3D11DeviceContext *c,UINT n,ID3D11RenderTargetView *const *rtv,ID3D11DepthStencilView *dsv,
                                        UINT start,UINT uavs,ID3D11UnorderedAccessView *const *views,const UINT *counts) { COST_SCOPE("gpu_profile::set_targets_uav"); DepthGuard depth_guard;
    if(n!=D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)boundary(c,n,rtv,dsv,uavs!=D3D11_KEEP_UNORDERED_ACCESS_VIEWS && uavs?1u:0u);
    REAL(p_set_uav,OMSetRenderTargetsAndUnorderedAccessViews,ID3D11DeviceContextVtbl)(c,n,rtv,dsv,start,uavs,views,counts);
}
void STDMETHODCALLTYPE draw_indexed(ID3D11DeviceContext *c,UINT a,UINT b,INT d) { COST_SCOPE("gpu_profile::draw_indexed"); DepthGuard depth_guard;
    count(c,false);REAL(p_draw[0],DrawIndexed,ID3D11DeviceContextVtbl)(c,a,b,d);
}
void STDMETHODCALLTYPE draw(ID3D11DeviceContext *c,UINT a,UINT b) { COST_SCOPE("gpu_profile::draw"); DepthGuard depth_guard;
    count(c,false);REAL(p_draw[1],Draw,ID3D11DeviceContextVtbl)(c,a,b);
}
void STDMETHODCALLTYPE draw_indexed_instanced(ID3D11DeviceContext *c,UINT a,UINT b,UINT d,INT e,UINT f) { COST_SCOPE("gpu_profile::draw_indexed_instanced"); DepthGuard depth_guard;
    count(c,false);REAL(p_draw[2],DrawIndexedInstanced,ID3D11DeviceContextVtbl)(c,a,b,d,e,f);
}
void STDMETHODCALLTYPE draw_instanced(ID3D11DeviceContext *c,UINT a,UINT b,UINT d,UINT e) { COST_SCOPE("gpu_profile::draw_instanced"); DepthGuard depth_guard;
    count(c,false);REAL(p_draw[3],DrawInstanced,ID3D11DeviceContextVtbl)(c,a,b,d,e);
}
void STDMETHODCALLTYPE draw_auto(ID3D11DeviceContext *c) { COST_SCOPE("gpu_profile::draw_auto"); DepthGuard depth_guard;
    count(c,false);REAL(p_draw[4],DrawAuto,ID3D11DeviceContextVtbl)(c);
}
void STDMETHODCALLTYPE draw_indexed_indirect(ID3D11DeviceContext *c,ID3D11Buffer *b,UINT o) { COST_SCOPE("gpu_profile::draw_indexed_indirect"); DepthGuard depth_guard;
    count(c,false);REAL(p_draw[5],DrawIndexedInstancedIndirect,ID3D11DeviceContextVtbl)(c,b,o);
}
void STDMETHODCALLTYPE draw_indirect(ID3D11DeviceContext *c,ID3D11Buffer *b,UINT o) { COST_SCOPE("gpu_profile::draw_indirect"); DepthGuard depth_guard;
    count(c,false);REAL(p_draw[6],DrawInstancedIndirect,ID3D11DeviceContextVtbl)(c,b,o);
}
void STDMETHODCALLTYPE dispatch(ID3D11DeviceContext *c,UINT x,UINT y,UINT z) { COST_SCOPE("gpu_profile::dispatch"); DepthGuard depth_guard;
    count(c,true);REAL(p_dispatch[0],Dispatch,ID3D11DeviceContextVtbl)(c,x,y,z);
}
void STDMETHODCALLTYPE dispatch_indirect(ID3D11DeviceContext *c,ID3D11Buffer *b,UINT o) { COST_SCOPE("gpu_profile::dispatch_indirect"); DepthGuard depth_guard;
    count(c,true);REAL(p_dispatch[1],DispatchIndirect,ID3D11DeviceContextVtbl)(c,b,o);
}

void STDMETHODCALLTYPE execute_list(ID3D11DeviceContext *c,ID3D11CommandList *list,BOOL restore) { COST_SCOPE("gpu_profile::execute_list"); DepthGuard depth_guard;
    if(pacing_on)InterlockedIncrement(&exec_lists);
    REAL(p_execute,ExecuteCommandList,ID3D11DeviceContextVtbl)(c,list,restore);
}

void patch_context(ID3D11DeviceContext *c) {
#define SLOT(member) offsetof(ID3D11DeviceContextVtbl,member)
    bool ok=patch(p_set,c,SLOT(OMSetRenderTargets),reinterpret_cast<void *>(set_targets),"context_class_skipped");
    if(!ok)return;
    context_patched=true;
    patch(p_set_uav,c,SLOT(OMSetRenderTargetsAndUnorderedAccessViews),reinterpret_cast<void *>(set_targets_uav),"context_slot");
    patch(p_draw[0],c,SLOT(DrawIndexed),reinterpret_cast<void *>(draw_indexed),"context_slot");
    patch(p_draw[1],c,SLOT(Draw),reinterpret_cast<void *>(draw),"context_slot");
    patch(p_draw[2],c,SLOT(DrawIndexedInstanced),reinterpret_cast<void *>(draw_indexed_instanced),"context_slot");
    patch(p_draw[3],c,SLOT(DrawInstanced),reinterpret_cast<void *>(draw_instanced),"context_slot");
    patch(p_draw[4],c,SLOT(DrawAuto),reinterpret_cast<void *>(draw_auto),"context_slot");
    patch(p_draw[5],c,SLOT(DrawIndexedInstancedIndirect),reinterpret_cast<void *>(draw_indexed_indirect),"context_slot");
    patch(p_draw[6],c,SLOT(DrawInstancedIndirect),reinterpret_cast<void *>(draw_indirect),"context_slot");
    patch(p_dispatch[0],c,SLOT(Dispatch),reinterpret_cast<void *>(dispatch),"context_slot");
    patch(p_dispatch[1],c,SLOT(DispatchIndirect),reinterpret_cast<void *>(dispatch_indirect),"context_slot");
    if(pacing_on)patch(p_execute,c,SLOT(ExecuteCommandList),reinterpret_cast<void *>(execute_list),"context_slot");
#undef SLOT
}

void release_queries() {
    for(Slot &s:slots) {
        if(s.disjoint)ID3D11Query_Release(s.disjoint);
        for(ID3D11Query *&q:s.ts)if(q)ID3D11Query_Release(q),q=nullptr;
        s.disjoint=nullptr;s.pending=false;s.n=0;
    }
    current=nullptr;
}
bool create_queries(ID3D11Device *d) {
    D3D11_QUERY_DESC qd={D3D11_QUERY_TIMESTAMP_DISJOINT,0};
    for(Slot &s:slots) {
        if(FAILED(ID3D11Device_CreateQuery(d,&qd,&s.disjoint)))return false;
        D3D11_QUERY_DESC td={D3D11_QUERY_TIMESTAMP,0};
        for(ID3D11Query *&q:s.ts)if(FAILED(ID3D11Device_CreateQuery(d,&td,&q)))return false;
    }
    return true;
}
// Track the largest swap chain (the game also owns a secondary one).
bool adopt(IDXGISwapChain *sc) {
    if(sc==chain)return true;
    DXGI_SWAP_CHAIN_DESC desc;
    if(FAILED(IDXGISwapChain_GetDesc(sc,&desc)))return false;
    const UINT area=desc.BufferDesc.Width*desc.BufferDesc.Height;
    if(chain && area<=chain_area)return false;
    ID3D11Device *d=nullptr;
    if(FAILED(IDXGISwapChain_GetDevice(sc,IID_ID3D11Device,reinterpret_cast<void **>(&d))) || !d)return false;
    ID3D11DeviceContext *c=nullptr;ID3D11Device_GetImmediateContext(d,&c);
    if(passes_on && d!=query_device) {
        release_queries();
        if(query_device)ID3D11Device_Release(query_device);
        query_device=d;ID3D11Device_AddRef(d);
        if(!create_queries(d)) {broken=true;note("query_create_failed",d);}
    }
    ID3D11Device_Release(d);
    if(!c)return false;
    current=nullptr;for(Slot &s:slots)s.pending=false;
    chain=sc;chain_area=area;window=desc.OutputWindow;context=c;
    patch_context(c);
    ID3D11DeviceContext_Release(c);    // the game keeps its device and context alive
    char line[96];int n=snprintf(line,sizeof line,"chain_%ux%u",desc.BufferDesc.Width,desc.BufferDesc.Height);
    if(n>0)note(line,sc,c);
    return true;   // frame_edge checks `broken` itself; frame pacing needs no queries
}

bool same_key(const Key &a,const Key &b) {return !memcmp(&a,&b,sizeof(Key));}
void resolve(Slot &f) {
    ID3D11DeviceContext *c=context;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj;
    if(ID3D11DeviceContext_GetData(c,reinterpret_cast<ID3D11Asynchronous *>(f.disjoint),&dj,sizeof dj,D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK)return;
    f.pending=false;
    if(dj.Disjoint || !dj.Frequency) {++dropped;return;}
    static UINT64 t[kSegs+1];
    for(UINT i=0;i<=f.n;++i)
        if(ID3D11DeviceContext_GetData(c,reinterpret_cast<ID3D11Asynchronous *>(f.ts[i]),&t[i],sizeof t[i],D3D11_ASYNC_GETDATA_DONOTFLUSH)!=S_OK) {++dropped;return;}
    ++resolved;
    const double scale=1000.0/double(dj.Frequency);
    const double total=t[f.n]>t[0]?double(t[f.n]-t[0])*scale:0.0;
    frame_ms+=total;if(total>frame_max)frame_max=total;
    for(UINT j=0;j<f.n;++j) {
        UINT occ=0;
        for(UINT i=0;i<j;++i)occ+=same_key(f.seg[i].key,f.seg[j].key);
        UINT k=0;
        while(k<agg_count && !(agg[k].occ==occ && same_key(agg[k].key,f.seg[j].key)))++k;
        if(k==agg_count) {
            if(agg_count==kAgg)k=kAgg-1;   // last entry doubles as overflow bucket
            else {agg[k]={};agg[k].key=f.seg[j].key;agg[k].occ=occ;++agg_count;}
        }
        const double ms=t[j+1]>t[j]?double(t[j+1]-t[j])*scale:0.0;
        Agg &a=agg[k];
        a.ms+=ms;if(ms>a.max_ms)a.max_ms=ms;a.pos+=j;++a.frames;
        a.draws+=f.seg[j].draws;a.dispatches+=f.seg[j].dispatches;
    }
}
void describe_text(char *out,size_t size,const Target &t,UINT rts,UINT uav,bool target) {
    if(target && rts==kInherited) {snprintf(out,size,"start");return;}
    if(!t.w) {
        if(target && rts)snprintf(out,size,"n%u",rts);else snprintf(out,size,"-");
        return;
    }
    if(target)snprintf(out,size,"%ux%u/f%u/s%u/n%u%s",t.w,t.h,t.fmt,t.samples,rts,uav?"+uav":"");
    else snprintf(out,size,"%ux%u/f%u/s%u",t.w,t.h,t.fmt,t.samples);
}
void flush(const LARGE_INTEGER &now) {
    const double secs=double(now.QuadPart-window_start.QuadPart)/double(frequency.QuadPart);
    if(secs<kWindowSeconds)return;
    HWND fg=GetForegroundWindow();
    const int focus=window && (fg==window || GetAncestor(fg,GA_ROOTOWNER)==window);
    const int minimized=window && IsIconic(window);
    char prefix[160];
    snprintf(prefix,sizeof prefix,"%.1f,%.2f,%u,%.1f,%u,%u,%d,%d,",
        double(now.QuadPart-origin.QuadPart)/double(frequency.QuadPart),secs,presents,presents/secs,resolved,dropped,focus,minimized);
    size_t len=0;
    auto put=[&](int n) {if(n>0 && len+static_cast<size_t>(n)<sizeof text)len+=static_cast<size_t>(n);};
    const double frames=resolved?double(resolved):1.0;
    put(snprintf(text+len,sizeof text-len,"%sFRAME,0,-,-,%.3f,%.3f,100,,,\r\n",prefix,frame_ms/frames,frame_max));
    UINT order[kAgg];
    for(UINT i=0;i<agg_count;++i) {
        UINT j=i;
        while(j && agg[order[j-1]].pos/agg[order[j-1]].frames>agg[i].pos/agg[i].frames) {order[j]=order[j-1];--j;}
        order[j]=i;
    }
    for(UINT i=0;i<agg_count && len+512<sizeof text;++i) {
        const Agg &a=agg[order[i]];char rt[64],ds[64];
        describe_text(rt,sizeof rt,a.key.rt,a.key.rts,a.key.uav,true);
        describe_text(ds,sizeof ds,a.key.ds,0,0,false);
        put(snprintf(text+len,sizeof text-len,"%s%u,%u,%s,%s,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f\r\n",prefix,i+1,a.occ,rt,ds,
            a.ms/frames,a.max_ms,frame_ms>0?100.0*a.ms/frame_ms:0.0,double(a.draws)/frames,double(a.dispatches)/frames,a.pos/a.frames));
    }
    write_text(text,len);
    agg_count=resolved=dropped=presents=0;frame_ms=frame_max=0;window_start=now;
}
// Before the game's Present: close the frame. After it: start the next one.
void frame_edge(IDXGISwapChain *sc,bool before) {
    if(broken)return;
    AcquireSRWLockExclusive(&lock);
    if(adopt(sc) && !broken) {
        ID3D11DeviceContext *c=context;
        if(before) {
            if(Slot *f=current) {
                ID3D11DeviceContext_End(c,reinterpret_cast<ID3D11Asynchronous *>(f->ts[f->n]));
                ID3D11DeviceContext_End(c,reinterpret_cast<ID3D11Asynchronous *>(f->disjoint));
                f->pending=true;current=nullptr;
            }
            ++presents;
        } else {
            for(unsigned i=1;i<=kSlots;++i) {Slot &s=slots[(slot_index+i)%kSlots];if(s.pending)resolve(s);}
            slot_index=(slot_index+1)%kSlots;
            Slot &f=slots[slot_index];
            if(f.pending) {f.pending=false;++dropped;}   // GPU still behind: reuse the slot
            ID3D11DeviceContext_Begin(c,reinterpret_cast<ID3D11Asynchronous *>(f.disjoint));
            ID3D11DeviceContext_End(c,reinterpret_cast<ID3D11Asynchronous *>(f.ts[0]));
            f.n=1;f.seg[0]={};f.seg[0].key.rts=kInherited;bound_valid=false;current=&f;
            LARGE_INTEGER now;QueryPerformanceCounter(&now);flush(now);
        }
    }
    ReleaseSRWLockExclusive(&lock);
}
// ---- frame pacing (frame_profile=1): one row per 5 s in frame_pacing.csv, render thread only ----
// The game's render device object (variable 27DF718 in 1a4a447d): +0x3080 main swap chain, +0x305C SyncInterval that the Present
// wrapper 956F20 passes, +0x32E0 ticks its last call took, +0x32F0 the game clock (QPC) at the end of that call (after Present).
unsigned char *game_device() {
    const unsigned char *variable=gpu_profile_game_data(2);
    return variable?*reinterpret_cast<unsigned char *const volatile *>(variable):nullptr;
}
constexpr UINT kIntervals=4096;
struct Pacing {
    HANDLE file;DWORD thread;bool started;
    LARGE_INTEGER window_start;unsigned long long tsc_start,thread_time_start,updates_start,renders_start;bool counters_start;
    LONGLONG last_enter;ULONG64 last_cycles;
    float interval[kIntervals];UINT n,frames,presents,ends,no_end,other_thread,foreign,sync_changes;LONG sync;
    double present_ms,wrapper_ms,out_ms;ULONG64 cycles;
};
Pacing pacing={INVALID_HANDLE_VALUE};
bool uncap_ok=true;unsigned uncap_writes;
volatile LONG foreign_total;

void pacing_note(const char *format,...) {
    if(pacing.file==INVALID_HANDLE_VALUE)return;
    char line[512];LARGE_INTEGER now;QueryPerformanceCounter(&now);
    int n=snprintf(line,sizeof line,"# %.1f ",double(now.QuadPart-origin.QuadPart)/double(frequency.QuadPart));
    if(n<0)return;
    va_list args;va_start(args,format);
    int m=vsnprintf(line+n,sizeof line-static_cast<size_t>(n)-2,format,args);
    va_end(args);
    if(m<0)return;
    size_t len=(std::min)(sizeof line-3,static_cast<size_t>(n+m));
    line[len++]='\r';line[len++]='\n';
    DWORD written;WriteFile(pacing.file,line,static_cast<DWORD>(len),&written,nullptr);
}
unsigned long long thread_time() {   // kernel + user, 100 ns units (quantum-granular; sound over a 5 s window)
    FILETIME c,e,k,u;
    if(!GetThreadTimes(GetCurrentThread(),&c,&e,&k,&u))return 0;
    return (static_cast<unsigned long long>(k.dwHighDateTime)<<32 | k.dwLowDateTime)+(static_cast<unsigned long long>(u.dwHighDateTime)<<32 | u.dwLowDateTime);
}
void timer_resolution(ULONG &coarsest,ULONG &finest,ULONG &now_res) {   // 100 ns units
    coarsest=finest=now_res=0;
    using Query=LONG(WINAPI *)(PULONG,PULONG,PULONG);
    static const Query query=reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryTimerResolution"));
    if(query)query(&coarsest,&finest,&now_res);
}
// The application's UpdateFPS / RenderFPS counters: +E8 and +178 count calls of app slots 2 (update, 14C5E0) and 8 (render, 154960).
bool app_counters(unsigned long long &updates,unsigned long long &renders) {
    const unsigned char *variable=gpu_profile_game_data(1);
    const unsigned char *app=variable?*reinterpret_cast<const unsigned char *const volatile *>(variable):nullptr;
    if(!app)return false;
    updates=*reinterpret_cast<const volatile unsigned long long *>(app+0xE8);
    renders=*reinterpret_cast<const volatile unsigned long long *>(app+0x178);
    return true;
}
void pacing_window_start(LONGLONG now) {
    pacing.window_start.QuadPart=now;pacing.tsc_start=__rdtsc();pacing.thread_time_start=thread_time();
    pacing.counters_start=app_counters(pacing.updates_start,pacing.renders_start);
    pacing.n=pacing.frames=pacing.presents=pacing.ends=pacing.no_end=pacing.other_thread=pacing.foreign=pacing.sync_changes=0;
    pacing.present_ms=pacing.wrapper_ms=pacing.out_ms=0;pacing.cycles=0;
    InterlockedExchange(&imm_draws,0);InterlockedExchange(&other_draws,0);InterlockedExchange(&exec_lists,0);
}
void pacing_first(IDXGISwapChain *sc,const unsigned char *dev,LONGLONG now) {
    DXGI_SWAP_CHAIN_DESC d={};IDXGISwapChain_GetDesc(sc,&d);
    if(!window)window=d.OutputWindow;
    DWORD hz=0;MONITORINFOEXW mi={};mi.cbSize=sizeof mi;DEVMODEW dm={};dm.dmSize=sizeof dm;
    HMONITOR monitor=MonitorFromWindow(d.OutputWindow,MONITOR_DEFAULTTONEAREST);
    if(monitor && GetMonitorInfoW(monitor,reinterpret_cast<MONITORINFO *>(&mi)) && EnumDisplaySettingsW(mi.szDevice,ENUM_CURRENT_SETTINGS,&dm))hz=dm.dmDisplayFrequency;
    ULONG coarse,fine,res;timer_resolution(coarse,fine,res);
    const LONG sync=dev?*reinterpret_cast<const volatile LONG *>(dev+0x305C):-1;
    pacing_note("first_present device=%p sync=%ld chain=%ux%u format=%u buffers=%u effect=%u chain_flags=0x%X windowed=%d refresh=%u/%u display_hz=%lu timer_100ns=%lu/%lu/%lu uncap=%d thread=%lu",
        dev,sync,d.BufferDesc.Width,d.BufferDesc.Height,static_cast<UINT>(d.BufferDesc.Format),d.BufferCount,static_cast<UINT>(d.SwapEffect),d.Flags,
        d.Windowed?1:0,d.BufferDesc.RefreshRate.Numerator,d.BufferDesc.RefreshRate.Denominator,hz,coarse,fine,res,uncap_on?1:0,GetCurrentThreadId());
    gpu_profile_event("frame_pacing_start",hz,static_cast<UINT>(sync<0?0xFF:sync) | static_cast<UINT>(d.SwapEffect)<<8 | d.BufferCount<<16 | (uncap_on?1u:0u)<<24,dev?res:0u);
    pacing.thread=GetCurrentThreadId();pacing.sync=sync;pacing.started=true;
    pacing_window_start(now);
}
float percentile(double q) {
    const UINT i=static_cast<UINT>(q*double(pacing.n-1)+0.5);
    return pacing.interval[i<pacing.n?i:pacing.n-1];
}
void pacing_flush(LONGLONG now) {
    const double secs=double(now-pacing.window_start.QuadPart)/double(frequency.QuadPart);
    if(secs<kWindowSeconds)return;
    const double tsc_ms=double(__rdtsc()-pacing.tsc_start)/(secs*1000.0);   // invariant TSC ticks per ms (QueryThreadCycleTime unit)
    const double cpu_thread=double(thread_time()-pacing.thread_time_start)/10000.0;
    unsigned long long updates=0,renders=0;
    const bool counters=pacing.counters_start && app_counters(updates,renders);
    const double frames=pacing.frames?double(pacing.frames):1.0,ends=pacing.ends?double(pacing.ends):1.0;
    float p50=0,p95=0,p99=0,peak=0;
    if(pacing.n) {std::sort(pacing.interval,pacing.interval+pacing.n);p50=percentile(0.50);p95=percentile(0.95);p99=percentile(0.99);peak=pacing.interval[pacing.n-1];}
    const double cpu_frame=(tsc_ms>0?double(pacing.cycles)/tsc_ms:0.0)/frames;
    const double out=pacing.ends?pacing.out_ms/ends:-1.0;
    const double idle=pacing.ends?(out>cpu_frame?out-cpu_frame:0.0):-1.0;
    HWND fg=GetForegroundWindow();
    const int focus=window && (fg==window || GetAncestor(fg,GA_ROOTOWNER)==window);
    const int minimized=window && IsIconic(window);
    ULONG coarse,fine,res;timer_resolution(coarse,fine,res);
    const LONG imm=InterlockedExchange(&imm_draws,0),other=InterlockedExchange(&other_draws,0),lists=InterlockedExchange(&exec_lists,0);
    const bool drawn=context_patched;
    const long used=uncap_on && uncap_ok && uncap_writes?0L:static_cast<long>(pacing.sync);
    char line[720];
    int n=snprintf(line,sizeof line,"%.1f,%.2f,%u,%.1f,%.2f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f,%.2f,%ld,%ld,%u,%.3f,%d,%d,%u,%u,%u\r\n",
        double(now-origin.QuadPart)/double(frequency.QuadPart),secs,pacing.presents,pacing.presents/secs,p50,p95,p99,peak,
        pacing.ends?pacing.present_ms/ends:-1.0,pacing.ends?pacing.wrapper_ms/ends:-1.0,out,cpu_frame,idle,cpu_thread/frames,
        counters?double(updates-pacing.updates_start)/secs:-1.0,counters?double(renders-pacing.renders_start)/secs:-1.0,
        drawn?double(imm)/frames:-1.0,drawn?double(other)/frames:-1.0,drawn?double(lists)/frames:-1.0,
        static_cast<long>(pacing.sync),used,pacing.sync_changes,double(res)/10000.0,focus,minimized,pacing.other_thread,pacing.foreign,pacing.no_end);
    if(n>0 && pacing.file!=INVALID_HANDLE_VALUE) {DWORD written;WriteFile(pacing.file,line,static_cast<DWORD>((std::min<size_t>)(sizeof line-1,static_cast<size_t>(n))),&written,nullptr);}
    pacing_window_start(now);
}
// One frame edge on the render thread (inside the game's 956F20, before the real Present).
void pace(IDXGISwapChain *sc,const unsigned char *dev) {
    LARGE_INTEGER now;QueryPerformanceCounter(&now);
    if(!pacing.started)pacing_first(sc,dev,now.QuadPart);
    if(GetCurrentThreadId()!=pacing.thread) {++pacing.other_thread;return;}
    ULONG64 cycles=0;QueryThreadCycleTime(GetCurrentThread(),&cycles);
    const double ms=1000.0/double(frequency.QuadPart);
    if(pacing.last_enter) {
        if(pacing.n<kIntervals)pacing.interval[pacing.n++]=float(double(now.QuadPart-pacing.last_enter)*ms);
        ++pacing.frames;pacing.cycles+=cycles-pacing.last_cycles;
        if(dev) {
            // The previous 956F20 call ended after our previous edge and before this one when the game clock is QPC (it is
            // whenever QueryPerformanceCounter exists); anything else is counted in no_end and left out.
            const LONGLONG end=*reinterpret_cast<const volatile LONGLONG *>(dev+0x32F0);
            const LONGLONG inside=*reinterpret_cast<const volatile LONGLONG *>(dev+0x32E0);
            if(end>pacing.last_enter && end<=now.QuadPart) {
                pacing.present_ms+=double(end-pacing.last_enter)*ms;pacing.out_ms+=double(now.QuadPart-end)*ms;
                if(inside>0 && inside<frequency.QuadPart)pacing.wrapper_ms+=double(inside)*ms;
                ++pacing.ends;
            } else ++pacing.no_end;
        }
    }
    if(dev && !uncap_on) {
        const LONG v=*reinterpret_cast<const volatile LONG *>(dev+0x305C);
        if(v!=pacing.sync) {pacing.sync=v;++pacing.sync_changes;}
    }
    pacing.last_enter=now.QuadPart;pacing.last_cycles=cycles;++pacing.presents;
    pacing_flush(now.QuadPart);
}
// present_uncap: the game passes device +0x305C to Present (956F20); its device reset (9571D0) writes it again, so it is
// checked at every edge. A value outside DXGI's 0..4 means the device layout is not the expected one: refused once, never written.
void uncap(unsigned char *dev) {
    if(!uncap_ok)return;
    if(!dev) {uncap_ok=false;gpu_profile_event("present_uncap_refused",0,0,0);pacing_note("present_uncap_refused no_device");return;}
    volatile LONG *sync=reinterpret_cast<volatile LONG *>(dev+0x305C);
    const LONG v=*sync;
    if(!v)return;
    if(v<0 || v>4) {uncap_ok=false;gpu_profile_event("present_uncap_refused",1,static_cast<unsigned>(v),0);pacing_note("present_uncap_refused sync=%ld",v);return;}
    *sync=0;
    if(pacing.sync!=v) {pacing.sync=v;}
    ++pacing.sync_changes;
    if(++uncap_writes<=4)gpu_profile_event("present_uncap_applied",static_cast<unsigned>(v),uncap_writes,0);
}
// Layout overlay callback: render thread, before each real Present of any swap chain (DXGI_PRESENT_TEST presents skipped).
void __cdecl on_present(void *chain_pointer,void *) { COST_SCOPE("gpu_profile::on_present");
    IDXGISwapChain *sc=static_cast<IDXGISwapChain *>(chain_pointer);
    unsigned char *dev=game_device();
    if(dev && *reinterpret_cast<void *const volatile *>(dev+0x3080)!=chain_pointer) {   // probe, secondary target
        ++pacing.foreign;
        // Logged twice: the first one, and 600 with no main-chain frame yet (device variable or +0x3080 not what we expect).
        const LONG n=InterlockedIncrement(&foreign_total);
        if(n==1 || (n==600 && !pacing.started)) {
            gpu_profile_event(n==1?"gpu_profile_foreign_chain":"gpu_profile_no_main_chain",static_cast<unsigned>(n),pacing.started?1u:0u,0);
            pacing_note("foreign_chain count=%ld chain=%p device=%p device_chain=%p",n,chain_pointer,dev,*reinterpret_cast<void *const volatile *>(dev+0x3080));
        }
        return;
    }
    if(passes_on) {frame_edge(sc,true);frame_edge(sc,false);}   // close frame N, open N+1 (Present itself has no GPU work of ours)
    if(pacing_on)pace(sc,dev);
    if(uncap_on)uncap(dev);   // takes effect from the next Present
}
// Layout built callback (game UI thread, every build until subscribed). Subscribing makes Layout probe the Present vtable with a
// temporary swap chain; doing that before the game's own chain exists could let the Steam overlay take Layout's slot as its
// original (the local205 cycle, mechanism inferred), so wait for device +0x3080. Without a resolved device variable this keeps Layout's own timing:
// its diagnostic overlay subscribes at the first tracked layout build too.
void __cdecl on_layout_built(void *,void *) {
    if(subscribed)return;
    const unsigned char *variable=gpu_profile_game_data(2);
    unsigned char *dev=game_device();
    if(variable && (!dev || !*reinterpret_cast<void *const volatile *>(dev+0x3080)))return;
    if(InterlockedCompareExchange(&subscribed,1,0))return;
    const int index=gpu_profile_overlay_subscribe(&on_present);
    gpu_profile_event("gpu_profile_present_subscribed",index>=0?1u:0u,static_cast<unsigned>(index),dev?1u:0u);
    pacing_note("present_subscribed index=%d device=%p",index,dev);
}
// fps_120: setting 1 of the frame-rate table becomes 120 / (1/120 s). Refused unless the live values are exactly the
// original 30, 60, 1/30, 1/60 (bit patterns of this build). The application object's existence at patch time is logged:
// app slot 6 (154D50) hands 1/fps to the engine when it runs, so a patch after it may leave the engine at 1/60.
void patch_fps_table() {
    unsigned char *table=gpu_profile_game_data(0);
    const unsigned char *variable=gpu_profile_game_data(1);
    const unsigned app=variable && *reinterpret_cast<void *const volatile *>(variable)?1u:0u;
    FILETIME creation,exit_time,kernel,user,now;GetSystemTimeAsFileTime(&now);
    unsigned uptime=0;
    if(GetProcessTimes(GetCurrentProcess(),&creation,&exit_time,&kernel,&user)) {
        const unsigned long long a=static_cast<unsigned long long>(creation.dwHighDateTime)<<32 | creation.dwLowDateTime;
        const unsigned long long b=static_cast<unsigned long long>(now.dwHighDateTime)<<32 | now.dwLowDateTime;
        uptime=b>a?static_cast<unsigned>((b-a)/10000):0u;
    }
    if(!table) {gpu_profile_event("fps120_refused",0,0,app);return;}
    static const unsigned original[4]={30u,60u,0x3D088889u,0x3C888888u};   // int 30, 60; float 1/30, 1/60
    unsigned live[4];memcpy(live,table,sizeof live);
    if(memcmp(live,original,sizeof live)) {gpu_profile_event("fps120_refused",live[1],live[3],app);return;}
    const int fps=120;const float seconds=1.0f/120.0f;
    memcpy(table+12,&seconds,sizeof seconds);memcpy(table+4,&fps,sizeof fps);
    gpu_profile_event("fps120_patched",app,uptime,0);
}
} // namespace

extern "C" void gpu_profile_start(HMODULE self) {
    wchar_t path[MAX_PATH];
    if(!GetModuleFileNameW(self,path,MAX_PATH))return;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return;*slash=0;
    wchar_t config[MAX_PATH];wcscpy_s(config,path);
    if(wcscat_s(config,L"\\AI_TacticsMod\\기능설정.ini"))return;
    if(!GetPrivateProfileIntW(L"groups",L"diagnostics",1,config))return;
    passes_on=GetPrivateProfileIntW(L"diagnostics",L"gpu_profile",0,config)!=0;
    pacing_on=GetPrivateProfileIntW(L"diagnostics",L"frame_profile",0,config)!=0;
    uncap_on=GetPrivateProfileIntW(L"diagnostics",L"present_uncap",0,config)!=0;
    const bool fps120=GetPrivateProfileIntW(L"diagnostics",L"fps_120",0,config)!=0;
    if(fps120)patch_fps_table();   // first: before the game builds its engine settings from the table
    if(!passes_on && !pacing_on && !uncap_on)return;
    gpu_profile_event("gpu_profile_start",(passes_on?1u:0u)|(pacing_on?2u:0u)|(uncap_on?4u:0u)|(fps120?8u:0u),gpu_profile_game_data(2)?1u:0u,0);
    if(wcscat_s(path,L"\\AI_TacticsMod\\data"))return;
    CreateDirectoryW(path,nullptr);
    if(wcscat_s(path,L"\\diagnostics"))return;
    CreateDirectoryW(path,nullptr);
    QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&origin);window_start=origin;
    const size_t base=wcslen(path);
    if(passes_on && !wcscat_s(path,L"\\gpu_passes.csv")) {
        file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        LARGE_INTEGER size{};
        if(file!=INVALID_HANDLE_VALUE && GetFileSizeEx(file,&size) && !size.QuadPart) {
            static const char head[]="session_s,window_s,presents,fps,resolved,dropped,focus,minimized,pass,occ,rt,ds,avg_ms,max_ms,share_pct,avg_draws,avg_dispatch,avg_pos\r\n";
            write_text(head,sizeof head-1);
        }
        if(file==INVALID_HANDLE_VALUE)passes_on=false;
    }
    path[base]=0;
    if(pacing_on && !wcscat_s(path,L"\\frame_pacing.csv")) {
        pacing.file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(pacing.file==INVALID_HANDLE_VALUE)pacing_on=false;
        else {
            // The header is repeated at every session: local205 rows in the same file have other columns.
            pacing_note("session local206 uncap=%d fps_120=%d gpu_profile=%d device_variable=%d",uncap_on?1:0,fps120?1:0,passes_on?1:0,gpu_profile_game_data(2)?1:0);
            static const char head[]="session_s,window_s,presents,fps,p50_ms,p95_ms,p99_ms,max_ms,present_ms,wrapper_ms,out_ms,cpu_frame_ms,idle_out_ms,cpu_thread_ms,"
                "updates_per_s,renders_per_s,imm_draws,other_draws,exec_lists,game_sync,used_sync,sync_changes,timer_ms,focus,minimized,other_thread_presents,foreign_presents,no_end\r\n";
            DWORD written;WriteFile(pacing.file,head,sizeof head-1,&written,nullptr);
        }
    }
    if(!passes_on && !pacing_on && !uncap_on)return;
    // Layout owns the shared Present hook; this unit writes no DXGI slot (local206). Subscription waits for the game's swap chain.
    const int watch=gpu_profile_layout_watch(&on_layout_built);
    gpu_profile_event("gpu_profile_layout_watch",watch>0?1u:0u,static_cast<unsigned>(watch),0);
    if(watch<=0)note("layout_watch_failed");
}
