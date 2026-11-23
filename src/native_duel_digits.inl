// Relay duel round number (local298, [extensions] duel_relay since v16.7, [diagnostics] duel_manual_test before; user 「숫자그림 1~5합 … 6 7 8 … 확장 못하나?」): the original duel
// medallion has brush digits for rounds 1-5 only, so the relay scene draws its own number over it on Present (overlay row
// duel_digits). Glyphs: Nanum Brush Script fill coverage (duel_digits_font.h, SIL OFL 1.1), made into premultiplied RGBA once per
// device with the rehearsal's dark stroke (kStroke px), orange glow (box blur kGlowBox x 3 at kGlow) and the gold gradient over the
// glyph box; shader duel_digits.hlsl (the advisor overlay's shader ignores texture alpha). duel_manual publishes the number and the
// medallion centre (1920 x 1080 units) from the UI thread; 0 = draw nothing. Self-contained (D3D11, <vector>, log_event) so the shared
// overlay fixture draws it on a real device. A failed build or a fault turns it off for the process (duel_manual keeps the original
// digits then: number_step checks broken()).
// 투지 bars (same plan, user 「투지 … 글자만 지워주고 노란색바로 하자」): under both main HP bars, a dark track with the side's 투지 (0-300) in
// yellow, ticks at 100 and 200 (one 필살 unit each), the 투지 the player's listed commands will spend in red at the end of the yellow;
// the right bar fills from the right like the original HP bar. Drawn from 1 x 16 gradient textures through the same shader.
// local299 (user 「원작 합숫자도 못가리고」, 「필살기 써도 필살기 바가 안달고」): the number sits on a soft disc of the medallion's colour
// (kDisc*, sampled from the user's screen, radius 72) that covers the original digit whatever the gauge shows; the empty track has a
// light rim (RRim) so a bar at 투지 0 still shows where it is. The relay command window is drawn here too (window section below).
#include <d3d11.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "duel_digits_font.h"
#include "duel_window_font.h"
#include "duel_digits_vs.h"
#include "duel_digits_ps.h"
namespace duel_digits {
static const int kPad=86,kStroke=7,kGlowBox=26,kFontPx=236;
static const float kSingle=118.0f,kDouble=100.0f,kGlow=0.75f;
static volatile LONG64 shown;   // 0 nothing, else number | logical x * 4 << 8 | logical y * 4 << 24
static volatile LONG64 bars;    // 0 nothing, else 1 | left 투지 << 1 | left red << 10 | right 투지 << 19 | right red << 28 (0-300 each)
static volatile LONG64 bar_rect[2];   // left / right bar: x * 4 | y * 4 << 16 | w * 4 << 32 | h * 4 << 48 (1920 x 1080)
static const int kMoraleFull=300,kRamps=5;
enum Ramp:int {RTrack=0,RYellow,RRed,RTick,RRim};
static const float kRampTop[kRamps][4]={{0.106f,0.078f,0.063f,0.92f},{1.0f,0.886f,0.478f,1.0f},{0.878f,0.376f,0.243f,0.95f},{0.051f,0.039f,0.031f,0.6f},
                                        {0.69f,0.541f,0.29f,0.95f}};
static const float kRampLow[kRamps][4]={{0.051f,0.039f,0.031f,0.92f},{0.851f,0.647f,0.11f,1.0f},{0.612f,0.165f,0.086f,0.95f},{0.051f,0.039f,0.031f,0.6f},
                                        {0.431f,0.322f,0.188f,0.95f}};
static const int kDiscPx=64;
static const float kDiscRx=72.0f,kDiscRy=72.0f,kDiscSoft=0.86f;   // half axes (1920 x 1080), the radius share where the edge fade starts
static const float kDiscIn[3]={0.361f,0.243f,0.188f},kDiscOut[3]={0.329f,0.204f,0.149f};   // #5c3e30 centre -> #543426 edge
static ID3D11Texture2D *disc_tex;
static ID3D11ShaderResourceView *disc;
static ID3D11Texture2D *ramp_tex[kRamps];
static ID3D11ShaderResourceView *ramp[kRamps];
static volatile LONG dead;
static ID3D11Device *owner;
static ID3D11VertexShader *vs;
static ID3D11PixelShader *ps;
static ID3D11SamplerState *sampler;
static ID3D11BlendState *blend;
static ID3D11RasterizerState *raster;
static ID3D11DepthStencilState *depth;
static ID3D11Texture2D *tex[10];
static ID3D11ShaderResourceView *view[10];
static int tw[10],th[10];
static bool built,srgb;
static unsigned draws,build_fails;
template<class T> static void drop(T *&p) {if(p){p->Release();p=nullptr;}}
static void release_window_assets();
static void release_chips();
static void release_all() {
    release_window_assets();
    release_chips();
    for(int i=0;i<10;++i){drop(view[i]);drop(tex[i]);}
    for(int i=0;i<kRamps;++i){drop(ramp[i]);drop(ramp_tex[i]);}
    drop(disc);drop(disc_tex);
    drop(vs);drop(ps);drop(sampler);drop(blend);drop(raster);drop(depth);drop(owner);built=false;
}
static int base36(char c) {return c<='9'?c-'0':c-'a'+10;}
// rle (duel_digits_font.h): a hex digit 0-e = one pixel at that level, "z<n>." / "f<n>." = n pixels (base 36) at level 0 / 15
static bool decode(const duel_digits_font::Mask &mk,float *a,int W) {
    int x=0,y=0;
    auto put=[&](int level){if(y<mk.h){a[(y+kPad)*W+x+kPad]=static_cast<float>(level)/15.0f;if(++x==mk.w){x=0;++y;}}};
    for(const char *s=mk.rle;*s;) {
        if(*s=='z' || *s=='f') {
            const int level=*s=='z'?0:15;
            int count=0;
            for(++s;*s && *s!='.';++s)count=count*36+base36(*s);
            if(*s)++s;
            while(count-->0)put(level);
        }
        else put(base36(*s++));
    }
    return y==mk.h && x==0;
}
static void max_pass(const float *in,float *out,int W,int H,bool horiz,int r) {   // max over +-r along rows or columns
    const int len=horiz?W:H,lines=horiz?H:W,step=horiz?1:W,stride=horiz?W:1;
    for(int l=0;l<lines;++l)for(int i=0;i<len;++i) {
        float v=0.0f;
        for(int j=i-r;j<=i+r;++j)if(j>=0 && j<len && in[l*stride+j*step]>v)v=in[l*stride+j*step];
        out[l*stride+i*step]=v;
    }
}
static void box_pass(const float *in,float *out,int W,int H,bool horiz,int r) {   // mean over +-r (running sum, zero outside)
    const int len=horiz?W:H,lines=horiz?H:W,step=horiz?1:W,stride=horiz?W:1;
    const float inv=1.0f/static_cast<float>(2*r+1);
    for(int l=0;l<lines;++l) {
        const float *src=in+l*stride;
        float *dst=out+l*stride,acc=0.0f;
        for(int j=0;j<=r && j<len;++j)acc+=src[j*step];
        for(int i=0;i<len;++i) {
            dst[i*step]=acc*inv;
            if(i+r+1<len)acc+=src[(i+r+1)*step];
            if(i-r>=0)acc-=src[(i-r)*step];
        }
    }
}
static unsigned char byte_of(float v) {return static_cast<unsigned char>(v<=0.0f?0:v>=1.0f?255:static_cast<int>(v*255.0f+0.5f));}
// One digit as premultiplied RGBA: the fill over the stroke over the glow (the rehearsal canvas: stroke with shadow, then the fill).
static bool make_glyph(ID3D11Device *d,int i) {
    const duel_digits_font::Mask &mk=duel_digits_font::kMasks[i];
    const int W=mk.w+2*kPad,H=mk.h+2*kPad,n=W*H;
    std::vector<float> a(n,0.0f),t(n),s(n),g(n);
    if(!decode(mk,a.data(),W))return false;
    max_pass(a.data(),t.data(),W,H,true,kStroke);max_pass(t.data(),s.data(),W,H,false,kStroke);
    box_pass(s.data(),g.data(),W,H,true,kGlowBox);box_pass(g.data(),t.data(),W,H,true,kGlowBox);box_pass(t.data(),g.data(),W,H,true,kGlowBox);
    box_pass(g.data(),t.data(),W,H,false,kGlowBox);box_pass(t.data(),g.data(),W,H,false,kGlowBox);box_pass(g.data(),t.data(),W,H,false,kGlowBox);
    static const float kTop[3]={1.0f,0.941f,0.69f},kMid[3]={1.0f,0.753f,0.29f},kLow[3]={0.941f,0.51f,0.118f};   // #fff0b0 / #ffc04a / #f0821e
    static const float kStrokeRgb[3]={0.227f,0.086f,0.031f},kGlowRgb[3]={1.0f,0.51f,0.118f};                    // #3a1608 / rgb(255,130,30)
    std::vector<unsigned> px(n);
    for(int y=0;y<H;++y) {
        float u=mk.h>1?static_cast<float>(y-kPad)/static_cast<float>(mk.h-1):0.0f;
        u=u<0.0f?0.0f:u>1.0f?1.0f:u;
        float grad[3];
        for(int c=0;c<3;++c)grad[c]=u<0.45f?kTop[c]+(kMid[c]-kTop[c])*(u/0.45f):kMid[c]+(kLow[c]-kMid[c])*((u-0.45f)/0.55f);
        for(int x=0;x<W;++x) {
            const int k=y*W+x;
            const float f=a[k],st=s[k],gl=t[k]*kGlow,under=st+(1.0f-st)*gl,al=f+(1.0f-f)*under;
            unsigned char rgb[3];
            for(int c=0;c<3;++c)rgb[c]=byte_of(f*grad[c]+(1.0f-f)*(st*kStrokeRgb[c]+(1.0f-st)*gl*kGlowRgb[c]));   // premultiplied
            px[k]=rgb[0]|(rgb[1]<<8)|(rgb[2]<<16)|(static_cast<unsigned>(byte_of(al))<<24);
        }
    }
    D3D11_TEXTURE2D_DESC td{};
    td.Width=static_cast<UINT>(W);td.Height=static_cast<UINT>(H);td.MipLevels=td.ArraySize=1;
    td.Format=srgb?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;
    td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{};sd.pSysMem=px.data();sd.SysMemPitch=static_cast<UINT>(W*4);
    if(FAILED(d->CreateTexture2D(&td,&sd,&tex[i])) || FAILED(d->CreateShaderResourceView(tex[i],nullptr,&view[i])))return false;
    tw[i]=W;th[i]=H;
    return true;
}
// #1b1410 -> #0d0a08 track, #ffe27a -> #d9a51c 투지, #e0603e -> #9c2a16 planned spend (the rehearsal's colours), tick #0d0a08
static bool make_ramp(ID3D11Device *d,int i) {
    unsigned px[16];
    for(int y=0;y<16;++y) {
        const float u=static_cast<float>(y)/15.0f,a=kRampTop[i][3]+(kRampLow[i][3]-kRampTop[i][3])*u;
        unsigned char c[3];
        for(int k=0;k<3;++k)c[k]=byte_of((kRampTop[i][k]+(kRampLow[i][k]-kRampTop[i][k])*u)*a);   // premultiplied
        px[y]=c[0]|(c[1]<<8)|(c[2]<<16)|(static_cast<unsigned>(byte_of(a))<<24);
    }
    D3D11_TEXTURE2D_DESC td{};
    td.Width=1;td.Height=16;td.MipLevels=td.ArraySize=1;
    td.Format=srgb?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;
    td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{};sd.pSysMem=px;sd.SysMemPitch=4;
    return SUCCEEDED(d->CreateTexture2D(&td,&sd,&ramp_tex[i])) && SUCCEEDED(d->CreateShaderResourceView(ramp_tex[i],nullptr,&ramp[i]));
}
// The disc under the number: a kDiscPx square, full inside kDiscSoft of the radius, smoothstep to clear at the rim, premultiplied.
static bool make_disc(ID3D11Device *d) {
    unsigned px[kDiscPx*kDiscPx];
    for(int y=0;y<kDiscPx;++y)for(int x=0;x<kDiscPx;++x) {
        const float u=(static_cast<float>(x)+0.5f)/kDiscPx*2.0f-1.0f,v=(static_cast<float>(y)+0.5f)/kDiscPx*2.0f-1.0f,r=std::sqrt(u*u+v*v);
        float t=r<=kDiscSoft?0.0f:r>=1.0f?1.0f:(r-kDiscSoft)/(1.0f-kDiscSoft);
        const float a=1.0f-t*t*(3.0f-2.0f*t),k=r>1.0f?1.0f:r;
        unsigned char c[3];
        for(int i=0;i<3;++i)c[i]=byte_of((kDiscIn[i]+(kDiscOut[i]-kDiscIn[i])*k)*a);
        px[y*kDiscPx+x]=c[0]|(c[1]<<8)|(c[2]<<16)|(static_cast<unsigned>(byte_of(a))<<24);
    }
    D3D11_TEXTURE2D_DESC td{};
    td.Width=td.Height=kDiscPx;td.MipLevels=td.ArraySize=1;
    td.Format=srgb?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;
    td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{};sd.pSysMem=px;sd.SysMemPitch=kDiscPx*4;
    return SUCCEEDED(d->CreateTexture2D(&td,&sd,&disc_tex)) && SUCCEEDED(d->CreateShaderResourceView(disc_tex,nullptr,&disc));
}
static bool build(ID3D11Device *d) {
    if(FAILED(d->CreateVertexShader(duel_digits_vs,sizeof(duel_digits_vs),nullptr,&vs)) ||
       FAILED(d->CreatePixelShader(duel_digits_ps,sizeof(duel_digits_ps),nullptr,&ps)))return false;
    D3D11_SAMPLER_DESC sd{};
    sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
    D3D11_BLEND_DESC bd{};
    D3D11_RENDER_TARGET_BLEND_DESC &rt=bd.RenderTarget[0];
    rt.BlendEnable=TRUE;rt.SrcBlend=D3D11_BLEND_ONE;rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOp=D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha=D3D11_BLEND_ONE;rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
    D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=FALSE;dd.StencilEnable=FALSE;
    if(FAILED(d->CreateSamplerState(&sd,&sampler)) || FAILED(d->CreateBlendState(&bd,&blend)) || FAILED(d->CreateRasterizerState(&rd,&raster)) ||
       FAILED(d->CreateDepthStencilState(&dd,&depth)))return false;
    for(int i=0;i<10;++i)if(!make_glyph(d,i))return false;
    for(int i=0;i<kRamps;++i)if(!make_ramp(d,i))return false;
    return make_disc(d);
}
struct Saved {   // what draw() changes on the game's context, put back after
    ID3D11RenderTargetView *rt[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]={};
    ID3D11DepthStencilView *dsv=nullptr;
    ID3D11BlendState *bs=nullptr;FLOAT factor[4]={};UINT mask=0;
    ID3D11DepthStencilState *ds=nullptr;UINT ref=0;
    ID3D11RasterizerState *rs=nullptr;
    D3D11_VIEWPORT vp[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]={};UINT vps=D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_PRIMITIVE_TOPOLOGY topology=D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11InputLayout *layout=nullptr;
    ID3D11VertexShader *v=nullptr;ID3D11PixelShader *p=nullptr;ID3D11GeometryShader *g=nullptr;ID3D11HullShader *h=nullptr;ID3D11DomainShader *dm=nullptr;
    ID3D11ShaderResourceView *srv=nullptr;ID3D11SamplerState *smp=nullptr;
    explicit Saved(ID3D11DeviceContext *c) {
        c->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,rt,&dsv);c->OMGetBlendState(&bs,factor,&mask);c->OMGetDepthStencilState(&ds,&ref);
        c->RSGetState(&rs);c->RSGetViewports(&vps,vp);c->IAGetPrimitiveTopology(&topology);c->IAGetInputLayout(&layout);
        c->VSGetShader(&v,nullptr,nullptr);c->PSGetShader(&p,nullptr,nullptr);c->GSGetShader(&g,nullptr,nullptr);
        c->HSGetShader(&h,nullptr,nullptr);c->DSGetShader(&dm,nullptr,nullptr);
        c->PSGetShaderResources(0,1,&srv);c->PSGetSamplers(0,1,&smp);
    }
    void put(ID3D11DeviceContext *c) {
        c->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,rt,dsv);c->OMSetBlendState(bs,factor,mask);c->OMSetDepthStencilState(ds,ref);
        c->RSSetState(rs);c->RSSetViewports(vps,vp);c->IASetPrimitiveTopology(topology);c->IASetInputLayout(layout);
        c->VSSetShader(v,nullptr,0);c->PSSetShader(p,nullptr,0);c->GSSetShader(g,nullptr,0);c->HSSetShader(h,nullptr,0);c->DSSetShader(dm,nullptr,0);
        c->PSSetShaderResources(0,1,&srv);c->PSSetSamplers(0,1,&smp);
        for(ID3D11RenderTargetView *&x:rt)drop(x);
        drop(dsv);drop(bs);drop(ds);drop(rs);drop(layout);drop(v);drop(p);drop(g);drop(h);drop(dm);drop(srv);drop(smp);
    }
};
static bool broken() {return InterlockedCompareExchange(&dead,0,0)!=0;}
// duel_manual_number army 8 = the glyphs did not build (value fails, hex back buffer format), army 9 = the draw faulted (value frames
// drawn); either turns the number off for the process.
struct Screen {float s,ox,oy;};   // 1920 x 1080 units to back buffer pixels (letterboxed like the game's UI)
static void quad(ID3D11DeviceContext *c,ID3D11ShaderResourceView *v,float x,float y,float w,float h) {
    if(!(w>0.25f) || !(h>0.25f))return;
    const D3D11_VIEWPORT vp{x,y,w,h,0.0f,1.0f};
    c->RSSetViewports(1,&vp);c->PSSetShaderResources(0,1,&v);c->Draw(3,0);
}
static void draw_bars(ID3D11DeviceContext *c,const Screen &sc,LONG64 b) {
    for(int side=0;side<2;++side) {
        const LONG64 r=InterlockedCompareExchange64(&bar_rect[side],0,0);
        if(!r)continue;
        const float x=sc.ox+static_cast<float>(r&0xFFFF)*0.25f*sc.s,y=sc.oy+static_cast<float>((r>>16)&0xFFFF)*0.25f*sc.s;
        const float w=static_cast<float>((r>>32)&0xFFFF)*0.25f*sc.s,h=static_cast<float>((r>>48)&0xFFFF)*0.25f*sc.s;
        const int morale=static_cast<int>((b>>(1+side*18))&0x1FF),red_raw=static_cast<int>((b>>(10+side*18))&0x1FF);
        const int m=morale>kMoraleFull?kMoraleFull:morale,red=red_raw>m?m:red_raw;
        const float fw=w*static_cast<float>(m-red)/static_cast<float>(kMoraleFull),rw=w*static_cast<float>(red)/static_cast<float>(kMoraleFull);
        const float edge=sc.s>0.5f?sc.s:0.5f;
        quad(c,ramp[RRim],x-2.0f*edge,y-2.0f*edge,w+4.0f*edge,h+4.0f*edge);   // local299: the empty track shows at 투지 0
        quad(c,ramp[RTrack],x,y,w,h);
        if(side==0) {quad(c,ramp[RYellow],x,y,fw,h);quad(c,ramp[RRed],x+fw,y,rw,h);}
        else {quad(c,ramp[RYellow],x+w-fw,y,fw,h);quad(c,ramp[RRed],x+w-fw-rw,y,rw,h);}
        for(int k=1;k<3;++k)quad(c,ramp[RTick],x+w*static_cast<float>(k)/3.0f-edge,y,2.0f*edge,h);
    }
}
static void draw_number(ID3D11DeviceContext *c,const Screen &sc,LONG64 v) {
    const unsigned number=static_cast<unsigned>(v&0xFF);
    const float lx=static_cast<float>((v>>8)&0xFFFF)*0.25f,ly=static_cast<float>((v>>24)&0xFFFF)*0.25f;
    int dig[2],count=0;
    if(number>=10)dig[count++]=static_cast<int>(number/10%10);
    dig[count++]=static_cast<int>(number%10);
    const float k=(count>1?kDouble:kSingle)/static_cast<float>(kFontPx)*sc.s;
    float total=0.0f,asc=0.0f,dsc=0.0f;
    for(int i=0;i<count;++i) {
        const duel_digits_font::Mask &mk=duel_digits_font::kMasks[dig[i]];
        total+=static_cast<float>(mk.adv);
        if(static_cast<float>(-mk.oy)>asc)asc=static_cast<float>(-mk.oy);
        if(static_cast<float>(mk.oy+mk.h)>dsc)dsc=static_cast<float>(mk.oy+mk.h);
    }
    const float cx=sc.ox+lx*sc.s,base=sc.oy+ly*sc.s+(asc-dsc)*0.5f*k;   // the glyphs' ink box centred on the point
    quad(c,disc,cx-kDiscRx*sc.s,sc.oy+ly*sc.s-kDiscRy*sc.s,2.0f*kDiscRx*sc.s,2.0f*kDiscRy*sc.s);   // the original digit covered
    float pen=-total*0.5f;
    for(int i=0;i<count;++i) {
        const int gi=dig[i];
        const duel_digits_font::Mask &mk=duel_digits_font::kMasks[gi];
        const float centre=pen+static_cast<float>(mk.adv)*0.5f;
        pen+=static_cast<float>(mk.adv);
        quad(c,view[gi],cx+(centre+static_cast<float>(mk.ox-kPad))*k,base+static_cast<float>(mk.oy-kPad)*k,static_cast<float>(tw[gi])*k,static_cast<float>(th[gi])*k);
    }
}
// ---- command window (local299, user 「명령창 UI 아티팩트에서 보던거랑 달라」: the rehearsal artifact's window drawn by the mod) ----
// duel_manual publishes the window's state (publish_window) while its Layout click catcher is up; this draws it in the artifact's look
// (ink panel of soft smudges made at half size, brush plates, Nanum Myeongjo texts). local305 (user 2026-11-23 「대기화면은 일기토 처음
// 시작하고나서 나오고 난입할때나오고 교체버튼 누를때나오고 필살버튼 누를때 나오는거야 … 실시간으로 태세를 바꾸는거지」): the strip is up from
// the duel's start to its end, bottom left — the four stances on the left (the one chosen lit), 필살 (gold when the engine allows one,
// red pending), 교대 (dark, blue pending; 「대기 무장 없음」 / 「교대 사용함」 dim), 자동 (lit while on) and, in a full hold, 「확정」 on the
// right; the header line over it: the 합 a hold is for and 「지금 …」 (the stance in play: the chosen one plays from the next 합). A
// full hold's ink reaches up over the list place: the 필살 list (필살기 / 급소 / 무쌍 left, 기합 / 견수 / 「돌아가기」 right; picked gold,
// not allowed dim) or the 교대 members (name and HP, blue; picked gold), else the hint. Plates are made once per device as the
// artifact's brush() draws them (same seeds and pseudo-random sequence: torn edges, a diagonal gradient, streaks); a plate over a live
// Layout button is opaque (the button under it never shows), a dim one keeps the artifact's 0.75. Texts: duel_window_font.h fill /
// edge masks coloured here (the artifact's 5 px black stroke at 0.8). win_draws counts drawn frames (duel_manual builds the button
// window when it does not move); a failed build sets win_dead (duel_manual_ovwin_build). local306 (user 「크리티컬이나 자동발동견수
// 퇴각(교대 횟수 다쓰면 퇴각으로 변하기)」, 「1B 2A 3A 4A」): the 교대 place reads 퇴각 while no 교대 can be made (red, blue pending,
// dim 「투지 부족」), the 퇴각 hold's list is 퇴각 (gold picked), its hint and 「돌아가기」; a status line under the strip shows the
// critical's charge (「크리티컬 n/6」 / 「크리티컬 가능」) and the guard timers (「견수 n합」 own, 「적 견수 n합」 the other side's).
static const int kWinX=0,kWinY=520,kWinW=520,kWinH=560,kRowStep=58,kSlotX=36,kPlateW=214,kPlateH=52,kOptX=276,kOptSlant=5;
static const int kStripY=818,kStripPanel[4]={0,760,520,320},kListY=548;
static const float kHeadY=790.0f;
static const float kStatY=1062.0f,kStatSpace=5.0f,kStatGap=18.0f;   // local306 status line (under the strip's last row, 1044)
// strip place i: 0-3 the stances (left), 4 필살, 5 교대, 6 자동, 7 「확정」 (right, on a slant); list place k: 0-2 left, 3-5 right
static void strip_rect(int i,int r[4]) {const int row=i&3;r[0]=i<4?kSlotX:kOptX-row*kOptSlant;r[1]=kStripY+row*kRowStep;r[2]=kPlateW;r[3]=kPlateH;}
static void list_rect(int k,int r[4]) {const int row=k%3;r[0]=k<3?kSlotX:kOptX-row*kOptSlant;r[1]=kListY+row*kRowStep;r[2]=kPlateW;r[3]=kPlateH;}
static const int kListSpc[6]={4,7,8,5,6,-1};   // the 필살 list: 필살기, 급소, 무쌍, 기합, 견수, 「돌아가기」 (commands, duel_manual kCmdName order)
// state (window_state packs a WinView): bit WOn | mode (0 the strip alone, else the full hold's kind 1-5) << WMode | list (0 none, 1
// 필살, 2 교대 members, 3 퇴각) << WList | the 합 a hold is for (0 none) << WNext | stance in play << WNow | stance chosen << WStance |
// 필살 (0 off, 1 on, 2 pending) << WSpc | 교대 (0 no member, 1 on, 2 pending, 3 used) << WSwap | 자동 << WAuto | 「확정」 << WOk | list
// place k allowed << WItem + k | picked place + 1 << WPick | list member j + 1 (engine slot) << WMember + j * 2 | local306: 퇴각 on
// the 교대 place (0 none: the 교대 plate, 1 on, 2 pending, 3 short of 투지) << WRet | the status line << WStat | critical charge (0-5
// of 6, 6 ready) << WCrit | own guard 합 << WGuard | the other side's guard 합 << WFoe
enum WinBit:int {WOn=0,WMode=1,WList=4,WNext=6,WNow=13,WStance=15,WSpc=17,WSwap=19,WAuto=21,WOk=22,WItem=23,WPick=29,WMember=32,WRet=36,
                 WStat=38,WCrit=39,WGuard=42,WFoe=46};
