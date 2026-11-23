#include "game_addresses.h"
/* Included after the native adapter helpers. All orders run on the game's AI
 * thread. Dates use its own 30-day calendar conversion, not wall-clock time. */
static bool construction_completed(const Progress &p) { COST_SCOPE("campaign:construction_completed");
    if(p.finished_build || p.finished_demolition)return game_day()<=p.return_deadline_day;
    if(!p.facility || p.construction_hex>=48400)return false;
    Obj tile=table(0xE270,p.construction_hex,48399);
    if(!valid(tile) || !word(tile,0x1A))return false;
    Obj building=fn<Obj(__fastcall *)(Obj)>(GA(GA_2315A0))(tile);
    if(!valid(building) || building[0x13]!=p.force || building[0x12]!=p.facility)return false;
    int maximum=fn<int(__fastcall *)(Obj)>(GA(GA_2AE450))(building);
    return maximum>0 && word(building,0x14)>=maximum;
}
static bool attack_mode(unsigned mode) { return mode==8 || mode==9 || mode==10 || mode==11 || mode==22; }
// AI-CONTRACT [policy]: Game days use the native 30-day calendar. Preserve actual hostile castle/gate damage and cohort-only 30-day forced-march exceptions before ordinary 40-day stationary retreat. Do not infer a siege lock from an attack order alone.
static constexpr int campaign_stationary_days=40;
static Obj campaign_goal(unsigned city) { COST_SCOPE("campaign:campaign_goal");
    if (!city || city>=52) return nullptr;
    Obj goal=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(world(),city);
    return valid(goal) && word(goal,0x4E)==city ? goal : nullptr;
}
static bool hostile_city(Obj army, unsigned city) { COST_SCOPE("campaign:hostile_city");
    Obj goal=campaign_goal(city);
    Obj force=goal ? fn<Obj(__fastcall *)(Obj)>(GA(GA_22E3E0))(goal) : nullptr;
    if (!valid(force)) return false;
    auto id=*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(force,0)+0x60);
    return fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(army_force(army),id(force))!=0;
}
// local47 no_tribe_target: a castle of a tribe force (46..50) is never a mod target either (the original never lets a
// non-player force take one, 285DB0).
static bool tribe_goal(unsigned city) { COST_SCOPE("campaign:tribe_goal");
    if (!mod_features.no_tribe_target) return false;
    Obj goal=campaign_goal(city);
    Obj force=goal ? fn<Obj(__fastcall *)(Obj)>(GA(GA_22E3E0))(goal) : nullptr;
    if (!valid(force)) return false;
    const unsigned id=(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(force,0)+0x60))(force);
    return id>=46 && id<=50;
}
static bool campaign_transit_region(unsigned id);
static unsigned campaign_path_capture_hex(Obj army,unsigned destination);
static unsigned campaign_area_id(unsigned hex);
static bool campaign_region_open(Obj army,unsigned id);
static void remember_campaign(Obj army, Progress &p) { COST_SCOPE("campaign:remember_campaign");
    if(p.returning || p.forced_return) {p.campaign=false;return;}
    if(p.operation_city && campaign_operation_member(army,p)) {
        p.city=p.operation_city;p.campaign=true;return;
    }
    if(p.campaign && (!hostile_city(army,p.city) || tribe_goal(p.city))) {
        p.campaign=false;
    }
    if (word(army,0x16)<1000) { p.campaign=false; return; }
    Obj troops=table(0x7F8D8,word(army,0x58),500);
    if (!valid(troops)) { p.campaign=false; return; }
    unsigned mode=word(troops,0x14), city=word(troops,0x16);
    // Recover independent workers from their native orders after loading.
    // Their friendly source-city group goal must not become an assault goal.
    unsigned capture_region=army[0x38]==9?campaign_area_id(word(army,0x3A)):0;
    bool capture_order=army[0x37]==1 && capture_region &&
        word(table(0x6CAF0,capture_region,500),0x3A)==word(army,0x3A) && !campaign_transit_region(capture_region);
    if(!p.regional_worker && word(army,0x16)<=1000 && attack_mode(mode) && !hostile_city(army,city) &&
        (capture_order || army[0x37]==8 || army[0x37]==9)) {
        unsigned home=city;
        if(!campaign_goal(home) || hostile_city(army,home)) {
            Obj person=table(0x158,word(army,0x12),6000);home=valid(person)?word(person,0xE2):0;
        }
        p.regional_worker=true;p.work_source=static_cast<unsigned short>(home);
        if(capture_order) {p.region_target=word(army,0x3A);
            Obj t=table(0xE270,p.region_target,48399);p.region_owner=valid(t)?t[0x14]:0;}
    }
    if(p.regional_worker) {p.campaign=false;p.siege_city=0;return;}
    if (attack_mode(mode) && hostile_city(army,city)) {
        if (!p.campaign || p.city!=city) {
            p.city=static_cast<unsigned short>(city); p.region_target=48400;
            p.staging_hex=48400;
            log_event("campaign_remembered",army_id(army),city,word(army,0x2A));
        }
        p.campaign=true;
    } else if (p.campaign && (p.built_once || p.did_demolition) && mode==7 && construction_completed(p) && hostile_city(army,p.city)) {
        /* A construction detour must not replace the saved offensive goal
         * with a return HEX. Only restore a group whose every healthy member
         * belongs to this same remembered campaign. */
        bool consistent=true;
        for(unsigned i=1;i<=500;++i) {
            Obj member=table(0x7E838,i,500);
            if (!valid(member) || !belongs_to_troops(member,troops)) continue;
            Progress *m=observe_capture(member);
            if (!computer_army(member) || word(member,0x16)<1000 || !m || !m->campaign || m->city!=p.city) { consistent=false; break; }
        }
        if (consistent) {
            *reinterpret_cast<unsigned short *>(troops+0x14)=10;
            *reinterpret_cast<unsigned short *>(troops+0x16)=p.city;
            log_event("construction_return_replaced",army_id(army),p.city,word(army,0x2A));
        } else p.campaign=false;
    } else p.campaign=false;
    if (p.campaign && !hostile_city(army,p.city)) {
        p.campaign=false;
    }
}
static bool region_reserved(unsigned hex, Obj actor) { COST_SCOPE("campaign:region_reserved");
    for(unsigned i=1;i<=500;++i) {
        Obj other=table(0x7E838,i,500);
        if (other==actor || !computer_army(other) || army_force(other)!=army_force(actor)) continue;
        Progress &p=progress[i];
        if (p.army==other && p.commander==word(other,0x12) && p.region_target==hex) return true;
        if(p.army==other && p.root==world() && p.commander==word(other,0x12) &&
           p.near_capture_hex==hex && game_day()>=p.near_capture_day && game_day()-p.near_capture_day<10)return true;
        if (word(other,0x2C)==hex && (dword(other,0x1F0)&2)) return true;
    }
    return false;
}
static void offensive_order(Obj army, unsigned city, bool clear) { COST_SCOPE("campaign:offensive_order");
    if(path_wall_locked(army,0) || path_wall_done(army,0)) return; // path_wall lock
    if (clear) fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
    fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,4);
    fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,5,static_cast<int>(city));
}
static bool campaign_transit_region(unsigned id);
#ifdef SORTIE_FORMATION
static bool campaign_request_companion(Obj army,Progress &p,unsigned target);
#else
static bool campaign_request_companion(Obj,Progress &,unsigned) {return false;}
#endif
#include "native_campaign_routes.inl"
[[maybe_unused]] static bool assign_region(Obj army, Progress &p, unsigned hex) { COST_SCOPE("campaign:assign_region");
    return campaign_assign_safe(army,p,hex,true);
}
// 2 blocks city assault until the graph corridor/guarded route is available.
static bool campaign_request_guard(Obj army,Progress &p,unsigned region,unsigned goal);
static bool campaign_transit_relay(Obj army,Progress &p,unsigned region,unsigned goal,bool plan);
static bool campaign_defensive_wait(Obj army,Progress &p);
static bool campaign_guard_priority(Obj army,Progress &p);
static bool campaign_transit_region(unsigned id) { COST_SCOPE("campaign:campaign_transit_region");
    Obj area=table(0x6CAF0,id,500);
    return valid(area) && (fn<int(__fastcall *)(Obj)>(GA(GA_234020))(area) || fn<int(__fastcall *)(Obj)>(GA(GA_234590))(area));
}
static bool campaign_request_companion(Obj army,Progress &p,unsigned target);
// Adjacency selects missions only; every movement path remains native.
static bool campaign_adjacent_city_base(unsigned hex,unsigned city) { COST_SCOPE("campaign:campaign_adjacent_city_base");
    if(!city || city>61 || hex>=48400)return false;
    Obj goal=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(world(),city);
    if(!valid(goal) || word(goal,0x4E)!=city)return false;
    unsigned h=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(goal);
    unsigned region=campaign_area_id(h),base=campaign_area_id(hex);
    Obj area=table(0x6CAF0,region,500);if(!valid(area) || !base || base==region || campaign_transit_region(base))return false;
    for(unsigned i=0;i<8;++i)if((area[0x66]&(1u<<i)) && word(area,0x56+i*2)==base)return true;
    return false;
}
static std::vector<unsigned> campaign_home_bases(Obj army,unsigned only_city=0) { COST_SCOPE("campaign:campaign_home_bases");
    std::vector<unsigned> result;unsigned depth[501]={};unsigned here=word(army,0x2A),own=army_force(army);
    for(unsigned id=1;id<=51;++id) {
        if(only_city && only_city!=id)continue;
        Obj city=campaign_goal(id);if(!city)continue;
        unsigned h=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(city);Obj t=table(0xE270,h,48399);
        if(!valid(t) || t[0x14]!=own || (!only_city && distance(here,h)>20))continue;
        Obj area=table(0x6CAF0,campaign_area_id(h),500);if(!valid(area))continue;
        unsigned source_region=campaign_area_id(h);if(!source_region)continue;
        std::vector<std::pair<unsigned,unsigned>> queue={{source_region,0}};bool visited[501]={};visited[source_region]=true;
        for(size_t k=0;k<queue.size();++k) {
            unsigned layer=queue[k].second;if(layer==2)continue;
            Obj from=table(0x6CAF0,queue[k].first,500);if(!valid(from))continue;
            for(unsigned i=0;i<8;++i) {
                unsigned n=word(from,0x56+i*2);if(!(from[0x66]&(1u<<i)) || !n || n>500 || visited[n])continue;
                visited[n]=true;Obj base=table(0x6CAF0,n,500);
                if(!valid(base) || campaign_transit_region(n) || !campaign_region_open(army,n))continue;
                queue.push_back({n,layer+1}); // Owned/reserved first-ring bases still lead to the second ring.
                if(!campaign_region_hostile(army,n))continue;
                unsigned center=word(base,0x3A);if(center>=48400 || region_reserved(center,army))continue;
                if(!depth[n]) {result.push_back(center);depth[n]=layer+1;}
                else depth[n]=(std::min)(depth[n],layer+1);
            }
        }
    }
    std::sort(result.begin(),result.end(),[here,&depth](unsigned a,unsigned b){
        unsigned da=depth[campaign_area_id(a)],db=depth[campaign_area_id(b)];if(da!=db)return da<db;
        int ra=distance(here,a),rb=distance(here,b);return ra!=rb?ra<rb:a<b;});
    return result;
}
static bool recovery_return(Obj army,Progress &p);
static unsigned campaign_path_capture_hex(Obj army,unsigned destination) {
    COST_SCOPE("campaign_path_capture_hex");
    CampaignRoute onward=campaign_safe_route(army,destination,false,true);
    // Inspect regions in native travel order, including the current region.
    // A port is terrain 17, not a city/gate assault target.
    for(unsigned region:onward.regions) {
        Obj a=table(0x6CAF0,region,500);if(!valid(a) || fn<int(__fastcall *)(Obj)>(GA(GA_234020))(a) ||
            !campaign_region_hostile(army,region))continue;
        unsigned center=word(a,0x3A);if(center>=48400)continue;
        CampaignRoute capture=campaign_safe_route(army,center,false,true);
        if(!capture.cells.empty())return center;
    }
    return 48400;
}
static bool campaign_near_capture(Obj army,Progress &p) {
    COST_SCOPE("campaign_near_capture");
    if(p.regional_worker || campaign_group_member(army) || p.operation_leader || p.facility || p.credit || p.build_chain || p.forced_return || p.returning || p.morale_recovering ||
       p.combat_target || p.combat_city || p.home_defense_city || p.guard_beneficiary || priority_mission(army,p))return false;
    Obj group=table(0x7F8D8,word(army,0x58),500);
    unsigned city=p.operation_city?p.operation_city:p.campaign?p.city:valid(group)?word(group,0x16):0;
    if(!city || !hostile_city(army,city) || word(army,0x16)<1000)return false;
    unsigned own=army_force(army);
    if(p.near_capture_hex<48400) {
        Obj t=table(0xE270,p.near_capture_hex,48399);
        if(valid(t) && t[0x14]!=own && game_day()>=p.near_capture_day && game_day()-p.near_capture_day<50) {
            CampaignRoute r=campaign_safe_route(army,p.near_capture_hex,false,true);
            if(!r.cells.empty() && campaign_route_order(army,r))return true;
        }
        p.near_capture_hex=48400;
    }
    Obj goal=campaign_goal(city);unsigned destination=goal?fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(goal):48400;
    if(p.region_target<48400 && campaign_region_hostile(army,campaign_area_id(p.region_target)))destination=p.region_target;
    else if(army[0x37]==1 && army[0x38]==9 && word(army,0x3A)<48400)destination=word(army,0x3A);
    unsigned h=campaign_path_capture_hex(army,destination);
    if(h<48400 && h!=destination && !region_reserved(h,army)) {
        bool occupied=false;for(unsigned id=1;id<=500;++id) {Obj other=table(0x7E838,id,500);
            if(other!=army && valid(other) && word(other,0x2A)==h) {occupied=true;break;}}
        if(occupied)return false;
        CampaignRoute r=campaign_safe_route(army,h,false,true);
        if(r.cells.empty() || !campaign_route_order(army,r))return false;
        p.near_capture_hex=static_cast<unsigned short>(h);p.near_capture_day=game_day();
        p.near_capture_owner=table(0xE270,h,48399)[0x14];
        *reinterpret_cast<unsigned *>(army+0x1F0)|=2;
        log_event("campaign_native_path_region_capture",army_id(army),h,city);return true;
    }
    return false;
}
// [combat] retake_adjacent, the unit's own steering (local85, user 2026-11-12 "관평 요화 장비 모두 제일 부대가 많은 곳으로
// 바로 돌격", earlier "적부대를 회피기동하면서 스마트하게 이동경로를 짜서 점령 수복"). Replay 17:47: the two retake units
// left for 지강 and 화용, then the home-capture worker step picked its base again every turn (nearest hostile base, and
// the first hostile region on the way) and turned them to 장판, where the player's armies stood. A retake unit now
//   1. keeps the base it was sent to until that base is taken back; then it takes the next lost first-ring base of its
//      castle that has no hostile army within 2 hexes, or goes home when there is none;
//   2. walks around hostile armies: when the original path comes within 2 hexes of a hostile army, a terrain-cost
//      search (original terrain costs BF1B0, blocking facilities as the original path search) that never enters a
//      hex next to a hostile army and pays extra 2 and 3 hexes away gives the way, written as the original move
//      order with up to five relay points (2F05A0); the original path search still makes every step;
//   3. goes home when no such way exists (a hostile army stands on or next to the base: the interception's business).
// Decided once per turn; nothing in the save.
static bool retake_hostile_near(unsigned own,unsigned hex,int range) { COST_SCOPE("campaign:retake_hostile_near");
    for(unsigned i=1;i<=500;++i) {
        Obj a=table(0x7E838,i,500);if(!valid(a))continue;
        const unsigned force=army_force(a);
        if(!force || force==own || word(a,0x2A)>=48400 || distance(word(a,0x2A),hex)>range)continue;
        if(fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,force))return true;
    }
    return false;
}
// local86 (user 2026-11-12 "출진 첫번째 중계 위치 자체가 적하고 만날수밖에 없는곳 ... 첫 중계 지점이 적쪽이니까 가장 가까운
// 북쪽으로 나가는거지", "뺏기기전부터 나오지"). Replay 18:00: the three units of 강릉 appeared two hexes north of the
// castle, on the player's side, and stood there in contact. A unit leaves its castle toward its first relay point, so
// the way around hostile armies is now part of the departure itself: its relay points go into the departure command
// and into the first order, and the unit comes out on that side.
static constexpr int kRetakeThreat=8;   // a hostile army this near an own first-ring base = the base gets a guard
static constexpr int kRetakePortReach=16;   // a second-ring port farther than this from the castle is left out (first-ring p90 17)
static constexpr unsigned char kRetakePortTerrain=18;   // port center tile (KB rules_movement: 16 castle, 17 gate, 18 port)
// local89 (user 2026-11-12 "적부대가 제일 많이 서있는 거점 1개를 최대 위협으로 설정하고 나머지 지역은 수복대상 ... 최대 위협지역을
// 우회하는 인접거점을 전부 점령하면서 거점 점령을 역으로 뒤집고 ... 1차 인접선과 이어진 다른 지역도 ... 아군 점령률이 높은
// 지역은 적이 점령한 타일을 뒤집으려고(병참차단) 시도하고 지역버프를 그 이후에 수복").
//   rings   the castle's first-ring regions and the regions next to those (castle/gate regions left out; second ring:
//           no region next to another castle's region, ports only within kRetakePortReach hexes, local143);
//   threat  the one ring region with the most hostile armies within 8 hexes of its base: no unit is sent there, no
//           unit takes it, and no way crosses it (local90);
//   targets every other ring region whose base is not ours: a unit takes its base, then the nearest next one (first
//           ring before the second), and goes home only when none is left;
//   cut     a unit whose target region holds more of our hexes than hostile ones lets the original supply-cut node go
//           first (retake_cut_first); otherwise it walks to the base at once.
struct RetakeRings {unsigned home,threat;std::vector<unsigned> ring1,ring2;};
static RetakeRings retake_rings(unsigned own,unsigned city_id) { COST_SCOPE("campaign:retake_rings");
    RetakeRings r{0,0,{},{}};
    Obj city=campaign_goal(city_id);if(!city)return r;
    r.home=campaign_area_id(fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(city));
    Obj home=table(0x6CAF0,r.home,500);if(!r.home || !valid(home))return r;
    bool seen[501]={};seen[r.home]=true;
    auto spread=[&](Obj from,std::vector<unsigned> &ring) {
        for(unsigned i=0;i<8;++i) {
            const unsigned n=word(from,0x56+i*2);if(!(from[0x66]&(1u<<i)) || !n || n>500 || seen[n])continue;
            seen[n]=true;
            if(!valid(table(0x6CAF0,n,500)) || campaign_transit_region(n))continue;
            ring.push_back(n);
        }
    };
    spread(home,r.ring1);
    // local143 (user 2026-11-14 "지역수복 2차링을 부활 ... 다른성의 1차인접지역이 아닌 지역을 필터링 걸고, 항구도 2차 링에 포함 ...
    // 우리 성에서 가까운 항구여야하는데 너무 먼 항구로 가는건 곤란해"): the second ring is back (local139 had dropped it because
    // 신야 units walked on to 극양, a first-ring region of 완). A region next to a first-ring region joins it only when no
    // neighbour of it is the castle region of another castle (234020, any owner), so no unit walks into another castle's
    // first line; a port region (center terrain 18, never a castle region) joins only within kRetakePortReach hexes of
    // this castle. Units are still sent only for first-ring bases (retake_adjacent_tick); the second ring is where a unit
    // goes on after its base, 6 hexes of penalty behind the first ring (retake_step).
    const unsigned castle_hex=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(city);
    auto other_castle_line=[&](Obj area) {
        for(unsigned i=0;i<8;++i) {
            const unsigned k=word(area,0x56+i*2);if(!(area[0x66]&(1u<<i)) || !k || k>500 || k==r.home)continue;
            Obj a=table(0x6CAF0,k,500);
            if(valid(a) && fn<int(__fastcall *)(Obj)>(GA(GA_234020))(a))return true;
        }
        return false;
    };
    const std::vector<unsigned> first=r.ring1;
    for(unsigned n1:first) {
        Obj from=table(0x6CAF0,n1,500);if(!valid(from))continue;
        for(unsigned i=0;i<8;++i) {
            const unsigned n=word(from,0x56+i*2);if(!(from[0x66]&(1u<<i)) || !n || n>500 || seen[n])continue;
            seen[n]=true;
            Obj area=table(0x6CAF0,n,500);if(!valid(area) || campaign_transit_region(n) || other_castle_line(area))continue;
            const unsigned base=word(area,0x3A);Obj tile=base<48400?table(0xE270,base,48399):nullptr;if(!valid(tile))continue;
            if(tile[0x10]==kRetakePortTerrain && (castle_hex>=48400 || distance(castle_hex,base)>kRetakePortReach))continue;
            r.ring2.push_back(n);
        }
    }
    // local90 (user 2026-11-12 "첫번째 출진은 그냥 바로 직행 ... 그 거점 타일 자체를 우회하길"; replay 19:02: the first unit
    // left while the player's armies were still one region away from 장판, so 장판 was no threat yet and the unit walked
    // across its base into them). An army counts for every ring base it is within kRetakeThreat hexes of, standing
    // in the region or still coming; the first ring wins a tie.
    unsigned most=0;
    for(const std::vector<unsigned> *ring:{&r.ring1,&r.ring2})for(unsigned n:*ring) {
        const unsigned base=word(table(0x6CAF0,n,500),0x3A);if(base>=48400)continue;
        unsigned count=0;
        for(unsigned i=1;i<=500;++i) {
            Obj a=table(0x7E838,i,500);if(!valid(a))continue;
            const unsigned force=army_force(a);
            if(!force || force==own || word(a,0x2A)>=48400 || distance(word(a,0x2A),base)>kRetakeThreat)continue;
            if(fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,force))++count;
        }
        if(count>most) {most=count;r.threat=n;}
    }
    return r;
}
static bool retake_cut_first(Obj army,const Progress &p) { COST_SCOPE("campaign:retake_cut_first");
    const unsigned target=p.region_target,own=army_force(army);if(target>=48400)return false;
    const unsigned area=campaign_area_id(target);if(!area)return false;
    unsigned mine=0,theirs=0;
    for(unsigned h=0;h<48400;++h) {
        Obj tile=table(0xE270,h,48399);if(!valid(tile) || word(tile,0x12)!=area)continue;
        const unsigned holder=tile[0x14];
        if(holder==own)++mine;
        else if(holder && fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,holder))++theirs;
    }
    return theirs && mine>theirs;
}
// local92 (user 2026-11-12 "적의 투석대(3헥스 사정거리) 망루(2헥스 사정거리)에 벌점 ... 그러나 그 때문에 거점 점령이 포기되어선
// 안 됨"): a hex inside the range of a hostile catapult (kind 6, 3 hexes) or watchtower (kind 5, 2 hexes) costs 3 extra
// hexes in the way search; it is never closed. One still under construction does not count (local93).
static bool retake_tower(Obj b,unsigned own,unsigned &reach) {
    if(!valid(b) || !word(b,0x14) || word(b,0x10)>=48400 || (b[0x12]!=5 && b[0x12]!=6))return false;
    if(!(b[0x16]&1))return false;   // local93 (user: "건설중인 망루랑 투석대는 벌점을 먹이면 안되지"): +0x16 bit0 = completed (2DAE00)
    const unsigned holder=b[0x13];
    if(!holder || holder==own || !fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,holder))return false;
    reach=b[0x12]==6?3u:2u;return true;
}
static bool retake_tower_covers(unsigned own,unsigned hex) { COST_SCOPE("campaign:retake_tower_covers");
    for(unsigned id=62;id<=3000;++id) {
        Obj b=table(0x6DA98,id,3000);unsigned reach=0;
        if(retake_tower(b,own,reach) && distance(word(b,0x10),hex)<=static_cast<int>(reach))return true;
    }
    return false;
}
// land_mul (local269): a ship's own way (helm_route, helm departures) prices every hex outside helm_water (terrain 13, 14, 24)
// land_mul times its terrain cost.
// water_only (local271): every hex outside the river (13, 14, 24) and the ports (kRetakePortTerrain) is closed, except the
// target (a bank goal): a ship already afloat never crosses land (local270 log: a land hex costing 8 times still crossed the
// 건업 island, 3000 -> 882 soldiers, never on water).
// why (local271): 0 found, 1 out of range (off the map, over 40 hexes, outside the box), 2 target closed (no cost, next to a
// hostile army, threat region), 3 every neighbour of the start closed, 4 searched without a way.
enum : unsigned {kPathFound=0,kPathRange=1,kPathTarget=2,kPathStart=3,kPathNone=4};
static bool retake_path(unsigned own,unsigned from,unsigned target,std::vector<unsigned> &cells,unsigned avoid_area=0,unsigned land_mul=1,
                        bool water_only=false,unsigned *why=nullptr) { COST_SCOPE("campaign:retake_path");
    cells.clear();
    if(why)*why=kPathFound;
    if(from>=48400 || target>=48400) {if(why)*why=kPathRange;return false;}
    if(from==target) {cells.push_back(from);return true;}
    const int x0=static_cast<int>(from%220),y0=static_cast<int>(from/220),x1=static_cast<int>(target%220),y1=static_cast<int>(target/220);
    if(distance(from,target)>40) {if(why)*why=kPathRange;return false;}
    const int lx=(std::max)(0,(std::min)(x0,x1)-12),hx=(std::min)(219,(std::max)(x0,x1)+12);
    const int ly=(std::max)(0,(std::min)(y0,y1)-12),hy=(std::min)(219,(std::max)(y0,y1)+12);
    const int w=hx-lx+1,h=hy-ly+1;
    auto index=[&](unsigned hex)->int {
        if(hex>=48400)return -1;
        const int x=static_cast<int>(hex%220),y=static_cast<int>(hex/220);
        return x<lx || x>hx || y<ly || y>hy?-1:(y-ly)*w+(x-lx);
    };
    std::vector<unsigned> cost(static_cast<size_t>(w)*h,0),best(static_cast<size_t>(w)*h,0xFFFFFFFFu);
    std::vector<int> back(static_cast<size_t>(w)*h,-1);
    std::vector<unsigned char> danger(static_cast<size_t>(w)*h,255),heavy(static_cast<size_t>(w)*h,0),tower(static_cast<size_t>(w)*h,0);
    for(int y=ly;y<=hy;++y)for(int x=lx;x<=hx;++x) {
        Obj tile=table(0xE270,static_cast<unsigned>(y*220+x),48399);
        Obj terrain=valid(tile)?fn<Obj(__fastcall *)(Obj)>(GA(GA_233310))(tile):nullptr;
        const int c=valid(terrain)?fn<int(__fastcall *)(Obj)>(GA(GA_BF1B0))(terrain):0;
        unsigned step=c>0?static_cast<unsigned>(c):0u;
        const bool river=valid(tile) && (tile[0x10]==13 || tile[0x10]==14 || tile[0x10]==24);
        if(step && water_only && !river && !(valid(tile) && tile[0x10]==kRetakePortTerrain) && static_cast<unsigned>(y*220+x)!=target)step=0;
        if(step && land_mul>1 && !river)step*=land_mul;
        cost[static_cast<size_t>((y-ly)*w+(x-lx))]=step;
        if(avoid_area && valid(tile) && word(tile,0x12)==avoid_area)heavy[static_cast<size_t>((y-ly)*w+(x-lx))]=1;
    }
    for(unsigned id=62;id<=3000;++id) {   // facilities the original path search never crosses
        Obj b=table(0x6DA98,id,3000);if(!valid(b) || !word(b,0x14))continue;
        unsigned reach=0;
        if(retake_tower(b,own,reach)) {   // catapult / watchtower range: extra cost only
            const unsigned at=word(b,0x10);const int bx=static_cast<int>(at%220),by=static_cast<int>(at/220);
            for(int y=(std::max)(ly,by-4);y<=(std::min)(hy,by+4);++y)for(int x=(std::max)(lx,bx-4);x<=(std::min)(hx,bx+4);++x)
                if(distance(at,static_cast<unsigned>(y*220+x))<=static_cast<int>(reach))tower[static_cast<size_t>((y-ly)*w+(x-lx))]=1;
        }
        if(word(b,0x10)==from || word(b,0x10)==target)continue;
        const int k=index(word(b,0x10));if(k<0)continue;
        Obj type=fn<Obj(__fastcall *)(Obj)>(GA(GA_EDF0))(b);
        if(valid(type) && (type[0x2C]==2 || type[0x2C]==3))cost[static_cast<size_t>(k)]=0;
    }
    for(unsigned i=1;i<=500;++i) {   // rings 0..3 around every hostile army
        Obj a=table(0x7E838,i,500);if(!valid(a))continue;
        const unsigned force=army_force(a),at=word(a,0x2A);
        if(!force || force==own || at>=48400 || !fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,force))continue;
        const int ax=static_cast<int>(at%220),ay=static_cast<int>(at/220);
        if(ax<lx-4 || ax>hx+4 || ay<ly-4 || ay>hy+4)continue;
        unsigned ring[64],count=0;ring[count++]=at;
        for(unsigned depth=0,first=0;depth<=3;++depth) {
            const unsigned last=count;
            for(unsigned k=first;k<last;++k) {
                const int cell=index(ring[k]);
                if(cell>=0 && danger[static_cast<size_t>(cell)]>depth)danger[static_cast<size_t>(cell)]=static_cast<unsigned char>(depth);
                if(depth==3)continue;
                for(unsigned d=0;d<6;++d) {
                    const unsigned next=sortie::neighbor(ring[k],d);if(next>=48400 || count>=64)continue;
                    bool seen=false;for(unsigned s=0;s<count && !seen;++s)seen=ring[s]==next;
                    if(!seen)ring[count++]=next;
                }
            }
            first=last;
        }
    }
    const int start=index(from),goal=index(target);
    if(start<0 || goal<0) {if(why)*why=kPathRange;return false;}
    // local90: the largest threat region is not entered at all; a unit already inside it pays the 10 hexes instead.
    const bool leaving=heavy[static_cast<size_t>(start)]!=0;
    using Node=std::pair<unsigned,int>;
    std::priority_queue<Node,std::vector<Node>,std::greater<Node>> open;
    best[static_cast<size_t>(start)]=0;open.push({0u,start});
    while(!open.empty()) {
        const Node now=open.top();open.pop();
        if(now.first!=best[static_cast<size_t>(now.second)])continue;
        if(now.second==goal)break;
        const unsigned hex=static_cast<unsigned>((now.second/w+ly)*220+(now.second%w+lx));
        for(unsigned d=0;d<6;++d) {
            const int next=index(sortie::neighbor(hex,d));if(next<0)continue;
            const unsigned step=cost[static_cast<size_t>(next)],risk=danger[static_cast<size_t>(next)];
            if(!step || risk<=1 || (heavy[static_cast<size_t>(next)] && !leaving))continue;
            const unsigned total=now.first+step+(risk==2?960u:risk==3?480u:0u)+(heavy[static_cast<size_t>(next)]?2400u:0u)+(tower[static_cast<size_t>(next)]?720u:0u);
            if(total<best[static_cast<size_t>(next)]) {best[static_cast<size_t>(next)]=total;back[static_cast<size_t>(next)]=now.second;open.push({total,next});}
        }
    }
    if(best[static_cast<size_t>(goal)]==0xFFFFFFFFu) {
        if(why) {
            auto closed=[&](int k) {return !cost[static_cast<size_t>(k)] || danger[static_cast<size_t>(k)]<=1 || (heavy[static_cast<size_t>(k)] && !leaving);};
            bool open_start=false;
            for(unsigned d=0;d<6 && !open_start;++d) {const int k=index(sortie::neighbor(from,d));open_start=k>=0 && !closed(k);}
            *why=closed(goal)?kPathTarget:!open_start?kPathStart:kPathNone;
        }
        return false;
    }
    for(int k=goal;k>=0;k=back[static_cast<size_t>(k)])cells.push_back(static_cast<unsigned>((k/w+ly)*220+(k%w+lx)));
    std::reverse(cells.begin(),cells.end());
    return cells.size()>=2 && cells.front()==from;
}
static bool retake_route_order(Obj army,Progress &p,unsigned target,unsigned avoid_area=0) { COST_SCOPE("campaign:retake_route_order");
    if(campaign_castle_locked(army) || native_search_depth || target>=48400)return false;
    const int day=game_day();const unsigned own=army_force(army),here=word(army,0x2A);
    if(p.retake_day==day && army[0x37]==1 && army[0x38]==9 && word(army,0x3A)==target && word(army,0x48)<48400)return true;
    if(here==target)return true;
    // local87: still inside its castle = the departure order stands. The original departure (1EDC30) searches the way
    // from the castle to the first relay and stores the first hex outside the castle (army +0x48); rewriting the order
    // before the unit is out replaced that hex and the unit came out on the default side (replays 17:47 - 18:16: always
    // two hexes north of 강릉, whatever the base).
    Obj origin=fn<Obj(__fastcall *)(Obj)>(GA(GA_22BFE0))(army);
    const bool inside=valid(origin) && fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(origin)==here;
    if(inside && army[0x37]==1 && army[0x38]==9 && word(army,0x3A)==target && word(army,0x48)<48400)return true;
    // The original way first: kept as it is when it stays 3 or more hexes away from every hostile army.
    CampaignRoute plain=campaign_safe_route(army,target,false,true);
    bool clear=!plain.cells.empty();
    for(size_t i=1;clear && i<plain.cells.size();++i)
        clear=!retake_hostile_near(own,plain.cells[i],2) && !(avoid_area && campaign_area_id(plain.cells[i])==avoid_area) &&
              !retake_tower_covers(own,plain.cells[i]);
    unsigned length=static_cast<unsigned>(plain.cells.size()),mode=1;
    bool ordered=clear && campaign_route_order(army,plain);
    if(!ordered) {
        std::vector<unsigned> cells;
        if(!retake_path(own,here,target,cells,avoid_area))return false;
        unsigned points[5];const unsigned count=campaign_relay_points(cells,static_cast<unsigned>(cells.size()-1),points);
        if(!count)return false;
        unsigned char saved[0x34];memcpy(saved,army+0x2C,sizeof(saved));
        const unsigned flags=dword(army,0x1F0);const unsigned short future=word(army,0x1F8);
        fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
        fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,1);
        fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,9,static_cast<int>(target));
        for(unsigned i=0;i<count;++i)fn<void(__fastcall *)(Obj,int,unsigned)>(GA(GA_2F05A0))(army,static_cast<int>(i),points[i]);
        fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(army);
        if(word(army,0x48)>=48400 || distance(here,word(army,0x48))!=1) {
            memcpy(army+0x2C,saved,sizeof(saved));*reinterpret_cast<unsigned *>(army+0x1F0)=flags;
            *reinterpret_cast<unsigned short *>(army+0x1F8)=future;
            return false;
        }
        length=static_cast<unsigned>(cells.size());mode=2;
    }
    *reinterpret_cast<unsigned *>(army+0x1F0)|=2;
    p.region_target=static_cast<unsigned short>(target);p.region_owner=table(0xE270,target,48399)[0x14];
    p.retake_day=day;p.returning=false;
    // value = cells of the way | straight distance << 8 | (1 original way, 2 way around hostile armies) << 16
    log_event("retake_route",army_id(army),(length&0xFF)|((static_cast<unsigned>(distance(here,target))&0xFF)<<8)|(mode<<16),target);
    return true;
}
static bool retake_step(Obj army,Progress &p) { COST_SCOPE("campaign:retake_step");
    const unsigned own=army_force(army),here=word(army,0x2A);
    const RetakeRings rings=retake_rings(own,p.work_source);
    // A base worth walking to now: not ours, outside the largest threat, no hostile army within 2 hexes of it.
    auto open=[&](unsigned base) {
        const unsigned area=base<48400?campaign_area_id(base):0;
        return area && area!=rings.threat && campaign_region_hostile(army,area) && !retake_hostile_near(own,base,2);
    };
    const unsigned target=p.region_target;
    if(open(target) && retake_route_order(army,p,target,rings.threat))return true;
    // The next base: nearest first, the first ring before the second.
    std::vector<std::pair<int,unsigned>> next;
    for(const std::vector<unsigned> *ring:{&rings.ring1,&rings.ring2})for(unsigned n:*ring) {
        const unsigned base=word(table(0x6CAF0,n,500),0x3A);
        if(base==target || !open(base) || region_reserved(base,army))continue;
        next.push_back({distance(here,base)+(ring==&rings.ring2?6:0),base});
    }
    std::sort(next.begin(),next.end());
    for(const auto &entry:next)if(retake_route_order(army,p,entry.second,rings.threat)) {
        log_event("retake_next",army_id(army),target,entry.second);
        return true;
    }
    // Nothing left to take: a base of ours with a hostile army within kRetakeThreat hexes is held, otherwise home.
    Obj kept=target<48400?table(0xE270,target,48399):nullptr;
    if(valid(kept) && kept[0x14]==own && campaign_area_id(target)!=rings.threat && retake_hostile_near(own,target,kRetakeThreat)) {
        p.motion_day=game_day();   // standing on the base is the mission, not a stall
        if(here==target || retake_route_order(army,p,target,rings.threat))return true;
    }
    if(!p.returning)log_event("retake_done_return",army_id(army),word(army,0x16),here);
    return recovery_return(army,p);
}
// [combat] gate_intercept (user 2026-11-13 "아군의 관문이 공격당하면 관문소속성이나 관문에서 안행진이 2000명으로 나와서 적이 관문
// 공격하는 반대방향에서 세워놔야해. 적 - 관문 - 안행진 ... 다른 원작의 명령을 받으면 안되겠지"). A gate of a computer force with a
// hostile army within kInterceptAlert hexes of its center gets one 2,000-soldier 안행 unit (from the gate when it can
// spare it, else from the nearest own castle). The unit is a home-capture worker like the retake units (group kind 10,
// group tree skipped by retake_tree_hook, first army node answered here), so no original order reaches it. It walks
// to the stand behind the gate (intercept::pick: a free hex touching the gate's own tiles, on the far side from the
// nearest attacker and as near it as possible, never next to an attacker), waits there and fights whatever comes in range, follows the stand when the
// attackers move, and goes home after kInterceptQuiet days without a hostile army near the gate, when the gate is lost,
// or under kInterceptMinimum soldiers.
static unsigned intercept_base_owner(Obj base) {   // as campaign_city_owner (some fixtures build this file without it)
    Obj force=valid(base)?fn<Obj(__fastcall *)(Obj)>(GA(GA_22E3E0))(base):nullptr;if(!valid(force))return 0;
    return (*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(force,0)+0x60))(force);
}
// local104: the step runs on the 10-day ticks (live log 2026-11-13: both units went home on the first empty tick before reaching
// the stand); two empty ticks are needed before going home.
static constexpr int kInterceptAlert=3,kInterceptQuiet=20;
static constexpr unsigned kInterceptMinimum=500;
static unsigned intercept_attackers(unsigned own,unsigned center,int range,unsigned *out,unsigned limit) { COST_SCOPE("campaign:intercept_attackers");
    unsigned count=0;
    for(unsigned i=1;i<=500 && count<limit;++i) {
        Obj a=table(0x7E838,i,500);if(!valid(a))continue;
        const unsigned force=army_force(a),at=word(a,0x2A);
        if(!force || force==own || at>=48400 || distance(at,center)>range)continue;
        if(fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,force))out[count++]=at;
    }
    return count;
}
static bool intercept_open(unsigned hex,Obj self) {   // walkable, no building, nobody standing there
    Obj tile=hex<48400?table(0xE270,hex,48399):nullptr;
    if(!valid(tile) || word(tile,0x1A))return false;
    Obj terrain=fn<Obj(__fastcall *)(Obj)>(GA(GA_233310))(tile);
    if(!valid(terrain) || fn<int(__fastcall *)(Obj)>(GA(GA_BF1B0))(terrain)<=0)return false;
    for(unsigned i=1;i<=500;++i) {Obj a=table(0x7E838,i,500);if(a!=self && valid(a) && word(a,0x2A)==hex)return false;}
    return true;
}
// local179: the gate's stands in pick() order (out[0] = intercept_stand), *front = the equally best ones (one unit each).
static unsigned intercept_stands(unsigned own,unsigned center,Obj self,unsigned *out,unsigned limit,unsigned *front) { COST_SCOPE("campaign:intercept_stands");
    if(front)*front=0;
    unsigned attackers[64];const unsigned n=intercept_attackers(own,center,kInterceptAlert+1,attackers,64);
    if(!n || center>=48400)return 0;
    // The gate's own tiles carry its building id (+0x1A, KB structs.md: center and the six around it); the stand
    // touches one of them (user: "관문 1헥스 범위에 딱붙여야돼").
    Obj middle=table(0xE270,center,48399);const unsigned gate_id=valid(middle)?word(middle,0x1A):0;
    auto own_tile=[&](unsigned h) {Obj t=h<48400?table(0xE270,h,48399):nullptr;return h==center || (gate_id && valid(t) && word(t,0x1A)==gate_id);};
    std::vector<unsigned> body,open;
    const int cx=static_cast<int>(center%220),cy=static_cast<int>(center/220);
    for(int y=(std::max)(0,cy-3);y<=(std::min)(219,cy+3);++y)for(int x=(std::max)(0,cx-3);x<=(std::min)(219,cx+3);++x) {
        const unsigned h=static_cast<unsigned>(y*220+x);
        if(intercept::distance(h,center)<=2 && own_tile(h))body.push_back(h);
    }
    for(unsigned b:body)for(unsigned d=0;d<6;++d) {
        const unsigned h=sortie::neighbor(b,d);
        if(h>=48400 || own_tile(h) || std::find(open.begin(),open.end(),h)!=open.end() || !intercept_open(h,self))continue;
        open.push_back(h);
    }
    return intercept::ranked(center,attackers,n,open.data(),static_cast<unsigned>(open.size()),out,limit,front);
}
// local179: up to kInterceptUnits units per gate, one per equally best stand behind it.
static constexpr unsigned kInterceptUnits=3;
// The stands the other intercept units of this gate hold (their region_target), so each unit takes its own hex.
static unsigned intercept_claims(unsigned own,unsigned gate,Obj self,unsigned *out,unsigned limit) {
    unsigned count=0;
    for(unsigned i=1;i<=500 && count<limit;++i) {
        Obj a=table(0x7E838,i,500);if(!valid(a) || a==self || army_force(a)!=own)continue;
        const Progress &q=progress[i];
        if(q.army==a && q.root==world() && q.commander==word(a,0x12) && q.intercept && !q.helm && q.intercept_gate==gate &&
           !q.returning)out[count++]=q.region_target;
    }
    return count;
}
// The best stand no sibling holds; *front_free = whether it is one of the equally best stands.
static unsigned intercept_free_stand(unsigned own,unsigned center,unsigned gate,Obj self,bool *front_free=nullptr) {
    if(front_free)*front_free=false;
    unsigned stands[64],front=0,claims[16];
    const unsigned n=intercept_stands(own,center,self,stands,64,&front),c=intercept_claims(own,gate,self,claims,16);
    for(unsigned k=0;k<n;++k) {
        bool taken=false;for(unsigned j=0;j<c && !taken;++j)taken=claims[j]==stands[k];
        if(taken)continue;
        if(front_free)*front_free=k<front;
        return stands[k];
    }
    return 48400;
}
static Obj intercept_gate_object(unsigned gate) {
    if(gate<52 || gate>61)return nullptr;
    Obj g=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(world(),gate);
    return valid(g) && word(g,0x4E)==gate?g:nullptr;
}
// [combat] helm_intercept (local148, user 2026-11-14 "건업 시상 강하 여남 장사 무릉에 있는 조타 개성 장수들은 시정이 수군육성 Lv 5 이상
// (몽충, 누선) 일때 바다에서 적극적으로 요격 ... 원작 명령을 안받고 바다에 나가서 적부대 인근 3칸에 벌점을 주고 적 주변 타일을 뒤집는데
// 큰 점수 ... 아군 항구의 점령률이 떨어지면 출진"; local149 "거리 상관없이 출진해서 색칠"). The ship rides the intercept plumbing (no
// original order) and is steered here: it paints the hexes of its port region (area id) that the force lost - no distance
// limit to the castle or to the attackers; hexes within 3 of a hostile army are penalised hard. It re-picks daily, and goes
// home when the whole port region is the force's again or under 1,000 soldiers; a lost port is retaken first (nearest to the castle).
static constexpr int kHelmScan=20;   // half side of the box searched for the port region's hexes around its base
static constexpr unsigned kHelmMinimum=1000;
static constexpr int kHelmNear=15;   // local152: the attackers' penalty applies only once the ship is this close to one
static constexpr int kHelmSweep=12;  // local161: farthest target (hexes); the way there is what the ship paints
static constexpr int kHelmRingBonus=3;      // enemy tile 4..7 hexes from the attackers: counts 3+3 on the way (local162; local263 4..7)
static constexpr int kHelmRingGoal=40;       // goal on that ring (local162)
static bool helm_water(unsigned hex) {
    Obj tile=hex<48400?table(0xE270,hex,48399):nullptr;
    return valid(tile) && (tile[0x10]==13 || tile[0x10]==14 || tile[0x10]==24);
}
// local169 (user 2026-11-15 "조타 장수들은 출진할때 육지에서 바다로 나가고 바다에서 다시 적 육지로 가는거 자제 ... 장강을 계속 색칠해서
// 보급선을 유지"): the ship's goals are river water or the bank beside it (a land hex with a water neighbour); inland hexes are
// neither picked nor counted as left to paint.
static bool helm_shore(unsigned hex) {
    if(helm_water(hex))return true;
    if(hex>=48400)return false;
    for(unsigned d=0;d<6;++d)if(helm_water(sortie::neighbor(hex,d)))return true;
    return false;
}
// local170 (user 2026-11-15 "몽충,누선 상태에서 적 타일로 색칠된 육지로 올라가는걸 자제하고 장강 임무를 계속"): a ship's goal is
// river water, or a bank hex not held by a hostile force; hostile land is turned only by the footprint from the water.
static bool helm_hostile_tile(unsigned own,unsigned hex) {
    Obj t=hex<48400?table(0xE270,hex,48399):nullptr;if(!valid(t))return false;
    const unsigned holder=t[0x14];
    return holder && holder!=own && fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,holder)!=0;
}
static bool helm_goal_ok(unsigned own,unsigned hex) {return helm_water(hex) || (helm_shore(hex) && !helm_hostile_tile(own,hex));}
// local170 (user 2026-11-15 "육지의 투석대나 망루 범위에 반응 못하는 문제 ... 그걸 회피해야하는데"): hostile finished 투석대 (6,
// range 3) and 망루 (5, range 2) (buildings world+0x6DA98: +0x10 hex, +0x12 kind, +0x13 force, +0x16 bit0 finished); goals in
// their range are skipped and each step of the way in range costs. Siege missions are not helm ships and still land to break them.
struct HelmTower {unsigned short hex;unsigned char range;};
static unsigned helm_towers(unsigned own,unsigned centre,int box,HelmTower *out,unsigned cap) {
    unsigned n=0;if(centre>=48400)return 0;
    for(unsigned i=1;i<=3000 && n<cap;++i) {
        Obj b=table(0x6DA98,i,3000);
        if(!valid(b) || (b[0x12]!=5 && b[0x12]!=6) || !(b[0x16]&1))continue;
        const unsigned hex=word(b,0x10),holder=b[0x13];
        if(hex>=48400 || !holder || holder==own || distance(hex,centre)>box)continue;
        if(fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,holder)==0)continue;
        out[n++]=HelmTower{static_cast<unsigned short>(hex),static_cast<unsigned char>(b[0x12]==6?3:2)};
    }
    return n;
}
static bool helm_tower_hit(const HelmTower *t,unsigned n,unsigned hex) {for(unsigned i=0;i<n;++i)if(distance(hex,t[i].hex)<=t[i].range)return true;return false;}
static Obj helm_area_object(unsigned area) {Obj a=area && area<=500?table(0x6CAF0,area,500):nullptr;return valid(a)?a:nullptr;}
// The best hex of the port region to paint: a hex of the region not held by the force (hostile 30, neutral 10), +6 for each
// hostile neighbour, +10 on water (the 조타 ship ignores ZOC there), -200 within 3 hexes of a hostile army, minus 2 per hex
// of the way. left = hexes of the region still to paint (0 = the region is whole again).
static unsigned helm_pick(unsigned own,unsigned area_id,unsigned here,Obj self,int *best_score=nullptr,unsigned *left=nullptr,
                          int reach=12,const unsigned *avoid=nullptr,unsigned navoid=0,bool allow_far=true,int near_min=0,bool water_only=false) { COST_SCOPE("campaign:helm_pick");
    if(left)*left=0;
    Obj area=helm_area_object(area_id);
    const unsigned port=area?word(area,0x3A):48400;
    if(port>=48400)return 48400;
    unsigned foes[64];const unsigned nf=intercept_attackers(own,port,kHelmScan+4,foes,64);
    HelmTower towers[64];const unsigned nt=helm_towers(own,port,kHelmScan+kHelmSweep+3,towers,64);
    auto hostile=[&](unsigned q) {
        Obj t=q<48400?table(0xE270,q,48399):nullptr;if(!valid(t))return false;
        const unsigned holder=t[0x14];
        return holder && holder!=own && fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,holder)!=0;
    };
    const int px=static_cast<int>(port%220),py=static_cast<int>(port/220);
    // local152 (user 2026-11-14 "점령률 낮아졌는데 출진 안하는건 문제 ... 일단 적방향으로 출진하고 적하고 15칸 안으로 들어왔을때
    // 벌점을 부여"): the best hex always wins whatever its score (before, a score <= 0 was never picked: far ports and hexes near
    // the attackers left no target, helm_no_target). The ship (or the castle, at departure) more than kHelmNear hexes from every
    // attacker heads toward them (+1 per hex closer to the nearest one, no penalty); within kHelmNear the hexes within 3 of an
    // attacker are penalised.
    int foe_gap=99;if(here<48400)for(unsigned i=0;i<nf;++i)foe_gap=(std::min)(foe_gap,distance(here,foes[i]));
    const bool foe_close=foe_gap<=kHelmNear;
    // local161 (user 2026-11-15 "조타 장수들 1cm씩 움직이는거 뭐야 목표지점을 중계점으로 찍어서 매턴 최대 기동력을 쓰면서 미점령 or
    // 적타일을 최대한 많이 뒤집어야지"): before, -2 per hex of the way made the nearest unpainted hex win, one hex a day. Now a hex
    // up to kHelmSweep hexes away is scored by what the ship paints on the way there: every hex of the straight walk (each step to
    // the neighbour nearest the target) and its six neighbours (the footprint), counted once, not held by the force: neutral 2,
    // hostile 3. The target is kept until it is reached or no longer worth it (helm_step), so the ship sails the whole way.
    auto holder_value=[&](unsigned q)->int {
        Obj t=q<48400?table(0xE270,q,48399):nullptr;if(!valid(t))return 0;
        const unsigned holder=t[0x14];
        if(!holder)return 2;
        if(holder==own)return 0;
        return fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,holder)!=0?3:0;
    };
    // local162 (user 2026-11-15 "적부대를 감싸는 4헥스부터 6헥스 사이에 and 적타일에 포상 ... 원으로 감싸서 병참단절을 유도"): an enemy
    // tile 4..6 hexes from the nearest attacker (a ring around the whole group, outside the 3-hex penalty) counts double on the way,
    // and a goal on that ring gets +kHelmRingGoal, so the ship circles the attackers and turns the tiles their supply runs over.
    auto in_ring=[&](unsigned q)->bool {
        int m=99;for(unsigned i=0;i<nf;++i)m=(std::min)(m,distance(q,foes[i]));
        return m>=4 && m<=7;   // local263 (was 4..6)
    };
    auto sweep=[&](unsigned to,int &near_foes,int &tower_steps)->int {
        near_foes=0;tower_steps=0;
        if(here>=48400)return 0;
        unsigned seen[(kHelmSweep+1)*7];unsigned ns=0;int gain=0;
        auto add=[&](unsigned q) {
            if(q>=48400)return;
            for(unsigned i=0;i<ns;++i)if(seen[i]==q)return;
            if(ns<sizeof(seen)/sizeof(seen[0]))seen[ns++]=q;
            const int v=holder_value(q);
            gain+=v;
            if(v==3 && in_ring(q))gain+=kHelmRingBonus;
        };
        unsigned c=here;
        for(int step=0;step<reach && step<kHelmSweep && c!=to;++step) {
            unsigned next=c;int nd=distance(c,to);
            for(unsigned d=0;d<6;++d) {const unsigned n=sortie::neighbor(c,d);if(n<48400 && distance(n,to)<nd) {nd=distance(n,to);next=n;}}
            if(next==c)break;
            c=next;add(c);for(unsigned d=0;d<6;++d)add(sortie::neighbor(c,d));
            if(foe_close)for(unsigned i=0;i<nf;++i)if(distance(c,foes[i])<=3) {++near_foes;break;}
            if(helm_tower_hit(towers,nt,c))++tower_steps;
        }
        return gain;
    };
    unsigned best=48400,count=0,far_best=48400;int top=-(1<<30),far_top=-(1<<30);
    for(int y=(std::max)(0,py-kHelmScan);y<=(std::min)(219,py+kHelmScan);++y)
        for(int x=(std::max)(0,px-kHelmScan);x<=(std::min)(219,px+kHelmScan);++x) {
            const unsigned h=static_cast<unsigned>(y*220+x);
            Obj t=table(0xE270,h,48399);
            if(!valid(t) || word(t,0x12)!=area_id || t[0x14]==own || !helm_goal_ok(own,h) || helm_tower_hit(towers,nt,h))continue;   // local170
            ++count;
            if(water_only && !helm_water(h))continue;   // local273: relays are water hexes (count = the region's hexes left, banks too)
            if(!intercept_open(h,self))continue;
            if(here<48400 && distance(here,h)>reach) {   // out of one sweep (e.g. the castle at departure): nearest first
                const int far_score=-1000+(hostile(h)?30:10)-2*distance(here,h);
                if(allow_far && far_score>far_top) {far_top=far_score;far_best=h;}
                continue;
            }
            if(near_min && here<48400 && distance(here,h)<near_min)continue;   // local263: the first point at least near_min hexes on
            {bool near_avoid=false;for(unsigned i=0;i<navoid;++i)if(distance(h,avoid[i])<=3)near_avoid=true;if(near_avoid)continue;}
            int closest=99;for(unsigned i=0;i<nf;++i)closest=(std::min)(closest,distance(h,foes[i]));
            int near_foes=0,tower_steps=0;
            int score=100+(hostile(h)?30:10)+10*sweep(h,near_foes,tower_steps);
            score-=40*tower_steps;   // local170: each step in a 투석대/망루 range
            if(helm_water(h))score+=10;
            if(hostile(h) && closest>=4 && closest<=7)score+=kHelmRingGoal;   // local263 (was 4..6)
            if(foe_close) {if(closest<=3)score-=200;score-=40*near_foes;}
            else if(nf)score+=kHelmScan+4-(std::min)(closest,kHelmScan+4);
            if(score>top) {top=score;best=h;}
        }
    if(best>=48400 && far_best<48400) {best=far_best;top=far_top;}
    if(best_score)*best_score=top;
    if(left)*left=count;
    return best;
}
// local161 (user 2026-11-15 "목표지점을 중계점으로 찍어서 ... 중계점 5개 찍는거지?"): one order through five relay points and a goal,
// each leg (at most kHelmLeg hexes) picked from the end of the previous one by helm_pick (what the ship paints on the way), never
// within 3 hexes of an earlier point. Written as retake_route_order's own relay branch writes it (A3370, 2ECEA0 1, 2ECF00 9 goal,
// 2F05A0 per relay, 26E260); a refused order (no first step) is restored and the plain way to the goal is taken instead.
static constexpr int kHelmLeg=6;
static constexpr unsigned kHelmRelays=5;
// local263 (user 2026-11-19 "조타이거 내 생각하고 다르네.. v16.3.1 버전으로 되돌아가는 형식으로 가자"): the helm code is the v16.3.1
// one (local186) again - no prey (local208/209), no supply marks (local210), no 20-hex pads (local261), adjacency rules of
// v16.3.1 (penalty within 3 hexes) - with: ring bonus 4..7 hexes (was 4..6) and the first point at least kHelmFirstMin hexes on.
static constexpr int kHelmFirstMin=8;
// local269 (user 2026-11-19 "조타 장수들이 육지를 가로질러가는 경로를 찍네 중계점은 대하에 찍혀있는데 중간에 육지를 거쳐가는 경로를
// 찍어서 육지위로 올라가버려"): the native search between two relays takes land whenever land is cheaper (terrain cost 대하 300,
// 평지 225, 가도 150, then the formation's mobility per hex, 187880), so river relays alone never kept a ship on the river. Every
// helm move order goes through helm_route instead of helm_chain_order / retake_route_order:
// - the ship's own way through the asked points in order: retake_path with every land hex costing kHelmLandMul (hostile army
//   rings and tower ranges as before); an intermediate asked point on land is passed by; a point with no way ends the way there;
// - relays every kHelmStride hexes of that way (five relays and a goal at most; local269 3 hexes = 18 hexes, local270 below);
// - the order's native route is read back: the whole way through the relays, the way the native movement will search it
//   leg by leg (local270: 26E260 -> 303EA0 searches only to the first relay, 2B7450; the chain probe asks the same 185710 call
//   for all legs at once, CampaignChainProbe). A hex on land that the own way does not cross, or a leg the native search does
//   not find, marks its leg; the legs before it stay and the rest is laid again at the next shorter stride, down to hex by hex
//   (a one-hex leg has no other way). Still land then: the order ends before that leg, or the ship holds when it is the first
//   leg. A route that cannot be read (empty, or not from the ship) is never taken as checked: the points are laid hex by hex.
// - no own way, or no first step: no order (the caller holds); never the plain retake way (it crosses land the same way).
// local270: relays every 6 hexes first (36 hexes at most, local268's reach: a ship on 대하 sails about 4 / 8 / 12.5 hexes a 순
// as 주가 / 몽충 / 누선, twice that with the force's water bonus), then 3, 2 and 1 from the leg that went over land.
// local271 (user 2026-11-20 "조타는 매순마다 끊임없이 움직여야하는데"): only the first leg of a ship standing on land may cross land
// (kHelmLandMul, to its first point: helm_step makes that the nearest water hex, helm_launch); every other leg is water only
// (retake_path water_only: river and ports, the leg's own point may be a bank).
static constexpr unsigned kHelmLandMul=8;
static constexpr unsigned kHelmStride=6;
// local271: why the last helm_route on this thread gave no order: 1..4 retake_path's reason for the first leg (kPathRange ..
// kPathNone), 5 the first hex goes over land (blocked), 6 no first step (refused), 7 not routed (castle lock, inside a search).
enum : unsigned {kHelmWhyBlocked=5,kHelmWhyRefused=6,kHelmWhyLocked=7};
static __declspec(thread) unsigned helm_route_why;
// The ship stands in the castle it left from (local87: the departure order stands there).
static bool helm_in_castle(Obj army,unsigned here) {
    Obj origin=fn<Obj(__fastcall *)(Obj)>(GA(GA_22BFE0))(army);
    return valid(origin) && fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(origin)==here;
}
// local271: the nearest water hex a ship on land reaches, by the shortest own way (natural terrain cost) of the nearest ring
// that has a way to one (kHelmLaunchTries ways asked at most); 48400 when afloat or none within kHelmLaunchScan hexes.
static constexpr int kHelmLaunchScan=6;
static constexpr unsigned kHelmLaunchTries=8;
static constexpr unsigned kHelmPicks=3;   // local271: picks tried in a day before the ship holds
static constexpr int kHelmTopUp=16;       // local271: a kept order with this many hexes or fewer left goes on past its goal
static unsigned helm_launch(unsigned own,unsigned here,unsigned *steps) { COST_SCOPE("campaign:helm_launch");
    if(steps)*steps=0;
    if(here>=48400 || helm_water(here))return 48400;
    const int cx=static_cast<int>(here%220),cy=static_cast<int>(here/220);
    std::vector<unsigned> cells;unsigned tries=0;
    for(int d=1;d<=kHelmLaunchScan && tries<kHelmLaunchTries;++d) {
        unsigned best=48400;size_t len=~size_t(0);
        for(int y=(std::max)(0,cy-d);y<=(std::min)(219,cy+d) && tries<kHelmLaunchTries;++y)
            for(int x=(std::max)(0,cx-2*d);x<=(std::min)(219,cx+2*d) && tries<kHelmLaunchTries;++x) {
                const unsigned h=static_cast<unsigned>(y*220+x);
                if(distance(here,h)!=d || !helm_water(h))continue;
                ++tries;
                if(retake_path(own,here,h,cells) && cells.size()<len) {len=cells.size();best=h;}
            }
        if(best<48400) {if(steps)*steps=static_cast<unsigned>(len-1);return best;}
    }
    return 48400;
}
// local271: the points an order still sails through: the relays not yet reached (2B7450: relay[0] at +0x2C is the next one,
// reached ones shift out) and the goal.
static unsigned helm_points_ahead(Obj army,unsigned goal,unsigned *out) {
    unsigned n=0;
    for(unsigned j=0;j<5;++j) {const unsigned r=word(army,0x2C+2*j);if(r<48400 && r!=goal)out[n++]=r;}
    if(goal<48400)out[n++]=goal;
    return n;
}
static void helm_order_write(Obj army,const unsigned *pts,unsigned np,std::vector<unsigned> *trace) {
    fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
    fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,1);
    fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,9,static_cast<int>(pts[np-1]));
    for(unsigned i=0;i+1<np;++i)fn<void(__fastcall *)(Obj,int,unsigned)>(GA(GA_2F05A0))(army,static_cast<int>(i),pts[i]);
    if(trace)trace->clear();
    // local270: one point = the native route is the whole way; with relays it reaches only pts[0], so the chain probe reads
    // the whole way in the same call (local269 compared the first leg with the goal: every chain counted as unreadable)
    CampaignChainProbe probe{word(army,0x2A),pts,np,trace,false};
    auto prior=campaign_native_trace;auto prior_probe=campaign_chain_probe;
    campaign_native_trace=np==1?trace:nullptr;campaign_chain_probe=trace && np>1?&probe:nullptr;
    fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(army);
    campaign_native_trace=prior;campaign_chain_probe=prior_probe;
