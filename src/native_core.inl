#pragma once
// Shared core of every native_adapter.cpp translation unit (2026-11-16 refactor, docs/REFACTOR_EQUIVALENCE.md section 4):
// settings, log, address helpers and the state every module reads. Namespace-scope definitions are `inline` (C++17), so a
// split build (SAN14_MULTI_TU) shares one copy of each across its translation units; the unity build is unchanged in meaning.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <initializer_list>
#include "game_addresses.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>
#include <intrin.h>
#include "native_profile.h"
#include "native_cost_profile.h"
#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG)
#include <new>
namespace costprofile {
inline bool call_trace_enabled=false;
struct HotScope {
    alignas(Scope) unsigned char storage[sizeof(Scope)];
    Scope *scope;
    explicit HotScope(const Key *key):scope(key ? new(storage) Scope(*key) : nullptr) {}
    ~HotScope() {if(scope)scope->~Scope();}
};
}
#define COST_SCOPE_HOT_INDEX(name,n) costprofile::HotScope COST_JOIN(hot_scope_,n)(costprofile::call_trace_enabled ? []()->const costprofile::Key * {static const costprofile::Key key(name);return &key;}() : nullptr)
#define COST_SCOPE_HOT(name) COST_SCOPE_HOT_INDEX(name,__COUNTER__)
#define COST_CRUMB_HOT(name,value) do {if(costprofile::call_trace_enabled)COST_CRUMB(name,value);} while(0)
#else
#define COST_SCOPE_HOT(name) do {} while(0)
#define COST_CRUMB_HOT(name,value) do {} while(0)
#endif
#include <vector>
#include <queue>
#include <algorithm>
#if defined(SPLIT_BASE) || defined(SPLIT_COMBAT)
#include "split_module_api.h"
#endif
#ifdef SPLIT_BASE
inline const SplitCombatApi *split_combat_api;
#endif

/* All game callbacks run on the game's own AI thread, synchronously from its
 * existing nodes. The loader worker only installs validated code/vtable links;
 * it never enumerates units or issues orders. Expert regional settings share
 * native unit stat/preview paths; actual AI pairs retain their fixed rate. */
// AI-CONTRACT [ABI]: Obj is a borrowed native object, never an owning buffer. Every offset/RVA is build-specific. Preserve __fastcall signatures and validate caller + original bytes before changing bindings.
using Obj = unsigned char *;
using Execute = uint64_t(__fastcall *)(Obj, Obj, Obj);
inline Obj image;
inline HMODULE self_module;
inline volatile LONG enabled;
// 2026-11-20 의용병 무리: SAN14_CCZ.dll's bands->power (morale/kind factor on 2AAF30), set by native_ccz_bands.inl after the band
// links publish, read by the battle_rules 2AAF30 hook (native_adjacency.inl, another translation unit). Null = no bands = original.
inline float (__cdecl *volatile band_power_fn)(Obj army) = nullptr;
#ifndef NO_RUNTIME_LOG
inline SRWLOCK log_lock = SRWLOCK_INIT;
#endif
// Native path searches reuse a node buffer. A callback must not start another
// mod probe while an original search is still consuming that buffer.
inline __declspec(thread) unsigned native_search_depth;
// Settings (2026-10-22): every switch lives in AI_TacticsMod\기능설정.ini, managed by the installer
// (기타(개발용) tab for diagnostics). A key missing there falls back to its former file, then default.
// local99: the scenery skip is one topic of the advisor briefing now ("명승지 발견"); the briefing keeps this in step with
// its in-game setting and the scenery state hooks (native_modpack.inl) read it.
inline volatile LONG scenery_collect_live=1;
inline bool mod_ini_path(wchar_t *out,const wchar_t *file)
{
    if(!GetModuleFileNameW(self_module,out,MAX_PATH))return false;
    wchar_t *slash=wcsrchr(out,L'\\');if(!slash)return false;*slash=0;
    return !wcscat_s(out,MAX_PATH,L"\\AI_TacticsMod\\") && !wcscat_s(out,MAX_PATH,file);
}
// AI-CONTRACT [settings]: Maintain the unified INI lookup and legacy fallback. A new exposed key must be added to InstallerGui.cs and distribution/FeatureSelect.ps1 in the same patch; keep parent && child gating.
inline int mod_setting(const wchar_t *section,const wchar_t *key,int fallback,const wchar_t *legacy=nullptr)
{
    constexpr UINT kAbsent=0x7FFFFFF0u;wchar_t path[MAX_PATH];
    if(mod_ini_path(path,L"기능설정.ini")) {UINT v=GetPrivateProfileIntW(section,key,kAbsent,path);if(v!=kAbsent)return static_cast<int>(v);}
    if(legacy && mod_ini_path(path,legacy))return static_cast<int>(GetPrivateProfileIntW(section,key,static_cast<UINT>(fallback),path));
    return fallback;
}
// [groups] diagnostics: master of the 기타(개발용) tab; absent = 1 so older settings keep working.
inline bool dev_group() {return mod_setting(L"groups",L"diagnostics",1)!=0;}
#ifndef NO_RUNTIME_LOG
inline wchar_t log_path[MAX_PATH];
#include "log_append.h"
inline diagnostics::AppendFile log_append;
inline volatile LONG reuse_log_handle=1;
inline volatile LONG diagnostic_logging;
[[maybe_unused]] inline void load_diagnostic_settings(const wchar_t *config) {
    InterlockedExchange(&diagnostic_logging,GetPrivateProfileIntW(L"diagnostics",L"enabled",0,config)!=0?1:0);
}
inline void initialize_log_settings() {
    InterlockedExchange(&diagnostic_logging,dev_group() && mod_setting(L"diagnostics",L"enabled",0,L"diagnostics.ini")!=0?1:0);
#ifdef RUNTIME_COST_PROFILE
    costprofile::set_crumbs_enabled(diagnostic_logging!=0);
    costprofile::call_trace_enabled=dev_group() && mod_setting(L"diagnostics",L"call_trace",0,L"diagnostics.ini")!=0;
#endif
    InterlockedExchange(&reuse_log_handle,mod_setting(L"diagnostics",L"reuse_handle",1,L"diagnostics.ini")!=0?1:0);
}
#else
inline constexpr LONG diagnostic_logging=0,reuse_log_handle=0;
inline void initialize_log_settings() {}
template<class... T> inline void runtime_log_discard(T...) {}
#endif
#include "native_crash_diagnostic.inl"
#ifdef INJURY_PROTECTION
static void injury_synchronize_city(Obj city);
static bool injury_auto_district(Obj district);
#endif

