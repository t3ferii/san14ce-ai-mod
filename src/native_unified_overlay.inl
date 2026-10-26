// A small in-game D3D11 entry button; personnel dialogs still use the game UI.
// Reference: martial tournament v30's Present/cached-texture/input separation.
#include <d3d11.h>
#include <dxgi1_2.h>
#include "unified_overlay_policy.h"
namespace unified_overlay {
using Present=HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain *,UINT,UINT);
using Present1=HRESULT(STDMETHODCALLTYPE *)(IDXGISwapChain1 *,UINT,UINT,const DXGI_PRESENT_PARAMETERS *);
static Present original_present;
static Present1 original_present1;
static LONG requested,visible,ready,hover,dirty=1,generation;
static bool attempted;
static SRWLOCK geometry_lock=SRWLOCK_INIT,draw_lock=SRWLOCK_INIT;
static Bounds rectangle;
static int buffer_width,buffer_height;
static HWND window;
static WNDPROC original_window;
static Pointer mouse;
static ID3D11Device *device;
static ID3D11DeviceContext *context;
static ID3D11Texture2D *texture;
static DXGI_FORMAT texture_format=DXGI_FORMAT_UNKNOWN;
static unsigned texture_width,texture_height;
static std::vector<unsigned> pixels;
static LONG read_flag(volatile LONG *value){return InterlockedCompareExchange(value,0,0);}
static void show(bool value) {
    LONG next=value?1:0;
    if(InterlockedExchange(&visible,next)!=next)InterlockedIncrement(&generation);
    if(!value)InterlockedExchange(&ready,0);
}
static bool take_click(){return InterlockedExchange(&requested,0)!=0;}
}
namespace officer_panel {
static bool input(HWND hwnd,UINT message,LPARAM lparam);
static void draw(ID3D11Device *device,ID3D11DeviceContext *context,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc,bool rgba);
static bool is_visible();
}
#include "native_advisor_overlay.inl"
namespace unified_overlay {
static LRESULT CALLBACK window_proc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
    if(briefing_overlay::input(hwnd,message,wparam,lparam))return 0;
    if(officer_panel::input(hwnd,message,lparam))return 0;
    bool active=read_flag(&visible) && read_flag(&ready),hit=false;
    if(active && (message==WM_MOUSEMOVE || message==WM_LBUTTONDOWN || message==WM_LBUTTONUP || message==WM_LBUTTONDBLCLK)) {
        RECT client={};GetClientRect(hwnd,&client);
        AcquireSRWLockShared(&geometry_lock);
        hit=client_hit(rectangle,static_cast<short>(LOWORD(lparam)),static_cast<short>(HIWORD(lparam)),
            client.right,client.bottom,buffer_width,buffer_height);
        ReleaseSRWLockShared(&geometry_lock);
    }
    if(message==WM_MOUSEMOVE) {
        if(InterlockedExchange(&hover,hit?1:0)!=(hit?1:0))InterlockedExchange(&dirty,1);
    } else if(message==WM_LBUTTONDOWN || message==WM_LBUTTONDBLCLK) {
        if(mouse.down(active,hit,static_cast<unsigned>(read_flag(&generation))))return 0;
    } else if(message==WM_LBUTTONUP) {
        bool fire=false;
        if(mouse.up(active,hit,static_cast<unsigned>(read_flag(&generation)),fire)) {
            if(fire)InterlockedExchange(&requested,1);
            return 0;
        }
    } else if(message==WM_KILLFOCUS || message==WM_CAPTURECHANGED || message==WM_SIZE || message==WM_DPICHANGED) {
        mouse.armed=false;InterlockedExchange(&hover,0);InterlockedExchange(&dirty,1);
    }
    WNDPROC previous=reinterpret_cast<WNDPROC>(InterlockedCompareExchangePointer(reinterpret_cast<void *volatile *>(&original_window),nullptr,nullptr));
    return previous?CallWindowProcW(previous,hwnd,message,wparam,lparam):DefWindowProcW(hwnd,message,wparam,lparam);
}
static bool bind(HWND hwnd) {
    if(window==hwnd && original_window)return true;
    if(window && IsWindow(window))return false;
    auto previous=reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd,GWLP_WNDPROC));
    if(!previous || previous==&window_proc)return false;
    InterlockedExchangePointer(reinterpret_cast<void *volatile *>(&original_window),reinterpret_cast<void *>(previous));
    SetLastError(0);LONG_PTR result=SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&window_proc));
    if(!result && GetLastError()){original_window=nullptr;return false;}
    InterlockedExchangePointer(reinterpret_cast<void *volatile *>(&original_window),reinterpret_cast<void *>(result));
    window=hwnd;return true;
}
static bool raster(unsigned width,unsigned height,bool highlighted) {
    BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=static_cast<LONG>(width);info.bmiHeader.biHeight=-static_cast<LONG>(height);
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return false;
    void *bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bitmap || !bits){if(bitmap)DeleteObject(bitmap);DeleteDC(dc);return false;}
    HGDIOBJ old_bitmap=SelectObject(dc,bitmap);
    auto data=static_cast<unsigned *>(bits);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        unsigned shade=highlighted?12u:0u;
        unsigned red=(std::min)(255u,241u+shade-y*18u/height);
        unsigned green=(std::min)(255u,231u+shade-y*24u/height);
        unsigned blue=(std::min)(255u,186u+shade-y*30u/height);
        data[y*width+x]=0xff000000u|(red<<16)|(green<<8)|blue;
    }
    HPEN pen=CreatePen(PS_SOLID,(std::max)(1,static_cast<int>(height/21)),RGB(113,72,25));
    HGDIOBJ old_pen=SelectObject(dc,pen),old_brush=SelectObject(dc,GetStockObject(NULL_BRUSH));
    Rectangle(dc,0,0,static_cast<int>(width),static_cast<int>(height));
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(64,43,28));
    HFONT font=CreateFontW(-(std::max)(12,static_cast<int>(height*0.52)),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
        HANGUL_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"궁서");
    HGDIOBJ old_font=SelectObject(dc,font);
    RECT text={2,0,static_cast<LONG>(width)-2,static_cast<LONG>(height)};
    DrawTextW(dc,L"통합 인사",-1,&text,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    pixels.assign(data,data+static_cast<size_t>(width)*height);
    SelectObject(dc,old_font);SelectObject(dc,old_brush);SelectObject(dc,old_pen);SelectObject(dc,old_bitmap);
    DeleteObject(font);DeleteObject(pen);DeleteObject(bitmap);DeleteDC(dc);return true;
}
static bool draw_locked(IDXGISwapChain *chain) {
    DXGI_SWAP_CHAIN_DESC swap={};
    if(FAILED(chain->GetDesc(&swap)) || !swap.OutputWindow)return false;
    DWORD pid=0;GetWindowThreadProcessId(swap.OutputWindow,&pid);if(pid!=GetCurrentProcessId())return false;
    ID3D11Texture2D *back=nullptr;
    if(FAILED(chain->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&back))))return false;
    D3D11_TEXTURE2D_DESC desc={};back->GetDesc(&desc);
    bool rgba=desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    bool bgra=desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB ||
        desc.Format==DXGI_FORMAT_B8G8R8X8_UNORM || desc.Format==DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
    if((!rgba && !bgra) || desc.SampleDesc.Count!=1){back->Release();return false;}
    ID3D11Device *current=nullptr;back->GetDevice(&current);
    if(!current){back->Release();return false;}
    if(device!=current) {
        if(texture){texture->Release();texture=nullptr;}
        if(context)context->Release();if(device)device->Release();device=current;
        device->GetImmediateContext(&context);
    } else current->Release();
    Bounds next=bounds(static_cast<int>(desc.Width),static_cast<int>(desc.Height));
    bool success=false;
    if(context && briefing_overlay::is_visible() && bind(swap.OutputWindow)) {
        AcquireSRWLockExclusive(&geometry_lock);buffer_width=static_cast<int>(desc.Width);buffer_height=static_cast<int>(desc.Height);ReleaseSRWLockExclusive(&geometry_lock);
        briefing_overlay::draw(device,context,back,desc);
    }
    // The roster filter panel shares the device, back buffer and window hook.
    if(context && officer_panel::is_visible() && bind(swap.OutputWindow)) {
        AcquireSRWLockExclusive(&geometry_lock);buffer_width=static_cast<int>(desc.Width);buffer_height=static_cast<int>(desc.Height);ReleaseSRWLockExclusive(&geometry_lock);
        officer_panel::draw(device,context,back,desc,rgba);
    }
    if(context && read_flag(&visible) && next.width>0 && next.height>0) {
        if(!texture || texture_width!=static_cast<unsigned>(next.width) || texture_height!=static_cast<unsigned>(next.height) || texture_format!=desc.Format) {
            if(texture){texture->Release();texture=nullptr;}
            D3D11_TEXTURE2D_DESC td={};td.Width=static_cast<unsigned>(next.width);td.Height=static_cast<unsigned>(next.height);
            td.MipLevels=1;td.ArraySize=1;td.Format=desc.Format;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;
            device->CreateTexture2D(&td,nullptr,&texture);
            texture_width=td.Width;texture_height=td.Height;texture_format=td.Format;InterlockedExchange(&dirty,1);
        }
        if(texture && (read_flag(&dirty)!=0)) {
            if(raster(texture_width,texture_height,read_flag(&hover)!=0)) {
                if(rgba)for(auto &pixel:pixels)pixel=(pixel&0xff00ff00u)|((pixel&255u)<<16)|((pixel>>16)&255u);
                context->UpdateSubresource(texture,0,nullptr,pixels.data(),texture_width*4,0);InterlockedExchange(&dirty,0);
            }
        }
        if(texture && !read_flag(&dirty) && bind(swap.OutputWindow)) {
            context->CopySubresourceRegion(back,0,static_cast<UINT>(next.x),static_cast<UINT>(next.y),0,texture,0,nullptr);
            AcquireSRWLockExclusive(&geometry_lock);rectangle=next;buffer_width=static_cast<int>(desc.Width);buffer_height=static_cast<int>(desc.Height);ReleaseSRWLockExclusive(&geometry_lock);
            success=true;
        }
    }
    back->Release();return success;
}
static void draw(IDXGISwapChain *chain,UINT flags) {
    if((flags&DXGI_PRESENT_TEST) || (!read_flag(&visible) && !officer_panel::is_visible() && !briefing_overlay::is_visible()))return;
    if(TryAcquireSRWLockExclusive(&draw_lock)) {
        InterlockedExchange(&ready,draw_locked(chain)?1:0);ReleaseSRWLockExclusive(&draw_lock);
    }
}
static HRESULT STDMETHODCALLTYPE present(IDXGISwapChain *chain,UINT sync,UINT flags) {
    draw(chain,flags);return original_present(chain,sync,flags);
}
static HRESULT STDMETHODCALLTYPE present1(IDXGISwapChain1 *chain,UINT sync,UINT flags,const DXGI_PRESENT_PARAMETERS *parameters) {
    draw(chain,flags);return original_present1(chain,sync,flags,parameters);
}
static bool install_slot(void **slot,void *hook,void **original) {
    DWORD protection=0;if(!VirtualProtect(slot,sizeof(void *),PAGE_READWRITE,&protection))return false;
    *original=*slot;InterlockedExchangePointer(slot,hook);DWORD ignored=0;VirtualProtect(slot,sizeof(void *),protection,&ignored);return true;
}
static void install() {
    if(attempted)return;attempted=true;
    HMODULE library=GetModuleHandleW(L"d3d11.dll");if(!library)return;
    auto create=reinterpret_cast<decltype(&D3D11CreateDeviceAndSwapChain)>(GetProcAddress(library,"D3D11CreateDeviceAndSwapChain"));if(!create)return;
    // Hidden probe only; no separate visible personnel window or message loop.
    HWND probe=CreateWindowExW(0,L"STATIC",L"san14-personnel-probe",WS_POPUP,0,0,64,64,nullptr,nullptr,nullptr,nullptr);if(!probe)return;
    DXGI_SWAP_CHAIN_DESC desc={};desc.BufferCount=1;desc.BufferDesc.Width=64;desc.BufferDesc.Height=64;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow=probe;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain *chain=nullptr;ID3D11Device *temporary=nullptr;ID3D11DeviceContext *temporary_context=nullptr;
    HRESULT result=create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&chain,&temporary,nullptr,&temporary_context);
    if(SUCCEEDED(result) && chain) {
        auto table=*reinterpret_cast<void ***>(chain);
        install_slot(table+8,reinterpret_cast<void *>(&present),reinterpret_cast<void **>(&original_present));
        IDXGISwapChain1 *extended=nullptr;
        if(SUCCEEDED(chain->QueryInterface(__uuidof(IDXGISwapChain1),reinterpret_cast<void **>(&extended)))) {
            auto extra=*reinterpret_cast<void ***>(extended);
            install_slot(extra+22,reinterpret_cast<void *>(&present1),reinterpret_cast<void **>(&original_present1));extended->Release();
        }
    }
    if(temporary_context)temporary_context->Release();if(temporary)temporary->Release();if(chain)chain->Release();DestroyWindow(probe);
}
}
#include "native_officer_panel.inl"
