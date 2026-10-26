#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>
#include <intrin.h>
#include "tactics_policy.h"
#include "native_profile.h"
#include "obstacle_policy.h"
#include "sortie_policy.h"
#include "campaign_policy.h"
#include "native_cost_profile.h"
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
static SRWLOCK policy_lock = SRWLOCK_INIT;
static __declspec(thread) int in_cut;
static __declspec(thread) int in_policy;
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
static void load_diagnostic_settings(const wchar_t *config) {
    InterlockedExchange(&diagnostic_logging,GetPrivateProfileIntW(L"diagnostics",L"enabled",0,config)!=0?1:0);
}
static void initialize_log_settings() {
    InterlockedExchange(&diagnostic_logging,dev_group() && mod_setting(L"diagnostics",L"enabled",0,L"diagnostics.ini")!=0?1:0);
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

#ifdef TACTICS_FIXTURE
static void *fixture_function(unsigned rva);
template<class T> static T fn(unsigned rva) { return reinterpret_cast<T>(fixture_function(rva)); }
#else
template<class T> static T fn(unsigned rva) { return reinterpret_cast<T>(image + rva); }
#endif
static unsigned short word(Obj p, unsigned off) { return *reinterpret_cast<unsigned short *>(p + off); }
static unsigned dword(Obj p, unsigned off) { return *reinterpret_cast<unsigned *>(p + off); }
static Obj pointer(Obj p, unsigned off) { return *reinterpret_cast<Obj *>(p + off); }
static Obj world() { return pointer(image, 0x277DF18); }
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
    bool recruit_ai = true, awards_ai = true, injury_herbal = true;          // [personnel] v12.8 split
    bool domestic = true, population = true, region_officers = true;         // [domestic]
    bool domestic_free = true, region_officer_traits = true;                 // [domestic] v12.8 split (dependent)
    bool domestic_direct = false;
    bool ai_trace = false, trace_original = false;                          // [diagnostics] native AI trace (v12.12)                                            // [domestic] v12.10: ruler's 1st corps too (dependent, default off)
    bool battle_rules = true;                                               // [battle_rules]
    bool unified = true, loyalty = true;                                    // [officer_ui]
    bool building_destruction_all = false;                                 // [stratagem], gameplay tab
    bool diplomacy_overinvest = false, diplomacy_counterturn = false;       // [diplomacy], incoming computer negotiations
    bool damage_preview = true;                                            // [officer_ui], convenience only
    bool traits9 = true, tooltip_collab = true, stats200 = true, battle_deaths = true, scenery_skip = true; // [extensions]
    bool captive_dialog = true, stats200_growth = true;                     // [extensions] v12.8 split
    bool speed_buttons = true, occupy_fast = true, fast_save = true;        // [game_flow]
    bool caravan_stay = false, fast_menu = false;                           // [game_flow] 2026-10-23
    bool advisor_briefing = false;                                         // [game_flow], one-cycle adviser inbox
    bool appear_batch = false, area_index = false, path_fast = false, face_keep = false, face_prefetch = false;    // [performance] 2026-10-25
    bool stat_exp = false, dispatch_days = false, mediation_plus = false, drag_learn = false, lord_change = false; // [officer_ui] 2026-10-23
    bool distance = true, name_lookup = true, layout_keep = true, timer_1ms = false; // [performance]
    bool ai_wall_block = true;                                              // [combat] (combat module)
    bool district_intercept_block = true;                                   // [combat], gameplay tab
    bool combat_cut = true, hit_response = true, obstacle = true, recall_veto = true; // [combat] v12.8 leaf gates
    bool sortie_formation = true, departures = true, group_advance = true, stationary_retreat = true, morale = true, support = true;
    // [combat] mission_v2 (2026-10-21, default off until verified): siege-ground construction only, no
    // build wait, mod field steering yields to a native order on the mission goal.
    bool mission_v2 = false;
    // [combat] ai_max_aggression (2026-10-21): behaviour-tree "AI setting stage" reads as 궁극(4)
    // and the corps progress check loses its stage-based turn skipping (see bt_condition).
    bool ai_max_aggression = false;
    // attack_recheck_fix (2026-10-22, under mission_v2): original tree-1 post-departure recheck bug (#10114/#10115).
    bool attack_recheck_fix = false;
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
    f.awards = on && feature_key(config, L"personnel", L"awards", true);
    f.recruit = on && feature_key(config, L"personnel", L"recruit", true);
    f.injury = on && feature_key(config, L"personnel", L"injury", true);
    // v12.8: keys absent from older files inherit the switch they were split from.
    f.recruit_ai = on && feature_key(config, L"personnel", L"recruit_ai", f.recruit);
    f.awards_ai = f.awards && feature_key(config, L"personnel", L"awards_ai", true);   // needs the warehouse
    f.injury_herbal = on && feature_key(config, L"personnel", L"injury_herbal", f.injury);
    f.domestic = on && feature_key(config, L"domestic", L"domestic", true);
    f.population = on && feature_key(config, L"domestic", L"population", true);
    f.region_officers = on && feature_key(config, L"domestic", L"region_officers", true);
    f.domestic_free = f.domestic && feature_key(config, L"domestic", L"domestic_free", true);
    f.domestic_direct = f.domestic && feature_key(config, L"domestic", L"domestic_direct", false);
    f.region_officer_traits = f.region_officers && feature_key(config, L"domestic", L"region_officer_traits", true);
    f.battle_rules = on && feature_key(config, L"battle_rules", L"battle_rules", true);
    f.unified = on && feature_key(config, L"officer_ui", L"unified", true);
    f.loyalty = on && feature_key(config, L"officer_ui", L"loyalty", true);
    f.building_destruction_all = on && feature_key(config, L"stratagem", L"building_destruction_all", false);
    f.diplomacy_overinvest = on && feature_key(config, L"diplomacy", L"diplomacy_overinvest", false);
    f.diplomacy_counterturn = on && feature_key(config, L"diplomacy", L"diplomacy_counterturn", false);
    f.damage_preview = on && feature_key(config, L"officer_ui", L"damage_preview", true);
    f.traits9 = on && feature_key(config, L"extensions", L"traits9", true);
    f.captive_dialog = on && feature_key(config, L"extensions", L"captive_dialog", f.traits9);
    f.tooltip_collab = on && feature_key(config, L"extensions", L"tooltip_collab", true);
    f.stats200 = on && feature_key(config, L"extensions", L"stats200", true);
    f.stats200_growth = f.stats200 && feature_key(config, L"extensions", L"stats200_growth", true);
    f.battle_deaths = on && feature_key(config, L"extensions", L"battle_deaths", true);
    f.scenery_skip = on && feature_key(config, L"extensions", L"scenery_skip", true);
    f.speed_buttons = on && feature_key(config, L"game_flow", L"speed_buttons", true);
    f.occupy_fast = on && feature_key(config, L"game_flow", L"occupy_fast", true);
    f.fast_save = on && feature_key(config, L"game_flow", L"fast_save", true);
    f.caravan_stay = on && feature_key(config, L"game_flow", L"caravan_stay", false);
    f.advisor_briefing = on && feature_key(config, L"game_flow", L"advisor_briefing", false);
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
    f.distance = fast && feature_key(config, L"performance", L"distance", true);
    f.name_lookup = fast && feature_key(config, L"performance", L"name_lookup", true);
    f.layout_keep = fast && feature_key(config, L"performance", L"layout_keep", true);
    f.timer_1ms = fast && feature_key(config, L"performance", L"timer_1ms", false);   // measured: no effect on layout loads
    f.ai_wall_block = feature_key(config, L"combat", L"ai_wall_block", true);
    f.district_intercept_block = feature_key(config, L"combat", L"district_intercept_block", true);
    f.combat_cut = feature_key(config, L"combat", L"combat_cut", true);
    f.hit_response = feature_key(config, L"combat", L"hit_response", true);
    f.obstacle = feature_key(config, L"combat", L"obstacle", true);
    f.recall_veto = feature_key(config, L"combat", L"recall_veto", true);
    f.sortie_formation = feature_key(config, L"combat", L"sortie_formation", true);
    f.departures = feature_key(config, L"combat", L"departures", true);
    f.group_advance = feature_key(config, L"combat", L"group_advance", true);
    f.stationary_retreat = feature_key(config, L"combat", L"stationary_retreat", true);
    f.morale = feature_key(config, L"combat", L"morale", true);
    f.support = feature_key(config, L"combat", L"support", true);
    f.mission_v2 = feature_key(config, L"combat", L"mission_v2", false);
    f.ai_max_aggression = feature_key(config, L"combat", L"ai_max_aggression", false);
    f.attack_recheck_fix = f.mission_v2;   // no own key: part of mission_v2 (installer catalog unchanged)
    // mission_v2 excludes the mod's own departures, group march and escorts (installer ExcludedBy).
    if (f.mission_v2) { f.departures = false; f.group_advance = false; f.support = false; }
}
static int game_day();
#ifdef SORTIE_FORMATION
static bool sortie_controlled(Obj army);
#endif
#ifdef AWARD_WAREHOUSE
static void award_ai_city(Obj city);
static void award_synchronize();
#endif

// Decision labels for ai_trace: the convenience module reads them after each traced node call on the
// same AI thread (SAN14CombatTraceStep). Off until the trace asks for them.
static volatile LONG mod_step_wanted;
static __declspec(thread) char mod_step_text[160];
static __declspec(thread) unsigned mod_step_length;
static bool mod_step(const char *name) {
    if(!mod_step_wanted || !name)return true;
    size_t n=strlen(name);
    if(mod_step_length+n+2>=sizeof(mod_step_text))return true;
    if(mod_step_length)mod_step_text[mod_step_length++]=';';
    memcpy(mod_step_text+mod_step_length,name,n);mod_step_length+=static_cast<unsigned>(n);mod_step_text[mod_step_length]=0;
    return true;
}
#ifndef NO_RUNTIME_LOG
static volatile LONG log_day = -1;
static void write_log_event(const char *kind, unsigned army = 0, unsigned value = 0, unsigned hex = 0)
{
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

static bool valid(Obj obj)
{
    return obj != nullptr && fn<int(__fastcall *)(Obj)>(0x32F500)(obj) != 0;
}
static Obj table(unsigned offset, unsigned id, unsigned maximum)
{
    Obj root = world();
    return root && id <= maximum ? pointer(root, offset + id * 8) : nullptr;
}
static unsigned army_force(Obj army)
{
    return fn<unsigned(__fastcall *)(Obj)>(0x22CC40)(army);
}
// Ruler inner trait 건설 (Person+180 s16; UI 1..5 = -20,-10,0,10,20, column getter 344D10/356260).
// A ruler at 5 (20) makes its armies fill their own regions' facility capacity (2026-10-21).
static bool builder_force(unsigned force)
{
    Obj f=force && force<=51?table(0xDF30,force,51):nullptr;
    Obj ruler=valid(f)?table(0x158,word(f,0x10),6000):nullptr;
    return valid(ruler) && *reinterpret_cast<short *>(ruler+0x180)>=20;
}
static unsigned army_id(Obj army)
{
    /* 22DA10 computes (army - root->Army[0]) / 512. Army+10 is
     * a state field, shared by many active units, never their identity. */
    unsigned id = fn<unsigned(__fastcall *)(Obj)>(0x22DA10)(army);
    return id > 0 && id <= 500 && table(0x7E838, id, 500) == army ? id : 0;
}
static bool belongs_to_troops(Obj army, Obj troops)
{
    /* Resolve the native Troops ID stored in Army+58 to its object.
     * Troops+10 is also a state field, so compare object identity. */
    return troops != nullptr && table(0x7F8D8, word(army, 0x58), 500) == troops;
}
static unsigned player_force()
{
    Obj root = world();
    if (!root) return 0;
    Obj game_context = pointer(root, 0x871E8);
    if (!game_context) return 0;
    Obj force = fn<Obj(__fastcall *)(Obj)>(0x32E910)(game_context);
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
static bool computer_army(Obj army)
{
    if (!valid(army) || army[0x140]) return false;
    unsigned player = player_force();
    unsigned force = army_force(army);
    return force > 0 && force < 52 && force != player && (player != 0 || observer_mode());
}

static bool campaign_controlled(Obj army) {
    if(computer_army(army))return true;
#ifdef SORTIE_FORMATION
    return sortie_controlled(army);
#else
    return false;
#endif
}

struct Link { Obj value; Link *next; };
struct GameList { Obj vtable; unsigned *handle; };
static void manager_lock()
{
    if (dword(image, 0x27D10B0)) EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(image + 0x27D10B8));
}
static void manager_unlock()
{
    if (dword(image, 0x27D10B0)) LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(image + 0x27D10B8));
}
static void list_init(GameList &list, unsigned vtable = 0x1343670)
{
    list.vtable = image + vtable; list.handle = nullptr;
    fn<void(__fastcall *)(GameList *)>(0x21E2B0)(&list);
}
static void list_free(GameList &list) { fn<void(__fastcall *)(GameList *)>(0x424D60)(&list); }
static bool list_add(GameList &list, Obj obj)
{
    if (!list.handle) return false;
    manager_lock();
    Obj entry = fn<Obj(__fastcall *)(Obj, unsigned, unsigned char)>(0x18DF0)(image + 0x27D10E0, *list.handle, 1);
    if (entry) *reinterpret_cast<Obj *>(entry) = obj;
    manager_unlock();
    return entry != nullptr;
}
static Link *list_first(GameList &list)
{
    if (!list.handle || !pointer(image, 0x27D10E8) || *list.handle >= dword(image, 0x27D1120)) return nullptr;
    Obj heads = pointer(image, 0x27D10F0);
    return heads ? *reinterpret_cast<Link **>(heads + *list.handle * 8) : nullptr;
}

