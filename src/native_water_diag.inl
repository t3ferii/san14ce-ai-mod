// User 2026-10-29: one-run, observation-only water diagnostics.
#pragma once
#ifndef NO_RUNTIME_LOG
#define SAN14_WATER_DIAG_INCLUDED 1
#include <d3d11.h>
#include <bcrypt.h>
#include <algorithm>
#include <initializer_list>
namespace water_diag {
// local54: renderer owns all writes; this observer supplies the actual VS t0
// identity and cb0 UV only after the existing read-only sample succeeds.
static void (*mask_sample)(ID3D11Texture2D *,const float *)=nullptr;
static constexpr unsigned MaxSites=32,MaxContexts=4,MaxShaders=32;
static SRWLOCK hook_lock=SRWLOCK_INIT,shader_lock=SRWLOCK_INIT;
static bool stopping=false,permitted=true,early_attempted=false;
static volatile LONG observing=1,fail_reports=0;
static HMODULE library; // Keep originals mapped through in-flight callbacks.
static bool active() {return InterlockedCompareExchange(&observing,1,1) && enabled && mod_features.convenience && mod_features.terrain_edit;}
// User 2026-10-29: the per-bind VSSetShader observer cost frames once its
// answer (one ocean plane, level 450) was known. Install only on request.
#ifndef WATER_DIAG_WANTED
#define WATER_DIAG_WANTED() (mod_setting(L"diagnostics",L"water_diag",0,L"diagnostics.ini")!=0)
#endif
static bool wanted() {static const bool value=WATER_DIAG_WANTED();return value;}
struct Site {void **table;unsigned slot;void *original,*replacement;unsigned char bytes[16];bool published;bool context_table;};
static Site sites[MaxSites];
struct Shader {IUnknown *object;unsigned kind;};
static Shader shaders[5][MaxShaders];
static unsigned shader_counts[5];
struct Budget {
    unsigned first=0,binds=0,bits=0;ULONGLONG last_sample=0,last_report=0;
    bool sample(ULONGLONG now) {
        if(first<8){++first;last_sample=now;return true;}
        if(now-last_sample<5000)return false;
        last_sample=now;return true;
    }
};
struct Context {
    ID3D11DeviceContext *object; // Owned until restore; excludes deferred contexts.
    ID3D11VertexShader *current; // Identity only; the context owns its binding.
    ID3D11Buffer *staging;UINT width;bool readback_failed;bool disabled;
    Budget budget;
};
static Context contexts[MaxContexts];
static bool original_site(void **table,void *original,bool context_table=false) {
#ifdef WATER_DIAG_FIXTURE_SITE
    return water_diag_fixture_site(table,original,context_table);
#else
    HMODULE code=nullptr,data=nullptr;
    if(!original || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(original),&code))return false;
    bool table_in_image=GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(table),&data)!=FALSE;
    if(context_table) {
        // User/Claude 2026-10-29: D3D11 allocates a private table per immediate
        // context. Never extend this exception to device tables or image hooks.
        MEMORY_BASIC_INFORMATION region={};
        if(table_in_image || !VirtualQuery(table,&region,sizeof(region)) || region.State!=MEM_COMMIT ||
           region.Type!=MEM_PRIVATE || (region.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
           !(region.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
        auto offset=reinterpret_cast<ULONG_PTR>(table)-reinterpret_cast<ULONG_PTR>(region.BaseAddress);
        if(offset>region.RegionSize || region.RegionSize-offset<12*sizeof(void *))return false;
    } else if(!table_in_image || data!=code)return false;
    wchar_t path[MAX_PATH]={},system[MAX_PATH]={};
    DWORD length=GetModuleFileNameW(code,path,MAX_PATH);UINT size=GetSystemDirectoryW(system,MAX_PATH);
    constexpr wchar_t suffix[]=L"\\d3d11.dll";
    if(!length || length>=MAX_PATH || !size || size+sizeof(suffix)/sizeof(*suffix)>MAX_PATH ||
       wcscat_s(system,MAX_PATH,suffix) || _wcsicmp(path,system))return false;
    MEMORY_BASIC_INFORMATION info={};
    return VirtualQuery(original,&info,sizeof(info)) && info.State==MEM_COMMIT &&
        !(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)) &&
        (info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
#endif
}
static bool site_matches(const Site &s) {
    return original_site(s.table,s.original,s.context_table) && s.table[s.slot]==s.replacement &&
        !memcmp(s.bytes,s.original,sizeof(s.bytes));
}
static bool guards_match() {
    bool ok=true;AcquireSRWLockShared(&hook_lock);
    for(const auto &s:sites)if(s.published && !s.context_table && !site_matches(s))ok=false;
    ReleaseSRWLockShared(&hook_lock);return ok;
}
static void *original(void *object,unsigned slot) {
    auto table=*reinterpret_cast<void ***>(object);void *result=nullptr;
    AcquireSRWLockShared(&hook_lock);
    for(const auto &s:sites)if(s.table==table && s.slot==slot){result=s.original;break;}
    ReleaseSRWLockShared(&hook_lock);return result;
}
static void restore_site(Site &s) {
    if(!s.published)return;
    if(s.context_table && s.table[s.slot]!=s.replacement){s.published=false;return;}
    DWORD prior=0;
    if(!VirtualProtect(s.table+s.slot,sizeof(void *),PAGE_READWRITE,&prior))return;
    InterlockedCompareExchangePointer(s.table+s.slot,s.original,s.replacement);
    DWORD ignored=0;VirtualProtect(s.table+s.slot,sizeof(void *),prior,&ignored);s.published=false;
}
// Transactional per-table publication. Failure affects only this observer.
static bool install_slots(void *object,const unsigned *slots,void *const *hooks,unsigned count,bool context_table=false) {
    auto table=*reinterpret_cast<void ***>(object);bool ok=true;Site *added[7]={};unsigned used=0;
    AcquireSRWLockExclusive(&hook_lock);
    if(stopping || !permitted)ok=false;
    for(unsigned i=0;ok && i<count;++i) {
        Site *known=nullptr,*free=nullptr;
        for(auto &s:sites){if(s.table==table && s.slot==slots[i])known=&s;else if(!s.table && !free)free=&s;}
        if(known && known->published){ok=site_matches(*known);continue;}
        if(known) {
            // A rolled-back probe can be retried on the real device. Retain its
            // saved original for wrappers that entered during publication.
            if(table[slots[i]]!=known->original || memcmp(known->bytes,known->original,sizeof(known->bytes))){ok=false;break;}
            free=known;
        }
        void *target=table[slots[i]];
        if(!free || !original_site(table,target,context_table)){ok=false;break;}
        *free={table,slots[i],target,hooks[i],{},false,context_table};memcpy(free->bytes,target,sizeof(free->bytes));
        DWORD prior=0;
        if(VirtualProtect(table+slots[i],sizeof(void *),PAGE_READWRITE,&prior)) {
            if(!memcmp(free->bytes,target,sizeof(free->bytes)) &&
               InterlockedCompareExchangePointer(table+slots[i],hooks[i],target)==target) {
                free->published=true;added[used++]=free;
            } else ok=false;
            DWORD ignored=0;VirtualProtect(table+slots[i],sizeof(void *),prior,&ignored);
        } else ok=false;
    }
    if(!ok)for(unsigned i=used;i>0;--i)restore_site(*added[i-1]);
    ReleaseSRWLockExclusive(&hook_lock);return ok;
}
static bool has(const unsigned char *bytes,size_t length,const char *word) {
    size_t n=strlen(word);if(length<n)return false;
    for(size_t i=0;i<=length-n;++i)if(!memcmp(bytes+i,word,n))return true;
    return false;
}
static bool digest(const void *bytes,size_t length,unsigned char (&hash)[32]) {
    if(length>0xffffffffu)return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    NTSTATUS status=BCryptHash(algorithm,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<void *>(bytes)),
        static_cast<ULONG>(length),hash,sizeof(hash));
    BCryptCloseAlgorithmProvider(algorithm,0);return status>=0;
}
static unsigned classify(bool pixel,const void *code,size_t size,unsigned &low) {
    low=0;if(!code || !size)return 0;
    auto bytes=static_cast<const unsigned char *>(code);
    bool ocean=has(bytes,size,"sOceanHeightMask") || has(bytes,size,"fWaterLevel");
    bool river=has(bytes,size,"sRiverHeightMask"),uv=has(bytes,size,"vHeightMaskUV");
    if(!ocean && !river && !uv && size!=(pixel?13012u:9068u))return 0;
    unsigned char hash[32]={};bool hashed=digest(code,size,hash);
    // SHA-256 as printed in WATER.md; low32 is its final eight hex digits.
    static constexpr unsigned char ocean_vs[32]={0xe9,0x8c,0xd1,0xcb,0x82,0x47,0x53,0x84,0x6e,0x8b,0xdf,0x90,0xed,0x3c,0x63,0xc1,0x25,0xd9,0xab,0x7c,0xf6,0x27,0x17,0x08,0x49,0xdb,0xcd,0x99,0x99,0x3e,0xe9,0xfd};
    static constexpr unsigned char ocean_ps[32]={0xcd,0x3e,0x0d,0x02,0x25,0xf7,0x27,0x04,0xbe,0x18,0xa6,0xe1,0x35,0x85,0x00,0xa8,0x83,0xdc,0xe8,0x01,0xfa,0x79,0x3e,0xc9,0xb5,0x44,0x31,0x39,0x9b,0x6e,0xb8,0x89};
    if(hashed) {
        low=(unsigned(hash[28])<<24)|(unsigned(hash[29])<<16)|(unsigned(hash[30])<<8)|hash[31];
        if(size==(pixel?13012u:9068u) && !memcmp(hash,pixel?ocean_ps:ocean_vs,32))ocean=true;
    }
    return river?(pixel?4u:3u):ocean?(pixel?2u:1u):uv?5u:0u;
}
static void remember(bool pixel,const void *code,size_t size,IUnknown *object) {
    if(!object || !active())return;
    unsigned low=0,kind=classify(pixel,code,size,low);if(!kind)return;
    bool saved=false;AcquireSRWLockExclusive(&shader_lock);
    if(InterlockedCompareExchange(&observing,1,1) && shader_counts[kind-1]<MaxShaders) {
        bool known=false;for(unsigned i=0;i<shader_counts[kind-1];++i)if(shaders[kind-1][i].object==object)known=true;
        if(!known){object->AddRef();shaders[kind-1][shader_counts[kind-1]++]={object,kind};saved=true;}
    }
    ReleaseSRWLockExclusive(&shader_lock);
    if(saved)log_event("water_shader",kind,static_cast<unsigned>(size),low);
}
static unsigned shader_kind(ID3D11VertexShader *vs) {
    unsigned kind=0;AcquireSRWLockShared(&shader_lock);
    if(InterlockedCompareExchange(&observing,1,1))for(unsigned k:{0u,2u,4u})for(unsigned i=0;i<shader_counts[k];++i)
        if(shaders[k][i].object==vs)kind=k+1;
    ReleaseSRWLockShared(&shader_lock);return kind;
}
using CreateVS=HRESULT(STDMETHODCALLTYPE *)(ID3D11Device *,const void *,SIZE_T,ID3D11ClassLinkage *,ID3D11VertexShader **);
using CreatePS=HRESULT(STDMETHODCALLTYPE *)(ID3D11Device *,const void *,SIZE_T,ID3D11ClassLinkage *,ID3D11PixelShader **);
static HRESULT STDMETHODCALLTYPE create_vs(ID3D11Device *d,const void *b,SIZE_T n,ID3D11ClassLinkage *l,ID3D11VertexShader **out) {
    auto call=reinterpret_cast<CreateVS>(original(d,12));HRESULT hr=call(d,b,n,l,out);
    if(SUCCEEDED(hr) && out)remember(false,b,n,*out);return hr;
}
static HRESULT STDMETHODCALLTYPE create_ps(ID3D11Device *d,const void *b,SIZE_T n,ID3D11ClassLinkage *l,ID3D11PixelShader **out) {
    auto call=reinterpret_cast<CreatePS>(original(d,15));HRESULT hr=call(d,b,n,l,out);
    if(SUCCEEDED(hr) && out)remember(true,b,n,*out);return hr;
}
static Context *find_context(ID3D11DeviceContext *ctx) {
    Context *found=nullptr;AcquireSRWLockShared(&hook_lock);
    for(auto &c:contexts)if(c.object==ctx)found=&c;
    ReleaseSRWLockShared(&hook_lock);return found;
}
// local56b (user 2026-10-29): torrent foam is not in the edited maps. Dump the
// bound PS inputs t3 (BC4 1024) and t10 (BC1 4096) once, mip 0, to find where
// the foam is stored. Diagnostic setting only; one staging copy per slot.
static unsigned ps_dumped; // bit per PS slot; bit 31 = constant buffers
static bool dump_ps_input(ID3D11DeviceContext *ctx,unsigned slot) {
    ID3D11ShaderResourceView *view=nullptr;ctx->PSGetShaderResources(slot,1,&view);if(!view)return false;
    ID3D11Resource *resource=nullptr;view->GetResource(&resource);view->Release();if(!resource)return false;
    ID3D11Texture2D *texture=nullptr;resource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&texture));resource->Release();
    if(!texture)return false;
    D3D11_TEXTURE2D_DESC d={};texture->GetDesc(&d);
    D3D11_TEXTURE2D_DESC s=d;s.MipLevels=1;s.ArraySize=1;s.Usage=D3D11_USAGE_STAGING;s.BindFlags=0;s.CPUAccessFlags=D3D11_CPU_ACCESS_READ;s.MiscFlags=0;
    ID3D11Device *device=nullptr;ctx->GetDevice(&device);ID3D11Texture2D *staging=nullptr;
    if(device && SUCCEEDED(device->CreateTexture2D(&s,nullptr,&staging)) && staging) {
        ctx->CopySubresourceRegion(staging,0,0,0,0,texture,0,nullptr);
        D3D11_MAPPED_SUBRESOURCE m={};
        if(SUCCEEDED(ctx->Map(staging,0,D3D11_MAP_READ,0,&m))) {
            wchar_t path[MAX_PATH]={};DWORD n=GetModuleFileNameW(nullptr,path,MAX_PATH);
            wchar_t *cut=n && n<MAX_PATH?wcsrchr(path,L'\\'):nullptr;
            if(cut) {
                *cut=0;wchar_t name[96]={};swprintf_s(name,L"\\AI_TacticsMod\\data\\performance\\water_ps_t%u_%lu.bin",slot,GetCurrentProcessId());
                if(!wcscat_s(path,name)) {
                    HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
                    if(file!=INVALID_HANDLE_VALUE) {
                        // Header: width, height, format, row pitch; then 4x4-block rows (height/4 rows for BC).
                        unsigned header[4]={d.Width,d.Height,static_cast<unsigned>(d.Format),m.RowPitch};DWORD w=0;
                        WriteFile(file,header,sizeof(header),&w,nullptr);
                        unsigned rows=((d.Format>=70 && d.Format<=84) || (d.Format>=94 && d.Format<=99))?(d.Height+3)/4:d.Height;
                        WriteFile(file,m.pData,m.RowPitch*rows,&w,nullptr);CloseHandle(file);
                        log_event("water_ps_dump",slot,d.Format,d.Width);
                    }
                }
            }
            ctx->Unmap(staging,0);
        }
        staging->Release();
    }
    if(device)device->Release();
    texture->Release();return true;
}
// Raw constant buffers of the bound ocean draw (VS/PS slots 0..3), once.
static void dump_buffers(ID3D11DeviceContext *ctx) {
    wchar_t path[MAX_PATH]={};DWORD n=GetModuleFileNameW(nullptr,path,MAX_PATH);
    wchar_t *cut=n && n<MAX_PATH?wcsrchr(path,L'\\'):nullptr;if(!cut)return;*cut=0;
    wchar_t name[96]={};swprintf_s(name,L"\\AI_TacticsMod\\data\\performance\\water_cb_%lu.bin",GetCurrentProcessId());
    if(wcscat_s(path,name))return;
    HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    ID3D11Device *device=nullptr;ctx->GetDevice(&device);
    for(unsigned stage=0;stage<2;++stage)for(unsigned slot=0;slot<4;++slot) {
        ID3D11Buffer *buffer=nullptr;
        if(stage)ctx->PSGetConstantBuffers(slot,1,&buffer);else ctx->VSGetConstantBuffers(slot,1,&buffer);
        unsigned header[3]={stage,slot,0};DWORD w=0;
        if(buffer && device) {
            D3D11_BUFFER_DESC d={};buffer->GetDesc(&d);
            D3D11_BUFFER_DESC s={};s.ByteWidth=d.ByteWidth;s.Usage=D3D11_USAGE_STAGING;s.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            ID3D11Buffer *staging=nullptr;
            if(SUCCEEDED(device->CreateBuffer(&s,nullptr,&staging)) && staging) {
                ctx->CopyResource(staging,buffer);D3D11_MAPPED_SUBRESOURCE m={};
                if(SUCCEEDED(ctx->Map(staging,0,D3D11_MAP_READ,0,&m))) {
                    header[2]=d.ByteWidth;WriteFile(file,header,sizeof(header),&w,nullptr);WriteFile(file,m.pData,d.ByteWidth,&w,nullptr);
                    ctx->Unmap(staging,0);
                }
                staging->Release();
            }
        }
        if(!header[2])WriteFile(file,header,sizeof(header),&w,nullptr);
        if(buffer)buffer->Release();
    }
    if(device)device->Release();
    CloseHandle(file);log_event("water_cb_dump",1,0,0);
}
// Called only on the immediate context's existing render caller thread.
static void sample(Context &c,unsigned kind) {
    if(kind==1) {
        // t10 (4096) is bound only on some ocean draws: retry within the sample budget.
        for(unsigned slot:{8u,9u,10u})if(!(ps_dumped&(1u<<slot)) && dump_ps_input(c.object,slot))ps_dumped|=1u<<slot;
        if(!(ps_dumped&0x80000000u)){ps_dumped|=0x80000000u;dump_buffers(c.object);}
    }
    auto ctx=c.object;
    struct SampleTexture {ID3D11Texture2D *object=nullptr;~SampleTexture(){if(object)object->Release();}} mask;
    ID3D11ShaderResourceView *view=nullptr;ctx->VSGetShaderResources(0,1,&view);
    UINT width=0,height=0,format=0;
    if(view) {
        ID3D11Resource *resource=nullptr;view->GetResource(&resource);
        if(resource) {
            ID3D11Texture2D *texture=nullptr;
            if(SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&texture)))) {
                D3D11_TEXTURE2D_DESC desc={};texture->GetDesc(&desc);width=desc.Width;height=desc.Height;format=desc.Format;texture->Release();
                if(kind==1)resource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&mask.object));
            }
            resource->Release();
        }
        view->Release();
    }
    log_event("water_bind_sample",kind,0,0);log_event("water_srv",width,height,format);
    // local56: list every bound water input (searching StatisticalOceanWaveAndBubbleMaskMap).
    for(unsigned stage=0;stage<2;++stage) {
        ID3D11ShaderResourceView *views[16]={};
        if(stage)ctx->PSGetShaderResources(0,16,views);else ctx->VSGetShaderResources(0,16,views);
        for(unsigned slot=0;slot<16;++slot)if(views[slot]) {
            D3D11_SHADER_RESOURCE_VIEW_DESC vd={};views[slot]->GetDesc(&vd);
            unsigned w=0,h=0,extra=static_cast<unsigned>(vd.ViewDimension)*1000;
            ID3D11Resource *resource=nullptr;views[slot]->GetResource(&resource);
            ID3D11Texture2D *texture=nullptr;
            if(resource && SUCCEEDED(resource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&texture)))) {
                D3D11_TEXTURE2D_DESC d={};texture->GetDesc(&d);w=d.Width;h=d.Height;extra+=d.Usage*100+d.MipLevels;texture->Release();
            }
            if(resource)resource->Release();
            views[slot]->Release();
            log_event(stage?"water_ps_srv":"water_vs_srv",slot*1000+static_cast<unsigned>(vd.Format),w|(h<<16),extra);
        }
    }
    if(c.readback_failed)return;
    ID3D11Buffer *source=nullptr;ctx->VSGetConstantBuffers(0,1,&source);if(!source)return;
    D3D11_BUFFER_DESC native={};source->GetDesc(&native);
    if(!c.staging || c.width!=native.ByteWidth) {
        if(c.staging){c.staging->Release();c.staging=nullptr;}
        D3D11_BUFFER_DESC desc={};desc.ByteWidth=native.ByteWidth;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ID3D11Device *device=nullptr;ctx->GetDevice(&device);
        HRESULT hr=device?device->CreateBuffer(&desc,nullptr,&c.staging):E_FAIL;
        if(device)device->Release();
        if(FAILED(hr)){source->Release();c.readback_failed=true;log_event("water_read_error",kind,static_cast<unsigned>(hr),1);return;}
        c.width=native.ByteWidth;
    }
    ctx->CopyResource(c.staging,source);source->Release();
    D3D11_MAPPED_SUBRESOURCE mapped={};HRESULT hr=ctx->Map(c.staging,0,D3D11_MAP_READ,0,&mapped);
    if(FAILED(hr)){c.readback_failed=true;log_event("water_read_error",kind,static_cast<unsigned>(hr),2);return;}
    unsigned words[64]={};memcpy(words,mapped.pData,(std::min)(size_t(c.width),sizeof(words)));ctx->Unmap(c.staging,0);
    // Log raw float bits; short buffers and unknown layouts are never decoded.
    if(kind==1 && c.width>=0xe4) {
        log_event("water_cb",kind,words[0xe0/4],words[0xd4/4]);
        log_event("water_cb_uv",kind,words[0xd0/4],words[0xd8/4]);
        log_event("water_cb_tail",kind,words[0xdc/4],0);
        float uv[4];memcpy(uv,words+0xd0/4,sizeof(uv));
        if(mask.object && mask_sample)mask_sample(mask.object,uv);
    } else if(kind==3 && c.width>=0x90) {
        log_event("water_cb",kind,words[0x70/4],words[0x74/4]);
        log_event("water_cb_uv",kind,words[0x78/4],words[0x7c/4]);
        log_event("water_cb_tail",kind,words[0x80/4],words[0x84/4]);
    } else log_event("water_cb_unknown",kind,c.width,0);
}
// User/Claude 2026-10-29: context fast paths rewrite draw slots. Hook only
// VSSetShader, and abandon a replaced context slot permanently without writing it.
static bool context_ready(Context &c) {
    bool ready=false,report=false;
    AcquireSRWLockExclusive(&hook_lock);
    if(!c.disabled && !stopping) {
        auto table=*reinterpret_cast<void ***>(c.object);
        Site *site=nullptr;
        for(auto &s:sites)if(s.context_table && s.table==table && s.slot==11)site=&s;
        ready=site && site->published && site_matches(*site);
        if(!ready) {
            c.disabled=true;report=true;
            // Do not restore or reinstall the replacement, including runtime
            // System32 dispatch changes. Keep the saved target for entered calls.
            if(site && site->table[11]!=site->replacement)site->published=false;
        }
    }
    ReleaseSRWLockExclusive(&hook_lock);
    if(report)log_event("water_hook_replaced",11,0,0);
    return ready;
}
static void report_binds(Context &c,ULONGLONG now) {
    if(now-c.budget.last_report<5000)return;
    log_event("water_binds",c.budget.binds,c.budget.bits,0);
    c.budget.binds=c.budget.bits=0;c.budget.last_report=now;
}
static void observe_bind(Context &c,ID3D11VertexShader *next) {
    // ClearState/ExecuteCommandList can change the outgoing shader without
    // VSSetShader. Confirm it before reading that region's cb0 and t0.
    ID3D11VertexShader *vs=nullptr;c.object->VSGetShader(&vs,nullptr,nullptr);c.current=vs;
    unsigned previous_kind=shader_kind(vs);if(vs)vs->Release();
    ULONGLONG now=GetTickCount64();
    if(previous_kind && c.budget.sample(now))sample(c,previous_kind);
    unsigned next_kind=shader_kind(next);
    if(next_kind){++c.budget.binds;c.budget.bits|=1u<<(next_kind-1);}
    report_binds(c,now);
}
using SetVS=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,ID3D11VertexShader *,ID3D11ClassInstance *const *,UINT);
static void STDMETHODCALLTYPE set_vs(ID3D11DeviceContext *c,ID3D11VertexShader *v,ID3D11ClassInstance *const *i,UINT n) {
    auto call=reinterpret_cast<SetVS>(original(c,11));
    Context *state=find_context(c);
    if(active() && state && context_ready(*state))observe_bind(*state,v);
    // Sample the outgoing water region before native code replaces its binding.
    call(c,v,i,n);
    if(state){state->current=v;if(active())context_ready(*state);}
}
static void restore();
static void on_device(ID3D11Device *device,bool early=false) {
    if(!device || !active() || !wanted())return;
    const unsigned device_slots[]={12,15};void *device_hooks[]={reinterpret_cast<void *>(&create_vs),reinterpret_cast<void *>(&create_ps)};
    bool created=install_slots(device,device_slots,device_hooks,2);
    if(!created) {
        if(InterlockedIncrement(&fail_reports)<=8)log_event("water_hook_unavailable",early?2:1,0,0);
        if(!early){log_event("feature_guard_mismatch",30,0,0);restore();}
        return;
    }
    // Context tables are instance-private; never retain a probe's heap table.
    if(early)return;
    ID3D11DeviceContext *ctx=nullptr;device->GetImmediateContext(&ctx);if(!ctx)return;
    Context *state=find_context(ctx);bool fresh=false;
    if(!state) {
        AcquireSRWLockExclusive(&hook_lock);
        for(auto &c:contexts)if(c.object==ctx)state=&c;
        if(!state)for(auto &c:contexts)if(!c.object) {
            c.object=ctx;c.budget.last_report=GetTickCount64();state=&c;fresh=true;
            // Own the reference even after a context observer is disabled.
            ctx->AddRef();break;
        }
        ReleaseSRWLockExclusive(&hook_lock);
    }
    if(state && fresh) {
        const unsigned context_slots[]={11};void *context_hooks[]={reinterpret_cast<void *>(&set_vs)};
        if(!install_slots(ctx,context_slots,context_hooks,1,true)) {
            state->disabled=true;log_event("water_hook_unavailable",11,0,0);
        }
    }
    if(state && context_ready(*state))report_binds(*state,GetTickCount64());
    ctx->Release();
}
static void install_early() {
    if(!active() || !wanted() || early_attempted)return;early_attempted=true;
    wchar_t path[MAX_PATH]={};UINT n=GetSystemDirectoryW(path,MAX_PATH);constexpr wchar_t suffix[]=L"\\d3d11.dll";
    if(!n || n+sizeof(suffix)/sizeof(*suffix)>MAX_PATH || wcscat_s(path,MAX_PATH,suffix))return;
    library=LoadLibraryW(path);if(!library)return;
    auto create=reinterpret_cast<decltype(&D3D11CreateDevice)>(GetProcAddress(library,"D3D11CreateDevice"));
    ID3D11Device *device=nullptr;
    HRESULT hr=create?create(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr):E_FAIL;
    if(SUCCEEDED(hr) && device)on_device(device,true);
    if(device)device->Release();
    log_event("water_hook",SUCCEEDED(hr)?1:0,0,0);
}
static void restore() {
    InterlockedExchange(&observing,0);
    for(auto &c:contexts)if(c.object)context_ready(c);
    AcquireSRWLockExclusive(&hook_lock);stopping=true;
    for(auto &s:sites)restore_site(s);
    // Keep context/staging storage through any in-flight render wrapper. Their
    // bounded references are released only at DLL/process teardown below.
    ReleaseSRWLockExclusive(&hook_lock);
    AcquireSRWLockExclusive(&shader_lock);permitted=false;ReleaseSRWLockExclusive(&shader_lock);
}
struct Lifetime {
    ~Lifetime() {
        restore();
        for(auto &c:contexts){if(c.staging)c.staging->Release();if(c.object)c.object->Release();}
        for(unsigned k=0;k<5;++k)for(unsigned i=0;i<shader_counts[k];++i)shaders[k][i].object->Release();
    }
};
static Lifetime lifetime;
}
static bool water_diag_guards_match() {return water_diag::guards_match();}
#endif