#ifdef TACTICS_FIXTURE
static void *fixture_function(unsigned rva);
template<class T> inline T fn(unsigned rva) { return reinterpret_cast<T>(fixture_function(rva)); }
#else
template<class T> inline T fn(unsigned rva) {
#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG)
    if(costprofile::call_trace_enabled)costprofile::crumb("game_function",costprofile::Call,rva);
#endif
    return rva ? reinterpret_cast<T>(image + rva) : nullptr;   // 0 = unresolved address table id (docs/PATCH_RESILIENCE.md): never call image+0
}
#endif
// Layout hook manager chains (design 8.0 G1): `next` is the function the manager hands a replacement; null (no manager:
// split fixtures, combat module) means the original game function.
template<class T> inline T next_or(void *next, unsigned rva) { return next ? reinterpret_cast<T>(next) : fn<T>(rva); }
// Same for a typed next pointer that the manager fills in (null in fixtures that call a replacement body directly).
template<class T> inline T next_pick(T next, unsigned rva) { return next ? next : fn<T>(rva); }
inline unsigned short word(Obj p, unsigned off) { return *reinterpret_cast<unsigned short *>(p + off); }
inline unsigned dword(Obj p, unsigned off) { return *reinterpret_cast<unsigned *>(p + off); }
inline Obj pointer(Obj p, unsigned off) { return *reinterpret_cast<Obj *>(p + off); }
inline Obj world() { return pointer(image, GA(GA_2780F60)); }
// User-facing feature switches (AI_TacticsMod\기능설정.ini). Read once before
// hooks publish; restart applies. Each key is read from its section first, then
// from the legacy [features] key of the same name, then the default.
// [groups] combat (loader only), convenience, performance gate their sections.
// Coupled code shares one key: battle_rules = combat pair scope + adjacency +
// expert attack/defense settings; tooltip_collab = imported tooltip code whose
// UI init also runs the collab first unlock. The domestic tick hook carries
// domestic, awards (AI distribution) and injury (herbal sync) and is published
// when any of them is on. Merit/warehouse save links stay published always.
// AI-CONTRACT [variants]: This shared core is compiled into every translation unit of both roots (SPLIT_BASE/SPLIT_COMBAT; src/tu_<root>_<unit>.cpp in SAN14_MULTI_TU builds). Follow native_split_install.inl to prove reachability and owner; a declaration or fixture alone does not establish runtime activation.
struct ModFeatures {
    bool convenience = true, performance = true;
    bool merit = true, awards = true, recruit = true, injury = true;          // [personnel]
    bool merit_friendship = true;                                            // [personnel] 2026-11-05: stage-3 mutual friendship on top of the native effect
    unsigned siege_guard_range = 2;                                          // [combat] 1..4 = siege guard intercept radius 6/8/10/12 (default 8, user 2026-11-15)
unsigned merit_troops_level = 4;                                       // [personnel] 1..4 = stage 1/2 troops 1500/2000/2500/3000 (default 3000)
    bool recruit_ai = true, awards_ai = true, injury_herbal = true;          // [personnel] v12.8 split
    bool domestic = true, population = true, region_officers = true;         // [domestic]
    unsigned population_rate = 4;                                           // [domestic] 1..4 (user 2026-11-03), 1 = original
    bool domestic_free = true, region_officer_traits = true;                 // [domestic] v12.8 split (dependent)
    bool domestic_direct = false;
    bool ai_trace = false, trace_original = false;                          // [diagnostics] native AI trace (v12.12)                                            // [domestic] v12.10: ruler's 1st corps too (dependent, default off)
    bool battle_rules = true;                                               // [battle_rules]
    bool loyalty = true;                                                    // [officer_ui]
    bool terrain_edit = false;                                              // [real_estate]
    // 0=foam+renderer, 1=no foam, 2=no renderer/height capture, 3=neither.
    // Diagnostic startup choice only; terrain editing UI/trees/save remain owned by terrain_edit.
    bool season_map = false;                                               // [diagnostics] 4계절 G1a ground bake
    bool season_weather = false, season_disaster = false, season_damage = false, season_news = false, season_fire = false, season_domestic = false, season_rain = false, season_cloud = false, season_tiles = false;   // [diagnostics] 4계절 rules (SAN14_ModPack.dll)
    unsigned season_probe = 0;                                              // [diagnostics] season G0 probe bits
    unsigned terrain_diag_mode = 0;                                         // [diagnostics]
    bool building_destruction_all = false;                                 // [stratagem], gameplay tab
    bool building_destruction_boost = false;                               // [stratagem] local92: a successful roll counts 1.5 times
    bool diplomacy_overinvest = false, diplomacy_counterturn = false;       // [diplomacy], incoming computer negotiations
    bool damage_preview = true;                                            // [officer_ui], convenience only
    bool traits9 = true, tooltip_collab = true, stats200 = true, battle_deaths = true, scenery_skip = true; // [extensions]
    bool stats200_growth = true;                     // [extensions] v12.8 split
    bool speed_buttons = true, occupy_fast = true, fast_save = true;        // [game_flow]
    bool caravan_stay = false, fast_menu = false;                           // [game_flow] 2026-10-23
    bool advisor_briefing = false;                                         // [game_flow], one-cycle adviser inbox
    bool appear_batch = false, area_index = false, path_fast = false, face_keep = false, face_prefetch = false;    // [performance] 2026-10-25
    bool stat_exp = false, dispatch_days = false, mediation_plus = false, drag_learn = false, lord_change = false; // [officer_ui] 2026-10-23
    bool tactic_priority = false;                                           // [officer_ui] 2026-11-06
    bool distance = true, name_lookup = true, layout_keep = true, timer_1ms = false; // [performance]
    bool ai_wall_block = true;                                              // [combat] (combat module)
    bool ai_catapult_block = false;                                         // [combat] (combat module)
    bool district_intercept_block = true;                                   // [combat], gameplay tab
    bool combat_cut = true, hit_response = true, obstacle = true, recall_veto = true; // [combat] v12.8 leaf gates
    bool sortie_formation = true, departures = true, group_advance = true, stationary_retreat = true, morale = true, support = true;
    // [combat] mission_v2 (2026-10-21, default off until verified): siege-ground construction only, no
    // build wait, mod field steering yields to a native order on the mission goal.
    bool mission_v2 = false;
    // [combat] builder_ruler_fill (user 2026-11-01, under mission_v2, default off): a 건설 5 ruler's
    // armies ignore siege-ground limits and fill their regions' facility capacity.
    bool builder_ruler_fill = false;
    // [combat] ai_max_aggression (2026-10-21): behaviour-tree "AI setting stage" reads as 궁극(4)
    // and the corps progress check loses its stage-based turn skipping (see bt_condition).
    bool ai_max_aggression = false;
    // attack_recheck_fix (2026-10-22, under mission_v2): original tree-1 post-departure recheck bug (#10114/#10115).
    bool attack_recheck_fix = false;
    bool ccz_sprites = false, ccz_hide_model = true, ccz_depth = true, ccz_block = false, ccz_critical = false;     // [ccz] 2026-10-31 user: 조조전 병종 그림, gameplay tab
    bool ccz_far_keep = false;                                              // retired, retained as disabled code path
    // local17 (user 2026-11-10): the changes kept from local16, each behind its own key. Every key defaults to 0 so a
    // fresh install equals v15.7.1; the user enables them one by one. Computer-side rules never reach the player force.
    bool siege_ring10 = false, siege_stall_near = false;
    bool siege_fort_first = false;   // [combat] local74: original camp build -> fort, band, camp
    bool retake_build = false;   // [combat] local91: retake units also build after taking a base (off = they never build)
    bool retake_adjacent = false;   // [combat] local80: the original area-occupy filter asks radius 2 for a lost first-ring region
    bool gate_intercept = false;    // [combat] user 2026-11-13: a 2,000-soldier 안행 unit stands behind an attacked computer gate
    bool garrison_recall = false;   // [combat] local148: an attacked castle with fewer soldiers than durability calls its own armies home
    bool tribe_terrain_path = false; // [combat] local149: 장사·사이 computer armies route through forest / mountains with a foe within 15 hexes
    bool computer_trade = false;    // [combat] local167: monthly 상순 trade to the least friendly tribe city, 통솔<50 특사/정치 officers
    bool front_hire = false;        // [combat] local167: frontline cities hire only 응원/악주/조타/모집 trait holders
    bool helm_intercept = false;    // [combat] local148: 조타 officers of six river castles sail out to paint the water around attackers of an own port
    bool ranged_facility_kill = false;   // [combat] user 2026-11-13: facilities hit from range can drop to 0 (2A0750 cap)
    bool siege_no_catapult = false, siege_no_watchtower = false, siege_no_trap = false, ai_policy_cap = false;   // [combat] local54, under mission_v2                    // [combat] under mission_v2
    bool attack_eval70 = false, retreat_eval60 = false;                     // [combat] attack review 70%, field retreat recheck 60%
    bool path_wall = false, siege_guard = false, supply_relay = false, no_tribe_target = false;                          // [combat] path_wall needs obstacle
    bool ship_repair_strike = false;   // [combat] local226: a computer 누선 hits a facility and its repairer together
    bool delegated_exclude = false;                                         // [combat] local18: player delegated corps leave the campaign rules
    bool ai_search2 = true, officer_first = true;                           // [personnel] local56 computer corps (native_corps_domestic.inl)
    bool ai_learn = true;                                                   // [personnel] local150 computer corps learn a tactic instead of the 2nd search
    bool ai_tactic_order = false;   // [combat] local150 computer departures carry tactics in the computer order
    bool officer_rotation = true;                                           // [personnel] local57 computer forces (native_officer_rotation.inl)
    bool awards_horse = false, awards_v16 = false;                          // [personnel] under awards_ai
    bool merit_bond16 = false;                                              // [personnel] under merit
    bool briefing16 = false;                                                // [game_flow] under advisor_briefing
    bool expert_ingame = false;                                             // [battle_rules] under battle_rules
    bool drag_learn_multi = false;                                          // [officer_ui] under drag_learn
    bool auto_search = false, auto_reward = false;                          // [game_flow] main-screen toggles (local17, default off)
    bool sprite_fallback = false;                                           // [ccz] under ccz_sprites
    bool ccz_bands = false;                                                 // [ccz] bands (2026-11-17 종군 중립군; band_dev_spawn retired local248)
    unsigned band_troops = 3;                                               // [ccz] 1..3 = idle band 300->3000 / 400->4000 / 500->5000 (user 2026-11-19)
    unsigned band_morale = 2;                                               // [ccz] 1..3 = band kill/rout morale x0.5 / x1 / x1.5 (user 2026-11-20)
    bool ambush = false;                                                    // [game_flow] under advisor_briefing (ambush tickets)
    bool destruction_ticket = false;                                        // [game_flow] under ambush: sure-success building destruction tickets
};
inline ModFeatures mod_features;
#ifndef SPLIT_COMBAT   // 2026-11-16: the terrain engine and the season rules are convenience-DLL units only (feature_deps D3)
// The terrain engine is a service (src/terrain_core.h): on while 부동산 개발 is on, or while the 4계절 disaster tiles are on AND the
// season rules can run (season_rules::can_run at the first feature guard pass, before any terrain hook; cleared again when the rules
// fail to install). Without the rules (SAN14_ModPack.dll absent, the time-step CALL taken by another season mod) the tiles have no
// painter, so the engine stays exactly as it was with 부동산 개발 off (interop 2026-11-16: off / DLL absent = original).
inline bool season_tiles_runnable;
#define SAN14_TERRAIN_CORE_USERS (mod_features.terrain_edit || (mod_features.season_tiles && season_tiles_runnable))   // src/terrain_core.h
#include "terrain_core.h"
#endif
inline bool feature_key(const wchar_t *config, const wchar_t *section, const wchar_t *key, bool fallback)
{
    const int legacy = static_cast<int>(GetPrivateProfileIntW(L"features", key, fallback ? 1 : 0, config));
    return GetPrivateProfileIntW(section, key, legacy, config) != 0;
}
inline void load_mod_features()
{
    wchar_t config[MAX_PATH];
    if (!GetModuleFileNameW(self_module, config, MAX_PATH)) return;
    wchar_t *slash = wcsrchr(config, L'\\'); if (!slash) return; *slash = 0;
    if (wcscat_s(config, L"\\AI_TacticsMod\\기능설정.ini")) return;
    auto &f = mod_features;
    f.convenience = GetPrivateProfileIntW(L"groups", L"convenience", 1, config) != 0;
    // Analysis: ai_trace=1 observes native behaviour-tree nodes (chained over mod hooks);
    // ai_trace_original=1 additionally publishes no mod hook at all (pure original AI).
    f.ai_trace = dev_group() && GetPrivateProfileIntW(L"diagnostics", L"ai_trace", 0, config) != 0;
    f.trace_original = f.ai_trace && GetPrivateProfileIntW(L"diagnostics", L"ai_trace_original", 0, config) != 0;
    f.performance = GetPrivateProfileIntW(L"groups", L"performance", 1, config) != 0;
    const bool on = f.convenience, fast = f.performance;
    f.merit = on && feature_key(config, L"personnel", L"merit", true);
    f.merit_friendship = f.merit && feature_key(config, L"personnel", L"merit_friendship", true);
    f.merit_troops_level = (std::max)(1u, (std::min)(4u, GetPrivateProfileIntW(L"personnel", L"merit_troops_level", 4, config)));
    f.awards = on && feature_key(config, L"personnel", L"awards", true);
    f.recruit = on && feature_key(config, L"personnel", L"recruit", true);
    f.injury = on && feature_key(config, L"personnel", L"injury", true);
    // v12.8: keys absent from older files inherit the switch they were split from.
    f.recruit_ai = on && feature_key(config, L"personnel", L"recruit_ai", f.recruit);
    f.awards_ai = f.awards && feature_key(config, L"personnel", L"awards_ai", true);   // needs the warehouse
    f.injury_herbal = on && feature_key(config, L"personnel", L"injury_herbal", f.injury);
    f.ai_search2 = on && feature_key(config, L"personnel", L"ai_search2", true);
    f.ai_learn = f.ai_search2 && feature_key(config, L"personnel", L"ai_learn", true);   // local150
    f.officer_first = on && feature_key(config, L"personnel", L"officer_first", true);
    f.officer_rotation = on && feature_key(config, L"personnel", L"officer_rotation", true);
    f.domestic = on && feature_key(config, L"domestic", L"domestic", true);
    f.population = on && feature_key(config, L"domestic", L"population", true);
    f.population_rate = (std::max)(1u, (std::min)(4u, GetPrivateProfileIntW(L"domestic", L"population_rate", 4, config)));
    f.region_officers = on && feature_key(config, L"domestic", L"region_officers", true);
    f.domestic_free = f.domestic && feature_key(config, L"domestic", L"domestic_free", true);
    f.domestic_direct = f.domestic && feature_key(config, L"domestic", L"domestic_direct", false);
    f.region_officer_traits = f.region_officers && feature_key(config, L"domestic", L"region_officer_traits", true);
    f.battle_rules = on && feature_key(config, L"battle_rules", L"battle_rules", true);
    f.loyalty = on && feature_key(config, L"officer_ui", L"loyalty", true);
    const int terrain_diag = dev_group() ? mod_setting(L"diagnostics", L"terrain_diag_mode", 0) : 0;
    f.terrain_diag_mode = terrain_diag >= 0 && terrain_diag <= 3 ? static_cast<unsigned>(terrain_diag) : 0u;
    const int season_probe = dev_group() ? mod_setting(L"diagnostics", L"season_probe", 0) : 0;
    f.season_probe = season_probe > 0 && season_probe <= 15 ? static_cast<unsigned>(season_probe) : 0u;
    // [groups] modpack: master of the 「180(wprkfdid12-ux) 모드팩 호환」 tab (user 2026-11-13). Keys live in [season]; a key still
    // under [diagnostics] (the dev-tab test builds) is the fallback, so an existing setting keeps its value.
    const bool modpack_group = on && GetPrivateProfileIntW(L"groups", L"modpack", 1, config) != 0;
    auto season_old = [&](const wchar_t *key, int fallback) {return GetPrivateProfileIntW(L"diagnostics", key, fallback, config) != 0;};
    f.season_map = modpack_group && feature_key(config, L"season", L"season_map", season_old(L"season_map", 0));   // 4계절 ground bake
    f.season_weather = f.season_map && feature_key(config, L"season", L"season_weather", season_old(L"season_weather", 0));   // rules; the rest need it
    f.season_disaster = f.season_weather && feature_key(config, L"season", L"season_disaster", season_old(L"season_disaster", 1));
    f.season_damage = f.season_weather && feature_key(config, L"season", L"season_damage", season_old(L"season_damage", 1));
    f.season_news = f.season_weather && feature_key(config, L"season", L"season_news", season_old(L"season_news", 1));
    f.season_fire = false;   // local133 (user 2026-11-14 "비표시 빼자, 홍수랑 결빙만", "결빙타일을 만들어서 대하를 바꾸면"): no weather fire rule (the game's own fire stays)
    f.season_domestic = f.season_weather && feature_key(config, L"season", L"season_domestic", season_old(L"season_domestic", 1));   // order / population
    f.season_rain = false;   // local133: no rain layer (the rain picture is off; the rules still keep their weather)
    f.season_cloud = f.season_weather && feature_key(config, L"season", L"season_cloud", true);   // local152 (user 2026-11-14 "구름만 진행"): the cloud body alone over the rain cells, far view
    f.season_tiles = f.season_weather && feature_key(config, L"season", L"season_tiles", season_old(L"season_tiles", 1));   // disaster hexes (terrain painter)
    f.building_destruction_all = on && feature_key(config, L"stratagem", L"building_destruction_all", false);
    f.building_destruction_boost = on && feature_key(config, L"stratagem", L"building_destruction_boost", true);   // local92
    f.diplomacy_overinvest = false; // retired 2026-11-03: native cap/marker restored; key no longer read
    f.diplomacy_counterturn = on && feature_key(config, L"diplomacy", L"diplomacy_counterturn", false);
    f.damage_preview = on && feature_key(config, L"officer_ui", L"damage_preview", true);
    f.traits9 = on && feature_key(config, L"extensions", L"traits9", true);
    f.tooltip_collab = on && feature_key(config, L"extensions", L"tooltip_collab", true);
    f.stats200 = on && feature_key(config, L"extensions", L"stats200", true);
    f.stats200_growth = f.stats200 && feature_key(config, L"extensions", L"stats200_growth", true);
    f.battle_deaths = on && feature_key(config, L"extensions", L"battle_deaths", true);
    f.speed_buttons = on && feature_key(config, L"game_flow", L"speed_buttons", true);
    f.occupy_fast = on && feature_key(config, L"game_flow", L"occupy_fast", true);
    f.fast_save = on && feature_key(config, L"game_flow", L"fast_save", true);
    f.caravan_stay = on && feature_key(config, L"game_flow", L"caravan_stay", false);
    f.auto_search = on && feature_key(config, L"game_flow", L"auto_search", false);
    f.auto_reward = on && feature_key(config, L"game_flow", L"auto_reward", false);
    f.advisor_briefing = on && feature_key(config, L"game_flow", L"advisor_briefing", false);
    f.scenery_skip = f.advisor_briefing;   // local99: no own key; the briefing topic "명승지 발견" decides at run time
    f.terrain_edit = f.advisor_briefing && feature_key(config, L"real_estate", L"terrain_edit", false);
    f.fast_menu = fast && feature_key(config, L"performance", L"fast_menu", false);   // 2026-10-23 user: performance tab
    f.area_index = fast && feature_key(config, L"performance", L"area_index", false);
    f.path_fast = fast && feature_key(config, L"performance", L"path_fast", false);
    f.appear_batch = fast && feature_key(config, L"performance", L"appear_batch", false);   // 2026-10-25 user: 0-day army appearance
    f.face_keep = fast && feature_key(config, L"performance", L"face_keep", false);   // 2026-10-25 user: army flag faces stay loaded
    f.face_prefetch = f.face_keep && feature_key(config, L"performance", L"face_prefetch", false);   // 2026-10-25 user: request faces ahead
    f.stat_exp = on && feature_key(config, L"officer_ui", L"stat_exp", false);
    f.dispatch_days = on && feature_key(config, L"officer_ui", L"dispatch_days", false);
    f.mediation_plus = on && feature_key(config, L"officer_ui", L"mediation_plus", false);
    f.drag_learn = on && feature_key(config, L"officer_ui", L"drag_learn", false);
    f.lord_change = on && feature_key(config, L"officer_ui", L"lord_change", false);
    f.tactic_priority = on && feature_key(config, L"officer_ui", L"tactic_priority", false);
    const bool ccz = GetPrivateProfileIntW(L"groups", L"ccz", 1, config) != 0;
    f.ccz_sprites = on && ccz && feature_key(config, L"ccz", L"ccz_sprites", false);   // draw 조조전 unit sprites
    f.ccz_hide_model = f.ccz_sprites;
    f.ccz_depth = f.ccz_sprites;
    f.ccz_far_keep = false;
    f.ccz_block = on && ccz && feature_key(config, L"ccz", L"ccz_block", false);
    f.ccz_critical = on && ccz && feature_key(config, L"ccz", L"ccz_critical", false);
    f.distance = fast && feature_key(config, L"performance", L"distance", true);
    f.name_lookup = fast && feature_key(config, L"performance", L"name_lookup", true);
    f.layout_keep = fast && feature_key(config, L"performance", L"layout_keep", true);
    f.timer_1ms = false;   // retired 2026-11-03 (measured: no effect on layout loads); code path kept off
    f.ai_wall_block = feature_key(config, L"combat", L"ai_wall_block", true);
    f.ai_catapult_block = feature_key(config, L"combat", L"ai_catapult_block", false);
    f.district_intercept_block = feature_key(config, L"combat", L"district_intercept_block", true);
    f.combat_cut = feature_key(config, L"combat", L"combat_cut", true);
    f.hit_response = feature_key(config, L"combat", L"hit_response", true);
    f.obstacle = feature_key(config, L"combat", L"obstacle", true);
    f.recall_veto = feature_key(config, L"combat", L"recall_veto", true);
    f.sortie_formation = feature_key(config, L"combat", L"sortie_formation", true);
    f.stationary_retreat = feature_key(config, L"combat", L"stationary_retreat", true);
    f.morale = feature_key(config, L"combat", L"morale", true);
    f.mission_v2 = feature_key(config, L"combat", L"mission_v2", false);
    f.builder_ruler_fill = f.mission_v2 && feature_key(config, L"combat", L"builder_ruler_fill", false);
    f.ai_max_aggression = feature_key(config, L"combat", L"ai_max_aggression", false);
    f.attack_recheck_fix = f.mission_v2;   // no own key: part of mission_v2 (installer catalog unchanged)
    // local17 keys (user 2026-11-10), all default 0.
    f.siege_ring10 = f.mission_v2 && feature_key(config, L"combat", L"siege_ring10", false);
    f.siege_no_catapult = f.mission_v2 && feature_key(config, L"combat", L"siege_no_catapult", false);   // local54
    f.siege_no_watchtower = f.mission_v2 && feature_key(config, L"combat", L"siege_no_watchtower", false);   // local54
    f.siege_no_trap = f.mission_v2 && feature_key(config, L"combat", L"siege_no_trap", false);   // local54
    f.ai_policy_cap = f.siege_no_catapult;   // local156/158: the computer's 지세연구 stops at 7..9 when it may not build 투석대
    f.siege_fort_first = feature_key(config, L"combat", L"siege_fort_first", true);   // local74
    f.retake_adjacent = f.mission_v2 && feature_key(config, L"combat", L"retake_adjacent", true);   // local84
    f.retake_build = f.retake_adjacent && feature_key(config, L"combat", L"retake_build", false);   // local91 (user: "기본값은 off ... 매우 하드코어")
    f.gate_intercept = f.mission_v2 && feature_key(config, L"combat", L"gate_intercept", true);   // user 2026-11-13: 안행 2,000 behind an attacked gate
    f.garrison_recall = f.mission_v2 && feature_key(config, L"combat", L"garrison_recall", true);   // local148: soldiers above durability while attacked
    f.computer_trade = feature_key(config, L"combat", L"computer_trade", true);   // local167
    f.front_hire = feature_key(config, L"combat", L"front_hire", true);   // local167
    f.helm_intercept = f.mission_v2 && feature_key(config, L"combat", L"helm_intercept", true);   // local148: 조타 ships paint the water near an attacked port
    f.ranged_facility_kill = feature_key(config, L"combat", L"ranged_facility_kill", true);   // user 2026-11-13
    f.siege_stall_near = f.mission_v2 && feature_key(config, L"combat", L"siege_stall_near", false);
    f.attack_eval70 = feature_key(config, L"combat", L"attack_eval70", false);
    f.retreat_eval60 = feature_key(config, L"combat", L"retreat_eval60", false);
    f.path_wall = f.obstacle && feature_key(config, L"combat", L"path_wall", false);
    f.ship_repair_strike = feature_key(config, L"combat", L"ship_repair_strike", false);   // local226
    f.siege_guard = feature_key(config, L"combat", L"siege_guard", false);
    f.siege_guard_range = (std::max)(1u, (std::min)(4u, GetPrivateProfileIntW(L"combat", L"siege_guard_range", 2, config)));
    f.supply_relay = feature_key(config, L"combat", L"supply_relay", true);
    f.no_tribe_target = feature_key(config, L"combat", L"no_tribe_target", true);
    f.tribe_terrain_path = feature_key(config, L"combat", L"tribe_terrain_path", true);   // local149
    f.ai_tactic_order = feature_key(config, L"combat", L"ai_tactic_order", true);   // local150
    // local94 (user 2026-11-12 "이건 선택사항이 아니잖아 ... 항상 제외로 고정"): no longer an option. A delegated corps of
    // the player force never takes the computer sortie/campaign rules; the key is not read.
    f.delegated_exclude = true;
    f.awards_v16 = f.awards_ai && feature_key(config, L"personnel", L"awards_v16", false);
    f.awards_horse = f.awards_ai && feature_key(config, L"personnel", L"awards_horse", false);
    f.merit_bond16 = f.merit && feature_key(config, L"personnel", L"merit_bond16", false);
    f.briefing16 = f.advisor_briefing && feature_key(config, L"game_flow", L"briefing16", false);
    f.expert_ingame = f.battle_rules && feature_key(config, L"battle_rules", L"expert_ingame", false);
    f.drag_learn_multi = f.drag_learn && feature_key(config, L"officer_ui", L"drag_learn_multi", false);
    f.sprite_fallback = f.ccz_sprites && feature_key(config, L"ccz", L"sprite_fallback", false);
    // 2026-11-18 user: 의용병 무리 moved to the gameplay tab (convenience group); the [ccz] group no longer gates it. The key stays
    // [ccz] bands (saved settings keep working). Pictures follow 조조전 병종 그림: on = sprites, off = CCZ portrait on the flag.
    f.ccz_bands = on && feature_key(config, L"ccz", L"bands", false);   // docs/NEUTRAL_BANDITS.md (local248: band_dev_spawn retired)
    f.band_troops = (std::max)(1u, (std::min)(3u, GetPrivateProfileIntW(L"ccz", L"band_troops", 3, config)));
    f.band_morale = (std::max)(1u, (std::min)(3u, GetPrivateProfileIntW(L"ccz", L"band_morale", 2, config)));
    f.ambush = f.advisor_briefing && feature_key(config, L"game_flow", L"ambush", false);
    f.destruction_ticket = f.ambush && feature_key(config, L"game_flow", L"destruction_ticket", true);   // user 2026-11-13
    // retired 2026-10-28 (mission_v2 conflict); keys no longer read
    f.departures = f.group_advance = f.support = false;
}
inline int game_day();
#ifdef AWARD_WAREHOUSE
static void award_ai_city(Obj city);
static void award_synchronize();
#endif

