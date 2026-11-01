// User/Claude 2026-10-29 local56: mask = original + ground offset + carve change; inlier fit selects the mask in every build.
#define TERRAIN_MASK_DUMP_FIXTURE 1
#define WIN32_LEAN_AND_MEAN
#define COST_SCOPE(name) do {} while(0)
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <cstring>
#include <vector>
static bool enabled=true;
static struct {bool convenience=true,terrain_edit=true;} mod_features;
static unsigned checks;
static void require(bool ok,const char *label){++checks;if(!ok){fprintf(stderr,"FAIL %s (%u)\n",label,checks);ExitProcess(1);}}
#ifndef NO_RUNTIME_LOG
struct Event {const char *name;unsigned a,b,c;};
static std::vector<Event> events;
static void log_event(const char *name,unsigned a,unsigned b,unsigned c){events.push_back({name,a,b,c});}
#endif
#include "../src/native_terrain_render.inl"
using namespace terrain_render;
static ID3D11Device *device;
static ID3D11DeviceContext *context;
static std::vector<float> ground,mask;
static D3D11_TEXTURE2D_DESC description(unsigned size,D3D11_USAGE usage= D3D11_USAGE_IMMUTABLE) {
    D3D11_TEXTURE2D_DESC d={};d.Width=d.Height=size;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_R32_TYPELESS;d.Usage=usage;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;return d;
}
static ID3D11Texture2D *make(const D3D11_TEXTURE2D_DESC &d,const void *source,unsigned pitch) {
    ID3D11Texture2D *t=nullptr;D3D11_SUBRESOURCE_DATA initial={source,pitch,0};
    require(SUCCEEDED(device->CreateTexture2D(&d,source?&initial:nullptr,&t)),"production CreateTexture2D wrapper succeeds");return t;
}
static std::vector<float> read(ID3D11Texture2D *t) {
    D3D11_TEXTURE2D_DESC d={};t->GetDesc(&d);unsigned size=d.Width;
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    auto staging=make(d,nullptr,0);context->CopyResource(staging,t);
    D3D11_MAPPED_SUBRESOURCE map={};require(SUCCEEDED(context->Map(staging,0,D3D11_MAP_READ,0,&map)),"independent GPU readback");
    std::vector<float> result(static_cast<size_t>(size)*size);
    for(unsigned z=0;z<size;++z)memcpy(result.data()+static_cast<size_t>(z)*size,
        static_cast<unsigned char *>(map.pData)+static_cast<size_t>(z)*map.RowPitch,size*4);
    context->Unmap(staging,0);staging->Release();return result;
}
static bool identical(const std::vector<float> &a,const std::vector<float> &b) {
    return a.size()==b.size() && !memcmp(a.data(),b.data(),a.size()*4);
}
static void source(int dx,int dz,float scale=1,bool half=false) {
    ground.resize(static_cast<size_t>(CtrlSize)*CtrlSize);
    for(unsigned z=0;z<CtrlSize;++z)for(unsigned x=0;x<CtrlSize;++x)
        ground[static_cast<size_t>(z)*CtrlSize+x]=786+80*std::sin(x*0.023f)+90*std::cos(z*0.031f)+50*std::sin((x+z)*0.071f);
    mask.resize(static_cast<size_t>(MaskSize)*MaskSize);
    for(unsigned z=0;z<MaskSize;++z)for(unsigned x=0;x<MaskSize;++x) {
        int gx=(std::max)(0,(std::min)(2048,static_cast<int>(x)+dx));
        int gz=(std::max)(0,(std::min)(2048,static_cast<int>(z)+dz));
        int gx1=(std::min)(2048,gx+1),gz1=(std::min)(2048,gz+1);
        size_t at=static_cast<size_t>(gz)*CtrlSize+gx;
        float value=half?(ground[at]+ground[static_cast<size_t>(gz)*CtrlSize+gx1]+
            ground[static_cast<size_t>(gz1)*CtrlSize+gx]+ground[static_cast<size_t>(gz1)*CtrlSize+gx1])*0.25f:ground[at];
        mask[static_cast<size_t>(z)*MaskSize+x]=value*scale;
    }
}
static void check_edit(ID3D11Texture2D *asset,ID3D11Texture2D *work,unsigned hex) {
    auto gpu=read(asset),mirror=read(work);unsigned changed=0;(void)hex;
    for(unsigned z=0;z<MaskSize;++z)for(unsigned x=0;x<MaskSize;++x) {
        size_t at=static_cast<size_t>(z)*MaskSize+x;
        int gx=static_cast<int>(x)+mask_offset_x,gz=static_cast<int>(z)+mask_offset_z;
        float expected=mask[at];
        if(gx>=0 && gx<2049 && gz>=0 && gz<2049) {
            float offset=height_at(gx,gz)-original_height[static_cast<size_t>(gz)*CtrlSize+gx]+field_at(carve_delta,gx,gz);
            if(offset!=0.0f)expected=mask[at]+offset*mask_scale;
        }
        if(expected!=mask[at])++changed;
        require(gpu[at]==expected && mirror[at]==expected,"whole GPU mask and work mirror = original + offset + carve, elsewhere unchanged");
    }
    require(changed>0 && changed<=529,"writes stay inside the edited hex box");
}
#ifndef NO_RUNTIME_LOG
static void verify_dump(const wchar_t *name,const std::vector<float> &expected) {
    wchar_t path[MAX_PATH]={};swprintf_s(path,L"%s_%lu.bin",name,GetCurrentProcessId());
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    require(file!=INVALID_HANDLE_VALUE,"confirmation creates raw PID dump");
    LARGE_INTEGER size={};require(GetFileSizeEx(file,&size) && size.QuadPart==static_cast<LONGLONG>(expected.size()*4),"dump exact byte count, no header");
    std::vector<float> bytes(expected.size());DWORD got=0;
    require(ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()*4),&got,nullptr) && got==bytes.size()*4 && identical(bytes,expected),"dump contains original floats and row padding is excluded");
    CloseHandle(file);
}
#endif
int main() {
    require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"real WARP device");
    on_device(device);require(permitted && guards_match(),"existing guarded hook admits actual System32 implementation");
    source(3,0);
    auto gd=description(CtrlSize);auto g=make(gd,ground.data(),CtrlPitch);
    unsigned pitch=MaskPitch+16;std::vector<unsigned char> padded(static_cast<size_t>(pitch)*MaskSize,0xa5);
    for(unsigned z=0;z<MaskSize;++z)memcpy(padded.data()+static_cast<size_t>(z)*pitch,mask.data()+static_cast<size_t>(z)*MaskSize,MaskPitch);
    auto md=description(MaskSize);auto a=make(md,padded.data(),pitch);
    float uv[4]={1.0f/204800,1.0f/204800,-300,700};
    confirm_mask(a,uv);require(!mask_ready,"diagnostic binding callback alone never authorizes writes");
    auto wd=description(MaskSize,D3D11_USAGE_DEFAULT);wd.BindFlags|=D3D11_BIND_RENDER_TARGET|D3D11_BIND_UNORDERED_ACCESS;
    auto work=make(wd,nullptr,0);context->UpdateSubresource(work,0,nullptr,mask.data(),MaskPitch,0);
    require(mask_work_count==1,"uninitialized same-device R32 work texture retained");
    unsigned hex=22100;mark(hex,13,1);
    require(carve_delta[hex]==-1518.0f,"land-to-river carve change recorded");
    height_cell_locked(context,hex);
    require(mask_selected==0 && mask_ready && mask_offset_x==3 && mask_offset_z==0 && mask_scale==1 && mask_model==0,"sole fitting candidate selected with integer offset");
