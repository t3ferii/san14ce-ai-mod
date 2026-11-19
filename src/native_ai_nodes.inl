#pragma once
// Combat AI: behaviour-tree node wrappers (apply_priority, bt_condition, cut/city/retreat nodes), moved verbatim out of native_adapter.cpp (W6-D).
// army_watch (user 2026-11-10 "장료·전풍이 남피↔북평 왕복", diagnostic build only): armies whose commander is listed in
// [diagnostics] watch_officers=663,713 log their mission state once per day and around each node wrapper whenever the
// call changed it (who changed it: the mod branch or the original). Read only; nothing is written to the game.
#ifndef NO_RUNTIME_LOG
namespace army_watch {
static unsigned officers[16],officer_count;static bool loaded;
static struct {Obj root;int day;unsigned long long state;} last[501];
static void load() {
    if(loaded)return;loaded=true;
    wchar_t path[MAX_PATH],text[256]={};
    if(!mod_ini_path(path,L"기능설정.ini"))return;
    GetPrivateProfileStringW(L"diagnostics",L"watch_officers",L"",text,256,path);
    for(wchar_t *s=text;*s && officer_count<16;) {
        while(*s && (*s<L'0' || *s>L'9'))++s;
        unsigned v=0;bool any=false;while(*s>=L'0' && *s<=L'9') {v=v*10+static_cast<unsigned>(*s-L'0');++s;any=true;}
        if(any && v)officers[officer_count++]=v;
    }
    log_event("watch_officers_loaded",officer_count,officer_count?officers[0]:0,officer_count>1?officers[1]:0);
}
static bool wanted(Obj army) {
    load();if(!valid(army))return false;
    if(AI_TRACE_TRACKED(army))return true;   // ai_trace: build departures are watched too
    if(!officer_count)return false;
    const unsigned c=word(army,0x12);
    for(unsigned i=0;i<officer_count;++i)if(officers[i]==c)return true;
    return false;
}
static unsigned long long state(Obj army) {
    Obj troops=table(0x7F8D8,word(army,0x58),500);
    const unsigned goal=valid(troops)?word(troops,0x16):0,mode=valid(troops)?word(troops,0x14):0;
    return (static_cast<unsigned long long>(army[0x37])<<56)|(static_cast<unsigned long long>(army[0x38])<<48)|
           (static_cast<unsigned long long>(word(army,0x3A))<<32)|(static_cast<unsigned long long>(word(army,0x2C))<<16)|
           (static_cast<unsigned long long>(goal&0xFF)<<8)|(mode&0xFF);
}
static void emit(const char *kind,Obj army,unsigned long long s) {
    log_event(kind,army_id(army),static_cast<unsigned>(s>>32),static_cast<unsigned>(s));
}
// Node entry: one full line per army per day (commander, troops, hex, mod progress), then the mission state.
static unsigned long long before(Obj army,Progress *p) {
    if(!wanted(army))return 0;
    const unsigned id=army_id(army);if(!id || id>500)return 0;
    const unsigned long long s=state(army);const int day=game_day();
    if(last[id].root!=world() || last[id].day!=day) {
        last[id].root=world();last[id].day=day;
        log_event("watch_army",id,word(army,0x12),(static_cast<unsigned>(word(army,0x16))<<16)|word(army,0x2A));
        if(p)log_event("watch_progress",id,p->city,(p->campaign?1u:0)|(p->forced_return?2u:0)|(p->returning?4u:0)|
            (p->regional_worker?8u:0)|(p->operation_leader?16u:0)|(p->combat_target?32u:0)|(p->combat_city?64u:0)|
            (p->home_defense_city?128u:0)|(p->facility?256u:0)|(p->credit?512u:0));
        emit("watch_state",army,s);last[id].state=s;
    } else if(s!=last[id].state) {emit("watch_state_between",army,s);last[id].state=s;}
    return s|1ull<<63|1;   // non-zero marker even for an all-zero state
}
// Node exit: logs only when this call changed the mission; kind names the node and who decided.
static void after(Obj army,unsigned long long marker,const char *kind) {
    if(!marker || !valid(army))return;
    const unsigned id=army_id(army);if(!id || id>500)return;
    const unsigned long long s=state(army);
    if(s!=last[id].state) {emit(kind,army,s);last[id].state=s;}
}
// Troops node (local29f): the work list +0x18 u16[10] and every watched member's five waypoints at
// each step of city_node (pre = before mission_v2, mod = after mission_v2, native = after 69AB0,
// end = after the mod's member pass), so the line where a capture region appears or swaps is visible.
static void troops(Obj troops,const char *tag) {
    load();if(!officer_count || !valid(troops))return;
    static volatile LONG lines;if(lines>8000)return;
    Obj members[16];unsigned count=0,tid=0;
    for(unsigned i=1;i<=500 && count<16;++i) {
        Obj a=table(0x7E838,i,500);
        if(!valid(a) || a[0x140] || table(0x7F8D8,word(a,0x58),500)!=troops || !wanted(a))continue;
        members[count++]=a;tid=word(a,0x58);
    }
    if(!count)return;
    const unsigned short *l=reinterpret_cast<const unsigned short *>(troops+0x18);
    InterlockedAdd(&lines,static_cast<LONG>(4+2*count));
    log_event(tag,tid,(static_cast<unsigned>(word(troops,0x14))<<16)|word(troops,0x16),count);
    log_event("watch_list_a",tid,(static_cast<unsigned>(l[0])<<16)|l[1],(static_cast<unsigned>(l[2])<<16)|l[3]);
    log_event("watch_list_b",tid,(static_cast<unsigned>(l[4])<<16)|l[5],(static_cast<unsigned>(l[6])<<16)|l[7]);
    log_event("watch_list_c",tid,(static_cast<unsigned>(l[8])<<16)|l[9],0);
    for(unsigned m=0;m<count;++m) {
        Obj a=members[m];
        log_event("watch_member_a",army_id(a),(static_cast<unsigned>(word(a,0x2C))<<16)|word(a,0x2E),
            (static_cast<unsigned>(word(a,0x30))<<16)|word(a,0x32));
        log_event("watch_member_b",army_id(a),(static_cast<unsigned>(word(a,0x34))<<16)|word(a,0x2A),
            (static_cast<unsigned>(a[0x37])<<24)|(static_cast<unsigned>(a[0x38])<<16)|word(a,0x3A));
    }
}
}
#define WATCH_BEFORE(army,p) const unsigned long long watch_marker=army_watch::before((army),(p))
#define WATCH_AFTER(army,kind) army_watch::after((army),watch_marker,(kind))
#define WATCH_TROOPS(t,tag) army_watch::troops((t),(tag))
#else
#define WATCH_BEFORE(army,p) ((void)0)
#define WATCH_AFTER(army,kind) ((void)0)
#define WATCH_TROOPS(t,tag) ((void)0)
#endif
static bool apply_priority(Obj army, Obj node, Obj context)
{
    COST_SCOPE("apply_priority");
    if(native_search_depth)return false;
    if (!campaign_controlled(army)) return false;
    Progress *p = observe_capture(army);
    if(p) {
        remember_campaign(army,*p);
        // gate_intercept: the unit behind a gate takes no other order (individual priorities, cuts, builds included).
        if(p->regional_worker && p->intercept && computer_side_army(army)) {intercept_step(army,*p);return mod_step("ap:intercept"),true;}
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
        // local45: a cut the original planned today wins over the group march and siege priorities below.
        if(mod_features.combat_cut && computer_side_army(army) && restore_cut(army,*p))return mod_step("ap:restore_cut"),true;
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
    // U39: the cut is planned only by the original cut node (cut_node below); no second cut writer here.
    if(p && restore_cut(army,*p))return mod_step("ap:restore_cut"),true;
    bool cancelled = p && cancel_wall_order(army,*p);
    if(p && cancel_catapult_order(army,*p))cancelled=true;
    if(p && cancel_unqualified_neutral_order(army,*p))cancelled=true;
    if(p && !p->regional_worker && recover_idle(army,*p))return mod_step("ap:recover_idle"),true;
    if(word(army,0x16)<1000)return cancelled && mod_step("ap:cancel_order");
    // local91 (user 2026-11-12 "수복부대는 건설을 off ... 옵션, 기본값 off, 매우 하드코어"): a retake unit builds only with
    // [combat] retake_build; without it the capture credit and the build chain are dropped and it goes on to the next base.
    const bool no_build=p && p->regional_worker && p->retake && !mod_features.retake_build && !p->facility;
    if(no_build) {p->credit=false;p->build_chain=false;}
    // local92 (user 2026-11-12 "주변에 적 건설물이 있으면 건설물 공격명령이 내려지더라고? 그것도 X"): a retake unit never takes
    // the facility-attack order; it keeps walking to its base.
    const bool retake_unit=p && p->regional_worker && p->retake;
    if(p && !retake_unit && try_destroy(army,*p,true)) {pause_assembly(*p);return mod_step("ap:destroy_priority"),true;}
    if(p && p->facility && continue_build(army,*p)) {pause_assembly(*p);return mod_step("ap:continue_build"),true;}
    if(p && !retake_unit && try_destroy(army,*p)) {pause_assembly(*p);return mod_step("ap:destroy"),true;}
    if(p && !no_build && (p->campaign || p->regional_worker))prepare_local_build(army,*p);
    if (p && !no_build && try_build(army, *p)) {pause_assembly(*p);return mod_step("ap:build"),true;}
    // Build policy v2: a blocked build never parks the army (former "build_hold_wait" stood units
    // on open ground for weeks); the mission or the original AI continues instead.
    if(mod_features.mission_v2) {
        if(p && (p->credit || p->build_chain) && !p->facility) {p->credit=false;p->build_chain=false;}
    } else if(p && (p->credit || p->build_chain)) {
        // Former policy: keep the native AI from replacing a temporarily blocked build with retreat.
        if(army[0x37]!=13)log_event("build_hold_wait",army_id(army),army[0x37],word(army,0x2A));
        fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
        fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,13);
        fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,255,0);
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
// [combat] retake_adjacent (user 2026-11-12, local84: "전투력에 관계없이 1차 인접선은 회복해야 ... 요격군만 전투력을 보면 돼").
// local80-83 relaxed the original area-occupy departure (condition 84 radius, the two hostile-near-castle checks in 410F0 /
// 44B60, the power cancel in 29BA0); it still never left, because that departure is built to stay home near an enemy
// (more gates follow: garrison and resource thresholds, path cost, commander choice). The retake is therefore issued by
// the mod itself, with one rule: a computer castle sends one unit to each first-ring neighbour region a hostile force
// holds, unless a hostile army stands within 2 hexes of that base (left to the original interception), a unit of the
// force already heads there, or a hostile army stands within 2 hexes of the castle (the original defence owns the
// garrison). No strength comparison. The unit is a home-capture worker of the mission flow (campaign_paid_single: group
// kind 10, which no original group branch runs), so the original trees never order it and the interception stays
// entirely the original's. Runs once per force and game day at the first condition of that force's AI thread.
// local85: the unit keeps that base and walks around hostile armies (retake_step in native_campaign.inl).
// local88: the original group tree is not run for a retake group (retake_tree_hook in native_siege_guard.inl) and a
// retake unit lets the original supply-cut node go first (first army node above).
// local86: a first-ring base still held gets the same unit as a guard once a hostile army is within 8 hexes of it; the
// departure command carries the relay points of the way around hostile armies, so the unit comes out on that side.
static bool retake_first_ring_lost(unsigned own,unsigned hex) { COST_SCOPE("ai_nodes:retake_first_ring_lost");
    if(!own || own>51 || hex>=48400)return false;
    Obj tile=table(0xE270,hex,48399);if(!valid(tile))return false;
    const unsigned holder=tile[0x14];
    return holder && holder!=own && fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,holder)!=0;
}
#ifdef SORTIE_FORMATION
static constexpr unsigned kRetakeSoldiers=2000,kRetakeGarrison=6000;
static bool retake_unit_heading(unsigned own,unsigned hex) {
    for(unsigned i=1;i<=500;++i) {
        Obj a=table(0x7E838,i,500);if(!valid(a) || army_force(a)!=own)continue;
        if(a[0x37]==1 && a[0x38]==9 && word(a,0x3A)==hex)return true;
        const Progress &p=progress[i];
        if(p.army==a && p.commander==word(a,0x12) && p.region_target==hex)return true;
    }
    return false;
}
// gate_intercept (native_campaign.inl intercept_step): once per force and day, each gate of the force with a hostile army
// within kInterceptAlert hexes of its center and no intercept unit of its own yet sends one. The gate itself leaves when
// it keeps kInterceptKeep soldiers after the 2,000; otherwise the own castles within kInterceptReach hexes, nearest
// first, that hold kRetakeGarrison. No stand behind the gate (all hexes taken, walled or next to an attacker) = not sent.
static constexpr unsigned kInterceptSoldiers=2000,kInterceptKeep=2000;
static constexpr int kInterceptReach=30;
static unsigned intercept_units_out(unsigned own,unsigned gate) {
    unsigned claims[16];return intercept_claims(own,gate,nullptr,claims,16);
}
// local179 (user 2026-11-15): one unit per attack stand (free hex across the gate within 안행 reach of an attacker,
// intercept::ranked; 동관 3, 양평관 1, 검각 1 on the original map), up to kInterceptUnits; the first unit goes as before,
// each further one only to a still unheld attack stand.
static void gate_intercept_tick(unsigned own) { COST_SCOPE("ai_nodes:gate_intercept_tick");
    for(unsigned gid=52;gid<=61;++gid) {
        Obj gate=intercept_gate_object(gid);if(!gate || intercept_base_owner(gate)!=own)continue;
        const unsigned center=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(gate);if(center>=48400)continue;
        unsigned attackers[1];
        if(!intercept_attackers(own,center,kInterceptAlert,attackers,1))continue;
        unsigned front=0;intercept_stands(own,center,nullptr,nullptr,0,&front);
        const unsigned wanted=front<1?1u:(std::min)(front,kInterceptUnits);
      for(unsigned out=intercept_units_out(own,gid);out<wanted;++out) {
        bool front_free=false;
        const unsigned stand=intercept_free_stand(own,center,gid,nullptr,&front_free);
        if(out && (stand>=48400 || !front_free))break;   // extra units only for the equally best stands
        if(stand>=48400) {log_event("intercept_no_stand",gid,attackers[0],center);break;}
        std::vector<std::pair<int,Obj>> sources;
        if(dword(gate,0x3C)>=kInterceptSoldiers+kInterceptKeep)sources.push_back({-1,gate});
        for(unsigned id=1;id<=51;++id) {
            Obj city=campaign_goal(id);if(!city || intercept_base_owner(city)!=own || dword(city,0x3C)<kRetakeGarrison)continue;
            const int range=distance(fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(city),center);
            if(range<=kInterceptReach)sources.push_back({range,city});
        }
        std::sort(sources.begin(),sources.end(),[](const std::pair<int,Obj> &a,const std::pair<int,Obj> &b){return a.first<b.first;});
        Obj army=nullptr;Obj from=nullptr;
        for(const auto &s:sources) {
            army=campaign_paid_single(s.second,s.second,stand,kInterceptSoldiers,true,false,0,true);
            if(valid(army)) {from=s.second;break;}
        }
        Progress *p=valid(army)?observe_capture(army):nullptr;
        if(p) {p->intercept_gate=static_cast<unsigned short>(gid);p->intercept_seen_day=game_day();}
        // value = army | source base << 16 (52..61 = the gate itself), hex = stand
        log_event(p?"intercept_departed":"intercept_failed",gid,p?(army_id(army)|(static_cast<unsigned>(word(from,0x4E))<<16)):static_cast<unsigned>(sources.size()),stand);
        if(!p)break;
        if(out)log_event("intercept_extra_unit",gid,out+1,front);   // local179: unit number, equally best stands
      }
    }
}
// helm_intercept (native_campaign.inl helm_step): once per force and day. For each of the six river castles (여강 13 since local172, 건업 25,
// 시상 28, 강하 32, 장사 36, 무릉 37) held by the force while its ship tier (force +0x15E, KB structs 3.9) is 몽충 (12) or
// better: every port of that castle (a region whose city is the castle, area +0x35, and whose base hex is a port, terrain 18,
// still held) is watched by its occupancy (area +0x68 owned tiles); when it falls since the day before, one 조타 ship sails
// out from the castle to paint that port region back, however far it is (local149, user 2026-11-14 "성 주변 몇칸 적군부대
// 몇칸 이러면 안돼 ... 소속된 항구의 점령률을 체크하고 있다가 떨어지면 거리 상관없이 출진해서 색칠"), unless one is out for it.
// local169: 강릉 (35) added (user 2026-11-15 "시상이나 강하 강릉에 있는 장수들도").
static constexpr unsigned kHelmCities[7]={13,25,28,32,35,36,37};   // local184: 여강 13 (17 was 여남)
static constexpr unsigned kHelmSoldiers=3000;
static bool helm_unit_out(unsigned own,unsigned area) {
    for(unsigned i=1;i<=500;++i) {
        Obj a=table(0x7E838,i,500);if(!valid(a) || army_force(a)!=own)continue;
        const Progress &p=progress[i];
        if(p.army==a && p.root==world() && p.commander==word(a,0x12) && p.helm && p.helm_area==area && !p.returning)return true;
    }
    return false;
}
static bool helm_ring_out(unsigned own,unsigned city_id) {
    for(unsigned i=1;i<=500;++i) {
        Obj a=table(0x7E838,i,500);if(!valid(a) || army_force(a)!=own)continue;
        const Progress &p=progress[i];
        if(p.army==a && p.root==world() && p.commander==word(a,0x12) && p.helm && p.helm_ring==city_id && !p.returning)return true;
    }
    return false;
}
// local184 (user 2026-11-15 "B안으로 가주고"): when the castle of a port / river region has no idle 조타 officer (why 2), the other
// river castles of the same force (same garrison and ship checks) sail instead, nearest to the goal first, up to three tries -
// (local185: for 시상 only, below). from = the castle that sailed; why = the last reason.
// local185 (user "조타 지원은 시상만 지원을 받을수 있게하자 ... 건업에서 시상 지원하는거지 시상에서 건업은 안가고"): only 시상 (28)
// receives this support; any other castle's why 2 stays a failure, so 시상 never sails for another castle.
static constexpr unsigned kHelmSupported=28;
static Obj helm_neighbour(unsigned own,unsigned skip,unsigned goal,unsigned &why,unsigned &from) {
    if(skip!=kHelmSupported)return nullptr;
    std::pair<int,unsigned> cand[7];unsigned n=0;
    for(unsigned id:kHelmCities) {
        if(id==skip)continue;
        Obj city=campaign_goal(id);if(!city || intercept_base_owner(city)!=own || dword(city,0x3C)<kRetakeGarrison)continue;
        const unsigned castle=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(city);if(castle>=48400)continue;
        cand[n++]={distance(castle,goal),id};
    }
    std::sort(cand,cand+n);
    for(unsigned i=0;i<n && i<3;++i) {
        Obj city=campaign_goal(cand[i].second);unsigned w=0;
        Obj army=campaign_paid_single(city,city,goal,kHelmSoldiers,true,false,0,true,true,&w);
        if(valid(army)) {from=cand[i].second;return army;}
        why=w;
    }
    return nullptr;
}
// local169: the river ring (helm_ring_of: 대하·격류 within max(15, nearest + 10) hexes of the castle); when the force holds
// fewer of its hexes than the day before and some are lost to a hostile force, one 조타 ship sails out for the region with
// the most lost ring hexes and keeps painting ring regions until none is lost (helm_step).
static void helm_ring_tick(unsigned own,unsigned id,Obj city,unsigned castle,int day) {
    static Obj seen_root;static unsigned short held_was[52];static int held_day[52];
    if(seen_root!=world()) {seen_root=world();memset(held_was,0,sizeof(held_was));for(int &d:held_day)d=-1;}
    if(id>=52)return;
    unsigned held=0,ring_area=0;const unsigned lost=helm_ring_lost(own,id,&held,&ring_area);
    const bool fell=held_day[id]>=0 && held_day[id]!=day && held<held_was[id];
    if(held_day[id]!=day) {held_was[id]=static_cast<unsigned short>((std::min)(held,65535u));held_day[id]=day;}
    if(!fell || !lost || !ring_area || helm_ring_out(own,id))return;
    const unsigned goal=helm_pick(own,ring_area,castle,nullptr);
    if(goal>=48400) {log_event("helm_ring_no_target",id,lost,ring_area);return;}
    unsigned why=0,from=id;Obj army=campaign_paid_single(city,city,goal,kHelmSoldiers,true,false,0,true,true,&why);
    if(!valid(army) && why==2 && (army=helm_neighbour(own,id,goal,why,from))!=nullptr)
        log_event("helm_neighbour",ring_area,from|(id<<8)|(1u<<16),goal);   // value = castle that sailed | owner castle << 8 | ring << 16
    Progress *p=valid(army)?observe_capture(army):nullptr;
    if(p) {p->helm=true;p->helm_area=static_cast<unsigned short>(ring_area);p->helm_ring=static_cast<unsigned char>(id);p->helm_stuck=0;
           p->intercept_gate=0;p->intercept_seen_day=day;p->helm_pick_day=-1;}
    // departed: army = ring area, value = army | castle << 16; failed: value = castle | reason << 8; hex = goal
    log_event(p?"helm_ring_departed":"helm_ring_failed",ring_area,p?(army_id(army)|(id<<16)):(id|(why<<8)),goal);
}
static void helm_intercept_tick(unsigned own) { COST_SCOPE("ai_nodes:helm_intercept_tick");
    static Obj seen_root;static unsigned short last[501];static int last_day[501];
    if(seen_root!=world()) {seen_root=world();memset(last,0,sizeof(last));for(int &d:last_day)d=-1;}
    Obj force=pointer(world(),0xDF30+(own<0x34?own:0u)*8);
    if(!valid(force) || force[0x15E]<12 || force[0x15E]>13)return;
    const int day=game_day();
    for(unsigned id:kHelmCities) {
        Obj city=campaign_goal(id);if(!city || intercept_base_owner(city)!=own || dword(city,0x3C)<kRetakeGarrison)continue;
        const unsigned castle=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(city);if(castle>=48400)continue;
        helm_ring_tick(own,id,city,castle,day);
        for(unsigned n=1;n<=500;++n) {
            // local169: the castle's river regions too (helm_watch_area), not only its ports
            Obj area=table(0x6CAF0,n,500);if(!valid(area) || area[0x35]!=id || !helm_watch_area(own,id,n))continue;
            const unsigned base=word(area,0x3A);
            const unsigned now=word(area,0x68);
            const bool fell=last_day[n]>=0 && last_day[n]!=day && now<last[n];
            if(last_day[n]!=day) {last[n]=static_cast<unsigned short>(now);last_day[n]=day;}
            if(!fell || helm_unit_out(own,n))continue;
            unsigned goal=helm_pick(own,n,castle,nullptr);
            if(goal>=48400) {log_event("helm_no_target",n,now,base);continue;}
            // local166 (user 2026-11-15 "조타 실패22건은 뭐지 ... 둘다 고쳐"): half the failures were a way blocked by the attackers
            // (retake_path refuses; the same goal sailed fine days later). Then try up to two more goals, each picked again
            // with the failed ones (and 3 hexes around them) left out; any other reason (no idle 조타 officer, soldiers, gold)
            // does not change with the goal and stops at once.
            unsigned why=0,tried[3]={goal},ntried=1;Obj army=nullptr;
            for(;;) {
                army=campaign_paid_single(city,city,goal,kHelmSoldiers,true,false,0,true,true,&why);
                if(valid(army) || why!=3 || ntried>=3)break;
                const unsigned next=helm_pick(own,n,castle,nullptr,nullptr,nullptr,kHelmSweep,tried,ntried);
                if(next>=48400)break;
                log_event("helm_goal_retry",n,goal,next);
                goal=next;tried[ntried++]=next;
            }
            unsigned from=id;
            if(!valid(army) && why==2 && (army=helm_neighbour(own,id,goal,why,from))!=nullptr)
                log_event("helm_neighbour",n,from|(id<<8),goal);   // value = castle that sailed | owner castle << 8
            Progress *p=valid(army)?observe_capture(army):nullptr;
            if(p) {p->helm=true;p->helm_area=static_cast<unsigned short>(n);p->intercept_gate=0;p->intercept_seen_day=day;p->helm_pick_day=-1;}
            // departed: value = army | castle << 16; failed: value = castle | reason << 8 | goals tried << 16; hex = goal
            log_event(p?"helm_departed":"helm_failed",n,p?(army_id(army)|(from<<16)):(id|(why<<8)|(ntried<<16)),goal);
        }
    }
}
// garrison_recall (local148, user 2026-11-14 "성에서 수비 요격군이나 수복군이 나갔어. 그런데 우리 성이 공격받는중이야 내구도보다 병사가
// 작으면 주변 가까운부대를 퇴각시켜서 내구도보다 병사가 많게 유지 ... 원작 요격부대에도"): once per force and day, a castle of the
// force with a hostile army within 3 hexes whose soldiers (+0x3C) are fewer than its durability (its building, facility table
// id = castle id, +0x14) calls home its own armies from that castle - mod units (retake / intercept / 조타) and original
// ones (interceptions included) alike - nearest first within kRecallReach hexes, until the soldiers on their way home
// lift it above the durability. A called army takes the mod's forced return (recovery_return, kept by
// campaign_individual_priority), so no original order turns it round.
static constexpr int kRecallReach=20;
static void garrison_recall_tick(unsigned own) { COST_SCOPE("ai_nodes:garrison_recall_tick");
    for(unsigned id=1;id<=51;++id) {
        Obj city=campaign_goal(id);if(!city || intercept_base_owner(city)!=own)continue;
        const unsigned castle=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(city);if(castle>=48400)continue;
        Obj building=table(0x6DA98,id,3000);
        if(!valid(building) || word(building,0x10)!=castle)continue;
        const unsigned durability=word(building,0x14),soldiers=dword(city,0x3C);
        if(!durability || soldiers>durability)continue;
        unsigned foes[1];if(!intercept_attackers(own,castle,3,foes,1))continue;
        unsigned coming=0;std::vector<std::pair<int,Obj>> out;
        for(unsigned i=1;i<=500;++i) {
            Obj a=table(0x7E838,i,500);if(!valid(a) || army_force(a)!=own || !computer_side_army(a))continue;
            Obj origin=fn<Obj(__fastcall *)(Obj)>(GA(GA_22BFE0))(a);if(origin!=city)continue;
            const unsigned at=word(a,0x2A);if(at>=48400 || at==castle)continue;
            const Progress &q=progress[i];
            if((q.army==a && q.root==world() && (q.forced_return || q.returning)) || a[0x37]==11) {coming+=word(a,0x16);continue;}
            const int range=distance(at,castle);if(range<=kRecallReach)out.push_back({range,a});
        }
        std::sort(out.begin(),out.end(),[](const std::pair<int,Obj> &x,const std::pair<int,Obj> &y){return x.first<y.first;});
        for(const auto &e:out) {
            if(soldiers+coming>durability)break;
            Progress *p=observe_capture(e.second);if(!p)continue;
            p->forced_return=true;p->guard_beneficiary=0;p->combat_target=0;p->combat_city=0;
            if(!recovery_return(e.second,*p)) {p->forced_return=false;continue;}
            coming+=word(e.second,0x16);
            // value = army | soldiers << 16 (capped), hex = castle; third = garrison | durability << 16
            log_event("garrison_recall",army_id(e.second)|((std::min)(static_cast<unsigned>(word(e.second,0x16)),65535u)<<16),castle,(soldiers&0xFFFF)|(durability<<16));
        }
    }
}
namespace trade_hire {static void trade_tick(unsigned own);}
static void retake_adjacent_tick(Obj context) { COST_SCOPE("ai_nodes:retake_adjacent_tick");
    static thread_local int seen_day=-1;
    const int day=game_day();
    if(seen_day==day)return;
    seen_day=day;
    // local167: the monthly computer trade rides on the same per-force daily tick, independent of mission_v2.
    const bool v2=(mod_features.retake_adjacent || mod_features.gate_intercept || mod_features.helm_intercept || mod_features.garrison_recall) && mod_features.mission_v2;
    if((!v2 && !mod_features.computer_trade) || !enabled || in_policy)return;
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context),corps=nullptr;
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(blackboard,0xD6A5EAAEu,&corps,0) || !valid(corps))
        {seen_day=-1;return;}   // not a corps tree yet: try again at a later condition of this thread
    if(fn<int(__fastcall *)(Obj)>(GA(GA_2350F0))(corps))return;   // corps of the player force
    const unsigned own=corps[0x10];
    if(!own || own>51 || own==player_force())return;
    static Obj tick_root;static int tick_day[52];
    if(tick_root!=world()) {tick_root=world();for(int &d:tick_day)d=-1;}
    if(tick_day[own]==day)return;
    tick_day[own]=day;
    ++in_policy;
    AcquireSRWLockExclusive(&policy_lock);
    if(mod_features.computer_trade)trade_hire::trade_tick(own);
    if(v2 && mod_features.gate_intercept)gate_intercept_tick(own);
    if(v2 && mod_features.helm_intercept)helm_intercept_tick(own);
    if(v2 && mod_features.garrison_recall)garrison_recall_tick(own);
    for(unsigned id=1;v2 && mod_features.retake_adjacent && id<=51;++id) {
        Obj city=campaign_goal(id);if(!city)continue;
        Obj lord=fn<Obj(__fastcall *)(Obj)>(GA(GA_22E3E0))(city);
        if(!valid(lord) || (*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(lord,0)+0x60))(lord)!=own)continue;
        const unsigned castle=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(city);
        Obj home=table(0x6CAF0,campaign_area_id(castle),500);if(castle>=48400 || !valid(home))continue;
        bool reviewed=false;
        const RetakeRings rings=retake_rings(own,id);   // local89: the largest threat is left out and walked around
        for(unsigned i=0;i<8;++i) {
            const unsigned n=word(home,0x56+i*2);if(!(home[0x66]&(1u<<i)) || !n || n>500)continue;
            Obj area=table(0x6CAF0,n,500);if(!valid(area))continue;
            const unsigned base=word(area,0x3A);
            if(campaign_transit_region(n))continue;   // a port or another castle is not a base to walk onto
            // local86: a base still ours gets its unit before it falls, once a hostile army is within kRetakeThreat hexes.
            const bool lost=retake_first_ring_lost(own,base);
            Obj kept=base<48400?table(0xE270,base,48399):nullptr;
            const bool threatened=!lost && valid(kept) && kept[0x14]==own && retake_hostile_near(own,base,kRetakeThreat);
            if(!lost && !threatened)continue;
            if(n==rings.threat) {log_event("retake_max_threat",id,n,base);continue;}
            if(!reviewed) {
                reviewed=true;
                if(retake_hostile_near(own,castle,2)) {log_event("retake_castle_attacked",id,0,castle);break;}
            }
            if(retake_hostile_near(own,base,2)) {log_event("retake_left_to_interception",id,0,base);continue;}
            if(retake_unit_heading(own,base)) {log_event("retake_already_sent",id,0,base);continue;}
            // local92 (user 2026-11-12 "성에 남은 병력 6000명 미만이면 출진 X"): no unit leaves a castle holding fewer than 6,000.
            if(dword(city,0x3C)<kRetakeGarrison || campaign::available(dword(city,0x3C))<1000) {log_event("retake_no_spare",id,dword(city,0x3C),base);break;}
            // local85: no way that stays clear of hostile armies = not sent (asked again next turn).
            std::vector<unsigned> way;
            if(!retake_path(own,castle,base,way,rings.threat)) {log_event("retake_no_safe_way",id,0,base);continue;}
            // local88: the units ran into the player's armies because the original group tree re-purposed their group
            // (retake_tree_hook), not because they left; the "reaches the base first" rule of local87 is gone, so a unit
            // is out before the base falls and can cut the supply of the armies that come for it.
            Obj army=campaign_paid_single(city,city,base,kRetakeSoldiers,true,true,rings.threat);
            log_event(valid(army)?(lost?"retake_departed":"retake_guard_departed"):"retake_failed",id,valid(army)?army_id(army):0,base);
        }
    }
    ReleaseSRWLockExclusive(&policy_lock);
    --in_policy;
}
#else
static void retake_adjacent_tick(Obj) {}
#endif
// [combat] computer_trade / front_hire (local167, user 2026-11-15 "컴퓨터군단 교역을 왜 안가지 .. 통솔 50미만중에 정치 높거나 특사개성
// 있는 애들 교역 보내게 해줘. 우호도 1000이면 1명만 보내고 / 고용무장 ... 전선도시에 응원 악주 조타 모집 개성만 고용하게 해줘").
// Trade: the original tree (CCommandExecutionTradeNode 54CE0) only trades from a capital with gold >= 9000, food >= 30000,
// soldiers >= 6000, an empty caution list and 8+ city officers, so computer forces almost never do. Here, once a month in
// 상순 (the first day of 상순 that works), a computer force sends a trade (command 0x30 bytes: person list, +0x10 trade nation
// index 22F740, +0x20 gold, +0x24 food, +0x28 soldiers; issued by 1EBC60 as 54CE0 does) to the trade nation (교역국, table 36:
// 대진국·안식 etc., world +0x85A20, opened through a region benefit = 2C8530(force,2,index), not trading now = 711BE0) it is
// least friendly with (6E01F0 candidates, 우호도 force +0x184 + index*2, 0..1000): officers idle in the capital, not the lord or a corps
// governor, 통솔 < 50, 특사 (185) first, then 정치 descending; three of them, one when the intimacy is already 1000.
// The original trade node, when it fires, issues the same pick for its own target instead of its officer list.
// Hire: CCommandExecutionHireNode (2DAB0) hires every person of the list in blackboard var node+0x3C for the city in var
// node+0x38. In a frontline city (a region borders a hostile regular force) of a computer force, a listed person without
// 응원 96 / 악주 23 / 조타 154 / 모집 117 is replaced by the force's hire pool (force +0x250) holder of one of them with the
// highest 통솔; none = no hire this time (the pool's traits change every 상순).
namespace trade_hire {
static constexpr unsigned kTradeGold=5000,kTradeSoldiers=2000,kMinGold=8000,kMinSoldiers=5000,kFoodSpare=10000;
static constexpr int kLeadBelow=50,kLeadMax=70;   // local171: 통솔 below 70 may go, below 50 first
static constexpr unsigned kEnvoy=185,kMaxOfficers=3,kIntimacyMax=1000;
static constexpr unsigned kFrontTraits[4]={96,23,154,117};
struct TradeCommand {Obj vtable;unsigned *handle;unsigned tribe,pad0;unsigned long long zero;unsigned gold,food,soldiers,pad1;};
static_assert(sizeof(TradeCommand)==0x30,"trade command layout (54CE0 locals)");
static bool has_trait(Obj p,unsigned t) {
    for(unsigned s=0;s<9;++s)if(fn<unsigned(__fastcall *)(Obj,unsigned)>(GA(GA_232030))(p,s)==t)return true;
    return false;
}
static int stat(Obj p,unsigned i) {return fn<int(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_231770))(p,i,0xFFF);}
static Obj force_obj(unsigned own) {return own && own<0x34?pointer(world(),0xDF30+own*8):nullptr;}
static unsigned city_force(Obj city) {
    Obj f=valid(city)?fn<Obj(__fastcall *)(Obj)>(GA(GA_22E3E0))(city):nullptr;
    return valid(f)?(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(f,0)+0x60))(f):0;
}
static bool computer_force(unsigned own) {return own && own<=45 && own!=player_force();}
static bool idle_in(Obj p,unsigned city) {
    if(word(p,0xE2)!=city || word(p,0xE4)!=city || (word(p,0x15E)&0x4001))return false;
    if(fn<int(__fastcall *)(Obj)>(GA(GA_233BE0))(p)!=0 || fn<int(__fastcall *)(Obj,int)>(GA(GA_234AB0))(p,0)!=0)return false;
    if(valid(fn<Obj(__fastcall *)(Obj)>(GA(GA_22C4B0))(p)))return false;           // already on a job
    return !valid(fn<Obj(__fastcall *)(Obj,int)>(GA(GA_22BAB0))(p,1));            // not commanding an army
}
static bool governor(unsigned own,Obj p) {
    for(unsigned i=0;i<0x34;++i) {
        Obj c=table(0xE0D0,i,51);
        if(valid(c) && c[0x10]==own && word(c,0x12)==word(p,0x10))return true;
    }
    return false;
}
static bool front_city(unsigned id,unsigned own);
// Trade officers from every non-front city of the force (user 2026-11-15): idle in their own city, 통솔 below kLeadBelow,
// not the lord or a governor; 특사 first, then 정치 descending. The original 1F5870 departs from the first officer's location
// (+0xE2) and takes gold/food/soldiers from the force capital. local171 (user 2026-11-15 "교역무장은 같이 보낼 무장이 꼭 같은
// 도시에 없어도 보낼수있어(원작)"): companions come from any city; 통솔 below kLeadMax qualifies, below kLeadBelow sorted first.
static std::vector<Obj> trade_officers(unsigned own,Obj force,unsigned count) {
    struct Pick {Obj p;bool envoy;bool low;int politics;unsigned city;};
    std::vector<Pick> picks;
    signed char front[0x34];for(auto &f:front)f=-1;
    Obj lord=table(0x158,word(force,0x10),6000);   // ruler +0x10 (structs.md FORCE); +0x52 = 군사, read until local257
    for(unsigned id=1;id<=6000;++id) {
        Obj p=table(0x158,id,6000);
        if(!valid(p) || p==lord || fn<unsigned(__fastcall *)(Obj)>(GA(GA_22E870))(p)!=own)continue;
        const unsigned city=word(p,0xE2);
        if(!city || city>=0x34 || !idle_in(p,city) || city_force(table(0xE0D0,city,51))!=own)continue;
        const int lead=stat(p,0);
        if(lead>=kLeadMax || governor(own,p))continue;
        if(front[city]<0)front[city]=front_city(city,own)?1:0;
        if(front[city])continue;
        picks.push_back({p,has_trait(p,kEnvoy),lead<kLeadBelow,stat(p,3),city});
    }
    std::stable_sort(picks.begin(),picks.end(),[](const Pick &a,const Pick &b){return a.envoy!=b.envoy?a.envoy:a.low!=b.low?a.low:a.politics>b.politics;});
    std::vector<Obj> out;
    for(const Pick &k:picks) {if(out.size()>=count)break;out.push_back(k.p);}
    return out;
}
// One trade for force `own` to `want` (null = the least friendly candidate). Returns true once the command reached 1EBC60.
static bool issue(unsigned own,Obj want,bool log_fail) { COST_SCOPE("trade_hire:issue");
    auto fail=[&](unsigned why,unsigned detail) {if(log_fail)log_event("trade_skipped",own,why,detail);return false;};
    Obj force=force_obj(own);if(!valid(force))return false;
    Obj capital=fn<Obj(__fastcall *)(Obj)>(GA(GA_230350))(force);
    Obj corps=fn<Obj(__fastcall *)(Obj)>(GA(GA_230270))(force);
    if(!valid(capital) || !valid(corps))return fail(1,0);
    unsigned code=0;
    if(!fn<unsigned char(__fastcall *)(Obj,unsigned *,unsigned)>(GA(GA_70F210))(corps,&code,0))return fail(2,code);
    GameList candidates;list_init(candidates,GA(GA_134F6D8));
    if(!candidates.handle)return fail(3,0);
    fn<int(__fastcall *)(Obj,GameList *,int)>(GA(GA_6E01F0))(corps,&candidates,0);
    Obj target=nullptr;unsigned tribe=0,intimacy=~0u;
    for(Link *l=list_first(candidates);l;l=l->next) {
        Obj c=l->value;if(!valid(c) || (want && c!=want))continue;
        const unsigned idx=static_cast<unsigned>(fn<long long(__fastcall *)(Obj)>(GA(GA_22F740))(c));
        if(!idx || idx>4)continue;
        const unsigned v=word(force,0x184+idx*2);
        if(v<intimacy) {intimacy=v;target=c;tribe=idx;}
    }
    list_free(candidates);
    if(!target)return fail(4,0);
    const std::vector<Obj> officers=trade_officers(own,force,intimacy>=kIntimacyMax?1u:kMaxOfficers);
    if(officers.empty())return fail(5,0);
    GameList copy;list_init(copy,GA(GA_13466B0));
    if(!copy.handle)return fail(3,1);
    for(Obj p:officers)list_add(copy,p);
    const int food=fn<int(__fastcall *)(Obj,int,GameList *)>(GA(GA_32F080))(target,static_cast<int>(kTradeSoldiers),&copy);   // frees the copy
    Obj vt=*reinterpret_cast<Obj *>(capital);
    const int gold_have=reinterpret_cast<int(__fastcall *)(Obj)>(*reinterpret_cast<void **>(vt+0x90))(capital);
    const int food_have=reinterpret_cast<int(__fastcall *)(Obj)>(*reinterpret_cast<void **>(vt+0xA0))(capital);
    const int soldiers_have=static_cast<int>(dword(capital,0x3C));
    if(gold_have<static_cast<int>(kMinGold) || soldiers_have<static_cast<int>(kMinSoldiers))return fail(6,static_cast<unsigned>(gold_have>0?gold_have:0)|(static_cast<unsigned>(soldiers_have>0?(std::min)(soldiers_have,65535):0)<<16));
    if(food<0 || food_have<food+static_cast<int>(kFoodSpare))return fail(7,static_cast<unsigned>(food_have>0?food_have:0));
    TradeCommand cmd{image+GA(GA_13466B0),nullptr,tribe,0,0,kTradeGold,static_cast<unsigned>(food),kTradeSoldiers,0};
    fn<void(__fastcall *)(void *)>(GA(GA_1AC60))(&cmd);
    if(!cmd.handle)return fail(3,2);
    for(Obj p:officers)list_add(*reinterpret_cast<GameList *>(&cmd),p);
    fn<void(__fastcall *)(void *,int)>(GA(GA_1EBC60))(&cmd,0);
    fn<void(__fastcall *)(void *)>(GA(GA_1BFFF0))(&cmd);
    // 1EBC60 validates and may drop it; either way the month counts as done (no second trade from a job flag read too early).
    const bool ok=valid(fn<Obj(__fastcall *)(Obj)>(GA(GA_22C4B0))(officers.front()));
    // value = tribe | officers << 8 | intimacy << 16, hex = first officer id
    log_event(ok?"trade_issued":"trade_unconfirmed",own,tribe|(static_cast<unsigned>(officers.size())<<8)|((intimacy&0xFFFF)<<16),word(officers.front(),0x10));
    return true;
}
// Once per force and month, on the 상순 days (1..10) until one trade is sent; the reason of the first failed try of the month
// is logged (the tick may only run on some days of 상순).
static void trade_tick(unsigned own) { COST_SCOPE("trade_hire:trade_tick");
    if(!computer_force(own))return;
    Obj ctx=pointer(world(),0x871E8);if(!ctx)return;
    const unsigned day=ctx[0x37];if(day<1 || day>10)return;
    const int month=static_cast<int>(word(ctx,0x34))*12+ctx[0x36];
    static Obj root;static int done[46],logged[46];
    if(root!=world()) {root=world();for(int &d:done)d=-1;for(int &d:logged)d=-1;}
    if(done[own]==month)return;
    const bool log_fail=logged[own]!=month;logged[own]=month;
    if(issue(own,nullptr,log_fail) || day==10)done[own]=month;
}
// A region of city `id` borders a region held by another hostile regular force (1..45), as officer_rotation::front_cities.
static bool front_city(unsigned id,unsigned own) {
    for(unsigned n=1;n<=500;++n) {
        Obj a=table(0x6CAF0,n,500);if(!valid(a) || a[0x35]!=id)continue;
        for(unsigned i=0;i<8;++i) {
            const unsigned m=word(a,0x56+i*2);if(!(a[0x66]&(1u<<i)) || !m || m>500)continue;
            Obj b=table(0x6CAF0,m,500);
            Obj tile=valid(b)?table(0xE270,word(b,0x3A),48399):nullptr;
            const unsigned owner=valid(tile)?tile[0x14]:0;
            if(owner && owner<=45 && owner!=own && fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(owner,own)!=0)return true;
        }
    }
    return false;
}
static bool front_trait(Obj p) {for(unsigned t:kFrontTraits)if(has_trait(p,t))return true;return false;}
static Obj best_hire(Obj force,GameList &listed) {
    GameList pool{nullptr,*reinterpret_cast<unsigned **>(force+0x250)};
    Obj best=nullptr;int best_lead=-1;
    for(Link *l=list_first(pool);l;l=l->next) {
        Obj p=l->value;if(!valid(p) || !front_trait(p))continue;
        bool taken=false;for(Link *k=list_first(listed);k;k=k->next)if(k->value==p) {taken=true;break;}
        if(taken)continue;
        const int lead=stat(p,0);if(lead>best_lead) {best_lead=lead;best=p;}
    }
    return best;
}
}   // namespace trade_hire
static uint64_t __fastcall hire_node(Obj node,Obj context,Obj entity) { COST_SCOPE("hire_node");
    auto original=fn<Execute>(GA(GA_2DAB0));
    if(!mod_features.front_hire || !enabled || !node || !context)return original(node,context,entity);
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context),city=nullptr,people=nullptr;
    auto read=fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20));
    if(!blackboard || !read(blackboard,dword(node,0x38),&city,0) || !valid(city) || !read(blackboard,dword(node,0x3C),&people,0) || !people)
        return original(node,context,entity);
    const unsigned own=trade_hire::city_force(city),id=word(city,0x4E);
    if(!trade_hire::computer_force(own) || !trade_hire::front_city(id,own))return original(node,context,entity);
    Obj force=trade_hire::force_obj(own);if(!valid(force))return original(node,context,entity);
    GameList &listed=*reinterpret_cast<GameList *>(people);
    unsigned swapped=0,kept=0;
    for(Link *l=list_first(listed);l;l=l->next) {
        if(valid(l->value) && trade_hire::front_trait(l->value)) {++kept;continue;}
        Obj pick=trade_hire::best_hire(force,listed);
        if(!pick) {
            static Obj root;static int logged[52];
            if(root!=world()) {root=world();for(int &d:logged)d=-1;}
            if(id<52 && logged[id]!=game_day()) {logged[id]=game_day();log_event("front_hire_none",id,own,valid(l->value)?word(l->value,0x10):0);}
            return 2;
        }
        // value = replaced id | hired id << 16, hex = force
        log_event("front_hire_swap",id,(valid(l->value)?word(l->value,0x10):0)|(static_cast<unsigned>(word(pick,0x10))<<16),own);
        l->value=pick;++swapped;
    }
    const uint64_t result=original(node,context,entity);
    log_event("front_hire",id,swapped|(kept<<8)|(own<<16),static_cast<unsigned>(result));
    return result;
}
static uint64_t __fastcall trade_node(Obj node,Obj context,Obj entity) { COST_SCOPE("trade_node");
    auto original=fn<Execute>(GA(GA_54CE0));
    if(!mod_features.computer_trade || !enabled || in_policy || !node || !context)return original(node,context,entity);
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context),city=nullptr;
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(blackboard,dword(node,0x38),&city,0) || !valid(city))
        return original(node,context,entity);
    Obj force=fn<Obj(__fastcall *)(Obj,Obj)>(GA(GA_B7210))(context,entity);
    const unsigned own=valid(force)?(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(force,0)+0x60))(force):0;
    if(!trade_hire::computer_force(own))return original(node,context,entity);
    ++in_policy;
    AcquireSRWLockExclusive(&policy_lock);
    const bool ok=trade_hire::issue(own,city,true);
    ReleaseSRWLockExclusive(&policy_lock);
    --in_policy;
    return ok?1u:2u;
}
static uint64_t __fastcall bt_condition(unsigned op,unsigned char negate,Obj context,Obj entity,int *args,int count)
{ COST_SCOPE("bt_condition");
    if(mod_features.ai_max_aggression && op==0x73 && args)return args[0]==4?1u:0u;
    auto original=fn<BtCondition>(GA(GA_C36A0));
    if(context)retake_adjacent_tick(context);
    uint64_t result=original(op,negate,context,entity,args,count);
    if(!mod_features.ai_max_aggression)return result;
    if(op==0x74 && !(result&0xFF) && args && context) {
        Obj corps=nullptr;Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);
        if(blackboard && fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(blackboard,static_cast<unsigned>(args[0]),&corps,0) &&
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
{ COST_SCOPE("person_info_node");
    uint64_t result=fn<uint64_t(__fastcall *)(Obj,Obj,Obj)>(GA(GA_8F810))(node,context,entity);
    if(!mod_features.ai_max_aggression || result!=1 || !node || !context)return result;
    unsigned item=dword(node,0x3C);
    if(item!=0x0D && item!=0x20)return result;   // 34 (+17E) is 계략, not aggression (REA 합본 2장, 2026-10-22)
    Obj person=nullptr;Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(blackboard,dword(node,0x38),&person,0) ||
       !valid(person))return result;
    Obj district=table(0xE0D0,person[0xE0],51);
    if(!valid(district) || fn<int(__fastcall *)(Obj)>(GA(GA_2350F0))(district))return result; // no corps / player force
    fn<char(__fastcall *)(Obj,unsigned,int)>(GA(GA_B29250))(blackboard,dword(node,0x40),20);
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
// attack_eval70 / retreat_eval60 (user 2026-11-10, computer corps only). The attack review (tree cca2ce60 N1990) and the
// field retreat recheck (N10537) both run CGetFootholdFightingStrengthNode (7EBA0) with input 새공격목표 0xAD8610FE, output
// 목표요구전투력 0xB57EAF65 and radius 10, so the parameters cannot tell them apart. N2076 (CVariablePlusVariableNode 922F0:
// 공격검토기준 0x19FCD973 += 목표기초보정액 0x2E969939) runs only in the attack review, after N1990 in the same corps run: the
// radius-10 node object seen last before N2076 is N1990, every other radius-10 node is N10537 (pw36c_bt_probe.py). The node
// objects belong to the loaded tree, so the learnt pointer stays valid until the world changes. Until the first N2076 of a
// world, nothing is scaled. The original value is computed first and only the output variable is rescaled afterwards.
// The interception review is a different node and keeps the original value.
static constexpr unsigned kTargetCityVar=0xAD8610FEu,kCorpsVar=0xD6A5EAAEu,kAttackThreshold=0x19FCD973u,kBaseCorrection=0x2E969939u;
static Obj g_attack_foothold;            // N1990 node object (attack review)
static Obj g_foothold_world;             // world the pointer was learnt in
static thread_local Obj g_foothold_last; // radius-10 target node this thread ran last
static SRWLOCK g_foothold_lock=SRWLOCK_INIT;
static bool attack_eval_wanted() {return mod_features.attack_eval70 || mod_features.retreat_eval60;}
// 1 = attack review (N1990), 2 = retreat recheck (N10537), 0 = not known yet.
static unsigned foothold10_kind(Obj node) { COST_SCOPE("ai_nodes:foothold10_kind");
    Obj root=world();
    AcquireSRWLockExclusive(&g_foothold_lock);
    if(g_foothold_world!=root) {g_attack_foothold=nullptr;g_foothold_world=root;}
    unsigned kind=!g_attack_foothold?0u:node==g_attack_foothold?1u:2u;
    ReleaseSRWLockExclusive(&g_foothold_lock);
    return kind;
}
// The corps under review belongs to a computer force (never the player's direct or delegated corps).
static bool foothold_computer_corps(Obj context) { COST_SCOPE("ai_nodes:foothold_computer_corps");
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context),corps=nullptr;
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(blackboard,kCorpsVar,&corps,0) || !valid(corps))
        return false;
    return fn<int(__fastcall *)(Obj)>(GA(GA_2350F0))(corps)==0;   // 2350F0: corps of the player force
}
static void foothold_scale(Obj context,unsigned percent,const char *what) { COST_SCOPE("ai_nodes:foothold_scale");
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);int value=0;
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,int *,int)>(GA(GA_B28BB0))(blackboard,kRecheckEnemy,&value,0) || value<=0)return;
    int scaled=static_cast<int>(static_cast<long long>(value)*percent/100);
    if(scaled<1)scaled=1;
    if(scaled>=value)return;
    fn<char(__fastcall *)(Obj,unsigned,int)>(GA(GA_B29250))(blackboard,kRecheckEnemy,scaled);
