#include "game_addresses.h"
/* Periodic departures use the existing AI callback, native candidate list,
 * command preparation and paid dispatcher. No fabricated behavior node. */
struct CampaignDepartureAudit {Obj root;int day;};
static CampaignDepartureAudit campaign_departure_force[52],campaign_departure_city[52];
// intercept (gate_intercept, user 2026-11-13): exactly `wanted` soldiers in 안행 (formation 3) from an officer who holds
// it, leaving toward `hex` through the relay points of the way around hostile armies; the caller marks the unit.
// helm (helm_intercept, local148): with intercept, only an officer holding 조타 (trait 154, person +0x118 u16[9]) leaves,
// in his own field formation (the force's ship tier carries him on the water).
static constexpr unsigned kHelmTrait=154;
static bool helm_person(Obj person) {
    for(unsigned i=0;i<9;++i)if(word(person,0x118+i*2)==kHelmTrait)return true;
    return false;
}
// local166: *why (optional) says why no army left: 1 soldiers, 2 no free officer (for helm: no idle 조타 officer), 3 no way
// around hostile armies to hex, 4 gold, 5 no free army slot, 6 capacity/formation, 7 the original departure refused.
static Obj campaign_paid_single(Obj source,Obj target,unsigned hex,unsigned wanted,bool worker,bool retake=false,unsigned avoid_area=0,bool intercept=false,bool helm=false,unsigned *why=nullptr) {
    COST_SCOPE("campaign_paid_single");
    unsigned why_local=2;unsigned &reason=why?*why:why_local;reason=2;
    unsigned budget=campaign::available(dword(source,0x3C));if(budget<(intercept?wanted:worker?1000u:3000u)) {reason=1;return nullptr;}
    GameList people;list_init(people,GA(GA_13466B0));
    fn<void(__fastcall *)(Obj,GameList *)>(GA(GA_2D9530))(source,&people);
    fn<void(__fastcall *)(GameList *)>(GA(GA_2A26B0))(&people);
    fn<void(__fastcall *)(GameList *,int,int,int)>(GA(GA_D4350))(&people,0,1,0);
    std::vector<Obj> candidates;
    for(Link *l=list_first(people);l;l=l->next)if(sortie_auto_person(l->value) && !l->value[0x161])candidates.push_back(l->value);
    list_free(people);
    std::sort(candidates.begin(),candidates.end(),[](Obj a,Obj b){
        return fn<int(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_231770))(a,0,0xFFF)>
               fn<int(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_231770))(b,0,0xFFF);
    });
    for(Obj person:candidates) {
        if(helm && !helm_person(person))continue;
        if((!worker || intercept) && sortie_female_capped(person))continue;   // capped officers leave with 1,000 only
        bool deployed=false;for(unsigned i=1;i<=500;++i) {Obj a=table(0x7E838,i,500);
            if(valid(a) && word(a,0x12)==word(person,0x10)) {deployed=true;break;}}
        if(deployed)continue;
        Obj district=table(0xE0D0,person[0xE0],51);
        // local157 (user 2026-11-14 "조타 장수 ... 점령범위 넓혀주는 병력수로 출진 ... 학익진 우선 ... 영명 개성이 있으면 필요한 병사수가
        // 줄어드는데 ... 점령상승 문턱값 +1000명 ... 불가능하면 봉시 추행진, 아니면 아무거나"): the threshold is the original's
        // (2AF740): formation value 2B3860(formation, 1 wide / 0 narrow) x 100, lowered by the commander's trait effect 0xE %.
        // 학익 (5) first, then the officer's other formations; the wide band before the narrow one; the first that fits
        // capacity and budget with 1,000 more leaves with exactly that. None: 봉시 (2), else 추행 (7), else his field formation.
        unsigned helm_soldiers=0,helm_formation_pick=0,want_now=wanted;
        if(helm) {
            const int capacity_now=fn<int(__fastcall *)(Obj)>(GA(GA_232C00))(person);
            const unsigned room=(std::min)(budget,capacity_now>0?static_cast<unsigned>(capacity_now):0u);
            const float cut=static_cast<float>(fn<int(__fastcall *)(Obj,unsigned)>(GA(GA_2B5120))(person,0xE))/100.0f;
            unsigned pick=0;
            for(unsigned f:{5u,1u,2u,3u,4u,6u,7u,8u}) {
                if(pick || !fn<int(__fastcall *)(Obj,unsigned)>(GA(GA_233CF0))(person,f))continue;
                Obj shape=table(0x77428,f,20);if(!valid(shape))continue;
                for(int band=1;band>=0 && !pick;--band) {
                    const int base=static_cast<int>(fn<unsigned char(__fastcall *)(Obj,int)>(GA(GA_2B3860))(shape,band))*100;
                    const int need=base-static_cast<int>(static_cast<float>(base)*cut);
                    if(need<=0 || static_cast<unsigned>(need)+1000u>room)continue;
                    pick=f;helm_soldiers=static_cast<unsigned>(need)+1000u;
                }
            }
            if(!pick)pick=fn<int(__fastcall *)(Obj,unsigned)>(GA(GA_233CF0))(person,2)?2u:fn<int(__fastcall *)(Obj,unsigned)>(GA(GA_233CF0))(person,7)?7u:0u;
            if(!pick)pick=sortie_field_formation(person);
            if(pick && !helm_soldiers)helm_soldiers=wanted;
#ifndef NO_RUNTIME_LOG
            log_event("helm_formation",word(person,0x10)|(pick<<16),helm_soldiers,room);
#endif
            want_now=helm_soldiers?helm_soldiers:wanted;
            if(helm_soldiers>budget) {reason=1;continue;}
            helm_formation_pick=pick;
        }
        unsigned formation=helm?helm_formation_pick:
                           intercept?(fn<int(__fastcall *)(Obj,unsigned)>(GA(GA_233CF0))(person,3)?3u:0u):
                           worker?sortie_capture_formation(person):sortie_field_formation(person);
        int capacity=fn<int(__fastcall *)(Obj)>(GA(GA_232C00))(person);
        if(!valid(district) || !district[0x14] || !formation || capacity<(intercept?static_cast<int>(want_now):worker?1000:3000)) {reason=6;continue;}
        // local84: a worker leaves with the asked size (1,000 from the older callers), within budget and capacity.
        unsigned soldiers=(std::min)(budget,(std::min)(worker?(std::max)(want_now,1000u):want_now,static_cast<unsigned>(capacity)));
        unsigned cmd[30];sortie_command(nullptr,target,person,formation,soldiers,cmd);
        if(worker) {cmd[13]=1;cmd[14]=9;cmd[15]=hex;}
        if(retake || intercept) {   // local86: the way around hostile armies decides the side the unit comes out on
            std::vector<unsigned> way;unsigned points[5];
            const unsigned castle=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(source),own=campaign_city_owner(source);
            // local269: a 조타 ship's departure way prices land kHelmLandMul times (its first relays were on land: 27259, 28350)
            if(!retake_path(own,castle,hex,way,avoid_area,helm?kHelmLandMul:1u)) {reason=3;return nullptr;}
            const unsigned count=campaign_relay_points(way,static_cast<unsigned>(way.size()-1),points);
            for(unsigned i=0;i<count;++i)cmd[8+i]=points[i];
        }
        Obj land=table(0x77428,cmd[3],20),water=table(0x77428,cmd[4],20);
        if(!valid(land) || !valid(water)) {reason=6;continue;}
        auto money=*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(source,0)+0x90);
        if(money(source)<word(land,0x9A)+word(water,0x9A)) {reason=4;continue;}
        unsigned free_armies=0,free_groups=0;
        for(unsigned i=1;i<=500;++i) {if(!valid(table(0x7E838,i,500)))++free_armies;if(!valid(table(0x7F8D8,i,500)))++free_groups;}
        if(!free_armies || !free_groups) {reason=5;return nullptr;}
        Obj army=sortie_issue(cmd,1);if(!valid(army)) {reason=7;continue;}
        Obj group=table(0x7F8D8,word(army,0x58),500);
        if(valid(group)) { *reinterpret_cast<unsigned short *>(group+0x14)=10;
            *reinterpret_cast<unsigned short *>(group+0x16)=word(target,0x4E);}
        Progress *p=observe_capture(army);if(!p)return army;
        if(worker) {
            p->siege_city=0;p->campaign=false;p->regional_worker=true;p->work_source=word(source,0x4E);
            p->region_target=static_cast<unsigned short>(hex);
            // local85: a retake unit keeps this base and takes the way around hostile armies (retake_route_order).
            // local87: the departure command already carries the move order and the relay points; nothing is rewritten
            // here, so the first hex outside the castle that the original departure stored (+0x48) stays.
            if(retake) {
                p->retake=true;p->retake_day=game_day();
                Obj goal_tile=table(0xE270,hex,48399);p->region_owner=valid(goal_tile)?goal_tile[0x14]:0;
                *reinterpret_cast<unsigned *>(army+0x1F0)|=2;
                log_event("retake_first_step",army_id(army),word(army,0x48),word(army,0x2C));   // value = first hex outside, hex = first relay
            }
            else if(intercept) {
                p->intercept=true;p->retake_day=game_day();p->intercept_seen_day=game_day();
                Obj goal_tile=table(0xE270,hex,48399);p->region_owner=valid(goal_tile)?goal_tile[0x14]:0;
                *reinterpret_cast<unsigned *>(army+0x1F0)|=2;
                log_event("intercept_first_step",army_id(army),word(army,0x48),word(army,0x2C));
            }
            else campaign_assign_safe(army,*p,hex,true);
            log_event("home_capture_worker_departed",army_id(army),soldiers,hex);
        } else {
            p->campaign=true;p->city=word(target,0x4E);p->siege_city=p->city;
            offensive_order(army,p->city,true);fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(army);
            log_event("weak_city_single_departed",army_id(army),word(army,0x16),p->city);
        }
        return army;
    }
    return nullptr;
}
static bool campaign_city_connected(Obj source,Obj target,unsigned force) { COST_SCOPE("campaign_departures:campaign_city_connected");
    unsigned from=campaign_area_id(fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(source));
    unsigned to=campaign_area_id(fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(target));
    unsigned enemy=campaign_city_owner(target);if(!from || !to)return false;
    bool seen[501]={};std::vector<unsigned> queue={from};seen[from]=true;
    for(size_t k=0;k<queue.size();++k) {
        if(queue[k]==to)return true;Obj a=table(0x6CAF0,queue[k],500);if(!valid(a))continue;
        for(unsigned i=0;i<8;++i) {
            unsigned n=word(a,0x56+i*2);if(!(a[0x66]&(1u<<i)) || !n || n>500 || seen[n])continue;
            Obj b=table(0x6CAF0,n,500);if(!valid(b))continue;Obj t=table(0xE270,word(b,0x3A),48399);
            if(!valid(t) || (t[0x14] && t[0x14]!=force && t[0x14]!=enemy))continue;
            seen[n]=true;queue.push_back(n);
        }
    }
    return false;
}
// mission_v2 (user decision 2026-10-21, option A): a city holding 90%+ of its maximum garrison and
// 40,000+ spare soldiers sends the mod's front/middle/rear siege formation to the nearest hostile
// city linked by region adjacency. Everything else stays with the original departure judgment.
#ifndef NO_RUNTIME_LOG
// Diagnostic build only, read-only: every 30 days, each owned city's non-owned neighbour regions with
// the facts the original corps occupation filter uses (corps tree N201/N2218-N2228): neighbour owner,
// port (232BF0) / base core (232680), and its FIRST neighbour's port/core status and ownership.
// value = area | owner<<9 | port<<15 | core<<16 | first_special<<17 | first_neutral<<18; hex = centre.
static void campaign_frontier_diagnostic() { COST_SCOPE("campaign_departures:campaign_frontier_diagnostic");
    static volatile LONG last_day=-1000;int day=game_day();
    if(!diagnostic_logging || day-last_day<30 || day<last_day+0)return;
    if(InterlockedExchange(&last_day,day)==day)return;
    auto owner_of=[](Obj area)->unsigned {Obj t=valid(area)?table(0xE270,word(area,0x3A),48399):nullptr;return valid(t)?t[0x14]:255;};
    for(unsigned id=1;id<=500;++id) {
        Obj a=table(0x6CAF0,id,500);if(!valid(a))continue;
        unsigned own=owner_of(a);if(!own || own>51)continue;
        for(unsigned e=0;e<8;++e) {
            unsigned n=word(a,0x56+e*2);if(!(a[0x66]&(1u<<e)) || !n || n>500)continue;
            Obj b=table(0x6CAF0,n,500);if(!valid(b))continue;
            unsigned other=owner_of(b);if(other==own)continue;
            unsigned port=fn<int(__fastcall *)(Obj)>(GA(GA_234590))(b)?1:0,core=fn<int(__fastcall *)(Obj)>(GA(GA_234020))(b)?1:0;
            unsigned special=0,neutral=0;
            for(unsigned f=0;f<8;++f) {unsigned m=word(b,0x56+f*2);if(!(b[0x66]&(1u<<f)) || !m || m>500)continue;
                Obj c=table(0x6CAF0,m,500);if(!valid(c))break;
                special=(fn<int(__fastcall *)(Obj)>(GA(GA_234590))(c) || fn<int(__fastcall *)(Obj)>(GA(GA_234020))(c))?1:0;
                neutral=owner_of(c)==0?1:0;break;}
            log_event("frontier_area",a[0x35],n|(other<<9)|(port<<15)|(core<<16)|(special<<17)|(neutral<<18),word(b,0x3A));
        }
    }
}
#else
static void campaign_frontier_diagnostic() {}
#endif
static void campaign_saturated_departures(Obj actor) {
    COST_SCOPE("campaign_saturated_departures");
    if(!mod_features.mission_v2 || !mod_features.sortie_formation)return;
    unsigned force=army_force(actor);int day=game_day();if(!force || force>51 || !campaign_controlled(actor))return;
    auto &audit=campaign_departure_force[force];
    if(audit.root==world() && day>=audit.day && day-audit.day<10)return;
    audit={world(),day};
    for(unsigned id=1;id<=51;++id) {
        Obj source=campaign_goal(id);if(!source || campaign_city_owner(source)!=force)continue;
        unsigned stock=dword(source,0x3C);
        int maximum=(*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(source,0)+200))(source);
        if(maximum<=0 || static_cast<uint64_t>(stock)*10<static_cast<uint64_t>(maximum)*9 ||
           campaign::available(stock)<sortie_split_threshold)continue;
        auto &used=campaign_departure_city[id];if(used.root==world() && day>=used.day && day-used.day<30)continue;
        Obj target=nullptr;int best=INT32_MAX;unsigned source_hex=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(source);
        for(unsigned city=1;city<=51;++city) {
            Obj g=campaign_goal(city);if(!g || !hostile_city(actor,city) || tribe_goal(city))continue;
            int range=distance(source_hex,fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(g));
            if(range>60 || !campaign_city_connected(source,g,force))continue;
            uint64_t incoming=0;for(unsigned i=1;i<=500;++i) {
                Obj a=table(0x7E838,i,500);if(!valid(a) || army_force(a)!=force || a[0x37]==11 || a[0x37]==12)continue;
                Obj group=table(0x7F8D8,word(a,0x58),500);
                if(valid(group) && attack_mode(word(group,0x14)) && word(group,0x16)==city)incoming+=word(a,0x16);
            }
            if(incoming>=(std::max)(3000u,dword(g,0x3C)*3/2))continue;
            if(range<best) {best=range;target=g;}
        }
        if(!target)continue;
        used={world(),day};
        bool sent=sortie_departure_from(nullptr,target,{source},campaign::available(stock));
        log_event(sent?"saturated_city_departed":"saturated_city_unavailable",id,stock,word(target,0x4E));
    }
}
static void campaign_proactive_departures(Obj actor) {
    COST_SCOPE("campaign_proactive_departures");
    if(!mod_features.departures)return;   // [combat] departures
    unsigned force=army_force(actor);int day=game_day();if(!force || force>51 || !campaign_controlled(actor))return;
    auto &audit=campaign_departure_force[force];
    if(audit.root==world() && day>=audit.day && day-audit.day<10)return;
    audit={world(),day};
    for(unsigned id=1;id<=51;++id) {
        Obj source=campaign_goal(id);if(!source || campaign_city_owner(source)!=force || campaign::available(dword(source,0x3C))<1000)continue;
        auto &used=campaign_departure_city[id];if(used.root==world() && day>=used.day && day-used.day<10)continue;
        used={world(),day};
        // Up to two independent 1,000-soldier workers per city per turn.
        for(unsigned n=0;n<2;++n) {
            auto bases=campaign_home_bases(actor,id);if(bases.empty())break;
            if(!campaign_paid_single(source,source,bases[0],1000,true))break;
        }
        Obj target=nullptr;int score=INT32_MAX;unsigned source_hex=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(source);
        for(unsigned city=1;city<=51;++city) {
            Obj g=campaign_goal(city);if(!g || !hostile_city(actor,city) || tribe_goal(city))continue;
            unsigned budget=campaign::available(dword(source,0x3C));bool weak=dword(g,0x3C)<10000;
            if(budget<(weak?3000u:20000u))continue;
            if(!weak && !campaign::siege_departure(budget,dword(g,0x3C)))continue;
            int range=distance(source_hex,fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(g));
            if(range>60 || !campaign_city_connected(source,g,force))continue;
            uint64_t incoming=0;for(unsigned i=1;i<=500;++i) {
                Obj a=table(0x7E838,i,500);if(!valid(a) || army_force(a)!=force || a[0x37]==11 || a[0x37]==12)continue;
                Obj group=table(0x7F8D8,word(a,0x58),500);
                if(valid(group) && attack_mode(word(group,0x14)) && word(group,0x16)==city)incoming+=word(a,0x16);
            }
            if(incoming>=(std::max)(3000u,dword(g,0x3C)*3/2))continue;
            int value=range+(weak?0:100);if(value<score) {score=value;target=g;}
        }
        if(!target)continue;
        if(campaign::siege_departure(campaign::available(dword(source,0x3C)),dword(target,0x3C)))sortie_departure_from(nullptr,target,{source});
        else if(dword(target,0x3C)<10000)campaign_paid_single(source,target,48400,
            (std::max)(3000u,dword(target,0x3C)*3/2),false);
        else sortie_departure_from(nullptr,target,{source});
    }
}
