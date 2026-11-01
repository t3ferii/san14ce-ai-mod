// User/Claude 2026-10-29: standalone fixture instrumentation declaration.
#ifndef COST_SCOPE
#define COST_SCOPE(name) do {} while(0)
#endif
// User/Claude 2026-10-29 local48: production cliff profiles, CPU and WARP GPU.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
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
static unsigned encoded(float h,float low,float high) {
    double t=(h-low)/(high-low);t=t<0?0:t>1?1:t;
    return static_cast<unsigned>(t*4294967295.0+0.5);
}
struct Cpu {
    std::vector<unsigned long long> memory;
    float low,high;
    Cpu(float l,float h):memory((0x58+static_cast<size_t>(CtrlSize)*CtrlSize*4+7)/8),low(l),high(h) {
        auto o=object();*reinterpret_cast<const void **>(o)=heightfield_vtable;
        memcpy(o+0x38,"_MHK0100",8);
        *reinterpret_cast<unsigned short *>(o+0x48)=CtrlSize-1;
        *reinterpret_cast<unsigned short *>(o+0x4a)=CtrlSize-1;
        *reinterpret_cast<float *>(o+0x50)=low;*reinterpret_cast<float *>(o+0x54)=high;
        reset();
    }
    unsigned char *object(){return reinterpret_cast<unsigned char *>(memory.data());}
    unsigned *data(){return reinterpret_cast<unsigned *>(object()+0x58);}
    void reset(){for(size_t at=0;at<original_height.size();++at)data()[at]=encoded(original_height[at],low,high);}
};
static ID3D11Device *device;
static ID3D11DeviceContext *context;
static ID3D11Texture2D *staging;
static void make_texture(ID3D11Texture2D **out,DXGI_FORMAT format,const void *source) {
    D3D11_TEXTURE2D_DESC d={};d.Width=d.Height=CtrlSize;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=format;d.Usage=D3D11_USAGE_DEFAULT;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial={source,CtrlPitch,0};
    require(SUCCEEDED(device->CreateTexture2D(&d,&initial,out)),"fixture GPU texture creation");
}
static std::vector<float> read_gpu(ID3D11Texture2D *texture) {
    context->CopyResource(staging,texture);D3D11_MAPPED_SUBRESOURCE mapped={};
    require(SUCCEEDED(context->Map(staging,0,D3D11_MAP_READ,0,&mapped)),"independent WARP height readback");
    std::vector<float> result(original_height.size());
    for(unsigned z=0;z<CtrlSize;++z)memcpy(result.data()+static_cast<size_t>(z)*CtrlSize,
        static_cast<unsigned char *>(mapped.pData)+static_cast<size_t>(z)*mapped.RowPitch,CtrlPitch);
    context->Unmap(staging,0);return result;
}
static void upload_asset() {
    shadow_height=original_height;
    context->UpdateSubresource(height_texture,0,nullptr,original_height.data(),CtrlPitch,0);
    for(auto texture:height_work)context->UpdateSubresource(texture,0,nullptr,original_height.data(),CtrlPitch,0);
}
static void verify_box(unsigned hex,Cpu &a,Cpu &b) {
    height_cell_locked(context,hex);
    auto gpu=read_gpu(height_texture),work0=read_gpu(height_work[0]),work1=read_gpu(height_work[1]);
    Center c=center(hex);int cx=static_cast<int>(center_pixel(c.x)),cz=static_cast<int>(center_pixel(c.z));
    for(int z=cz-11;z<=cz+11;++z)for(int x=cx-11;x<=cx+11;++x) {
        size_t at=static_cast<size_t>(z)*CtrlSize+x;float expected=height_at(x,z);
        require(gpu[at]==expected && work0[at]==expected && work1[at]==expected,"asset and both GPU work textures use height_at");
        require(a.data()[at]==encoded(expected,a.low,a.high) && b.data()[at]==encoded(expected,b.low,b.high),"both CPU ranges encode the same height_at");
    }
}
static float sample(double x,double z){return height_at(center_pixel(x),center_pixel(z));}
static void set_center(unsigned hex,float value) {
    Center c=center(hex);original_height[static_cast<size_t>(center_pixel(c.z))*CtrlSize+center_pixel(c.x)]=value;
}
int main() {
    require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"real WARP device");
    original_height.assign(static_cast<size_t>(CtrlSize)*CtrlSize,800.0f);
    original_ctrl.assign(original_height.size(),0xff00ff00u);shadow_ctrl=original_ctrl;
    make_texture(&ctrl_texture,DXGI_FORMAT_B8G8R8A8_TYPELESS,original_ctrl.data());ctrl_device=device;
    make_texture(&height_texture,DXGI_FORMAT_R32_TYPELESS,original_height.data());height_device=device;
    for(auto &texture:height_work)make_texture(&texture,DXGI_FORMAT_R32_TYPELESS,original_height.data());
    height_work_count=MaxWork;
    D3D11_TEXTURE2D_DESC d={};height_texture->GetDesc(&d);d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    require(SUCCEEDED(device->CreateTexture2D(&d,nullptr,&staging)),"read-only R32 staging");
    static const unsigned fake_vtable=1;heightfield_vtable=&fake_vtable;
    Cpu a(0,6966.2f),b(-1000,7966.2f);
    heightfields[0]=a.object();heightfields[1]=b.object();
    require(height_change(20,1)==0 && height_change(20,19)==0 && height_change(20,20)==0,"cliffs never add a height delta");
    for(unsigned col: {100u,101u}) {
        unsigned hex=100*220+col;Center c=center(hex);
        // Independent neighbour construction tests all six directions in both parities.
        unsigned neighbours[6]={hex-220,hex+220,hex-1,hex+1,
            static_cast<unsigned>(static_cast<int>(hex)-1+(col%2==0?220:-220)),
            static_cast<unsigned>(static_cast<int>(hex)+1+(col%2==0?220:-220))};
        for(unsigned direction=0;direction<6;++direction) {
            clear_targets_locked();std::fill(original_height.begin(),original_height.end(),800.0f);
            unsigned hi=neighbours[direction],lo=neighbours[direction^1];
            // Opposite direction is not required: use the actual high-low vector.
            set_center(hi,1600);set_center(lo,400);a.reset();b.reset();upload_asset();
#ifndef NO_RUNTIME_LOG
            unsigned count=static_cast<unsigned>(events.size());
#endif
            mark(hex,20,1);CliffSection section=cliff_section(hex);
            require(cliff[hex] && height_delta[hex]==0 && section.hi==1600 && section.lo==400,"six adjacent centres supply high/low with correct parity");
#ifndef NO_RUNTIME_LOG
            require(events.size()==count+1 && !strcmp(events.back().name,"terrain_cliff") && events.back().a==hex && events.back().b==1200 && events.back().c==1,"one mark diagnostic for slope mode");
#endif
            Center h=center(hi),l=center(lo);double dx=h.x-l.x,dz=h.z-l.z,len=std::sqrt(dx*dx+dz*dz);dx/=len;dz/=len;
            float high=sample(c.x+dx*385,c.z+dz*385),low=sample(c.x-dx*385,c.z-dz*385);
            require(high>1580 && low<420,"high-side edge approaches hi and low-side edge approaches lo");
            float middle=sample(c.x+dx*100,c.z+dz*100)-sample(c.x-dx*100,c.z-dz*100);
            float outer=high-sample(c.x+dx*200,c.z+dz*200);
            require(middle>600 && middle>outer*3,"sigmoid concentrates the steep fall near the centre");
            auto box=hex_box(hex);
            for(unsigned z=box.top;z<box.bottom;++z)for(unsigned x=box.left;x<box.right;++x)
                require((pixel_owner(x,z)==hex)==owns_pixel(hex,x,z),"profile owner matches material owner");
            for(unsigned z=center_pixel(c.z)-11;z<=center_pixel(c.z)+11;++z)for(unsigned x=center_pixel(c.x)-11;x<=center_pixel(c.x)+11;++x)
                if(!owns_pixel(hex,x,z))require(height_at(x,z)==original_height[static_cast<size_t>(z)*CtrlSize+x],"cliff leaves every neighbouring pixel unchanged");
            verify_box(hex,a,b);
            mark(hex,1,1);verify_box(hex,a,b);
            require(!cliff[hex] && shadow_height==original_height,"original terrain restores every GPU shadow float exactly");
        }
    }
    unsigned hex=22100;Center c=center(hex);unsigned x=center_pixel(c.x),z=center_pixel(c.z);size_t at=static_cast<size_t>(z)*CtrlSize+x;
    clear_targets_locked();std::fill(original_height.begin(),original_height.end(),800.0f);a.reset();b.reset();upload_asset();
    mark(hex,20,1);
    require(height_at(x,z)==1200 && sample(c.x+300,c.z)==1200,"flat plateau centre and inner area rise by 400");
    require(sample(c.x+400,c.z)==800,"plateau outer edge returns to original height");
    unsigned transition_pixels=0;
    for(unsigned pz=z-5;pz<=z+5;++pz)for(unsigned px=x-5;px<=x+5;++px)if(owns_pixel(hex,px,pz)) {
        double dx=px*100.0-c.x,dz=pz*100.0-c.z,r=std::sqrt(dx*dx+dz*dz)/468.0;
        if(r>0.70 && r<0.82) {
            float h=height_at(px,pz);require(h>800 && h<1200,"plateau transition pixels fall sharply in 0.70..0.82 radius");++transition_pixels;
        }
    }
    require(transition_pixels>0,"actual pixel grid samples plateau transition");
