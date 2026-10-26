// Two disjoint owners. Keep legacy integrated fixtures/builds unchanged.
// owner: 0 shared, 1 unified personnel UI, 2 auto domestic ([features] keys).
enum : unsigned char {kPointerShared,kPointerUnified,kPointerDomestic,kPointerHud};
struct SplitPointer {unsigned slot,original;void *replacement;unsigned char owner;};
#if !defined(SPLIT_COMBAT)
// Officer roster: the setup call inside 8DF7E0, and the entry of the list
// builder 2C0820. (Its call inside 5CC120 sits at 5CC37E, which crosses a
// 16-byte block and cannot be swapped atomically, so we gate the entry and
// pass every other caller straight through.)
static const unsigned char info_setup_call[]={0xE8,0x1B,0xC8,0xCE,0xFF};
static const unsigned char info_list_entry[]={0x40,0x55,0x56,0x57,0x41,0x54}; // push rbp/rsi/rdi/r12
static bool info_guards_match() {
    return !memcmp(image+0x8DF900,info_setup_call,5) && !memcmp(image+0x2C0820,info_list_entry,6) &&
        pointer(image,0x14AB2C0+0x288)==image+0x8DF7E0 && pointer(image,0x14AB2C0+0x250)==image+0x5CC120;
}
static bool prepare_info(CodeLink *links,Obj relay) {
    absolute_jump(relay+3200,reinterpret_cast<void *>(&unified_native::info_setup));
    absolute_jump(relay+3232,reinterpret_cast<void *>(&unified_native::info_list_create));
    memcpy(relay+3264,info_list_entry,6);absolute_jump(relay+3270,image+0x2C0826);
    unified_native::info_list_original=reinterpret_cast<unified_native::InfoListCreate>(relay+3264);
    return prepare_link(links[0],0x8DF900,0xE8,relay+3200) && prepare_link(links[1],0x2C0820,0xE9,relay+3232);
}
#endif
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
#include "native_modpack.inl"
#endif
// AI-CONTRACT [ownership]: Each original slot/site has one DLL owner. Check this table and conditional CodeLink paths together before adding a hook; shared helper code does not authorize a second publisher.
static const SplitPointer split_pointers[]={
#ifdef SPLIT_COMBAT
    {0x1344518,0x201E0,reinterpret_cast<void *>(&first_army_node)},
    {0x1343C98,0x1F3A0,reinterpret_cast<void *>(&cut_node)},
    {0x134B4F8,0x68CE0,reinterpret_cast<void *>(&city_node)},
    {0x134AF00,0x61120,reinterpret_cast<void *>(&retreat_node)},
    {0x13512A0,0x8E7C0,reinterpret_cast<void *>(&person_info_node)},   // ai_max_aggression trait reads
    {0x134D248,0x7DC00,reinterpret_cast<void *>(&foothold_strength_node)},   // attack_recheck_fix #10114
    {0x13517A8,0x91030,reinterpret_cast<void *>(&variable_minus_node)},      // attack_recheck_fix #10115
#else
    {0x1458848,0x7DF540,reinterpret_cast<void *>(&tactic_priority::update),kPointerUnified},
    {0x1458830,0x7D9E50,reinterpret_cast<void *>(&tactic_priority::leave),kPointerUnified},
    {0x14751D8,0x894420,reinterpret_cast<void *>(&unified_native::frame),kPointerHud},
    {0x14751F0,0x8782E0,reinterpret_cast<void *>(&unified_native::leave),kPointerHud},
    {0x13AC588,0x2FB440,reinterpret_cast<void *>(&unified_dialogs::frame),kPointerUnified},
    {0x13AC5B0,0x258170,reinterpret_cast<void *>(&unified_dialogs::controls),kPointerUnified},
    {0x13AC578,0x256970,reinterpret_cast<void *>(&unified_dialogs::destroy),kPointerUnified},
    {0x14AB2D0,0x2FB440,reinterpret_cast<void *>(&unified_native::info_frame),kPointerUnified},
    {0x14AB2C0,0x8C6150,reinterpret_cast<void *>(&unified_native::info_destroy),kPointerUnified},
    {0x14AB2E8,0x8FD320,reinterpret_cast<void *>(&unified_native::info_leave),kPointerUnified},
    {0x1346E70,0x36320,reinterpret_cast<void *>(&domestic_officer_node),kPointerDomestic},
    {0x134C8F8,0x7A110,reinterpret_cast<void *>(&domestic_selection_node),kPointerDomestic},
#endif
};
struct SplitInstallState {
    Obj relay;
    CodeLink core[6];unsigned count;
    bool prepared,published,attempted;
#ifdef SPLIT_COMBAT
    SortieCode sortie;
#else
    MeritCode merit;
    RecruitAlertCode alert;
    InjuryCode injury;
    AwardCode awards;
    CodeLink loyalty[2];
    ExpertCode expert;
    bool save_ready; // merit/warehouse sidecar has its own guard, independent of merit gameplay
    bool info_ready;
    CodeLink info[2]; // officer roster: 8DF900 -> 5CC120, 5CC37E -> 2C0820
    bool intercept_ready;
    CodeLink intercept; // 1EC890 entry -> district_intercept::dispatch (only without the combat module)
    bool safety_ready;
    CodeLink safety;    // C2F73 CALL AEE60 (condition 84) -> district_intercept::transport_safety
#endif
};
static SplitInstallState split_install;
#ifdef SPLIT_MODULE_FIXTURE
static unsigned split_fixture_fail_after;
static unsigned split_fixture_disabled;
#endif
static bool split_core_enabled(unsigned i);
static bool split_pointer_enabled(const SplitPointer &p);
#ifndef SPLIT_COMBAT
// Stable diagnostic feature IDs (no new settings): 1 battle_rules, 2 domestic,
// 3 domestic_free, 4 population, 5 unified, 6 merit, 7 save sidecar, 8 recruit,
// 9 recruit_ai, 10 injury, 11 herbal, 12 awards, 13 loyalty, 14 stats200,
// 15 growth, 16 tooltip_collab, 17 scenery, 18 deaths, 19 traits9, 20 captive.
static void split_disable(bool &feature,unsigned id) {
    if(feature) {feature=false;log_event("feature_guard_mismatch",id);}
}
// AI-CONTRACT [compatibility]: New hook sites need feature-specific original-byte guards and a unique split_disable diagnostic ID. A mismatch disables that feature; do not replace these checks with a whole-image CRC requirement.
static void split_feature_guards() {
    auto &f=mod_features;auto &s=split_install;
    if(!f.convenience || f.trace_original)return;
    if(f.battle_rules && (memcmp(image+0x169F44,kPairCallBytes,5) ||
        memcmp(image+0x2B07C0,kAdjacencyEntryBytes,5) || !expert_guards_match()))split_disable(f.battle_rules,1);
    if((f.domestic || f.awards || f.injury || f.injury_herbal) && memcmp(image+0x295160,kDomesticTickEntryBytes,5)) {
        split_disable(f.domestic,2);split_disable(f.awards,12);
        split_disable(f.injury,10);split_disable(f.injury_herbal,11);
    }
    if(f.domestic_free && (memcmp(image+0x36423,kDomesticBudgetBranchBytes,6) ||
        memcmp(image+0x1ED475,kDomesticCostLoadBytes,6)))split_disable(f.domestic_free,3);
    if(f.population && memcmp(image+0x302980,population_entry,16))split_disable(f.population,4);
    for(const auto &p:split_pointers)if(split_pointer_enabled(p) && pointer(image,p.slot)!=image+p.original) {
        if(p.owner==kPointerUnified)split_disable(f.unified,5);
        else if(p.owner==kPointerHud){split_disable(f.unified,5);split_disable(f.advisor_briefing,26);}
        else if(p.owner==kPointerDomestic)split_disable(f.domestic,2);
    }
    if(s.save_ready && !merit_save_guards_match()) {
        split_disable(s.save_ready,7);split_disable(f.merit,6);split_disable(f.awards,12);
    }
    if(f.merit && !merit_gameplay_guards_match())split_disable(f.merit,6);
    if(f.recruit && (!recruit_ui_guards_match() || !recruit_alert_guards_match()))split_disable(f.recruit,8);
    if(f.recruit_ai && !recruit_ai_guards_match())split_disable(f.recruit_ai,9);
    if(f.injury && !injury_guards_match(true,false))split_disable(f.injury,10);
    if(f.injury_herbal && !injury_guards_match(false,true))split_disable(f.injury_herbal,11);
    if(f.awards && !award_guards_match())split_disable(f.awards,12);
    if(f.loyalty && !loyalty_guards_match())split_disable(f.loyalty,13);
    // split_disable code 21 reserved: strategy_batch removed (2026-10-25, screen corruption).
    if(f.area_index && !area_index_guards_match())split_disable(f.area_index,22); // area count entry, helpers and callers
    if(f.path_fast && !path_fast_guards_match())split_disable(f.path_fast,23); // 183A30, helpers, body clear site and buffer ownership
    if((f.diplomacy_overinvest || f.diplomacy_counterturn) && !diplomacy_intel_guards_match()) {
        split_disable(f.diplomacy_overinvest,24);split_disable(f.diplomacy_counterturn,25);
    }
    if(f.advisor_briefing && !advisor_briefing_guards_match())split_disable(f.advisor_briefing,26);
#ifndef SPLIT_MODULE_FIXTURE
    if(!text_exact) {
        // Imported installers do not expose all original-site fingerprints.
        split_disable(f.tooltip_collab,16);split_disable(f.battle_deaths,18);
    }
    if(f.stats200 && !modpack_guards_match(true,false,false,false))split_disable(f.stats200,14);
    if(f.stats200_growth && !modpack_guards_match(false,true,false,false))split_disable(f.stats200_growth,15);
    if(f.tooltip_collab && !modpack_guards_match(false,false,true,false))split_disable(f.tooltip_collab,16);
    if(f.scenery_skip && !modpack_guards_match(false,false,false,true))split_disable(f.scenery_skip,17);
    if(f.traits9 && !traits9_guards_match(true,false))split_disable(f.traits9,19);
    if(f.captive_dialog && !traits9_guards_match(false,true))split_disable(f.captive_dialog,20);
#endif
    f.domestic_free=f.domestic && f.domestic_free;f.domestic_direct=f.domestic && f.domestic_direct;
    f.awards_ai=f.awards && f.awards_ai;f.stats200_growth=f.stats200 && f.stats200_growth;
}
#endif
static bool split_guards_match() {
#ifdef SPLIT_COMBAT
    for(const auto &p:split_pointers)if(pointer(image,p.slot)!=image+p.original)return false;
    return !memcmp(image+0xD2486,kCutCallBytes,5) && !memcmp(image+0x1749A0,kDamageEntryBytes,5) &&
        !memcmp(image+0x279190,kBuildPermissionEntryBytes,5) && sortie_guards_match();
#else
    split_feature_guards();return true; // an empty feature set is a successful no-op
#endif
}
static void split_rollback() {
    InterlockedExchange(&enabled,0);
    if(!split_install.attempted)return;
#ifdef SPLIT_COMBAT
    restore_sortie(split_install.sortie);
#else
    restore_link(split_install.safety);restore_link(split_install.intercept);
    restore_link(split_install.info[1]);restore_link(split_install.info[0]);
#ifndef SPLIT_MODULE_FIXTURE
    restore_traits9();restore_modpack();
#endif
    restore_expert(split_install.expert);
    restore_awards(split_install.awards);restore_injury(split_install.injury);
    restore_recruit_alert(split_install.alert);restore_recruit_ai();restore_recruit_ui();
    restore_merit(split_install.merit);
    for(auto &l:split_install.loyalty)restore_link(l);
#endif
    for(const auto &p:split_pointers)restore_pointer(p.slot,p.original,p.replacement);
    for(unsigned i=split_install.count;i>0;--i)restore_link(split_install.core[i-1]);
    split_install.published=false;
    // Retain any gate that another native thread could have entered.
}
static bool split_prepare() {
    if(split_install.prepared)return false;
    load_mod_features();
#ifdef SPLIT_MODULE_FIXTURE
    if(split_fixture_fail_after==1000)return false; // allocation/preparation failure
#ifndef SPLIT_COMBAT
    if(split_fixture_disabled&1)mod_features.unified=false;
    if(split_fixture_disabled&2)mod_features.merit=false;
    if(split_fixture_disabled&4)mod_features.convenience=false;
    if(split_fixture_disabled&8)mod_features.advisor_briefing=true; // fixture-only independent HUD owner case
#endif
#endif
#ifndef SPLIT_COMBAT
    split_install.save_ready=mod_features.convenience && !mod_features.trace_original;
#endif
    if(!split_guards_match())return false;
    auto &s=split_install;
    s.relay=allocate_near(image+0x169F44);if(!s.relay)return false;
    Obj r=s.relay;bool ok;
#ifdef SPLIT_COMBAT
    absolute_jump(r,reinterpret_cast<void *>(&cut_player_predicate));
    absolute_jump(r+96,reinterpret_cast<void *>(&build_permission));
    memcpy(r+128,kBuildPermissionEntryBytes,5);absolute_jump(r+133,image+0x279195);
    original_build_permission=reinterpret_cast<BuildPermission>(r+128);
    absolute_jump(r+1536,reinterpret_cast<void *>(&combat_apply_damage));
    memcpy(r+1568,kDamageEntryBytes,5);absolute_jump(r+1573,image+0x1749A5);
    original_damage=reinterpret_cast<ApplyDamage>(r+1568);
    s.count=3;
    ok=prepare_link(s.core[0],0xD2486,0xE8,r) &&
       prepare_link(s.core[1],0x1749A0,0xE9,r+1536) &&
       prepare_link(s.core[2],0x279190,0xE9,r+96) && prepare_sortie(s.sortie,r);
#else
    // [groups] convenience=0: coordinate the combat module only, publish no own hook.
    const bool trace_only=mod_features.trace_original; // [diagnostics] ai_trace_original: no feature hook
    if(trace_only || !mod_features.convenience) {s.count=0;ok=true;} else {
    tactic_priority::load();
    absolute_jump(r,reinterpret_cast<void *>(&combat_pair_scores));
    absolute_jump(r+32,reinterpret_cast<void *>(&contextual_adjacency));
    memcpy(r+64,kAdjacencyEntryBytes,5);absolute_jump(r+69,image+0x2B07C5);
    original_adjacency=reinterpret_cast<Adjacency>(r+64);
    absolute_jump(r+160,reinterpret_cast<void *>(&domestic_tick));
    memcpy(r+192,kDomesticTickEntryBytes,5);absolute_jump(r+197,image+0x295165);
    original_domestic_tick=reinterpret_cast<DomesticTick>(r+192);
    make_domestic_cost_bridge(r+224);
    absolute_jump(r+3840,reinterpret_cast<void *>(&population_growth));
    memcpy(r+3872,population_entry,5);absolute_jump(r+3877,image+0x302985);
    original_population_growth=reinterpret_cast<PopulationGrowth>(r+3872);
    const unsigned char no_budget[]={0x90,0x90,0x90,0x90,0x90,0x90};
    const unsigned char numeric[]={0x48,0x89,0xD0,0x90},text[]={0x90,0x90,0x90};
    s.count=6;
    const auto &f=mod_features;
    ok=(!split_core_enabled(0) || prepare_link(s.core[0],0x169F44,0xE8,r)) &&
       (!split_core_enabled(1) || prepare_link(s.core[1],0x2B07C0,0xE9,r+32)) &&
       (!split_core_enabled(2) || prepare_link(s.core[2],0x295160,0xE9,r+160)) &&
       (!split_core_enabled(3) || prepare_link(s.core[3],0x1ED475,0xE8,r+224)) &&
       (!split_core_enabled(4) || prepare_bytes(s.core[4],0x36423,no_budget,6)) &&
       (!split_core_enabled(5) || prepare_link(s.core[5],0x302980,0xE9,r+3840)) &&
       (!s.save_ready || prepare_merit(s.merit,r)) && (!f.recruit || prepare_recruit_alert(s.alert,r)) &&
       (!(f.injury || f.injury_herbal) || prepare_injury(s.injury,r)) &&
       (!f.awards || prepare_awards(s.awards,r)) &&
       (!f.loyalty || (prepare_bytes(s.loyalty[0],0x34A8D6,numeric,4) && prepare_bytes(s.loyalty[1],0x35CA35,text,3))) &&
       (!f.battle_rules || prepare_expert(s.expert))
#ifndef SPLIT_MODULE_FIXTURE
       && prepare_modpack() && (!(f.traits9 || f.captive_dialog) || prepare_traits9())
#endif
       ;
    // The roster panel is optional: if its sites differ, keep every other feature.
    s.info_ready=ok && mod_features.unified && info_guards_match() && prepare_info(s.info,r);
    // Optional like the roster. Skipped when the combat module owns 1EC890 (it applies the policy).
    s.intercept_ready=ok && mod_features.district_intercept_block && district_intercept::prepare(s.intercept,r);
    s.safety_ready=ok && mod_features.district_intercept_block && district_intercept::prepare_safety(s.safety,r);
    if(ok && split_core_enabled(3))s.core[3].modified[0x1ED475-s.core[3].block+5]=0x90;
    }
#endif
    DWORD old=0;
    if(!ok || !VirtualProtect(r,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),r,4096)) {
#ifdef SPLIT_BASE
        if(s.expert.relay)VirtualFree(s.expert.relay,0,MEM_RELEASE);
#endif
        VirtualFree(r,0,MEM_RELEASE);s={};return false;
    }
    s.prepared=true;return true;
}
// Which prepared core link each switch owns (combat module: all three).
static bool split_core_enabled(unsigned i) {
#ifdef SPLIT_COMBAT
    (void)i;return true;
#else
    const auto &f=mod_features;
    switch(i) {
    case 0:case 1:return f.battle_rules;                    // pair scope + adjacency
    case 2:return f.domestic || f.awards || f.injury || f.injury_herbal; // domestic tick carries all of them
    case 3:case 4:return f.domestic_free;                   // auto-domestic free cost bridge, budget branch
    default:return f.population;
    }
#endif
}
static bool split_pointer_enabled(const SplitPointer &p) {
#ifdef SPLIT_COMBAT
    (void)p;return true;
#else
    if(!mod_features.convenience || mod_features.trace_original)return false;
    return p.owner==kPointerUnified?mod_features.unified:p.owner==kPointerDomestic?mod_features.domestic:
        p.owner==kPointerHud?(mod_features.unified || mod_features.advisor_briefing):true;
#endif
}
static bool split_publish() {
    auto &s=split_install;
    if(!s.prepared || s.attempted || !split_guards_match())return false;
    s.attempted=true;
    for(unsigned i=0;i<s.count;++i) {
        if(!split_core_enabled(i))continue;
        if(!publish_link(s.core[i])) {split_rollback();return false;}
#ifdef SPLIT_MODULE_FIXTURE
        if(split_fixture_fail_after==i+1) {split_rollback();return false;}
#endif
    }
    for(const auto &p:split_pointers) {
        if(!split_pointer_enabled(p))continue;
        if(!patch_pointer(p.slot,p.original,p.replacement)) {split_rollback();return false;}
    }
#ifdef SPLIT_COMBAT
    bool ok=publish_sortie(s.sortie);
#else
    const auto &f=mod_features;
    // Merit save links/hosts also carry the warehouse sidecar: always published.
    bool ok=!f.convenience || s.count==0 || ((!s.save_ready || publish_merit(s.merit)) &&
        (!f.recruit || (publish_recruit_ui() && publish_recruit_alert(s.alert))) &&
        (!f.recruit_ai || publish_recruit_ai()) &&   // v12.8: AI limit shares only pure judgment helpers
        publish_injury(s.injury,f.injury,f.injury_herbal) && (!f.awards || publish_awards(s.awards)) &&
        (!f.loyalty || (publish_link(s.loyalty[0]) && publish_link(s.loyalty[1]))) &&
        (!f.battle_rules || publish_expert(s.expert))
#ifndef SPLIT_MODULE_FIXTURE
        && publish_modpack() && publish_traits9(f.traits9,f.captive_dialog) && activate_modpack()
#endif
        );
    if(!f.unified || (s.info_ready && !info_guards_match()))s.info_ready=false;
    if(ok && s.info_ready && !(publish_link(s.info[0]) && publish_link(s.info[1]))) {
        restore_link(s.info[1]);restore_link(s.info[0]);s.info[0].published=s.info[1].published=false;s.info_ready=false;
    }
    if(ok && s.intercept_ready && !publish_link(s.intercept))s.intercept_ready=false;
    if(ok && s.safety_ready && !publish_link(s.safety))s.safety_ready=false;
    district_intercept::diag_line(s.intercept_ready?"0,0,entry_hook_published\r\n":"0,0,entry_hook_not_published\r\n");
#endif
    if(!ok) {split_rollback();return false;}
    s.published=true;return true;
}
static void split_activate() {
    if(!split_install.published)return;
    InterlockedExchange(&enabled,1);
#ifdef SPLIT_COMBAT
    // Native interception/cancellation runs unchanged; no target-retreat patch.
    log_event("combat_module_hooks_installed");
#else
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
    if(mod_features.trace_original) {ai_trace::install(true); return;} // original AI, observed only
#endif
    if(mod_features.distance)install_progress_native();
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
    if(mod_features.name_lookup)install_name_lookup(); // optional; rejection keeps the original E09910
#endif
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
    if(mod_features.speed_buttons)speed_buttons::install(); // optional; needs the ScreenLayout patch for its buttons
    layout_wait::install(); // layout_keep (default on) and/or [diagnostics] layout_wait
    gpu_profile_start(self_module); // [diagnostics] gpu_profile=1 only; observation-only GPU timers
    if(mod_features.fast_save)save_codec_hook::install(); // falls back to the original encoder per save
    if(mod_features.occupy_fast)occupy_fast::install(); // auto-capture presentation waits about halved
    if(mod_features.caravan_stay)caravan_stay::install(); // trade members keep their own city
    if(mod_features.stat_exp)stat_exp::install(); // pentagon chart "/N%" stat experience
    if(mod_features.dispatch_days)dispatch_days::install(); // dispatch list "N일" until back
    if(mod_features.fast_menu || mod_features.lord_change)fast_menu::install(); // map state busy slot (+ lord_change status fix)
    if(mod_features.area_index)area_index::install();
    if(mod_features.path_fast)path_fast::install(); // sole 183A30 owner if installed
#ifndef NO_RUNTIME_LOG
    if(path_fast::active)log_event("path_count_skipped_path_fast");
    else path_count::install(progress_native_active && progress_native_profile,[]() {return progress_sample!=nullptr;});
#endif
    if(mod_features.appear_batch)appear_batch::install(); // CAppearArmyState: several armies per frame (0-day wait)
    if(mod_features.face_keep)face_keep::install(); // army flag commander faces keep one extra image reference
    if(mod_features.mediation_plus)mediation_plus::install(); // sworn join, polygamy, divorce
    if(mod_features.diplomacy_overinvest || mod_features.diplomacy_counterturn)diplomacy_intel::install();
    if(mod_features.advisor_briefing && !advisor_briefing::install())split_disable(mod_features.advisor_briefing,26);
    if(mod_features.drag_learn)drag_learn::install(); // several books x officers per learning command
    if(mod_features.lord_change)lord_change::install(); // new game: force select footer button 「세력내 군주교체」 (needs the ScreenLayout patch)
    if(mod_features.region_officers)region_officers::install(); // read-only native map portraits
    if(mod_features.building_destruction_all)building_destruction::install();
    if(mod_features.damage_preview)damage_forecast::install(); // target UI only; no damage application
    // The game never raises the timer resolution (no winmm import), so its Sleep(n)
    // and timed waits run at the default ~15.6 ms tick. Synchronous layout loads
    // (86C570) wait on such stages: ~25-30 ms each. Process-wide 1 ms request.
    if(mod_features.timer_1ms) {
        if(HMODULE winmm=LoadLibraryW(L"winmm.dll"))
            if(auto begin=reinterpret_cast<unsigned(__stdcall *)(unsigned)>(GetProcAddress(winmm,"timeBeginPeriod")))
                log_event("timer_1ms",begin(1)==0?1:0);
    }
#endif
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
    // Last, so the trace chains over every published mod hook of both modules.
#ifdef RUNTIME_COST_PROFILE
    if(costprofile::on)face_timing::install(); // [diagnostics] cost_profile: 1B91E0 face setter per call site
    if(costprofile::on)ai_trace::install(false,true); // [diagnostics] cost_profile: per-node cost of the original tree
    else if(mod_features.ai_trace)ai_trace::install(false);
#else
    if(mod_features.ai_trace)ai_trace::install(false);
#endif
#endif
    log_event("other_module_hooks_installed");
#endif
}
