// Split owners stay independent; legacy integrated fixtures/builds remain unchanged.
// owner: 0 shared, 1 CCZ dialog, 2 auto domestic, 3 advisor HUD, 4 terrain.
#include "game_addresses.h"
#include "compat_report.h"   // user 2026-11-09: switched-off features also go to ai행동교정모드호환성.log
// Imported-code units (modpack, traits9). Production convenience builds carry them; the split fixtures do not (no imported COFF,
// no generated payload headers) except the address guard fixture, which defines SPLIT_FIXTURE_MODPACK to run their real guards
// against the unpacked EXE of each build (2026-11-05 local3, docs/HISTORY.md section 12).
#if defined(SPLIT_BASE) && (!defined(SPLIT_MODULE_FIXTURE) || defined(SPLIT_FIXTURE_MODPACK))
#define SPLIT_MODPACK 1
#endif
// SplitPointer owner enum, struct, table and owner gates are generated (development/module_registry.json "pointers" -> src/module_pointers.inl).
#include "module_pointers.inl"
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
    next_or<void(__fastcall *)(Obj)>(next_frame,GA(GA_896710))(layout);
#ifndef SPLIT_MODULE_FIXTURE
    if(!enabled || !mod_features.advisor_briefing) {unified_overlay::show(false);return;}
    Obj manager=fn<Obj(__fastcall *)()>(GA(GA_11040))();
    Obj modal=manager?fn<Obj(__fastcall *)(Obj)>(GA(GA_572480))(manager):nullptr;
    bool active=unified_overlay::eligible(world()!=nullptr,player_force()!=0,fn<unsigned(__fastcall *)(Obj)>(GA(GA_871600))(layout)!=0,modal!=nullptr);
    advisor_briefing::hud(layout,active,modal!=nullptr);
    unified_overlay::show(false);
