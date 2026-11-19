// Terrain engine service (interop ST, user 2026-11-16 "UI 진입점과 기능, 공용 엔진을 나눠서 의존"): the change/draw engine
// (terrain_render, terrain_foam, terrain_trees, water_diag, the collision-height link) is shared by its users and is on while ANY
// user is on. Users: 부동산 개발 real_estate.terrain_edit (its UI - the advisor-report button, the hex menu slots kPointerTerrain and
// the .terrain sidecar - still follow terrain_edit alone) and the 4계절 disaster tiles season.season_tiles (flood / ice cells through
// terrain_edit::flood_sync). The adapter defines SAN14_TERRAIN_CORE_USERS with every user; a fixture that does not keeps the old
// single-user meaning (terrain_edit), so its mock flag struct needs no new member.
#pragma once
#ifndef SAN14_TERRAIN_CORE_USERS
#define SAN14_TERRAIN_CORE_USERS (mod_features.terrain_edit)
#endif
#define terrain_core_on() (SAN14_TERRAIN_CORE_USERS)
