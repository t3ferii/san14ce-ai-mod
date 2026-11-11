// Feature gates over the address table (user 2026-11-05 patch resilience, docs/PATCH_RESILIENCE.md principle 4).
// A feature whose source files use a table id that is unresolved in the running build (signature ambiguity, or a function the
// game update changed and the table therefore denies) is switched off BEFORE any of its hooks is prepared; the log gets
// addr_unresolved(feature code, unresolved ids of the build, 0). The original-byte guards still run afterwards.
// The file lists are the owners of the ids (tools/game addresses: ga_users, src file -> ids); infrastructure ids are listed
// explicitly. Codes 41.. are new (no setting): see docs/PATCH_RESILIENCE.md section "Implementation".
inline bool ga_ids_known(std::initializer_list<unsigned> ids) {
    for(unsigned id:ids)if(!ga::known(id))return false;
    return true;
}
// Like ga::files_ok, but ids in `except` are ignored: a feature that does not use them stays on while another feature of the same
// source file (which does) is switched off through its own ids (B stage 2026-11-05: modpack stat-cap patches).
inline bool ga_files_ok_except(std::initializer_list<unsigned> files,std::initializer_list<unsigned> except) {
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
    ga_gate(f.battle_rules,41,ga_ids_known({GA_16AEF4,GA_2B2A90,GA_2B2A95,GA_2B2E28}) &&
        ga::files_ok({GF_native_expert_links_inl,GF_native_expert_settings_inl,GF_native_adjacency_inl}));
    ga_gate(f.domestic,42,ga_ids_known({GA_1137FF0}) && ga::files_ok({GF_native_domestic_inl}));
    ga_gate(f.domestic_free,43,f.domestic);
    ga_gate(f.awards,44,ga_ids_known({GA_1137FF0}) && ga::files_ok({GF_native_awards_inl,GF_native_personnel_inl}));
    ga_gate(f.injury,45,ga_ids_known({GA_1137FF0}) && ga::files_ok({GF_native_injury_inl,GF_native_injury_links_inl,GF_native_personnel_inl}));
    ga_gate(f.injury_herbal,46,ga_ids_known({GA_1137FF0}) && ga::files_ok({GF_native_injury_inl,GF_native_injury_links_inl,GF_native_personnel_inl}));
    ga_gate(f.recruit,47,ga::files_ok({GF_native_recruit_ui_inl}));
    ga_gate(f.advisor_briefing,48,ga::files_ok({GF_advisor_briefing_profile_h,GF_native_advisor_briefing_inl}));
    ga_gate(f.damage_preview,49,ga::files_ok({GF_damage_preview_profile_h,GF_native_damage_preview_inl}));
    ga_gate(f.building_destruction_all,50,ga::files_ok({GF_building_destruction_profile_h,GF_native_building_destruction_inl}));
    ga_gate(f.diplomacy_overinvest,51,ga::files_ok({GF_diplomacy_intel_profile_h,GF_native_diplomacy_intel_inl}));
    ga_gate(f.diplomacy_counterturn,52,ga::files_ok({GF_diplomacy_intel_profile_h,GF_native_diplomacy_intel_inl}));
    // The stat-cap byte patches (modpack_bytes rows) belong to stats200/stats200_growth only; tooltip_collab, scenery_skip and
    // battle_deaths never touch them. 230098 (item-0x7b clamp): 1a4a447d uses the relay form (native_modpack.inl modpack_clamp_short, local10).
    // The 5 battle_deaths ids (GA_25B022..GA_297B6F) are also attributed to native_modpack.inl through the pre-rebase backup source
    // (build/rebase_logs/src_before_rebase, docs/HISTORY.md 3.4): they must not switch the stat-cap/collab/scenery features off.
    const std::initializer_list<unsigned> deaths_ids={GA_25B022,GA_27F716,GA_25B0EB,GA_27F7A9,GA_297B6F};
    const bool modpack=ga_files_ok_except({GF_modpack_tables_h,GF_native_modpack_inl},deaths_ids);
    const bool modpack_core=ga_files_ok_except({GF_modpack_tables_h,GF_native_modpack_inl},{GA_226E2A,GA_226E7F,GA_22AE26,GA_22AE99,GA_23C458,GA_23C4B6,GA_20B7C5,GA_20BFDA,GA_5DFD18,GA_142FC70,GA_5DFB83,GA_321F3E,GA_231AFB,GA_2BC14A,GA_25B022,GA_27F716,GA_25B0EB,GA_27F7A9,GA_297B6F});
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
    ga_gate(f.tactic_priority,74,ga::files_ok({GF_native_tactic_priority_inl,GF_native_tactic_priority_grid_inl}));
    // 88/89 (local56 corps_domestic): blackboard reads, the player-force test and the slots / CALL the two switches use.
    ga_gate(f.ai_search2,88,ga_ids_known({GA_1137FF0,GA_B28BB0,GA_B28D20,GA_2350F0,GA_134C580,GA_54AD0}));
    // 90 (local57 officer_rotation): the native move command and the corps-edit governor change it reuses.
    ga_gate(f.officer_rotation,90,ga_ids_known({GA_32E3B0,GA_22E3E0,GA_231770,GA_233BE0,GA_234AB0,GA_22C4B0,GA_22BAB0,GA_22E870,
        GA_2350F0,GA_13466B0,GA_1AC60,GA_18D50,GA_27D4130,GA_22EF50,GA_1F2630,GA_1BFFF0,GA_1A4B4D0,GA_2435D0,GA_284520,GA_2780F60,GA_2CC630,GA_234880,GA_232030}));
    ga_gate(f.officer_first,89,ga_ids_known({GA_1137FF0,GA_B28D20,GA_2350F0,GA_1355B18,GA_7F4F0,GA_C4383,GA_A9960,GA_2780F60}));
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
    // 76: auto-commands' 자동포상 redirect and the toggle popup read original objects on the UI thread; any unresolved id
    // switches the installer switch off so neither the hook nor the popup is attempted (PW24a; 자동탐색 = PW24b).
    ga_gate(f.auto_reward,76,ga_ids_known({GA_17B990,GA_44EDD1,GA_331780,GA_13D9430,GA_2780F60,GA_230270,GA_330B90,
        GA_22E870,GA_233BE0,GA_234AB0,GA_32E3B0,GA_253F0,GA_18C30,GA_1B5F9E0,GA_1F3C90,GA_9E00,GA_4603F0,GA_11040,GA_30F010,
        // PW36b main-screen toggles: CGameLayout setup/destroy slots and the two redirected 8685C0 call sites.
        GA_13D9268,GA_13D90E0,GA_4452B0,GA_43BE70,GA_44616D,GA_445FA6,GA_8685C0}));
    // 77: 자동탐색 (PW24b/PW36b) — 진행 콜사이트 리다이렉트와 후보 registry, 메인 화면 토글의 CGameLayout 훅.
    // 하나라도 unresolved면 설치를 시도하지 않으므로 훅도 없다.
    ga_gate(f.auto_search,77,ga_ids_known({GA_7161D0,GA_6DF1B0,GA_1F3F50,
        GA_235300,GA_6D5A60,GA_18AC0,GA_186D0,GA_18790,GA_27D4178,GA_27D4130,GA_27D4140,GA_27D4170,GA_27812C8,
        GA_13466B0,GA_44F281,GA_453310,GA_11040,GA_230270,GA_330B90,
        // PW36b main-screen toggles: CGameLayout setup/destroy slots and the two redirected 8685C0 call sites.
        GA_13D9268,GA_13D90E0,GA_4452B0,GA_43BE70,GA_44616D,GA_445FA6,GA_8685C0}));
}
#endif
