/* GPU pass-timing diagnostic (SAN14_Convenience.dll, [diagnostics] gpu_profile=1).
 * Observation only. The game's D3D11/DXGI calls are forwarded unchanged; inside
 * them, on the game's own render thread, timestamp queries are issued at every
 * render-target change and read back a few frames later (DONOTFLUSH).
 * Hooks are vtable slots of the exact classes the game uses:
 *   DXGI factory CreateSwapChain/CreateSwapChainForHwnd (learned from a throwaway
 *   factory at start) -> the created swap chain's Present -> at the first Present,
 *   that device's immediate context (OM target changes, draws, dispatches).
 * A slot is patched for one vtable only; a different class is logged and left alone,
 * so every saved original belongs to the vtable it was taken from.
 * Output: AI_TacticsMod\data\diagnostics\gpu_passes.csv, one block per 5 s. */
#define CINTERFACE
#define COBJMACROS
#define D3D11_NO_HELPERS   // C++ helper structs call C++ interface methods
#include "gpu_profile.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

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

void write_text(const char *s,size_t n) {
    if(file==INVALID_HANDLE_VALUE || !n)return;
    DWORD written;WriteFile(file,s,static_cast<DWORD>(n),&written,nullptr);
}
void note(const char *what,const void *a=nullptr,const void *b=nullptr) {
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

Patch p_create,p_create_hwnd,p_present,p_set,p_set_uav,p_draw[7],p_dispatch[2];

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
    if(c!=context)return;
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
    Slot *f=current;   // slots are static; a racing frame edge can only misattribute one count
    if(c!=context || !f || !f->n)return;
    Seg &s=f->seg[f->n-1];
    if(dispatch)++s.dispatches;else ++s.draws;
}

void STDMETHODCALLTYPE set_targets(ID3D11DeviceContext *c,UINT n,ID3D11RenderTargetView *const *rtv,ID3D11DepthStencilView *dsv) {
    boundary(c,n,rtv,dsv,0);
    REAL(p_set,OMSetRenderTargets,ID3D11DeviceContextVtbl)(c,n,rtv,dsv);
}
void STDMETHODCALLTYPE set_targets_uav(ID3D11DeviceContext *c,UINT n,ID3D11RenderTargetView *const *rtv,ID3D11DepthStencilView *dsv,
                                        UINT start,UINT uavs,ID3D11UnorderedAccessView *const *views,const UINT *counts) {
    if(n!=D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL)boundary(c,n,rtv,dsv,uavs!=D3D11_KEEP_UNORDERED_ACCESS_VIEWS && uavs?1u:0u);
    REAL(p_set_uav,OMSetRenderTargetsAndUnorderedAccessViews,ID3D11DeviceContextVtbl)(c,n,rtv,dsv,start,uavs,views,counts);
}
void STDMETHODCALLTYPE draw_indexed(ID3D11DeviceContext *c,UINT a,UINT b,INT d) {
    count(c,false);REAL(p_draw[0],DrawIndexed,ID3D11DeviceContextVtbl)(c,a,b,d);
}
void STDMETHODCALLTYPE draw(ID3D11DeviceContext *c,UINT a,UINT b) {
    count(c,false);REAL(p_draw[1],Draw,ID3D11DeviceContextVtbl)(c,a,b);
}
void STDMETHODCALLTYPE draw_indexed_instanced(ID3D11DeviceContext *c,UINT a,UINT b,UINT d,INT e,UINT f) {
    count(c,false);REAL(p_draw[2],DrawIndexedInstanced,ID3D11DeviceContextVtbl)(c,a,b,d,e,f);
}
void STDMETHODCALLTYPE draw_instanced(ID3D11DeviceContext *c,UINT a,UINT b,UINT d,UINT e) {
    count(c,false);REAL(p_draw[3],DrawInstanced,ID3D11DeviceContextVtbl)(c,a,b,d,e);
}
void STDMETHODCALLTYPE draw_auto(ID3D11DeviceContext *c) {
    count(c,false);REAL(p_draw[4],DrawAuto,ID3D11DeviceContextVtbl)(c);
}
void STDMETHODCALLTYPE draw_indexed_indirect(ID3D11DeviceContext *c,ID3D11Buffer *b,UINT o) {
    count(c,false);REAL(p_draw[5],DrawIndexedInstancedIndirect,ID3D11DeviceContextVtbl)(c,b,o);
}
void STDMETHODCALLTYPE draw_indirect(ID3D11DeviceContext *c,ID3D11Buffer *b,UINT o) {
    count(c,false);REAL(p_draw[6],DrawInstancedIndirect,ID3D11DeviceContextVtbl)(c,b,o);
}
void STDMETHODCALLTYPE dispatch(ID3D11DeviceContext *c,UINT x,UINT y,UINT z) {
    count(c,true);REAL(p_dispatch[0],Dispatch,ID3D11DeviceContextVtbl)(c,x,y,z);
}
void STDMETHODCALLTYPE dispatch_indirect(ID3D11DeviceContext *c,ID3D11Buffer *b,UINT o) {
    count(c,true);REAL(p_dispatch[1],DispatchIndirect,ID3D11DeviceContextVtbl)(c,b,o);
}