#endif
}
static void __fastcall leave(Obj layout,Obj context) {
#ifndef SPLIT_MODULE_FIXTURE
    briefing_overlay::publish({},false);unified_overlay::show(false);unified_overlay::take_click();
#endif
    next_or<void(__fastcall *)(Obj,Obj)>(next_leave,GA(GA_87A690))(layout,context);
}
}
static_assert(terrain_render::CreateTexture2DSlot==5,"ID3D11Device::CreateTexture2D ABI");
#endif
#ifdef SPLIT_MODPACK
// Imported-code features, one unit each; this hub joins them (native_modpack.inl names none of the others).
#include "native_modpack.inl"
#include "native_battle_deaths.inl"
#include "native_trait_hover.inl"
#include "native_traits9.inl"
#endif
#if !defined(SPLIT_COMBAT) && !defined(SPLIT_MODULE_FIXTURE)
// Terrain edit, user 2026-10-29: units sank into raised cells. BC2380 builds two CPU
// height collision objects with A07B70 at BC2467/BC247C and stores them in its owner
// (+B0/+B8; no static path). Redirect only those two CALLs to record the results.
namespace terrain_height_link {
static const unsigned kSites[2]={GA(GA_BC4BD7),GA(GA_BC4BEC)};
static constexpr unsigned char kCreateEntry[]={0x4C,0x8B,0xDC,0x56,0x41,0x56,0x41,0x57,0x48,0x81,0xEC,0x10,0x01,0x00,0x00};
static CallLink links[2];
static Obj relay;
using Create=Obj(__fastcall *)(Obj,Obj);
template<unsigned I> static Obj __fastcall create(Obj source,Obj allocator) { COST_SCOPE(I==0?"terrain_height_create_0":"terrain_height_create_1");
    Obj object=terrain_profile::invoke(terrain_profile::Id::HeightNative,fn<Create>(GA(GA_A0A350)),source,allocator);
    terrain_render::heightfield_created(I,object,image+GA(GA_14D5350));
    return object;
}
static bool guards() {
    if(ga::mcmp(image,GA_A0A350,0,kCreateEntry,sizeof(kCreateEntry)))return false;
    for(unsigned site:kSites)
        if(image[site]!=0xE8 || image+site+5+*reinterpret_cast<int *>(image+site+1)!=image+GA(GA_A0A350))return false;
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
#define MODULE_POINTERS_ROWS   // the table (generated), after the namespaces its rows name
#include "module_pointers.inl"
#undef MODULE_POINTERS_ROWS
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
// 2026-11-05 local3 (user: a refused optional feature switches only itself off): feature_off(feature id, reason).
// Reasons: 1 imported code is fixed to the reference build, 2 prepare failed, 3 publish failed, 4 imported installer returned 0.
// 40 siege_guard (combat module only, 2026-11-09): set from split_prepare/split_publish.
// (feature_guard_mismatch = original bytes/slots differ, feature gates = addr_unresolved.)
// The compat log is always on (release builds too); only an actual switch-off is recorded.
static void compat_feature_off(unsigned id,unsigned reason) {
    wchar_t what[32];
    if(swprintf_s(what,32,L"기능 %u",id)>=0)compat_report::add_off(what,reason);
}
[[maybe_unused]] static void split_feature_off(bool &feature,unsigned id,unsigned reason) {
    if(feature) {feature=false;log_event("feature_off",id,reason);compat_feature_off(id,reason);}
}
#ifndef SPLIT_COMBAT
static void module_subscribe_all();   // generated (module_table_convenience.inl)
// Stable diagnostic feature IDs (no new settings): 1 battle_rules, 2 domestic,
// 3 domestic_free, 4 population, 5 reserved: unified removed (2026-11-03), 6 merit, 7 save sidecar, 8 recruit,
// 9 recruit_ai, 10 injury, 11 herbal, 12 awards, 13 loyalty, 14 stats200,
// 15 growth, 16 tooltip_collab, 17 scenery, 18 deaths, 19 traits9, 42 ai_search2, 43 officer_first (local56), 44 officer_rotation (local57), 20 (captive dialog, removed local10), 34 hover (internal flag of tooltip_collab;
// own source native_trait_hover.inl since 2026-11-06: reasons 1 guard, 5 install; owner "trait_hover" rows of the Layout registry).
static void split_disable(bool &feature,unsigned id) {
    if(feature) {feature=false;log_event("feature_guard_mismatch",id);compat_feature_off(id,6);}
}
#define MODULE_POINTERS_OWNERS   // owner gates of the SplitPointer rows (generated)
#include "module_pointers.inl"
#undef MODULE_POINTERS_OWNERS
#include "core_module.h"
#include "module_closure.inl"   // feature_off_closure: a child flag follows its parent (development/module_registry.json requires)
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
        trait_hover_yield=layout_client::yield_to_peer("trait_hover",GA(GA_809461));   // rva only labels the log line; a hover slot rva here would trip verify_modpack_static pointer-overlap scan
        modpack_yield_collab=layout_client::yield_to_peer("collab",GA(GA_13AA260));
        if(trait_hover_yield && modpack_yield_collab)f.tooltip_collab=false;
    }
    if(f.scenery_skip && layout_client::yield_to_peer("scenery",GA(GA_13D5F90)))f.scenery_skip=false;
    if(f.battle_deaths && layout_client::yield_to_peer("battle_deaths",GA(GA_25B022)))f.battle_deaths=false;
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
    if(f.battle_rules && (ga::mcmp(image,GA_16AEF4,0,kPairCallBytes,5) ||
        ga::mcmp(image,GA_2B2A90,0,kAdjacencyEntryBytes,5) || !expert_guards_match()))split_disable(f.battle_rules,1);
    if(services::domestic_tick.any() && ga::mcmp(image,GA_297070,0,kDomesticTickEntryBytes,5)) {   // domestic-tick service guard: all subscribers off
        for(unsigned i=0;i<services::domestic_tick.fail_count;++i)
            split_disable(*services::domestic_tick.fails[i].flag,services::domestic_tick.fails[i].id);
    }
    if(f.domestic_free && (ga::mcmp(image,GA_36F83,0,kDomesticBudgetBranchBytes,6) ||
        ga::mcmp(image,GA_1EE815,0,kDomesticCostLoadBytes,6)))split_disable(f.domestic_free,3);
    if(f.population && ga::mcmp(image,GA_304D70,0,population_entry,16))split_disable(f.population,4);
    for(const auto &p:split_pointers)if(split_pointer_enabled(p) &&
#ifdef LAYOUT_ROUTED
        !p.next &&   // routed slots: ownership is judged by the Layout hook manager at publish
#endif
        pointer(image,p.slot)!=image+p.original) {
        split_pointer_owner_fail(p.owner,f);   // the owner switches itself off (ui-dialogs: every user of the service)
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
    feature_off_closure(f);   // domestic_free/direct follow domestic, awards_ai awards, stats200_growth stats200
}
#endif
// local17: a combat pointer row that its keys leave unpublished (N2076, attack_eval70/retreat_eval60 off) is not guarded either,
// so an off key never blocks the module on another mod's or a test image's slot value.
[[maybe_unused]] static bool split_pointer_guarded(const SplitPointer &p) {
#ifdef SPLIT_COMBAT
    if(p.original==GA(GA_922F0))return mod_features.attack_eval70 || mod_features.retreat_eval60;
#else
    (void)p;
#endif
    return true;
}
static bool split_guards_match() {
#ifdef SPLIT_COMBAT
    combat_address_gates(mod_features);
    for(const auto &p:split_pointers)if(p.slot && p.original && split_pointer_guarded(p) && pointer(image,p.slot)!=image+p.original)return false;   // unresolved id: that slot is skipped
    return !ga::mcmp(image,GA_D3516,0,kCutCallBytes,5) && !ga::mcmp(image,GA_175950,0,kDamageEntryBytes,5) &&
        !ga::mcmp(image,GA_27AAC0,0,kBuildPermissionEntryBytes,5) && sortie_guards_match();
#else
    split_feature_guards();return true; // an empty feature set is a successful no-op
#endif
}
static void split_services_reset() {
#ifndef SPLIT_COMBAT
    services::reset_all();
    merit_sidecars.clear();
#endif
}
static void split_rollback() { COST_SCOPE("split_rollback");
    InterlockedExchange(&enabled,0);
    split_services_reset();
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
    route_hook::restore();
    siege_guard_hook::restore();
#else
    terrain_edit::reset_ui_context();
    restore_link(split_install.safety);restore_link(split_install.intercept);
    restore_expert(split_install.expert);
    restore_awards(split_install.awards);restore_injury(split_install.injury);
#ifdef CORPS_DOMESTIC_HOOKS
    corps_domestic::restore();
#endif
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
    compat_report::session();   // "=== 게임 실행 ===" once per process so the installer can tell "no refusals" from "never ran"
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
    module_subscribe_all();   // services see the final flag set through live predicates; subscribed before any guard or hook
#endif
    if(!split_guards_match()) {split_services_reset();return false;}
    auto &s=split_install;
    s.relay=allocate_near(image+GA(GA_16AEF4),0x2000);if(!s.relay) {split_services_reset();return false;}
    Obj r=s.relay;bool ok;
#ifdef SPLIT_COMBAT
    absolute_jump(r,reinterpret_cast<void *>(&cut_player_predicate));
    absolute_jump(r+96,reinterpret_cast<void *>(&build_permission));
    memcpy(r+128,kBuildPermissionEntryBytes,5);absolute_jump(r+133,image+GA(GA_27AAC5));
    original_build_permission=reinterpret_cast<BuildPermission>(r+128);
    absolute_jump(r+1536,reinterpret_cast<void *>(&combat_apply_damage));
    memcpy(r+1568,kDamageEntryBytes,5);absolute_jump(r+1573,image+GA(GA_175955));
    original_damage=reinterpret_cast<ApplyDamage>(r+1568);
    s.count=3;
    ok=prepare_link(s.core[0],GA(GA_D3516),0xE8,r) &&
       prepare_link(s.core[1],GA(GA_175950),0xE9,r+1536) &&
       prepare_link(s.core[2],GA(GA_27AAC0),0xE9,r+96) && prepare_sortie(s.sortie,r);
    // local17 siege_guard: optional, its own relay page; a refused CALL switches only this feature off (id 40).
    if(ok) {unsigned why=siege_guard_hook::prepare();if(why) {mod_features.siege_guard=false;log_event("feature_off",40,why);}}
    // U39 route_hook: morale hold / neutral wall breach after the original route writes (id 41).
    if(ok) {unsigned why=route_hook::prepare();if(why) {mod_features.morale=false;mod_features.path_wall=false;mod_features.supply_relay=false;log_event("feature_off",41,why);}}
#else
    // [groups] convenience=0: coordinate the combat module only, publish no own hook.
    const bool trace_only=mod_features.trace_original; // [diagnostics] ai_trace_original: no feature hook
    if(trace_only || !mod_features.convenience) {s.count=0;ok=true;} else {
    absolute_jump(r,reinterpret_cast<void *>(&combat_pair_scores));
    absolute_jump(r+32,reinterpret_cast<void *>(&contextual_adjacency));
    memcpy(r+64,image+GA(GA_2B2A90),5);absolute_jump(r+69,image+GA(GA_2B2A95)); // the 5 displaced PUSHes of the running build (push r14); the guard above already matched them
    original_adjacency=reinterpret_cast<Adjacency>(r+64);
    absolute_jump(r+160,reinterpret_cast<void *>(&domestic_tick));
    memcpy(r+192,kDomesticTickEntryBytes,5);absolute_jump(r+197,image+GA(GA_297075));
    original_domestic_tick=reinterpret_cast<DomesticTick>(r+192);
    make_domestic_cost_bridge(r+224);
    absolute_jump(r+3840,reinterpret_cast<void *>(&population_growth));
    memcpy(r+3872,population_entry,5);absolute_jump(r+3877,image+GA(GA_304D75));
    original_population_growth=reinterpret_cast<PopulationGrowth>(r+3872);
    const unsigned char no_budget[]={0x90,0x90,0x90,0x90,0x90,0x90};
    const unsigned char numeric[]={0x48,0x89,0xD0,0x90},text[]={0x90,0x90,0x90};
    s.count=6;
    // Core links (shared by several features): a failed preparation fails the whole module.
    ok=(!split_core_enabled(0) || prepare_link(s.core[0],GA(GA_16AEF4),0xE8,r)) &&
       (!split_core_enabled(1) || prepare_link(s.core[1],GA(GA_2B2A90),0xE9,r+32)) &&
       (!split_core_enabled(2) || prepare_link(s.core[2],GA(GA_297070),0xE9,r+160)) &&
       (!split_core_enabled(3) || prepare_link(s.core[3],GA(GA_1EE815),0xE8,r+224)) &&
       (!split_core_enabled(4) || prepare_bytes(s.core[4],GA(GA_36F83),no_budget,6)) &&
       (!split_core_enabled(5) || prepare_link(s.core[5],GA(GA_304D70),0xE9,r+3840));
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
        if(m.awards && !prepare_awards(s.awards,r)) {split_feature_off(m.awards,12,2);feature_off_closure(m);}
#ifdef CORPS_DOMESTIC_HOOKS
        if(corps_domestic::prepare())split_feature_off(m.officer_first,43,2);   // local56: the condition-116 CALL moved or no relay
#endif
        if(m.loyalty && !(prepare_bytes(s.loyalty[0],GA(GA_34CB96),numeric,4) && prepare_bytes(s.loyalty[1],GA(GA_35EF25),text,3)))
            split_feature_off(m.loyalty,13,2);
        if(m.battle_rules && !prepare_expert(s.expert))split_feature_off(m.battle_rules,1,2);   // core 0/1 are then skipped at publish
#ifdef SPLIT_MODPACK
        unsigned bad=prepare_modpack(m.stats200,m.stats200_growth,m.tooltip_collab,m.scenery_skip);
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
    if(ok && split_core_enabled(3))s.core[3].modified[GA(GA_1EE815)-s.core[3].block+5]=0x90;
    }
#endif
    DWORD old=0;
    if(!ok || !VirtualProtect(r,8192,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),r,8192)) {
#ifdef SPLIT_BASE
        if(s.expert.relay)VirtualFree(s.expert.relay,0,MEM_RELEASE);
#endif
        VirtualFree(r,0,MEM_RELEASE);s={};split_services_reset();return false;
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
    case 2:return services::domestic_tick.any();            // the domestic tick carries all of its subscribers (awards, injury, domestic)
    case 3:case 4:return f.domestic_free;                   // auto-domestic free cost bridge, budget branch
    default:return f.population;
    }
#endif
}
static bool split_pointer_enabled(const SplitPointer &p) {
    if(!p.slot || !p.original)return false;   // unresolved address table id (docs/PATCH_RESILIENCE.md): never patch image+0
#ifdef SPLIT_COMBAT
    // local17: N2076 (922F0) is wrapped only while attack_eval70 or retreat_eval60 is on; off = original slot.
    if(p.original==GA(GA_922F0))return mod_features.attack_eval70 || mod_features.retreat_eval60;
    return true;
#else
    if(!mod_features.convenience || mod_features.trace_original)return false;
    if(p.slot==terrain_trees::Slot)return mod_features.terrain_edit && terrain_trees::permitted;
    return split_pointer_owner_enabled(p.owner);
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
    if(ok && !siege_guard_hook::publish()) {mod_features.siege_guard=false;log_event("feature_off",40,3);}
    if(ok && !route_hook::publish()) {mod_features.morale=false;mod_features.path_wall=false;mod_features.supply_relay=false;log_event("feature_off",41,3);}
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
                split_feature_off(f.merit,6,3);split_feature_off(f.awards,12,3);feature_off_closure(f);
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
#ifdef CORPS_DOMESTIC_HOOKS
        // local56: after injury (its links already wrap both nodes when it is on); 42 ai_search2, 43 officer_first.
        if(!split_publish_step(16,corps_domestic::publish(),0)) {split_feature_off(f.ai_search2,42,3);split_feature_off(f.officer_first,43,3);}
#endif
        if(f.awards && !split_publish_step(8,publish_awards(s.awards),0)) {
            restore_awards(s.awards);split_feature_off(f.awards,12,3);feature_off_closure(f);
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
        unsigned bad=publish_modpack(f.stats200,f.stats200_growth,f.tooltip_collab && !modpack_yield_collab,f.scenery_skip);
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
#ifndef SPLIT_COMBAT
// Activation registry (docs/MODULARIZATION_PLAN.md W4-0): the former split_activate statements, one row each, generated from
// development/module_registry.json. The helpers below are the rows that were more than one call.
#if !defined(NO_RUNTIME_LOG) && !defined(SPLIT_MODULE_FIXTURE)
static bool split_terrain_profile_init() {
    // Focused terrain CPU candidate: use the existing log switch, not broad AI/CPU profiling.
    terrain_profile::initialize(mod_features.terrain_edit && diagnostic_logging && dev_group(),
        +[](const char *kind,unsigned a,unsigned b,unsigned c){log_event(kind,a,b,c);});
    return true;
}
#endif
#ifndef NO_RUNTIME_LOG
static bool split_terrain_diag_log() {
    // Report the hooks actually published at activation, not just the requested A/B/C bits.
    unsigned terrain_foam_active=0;
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
    terrain_foam_active=terrain_foam::active()?1u:0u;
#endif
    log_event("terrain_diag_mode",mod_features.terrain_diag_mode,terrain_foam_active,
        terrain_render::hooks_active()?1u:0u);
    return true;
}
#endif
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
#ifndef NO_RUNTIME_LOG
static void split_path_count_install() {
    if(path_fast::active)log_event("path_count_skipped_path_fast");
    else path_count::install(progress_native_active && progress_native_profile,[]() {return progress_sample!=nullptr;});
}
#endif
// The game never raises the timer resolution (no winmm import), so its Sleep(n)
// and timed waits run at the default ~15.6 ms tick. Synchronous layout loads
// (86C570) wait on such stages: ~25-30 ms each. Process-wide 1 ms request.
static void split_timer_1ms() {
    if(HMODULE winmm=LoadLibraryW(L"winmm.dll"))
        if(auto begin=reinterpret_cast<unsigned(__stdcall *)(unsigned)>(GetProcAddress(winmm,"timeBeginPeriod")))
            log_event("timer_1ms",begin(1)==0?1:0);
}
// Last, so the trace chains over every published mod hook of both modules.
static bool split_ai_trace_wanted() {
#ifdef RUNTIME_COST_PROFILE
    return costprofile::on || mod_features.ai_trace;
#else
    return mod_features.ai_trace;
#endif
}
static void split_ai_trace_install() {
    // 2026-11-10: Layout registrations are deferred until the loader's final publish.
    // Flush them before diagnostic wrappers snapshot the slots; otherwise cost_profile
    // takes the domestic slots first and Layout refuses our own wrapper as OtherOwn.
    // This is the last module row, so all feature registrations are complete. The
    // loader's later publish is idempotent. Keep the duplicate-owner guard intact.
    if(HMODULE layout=GetModuleHandleW(L"SAN14_Layout.dll")) {
        auto publish=reinterpret_cast<PublishLayout>(GetProcAddress(layout,"SAN14LayoutPublish"));
        if(!publish)return;   // do not trace over an unpublished manager
        publish();
    }
#ifdef RUNTIME_COST_PROFILE
    if(costprofile::on)ai_trace::install(false,true); // [diagnostics] cost_profile: per-node cost of the original tree
    else ai_trace::install(false);
#else
    ai_trace::install(false);
#endif
}
#endif
#include "module_table_convenience.inl"
// Runs every wanted row in table order; a row whose install returns false takes its own refusal path (feature_off / split_disable).
static void split_run_modules() {
#ifndef NO_RUNTIME_LOG
    unsigned table_hash=kModuleHashSeed,run_hash=table_hash,ran=0;
    for(const auto &m:kModules)table_hash=module_name_hash(m.name,table_hash);
#else
    const unsigned table_hash=0,run_hash=0,ran=0;   // the log call below is discarded in release builds
#endif
    for(const auto &m:kModules) {
        if(!m.wanted())continue;
#ifndef NO_RUNTIME_LOG
        ++ran;run_hash=module_name_hash(m.name,run_hash);
#endif
        if(!m.install())m.refused();
    }
    // feature_order: hash of the compiled table, rows | ran<<16, hash of the rows that ran (compare with tests/module_registry_fixture.cpp).
    log_event("feature_order",table_hash,static_cast<unsigned>(sizeof(kModules)/sizeof(kModules[0]))|(ran<<16),run_hash);
}
#endif
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
            c.obstacle,c.recall_veto,c.delegated_exclude};
        unsigned mask=0;for(unsigned i=0;i<sizeof(bits)/sizeof(bits[0]);++i)if(bits[i])mask|=1u<<i;
        log_event("combat_features_enabled",mask,static_cast<unsigned>(sizeof(bits)/sizeof(bits[0])),0);
    }
#else
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
    if(mod_features.trace_original) {ai_trace::install(true); return;} // original AI, observed only
#endif
    split_run_modules();
    log_event("other_module_hooks_installed");
#endif
}
