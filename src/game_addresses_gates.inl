// Feature gates over the address table (user 2026-11-05 patch resilience, docs/PATCH_RESILIENCE.md principle 4).
// A feature whose source files use a table id that is unresolved in the running build (signature ambiguity, or a function the
// game update changed and the table therefore denies) is switched off BEFORE any of its hooks is prepared; the log gets
// addr_unresolved(feature code, unresolved ids of the build, 0). The original-byte guards still run afterwards.
// The file lists are the owners of the ids (tools/game addresses: ga_users, src file -> ids); infrastructure ids are listed
// explicitly. Codes 41.. are new (no setting): see docs/PATCH_RESILIENCE.md section "Implementation".
static bool ga_ids_known(std::initializer_list<unsigned> ids) {
    for(unsigned id:ids)if(!ga::known(id))return false;
    return true;
}
// Like ga::files_ok, but ids in `except` are ignored: a feature that does not use them stays on while another feature of the same
// source file (which does) is switched off through its own ids (B stage 2026-11-05: modpack stat-cap patches).
static bool ga_files_ok_except(std::initializer_list<unsigned> files,std::initializer_list<unsigned> except) {
    for(unsigned id=0;id<GA_COUNT;++id) {
        if(ga::known(id))continue;
        bool skip=false;
        for(unsigned x:except)if(x==id) {skip=true;break;}
        if(skip)continue;
        for(unsigned f:files)if((ga_users[id][f>>6]>>(f&63))&1ull)return false;
    }
    return true;
}
static void ga_gate(bool &flag,unsigned code,bool ok) {
    if(flag && !ok) {
        flag=false;
        log_event("addr_unresolved",code,ga::state().unresolved,ga::state().build<0?0u:static_cast<unsigned>(ga::state().build)+1u);
    }
}
#ifndef SPLIT_COMBAT
static void split_address_gates(ModFeatures &f) {
    if(!ga::state().unresolved)return;       // every id resolved (reference build): nothing to gate
    ga_gate(f.battle_rules,41,ga_ids_known({GA_169F44,GA_2B07C0,GA_2B07C5,GA_2B0B69}) &&
        ga::files_ok({GF_native_expert_links_inl,GF_native_expert_settings_inl,GF_native_adjacency_inl}));
    ga_gate(f.domestic,42,ga_ids_known({GA_BF7A40}) && ga::files_ok({GF_native_domestic_inl}));
    ga_gate(f.domestic_free,43,f.domestic);
    ga_gate(f.awards,44,ga_ids_known({GA_BF7A40}) && ga::files_ok({GF_native_awards_inl,GF_native_personnel_inl}));
    ga_gate(f.injury,45,ga_ids_known({GA_BF7A40}) && ga::files_ok({GF_native_injury_inl,GF_native_injury_links_inl,GF_native_personnel_inl}));
    ga_gate(f.injury_herbal,46,ga_ids_known({GA_BF7A40}) && ga::files_ok({GF_native_injury_inl,GF_native_injury_links_inl,GF_native_personnel_inl}));
    ga_gate(f.recruit,47,ga::files_ok({GF_native_recruit_ui_inl}));
    ga_gate(f.advisor_briefing,48,ga::files_ok({GF_advisor_briefing_profile_h,GF_native_advisor_briefing_inl}));
    ga_gate(f.damage_preview,49,ga::files_ok({GF_damage_preview_profile_h,GF_native_damage_preview_inl}));
    ga_gate(f.building_destruction_all,50,ga::files_ok({GF_building_destruction_profile_h,GF_native_building_destruction_inl}));
    ga_gate(f.diplomacy_overinvest,51,ga::files_ok({GF_diplomacy_intel_profile_h,GF_native_diplomacy_intel_inl}));
    ga_gate(f.diplomacy_counterturn,52,ga::files_ok({GF_diplomacy_intel_profile_h,GF_native_diplomacy_intel_inl}));
    // The stat-cap byte patches (modpack_bytes rows) belong to stats200/stats200_growth only; tooltip_collab, scenery_skip and
    // battle_deaths never touch them. 230098 (item-0x7b clamp): 1a4a447d uses the relay form (native_modpack.inl modpack_clamp_short, local10).
    // The 5 battle_deaths ids (GA_259682..GA_295C5F) are also attributed to native_modpack.inl through the pre-rebase backup source
    // (build/rebase_logs/src_before_rebase, docs/BATTLE_DEATHS_PLAN.md 3.4): they must not switch the stat-cap/collab/scenery features off.
    const std::initializer_list<unsigned> deaths_ids={GA_259682,GA_27D976,GA_25974B,GA_27DA09,GA_295C5F};
    const bool modpack=ga_files_ok_except({GF_modpack_tables_h,GF_native_modpack_inl},deaths_ids);
    const bool modpack_core=ga_files_ok_except({GF_modpack_tables_h,GF_native_modpack_inl},{GA_2253FA,GA_22544F,GA_2293F6,GA_229469,GA_23AAA8,GA_23AB06,GA_20A425,GA_20ABB2,GA_5DD4B8,GA_142CBE0,GA_5DD323,GA_31FECE,GA_230098,GA_2B9DCA,GA_259682,GA_27D976,GA_25974B,GA_27DA09,GA_295C5F});
    ga_gate(f.stats200,53,modpack);ga_gate(f.stats200_growth,54,modpack);ga_gate(f.tooltip_collab,55,modpack_core);
    ga_gate(f.scenery_skip,56,modpack_core);ga_gate(f.battle_deaths,57,ga::files_ok({GF_native_battle_deaths_inl,GF_battle_deaths_profile_h}));
    ga_gate(f.lord_change,58,ga::files_ok({GF_native_lord_change_inl}));
    ga_gate(f.mediation_plus,59,ga::files_ok({GF_native_mediation_plus_inl}));
    ga_gate(f.occupy_fast,60,ga::files_ok({GF_native_occupy_fast_inl}));
    ga_gate(f.speed_buttons,61,ga::files_ok({GF_native_speed_buttons_inl}));
    ga_gate(f.stat_exp,62,ga::files_ok({GF_native_stat_exp_inl}));
    ga_gate(f.dispatch_days,63,ga::files_ok({GF_native_dispatch_days_inl}));
    ga_gate(f.path_fast,64,ga::files_ok({GF_native_path_fast_inl}));
    ga_gate(f.region_officers,65,ga::files_ok({GF_native_region_officers_inl}));
    ga_gate(f.terrain_edit,66,ga::files_ok({GF_native_terrain_edit_ui_inl}));
    ga_gate(f.merit,67,ga::files_ok({GF_native_merit_scroll_inl}));
    ga_gate(f.district_intercept_block,68,ga::files_ok({GF_native_district_intercept_link_inl}));
    ga_gate(f.ccz_sprites,69,ga::files_ok({GF_native_ccz_sprites_inl,GF_native_ccz_title_inl,GF_native_ccz_editor_inl}));
    ga_gate(f.ccz_block,70,ga::files_ok({GF_native_ccz_bridge_inl}));
    ga_gate(f.ccz_critical,71,ga::files_ok({GF_native_ccz_bridge_inl}));
    // 73 (local10): traits9 manifests/core patches use table ids of native_traits9.inl; an unresolved one switches the nine slots off here
    ga_gate(f.traits9,73,ga::files_ok({GF_native_traits9_inl}));
    f.stats200_growth=f.stats200 && f.stats200_growth;
}
#else
// Combat module: the sortie / campaign family (planner hooks, call-site redirects, group steps) stands or falls together; the
// core hooks (cut, damage, build permission, AI node slots) keep working. Other combat code only runs behind guarded slots.
static void combat_address_gates(ModFeatures &f) {
    if(!ga::state().unresolved)return;
    bool ok=ga::files_ok({GF_native_sortie_inl,GF_native_sortie_links_inl,GF_native_campaign_departures_inl,
        GF_native_campaign_reinforcements_inl,GF_native_campaign_routes_inl,GF_native_target_retreat_inl,GF_native_target_retreat_profile_h});
    sortie_addr_ok=ok;
    ga_gate(f.sortie_formation,80,ok);ga_gate(f.departures,81,ok);ga_gate(f.group_advance,82,ok);
    ga_gate(f.stationary_retreat,83,ok);ga_gate(f.morale,84,ok);ga_gate(f.support,85,ok);ga_gate(f.mission_v2,86,ok);
    ga_gate(f.district_intercept_block,87,ga::files_ok({GF_native_district_intercept_link_inl}));
}
#endif
