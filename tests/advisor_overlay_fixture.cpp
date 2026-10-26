#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <algorithm>
#include <vector>
#include <cstdio>
#include <cstdlib>
namespace unified_overlay {static int buffer_width=1920,buffer_height=1080;}
#include "../src/native_advisor_overlay.inl"
static unsigned checks;
static void check(bool value,const char *why){++checks;if(!value){fprintf(stderr,"%s\n",why);exit(1);}}
static void bitmap(const char *path,const std::vector<unsigned> &p,int w,int h) {
    BITMAPFILEHEADER file{};file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+w*h*4;
    BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=w;info.biHeight=-h;info.biPlanes=1;info.biBitCount=32;
    FILE *f=nullptr;check(fopen_s(&f,path,"wb")==0,"renderer preview file opened");fwrite(&file,1,sizeof(file),f);fwrite(&info,1,sizeof(info),f);fwrite(p.data(),4,p.size(),f);fclose(f);
}
int main() {
    using namespace briefing_overlay;
    ID3D11Device *d=nullptr;ID3D11DeviceContext *c=nullptr;D3D_FEATURE_LEVEL level;
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c)),"offscreen WARP device, no game or window");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=1920;desc.Height=1080;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D *back=nullptr;check(SUCCEEDED(d->CreateTexture2D(&desc,nullptr,&back)),"synthetic back buffer created");
    std::vector<unsigned> pixels(1920*1080,0xff63877cu);
    // User 2026-10-26: the real backbuffer can have useful RGB and zero alpha.
    for(unsigned y=10;y<98;++y)for(unsigned x=10;x<98;++x)pixels[y*1920+x]=0x00d5ba84u;
    // A clearly synthetic portrait stand-in. The shipped mod captures the
    // original adviser face node; this fixture never loads or runs the game.
    for(unsigned y=24;y<62;++y)for(unsigned x=36;x<72;++x)pixels[y*1920+x]=0x0034485fu;
    for(unsigned y=100;y<188;++y)for(unsigned x=10;x<98;++x)pixels[y*1920+x]=0;
    c->UpdateSubresource(back,0,nullptr,pixels.data(),1920*4,0);
    ID3D11RenderTargetView *original_target=nullptr;check(SUCCEEDED(d->CreateRenderTargetView(back,nullptr,&original_target)),"fixture render target");c->OMSetRenderTargets(1,&original_target,nullptr);
    D3D11_VIEWPORT original_viewport{5,7,733,511,0,1};c->RSSetViewports(1,&original_viewport);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
    Model m;m.adviser=L"제갈량";m.title=L"뜻밖의 수확  1 / 4";m.body=L"발견한 금 합계 1,000\n\n3건의 소식을 모았습니다.\n\n• 유비  ·  200\n\n• 관우  ·  300\n\n• 조운  ·  500\n\n원문 보기에서 무장별 보고를 확인할 수 있습니다.";m.unread=7;m.open=true;m.generation=1;
    publish(m,true);opened_at=GetTickCount64()-300;
    request_portrait({-146,708,484,512},1);draw(d,c,back,desc);
    check(available() && !captured(1),"invalid or unavailable original portrait cannot block the entire inbox renderer");
    check(avatar_view!=nullptr && panel_view!=nullptr,"visible fallback avatar and bubble exist before native portrait capture");
    portrait_attempt_at=0;request_portrait({10,100,88,88},1);draw(d,c,back,desc);
    check(!captured(1) && avatar_view && flag(&portrait_requested),"black source is rejected and fallback remains visible for retry");
    portrait_attempt_at=0;request_portrait({10,10,88,88},1);
    draw(d,c,back,desc);check(available() && captured(1),"original source texture captured and overlay draw completed");
    auto *good_face=portrait_view;portrait_attempt_at=0;request_portrait({10,100,88,88},2);draw(d,c,back,desc);
    check(!captured(2) && portrait_view==good_face,"bad replacement preserves the last validated portrait");
    D3D11_PRIMITIVE_TOPOLOGY topology;c->IAGetPrimitiveTopology(&topology);check(topology==D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,"immediate primitive topology preserved");
    UINT count=1;D3D11_VIEWPORT viewport{};c->RSGetViewports(&count,&viewport);check(count==1 && viewport.Width==733 && viewport.TopLeftX==5,"immediate viewport preserved");
    ID3D11RenderTargetView *after=nullptr;c->OMGetRenderTargets(1,&after,nullptr);check(after==original_target,"immediate render target preserved");if(after)after->Release();
    D3D11_TEXTURE2D_DESC staging=desc;staging.BindFlags=0;staging.Usage=D3D11_USAGE_STAGING;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D *readback=nullptr;check(SUCCEEDED(d->CreateTexture2D(&staging,nullptr,&readback)),"readback buffer created");c->CopyResource(readback,back);
    D3D11_MAPPED_SUBRESOURCE mapped{};check(SUCCEEDED(c->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"overlay pixels mapped");
    for(unsigned y=0;y<1080;++y)memcpy(pixels.data()+y*1920,static_cast<unsigned char *>(mapped.pData)+y*mapped.RowPitch,1920*4);c->Unmap(readback,0);
    check(pixels[956*1920+30]==0xff63877cu,"circular portrait corner preserves the underlying map");
    check((pixels[1000*1920+74]&0xffffffu)==0x34485fu,"portrait RGB stays visible even when captured source alpha is zero");
    check(pixels[458*1920+136]==0xff63877cu,"rounded bubble corner preserves the underlying map");
    check(pixels[500*1920+150]!=0xff63877cu,"bubble body is composited");
    bitmap("advisor_briefing_preview.bmp",pixels,1920,1080);
    check(widget(74,1000,false)==1 && widget(30,956,false)==0,"only the circular face consumes portrait clicks");
    check(widget(610,485,true)==2 && widget(174,994,true)==3 && widget(294,994,true)==4,"bubble close and page hit boxes");
    check(widget(400,994,true)==5 && widget(545,994,true)==6 && widget(400,800,true)==9,"deletion and body input stay within bubble");
    check(input(nullptr,WM_KEYUP,VK_ESCAPE,0) && flag(&pending)==2,"Escape closes the bubble without passing through to game");
    m.collecting=true;++m.generation;publish(m,true);check(!input(nullptr,WM_KEYUP,VK_ESCAPE,0),"progress mode does not open the inbox");
    forget();check(!available() && !is_visible(),"world teardown clears readiness and portrait capture");
    release_device();readback->Release();original_target->Release();back->Release();c->Release();d->Release();
    printf("{\"status\":\"advisor_overlay_warp_fixture_passed\",\"checks\":%u,\"game_launched\":false,\"live_game\":false}\n",checks);return 0;
}
