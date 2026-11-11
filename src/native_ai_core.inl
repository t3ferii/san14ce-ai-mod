#pragma once
// Combat AI core (docs/MODULARIZATION_PLAN.md W6-D): the helpers and per-army state that the campaign AI, sortie and the behaviour-tree
// nodes share, moved verbatim out of native_adapter.cpp. Compiled by the combat root only (module_unity_combat.inl slot ai_support).
#include "tactics_policy.h"
#include "obstacle_policy.h"
#include "sortie_policy.h"
#include "campaign_policy.h"
static SRWLOCK policy_lock = SRWLOCK_INIT;
static __declspec(thread) int in_cut;
static __declspec(thread) int in_policy;
#ifdef SORTIE_FORMATION
static bool sortie_controlled(Obj army);
#endif
// Ruler inner trait 건설 (Person+180 s16; UI 1..5 = -20,-10,0,10,20, column getter 344D10/356260).
// A ruler at 5 (20) makes its armies fill their own regions' facility capacity (2026-10-21),
// only while builder_ruler_fill is on (user 2026-11-01).
static bool builder_force(unsigned force)
{ COST_SCOPE("ai_core:builder_force");
    if(!mod_features.builder_ruler_fill)return false;
    Obj f=force && force<=51?table(0xDF30,force,51):nullptr;
    Obj ruler=valid(f)?table(0x158,word(f,0x10),6000):nullptr;
    return valid(ruler) && *reinterpret_cast<short *>(ruler+0x180)>=20;
}
static unsigned army_id(Obj army)
{ COST_SCOPE("ai_core:army_id");
    /* 22DA10 computes (army - root->Army[0]) / 512. Army+10 is
     * a state field, shared by many active units, never their identity. */
    unsigned id = fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F4A0))(army);
    return id > 0 && id <= 500 && table(0x7E838, id, 500) == army ? id : 0;
}
static bool belongs_to_troops(Obj army, Obj troops)
{ COST_SCOPE("ai_core:belongs_to_troops");
    /* Resolve the native Troops ID stored in Army+58 to its object.
     * Troops+10 is also a state field, so compare object identity. */
    return troops != nullptr && table(0x7F8D8, word(army, 0x58), 500) == troops;
}
static bool computer_army(Obj army)
{ COST_SCOPE("ai_core:computer_army");
    if (!valid(army) || army[0x140]) return false;
    unsigned player = player_force();
    unsigned force = army_force(army);
    return force > 0 && force < 52 && force != player && (player != 0 || observer_mode());
}
// user 2026-11-10: a rule written for computer corps never reaches the player force - neither its direct
// armies nor its delegated corps (both carry the player's force id). Every rule added after v15.7.1 asks these.
[[maybe_unused]] static bool computer_side_force(unsigned force)
{ COST_SCOPE("ai_core:computer_side_force");
    unsigned player = player_force();
    return force > 0 && force < 52 && force != player && (player != 0 || observer_mode());
}
static bool computer_side_army(Obj army) { return computer_army(army); }
// U39 route_hook (native_route_hook.inl) publishes these while its CALLs are live; the older per-army morale and
// neutral wall code steps aside. route_hook_owns(army) = the army holds at a band or breaches a wall right now.
static bool route_hook_morale, route_hook_breach;
namespace route_hook { static bool assign_fifth(Obj a, unsigned wall_hex); }   // local55, native_route_hook.inl
static bool (*route_hook_owns)(Obj army);

static bool campaign_controlled(Obj army) { COST_SCOPE("ai_core:campaign_controlled");
    if(computer_army(army))return true;
#ifdef SORTIE_FORMATION
    return sortie_controlled(army);
#else
    return false;
#endif
}
/* This wrapper is installed at the one CALL inside D06D0. Restricting its
 * candidate list before the native predicate prevents choosing an AI victim
 * and avoids calling the predicate twice after it temporarily changes owners. */