#ifndef NO_RUNTIME_LOG
    // value = probe answered (the matching 185710 call came) | chain cells << 1 | points << 13; hex = the ship
    if(trace && np>1)log_event("helm_route_probe",army_id(army),(probe.done?1u:0u)|
        (static_cast<unsigned>((std::min)(trace->size(),size_t(4095)))<<1)|(np<<13),word(army,0x2A));
#endif
}
// The leg (0-based, the one ending at pts[leg]) whose native route first steps on land the own way does not cross, jumps
// (185710 goes on with the next leg when one is not found) or ends short of the goal; np = none, np + 1 = cannot be read.
static unsigned helm_dry_leg(const std::vector<unsigned> &trace,unsigned here,const unsigned *pts,unsigned np,const std::vector<unsigned> &way) {
    if(trace.size()<2 || trace.front()!=here)return np+1;
    unsigned leg=0;
    for(size_t i=1;i<trace.size();++i) {
        const unsigned c=trace[i];
        if(distance(trace[i-1],c)!=1 || (!helm_water(c) && std::find(way.begin(),way.end(),c)==way.end()))return (std::min)(leg,np-1);
        if(leg<np && c==pts[leg])++leg;
    }
    return leg<np?leg:np;
}
static bool helm_route(Obj army,Progress &p,const unsigned *ask,unsigned nask) { COST_SCOPE("campaign:helm_route");
    helm_route_why=kHelmWhyLocked;
    if(!nask || ask[nask-1]>=48400 || campaign_castle_locked(army) || native_search_depth)return false;
    const unsigned own=army_force(army),here=word(army,0x2A),end=ask[nask-1];
    if(here==end) {helm_route_why=0;return true;}
    // local87: inside the castle the departure order stands (rewriting it moves the side the ship comes out on)
    if(helm_in_castle(army,here)) {const bool out=army[0x37]==1 && word(army,0x48)<48400;helm_route_why=out?0u:kHelmWhyLocked;return out;}
    std::vector<unsigned> way{here},leg;
    const bool afloat=helm_water(here);unsigned first_why=0;
    for(unsigned i=0;i<nask;++i) {
        const unsigned to=ask[i];
        if(to>=48400 || to==way.back() || (i+1<nask && !helm_water(to)))continue;
        const bool landing=!afloat && way.size()==1;   // local271: only the first leg of a ship on land crosses land
        unsigned why=0;
        if(!retake_path(own,way.back(),to,leg,0,landing?kHelmLandMul:1u,!landing,&why) || leg.size()<2) {if(way.size()==1)first_why=why;break;}
        way.insert(way.end(),leg.begin()+1,leg.end());
    }
    if(way.size()<2) {
        helm_route_why=first_why?first_why:kPathNone;
        // value = the ship's hex | why << 16 (1 range, 2 target closed, 3 start closed, 4 no way; local271), hex = goal
        log_event("helm_route_none",army_id(army),here|(helm_route_why<<16),end);return false;
    }
    unsigned pts[kHelmRelays+1];size_t at[kHelmRelays+1];unsigned np=0;
    auto lay=[&](unsigned keep,unsigned stride) {   // pts[0..keep-1] stay; then a point every stride hexes of the way
        np=keep;size_t k=keep?at[keep-1]:0;
        while(np<kHelmRelays+1 && k+1<way.size()) {
            k=(std::min)(k+stride,way.size()-1);
            // local270: never a point equal to the one before (a way turning back through it): 185710 reads the second node
            // of every later leg without a check
            if(way[k]==(np?pts[np-1]:here))continue;
            pts[np]=way[k];at[np++]=k;
        }
    };
    unsigned char saved[0x34];memcpy(saved,army+0x2C,sizeof(saved));
    const unsigned flags=dword(army,0x1F0);const unsigned short future=word(army,0x1F8);
    auto refused=[&]() {return word(army,0x48)>=48400 || distance(here,word(army,0x48))!=1;};
    std::vector<unsigned> trace;
    unsigned stride=kHelmStride,tries=0,dry=0,first_dry=15;bool blocked=false;
    lay(0,stride);
    for(;;) {
        helm_order_write(army,pts,np,&trace);++tries;
        if(refused())break;
        dry=helm_dry_leg(trace,here,pts,np,way);
        if(first_dry==15)first_dry=dry>np?14u:dry==np?15u:(std::min)(dry,13u);
        if(dry==np)break;
        if(dry>np) {if(stride==1)break;stride=1;lay(0,1);continue;}   // unreadable: hex by hex, taken as it is
        if(stride==1) {   // land even on a one-hex leg: end the order before that leg
            if(!dry) {blocked=true;break;}   // local270: the first hex itself goes over land: hold (local269 kept the order)
            np=dry;helm_order_write(army,pts,np,nullptr);++tries;
            break;
        }
        stride=stride>3u?3u:stride==3u?2u:1u;lay(dry,stride);
    }
    if(blocked || refused()) {
        memcpy(army+0x2C,saved,sizeof(saved));*reinterpret_cast<unsigned *>(army+0x1F0)=flags;
        *reinterpret_cast<unsigned short *>(army+0x1F8)=future;
        log_event(blocked?"helm_route_blocked":"helm_route_refused",army_id(army),pts[np-1],here);
        helm_route_why=blocked?kHelmWhyBlocked:kHelmWhyRefused;
        return false;
    }
    helm_route_why=0;
    *reinterpret_cast<unsigned *>(army+0x1F0)|=2;
    const unsigned goal=pts[np-1];
    p.region_target=static_cast<unsigned short>(goal);p.region_owner=table(0xE270,goal,48399)[0x14];
    p.retake_day=game_day();p.returning=false;p.helm_routed=true;
    // value = points | last stride << 4 | tries << 8 | first check (leg 0..13, 14 unreadable, 15 clean) << 12 | final check << 16
    //         | own way hexes << 20; hex = goal
    const unsigned last=dry>np?14u:dry==np?15u:(std::min)(dry,13u);
    log_event("helm_route",army_id(army),np|(stride<<4)|((std::min)(tries,15u)<<8)|(first_dry<<12)|(last<<16)|
              (static_cast<unsigned>((std::min)(way.size(),size_t(4095)))<<20),goal);
#ifndef NO_RUNTIME_LOG
    {unsigned q[kHelmRelays+1];for(unsigned i=0;i<kHelmRelays+1;++i)q[i]=i<np?pts[i]:48400u;
     log_event("helm_route_pts",army_id(army),q[0]|(q[1]<<16),q[2]|(q[3]<<16));
     if(np>4)log_event("helm_route_pts2",army_id(army),q[4]|(q[5]<<16),here);}
#endif
    return true;
}
// local269: a sailing order is left as it is while it came from helm_route and its next hex is water (or the goal); 303EA0
// searches again from the ship's hex during the turn, so helm_step looks at this every turn.
static bool helm_sailing_ok(Obj army,const Progress &p,unsigned target) {
    if(army[0x37]!=1 || army[0x38]!=9 || word(army,0x3A)!=target)return false;
    const unsigned next=word(army,0x48);
    return next<48400 && p.helm_routed && (helm_water(next) || next==target);
}
// local167 delayed retreat (user 2026-11-15 "우리 조타 요격나가는거 퇴각하는상황이 될때 지연 퇴각하게해 플래그 달아서 2턴동안
// 적타일 뒤집다가 퇴각하게 해줘"): the first reason to go home (worn, port lost, region whole) sets p.helm_delay for
// kHelmDelayDays (2 turns); meanwhile the ship sails to the hostile tile within kHelmLeg hexes whose straight way turns the
// most hostile tiles (each step and its six neighbours, +10 on water), never a tile within 3 hexes of a hostile army, and
// waits in place when there is none (local273: patrols our own water then, helm_delay_patrol). At the end it goes home as before. The garrison recall (castle under attack) still
// calls it home at once.
static constexpr int kHelmDelayDays=20;
static unsigned helm_delay_pick(unsigned own,unsigned here,Obj self,int *best_score) { COST_SCOPE("campaign:helm_delay_pick");
    if(best_score)*best_score=0;
    if(here>=48400)return 48400;
    auto hostile=[&](unsigned q) {
        Obj t=q<48400?table(0xE270,q,48399):nullptr;if(!valid(t))return false;
        const unsigned holder=t[0x14];
        return holder && holder!=own && fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,holder)!=0;
    };
    const int cx=static_cast<int>(here%220),cy=static_cast<int>(here/220);
    unsigned best=48400;int top=0;
    HelmTower towers[64];const unsigned nt=helm_towers(own,here,kHelmLeg+3,towers,64);   // local170
    for(int y=(std::max)(0,cy-kHelmLeg);y<=(std::min)(219,cy+kHelmLeg);++y)
        for(int x=(std::max)(0,cx-kHelmLeg);x<=(std::min)(219,cx+kHelmLeg);++x) {
            const unsigned h=static_cast<unsigned>(y*220+x);
            // local170: hostile tiles on the water only (no landing on hostile ground), never in a 투석대/망루 range
            if(h==here || distance(here,h)>kHelmLeg || !hostile(h) || !helm_water(h) || helm_tower_hit(towers,nt,h) || !intercept_open(h,self) || retake_hostile_near(own,h,3))continue;
            unsigned seen[(kHelmLeg+1)*7];unsigned ns=0;int gain=0;
            auto add=[&](unsigned q) {
                if(q>=48400)return;
                for(unsigned i=0;i<ns;++i)if(seen[i]==q)return;
                if(ns<sizeof(seen)/sizeof(seen[0]))seen[ns++]=q;
                if(hostile(q))++gain;
            };
            unsigned c=here;int tower_steps=0;
            for(int step=0;step<kHelmLeg && c!=h;++step) {
                unsigned next=c;int nd=distance(c,h);
                for(unsigned d=0;d<6;++d) {const unsigned n=sortie::neighbor(c,d);if(n<48400 && distance(n,h)<nd) {nd=distance(n,h);next=n;}}
                if(next==c)break;
                c=next;add(c);for(unsigned d=0;d<6;++d)add(sortie::neighbor(c,d));
                if(helm_tower_hit(towers,nt,c))++tower_steps;
            }
            const int score=10*gain+(helm_water(h)?10:0)-distance(here,h)-20*tower_steps;
            if(score>top) {top=score;best=h;}
        }
    if(best_score)*best_score=top;
    return best;
}
// local168 (user 2026-11-15 "2턴 지난다음에 퇴각 명령 내리기직전에 또 항구 점령률이 떨어지고 적부대가 있으면 새목표 잡아서 계속
// 타일을 뒤집었으면"): the hostile hexes of the port region counted when the delay began; at its end, when there are more of
// them now and a hostile army is within kHelmExtendNear hexes of the port base, the delay runs another kHelmDelayDays with a
// new target. Not for a worn ship (below kHelmMinimum soldiers).
static constexpr int kHelmExtendNear=6;
static unsigned helm_delay_hostile(unsigned own,Progress &p,unsigned here,Obj army) {
    unsigned left=0;helm_pick(own,p.helm_area,here,army,nullptr,&left,kHelmLeg);return left;
}
static unsigned helm_ring_lost(unsigned own,unsigned city_id,unsigned *held,unsigned *area);
struct HelmRing;static const HelmRing *helm_ring_of(unsigned city_id);static unsigned helm_ring_castle(unsigned city_id);
static bool helm_delay_patrol(Obj army,Progress &p,unsigned own,unsigned here,int day);   // local273, after helm_pad_pick
// local169: a ship of the river ring counts the ring's lost hexes and looks for attackers near the castle instead.
static unsigned helm_delay_count(unsigned own,Progress &p,unsigned here,Obj army) {
    if(p.helm_ring)return helm_ring_lost(own,p.helm_ring,nullptr,nullptr);
    return helm_area_object(p.helm_area)?helm_delay_hostile(own,p,here,army):0;
}
static bool helm_delay_step(Obj army,Progress &p,int day) { COST_SCOPE("campaign:helm_delay_step");
    const unsigned own=army_force(army),here=word(army,0x2A);
    if(day>=p.helm_delay_until) {
        Obj area=helm_area_object(p.helm_area);
        const unsigned port=p.helm_ring?helm_ring_castle(p.helm_ring):area?word(area,0x3A):48400;
        const unsigned left=helm_delay_count(own,p,here,army);
        if(word(army,0x16)>=kHelmMinimum && port<48400 && left>p.helm_delay_left && retake_hostile_near(own,port,kHelmExtendNear)) {
            log_event("helm_delay_extend",army_id(army),p.helm_delay_left|((std::min)(left,65535u)<<16),port);
            p.helm_delay_until=day+kHelmDelayDays;p.helm_delay_left=left;p.region_target=48400;
        }
        else {
            log_event("helm_delay_return",army_id(army),word(army,0x16),p.helm_area);
            recovery_return(army,p);return true;
        }
    }
    unsigned target=p.region_target;
    Obj t=target<48400?table(0xE270,target,48399):nullptr;
    HelmTower towers[64];const unsigned nt=valid(t)?helm_towers(own,target,4,towers,64):0;   // local170
    const bool keep=valid(t) && target!=here && t[0x14] && t[0x14]!=own &&
                    fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,t[0x14])!=0 &&
                    helm_water(target) && !helm_tower_hit(towers,nt,target) &&
                    intercept_open(target,army) && !retake_hostile_near(own,target,3);
    if(!keep) {
        int score=0;target=helm_delay_pick(own,here,army,&score);
        // local273: with no hostile tile region_target stays as it is (the patrol goal, helm_delay_patrol)
        if(target<48400) {p.region_target=static_cast<unsigned short>(target);log_event("helm_delay_target",army_id(army),static_cast<unsigned>(score),target);}
    }
    if(target<48400 && here!=target) {
        // already sailing there (local269: on a helm_route order whose next hex is water; else laid again, kept only over water)
        const bool sailing=army[0x37]==1 && army[0x38]==9 && word(army,0x3A)==target && word(army,0x48)<48400;
        if(sailing && helm_sailing_ok(army,p,target))return true;
        if(sailing && p.helm_routed && p.retake_day==day)return true;   // local271: today's order stands (helm_step runs twice a day)
        if(helm_route(army,p,&target,1))return true;
        if(sailing && helm_water(word(army,0x48)))return true;
        p.region_target=48400;
    }
    if(helm_delay_patrol(army,p,own,here,day))return true;   // local273: nothing to turn: patrol our own water
    if(army[0x37]!=13) {   // nothing safe to turn here: wait; the original combat fights in range
        fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
        fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,13);
        fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,255,0);
    }
    return true;
}
// local169 (user 2026-11-15 "건업은 이 위에 해릉항이 짧아서 그런지 잘 반응을 못하네"): besides the castle's ports, the castle's
// other regions with at least kHelmRiverHexes river hexes (the castle's own region on the 장강 included) are watched and
// painted. Water hexes per region are counted once a day.
static constexpr unsigned kHelmRiverHexes=3;
static unsigned helm_area_water(unsigned n) {
    static Obj root;static int day_of[501];static unsigned short water[501];
    if(root!=world()) {root=world();for(int &d:day_of)d=-1;}
    if(n<1 || n>500)return 0;
    const int day=game_day();
    if(day_of[n]==day)return water[n];
    day_of[n]=day;water[n]=0;
    Obj area=helm_area_object(n);const unsigned base=area?word(area,0x3A):48400;if(base>=48400)return 0;
    const int px=static_cast<int>(base%220),py=static_cast<int>(base/220);unsigned w=0;
    for(int y=(std::max)(0,py-kHelmScan);y<=(std::min)(219,py+kHelmScan);++y)
        for(int x=(std::max)(0,px-kHelmScan);x<=(std::min)(219,px+kHelmScan);++x) {
            const unsigned h=static_cast<unsigned>(y*220+x);Obj t=table(0xE270,h,48399);
            if(valid(t) && word(t,0x12)==n && helm_water(h))++w;
        }
    water[n]=static_cast<unsigned short>((std::min)(w,65535u));
    return water[n];
}
// A region the 조타 ship watches for castle `city_id`: the castle's region, its base still held by the force, and either a port
// (base terrain kRetakePortTerrain) or one with river water.
static bool helm_watch_area(unsigned own,unsigned city_id,unsigned n) {
    Obj area=helm_area_object(n);if(!area || area[0x35]!=city_id)return false;
    const unsigned base=word(area,0x3A);Obj tile=base<48400?table(0xE270,base,48399):nullptr;
    if(!valid(tile) || tile[0x14]!=own)return false;
    return tile[0x10]==kRetakePortTerrain || helm_area_water(n)>=kHelmRiverHexes;
}
static bool helm_area_taken(unsigned own,unsigned n,Obj self) {
    for(unsigned i=1;i<=500;++i) {
        Obj a=table(0x7E838,i,500);if(!valid(a) || a==self || army_force(a)!=own)continue;
        const Progress &q=progress[i];
        if(q.army==a && q.root==world() && q.commander==word(a,0x12) && q.helm && q.helm_area==n && !q.returning)return true;
    }
    return false;
}
// When the ship's region is whole again: the castle's other watched region with the most hexes left to paint, not taken by
// another ship (0 = none, go home).
// skip: regions left in this 순 for having no water way (local273), passed by too.
static bool helm_area_listed(const unsigned short *list,unsigned n,unsigned area) {for(unsigned i=0;i<n;++i)if(list[i]==area)return true;return false;}
static unsigned helm_next_area(unsigned own,unsigned cur,unsigned here,Obj self,const unsigned short *skip=nullptr,unsigned nskip=0) {
    Obj area=helm_area_object(cur);const unsigned city_id=area?area[0x35]:0;if(!city_id)return 0;
    unsigned best=0,most=0;
    for(unsigned n=1;n<=500;++n) {
        if(n==cur || helm_area_listed(skip,nskip,n) || !helm_watch_area(own,city_id,n) || helm_area_taken(own,n,self))continue;
        unsigned left=0;helm_pick(own,n,here,self,nullptr,&left,kHelmLeg);
        if(left>most) {most=left;best=n;}
    }
    return best;
}
// local169 (user 2026-11-15 "조타 장수들의 반응 범위를 현행 조건에 추가로 성주변의 장강 타일 ... 대하 격류 타일 ... 15칸으로하자 ...
// 장강에 충분히 멀어도 반응했으면"): the river ring of a 조타 castle = the 대하 (14) and 격류 (24) hexes within
// max(15, nearest such hex + 10) hexes of the castle (무릉 lies far from the 장강), listed once per world.
static constexpr int kHelmRingMin=15,kHelmRingExtra=10,kHelmStuckDays=3,kHelmRingFoeNear=6;
struct HelmRing {Obj root=nullptr;unsigned castle=48400;int radius=0;std::vector<unsigned short> cells;};
static HelmRing helm_rings[52];
static const HelmRing *helm_ring_of(unsigned city_id) {
    if(!city_id || city_id>=52)return nullptr;
    HelmRing &r=helm_rings[city_id];
    if(r.root==world())return r.castle<48400?&r:nullptr;
    r=HelmRing{};r.root=world();
    Obj city=campaign_goal(city_id);
    const unsigned castle=city?fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(city):48400;
    if(castle>=48400) {r.root=nullptr;return nullptr;}
    auto river=[](unsigned h){Obj t=table(0xE270,h,48399);return valid(t) && (t[0x10]==14 || t[0x10]==24);};
    int nearest=99;
    for(unsigned h=0;h<48400;++h)if(river(h))nearest=(std::min)(nearest,distance(castle,h));
    r.castle=castle;r.radius=(std::max)(kHelmRingMin,nearest+kHelmRingExtra);
    for(unsigned h=0;h<48400;++h)if(river(h) && distance(castle,h)<=r.radius)r.cells.push_back(static_cast<unsigned short>(h));
    log_event("helm_ring_built",city_id,static_cast<unsigned>(r.radius)|(static_cast<unsigned>(r.cells.size())<<8),castle);
    return &r;
}
// Ring hexes held by the force (*held) and lost to a hostile force (return); *area = the region with the most lost ones.
static unsigned helm_ring_lost(unsigned own,unsigned city_id,unsigned *held,unsigned *area) {
    if(held)*held=0;if(area)*area=0;
    const HelmRing *r=helm_ring_of(city_id);if(!r)return 0;
    unsigned lost=0;unsigned short by[501]={};
    for(unsigned short h:r->cells) {
        Obj t=table(0xE270,h,48399);if(!valid(t))continue;
        const unsigned holder=t[0x14];
        if(holder==own) {if(held)++*held;continue;}
        if(!holder || fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,holder)==0)continue;
        ++lost;const unsigned a=word(t,0x12);if(a<=500)++by[a];
    }
    if(area) {unsigned most=0;for(unsigned a=1;a<=500;++a)if(by[a]>most) {most=by[a];*area=a;}}
    return lost;
}
static unsigned helm_ring_castle(unsigned city_id) {const HelmRing *r=helm_ring_of(city_id);return r?r->castle:48400;}
// local171 (user 2026-11-15 "목표 이동거리가 짧으면 그 목표 주면 아군 타일을 찍어서라도 더 진행했으면"): when the relay chain adds
// up to fewer than kHelmPadTotal hexes, more relays are added from its end: a free water hex 3..kHelmLeg hexes on, also one
// already ours, outside 투석대/망루 ranges and 3 hexes of hostile armies, away from the earlier points; scored by the unheld
// hexes of its footprint and by the distance from the ship.
// local273 (user 2026-11-20 "(2)번도 적용해"; local272 log: pads on the 소호 banks, terrain 4, walked the ships ashore and back):
// water hexes only (before: a bank not held by a hostile force too). anchor < 48400: only hexes within reach of it (the patrol).
static constexpr int kHelmPadTotal=15;
static unsigned helm_pad_pick(unsigned own,unsigned from,unsigned here,Obj self,const unsigned *avoid,unsigned navoid,const HelmTower *tw,unsigned ntw,
                              unsigned anchor=48400,int reach=0) {
    if(from>=48400)return 48400;
    const int cx=static_cast<int>(from%220),cy=static_cast<int>(from/220);
    unsigned best=48400;int top=-(1<<30);
    for(int y=(std::max)(0,cy-kHelmLeg);y<=(std::min)(219,cy+kHelmLeg);++y)
        for(int x=(std::max)(0,cx-kHelmLeg);x<=(std::min)(219,cx+kHelmLeg);++x) {
            const unsigned h=static_cast<unsigned>(y*220+x);const int d=distance(from,h);
            if(d<3 || d>kHelmLeg || !helm_water(h) || helm_tower_hit(tw,ntw,h) || !intercept_open(h,self))continue;
            if(anchor<48400 && distance(anchor,h)>reach)continue;
            bool close_to=false;for(unsigned i=0;i<navoid && !close_to;++i)close_to=distance(h,avoid[i])<=2;
            if(close_to)continue;
            int gain=0;
            auto value=[&](unsigned q) {Obj t=q<48400?table(0xE270,q,48399):nullptr;if(valid(t) && t[0x14]!=own)gain+=helm_hostile_tile(own,q)?3:2;};
            value(h);for(unsigned k=0;k<6;++k)value(sortie::neighbor(h,k));
            const int score=10*gain+3*distance(here,h)+d;
            if(score<=top)continue;
            if(retake_hostile_near(own,h,3))continue;
            top=score;best=h;
        }
    return best;
}
// local273 (user 2026-11-20 "지연퇴각에서 뒤집을 타일이 없으면 아군 물타일을 순찰하면 되잖아"; local272 log: about 23 ship-turns of the
// delay stood still with no hostile water tile in reach): the delayed ship patrols instead of waiting - up to kHelmRelays + 1
// water hexes (ours too) picked one after another as the pads are (helm_pad_pick), within kHelmPatrolReach hexes of its port
// (the ring's castle; anywhere when none is in reach of the ship), sailed to the end within the 순 they were laid in. A hostile
// tile coming in reach still comes first (helm_delay_step picks it before this).
static constexpr int kHelmPatrolReach=12;
static bool helm_delay_patrol(Obj army,Progress &p,unsigned own,unsigned here,int day) { COST_SCOPE("campaign:helm_delay_patrol");
    if(here>=48400 || helm_in_castle(army,here))return false;
    const unsigned target=p.region_target;
    const bool ordered=target<48400 && target!=here && army[0x37]==1 && army[0x38]==9 && word(army,0x3A)==target && word(army,0x48)<48400;
    if(ordered && p.helm_routed && (p.retake_day==day || (p.retake_day/10==day/10 && helm_sailing_ok(army,p,target))))return true;
    Obj area=helm_area_object(p.helm_area);
    const unsigned anchor=p.helm_ring?helm_ring_castle(p.helm_ring):area?word(area,0x3A):48400;
    HelmTower tw[64];const unsigned ntw=helm_towers(own,here,kHelmLeg*(kHelmRelays+1),tw,64);
    unsigned chain[kHelmRelays+1],n=0;int total=0;bool anchored=false;
    for(int pass=0;pass<2 && !n;++pass) {   // pass 1: no anchor (the ship is out of the port's reach)
        unsigned avoid[kHelmRelays+2],navoid=0;avoid[navoid++]=here;
        unsigned from=here;total=0;anchored=!pass && anchor<48400;
        while(n<kHelmRelays+1) {
            const unsigned next=helm_pad_pick(own,from,here,army,avoid,navoid,tw,ntw,anchored?anchor:48400u,kHelmPatrolReach);
            if(next>=48400)break;
            total+=distance(from,next);chain[n++]=next;avoid[navoid++]=next;from=next;
        }
        if(anchor>=48400)break;
    }
    if(!n || !helm_route(army,p,chain,n))return false;
    // value = points | hexes through them << 4 | near the port << 16; hex = the patrol goal
    log_event("helm_patrol",army_id(army),n|(static_cast<unsigned>((std::min)(total,4095))<<4)|(anchored?1u<<16:0u),p.region_target);
    p.helm_stuck=0;
    return true;
}
// local186 (user 2026-11-15 "원래 출진 성 항구 5칸이내에 적부대가 보이면 시상 지원을 끄고 퇴각하는게 아니라 해당 성의 항구 적부대로
// 목표 바꾸고 ... 처음부터 건업 방어 임무 받은것처럼 이어갈수 있나? 중간에 로직이 안끊기고 전환"): a ship working for another
// castle (the 시상 support of helm_neighbour: its region's city is not the castle it sailed from) takes on its own castle's
// port when a hostile army comes within kHelmHomeAlert hexes of that port's base - the port nearest the ship, unless
// another ship of the force already works it. Only the region changes (ring cleared, goal re-picked the same day), as a
// ship sent out for that port: the port retake, region switches, ring join and the way home all go on from there.
static constexpr int kHelmHomeAlert=5;
static bool helm_home_call(Obj army,Progress &p,unsigned own,unsigned here) {
    Obj origin=fn<Obj(__fastcall *)(Obj)>(GA(GA_22BFE0))(army);
    if(!valid(origin) || intercept_base_owner(origin)!=own)return false;
    const unsigned home=word(origin,0x4E);if(!home || home>=52)return false;
    Obj area=helm_area_object(p.helm_area);
    const unsigned working=p.helm_ring?p.helm_ring:(area?area[0x35]:0);
    if(working==home)return false;   // already on its own castle's work
    unsigned best=0,foe_at=48400;int closest=1<<30;
    for(unsigned n=1;n<=500;++n) {
        Obj a=table(0x6CAF0,n,500);if(!valid(a) || a[0x35]!=home)continue;
        const unsigned base=word(a,0x3A);Obj t=base<48400?table(0xE270,base,48399):nullptr;
        if(!valid(t) || t[0x10]!=kRetakePortTerrain)continue;
        unsigned foe[1];if(!intercept_attackers(own,base,kHelmHomeAlert,foe,1) || helm_area_taken(own,n,army))continue;
        const int d=here<48400?distance(here,base):0;
        if(d<closest) {closest=d;best=n;foe_at=foe[0];}
    }
    if(!best)return false;
    // army, old region | new region << 16, hostile hex | home castle << 16
    log_event("helm_home_switch",army_id(army),p.helm_area|(best<<16),foe_at|(home<<16));
    p.helm_ring=0;p.helm_area=static_cast<unsigned short>(best);p.region_target=48400;p.helm_pick_day=-1;p.helm_stuck=0;
    return true;
}
static bool helm_step(Obj army,Progress &p);
// local170: after a region switch the ship picks and sails the same day (before, the day of the switch was spent waiting).
static bool helm_switch_again(Obj army,Progress &p) {static int depth;if(depth>=2)return true;++depth;const bool r=helm_step(army,p);--depth;return r;}
static bool helm_step(Obj army,Progress &p) { COST_SCOPE("campaign:helm_step");
    const unsigned own=army_force(army),here=word(army,0x2A);const int day=game_day();
#ifndef NO_RUNTIME_LOG
    if(p.helm_seen_day!=day) {   // local269 (user 2026-11-19 "여전히 한턴에 짧게 움직여 (주창 추적해봐)"): one line per ship and turn (orders
        // alone did not show where a ship was; local271: the first of the day's two runs only). army = id | commander << 16; value
        // = hex | hexes moved since the last line << 16 (255 first) | on water << 24 | delay << 25 | returning << 26 | helm_route
        // order << 27; hex = next hex | action << 16 | target kind << 24.
        // helm_turn2: value = order target | mission target << 16, hex = soldiers | morale << 16 | days since the last line << 24.
        // helm_turn3 (local271, a stop with a move order had no cause in the log): value = 혼란 +0x23 | 정지 +0x24 << 8 | 도발 +0x25
        // << 16 | hostile armies next to the ship << 24 | next hex taken by an army << 28; hex = land formation +0x1B | water
        // formation +0x1C << 8.
        unsigned foes=0,taken=0;const unsigned next_hex=word(army,0x48);
        for(unsigned i=1;i<=500 && here<48400;++i) {
            Obj a=table(0x7E838,i,500);if(!valid(a) || a==army)continue;
            const unsigned at=word(a,0x2A);if(at>=48400)continue;
            if(at==next_hex)taken=1;
            const unsigned force=army_force(a);
            if(force && force!=own && distance(at,here)==1 && fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,force))++foes;
        }
        log_event("helm_turn3",army_id(army),static_cast<unsigned>(army[0x23])|(static_cast<unsigned>(army[0x24])<<8)|
                  (static_cast<unsigned>(army[0x25])<<16)|((std::min)(foes,15u)<<24)|(taken<<28),
                  static_cast<unsigned>(army[0x1B])|(static_cast<unsigned>(army[0x1C])<<8));
        unsigned moved=255;
        if(p.helm_seen_day>0 && p.helm_seen_hex<48400 && here<48400)moved=static_cast<unsigned>((std::min)(distance(p.helm_seen_hex,here),254));
        const unsigned days=p.helm_seen_day>0?static_cast<unsigned>((std::min)(day-p.helm_seen_day,255)):255u;
        log_event("helm_turn",army_id(army)|(static_cast<unsigned>(word(army,0x12))<<16),
                  (here&0xFFFFu)|(moved<<16)|(helm_water(here)?1u<<24:0u)|(p.helm_delay?1u<<25:0u)|(p.returning?1u<<26:0u)|(p.helm_routed?1u<<27:0u),
                  word(army,0x48)|(static_cast<unsigned>(army[0x37])<<16)|(static_cast<unsigned>(army[0x38])<<24));
        log_event("helm_turn2",army_id(army),word(army,0x3A)|(static_cast<unsigned>(p.region_target)<<16),
                  word(army,0x16)|(static_cast<unsigned>(army[0x1A])<<16)|(days<<24));
        // helm_turn4 (local273; "moved" above is the straight distance between two lines, a loop reads short): value = hexes
        // ahead through the order's relays and goal (0 without a sailing order) | points << 12; hex = straight distance to the goal.
        {
            unsigned ahead=0,np=0,goal_gap=0;const unsigned goal=word(army,0x3A);
            if(army[0x37]==1 && army[0x38]==9 && here<48400) {
                unsigned pts[kHelmRelays+1];np=helm_points_ahead(army,goal,pts);
                unsigned c=here;for(unsigned i=0;i<np;++i){ahead+=static_cast<unsigned>(distance(c,pts[i]));c=pts[i];}
                if(goal<48400)goal_gap=static_cast<unsigned>(distance(here,goal));
            }
            log_event("helm_turn4",army_id(army),(std::min)(ahead,4095u)|(np<<12),goal_gap);
        }
        p.helm_seen_hex=static_cast<unsigned short>(here<48400?here:48400);p.helm_seen_day=day;
    }
