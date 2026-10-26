#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned checks;
static void check(bool value,const char *label) {++checks;if(!value){fprintf(stderr,"%s\n",label);exit(1);}}
int main(int argc,char **argv) {
    check(argc==3,"loader path and expected initializer calls provided");
    wchar_t path[MAX_PATH];
    check(MultiByteToWideChar(CP_UTF8,0,argv[1],-1,path,MAX_PATH)>0,"unicode loader path");
    HMODULE loader=LoadLibraryW(path);check(loader!=nullptr,"loader loads");
    check(!GetProcAddress(loader,"AIInitialize") && !GetProcAddress(loader,"SAN14CombatGetApi") &&
          !GetProcAddress(loader,"SAN14ConvenienceInitialize"),"loader contains no game-module entry points");
    using Factory=HRESULT(WINAPI *)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN);
    auto factory=reinterpret_cast<Factory>(GetProcAddress(loader,"DirectInput8Create"));
    check(factory!=nullptr,"input forwarding export");
    IDirectInput8W *input=nullptr;
    check(SUCCEEDED(factory(GetModuleHandleW(nullptr),DIRECTINPUT_VERSION,IID_IDirectInput8W,reinterpret_cast<void **>(&input),nullptr)) && input,
          "original DirectInput factory succeeds independently of module availability");
    input->Release();
    Sleep(1000);
    HMODULE convenience=GetModuleHandleW(L"SAN14_Convenience.dll");
    using Calls=unsigned (__cdecl *)();
    auto calls=convenience?reinterpret_cast<Calls>(GetProcAddress(convenience,"SAN14TestCalls")):nullptr;
    unsigned count=calls?calls():0;
    check(count==static_cast<unsigned>(atoi(argv[2])),"only a complete ABI-compatible module pair initializes, exactly once");
    printf("{\"status\":\"loader_fixture_passed\",\"checks\":%u}\n",checks);
    return 0;
}