/* This wrapper is installed at the one CALL inside D06D0. Restricting its
 * candidate list before the native predicate prevents choosing an AI victim
 * and avoids calling the predicate twice after it temporarily changes owners. */
static int __fastcall cut_player_predicate(unsigned owner, GameList *tiles, GameList *enemies, Obj *target)
{
    auto original = fn<int(__fastcall *)(unsigned, GameList *, GameList *, Obj *)>(0xC0C80);
    if (!in_cut) return original(owner, tiles, enemies, target);
    unsigned player = player_force();
    if (!player || owner == player) return 0;
    GameList filtered; list_init(filtered, 0x13423D0);
    bool have = false;
    for (Link *item = list_first(*enemies); item; item = item->next) {
        if (valid(item->value) && army_force(item->value) == player) {
            have = list_add(filtered, item->value) || have;
        }
    }
    int result = have ? original(owner, tiles, &filtered, target) : 0;
    list_free(filtered);
    return result;
}

static bool campaign_castle_locked(Obj army);
static bool try_cut(Obj army)
{
    if (!mod_features.combat_cut || !computer_army(army) || campaign_castle_locked(army)) return false;
    unsigned player = player_force();
    if (!player) return false; // No player logistics target exists in demo.
    bool have_enemy = false;
    unsigned own = army_force(army);
    if (!fn<int(__fastcall *)(unsigned, unsigned)>(0x2CA280)(player, own)) return false;
    for (unsigned i = 1; i <= 500; ++i) {
        Obj enemy = table(0x7E838, i, 500);
        if (valid(enemy) && army_force(enemy) == player) { have_enemy = true; break; }
    }
    if (!have_enemy) return false;
    GameList route; list_init(route);
    Obj target = nullptr;
    ++in_cut;
    int success = fn<int(__fastcall *)(Obj, GameList *, Obj *)>(0xD06D0)(army, &route, &target);
    --in_cut;
    list_free(route);
    if (!success || !valid(target) || army_force(target) != player) return false;
    *reinterpret_cast<unsigned *>(army + 0x1F0) |= 8;
    *reinterpret_cast<unsigned *>(army + 0x1F4) = fn<unsigned(__fastcall *)(Obj)>(0x22DA10)(target);
    log_event("cut_player", army_id(army), army_id(target), word(army, 0x2A));
    return true;
}

struct Progress {
    ObstacleHistory obstacle;
    Obj army, root;
    unsigned short commander, previous_center, previous_hex, construction_hex;
    unsigned char force, previous_owner, facility;
    bool credit;
    unsigned short city, region_target, staging_hex;
    unsigned char region_owner;
    bool campaign, built_once, finished_build, did_demolition, finished_demolition;
    int return_deadline_day;
    unsigned short build_center, destroy_id, destroy_hex, cut_commander;
    unsigned char destroy_type, destroy_owner;
    unsigned char destroy_reason;
    unsigned short destroy_goal;
    int destroy_progress;
    unsigned cycle_mask;
    bool build_chain;
    int observed_day, progress_day, destroy_day, cut_day;
    unsigned short cut_target;
    unsigned short cut_route[5], cut_goal;
    unsigned char cut_action, cut_kind;
    unsigned short motion_hex, recovery_hex;
    int motion_day, region_blocked_day;
    bool returning;
    bool forced_return;
    unsigned short guard_beneficiary, guard_goal, guard_hex;
    unsigned short combat_target, combat_commander;
    int combat_day;
    unsigned short combat_city,companion_target,companion_commander,companion_parent,home_defense_city;
    int build_audit_day;
    unsigned build_audit_mask;
    int build_retry_day;
    bool morale_recovering, morale_saved, morale_building;
    bool morale_build_credit,morale_build_chain;
    unsigned short morale_build_center;
    unsigned morale_build_cycle;
    unsigned char morale_order[0x34];
    unsigned morale_flags;
    unsigned short morale_band, morale_hex, morale_group_mode, morale_group_goal, siege_city;
    unsigned short guard_region, guard_city, defensive_hex;
    bool guard_requested;
    int guard_day;
    int fire_policy_turn;
    bool fire_avoid;
    unsigned short siege_capture_city, siege_capture_hex;
    unsigned short operation_leader,operation_city;
    int operation_retry_day;
    bool regional_worker;
    unsigned short work_source,work_reviewed_center,near_capture_hex;
    unsigned char near_capture_owner;
    int near_capture_day;
    unsigned short forced_siege_city;
    unsigned char forced_siege_reason;
    // Set only by the paid dispatcher for a fresh departure (see sortie_dispatch).
    bool dispatched;
    int dispatch_day;
    // Consecutive days on which every build site was rejected (see try_build).
    unsigned char build_rejections;
    int build_reject_day;
};
static Progress progress[501];
static __declspec(thread) unsigned campaign_group_march_depth;
static bool campaign_castle_locked(Obj army) {
    unsigned id=valid(army)?army_id(army):0;if(!id || id>500 || army[0x140])return false;
    const Progress &p=progress[id];
    return p.army==army && p.root==world() && p.commander==word(army,0x12) && p.force==army_force(army) &&
        p.forced_siege_city && (p.forced_siege_reason==1 || (p.forced_siege_reason==3 && !campaign_group_march_depth)) && !p.forced_return && !p.returning;
}
static bool campaign_forced_siege_priority(Obj army,Progress &p);
static bool campaign_operation_member(Obj army,const Progress &p);
#ifdef SORTIE_FORMATION
static bool campaign_group_marching(Obj army);
static bool campaign_group_member(Obj army);
#else
static bool campaign_group_marching(Obj) {return false;}
static bool campaign_group_member(Obj) {return false;}
#endif
static Obj army_area(Obj army)
{
    Obj tile = table(0xE270, word(army, 0x2A), 48399);
    return valid(tile) ? fn<Obj(__fastcall *)(Obj)>(0x229EA0)(tile) : nullptr;
}
static int distance(unsigned a, unsigned b) { return fn<int(__fastcall *)(int, int)>(0x2AA850)(static_cast<int>(a), static_cast<int>(b)); }
static Progress *observe_capture(Obj army)
{
    unsigned id = army_id(army);
    if (!id || id > 500) return nullptr;
    Progress &p = progress[id];
    unsigned owner = army_force(army);
    Obj root = world();
    int day=game_day();
    if (p.army != army || p.root != root || p.commander != word(army, 0x12) || p.force != owner || day<p.observed_day) {
        memset(&p, 0, sizeof(p)); p.army = army; p.root = root;
        p.commander = word(army, 0x12); p.force = static_cast<unsigned char>(owner);
        p.previous_center = 48400; p.construction_hex = 48400;
        p.region_target = 48400; p.staging_hex = 48400;
        p.build_center=48400;p.destroy_hex=48400;
        p.work_reviewed_center=48400;
        p.near_capture_hex=48400;
        p.progress_day=day;
        p.motion_hex=word(army,0x2A);p.motion_day=day;
        p.recovery_hex=48400;p.region_blocked_day=-1;
        p.build_audit_day=day-1;
        p.build_retry_day=day-1;
        p.cut_day=day-1;
        p.guard_goal=48400;p.guard_hex=48400;p.combat_day=-1;
        p.fire_policy_turn=-1;p.defensive_hex=48400;
        /* Resume saved construction from native fields; capture credit itself
         * is observed after loading, not guessed from an already-owned region. */
        if (army[0x37] == 8 || army[0x37] == 9) {
            p.facility = army[0x46]; p.construction_hex = word(army, 0x42);
            p.built_once = p.facility!=0;
            p.build_chain=p.facility!=0;
        }
    }
    // Native group orders can clear action/target before our first callback,
    // but retain the builder's facility and construction hex. Reconcile those
    // fields with a nearby, owned, unfinished building before counting readiness.
    if(army[0x46]>=3 && army[0x46]<=14 && word(army,0x42)<48400) {
        bool unfinished=army[0x37]==8;
        Obj site=table(0xE270,word(army,0x42),48399);
        if(valid(site) && word(site,0x1A)>0 && word(site,0x1A)<=3000) {
            unsigned building_id=word(site,0x1A);
            Obj b=table(0x6DA98,building_id,3000);
            if(valid(b) && b[0x13]==owner && b[0x12]==army[0x46] && word(b,0x10)==word(army,0x42)) {
                int max=fn<int(__fastcall *)(Obj)>(0x2AC120)(b);
                bool assigned=army[0x37]==9 && army[0x38]==11 && word(army,0x3A)==building_id;
                unfinished=max>0 && word(b,0x14)>0 && word(b,0x14)<max &&
                           (assigned || distance(word(army,0x2A),word(army,0x42))<=2);
            }
        }
        if(unfinished) {
            if(army[0x37]!=8 && army[0x37]!=9 && !p.facility)
                log_event("build_recovered_after_native_reset",id,army[0x46],word(army,0x42));
            p.facility=army[0x46];p.construction_hex=word(army,0x42);p.built_once=true;
            p.build_chain=true;p.finished_build=false;
        }
    }
    if (p.previous_center < 48400 && p.previous_owner != owner) {
        Obj previous = table(0xE270, p.previous_center, 48399);
        // Build policy v2 (2026-10-21): a captured region no longer obliges a field army to build.
        if ((p.regional_worker || !mod_features.mission_v2) && valid(previous) && previous[0x14] == owner && distance(p.previous_hex, p.previous_center) < 6) {
            p.credit = true;
            p.build_chain=true;p.build_center=p.previous_center;p.cycle_mask=0;p.progress_day=day;
            log_event("capture_credit", id, owner, p.previous_center);
        }
    }
    // A selected center can be captured between two AI observations without
    // the previous position having been inside its region. Track that mission
    // explicitly as well as the original vicinity observation.
    if((p.regional_worker || (!mod_features.mission_v2 && p.campaign)) && p.region_target<48400 && p.region_owner!=owner) {
        Obj center=table(0xE270,p.region_target,48399);
        if(valid(center) && center[0x14]==owner && distance(word(army,0x2A),p.region_target)<6) {
            if(!p.credit)log_event("capture_credit",id,owner,p.region_target);
            p.credit=true;p.region_owner=static_cast<unsigned char>(owner);
            p.build_chain=true;p.build_center=p.region_target;p.cycle_mask=0;p.progress_day=day;
        }
    }
    if(p.near_capture_hex<48400 && p.near_capture_owner!=owner) {
        Obj center=table(0xE270,p.near_capture_hex,48399);
        if(valid(center) && center[0x14]==owner && distance(word(army,0x2A),p.near_capture_hex)<6) {
            p.near_capture_owner=static_cast<unsigned char>(owner);
            Obj captured=table(0x6CAF0,word(center,0x12),500);
            if((p.regional_worker || !mod_features.mission_v2) && valid(captured) && !fn<int(__fastcall *)(Obj)>(0x232680)(captured) && !fn<int(__fastcall *)(Obj)>(0x232BF0)(captured)) {
                p.credit=true;p.build_chain=true;p.build_center=p.near_capture_hex;p.cycle_mask=0;p.progress_day=day;
            }
        }
    }
    Obj area = army_area(army);
    if (valid(area)) {
        p.previous_center = word(area, 0x3A);
        Obj center = table(0xE270, p.previous_center, 48399);
        p.previous_owner = valid(center) ? center[0x14] : 0;
        p.previous_hex = word(army, 0x2A);
        if(p.build_chain && p.build_center>=48400)p.build_center=p.previous_center;
    }
    p.observed_day=day;
    if(p.motion_hex!=word(army,0x2A)) {p.motion_hex=word(army,0x2A);p.motion_day=day;}
    return &p;
}
static bool reserved_hex(unsigned hex, Obj actor)
{
    for (unsigned i = 1; i <= 500; ++i) {
        Obj other = table(0x7E838, i, 500);
        if (!valid(other)) continue;
        if (word(other, 0x2A) == hex) return true;
        if (other != actor && (other[0x37] == 8 || other[0x37] == 9) && word(other, 0x42) == hex) return true;
    }
    return false;
}
static bool campaign_build_order(Obj army,unsigned facility,unsigned hex);
static bool set_build(Obj army, unsigned facility, unsigned hex)
{
    if(campaign_castle_locked(army))return false;
    if (army[0x140] || !facility || facility >= 31 || hex >= 48400) return false;
    if (computer_army(army) && (facility == 9 || facility == 10)) return false;
    return campaign_build_order(army,facility,hex);
}

