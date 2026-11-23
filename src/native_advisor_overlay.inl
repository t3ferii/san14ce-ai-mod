// Shares the existing Present/WndProc owner. Never dereferences a game object.
#include "advisor_briefing.h"
#include "advisor_face_files.h"   // local224: face-file heads (kPixelBytes, kLarge, kSmall)
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
struct Model {std::wstring adviser=L"군사",title=L"열흘 보고",body,note;unsigned unread=0,page=0,pages=0,generation=0,tickets=0,offers=0,offer_kind=0;bool collecting=false,open=false,city_list=false,offer=false,advice=false,settings=false;std::vector<std::wstring> names,cities;
    // local222 report view: tabs (one per kind, 전체 last; page = the open tab), one row per message, the selected row, the info bar
    // ("label\tvalue") and the detail panel (title, whole text, the same name's other messages "day\ttopic\tsummary").
    // local228: kinds = the 구분 column is shown (report_kinds); detail_caption = "topic, day" after the detail title.
    bool report=false,kinds=false;unsigned selected=0;std::vector<std::wstring> tabs,facts,records;std::vector<ReportRow> rows;std::wstring detail_title,detail_caption,detail_body;
    int every_tab=-1;   // local232: the 전체 tab's index (-2 none); -1 = the last of several tabs, as before the 로그 tab came after it
    bool log_view=false;};   // local235: the 로그 tab is open: no detail panel, the rows run to its bottom edge (rows_rect tall)
