// Officer filter side panel for the native officer roster (정보 > 무장).
// Drawn like the HUD entry button: a cached opaque texture copied into the
// back buffer, rebuilt only when its model changes. Clicks are queued here and
// handled by the roster's own UI frame; no game call happens in this file.
#include <string>
#include <vector>
namespace officer_panel {
enum Widget : unsigned {
    None=0,Advanced,ScopeAll,ScopeName,ScopeTrait,ScopeFormation,ScopeTactic,Keyword,
    PickAll,PickAny,PickExclude,Presets,Clear,Multi,Commands,Count
};
struct Rect {int x,y,w,h;};
struct Model {
    unsigned scope=0;               // 0 all,1 name,2 trait,3 formation,4 tactic
    std::wstring keyword,all,any,exclude,advanced;
    unsigned matched=0,total=0;bool multi=false;unsigned selected=0;
    std::wstring status;
};
static SRWLOCK model_lock=SRWLOCK_INIT;
static Model model;
static volatile LONG panel_visible,panel_ready,panel_dirty=1,pending;
static unified_overlay::Bounds virtual_bounds; // panel rectangle in 1920x1080 space
static unified_overlay::Bounds pixel_bounds;   // last drawn rectangle in back-buffer pixels
static ID3D11Texture2D *panel_texture;
static DXGI_FORMAT panel_format=DXGI_FORMAT_UNKNOWN;
static unsigned panel_width,panel_height;
static std::vector<unsigned> panel_pixels;
static unified_overlay::Pointer panel_mouse;
static LONG read(volatile LONG *v){return InterlockedCompareExchange(v,0,0);}
static bool is_visible(){return read(&panel_visible)!=0;}
static constexpr int kWidth=460;
// Widget rectangles relative to the panel (virtual 1920x1080 pixels).
static Rect widget_rect(unsigned id) {
    switch(id) {
    case Advanced:return {300,14,144,40};
    case ScopeAll:case ScopeName:case ScopeTrait:case ScopeFormation:case ScopeTactic:
        return {16+static_cast<int>(id-ScopeAll)*86,96,82,36};
    case Keyword:return {16,140,428,40};
    case PickAll:return {356,198,88,34};
    case PickAny:return {356,290,88,34};
    case PickExclude:return {356,382,88,34};
    case Presets:return {16,474,206,40};
    case Clear:return {238,474,206,40};
    case Multi:return {16,580,206,40};
    case Commands:return {238,580,206,40};
    default:return {0,0,0,0};
    }
}
static unsigned hit(int vx,int vy) {
    for(unsigned id=Advanced;id<Count;++id){Rect r=widget_rect(id);if(vx>=r.x && vy>=r.y && vx<r.x+r.w && vy<r.y+r.h)return id;}
    return None;
}
static void set_model(const Model &next) {
    AcquireSRWLockExclusive(&model_lock);model=next;ReleaseSRWLockExclusive(&model_lock);InterlockedExchange(&panel_dirty,1);
}
static void show(bool value,const unified_overlay::Bounds &where) {
    AcquireSRWLockExclusive(&model_lock);
    bool moved=where.x!=virtual_bounds.x || where.y!=virtual_bounds.y || where.width!=virtual_bounds.width || where.height!=virtual_bounds.height;
    virtual_bounds=where;ReleaseSRWLockExclusive(&model_lock);
    if(moved)InterlockedExchange(&panel_dirty,1);
    if(InterlockedExchange(&panel_visible,value?1:0)!=(value?1:0) && !value){InterlockedExchange(&panel_ready,0);panel_mouse.armed=false;}
}
static unsigned take() {return static_cast<unsigned>(InterlockedExchange(&pending,0));}
// Map a client-area point to a widget. Returns true when the point is on the panel.
static bool locate(HWND hwnd,LPARAM lparam,unsigned &widget) {
    widget=None;if(!read(&panel_visible) || !read(&panel_ready))return false;
    RECT client={};GetClientRect(hwnd,&client);if(client.right<=0 || client.bottom<=0)return false;
    AcquireSRWLockShared(&model_lock);unified_overlay::Bounds p=pixel_bounds,v=virtual_bounds;ReleaseSRWLockShared(&model_lock);
    int bw=unified_overlay::buffer_width,bh=unified_overlay::buffer_height;if(bw<=0 || bh<=0 || p.width<=0 || p.height<=0)return false;
    int x=static_cast<int>(static_cast<long long>(static_cast<short>(LOWORD(lparam)))*bw/client.right);
    int y=static_cast<int>(static_cast<long long>(static_cast<short>(HIWORD(lparam)))*bh/client.bottom);
    if(!unified_overlay::contains(p,x,y))return false;
    int vx=static_cast<int>(static_cast<long long>(x-p.x)*v.width/p.width),vy=static_cast<int>(static_cast<long long>(y-p.y)*v.height/p.height);
    widget=hit(vx,vy);return true;
}
// Returns true when the message was consumed by the panel.
static bool input(HWND hwnd,UINT message,LPARAM lparam) {
    if(message!=WM_LBUTTONDOWN && message!=WM_LBUTTONUP && message!=WM_LBUTTONDBLCLK && message!=WM_RBUTTONDOWN &&
       message!=WM_RBUTTONUP && message!=WM_MOUSEWHEEL)return false;
    unsigned widget=None;
    if(message==WM_MOUSEWHEEL) {
        POINT pt{static_cast<short>(LOWORD(lparam)),static_cast<short>(HIWORD(lparam))};ScreenToClient(hwnd,&pt);
        return locate(hwnd,MAKELPARAM(pt.x,pt.y),widget);
    }
    bool inside=locate(hwnd,lparam,widget);
    if(message==WM_LBUTTONDOWN || message==WM_LBUTTONDBLCLK){panel_mouse.down(inside,inside && widget!=None,0);return inside;}
    if(message==WM_LBUTTONUP) {
        bool fire=false;bool handled=panel_mouse.up(inside,inside && widget!=None,0,fire);
        if(fire)InterlockedExchange(&pending,static_cast<LONG>(widget));
        return handled || inside;
    }
    return inside;
}
static std::wstring field_text(const std::wstring &s,const wchar_t *empty){return s.empty()?std::wstring(empty):s;}
static bool raster(unsigned width,unsigned height,const Model &m) {
    if(!width || !height)return false;
    double scale=static_cast<double>(width)/kWidth;
    auto S=[&](int v){return static_cast<int>(v*scale+0.5);};
    BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=static_cast<LONG>(width);info.bmiHeader.biHeight=-static_cast<LONG>(height);
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return false;
    void *bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bitmap || !bits){if(bitmap)DeleteObject(bitmap);DeleteDC(dc);return false;}
    HGDIOBJ old_bitmap=SelectObject(dc,bitmap);auto data=static_cast<unsigned *>(bits);
    // Dark lacquered frame with a parchment body, the colours of the native list.
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        unsigned r=46u+y*14u/height,g=36u+y*10u/height,b=24u+y*6u/height;
        data[y*width+x]=0xff000000u|(r<<16)|(g<<8)|b;
    }
    HPEN gold=CreatePen(PS_SOLID,(std::max)(1,S(2)),RGB(176,141,72));
    HPEN brown=CreatePen(PS_SOLID,(std::max)(1,S(1)),RGB(113,72,25));
    HBRUSH parchment=CreateSolidBrush(RGB(236,226,190)),field=CreateSolidBrush(RGB(248,242,222)),
           active=CreateSolidBrush(RGB(160,52,28)),button=CreateSolidBrush(RGB(222,206,160));
    HFONT title=CreateFontW(-S(26),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,HANGUL_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"궁서");
    HFONT body=CreateFontW(-S(19),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,HANGUL_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"궁서");
    HFONT minor_font=CreateFontW(-S(16),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,HANGUL_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"궁서");
    HGDIOBJ old_pen=SelectObject(dc,gold),old_brush=SelectObject(dc,GetStockObject(NULL_BRUSH)),old_font=SelectObject(dc,body);
    Rectangle(dc,S(2),S(2),static_cast<int>(width)-S(2),static_cast<int>(height)-S(2));
    SetBkMode(dc,TRANSPARENT);
    auto text=[&](HFONT font,COLORREF color,int x,int y,int w,int h,const std::wstring &s,UINT flags){
        SelectObject(dc,font);SetTextColor(dc,color);RECT r{S(x),S(y),S(x+w),S(y+h)};
        DrawTextW(dc,s.c_str(),-1,&r,flags|DT_NOPREFIX|DT_SINGLELINE|DT_END_ELLIPSIS);
    };
    auto box=[&](Rect r,HBRUSH fill,HPEN pen){SelectObject(dc,fill);SelectObject(dc,pen);
        RoundRect(dc,S(r.x),S(r.y),S(r.x+r.w),S(r.y+r.h),S(8),S(8));};
    auto push=[&](unsigned id,const std::wstring &label,bool on){Rect r=widget_rect(id);box(r,on?active:button,brown);
        text(body,on?RGB(250,236,200):RGB(64,43,28),r.x,r.y,r.w,r.h,label,DT_CENTER|DT_VCENTER);};
    // Title bar.
    text(title,RGB(236,214,160),18,14,280,40,L"무장 선별",DT_LEFT|DT_VCENTER);
    push(Advanced,m.advanced.empty()?L"고급 조건":L"고급 조건 ●",!m.advanced.empty());
    // Parchment body.
    SelectObject(dc,parchment);SelectObject(dc,brown);
    Rectangle(dc,S(8),S(62),static_cast<int>(width)-S(8),static_cast<int>(height)-S(8));
    text(minor_font,RGB(90,60,30),18,68,420,24,L"키워드 검색 범위",DT_LEFT|DT_VCENTER);
    const wchar_t *scopes[]={L"전체",L"이름",L"개성",L"진형",L"전법"};
    for(unsigned i=0;i<5;++i)push(ScopeAll+i,scopes[i],m.scope==i);
    {Rect r=widget_rect(Keyword);box(r,field,brown);
     text(body,m.keyword.empty()?RGB(150,130,100):RGB(40,28,18),r.x+10,r.y,r.w-20,r.h,field_text(m.keyword,L"키워드 입력 (클릭)"),DT_LEFT|DT_VCENTER);}
    struct Row {unsigned pick;int label_y;const wchar_t *caption;const std::wstring *value;};
    Row rows[]={{PickAll,198,L"모두 보유",&m.all},{PickAny,290,L"하나 이상",&m.any},{PickExclude,382,L"제외",&m.exclude}};
    for(const auto &row:rows) {
        text(body,RGB(64,43,28),18,row.label_y,320,34,row.caption,DT_LEFT|DT_VCENTER);
        push(row.pick,L"선택",false);
        Rect f{16,row.label_y+40,428,40};box(f,field,brown);
        text(minor_font,row.value->empty()?RGB(150,130,100):RGB(40,28,18),f.x+10,f.y,f.w-20,f.h,
             field_text(*row.value,L"개성·진형·전법 이름, 쉼표로 구분"),DT_LEFT|DT_VCENTER);
    }
    push(Presets,L"프리셋",false);push(Clear,L"조건 비우기",false);
    text(body,RGB(64,43,28),18,526,420,40,L"일치 "+std::to_wstring(m.matched)+L" / "+std::to_wstring(m.total)+
        (m.multi?L"   선택 "+std::to_wstring(m.selected)+L"명":L""),DT_LEFT|DT_VCENTER);
    push(Multi,m.multi?L"다중 선택: 켜짐":L"다중 선택: 꺼짐",m.multi);push(Commands,L"일괄 명령",false);
    if(!m.advanced.empty())text(minor_font,RGB(90,60,30),18,630,420,30,L"고급: "+m.advanced,DT_LEFT|DT_VCENTER);
    text(minor_font,RGB(110,80,50),18,static_cast<int>(height/scale)-44,420,30,
        m.status.empty()?std::wstring(L"목록에서 무장을 고르면 연계 탭이 그 무장 기준"):m.status,DT_LEFT|DT_VCENTER);
    panel_pixels.assign(data,data+static_cast<size_t>(width)*height);
    SelectObject(dc,old_font);SelectObject(dc,old_brush);SelectObject(dc,old_pen);SelectObject(dc,old_bitmap);
    for(HGDIOBJ o:{static_cast<HGDIOBJ>(gold),static_cast<HGDIOBJ>(brown),static_cast<HGDIOBJ>(parchment),static_cast<HGDIOBJ>(field),
        static_cast<HGDIOBJ>(active),static_cast<HGDIOBJ>(button),static_cast<HGDIOBJ>(title),static_cast<HGDIOBJ>(body),static_cast<HGDIOBJ>(minor_font)})DeleteObject(o);
    DeleteObject(bitmap);DeleteDC(dc);return true;
}
// Called from the Present hook with the back buffer already resolved.
static void draw(ID3D11Device *device,ID3D11DeviceContext *context,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc,bool rgba) {
    if(!read(&panel_visible) || !device || !context)return;
    AcquireSRWLockShared(&model_lock);unified_overlay::Bounds v=virtual_bounds;Model m=model;ReleaseSRWLockShared(&model_lock);
    if(v.width<=0 || v.height<=0)return;
    if(panel_texture) {
        ID3D11Device *owner=nullptr;panel_texture->GetDevice(&owner);
        if(owner!=device){panel_texture->Release();panel_texture=nullptr;}
        if(owner)owner->Release();
    }
    double scale=(std::min)(desc.Width/1920.0,desc.Height/1080.0);
    double ox=(desc.Width-1920*scale)/2,oy=(desc.Height-1080*scale)/2;
    unified_overlay::Bounds p{static_cast<int>(ox+v.x*scale+0.5),static_cast<int>(oy+v.y*scale+0.5),
        (std::max)(1,static_cast<int>(v.width*scale+0.5)),(std::max)(1,static_cast<int>(v.height*scale+0.5))};
    if(p.x<0 || p.y<0 || p.x+p.width>static_cast<int>(desc.Width) || p.y+p.height>static_cast<int>(desc.Height))return;
    if(!panel_texture || panel_width!=static_cast<unsigned>(p.width) || panel_height!=static_cast<unsigned>(p.height) || panel_format!=desc.Format) {
        if(panel_texture){panel_texture->Release();panel_texture=nullptr;}
        D3D11_TEXTURE2D_DESC td={};td.Width=static_cast<unsigned>(p.width);td.Height=static_cast<unsigned>(p.height);
        td.MipLevels=1;td.ArraySize=1;td.Format=desc.Format;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;
        if(FAILED(device->CreateTexture2D(&td,nullptr,&panel_texture)))return;
        panel_width=td.Width;panel_height=td.Height;panel_format=td.Format;InterlockedExchange(&panel_dirty,1);
    }
    if(read(&panel_dirty)) {
        if(!raster(panel_width,panel_height,m))return;
        if(rgba)for(auto &pixel:panel_pixels)pixel=(pixel&0xff00ff00u)|((pixel&255u)<<16)|((pixel>>16)&255u);
        context->UpdateSubresource(panel_texture,0,nullptr,panel_pixels.data(),panel_width*4,0);InterlockedExchange(&panel_dirty,0);
    }
    context->CopySubresourceRegion(back,0,static_cast<UINT>(p.x),static_cast<UINT>(p.y),0,panel_texture,0,nullptr);
    AcquireSRWLockExclusive(&model_lock);pixel_bounds=p;ReleaseSRWLockExclusive(&model_lock);
    InterlockedExchange(&panel_ready,1);
}
}
