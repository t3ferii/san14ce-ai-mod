/* Pass through the game's sole D3D11 import. Load the original provider by
 * absolute path so a pre-existing game-directory D3D11.dll remains in use. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
static INIT_ONCE graphics_once = INIT_ONCE_STATIC_INIT;
static decltype(&D3D11CreateDevice) graphics_create;
static BOOL CALLBACK load_graphics(PINIT_ONCE, PVOID, PVOID *)
{
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(nullptr,path,MAX_PATH)) return FALSE;
    wchar_t *end = wcsrchr(path,L'\\');
    if (!end) return FALSE;
    wcscpy_s(end+1,MAX_PATH-static_cast<size_t>(end+1-path),L"D3D11.dll");
    HMODULE provider = nullptr;
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) {
        provider = LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    } else {
        provider = LoadLibraryExW(L"D3D11.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    if (!provider) return FALSE;
    graphics_create = reinterpret_cast<decltype(graphics_create)>(GetProcAddress(provider,"D3D11CreateDevice"));
    return graphics_create != nullptr;
}
#pragma comment(linker, "/EXPORT:D3D11CreateDevice=proxy_D3D11CreateDevice")
extern "C" HRESULT WINAPI proxy_D3D11CreateDevice(
    IDXGIAdapter *adapter,D3D_DRIVER_TYPE driver,HMODULE software,UINT flags,
    const D3D_FEATURE_LEVEL *levels,UINT count,UINT sdk,
    ID3D11Device **device,D3D_FEATURE_LEVEL *level,ID3D11DeviceContext **context)
{
    if (!InitOnceExecuteOnce(&graphics_once,load_graphics,nullptr,nullptr)) return E_FAIL;
    return graphics_create(adapter,driver,software,flags,levels,count,sdk,device,level,context);
}
