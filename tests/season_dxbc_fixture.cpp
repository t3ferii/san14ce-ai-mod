// 4계절 D fixture (no game process): the in-memory ground-shader patch (src/season_dxbc.h).
// Usage: season_dxbc_fixture <shader.dxbc>...   (local dumps from season_probe bit 2; never in git)
// Checks per file: our checksum reproduces the container's, the patch applies, the patched blob disassembles with the
// five added instructions, and a hardware device accepts it (CreatePixelShader). Also a render check on a shader of our
// own with the same `movc o0.xyz` + `sample t10, s10` shape: o0 = o0 * (1 - D) + A.
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <string>
#include "../src/season_dxbc.h"
#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")
static int failures=0;
static void check(bool ok,const char *what,const char *file){if(!ok){++failures;printf("FAIL %s (%s)\n",what,file);}}
static std::vector<unsigned char> read_file(const wchar_t *path) {
    std::vector<unsigned char> b;FILE *f=nullptr;if(_wfopen_s(&f,path,L"rb") || !f)return b;
    fseek(f,0,SEEK_END);b.resize(static_cast<size_t>(ftell(f)));fseek(f,0,SEEK_SET);fread(b.data(),1,b.size(),f);fclose(f);return b;
}
static std::string disasm(const std::vector<unsigned char> &code) {
    ID3DBlob *text=nullptr;if(FAILED(D3DDisassemble(code.data(),code.size(),0,nullptr,&text)))return {};
    std::string s(static_cast<const char *>(text->GetBufferPointer()),text->GetBufferSize());text->Release();return s;
}
static void print_around(const std::string &s,const char *needle) {
    size_t at=s.find(needle);if(at==std::string::npos)return;
    size_t a=s.rfind('\n',at>400?at-400:0);size_t b=at;for(int k=0;k<6 && b!=std::string::npos;++k)b=s.find('\n',b+1);
    printf("%s\n",s.substr(a==std::string::npos?0:a,(b==std::string::npos?s.size():b)-(a==std::string::npos?0:a)).c_str());
}
int wmain(int argc,wchar_t **argv) {
    ID3D11Device *dev=nullptr;ID3D11DeviceContext *ctx=nullptr;D3D_FEATURE_LEVEL fl=D3D_FEATURE_LEVEL_11_0;
    if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,&fl,1,D3D11_SDK_VERSION,&dev,nullptr,&ctx))){printf("FAIL device\n");return 2;}
    for(int a=1;a<argc;++a) {
        char name[MAX_PATH];WideCharToMultiByte(CP_UTF8,0,argv[a],-1,name,MAX_PATH,nullptr,nullptr);
        auto code=read_file(argv[a]);check(code.size()>32,"read",name);if(code.size()<=32)continue;
        uint32_t sum[4];season_dxbc::checksum(code.data(),code.size(),sum);
        check(!memcmp(sum,code.data()+4,16),"checksum_matches_original",name);
        ID3D11PixelShader *ps=nullptr;check(SUCCEEDED(dev->CreatePixelShader(code.data(),code.size(),nullptr,&ps)),"original_created",name);if(ps)ps->Release();
        std::vector<unsigned char> patched;bool ok=season_dxbc::patch_ground(code.data(),code.size(),patched);
        check(ok,"patch_applies",name);if(!ok)continue;
        ps=nullptr;HRESULT hr=dev->CreatePixelShader(patched.data(),patched.size(),nullptr,&ps);
        check(SUCCEEDED(hr),"patched_created",name);if(ps)ps->Release();
        std::string text=disasm(patched);check(!text.empty(),"patched_disassembles",name);
        check(text.find("t100")!=std::string::npos && text.find("t101")!=std::string::npos,"declares_t100_t101",name);
        printf("== %s: %zu -> %zu bytes, hr %08lx\n",name,code.size(),patched.size(),static_cast<unsigned long>(hr));
        print_around(text,"t101.xyzw");
    }
    // Render check with our own shader of the same shape.
    const char *src="Texture2D<float4> mapm:register(t10);SamplerState sm:register(s10);cbuffer C:register(b0){float4 k;};"
        "float4 main(float4 p:SV_Position,float2 uv:TEXCOORD0):SV_Target{float3 m=mapm.Sample(sm,uv).rgb;float3 base=float3(0.5,0.4,0.2)*m;"
        "float4 o=float4(k.x>0.5?float3(1,0,1):base,1);return o;}";
    ID3DBlob *blob=nullptr,*err=nullptr;
    if(FAILED(D3DCompile(src,strlen(src),"own",nullptr,nullptr,"main","ps_5_0",0,0,&blob,&err))){printf("FAIL own_compile %s\n",err?static_cast<char *>(err->GetBufferPointer()):"");return 2;}
    std::vector<unsigned char> own(static_cast<unsigned char *>(blob->GetBufferPointer()),static_cast<unsigned char *>(blob->GetBufferPointer())+blob->GetBufferSize());
    std::vector<unsigned char> own_patched;bool own_ok=season_dxbc::patch_ground(own.data(),own.size(),own_patched);
    printf("own shader patch %s\n",own_ok?"applied":"not applied (shape differs)");
    if(own_ok) {
        const char *vs_src="struct O{float4 p:SV_Position;float2 uv:TEXCOORD0;};O main(uint id:SV_VertexID){O o;float2 t=float2((id<<1)&2,id&2);o.p=float4(t*float2(2,-2)+float2(-1,1),0,1);o.uv=t;return o;}";
        ID3DBlob *vb=nullptr;D3DCompile(vs_src,strlen(vs_src),"vs",nullptr,nullptr,"main","vs_5_0",0,0,&vb,nullptr);
        ID3D11VertexShader *vs=nullptr;dev->CreateVertexShader(vb->GetBufferPointer(),vb->GetBufferSize(),nullptr,&vs);
        ID3D11PixelShader *pps=nullptr;check(SUCCEEDED(dev->CreatePixelShader(own_patched.data(),own_patched.size(),nullptr,&pps)),"own_patched_created","own");
        auto solid=[&](float r,float g,float b){float px[4*16];for(int i=0;i<16;++i){px[i*4]=r;px[i*4+1]=g;px[i*4+2]=b;px[i*4+3]=1;}
            D3D11_TEXTURE2D_DESC d={4,4,1,1,DXGI_FORMAT_R32G32B32A32_FLOAT,{1,0},D3D11_USAGE_IMMUTABLE,D3D11_BIND_SHADER_RESOURCE,0,0};D3D11_SUBRESOURCE_DATA s={px,64,0};
            ID3D11Texture2D *t=nullptr;dev->CreateTexture2D(&d,&s,&t);ID3D11ShaderResourceView *v=nullptr;dev->CreateShaderResourceView(t,nullptr,&v);t->Release();return v;};
        ID3D11ShaderResourceView *m=solid(1,1,1),*dd=solid(0.5f,0.25f,0),*aa=solid(0.1f,0.2f,0.3f);
        D3D11_TEXTURE2D_DESC td={4,4,1,1,DXGI_FORMAT_R32G32B32A32_FLOAT,{1,0},D3D11_USAGE_DEFAULT,D3D11_BIND_RENDER_TARGET,0,0};
        ID3D11Texture2D *rt=nullptr,*st=nullptr;dev->CreateTexture2D(&td,nullptr,&rt);td.Usage=D3D11_USAGE_STAGING;td.BindFlags=0;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;dev->CreateTexture2D(&td,nullptr,&st);
        ID3D11RenderTargetView *rtv=nullptr;dev->CreateRenderTargetView(rt,nullptr,&rtv);
        D3D11_SAMPLER_DESC sd={D3D11_FILTER_MIN_MAG_MIP_POINT,D3D11_TEXTURE_ADDRESS_CLAMP,D3D11_TEXTURE_ADDRESS_CLAMP,D3D11_TEXTURE_ADDRESS_CLAMP,0,1,D3D11_COMPARISON_NEVER,{0,0,0,0},0,D3D11_FLOAT32_MAX};
        ID3D11SamplerState *smp=nullptr;dev->CreateSamplerState(&sd,&smp);
        float k[4]={};D3D11_BUFFER_DESC bd={16,D3D11_USAGE_DEFAULT,D3D11_BIND_CONSTANT_BUFFER,0,0,0};D3D11_SUBRESOURCE_DATA kd={k,0,0};ID3D11Buffer *cb=nullptr;dev->CreateBuffer(&bd,&kd,&cb);
        D3D11_VIEWPORT vp={0,0,4,4,0,1};
        auto run=[&](bool bind,float out[4]){
            ctx->OMSetRenderTargets(1,&rtv,nullptr);ctx->RSSetViewports(1,&vp);ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ctx->VSSetShader(vs,nullptr,0);ctx->PSSetShader(pps,nullptr,0);ctx->PSSetShaderResources(10,1,&m);ctx->PSSetSamplers(10,1,&smp);ctx->PSSetConstantBuffers(0,1,&cb);
            ID3D11ShaderResourceView *two[2]={bind?dd:nullptr,bind?aa:nullptr};ctx->PSSetShaderResources(100,2,two);
            ctx->Draw(3,0);ctx->CopyResource(st,rt);D3D11_MAPPED_SUBRESOURCE mp={};ctx->Map(st,0,D3D11_MAP_READ,0,&mp);memcpy(out,mp.pData,16);ctx->Unmap(st,0);};
        float a[4],b[4];run(true,a);run(false,b);
        // base (0.5,0.4,0.2) * (1 - D) + A = (0.35, 0.5, 0.5); unbound = base
        auto close=[](float x,float y){return x>y-0.002f && x<y+0.002f;};
        check(close(a[0],0.35f) && close(a[1],0.5f) && close(a[2],0.5f),"own_render_math","own");
        check(close(b[0],0.5f) && close(b[1],0.4f) && close(b[2],0.2f),"own_render_unbound_is_original","own");
        printf("own render bound %.3f %.3f %.3f / unbound %.3f %.3f %.3f\n",a[0],a[1],a[2],b[0],b[1],b[2]);
    }
    printf("{\"status\":\"%s\",\"failures\":%d}\n",failures?"season_dxbc_fixture_failed":"season_dxbc_fixture_passed",failures);
    return failures?1:0;
}