static bool continue_build(Obj army, Progress &p)
{
    COST_SCOPE("continue_build");
    if(campaign_castle_locked(army))return false;
    if (!p.facility || p.construction_hex >= 48400) return false;
    Obj tile = table(0xE270, p.construction_hex, 48399);
    if (!valid(tile)) { p.facility = 0; return false; }
    unsigned building_id = word(tile, 0x1A);
    if (building_id > 0 && building_id <= 3000) {
        Obj building = fn<Obj(__fastcall *)(Obj)>(0x22FB10)(tile);
        if (!valid(building) || building[0x13] != army_force(army) || building[0x12] != p.facility) {
            p.facility = 0; return false;
        }
        int maximum = fn<int(__fastcall *)(Obj)>(0x2AC120)(building);
        if (maximum <= 0) { p.facility = 0; return false; }
        if (word(building, 0x14) >= maximum) {
            p.cycle_mask|=1u<<p.facility;
            if(p.facility>=11 && p.facility<=14)p.cycle_mask|=(15u<<11);
            p.facility = 0; p.finished_build = true;
            // Keep the captured base until its native capacity is filled.
            p.build_chain=true;
            Obj context=pointer(world(),0x871E8);
            p.return_deadline_day=fn<int(__fastcall *)(Obj)>(0x32BA50)(context+0x34)+10;
            p.progress_day=game_day();
            log_event("build_finished_resume_campaign", army_id(army), p.city, p.construction_hex);
            return false;
        }
        return set_build(army, p.facility, p.construction_hex);
    }
    Obj person = table(0x158, word(army, 0x12), 6000);
    Obj definition = table(0x73958, p.facility, 30);
    Obj origin = fn<Obj(__fastcall *)(Obj)>(0x22A5A0)(army);
    Obj force = table(0xDF30, army_force(army), 51);
    if (!valid(person) || !valid(definition) || !valid(origin) || !valid(force)) {
        p.facility = 0; return false;
    }
    auto resource = *reinterpret_cast<int(__fastcall **)(Obj)>(pointer(origin, 0) + 0x90);
    int cost = fn<int(__fastcall *)(Obj, Obj)>(0x2AB390)(force, definition);
    if (cost < 0 || resource(origin) < cost) { p.facility = 0; return false; }
    GameList people; list_init(people, 0x13436B0); list_add(people, person);
    int possible = fn<int(__fastcall *)(GameList *, Obj, Obj)>(0x278FA0)(&people, tile, definition);
    list_free(people);
    if (!possible) { p.facility = 0; return false; }
    return set_build(army, p.facility, p.construction_hex);
}

#include "native_obstacles.inl"
#include "native_facilities.inl"
static void build_skip(Obj army, Progress &p, unsigned facility, const char *reason) {
    int day=game_day();
    if(p.build_audit_day!=day) {p.build_audit_day=day;p.build_audit_mask=0;}
    unsigned bit=1u<<facility;if(p.build_audit_mask&bit)return;
    p.build_audit_mask|=bit;log_event(reason,army_id(army),facility,word(army,0x2A));
}
static bool try_build(Obj army, Progress &p, unsigned cohort_center=48400,unsigned preferred=0)
{
    COST_SCOPE("try_build");
    if(campaign_castle_locked(army))return false;
    if (continue_build(army, p)) return true;
    if (!p.credit && !p.build_chain) return false;
    Obj area = army_area(army);
    if (!valid(area) || distance(word(army, 0x2A), word(area, 0x3A)) >= 11) return false;
    if(p.build_center>=48400)p.build_center=word(area,0x3A);
    if(word(area,0x3A)!=p.build_center)return false;
    if(area_full(area)) {
        // Mixed hostile capacity is handled by the existing demolition path.
        // Do not abandon a captured base just because an enemy occupies a slot.
        if(!area_hostile_facilities(army,area)) {
            p.credit=false;p.build_chain=false;p.work_reviewed_center=p.build_center;
            if(p.regional_worker)p.region_target=48400;
        }
        build_skip(army,p,0,"build_skip_area_cap");return false;
    }
    if(game_day()<p.build_retry_day && !preferred)return false;
    Obj person = table(0x158, word(army, 0x12), 6000);
    Obj troops = table(0x7F8D8, word(army, 0x58), 500);
    Obj origin = fn<Obj(__fastcall *)(Obj)>(0x22A5A0)(army);
    if (!valid(person) || !valid(troops) || !valid(origin)) return false;
    Obj district = fn<Obj(__fastcall *)(Obj)>(0x22BAA0)(troops);
    Obj goal = fn<Obj(__fastcall *)(Obj, unsigned)>(0x32C130)(world(), p.operation_city?p.operation_city:word(troops, 0x16));
    Obj goal_force = valid(goal) ? fn<Obj(__fastcall *)(Obj)>(0x22C990)(goal) : nullptr;
    // Home capture groups name their own source city, not an enemy force.
    if(p.regional_worker)goal_force=nullptr;
    Obj force = table(0xDF30, army_force(army), 51);
    if (!valid(district) || !valid(force)) return false;
    auto resource = *reinterpret_cast<int(__fastcall **)(Obj)>(pointer(origin, 0) + 0x90);
    GameList people; list_init(people, 0x13436B0); list_add(people, person);
    static const unsigned order[] = {6, 4, 7, 3, 5, 11, 12, 14, 13};
    bool applied = false;
    unsigned seen=p.cycle_mask|local_facilities(army,area);
    // Each missing type once (user decision 2026-10-21: no filling capacity with repeated types).
    // A builder ruler (inner 건설 5) also repeats useful types until the region is full.
    const bool builder=mod_features.mission_v2 && !p.regional_worker && builder_force(army_force(army));
    bool missing=false;
    for (unsigned pass=0;pass<(builder?2u:1u) && !applied;++pass) for (unsigned facility : order) {
        if(preferred && facility!=preferred)continue;
        if(pass==0 && (seen & (1u<<facility))) {build_skip(army,p,facility,"build_skip_present");continue;}
        if(pass==1 && (preferred || !(seen & (1u<<facility))))continue;
        // Siege ground (non-worker): only the siege facilities camp 3, fort 4, catapult 6, band 7.
        if(mod_features.mission_v2 && !p.regional_worker && !builder && !preferred && facility!=3 && facility!=4 && facility!=6 && facility!=7)continue;
        missing=true;
        Obj definition = table(0x73958, facility, 30);
        if (!valid(definition)) {build_skip(army,p,facility,"build_skip_definition");continue;}
        int cost = fn<int(__fastcall *)(Obj, Obj)>(0x2AB390)(force, definition);
        if (cost < 0 || resource(origin) < cost) {build_skip(army,p,facility,"build_skip_money");continue;}
        GameList candidates; list_init(candidates);
        fn<void(__fastcall *)(Obj, Obj, Obj, GameList *, Obj, Obj, Obj)>(0xB9070)
            (area, person, definition, &candidates, goal_force, district, nullptr);
        // Offensive placement can yield no sites in a peaceful home region.
        // Review local owned land instead; the same native permission, cost,
        // capacity, occupied-site and actual movement checks still apply.
        if(!list_first(candidates)) {
            GameList local;list_init(local);
            Obj center=table(0xE270,p.build_center,48399);
            if(valid(center))fn<void(__fastcall *)(Obj,int,GameList *)>(0x2A7AA0)(center,10,&local);
            for(Link *item=list_first(local);item;item=item->next)if(valid(item->value) &&
                item->value[0x14]==army_force(army) && fn<Obj(__fastcall *)(Obj)>(0x229EA0)(item->value)==area)
                list_add(candidates,item->value);
            list_free(local);
        }
        bool offered=list_first(candidates)!=nullptr,native_rejected=false;
        for (Link *item = list_first(candidates); item; item = item->next) {
            Obj tile = item->value;
            if (!valid(tile) || tile[0x14] != army_force(army)) continue;
            unsigned hex = fn<unsigned(__fastcall *)(Obj)>(0x22DDA0)(tile);
            if (hex >= 48400 || reserved_hex(hex, army)) continue;
            if(cohort_center<48400 && distance(cohort_center,hex)>2)continue;
            if (!fn<int(__fastcall *)(GameList *, Obj, Obj)>(0x278FA0)(&people, tile, definition)) {native_rejected=true;continue;}
            if (set_build(army, facility, hex)) {
                p.facility = static_cast<unsigned char>(facility);
                p.construction_hex = static_cast<unsigned short>(hex); p.credit = false;
                p.built_once = true; p.finished_build = false;
                p.build_chain=true;
                p.build_retry_day=game_day();p.build_rejections=0;
                if(mod_features.mission_v2 && !p.regional_worker && !builder)p.build_chain=false; // siege ground: one facility, then the mission
                log_event("build_started", army_id(army), facility, hex);
                applied = true;
            }
            if (applied) break;
        }
        list_free(candidates);
        if(!applied)build_skip(army,p,facility,!offered?"build_skip_native_candidates":
                              (native_rejected?"build_skip_native_check":"build_skip_no_free_site"));
        if (applied) break;
    }
    list_free(people);
    if(!applied && !missing && !preferred) {
        // Every type already stands here: the capture work is done; resume the mission.
        log_event("build_types_complete",army_id(army),seen,p.build_center);
        p.credit=false;p.build_chain=false;p.build_rejections=0;p.work_reviewed_center=p.build_center;
        if(p.regional_worker)p.region_target=48400;
        return false;
    }
    // Native cost/site rejection is often temporary: retry once ten days later. A second
    // rejection releases the capture obligation; otherwise the hold in apply_priority keeps the
    // army waiting (대기) over native orders until the 40-day stationary retreat (trace 2026-10-20:
    // 328 of 576 stationary retreats).
    if(!applied) {
        int day=game_day();p.build_retry_day=day+10;
        if(!preferred && p.build_reject_day!=day) {
            p.build_reject_day=day;
            if(++p.build_rejections>=2) {
                log_event("build_released_after_rejections",army_id(army),p.build_rejections,p.build_center);
                p.credit=false;p.build_chain=false;p.build_rejections=0;
                p.work_reviewed_center=p.build_center;
                if(p.regional_worker)p.region_target=48400;
            }
        }
    }
    return applied;
}

/* These nodes do not read their own node object. Each resolves the supplied
 * Army and only reports success when the native alternative was selected. */
