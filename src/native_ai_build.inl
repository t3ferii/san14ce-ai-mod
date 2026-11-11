#pragma once
// Combat AI: build orders (try_build / try_other), moved verbatim out of native_adapter.cpp (W6-D).
static void build_skip(Obj army, Progress &p, unsigned facility, const char *reason) { COST_SCOPE("ai_build:build_skip");
    int day=game_day();
    if(p.build_audit_day!=day) {p.build_audit_day=day;p.build_audit_mask=0;}
    unsigned bit=1u<<facility;if(p.build_audit_mask&bit)return;
    p.build_audit_mask|=bit;log_event(reason,army_id(army),facility,word(army,0x2A));
}
static bool catapult_player_ground(Obj tile);   // native_ai_wall.inl
static bool try_build(Obj army, Progress &p, unsigned cohort_center=48400,unsigned preferred=0)
{
    COST_SCOPE("try_build");
    if(campaign_castle_locked(army))return false;
    if (continue_build(army, p)) return true;
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
    struct Step {unsigned facility,pass;};
    Step steps[40];unsigned step_count=0;
    // local53 siege slots (user 2026-11-11 "투석대 요새 군악대 1개씩 ... 요새를 못지으면 진 ... 투석대를 못지으면 망루 ...
    // 나머지 부대는 함정건설"): siege ground fills three slots per region, each once: catapult 6 (else watchtower 5),
    // fort 4 (else camp 3), band 7. Next to a player region (ai_catapult_block) the catapult is skipped as if not
    // unlocked and the watchtower fills that slot. With every slot filled, each further army builds one trap (11, 12, 14, 13).
    const bool siege_slots=mod_features.mission_v2 && !p.regional_worker && !builder && !preferred;
    bool trap_phase=false;
    if(siege_slots) {
        const bool catapult_banned=mod_features.ai_catapult_block && computer_army(army) &&
            catapult_player_ground(table(0xE270,word(area,0x3A),48399));
        // User 2026-11-11: next to a player region the catapult counts as not unlocked; the watchtower still fills the slot.
        // local54 [combat] siege_no_catapult / siege_no_watchtower / siege_no_trap (computer side, under mission_v2): an
        // excluded type is never tried; a slot whose types are all excluded counts as done, so the traps can follow.
        const bool computer=computer_side_army(army);
        const bool allow6=!catapult_banned && !(computer && mod_features.siege_no_catapult);
        const bool allow5=!(computer && mod_features.siege_no_watchtower);
        const bool allow_trap=!(computer && mod_features.siege_no_trap);
        const bool siege_open=(allow6 || allow5) && !(seen&((1u<<6)|(1u<<5)));
        const bool fort_open=!(seen&((1u<<4)|(1u<<3))),band_open=!(seen&(1u<<7));
        if(siege_open) {if(allow6)steps[step_count++]={6,0};if(allow5)steps[step_count++]={5,0};}
        if(fort_open) {steps[step_count++]={4,0};steps[step_count++]={3,0};}
        if(band_open)steps[step_count++]={7,0};
        if(!siege_open && !fort_open && !band_open && allow_trap) {
            trap_phase=true;
            // 17 rockfall (user 2026-11-11 "시정으로 열리는 낙석같은것도"): opened by the 산천시설 policy, mountain sites only;
            // the original candidate list and permission decide both (unlock and terrain).
            for(unsigned f:{11u,12u,17u,14u,13u})steps[step_count++]={f,0};
        }
    } else
    for(unsigned pass=0;pass<(builder?2u:1u);++pass)for(unsigned f:order)steps[step_count++]={f,pass};
    for (unsigned step=0;step<step_count && !applied;++step) {
        const unsigned facility=steps[step].facility,pass=steps[step].pass;
        if(preferred && facility!=preferred)continue;
        if(pass==0 && !trap_phase && (seen & (1u<<facility))) {build_skip(army,p,facility,"build_skip_present");continue;}
        if(pass==1 && (preferred || !(seen & (1u<<facility))))continue;
        missing=true;
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
        if(!list_first(candidates)) {
            GameList local;list_init(local);
            Obj center=table(0xE270,p.build_center,48399);
            if(valid(center))fn<void(__fastcall *)(Obj,int,GameList *)>(GA(GA_2A9C90))(center,10,&local);
            for(Link *item=list_first(local);item;item=item->next)if(valid(item->value) &&
                item->value[0x14]==army_force(army) && fn<Obj(__fastcall *)(Obj)>(GA(GA_22B8E0))(item->value)==area)
                list_add(candidates,item->value);
            list_free(local);
        }
        bool offered=list_first(candidates)!=nullptr,native_rejected=false,out_of_range=false;
        for (Link *item = list_first(candidates); item; item = item->next) {
            Obj tile = item->value;
            if (!valid(tile) || tile[0x14] != army_force(army)) continue;
            unsigned hex = fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F830))(tile);
            if (hex >= 48400 || reserved_hex(hex, army)) continue;
            if(siege_range && (siege_castle>=48400 || distance(siege_castle,hex)<3 || distance(siege_castle,hex)>ring_max)) {out_of_range=true;continue;}
            if(morale_band && distance(word(army,0x2A),hex)>6) {out_of_range=true;continue;}
            if(cohort_center<48400 && distance(cohort_center,hex)>2)continue;
            if (!fn<int(__fastcall *)(GameList *, Obj, Obj)>(GA(GA_27A8D0))(&people, tile, definition)) {native_rejected=true;continue;}
            if (set_build(army, facility, hex)) {
                p.facility = static_cast<unsigned char>(facility);
                p.construction_hex = static_cast<unsigned short>(hex); p.credit = false;
                p.built_once = true; p.finished_build = false;
                p.build_chain=true;
                p.build_retry_day=game_day();p.build_rejections=0;
                if(mod_features.mission_v2 && !p.regional_worker && !builder)p.build_chain=false; // siege ground: one facility, then the mission
                if(trap_phase)p.work_reviewed_center=p.build_center;   // local53: one trap per army per region
                log_event("build_started", army_id(army), facility, hex);
                applied = true;
            }
            if (applied) break;
        }
        list_free(candidates);
        if(!applied)build_skip(army,p,facility,!offered?"build_skip_native_candidates":
                              (native_rejected?"build_skip_native_check":out_of_range?"build_skip_siege_range":"build_skip_no_free_site"));
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