static int __fastcall cut_player_predicate(unsigned owner, GameList *tiles, GameList *enemies, Obj *target)
{ COST_SCOPE("cut_player_predicate");
    auto original = fn<int(__fastcall *)(unsigned, GameList *, GameList *, Obj *)>(GA(GA_C1D10));
    if (!in_cut) return original(owner, tiles, enemies, target);
    unsigned player = player_force();
    if (!player || owner == player) return 0;
    GameList filtered; list_init(filtered, GA(GA_13453D0));
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
// Fixture entry for the D1760 victim filter; live cuts come from the original cut node (cut_node).
[[maybe_unused]] static bool try_cut(Obj army)
{ COST_SCOPE("ai_core:try_cut");
    if (!mod_features.combat_cut || !computer_army(army) || campaign_castle_locked(army)) return false;
    unsigned player = player_force();
    if (!player) return false; // No player logistics target exists in demo.
    bool have_enemy = false;
    unsigned own = army_force(army);
    if (!fn<int(__fastcall *)(unsigned, unsigned)>(GA(GA_2CC630))(player, own)) return false;
    for (unsigned i = 1; i <= 500; ++i) {
        Obj enemy = table(0x7E838, i, 500);
        if (valid(enemy) && army_force(enemy) == player) { have_enemy = true; break; }
    }
    if (!have_enemy) return false;
    GameList route; list_init(route);
    Obj target = nullptr;
    ++in_cut;
    int success = fn<int(__fastcall *)(Obj, GameList *, Obj *)>(GA(GA_D1760))(army, &route, &target);
    --in_cut;
    list_free(route);
    if (!success || !valid(target) || army_force(target) != player) return false;
    *reinterpret_cast<unsigned *>(army + 0x1F0) |= 8;
    *reinterpret_cast<unsigned *>(army + 0x1F4) = fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F4A0))(target);
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
    // local17 path_wall (memory only): the waypoint wall and the demolition lock held until the building falls.
    unsigned short path_wall_hex, path_wall_goal;
    int path_wall_day, path_wall_checked;
    unsigned short path_wall_lock_id, path_wall_lock_hex;
    int path_wall_lock_day;
    bool path_wall_done;
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
static bool campaign_castle_locked(Obj army) { COST_SCOPE("ai_core:campaign_castle_locked");
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
{ COST_SCOPE("ai_core:army_area");
    Obj tile = table(0xE270, word(army, 0x2A), 48399);
    return valid(tile) ? fn<Obj(__fastcall *)(Obj)>(GA(GA_22B8E0))(tile) : nullptr;
}
static int distance(unsigned a, unsigned b) { return fn<int(__fastcall *)(int, int)>(GA(GA_2ACB80))(static_cast<int>(a), static_cast<int>(b)); }
static Progress *observe_capture(Obj army)
{ COST_SCOPE("ai_core:observe_capture");
    unsigned id = army_id(army);
    if (!id || id > 500) return nullptr;
    Progress &p = progress[id];
    unsigned owner = army_force(army);
    Obj root = world();
    int day=game_day();
    if (p.army != army || p.root != root || p.commander != word(army, 0x12) || p.force != owner || day<p.observed_day) {
        if (p.path_wall_lock_id && (p.army != army || p.root != root || p.commander != word(army, 0x12) || p.force != owner))
            log_event("path_wall_lock_release", army_id(army), p.path_wall_lock_id, 3);
        memset(&p, 0, sizeof(p)); p.army = army; p.root = root;
        p.commander = word(army, 0x12); p.force = static_cast<unsigned char>(owner);
        p.previous_center = 48400; p.construction_hex = 48400;
        p.region_target = 48400; p.staging_hex = 48400;
        p.build_center=48400;p.destroy_hex=48400;p.path_wall_hex=48400;p.path_wall_lock_hex=48400;
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
    if(army[0x46]>=3 && (army[0x46]<=14 || army[0x46]==17) && word(army,0x42)<48400) {   // 17 rockfall: local53 trap list
        bool unfinished=army[0x37]==8;
        Obj site=table(0xE270,word(army,0x42),48399);
        if(valid(site) && word(site,0x1A)>0 && word(site,0x1A)<=3000) {
            unsigned building_id=word(site,0x1A);
            Obj b=table(0x6DA98,building_id,3000);
            if(valid(b) && b[0x13]==owner && b[0x12]==army[0x46] && word(b,0x10)==word(army,0x42)) {
                int max=fn<int(__fastcall *)(Obj)>(GA(GA_2AE450))(b);
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
            if((p.regional_worker || !mod_features.mission_v2) && valid(captured) && !fn<int(__fastcall *)(Obj)>(GA(GA_234020))(captured) && !fn<int(__fastcall *)(Obj)>(GA(GA_234590))(captured)) {
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
{ COST_SCOPE("ai_core:reserved_hex");
    for (unsigned i = 1; i <= 500; ++i) {
        Obj other = table(0x7E838, i, 500);
        if (!valid(other)) continue;
        if (word(other, 0x2A) == hex) return true;
        if (other != actor && (other[0x37] == 8 || other[0x37] == 9) && word(other, 0x42) == hex) return true;
    }
    return false;
}
static bool campaign_build_order(Obj army,unsigned facility,unsigned hex);
static Obj campaign_goal(unsigned city);
static bool hostile_city(Obj army, unsigned city);
static bool tribe_goal(unsigned city);
static bool set_build(Obj army, unsigned facility, unsigned hex)
{ COST_SCOPE("ai_core:set_build");
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
        Obj building = fn<Obj(__fastcall *)(Obj)>(GA(GA_2315A0))(tile);
        if (!valid(building) || building[0x13] != army_force(army) || building[0x12] != p.facility) {
            p.facility = 0; return false;
        }
        int maximum = fn<int(__fastcall *)(Obj)>(GA(GA_2AE450))(building);
        if (maximum <= 0) { p.facility = 0; return false; }
        if (word(building, 0x14) >= maximum) {
            p.cycle_mask|=1u<<p.facility;
            if((p.facility>=11 && p.facility<=14) || p.facility==17)p.cycle_mask|=(15u<<11)|(1u<<17);
            p.facility = 0; p.finished_build = true;
            // Keep the captured base until its native capacity is filled.
            p.build_chain=true;
            Obj context=pointer(world(),0x871E8);
            p.return_deadline_day=fn<int(__fastcall *)(Obj)>(GA(GA_32DD00))(context+0x34)+10;
            p.progress_day=game_day();
            log_event("build_finished_resume_campaign", army_id(army), p.city, p.construction_hex);
            return false;
        }
        return set_build(army, p.facility, p.construction_hex);
    }
    Obj person = table(0x158, word(army, 0x12), 6000);
    Obj definition = table(0x73958, p.facility, 30);
    Obj origin = fn<Obj(__fastcall *)(Obj)>(GA(GA_22BFE0))(army);
    Obj force = table(0xDF30, army_force(army), 51);
    if (!valid(person) || !valid(definition) || !valid(origin) || !valid(force)) {
        p.facility = 0; return false;
    }
    auto resource = *reinterpret_cast<int(__fastcall **)(Obj)>(pointer(origin, 0) + 0x90);
    int cost = fn<int(__fastcall *)(Obj, Obj)>(GA(GA_2AD6C0))(force, definition);
    if (cost < 0 || resource(origin) < cost) { p.facility = 0; return false; }
    GameList people; list_init(people, GA(GA_13466B0)); list_add(people, person);
    int possible = fn<int(__fastcall *)(GameList *, Obj, Obj)>(GA(GA_27A8D0))(&people, tile, definition);
    list_free(people);
    if (!possible) { p.facility = 0; return false; }
    return set_build(army, p.facility, p.construction_hex);
}

// ai_trace (user 2026-11-11 "원작 판단 방식을 알고싶어 ... 진단판으로 이걸 다 볼 수 있게", diagnostic build only): follow every
// computer departure the original orders to build (action 8/9) and every departure or army of a watched commander
// ([diagnostics] watch_officers) from the dispatch to its disappearance. Logs, all read only:
//   trace_departure       person, source city << 16 | soldiers, action << 24 | kind << 16 | facility
//   trace_departure_goal  person, target id (hex / city / facility), formation
//   trace_departure_call  person, 1st / 2nd game-image return address (RVA) of the dispatch call (which original node)
//   trace_departure_call2 person, 3rd / 4th return address
//   trace_departure_army  army, troops id << 16 | mission kind (troops +0x14), troops goal (+0x16)
//   trace_order           army, action << 24 | kind << 16 | target, hex           (each day it changed)
//   trace_troops          army, troops id << 16 | mission kind, goal               (each day it changed)
//   trace_build           army, facility << 24 | durability, max durability       (while building)
//   trace_gone            army, commander, last hex                                (left the map: home, defeat, merged)
#ifndef NO_RUNTIME_LOG
namespace ai_trace {
struct Track { Obj root = nullptr, army = nullptr; unsigned commander = 0, order = 0, troops = 0, hex = 48400, build = 0; int born = 0; };
static Track tracks[501];
static unsigned officers[16], officer_count; static bool loaded;
static void load() {
    if (loaded) return; loaded = true;
    wchar_t path[MAX_PATH], text[256] = {};
    if (!mod_ini_path(path, L"기능설정.ini")) return;
    GetPrivateProfileStringW(L"diagnostics", L"watch_officers", L"", text, 256, path);
    for (wchar_t *s = text; *s && officer_count < 16;) {
        while (*s && (*s < L'0' || *s > L'9')) ++s;
        unsigned v = 0; bool any = false; while (*s >= L'0' && *s <= L'9') { v = v * 10 + static_cast<unsigned>(*s - L'0'); ++s; any = true; }
        if (any && v) officers[officer_count++] = v;
    }
}
static bool officer(unsigned person) { load(); for (unsigned i = 0; i < officer_count; ++i) if (officers[i] == person) return true; return false; }
static bool tracked(Obj army) {
    if (!diagnostic_logging || !valid(army)) return false;
    const unsigned id = army_id(army);
    if (id && id <= 500 && tracks[id].root == world() && tracks[id].army == army && tracks[id].commander == word(army, 0x12)) return true;
    return officer(word(army, 0x12));
}
static unsigned order_of(Obj a) { return (static_cast<unsigned>(a[0x37]) << 24) | (static_cast<unsigned>(a[0x38]) << 16) | word(a, 0x3A); }
static unsigned troops_of(Obj a) {
    Obj t = table(0x7F8D8, word(a, 0x58), 500);
    return valid(t) ? (static_cast<unsigned>(word(a, 0x58)) << 16) | (word(t, 0x14) & 0xFFFF) : 0xFFFF0000u;
}
static unsigned goal_of(Obj a) { Obj t = table(0x7F8D8, word(a, 0x58), 500); return valid(t) ? word(t, 0x16) : 65535u; }
static size_t image_bytes() {
    static size_t n;
    if (!n && image) {
        const IMAGE_DOS_HEADER *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(image);
        n = reinterpret_cast<const IMAGE_NT_HEADERS *>(reinterpret_cast<const unsigned char *>(image) + dos->e_lfanew)->OptionalHeader.SizeOfImage;
    }
    return n;
}
// Called by sortie_dispatch with the final command and the army the original made (nullptr when refused).
static void departure(const unsigned *c, Obj army) {
    if (!diagnostic_logging || !c) return;
    const bool build = c[13] == 8 || c[13] == 9;
    if (!build && !officer(c[0])) return;
    Obj person = table(0x158, c[0], 6000);
    const unsigned city = valid(person) ? word(person, 0xE2) : 65535u;
    log_event("trace_departure", c[0], (city << 16) | (c[2] & 0xFFFF), (c[13] << 24) | ((c[14] & 0xFF) << 16) | (c[23] & 0xFFFF));
    log_event("trace_departure_goal", c[0], c[15], c[3]);
    void *frames[16] = {}; unsigned rva[4] = {}, found = 0;
    const USHORT n = RtlCaptureStackBackTrace(1, 16, frames, nullptr);
    const uintptr_t base = reinterpret_cast<uintptr_t>(image), size = image_bytes();
    for (USHORT i = 0; i < n && found < 4; ++i) {
        const uintptr_t at = reinterpret_cast<uintptr_t>(frames[i]);
        if (base && at >= base && at - base < size) rva[found++] = static_cast<unsigned>(at - base);
    }
    log_event("trace_departure_call", c[0], rva[0], rva[1]);
    log_event("trace_departure_call2", c[0], rva[2], rva[3]);
    if (!valid(army)) { log_event("trace_departure_army", 0, 0, c[0]); return; }
    const unsigned id = army_id(army);
    log_event("trace_departure_army", id, troops_of(army), goal_of(army));
    if (!id || id > 500) return;
    Track &t = tracks[id];
    t = Track{}; t.root = world(); t.army = army; t.commander = word(army, 0x12); t.born = game_day();
    t.order = order_of(army); t.troops = troops_of(army); t.hex = word(army, 0x2A);
}
// Daily (route_hook slot 0): each tracked army's order, troops mission and construction when they changed; the day it leaves.
static void daily() {
    if (!diagnostic_logging) return;
    load();
    for (unsigned i = 1; i <= 500; ++i) {
        Track &t = tracks[i];
        Obj a = table(0x7E838, i, 500);
        const bool alive = valid(a) && !a[0x140] && word(a, 0x2A) < 48400;
        if (t.root == world() && t.army) {
            if (!alive || a != t.army || word(a, 0x12) != t.commander) {
                log_event("trace_gone", i, t.commander, t.hex); t = Track{};
                if (!alive) continue;
            }
        }
        if (!alive) continue;
        if (!(t.root == world() && t.army == a)) {
            if (!officer(word(a, 0x12))) continue;
            t = Track{}; t.root = world(); t.army = a; t.commander = word(a, 0x12); t.born = game_day(); t.order = 0xFFFFFFFFu; t.troops = 0xFFFFFFFFu;
            log_event("trace_watch_start", i, t.commander, word(a, 0x2A));
        }
        t.hex = word(a, 0x2A);
        const unsigned order = order_of(a), troops = troops_of(a);
        if (order != t.order) { log_event("trace_order", i, order, t.hex); t.order = order; }
        if (troops != t.troops) { log_event("trace_troops", i, troops, goal_of(a)); t.troops = troops; }
        if (a[0x37] == 8 || a[0x37] == 9) {
            Obj tile = table(0xE270, word(a, 0x42), 48399);
            unsigned b = valid(tile) ? word(tile, 0x1A) : 0, state = 0, maximum = 0;
            if (b && b <= 3000) {
                Obj f = table(0x6DA98, b, 3000);
                if (valid(f)) { state = (static_cast<unsigned>(f[0x12]) << 24) | word(f, 0x14); maximum = static_cast<unsigned>(fn<int(__fastcall *)(Obj)>(GA(GA_2AE450))(f)); }
            } else state = static_cast<unsigned>(a[0x46]) << 24;
            if (state != t.build) { log_event("trace_build", i, state, maximum); t.build = state; }
        }
    }
}
}
#define AI_TRACE_DEPARTURE(c, army) ai_trace::departure((c), (army))
#define AI_TRACE_DAILY() ai_trace::daily()
#define AI_TRACE_TRACKED(army) ai_trace::tracked(army)
#else
#define AI_TRACE_DEPARTURE(c, army) ((void)0)
#define AI_TRACE_DAILY() ((void)0)
#define AI_TRACE_TRACKED(army) false
#endif