static bool try_other(Obj army, Obj node, Obj context)
{
    if (army[0x37] != 4 || army[0x38] != 5) return false;
    static const unsigned alternatives[] = { 0x1FFE0, 0x1FF60, 0x1F120, 0x1FEA0, 0x1FF00};
    for (unsigned rva : alternatives) {
        if (fn<Execute>(rva)(node, context, army) == 1) {
            log_event("city_alternative", army_id(army), rva, word(army, 0x2A));
            return true;
        }
    }
    return false;
}
#include "native_campaign.inl"
#include "native_campaign_support.inl"
#include "native_campaign_logistics.inl"
// Shared native eligibility covers both dedicated AI builders and campaign
// construction. Tile ownership alone cannot identify the acting force.
using BuildPermission = bool(__fastcall *)(Obj, Obj, Obj, int);
static BuildPermission original_build_permission = nullptr;
static volatile LONG wall_log_day[52][2];
static unsigned blocked_wall_force(Obj force, Obj definition) {
    if (!definition || (definition != table(0x73958,9,30) && definition != table(0x73958,10,30))) return 0;
    unsigned player = player_force();
    if (!player && !observer_mode()) return 0;
    for (unsigned id = 1; id < 52; ++id)
        if (force && force == table(0xDF30,id,51) && valid(force)) return id != player ? id : 0;
    return 0;
}
static bool __fastcall build_permission(Obj force, Obj tile, Obj definition, int check_policy) {
    unsigned owner = enabled && mod_features.ai_wall_block ? blocked_wall_force(force,definition) : 0;
    if (owner) {
        unsigned facility = definition == table(0x73958,9,30) ? 9 : 10;
        LONG stamp = static_cast<LONG>(game_day()+1);
        if (InterlockedExchange(&wall_log_day[owner][facility-9],stamp) != stamp)
            log_event("ai_wall_build_blocked",0,(owner<<8)|facility,
                      valid(tile) ? fn<unsigned(__fastcall *)(Obj)>(0x22DDA0)(tile) : 48400);
        return false;
    }
    return original_build_permission(force,tile,definition,check_policy);
}
static bool cancel_wall_order(Obj army, Progress &p) {
    if (!computer_army(army) || (army[0x37] != 8 && army[0x37] != 9) ||
        (army[0x46] != 9 && army[0x46] != 10)) return false;
    unsigned facility = army[0x46], hex = word(army,0x42);
    fn<void(__fastcall *)(Obj)>(0xA2410)(army);
    fn<void(__fastcall *)(Obj,int)>(0x2EAAE0)(army,13);
    fn<void(__fastcall *)(Obj,int,int)>(0x2EAB40)(army,255,0);
    army[0x46]=0;*reinterpret_cast<unsigned short *>(army+0x42)=48400;
    p.facility=0;p.construction_hex=48400;p.credit=false;p.build_chain=false;
    p.finished_build=false;
    fn<void(__fastcall *)(Obj)>(0x26C940)(army);
    log_event("ai_wall_order_cancelled",army_id(army),facility,hex);
    return true;
}

#include "native_district_intercept.inl" // corps policy judged at the departure dispatcher
#ifdef SORTIE_FORMATION
#include "native_sortie.inl"
#include "native_campaign_reinforcements.inl"
#include "native_campaign_departures.inl"
#endif
#include "native_campaign_operations.inl"
#include "native_campaign_assault.inl"
static bool apply_priority(Obj army, Obj node, Obj context)
{
    COST_SCOPE("apply_priority");
    if(native_search_depth)return false;
    if (!campaign_controlled(army)) return false;
    Progress *p = observe_capture(army);
    if(p) {
        remember_campaign(army,*p);
        if(campaign_individual_priority(army,*p)) {
#ifdef SORTIE_FORMATION
            SortieGroup g;unsigned id;
            if((p->forced_return || p->combat_target || p->combat_city || p->home_defense_city || p->guard_beneficiary) && sortie_snapshot(army,g,id)) {
                g.active=false;sortie_commit(id,g);
            }
            if(p->forced_return || p->combat_target || p->combat_city) {
                AcquireSRWLockExclusive(&sortie_state_lock);sortie_escorts[army_id(army)]={};ReleaseSRWLockExclusive(&sortie_state_lock);
            }
#endif
            mod_step("ap:individual");return true;
        }
#ifdef SORTIE_FORMATION
        // mission_v2: route regions go into the troops work list instead (mission_route_waypoint);
        // a direct order here was reset by the original 613A0 every turn (232 of 307 in run 3).
        if(!mod_features.mission_v2 && campaign_near_capture(army,*p))return mod_step("ap:near_capture"),true;
        if(sortie_support_step(army) || sortie_step(army))return mod_step("ap:group_step"),true;
#endif
        if(!p->regional_worker && (p->operation_leader || (!p->credit && !p->build_chain && !p->facility)) && campaign_siege_priority(army,*p))return mod_step("ap:siege_priority"),true;
        if(p->operation_leader)return mod_step("ap:operation_hold"),true;
    }
#ifdef SORTIE_FORMATION
    if(sortie_support_step(army) || sortie_step(army))return mod_step("ap:group_step"),true;
#endif
    if(p && p->forced_return)return false;
    if (p) {remember_campaign(army, *p);observe_obstacle_motion(army,*p);}
    if (!(p && p->regional_worker) && try_cut(army)) {
        if(p)remember_cut(army,*p);
        mod_step("ap:cut");return true;
    }
    if(p && restore_cut(army,*p))return mod_step("ap:restore_cut"),true;
    bool cancelled = p && cancel_wall_order(army,*p);
    if(p && cancel_unqualified_neutral_order(army,*p))cancelled=true;
    if(p && !p->regional_worker && recover_idle(army,*p))return mod_step("ap:recover_idle"),true;
    if(word(army,0x16)<1000)return cancelled && mod_step("ap:cancel_order");
    if(p && try_destroy(army,*p,true)) {pause_assembly(*p);return mod_step("ap:destroy_priority"),true;}
    if(p && p->facility && continue_build(army,*p)) {pause_assembly(*p);return mod_step("ap:continue_build"),true;}
    if(p && try_destroy(army,*p)) {pause_assembly(*p);return mod_step("ap:destroy"),true;}
    if(p && (p->campaign || p->regional_worker))prepare_local_build(army,*p);
    if (p && try_build(army, *p)) {pause_assembly(*p);return mod_step("ap:build"),true;}
    // Build policy v2: a blocked build never parks the army (former "build_hold_wait" stood units
    // on open ground for weeks); the mission or the original AI continues instead.
    if(mod_features.mission_v2) {
        if(p && (p->credit || p->build_chain) && !p->facility) {p->credit=false;p->build_chain=false;}
    } else if(p && (p->credit || p->build_chain)) {
        // Former policy: keep the native AI from replacing a temporarily blocked build with retreat.
        if(army[0x37]!=13)log_event("build_hold_wait",army_id(army),army[0x37],word(army,0x2A));
        fn<void(__fastcall *)(Obj)>(0xA2410)(army);
        fn<void(__fastcall *)(Obj,int)>(0x2EAAE0)(army,13);
        fn<void(__fastcall *)(Obj,int,int)>(0x2EAB40)(army,255,0);
        mod_step("ap:build_hold_wait");return true;
    }
    if(p && p->regional_worker) {if(campaign_regional_step(army,*p))return mod_step("ap:regional_step"),true;return cancelled && mod_step("ap:cancel_order");}
    if (p && p->campaign && mod_features.mission_v2 && native_on_mission(army)) return mod_step("ap:native_on_mission"),false;
    if (p && p->campaign && campaign_step(army, *p, node, context)) return mod_step("ap:campaign_step"),true;
    if(try_other(army, node, context))return mod_step("ap:native_alternative"),true;
    return cancelled && mod_step("ap:cancel_order");
}

