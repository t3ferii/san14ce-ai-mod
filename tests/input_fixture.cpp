#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <stdio.h>
int main() {
    HMODULE loader=GetModuleHandleW(L"dinput8.dll");
    if (!loader || !GetProcAddress(loader,"AIInitialize")) return 1;
    IDirectInput8W *input=nullptr;
    HRESULT result=DirectInput8Create(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,reinterpret_cast<void **>(&input),nullptr);
    if (FAILED(result) || !input) return 2;
    input->Release(); Sleep(500);
    puts("Input loader automatically loaded; original DirectInput factory succeeded; host version guard exercised.");
    return 0;
}
