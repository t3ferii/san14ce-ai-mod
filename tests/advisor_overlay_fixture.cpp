#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"windowscodecs.lib")
#include <algorithm>
#include <vector>
#include <cstdio>
#include <cstdlib>
#define COST_SCOPE(name) do {} while(0)
namespace unified_overlay {static int buffer_width=1920,buffer_height=1080;}
#ifndef NO_RUNTIME_LOG
static unsigned portrait_sample_logs,portrait_accept_logs;
static void log_event(const char *kind,unsigned,unsigned,unsigned accepted){if(strcmp(kind,"advisor_portrait_sample")==0){++portrait_sample_logs;portrait_accept_logs+=accepted;}}
#endif
#include "../src/native_advisor_overlay.inl"
static unsigned checks;
static void check(bool value,const char *why){++checks;if(!value){fprintf(stderr,"%s\n",why);exit(1);}}
static void bitmap(const char *path,const std::vector<unsigned> &p,int w,int h) {
    BITMAPFILEHEADER file{};file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+w*h*4;
    BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=w;info.biHeight=-h;info.biPlanes=1;info.biBitCount=32;
    FILE *f=nullptr;check(fopen_s(&f,path,"wb")==0,"renderer preview file opened");fwrite(&file,1,sizeof(file),f);fwrite(&info,1,sizeof(info),f);fwrite(p.data(),4,p.size(),f);fclose(f);
}
static void png(const wchar_t *path,const std::vector<unsigned> &pixels,UINT w,UINT h){
    IWICImagingFactory *factory=nullptr;IWICStream *stream=nullptr;IWICBitmapEncoder *encoder=nullptr;IWICBitmapFrameEncode *frame=nullptr;IPropertyBag2 *properties=nullptr;
    check(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))),"PNG encoder factory");
    check(SUCCEEDED(factory->CreateStream(&stream)) && SUCCEEDED(stream->InitializeFromFilename(path,GENERIC_WRITE)),"PNG preview output");
    check(SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder)) && SUCCEEDED(encoder->Initialize(stream,WICBitmapEncoderNoCache)),"PNG encoder initialized");
    check(SUCCEEDED(encoder->CreateNewFrame(&frame,&properties)) && SUCCEEDED(frame->Initialize(properties)) && SUCCEEDED(frame->SetSize(w,h)),"PNG frame sized");
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;check(SUCCEEDED(frame->SetPixelFormat(&format)) && IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA),"PNG retains exact BGRA pixels");
    check(SUCCEEDED(frame->WritePixels(h,w*4,w*h*4,reinterpret_cast<BYTE *>(const_cast<unsigned *>(pixels.data())))) && SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit()),"PNG saved");
    if(properties)properties->Release();frame->Release();encoder->Release();stream->Release();factory->Release();
}
// Isolate the production portrait quad so focus dim and shadow do not alter
// the original circular-mask expectation (Claude 2026-10-28).
static void portrait_mask_frame(ID3D11Device *device,ID3D11DeviceContext *context,ID3D11Texture2D *back){
    using namespace briefing_overlay;
    ID3D11RenderTargetView *target=nullptr;check(SUCCEEDED(device->CreateRenderTargetView(back,nullptr,&target)),"isolated portrait target");
    PipelineState saved(context);drawing_context=context;
    context->OMSetRenderTargets(1,&target,nullptr);context->OMSetBlendState(blend,nullptr,~0u);context->OMSetDepthStencilState(depth,0);
    context->RSSetState(raster_state);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);context->IASetInputLayout(nullptr);
    context->VSSetShader(vs,nullptr,0);context->PSSetShader(ps,nullptr,0);context->PSSetSamplers(0,1,&sampler);context->PSSetConstantBuffers(0,1,&constants);
    quad(head_rect(head),1,0,0,44,1,portrait_view);drawing_context=nullptr;target->Release();
}
struct FakeDevice {void **table;DWORD type;bool info_ok=true;unsigned info_calls=0;};
static HRESULT STDMETHODCALLTYPE fake_info(IDirectInputDevice8W *p,LPDIDEVCAPS info){auto *f=reinterpret_cast<FakeDevice *>(p);++f->info_calls;if(!f->info_ok || info->dwSize!=sizeof(DIDEVCAPS))return E_FAIL;info->dwDevType=f->type;return S_OK;}
static HRESULT STDMETHODCALLTYPE fake_state(IDirectInputDevice8W *,DWORD,LPVOID){return S_OK;}
static HRESULT STDMETHODCALLTYPE fake_data(IDirectInputDevice8W *,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD){return DI_BUFFEROVERFLOW;}
static HRESULT STDMETHODCALLTYPE fake_state_other(IDirectInputDevice8W *,DWORD,LPVOID){return S_FALSE;}
static unsigned steam_state_calls,steam_data_calls;
static DWORD steam_state_bytes,steam_data_bytes,steam_data_flags;
static HRESULT STDMETHODCALLTYPE steam_state(IDirectInputDevice8W *,DWORD bytes,LPVOID output){
    ++steam_state_calls;steam_state_bytes=bytes;
    auto *state=static_cast<DIMOUSESTATE2 *>(output);state->lX=37;state->lY=-19;state->lZ=120;state->rgbButtons[0]=0x80;state->rgbButtons[1]=0x81;
    return S_FALSE;
}
static HRESULT STDMETHODCALLTYPE steam_data(IDirectInputDevice8W *,DWORD bytes,LPDIDEVICEOBJECTDATA output,LPDWORD count,DWORD flags){
    ++steam_data_calls;steam_data_bytes=bytes;steam_data_flags=flags;
    if(*count<3)return E_FAIL;*count=3;
    output[0].dwOfs=DIMOFS_Z;output[1].dwOfs=DIMOFS_BUTTON0;output[2].dwOfs=DIMOFS_X;
    for(unsigned i=0;i<3;++i){output[i].dwData=120;output[i].dwTimeStamp=43;output[i].dwSequence=71;}
    return DI_BUFFEROVERFLOW;
}
static HRESULT STDMETHODCALLTYPE later_data(IDirectInputDevice8W *,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD){return S_FALSE;}
static BOOL WINAPI failed_mouse_protect(LPVOID,SIZE_T,DWORD,PDWORD){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
static void *fake_input_table[4]{};static void **fake_input_vtable=fake_input_table;
static FakeDevice created_mouse[2]{};
static unsigned create_mode[2]{},create_calls[2]{},released_com;
static ULONG STDMETHODCALLTYPE fake_com_release(IUnknown *){++released_com;return 1;}
static HRESULT STDMETHODCALLTYPE fake_create_device(void *,REFGUID,void **out,IUnknown *) {
    unsigned kind=create_calls[0]>create_calls[1]?0u:1u;
    *out=create_mode[kind]==6?nullptr:&created_mouse[kind];
    if(create_mode[kind]==5){*out=nullptr;return DIERR_DEVICENOTREG;}
    return S_OK;
}
static HRESULT WINAPI fake_input_create(HINSTANCE,DWORD version,REFIID iid,void **out,IUnknown *) {
    unsigned kind=iid.Data1==0xBF798030?0u:1u;++create_calls[kind];
    check(version==0x0800 && (iid.Data1==0xBF798030 || iid.Data1==0xBF798031),"explicit DI8 version and both real interface IIDs");
    *out=create_mode[kind]==4?nullptr:&fake_input_vtable;
    if(create_mode[kind]==3){*out=nullptr;return E_NOINTERFACE;}
    return S_OK;
}
static void di_stage(unsigned kind,briefing_overlay::DiStage stage){
#ifndef NO_RUNTIME_LOG
    check(briefing_overlay::di_install_details[kind-1][0]==stage,"exact first installation failure/success stage");
#else
    (void)kind;(void)stage;
#endif
}
// Synthetic provider ownership only for fixture functions; other addresses use Windows.
static bool fake_owner_path(void *address,wchar_t (&path)[MAX_PATH]) {
    if(address==reinterpret_cast<void *>(&steam_state) || address==reinterpret_cast<void *>(&steam_data))return briefing_overlay::steam_overlay_path(path);
    if(address!=reinterpret_cast<void *>(&fake_state) && address!=reinterpret_cast<void *>(&fake_state_other) && address!=reinterpret_cast<void *>(&fake_data))return briefing_overlay::mouse_owner_path(address,path);
    if(!GetSystemDirectoryW(path,MAX_PATH))return false;wcscat_s(path,L"\\dinput8.dll");return true;
}
static void directinput_checks(){
    using namespace briefing_overlay;
    void *table[33]{};table[9]=reinterpret_cast<void *>(&fake_state);table[10]=reinterpret_cast<void *>(&fake_data);table[3]=reinterpret_cast<void *>(&fake_info);table[2]=reinterpret_cast<void *>(&fake_com_release);
    auto module=GetModuleHandleW(nullptr);
    const wchar_t *sys=L"C:\\Windows\\System32",*game=L"D:\\Steam\\SAN14\\SAN14CE_KO.exe";
    check(allowed_mouse_path(L"c:\\windows\\system32\\DINPUT8.DLL",sys,game),"2026-10-28: exact system dinput8 path allowed, case insensitive");
    check(allowed_mouse_path(L"D:\\Steam\\SAN14\\SAN14_AI_PreviousInput.dll",sys,game),"game directory PreviousInput chain allowed");
    // 사용자 2026-10-28 local22: a basename alone does not establish Steam ownership.
    const wchar_t *trusted_steam=L"C:\\Program Files (x86)\\Steam\\gameoverlayrenderer64.dll";
    check(allowed_mouse_path(L"c:\\program files (x86)\\steam\\GAMEOVERLAYRENDERER64.DLL",sys,game,trusted_steam),"Steam installation's exact overlay path allowed case insensitively");
    check(!allowed_mouse_path(trusted_steam,sys,game),"Steam owner rejected without an independently resolved installation path");
    for(const wchar_t *path:{L"D:\\Steam\\SAN14\\gameoverlayrenderer64.dll",L"C:\\Other\\gameoverlayrenderer64.dll",L"C:\\Program Files (x86)\\Steam\\bin\\gameoverlayrenderer64.dll",L"C:\\Program Files (x86)\\Steam\\gameoverlayrenderer.dll"})
        check(!allowed_mouse_path(path,sys,game,trusted_steam),"same-name foreign overlay, subdirectory and 32-bit overlay stay rejected");
    wchar_t actual_steam[MAX_PATH]{};check(steam_overlay_path(actual_steam),"registered Steam installation has an existing overlay file; no overlay code executed");
    DWORD steam_attributes=GetFileAttributesW(actual_steam);
    check(steam_attributes!=INVALID_FILE_ATTRIBUTES && !(steam_attributes&FILE_ATTRIBUTE_DIRECTORY),"trusted Steam overlay is an actual file");
    for(const wchar_t *path:{L"D:\\Steam\\SAN14\\dinput8.dll",L"D:\\Steam\\SAN14\\SAN14_Convenience.dll",L"D:\\Steam\\SAN14\\SAN14_CombatAI.dll",L"C:\\Other\\dinput8.dll",L"C:\\Windows\\System32\\dinput.dll",L"C:\\Other\\SAN14_AI_PreviousInput.dll"})
        check(!allowed_mouse_path(path,sys,game),"same-name loader, own DLLs and unrelated modules rejected");
    wchar_t slot_owner[MAX_PATH]{};check(mouse_owner_path(table[9],slot_owner) && *slot_owner,"FROM_ADDRESS records actual fixture module path");
    check(!install_mouse_table(table) && table[9]==reinterpret_cast<void *>(&fake_state),"default resolver rejects fixture executable");
    void *bad[33]{};bad[9]=reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"GetTickCount"));bad[10]=table[10];
    check(!install_mouse_table(bad,2,&VirtualProtect,&fake_owner_path) && bad[9]!=reinterpret_cast<void *>(&filtered_device_state) && !mouse_slots,"foreign-module originals leave both slots unchanged");
    di_stage(2,di_range_failed);
    check(!install_mouse_table(table,2,&failed_mouse_protect,&fake_owner_path) && table[9]==reinterpret_cast<void *>(&fake_state),"VirtualProtect failure publishes neither slot");di_stage(2,di_protect_failed);
