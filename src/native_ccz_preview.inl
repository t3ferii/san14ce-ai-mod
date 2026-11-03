// Representative sprites inside the native selection dialog. Read-only D3D overlay, no Windows popup.
#include "ccz_catalog.h"
#include "ccz_preview_palette.h"
namespace ccz_preview {
struct Picture {std::wstring label,detail,badge;std::array<unsigned,48*48> pixels{};};
constexpr unsigned kGutter=0,kPanelWidth=560,kCanvasWidth=kGutter+kPanelWidth,kCanvasHeight=600;
static SRWLOCK lock=SRWLOCK_INIT;
static std::vector<Picture> pictures;static std::wstring heading,hint;
static volatile LONG visible,dirty=1,release_pending;static ID3D11Texture2D *texture;static DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
static unsigned width,height;static std::vector<unsigned> pixels;
static bool needs_draw(){return InterlockedCompareExchange(&visible,0,0)!=0 || InterlockedCompareExchange(&release_pending,0,0)!=0;}
static void hide(){InterlockedExchange(&visible,0);AcquireSRWLockExclusive(&lock);pictures.clear();heading.clear();hint.clear();ReleaseSRWLockExclusive(&lock);InterlockedExchange(&dirty,1);InterlockedExchange(&release_pending,1);}
static void set(std::wstring title,std::vector<Picture> next,std::wstring note=L""){AcquireSRWLockExclusive(&lock);heading=std::move(title);hint=std::move(note);pictures=std::move(next);ReleaseSRWLockExclusive(&lock);InterlockedExchange(&dirty,1);InterlockedExchange(&visible,1);}
static Picture picture(ccz_catalog::Reader &catalog,unsigned cell,const std::wstring &label){
    Picture p;p.label=label;std::array<unsigned char,2304> raw{};
    if(catalog.preview(cell,raw))for(size_t i=0;i<raw.size();++i){const auto &pal=catalog.palettes[cell>=4096?1:0];p.pixels[i]=ccz_catalog::representative_colour(pal.data(),raw[i]);}
    return p;
}
static int sprite_size(int target,double scale){return 48*(std::max)(1,static_cast<int>(target*scale/48+0.5));}
static bool raster(unsigned w,unsigned h){
    std::vector<Picture> copy;std::wstring title,note;AcquireSRWLockShared(&lock);copy=pictures;title=heading;note=hint;ReleaseSRWLockShared(&lock);
    BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=static_cast<LONG>(w);bi.bmiHeader.biHeight=-static_cast<LONG>(h);bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(nullptr);void *bits=nullptr;HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!bm || !dc){if(bm)DeleteObject(bm);if(dc)DeleteDC(dc);return false;}
    HGDIOBJ old=SelectObject(dc,bm);auto out=static_cast<unsigned *>(bits);for(size_t i=0;i<static_cast<size_t>(w)*h;++i)out[i]=0xFF211E19u;
    double s=w/static_cast<double>(kCanvasWidth);const int origin=static_cast<int>(kGutter);
    auto box=[&](int x,int y,int bw,int bh,unsigned colour){
        int x0=(std::max)(0,static_cast<int>(x*s)),y0=(std::max)(0,static_cast<int>(y*s));
        int x1=(std::min)(static_cast<int>(w),static_cast<int>((x+bw)*s)),y1=(std::min)(static_cast<int>(h),static_cast<int>((y+bh)*s));
        for(int py=y0;py<y1;++py)for(int px=x0;px<x1;++px)out[py*w+px]=colour;
    };
    auto make_font=[&](int size,int weight){return CreateFontW(-(std::max)(10,static_cast<int>(size*s)),0,0,0,weight,FALSE,FALSE,FALSE,HANGUL_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"맑은 고딕");};
    HFONT large=make_font(28,FW_BOLD),normal=make_font(24,FW_NORMAL),font_small=make_font(18,FW_NORMAL);
    HGDIOBJ old_font=SelectObject(dc,normal);SetBkMode(dc,TRANSPARENT);
    auto text=[&](const std::wstring &value,int x,int y,int tw,int th,HFONT font,COLORREF colour,UINT flags){
        RECT r={static_cast<LONG>((origin+x)*s),static_cast<LONG>(y*s),static_cast<LONG>((origin+x+tw)*s),static_cast<LONG>((y+th)*s)};
        SelectObject(dc,font);SetTextColor(dc,colour);DrawTextW(dc,value.c_str(),-1,&r,flags|DT_NOPREFIX);
    };
    box(origin,0,560,600,0xFF96805Bu);box(origin+2,2,556,596,0xFF26221Cu);
    box(origin+24,100,512,1,0xFF65553Bu);
    text(title,24,18,512,38,large,RGB(236,213,165),DT_SINGLELINE|DT_END_ELLIPSIS);
    text(note,24,60,512,38,font_small,RGB(184,175,156),DT_WORDBREAK|DT_END_ELLIPSIS);
    const bool single=copy.size()==1;
    for(size_t i=0;i<copy.size() && i<3;++i){
        int top=single?112:108+static_cast<int>(i)*136,card_height=single?404:128;
        box(origin+16,top,528,card_height,0xFF574B36u);box(origin+17,top+1,526,card_height-2,0xFF312B23u);
        int size=sprite_size(single?192:96,s);
        int left=static_cast<int>((origin+(single?280:100))*s)-size/2;
        int sprite_top=static_cast<int>((top+(single?156:64))*s)-size/2;
        bool nonempty=false;
        for(int y=0;y<size;++y)for(int x=0;x<size;++x){
            unsigned colour=copy[i].pixels[(y*48/size)*48+x*48/size];int px=left+x,py=sprite_top+y;
            if((colour>>24) && px>=0 && py>=0 && px<static_cast<int>(w) && py<static_cast<int>(h)){out[py*w+px]=colour;nonempty=true;}
        }
        if(single){
            text(copy[i].badge,32,top+24,496,28,font_small,RGB(201,173,117),DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
            if(!nonempty)text(L"3D",32,top+180,496,60,large,RGB(145,134,114),DT_CENTER|DT_SINGLELINE);
            text(copy[i].label,32,top+274,496,42,large,RGB(238,228,207),DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
            text(copy[i].detail,32,top+324,496,32,font_small,RGB(179,170,151),DT_CENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        }else{
            if(!nonempty)text(L"3D",24,top+48,152,40,large,RGB(145,134,114),DT_CENTER|DT_SINGLELINE);
            text(copy[i].badge,196,top+14,324,28,font_small,RGB(201,173,117),DT_SINGLELINE|DT_END_ELLIPSIS);
            text(copy[i].label,196,top+47,324,40,normal,RGB(238,228,207),DT_SINGLELINE|DT_END_ELLIPSIS);
            text(copy[i].detail,196,top+88,324,28,font_small,RGB(179,170,151),DT_SINGLELINE|DT_END_ELLIPSIS);
        }
    }
    box(origin+24,524,512,1,0xFF65553Bu);
    text(L"대표색은 빨강 · 게임에서는 세력색 적용",20,540,520,24,font_small,RGB(166,156,136),DT_CENTER|DT_SINGLELINE);
    text(L"처음 설정 화면 하단 ‘결정’으로 전체 설정 저장",20,570,520,22,font_small,RGB(211,192,154),DT_CENTER|DT_SINGLELINE);
    GdiFlush();for(size_t i=0;i<static_cast<size_t>(w)*h;++i)out[i]|=0xFF000000u;
    pixels.assign(out,out+static_cast<size_t>(w)*h);SelectObject(dc,old_font);DeleteObject(large);DeleteObject(normal);DeleteObject(font_small);SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);return true;
}
static void draw(ID3D11Device *device,ID3D11DeviceContext *context,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc,bool rgba){
    if(!needs_draw())return;
    if(!InterlockedCompareExchange(&visible,0,0)){if(texture){texture->Release();texture=nullptr;}std::vector<unsigned>().swap(pixels);InterlockedExchange(&release_pending,0);return;}
    InterlockedExchange(&release_pending,0);
    double s=(std::min)(desc.Width/1920.0,desc.Height/1080.0);int x=static_cast<int>((desc.Width-1920*s)/2+1280*s),y=static_cast<int>((desc.Height-1080*s)/2+90*s);
    unsigned w=(std::max)(1u,static_cast<unsigned>(kCanvasWidth*s)),h=(std::max)(1u,static_cast<unsigned>(kCanvasHeight*s));if(x<0 || y<0 || x+w>desc.Width || y+h>desc.Height)return;
    if(texture){ID3D11Device *owner=nullptr;texture->GetDevice(&owner);if(owner!=device){texture->Release();texture=nullptr;}if(owner)owner->Release();}
    if(!texture || width!=w || height!=h || format!=desc.Format){if(texture){texture->Release();texture=nullptr;}D3D11_TEXTURE2D_DESC td={};td.Width=w;td.Height=h;td.MipLevels=td.ArraySize=1;td.Format=desc.Format;td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;if(FAILED(device->CreateTexture2D(&td,nullptr,&texture)))return;width=w;height=h;format=desc.Format;InterlockedExchange(&dirty,1);}
    if(InterlockedExchange(&dirty,0)){if(!raster(w,h)){InterlockedExchange(&dirty,1);return;}if(rgba)for(auto &p:pixels)p=(p&0xFF00FF00u)|((p&255u)<<16)|((p>>16)&255u);context->UpdateSubresource(texture,0,nullptr,pixels.data(),w*4,0);}
    if(InterlockedCompareExchange(&visible,0,0))context->CopySubresourceRegion(back,0,x,y,0,texture,0,nullptr);
}
}