void patch_context(ID3D11DeviceContext *c) {
#define SLOT(member) offsetof(ID3D11DeviceContextVtbl,member)
    bool ok=patch(p_set,c,SLOT(OMSetRenderTargets),reinterpret_cast<void *>(set_targets),"context_class_skipped");
    if(!ok)return;
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
    if(d!=query_device) {
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
    return !broken;
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
HRESULT STDMETHODCALLTYPE present(IDXGISwapChain *sc,UINT sync,UINT flags) {
    const bool real_frame=!(flags & DXGI_PRESENT_TEST);
    if(real_frame)frame_edge(sc,true);
    const HRESULT hr=REAL(p_present,Present,IDXGISwapChainVtbl)(sc,sync,flags);
    if(real_frame)frame_edge(sc,false);
    return hr;
}
void patch_chain(IDXGISwapChain *sc) {
    AcquireSRWLockExclusive(&lock);
    if(patch(p_present,sc,offsetof(IDXGISwapChainVtbl,Present),reinterpret_cast<void *>(present),"chain_class_skipped"))note("present_hooked",sc);
    ReleaseSRWLockExclusive(&lock);
}
HRESULT STDMETHODCALLTYPE create_chain(IDXGIFactory *f,IUnknown *device,DXGI_SWAP_CHAIN_DESC *desc,IDXGISwapChain **out) {
    const HRESULT hr=REAL(p_create,CreateSwapChain,IDXGIFactoryVtbl)(f,device,desc,out);
    if(SUCCEEDED(hr) && out && *out)patch_chain(*out);
    return hr;
}
HRESULT STDMETHODCALLTYPE create_chain_hwnd(IDXGIFactory2 *f,IUnknown *device,HWND hwnd,const DXGI_SWAP_CHAIN_DESC1 *desc,
                                            const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *full,IDXGIOutput *output,IDXGISwapChain1 **out) {
    const HRESULT hr=REAL(p_create_hwnd,CreateSwapChainForHwnd,IDXGIFactory2Vtbl)(f,device,hwnd,desc,full,output,out);
    if(SUCCEEDED(hr) && out && *out)patch_chain(reinterpret_cast<IDXGISwapChain *>(*out));
    return hr;
}
} // namespace

extern "C" void gpu_profile_start(HMODULE self) {
    wchar_t path[MAX_PATH];
    if(!GetModuleFileNameW(self,path,MAX_PATH))return;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return;*slash=0;
    wchar_t config[MAX_PATH];wcscpy_s(config,path);
    if(wcscat_s(config,L"\\AI_TacticsMod\\기능설정.ini"))return;
    if(!GetPrivateProfileIntW(L"groups",L"diagnostics",1,config) || !GetPrivateProfileIntW(L"diagnostics",L"gpu_profile",0,config))return;
    if(wcscat_s(path,L"\\AI_TacticsMod\\data"))return;
    CreateDirectoryW(path,nullptr);
    if(wcscat_s(path,L"\\diagnostics"))return;
    CreateDirectoryW(path,nullptr);
    if(wcscat_s(path,L"\\gpu_passes.csv"))return;
    file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&origin);window_start=origin;
    LARGE_INTEGER size{};
    if(GetFileSizeEx(file,&size) && !size.QuadPart) {
        static const char head[]="session_s,window_s,presents,fps,resolved,dropped,focus,minimized,pass,occ,rt,ds,avg_ms,max_ms,share_pct,avg_draws,avg_dispatch,avg_pos\r\n";
        write_text(head,sizeof head-1);
    }
    wchar_t dxgi_path[MAX_PATH];
    UINT n=GetSystemDirectoryW(dxgi_path,MAX_PATH);
    if(!n || n>=MAX_PATH || wcscat_s(dxgi_path,L"\\dxgi.dll")) {note("dxgi_path_failed");return;}
    HMODULE dxgi=LoadLibraryExW(dxgi_path,nullptr,0);   // kept loaded: the game imports it too
    auto make=dxgi?reinterpret_cast<HRESULT(WINAPI *)(REFIID,void **)>(GetProcAddress(dxgi,"CreateDXGIFactory")):nullptr;
    IDXGIFactory *factory=nullptr;
    if(!make || FAILED(make(IID_IDXGIFactory,reinterpret_cast<void **>(&factory))) || !factory) {note("factory_failed");return;}
    AcquireSRWLockExclusive(&lock);
    patch(p_create,factory,offsetof(IDXGIFactoryVtbl,CreateSwapChain),reinterpret_cast<void *>(create_chain),"factory_slot");
    IDXGIFactory2 *factory2=nullptr;
    if(SUCCEEDED(IDXGIFactory_QueryInterface(factory,IID_IDXGIFactory2,reinterpret_cast<void **>(&factory2))) && factory2) {
        patch(p_create_hwnd,factory2,offsetof(IDXGIFactory2Vtbl,CreateSwapChainForHwnd),reinterpret_cast<void *>(create_chain_hwnd),"factory2_slot");
        IDXGIFactory2_Release(factory2);
    }
    note("installed",p_create.vtable,p_create_hwnd.vtable);
    ReleaseSRWLockExclusive(&lock);
    IDXGIFactory_Release(factory);
}
