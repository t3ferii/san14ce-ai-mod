// User/Claude 2026-10-29: standalone fixture instrumentation declaration.
#ifndef COST_SCOPE
#define COST_SCOPE(name) do {} while(0)
#endif
// User 2026-10-29 R2b: real WARP resources, production wrapper and UI boundary.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include <thread>
static bool enabled=true;
static struct {bool convenience=true,terrain_edit=true;unsigned terrain_diag_mode=0;} mod_features;
static unsigned checks;
static void require(bool ok,const char *label){++checks;if(!ok){fprintf(stderr,"FAIL %s (%u)\n",label,checks);ExitProcess(1);}}
#ifndef NO_RUNTIME_LOG
struct Event {const char *name;unsigned a,b,c;DWORD thread;};
static std::vector<Event> events;
static void log_event(const char *name,unsigned a,unsigned b,unsigned c){events.push_back({name,a,b,c,GetCurrentThreadId()});}
static unsigned count_event(const char *name){unsigned count=0;for(const auto &e:events)if(!strcmp(name,e.name))++count;return count;}
#endif
#include "../src/native_terrain_render.inl"
using Obj=unsigned char *;
static unsigned char root[2],tile0[32],tile1[32];
static DWORD ui_thread;
static unsigned game_reads;
static Obj world(){require(GetCurrentThreadId()==ui_thread,"world getter stays on native UI caller");++game_reads;return root;}
static Obj table(unsigned offset,unsigned id,unsigned) {
    require(GetCurrentThreadId()==ui_thread,"tile getter stays on native UI caller");++game_reads;
    return offset==0xE270?(id==0?tile0:id==22101?tile1:nullptr):nullptr;
}
static bool valid(Obj){return false;}
static unsigned short word(Obj,unsigned){return 0;}
static unsigned cost_cache_clears=0;
static void terrain_edit_fixture_cost_cache_clear() {++cost_cache_clears;}
#define TERRAIN_EDIT_FIXTURE_COST_CACHE 1
#include "../src/native_terrain_edit.inl"
static unsigned gpu_pixel(ID3D11Device *device,ID3D11DeviceContext *context,ID3D11Texture2D *texture,unsigned hex) {
    auto c=terrain_render::center(hex);unsigned x=static_cast<unsigned>(std::floor(c.x/100+0.5)),z=static_cast<unsigned>(std::floor(c.z/100+0.5));
    D3D11_TEXTURE2D_DESC desc={};desc.Width=desc.Height=desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_B8G8R8A8_TYPELESS;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D *staging=nullptr;require(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&staging)),"independent fixture staging creation");
    D3D11_BOX box={x,z,0,x+1,z+1,1};context->CopySubresourceRegion(staging,0,0,0,0,texture,0,&box);
    D3D11_MAPPED_SUBRESOURCE mapped={};require(SUCCEEDED(context->Map(staging,0,D3D11_MAP_READ,0,&mapped)),"independent fixture GPU read");
    unsigned value;memcpy(&value,mapped.pData,4);context->Unmap(staging,0);staging->Release();return value;
}
int main() {
    using namespace terrain_render;
    ui_thread=GetCurrentThreadId();tile0[0x10]=tile1[0x10]=1;
    ID3D11Device *device=nullptr;ID3D11DeviceContext *context=nullptr;
    require(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"real software D3D11 device");
    on_device(device);require(permitted && guards_match(),"production system DLL and vtable guard accepts WARP device");
    const unsigned row_pitch=8216;
    std::vector<unsigned char> source(static_cast<size_t>(row_pitch)*2049,0x6c);
    const unsigned asset=0xff06ff00u;
    for(unsigned z=0;z<2049;++z)for(unsigned x=0;x<2049;++x)memcpy(source.data()+static_cast<size_t>(z)*row_pitch+x*4,&asset,4);
    auto untouched=source;
    D3D11_TEXTURE2D_DESC desc={};desc.Width=desc.Height=2049;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_B8G8R8A8_TYPELESS;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial={source.data(),row_pitch,0};ID3D11Texture2D *first=nullptr;
    require(SUCCEEDED(device->CreateTexture2D(&desc,&initial,&first)) && ctrl_texture==first,"production control capture");
    D3D11_TEXTURE2D_DESC actual={};first->GetDesc(&actual);require(actual.Usage==D3D11_USAGE_DEFAULT,"captured real GPU texture has DEFAULT usage");
    require(terrain_edit::apply(0,3)==terrain_edit::Result::Applied && terrain_edit::apply(22101,5)==terrain_edit::Result::Applied,"two UI terrain edits");
    on_present(context);
    require(gpu_pixel(device,context,first,0)==0xff01ff00u && gpu_pixel(device,context,first,22101)==0xff04ff00u,"real GPU pixels reflect both normal uploads");
#ifndef NO_RUNTIME_LOG
    require(count_event("terrain_render_readback")==1 && !count_event("terrain_render_readback_error"),"one diagnostic readback for first cell, no errors");
    bool sample_ok=false;for(const auto &e:events)if(!strcmp(e.name,"terrain_render_readback"))sample_ok=e.a==0 && e.b==0xff01ff00u && e.c==e.b && e.thread==ui_thread;
    require(sample_ok,"readback log carries hex, actual GPU word and expected shadow word");
    require(readback_texture!=nullptr,"diagnostic staging retained for reuse");
    readback_texture->GetDesc(&actual);
    require(actual.Width==1 && actual.Height==1 && actual.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS && actual.Usage==D3D11_USAGE_STAGING && actual.CPUAccessFlags==D3D11_CPU_ACCESS_READ && !actual.BindFlags,"actual diagnostic staging descriptor");
#endif
    terrain_edit::before_world_change();
    require(tile0[0x10]==1 && tile1[0x10]==1 && !terrain_edit::changes.size(),"native tiles rolled back at load boundary");
    require(has_goal[0] && goals[0]==1 && has_goal[22101] && goals[22101]==4 && dirty_count==0 && !needs_present(),"CPU rollback preserves both render goals and cancels old uploads");
    unsigned reads_before=game_reads;ID3D11Texture2D *second=nullptr;HRESULT loaded=E_FAIL;
    std::thread load_thread([&]{loaded=device->CreateTexture2D(&desc,&initial,&second);});load_thread.join();
    require(SUCCEEDED(loaded) && ctrl_texture==second && game_reads==reads_before,"load-thread initial patching never reads game memory");
    require(source==untouched && original_ctrl.front()==asset,"source and raw original copy remain untouched");
    require(gpu_pixel(device,context,second,0)==0xff01ff00u && gpu_pixel(device,context,second,22101)==0xff04ff00u,"preserved goals exist on GPU immediately at CreateTexture2D return, before UI restore or upload");
    require(!dirty_count && !needs_present() && resync_needed,"prepatched capture waits for UI reconciliation without issuing uploads");
#ifndef NO_RUNTIME_LOG
    require(!readback_texture,"new captured texture releases previous diagnostic staging");
    bool initial_ok=false;for(const auto &e:events)if(!strcmp(e.name,"terrain_render_initial") && e.a==2 && !e.b && !e.c && e.thread!=ui_thread)initial_ok=true;
    require(initial_ok,"creation diagnostic reports two goals applied on load caller");
#endif
    // Same sequence as restore_sidecar: accepted list, mark each, then sync_render.
    // The new sidecar omits hex0 and changes hex22101 to poison marsh (material8).
    terrain_edit::Changes replacement;require(replacement.edit(true,22101,1,7,false)==terrain_edit::Result::Applied,"different sidecar fixture");
    unsigned failed=0;require(terrain_edit::changes.restore(replacement,terrain_edit::tile_value,failed)==1 && !failed,"restore replacement list");
    terrain_edit::changed_world=world(); // restore_sidecar also binds the completed world.
    for(unsigned i=0;i<terrain_edit::changes.size();++i){const auto &c=terrain_edit::changes[i];mark(c.hex,c.next,c.original);}
    terrain_edit::sync_render();
    require(dirty_count==2 && goals[0]==RestoreOriginal && !has_goal[0] && goals[22101]==8 && has_goal[22101],"full resync queues omitted old cell restoration and current sidecar target");
    on_present(context);
    require(gpu_pixel(device,context,second,0)==asset && gpu_pixel(device,context,second,22101)==0xff08ff00u,"GPU restores omitted cell from raw asset and applies changed sidecar");
    require(!needs_present(),"diagnostic readback does not extend dirty-only Present gate");
    require(terrain_edit::apply(22101,1)==terrain_edit::Result::Applied,"user original-terrain restore");on_present(context);
    require(gpu_pixel(device,context,second,22101)==asset,"user revert restores raw prepatch pixels rather than prepatched initial values");
#ifndef NO_RUNTIME_LOG
    require(count_event("terrain_render_readback")==3 && !count_event("terrain_render_readback_error"),"one real readback per uploading frame including original restoration");
    for(const auto &e:events)if(!strcmp(e.name,"terrain_render_readback"))require(e.b==e.c,"every observed GPU word matches its expected value");
#endif
    // Empty sidecar must reconcile every old cell, including a material-zero goal.
    mark(0,1,3);terrain_edit::before_world_change();
    ID3D11Texture2D *third=nullptr;require(SUCCEEDED(device->CreateTexture2D(&desc,&initial,&third)),"material-zero initial capture");
    require(gpu_pixel(device,context,third,0)==0xff00ff00u,"material zero is a real retained target, distinct from absence");
    terrain_edit::changes.clear();terrain_edit::sync_render();on_present(context);
    require(gpu_pixel(device,context,third,0)==asset && !has_goal[0],"empty sidecar restores retained material-zero goal");
    restore();require(!ctrl_texture && !needs_present(),"unload releases capture and pending work");
#ifndef NO_RUNTIME_LOG
    require(!readback_texture && !ctrl_create,"unload releases diagnostic staging and its saved factory");
#endif
    first->Release();second->Release();third->Release();context->Release();device->Release();
    printf("{\"terrain_render_r2b_checks\":%u,\"failed\":0,\"real_d3d11_warp\":true,\"gameplay_verified\":false}\n",checks);
}
