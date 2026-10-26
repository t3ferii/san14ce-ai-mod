/* Included after the native adapter helpers. All orders run on the game's AI
 * thread. Dates use its own 30-day calendar conversion, not wall-clock time. */
static int game_day();
static bool construction_completed(const Progress &p) {
    if(p.finished_build || p.finished_demolition)return game_day()<=p.return_deadline_day;
    if(!p.facility || p.construction_hex>=48400)return false;
    Obj tile=table(0xE270,p.construction_hex,48399);
    if(!valid(tile) || !word(tile,0x1A))return false;
    Obj building=fn<Obj(__fastcall *)(Obj)>(0x22FB10)(tile);
    if(!valid(building) || building[0x13]!=p.force || building[0x12]!=p.facility)return false;
    int maximum=fn<int(__fastcall *)(Obj)>(0x2AC120)(building);
    return maximum>0 && word(building,0x14)>=maximum;
}
static bool attack_mode(unsigned mode) { return mode==8 || mode==9 || mode==10 || mode==11 || mode==22; }
// AI-CONTRACT [policy]: Game days use the native 30-day calendar. Preserve actual hostile castle/gate damage and cohort-only 30-day forced-march exceptions before ordinary 40-day stationary retreat. Do not infer a siege lock from an attack order alone.
static constexpr int campaign_stationary_days=40;
static int game_day() {
    Obj context = pointer(world(),0x871E8);
    int day = fn<int(__fastcall *)(Obj)>(0x32BA50)(context+0x34);
#ifndef NO_RUNTIME_LOG
    InterlockedExchange(&log_day, day);
#endif
    return day;
}
static Obj campaign_goal(unsigned city) {
    if (!city || city>=52) return nullptr;
    Obj goal=fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),city);
    return valid(goal) && word(goal,0x4E)==city ? goal : nullptr;
}
static bool hostile_city(Obj army, unsigned city) {
    Obj goal=campaign_goal(city);
    Obj force=goal ? fn<Obj(__fastcall *)(Obj)>(0x22C990)(goal) : nullptr;
    if (!valid(force)) return false;
    auto id=*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(force,0)+0x60);
    return fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(army_force(army),id(force))!=0;
}
static bool campaign_transit_region(unsigned id);
static unsigned campaign_path_capture_hex(Obj army,unsigned destination);
static unsigned campaign_area_id(unsigned hex);
static bool campaign_region_open(Obj army,unsigned id);
static void remember_campaign(Obj army, Progress &p) {
    if(p.returning || p.forced_return) {p.campaign=false;return;}
    if(p.operation_city && campaign_operation_member(army,p)) {
        p.city=p.operation_city;p.campaign=true;return;
    }
    if(p.campaign && !hostile_city(army,p.city)) {
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
static bool region_reserved(unsigned hex, Obj actor) {
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
static void offensive_order(Obj army, unsigned city, bool clear) {
    if (clear) fn<void(__fastcall *)(Obj)>(0xA2410)(army);
    fn<void(__fastcall *)(Obj,int)>(0x2EAAE0)(army,4);
    fn<void(__fastcall *)(Obj,int,int)>(0x2EAB40)(army,5,static_cast<int>(city));
}
static bool campaign_transit_region(unsigned id);
#ifdef SORTIE_FORMATION
static bool campaign_request_companion(Obj army,Progress &p,unsigned target);
#else
static bool campaign_request_companion(Obj,Progress &,unsigned) {return false;}
#endif
#include "native_campaign_routes.inl"
static bool assign_region(Obj army, Progress &p, unsigned hex) {
    return campaign_assign_safe(army,p,hex,true);
}
// 2 blocks city assault until the graph corridor/guarded route is available.
static bool campaign_request_guard(Obj army,Progress &p,unsigned region,unsigned goal);
static bool campaign_transit_relay(Obj army,Progress &p,unsigned region,unsigned goal,bool plan);
static bool campaign_defensive_wait(Obj army,Progress &p);
static bool campaign_guard_priority(Obj army,Progress &p);
static bool campaign_transit_region(unsigned id) {
    Obj area=table(0x6CAF0,id,500);
    return valid(area) && (fn<int(__fastcall *)(Obj)>(0x232680)(area) || fn<int(__fastcall *)(Obj)>(0x232BF0)(area));
}
static bool campaign_request_companion(Obj army,Progress &p,unsigned target);
// Adjacency selects missions only; every movement path remains native.
static bool campaign_adjacent_city_base(unsigned hex,unsigned city) {
    if(!city || city>61 || hex>=48400)return false;
    Obj goal=fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),city);
    if(!valid(goal) || word(goal,0x4E)!=city)return false;
    unsigned h=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(goal);
    unsigned region=campaign_area_id(h),base=campaign_area_id(hex);
    Obj area=table(0x6CAF0,region,500);if(!valid(area) || !base || base==region || campaign_transit_region(base))return false;
    for(unsigned i=0;i<8;++i)if((area[0x66]&(1u<<i)) && word(area,0x56+i*2)==base)return true;
    return false;
}
static std::vector<unsigned> campaign_home_bases(Obj army,unsigned only_city=0) {
    std::vector<unsigned> result;unsigned depth[501]={};unsigned here=word(army,0x2A),own=army_force(army);
    for(unsigned id=1;id<=51;++id) {
        if(only_city && only_city!=id)continue;
        Obj city=campaign_goal(id);if(!city)continue;
        unsigned h=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(city);Obj t=table(0xE270,h,48399);
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
        Obj a=table(0x6CAF0,region,500);if(!valid(a) || fn<int(__fastcall *)(Obj)>(0x232680)(a) ||
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
    Obj goal=campaign_goal(city);unsigned destination=goal?fn<unsigned(__fastcall *)(Obj)>(0x22D740)(goal):48400;
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
static bool campaign_regional_step(Obj army,Progress &p) {
    COST_SCOPE("campaign_regional_step");
    if(!p.regional_worker)return false;
    if(p.credit || p.build_chain || p.facility)return true;
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
static unsigned next_region(Obj army, Progress &p, unsigned city_hex,bool sequential=false) {
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
    if(valid(city_tile))fn<void(__fastcall *)(Obj,int,GameList *)>(0x2A7AA0)(city_tile,15,&nearby);
    bool seen[501]={};
    for(Link *entry=list_first(nearby);entry;entry=entry->next) {
        unsigned id=valid(entry->value)?campaign_area_id(fn<unsigned(__fastcall *)(Obj)>(0x22DDA0)(entry->value)):0;
        if(!id || seen[id])continue;seen[id]=true;Obj area=table(0x6CAF0,id,500);
        if(!valid(area) || campaign_transit_region(id) || !campaign_region_hostile(army,id))continue;
        unsigned hex=word(area,0x3A);if(hex<48400)candidates.push_back({hex,distance(word(army,0x2A),hex)});
    }
    list_free(nearby);
    std::sort(candidates.begin(),candidates.end(),[](const Candidate &a,const Candidate &b){return a.score!=b.score?a.score<b.score:a.hex<b.hex;});
    for(const Candidate &c:candidates)if(campaign_assign_safe(army,p,c.hex,true,sequential))return 1;
    p.region_target=48400;return candidates.empty()?0u:2u;
}
static void prepare_local_build(Obj army, Progress &p) {
    if(p.facility || p.credit || p.build_chain)return;
    Obj area=army_area(army);if(!valid(area))return;
    unsigned hex=word(area,0x3A);Obj center=table(0xE270,hex,48399);
    Obj goal=p.campaign?campaign_goal(p.city):nullptr;
    unsigned castle=goal?fn<unsigned(__fastcall *)(Obj)>(0x22D740)(goal):48400;
    Obj standing=table(0xE270,word(army,0x2A),48399);
    bool siege_ground=goal && hostile_city(army,p.city) && valid(standing) && standing[0x14]==army_force(army) &&
        campaign_area_id(castle)==campaign_area_id(word(army,0x2A));
    // Build policy v2 (2026-10-21): facilities serve a siege. Only an army of a hostile-city mission
    // standing on its own ground within 8 hexes of that castle may build, and only while the region
    // around it has none of the siege facilities (fort 4, catapult 6, band 7, camp 3) of its force,
    // including ones another army is already building. Passing friendly regions build nothing.
    if(mod_features.mission_v2 && !p.regional_worker && !builder_force(army_force(army))) {
        if(!goal || !hostile_city(army,p.city) || !valid(standing) || standing[0x14]!=army_force(army) ||
           castle>=48400 || distance(word(army,0x2A),castle)>8)return;
        if(local_facilities(army,area) & ((1u<<3)|(1u<<4)|(1u<<6)|(1u<<7)))return;
        if(p.work_reviewed_center==hex)return; // already rejected twice / complete here
        if(p.build_audit_day==game_day() && p.build_audit_mask)return;
        p.build_chain=true;p.build_center=static_cast<unsigned short>(hex);p.cycle_mask=0;
        log_event("siege_build_opportunity",army_id(army),p.city,hex);
        return;
    }
    if(campaign_group_marching(army) && !siege_ground)return;
    if(siege_ground && p.build_audit_day==game_day() && p.build_audit_mask)return;
    if(!valid(center) || (!siege_ground && (center[0x14]!=army_force(army) ||
       fn<int(__fastcall *)(Obj)>(0x232680)(area) || fn<int(__fastcall *)(Obj)>(0x232BF0)(area) ||
       distance(word(army,0x2A),hex)>=11)))return;
    if(p.regional_worker && ((p.work_reviewed_center==hex && area_full(area)) ||
       (p.region_target!=hex && distance(word(army,0x2A),hex)>1)))return;
    // This is an opportunity to build in a friendly offensive region, not
    // a fabricated historical capture. Native cost/cap/terrain checks follow.
    p.build_chain=true;p.build_center=static_cast<unsigned short>(hex);p.cycle_mask=0;
}
static bool recovery_return(Obj army, Progress &p) {
    Obj origin=fn<Obj(__fastcall *)(Obj)>(0x22A5A0)(army);
    unsigned hex=valid(origin)?fn<unsigned(__fastcall *)(Obj)>(0x22D740)(origin):48400;
    Obj tile=table(0xE270,hex,48399);
    if(hex>=48400 || !valid(tile) || tile[0x14]!=army_force(army)) {
        if(!p.forced_return)return false;
        int nearest=INT32_MAX;origin=nullptr;
        for(unsigned id=1;id<=51;++id) {
            Obj city=campaign_goal(id);if(!city)continue;
            unsigned h=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(city);Obj t=table(0xE270,h,48399);
            if(!valid(t) || t[0x14]!=army_force(army))continue;
            int range=distance(word(army,0x2A),h);if(range<nearest) {nearest=range;origin=city;hex=h;}
        }
        if(!origin)return false;
    }
    if(word(army,0x2A)==hex) {p.returning=false;return false;}
    fn<void(__fastcall *)(Obj)>(0xA2410)(army);
    // A gate home (base 52..61) takes the original gate-retreat form: action 12, target kind 6, gate
    // number base-51 (611C0). The former city form (11/5/base) was rejected by daily processing every
    // turn and parked the army in place (2026-10-22: 사마의 at 유미 for ~360 days).
    unsigned base=word(origin,0x4E);bool gate=base>51 && base<=61;
    if(p.returning && army[0x37]!=(gate?12:11))log_event("return_order_not_kept",army_id(army),army[0x37],base);
    fn<void(__fastcall *)(Obj,int)>(0x2EAAE0)(army,gate?12:11);
    fn<void(__fastcall *)(Obj,int,int)>(0x2EAB40)(army,gate?6:5,static_cast<int>(gate?base-51:base));
    fn<void(__fastcall *)(Obj)>(0x26C940)(army);
    if(!p.returning)log_event(p.forced_return?"forced_return_home":"idle_return_home",army_id(army),word(army,0x16),hex);
    p.returning=true;p.recovery_hex=static_cast<unsigned short>(hex);p.campaign=false;
    p.region_target=48400;p.staging_hex=48400;return true;
}
// The original AI already orders this army against its troops' hostile goal city/gate: the mod's
// own field steering (path captures, campaign steps) stands aside (2026-10-21, mission coherence).
static bool native_on_mission(Obj army) {
    Obj troops=table(0x7F8D8,word(army,0x58),500);if(!valid(troops) || !attack_mode(word(troops,0x14)))return false;
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
static unsigned char mission_corridor_distance(Obj lead,unsigned from,unsigned goal,unsigned char *dist) {
    memset(dist,255,501);dist[from]=0;unsigned queue[500],head=0,tail=0;queue[tail++]=from;
    while(head<tail) {
        unsigned id=queue[head++];Obj a=table(0x6CAF0,id,500);if(!valid(a))continue;
        if(id!=from && fn<int(__fastcall *)(Obj)>(0x232680)(a))continue;   // castles and gates end a chain
        for(unsigned i=0;i<8;++i) {
            unsigned n=word(a,0x56+i*2);if(!(a[0x66]&(1u<<i)) || !n || n>500 || dist[n]!=255)continue;
            if(n!=goal && !campaign_region_open(lead,n))continue;
            dist[n]=static_cast<unsigned char>(dist[id]+1);if(tail<500)queue[tail++]=n;
        }
    }
    return dist[goal];
}
static struct {Obj root;unsigned short goal;} mission_corridor_done[501];
static void mission_route_corridor(Obj troops,Obj lead,unsigned castle_hex) {
    unsigned id=word(lead,0x58),goal=word(troops,0x16);if(!id || id>500)return;
    auto &done=mission_corridor_done[id];if(done.root==world() && done.goal==goal)return;
    done={world(),static_cast<unsigned short>(goal)};
    Obj origin=fn<Obj(__fastcall *)(Obj)>(0x22A5A0)(lead);
    unsigned from=campaign_area_id(valid(origin)?fn<unsigned(__fastcall *)(Obj)>(0x22D740)(origin):word(lead,0x2A));
    unsigned to=campaign_area_id(castle_hex);if(!from || !to || from==to)return;
    unsigned char ds[501],dg[501];unsigned shortest=mission_corridor_distance(lead,from,to,ds);
    if(shortest==255 || shortest>12 || mission_corridor_distance(lead,to,from,dg)==255)return;
    unsigned short *list=reinterpret_cast<unsigned short *>(troops+0x18);
    unsigned free_slots=0;for(unsigned i=0;i<10;++i)if(list[i]>=48400)++free_slots;
    std::vector<unsigned> pick;
    for(unsigned r=1;r<=500;++r) {
        if(r==from || r==to || ds[r]==255 || dg[r]==255 || static_cast<unsigned>(ds[r]+dg[r])>shortest+1)continue;
        Obj a=table(0x6CAF0,r,500);if(!valid(a) || fn<int(__fastcall *)(Obj)>(0x232680)(a) || !campaign_region_hostile(lead,r))continue;
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
static void mission_route_waypoint(Obj troops) {
    if(!valid(troops) || !attack_mode(word(troops,0x14)))return;
    unsigned goal=word(troops,0x16);Obj castle=goal && goal<=61?campaign_goal(goal):nullptr;
    if(!castle && goal>51 && goal<=61)castle=fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),goal);
    if(!valid(castle))return;
    Obj lead=nullptr;
    for(unsigned i=1;i<=500;++i) {Obj a=table(0x7E838,i,500);
        if(valid(a) && belongs_to_troops(a,troops) && campaign_controlled(a) && word(a,0x16)>=1000 &&
           (!lead || word(a,0x16)>word(lead,0x16)))lead=a;}
    if(!lead || !hostile_city(lead,goal))return;
    unsigned castle_hex=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(castle);
    mission_route_corridor(troops,lead,castle_hex);
    unsigned h=campaign_path_capture_hex(lead,castle_hex);
    if(h>=48400 || h==castle_hex || campaign_area_id(h)==campaign_area_id(castle_hex))return;
    unsigned short *list=reinterpret_cast<unsigned short *>(troops+0x18);
    for(unsigned i=0;i<10;++i)if(list[i]==h)return;
    for(unsigned i=9;i>0;--i)list[i]=list[i-1];
    list[0]=static_cast<unsigned short>(h);
    log_event("mission_route_waypoint",army_id(lead),h,goal);
}
static bool targetless(Obj army) {
    if(army[0x37]!=13 || army[0x38]!=255 || word(army,0x3A)!=0)return false;
    for(unsigned j=0;j<5;++j)if(word(army,0x2C+j*2)<48400)return false;
    return true;
}
static bool recover_idle(Obj army, Progress &p) {
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
        if(goal<52 && hostile_city(army,goal) && range<best) {best=range;city=goal;}
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
static bool campaign_step(Obj army, Progress &p, Obj node=nullptr, Obj context=nullptr) {
    Obj goal=campaign_goal(p.city);
    if(!goal || !hostile_city(army,p.city)) {p.campaign=false;return false;}
    unsigned hex=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(goal);
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
    fn<void(__fastcall *)(Obj)>(0x26C940)(army);
    log_event("campaign_city_last_fallback",army_id(army),p.city,hex);
    return true;
}
static bool campaign_morale_priority(Obj army,Progress &p);
// AI-CONTRACT [writer order]: Ordering is behavior. Audit competing command writers, same-day callbacks, goal capture, identity reuse and direct-player ownership before moving a branch; compare campaign priority/operation fixtures.
static bool campaign_individual_priority(Obj army,Progress &p) {
    if(campaign_forced_siege_priority(army,p))return true;
    const CampaignHit &hit=campaign_hits[army_id(army)];
    bool recent_hit=hit.present && hit.root==world() && hit.victim==army && hit.commander==word(army,0x12) &&
        hit.force==army_force(army) && game_day()>=hit.day && game_day()-hit.day<=10;
    // Only a changed tile resets motion_day in observe_capture. No morale,
    // work, damage or attack-order exemption may extend forty stationary days.
    // mission_v2 (user decision 2026-10-21, option B): an army attacking a hostile city or gate is
    // besieging, not stalled; every such day counts as progress.
    if(mod_features.mission_v2 && (army[0x37]==4 || army[0x37]==5) && (army[0x38]==5 || army[0x38]==6)) {
        unsigned goal=army[0x38]==6?word(army,0x3A)+51:word(army,0x3A);
        if(goal && goal<=61 && hostile_city(army,goal))p.motion_day=game_day();
    }
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
        Obj city=campaign_goal(p.siege_city);unsigned h=city?fn<unsigned(__fastcall *)(Obj)>(0x22D740)(city):48400;
        if(campaign_area_id(h)==campaign_area_id(word(army,0x2A)))prepare_local_build(army,p);
        if(!p.credit && !p.build_chain) {
            if(campaign_near_capture(army,p))return true;
            offensive_order(army,p.siege_city,true);fn<void(__fastcall *)(Obj)>(0x26C940)(army);return true;
        }
    }
    return false;
}