#ifndef NO_RUNTIME_LOG
    check(di_install_details[1][4]==ERROR_ACCESS_DENIED && di_install_details[1][3]==3,"range mask and Win32 protect error retained");
#endif
    check(install_mouse_table(table,2,&VirtualProtect,&fake_owner_path),"both validated provider slots install together");
    FakeDevice mouse{table,DI8DEVTYPE_MOUSE},keyboard{table,DI8DEVTYPE_KEYBOARD},unknown{table,DI8DEVTYPE_MOUSE,false};
    auto get=reinterpret_cast<DeviceState>(table[9]);auto data=reinterpret_cast<DeviceData>(table[10]);
    DIMOUSESTATE2 state{};state.lX=11;state.lY=-12;state.lZ=120;state.rgbButtons[0]=0x80;state.rgbButtons[1]=0x81;
    wheel_block=0;block_buttons=0;get(reinterpret_cast<IDirectInputDevice8W *>(&mouse),sizeof(state),&state);
    check(state.lZ==120 && state.rgbButtons[0]==0x80,"unblocked DirectInput returns original state");
    wheel_block=1;get(reinterpret_cast<IDirectInputDevice8W *>(&mouse),sizeof(state),&state);
    check(!state.lZ && state.rgbButtons[0]==0x80 && state.lX==11 && state.lY==-12,"wheel block clears only mouse lZ");
    block_buttons=1;state.lZ=120;get(reinterpret_cast<IDirectInputDevice8W *>(&mouse),sizeof(DIMOUSESTATE),&state);
    check(!state.lZ && !state.rgbButtons[0] && state.rgbButtons[1]==0x81 && mouse.info_calls==1,"both mouse sizes filter button0 only and cache identity");
    state.lZ=120;state.rgbButtons[0]=0x80;get(reinterpret_cast<IDirectInputDevice8W *>(&keyboard),sizeof(state),&state);
    check(state.lZ==120 && state.rgbButtons[0]==0x80,"shared class vtable never changes keyboard state");
    get(reinterpret_cast<IDirectInputDevice8W *>(&unknown),sizeof(state),&state);check(state.lZ==120,"unknown identification preserves original state");
    DIDEVICEOBJECTDATA events[4]{};events[0].dwOfs=DIMOFS_Z;events[1].dwOfs=DIMOFS_BUTTON0;events[2].dwOfs=DIMOFS_BUTTON1;events[3].dwOfs=DIMOFS_X;
    for(auto &e:events){e.dwData=120;e.dwSequence=7;e.dwTimeStamp=9;}DWORD count=4;
    check(data(reinterpret_cast<IDirectInputDevice8W *>(&mouse),sizeof(events[0]),events,&count,DIGDD_PEEK)==DI_BUFFEROVERFLOW && !events[0].dwData && !events[1].dwData && events[2].dwData==120 && events[3].dwData==120 && count==4 && events[0].dwSequence==7,"buffered wheel/button0 filter preserves HRESULT, count, PEEK and other fields");
    wheel_block=block_buttons=0;events[0].dwData=events[1].dwData=120;data(reinterpret_cast<IDirectInputDevice8W *>(&mouse),sizeof(events[0]),events,&count,0);
    check(events[0].dwData==120 && events[1].dwData==120,"unblocked buffered data unchanged");
    wheel_block=block_buttons=1;data(reinterpret_cast<IDirectInputDevice8W *>(&keyboard),sizeof(events[0]),events,&count,0);
    check(events[0].dwData==120 && events[1].dwData==120,"keyboard buffered data unchanged");
    restore_mouse_filter();check(table[9]==reinterpret_cast<void *>(&fake_state) && table[10]==reinterpret_cast<void *>(&fake_data) && !wheel_block && !block_buttons,"cleanup restores originals and clears flags");mouse_identity_count=0;
    table[9]=reinterpret_cast<void *>(&steam_state);table[10]=reinterpret_cast<void *>(&steam_data);
    check(install_mouse_table(table,2,&VirtualProtect,&fake_owner_path) && mouse_tables[0].state==&steam_state && mouse_tables[0].data==&steam_data,"Steam-owned slots accepted and current pointers retained as chain targets");
#ifndef NO_RUNTIME_LOG
    check(!_wcsicmp(di_owner_paths[1][0],actual_steam) && !_wcsicmp(di_owner_paths[1][1],actual_steam),"both diagnostic chain targets retain actual Steam module path");