struct WinView {int mode=0,list=0,next=0,now=0,stance=0,spc=0,swp=0;bool autoplay=false,ok=false,item[6]={};int pick=-1,member[2]={-1,-1};
                int ret=0;bool stat=false;int crit=0,guard=0,foe=0;};
static volatile LONG64 win;
static const int kNameCap=16,kNamePx=20,kLabelPx=22,kNameStroke=3;
struct NameSet {wchar_t name[3][kNameCap];int hp[3];};
static NameSet names_pub;           // UI thread writes between two names_seq steps (odd while writing)
static volatile LONG names_seq;
static LONG names_built=-1;         // Present thread: the names_seq its textures show (-1 none)
static volatile LONG win_draws,win_dead;
static unsigned win_build_ms;   // the assets' build on the Present thread (the fixture prints it)
enum Tone:int {TDark=0,TLit,TGold,TDim,TBlue,TPurple,TBack,TTones};   // TBack: the dim colours, opaque (「돌아가기」 is live)
struct Rgb {unsigned char r,g,b;};   // a colour as bytes (the artifact's #rrggbb)
static const Rgb kTone[TTones][2]={{Rgb{43,25,19},Rgb{75,44,32}},{Rgb{122,30,20},Rgb{194,64,42}},{Rgb{107,74,18},Rgb{199,154,46}},{Rgb{38,35,33},Rgb{58,54,51}},{Rgb{28,43,74},Rgb{60,93,150}},{Rgb{58,32,80},Rgb{109,69,150}},{Rgb{38,35,33},Rgb{58,54,51}}};
static const int kPlatePad=12,kSlotSeed=11,kOptSeed=40,kOkSeed=90,kAutoSeed=91,kInkSeed=7,kInkHalf=2;
static const float kInkFade=56.0f;   // the ink panel's top and right edges fade over this (1920 x 1080 units)
struct Tex {ID3D11Texture2D *t;ID3D11ShaderResourceView *v;};
static Tex slot_plate[4][2],opt_plate[6][TTones],ok_plate,ink_panel,strip_ink;
// local306: GCmd / GCmdDim hold 11 commands (10 퇴각); GLack 「투지 부족」, GRetHint the 퇴각 hold's hint, GSt* the status line's
// pieces (GStD0 + d the digits).
enum TextGroup:int {GHead=0,GNow=GHead+12,GCmd=GNow+4,GCmdDim=GCmd+11,GSpecial=GCmdDim+11,GSpecialDim,GBack,GNoMember,GSwapUsed,GOk,GAuto,GHint,
                    GLack,GRetHint,GStCrit,GStReady,GStOf6,GStGuard,GStFoe,GStHap,GStD0,GTexts=GStD0+10};