// [combat] ai_max_aggression. The behaviour-tree condition dispatcher is one function pointer at
// image+1B95CC0 (registry 1B95CB0 +10), set by AI init C0150 to C2610 (switch on the condition op).
// Op 0x73 (AI setting stage == arg, 3E9E60 = game AI setting 0..4) answers as stage 4 (궁극).
// Op 0x74 (corps progress check, A8A00) skips turns when stage < 2 and the force holds under the
// threshold share of bases; with stage 4 it only fails for an invalid corps, so a failure for a valid
// corps is turned into success. All other ops go to the original unchanged.
static bool patch_pointer(unsigned rva, unsigned expected, void *replacement);
using BtCondition=uint64_t(__fastcall *)(unsigned,unsigned char,Obj,Obj,int *,int);
static uint64_t __fastcall bt_condition(unsigned op,unsigned char negate,Obj context,Obj entity,int *args,int count)
{
    if(mod_features.ai_max_aggression && op==0x73 && args)return args[0]==4?1u:0u;
    auto original=fn<BtCondition>(0xC2610);
    uint64_t result=original(op,negate,context,entity,args,count);
    if(!mod_features.ai_max_aggression)return result;
    if(op==0x74 && !(result&0xFF) && args && context) {
        Obj corps=nullptr;Obj blackboard=fn<Obj(__fastcall *)(Obj)>(0xBF7A40)(context);
        if(blackboard && fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(0xB26470)(blackboard,static_cast<unsigned>(args[0]),&corps,0) &&
           valid(corps))result=(result&~0xFFull)|1u;
    }
    return result;
}
// [combat] ai_max_aggression: CVariableGetPersonInfoNode (8E7C0, slot 13512A0) reads a person field
// into a blackboard variable (node+38 person var, +3C item, +40 output var; B26990 stores it).
// Items 13 (호전 +16A: corps leader's target-strength discount), 32 (야망 +17C: ruler's force policy)
// read as 20 (UI 5) for computer forces. Item 34 (+17E) is 계략 (stratagem preference with 외교, N2885),
// so it is left unchanged.
static uint64_t __fastcall person_info_node(Obj node,Obj context,Obj entity)
{
    uint64_t result=fn<uint64_t(__fastcall *)(Obj,Obj,Obj)>(0x8E7C0)(node,context,entity);
    if(!mod_features.ai_max_aggression || result!=1 || !node || !context)return result;
    unsigned item=dword(node,0x3C);
    if(item!=0x0D && item!=0x20)return result;   // 34 (+17E) is 계략, not aggression (REA 합본 2장, 2026-10-22)
    Obj person=nullptr;Obj blackboard=fn<Obj(__fastcall *)(Obj)>(0xBF7A40)(context);
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(0xB26470)(blackboard,dword(node,0x38),&person,0) ||
       !valid(person))return result;
    Obj district=table(0xE0D0,person[0xE0],51);
    if(!valid(district) || fn<int(__fastcall *)(Obj)>(0x233750)(district))return result; // no corps / player force
    fn<char(__fastcall *)(Obj,unsigned,int)>(0xB26990)(blackboard,dword(node,0x40),20);
    return result;
}
// attack_recheck_fix (mission_v2). Tree 1's post-departure recheck of a foothold-attack group (#10050..#10125,
// REA 합본 16장) sums the friendly power aimed at the target into variable 0x47EAF02B, then inside the
// corps-base loop #10114 CGetFootholdFightingStrengthNode (7DC00: out var +3C, radius +40) assigns one
// base's 5-hex power into the SAME variable and #10115 CVariableMinusVariableNode (91030: A=+38 -= B=+3C)
// subtracts it from the enemy power 0xB57EAF65. The final "enemy x70% > friendly" check then compares
// against one base instead of the whole attack, sending fresh attacks home. The wrappers keep the
// original order: the base power still reduces the enemy, and the friendly sum is restored.
static constexpr unsigned kRecheckFriendly=0x47EAF02Bu,kRecheckEnemy=0xB57EAF65u;
static thread_local bool recheck_pending;
static thread_local int recheck_base_power;
static uint64_t __fastcall foothold_strength_node(Obj node,Obj context,Obj entity)
{
    auto original=fn<uint64_t(__fastcall *)(Obj,Obj,Obj)>(0x7DC00);
    if(!mod_features.attack_recheck_fix || !node || !context || dword(node,0x3C)!=kRecheckFriendly || dword(node,0x40)!=5)
        return original(node,context,entity);
    recheck_pending=false;
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(0xBF7A40)(context);int sum=0;
    auto read=fn<char(__fastcall *)(Obj,unsigned,int *,int)>(0xB26300);
    bool had=blackboard && read(blackboard,kRecheckFriendly,&sum,0);
    uint64_t result=original(node,context,entity);
    int base=0;
    if(had && read(blackboard,kRecheckFriendly,&base,0)) {
        fn<char(__fastcall *)(Obj,unsigned,int)>(0xB26990)(blackboard,kRecheckFriendly,sum);
        recheck_base_power=base;recheck_pending=true;
    }
    return result;
}
static uint64_t __fastcall variable_minus_node(Obj node,Obj context,Obj entity)
{
    auto original=fn<uint64_t(__fastcall *)(Obj,Obj,Obj)>(0x91030);
    if(!recheck_pending || !node || !context || dword(node,0x38)!=kRecheckEnemy || dword(node,0x3C)!=kRecheckFriendly)
        return original(node,context,entity);
    recheck_pending=false;
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(0xBF7A40)(context);int sum=0;
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,int *,int)>(0xB26300)(blackboard,kRecheckFriendly,&sum,0))
        return original(node,context,entity);
    auto write=fn<char(__fastcall *)(Obj,unsigned,int)>(0xB26990);
    write(blackboard,kRecheckFriendly,recheck_base_power);   // enemy -= this base's power (original intent)
    uint64_t result=original(node,context,entity);
    write(blackboard,kRecheckFriendly,sum);                  // the friendly attack sum survives the loop
    log_event("attack_recheck_sum_kept",0,static_cast<unsigned>(sum),static_cast<unsigned>(recheck_base_power));
    return result;
}
static volatile LONG bt_condition_state; // 0 pending, 1 installed, 2 unexpected pointer
static void ensure_bt_condition()
{
    if(!mod_features.ai_max_aggression || bt_condition_state)return;
    void *current=*reinterpret_cast<void **>(image+0x1B95CC0);
    if(!current)return; // AI not initialised yet
    if(patch_pointer(0x1B95CC0,0xC2610,reinterpret_cast<void *>(&bt_condition))) {
        InterlockedExchange(&bt_condition_state,1);log_event("ai_max_aggression_installed",0,0,0);
    } else if(InterlockedCompareExchange(&bt_condition_state,2,0)==0)
        log_event("ai_max_aggression_unexpected_pointer",0,static_cast<unsigned>(reinterpret_cast<Obj>(current)-image),0);
}
static uint64_t __fastcall first_army_node(Obj node, Obj context, Obj entity)
{
    COST_SCOPE("first_army_node");
    NativePhase phase("army_priority");
    ensure_bt_condition();
    auto original = fn<Execute>(0x201E0);
    if (!enabled || in_policy || native_search_depth) return original(node, context, entity);
    Obj army = nullptr;
    Obj blackboard = fn<Obj(__fastcall *)(Obj)>(0xBF7A40)(context);
    fn<void(__fastcall *)(Obj, unsigned, Obj *, int)>(0xB26470)(blackboard, dword(node, 0x38), &army, 0);
    ++in_policy;
    AcquireSRWLockExclusive(&policy_lock);
    Progress *p=campaign_controlled(army)?observe_capture(army):nullptr;
#ifdef SORTIE_FORMATION
    if(p)campaign_proactive_departures(army);
    if(p)campaign_saturated_departures(army);
    campaign_frontier_diagnostic();
#endif
    bool changed=p && campaign_individual_priority(army,*p);
    bool urgent=p && (p->forced_return || p->combat_target || p->combat_city || p->home_defense_city || campaign_castle_locked(army));
    if(!changed && !urgent && p && !p->credit && !p->build_chain && !p->facility && !campaign_group_member(army) && !p->regional_worker && node && context && fn<Execute>(0x20060)(node,context,army)==1) {
        log_event("native_supply_restore",army_id(army),0,word(army,0x2A));changed=true;
    }
    if(!changed)changed=apply_priority(army,node,context);
    ReleaseSRWLockExclusive(&policy_lock);
    --in_policy;
    mod_step(changed?"army_node:mod":"army_node:native");
    return changed ? 1 : original(node, context, entity);
}
static uint64_t __fastcall cut_node(Obj node, Obj context, Obj army)
{
    COST_SCOPE("cut_node");
    NativePhase phase("army_cut");
    if (!enabled || in_policy || native_search_depth) return fn<Execute>(0x1F3A0)(node, context, army);
    Obj supplied = army;
    army = fn<Obj(__fastcall *)(Obj, Obj)>(0xB6070)(context, supplied);
    if (!campaign_controlled(army)) return fn<Execute>(0x1F3A0)(node, context, supplied);
    ++in_policy;
    AcquireSRWLockExclusive(&policy_lock);
    Progress *p=observe_capture(army);
    bool changed=p && campaign_individual_priority(army,*p);
    bool urgent=p && (p->forced_return || p->combat_target || p->combat_city || p->home_defense_city || campaign_castle_locked(army));
    if(!urgent && p && !campaign_group_member(army) && !p->regional_worker && node && context && fn<Execute>(0x20060)(node,context,army)==1)changed=true;
    if(!changed && p && !campaign_group_member(army) && !p->regional_worker)changed=campaign_siege_priority(army,*p);
    if(!changed && p && p->operation_leader)changed=true;
    if(!changed && p && p->regional_worker) {apply_priority(army,node,context);changed=true;}
#ifdef SORTIE_FORMATION
    if(!changed && (sortie_member(army) || sortie_escort_member(army)))changed=true;
#endif
    if(!changed && !(p && p->forced_return))changed=try_cut(army);
    if(changed && p && (dword(army,0x1F0)&8))remember_cut(army,*p);
    ReleaseSRWLockExclusive(&policy_lock);
    --in_policy;
    mod_step(changed?"cut_node:mod":"cut_node:native_skip");
    return changed ? 1 : 2;
}
static uint64_t __fastcall city_node(Obj node, Obj context, Obj troops)
{
    COST_SCOPE("city_node");
    NativePhase phase("troops_city");
    auto original = fn<Execute>(0x68CE0);
    if (!enabled || in_policy || native_search_depth || !valid(troops)) return original(node, context, troops);
    ++in_policy;
    AcquireSRWLockExclusive(&policy_lock);
    /* Record any capture/build state before the original clears waypoint lists. */
    for (unsigned i = 1; i <= 500; ++i) {
        Obj army = table(0x7E838, i, 500);
        if (campaign_controlled(army) && belongs_to_troops(army, troops)) {
            Progress *p=observe_capture(army);
            if(p) remember_campaign(army,*p);
        }
    }
    unsigned short before_mode=word(troops,0x14),before_goal=word(troops,0x16);
    if(mod_features.mission_v2)mission_route_waypoint(troops);
    uint64_t result = original(node, context, troops);
    unsigned short after_mode=word(troops,0x14);
    if(before_mode!=after_mode || before_goal!=word(troops,0x16)) {
        // Native Troops AI re-judges a group every day. A group the mod has
        // just dispatched must not be recalled before it leaves its city.
        Obj lead=nullptr;unsigned soldiers=0,fresh=0,members=0;int day=game_day();
        for(unsigned i=1;i<=500;++i) {
            Obj army=table(0x7E838,i,500);
            if(!valid(army) || !belongs_to_troops(army,troops) || !campaign_controlled(army))continue;
            ++members;soldiers+=word(army,0x16);
            Progress *p=observe_capture(army);
            if(p && p->dispatched && day>=p->dispatch_day && day-p->dispatch_day<10 && !p->forced_return)++fresh;
            if(!lead || word(army,0x16)>word(lead,0x16))lead=army;
        }
        log_event("troops_mode_change",members,(static_cast<unsigned>(before_mode)<<16)|after_mode,
            (static_cast<unsigned>(before_goal)<<16)|word(troops,0x16));
        if(mod_features.recall_veto && lead && fresh && fresh==members && soldiers>=3000 && attack_mode(before_mode) && !attack_mode(after_mode) &&
           before_goal && before_goal<=61 && hostile_city(lead,before_goal)) {
            *reinterpret_cast<unsigned short *>(troops+0x14)=before_mode;
            *reinterpret_cast<unsigned short *>(troops+0x16)=before_goal;
            log_event("fresh_troops_recall_vetoed",army_id(lead),soldiers,before_goal);
        }
    }
    mod_step("city_node:native_then_members");
    for (unsigned i = 1; i <= 500; ++i) {
        Obj army = table(0x7E838, i, 500);
        if (valid(army) && belongs_to_troops(army, troops)) {
            if(campaign_controlled(army))apply_priority(army,node,context);
        }
    }
    ReleaseSRWLockExclusive(&policy_lock);
    --in_policy;
    return result;
}
static uint64_t __fastcall retreat_node(Obj node, Obj context, Obj entity)
{
    COST_SCOPE("retreat_node");
    NativePhase phase("army_retreat");
    auto original=fn<Execute>(0x61120);
    if(!enabled || in_policy || native_search_depth)return original(node,context,entity);
    Obj army=nullptr;
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(0xBF7A40)(context);
    fn<void(__fastcall *)(Obj,unsigned,Obj *,int)>(0xB26470)(blackboard,dword(node,0x38),&army,0);
    if(!campaign_controlled(army))return original(node,context,entity);
    ++in_policy;AcquireSRWLockExclusive(&policy_lock);
    Progress *p=observe_capture(army);if(p)remember_campaign(army,*p);
    bool changed=false;
    // <=3,000 soldiers: the original retreat decision stands (user decision 2026-10-21); the mod only
    // keeps its own forced return, which also heads home.
    bool small=word(army,0x16)<=3000;
    if(p && !small)changed=campaign_individual_priority(army,*p);
    if(p && p->forced_return && !changed)changed=recovery_return(army,*p);
#ifdef SORTIE_FORMATION
    if(!changed && !small && p && !p->forced_return && !p->returning && word(army,0x16)>=1000) {
        SortieGroup cohort;unsigned cohort_id=0;
        bool cohort_work=sortie_snapshot(army,cohort,cohort_id,true) && sortie_valid_group(cohort);
        if(p->regional_worker || cohort_work) {
            changed=apply_priority(army,node,context);
            // The native <=3,000 retreat gate must not cancel an active rear or
            // support seat and turn its entire freshly formed group home.
            if(!changed && cohort_work && sortie_valid_group(cohort))changed=true;
        }
        // A 1,000-soldier support escort is below the native <=3,000 retreat
        // gate. While its leader and campaign group are valid it keeps its
        // rear seat instead of turning home on the turn after departure.
        if(!changed && !p->regional_worker && !cohort_work && sortie_escort_member(army))changed=sortie_support_step(army);
        // Same gate for other small units the mod dispatched with an explicit
        // mission: 1,000 protectors, capture companions, guards and female
        // commanders capped at 1,000. Missionless/depleted units still use the
        // native retreat below; campaign_individual_priority keeps forced returns.
        bool mission=(p->campaign && p->city && hostile_city(army,p->city)) || p->companion_parent ||
            p->guard_beneficiary || p->operation_leader;
        if(!changed && mission && !p->regional_worker && !cohort_work && word(army,0x16)<=3000) {
            changed=campaign_individual_priority(army,*p);
            if(!changed)changed=apply_priority(army,node,context);
            if(!changed && p->campaign && hostile_city(army,p->city))changed=campaign_step(army,*p,node,context);
            if(changed)log_event("small_mission_retreat_vetoed",army_id(army),word(army,0x16),word(army,0x2A));
        }
    }
#endif
    if(!changed && p && word(army,0x16)>3000)changed=campaign_individual_priority(army,*p);
    if(!changed && p && word(army,0x16)>3000)changed=campaign_siege_priority(army,*p);
    if(!changed && p && word(army,0x16)>3000 && p->operation_leader)changed=true;
    if(!changed && p && word(army,0x16)>3000) {
        if(!p->campaign) {
            unsigned city=0;int nearest=INT32_MAX;
            for(unsigned i=1;i<=500;++i) {
                Obj ally=table(0x7E838,i,500);if(ally==army || !campaign_controlled(ally) || army_force(ally)!=army_force(army))continue;
                Progress *a=observe_capture(ally);if(!a)continue;remember_campaign(ally,*a);
                int range=distance(word(army,0x2A),word(ally,0x2A));
                if(a->campaign && hostile_city(army,a->city) && range<nearest) {nearest=range;city=a->city;}
            }
            if(city) {p->city=static_cast<unsigned short>(city);p->campaign=true;}
        }
        changed=apply_priority(army,node,context);
        if(!changed) {
            unsigned city=campaign_field_city(army,*p);
            if(city) {p->city=static_cast<unsigned short>(city);p->campaign=true;changed=campaign_step(army,*p,node,context);}
        }
        // Native retreat is vetoed for healthy field armies. Native field work
        // remains available even when no hostile city currently exists.
        if(!changed) {changed=try_other(army,node,context);if(!changed)changed=true;}
    }
    if(!changed && p)log_event("native_retreat_allowed",army_id(army),word(army,0x16),word(army,0x2A));
    mod_step(changed?"retreat_node:mod_veto":"retreat_node:native");
    ReleaseSRWLockExclusive(&policy_lock);--in_policy;
    return changed ? 1 : original(node,context,entity);
}

#include "native_adjacency.inl"
#include "native_expert_settings.inl"

