/* Native import loader: no EXE changes. Existing input providers can be chained
 * through a separately backed-up file. Never load anything from _files. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
using InputCreate = HRESULT(WINAPI *)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN);
static INIT_ONCE input_once = INIT_ONCE_STATIC_INIT;
static InputCreate input_create;
static BOOL CALLBACK load_input(PINIT_ONCE,PVOID,PVOID *)
{
    wchar_t path[MAX_PATH];
    DWORD length=GetModuleFileNameW(nullptr,path,MAX_PATH);
    if (!length || length>=MAX_PATH) return FALSE;
    wchar_t *end=wcsrchr(path,L'\\');
    if (!end) return FALSE;
    wcscpy_s(end+1,MAX_PATH-static_cast<size_t>(end+1-path),L"SAN14_AI_PreviousInput.dll");
    if (GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES) {
        length=GetSystemDirectoryW(path,MAX_PATH);
        if (!length || length+13>=MAX_PATH) return FALSE;
        wcscat_s(path,L"\\dinput8.dll");
    }
    HMODULE provider=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!provider) return FALSE;
    input_create=reinterpret_cast<InputCreate>(GetProcAddress(provider,"DirectInput8Create"));
    return input_create!=nullptr;
}
#pragma comment(linker,"/EXPORT:DirectInput8Create=proxy_DirectInput8Create")
extern "C" HRESULT WINAPI proxy_DirectInput8Create(HINSTANCE instance,DWORD version,REFIID iid,LPVOID *out,LPUNKNOWN outer)
{
    if (!InitOnceExecuteOnce(&input_once,load_input,nullptr,nullptr)) return E_FAIL;
    return input_create(instance,version,iid,out,outer);
}
