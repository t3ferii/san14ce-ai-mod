// route_hook (U37/U39, user 2026-11-10, docs/COMBAT_REDESIGN.md 4-5절): single-writer corrections right after the
// original writes a group's routes. The original clears every member relay before each group tree run (B6B00) and
// rebuilds them in 61F10 -> CC160 (CALL 64F1B) and 65A60 -> CC160 / CAA60 (CALLs 667C7 / 667C0). These three CALLs are
// wrapped: the original runs first, then for each computer-side member (never the player's direct armies or delegated
// corps) at most one correction is written with the original setters:
//   [combat] morale    morale <= 85 (urgent <= 70 or mountain): hold at a completed own music band (2ECEA0(1) +
//                      2ECF00(9, hex next to the band)), or build one through the original build path.
//   [combat] path_wall neutral wall breach: a two-pass Dijkstra (walls as they are / a neutral wall crossable for its
//                      demolition time) per group, once per day at most; when the wall blocks the shortest way, the
//                      member nearest the wall takes the original facility attack order 2ECEA0(6) + 2ECF00(11, id).
// The original orders written just before a correction are kept in memory; the daily check (CALL 44E50D -> 2E6E00,
// time slot 0) writes them back when morale reaches 100, the band is gone or the wall has fallen, and the next
// original re-plan takes over. Memory only, nothing in the save. Switch off / DLL unload: the four CALLs are restored.
namespace route_hook {

using Route = uint64_t(__fastcall *)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
using Daily = void(__fastcall *)(int);

struct Orders {
    unsigned char action = 0, kind = 0;
    unsigned short id = 0, relay[5] = {48400, 48400, 48400, 48400, 48400};
};
enum Kind : unsigned char {none = 0, hold = 1, build = 2, breach = 3, ship = 4};
struct Entry {
    Obj root = nullptr, army = nullptr;
    unsigned commander = 0, troops = 0;
    Kind kind = none;
    unsigned band = 0, hex = 48400, wall = 0;   // band id / hold or build hex / wall building id (ship: the facility)
    unsigned goal = 0;                          // breach: the group goal (city/gate id) the wall blocks
    bool hit = false;                           // local108: breach for the hit response (no group goal)
    unsigned stand = 48400;                     // local226: the hex the army hits the wall / facility from (48400 = where it is)
    bool forced = false;                        // local226: no way around the wall, so it is broken even under tower fire
    unsigned repairer = 0;                      // local226 ship: the hostile army repairing the facility
    int day = 0;                                // local226 ship: start day
    Orders original, mine;
};
static Entry entries[501];


static Orders read(Obj a) {
    Orders o;
    o.action = a[0x37]; o.kind = a[0x38]; o.id = word(a, 0x3A);
    for (unsigned i = 0; i < 5; ++i) o.relay[i] = word(a, 0x2C + i * 2);
    return o;
}
static bool same(const Orders &x, const Orders &y) {
    if (x.action != y.action || x.kind != y.kind || x.id != y.id) return false;
    for (unsigned i = 0; i < 5; ++i) if (x.relay[i] != y.relay[i]) return false;
    return true;
}
static void write(Obj a, const Orders &o) {
    fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(a);
    fn<void(__fastcall *)(Obj, int)>(GA(GA_2ECEA0))(a, o.action);
    fn<void(__fastcall *)(Obj, int, int)>(GA(GA_2ECF00))(a, o.kind, o.id);
    for (unsigned i = 0; i < 5; ++i) fn<void(__fastcall *)(Obj, int, unsigned)>(GA(GA_2F05A0))(a, static_cast<int>(i), o.relay[i]);
    fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(a);
}
static void order(Obj a, int action, int kind, int id) {
    fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(a);
    fn<void(__fastcall *)(Obj, int)>(GA(GA_2ECEA0))(a, action);
    fn<void(__fastcall *)(Obj, int, int)>(GA(GA_2ECF00))(a, kind, id);
    fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(a);
}
static Entry *entry(Obj a) {
    unsigned id = army_id(a);
    if (!id) return nullptr;
    Entry &e = entries[id];
    if (e.kind != none && (e.root != world() || e.army != a || e.commander != word(a, 0x12))) e = Entry{};
    return &e;
}
// Used by the old campaign tick: a member holding at a band or breaching a wall takes no other mod order (a band
// build keeps running through the old facility progress).
static bool owns(Obj a) {
    if (!valid(a)) return false;
    unsigned id = army_id(a);
    return id && (entries[id].kind == hold || entries[id].kind == breach) && entries[id].root == world() && entries[id].army == a &&
           entries[id].commander == word(a, 0x12);
}
static void finish(Obj a, Entry &e, const char *why) {
    if (valid(a) && !a[0x140] && same(read(a), e.mine)) write(a, e.original);   // only our own correction is undone
    log_event(why, army_id(a), e.kind, e.hex);
    e = Entry{};
}

// ---- morale (U39: start <= 85) ----
static bool morale_target(Obj a) {
    if (!mod_features.morale || !route_hook_morale || !computer_side_army(a) || a[0x140]) return false;
    if (campaign_castle_locked(a) || word(a, 0x16) < 1000 || a[0x37] == 11 || a[0x37] == 12) return false;
    return true;
}
static bool urgent(Obj a) {
    if (a[0x1A] <= 70 || campaign_mountain(word(a, 0x2A))) return true;
    for (unsigned i = 0; i < 5; ++i) { unsigned h = word(a, 0x2C + i * 2); if (h < 48400 && campaign_mountain(h)) return true; }
    return false;
}
// Completed own band: within 5 hexes, or (urgent) within 15 and at most 5 hexes of detour toward the current goal.
static unsigned find_band(Obj a, unsigned goal) {
    const unsigned here = word(a, 0x2A);
    const bool hurry = urgent(a);
    unsigned best = 0; int best_range = 1 << 20;
    for (unsigned id = 1; id <= 3000; ++id) {
        if (!campaign_band(a, id)) continue;
        unsigned hex = word(table(0x6DA98, id, 3000), 0x10);
        int range = distance(here, hex);
        if (range > 5 && (!hurry || range > 15 || (goal < 48400 && distance(hex, goal) + range > distance(here, goal) + 5))) continue;
        if (range < best_range) { best_range = range; best = id; }
    }
    return best;
}
// The band's effect reaches two hexes: stay where we are when already in range, else the nearest own, non-mountain
// neighbour of the band.
static unsigned wait_hex(Obj a, unsigned band_hex) {
    const unsigned here = word(a, 0x2A);
    if (distance(here, band_hex) <= 2 && !campaign_mountain(here)) return here;
    unsigned best = 48400; int best_range = 1 << 20;
    for (unsigned d = 0; d < 6; ++d) {
        unsigned hex = sortie::neighbor(band_hex, d);
        Obj tile = table(0xE270, hex, 48399);
        if (!valid(tile) || tile[0x14] != army_force(a) || campaign_mountain(hex)) continue;
        int range = distance(here, hex);
        if (range < best_range) { best_range = range; best = hex; }
    }
    return best;
}
static void morale_member(Obj a, Entry &e) {
    if (!morale_target(a)) return;
    const unsigned morale = a[0x1A];
    if (e.kind == hold || e.kind == build) {
        // Our previous correction was replaced by this re-plan: re-apply it unless finished.
        if (morale >= 100 && e.kind == hold) { e = Entry{}; return; }
    } else if (morale > 85) return;   // local38 (user 2026-11-10 장료 105 on a mountain): recovery starts at <=85 only; urgent only widens find_band
    Progress *p = observe_capture(a);
    if (e.kind == build) {   // the band build runs through the old facility progress; wait until it ends
        if (p && p->facility == 7) return;
        e = Entry{};
    }
    const Orders fresh = read(a);
    unsigned goal = fresh.kind == 9 && fresh.id < 48400 ? fresh.id : 48400;
    for (unsigned i = 0; i < 5 && goal >= 48400; ++i) if (fresh.relay[i] < 48400) goal = fresh.relay[i];
    if (unsigned band = find_band(a, goal)) {
        unsigned hex = wait_hex(a, word(table(0x6DA98, band, 3000), 0x10));
        if (hex >= 48400) return;
        Entry next; next.root = world(); next.army = a; next.commander = word(a, 0x12); next.troops = word(a, 0x58);
        next.kind = hold; next.band = band; next.hex = hex; next.original = fresh;
        order(a, 1, 9, static_cast<int>(hex));
        next.mine = read(a);
        if (e.kind != hold || e.band != band) log_event("route_morale_hold", army_id(a), morale, hex);
        e = next;
        return;
    }
    // No band in reach: build one through the original build path (cost/site/permission checks in try_build).
    Obj area = army_area(a);
    if (!p || p->facility || !valid(area) || (local_facilities(a, area) & (1u << 7))) return;
    const bool credit = p->credit, chain = p->build_chain; const unsigned short center = p->build_center;
    p->build_chain = true; p->build_center = word(area, 0x3A);
    band_way_goal = goal;   // local211: sites on the way to the goal, no mountain
    const bool built = try_build(a, *p, 48400, 7);
    band_way_goal = 48400;
    p->credit = credit; p->build_chain = chain; p->build_center = center;
    if (!built) return;
    Entry next; next.root = world(); next.army = a; next.commander = word(a, 0x12); next.troops = word(a, 0x58);
    next.kind = build; next.hex = p->construction_hex; next.original = fresh; next.mine = read(a);
    log_event("route_morale_build", army_id(a), morale, next.hex);
    e = next;
}

// ---- neutral wall breach ----
static unsigned char grid[wall_path::cells];
static unsigned short grid_wall[wall_path::cells];   // neutral wall building id per hex
static unsigned short grid_cost[wall_path::cells];   // local38b: terrain movement cost (BF1B0, ~240 plain) per hex
static int grid_day = -1000000;
static Obj grid_root;
static unsigned grid_walls;
static bool neutral_wall(Obj b) {
    unsigned kind = b[0x12];
    return (kind == 9 || kind == 10 || kind == 16) && !b[0x13] && word(b, 0x14) && word(b, 0x10) < 48400;
}
// Once per day: terrain cost > 0 (BF1B0 of the tile terrain) is open; a non-city/gate facility of type class 2/3
// blocks (the original path search skips it whatever its owner); neutral walls are marked 2.
static void build_grid() { COST_SCOPE("route_hook:build_grid");
    Obj root = world();
    const int day = game_day();
    if (root == grid_root && day == grid_day) return;
    grid_root = root; grid_day = day; grid_walls = 0;
    for (unsigned h = 0; h < wall_path::cells; ++h) {
        grid_wall[h] = 0;
        Obj tile = table(0xE270, h, 48399);
        Obj terrain = valid(tile) ? fn<Obj(__fastcall *)(Obj)>(GA(GA_233310))(tile) : nullptr;
        const int cost = valid(terrain) ? fn<int(__fastcall *)(Obj)>(GA(GA_BF1B0))(terrain) : 0;
        grid[h] = cost > 0 ? neutral_breach::open_cell : neutral_breach::blocked;
        grid_cost[h] = static_cast<unsigned short>(cost > 0 ? (cost < 65535 ? cost : 65535) : 0);
    }
    for (unsigned id = 62; id <= 3000; ++id) {
        Obj b = table(0x6DA98, id, 3000);
        if (!valid(b) || !word(b, 0x14) || word(b, 0x10) >= 48400) continue;
        const unsigned hex = word(b, 0x10);
        if (neutral_wall(b)) { grid[hex] = neutral_breach::wall; grid_wall[hex] = static_cast<unsigned short>(id); ++grid_walls; continue; }
        Obj type = fn<Obj(__fastcall *)(Obj)>(GA(GA_EDF0))(b);
        if (valid(type) && (type[0x2C] == 2 || type[0x2C] == 3)) grid[hex] = neutral_breach::blocked;
    }
}
struct GroupPlan { Obj root = nullptr; int day = -1000000; unsigned goal = 0; unsigned wall = 0; };
static GroupPlan plans[501];
static bool wall_near(unsigned from, unsigned goal) {
    const int x0 = static_cast<int>(from % 220), y0 = static_cast<int>(from / 220);
    const int x1 = static_cast<int>(goal % 220), y1 = static_cast<int>(goal / 220);
    const int lx = (std::min)(x0, x1) - 15, hx = (std::max)(x0, x1) + 15, ly = (std::min)(y0, y1) - 15, hy = (std::max)(y0, y1) + 15;
    for (unsigned id = 62; id <= 3000; ++id) {
        Obj b = table(0x6DA98, id, 3000);
        if (!valid(b) || !neutral_wall(b)) continue;
        const int x = word(b, 0x10) % 220, y = word(b, 0x10) / 220;
        if (x >= lx && x <= hx && y >= ly && y <= hy) return true;
    }
    return false;
}
// ---- local226 stands: outside hostile tower range (path_wall / hit response), and the 누선 repair strike ----
// Tower ranges are the original ones (KB rules_domestic 4, 192110 / 2AD8F0: 투석대 2..3, 망루 1..2 hexes, finished
// facilities only). The army's reach is the original combat radius (2C9080(force, formation) != 0) + 1 (KB
// rules_combat 1: 안행·누선 2, others 1). A stand two hexes away keeps the army on the same land/water kind, so its
// formation (and with it the radius) does not change on the way.
static bool water_hex(unsigned hex) {
    Obj t = hex < 48400 ? table(0xE270, hex, 48399) : nullptr;
    return valid(t) && (t[0x10] == 14 || t[0x10] == 24);
}
static int strike_reach(Obj a) {
    Obj force = fn<Obj(__fastcall *)(Obj)>(GA(GA_22E2A0))(a);
    if (!valid(force)) return 1;
    Obj formation = fn<Obj(__fastcall *)(Obj)>(GA(GA_22CAD0))(a);
    return fn<int(__fastcall *)(Obj, Obj)>(GA(GA_2C9080))(force, formation) ? 2 : 1;
}
static std::vector<tower_reach::Tower> hostile_towers(unsigned own) {
    std::vector<tower_reach::Tower> out;
    if (!own) return out;
    for (unsigned id = 62; id <= 3000; ++id) {
        Obj b = table(0x6DA98, id, 3000);
        if (!valid(b) || !word(b, 0x14) || word(b, 0x10) >= 48400 || !(b[0x16] & 1)) continue;
        const unsigned kind = b[0x12], holder = b[0x13];
        if ((kind != 5 && kind != 6) || !holder || holder == own) continue;
        if (!fn<int(__fastcall *)(unsigned, unsigned)>(GA(GA_2CC630))(own, holder)) continue;
        out.push_back({word(b, 0x10), static_cast<unsigned char>(kind == 6 ? 2 : 1), static_cast<unsigned char>(kind == 6 ? 3 : 2)});
    }
    return out;
}
// The hex outside every hostile tower range the army hits the wall from (its own hex when that already does), or 48400.
static unsigned wall_stand(Obj a, unsigned wall_hex, const std::vector<tower_reach::Tower> &towers) {
    if (!valid(a) || wall_hex >= 48400) return 48400;
    const unsigned here = word(a, 0x2A);
    const bool wet = water_hex(here);
    auto open = [&](unsigned h) { return intercept_open(h, a) && (distance(h, wall_hex) < 2 || water_hex(h) == wet); };
    const unsigned s = tower_reach::stand(wall_hex, here < 48400 ? here : wall_path::cells, strike_reach(a), towers, open);
    return s < 48400 ? s : 48400;
}
static unsigned wall_stand(Obj a, unsigned wall_hex) { return valid(a) ? wall_stand(a, wall_hex, hostile_towers(army_force(a))) : 48400; }
static bool wall_has_stand(Obj a, unsigned wall_hex) { return wall_stand(a, wall_hex) < 48400; }   // refresh_path_wall
// The breach order: attack the wall, walking over the stand first when the army is not on it.
static void breach_write(Obj a, Entry &e) {
    Orders o; o.action = 6; o.kind = 11; o.id = static_cast<unsigned short>(e.wall);
    if (e.stand < 48400 && e.stand != word(a, 0x2A)) o.relay[0] = static_cast<unsigned short>(e.stand);
    write(a, o);
    e.mine = read(a);
}

// [combat] ship_repair_strike (local226, user 2026-11-18 "컴퓨터가 누선일때 플레이어가 수리하면 달라붙어서 플레이어 수리하는
// 부대도 동시에 쳤으면해 누선 사거리가 안행처럼 주변 2헥스"): a computer-side 누선 (water formation 13 on 대하/격류) attacking a
// hostile facility (original order 6/11) within 8 hexes, while a hostile army stands next to that facility repairing it
// (order 8 or 9 on it), moves to a water hex within 2 of both, so the original combat (every hostile unit and facility
// within the radius, up to 3 records) hits the facility and the repairer together. Hexes outside hostile tower range
// first, then the nearest. Held until the repair stops, the facility falls, the ship leaves the water or its 누선
// formation, retreats, or 10 days pass. Player direct armies and delegated corps never take it.
static bool ship_ready(Obj a) {
    if (!mod_features.ship_repair_strike || !valid(a) || a[0x140] || !computer_side_army(a) || word(a, 0x16) < 1000) return false;
    if (a[0x1C] != 13 || !water_hex(word(a, 0x2A)) || a[0x37] == 11 || a[0x37] == 12 || campaign_castle_locked(a)) return false;
    return strike_reach(a) >= 2;
}
static Obj ship_repairer(unsigned own, unsigned facility, unsigned facility_hex) {
    for (unsigned i = 1; i <= 500; ++i) {
        Obj r = table(0x7E838, i, 500);
        if (!valid(r) || word(r, 0x2A) >= 48400 || !word(r, 0x16)) continue;
        if ((r[0x37] != 8 && r[0x37] != 9) || r[0x38] != 11 || word(r, 0x3A) != facility) continue;
        if (distance(word(r, 0x2A), facility_hex) > 1) continue;
        if (!fn<int(__fastcall *)(unsigned, unsigned)>(GA(GA_2CC630))(own, army_force(r))) continue;
        return r;
    }
    return nullptr;
}
static Obj ship_facility(Obj a) {
    if (a[0x37] != 6 || a[0x38] != 11) return nullptr;
    const unsigned id = word(a, 0x3A);
    Obj f = id && id <= 3000 ? table(0x6DA98, id, 3000) : nullptr;
    if (!valid(f) || !word(f, 0x14) || word(f, 0x10) >= 48400 || !f[0x13]) return nullptr;
    return fn<int(__fastcall *)(unsigned, unsigned)>(GA(GA_2CC630))(army_force(a), f[0x13]) ? f : nullptr;
}
static void ship_check(Obj a) { COST_SCOPE("route_hook:ship_check");
    if (!ship_ready(a)) return;
    Entry *e = entry(a);
    if (!e || e->kind != none) return;
    Obj f = ship_facility(a);
    if (!f) return;
    const unsigned fid = word(a, 0x3A), fhex = word(f, 0x10), here = word(a, 0x2A), own = army_force(a);
    if (distance(here, fhex) > 8) return;
    Obj r = ship_repairer(own, fid, fhex);
    if (!r) return;
    const unsigned rhex = word(r, 0x2A);
    if (distance(here, fhex) <= 2 && distance(here, rhex) <= 2) return;   // already hits both
    const std::vector<tower_reach::Tower> towers = hostile_towers(own);
    auto open = [&](unsigned h) { return water_hex(h) && intercept_open(h, a); };
    const unsigned s = tower_reach::stand(fhex, here, 2, towers, open, rhex, false);
    if (s >= 48400) {
        static volatile LONG misses;
        if (InterlockedIncrement(&misses) <= 200) log_event("ship_repair_no_stand", army_id(a), fid, rhex);
        return;
    }
    Entry next; next.root = world(); next.army = a; next.commander = word(a, 0x12); next.troops = word(a, 0x58);
    next.kind = ship; next.wall = fid; next.hex = fhex; next.repairer = army_id(r); next.stand = s; next.day = game_day();
    next.original = read(a);
    breach_write(a, next);
    *e = next;
    log_event("ship_repair_strike", army_id(a), fid | (army_id(r) << 16), s);
}
// Keeps or ends a ship entry; true while it holds.
static bool ship_keep(Obj a, Entry &e) {
    Obj r = e.repairer && e.repairer <= 500 ? table(0x7E838, e.repairer, 500) : nullptr;
    Obj f = table(0x6DA98, e.wall, 3000);
    const bool repairing = valid(r) && word(r, 0x2A) < 48400 && (r[0x37] == 8 || r[0x37] == 9) && r[0x38] == 11 &&
                           word(r, 0x3A) == e.wall && valid(f) && word(f, 0x14) && word(f, 0x10) == e.hex;
    const bool alive = mod_features.ship_repair_strike && a[0x1C] == 13 && water_hex(word(a, 0x2A)) && a[0x37] != 11 && a[0x37] != 12 &&
                       word(a, 0x16) >= 1000 && game_day() - e.day <= 10;
    if (repairing && alive) return true;
    log_event("ship_repair_done", army_id(a), e.wall, repairing ? 2u : 1u);
    finish(a, e, "ship_repair_released");
    return false;
}

static unsigned plain_hexes(unsigned c) { return c == neutral_breach::none ? 65535u : (c / 240u > 65534u ? 65534u : c / 240u); }
// local108 (user 2026-11-13 "중립건물 돌파에 이 기능을 자연스럽게 넣어줄 수 있나? 2칸이상 돌아가면 부수면 좋겠어", "망루나
// 투석대도"): the hit response is a breach entry. A computer-side army hit within 10 days by the player's 안행 / water
// formation army or military facility (망루·투석대·망대) with a neutral wall between them (ranged_wall) compares the way
// to the shooter around the walls with the way as if no neutral wall stood (terrain-weighted, as breach_group). It breaks
// the first wall on the wall-free way only when going around costs at least 2 plain hexes more (or is impossible);
// otherwise the original AI walks around. The order is held like a breach until the wall falls, the hit gets older than
// 20 days or the shooter stops qualifying. Once per army per day.
static constexpr unsigned kHitDetour = 2;
struct HitTry { Obj root = nullptr; int day = -1000000; };
static HitTry hit_tries[501];
static void hit_check(Obj a) { COST_SCOPE("route_hook:hit_check");
    if (!route_hook_hit || !valid(a) || a[0x140] || !computer_side_army(a) || word(a, 0x16) < 1000 || word(a, 0x2A) >= 48400) return;
    if (campaign_castle_locked(a) || a[0x37] == 11 || a[0x37] == 12 || !ranged_hit_ready(a)) return;
    const unsigned id = army_id(a);
    if (!id || id > 500) return;
    Entry *e = entry(a);
    if (!e || e->kind != none) return;
    HitTry &t = hit_tries[id];
    const int day = game_day();
    if (t.root == world() && t.day == day) return;
    t.root = world(); t.day = day;
    bool between = false;
    for (unsigned w = 62; w <= 3000 && !between; ++w) {
        Obj b = table(0x6DA98, w, 3000);
        between = valid(b) && neutral_wall(b) && ranged_wall(a, b);
    }
    if (!between) return;
    build_grid();
    const unsigned from = word(a, 0x2A), to = ranged_hit_source(a);
    const neutral_breach::Plan plan = neutral_breach::plan(grid, from, to, 0, grid_cost, 240);
    const unsigned wall_id = plan.wall_hex < wall_path::cells ? grid_wall[plan.wall_hex] : 0u;
    log_event("route_hit_plan", id, plain_hexes(plan.closed), plain_hexes(plan.free) | (wall_id << 16));
    if (!wall_id || plan.free == neutral_breach::none || distance(from, plan.wall_hex) > 10) return;
    if (plan.closed != neutral_breach::none && plan.closed < plan.free + kHitDetour * 240u) return;   // walk around
    Obj wall = table(0x6DA98, wall_id, 3000);
    if (!valid(wall) || !neutral_wall(wall)) return;
    // local226: the wall is broken from outside hostile tower range; with no such stand the army walks around, unless
    // there is no way around at all (then as before).
    const bool forced = plan.closed == neutral_breach::none;
    const unsigned stand = wall_stand(a, plan.wall_hex);
    if (stand >= 48400 && !forced) { log_event("route_breach_no_stand", id, wall_id, plan.wall_hex); return; }
    Entry next; next.root = world(); next.army = a; next.commander = word(a, 0x12); next.troops = word(a, 0x58);
    next.kind = breach; next.hit = true; next.wall = wall_id; next.hex = plan.wall_hex; next.goal = 0; next.original = read(a);
    next.stand = stand; next.forced = forced;
    breach_write(a, next);
    *e = next;
    log_event("route_breach_hit", id, wall_id | (static_cast<unsigned>(wall[0x12]) << 16), plan.wall_hex);
    log_event("route_breach_stand", id, stand | (forced ? (1u << 16) : 0u), plan.wall_hex);
}
static void breach_group(Obj troops, unsigned tid) { COST_SCOPE("route_hook:breach_group");
    if (!mod_features.path_wall || !route_hook_breach || !tid || tid > 500) return;
    const unsigned mode = word(troops, 0x14), goal_id = word(troops, 0x16);
    if (!attack_mode(mode) || !goal_id || goal_id > 61) return;
    // Already breaching for this group: keep the assignment (re-applied per member below).
    for (unsigned i = 1; i <= 500; ++i)
        if (entries[i].kind == breach && entries[i].troops == tid && entries[i].root == world()) return;
    GroupPlan &gp = plans[tid];
    const int day = game_day();
    if (gp.root == world() && gp.day == day && gp.goal == goal_id) return;   // decided today already
    gp.root = world(); gp.day = day; gp.goal = goal_id; gp.wall = 0;
    Obj castle = campaign_goal(goal_id);
    if (!castle) return;
    const unsigned goal = fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(castle);
    // Representative = the strongest computer-side member that stands on the map.
    Obj lead = nullptr;
    for (unsigned i = 1; i <= 500; ++i) {
        Obj a = table(0x7E838, i, 500);
        if (!valid(a) || a[0x140] || !belongs_to_troops(a, troops) || !computer_side_army(a) || word(a, 0x2A) >= 48400) continue;
        if (!lead || word(a, 0x16) > word(lead, 0x16)) lead = a;
    }
    if (!lead || goal >= 48400 || !wall_near(word(lead, 0x2A), goal)) return;
    build_grid();
    if (!grid_walls) return;
    // Penalty from the first neutral wall's durability and the lead's original daily facility damage (2AC5A0).
    Obj formation = fn<Obj(__fastcall *)(Obj)>(GA(GA_22CAD0))(lead);
    const float damage_f = fn<float(__fastcall *)(Obj, Obj, int, int)>(GA(GA_2AC5A0))(lead, formation, 1, 0);
    const int damage = damage_f > 0 ? static_cast<int>(damage_f + 0.5f) : 0;
    unsigned probe = neutral_breach::none;
    neutral_breach::search(grid, word(lead, 0x2A), goal, 0, true, &probe, grid_cost);
    if (probe == neutral_breach::none) {   // no neutral wall on the free way (terrain-weighted, as the original path search)
        log_event("route_breach_none", tid, word(lead, 0x2A), goal);
        return;
    }
    Obj wall = table(0x6DA98, grid_wall[probe], 3000);
    if (!valid(wall)) return;
    const unsigned penalty = neutral_breach::penalty(word(wall, 0x14), damage > 0 ? static_cast<unsigned>(damage) : 1u);
    // local38b: costs are the original terrain movement costs (plain ~240 per hex), so a slow land detour and a fast
    // river/sea way compare as the original path search does.
    const neutral_breach::Plan plan = neutral_breach::plan(grid, word(lead, 0x2A), goal, penalty, grid_cost, 240);
    // In plain-hex units: value = way around the walls, hex = (wall-free way) | (through the wall with the demolition penalty << 16)
    auto hexes = [](unsigned c) { return c == neutral_breach::none ? 65535u : (c / 240u > 65534u ? 65534u : c / 240u); };
    log_event("route_breach_plan", tid, hexes(plan.closed), hexes(plan.free) | (hexes(plan.open) << 16));
    if (!neutral_breach::breach(plan)) return;
    gp.wall = grid_wall[plan.wall_hex];
    // Breaker = the member nearest the wall (any army may cross water by boat). local226: it must have a stand outside
    // hostile tower range (wall_stand); none = the group walks around, unless no way around exists (then as before).
    const bool forced = plan.closed == neutral_breach::none;
    const std::vector<tower_reach::Tower> towers = hostile_towers(army_force(lead));
    Obj breaker = nullptr; int best = 1 << 20; unsigned breaker_stand = 48400;
    for (unsigned i = 1; i <= 500; ++i) {
        Obj a = table(0x7E838, i, 500);
        if (!valid(a) || a[0x140] || !belongs_to_troops(a, troops) || !computer_side_army(a) || word(a, 0x16) < 1000) continue;
        if (owns(a)) continue;
        int range = distance(word(a, 0x2A), plan.wall_hex);
        if (range >= best) continue;
        const unsigned s = wall_stand(a, plan.wall_hex, towers);
        if (s >= 48400 && !forced) continue;
        best = range; breaker = a; breaker_stand = s;
    }
    if (!breaker) {
        log_event("route_breach_no_stand", tid, gp.wall, plan.wall_hex);
        gp.wall = 0;
        return;
    }
    Entry *e = entry(breaker);
    if (!e) return;
    Entry next; next.root = world(); next.army = breaker; next.commander = word(breaker, 0x12); next.troops = tid;
    next.kind = breach; next.wall = gp.wall; next.hex = plan.wall_hex; next.goal = goal_id; next.original = read(breaker);
    next.stand = breaker_stand; next.forced = forced;
    breach_write(breaker, next);
    *e = next;
    log_event("route_breach_start", army_id(breaker), gp.wall | (static_cast<unsigned>(table(0x6DA98, gp.wall, 3000)[0x12]) << 16), plan.wall_hex);   // value = wall id | kind (9 토성 / 10 석벽 / 16 철쇄) << 16
    log_event("route_breach_stand", army_id(breaker), breaker_stand | (forced ? (1u << 16) : 0u), plan.wall_hex);
}
// local55 (user 2026-11-11 "출진하는 5번째 부대마다 중립건물 파괴명령 코드 다시 부활", "부수다가 뒤로가다가"): the
// every-fifth-army pick (refresh_path_wall, PW14/PW15) comes back, but its order is no longer written once from the
// mod tick, where the original group run (61F10 / route writes) rewrote it the same turn and the army swung between
// the wall and the castle way (old log 2026-11-09: destroy_path_wall, 10 days of the original way, destroy again).
// It now takes a breach entry here, which is put back after every original route write and on the daily pass until
// the wall falls, the group goal changes or the army retreats (local41/42 rules).
static bool assign_fifth(Obj a, unsigned wall_hex) {
    if (!mod_features.path_wall || !route_hook_breach || !valid(a) || a[0x140] || !computer_side_army(a) || word(a, 0x16) < 1000) return false;
    Obj troops = table(0x7F8D8, word(a, 0x58), 500);
    if (!valid(troops) || !attack_mode(word(troops, 0x14))) return false;
    const unsigned goal = word(troops, 0x16);
    if (!goal || goal > 61 || campaign_castle_locked(a) || a[0x37] == 11 || a[0x37] == 12) return false;
    unsigned wall = 0;
    for (unsigned id = 1; id <= 3000 && !wall; ++id) {
        Obj b = table(0x6DA98, id, 3000);
        if (valid(b) && neutral_wall(b) && word(b, 0x10) == wall_hex) wall = id;
    }
    if (!wall) return false;
    Entry *e = entry(a);
    if (!e || e->kind != none) return false;
    const unsigned stand = wall_stand(a, wall_hex);   // local226: never under hostile tower fire (refresh_path_wall picks such walls only)
    if (stand >= 48400) { log_event("route_breach_no_stand", army_id(a), wall, wall_hex); return false; }
    Entry next; next.root = world(); next.army = a; next.commander = word(a, 0x12); next.troops = word(a, 0x58);
    next.kind = breach; next.wall = wall; next.hex = wall_hex; next.goal = goal; next.original = read(a);
    next.stand = stand;
    breach_write(a, next);
    *e = next;
    log_event("route_breach_fifth", army_id(a), wall | (static_cast<unsigned>(table(0x6DA98, wall, 3000)[0x12]) << 16), wall_hex);
    log_event("route_breach_stand", army_id(a), stand, wall_hex);
    return true;
}
static bool wall_standing(const Entry &e) {
    Obj b = table(0x6DA98, e.wall, 3000);
    return valid(b) && neutral_wall(b) && word(b, 0x10) == e.hex;
}

// ---- supply relay (local46, user 2026-11-11 "목표가 바뀔 때만, 한 거점 뒤 아군 칸을 밟고 병참을 이어서") ----
// Original region-officer land take (25FA40 plan -> 260710 apply, turn processing; San14Sim/docs/orig/rules_domestic.md
// 4): a region whose officer (+0x36) serves the region owner (centre tile +0x3A) takes back the tiles nearest its
// centre unless ANY unit hostile to the owner stands inside the region. So when a computer army changes its target
// while it is the last unit inside a hostile officer region and the new way leaves that region within one turn, the
// tiles it painted flip at turn end and the army is cut off alone. At that change only, one own tile in an own region
// behind (radius 6) is put in front of the original relays; the target itself stays as the original wrote it. The
// relay is kept until the army steps on it, the target changes again or 10 days pass. Nothing in the save.
static unsigned packed_order(Obj a) { return (static_cast<unsigned>(a[0x37]) << 24) | (static_cast<unsigned>(a[0x38]) << 16) | word(a, 0x3A); }
struct Bridge {
    Obj root = nullptr, army = nullptr;
    unsigned commander = 0, last = 0, target = 0, tile = 48400;
    int day = 0;
};
static Bridge bridges[501];
static constexpr int kTurnReach = 10;     // hexes an army covers in one turn (dump P90, rules_movement.md 5)
static constexpr int kRegionRadius = 4;   // a region reaches about 4 hexes from any of its tiles to its edge
static constexpr int kBehind = 6;         // search radius for the own tile behind
static Obj tile_at(unsigned hex) { return hex < 48400 ? table(0xE270, hex, 48399) : nullptr; }
static Obj region_at(unsigned hex) {
    Obj t = tile_at(hex);
    return valid(t) ? fn<Obj(__fastcall *)(Obj)>(GA(GA_22B8E0))(t) : nullptr;
}
static unsigned region_owner(Obj r) { Obj c = tile_at(word(r, 0x3A)); return valid(c) ? c[0x14] : 0u; }
static bool hostile(unsigned a, unsigned b) {
    return a && b && a != b && fn<int(__fastcall *)(unsigned, unsigned)>(GA(GA_2CC630))(a, b) != 0;
}
static bool officer_takes(Obj r, unsigned owner) {
    const unsigned officer = word(r, 0x36);
    if (!officer || officer > 6000) return false;
    Obj p = table(0x158, officer, 6000);
    return valid(p) && fn<unsigned(__fastcall *)(Obj)>(GA(GA_22E870))(p) == owner;
}
static bool last_in_region(Obj a, Obj r, unsigned owner) {
    for (unsigned i = 1; i <= 500; ++i) {
        Obj x = table(0x7E838, i, 500);
        if (!valid(x) || x == a || region_at(word(x, 0x2A)) != r) continue;
        if (hostile(army_force(x), owner)) return false;
    }
    return true;
}
// Planned points in order: the relays, then the target hex (hex target or facility). Returns the first point outside
// the region when the army is out of it within one turn (distance to the last point inside + region radius <= reach).
static unsigned leaving_point(Obj a, Obj r) {
    unsigned points[6]; unsigned n = 0;
    for (unsigned i = 0; i < 5; ++i) { unsigned h = word(a, 0x2C + i * 2); if (h < 48400) points[n++] = h; }
    const unsigned kind = a[0x38], id = word(a, 0x3A);
    if (kind == 9 && id < 48400) points[n++] = id;
    else if (kind == 11 && id && id <= 3000) { Obj b = table(0x6DA98, id, 3000); if (valid(b) && word(b, 0x10) < 48400) points[n++] = word(b, 0x10); }
    unsigned prev = word(a, 0x2A); int inside = 0;
    for (unsigned i = 0; i < n; ++i) {
        if (region_at(points[i]) != r) return inside + kRegionRadius <= kTurnReach ? points[i] : 48400u;
        inside += distance(prev, points[i]); prev = points[i];
    }
    return 48400;
}
// wet (local269): a 조타 ship's tile behind is river water (helm_water terrain 13, 14, 24) only; a land tile took it ashore.
static unsigned behind_tile(Obj a, Obj r, unsigned force, unsigned toward, bool wet = false) {
    const unsigned here = word(a, 0x2A);
    static unsigned queue[256]; unsigned head = 0, tail = 0;
    queue[tail++] = here;
    unsigned best = 48400; int best_score = 1 << 20;
    while (head < tail) {
        const unsigned hex = queue[head++];
        Obj t = tile_at(hex);
        if (valid(t) && t[0x14] == force && (!wet || t[0x10] == 13 || t[0x10] == 14 || t[0x10] == 24)) {
            Obj tr = region_at(hex);
            if (valid(tr) && tr != r && region_owner(tr) == force && !campaign_mountain(hex)) {
                const int score = distance(here, hex) + distance(hex, toward);
                if (score < best_score) { best_score = score; best = hex; }
            }
        }
        for (unsigned d = 0; d < 6 && tail < 256; ++d) {
            const unsigned next = sortie::neighbor(hex, d);
            if (next >= 48400 || distance(here, next) > kBehind) continue;
            bool seen = false;
            for (unsigned k = 0; k < tail && !seen; ++k) seen = queue[k] == next;
            if (!seen) queue[tail++] = next;
        }
    }
    return best;
}
static void put_relay(Obj a, unsigned hex) {
    Orders o = read(a);
    if (o.relay[0] == hex) return;
    for (unsigned i = 4; i > 0; --i) o.relay[i] = o.relay[i - 1];
    o.relay[0] = static_cast<unsigned short>(hex);
    write(a, o);
}
static void supply_check(Obj a) { COST_SCOPE("route_hook:supply_check");
    if (!mod_features.supply_relay || !valid(a) || a[0x140] || !computer_side_army(a)) return;
    const unsigned id = army_id(a);
    if (!id || id > 500) return;
    if (progress[id].army == a && progress[id].regional_worker && progress[id].retake) return;   // local85: a retake unit writes its own relays
    // local263 (user 2026-11-19 "병참차단 하고 목표 바뀌면 가까운 아군타일 중계점 찍은다음 이동하고 (모드에서 점령하는 로직만 뺀)"): a
    // 조타 ship gets the own tile behind as before (v16.3.1) but never the local53 base capture below (local262 had dropped both;
    // 18:41:57 supply_relay_capture army 33 put a ship onto a land base).
    const bool helm_ship = progress[id].army == a && progress[id].regional_worker && progress[id].helm;
    Bridge &b = bridges[id];
    const unsigned now = packed_order(a);
    if (b.root != world() || b.army != a || b.commander != word(a, 0x12)) {   // first sight: no change yet
        b = Bridge{}; b.root = world(); b.army = a; b.commander = word(a, 0x12); b.last = now;
        return;
    }
    const int today = game_day();
    if (b.tile < 48400) {
        const bool done = word(a, 0x2A) == b.tile;
        if (done || today - b.day > 10 || a[0x37] == 11 || a[0x37] == 12) {
            log_event(done ? "supply_relay_done" : "supply_relay_expired", id, b.tile, now);
            b.tile = 48400;
        } else if (now == b.target) {
            if (word(a, 0x2C) != b.tile) { put_relay(a, b.tile); log_event("supply_relay_reapply", id, b.tile, now); }
            b.last = now;
            return;
        } else b.tile = 48400;   // the target changed again: judge the new change below
    }
    if (now == b.last) return;
    b.last = now;
    if (a[0x37] == 11 || a[0x37] == 12 || campaign_castle_locked(a) || entries[id].kind != none) return;
    const unsigned here = word(a, 0x2A);
    Obj r = region_at(here);
    if (!valid(r)) return;
    const unsigned owner = region_owner(r), force = army_force(a);
    if (!hostile(force, owner) || !officer_takes(r, owner) || !last_in_region(a, r, owner)) return;
    const unsigned out = leaving_point(a, r);
    if (out >= 48400) return;
    const unsigned hex = behind_tile(a, r, force, out, helm_ship);
    // local53 (user 2026-11-11 "자동점령이 발생하는 지역 거점을 지역 버프 잇는것보다 먼저 할 수 있다면. 동률이어도 점령이
    // 우선"): when the region's own base hex (no building on it, not a mountain, within the 6-hex search) is no farther than
    // the own tile behind, the relay goes over that base instead; the army takes the region before the officer's land take
    // and then follows its new mission.
    const unsigned base = word(r, 0x3A);
    Obj base_tile = base < 48400 ? tile_at(base) : nullptr;
    if (!helm_ship && valid(base_tile) && !word(base_tile, 0x1A) && base_tile[0x14] == owner && !campaign_mountain(base) &&
        distance(here, base) <= kBehind && (hex >= 48400 || distance(here, base) <= distance(here, hex))) {
        put_relay(a, base);
        b.tile = base; b.target = now; b.day = today;
        log_event("supply_relay_capture", id, base, (owner << 16) | (hex < 48400 ? distance(here, hex) : 255u));
        return;
    }
    if (hex >= 48400) { log_event("supply_relay_none", id, word(r, 0x3A), now); return; }
    put_relay(a, hex);
    b.tile = hex; b.target = now; b.day = today;
    log_event("supply_relay_start", id, hex, (owner << 16) | word(r, 0x3A));
}

// ---- after each original route write ----
static __declspec(thread) unsigned depth;
static void after(Obj troops) { COST_SCOPE("route_hook:after");
    if (!enabled || !valid(troops) || depth) return;
    ++depth;
    unsigned tid = 0;
    for (unsigned i = 1; i <= 500; ++i) if (table(0x7F8D8, i, 500) == troops) { tid = i; break; }
    if (tid) breach_group(troops, tid);
    bool fallen = false;
    for (unsigned i = 1; i <= 500 && tid; ++i) {
        Obj a = table(0x7E838, i, 500);
        if (!valid(a) || a[0x140] || !belongs_to_troops(a, troops) || !computer_side_army(a)) continue;
        Entry *e = entry(a);
        if (!e) continue;
        if (e->kind == none) hit_check(a);   // local108
        if (e->kind == breach) {
            const bool hit = e->hit;
            if ((hit ? !route_hook_hit : !mod_features.path_wall) || !wall_standing(*e)) {
                finish(a, *e, "route_breach_done"); if (!hit) fallen = true; continue;
            }
            if (hit ? !ranged_hit_ready(a, 20) : (!attack_mode(word(troops, 0x14)) || word(troops, 0x16) != e->goal)) {
                finish(a, *e, "route_breach_released"); continue;
            }
            const Orders now = read(a);
            if (!same(now, e->mine)) { e->original = now; breach_write(a, *e); }   // local226: keeps the stand relay
            continue;
        }
        if (e->kind == ship) {   // local226 ship_repair_strike
            if (ship_keep(a, *e)) { const Orders now = read(a); if (!same(now, e->mine)) { e->original = now; breach_write(a, *e); } }
            continue;
        }
        if (e->kind == none) ship_check(a);
        if (e->kind == ship) continue;
        morale_member(a, *e);
        supply_check(a);
    }
    if (fallen && mod_features.path_wall) { plans[tid].day = -1000000; breach_group(troops, tid); }   // local42: next wall at once
    --depth;
}
static uint64_t __fastcall route(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
    const uint64_t result = fn<Route>(GA(GA_CC160))(a, b, c, d, e);
    after(reinterpret_cast<Obj>(a));
    return result;
}
static uint64_t __fastcall listed(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e) {
    const uint64_t result = fn<Route>(GA(GA_CAA60))(a, b, c, d, e);
    after(reinterpret_cast<Obj>(a));
    return result;
}
// Diagnostics only (user 2026-11-10, 계 armies out and back in within two turns): each computer-side army is followed
// for its first 6 days on the map. Order changes in that window and a disappearance within it are logged with the last
// raw orders (act/target kind/target) and the group mode/goal. Reads only; no writes.
struct Young {
    Obj root = nullptr, army = nullptr;
    unsigned commander = 0, born = 0, hex = 48400, soldiers = 0, order = 0, group = 0;
};
static Young young[501];
static unsigned group_state(Obj a) {
    Obj t = table(0x7F8D8, word(a, 0x58), 500);
    return valid(t) ? (static_cast<unsigned>(word(t, 0x14)) << 16) | word(t, 0x16) : 0xFFFFFFFFu;
}
static void watch_young() {
    const int today = game_day();
    if (today <= 0) return;
    const unsigned day = static_cast<unsigned>(today);
    for (unsigned i = 1; i <= 500; ++i) {
        Young &y = young[i];
        Obj a = table(0x7E838, i, 500);
        const bool alive = valid(a) && computer_side_army(a);
        const bool same_army = y.army && alive && y.root == world() && a == y.army && word(a, 0x12) == y.commander;
        if (y.army && !same_army) {
            if (y.root == world() && day - y.born <= 6) {
                log_event("sortie_quick_return", i, (y.commander << 16) | (day - y.born), y.hex);
                log_event("sortie_quick_return_last", i, y.order, y.group);
                log_event("sortie_quick_return_size", i, y.soldiers, y.born);
            }
            y = Young{};
        }
        if (!alive) continue;
        if (!y.army) {
            y.root = world(); y.army = a; y.commander = word(a, 0x12); y.born = day;
            y.order = packed_order(a); y.group = group_state(a); y.hex = word(a, 0x2A); y.soldiers = word(a, 0x16);
            log_event("sortie_young_born", i, (y.commander << 16) | (word(a, 0x16) & 0xFFFF), y.hex);
            continue;
        }
        const unsigned order = packed_order(a), group = group_state(a);
        if (day - y.born <= 6 && (order != y.order || group != y.group))
            log_event("sortie_young_order", i, order, group);
        y.order = order; y.group = group; y.hex = word(a, 0x2A); y.soldiers = word(a, 0x16);
    }
}

// Daily (time slot 0 of the original army status pass): end finished corrections.
static void __fastcall daily(int slot) {
    fn<Daily>(GA(GA_2E6E00))(slot);
    if (!enabled || slot != 0) return;
    COST_SCOPE("route_hook:daily");
    if (diagnostic_logging) { watch_young(); AI_TRACE_DAILY(); }
    if (mod_features.supply_relay) for (unsigned i = 1; i <= 500; ++i) supply_check(table(0x7E838, i, 500));
    if (route_hook_hit) for (unsigned i = 1; i <= 500; ++i) hit_check(table(0x7E838, i, 500));   // local108
    if (mod_features.ship_repair_strike) for (unsigned i = 1; i <= 500; ++i) ship_check(table(0x7E838, i, 500));   // local226
    for (unsigned i = 1; i <= 500; ++i) {
        Entry &e = entries[i];
        if (e.kind == none) continue;
        Obj a = table(0x7E838, i, 500);
        if (e.root != world() || !valid(a) || a != e.army || a[0x140] || word(a, 0x12) != e.commander) {
            if (e.kind == breach) log_event("route_breach_released", i, valid(a) ? packed_order(a) : 0u, 1);
            e = Entry{}; continue;
        }
        if (e.kind == breach) {
            // local41 (user 2026-11-10 정원지 left the chain half broken for the long land way): the group run clears
            // member relays (B6B00) without always rebuilding them, so a relay difference alone is not a rival order.
            // While the wall stands and the group still attacks the same goal, a rewritten order is put back.
            const bool hit = e.hit;
            if ((hit ? !route_hook_hit : !mod_features.path_wall) || !wall_standing(e)) {
                // local42 (user 2026-11-10 토성 left standing after the chain fell): the next wall on the way is judged
                // at once instead of waiting for the group's next route write.
                const unsigned tid = e.troops;
                finish(a, e, "route_breach_done");
                Obj t = tid && tid <= 500 ? table(0x7F8D8, tid, 500) : nullptr;
                if (!hit && mod_features.path_wall && valid(t)) { plans[tid].day = -1000000; breach_group(t, tid); }
                continue;
            }
            if (hit && !ranged_hit_ready(a, 20)) { finish(a, e, "route_breach_released"); continue; }
            // local226: the stand is judged again each day (towers may be built or fall, the army moves); with no stand
            // outside hostile tower range the wall is left to the original AI, unless there is no way around it.
            bool moved_stand = false;
            if (!e.forced) {
                const unsigned s = wall_stand(a, e.hex);
                if (s >= 48400) { log_event("route_breach_unsafe", i, e.wall, e.hex); finish(a, e, "route_breach_released"); continue; }
                moved_stand = s != e.stand;
                e.stand = s;
            }
            const Orders now = read(a);
            const bool relay_ok = e.stand >= 48400 || e.stand == word(a, 0x2A) || now.relay[0] == e.stand;
            if (now.action == e.mine.action && now.kind == e.mine.kind && now.id == e.mine.id && relay_ok && !moved_stand) continue;
            Obj t = table(0x7F8D8, word(a, 0x58), 500);
            const bool alive = now.action != 11 && now.action != 12 && word(a, 0x16) >= 1000;
            const bool keep = alive && (hit || (word(a, 0x58) == e.troops && valid(t) && attack_mode(word(t, 0x14)) && word(t, 0x16) == e.goal));
            if (!keep) { log_event("route_breach_released", i, packed_order(a), group_state(a)); e = Entry{}; continue; }
            log_event("route_breach_reapply", i, packed_order(a), e.wall);
            if (!same(now, e.mine)) e.original = now;
            breach_write(a, e);
            continue;
        }
        if (e.kind == ship) { if (ship_keep(a, e) && !same(read(a), e.mine)) breach_write(a, e); continue; }   // local226
        if (!same(read(a), e.mine)) { e = Entry{}; continue; }   // someone else (original/player) wrote: let go
        if (e.kind == hold) {
            if (!mod_features.morale || a[0x1A] >= 100 || !campaign_band(a, e.band)) finish(a, e, "route_morale_done");
        }
    }
}

static bool call_targets(unsigned site, unsigned target) {
    if (!image || !site || !target || image[site] != 0xE8) return false;
    int delta = 0;
    memcpy(&delta, image + site + 1, 4);
    return image + site + 5 + delta == image + target;
}
static CallLink calls[4];
static Obj relay;
static bool ready;
static bool hit_wanted() { return mod_features.obstacle && mod_features.hit_response; }   // local108
static bool wanted() { return mod_features.morale || mod_features.path_wall || mod_features.supply_relay || hit_wanted() || mod_features.ship_repair_strike; }
// 0 = prepared (or off), 1 = a CALL no longer targets its original function, 2 = prepare failed.
static unsigned prepare() { COST_SCOPE("route_hook:prepare");
    for (auto &c : calls) c = CallLink{};
    ready = false; route_hook_morale = route_hook_breach = route_hook_hit = false; route_hook_owns = nullptr;
    if (!wanted()) return 0;
    const unsigned sites[4] = {GA(GA_64F1B), GA(GA_667C7), GA(GA_667C0), GA(GA_44E50D)};
    const unsigned targets[4] = {GA(GA_CC160), GA(GA_CC160), GA(GA_CAA60), GA(GA_2E6E00)};
    for (unsigned i = 0; i < 4; ++i) if (!call_targets(sites[i], targets[i])) return 1;
    if (!relay) relay = allocate_near(image + sites[0], 0x1000);
    if (!relay) return 2;
    DWORD old = 0;
    if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READWRITE, &old)) return 2;
    absolute_jump(relay, reinterpret_cast<void *>(&route));
    absolute_jump(relay + 32, reinterpret_cast<void *>(&listed));
    absolute_jump(relay + 64, reinterpret_cast<void *>(&daily));
    bool ok = call_prepare(calls[0], sites[0], relay) && call_prepare(calls[1], sites[1], relay) &&
              call_prepare(calls[2], sites[2], relay + 32) && call_prepare(calls[3], sites[3], relay + 64);
    if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READ, &old) || !FlushInstructionCache(GetCurrentProcess(), relay, 96)) ok = false;
    if (!ok) return 2;
    ready = true;
    return 0;
}
static void restore() { COST_SCOPE("route_hook:restore");
    for (unsigned i = 4; i > 0; --i) call_restore(calls[i - 1]);
    ready = false; route_hook_morale = route_hook_breach = route_hook_hit = false; route_hook_owns = nullptr;
    for (auto &e : entries) e = Entry{};
    for (auto &b : bridges) b = Bridge{};
}
static bool publish() { COST_SCOPE("route_hook:publish");
    if (!wanted()) return true;
    bool ok = ready;
    for (unsigned i = 0; i < 4 && ok; ++i) {
        int delta = 0;
        memcpy(&delta, image + calls[i].site + 1, 4);
        ok = image[calls[i].site] == 0xE8 && delta == calls[i].original && call_publish(calls[i]);
    }
    if (!ok) restore();
    else { route_hook_morale = mod_features.morale; route_hook_breach = mod_features.path_wall; route_hook_hit = hit_wanted(); route_hook_owns = &owns; }
#ifndef NO_RUNTIME_LOG
    log_event("route_hook_installed", ok ? 1u : 0u, route_hook_morale, (route_hook_breach ? 1u : 0u) | (route_hook_hit ? 2u : 0u) | (ok && mod_features.ship_repair_strike ? 4u : 0u));
#endif
    return ok;
}

}   // namespace route_hook