#endif
    state={};wheel_block=block_buttons=1;
    check(filtered_device_state(reinterpret_cast<IDirectInputDevice8W *>(&mouse),sizeof(state),&state)==S_FALSE && steam_state_calls==1 && steam_state_bytes==sizeof(state),"GetDeviceState calls saved Steam wrapper once with original ABI and HRESULT");
    check(!state.lZ && !state.rgbButtons[0] && state.lX==37 && state.lY==-19 && state.rgbButtons[1]==0x81,"filter changes Steam-returned wheel/button only, retaining movement and other buttons");
    count=4;
    check(filtered_device_data(reinterpret_cast<IDirectInputDevice8W *>(&mouse),sizeof(events[0]),events,&count,DIGDD_PEEK)==DI_BUFFEROVERFLOW && steam_data_calls==1 && steam_data_bytes==sizeof(events[0]) && steam_data_flags==DIGDD_PEEK,"GetDeviceData calls Steam once with original byte size, PEEK flags and HRESULT");
    check(count==3 && !events[0].dwData && !events[1].dwData && events[2].dwData==120 && events[0].dwTimeStamp==43 && events[0].dwSequence==71,"buffer filter preserves Steam count, metadata and other events");
    restore_mouse_filter();check(table[9]==reinterpret_cast<void *>(&steam_state) && table[10]==reinterpret_cast<void *>(&steam_data),"normal removal restores Steam chain instead of bypassing it to a system export");
    for(unsigned changed:{1u,2u,3u}){
        check(install_mouse_table(table,2,&VirtualProtect,&fake_owner_path),"Steam chain reinstalled for per-slot competing-owner test");
        if(changed&1)table[9]=reinterpret_cast<void *>(&fake_state_other);
        if(changed&2)table[10]=reinterpret_cast<void *>(&later_data);
#ifndef NO_RUNTIME_LOG
        auto restored_before=di_restore_counts[0],changed_before=di_restore_counts[1];
#endif
        restore_mouse_filter();
        check(table[9]==reinterpret_cast<void *>(changed&1?&fake_state_other:&steam_state) && table[10]==reinterpret_cast<void *>(changed&2?&later_data:&steam_data),"removal detects overwritten slots independently and leaves every newer owner unchanged");
#ifndef NO_RUNTIME_LOG
        unsigned foreign=(changed&1?1u:0u)+(changed&2?1u:0u);
        check(di_restore_counts[0]==restored_before+2-foreign && di_restore_counts[1]==changed_before+foreign,"diagnostics distinguish restored slots and skipped competing owners");
#endif
        table[9]=reinterpret_cast<void *>(&steam_state);table[10]=reinterpret_cast<void *>(&steam_data);
    }
    table[9]=reinterpret_cast<void *>(&fake_state);table[10]=reinterpret_cast<void *>(&fake_data);
    // 사용자 2026-10-28 설계 전환 3단계: A/W and every installation gate.
    install_mouse_provider(nullptr,nullptr);di_stage(1,di_no_proxy);di_stage(2,di_no_proxy);
    install_mouse_provider(module,nullptr);di_stage(1,di_no_export);di_stage(2,di_no_export);
    fake_input_table[2]=reinterpret_cast<void *>(&fake_com_release);fake_input_table[3]=reinterpret_cast<void *>(&fake_create_device);
    void *other[33]{};memcpy(other,table,sizeof(table));other[9]=reinterpret_cast<void *>(&fake_state_other);
    created_mouse[0]={table,DI8DEVTYPE_MOUSE};created_mouse[1]={other,DI8DEVTYPE_MOUSE};
    for(unsigned failure:{3u,4u,5u,6u}) {
        create_calls[0]=create_calls[1]=0;create_mode[0]=create_mode[1]=failure;
        install_mouse_provider(module,&fake_input_create,&fake_owner_path);di_stage(1,static_cast<DiStage>(failure));di_stage(2,static_cast<DiStage>(failure));
        check(!mouse_slots,"failed A/W create never publishes a device wrapper");
#ifndef NO_RUNTIME_LOG
        if(failure==3)check(di_install_details[0][1]==static_cast<unsigned long>(E_NOINTERFACE) && di_install_details[1][1]==static_cast<unsigned long>(E_NOINTERFACE),"A/W DirectInput8Create HRESULTs retained exactly");
        if(failure==5)check(di_install_details[0][2]==static_cast<unsigned long>(DIERR_DEVICENOTREG) && di_install_details[1][2]==static_cast<unsigned long>(DIERR_DEVICENOTREG),"A/W CreateDevice HRESULTs retained exactly");
#endif
    }
    create_calls[0]=create_calls[1]=0;create_mode[0]=3;create_mode[1]=0;
    install_mouse_provider(module,&fake_input_create,&fake_owner_path);di_stage(1,di_create_failed);di_stage(2,di_installed);
    check(create_calls[0]==1 && create_calls[1]==1 && mouse_interface_success==2,"A failure still attempts W, retaining partial success");restore_mouse_filter();
    create_calls[0]=create_calls[1]=0;create_mode[0]=create_mode[1]=0;
    install_mouse_provider(module,&fake_input_create,&fake_owner_path);di_stage(1,di_installed);di_stage(2,di_installed);
    check(mouse_interface_success==3 && mouse_tables[0].device && mouse_tables[1].device,"distinct A/W tables retain independent originals and COM lifetimes");
    void *third[33]{};third[9]=reinterpret_cast<void *>(&fake_state);third[10]=reinterpret_cast<void *>(&fake_data);
    check(!install_mouse_table(third,1,&VirtualProtect,&fake_owner_path) && third[9]==reinterpret_cast<void *>(&fake_state),"bounded table registry cannot overwrite either installed interface");di_stage(1,di_table_full);
    state.lZ=120;wheel_block=1;
    check(filtered_device_state(reinterpret_cast<IDirectInputDevice8W *>(&created_mouse[0]),sizeof(state),&state)==S_OK && !state.lZ,"A device filters via its own original ABI");
    state.lZ=120;check(filtered_device_state(reinterpret_cast<IDirectInputDevice8W *>(&created_mouse[1]),sizeof(state),&state)==S_FALSE && !state.lZ,"W device dispatches its different original");
    unsigned releases=released_com;restore_mouse_filter();check(released_com==releases+4 && table[9]==reinterpret_cast<void *>(&fake_state) && other[9]==reinterpret_cast<void *>(&fake_state_other),"both tables restore before releasing all four owned COM references");
    created_mouse[1].table=table;create_calls[0]=create_calls[1]=0;
    install_mouse_provider(module,&fake_input_create,&fake_owner_path);
    check(mouse_interface_success==3 && mouse_tables[0].table==table && !mouse_tables[1].table && mouse_tables[0].state==&fake_state,"shared A/W vtable patches once and never saves our wrapper as original");restore_mouse_filter();
    // Native OS provider only, in this fixture process; never loads the game/proxy.
    wchar_t system_path[MAX_PATH]{};check(GetSystemDirectoryW(system_path,MAX_PATH)!=0,"system provider directory");wcscat_s(system_path,L"\\dinput8.dll");
    HMODULE system=LoadLibraryExW(system_path,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    check(system!=nullptr,"system DirectInput loads in isolated fixture");
    auto create=reinterpret_cast<InputCreate>(GetProcAddress(system,"DirectInput8Create"));
    check(create!=nullptr,"native system DirectInput8Create exists");install_mouse_provider(system,create);
    check(mouse_interface_success==3,"native system A/W create, mouse device, module ranges and publication all pass");di_stage(1,di_installed);di_stage(2,di_installed);
    restore_mouse_filter();FreeLibrary(system);
    mouse_attempts=0;mouse_retry_at=0;
    check(begin_mouse_attempt(500) && mouse_attempts==1,"first installation attempt runs once");
    check(!begin_mouse_attempt(1499) && mouse_attempts==1,"failed provider does not retry COM every Present");
    check(begin_mouse_attempt(1500) && begin_mouse_attempt(2500) && !begin_mouse_attempt(3500) && mouse_attempts==3,"failed startup retries after one second and stops at three attempts");
    mouse_attempts=0;mouse_retry_at=0;mouse_interface_success=3;
    check(!begin_mouse_attempt(4500) && !mouse_attempts,"both installed interfaces prevent further COM work");
    mouse_interface_success=0;mouse_attempts=3;
}
static void pipeline_equal(const briefing_overlay::PipelineState &a,const briefing_overlay::PipelineState &b){
    check(!memcmp(a.targets,b.targets,sizeof(a.targets)) && a.dsv==b.dsv,"all eight render targets and depth view restored");
    check(a.blend_state==b.blend_state && a.mask==b.mask && !memcmp(a.factors,b.factors,sizeof(a.factors)),"blend state, factors and sample mask restored");
    check(a.depth_state==b.depth_state && a.stencil==b.stencil,"depth/stencil state and reference restored");
    check(a.rs==b.rs && a.viewport_count==b.viewport_count && !memcmp(a.viewports,b.viewports,sizeof(a.viewports)) && a.scissor_count==b.scissor_count && !memcmp(a.scissors,b.scissors,sizeof(a.scissors)),"rasterizer, all viewports and scissors restored");
    check(a.layout==b.layout && a.topology==b.topology,"input layout and topology restored");
    check(a.vertex==b.vertex && a.pixel==b.pixel && a.vn==b.vn && a.pn==b.pn && !memcmp(a.vc,b.vc,sizeof(a.vc)) && !memcmp(a.pc,b.pc,sizeof(a.pc)),"Claude 2026-10-28: 인터페이스 없는 셰이더의 클래스 인스턴스는 0이 정상; VS/PS restore actual counts and pointers");
    check(a.geometry==b.geometry && a.hull==b.hull && a.domain==b.domain && a.gn==b.gn && a.hn==b.hn && a.dn==b.dn,"GS/HS/DS bindings restored");
    check(a.vbuffer==b.vbuffer && a.pbuffer==b.pbuffer && a.resource==b.resource && a.sample==b.sample,"VS/PS buffer0, PS SRV0 and sampler0 restored");
}
int main() {
    using namespace briefing_overlay;
    directinput_checks();
    check(SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)),"fixture COM for PNG export");
    ID3D11Device *d=nullptr;ID3D11DeviceContext *c=nullptr;D3D_FEATURE_LEVEL level;
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&level,&c)),"offscreen WARP device, no game or window");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=1920;desc.Height=1080;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D *back=nullptr;check(SUCCEEDED(d->CreateTexture2D(&desc,nullptr,&back)),"synthetic back buffer created");
    std::vector<unsigned> pixels(1920*1080,0xff63877cu);
    // User 2026-10-26: the real backbuffer can have useful RGB and zero alpha.
    for(unsigned y=10;y<98;++y)for(unsigned x=10;x<98;++x)pixels[y*1920+x]=0x00d5ba84u;
    // A clearly synthetic portrait stand-in. The shipped mod captures the
    // original adviser face node; this fixture never loads or runs the game.
    for(unsigned y=24;y<62;++y)for(unsigned x=36;x<72;++x)pixels[y*1920+x]=0x0034485fu;
    for(unsigned y=100;y<188;++y)for(unsigned x=10;x<98;++x)pixels[y*1920+x]=0;
    auto scene=pixels;
    c->UpdateSubresource(back,0,nullptr,pixels.data(),1920*4,0);
    ID3D11RenderTargetView *original_target=nullptr;check(SUCCEEDED(d->CreateRenderTargetView(back,nullptr,&original_target)),"fixture render target");c->OMSetRenderTargets(1,&original_target,nullptr);
    D3D11_VIEWPORT original_viewport{5,7,733,511,0,1};c->RSSetViewports(1,&original_viewport);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
    Model m;m.adviser=L"제갈량";m.title=L"뜻밖의 수확 · 3건";m.body=L"발견한 금 합계 1,000\n\n• 유비  ·  200\n• 관우  ·  300\n• 조운  ·  500";m.unread=7;m.open=true;m.generation=1;m.pages=4;
    m.names={L"유비",L"관우",L"조운"};
    auto frame=[&](){c->UpdateSubresource(back,0,nullptr,scene.data(),1920*4,0);draw(d,c,back,desc);};
    progress_flags(true,true);publish(m,true);opened_at=GetTickCount64()-300;
    request_portrait({-146,708,484,512},1);frame();
    check(available() && !captured(1),"invalid or unavailable original portrait cannot block the entire inbox renderer");
    check(avatar_view!=nullptr && panel_view!=nullptr,"visible fallback avatar and bubble exist before native portrait capture");
    portrait_attempt_at=0;request_portrait({10,100,88,88},1);frame();
    check(!captured(1) && avatar_view && flag(&portrait_requested),"black source is rejected and fallback remains visible for retry");
    for(int attempt=0;attempt<30 && portrait_staging;++attempt){c->Flush();Sleep(2);frame();}
    auto maps_before=portrait_maps;portrait_attempt_at=0;request_portrait({10,10,88,88},1);
    frame();check(!captured(1) && portrait_maps==maps_before,"portrait copy never maps on the same Present");
    auto finish_candidate=[&](){for(int attempt=0;attempt<60 && portrait_staging;++attempt){c->Flush();Sleep(2);frame();}};
    finish_candidate();
    check(!captured(1) && portrait_confirmation.present && flag(&portrait_requested) && avatar_view,"first valid capture keeps fallback and portrait request");
    // User 2026-10-30: keep the first source valid but replace it with a different face.
    const auto face_scene=scene;
    for(unsigned y=10;y<98;++y)for(unsigned x=10;x<98;++x)scene[y*1920+x]^=0x00a07090u;
    portrait_confirmation.at=GetTickCount64()-500;portrait_attempt_at=0;frame();finish_candidate();
    check(!captured(1) && flag(&portrait_requested) && avatar_view,"different second valid source cannot freeze the first terrain capture");
    scene=face_scene;
    portrait_confirmation.at=GetTickCount64()-500;portrait_attempt_at=0;frame();finish_candidate();
    check(!captured(1) && flag(&portrait_requested),"changed third source replaces the pending sample");
    auto sample_at=portrait_confirmation.at;auto map_count=portrait_maps;portrait_attempt_at=0;frame();
    check(!portrait_staging && portrait_maps==map_count && portrait_confirmation.at==sample_at,"no second GPU capture or comparison before 500ms");
    portrait_confirmation.at=GetTickCount64()-500;portrait_attempt_at=0;frame();finish_candidate();
    check(available() && captured(1),"original source texture captured and overlay draw completed");
