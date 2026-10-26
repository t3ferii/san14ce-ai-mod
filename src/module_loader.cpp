// Input loader only. All game guards, hooks and features live in the two modules;
// the loader only decides which modules to load.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "split_module_api.h"
static HINSTANCE loader_module;
static HMODULE load_module(const wchar_t *name) {
    wchar_t path[MAX_PATH];
    DWORD length=GetModuleFileNameW(loader_module,path,MAX_PATH);
    if(!length || length>=MAX_PATH)return nullptr;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return nullptr;
    if(wcscpy_s(slash+1,MAX_PATH-static_cast<size_t>(slash+1-path),name))return nullptr;
    return LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
}
// AI_TacticsMod\기능설정.ini [groups]: combat=0 never loads the combat module and
// hands the coordinator a hookless API. The convenience module is the guard/publish
// coordinator and also owns the performance hooks, so it loads unless all three
// groups are 0 (with convenience=0 it publishes none of its own feature hooks).
// Missing file or key = 1.
static bool group_enabled(const wchar_t *key) {
    wchar_t path[MAX_PATH];
    DWORD length=GetModuleFileNameW(loader_module,path,MAX_PATH);
    if(!length || length>=MAX_PATH)return true;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return true;
    if(wcscpy_s(slash+1,MAX_PATH-static_cast<size_t>(slash+1-path),L"AI_TacticsMod\\기능설정.ini"))return true;
    return GetPrivateProfileIntW(L"groups",key,1,path)!=0;
}
static bool __cdecl null_prepare(unsigned char *) {return true;}
static bool __cdecl null_publish() {return true;}
static void __cdecl null_call() {}
static void __cdecl null_scope(int) {}
static const SplitCombatApi null_combat={sizeof(SplitCombatApi),split_module_abi,null_prepare,null_publish,null_call,null_call,null_scope};
// AI-CONTRACT [lifecycle]: This worker negotiates the split ABI and publishes validated hooks only. Do not add gameplay getters/orders here. Module handles stay resident because callbacks can remain in flight.
static DWORD WINAPI initialize_modules(LPVOID) {
    const bool use_combat=group_enabled(L"combat");
    if(!use_combat && !group_enabled(L"convenience") && !group_enabled(L"performance"))return 0;
    HMODULE combat=use_combat?load_module(L"SAN14_CombatAI.dll"):nullptr;
    HMODULE convenience=load_module(L"SAN14_Convenience.dll");
    if((use_combat && !combat) || !convenience)return 0;
    auto get_api=combat?reinterpret_cast<GetSplitCombatApi>(GetProcAddress(combat,"SAN14CombatGetApi")):nullptr;
    auto start=reinterpret_cast<InitializeSplitConvenience>(GetProcAddress(convenience,"SAN14ConvenienceInitialize"));
    const SplitCombatApi *api=use_combat?(get_api?get_api(split_module_abi):nullptr):&null_combat;
    if(!start || !api || api->size!=sizeof(SplitCombatApi) || api->abi!=split_module_abi ||
       !api->prepare || !api->publish || !api->rollback || !api->activate || !api->departure_scope)return 0;
    start(split_module_abi,api);
    // Modules remain loaded for process lifetime; callbacks can be in flight.
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        loader_module=module;
        HANDLE worker=CreateThread(nullptr,0,initialize_modules,nullptr,0,nullptr);
        if(worker)CloseHandle(worker);
    }
    return TRUE;
}
