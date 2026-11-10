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
static bool call_trace_enabled=false;
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
static const SplitCombatApi *split_combat_api;
#endif

/* All game callbacks run on the game's own AI thread, synchronously from its
 * existing nodes. The loader worker only installs validated code/vtable links;
 * it never enumerates units or issues orders. Expert regional settings share
 * native unit stat/preview paths; actual AI pairs retain their fixed rate. */
// AI-CONTRACT [ABI]: Obj is a borrowed native object, never an owning buffer. Every offset/RVA is build-specific. Preserve __fastcall signatures and validate caller + original bytes before changing bindings.
using Obj = unsigned char *;
using Execute = uint64_t(__fastcall *)(Obj, Obj, Obj);
static Obj image;
static HMODULE self_module;
static volatile LONG enabled;
#ifndef NO_RUNTIME_LOG
static SRWLOCK log_lock = SRWLOCK_INIT;
#endif
// Native path searches reuse a node buffer. A callback must not start another
// mod probe while an original search is still consuming that buffer.
static __declspec(thread) unsigned native_search_depth;
// Settings (2026-10-22): every switch lives in AI_TacticsMod\기능설정.ini, managed by the installer
// (기타(개발용) tab for diagnostics). A key missing there falls back to its former file, then default.
static bool mod_ini_path(wchar_t *out,const wchar_t *file)
{
    if(!GetModuleFileNameW(self_module,out,MAX_PATH))return false;
    wchar_t *slash=wcsrchr(out,L'\\');if(!slash)return false;*slash=0;
    return !wcscat_s(out,MAX_PATH,L"\\AI_TacticsMod\\") && !wcscat_s(out,MAX_PATH,file);
}
// AI-CONTRACT [settings]: Maintain the unified INI lookup and legacy fallback. A new exposed key must be added to InstallerGui.cs and distribution/FeatureSelect.ps1 in the same patch; keep parent && child gating.
static int mod_setting(const wchar_t *section,const wchar_t *key,int fallback,const wchar_t *legacy=nullptr)
{
    constexpr UINT kAbsent=0x7FFFFFF0u;wchar_t path[MAX_PATH];
    if(mod_ini_path(path,L"기능설정.ini")) {UINT v=GetPrivateProfileIntW(section,key,kAbsent,path);if(v!=kAbsent)return static_cast<int>(v);}
    if(legacy && mod_ini_path(path,legacy))return static_cast<int>(GetPrivateProfileIntW(section,key,static_cast<UINT>(fallback),path));
    return fallback;
}
// [groups] diagnostics: master of the 기타(개발용) tab; absent = 1 so older settings keep working.
static bool dev_group() {return mod_setting(L"groups",L"diagnostics",1)!=0;}
#ifndef NO_RUNTIME_LOG
static wchar_t log_path[MAX_PATH];
#include "log_append.h"
static diagnostics::AppendFile log_append;
static volatile LONG reuse_log_handle=1;
static volatile LONG diagnostic_logging;
[[maybe_unused]] static void load_diagnostic_settings(const wchar_t *config) {
    InterlockedExchange(&diagnostic_logging,GetPrivateProfileIntW(L"diagnostics",L"enabled",0,config)!=0?1:0);
}
static void initialize_log_settings() {
    InterlockedExchange(&diagnostic_logging,dev_group() && mod_setting(L"diagnostics",L"enabled",0,L"diagnostics.ini")!=0?1:0);
#ifdef RUNTIME_COST_PROFILE
    costprofile::set_crumbs_enabled(diagnostic_logging!=0);
    costprofile::call_trace_enabled=dev_group() && mod_setting(L"diagnostics",L"call_trace",0,L"diagnostics.ini")!=0;
#endif
    InterlockedExchange(&reuse_log_handle,mod_setting(L"diagnostics",L"reuse_handle",1,L"diagnostics.ini")!=0?1:0);
}
#else
static constexpr LONG diagnostic_logging=0,reuse_log_handle=0;
static void initialize_log_settings() {}
template<class... T> static void runtime_log_discard(T...) {}
#endif
#include "native_crash_diagnostic.inl"
#ifdef INJURY_PROTECTION
static void injury_synchronize_city(Obj city);
static bool injury_auto_district(Obj district);
#endif

