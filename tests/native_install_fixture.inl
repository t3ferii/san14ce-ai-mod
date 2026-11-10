// W7 (user 2026-11-07): retired integrated installer retained only as the existing atomic-publish test harness.
#ifndef TACTICS_FIXTURE
#error "fixture installer is test-only"
#endif
// TACTICS_FIXTURE unit-test shells only (the integrated DLL build is retired, plan D-1): no registry runs there, so the domestic-tick subscribers
// and sidecar plugins are joined by static initialization (the fixtures call the hooks without install()).
static void fixture_domestic_subscribers()
{
    services::domestic_tick.clear();
#ifdef AWARD_WAREHOUSE
    services::domestic_tick.add("awards",{[](Obj city){if(mod_features.awards_ai)award_ai_city(city);else award_synchronize();},nullptr},[]{return mod_features.awards;},10);
#endif
#ifdef INJURY_PROTECTION
    services::domestic_tick.add("injury",{[](Obj city){injury_synchronize_city(city);},nullptr},[]{return mod_features.injury || mod_features.injury_herbal;},20);
#endif
    services::domestic_tick.add("domestic",{[](Obj city){synchronize_domestic(city);},[](Obj city){synchronize_domestic(city);}},[]{return mod_features.domestic;},30);
}
static void fixture_sidecar_plugins()
{
#ifdef MERIT_CANDIDATE
    merit_sidecars.clear();
    merit_sidecars.add(&terrain_edit::sidecar_plugin,10);
    merit_sidecars.add(&tactic_memory_sidecar_plugin,20);
    merit_sidecars.add(&merit_sidecar_plugin,30);
#ifdef AWARD_WAREHOUSE
    merit_sidecars.add(&award_sidecar_plugin,40);
#endif
#endif
}
static const bool fixture_services_joined=(fixture_domestic_subscribers(),fixture_sidecar_plugins(),true);   // static init: the fixtures call the hooks without install()
static bool install()
{
#ifdef AWARD_WAREHOUSE
    if(!award_guards_match())return false;
#endif
#ifdef INJURY_PROTECTION
    if(!injury_guards_match())return false;
#endif
#ifdef RECRUITMENT_UI_CANDIDATE
    if(!recruit_ui_guards_match() || !recruit_ai_guards_match() || !recruit_alert_guards_match())return false;
#endif
#ifdef MERIT_CANDIDATE
    if(!merit_guards_match())return false;
#endif
#ifdef PERSONNEL_CANDIDATE
    if(!personnel_guards_match())return false;
#endif
#ifdef HIDDEN_LOYALTY
    if(!loyalty_guards_match())return false;
#endif
#ifdef SORTIE_FORMATION
    if(!sortie_guards_match())return false; // at least one compatible planner CALL
#endif
    Obj call = image + GA(GA_D3516);
    if (memcmp(call, kCutCallBytes, 5)) return false;
    if(ga::mcmp(image,GA_16AEF4,0,kPairCallBytes,5) || ga::mcmp(image,GA_175950,0,kDamageEntryBytes,5) || ga::mcmp(image,GA_2B2A90,0,kAdjacencyEntryBytes,5) ||
       ga::mcmp(image,GA_27AAC0,0,kBuildPermissionEntryBytes,5) ||
       ga::mcmp(image,GA_297070,0,kDomesticTickEntryBytes,5) ||
       ga::mcmp(image,GA_36F83,0,kDomesticBudgetBranchBytes,6) ||
       ga::mcmp(image,GA_1EE815,0,kDomesticCostLoadBytes,6) || ga::mcmp(image,GA_304D70,0,population_entry,16))return false;
    for (unsigned pair : {GA(GA_1347518), GA(GA_1346C98), GA(GA_134E518), GA(GA_134DF20)}) {
        unsigned original = pair == GA(GA_1347518) ? GA(GA_201C0) : pair == GA(GA_1346C98) ? GA(GA_1F380) : pair == GA(GA_134E518) ? GA(GA_69AB0) : GA(GA_61C90);
        if (pointer(image, pair) != image + original) return false;
    }
    if(pointer(image,GA(GA_1349E98))!=image+GA(GA_36E80) || pointer(image,GA(GA_134F918))!=image+GA(GA_7B0B0))return false;
    Obj relay = allocate_near(call);
    if (!relay) return false;
    Obj combat_relay=allocate_near(image+GA(GA_16AEF4),0x2000);
    if(!combat_relay) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    absolute_jump(combat_relay,reinterpret_cast<void *>(&combat_pair_scores));
    absolute_jump(combat_relay+1536,reinterpret_cast<void *>(&combat_apply_damage));
    memcpy(combat_relay+1568,kDamageEntryBytes,5);
    absolute_jump(combat_relay+1573,image+GA(GA_175955));
    original_damage=reinterpret_cast<ApplyDamage>(combat_relay+1568);
    absolute_jump(combat_relay+32,reinterpret_cast<void *>(&contextual_adjacency));
    // All five stolen bytes are complete, position-independent PUSHs. The
    // trampoline executes those and rejoins before SUB RSP at native +5.
    memcpy(combat_relay+64,image+GA(GA_2B2A90),5); // displaced PUSHes of the running build (push r15 / push r14), matched by the guard above
    absolute_jump(combat_relay+69,image+GA(GA_2B2A95));
    original_adjacency=reinterpret_cast<Adjacency>(combat_relay+64);
    absolute_jump(combat_relay+96,reinterpret_cast<void *>(&build_permission));
    memcpy(combat_relay+128,kBuildPermissionEntryBytes,5);
    absolute_jump(combat_relay+133,image+GA(GA_27AAC5));
    original_build_permission=reinterpret_cast<BuildPermission>(combat_relay+128);
    absolute_jump(combat_relay+160,reinterpret_cast<void *>(&domestic_tick));
    memcpy(combat_relay+192,kDomesticTickEntryBytes,5);
    absolute_jump(combat_relay+197,image+GA(GA_297075));
    original_domestic_tick=reinterpret_cast<DomesticTick>(combat_relay+192);
    unsigned domestic_bridge_size=make_domestic_cost_bridge(combat_relay+224);
    CodeLink pair_link,damage_link,adj_link,build_link,tick_link,cost_link,budget_link,population_link;
    absolute_jump(combat_relay+3840,reinterpret_cast<void *>(&population_growth));
    memcpy(combat_relay+3872,population_entry,5);absolute_jump(combat_relay+3877,image+GA(GA_304D75));
    original_population_growth=reinterpret_cast<PopulationGrowth>(combat_relay+3872);
    if(!prepare_link(population_link,GA(GA_304D70),0xE9,combat_relay+3840)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#ifdef SORTIE_FORMATION
    SortieCode sortie_code;
    if(!prepare_sortie(sortie_code,combat_relay)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#endif
#ifdef RECRUITMENT_UI_CANDIDATE
    RecruitAlertCode recruit_alert_code;
    if(!prepare_recruit_alert(recruit_alert_code,combat_relay)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#endif
#ifdef AWARD_WAREHOUSE
    AwardCode award_code;
    if(!prepare_awards(award_code,combat_relay)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#endif
#ifdef INJURY_PROTECTION
    InjuryCode injury_code;
    if(!prepare_injury(injury_code,combat_relay)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#endif
#ifdef MERIT_CANDIDATE
    MeritCode merit_code;
    if(!prepare_merit(merit_code,combat_relay)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#endif
#if defined(PERSONNEL_CANDIDATE) || defined(HIDDEN_LOYALTY)
    CodeLink loyalty_numeric,loyalty_text;
    const unsigned char raw_loyalty_pointer[4]={0x48,0x89,0xD0,0x90};
    const unsigned char no_loyalty_clamp[3]={0x90,0x90,0x90};
    if(!prepare_bytes(loyalty_numeric,GA(GA_34CB96),raw_loyalty_pointer,4) ||
       !prepare_bytes(loyalty_text,GA(GA_35EF25),no_loyalty_clamp,3)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
#endif
    const unsigned char no_budget_check[6]={0x90,0x90,0x90,0x90,0x90,0x90};
    if(!prepare_link(pair_link,GA(GA_16AEF4),0xE8,combat_relay) ||
       !prepare_link(damage_link,GA(GA_175950),0xE9,combat_relay+1536) ||
       !prepare_link(adj_link,GA(GA_2B2A90),0xE9,combat_relay+32) ||
       !prepare_link(build_link,GA(GA_27AAC0),0xE9,combat_relay+96) ||
       !prepare_link(tick_link,GA(GA_297070),0xE9,combat_relay+160) ||
       !prepare_link(cost_link,GA(GA_1EE815),0xE8,combat_relay+224) ||
       !prepare_bytes(budget_link,GA(GA_36F83),no_budget_check,6)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
    // Replace the complete six-byte RIP-relative MOV with CALL + NOP.
    cost_link.modified[GA(GA_1EE815)-cost_link.block+5]=0x90;
    DWORD combat_old=0;
    if(!VirtualProtect(combat_relay,0x2000,PAGE_EXECUTE_READ,&combat_old)) {
        VirtualFree(relay,0,MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE);return false;
    }
    // Population trampoline ends at byte 3891; flush the entire allocated
    // relay page, including that monthly-only path and future additions.
    unsigned relay_used=224+domestic_bridge_size;
    if(relay_used<0x2000)relay_used=0x2000;
#ifdef RECRUITMENT_UI_CANDIDATE
    if(relay_used<3040)relay_used=3040;
#endif
#ifdef SORTIE_FORMATION
    if(relay_used<2944)relay_used=2944;
#endif
#ifdef AWARD_WAREHOUSE
    if(relay_used<2432)relay_used=2432;
#endif
#ifdef INJURY_PROTECTION
    if(relay_used<1664)relay_used=1664;
#endif
    if(relay_used<1587)relay_used=1587;
#ifdef MERIT_CANDIDATE
    if(relay_used<1152)relay_used=1152;
#endif
    FlushInstructionCache(GetCurrentProcess(),combat_relay,relay_used);
    /* FF25 [RIP+0] jumps without clobbering argument or return registers. */
    relay[0] = 0xFF; relay[1] = 0x25; memset(relay + 2, 0, 4);
    *reinterpret_cast<void **>(relay + 6) = reinterpret_cast<void *>(&cut_player_predicate);
    DWORD old = 0;
    if (!VirtualProtect(relay, 0x1000, PAGE_EXECUTE_READ, &old)) {
        VirtualFree(relay, 0, MEM_RELEASE);VirtualFree(combat_relay,0,MEM_RELEASE); return false;
    }
    FlushInstructionCache(GetCurrentProcess(), relay, 14);
    intptr_t delta = reinterpret_cast<intptr_t>(relay) - reinterpret_cast<intptr_t>(call + 5);
    if (delta < INT32_MIN || delta > INT32_MAX) { VirtualFree(relay, 0, MEM_RELEASE); return false; }
    unsigned char original_block[16], modified_block[16];
    memcpy(original_block, image + GA(GA_D3510), 16); memcpy(modified_block, original_block, 16);
    modified_block[6] = 0xE8; memcpy(modified_block + 7, &delta, 4);
    if (!replace_call(original_block, modified_block)) { VirtualFree(relay, 0, MEM_RELEASE); return false; }
    if (!publish_link(adj_link) || !publish_link(pair_link) || !publish_link(damage_link) || !publish_link(build_link) ||
        !publish_link(tick_link) || !publish_link(cost_link) || !publish_link(budget_link) || !publish_link(population_link) ||
        !patch_pointer(GA(GA_1347518), GA(GA_201C0), reinterpret_cast<void *>(&first_army_node)) ||
        !patch_pointer(GA(GA_1346C98), GA(GA_1F380), reinterpret_cast<void *>(&cut_node)) ||
        !patch_pointer(GA(GA_134E518), GA(GA_69AB0), reinterpret_cast<void *>(&city_node)) ||
        !patch_pointer(GA(GA_134DF20), GA(GA_61C90), reinterpret_cast<void *>(&retreat_node)) ||
        !patch_pointer(GA(GA_1349E98), GA(GA_36E80), reinterpret_cast<void *>(&domestic_officer_node)) ||
        !patch_pointer(GA(GA_134F918), GA(GA_7B0B0), reinterpret_cast<void *>(&domestic_selection_node))
#ifdef MERIT_CANDIDATE
        || !publish_merit(merit_code)
#endif
#ifdef RECRUITMENT_UI_CANDIDATE
        || !publish_recruit_ui() || !publish_recruit_ai() || !publish_recruit_alert(recruit_alert_code)
#endif
#ifdef INJURY_PROTECTION
        || !publish_injury(injury_code,true,true)
#endif
#ifdef AWARD_WAREHOUSE
        || !publish_awards(award_code)
#endif
#ifdef SORTIE_FORMATION
        || !publish_sortie(sortie_code)
#endif
#if defined(PERSONNEL_CANDIDATE) || defined(HIDDEN_LOYALTY)
        || !publish_link(loyalty_numeric) || !publish_link(loyalty_text)
#endif
#ifdef PERSONNEL_CANDIDATE
        || !publish_personnel_nodes()
#endif
        ) {
#ifdef AWARD_WAREHOUSE
        restore_awards(award_code);
#endif
#ifdef SORTIE_FORMATION
        restore_sortie(sortie_code);
#endif
#ifdef INJURY_PROTECTION
        restore_injury(injury_code);
#endif
#ifdef RECRUITMENT_UI_CANDIDATE
        restore_recruit_alert(recruit_alert_code);
        restore_recruit_ai();
        restore_recruit_ui();
#endif
#ifdef MERIT_CANDIDATE
        restore_merit(merit_code);
#endif
#ifdef PERSONNEL_CANDIDATE
        restore_personnel_nodes();
#endif
#if defined(PERSONNEL_CANDIDATE) || defined(HIDDEN_LOYALTY)
        restore_link(loyalty_numeric);restore_link(loyalty_text);
#endif
        restore_pointer(GA(GA_1347518), GA(GA_201C0), reinterpret_cast<void *>(&first_army_node));
        restore_pointer(GA(GA_1346C98), GA(GA_1F380), reinterpret_cast<void *>(&cut_node));
        restore_pointer(GA(GA_134E518), GA(GA_69AB0), reinterpret_cast<void *>(&city_node));
        restore_pointer(GA(GA_134DF20), GA(GA_61C90), reinterpret_cast<void *>(&retreat_node));
        restore_pointer(GA(GA_1349E98), GA(GA_36E80), reinterpret_cast<void *>(&domestic_officer_node));
        restore_pointer(GA(GA_134F918), GA(GA_7B0B0), reinterpret_cast<void *>(&domestic_selection_node));
        replace_call(modified_block, original_block);
        restore_link(pair_link);restore_link(damage_link);restore_link(adj_link);restore_link(build_link);
        restore_link(tick_link);restore_link(cost_link);restore_link(budget_link);
        restore_link(population_link);
        /* Keep the tiny relay allocated: another thread could already be
         * returning through it. With policies disabled it invokes native code. */
        return false;
    }
    InterlockedExchange(&enabled, 1);
#ifdef RECRUITMENT_UI_CANDIDATE
    // Optional and independently guarded: rejection leaves the original game
    // functions and the already validated integrated features operational.
    if(mod_features.distance)install_progress_native();
    // Native interception/cancellation runs unchanged; no target-retreat patch.
#endif
    return true;
}