static Tex text_tex[GTexts];
static Tex name_tex[3],label_tex[3];   // local301: member i's name / 「name  체력 N」 (the 교대 list)
static int name_wh[3][3],label_wh[3][3];   // w, h, the glyphs' middle from the top (gdi_text)
static bool win_built;
static void drop_tex(Tex &x) {drop(x.v);drop(x.t);}
static void drop_names() {
    for(int i=0;i<3;++i){drop_tex(name_tex[i]);drop_tex(label_tex[i]);for(int k=0;k<3;++k)name_wh[i][k]=label_wh[i][k]=0;}
    names_built=-1;
}
// ---- stickers (local301 the join, user 「만화책 스럽게 난입 !~ 이런 큰글자로 한단어로 딱 스티커?처럼」; local303 every round event
// and the end, user 「아직도 하얀 글씨 뭐 뜨는데 아무것도 안보여 난입처럼 스티커화 해서 보여줄 수 없나?」) ----
// duel_manual publishes a round's events as the round shows them (publish_sticker: a kind for the left half, the right half — the
// half of that side's HUD — and the centre, the tick); this draws the kind's word (Nanum Brush Script, cream with a black stroke) on
// a burst in the kind's colours (the command window's plates: 필살기 / 급소 red, 무쌍 purple, 기합 / 견수 blue; 교대 green, 판정승
// gold, 무승부 grey) with a black rim and a white sticker edge, tilted kStickerTilt, popping in (kPopFrom -> 0.92 over kPopMs, back
// to 1 over kSettleMs), held, shrinking away over kOutMs; gone kStickerMs after the tick. Each kind is made once per device, one kind
// a Present while the window or a sticker shows (a sticker took 49 ms to make in local302: nine in one frame would stall it);
// sticker_live holds a bit per kind made (duel_manual keeps the caption for a kind not made yet), a failed build sets sticker_dead.
// local306 (user 「크리티컬이나 자동발동견수 퇴각 … 삼11 코드로 확인해서」): the stances' criticals 「연속 공격!」 (orange), 「자동 견수!」
// (teal), 「투지 상승!」 (amber) and 「퇴각!」 (slate). local312 (user 「낮은 확률로 … 최대 35합까지」): 「연장!」 (crimson, centre) when the 합
// cap's roll adds 합. 14 kinds: kind + 1 still fits the state's 4 bits.
enum StickerKind:int {SJoin=0,SSwap,SSpecial,SCrit,SMusou,SKiai,SGuard,SWin,SDraw,SCombo,SAutoGuard,SSpirit,SRetreat,SExtend,SKinds};
static const int kStickerMs=1600,kPopMs=180,kSettleMs=110,kOutMs=150,kBurstPoints=16,kBurstSeed=23,kRim=6,kWhiteEdge=5;
static const float kStickerTilt=-0.14f,kPopFrom=1.7f,kStickerX[3]={500.0f,1420.0f,960.0f},kStickerY=330.0f;
static const int kStickerShift=13;   // the tick's place in the state
static volatile LONG64 sticker;   // 0 none, else 1 | (kind + 1) left << 1 | right << 5 | centre << 9 (4 bits each, 0 none) | GetTickCount64 at the show << 13
static volatile LONG sticker_dead;
static volatile LONG sticker_live;   // bit per kind made on the current device
static Tex sticker_tex[SKinds];
static int sticker_wh[SKinds][2];
static int sticker_made;   // Present thread: kinds made in order (0 .. SKinds)
// local313 연장 badge (user 「연장은 조금 크기 작게해서 우하단에 계속 떠있게하자 5합이니까 (너무 우하단으로 가면 원작 스킵버튼을 가려서
// 안가릴정도의 위치로)」, 「반듯한 네모 느낌으로 감싸줄수 잇어? 정돈된 느낌을 주는게 좋을거 같아. 계속 떠있을거니까」): duel_manual publishes it
// when the 합 cap's roll adds 합 (publish_badge) and clears it with the scene. An upright rounded plate (crimson, a cream line, a black
// rim) with 「연장 +5합」 in the window's face (MBadgeExtend), in a place of its own at the right above the original skip buttons (they
// end near x 1876, top y 980 of the letterboxed 1920 x 1080 UI), so the round stickers never replace it; it pops in and stays.
static const float kBadgeRight=1876.0f,kBadgeBottom=950.0f,kBadgePopFrom=1.35f;
static const int kBadgePadX=20,kBadgePadY=9,kBadgeRadius=8,kBadgeLine=3,kBadgeRim=3;
static volatile LONG64 badge;   // 0 none, else GetTickCount64 at the show | 1
static volatile LONG badge_live;   // the plate is made on the current device
static Tex badge_tex;
static int badge_wh[2];
static bool badge_failed;   // Present thread: one try a device
static void release_window_assets() {
    for(auto &r:slot_plate)for(Tex &x:r)drop_tex(x);
    for(auto &r:opt_plate)for(Tex &x:r)drop_tex(x);
    drop_tex(ok_plate);drop_tex(ink_panel);drop_tex(strip_ink);
    for(Tex &x:text_tex)drop_tex(x);
    drop_names();
    for(int k=0;k<SKinds;++k){drop_tex(sticker_tex[k]);sticker_wh[k][0]=sticker_wh[k][1]=0;}
    sticker_made=0;InterlockedExchange(&sticker_live,0);
    drop_tex(badge_tex);badge_wh[0]=badge_wh[1]=0;badge_failed=false;InterlockedExchange(&badge_live,0);
    win_built=false;
}
static bool make_tex(ID3D11Device *d,int W,int H,const unsigned *px,Tex &out) {
    D3D11_TEXTURE2D_DESC td{};
    td.Width=static_cast<UINT>(W);td.Height=static_cast<UINT>(H);td.MipLevels=td.ArraySize=1;
    td.Format=srgb?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;
    td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd{};sd.pSysMem=px;sd.SysMemPitch=static_cast<UINT>(W*4);
    return SUCCEEDED(d->CreateTexture2D(&td,&sd,&out.t)) && SUCCEEDED(d->CreateShaderResourceView(out.t,nullptr,&out.v));
}
static void rgb_of(Rgb x,float c[3]) {c[0]=static_cast<float>(x.r)/255.0f;c[1]=static_cast<float>(x.g)/255.0f;c[2]=static_cast<float>(x.b)/255.0f;}
static unsigned pack(const float c[3],float a) {   // straight colour and alpha -> premultiplied RGBA
    return byte_of(c[0]*a)|(byte_of(c[1]*a)<<8)|(byte_of(c[2]*a)<<16)|(static_cast<unsigned>(byte_of(a))<<24);
}
struct Rnd {double s;double operator()(){s=std::sin(s*9301.0+49297.0)*233280.0;return s-std::floor(s);}};   // the artifact's rnd()
// Coverage of a closed polygon (even-odd) on a W x H grid: 4 sub-rows a pixel row, exact span coverage along each.
static void fill_poly(const float *px,const float *py,int n,int W,int H,float *cov) {
    float xs[64];
    for(int y=0;y<H;++y)for(int sub=0;sub<4;++sub) {
        const float yc=static_cast<float>(y)+(static_cast<float>(sub)+0.5f)*0.25f;
        int m=0;
        for(int i=0;i<n && m<64;++i) {
            const int j=(i+1)%n;
            const float ya=py[i],yb=py[j];
            if((ya<=yc && yb>yc) || (yb<=yc && ya>yc))xs[m++]=px[i]+(yc-ya)/(yb-ya)*(px[j]-px[i]);
        }
        std::sort(xs,xs+m);
        for(int k=0;k+1<m;k+=2) {
            const float a=xs[k]<0.0f?0.0f:xs[k],b=xs[k+1]>static_cast<float>(W)?static_cast<float>(W):xs[k+1];
            for(int x=static_cast<int>(a);x<W && static_cast<float>(x)<b;++x) {
                const float l=static_cast<float>(x)>a?static_cast<float>(x):a,r=static_cast<float>(x+1)<b?static_cast<float>(x+1):b;
                if(r>l)cov[y*W+x]+=(r-l)*0.25f;
            }
        }
    }
}
static float seg_dist2(float x,float y,float ax,float ay,float bx,float by) {   // squared
    const float dx=bx-ax,dy=by-ay,l=dx*dx+dy*dy;
    float t=l>0.0f?((x-ax)*dx+(y-ay)*dy)/l:0.0f;t=t<0.0f?0.0f:t>1.0f?1.0f:t;
    const float ex=ax+t*dx-x,ey=ay+t*dy-y;return ex*ex+ey*ey;
}
static float seg_dist(float x,float y,float ax,float ay,float bx,float by) {return std::sqrt(seg_dist2(x,y,ax,ay,bx,by));}
// The artifact's brush(x, y, w, h, tone, seed) at the origin, kPlatePad around it.
static bool make_plate(ID3D11Device *d,int w,int h,int tone,int seed,float alpha,Tex &out) {
    Rnd r{static_cast<double>(seed)};
    float px[40],py[40];int n=0;
    const float fw=static_cast<float>(w),fh=static_cast<float>(h),p=static_cast<float>(kPlatePad);
    auto pt=[&](float x,float y){px[n]=x+p;py[n]=y+p;++n;};
    {const float x=static_cast<float>(r()*14.0),y=static_cast<float>(r()*4.0);pt(x,y);}
    for(int i=1;i<=10;++i){const float x=fw*static_cast<float>(i)/10.0f-static_cast<float>(r()*6.0),y=static_cast<float>(r()*5.0);pt(x,y);}
    for(int i=0;i<=6;++i)pt(fw-static_cast<float>(r()*16.0)+(i%2?8.0f:0.0f),fh*static_cast<float>(i)/6.0f);
    for(int i=10;i>=0;--i){const float x=fw*static_cast<float>(i)/10.0f+static_cast<float>(r()*6.0),y=fh-static_cast<float>(r()*5.0);pt(x,y);}
    for(int i=6;i>=0;--i)pt(static_cast<float>(r()*18.0)-(i%2?6.0f:0.0f),fh*static_cast<float>(i)/6.0f);
    const int W=w+2*kPlatePad,H=h+2*kPlatePad;
    std::vector<float> cov(static_cast<size_t>(W*H),0.0f);
    fill_poly(px,py,n,W,H,cov.data());
    float c0[3],c1[3];rgb_of(kTone[tone][0],c0);rgb_of(kTone[tone][1],c1);
    std::vector<float> col(static_cast<size_t>(W*H*3));
    for(int y=0;y<H;++y)for(int x=0;x<W;++x) {
        const float u=static_cast<float>(x)+0.5f-p,v=static_cast<float>(y)+0.5f-p;
        float t=(u*fw+v*fh)/(fw*fw+fh*fh);t=t<0.0f?0.0f:t>1.0f?1.0f:t;
        for(int k=0;k<3;++k)col[(y*W+x)*3+k]=t<0.55f?c0[k]+(c1[k]-c0[k])*(t/0.55f):c1[k]+(c0[k]-c1[k])*((t-0.55f)/0.45f);
    }
    float lw=1.0f;
    auto streak=[&](float ax,float ay,float bx,float by,float a,float to) {   // a line of width lw blended towards `to` (0 / 1) at a
        const float lo=(ay<by?ay:by)+p-lw-1.0f,hi=(ay>by?ay:by)+p+lw+1.0f;
        const int row0=lo<0.0f?0:static_cast<int>(lo),row1=hi>static_cast<float>(H)?H:static_cast<int>(hi)+1;
        for(int y=row0;y<row1;++y)for(int x=0;x<W;++x) {
            const float dd=seg_dist(static_cast<float>(x)+0.5f,static_cast<float>(y)+0.5f,ax+p,ay+p,bx+p,by+p);
            float c=lw*0.5f+0.5f-dd;
            if(c<=0.0f)continue;
            c=(c>1.0f?1.0f:c)*a;
            for(int k=0;k<3;++k){float &q=col[(y*W+x)*3+k];q=q+(to-q)*c;}
        }
    };
    for(int i=0;i<14;++i) {
        lw=1.0f+static_cast<float>(r()*2.0);
        const float yy=static_cast<float>(r()*fh),end=yy+static_cast<float>((r()-0.5)*6.0);
        streak(0.0f,yy,fw,end,0.18f,0.0f);
    }
    for(int i=0;i<6;++i) {
        const float yy=static_cast<float>(r()*fh*0.5),end=yy+static_cast<float>((r()-0.5)*4.0);
        streak(fw*0.1f,yy,fw*0.9f,end,0.12f,1.0f);
    }
    std::vector<unsigned> out_px(static_cast<size_t>(W*H));
    for(int i=0;i<W*H;++i){const float a=(cov[i]>1.0f?1.0f:cov[i])*alpha;out_px[i]=pack(&col[i*3],a);}
    return make_tex(d,W,H,out_px.data(),out);
}
// The artifact's ink(0, 520, 520, 560) at half size: 26 rotated ellipses of rgb(10, 7, 5), radial alpha 0.55 / 0.35 (0.7) / 0
// (local305: also the strip's 520 x 320).
static bool make_ink(ID3D11Device *d,int pw,int ph,Tex &out) {
    const int W=pw/kInkHalf,H=ph/kInkHalf;
    Rnd r{static_cast<double>(kInkSeed)};
    std::vector<float> acc(static_cast<size_t>(W*H),0.0f);
    const float w=static_cast<float>(pw),h=static_cast<float>(ph);
    for(int k=0;k<26;++k) {
        const float cx=w*(0.15f+0.7f*static_cast<float>(r())),cy=h*(0.1f+0.8f*static_cast<float>(r()));
        const float rx=w*(0.25f+0.25f*static_cast<float>(r())),ry=h*(0.12f+0.12f*static_cast<float>(r()));
        const float rot=static_cast<float>((r()-0.5)*0.6),cs=std::cos(rot),sn=std::sin(rot),big=rx>ry?rx:ry;
        for(int y=0;y<H;++y)for(int x=0;x<W;++x) {
            const float u=(static_cast<float>(x)+0.5f)*kInkHalf-cx,v=(static_cast<float>(y)+0.5f)*kInkHalf-cy;
            const float eu=(u*cs+v*sn)/rx,ev=(-u*sn+v*cs)/ry;
            if(eu*eu+ev*ev>1.0f)continue;
            const float t=std::sqrt(u*u+v*v)/big;
            const float a=t<0.7f?0.55f+(0.35f-0.55f)*(t/0.7f):t<1.0f?0.35f*(1.0f-(t-0.7f)/0.3f):0.0f;
            float &A=acc[static_cast<size_t>(y*W+x)];A=a+A*(1.0f-a);
        }
    }
    float c[3];rgb_of(Rgb{10,7,5},c);
    std::vector<unsigned> px(static_cast<size_t>(W*H));
    for(int y=0;y<H;++y)for(int x=0;x<W;++x) {   // the top and right edges fade out (the panel's own edges, not the screen's)
        float ft=(static_cast<float>(y)+0.5f)*kInkHalf/kInkFade,fr=(w-(static_cast<float>(x)+0.5f)*kInkHalf)/kInkFade;
        ft=ft>1.0f?1.0f:ft;fr=fr>1.0f?1.0f:fr<0.0f?0.0f:fr;
        const float k=ft*ft*(3.0f-2.0f*ft)*fr*fr*(3.0f-2.0f*fr);
        px[static_cast<size_t>(y*W+x)]=pack(c,acc[static_cast<size_t>(y*W+x)]*k);
    }
    return make_tex(d,W,H,px.data(),out);
}
static bool decode_mask(const char *rle,float *a,int n) {   // duel_window_font.h rle into n levels / 15
    int i=0;
    for(const char *s=rle;*s && i<=n;) {
        if(*s=='z' || *s=='f') {
            const float level=*s=='z'?0.0f:1.0f;
            int count=0;
            for(++s;*s && *s!='.';++s)count=count*36+base36(*s);
            if(*s)++s;
            while(count-->0 && i<n)a[i++]=level;
        }
        else {if(i<n)a[i]=static_cast<float>(base36(*s))/15.0f;++i;++s;}
    }
    return i==n;
}
// A text mask coloured: the fill in `fill`, the stroke (edge minus fill) black at 0.8; brush digits: a brown stroke at 1 over an
// orange glow (the edge blurred, sigma about 5.5 as the artifact's shadowBlur 11, at 0.7).
static bool make_text(ID3D11Device *d,int m,Rgb fill,bool brush,Tex &out) {
    const duel_window_font::Mask &mk=duel_window_font::kMasks[m];
    const int W=mk.w,H=mk.h,n=W*H;
    std::vector<float> f(static_cast<size_t>(n)),e(static_cast<size_t>(n)),g(static_cast<size_t>(n)),t(static_cast<size_t>(n));
    if(!decode_mask(mk.fill,f.data(),n) || !decode_mask(mk.edge,e.data(),n))return false;
    float fc[3],sc3[3]={0.0f,0.0f,0.0f},gc[3]={1.0f,0.549f,0.157f};
    rgb_of(fill,fc);
    float stroke_a=0.8f;
    if(brush) {
        rgb_of(Rgb{58,22,8},sc3);stroke_a=1.0f;
        box_pass(e.data(),g.data(),W,H,true,5);box_pass(g.data(),t.data(),W,H,true,5);box_pass(t.data(),g.data(),W,H,true,5);
        box_pass(g.data(),t.data(),W,H,false,5);box_pass(t.data(),g.data(),W,H,false,5);box_pass(g.data(),t.data(),W,H,false,5);
    }
    std::vector<unsigned> px(static_cast<size_t>(n));
    for(int i=0;i<n;++i) {
        const float fi=f[static_cast<size_t>(i)],ei=e[static_cast<size_t>(i)]*stroke_a,gl=brush?t[static_cast<size_t>(i)]*0.7f:0.0f;
        const float under=ei+(1.0f-ei)*gl,a=fi+(1.0f-fi)*under;
        unsigned char c[3];
        for(int k=0;k<3;++k)c[k]=byte_of(fi*fc[k]+(1.0f-fi)*(ei*sc3[k]+(1.0f-ei)*gl*gc[k]));
        px[static_cast<size_t>(i)]=c[0]|(c[1]<<8)|(c[2]<<16)|(static_cast<unsigned>(byte_of(a))<<24);
    }
    return make_tex(d,W,H,px.data(),out);
}
// make_text's colouring for coverage made at run time: the fill in `fill` over the edge in black at 0.8.
static bool compose_text(ID3D11Device *d,int W,int H,const float *f,const float *e,Rgb fill,Tex &out) {
    float fc[3];rgb_of(fill,fc);
    std::vector<unsigned> px(static_cast<size_t>(W*H));
    for(int i=0;i<W*H;++i) {
        const float fi=f[i],a=fi+(1.0f-fi)*e[i]*0.8f;
        px[static_cast<size_t>(i)]=byte_of(fi*fc[0])|(byte_of(fi*fc[1])<<8)|(byte_of(fi*fc[2])<<16)|(static_cast<unsigned>(byte_of(a))<<24);
    }
    return make_tex(d,W,H,px.data(),out);
}
static void disk_max(const float *in,float *out,int W,int H,int r) {   // max over a disk of radius r (a round stroke)
    for(int y=0;y<H;++y)for(int x=0;x<W;++x) {
        float v=0.0f;
        for(int dy=-r;dy<=r;++dy) {
            const int yy=y+dy;
            if(yy<0 || yy>=H)continue;
            const int span=static_cast<int>(std::sqrt(static_cast<float>(r*r-dy*dy))+0.5f);
            for(int dx=-span;dx<=span;++dx){const int xx=x+dx;if(xx>=0 && xx<W && in[yy*W+xx]>v)v=in[yy*W+xx];}
        }
        out[y*W+x]=v;
    }
}
// local301: a text known only at run time (a member's name) as fill / edge coverage: GDI draws it white on black in Malgun Gothic bold
// with grayscale anti-aliasing (ANTIALIASED_QUALITY: no ClearType fringes), the edge = the fill dilated by `stroke`; mid = the glyphs'
// middle from the bitmap top (ascent - 0.4 em, Hangul sits there). False: empty text or GDI failed.
static bool gdi_text(const wchar_t *text,int px,int stroke,std::vector<float> &f,std::vector<float> &e,int &W,int &H,int &mid) {
    const int len=lstrlenW(text);
    if(len<=0)return false;
    HDC dc=CreateCompatibleDC(nullptr);
    if(!dc)return false;
    HFONT font=CreateFontW(-px,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,HANGUL_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
    HGDIOBJ old_font=font?SelectObject(dc,font):nullptr;
    SIZE sz{};TEXTMETRICW tm{};
    bool ok=font && GetTextExtentPoint32W(dc,text,len,&sz) && GetTextMetricsW(dc,&tm) && sz.cx>0 && sz.cy>0 && sz.cx<1200 && sz.cy<200;
    const int pad=stroke+2;
    W=ok?static_cast<int>(sz.cx)+2*pad:0;H=ok?static_cast<int>(sz.cy)+2*pad:0;
    mid=ok?pad+static_cast<int>(tm.tmAscent)-static_cast<int>(static_cast<float>(px)*0.4f+0.5f):0;
    void *bits=nullptr;HBITMAP bmp=nullptr;HGDIOBJ old_bmp=nullptr;
    if(ok) {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize=sizeof bi.bmiHeader;bi.bmiHeader.biWidth=W;bi.bmiHeader.biHeight=-H;bi.bmiHeader.biPlanes=1;
        bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
        bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
        ok=bmp && bits;
    }
    if(ok) {
        old_bmp=SelectObject(dc,bmp);
        memset(bits,0,static_cast<size_t>(W)*static_cast<size_t>(H)*4);
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
        ok=TextOutW(dc,pad,pad,text,len)!=FALSE;
        GdiFlush();
    }
    if(ok) {
        const unsigned char *p=static_cast<const unsigned char *>(bits);
        const size_t n=static_cast<size_t>(W)*static_cast<size_t>(H);
        f.assign(n,0.0f);e.assign(n,0.0f);
        for(size_t i=0;i<n;++i)f[i]=static_cast<float>(p[i*4+1])/255.0f;
        disk_max(f.data(),e.data(),W,H,stroke);
    }
    if(old_bmp)SelectObject(dc,old_bmp);
    if(bmp)DeleteObject(bmp);
    if(old_font)SelectObject(dc,old_font);
    if(font)DeleteObject(font);
    DeleteDC(dc);
    return ok;
}
static const Rgb kBright=Rgb{246,234,210},kDimText=Rgb{125,114,102},kBracket=Rgb{141,127,106},kHeadText=Rgb{233,217,184},kNowText=Rgb{205,185,143};
static const Rgb kStReady=Rgb{255,214,102},kStGuard=Rgb{154,216,255},kStFoe=Rgb{255,160,140};   // local306 status line
static unsigned build_window_assets(ID3D11Device *d) {   // 0 ok, else the step that failed
    namespace wf=duel_window_font;
    for(int i=0;i<4;++i)for(int k=0;k<2;++k)if(!make_plate(d,kPlateW,kPlateH,k?TLit:TDark,kSlotSeed+i,1.0f,slot_plate[i][k]))return 1;
    for(int j=0;j<6;++j)for(int t=0;t<TTones;++t)if(!make_plate(d,kPlateW,kPlateH,t,kOptSeed+j*3,t==TDim?0.75f:1.0f,opt_plate[j][t]))return 2;
    if(!make_plate(d,kPlateW,kPlateH,TGold,kOkSeed,1.0f,ok_plate))return 3;
    if(!make_ink(d,kWinW,kWinH,ink_panel) || !make_ink(d,kStripPanel[2],kStripPanel[3],strip_ink))return 4;
    bool ok=true;
    for(int i=0;i<12;++i)ok=ok && make_text(d,wf::MHead0+i,kHeadText,false,text_tex[GHead+i]);
    for(int i=0;i<4;++i)ok=ok && make_text(d,wf::MNow0+i,kNowText,false,text_tex[GNow+i]);
    for(int i=0;i<11;++i)ok=ok && make_text(d,wf::MCmd0+i,kBright,false,text_tex[GCmd+i]) && make_text(d,wf::MCmd0+i,kDimText,false,text_tex[GCmdDim+i]);
    ok=ok && make_text(d,wf::MSpecial,kBright,false,text_tex[GSpecial]) && make_text(d,wf::MSpecial,kDimText,false,text_tex[GSpecialDim]) &&
       make_text(d,wf::MBack,kBright,false,text_tex[GBack]) && make_text(d,wf::MNoMember,kBracket,false,text_tex[GNoMember]) &&
       make_text(d,wf::MSwapUsed,kBracket,false,text_tex[GSwapUsed]) && make_text(d,wf::MOk,Rgb{255,244,214},false,text_tex[GOk]) &&
       make_text(d,wf::MAuto,kHeadText,false,text_tex[GAuto]) && make_text(d,wf::MHint,Rgb{168,152,126},false,text_tex[GHint]);
    // local306: 「투지 부족」, the 퇴각 hint, the status line (labels in their colours, digits in the header's)
    ok=ok && make_text(d,wf::MLack,kBracket,false,text_tex[GLack]) && make_text(d,wf::MRetHint,Rgb{168,152,126},false,text_tex[GRetHint]) &&
       make_text(d,wf::MStCrit,kHeadText,false,text_tex[GStCrit]) && make_text(d,wf::MStReady,kStReady,false,text_tex[GStReady]) &&
       make_text(d,wf::MStOf6,kHeadText,false,text_tex[GStOf6]) && make_text(d,wf::MStGuard,kStGuard,false,text_tex[GStGuard]) &&
       make_text(d,wf::MStFoe,kStFoe,false,text_tex[GStFoe]) && make_text(d,wf::MStHap,kHeadText,false,text_tex[GStHap]);
    for(int i=0;i<10;++i)ok=ok && make_text(d,wf::MSt0+i,kHeadText,false,text_tex[GStD0+i]);
    return ok?0u:5u;
}
// local301: member i's name and 「name  체력 N」 from a published set (an empty name: no texture). Returns the textures made.
static unsigned build_names(ID3D11Device *d,const NameSet &ns) {
    drop_names();
    unsigned made=0;
    for(int i=0;i<3;++i) {
        wchar_t nm[kNameCap];
        memcpy(nm,ns.name[i],sizeof nm);nm[kNameCap-1]=0;
        if(!nm[0])continue;
        wchar_t lb[kNameCap+24];
        swprintf_s(lb,L"%ls  체력 %d",nm,ns.hp[i]);
        std::vector<float> f,e;
        int W=0,H=0,mid=0;
        if(gdi_text(nm,kNamePx,kNameStroke,f,e,W,H,mid) && compose_text(d,W,H,f.data(),e.data(),kBright,name_tex[i]))
            {name_wh[i][0]=W;name_wh[i][1]=H;name_wh[i][2]=mid;++made;}
        if(gdi_text(lb,kLabelPx,kNameStroke,f,e,W,H,mid) && compose_text(d,W,H,f.data(),e.data(),kBright,label_tex[i]))
            {label_wh[i][0]=W;label_wh[i][1]=H;label_wh[i][2]=mid;++made;}
    }
    return made;
}
static float tex_sample(const std::vector<float> &a,int W,int H,float u,float v) {   // bilinear, zero outside (u, v in pixel units)
    const float x=u-0.5f,y=v-0.5f;
    const int x0=static_cast<int>(std::floor(x)),y0=static_cast<int>(std::floor(y));
    const float fx=x-static_cast<float>(x0),fy=y-static_cast<float>(y0);
    auto at=[&](int xx,int yy){return xx<0 || yy<0 || xx>=W || yy>=H?0.0f:a[static_cast<size_t>(yy*W+xx)];};
    return (at(x0,y0)*(1.0f-fx)+at(x0+1,y0)*fx)*(1.0f-fy)+(at(x0,y0+1)*(1.0f-fx)+at(x0+1,y0+1)*fx)*fy;
}
static bool make_sticker(ID3D11Device *d,int kind) {
    namespace wf=duel_window_font;
    static const int kWord[SKinds]={wf::MJoin,wf::MStkSwap,wf::MStkSpecial,wf::MStkCrit,wf::MStkMusou,wf::MStkKiai,wf::MStkGuard,wf::MStkWin,wf::MStkDraw,
                                    wf::MStkCombo,wf::MStkAutoGuard,wf::MStkSpirit,wf::MStkRetreat,wf::MStkExtend};
    static const float kBurst[SKinds][2][3]={   // the burst's centre -> its spikes
        {{1.0f,0.749f,0.243f},{0.851f,0.157f,0.106f}},   // 난입 #ffbf3e -> #d9281b (local301)
        {{0.784f,0.941f,0.431f},{0.122f,0.561f,0.235f}},   // 교대 #c8f06e -> #1f8f3c
        {{1.0f,0.541f,0.4f},{0.659f,0.078f,0.11f}},   // 필살기 #ff8a66 -> #a8141c
        {{1.0f,0.541f,0.4f},{0.659f,0.078f,0.11f}},   // 급소
        {{0.906f,0.651f,1.0f},{0.42f,0.133f,0.722f}},   // 무쌍 #e7a6ff -> #6b22b8
        {{0.604f,0.847f,1.0f},{0.122f,0.345f,0.761f}},   // 기합 #9ad8ff -> #1f58c2
        {{0.604f,0.847f,1.0f},{0.122f,0.345f,0.761f}},   // 견수
        {{1.0f,0.953f,0.541f},{0.851f,0.604f,0.024f}},   // 판정승 #fff38a -> #d99a06
        {{0.949f,0.949f,0.949f},{0.435f,0.471f,0.518f}},   // 무승부 #f2f2f2 -> #6f7884
        {{1.0f,0.824f,0.478f},{0.878f,0.337f,0.106f}},   // 연속 공격 #ffd27a -> #e0561b (local306)
        {{0.604f,0.941f,0.878f},{0.122f,0.561f,0.612f}},   // 자동 견수 #9af0e0 -> #1f8f9c
        {{1.0f,0.886f,0.541f},{0.78f,0.49f,0.024f}},   // 투지 상승 #ffe28a -> #c77d06
        {{0.851f,0.824f,0.914f},{0.294f,0.247f,0.42f}},   // 퇴각 #d9d2e9 -> #4b3f6b
        {{1.0f,0.761f,0.839f},{0.761f,0.122f,0.357f}}};   // 연장 #ffc2d6 -> #c21f5b (local312)
    if(kind<0 || kind>=SKinds)return false;
    const wf::Mask &mk=wf::kMasks[kWord[kind]];
    const int mw=mk.w,mh=mk.h,tn=mw*mh;
    std::vector<float> tf(static_cast<size_t>(tn)),te(static_cast<size_t>(tn));
    if(!decode_mask(mk.fill,tf.data(),tn) || !decode_mask(mk.edge,te.data(),tn))return false;
    const float rx=static_cast<float>(mw)*0.66f+16.0f,ry=static_cast<float>(mh)*0.74f+14.0f;   // the burst's spike radii
    const float cs=std::cos(kStickerTilt),sn=std::sin(kStickerTilt),edge=static_cast<float>(kRim+kWhiteEdge);
    // the tilted ellipse's half extents, 1.1 x for the longest spike (0.97 + 0.12 of the radius; local303: a kind's own seed reached it)
    const float hx=1.1f*std::sqrt(rx*rx*cs*cs+ry*ry*sn*sn),hy=1.1f*std::sqrt(rx*rx*sn*sn+ry*ry*cs*cs);
    const int W=static_cast<int>(2.0f*(hx+edge+4.0f)),H=static_cast<int>(2.0f*(hy+edge+4.0f));
    const float cx=static_cast<float>(W)*0.5f,cy=static_cast<float>(H)*0.5f;
    Rnd r{static_cast<double>(kBurstSeed+kind*7)};
    float px[kBurstPoints*2],py[kBurstPoints*2];
    for(int k=0;k<kBurstPoints*2;++k) {   // spikes and valleys around an ellipse, a little uneven like a hand-cut sticker
        const float a=6.2831853f*(static_cast<float>(k)+static_cast<float>(r()*0.3))/static_cast<float>(kBurstPoints*2);
        const float s=k%2==0?0.97f+static_cast<float>(r()*0.12):0.76f+static_cast<float>(r()*0.06);
        const float lx=std::cos(a)*rx*s,ly=std::sin(a)*ry*s;
        px[k]=cx+lx*cs-ly*sn;py[k]=cy+lx*sn+ly*cs;
    }
    std::vector<float> cov(static_cast<size_t>(W*H),0.0f);
    fill_poly(px,py,kBurstPoints*2,W,H,cov.data());
    const float cream[3]={1.0f,0.957f,0.839f},black[3]={0.0f,0.0f,0.0f},white[3]={1.0f,1.0f,1.0f};
    const float *hot=kBurst[kind][0],*red=kBurst[kind][1];
    std::vector<unsigned> out(static_cast<size_t>(W*H));
    for(int y=0;y<H;++y)for(int x=0;x<W;++x) {
        const size_t i=static_cast<size_t>(y*W+x);
        const float fxp=static_cast<float>(x)+0.5f,fyp=static_cast<float>(y)+0.5f;
        float dist=0.0f;   // outside the burst: the distance to its outline (rim and sticker edge)
        if(cov[i]<0.5f){float d2=1e18f;for(int k=0;k<kBurstPoints*2;++k){const int j=(k+1)%(kBurstPoints*2);const float dd=seg_dist2(fxp,fyp,px[k],py[k],px[j],py[j]);if(dd<d2)d2=dd;}dist=std::sqrt(d2);}
        auto clamp01=[](float v){return v<0.0f?0.0f:v>1.0f?1.0f:v;};
        const float wa=clamp01(edge+0.5f-dist),ra=clamp01(static_cast<float>(kRim)+0.5f-dist),ba=clamp01(cov[i]);
        const float dx=fxp-cx,dy=fyp-cy,u=dx*cs+dy*sn,v=-dx*sn+dy*cs;   // the canvas point in the text's (untilted) frame
        const float t=clamp01(std::sqrt((u/rx)*(u/rx)+(v/ry)*(v/ry))*1.15f);
        float bc[3];
        for(int k=0;k<3;++k)bc[k]=hot[k]+(red[k]-hot[k])*t;
        const float tu=u+static_cast<float>(mw)*0.5f,tv=v+static_cast<float>(mh)*0.5f-4.0f;
        const float fa=tex_sample(tf,mw,mh,tu,tv),ea=tex_sample(te,mw,mh,tu,tv);
        float pr[3]={0.0f,0.0f,0.0f},pa=0.0f;   // premultiplied, source-over from the back
        auto over=[&](const float c3[3],float a){for(int k=0;k<3;++k)pr[k]=c3[k]*a+pr[k]*(1.0f-a);pa=a+pa*(1.0f-a);};
        over(white,wa);over(black,ra);over(bc,ba);over(black,ea);over(cream,fa);
        out[i]=byte_of(pr[0])|(byte_of(pr[1])<<8)|(byte_of(pr[2])<<16)|(static_cast<unsigned>(byte_of(pa))<<24);
    }
    sticker_wh[kind][0]=W;sticker_wh[kind][1]=H;
    return make_tex(d,W,H,out.data(),sticker_tex[kind]);
}
static float sticker_scale(unsigned age) {   // the pop: big to a little small, settle, hold, shrink away
    if(age<static_cast<unsigned>(kPopMs)){const float t=static_cast<float>(age)/static_cast<float>(kPopMs);return kPopFrom+(0.92f-kPopFrom)*(1.0f-(1.0f-t)*(1.0f-t));}
    if(age<static_cast<unsigned>(kPopMs+kSettleMs))return 0.92f+0.08f*static_cast<float>(age-kPopMs)/static_cast<float>(kSettleMs);
    if(age+static_cast<unsigned>(kOutMs)>static_cast<unsigned>(kStickerMs)){const float t=static_cast<float>(kStickerMs-static_cast<int>(age))/static_cast<float>(kOutMs);return t<0.0f?0.0f:t*t;}
    return 1.0f;
}
static bool sticker_broken() {return InterlockedCompareExchange(&sticker_dead,0,0)!=0;}
static bool sticker_ready(int kind) {   // made on Present, usable
    return kind>=0 && kind<SKinds && ((InterlockedCompareExchange(&sticker_live,0,0)>>kind)&1)!=0 && !sticker_broken();
}
static bool sticker_on(LONG64 st,ULONGLONG now) {
    const ULONGLONG t0=static_cast<ULONGLONG>(st)>>kStickerShift;
    return st && now>=t0 && now-t0<static_cast<ULONGLONG>(kStickerMs) && !sticker_broken();
}
// local313: the 연장 badge as premultiplied RGBA: outside in, the black rim, the cream line, the crimson plate (lighter at the top), the
// text's black edge and cream fill, 1:1 at 1080p.
static bool make_badge(ID3D11Device *d) {
    namespace wf=duel_window_font;
    const wf::Mask &mk=wf::kMasks[wf::MBadgeExtend];
    const int mw=mk.w,mh=mk.h,tn=mw*mh;
    std::vector<float> tf(static_cast<size_t>(tn)),te(static_cast<size_t>(tn));
    if(!decode_mask(mk.fill,tf.data(),tn) || !decode_mask(mk.edge,te.data(),tn))return false;
    const int pw=mw+2*kBadgePadX,ph=mh+2*kBadgePadY,W=pw+2*kBadgeRim+2,H=ph+2*kBadgeRim+2;
    const float cx=static_cast<float>(W)*0.5f,cy=static_cast<float>(H)*0.5f,r=static_cast<float>(kBadgeRadius);
    const float hx=static_cast<float>(pw)*0.5f-r,hy=static_cast<float>(ph)*0.5f-r;
    const float cream[3]={1.0f,0.957f,0.839f},line[3]={0.953f,0.863f,0.635f},black[3]={0.0f,0.0f,0.0f};
    const float top[3]={0.737f,0.165f,0.373f},bottom[3]={0.431f,0.059f,0.212f};   // #bc2a5f -> #6e0f36
    auto clamp01=[](float v){return v<0.0f?0.0f:v>1.0f?1.0f:v;};
    const int tx0=(W-mw)/2,ty0=(H-mh)/2;
    std::vector<unsigned> out(static_cast<size_t>(W*H));
    for(int y=0;y<H;++y)for(int x=0;x<W;++x) {
        const float px=std::fabs(static_cast<float>(x)+0.5f-cx)-hx,py=std::fabs(static_cast<float>(y)+0.5f-cy)-hy;
        const float ox=px>0.0f?px:0.0f,oy=py>0.0f?py:0.0f,mx=px>py?px:py,in=mx<0.0f?mx:0.0f;
        const float dist=std::sqrt(ox*ox+oy*oy)+in-r;   // the rounded plate's signed distance (inside < 0)
        const float ra=clamp01(static_cast<float>(kBadgeRim)+0.5f-dist),la=clamp01(0.5f-dist),fa=clamp01(-static_cast<float>(kBadgeLine)+0.5f-dist);
        const float t=clamp01((static_cast<float>(y)+0.5f-static_cast<float>(kBadgeRim+1))/static_cast<float>(ph));
        float bc[3];
        for(int k=0;k<3;++k)bc[k]=top[k]+(bottom[k]-top[k])*t;
        const int u=x-tx0,v=y-ty0;
        const bool on=u>=0 && v>=0 && u<mw && v<mh;
        const float ea=on?te[static_cast<size_t>(v*mw+u)]:0.0f,xa=on?tf[static_cast<size_t>(v*mw+u)]:0.0f;
        float pr[3]={0.0f,0.0f,0.0f},pa=0.0f;   // premultiplied, source-over from the back
        auto over=[&](const float c3[3],float a){for(int k=0;k<3;++k)pr[k]=c3[k]*a+pr[k]*(1.0f-a);pa=a+pa*(1.0f-a);};
        over(black,ra);over(line,la);over(bc,fa);over(black,ea);over(cream,xa);
        out[static_cast<size_t>(y*W+x)]=byte_of(pr[0])|(byte_of(pr[1])<<8)|(byte_of(pr[2])<<16)|(static_cast<unsigned>(byte_of(pa))<<24);
    }
    badge_wh[0]=W;badge_wh[1]=H;
    return make_tex(d,W,H,out.data(),badge_tex);
}
static float badge_scale(unsigned age) {   // the sticker's pop, softer, then it stays
    if(age<static_cast<unsigned>(kPopMs)){const float t=static_cast<float>(age)/static_cast<float>(kPopMs);return kBadgePopFrom+(0.95f-kBadgePopFrom)*(1.0f-(1.0f-t)*(1.0f-t));}
    if(age<static_cast<unsigned>(kPopMs+kSettleMs))return 0.95f+0.05f*static_cast<float>(age-kPopMs)/static_cast<float>(kSettleMs);
    return 1.0f;
}
static bool badge_ready() {return InterlockedCompareExchange(&badge_live,0,0)!=0 && !sticker_broken();}
static float snap(float v) {return std::floor(v+0.5f);}   // whole back buffer pixels: a 1:1 text texture stays sharp
static void plate_at(ID3D11DeviceContext *c,const Screen &sc,const Tex &t,int x,int y,int w,int h) {
    const float p=static_cast<float>(kPlatePad);
    quad(c,t.v,snap(sc.ox+(static_cast<float>(x)-p)*sc.s),snap(sc.oy+(static_cast<float>(y)-p)*sc.s),(static_cast<float>(w)+2.0f*p)*sc.s,(static_cast<float>(h)+2.0f*p)*sc.s);
}
static void mask_at(ID3D11DeviceContext *c,const Screen &sc,int g,int m,float pen,float y) {   // pen / em middle in 1920 x 1080 units
    const duel_window_font::Mask &mk=duel_window_font::kMasks[m];
    quad(c,text_tex[g].v,snap(sc.ox+(pen+static_cast<float>(mk.lx))*sc.s),snap(sc.oy+(y+static_cast<float>(mk.ty))*sc.s),
         static_cast<float>(mk.w)*sc.s,static_cast<float>(mk.h)*sc.s);
}
static void text_at(ID3D11DeviceContext *c,const Screen &sc,int g,int m,float x,float y,int align) {   // align 0 left, 1 centre, 2 right
    const float adv=static_cast<float>(duel_window_font::kMasks[m].adv);
    mask_at(c,sc,g,m,align==0?x:align==1?x-adv*0.5f:x-adv,y);
}
// A runtime text texture (build_names) with its glyphs' middle on y; align 0: the ink starts at x, 1: centred on x.
static void runtime_at(ID3D11DeviceContext *c,const Screen &sc,const Tex &t,const int wh[3],float x,float y,int align) {
    if(!t.v || wh[0]<=0)return;
    const float pad=static_cast<float>(kNameStroke+2),left=align==0?x-pad:x-static_cast<float>(wh[0])*0.5f;
    quad(c,t.v,snap(sc.ox+left*sc.s),snap(sc.oy+(y-static_cast<float>(wh[2]))*sc.s),static_cast<float>(wh[0])*sc.s,static_cast<float>(wh[1])*sc.s);
}
static void draw_window(ID3D11DeviceContext *c,const Screen &sc,LONG64 st) {
    namespace wf=duel_window_font;
    const int mode=static_cast<int>((st>>WMode)&7),list=static_cast<int>((st>>WList)&3),next=static_cast<int>((st>>WNext)&127);
    const int now=static_cast<int>((st>>WNow)&3),stance=static_cast<int>((st>>WStance)&3),spc=static_cast<int>((st>>WSpc)&3),swp=static_cast<int>((st>>WSwap)&3);
    const bool autoplay=((st>>WAuto)&1)!=0,okb=((st>>WOk)&1)!=0;
    const int pick=static_cast<int>((st>>WPick)&7)-1;
    if(mode)quad(c,ink_panel.v,sc.ox+static_cast<float>(kWinX)*sc.s,sc.oy+static_cast<float>(kWinY)*sc.s,static_cast<float>(kWinW)*sc.s,static_cast<float>(kWinH)*sc.s);
    else quad(c,strip_ink.v,sc.ox+static_cast<float>(kStripPanel[0])*sc.s,sc.oy+static_cast<float>(kStripPanel[1])*sc.s,
              static_cast<float>(kStripPanel[2])*sc.s,static_cast<float>(kStripPanel[3])*sc.s);
    if(next>0) {   // 「N합」 from the header glyphs, left at 44
        int ids[4],n=0;
        if(next>=10)ids[n++]=wf::MHead0+next/10%10;
        ids[n++]=wf::MHead0+next%10;ids[n++]=wf::MHeadHap;
        float pen=44.0f;
        for(int i=0;i<n;++i){mask_at(c,sc,GHead+ids[i]-wf::MHead0,ids[i],pen,kHeadY);pen+=static_cast<float>(wf::kMasks[ids[i]].adv);}
    }
    text_at(c,sc,GNow+now,wf::MNow0+now,466.0f,kHeadY,2);
    int r[4];
    for(int i=0;i<4;++i) {
        strip_rect(i,r);
        plate_at(c,sc,slot_plate[i][i==stance?1:0],r[0],r[1],r[2],r[3]);
        text_at(c,sc,GCmd+i,wf::MCmd0+i,static_cast<float>(r[0])+107.0f,static_cast<float>(r[1])+27.0f,1);
    }
    strip_rect(4,r);
    plate_at(c,sc,opt_plate[0][spc==1?TGold:spc==2?TLit:TDim],r[0],r[1],r[2],r[3]);
    text_at(c,sc,spc?GSpecial:GSpecialDim,wf::MSpecial,static_cast<float>(r[0])+107.0f,static_cast<float>(r[1])+27.0f,1);
    strip_rect(5,r);
    const int ret=static_cast<int>((st>>WRet)&3);
    if(ret) {   // local306: 퇴각 on the 교대 place (red on, blue pending, dim short of 투지 with 「투지 부족」)
        plate_at(c,sc,opt_plate[1][ret==1?TLit:ret==2?TBlue:TDim],r[0],r[1],r[2],r[3]);
        text_at(c,sc,(ret==3?GCmdDim:GCmd)+10,wf::MCmd10,static_cast<float>(r[0])+107.0f,static_cast<float>(r[1])+27.0f,1);
        if(ret==3)text_at(c,sc,GLack,wf::MLack,static_cast<float>(r[0])+107.0f,static_cast<float>(r[1])+47.0f,1);
    }
    else {
        plate_at(c,sc,opt_plate[1][swp==1?TDark:swp==2?TBlue:TDim],r[0],r[1],r[2],r[3]);
        text_at(c,sc,(swp==1 || swp==2?GCmd:GCmdDim)+9,wf::MCmd9,static_cast<float>(r[0])+107.0f,static_cast<float>(r[1])+27.0f,1);
        if(swp==0)text_at(c,sc,GNoMember,wf::MNoMember,static_cast<float>(r[0])+107.0f,static_cast<float>(r[1])+47.0f,1);
        else if(swp==3)text_at(c,sc,GSwapUsed,wf::MSwapUsed,static_cast<float>(r[0])+107.0f,static_cast<float>(r[1])+47.0f,1);
    }
    strip_rect(6,r);
    plate_at(c,sc,opt_plate[2][autoplay?TLit:TDark],r[0],r[1],r[2],r[3]);
    text_at(c,sc,GAuto,wf::MAuto,static_cast<float>(r[0])+107.0f,static_cast<float>(r[1])+27.0f,1);
    if(okb) {
        strip_rect(7,r);
        plate_at(c,sc,ok_plate,r[0],r[1],r[2],r[3]);
        text_at(c,sc,GOk,wf::MOk,static_cast<float>(r[0])+107.0f,static_cast<float>(r[1])+27.0f,1);
    }
    static const int kSpcTone[6]={TLit,TLit,TPurple,TBlue,TBlue,TBack};
    if(((st>>WStat)&1)!=0) {   // local306 status line under the strip: 「크리티컬 n/6」 or 「크리티컬 가능」, 「견수 n합」, 「적 견수 n합」
        const int crit=static_cast<int>((st>>WCrit)&7),guard=static_cast<int>((st>>WGuard)&15),foe=static_cast<int>((st>>WFoe)&15);
        float pen=44.0f;
        const float y=kStatY;
        auto piece=[&](int g,int m){mask_at(c,sc,g,m,pen,y);pen+=static_cast<float>(wf::kMasks[m].adv);};
        auto number=[&](int v){if(v>=10)piece(GStD0+v/10%10,wf::MSt0+v/10%10);piece(GStD0+v%10,wf::MSt0+v%10);};
        piece(GStCrit,wf::MStCrit);pen+=kStatSpace;
        if(crit>=6)piece(GStReady,wf::MStReady);
        else {number(crit);piece(GStOf6,wf::MStOf6);}
        if(guard>0){pen+=kStatGap;piece(GStGuard,wf::MStGuard);pen+=kStatSpace;number(guard);piece(GStHap,wf::MStHap);}
        if(foe>0){pen+=kStatGap;piece(GStFoe,wf::MStFoe);pen+=kStatSpace;number(foe);piece(GStHap,wf::MStHap);}
    }
    if(list==3) {   // local306 the 퇴각 hold: 퇴각 (picked gold), its hint, 「돌아가기」
        list_rect(0,r);
        plate_at(c,sc,opt_plate[0][pick==0?TGold:TLit],r[0],r[1],r[2],r[3]);
        text_at(c,sc,GCmd+10,wf::MCmd10,static_cast<float>(r[0])+107.0f,static_cast<float>(r[1])+27.0f,1);
        text_at(c,sc,GRetHint,wf::MRetHint,44.0f,static_cast<float>(kListY+kRowStep)+27.0f,0);   // row 1: places 1 and 4 stay empty
        list_rect(5,r);
        plate_at(c,sc,opt_plate[5][TBack],r[0],r[1],r[2],r[3]);
        text_at(c,sc,GBack,wf::MBack,static_cast<float>(r[0])+107.0f,static_cast<float>(r[1])+27.0f,1);
    }
    else if(list==1 || list==2) {
        for(int k=0;k<6;++k) {
            list_rect(k,r);
            const float cx=static_cast<float>(r[0])+107.0f,cy=static_cast<float>(r[1])+27.0f;
            if(k==5){plate_at(c,sc,opt_plate[k][TBack],r[0],r[1],r[2],r[3]);text_at(c,sc,GBack,wf::MBack,cx,cy,1);continue;}
            const bool on=((st>>(WItem+k))&1)!=0;
            if(list==1) {
                const int cmd=kListSpc[k];
                plate_at(c,sc,opt_plate[k][!on?TDim:pick==k?TGold:kSpcTone[k]],r[0],r[1],r[2],r[3]);
                text_at(c,sc,(on?GCmd:GCmdDim)+cmd,wf::MCmd0+cmd,cx,cy,1);
            }
            else {
                const int pm=k<2?static_cast<int>((st>>(WMember+k*2))&3)-1:-1;
                if(pm<0)continue;
                plate_at(c,sc,opt_plate[k][pick==k?TGold:TBlue],r[0],r[1],r[2],r[3]);
                runtime_at(c,sc,label_tex[pm],label_wh[pm],cx,cy,1);
            }
        }
    }
    else if(mode)text_at(c,sc,GHint,wf::MHint,44.0f,static_cast<float>(kListY)+60.0f,0);
}
static bool window_broken() {return InterlockedCompareExchange(&win_dead,0,0)!=0;}
static LONG window_draws() {return InterlockedCompareExchange(&win_draws,0,0);}
static void publish_window(LONG64 state) {InterlockedExchange64(&win,state);}   // UI thread; 0 = nothing
static void publish_names(const NameSet &ns) {   // UI thread, the only writer: a change goes in between two names_seq steps
    if(memcmp(&ns,&names_pub,sizeof ns)==0)return;
    InterlockedIncrement(&names_seq);names_pub=ns;InterlockedIncrement(&names_seq);
}
// The sticker state for a kind (-1 none) on the left half, the right half and the centre, shown at tick; nothing at all = 0.
static LONG64 sticker_state(int left,int right,int centre,ULONGLONG tick) {
    auto k=[](int v){return v>=0 && v<SKinds?static_cast<LONG64>(v+1):0LL;};
    const LONG64 kinds=k(left)|(k(right)<<4)|(k(centre)<<8);
    return kinds?1LL|(kinds<<1)|(static_cast<LONG64>(tick)<<kStickerShift):0;
}
// UI thread: stickers show now (a kind or -1 per place); clear_sticker = none.
static void publish_sticker(int left,int right,int centre) {InterlockedExchange64(&sticker,sticker_state(left,right,centre,GetTickCount64()));}
static void clear_sticker() {InterlockedExchange64(&sticker,0);}
// UI thread (local313): the 연장 badge shows from now until clear_badge (the scene's end).
static void publish_badge() {InterlockedExchange64(&badge,static_cast<LONG64>(GetTickCount64()|1ull));}
static void clear_badge() {InterlockedExchange64(&badge,0);}
static LONG64 window_state(const WinView &v) {
    LONG64 x=1LL<<WOn;
    x|=static_cast<LONG64>(v.mode&7)<<WMode;x|=static_cast<LONG64>(v.list&3)<<WList;x|=static_cast<LONG64>(v.next&127)<<WNext;
    x|=static_cast<LONG64>(v.now&3)<<WNow;x|=static_cast<LONG64>(v.stance&3)<<WStance;x|=static_cast<LONG64>(v.spc&3)<<WSpc;
    x|=static_cast<LONG64>(v.swp&3)<<WSwap;
    if(v.autoplay)x|=1LL<<WAuto;
    if(v.ok)x|=1LL<<WOk;
    for(int k=0;k<6;++k)if(v.item[k])x|=1LL<<(WItem+k);
    if(v.pick>=0 && v.pick<6)x|=static_cast<LONG64>(v.pick+1)<<WPick;
    for(int j=0;j<2;++j)if(v.member[j]>=0 && v.member[j]<3)x|=static_cast<LONG64>(v.member[j]+1)<<(WMember+j*2);
    x|=static_cast<LONG64>(v.ret&3)<<WRet;
    if(v.stat) {
        x|=1LL<<WStat;
        x|=static_cast<LONG64>((v.crit<0?0:v.crit>6?6:v.crit)&7)<<WCrit;
        x|=static_cast<LONG64>((v.guard<0?0:v.guard>15?15:v.guard)&15)<<WGuard;
        x|=static_cast<LONG64>((v.foe<0?0:v.foe>15?15:v.foe)&15)<<WFoe;
    }
    return x;
}
// ---- helper chips (local308 mock C; user 「보조무장 C안으로 하고 메인일기토 무장의 필살기 게이지를 가리지 않도록 조심」, 「지금 초상화만 줄여서
// 만들면 안되나?」) ----
// duel_multi keeps each waiting member's round portrait from its gauge copy (the original draws it, the copy scaled to kChipScale with
// its other parts hidden, placed so the portrait's middle sits on a chip place) and publishes the chips; this draws the HP ring around
// each portrait (a dark track, the HP in the gauge's blue from the top, clockwise) and 「name 무력」 under it (name cream, 무력 gold,
// GDI like build_names). A portrait that did not scale (duel_multi bar_face_off) gets a dark disc instead. Layout (1920 x 1080): the
// rings' tops at kChipCY - kChipOuter stay under the 투지 bars (y 114-122 at the original gauge), the left chips right of the main
// portrait (its disc ends at x 383), the right ones left of the other (from x 1556); row 0 (most HP) next to the main portrait.
static const float kChipScale=0.4f,kChipCY=178.0f,kChipX[2][2]={{436.0f,540.0f},{1484.0f,1380.0f}};   // [half][row]: the portraits' middles
static const float kFaceCX=138.0f,kFaceCY=104.5f,kFaceR=95.0f;   // the portrait disc in its face control at full size (local307 picture)
static const float kRingIn=1.0f,kRingW=5.5f,kRingRim=1.2f,kChipOuter=kFaceR*kChipScale+kRingIn+kRingW+kRingRim,kChipTextGap=14.0f;
static const int kChips=4,kRingTex=104;
static const Rgb kChipPower=Rgb{232,176,74};
struct ChipSet {struct Chip {bool on,face;float cx,cy,r;int hp,power;wchar_t name[kNameCap];} chip[kChips];};
static ChipSet chips_pub;            // UI thread writes between two chips_seq steps (odd while writing)
static volatile LONG chips_seq,chips_live;   // chips_live: chips on in the last set
static LONG chips_built=-1;          // Present thread: the set its textures show
static ChipSet chips_now;            // Present thread: that set
static Tex chip_name[kChips],chip_power[kChips],chip_ring[kChips];
static int chip_name_wh[kChips][3],chip_power_wh[kChips][3];
static unsigned chip_builds;
static void release_chips() {
    for(int i=0;i<kChips;++i){drop_tex(chip_name[i]);drop_tex(chip_power[i]);drop_tex(chip_ring[i]);for(int k=0;k<3;++k)chip_name_wh[i][k]=chip_power_wh[i][k]=0;}
    memset(&chips_now,0,sizeof chips_now);chips_built=-1;
}
static float cover01(float v) {return v<=0.0f?0.0f:v>=1.0f?1.0f:v;}
static bool make_ring(ID3D11Device *d,float r0,int hp,bool face,Tex &out) {   // kRingTex square, 1920 x 1080 units, centred
    static const float kTrack[3]={0.173f,0.204f,0.251f},kHp[3]={0.353f,0.663f,0.902f},kRimC[3]={0.071f,0.055f,0.043f},kDiscC[3]={0.165f,0.125f,0.09f};
    const int T=kRingTex;
    const float mid=static_cast<float>(T)*0.5f,ri=r0+kRingIn,ro=ri+kRingW,frac=static_cast<float>(hp<0?0:hp>100?100:hp)/100.0f,turn=6.2831853f;
    std::vector<unsigned> px(static_cast<size_t>(T*T));
    for(int y=0;y<T;++y)for(int x=0;x<T;++x) {
        const float dx=static_cast<float>(x)+0.5f-mid,dy=static_cast<float>(y)+0.5f-mid,dist=std::sqrt(dx*dx+dy*dy);
        const float band=cover01(dist-ri+0.5f)*cover01(ro-dist+0.5f),rim=cover01(dist-(ri-kRingRim)+0.5f)*cover01(ro+kRingRim-dist+0.5f)*0.85f;
        float ang=std::atan2(dx,-dy);
        if(ang<0.0f)ang+=turn;
        const float lit=frac>=1.0f?1.0f:frac<=0.0f?0.0f:cover01((frac*turn-ang)*dist+0.5f)*cover01(ang*dist+0.5f);
        float pr[3],pa=band+rim*(1.0f-band);
        for(int k=0;k<3;++k)pr[k]=(kTrack[k]+(kHp[k]-kTrack[k])*lit)*band+kRimC[k]*rim*(1.0f-band);
        if(!face) {   // no portrait: a dark disc in its place
            const float fill=cover01(r0-dist+0.5f)*0.92f;
            for(int k=0;k<3;++k)pr[k]+=kDiscC[k]*fill*(1.0f-pa);
            pa+=fill*(1.0f-pa);
        }
        px[static_cast<size_t>(y*T+x)]=byte_of(pr[0])|(byte_of(pr[1])<<8)|(byte_of(pr[2])<<16)|(static_cast<unsigned>(byte_of(pa))<<24);
    }
    return make_tex(d,T,T,px.data(),out);
}
// Present thread: a new published set; texts and rings made again where they changed. duel_manual_chips_build: army texts made | rings
// made << 8 | chips on << 16, value chips_seq, hex ms.
static void chips_sync(ID3D11Device *d) {
    const LONG s1=InterlockedCompareExchange(&chips_seq,0,0);
    if(s1==chips_built || (s1&1))return;
    ChipSet copy;
    memcpy(&copy,&chips_pub,sizeof copy);
    if(InterlockedCompareExchange(&chips_seq,0,0)!=s1)return;
    const ULONGLONG t0=GetTickCount64();
    unsigned texts=0,rings=0,on=0;
    for(int i=0;i<kChips;++i) {
        const ChipSet::Chip &n=copy.chip[i],&o=chips_now.chip[i];
        const bool fresh=chips_built<0;
        if(fresh || o.on!=n.on || o.power!=n.power || wcsncmp(o.name,n.name,kNameCap)!=0) {
            drop_tex(chip_name[i]);drop_tex(chip_power[i]);
            for(int k=0;k<3;++k)chip_name_wh[i][k]=chip_power_wh[i][k]=0;
            if(n.on) {
                wchar_t nm[kNameCap],pw[8];
                memcpy(nm,n.name,sizeof nm);nm[kNameCap-1]=0;
                swprintf_s(pw,L"%d",n.power<0?0:n.power>999?999:n.power);
                std::vector<float> f,e;
                int W=0,H=0,m=0;
                if(nm[0] && gdi_text(nm,kNamePx,kNameStroke,f,e,W,H,m) && compose_text(d,W,H,f.data(),e.data(),kBright,chip_name[i]))
                    {chip_name_wh[i][0]=W;chip_name_wh[i][1]=H;chip_name_wh[i][2]=m;++texts;}
                if(n.power>0 && gdi_text(pw,kNamePx,kNameStroke,f,e,W,H,m) && compose_text(d,W,H,f.data(),e.data(),kChipPower,chip_power[i]))
                    {chip_power_wh[i][0]=W;chip_power_wh[i][1]=H;chip_power_wh[i][2]=m;++texts;}
            }
        }
        if(fresh || o.on!=n.on || o.hp!=n.hp || o.face!=n.face || std::fabs(o.r-n.r)>0.01f || (n.on && !chip_ring[i].v)) {
            drop_tex(chip_ring[i]);
            if(n.on && n.r>4.0f && n.r<kRingTex*0.5f-kRingIn-kRingW-kRingRim-1.0f && make_ring(d,n.r,n.hp,n.face,chip_ring[i]))++rings;
        }
        if(n.on)++on;
    }
    chips_now=copy;chips_built=s1;++chip_builds;
    log_event("duel_manual_chips_build",texts|(rings<<8)|(on<<16),static_cast<unsigned>(s1),static_cast<unsigned>(GetTickCount64()-t0));
}
static void draw_chips(ID3D11DeviceContext *c,const Screen &sc) {
    const float T=static_cast<float>(kRingTex),pad=static_cast<float>(kNameStroke+2);
    for(int i=0;i<kChips;++i) {
        const ChipSet::Chip &ch=chips_now.chip[i];
        if(!ch.on)continue;
        if(chip_ring[i].v)quad(c,chip_ring[i].v,sc.ox+(ch.cx-T*0.5f)*sc.s,sc.oy+(ch.cy-T*0.5f)*sc.s,T*sc.s,T*sc.s);
        const float wn=chip_name_wh[i][0]>0?static_cast<float>(chip_name_wh[i][0])-2.0f*pad:0.0f,wp=chip_power_wh[i][0]>0?static_cast<float>(chip_power_wh[i][0])-2.0f*pad:0.0f;
        const float gap=wn>0.0f && wp>0.0f?5.0f:0.0f,x0=ch.cx-(wn+gap+wp)*0.5f,y=ch.cy+ch.r+kRingIn+kRingW+kChipTextGap;
        runtime_at(c,sc,chip_name[i],chip_name_wh[i],x0,y,0);
        runtime_at(c,sc,chip_power[i],chip_power_wh[i],x0+wn+gap,y,0);
    }
}
static void publish_chips(const ChipSet &cs) {   // UI thread, the only writer (an unchanged set is not published again; zero-fill the set)
    if(memcmp(&cs,&chips_pub,sizeof cs)==0)return;
    InterlockedIncrement(&chips_seq);chips_pub=cs;InterlockedIncrement(&chips_seq);
    LONG n=0;
    for(const ChipSet::Chip &ch:cs.chip)if(ch.on)++n;
    InterlockedExchange(&chips_live,n);
}
static void draw_raw(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc) {
    const LONG64 v=InterlockedCompareExchange64(&shown,0,0),b=InterlockedCompareExchange64(&bars,0,0),wv=InterlockedCompareExchange64(&win,0,0);
    const LONG64 sk=InterlockedCompareExchange64(&sticker,0,0);
    const ULONGLONG now=GetTickCount64();
    const bool sk_on=sticker_on(sk,now);
    const bool chips_on=InterlockedCompareExchange(&chips_live,0,0)!=0;   // local308 helper chips
    const LONG64 bg=InterlockedCompareExchange64(&badge,0,0);   // local313 연장 badge
    if((!v && !b && !wv && !sk_on && !chips_on && !bg) || broken() || !d || !c || !back || !desc.Width || !desc.Height)return;
    if(owner!=d){release_all();owner=d;owner->AddRef();}
    if(!built) {
        srgb=desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || desc.Format==DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
        if(!build(d)) {
            ++build_fails;InterlockedExchange(&dead,1);release_all();
            log_event("duel_manual_number",8,build_fails,static_cast<unsigned>(desc.Format));
            return;
        }
        built=true;
    }
    if(wv && !win_built && !window_broken()) {   // duel_manual_ovwin_build: army 0 built / 8 failed, value the failed step, hex ms
        const ULONGLONG t0=GetTickCount64();
        const unsigned f=build_window_assets(d);
        if(f){InterlockedExchange(&win_dead,1);release_window_assets();}
        else win_built=true;
        win_build_ms=static_cast<unsigned>(GetTickCount64()-t0);
        log_event("duel_manual_ovwin_build",f?8u:0u,f,win_build_ms);
    }
    // local301 duel_manual_sticker_build (made with the window so a round never waits for one; local303 one kind a Present, in kind
    // order): army kind | 8 failed, value w | h << 16, hex ms.
    if((wv || sk_on) && sticker_made<SKinds && !sticker_broken()) {
        const int kind=sticker_made;
        const ULONGLONG t0=GetTickCount64();
        const bool ok=make_sticker(d,kind);
        if(ok){++sticker_made;InterlockedOr(&sticker_live,1L<<kind);}
        else {InterlockedExchange(&sticker_dead,1);for(Tex &x:sticker_tex)drop_tex(x);sticker_made=0;InterlockedExchange(&sticker_live,0);}
        log_event("duel_manual_sticker_build",static_cast<unsigned>(kind)|(ok?0u:8u),static_cast<unsigned>(sticker_wh[kind][0])|(static_cast<unsigned>(sticker_wh[kind][1])<<16),
                  static_cast<unsigned>(GetTickCount64()-t0));
    }
    // local313 duel_manual_badge_build (after the stickers, one Present): army 0 made / 8 failed, value w | h << 16, hex ms.
    else if((wv || bg) && !badge_tex.v && !badge_failed && !sticker_broken()) {
        const ULONGLONG t0=GetTickCount64();
        const bool ok=make_badge(d);
        if(ok)InterlockedExchange(&badge_live,1);
        else {badge_failed=true;drop_tex(badge_tex);}
        log_event("duel_manual_badge_build",ok?0u:8u,static_cast<unsigned>(badge_wh[0])|(static_cast<unsigned>(badge_wh[1])<<16),static_cast<unsigned>(GetTickCount64()-t0));
    }
    const bool window=wv && win_built;
    // local301 duel_manual_ovwin_names (a published name set made into texts): army textures made (of 6 at most), value names_seq, hex ms.
    if(window) {
        const LONG s1=InterlockedCompareExchange(&names_seq,0,0);
        if(s1!=names_built && !(s1&1)) {
            NameSet copy;
            memcpy(&copy,&names_pub,sizeof copy);
            if(InterlockedCompareExchange(&names_seq,0,0)==s1) {
                const ULONGLONG t0=GetTickCount64();
                const unsigned made=build_names(d,copy);
                names_built=s1;
                log_event("duel_manual_ovwin_names",made,static_cast<unsigned>(s1),static_cast<unsigned>(GetTickCount64()-t0));
            }
        }
    }
    if(chips_on)chips_sync(d);
    const bool stick=sk_on && sticker_made>0;
    const bool badge_on=bg && badge_tex.v && !sticker_broken();
    if(!v && !b && !window && !stick && !chips_on && !badge_on)return;
    Screen sc;
    const float sw=static_cast<float>(desc.Width)/1920.0f,sh=static_cast<float>(desc.Height)/1080.0f;
    sc.s=sw<sh?sw:sh;sc.ox=(static_cast<float>(desc.Width)-1920.0f*sc.s)*0.5f;sc.oy=(static_cast<float>(desc.Height)-1080.0f*sc.s)*0.5f;
    ID3D11RenderTargetView *target=nullptr;
    if(FAILED(d->CreateRenderTargetView(back,nullptr,&target)))return;
    Saved saved(c);
    c->OMSetRenderTargets(1,&target,nullptr);c->OMSetBlendState(blend,nullptr,~0u);c->OMSetDepthStencilState(depth,0);
    c->RSSetState(raster);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);c->IASetInputLayout(nullptr);
    c->GSSetShader(nullptr,nullptr,0);c->HSSetShader(nullptr,nullptr,0);c->DSSetShader(nullptr,nullptr,0);
    c->VSSetShader(vs,nullptr,0);c->PSSetShader(ps,nullptr,0);c->PSSetSamplers(0,1,&sampler);
    if(b)draw_bars(c,sc,b);
    if(chips_on)draw_chips(c,sc);
    if(v)draw_number(c,sc,v);
    if(window)draw_window(c,sc,wv);
    if(badge_on) {   // local313: right, above the skip buttons, popping about its middle; whole pixels once settled
        const float k=badge_scale(static_cast<unsigned>(now-(static_cast<ULONGLONG>(bg)&~1ull)))*sc.s;
        const float bw=static_cast<float>(badge_wh[0]),bh=static_cast<float>(badge_wh[1]),w=bw*k,h=bh*k;
        const float mx=sc.ox+(kBadgeRight-bw*0.5f)*sc.s,my=sc.oy+(kBadgeBottom-bh*0.5f)*sc.s;
        float x=mx-w*0.5f,y=my-h*0.5f;
        if(k==sc.s){x=snap(x);y=snap(y);}
        quad(c,badge_tex.v,x,y,w,h);
    }
    if(stick) {   // local301: the stickers over everything, centred on their half (local303: or the centre)
        const float k=sticker_scale(static_cast<unsigned>(now-(static_cast<ULONGLONG>(sk)>>kStickerShift)))*sc.s;
        for(int at=0;at<3;++at) {
            const int kind=static_cast<int>((sk>>(1+at*4))&15)-1;
            if(kind<0 || kind>=sticker_made || !sticker_tex[kind].v)continue;
            const float w=static_cast<float>(sticker_wh[kind][0])*k,h=static_cast<float>(sticker_wh[kind][1])*k;
            quad(c,sticker_tex[kind].v,sc.ox+kStickerX[at]*sc.s-w*0.5f,sc.oy+kStickerY*sc.s-h*0.5f,w,h);
        }
    }
    saved.put(c);
    target->Release();
    ++draws;
    if(window)InterlockedIncrement(&win_draws);
}
static bool gate() {
    return (InterlockedCompareExchange64(&shown,0,0)!=0 || InterlockedCompareExchange64(&bars,0,0)!=0 || InterlockedCompareExchange64(&win,0,0)!=0 ||
            sticker_on(InterlockedCompareExchange64(&sticker,0,0),GetTickCount64()) || InterlockedCompareExchange(&chips_live,0,0)!=0 ||
            InterlockedCompareExchange64(&badge,0,0)!=0) && !broken();
}
static void draw(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc) {
    __try {draw_raw(d,c,back,desc);}
    __except(EXCEPTION_EXECUTE_HANDLER) {InterlockedExchange(&dead,1);log_event("duel_manual_number",9,draws,0);}
}
static void publish(unsigned number,float x,float y) {   // UI thread; number 0 = nothing
    const bool ok=number && x>=0.0f && y>=0.0f && x<16000.0f && y<16000.0f;
    const LONG64 v=ok?static_cast<LONG64>(number&0xFF)|(static_cast<LONG64>(static_cast<unsigned>(x*4.0f)&0xFFFF)<<8)|
                      (static_cast<LONG64>(static_cast<unsigned>(y*4.0f)&0xFFFF)<<24):0;
    InterlockedExchange64(&shown,v);
}
// UI thread. on false = no bars; morale 0-300 per side (left, right as the gauge shows them), red = the part the listed commands spend.
static void publish_bars(bool on,const int morale[2],const int red[2]) {
    LONG64 v=0;
    if(on) {
        v=1;
        for(int side=0;side<2;++side) {
            const int m=morale[side]<0?0:morale[side]>kMoraleFull?kMoraleFull:morale[side],r=red[side]<0?0:red[side]>m?m:red[side];
            v|=(static_cast<LONG64>(m)<<(1+side*18))|(static_cast<LONG64>(r)<<(10+side*18));
        }
    }
    InterlockedExchange64(&bars,v);
}
static void place_bar(int side,float x,float y,float w,float h) {   // 1920 x 1080 units; a rectangle off the screen = no bar on that side
    const bool ok=x>=0.0f && y>=0.0f && w>=1.0f && h>=1.0f && x+w<=1920.0f && y+h<=1080.0f;
    const LONG64 v=ok?static_cast<LONG64>(static_cast<unsigned>(x*4.0f)&0xFFFF)|(static_cast<LONG64>(static_cast<unsigned>(y*4.0f)&0xFFFF)<<16)|
                      (static_cast<LONG64>(static_cast<unsigned>(w*4.0f)&0xFFFF)<<32)|(static_cast<LONG64>(static_cast<unsigned>(h*4.0f)&0xFFFF)<<48):0;
    InterlockedExchange64(&bar_rect[side&1],v);
}
}
