// 4계절 패치 G1a offline bake fixture (no game process). Inputs: texture dumps from season_probe bit 8
// (data/diagnostics/season_probe/*.sptx) and the Season Map data textures (*.dds). Runs the precompiled bake
// shader (build/season/bake_*.cso) on a hardware device, writes PNG previews and times a full 4096^2 bake.
// Usage: season_bake_fixture <dump dir> <season dds dir> <cso dir> <out dir>
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"windowscodecs.lib")
#pragma comment(lib,"ole32.lib")
static ID3D11Device *dev;static ID3D11DeviceContext *ctx;
static std::wstring dump_dir,dds_dir,cso_dir,out_dir;
static void fail(const char *what,HRESULT hr=E_FAIL){printf("{\"status\":\"season_bake_fixture_failed\",\"at\":\"%s\",\"hr\":\"%08lx\"}\n",what,static_cast<unsigned long>(hr));ExitProcess(1);}
static std::vector<unsigned char> read_file(const std::wstring &p) {
    std::vector<unsigned char> b;FILE *f=nullptr;if(_wfopen_s(&f,p.c_str(),L"rb") || !f)return b;
    fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);b.resize(static_cast<size_t>(n));fread(b.data(),1,b.size(),f);fclose(f);return b;
}
static bool block_format(DXGI_FORMAT f,unsigned &bytes) {
    if((f>=70&&f<=72)||(f>=79&&f<=81)){bytes=8;return true;}
    if((f>=73&&f<=78)||(f>=82&&f<=84)||(f>=94&&f<=99)){bytes=16;return true;}
    return false;
}
static unsigned texel_bytes(DXGI_FORMAT f) {
    switch(f){case DXGI_FORMAT_R16G16B16A16_FLOAT:return 8;case DXGI_FORMAT_R32_FLOAT:case DXGI_FORMAT_R32_TYPELESS:return 4;default:return 4;}
}
struct Tex {ID3D11Texture2D *tex=nullptr;ID3D11ShaderResourceView *srv=nullptr;UINT w=0,h=0,mips=0,array=1;DXGI_FORMAT fmt=DXGI_FORMAT_UNKNOWN;};
// Builds an immutable texture; data = per subresource packed rows.
static Tex make_texture(UINT w,UINT h,UINT mips,UINT array,DXGI_FORMAT store,DXGI_FORMAT view,const std::vector<const unsigned char *> &subs) {
    unsigned bb=0;bool block=block_format(store,bb);
    std::vector<D3D11_SUBRESOURCE_DATA> init(subs.size());
    for(size_t i=0;i<subs.size();++i) {
        UINT mip=static_cast<UINT>(i%mips),mw=w>>mip,mh=h>>mip;if(!mw)mw=1;if(!mh)mh=1;
        UINT pitch=block?((mw+3)/4)*bb:mw*texel_bytes(store);
        init[i]={subs[i],pitch,pitch*(block?(mh+3)/4:mh)};
    }
    D3D11_TEXTURE2D_DESC d={w,h,mips,array,store,{1,0},D3D11_USAGE_IMMUTABLE,D3D11_BIND_SHADER_RESOURCE,0,0};
    Tex t;HRESULT hr=dev->CreateTexture2D(&d,init.data(),&t.tex);if(FAILED(hr))fail("create_texture",hr);
    D3D11_SHADER_RESOURCE_VIEW_DESC v={};v.Format=view;
    if(array>1){v.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;v.Texture2DArray={0,mips,0,array};}
    else {v.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;v.Texture2D={0,mips};}
    hr=dev->CreateShaderResourceView(t.tex,&v,&t.srv);if(FAILED(hr))fail("create_srv",hr);
    t.w=w;t.h=h;t.mips=mips;t.array=array;t.fmt=view;return t;
}
// .sptx: 'SPTX', W, H, mips, array, format, count; per subresource row_bytes, rows, rows packed.
static std::vector<unsigned char> sptx_store[16];
static Tex load_sptx(int index,const wchar_t *name,DXGI_FORMAT store,DXGI_FORMAT view) {
    auto &b=sptx_store[index];b=read_file(dump_dir+L"\\"+name);if(b.size()<28)fail("sptx_missing");
    const UINT *h=reinterpret_cast<const UINT *>(b.data());
    std::vector<const unsigned char *> subs;size_t at=28;
    for(UINT i=0;i<h[6];++i){const UINT *s=reinterpret_cast<const UINT *>(b.data()+at);subs.push_back(b.data()+at+8);at+=8+static_cast<size_t>(s[0])*s[1];}
    return make_texture(h[1],h[2],h[3],h[4]?h[4]:1,store,view,subs);
}
static std::vector<unsigned char> dds_store[24];static int dds_count;
static Tex load_dds(const wchar_t *name,bool srgb=false) {
    auto &b=dds_store[dds_count++];b=read_file(dds_dir+L"\\"+name);if(b.size()<128)fail("dds_missing");
    const UINT *h=reinterpret_cast<const UINT *>(b.data()+4);
    UINT height=h[2],width=h[3],mips=h[6]?h[6]:1,array=1;size_t at=128;DXGI_FORMAT f=DXGI_FORMAT_UNKNOWN;
    UINT four=*reinterpret_cast<const UINT *>(b.data()+84);
    if(four==MAKEFOURCC('D','X','1','0')){const UINT *x=reinterpret_cast<const UINT *>(b.data()+128);f=static_cast<DXGI_FORMAT>(x[0]);array=x[3]?x[3]:1;at=148;}
    else if(four==MAKEFOURCC('D','X','T','1'))f=DXGI_FORMAT_BC1_UNORM;
    else {UINT rmask=*reinterpret_cast<const UINT *>(b.data()+92);f=rmask==0xFFu?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_B8G8R8A8_UNORM;}
    unsigned bb=0;bool block=block_format(f,bb);std::vector<const unsigned char *> subs;
    for(UINT a=0;a<array;++a)for(UINT m=0;m<mips;++m) {
        UINT mw=width>>m,mh=height>>m;if(!mw)mw=1;if(!mh)mh=1;
        size_t size=block?static_cast<size_t>((mw+3)/4)*bb*((mh+3)/4):static_cast<size_t>(mw)*mh*texel_bytes(f);
        if(at+size>b.size())fail("dds_short");subs.push_back(b.data()+at);at+=size;
    }
    DXGI_FORMAT view=f;
    if(srgb && f==DXGI_FORMAT_BC1_UNORM)view=DXGI_FORMAT_BC1_UNORM_SRGB;
    return make_texture(width,height,mips,array,f,view,subs);
}
static ID3DBlob *compile(const char *src,const char *entry,const char *target) {
    ID3DBlob *code=nullptr,*err=nullptr;
    HRESULT hr=D3DCompile(src,strlen(src),"inline",nullptr,nullptr,entry,target,0,0,&code,&err);
    if(FAILED(hr)){if(err)printf("%s\n",static_cast<const char *>(err->GetBufferPointer()));fail("compile",hr);}
    return code;
}
struct Target {ID3D11Texture2D *tex=nullptr,*staging=nullptr;ID3D11RenderTargetView *rtv=nullptr;UINT w=0,h=0;};
static Target make_target(UINT w,UINT h) {
    Target t;t.w=w;t.h=h;
    D3D11_TEXTURE2D_DESC d={w,h,1,1,DXGI_FORMAT_R8G8B8A8_TYPELESS,{1,0},D3D11_USAGE_DEFAULT,D3D11_BIND_RENDER_TARGET,0,0};
    if(FAILED(dev->CreateTexture2D(&d,nullptr,&t.tex)))fail("target");
    D3D11_RENDER_TARGET_VIEW_DESC r={};r.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;r.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
    if(FAILED(dev->CreateRenderTargetView(t.tex,&r,&t.rtv)))fail("rtv");
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    if(FAILED(dev->CreateTexture2D(&d,nullptr,&t.staging)))fail("staging");
    return t;
}
static std::vector<unsigned> read_target(const Target &t) {
    ctx->CopyResource(t.staging,t.tex);D3D11_MAPPED_SUBRESOURCE m={};
    if(FAILED(ctx->Map(t.staging,0,D3D11_MAP_READ,0,&m)))fail("map");
    std::vector<unsigned> px(static_cast<size_t>(t.w)*t.h);
    for(UINT y=0;y<t.h;++y)memcpy(px.data()+static_cast<size_t>(y)*t.w,static_cast<unsigned char *>(m.pData)+static_cast<size_t>(y)*m.RowPitch,t.w*4);
    ctx->Unmap(t.staging,0);return px;
}
static void write_png(const std::wstring &name,const std::vector<unsigned> &px,UINT w,UINT h) {
    IWICImagingFactory *f=nullptr;if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f))))fail("wic");
    IWICStream *s=nullptr;f->CreateStream(&s);std::wstring p=out_dir+L"\\"+name;
    if(FAILED(s->InitializeFromFilename(p.c_str(),GENERIC_WRITE)))fail("png_open");
    IWICBitmapEncoder *e=nullptr;f->CreateEncoder(GUID_ContainerFormatPng,nullptr,&e);e->Initialize(s,WICBitmapEncoderNoCache);
    IWICBitmapFrameEncode *fr=nullptr;e->CreateNewFrame(&fr,nullptr);fr->Initialize(nullptr);fr->SetSize(w,h);
    WICPixelFormatGUID g=GUID_WICPixelFormat32bppRGBA;fr->SetPixelFormat(&g);
    std::vector<unsigned> opaque(px);for(auto &v:opaque)v|=0xFF000000u;
    fr->WritePixels(h,w*4,static_cast<UINT>(opaque.size()*4),reinterpret_cast<BYTE *>(opaque.data()));
    fr->Commit();e->Commit();fr->Release();e->Release();s->Release();f->Release();
}
static float lin(unsigned c){float v=c/255.0f;return v<=0.04045f?v/12.92f:std::pow((v+0.055f)/1.055f,2.4f);}
static unsigned enc(float v){v=v<0?0:v>1?1:v;float s=v<=0.0031308f?v*12.92f:1.055f*std::pow(v,1/2.4f)-0.055f;return static_cast<unsigned>(s*255.0f+0.5f);}
static std::vector<unsigned> multiply(const std::vector<unsigned> &a,const std::vector<unsigned> &b) {
    std::vector<unsigned> r(a.size());
    for(size_t i=0;i<a.size();++i){unsigned o=0;for(int c=0;c<3;++c)o|=enc(lin((a[i]>>(8*c))&255)*lin((b[i]>>(8*c))&255))<<(8*c);r[i]=o;}
    return r;
}
int wmain(int argc,wchar_t **argv) {
    if(argc<5)fail("usage");
    dump_dir=argv[1];dds_dir=argv[2];cso_dir=argv[3];out_dir=argv[4];CreateDirectoryW(out_dir.c_str(),nullptr);
    CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    D3D_FEATURE_LEVEL fl=D3D_FEATURE_LEVEL_11_0;
    HRESULT hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,&fl,1,D3D11_SDK_VERSION,&dev,nullptr,&ctx);
    if(FAILED(hr))fail("device",hr);
    // Game textures (local113 dumps). t10 = BC1 4096 (tex_3); t4 candidates = BC3/BC7 4096.
    Tex t10=load_sptx(0,L"tex_3_4096x4096_f70.sptx",DXGI_FORMAT_BC1_TYPELESS,DXGI_FORMAT_BC1_UNORM_SRGB);
    Tex cand[5]={load_sptx(1,L"tex_1_4096x4096_f97.sptx",DXGI_FORMAT_BC7_TYPELESS,DXGI_FORMAT_BC7_UNORM_SRGB),
                 load_sptx(2,L"tex_6_4096x4096_f76.sptx",DXGI_FORMAT_BC3_TYPELESS,DXGI_FORMAT_BC3_UNORM_SRGB),
                 load_sptx(3,L"tex_7_4096x4096_f76.sptx",DXGI_FORMAT_BC3_TYPELESS,DXGI_FORMAT_BC3_UNORM_SRGB),
                 load_sptx(4,L"tex_8_4096x4096_f76.sptx",DXGI_FORMAT_BC3_TYPELESS,DXGI_FORMAT_BC3_UNORM_SRGB),load_sptx(7,L"tex_2_4096x4096_f76.sptx",DXGI_FORMAT_BC3_TYPELESS,DXGI_FORMAT_BC3_UNORM_SRGB)};
    Tex height=load_sptx(5,L"tex_4_2049x2049_f39.sptx",DXGI_FORMAT_R32_TYPELESS,DXGI_FORMAT_R32_FLOAT);
    Tex control=load_sptx(6,L"tex_5_2049x2049_f90.sptx",DXGI_FORMAT_B8G8R8A8_TYPELESS,DXGI_FORMAT_B8G8R8A8_UNORM);
    int far_index=_wtoi(argc>5?argv[5]:L"1");
    // Season Map data textures (register numbers from its d3dx.ini).
    struct Slot {UINT reg;const wchar_t *file;bool srgb;} slots[]={
        {102,L"season_relief.dds",false},{103,L"season_tags.dds",false},{104,L"season_rock3.dds",false},{105,L"season_rock2.dds",false},
        {106,L"season_flood.dds",false},{107,L"season_hazard.dds",false},{108,L"season_zone.dds",false},{109,L"season_climate.dds",false},
        {111,L"season_snow.dds",false},{112,L"season_veg.dds",false},{113,L"season_freeze.dds",false},{114,L"season_rockregion.dds",false},
        {115,L"season_rock.dds",false},{116,L"season_rocklayer.dds",false},{117,L"season_ground.dds",false},{118,L"season_soil.dds",false},
        {119,L"season_forest.dds",false}};
    for(auto &s:slots){Tex t=load_dds(s.file,s.srgb);ctx->PSSetShaderResources(s.reg,1,&t.srv);}
    ctx->PSSetShaderResources(110,1,&height.srv);ctx->PSSetShaderResources(3,1,&control.srv);
    ctx->PSSetShaderResources(20,1,&t10.srv);ctx->PSSetShaderResources(21,1,&cand[far_index].srv);
    // IniParams (Texture1D, 4096 float4): month, on, xun, lying-snow turn; weather/direct link invalid.
    std::vector<float> ini(4096*4,0.0f);
    D3D11_TEXTURE1D_DESC id={4096,1,1,DXGI_FORMAT_R32G32B32A32_FLOAT,D3D11_USAGE_DEFAULT,D3D11_BIND_SHADER_RESOURCE,0,0};
    ID3D11Texture1D *ini_tex=nullptr;D3D11_SUBRESOURCE_DATA ini_data={ini.data(),4096*16,0};
    if(FAILED(dev->CreateTexture1D(&id,&ini_data,&ini_tex)))fail("ini");
    ID3D11ShaderResourceView *ini_srv=nullptr;dev->CreateShaderResourceView(ini_tex,nullptr,&ini_srv);ctx->PSSetShaderResources(120,1,&ini_srv);
    D3D11_SAMPLER_DESC sd={D3D11_FILTER_ANISOTROPIC,D3D11_TEXTURE_ADDRESS_CLAMP,D3D11_TEXTURE_ADDRESS_CLAMP,D3D11_TEXTURE_ADDRESS_CLAMP,0,8,D3D11_COMPARISON_NEVER,{0,0,0,0},0,D3D11_FLOAT32_MAX};
    ID3D11SamplerState *clamp=nullptr,*wrap=nullptr;dev->CreateSamplerState(&sd,&clamp);
    sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;dev->CreateSamplerState(&sd,&wrap);
    ctx->PSSetSamplers(3,1,&clamp);ctx->PSSetSamplers(7,1,&wrap);
    float cb[8]={};D3D11_BUFFER_DESC bd={32,D3D11_USAGE_DEFAULT,D3D11_BIND_CONSTANT_BUFFER,0,0,0};
    ID3D11Buffer *bake_cb=nullptr;dev->CreateBuffer(&bd,nullptr,&bake_cb);ctx->PSSetConstantBuffers(0,1,&bake_cb);
    float cam[20]={};bd.ByteWidth=80;D3D11_SUBRESOURCE_DATA cd={cam,0,0};ID3D11Buffer *cam_cb=nullptr;dev->CreateBuffer(&bd,&cd,&cam_cb);ctx->PSSetConstantBuffers(13,1,&cam_cb);
    auto vs_code=read_file(cso_dir+L"\\season_bake_vs.cso"),ps_code=read_file(cso_dir+L"\\season_bake_ps.cso");
    ID3D11VertexShader *vs=nullptr;ID3D11PixelShader *ps=nullptr;
    if(FAILED(dev->CreateVertexShader(vs_code.data(),vs_code.size(),nullptr,&vs)))fail("bake_vs");
    if(FAILED(dev->CreatePixelShader(ps_code.data(),ps_code.size(),nullptr,&ps)))fail("bake_ps");
    // Preview shader: sample t20 / t21 at a mip.
    const char *show_src="Texture2D<float4> a:register(t20);Texture2D<float4> b:register(t21);SamplerState s:register(s3);"
        "cbuffer C:register(b0){float4 k0;float4 k1;};"
        "float4 main(float4 p:SV_Position):SV_Target{float2 uv=(p.xy+k0.zw)/k0.xy;return k1.y>0.5?b.SampleLevel(s,uv,k1.x):a.SampleLevel(s,uv,k1.x);}";
    ID3DBlob *show_blob=compile(show_src,"main","ps_5_0");ID3D11PixelShader *show=nullptr;dev->CreatePixelShader(show_blob->GetBufferPointer(),show_blob->GetBufferSize(),nullptr,&show);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);ctx->VSSetShader(vs,nullptr,0);
    const UINT P=512;const float PreviewMip=3;Target prev=make_target(P,P);D3D11_VIEWPORT vp={0,0,static_cast<float>(P),static_cast<float>(P),0,1};
    auto draw=[&](ID3D11PixelShader *shader,float mip,float which){
        cb[0]=cb[1]=static_cast<float>(P);cb[2]=cb[3]=0;cb[4]=mip;cb[5]=which;ctx->UpdateSubresource(bake_cb,0,nullptr,cb,0,0);
        ctx->OMSetRenderTargets(1,&prev.rtv,nullptr);ctx->RSSetViewports(1,&vp);ctx->PSSetShader(shader,nullptr,0);ctx->Draw(3,0);};
    draw(show,PreviewMip,0);auto orig_mul=read_target(prev);write_png(L"t10_original.png",orig_mul,P,P);
    for(int i=0;i<5;++i){ctx->PSSetShaderResources(21,1,&cand[i].srv);draw(show,i==4?0.0f:2.0f,1);wchar_t n[40];swprintf_s(n,L"far_candidate_%d.png",i);write_png(n,read_target(prev),P,P);}
    ctx->PSSetShaderResources(21,1,&cand[far_index].srv);draw(show,PreviewMip,1);auto far_px=read_target(prev);
    auto look_orig=multiply(orig_mul,far_px);write_png(L"look_original.png",look_orig,P,P);
    // G1c sheet: rows = months 1,4,7,11 (middle xun); columns = original look, Season Map target, baked far
    // (t10'*t4'), baked near approximation (t10' * original t4, the detail averaging to t4).
    const int months[4]={1,4,7,11};const UINT C=4;std::vector<unsigned> sheet(static_cast<size_t>(P)*C*P*4);
    auto put=[&](const std::vector<unsigned> &img,UINT col,UINT row){for(UINT y=0;y<P;++y)memcpy(sheet.data()+(static_cast<size_t>(row)*P+y)*P*C+col*P,img.data()+static_cast<size_t>(y)*P,P*4);};
    for(UINT r=0;r<4;++r) {
        const int month=months[r],xun=1;ini[0]=static_cast<float>(month);ini[1]=1.0f;ini[4]=static_cast<float>(xun);
        ini[99*4+0]=0.0f;ini[99*4+1]=static_cast<float>((month-1)*3+xun+1);
        ctx->UpdateSubresource(ini_tex,0,nullptr,ini.data(),0,0);
        draw(ps,PreviewMip,0);auto mulp=read_target(prev);
        draw(ps,PreviewMip,1);auto farp=read_target(prev);
        draw(ps,PreviewMip,2);auto target=read_target(prev);
        put(look_orig,0,r);put(target,1,r);put(multiply(mulp,farp),2,r);put(multiply(mulp,far_px),3,r);
        wchar_t n[48];swprintf_s(n,L"t10_month%02d.png",month);write_png(n,mulp,P,P);
        swprintf_s(n,L"t4_month%02d.png",month);write_png(n,farp,P,P);
    }
    write_png(L"sheet_g1c.png",sheet,P*C,P*4);    // Full-size timing: 4096^2 in 512^2 tiles (the runtime plan bakes one tile per frame).
    Target full=make_target(512,512);D3D11_VIEWPORT fvp={0,0,512,512,0,1};
    D3D11_QUERY_DESC qd={D3D11_QUERY_TIMESTAMP_DISJOINT,0};ID3D11Query *disjoint=nullptr,*t0=nullptr,*t1=nullptr;dev->CreateQuery(&qd,&disjoint);
    qd.Query=D3D11_QUERY_TIMESTAMP;dev->CreateQuery(&qd,&t0);dev->CreateQuery(&qd,&t1);
    double worst=0,total=0;
    for(UINT ty=0;ty<8;++ty)for(UINT tx=0;tx<8;++tx) {
        cb[0]=cb[1]=4096;cb[2]=tx*512.0f;cb[3]=ty*512.0f;cb[4]=0;cb[5]=0;ctx->UpdateSubresource(bake_cb,0,nullptr,cb,0,0);
        ctx->Begin(disjoint);ctx->End(t0);
        ctx->OMSetRenderTargets(1,&full.rtv,nullptr);ctx->RSSetViewports(1,&fvp);ctx->PSSetShader(ps,nullptr,0);ctx->Draw(3,0);
        ctx->End(t1);ctx->End(disjoint);
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj={};while(ctx->GetData(disjoint,&dj,sizeof(dj),0)==S_FALSE)Sleep(0);
        UINT64 a=0,b=0;ctx->GetData(t0,&a,sizeof(a),0);ctx->GetData(t1,&b,sizeof(b),0);
        double ms=dj.Disjoint?0:(b-a)*1000.0/dj.Frequency;total+=ms;if(ms>worst)worst=ms;
        if(tx==4 && ty==3)write_png(L"tile_full_res_month10.png",read_target(full),512,512);
    }
    // BC1 write path: R32G32_UINT blocks copied into a DEFAULT BC1 texture, read back through a second copy.
    D3D11_TEXTURE2D_DESC bdsc={1024,1024,1,1,DXGI_FORMAT_R32G32_UINT,{1,0},D3D11_USAGE_DEFAULT,0,0,0};
    std::vector<unsigned> blocks(1024*1024*2);for(size_t i=0;i<blocks.size();++i)blocks[i]=static_cast<unsigned>(i*2654435761u);
    D3D11_SUBRESOURCE_DATA bdata={blocks.data(),1024*8,0};ID3D11Texture2D *uint_tex=nullptr,*bc1=nullptr,*back=nullptr;
    dev->CreateTexture2D(&bdsc,&bdata,&uint_tex);
    D3D11_TEXTURE2D_DESC cdsc={4096,4096,1,1,DXGI_FORMAT_BC1_TYPELESS,{1,0},D3D11_USAGE_DEFAULT,D3D11_BIND_SHADER_RESOURCE,0,0};dev->CreateTexture2D(&cdsc,nullptr,&bc1);
    bdsc.Usage=D3D11_USAGE_STAGING;bdsc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;dev->CreateTexture2D(&bdsc,nullptr,&back);
    bool copy_ok=false;
    if(uint_tex && bc1 && back) {
        ctx->CopySubresourceRegion(bc1,0,0,0,0,uint_tex,0,nullptr);ctx->CopySubresourceRegion(back,0,0,0,0,bc1,0,nullptr);
        D3D11_MAPPED_SUBRESOURCE m={};
        if(SUCCEEDED(ctx->Map(back,0,D3D11_MAP_READ,0,&m))){copy_ok=memcmp(m.pData,blocks.data(),1024*8)==0 &&
            memcmp(static_cast<unsigned char *>(m.pData)+static_cast<size_t>(1023)*m.RowPitch,blocks.data()+1023*2048,1024*8)==0;ctx->Unmap(back,0);}
    }
    printf("{\"status\":\"season_bake_fixture_passed\",\"tile_ms_worst\":%.2f,\"full_4096_ms\":%.1f,\"bc1_copy\":%s,\"far_index\":%d}\n",worst,total,copy_ok?"true":"false",far_index);
    return copy_ok?0:2;
}
