// [combat] siege_guard (user 2026-11-10, local17). Computer siege-group field armies whose commander has
// effective intelligence >= 80 intercept player armies (direct or delegated corps) within siege_guard::kRadius = 6 hexes (local160; 10 at local148, 6 at local43, 5 before).
//
// Rule "off = original, nothing in the save": the original per-army tick 273AC0 reads the engage option
// Army+0x40 once at its start and never writes it (2 = engage enemies in range). The two outer callers
// (273100 at 2732CA, 276250 at 276296) are wrapped: only for a guard with a live player army within 5 hexes
// the option is set to 2 for the duration of that one call and put back afterwards (when it is still 2).
// The search inside the engage branch (2B76B0 at 27402F, native radius 3) uses radius 5 for that guard.
// The guard set is a memory-only table, rebuilt once per game day. Switch off or DLL unload = the three
// CALLs are restored; no Army field keeps a mod value.
#include "siege_guard_policy.h"

static Obj allocate_near(Obj call,unsigned bytes);   // defined later in native_adapter.cpp

namespace siege_guard_hook {

struct Guard {
    Obj root = nullptr;      // world() when chosen
    Obj army = nullptr;      // guard army object
    unsigned commander = 0;  // Army+0x12 when chosen
};
static Guard guards[501];               // keyed by army id; static storage, game thread only
static unsigned players[500];           // live player-force army ids, refreshed once per game day
static unsigned player_count;
static int built_day = -1000000;
static Obj built_root;
static thread_local Obj active_army;    // army whose 273AC0 call currently runs with the transient option (review CR-06: per thread)

using Tick = void(__fastcall *)(Obj);
using Collect = unsigned(__fastcall *)(Obj, int, Obj);

static bool player_side_army(Obj army) { COST_SCOPE("siege_guard:player_side_army");
    unsigned player = player_force();
    return player && valid(army) && army_force(army) == player;
}

// [combat] wall_hold_chase (user 2026-11-21 「저돌이나 적근접시 공격을 가진 부대가 중립건물을 공격하고 있으면 바다위에서 갈팡질팡」,
// 2026-11-22 「중립건물 칠때만 무효화되어야해 다른때에는 저돌이 켜져야해」, docs/NEUTRAL_WALL_AGE.md 4절): the wall a computer-side army
// is attacking for the mod - a route_hook breach entry (hit response, shortest way, every fifth army, siege weapons) whose wall
// stands, or without route_hook the older destroy mission (destroy_reason 1..3, order 6/11 on a neutral wall). 0 = none: the
// original engage option and 저돌 work as usual (on the way home, between targets, every other army).
static unsigned wall_held(Obj army) { COST_SCOPE("siege_guard:wall_held");
    if (!mod_features.wall_hold_chase || !valid(army) || army[0x140] || !computer_side_army(army)) return 0;
    const unsigned id = army_id(army);
    const Progress *p = id && id <= 500 && progress[id].army == army && progress[id].root == world() ? &progress[id] : nullptr;
    unsigned wall = route_hook::holds_wall(army);
    // The older path still gives reason 2 without route_hook_hit and reasons 1/3 without route_hook_breach (path_wall off).
    if (!wall && p && p->destroy_reason >= 1 && p->destroy_reason <= 3 &&
        army[0x37] == 6 && army[0x38] == 11) {
        Obj b = table(0x6DA98, word(army, 0x3A), 3000);
        const unsigned kind = valid(b) ? b[0x12] : 0u;
        if ((kind == 9 || kind == 10 || kind == 16) && !b[0x13] && word(b, 0x14) && word(b, 0x10) < 48400) wall = word(army, 0x3A);
    }
    return wall_age::hold_chase(true, wall != 0, true, p && (p->returning || p->forced_return)) ? wall : 0u;
}

static bool capable(Obj army) { COST_SCOPE("siege_guard:capable");
    if (!valid(army) || army[0x140] || !computer_side_army(army)) return false;
    if (wall_held(army)) return false;   // wall_hold_chase: a wall breaker is no guard
    // local88: a home-capture / retake worker rides in a group of kind 10 too, but it is no siege army.
    { const unsigned id = army_id(army); if (id && id <= 500 && progress[id].army == army && progress[id].regional_worker) return false; }
    if (path_wall_locked(army, 0) || path_wall_done(army, 0)) return false;   // a path_wall demolition keeps its mission
    unsigned gid = word(army, 0x58);
    Obj group = gid && gid <= 500 ? table(0x7F8D8, gid, 500) : nullptr;
    if (!valid(group) || !attack_mode(word(group, 0x14)) || !belongs_to_troops(army, group)) return false;
    unsigned commander = word(army, 0x12);
    Obj c = commander ? table(0x158, commander, 6000) : nullptr;
    if (!valid(c) || word(c, 0x10) != commander) return false;
    unsigned value = static_cast<unsigned>(fn<int(__fastcall *)(Obj, unsigned, unsigned)>(GA(GA_231770))(c, 2, 0xFFF));
    return siege_guard::guard_capable(army[0x1B], value, false);
}

// Once per game day (first tick of the day): the guard table and the player army list.
static void rebuild() { COST_SCOPE("siege_guard:rebuild");
    Obj root = world();
    int day = game_day();
    if (root == built_root && day == built_day) return;
    built_root = root; built_day = day;
    player_count = 0;
    unsigned count = 0;
    for (unsigned id = 1; id <= 500; ++id) {
        guards[id] = Guard{};
        Obj a = root ? table(0x7E838, id, 500) : nullptr;
        if (!valid(a) || a[0x140]) continue;
        if (player_side_army(a)) { if (player_count < 500) players[player_count++] = id; continue; }
        if (!capable(a)) continue;
        guards[id].root = root; guards[id].army = a; guards[id].commander = word(a, 0x12);
        ++count;
    }
#ifndef NO_RUNTIME_LOG
    log_event("siege_guard_day", count, player_count, static_cast<unsigned>(day));
#endif
}

static bool is_guard(Obj army) { COST_SCOPE("siege_guard:is_guard");
    unsigned id = army_id(army);
    if (!id || id > 500) return false;
    const Guard &g = guards[id];
    return g.army == army && g.root == world() && word(army, 0x12) == g.commander && !army[0x140];
}

// [combat] siege_guard_range 1..4 -> 6/8/10/12 hexes (user 2026-11-15); kRadius is the default.
static int guard_radius() { return siege_guard::kRadius + 2 * (static_cast<int>(mod_features.siege_guard_range) - 1); }

static bool player_near(Obj army) { COST_SCOPE("siege_guard:player_near");
    unsigned here = word(army, 0x2A);
    if (here >= 48400) return false;
    for (unsigned i = 0; i < player_count; ++i) {
        Obj other = table(0x7E838, players[i], 500);
        if (!valid(other) || other[0x140] || !player_side_army(other)) continue;
        unsigned hex = word(other, 0x2A);
        if (hex < 48400 && distance(here, hex) <= guard_radius()) return true;
    }
    return false;
}

// Wrapper for the two outer 273AC0 calls. Recursive 274EA9 calls run inside it untouched.
static void __fastcall tick(Obj army) { COST_SCOPE("siege_guard:tick");
    Tick original = fn<Tick>(GA(GA_273AC0));
    if (!enabled || !mod_features.siege_guard || !army || active_army) { original(army); return; }
    rebuild();
    // wall_hold_chase: an army assigned to a wall during the day leaves the guards at once (rebuild runs once a day).
    if (!is_guard(army) || wall_held(army) || !player_near(army)) { original(army); return; }
    unsigned char old = army[0x40];
    static int logged_day[501];
    const unsigned id = army_id(army);
    if (id && id <= 500 && logged_day[id] != game_day()) {   // once per army and day: the guard engaged with option 2
        logged_day[id] = game_day();
        log_event("siege_guard_engage", id, word(army, 0x12), word(army, 0x2A));
    }
    active_army = army;
    if (old != 2) army[0x40] = 2;
    original(army);
    if (old != 2 && army[0x40] == 2) army[0x40] = old;
    active_army = nullptr;
}

// The engage-branch search (sole 2B76B0 caller). Radius 5 only for the guard inside its wrapped tick.
// wall_hold_chase: for an army that breaks a neutral wall for the mod, no candidate (0 found, the list left empty), so 273AC0
// does not call 2903E0 and sets no chase target (KB: 273AC0 only acts on a count > 0; the engage option and 저돌 both reach
// the chase through this one call). Fighting an adjacent enemy stays the original combat's.
static unsigned __fastcall collect(Obj army, int radius, Obj list) { COST_SCOPE("siege_guard:collect");
    Collect original = fn<Collect>(GA(GA_2B76B0));
    if (enabled && army) {
        if (const unsigned wall = wall_held(army)) {
            static int logged_day[501];
            const unsigned id = army_id(army);
            if (id && id <= 500 && logged_day[id] != game_day() + 1) {   // once per army and day
                logged_day[id] = game_day() + 1;
                log_event("wall_chase_hold", id, wall, static_cast<unsigned>(radius));
            }
            return 0;
        }
    }
    if (enabled && mod_features.siege_guard && army && army == active_army && radius < guard_radius())
        radius = guard_radius();
    return original(army, radius, list);
}
static bool wanted() { return mod_features.siege_guard || mod_features.wall_hold_chase; }

static bool call_targets(unsigned site, unsigned target) { COST_SCOPE("siege_guard:call_targets");
    if (!image || image[site] != 0xE8) return false;
    int delta = 0;
    memcpy(&delta, image + site + 1, 4);
    return image + site + 5 + delta == image + target;
}

static CallLink calls[3];
static Obj relay;
static bool ready;

// 0 = prepared (or off), 1 = a CALL no longer targets its original function, 2 = prepare failed.
static unsigned prepare() { COST_SCOPE("siege_guard:prepare");
    for (auto &c : calls) c = CallLink{};
    ready = false;
    if (!wanted()) return 0;   // the tick CALLs pass straight through without siege_guard
    const unsigned sites[3] = {GA(GA_2732CA), GA(GA_276296), GA(GA_27402F)};
    const unsigned targets[3] = {GA(GA_273AC0), GA(GA_273AC0), GA(GA_2B76B0)};
    for (unsigned i = 0; i < 3; ++i) if (!sites[i] || !targets[i] || !call_targets(sites[i], targets[i])) return 1;
    if (!relay) relay = allocate_near(image + sites[0], 0x1000);
    if (!relay) return 2;
    DWORD old = 0;
    if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READWRITE, &old)) return 2;
    absolute_jump(relay, reinterpret_cast<void *>(&tick));
    absolute_jump(relay + 32, reinterpret_cast<void *>(&collect));
    bool ok = call_prepare(calls[0], sites[0], relay) && call_prepare(calls[1], sites[1], relay) &&
              call_prepare(calls[2], sites[2], relay + 32);
    if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READ, &old) || !FlushInstructionCache(GetCurrentProcess(), relay, 64)) ok = false;
    if (!ok) return 2;
    ready = true;
    return 0;
}

