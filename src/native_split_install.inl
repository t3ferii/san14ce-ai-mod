// Two disjoint owners. Keep legacy integrated fixtures/builds unchanged.
// owner: 0 shared, 1 unified personnel UI, 2 auto domestic ([features] keys).
enum : unsigned char {kPointerShared,kPointerUnified,kPointerDomestic,kPointerHud,kPointerTerrain};
struct SplitPointer {unsigned slot,original;void *replacement;unsigned char owner;};
#if !defined(SPLIT_COMBAT)
// Runtime COM slot, not an EXE RVA. Sole owner: SAN14_Convenience.
// Its original D3D11 implementation/bytes are validated when a device arrives.
static const struct SplitDeviceSlot {
    unsigned slot;
    bool (*guards)();
    void (*restore)();
} split_terrain_render_slot={terrain_render::CreateTexture2DSlot,&terrain_render_guards_match,&terrain_render::restore};
static_assert(terrain_render::CreateTexture2DSlot==5,"ID3D11Device::CreateTexture2D ABI");
// Officer roster: keep 2C0820 intact; redirect only its 5CC120 caller.
static const unsigned char info_setup_call[]={0xE8,0x1B,0xC8,0xCE,0xFF};
static Obj info_list_target() {
    if(image[0x5CC37E]!=0xE8)return nullptr;
    int delta;memcpy(&delta,image+0x5CC37F,4);
    uintptr_t target=reinterpret_cast<uintptr_t>(image+0x5CC383)+delta;
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+reinterpret_cast<IMAGE_DOS_HEADER *>(image)->e_lfanew);
    uintptr_t begin=reinterpret_cast<uintptr_t>(image),end=begin+pe->OptionalHeader.SizeOfImage;
    return target==begin+0x2C0820 || target<begin || target>=end?reinterpret_cast<Obj>(target):nullptr;
}
static bool info_setup_guards_match() {
    return !memcmp(image+0x8DF900,info_setup_call,5) &&
        pointer(image,0x14AB2C0+0x288)==image+0x8DF7E0 && pointer(image,0x14AB2C0+0x250)==image+0x5CC120;
}
static bool info_guards_match() {return info_setup_guards_match() && info_list_target();}
static bool prepare_info(CallLink *links,Obj relay) {
    absolute_jump(relay+3200,reinterpret_cast<void *>(&unified_native::info_setup));
    absolute_jump(relay+3232,reinterpret_cast<void *>(&unified_native::info_list_create));
    links[1]={};Obj next=info_list_target();
    if(next && call_prepare(links[1],0x5CC37E,relay+3232))
        unified_native::info_list_original=reinterpret_cast<unified_native::InfoListCreate>(next);
    else log_event("info_call_skip",0x5CC37E,0x2C0820,0);
    return call_prepare(links[0],0x8DF900,relay+3200);
}
static void restore_info(CallLink *links) {
    for(unsigned i=2;i>0;--i) {
        auto &c=links[i-1];if(!c.published || image[c.site]!=0xE8)continue;
        int delta;memcpy(&delta,image+c.site+1,4);if(delta!=c.redirected)continue;
        if(c.raw)call_write(c.site,c.redirected,c.original);
        else {
            CodeLink undo{};undo.block=c.site&~15u;
            memcpy(undo.original,image+undo.block,16);memcpy(undo.modified,undo.original,16);
            memcpy(undo.modified+(c.site-undo.block)+1,&c.original,4);
            replace_code16(undo.block,undo.original,undo.modified);
        }
        c.published=false;
    }
}
static bool publish_info(CallLink *links) {
    bool setup=false;
    for(unsigned i=0;i<2;++i) {
        auto &c=links[i];if(!c.site)continue;
        int delta;memcpy(&delta,image+c.site+1,4);
        bool ok=image[c.site]==0xE8 && delta==c.original && call_publish(c);
        if(!ok)log_event("info_call_skip",c.site,0,1);
        if(i==0)setup=ok;
    }
    return setup;
}
#endif
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
#include "native_modpack.inl"
#endif
#if !defined(SPLIT_COMBAT) && !defined(SPLIT_MODULE_FIXTURE)
// Terrain edit, user 2026-10-29: units sank into raised cells. BC2380 builds two CPU
// height collision objects with A07B70 at BC2467/BC247C and stores them in its owner
// (+B0/+B8; no static path). Redirect only those two CALLs to record the results.
namespace terrain_height_link {
static constexpr unsigned kSites[2]={0xBC2467,0xBC247C};
static constexpr unsigned char kCreateEntry[]={0x4C,0x8B,0xDC,0x56,0x41,0x56,0x41,0x57,0x48,0x81,0xEC,0x10,0x01,0x00,0x00};
static CallLink links[2];
static Obj relay;
using Create=Obj(__fastcall *)(Obj,Obj);
template<unsigned I> static Obj __fastcall create(Obj source,Obj allocator) { COST_SCOPE(I==0?"terrain_height_create_0":"terrain_height_create_1");
    Obj object=fn<Create>(0xA07B70)(source,allocator);
    terrain_render::heightfield_created(I,object,image+0x14D22F0);
    return object;
}
static bool guards() {
    if(memcmp(image+0xA07B70,kCreateEntry,sizeof(kCreateEntry)))return false;
    for(unsigned site:kSites)
        if(image[site]!=0xE8 || image+site+5+*reinterpret_cast<int *>(image+site+1)!=image+0xA07B70)return false;
    return true;
}
// split_activate, before any map load reaches BC2380.
static void install() {
    if(relay || !mod_features.terrain_edit || !guards())return;
    relay=allocate_near(image+kSites[0]);if(!relay)return;
    absolute_jump(relay,reinterpret_cast<void *>(&create<0>));absolute_jump(relay+32,reinterpret_cast<void *>(&create<1>));
    DWORD old=0;
    bool ok=VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) && FlushInstructionCache(GetCurrentProcess(),relay,4096) &&
        call_prepare(links[0],kSites[0],relay) && call_prepare(links[1],kSites[1],relay+32) &&
        call_publish(links[0]) && call_publish(links[1]);
    if(!ok){call_restore(links[1]);call_restore(links[0]);}
#ifndef NO_RUNTIME_LOG
    log_event("terrain_height_hook",ok?1:0,0,0);
#endif
}
static void restore() {call_restore(links[1]);call_restore(links[0]);}
}
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
    {terrain_trees::Slot,terrain_trees::UpdateRva,reinterpret_cast<void *>(&terrain_trees::update),kPointerTerrain},
    {0x144D350,0x7A7DB0,reinterpret_cast<void *>(&terrain_edit::domestic_click),kPointerTerrain},
    {0x14410B8,0x724660,reinterpret_cast<void *>(&terrain_edit::hex_menu_state),kPointerTerrain},
    {0x14410C0,0x71E180,reinterpret_cast<void *>(&terrain_edit::hex_menu_exit),kPointerTerrain},
    {0x144DD28,0x7A8060,reinterpret_cast<void *>(&terrain_edit::hex_menu_click),kPointerTerrain},
    {0x144DD60,0x7A92C0,reinterpret_cast<void *>(&terrain_edit::hex_menu_hint),kPointerTerrain},
    {0x143C8C8,0x6EB5C0,reinterpret_cast<void *>(&terrain_edit::menu_build),kPointerTerrain},
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
    CallLink info[2]; // officer roster: 8DF900 -> 5CC120, 5CC37E -> 2C0820
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
static void split_terrain_render_guard() {
    if(mod_features.terrain_edit && !split_terrain_render_slot.guards())split_disable(terrain_render::permitted,28);
#ifdef SAN14_WATER_DIAG_INCLUDED
    if(mod_features.terrain_edit && water_diag::permitted && !water_diag_guards_match()) {
        split_disable(water_diag::permitted,30);water_diag::restore();
    }
#endif
}
// AI-CONTRACT [compatibility]: New hook sites need feature-specific original-byte guards and a unique split_disable diagnostic ID. A mismatch disables that feature; do not replace these checks with a whole-image CRC requirement.
static void split_feature_guards() {
    auto &f=mod_features;auto &s=split_install;
    if(!f.convenience || f.trace_original)return;
    // ResourceMap compatibility is independent of the terrain UI/height hooks.
    if(f.terrain_edit && terrain_trees::permitted && !terrain_trees_guards_match())split_disable(terrain_trees::permitted,29);
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
    if(f.terrain_edit && terrain_foam::permitted && !terrain_foam_guards_match())split_disable(terrain_foam::permitted,31);
#endif
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
        else if(p.owner==kPointerTerrain)split_disable(f.terrain_edit,27);
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
    if(f.terrain_edit && !terrain_edit_guards_match())split_disable(f.terrain_edit,27);
    if(f.ccz_sprites && !ccz_sprites_guards_match())split_disable(f.ccz_sprites,32); // army display submit 1C6EC0
    split_terrain_render_guard();
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
static void split_rollback() { COST_SCOPE("split_rollback");
    InterlockedExchange(&enabled,0);
#ifndef SPLIT_COMBAT
    split_terrain_render_slot.restore();
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
    terrain_foam::restore();
#endif
#ifndef SPLIT_MODULE_FIXTURE
    terrain_height_link::restore();
#endif
#endif
    if(!split_install.attempted)return;
#ifdef SPLIT_COMBAT
    restore_sortie(split_install.sortie);
#else
    terrain_edit::reset_ui_context();
    restore_link(split_install.safety);restore_link(split_install.intercept);
    restore_info(split_install.info);
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
#if defined(SPLIT_MODULE_FIXTURE) && !defined(SPLIT_COMBAT)
// Fixture-only access to the registered runtime slot; absent from all releases.
extern "C" __declspec(dllexport) unsigned __cdecl SAN14TerrainRenderFixture(unsigned action,ID3D11Device *device) {
    if(action==0)return split_terrain_render_slot.slot;
    LONG saved_enabled=enabled;auto saved_features=mod_features;
    enabled=1;mod_features.convenience=mod_features.terrain_edit=true;
    unsigned result=0;
    if(action==1) {terrain_render::on_device(device);result=split_terrain_render_slot.guards() && terrain_render::permitted;}
    if(action==2) {
        split_terrain_render_guard();
        result=(!terrain_render::permitted?1u:0u)|(mod_features.terrain_edit?2u:0u)|(mod_features.convenience?4u:0u);
    }
    if(action==3) {split_rollback();result=1;}
    if(action==4 && device) {
        auto table=*reinterpret_cast<void ***>(device);
        for(auto &s:terrain_render::slots)if(s.table==table && table[5]==reinterpret_cast<void *>(&terrain_render::create_texture_2d))s.published=true;
        split_rollback();result=1;
    }
    enabled=saved_enabled;mod_features=saved_features;return result;
}
#endif
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
    s.relay=allocate_near(image+0x169F44,0x2000);if(!s.relay)return false;
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
    s.info_ready=ok && mod_features.unified && info_setup_guards_match() && prepare_info(s.info,r);
    // Optional like the roster. Skipped when the combat module owns 1EC890 (it applies the policy).
    s.intercept_ready=ok && mod_features.district_intercept_block && district_intercept::prepare(s.intercept,r);
    s.safety_ready=ok && mod_features.district_intercept_block && district_intercept::prepare_safety(s.safety,r);
    if(ok && split_core_enabled(3))s.core[3].modified[0x1ED475-s.core[3].block+5]=0x90;
    }
#endif
    DWORD old=0;
    if(!ok || !VirtualProtect(r,8192,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),r,8192)) {
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
    if(p.slot==terrain_trees::Slot)return mod_features.terrain_edit && terrain_trees::permitted;
    return p.owner==kPointerUnified?mod_features.unified:p.owner==kPointerDomestic?mod_features.domestic:
        p.owner==kPointerHud?(mod_features.unified || mod_features.advisor_briefing):
        p.owner==kPointerTerrain?mod_features.terrain_edit:true;
#endif
}
static bool split_publish() { COST_SCOPE("split_publish");
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
#ifndef SPLIT_COMBAT
#ifndef NO_RUNTIME_LOG
    log_event("terrain_edit_installed",[]() {unsigned count=0;for(const auto &p:split_pointers)if(p.owner==kPointerTerrain && pointer(image,p.slot)==reinterpret_cast<Obj>(p.replacement))++count;return count;}(),0,mod_features.terrain_edit);
#endif
#endif
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
    if(!f.unified || (s.info_ready && !info_setup_guards_match()))s.info_ready=false;
    if(ok && s.info_ready && !publish_info(s.info)) {
        restore_info(s.info);s.info[0].published=s.info[1].published=false;s.info_ready=false;
    }
    if(ok && s.intercept_ready && !publish_link(s.intercept))s.intercept_ready=false;
    if(ok && s.safety_ready && !publish_link(s.safety))s.safety_ready=false;
    district_intercept::diag_line(s.intercept_ready?"0,0,entry_hook_published\r\n":"0,0,entry_hook_not_published\r\n");
#endif
    if(!ok) {split_rollback();return false;}
    s.published=true;return true;
}
static void split_activate() { COST_SCOPE("split_activate");
    if(!split_install.published)return;
    InterlockedExchange(&enabled,1);
#ifdef SPLIT_COMBAT
    // Native interception/cancellation runs unchanged; no target-retreat patch.
    log_event("combat_module_hooks_installed");
#else
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
    if(mod_features.trace_original) {ai_trace::install(true); return;} // original AI, observed only
#endif
    terrain_render::install_early();
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
    terrain_foam::install();
#endif
#ifndef SPLIT_MODULE_FIXTURE
    terrain_height_link::install();
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
    if(mod_features.ccz_sprites && !ccz_sprites::install(mod_features.ccz_hide_model,mod_features.ccz_depth))split_disable(mod_features.ccz_sprites,32); // 조조전 병종 그림
    if((mod_features.ccz_block || mod_features.ccz_critical) && !ccz_combat::install(mod_features.ccz_block,mod_features.ccz_critical)) {
        split_disable(mod_features.ccz_block,33);split_disable(mod_features.ccz_critical,33);   // 16FDD0 call at 169F8B
    }
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