// Decision labels for ai_trace: the convenience module reads them after each traced node call on the
// same AI thread (SAN14CombatTraceStep). Off until the trace asks for them.
inline volatile LONG mod_step_wanted;
inline __declspec(thread) char mod_step_text[160];
inline __declspec(thread) unsigned mod_step_length;
[[maybe_unused]] inline bool mod_step(const char *name) {
    if(!mod_step_wanted || !name)return true;
    size_t n=strlen(name);
    if(mod_step_length+n+2>=sizeof(mod_step_text))return true;
    if(mod_step_length)mod_step_text[mod_step_length++]=';';
    memcpy(mod_step_text+mod_step_length,name,n);mod_step_length+=static_cast<unsigned>(n);mod_step_text[mod_step_length]=0;
    return true;
}
#ifndef NO_RUNTIME_LOG
// User 2026-10-29: logger cost only; includes lock wait and IO, including perf_*.
namespace log_perf {
alignas(8) inline volatile LONG64 calls,total_us,max_us;
inline const LONGLONG frequency=[] {LARGE_INTEGER f={};QueryPerformanceFrequency(&f);return f.QuadPart?f.QuadPart:1;}();
struct Sample {
    LARGE_INTEGER start;
    Sample(){QueryPerformanceCounter(&start);}
    ~Sample(){
        LARGE_INTEGER end;QueryPerformanceCounter(&end);
        LONG64 us=(end.QuadPart-start.QuadPart)*1000000/frequency;
        InterlockedIncrement64(&calls);InterlockedAdd64(&total_us,us);
        LONG64 old=InterlockedCompareExchange64(&max_us,0,0);
        while(us>old){LONG64 seen=InterlockedCompareExchange64(&max_us,us,old);if(seen==old)break;old=seen;}
    }
};
}
inline volatile LONG log_day = -1;
inline void write_log_event(const char *kind, unsigned army = 0, unsigned value = 0, unsigned hex = 0)
{
    log_perf::Sample perf_sample;
    if(strncmp(kind,"build_skip",10))mod_step(kind);
    AcquireSRWLockExclusive(&log_lock);
    {
        char line[256];
        SYSTEMTIME t; GetLocalTime(&t);
        // Last game day read by game_day() on an AI thread (ai_trace's calendar); no game call here,
        // since the loader worker also logs.
        int day = static_cast<int>(log_day);
        int n = sprintf_s(line, "%04u-%02u-%02u %02u:%02u:%02u day=%d thread=%lu %s army=%u value=%u hex=%u\r\n",
                          t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
                          day, GetCurrentThreadId(), kind, army, value, hex);
        if (n > 0) log_append.write(log_path,line,static_cast<DWORD>(n),reuse_log_handle!=0);
    }
    ReleaseSRWLockExclusive(&log_lock);
}
// Skip formatting, locks, IO and log-only argument evaluation in normal play.
#define log_event(...) do {if(diagnostic_logging)write_log_event(__VA_ARGS__);}while(0)
#else
// No logger, log paths, locks, formatting or argument evaluation in release.
#define log_event(...) do {if constexpr(false)runtime_log_discard(__VA_ARGS__);}while(0)
#endif