static void restore() { COST_SCOPE("siege_guard:restore");
    for (unsigned i = 3; i > 0; --i) call_restore(calls[i - 1]);
    ready = false;
}

// Each CALL is re-validated at publish; any failure restores the ones already written.
static bool publish() { COST_SCOPE("siege_guard:publish");
    if (!wanted()) return true;   // an off switch is not a publish failure
    bool ok = ready;
    for (unsigned i = 0; i < 3 && ok; ++i) {
        int delta = 0;
        memcpy(&delta, image + calls[i].site + 1, 4);
        ok = image[calls[i].site] == 0xE8 && delta == calls[i].original && call_publish(calls[i]);
    }
    if (!ok) restore();
#ifndef NO_RUNTIME_LOG
    log_event("siege_guard_installed", ok ? 1u : 0u, 0, 0);
#endif
    return ok;
}

}   // namespace siege_guard_hook
// [combat] siege_fort_first (user 2026-11-12, local74: "요새를 지을 수 있으면 진은 짓지마 ... 요새, 군악대, 진 우선순위").
// The original castle-siege node 61F10 builds through 72D30 (start) and 720E0 (resume), which only know the camp
// (kind 3, world+0x73970): they give every free member the build action 8 on one tile with Army+0x46 = 3.
// The four CALLs are wrapped. After the original returns with the stage "building" (+0x1F = 2) and no facility on the
// tile yet, the kind of those orders becomes the first of fort 4, band 7 that the region does not have, the force can
// pay, the spacing rule allows and the original permission 27A8D0 accepts for that tile and commander; otherwise the
// camp stays. Only the kind byte of an order the original just wrote is changed; tile, members and stages are the
// original's. Computer side only. Switch off or DLL unload = the CALLs are restored (off = original).
namespace camp_hook {
using Stage = void(__fastcall *)(Obj, Obj, Obj, Obj);
static CallLink calls[4];
static Obj relay;
static bool ready;

// why[0] fort, why[1] band: 0 chosen/not reached, 1 the region has one, 2 no definition, 3 money, 4 spacing, 5 original permission
// (build_possible: 27A8D0 with the unlock check on, local214/216).
static unsigned pick(Obj army, Obj tile, unsigned hex, unsigned *why) { COST_SCOPE("camp_hook:pick");
    Obj area = fn<Obj(__fastcall *)(Obj)>(GA(GA_22B8E0))(tile);
    Obj person = table(0x158, word(army, 0x12), 6000);
    Obj origin = fn<Obj(__fastcall *)(Obj)>(GA(GA_22BFE0))(army);
    Obj force = table(0xDF30, army_force(army), 51);
    if (!valid(area) || !valid(person) || !valid(origin) || !valid(force)) return 0;
    auto resource = *reinterpret_cast<int(__fastcall **)(Obj)>(pointer(origin, 0) + 0x90);
    const unsigned seen = local_facilities(army, area);
    GameList people; list_init(people, GA(GA_13466B0)); list_add(people, person);
    unsigned chosen = 0;
    unsigned slot = 0;
    for (unsigned kind : {4u, 7u}) {
        unsigned &reason = why[slot++];
        if (seen & (1u << kind)) { reason = 1; continue; }
        Obj definition = table(0x73958, kind, 30);
        if (!valid(definition)) { reason = 2; continue; }
        const int cost = fn<int(__fastcall *)(Obj, Obj)>(GA(GA_2AD6C0))(force, definition);
        if (cost < 0 || resource(origin) < cost) { reason = 3; continue; }
        if (military_spacing_blocked(army, kind, hex)) { reason = 4; continue; }
        if (!build_possible(&people, tile, definition)) { reason = 5; continue; }
        chosen = kind; break;
    }
    list_free(people);
    return chosen;
}
static void upgrade(Obj troops, Obj state) { COST_SCOPE("camp_hook:upgrade");
    if (!mod_features.siege_fort_first || !valid(troops) || !state || state[0x1F] != 2) return;
    const unsigned hex = dword(state, 0x20);
    if (hex >= 48400) return;
    Obj tile = table(0xE270, hex, 48399);
    if (!valid(tile) || valid(fn<Obj(__fastcall *)(Obj)>(GA(GA_2315A0))(tile))) return;   // a facility already stands there: its kind is fixed
    unsigned chosen = 0, why[2] = {}; bool decided = false;
    for (unsigned i = 1; i <= 500; ++i) {
        Obj a = table(0x7E838, i, 500);
        if (!valid(a) || table(0x7F8D8, word(a, 0x58), 500) != troops) continue;
        if (a[0x37] != 8 || a[0x46] != 3 || word(a, 0x42) != hex || !computer_side_army(a)) continue;
        if (!decided) { decided = true; chosen = pick(a, tile, hex, why); }
        if (!chosen) break;
        a[0x46] = static_cast<unsigned char>(chosen);
#ifndef NO_RUNTIME_LOG
        log_event("camp_upgrade", army_id(a), chosen, hex);
#endif
    }
#ifndef NO_RUNTIME_LOG
    if (decided && !chosen) { static unsigned kept; if (kept < 64) { ++kept; log_event("camp_kept", why[0], why[1], hex); } }
#endif
}
static void __fastcall start(Obj a, Obj troops, Obj c, Obj state) { COST_SCOPE("camp_hook:start");
    fn<Stage>(GA(GA_72D30))(a, troops, c, state); upgrade(troops, state);
}
static void __fastcall resume(Obj a, Obj troops, Obj c, Obj state) { COST_SCOPE("camp_hook:resume");
    fn<Stage>(GA(GA_720E0))(a, troops, c, state); upgrade(troops, state);
}
// 0 = prepared (or off), 1 = a CALL no longer targets its original function, 2 = prepare failed.
static unsigned prepare() { COST_SCOPE("camp_hook:prepare");
    for (auto &c : calls) c = CallLink{};
    ready = false;
    if (!mod_features.siege_fort_first) return 0;
    const unsigned sites[4] = {GA(GA_64F39), GA(GA_636A0), GA(GA_63819), GA(GA_64E72)};
    const unsigned targets[4] = {GA(GA_72D30), GA(GA_720E0), GA(GA_720E0), GA(GA_720E0)};
    for (unsigned i = 0; i < 4; ++i) if (!sites[i] || !targets[i] || !siege_guard_hook::call_targets(sites[i], targets[i])) return 1;
    if (!GA(GA_2315A0) || !GA(GA_22B8E0) || !GA(GA_27A8D0) || !GA(GA_2AD6C0) || !GA(GA_22BFE0)) return 1;
    if (!relay) relay = allocate_near(image + sites[0], 0x1000);
    if (!relay) return 2;
    DWORD old = 0;
    if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READWRITE, &old)) return 2;
    absolute_jump(relay, reinterpret_cast<void *>(&start));
    absolute_jump(relay + 32, reinterpret_cast<void *>(&resume));
    bool ok = call_prepare(calls[0], sites[0], relay);
    for (unsigned i = 1; i < 4 && ok; ++i) ok = call_prepare(calls[i], sites[i], relay + 32);
    if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READ, &old) || !FlushInstructionCache(GetCurrentProcess(), relay, 64)) ok = false;
    if (!ok) return 2;
    ready = true;
    return 0;
}
static void restore() { COST_SCOPE("camp_hook:restore");
    for (unsigned i = 4; i > 0; --i) call_restore(calls[i - 1]);
    ready = false;
}
static bool publish() { COST_SCOPE("camp_hook:publish");
    if (!mod_features.siege_fort_first) return true;
    bool ok = ready;
    for (unsigned i = 0; i < 4 && ok; ++i) {
        int delta = 0;
        memcpy(&delta, image + calls[i].site + 1, 4);
        ok = image[calls[i].site] == 0xE8 && delta == calls[i].original && call_publish(calls[i]);
    }
    if (!ok) restore();
#ifndef NO_RUNTIME_LOG
    log_event("camp_hook_installed", ok ? 1u : 0u, 0, 0);
#endif
    return ok;
}
}   // namespace camp_hook

