#pragma once
// local135 (user 2026-11-14 "이동력은 눈 결빙 타일만 눈 -1 결빙-3 (부대의 기동 ... 고정수치로)"): the season rules' fixed mobility change.
// SAN14_ModPack.dll answers per cell (river ice -3, lying snow -1, else 0; ModPackApi::season_mobility). SAN14_Convenience.dll loads that
// module and hands the entry to SAN14_CombatAI.dll, which owns the native mobility getter 2B2550 hook (native_sortie.inl) and adds
// the change there. nullptr = off (the original mobility).
using SeasonMobility=int (__cdecl *)(int cell);
using GiveSeasonMobility=bool (__cdecl *)(unsigned version,SeasonMobility source);
static const unsigned kSeasonMobilityVersion=1;
