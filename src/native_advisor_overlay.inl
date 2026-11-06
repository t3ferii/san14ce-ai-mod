// Shares the existing Present/WndProc owner. Never dereferences a game object.
#include "advisor_briefing.h"
#include "advisor_vs.h"
#include "advisor_ps.h"
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#pragma comment(lib,"advapi32.lib")
namespace briefing_overlay {
using namespace advisor_briefing_core;
static volatile LONG wheel_block,block_buttons;
#ifndef NO_RUNTIME_LOG
static volatile LONG64 present_thread_id,present_tick_ms,wndproc_thread_id,wndproc_tick_ms;
static volatile LONG64 lock_thread_id,lock_tick_ms;
// Interface code: 0=none, 1=A, 2=W (first installed table).
static volatile LONG64 di_install_attempted,di_install_success,di_patched_table,di_interface_kind;
// Per IID A/W: stage, Create HRESULT, device HRESULT, range mask, Win32 error,
// input pointer, device table, attempts. HRESULTs retain their unsigned 32 bits.
static volatile LONG64 di_install_details[2][8]{};
static volatile LONG64 di_provider_stage,di_interface_mask;
static wchar_t di_owner_paths[2][2][MAX_PATH]{};
static volatile LONG64 di_owner_checks[2]{},outside_click_counts[2]{},context_resets;
static volatile LONG64 di_restore_counts[2]{}; // restored, changed by another owner
static volatile LONG64 di_state_calls,di_data_calls,di_state_modified,di_data_modified;
static volatile LONG64 draw_us,draw_max_us,present_us,present_max_us;
static unsigned long long quad_calls;
static LARGE_INTEGER last_present_qpc{};
struct DrawTiming {
    LARGE_INTEGER start{},frequency{};
    DrawTiming(){QueryPerformanceCounter(&start);QueryPerformanceFrequency(&frequency);
        if(last_present_qpc.QuadPart){LONG64 us=(start.QuadPart-last_present_qpc.QuadPart)*1000000/frequency.QuadPart;InterlockedExchange64(&present_us,us);if(us>present_max_us)InterlockedExchange64(&present_max_us,us);}last_present_qpc=start;}
    ~DrawTiming(){LARGE_INTEGER end;QueryPerformanceCounter(&end);LONG64 us=(end.QuadPart-start.QuadPart)*1000000/frequency.QuadPart;InterlockedExchange64(&draw_us,us);if(us>draw_max_us)InterlockedExchange64(&draw_max_us,us);}
};
#endif
using DeviceState=HRESULT(STDMETHODCALLTYPE *)(IDirectInputDevice8W *,DWORD,LPVOID);
using DeviceData=HRESULT(STDMETHODCALLTYPE *)(IDirectInputDevice8W *,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD);
static DeviceState original_device_state;
static DeviceData original_device_data;
static void **mouse_slots;
struct MouseTable {void **table;DeviceState state;DeviceData data;IUnknown *input,*device;};
static MouseTable mouse_tables[2]{};
static unsigned mouse_attempts;
static unsigned mouse_interface_success;
static ULONGLONG mouse_retry_at;
enum DiStage {di_untried,di_no_proxy,di_no_export,di_create_failed,di_input_null,
    di_device_failed,di_device_null,di_range_failed,di_protect_failed,di_installed,di_table_full};
static void di_detail(unsigned kind,unsigned field,LONG64 value) {
#ifndef NO_RUNTIME_LOG
    InterlockedExchange64(&di_install_details[kind-1][field],value);
#else
    (void)kind;(void)field;(void)value;
#endif
}
static SRWLOCK mouse_lock=SRWLOCK_INIT;
struct MouseIdentity {IDirectInputDevice8W *device;void **table;bool mouse;};
static MouseIdentity mouse_identities[64]{};
static unsigned mouse_identity_count;
static bool mouse_device(IDirectInputDevice8W *device) {
    if(!device)return false;
    AcquireSRWLockExclusive(&mouse_lock);void **table=*reinterpret_cast<void ***>(device);
    for(unsigned i=0;i<mouse_identity_count;++i)if(mouse_identities[i].device==device && mouse_identities[i].table==table){bool yes=mouse_identities[i].mouse;ReleaseSRWLockExclusive(&mouse_lock);return yes;}
    // GetCapabilities has identical A/W ABI; GetDeviceInfoW rejects A's size.
    DIDEVCAPS info{};info.dwSize=sizeof(info);
    bool yes=SUCCEEDED(device->GetCapabilities(&info)) && GET_DIDEVICE_TYPE(info.dwDevType)==DI8DEVTYPE_MOUSE;
    if(mouse_identity_count<64)mouse_identities[mouse_identity_count++]={device,table,yes};else yes=false;
    ReleaseSRWLockExclusive(&mouse_lock);return yes;
}
static HRESULT STDMETHODCALLTYPE filtered_device_state(IDirectInputDevice8W *device,DWORD bytes,LPVOID data) { COST_SCOPE("filtered_device_state");
    auto original=original_device_state;
    for(const auto &t:mouse_tables)if(t.table==*reinterpret_cast<void ***>(device)){original=t.state;break;}
    HRESULT result=original(device,bytes,data);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&di_state_calls);
#endif
    if(SUCCEEDED(result) && data && (bytes==sizeof(DIMOUSESTATE) || bytes==sizeof(DIMOUSESTATE2)) && mouse_device(device)) {
        auto *state=static_cast<DIMOUSESTATE *>(data);bool changed=false;
        if(InterlockedCompareExchange(&wheel_block,0,0) && state->lZ){state->lZ=0;changed=true;}
        if(InterlockedCompareExchange(&block_buttons,0,0) && state->rgbButtons[0]){state->rgbButtons[0]=0;changed=true;}
#ifndef NO_RUNTIME_LOG
        if(changed)InterlockedIncrement64(&di_state_modified);
#endif
    }
    return result;
}
static HRESULT STDMETHODCALLTYPE filtered_device_data(IDirectInputDevice8W *device,DWORD bytes,LPDIDEVICEOBJECTDATA data,LPDWORD count,DWORD flags) { COST_SCOPE("filtered_device_data");
    auto original=original_device_data;
    for(const auto &t:mouse_tables)if(t.table==*reinterpret_cast<void ***>(device)){original=t.data;break;}
    DWORD capacity=count?*count:0;HRESULT result=original(device,bytes,data,count,flags);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&di_data_calls);
