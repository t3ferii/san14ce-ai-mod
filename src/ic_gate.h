#pragma once
// Integrated Convenience gate (2026-11-18, integrated patch R43.42 compatibility).
// SAN14_IntegratedConvenience.dll (IP features 1-7) compares 2,346 original function bodies once at start (up to about
// three minutes) and turns 1-7 off when any of them differs. A few of our owners patch inside those ranges: the combat
// sortie/route CALLs (184FF9, 26AB48, 303FC9, 44E50D), path_fast/path_count (184D60, 1857B5), ccz_bands (four entries)
// and the advisor text setter (9332B0). With IC installed they publish only after IC reports the check done; every
// other owner installs at the normal time (ours must stay first at 3FA342, where IP MeritTraits chains onto us).
// Detection and wait follow the integrated patch's own ic_after_restore.h:
//   installed = <game>\SAN14_IntegratedConvenience.dll exists and SAN14Mods\modules\IntegratedConvenience_GreenlandCompat.ini
//               exists with [Mod] Enabled (default 1) != 0;
//   done      = export SAN14_WaitConvenienceV1 (unsigned (*)(), nonzero once the check finished; one call may block ~15 s);
//   module not loaded within 60 s, no export, or still checking after 10 min -> publish anyway (previous behaviour).
// Header-only C++17 inline state: shared by native_adapter.cpp and tu_convenience_ui.cpp (SAN14_MULTI_TU).
#include <windows.h>
#include <wchar.h>
namespace ic_gate {
inline volatile LONG late=0;   // 1 from install() until the late worker has published the deferred owners
inline bool pending() {return InterlockedCompareExchange(&late,0,0)!=0;}
inline bool game_path(wchar_t *path,const wchar_t *relative) {
    DWORD n=GetModuleFileNameW(nullptr,path,MAX_PATH);
    if(!n || n>=MAX_PATH)return false;
    wchar_t *slash=wcsrchr(path,L'\\');
    if(!slash)return false;
    slash[1]=0;
    return wcscat_s(path,MAX_PATH,relative)==0;
}
inline bool installed() {
    wchar_t dll[MAX_PATH],ini[MAX_PATH];
    if(!game_path(dll,L"SAN14_IntegratedConvenience.dll") || GetFileAttributesW(dll)==INVALID_FILE_ATTRIBUTES)return false;
    if(!game_path(ini,L"SAN14Mods\\modules\\IntegratedConvenience_GreenlandCompat.ini") ||
       GetFileAttributesW(ini)==INVALID_FILE_ATTRIBUTES)return false;
    return GetPrivateProfileIntW(L"Mod",L"Enabled",1,ini)!=0;
}
// Outcome codes for the ic_gate_ready log line.
enum : unsigned {kReady=1,kNoExport=2,kNoModule=3,kTimeout=4};
inline unsigned wait(unsigned &elapsed_ms) {
    const ULONGLONG start=GetTickCount64();
    auto since=[start]{return static_cast<unsigned>(GetTickCount64()-start);};
    HMODULE ic=nullptr;
    while(!(ic=GetModuleHandleW(L"SAN14_IntegratedConvenience.dll"))) {
        if(since()>=60000u) {elapsed_ms=since();return kNoModule;}
        Sleep(200);
    }
    using Done=unsigned (*)();
    Done done=reinterpret_cast<Done>(reinterpret_cast<void *>(GetProcAddress(ic,"SAN14_WaitConvenienceV1")));
    if(!done) {elapsed_ms=since();return kNoExport;}
    while(!done()) {
        if(since()>=600000u) {elapsed_ms=since();return kTimeout;}
        Sleep(500);
    }
    elapsed_ms=since();return kReady;
}
}