#ifndef NO_RUNTIME_LOG
    verify_dump(L"water_mask",mask);verify_dump(L"ground_height",ground);
    unsigned fit3=0,ready=0;
    for(auto e:events){if(!strcmp(e.name,"water_mask_fit3"))++fit3;if(!strcmp(e.name,"water_mask_ready") && e.a==1)++ready;}
    require(fit3==1 && ready==1,"fit and readiness logged once");
#endif
    check_edit(a,work,hex);
    require(identical(ground,original_height),"ground original never mutated");
    mark(hex,1,1);height_cell_locked(context,hex);
    require(identical(read(a),mask) && identical(read(work),mask) && identical(water_masks[0].shadow,mask),"restoration returns original bits everywhere");
    mark(hex,21,14);height_cell_locked(context,hex);
    require(height_delta[hex]==0 && carve_delta[hex]==-3266.0f,"water-to-rapids keeps the bed but deepens the mask");
    check_edit(a,work,hex);
    mark(hex,24,14);height_cell_locked(context,hex);require(identical(read(a),mask),"great river to torrent has no carve change");
    mark(hex,1,13);height_cell_locked(context,hex);check_edit(a,work,hex);
    mark(hex,13,13);height_cell_locked(context,hex);require(identical(read(a),mask),"water-to-land edit restores exactly");
    mark(hex,13,1);height_cell_locked(context,hex);before_world_change();bool install=false;
    std::vector<unsigned> ctrl(ground.size(),0xff00ff00u);auto cd=gd;cd.Format=DXGI_FORMAT_B8G8R8A8_TYPELESS;
    auto control=make(cd,ctrl.data(),CtrlPitch);require(take_ui_work(install),"existing load resync consumes targets");
    height_cell_locked(context,hex);require(identical(read(a),mask),"omitted sidecar targets restore mask during resync");
    // A late second passing candidate must disable, restoring already edited bits.
    mark(hex,13,1);height_cell_locked(context,hex);
    auto late_work=make(wd,nullptr,0);
    auto b=make(md,mask.data(),MaskPitch);height_cell_locked(context,hex);
    require(mask_fit_attempted && mask_ready && mask_selected==1,"newest passing candidate wins");
    require(identical(read(a),mask),"older candidate's earlier edits are restored");
    require(!identical(read(b),mask),"newest candidate receives the edit");late_work->Release();