#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG) && !defined(SPLIT_COMBAT)
extern "C" void gpu_profile_crumb(const char *name,bool entering) {
    if(costprofile::call_trace_enabled)costprofile::crumb(name,entering?costprofile::Enter:costprofile::Leave);
}
#endif
#ifdef TACTICS_FIXTURE
static void *fixture_function(unsigned rva);
template<class T> static T fn(unsigned rva) { return reinterpret_cast<T>(fixture_function(rva)); }
#else
template<class T> static T fn(unsigned rva) {
#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG)
    if(costprofile::call_trace_enabled)costprofile::crumb("game_function",costprofile::Call,rva);
#endif
    return rva ? reinterpret_cast<T>(image + rva) : nullptr;   // 0 = unresolved address table id (docs/PATCH_RESILIENCE.md): never call image+0
}
#endif
// Layout hook manager chains (design 8.0 G1): `next` is the function the manager hands a replacement; null (no manager:
// split fixtures, combat module) means the original game function.
template<class T> static T next_or(void *next, unsigned rva) { return next ? reinterpret_cast<T>(next) : fn<T>(rva); }
// Same for a typed next pointer that the manager fills in (null in fixtures that call a replacement body directly).
template<class T> static T next_pick(T next, unsigned rva) { return next ? next : fn<T>(rva); }
static unsigned short word(Obj p, unsigned off) { return *reinterpret_cast<unsigned short *>(p + off); }
static unsigned dword(Obj p, unsigned off) { return *reinterpret_cast<unsigned *>(p + off); }
static Obj pointer(Obj p, unsigned off) { return *reinterpret_cast<Obj *>(p + off); }
static Obj world() { return pointer(image, GA(GA_2780F60)); }
// User-facing feature switches (AI_TacticsMod\기능설정.ini). Read once before
// hooks publish; restart applies. Each key is read from its section first, then
// from the legacy [features] key of the same name, then the default.
// [groups] combat (loader only), convenience, performance gate their sections.
// Coupled code shares one key: battle_rules = combat pair scope + adjacency +
// expert attack/defense settings; tooltip_collab = imported tooltip code whose
// UI init also runs the collab first unlock. The domestic tick hook carries
// domestic, awards (AI distribution) and injury (herbal sync) and is published
// when any of them is on. Merit/warehouse save links stay published always.
// AI-CONTRACT [variants]: This translation unit builds twice with SPLIT_BASE/SPLIT_COMBAT. Follow native_split_install.inl to prove reachability and owner; a declaration or fixture alone does not establish runtime activation.
struct ModFeatures {
    bool convenience = true, performance = true;
    bool merit = true, awards = true, recruit = true, injury = true;          // [personnel]
    bool merit_friendship = true;                                            // [personnel] 2026-11-05: stage-3 mutual friendship on top of the native effect
    unsigned merit_troops_level = 4;                                         // [personnel] 1..4 = stage 1/2 troops 1500/2000/2500/3000 (default 3000)
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
    unsigned terrain_diag_mode = 0;                                         // [diagnostics]
    bool building_destruction_all = false;                                 // [stratagem], gameplay tab
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
    bool siege_ring10 = false, siege_stall_near = false;                    // [combat] under mission_v2
    bool attack_eval70 = false, retreat_eval60 = false;                     // [combat] attack review 70%, field retreat recheck 60%
    bool path_wall = false, siege_guard = false;                            // [combat] path_wall needs obstacle
    bool delegated_exclude = false;                                         // [combat] local18: player delegated corps leave the campaign rules
    bool awards_horse = false, awards_v16 = false;                          // [personnel] under awards_ai
    bool merit_bond16 = false;                                              // [personnel] under merit
    bool briefing16 = false;                                                // [game_flow] under advisor_briefing
    bool expert_ingame = false;                                             // [battle_rules] under battle_rules
    bool drag_learn_multi = false;                                          // [officer_ui] under drag_learn
    bool auto_search = false, auto_reward = false;                          // [game_flow] main-screen toggles (local17, default off)
    bool sprite_fallback = false;                                           // [ccz] under ccz_sprites
};
static ModFeatures mod_features;
static bool feature_key(const wchar_t *config, const wchar_t *section, const wchar_t *key, bool fallback)
{
    const int legacy = static_cast<int>(GetPrivateProfileIntW(L"features", key, fallback ? 1 : 0, config));
    return GetPrivateProfileIntW(section, key, legacy, config) != 0;
}
static void load_mod_features()
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
    f.building_destruction_all = on && feature_key(config, L"stratagem", L"building_destruction_all", false);
    f.diplomacy_overinvest = false; // retired 2026-11-03: native cap/marker restored; key no longer read
    f.diplomacy_counterturn = on && feature_key(config, L"diplomacy", L"diplomacy_counterturn", false);
    f.damage_preview = on && feature_key(config, L"officer_ui", L"damage_preview", true);
    f.traits9 = on && feature_key(config, L"extensions", L"traits9", true);
    f.tooltip_collab = on && feature_key(config, L"extensions", L"tooltip_collab", true);
    f.stats200 = on && feature_key(config, L"extensions", L"stats200", true);
    f.stats200_growth = f.stats200 && feature_key(config, L"extensions", L"stats200_growth", true);
    f.battle_deaths = on && feature_key(config, L"extensions", L"battle_deaths", true);
    f.scenery_skip = on && feature_key(config, L"extensions", L"scenery_skip", true);
    f.speed_buttons = on && feature_key(config, L"game_flow", L"speed_buttons", true);
    f.occupy_fast = on && feature_key(config, L"game_flow", L"occupy_fast", true);
    f.fast_save = on && feature_key(config, L"game_flow", L"fast_save", true);
    f.caravan_stay = on && feature_key(config, L"game_flow", L"caravan_stay", false);
    f.auto_search = on && feature_key(config, L"game_flow", L"auto_search", false);
    f.auto_reward = on && feature_key(config, L"game_flow", L"auto_reward", false);
    f.advisor_briefing = on && feature_key(config, L"game_flow", L"advisor_briefing", false);
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
    f.siege_stall_near = f.mission_v2 && feature_key(config, L"combat", L"siege_stall_near", false);
    f.attack_eval70 = feature_key(config, L"combat", L"attack_eval70", false);
    f.retreat_eval60 = feature_key(config, L"combat", L"retreat_eval60", false);
    f.path_wall = f.obstacle && feature_key(config, L"combat", L"path_wall", false);
    f.siege_guard = feature_key(config, L"combat", L"siege_guard", false);
    f.delegated_exclude = feature_key(config, L"combat", L"delegated_exclude", false);
    f.awards_v16 = f.awards_ai && feature_key(config, L"personnel", L"awards_v16", false);
    f.awards_horse = f.awards_ai && feature_key(config, L"personnel", L"awards_horse", false);
    f.merit_bond16 = f.merit && feature_key(config, L"personnel", L"merit_bond16", false);
    f.briefing16 = f.advisor_briefing && feature_key(config, L"game_flow", L"briefing16", false);
    f.expert_ingame = f.battle_rules && feature_key(config, L"battle_rules", L"expert_ingame", false);
    f.drag_learn_multi = f.drag_learn && feature_key(config, L"officer_ui", L"drag_learn_multi", false);
    f.sprite_fallback = f.ccz_sprites && feature_key(config, L"ccz", L"sprite_fallback", false);
    // retired 2026-10-28 (mission_v2 conflict); keys no longer read
    f.departures = f.group_advance = f.support = false;
}
static int game_day();
#ifdef AWARD_WAREHOUSE
static void award_ai_city(Obj city);
static void award_synchronize();
#endif

