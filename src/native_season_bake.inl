// 4계절 패치 G1a (user 2026-11-13 "계절 맵 수식 이식", "다음 계절 값을 미리 계산해서 캐싱"): the Season Map's ground
// picture (season(), wprkfdid12-ux, no-conditions licence) is baked into the game's two ground map textures instead of
// being drawn per screen pixel each frame:
//   t10 = BC1 4096^2 colour multiplier, t4 = BC3 4096^2 far albedo (ground PS d3e1f71e / 9addb6cb, research/season_map/DESIGN.md 3.7).
// Both are created DEFAULT instead of IMMUTABLE (same contents); an IMMUTABLE copy keeps the originals as bake input.
// The present overlay bakes one 512^2 tile per frame into a spare texture (GPU, then GPU block compression and a format-cast
// copy); when a whole season state is ready it is copied in with CopyResource. The next 10-day turn is pre-baked during the
// current one, so the turn change itself is one copy. Off ([season] season_map=0) = no capture, the textures stay IMMUTABLE
// and untouched. Inputs: the control and height maps (own immutable copies of the creation data) and the Season Map data
// textures + precompiled shaders from AI_TacticsMod\data\season.
#pragma once
namespace season_bake {
static bool on() {return enabled && mod_features.convenience && mod_features.season_map;}
static constexpr unsigned MapSize=4096,MapMips=11,Tile=512;
static constexpr unsigned long long kMulPrint=(746583057ull<<32)|4059934600ull;   // first rows of t10 (local112 season_tex4)
static constexpr unsigned long long kFarPrint=(1297425982ull<<32)|475573157ull;   // first rows of t4
static unsigned long long print(const D3D11_SUBRESOURCE_DATA *initial) {
    if(!initial || !initial[0].pSysMem || !initial[0].SysMemPitch)return 0;
    size_t n=static_cast<size_t>(initial[0].SysMemPitch)*4u;if(n>65536)n=65536;
    unsigned long long h=0;auto p=static_cast<const unsigned char *>(initial[0].pSysMem);
    for(size_t i=0;i<n;++i){h*=1099511628211ull;h^=p[i];}
    return h;
}
template<class T> static void drop(T *&p) {if(p){p->Release();p=nullptr;}}
struct Map {
    ID3D11Texture2D *live=nullptr,*orig=nullptr,*next=nullptr;   // live = the game's texture (our reference)
    ID3D11ShaderResourceView *orig_srv=nullptr;DXGI_FORMAT srgb=DXGI_FORMAT_UNKNOWN;bool bc3=false;
};
static SRWLOCK lock=SRWLOCK_INIT;
static Map maps[2];   // 0 t10, 1 t4
static ID3D11Device *device;   // the device the maps belong to (kept by the textures)
static ID3D11Texture2D *height_tex,*control_tex;static ID3D11ShaderResourceView *height_srv,*control_srv;
static bool failed;
// Our own CreateTexture2D calls go through the same hooked vtable: mark them so the wrapper passes them straight on
// (local114 crashed with endless self-copies of the 2049 control map).
static thread_local int inside;
struct Inside {Inside(){++inside;} ~Inside(){--inside;} Inside(const Inside &)=delete;Inside &operator=(const Inside &)=delete;};
struct Job {int key=-1;unsigned item=0;bool active=false;ULONGLONG started=0;bool shader=false;unsigned epoch=0;};
static Job job;
static int live_key=-1,ready_key=-1;   // state shown, state waiting in the spare textures
// G1b: the season rules (SAN14_ModPack.dll) hand their IniParams list (rain, snowfall, river ice, disasters) through this entry while
// they run; each change of the list is baked as soon as the bake before it is done. Null = the calendar-only picture as before.
static const int kParams=7460;
static unsigned (__cdecl *weather_params)(float *out,int count);
static unsigned live_epoch;
static float weather_buf[kParams*4];
static bool live_shader=false,game_dirty=false;   // the shown state is in our D/A maps; the game maps hold a baked state
// ---- creation (render/loader thread inside the CreateTexture2D wrapper) ----------------------------------------
static int kind(const D3D11_TEXTURE2D_DESC *d,const D3D11_SUBRESOURCE_DATA *initial) {
    if(!on() || !d || !initial || d->Width!=MapSize || d->Height!=MapSize || d->MipLevels!=MapMips || d->ArraySize!=1 || d->SampleDesc.Count!=1)return -1;
    if(d->Format==DXGI_FORMAT_BC1_TYPELESS && print(initial)==kMulPrint)return 0;
    if(d->Format==DXGI_FORMAT_BC3_TYPELESS && print(initial)==kFarPrint)return 1;
    return -1;
}
static ID3D11ShaderResourceView *view(ID3D11Device *dev,ID3D11Texture2D *tex,DXGI_FORMAT format,unsigned mips) {
    D3D11_SHADER_RESOURCE_VIEW_DESC v={};v.Format=format;v.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;v.Texture2D={0,mips};
    ID3D11ShaderResourceView *srv=nullptr;return SUCCEEDED(dev->CreateShaderResourceView(tex,&v,&srv))?srv:nullptr;
}
static void reset_locked() {
    for(auto &m:maps){drop(m.live);drop(m.orig);drop(m.next);drop(m.orig_srv);}
    drop(height_srv);drop(control_srv);drop(height_tex);drop(control_tex);device=nullptr;
}
static void map_created(ID3D11Device *dev,int k,const D3D11_TEXTURE2D_DESC *d,const D3D11_SUBRESOURCE_DATA *initial,ID3D11Texture2D *live) {
    D3D11_TEXTURE2D_DESC od=*d;od.Usage=D3D11_USAGE_IMMUTABLE;od.BindFlags=D3D11_BIND_SHADER_RESOURCE;od.CPUAccessFlags=0;od.MiscFlags=0;
    D3D11_TEXTURE2D_DESC nd=od;nd.Usage=D3D11_USAGE_DEFAULT;
    ID3D11Texture2D *orig=nullptr,*next=nullptr;Inside guard;
    bool ok=SUCCEEDED(dev->CreateTexture2D(&od,initial,&orig)) && SUCCEEDED(dev->CreateTexture2D(&nd,initial,&next));
    DXGI_FORMAT srgb=k==0?DXGI_FORMAT_BC1_UNORM_SRGB:DXGI_FORMAT_BC3_UNORM_SRGB;
    ID3D11ShaderResourceView *srv=ok?view(dev,orig,srgb,MapMips):nullptr;
    AcquireSRWLockExclusive(&lock);
    if(device && device!=dev)reset_locked();
    Map &m=maps[k];drop(m.live);drop(m.orig);drop(m.next);drop(m.orig_srv);
    if(ok && srv){live->AddRef();m={live,orig,next,srv,srgb,k==1};device=dev;orig=next=nullptr;srv=nullptr;}
    live_key=ready_key=-1;job.active=false;job.epoch=0;live_epoch=0;game_dirty=false;live_shader=false;   // a (re)loaded map starts from the original pictures
    ReleaseSRWLockExclusive(&lock);
    drop(orig);drop(next);drop(srv);
#ifndef NO_RUNTIME_LOG
    log_event("season_bake_capture",static_cast<unsigned>(k),ok?1u:0u,0);
#endif
}
// Control (2049^2 BGRA, format 90) and height (2049^2 R32, format 39): own immutable copies of the creation data.
static void source_created(ID3D11Device *dev,const D3D11_TEXTURE2D_DESC *d,const D3D11_SUBRESOURCE_DATA *initial) {
    if(!on() || !d || !initial || !initial[0].pSysMem || d->Width!=2049 || d->Height!=2049 || d->ArraySize!=1)return;
    const bool control=d->Format==DXGI_FORMAT_B8G8R8A8_TYPELESS,height=d->Format==DXGI_FORMAT_R32_TYPELESS;
    if(!control && !height)return;
    D3D11_TEXTURE2D_DESC c=*d;c.Usage=D3D11_USAGE_IMMUTABLE;c.BindFlags=D3D11_BIND_SHADER_RESOURCE;c.CPUAccessFlags=0;c.MiscFlags=0;
    ID3D11Texture2D *tex=nullptr;ID3D11ShaderResourceView *srv=nullptr;Inside guard;
    if(SUCCEEDED(dev->CreateTexture2D(&c,initial,&tex)))srv=view(dev,tex,control?DXGI_FORMAT_B8G8R8A8_UNORM:DXGI_FORMAT_R32_FLOAT,c.MipLevels?c.MipLevels:1);
    AcquireSRWLockExclusive(&lock);
    if(control){drop(control_srv);drop(control_tex);control_tex=tex;control_srv=srv;}
    else {drop(height_srv);drop(height_tex);height_tex=tex;height_srv=srv;}
    tex=nullptr;srv=nullptr;
    ReleaseSRWLockExclusive(&lock);
#ifndef NO_RUNTIME_LOG
    log_event("season_bake_source",control?1u:2u,control?(control_srv?1u:0u):(height_srv?1u:0u),0);
#endif
}
// ---- resources loaded on the first baking frame ---------------------------------------------------------------
static std::vector<unsigned char> read_data(const wchar_t *name) {
    std::vector<unsigned char> b;wchar_t path[MAX_PATH]={};
    DWORD n=GetModuleFileNameW(nullptr,path,MAX_PATH);if(!n || n>=MAX_PATH)return b;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return b;*slash=0;
    if(wcscat_s(path,L"\\AI_TacticsMod\\data\\season\\") || wcscat_s(path,name))return b;
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE)return b;
    LARGE_INTEGER size={};
    if(GetFileSizeEx(f,&size) && size.QuadPart>0 && size.QuadPart<(256ll<<20)) {
        b.resize(static_cast<size_t>(size.QuadPart));DWORD got=0;
        if(!ReadFile(f,b.data(),static_cast<DWORD>(b.size()),&got,nullptr) || got!=b.size())b.clear();
    }
    CloseHandle(f);return b;
}
static bool block_bytes(DXGI_FORMAT f,unsigned &bytes) {
    if((f>=70&&f<=72)||(f>=79&&f<=81)){bytes=8;return true;}
    if((f>=73&&f<=78)||(f>=82&&f<=84)||(f>=94&&f<=99)){bytes=16;return true;}
    return false;
}
// Minimal DDS reader for the Season Map data set: DX10 headers, legacy DXT1 and legacy 32-bit RGBA/BGRA.
static ID3D11ShaderResourceView *load_dds(ID3D11Device *dev,const wchar_t *name) {
    auto b=read_data(name);if(b.size()<128 || memcmp(b.data(),"DDS ",4))return nullptr;
    auto u32=[&](size_t at){unsigned v=0;memcpy(&v,b.data()+at,4);return v;};
    UINT height=u32(12),width=u32(16),mips=u32(28)?u32(28):1,array=1;size_t at=128;DXGI_FORMAT f=DXGI_FORMAT_UNKNOWN;
    const unsigned four=u32(84);
    if(four==808540228u){if(b.size()<148)return nullptr;f=static_cast<DXGI_FORMAT>(u32(128));array=u32(140)?u32(140):1;at=148;}
    else if(four==827611204u)f=DXGI_FORMAT_BC1_UNORM;
    else if(u32(88)==32)f=u32(92)==255u?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_B8G8R8A8_UNORM;
    else return nullptr;
    unsigned bb=0;const bool block=block_bytes(f,bb);
    const unsigned texel=f==DXGI_FORMAT_R16G16B16A16_FLOAT?8u:4u;
    std::vector<D3D11_SUBRESOURCE_DATA> init;
    for(UINT a=0;a<array;++a)for(UINT m=0;m<mips;++m) {
        UINT w=width>>m,h=height>>m;if(!w)w=1;if(!h)h=1;
        UINT pitch=block?((w+3)/4)*bb:w*texel,rows=block?(h+3)/4:h;size_t size=static_cast<size_t>(pitch)*rows;
        if(at+size>b.size())return nullptr;
        init.push_back({b.data()+at,pitch,0});at+=size;
    }
    D3D11_TEXTURE2D_DESC d={width,height,mips,array,f,{1,0},D3D11_USAGE_IMMUTABLE,D3D11_BIND_SHADER_RESOURCE,0,0};
    ID3D11Texture2D *tex=nullptr;if(FAILED(dev->CreateTexture2D(&d,init.data(),&tex)))return nullptr;
    D3D11_SHADER_RESOURCE_VIEW_DESC v={};v.Format=f;
    if(array>1){v.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2DARRAY;v.Texture2DArray={0,mips,0,array};}
    else {v.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;v.Texture2D={0,mips};}
    ID3D11ShaderResourceView *srv=nullptr;dev->CreateShaderResourceView(tex,&v,&srv);tex->Release();return srv;
}
struct DataSlot {UINT reg;const wchar_t *file;};
static constexpr DataSlot kData[]={
    {102,L"season_relief.dds"},{103,L"season_tags.dds"},{104,L"season_rock3.dds"},{105,L"season_rock2.dds"},{106,L"season_flood.dds"},
    {107,L"season_hazard.dds"},{108,L"season_zone.dds"},{109,L"season_climate.dds"},{111,L"season_snow.dds"},{112,L"season_veg.dds"},
    {113,L"season_freeze.dds"},{114,L"season_rockregion.dds"},{115,L"season_rock.dds"},{116,L"season_rocklayer.dds"},{117,L"season_ground.dds"},
    {118,L"season_soil.dds"},{119,L"season_forest.dds"}};
