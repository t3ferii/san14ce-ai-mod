// Shares the existing Present/WndProc owner. Never dereferences a game object.
#include "advisor_briefing.h"
#include "advisor_vs.h"
#include "advisor_ps.h"
namespace briefing_overlay {
using namespace advisor_briefing_core;
struct Model {std::wstring adviser=L"군사",title=L"열흘 보고",body;unsigned unread=0,page=0,pages=0,generation=0;bool collecting=false,open=false,detail=false;};
static SRWLOCK lock=SRWLOCK_INIT;
static Model model;
static Model draw_model;
static unsigned model_revision=0,draw_revision=~0u;
static volatile LONG visible,ready,pending,pending_generation,portrait_requested,portrait_revision,portrait_captured;
static Rect source;
static int source_revision,armed=0,scroll=0,max_scroll=0;
static unsigned arm_generation=0;
static ID3D11Device *owner;
static ID3D11DeviceContext *deferred;
static ID3D11VertexShader *vs;
static ID3D11PixelShader *ps;
static ID3D11SamplerState *sampler;
static ID3D11BlendState *blend;
static ID3D11RasterizerState *raster_state;
static ID3D11DepthStencilState *depth;
static ID3D11Buffer *constants;
static ID3D11Texture2D *portrait_texture;
static ID3D11ShaderResourceView *portrait_view;
static ID3D11Texture2D *panel_texture,*badge_texture;
static ID3D11ShaderResourceView *panel_view,*badge_view;
static ID3D11Texture2D *avatar_texture;
static ID3D11ShaderResourceView *avatar_view;
static unsigned painted_generation=~0u,painted_unread=~0u;
static int painted_scroll=-1;
static bool painted_open=false;
static bool painted_detail=false;
static ULONGLONG opened_at;
static ULONGLONG portrait_attempt_at;
static LONG flag(volatile LONG *v){return InterlockedCompareExchange(v,0,0);}
template<class T>static void drop(T *&p){if(p){p->Release();p=nullptr;}}
static void release_device() {
    InterlockedExchange(&ready,0);InterlockedExchange(&portrait_captured,0);
    drop(portrait_view);drop(portrait_texture);drop(panel_view);drop(panel_texture);drop(badge_view);drop(badge_texture);
    drop(avatar_view);drop(avatar_texture);
    drop(vs);drop(ps);drop(sampler);drop(blend);drop(raster_state);drop(depth);drop(constants);drop(deferred);drop(owner);
    painted_generation=~0u;painted_unread=~0u;painted_scroll=-1;portrait_attempt_at=0;
}
static bool is_visible(){return flag(&visible)!=0;}
static bool available(){return flag(&ready)!=0;}
static void publish(const Model &next,bool show) {
    AcquireSRWLockExclusive(&lock);
    if(next.generation!=model.generation || next.page!=model.page || next.detail!=model.detail){scroll=0;}
    if(next.open && !model.open)opened_at=GetTickCount64();
    if(next.generation!=model.generation || next.page!=model.page || next.open!=model.open || next.collecting!=model.collecting || next.adviser!=model.adviser || next.detail!=model.detail){model=next;++model_revision;}
    if(!show)armed=0;
    ReleaseSRWLockExclusive(&lock);
    InterlockedExchange(&visible,show?1:0);
    if(!show)InterlockedExchange(&pending,0);
}
static void request_portrait(Rect r,int revision) {
    AcquireSRWLockExclusive(&lock);source=r;source_revision=revision;ReleaseSRWLockExclusive(&lock);
    InterlockedExchange(&portrait_requested,1);
}
static bool captured(int revision){return flag(&portrait_captured) && flag(&portrait_revision)==revision;}
static void forget() {
    InterlockedExchange(&ready,0);
    InterlockedExchange(&portrait_captured,0);InterlockedExchange(&portrait_requested,0);InterlockedExchange(&pending,0);
    InterlockedExchange(&visible,0);armed=0;
}
static unsigned take(unsigned &generation) {
    // Both are published/consumed under the same lock, never split across cycles.
    AcquireSRWLockExclusive(&lock);unsigned result=static_cast<unsigned>(flag(&pending));
    generation=static_cast<unsigned>(flag(&pending_generation));InterlockedExchange(&pending,0);ReleaseSRWLockExclusive(&lock);return result;
}
static int widget(float x,float y,bool open) {
    float dx=x-74,dy=y-1000;if(dx*dx+dy*dy<=44*44)return 1;
    if(!open || !bubble.contains(x,y))return 0;
    if(Rect{596,474,34,34}.contains(x,y))return 2;
    if(Rect{156,978,90,36}.contains(x,y))return 3;
    if(Rect{262,978,90,36}.contains(x,y))return 4;
    if(Rect{368,978,100,36}.contains(x,y))return 5;
    if(Rect{494,978,130,36}.contains(x,y))return 6;
    if(Rect{500,934,124,36}.contains(x,y))return 7;
    return 9; // bubble body consumes input so the map cannot react underneath
}
static bool input(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
    if(message==WM_KILLFOCUS || message==WM_CAPTURECHANGED || message==WM_SIZE || message==WM_DPICHANGED){armed=0;return false;}
    if(!is_visible() || !available())return false;
    if(message!=WM_LBUTTONDOWN && message!=WM_LBUTTONUP && message!=WM_LBUTTONDBLCLK && message!=WM_RBUTTONDOWN && message!=WM_RBUTTONUP && message!=WM_MOUSEWHEEL && message!=WM_KEYDOWN && message!=WM_KEYUP)return false;
    AcquireSRWLockExclusive(&lock);
    if(model.collecting){ReleaseSRWLockExclusive(&lock);return false;}
    if(message==WM_KEYDOWN || message==WM_KEYUP) {
        bool handle=model.open && wparam==VK_ESCAPE;
        if(handle && message==WM_KEYUP){InterlockedExchange(&pending_generation,static_cast<LONG>(model.generation));InterlockedExchange(&pending,2);}
        ReleaseSRWLockExclusive(&lock);return handle;
    }
    RECT client{};GetClientRect(hwnd,&client);POINT p{static_cast<short>(LOWORD(lparam)),static_cast<short>(HIWORD(lparam))};
    if(message==WM_MOUSEWHEEL)ScreenToClient(hwnd,&p);
    float x=0,y=0;bool mapped=client_point(p.x,p.y,client.right,client.bottom,unified_overlay::buffer_width,unified_overlay::buffer_height,x,y);
    int hit=mapped?widget(x,y,model.open):0;
    if(message==WM_MOUSEWHEEL && hit==9) {
        scroll=(std::max)(0,(std::min)(max_scroll,scroll+(GET_WHEEL_DELTA_WPARAM(wparam)>0?-80:80)));
    } else if(message==WM_LBUTTONDOWN || message==WM_LBUTTONDBLCLK){armed=hit;arm_generation=model.generation;}
    else if(message==WM_LBUTTONUP) {
        bool fire=hit && hit==armed && arm_generation==model.generation;bool consumed=armed!=0;armed=0;
        if(fire && hit!=9){InterlockedExchange(&pending_generation,static_cast<LONG>(model.generation));InterlockedExchange(&pending,hit);}
        hit=hit || consumed;
    }
    ReleaseSRWLockExclusive(&lock);return hit!=0;
}
static bool initialize(ID3D11Device *d) {
    if(owner==d && deferred && vs && ps && sampler && blend && constants && raster_state && depth)return true;
    release_device();owner=d;owner->AddRef();
    if(FAILED(d->CreateDeferredContext(0,&deferred)) || FAILED(d->CreateVertexShader(advisor_vs,sizeof(advisor_vs),nullptr,&vs)) || FAILED(d->CreatePixelShader(advisor_ps,sizeof(advisor_ps),nullptr,&ps)))return false;
    D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
    D3D11_BLEND_DESC bd{};auto &rt=bd.RenderTarget[0];rt.BlendEnable=TRUE;rt.SrcBlend=D3D11_BLEND_ONE;rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOp=D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha=D3D11_BLEND_ONE;rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
    D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=FALSE;dd.StencilEnable=FALSE;
    D3D11_BUFFER_DESC cb{};cb.ByteWidth=16;cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    return SUCCEEDED(d->CreateSamplerState(&sd,&sampler)) && SUCCEEDED(d->CreateBlendState(&bd,&blend)) && SUCCEEDED(d->CreateRasterizerState(&rd,&raster_state)) && SUCCEEDED(d->CreateDepthStencilState(&dd,&depth)) && SUCCEEDED(d->CreateBuffer(&cb,nullptr,&constants));
}
static bool upload(ID3D11DeviceContext *c,ID3D11Texture2D *&t,ID3D11ShaderResourceView *&v,unsigned w,unsigned h,const unsigned *pixels) {
    if(!t){D3D11_TEXTURE2D_DESC td{};td.Width=w;td.Height=h;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(owner->CreateTexture2D(&td,nullptr,&t)) || FAILED(owner->CreateShaderResourceView(t,nullptr,&v)))return false;}
    c->UpdateSubresource(t,0,nullptr,pixels,w*4,0);return true;
}
static bool raster(ID3D11DeviceContext *c,const Model &m,int scroll_y,bool badge,bool avatar=false) {
    int w=avatar?88:badge?38:510,h=avatar?88:badge?38:574;BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return false;void *bits=nullptr;HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bm || !bits){if(bm)DeleteObject(bm);DeleteDC(dc);return false;}
    auto old=SelectObject(dc,bm);auto data=static_cast<unsigned *>(bits);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x)data[y*w+x]=badge?0xffc54f4b:0xff202a35;
    SetBkMode(dc,TRANSPARENT);HFONT font=CreateFontW(avatar?-42:badge?-18:-20,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,HANGUL_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"맑은 고딕");auto oldfont=SelectObject(dc,font);
    auto text=[&](const std::wstring &s,RECT r,COLORREF color,UINT format){SetTextColor(dc,color);DrawTextW(dc,s.c_str(),static_cast<int>(s.size()),&r,format|DT_NOPREFIX);};
    if(avatar)text(L"軍",{0,0,w,h},RGB(225,204,157),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    else if(badge)text(m.unread>99?L"99+":std::to_wstring(m.unread),{0,0,w,h},RGB(255,255,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    else {
        text(m.adviser+L"의 열흘 보고",{24,18,440,52},RGB(225,204,157),DT_SINGLELINE);
        text(L"×",{461,15,493,52},RGB(180,190,202),DT_CENTER|DT_SINGLELINE);
        text(m.collecting?L"진행 중 · 소식을 모으고 있습니다":L"지난 순의 소식 · 다음 진행 전까지 보관",{24,58,485,90},RGB(151,168,184),DT_SINGLELINE);
        text(m.title,{24,112,485,147},RGB(245,242,231),DT_SINGLELINE|DT_END_ELLIPSIS);
        int saved=SaveDC(dc);IntersectClipRect(dc,24,158,486,470);
        RECT measure{24,0,486,0};DrawTextW(dc,m.body.c_str(),static_cast<int>(m.body.size()),&measure,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);
        scroll_y=(std::min)(scroll_y,(std::max)(0,static_cast<int>(measure.bottom)-312));
        AcquireSRWLockExclusive(&lock);max_scroll=(std::max)(0,static_cast<int>(measure.bottom)-312);scroll=scroll_y;ReleaseSRWLockExclusive(&lock);
        text(m.body,{24,158-scroll_y,486,158-scroll_y+measure.bottom},RGB(209,219,227),DT_WORDBREAK);
        RestoreDC(dc,saved);
        text(L"‹ 이전",{20,520,110,556},RGB(208,189,147),DT_SINGLELINE);
        text(L"다음 ›",{126,520,216,556},RGB(208,189,147),DT_SINGLELINE);
        text(L"지우기",{232,520,332,556},RGB(167,182,197),DT_SINGLELINE);
        text(L"모두 비우기",{358,520,490,556},RGB(167,182,197),DT_SINGLELINE);
        text(m.detail?L"요약 보기":L"원문 보기",{364,480,488,510},RGB(167,182,197),DT_RIGHT|DT_SINGLELINE);
    }
    // GDI clears alpha for glyph pixels; restore opaque alpha before the shader's
    // geometric mask. There are no black rectangles around the circular portrait.
    for(int i=0;i<w*h;++i)data[i]|=0xff000000u;
    bool ok=avatar?upload(c,avatar_texture,avatar_view,w,h,data):badge?upload(c,badge_texture,badge_view,w,h,data):upload(c,panel_texture,panel_view,w,h,data);
    SelectObject(dc,oldfont);SelectObject(dc,old);DeleteObject(font);DeleteObject(bm);DeleteDC(dc);return ok;
}
static void quad(Rect r,double scale,double ox,double oy,float radius,float opacity,ID3D11ShaderResourceView *view) {
    D3D11_VIEWPORT viewport{static_cast<float>(ox+r.x*scale),static_cast<float>(oy+r.y*scale),static_cast<float>(r.w*scale),static_cast<float>(r.h*scale),0,1};
    deferred->RSSetViewports(1,&viewport);float values[]={viewport.Width,viewport.Height,static_cast<float>(radius*scale),opacity};
    deferred->UpdateSubresource(constants,0,nullptr,values,0,0);deferred->PSSetShaderResources(0,1,&view);deferred->Draw(3,0);
}
static bool capture_portrait(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc,int x,int y,int w,int h) {
    // A successful GPU copy can still contain a loading/black frame. Validate
    // RGB on this render thread and retry later without discarding a good face.
    if(desc.SampleDesc.Count!=1 || (desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM && desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB && desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM && desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB))return false;
    ID3D11Texture2D *candidate=nullptr,*staging=nullptr;ID3D11ShaderResourceView *view=nullptr;
    D3D11_TEXTURE2D_DESC td=desc;td.Width=w;td.Height=h;td.MipLevels=td.ArraySize=1;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;td.CPUAccessFlags=td.MiscFlags=0;td.Usage=D3D11_USAGE_DEFAULT;
    bool valid=false;
    if(SUCCEEDED(d->CreateTexture2D(&td,nullptr,&candidate)) && SUCCEEDED(d->CreateShaderResourceView(candidate,nullptr,&view))) {
        D3D11_BOX b{static_cast<UINT>(x),static_cast<UINT>(y),0,static_cast<UINT>(x+w),static_cast<UINT>(y+h),1};c->CopySubresourceRegion(candidate,0,0,0,0,back,0,&b);
        td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        if(SUCCEEDED(d->CreateTexture2D(&td,nullptr,&staging))) {
            c->CopyResource(staging,candidate);D3D11_MAPPED_SUBRESOURCE mapped{};
            if(SUCCEEDED(c->Map(staging,0,D3D11_MAP_READ,0,&mapped))) {
                unsigned lit=0,total=0,lowest=255,highest=0;
                for(int py=h/8;py<7*h/8;py+=2)for(int px=w/8;px<7*w/8;px+=2) {
                    const auto *p=static_cast<const unsigned char *>(mapped.pData)+py*mapped.RowPitch+px*4;
                    unsigned rgb=(static_cast<unsigned>(p[0])+p[1]+p[2])/3;
                    ++total;if(rgb>16)++lit;lowest=(std::min)(lowest,rgb);highest=(std::max)(highest,rgb);
                }
                c->Unmap(staging,0);valid=total && lit>total/20 && highest-lowest>8;
            }
        }
    }
    drop(staging);
    if(valid){drop(portrait_view);drop(portrait_texture);portrait_texture=candidate;portrait_view=view;}
    else {drop(view);drop(candidate);}
    return valid;
}
static void draw(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc) {
    if(!is_visible() || !initialize(d))return;
    double scale=(std::min)(desc.Width/1920.0,desc.Height/1080.0),ox=(desc.Width-1920*scale)/2,oy=(desc.Height-1080*scale)/2;
    if(flag(&portrait_requested) && (!portrait_attempt_at || GetTickCount64()-portrait_attempt_at>=120)) {
        portrait_attempt_at=GetTickCount64();
        AcquireSRWLockShared(&lock);Rect s=source;int revision=source_revision;ReleaseSRWLockShared(&lock);
        int x=static_cast<int>(ox+s.x*scale),y=static_cast<int>(oy+s.y*scale),w=static_cast<int>(s.w*scale),h=static_cast<int>(s.h*scale);
        if(x>=0 && y>=0 && w>8 && h>8 && x+w<=static_cast<int>(desc.Width) && y+h<=static_cast<int>(desc.Height)) {
            if(capture_portrait(d,c,back,desc,x,y,w,h)) {
                InterlockedExchange(&portrait_revision,revision);InterlockedExchange(&portrait_captured,1);InterlockedExchange(&portrait_requested,0);
            }
        }
    }
    AcquireSRWLockShared(&lock);if(draw_revision!=model_revision){draw_model=model;draw_revision=model_revision;}
    int sy=scroll;ULONGLONG started=opened_at;ReleaseSRWLockShared(&lock);const Model &m=draw_model;
    if((!flag(&portrait_captured) || !portrait_view) && !avatar_view && !raster(c,m,0,false,true))return;
    if(m.open && (painted_generation!=m.generation || painted_scroll!=sy || !painted_open || painted_detail!=m.detail)) {
        if(!raster(c,m,sy,false))return;painted_generation=m.generation;painted_scroll=sy;
    }
    if(painted_unread!=m.unread){if(!raster(c,m,0,true))return;painted_unread=m.unread;}painted_open=m.open;painted_detail=m.detail;
    ID3D11RenderTargetView *target=nullptr;if(FAILED(d->CreateRenderTargetView(back,nullptr,&target)))return;
    deferred->ClearState();deferred->OMSetRenderTargets(1,&target,nullptr);deferred->OMSetBlendState(blend,nullptr,~0u);deferred->OMSetDepthStencilState(depth,0);
    deferred->RSSetState(raster_state);deferred->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    deferred->VSSetShader(vs,nullptr,0);deferred->PSSetShader(ps,nullptr,0);deferred->PSSetSamplers(0,1,&sampler);deferred->PSSetConstantBuffers(0,1,&constants);
    quad(portrait,scale,ox,oy,44,1,flag(&portrait_captured) && portrait_view?portrait_view:avatar_view);
    if(m.unread)quad({92,948,38,38},scale,ox,oy,19,1,badge_view);
    if(m.open && panel_view) {
        double t=ease((GetTickCount64()-started)/190.0);Rect b=bubble;
        b.y+=static_cast<float>((1-t)*22);quad({b.x-18,b.y+524,20,28},scale,ox,oy,-1,static_cast<float>(t),panel_view);
        quad(b,scale,ox,oy,24,static_cast<float>(t),panel_view);
    }
    ID3D11CommandList *commands=nullptr;
    if(SUCCEEDED(deferred->FinishCommandList(FALSE,&commands))) {
        // The D3D runtime saves/restores the complete immediate pipeline state.
        c->ExecuteCommandList(commands,TRUE);commands->Release();InterlockedExchange(&ready,1);
    }
    target->Release();
}
}
