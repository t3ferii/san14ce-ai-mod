// User/Claude 2026-10-29: standalone fixture instrumentation declaration.
#ifndef COST_SCOPE
#define COST_SCOPE(name) do {} while(0)
#endif
// User 2026-10-29 R1b: early hardware probe, no window/swapchain, late fallback.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <initializer_list>
#include <vector>
static bool enabled=true;
static struct {bool convenience=true,terrain_edit=true;} mod_features;
static unsigned checks,loads,frees,creates,adds,releases,original_calls;
static void require(bool ok,const char *label) {++checks;if(!ok){fprintf(stderr,"FAIL %s (%u)\n",label,checks);ExitProcess(1);}}
struct Device {void **table;LONG references=1;};
static void *tables[2][6]={};
static Device probe_device,game,other;
static bool directory_ok=true,load_ok=true,export_ok=true,create_ok=true,return_device=true,site_ok=true,protect_ok=true;
static HMODULE system_library_handle=reinterpret_cast<HMODULE>(static_cast<uintptr_t>(0x1234));
static DWORD directory_size;
#ifndef NO_RUNTIME_LOG
struct Event {const char *name;unsigned value;};
static std::vector<Event> events;
static void log_event(const char *name,unsigned a,unsigned b,unsigned c) {require(b==0 && c==0,"early/table diagnostics keep zero trailing fields");events.push_back({name,a});}
#endif
static unsigned count_event(const char *name,unsigned value) {
#ifndef NO_RUNTIME_LOG
    unsigned n=0;for(const auto &e:events)if(!strcmp(name,e.name) && e.value==value)++n;return n;
#else
    (void)name;(void)value;return 0;
#endif
}
static ULONG STDMETHODCALLTYPE add_ref(ID3D11Device *dev) {++adds;return static_cast<ULONG>(++reinterpret_cast<Device *>(dev)->references);}
static ULONG STDMETHODCALLTYPE release(ID3D11Device *dev) {++releases;return static_cast<ULONG>(--reinterpret_cast<Device *>(dev)->references);}
static HRESULT STDMETHODCALLTYPE original_texture(ID3D11Device *dev,const D3D11_TEXTURE2D_DESC *,const D3D11_SUBRESOURCE_DATA *,ID3D11Texture2D **out) {
    require(dev==reinterpret_cast<ID3D11Device *>(&game),"released probe does not replace the real caller's this pointer");
    ++original_calls;if(out)*out=nullptr;return S_FALSE;
}
static bool terrain_render_fixture_site(void **table,void *original) {
    return site_ok && (table==tables[0] || table==tables[1]) && original==reinterpret_cast<void *>(&original_texture);
}
static UINT WINAPI system_directory(LPWSTR out,UINT capacity) {
    if(!directory_ok)return 0;
    if(directory_size)return directory_size;
    require(wcscpy_s(out,capacity,L"C:\\Windows\\System32")==0,"system directory copied");return 19;
}
static HMODULE WINAPI load_library(LPCWSTR path) {
    ++loads;require(!wcscmp(path,L"C:\\Windows\\System32\\d3d11.dll"),"only absolute system DLL loaded, never proxy basename");return load_ok?system_library_handle:nullptr;
}
static BOOL WINAPI free_library(HMODULE module) {++frees;require(module==system_library_handle,"only owned system library reference released");return TRUE;}
static HRESULT WINAPI create_device(IDXGIAdapter *adapter,D3D_DRIVER_TYPE type,HMODULE software,UINT flags,
    const D3D_FEATURE_LEVEL *levels,UINT count,UINT sdk,ID3D11Device **out,D3D_FEATURE_LEVEL *level,ID3D11DeviceContext **context) {
    ++creates;require(!adapter && type==D3D_DRIVER_TYPE_HARDWARE && !software && !flags && !levels && !count && sdk==D3D11_SDK_VERSION && out && !level && !context,
        "hardware-only device creation: no window, swapchain, requested context or extra flags");
    probe_device.references=1;*out=return_device?reinterpret_cast<ID3D11Device *>(&probe_device):nullptr;return create_ok?S_OK:E_FAIL;
}
static FARPROC WINAPI get_proc(HMODULE module,LPCSTR name) {
    require(module==system_library_handle && !strcmp(name,"D3D11CreateDevice"),"only system module's device creation export used");return export_ok?reinterpret_cast<FARPROC>(&create_device):nullptr;
}
static BOOL WINAPI protect(LPVOID address,SIZE_T size,DWORD protection,PDWORD prior) {return protect_ok?VirtualProtect(address,size,protection,prior):FALSE;}
#define TERRAIN_RENDER_FIXTURE
#define GetSystemDirectoryW system_directory
#define LoadLibraryW load_library
#define FreeLibrary free_library
#define GetProcAddress get_proc
#define VirtualProtect protect
#include "../src/native_terrain_render.inl"
#undef VirtualProtect
#undef GetProcAddress
#undef FreeLibrary
#undef LoadLibraryW
#undef GetSystemDirectoryW
static void reset() {
    using namespace terrain_render;
    protect_ok=true;restore();
    if(early_library)free_library(early_library);
    memset(slots,0,sizeof(slots));early_table=nullptr;early_library=nullptr;early_attempted=false;stopping=false;permitted=true;
#ifndef NO_RUNTIME_LOG
    reported_count=record_count=bc_count=0;saw_2049=false;events.clear();
#endif
    enabled=mod_features.convenience=mod_features.terrain_edit=true;
    directory_ok=load_ok=export_ok=create_ok=return_device=site_ok=protect_ok=true;directory_size=0;
    loads=frees=creates=adds=releases=original_calls=0;
    for(auto &table:tables) {table[1]=reinterpret_cast<void *>(&add_ref);table[2]=reinterpret_cast<void *>(&release);table[5]=reinterpret_cast<void *>(&original_texture);}
    probe_device={tables[0],1};game={tables[0],1};other={tables[1],1};
}
int main() {
    using namespace terrain_render;
    reset();
    for(unsigned off:{0u,1u,2u}) {
        enabled=off!=0;mod_features.convenience=off!=1;mod_features.terrain_edit=off!=2;install_early();
        require(!loads && !creates && !early_attempted,"disabled early observer does not load DLL or create device");
    }
    enabled=mod_features.convenience=mod_features.terrain_edit=true;install_early();
    require(loads==1 && creates==1 && !frees && early_library==system_library_handle,"successful probe retains one library reference");
    require(probe_device.references==0 && adds==0 && releases==1 && slots[0].keep==nullptr,"temporary device fully released, no slot AddRef retained");
    require(early_table==tables[0] && guards_match() && tables[0][5]==reinterpret_cast<void *>(&create_texture_2d),"slot and fingerprint survive destruction of probe");
#ifndef NO_RUNTIME_LOG
    require(count_event("terrain_texture_hook",2)==1 && !count_event("terrain_texture_hook",1),"early installation uses hook reason two only");
#endif
    install_early();require(loads==1 && creates==1 && releases==1,"early install attempted only once");
    ID3D11Texture2D *out=nullptr;
    require(create_texture_2d(reinterpret_cast<ID3D11Device *>(&game),nullptr,nullptr,&out)==S_FALSE && original_calls==1,"game texture creation works before any overlay/device observation");
    on_device(reinterpret_cast<ID3D11Device *>(&game));on_device(reinterpret_cast<ID3D11Device *>(&game));
    require(adds==0 && game.references==1 && !slots[0].keep,"same game vtable is not rehooked and acquires no probe reference");
#ifndef NO_RUNTIME_LOG
    require(count_event("terrain_texture_table",1)==1 && !count_event("terrain_texture_hook",1),"same game vtable logged once without duplicate installation");
#endif
    on_device(reinterpret_cast<ID3D11Device *>(&other));on_device(reinterpret_cast<ID3D11Device *>(&other));
    require(other.references==2 && adds==1 && slots[1].keep==reinterpret_cast<ID3D11Device *>(&other),"different game table receives independent normal installation");
#ifndef NO_RUNTIME_LOG
    require(count_event("terrain_texture_table",0)==1 && count_event("terrain_texture_hook",1)==1,"different table logs zero then normal hook once");
    size_t table_index=events.size(),hook_index=events.size();
    for(size_t i=0;i<events.size();++i) {
        if(!strcmp(events[i].name,"terrain_texture_table") && events[i].value==0)table_index=i;
        if(!strcmp(events[i].name,"terrain_texture_hook") && events[i].value==1)hook_index=i;
    }
    require(table_index<hook_index,"different-table diagnostic precedes its hook installation");
#endif
    restore();require(tables[0][5]==reinterpret_cast<void *>(&original_texture) && tables[1][5]==reinterpret_cast<void *>(&original_texture) && other.references==1 && probe_device.references==0,"rollback restores both tables without a second probe release");
    require(!frees && early_library==system_library_handle,"system module retained for in-flight original calls after restoration");
    for(unsigned failure:{0u,1u,2u,3u,4u,5u,6u,7u}) {
        reset();
        if(failure==0)directory_ok=false;
        if(failure==1)load_ok=false;
        if(failure==2)export_ok=false;
        if(failure==3)create_ok=false;
        if(failure==4)return_device=false;
        if(failure==5)site_ok=false;
        if(failure==6)protect_ok=false;
        if(failure==7)directory_size=MAX_PATH;
        install_early();
        require(!early_table && !early_library && permitted && tables[0][5]==reinterpret_cast<void *>(&original_texture),"probe failure publishes no slot and preserves late fallback");
        if(creates && return_device)require(probe_device.references==0 && adds==0 && releases==1,"even failed or rejected device is released exactly once");
        if(loads && load_ok)require(frees==1,"failed probe releases library reference");
        site_ok=protect_ok=true;
        on_device(reinterpret_cast<ID3D11Device *>(&game));
        require(game.references==2 && slots[0].keep==reinterpret_cast<ID3D11Device *>(&game) && tables[0][5]==reinterpret_cast<void *>(&create_texture_2d),"late game-device installation succeeds after early failure");
#ifndef NO_RUNTIME_LOG
        require(!count_event("terrain_texture_hook",2) && count_event("terrain_texture_hook",1)==1 && !count_event("terrain_texture_table",0) && !count_event("feature_guard_mismatch",28),"failure diagnostics do not falsely mark early install or block fallback");
#endif
    }
    reset();stopping=true;install_early();require(!loads && !creates,"rollback prevents later probe creation");
    printf("{\"terrain_render_early_checks\":%u,\"failed\":0,\"temporary_device_released\":true,\"late_fallback_verified\":true}\n",checks);
    return 0;
}