#ifndef NO_RUNTIME_LOG
    static volatile LONG lines;
    if(InterlockedIncrement(&lines)<=300)log_event(what,static_cast<unsigned>(value),static_cast<unsigned>(scaled),percent);
#else
    (void)what;
#endif
}
// no_tribe_target (local47, user 2026-11-11 "이민족 성은 어떤 경우에도 목표로 못 삼게"): the original never lets a
// non-player force take a tribe castle (285DB0, rules_combat C12), yet a low enemy estimate makes computer corps attack
// one. When the target of the attack review / retreat recheck (radius-10 strength node, input 새공격목표) is a castle of a
// tribe force (46..50), the required strength is raised to a value no corps reaches: the review drops the target and a
// corps already on its way retreats through the original recheck. Computer corps only; off = original value.
static constexpr int kTribeStrength=0x3FFFFFFF;
static bool tribe_city(Obj city) { COST_SCOPE("ai_nodes:tribe_city");
    if(!valid(city))return false;
    Obj force=fn<Obj(__fastcall *)(Obj)>(GA(GA_22E3E0))(city);
    if(!valid(force))return false;
    const unsigned id=(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(force,0)+0x60))(force);
    return id>=46 && id<=50;
}
static bool tribe_target_block(Obj context) { COST_SCOPE("ai_nodes:tribe_target_block");
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context),city=nullptr;
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(blackboard,kTargetCityVar,&city,0) || !tribe_city(city))
        return false;
    fn<char(__fastcall *)(Obj,unsigned,int)>(GA(GA_B29250))(blackboard,kRecheckEnemy,kTribeStrength);