// game-day service (W4-6): the calendar day, read on the calling game thread; it also feeds the log's day= column.
inline int game_day() {
    Obj context = pointer(world(),0x871E8);
    int day = fn<int(__fastcall *)(Obj)>(GA(GA_32DD00))(context+0x34);
#ifndef NO_RUNTIME_LOG
    InterlockedExchange(&log_day, day);
#endif
    return day;
}
inline bool valid(Obj obj)
{
    return obj != nullptr && fn<int(__fastcall *)(Obj)>(GA(GA_331780))(obj) != 0;
}
inline Obj table(unsigned offset, unsigned id, unsigned maximum)
{
    Obj root = world();
    return root && id <= maximum ? pointer(root, offset + id * 8) : nullptr;
}
inline unsigned army_force(Obj army)
{
    return fn<unsigned(__fastcall *)(Obj)>(GA(GA_22E690))(army);
}
inline unsigned player_force()
{
    Obj root = world();
    if (!root) return 0;
    Obj game_context = pointer(root, 0x871E8);
    if (!game_context) return 0;
    Obj force = fn<Obj(__fastcall *)(Obj)>(GA(GA_330B90))(game_context);
    if (!valid(force)) return 0;
    auto get_id = *reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(force, 0) + 0x60);
    unsigned id = get_id(force);
    return id > 0 && id < 52 ? id : 0;
}
inline bool observer_mode()
{
    // Demo/spectator has an explicit zero controlling-force field. A failed
    // player lookup alone must not turn an unknown context into an all-AI game.
    Obj root = world();
    Obj context = root ? pointer(root, 0x871E8) : nullptr;
    return context != nullptr && context[0x3A] == 0;
}