static constexpr unsigned DataCount=sizeof(kData)/sizeof(*kData);
struct Gpu {
    bool loaded=false;
    ID3D11ShaderResourceView *data[DataCount]={};
    ID3D11VertexShader *vs=nullptr;ID3D11PixelShader *ps=nullptr;ID3D11ComputeShader *bc1=nullptr,*bc3=nullptr;
    ID3D11Texture1D *params=nullptr;ID3D11ShaderResourceView *params_srv=nullptr;
    ID3D11SamplerState *clamp=nullptr,*wrap=nullptr;
    ID3D11Buffer *bake_cb=nullptr,*cam_cb=nullptr,*bc_cb=nullptr;
    ID3D11Texture2D *tile=nullptr;ID3D11RenderTargetView *tile_rtv=nullptr;ID3D11ShaderResourceView *tile_srv=nullptr;
    ID3D11Texture2D *blocks8=nullptr,*blocks16=nullptr;ID3D11UnorderedAccessView *uav8=nullptr,*uav16=nullptr;
    ID3D11RasterizerState *raster=nullptr;
    // D: our own baked D = 1 - M and A maps (BC1 4096^2, 11 mips) read by the patched ground shaders at t100/t101.
    ID3D11Texture2D *own_live[2]={},*own_next[2]={};ID3D11ShaderResourceView *own_srv[2]={};
};
static Gpu gpu;
// ---- D: the two ground pixel shaders patched at creation (season_dxbc.h; memory only) -------------------------
using CreatePS=HRESULT(STDMETHODCALLTYPE *)(ID3D11Device *,const void *,SIZE_T,ID3D11ClassLinkage *,ID3D11PixelShader **);
static constexpr unsigned PSSlot=15,MaxPsTables=4;
struct PsSlot {void **table;CreatePS original;bool published;};
static PsSlot ps_slots[MaxPsTables];static SRWLOCK ps_lock=SRWLOCK_INIT;
static volatile LONG patched_bits,patch_failed_bits;   // 1 near d3e1f71e, 2 far 9addb6cb
// DXBC checksums of the current Steam build's ground shaders (season_probe dumps, 2026-11-13). Another build = no patch.
static constexpr uint32_t kGroundSum[2][4]={{608917079u,2515758241u,3706443082u,2849861075u},{4034831269u,1325773663u,3178005527u,2091776099u}};
static int ground_kind(const void *code,SIZE_T size) {
    if(!code || size<32)return -1;
    for(int k=0;k<2;++k)if(!memcmp(static_cast<const unsigned char *>(code)+4,kGroundSum[k],16))return k;
    return -1;
}
static bool shader_mode() {return (InterlockedCompareExchange(&patched_bits,0,0)&3)==3;}
// R1c rain layer: the game's two lighting passes of the map layer (season patch d3dx.ini LitFinal 70b6fa39daa0880e / LitFinalB
// 6642ae48461b1425; DXBC checksums from the season_probe dumps of the current Steam build). Remembered at creation; the rain is
// drawn right after their draw call. Another build = never matched = no rain layer.
static constexpr uint32_t kLitSum[2][4]={{778835956u,1831989519u,1852173384u,3853212897u},{3604155623u,3595158854u,1229640655u,3825602748u}};
static ID3D11PixelShader *volatile lit_ps[2];
static int lit_kind(const void *code,SIZE_T size) {
    if(!code || size<32)return -1;
    for(int k=0;k<2;++k)if(!memcmp(static_cast<const unsigned char *>(code)+4,kLitSum[k],16))return k;
    return -1;
}
static bool swap_slot(void **table,unsigned slot,void *expected,void *replacement) {
    DWORD prior=0;if(!VirtualProtect(table+slot,sizeof(void *),PAGE_READWRITE,&prior))return false;
    bool ok=InterlockedCompareExchangePointer(table+slot,replacement,expected)==expected;
    DWORD ignored=0;VirtualProtect(table+slot,sizeof(void *),prior,&ignored);return ok;
}// ---- R2 new ground materials (user 2026-11-13 "에셋 확장"): the near ground PS reads the material detail arrays t7/t8/t9
// (1024^2 BC3 x 19 layers each, IMMUTABLE) by `layer = cb0[7+idx].x`, tiling cb0[517+idx].x / cb0[772+idx].x, idx = the
// control map's material byte 0..254 (season patch translation d3e1f71e:1324-1334). So new materials need no existing
// layer replaced: each array is created with 4 more layers (generated at creation from layer 0: same grain, our colours -
// burnt ground, mud, rubble, cracked earth; nothing KOEI-derived is shipped) and the 16432-byte material table gets entries
// 240..243 -> layers 19..22 wherever its contents pass through the CPU (CreateBuffer / Map+Unmap / UpdateSubresource).
// Only cells the painter gives 240..243 change; off (season_tiles=0) = arrays and table untouched.
static constexpr unsigned kMatLayers=19,kMatNew=5,kMatFirst=240,kTableBytes=1027u*16u;   // local133: 244 river ice
static bool mat_on() {return on() && mod_features.season_tiles;}
static volatile LONG mat_streak,mat_color,mat_table;   // extensions in a row, the colour array extended, the table patched
static bool mat_ready() {return InterlockedCompareExchange(&mat_streak,0,0)>=3 && InterlockedCompareExchange(&mat_color,0,0) && InterlockedCompareExchange(&mat_table,0,0);}
static void rgb565(unsigned v,float c[3]) {c[0]=((v>>11)&31)/31.0f;c[1]=((v>>5)&63)/63.0f;c[2]=(v&31)/31.0f;}
static unsigned to565(float r,float g,float b) {
    auto q=[](float v,int m){v=v<0?0:v>1?1:v;return static_cast<unsigned>(v*m+0.5f);};
    return (q(r,31)<<11)|(q(g,63)<<5)|q(b,31);
}
static float hash2(int x,int y) {unsigned h=static_cast<unsigned>(x)*374761393u+static_cast<unsigned>(y)*668265263u;h=(h^(h>>13))*1274126177u;return ((h^(h>>16))&65535u)/65535.0f;}
static float vnoise(float x,float y) {
    const int ix=static_cast<int>(std::floor(x)),iy=static_cast<int>(std::floor(y));const float fx=x-ix,fy=y-iy;
    const float sx=fx*fx*(3-2*fx),sy=fy*fy*(3-2*fy);
    const float a=hash2(ix,iy),b=hash2(ix+1,iy),c=hash2(ix,iy+1),d=hash2(ix+1,iy+1);
    return a+(b-a)*sx+(c-a)*sy+(a-b-c+d)*sx*sy;
}
static float fbm(float x,float y) {return 0.55f*vnoise(x,y)+0.3f*vnoise(x*2.1f,y*2.1f)+0.15f*vnoise(x*4.3f,y*4.3f);}
// The array tiles, so the noise wraps every 256 blocks (the top mip's block grid).
static float tfbm(float x,float y,float period) {
    const float u=x/256.0f,v=y/256.0f;
    const float a=fbm(x/period,y/period),b=fbm((x-256)/period,y/period),c=fbm(x/period,(y-256)/period),d=fbm((x-256)/period,(y-256)/period);
    return a*(1-u)*(1-v)+b*u*(1-v)+c*(1-u)*v+d*u*v;
}
static float crack_edge(float x,float y) {   // distance to the nearest Voronoi edge in blocks, cells of 24 blocks
    const float s=24.0f;const float px=x/s,py=y/s;const int cx=static_cast<int>(std::floor(px)),cy=static_cast<int>(std::floor(py));
    float d1=9,d2=9;
    for(int j=-1;j<=1;++j)for(int i=-1;i<=1;++i) {
        const int gx=cx+i,gy=cy+j;const int wx=((gx%11)+11)%11,wy=((gy%11)+11)%11;   // ~256/24 cells: wraps close enough
        const float ox=gx+hash2(wx*7+1,wy*13+3),oy=gy+hash2(wx*5+9,wy*3+7);
        const float d=std::sqrt((px-ox)*(px-ox)+(py-oy)*(py-oy));
        if(d<d1){d2=d1;d1=d;}else if(d<d2)d2=d;
    }
    return (d2-d1)*s;
}
static void palette(int kind,float X,float Y,float out[3]) {
    const float t=tfbm(X,Y,22.0f),u=tfbm(X+97,Y+41,9.0f);
    auto mix=[&](const float a[3],const float b[3],float w){for(int i=0;i<3;++i)out[i]=a[i]+(b[i]-a[i])*w;};
    if(kind==0) {   // burnt ground: char and ash, a few brown embers
        static const float ash[3]={0.07f,0.07f,0.07f},chr[3]={0.18f,0.15f,0.12f},emb[3]={0.33f,0.20f,0.11f};
        mix(ash,chr,t);if(u>0.78f)mix(out,emb,(u-0.78f)*3.0f);
    } else if(kind==1) {   // mud: wet dark soil, grey puddle sheen
        static const float wet[3]={0.18f,0.14f,0.10f},dry[3]={0.33f,0.26f,0.18f},pud[3]={0.30f,0.33f,0.35f};
        mix(wet,dry,t);if(u>0.74f)mix(out,pud,(u-0.74f)*3.0f);
    } else if(kind==2) {   // rubble: grey and brown stone, dark gaps
        static const float brn[3]={0.36f,0.31f,0.26f},gry[3]={0.52f,0.50f,0.47f};
        mix(brn,gry,t);if(u<0.28f)for(int i=0;i<3;++i)out[i]*=0.6f;
    } else if(kind==3) {   // cracked earth: dry tan with dark cracks
        static const float tan[3]={0.62f,0.52f,0.37f},crk[3]={0.22f,0.18f,0.13f};
        for(int i=0;i<3;++i)out[i]=tan[i]*(0.88f+0.24f*t);
        const float e=crack_edge(X,Y);if(e<1.2f)mix(out,crk,1.0f-e/1.2f);
    } else {   // local133 river ice: pale blue-white, blue-grey patches, thin dark cracks, frost specks
        static const float ice[3]={0.70f,0.80f,0.88f},blu[3]={0.50f,0.62f,0.74f},crk[3]={0.34f,0.44f,0.55f},frs[3]={0.90f,0.93f,0.96f};
        mix(ice,blu,t*0.7f);if(u>0.80f)mix(out,frs,(u-0.80f)*3.0f);
        const float e=crack_edge(X*0.7f+31,Y*0.7f+17);if(e<0.8f)mix(out,crk,0.7f*(1.0f-e/0.8f));
    }
}
// Recolour a BC3 mip copied from layer 0: alpha and colour indices stay (the grain), the two colour endpoints become our
// palette scaled by the source endpoint's brightness against the mip's mean.
static void recolor(int kind,unsigned mip,unsigned char *data,UINT pitch,UINT rows) {
    const UINT bw=pitch/16;double sum=0;size_t n=0;float c[3];
    for(UINT y=0;y<rows;++y)for(UINT x=0;x<bw;++x) {
        const unsigned char *b=data+static_cast<size_t>(y)*pitch+x*16u;
        for(int e=0;e<2;++e){rgb565(b[8+e*2]|(b[9+e*2]<<8),c);sum+=(c[0]+c[1]+c[2])/3.0;++n;}
    }
    const float mean=n?static_cast<float>(sum/n):0.5f;
    for(UINT y=0;y<rows;++y)for(UINT x=0;x<bw;++x) {
        unsigned char *b=data+static_cast<size_t>(y)*pitch+x*16u;
        const float X=static_cast<float>(x<<mip),Y=static_cast<float>(y<<mip);float p[3];palette(kind,X,Y,p);
        for(int e=0;e<2;++e) {
            rgb565(b[8+e*2]|(b[9+e*2]<<8),c);
            float f=mean>0.01f?((c[0]+c[1]+c[2])/3.0f)/mean:1.0f;f=f<0.55f?0.55f:f>1.45f?1.45f:f;
            const unsigned v=to565(p[0]*f,p[1]*f,p[2]*f);b[8+e*2]=static_cast<unsigned char>(v);b[9+e*2]=static_cast<unsigned char>(v>>8);
        }
    }
}
// 0 colour, 1 normal, 2 other (grey): from the endpoint colours of every layer's smallest mip.
static int mat_class(const D3D11_SUBRESOURCE_DATA *initial,unsigned mips) {
    double r=0,g=0,b=0,grey=0;unsigned n=0;float c[3];
    for(unsigned s=0;s<kMatLayers;++s) {
        auto p=static_cast<const unsigned char *>(initial[s*mips+mips-1].pSysMem);if(!p)return -1;
        for(int e=0;e<2;++e){rgb565(p[8+e*2]|(p[9+e*2]<<8),c);r+=c[0];g+=c[1];b+=c[2];grey+=std::fabs(c[0]-c[1])+std::fabs(c[1]-c[2]);++n;}
    }
    r/=n;g/=n;b/=n;grey/=n;
    if(b>0.68 && std::fabs(r-0.5)<0.22 && std::fabs(g-0.5)<0.22)return 1;
    if(grey<0.05)return 2;
    return 0;
}
struct MatExt {D3D11_TEXTURE2D_DESC desc{};std::vector<D3D11_SUBRESOURCE_DATA> init;std::vector<std::vector<unsigned char>> store;int cls=-1;};
static bool mat_match(const D3D11_TEXTURE2D_DESC *d,const D3D11_SUBRESOURCE_DATA *initial) {
    return mat_on() && d && initial && d->Width==1024 && d->Height==1024 && d->ArraySize==kMatLayers && d->Format==DXGI_FORMAT_BC3_TYPELESS &&
        d->MipLevels>=1 && d->MipLevels<=11 && d->SampleDesc.Count==1 && !(d->MiscFlags&D3D11_RESOURCE_MISC_TEXTURECUBE);
}
static bool mat_extend(const D3D11_TEXTURE2D_DESC *d,const D3D11_SUBRESOURCE_DATA *initial,MatExt &x) {
    const unsigned mips=d->MipLevels;
    for(unsigned i=0;i<kMatLayers*mips;++i)if(!initial[i].pSysMem || !initial[i].SysMemPitch)return false;
    x.cls=mat_class(initial,mips);if(x.cls<0)return false;
    x.desc=*d;x.desc.ArraySize=kMatLayers+kMatNew;
    x.init.assign(initial,initial+kMatLayers*mips);x.store.reserve(kMatNew*mips);
    for(unsigned k=0;k<kMatNew;++k)for(unsigned m=0;m<mips;++m) {
        const D3D11_SUBRESOURCE_DATA &src=initial[m];
        UINT h=1024>>m;if(!h)h=1;const UINT rows=(h+3)/4;const size_t size=static_cast<size_t>(src.SysMemPitch)*rows;
        x.store.emplace_back(static_cast<const unsigned char *>(src.pSysMem),static_cast<const unsigned char *>(src.pSysMem)+size);
        if(x.cls==0)recolor(static_cast<int>(k),m,x.store.back().data(),src.SysMemPitch,rows);
    }
    size_t at=0;for(unsigned k=0;k<kMatNew;++k)for(unsigned m=0;m<mips;++m){x.init.push_back({x.store[at].data(),initial[m].SysMemPitch,0});++at;}
    return true;
}
static void mat_created(bool matched,bool ok,int cls) {
    if(!matched)return;
    if(ok){InterlockedIncrement(&mat_streak);if(cls==0)InterlockedExchange(&mat_color,1);}
    else {InterlockedExchange(&mat_streak,0);InterlockedExchange(&mat_color,0);}
#ifndef NO_RUNTIME_LOG
    log_event("season_mat_array",static_cast<unsigned>(cls+1),ok?1u:0u,static_cast<unsigned>(InterlockedCompareExchange(&mat_streak,0,0)));
#endif
}
// The table: ints 0..18 at [7..22].x, a positive tiling at [517].x; at least three different layers among the first 16.
static bool table_ok(const unsigned char *p) {
    int seen=0;unsigned mask=0;
    for(unsigned i=0;i<16;++i){int v;memcpy(&v,p+(7+i)*16,4);if(v<0 || v>18)return false;mask|=1u<<v;}
    for(unsigned m=mask;m;m&=m-1)++seen;
    float t0,t1;memcpy(&t0,p+517*16,4);memcpy(&t1,p+772*16,4);
    return seen>=3 && t0>0 && t0<1e6f && t1>0 && t1<1e6f;
}
static void table_patch(unsigned char *p,unsigned path) {
    float t0,t1;memcpy(&t0,p+517*16,4);memcpy(&t1,p+772*16,4);
#ifndef NO_RUNTIME_LOG
    {   // local138 diagnostics: the game's material -> layer table (ids 0..15, 5 bits each) to compare with season_terrain.hlsl MAT2SLICE
        unsigned pk[3]={0,0,0};for(unsigned i=0;i<16;++i){int v;memcpy(&v,p+(7+i)*16,4);pk[i/6]|=(static_cast<unsigned>(v)&31u)<<(5*(i%6));}
        static volatile LONG once;if(!InterlockedExchange(&once,1))log_event("season_mat_layers",pk[0],pk[1],pk[2]);
    }
#endif
    for(unsigned k=0;k<kMatNew;++k) {
        const int layer=static_cast<int>(kMatLayers+k);
        memcpy(p+(7+kMatFirst+k)*16,&layer,4);memcpy(p+(517+kMatFirst+k)*16,&t0,4);memcpy(p+(772+kMatFirst+k)*16,&t1,4);
    }
#ifndef NO_RUNTIME_LOG
    static volatile LONG paths;if(!(InterlockedOr(&paths,1<<path)&(1<<path)))log_event("season_mat_table",path,1,0);
#else
    (void)path;
#endif
    InterlockedExchange(&mat_table,1);
}
using CreateBufferFn=HRESULT(STDMETHODCALLTYPE *)(ID3D11Device *,const D3D11_BUFFER_DESC *,const D3D11_SUBRESOURCE_DATA *,ID3D11Buffer **);
using CreateSrvFn=HRESULT(STDMETHODCALLTYPE *)(ID3D11Device *,ID3D11Resource *,const D3D11_SHADER_RESOURCE_VIEW_DESC *,ID3D11ShaderResourceView **);
static constexpr unsigned kSlotCreateBuffer=3,kSlotCreateSrv=7;
struct DevSlot {void **table;CreateBufferFn buffer;CreateSrvFn srv;bool published;};
static DevSlot dev_slots[MaxPsTables];
static HRESULT STDMETHODCALLTYPE create_buffer(ID3D11Device *dev,const D3D11_BUFFER_DESC *d,const D3D11_SUBRESOURCE_DATA *init,ID3D11Buffer **out) {
    CreateBufferFn original=nullptr;auto table=*reinterpret_cast<void ***>(dev);
    AcquireSRWLockShared(&ps_lock);for(const auto &s:dev_slots)if(s.table==table){original=s.buffer;break;}ReleaseSRWLockShared(&ps_lock);
    if(!original)return E_FAIL;
    if(mat_on() && !inside && d && d->ByteWidth==kTableBytes && (d->BindFlags&D3D11_BIND_CONSTANT_BUFFER) && init && init->pSysMem &&
       table_ok(static_cast<const unsigned char *>(init->pSysMem))) {
        std::vector<unsigned char> copy(static_cast<const unsigned char *>(init->pSysMem),static_cast<const unsigned char *>(init->pSysMem)+kTableBytes);
        table_patch(copy.data(),1);D3D11_SUBRESOURCE_DATA c=*init;c.pSysMem=copy.data();
        return original(dev,d,&c,out);
    }
    return original(dev,d,init,out);
}
// A view the game makes over exactly the original 19 layers is widened to our 23 (a null desc already covers all).
static HRESULT STDMETHODCALLTYPE create_srv(ID3D11Device *dev,ID3D11Resource *res,const D3D11_SHADER_RESOURCE_VIEW_DESC *d,ID3D11ShaderResourceView **out) {
    CreateSrvFn original=nullptr;auto table=*reinterpret_cast<void ***>(dev);
    AcquireSRWLockShared(&ps_lock);for(const auto &s:dev_slots)if(s.table==table){original=s.srv;break;}ReleaseSRWLockShared(&ps_lock);
    if(!original)return E_FAIL;
    if(res && d && d->ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DARRAY && d->Texture2DArray.ArraySize==kMatLayers && !d->Texture2DArray.FirstArraySlice) {
        D3D11_RESOURCE_DIMENSION dim{};res->GetType(&dim);
        if(dim==D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
            D3D11_TEXTURE2D_DESC td{};static_cast<ID3D11Texture2D *>(res)->GetDesc(&td);
            if(td.ArraySize==kMatLayers+kMatNew && td.Width==1024 && td.Format==DXGI_FORMAT_BC3_TYPELESS) {
                D3D11_SHADER_RESOURCE_VIEW_DESC w=*d;w.Texture2DArray.ArraySize=kMatLayers+kMatNew;
#ifndef NO_RUNTIME_LOG
                static volatile LONG once;if(!InterlockedExchange(&once,1))log_event("season_mat_view",kMatLayers,kMatLayers+kMatNew,0);
#endif
                return original(dev,res,&w,out);
            }
        }
    }
    return original(dev,res,d,out);
}
static HRESULT STDMETHODCALLTYPE create_ps(ID3D11Device *dev,const void *code,SIZE_T size,ID3D11ClassLinkage *link,ID3D11PixelShader **out) {
    CreatePS original=nullptr;auto table=*reinterpret_cast<void ***>(dev);
    AcquireSRWLockShared(&ps_lock);for(const auto &s:ps_slots)if(s.table==table){original=s.original;break;}ReleaseSRWLockShared(&ps_lock);
    if(!original)return E_FAIL;   // unreachable: an installed slot always keeps its original
    const int k=on() && !inside?ground_kind(code,size):-1;
    if(k>=0) {
        bool patched=false;std::vector<unsigned char> blob;
        try {patched=season_dxbc::patch_ground(static_cast<const unsigned char *>(code),size,blob);} catch(...) {patched=false;}
        if(patched) {
            HRESULT r=original(dev,blob.data(),blob.size(),link,out);
            if(SUCCEEDED(r)) {
#ifndef NO_RUNTIME_LOG
                if(!(InterlockedOr(&patched_bits,1<<k)&(1<<k)))log_event("season_shader_patch",static_cast<unsigned>(k),1,static_cast<unsigned>(blob.size()));
#else
                InterlockedOr(&patched_bits,1<<k);
#endif
                return r;
            }
        }
#ifndef NO_RUNTIME_LOG
        if(!(InterlockedOr(&patch_failed_bits,1<<k)&(1<<k)))log_event("season_shader_patch",static_cast<unsigned>(k),0,patched?2u:1u);
#endif
    }
    const int lit=on() && !inside && (mod_features.season_rain || mod_features.season_cloud)?lit_kind(code,size):-1;
    if(lit>=0) {
        HRESULT r=original(dev,code,size,link,out);
        if(SUCCEEDED(r) && out && *out) {
            InterlockedExchangePointer(reinterpret_cast<void *volatile *>(&lit_ps[lit]),*out);
#ifndef NO_RUNTIME_LOG
            log_event("season_rain_lit",static_cast<unsigned>(lit),1,0);
#endif
        }
        return r;
    }
    return original(dev,code,size,link,out);
}
static bool swap_ps(void **table,void *expected,void *replacement) {
    DWORD prior=0;if(!VirtualProtect(table+PSSlot,sizeof(void *),PAGE_READWRITE,&prior))return false;
    bool ok=InterlockedCompareExchangePointer(table+PSSlot,replacement,expected)==expected;
    DWORD ignored=0;VirtualProtect(table+PSSlot,sizeof(void *),prior,&ignored);return ok;
}
// site: the same system-d3d11 check the texture hook uses (a foreign proxy in the slot = no patch, bake mode stays).
template<class Site> static void install_ps(ID3D11Device *dev,Site site) {
    if(!dev || !on())return;
    auto table=*reinterpret_cast<void ***>(dev);
    AcquireSRWLockExclusive(&ps_lock);
    PsSlot *free_slot=nullptr;bool known=false;
    for(auto &s:ps_slots){if(s.table==table)known=true;else if(!s.table && !free_slot)free_slot=&s;}
    bool ok=false;
    if(!known && free_slot) {
        auto current=reinterpret_cast<CreatePS>(table[PSSlot]);
        if(current!=&create_ps && site(table,reinterpret_cast<void *>(current))) {
            *free_slot={table,current,false};
            if(swap_ps(table,reinterpret_cast<void *>(current),reinterpret_cast<void *>(&create_ps))){free_slot->published=true;ok=true;}
            else *free_slot={};
        }
    }
    // R2 materials: CreateBuffer (the material table's creation data) and CreateShaderResourceView (widen a 19-layer view).
    bool mat=false;
    if(!known && mod_features.season_tiles) {
        DevSlot *ds=nullptr;for(auto &s:dev_slots)if(!s.table){ds=&s;break;}
        auto buffer=reinterpret_cast<CreateBufferFn>(table[kSlotCreateBuffer]);auto srv=reinterpret_cast<CreateSrvFn>(table[kSlotCreateSrv]);
        if(ds && buffer!=&create_buffer && srv!=&create_srv && site(table,reinterpret_cast<void *>(buffer)) && site(table,reinterpret_cast<void *>(srv))) {
            *ds={table,buffer,srv,false};
            const bool swap_buf=swap_slot(table,kSlotCreateBuffer,reinterpret_cast<void *>(buffer),reinterpret_cast<void *>(&create_buffer));
            const bool swap_srv=swap_slot(table,kSlotCreateSrv,reinterpret_cast<void *>(srv),reinterpret_cast<void *>(&create_srv));
            ds->published=swap_buf || swap_srv;mat=swap_buf && swap_srv;
        }
    }
    ReleaseSRWLockExclusive(&ps_lock);
#ifndef NO_RUNTIME_LOG
    if(!known)log_event("season_shader_hook",ok?1u:0u,mat?1u:0u,0);
#else
    (void)ok;(void)mat;
#endif
}
static void restore_ps() {
    AcquireSRWLockExclusive(&ps_lock);
    for(auto &s:dev_slots)if(s.published) {
        swap_slot(s.table,kSlotCreateBuffer,reinterpret_cast<void *>(&create_buffer),reinterpret_cast<void *>(s.buffer));
        swap_slot(s.table,kSlotCreateSrv,reinterpret_cast<void *>(&create_srv),reinterpret_cast<void *>(s.srv));s.published=false;
    }
    for(auto &s:ps_slots)if(s.published){swap_ps(s.table,reinterpret_cast<void *>(&create_ps),reinterpret_cast<void *>(s.original));s.published=false;}
    ReleaseSRWLockExclusive(&ps_lock);   // originals stay saved for any wrapper already entered
}
static bool load_gpu(ID3D11Device *dev) {
    Inside guard;
    unsigned missing=0;
    for(unsigned i=0;i<DataCount;++i)if(!(gpu.data[i]=load_dds(dev,kData[i].file)))++missing;
    auto vs=read_data(L"season_bake_vs.cso"),ps=read_data(L"season_bake_ps.cso"),c1=read_data(L"season_bc1_cs.cso"),c3=read_data(L"season_bc3_cs.cso");
    bool shaders=!vs.empty() && !ps.empty() && !c1.empty() && !c3.empty() &&
        SUCCEEDED(dev->CreateVertexShader(vs.data(),vs.size(),nullptr,&gpu.vs)) && SUCCEEDED(dev->CreatePixelShader(ps.data(),ps.size(),nullptr,&gpu.ps)) &&
        SUCCEEDED(dev->CreateComputeShader(c1.data(),c1.size(),nullptr,&gpu.bc1)) && SUCCEEDED(dev->CreateComputeShader(c3.data(),c3.size(),nullptr,&gpu.bc3));
    D3D11_TEXTURE1D_DESC pd={kParams,1,1,DXGI_FORMAT_R32G32B32A32_FLOAT,D3D11_USAGE_DEFAULT,D3D11_BIND_SHADER_RESOURCE,0,0};
    bool ok=shaders && !missing && SUCCEEDED(dev->CreateTexture1D(&pd,nullptr,&gpu.params)) && SUCCEEDED(dev->CreateShaderResourceView(gpu.params,nullptr,&gpu.params_srv));
    D3D11_SAMPLER_DESC sd={D3D11_FILTER_ANISOTROPIC,D3D11_TEXTURE_ADDRESS_CLAMP,D3D11_TEXTURE_ADDRESS_CLAMP,D3D11_TEXTURE_ADDRESS_CLAMP,0,8,D3D11_COMPARISON_NEVER,{0,0,0,0},0,D3D11_FLOAT32_MAX};
    ok=ok && SUCCEEDED(dev->CreateSamplerState(&sd,&gpu.clamp));
    sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;ok=ok && SUCCEEDED(dev->CreateSamplerState(&sd,&gpu.wrap));
    D3D11_BUFFER_DESC bd={32,D3D11_USAGE_DEFAULT,D3D11_BIND_CONSTANT_BUFFER,0,0,0};
    ok=ok && SUCCEEDED(dev->CreateBuffer(&bd,nullptr,&gpu.bake_cb));
    bd.ByteWidth=16;ok=ok && SUCCEEDED(dev->CreateBuffer(&bd,nullptr,&gpu.bc_cb));
    bd.ByteWidth=80;ok=ok && SUCCEEDED(dev->CreateBuffer(&bd,nullptr,&gpu.cam_cb));
    D3D11_TEXTURE2D_DESC td={Tile,Tile,1,1,DXGI_FORMAT_R8G8B8A8_TYPELESS,{1,0},D3D11_USAGE_DEFAULT,D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,0,0};
    ok=ok && SUCCEEDED(dev->CreateTexture2D(&td,nullptr,&gpu.tile));
    if(ok) {
        D3D11_RENDER_TARGET_VIEW_DESC r={};r.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;r.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
        ok=SUCCEEDED(dev->CreateRenderTargetView(gpu.tile,&r,&gpu.tile_rtv)) && (gpu.tile_srv=view(dev,gpu.tile,DXGI_FORMAT_R8G8B8A8_UNORM,1))!=nullptr;
    }
    D3D11_TEXTURE2D_DESC bd8={Tile/4,Tile/4,1,1,DXGI_FORMAT_R32G32_UINT,{1,0},D3D11_USAGE_DEFAULT,D3D11_BIND_UNORDERED_ACCESS,0,0};
    D3D11_TEXTURE2D_DESC bd16=bd8;bd16.Format=DXGI_FORMAT_R32G32B32A32_UINT;
    ok=ok && SUCCEEDED(dev->CreateTexture2D(&bd8,nullptr,&gpu.blocks8)) && SUCCEEDED(dev->CreateTexture2D(&bd16,nullptr,&gpu.blocks16)) &&
       SUCCEEDED(dev->CreateUnorderedAccessView(gpu.blocks8,nullptr,&gpu.uav8)) && SUCCEEDED(dev->CreateUnorderedAccessView(gpu.blocks16,nullptr,&gpu.uav16));
    D3D11_RASTERIZER_DESC rd={D3D11_FILL_SOLID,D3D11_CULL_NONE,FALSE,0,0,0,TRUE,FALSE,FALSE,FALSE};
    ok=ok && SUCCEEDED(dev->CreateRasterizerState(&rd,&gpu.raster));
    D3D11_TEXTURE2D_DESC od={MapSize,MapSize,MapMips,1,DXGI_FORMAT_BC1_TYPELESS,{1,0},D3D11_USAGE_DEFAULT,D3D11_BIND_SHADER_RESOURCE,0,0};
    for(int i=0;i<2 && ok;++i)
        ok=SUCCEEDED(dev->CreateTexture2D(&od,nullptr,&gpu.own_live[i])) && SUCCEEDED(dev->CreateTexture2D(&od,nullptr,&gpu.own_next[i])) &&
           (gpu.own_srv[i]=view(dev,gpu.own_live[i],DXGI_FORMAT_BC1_UNORM_SRGB,MapMips))!=nullptr;
#ifndef NO_RUNTIME_LOG
    log_event("season_bake_load",ok?1u:0u,missing,shaders?1u:0u);
#endif
    gpu.loaded=true;return ok;
}
// ---- the calendar (pure read of the running world, same fields as native_ambush calendar()) -------------------
static int turn_key() {
    Obj root=world();Obj context=root?pointer(root,0x871E8):nullptr;if(!context)return -1;
    Obj d=context+0x34;const int month=static_cast<signed char>(d[2]),day=static_cast<signed char>(d[3]);
    if(month<1 || month>12 || day<1 || day>31)return -1;
    return (month-1)*3+(day<=10?0:day<=20?1:2);   // 0..35
}
// ---- one bake job = both maps, every mip, 512^2 tiles ----------------------------------------------------------
static unsigned items_per_map() {unsigned n=0;for(unsigned m=0;m<MapMips;++m){unsigned s=MapSize>>m,t=(s+Tile-1)/Tile;n+=t*t;}return n;}
static void set_params(ID3D11DeviceContext *c,int key) {
    static float p[kParams*4];
    if(job.epoch)memcpy(p,weather_buf,sizeof(p));   // the rules' list: day weather, cell states, lying-snow turn and day progress
    else {memset(p,0,sizeof(p));p[99*4+1]=static_cast<float>(key+1);}   // lying-snow turn (1..36), progress 0
    const int month=key/3+1,xun=key%3;
    p[0]=static_cast<float>(month);p[1]=1.0f;p[4]=static_cast<float>(xun);
    c->UpdateSubresource(gpu.params,0,nullptr,p,0,0);
}
struct Saved {   // every binding the bake touches; Get calls AddRef
    ID3D11DeviceContext *c;ID3D11RenderTargetView *rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};ID3D11DepthStencilView *dsv=nullptr;
    ID3D11BlendState *blend=nullptr;FLOAT factor[4]{};UINT mask=0;ID3D11DepthStencilState *depth=nullptr;UINT stencil=0;ID3D11RasterizerState *rs=nullptr;
    UINT vp_count=D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;D3D11_VIEWPORT vp[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
    D3D11_PRIMITIVE_TOPOLOGY topology{};ID3D11InputLayout *il=nullptr;
    ID3D11VertexShader *v=nullptr;ID3D11PixelShader *p=nullptr;ID3D11GeometryShader *g=nullptr;ID3D11HullShader *h=nullptr;ID3D11DomainShader *d=nullptr;ID3D11ComputeShader *cs=nullptr;
    ID3D11ShaderResourceView *ps3=nullptr,*ps20[2]{},*ps102[19]{},*cs0=nullptr;ID3D11SamplerState *s3=nullptr,*s7=nullptr;ID3D11Buffer *cb0=nullptr,*cb13=nullptr,*ccb0=nullptr;
    ID3D11UnorderedAccessView *uav0=nullptr;
    explicit Saved(ID3D11DeviceContext *context):c(context) {
        c->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,rtv,&dsv);c->OMGetBlendState(&blend,factor,&mask);c->OMGetDepthStencilState(&depth,&stencil);
        c->RSGetState(&rs);c->RSGetViewports(&vp_count,vp);c->IAGetPrimitiveTopology(&topology);c->IAGetInputLayout(&il);
        c->VSGetShader(&v,nullptr,nullptr);c->PSGetShader(&p,nullptr,nullptr);c->GSGetShader(&g,nullptr,nullptr);c->HSGetShader(&h,nullptr,nullptr);c->DSGetShader(&d,nullptr,nullptr);
        c->CSGetShader(&cs,nullptr,nullptr);
        c->PSGetShaderResources(3,1,&ps3);c->PSGetShaderResources(20,2,ps20);c->PSGetShaderResources(102,19,ps102);c->CSGetShaderResources(0,1,&cs0);
        c->PSGetSamplers(3,1,&s3);c->PSGetSamplers(7,1,&s7);c->PSGetConstantBuffers(0,1,&cb0);c->PSGetConstantBuffers(13,1,&cb13);c->CSGetConstantBuffers(0,1,&ccb0);
        c->CSGetUnorderedAccessViews(0,1,&uav0);
    }
    ~Saved() {
        ID3D11ShaderResourceView *none[1]={};ID3D11UnorderedAccessView *no_uav=nullptr;
        c->CSSetShaderResources(0,1,none);c->CSSetUnorderedAccessViews(0,1,&no_uav,nullptr);
        c->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,rtv,dsv);c->OMSetBlendState(blend,factor,mask);c->OMSetDepthStencilState(depth,stencil);
        c->RSSetState(rs);c->RSSetViewports(vp_count,vp);c->IASetPrimitiveTopology(topology);c->IASetInputLayout(il);
        c->VSSetShader(v,nullptr,0);c->PSSetShader(p,nullptr,0);c->GSSetShader(g,nullptr,0);c->HSSetShader(h,nullptr,0);c->DSSetShader(d,nullptr,0);c->CSSetShader(cs,nullptr,0);
        c->PSSetShaderResources(3,1,&ps3);c->PSSetShaderResources(20,2,ps20);c->PSSetShaderResources(102,19,ps102);c->CSSetShaderResources(0,1,&cs0);
        c->PSSetSamplers(3,1,&s3);c->PSSetSamplers(7,1,&s7);c->PSSetConstantBuffers(0,1,&cb0);c->PSSetConstantBuffers(13,1,&cb13);c->CSSetConstantBuffers(0,1,&ccb0);
        UINT keep=static_cast<UINT>(-1);c->CSSetUnorderedAccessViews(0,1,&uav0,&keep);
        for(auto &x:rtv)drop(x);drop(dsv);drop(blend);drop(depth);drop(rs);drop(il);drop(v);drop(p);drop(g);drop(h);drop(d);drop(cs);
        drop(ps3);for(auto &x:ps20)drop(x);for(auto &x:ps102)drop(x);drop(cs0);drop(s3);drop(s7);drop(cb0);drop(cb13);drop(ccb0);drop(uav0);
    }
};
// Item -> map, mip, tile.
static void locate(unsigned item,unsigned &map,unsigned &mip,unsigned &tx,unsigned &ty,unsigned &size) {
    const unsigned per=items_per_map();map=item/per;unsigned rest=item%per;
    for(mip=0;mip<MapMips;++mip){size=MapSize>>mip;unsigned t=(size+Tile-1)/Tile;if(rest<t*t){tx=rest%t;ty=rest/t;return;}rest-=t*t;}
    mip=MapMips-1;tx=ty=0;size=MapSize>>mip;
}
static void bake_item(ID3D11DeviceContext *c) { COST_SCOPE("season_bake:bake_item");
    unsigned map=0,mip=0,tx=0,ty=0,size=0;locate(job.item,map,mip,tx,ty,size);
    const unsigned w=size<Tile?size:Tile;
    Saved saved(c);
    const float mode=static_cast<float>(job.shader?3+map:map);
    float cb[8]={static_cast<float>(size),static_cast<float>(size),static_cast<float>(tx*Tile),static_cast<float>(ty*Tile),static_cast<float>(mip),mode,0,0};
    c->UpdateSubresource(gpu.bake_cb,0,nullptr,cb,0,0);
    ID3D11RenderTargetView *rtv[1]={gpu.tile_rtv};c->OMSetRenderTargets(1,rtv,nullptr);
    c->OMSetBlendState(nullptr,nullptr,4294967295u);c->OMSetDepthStencilState(nullptr,0);c->RSSetState(gpu.raster);
    D3D11_VIEWPORT vp={0,0,static_cast<float>(w),static_cast<float>(w),0,1};c->RSSetViewports(1,&vp);
    c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);c->IASetInputLayout(nullptr);
    c->VSSetShader(gpu.vs,nullptr,0);c->PSSetShader(gpu.ps,nullptr,0);c->GSSetShader(nullptr,nullptr,0);c->HSSetShader(nullptr,nullptr,0);c->DSSetShader(nullptr,nullptr,0);
    c->PSSetShaderResources(3,1,&control_srv);
    ID3D11ShaderResourceView *orig[2]={maps[0].orig_srv,maps[1].orig_srv};c->PSSetShaderResources(20,2,orig);
    ID3D11ShaderResourceView *data[19]={};
    for(unsigned i=0;i<DataCount;++i)data[kData[i].reg-102]=gpu.data[i];
    data[110-102]=height_srv;
    c->PSSetShaderResources(102,19,data);
    ID3D11ShaderResourceView *params=gpu.params_srv;c->PSSetShaderResources(120,1,&params);
    c->PSSetSamplers(3,1,&gpu.clamp);c->PSSetSamplers(7,1,&gpu.wrap);
    c->PSSetConstantBuffers(0,1,&gpu.bake_cb);c->PSSetConstantBuffers(13,1,&gpu.cam_cb);
    c->Draw(3,0);
    ID3D11RenderTargetView *none_rtv[1]={};c->OMSetRenderTargets(1,none_rtv,nullptr);
    ID3D11ShaderResourceView *none_ps[1]={};c->PSSetShaderResources(120,1,none_ps);
    // Compress the tile and copy the blocks into the spare texture.
    const bool bc3=!job.shader && maps[map].bc3;const unsigned blocks=w/4;
    ID3D11Texture2D *target=job.shader?gpu.own_next[map]:maps[map].next;
    unsigned cc[4]={blocks,blocks,0,0};c->UpdateSubresource(gpu.bc_cb,0,nullptr,cc,0,0);
    c->CSSetShader(bc3?gpu.bc3:gpu.bc1,nullptr,0);c->CSSetShaderResources(0,1,&gpu.tile_srv);c->CSSetConstantBuffers(0,1,&gpu.bc_cb);
    ID3D11UnorderedAccessView *uav=bc3?gpu.uav16:gpu.uav8;c->CSSetUnorderedAccessViews(0,1,&uav,nullptr);
    c->Dispatch((blocks+7)/8,(blocks+7)/8,1);
    ID3D11UnorderedAccessView *no_uav=nullptr;c->CSSetUnorderedAccessViews(0,1,&no_uav,nullptr);
    D3D11_BOX box={0,0,0,blocks,blocks,1};
    c->CopySubresourceRegion(target,D3D11CalcSubresource(mip,0,MapMips),tx*Tile,ty*Tile,0,bc3?gpu.blocks16:gpu.blocks8,0,&box);
}
static bool ready_locked() {return maps[0].live && maps[1].live && height_srv && control_srv && device;}
static bool wants() {
    if(!on())return false;
    AcquireSRWLockShared(&lock);bool r=ready_locked() && !failed;ReleaseSRWLockShared(&lock);return r;
}
static void swap_in(ID3D11DeviceContext *c,int key) { COST_SCOPE("season_bake:swap_in");
    if(job.shader){for(int i=0;i<2;++i)c->CopyResource(gpu.own_live[i],gpu.own_next[i]);}
    else {c->CopyResource(maps[0].live,maps[0].next);c->CopyResource(maps[1].live,maps[1].next);game_dirty=true;}
    live_key=key;live_shader=job.shader;
#ifndef NO_RUNTIME_LOG
    log_event("season_bake_swap",static_cast<unsigned>(key),job.shader?1u:0u,0);
#endif
}
// ---- R1c rain layer (season patch SeasonRain*/SeasonRainDrops*.hlsl, vendored in src/season) ----------------------------------
// Right after the game's lighting pass of the map layer: copy its picture (t101), the drops as geometry with the game's depth test
// (level 3, rain under the cloud), then the full-screen rain/snow haze with the game's own vertex shader (as the season patch's
// d3dx.ini: draw 3,0 from the caller's state). Inputs: the rules' IniParams list (t120, rows 0..2 head written here each frame:
// month/xun/time), the pass's depth (t4 / t5) and pixel constants (cb0, given to the drop VS as cb13). Every touched binding is
// put back. Off: nothing is hooked (season_weather off, missing .cso, or the lighting shaders of another build never seen).
struct Fx {
    bool tried=false,ok=false;
    ID3D11PixelShader *rain[2]{},*drops_ps[2]{};ID3D11VertexShader *drops_vs[2]{};
    ID3D11Texture1D *params=nullptr;ID3D11ShaderResourceView *params_srv=nullptr;
    ID3D11BlendState *blend=nullptr;ID3D11DepthStencilState *depth_on=nullptr,*depth_off=nullptr;
    ID3D11Texture2D *scene=nullptr;ID3D11ShaderResourceView *scene_srv=nullptr;UINT scene_w=0,scene_h=0;DXGI_FORMAT scene_fmt=DXGI_FORMAT_UNKNOWN;
    // local152 cloud layer: the cloud body of the same shaders (CLOUD_ONLY), our full-screen VS, and per lit kind a copy of the
    // lighting pass's pixel constants (cb0) and a reference to its depth view, taken while that pass was bound
    ID3D11PixelShader *cloud[2]{};ID3D11VertexShader *cloud_vs=nullptr;
    ID3D11Buffer *cb[2]{};UINT cb_bytes[2]{};bool cb_ok[2]{};ID3D11ShaderResourceView *depth[2]{};
};
static Fx fx;
static volatile LONG fx_level;   // 0 nothing, 2 haze, 3 drops + haze (the rules' picture level y100 while it rains somewhere, y97)
static float fx_buf[kParams*4];static unsigned fx_epoch;static bool fx_dirty;static ULONGLONG fx_fetch_at,bake_end_at,fx_t0;
static ID3D11DeviceContext *fx_ctx;static ID3D11PixelShader *cur_ps;static int in_fx;
using PSSetShaderFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,ID3D11PixelShader *,ID3D11ClassInstance *const *,UINT);
using DrawFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,UINT,UINT);
using DrawIndexedFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,UINT,UINT,INT);
static constexpr unsigned kSlotPSSetShader=9,kSlotDrawIndexed=12,kSlotDraw=13;   // ID3D11DeviceContext vtable
static void **ctx_table;static PSSetShaderFn orig_pss;static DrawFn orig_draw;static DrawIndexedFn orig_drawi;
static bool load_fx(ID3D11Device *dev) {
    Inside guard;fx.tried=true;
    bool ok=true;std::vector<unsigned char> r0;
    if(mod_features.season_rain) {
        r0=read_data(L"season_rain_ps.cso");
        auto r1=read_data(L"season_rainb_ps.cso"),v0=read_data(L"season_drops_vs.cso"),p0=read_data(L"season_drops_ps.cso"),
             v1=read_data(L"season_dropsb_vs.cso"),p1=read_data(L"season_dropsb_ps.cso");
        ok=!r0.empty() && !r1.empty() && !v0.empty() && !p0.empty() && !v1.empty() && !p1.empty() &&
            SUCCEEDED(dev->CreatePixelShader(r0.data(),r0.size(),nullptr,&fx.rain[0])) && SUCCEEDED(dev->CreatePixelShader(r1.data(),r1.size(),nullptr,&fx.rain[1])) &&
            SUCCEEDED(dev->CreateVertexShader(v0.data(),v0.size(),nullptr,&fx.drops_vs[0])) && SUCCEEDED(dev->CreatePixelShader(p0.data(),p0.size(),nullptr,&fx.drops_ps[0])) &&
            SUCCEEDED(dev->CreateVertexShader(v1.data(),v1.size(),nullptr,&fx.drops_vs[1])) && SUCCEEDED(dev->CreatePixelShader(p1.data(),p1.size(),nullptr,&fx.drops_ps[1]));
    }
    else {   // local152: the cloud layer alone
        r0=read_data(L"season_cloud_ps.cso");
        auto r1=read_data(L"season_cloudb_ps.cso"),v0=read_data(L"season_cloud_vs.cso");
        ok=!r0.empty() && !r1.empty() && !v0.empty() &&
            SUCCEEDED(dev->CreatePixelShader(r0.data(),r0.size(),nullptr,&fx.cloud[0])) && SUCCEEDED(dev->CreatePixelShader(r1.data(),r1.size(),nullptr,&fx.cloud[1])) &&
            SUCCEEDED(dev->CreateVertexShader(v0.data(),v0.size(),nullptr,&fx.cloud_vs));
    }
    D3D11_TEXTURE1D_DESC pd={kParams,1,1,DXGI_FORMAT_R32G32B32A32_FLOAT,D3D11_USAGE_DEFAULT,D3D11_BIND_SHADER_RESOURCE,0,0};
    ok=ok && SUCCEEDED(dev->CreateTexture1D(&pd,nullptr,&fx.params)) && SUCCEEDED(dev->CreateShaderResourceView(fx.params,nullptr,&fx.params_srv));
    D3D11_BLEND_DESC bd={};bd.RenderTarget[0]={TRUE,D3D11_BLEND_SRC_ALPHA,D3D11_BLEND_INV_SRC_ALPHA,D3D11_BLEND_OP_ADD,D3D11_BLEND_ZERO,D3D11_BLEND_ONE,D3D11_BLEND_OP_ADD,7};
    ok=ok && SUCCEEDED(dev->CreateBlendState(&bd,&fx.blend));
    D3D11_DEPTH_STENCIL_DESC dd={};dd.DepthEnable=TRUE;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;dd.DepthFunc=D3D11_COMPARISON_GREATER_EQUAL;
    ok=ok && SUCCEEDED(dev->CreateDepthStencilState(&dd,&fx.depth_on));
    dd.DepthEnable=FALSE;dd.DepthFunc=D3D11_COMPARISON_ALWAYS;ok=ok && SUCCEEDED(dev->CreateDepthStencilState(&dd,&fx.depth_off));
#ifndef NO_RUNTIME_LOG
    log_event("season_rain_load",ok?1u:0u,r0.empty()?0u:1u,mod_features.season_rain?0u:1u);   // hex 1 = the cloud layer alone
#endif
    return fx.ok=ok;
}
static bool scene_for(ID3D11Texture2D *rt,DXGI_FORMAT view_fmt) {
    D3D11_TEXTURE2D_DESC d={};rt->GetDesc(&d);
    if(d.SampleDesc.Count!=1 || d.ArraySize!=1)return false;
    if(fx.scene && fx.scene_w==d.Width && fx.scene_h==d.Height && fx.scene_fmt==d.Format)return true;
    drop(fx.scene_srv);drop(fx.scene);
    D3D11_TEXTURE2D_DESC s=d;s.MipLevels=1;s.Usage=D3D11_USAGE_DEFAULT;s.BindFlags=D3D11_BIND_SHADER_RESOURCE;s.CPUAccessFlags=0;s.MiscFlags=0;
    ID3D11Device *dev=nullptr;rt->GetDevice(&dev);if(!dev)return false;
    {Inside guard;if(FAILED(dev->CreateTexture2D(&s,nullptr,&fx.scene)))fx.scene=nullptr;}
    if(fx.scene)fx.scene_srv=view(dev,fx.scene,view_fmt,1);
    dev->Release();
    if(!fx.scene_srv){drop(fx.scene);return false;}
    fx.scene_w=d.Width;fx.scene_h=d.Height;fx.scene_fmt=d.Format;return true;   // mip 0 is copied (CopySubresourceRegion)
}
// Diagnostics (Fable review 2026-11-13: the layer was never drawn): lit shader binds, draws after them by call kind,
// and why run_rain gave up (1 no target, 2 not a 2D view, 3 MSAA / array / scene texture failed). One line every 5 s.
static volatile LONG rd_lit,rd_draw[4],rd_exit[4];
// local125 (user screenshot: purple wash over the rain areas): the haze runs at the END of the lit pass, so the VS (draw 3,0) and the
// pixel constants cb0 (clip -> world) may already be the next pass's. Counted per drawn haze: [0] VS differs from the one bound with the
// lit shader, [1] PS cb0 differs. season_rain_drawn hex = render-target view format | scene width << 8.
static volatile LONG rd_state[2];static ID3D11VertexShader *lit_vs;static ID3D11Buffer *lit_cb0;
static void run_rain(ID3D11DeviceContext *c,int k,LONG level) {
    ++in_fx;
    ID3D11RenderTargetView *rtv=nullptr;ID3D11DepthStencilView *dsv=nullptr;c->OMGetRenderTargets(1,&rtv,&dsv);
    ID3D11Texture2D *rt=nullptr;D3D11_RENDER_TARGET_VIEW_DESC rd={};
    if(rtv){ID3D11Resource *res=nullptr;rtv->GetResource(&res);if(res){res->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&rt));res->Release();}rtv->GetDesc(&rd);}
    if(!rt)InterlockedIncrement(&rd_exit[1]);
    else if(rd.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D)InterlockedIncrement(&rd_exit[2]);
    else if(!scene_for(rt,rd.Format))InterlockedIncrement(&rd_exit[3]);
    else {
        c->CopySubresourceRegion(fx.scene,0,0,0,0,rt,rd.Texture2D.MipSlice,nullptr);
        {ID3D11VertexShader *v=nullptr;ID3D11Buffer *b=nullptr;c->VSGetShader(&v,nullptr,nullptr);c->PSGetConstantBuffers(0,1,&b);
         if(v!=lit_vs)InterlockedIncrement(&rd_state[0]);if(b!=lit_cb0)InterlockedIncrement(&rd_state[1]);drop(v);drop(b);}
        ID3D11BlendState *blend=nullptr;FLOAT factor[4]{};UINT mask=0;c->OMGetBlendState(&blend,factor,&mask);
        ID3D11DepthStencilState *depth=nullptr;UINT stencil=0;c->OMGetDepthStencilState(&depth,&stencil);
        ID3D11RasterizerState *rs=nullptr;c->RSGetState(&rs);
        ID3D11PixelShader *ps=nullptr;c->PSGetShader(&ps,nullptr,nullptr);
        ID3D11ShaderResourceView *ps101=nullptr,*ps120=nullptr;c->PSGetShaderResources(101,1,&ps101);c->PSGetShaderResources(120,1,&ps120);
        ID3D11ShaderResourceView *scene=fx.scene_srv,*params=fx.params_srv;
        c->OMSetBlendState(fx.blend,nullptr,4294967295u);c->RSSetState(gpu.raster);
        c->PSSetShaderResources(101,1,&scene);c->PSSetShaderResources(120,1,&params);
        if(level>=3) {
            const UINT dslot=k?5u:4u;
            ID3D11VertexShader *vs=nullptr;c->VSGetShader(&vs,nullptr,nullptr);
            ID3D11InputLayout *il=nullptr;c->IAGetInputLayout(&il);D3D11_PRIMITIVE_TOPOLOGY topo{};c->IAGetPrimitiveTopology(&topo);
            ID3D11ShaderResourceView *vsd=nullptr,*vs120=nullptr,*pd=nullptr;c->VSGetShaderResources(dslot,1,&vsd);c->VSGetShaderResources(120,1,&vs120);
            ID3D11Buffer *vcb13=nullptr,*pcb0=nullptr;c->VSGetConstantBuffers(13,1,&vcb13);c->PSGetConstantBuffers(0,1,&pcb0);
            c->PSGetShaderResources(dslot,1,&pd);
            c->VSSetShaderResources(dslot,1,&pd);c->VSSetShaderResources(120,1,&params);c->VSSetConstantBuffers(13,1,&pcb0);
            c->IASetInputLayout(nullptr);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            c->VSSetShader(fx.drops_vs[k],nullptr,0);orig_pss(c,fx.drops_ps[k],nullptr,0);c->OMSetDepthStencilState(fx.depth_on,0);
            orig_draw(c,248832,0);
            c->VSSetShader(vs,nullptr,0);c->IASetInputLayout(il);c->IASetPrimitiveTopology(topo);
            c->VSSetShaderResources(dslot,1,&vsd);c->VSSetShaderResources(120,1,&vs120);c->VSSetConstantBuffers(13,1,&vcb13);
            drop(vs);drop(il);drop(vsd);drop(vs120);drop(pd);drop(vcb13);drop(pcb0);
        }
        orig_pss(c,fx.rain[k],nullptr,0);c->OMSetDepthStencilState(fx.depth_off,0);
        orig_draw(c,3,0);   // the caller's own full-screen vertex shader and topology (season patch: draw = 3, 0)
        orig_pss(c,ps,nullptr,0);c->OMSetBlendState(blend,factor,mask);c->OMSetDepthStencilState(depth,stencil);c->RSSetState(rs);
        c->PSSetShaderResources(101,1,&ps101);c->PSSetShaderResources(120,1,&ps120);
        drop(blend);drop(depth);drop(rs);drop(ps);drop(ps101);drop(ps120);
        InterlockedIncrement(&rd_exit[0]);
#ifndef NO_RUNTIME_LOG
        static volatile LONG drawn;if(!InterlockedExchange(&drawn,1))log_event("season_rain_drawn",static_cast<unsigned>(k),static_cast<unsigned>(level),static_cast<unsigned>(rd.Format)|(fx.scene_w<<8));
#endif
    }
    drop(rt);drop(rtv);drop(dsv);
    --in_fx;
}
// local123 log: the lit shader was bound ~60 times a second but no hooked Draw* ever followed it (the engine draws that pass
// some other way). The rain now also runs when the lit pass ends: the next PSSetShader or OMSetRenderTargets after a lit bind
// (the pass's targets, depth and constants are still bound then), whatever call drew it. rd_draw counts every hooked draw.
static bool lit_pending;static int lit_k;
// ---- local152 cloud layer (user 2026-11-14 "원 모드에 있는 원경에서 구름 ... 구름만 진행") --------------------------------------
// The rain layer drew at the END of the lit pass with whatever VS and cb0 were bound then (local125: a purple wash - possibly the next
// pass's). The cloud keeps its own copies instead: the lighting pass's pixel constants (b0) are copied into our buffer and its depth
// view (t4 / t5) is kept, at the pass's own draw when one is seen through a hooked Draw* (drawn=true: final for that pass), else
// whenever the pass binds or rewrites them while the lit shader is current. At the pass's end the cloud body is drawn over its
// render target with our full-screen VS, these copies, the picture copy (t101) and the rules' list (t120); the depth target is
// unbound for that one draw and every binding is put back.
using OMSetRTFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,UINT,ID3D11RenderTargetView *const *,ID3D11DepthStencilView *);
static OMSetRTFn orig_omsetrt;
static volatile LONG rd_grab[3];   // copies taken: [0] at the lit bind, [1] on a later bind / rewrite in the pass, [2] at a draw
static bool cloud_drawn;            // this pass's copy came from its draw
static ID3D11Buffer *cloud_src;     // the cb0 last copied (identity only, compared in Unmap / UpdateSubresource)
static void cloud_grab(ID3D11DeviceContext *c,int k,int why) {
    if(mod_features.season_rain || k<0 || k>1)return;
    ++in_fx;
    ID3D11Buffer *b=nullptr;c->PSGetConstantBuffers(0,1,&b);
    if(b) {
        D3D11_BUFFER_DESC d{};b->GetDesc(&d);
        if(!fx.cb[k] || fx.cb_bytes[k]!=d.ByteWidth) {
            drop(fx.cb[k]);fx.cb_ok[k]=false;
            D3D11_BUFFER_DESC o{};o.ByteWidth=d.ByteWidth;o.Usage=D3D11_USAGE_DEFAULT;o.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
            ID3D11Device *dev=nullptr;c->GetDevice(&dev);
            if(dev){Inside guard;if(FAILED(dev->CreateBuffer(&o,nullptr,&fx.cb[k])))fx.cb[k]=nullptr;dev->Release();}
            fx.cb_bytes[k]=fx.cb[k]?d.ByteWidth:0;
        }
        if(fx.cb[k] && d.ByteWidth>=31*16){c->CopyResource(fx.cb[k],b);fx.cb_ok[k]=true;InterlockedIncrement(&rd_grab[why]);}
        cloud_src=b;b->Release();
    }
    ID3D11ShaderResourceView *v=nullptr;c->PSGetShaderResources(k?5u:4u,1,&v);
    if(v){if(v!=fx.depth[k]){drop(fx.depth[k]);fx.depth[k]=v;}else v->Release();}
    --in_fx;
}
// local153 (user 2026-11-14 "전략중에 뚝뚝 끊기는데"): the local152 log showed >255 copies per 5 s from binds / rewrites (the lit
// shader stays current through many unrelated binds) - at most kCloudTouches copies per lit pass after the one at its bind.
static constexpr unsigned kCloudTouches=2;
static unsigned cloud_touches;
static void cloud_touch(ID3D11DeviceContext *c) {   // a bind or rewrite while the lit shader is current (before its draw)
    if(!lit_pending || cloud_drawn || cloud_touches>=kCloudTouches || c!=fx_ctx || in_fx)return;
    if(cur_ps && (cur_ps==lit_ps[0] || cur_ps==lit_ps[1])) {++cloud_touches;cloud_grab(c,lit_k,1);}
}
static void run_cloud(ID3D11DeviceContext *c,int k) {
    if(!fx.cloud[k] || !fx.cloud_vs || !fx.cb_ok[k] || !fx.depth[k]){InterlockedIncrement(&rd_exit[1]);return;}
    ++in_fx;
    ID3D11RenderTargetView *rtvs[8]{};ID3D11DepthStencilView *dsv=nullptr;c->OMGetRenderTargets(8,rtvs,&dsv);
    ID3D11Texture2D *rt=nullptr;D3D11_RENDER_TARGET_VIEW_DESC rd={};
    if(rtvs[0]){ID3D11Resource *res=nullptr;rtvs[0]->GetResource(&res);if(res){res->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&rt));res->Release();}rtvs[0]->GetDesc(&rd);}
    if(!rt)InterlockedIncrement(&rd_exit[1]);
    else if(rd.ViewDimension!=D3D11_RTV_DIMENSION_TEXTURE2D)InterlockedIncrement(&rd_exit[2]);
    else if(!scene_for(rt,rd.Format))InterlockedIncrement(&rd_exit[3]);
    else {
        c->CopySubresourceRegion(fx.scene,0,0,0,0,rt,rd.Texture2D.MipSlice,nullptr);
        const UINT dslot=k?5u:4u;
        ID3D11BlendState *blend=nullptr;FLOAT factor[4]{};UINT mask=0;c->OMGetBlendState(&blend,factor,&mask);
        ID3D11DepthStencilState *depth=nullptr;UINT stencil=0;c->OMGetDepthStencilState(&depth,&stencil);
        ID3D11RasterizerState *rs=nullptr;c->RSGetState(&rs);
        ID3D11PixelShader *ps=nullptr;c->PSGetShader(&ps,nullptr,nullptr);
        ID3D11VertexShader *vs=nullptr;c->VSGetShader(&vs,nullptr,nullptr);
        ID3D11InputLayout *il=nullptr;c->IAGetInputLayout(&il);D3D11_PRIMITIVE_TOPOLOGY topo{};c->IAGetPrimitiveTopology(&topo);
        ID3D11Buffer *pcb0=nullptr;c->PSGetConstantBuffers(0,1,&pcb0);
        ID3D11ShaderResourceView *psd=nullptr,*ps101=nullptr,*ps120=nullptr;
        c->PSGetShaderResources(dslot,1,&psd);c->PSGetShaderResources(101,1,&ps101);c->PSGetShaderResources(120,1,&ps120);
        ID3D11ShaderResourceView *scene=fx.scene_srv,*params=fx.params_srv,*dv=fx.depth[k];ID3D11Buffer *cb=fx.cb[k];
        orig_omsetrt(c,1,rtvs,nullptr);   // the depth target off for this draw (its view is read at t4 / t5)
        c->OMSetBlendState(fx.blend,nullptr,4294967295u);c->OMSetDepthStencilState(fx.depth_off,0);c->RSSetState(gpu.raster);
        c->IASetInputLayout(nullptr);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        c->VSSetShader(fx.cloud_vs,nullptr,0);orig_pss(c,fx.cloud[k],nullptr,0);
        c->PSSetConstantBuffers(0,1,&cb);
        c->PSSetShaderResources(dslot,1,&dv);c->PSSetShaderResources(101,1,&scene);c->PSSetShaderResources(120,1,&params);
        orig_draw(c,3,0);
        c->PSSetShaderResources(dslot,1,&psd);c->PSSetShaderResources(101,1,&ps101);c->PSSetShaderResources(120,1,&ps120);
        c->PSSetConstantBuffers(0,1,&pcb0);
        c->VSSetShader(vs,nullptr,0);orig_pss(c,ps,nullptr,0);c->IASetInputLayout(il);c->IASetPrimitiveTopology(topo);
        c->OMSetBlendState(blend,factor,mask);c->OMSetDepthStencilState(depth,stencil);c->RSSetState(rs);
        orig_omsetrt(c,8,rtvs,dsv);
        drop(blend);drop(depth);drop(rs);drop(ps);drop(vs);drop(il);drop(pcb0);drop(psd);drop(ps101);drop(ps120);
        InterlockedIncrement(&rd_exit[0]);
#ifndef NO_RUNTIME_LOG
        static volatile LONG drawn;if(!InterlockedExchange(&drawn,1))log_event("season_cloud_drawn",static_cast<unsigned>(k),fx.cb_bytes[k],static_cast<unsigned>(rd.Format)|(fx.scene_w<<8));
#endif
    }
    drop(rt);for(auto *&v:rtvs)drop(v);drop(dsv);
    --in_fx;
}
static void lit_end(ID3D11DeviceContext *c) {
    if(!lit_pending || in_fx || c!=fx_ctx)return;
    lit_pending=false;
    const LONG level=InterlockedCompareExchange(&fx_level,0,0);if(level<2)return;
    if(mod_features.season_rain)run_rain(c,lit_k,level);
    else run_cloud(c,lit_k);
}
static void after_draw(ID3D11DeviceContext *c,int kind) {
    if(in_fx || c!=fx_ctx)return;
    InterlockedIncrement(&rd_draw[kind]);
    if(!cur_ps)return;
    const int k=cur_ps==lit_ps[0]?0:cur_ps==lit_ps[1]?1:-1;if(k<0)return;
    if(!mod_features.season_rain) {   // local152 cloud: the pass's own state at its draw; drawn at the pass's end
        if(!cloud_drawn){cloud_grab(c,k,2);cloud_drawn=true;}
        return;
    }
    lit_pending=false;
    const LONG level=InterlockedCompareExchange(&fx_level,0,0);if(level<2)return;
    run_rain(c,k,level);
}
using MapFn=HRESULT(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,ID3D11Resource *,UINT,D3D11_MAP,UINT,D3D11_MAPPED_SUBRESOURCE *);
using UnmapFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,ID3D11Resource *,UINT);
using DrawIIFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,UINT,UINT,UINT,INT,UINT);
using DrawIFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,UINT,UINT,UINT,UINT);
using UpdateFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,ID3D11Resource *,UINT,const D3D11_BOX *,const void *,UINT,UINT);
static constexpr unsigned kSlotMap=14,kSlotUnmap=15,kSlotDrawII=20,kSlotDrawI=21,kSlotOMSetRT=33,kSlotUpdate=48;
static MapFn orig_map;static UnmapFn orig_unmap;static DrawIIFn orig_drawii;static DrawIFn orig_drawinst;static UpdateFn orig_update;
static ID3D11Resource *table_res;static unsigned char *table_ptr;   // immediate context only (render thread)
static bool table_buffer(ID3D11Resource *r) {
    if(!r)return false;
    D3D11_RESOURCE_DIMENSION dim{};r->GetType(&dim);if(dim!=D3D11_RESOURCE_DIMENSION_BUFFER)return false;
    D3D11_BUFFER_DESC d{};static_cast<ID3D11Buffer *>(r)->GetDesc(&d);
    return d.ByteWidth==kTableBytes && (d.BindFlags&D3D11_BIND_CONSTANT_BUFFER);
}
static HRESULT STDMETHODCALLTYPE hook_map(ID3D11DeviceContext *c,ID3D11Resource *r,UINT sub,D3D11_MAP type,UINT flags,D3D11_MAPPED_SUBRESOURCE *m) {
    const HRESULT h=orig_map(c,r,sub,type,flags,m);
    if(c==fx_ctx && SUCCEEDED(h) && m && m->pData && (type==D3D11_MAP_WRITE_DISCARD || type==D3D11_MAP_WRITE) && mat_on() && table_buffer(r)) {
        table_res=r;table_ptr=static_cast<unsigned char *>(m->pData);
    }
    return h;
}
static void STDMETHODCALLTYPE hook_unmap(ID3D11DeviceContext *c,ID3D11Resource *r,UINT sub) {
    if(c==fx_ctx && r==table_res && table_ptr) {
        if(table_ok(table_ptr))table_patch(table_ptr,2);
        table_res=nullptr;table_ptr=nullptr;
    }
    orig_unmap(c,r,sub);
    if(r && r==cloud_src)cloud_touch(c);   // local152: the lit pass's constants rewritten
}
static void STDMETHODCALLTYPE hook_update(ID3D11DeviceContext *c,ID3D11Resource *r,UINT sub,const D3D11_BOX *box,const void *src,UINT rp,UINT dp) {
    if(c==fx_ctx && !box && src && !in_fx && mat_on() && table_buffer(r) && table_ok(static_cast<const unsigned char *>(src))) {
        static unsigned char copy[kTableBytes];memcpy(copy,src,kTableBytes);table_patch(copy,3);
        orig_update(c,r,sub,box,copy,rp,dp);return;
    }
    orig_update(c,r,sub,box,src,rp,dp);
    if(r && r==cloud_src)cloud_touch(c);   // local152
}
static void STDMETHODCALLTYPE hook_pss(ID3D11DeviceContext *c,ID3D11PixelShader *ps,ID3D11ClassInstance *const *ci,UINT n) {
    if(c==fx_ctx && !in_fx) {
        if(ps!=cur_ps)lit_end(c);
        cur_ps=ps;
        if(ps && (ps==lit_ps[0] || ps==lit_ps[1])) {
            InterlockedIncrement(&rd_lit);lit_pending=true;lit_k=ps==lit_ps[0]?0:1;
            ID3D11VertexShader *v=nullptr;ID3D11Buffer *b=nullptr;c->VSGetShader(&v,nullptr,nullptr);c->PSGetConstantBuffers(0,1,&b);
            lit_vs=v;lit_cb0=b;drop(v);drop(b);   // identity only (compared in run_rain), no reference kept
            cloud_drawn=false;cloud_touches=0;
            orig_pss(c,ps,ci,n);cloud_grab(c,lit_k,0);return;   // local152: the copy as bound with the lit shader
        }
    }
    orig_pss(c,ps,ci,n);
}
// local152: the lit pass binding its constants (b0) or its depth view (t4 / t5) after the shader
using PSSetSrvFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,UINT,UINT,ID3D11ShaderResourceView *const *);
using PSSetCbFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,UINT,UINT,ID3D11Buffer *const *);
using DrawIndirectFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,ID3D11Buffer *,UINT);
static constexpr unsigned kSlotPSSetSrv=8,kSlotPSSetCb=16,kSlotDrawIIIndirect=39,kSlotDrawIIndirect=40;
static PSSetSrvFn orig_pssrv;static PSSetCbFn orig_pscb;static DrawIndirectFn orig_drawiii,orig_drawiind;
static void STDMETHODCALLTYPE hook_pssrv(ID3D11DeviceContext *c,UINT start,UINT n,ID3D11ShaderResourceView *const *v) {
    orig_pssrv(c,start,n,v);
    const UINT ds=lit_k?5u:4u;
    if(ds>=start && ds<start+n)cloud_touch(c);
}
static void STDMETHODCALLTYPE hook_pscb(ID3D11DeviceContext *c,UINT start,UINT n,ID3D11Buffer *const *b) {
    orig_pscb(c,start,n,b);
    if(start==0 && n)cloud_touch(c);
}
static void STDMETHODCALLTYPE hook_omsetrt(ID3D11DeviceContext *c,UINT n,ID3D11RenderTargetView *const *rtv,ID3D11DepthStencilView *dsv) {
    lit_end(c);orig_omsetrt(c,n,rtv,dsv);
}
static void STDMETHODCALLTYPE hook_draw(ID3D11DeviceContext *c,UINT n,UINT start) {orig_draw(c,n,start);after_draw(c,0);}
static void STDMETHODCALLTYPE hook_drawi(ID3D11DeviceContext *c,UINT n,UINT start,INT base) {orig_drawi(c,n,start,base);after_draw(c,1);}
static void STDMETHODCALLTYPE hook_drawii(ID3D11DeviceContext *c,UINT n,UINT inst,UINT start,INT base,UINT si) {orig_drawii(c,n,inst,start,base,si);after_draw(c,2);}
static void STDMETHODCALLTYPE hook_drawinst(ID3D11DeviceContext *c,UINT n,UINT inst,UINT start,UINT si) {orig_drawinst(c,n,inst,start,si);after_draw(c,3);}
static void STDMETHODCALLTYPE hook_drawiii_b(ID3D11DeviceContext *c,ID3D11Buffer *a,UINT o) {orig_drawiii(c,a,o);after_draw(c,1);}   // local152: indirect draws count as indexed / plain
static void STDMETHODCALLTYPE hook_drawiind_b(ID3D11DeviceContext *c,ID3D11Buffer *a,UINT o) {orig_drawiind(c,a,o);after_draw(c,0);}
struct CtxHook {unsigned slot;void **orig;void *hook;};
static void *ctx_orig[13];
static const CtxHook ctx_hooks[]={
    {kSlotPSSetShader,&ctx_orig[0],reinterpret_cast<void *>(&hook_pss)},{kSlotDraw,&ctx_orig[1],reinterpret_cast<void *>(&hook_draw)},
    {kSlotDrawIndexed,&ctx_orig[2],reinterpret_cast<void *>(&hook_drawi)},{kSlotDrawII,&ctx_orig[3],reinterpret_cast<void *>(&hook_drawii)},
    {kSlotDrawI,&ctx_orig[4],reinterpret_cast<void *>(&hook_drawinst)},{kSlotMap,&ctx_orig[5],reinterpret_cast<void *>(&hook_map)},
    {kSlotUnmap,&ctx_orig[6],reinterpret_cast<void *>(&hook_unmap)},{kSlotUpdate,&ctx_orig[7],reinterpret_cast<void *>(&hook_update)},
    {kSlotOMSetRT,&ctx_orig[8],reinterpret_cast<void *>(&hook_omsetrt)},
    {kSlotPSSetSrv,&ctx_orig[9],reinterpret_cast<void *>(&hook_pssrv)},{kSlotPSSetCb,&ctx_orig[10],reinterpret_cast<void *>(&hook_pscb)},
    {kSlotDrawIIIndirect,&ctx_orig[11],reinterpret_cast<void *>(&hook_drawiii_b)},{kSlotDrawIIndirect,&ctx_orig[12],reinterpret_cast<void *>(&hook_drawiind_b)}};
