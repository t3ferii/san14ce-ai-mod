// User/Claude 2026-10-29: standalone fixture instrumentation declaration.
#ifndef COST_SCOPE
#define COST_SCOPE(name) do {} while(0)
#endif
// User 2026-10-29 R2: production wrapper and uploads on synthetic COM objects.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <thread>
#include <cmath>
static bool enabled=true;
static struct {bool convenience=true,terrain_edit=true;} mod_features;
static unsigned checks;
static void require(bool ok,const char *label){++checks;if(!ok){fprintf(stderr,"FAIL %s (%u)\n",label,checks);ExitProcess(1);}}
static bool terrain_render_fixture_site(void **,void *){return true;}
#define TERRAIN_RENDER_FIXTURE
#ifndef NO_RUNTIME_LOG
struct Event {const char *name;unsigned a,b,c;};
static std::vector<Event> events;
static void log_event(const char *name,unsigned a,unsigned b,unsigned c){events.push_back({name,a,b,c});}
#endif
#include "../src/native_terrain_render.inl"
struct Device {void **table;LONG refs=1;};
struct Texture {void **table;LONG refs=1;};
struct Context {void **table;};
static void *device_table[6]={},*texture_table[3]={},*context_table[49]={};
static Device device{device_table},other_device{device_table};
static Texture textures[2]={{texture_table},{texture_table}};
#ifndef NO_RUNTIME_LOG
static Texture staging{texture_table};
static unsigned staging_pixel;
#endif
static Context context{context_table};
static unsigned selected_texture;
static bool fail_default,empty_output;
static HRESULT fallback_result=S_FALSE;
struct Creation {const D3D11_TEXTURE2D_DESC *address;D3D11_TEXTURE2D_DESC desc;const D3D11_SUBRESOURCE_DATA *initial;ID3D11Device *device;ID3D11Texture2D **out;};
static std::vector<Creation> creations;
static ULONG STDMETHODCALLTYPE device_add(ID3D11Device *d){return ++reinterpret_cast<Device *>(d)->refs;}
static ULONG STDMETHODCALLTYPE device_release(ID3D11Device *d){return --reinterpret_cast<Device *>(d)->refs;}
static ULONG STDMETHODCALLTYPE texture_add(ID3D11Texture2D *t){return ++reinterpret_cast<Texture *>(t)->refs;}
static ULONG STDMETHODCALLTYPE texture_release(ID3D11Texture2D *t){return --reinterpret_cast<Texture *>(t)->refs;}
static HRESULT STDMETHODCALLTYPE create(ID3D11Device *dev,const D3D11_TEXTURE2D_DESC *d,const D3D11_SUBRESOURCE_DATA *initial,ID3D11Texture2D **out) {
    creations.push_back({d,d?*d:D3D11_TEXTURE2D_DESC{},initial,dev,out});
#ifndef NO_RUNTIME_LOG
    // User 2026-10-29 R2b: support the production one-pixel diagnostic copy.
    if(d && d->Usage==D3D11_USAGE_STAGING) {
        require(d->Width==1 && d->Height==1 && d->MipLevels==1 && d->ArraySize==1 && d->Format==DXGI_FORMAT_B8G8R8A8_TYPELESS && d->CPUAccessFlags==D3D11_CPU_ACCESS_READ && !d->BindFlags && !d->MiscFlags && !initial,"readback staging descriptor and null initial data");
        staging.refs=1;*out=reinterpret_cast<ID3D11Texture2D *>(&staging);return S_OK;
    }
#endif
    if(fail_default && d && d->Usage==D3D11_USAGE_DEFAULT){if(out)*out=nullptr;return E_INVALIDARG;}
    if(out)*out=empty_output?nullptr:reinterpret_cast<ID3D11Texture2D *>(&textures[selected_texture]);
    return d && d->Usage==D3D11_USAGE_IMMUTABLE?fallback_result:S_OK;
}
static std::vector<unsigned> gpu;
static std::vector<D3D11_BOX> uploads;
#ifndef NO_RUNTIME_LOG
static void STDMETHODCALLTYPE copy_pixel(ID3D11DeviceContext *,ID3D11Resource *destination,UINT sub,UINT x,UINT y,UINT z,ID3D11Resource *source,UINT source_sub,const D3D11_BOX *box) {
    require(destination==reinterpret_cast<ID3D11Resource *>(&staging) && source==terrain_render::ctrl_texture && !sub && !source_sub && !x && !y && !z,"readback source/destination and subresources");
    require(box && box->right==box->left+1 && box->bottom==box->top+1 && box->front==0 && box->back==1,"exact one-pixel readback source box");
    staging_pixel=gpu[static_cast<size_t>(box->top)*2049+box->left];
}
static HRESULT STDMETHODCALLTYPE map_pixel(ID3D11DeviceContext *,ID3D11Resource *resource,UINT sub,D3D11_MAP access,UINT flags,D3D11_MAPPED_SUBRESOURCE *mapped) {
    require(resource==reinterpret_cast<ID3D11Resource *>(&staging) && sub==0 && access==D3D11_MAP_READ && flags==0,"synchronous readback maps READ without extra idle Presents");
    *mapped={&staging_pixel,4,0};return S_OK;
}
static void STDMETHODCALLTYPE unmap_pixel(ID3D11DeviceContext *,ID3D11Resource *resource,UINT sub) {
    require(resource==reinterpret_cast<ID3D11Resource *>(&staging) && !sub,"successful readback unmaps staging");
}
#endif
static void STDMETHODCALLTYPE upload(ID3D11DeviceContext *ctx,ID3D11Resource *resource,UINT sub,const D3D11_BOX *box,const void *source,UINT pitch,UINT depth) {
    using namespace terrain_render;
    require(ctx==reinterpret_cast<ID3D11DeviceContext *>(&context) && resource==ctrl_texture && sub==0,"owned resource/context and subresource zero");
    require(box && box->front==0 && box->back==1 && box->left<box->right && box->top<box->bottom && box->right<=CtrlSize && box->bottom<=CtrlSize,"bounded nonempty 2D box");
    require(pitch==8196 && depth==0 && source==shadow_ctrl.data()+static_cast<size_t>(box->top)*2049+box->left,"full image row pitch, exact offset pointer, no depth pitch");
    uploads.push_back(*box);
    for(unsigned row=box->top;row<box->bottom;++row)memcpy(gpu.data()+static_cast<size_t>(row)*2049+box->left,
        static_cast<const unsigned char *>(source)+static_cast<size_t>(row-box->top)*pitch,(box->right-box->left)*4);
}
static D3D11_TEXTURE2D_DESC description() {
    D3D11_TEXTURE2D_DESC d={};d.Width=d.Height=2049;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_B8G8R8A8_TYPELESS;d.Usage=D3D11_USAGE_IMMUTABLE;d.BindFlags=D3D11_BIND_SHADER_RESOURCE;return d;
}
static void reset() {
    using namespace terrain_render;
    restore();memset(slots,0,sizeof(slots));stopping=false;permitted=true;render_failed=false;
    resync_needed=present_install_requested=false;clear_targets_locked();creations.clear();uploads.clear();
    enabled=mod_features.convenience=mod_features.terrain_edit=true;fail_default=empty_output=false;selected_texture=0;
    device.refs=other_device.refs=1;for(auto &t:textures)t.refs=1;
    device_table[1]=reinterpret_cast<void *>(&device_add);device_table[2]=reinterpret_cast<void *>(&device_release);device_table[5]=reinterpret_cast<void *>(&create);
    texture_table[1]=reinterpret_cast<void *>(&texture_add);texture_table[2]=reinterpret_cast<void *>(&texture_release);
    context_table[48]=reinterpret_cast<void *>(&upload);
#ifndef NO_RUNTIME_LOG
    context_table[46]=reinterpret_cast<void *>(&copy_pixel);context_table[14]=reinterpret_cast<void *>(&map_pixel);context_table[15]=reinterpret_cast<void *>(&unmap_pixel);
#endif
    on_device(reinterpret_cast<ID3D11Device *>(&device));
#ifndef NO_RUNTIME_LOG
    events.clear();present_reported=false;record_count=bc_count=0;saw_2049=false;
#endif
}
static ID3D11Texture2D *invoke(const D3D11_TEXTURE2D_DESC &d,const D3D11_SUBRESOURCE_DATA *initial,HRESULT *result=nullptr) {
    ID3D11Texture2D *out=nullptr;HRESULT hr=terrain_render::create_texture_2d(reinterpret_cast<ID3D11Device *>(&device),&d,initial,&out);
    if(result)*result=hr;return out;
}
// Independent reference: all 48,400 centers, not the production 3x3 search.
static unsigned nearest(unsigned px,unsigned pz) {
    double best=1e100;unsigned winner=0;
    for(unsigned row=0;row<220;++row)for(unsigned col=0;col<220;++col) {
        double dx=px*100.0-(37594+col*702),dz=pz*100.0-(13335.3+(row+(col%2==0?0.5:0))*std::sqrt(3.0)*468);
        double distance=dx*dx+dz*dz;if(distance<best){best=distance;winner=row*220+col;}
    }
    return winner;
}
static unsigned event_count(const char *name) {
#ifndef NO_RUNTIME_LOG
    unsigned n=0;for(const auto &e:events)if(!strcmp(name,e.name))++n;return n;
#else
    (void)name;return 0;
#endif
}
int main() {
    using namespace terrain_render;
    auto dev=reinterpret_cast<ID3D11Device *>(&device);auto ctx=reinterpret_cast<ID3D11DeviceContext *>(&context);
    const unsigned source_pitch=8216;std::vector<unsigned char> bytes(static_cast<size_t>(source_pitch)*2049+1,0xa5);
    std::vector<unsigned> expected(static_cast<size_t>(2049)*2049);
    for(unsigned z=0;z<2049;++z)for(unsigned x=0;x<2049;++x) {
        unsigned value=0xff000000u|((x%16)<<16)|((z%251)<<8)|((x+z)%255);expected[static_cast<size_t>(z)*2049+x]=value;
        memcpy(bytes.data()+1+static_cast<size_t>(z)*source_pitch+x*4,&value,4);
    }
    auto original_bytes=bytes;D3D11_SUBRESOURCE_DATA initial={bytes.data()+1,source_pitch,0};auto d=description();
    // Exact match only. Each one-field mismatch is an unchanged native call.
    for(unsigned field=0;field<14;++field) {
        reset();auto bad=d;auto input=initial;
        switch(field) {
            case 0:--bad.Width;break;case 1:--bad.Height;break;case 2:bad.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;break; // R3 captures the R32 heightmap separately.
            case 3:bad.MipLevels=2;break;case 4:bad.ArraySize=2;break;case 5:bad.Usage=D3D11_USAGE_DEFAULT;break;
            case 6:bad.BindFlags|=D3D11_BIND_RENDER_TARGET;break;case 7:bad.CPUAccessFlags=D3D11_CPU_ACCESS_READ;break;
            case 8:bad.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;break;case 9:bad.SampleDesc.Count=2;break;
            case 10:bad.SampleDesc.Quality=1;break;case 11:input.pSysMem=nullptr;break;
            case 12:input.SysMemPitch=8195;break;case 13:break;
        }
        invoke(bad,field==13?nullptr:&input);
        require(creations.size()==1 && creations[0].address==&bad && !memcmp(&bad,&creations[0].desc,sizeof(bad)) && !ctrl_texture,"mismatching descriptors/data pass through untouched, including another 2049 format");
        require(!event_count("terrain_render_capture"),"non-target emits no capture event");
    }
    for(unsigned off=0;off<3;++off) {
        reset();enabled=off!=0;mod_features.convenience=off!=1;mod_features.terrain_edit=off!=2;invoke(d,&initial);
        require(creations.size()==1 && creations[0].address==&d && !ctrl_texture,"all existing feature gates preserve immutable input");
    }
    reset();create_texture_2d(dev,&d,&initial,nullptr);require(creations[0].address==&d && !ctrl_texture,"validation-only call remains unchanged");
    reset();HRESULT result;auto captured=invoke(d,&initial,&result);auto want=d;want.Usage=D3D11_USAGE_DEFAULT;
    require(result==S_OK && creations.size()==1 && !memcmp(&want,&creations[0].desc,sizeof(want)) && creations[0].initial==&initial && creations[0].device==dev,"only Usage changed, native arguments and success preserved");
    require(d.Usage==D3D11_USAGE_IMMUTABLE && ctrl_texture==captured && textures[0].refs==2,"caller descriptor untouched and texture retained exactly once");
    require(original_ctrl==expected && shadow_ctrl==expected && bytes==original_bytes,"unaligned pitched rows copied without padding or source modification");
    require(resync_needed && !needs_present(),"capture requests UI resync but idle frame remains unchanged");
    on_present(ctx);require(uploads.empty(),"first idle Present logs entry without uploading");
    bool install=false;require(take_ui_work(install) && install,"first UI service consumes resync and requests Present install");
    require(!take_ui_work(install) && !install,"UI installation request occurs once");
    Center c0=center(0),c1=center(22101),c2=center(22100);
    require(std::abs(c0.x-37594)<1e-8 && std::abs(c0.z-13740.599888971117)<1e-8,"hex zero hand-calculated world center");
    require(std::abs(c1.x-108496)<1e-8 && std::abs(c1.z-94395.27779422345)<1e-8,"row100 col101 hand-calculated center");
    require(std::abs(c2.x-107794)<1e-8 && std::abs(c2.z-94800.57768319457)<1e-8,"row100 col100 staggered center");
    auto b0=hex_box(0),b1=hex_box(22101);require(b0.left==370 && b0.top==131 && b0.right==383 && b0.bottom==144 && b1.left==1079 && b1.top==938 && b1.right==1092 && b1.bottom==951,"fixed rounded pixels (376,137) and (1085,944), radius six inclusive");
    auto clipped=pixel_box(0,2048);require(clipped.left==0 && clipped.top==2042 && clipped.right==7 && clipped.bottom==2049,"box clips texture edges");
    const int terrain_map[][2]={{1,0},{3,1},{2,2},{5,4},{6,4},{8,5},{9,5},{10,5},{11,5},{19,6},{20,6},{4,7},{7,8},{12,9},{13,9},{14,9},{24,9},{21,9}};
    for(const auto &pair:terrain_map)require(material(pair[0])==pair[1],"specified terrain material table");
    mark(48400,3,1);mark(0,15,1);require(!dirty_count && !needs_present(),"invalid hex/material do not enqueue");
    gpu=expected;
    for(unsigned hex:{0u,22101u,22100u,48399u}) {
        mark(hex,3,1);mark(hex,5,1);require(dirty_count==1 && needs_present(),"same-cell updates coalesce and publish atomically");
        on_present(nullptr);require(dirty_count==1,"missing context defers uploads");
        auto box=hex_box(hex);unsigned owned=0,neighbours=0;
        auto before_gpu=gpu;on_present(ctx);
        require(!needs_present() && dirty_count==0 && uploads.back().left==box.left && uploads.back().top==box.top,"upload drains one cell with exact box");
        for(unsigned z=box.top;z<box.bottom;++z)for(unsigned x=box.left;x<box.right;++x) {
            bool own=nearest(x,z)==hex;require(owns_pixel(hex,x,z)==own,"production local-nearest test matches independent full-grid search");
            size_t at=static_cast<size_t>(z)*2049+x;
            require(gpu[at]==(own?0xff04ff00u:before_gpu[at]),"owned pixels use byte2 material; all neighbour pixels preserved");
            if(own)++owned;else ++neighbours;
        }
        require(owned>0 && neighbours>0,"test box covers target and neighbours");
        for(unsigned z=0;z<2049;++z)for(unsigned x=0;x<2049;++x)if(x<box.left || x>=box.right || z<box.top || z>=box.bottom)
            if(gpu[static_cast<size_t>(z)*2049+x]!=before_gpu[static_cast<size_t>(z)*2049+x])require(false,"outside box unchanged");
        mark(hex,1,1);on_present(ctx);require(gpu==before_gpu && shadow_ctrl==gpu,"restore returns original mixed pixels byte-for-byte");
    }
#ifndef NO_RUNTIME_LOG
    require(event_count("terrain_render_present")==1 && event_count("terrain_render_capture")==1 && event_count("terrain_render_upload")==8,"one lifetime Present event, capture success, one event per nonempty frame");
    for(const auto &e:events)if(!strcmp(e.name,"terrain_render_upload"))require(e.a==1 && e.b>0 && e.c==0,"upload diagnostics contain cell and pixel totals");
#endif
    uploads.clear();for(unsigned hex=0;hex<257;++hex)mark(hex,3,1);
    on_present(ctx);require(uploads.size()==256 && dirty_count==1 && needs_present(),"256 cell frame limit leaves remainder pending");
    on_present(ctx);require(uploads.size()==257 && !dirty_count && !needs_present(),"remaining cell uploads next frame");
    unsigned events_before=event_count("terrain_render_upload");on_present(ctx);require(event_count("terrain_render_upload")==events_before,"empty frames emit no upload diagnostic");
    for(unsigned hex=0;hex<48400;++hex)mark(hex,3,1);
    require(dirty_count==48400,"full-world queue fits all cells after ring head moved");
    mark(17,5,1);require(dirty_count==48400 && goals[17]==4,"full ring coalesces latest cell goal");
    // User 2026-10-29 R2b supersedes R2's goal-reset boundary.
    before_world_change();require(dirty_count==0 && !needs_present() && !queued[17] && goals[17]==4 && has_goal[17],"world boundary preserves targets while clearing upload queue and atomic flag");
    // Existing UI producer and Present consumer may overlap, but never lose a goal.
    std::thread ui_thread([] {for(unsigned h=1000;h<1600;++h){mark(h,3,1);mark(h,5,1);}});
    for(unsigned pass=0;pass<4;++pass)on_present(ctx);
    ui_thread.join();while(needs_present())on_present(ctx);
    for(unsigned h=1000;h<1600;++h) {
        Center c=center(h);unsigned x=static_cast<unsigned>(std::floor(c.x/100+0.5)),z=static_cast<unsigned>(std::floor(c.z/100+0.5));
        require(gpu[static_cast<size_t>(z)*2049+x]==0xff04ff00u,"concurrent mark/upload retains final material per cell");
    }
    before_world_change();
    mark(0,3,1);selected_texture=1;invoke(d,&initial);
    require(textures[0].refs==1 && textures[1].refs==2 && resync_needed && !needs_present(),"replacement releases old retention, resets shadow and waits for UI replay");
    require(take_ui_work(install) && !install,"replacement resync does not reinstall Present");
    mark(0,3,1);on_device(reinterpret_cast<ID3D11Device *>(&other_device));
    require(!ctrl_texture && textures[1].refs==1 && original_ctrl.empty() && shadow_ctrl.empty() && !needs_present(),"device change discards retained texture and CPU maps");
    reset();fail_default=true;invoke(d,&initial,&result);
    require(creations.size()==2 && creations[0].desc.Usage==D3D11_USAGE_DEFAULT && creations[1].address==&d && creations[1].initial==&initial && result==S_FALSE,"DEFAULT failure retries exact native descriptor and returns retry HRESULT");
    require(render_failed && !ctrl_texture && !needs_present(),"failure disables rendering for session");
#ifndef NO_RUNTIME_LOG
    bool failure_event=false;for(const auto &event:events)if(!strcmp(event.name,"terrain_render_capture") && event.a==90 && !event.b)failure_event=true;
    require(event_count("terrain_render_capture")==1 && failure_event,"failed capture diagnostic format and zero result");
#endif
    invoke(d,&initial);require(creations.size()==3 && creations.back().address==&d,"failed session never attempts mutable creation again");
    reset();invoke(d,&initial);mark(0,3,1);restore();require(textures[0].refs==1 && !ctrl_texture && !needs_present(),"DLL restore releases retained texture and cancels pending upload");
    restore();require(textures[0].refs==1,"repeated DLL cleanup does not double release");
    printf("{\"terrain_render_r2_checks\":%u,\"failed\":0,\"real_game\":false}\n",checks);
}