#ifdef NO_RUNTIME_LOG
    wchar_t file[MAX_PATH]={};swprintf_s(file,L"water_mask_%lu.bin",GetCurrentProcessId());
    require(GetFileAttributesW(file)==INVALID_FILE_ATTRIBUTES,"release emits no mask dump");
#endif
    clear_targets_locked();discard_masks_locked();
    std::vector<float> corrupt(mask.size(),123);auto rejected=make(md,corrupt.data(),MaskPitch);
    confirm_release_masks_locked(context);
    require(mask_fit_attempted && !mask_ready,"unrelated captured candidate fails fitting");
    mark(hex,13,1);height_cell_locked(context,hex);
    require(identical(read(rejected),corrupt),"failed fit never changes candidate GPU bytes");
    water_masks[0].original=mask;confirm_release_masks_locked(context);
    require(!mask_ready,"failed fit is not retried every frame");
    rejected->Release();clear_targets_locked();discard_masks_locked();
    for(int dx:{-8,-3,0,3,8})for(int dz:{-8,0,8}) {
        source(dx,dz);original_height=ground;WaterMask candidate;candidate.original=mask;unsigned error=0;
        require(fit_mask_locked(candidate,uv,error) && mask_offset_x==dx && mask_offset_z==dz && mask_scale==1 && mask_model==0 && error==0,"synthetic fit finds signed boundary offsets");
    }
    {   // Carved water on about a third of the map is excluded from the fit.
        source(1,1);original_height=ground;WaterMask candidate;candidate.original=mask;unsigned error=0;
        for(unsigned z=0;z<MaskSize;++z)if((z/64)%3==0)for(unsigned x=0;x<MaskSize;++x)candidate.original[static_cast<size_t>(z)*MaskSize+x]-=2000;
        require(fit_mask_locked(candidate,uv,error) && mask_offset_x==1 && mask_offset_z==1 && error==0,"carved outliers do not move the fit");
    }
    require(mask_acceptable(40,200,100,0.6f) && !mask_acceptable(40.001f,200,100,0.6f) && !mask_acceptable(40,200.001f,100,0.6f),"median/p90 thresholds are inclusive");
    require(!mask_acceptable(0,0,100,0.599f),"too few inliers rejected");
    require(!mask_acceptable(0,0,49.999f,1),"flat coordinate fit remains rejected");
    WaterMask invalid;invalid.original.assign(mask.size(),123);unsigned error=0;
    require(!fit_mask_locked(invalid,uv,error),"unrelated values rejected");
    original_height.assign(ground.size(),800);invalid.original.assign(mask.size(),800);
    require(!fit_mask_locked(invalid,uv,error),"flat ground cannot establish mapping");
    // Two candidates, only one accepted; confirmation also works when ground arrives last.
    discard_height_locked();
    invalid.original.assign(mask.size(),123);auto bad=make(md,invalid.original.data(),MaskPitch);
    source(-2,1);auto good=make(md,mask.data(),MaskPitch);auto later_ground=make(gd,ground.data(),CtrlPitch);
    height_cell_locked(context,hex);
    require(mask_ready && mask_selected==1 && mask_model==0 && mask_offset_x==-2 && mask_offset_z==1,"sole passing candidate selected with ground captured last");
    auto overflow=make(md,mask.data(),MaskPitch);height_cell_locked(context,hex);
    require(!mask_ready && mask_overflow,"candidate overflow conservatively disables writes");
    overflow->Release();later_ground->Release();good->Release();bad->Release();
    md.Format=DXGI_FORMAT_R32_FLOAT;D3D11_SUBRESOURCE_DATA initial={mask.data(),MaskPitch,0};
    require(!mask_desc(&md,&initial),"only initialized immutable format39 is admitted");
    control->Release();work->Release();b->Release();a->Release();g->Release();
    restore();context->Release();device->Release();
    printf("{\"terrain_render_water_mask_checks\":%u,\"failed\":0,\"real_d3d11_warp\":true,\"gameplay_verified\":false}\n",checks);
}
