// [combat] siege_guard (user 2026-11-10, local17). Computer siege-group field armies whose commander has
// effective intelligence >= 80 intercept player armies (direct or delegated corps) within 6 hexes (local43; was 5).
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
static Obj active_army;                 // army whose 273AC0 call currently runs with the transient option

using Tick = void(__fastcall *)(Obj);
using Collect = unsigned(__fastcall *)(Obj, int, Obj);

static bool player_side_army(Obj army) { COST_SCOPE("siege_guard:player_side_army");
    unsigned player = player_force();
    return player && valid(army) && army_force(army) == player;
}

static bool capable(Obj army) { COST_SCOPE("siege_guard:capable");
    if (!valid(army) || army[0x140] || !computer_side_army(army)) return false;
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

static bool player_near(Obj army) { COST_SCOPE("siege_guard:player_near");
    unsigned here = word(army, 0x2A);
    if (here >= 48400) return false;
    for (unsigned i = 0; i < player_count; ++i) {
        Obj other = table(0x7E838, players[i], 500);
        if (!valid(other) || other[0x140] || !player_side_army(other)) continue;
        unsigned hex = word(other, 0x2A);
        if (hex < 48400 && distance(here, hex) <= siege_guard::kRadius) return true;
    }
    return false;
}

// Wrapper for the two outer 273AC0 calls. Recursive 274EA9 calls run inside it untouched.
static void __fastcall tick(Obj army) { COST_SCOPE("siege_guard:tick");
    Tick original = fn<Tick>(GA(GA_273AC0));
    if (!enabled || !mod_features.siege_guard || !army || active_army) { original(army); return; }
    rebuild();
    if (!is_guard(army) || !player_near(army)) { original(army); return; }
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
static unsigned __fastcall collect(Obj army, int radius, Obj list) { COST_SCOPE("siege_guard:collect");
    Collect original = fn<Collect>(GA(GA_2B76B0));
    if (enabled && mod_features.siege_guard && army && army == active_army && radius < siege_guard::kRadius)
        radius = siege_guard::kRadius;
    return original(army, radius, list);
}

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
    if (!mod_features.siege_guard) return 0;
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
    if (!mod_features.siege_guard) return true;   // an off switch is not a publish failure
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