#ifndef NO_RUNTIME_LOG
    static volatile LONG lines;
    if(InterlockedIncrement(&lines)<=300)log_event("tribe_target_blocked",word(city,0x4E),0,0);
#endif
    return true;
}
static uint64_t __fastcall foothold_strength_node(Obj node,Obj context,Obj entity)
{ COST_SCOPE("foothold_strength_node");
    auto original=fn<uint64_t(__fastcall *)(Obj,Obj,Obj)>(GA(GA_7EBA0));
    if((attack_eval_wanted() || mod_features.no_tribe_target) && node && context && dword(node,0x38)==kTargetCityVar &&
       dword(node,0x3C)==kRecheckEnemy && dword(node,0x40)==10) {
        g_foothold_last=node;
        unsigned kind=foothold10_kind(node);
        uint64_t result=original(node,context,entity);
        if(mod_features.no_tribe_target && foothold_computer_corps(context) && tribe_target_block(context))return result;
        if(kind==1 && mod_features.attack_eval70 && foothold_computer_corps(context))foothold_scale(context,70,"attack_eval70");
        if(kind==2 && mod_features.retreat_eval60 && foothold_computer_corps(context))foothold_scale(context,60,"retreat_eval60");
        return result;
    }
    if(!mod_features.attack_recheck_fix || !node || !context || dword(node,0x3C)!=kRecheckFriendly || dword(node,0x40)!=5)
        return original(node,context,entity);
    recheck_pending=false;
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);int sum=0;
    auto read=fn<char(__fastcall *)(Obj,unsigned,int *,int)>(GA(GA_B28BB0));
    bool had=blackboard && read(blackboard,kRecheckFriendly,&sum,0);
    uint64_t result=original(node,context,entity);
    int base=0;
    if(had && read(blackboard,kRecheckFriendly,&base,0)) {
        fn<char(__fastcall *)(Obj,unsigned,int)>(GA(GA_B29250))(blackboard,kRecheckFriendly,sum);
        recheck_base_power=base;recheck_pending=true;
    }
    return result;
}
// N2076 (attack_eval70 / retreat_eval60 only; the row is published only when one of them is on): learn which radius-10
// target node is the attack review. No blackboard write.
static uint64_t __fastcall attack_threshold_node(Obj node,Obj context,Obj entity)
{ COST_SCOPE("attack_threshold_node");
    uint64_t result=fn<uint64_t(__fastcall *)(Obj,Obj,Obj)>(GA(GA_922F0))(node,context,entity);
    if(!attack_eval_wanted() || !node || dword(node,0x38)!=kAttackThreshold || dword(node,0x3C)!=kBaseCorrection)return result;
    Obj last=g_foothold_last;g_foothold_last=nullptr;
    if(!last)return result;
    Obj root=world();
    AcquireSRWLockExclusive(&g_foothold_lock);
    bool learnt=g_attack_foothold!=last;
    g_attack_foothold=last;g_foothold_world=root;
    ReleaseSRWLockExclusive(&g_foothold_lock);
#ifndef NO_RUNTIME_LOG
    if(learnt)log_event("attack_review_node_learnt",0,0,0);
#else
    (void)learnt;
#endif
    return result;
}
static uint64_t __fastcall variable_minus_node(Obj node,Obj context,Obj entity)
{ COST_SCOPE("variable_minus_node");
    auto original=fn<uint64_t(__fastcall *)(Obj,Obj,Obj)>(GA(GA_92080));
    if(!recheck_pending || !node || !context || dword(node,0x38)!=kRecheckEnemy || dword(node,0x3C)!=kRecheckFriendly)
        return original(node,context,entity);
    recheck_pending=false;
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);int sum=0;
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,int *,int)>(GA(GA_B28BB0))(blackboard,kRecheckFriendly,&sum,0))
        return original(node,context,entity);
    auto write=fn<char(__fastcall *)(Obj,unsigned,int)>(GA(GA_B29250));
    write(blackboard,kRecheckFriendly,recheck_base_power);   // enemy -= this base's power (original intent)
    uint64_t result=original(node,context,entity);
    write(blackboard,kRecheckFriendly,sum);                  // the friendly attack sum survives the loop
    log_event("attack_recheck_sum_kept",0,static_cast<unsigned>(sum),static_cast<unsigned>(recheck_base_power));
    return result;
}
static volatile LONG bt_condition_state; // 0 pending, 1 installed, 2 unexpected pointer
static void ensure_bt_condition()
{ COST_SCOPE("ai_nodes:ensure_bt_condition");
    if((!mod_features.ai_max_aggression && !mod_features.retake_adjacent && !mod_features.gate_intercept && !mod_features.helm_intercept &&
        !mod_features.garrison_recall && !mod_features.computer_trade) || bt_condition_state)return;
    void *current=*reinterpret_cast<void **>(image+GA(GA_1B98D10));
    if(!current)return; // AI not initialised yet
    if(patch_pointer(GA(GA_1B98D10),GA(GA_C36A0),reinterpret_cast<void *>(&bt_condition))) {
        InterlockedExchange(&bt_condition_state,1);log_event("ai_max_aggression_installed",0,0,0);
    } else if(InterlockedCompareExchange(&bt_condition_state,2,0)==0)
        log_event("ai_max_aggression_unexpected_pointer",0,static_cast<unsigned>(reinterpret_cast<Obj>(current)-image),0);
}
static uint64_t __fastcall first_army_node(Obj node, Obj context, Obj entity)
{
    COST_SCOPE("first_army_node");
    NativePhase phase("army_priority");
    ensure_bt_condition();
    auto original = fn<Execute>(GA(GA_201C0));
    if (!enabled || in_policy || native_search_depth) return original(node, context, entity);
    Obj army = nullptr;
    Obj blackboard = fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);
    fn<void(__fastcall *)(Obj, unsigned, Obj *, int)>(GA(GA_B28D20))(blackboard, dword(node, 0x38), &army, 0);
    ++in_policy;
    AcquireSRWLockExclusive(&policy_lock);
    Progress *p=campaign_controlled(army)?observe_capture(army):nullptr;
    WATCH_BEFORE(army,p);