#ifndef NO_RUNTIME_LOG
    check(portrait_sample_logs>=4 && portrait_accept_logs==1,"minimal sample diagnostics mark only the confirmed capture accepted");
#endif
    auto *good_face=portrait_view;portrait_attempt_at=0;request_portrait({10,100,88,88},2);frame();
    check(!captured(2) && portrait_view==good_face,"bad replacement preserves the last validated portrait");
    check(!portrait_confirmation.present,"revision change clears pending CPU sample");finish_candidate();
    portrait_attempt_at=0;request_portrait({10,10,88,88},2);frame();finish_candidate();
    check(portrait_confirmation.present && !captured(2) && portrait_view==good_face,"new revision needs two samples while preserving previous face");
    request_portrait({13,10,88,88},2);check(!portrait_confirmation.present,"source rect change clears CPU sample");
    {
        // Claude 2026-10-28: 인터페이스 없는 셰이더의 클래스 인스턴스는 0이 정상.
        // Compare the actual GetShader counts/pointers before and after drawing.
        HMODULE compiler=LoadLibraryW(L"d3dcompiler_47.dll");check(compiler!=nullptr,"fixture shader compiler available");
        auto compile=reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(compiler,"D3DCompile"));check(compile!=nullptr,"fixture compiler export");
        const char *hlsl=R"hlsl(
            struct V {float4 pos:SV_POSITION;float2 uv:TEXCOORD0;};
            V seeded_vs(float4 pos:POSITION){V v;v.pos=pos;v.uv=float2(0,0);return v;}
            float4 seeded_ps(V v):SV_TARGET{return float4(v.uv,0,1);}
        )hlsl";
        ID3DBlob *vblob=nullptr,*pblob=nullptr,*errors=nullptr;
        HRESULT hr=compile(hlsl,strlen(hlsl),nullptr,nullptr,nullptr,"seeded_vs","vs_5_0",D3DCOMPILE_SKIP_OPTIMIZATION,0,&vblob,&errors);if(FAILED(hr) && errors)fprintf(stderr,"%s",static_cast<const char *>(errors->GetBufferPointer()));drop(errors);check(SUCCEEDED(hr),"fixture vertex shader compiles");
        hr=compile(hlsl,strlen(hlsl),nullptr,nullptr,nullptr,"seeded_ps","ps_5_0",D3DCOMPILE_SKIP_OPTIMIZATION,0,&pblob,&errors);if(FAILED(hr) && errors)fprintf(stderr,"%s",static_cast<const char *>(errors->GetBufferPointer()));drop(errors);check(SUCCEEDED(hr),"fixture pixel shader compiles");
        ID3D11VertexShader *seed_vs=nullptr;ID3D11PixelShader *seed_ps=nullptr;ID3D11InputLayout *seed_layout=nullptr;
        check(SUCCEEDED(d->CreateVertexShader(vblob->GetBufferPointer(),vblob->GetBufferSize(),nullptr,&seed_vs)) && SUCCEEDED(d->CreatePixelShader(pblob->GetBufferPointer(),pblob->GetBufferSize(),nullptr,&seed_ps)),"distinct VS/PS without dynamic interfaces");
        D3D11_INPUT_ELEMENT_DESC element{"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0};
        check(SUCCEEDED(d->CreateInputLayout(&element,1,vblob->GetBufferPointer(),vblob->GetBufferSize(),&seed_layout)),"nonnull fixture input layout");
        ID3D11Texture2D *depth_tex=nullptr,*extra_tex=nullptr;ID3D11DepthStencilView *depth_view=nullptr;ID3D11RenderTargetView *extra_target=nullptr;
        auto depth_desc=desc;depth_desc.Format=DXGI_FORMAT_D32_FLOAT;depth_desc.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        check(SUCCEEDED(d->CreateTexture2D(&depth_desc,nullptr,&depth_tex)) && SUCCEEDED(d->CreateDepthStencilView(depth_tex,nullptr,&depth_view)) && SUCCEEDED(d->CreateTexture2D(&desc,nullptr,&extra_tex)) && SUCCEEDED(d->CreateRenderTargetView(extra_tex,nullptr,&extra_target)),"nonnull depth view and second render target");
        ID3D11Buffer *seed_buffer=nullptr;D3D11_BUFFER_DESC buffer_desc{};buffer_desc.ByteWidth=16;buffer_desc.Usage=D3D11_USAGE_DEFAULT;buffer_desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        check(SUCCEEDED(d->CreateBuffer(&buffer_desc,nullptr,&seed_buffer)),"distinct original constant buffer");
        ID3D11RenderTargetView *targets[2]{original_target,extra_target};c->OMSetRenderTargets(2,targets,depth_view);
        D3D11_VIEWPORT views[2]{original_viewport,{12,13,40,50,.2f,.8f}};c->RSSetViewports(2,views);
        D3D11_RECT rects[2]{{1,2,33,44},{10,20,300,400}};c->RSSetScissorRects(2,rects);
        FLOAT factors[4]{.1f,.2f,.3f,.4f};c->OMSetBlendState(blend,factors,0x76543210);c->OMSetDepthStencilState(depth,17);c->RSSetState(scissor_state);
        c->IASetInputLayout(seed_layout);c->VSSetShader(seed_vs,nullptr,0);c->PSSetShader(seed_ps,nullptr,0);c->VSSetConstantBuffers(0,1,&seed_buffer);c->PSSetConstantBuffers(0,1,&seed_buffer);c->PSSetShaderResources(0,1,&avatar_view);c->PSSetSamplers(0,1,&sampler);
        {PipelineState before_state(c);frame();PipelineState after_state(c);pipeline_equal(before_state,after_state);}
        drop(seed_buffer);drop(depth_view);drop(depth_tex);drop(extra_target);drop(extra_tex);drop(seed_layout);drop(seed_vs);drop(seed_ps);drop(vblob);drop(pblob);FreeLibrary(compiler);
    }
    c->OMSetRenderTargets(1,&original_target,nullptr);c->RSSetViewports(1,&original_viewport);
    D3D11_PRIMITIVE_TOPOLOGY topology;c->IAGetPrimitiveTopology(&topology);check(topology==D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,"immediate primitive topology preserved");
    UINT count=1;D3D11_VIEWPORT viewport{};c->RSGetViewports(&count,&viewport);check(count==1 && viewport.Width==733 && viewport.TopLeftX==5,"immediate viewport preserved");
    ID3D11RenderTargetView *after=nullptr;c->OMGetRenderTargets(1,&after,nullptr);check(after==original_target,"immediate render target preserved");if(after)after->Release();
    D3D11_TEXTURE2D_DESC staging=desc;staging.BindFlags=0;staging.Usage=D3D11_USAGE_STAGING;staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D *readback=nullptr;check(SUCCEEDED(d->CreateTexture2D(&staging,nullptr,&readback)),"readback buffer created");c->CopyResource(readback,back);
    D3D11_MAPPED_SUBRESOURCE mapped{};check(SUCCEEDED(c->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"overlay pixels mapped");
    for(unsigned y=0;y<1080;++y)memcpy(pixels.data()+y*1920,static_cast<unsigned char *>(mapped.pData)+y*mapped.RowPitch,1920*4);c->Unmap(readback,0);
    const auto composited_pixels=pixels;
    c->UpdateSubresource(back,0,nullptr,scene.data(),1920*4,0);portrait_mask_frame(d,c,back);
    c->CopyResource(readback,back);check(SUCCEEDED(c->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"isolated circular portrait mapped");
    for(unsigned y=0;y<1080;++y)memcpy(pixels.data()+y*1920,static_cast<unsigned char *>(mapped.pData)+y*mapped.RowPitch,1920*4);c->Unmap(readback,0);
    check(pixels[956*1920+30]==0xff63877cu,"circular portrait corner preserves the underlying map");
    check((pixels[1000*1920+74]&0xffffffu)==0x34485fu,"isolated circular portrait retains the original RGB");
    pixels=composited_pixels;
    check((pixels[1000*1920+74]&0xffffffu)==0x34485fu,"portrait RGB stays visible even when captured source alpha is zero");
    check((pixels[520*1920+130]&0xffffffu)==0x516f66u,"사용자 결정 2026-10-28 드래그 헤드: outside the bubble shadow has exactly the 18 percent focus dim");
    check((pixels[800*1920+152]&0xffffffu)<0x303030u,"ink background is dark instead of washed-out gray");
    bitmap("advisor_briefing_preview.bmp",pixels,1920,1080);
    // The panel is look::kPanelOpacity translucent: a glyph pixel is the name colour at that opacity plus at most the rest of the scene (<= 18 per channel).
    unsigned colored_pixels=0;
    for(int y=310;y<470;++y)for(int x=210;x<360;++x)for(unsigned ink:look::kNames){
        bool near_ink=true;for(int shift=0;shift<24;shift+=8){int want=static_cast<int>(((ink>>shift)&255)*look::kPanelOpacity),got=static_cast<int>((pixels[y*1920+x]>>shift)&255);if(got<want-4 || got>want+22)near_ink=false;}
        if(near_ink)++colored_pixels;
    }
    check(colored_pixels>20,"rendered officer names use the repeating high-contrast palette rather than plain body ink");
    HDC dc=CreateCompatibleDC(nullptr);HFONT font=serif_font(dc,23);auto old=SelectObject(dc,font);wchar_t face[LF_FACESIZE]{};GetTextFaceW(dc,LF_FACESIZE,face);
    check(_wcsicmp(face,L"Noto Serif KR")==0 || _wcsicmp(face,L"함초롬바탕")==0 || _wcsicmp(face,L"바탕")==0 || _wcsicmp(face,L"Batang")==0,"bubble uses an available Korean serif family instead of the old system sans font");SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);
    unsigned expected=pixels[800*1920+152];
    ID3D11Texture2D *srgb_back=nullptr,*srgb_readback=nullptr;auto srgb_desc=desc;srgb_desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    check(SUCCEEDED(d->CreateTexture2D(&srgb_desc,nullptr,&srgb_back)),"sRGB game-style target created");
    c->UpdateSubresource(srgb_back,0,nullptr,scene.data(),1920*4,0);portrait_attempt_at=0;request_portrait({10,10,88,88},3);
    portrait_confirmation.present=true;draw(d,c,srgb_back,srgb_desc);
    check(!portrait_confirmation.present,"format reset clears the CPU confirmation sample");
    auto srgb_staging=staging;srgb_staging.Format=srgb_desc.Format;check(SUCCEEDED(d->CreateTexture2D(&srgb_staging,nullptr,&srgb_readback)),"sRGB readback created");c->CopyResource(srgb_readback,srgb_back);
    check(SUCCEEDED(c->Map(srgb_readback,0,D3D11_MAP_READ,0,&mapped)),"sRGB output mapped");unsigned actual=*reinterpret_cast<const unsigned *>(static_cast<const unsigned char *>(mapped.pData)+800*mapped.RowPitch+152*4);c->Unmap(srgb_readback,0);
    for(unsigned channel=0;channel<3;++channel)check(std::abs(static_cast<int>((actual>>(channel*8))&255)-static_cast<int>((expected>>(channel*8))&255))<=12,"sRGB output retains intended ink colors rather than lifting them to gray (blending the translucent panel in linear space differs by a few levels, a gray lift by far more)");
    srgb_readback->Release();srgb_back->Release();
    check(widget(74,1000,false)==1 && widget(30,956,false)==0,"only the circular face consumes portrait clicks");
    check(bubble.w==1160 && bubble.h==1000,"user 2026-10-27: window width and height double independently");
    check(widget(1252,79,true)==2 && widget(1000,79,true)==3 && widget(1168,79,true)==4,"user 2026-10-27: top-right arrows and close hit boxes match the header controls");
    check(widget(208,985,true)==9 && widget(260,985,true)==9,"old footer arrow positions no longer dispatch navigation");
    check(widget(1078,985,true)==5 && widget(1194,985,true)==6 && widget(400,800,true)==9 && widget(1198,205,true)==7,"deletion, real-estate and body input stay within the redesigned bubble");
    toast(L"지금부터 낙양의 토목공사를 시작합니다.");
    check(!toast_text.empty() && toast_at && raster_toast(c,toast_text) && toast_view,"the 5-second military toast rasterizes through the real device");
    toast_text.clear();toast_at=0;
    {   // 2026-11-08 user: the seventh button opens the owned-castle list; rows are direct hit targets.
        m.city_list=true;m.cities={L"낙양",L"허창",L"업"};++m.generation;publish(m,true);opened_at=GetTickCount64()-300;
        Rect panel=panel_rect(head.x);
        for(unsigned i=0;i<3;++i){Rect cell=city_cell(panel,i,3);check(widget(cell.x+4,cell.y+4,true)==100+static_cast<int>(i),"city row hit box matches the three-column grid");}
        check(widget(control(3,panel).x+2,control(3,panel).y+2,true)==9 && widget(control(5,panel).x+2,control(5,panel).y+2,true)==9,"report paging/deletion buttons are not hit targets in the city list");
        check(widget(control(7,panel).x+2,control(7,panel).y+2,true)==7,"the real-estate button stays available as the way back");
        m.city_list=false;m.cities.clear();++m.generation;publish(m,true);
    }
    dc=CreateCompatibleDC(nullptr);HFONT body=cached_font(dc,0);LOGFONTW actual_font{};GetObjectW(body,sizeof(actual_font),&actual_font);
    check(actual_font.lfHeight==-23 && actual_font.lfWeight>=FW_SEMIBOLD && cached_font(dc,0)==body,"user 2026-10-27: stronger body strokes retain font size and cached response speed");
    SelectObject(dc,body);int extent=0;auto lines=body_lines(dc,L"• 가나다라마바 사아자차카타 파하가나다라 마바사아자차\n\n다음 보고",125,extent);
    check(lines.size()>3 && lines[0].bullet && lines[0].x==80 && !lines[1].bullet && lines[1].x==80,"wrapped report bullets keep their hanging indentation");
    check(lines.back().x==56 && extent>static_cast<int>(lines.size())*36,"paragraph spacing separates distinct reports without changing glyph size");
    lines=body_lines(dc,L"• 유비  ·  200\n• 제갈량  ·  1200",1048,extent);
    check(lines.size()==4 && lines[1].right==440 && lines[3].right==440 && lines[1].y==lines[0].y,"officer names and money totals form measured columns with right-aligned amounts");
    std::wstring income=income_summary(L"제1군단",{{10,L"56105"},{11,L"(+0)"},{12,L"0"},{13,L"0"},{19,L"209376"},{20,L"(-3665)"},{21,L"0"},{22,L"3665"},{28,L"110000"},{29,L"(+0)"},{30,L"0"},{31,L"—"},{37,L"0"},{38,L"(+0)"},{39,L"0"},{40,L"—"}});
    lines=body_lines(dc,L"• 10일차 · "+income,1048,extent);
    check(lines.size()==26 && lines[7].right==lines[12].right && lines[12].right==lines[17].right && lines[17].right==lines[22].right,"gold, grain, soldiers and military-point stock values share one fixed right edge");
    check(lines[6].y==lines[10].y && lines[1].header && !lines[6].header && lines[7].text==L"56,105","table headers and all five cells stay on their own shared row baseline");
    check(lines[2].right==lines[7].right && lines[2].header && lines[7].right>lines[7].x,"numeric columns right-align their header with their values");
    {   // ---- user 2026-11-07: tab-separated runs are aligned tables; messages without a row stay text ----
        auto width_of=[&](const std::wstring &t){SIZE e{};GetTextExtentPoint32W(dc,t.c_str(),static_cast<int>(t.size()),&e);return static_cast<int>(e.cx);};
        std::vector<TableBand> bands;
        const std::wstring table=table_block({discovery_scene_row(0,L"관우",L"청룡언월도",L"",true),discovery_scene_row(2,L"장비",L"호로병",L"낙양",true),discovery_scene_row(0,L"조운",L"용담창",L"",true)});
        lines=body_lines(dc,L"• 앞 소식\n\n"+table+L"\n\n• 뒤 소식",1048,extent,&bands);
        check(bands.size()==4 && bands[0].header && !bands[1].header && bands[0].index==0 && bands[3].index==3 && bands[0].y<bands[1].y && bands[2].y<bands[3].y,"one band per table row, the first one is the header");
        check(lines.size()==18 && lines.front().text==L"앞 소식" && lines.back().text==L"뒤 소식" && !lines.front().table && !lines.back().table && lines[1].table && lines[16].table,"text before and after a table stay ordinary text lines (fallback for unparsed messages)");
        bool aligned=true,inside=true;
        for(int row=1;row<4;++row)for(int col=0;col<4;++col){
            const auto &head_cell=lines[1+col],&cell=lines[1+row*4+col];
            aligned=aligned && cell.x==head_cell.x && cell.y==lines[1+row*4].y && !cell.header && cell.table;
            inside=inside && cell.y>=bands[row].y && cell.y<bands[row].y+bands[row].h;
        }
        check(aligned && inside && lines[1].header && lines[1].x<lines[2].x && lines[2].x<lines[3].x && lines[3].x<lines[4].x,"cells of one column share x, columns run left to right, each row's text lies inside its band");
        std::vector<std::wstring> columns[4];for(int row=0;row<4;++row)for(int col=0;col<4;++col)columns[col].push_back(lines[1+row*4+col].text);
        bool measured=true;for(int col=0;col<3;++col){int widest=0;for(const auto &t:columns[col])widest=(std::max)(widest,width_of(t));measured=measured && lines[2+col].x-lines[1+col].x>=widest+look::kCellGap;}
        check(measured && bands[1].h==look::kLinePitch+look::kRowPad,"column widths come from the measured content plus the gap; single-line rows have the base height");
        // a narrow window wraps long cells, keeps every character and every line inside the window
        const std::wstring wide=table_block({discovery_scene_row(1,L"가후",L"조루",L"진창",true),discovery_scene_row(1,L"조조",L"유도",L"",false)});
        std::vector<TableBand> narrow_bands;auto narrow=body_lines(dc,wide,360,extent,&narrow_bands);
        size_t want=0,got=0;for(wchar_t ch:wide)if(ch!=L'\t' && ch!=L'\n' && ch!=L' ')++want;
        bool fits=true;for(const auto &l:narrow){for(wchar_t ch:l.text)if(ch!=L' ')++got;fits=fits && (l.right?l.right:l.x+width_of(l.text))<=56+360-look::kTablePad+1;}
        check(want==got && fits && narrow.size()>12 && narrow_bands.size()==3 && narrow_bands[1].h>look::kLinePitch+look::kRowPad,"in a narrow window long cells wrap inside their column, no text is lost or clipped");
        // paging: a different card is a different body; tables are rebuilt without stale bands
        std::vector<TableBand> second;lines=body_lines(dc,L"• 한 줄",1048,extent,&second);
        check(second.empty() && lines.size()==1,"a body without a table has no bands");
        lines=body_lines(dc,L"항목\t값\n가\t1,200\n나\t-30\n\n• 끝",1048,extent,&second);
        check(second.size()==3 && lines.size()==7 && lines[3].right>0 && lines[5].right==lines[3].right && lines[6].text==L"끝","a numeric second column is right-aligned from the header down");
    }
    DeleteDC(dc);
    check(input(nullptr,WM_KEYUP,VK_ESCAPE,0) && flag(&pending)==2,"Escape closes the bubble without passing through to game");
    unsigned generation=0;check(take(generation)==2 && generation==m.generation && input_dispatches==1,"diagnostic input delay measures queue dispatch without changing game memory");
    HWND fixture_window=CreateWindowExW(0,L"STATIC",L"inbox input fixture",WS_POPUP,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    check(fixture_window!=nullptr,"hidden fixture-owned client window, never the game");
    unified_overlay::buffer_width=1280;unified_overlay::buffer_height=720;
    LPARAM face_point=MAKELPARAM(49,667);
    check(input(fixture_window,WM_LBUTTONDOWN,0,face_point) && armed==1,"real client geometry maps a 1280x720 face press to the virtual portrait");
    check(input(fixture_window,WM_LBUTTONUP,0,face_point) && take(generation)==1 && generation==m.generation,"matching press/release publishes one portrait click");
    {   // 2026-11-08 user live report: city names looked like plain text. The real input path must post the row action.
        m.city_list=true;m.cities={L"낙양",L"허창",L"업"};++m.generation;publish(m,true);
        Rect panel=panel_rect(head.x),cell=city_cell(panel,1,3);
        const int cx=static_cast<int>((cell.x+4)*1280/1920),cy=static_cast<int>((cell.y+4)*720/1080);
        LPARAM city_point=MAKELPARAM(cx,cy);
        check(input(fixture_window,WM_LBUTTONDOWN,0,city_point) && armed==101,"real client click arms the second city row");
        check(input(fixture_window,WM_LBUTTONUP,0,city_point) && take(generation)==101 && generation==m.generation,"city row click posts its index action through the real input path");
        m.city_list=false;m.cities.clear();++m.generation;publish(m,true);
    }
    input(fixture_window,WM_LBUTTONDOWN,0,face_point);++m.generation;publish(m,true);input(fixture_window,WM_LBUTTONUP,0,face_point);
    check(take(generation)==0 && input_cancel_generation==1,"generation cancellation is counted separately from queue and drawing latency");
    input(fixture_window,WM_LBUTTONDOWN,0,face_point);input(fixture_window,WM_CAPTURECHANGED,0,0);input(fixture_window,WM_LBUTTONUP,0,face_point);
    check(take(generation)==0,"capture loss still cancels input instead of replaying a click");DestroyWindow(fixture_window);
    m.collecting=true;++m.generation;publish(m,true);check(!input(nullptr,WM_KEYUP,VK_ESCAPE,0),"progress mode does not open the inbox");
    m.collecting=false;m.open=false;++m.generation;publish(m,true);closed_at=GetTickCount64()-200;frame(); // 사용자 결정 2026-10-28 드래그 헤드: finish the 140ms close before isolating the native mask.
    mask_report(true);std::vector<unsigned> flash(1920*1080,0xffeeeeeeu);c->UpdateSubresource(back,0,nullptr,flash.data(),1920*4,0);draw(d,c,back,desc);
    auto sample=[&](){c->CopyResource(readback,back);check(SUCCEEDED(c->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"transition frame maps for verification");unsigned value=*reinterpret_cast<const unsigned *>(static_cast<const unsigned char *>(mapped.pData)+300*mapped.RowPitch+900*4);c->Unmap(readback,0);return value;};
    check(sample()==scene[300*1920+900],"a native report-colored frame is replaced by the preceding clean scene before presentation");
    publish(m,false);check(needs_draw() && !is_visible(),"report masking still renders when the normal inbox HUD is hidden");c->UpdateSubresource(back,0,nullptr,flash.data(),1920*4,0);draw(d,c,back,desc);check(sample()==scene[300*1920+900],"hidden native report state cannot bypass the presentation mask");publish(m,true);
    mask_report(false);
    for(unsigned i=0;i<2;++i){c->UpdateSubresource(back,0,nullptr,flash.data(),1920*4,0);draw(d,c,back,desc);check(sample()==scene[300*1920+900],"native report cleanup is masked for two final presentation frames");}
    c->UpdateSubresource(back,0,nullptr,flash.data(),1920*4,0);draw(d,c,back,desc);check(sample()==0xffeeeeeeu,"normal scene updates resume after the short report cleanup tail");
    m.collecting=true;publish(m,false);progress_flags(false,false);auto held_copies=scene_copies;
    c->UpdateSubresource(back,0,nullptr,flash.data(),1920*4,0);draw(d,c,back,desc);
    check(scene_copies==held_copies && !is_visible() && needs_draw(),"hidden interaction keeps report-window entry alive but does not capture diplomacy");
    progress_flags(true,true);frame();check(is_visible() && scene_copies==held_copies+1,"clean progress resumes without new event/model publication");
    mask_report(true);c->UpdateSubresource(back,0,nullptr,flash.data(),1920*4,0);draw(d,c,back,desc);
    check(sample()==scene[300*1920+900],"report mask uses the refreshed map rather than stale diplomacy");mask_report(false);frame();frame();
    m.collecting=false;publish(m,true);
    m.open=true;m.title=L"수입과 지출 · 4건";m.body=L"• 10일차 · "+income+L"\n\n• 10일차 · "+income_summary(L"제2군단",{{10,L"22667"},{11,L"(-3005)"},{12,L"0"},{13,L"3005"},{19,L"128268"},{20,L"(-2406)"},{21,L"0"},{22,L"2406"},{28,L"41626"},{29,L"(+0)"},{30,L"0"},{31,L"—"},{37,L"0"},{38,L"(+0)"},{39,L"0"},{40,L"—"}});++m.generation;post_input(4,m.generation);check(take(generation)==4,"navigation latency probe consumes its own posted ticket");publish(m,true,true);opened_at=GetTickCount64()-300;frame();c->CopyResource(readback,back);
    check(input_accepted==1 && render_input_started==0 && input_posts>=3,"accepted input is timed through updated GPU command submission without changing its dispatch");
    check(SUCCEEDED(c->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"aligned income preview mapped");for(unsigned y=0;y<1080;++y)memcpy(pixels.data()+y*1920,static_cast<unsigned char *>(mapped.pData)+y*mapped.RowPitch,1920*4);c->Unmap(readback,0);bitmap("advisor_income_preview.bmp",pixels,1920,1080);
    {   // visual preview of the report tables (user 2026-11-07): a 22-message "전해 온 소식" page shaped like the in-game screenshot, merged into one table per kind
        Card page;page.kind=Kind::News;
        auto msg=[&](unsigned day,const wchar_t *text){Event e;e.day=day;e.text=text;page.events.push_back(e);};
        msg(10,L"정욱, 양양의 탐색에 실패");msg(10,L"최림, 격려의 서 발견");msg(8,L"전령\n보고드립니다.\n김선군의 사자가\n조조님께 알현을 요청하였습니다.");
        {Event e;e.day=8;e.text=e.report=L"한기가 여궁에서 명승을 발견했습니다.";e.row=discovery_scene_row(0,L"한기",L"여궁",L"",true);page.events.push_back(e);}
        msg(10,L"조휴부대, 병사감소에 의해 퇴각");msg(7,L"천시가 변했습니다.");msg(10,L"채염, 교란의 서 발견");msg(9,L"전령\n보고드립니다.\n유도군의 사자가\n조조님께 알현을 요청하였습니다.");
        msg(10,L"조조군, 유도군의 교환요청 거부");msg(6,L"서서, 풍운의 서 발견");msg(9,L"한현군, 유비군에 항복");msg(5,L"조조군, 유표군의 공격요청 승낙");
        msg(4,L"장합부대, 비어에 의해 출진불가");msg(4,L"황개, 시상의 탐색에 실패");msg(3,L"홍수가 발생했습니다.");msg(3,L"조운, 청룡의 서 발견");
        msg(2,L"조조군, 손권군측과 동맹 실패");msg(9,L"곽가, 모략의 서 발견");msg(2,L"전령\n유도군의 사자가 찾아왔습니다.");msg(1,L"가후가 새 임무를 받았습니다.");msg(10,L"조조, 유도의 등용에 성공");msg(6,L"최림, 완의 탐색을 단념");
        Model tm=m;tm.title=L"전해 온 소식 · 22건";tm.names={L"정욱",L"최림",L"한기",L"채염",L"서서",L"조운",L"황개",L"곽가",L"조조",L"전령"};
        tm.body=card_body(page);
        ++tm.generation;publish(tm,true,false);opened_at=GetTickCount64()-300;frame();c->CopyResource(readback,back);
        check(SUCCEEDED(c->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"report tables preview mapped");for(unsigned y=0;y<1080;++y)memcpy(pixels.data()+y*1920,static_cast<unsigned char *>(mapped.pData)+y*mapped.RowPitch,1920*4);c->Unmap(readback,0);bitmap("advisor_tables_preview.bmp",pixels,1920,1080);
        scrolling.shown=scrolling.target=720;frame();c->CopyResource(readback,back);   // second half of the same page
        check(SUCCEEDED(c->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"scrolled report tables preview mapped");for(unsigned y=0;y<1080;++y)memcpy(pixels.data()+y*1920,static_cast<unsigned char *>(mapped.pData)+y*mapped.RowPitch,1920*4);c->Unmap(readback,0);bitmap("advisor_tables_scrolled_preview.bmp",pixels,1920,1080);scrolling.reset();
        publish(m,true,false);opened_at=GetTickCount64()-300;frame();
    }
    portrait_attempt_at=0;request_portrait({10,10,88,88},4);frame();
    for(int attempt=0;attempt<60 && !captured(4);++attempt){c->Flush();Sleep(2);frame();}
    check(captured(4),"asynchronous portrait validation completes on a later Present");
    // Warmed panel must never raster or upload for hover, armed or scrolling.
    portrait_attempt_at=GetTickCount64();InterlockedExchange(&portrait_requested,0);hover=armed=0;badge_at=0;
    head=HeadMotion{};opened_at=GetTickCount64()-300;frame();
    const auto preview_body=m.body;
    for(int row=0;row<320;++row)m.body+=L"\n• 긴 보고의 스크롤과 본문 가림을 검사합니다.";
    ++m.generation;publish(m,true);opened_at=GetTickCount64()-300;frame();
    D3D11_TEXTURE2D_DESC body_desc{};body_texture->GetDesc(&body_desc);
    check(body_desc.Width==1048 && body_desc.Height==8192 && max_scroll==8192-618,"body texture is bounded to 8192 pixels and scroll range matches its visible content");
    auto raster_before=raster_calls,uploads_before=panel_uploads;
    for(int frame_index=0;frame_index<60;++frame_index)frame();
    check(raster_calls==raster_before && panel_uploads==uploads_before,"sixty stationary frames perform zero GDI rasters and zero panel uploads");
    for(int frame_index=0;frame_index<60;++frame_index){hover=2+frame_index%6;armed=frame_index%2?hover:0;scrolling.wheel(80,max_scroll);animation_at=GetTickCount64()-16;frame();}
    check(raster_calls==raster_before && panel_uploads==uploads_before && scrolling.shown>0,"hover, pressed controls and actual smooth scrolling reuse all three raster layers");
    hover=armed=0;scrolling.shown=scrolling.target=4000;frame();
    c->CopyResource(readback,back);check(SUCCEEDED(c->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"scrolled panel readback");
    Rect scrolled_panel=panel_rect(head.x),reading=body_rect(scrolled_panel);
    auto sampled=[&](float x,float y){return *reinterpret_cast<const unsigned *>(static_cast<const unsigned char *>(mapped.pData)+static_cast<int>(y)*mapped.RowPitch+static_cast<int>(x)*4);};
    unsigned header_pixel=sampled(scrolled_panel.x+160,scrolled_panel.y+100);
    unsigned footer_pixel=sampled(scrolled_panel.x+160,scrolled_panel.y+scrolled_panel.h-40);
    std::vector<unsigned> clipped_pixels(1920*1080);
    for(unsigned y=0;y<1080;++y)memcpy(clipped_pixels.data()+y*1920,static_cast<unsigned char *>(mapped.pData)+y*mapped.RowPitch,1920*4);
    c->Unmap(readback,0);
    const unsigned paper_pixel=sampled(reading.x+reading.w-6,reading.y+reading.h/2)&0xffffffu;   // body background (translucent navy over the dimmed scene), no text there
    check((header_pixel&0xffffffu)!=paper_pixel && (footer_pixel&0xffffffu)!=paper_pixel,"Claude 2026-10-28: 종이 영역 밖 표본 — header and footer are chrome, not the body background");
    // Draw the identical production frame with only ScissorEnable changed.
    auto *enabled_scissor=scissor_state;scissor_state=raster_state;frame();scissor_state=enabled_scissor;
    c->CopyResource(readback,back);check(SUCCEEDED(c->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"unclipped comparison frame mapped");
    for(float row:{reading.y-8,reading.y+reading.h+8}){
        unsigned leaking_glyphs=0;const int edge=static_cast<int>(reading.x+reading.w-6);
        const unsigned unclipped_paper=sampled(static_cast<float>(edge),row)&0xffffffu,clipped_paper=clipped_pixels[static_cast<size_t>(row)*1920+edge]&0xffffffu;
        for(int x=static_cast<int>(reading.x);x<static_cast<int>(reading.x+reading.w);++x){
            unsigned unclipped=sampled(static_cast<float>(x),row)&0xffffffu;
            unsigned clipped=clipped_pixels[static_cast<size_t>(row)*1920+x]&0xffffffu;
            if(unclipped!=unclipped_paper){++leaking_glyphs;check(clipped==clipped_paper,"scissor removes glyph pixels immediately outside the reading viewport");}
        }
        check(leaking_glyphs>0,"Claude 2026-10-28: 종이 영역 밖 표본 — disabling scissor exposes text above and below the reading viewport");
    }
    c->Unmap(readback,0);frame();
    hover=armed=0;scrolling.reset();
    collecting_ended_at=0;was_collecting=false;auto copies_before=scene_copies;
    for(int frame_index=0;frame_index<60;++frame_index)frame();
    check(scene_copies==copies_before,"normal player standby performs no stable-scene copies");
    m.collecting=true;publish(m,true);copies_before=scene_copies;for(int i=0;i<3;++i)frame();check(scene_copies==copies_before+3,"collecting copies one clean scene each frame");
    m.collecting=false;publish(m,true);copies_before=scene_copies;for(int i=0;i<3;++i)frame();check(scene_copies==copies_before+3,"eight-second post-collection window copies each frame");
    collecting_ended_at=GetTickCount64()-8001;copies_before=scene_copies;frame();check(scene_copies==copies_before,"clean-scene copies stop after eight seconds");
    mask_report(true);copies_before=scene_copies;frame();frame();check(scene_copies==copies_before+2,"masked frames retain one restoration copy each frame");mask_report(false);copies_before=scene_copies;frame();frame();check(scene_copies==copies_before+2 && !flag(&report_tail),"exactly two tail frames retain restoration copies");
    copies_before=scene_copies;frame();check(scene_copies==copies_before,"no scene copies after the tail outside the report window");
    m.open=false;publish(m,true);closing=false;badge_at=0;head=HeadMotion{};scrolling.reset();frame();
    auto idle_quads=quad_calls,idle_rasters=raster_calls,idle_uploads=panel_uploads,idle_copies=scene_copies;
    for(int i=0;i<10;++i)frame();
    check(quad_calls==idle_quads+20 && raster_calls==idle_rasters && panel_uploads==idle_uploads && scene_copies==idle_copies,"closed stationary bubble draws only head and badge, with no raster/upload/scene copy");
    check(draw_us>=0 && draw_max_us>=draw_us && present_us>0 && present_max_us>=present_us,"QPC draw and Present interval metrics publish recent/max microseconds");
    // Real WndProc path in a hidden fixture-owned window, with virtual 1:1 pixels.
    HWND drag_window=CreateWindowExW(0,L"STATIC",L"drag UI fixture",WS_POPUP,0,0,1920,1080,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    check(drag_window!=nullptr,"hidden drag fixture window");unified_overlay::buffer_width=1920;unified_overlay::buffer_height=1080;
    m.open=false;publish(m,true);closed_at=GetTickCount64()-200;frame();head=HeadMotion{};
    check(input(drag_window,WM_LBUTTONDOWN,0,MAKELPARAM(74,1000)) && GetCapture()==drag_window,"head press captures pointer");
    check(flag(&wheel_block) && flag(&block_buttons),"head press blocks DI wheel and left button");
    check(input(drag_window,WM_MOUSEMOVE,0,MAKELPARAM(81,1000)) && !head.dragging,"under-eight-pixel head motion is consumed without dragging");
    check(input(drag_window,WM_LBUTTONUP,0,MAKELPARAM(81,1000)) && take(generation)==1 && !GetCapture(),"short head gesture posts exactly one tap and releases capture");
    input(drag_window,WM_LBUTTONDOWN,0,MAKELPARAM(74,1000));check(input(drag_window,WM_MOUSEMOVE,0,MAKELPARAM(82,1000)) && head.dragging,"eight-pixel move starts a drag and consumes map input");
    check(!input(drag_window,WM_CAPTURECHANGED,0,reinterpret_cast<LPARAM>(drag_window)) && head.pressed && head.dragging && capture_window==drag_window && GetCapture()==drag_window,"recapture by the same window keeps the active head drag and passes the notification through");
    check(input(drag_window,WM_MOUSEMOVE,0,MAKELPARAM(1700,40)) && head.x==1700 && head.y==110,"head follows pointer and clamps top y");
    check(input(drag_window,WM_LBUTTONUP,0,MAKELPARAM(1700,40)) && take(generation)==0 && head.snapping,"drag release posts no action and starts edge spring");
    check(input(drag_window,WM_MOUSEMOVE,0,MAKELPARAM(900,500)) && input(drag_window,WM_LBUTTONUP,0,MAKELPARAM(900,500)),"snap motion and release cannot reach the map");
    for(int i=0;i<22;++i)head.advance(1.0/60);check(head.x==1846 && head.y==110 && !head.snapping,"WndProc gesture settles at the right edge within 367ms");
    Rect dynamic_panel=panel_rect(head.x);check(dynamic_panel.x+dynamic_panel.w==head.x-68,"right-snapped panel is anchored left of head");
    for(unsigned id=2;id<=7;++id){Rect item=control(id,dynamic_panel);check(widget(item.x+item.w/2,item.y+item.h/2,true)==static_cast<int>(id),"dynamic control hit box matches its drawing rectangle");}
    m.open=true;publish(m,true);opened_at=GetTickCount64()-300;frame();
    m.open=false;publish(m,true);closed_at=GetTickCount64();
    check(closing && !input(drag_window,WM_LBUTTONDOWN,0,MAKELPARAM(20,1040)) && !input(drag_window,WM_LBUTTONUP,0,MAKELPARAM(20,1040)),"during the 140ms close, map clicks outside the bubble and head pass through");
    check(input(drag_window,WM_LBUTTONDOWN,0,MAKELPARAM(1846,110)) && input(drag_window,WM_LBUTTONUP,0,MAKELPARAM(1846,110)) && take(generation)==0,"closing still consumes head clicks without posting a tap");
    Rect closing_panel=panel_rect(head.x);
    check(input(drag_window,WM_LBUTTONDOWN,0,MAKELPARAM(static_cast<int>(closing_panel.x+200),static_cast<int>(closing_panel.y+400))),"the still-visible closing bubble consumes its own clicks");
    scrolling.press(400);
    check(input(drag_window,WM_RBUTTONDOWN,0,MAKELPARAM(20,1040)) && scrolling.pressed,"an active body press still consumes outside mouse input during closing");scrolling.release();
    m.open=true;publish(m,true);opened_at=GetTickCount64()-300;
    input(drag_window,WM_LBUTTONDOWN,0,MAKELPARAM(1846,110));input(drag_window,WM_MOUSEMOVE,0,MAKELPARAM(1800,120));check(take(generation)==2,"dragging an open head posts only the existing close action");
    publish(m,false);check(!flag(&wheel_block) && !flag(&block_buttons),"hidden bubble clears DI blocking immediately");check(!head.dragging && !head.pressed && head.x==1800,"hidden bubble cancels capture and retains its last position");
    m.collecting=true;m.open=false;publish(m,true);frame();input(drag_window,WM_LBUTTONDOWN,0,MAKELPARAM(1800,120));input(drag_window,WM_LBUTTONUP,0,MAKELPARAM(1800,120));check(take(generation)==0,"collecting blocks head taps");
    input(drag_window,WM_LBUTTONDOWN,0,MAKELPARAM(1800,120));check(input(drag_window,WM_MOUSEMOVE,0,MAKELPARAM(400,1400)) && head.dragging && head.y==1000,"collecting permits pure UI drag with bottom clamp");
    check(input(drag_window,WM_CAPTURECHANGED,0,0) && head.snapping && take(generation)==0,"capture loss snaps a drag without posting a tap");
    head.advance(.36);check(head.x==74 && head.y==1000,"capture-loss drag settles on nearest left edge");
    if(GetCapture()==drag_window)ReleaseCapture();DestroyWindow(drag_window);
    m.collecting=false;m.open=true;m.body=preview_body;++m.generation;publish(m,true);opened_at=GetTickCount64()-300;badge_at=0;hover=armed=0;head=HeadMotion{};scrolling.reset();frame();
    auto preview=[&](const wchar_t *path){c->CopyResource(readback,back);check(SUCCEEDED(c->Map(readback,0,D3D11_MAP_READ,0,&mapped)),"chathead preview readback");for(unsigned y=0;y<1080;++y)memcpy(pixels.data()+y*1920,static_cast<unsigned char *>(mapped.pData)+y*mapped.RowPitch,1920*4);c->Unmap(readback,0);png(path,pixels,1920,1080);};
    preview(L"advisor_drag_left_preview.png");head.x=1846;head.y=650;frame();preview(L"advisor_drag_right_preview.png");
    // WARP rendering at the requested three target resolutions.
    for(unsigned width:{1280u,2560u,3840u}){
        auto size_desc=desc;size_desc.Width=width;size_desc.Height=width*9/16;ID3D11Texture2D *size_back=nullptr,*size_read=nullptr;
        check(SUCCEEDED(d->CreateTexture2D(&size_desc,nullptr,&size_back)),"multi-resolution WARP render target");
        std::vector<unsigned> size_pixels(size_t(width)*size_desc.Height,0xff63877cu);c->UpdateSubresource(size_back,0,nullptr,size_pixels.data(),width*4,0);
        draw(d,c,size_back,size_desc);auto size_stage=size_desc;size_stage.BindFlags=0;size_stage.Usage=D3D11_USAGE_STAGING;size_stage.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        check(SUCCEEDED(d->CreateTexture2D(&size_stage,nullptr,&size_read)),"multi-resolution WARP readback");c->CopyResource(size_read,size_back);check(SUCCEEDED(c->Map(size_read,0,D3D11_MAP_READ,0,&mapped)),"multi-resolution render mapped");
        double factor=width/1920.0;float vx=0,vy=0;int hx=static_cast<int>(head.x*factor),hy=static_cast<int>(head.y*factor);
        check(client_point(hx,hy,static_cast<int>(width),static_cast<int>(size_desc.Height),static_cast<int>(width),static_cast<int>(size_desc.Height),vx,vy) && widget(vx,vy,true)==1,"scaled head pixels and hit geometry agree");
        unsigned face_pixel=*reinterpret_cast<const unsigned *>(static_cast<const unsigned char *>(mapped.pData)+hy*mapped.RowPitch+hx*4);
        check((face_pixel&0xffffffu)==0x34485fu,"WARP head center is drawn at the transformed hit position");
        Rect panel=panel_rect(head.x);int px=static_cast<int>((panel.x+20)*factor),py=static_cast<int>((panel.y+800)*factor);
        unsigned panel_pixel=*reinterpret_cast<const unsigned *>(static_cast<const unsigned char *>(mapped.pData)+py*mapped.RowPitch+px*4);
        check((panel_pixel&0xffffffu)!=0x63877cu && panel.contains(static_cast<float>(px/factor),static_cast<float>(py/factor)),"WARP dynamic panel draw stays in its transformed rectangle");
        c->Unmap(size_read,0);size_read->Release();size_back->Release();
    }
    // 사용자 2026-10-28 입력·대기창 보정: complete outside click pair.
    HWND outside_window=CreateWindowExW(0,L"STATIC",L"outside input fixture",WS_POPUP,0,0,1920,1080,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    check(outside_window!=nullptr,"outside click fixture window");
    unified_overlay::buffer_width=1920;unified_overlay::buffer_height=1080;
    head=HeadMotion{};scrolling.reset();armed=hover=0;m.collecting=false;m.open=true;publish(m,true);closing=false;opened_at=GetTickCount64()-300;
    LPARAM outside=MAKELPARAM(1800,1040);
    check(input(outside_window,WM_LBUTTONDOWN,0,outside) && outside_click && flag(&block_buttons) && take(generation)==2,"outside DOWN posts close and blocks DI left button");
    wheel_block=1;progress_flags(false,true,false);
    check(flag(&wheel_block) && flag(&block_buttons),"player log frame updates scene flags without clearing the active wheel/button pair");
    m.open=false;publish(m,true);closed_at=GetTickCount64()-200;
    check(input(outside_window,WM_MOUSEMOVE,0,outside) && outside_click && flag(&block_buttons),"dismiss click survives native close and animation expiry until UP");
    check(input(outside_window,WM_LBUTTONUP,0,outside) && !outside_click && !flag(&block_buttons) && !take(generation),"matching outside UP consumed once without second action");
    check(!input(outside_window,WM_LBUTTONDOWN,0,outside) && !input(outside_window,WM_LBUTTONUP,0,outside),"following map click passes through closed panel");
    m.open=true;publish(m,true);opened_at=GetTickCount64()-300;closing=false;
    scrolling.press(300);scrolling.move(320,.016,1000);
    check(!input(outside_window,WM_LBUTTONDOWN,0,outside) && !outside_click && !take(generation),"active body drag preserves existing outside DOWN behavior");scrolling.reset();
    scrolling.wheel(80,1000);check(!input(outside_window,WM_LBUTTONDOWN,0,outside) && !outside_click && !take(generation),"moving wheel scroll does not outside-dismiss");scrolling.reset();
    head.pressed=head.dragging=true;check(!input(outside_window,WM_LBUTTONDOWN,0,outside) && !outside_click && !take(generation),"head drag cannot trigger outside-dismiss");head.cancel();
    wheel_block=block_buttons=1;head.pressed=true;scrolling.press(300);post_input(1,m.generation);
    player_context(false);
    check(!model.open && !closing && !is_visible() && !head.pressed && !scrolling.pressed && !flag(&wheel_block) && !flag(&block_buttons) && !take(generation),"covered native state closes model, cancels input and releases all DI blocks");
    check(!input(outside_window,WM_LBUTTONDOWN,0,MAKELPARAM(74,1000)) && !input(outside_window,WM_LBUTTONUP,0,outside),"covered panel and head pass clicks to native dialog");
    player_context(true);check(is_visible() && !model.open,"player return restores head with panel closed");
#ifndef NO_RUNTIME_LOG
    check(outside_click_counts[0]==1 && outside_click_counts[1]==1 && context_resets>0 && di_owner_checks[0]>0 && di_owner_checks[1]>0,"input correction diagnostics count accepted and rejected paths");
#endif
    DestroyWindow(outside_window);
    portrait_confirmation.present=true;forget();check(!available() && !is_visible(),"world teardown clears readiness and portrait capture");
    check(!portrait_confirmation.present,"forget clears the CPU confirmation sample");
    portrait_confirmation.present=true;release_device();check(!portrait_confirmation.present,"device release clears the CPU confirmation sample");
    c->ClearState();readback->Release();original_target->Release();back->Release();c->Release();d->Release();CoUninitialize();
    printf("{\"status\":\"advisor_overlay_warp_fixture_passed\",\"checks\":%u,\"game_launched\":false,\"live_game\":false}\n",checks);return 0;
}
