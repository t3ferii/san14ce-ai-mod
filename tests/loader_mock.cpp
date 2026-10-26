#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../src/split_module_api.h"
#ifdef MOCK_COMBAT
static bool __cdecl prepare(unsigned char *) {return true;}
static bool __cdecl publish() {return true;}
static void __cdecl noop() {}
static void __cdecl scope(int) {}
extern "C" __declspec(dllexport) const SplitCombatApi *__cdecl SAN14CombatGetApi(unsigned abi) {
    static const SplitCombatApi api={sizeof(SplitCombatApi),split_module_abi,prepare,publish,noop,noop,scope};
#ifdef MOCK_BAD_ABI
    (void)abi;return nullptr;
#else
    return abi==split_module_abi?&api:nullptr;
#endif
}
#else
static volatile LONG calls;
extern "C" __declspec(dllexport) bool __cdecl SAN14ConvenienceInitialize(unsigned abi,const SplitCombatApi *api) {
    if(abi!=split_module_abi || !api || api->abi!=abi)return false;
    InterlockedIncrement(&calls);return true;
}
extern "C" __declspec(dllexport) unsigned __cdecl SAN14TestCalls() {return static_cast<unsigned>(calls);}
#endif
BOOL WINAPI DllMain(HINSTANCE,DWORD,LPVOID) {return TRUE;}
