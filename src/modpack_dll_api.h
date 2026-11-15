#pragma once
// SAN14_ModPack.dll (user 2026-11-13: modder patches from other authors live in their own DLL; first: 4계절 패치 (wprkfdid12-ux)
// season rules). SAN14_Convenience.dll loads it only when a season rule is on, owns the hook publication (the time-step CALL) and the
// Present-side picture, and forwards to these entries. The module calls game functions only inside those game-thread callbacks.
static const unsigned modpack_dll_abi=3;   // 2: season_flood (local132), 3: season_mobility (local135)
using ModPackLog=void (__cdecl *)(const char *,unsigned,unsigned,unsigned);
enum : unsigned {kSeasonDisasters=1,kSeasonNews=2,kSeasonDamage=8,kSeasonFire=16,kSeasonDomestic=32,kSeasonFloodTerrain=64};
struct ModPackApi {
    unsigned size,abi;
    // folder: ...\AI_TacticsMod\data\season\ (SeasonRules.dat, SeasonRules.log). Returns 0 failed, 1 tables only, 2 living climate.
    int (__cdecl *season_start)(unsigned char *image,const wchar_t *folder,unsigned flags,ModPackLog log);
    void (__cdecl *season_flags)(unsigned flags);
    void (__cdecl *season_day)();                                  // game thread, right after the original time step
    unsigned (__cdecl *season_params)(float *out,int count);       // IniParams list copy; returns its change count
    float (__cdecl *season_speed)(int cell);                       // any thread: the day's tables
    float (__cdecl *season_route)(int cell);
    int (__cdecl *season_cell)(int cell);                          // 1 river ice, 2 landslide block
    // ground tile of a disaster: 0 none, 1 burning, 2 landslide, 3 flood, 4 burnt, 5 shaken, 6 mud, 7 drought crack (game thread)
    int (__cdecl *season_paint)(int cell);
    // local132: terrain of a flooded cell while the water stands: 0 its own, 12 여울, 13 강 (game thread, after season_day)
    int (__cdecl *season_flood)(int cell);
    // local135: fixed mobility change on a cell: -3 river ice, -1 lying snow, 0 none (any thread: the day's tables)
    int (__cdecl *season_mobility)(int cell);
};
using GetModPackApi=const ModPackApi *(__cdecl *)(unsigned);
