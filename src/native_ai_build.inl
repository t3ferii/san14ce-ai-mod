#pragma once
// Combat AI: build orders (try_build / try_other), moved verbatim out of native_adapter.cpp (W6-D).
static void build_skip(Obj army, Progress &p, unsigned facility, const char *reason) { COST_SCOPE("ai_build:build_skip");
    int day=game_day();
    if(p.build_audit_day!=day) {p.build_audit_day=day;p.build_audit_mask=0;}
    unsigned bit=1u<<facility;if(p.build_audit_mask&bit)return;
    p.build_audit_mask|=bit;log_event(reason,army_id(army),facility,word(army,0x2A));
}
static bool catapult_player_ground(Obj tile);   // native_ai_wall.inl
static bool campaign_mountain(unsigned hex);      // native_campaign_support.inl
// local211 (user 2026-11-17 "군악대 ... 매일 확인할때 목표로 가는 길쪽으로만 확인할수 있나? 산악지형에 올라갔다가 내려갔다가 반복할까봐"):
// the goal of the army asking for a morale band (campaign_morale_goal / the route hook's goal), set around its try_build.
static __declspec(thread) unsigned band_way_goal=48400;
// local213 (user 2026-11-17 "하루한번이 한턴(한순 / 10일)이지? 그렇게 바꿔주고 원작 건설 조건을 만족해야해", "강이나 이웃 군사시설 2헥스
// 성 3헥스 내에 겹쳐짓는일이 있어선 안돼"): a morale band (preferred 7) is tried at most once per 순 per army (same army,
// commander and world). local215 (user 2026-11-17 "5일에 건설 실패 했어 그럼 15일날 다시풀리는거야? ... 1-10일 11-20일 21-30일
// 순서로 턴이 진행"): the period is the 순 (elapsed day / 10; months are 30 days, so 순 starts are 10 days apart, KB
// rules_time 1), not 10 days from the try: a try on day 5 opens again on day 11. The AI nodes that ask for a band run at
// the 순 start anyway (log: every build_started / route_morale_build day is a 순 start), several calls per army that day. The site still passes the original final check 27A8D0 -> 27AAC0 (band = class 2,
// terrain mask 0xE plain/road/sand: no river, no mountain; spacing ring; region cap; cost 2AD6C0), the strict rules hooked
// on 27AAC0 for computer forces (military 2-hex spacing incl. unfinished sites, nothing within 3 hexes of a city or gate
// footprint) and military_spacing_blocked / military_order_near below (other armies' pending military build orders).
static constexpr int kBandPeriodDays=10;
struct BandTry {Obj root=nullptr,army=nullptr;unsigned short commander=0;int day=0;};
static BandTry band_tries[501];
static bool band_try_due(Obj army,bool mark) {
    const unsigned id=army_id(army);if(!id || id>500)return false;
    BandTry &t=band_tries[id];const int day=game_day();
    if(t.root==world() && t.army==army && t.commander==word(army,0x12) && day>=t.day && day/kBandPeriodDays==t.day/kBandPeriodDays)return false;
    if(mark) {t.root=world();t.army=army;t.commander=static_cast<unsigned short>(word(army,0x12));t.day=day;}
    return true;
}
// A band site is on the way: not a mountain (9/10/19), at most 2 hexes of detour toward band_way_goal, and reached by a
// shortest hex line that steps on no mountain (each step takes a neighbour one hex closer, a non-mountain one).
static bool band_site_on_way(unsigned here,unsigned hex) {
    if(here>=48400 || hex>=48400 || campaign_mountain(hex))return false;
    const unsigned goal=band_way_goal;
    if(goal<48400 && goal!=here && distance(here,hex)+distance(hex,goal)>distance(here,goal)+2)return false;
    unsigned c=here;
    for(int step=0;step<12 && c!=hex;++step) {
        const int nd=distance(c,hex)-1;unsigned next=48400;
        for(unsigned d=0;d<6 && next>=48400;++d) {const unsigned n=sortie::neighbor(c,d);if(n<48400 && distance(n,hex)==nd && (n==hex || !campaign_mountain(n)))next=n;}
        if(next>=48400)return false;   // every closer step is a mountain
        c=next;
    }
    return c==hex;
}
static bool try_build(Obj army, Progress &p, unsigned cohort_center=48400,unsigned preferred=0)
{
    COST_SCOPE("try_build");
    if(campaign_castle_locked(army))return false;
    // local269: a 조타 ship builds nothing (every site is on land; 71180 build_skip_spacing army 63 was a ship looking for a band site)
    if(p.helm)return false;
    if (continue_build(army, p)) return true;
    if (preferred==7 && !band_try_due(army,true)) return false;   // local213/215: one morale band try per 순
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
    Obj origin = fn<Obj(__fastcall *)(Obj)>(GA(GA_22BFE0))(army);
    if (!valid(person) || !valid(troops) || !valid(origin)) return false;
    Obj district = fn<Obj(__fastcall *)(Obj)>(GA(GA_22D4E0))(troops);
    Obj goal = fn<Obj(__fastcall *)(Obj, unsigned)>(GA(GA_32E3B0))(world(), p.operation_city?p.operation_city:word(troops, 0x16));
    Obj goal_force = valid(goal) ? fn<Obj(__fastcall *)(Obj)>(GA(GA_22E3E0))(goal) : nullptr;
    // Home capture groups name their own source city, not an enemy force.
    if(p.regional_worker)goal_force=nullptr;
    Obj force = table(0xDF30, army_force(army), 51);
    if (!valid(district) || !valid(force)) return false;
    auto resource = *reinterpret_cast<int(__fastcall **)(Obj)>(pointer(origin, 0) + 0x90);
    GameList people; list_init(people, GA(GA_13466B0)); list_add(people, person);
    static const unsigned order[] = {6, 4, 7, 3, 5, 11, 12, 14, 13};
    bool applied = false;
    unsigned seen=p.cycle_mask|local_facilities(army,area);
    // Each missing type once (user decision 2026-10-21: no filling capacity with repeated types).
    // A builder ruler (inner 건설 5) also repeats useful types until the region is full.
    const bool builder=mod_features.mission_v2 && !p.regional_worker && builder_force(army_force(army));
    // Siege ground (user 2026-11-01): sites stand 3..6 hexes from the hostile goal castle, outside
    // its attack range. Without such a castle this army builds nothing; the mission/native AI go on.
    // siege_ring10 (user 2026-11-10, computer side only): the ring is 3..10 hexes, and a morale band (preferred 7)
    // is built within 6 hexes of the army instead of on the ring. Off, or a player army: the v15.7.1 3..6 ring.
    const bool ring10=mod_features.siege_ring10 && computer_side_army(army);
    const bool morale_band=ring10 && preferred==7;
    const int ring_max=ring10?10:6;
    const bool siege_range=mod_features.mission_v2 && !p.regional_worker && !builder && !morale_band;
    unsigned siege_castle=48400;
    if(siege_range) {
        unsigned city=p.campaign && p.city ? p.city : (p.operation_city?p.operation_city:word(troops,0x16));
        Obj castle=hostile_city(army,city) ? campaign_goal(city) : nullptr;
        if(castle)siege_castle=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(castle);
    }
    bool missing=false;
    // optional (local263): a band or trap that cannot be placed does not hold the army for a retry; it goes on to the siege.
    struct Step {unsigned facility,pass;bool optional;};
    Step steps[40];unsigned step_count=0;
    // local53 siege slots (user 2026-11-11 "투석대 요새 군악대 1개씩 ... 요새를 못지으면 진 ... 투석대를 못지으면 망루 ...
    // 나머지 부대는 함정건설"): catapult 6 (else watchtower 5), fort 4 (else camp 3), band 7. Next to a player region
    // (ai_catapult_block) the catapult is skipped as if not unlocked and the watchtower fills that slot.
    // local263 (user 2026-11-19 "종류별로 2개씩만 건설 가능하게 투석대 2개 요새 2개 ~ 이렇게. 투석대+망루랑 같이 카운트되고
    // 요새+진이랑 같이 카운트되고 군악대나 함정은 제한 없어 대신 함정은 확률로 안걸리면 건너띄고 공성 들어가고"): 투석대+망루 and
    // 요새+진 each stop at 2 of the force in the region (local_facility_pair, sites under way included); band and traps have
    // no count; the trap is built only on its roll, a miss leaves no trap and the army goes on to the siege. Replaces
    // local53's one-per-slot and local69's unlimited repeat of every type once all slots were filled.
    const bool siege_slots=mod_features.mission_v2 && !p.regional_worker && !builder && !preferred;
    const bool catapult_banned=mod_features.ai_catapult_block && computer_army(army) &&
        catapult_player_ground(table(0xE270,word(area,0x3A),48399));
    // User 2026-11-11: next to a player region the catapult counts as not unlocked; the watchtower still fills the slot.
    // local54 [combat] siege_no_catapult / siege_no_watchtower / siege_no_trap (computer side, under mission_v2): an
    // excluded type is never tried; a slot whose types are all excluded counts as done, so the traps can follow.
    const bool computer=computer_side_army(army);
    const bool allow6=!catapult_banned && !(computer && mod_features.siege_no_catapult);
    // local264 (user 2026-11-19 "투석대 지을 수 있으면 망루 건설 금지고 요새 지을 수 있으면 진 건설 금지 ... 시설개발 lv 10인데
    // 망루를 짓길레"): the watchtower only while the catapult is closed to this army (not unlocked for the force: the original
    // unlock check 2C6D40, policy effect level against the definition's threshold, catapult 13/10; or excluded above), the
    // camp only while the fort (13/7) is not unlocked. local53 fell back on them whenever no catapult/fort site passed this
    // try (log 2026-11-19 19:43~19:45, armies 9/38/57/36: catapult native check failed on its sites, watchtower built).
    // local269 (user 2026-11-19 "장료는 요새 지을 수 있는데 진을 짓네"): the same rule in the plain order below (regional workers,
    // builder rulers): 71400 army 3 (장료, home capture worker) skipped 6/4/7 as present and built a camp, then a watchtower.
    const Obj force_obj=fn<Obj(__fastcall *)(Obj)>(GA(GA_22E450))(person);   // the force 27A8D0 hands to 27AAC0 / 2C6D40
    auto unlocked=[force_obj](unsigned f) {
        Obj d=table(0x73958,f,30);
        return valid(force_obj) && valid(d) && fn<int(__fastcall *)(Obj,Obj)>(GA(GA_2C6D40))(force_obj,d)!=0;
    };
    const bool catapult_open=allow6 && unlocked(6),fort_unlocked=unlocked(4);
    if(siege_slots) {
        const bool allow5=!(computer && mod_features.siege_no_watchtower) && !catapult_open;
        const bool allow3=!fort_unlocked;
        const bool allow_trap=!(computer && mod_features.siege_no_trap);
        const unsigned siege_n=local_facility_pair(army,area,6,5),fort_n=local_facility_pair(army,area,4,3);   // local263
        const bool siege_open=(allow6 || allow5) && siege_n<2;
        const bool fort_open=fort_n<2;
        if(siege_open) {if(allow6)steps[step_count++]={6,0,false};if(allow5)steps[step_count++]={5,0,false};}
        if(fort_open) {steps[step_count++]={4,0,false};if(allow3)steps[step_count++]={3,0,false};}
        // band: no count (local263), one per army and region (bit 7 of the army's cycle mask once its band stands), since a
        // band keeps build_chain (local210) and the same army would otherwise chain bands.
        const bool band_open=!(p.cycle_mask&(1u<<7));
        if(band_open)steps[step_count++]={7,0,true};
        // local69 (user 2026-11-12 "함정은 확률로 (10프로) 분기하고 다시 투석대 우선순위로 돌아가게끔"): the trap is a 10 % branch per
        // army and day, tried first on a hit; a miss adds no trap (local263).
        const unsigned roll=(army_id(army)*2654435761u+static_cast<unsigned>(game_day())*40503u+word(area,0x3A)*97u)%100u;
        const bool trap_roll=allow_trap && roll<10u;
        if(trap_roll) {
            // 17 rockfall (user 2026-11-11 "시정으로 열리는 낙석같은것도"): opened by the 산천시설 policy, mountain sites only;
            // the original permission decides both (build_possible: 27A8D0 with the unlock check on, local214/216).
            Step first[5];unsigned n=0;for(unsigned f:{11u,12u,17u,14u,13u})first[n++]={f,0,true};
            for(unsigned i=step_count;i--;)steps[i+n]=steps[i];
            for(unsigned i=0;i<n;++i)steps[i]=first[i];
            step_count+=n;
        }
#ifndef NO_RUNTIME_LOG
        // army, 투석대+망루 | 요새+진 << 8 | band allowed << 16 | trap hit << 17 | catapult open << 18 | fort unlocked << 19, roll
        log_event("build_siege_caps",army_id(army),siege_n|fort_n<<8|(band_open?1u:0u)<<16|(trap_roll?1u:0u)<<17|
                  (catapult_open?1u:0u)<<18|(fort_unlocked?1u:0u)<<19,roll);
#endif
    } else {
        // local270: only a computer-side army under mission_v2 (off = the order as before)
        const bool plain_ban=computer && mod_features.mission_v2;
        for(unsigned pass=0;pass<(builder?2u:1u);++pass)for(unsigned f:order) {
            if(plain_ban && ((f==3 && fort_unlocked) || (f==5 && catapult_open)))continue;   // local269 (local264's rule in the plain order)
            steps[step_count++]={f,pass,false};
        }
#ifndef NO_RUNTIME_LOG
        if(plain_ban && ((fort_unlocked && !(seen&(1u<<3))) || (catapult_open && !(seen&(1u<<5)))))   // a camp / watchtower that would have been tried
            log_event("build_plain_unlocked",army_id(army),(fort_unlocked?1u:0u)|(catapult_open?2u:0u)|(p.regional_worker?4u:0u)|(builder?8u:0u),preferred);
#endif
    }
    for (unsigned step=0;step<step_count && !applied;++step) {
        const unsigned facility=steps[step].facility,pass=steps[step].pass;
        if(preferred && facility!=preferred)continue;
        // siege ground: the counts above decide (local263), not the types already seen in the region
        if(pass==0 && !siege_slots && (seen & (1u<<facility))) {build_skip(army,p,facility,"build_skip_present");continue;}
        if(pass==1 && (preferred || !(seen & (1u<<facility))))continue;
        if(!steps[step].optional)missing=true;
        Obj definition = table(0x73958, facility, 30);
        if (!valid(definition)) {build_skip(army,p,facility,"build_skip_definition");continue;}
        int cost = fn<int(__fastcall *)(Obj, Obj)>(GA(GA_2AD6C0))(force, definition);
        if (cost < 0 || resource(origin) < cost) {build_skip(army,p,facility,"build_skip_money");continue;}
        GameList candidates; list_init(candidates);
        fn<void(__fastcall *)(Obj, Obj, Obj, GameList *, Obj, Obj, Obj)>(GA(GA_BA100))
            (area, person, definition, &candidates, goal_force, district, nullptr);
        // Offensive placement can yield no sites in a peaceful home region.
        // Review local owned land instead; the same native permission, cost,
        // capacity, occupied-site and actual movement checks still apply.
        // local210 (user 2026-11-17 "사기 떨어졌을때 건설 가능하면 군악대 바로 건설"): a morale band also tries the own land of the
        // region within 6 hexes of the army, after the native sites.
        if(!list_first(candidates) || morale_band) {
            GameList local;list_init(local);
            Obj center=table(0xE270,morale_band?word(army,0x2A):p.build_center,48399);
            if(valid(center))fn<void(__fastcall *)(Obj,int,GameList *)>(GA(GA_2A9C90))(center,morale_band?6:10,&local);
            for(Link *item=list_first(local);item;item=item->next)if(valid(item->value) &&
                item->value[0x14]==army_force(army) && fn<Obj(__fastcall *)(Obj)>(GA(GA_22B8E0))(item->value)==area)
                list_add(candidates,item->value);
            list_free(local);
        }
        bool offered=list_first(candidates)!=nullptr,native_rejected=false,out_of_range=false,spaced=false,off_way=false;
        for (Link *item = list_first(candidates); item; item = item->next) {
            Obj tile = item->value;
            if (!valid(tile) || tile[0x14] != army_force(army)) continue;
            unsigned hex = fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F830))(tile);
            if (hex >= 48400 || reserved_hex(hex, army)) continue;
            if (military_spacing_blocked(army, facility, hex) || military_order_near(army, facility, hex)) {spaced=true;continue;}
            // local166: nothing within 3 hexes of the castle footprint (centre distance <= 4, fixdata base interval 3).
            if(siege_range && (siege_castle>=48400 || distance(siege_castle,hex)<=4 || distance(siege_castle,hex)>ring_max)) {out_of_range=true;continue;}
            if(morale_band && distance(word(army,0x2A),hex)>6) {out_of_range=true;continue;}
            if(morale_band && !band_site_on_way(word(army,0x2A),hex)) {off_way=true;continue;}   // local211
            if(cohort_center<48400 && distance(cohort_center,hex)>2)continue;
            if (!build_possible(&people, tile, definition)) {native_rejected=true;continue;}   // local216: one original check incl. unlock
            if (set_build(army, facility, hex)) {
                p.facility = static_cast<unsigned char>(facility);
                p.construction_hex = static_cast<unsigned short>(hex); p.credit = false;
                p.built_once = true; p.finished_build = false;
                p.build_chain=true;
                p.build_retry_day=game_day();p.build_rejections=0;
                // siege ground: one facility, then the mission. local210 (user 2026-11-17 "공성군세 건설 1개만 하는거에 군악대는 카운트
                // 안했으면"): a band (7) does not use up that one facility.
                if(mod_features.mission_v2 && !p.regional_worker && !builder && facility!=7)p.build_chain=false;
                if(siege_slots && ((facility>=11 && facility<=14) || facility==17))
                    p.work_reviewed_center=p.build_center;   // local53: one trap per army per region
                log_event("build_started", army_id(army), facility, hex);
                applied = true;
            }
            if (applied) break;
        }
        list_free(candidates);
        if(!applied)build_skip(army,p,facility,!offered?"build_skip_native_candidates":
                              (native_rejected?"build_skip_native_check":out_of_range?"build_skip_siege_range":spaced?"build_skip_spacing":off_way?"build_skip_band_way":"build_skip_no_free_site"));
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
{ COST_SCOPE("ai_build:try_other");
    if (army[0x37] != 4 || army[0x38] != 5) return false;
    static const unsigned alternatives[] = { GA(GA_1FFC0), GA(GA_1FF40), GA(GA_1F100), GA(GA_1FE80), GA(GA_1FEE0)};
    for (unsigned rva : alternatives) {
        if (fn<Execute>(rva)(node, context, army) == 1) {
            log_event("city_alternative", army_id(army), rva, word(army, 0x2A));
            return true;
        }
    }
    return false;
}
