// SAN14_Layout.dll owner of the shared Present hook (IDXGISwapChain vt[8], IDXGISwapChain1 vt[22]) and of the
// game-window subclass (user 2026-11-05; moved from SAN14_Convenience.dll). Other modules subscribe.
// Present slots follow the C-2 chain policy: whatever module already hooked the slot (for example the fog
// mod) stays in the chain as `next`. The DLL is pinned, so slots are never restored at unload.
namespace layout {
struct Sub {void *fn;void *ctx;};
static const unsigned kMaxSubs=8;
static Sub overlay_subs[kMaxSubs],wnd_subs[kMaxSubs];
static volatile LONG overlay_count,wnd_count;
static SRWLOCK sub_lock=SRWLOCK_INIT;
static int subscribe(Sub *subs,volatile LONG *count,void *fn,void *ctx) {
    if(!fn)return -1;
    int result;
    AcquireSRWLockExclusive(&sub_lock);
    LONG n=*count;result=-1;
    for(LONG i=0;i<n;++i)if(subs[i].fn==fn && subs[i].ctx==ctx)result=static_cast<int>(i);
    if(result<0) {
        if(n>=static_cast<LONG>(kMaxSubs))result=-3;
        else {
            subs[n].fn=fn;subs[n].ctx=ctx;MemoryBarrier();   // entry is complete before it is counted
            InterlockedExchange(count,n+1);result=static_cast<int>(n);
        }
    }
    ReleaseSRWLockExclusive(&sub_lock);
    return result;
}
// ---- Present ----
static const UINT kPresentTest=0x00000001u;   // DXGI_PRESENT_TEST
static void *volatile present_next;
static void *volatile present1_next;
static void run_overlays(void *chain,UINT flags) {
    if(flags&kPresentTest)return;
    LONG n=InterlockedCompareExchange(&overlay_count,0,0);
    for(LONG i=0;i<n;++i)reinterpret_cast<void (*)(void *,void *)>(overlay_subs[i].fn)(chain,overlay_subs[i].ctx);
}
static HRESULT STDMETHODCALLTYPE present_hook(void *chain,UINT sync,UINT flags) {
    COST_SCOPE("layout_present_hook");
    run_overlays(chain,flags);
    return reinterpret_cast<HRESULT (STDMETHODCALLTYPE *)(void *,UINT,UINT)>(present_next)(chain,sync,flags);
}
static HRESULT STDMETHODCALLTYPE present1_hook(void *chain,UINT sync,UINT flags,const void *parameters) {
    COST_SCOPE("layout_present1_hook");
    run_overlays(chain,flags);
    return reinterpret_cast<HRESULT (STDMETHODCALLTYPE *)(void *,UINT,UINT,const void *)>(present1_next)(chain,sync,flags,parameters);
}
static void install_present_tables(void **table,void **table1) {
    // next is written by the manager before the slot can reach the hook.
    hook_abs_slot("layout_present",&table[8],reinterpret_cast<void *>(&present_hook),const_cast<void **>(&present_next),LAYOUT_CHAIN_OK,L"dxgi.dll");
    if(table1)hook_abs_slot("layout_present1",&table1[22],reinterpret_cast<void *>(&present1_hook),const_cast<void **>(&present1_next),LAYOUT_CHAIN_OK,L"dxgi.dll");
}
static volatile LONG present_attempted;
#ifdef LAYOUT_FIXTURE
static void **fixture_table,**fixture_table1;
#endif
// Same probe as the former Convenience hook: a hidden temporary device exposes the swap-chain vtables.
static void install_present_once() {
    if(InterlockedCompareExchange(&present_attempted,1,0)!=0)return;
#ifdef LAYOUT_FIXTURE
    if(fixture_table) {install_present_tables(fixture_table,fixture_table1);return;}
#endif
    HMODULE library=GetModuleHandleW(L"d3d11.dll");if(!library)return;
    auto create=reinterpret_cast<decltype(&D3D11CreateDeviceAndSwapChain)>(GetProcAddress(library,"D3D11CreateDeviceAndSwapChain"));if(!create)return;
    HWND probe=CreateWindowExW(0,L"STATIC",L"san14-layout-probe",WS_POPUP,0,0,64,64,nullptr,nullptr,nullptr,nullptr);if(!probe)return;
    DXGI_SWAP_CHAIN_DESC desc={};desc.BufferCount=1;desc.BufferDesc.Width=64;desc.BufferDesc.Height=64;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow=probe;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain *chain=nullptr;ID3D11Device *temporary=nullptr;ID3D11DeviceContext *temporary_context=nullptr;
    HRESULT result=create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&chain,&temporary,nullptr,&temporary_context);
    if(SUCCEEDED(result) && chain) {
        auto table=*reinterpret_cast<void ***>(chain);
        IDXGISwapChain1 *extended=nullptr;
        void **extra=nullptr;
        if(SUCCEEDED(chain->QueryInterface(__uuidof(IDXGISwapChain1),reinterpret_cast<void **>(&extended))) && extended) {
            extra=*reinterpret_cast<void ***>(extended);
        }
        install_present_tables(table,extra);
        if(extended)extended->Release();
    } else emit("layout_present_probe_failed",static_cast<unsigned>(result),0,0);
    if(temporary_context)temporary_context->Release();if(temporary)temporary->Release();if(chain)chain->Release();DestroyWindow(probe);
}
static int api_overlay_subscribe(void (*draw)(void *,void *),void *ctx) {
    int index=subscribe(overlay_subs,&overlay_count,reinterpret_cast<void *>(draw),ctx);
    if(index>=0)install_present_once();
    return index;
}
// ---- window procedure ----
static HWND bound_window;
static WNDPROC previous_proc;
static LRESULT CALLBACK layout_wndproc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
    COST_SCOPE("layout_wndproc");
    LONG n=InterlockedCompareExchange(&wnd_count,0,0);
    for(LONG i=0;i<n;++i)
        if(reinterpret_cast<int (*)(void *,unsigned,uintptr_t,intptr_t,void *)>(wnd_subs[i].fn)(hwnd,message,wparam,static_cast<intptr_t>(lparam),wnd_subs[i].ctx))return 0;
    WNDPROC previous=reinterpret_cast<WNDPROC>(InterlockedCompareExchangePointer(reinterpret_cast<void *volatile *>(&previous_proc),nullptr,nullptr));
    return previous?CallWindowProcW(previous,hwnd,message,wparam,lparam):DefWindowProcW(hwnd,message,wparam,lparam);
}
static int api_wndproc_subscribe(int (*proc)(void *,unsigned,uintptr_t,intptr_t,void *),void *ctx) {
    return subscribe(wnd_subs,&wnd_count,reinterpret_cast<void *>(proc),ctx);
}
// One subclass for the whole process; the previous procedure is always called last.
static int api_wndproc_bind(void *handle) {
    HWND hwnd=static_cast<HWND>(handle);
    if(!hwnd)return 0;
    if(bound_window==hwnd && previous_proc)return 1;
    if(bound_window && IsWindow(bound_window))return 0;
    auto previous=reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd,GWLP_WNDPROC));
    if(!previous || previous==&layout_wndproc)return 0;
    InterlockedExchangePointer(reinterpret_cast<void *volatile *>(&previous_proc),reinterpret_cast<void *>(previous));
    SetLastError(0);
    LONG_PTR result=SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&layout_wndproc));
    if(!result && GetLastError()) {previous_proc=nullptr;return 0;}
    InterlockedExchangePointer(reinterpret_cast<void *volatile *>(&previous_proc),reinterpret_cast<void *>(result));
    bound_window=hwnd;
    emit("layout_wndproc_bound",1,0,0);
    return 1;
}
}