#ifdef SORTIE_FORMATION
    if(p)campaign_proactive_departures(army);
    if(p)campaign_saturated_departures(army);
    campaign_frontier_diagnostic();
#endif
    bool changed=p && campaign_individual_priority(army,*p);
    bool urgent=p && (p->forced_return || p->combat_target || p->combat_city || p->home_defense_city || campaign_castle_locked(army));
    if(!changed && !urgent && p && !p->credit && !p->build_chain && !p->facility && !campaign_group_member(army) && !p->regional_worker && node && context && fn<Execute>(GA(GA_20040))(node,context,army)==1) {
        log_event("native_supply_restore",army_id(army),0,word(army,0x2A));changed=true;
    }
    // local88 (user 2026-11-12 "점령하기전에 나와서 적의 거점 점령부대 병참을 짜르길 원해"): a retake unit does not claim the
    // turn at this first node, so the original chain goes on to its supply-cut node (cut_node below: the original
    // plans the cut, the mod narrows the victims to player armies). No cut = the retake step there.
    // local89: only where the target region holds more of our hexes than hostile ones (retake_cut_first).
    const bool cutter=p && p->regional_worker && p->retake && mod_features.combat_cut && !p->forced_return && !p->returning && retake_cut_first(army,*p);
    if(!changed && !cutter)changed=apply_priority(army,node,context);
    ReleaseSRWLockExclusive(&policy_lock);
    --in_policy;
    mod_step(changed?"army_node:mod":"army_node:native");
    WATCH_AFTER(army,changed?"watch_army_node_mod":"watch_army_node_pre");
    const uint64_t result=changed ? 1 : original(node, context, entity);
    if(!changed)WATCH_AFTER(army,"watch_army_node_native");
    return result;
}
static uint64_t __fastcall cut_node(Obj node, Obj context, Obj army)
{
    COST_SCOPE("cut_node");
    NativePhase phase("army_cut");
    if (!enabled || in_policy || native_search_depth) return fn<Execute>(GA(GA_1F380))(node, context, army);
    Obj supplied = army;
    army = fn<Obj(__fastcall *)(Obj, Obj)>(GA(GA_B7100))(context, supplied);
    if (!campaign_controlled(army)) return fn<Execute>(GA(GA_1F380))(node, context, supplied);
    ++in_policy;
    AcquireSRWLockExclusive(&policy_lock);
    Progress *p=observe_capture(army);
    WATCH_BEFORE(army,p);
    bool changed=p && campaign_individual_priority(army,*p);
    bool urgent=p && (p->forced_return || p->combat_target || p->combat_city || p->home_defense_city || campaign_castle_locked(army));
    // local45 (user 2026-11-10 "모든 컴퓨터 군세가 기본적으로 병참단절을 노려야"): the original cut node used to run only
    // after every mod priority (group march, siege priority, operation hold, sortie member) had passed, so a computer
    // group member almost never reached it (no cut_player in the 11-10 logs). It now runs first for a computer-side
    // army that has no urgent duty; the original decides as before (통솔 or 지력 > 64, troops mode 8 excluded, D1760
    // route) and the mod still narrows the victims to player armies. No cut found: the mod priorities follow.
    bool cut_tried=false;
    if(!changed && !urgent && mod_features.combat_cut && computer_side_army(army)) {
        cut_tried=true;
        ++in_cut;
        changed=fn<Execute>(GA(GA_1F380))(node,context,supplied)==1;
        --in_cut;
        if(changed)log_event("cut_player",army_id(army),dword(army,0x1F4),word(army,0x2A));
    }
    if(!urgent && p && !(cut_tried && changed) && !campaign_group_member(army) && !p->regional_worker && node && context && fn<Execute>(GA(GA_20040))(node,context,army)==1)changed=true;
    if(!changed && p && !campaign_group_member(army) && !p->regional_worker)changed=campaign_siege_priority(army,*p);
    if(!changed && p && p->operation_leader)changed=true;
    if(!changed && p && p->regional_worker) {apply_priority(army,node,context);changed=true;}
#ifdef SORTIE_FORMATION
    if(!changed && (sortie_member(army) || sortie_escort_member(army)))changed=true;
#endif
    // U39 combat_cut (docs/COMBAT_REDESIGN.md): the original cut node plans and orders (its 64 ability gate, mode 8
    // exclusion, D1760 route, 26E260). For a computer-side army the mod only narrows D1760's victim list to player
    // armies (C1D10 wrapper at D3516, active while in_cut). Player direct armies, delegated corps and combat_cut=0
    // run the node unchanged = original.
    if(!changed && !cut_tried && !(p && p->forced_return)) {
        const bool filter=mod_features.combat_cut && computer_side_army(army) && !campaign_castle_locked(army);
        if(filter)++in_cut;
        changed=fn<Execute>(GA(GA_1F380))(node,context,supplied)==1;
        if(filter)--in_cut;
        if(changed && filter)log_event("cut_player",army_id(army),dword(army,0x1F4),word(army,0x2A));
    }
    if(changed && p && (dword(army,0x1F0)&8))remember_cut(army,*p);
    ReleaseSRWLockExclusive(&policy_lock);
    --in_policy;
    mod_step(changed?"cut_node:mod":"cut_node:native_skip");
    WATCH_AFTER(army,changed?"watch_cut_node_mod":"watch_cut_node_skip");
    return changed ? 1 : 2;
}
static uint64_t __fastcall city_node(Obj node, Obj context, Obj troops)
{
    COST_SCOPE("city_node");
    NativePhase phase("troops_city");
    auto original = fn<Execute>(GA(GA_69AB0));
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
    WATCH_TROOPS(troops,"watch_troops_pre");
    if(mod_features.mission_v2)mission_route_waypoint(troops);
    WATCH_TROOPS(troops,"watch_troops_mod");
    uint64_t result = original(node, context, troops);
    WATCH_TROOPS(troops,"watch_troops_native");
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
        } else if(lead && attack_mode(before_mode) && !attack_mode(after_mode)) {
            // Diagnostics (2026-11-10): an attack group the original turned back and why the veto did not apply.
            unsigned why=(mod_features.recall_veto?0u:1u)|(fresh?0u:2u)|(fresh==members?0u:4u)|(soldiers>=3000?0u:8u)|
                         (before_goal && before_goal<=61?0u:16u)|(before_goal && before_goal<=61 && hostile_city(lead,before_goal)?0u:32u);
            log_event("troops_recall_allowed",army_id(lead),(why<<24)|(fresh<<16)|members,soldiers);
        }
    }
    mod_step("city_node:native_then_members");
    for (unsigned i = 1; i <= 500; ++i) {
        Obj army = table(0x7E838, i, 500);
        if (valid(army) && belongs_to_troops(army, troops)) {
            if(campaign_controlled(army))apply_priority(army,node,context);
        }
    }
    WATCH_TROOPS(troops,"watch_troops_end");
    ReleaseSRWLockExclusive(&policy_lock);
    --in_policy;
    return result;
}
static uint64_t __fastcall retreat_node(Obj node, Obj context, Obj entity)
{
    COST_SCOPE("retreat_node");
    NativePhase phase("army_retreat");
    auto original=fn<Execute>(GA(GA_61C90));
    if(!enabled || in_policy || native_search_depth)return original(node,context,entity);
    Obj army=nullptr;
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);
    fn<void(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(blackboard,dword(node,0x38),&army,0);
    if(!campaign_controlled(army))return original(node,context,entity);
    ++in_policy;AcquireSRWLockExclusive(&policy_lock);
    Progress *p=observe_capture(army);if(p)remember_campaign(army,*p);
    WATCH_BEFORE(army,p);
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
                if(a->campaign && hostile_city(army,a->city) && !tribe_goal(a->city) && range<nearest) {nearest=range;city=a->city;}
            }
            if(city) {p->city=static_cast<unsigned short>(city);p->campaign=true;}
        }
        changed=apply_priority(army,node,context);
        if(!changed) {
            unsigned city=campaign_field_city(army,*p);
            if(city) {p->city=static_cast<unsigned short>(city);p->campaign=true;changed=campaign_step(army,*p,node,context);}
        }
        // 모드 일거리·원작 대체 행동이 모두 없으면 원작 퇴각을 허용
        // (사용자 결정 2026-10-29: 목표 상실 후 40일 제자리 대기 방지).
        if(!changed)changed=try_other(army,node,context);
    }
    if(!changed && p)log_event("native_retreat_allowed",army_id(army),word(army,0x16),word(army,0x2A));
    mod_step(changed?"retreat_node:mod_veto":"retreat_node:native");
    WATCH_AFTER(army,changed?"watch_retreat_node_mod":"watch_retreat_node_pre");
    ReleaseSRWLockExclusive(&policy_lock);--in_policy;
    const uint64_t result=changed ? 1 : original(node,context,entity);
    if(!changed)WATCH_AFTER(army,"watch_retreat_node_native");
    return result;
}

