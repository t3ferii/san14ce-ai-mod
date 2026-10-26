#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <cstdio>
#include "../src/native_unified_overlay.inl"
static unsigned checks;
static void check(bool ok,const char *label){++checks;if(!ok){fprintf(stderr,"%s\n",label);ExitProcess(1);}}
int main() {
    using namespace unified_overlay;
    Bounds full=bounds(1920,1080);check(full.x==218 && full.y==156 && full.width==164 && full.height==42,"native UI anchor at full HD");
    Bounds letter=bounds(1280,800);check(letter.x==145 && letter.y==144 && letter.width==109 && letter.height==28,"windowed letterboxing preserves native UI position");
    check(!contains({},0,0) && !client_hit(full,100,100,0,0,1920,1080),"invalid geometry cannot consume input");
    check(client_hit(full,220,158,1920,1080,1920,1080),"client-to-backbuffer hit mapping");
    check(!contains(full,full.x+full.width,full.y),"right edge belongs to the game");
    check(eligible(true,true,true,false) && !eligible(true,true,true,true) && !eligible(true,true,false,false),"native map and popup gate");
    Pointer pointer;bool fire=false;
    check(!pointer.down(true,false,1) && !pointer.up(true,true,1,fire) && !fire,"outside clicks pass through");
    check(pointer.down(true,true,1) && pointer.up(true,true,1,fire) && fire,"inside click activates once on release");
    check(!pointer.up(true,true,1,fire) && !fire,"duplicate releases do not reopen the dialog");
    pointer.down(true,true,1);check(pointer.up(true,false,1,fire) && !fire,"dragging off the button cancels");
    pointer.down(true,true,1);check(pointer.up(true,true,2,fire) && !fire,"state transitions cancel a pending click");
    HWND target=CreateWindowExW(0,L"STATIC",L"personnel-overlay-fixture",WS_OVERLAPPEDWINDOW,0,0,1280,830,nullptr,nullptr,nullptr,nullptr);
    check(target!=nullptr,"hidden fixture window");
    DXGI_SWAP_CHAIN_DESC desc={};desc.BufferCount=1;desc.BufferDesc.Width=1280;desc.BufferDesc.Height=800;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow=target;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    IDXGISwapChain *chain=nullptr;ID3D11Device *test_device=nullptr;ID3D11DeviceContext *test_context=nullptr;
    HRESULT result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&chain,&test_device,nullptr,&test_context);
    check(SUCCEEDED(result) && chain,"real D3D11 fixture device");
    install();check(original_present && (*reinterpret_cast<void ***>(chain))[8]==reinterpret_cast<void *>(&present),"reference-style shared Present hook is actually installed");
    ID3D11Texture2D *back=nullptr;check(SUCCEEDED(chain->GetBuffer(0,__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&back))),"fixture backbuffer");
    ID3D11RenderTargetView *view=nullptr;check(SUCCEEDED(test_device->CreateRenderTargetView(back,nullptr,&view)),"fixture render target");
    const float clear[]={16.0f/255,32.0f/255,48.0f/255,1};test_context->ClearRenderTargetView(view,clear);
    show(true);draw(chain,0);check(read_flag(&ready)==1 && texture && window==target,"production renderer binds and draws into the game-like backbuffer");
    D3D11_TEXTURE2D_DESC staging_desc={};back->GetDesc(&staging_desc);staging_desc.Usage=D3D11_USAGE_STAGING;
    staging_desc.BindFlags=0;staging_desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;staging_desc.MiscFlags=0;
    ID3D11Texture2D *staging=nullptr;check(SUCCEEDED(test_device->CreateTexture2D(&staging_desc,nullptr,&staging)),"readback texture");
    test_context->CopyResource(staging,back);D3D11_MAPPED_SUBRESOURCE mapped={};check(SUCCEEDED(test_context->Map(staging,0,D3D11_MAP_READ,0,&mapped)),"read rendered pixels");
    auto bytes=static_cast<const unsigned char *>(mapped.pData);
    unsigned outside=*reinterpret_cast<const unsigned *>(bytes);
    unsigned inside=*reinterpret_cast<const unsigned *>(bytes+(letter.y+3)*mapped.RowPitch+(letter.x+3)*4);
    check(outside==0xff302010u && inside!=outside,"only the small button rectangle changes; existing game pixels are preserved");test_context->Unmap(staging,0);
    auto cached=texture;draw(chain,0);check(texture==cached,"unchanged frames reuse the cached small texture");
    RECT client={};GetClientRect(target,&client);int x=(letter.x+letter.width/2)*client.right/1280,y=(letter.y+letter.height/2)*client.bottom/800;
    SendMessageW(target,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));SendMessageW(target,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    check(take_click() && !take_click(),"real window messages enqueue one UI-thread request");
    show(false);SendMessageW(target,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));SendMessageW(target,WM_LBUTTONUP,0,MAKELPARAM(x,y));
    check(!take_click(),"hidden overlay cannot open a dialog");
    staging->Release();view->Release();back->Release();test_context->Release();test_device->Release();chain->Release();DestroyWindow(target);
    printf("{\"status\":\"unified_overlay_fixture_passed\",\"checks\":%u,\"real_d3d11_pixel_readback\":true,\"gameplay_verified\":false}\n",checks);
}