static unsigned crc32(const unsigned char *data, unsigned size)
{
    unsigned crc = ~0u;
    for (unsigned i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
#if defined(PERSONNEL_CANDIDATE) || defined(RECRUITMENT_UI_CANDIDATE)
#include "personnel_policy.h"
static bool personnel_healthy_actor(Obj node,Obj context,unsigned field);
static Obj personnel_context_object(Obj node,Obj context,unsigned field);
#endif
#include "native_domestic.inl"
#include "native_population.inl"
#if defined(PERSONNEL_CANDIDATE) || defined(RECRUITMENT_UI_CANDIDATE)
#include "native_personnel.inl"
#include "native_personnel_batch.inl"
#include "native_recruit_ai.inl"
#endif
#ifdef INJURY_PROTECTION
#include "native_injury.inl"
#endif
#ifdef RECRUITMENT_UI_CANDIDATE
#include "native_ui_common.inl"
#include "native_recruit_ui.inl"
#endif
#ifdef AWARD_WAREHOUSE
#include "native_awards.inl"
#endif
#if defined(PERSONNEL_CANDIDATE) || defined(FRIENDSHIP_CANDIDATE)
#include "native_friendship.inl"
#include "native_friendship_save.inl"
#endif
#ifdef MERIT_CANDIDATE
#include "native_merit.inl"
#ifdef AWARD_WAREHOUSE
#include "native_awards_save.inl"
#endif
#include "native_merit_save.inl"
#endif

static bool code_ready()
{
    __try {
        return crc32(image + 0x1000, kTextSize) == kTextCrc32;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// Steam may still be decoding .text. A mismatched CRC alone is never readiness.
static bool text_exact=true;
static constexpr unsigned code_poll_limit=1200;
struct CodeReadiness {unsigned polls,crc,mask;bool sampled;};
static CodeReadiness code_readiness;
struct DecodeProbe {unsigned rva;unsigned char bytes[5];};
static const DecodeProbe decode_probes[]={
    {0xD2486,{0xE8,0xF5,0xE7,0xFE,0xFF}},
    {0x169F44,{0xE8,0xE7,0,0,0}},
    {0x2B07C0,{0x40,0x53,0x55,0x41,0x57}},
    {0x295160,{0x4C,0x89,0x44,0x24,0x18}},
    {0x1749A0,{0x40,0x55,0x57,0x41,0x56}},
    {0x279190,{0x48,0x89,0x6C,0x24,0x10}},
    {0x302980,{0x40,0x55,0x53,0x56,0x57}},
    {0x232EE0,{0x48,0x89,0x5C,0x24,0x08}},
    {0x7B360,{0x40,0x57,0x48,0x83,0xEC}},
    {0x8DF900,{0xE8,0x1B,0xC8,0xCE,0xFF}},
    {0x6566E0,{0x40,0x57,0x41,0x56,0x41}},
    {0x1ED475,{0x8B,0x1D,0x61,0xAF,0x85}}
};
static unsigned decode_matches(unsigned mask) {
    unsigned n=0;for(;mask;mask>>=1)n+=mask&1;return n;
}
static bool code_sample_ready(CodeReadiness &s,unsigned crc,unsigned mask) {
    if(s.polls++>=code_poll_limit)return false;
    constexpr unsigned total=sizeof(decode_probes)/sizeof(decode_probes[0]);
    unsigned matches=decode_matches(mask);
    if(crc==kTextCrc32) {text_exact=true;return true;}
    bool ready=matches*4>=total*3 && s.sampled && s.crc==crc && s.mask==mask;
    s.sampled=matches*4>=total*3;s.crc=crc;s.mask=mask;
    if(ready) {text_exact=false;log_event("text_crc_mismatch_compat",matches,total);}
    return ready;
}
static bool code_unpacked_ready(CodeReadiness &s) {
    __try {
        unsigned mask=0;
        for(unsigned i=0;i<sizeof(decode_probes)/sizeof(decode_probes[0]);++i)
            if(!memcmp(image+decode_probes[i].rva,decode_probes[i].bytes,5))mask|=1u<<i;
        // Avoid hashing encrypted text on each poll. Strict text matches every probe.
        if(decode_matches(mask)*4<3*(sizeof(decode_probes)/sizeof(decode_probes[0])))
            return code_sample_ready(s,0,mask);
        return code_sample_ready(s,crc32(image+0x1000,kTextSize),mask);
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
#ifdef RECRUITMENT_UI_CANDIDATE
struct RecruitUILink {unsigned slot,original;void *replacement;};
static const RecruitUILink recruit_ui_links[]={
    {0x1440FE8,0x6F1D10,reinterpret_cast<void *>(&recruit_layout_init)},
    {0x1440DF0,0x733BD0,reinterpret_cast<void *>(&recruit_state_update)},
    {0x13AC770,0x2C0820,reinterpret_cast<void *>(&recruit_selector_init)},
    {0x13AC708,0x6BE8E0,reinterpret_cast<void *>(&recruit_selector_close)}
};
static bool recruit_ui_guards_match() {
    for(const auto &link:recruit_ui_links)if(pointer(image,link.slot)!=image+link.original)return false;
    return true;
}
static bool publish_recruit_ui() {
    for(const auto &link:recruit_ui_links)if(!patch_pointer(link.slot,link.original,link.replacement))return false;
    return true;
}
static void restore_recruit_ui() {
    for(const auto &link:recruit_ui_links)restore_pointer(link.slot,link.original,link.replacement);
}
#endif
// Publish only the two recruitment AI nodes in the integrated build. The
// broader, uninstalled PERSONNEL_CANDIDATE also changes unrelated commands.
#if defined(PERSONNEL_CANDIDATE) || defined(RECRUITMENT_UI_CANDIDATE)
struct RecruitAILink {unsigned slot,original;Execute replacement;const unsigned char *entry;};
static const unsigned char recruit_ai_target_entry[]={0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0xC7,0x44,0x24,0x28,0xFE,0xFF,0xFF,0xFF,0x48};
static const unsigned char recruit_ai_command_entry[]={0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x40,0x48,0x8B,0xF9,0x48,0xC7,0x44};
static const RecruitAILink recruit_ai_links[]={
    {0x134CDF8,0x7B360,&recruit_ai_targets,recruit_ai_target_entry},
    {0x13463B0,0x2CC50,&recruit_ai_command,recruit_ai_command_entry}
};
static bool recruit_ai_guards_match() {
    for(const auto &link:recruit_ai_links)
        if(pointer(image,link.slot)!=image+link.original || memcmp(image+link.original,link.entry,16))return false;
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
static bool replace_call(const unsigned char *expected,const unsigned char *replacement) {
    return replace_code16(0xD2480,expected,replacement);
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
static bool prepare_link(CodeLink &link,unsigned rva,unsigned char opcode,Obj relay) {
    link={};link.rva=rva;link.block=rva&~15u;
    unsigned offset=rva-link.block;if(offset>11)return false;
    intptr_t delta=reinterpret_cast<intptr_t>(relay)-reinterpret_cast<intptr_t>(image+rva+5);
    if(delta<INT32_MIN || delta>INT32_MAX)return false;
    memcpy(link.original,image+link.block,16);memcpy(link.modified,link.original,16);
    link.modified[offset]=opcode;memcpy(link.modified+offset+1,&delta,4);return true;
}
static bool prepare_bytes(CodeLink &link,unsigned rva,const unsigned char *bytes,unsigned count) {
    link={};link.rva=rva;link.block=rva&~15u;
    unsigned offset=rva-link.block;if(offset+count>16)return false;
    memcpy(link.original,image+link.block,16);memcpy(link.modified,link.original,16);
    memcpy(link.modified+offset,bytes,count);return true;
}
static unsigned make_domestic_cost_bridge(Obj at) {
    // Called mid-function at native 1ED475, where the caller's RSP is aligned.
    // Preserve all volatile GPRs, XMM0..5 and flags. Only EBX is the new cost.
    unsigned n=0;
    const unsigned char push[]={0x9C,0x50,0x51,0x52,0x41,0x50,0x41,0x51,0x41,0x52,0x41,0x53,
                               0x48,0x81,0xEC,0x88,0,0,0};
    memcpy(at+n,push,sizeof(push));n+=sizeof(push);
    for(unsigned xmm=0;xmm<6;++xmm) {
        unsigned char save[]={0xF3,0x0F,0x7F,static_cast<unsigned char>(0x44+xmm*8),0x24,static_cast<unsigned char>(32+xmm*16)};
        memcpy(at+n,save,sizeof(save));n+=sizeof(save);
    }
    at[n++]=0x48;at[n++]=0xB8;
    *reinterpret_cast<void **>(at+n)=reinterpret_cast<void *>(&domestic_command_cost);n+=8;
    at[n++]=0xFF;at[n++]=0xD0;at[n++]=0x89;at[n++]=0xC3; // CALL RAX; MOV EBX,EAX
    for(unsigned xmm=0;xmm<6;++xmm) {
        unsigned char load[]={0xF3,0x0F,0x6F,static_cast<unsigned char>(0x44+xmm*8),0x24,static_cast<unsigned char>(32+xmm*16)};
        memcpy(at+n,load,sizeof(load));n+=sizeof(load);
    }
    const unsigned char pop[]={0x48,0x81,0xC4,0x88,0,0,0,0x41,0x5B,0x41,0x5A,0x41,0x59,0x41,0x58,0x5A,0x59,0x58,0x9D,0xC3};
    memcpy(at+n,pop,sizeof(pop));return n+sizeof(pop);
}
#if defined(PERSONNEL_CANDIDATE) || defined(FRIENDSHIP_CANDIDATE)
[[maybe_unused]] static unsigned make_friendship_archive_bridge(Obj at,bool save) {
    // Original save site: FF10 + CMP qword [RBX+18],10 (seven bytes).
    // Original load site: FF5008 + TEST EAX,EAX (five bytes).
    // Reproduce the displaced comparison AFTER the helper, leaving its
    // flags intact for the native JC/JZ. Preserve the native return value.
    const unsigned char begin[]={0x48,0x83,0xEC,0x28,0x48,0xB8};
    memcpy(at,begin,sizeof(begin));unsigned n=sizeof(begin);
    *reinterpret_cast<void **>(at+n)=save?reinterpret_cast<void *>(&friendship_file_write):
                                        reinterpret_cast<void *>(&friendship_file_read);n+=8;
    const unsigned char end[]={0xFF,0xD0,0x48,0x83,0xC4,0x28};
    memcpy(at+n,end,sizeof(end));n+=sizeof(end);
    if(save) {const unsigned char cmp[]={0x48,0x83,0x7B,0x18,0x10};memcpy(at+n,cmp,sizeof(cmp));n+=sizeof(cmp);}
    else {at[n++]=0x85;at[n++]=0xC0;}
    at[n++]=0xC3;return n;
}
#endif
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
static void call_restore(CallLink &c) {
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
#ifdef RECRUITMENT_UI_CANDIDATE
struct RecruitAlertEntry {unsigned rva;unsigned char block[16];void *replacement;};
static const RecruitAlertEntry recruit_alert_entries[]={
    {0x2DF204,{0xD5,0x48,0x8B,0xCF,0xE8,0x77,0x04,0xF1,0xFF,0x85,0xC0,0x74,0x3B,0x48,0x85,0xF6},reinterpret_cast<void *>(&recruit_alert_candidate)},
    {0x2DEFAB,{0x8D,0x45,0x50,0x48,0x8D,0x55,0x48,0x48,0x8D,0x4D,0xE0,0xE8,0x60,0x3F,0xFD,0xFF},reinterpret_cast<void *>(&recruit_alert_search)},
    {0x2B3281,{0xBF,0xE8,0xDA,0xE9,0xF7,0xFF,0x49,0x8B,0xF7,0x4C,0x89,0x7D,0x9F,0x8B,0x45,0xBF},reinterpret_cast<void *>(&recruit_alert_location)}
};
struct RecruitAlertCode {CodeLink links[3];};
static bool recruit_alert_guards_match() {
    for(const auto &entry:recruit_alert_entries)
        if(memcmp(image+(entry.rva&~15u),entry.block,16))return false;
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
#ifdef INJURY_PROTECTION
#include "native_injury_links.inl"
#endif
#ifndef SPLIT_COMBAT
#include "native_district_intercept_link.inl" // entry hook when the combat module is absent
#endif
#ifdef MERIT_CANDIDATE
#include "native_merit_links.inl"
#endif
#ifdef AWARD_WAREHOUSE
#include "native_awards_links.inl"
#endif
#ifdef SORTIE_FORMATION
#include "native_sortie_links.inl"
#endif
#if defined(PERSONNEL_CANDIDATE) || defined(FRIENDSHIP_CANDIDATE)
struct FriendshipEntry {unsigned rva,size;unsigned char bytes[6];void *replacement;};
static const FriendshipEntry friendship_entries[]={
    {0x223780,5,{0x48,0x89,0x5C,0x24,0x08},reinterpret_cast<void *>(&friendship_add)},
    {0x232EE0,5,{0x48,0x89,0x5C,0x24,0x08},reinterpret_cast<void *>(&friendship_has)},
    {0x2283F0,6,{0x4C,0x8B,0xDC,0x57,0x41,0x54},reinterpret_cast<void *>(&friendship_remove)},
    {0x22E310,5,{0x48,0x89,0x5C,0x24,0x08},reinterpret_cast<void *>(&friendship_list)},
    {0x21B2E0,5,{0x48,0x8B,0xC4,0x41,0x54},reinterpret_cast<void *>(&friendship_cleanup)},
    {0x2E2330,6,{0x48,0x8B,0xC4,0x55,0x41,0x54},reinterpret_cast<void *>(&friendship_normalize)},
    {0x227600,5,{0x48,0x89,0x5C,0x24,0x10},reinterpret_cast<void *>(&friendship_reset)}
};
struct FriendshipCode {CodeLink links[9];};
[[maybe_unused]] static bool friendship_guards_match() {
    for(const auto &entry:friendship_entries)
        if(memcmp(image+entry.rva,entry.bytes,entry.size))return false;
    const unsigned char save[]={0xFF,0x10,0x48,0x83,0x7B,0x18,0x10},load[]={0xFF,0x50,0x08,0x85,0xC0};
    return !memcmp(image+0x3F7E32,save,sizeof(save)) && !memcmp(image+0x3FA5C4,load,sizeof(load)) &&
        pointer(image,0x13E69A0)==image+0x50B670 && pointer(image,0x13E60F8)==image+0x509580 &&
        pointer(image,0x13A6BB0)==image+0x2390B0;
}
[[maybe_unused]] static bool prepare_friendship(FriendshipCode &code,Obj relay) {
    // Caller reserves [512,1024) of the same near relay page. Code guards
    // must match before anything is prepared or published.
    if(!friendship_guards_match())return false;
    for(unsigned i=0;i<7;++i) {
        const auto &entry=friendship_entries[i];Obj stub=relay+(i<4?512+i*64:832+(i-4)*64),gate=stub+32;
        absolute_jump(stub,entry.replacement);memcpy(gate,entry.bytes,entry.size);
        absolute_jump(gate+entry.size,image+entry.rva+entry.size);
        if(!prepare_link(code.links[i],entry.rva,0xE9,stub))return false;
        if(entry.size==6)code.links[i].modified[entry.rva-code.links[i].block+5]=0x90;
    }
    original_friend_add=reinterpret_cast<FriendAdd>(relay+544);
    original_friend_has=reinterpret_cast<FriendHas>(relay+608);
    original_friend_remove=reinterpret_cast<FriendAdd>(relay+672);
    original_friend_list=reinterpret_cast<FriendList>(relay+736);
    original_friend_cleanup=reinterpret_cast<FriendOne>(relay+864);
    original_friend_normalize=reinterpret_cast<FriendOne>(relay+928);
    original_friend_reset=reinterpret_cast<FriendOne>(relay+992);
    make_friendship_archive_bridge(relay+768,true);make_friendship_archive_bridge(relay+800,false);
    if(!prepare_link(code.links[7],0x3F7E32,0xE8,relay+768) ||
       !prepare_link(code.links[8],0x3FA5C4,0xE8,relay+800))return false;
    // The CMP's complete five-byte instruction is replayed in the relay.
    code.links[7].modified[7]=0x90;code.links[7].modified[8]=0x90;return true;
}
[[maybe_unused]] static bool publish_friendship(FriendshipCode &code) {
    for(auto &link:code.links)if(!publish_link(link))return false;
    return patch_pointer(0x13E69A0,0x50B670,reinterpret_cast<void *>(&friendship_save_host)) &&
           patch_pointer(0x13E60F8,0x509580,reinterpret_cast<void *>(&friendship_load_host)) &&
           patch_pointer(0x13A6BB0,0x2390B0,reinterpret_cast<void *>(&friendship_person_read));
}
[[maybe_unused]] static void restore_friendship(FriendshipCode &code) {
    restore_pointer(0x13E69A0,0x50B670,reinterpret_cast<void *>(&friendship_save_host));
    restore_pointer(0x13E60F8,0x509580,reinterpret_cast<void *>(&friendship_load_host));
    restore_pointer(0x13A6BB0,0x2390B0,reinterpret_cast<void *>(&friendship_person_read));
    for(auto &link:code.links)restore_link(link);
}
#endif
#if defined(PERSONNEL_CANDIDATE) || defined(HIDDEN_LOYALTY)
static bool loyalty_guards_match() {
    const unsigned char numeric[]={0x48,0x0F,0x4E,0xC2},text[]={0x0F,0x4F,0xD8};
    return !memcmp(image+0x34A8D6,numeric,sizeof(numeric)) && !memcmp(image+0x35CA35,text,sizeof(text));
}
#endif
#ifdef PERSONNEL_CANDIDATE
struct PersonnelNodeLink {unsigned slot,original;Execute replacement;};
static const PersonnelNodeLink personnel_links[]={
    {0x134CDF8,0x7B360,&personnel_recruitment_targets},
    {0x1347318,0x39490,&personnel_employ_actors},
    {0x1347528,0x3D170,&personnel_search_actors},
    {0x13463B0,0x2CC50,&personnel_employ_command},
    {0x1349560,0x53E50,&personnel_search_command},
    {0x1352AF8,0x7E550,&personnel_officer_candidates},
    {0x1346AF8,0x34F10,&personnel_departure},
    {0x1346BA0,0x35770,&personnel_transport},
    {0x1347A28,0x3DBF0,&personnel_recruit_officer},
    {0x1345790,0x3E490,&personnel_training_officer},
    {0x1347F20,0x3E100,&personnel_spy_officer}
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
static Obj allocate_near(Obj call)
{
    SYSTEM_INFO info; GetSystemInfo(&info);
    uintptr_t start = reinterpret_cast<uintptr_t>(call) & ~(uintptr_t(info.dwAllocationGranularity) - 1);
    for (uintptr_t offset = info.dwAllocationGranularity; offset < 0x70000000; offset += info.dwAllocationGranularity) {
        for (int sign : {1, -1}) {
            uintptr_t address = sign > 0 ? start + offset : (start > offset ? start - offset : 0);
            if (!address) continue;
            Obj memory = static_cast<Obj>(VirtualAlloc(reinterpret_cast<void *>(address), 0x1000,
                                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            if (memory) return memory;
        }
    }
    return nullptr;
}
#include "native_path_count.inl"
#if defined(RECRUITMENT_UI_CANDIDATE) || defined(PROGRESS_NATIVE_FIXTURE)
#include "native_progress.inl"
#endif
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
#include "native_name_lookup.inl"
#include "native_speed_buttons.inl"
#include "native_layout_wait.inl"
#include "gpu_profile.h"
#include "native_save_codec.inl"
#include "native_occupy_fast.inl"
#include "native_caravan_stay.inl"
#include "native_stat_exp.inl"
#include "native_dispatch_days.inl"
#include "native_fast_menu.inl"
#include "native_appear_batch.inl"
#include "native_area_index.inl"
#include "native_path_fast.inl"
#include "native_face_keep.inl"
#include "native_mediation_plus.inl"
#include "native_drag_learn.inl"
#include "native_lord_change.inl"
#include "native_region_officers.inl"
#include "native_damage_preview.inl"
#include "native_building_destruction.inl"
#include "native_diplomacy_intel.inl"
#ifdef RUNTIME_COST_PROFILE
#include "native_face_timing.inl" // [diagnostics] cost_profile: army flag face setter timing
#endif
#include <array>
#include "native_ai_trace.inl" // [diagnostics] ai_trace: observation of native AI nodes
#endif
#if defined(RECRUITMENT_UI_CANDIDATE) || defined(TARGET_RETREAT_FIXTURE)
#ifdef TARGET_RETREAT_FIXTURE
#include "native_target_retreat.inl" // Archived policy fixture only, never installed.
#endif
#endif
#if defined(SPLIT_MODULE_FIXTURE) || defined(TACTICS_FIXTURE)
#include "native_area_index.inl" // guard-only binding for synthetic images
#include "native_path_fast.inl" // guard-only binding for synthetic images
#include "native_diplomacy_intel.inl" // guard-only binding for synthetic images
#include "native_advisor_briefing.inl" // guard-only binding for synthetic images
#endif
#if defined(SPLIT_BASE) || defined(SPLIT_COMBAT)
#ifdef SPLIT_BASE
#include "native_expert_links.inl"
#include "native_tactic_priority.inl"
#include "native_unified_personnel.inl"
#include "native_unified_settings.inl"
#include "native_unified_actions.inl"
#include "native_unified_posts_trade.inl"
#if !defined(SPLIT_MODULE_FIXTURE)
#include "native_advisor_briefing.inl"
#endif
#endif
#include "native_split_install.inl"
#else
static bool install()
{
#ifdef AWARD_WAREHOUSE
    if(!award_guards_match())return false;
#endif
#ifdef INJURY_PROTECTION
    if(!injury_guards_match())return false;
#endif
#ifdef RECRUITMENT_UI_CANDIDATE
    if(!recruit_ui_guards_match() || !recruit_ai_guards_match() || !recruit_alert_guards_match())return false;
#endif
#ifdef MERIT_CANDIDATE
    if(!merit_guards_match())return false;
#endif
#ifdef FRIENDSHIP_CANDIDATE
    if(!friendship_guards_match())return false;
#endif
#ifdef PERSONNEL_CANDIDATE
    if(!personnel_guards_match())return false;
#endif
#ifdef HIDDEN_LOYALTY
    if(!loyalty_guards_match())return false;
#endif
#ifdef SORTIE_FORMATION
    if(!sortie_guards_match())return false;
#endif
    Obj call = image + 0xD2486;
    if (memcmp(call, kCutCallBytes, 5)) return false;
    if(memcmp(image+0x169F44,kPairCallBytes,5) || memcmp(image+0x1749A0,kDamageEntryBytes,5) || memcmp(image+0x2B07C0,kAdjacencyEntryBytes,5) ||
       memcmp(image+0x279190,kBuildPermissionEntryBytes,5) ||
       memcmp(image+0x295160,kDomesticTickEntryBytes,5) ||
       memcmp(image+0x36423,kDomesticBudgetBranchBytes,6) ||
       memcmp(image+0x1ED475,kDomesticCostLoadBytes,6) || memcmp(image+0x302980,population_entry,16))return false;
    for (auto pair : {0x1344518u, 0x1343C98u, 0x134B4F8u, 0x134AF00u}) {
        unsigned original = pair == 0x1344518 ? 0x201E0 : pair == 0x1343C98 ? 0x1F3A0 : pair == 0x134B4F8 ? 0x68CE0 : 0x61120;
        if (pointer(image, pair) != image + original) return false;
    }
    if(pointer(image,0x1346E70)!=image+0x36320 || pointer(image,0x134C8F8)!=image+0x7A110)return false;
    Obj relay = allocate_near(call);
    if (!relay) return false;
    Obj combat_relay=allocate_near(image+0x169F44);
    if(!combat_relay) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    absolute_jump(combat_relay,reinterpret_cast<void *>(&combat_pair_scores));
    absolute_jump(combat_relay+1536,reinterpret_cast<void *>(&combat_apply_damage));
    memcpy(combat_relay+1568,kDamageEntryBytes,5);
    absolute_jump(combat_relay+1573,image+0x1749A5);
    original_damage=reinterpret_cast<ApplyDamage>(combat_relay+1568);
    absolute_jump(combat_relay+32,reinterpret_cast<void *>(&contextual_adjacency));
    // All five stolen bytes are complete, position-independent PUSHs. The
    // trampoline executes those and rejoins before SUB RSP at native +5.
    memcpy(combat_relay+64,kAdjacencyEntryBytes,5);
    absolute_jump(combat_relay+69,image+0x2B07C5);
    original_adjacency=reinterpret_cast<Adjacency>(combat_relay+64);
    absolute_jump(combat_relay+96,reinterpret_cast<void *>(&build_permission));
    memcpy(combat_relay+128,kBuildPermissionEntryBytes,5);
    absolute_jump(combat_relay+133,image+0x279195);
    original_build_permission=reinterpret_cast<BuildPermission>(combat_relay+128);
    absolute_jump(combat_relay+160,reinterpret_cast<void *>(&domestic_tick));
    memcpy(combat_relay+192,kDomesticTickEntryBytes,5);
    absolute_jump(combat_relay+197,image+0x295165);
    original_domestic_tick=reinterpret_cast<DomesticTick>(combat_relay+192);
    unsigned domestic_bridge_size=make_domestic_cost_bridge(combat_relay+224);
    CodeLink pair_link,damage_link,adj_link,build_link,tick_link,cost_link,budget_link,population_link;
    absolute_jump(combat_relay+3840,reinterpret_cast<void *>(&population_growth));
    memcpy(combat_relay+3872,population_entry,5);absolute_jump(combat_relay+3877,image+0x302985);
    original_population_growth=reinterpret_cast<PopulationGrowth>(combat_relay+3872);
    if(!prepare_link(population_link,0x302980,0xE9,combat_relay+3840)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#ifdef SORTIE_FORMATION
    SortieCode sortie_code;
    if(!prepare_sortie(sortie_code,combat_relay)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#endif
#ifdef RECRUITMENT_UI_CANDIDATE
    RecruitAlertCode recruit_alert_code;
    if(!prepare_recruit_alert(recruit_alert_code,combat_relay)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#endif
#ifdef AWARD_WAREHOUSE
    AwardCode award_code;
    if(!prepare_awards(award_code,combat_relay)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#endif
#ifdef INJURY_PROTECTION
    InjuryCode injury_code;
    if(!prepare_injury(injury_code,combat_relay)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#endif
#ifdef MERIT_CANDIDATE
    MeritCode merit_code;
    if(!prepare_merit(merit_code,combat_relay)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#endif
#ifdef FRIENDSHIP_CANDIDATE
    FriendshipCode friend_code;
    if(!prepare_friendship(friend_code,combat_relay)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#endif
#if defined(PERSONNEL_CANDIDATE) || defined(HIDDEN_LOYALTY)
    CodeLink loyalty_numeric,loyalty_text;
    const unsigned char raw_loyalty_pointer[4]={0x48,0x89,0xD0,0x90};
    const unsigned char no_loyalty_clamp[3]={0x90,0x90,0x90};
    if(!prepare_bytes(loyalty_numeric,0x34A8D6,raw_loyalty_pointer,4) ||
       !prepare_bytes(loyalty_text,0x35CA35,no_loyalty_clamp,3)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#endif
    const unsigned char no_budget_check[6]={0x90,0x90,0x90,0x90,0x90,0x90};
    if(!prepare_link(pair_link,0x169F44,0xE8,combat_relay) ||
       !prepare_link(damage_link,0x1749A0,0xE9,combat_relay+1536) ||
       !prepare_link(adj_link,0x2B07C0,0xE9,combat_relay+32) ||
       !prepare_link(build_link,0x279190,0xE9,combat_relay+96) ||
       !prepare_link(tick_link,0x295160,0xE9,combat_relay+160) ||
       !prepare_link(cost_link,0x1ED475,0xE8,combat_relay+224) ||
       !prepare_bytes(budget_link,0x36423,no_budget_check,6)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
    // Replace the complete six-byte RIP-relative MOV with CALL + NOP.
    cost_link.modified[0x1ED475-cost_link.block+5]=0x90;
    DWORD combat_old=0;
    if(!VirtualProtect(combat_relay,0x1000,PAGE_EXECUTE_READ,&combat_old)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
    // Population trampoline ends at byte 3891; flush the entire allocated
    // relay page, including that monthly-only path and future additions.
    unsigned relay_used=224+domestic_bridge_size;
    if(relay_used<0x1000)relay_used=0x1000;
#ifdef RECRUITMENT_UI_CANDIDATE
    if(relay_used<3040)relay_used=3040;
#endif
#ifdef SORTIE_FORMATION
    if(relay_used<2944)relay_used=2944;
#endif
#ifdef AWARD_WAREHOUSE
    if(relay_used<2432)relay_used=2432;
#endif
#ifdef INJURY_PROTECTION
    if(relay_used<1664)relay_used=1664;
#endif
    if(relay_used<1587)relay_used=1587;
#ifdef MERIT_CANDIDATE
    if(relay_used<1152)relay_used=1152;
#endif
#ifdef FRIENDSHIP_CANDIDATE
    if(relay_used<1024)relay_used=1024;
#endif
    FlushInstructionCache(GetCurrentProcess(),combat_relay,relay_used);
    /* FF25 [RIP+0] jumps without clobbering argument or return registers. */
    relay[0] = 0xFF; relay[1] = 0x25; memset(relay + 2, 0, 4);
    *reinterpret_cast<void **>(relay + 6) = reinterpret_cast<void *>(&cut_player_predicate);
    DWORD old = 0;
    if (!VirtualProtect(relay, 0x1000, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(relay, 0, MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE); return false;
    }
    FlushInstructionCache(GetCurrentProcess(), relay, 14);
    intptr_t delta = reinterpret_cast<intptr_t>(relay) - reinterpret_cast<intptr_t>(call + 5);
    if (delta < INT32_MIN || delta > INT32_MAX) { VirtualFree(relay, 0, MEM_RELEASE); return false; }
    unsigned char original_block[16], modified_block[16];
    memcpy(original_block, image + 0xD2480, 16); memcpy(modified_block, original_block, 16);
    modified_block[6] = 0xE8; memcpy(modified_block + 7, &delta, 4);
    if (!replace_call(original_block, modified_block)) { VirtualFree(relay, 0, MEM_RELEASE); return false; }
    if (!publish_link(adj_link) || !publish_link(pair_link) || !publish_link(damage_link) || !publish_link(build_link) ||
        !publish_link(tick_link) || !publish_link(cost_link) || !publish_link(budget_link) || !publish_link(population_link) ||
        !patch_pointer(0x1344518, 0x201E0, reinterpret_cast<void *>(&first_army_node)) ||
        !patch_pointer(0x1343C98, 0x1F3A0, reinterpret_cast<void *>(&cut_node)) ||
        !patch_pointer(0x134B4F8, 0x68CE0, reinterpret_cast<void *>(&city_node)) ||
        !patch_pointer(0x134AF00, 0x61120, reinterpret_cast<void *>(&retreat_node)) ||
        !patch_pointer(0x1346E70, 0x36320, reinterpret_cast<void *>(&domestic_officer_node)) ||
        !patch_pointer(0x134C8F8, 0x7A110, reinterpret_cast<void *>(&domestic_selection_node))
#ifdef MERIT_CANDIDATE
        || !publish_merit(merit_code)
#endif
#ifdef RECRUITMENT_UI_CANDIDATE
        || !publish_recruit_ui() || !publish_recruit_ai() || !publish_recruit_alert(recruit_alert_code)
#endif
#ifdef INJURY_PROTECTION
        || !publish_injury(injury_code,true,true)
#endif
#ifdef AWARD_WAREHOUSE
        || !publish_awards(award_code)
#endif
#ifdef SORTIE_FORMATION
        || !publish_sortie(sortie_code)
#endif
#if defined(PERSONNEL_CANDIDATE) || defined(HIDDEN_LOYALTY)
        || !publish_link(loyalty_numeric) || !publish_link(loyalty_text)
#endif
#ifdef PERSONNEL_CANDIDATE
        || !publish_personnel_nodes()
#endif
#ifdef FRIENDSHIP_CANDIDATE
        || !publish_friendship(friend_code)
#endif
        ) {
#ifdef AWARD_WAREHOUSE
        restore_awards(award_code);
#endif
#ifdef SORTIE_FORMATION
        restore_sortie(sortie_code);
#endif
#ifdef INJURY_PROTECTION
        restore_injury(injury_code);
#endif
#ifdef RECRUITMENT_UI_CANDIDATE
        restore_recruit_alert(recruit_alert_code);
        restore_recruit_ai();
        restore_recruit_ui();
#endif
#ifdef MERIT_CANDIDATE
        restore_merit(merit_code);
#endif
#ifdef FRIENDSHIP_CANDIDATE
        restore_friendship(friend_code);
#endif
#ifdef PERSONNEL_CANDIDATE
        restore_personnel_nodes();
#endif
#if defined(PERSONNEL_CANDIDATE) || defined(HIDDEN_LOYALTY)
        restore_link(loyalty_numeric);restore_link(loyalty_text);
#endif
        restore_pointer(0x1344518, 0x201E0, reinterpret_cast<void *>(&first_army_node));
        restore_pointer(0x1343C98, 0x1F3A0, reinterpret_cast<void *>(&cut_node));
        restore_pointer(0x134B4F8, 0x68CE0, reinterpret_cast<void *>(&city_node));
        restore_pointer(0x134AF00, 0x61120, reinterpret_cast<void *>(&retreat_node));
        restore_pointer(0x1346E70, 0x36320, reinterpret_cast<void *>(&domestic_officer_node));
        restore_pointer(0x134C8F8, 0x7A110, reinterpret_cast<void *>(&domestic_selection_node));
        replace_call(modified_block, original_block);
        restore_link(pair_link);restore_link(damage_link);restore_link(adj_link);restore_link(build_link);
        restore_link(tick_link);restore_link(cost_link);restore_link(budget_link);
        restore_link(population_link);
        /* Keep the tiny relay allocated: another thread could already be
         * returning through it. With policies disabled it invokes native code. */
        return false;
    }
    InterlockedExchange(&enabled, 1);
#ifdef RECRUITMENT_UI_CANDIDATE
    // Optional and independently guarded: rejection leaves the original game
    // functions and the already validated integrated features operational.
    install_progress_native();
    // Native interception/cancellation runs unchanged; no target-retreat patch.
#endif
    return true;
}
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
static bool __cdecl combat_module_prepare(Obj host) {
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
    if(diagnostic_logging)AddVectoredExceptionHandler(0,observe_native_exception);
    return split_prepare();
}
static void __cdecl combat_departure_scope(int delta) {
    if(delta>0)++sortie_auto_departure_scope;
    else if(delta<0 && sortie_auto_departure_scope)--sortie_auto_departure_scope;
}
#ifndef NO_RUNTIME_LOG // diagnostic builds only; release export list stays fixed
// Optional diagnostic export (not part of SplitCombatApi): size 0 enables labels; otherwise copies and
// clears this thread's labels. Called by ai_trace on the AI thread that ran the traced node.
extern "C" __declspec(dllexport) unsigned __cdecl SAN14CombatTraceStep(char *out,unsigned size) {
    if(!size) {InterlockedExchange(&mod_step_wanted,1);return 0;}
    unsigned n=mod_step_length<size?mod_step_length:size-1;
    if(out) {memcpy(out,mod_step_text,n);out[n]=0;}
    mod_step_length=0;mod_step_text[0]=0;return n;
}
#endif
extern "C" __declspec(dllexport) const SplitCombatApi *__cdecl SAN14CombatGetApi(unsigned abi) {
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
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.TimeDateStamp != kImageTimestamp ||
        pe->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) { log_event("unsupported_executable"); return 0; }
    crash_image_size=pe->OptionalHeader.SizeOfImage;
    auto module_image=reinterpret_cast<Obj>(self_module);
    auto module_dos=reinterpret_cast<IMAGE_DOS_HEADER *>(module_image);
    auto module_pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(module_image+module_dos->e_lfanew);
    crash_module_size=module_pe->OptionalHeader.SizeOfImage;
    for (unsigned attempt = 0; attempt < 1200; ++attempt) {
#ifdef SPLIT_BASE
        bool ready=code_unpacked_ready(code_readiness);
#else
        // Preserve the legacy integrated route for regression comparisons.
        bool ready=false;
        __try {ready=!memcmp(image+0xD2486,kCutCallBytes,5);}
        __except(EXCEPTION_EXECUTE_HANDLER) {ready=false;}
        ready=ready && code_ready();
#endif
        if (ready) {
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
#elif defined(MERIT_CANDIDATE)
            log_event(installed ? "merit_bond_candidate_hooks_installed" : "installation_rejected");
#elif defined(FRIENDSHIP_CANDIDATE)
            log_event(installed ? "friendship_500_candidate_hooks_installed" : "installation_rejected");
#elif defined(PERSONNEL_CANDIDATE)
            log_event(installed ? "personnel_partial_candidate_hooks_installed" : "installation_rejected");
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
extern "C" __declspec(dllexport) bool __cdecl SAN14ConvenienceInitialize(unsigned abi,const SplitCombatApi *api) {
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
static unsigned __cdecl split_fixture_scope() {return sortie_auto_departure_scope;}
static uint64_t __cdecl split_fixture_departure() {return personnel_departure(nullptr,nullptr,nullptr);}
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
extern "C" __declspec(dllexport) void __cdecl SAN14SplitFixtureDisable(unsigned mask) {split_fixture_disabled=mask;}
extern "C" __declspec(dllexport) unsigned __cdecl SAN14SplitFixtureFeatures() {
    return (mod_features.unified?1u:0u)|(mod_features.merit?2u:0u)|(mod_features.domestic?4u:0u)|
        (mod_features.population?8u:0u)|(mod_features.battle_rules?16u:0u);
}
extern "C" __declspec(dllexport) const SplitFixtureApi *__cdecl SAN14SplitFixtureGetApi() {
    static const SplitFixtureApi api={split_fixture_prepare,split_publish,split_rollback,split_activate,
        split_fixture_scope,split_fixture_departure,split_fixture_failure};return &api;
}
#endif
extern "C" __declspec(dllexport) void AIInitialize() { /* import anchor */ }
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
#endif
#if !defined(SPLIT_COMBAT) && !defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
        HANDLE worker = CreateThread(nullptr, 0, initialize, nullptr, 0, nullptr);
        if (worker) CloseHandle(worker);
#endif
    }
    return TRUE;
}
#endif

