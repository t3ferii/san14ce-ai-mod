// User/Claude 2026-10-29: standalone fixture instrumentation declaration.
#ifndef COST_SCOPE
#define COST_SCOPE(name) do {} while(0)
#endif
// User 2026-10-29: exercise production original_site with mocked Windows APIs.
// A game-folder proxy may own the basename while code/table live in System32.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <initializer_list>
static bool enabled=true;
static struct {bool convenience=true,terrain_edit=true;unsigned terrain_diag_mode=0;} mod_features;
#ifndef NO_RUNTIME_LOG
static void log_event(const char *,unsigned,unsigned,unsigned) {}
#endif
static unsigned checks,named_lookups,module_path_calls,system_path_calls,address_calls;
static void require(bool ok,const char *label) {++checks;if(!ok){fprintf(stderr,"FAIL %s (%u)\n",label,checks);ExitProcess(1);}}
static HMODULE code_module=reinterpret_cast<HMODULE>(static_cast<uintptr_t>(0x1000));
static HMODULE table_module=code_module;
static HMODULE proxy_module=reinterpret_cast<HMODULE>(static_cast<uintptr_t>(0x2000));
static const wchar_t *module_path=L"C:\\Windows\\System32\\d3d11.dll",*system_directory=L"C:\\Windows\\System32";
static bool code_lookup=true,table_lookup=true,module_path_ok=true,system_path_ok=true,query_ok=true;
static DWORD module_length_override,system_length_override,memory_protection=PAGE_EXECUTE_READ,memory_state=MEM_COMMIT;
static void *fake_table[6]={};
static HRESULT STDMETHODCALLTYPE fake_create(ID3D11Device *,const D3D11_TEXTURE2D_DESC *,const D3D11_SUBRESOURCE_DATA *,ID3D11Texture2D **) {return E_NOTIMPL;}
static HMODULE WINAPI mock_named_lookup(LPCWSTR) {++named_lookups;return proxy_module;}
static BOOL WINAPI mock_address_lookup(DWORD flags,LPCWSTR address,HMODULE *out) {
    ++address_calls;
    require(flags==(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT),"address lookup never changes module references");
    if(address==reinterpret_cast<LPCWSTR>(&fake_create)) {*out=code_module;return code_lookup;}
    if(address==reinterpret_cast<LPCWSTR>(fake_table)) {*out=table_module;return table_lookup;}
    require(false,"only actual function/table addresses queried");return FALSE;
}
static DWORD WINAPI mock_module_path(HMODULE module,LPWSTR out,DWORD capacity) {
    ++module_path_calls;require(module==code_module,"path obtained from actual function module");
    if(!module_path_ok)return 0;
    if(module_length_override)return module_length_override;
    require(wcslen(module_path)<capacity,"fixture module path fits");wcscpy_s(out,capacity,module_path);return static_cast<DWORD>(wcslen(module_path));
}
static UINT WINAPI mock_system_path(LPWSTR out,UINT capacity) {
    ++system_path_calls;
    if(!system_path_ok)return 0;
    if(system_length_override)return system_length_override;
    require(wcslen(system_directory)<capacity,"fixture system directory fits");wcscpy_s(out,capacity,system_directory);return static_cast<UINT>(wcslen(system_directory));
}
static SIZE_T WINAPI mock_virtual_query(LPCVOID address,PMEMORY_BASIC_INFORMATION out,SIZE_T size) {
    require(address==reinterpret_cast<LPCVOID>(&fake_create) && size==sizeof(*out),"executable query uses actual function address");
    if(!query_ok)return 0;
    *out={};out->State=memory_state;out->Protect=memory_protection;return sizeof(*out);
}
#define GetModuleHandleW mock_named_lookup
#define GetModuleHandleExW mock_address_lookup
#define GetModuleFileNameW mock_module_path
#define GetSystemDirectoryW mock_system_path
#define VirtualQuery mock_virtual_query
#include "../src/native_terrain_render.inl"
#undef VirtualQuery
#undef GetSystemDirectoryW
#undef GetModuleFileNameW
#undef GetModuleHandleExW
#undef GetModuleHandleW
int main() {
    auto check_site=[] {return terrain_render::original_site(fake_table,&fake_create);};
    require(proxy_module!=code_module && check_site(),"game-folder proxy present, actual System32 code and vtable accepted");
    require(named_lookups==0 && address_calls==2 && module_path_calls==1 && system_path_calls==1,"basename lookup is never used");
    module_path=L"c:\\WINDOWS\\system32\\D3D11.DLL";require(check_site(),"system module path comparison ignores case");
    module_path=L"C:\\Windows\\System32\\d3d11.dll";
    table_module=proxy_module;require(!check_site(),"code and vtable in different modules rejected");table_module=code_module;
    for(const wchar_t *path:{L"C:\\Games\\SAN14\\D3D11.dll",L"C:\\Windows\\System32\\foreign_hook.dll",L"C:\\Windows\\System32\\d3d11.dll.proxy",L"C:\\Windows\\SysWOW64\\d3d11.dll"}) {
        module_path=path;require(!check_site(),"same-module proxy or external code outside exact system DLL path rejected");
    }
    module_path=L"C:\\Windows\\System32\\d3d11.dll";
    system_directory=L"D:\\Windows\\System32";require(!check_site(),"system directory comes from Windows API rather than hardcoded C drive");
    module_path=L"D:\\Windows\\System32\\d3d11.dll";require(check_site(),"actual system directory on another drive accepted");
    system_directory=L"C:\\Windows\\System32";module_path=L"C:\\Windows\\System32\\d3d11.dll";
    code_lookup=false;require(!check_site(),"missing code module rejected");code_lookup=true;
    table_lookup=false;require(!check_site(),"missing table module rejected");table_lookup=true;
    module_path_ok=false;require(!check_site(),"module path query failure rejected");module_path_ok=true;
    system_path_ok=false;require(!check_site(),"system directory query failure rejected");system_path_ok=true;
    module_length_override=MAX_PATH;require(!check_site(),"truncated module path rejected");module_length_override=0;
    for(UINT length:{static_cast<UINT>(MAX_PATH),static_cast<UINT>(MAX_PATH-5),0xffffffffu}) {
        system_length_override=length;require(!check_site(),"oversized system path rejected before secure string append");
    }
    system_length_override=0;
    query_ok=false;require(!check_site(),"unqueryable code rejected");query_ok=true;
    for(DWORD protection:{PAGE_READONLY,PAGE_EXECUTE_READ|PAGE_GUARD,PAGE_EXECUTE_READ|PAGE_NOACCESS}) {
        memory_protection=protection;require(!check_site(),"nonexecutable or inaccessible code rejected");
    }
    memory_protection=PAGE_EXECUTE_READ;memory_state=MEM_RESERVE;require(!check_site(),"uncommitted code rejected");memory_state=MEM_COMMIT;
    require(!terrain_render::original_site(fake_table,nullptr),"null original rejected");
    require(check_site() && named_lookups==0,"valid system module remains accepted without any basename lookup");
    printf("{\"terrain_render_site_checks\":%u,\"failed\":0,\"proxy_forwarding_allowed\":true,\"foreign_module_rejected\":true}\n",checks);
    return 0;
}
