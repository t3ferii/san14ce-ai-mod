#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <d3d11.h>
int main() {
    HMODULE mod = GetModuleHandleW(L"AI14.dll");
    if (!mod || !GetProcAddress(mod,"AIInitialize")) return 1;
    auto create = reinterpret_cast<decltype(&D3D11CreateDevice)>(GetProcAddress(mod,"D3D11CreateDevice"));
    if (!create) return 2;
    ID3D11Device *device = nullptr;
    HRESULT result = D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr);
    if (FAILED(result) || !device || !GetModuleHandleW(L"D3D11.dll")) return 3;
    device->Release();
    Sleep(500);
    puts("Explicit PE import loaded the tactics DLL; unsupported-host guard exercised.");
    return 0;
}