struct Link { Obj value; Link *next; };
struct GameList { Obj vtable; unsigned *handle; };
inline void manager_lock()
{
    if (dword(image, GA(GA_27D4100))) EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(image + GA(GA_27D4108)));
}
inline void manager_unlock()
{
    if (dword(image, GA(GA_27D4100))) LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(image + GA(GA_27D4108)));
}
inline void list_init(GameList &list, unsigned vtable = GA(GA_1346670))
{
    list.vtable = image + vtable; list.handle = nullptr;
    fn<void(__fastcall *)(GameList *)>(GA(GA_1AC60))(&list);
}
inline void list_free(GameList &list) { fn<void(__fastcall *)(GameList *)>(GA(GA_1BFFF0))(&list); }
inline bool list_add(GameList &list, Obj obj)
{
    if (!list.handle) return false;
    manager_lock();
    Obj entry = fn<Obj(__fastcall *)(Obj, unsigned, unsigned char)>(GA(GA_18D50))(image + GA(GA_27D4130), *list.handle, 1);
    if (entry) *reinterpret_cast<Obj *>(entry) = obj;
    manager_unlock();
    return entry != nullptr;
}
inline Link *list_first(GameList &list)
{
    if (!list.handle || !pointer(image, GA(GA_27D4138)) || *list.handle >= dword(image, GA(GA_27D4170))) return nullptr;
    Obj heads = pointer(image, GA(GA_27D4140));
    return heads ? *reinterpret_cast<Link **>(heads + *list.handle * 8) : nullptr;
}

#include "native_patch_core.inl"   // stateless patch helpers shared by every unit (2026-11-16 refactor)