// Decision labels for ai_trace: the convenience module reads them after each traced node call on the
// same AI thread (SAN14CombatTraceStep). Off until the trace asks for them.
static volatile LONG mod_step_wanted;
static __declspec(thread) char mod_step_text[160];
static __declspec(thread) unsigned mod_step_length;
[[maybe_unused]] static bool mod_step(const char *name) {
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
alignas(8) static volatile LONG64 calls,total_us,max_us;
static const LONGLONG frequency=[] {LARGE_INTEGER f={};QueryPerformanceFrequency(&f);return f.QuadPart?f.QuadPart:1;}();
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
static volatile LONG log_day = -1;
static void write_log_event(const char *kind, unsigned army = 0, unsigned value = 0, unsigned hex = 0)
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
static int game_day() {
    Obj context = pointer(world(),0x871E8);
    int day = fn<int(__fastcall *)(Obj)>(GA(GA_32DD00))(context+0x34);
#ifndef NO_RUNTIME_LOG
    InterlockedExchange(&log_day, day);
#endif
    return day;
}
static bool valid(Obj obj)
{
    return obj != nullptr && fn<int(__fastcall *)(Obj)>(GA(GA_331780))(obj) != 0;
}
static Obj table(unsigned offset, unsigned id, unsigned maximum)
{
    Obj root = world();
    return root && id <= maximum ? pointer(root, offset + id * 8) : nullptr;
}
static unsigned army_force(Obj army)
{
    return fn<unsigned(__fastcall *)(Obj)>(GA(GA_22E690))(army);
}
static unsigned player_force()
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
static bool observer_mode()
{
    // Demo/spectator has an explicit zero controlling-force field. A failed
    // player lookup alone must not turn an unknown context into an all-AI game.
    Obj root = world();
    Obj context = root ? pointer(root, 0x871E8) : nullptr;
    return context != nullptr && context[0x3A] == 0;
}

struct Link { Obj value; Link *next; };
struct GameList { Obj vtable; unsigned *handle; };
static void manager_lock()
{
    if (dword(image, GA(GA_27D4100))) EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(image + GA(GA_27D4108)));
}
static void manager_unlock()
{
    if (dword(image, GA(GA_27D4100))) LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(image + GA(GA_27D4108)));
}
static void list_init(GameList &list, unsigned vtable = GA(GA_1346670))
{
    list.vtable = image + vtable; list.handle = nullptr;
    fn<void(__fastcall *)(GameList *)>(GA(GA_1AC60))(&list);
}
static void list_free(GameList &list) { fn<void(__fastcall *)(GameList *)>(GA(GA_1BFFF0))(&list); }
static bool list_add(GameList &list, Obj obj)
{
    if (!list.handle) return false;
    manager_lock();
    Obj entry = fn<Obj(__fastcall *)(Obj, unsigned, unsigned char)>(GA(GA_18D50))(image + GA(GA_27D4130), *list.handle, 1);
    if (entry) *reinterpret_cast<Obj *>(entry) = obj;
    manager_unlock();
    return entry != nullptr;
}
static Link *list_first(GameList &list)
{
    if (!list.handle || !pointer(image, GA(GA_27D4138)) || *list.handle >= dword(image, GA(GA_27D4170))) return nullptr;
    Obj heads = pointer(image, GA(GA_27D4140));
    return heads ? *reinterpret_cast<Link **>(heads + *list.handle * 8) : nullptr;
}

#define UNITY_SLOT_AI_SUPPORT
#include "module_unity.inl"
#undef UNITY_SLOT_AI_SUPPORT
#include "core_services.h"   // subscription services (map-tick, domestic-tick, ui-dialogs, overlay)
#define UNITY_SLOT_CAMPAIGN
#include "module_unity.inl"
#undef UNITY_SLOT_CAMPAIGN
#define UNITY_SLOT_DEPARTURES
#include "module_unity.inl"
#undef UNITY_SLOT_DEPARTURES
#define UNITY_SLOT_EXPERT
#include "module_unity.inl"
#undef UNITY_SLOT_EXPERT