#endif
    if(p.returning) {recovery_return(army,p);return true;}
    if(p.helm_delay)return helm_delay_step(army,p,day);
    if(helm_home_call(army,p,own,here))return helm_switch_again(army,p);   // local186
    Obj area=helm_area_object(p.helm_area);
    const unsigned port=area?word(area,0x3A):48400;
    Obj port_tile=port<48400?table(0xE270,port,48399):nullptr;
    const char *why=nullptr;
    p.motion_day=day;   // sailing to the port region is the mission, not a stall
    if(word(army,0x16)<kHelmMinimum)why="helm_worn_return";
    else if(!p.helm_ring && (!valid(port_tile) || port_tile[0x14]!=own)) {   // local169: a river-ring ship has no port to retake
        // local150 (user 2026-11-14 "항구를 잃었을 때 -> 바로 퇴각하지 말고 성하고 가장 가까운 (거리조건)으로 수복해야지"): the ship
        // retakes the castle's lost port nearest to the castle (a port region of the same city, area +0x35, held by a hostile
        // force) by standing on its base, then paints that port region; home only when the castle has no lost port left.
        const unsigned city_id=area?area[0x35]:0;Obj city=city_id?campaign_goal(city_id):nullptr;
        const unsigned castle=city && intercept_base_owner(city)==own?fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(city):48400;
        unsigned lost_area=0,lost_base=48400;int closest=1<<30;
        if(castle<48400)for(unsigned n=1;n<=500;++n) {
            Obj a=table(0x6CAF0,n,500);if(!valid(a) || a[0x35]!=city_id)continue;
            const unsigned base=word(a,0x3A);Obj t=base<48400?table(0xE270,base,48399):nullptr;
            if(!valid(t) || t[0x10]!=kRetakePortTerrain)continue;
            const unsigned holder=t[0x14];
            if(!holder || holder==own || fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,holder)==0)continue;
            const int d=distance(castle,base);
            if(d<closest) {closest=d;lost_area=n;lost_base=base;}
        }
        if(!lost_area)why="helm_port_lost";
        else {
            if(p.helm_area!=lost_area) {log_event("helm_port_retake",army_id(army),p.helm_area|(lost_area<<16),lost_base);p.helm_area=static_cast<unsigned short>(lost_area);}
            p.region_target=static_cast<unsigned short>(lost_base);
            if(here!=lost_base && helm_route(army,p,&lost_base,1))return true;   // local269 (was retake_route_order)
            if(army[0x37]!=13) {   // on the base or no way there today: wait and fight in range
                fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
                fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,13);
                fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,255,0);
            }
            return true;
        }
    }
    unsigned target=p.region_target;
    bool route_failed=false;
    // local271 (user 2026-11-20 "주창 내가 봤는데 3칸인가 4칸 움직인 날이 있어.. 조타는 매순마다 끊임없이 움직여야하는데.."; local270
    // log: a ship stood the rest of a 순 at its goal, two 순 on the river when its pick had no way, and one walked the 건업 island
    // for three 순): the relay chain after its first points, for a new pick and for the top-up of a kept goal.
    auto grow=[&](unsigned *chain,unsigned n,unsigned *avoid,unsigned navoid,unsigned origin)->unsigned {
        while(n<kHelmRelays+1) {   // local273: relays on water only (local272: 소호 bank relays walked the ships ashore)
            const unsigned next=helm_pick(own,p.helm_area,chain[n-1],army,nullptr,nullptr,kHelmLeg,avoid,navoid,false,0,true);
            if(next>=48400)break;
            chain[n++]=next;avoid[navoid++]=next;
        }
        // local170 (user 2026-11-15 "한턴 이동량을 다 못쓰는 경우 ... 다음 미점령지역을 찍어서 추가로 이동"): a chain cut short by its
        // region goes on through the castle's other watched regions (and the ring's most lost one) not taken by another ship;
        // each relay is the best of their picks.
        if(n<kHelmRelays+1) {
            unsigned ext[4],ne=0;
            const unsigned city_id=p.helm_ring?p.helm_ring:area?area[0x35]:0;
            if(p.helm_ring) {unsigned ra=0;if(helm_ring_lost(own,p.helm_ring,nullptr,&ra) && ra && ra!=p.helm_area && !helm_area_taken(own,ra,army))ext[ne++]=ra;}
            for(unsigned a=1;a<=500 && ne<4 && city_id;++a)
                if(a!=p.helm_area && (ne==0 || ext[0]!=a) && helm_watch_area(own,city_id,a) && !helm_area_taken(own,a,army))ext[ne++]=a;
            const unsigned before=n;
            while(ne && n<kHelmRelays+1) {
                unsigned next=48400;int top=-(1<<30);
                for(unsigned i=0;i<ne;++i) {
                    int sc=0;const unsigned h=helm_pick(own,ext[i],chain[n-1],army,&sc,nullptr,kHelmLeg,avoid,navoid,false,0,true);
                    if(h<48400 && sc>top) {top=sc;next=h;}
                }
                if(next>=48400)break;
                chain[n++]=next;avoid[navoid++]=next;
            }
            if(n>before)log_event("helm_chain_extend",army_id(army),before|(n<<8),chain[n-1]);
        }
        int total=0;for(unsigned i=0;i<n;++i)total+=distance(i?chain[i-1]:origin,chain[i]);
        if(total<kHelmPadTotal && n<kHelmRelays+1) {   // local171: a short chain goes on over our own water
            HelmTower tw[64];const unsigned ntw=helm_towers(own,chain[n-1],kHelmLeg*3,tw,64);
            const unsigned before=n;
            while(n<kHelmRelays+1 && total<kHelmPadTotal) {
                const unsigned next=helm_pad_pick(own,chain[n-1],origin,army,avoid,navoid,tw,ntw);
                if(next>=48400)break;
                total+=distance(chain[n-1],next);chain[n++]=next;avoid[navoid++]=next;
            }
            if(n>before)log_event("helm_chain_pad",army_id(army),before|(n<<8)|(static_cast<unsigned>(total)<<16),chain[n-1]);
        }
        return n;
    };
    if(!why && p.helm_pick_day!=day) {
        p.helm_pick_day=day;
        // local271: a ship on land out of its castle (the departure walk, a bank goal, the 건업 island) sails from the nearest water
        // hex: that hex is the first point of its order and the picks are made from it (local270: island banks were picked from
        // the island and reached over it).
        unsigned launch=48400,launch_steps=0;
        if(here<48400 && !helm_water(here) && !helm_in_castle(army,here)) {
            launch=helm_launch(own,here,&launch_steps);
            // value = launch hex | hexes of the walk there << 16, hex = the ship
            if(launch<48400)log_event("helm_launch",army_id(army),launch|((std::min)(launch_steps,65535u)<<16),here);
        }
        const unsigned from=launch<48400?launch:here;
        // local263 (user 2026-11-19 "20칸 찍는건 이렇게바꾸자 첫 중계점을 최소 8칸 떨어진곳에 찍기"): the first point is picked kHelmFirstMin..
        // kHelmSweep hexes on (beyond: the far fallback as before); a region with no such hex falls back to the one-leg pick.
        int score=0;unsigned left=0;unsigned best=helm_pick(own,p.helm_area,from,army,&score,&left,kHelmSweep,nullptr,0,true,kHelmFirstMin);
        if(best>=48400)best=helm_pick(own,p.helm_area,from,army,&score,&left,kHelmLeg);
        Obj t=target<48400?table(0xE270,target,48399):nullptr;
        // local170: a goal is kept while the relay order still sails to it, also one in the next region (chain extension);
        // never hostile land or a hex in a 투석대/망루 range.
        const bool sailing=army[0x37]==1 && army[0x38]==9 && word(army,0x3A)==target;
        HelmTower towers[64];const unsigned nt=valid(t)?helm_towers(own,target,4,towers,64):0;
        // local172 (user 2026-11-15 "추가 이동력을 낭비하지 않기위한 아군타일 목표니까 다음순(기동력회복)에서는 또 적 타일을 우선"):
        // a goal on our own tile (a padded chain) is kept only within the 순 (10 days) it was ordered in; in the next one the
        // ship picks unheld tiles first again.
        const bool same_period=p.retake_day/10==day/10;
        const bool keep=launch>=48400 && valid(t) && target!=here && (word(t,0x12)==p.helm_area || sailing) && (t[0x14]!=own || (sailing && same_period)) && intercept_open(target,army) &&
                        helm_goal_ok(own,target) && !helm_tower_hit(towers,nt,target) &&
                        !retake_hostile_near(own,target,3);   // local161: a kept target is dropped once attackers come near it
        if(p.helm_ring && !left) {   // local169: a river-ring ship works until no ring hex is lost, region by region
            unsigned ring_area=0;const unsigned lost=helm_ring_lost(own,p.helm_ring,nullptr,&ring_area);
            if(lost && ring_area && ring_area!=p.helm_area) {log_event("helm_area_switch",army_id(army),p.helm_area|(ring_area<<16),here);p.helm_area=static_cast<unsigned short>(ring_area);p.region_target=48400;p.helm_pick_day=-1;p.helm_stuck=0;return helm_switch_again(army,p);}
            if(!lost)why="helm_done_return";
            else why="helm_ring_unreachable";
        }
        else if(!left) {   // every hex of the port region is ours again: another watched region of the castle, else home
            const unsigned next=helm_next_area(own,p.helm_area,here,army);
            if(next) {log_event("helm_area_switch",army_id(army),p.helm_area|(next<<16),here);p.helm_area=static_cast<unsigned short>(next);p.region_target=48400;p.helm_pick_day=-1;return helm_switch_again(army,p);}
            // local170 (user 2026-11-15 "장강 임무를 계속 부여받았으면"): the port ship takes on the castle's river ring when a
            // ring hex is lost, instead of going home.
            const unsigned city_id=area?area[0x35]:0;unsigned ring_area=0;
            if(city_id && city_id<52 && helm_ring_lost(own,city_id,nullptr,&ring_area) && ring_area && ring_area!=p.helm_area && !helm_area_taken(own,ring_area,army)) {
                log_event("helm_ring_join",army_id(army),p.helm_area|(ring_area<<16),city_id);
                p.helm_ring=static_cast<unsigned char>(city_id);p.helm_area=static_cast<unsigned short>(ring_area);
                p.region_target=48400;p.helm_pick_day=-1;p.helm_stuck=0;return helm_switch_again(army,p);
            }
            why="helm_done_return";
        }
        else if(best>=48400) {   // only hexes next to the attackers left: wait (local169: home after kHelmStuckDays days)
            target=48400;
            if(p.helm_ring && ++p.helm_stuck>=kHelmStuckDays)why="helm_stuck_return";
        }
        else if(!keep) {   // local161: a target is sailed to the end (re-picked only when reached, ours already or blocked)
            // local271: a pick with no way (helm_route_none, blocked, refused) is followed the same day by the next best pick
            // (kHelmPicks in all, none within 3 hexes of a failed one), then by the launch hex alone; the hold below only when
            // none sails (local270 log: 15 picks with no way, 5 holds, one ship two 순 in place on the river).
            unsigned tried[kHelmPicks],ntried=0,last_failed=0;bool no_way=true;   // local273: every failed pick had no water way
            for(;;) {
                log_event("helm_target",army_id(army),static_cast<unsigned>(score)|((std::min)(left,65535u)<<16),best);
                target=best;p.region_target=static_cast<unsigned short>(best);
                // local161: the relay chain from best on (five relays and a goal when the region has room for them).
                unsigned chain[kHelmRelays+1],n=0,avoid[kHelmRelays+4],navoid=0;
                avoid[navoid++]=here;
                if(launch<48400) {avoid[navoid++]=launch;if(best!=launch)chain[n++]=launch;}
                chain[n++]=best;avoid[navoid++]=best;
                if(distance(from,best)<=kHelmSweep)n=grow(chain,n,avoid,navoid,from);   // local263: relays after a first point up to kHelmSweep on
                if(helm_route(army,p,chain,n)) {p.helm_stuck=0;return true;}   // local269 (was helm_chain_order)
                const unsigned failed=helm_route_why;tried[ntried++]=best;last_failed=failed;
                no_way=no_way && (failed==kPathNone || failed==kPathTarget || failed==kPathRange);
                if(ntried>=kHelmPicks || failed==kPathStart || failed==kHelmWhyLocked)break;   // the ship itself is shut in
                unsigned next=helm_pick(own,p.helm_area,from,army,&score,nullptr,kHelmSweep,tried,ntried,false,kHelmFirstMin);
                if(next>=48400)next=helm_pick(own,p.helm_area,from,army,&score,nullptr,kHelmLeg,tried,ntried,false);
                if(next>=48400)break;
                log_event("helm_retry",army_id(army),best|(failed<<16),next);   // value = the pick with no way | why << 16, hex = next pick
                best=next;
            }
            if(launch<48400 && helm_route(army,p,&launch,1)) {p.helm_stuck=0;return true;}
            route_failed=true;
            // local273 (user 2026-11-20 "(3) 도 적용해"; local272 log: picks in the 소호 lake, a water body the ship's river does
            // not reach, ended in helm_route_none and held 서성·정봉·황개 there): when every pick of the region failed for want of a
            // water way (none, target closed, out of range; not a blocked or refused order, which can pass) it takes the castle's
            // other watched region with the most hexes left, else the castle's river ring. The regions left this way are kept for
            // the 순 (up to four) and passed by, so the 소호 regions (80/88/90) are tried once each, not in turn every day; the
            // same-day re-run (helm_switch_again) tries the next one at once.
            if(!p.helm_ring && no_way && ntried) {
                if(p.helm_noway_period!=day/10) {memset(p.helm_noway,0,sizeof(p.helm_noway));p.helm_noway_period=day/10;}
                const unsigned from_area=p.helm_area;const unsigned cap=sizeof(p.helm_noway)/sizeof(*p.helm_noway);
                if(!helm_area_listed(p.helm_noway,cap,from_area)) {
                    unsigned slot=0;while(slot+1<cap && p.helm_noway[slot])++slot;
                    p.helm_noway[slot]=static_cast<unsigned short>(from_area);   // the last slot is reused when all four are taken
                }
                const unsigned next=helm_next_area(own,from_area,here,army,p.helm_noway,cap);
                // helm_noway_switch / helm_noway_ring: value = old region | new region << 16, hex = the ship | last reason << 16
                if(next) {
                    log_event("helm_noway_switch",army_id(army),from_area|(next<<16),here|(last_failed<<16));
                    p.helm_area=static_cast<unsigned short>(next);p.region_target=48400;p.helm_pick_day=-1;p.helm_stuck=0;return helm_switch_again(army,p);
                }
                const unsigned city_id=area?area[0x35]:0;unsigned ring_area=0;
                if(city_id && city_id<52 && helm_ring_lost(own,city_id,nullptr,&ring_area) && ring_area && ring_area!=from_area &&
                   !helm_area_listed(p.helm_noway,cap,ring_area) && !helm_area_taken(own,ring_area,army)) {
                    log_event("helm_noway_ring",army_id(army),from_area|(ring_area<<16),here|(last_failed<<16));
                    p.helm_ring=static_cast<unsigned char>(city_id);p.helm_area=static_cast<unsigned short>(ring_area);
                    p.region_target=48400;p.helm_pick_day=-1;p.helm_stuck=0;return helm_switch_again(army,p);
                }
            }
        }
        else if(sailing && p.helm_routed) {
            // local271: the kept order has under kHelmTopUp hexes left through its relays: it goes on past its goal (the relays
            // still ahead and the goal, then picks from the goal), so the ship does not stand there for the rest of the 순
            // (local270 log: 1 and 2 hexes, then waiting).
            unsigned chain[kHelmRelays+1];unsigned n=helm_points_ahead(army,target,chain);
            int ahead=0;for(unsigned i=0;i<n;++i)ahead+=distance(i?chain[i-1]:here,chain[i]);
            if(n==kHelmRelays+1) {chain[0]=target;n=1;}   // six points within kHelmTopUp hexes are a short-stride way: laid again from the goal
            if(n && ahead<=kHelmTopUp) {
                unsigned avoid[kHelmRelays+4],navoid=0;avoid[navoid++]=here;for(unsigned i=0;i<n;++i)avoid[navoid++]=chain[i];
                const unsigned before=n;n=grow(chain,n,avoid,navoid,here);
                // value = hexes left | points before << 8 | points after << 12, hex = the new goal
                if(n>before && helm_route(army,p,chain,n)) {
                    log_event("helm_topup",army_id(army),static_cast<unsigned>(ahead)|(before<<8)|(n<<12),p.region_target);
                    p.helm_stuck=0;return true;
                }
            }
        }
    }
    // local161: the relay order already sails to this goal: leave it (re-ordering would drop the relays).
    // local269: only while it came from helm_route and the next hex is water (or the goal); otherwise (the departure way on its
    // first turn out, a supply relay, 303EA0 found land during the turn) the way is laid again; with no own way the old order
    // is kept only when its next hex is water, else the ship holds.
    // local271: an order helm_route wrote today stands (helm_step runs twice a day; the second run laid it again to the goal
    // alone when its next hex was land, dropping the relays: local270 log, a 41-hex way became 13 hexes across the 건업 island),
    // and on a later 순 it is laid again through the relays still ahead, never the goal alone.
    if(!why && target<48400 && here!=target && army[0x37]==1 && army[0x38]==9 && word(army,0x3A)==target && word(army,0x48)<48400) {
        if(helm_sailing_ok(army,p,target) || (p.helm_routed && p.retake_day==day)) {p.helm_stuck=0;return true;}
        unsigned ahead[kHelmRelays+1];const unsigned na=helm_points_ahead(army,target,ahead);
        if(helm_route(army,p,ahead,na) || helm_water(word(army,0x48))) {p.helm_stuck=0;return true;}
        route_failed=true;
    }
    if(why && (!strcmp(why,"helm_stuck_return") || !strcmp(why,"helm_ring_unreachable"))) {   // local169: nothing reachable: home now
        log_event(why,army_id(army),word(army,0x16),p.helm_area);
        recovery_return(army,p);return true;
    }
    if(why) {   // local167: the reason is logged now, the ship goes home after the delay
        log_event(why,army_id(army),word(army,0x16),p.helm_area);
        p.helm_delay=true;p.helm_delay_until=day+kHelmDelayDays;p.region_target=48400;
        p.helm_delay_left=helm_delay_count(own,p,here,army);
        log_event("helm_delay_start",army_id(army),word(army,0x16),static_cast<unsigned>(p.helm_delay_until));
        return helm_delay_step(army,p,day);
    }
    if(!route_failed && target<48400 && here!=target && helm_route(army,p,&target,1)) {p.helm_stuck=0;return true;}   // local269 (was retake_route_order)
    if(target<48400 && here!=target) {   // local161: no way there today: pick again tomorrow (local169: counts as a stuck day)
        p.region_target=48400;
        if(p.helm_ring && p.helm_pick_day==day && ++p.helm_stuck>=kHelmStuckDays) {log_event("helm_stuck_return",army_id(army),word(army,0x16),p.helm_area);recovery_return(army,p);return true;}
    }
    if(army[0x37]!=13) {   // nothing safe to paint now: wait; the original combat fights in range
        fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
        fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,13);
        fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,255,0);
        log_event("helm_hold",army_id(army),here,p.helm_area);
    }
    return true;
}
static bool intercept_step(Obj army,Progress &p) { COST_SCOPE("campaign:intercept_step");
    if(p.helm)return helm_step(army,p);
    const unsigned own=army_force(army),here=word(army,0x2A);const int day=game_day();
    if(p.returning) {recovery_return(army,p);return true;}
    Obj gate=intercept_gate_object(p.intercept_gate);
    const unsigned center=gate?fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(gate):48400;
    const char *why=nullptr;
    if(!gate || center>=48400 || intercept_base_owner(gate)!=own)why="intercept_gate_lost";
    else if(word(army,0x16)<kInterceptMinimum)why="intercept_worn_return";
    else {
        unsigned seen[1];
        if(intercept_attackers(own,center,kInterceptAlert+1,seen,1))p.intercept_seen_day=day;
        else if(day-p.intercept_seen_day>=kInterceptQuiet)why="intercept_done_return";
    }
    if(why) {log_event(why,army_id(army),word(army,0x16),p.intercept_gate);recovery_return(army,p);return true;}
    // The stand follows the attackers; a stand that is still behind the gate and free is kept.
    unsigned target=p.region_target;
    // local179: siblings of the same gate keep their own stands; the best one nobody else holds.
    bool best_attack=false;
    const unsigned best=intercept_free_stand(own,center,p.intercept_gate,army,&best_attack);
    if(best<48400 && best!=target) {
        unsigned attackers[64];const unsigned n=intercept_attackers(own,center,kInterceptAlert+1,attackers,64);
        unsigned claims[16];const unsigned c=intercept_claims(own,p.intercept_gate,army,claims,16);
        bool claimed=false;for(unsigned j=0;j<c && !claimed;++j)claimed=claims[j]==target;
        unsigned stands[64],front=0;const unsigned listed=intercept_stands(own,center,army,stands,64,&front);
        bool target_attack=false;for(unsigned k=0;k<front && k<listed && !target_attack;++k)target_attack=stands[k]==target;
        const unsigned both[2]={target,best};   // kept while it is as good a stand as the new one
        // An attack stand (across the gate, in 안행 reach) is kept; otherwise kept only against a stand that is no attack stand.
        const bool keep=target<48400 && !claimed && intercept_open(target,army) &&
            (target_attack || (!best_attack && intercept::pick(center,attackers,n,both,2)==target));
        if(!keep) {log_event("intercept_stand",army_id(army),target,best);target=best;p.region_target=static_cast<unsigned short>(best);}
    }
    p.motion_day=day;   // standing behind the gate is the mission, not a stall
    if(target<48400 && here!=target && retake_route_order(army,p,target))return true;
    if(army[0x37]!=13) {   // at the stand (or no way to it today): wait there; the original combat fights in range
        fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
        fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,13);
        fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,255,0);
        log_event("intercept_hold",army_id(army),here,p.intercept_gate);
    }
    return true;
}
// wall_age_siege (user 2026-11-21, docs/NEUTRAL_WALL_AGE.md 5절): the siege-weapon unit. It first marches on its departure way
// (move order with the relay points around hostile armies); within kWallSiegeNear hexes of its wall, or once that move order is
// over, it takes the wall order - the route_hook breach entry (assign_siege, put back by daily/after like every breach). While
// the wall stands the entry holds; when it fell or the entry ended, the same wall again if it still stands, else the next target
// within kWallSiegeRetarget hexes (round robin); none, the switch off, under 1,000 soldiers or a retreat = home by the intercept
// units' return (recovery_return). On the way home the unit no longer counts towards the ten.
static constexpr int kWallSiegeRetarget=30,kWallSiegeNear=6;
static bool wall_siege_step(Obj army,Progress &p) { COST_SCOPE("campaign:wall_siege_step");
    if(p.returning) {recovery_return(army,p);return true;}
    unsigned why=0;
    if(!mod_features.wall_age_siege)why=1;
    else if(word(army,0x16)<1000)why=2;
    else if(army[0x37]==11 || army[0x37]==12)why=3;
    // a hit-response breach or a morale stop (route_hook hold) first, a status (+0x140) waits: no retarget or way home for that
    if(!why && !route_hook::siege_holding(army) && (army[0x140] || (route_hook_owns && route_hook_owns(army)))) {p.motion_day=game_day();return true;}
    if(!why && !route_hook::siege_holding(army)) {
        const unsigned wall_hex=p.siege_wall?route_hook::siege_wall_hex(p.siege_wall):48400u;
        if(wall_hex<48400 && army[0x37]==1 && distance(word(army,0x2A),wall_hex)>kWallSiegeNear) {p.motion_day=game_day();return true;}   // on the way
        unsigned next=wall_hex<48400 && route_hook::assign_siege(army,p.siege_wall,false)?p.siege_wall:0u;
        if(!next) {
            next=route_hook::siege_retarget(army,kWallSiegeRetarget,p.siege_wall);
            // value = army, hex = old wall | new wall << 16, third = army hex
            if(next)log_event("wall_siege_retarget",army_id(army),p.siege_wall|(next<<16),word(army,0x2A));
        }
        if(next)p.siege_wall=static_cast<unsigned short>(next);else why=4;
    }
    if(why) {
        // value = army, hex = last wall, third = reason (1 switch off, 2 under 1,000, 3 retreat, 4 no wall left) | soldiers << 8
        log_event("wall_siege_home",army_id(army),p.siege_wall,why|(static_cast<unsigned>(word(army,0x16))<<8));
        p.siege_wall=0;
        if(!recovery_return(army,p) && !p.returning) {p.forced_return=true;recovery_return(army,p);}
        return true;
    }
    p.motion_day=game_day();   // breaking the wall is the mission, not a stall
    return true;
}
static bool campaign_regional_step(Obj army,Progress &p) {
    COST_SCOPE("campaign_regional_step");
    if(!p.regional_worker)return false;
    if(p.intercept)return intercept_step(army,p);
    if(p.siege_unit)return wall_siege_step(army,p);
    if(p.credit || p.build_chain || p.facility)return true;
    if(p.retake)return retake_step(army,p);
    for(unsigned h:campaign_home_bases(army,p.work_source))if(campaign_assign_safe(army,p,h,true))return true;
    if(p.region_target<48400 && campaign_region_hostile(army,campaign_area_id(p.region_target)) &&
        campaign_assign_safe(army,p,p.region_target,true))return true;
    // Expand one edge at a time after securing the home city's direct neighbors.
    Obj current=army_area(army);std::vector<unsigned> next;
    if(valid(current))for(unsigned i=0;i<8;++i) {
        unsigned n=word(current,0x56+i*2);if(!(current[0x66]&(1u<<i)) || !n || n>500 || campaign_transit_region(n) || !campaign_region_hostile(army,n))continue;
        unsigned h=word(table(0x6CAF0,n,500),0x3A);if(h<48400 && !region_reserved(h,army))next.push_back(h);
    }
    unsigned here=word(army,0x2A);std::sort(next.begin(),next.end(),[here](unsigned a,unsigned b){return distance(here,a)<distance(here,b);});
    for(unsigned h:next)if(campaign_assign_safe(army,p,h,true))return true;
    log_event("regional_no_target_return",army_id(army),word(army,0x16),word(army,0x2A));
    return recovery_return(army,p); // No castle fallback for capture/retreat formations.
}
static std::vector<unsigned> campaign_enemy_bases(Obj army,unsigned city);
static unsigned next_region(Obj army, Progress &p, unsigned city_hex,bool sequential=false) { COST_SCOPE("campaign:next_region");
    if(!campaign_group_member(army))for(unsigned h:campaign_home_bases(army))
        if(campaign_assign_safe(army,p,h,true,sequential))return 1;
    if(p.region_target<48400 && campaign_region_hostile(army,campaign_area_id(p.region_target)) &&
       campaign_assign_safe(army,p,p.region_target,true,sequential))return 1;
    auto enemy_bases=campaign_enemy_bases(army,p.city);
    unsigned here=word(army,0x2A);
    std::sort(enemy_bases.begin(),enemy_bases.end(),[here](unsigned a,unsigned b){return distance(here,a)<distance(here,b);});
    for(unsigned h:enemy_bases)if(!region_reserved(h,army) && campaign_assign_safe(army,p,h,true,sequential))return 1;
    struct Candidate {unsigned hex;int score;};std::vector<Candidate> candidates;
    GameList nearby;list_init(nearby);Obj city_tile=table(0xE270,city_hex,48399);
    if(valid(city_tile))fn<void(__fastcall *)(Obj,int,GameList *)>(GA(GA_2A9C90))(city_tile,15,&nearby);
    bool seen[501]={};
    for(Link *entry=list_first(nearby);entry;entry=entry->next) {
        unsigned id=valid(entry->value)?campaign_area_id(fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F830))(entry->value)):0;
        if(!id || seen[id])continue;seen[id]=true;Obj area=table(0x6CAF0,id,500);
        if(!valid(area) || campaign_transit_region(id) || !campaign_region_hostile(army,id))continue;
        unsigned hex=word(area,0x3A);if(hex<48400)candidates.push_back({hex,distance(word(army,0x2A),hex)});
    }
    list_free(nearby);
    std::sort(candidates.begin(),candidates.end(),[](const Candidate &a,const Candidate &b){return a.score!=b.score?a.score<b.score:a.hex<b.hex;});
    for(const Candidate &c:candidates)if(campaign_assign_safe(army,p,c.hex,true,sequential))return 1;
    p.region_target=48400;return candidates.empty()?0u:2u;
}
static void prepare_local_build(Obj army, Progress &p) { COST_SCOPE("campaign:prepare_local_build");
    if(p.facility || p.credit || p.build_chain)return;
    Obj area=army_area(army);if(!valid(area))return;
    unsigned hex=word(area,0x3A);Obj center=table(0xE270,hex,48399);
    Obj goal=p.campaign?campaign_goal(p.city):nullptr;
    unsigned castle=goal?fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(goal):48400;
    Obj standing=table(0xE270,word(army,0x2A),48399);
    bool siege_ground=goal && hostile_city(army,p.city) && valid(standing) && standing[0x14]==army_force(army) &&
        campaign_area_id(castle)==campaign_area_id(word(army,0x2A));
    // Build policy v2 (2026-10-21): facilities serve a siege. Only an army of a hostile-city mission
    // standing on its own ground within 10 hexes of that castle may build, and only while the region
    // around it has none of the siege facilities (fort 4, catapult 6, band 7, camp 3) of its force,
    // including ones another army is already building. Passing friendly regions build nothing.
    if(mod_features.mission_v2 && !p.regional_worker && !builder_force(army_force(army))) {
        if(!goal || !hostile_city(army,p.city) || !valid(standing) || standing[0x14]!=army_force(army) ||
           castle>=48400 || distance(word(army,0x2A),castle)>10)return; // 8 -> 10 (user 2026-11-01)
        // local53: no longer stops at the first siege facility; try_build builds catapult/watchtower and fort/camp up to
        // 2 each (local263), a band, and a trap on its roll. A full region stops in try_build.
        if(area_full(area))return;
        if(p.work_reviewed_center==hex)return; // already rejected twice / complete here
        if(p.build_audit_day==game_day() && p.build_audit_mask)return;
        p.build_chain=true;p.build_center=static_cast<unsigned short>(hex);p.cycle_mask=0;
        log_event("siege_build_opportunity",army_id(army),p.city,hex);
        return;
    }
    if(campaign_group_marching(army) && !siege_ground)return;
    if(siege_ground && p.build_audit_day==game_day() && p.build_audit_mask)return;
    if(!valid(center) || (!siege_ground && (center[0x14]!=army_force(army) ||
       fn<int(__fastcall *)(Obj)>(GA(GA_234020))(area) || fn<int(__fastcall *)(Obj)>(GA(GA_234590))(area) ||
       distance(word(army,0x2A),hex)>=11)))return;
    if(p.regional_worker && ((p.work_reviewed_center==hex && area_full(area)) ||
       (p.region_target!=hex && distance(word(army,0x2A),hex)>1)))return;
    // This is an opportunity to build in a friendly offensive region, not
    // a fabricated historical capture. Native cost/cap/terrain checks follow.
    p.build_chain=true;p.build_center=static_cast<unsigned short>(hex);p.cycle_mask=0;
}
static bool recovery_return(Obj army, Progress &p) { COST_SCOPE("campaign:recovery_return");
    Obj origin=fn<Obj(__fastcall *)(Obj)>(GA(GA_22BFE0))(army);
    unsigned hex=valid(origin)?fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(origin):48400;
    Obj tile=table(0xE270,hex,48399);
    if(hex>=48400 || !valid(tile) || tile[0x14]!=army_force(army)) {
        if(!p.forced_return)return false;
        int nearest=INT32_MAX;origin=nullptr;
        for(unsigned id=1;id<=51;++id) {
            Obj city=campaign_goal(id);if(!city)continue;
            unsigned h=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(city);Obj t=table(0xE270,h,48399);
            if(!valid(t) || t[0x14]!=army_force(army))continue;
            int range=distance(word(army,0x2A),h);if(range<nearest) {nearest=range;origin=city;hex=h;}
        }
        if(!origin)return false;
    }
    if(word(army,0x2A)==hex) {p.returning=false;return false;}
    fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
    // A gate home (base 52..61) takes the original gate-retreat form: action 12, target kind 6, gate
    // number base-51 (611C0). The former city form (11/5/base) was rejected by daily processing every
    // turn and parked the army in place (2026-10-22: 사마의 at 유미 for ~360 days).
    unsigned base=word(origin,0x4E);bool gate=base>51 && base<=61;
    if(p.returning && army[0x37]!=(gate?12:11))log_event("return_order_not_kept",army_id(army),army[0x37],base);
    fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,gate?12:11);
    fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,gate?6:5,static_cast<int>(gate?base-51:base));
    fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(army);
    if(!p.returning)log_event(p.forced_return?"forced_return_home":"idle_return_home",army_id(army),word(army,0x16),hex);
    p.returning=true;p.recovery_hex=static_cast<unsigned short>(hex);p.campaign=false;
    p.region_target=48400;p.staging_hex=48400;return true;
}
// The original AI already orders this army against its troops' hostile goal city/gate: the mod's
// own field steering (path captures, campaign steps) stands aside (2026-10-21, mission coherence).
// 1a4a447d (docs/HISTORY.md, conflict 2): the original 27D490/2A3E30 (called from 68CE0/613A0/64E10) hands an
// attacking troop's member to a hostile enemy foothold or watchtower: action 6, kind 11, target = building id. That is the
// original AI's own mission, so it is recognised like the city assault (the previous build never issued it from these nodes; a hostile
// kind-11 target is harmless there too, the mod never orders action 6).
static bool native_fort_assault(Obj army,unsigned *target_hex=nullptr) { COST_SCOPE("campaign:native_fort_assault");
    Obj troops=table(0x7F8D8,word(army,0x58),500);if(!valid(troops) || !attack_mode(word(troops,0x14)))return false;
    unsigned id=word(army,0x3A);
    if(army[0x37]!=6 || army[0x38]!=11 || !id || id>3000)return false;
    Obj building=table(0x6DA98,id,3000);if(!valid(building))return false;
    unsigned owner=building[0x13];
    if(!(owner>0 && owner<52 && fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(army_force(army),owner)!=0))return false;
    if(target_hex)*target_hex=word(building,0x10); // building tile (same field as native_campaign_support.inl)
    return true;
}
// siege_stall_near (user 2026-11-10): centre hex of a siege goal; goal>51 is a gate (mission_route_waypoint rule).
static unsigned siege_goal_hex(unsigned goal) { COST_SCOPE("campaign:siege_goal_hex");
    if(!goal || goal>61)return 48400;
    Obj city=campaign_goal(goal);
    if(!valid(city) && goal>51)city=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(world(),goal);
    if(!valid(city))return 48400;
    return fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(city);
}
static unsigned siege_tile_distance(unsigned a,unsigned b) { COST_SCOPE("campaign:siege_tile_distance");
    if(a>=48400 || b>=48400)return 9999u;
    return static_cast<unsigned>(distance(a,b));
}
static bool native_on_mission(Obj army) { COST_SCOPE("campaign:native_on_mission");
    Obj troops=table(0x7F8D8,word(army,0x58),500);if(!valid(troops) || !attack_mode(word(troops,0x14)))return false;
    if(native_fort_assault(army)) {
        static volatile LONG samples;
        if(InterlockedIncrement(&samples)<=200)log_event("native_fort_assault",army_id(army),word(army,0x3A),word(army,0x2A));
        return true;
    }
    unsigned goal=word(troops,0x16);
    return goal && goal<=61 && (army[0x37]==4 || army[0x37]==5) && (army[0x38]==5 || army[0x38]==6) &&
        word(army,0x3A)==(army[0x38]==6 && goal>51?goal-51:goal) && hostile_city(army,goal);
}
// mission_v2 (2026-10-21): the original 613A0 (CAttackFootholdRelayAreaAttackNode) resets every
// member to the goal-city assault each turn and then assigns captures from the troops work list
// (+18 u16[10] region centers); it already prepends a hostile region its leader stands in. The mod
// adds the next hostile region crossed by the native route to that castle the same way, so the
// original AI orders the capture itself and no order is fought over.
// mission_v2 corridor (user request 2026-10-23): in addition to the route waypoint, offer the original
// list a few more capturable regions between the departure city and the goal, so 613A0 takes the
// neighbours that feed the goal's adjacency bonus. Region graph = Area+56/+66 (same as 2B07C0); a
// city/gate region (232680) is never crossed, so a route cannot run past a castle or gate. A region
// qualifies when departure->region->goal is at most one region longer than the shortest chain; ports
// are ordinary candidates under the same cost bound. Only empty list slots are filled, once per goal,
// so the original keeps its own entries, order, assignment and the decision to start the siege.
static unsigned char mission_corridor_distance(Obj lead,unsigned from,unsigned goal,unsigned char *dist) { COST_SCOPE("campaign:mission_corridor_distance");
    memset(dist,255,501);dist[from]=0;unsigned queue[500],head=0,tail=0;queue[tail++]=from;
    while(head<tail) {
        unsigned id=queue[head++];Obj a=table(0x6CAF0,id,500);if(!valid(a))continue;
        if(id!=from && fn<int(__fastcall *)(Obj)>(GA(GA_234020))(a))continue;   // castles and gates end a chain
        for(unsigned i=0;i<8;++i) {
            unsigned n=word(a,0x56+i*2);if(!(a[0x66]&(1u<<i)) || !n || n>500 || dist[n]!=255)continue;
            if(n!=goal && !campaign_region_open(lead,n))continue;
            dist[n]=static_cast<unsigned char>(dist[id]+1);if(tail<500)queue[tail++]=n;
        }
    }
    return dist[goal];
}
static struct {Obj root;unsigned short goal;} mission_corridor_done[501];
static void mission_route_corridor(Obj troops,Obj lead,unsigned castle_hex) { COST_SCOPE("campaign:mission_route_corridor");
    unsigned id=word(lead,0x58),goal=word(troops,0x16);if(!id || id>500)return;
    auto &done=mission_corridor_done[id];if(done.root==world() && done.goal==goal)return;
    done={world(),static_cast<unsigned short>(goal)};
    Obj origin=fn<Obj(__fastcall *)(Obj)>(GA(GA_22BFE0))(lead);
    unsigned from=campaign_area_id(valid(origin)?fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(origin):word(lead,0x2A));
    unsigned to=campaign_area_id(castle_hex);if(!from || !to || from==to)return;
    unsigned char ds[501],dg[501];unsigned shortest=mission_corridor_distance(lead,from,to,ds);
    if(shortest==255 || shortest>12 || mission_corridor_distance(lead,to,from,dg)==255)return;
    unsigned short *list=reinterpret_cast<unsigned short *>(troops+0x18);
    unsigned free_slots=0;for(unsigned i=0;i<10;++i)if(list[i]>=48400)++free_slots;
    std::vector<unsigned> pick;
    for(unsigned r=1;r<=500;++r) {
        if(r==from || r==to || ds[r]==255 || dg[r]==255 || static_cast<unsigned>(ds[r]+dg[r])>shortest+1)continue;
        Obj a=table(0x6CAF0,r,500);if(!valid(a) || fn<int(__fastcall *)(Obj)>(GA(GA_234020))(a) || !campaign_region_hostile(lead,r))continue;
        unsigned center=word(a,0x3A);if(center>=48400)continue;
        bool listed=false;for(unsigned i=0;i<10;++i)if(list[i]==center)listed=true;
        if(!listed)pick.push_back(r);
    }
    // Goal neighbours first (they carry the defender's adjacency bonus), then nearest to the departure.
    std::sort(pick.begin(),pick.end(),[&](unsigned a,unsigned b) {
        bool na=dg[a]==1,nb=dg[b]==1;return na!=nb?na:ds[a]!=ds[b]?ds[a]<ds[b]:a<b;});
    if(pick.size()>(std::min)(3u,free_slots))pick.resize((std::min)(3u,free_slots));
    std::sort(pick.begin(),pick.end(),[&](unsigned a,unsigned b) {return ds[a]!=ds[b]?ds[a]<ds[b]:a<b;});
    unsigned added=0;
    for(unsigned r:pick)for(unsigned i=0;i<10;++i)if(list[i]>=48400) {
        list[i]=word(table(0x6CAF0,r,500),0x3A);++added;break;}
    if(added)log_event("mission_route_corridor",army_id(lead),added|(shortest<<8),goal);
}
static void mission_route_waypoint(Obj troops) { COST_SCOPE("campaign:mission_route_waypoint");
    if(!valid(troops) || !attack_mode(word(troops,0x14)))return;
    unsigned goal=word(troops,0x16);Obj castle=goal && goal<=61?campaign_goal(goal):nullptr;
    if(!castle && goal>51 && goal<=61)castle=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(world(),goal);
    if(!valid(castle))return;
    Obj lead=nullptr;
    for(unsigned i=1;i<=500;++i) {Obj a=table(0x7E838,i,500);
        if(valid(a) && belongs_to_troops(a,troops) && campaign_controlled(a) && word(a,0x16)>=1000 &&
           (!lead || word(a,0x16)>word(lead,0x16)))lead=a;}
    if(!lead || !hostile_city(lead,goal))return;
    unsigned castle_hex=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(castle);
    mission_route_corridor(troops,lead,castle_hex);
    unsigned h=campaign_path_capture_hex(lead,castle_hex);
    if(h>=48400 || h==castle_hex || campaign_area_id(h)==campaign_area_id(castle_hex))return;
    unsigned short *list=reinterpret_cast<unsigned short *>(troops+0x18);
    for(unsigned i=0;i<10;++i)if(list[i]==h)return;
    for(unsigned i=9;i>0;--i)list[i]=list[i-1];
    list[0]=static_cast<unsigned short>(h);
    log_event("mission_route_waypoint",army_id(lead),h,goal);
}
static bool targetless(Obj army) { COST_SCOPE("campaign:targetless");
    if(army[0x37]!=13 || army[0x38]!=255 || word(army,0x3A)!=0)return false;
    for(unsigned j=0;j<5;++j)if(word(army,0x2C+j*2)<48400)return false;
    return true;
}
static bool recover_idle(Obj army, Progress &p) { COST_SCOPE("campaign:recover_idle");
    if(!p.forced_return && p.returning && word(army,0x16)>=1000) {p.returning=false;return false;}
    if(p.returning)return recovery_return(army,p);
    if(!targetless(army) || p.facility || priority_mission(army,p))return false;
    if(word(army,0x16)<1000)return recovery_return(army,p);
    if(p.campaign)return false; // Immediately continue its regional work.
    Obj troops=table(0x7F8D8,word(army,0x58),500);
    bool can_retarget=valid(troops) && attack_mode(word(troops,0x14));
    // A Troops goal is shared. Do not redirect a group while any other member
    // is moving, fighting, building, cutting logistics or has depleted troops.
    for(unsigned i=1;can_retarget && i<=500;++i) {
        Obj member=table(0x7E838,i,500);
        if(!valid(member) || member==army || !belongs_to_troops(member,troops))continue;
        Progress *m=observe_capture(member);
        if(!computer_army(member) || word(member,0x16)<1000 || !targetless(member) ||
           !m || m->facility || priority_mission(member,*m))can_retarget=false;
    }
    unsigned city=0;int best=13;
    for(unsigned i=1;can_retarget && i<=500;++i) {
        Obj ally=table(0x7E838,i,500);
        if(ally==army || !computer_army(ally) || army_force(ally)!=army_force(army) ||
           word(ally,0x16)<1000 || targetless(ally))continue;
        Obj group=table(0x7F8D8,word(ally,0x58),500);
        if(!valid(group) || !attack_mode(word(group,0x14)))continue;
        unsigned goal=word(group,0x16);int range=distance(word(army,0x2A),word(ally,0x2A));
        if(goal<52 && hostile_city(army,goal) && !tribe_goal(goal) && range<best) {best=range;city=goal;}
    }
    if(city) {
        *reinterpret_cast<unsigned short *>(troops+0x14)=10;
        *reinterpret_cast<unsigned short *>(troops+0x16)=static_cast<unsigned short>(city);
        p.city=static_cast<unsigned short>(city);p.campaign=true;
        p.region_target=48400;p.staging_hex=48400;
        log_event("idle_join_campaign",army_id(army),city,word(army,0x2A));
        // Joining supplies a strategic goal; apply_priority still selects
        // construction/regions/rally before permitting the city command.
        return false;
    }
    return false; // Healthy armies are not sent home because work is reserved.
}
// Keep the old per-unit marker cleared; there is no assembly gate in v5.
static void pause_assembly(Progress &p) {p.staging_hex=48400;}
static bool campaign_step(Obj army, Progress &p, Obj node=nullptr, Obj context=nullptr) { COST_SCOPE("campaign:campaign_step");
    Obj goal=campaign_goal(p.city);
    if(!goal || !hostile_city(army,p.city)) {p.campaign=false;return false;}
    unsigned hex=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(goal);
    if(hex>=48400) {p.campaign=false;return false;}
    unsigned region_state=next_region(army,p,hex);
    if(region_state==1) {p.region_blocked_day=-1;p.staging_hex=48400;return true;}
    if(region_state==2 && node && context) {
        offensive_order(army,p.city,true);
        if(try_other(army,node,context))return true;
    }
    // No stand-at-HEX, timer, participant count, or healthy-unit return.
    // If a center cannot be reached, try native field work and local centers
    // before the final city fallback. Native path/threat checks remain active.
    p.credit=false;p.build_chain=false;p.region_blocked_day=-1;
    offensive_order(army,p.city,true);
    (void)node;(void)context;
    fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(army);
    log_event("campaign_city_last_fallback",army_id(army),p.city,hex);
    return true;
}
static bool campaign_morale_priority(Obj army,Progress &p);
// AI-CONTRACT [writer order]: Ordering is behavior. Audit competing command writers, same-day callbacks, goal capture, identity reuse and direct-player ownership before moving a branch; compare campaign priority/operation fixtures.
static bool campaign_individual_priority(Obj army,Progress &p) { COST_SCOPE("campaign:campaign_individual_priority");
    if(campaign_forced_siege_priority(army,p))return true;
    const CampaignHit &hit=campaign_hits[army_id(army)];
    bool recent_hit=hit.present && hit.root==world() && hit.victim==army && hit.commander==word(army,0x12) &&
        hit.force==army_force(army) && game_day()>=hit.day && game_day()-hit.day<=10;
    // Only a changed tile resets motion_day in observe_capture. No morale,
    // work, damage or attack-order exemption may extend forty stationary days.
    // mission_v2 (user decision 2026-10-21, option B): an army attacking a hostile city or gate is
    // besieging, not stalled; every such day counts as progress.
    // siege_stall_near (user 2026-11-10, computer side only): only while the army really stands next to the castle
    // (1 hex) or was hit recently; an army blocked farther away still reaches the 40-day retreat.
    const bool stall_near=mod_features.siege_stall_near && computer_side_army(army);
    if(mod_features.mission_v2 && (army[0x37]==4 || army[0x37]==5) && (army[0x38]==5 || army[0x38]==6)) {
        unsigned goal=army[0x38]==6?word(army,0x3A)+51:word(army,0x3A);
        if(goal && goal<=61 && hostile_city(army,goal) &&
           (!stall_near || recent_hit || siege_tile_distance(word(army,0x2A),siege_goal_hex(goal))<=1))p.motion_day=game_day();
    }
    // Same for the original AI's own assault on a hostile foothold/watchtower (action 6, kind 11; 1a4a447d 27D490).
    if(mod_features.mission_v2) {
        unsigned fort_hex=48400;
        if(native_fort_assault(army,&fort_hex) &&
           (!stall_near || recent_hit || siege_tile_distance(word(army,0x2A),fort_hex)<=1))p.motion_day=game_day();
    }
    // A route_hook correction (morale hold / wall breach) is the only mod writer for this army; holding still is intended.
    if(route_hook_owns && route_hook_owns(army)) {p.motion_day=game_day();return true;}
    if(mod_features.stationary_retreat && !p.forced_return && game_day()-p.motion_day>=campaign_stationary_days) {
        p.forced_return=true;p.guard_beneficiary=0;p.combat_target=0;p.combat_city=0;
        p.facility=0;p.credit=false;p.build_chain=false;
        p.morale_recovering=false;p.morale_building=false;p.morale_saved=false;
        army[0x46]=0;*reinterpret_cast<unsigned short *>(army+0x42)=48400;
        log_event("four_turn_stationary_retreat",army_id(army),word(army,0x16),word(army,0x2A));
    }
    if(p.forced_return)return recovery_return(army,p);
    // Original AI owns army interception and castle retaliation. Damage is
    // retained solely for obstacle evidence and depleted-unit recovery.
    p.combat_target=0;p.combat_city=0;p.home_defense_city=0;
    if(recent_hit && word(army,0x16)<1000) {p.forced_return=true;log_event("depleted_hit_return",army_id(army),word(army,0x16),word(army,0x2A));return recovery_return(army,p);}
    if(campaign_morale_priority(army,p))return true;
    if(p.guard_beneficiary && campaign_guard_priority(army,p))return true;
    if(p.siege_city && hostile_city(army,p.siege_city) && !p.facility) {
        // A direct weak-city attack still reviews usable siege ground before
        // committing to the castle; an active build is handled below.
        Obj city=campaign_goal(p.siege_city);unsigned h=city?fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(city):48400;
        if(campaign_area_id(h)==campaign_area_id(word(army,0x2A)))prepare_local_build(army,p);
        if(!p.credit && !p.build_chain) {
            if(campaign_near_capture(army,p))return true;
            offensive_order(army,p.siege_city,true);fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(army);return true;
        }
    }
    return false;
}
