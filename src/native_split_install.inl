// Split owners stay independent; legacy integrated fixtures/builds remain unchanged.
// owner: 0 shared, 1 CCZ dialog, 2 auto domestic, 3 advisor HUD, 4 terrain.
#include "game_addresses.h"
// Imported-code units (modpack, traits9). Production convenience builds carry them; the split fixtures do not (no imported COFF,
// no generated payload headers) except the address guard fixture, which defines SPLIT_FIXTURE_MODPACK to run their real guards
// against the unpacked EXE of each build (2026-11-05 local3, docs/INSTALL_LOCAL2_TRIAGE.md section 12).
#if defined(SPLIT_BASE) && (!defined(SPLIT_MODULE_FIXTURE) || defined(SPLIT_FIXTURE_MODPACK))
#define SPLIT_MODPACK 1
#endif
enum : unsigned char {kPointerShared,kPointerCczDialog,kPointerDomestic,kPointerHud,kPointerTerrain};
// next != null: a UI/AI slot that the Layout hook manager owns in production builds (user exception 2026-11-05, design 8.1);
// split fixtures keep the direct patch against their synthetic image.
struct SplitPointer {unsigned slot,original;void *replacement;unsigned char owner;void **next;};
#if !defined(SPLIT_COMBAT)
// Runtime COM slot, not an EXE RVA. Sole owner: SAN14_Convenience.
// Its original D3D11 implementation/bytes are validated when a device arrives.
static const struct SplitDeviceSlot {
    unsigned slot;
    bool (*guards)();
    void (*restore)();
} split_terrain_render_slot={terrain_render::CreateTexture2DSlot,&terrain_render_guards_match,&terrain_render::restore};
namespace advisor_hud {
static void *next_frame,*next_leave;   // Layout manager chain (null: original)
static void __fastcall frame(Obj layout) {
    next_or<void(__fastcall *)(Obj)>(next_frame,GA(GA_894420))(layout);
#ifndef SPLIT_MODULE_FIXTURE
    if(!enabled || !mod_features.advisor_briefing) {unified_overlay::show(false);return;}
    Obj manager=fn<Obj(__fastcall *)()>(GA(GA_110E0))();
    Obj modal=manager?fn<Obj(__fastcall *)(Obj)>(GA(GA_56FC20))(manager):nullptr;
    bool active=unified_overlay::eligible(world()!=nullptr,player_force()!=0,fn<unsigned(__fastcall *)(Obj)>(GA(GA_86F250))(layout)!=0,modal!=nullptr);
    advisor_briefing::hud(layout,active,modal!=nullptr);
    unified_overlay::show(false);
#endif
}
static void __fastcall leave(Obj layout,Obj context) {
#ifndef SPLIT_MODULE_FIXTURE
    briefing_overlay::publish({},false);unified_overlay::show(false);unified_overlay::take_click();
#endif
    next_or<void(__fastcall *)(Obj,Obj)>(next_leave,GA(GA_8782E0))(layout,context);
}
}
static_assert(terrain_render::CreateTexture2DSlot==5,"ID3D11Device::CreateTexture2D ABI");
#endif
#ifdef SPLIT_MODPACK
#include "native_modpack.inl"
#endif
#if !defined(SPLIT_COMBAT) && !defined(SPLIT_MODULE_FIXTURE)
// Terrain edit, user 2026-10-29: units sank into raised cells. BC2380 builds two CPU
// height collision objects with A07B70 at BC2467/BC247C and stores them in its owner
// (+B0/+B8; no static path). Redirect only those two CALLs to record the results.
namespace terrain_height_link {
static const unsigned kSites[2]={GA(GA_BC2467),GA(GA_BC247C)};
static constexpr unsigned char kCreateEntry[]={0x4C,0x8B,0xDC,0x56,0x41,0x56,0x41,0x57,0x48,0x81,0xEC,0x10,0x01,0x00,0x00};
static CallLink links[2];
static Obj relay;
using Create=Obj(__fastcall *)(Obj,Obj);
template<unsigned I> static Obj __fastcall create(Obj source,Obj allocator) { COST_SCOPE(I==0?"terrain_height_create_0":"terrain_height_create_1");
    Obj object=terrain_profile::invoke(terrain_profile::Id::HeightNative,fn<Create>(GA(GA_A07B70)),source,allocator);
    terrain_render::heightfield_created(I,object,image+GA(GA_14D22F0));
    return object;
}
static bool guards() {
    if(ga::mcmp(image,GA_A07B70,0,kCreateEntry,sizeof(kCreateEntry)))return false;
    for(unsigned site:kSites)
        if(image[site]!=0xE8 || image+site+5+*reinterpret_cast<int *>(image+site+1)!=image+GA(GA_A07B70))return false;
    return true;
}
// split_activate, before any map load reaches BC2380.
static void install() {
    // B/C deliberately keep the edit UI and trees but do not capture collision-height objects.
    if(relay || !mod_features.terrain_edit || (mod_features.terrain_diag_mode & 2u) || !guards())return;
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
// Owner record (user exception 2026-11-05): the shared Present hook (IDXGISwapChain vt[8], IDXGISwapChain1 vt[22])
// and the game-window subclass are owned by SAN14_Layout.dll, not by a split_pointers row or by this module.
// native_unified_overlay.inl only subscribes to them through the Layout API (LAYOUT_OWNS_PRESENT builds).
#include "game_addresses_gates.inl"
// AI-CONTRACT [ownership]: Each original slot/site has one DLL owner. Check this table and conditional CodeLink paths together before adding a hook; shared helper code does not authorize a second publisher.
static const SplitPointer split_pointers[]={
#ifdef SPLIT_COMBAT
    {GA(GA_1344518),GA(GA_201E0),reinterpret_cast<void *>(&first_army_node)},
    {GA(GA_1343C98),GA(GA_1F3A0),reinterpret_cast<void *>(&cut_node)},
    {GA(GA_134B4F8),GA(GA_68CE0),reinterpret_cast<void *>(&city_node)},
    {GA(GA_134AF00),GA(GA_61120),reinterpret_cast<void *>(&retreat_node)},
    {GA(GA_13512A0),GA(GA_8E7C0),reinterpret_cast<void *>(&person_info_node)},   // ai_max_aggression trait reads
    {GA(GA_134D248),GA(GA_7DC00),reinterpret_cast<void *>(&foothold_strength_node)},   // attack_recheck_fix #10114
    {GA(GA_13517A8),GA(GA_91030),reinterpret_cast<void *>(&variable_minus_node)},      // attack_recheck_fix #10115
#else
    {terrain_trees::Slot,terrain_trees::UpdateRva,reinterpret_cast<void *>(&terrain_trees::update),kPointerTerrain},
    {GA(GA_144D350),GA(GA_7A7DB0),reinterpret_cast<void *>(&terrain_edit::domestic_click),kPointerTerrain},
    {GA(GA_14410B8),GA(GA_724660),reinterpret_cast<void *>(&terrain_edit::hex_menu_state),kPointerTerrain},
    {GA(GA_14410C0),GA(GA_71E180),reinterpret_cast<void *>(&terrain_edit::hex_menu_exit),kPointerTerrain},
    {GA(GA_144DD28),GA(GA_7A8060),reinterpret_cast<void *>(&terrain_edit::hex_menu_click),kPointerTerrain},
    {GA(GA_144DD60),GA(GA_7A92C0),reinterpret_cast<void *>(&terrain_edit::hex_menu_hint),kPointerTerrain},
    {GA(GA_143C8C8),GA(GA_6EB5C0),reinterpret_cast<void *>(&terrain_edit::menu_build),kPointerTerrain},
    {GA(GA_14751D8),GA(GA_894420),reinterpret_cast<void *>(&advisor_hud::frame),kPointerHud,&advisor_hud::next_frame},
    {GA(GA_14751F0),GA(GA_8782E0),reinterpret_cast<void *>(&advisor_hud::leave),kPointerHud,&advisor_hud::next_leave},
    {GA(GA_13AC588),GA(GA_2FB440),reinterpret_cast<void *>(&unified_dialogs::frame),kPointerCczDialog,&unified_dialogs::next_frame},
    {GA(GA_13AC5B0),GA(GA_258170),reinterpret_cast<void *>(&unified_dialogs::controls),kPointerCczDialog,&unified_dialogs::next_controls},
    {GA(GA_13AC578),GA(GA_256970),reinterpret_cast<void *>(&unified_dialogs::destroy),kPointerCczDialog,&unified_dialogs::next_destroy},
    {GA(GA_1346E70),GA(GA_36320),reinterpret_cast<void *>(&domestic_officer_node),kPointerDomestic,&next_domestic_officer},
    {GA(GA_134C8F8),GA(GA_7A110),reinterpret_cast<void *>(&domestic_selection_node),kPointerDomestic,&next_domestic_selection},
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
#ifdef SPLIT_MODULE_FIXTURE
// Fixture only: fail_after(2000+step) makes that optional publish step report failure after it really published.
static bool split_fixture_step_fails(unsigned step) {return split_fixture_fail_after==2000u+step;}
#else
static bool split_fixture_step_fails(unsigned) {return false;}
#endif
// split_publish_step(step, ok, detail): 1 core link (detail = index), 2 pointer slot (detail = slot RVA), 3 merit, 4 recruit_ui,
// 5 recruit_alert, 6 recruit_ai, 7 injury, 8 awards, 9 loyalty, 10 expert, 11 modpack (detail = failed pieces), 12 traits9 (detail =
// failed parts), 13 unused (was modpack_activate), 14 district intercept entry, 15 district safety.
static bool split_publish_step(unsigned step,bool ok,unsigned detail) {
    ok=ok && !split_fixture_step_fails(step);
    log_event("split_publish_step",step,ok?1u:0u,detail);
    return ok;
}
static bool split_pointer_enabled(const SplitPointer &p);
#ifndef SPLIT_COMBAT
// Stable diagnostic feature IDs (no new settings): 1 battle_rules, 2 domestic,
// 3 domestic_free, 4 population, 5 reserved: unified removed (2026-11-03), 6 merit, 7 save sidecar, 8 recruit,
// 9 recruit_ai, 10 injury, 11 herbal, 12 awards, 13 loyalty, 14 stats200,
// 15 growth, 16 tooltip_collab, 17 scenery, 18 deaths, 19 traits9, 20 (captive dialog, removed local10), 34 hover (internal flag of tooltip_collab;
// own source native_trait_hover.inl since 2026-11-06: reasons 1 guard, 5 install; owner "trait_hover" rows of the Layout registry).
static void split_disable(bool &feature,unsigned id) {
    if(feature) {feature=false;log_event("feature_guard_mismatch",id);}
}
// 2026-11-05 local3 (user: a refused optional feature switches only itself off): feature_off(feature id, reason).
// Reasons: 1 imported code is fixed to the reference build, 2 prepare failed, 3 publish failed, 4 imported installer returned 0.
// (feature_guard_mismatch = original bytes/slots differ, feature gates = addr_unresolved.)
static void split_feature_off(bool &feature,unsigned id,unsigned reason) {
    if(feature) {feature=false;log_event("feature_off",id,reason);}
}
static void split_terrain_render_guard() {
    if(mod_features.terrain_edit && !split_terrain_render_slot.guards())split_disable(terrain_render::permitted,28);
#ifdef SAN14_WATER_DIAG_INCLUDED
    if(mod_features.terrain_edit && water_diag::permitted && !water_diag_guards_match()) {
        split_disable(water_diag::permitted,30);water_diag::restore();
    }
#endif
}
#ifdef LAYOUT_ROUTED
// Same-function peers (design 8.2, user 2026-11-05 Layout compatibility design 8.7): decided here, before the original
// byte guards, so that a peer that already patched the same bytes shows up as "yielded" and not as a guard mismatch.
// Hooks that go through the manager carry the YIELD_IF_PEER flag themselves; this covers owners that apply their own
// patches (imported code, byte patches).
static void split_layout_yield() {
    static bool decided;   // guards run at prepare and again at publish: decide and log once
    if(decided)return;
    decided=true;
    auto &f=mod_features;
    if(f.stats200 && layout_client::yield_to_peer("stats200",modpack_bytes[0].rva)) {f.stats200=false;f.stats200_growth=false;}
    if(f.tooltip_collab) {
        trait_hover_yield=layout_client::yield_to_peer("trait_hover",GA(GA_806D51));   // rva only labels the log line; a hover slot rva here would trip verify_modpack_static pointer-overlap scan
        modpack_yield_collab=layout_client::yield_to_peer("collab",GA(GA_13A7220));
        if(trait_hover_yield && modpack_yield_collab)f.tooltip_collab=false;
    }
    if(f.scenery_skip && layout_client::yield_to_peer("scenery",GA(GA_13D2F00)))f.scenery_skip=false;
    if(f.battle_deaths && layout_client::yield_to_peer("battle_deaths",GA(GA_259682)))f.battle_deaths=false;
    if(f.traits9 && layout_client::yield_to_peer("traits9",trait9_manifests[0].sites[0].rva))f.traits9=false;
}
#endif
// AI-CONTRACT [compatibility]: New hook sites need feature-specific original-byte guards and a unique split_disable diagnostic ID. A mismatch disables that feature; do not replace these checks with a whole-image CRC requirement.
static void split_feature_guards() {
    auto &f=mod_features;auto &s=split_install;
    if(!f.convenience || f.trace_original)return;
#ifdef LAYOUT_ROUTED
    split_layout_yield();
#endif
    split_address_gates(f);   // user 2026-11-05: unresolved address table ids switch their features off first
    // ResourceMap compatibility is independent of the terrain UI/height hooks.
    if(f.terrain_edit && terrain_trees::permitted && !terrain_trees_guards_match())split_disable(terrain_trees::permitted,29);
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
    if(f.terrain_edit && terrain_foam::permitted && !terrain_foam_guards_match())split_disable(terrain_foam::permitted,31);
#endif
    if(f.battle_rules && (ga::mcmp(image,GA_169F44,0,kPairCallBytes,5) ||
        ga::mcmp(image,GA_2B07C0,0,kAdjacencyEntryBytes,5) || !expert_guards_match()))split_disable(f.battle_rules,1);
    if((f.domestic || f.awards || f.injury || f.injury_herbal) && ga::mcmp(image,GA_295160,0,kDomesticTickEntryBytes,5)) {
        split_disable(f.domestic,2);split_disable(f.awards,12);
        split_disable(f.injury,10);split_disable(f.injury_herbal,11);
    }
    if(f.domestic_free && (ga::mcmp(image,GA_36423,0,kDomesticBudgetBranchBytes,6) ||
        ga::mcmp(image,GA_1ED475,0,kDomesticCostLoadBytes,6)))split_disable(f.domestic_free,3);
    if(f.population && ga::mcmp(image,GA_302980,0,population_entry,16))split_disable(f.population,4);
    for(const auto &p:split_pointers)if(split_pointer_enabled(p) &&
#ifdef LAYOUT_ROUTED
        !p.next &&   // routed slots: ownership is judged by the Layout hook manager at publish
#endif
        pointer(image,p.slot)!=image+p.original) {
        if(p.owner==kPointerCczDialog)split_disable(f.ccz_sprites,32);
        else if(p.owner==kPointerHud)split_disable(f.advisor_briefing,26);
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
    if(f.ccz_sprites
#ifdef SPLIT_MODULE_FIXTURE
       && !(split_fixture_disabled&16)
#endif
       && !ccz_sprites_guards_match())split_disable(f.ccz_sprites,32); // army display submit 1C6EC0
    split_terrain_render_guard();
#ifdef SPLIT_MODPACK
    // battle_deaths and the hover are own source (native_battle_deaths.inl, native_trait_hover.inl): their own guards (function hashes,
    // bytes, registry rows) decide, off = original game behaviour.
    if(f.tooltip_collab && !trait_hover_off && !trait_hover_guards_ok()) {trait_hover_off=true;log_event("feature_off",34,1);}
    if(f.battle_deaths && !battle_deaths_guards_ok())split_feature_off(f.battle_deaths,18,1);
    if(f.stats200 && !modpack_guards_match(true,false,false,false))split_disable(f.stats200,14);
    if(f.stats200_growth && !modpack_guards_match(false,true,false,false))split_disable(f.stats200_growth,15);
    if(f.tooltip_collab && !modpack_guards_match(false,false,true,false))split_disable(f.tooltip_collab,16);
    if(f.scenery_skip && !modpack_guards_match(false,false,false,true))split_disable(f.scenery_skip,17);
    if(f.traits9 && !traits9_guards_match())split_disable(f.traits9,19);
#endif
    f.domestic_free=f.domestic && f.domestic_free;f.domestic_direct=f.domestic && f.domestic_direct;
    f.awards_ai=f.awards && f.awards_ai;f.stats200_growth=f.stats200 && f.stats200_growth;
}
#endif
static bool split_guards_match() {
#ifdef SPLIT_COMBAT
    combat_address_gates(mod_features);
    for(const auto &p:split_pointers)if(p.slot && p.original && pointer(image,p.slot)!=image+p.original)return false;   // unresolved id: that slot is skipped
    return !ga::mcmp(image,GA_D2486,0,kCutCallBytes,5) && !ga::mcmp(image,GA_1749A0,0,kDamageEntryBytes,5) &&
        !ga::mcmp(image,GA_279190,0,kBuildPermissionEntryBytes,5) && sortie_guards_match();
#else
    split_feature_guards();return true; // an empty feature set is a successful no-op
#endif
}
static void split_rollback() { COST_SCOPE("split_rollback");
    InterlockedExchange(&enabled,0);
#if !defined(SPLIT_COMBAT) && defined(LAYOUT_ROUTED)
    layout_client::cancel_all();   // registrations not yet published (or installed hooks: Layout unhook/bypass)
#endif
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
#ifdef SPLIT_MODPACK
    restore_traits9();restore_modpack();
#endif
    restore_expert(split_install.expert);
    restore_awards(split_install.awards);restore_injury(split_install.injury);
    restore_recruit_alert(split_install.alert);restore_recruit_ai();restore_recruit_ui();
    restore_merit(split_install.merit);
    for(auto &l:split_install.loyalty)restore_link(l);
#endif
    for(const auto &p:split_pointers) {
#ifdef LAYOUT_ROUTED
        if(p.next)continue;
#endif
        if(p.slot && p.original)restore_pointer(p.slot,p.original,p.replacement);
    }
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
#if defined(SPLIT_BASE) && defined(LAYOUT_OWNS_PRESENT) && !defined(NO_RUNTIME_LOG)
    layout_client::connect_log();   // Layout decisions (installed/chained/yielded/refused/bypassed) use the common log_event writer
#endif
#ifdef SPLIT_MODULE_FIXTURE
    if(split_fixture_fail_after==1000)return false; // allocation/preparation failure
#ifndef SPLIT_COMBAT
    if(split_fixture_disabled&1)mod_features.ccz_sprites=false;
    if(split_fixture_disabled&2)mod_features.merit=false;
    if(split_fixture_disabled&4)mod_features.convenience=false;
    if(split_fixture_disabled&8)mod_features.advisor_briefing=true; // fixture-only independent HUD owner case
    if(split_fixture_disabled&16)mod_features.ccz_sprites=true; // fixture-only shared dialog owner case
#endif
#endif
#ifndef SPLIT_COMBAT
    split_install.save_ready=mod_features.convenience && !mod_features.trace_original;
#endif
    if(!split_guards_match())return false;
    auto &s=split_install;
    s.relay=allocate_near(image+GA(GA_169F44),0x2000);if(!s.relay)return false;
    Obj r=s.relay;bool ok;
#ifdef SPLIT_COMBAT
    absolute_jump(r,reinterpret_cast<void *>(&cut_player_predicate));
    absolute_jump(r+96,reinterpret_cast<void *>(&build_permission));
    memcpy(r+128,kBuildPermissionEntryBytes,5);absolute_jump(r+133,image+GA(GA_279195));
    original_build_permission=reinterpret_cast<BuildPermission>(r+128);
    absolute_jump(r+1536,reinterpret_cast<void *>(&combat_apply_damage));
    memcpy(r+1568,kDamageEntryBytes,5);absolute_jump(r+1573,image+GA(GA_1749A5));
    original_damage=reinterpret_cast<ApplyDamage>(r+1568);
    s.count=3;
    ok=prepare_link(s.core[0],GA(GA_D2486),0xE8,r) &&
       prepare_link(s.core[1],GA(GA_1749A0),0xE9,r+1536) &&
       prepare_link(s.core[2],GA(GA_279190),0xE9,r+96) && prepare_sortie(s.sortie,r);
#else
    // [groups] convenience=0: coordinate the combat module only, publish no own hook.
    const bool trace_only=mod_features.trace_original; // [diagnostics] ai_trace_original: no feature hook
    if(trace_only || !mod_features.convenience) {s.count=0;ok=true;} else {
    absolute_jump(r,reinterpret_cast<void *>(&combat_pair_scores));
    absolute_jump(r+32,reinterpret_cast<void *>(&contextual_adjacency));
    memcpy(r+64,image+GA(GA_2B07C0),5);absolute_jump(r+69,image+GA(GA_2B07C5)); // the 5 displaced PUSHes of the running build (push r15 in 1fbe6be7, push r14 in 1a4a447d); the guard above already matched them
    original_adjacency=reinterpret_cast<Adjacency>(r+64);
    absolute_jump(r+160,reinterpret_cast<void *>(&domestic_tick));
    memcpy(r+192,kDomesticTickEntryBytes,5);absolute_jump(r+197,image+GA(GA_295165));
    original_domestic_tick=reinterpret_cast<DomesticTick>(r+192);
    make_domestic_cost_bridge(r+224);
    absolute_jump(r+3840,reinterpret_cast<void *>(&population_growth));
    memcpy(r+3872,population_entry,5);absolute_jump(r+3877,image+GA(GA_302985));
    original_population_growth=reinterpret_cast<PopulationGrowth>(r+3872);
    const unsigned char no_budget[]={0x90,0x90,0x90,0x90,0x90,0x90};
    const unsigned char numeric[]={0x48,0x89,0xD0,0x90},text[]={0x90,0x90,0x90};
    s.count=6;
    // Core links (shared by several features): a failed preparation fails the whole module.
    ok=(!split_core_enabled(0) || prepare_link(s.core[0],GA(GA_169F44),0xE8,r)) &&
       (!split_core_enabled(1) || prepare_link(s.core[1],GA(GA_2B07C0),0xE9,r+32)) &&
       (!split_core_enabled(2) || prepare_link(s.core[2],GA(GA_295160),0xE9,r+160)) &&
       (!split_core_enabled(3) || prepare_link(s.core[3],GA(GA_1ED475),0xE8,r+224)) &&
       (!split_core_enabled(4) || prepare_bytes(s.core[4],GA(GA_36423),no_budget,6)) &&
       (!split_core_enabled(5) || prepare_link(s.core[5],GA(GA_302980),0xE9,r+3840));
    // 2026-11-05 local3 (user: a refused optional feature switches only itself off): optional features prepare one by one; a
    // failed one is switched off with feature_off(id, 2) and the others continue.
    if(ok) {
        auto &m=mod_features;
        if(s.save_ready && !prepare_merit(s.merit,r)) {
            s.save_ready=false;split_feature_off(m.merit,6,2);split_feature_off(m.awards,12,2);
        }
        if(m.recruit && !prepare_recruit_alert(s.alert,r))split_feature_off(m.recruit,8,2);
        if((m.injury || m.injury_herbal) && !prepare_injury(s.injury,r)) {
            split_feature_off(m.injury,10,2);split_feature_off(m.injury_herbal,11,2);
        }
        if(m.awards && !prepare_awards(s.awards,r)) {split_feature_off(m.awards,12,2);m.awards_ai=false;}
        if(m.loyalty && !(prepare_bytes(s.loyalty[0],GA(GA_34A8D6),numeric,4) && prepare_bytes(s.loyalty[1],GA(GA_35CA35),text,3)))
            split_feature_off(m.loyalty,13,2);
        if(m.battle_rules && !prepare_expert(s.expert))split_feature_off(m.battle_rules,1,2);   // core 0/1 are then skipped at publish
#ifdef SPLIT_MODPACK
        unsigned bad=prepare_modpack();
        if(bad&1) {split_feature_off(m.stats200_growth,15,2);}
        if(bad==3 && !modpack_guards_match(m.stats200,m.stats200_growth,m.tooltip_collab,m.scenery_skip)) {   // guards changed since the guard pass
            split_feature_off(m.stats200,14,2);split_feature_off(m.tooltip_collab,16,2);split_feature_off(m.scenery_skip,17,2);
        }
        if(m.traits9 && !prepare_traits9())split_feature_off(m.traits9,19,2);
#endif
    }
    // Optional like the roster. Skipped when the combat module owns 1EC890 (it applies the policy).
    s.intercept_ready=ok && mod_features.district_intercept_block && district_intercept::prepare(s.intercept,r);
    s.safety_ready=ok && mod_features.district_intercept_block && district_intercept::prepare_safety(s.safety,r);
    if(ok && split_core_enabled(3))s.core[3].modified[GA(GA_1ED475)-s.core[3].block+5]=0x90;
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
    if(!p.slot || !p.original)return false;   // unresolved address table id (docs/PATCH_RESILIENCE.md): never patch image+0
#ifdef SPLIT_COMBAT
    return true;
#else
    if(!mod_features.convenience || mod_features.trace_original)return false;
    if(p.slot==terrain_trees::Slot)return mod_features.terrain_edit && terrain_trees::permitted;
    if(p.slot==GA(GA_13AC588) || p.slot==GA(GA_13AC5B0) || p.slot==GA(GA_13AC578))return mod_features.ccz_sprites;
    return p.owner==kPointerCczDialog?mod_features.ccz_sprites:p.owner==kPointerDomestic?mod_features.domestic:
        p.owner==kPointerHud?mod_features.advisor_briefing:
        p.owner==kPointerTerrain?mod_features.terrain_edit:true;
#endif
}
static bool split_publish() { COST_SCOPE("split_publish");
    auto &s=split_install;
    if(!s.prepared || s.attempted || !split_guards_match())return false;
    s.attempted=true;
    for(unsigned i=0;i<s.count;++i) {
        if(!split_core_enabled(i))continue;
        if(!split_publish_step(1,publish_link(s.core[i]),i)) {split_rollback();return false;}
#ifdef SPLIT_MODULE_FIXTURE
        if(split_fixture_fail_after==i+1) {split_rollback();return false;}
#endif
    }
    for(const auto &p:split_pointers) {
        if(!split_pointer_enabled(p))continue;
#ifdef LAYOUT_ROUTED
        bool ok=p.next?layout_client::vt(p.slot,p.replacement,p.next)>0:patch_pointer(p.slot,p.original,p.replacement);
#else
        bool ok=patch_pointer(p.slot,p.original,p.replacement);
        if(ok && p.next)*p.next=image+p.original;   // fixtures: direct patch; keep the next pointer on the original
#endif
#ifndef SPLIT_COMBAT
        if(!split_publish_step(2,ok,p.slot)) {split_rollback();return false;}
#else
        if(!ok) {split_rollback();return false;}
#endif
    }
#ifndef SPLIT_COMBAT
#ifndef NO_RUNTIME_LOG
    log_event("terrain_edit_installed",[]() {unsigned count=0;for(const auto &p:split_pointers)if(p.owner==kPointerTerrain && pointer(image,p.slot)==reinterpret_cast<Obj>(p.replacement))++count;return count;}(),0,mod_features.terrain_edit);
#endif
#endif
#ifdef SPLIT_COMBAT
    bool ok=publish_sortie(s.sortie);
#else
    // 2026-11-05 local3 (user: a refused optional feature switches only itself off). The shared core links and pointers above
    // are the only steps whose failure rolls the whole module back. Every optional feature below publishes on its own:
    // a failure takes that feature's written bytes and unpublished manager registrations back (restore_x +
    // layout_client::rollback_to), logs feature_off(id, 3) and the next feature continues.
    bool ok=true;
    if(mod_features.convenience && s.count!=0) {
        auto &f=mod_features;
        // Merit save links/hosts also carry the warehouse sidecar: always published (awards need the sidecar).
        if(s.save_ready) {
            unsigned mark=layout_client::current_mark();
            if(!split_publish_step(3,publish_merit(s.merit),0)) {
                restore_merit(s.merit);layout_client::rollback_to(mark);s.save_ready=false;
                split_feature_off(f.merit,6,3);split_feature_off(f.awards,12,3);f.awards_ai=false;
            }
        }
        if(f.recruit) {
            unsigned mark=layout_client::current_mark();
            if(!(split_publish_step(4,publish_recruit_ui(),0) && split_publish_step(5,publish_recruit_alert(s.alert),0))) {
                restore_recruit_alert(s.alert);restore_recruit_ui();layout_client::rollback_to(mark);split_feature_off(f.recruit,8,3);
            }
        }
        if(f.recruit_ai && !split_publish_step(6,publish_recruit_ai(),0)) {   // v12.8: AI limit shares only pure judgment helpers
            restore_recruit_ai();split_feature_off(f.recruit_ai,9,3);
        }
        if((f.injury || f.injury_herbal) && !split_publish_step(7,publish_injury(s.injury,f.injury,f.injury_herbal),0)) {
            restore_injury(s.injury);split_feature_off(f.injury,10,3);split_feature_off(f.injury_herbal,11,3);
        }
        if(f.awards && !split_publish_step(8,publish_awards(s.awards),0)) {
            restore_awards(s.awards);split_feature_off(f.awards,12,3);f.awards_ai=false;
        }
        if(f.loyalty && !split_publish_step(9,publish_link(s.loyalty[0]) && publish_link(s.loyalty[1]),0)) {
            for(auto &l:s.loyalty)restore_link(l);split_feature_off(f.loyalty,13,3);
        }
        if(f.battle_rules) {
            unsigned mark=layout_client::current_mark();
            bool published=split_publish_step(10,publish_expert(s.expert),0);
            if(published)log_event("expert_links_installed",expert_call_count,static_cast<unsigned>(sizeof(expert_pointers)/sizeof(expert_pointers[0])),layout_client::current_mark()-mark);
            else {
                // battle_rules = pair scope + adjacency + expert UI: all of it goes, the other features stay.
                restore_expert(s.expert);layout_client::rollback_to(mark);
                restore_link(s.core[1]);restore_link(s.core[0]);split_feature_off(f.battle_rules,1,3);
            }
        }
#ifdef SPLIT_MODPACK
        unsigned bad=publish_modpack();
        split_publish_step(11,bad==0,bad);
        if(bad&1) {split_feature_off(f.stats200,14,3);split_feature_off(f.stats200_growth,15,3);}
        if(bad&2)split_feature_off(f.tooltip_collab,16,3);   // collab slots are the tooltip_collab key; trait_hover registers separately at activation (bit 8 below = wanted here, trait_hover_installed = done)
        if(bad&4)split_feature_off(f.scenery_skip,17,3);
        unsigned bad9=publish_traits9(f.traits9);
        split_publish_step(12,bad9==0,bad9);
        if(bad9&1)split_feature_off(f.traits9,19,3);
        log_event("modpack_installed",(f.stats200?1u:0u)|(f.stats200_growth?2u:0u)|(f.tooltip_collab?4u:0u)|(trait_hover_wanted()?8u:0u)|
            (f.scenery_skip?16u:0u)|(f.battle_deaths?32u:0u),(f.traits9?1u:0u),0);
#endif
    }
    if(ok && s.intercept_ready && !split_publish_step(14,publish_link(s.intercept),0))s.intercept_ready=false;
    if(ok && s.safety_ready && !split_publish_step(15,publish_link(s.safety),0))s.safety_ready=false;
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
    {   // local5 audit log: which combat switches are still on after the guards and address gates (bit order below), two words
        const auto &c=mod_features;
        const bool bits[]={c.sortie_formation,c.departures,c.group_advance,c.stationary_retreat,c.morale,c.support,c.mission_v2,c.builder_ruler_fill,
            c.ai_max_aggression,c.attack_recheck_fix,c.ai_wall_block,c.ai_catapult_block,c.district_intercept_block,c.combat_cut,c.hit_response,
            c.obstacle,c.recall_veto};
        unsigned mask=0;for(unsigned i=0;i<sizeof(bits)/sizeof(bits[0]);++i)if(bits[i])mask|=1u<<i;
        log_event("combat_features_enabled",mask,static_cast<unsigned>(sizeof(bits)/sizeof(bits[0])),0);
    }
#else
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
    if(mod_features.trace_original) {ai_trace::install(true); return;} // original AI, observed only
#endif
#if !defined(NO_RUNTIME_LOG) && !defined(SPLIT_MODULE_FIXTURE)
    // Focused terrain CPU candidate: use the existing log switch, not broad AI/CSV profiling.
    terrain_profile::initialize(mod_features.terrain_edit && diagnostic_logging && dev_group(),
        +[](const char *kind,unsigned a,unsigned b,unsigned c){log_event(kind,a,b,c);});
#endif
    terrain_render::install_early();
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
    terrain_foam::install();
#endif
#ifndef SPLIT_MODULE_FIXTURE
    terrain_height_link::install();
#endif
#ifndef NO_RUNTIME_LOG
    // Report the hooks actually published at activation, not just the requested A/B/C bits.
    unsigned terrain_foam_active=0;
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
    terrain_foam_active=terrain_foam::active()?1u:0u;
#endif
    log_event("terrain_diag_mode",mod_features.terrain_diag_mode,terrain_foam_active,
        terrain_render::hooks_active()?1u:0u);
#endif
    if(mod_features.distance)install_progress_native();
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
    if(mod_features.name_lookup)install_name_lookup(); // optional; rejection keeps the original E09910
#endif
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
    // 2026-11-05 local3: each installer below owns its failure (layout_client::Transaction rolls back only its own registrations);
    // a refused one is logged as feature_off(id, 5) and switches only that feature off. Ids 35 speed_buttons, 36 lord_change,
    // 37 region_officers, 38 ccz_title (log only: the renderer stays on without its title editor).
    if(mod_features.speed_buttons && !speed_buttons::install())split_feature_off(mod_features.speed_buttons,35,5); // optional; needs the ScreenLayout patch for its buttons
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
    if(mod_features.drag_learn)drag_learn::install();
    if(mod_features.lord_change && !lord_change::install())split_feature_off(mod_features.lord_change,36,5); // new game: force select footer button 「세력내 군주교체」 (needs the ScreenLayout patch)
    if(mod_features.region_officers && !region_officers::install())split_feature_off(mod_features.region_officers,37,5); // read-only native map portraits
    if(mod_features.ccz_sprites && !ccz_sprites::install(mod_features.ccz_hide_model,mod_features.ccz_depth,mod_features.ccz_far_keep))split_disable(mod_features.ccz_sprites,32); // 조조전 병종 그림
    if(mod_features.ccz_sprites && !ccz_title::install())log_event("feature_off",38,5); // title-only appearance editor; renderer stays on if its menu guard fails
    layout_wait::install(); // after ccz_title: it patches 86E0C0, which title guards require as original
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
#ifdef SPLIT_MODPACK
    // AI death cut-in and game record (BATTLE_DEATHS_PLAN S4): own source, registered with the Layout manager (span first, then 4 CALL rows)
    if(mod_features.battle_deaths && !battle_deaths_install())split_feature_off(mod_features.battle_deaths,18,5);
    // trait/tactic/editor tooltips (TRAIT_HOVER_PLAN S4): registered last of the convenience features; Layout publishes the owner as one unit
    if(trait_hover_wanted() && !trait_hover_install()) {trait_hover_off=true;log_event("feature_off",34,5);}
#endif
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
