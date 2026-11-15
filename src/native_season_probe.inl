// Season map G0 probe (user 2026-11-13 "셰이더 최소수정까지 해서 만들어 보자").
// Diagnostic DLL only: the whole probe is compiled out of NO_RUNTIME_LOG builds.
// [diagnostics] season_probe bits: 1 creation log, 2 shader bytecode dump,
// 4 tint large initial-data textures with one colour stripe each (screen test of
// the "working copy" trap). Creation-time only: no per-frame Set*/Draw hook.
// Shader dumps are derived from the game and stay under data/diagnostics (never git/release).
#pragma once
#ifndef NO_RUNTIME_LOG
namespace season_probe {
static unsigned bits() {return enabled && mod_features.convenience ? mod_features.season_probe : 0u;}
static bool on() {return bits()!=0;}
static LARGE_INTEGER start_qpc,qpc_rate;
static volatile LONG texture_seq,shader_seq,tint_seq,dump_count,env_logged;
static constexpr unsigned MaxDumps=3000,MaxTints=64;
static double now_ms() {
    if(!qpc_rate.QuadPart)return 0;
    LARGE_INTEGER t;QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart-start_qpc.QuadPart)*1000.0/static_cast<double>(qpc_rate.QuadPart);
}
// 3DMigoto names ShaderFixes by fnv_64_buf (FNV-1, basis 0) over the whole bytecode;
// the FNV-1a variant is logged too in case that assumption is wrong.
static void hashes(const void *data,size_t size,unsigned long long &migoto,unsigned long long &fnv1a) {
    constexpr unsigned long long prime=0x100000001b3ull;
    unsigned long long a=0,b=0xcbf29ce484222325ull;
    auto p=static_cast<const unsigned char *>(data);
    for(size_t i=0;i<size;++i){a*=prime;a^=p[i];b^=p[i];b*=prime;}
    migoto=a;fnv1a=b;
}
static bool probe_directory(wchar_t *out) {
    DWORD length=GetModuleFileNameW(nullptr,out,MAX_PATH);
    if(!length || length>=MAX_PATH)return false;
    wchar_t *slash=wcsrchr(out,L'\\');if(!slash)return false;*slash=0;
    if(wcscat_s(out,MAX_PATH,L"\\AI_TacticsMod\\data"))return false;CreateDirectoryW(out,nullptr);
    if(wcscat_s(out,MAX_PATH,L"\\diagnostics"))return false;CreateDirectoryW(out,nullptr);
    if(wcscat_s(out,MAX_PATH,L"\\season_probe"))return false;CreateDirectoryW(out,nullptr);
    return true;
}
static void dump_shader(const wchar_t *kind,unsigned long long hash,const void *data,SIZE_T size) {
    if(!(bits()&2u) || !data || !size || size>(16u<<20))return;
    // The shaders the season map replaces are always dumped; others stop at MaxDumps
    // (local111: more than 3,000 distinct shaders).
    static constexpr unsigned long long kSeasonTargets[8]={0xd3e1f71e809bac74ull,0x9addb6cb884c7a7cull,0x425be6c323067e73ull,
        0x48bed74946919969ull,0x8f8964766ab0977eull,0xd28d1d5e4d8d3a3cull,0xc9a4e0386624d4d2ull,0xfbe6dbf68635f4f4ull};
    bool target=false;for(auto t:kSeasonTargets)if(t==hash)target=true;
    if(!target && static_cast<unsigned>(InterlockedCompareExchange(&dump_count,0,0))>=MaxDumps)return;
    wchar_t path[MAX_PATH]={};
    if(!probe_directory(path))return;
    wchar_t name[48]={};
    if(swprintf_s(name,L"\\%s_%016llx.dxbc",kind,hash)<0 || wcscat_s(path,name))return;
    // local110: the game recreates the same shaders thousands of times; count new files only.
    HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return; // Same hash already dumped.
    InterlockedIncrement(&dump_count);
    DWORD written=0;WriteFile(file,data,static_cast<DWORD>(size),&written,nullptr);CloseHandle(file);
}
static void log_environment() {
    if(InterlockedExchange(&env_logged,1))return;
    if(!qpc_rate.QuadPart){QueryPerformanceFrequency(&qpc_rate);QueryPerformanceCounter(&start_qpc);}
    wchar_t loaded[MAX_PATH]={},system[MAX_PATH]={},ini[MAX_PATH]={};
    HMODULE d3d=GetModuleHandleW(L"d3d11.dll");
    bool is_system=false;
    if(d3d && GetModuleFileNameW(d3d,loaded,MAX_PATH) && GetSystemDirectoryW(system,MAX_PATH) &&
       !wcscat_s(system,L"\\d3d11.dll"))is_system=_wcsicmp(loaded,system)==0;
    bool d3dx=false;
    DWORD length=GetModuleFileNameW(nullptr,ini,MAX_PATH);
    if(length && length<MAX_PATH) {
        wchar_t *slash=wcsrchr(ini,L'\\');
        if(slash){*slash=0;if(!wcscat_s(ini,L"\\d3dx.ini"))d3dx=GetFileAttributesW(ini)!=INVALID_FILE_ATTRIBUTES;}
    }
    log_event("season_probe_env",d3d?1u:0u,is_system?1u:0u,(d3dx?1u:0u)|(bits()<<4));
}
// ---- textures ----------------------------------------------------------------
static bool large(const D3D11_TEXTURE2D_DESC &d) {return d.Width>=1024 && d.Height>=1024;}
static unsigned mip_count(const D3D11_TEXTURE2D_DESC &d) {
    if(d.MipLevels)return d.MipLevels;
    unsigned n=1,size=d.Width>d.Height?d.Width:d.Height;while(size>1){size>>=1;++n;}return n;
}
// 0 none, else bytes per 4x4 block (8/16) or 4 for 32-bit pixels (negative-free tag below).
enum TintKind : unsigned {TintNone,TintBC1,TintBC3,TintBC4,TintBC7,TintRGBA,TintBGRA};
static TintKind tint_kind(DXGI_FORMAT f) {
    switch(f) {
    // local110 log: the game creates its assets TYPELESS (70/76/79/97) with IMMUTABLE usage.
    case DXGI_FORMAT_BC1_TYPELESS:case DXGI_FORMAT_BC1_UNORM:case DXGI_FORMAT_BC1_UNORM_SRGB:return TintBC1;
    case DXGI_FORMAT_BC3_TYPELESS:case DXGI_FORMAT_BC3_UNORM:case DXGI_FORMAT_BC3_UNORM_SRGB:return TintBC3;
    case DXGI_FORMAT_BC4_TYPELESS:case DXGI_FORMAT_BC4_UNORM:return TintBC4;
    case DXGI_FORMAT_BC7_TYPELESS:case DXGI_FORMAT_BC7_UNORM:case DXGI_FORMAT_BC7_UNORM_SRGB:return TintBC7;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:return TintRGBA;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:case DXGI_FORMAT_B8G8R8A8_UNORM:case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:return TintBGRA;
    default:return TintNone;
    }
}
static constexpr unsigned rgb(unsigned r,unsigned g,unsigned b) {return (r<<16)|(g<<8)|b;}
// red, green, blue, yellow, magenta, cyan, white, orange (stripe band 0..7)
static constexpr unsigned kPalette[8]={rgb(255,0,0),rgb(0,255,0),rgb(0,0,255),rgb(255,255,0),rgb(255,0,255),rgb(0,255,255),rgb(255,255,255),rgb(255,128,0)};
static unsigned short rgb565(unsigned rgb) {
    unsigned r=(rgb>>16)&255,g=(rgb>>8)&255,b=rgb&255;
    return static_cast<unsigned short>(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
static void put_bits(unsigned char *block,unsigned &pos,unsigned value,unsigned count) {
    for(unsigned i=0;i<count;++i,++pos)if(value&(1u<<i))block[pos>>3]|=static_cast<unsigned char>(1u<<(pos&7));
}
// BC7 mode 6: 7 mode bits, RGBA endpoints 7 bits x2 each, 2 p-bits, 63 index bits (all 0 -> endpoint 0).
static void bc7_solid(unsigned char *block,unsigned rgb) {
    memset(block,0,16);unsigned pos=0;put_bits(block,pos,1u<<6,7);
    const unsigned c[4]={(rgb>>16)&255,(rgb>>8)&255,rgb&255,255};
    for(unsigned ch=0;ch<4;++ch){put_bits(block,pos,c[ch]>>1,7);put_bits(block,pos,c[ch]>>1,7);}
    put_bits(block,pos,1,1);put_bits(block,pos,1,1); // p-bit 1: 7-bit 0x7F -> 0xFF; colours here are 0/255 heavy
}
static void fill_block(unsigned char *dst,TintKind kind,unsigned rgb) {
    if(kind==TintBC7){bc7_solid(dst,rgb);return;}
    if(kind==TintBC4){dst[0]=255;dst[1]=255;memset(dst+2,0,6);return;} // single channel: full value
    unsigned char *color=dst;
    if(kind==TintBC3){dst[0]=255;dst[1]=255;memset(dst+2,0,6);color=dst+8;}
    unsigned short c0=rgb565(rgb);if(!c0)c0=1;
    color[0]=static_cast<unsigned char>(c0);color[1]=static_cast<unsigned char>(c0>>8);
    color[2]=color[3]=0;memset(color+4,0,4); // c0>c1: four-colour mode, index 0 = c0
}
struct TintStore {
    std::vector<std::vector<unsigned char>> bytes;
    std::vector<D3D11_SUBRESOURCE_DATA> data;
};
// Returns a patched copy of the initial data, or nullptr to keep the original.
static const D3D11_SUBRESOURCE_DATA *tint(const D3D11_TEXTURE2D_DESC *desc,const D3D11_SUBRESOURCE_DATA *initial,TintStore &store,unsigned &ordinal) {
    if(!(bits()&4u) || !desc || !initial || !initial[0].pSysMem)return nullptr;
    // local111: 2048-class stripes also hit menu art; the ground followed a 4096-class asset.
    // G0b tints only 4096+ assets, one full-width horizontal band each (north->south order on the map).
    if(desc->Width<4096 || desc->Height<4096 || desc->SampleDesc.Count>1)return nullptr;
    TintKind kind=tint_kind(desc->Format);if(kind==TintNone)return nullptr;
    LONG k=InterlockedIncrement(&tint_seq)-1;if(k>=static_cast<LONG>(MaxTints))return nullptr;
    ordinal=static_cast<unsigned>(k);
    // Ordinal k -> colour k%8 and stripe position (k/8)%8: 64 distinct marks.
    const unsigned rgb=kPalette[ordinal%8],band=(ordinal/8)%8;
    const unsigned mips=mip_count(*desc),count=mips*(desc->ArraySize?desc->ArraySize:1);
    const bool block=kind==TintBC1 || kind==TintBC3 || kind==TintBC4 || kind==TintBC7;
    const unsigned unit=(kind==TintBC1 || kind==TintBC4)?8u:(block?16u:4u);
    try {
        store.bytes.resize(count);store.data.resize(count);
        for(unsigned i=0;i<count;++i) {
            const auto &src=initial[i];
            if(!src.pSysMem)return nullptr;
            unsigned mip=i%mips,w=desc->Width>>mip,h=desc->Height>>mip;if(!w)w=1;if(!h)h=1;
            unsigned cols=block?(w+3)/4:w,rows=block?(h+3)/4:h;
            size_t row_bytes=static_cast<size_t>(cols)*unit;
            if(src.SysMemPitch<row_bytes)return nullptr;
            size_t size=static_cast<size_t>(rows-1)*src.SysMemPitch+row_bytes;
            auto &out=store.bytes[i];out.assign(static_cast<const unsigned char *>(src.pSysMem),static_cast<const unsigned char *>(src.pSysMem)+size);
            const unsigned row_band=ordinal%8;
            unsigned c0=0,c1=cols;(void)band;
            unsigned r0=rows*row_band/8,r1=rows*(row_band+1)/8;if(r1<=r0)r1=r0+1;if(r1>rows)r1=rows;
            unsigned char pixel[16]={};
            if(block)fill_block(pixel,kind,rgb);
            else {
                unsigned r=(rgb>>16)&255,g=(rgb>>8)&255,b=rgb&255;
                unsigned value=kind==TintRGBA?(r|(g<<8)|(b<<16)|0xff000000u):(b|(g<<8)|(r<<16)|0xff000000u);
                memcpy(pixel,&value,4);
            }
            for(unsigned y=r0;y<r1;++y)for(unsigned x=c0;x<c1;++x)
                memcpy(out.data()+static_cast<size_t>(y)*src.SysMemPitch+static_cast<size_t>(x)*unit,pixel,unit);
            store.data[i]={out.data(),src.SysMemPitch,src.SysMemSlicePitch};
        }
    } catch(...) {return nullptr;}
    return store.data.data();
}
// Bit 8 (G1 bake fixture input): raw initial data of the 4096-class map textures and the 2049 control/height maps.
// File: u32 magic 'SPTX', Width, Height, MipLevels, ArraySize, Format, count; per subresource u32 row_bytes, rows, then rows packed.
static volatile LONG raw_seq;
static void dump_texture(const D3D11_TEXTURE2D_DESC *desc,const D3D11_SUBRESOURCE_DATA *initial) {
    if(!(bits()&8u) || !desc || !initial || !initial[0].pSysMem)return;
    const bool map4096=desc->Width==4096 && desc->Height==4096,map2049=desc->Width==2049 && desc->Height==2049;
    if(!map4096 && !map2049)return;
    const DXGI_FORMAT f=desc->Format;
    const bool bc8=(f>=70 && f<=72) || (f>=79 && f<=81);
    const bool bc16=(f>=73 && f<=78) || (f>=82 && f<=84) || (f>=94 && f<=99);
    unsigned texel=0;
    if(!bc8 && !bc16) {
        if(f==DXGI_FORMAT_R32_FLOAT || f==DXGI_FORMAT_R32_TYPELESS || (f>=27 && f<=32) || (f>=87 && f<=93) || f==DXGI_FORMAT_R10G10B10A2_UNORM)texel=4;
        else if(f==DXGI_FORMAT_R16_FLOAT || f==DXGI_FORMAT_R16_UNORM || f==DXGI_FORMAT_R16_TYPELESS)texel=2;
        else if(f==DXGI_FORMAT_R8_UNORM || f==DXGI_FORMAT_R8_TYPELESS)texel=1;
        else return;
    }
    wchar_t path[MAX_PATH]={};
    if(!probe_directory(path))return;
    wchar_t name[64]={};
    unsigned seq=static_cast<unsigned>(InterlockedIncrement(&raw_seq));
    if(seq>24 || swprintf_s(name,L"\\tex_%u_%ux%u_f%u.sptx",seq,desc->Width,desc->Height,static_cast<unsigned>(f))<0 || wcscat_s(path,name))return;
    HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    const unsigned mips=mip_count(*desc),count=mips*(desc->ArraySize?desc->ArraySize:1);
    const UINT head[7]={0x58545053u,desc->Width,desc->Height,mips,desc->ArraySize,static_cast<UINT>(f),count};
    DWORD written=0;bool ok=WriteFile(file,head,sizeof(head),&written,nullptr)!=0;
    for(unsigned i=0;ok && i<count;++i) {
        unsigned mip=i%mips,w=desc->Width>>mip,h=desc->Height>>mip;if(!w)w=1;if(!h)h=1;
        UINT rows=(bc8||bc16)?(h+3)/4:h,row_bytes=(bc8||bc16)?((w+3)/4)*(bc8?8u:16u):w*texel;
        if(!initial[i].pSysMem || initial[i].SysMemPitch<row_bytes){ok=false;break;}
        const UINT sub[2]={row_bytes,rows};ok=WriteFile(file,sub,sizeof(sub),&written,nullptr)!=0;
        auto src=static_cast<const unsigned char *>(initial[i].pSysMem);
        for(UINT r=0;ok && r<rows;++r)ok=WriteFile(file,src+static_cast<size_t>(r)*initial[i].SysMemPitch,row_bytes,&written,nullptr)!=0;
    }
    CloseHandle(file);
    log_event("season_tex_dump",seq,static_cast<unsigned>(f),ok?1u:0u);
}
static void texture_created(const D3D11_TEXTURE2D_DESC *desc,const D3D11_SUBRESOURCE_DATA *initial,HRESULT result,double ms,int tint_ordinal) {
    dump_texture(desc,initial);
    if(!(bits()&1u) || !desc || !large(*desc))return;
    log_environment();
    unsigned seq=static_cast<unsigned>(InterlockedIncrement(&texture_seq));
    unsigned long long h=0,unused=0;
    if(initial && initial[0].pSysMem && initial[0].SysMemPitch) {
        size_t sample=static_cast<size_t>(initial[0].SysMemPitch)*4u;if(sample>65536)sample=65536;
        hashes(initial[0].pSysMem,sample,h,unused);
    }
    log_event("season_tex",seq,desc->Width,(static_cast<unsigned>(desc->Format)<<16)|desc->Height);
    log_event("season_tex2",seq,desc->MipLevels*1000u+desc->ArraySize,desc->Usage*100000u+desc->BindFlags*10u+(initial?1u:0u));
    log_event("season_tex3",seq,static_cast<unsigned>(ms*1000.0),(SUCCEEDED(result)?0u:1u)|(desc->MiscFlags<<4));
    log_event("season_tex4",seq,static_cast<unsigned>(h>>32),static_cast<unsigned>(h));
    if(tint_ordinal>=0)log_event("season_tint",seq,static_cast<unsigned>(tint_ordinal),kPalette[tint_ordinal%8]|((static_cast<unsigned>(tint_ordinal)/8u%8u)<<24));
}
// ---- shaders (ID3D11Device slots 12 VS, 15 PS) --------------------------------
using CreateVS=HRESULT(STDMETHODCALLTYPE *)(ID3D11Device *,const void *,SIZE_T,ID3D11ClassLinkage *,ID3D11VertexShader **);
using CreatePS=HRESULT(STDMETHODCALLTYPE *)(ID3D11Device *,const void *,SIZE_T,ID3D11ClassLinkage *,ID3D11PixelShader **);
static constexpr unsigned VSSlot=12,PSSlot=15,MaxShaderTables=4;
struct ShaderSlot {void **table;CreateVS vs;CreatePS ps;bool published;};
static ShaderSlot shader_slots[MaxShaderTables]={};
static SRWLOCK shader_lock=SRWLOCK_INIT;
static bool shader_stopping;
static void shader_created(unsigned kind,const void *code,SIZE_T size,HRESULT result) {
    if(!(bits()&3u))return;
    unsigned long long migoto=0,fnv1a=0;
    if(code && size)hashes(code,size,migoto,fnv1a);
    unsigned seq=static_cast<unsigned>(InterlockedIncrement(&shader_seq));
    if(bits()&1u) {
        log_event("season_shader",kind*1000000u+seq,static_cast<unsigned>(size),(SUCCEEDED(result)?0u:1u)|(static_cast<unsigned>(now_ms()/1000.0)<<4));
        log_event("season_shader_hash",seq,static_cast<unsigned>(migoto>>32),static_cast<unsigned>(migoto));
        log_event("season_shader_hash1a",seq,static_cast<unsigned>(fnv1a>>32),static_cast<unsigned>(fnv1a));
    }
    dump_shader(kind==2?L"ps":L"vs",migoto,code,size);
}
template<class T> static T shader_original(ID3D11Device *dev,bool pixel) {
    auto table=*reinterpret_cast<void ***>(dev);T found=nullptr;
    AcquireSRWLockShared(&shader_lock);
    for(const auto &s:shader_slots)if(s.table==table){found=reinterpret_cast<T>(pixel?reinterpret_cast<void *>(s.ps):reinterpret_cast<void *>(s.vs));break;}
    ReleaseSRWLockShared(&shader_lock);
    return found;
}
static HRESULT STDMETHODCALLTYPE create_vs(ID3D11Device *dev,const void *code,SIZE_T size,ID3D11ClassLinkage *link,ID3D11VertexShader **out) {
    auto original=shader_original<CreateVS>(dev,false);
    if(!original)return E_FAIL; // Unreachable: only published tables point here.
    HRESULT result=original(dev,code,size,link,out);
    shader_created(1,code,size,result);
    return result;
}
static HRESULT STDMETHODCALLTYPE create_ps(ID3D11Device *dev,const void *code,SIZE_T size,ID3D11ClassLinkage *link,ID3D11PixelShader **out) {
    auto original=shader_original<CreatePS>(dev,true);
    if(!original)return E_FAIL;
    HRESULT result=original(dev,code,size,link,out);
    shader_created(2,code,size,result);
    return result;
}
static bool swap_slot(void **table,unsigned slot,void *expected,void *replacement) {
    DWORD prior=0;
    if(!VirtualProtect(table+slot,sizeof(void *),PAGE_READWRITE,&prior))return false;
    bool ok=InterlockedCompareExchangePointer(table+slot,replacement,expected)==expected;
    DWORD ignored=0;VirtualProtect(table+slot,sizeof(void *),prior,&ignored);
    return ok;
}
// `site` is terrain_render::original_site (system d3d11.dll code + its own vtable).
template<class Site> static void install(ID3D11Device *dev,Site site) {
    if(!dev || !(bits()&3u))return;
    log_environment();
    auto table=*reinterpret_cast<void ***>(dev);
    bool installed=false,rejected=false;
    AcquireSRWLockExclusive(&shader_lock);
    if(!shader_stopping) {
        ShaderSlot *free=nullptr;bool known=false;
        for(auto &s:shader_slots){if(s.table==table)known=true;else if(!s.table && !free)free=&s;}
        if(!known && free) {
            auto vs=reinterpret_cast<CreateVS>(table[VSSlot]);auto ps=reinterpret_cast<CreatePS>(table[PSSlot]);
            if(!site(table,reinterpret_cast<void *>(vs)) || !site(table,reinterpret_cast<void *>(ps)))rejected=true;
            else {
                *free={table,vs,ps,false};
                bool v=swap_slot(table,VSSlot,reinterpret_cast<void *>(vs),reinterpret_cast<void *>(&create_vs));
                bool p=swap_slot(table,PSSlot,reinterpret_cast<void *>(ps),reinterpret_cast<void *>(&create_ps));
                free->published=v||p;installed=v&&p;
                if(!free->published)*free={};
            }
        }
    }
    ReleaseSRWLockExclusive(&shader_lock);
    if(installed || rejected)log_event("season_probe_hook",installed?1u:0u,rejected?1u:0u,static_cast<unsigned>(now_ms()));
}
static void restore() {
    AcquireSRWLockExclusive(&shader_lock);shader_stopping=true;
    for(auto &s:shader_slots)if(s.published) {
        swap_slot(s.table,VSSlot,reinterpret_cast<void *>(&create_vs),reinterpret_cast<void *>(s.vs));
        swap_slot(s.table,PSSlot,reinterpret_cast<void *>(&create_ps),reinterpret_cast<void *>(s.ps));
        s.published=false; // Keep originals for wrappers already entered.
    }
    ReleaseSRWLockExclusive(&shader_lock);
}
}
#endif
