// User/Claude 2026-10-29: VSSetShader region sampling replaces draw hooks.
// Real WARP readback, forwarding, private-table replacement and device guards.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstring>
#include <vector>
static bool enabled=true;
static struct {bool convenience=true,terrain_edit=true;} mod_features;
static unsigned checks;
static void require(bool ok,const char *label){++checks;if(!ok){fflush(stdout);fprintf(stderr,"FAIL %s (%u)\n",label,checks);ExitProcess(1);}}
#ifndef NO_RUNTIME_LOG
struct Event {const char *name;unsigned a,b,c;};
static std::vector<Event> events;
static void log_event(const char *name,unsigned a,unsigned b,unsigned c){events.push_back({name,a,b,c});}
static unsigned event_count(const char *name){unsigned n=0;for(const auto &e:events)if(!strcmp(e.name,name))++n;return n;}
static unsigned forced_hash;
static NTSTATUS WINAPI test_hash(BCRYPT_ALG_HANDLE h,PUCHAR secret,ULONG sn,PUCHAR input,ULONG size,PUCHAR output,ULONG length) {
    if(forced_hash){
        const char *hex=forced_hash==1?"e98cd1cb824753846e8bdf90ed3c63c125d9ab7cf627170849dbcd99993ee9fd":
            "cd3e0d0225f72704be18a6e1358500a883dce801fa793ec9b54431399b6eb889";
        for(unsigned i=0;i<32;++i){unsigned v=0;sscanf_s(hex+i*2,"%2x",&v);output[i]=static_cast<unsigned char>(v);}return 0;
    }
    return BCryptHash(h,secret,sn,input,size,output,length);
}
#define BCryptHash test_hash
#endif
#define WATER_DIAG_WANTED() true
#include "../src/native_water_diag.inl"
#ifndef NO_RUNTIME_LOG
#undef BCryptHash
static ULONG references(IUnknown *p){p->AddRef();return p->Release();}
static ID3DBlob *compile(const char *text,const char *target) {
    ID3DBlob *code=nullptr,*errors=nullptr;
    HRESULT hr=D3DCompile(text,strlen(text),nullptr,nullptr,nullptr,"main",target,0,0,&code,&errors);
    if(errors){if(FAILED(hr))fprintf(stderr,"%s",static_cast<const char *>(errors->GetBufferPointer()));errors->Release();}
    require(SUCCEEDED(hr),"compile independent real DXBC");return code;
}
// Synthetic device exercises exact pointers, HRESULT, null output and failure.
static void *fake_table[16];
static struct {void **table=fake_table;} fake;
static const void *expected_code;static SIZE_T expected_size;
static ID3D11ClassLinkage *expected_link;
static ID3D11VertexShader **expected_out;
static ID3D11VertexShader *fake_shader;
static HRESULT native_hr=S_FALSE;static bool native_empty;
static unsigned create_calls,forward_calls;
static ID3D11Texture2D *sampled_mask;
static float sampled_uv[4];
static unsigned mask_samples;
static void observe_mask(ID3D11Texture2D *texture,const float *uv) {
    sampled_mask=texture;memcpy(sampled_uv,uv,sizeof(sampled_uv));++mask_samples;
}
static HRESULT STDMETHODCALLTYPE native_vs(ID3D11Device *d,const void *b,SIZE_T n,ID3D11ClassLinkage *l,ID3D11VertexShader **out) {
    ++create_calls;require(d==reinterpret_cast<ID3D11Device *>(&fake) && b==expected_code && n==expected_size && l==expected_link && out==expected_out,"shader forwards every argument unchanged");
    if(out)*out=native_empty?nullptr:fake_shader;return native_hr;
}
static ID3D11VertexShader *expected_vs;
static ID3D11ClassInstance *const *expected_classes;
static void STDMETHODCALLTYPE native_set(ID3D11DeviceContext *c,ID3D11VertexShader *v,ID3D11ClassInstance *const *i,UINT n) {
    require(c==reinterpret_cast<ID3D11DeviceContext *>(&fake) && v==expected_vs && i==expected_classes && n==3,"VSSetShader exact arguments");++forward_calls;
}
#endif
int main() {
#ifndef NO_RUNTIME_LOG
    using namespace water_diag;
    unsigned low=0;
    for(const char *name:{"fWaterLevel","sOceanHeightMask","sRiverHeightMask","vHeightMaskUV"}) {
        unsigned kind=classify(false,name,strlen(name),low);
        require(kind==(!strcmp(name,"sRiverHeightMask")?3u:!strcmp(name,"vHeightMaskUV")?5u:1u),"water marker detection");
        require(classify(true,name,strlen(name),low)==(kind==1?2u:kind==3?4u:5u),"pixel shader classification");
    }
    require(!classify(false,"ordinary",8,low) && !classify(false,nullptr,0,low),"ordinary and empty shaders untouched");
    require(!classify(false,"fWaterLevel",10,low),"bounded substring cannot overread");
    unsigned char hash[32]={};require(digest("abc",3,hash) && hash[0]==0xba && hash[31]==0xad,"real SHA-256 provider");
    std::vector<unsigned char> bytes(13012,0);
    forced_hash=1;require(classify(false,bytes.data(),9068,low)==1 && low==0x993ee9fd,"exact ocean VS fingerprint without strings");
    require(!classify(false,bytes.data(),9067,low),"wrong VS length rejected");
    forced_hash=2;require(classify(true,bytes.data(),13012,low)==2 && low==0x9b6eb889,"exact ocean PS fingerprint without strings");
    forced_hash=0;require(!classify(true,bytes.data(),13012,low),"same size different SHA rejected");
    Budget budget;
    for(unsigned i=0;i<8;++i)require(budget.sample(100),"initial eight samples");
    for(unsigned i=0;i<100;++i)require(!budget.sample(5099),"sample throttled before five seconds");
    require(budget.sample(5100) && !budget.sample(5100) && !budget.sample(10099) && budget.sample(10100),"one sample per subsequent five seconds");
    ID3D11Device *device=nullptr;ID3D11DeviceContext *ctx=nullptr;
    require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&ctx)),"real WARP device");
    on_device(device);require(guards_match() && permitted,"real System32 device guards");
    auto device_table=*reinterpret_cast<void ***>(device),context_table=*reinterpret_cast<void ***>(ctx);
    HMODULE context_module=nullptr;MEMORY_BASIC_INFORMATION context_region={};
    require(!GetModuleHandleExW(6,reinterpret_cast<LPCWSTR>(context_table),&context_module) &&
        VirtualQuery(context_table,&context_region,sizeof(context_region)) && context_region.Type==MEM_PRIVATE && context_region.State==MEM_COMMIT,
        "real WARP context has private committed table outside image modules");
    for(const auto &site:sites)if(site.published && site.context_table) {
        require(site.slot==11,"sole context hook is VSSetShader");
        require(original_site(context_table,site.original,true),"private immediate table with System32 function accepted");
        require(!original_site(context_table,site.original,false),"device validation rejects identical heap table");
        require(!original_site(device_table,site.original,true),"context heap exception rejects image tables");
    }
    auto state=find_context(ctx);require(state!=nullptr && context_ready(*state),"immediate context registered and enabled");
    ID3D11DeviceContext *deferred=nullptr;require(SUCCEEDED(device->CreateDeferredContext(0,&deferred)),"real deferred context");
    require(!find_context(deferred),"deferred context excluded");deferred->Release();
    auto code=compile("cbuffer Water:register(b0){float4 pad[13];float4 vHeightMaskUV;float fWaterLevel;}; Texture2D<float> sOceanHeightMask:register(t0); float4 main(uint id:SV_VertexID):SV_POSITION{return float4(id+vHeightMaskUV.x,sOceanHeightMask.Load(int3(0,0,0))+fWaterLevel,0,1);}","vs_5_0");
    ID3D11VertexShader *vs=nullptr;require(SUCCEEDED(device->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&vs)),"VS through production shader hook");
    require(shader_kind(vs)==1 && event_count("water_shader")==1,"ocean marker registered once");
    remember(false,code->GetBufferPointer(),code->GetBufferSize(),vs);require(event_count("water_shader")==1,"duplicate shader ignored");code->Release();
    auto pixel=compile("cbuffer Water:register(b0){float fWaterLevel;};float4 main():SV_TARGET{return fWaterLevel;}","ps_5_0");
    ID3D11PixelShader *ps=nullptr;require(SUCCEEDED(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps)),"PS through production shader hook");
    require(shader_counts[1]==1 && event_count("water_shader")==2,"real ocean PS registered");pixel->Release();
    unsigned data[64]={};data[0xd0/4]=0x3f800000;data[0xd4/4]=0x40000000;data[0xd8/4]=0x43960000;data[0xe0/4]=0x42c80000;
    D3D11_BUFFER_DESC bd={};bd.ByteWidth=sizeof(data);bd.Usage=D3D11_USAGE_DEFAULT;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA initial={data,0,0};ID3D11Buffer *buffer=nullptr;
    require(SUCCEEDED(device->CreateBuffer(&bd,&initial,&buffer)),"native constant buffer");ctx->VSSetConstantBuffers(0,1,&buffer);
    D3D11_TEXTURE2D_DESC td={};td.Width=16;td.Height=8;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R32_FLOAT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D *texture=nullptr;ID3D11ShaderResourceView *view=nullptr;
    require(SUCCEEDED(device->CreateTexture2D(&td,nullptr,&texture)) && SUCCEEDED(device->CreateShaderResourceView(texture,nullptr,&view)),"native VS t0 texture and view");
    // User/Claude 2026-10-29 local54: callback gets actual t0 identity and float4,
    // including zw, while preserving all native references and bindings.
    mask_sample=&observe_mask;
    ctx->VSSetShaderResources(0,1,&view);ctx->VSSetShader(vs,nullptr,0);
    require(event_count("water_cb")==0 && state->budget.binds==1,"first bind counts but samples no preceding water region");
    ULONG br=references(buffer),tr=references(texture),vr=references(view),dr=references(device),sr=references(vs);
    for(unsigned i=0;i<40;++i)ctx->VSSetShader(vs,nullptr,0);
    require(event_count("water_cb")==8 && event_count("water_srv")==8 && event_count("water_bind_sample")==8,"exactly eight samples via real VSSetShader hook");
    require(mask_samples==8 && sampled_mask==texture && !memcmp(sampled_uv,data+0xd0/4,sizeof(sampled_uv)),"mask callback forwards actual resource and all four cb0 UV values");
    require(sampled_uv[2]==300.0f && sampled_uv[3]==0.0f,"cb0+D8 is float4 zw.x world origin, not the Z scale");
    require(state->budget.binds==41 && state->budget.bits==1,"incoming water binding count and bit mask");
    require(references(buffer)==br && references(texture)==tr && references(view)==vr && references(vs)==sr,"all getter resource references released");
    ULONG after_staging=references(device);require(after_staging>=dr,"one staging buffer owns device");
    auto staging=state->staging;state->budget.last_sample=GetTickCount64()-5000;ctx->VSSetShader(vs,nullptr,0);
    require(state->staging==staging && references(device)==after_staging,"staging reused and GetDevice released");
    bool cb=false,srv=false;
    for(const auto &e:events){if(!strcmp(e.name,"water_cb") && e.a==1 && e.b==data[56] && e.c==data[53])cb=true;if(!strcmp(e.name,"water_srv") && e.a==16 && e.b==8 && e.c==DXGI_FORMAT_R32_FLOAT)srv=true;}
    require(cb && srv,"raw ocean float bits and t0 texture desc");
    ID3D11Buffer *bound_buffer=nullptr;ID3D11ShaderResourceView *bound_view=nullptr;ID3D11VertexShader *bound_vs=nullptr;
    ctx->VSGetConstantBuffers(0,1,&bound_buffer);ctx->VSGetShaderResources(0,1,&bound_view);ctx->VSGetShader(&bound_vs,nullptr,nullptr);
    require(bound_buffer==buffer && bound_view==view && bound_vs==vs,"sampling preserves all native bindings");
    bound_buffer->Release();bound_view->Release();bound_vs->Release();
    ctx->ClearState();unsigned sampled=event_count("water_cb");ctx->VSSetShader(nullptr,nullptr,0);
    require(event_count("water_cb")==sampled && state->budget.binds==42,"ClearState prevents stale water region sampling");
    state->budget.last_report=GetTickCount64()-5000;ctx->VSSetShader(vs,nullptr,0);
    require(event_count("water_binds")==1 && state->budget.binds==0,"five-second binding summary resets window");
    auto river=compile("cbuffer Water:register(b0){float4 pad[7];float4 vHeightMaskUV;float4 bubble;};Texture2D<float> sRiverHeightMask:register(t0);float4 main(uint id:SV_VertexID):SV_POSITION{return float4(id+vHeightMaskUV.x,sRiverHeightMask.Load(int3(0,0,0))+bubble.x,0,1);}","vs_5_0");
    ID3D11VertexShader *rv=nullptr;require(SUCCEEDED(device->CreateVertexShader(river->GetBufferPointer(),river->GetBufferSize(),nullptr,&rv)),"real river VS");river->Release();
    require(shader_kind(rv)==3,"real river classification");
    data[28]=0x3f000000;data[29]=0x3e800000;data[32]=0x40800000;ctx->UpdateSubresource(buffer,0,nullptr,data,0,0);
    ctx->VSSetConstantBuffers(0,1,&buffer);ctx->VSSetShader(rv,nullptr,0);state->budget.last_sample=GetTickCount64()-5000;
    ctx->VSSetShader(nullptr,nullptr,0);bool river_cb=false;
    for(const auto &e:events)if(!strcmp(e.name,"water_cb") && e.a==3 && e.b==data[28] && e.c==data[29])river_cb=true;
    ctx->VSGetShader(&bound_vs,nullptr,nullptr);
    require(river_cb && !bound_vs,"river region sampled before original VSSetShader unbinds it");
    require(state->budget.binds==1 && state->budget.bits==4,"river binding counted independently of outgoing sample");
    // Marker-only registration quota: distinct shaders, bounded owned references.
    for(unsigned i=0;i<40;++i) {
        char source[160];sprintf_s(source,"float4 main(uint id:SV_VertexID):SV_POSITION{return float4(id,%u,0,1);}",i+1);
        ID3D11VertexShader *other=nullptr;auto p=compile(source,"vs_5_0");
        require(SUCCEEDED(device->CreateVertexShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&other)),"ordinary VS created");p->Release();
        require(!shader_kind(other),"ordinary VS never registered");
        ctx->VSSetShader(other,nullptr,0);ctx->VSSetShader(nullptr,nullptr,0);
        remember(false,"sOceanHeightMask",16,other);other->Release();
    }
    require(shader_counts[0]==32,"kind quota capped at thirty-two");
    require(event_count("water_cb")==sampled+1 && state->budget.binds==1,"ordinary bindings do not sample or count water");
    // Forwarding wrappers can be tested without a render context or side effects.
    Site saved=sites[31];sites[31]={fake_table,12,reinterpret_cast<void *>(&native_vs),nullptr,{},false};
    expected_code="ordinary";expected_size=8;expected_link=reinterpret_cast<ID3D11ClassLinkage *>(size_t(0x9876));
    ID3D11VertexShader *output=nullptr;expected_out=&output;fake_shader=vs;
    require(create_vs(reinterpret_cast<ID3D11Device *>(&fake),expected_code,8,expected_link,&output)==S_FALSE && output==vs,"native HRESULT and output preserved");
    native_hr=E_INVALIDARG;require(create_vs(reinterpret_cast<ID3D11Device *>(&fake),expected_code,8,expected_link,&output)==E_INVALIDARG,"native shader failure preserved");
    native_hr=S_OK;native_empty=true;require(create_vs(reinterpret_cast<ID3D11Device *>(&fake),expected_code,8,expected_link,&output)==S_OK && !output,"null returned shader preserved");
    expected_out=nullptr;require(create_vs(reinterpret_cast<ID3D11Device *>(&fake),expected_code,8,expected_link,nullptr)==S_OK && create_calls==4,"null output pointer preserved");
    auto fc=reinterpret_cast<ID3D11DeviceContext *>(&fake);enabled=false;
    sites[31].slot=11;sites[31].original=reinterpret_cast<void *>(&native_set);expected_vs=vs;ID3D11ClassInstance *classes[3]={};expected_classes=classes;set_vs(fc,vs,classes,3);
    require(forward_calls==1,"native VSSetShader forwarded exactly once");enabled=true;sites[31]=saved;
    // Foreign executable/table and fingerprint mismatch leave other features on.
    unsigned fake_slot=12;void *fake_hook=reinterpret_cast<void *>(&create_vs);fake_table[12]=reinterpret_cast<void *>(&native_vs);
    require(!install_slots(&fake,&fake_slot,&fake_hook,1) && fake_table[12]==reinterpret_cast<void *>(&native_vs),"non-System32 table rejected without mutation");
    require(mod_features.convenience && mod_features.terrain_edit,"guard rejection isolates water observation");
    Site *owned=nullptr,*context_site=nullptr;
    for(auto &site:sites)if(site.published){if(site.context_table)context_site=&site;else if(!owned)owned=&site;}
    require(owned && context_site,"device and context sites available");
    owned->bytes[0]^=1;require(!guards_match(),"device original fingerprint mismatch rejected");owned->bytes[0]^=1;
    require(guards_match(),"restored device fingerprint accepted");
    require(event_count("water_hook_replaced")==0 && !state->disabled,"WARP binding and ClearState did not lose VSSetShader hook");
    // Emulate a native fast-path replacement with this context's saved System32
    // VSSetShader implementation. Production must abandon it without a write.
    DWORD context_protection=0;
    require(VirtualProtect(context_site->table+11,sizeof(void *),PAGE_READWRITE,&context_protection)!=0,"fixture runtime replacement");
    context_site->table[11]=context_site->original;
    DWORD ignored=0;VirtualProtect(context_site->table+11,sizeof(void *),context_protection,&ignored);
    require(original_site(context_site->table,context_site->table[11],true),"replacement remains native System32 executable code");
    on_device(device);
    require(state->disabled && !context_site->published && event_count("water_hook_replaced")==1,"runtime replacement disables only this context once");
    on_device(device);require(context_site->table[11]==context_site->original && event_count("water_hook_replaced")==1,"disabled context neither restored nor reinstalled");
    require(guards_match() && permitted && active() && mod_features.terrain_edit && mod_features.convenience,"context replacement never triggers device mismatch or disables other features");
    unsigned end_samples=event_count("water_cb");ctx->VSSetShader(vs,nullptr,0);ctx->VSSetShader(nullptr,nullptr,0);
    require(event_count("water_cb")==end_samples,"disabled context samples no further bindings");
    require(event_count("feature_guard_mismatch")==0,"context replacement emits no guard mismatch");
    ctx->ClearState();buffer->Release();view->Release();texture->Release();rv->Release();vs->Release();ps->Release();
    DWORD protection=0;require(VirtualProtect(owned->table+owned->slot,sizeof(void *),PAGE_READWRITE,&protection)!=0,"fixture foreign device slot publication");
    owned->table[owned->slot]=reinterpret_cast<void *>(&native_vs);
    require(!guards_match(),"foreign device slot rejected");
    restore();for(const auto &site:sites)require(!site.published,"every owned COM slot restored or abandoned");
    require(owned->table[owned->slot]==reinterpret_cast<void *>(&native_vs),"rollback preserves foreign device owner");
    require(context_site->table[11]==context_site->original && event_count("water_hook_replaced")==1,"rollback never writes replaced context slot");
    owned->table[owned->slot]=owned->original;VirtualProtect(owned->table+owned->slot,sizeof(void *),protection,&ignored);
    require(mod_features.convenience && mod_features.terrain_edit && !active(),"rollback disables diagnostic only");
    ctx->Release();device->Release();
#else
    require(true,"NO_RUNTIME_LOG includes no diagnostic definitions");
#endif
    printf("{\"water_diag_checks\":%u,\"failed\":0}\n",checks);return 0;
}