// crc32_run continues a running (not yet inverted) CRC state; crc32 is the whole-buffer value.
static unsigned crc32_run(unsigned crc, const unsigned char *data, unsigned size)
{
    for (unsigned i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}
static unsigned crc32(const unsigned char *data, unsigned size) {return ~crc32_run(~0u, data, size);}
#define UNITY_SLOT_PERSONNEL_POLICY
#include "module_unity.inl"
#undef UNITY_SLOT_PERSONNEL_POLICY
#if !defined(SPLIT_COMBAT) && (defined(PERSONNEL_CANDIDATE) || defined(RECRUITMENT_UI_CANDIDATE))
static bool personnel_healthy_actor(Obj node,Obj context,unsigned field);
static Obj personnel_context_object(Obj node,Obj context,unsigned field);
#endif
#define UNITY_SLOT_DOMESTIC
#include "module_unity.inl"
#undef UNITY_SLOT_DOMESTIC

// Text size / CRC of the running build come from the address table (user 2026-11-05 patch resilience); fixtures and
// unknown builds use the reference build's values. kTextSize/kTextCrc32 (native_profile.h) stay as the reference-build constants.
static unsigned text_size_now() {int b=ga::build_index();return ga_builds[b<0?0:b].text_size;}
static unsigned text_crc_now() {int b=ga::build_index();return ga_builds[b<0?0:b].text_crc;}
[[maybe_unused]] static bool code_ready()
{
    __try {
        return crc32(image + 0x1000, text_size_now()) == text_crc_now();
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// local11 (2026-11-06, docs/HISTORY.md section 18): the whole-text CRC with the displaced prefix of every OBSERVED entry
// (ga::observers: E9 rel32 leaving the image + NOP padding, San14Sim's version.dll) replaced by the original bytes. Without observers it is
// exactly crc32(text). Decision: the observers are taken out of the comparison (text_exact stays true) instead of weakening text_exact into
// a second "exact except observers" flag: every dependent (the observer-aware guards of battle_deaths, trait_hover) keeps one meaning,
// "the text is the known build", and a text that differs anywhere else still falls back to the compat rule below.
static unsigned crc32_text_observed(ga::ObserverSet &set)
{
    ga::observers(image, set);
    constexpr unsigned base = 0x1000;
    const unsigned size = text_size_now();
    unsigned order[GA_OBSERVER_COUNT], n = 0;
    for (unsigned k = 0; k < set.count; ++k) {    // prefixes inside .text, ascending
        if (set.rva[k] < base || set.rva[k] - base + set.len[k] > size) continue;
        unsigned j = n++;
        while (j && set.rva[order[j - 1]] > set.rva[k]) {order[j] = order[j - 1]; --j;}
        order[j] = k;
    }
    unsigned crc = ~0u, pos = 0;
    for (unsigned q = 0; q < n; ++q) {
        const unsigned k = order[q], off = set.rva[k] - base;
        if (off < pos) continue;                  // entries are distinct functions: never overlap, never counted twice
        crc = crc32_run(crc, image + base + pos, off - pos);
        crc = crc32_run(crc, set.bytes[k], set.len[k]);
        pos = off + set.len[k];
    }
    crc = crc32_run(crc, image + base + pos, size - pos);
    return ~crc;
}
static ga::ObserverSet text_observers;   // observers taken out of the last readiness CRC (logged once the text is accepted)
// Steam may still be decoding .text. A mismatched CRC alone is never readiness.
static bool text_exact=true;
static constexpr unsigned code_poll_limit=1200;
struct CodeReadiness {unsigned polls,crc,mask;bool sampled;};
static CodeReadiness code_readiness;
// Readiness probes: five-byte prologues of 12 table ids whose bytes are identical in every known build (game_addresses_data.inl).
struct DecodeProbe {unsigned rva;unsigned char bytes[5];};
static DecodeProbe make_probe(unsigned i) {DecodeProbe p={};p.rva=GA(ga_probes[i].id);memcpy(p.bytes,ga_probes[i].bytes,5);return p;}
static const DecodeProbe decode_probes[]={
    make_probe(0),make_probe(1),make_probe(2),make_probe(3),make_probe(4),make_probe(5),
    make_probe(6),make_probe(7),make_probe(8),make_probe(9),make_probe(10),make_probe(11)
};
static unsigned decode_matches(unsigned mask) {
    unsigned n=0;for(;mask;mask>>=1)n+=mask&1;return n;
}
static bool code_sample_ready(CodeReadiness &s,unsigned crc,unsigned mask) {
    if(s.polls++>=code_poll_limit)return false;
    constexpr unsigned total=sizeof(decode_probes)/sizeof(decode_probes[0]);
    unsigned matches=decode_matches(mask);
    if(crc==text_crc_now()) {
        text_exact=true;
        for(unsigned k=0;k<text_observers.count;++k)log_event("observer_hook_tolerated",0,text_observers.rva[k]);   // feature 0 = whole-text CRC
        return true;
    }
    bool ready=matches*4>=total*3 && s.sampled && s.crc==crc && s.mask==mask;
    s.sampled=matches*4>=total*3;s.crc=crc;s.mask=mask;
    if(ready) {text_exact=false;log_event("text_crc_mismatch_compat",matches,total);}
    return ready;
}
// Unknown game build (no per-build table): ready once the signature resolver finds 3/4 of the signature/RTTI ids on two
// consecutive polls; the resolved table is then adopted for every feature (docs/PATCH_RESILIENCE.md).
static bool ga_unknown_ready(CodeReadiness &s) {
    if(s.polls++>=code_poll_limit)return false;
    LARGE_INTEGER f,a,b;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
    std::vector<unsigned> v(GA_COUNT);
    ga::resolve_image(image,reinterpret_cast<uint64_t>(image),v.data());
    QueryPerformanceCounter(&b);
    unsigned want=0,got=0;
    for(unsigned i=0;i<GA_COUNT;++i)if(ga_entries[i].method!=0 && ga_entries[i].method!=2 && ga_entries[i].method!=5) {++want;if(v[i])++got;}
    unsigned mask=got*4>=want*3?0xFFFu:0;
    bool ready=mask && s.sampled && s.mask==mask;
    s.sampled=mask!=0;s.crc=0;s.mask=mask;
    if(ready) {
        text_exact=false;ga::adopt(v.data(),GA_COUNT);
        log_event("addr_resolve",static_cast<unsigned>((b.QuadPart-a.QuadPart)*10000/f.QuadPart),ga::state().resolved,ga::state().unresolved);
    }
    return ready;
}
static bool code_unpacked_ready(CodeReadiness &s) {
    __try {
        if(ga::build_index()<0 && ga::state().game_host)return ga_unknown_ready(s);
        unsigned mask=0;
        for(unsigned i=0;i<sizeof(decode_probes)/sizeof(decode_probes[0]);++i)
            if(!ga::mcmp_at(image,decode_probes[i].rva,decode_probes[i].bytes,5))mask|=1u<<i;
        // Avoid hashing encrypted text on each poll. Strict text matches every probe.
        if(decode_matches(mask)*4<3*(sizeof(decode_probes)/sizeof(decode_probes[0])))
            return code_sample_ready(s,0,mask);
        return code_sample_ready(s,crc32_text_observed(text_observers),mask);
    } __except(EXCEPTION_EXECUTE_HANDLER) {s.sampled=false;++s.polls;return false;}
}
static bool patch_pointer(unsigned rva, unsigned expected, void *replacement)
{
    auto slot = reinterpret_cast<void **>(image + rva);
    DWORD old = 0;
    if (*slot != image + expected || !VirtualProtect(slot, 8, PAGE_READWRITE, &old)) return false;
    void *previous = InterlockedCompareExchangePointer(slot, replacement, image + expected);
    DWORD restored = 0; VirtualProtect(slot, 8, old, &restored);
    return previous == image + expected;
}
static void restore_pointer(unsigned rva, unsigned original, void *replacement)
{
    auto slot = reinterpret_cast<void **>(image + rva);
    DWORD old = 0, restored = 0;
    if (!VirtualProtect(slot, 8, PAGE_READWRITE, &old)) return;
    InterlockedCompareExchangePointer(slot, image + original, replacement);
    VirtualProtect(slot, 8, old, &restored);
}
[[maybe_unused]] static bool other_mod_present(const wchar_t *name) {
    if(GetModuleHandleW(name))return true;
    wchar_t path[MAX_PATH],folder[MAX_PATH];if(!GetModuleFileNameW(nullptr,path,MAX_PATH))return false;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return false;*slash=0;
    if(wcscpy_s(folder,MAX_PATH,path) || wcscat_s(folder,MAX_PATH,L"\\") || wcscat_s(folder,MAX_PATH,name))return false;
    DWORD attr=GetFileAttributesW(folder);return attr!=INVALID_FILE_ATTRIBUTES && !(attr&FILE_ATTRIBUTE_DIRECTORY);
}
#ifndef SPLIT_COMBAT   // recruitment hook tables: the combat DLL never publishes them (W5-1)
#ifdef RECRUITMENT_UI_CANDIDATE
struct RecruitUILink {unsigned slot,original;void *replacement;void **next;};
static const RecruitUILink recruit_ui_links[]={
    {GA(GA_1444070),GA(GA_6F3FF0),reinterpret_cast<void *>(&recruit_layout_init),&next_recruit_layout_init},
    {GA(GA_1443E78),GA(GA_735F60),reinterpret_cast<void *>(&recruit_state_update),&next_recruit_state_update},
    {GA(GA_13AF800),GA(GA_2C2BA0),reinterpret_cast<void *>(&recruit_selector_init),&next_recruit_selector_init},
    {GA(GA_13AF798),GA(GA_6C1040),reinterpret_cast<void *>(&recruit_selector_close),&next_recruit_selector_close}
};
static bool recruit_ui_guards_match() {
#ifndef LAYOUT_ROUTED   // routed: slot ownership is judged by the Layout hook manager
    for(const auto &link:recruit_ui_links)if(pointer(image,link.slot)!=image+link.original)return false;
#endif
    return true;
}
static bool publish_recruit_ui() {
    for(const auto &link:recruit_ui_links)if(!ROUTE_VT(link.slot,link.original,link.replacement,link.next))return false;
    return true;
}
static void restore_recruit_ui() {
#ifndef LAYOUT_ROUTED
    for(const auto &link:recruit_ui_links)restore_pointer(link.slot,link.original,link.replacement);
#endif
}
#endif
// Publish only the two recruitment AI nodes in the integrated build. The
// broader, uninstalled PERSONNEL_CANDIDATE also changes unrelated commands.
#if defined(PERSONNEL_CANDIDATE) || defined(RECRUITMENT_UI_CANDIDATE)
struct RecruitAILink {unsigned slot,original;Execute replacement;const unsigned char *entry;};
static const unsigned char recruit_ai_target_entry[]={0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0xC7,0x44,0x24,0x28,0xFE,0xFF,0xFF,0xFF,0x48};
static const unsigned char recruit_ai_command_entry[]={0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x40,0x48,0x8B,0xF9,0x48,0xC7,0x44};
static const RecruitAILink recruit_ai_links[]={
    {GA(GA_134FE18),GA(GA_7C300),&recruit_ai_targets,recruit_ai_target_entry},
    {GA(GA_13493D8),GA(GA_2D720),&recruit_ai_command,recruit_ai_command_entry}
};
static bool recruit_ai_guards_match() {
    for(const auto &link:recruit_ai_links)
        if(pointer(image,link.slot)!=image+link.original || ga::mcmp_at(image,link.original,link.entry,16))return false;
    return true;
}
static bool publish_recruit_ai() {
    for(const auto &link:recruit_ai_links)
        if(!patch_pointer(link.slot,link.original,reinterpret_cast<void *>(link.replacement)))return false;
    return true;
}
static void restore_recruit_ai() {
    for(const auto &link:recruit_ai_links)
        restore_pointer(link.slot,link.original,reinterpret_cast<void *>(link.replacement));
}
#endif
#endif   // !SPLIT_COMBAT
static bool replace_code16(unsigned rva,const unsigned char *expected, const unsigned char *replacement)
{
    /* CALL crosses an eight-byte boundary: publish the enclosing aligned
     * sixteen bytes with CMPXCHG16B so no thread can decode a partial target. */
    if (!IsProcessorFeaturePresent(PF_COMPARE_EXCHANGE128)) return false;
    auto block = reinterpret_cast<volatile LONG64 *>(image + rva);
    alignas(16) LONG64 comparison[2], desired[2];
    memcpy(comparison, expected, 16); memcpy(desired, replacement, 16);
    DWORD old = 0, restored = 0;
    if (!VirtualProtect(image + rva, 16, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool result = _InterlockedCompareExchange128(block, desired[1], desired[0], comparison) != 0;
    VirtualProtect(image + rva, 16, old, &restored);
    FlushInstructionCache(GetCurrentProcess(), image + rva, 16);
    return result;
}
[[maybe_unused]] static bool replace_call(const unsigned char *expected,const unsigned char *replacement) {
    return replace_code16(GA(GA_D3510),expected,replacement);
}
struct CodeLink {
    unsigned rva,block;
    unsigned char original[16],modified[16];
    bool published;
};
static void absolute_jump(Obj at, void *target) {
    at[0]=0xFF;at[1]=0x25;memset(at+2,0,4);
    *reinterpret_cast<void **>(at+6)=target;
}
// local11: a patch that would overwrite the displaced prefix of an observed entry (San14Sim's E9 hook) is a real conflict: the observer keeps
// the entry, the feature that wanted it stays off (its own prepare failure follows). feature 0 = the hook preparer does not know its feature.
static bool observer_conflict(unsigned rva,unsigned count) {
    unsigned entry=0;
    if(!ga::observer_overlaps(image,rva,count,&entry))return false;
    log_event("observer_hook_conflict",0,entry);
    return true;
}
static bool prepare_link(CodeLink &link,unsigned rva,unsigned char opcode,Obj relay) {
    link={};link.rva=rva;link.block=rva&~15u;
    if(observer_conflict(rva,5))return false;
    unsigned offset=rva-link.block;if(offset>11)return false;
    intptr_t delta=reinterpret_cast<intptr_t>(relay)-reinterpret_cast<intptr_t>(image+rva+5);
    if(delta<INT32_MIN || delta>INT32_MAX)return false;
    memcpy(link.original,image+link.block,16);memcpy(link.modified,link.original,16);
    link.modified[offset]=opcode;memcpy(link.modified+offset+1,&delta,4);return true;
}
[[maybe_unused]] static bool prepare_bytes(CodeLink &link,unsigned rva,const unsigned char *bytes,unsigned count) {
    link={};link.rva=rva;link.block=rva&~15u;
    if(observer_conflict(rva,count))return false;
    unsigned offset=rva-link.block;if(offset+count>16)return false;
    memcpy(link.original,image+link.block,16);memcpy(link.modified,link.original,16);
    memcpy(link.modified+offset,bytes,count);return true;
}
// CALL rel32 redirection for any call site. Inside one 16-byte block the swap is the atomic
// replace_code16; a call whose rel32 crosses a block boundary (site offset > 11) is written once
// at install, before the patched UI/command path can run, and restored the same way.
struct CallLink {CodeLink link;unsigned site=0;int original=0,redirected=0;bool raw=false,published=false;};
static bool call_prepare(CallLink &c,unsigned site,Obj relay) {
    c=CallLink{};c.site=site;
    if(image[site]!=0xE8)return false;
    intptr_t delta=reinterpret_cast<intptr_t>(relay)-reinterpret_cast<intptr_t>(image+site+5);
    if(delta<INT32_MIN || delta>INT32_MAX)return false;
    c.original=*reinterpret_cast<int *>(image+site+1);c.redirected=static_cast<int>(delta);
    c.raw=(site&15u)>11;
    return c.raw || prepare_link(c.link,site,0xE8,relay);
}
static bool publish_link(CodeLink &link);
static bool call_write(unsigned site,int expected,int value) {
    DWORD old=0,restored=0;
    if(*reinterpret_cast<int *>(image+site+1)!=expected || !VirtualProtect(image+site,5,PAGE_EXECUTE_READWRITE,&old))return false;
    memcpy(image+site+1,&value,4);VirtualProtect(image+site,5,old,&restored);
    FlushInstructionCache(GetCurrentProcess(),image+site,5);return true;
}
static bool call_publish(CallLink &c) {
    if(!c.raw) {   // two sites in one block: build the swap from the block as it is now
        unsigned offset=c.site-c.link.block;
        memcpy(c.link.original,image+c.link.block,16);memcpy(c.link.modified,c.link.original,16);
        c.link.modified[offset]=0xE8;memcpy(c.link.modified+offset+1,&c.redirected,4);
    }
    c.published=c.raw?call_write(c.site,c.original,c.redirected):publish_link(c.link);return c.published;
}
static void restore_link(CodeLink &link);
[[maybe_unused]] static void call_restore(CallLink &c) {
    if(!c.published)return;
    if(c.raw)call_write(c.site,c.redirected,c.original);else restore_link(c.link);
    c.published=false;
}
static bool publish_link(CodeLink &link) {
    link.published=replace_code16(link.block,link.original,link.modified);return link.published;
}
static void restore_link(CodeLink &link) {
    if(link.published)replace_code16(link.block,link.modified,link.original);
}
#ifndef SPLIT_COMBAT   // recruitment hook tables: the combat DLL never publishes them (W5-1)
#ifdef RECRUITMENT_UI_CANDIDATE
struct RecruitAlertEntry {unsigned rva;unsigned char block[16];void *replacement;};
static const RecruitAlertEntry recruit_alert_entries[]={
    {GA(GA_2E1534),{0xD5,0x48,0x8B,0xCF,0xE8,0xF7,0xF4,0xF0,0xFF,0x85,0xC0,0x74,0x3B,0x48,0x85,0xF6},reinterpret_cast<void *>(&recruit_alert_candidate)},
    {GA(GA_2E12DB),{0x8D,0x45,0x50,0x48,0x8D,0x55,0x48,0x48,0x8D,0x4D,0xE0,0xE8,0xB0,0x3F,0xFD,0xFF},reinterpret_cast<void *>(&recruit_alert_search)},
    {GA(GA_2B5601),{0xBF,0xE8,0xFA,0xDF,0xF7,0xFF,0x49,0x8B,0xF7,0x4C,0x89,0x7D,0x9F,0x8B,0x45,0xBF},reinterpret_cast<void *>(&recruit_alert_location)}
};
struct RecruitAlertCode {CodeLink links[3];};
static bool recruit_alert_guards_match() {
    for(const auto &entry:recruit_alert_entries)
        if(ga::mcmp_at(image,(entry.rva&~15u),entry.block,16))return false;
    return true;
}
static bool prepare_recruit_alert(RecruitAlertCode &code,Obj relay) {
    if(!recruit_alert_guards_match())return false;
    for(unsigned i=0;i<3;++i) {
        Obj stub=relay+2944+i*32;absolute_jump(stub,recruit_alert_entries[i].replacement);
        if(!prepare_link(code.links[i],recruit_alert_entries[i].rva,0xE8,stub))return false;
    }
    return true;
}
static bool publish_recruit_alert(RecruitAlertCode &code) {
    for(auto &link:code.links)if(!publish_link(link))return false;
    return true;
}
static void restore_recruit_alert(RecruitAlertCode &code) {for(auto &link:code.links)restore_link(link);}
#endif
#endif   // !SPLIT_COMBAT
#define UNITY_SLOT_LINKS
#include "module_unity.inl"
#undef UNITY_SLOT_LINKS
#if defined(PERSONNEL_CANDIDATE) || defined(HIDDEN_LOYALTY)
[[maybe_unused]] static bool loyalty_guards_match() {
    const unsigned char numeric[]={0x48,0x0F,0x4E,0xC2},text[]={0x0F,0x4F,0xD8};
    return !ga::mcmp(image,GA_34CB96,0,numeric,sizeof(numeric)) && !ga::mcmp(image,GA_35EF25,0,text,sizeof(text));
}
#endif
#ifdef PERSONNEL_CANDIDATE
struct PersonnelNodeLink {unsigned slot,original;Execute replacement;};
static const PersonnelNodeLink personnel_links[]={
    {GA(GA_134FE18),GA(GA_7C300),&personnel_recruitment_targets},
    {GA(GA_134A340),GA(GA_39FF0),&personnel_employ_actors},
    {GA(GA_134A550),GA(GA_3DCD0),&personnel_search_actors},
    {GA(GA_13493D8),GA(GA_2D720),&personnel_employ_command},
    {GA(GA_134C580),GA(GA_54AD0),&personnel_search_command},
    {GA(GA_1355B18),GA(GA_7F4F0),&personnel_officer_candidates},
    {GA(GA_1349B20),GA(GA_35A70),&personnel_departure},
    {GA(GA_1349BC8),GA(GA_362D0),&personnel_transport},
    {GA(GA_134AA50),GA(GA_3E750),&personnel_recruit_officer},
    {GA(GA_13487B8),GA(GA_3EF50),&personnel_training_officer},
    {GA(GA_134AF48),GA(GA_3EBC0),&personnel_spy_officer}
};
static bool personnel_guards_match() {
    if(!loyalty_guards_match())return false;
    for(const auto &link:personnel_links)if(pointer(image,link.slot)!=image+link.original)return false;
    return true;
}
static bool publish_personnel_nodes() {
    for(const auto &link:personnel_links)
        if(!patch_pointer(link.slot,link.original,reinterpret_cast<void *>(link.replacement)))return false;
    return true;
}
static void restore_personnel_nodes() {
    for(const auto &link:personnel_links)
        restore_pointer(link.slot,link.original,reinterpret_cast<void *>(link.replacement));
}
#endif
static Obj allocate_near(Obj call,unsigned bytes=0x1000)
{
    SYSTEM_INFO info; GetSystemInfo(&info);
    uintptr_t start = reinterpret_cast<uintptr_t>(call) & ~(uintptr_t(info.dwAllocationGranularity) - 1);
    for (uintptr_t offset = info.dwAllocationGranularity; offset < 0x70000000; offset += info.dwAllocationGranularity) {
        for (int sign : {1, -1}) {
            uintptr_t address = sign > 0 ? start + offset : (start > offset ? start - offset : 0);
            if (!address) continue;
            Obj memory = static_cast<Obj>(VirtualAlloc(reinterpret_cast<void *>(address), bytes,
                                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            if (memory) return memory;
        }
    }
    return nullptr;
}
#include <array>
#define UNITY_SLOT_MODULES
#include "module_unity.inl"
#undef UNITY_SLOT_MODULES
#if defined(SPLIT_MODULE_FIXTURE) || defined(TACTICS_FIXTURE)
#include "native_area_index.inl" // guard-only binding for synthetic images
#include "native_path_fast.inl" // guard-only binding for synthetic images
#include "native_diplomacy_intel.inl" // guard-only binding for synthetic images
#include "native_advisor_briefing.inl" // guard-only binding for synthetic images
#endif
#if defined(SPLIT_BASE) || defined(SPLIT_COMBAT)
#define UNITY_SLOT_UI_HOSTS
#include "module_unity.inl"
#undef UNITY_SLOT_UI_HOSTS
#include "native_split_install.inl"
#else
#include "../tests/native_install_fixture.inl"
#endif

#ifdef SPLIT_BASE
static bool install() {
    const SplitCombatApi *api=split_combat_api;
    const bool valid_api=api && api->size==sizeof(SplitCombatApi) && api->abi==split_module_abi &&
        api->prepare && api->publish && api->rollback && api->activate && api->departure_scope;
    if(!valid_api) {log_event("combat_module_abi_rejected");split_combat_api=nullptr;}
    load_mod_features();
    const bool combat=valid_api && !mod_features.trace_original;
    // Prepare both owners before either publishes; failures belong to one owner.
    const bool combat_prepared=combat && api->prepare(image);
    const bool convenience_prepared=split_prepare();
    bool combat_installed=false,convenience_installed=false;
    if(combat_prepared) {
        combat_installed=api->publish();
        if(!combat_installed)api->rollback();
    } else if(combat)api->rollback();
    if(convenience_prepared) {
        convenience_installed=split_publish();
        if(!convenience_installed)split_rollback();
    } else split_rollback();
    if(combat_installed)api->activate();
    if(convenience_installed)split_activate();
    log_event("combat_installed",combat_installed?1:0);
    log_event("convenience_installed",convenience_installed?1:0);
    log_event("split_modules_installed",split_module_abi,(combat_installed?1u:0u)|(convenience_installed?2u:0u),0);
    return combat_installed || convenience_installed;
}
#endif
#ifdef SPLIT_COMBAT
static bool __cdecl combat_module_prepare(Obj host) { COST_SCOPE("combat_module_prepare");
#ifndef SPLIT_MODULE_FIXTURE
    if(host!=reinterpret_cast<Obj>(GetModuleHandleW(nullptr)))return false;
#endif
    image=host;initialize_log_settings();
    // The host has finished its unpacking wait. Reuse the identical strict/compat
    // predicate here, with a second 250 ms sample for mismatched text (ABI stays 1).
    CodeReadiness readiness={};
    if(!code_unpacked_ready(readiness)) {
        Sleep(250);if(!code_unpacked_ready(readiness))return false;
    }
    crash_image_size=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+reinterpret_cast<IMAGE_DOS_HEADER *>(image)->e_lfanew)->OptionalHeader.SizeOfImage;
    auto dll=reinterpret_cast<Obj>(self_module);
    crash_module_size=reinterpret_cast<IMAGE_NT_HEADERS64 *>(dll+reinterpret_cast<IMAGE_DOS_HEADER *>(dll)->e_lfanew)->OptionalHeader.SizeOfImage;
#ifndef NO_RUNTIME_LOG
    if(diagnostic_logging)crash_prepare_unwind();
#endif
    if(diagnostic_logging)AddVectoredExceptionHandler(0,observe_native_exception);
    return split_prepare();
}
static void __cdecl combat_departure_scope(int delta) { COST_SCOPE("combat_departure_scope");
    if(delta>0)++sortie_auto_departure_scope;
    else if(delta<0 && sortie_auto_departure_scope)--sortie_auto_departure_scope;
}
#ifndef NO_RUNTIME_LOG // diagnostic builds only; release export list stays fixed
// Optional diagnostic export (not part of SplitCombatApi): size 0 enables labels; otherwise copies and
// clears this thread's labels. Called by ai_trace on the AI thread that ran the traced node.
extern "C" __declspec(dllexport) unsigned __cdecl SAN14CombatTraceStep(char *out,unsigned size) { COST_SCOPE("SAN14CombatTraceStep");
    if(!size) {InterlockedExchange(&mod_step_wanted,1);return 0;}
    unsigned n=mod_step_length<size?mod_step_length:size-1;
    if(out) {memcpy(out,mod_step_text,n);out[n]=0;}
    mod_step_length=0;mod_step_text[0]=0;return n;
}
#endif
extern "C" __declspec(dllexport) const SplitCombatApi *__cdecl SAN14CombatGetApi(unsigned abi) { COST_SCOPE("SAN14CombatGetApi");
    static const SplitCombatApi api={sizeof(SplitCombatApi),split_module_abi,combat_module_prepare,
        split_publish,split_rollback,split_activate,combat_departure_scope};
    return abi==split_module_abi?&api:nullptr;
}
#else
static DWORD WINAPI initialize(LPVOID)
{
    initialize_log_settings();
    image = reinterpret_cast<Obj>(GetModuleHandleW(nullptr));
    wchar_t file[MAX_PATH]; GetModuleFileNameW(nullptr, file, MAX_PATH);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER *>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    auto pe = reinterpret_cast<IMAGE_NT_HEADERS64 *>(image + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) { log_event("unsupported_executable"); return 0; }
    if (ga::find_build(pe->FileHeader.TimeDateStamp) < 0) log_event("addr_unknown_build", pe->FileHeader.TimeDateStamp);   // signature resolver path (user 2026-11-05)
    crash_image_size=pe->OptionalHeader.SizeOfImage;
    auto module_image=reinterpret_cast<Obj>(self_module);
    auto module_dos=reinterpret_cast<IMAGE_DOS_HEADER *>(module_image);
    auto module_pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(module_image+module_dos->e_lfanew);
    crash_module_size=module_pe->OptionalHeader.SizeOfImage;
    for (unsigned attempt = 0; attempt < 1200; ++attempt) {
        bool ready=code_unpacked_ready(code_readiness);
        if (ready) {
#ifndef NO_RUNTIME_LOG
            if(diagnostic_logging)crash_prepare_unwind();
#endif
            if(diagnostic_logging && AddVectoredExceptionHandler(0,observe_native_exception))log_event("exception_diagnostic_ready");
            bool installed=install();
#ifdef AWARD_WAREHOUSE
            if(installed)log_event("award_warehouse_hooks_installed",10,6,0);
#endif
#ifdef INJURY_PROTECTION
            if(installed)log_event("injury_protection_hooks_installed",14,41,0);
#endif
#ifdef SORTIE_FORMATION
            if(installed)log_event("cohesive_sortie_hooks_installed",40000,5,8);
#endif
#ifdef RECRUITMENT_UI_CANDIDATE
            log_event(installed ? "merit_recruitment_ui_candidate_hooks_installed" : "installation_rejected");
#else
            log_event(installed ? "campaign_v7_domestic_hooks_installed" : "installation_rejected");
#endif
            return 0;
        }
        Sleep(250);
    }
    log_event("native_code_timeout");
    return 0;
}
#endif
#ifdef SPLIT_BASE
extern "C" __declspec(dllexport) bool __cdecl SAN14ConvenienceInitialize(unsigned abi,const SplitCombatApi *api) { COST_SCOPE("SAN14ConvenienceInitialize");
    if(abi!=split_module_abi || !api || api->size!=sizeof(SplitCombatApi) || api->abi!=split_module_abi ||
       !api->prepare || !api->publish || !api->rollback || !api->activate || !api->departure_scope)return false;
    static volatile LONG started=0;
    if(InterlockedCompareExchange(&started,1,0)!=0)return false;
    split_combat_api=api;
    initialize(nullptr);
    return true;
}
#endif
#ifdef SPLIT_MODULE_FIXTURE
static bool __cdecl split_fixture_prepare(Obj host,const SplitCombatApi *api) {
#ifdef SPLIT_COMBAT
    (void)api;return combat_module_prepare(host);
#else
    image=host;split_combat_api=api;
    CodeReadiness readiness={};
    if(!code_unpacked_ready(readiness)) {Sleep(250);if(!code_unpacked_ready(readiness))return false;}
    return split_prepare();
#endif
}
// Each root exposes only what it compiles (W5): the fixture reads the sortie scope from the combat root and the personnel departure wrapper
// from the base root; the other root's entry is never called.
static unsigned __cdecl split_fixture_scope() {
#ifdef SPLIT_COMBAT
    return sortie_auto_departure_scope;
#else
    return 0;
#endif
}
static uint64_t __cdecl split_fixture_departure() {
#ifdef SPLIT_BASE
    return personnel_departure(nullptr,nullptr,nullptr);
#else
    return 0;
#endif
}
static void __cdecl split_fixture_failure(unsigned step) {split_fixture_fail_after=step;}
extern "C" __declspec(dllexport) bool __cdecl SAN14SplitFixtureInstall(Obj host,const SplitCombatApi *api) {
#ifdef SPLIT_BASE
    image=host;split_combat_api=api;
    CodeReadiness readiness={};
    if(!code_unpacked_ready(readiness)) {Sleep(250);if(!code_unpacked_ready(readiness))return false;}
    return install();
#else
    (void)host;(void)api;return false;
#endif
}
#ifdef SPLIT_BASE
extern "C" __declspec(dllexport) void *__cdecl SAN14SharedControlsFixture(unsigned action,Obj object,void *next) {
    if(action==0)return reinterpret_cast<void *>(&unified_dialogs::controls);
    (void)next;unified_dialogs::controls(object);return nullptr;
}
#endif
extern "C" __declspec(dllexport) void __cdecl SAN14SplitFixtureDisable(unsigned mask) {split_fixture_disabled=mask;}
extern "C" __declspec(dllexport) unsigned __cdecl SAN14SplitFixtureFeatures() {
    return (mod_features.merit?2u:0u)|(mod_features.domestic?4u:0u)|
        (mod_features.population?8u:0u)|(mod_features.battle_rules?16u:0u)|(mod_features.advisor_briefing?32u:0u)|
        (mod_features.recruit?64u:0u)|(mod_features.injury?128u:0u)|(mod_features.loyalty?256u:0u)|(mod_features.awards?512u:0u)|
        (mod_features.recruit_ai?1024u:0u);   // local3: optional features whose own publication failure is tested separately
}
extern "C" __declspec(dllexport) const SplitFixtureApi *__cdecl SAN14SplitFixtureGetApi() {
    static const SplitFixtureApi api={split_fixture_prepare,split_publish,split_rollback,split_activate,
        split_fixture_scope,split_fixture_departure,split_fixture_failure};return &api;
}
#endif
extern "C" __declspec(dllexport) void AIInitialize() { COST_SCOPE("AIInitialize"); /* import anchor */ }
#ifndef TACTICS_FIXTURE
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        self_module = module;
#ifdef RUNTIME_COST_PROFILE
#ifdef NO_RUNTIME_LOG
        costprofile::initialize(self_module);
#else
        if(dev_group() && mod_setting(L"diagnostics",L"cost_profile",0))costprofile::initialize(self_module);
#endif
#endif
#ifndef NO_RUNTIME_LOG
        GetModuleFileNameW(self_module, log_path, MAX_PATH);
        wchar_t *separator = wcsrchr(log_path, L'\\');
        if (separator) wcscpy_s(separator + 1, MAX_PATH - static_cast<size_t>(separator + 1 - log_path),
#ifdef SPLIT_COMBAT
            L"SAN14_CombatAI.log");
#else
            L"SAN14_AITactics.log");
#endif
        WIN32_FILE_ATTRIBUTE_DATA log_info{};
        if (separator && GetFileAttributesExW(log_path, GetFileExInfoStandard, &log_info) &&
            (log_info.nFileSizeHigh || log_info.nFileSizeLow >= 16u * 1024u * 1024u)) {
            wchar_t old_path[MAX_PATH];
            if (wcscpy_s(old_path, log_path) == 0) {
                wchar_t *extension = wcsrchr(old_path, L'.');
                if (extension && wcscpy_s(extension, MAX_PATH - static_cast<size_t>(extension - old_path), L".old.log") == 0)
                    MoveFileExW(log_path, old_path, MOVEFILE_REPLACE_EXISTING);
            }
        }
#endif
    }
    return TRUE;
}
#endif

