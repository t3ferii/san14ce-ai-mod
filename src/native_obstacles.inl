// Evidence: research/v9. This callback observes the native DAMAGE application,
// not the earlier score/forecast stage. It only records; AI nodes issue orders.
#include "game_addresses.h"
static unsigned campaign_area_id(unsigned hex);
using ApplyDamage=uint64_t(__fastcall *)(Obj,Obj,Obj);
static ApplyDamage original_damage;
struct RangedHit {
    Obj root, victim, attacker;
    unsigned short victim_commander, attacker_commander, from, to;
    unsigned char victim_force;
    int day;
    bool present;
    unsigned short facility;   // local108: player military facility (망루·투석대 …) that fired; attacker is then null
};
static RangedHit ranged_hits[501];
struct CampaignHit {Obj root,victim,attacker;unsigned short commander,attacker_commander;unsigned char force;int day;bool present;unsigned short source_city;int city_day;};
static CampaignHit campaign_hits[501];
static bool obstacle_between(unsigned from,unsigned to,unsigned wall) { COST_SCOPE("obstacles:obstacle_between");
    if(from>=48400 || to>=48400 || wall>=48400 || from==to || from==wall || to==wall)return false;
    int total=distance(from,to),a=distance(from,wall),b=distance(wall,to);
    return total>1 && a>0 && b>0 && a<total && b<total && a+b<=total+1;
}
static bool player_ranged_army(Obj attacker) { COST_SCOPE("obstacles:player_ranged_army");
    unsigned player=player_force();
    if(!player || !valid(attacker) || army_force(attacker)!=player)return false;
    Obj active=fn<Obj(__fastcall *)(Obj)>(GA(GA_22CAD0))(attacker);
    // Active formation getter chooses land/water from the ACTUAL terrain.
    return active && (active==table(0x77428,3,20) || active==table(0x77428,13,20));
}
static uint64_t __fastcall combat_apply_damage(Obj manager,Obj pair,Obj output) { COST_SCOPE("combat_apply_damage");
    if(!enabled || in_policy)return original_damage(manager,pair,output);
    Obj source=pair?pointer(pair,0):nullptr;
    unsigned aid=source && dword(source,4)==27?word(source,0):0;
    unsigned vid=pair && dword(pair+8,4)==27?word(pair+8,0):0;
    Obj attacker=aid?table(0x7E838,aid,500):nullptr,victim=vid?table(0x7E838,vid,500):nullptr;
    unsigned source_kind=source?dword(source,4):0;
    unsigned source_city=source && (source_kind==5 || source_kind==6)?word(source,0):0;
    bool castle_kind=source_kind==5?source_city<=51:source_kind==6 && source_city>51 && source_city<=61;
    Obj firing_city=source_city && castle_kind?fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(world(),source_city):nullptr;
    Obj firing_force=valid(firing_city) && word(firing_city,0x4E)==source_city?fn<Obj(__fastcall *)(Obj)>(GA(GA_22E3E0))(firing_city):nullptr;
    unsigned city_force=valid(firing_force)?(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(firing_force,0)+0x60))(firing_force):0;
    bool city_hit=campaign_controlled(victim) && city_force && fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(army_force(victim),city_force);
    // [combat] hit_response=0: record nothing, so no hit-driven lock/escort/obstacle response.
    bool engaged=enabled && mod_features.hit_response && !in_policy && campaign_controlled(victim) && valid(attacker) &&
        fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(army_force(victim),army_force(attacker))!=0;
    engaged=engaged || (mod_features.hit_response && city_hit);
    bool watch=enabled && !in_policy && engaged && player_ranged_army(attacker) &&
        fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(army_force(victim),army_force(attacker))!=0;
    unsigned before=engaged?word(victim,0x16):0,commander=engaged?word(victim,0x12):0;
    unsigned attacking_commander=engaged && valid(attacker)?word(attacker,0x12):0,owner=engaged?army_force(victim):0;
    Obj saved_root=engaged?world():nullptr;
    unsigned from=watch?word(attacker,0x2A):48400,to=watch?word(victim,0x2A):48400;
    uint64_t result=original_damage(manager,pair,output);
    if(engaged && world()==saved_root && table(0x7E838,vid,500)==victim && valid(victim) &&
       word(victim,0x12)==commander && army_force(victim)==owner && word(victim,0x16)<before) {
        AcquireSRWLockExclusive(&policy_lock);
        const CampaignHit previous=campaign_hits[vid];
        bool same=previous.root==saved_root && previous.victim==victim && previous.commander==commander && previous.force==owner && game_day()>=previous.day;
        campaign_hits[vid]={saved_root,victim,attacker,static_cast<unsigned short>(commander),
            static_cast<unsigned short>(attacking_commander),static_cast<unsigned char>(owner),game_day(),true,
            static_cast<unsigned short>(city_hit?source_city:same?previous.source_city:0),city_hit?game_day():same?previous.city_day:0};
        if(watch)ranged_hits[vid]={saved_root,victim,attacker,static_cast<unsigned short>(commander),
            static_cast<unsigned short>(attacking_commander),static_cast<unsigned short>(from),
            static_cast<unsigned short>(to),static_cast<unsigned char>(owner),game_day(),true,0};
        ReleaseSRWLockExclusive(&policy_lock);
    }
    return result;
}
// local108: a player military facility (type class 2: 망루·투석대·망대) that still stands where it fired.
static bool player_firing_facility(unsigned id,unsigned hex) { COST_SCOPE("obstacles:player_firing_facility");
    unsigned player=player_force();
    Obj b=id && id<=3000?table(0x6DA98,id,3000):nullptr;
    if(!player || !valid(b) || b[0x13]!=player || !word(b,0x14) || word(b,0x10)!=hex)return false;
    Obj type=fn<Obj(__fastcall *)(Obj)>(GA(GA_EDF0))(b);
    return valid(type) && type[0x2C]==2;
}
static bool ranged_hit_ready(Obj army,int max_age=10) { COST_SCOPE("obstacles:ranged_hit_ready");
    unsigned id=army_id(army);if(!id)return false;
    const RangedHit &hit=ranged_hits[id];int elapsed=game_day()-hit.day;
    if(!hit.present || hit.root!=world() || hit.victim!=army || hit.victim_commander!=word(army,0x12) ||
       hit.victim_force!=army_force(army) || elapsed<0 || elapsed>max_age)return false;
    if(hit.facility)return player_firing_facility(hit.facility,hit.from);
    return player_ranged_army(hit.attacker) && word(hit.attacker,0x12)==hit.attacker_commander;
}
// Where the shooter stands now: the army's hex, or the facility hex it fired from.
static unsigned ranged_hit_source(Obj army) {
    const RangedHit &hit=ranged_hits[army_id(army)];
    return hit.facility?hit.from:word(hit.attacker,0x2A);
}
static bool ranged_wall(Obj army,Obj b,int max_age=10) { COST_SCOPE("obstacles:ranged_wall");
    if(!ranged_hit_ready(army,max_age))return false;
    const RangedHit &hit=ranged_hits[army_id(army)];unsigned wall=word(b,0x10);
    return distance(word(army,0x2A),wall)<=10 && obstacle_between(hit.from,hit.to,wall) &&
        obstacle_between(ranged_hit_source(army),word(army,0x2A),wall);
}
// local108: facility fire (192AF0 -> 179DE0(kind 0x1B, army id, amount, 0xB, facility id)) does not pass 175950, so
// the CALL at 192FBB records it here. Same victim rules as army fire: a computer-side army hostile to the player that
// lost soldiers.
static void record_facility_hit(unsigned vid,unsigned facility,unsigned before) { COST_SCOPE("obstacles:record_facility_hit");
    if(!enabled || in_policy || !mod_features.obstacle || !mod_features.hit_response || !vid || vid>500)return;
    Obj victim=table(0x7E838,vid,500);
    Obj b=facility && facility<=3000?table(0x6DA98,facility,3000):nullptr;
    if(!valid(victim) || !valid(b) || !campaign_controlled(victim) || word(victim,0x16)>=before)return;
    const unsigned from=word(b,0x10),player=player_force();
    if(!player_firing_facility(facility,from) ||
       !fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(army_force(victim),player))return;
    AcquireSRWLockExclusive(&policy_lock);
    ranged_hits[vid]={world(),victim,nullptr,word(victim,0x12),0,static_cast<unsigned short>(from),word(victim,0x2A),
        static_cast<unsigned char>(army_force(victim)),game_day(),true,static_cast<unsigned short>(facility)};
    ReleaseSRWLockExclusive(&policy_lock);
    static volatile LONG lines;
    if(InterlockedIncrement(&lines)<=200)log_event("ranged_hit_facility",vid,facility|(static_cast<unsigned>(b[0x12])<<16),from);
}
static unsigned campaign_goal_hex(const Progress &p) { COST_SCOPE("obstacles:campaign_goal_hex");
    Obj goal=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(world(),p.city);
    return valid(goal)?fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(goal):48400;
}
// Neutral wall areas and the cities owning them or an adjacent area (user 2026-11-09).
struct WallAreaTable {
    unsigned base(unsigned area) {
        Obj a=area && area<=500?table(0x6CAF0,area,500):nullptr;
        return valid(a)?a[0x35]:0;
    }
    unsigned neighbour(unsigned area,unsigned i) {
        Obj a=area && area<=500?table(0x6CAF0,area,500):nullptr;
        unsigned n=valid(a)?word(a,0x56+i*2):0;
        return n<=500?n:0;
    }
};
static bool path_wall_city(unsigned city) { COST_SCOPE("obstacles:path_wall_city");
    static std::array<bool,wall_path::max_city> cached{};
    static Obj cache_world=nullptr;
    static int cache_day=-1;
    Obj root=world();
    int day=game_day();
    if(root!=cache_world || cache_day<0 || day<cache_day || day-cache_day>=10) {
        std::vector<unsigned> areas;
        for(unsigned id=1;id<=3000;++id) {
            Obj b=table(0x6DA98,id,3000);
            if(!valid(b))continue;
            unsigned kind=b[0x12];
            if((kind==9 || kind==10 || kind==16) && word(b,0x14) && !b[0x13]) {
                unsigned area=campaign_area_id(word(b,0x10));
                if(area)areas.push_back(area);
            }
        }
        WallAreaTable wall_areas;
        cached=wall_path::wall_cities(areas,wall_areas);
        cache_world=root;cache_day=day;
#ifndef NO_RUNTIME_LOG
        unsigned walls=static_cast<unsigned>(areas.size()),flagged=0;
        unsigned mask_lo=0,mask_hi=0;
        for(unsigned i=1;i<wall_path::max_city;++i)if(cached[i]) {
            ++flagged;
            if(i<32)mask_lo|=1u<<i;else mask_hi|=1u<<(i-32);
        }
        log_event("path_wall_cities",walls,flagged,day);
        // local106b diagnostic (live log 11-11..11-13: 476 scans, walls 0 every time): every 토성/석벽/철쇄 record by
        // owner and durability. value = with durability | owner 0 << 10 | owner 0 and durability << 20, hex = owner 0
        // with no area.
        unsigned raw=0,raw_free=0,raw_live=0,no_area=0;
        for(unsigned id=1;id<=3000;++id) {
            Obj b=table(0x6DA98,id,3000);
            if(!valid(b) || !(b[0x12]==9 || b[0x12]==10 || b[0x12]==16))continue;
            ++raw;if(word(b,0x14))++raw_live;
            if(!b[0x13]) {++raw_free;if(word(b,0x14) && !campaign_area_id(word(b,0x10)))++no_area;}
        }
        log_event("path_wall_raw",raw,(std::min)(raw_live,1023u)|((std::min)(raw_free,1023u)<<10)|((std::min)(walls,1023u)<<20),no_area);
        log_event("path_wall_flagged",mask_lo,mask_hi,day);
#endif
    }
    return city<wall_path::max_city && cached[city];
}
// +0xE2 home_base is a PERSON field (San14Sim/docs/orig/structs.md 3.13), not
// an army field: read it from the army's commander. A city id 1..61 (51 castles
// + 10 gates) is resolved through the city table and checked by its +0x4E id.
static bool path_wall_city_id(unsigned id) { COST_SCOPE("obstacles:path_wall_city_id");
    if(id<1 || id>61)return false;
    Obj b=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(world(),id);
    return valid(b) && word(b,0x4E)==id;
}
// PW22 (user 2026-11-09): the demolition order issued for path_wall is a lock.
// While it is held the army accepts no other mod order (codes 1-5); code 6
// (destroy_order) and code 7 (refresh_path_wall) keep working for the locked
// building. A finished army is left entirely to the original AI.
static Progress *path_wall_progress(Obj army) { COST_SCOPE("obstacles:path_wall_progress");
    unsigned id=valid(army)?army_id(army):0;
    if(!id || id>500)return nullptr;
    Progress &p=progress[id];
    if(p.army!=army || p.root!=world() || p.commander!=word(army,0x12) || p.force!=army_force(army))return nullptr;
    return &p;
}
static bool path_wall_locked(Obj army,unsigned path) { COST_SCOPE("obstacles:path_wall_locked");
    if(path>=6 || !mod_features.path_wall || route_hook_breach || !valid(army))return false;
    const Progress *p=path_wall_progress(army);
    return p && p->path_wall_lock_id && p->path_wall_lock_hex<48400;
}
static bool path_wall_done(Obj army,unsigned path=0) { COST_SCOPE("obstacles:path_wall_done");
    (void)path;
    if(!mod_features.path_wall || route_hook_breach || !valid(army))return false;
    const Progress *p=path_wall_progress(army);
    return p && p->path_wall_done;
}
static void path_wall_release(Obj army,Progress &p,unsigned reason) { COST_SCOPE("obstacles:path_wall_release");
    unsigned id=p.path_wall_lock_id,hex=p.path_wall_lock_hex;
    bool same_army=!army[0x140] && p.army==army && p.root==world() &&
        p.commander==word(army,0x12) && p.force==army_force(army);
    if(same_army && army[0x37]==6 && army[0x38]==11 && word(army,0x3A)==id) {
        // PW22: native standby exactly like the existing neutral-wall cancel.
        fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
        fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,13);
        fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,255,0);
        fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(army);
    }
    if(reason==1 || reason==2)log_event("facility_destroyed_resume",army_id(army),id,hex);
    log_event("path_wall_lock_release",army_id(army),id,reason);
    p.path_wall_lock_id=0;p.path_wall_lock_hex=48400;p.path_wall_lock_day=0;
    p.path_wall_done=reason!=4;   // feature off keeps today's behaviour
    p.destroy_id=0;p.destroy_hex=48400;p.destroy_reason=0;
    p.destroy_type=0;p.destroy_owner=0;p.destroy_goal=0;p.destroy_progress=0;
}
static void refresh_path_wall(Obj army,Progress &p) { COST_SCOPE("obstacles:refresh_path_wall");
    // [combat] path_wall (child of obstacle, user 2026-11-09: its own installer item).
    // local17 (user 2026-11-10): computer forces only; player direct armies and delegated corps never take it.
    // local55: the every-fifth pick runs with route_hook too; its order then goes through route_hook::assign_fifth.
    if(!mod_features.path_wall || !p.campaign || p.returning || !computer_side_army(army))return;
    unsigned aid=army_id(army);
    int day=game_day();
    // PW13: +0xE2 is the commander's home base, not the army's. Keep the
    // area-of-current-position lookup only when that value is not a city.
    unsigned commander_id=word(army,0x12);
    Obj commander=table(0x158,commander_id,6000);
    unsigned origin=valid(commander) && word(commander,0x10)==commander_id?word(commander,0xE2):0;
    unsigned home=path_wall_city_id(origin)?origin:0;
    // MOD_DEV_LESSONS D1: every early return says why (once per army per 10 days).
    auto skip=[&](unsigned reason) {
#ifndef NO_RUNTIME_LOG
        if(!computer_army(army) || !aid)return;
        static Obj log_world=nullptr;
        static int skip_day[501]={0};
        static volatile LONG skip_lines=0;
        Obj root=world();
        if(root!=log_world) {log_world=root;for(unsigned i=0;i<501;++i)skip_day[i]=-1000000;}
        if(day>=skip_day[aid] && day-skip_day[aid]<10)return;
        skip_day[aid]=day;
        if(InterlockedIncrement(&skip_lines)<=600)
            log_event("path_wall_skip",aid,reason,(home<<16)|p.city);
#else
        (void)reason;
#endif
    };
    if(word(army,0x16)<3000) {skip(1);return;}
    if(word(army,0x2A)>=48400) {skip(2);return;}
    unsigned goal=campaign_goal_hex(p);
    if(goal>=48400) {skip(3);return;}
    // PW14: only the army's origin city qualifies now (the target city does not).
    if(!path_wall_city(home)) {skip(4);return;}
    if(p.path_wall_goal==goal && day>=p.path_wall_checked && day-p.path_wall_checked<10)return;
    static int cap_day=-1;
    static unsigned cap_count=0;
    if(day!=cap_day) {cap_day=day;cap_count=0;}
    if(cap_count>=8) {skip(5);return;}
    if(route_hook_breach ? p.path_wall_done : path_wall_done(army,7)) {skip(7);return;}     // PW22: a finished army keeps its count rank
    ++cap_count;
    unsigned from=word(army,0x2A);
    // PW14: rank first; only every fifth army of an origin picks a waypoint.
    static wall_path::NthCounter nth;
    static Obj nth_world=nullptr;
    Obj root=world();
    if(root!=nth_world) {nth.clear();nth_world=root;}
    unsigned rank=nth.rank(home,aid|(static_cast<unsigned>(word(army,0x12))<<16),day);
    p.path_wall_checked=day;p.path_wall_goal=static_cast<unsigned short>(goal);
    if(rank%wall_path::NthCounter::period) {
        p.path_wall_hex=48400;
        static volatile LONG spared;
        if(InterlockedIncrement(&spared)<=200)log_event("path_wall_spared",aid,home,rank);
        return;
    }
    // PW15 (user 2026-11-09): no path search at all. Every fifth army walks to
    // the neutral wall (토성/석벽/철쇄) next to its origin city with the
    // smallest detour; the existing try_destroy path then sends it there.
    WallAreaTable wall_areas;
    std::vector<unsigned> wall_hexes;
    for(unsigned id=1;id<=3000;++id) {
        Obj b=table(0x6DA98,id,3000);
        if(!valid(b))continue;
        unsigned kind=b[0x12];
        if(!((kind==9 || kind==10 || kind==16) && word(b,0x14) && !b[0x13]))continue;
        if(!wall_path::serves(campaign_area_id(word(b,0x10)),home,wall_areas))continue;
        // local226 (user 2026-11-18 "투석대랑 망루 사거리 ... 거기 밖에서 중립건물 부쉈으면해"): only walls this army can hit from
        // outside every hostile 망루/투석대 range (route_hook::wall_stand).
        if(route_hook_breach && !route_hook::wall_has_stand(army,word(b,0x10)))continue;
        wall_hexes.push_back(word(b,0x10));
    }
    // local106b (user 2026-11-13, replaces the local106 path search): no path search; a wall with a hostile army within
    // 2 hexes (local106c) is picked only when every candidate has one (then the one farthest from its enemy).
    std::vector<int> enemy(wall_hexes.size(),1<<20);
    for(unsigned i=1;i<=500 && !wall_hexes.empty();++i) {
        Obj b=table(0x7E838,i,500);
        if(!valid(b) || b==army || b[0x140] || word(b,0x2A)>=48400)continue;
        if(!fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(army_force(army),army_force(b)))continue;
        for(size_t k=0;k<wall_hexes.size();++k)enemy[k]=(std::min)(enemy[k],distance(word(b,0x2A),wall_hexes[k]));
    }
    int pick=wall_path::pick_waypoint(wall_hexes,enemy,from,goal);
    if(pick>=0) {
        unsigned hex=wall_hexes[static_cast<unsigned>(pick)];
        p.path_wall_hex=static_cast<unsigned short>(hex);p.path_wall_day=day;
        if(route_hook_breach) {   // local55: held by route_hook until the wall falls; this army is not picked again
            p.path_wall_hex=48400;
            if(route_hook::assign_fifth(army,hex))p.path_wall_done=true;
        }
        int detour=wall_path::distance(from,hex)+wall_path::distance(hex,goal)-wall_path::distance(from,goal);
        const int max_detour=(1<<20)-1;
        unsigned gain=detour<0?0u:detour>max_detour?static_cast<unsigned>(max_detour):static_cast<unsigned>(detour);
        static volatile LONG picks;
        if(InterlockedIncrement(&picks)<=200) {
            log_event("path_wall_pick",aid,hex,(rank<<20)|gain);
            // value = candidates | nearest hostile army of the pick (hexes, 4095 = none) << 16
            const int e=enemy[static_cast<unsigned>(pick)];
            log_event("path_wall_pick_enemy",aid,static_cast<unsigned>(wall_hexes.size())|(static_cast<unsigned>(e>4095?4095:e)<<16),hex);
        }
    } else {
        p.path_wall_hex=48400;
        static volatile LONG clear_lines;
        if(InterlockedIncrement(&clear_lines)<=200)log_event("path_wall_clear",aid,home,p.city<<16);
    }
}
static bool path_wall_fresh(const Progress &p,Obj b) { COST_SCOPE("obstacles:path_wall_fresh");
    if(!mod_features.path_wall || route_hook_breach || !p.campaign || !valid(b) || p.returning || !computer_side_army(p.army))return false;
    // PW22: the locked mission neither expires after 20 days nor follows goal changes.
    if(p.path_wall_lock_id)
        return p.path_wall_lock_hex<48400 && p.path_wall_lock_hex==word(b,0x10);
    return p.path_wall_hex && p.path_wall_hex<48400 &&
        p.path_wall_hex==word(b,0x10) && game_day()>=p.path_wall_day &&
        game_day()-p.path_wall_day<=20 && p.path_wall_goal==campaign_goal_hex(p);
}
static unsigned neutral_wall_reason(Obj army,const Progress &p,Obj b) { COST_SCOPE("obstacles:neutral_wall_reason");
    if(!mod_features.obstacle)return 0;
    if(b[0x13]!=0 || (b[0x12]!=9 && b[0x12]!=10 && b[0x12]!=16))return 0;
    // local107 (user 2026-11-13 "피격대응하고 관문 돌파는 무슨상관이야? 우리 모드끼리 충돌하면 안돼"): the hit response
    // (player 안행 / water-formation fire over a neutral wall) is independent of route_hook again. It only steps aside
    // for an army route_hook already holds (breach or morale), so the two never order the same army.
    if(route_hook_owns && route_hook_owns(army))return 0;
    // local108: with route_hook live the hit response runs there (detour check, sticky order); this old path is the
    // fallback when route_hook is absent.
    if(!route_hook_hit && ranged_wall(army,b))return 2;
    if(route_hook_breach)return 0;   // U39: the oscillation/path_wall reasons stay with route_hook
    if(!p.returning && p.campaign && obstacle_oscillating(p.obstacle) &&
       obstacle_between(word(army,0x2A),p.obstacle.goal,word(b,0x10))) {
        for(unsigned i=1;i<p.obstacle.count;++i)
            if(distance(p.obstacle.hex[0],p.obstacle.hex[i])>12)return 0;
        return 1;
    }
    if(path_wall_fresh(p,b))return 3;
    return 0;
}
static bool neutral_mission_valid(Obj army,const Progress &p,Obj b) { COST_SCOPE("obstacles:neutral_mission_valid");
    // Authorization is latched for at most 20 days so approaching the obstacle
    // does not itself erase the oscillation that justified the attack.
    if(p.destroy_reason==3)return path_wall_fresh(p,b);
    if(p.destroy_reason==2)return ranged_wall(army,b,20);
    return p.destroy_reason==1 && p.campaign && !p.returning &&
        p.destroy_goal==p.obstacle.goal && p.destroy_progress==p.progress_day &&
        obstacle_between(word(army,0x2A),p.destroy_goal,word(b,0x10));
}
static void observe_obstacle_motion(Obj army,Progress &p) { COST_SCOPE("obstacles:observe_obstacle_motion");
    if(p.returning || !p.campaign || word(army,0x16)<3000 || word(army,0x2A)>=48400) {
        p.obstacle.count=0;return;
    }
    unsigned hex=campaign_goal_hex(p);
    if(hex>=48400) {p.obstacle.count=0;return;}
    obstacle_sample(p.obstacle,game_day(),word(army,0x2A),hex,
                    distance(word(army,0x2A),hex),p.progress_day);
}