static unsigned ctx_published;   // bit per ctx_hooks entry
static void hook_context(ID3D11DeviceContext *c) {   // render thread, once; slots of the immediate context's class
    if(ctx_table || c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
    void **table=*reinterpret_cast<void ***>(c);
    for(const auto &h:ctx_hooks)*h.orig=table[h.slot];
    orig_pss=reinterpret_cast<PSSetShaderFn>(ctx_orig[0]);orig_draw=reinterpret_cast<DrawFn>(ctx_orig[1]);orig_drawi=reinterpret_cast<DrawIndexedFn>(ctx_orig[2]);
    orig_drawii=reinterpret_cast<DrawIIFn>(ctx_orig[3]);orig_drawinst=reinterpret_cast<DrawIFn>(ctx_orig[4]);orig_map=reinterpret_cast<MapFn>(ctx_orig[5]);
    orig_unmap=reinterpret_cast<UnmapFn>(ctx_orig[6]);orig_update=reinterpret_cast<UpdateFn>(ctx_orig[7]);orig_omsetrt=reinterpret_cast<OMSetRTFn>(ctx_orig[8]);
    orig_pssrv=reinterpret_cast<PSSetSrvFn>(ctx_orig[9]);orig_pscb=reinterpret_cast<PSSetCbFn>(ctx_orig[10]);
    orig_drawiii=reinterpret_cast<DrawIndirectFn>(ctx_orig[11]);orig_drawiind=reinterpret_cast<DrawIndirectFn>(ctx_orig[12]);
    fx_ctx=c;ctx_table=table;
    for(unsigned i=0;i<sizeof(ctx_hooks)/sizeof(*ctx_hooks);++i)
        if(swap_slot(table,ctx_hooks[i].slot,*ctx_hooks[i].orig,ctx_hooks[i].hook))ctx_published|=1u<<i;
#ifndef NO_RUNTIME_LOG
    log_event("season_rain_hook",ctx_published,sizeof(ctx_hooks)/sizeof(*ctx_hooks),0);
#endif
}
static void unhook_context() {
    if(!ctx_table)return;
    InterlockedExchange(&fx_level,0);
    for(unsigned i=0;i<sizeof(ctx_hooks)/sizeof(*ctx_hooks);++i)
        if(ctx_published&(1u<<i))swap_slot(ctx_table,ctx_hooks[i].slot,ctx_hooks[i].hook,*ctx_hooks[i].orig);
    ctx_published=0;
    // the originals stay saved: a call already inside a hook still forwards
}
static void rain_diag() {
#ifndef NO_RUNTIME_LOG
    static ULONGLONG at;const ULONGLONG t=GetTickCount64();if(t-at<5000)return;at=t;
    if(!ctx_table)return;
    const unsigned lit=static_cast<unsigned>(InterlockedExchange(&rd_lit,0));
    unsigned d[4],e[4];for(int i=0;i<4;++i){d[i]=static_cast<unsigned>(InterlockedExchange(&rd_draw[i],0));e[i]=static_cast<unsigned>(InterlockedExchange(&rd_exit[i],0));}
    auto cap=[](unsigned v){return v>255?255u:v;};
    log_event("season_rain_diag",lit,(cap(d[0])<<24)|(cap(d[1])<<16)|(cap(d[2])<<8)|cap(d[3]),
        (cap(e[0])<<24)|(cap(e[1])<<16)|(cap(e[2])<<8)|cap(e[3]));
    log_event("season_rain_bind",cap(static_cast<unsigned>(InterlockedExchange(&rd_state[0],0))),cap(static_cast<unsigned>(InterlockedExchange(&rd_state[1],0))),0);
    if(!mod_features.season_rain)   // local152: cloud copies taken at the lit bind / a later bind or rewrite / the pass's draw
        log_event("season_cloud_grab",cap(static_cast<unsigned>(InterlockedExchange(&rd_grab[0],0))),
            (cap(static_cast<unsigned>(InterlockedExchange(&rd_grab[1],0)))<<8)|cap(static_cast<unsigned>(InterlockedExchange(&rd_grab[2],0))),
            (fx.cb_ok[0]?1u:0u)|(fx.cb_ok[1]?2u:0u)|(fx.depth[0]?4u:0u)|(fx.depth[1]?8u:0u));
    log_event("season_rain_state",static_cast<unsigned>(InterlockedCompareExchange(&fx_level,0,0)),
        static_cast<unsigned>(fx_buf[97*4+1]*10)|(static_cast<unsigned>(fx_buf[100*4+1]*10)<<8)|(static_cast<unsigned>(fx_buf[60*4+3]*10)<<16),
        (mat_ready()?1u:0u)|(static_cast<unsigned>(InterlockedCompareExchange(&mat_streak,0,0))<<1)|(static_cast<unsigned>(InterlockedCompareExchange(&mat_table,0,0))<<8));
#endif
}
// The rules' list for the rain layer and the bake: fetched at most every 0.5 s; the rain level from rows 97/100.
static void fx_fetch() { COST_SCOPE("season_bake:fx_fetch");
    const ULONGLONG t=GetTickCount64();if(t-fx_fetch_at<500)return;fx_fetch_at=t;
    const unsigned e=weather_params(fx_buf,kParams*4);if(e && e!=fx_epoch){fx_epoch=e;fx_dirty=true;}
}
static void fx_frame(ID3D11DeviceContext *c,int now) {
    if(!(mod_features.season_rain || mod_features.season_cloud) || !weather_params){InterlockedExchange(&fx_level,0);return;}
    if(!fx.tried && device) {COST_SCOPE("season_bake:load_fx");load_fx(device);}
    if(!fx.ok)return;
    hook_context(c);
    LONG level=0;
    if(fx_epoch && fx_buf[97*4+1]>0.5f && fx_buf[100*4+1]>1.5f)level=fx_buf[100*4+1]>2.5f?3:2;
    if(level && now>=0) {
        if(fx_dirty){c->UpdateSubresource(fx.params,0,nullptr,fx_buf,0,0);fx_dirty=false;}
        if(!fx_t0)fx_t0=GetTickCount64();
        const float tm=static_cast<float>((GetTickCount64()-fx_t0)%3600000ull)/1000.0f;
        const float head[12]={static_cast<float>(now/3+1),1.0f,0,0, static_cast<float>(now%3),0,tm,0, 0,tm,0,0};
        D3D11_BOX box={0,0,0,3,1,1};c->UpdateSubresource(fx.params,0,&box,head,0,0);
    }
    InterlockedExchange(&fx_level,level);
}
// One step per presented frame (render thread, inside the overlay; the context is the game's immediate context).
static void frame(ID3D11DeviceContext *c) { COST_SCOPE("season_bake:frame");
    if(!c || !wants())return;
    AcquireSRWLockExclusive(&lock);
    if(!ready_locked() || failed){ReleaseSRWLockExclusive(&lock);return;}
    if(!gpu.loaded) {   // local144 probe: the one-time texture/shader load (Fable: ~65 ms on the present thread)
        bool loaded;{COST_SCOPE("season_bake:load_gpu");loaded=load_gpu(device);}
        if(!loaded){failed=true;ReleaseSRWLockExclusive(&lock);return;}
    }
    const int now=turn_key();
    const bool shader=shader_mode();
    if(!job.active && shader!=job.shader) {
        // The patched shaders appeared (or a new build lacks them): start over in the other mode. Leaving bake mode
        // puts the game maps back to their original pictures so the season is not applied twice.
        if(shader && game_dirty){c->CopyResource(maps[0].live,maps[0].orig);c->CopyResource(maps[1].live,maps[1].orig);game_dirty=false;}
        job.shader=shader;live_key=ready_key=-1;live_shader=false;
#ifndef NO_RUNTIME_LOG
        log_event("season_bake_mode",shader?1u:0u,static_cast<unsigned>(InterlockedCompareExchange(&patched_bits,0,0)),0);
#endif
    }
    // Weather: bake the rules' state once per turn (local119c re-baked on every daily list change: a 3.5~4.5 s bake running
    // back to back on the render thread = the demo-play lag). Disasters are painted per hex by the terrain painter
    // (season_tiles), the falling rain is the rain layer - neither needs a bake. The list is fetched every 0.5 s (fx_fetch).
    if(weather_params)fx_fetch();
    // Fable review: right after a turn change the list may still be the old day's (fetched every 0.5 s); wait for the rules'
    // next change (or 3 s) before the one bake of the turn, so the turn does not show yesterday's weather for 10 days.
    static int turn_seen=-1;static unsigned turn_epoch;static ULONGLONG turn_at;
    if(now!=turn_seen){turn_seen=now;turn_epoch=fx_epoch;turn_at=GetTickCount64();}
    const bool fresh=!live_epoch || fx_epoch!=turn_epoch || GetTickCount64()-turn_at>3000;
    if(now>=0 && !job.active && weather_params && fx_epoch && fresh && (now!=live_key || !live_epoch)) {
        memcpy(weather_buf,fx_buf,sizeof(weather_buf));
        job={now,0,true,GetTickCount64(),job.shader,fx_epoch};ready_key=-1;set_params(c,now);
    }
    else if(now>=0 && !(weather_params && job.epoch)) {
        if(now!=live_key && ready_key==now && !job.active)swap_in(c,now);   // the turn changed onto the pre-baked state
        else if(!job.active) {
            const int want=now!=live_key?now:(now+1)%36;   // the shown state first, then the next turn ahead
            if(want!=ready_key){job={want,0,true,GetTickCount64(),job.shader};set_params(c,want);}
        }
    }
    if(job.active) {
#ifndef NO_RUNTIME_LOG
        LARGE_INTEGER q0,q1,qf;QueryPerformanceCounter(&q0);
        bake_item(c);
        QueryPerformanceCounter(&q1);QueryPerformanceFrequency(&qf);
        const unsigned us=static_cast<unsigned>((q1.QuadPart-q0.QuadPart)*1000000/qf.QuadPart);
        static ULONGLONG slow_at;
        if(us>8000 && GetTickCount64()-slow_at>2000){slow_at=GetTickCount64();log_event("season_bake_slow_item",static_cast<unsigned>(job.item),us,0);}
#else
        bake_item(c);
#endif
        if(++job.item>=2*items_per_map()) {
            job.active=false;ready_key=job.key;
#ifndef NO_RUNTIME_LOG
            log_event("season_bake_done",static_cast<unsigned>(job.key),static_cast<unsigned>(GetTickCount64()-job.started),0);
#endif
            if(job.epoch){live_epoch=job.epoch;ready_key=-1;swap_in(c,job.key);}
            else if(now==ready_key && live_key!=now)swap_in(c,now);
        }
    }
    // The patched ground shaders read t100/t101 (unbound = original picture). Nothing in the game uses these slots;
    // bound again every frame in case the game clears its state.
    ID3D11ShaderResourceView *season[2]={};
    if(live_shader && live_key>=0){season[0]=gpu.own_srv[0];season[1]=gpu.own_srv[1];}
    c->PSSetShaderResources(100,2,season);
    if(mat_on())hook_context(c);   // R2 materials: the table may pass through Map/UpdateSubresource
    fx_frame(c,now);
    rain_diag();
    ReleaseSRWLockExclusive(&lock);
}
static void restore() {
    restore_ps();
    unhook_context();
    AcquireSRWLockExclusive(&lock);failed=true;reset_locked();
    for(int i=0;i<2;++i){drop(gpu.own_srv[i]);drop(gpu.own_live[i]);drop(gpu.own_next[i]);}
    for(auto &s:gpu.data)drop(s);
    drop(gpu.vs);drop(gpu.ps);drop(gpu.bc1);drop(gpu.bc3);drop(gpu.params_srv);drop(gpu.params);drop(gpu.clamp);drop(gpu.wrap);
    drop(gpu.bake_cb);drop(gpu.cam_cb);drop(gpu.bc_cb);drop(gpu.tile_rtv);drop(gpu.tile_srv);drop(gpu.tile);drop(gpu.uav8);drop(gpu.uav16);
    drop(gpu.blocks8);drop(gpu.blocks16);drop(gpu.raster);
    for(int i=0;i<2;++i){drop(fx.rain[i]);drop(fx.drops_ps[i]);drop(fx.drops_vs[i]);drop(fx.cloud[i]);drop(fx.cb[i]);drop(fx.depth[i]);fx.cb_ok[i]=false;fx.cb_bytes[i]=0;}
    drop(fx.cloud_vs);cloud_src=nullptr;
    drop(fx.params_srv);drop(fx.params);drop(fx.blend);drop(fx.depth_on);drop(fx.depth_off);drop(fx.scene_srv);drop(fx.scene);fx.ok=false;
    ReleaseSRWLockExclusive(&lock);
}
}