#endif
    if(SUCCEEDED(result) && data && count && bytes==sizeof(DIDEVICEOBJECTDATA) && *count<=capacity && mouse_device(device)) {
        bool changed=false;
        for(DWORD i=0;i<*count;++i)if((data[i].dwOfs==DIMOFS_Z && InterlockedCompareExchange(&wheel_block,0,0)) || (data[i].dwOfs==DIMOFS_BUTTON0 && InterlockedCompareExchange(&block_buttons,0,0))) {
            changed=changed || data[i].dwData!=0;data[i].dwData=0;
        }
#ifndef NO_RUNTIME_LOG
        if(changed)InterlockedIncrement64(&di_data_modified);
#endif
    }
    return result;
}
// Resolve the owner of each actual slot, never the ambiguous dinput8 basename.
using MouseOwnerPath=bool(*)(void *,wchar_t (&)[MAX_PATH]);
static bool mouse_owner_path(void *address,wchar_t (&path)[MAX_PATH]) {
    HMODULE owner=nullptr;
    if(!address || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(address),&owner))return false;
    DWORD n=GetModuleFileNameW(owner,path,MAX_PATH);return n && n<MAX_PATH;
}
// SteamPath is independent of the module name supplied by a foreign hook.
// Never load/execute the overlay to establish trust; require the installed file.
static bool steam_overlay_path(wchar_t (&path)[MAX_PATH]) {
    wchar_t directory[MAX_PATH]{};DWORD bytes=sizeof(directory);
    LSTATUS status=RegGetValueW(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath",RRF_RT_REG_SZ,nullptr,directory,&bytes);
    if(status!=ERROR_SUCCESS){
        bytes=sizeof(directory);
        status=RegGetValueW(HKEY_LOCAL_MACHINE,L"Software\\Valve\\Steam",L"InstallPath",RRF_RT_REG_SZ|RRF_SUBKEY_WOW6432KEY,nullptr,directory,&bytes);
    }
    if(status!=ERROR_SUCCESS || !directory[0])return false;
    std::wstring candidate=directory;std::replace(candidate.begin(),candidate.end(),L'/',L'\\');
    while(!candidate.empty() && candidate.back()==L'\\')candidate.pop_back();
    if(candidate.empty())return false;
    candidate+=L"\\gameoverlayrenderer64.dll";
    DWORD n=GetFullPathNameW(candidate.c_str(),MAX_PATH,path,nullptr);
    if(!n || n>=MAX_PATH){path[0]=0;return false;}
    DWORD attributes=GetFileAttributesW(path);
    if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&FILE_ATTRIBUTE_DIRECTORY)){path[0]=0;return false;}
    return true;
}
static bool allowed_mouse_path(const wchar_t *path,const wchar_t *system_directory,const wchar_t *game_exe,const wchar_t *steam_overlay=nullptr) {
    if(!path || !*path || !system_directory || !*system_directory || !game_exe)return false;
    std::wstring system=std::wstring(system_directory)+L"\\dinput8.dll";
    std::wstring game=game_exe;auto slash=game.find_last_of(L"\\/");
    if(slash==std::wstring::npos)return false;
    game.resize(slash+1);game+=L"SAN14_AI_PreviousInput.dll";
    return !_wcsicmp(path,system.c_str()) || !_wcsicmp(path,game.c_str()) ||
        (steam_overlay && *steam_overlay && !_wcsicmp(path,steam_overlay));
}
static bool allowed_mouse_owner(void *address,unsigned kind,unsigned slot,MouseOwnerPath resolve) {
    wchar_t path[MAX_PATH]{},system[MAX_PATH]{},game[MAX_PATH]{},steam[MAX_PATH]{};
    UINT n=GetSystemDirectoryW(system,MAX_PATH);DWORD g=GetModuleFileNameW(nullptr,game,MAX_PATH);
    bool resolved=resolve(address,path);
    bool have_steam=steam_overlay_path(steam);
    bool allowed=resolved && n && n<MAX_PATH && g && g<MAX_PATH && allowed_mouse_path(path,system,game,have_steam?steam:nullptr);
#ifndef NO_RUNTIME_LOG
    wcscpy_s(di_owner_paths[kind-1][slot],path);
    InterlockedIncrement64(&di_owner_checks[allowed?0:1]);
#else
    (void)kind;(void)slot;
#endif
    return allowed;
}
using MouseProtect=BOOL(WINAPI *)(LPVOID,SIZE_T,DWORD,PDWORD);
static bool install_mouse_table(void **table,unsigned kind=2,MouseProtect protect=&VirtualProtect,MouseOwnerPath resolve=&mouse_owner_path) {
    if(!table){di_detail(kind,0,di_device_null);return false;}
    for(const auto &t:mouse_tables)if(t.table==table){
        bool state_ok=allowed_mouse_owner(reinterpret_cast<void *>(t.state),kind,0,resolve);
        bool data_ok=allowed_mouse_owner(reinterpret_cast<void *>(t.data),kind,1,resolve);
        di_detail(kind,3,(state_ok?1:0)|(data_ok?2:0));di_detail(kind,6,reinterpret_cast<LONG64>(table));
        di_detail(kind,0,state_ok && data_ok?di_installed:di_range_failed);return state_ok && data_ok;
    }
    MouseTable *next=nullptr;for(auto &t:mouse_tables)if(!t.table){next=&t;break;}
    if(!next){di_detail(kind,0,di_table_full);return false;}
    bool state_ok=allowed_mouse_owner(table[9],kind,0,resolve),data_ok=allowed_mouse_owner(table[10],kind,1,resolve);
    unsigned ranges=(state_ok?1u:0u)|(data_ok?2u:0u);
    di_detail(kind,3,ranges);di_detail(kind,6,reinterpret_cast<LONG64>(table));
    if(ranges!=3){di_detail(kind,0,di_range_failed);return false;}
    DWORD prior=0;if(!protect(table+9,2*sizeof(void *),PAGE_READWRITE,&prior)){di_detail(kind,4,GetLastError());di_detail(kind,0,di_protect_failed);return false;}
    // Chain through whatever validated provider is currently in each slot,
    // including Steam's wrappers, rather than bypassing it to a system export.
    *next={table,reinterpret_cast<DeviceState>(table[9]),reinterpret_cast<DeviceData>(table[10]),nullptr,nullptr};
    if(!mouse_slots){original_device_state=next->state;original_device_data=next->data;mouse_slots=table;
#ifndef NO_RUNTIME_LOG
        InterlockedExchange64(&di_patched_table,reinterpret_cast<LONG64>(table));
        InterlockedExchange64(&di_interface_kind,kind);
#endif
    }
    InterlockedExchangePointer(table+9,reinterpret_cast<void *>(&filtered_device_state));InterlockedExchangePointer(table+10,reinterpret_cast<void *>(&filtered_device_data));
#ifndef NO_RUNTIME_LOG
    InterlockedExchange64(&di_install_success,1);
#endif
    di_detail(kind,0,di_installed);
    DWORD ignored;if(!VirtualProtect(table+9,2*sizeof(void *),prior,&ignored))di_detail(kind,4,GetLastError());return true;
}
using InputCreate=HRESULT(WINAPI *)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN);
template<class Input,class Device> static bool try_mouse_interface(InputCreate create,unsigned kind,MouseOwnerPath resolve) {
    GUID iid={kind==1?0xBF798030u:0xBF798031u,0x483A,0x4DA2,{0xAA,0x99,0x5D,0x64,0xED,0x36,0x97,0x00}};
    const GUID mouse={0x6F1D2B60,0xD5A0,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00}};
    Input *input=nullptr;Device *device=nullptr;
    HRESULT hr=create(GetModuleHandleW(nullptr),0x0800,iid,reinterpret_cast<void **>(&input),nullptr);
    di_detail(kind,1,static_cast<unsigned long>(hr));di_detail(kind,5,reinterpret_cast<LONG64>(input));
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&di_install_details[kind-1][7]);
#endif
    if(FAILED(hr) || !input){di_detail(kind,0,FAILED(hr)?di_create_failed:di_input_null);if(input)input->Release();return false;}
    hr=input->CreateDevice(mouse,&device,nullptr);di_detail(kind,2,static_cast<unsigned long>(hr));
    bool ok=false;
    if(FAILED(hr) || !device)di_detail(kind,0,FAILED(hr)?di_device_failed:di_device_null);
    else {
        auto table=*reinterpret_cast<void ***>(device);
        ok=install_mouse_table(table,kind,&VirtualProtect,resolve);
        // Keep COM/provider lifetime until restoration, including shared A/W tables.
        if(ok)for(auto &t:mouse_tables)if(t.table==table && !t.device){t.device=device;t.input=input;device=nullptr;input=nullptr;break;}
    }
    if(device)device->Release();if(input)input->Release();return ok;
}
static void install_mouse_provider(HMODULE proxy,InputCreate create,MouseOwnerPath resolve=&mouse_owner_path) {
    if(!proxy || !create) {
        unsigned stage=!proxy?di_no_proxy:di_no_export;
        for(unsigned kind=1;kind<=2;++kind)di_detail(kind,0,stage);
#ifndef NO_RUNTIME_LOG
        InterlockedExchange64(&di_provider_stage,stage);
#endif
        return;
    }
    bool a=try_mouse_interface<IDirectInput8A,IDirectInputDevice8A>(create,1,resolve);
    bool w=try_mouse_interface<IDirectInput8W,IDirectInputDevice8W>(create,2,resolve);
    mouse_interface_success|=(a?1u:0u)|(w?2u:0u);
#ifndef NO_RUNTIME_LOG
    InterlockedExchange64(&di_interface_mask,mouse_interface_success);
    InterlockedExchange64(&di_provider_stage,(a||w)?di_installed:di_untried);
#else
    (void)a;(void)w;
#endif
}
static bool begin_mouse_attempt(ULONGLONG now) {
    // Startup/provider availability must not permanently poison the first attempt.
    if(mouse_interface_success==3 || mouse_attempts>=3 || (mouse_retry_at && now<mouse_retry_at))return false;
    ++mouse_attempts;mouse_retry_at=now+1000;return true;
}
static void install_mouse_filter() {
    if(!begin_mouse_attempt(GetTickCount64()))return;
#ifndef NO_RUNTIME_LOG
    InterlockedExchange64(&di_install_attempted,1);
#endif
    HMODULE proxy=GetModuleHandleW(L"dinput8.dll");
    auto create=proxy?reinterpret_cast<InputCreate>(GetProcAddress(proxy,"DirectInput8Create")):nullptr;
    install_mouse_provider(proxy,create);
}
static void restore_mouse_filter() {
    InterlockedExchange(&wheel_block,0);InterlockedExchange(&block_buttons,0);
    for(auto &t:mouse_tables)if(t.table) {
        DWORD prior=0;if(!VirtualProtect(t.table+9,2*sizeof(void *),PAGE_READWRITE,&prior))continue;
        void *state_before=InterlockedCompareExchangePointer(t.table+9,reinterpret_cast<void *>(t.state),reinterpret_cast<void *>(&filtered_device_state));
        void *data_before=InterlockedCompareExchangePointer(t.table+10,reinterpret_cast<void *>(t.data),reinterpret_cast<void *>(&filtered_device_data));
#ifndef NO_RUNTIME_LOG
        InterlockedIncrement64(&di_restore_counts[state_before==reinterpret_cast<void *>(&filtered_device_state)?0:1]);
        InterlockedIncrement64(&di_restore_counts[data_before==reinterpret_cast<void *>(&filtered_device_data)?0:1]);
#else
        (void)state_before;(void)data_before;
#endif
        DWORD ignored;VirtualProtect(t.table+9,2*sizeof(void *),prior,&ignored);
        if(t.device)t.device->Release();if(t.input)t.input->Release();t={};
    }
    mouse_slots=nullptr;for(const auto &t:mouse_tables)if(t.table)mouse_slots=t.table;
    if(!mouse_slots) {
#ifndef NO_RUNTIME_LOG
        InterlockedExchange64(&di_install_success,0);
        InterlockedExchange64(&di_patched_table,0);
        InterlockedExchange64(&di_interface_kind,0);
        InterlockedExchange64(&di_interface_mask,0);
#endif
        mouse_identity_count=0;
        mouse_interface_success=0;
    }
}
struct MouseFilterLifetime {~MouseFilterLifetime(){restore_mouse_filter();}};
static MouseFilterLifetime mouse_filter_lifetime;
struct Model {std::wstring adviser=L"군사",title=L"열흘 보고",body;unsigned unread=0,page=0,pages=0,generation=0;bool collecting=false,open=false,detail=false;std::vector<std::wstring> names;};
static SRWLOCK lock=SRWLOCK_INIT;
static void enter_model_lock(bool shared) {
    if(shared)AcquireSRWLockShared(&lock);else AcquireSRWLockExclusive(&lock);
#ifndef NO_RUNTIME_LOG
    InterlockedExchange64(&lock_tick_ms,GetTickCount64());
    InterlockedExchange64(&lock_thread_id,GetCurrentThreadId());
#endif
}
static void leave_model_lock(bool shared) {
#ifndef NO_RUNTIME_LOG
    InterlockedExchange64(&lock_thread_id,0);
    InterlockedExchange64(&lock_tick_ms,0);
#endif
    if(shared)ReleaseSRWLockShared(&lock);else ReleaseSRWLockExclusive(&lock);
}
static Model model;
static Model draw_model;
static unsigned model_revision=0,draw_revision=~0u;
static volatile LONG visible,ready,pending,pending_generation,portrait_requested,portrait_revision,portrait_captured;
static Rect source;
static int source_revision,armed=0,max_scroll=0;
static HeadMotion head;
static ScrollMotion scrolling;
static HWND capture_window;
static ULONGLONG scroll_move_at,closed_at,collecting_ended_at,animation_at,badge_at;
static bool was_collecting=false,closing=false,outside_click=false;
static unsigned content_revision=0,painted_content=~0u;
static float close_zoom=1,close_alpha=1;
static unsigned long long raster_calls=0,panel_uploads=0,scene_copies=0,portrait_maps=0,present_serial=0;
static unsigned long long portrait_copy_serial=0;
static ID3D11Texture2D *portrait_candidate,*portrait_staging;
static ID3D11ShaderResourceView *portrait_candidate_view;
static int candidate_revision=0,candidate_width=0,candidate_height=0;
static Rect candidate_source;
static ULONGLONG candidate_at;
static PortraitConfirmation portrait_confirmation; // protected by model lock
static ID3D11Texture2D *body_texture;
static ID3D11ShaderResourceView *body_view;
struct Highlight {ID3D11Texture2D *texture=nullptr;ID3D11ShaderResourceView *view=nullptr;};
static Highlight highlights[8][2][2];
static int body_height=1,body_extent=0;
static int hover=0;
static bool srgb_target=false;
static unsigned arm_generation=0;
static ID3D11Device *owner;
static ID3D11DeviceContext *drawing_context; // borrowed only during draw
static ID3D11VertexShader *vs;
static ID3D11PixelShader *ps;
static ID3D11SamplerState *sampler;
static ID3D11BlendState *blend;
static ID3D11RasterizerState *raster_state,*scissor_state;
static ID3D11DepthStencilState *depth;
static ID3D11Buffer *constants;
static ID3D11Texture2D *portrait_texture;
static ID3D11ShaderResourceView *portrait_view;
static ID3D11Texture2D *panel_texture,*badge_texture;
static ID3D11ShaderResourceView *panel_view,*badge_view;
static ID3D11Texture2D *avatar_texture;
static ID3D11ShaderResourceView *avatar_view;
static ID3D11Texture2D *shadow_texture,*ring_texture;
static ID3D11ShaderResourceView *shadow_view,*ring_view;
static ID3D11Texture2D *stable_scene;
static volatile LONG report_masked,report_tail,scene_clean;
static volatile LONG64 report_window_until;
static unsigned painted_generation=~0u,painted_unread=~0u;
static ULONGLONG opened_at;
static ULONGLONG portrait_attempt_at;
static HFONT fonts[5]{};
#ifndef NO_RUNTIME_LOG
static volatile LONG64 input_posted_at,input_delay_ms,input_max_delay_ms,input_dispatches;
static volatile LONG64 input_posts,input_accepted,input_cancel_generation,input_render_ms,input_max_render_ms,raster_ms,raster_max_ms;
static volatile LONG64 dispatched_input_started;
static ULONGLONG render_input_started;
static unsigned render_input_revision;
#endif
static LONG flag(volatile LONG *v){return InterlockedCompareExchange(v,0,0);}
template<class T>static void drop(T *&p){if(p){p->Release();p=nullptr;}}
static void release_device() {
    InterlockedExchange(&ready,0);InterlockedExchange(&portrait_captured,0);
    enter_model_lock(false);portrait_confirmation.reset();leave_model_lock(false);
    drop(portrait_staging);drop(portrait_candidate_view);drop(portrait_candidate);
    drop(body_view);drop(body_texture);for(auto &by_id:highlights)for(auto &by_state:by_id)for(auto &item:by_state){drop(item.view);drop(item.texture);}
    drop(portrait_view);drop(portrait_texture);drop(panel_view);drop(panel_texture);drop(badge_view);drop(badge_texture);
    drop(avatar_view);drop(avatar_texture);
    drop(shadow_view);drop(shadow_texture);drop(ring_view);drop(ring_texture);
    drop(stable_scene);InterlockedExchange(&report_tail,0);
    drop(vs);drop(ps);drop(sampler);drop(blend);drop(raster_state);drop(scissor_state);drop(depth);drop(constants);drop(owner);
    for(auto &font:fonts)if(font){DeleteObject(font);font=nullptr;}
    painted_generation=~0u;painted_unread=~0u;painted_content=~0u;portrait_attempt_at=0;
}
static bool is_visible(){return flag(&visible)!=0;}
static void mask_report(bool value){InterlockedExchange(&report_masked,value?1:0);}
static bool report_window(){return static_cast<LONG64>(GetTickCount64())<=InterlockedCompareExchange64(&report_window_until,0,0) && InterlockedCompareExchange64(&report_window_until,0,0)!=0;}
static bool needs_draw(){return is_visible() || flag(&report_masked) || flag(&report_tail) || report_window();}
static void progress_flags(bool show,bool clean,bool update_visible=true) {
    InterlockedExchange(&scene_clean,clean?1:0);
    if(update_visible){InterlockedExchange64(&report_window_until,GetTickCount64()+8000);InterlockedExchange(&visible,show?1:0);}
    if(update_visible && !show){InterlockedExchange(&wheel_block,0);InterlockedExchange(&block_buttons,0);}
}
// Called only from an existing native frame, with its top-state decision.
static void player_context(bool player) {
    if(player){InterlockedExchange(&visible,1);return;}
    enter_model_lock(false);
#ifndef NO_RUNTIME_LOG
    if(is_visible() || model.open || closing || outside_click || flag(&wheel_block) || flag(&block_buttons))InterlockedIncrement64(&context_resets);
#endif
    if(model.open){model.open=false;++model_revision;}
    closing=false;outside_click=false;head.cancel();scrolling.reset();armed=hover=0;
    HWND captured_window=capture_window;capture_window=nullptr;
    InterlockedExchange(&pending,0);InterlockedExchange(&visible,0);
    InterlockedExchange(&wheel_block,0);InterlockedExchange(&block_buttons,0);
    leave_model_lock(false);
    if(captured_window && GetCapture()==captured_window)ReleaseCapture();
}
static bool available(){return flag(&ready)!=0;}
static void publish(const Model &next,bool show,bool accepted_input=false) {
    enter_model_lock(false);
    ULONGLONG now=GetTickCount64();
    if(next.generation!=model.generation || next.page!=model.page || next.detail!=model.detail)scrolling.reset();
    if(next.collecting)was_collecting=true;
    else if(was_collecting){collecting_ended_at=now;was_collecting=false;}
    if(next.collecting || (collecting_ended_at && now-collecting_ended_at<=8000))InterlockedExchange64(&report_window_until,next.collecting?now+8000:collecting_ended_at+8000);
    else InterlockedExchange64(&report_window_until,0);
    if(next.open && (!model.open || !is_visible()) && show){opened_at=now;closing=false;}
    if(!next.open && model.open && is_visible() && show){
        double t=(now-opened_at)/220.0;close_zoom=static_cast<float>(.92+.08*ease_back(t));close_alpha=static_cast<float>((std::min)(1.0,t));closed_at=now;closing=true;
    }
    if(next.unread>model.unread)badge_at=now;
    if(next.generation!=model.generation || next.page!=model.page || next.detail!=model.detail || next.pages!=model.pages || next.adviser!=model.adviser || next.title!=model.title || next.body!=model.body || next.names!=model.names || next.collecting!=model.collecting)++content_revision;
    if(next.generation!=model.generation || next.page!=model.page || next.detail!=model.detail || next.pages!=model.pages || next.adviser!=model.adviser || next.title!=model.title || next.body!=model.body || next.names!=model.names || next.collecting!=model.collecting || next.open!=model.open || next.unread!=model.unread){model=next;++model_revision;}
#ifndef NO_RUNTIME_LOG
    if(accepted_input){render_input_started=static_cast<ULONGLONG>(dispatched_input_started);render_input_revision=model_revision;InterlockedIncrement64(&input_accepted);}
#else
    (void)accepted_input;
#endif
    if(!show){outside_click=false;armed=hover=0;head.cancel();scrolling.release();closing=false;InterlockedExchange(&wheel_block,0);InterlockedExchange(&block_buttons,0);}
    leave_model_lock(false);
    InterlockedExchange(&visible,show?1:0);
    if(!show){InterlockedExchange(&pending,0);if(capture_window){HWND window=capture_window;capture_window=nullptr;if(GetCapture()==window)ReleaseCapture();}}
}
static void request_portrait(Rect r,int revision) {
    enter_model_lock(false);
    if(source_revision!=revision || !portrait_rect_matches(source,r))portrait_confirmation.reset();
    source=r;source_revision=revision;InterlockedExchange(&portrait_requested,1);leave_model_lock(false);
}
static bool captured(int revision){return flag(&portrait_captured) && flag(&portrait_revision)==revision;}
static void forget() {
    InterlockedExchange(&ready,0);
    InterlockedExchange(&portrait_captured,0);InterlockedExchange(&portrait_requested,0);InterlockedExchange(&pending,0);
    InterlockedExchange(&visible,0);armed=0;
    enter_model_lock(false);portrait_confirmation.reset();head.cancel();scrolling.reset();closing=false;outside_click=false;leave_model_lock(false);
    if(capture_window){HWND window=capture_window;capture_window=nullptr;if(GetCapture()==window)ReleaseCapture();}
    InterlockedExchange(&report_masked,0);InterlockedExchange(&report_tail,0);
    InterlockedExchange(&scene_clean,0);InterlockedExchange64(&report_window_until,0);InterlockedExchange(&wheel_block,0);InterlockedExchange(&block_buttons,0);
}
static unsigned take(unsigned &generation) {
    // Both are published/consumed under the same lock, never split across cycles.
    enter_model_lock(false);unsigned result=static_cast<unsigned>(flag(&pending));
    generation=static_cast<unsigned>(flag(&pending_generation));InterlockedExchange(&pending,0);
#ifndef NO_RUNTIME_LOG
    if(result) {
        InterlockedExchange64(&dispatched_input_started,input_posted_at);
        LONG64 delay=static_cast<LONG64>(GetTickCount64())-input_posted_at;
        InterlockedExchange64(&input_delay_ms,delay);if(delay>input_max_delay_ms)InterlockedExchange64(&input_max_delay_ms,delay);InterlockedIncrement64(&input_dispatches);
    }
#endif
    leave_model_lock(false);return result;
}
static void post_input(unsigned action,unsigned generation) {
#ifndef NO_RUNTIME_LOG
    InterlockedExchange64(&input_posted_at,static_cast<LONG64>(GetTickCount64()));
    InterlockedIncrement64(&input_posts);
#endif
    InterlockedExchange(&pending_generation,static_cast<LONG>(generation));InterlockedExchange(&pending,static_cast<LONG>(action));
}
static int widget(float x,float y,bool open) {
    float dx=x-head.x,dy=y-head.y;if(dx*dx+dy*dy<=44*44)return 1;
    Rect panel=panel_rect(head.x);double zoom=model.open?.92+.08*ease_back((GetTickCount64()-opened_at)/220.0):1;
    if(!open || !panel_transform(panel,head.x,head.y,zoom).contains(x,y))return 0;
    for(unsigned id=2;id<=7;++id)if(panel_transform(control(id,panel),head.x,head.y,zoom).contains(x,y))return static_cast<int>(id);
    return 9;
}
static bool input(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
#ifndef NO_RUNTIME_LOG
    InterlockedExchange64(&wndproc_tick_ms,GetTickCount64());
    InterlockedExchange64(&wndproc_thread_id,GetCurrentThreadId());
#endif
    if(message==WM_CAPTURECHANGED && reinterpret_cast<HWND>(lparam)==hwnd)return false;
    if(message==WM_KILLFOCUS || message==WM_CAPTURECHANGED || message==WM_SIZE || message==WM_DPICHANGED){
        enter_model_lock(false);bool consume=head.dragging || scrolling.dragging;
        if(message==WM_CAPTURECHANGED){head.release();animation_at=GetTickCount64();}else head.cancel();
        scrolling.release();outside_click=false;armed=hover=0;capture_window=nullptr;leave_model_lock(false);
        InterlockedExchange(&wheel_block,0);InterlockedExchange(&block_buttons,0);
        if(message!=WM_CAPTURECHANGED && GetCapture()==hwnd)ReleaseCapture();return consume;
    }
    if(!is_visible() || !available()){InterlockedExchange(&wheel_block,0);InterlockedExchange(&block_buttons,0);return false;}
    if(message!=WM_MOUSEMOVE && message!=WM_LBUTTONDOWN && message!=WM_LBUTTONUP && message!=WM_LBUTTONDBLCLK && message!=WM_RBUTTONDOWN && message!=WM_RBUTTONUP && message!=WM_MOUSEWHEEL && message!=WM_KEYDOWN && message!=WM_KEYUP)return false;
    enter_model_lock(false);
    ULONGLONG now=GetTickCount64();
    if(message==WM_KEYDOWN || message==WM_KEYUP) {
        bool handle=model.open && !model.collecting && wparam==VK_ESCAPE;
        if(handle && message==WM_KEYUP)post_input(2,model.generation);
        leave_model_lock(false);return handle;
    }
    RECT client{};GetClientRect(hwnd,&client);POINT p{static_cast<short>(LOWORD(lparam)),static_cast<short>(HIWORD(lparam))};
    if(message==WM_MOUSEWHEEL)ScreenToClient(hwnd,&p);
    float x=0,y=0;bool mapped=client_point(p.x,p.y,client.right,client.bottom,unified_overlay::buffer_width,unified_overlay::buffer_height,x,y);
    bool closing_now=closing && now-closed_at<140;
    int hit=mapped?widget(x,y,(model.open || closing_now) && !model.collecting):0;hover=hit;
    InterlockedExchange(&wheel_block,hit!=0?1:0);
    // Preserve the complete dismissing click even after native dispatch closed
    // the panel and its animation expired. Capture guarantees its matching UP.
    if(outside_click){
        bool release=message==WM_LBUTTONUP;
        if(release){outside_click=false;InterlockedExchange(&block_buttons,0);
#ifndef NO_RUNTIME_LOG
            InterlockedIncrement64(&outside_click_counts[1]);
#endif
        }
        leave_model_lock(false);
        if(release){capture_window=nullptr;if(GetCapture()==hwnd)ReleaseCapture();}
        return true;
    }
    if(message==WM_LBUTTONDOWN && mapped && !hit && model.open && !model.collecting && !closing_now &&
       !head.pressed && !head.dragging && !head.snapping && !scrolling.pressed && !scrolling.active() && !armed){
        outside_click=true;post_input(2,model.generation);InterlockedExchange(&block_buttons,1);
#ifndef NO_RUNTIME_LOG
        InterlockedIncrement64(&outside_click_counts[0]);
#endif
        leave_model_lock(false);capture_window=hwnd;SetCapture(hwnd);return true;
    }
    if(closing_now && !head.pressed && !scrolling.pressed){leave_model_lock(false);return hit!=0;}
    bool capture=false,release=false,consume=hit!=0 || (closing_now && (head.pressed || scrolling.pressed));
    if(message==WM_MOUSEMOVE && head.pressed){
        bool before=head.dragging;if(mapped)head.move(x,y);
        if(!before && head.dragging && model.open)post_input(2,model.generation);
        consume=true;
    } else if(message==WM_MOUSEMOVE && scrolling.pressed){
        if(mapped)scrolling.move(y,(now-scroll_move_at)/1000.0,max_scroll);scroll_move_at=now;consume=true;
    } else if(message==WM_MOUSEWHEEL && hit==9){scrolling.wheel(GET_WHEEL_DELTA_WPARAM(wparam)>0?-80:80,max_scroll);animation_at=now;}
    else if(message==WM_LBUTTONDOWN || message==WM_LBUTTONDBLCLK){
        armed=hit;arm_generation=model.generation;
        if(hit==1){head.press(x,y);animation_at=now;capture=true;}
        else if(hit==9 && body_rect(panel_rect(head.x)).contains(x,y)){scrolling.press(y);scroll_move_at=now;capture=true;}
    } else if(message==WM_LBUTTONUP){
        bool had_head=head.pressed,had_body=scrolling.pressed;
        bool was_drag=head.dragging || head.snapping;
        if(had_head && mapped){bool before=head.dragging;head.move(x,y);if(!before && head.dragging && model.open)post_input(2,model.generation);was_drag=was_drag || head.dragging;}
        bool tap=had_head?head.release():false;
        if(had_body)scrolling.release();if(had_head || had_body)animation_at=now;
        bool fire=hit && hit==armed && arm_generation==model.generation && (!had_head || tap) && !model.collecting;
        bool consumed=armed!=0 || had_head || had_body || was_drag;release=had_head || had_body;
#ifndef NO_RUNTIME_LOG
        if(hit && consumed && !fire && arm_generation!=model.generation)InterlockedIncrement64(&input_cancel_generation);
#endif
        armed=0;if(fire && hit!=9)post_input(hit,model.generation);consume=consume || consumed;
    } else if(message==WM_MOUSEMOVE && head.snapping)consume=true;
    InterlockedExchange(&block_buttons,(armed || head.pressed || head.dragging || scrolling.pressed || scrolling.dragging)?1:0);
    leave_model_lock(false);
    // SetCapture/ReleaseCapture can synchronously reenter WndProc: never hold lock.
    if(capture){capture_window=hwnd;SetCapture(hwnd);}
    if(release){capture_window=nullptr;if(GetCapture()==hwnd)ReleaseCapture();}
    return consume;
}
static bool initialize(ID3D11Device *d) {
    if(owner==d && vs && ps && sampler && blend && constants && raster_state && scissor_state && depth)return true;
    release_device();owner=d;owner->AddRef();
    if(FAILED(d->CreateVertexShader(advisor_vs,sizeof(advisor_vs),nullptr,&vs)) || FAILED(d->CreatePixelShader(advisor_ps,sizeof(advisor_ps),nullptr,&ps)))return false;
    D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
    D3D11_BLEND_DESC bd{};auto &rt=bd.RenderTarget[0];rt.BlendEnable=TRUE;rt.SrcBlend=D3D11_BLEND_ONE;rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOp=D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha=D3D11_BLEND_ONE;rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
    D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;
    D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=FALSE;dd.StencilEnable=FALSE;
    D3D11_BUFFER_DESC cb{};cb.ByteWidth=16;cb.Usage=D3D11_USAGE_DEFAULT;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    rd.ScissorEnable=TRUE;if(FAILED(d->CreateRasterizerState(&rd,&scissor_state)))return false;rd.ScissorEnable=FALSE;
    return SUCCEEDED(d->CreateSamplerState(&sd,&sampler)) && SUCCEEDED(d->CreateBlendState(&bd,&blend)) && SUCCEEDED(d->CreateRasterizerState(&rd,&raster_state)) && SUCCEEDED(d->CreateDepthStencilState(&dd,&depth)) && SUCCEEDED(d->CreateBuffer(&cb,nullptr,&constants));
}
static bool upload(ID3D11DeviceContext *c,ID3D11Texture2D *&t,ID3D11ShaderResourceView *&v,unsigned w,unsigned h,const unsigned *pixels) {
    if(t){D3D11_TEXTURE2D_DESC prior{};t->GetDesc(&prior);if(prior.Width!=w || prior.Height!=h){drop(v);drop(t);}}
    if(!t){D3D11_TEXTURE2D_DESC td{};td.Width=w;td.Height=h;td.MipLevels=td.ArraySize=1;td.Format=srgb_target?DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:DXGI_FORMAT_B8G8R8A8_UNORM;td.SampleDesc.Count=1;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        if(FAILED(owner->CreateTexture2D(&td,nullptr,&t)) || FAILED(owner->CreateShaderResourceView(t,nullptr,&v)))return false;}
    c->UpdateSubresource(t,0,nullptr,pixels,w*4,0);return true;
}
static HFONT serif_font(HDC dc,int height,int weight=FW_NORMAL) {
    for(const wchar_t *family:{L"Noto Serif KR",L"함초롬바탕",L"바탕"}) {
        HFONT font=CreateFontW(-height,0,0,0,weight,FALSE,FALSE,FALSE,HANGUL_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,family);
        if(!font)continue;auto old=SelectObject(dc,font);wchar_t actual[LF_FACESIZE]{};GetTextFaceW(dc,LF_FACESIZE,actual);SelectObject(dc,old);
        if(_wcsicmp(actual,family)==0)return font;DeleteObject(font);
    }
    return CreateFontW(-height,0,0,0,weight,FALSE,FALSE,FALSE,HANGUL_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Batang");
}
static HFONT cached_font(HDC dc,unsigned kind) {
    const int heights[]={23,27,18,40,21};const int weights[]={FW_SEMIBOLD,FW_SEMIBOLD,FW_MEDIUM,FW_NORMAL,FW_BOLD};
    if(!fonts[kind])fonts[kind]=serif_font(dc,heights[kind],weights[kind]);
    return fonts[kind];
}
struct TextLine {std::wstring text;int x,y;bool bullet;int right=0;bool table=false,header=false;};
static std::vector<TextLine> body_lines(HDC dc,const std::wstring &body,int width,int &height) {
    // User 2026-10-27: one reading edge, generous leading, and hanging bullets.
    // Use GDI's measured glyph widths; never split a UTF-16 surrogate pair.
    std::vector<TextLine> lines;height=0;
    for(size_t start=0;start<body.size();) {
        size_t end=body.find(L'\n',start);if(end==std::wstring::npos)end=body.size();
        std::wstring paragraph=body.substr(start,end-start);if(!paragraph.empty() && paragraph.back()==L'\r')paragraph.pop_back();
        bool bullet=paragraph.size()>=2 && paragraph[0]==L'•' && paragraph[1]==L' ';
        if(bullet)paragraph.erase(0,2);
        size_t split=paragraph.rfind(L"  ·  ");bool amount=false;
        if(bullet && split!=std::wstring::npos && split+5<paragraph.size()) {
            amount=true;for(size_t i=split+5;i<paragraph.size();++i)if((paragraph[i]<L'0' || paragraph[i]>L'9') && paragraph[i]!=L',')amount=false;
            SIZE name{};GetTextExtentPoint32W(dc,paragraph.c_str(),static_cast<int>(split),&name);
            amount=amount && name.cx<240 && width>=600;
        }
        if(paragraph.find(L'\t')!=std::wstring::npos) {
            std::vector<std::wstring> cells;size_t from=0;
            for(;;){size_t stop=paragraph.find(L'\t',from);cells.push_back(paragraph.substr(from,stop==std::wstring::npos?stop:stop-from));if(stop==std::wstring::npos)break;from=stop+1;}
            if(cells.size()==5) {
                int span=width-24;bool header=cells[0]==L"항목";
                for(unsigned i=0;i<5;++i) {
                    int left=80+static_cast<int>(span*i/5),right=80+static_cast<int>(span*(i+1)/5)-20;
                    lines.push_back({cells[i],left,height,false,i?right:0,true,header});
                }
                height+=42;start=end<body.size()?end+1:end;continue;
            }
        }
        if(paragraph.empty())height+=18;
        else if(amount) {
            lines.push_back({paragraph.substr(0,split),80,height,true});
            lines.push_back({paragraph.substr(split+5),328,height,false,440});height+=36;
        }
        else for(size_t at=0;at<paragraph.size();) {
            int fit=0;SIZE extent{};int room=width-(bullet?24:0);
            GetTextExtentExPointW(dc,paragraph.c_str()+at,static_cast<int>(paragraph.size()-at),room,&fit,nullptr,&extent);
            size_t count=(std::max)(size_t(1),static_cast<size_t>((std::max)(0,fit)));
            count=(std::min)(count,paragraph.size()-at);
            if(at+count<paragraph.size()) {
                size_t space=paragraph.rfind(L' ',at+count-1);
                if(space!=std::wstring::npos && space>at+count/2)count=space-at;
                if(count && paragraph[at+count-1]>=0xD800 && paragraph[at+count-1]<=0xDBFF)--count;
                if(!count)count=(std::min)(size_t(2),paragraph.size()-at);
            }
            std::wstring line=paragraph.substr(at,count);while(!line.empty() && line.back()==L' ')line.pop_back();
            lines.push_back({std::move(line),56+(bullet?24:0),height,bullet && at==0});height+=36;
            at+=count;while(at<paragraph.size() && paragraph[at]==L' ')++at;
        }
        start=end<body.size()?end+1:end;
    }
    return lines;
}
enum class Layer {Chrome,Body,Badge,Avatar,Highlight};
static bool raster(ID3D11DeviceContext *c,const Model &m,Layer layer,unsigned button=0,int state=0) {
    ++raster_calls;
    bool badge=layer==Layer::Badge,avatar=layer==Layer::Avatar,body=layer==Layer::Body,highlight=layer==Layer::Highlight;
    int w=avatar?88:badge?38:static_cast<int>(bubble.w),h=avatar?88:badge?38:static_cast<int>(bubble.h);
    int height=body_extent,room=static_cast<int>(bubble.h)-382;
    std::vector<TextLine> lines;
    if(body){HDC measure=CreateCompatibleDC(nullptr);if(!measure)return false;auto prior=SelectObject(measure,cached_font(measure,0));lines=body_lines(measure,m.body,w-112,height);SelectObject(measure,prior);DeleteDC(measure);body_extent=height;body_height=(std::max)(1,(std::min)(8192,height));enter_model_lock(false);max_scroll=(std::max)(0,body_height-room);leave_model_lock(false);}
    Rect button_rect=control(button);int bx=static_cast<int>(button_rect.x-bubble.x),by=static_cast<int>(button_rect.y-bubble.y);
    int rw=body?w-112:highlight?static_cast<int>(button_rect.w):w,rh=body?body_height:highlight?static_cast<int>(button_rect.h):h;
    int hot=highlight && state==0?static_cast<int>(button):0,pressed=highlight && state==1?static_cast<int>(button):0;
    if(pressed)hot=pressed;
#ifndef NO_RUNTIME_LOG
    ULONGLONG started=GetTickCount64();
#endif
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=rw;bi.bmiHeader.biHeight=-rh;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return false;void *bits=nullptr;HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bm || !bits){if(bm)DeleteObject(bm);DeleteDC(dc);return false;}
    auto old=SelectObject(dc,bm);auto data=static_cast<unsigned *>(bits);
    for(int y=0;y<rh;++y)for(int x=0;x<rw;++x) {
        int gy=highlight?y+by:y;
        unsigned r=static_cast<unsigned>(31-12*gy/(std::max)(1,h-1)),g=static_cast<unsigned>(36-12*gy/(std::max)(1,h-1)),b=static_cast<unsigned>(30-8*gy/(std::max)(1,h-1));
        data[y*rw+x]=body || (highlight && button==7)?0xffece5d3u:badge?0xffa3392cu:0xff000000u|(r<<16)|(g<<8)|b;
    }
    if(body)SetViewportOrgEx(dc,-56,0,nullptr);else if(highlight)SetViewportOrgEx(dc,-bx,-by,nullptr);
    SetBkMode(dc,TRANSPARENT);HFONT font=cached_font(dc,avatar?3:badge?4:0);auto oldfont=SelectObject(dc,font);
    HFONT heading=nullptr,caption_font=nullptr;
    auto text=[&](const std::wstring &s,RECT r,COLORREF color,UINT format){SetTextColor(dc,color);DrawTextW(dc,s.c_str(),static_cast<int>(s.size()),&r,format|DT_NOPREFIX);};
    auto outline=[&](RECT r,COLORREF color,COLORREF fill,int radius,bool filled){HPEN pen=CreatePen(PS_SOLID,1,color);HBRUSH brush=filled?CreateSolidBrush(fill):nullptr;auto op=SelectObject(dc,pen),ob=SelectObject(dc,filled?brush:GetStockObject(NULL_BRUSH));RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,ob);SelectObject(dc,op);if(brush)DeleteObject(brush);DeleteObject(pen);};
    if(avatar)text(L"軍",{0,0,w,h},RGB(216,190,132),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    else if(badge){outline({1,1,w-1,h-1},RGB(211,157,97),0,38,false);text(m.unread>99?L"99+":std::to_wstring(m.unread),{0,0,w,h},RGB(255,242,212),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    else {
        heading=cached_font(dc,1);caption_font=cached_font(dc,2);
        if(!body && !highlight){
        outline({1,1,w-1,h-1},RGB(142,124,85),0,24,false);
        outline({16,132,w-16,h-96},RGB(192,176,141),RGB(236,229,211),16,true);
        HPEN rule=CreatePen(PS_SOLID,1,RGB(197,183,153));auto op=SelectObject(dc,rule);
        MoveToEx(dc,56,222,nullptr);LineTo(dc,w-56,222);SelectObject(dc,op);DeleteObject(rule);
        HBRUSH accent=CreateSolidBrush(RGB(192,164,107));RECT bar{44,34,47,86};FillRect(dc,&bar,accent);DeleteObject(accent);
        SelectObject(dc,heading);text(m.adviser+L"의 열흘 보고",{64,30,w-354,68},RGB(235,218,180),DT_SINGLELINE|DT_VCENTER);
        SelectObject(dc,caption_font);text(m.collecting?L"진행 중  /  소식을 모으고 있습니다":L"지난 순의 전갈  /  다음 진행 전까지 보관",{65,76,w-80,105},RGB(162,166,144),DT_SINGLELINE);
        SelectObject(dc,font);text(m.title,{56,158,w-174,194},RGB(54,59,48),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
        }
        if(body){
        for(const auto &line:lines) {
            int y=line.y;if(y>=rh)continue;
            if(line.table && line.x==80) {
                if(line.header){RECT band{72,y-3,w-48,y+36};HBRUSH fill=CreateSolidBrush(RGB(219,209,185));FillRect(dc,&band,fill);DeleteObject(fill);}
                else {HPEN row_rule=CreatePen(PS_SOLID,1,RGB(214,204,182));auto prior=SelectObject(dc,row_rule);MoveToEx(dc,80,y+36,nullptr);LineTo(dc,w-56,y+36);SelectObject(dc,prior);DeleteObject(row_rule);}
            }
            if(line.bullet)text(L"·",{56,y,74,y+32},RGB(145,111,57),DT_SINGLELINE);
            COLORREF ink=RGB(30,35,29);
            if(line.table && !line.header && line.text.find(L'+')!=std::wstring::npos)ink=RGB(39,95,56);
            else if(line.table && !line.header && line.text.find(L'-')!=std::wstring::npos)ink=RGB(139,48,43);
            text(line.text,{line.x,y,line.right?line.right:w-56,y+32},ink,DT_SINGLELINE|(line.right?DT_RIGHT:0));
            SIZE full{};GetTextExtentPoint32W(dc,line.text.c_str(),static_cast<int>(line.text.size()),&full);int origin=line.right?line.right-full.cx:line.x;
            const COLORREF palette[]={RGB(16,84,103),RGB(135,44,48),RGB(37,70,133),RGB(44,100,55),RGB(104,57,132),RGB(133,80,20)};
            for(const auto &name:m.names)for(size_t at=0;(at=line.text.find(name,at))!=std::wstring::npos;at+=name.size()) {
                SIZE prefix{};GetTextExtentPoint32W(dc,line.text.c_str(),static_cast<int>(at),&prefix);
                SetTextColor(dc,palette[name_color_index(name)]);TextOutW(dc,origin+prefix.cx,y,name.c_str(),static_cast<int>(name.size()));
            }
        }
        }
        if(!body){
        for(unsigned id=2;id<=7;++id) {
            if(highlight && id!=button)continue;
            Rect r=control(id);RECT box{static_cast<LONG>(r.x-bubble.x),static_cast<LONG>(r.y-bubble.y),static_cast<LONG>(r.x-bubble.x+r.w),static_cast<LONG>(r.y-bubble.y+r.h)};
            bool over=hot==static_cast<int>(id),down=pressed==static_cast<int>(id);
            if(id==7)outline(box,over?RGB(154,127,78):RGB(199,184,154),down?RGB(208,193,161):over?RGB(224,212,187):RGB(236,229,211),8,true);
            else if(id==3 || id==4 || over)outline(box,over?RGB(169,146,98):RGB(77,85,65),down?RGB(72,65,44):RGB(39,47,36),8,true);
            SelectObject(dc,id==2 || id==3 || id==4?heading:caption_font);
            const wchar_t *label=id==2?L"×":id==3?L"‹":id==4?L"›":id==5?L"지우기":id==6?L"모두 지우기":m.detail?L"요약 보기":L"원문 보기";
            text(label,box,id==7?RGB(103,84,51):over?RGB(248,222,170):RGB(187,186,156),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }
        if(!highlight){
        SelectObject(dc,caption_font);text(m.pages?std::to_wstring(m.page+1)+L" / "+std::to_wstring(m.pages):L"소식 없음",{w-272,30,w-164,68},RGB(199,199,170),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        text(L"Esc로 닫기",{44,h-64,250,h-26},RGB(161,167,143),DT_VCENTER|DT_SINGLELINE);
        if(height>room)text(L"마우스 휠로 이어 읽기",{300,h-64,w-300,h-26},RGB(141,150,128),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
        }
        }
    // GDI clears alpha for glyph pixels; restore opaque alpha before the shader's
    // geometric mask. There are no black rectangles around the circular portrait.
    for(int i=0;i<rw*rh;++i)data[i]|=0xff000000u;
    bool ok=false;
    if(avatar)ok=upload(c,avatar_texture,avatar_view,rw,rh,data);
    else if(badge)ok=upload(c,badge_texture,badge_view,rw,rh,data);
    else if(body){ok=upload(c,body_texture,body_view,rw,rh,data);if(ok)++panel_uploads;}
    else if(highlight){auto &item=highlights[button][state][m.detail?1:0];ok=upload(c,item.texture,item.view,rw,rh,data);}
    else {ok=upload(c,panel_texture,panel_view,rw,rh,data);if(ok)++panel_uploads;}
    SelectObject(dc,oldfont);SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);
#ifndef NO_RUNTIME_LOG
    LONG64 elapsed=static_cast<LONG64>(GetTickCount64()-started);InterlockedExchange64(&raster_ms,elapsed);if(elapsed>raster_max_ms)InterlockedExchange64(&raster_max_ms,elapsed);
#endif
    return ok;
}
// Own only the bindings touched below. COM Get calls AddRef, released after restore.
struct PipelineState {
    ID3D11DeviceContext *c;
    ID3D11RenderTargetView *targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};ID3D11DepthStencilView *dsv=nullptr;
    ID3D11BlendState *blend_state=nullptr;FLOAT factors[4]{};UINT mask=0;
    ID3D11DepthStencilState *depth_state=nullptr;UINT stencil=0;
    ID3D11RasterizerState *rs=nullptr;UINT viewport_count=D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE,scissor_count=D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
    D3D11_RECT scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
    D3D11_PRIMITIVE_TOPOLOGY topology{};ID3D11InputLayout *layout=nullptr;
    ID3D11VertexShader *vertex=nullptr;ID3D11PixelShader *pixel=nullptr;
    ID3D11GeometryShader *geometry=nullptr;ID3D11HullShader *hull=nullptr;ID3D11DomainShader *domain=nullptr;
    ID3D11ClassInstance *vc[256]{},*pc[256]{},*gc[256]{},*hc[256]{},*dc[256]{};
    UINT vn=256,pn=256,gn=256,hn=256,dn=256;
    ID3D11Buffer *vbuffer=nullptr,*pbuffer=nullptr;ID3D11ShaderResourceView *resource=nullptr;ID3D11SamplerState *sample=nullptr;
    explicit PipelineState(ID3D11DeviceContext *context):c(context) {
        c->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,targets,&dsv);c->OMGetBlendState(&blend_state,factors,&mask);c->OMGetDepthStencilState(&depth_state,&stencil);
        c->RSGetState(&rs);c->RSGetViewports(&viewport_count,viewports);c->RSGetScissorRects(&scissor_count,scissors);c->IAGetPrimitiveTopology(&topology);c->IAGetInputLayout(&layout);
        c->VSGetShader(&vertex,vc,&vn);c->PSGetShader(&pixel,pc,&pn);c->GSGetShader(&geometry,gc,&gn);c->HSGetShader(&hull,hc,&hn);c->DSGetShader(&domain,dc,&dn);
        c->VSGetConstantBuffers(0,1,&vbuffer);c->PSGetConstantBuffers(0,1,&pbuffer);c->PSGetShaderResources(0,1,&resource);c->PSGetSamplers(0,1,&sample);
    }
    ~PipelineState() {
        c->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,targets,dsv);c->OMSetBlendState(blend_state,factors,mask);c->OMSetDepthStencilState(depth_state,stencil);
        c->RSSetState(rs);c->RSSetViewports(viewport_count,viewports);c->RSSetScissorRects(scissor_count,scissors);c->IASetPrimitiveTopology(topology);c->IASetInputLayout(layout);
        c->VSSetShader(vertex,vc,vn);c->PSSetShader(pixel,pc,pn);c->GSSetShader(geometry,gc,gn);c->HSSetShader(hull,hc,hn);c->DSSetShader(domain,dc,dn);
        c->VSSetConstantBuffers(0,1,&vbuffer);c->PSSetConstantBuffers(0,1,&pbuffer);c->PSSetShaderResources(0,1,&resource);c->PSSetSamplers(0,1,&sample);
        for(auto &t:targets)drop(t);drop(dsv);drop(blend_state);drop(depth_state);drop(rs);drop(layout);drop(vertex);drop(pixel);drop(geometry);drop(hull);drop(domain);
        for(auto &v:vc)drop(v);for(auto &v:pc)drop(v);for(auto &v:gc)drop(v);for(auto &v:hc)drop(v);for(auto &v:dc)drop(v);
        drop(vbuffer);drop(pbuffer);drop(resource);drop(sample);
    }
};
static void quad(Rect r,double scale,double ox,double oy,float radius,float opacity,ID3D11ShaderResourceView *view) {
#ifndef NO_RUNTIME_LOG
    ++quad_calls;
#endif
    D3D11_VIEWPORT viewport{static_cast<float>(ox+r.x*scale),static_cast<float>(oy+r.y*scale),static_cast<float>(r.w*scale),static_cast<float>(r.h*scale),0,1};
    drawing_context->RSSetViewports(1,&viewport);float values[]={viewport.Width,viewport.Height,radius<0?radius:static_cast<float>(radius*scale),opacity};
    drawing_context->UpdateSubresource(constants,0,nullptr,values,0,0);drawing_context->PSSetShaderResources(0,1,&view);drawing_context->Draw(3,0);
}
static void poll_portrait(ID3D11DeviceContext *c) {
    if(!portrait_staging || present_serial<=portrait_copy_serial)return;
    D3D11_MAPPED_SUBRESOURCE mapped{};++portrait_maps;
    HRESULT result=c->Map(portrait_staging,0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
    if(result==DXGI_ERROR_WAS_STILL_DRAWING)return;
    bool valid=false;PortraitSample sample;
    if(SUCCEEDED(result)){
        valid=portrait_sample_from_pixels(mapped.pData,mapped.RowPitch,candidate_width,candidate_height,sample);
        c->Unmap(portrait_staging,0);
    }
    enter_model_lock(false);
    if(valid && candidate_revision==source_revision && portrait_rect_matches(candidate_source,source) && flag(&portrait_requested)){
        unsigned diff=0;bool accepted=portrait_confirmation.observe(sample,candidate_revision,candidate_source,candidate_at,diff);
        // Start the confirmation interval when the CPU sample becomes available.
        if(!accepted && portrait_confirmation.at==candidate_at)portrait_confirmation.at=GetTickCount64();
#ifndef NO_RUNTIME_LOG
        log_event("advisor_portrait_sample",candidate_revision,diff,accepted?1:0);
#endif
        if(accepted){
            drop(portrait_view);drop(portrait_texture);portrait_texture=portrait_candidate;portrait_candidate=nullptr;portrait_view=portrait_candidate_view;portrait_candidate_view=nullptr;
            InterlockedExchange(&portrait_revision,source_revision);InterlockedExchange(&portrait_captured,1);InterlockedExchange(&portrait_requested,0);
        }
    }
    leave_model_lock(false);
    drop(portrait_staging);drop(portrait_candidate_view);drop(portrait_candidate);
}
static void capture_portrait(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc,int x,int y,int w,int h,int revision,Rect source_rect) {
    if(portrait_staging || desc.SampleDesc.Count!=1 || (desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM && desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB && desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM && desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB))return;
    D3D11_TEXTURE2D_DESC td=desc;td.Width=w;td.Height=h;td.MipLevels=td.ArraySize=1;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;td.CPUAccessFlags=td.MiscFlags=0;td.Usage=D3D11_USAGE_DEFAULT;
    if(FAILED(d->CreateTexture2D(&td,nullptr,&portrait_candidate)) || FAILED(d->CreateShaderResourceView(portrait_candidate,nullptr,&portrait_candidate_view))){drop(portrait_candidate_view);drop(portrait_candidate);return;}
    td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    if(FAILED(d->CreateTexture2D(&td,nullptr,&portrait_staging))){drop(portrait_candidate_view);drop(portrait_candidate);return;}
    D3D11_BOX box{static_cast<UINT>(x),static_cast<UINT>(y),0,static_cast<UINT>(x+w),static_cast<UINT>(y+h),1};
    c->CopySubresourceRegion(portrait_candidate,0,0,0,0,back,0,&box);c->CopyResource(portrait_staging,portrait_candidate);
    portrait_copy_serial=present_serial;candidate_revision=revision;candidate_width=w;candidate_height=h;candidate_source=source_rect;candidate_at=GetTickCount64();
}
static void draw(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc) {
#ifndef NO_RUNTIME_LOG
    InterlockedExchange64(&present_tick_ms,GetTickCount64());
    InterlockedExchange64(&present_thread_id,GetCurrentThreadId());
    DrawTiming timing;
#endif
    if(!needs_draw() || !initialize(d))return;
    install_mouse_filter();
    ++present_serial;ULONGLONG now=GetTickCount64();
    enter_model_lock(false);
    if(draw_revision!=model_revision){draw_model=model;draw_revision=model_revision;}
    double dt=animation_at?(now-animation_at)/1000.0:0;
    if(head.snapping || scrolling.active() || scrolling.shown>max_scroll)animation_at=now;else animation_at=0;
    if(head.snapping)head.advance(dt);
    if(scrolling.active() || scrolling.shown>max_scroll)scrolling.advance(dt,max_scroll);
    if(closing && now-closed_at>=140)closing=false;
    HeadMotion position=head;double sy=scrolling.shown;int hot=hover,pressed=armed;
    bool scroll_active=scrolling.active();
    unsigned content=content_revision;ULONGLONG started=opened_at,ended=collecting_ended_at,pop_at=badge_at;
    bool is_closing=closing;ULONGLONG close_at=closed_at;float closing_zoom=close_zoom,closing_alpha=close_alpha;
#ifndef NO_RUNTIME_LOG
    ULONGLONG input_started=render_input_started;unsigned input_revision=render_input_revision;
#endif
    leave_model_lock(false);const Model &m=draw_model;
    // Preserve the clean scene through the original pre-report mask and two tails.
    D3D11_TEXTURE2D_DESC saved{};if(stable_scene)stable_scene->GetDesc(&saved);
    if(stable_scene && (saved.Width!=desc.Width || saved.Height!=desc.Height || saved.Format!=desc.Format || saved.SampleDesc.Count!=desc.SampleDesc.Count))drop(stable_scene);
    bool masking=flag(&report_masked)!=0,tail=flag(&report_tail)!=0;
    if(masking)InterlockedExchange(&report_tail,2);
    if(masking || tail){
        if(stable_scene){c->CopyResource(back,stable_scene);++scene_copies;}
        if(!masking)InterlockedDecrement(&report_tail);
    } else if(flag(&scene_clean) && (m.collecting || (ended && now-ended<=8000))){
        if(!stable_scene){auto td=desc;td.BindFlags=td.CPUAccessFlags=td.MiscFlags=0;td.Usage=D3D11_USAGE_DEFAULT;d->CreateTexture2D(&td,nullptr,&stable_scene);}
        if(stable_scene){c->CopyResource(stable_scene,back);++scene_copies;}
    }
    if(!is_visible())return;
    bool next_srgb=desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if(srgb_target!=next_srgb) {
        enter_model_lock(false);portrait_confirmation.reset();leave_model_lock(false);
        srgb_target=next_srgb;drop(body_view);drop(body_texture);
        for(auto &by_id:highlights)for(auto &by_state:by_id)for(auto &item:by_state){drop(item.view);drop(item.texture);}
        drop(portrait_staging);drop(portrait_candidate_view);drop(portrait_candidate);drop(panel_view);drop(panel_texture);drop(badge_view);drop(badge_texture);drop(avatar_view);drop(avatar_texture);
        drop(shadow_view);drop(shadow_texture);drop(ring_view);drop(ring_texture);drop(portrait_view);drop(portrait_texture);InterlockedExchange(&portrait_captured,0);
        painted_generation=painted_unread=~0u;painted_content=~0u;
    }
    double scale=(std::min)(desc.Width/1920.0,desc.Height/1080.0),ox=(desc.Width-1920*scale)/2,oy=(desc.Height-1080*scale)/2;
    poll_portrait(c);
    enter_model_lock(true);bool sample_due=!portrait_confirmation.present || (now>=portrait_confirmation.at && now-portrait_confirmation.at>=500);leave_model_lock(true);
    if(flag(&portrait_requested) && sample_due && !portrait_staging && (!portrait_attempt_at || now-portrait_attempt_at>=120)){
        portrait_attempt_at=now;enter_model_lock(true);Rect source_rect=source;int revision=source_revision;leave_model_lock(true);
        int x=static_cast<int>(ox+source_rect.x*scale),y=static_cast<int>(oy+source_rect.y*scale),w=static_cast<int>(source_rect.w*scale),h=static_cast<int>(source_rect.h*scale);
        if(x>=0 && y>=0 && w>8 && h>8 && x+w<=static_cast<int>(desc.Width) && y+h<=static_cast<int>(desc.Height))capture_portrait(d,c,back,desc,x,y,w,h,revision,source_rect);
    }
    if((!flag(&portrait_captured) || !portrait_view) && !avatar_view && !raster(c,m,Layer::Avatar))return;
    if(m.open && (painted_content!=content || !panel_view || !body_view)){
        if(!raster(c,m,Layer::Body) || !raster(c,m,Layer::Chrome))return;
        for(unsigned id=2;id<=7;++id)for(int state=0;state<2;++state)if(!highlights[id][state][m.detail?1:0].view && !raster(c,m,Layer::Highlight,id,state))return;
        painted_content=content;painted_generation=m.generation;
    }
    if(painted_unread!=m.unread){if(m.unread && !raster(c,m,Layer::Badge))return;painted_unread=m.unread;}
    bool idle=!m.open && !is_closing && !position.pressed && !position.dragging && !position.snapping && !scroll_active && (!pop_at || now-pop_at>=250);
    unsigned black=0xff000000u,gold=0xffb79960u;
    if(!idle && ((!shadow_view && !upload(c,shadow_texture,shadow_view,1,1,&black)) || (!ring_view && !upload(c,ring_texture,ring_view,1,1,&gold))))return;
    ID3D11RenderTargetView *target=nullptr;if(FAILED(d->CreateRenderTargetView(back,nullptr,&target)))return;
    PipelineState saved_pipeline(c);drawing_context=c;
    c->OMSetRenderTargets(1,&target,nullptr);c->OMSetBlendState(blend,nullptr,~0u);c->OMSetDepthStencilState(depth,0);
    c->RSSetState(raster_state);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);c->IASetInputLayout(nullptr);
    c->GSSetShader(nullptr,nullptr,0);c->HSSetShader(nullptr,nullptr,0);c->DSSetShader(nullptr,nullptr,0);
    c->VSSetShader(vs,nullptr,0);c->PSSetShader(ps,nullptr,0);c->PSSetSamplers(0,1,&sampler);c->PSSetConstantBuffers(0,1,&constants);
    double panel_alpha=0,zoom=1;
    if(m.open){double t=(now-started)/220.0;panel_alpha=(std::min)(1.0,t);zoom=.92+.08*ease_back(t);}
    else if(is_closing){double t=(now-close_at)/140.0;panel_alpha=closing_alpha*(1-ease(t));zoom=closing_zoom-(closing_zoom-.92)*ease(t);}
    if(panel_alpha>0)quad({0,0,1920,1080},scale,ox,oy,0,static_cast<float>(.18*panel_alpha),shadow_view);
    float head_zoom=position.dragging?1.08f:hot==1?1.04f:1.0f;
    Rect face=head_rect(position,head_zoom);
    if(!idle)quad({face.x-6,face.y-4,face.w+12,face.h+12},scale,ox,oy,-2,position.dragging?.65f:.28f,shadow_view);
    float ring_alpha=hot==1?1.0f:m.collecting?static_cast<float>(.68+.17*std::sin(now*6.283185307/1600)):.8f;
    if(!idle)quad({face.x-2,face.y-2,face.w+4,face.h+4},scale,ox,oy,46*head_zoom,ring_alpha,ring_view);
    quad(face,scale,ox,oy,44*head_zoom,1,flag(&portrait_captured) && portrait_view?portrait_view:avatar_view);
    if(m.unread){double t=pop_at?(now-pop_at)/250.0:1;float pop=t<1?static_cast<float>(1+.25*std::exp(-5*t)*std::cos(9*t)):1;
        quad({position.x+18+19*(1-pop),position.y-52+19*(1-pop),38*pop,38*pop},scale,ox,oy,19*pop,1,badge_view);}
    if(panel_alpha>0 && panel_view){
        Rect base=panel_rect(position.x);auto transform=[&](Rect r){return panel_transform(r,position.x,position.y,zoom);};
        Rect panel=transform(base);float alpha=static_cast<float>(panel_alpha);
        quad({panel.x-10,panel.y-4,panel.w+20,panel.h+20},scale,ox,oy,-2,alpha*.36f,shadow_view);
        bool right=position.x>=960;float ty=bounded(position.y-14,base.y+24,base.y+base.h-52);
        quad(transform({right?base.x+base.w-4:base.x-18,ty,22,28}),scale,ox,oy,right?-1.25f:-1,alpha,panel_view);
        quad(panel,scale,ox,oy,18,alpha,panel_view);
        Rect clip=transform(body_rect(base));D3D11_RECT scissor{static_cast<LONG>(std::ceil(ox+clip.x*scale)),static_cast<LONG>(std::ceil(oy+clip.y*scale)),static_cast<LONG>(std::floor(ox+(clip.x+clip.w)*scale)),static_cast<LONG>(std::floor(oy+(clip.y+clip.h)*scale))};
        c->RSSetState(scissor_state);c->RSSetScissorRects(1,&scissor);
        quad(transform({base.x+56,base.y+250-static_cast<float>(sy),base.w-112,static_cast<float>(body_height)}),scale,ox,oy,0,alpha,body_view);c->RSSetState(raster_state);
        if(body_height>body_rect(base).h){Rect area=body_rect(base);float thumb=(std::max)(32.0f,area.h*area.h/body_height),offset=bounded(static_cast<float>(sy),0,static_cast<float>(max_scroll))*(area.h-thumb)/max_scroll;
            quad(transform({base.x+base.w-37,area.y,3,area.h}),scale,ox,oy,1,alpha*.16f,ring_view);
            quad(transform({base.x+base.w-37,area.y+offset,3,thumb}),scale,ox,oy,1,alpha*.65f,ring_view);}
        for(unsigned id=2;id<=7;++id)if(hot==static_cast<int>(id) || pressed==static_cast<int>(id)){
            auto &item=highlights[id][pressed==static_cast<int>(id)?1:0][m.detail?1:0];
            if(item.view)quad(transform(control(id,base)),scale,ox,oy,4,alpha,item.view);
        }
    }
    InterlockedExchange(&ready,1);
#ifndef NO_RUNTIME_LOG
        if(input_started && draw_revision>=input_revision) {
            LONG64 elapsed=static_cast<LONG64>(GetTickCount64()-input_started);InterlockedExchange64(&input_render_ms,elapsed);if(elapsed>input_max_render_ms)InterlockedExchange64(&input_max_render_ms,elapsed);
            enter_model_lock(false);if(render_input_started==input_started && render_input_revision==input_revision)render_input_started=0;leave_model_lock(false);
        }
#endif
    drawing_context=nullptr;target->Release();
}
}