// local108 (user 2026-11-13 "망루나 투석대도"): the facility attack 192AF0 applies its damage by CALL 192FBB ->
// 179DE0(kind 0x1B, army id, amount, 0xB, facility id = 22F930 on the stack), not through 175950 where army fire is
// recorded. This CALL is wrapped: the original runs, then a soldier loss of a computer-side army hostile to the player
// from a player military facility is recorded for the hit response. Records only; refused = facility fire is simply not
// recorded (feature_off 95). Off = CALL restored.
namespace facility_hit_hook {
using Apply = uint64_t(__fastcall *)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
static CallLink link;
static Obj relay;
static bool ready;
static uint64_t __fastcall apply(uint64_t kind, uint64_t id, uint64_t amount, uint64_t reason, uint64_t facility) {
    const unsigned victim_id = static_cast<unsigned>(id);
    const bool watch = enabled && static_cast<unsigned>(kind) == 0x1B && static_cast<unsigned>(reason) == 0xB && victim_id && victim_id <= 500;
    Obj victim = watch ? table(0x7E838, victim_id, 500) : nullptr;
    const unsigned before = valid(victim) ? word(victim, 0x16) : 0u;
    const uint64_t result = fn<Apply>(GA(GA_179DE0))(kind, id, amount, reason, facility);
    if (before) record_facility_hit(victim_id, static_cast<uint32_t>(facility), before);
    return result;
}
static bool wanted() { return mod_features.obstacle && mod_features.hit_response; }
static unsigned prepare() { COST_SCOPE("facility_hit:prepare");
    link = CallLink{}; ready = false;
    if (!wanted()) return 0;
    const unsigned site = GA(GA_192FBB);
    if (!site || !route_hook::call_targets(site, GA(GA_179DE0))) return 1;
    if (!relay) relay = allocate_near(image + site, 0x1000);
    if (!relay) return 2;
    DWORD old = 0;
    if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READWRITE, &old)) return 2;
    absolute_jump(relay, reinterpret_cast<void *>(&apply));
    bool ok = call_prepare(link, site, relay);
    if (!VirtualProtect(relay, 4096, PAGE_EXECUTE_READ, &old) || !FlushInstructionCache(GetCurrentProcess(), relay, 32)) ok = false;
    if (!ok) return 2;
    ready = true;
    return 0;
}
static void restore() { COST_SCOPE("facility_hit:restore"); call_restore(link); ready = false; }
static bool publish() { COST_SCOPE("facility_hit:publish");
    if (!wanted()) return true;
    int delta = 0;
    bool ok = ready;
    if (ok) { memcpy(&delta, image + link.site + 1, 4); ok = image[link.site] == 0xE8 && delta == link.original && call_publish(link); }
    if (!ok) restore();
#ifndef NO_RUNTIME_LOG
    log_event("facility_hit_installed", ok ? 1u : 0u, 0, 0);
#endif
    return ok;
}
}   // namespace facility_hit_hook