// [combat] retake_adjacent, the group side (local88, user 2026-11-12 "적접근시 [공격]이 켜져있는것처럼 빨려들어가네 ... 공성부대에서
// 켜지는 메뉴처럼", "계속 똑같이 북쪽에서 나오고 원하는대로 움직이지 않아"). A retake unit rides in a group of kind 10 whose goal is
// its own castle. The original runs that kind through the castle-siege nodes (69AB0 / 61F10, San14Sim/docs/orig/ai/
// roles.md 6-1): every turn they clear the member relay points (B6B00), rewrite the member orders, and because the goal
// castle is not hostile they turn the group to kind 7, which the next run re-purposes, e.g. to an interception of a
// nearby army (replay 18:00: group 10/35 -> 1/4 on day 6). That is what pulled the units into the player's armies and
// wiped the departure's way out of the castle. The two CALLs that run a group's tree (4403E0 at 44079D, 440B30 at
// 4414D0 -> D4E20) are wrapped: for a group whose members are all live retake units the original tree is not run, and
// the members take the retake step instead. Any other group runs the original unchanged. Switch off = CALLs restored.
namespace retake_tree_hook {
using Tree = void(__fastcall *)(Obj, Obj);
static CallLink calls[2];
static Obj relay;
static bool ready;
static bool wanted() { return mod_features.retake_adjacent || mod_features.gate_intercept || mod_features.helm_intercept || mod_features.wall_age_siege; }
static bool retake_group(Obj troops) { COST_SCOPE("retake_tree:retake_group");
    if (!valid(troops)) return false;
    bool any = false;
    for (unsigned i = 1; i <= 500; ++i) {
        Obj a = table(0x7E838, i, 500);
        if (!valid(a) || !belongs_to_troops(a, troops)) continue;
        const Progress &p = progress[i];
        if (p.army != a || p.root != world() || p.commander != word(a, 0x12) || !p.regional_worker || !computer_side_army(a)) return false;
        // gate_intercept: an intercept unit is the mod's until it is home (its return order is the mod's too).
        if (p.intercept) { if (!(p.helm ? mod_features.helm_intercept : mod_features.gate_intercept)) return false; any = true; continue; }
        // wall_age_siege (docs/NEUTRAL_WALL_AGE.md 5절): a siege-weapon unit is the mod's until it is home too.
        if (p.siege_unit) { if (!mod_features.wall_age_siege) return false; any = true; continue; }
        if (!mod_features.retake_adjacent || !p.retake || p.forced_return || p.returning) return false;
        any = true;
    }
    return any;
}
static void __fastcall tree(Obj manager, Obj troops) { COST_SCOPE("retake_tree:tree");
    if (!enabled || !wanted() || in_policy || native_search_depth || !retake_group(troops)) {
        fn<Tree>(GA(GA_D4E20))(manager, troops);
        return;
    }
    ++in_policy;
    AcquireSRWLockExclusive(&policy_lock);
    for (unsigned i = 1; i <= 500; ++i) {
        Obj a = table(0x7E838, i, 500);
        if (!valid(a) || !belongs_to_troops(a, troops)) continue;
        Progress *p = observe_capture(a);
        if (!p) continue;
        if (p->intercept) { intercept_step(a, *p); continue; }
        if (p->siege_unit) { wall_siege_step(a, *p); continue; }
#ifndef NO_RUNTIME_LOG
        static volatile LONG lines;   // value = position, hex = first hex of the stored way (+0x48)
        if (InterlockedIncrement(&lines) <= 400) log_event("retake_tree_skipped", i, word(a, 0x2A), word(a, 0x48));
#endif
        // A planned supply cut (original cut node) or a build keeps its order; otherwise the retake step steers.
        if (!mod_features.retake_build && !p->facility) { p->credit = false; p->build_chain = false; }
        if ((dword(a, 0x1F0) & 8) || p->credit || p->build_chain || p->facility || word(a, 0x16) < 1000) continue;
        retake_step(a, *p);
    }
    ReleaseSRWLockExclusive(&policy_lock);
    --in_policy;
}
static unsigned prepare() { COST_SCOPE("retake_tree:prepare");
    calls[0] = calls[1] = CallLink{}; ready = false;
    if (!wanted()) return 0;
    const unsigned sites[2] = {GA(GA_44079D), GA(GA_4414D0)}, target = GA(GA_D4E20);
    if (!target || !siege_guard_hook::call_targets(sites[0], target) || !siege_guard_hook::call_targets(sites[1], target)) return 1;
    if (!relay) relay = allocate_near(image + sites[0], 0x1000);
    if (!relay) return 2;
    DWORD old = 0;
    if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READWRITE, &old)) return 2;
    absolute_jump(relay, reinterpret_cast<void *>(&tree));
    bool ok = call_prepare(calls[0], sites[0], relay) && call_prepare(calls[1], sites[1], relay);
    if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READ, &old) || !FlushInstructionCache(GetCurrentProcess(), relay, 32)) ok = false;
    if (!ok) return 2;
    ready = true;
    return 0;
}
static void restore() { COST_SCOPE("retake_tree:restore"); call_restore(calls[1]); call_restore(calls[0]); ready = false; }
static bool publish() { COST_SCOPE("retake_tree:publish");
    if (!wanted()) return true;
    bool ok = ready;
    for (unsigned i = 0; i < 2 && ok; ++i) {
        int delta = 0;
        memcpy(&delta, image + calls[i].site + 1, 4);
        ok = image[calls[i].site] == 0xE8 && delta == calls[i].original && call_publish(calls[i]);
    }
    if (!ok) restore();
#ifndef NO_RUNTIME_LOG
    log_event("retake_tree_installed", ok ? 1u : 0u, 0, 0);
#endif
    return ok;
}
}   // namespace retake_tree_hook
// [combat] ranged_facility_kill (user 2026-11-13 "원작의 안행진으로 원거리에서 망루나 투석대 요새등 군사시설 타격하면 체력이 1에서
// 더 안떨어지는 문제 ... 근접해서 공격하지 않아도 시설물 파괴하도록"). The original facility damage 2A0750(building, damage,
// attacker army) caps the damage at durability - 1 unless the attacker stands exactly at the touching distance
// (1 for a facility, 2 for a city/gate: 233FF0 -> EDI = 2 : 1):
//   2A09A4  3B C7        cmp eax,edi            ; eax = distance(attacker, building)
//           74 04        je  +4
//           45 8D 7D FF  lea r15d,[r13-1]       ; r13d = durability, r15d = the damage cap
// These eight bytes become a CALL to a stub that skips the cap when EDI == 1 (a facility: watchtower, catapult, fort,
// walls, traps), so ranged attacks can bring it to 0 and the original removal runs. Cities and gates keep the
// original cap. The following `cmp r14d,r15d` sets the flags again; the stub uses no stack. Off = bytes restored.
namespace ranged_facility_hook {
static CodeLink link;
static Obj relay;
static bool ready;
static constexpr unsigned char kSite[]={0x3B,0xC7,0x74,0x04,0x45,0x8D,0x7D,0xFF,0x45,0x3B,0xF7};
static constexpr unsigned char kStub[]={0x83,0xFF,0x01, 0x74,0x08, 0x3B,0xC7, 0x74,0x04, 0x45,0x8D,0x7D,0xFF, 0xC3};
static unsigned prepare() { COST_SCOPE("ranged_facility:prepare");
    link = CodeLink{}; ready = false;
    if (!mod_features.ranged_facility_kill) return 0;
    const unsigned site = GA(GA_2A09A4);
    if (!site || memcmp(image + site, kSite, sizeof(kSite))) return 1;
    if (!relay) relay = allocate_near(image + site, 0x1000);
    if (!relay) return 2;
    DWORD old = 0;
    if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READWRITE, &old)) return 2;
    memcpy(relay, kStub, sizeof(kStub));
    if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READ, &old) || !FlushInstructionCache(GetCurrentProcess(), relay, sizeof(kStub))) return 2;
    if (!prepare_link(link, site, 0xE8, relay)) return 3;
    const unsigned off = site - link.block;
    if (off + 8 > 16) return 3;
    memset(link.modified + off + 5, 0x90, 3);   // the CALL covers five of the eight bytes
    ready = true;
    return 0;
}
static void restore() { COST_SCOPE("ranged_facility:restore"); restore_link(link); ready = false; }
static bool publish() { COST_SCOPE("ranged_facility:publish");
    if (!mod_features.ranged_facility_kill) return true;
    const bool ok = ready && publish_link(link);
    log_event(ok ? "ranged_facility_installed" : "ranged_facility_rejected", ready ? 1u : 0u);
    return ok;
}
}   // namespace ranged_facility_hook