static bool report_view(const Model &m){return m.report && !m.city_list && !m.offer && !m.advice && !m.settings && !m.rows.empty();}
static bool report_tall(const Model &m){return report_view(m) && m.log_view;}
static bool detail_shown(const Model &m){return report_view(m) && !m.log_view;}
static bool detail_has_face(const Model &m){return detail_shown(m) && m.selected<m.rows.size() && m.rows[m.selected].person!=0;}
// local229: the report's tab count (0 outside the report view) and how far a second tab line moves the info bar, header and rows.
static unsigned report_tabs(const Model &m){return report_view(m)?static_cast<unsigned>((std::min)(m.tabs.size(),static_cast<size_t>(kReportTabs))):0u;}
static float info_drop(const Model &m){return tab_shift(report_tabs(m));}
// local99: buttons of the open window. 13 = [수집 설정]; its page reuses the city grid (settings implies city_list).
static const unsigned kButtons[]={2,3,4,5,6,7,8,13};
static bool button_shown(const Model &m,unsigned id) {
    if(m.advice)return id==2 || id==8;
    if(m.offer)return id==2 || id==5 || id==6;
    if(m.settings)return id==2 || id==13;
    if(m.city_list)return id==2 || id==7;
    if(id==5 && report_view(m))return m.every_tab>=0?m.page!=static_cast<unsigned>(m.every_tab):m.every_tab==-2 || !(m.pages>1 && m.page+1==m.pages);   // local222: 전체 has no single kind to delete
    return true;
}
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
// 2026-11-08 user: a compact phone-message-style toast next to the advisor head, 5 s.
static std::wstring toast_text;
static ULONGLONG toast_at=0;
static volatile LONG toast_only=0;
static volatile LONG visible,ready,pending,pending_generation,portrait_requested,portrait_revision,portrait_captured;
static Rect source;
static int source_revision,armed=0,max_scroll=0;
static HeadMotion head;
static ScrollMotion scrolling;
static ScrollMotion detail_scroll;             // local222: the report view's detail text scrolls on its own (wheel)
static int detail_height=1,detail_max=0;       // detail_max is written under the model lock (read by input)
static ID3D11Texture2D *detail_texture;
static ID3D11ShaderResourceView *detail_view;
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
static ID3D11Texture2D *toast_texture;
static ID3D11ShaderResourceView *toast_view;
struct Highlight {ID3D11Texture2D *texture=nullptr;ID3D11ShaderResourceView *view=nullptr;};
static Highlight highlights[14][2];
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
static volatile LONG extended_toast;   // local17 briefing16 (set by advisor_briefing::hud): 15 s wrapped toast; off = v15.7.1 5 s line
static void set_extended(bool on){InterlockedExchange(&extended_toast,on?1:0);}
static volatile LONG toast_override;   // local64: a toast may carry its own duration (ambush ending notice, 15 s)
static ULONGLONG toast_ms(){const LONG own=InterlockedCompareExchange(&toast_override,0,0);return own>0?static_cast<ULONGLONG>(own):InterlockedCompareExchange(&extended_toast,0,0)?15000:5000;}
static SRWLOCK face_offer_lock=SRWLOCK_INIT;   // local17 briefing16 U6 direct face hand-off (offer_face / take_face_offer)
static ID3D11Texture2D *face_offer;
static int face_offer_revision;
static std::vector<unsigned char> face_offer_pixels;   // local232: or the adviser's head from the face files (face_files::kPixelBytes RGBA), under face_offer_lock
static ID3D11Texture2D *panel_texture,*badge_texture,*ticket_texture,*coupon_texture,*offer_texture;
static ID3D11ShaderResourceView *panel_view,*badge_view,*ticket_view,*coupon_view,*offer_view;
static ID3D11Texture2D *avatar_texture;
static ID3D11ShaderResourceView *avatar_view;
static ID3D11Texture2D *shadow_texture,*ring_texture;
static ID3D11ShaderResourceView *shadow_view,*ring_view;
static ID3D11Texture2D *stable_scene;
static volatile LONG report_masked,report_tail,scene_clean;
static volatile LONG64 report_window_until;
static unsigned painted_generation=~0u,painted_unread=~0u,painted_tickets=~0u,painted_coupon=~0u;static int painted_offer=-1;
static ULONGLONG opened_at;
static ULONGLONG portrait_attempt_at;
static HFONT fonts[8]{};
#ifndef NO_RUNTIME_LOG
static volatile LONG64 input_posted_at,input_delay_ms,input_max_delay_ms,input_dispatches;
static volatile LONG64 input_posts,input_accepted,input_cancel_generation,input_render_ms,input_max_render_ms,raster_ms,raster_max_ms;
static volatile LONG64 dispatched_input_started;
static ULONGLONG render_input_started;
static unsigned render_input_revision;
#endif
static LONG flag(volatile LONG *v){return InterlockedCompareExchange(v,0,0);}
template<class T>static void drop(T *&p){if(p){p->Release();p=nullptr;}}
// local222 row faces (user 2026-11-17 "무장이름에 초상화나오는 방식"): the UI thread offers one AddRef'd original face texture
// at a time with its person id (offer_row_face); draw() copies the head square into a small texture of its own (take_row_face), kept
// per person, least recently drawn replaced. The UI thread then gives its face reference back. A person whose texture could not be
// copied is kept with no view, so it is not requested again and its row keeps the drawn initial.
static constexpr unsigned kRowFaces=192;   // local223: a 48px copy is 2.3 KB, so the faces seen once stay for the session
struct RowFace {unsigned person=0;ID3D11Texture2D *texture=nullptr;ID3D11ShaderResourceView *view=nullptr;ULONGLONG used=0;};
static RowFace row_faces[kRowFaces];   // render thread only
static SRWLOCK row_face_lock=SRWLOCK_INIT;
static ID3D11Texture2D *row_face_offer;static unsigned row_face_offer_person;
static std::vector<unsigned char> row_face_offer_pixels;   // local224: or a head from the face files (face_files::kPixelBytes RGBA), under row_face_lock
static unsigned row_face_persons[kRowFaces];   // under row_face_lock: the persons row_faces holds (read by the UI thread)
static volatile LONG row_faces_stale;          // set by the UI thread when the world changes: person ids name other persons now
static volatile LONG row_first=-1;             // first row on screen in the report view, -1 = not drawn (read by the UI thread)
static bool row_face_ready(unsigned person) {
    if(!person)return false;
    AcquireSRWLockShared(&row_face_lock);bool found=false;for(unsigned p:row_face_persons)if(p==person)found=true;ReleaseSRWLockShared(&row_face_lock);
    return found;
}
static bool row_face_busy(){AcquireSRWLockShared(&row_face_lock);const bool busy=row_face_offer!=nullptr || !row_face_offer_pixels.empty();ReleaseSRWLockShared(&row_face_lock);return busy;}
static long row_view_first(){return InterlockedCompareExchange(&row_first,0,0);}
static bool offer_row_face(ID3D11Texture2D *texture,unsigned person) {
    AcquireSRWLockExclusive(&row_face_lock);const bool free_slot=!row_face_offer && row_face_offer_pixels.empty();
    if(free_slot){row_face_offer=texture;row_face_offer_person=person;}
    ReleaseSRWLockExclusive(&row_face_lock);
    return free_slot;
}
// local224: a head decoded from the face files, copied here (the large level then the small one, RGBA).
static bool offer_row_face_pixels(const unsigned char *pixels,size_t bytes,unsigned person) {
    if(!pixels || bytes!=face_files::kPixelBytes)return false;
    AcquireSRWLockExclusive(&row_face_lock);const bool free_slot=!row_face_offer && row_face_offer_pixels.empty();
    if(free_slot){row_face_offer_pixels.assign(pixels,pixels+bytes);row_face_offer_person=person;}
    ReleaseSRWLockExclusive(&row_face_lock);
    return free_slot;
}
static void forget_row_faces() {
    AcquireSRWLockExclusive(&row_face_lock);for(auto &p:row_face_persons)p=0;ReleaseSRWLockExclusive(&row_face_lock);
    InterlockedExchange(&row_faces_stale,1);
}
static void drop_row_faces() {
    for(auto &f:row_faces){drop(f.view);drop(f.texture);f=RowFace{};}
    AcquireSRWLockExclusive(&row_face_lock);
    for(auto &p:row_face_persons)p=0;
    ID3D11Texture2D *offered=row_face_offer;row_face_offer=nullptr;row_face_offer_pixels.clear();
    ReleaseSRWLockExclusive(&row_face_lock);
    if(offered)offered->Release();
}
// local228 the original list window frame (face_files::kFrameSprite, read from the archive by the briefing worker and handed over once
// by the UI thread): RGBA, under frame_lock. Until it arrives, or when the file has none, the panel draws its own double line.
static SRWLOCK frame_lock=SRWLOCK_INIT;
static std::vector<unsigned char> frame_rgba;static unsigned frame_w,frame_h;
static void offer_frame(const unsigned char *rgba,unsigned w,unsigned h) {
    const unsigned least=2*(look::kFrameMargin+look::kFrameSlice)+1;
    if(!rgba || w<least || h<least || w>256 || h>256)return;
    AcquireSRWLockExclusive(&frame_lock);frame_rgba.assign(rgba,rgba+static_cast<size_t>(w)*h*4);frame_w=w;frame_h=h;ReleaseSRWLockExclusive(&frame_lock);
    enter_model_lock(false);++content_revision;leave_model_lock(false);   // the panel rasters again with the frame
}
static void release_device() {
    InterlockedExchange(&ready,0);InterlockedExchange(&portrait_captured,0);
    enter_model_lock(false);portrait_confirmation.reset();leave_model_lock(false);
    drop(portrait_staging);drop(portrait_candidate_view);drop(portrait_candidate);
    drop(body_view);drop(body_texture);drop(detail_view);drop(detail_texture);for(auto &by_id:highlights)for(auto &item:by_id){drop(item.view);drop(item.texture);}
    drop(toast_view);drop(toast_texture);
    drop(portrait_view);drop(portrait_texture);drop(panel_view);drop(panel_texture);drop(badge_view);drop(badge_texture);drop(ticket_view);drop(ticket_texture);drop(coupon_view);drop(coupon_texture);drop(offer_view);drop(offer_texture);
    drop(avatar_view);drop(avatar_texture);
    drop(shadow_view);drop(shadow_texture);drop(ring_view);drop(ring_texture);
    drop(stable_scene);InterlockedExchange(&report_tail,0);
    {AcquireSRWLockExclusive(&face_offer_lock);ID3D11Texture2D *offered=face_offer;face_offer=nullptr;face_offer_pixels.clear();ReleaseSRWLockExclusive(&face_offer_lock);if(offered)offered->Release();}
    drop_row_faces();
    drop(vs);drop(ps);drop(sampler);drop(blend);drop(raster_state);drop(scissor_state);drop(depth);drop(constants);drop(owner);
    for(auto &font:fonts)if(font){DeleteObject(font);font=nullptr;}
    painted_generation=~0u;painted_unread=~0u;painted_tickets=~0u;painted_coupon=~0u;painted_offer=-1;painted_content=~0u;portrait_attempt_at=0;
}
static bool is_visible(){return flag(&visible)!=0;}
// local307 (user 「일기토 발생하면 군사보고좀 숨겨줘 거슬린다」): while the original's duel runs nothing of the briefing draws or takes
// input (the inbox keeps collecting, the model keeps updating). The flag is a copied value: native_advisor_briefing.inl's game-frame
// and event-thread hooks read CDuelManager +0x10 and publish it here (no game read on Present / WndProc). True when it changed.
static volatile LONG duel_hide;
bool set_duel(bool on) {
    const LONG v=on?1:0;
    if(InterlockedExchange(&duel_hide,v)==v)return false;
    if(on){InterlockedExchange(&wheel_block,0);InterlockedExchange(&block_buttons,0);}
    return true;
}
static void mask_report(bool value){InterlockedExchange(&report_masked,value?1:0);}
static bool report_window(){return static_cast<LONG64>(GetTickCount64())<=InterlockedCompareExchange64(&report_window_until,0,0) && InterlockedCompareExchange64(&report_window_until,0,0)!=0;}
bool needs_draw(){return !flag(&duel_hide) && (is_visible() || flag(&report_masked) || flag(&report_tail) || report_window());}
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
    closing=false;outside_click=false;head.cancel();scrolling.reset();detail_scroll.reset();armed=hover=0;
    HWND captured_window=capture_window;capture_window=nullptr;
    InterlockedExchange(&pending,0);InterlockedExchange(&visible,0);
    InterlockedExchange(&wheel_block,0);InterlockedExchange(&block_buttons,0);
    leave_model_lock(false);
    if(captured_window && GetCapture()==captured_window)ReleaseCapture();
}
bool available(){return flag(&ready)!=0;}
static void publish(const Model &next,bool show,bool accepted_input=false) {
    enter_model_lock(false);
    ULONGLONG now=GetTickCount64();
    if(next.generation!=model.generation || next.page!=model.page || next.city_list!=model.city_list)scrolling.reset();
    if(next.generation!=model.generation || next.page!=model.page || next.city_list!=model.city_list || next.selected!=model.selected || next.detail_body!=model.detail_body)detail_scroll.reset();
    if(next.collecting)was_collecting=true;
    else if(was_collecting){collecting_ended_at=now;was_collecting=false;}
    if(next.collecting || (collecting_ended_at && now-collecting_ended_at<=8000))InterlockedExchange64(&report_window_until,next.collecting?now+8000:collecting_ended_at+8000);
    else InterlockedExchange64(&report_window_until,0);
    if(next.open && (!model.open || !is_visible()) && show){opened_at=now;closing=false;}
    if(!next.open && model.open && is_visible() && show){
        double t=(now-opened_at)/220.0;close_zoom=static_cast<float>(.92+.08*ease_back(t));close_alpha=static_cast<float>((std::min)(1.0,t));closed_at=now;closing=true;
    }
    if(next.unread>model.unread)badge_at=now;
    if(next.generation!=model.generation || next.page!=model.page || next.city_list!=model.city_list || next.pages!=model.pages || next.adviser!=model.adviser || next.title!=model.title || next.body!=model.body || next.names!=model.names || next.cities!=model.cities || next.collecting!=model.collecting || next.offer!=model.offer || (next.offers!=0)!=(model.offers!=0) || next.offer_kind!=model.offer_kind || next.note!=model.note || next.advice!=model.advice || next.settings!=model.settings || next.report!=model.report || next.selected!=model.selected || next.tabs!=model.tabs || next.facts!=model.facts || next.records!=model.records || next.rows!=model.rows || next.detail_title!=model.detail_title || next.detail_caption!=model.detail_caption || next.kinds!=model.kinds || next.log_view!=model.log_view || next.detail_body!=model.detail_body)++content_revision;
    if(next.generation!=model.generation || next.page!=model.page || next.city_list!=model.city_list || next.pages!=model.pages || next.adviser!=model.adviser || next.title!=model.title || next.body!=model.body || next.names!=model.names || next.cities!=model.cities || next.collecting!=model.collecting || next.open!=model.open || next.unread!=model.unread || next.offer!=model.offer || next.tickets!=model.tickets || next.offers!=model.offers || next.offer_kind!=model.offer_kind || next.note!=model.note || next.advice!=model.advice || next.settings!=model.settings || next.report!=model.report || next.selected!=model.selected || next.tabs!=model.tabs || next.facts!=model.facts || next.records!=model.records || next.rows!=model.rows || next.detail_title!=model.detail_title || next.detail_caption!=model.detail_caption || next.kinds!=model.kinds || next.log_view!=model.log_view || next.detail_body!=model.detail_body){model=next;++model_revision;}
#ifndef NO_RUNTIME_LOG
    if(accepted_input){render_input_started=static_cast<ULONGLONG>(dispatched_input_started);render_input_revision=model_revision;InterlockedIncrement64(&input_accepted);}
#else
    (void)accepted_input;
#endif
    if(!show){outside_click=false;armed=hover=0;head.cancel();scrolling.release();closing=false;InterlockedExchange(&wheel_block,0);InterlockedExchange(&block_buttons,0);}
    const bool tip=!toast_text.empty() && toast_at && GetTickCount64()<toast_at+toast_ms();
    leave_model_lock(false);
    InterlockedExchange(&toast_only,(!show && tip)?1:0);
    InterlockedExchange(&visible,(show || tip)?1:0);
    if(!show){InterlockedExchange(&pending,0);if(capture_window){HWND window=capture_window;capture_window=nullptr;if(GetCapture()==window)ReleaseCapture();}}
}
void toast_for(const std::wstring &text,unsigned ms) {
    InterlockedExchange(&toast_override,static_cast<LONG>(ms));
    enter_model_lock(false);toast_text=text;toast_at=GetTickCount64();leave_model_lock(false);
    InterlockedExchange(&toast_only,1);InterlockedExchange(&visible,1);
}
static void request_portrait(Rect r,int revision) {
    enter_model_lock(false);
    if(source_revision!=revision || !portrait_rect_matches(source,r))portrait_confirmation.reset();
    source=r;source_revision=revision;InterlockedExchange(&portrait_requested,1);leave_model_lock(false);
}
static bool captured(int revision){return flag(&portrait_captured) && flag(&portrait_revision)==revision;}
// user 2026-11-09 (U6): the advisor portrait comes straight from the original face texture (512x512 BC3 of the face
// manager entry, advisor_briefing::face_texture_for) instead of a back-buffer capture that picked up other screens on
// load. The UI thread hands over one AddRef'd texture (face_offer, declared with portrait_texture); draw() copies the
// head area into its own texture.
static void offer_face(ID3D11Texture2D *texture,int revision) {
    AcquireSRWLockExclusive(&face_offer_lock);
    ID3D11Texture2D *old=face_offer;face_offer=texture;face_offer_revision=revision;face_offer_pixels.clear();
    ReleaseSRWLockExclusive(&face_offer_lock);
    if(old)old->Release();
}
// local232: the adviser's head decoded from the face files (the original advice panel is not drawn any more, so its resident texture
// and the capture may never come): the same head square as the 384 copy, 96 and 48 px RGBA. A later texture offer replaces it.
static bool offer_face_pixels(const unsigned char *pixels,size_t bytes,int revision) {
    if(!pixels || bytes!=face_files::kPixelBytes)return false;
    AcquireSRWLockExclusive(&face_offer_lock);
    ID3D11Texture2D *old=face_offer;face_offer=nullptr;face_offer_pixels.assign(pixels,pixels+bytes);face_offer_revision=revision;
    ReleaseSRWLockExclusive(&face_offer_lock);
    if(old)old->Release();
    return true;
}
static void take_face_offer(ID3D11Device *d,ID3D11DeviceContext *c) {
    AcquireSRWLockExclusive(&face_offer_lock);
    ID3D11Texture2D *offered=face_offer;const int revision=face_offer_revision;face_offer=nullptr;
    std::vector<unsigned char> pixels;pixels.swap(face_offer_pixels);
    ReleaseSRWLockExclusive(&face_offer_lock);
    if(!offered && pixels.size()==face_files::kPixelBytes) {   // local232: a face-file head
        D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=face_files::kLarge;td.MipLevels=2;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
        td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init[2]{};init[0].pSysMem=pixels.data();init[0].SysMemPitch=face_files::kLarge*4;
        init[1].pSysMem=pixels.data()+face_files::kLarge*face_files::kLarge*4;init[1].SysMemPitch=face_files::kSmall*4;
        ID3D11Texture2D *texture=nullptr;ID3D11ShaderResourceView *view=nullptr;bool ok=false;
        if(SUCCEEDED(d->CreateTexture2D(&td,init,&texture)) && texture) {
            D3D11_SHADER_RESOURCE_VIEW_DESC vd{};vd.Format=srgb_target?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;
            vd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;vd.Texture2D.MipLevels=2;
            if(SUCCEEDED(d->CreateShaderResourceView(texture,&vd,&view)) && view) {
                drop(portrait_view);drop(portrait_texture);portrait_texture=texture;portrait_view=view;texture=nullptr;
                InterlockedExchange(&portrait_revision,revision);InterlockedExchange(&portrait_captured,1);InterlockedExchange(&portrait_requested,0);
                ok=true;
            }
            drop(texture);
        }
#ifndef NO_RUNTIME_LOG
        log_event("advisor_face_direct",static_cast<unsigned>(revision),ok?1u:0u,4096u);   // 4096: from the face files
#endif
        return;
    }
    if(!offered)return;
    bool ok=false;
    ID3D11Device *owner_device=nullptr;offered->GetDevice(&owner_device);
    D3D11_TEXTURE2D_DESC sd{};offered->GetDesc(&sd);
    if(owner_device==d && sd.Width==512 && sd.Height==512 && sd.Format>=DXGI_FORMAT_BC3_TYPELESS && sd.Format<=DXGI_FORMAT_BC3_UNORM_SRGB) {
        D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=384;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_BC3_TYPELESS;
        td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        ID3D11Texture2D *texture=nullptr;ID3D11ShaderResourceView *view=nullptr;
        if(SUCCEEDED(d->CreateTexture2D(&td,nullptr,&texture)) && texture) {
            const D3D11_BOX head_box{64,0,0,448,384,1};   // block-aligned head-and-shoulders square of the 512x512 face
            c->CopySubresourceRegion(texture,0,0,0,0,offered,0,&head_box);
            D3D11_SHADER_RESOURCE_VIEW_DESC vd{};vd.Format=srgb_target?DXGI_FORMAT_BC3_UNORM_SRGB:DXGI_FORMAT_BC3_UNORM;
            vd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;vd.Texture2D.MipLevels=1;
            if(SUCCEEDED(d->CreateShaderResourceView(texture,&vd,&view)) && view) {
                drop(portrait_view);drop(portrait_texture);portrait_texture=texture;portrait_view=view;texture=nullptr;
                InterlockedExchange(&portrait_revision,revision);InterlockedExchange(&portrait_captured,1);InterlockedExchange(&portrait_requested,0);
                ok=true;
            }
            drop(texture);
        }
    }
    if(owner_device)owner_device->Release();
    offered->Release();
#ifndef NO_RUNTIME_LOG
    log_event("advisor_face_direct",static_cast<unsigned>(revision),ok?1u:0u,static_cast<unsigned>(sd.Format));
#endif
}
// The head square {64,0,448,384} of the 512x512 BC3 face, from the smallest mip level that still has 48 pixels (block aligned at
// levels 0..3), so a 44px row square is drawn from about its own size instead of shimmering down from 384.
static void take_row_face(ID3D11Device *d,ID3D11DeviceContext *c) {
    if(InterlockedExchange(&row_faces_stale,0))drop_row_faces();
    AcquireSRWLockExclusive(&row_face_lock);
    ID3D11Texture2D *offered=row_face_offer;const unsigned person=row_face_offer_person;row_face_offer=nullptr;
    std::vector<unsigned char> pixels;pixels.swap(row_face_offer_pixels);
    ReleaseSRWLockExclusive(&row_face_lock);
    if(!offered && pixels.empty())return;
    ID3D11Texture2D *texture=nullptr;ID3D11ShaderResourceView *view=nullptr;D3D11_TEXTURE2D_DESC sd{};
    if(!offered) {   // local224: a face-file head, 96 px and 48 px levels (the 112 px detail square and the 44 px rows each sample about their size)
        D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=face_files::kLarge;td.MipLevels=2;td.ArraySize=1;td.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
        td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_IMMUTABLE;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;sd=td;
        D3D11_SUBRESOURCE_DATA init[2]{};init[0].pSysMem=pixels.data();init[0].SysMemPitch=face_files::kLarge*4;
        init[1].pSysMem=pixels.data()+face_files::kLarge*face_files::kLarge*4;init[1].SysMemPitch=face_files::kSmall*4;
        if(SUCCEEDED(d->CreateTexture2D(&td,init,&texture)) && texture) {
            D3D11_SHADER_RESOURCE_VIEW_DESC vd{};vd.Format=srgb_target?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;
            vd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;vd.Texture2D.MipLevels=2;
            if(FAILED(d->CreateShaderResourceView(texture,&vd,&view)))view=nullptr;
        }
        if(!view)drop(texture);
    }
    ID3D11Device *owner_device=nullptr;if(offered){offered->GetDevice(&owner_device);offered->GetDesc(&sd);}
    if(offered && owner_device==d && sd.Width==512 && sd.Height==512 && sd.MipLevels>=1 && sd.ArraySize>=1 && sd.Format>=DXGI_FORMAT_BC3_TYPELESS && sd.Format<=DXGI_FORMAT_BC3_UNORM_SRGB) {
        const UINT level=(std::min)(sd.MipLevels-1,3u),side=384u>>level;
        D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=side;td.MipLevels=td.ArraySize=1;td.Format=DXGI_FORMAT_BC3_TYPELESS;
        td.SampleDesc.Count=1;td.Usage=D3D11_USAGE_DEFAULT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        if(SUCCEEDED(d->CreateTexture2D(&td,nullptr,&texture)) && texture) {
            const D3D11_BOX box{64u>>level,0,0,448u>>level,384u>>level,1};
            c->CopySubresourceRegion(texture,0,0,0,0,offered,D3D11CalcSubresource(level,0,sd.MipLevels),&box);
            D3D11_SHADER_RESOURCE_VIEW_DESC vd{};vd.Format=srgb_target?DXGI_FORMAT_BC3_UNORM_SRGB:DXGI_FORMAT_BC3_UNORM;
            vd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;vd.Texture2D.MipLevels=1;
            if(FAILED(d->CreateShaderResourceView(texture,&vd,&view)))view=nullptr;
        }
        if(!view)drop(texture);
    }
    if(owner_device)owner_device->Release();
    if(offered)offered->Release();
    size_t slot=0;
    for(size_t i=0;i<kRowFaces;++i){if(!row_faces[i].person){slot=i;break;}if(row_faces[i].used<row_faces[slot].used)slot=i;}
    drop(row_faces[slot].view);drop(row_faces[slot].texture);
    row_faces[slot].person=person;row_faces[slot].texture=texture;row_faces[slot].view=view;row_faces[slot].used=GetTickCount64();
    AcquireSRWLockExclusive(&row_face_lock);row_face_persons[slot]=person;ReleaseSRWLockExclusive(&row_face_lock);
#ifndef NO_RUNTIME_LOG
    log_event("advisor_row_face_copy",person,view?1u:0u,static_cast<unsigned>(sd.Format));
#endif
}
void toast(const std::wstring &text) {toast_for(text,0);}
void publish_hidden(){publish({},false);}   // the advisor HUD leave hook (native_split_install.inl, main unit since the 2026-11-16 unit split)
static void forget() {
    InterlockedExchange(&ready,0);
    InterlockedExchange(&portrait_captured,0);InterlockedExchange(&portrait_requested,0);InterlockedExchange(&pending,0);
    InterlockedExchange(&visible,0);armed=0;
    enter_model_lock(false);portrait_confirmation.reset();head.cancel();scrolling.reset();detail_scroll.reset();closing=false;outside_click=false;toast_text.clear();toast_at=0;leave_model_lock(false);InterlockedExchange(&toast_only,0);
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
// local64 (user 2026-11-12 "버블에서 말풍선 띄워서 수락/거절"): the pending ambush proposal is a balloon above the head
// (below it near the top edge) while the report is closed. 10 = 수락, 11 = 거절, 12 = balloon body.
static const float kOfferW=660,kOfferH=150;   // user 2026-11-13: the ambush line was cut at 560
static Rect offer_box(float hx,float hy) {
    Rect box{bounded(hx-44-236.0f,16.0f,1904.0f-kOfferW),hy-44-kOfferH-10,kOfferW,kOfferH};
    if(box.y<16)box.y=hy+44+10;
    return box;
}
static Rect offer_button(Rect box,bool accept){return {box.x+box.w-(accept?236.0f:120.0f),box.y+box.h-52,104,38};}
static bool offer_balloon(const Model &m){return m.offers && !m.open && !m.collecting;}
static int widget(float x,float y,bool open) {
    float dx=x-head.x,dy=y-head.y;if(dx*dx+dy*dy<=44*44)return 1;
    if(offer_balloon(model) && !open) {
        Rect box=offer_box(head.x,head.y);
        if(box.contains(x,y))return offer_button(box,true).contains(x,y)?10:offer_button(box,false).contains(x,y)?11:12;
    }
    Rect panel=panel_rect(head.x);double zoom=model.open?.92+.08*ease_back((GetTickCount64()-opened_at)/220.0):1;
    if(!open || !panel_transform(panel,head.x,head.y,zoom).contains(x,y))return 0;
    for(unsigned id:kButtons) {
        if(!button_shown(model,id))continue;
        if(panel_transform(control(id,panel,info_drop(model)),head.x,head.y,zoom).contains(x,y))return static_cast<int>(id);
    }
    if(model.city_list && !model.cities.empty())
        for(unsigned i=0;i<model.cities.size();++i)
            if(panel_transform(city_cell(panel,i,static_cast<unsigned>(model.cities.size())),head.x,head.y,zoom).contains(x,y))return 100+static_cast<int>(i);
    // local222 report view: 40+i = tab i, 1000+i = row i (fixed pitch, scrolled by the body scroll).
    if(report_view(model)) {
        const unsigned tabs=report_tabs(model);
        for(unsigned i=0;i<tabs;++i)if(panel_transform(tab_rect(panel,i,tabs),head.x,head.y,zoom).contains(x,y))return 40+static_cast<int>(i);
        const Rect rows=panel_transform(rows_rect(panel,tabs,report_tall(model)),head.x,head.y,zoom);
        if(rows.contains(x,y)) {
            const double at=(y-rows.y)/zoom+scrolling.shown;
            if(at>=0 && at/kRowPitch<static_cast<double>(model.rows.size())) {
                const unsigned i=static_cast<unsigned>(at/kRowPitch);
                // local243: the row's [지도 이동] button (same rectangle as the drawing: map_button_rect / map_button_x)
                if(model.rows[i].place && panel_transform(map_button_rect(panel,i,scrolling.shown,tabs,report_tall(model)),head.x,head.y,zoom).contains(x,y))return kMapHit+static_cast<int>(i);
                return 1000+static_cast<int>(i);
            }
        }
    }
    return 9;
}
bool input(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
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
    if(!is_visible() || !available() || flag(&duel_hide)){InterlockedExchange(&wheel_block,0);InterlockedExchange(&block_buttons,0);return false;}
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
    } else if(message==WM_MOUSEWHEEL && hit==9 && detail_shown(model) && detail_text_rect(panel_rect(head.x),detail_has_face(model)).contains(x,y)){
        detail_scroll.wheel(GET_WHEEL_DELTA_WPARAM(wparam)>0?-80:80,detail_max);animation_at=now;   // local222: the detail text has its own scroll
    } else if(message==WM_MOUSEWHEEL && (hit==9 || hit>=1000)){scrolling.wheel(GET_WHEEL_DELTA_WPARAM(wparam)>0?-80:80,max_scroll);animation_at=now;}
    else if(message==WM_LBUTTONDOWN || message==WM_LBUTTONDBLCLK){
        armed=hit;arm_generation=model.generation;
        if(hit==1){head.press(x,y);animation_at=now;capture=true;}
        else if((hit==9 || hit>=1000) && view_rect(panel_rect(head.x),report_view(model),report_tabs(model),report_tall(model)).contains(x,y)){scrolling.press(y);scroll_move_at=now;capture=true;}
    } else if(message==WM_LBUTTONUP){
        bool had_head=head.pressed,had_body=scrolling.pressed,body_drag=scrolling.dragging;
        bool was_drag=head.dragging || head.snapping;
        if(had_head && mapped){bool before=head.dragging;head.move(x,y);if(!before && head.dragging && model.open)post_input(2,model.generation);was_drag=was_drag || head.dragging;}
        bool tap=had_head?head.release():false;
        if(had_body)scrolling.release();if(had_head || had_body)animation_at=now;
        bool fire=hit && hit==armed && arm_generation==model.generation && (!had_head || tap) && !model.collecting && !(hit>=1000 && body_drag);
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
// family (optional): 0 Noto Serif KR, 1 함초롬바탕, 2 바탕, 3 the Batang fallback.
static HFONT serif_font(HDC dc,int height,int weight=FW_NORMAL,int *family=nullptr) {
    int index=0;
    for(const wchar_t *name:{L"Noto Serif KR",L"함초롬바탕",L"바탕"}) {
        HFONT font=CreateFontW(-height,0,0,0,weight,FALSE,FALSE,FALSE,HANGUL_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,name);
        if(font) {
            auto old=SelectObject(dc,font);wchar_t actual[LF_FACESIZE]{};GetTextFaceW(dc,LF_FACESIZE,actual);SelectObject(dc,old);
            if(_wcsicmp(actual,name)==0){if(family)*family=index;return font;}
            DeleteObject(font);
        }
        ++index;
    }
    if(family)*family=3;
    return CreateFontW(-height,0,0,0,weight,FALSE,FALSE,FALSE,HANGUL_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Batang");
}
// 5..7 = the report view's body, heading and caption (local223 user "글자크기 아주 살짝 줄여서": 23/27/18 -> 21/24/17).
// local228: the report body (summaries, detail text, record lines) in regular weight; names and headings stay semibold.
static constexpr int kFontHeights[8]={23,27,18,40,21,21,24,17},kFontWeights[8]={FW_SEMIBOLD,FW_SEMIBOLD,FW_MEDIUM,FW_NORMAL,FW_BOLD,FW_NORMAL,FW_SEMIBOLD,FW_MEDIUM};
static HFONT cached_font(HDC dc,unsigned kind) {
    if(!fonts[kind]) {
#ifndef NO_RUNTIME_LOG
        LARGE_INTEGER made_from{},made_to{},hz{};QueryPerformanceCounter(&made_from);
#endif
        int family=3;fonts[kind]=serif_font(dc,kFontHeights[kind],kFontWeights[kind],&family);
#ifndef NO_RUNTIME_LOG
        QueryPerformanceCounter(&made_to);QueryPerformanceFrequency(&hz);   // local229: kind, microseconds, family (serif_font)
        log_event("advisor_font",kind,static_cast<unsigned>((made_to.QuadPart-made_from.QuadPart)*1000000/(hz.QuadPart?hz.QuadPart:1)),static_cast<unsigned>(family));
#endif
    }
    return fonts[kind];
}
// local229 (user 2026-11-18 "처음열때 렉걸리는것"): the first raster of a report took 1.1 s (Body) and 0.2-0.4 s (Chrome) on the render
// thread (local228 log advisor_raster_slow): GDI loads the Noto Serif KR files and rasterises each glyph the first time a font of
// that size draws it in the process. Drawing the same characters once on another thread with fonts of the same specs fills the
// process-wide cache (offline: a cold first draw of 300 syllables 2346 ms, 1.5 ms after a warm on a worker thread). The face file
// worker calls this with the fixed words of the panels and the characters of each new message; it never touches fonts[] or the
// render thread's DCs and does not log (the hud logs for it). kinds: bit k = font kind k. Returns the characters drawn.
static unsigned warm_glyphs(const std::wstring &chars,unsigned kinds) {
    static HDC dc;static HFONT own[8];   // worker only, kept for the run
    if(chars.empty())return 0;
    if(!dc) {
        BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=1024;bi.bmiHeader.biHeight=-64;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
        HDC made=CreateCompatibleDC(nullptr);if(!made)return 0;
        void *bits=nullptr;HBITMAP bm=CreateDIBSection(made,&bi,DIB_RGB_COLORS,&bits,nullptr,0);   // 32 bpp like raster(): the same antialiased glyph path
        if(!bm || !bits){if(bm)DeleteObject(bm);DeleteDC(made);return 0;}
        SelectObject(made,bm);SetBkMode(made,TRANSPARENT);SetTextColor(made,RGB(238,230,214));dc=made;
    }
    for(unsigned k=0;k<8;++k) {
        if(!((kinds>>k)&1u))continue;
        if(!own[k])own[k]=serif_font(dc,kFontHeights[k],kFontWeights[k]);
        if(!own[k])continue;
        auto old=SelectObject(dc,own[k]);
        for(size_t i=0;i<chars.size();i+=24){RECT r{0,0,1024,64};DrawTextW(dc,chars.c_str()+i,static_cast<int>((std::min)(static_cast<size_t>(24),chars.size()-i)),&r,DT_SINGLELINE|DT_NOPREFIX);}
        SelectObject(dc,old);
    }
    GdiFlush();
    return static_cast<unsigned>(chars.size());
}
// Compact advisor message (2026-11-08 user: phone-message style, 5 s).
static std::vector<std::wstring> wrap_text(HDC dc,const std::wstring &text,int room);   // defined below (body layout)
// Compact advisor message (2026-11-08 user: phone-message style, 5 s).
// PW33 (user 2026-11-09): the text wraps downward (left-aligned, at most four lines) instead of being cut into one
// ellipsised line, and the same text is rasterised only once; the height the draw site needs is kept in toast_drawn_h.
static const int kToastWidth=560,kToastMaxLines=4;
static int toast_drawn_h=72;
static std::wstring toast_raster_text;
static bool raster_toast_wrapped(ID3D11DeviceContext *c,const std::wstring &text) {
    if(toast_texture && toast_view && text==toast_raster_text)return true;
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return false;
    auto oldfont=SelectObject(dc,cached_font(dc,0));
    TEXTMETRICW tm{};GetTextMetricsW(dc,&tm);
    const int pitch=(std::max)(20,static_cast<int>(tm.tmHeight)+6);
    std::vector<std::wstring> lines=wrap_text(dc,text,kToastWidth-44);
    if(lines.size()>static_cast<size_t>(kToastMaxLines)) {lines.resize(kToastMaxLines);lines.back()+=L"…";}
    const int w=kToastWidth,h=(std::max)(72,static_cast<int>(lines.size())*pitch+28);
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void *bits=nullptr;HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bm){SelectObject(dc,oldfont);DeleteDC(dc);return false;}auto old=SelectObject(dc,bm);
    auto rgb=[](unsigned v){return RGB((v>>16)&255,(v>>8)&255,v&255);};
    RECT full{0,0,w,h};HBRUSH fill=CreateSolidBrush(rgb(look::kPanelBottom));FillRect(dc,&full,fill);DeleteObject(fill);
    HPEN pen=CreatePen(PS_SOLID,1,rgb(look::kGoldBorder));auto op=SelectObject(dc,pen);auto ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
    RoundRect(dc,1,1,w-1,h-1,14,14);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(pen);
    SetBkMode(dc,TRANSPARENT);
    SetTextColor(dc,rgb(look::kText));
    const int top=(h-static_cast<int>(lines.size())*pitch)/2;
    for(size_t i=0;i<lines.size();++i) {
        RECT r{22,top+static_cast<int>(i)*pitch,w-22,top+static_cast<int>(i+1)*pitch};
        DrawTextW(dc,lines[i].c_str(),static_cast<int>(lines[i].size()),&r,DT_LEFT|DT_SINGLELINE|DT_NOPREFIX);
    }
    bool ok=upload(c,toast_texture,toast_view,w,h,static_cast<const unsigned *>(bits));
    SelectObject(dc,oldfont);SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);
    if(ok) {toast_raster_text=text;toast_drawn_h=h;}
    return ok;
}
static bool raster_toast_line(ID3D11DeviceContext *c,const std::wstring &text) {
    const int w=560,h=72;
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return false;
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void *bits=nullptr;HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bm){DeleteDC(dc);return false;}auto old=SelectObject(dc,bm);
    auto rgb=[](unsigned v){return RGB((v>>16)&255,(v>>8)&255,v&255);};
    RECT full{0,0,w,h};HBRUSH fill=CreateSolidBrush(rgb(look::kPanelBottom));FillRect(dc,&full,fill);DeleteObject(fill);
    HPEN pen=CreatePen(PS_SOLID,1,rgb(look::kGoldBorder));auto op=SelectObject(dc,pen);auto ob=SelectObject(dc,GetStockObject(NULL_BRUSH));
    RoundRect(dc,1,1,w-1,h-1,14,14);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(pen);
    SetBkMode(dc,TRANSPARENT);auto font=cached_font(dc,0);auto oldfont=SelectObject(dc,font);
    SetTextColor(dc,rgb(look::kText));RECT text_rect{22,0,w-22,h};
    DrawTextW(dc,text.c_str(),static_cast<int>(text.size()),&text_rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
    bool ok=upload(c,toast_texture,toast_view,w,h,static_cast<const unsigned *>(bits));
    SelectObject(dc,oldfont);SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);
    return ok;
}
static unsigned offer_texture_kind=~0u;   // destruction_ticket: the balloon text follows the proposal kind
static bool raster_offer(ID3D11DeviceContext *c,unsigned kind) {   // static content, rasterised once per device and kind
    if(offer_texture && offer_view && offer_texture_kind==kind)return true;
    drop(offer_view);drop(offer_texture);offer_texture_kind=kind;
    const int w=static_cast<int>(kOfferW),h=static_cast<int>(kOfferH);
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return false;
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void *bits=nullptr;HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bm){DeleteDC(dc);return false;}auto old=SelectObject(dc,bm);
    auto rgb=[](unsigned v){return RGB((v>>16)&255,(v>>8)&255,v&255);};
    auto box=[&](RECT r,unsigned line,unsigned fill,int radius){HPEN pen=CreatePen(PS_SOLID,1,rgb(line));HBRUSH brush=CreateSolidBrush(rgb(fill));
        auto op=SelectObject(dc,pen),ob=SelectObject(dc,brush);RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);
        SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(brush);DeleteObject(pen);};
    RECT full{0,0,w,h};HBRUSH fill=CreateSolidBrush(rgb(look::kPanelBottom));FillRect(dc,&full,fill);DeleteObject(fill);
    box({1,1,w-1,h-1},look::kGoldBorder,look::kPanelBottom,14);
    SetBkMode(dc,TRANSPARENT);auto oldfont=SelectObject(dc,cached_font(dc,0));
    SetTextColor(dc,rgb(look::kText));
    RECT line1{22,16,w-22,48},line2{22,50,w-22,82};
    const wchar_t *a=kind==1?L"주공, 맞닿은 지역의 적 시설을 확실히 부술 수 있습니다.":L"주공, 매복이동으로 전황을 바꿀 수도 있을 것 같습니다.",
                  *b=kind==1?L"금 1000을 주시면 파괴공작을 준비해 두겠습니다.":L"금 1000을 주시면 준비해 두겠습니다.";
    DrawTextW(dc,a,-1,&line1,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    DrawTextW(dc,b,-1,&line2,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    SelectObject(dc,cached_font(dc,2));SetTextColor(dc,rgb(look::kTitle));
    for(int accept=1;accept>=0;--accept) {
        RECT r{w-(accept?236:120),h-52,w-(accept?236:120)+104,h-14};
        box(r,look::kGoldBorder,look::kButtonFill,8);
        DrawTextW(dc,accept?L"수락":L"거절",-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    }
    bool ok=upload(c,offer_texture,offer_view,w,h,static_cast<const unsigned *>(bits));
    SelectObject(dc,oldfont);SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);
    return ok;
}
// local17 briefing16: 15 s wrapped toast; off = the v15.7.1 5 s single-line toast.
static bool raster_toast(ID3D11DeviceContext *c,const std::wstring &text) {
    if(InterlockedCompareExchange(&extended_toast,0,0) || InterlockedCompareExchange(&toast_override,0,0)>0)return raster_toast_wrapped(c,text);
    toast_raster_text.clear();toast_drawn_h=72;return raster_toast_line(c,text);
}
struct TextLine {std::wstring text;int x,y;bool bullet;int right=0;bool table=false,header=false,compact=false;};   // compact: a detail-panel table cell (font 7, local232)
// One table row's background: rows are laid out by body_lines, painted by raster (header fill, alternate tint, separator). index 0 is the header.
// index kCardBand = one advice card (a paragraph starting with kCardMark opens it, the next mark or the end closes it): tinted box, gold edge.
struct TableBand {int y,h,x0,x1,index;bool header;};
constexpr int kCardBand=-1;constexpr wchar_t kCardMark=L'\x1E';
// Greedy wrap of one paragraph or table cell to `room` pixels (GDI extents): break at a space past the middle of the line, else mid-word; a surrogate pair is never split.
static std::vector<std::wstring> wrap_text(HDC dc,const std::wstring &text,int room) {
    std::vector<std::wstring> out;
    for(size_t at=0;at<text.size();) {
        int fit=0;SIZE extent{};
        GetTextExtentExPointW(dc,text.c_str()+at,static_cast<int>(text.size()-at),room,&fit,nullptr,&extent);
        size_t count=(std::max)(size_t(1),static_cast<size_t>((std::max)(0,fit)));
        count=(std::min)(count,text.size()-at);
        if(at+count<text.size()) {
            size_t space=text.rfind(L' ',at+count-1);
            if(space!=std::wstring::npos && space>at+count/2)count=space-at;
            if(count && text[at+count-1]>=0xD800 && text[at+count-1]<=0xDBFF)--count;
            if(!count)count=(std::min)(size_t(2),text.size()-at);
        }
        std::wstring line=text.substr(at,count);while(!line.empty() && line.back()==L' ')line.pop_back();
        out.push_back(std::move(line));
        at+=count;while(at<text.size() && text[at]==L' ')++at;
    }
    return out;
}
// Tab-separated rows (rows[0] = header) as an aligned table: column widths from the measured content (look::kCellGap apart, water-filled when the
// table is wider than the window, long cells then wrap), numeric columns right-aligned, one band per row. `width` is the text width (text starts at x=56).
// compact (local232): the detail panel's table, measured and drawn in font 7 with the look::kDetail* metrics (TextLine::compact).
static HFONT cached_font(HDC dc,unsigned kind);
static void table_layout(HDC dc,std::vector<std::vector<std::wstring>> rows,int width,int &height,std::vector<TextLine> &lines,std::vector<TableBand> *bands,bool compact=false) {
    size_t columns=0;for(const auto &r:rows)columns=(std::max)(columns,r.size());
    if(!columns)return;
    const int pad=compact?look::kDetailTablePad:look::kTablePad,gap=compact?look::kDetailCellGap:look::kCellGap,pitch=compact?look::kDetailLinePitch:look::kLinePitch,
        row_pad=compact?look::kDetailRowPad:look::kRowPad,after=compact?look::kDetailTableGap:look::kTableGap;
    HGDIOBJ prior=compact?SelectObject(dc,cached_font(dc,7)):nullptr;
    for(auto &r:rows)r.resize(columns);
    std::vector<int> natural(columns,0);
    for(const auto &r:rows)for(size_t c=0;c<columns;++c){SIZE e{};GetTextExtentPoint32W(dc,r[c].c_str(),static_cast<int>(r[c].size()),&e);natural[c]=(std::max)(natural[c],static_cast<int>(e.cx));}
    const std::vector<int> widths=table_column_widths(natural,width-2*pad,gap,look::kMinColumn,look::kMaxExtra);
    std::vector<int> left(columns,56+pad);std::vector<bool> numeric(columns,false);
    for(size_t c=0;c<columns;++c){if(c)left[c]=left[c-1]+widths[c-1]+gap;numeric[c]=column_numeric(rows,c);}
    for(size_t r=0;r<rows.size();++r) {
        std::vector<std::vector<std::wstring>> cells(columns);size_t most=1;
        for(size_t c=0;c<columns;++c){cells[c]=wrap_text(dc,rows[r][c],widths[c]);most=(std::max)(most,cells[c].size());}
        const int row_height=static_cast<int>(most)*pitch+row_pad;
        if(bands)bands->push_back({height,row_height,56,56+width,static_cast<int>(r),r==0});
        for(size_t c=0;c<columns;++c)for(size_t k=0;k<cells[c].size();++k) {
            TextLine t{cells[c][k],left[c],height+row_pad/2+static_cast<int>(k)*pitch,false,numeric[c]?left[c]+widths[c]:0,true,r==0};t.compact=compact;
            lines.push_back(std::move(t));
        }
        height+=row_height;
    }
    height+=after;
    if(compact)SelectObject(dc,prior);
}
static std::vector<TextLine> body_lines(HDC dc,const std::wstring &body,int width,int &height,std::vector<TableBand> *bands=nullptr,bool compact=false) {
    // User 2026-10-27: one reading edge, generous leading, and hanging bullets.
    // Use GDI's measured glyph widths; never split a UTF-16 surrogate pair.
    // User 2026-11-07: consecutive tab-separated lines are one table (first line = header), see table_layout.
    // User 2026-11-13 "조언마다 테이블로 감싸서 배경으로 구분": kCardMark + title starts a card; the title is drawn gold (header line).
    std::vector<TextLine> lines;height=0;int card_top=-1;
    auto close_card=[&] {
        if(card_top<0)return;
        height+=12;if(bands)bands->push_back({card_top,height-card_top,44,56+width+12,kCardBand,false});height+=16;card_top=-1;
    };
    std::vector<std::wstring> paragraphs;
    for(size_t start=0;start<body.size();) {
        size_t end=body.find(L'\n',start);if(end==std::wstring::npos)end=body.size();
        std::wstring paragraph=body.substr(start,end-start);if(!paragraph.empty() && paragraph.back()==L'\r')paragraph.pop_back();
        paragraphs.push_back(std::move(paragraph));start=end<body.size()?end+1:end;
    }
    for(size_t index=0;index<paragraphs.size();) {
        std::wstring paragraph=paragraphs[index];
        if(paragraph.find(L'\t')!=std::wstring::npos) {
            std::vector<std::vector<std::wstring>> rows;
            for(;index<paragraphs.size() && paragraphs[index].find(L'\t')!=std::wstring::npos;++index)rows.push_back(split_cells(paragraphs[index]));
            table_layout(dc,std::move(rows),width,height,lines,bands,compact);continue;
        }
        ++index;
        if(!paragraph.empty() && paragraph[0]==kCardMark) {
            close_card();card_top=height;height+=12;
            paragraph.erase(0,1);
            for(const auto &part:wrap_text(dc,paragraph,width)){TextLine t{part,56,height,false};t.header=true;lines.push_back(std::move(t));height+=36;}
            continue;
        }
        bool bullet=paragraph.size()>=2 && paragraph[0]==L'•' && paragraph[1]==L' ';
        if(bullet)paragraph.erase(0,2);
        size_t split=paragraph.rfind(L"  ·  ");bool amount=false;
        if(bullet && split!=std::wstring::npos && split+5<paragraph.size()) {
            amount=true;for(size_t i=split+5;i<paragraph.size();++i)if((paragraph[i]<L'0' || paragraph[i]>L'9') && paragraph[i]!=L',')amount=false;
            SIZE name{};GetTextExtentPoint32W(dc,paragraph.c_str(),static_cast<int>(split),&name);
            amount=amount && name.cx<240 && width>=600;
        }
        if(paragraph.empty())height+=18;
        else if(amount) {
            lines.push_back({paragraph.substr(0,split),80,height,true});
            lines.push_back({paragraph.substr(split+5),328,height,false,440});height+=36;
        }
        else {
            const auto parts=wrap_text(dc,paragraph,width-(bullet?24:0));
            for(size_t k=0;k<parts.size();++k){lines.push_back({parts[k],56+(bullet?24:0),height,bullet && k==0});height+=36;}
        }
    }
    close_card();
    return lines;
}
// Owned-castle list (2026-11-08 user): row-major three-column grid, no wrapping; a long name is ellipsized by the text draw.
static std::vector<TextLine> city_lines(const std::vector<std::wstring> &cities,int width,int &height) {
    std::vector<TextLine> lines;height=0;
    if(cities.empty())return lines;
    (void)width;   // local240: the items are drawn as cards (city_cards in raster); only the height is measured here
    height=static_cast<int>((cities.size()+advisor_briefing_core::kCityColumns-1)/advisor_briefing_core::kCityColumns)*advisor_briefing_core::city_pitch(static_cast<unsigned>(cities.size()));
    return lines;
}
enum class Layer {Chrome,Body,Badge,Avatar,Highlight,Ticket,Coupon,Detail};
static bool raster(ID3D11DeviceContext *c,const Model &m,Layer layer,unsigned button=0,int state=0) {
    ++raster_calls;
    // Ticket = the blue ambush count left of the head (user 2026-11-11: blue, top-left, not the red unread badge).
    // local64: Ticket (blue circle) = pending proposals; Coupon = ready tickets, a ticket-shaped mark under the head.
    const bool coupon=layer==Layer::Coupon;
    bool ticket=layer==Layer::Ticket,badge=layer==Layer::Badge || ticket,avatar=layer==Layer::Avatar,body=layer==Layer::Body,highlight=layer==Layer::Highlight;
    const bool detail=layer==Layer::Detail;   // local222: the selected message's text in its own scrolling window
    int w=avatar?88:coupon?64:badge?38:static_cast<int>(bubble.w),h=avatar?88:coupon?34:badge?38:static_cast<int>(bubble.h);
    const bool rows_view=report_view(m);   // local222: the rows are the scrolling body
    const unsigned tab_count=report_tabs(m);const float drop=tab_shift(tab_count);   // local229: a second tab line moves the info bar, header and rows
    const bool tall=report_tall(m);   // local235: 로그 has no detail panel
    int height=body_extent,room=static_cast<int>(view_rect(bubble,rows_view,tab_count,tall).h);
    std::vector<TextLine> lines;std::vector<TableBand> bands;
    if(body){HDC measure=CreateCompatibleDC(nullptr);if(!measure)return false;auto prior=SelectObject(measure,cached_font(measure,0));if(rows_view)height=static_cast<int>(m.rows.size())*kRowPitch;else if(m.city_list)lines=city_lines(m.cities,w-112,height);else lines=body_lines(measure,m.body,w-112,height,&bands);SelectObject(measure,prior);DeleteDC(measure);body_extent=height;body_height=(std::max)(1,(std::min)(8192,(std::max)(height,room)));enter_model_lock(false);max_scroll=(std::max)(0,body_height-room);leave_model_lock(false);}
    const Rect detail_window=detail_text_rect(bubble,detail_has_face(m));
    if(detail){HDC measure=CreateCompatibleDC(nullptr);if(!measure)return false;auto prior=SelectObject(measure,cached_font(measure,5));lines=body_lines(measure,m.detail_body,static_cast<int>(detail_window.w)-10,height,&bands,true);SelectObject(measure,prior);DeleteDC(measure);room=static_cast<int>(detail_window.h);detail_height=(std::max)(1,(std::min)(8192,(std::max)(height,room)));enter_model_lock(false);detail_max=(std::max)(0,detail_height-room);leave_model_lock(false);}
    Rect button_rect=control(button,bubble,drop);int bx=static_cast<int>(button_rect.x-bubble.x),by=static_cast<int>(button_rect.y-bubble.y);
    int rw=detail?static_cast<int>(detail_window.w):body?(rows_view?static_cast<int>(view_rect(bubble,true,tab_count,tall).w):w-112):highlight?static_cast<int>(button_rect.w):w,rh=detail?detail_height:body?body_height:highlight?static_cast<int>(button_rect.h):h;
    int hot=highlight && state==0?static_cast<int>(button):0,pressed=highlight && state==1?static_cast<int>(button):0;
    if(pressed)hot=pressed;
#ifndef NO_RUNTIME_LOG
    ULONGLONG started=GetTickCount64();LARGE_INTEGER started_at{};QueryPerformanceCounter(&started_at);
#endif
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=rw;bi.bmiHeader.biHeight=-rh;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return false;void *bits=nullptr;HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bm || !bits){if(bm)DeleteObject(bm);DeleteDC(dc);return false;}
    auto old=SelectObject(dc,bm);auto data=static_cast<unsigned *>(bits);
    auto cr=[](unsigned c){return RGB((c>>16)&255,(c>>8)&255,c&255);};
    auto mix=[](unsigned a,unsigned b,int at,int span){   // 0xRRGGBB, a at 0 .. b at span
        const int t=(std::max)(0,(std::min)(at,span)),d=(std::max)(1,span);unsigned out=0;
        for(int shift=16;shift>=0;shift-=8){int ca=(a>>shift)&255,cb=(b>>shift)&255;out|=static_cast<unsigned>(ca+(cb-ca)*t/d)<<shift;}
        return out;
    };
    const unsigned flat=detail?look::kInfoFill:coupon?look::rgb(198,152,64):body?look::kInset:ticket?look::rgb(44,92,170):badge?look::rgb(163,57,44):highlight?(state==1?look::kButtonDown:look::kButtonHover):0;
    for(int y=0;y<rh;++y) {
        const unsigned row=(body || detail || badge || highlight || coupon)?flat:avatar?mix(look::kPanelTop,look::kPanelBottom,y,h-1):look::kPanelFill;
        for(int x=0;x<rw;++x)data[y*rw+x]=0xff000000u|row;
    }
    if((body && !rows_view) || detail)SetViewportOrgEx(dc,-56,0,nullptr);else if(highlight)SetViewportOrgEx(dc,-bx,-by,nullptr);
    SetBkMode(dc,TRANSPARENT);HFONT font=cached_font(dc,avatar?3:(badge || coupon)?4:detail?5:0);auto oldfont=SelectObject(dc,font);
    HFONT heading=nullptr,caption_font=nullptr;
    auto text=[&](const std::wstring &s,RECT r,COLORREF color,UINT format){SetTextColor(dc,color);DrawTextW(dc,s.c_str(),static_cast<int>(s.size()),&r,format|DT_NOPREFIX);};
    auto outline=[&](RECT r,COLORREF color,COLORREF fill,int radius,bool filled){HPEN pen=CreatePen(PS_SOLID,1,color);HBRUSH brush=filled?CreateSolidBrush(fill):nullptr;auto op=SelectObject(dc,pen),ob=SelectObject(dc,filled?brush:GetStockObject(NULL_BRUSH));RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);SelectObject(dc,ob);SelectObject(dc,op);if(brush)DeleteObject(brush);DeleteObject(pen);};
    auto fill_rect=[&](RECT r,unsigned color){HBRUSH brush=CreateSolidBrush(cr(color));FillRect(dc,&r,brush);DeleteObject(brush);};
    if(avatar)text(L"軍",{0,0,w,h},cr(look::kTitle),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    else if(detail) {   // the whole text, tables as tables; draw() scissors it to the detail text window and scrolls it
        const int width=rw-10;
        for(const auto &band:bands) {
            if(band.index==kCardBand)continue;
            if(band.header){fill_rect({band.x0,band.y,band.x1,band.y+band.h},look::kHeaderFill);fill_rect({band.x0,band.y+band.h-1,band.x1,band.y+band.h},look::kGoldBorder);}
            else if(band.index%2==0)fill_rect({band.x0,band.y,band.x1,band.y+band.h},look::kRowTint);
        }
        HFONT small_font=cached_font(dc,7);
        for(const auto &line:lines) {
            if(line.y>=rh)continue;
            if(line.bullet)text(L"·",{56,line.y,74,line.y+32},cr(look::kGold),DT_SINGLELINE);
            const int tone=line.table && !line.header?cell_tone(line.text):0;
            const COLORREF ink=cr(line.header?look::kGold:tone==1?look::kPositive:tone==2?look::kNegative:look::kText);
            if(line.compact)SelectObject(dc,small_font);   // local232: a table cell in the detail panel's small font
            text(line.text,{line.x,line.y,line.right?line.right:56+width,line.y+(line.compact?look::kDetailLinePitch:32)},ink,DT_SINGLELINE|DT_END_ELLIPSIS|(line.right?DT_RIGHT:0));
            if(line.compact)SelectObject(dc,font);
        }
    }
    else if(coupon) {   // stub with 伏, perforation, count
        outline({0,0,w,h},RGB(96,62,20),RGB(198,152,64),12,true);
        fill_rect({1,1,19,h-1},look::rgb(168,120,44));
        for(int yy=3;yy<h-3;yy+=5)fill_rect({19,yy,20,yy+3},look::rgb(96,62,20));
        text(L"伏",{0,0,19,h},RGB(70,40,10),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        text(m.tickets>99?L"99+":std::to_wstring(m.tickets),{20,0,w,h},RGB(58,32,8),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    else if(ticket){outline({1,1,w-1,h-1},RGB(150,190,240),0,38,false);text(m.tickets>99?L"99+":std::to_wstring(m.tickets),{0,0,w,h},RGB(236,244,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    else if(badge){outline({1,1,w-1,h-1},RGB(211,157,97),0,38,false);text(m.unread>99?L"99+":std::to_wstring(m.unread),{0,0,w,h},RGB(255,242,212),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    else {
        heading=cached_font(dc,1);caption_font=cached_font(dc,2);
        // Dialog button: dark fill, gold outline; hover brightens the fill and the outline, pressed darkens the fill.
        auto button_box=[&](unsigned id,bool over,bool down){
            Rect r=control(id,bubble,drop);RECT box{static_cast<LONG>(r.x-bubble.x),static_cast<LONG>(r.y-bubble.y),static_cast<LONG>(r.x-bubble.x+r.w),static_cast<LONG>(r.y-bubble.y+r.h)};
            outline(box,cr(over?look::kTitle:look::kGoldBorder),cr(down?look::kButtonDown:over?look::kButtonHover:look::kButtonFill),8,true);
            SelectObject(dc,id==2 || id==3 || id==4?heading:caption_font);
            const wchar_t *label=id==2?L"×":id==3?L"‹":id==4?L"›":id==5?(m.offer?L"수락":L"지우기"):id==6?(m.offer?L"거절":L"모두 지우기"):id==8?L"군사 제안":id==13?(m.settings?L"보고로":L"수집 설정"):m.city_list?L"보고로":L"부동산 개발";
            text(label,box,cr(over?look::kButtonHoverText:look::kTitle),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        };
        // local222 report view chrome: tabs, info bar, column header and the detail panel (the rows are the scrolling body).
        auto report_chrome=[&]{
            const HFONT head_font=cached_font(dc,6),body_font=cached_font(dc,5),small_font=cached_font(dc,7);   // local223: a size smaller
            const unsigned tabs=tab_count;
            for(unsigned i=0;i<tabs;++i) {
                const Rect t=tab_rect(bubble,i,tabs);const bool picked=i==m.page;
                const RECT box{static_cast<LONG>(t.x-bubble.x),static_cast<LONG>(t.y-bubble.y),static_cast<LONG>(t.x-bubble.x+t.w),static_cast<LONG>(t.y-bubble.y+t.h)};
                outline(box,cr(picked?look::kTabPickedEdge:look::kTabEdge),cr(picked?look::kTabPicked:look::kTabFill),6,true);
                for(HFONT f:{head_font,body_font,small_font}) {   // the largest font the label fits in
                    SelectObject(dc,f);SIZE e{};GetTextExtentPoint32W(dc,m.tabs[i].c_str(),static_cast<int>(m.tabs[i].size()),&e);
                    if(e.cx<=box.right-box.left-16)break;
                }
                text(m.tabs[i],{box.left+8,box.top,box.right-8,box.bottom},cr(picked?look::kTabPickedText:look::kTabText),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
            }
            if(tabs) {   // local228: a parchment rule under the tabs joins the open tab to the rows (it covers the tabs' lower corners)
                const Rect t0=tab_rect(bubble,0,tabs),last=tab_rect(bubble,tabs-1,tabs);const int ty=static_cast<int>(last.y-bubble.y+last.h);   // local229: under the last line
                fill_rect({static_cast<int>(t0.x-bubble.x),ty-4,w-24,ty+1},look::kTabPicked);
            }
            const Rect info=info_rect(bubble,tabs);
            const int ix=static_cast<int>(info.x-bubble.x),iy=static_cast<int>(info.y-bubble.y),iw=static_cast<int>(info.w),ih=static_cast<int>(info.h);
            fill_rect({ix,iy+ih-1,ix+iw,iy+ih},look::kGoldDim);   // local228: the info line sits on the panel, a rule below it
            int sx=ix+16;const int stop=static_cast<int>(control(13,bubble,drop).x-bubble.x)-12;
            for(size_t i=0;i<m.facts.size();++i) {
                const auto cells=split_cells(m.facts[i]);
                const std::wstring label=cells[0],value=cells.size()>1?cells[1]:std::wstring();
                SIZE a{},b{};SelectObject(dc,small_font);GetTextExtentPoint32W(dc,label.c_str(),static_cast<int>(label.size()),&a);
                SelectObject(dc,body_font);GetTextExtentPoint32W(dc,value.c_str(),static_cast<int>(value.size()),&b);
                const int span=static_cast<int>(a.cx+b.cx)+40;if(sx+span>stop)break;   // "label value" on one line, segments split by a rule
                if(i)fill_rect({sx-15,iy+12,sx-14,iy+ih-12},look::kGoldDim);
                SelectObject(dc,small_font);text(label,{sx,iy,sx+static_cast<int>(a.cx)+2,iy+ih},cr(look::kDim),DT_SINGLELINE|DT_VCENTER);
                SelectObject(dc,body_font);text(value,{sx+static_cast<int>(a.cx)+10,iy,sx+span,iy+ih},cr(look::kText),DT_SINGLELINE|DT_VCENTER);
                sx+=span;
            }
            const Rect column=column_rect(bubble,tabs);
            const int cx0=static_cast<int>(column.x-bubble.x),cy0=static_cast<int>(column.y-bubble.y),cw=static_cast<int>(column.w),day_x=cw-kColDayPad-kColDayWidth;
            fill_rect({cx0,cy0,cx0+cw,cy0+kHeadHeight},look::kHeaderFill);fill_rect({cx0,cy0+kHeadHeight-1,cx0+cw,cy0+kHeadHeight},look::kGoldBorder);
            SelectObject(dc,small_font);
            const int summary_x=m.kinds?kColSummary:kColChips;   // local228: 구분 only when the tab mixes topics
            text(L"인물",{cx0+kColName,cy0,cx0+kColChips,cy0+kHeadHeight},cr(look::kHeaderText),DT_SINGLELINE|DT_VCENTER);
            if(m.kinds)text(L"구분",{cx0+kColChips,cy0,cx0+kColSummary,cy0+kHeadHeight},cr(look::kHeaderText),DT_SINGLELINE|DT_VCENTER);
            text(L"내용",{cx0+summary_x,cy0,cx0+day_x,cy0+kHeadHeight},cr(look::kHeaderText),DT_SINGLELINE|DT_VCENTER);
            text(tall?L"위치":L"일차",{cx0+day_x,cy0,cx0+cw-kColDayPad,cy0+kHeadHeight},cr(look::kHeaderText),DT_SINGLELINE|DT_VCENTER|DT_RIGHT);   // local235: 로그 rows carry the map place, not a day
            for(int at:{kColChips,kColSummary,day_x})if(at!=kColSummary || m.kinds)fill_rect({cx0+at-10,cy0+8,cx0+at-9,cy0+kHeadHeight-8},look::kGoldDim);
            if(tall)return;   // local235: 로그 draws no detail panel (the rows run to its bottom edge)
            // Detail panel frame: the face square and title on the left (the text is the Detail layer), the same name's other messages on the right.
            const Rect detail=detail_rect(bubble);
            const int dx=static_cast<int>(detail.x-bubble.x),dy=static_cast<int>(detail.y-bubble.y),dw=static_cast<int>(detail.w),dh=static_cast<int>(detail.h);
            outline({dx,dy,dx+dw,dy+dh},cr(look::kGoldDim),cr(look::kInfoFill),10,true);
            const int split=dx+dw*11/20;fill_rect({split,dy+14,split+1,dy+dh-14},look::kGoldDim);
            // A row with a person reserves the face square (its initial here; draw() puts the face over it) left of the title and text.
            const bool detail_face=detail_has_face(m);
            const int tx=detail_face?dx+18+kDetailFace+16:dx+18;
            if(detail_face) {
                const ReportRow &who=m.rows[m.selected];const Rect fr=detail_face_rect(bubble);
                const RECT square{static_cast<LONG>(fr.x-bubble.x),static_cast<LONG>(fr.y-bubble.y),static_cast<LONG>(fr.x-bubble.x+fr.w),static_cast<LONG>(fr.y-bubble.y+fr.h)};
                fill_rect(square,mix(look::kInset,look::kNames[name_color_index(who.name)],1,3));outline(square,cr(look::kGoldDim),0,6,false);
                SelectObject(dc,head_font);text(who.name.substr(0,1),square,cr(look::kTitle),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            }
            // local228: the name large, then "topic, day" small after it (the first proposal).
            SelectObject(dc,head_font);SIZE named{};GetTextExtentPoint32W(dc,m.detail_title.c_str(),static_cast<int>(m.detail_title.size()),&named);
            const int title_end=(std::min)(tx+static_cast<int>(named.cx),split-140);
            text(m.detail_title,{tx,dy+8,title_end,dy+46},cr(look::kTitle),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
            SelectObject(dc,small_font);text(m.detail_caption,{title_end+12,dy+8,split-16,dy+46},cr(look::kDimmer),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
            // The text itself is the Detail layer: its own window under the title, scrolled by the wheel (draw()).
            SelectObject(dc,head_font);text(L"기록",{split+18,dy+8,split+100,dy+46},cr(look::kTitle),DT_SINGLELINE|DT_VCENTER);
            SelectObject(dc,small_font);
            text(m.records.empty()?std::wstring(L"이번 순에 같은 인물의 다른 소식은 없습니다"):std::wstring(L"같은 인물의 다른 소식"),{split+100,dy+8,dx+dw-16,dy+46},cr(look::kDimmer),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
            for(size_t i=0;i<m.records.size();++i) {   // "day  summary" (the first proposal; the topic stays in the data)
                const int ry=dy+54+static_cast<int>(i)*36;if(ry+34>dy+dh-8)break;
                const auto cells=split_cells(m.records[i]);
                SelectObject(dc,small_font);text(cells[0],{split+18,ry,split+96,ry+34},cr(look::kDim),DT_SINGLELINE|DT_VCENTER);
                SelectObject(dc,body_font);if(cells.size()>2)text(cells[2],{split+100,ry,dx+dw-16,ry+34},cr(look::kText),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
                fill_rect({split+18,ry+34,dx+dw-16,ry+35},look::kRule);
            }
        };
        // local222: one 52px row per message. The face square holds the initial; the person's face is drawn over it as a quad.
        auto report_rows=[&]{
            const HFONT head_font=cached_font(dc,6),body_font=cached_font(dc,5),small_font=cached_font(dc,7);   // local223: a size smaller
            const unsigned separator=mix(look::kInset,look::kGoldDim,1,3);
            for(size_t i=0;i<m.rows.size();++i) {
                const ReportRow &r=m.rows[i];const int top_y=static_cast<int>(i)*kRowPitch;if(top_y>=rh)break;
                const bool picked=i==m.selected;
                fill_rect({0,top_y,rw,top_y+kRowPitch},picked?look::kPickedTint:i%2?look::kRowTint:look::kInset);
                fill_rect({8,top_y+kRowPitch-1,rw-8,top_y+kRowPitch},separator);
                if(picked){outline({0,top_y,rw,top_y+kRowPitch},cr(look::kPicked),0,8,false);outline({1,top_y+1,rw-1,top_y+kRowPitch-1},cr(look::kPicked),0,8,false);}
                if(r.unread)fill_rect({4,top_y+22,9,top_y+30},look::kGold);   // not read before this page was opened
                if(r.named) {   // local228: a row without a name has no square and no name (no stand-in initial)
                    const RECT square{kColFace,top_y+4,kColFace+kFaceSize,top_y+4+kFaceSize};
                    fill_rect(square,mix(look::kInset,look::kNames[name_color_index(r.name)],1,3));
                    outline(square,cr(look::kGoldDim),0,4,false);
                    SelectObject(dc,head_font);
                    text(r.name.substr(0,1),square,cr(look::kTitle),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                    text(r.name,{kColName,top_y,kColChips-12,top_y+kRowPitch},cr(look::kTitle),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
                }
                if(m.kinds) {   // local228 구분: plain words, a result word in its colour (no boxes)
                    SelectObject(dc,small_font);int word_x=kColChips;
                    for(const auto &chip:r.chips) {
                        SIZE e{};GetTextExtentPoint32W(dc,chip.text.c_str(),static_cast<int>(chip.text.size()),&e);
                        if(word_x>=kColSummary-12)break;
                        const unsigned ink=chip.tone==1?look::kPositive:chip.tone==2?look::kNegative:look::kDim;
                        text(chip.text,{word_x,top_y,kColSummary-12,top_y+kRowPitch},cr(ink),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
                        word_x+=static_cast<int>(e.cx)+8;
                    }
                }
                SelectObject(dc,body_font);
                const int map_x=map_button_x(rw);   // local243: the same row-local x as map_button_rect (hit test)
                text(r.summary,{m.kinds?kColSummary:kColChips,top_y,r.place?map_x-10:rw-kColDayPad-kColDayWidth-8,top_y+kRowPitch},cr(look::kText),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
                SelectObject(dc,small_font);
                if(r.place) {
                    const int by0=top_y+(kRowPitch-kMapHeight)/2;
                    outline({map_x,by0,map_x+kMapWidth,by0+kMapHeight},cr(look::kGoldBorder),cr(look::kButtonFill),8,true);
                    text(L"지도 이동",{map_x,by0,map_x+kMapWidth,by0+kMapHeight},cr(look::kGold),DT_SINGLELINE|DT_VCENTER|DT_CENTER);
                }
                text(r.day,{rw-kColDayPad-kColDayWidth,top_y,rw-kColDayPad,top_y+kRowPitch},cr(look::kDim),DT_SINGLELINE|DT_VCENTER|DT_RIGHT);
            }
        };
        if(!body && !highlight){
            // local228 frame: the original list window's cell as a nine-slice over the whole panel. Its 4 px clear margin is cut off (the
            // panel quad rounds that corner), the corners stay 1:1 and the edges stretch; colours as stored, the panel's own opacity for
            // the original's 75% plus blur. The cell has gold double lines along the top and bottom only, with a fret knot in each
            // corner. Without the cell: the same two lines at top and bottom, without the knots.
            bool framed=false;
            AcquireSRWLockShared(&frame_lock);
            if(!frame_rgba.empty()) {
                const int m0=look::kFrameMargin,s=look::kFrameSlice,cw=static_cast<int>(frame_w)-2*m0,ch=static_cast<int>(frame_h)-2*m0;
                auto cell_at=[](int at,int size,int cell,int slice){return at<slice?at:at>=size-slice?cell-(size-at):slice+(at-slice)*(cell-2*slice)/(std::max)(1,size-2*slice);};
                GdiFlush();
                // Pixels thinner than the 75% fill (antialiased line edges, the clear corner) blend toward the fill, so the texture stays opaque.
                const unsigned fill[3]={(look::kPanelFill>>16)&255u,(look::kPanelFill>>8)&255u,look::kPanelFill&255u};
                for(int y=0;y<h;++y) {
                    const unsigned char *src=frame_rgba.data()+static_cast<size_t>(m0+cell_at(y,h,ch,s))*frame_w*4;
                    for(int x=0;x<w;++x) {
                        const unsigned char *p=src+static_cast<size_t>(m0+cell_at(x,w,cw,s))*4;const unsigned a=(std::min)(191u,static_cast<unsigned>(p[3]));
                        unsigned frame_px[3];for(int k=0;k<3;++k)frame_px[k]=(fill[k]*(191-a)+p[k]*a)/191;
                        data[y*w+x]=0xff000000u|frame_px[0]<<16|frame_px[1]<<8|frame_px[2];
                    }
                }
                framed=true;
            }
            ReleaseSRWLockShared(&frame_lock);
            if(!framed)for(int y:{1,4,h-5,h-2})fill_rect({1,y,w-1,y+1},look::kFrameLine);   // the cell's double lines run along the top and bottom only
            if(!rows_view)fill_rect({16,112,w-16,113},look::kGoldDim);   // title / body rule (the report view has its tabs there)
            fill_rect({44,34,47,86},look::kGold);
            if(!rows_view){outline({16,132,w-16,h-96},cr(look::kGoldDim),cr(look::kInset),16,true);fill_rect({56,222,w-56,223},look::kGoldDim);}
            SelectObject(dc,heading);text(m.offer || m.advice?m.adviser+L"의 진언":m.settings?L"수집 설정":m.city_list?L"부동산 개발":m.adviser+L"의 열흘 보고",{64,30,w-354,68},cr(look::kTitle),DT_SINGLELINE|DT_VCENTER);
            SelectObject(dc,caption_font);text(m.advice?L"군사 제안  /  준비된 작전":m.offer?(m.offer_kind==1?L"파괴공작  /  금 1000":L"매복기동  /  금 1000"):m.settings?L"켜면 군사가 알림함에 모아 보고  ·  끄면 원작 창과 연출 그대로":m.city_list?L"성을 고르면 그 성의 지역내정으로 이동합니다":m.collecting?L"진행 중  /  소식을 모으고 있습니다":L"다음 진행을 시작하면 지워집니다",{65,76,w-80,105},cr(look::kDim),DT_SINGLELINE);
            if(rows_view)report_chrome();
            else {SelectObject(dc,font);text(m.title,{56,184,w-412,220},cr(look::kText),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);}   // local240 "UI맞춰주고": on the 수집 설정/군사 제안/부동산 개발 button line (187..217)
            for(unsigned id:kButtons)if(button_shown(m,id))button_box(id,false,false);
            SelectObject(dc,caption_font);text(m.settings?std::wstring():m.city_list?(m.cities.empty()?std::wstring(L"성 없음"):std::to_wstring(m.cities.size())+L"곳"):(m.offer || m.advice?std::wstring():m.pages?std::to_wstring(m.page+1)+L" / "+std::to_wstring(m.pages):std::wstring(L"소식 없음")),{w-272,30,w-164,68},cr(look::kDim),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            text(L"Esc로 닫기",{44,h-64,250,h-26},cr(look::kDim),DT_VCENTER|DT_SINGLELINE);
            if(height>room)text(L"마우스 휠로 이어 읽기",{300,h-64,w-300,h-26},cr(look::kDimmer),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }
        if(highlight)button_box(button,hot==static_cast<int>(button),pressed==static_cast<int>(button));
        if(body && rows_view)report_rows();
        else if(body && m.city_list) {
            // local240 cards: this layer draws with the viewport origin at -56 (as the body text, at panel x), so a card at its hit cell
            // (city_card: body_rect x + col*cell) is drawn at 56+col*cell.
            // Card = vertical shade, gold-dim frame, a left accent bar; 수집 설정 cards show the topic left and its state as a chip right
            // (알림함 gold, 원작대로 dim); city cards show the name and a dim "내정 ›" cue. Hover is a tint quad drawn by the frame.
            const unsigned count=static_cast<unsigned>(m.cities.size());const int pitch=city_pitch(count),cell=rw/kCityColumns;
            HFONT chip_font=cached_font(dc,2);
            for(unsigned i=0;i<count;++i) {
                const int x0=56+static_cast<int>(i%kCityColumns)*cell+kCardInsetX,y0=static_cast<int>(i/kCityColumns)*pitch+kCardInsetY;
                const int x1=x0+cell-2*kCardInsetX,y1=y0+pitch-2*kCardInsetY;if(y0>=rh)break;
                const std::wstring &item=m.cities[i];const size_t dot=item.find(L"  ·  ");
                const bool topic=dot!=std::wstring::npos;const std::wstring name=topic?item.substr(0,dot):item,status=topic?item.substr(dot+5):std::wstring();
                const bool on=!topic || status==L"알림함";
                for(int y=y0;y<y1;++y)fill_rect({x0,y,x1,y+1},mix(look::kHeaderFill,look::kButtonFill,y-y0,y1-y0));
                outline({x0,y0,x1,y1},cr(on?look::kGoldBorder:look::kGoldDim),0,6,false);
                fill_rect({x0+1,y0+4,x0+5,y1-4},on?look::kGold:look::kGoldDim);
                SelectObject(dc,font);text(name,{x0+18,y0,x1-(topic?96:84),y1},cr(on?look::kText:look::kDim),DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
                SelectObject(dc,chip_font);
                if(topic) {
                    // toggle (user 2026-11-19 "원작대로 라고 하지말고 ... 토글"): on = gold track, knob right; off = dark track, knob left
                    const int cx1=x1-14,cx0=cx1-56,cy0=(y0+y1)/2-13,cy1=cy0+26;
                    outline({cx0,cy0,cx1,cy1},cr(on?look::kGold:look::kGoldDim),cr(on?look::kGoldBorder:look::kButtonDown),26,true);
                    const int knob=on?cx1-24:cx0+2;
                    outline({knob,cy0+2,knob+22,cy1-2},cr(on?look::kTitle:look::kDimmer),cr(on?look::kTitle:look::kDimmer),22,true);
                } else text(L"내정 ›",{x1-84,y0,x1-14,y1},cr(look::kDim),DT_SINGLELINE|DT_VCENTER|DT_RIGHT);
                SelectObject(dc,font);
            }
        }
        else if(body){
            for(const auto &band:bands) {
                if(band.index==kCardBand) {
                    fill_rect({band.x0,band.y,band.x1,band.y+band.h},look::kRowTint);
                    fill_rect({band.x0,band.y,band.x1,band.y+1},look::kGoldDim);fill_rect({band.x0,band.y+band.h-1,band.x1,band.y+band.h},look::kGoldDim);
                    fill_rect({band.x0,band.y,band.x0+1,band.y+band.h},look::kGoldDim);fill_rect({band.x1-1,band.y,band.x1,band.y+band.h},look::kGoldDim);
                    continue;
                }
                if(band.header){fill_rect({band.x0,band.y,band.x1,band.y+band.h},look::kHeaderFill);fill_rect({band.x0,band.y+band.h-1,band.x1,band.y+band.h},look::kGoldBorder);}
                else {
                    if(band.index%2==0)fill_rect({band.x0,band.y,band.x1,band.y+band.h},look::kRowTint);
                    fill_rect({band.x0+8,band.y+band.h-1,band.x1-8,band.y+band.h},look::kGoldDim);
                }
            }
            for(const auto &line:lines) {
                int y=line.y;if(y>=rh)continue;
                if(line.bullet)text(L"·",{56,y,74,y+32},cr(look::kGold),DT_SINGLELINE);
                COLORREF ink=cr(line.header?look::kGold:look::kText);
                if(line.table) {
                    const int tone=line.header?0:cell_tone(line.text);
                    ink=cr(line.header?look::kGold:tone==1?look::kPositive:tone==2?look::kNegative:look::kText);
                }
                text(line.text,{line.x,y,line.right?line.right:w-56,y+32},ink,DT_SINGLELINE|(line.right?DT_RIGHT:0));
                SIZE full{};GetTextExtentPoint32W(dc,line.text.c_str(),static_cast<int>(line.text.size()),&full);int origin=line.right?line.right-full.cx:line.x;
                for(const auto &name:m.names)for(size_t at=0;(at=line.text.find(name,at))!=std::wstring::npos;at+=name.size()) {
                    SIZE prefix{};GetTextExtentPoint32W(dc,line.text.c_str(),static_cast<int>(at),&prefix);
                    SetTextColor(dc,cr(look::kNames[name_color_index(name)]));TextOutW(dc,origin+prefix.cx,y,name.c_str(),static_cast<int>(name.size()));
                }
            }
        }
    }
    // GDI clears alpha for glyph pixels; restore opaque alpha before the shader's
    // geometric mask. There are no black rectangles around the circular portrait.
    for(int i=0;i<rw*rh;++i)data[i]|=0xff000000u;
#ifndef NO_RUNTIME_LOG
    LARGE_INTEGER drawn_at{};QueryPerformanceCounter(&drawn_at);
#endif
    bool ok=false;
    if(avatar)ok=upload(c,avatar_texture,avatar_view,rw,rh,data);
    else if(coupon)ok=upload(c,coupon_texture,coupon_view,rw,rh,data);
    else if(ticket)ok=upload(c,ticket_texture,ticket_view,rw,rh,data);
    else if(badge)ok=upload(c,badge_texture,badge_view,rw,rh,data);
    else if(detail){ok=upload(c,detail_texture,detail_view,rw,rh,data);if(ok)++panel_uploads;}
    else if(body){ok=upload(c,body_texture,body_view,rw,rh,data);if(ok)++panel_uploads;}
    else if(highlight){auto &item=highlights[button][state];ok=upload(c,item.texture,item.view,rw,rh,data);}
    else {ok=upload(c,panel_texture,panel_view,rw,rh,data);if(ok)++panel_uploads;}
    SelectObject(dc,oldfont);SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);
#ifndef NO_RUNTIME_LOG
    LONG64 elapsed=static_cast<LONG64>(GetTickCount64()-started);InterlockedExchange64(&raster_ms,elapsed);if(elapsed>raster_max_ms)InterlockedExchange64(&raster_max_ms,elapsed);
    if(elapsed>=20 && (body || detail || layer==Layer::Chrome)) {
        log_event("advisor_raster_slow",static_cast<unsigned>(layer),static_cast<unsigned>(elapsed),static_cast<unsigned>(rh));   // local223: open-time cost
        LARGE_INTEGER uploaded_at{},hz{};QueryPerformanceCounter(&uploaded_at);QueryPerformanceFrequency(&hz);const LONGLONG f=hz.QuadPart?hz.QuadPart:1;
        // local229: where the time went: layer, GDI drawing µs, texture upload µs (the slow log's ms has 15.6 ms steps)
        log_event("advisor_raster_split",static_cast<unsigned>(layer),static_cast<unsigned>((drawn_at.QuadPart-started_at.QuadPart)*1000000/f),static_cast<unsigned>((uploaded_at.QuadPart-drawn_at.QuadPart)*1000000/f));
    }
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
void draw(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc) {
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
    if(head.snapping || scrolling.active() || scrolling.shown>max_scroll || detail_scroll.active() || detail_scroll.shown>detail_max)animation_at=now;else animation_at=0;
    if(head.snapping)head.advance(dt);
    if(scrolling.active() || scrolling.shown>max_scroll)scrolling.advance(dt,max_scroll);
    if(detail_scroll.active() || detail_scroll.shown>detail_max)detail_scroll.advance(dt,detail_max);
    if(closing && now-closed_at>=140)closing=false;
    HeadMotion position=head;double sy=scrolling.shown,dsy=detail_scroll.shown;int hot=hover,pressed=armed;
    bool scroll_active=scrolling.active() || detail_scroll.active();
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
    if(flag(&toast_only)) {
        bool live=false;enter_model_lock(true);live=!toast_text.empty() && toast_at && now<toast_at+toast_ms();leave_model_lock(true);
        if(!live){InterlockedExchange(&visible,0);InterlockedExchange(&toast_only,0);return;}
    }
    bool next_srgb=desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB || desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if(srgb_target!=next_srgb) {
        enter_model_lock(false);portrait_confirmation.reset();leave_model_lock(false);
        srgb_target=next_srgb;drop(body_view);drop(body_texture);drop(detail_view);drop(detail_texture);
        for(auto &by_id:highlights)for(auto &item:by_id){drop(item.view);drop(item.texture);}
        drop(portrait_staging);drop(portrait_candidate_view);drop(portrait_candidate);drop(panel_view);drop(panel_texture);drop(badge_view);drop(badge_texture);drop(ticket_view);drop(ticket_texture);drop(coupon_view);drop(coupon_texture);drop(offer_view);drop(offer_texture);drop(avatar_view);drop(avatar_texture);
        drop(shadow_view);drop(shadow_texture);drop(ring_view);drop(ring_texture);drop(portrait_view);drop(portrait_texture);InterlockedExchange(&portrait_captured,0);
        drop_row_faces();   // their views carry the old sRGB choice; the UI thread asks again
        painted_generation=painted_unread=painted_tickets=painted_coupon=~0u;painted_offer=-1;painted_content=~0u;
    }
    double scale=(std::min)(desc.Width/1920.0,desc.Height/1080.0),ox=(desc.Width-1920*scale)/2,oy=(desc.Height-1080*scale)/2;
    take_face_offer(d,c);   // briefing16 U6: original face texture first; the capture below stays as the fallback
    take_row_face(d,c);     // local222: one row face per frame at most
    poll_portrait(c);
    enter_model_lock(true);bool sample_due=!portrait_confirmation.present || (now>=portrait_confirmation.at && now-portrait_confirmation.at>=500);leave_model_lock(true);
    if(flag(&portrait_requested) && sample_due && !portrait_staging && (!portrait_attempt_at || now-portrait_attempt_at>=120)){
        portrait_attempt_at=now;enter_model_lock(true);Rect source_rect=source;int revision=source_revision;leave_model_lock(true);
        int x=static_cast<int>(ox+source_rect.x*scale),y=static_cast<int>(oy+source_rect.y*scale),w=static_cast<int>(source_rect.w*scale),h=static_cast<int>(source_rect.h*scale);
        if(x>=0 && y>=0 && w>8 && h>8 && x+w<=static_cast<int>(desc.Width) && y+h<=static_cast<int>(desc.Height))capture_portrait(d,c,back,desc,x,y,w,h,revision,source_rect);
    }
    if((!flag(&portrait_captured) || !portrait_view) && !avatar_view && !raster(c,m,Layer::Avatar))return;
    if(painted_offer!=(m.offer?1:0)){   // 수락/거절 vs 지우기 labels are baked into the button highlights
        for(unsigned id=5;id<=6;++id)for(int state=0;state<2;++state){drop(highlights[id][state].view);drop(highlights[id][state].texture);}
        painted_offer=m.offer?1:0;
    }
    if(m.open && (painted_content!=content || !panel_view || !body_view || (detail_shown(m) && !detail_view))){
        if(!raster(c,m,Layer::Body) || (detail_shown(m) && !raster(c,m,Layer::Detail)) || !raster(c,m,Layer::Chrome))return;
        for(unsigned id:kButtons)for(int state=0;state<2;++state)if(!highlights[id][state].view && !raster(c,m,Layer::Highlight,id,state))return;
        painted_content=content;painted_generation=m.generation;
    }
    if(painted_unread!=m.unread){if(m.unread && !raster(c,m,Layer::Badge))return;painted_unread=m.unread;}
    if(painted_tickets!=m.tickets){if(m.tickets && !raster(c,m,Layer::Ticket))return;painted_tickets=m.tickets;}
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
    if(m.tickets && ticket_view)quad({position.x-56,position.y-52,38,38},scale,ox,oy,19,1,ticket_view);   // blue = ready ambush tickets
    if(offer_balloon(m) && !is_closing && raster_offer(c,m.offer_kind))quad(offer_box(position.x,position.y),scale,ox,oy,12,1,offer_view);   // blue ambush count, top-left
    {   // 2026-11-08 user: phone-message style military message next to the advisor head, 5 s.
        std::wstring tip;ULONGLONG tip_at=0;enter_model_lock(true);if(!toast_text.empty()){tip=toast_text;tip_at=toast_at;}leave_model_lock(true);
        const ULONGLONG span=toast_ms();
        if(!tip.empty() && tip_at && now>=tip_at && now<tip_at+span && raster_toast(c,tip)) {
            const ULONGLONG age=now-tip_at;float alpha=age>span-500?static_cast<float>((span-age)/500.0):1.0f;
            const float toast_h=static_cast<float>(toast_drawn_h);
            Rect box{bounded(face.x-236.0f,16.0f,1904.0f-560.0f),face.y+face.h+10.0f,560.0f,toast_h};
            if(box.y+box.h>1064)box.y=face.y-toast_h-10.0f;
            quad(box,scale,ox,oy,12,alpha,toast_view);
        }
    }
    LONG first_row=-1;
    if(panel_alpha>0 && panel_view){
        Rect base=panel_rect(position.x);auto transform=[&](Rect r){return panel_transform(r,position.x,position.y,zoom);};
        Rect panel=transform(base);float alpha=static_cast<float>(panel_alpha);
        const float opaque=alpha*look::kPanelOpacity;   // semi-transparent navy: chrome and body never overlap (chrome is drawn in four strips around the body window)
        quad({panel.x-10,panel.y-4,panel.w+20,panel.h+20},scale,ox,oy,-2,alpha*.36f,shadow_view);
        bool right=position.x>=960;float ty=bounded(position.y-14,base.y+24,base.y+base.h-52);
        quad(transform({right?base.x+base.w-4:base.x-18,ty,22,28}),scale,ox,oy,right?-1.25f:-1,opaque,panel_view);
        const bool rows_view=report_view(m),tall=report_tall(m),detail_on=rows_view && !tall;const unsigned tabs=report_tabs(m);const Rect area=view_rect(base,rows_view,tabs,tall);   // local222: the rows scroll in the report view; local235: 로그 (tall) has no detail window
        Rect clip=transform(area);D3D11_RECT scissor{static_cast<LONG>(std::ceil(ox+clip.x*scale)),static_cast<LONG>(std::ceil(oy+clip.y*scale)),static_cast<LONG>(std::floor(ox+(clip.x+clip.w)*scale)),static_cast<LONG>(std::floor(oy+(clip.y+clip.h)*scale))};
        const LONG left=static_cast<LONG>(std::floor(ox+panel.x*scale)),top=static_cast<LONG>(std::floor(oy+panel.y*scale)),right_edge=static_cast<LONG>(std::ceil(ox+(panel.x+panel.w)*scale)),bottom=static_cast<LONG>(std::ceil(oy+(panel.y+panel.h)*scale));
        // local222: the report view has a second window, the detail text below the rows; the chrome is drawn in seven strips around both.
        const Rect text_window=detail_text_rect(base,detail_has_face(m)),text_area=transform(text_window);
        const D3D11_RECT text_clip{static_cast<LONG>(std::ceil(ox+text_area.x*scale)),static_cast<LONG>(std::ceil(oy+text_area.y*scale)),static_cast<LONG>(std::floor(ox+(text_area.x+text_area.w)*scale)),static_cast<LONG>(std::floor(oy+(text_area.y+text_area.h)*scale))};
        const D3D11_RECT strips[7]{{left,top,right_edge,scissor.top},{left,scissor.top,scissor.left,scissor.bottom},{scissor.right,scissor.top,right_edge,scissor.bottom},
            {left,scissor.bottom,right_edge,detail_on?text_clip.top:bottom},{left,text_clip.top,text_clip.left,text_clip.bottom},{text_clip.right,text_clip.top,right_edge,text_clip.bottom},{left,text_clip.bottom,right_edge,bottom}};
        c->RSSetState(scissor_state);
        for(size_t s=0;s<(detail_on?7u:4u);++s){c->RSSetScissorRects(1,&strips[s]);quad(panel,scale,ox,oy,4,opaque,panel_view);}   // local228: the original frame's small corner
        c->RSSetScissorRects(1,&scissor);
        quad(transform({area.x,area.y-static_cast<float>(sy),area.w,static_cast<float>(body_height)}),scale,ox,oy,0,opaque,body_view);
        if(rows_view) {   // row faces over the drawn initials, inside the rows window
            const size_t from=static_cast<size_t>((std::max)(0.0,sy)/kRowPitch);first_row=static_cast<LONG>(from);
            for(size_t i=from;i<m.rows.size() && i<from+(tall?14:9);++i)if(const unsigned person=m.rows[i].person)
                for(auto &f:row_faces)if(f.person==person){if(f.view){quad(transform(face_rect(base,static_cast<unsigned>(i),sy,tabs,tall)),scale,ox,oy,4,alpha,f.view);f.used=now;}break;}
        }
        // Hover on a row is a tint quad over the warmed rows texture (no raster).
        if(rows_view && hot>=1000 && static_cast<size_t>(hot-1000)<m.rows.size())quad(transform(row_rect(base,static_cast<unsigned>(hot-1000),sy,tabs,tall)),scale,ox,oy,0,alpha*.14f,ring_view);
        if(rows_view && hot>=kMapHit && static_cast<size_t>(hot-kMapHit)<m.rows.size())quad(transform(map_button_rect(base,static_cast<unsigned>(hot-kMapHit),sy,tabs,tall)),scale,ox,oy,8,alpha*.30f,ring_view);   // local243
        // local240: hover on a card (수집 설정 / 부동산 개발) lights that card the same way
        if(!rows_view && m.city_list && hot>=100 && hot<1000 && static_cast<size_t>(hot-100)<m.cities.size())quad(transform(city_card(base,static_cast<unsigned>(hot-100),static_cast<unsigned>(m.cities.size()))),scale,ox,oy,0,alpha*.16f,ring_view);
        if(detail_on && detail_view){c->RSSetScissorRects(1,&text_clip);quad(transform({text_window.x,text_window.y-static_cast<float>(dsy),text_window.w,static_cast<float>(detail_height)}),scale,ox,oy,0,opaque,detail_view);}
        c->RSSetState(raster_state);
        if(detail_on && m.selected<m.rows.size() && m.rows[m.selected].person)
            for(auto &f:row_faces)if(f.person==m.rows[m.selected].person){if(f.view){quad(transform(detail_face_rect(base)),scale,ox,oy,8,alpha,f.view);f.used=now;}break;}
        if(body_height>area.h){const float bar=rows_view?base.x+base.w-34:base.x+base.w-37;float thumb=(std::max)(32.0f,area.h*area.h/body_height),offset=bounded(static_cast<float>(sy),0,static_cast<float>(max_scroll))*(area.h-thumb)/max_scroll;
            quad(transform({bar,area.y,3,area.h}),scale,ox,oy,1,alpha*.16f,ring_view);
            quad(transform({bar,area.y+offset,3,thumb}),scale,ox,oy,1,alpha*.65f,ring_view);}
        if(detail_on && detail_height>text_window.h){const float bar=text_window.x+text_window.w-5;const float range=static_cast<float>((std::max)(1,detail_max));
            const float thumb=(std::max)(24.0f,text_window.h*text_window.h/detail_height),offset=bounded(static_cast<float>(dsy),0,range)*(text_window.h-thumb)/range;
            quad(transform({bar,text_window.y,3,text_window.h}),scale,ox,oy,1,alpha*.16f,ring_view);
            quad(transform({bar,text_window.y+offset,3,thumb}),scale,ox,oy,1,alpha*.65f,ring_view);}
        for(unsigned id:kButtons)if(hot==static_cast<int>(id) || pressed==static_cast<int>(id)){
            auto &item=highlights[id][pressed==static_cast<int>(id)?1:0];
            if(item.view)quad(transform(control(id,base,tab_shift(tabs))),scale,ox,oy,4,opaque,item.view);
        }
        if(rows_view && hot>=40 && hot<40+static_cast<int>(tabs))
            quad(transform(tab_rect(base,static_cast<unsigned>(hot-40),tabs)),scale,ox,oy,6,alpha*.22f,ring_view);
    }
    InterlockedExchange(&row_first,first_row);
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
