/* Native import loader: no EXE changes. Existing input providers can be chained
 * through a separately backed-up file. Never load anything from _files. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
using InputCreate = HRESULT(WINAPI *)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN);
static INIT_ONCE input_once = INIT_ONCE_STATIC_INIT;
static InputCreate input_create;
static HMODULE own_module() {
    HMODULE self=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&own_module),&self);
    return self;
}
static InputCreate system_input() {
    wchar_t path[MAX_PATH];
    DWORD length=GetSystemDirectoryW(path,MAX_PATH);
    if (!length || length+13>=MAX_PATH) return nullptr;
    wcscat_s(path,L"\\dinput8.dll");
    HMODULE provider=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    return provider?reinterpret_cast<InputCreate>(GetProcAddress(provider,"DirectInput8Create")):nullptr;
}
static BOOL CALLBACK load_input(PINIT_ONCE,PVOID,PVOID *)
{
    wchar_t path[MAX_PATH];
    DWORD length=GetModuleFileNameW(nullptr,path,MAX_PATH);
    if (!length || length>=MAX_PATH) return FALSE;
    wchar_t *end=wcsrchr(path,L'\\');
    if (!end) return FALSE;
    wcscpy_s(end+1,MAX_PATH-static_cast<size_t>(end+1-path),L"SAN14_AI_PreviousInput.dll");
    if (GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES) {
        HMODULE provider=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        // User exception 2026-11-05 (Layout stage 1): never pick this module itself as the previous provider
        // (a renamed copy sitting in the Previous slot); fall through to the system provider instead.
        if (provider && provider!=own_module())
            input_create=reinterpret_cast<InputCreate>(GetProcAddress(provider,"DirectInput8Create"));
        else if (!provider) return FALSE;
    }
    if (!input_create) input_create=system_input();
    return input_create!=nullptr;
}
#pragma comment(linker,"/EXPORT:DirectInput8Create=proxy_DirectInput8Create")
extern "C" HRESULT WINAPI proxy_DirectInput8Create(HINSTANCE instance,DWORD version,REFIID iid,LPVOID *out,LPUNKNOWN outer)
{
    // A chain member that calls back into this loader (another mod's loader renamed into our Previous slot
    // and pointing at us) must leave through the system provider instead of recursing.
    static thread_local bool inside=false;
    if (inside) {
        InputCreate system=system_input();
        return system?system(instance,version,iid,out,outer):E_FAIL;
    }
    if (!InitOnceExecuteOnce(&input_once,load_input,nullptr,nullptr)) return E_FAIL;
    inside=true;
    HRESULT result=input_create(instance,version,iid,out,outer);
    inside=false;
    return result;
}