#ifndef NO_RUNTIME_LOG
    require(events.back().b==0 && events.back().c==2,"flat-mode diagnostic");
#endif
    verify_box(hex,a,b);
    // A later adjacent mountain edit must update the already-marked cliff as well.
    mark(hex+1,19,1);float revised=height_at(x,z);
    require(cliff_section(hex).hi-cliff_section(hex).lo>=150 && revised!=1200,"later neighbour delta changes cliff mode and height");
    verify_box(hex+1,a,b);
    mark(hex+1,1,1);verify_box(hex+1,a,b);
    require(height_at(x,z)==1200,"removing neighbour edit restores plateau");
    // Regression: an existing east-west cliff changes direction when its north
    // neighbour rises. Some south-edge pixels are outside that neighbour's box.
    mark(hex+1,19,1);mark(hex-220,19,1);
    require(queued[hex],"later neighbour edit queues cliff owner for complete GPU profile refresh");
    auto owned=hex_box(hex);
    for(unsigned pz=owned.top;pz<owned.bottom;++pz)for(unsigned px=owned.left;px<owned.right;++px)if(owns_pixel(hex,px,pz)) {
        size_t index=static_cast<size_t>(pz)*CtrlSize+px;float h=height_at(px,pz);
        require(a.data()[index]==encoded(h,a.low,a.high) && b.data()[index]==encoded(h,b.low,b.high),"neighbour edits refresh all owned CPU pixels including opposite edge");
    }
    on_present(context);auto updated=read_gpu(height_texture);
    for(unsigned pz=owned.top;pz<owned.bottom;++pz)for(unsigned px=owned.left;px<owned.right;++px)if(owns_pixel(hex,px,pz))
        require(updated[static_cast<size_t>(pz)*CtrlSize+px]==height_at(px,pz),"Present refreshes every owned GPU cliff pixel after neighbour edit");
    mark(hex+1,1,1);mark(hex-220,1,1);verify_box(hex,a,b);
    // Fresh collision objects must reapply zero-delta cliff targets.
    a.reset();heightfields[0]=nullptr;heightfield_created(0,a.object(),heightfield_vtable);
    require(a.data()[at]==encoded(1200,a.low,a.high),"fresh CPU collision object reapplies zero-delta cliff");
    bool install=false;resync_needed=true;require(take_ui_work(install),"full sidecar resync requested");
    require(!cliff[hex] && !has_goal[hex],"resync clears omitted cliff flag");verify_box(hex,a,b);
    mark(hex,20,1);before_world_change();require(!cliff[hex] && has_goal[hex] && world_transition,"world boundary clears cliff while retaining material goal");
    mark(hex,1,1);require(!cliff[hex],"world rollback cannot revive cliff");
    require(take_ui_work(install),"world boundary requests full resync");verify_box(hex,a,b);
    mark(hex,20,1);mark(hex,3,1);require(!cliff[hex],"reconcile to another terrain clears cliff");
    mark(hex,20,20);require(!cliff[hex],"original cliff remains original geometry");
    // Boundary hexes exercise clipped neighbour lists and absence of row wrapping.
    for(unsigned edge: {0u,219u,48180u,48399u}) {
        mark(edge,20,1);CliffSection section=cliff_section(edge);
        require(section.lo==800 && section.hi==800,"map boundary only samples valid adjacent centres");
        Center e=center(edge);require(sample(e.x,e.z)==1200,"boundary plateau remains finite");
        mark(edge,1,1);
    }
    clear_targets_locked();require(!cliff[hex],"target reset clears all cliff flags");
    heightfields[0]=heightfields[1]=nullptr;restore();staging->Release();context->Release();device->Release();
    printf("{\"terrain_render_cliff_checks\":%u,\"failed\":0,\"real_d3d11_warp\":true,\"gameplay_verified\":false}\n",checks);
}
