// Convenience engine only. Call on the native UI/save host caller thread.
// Menu binding is separate in native_terrain_edit_ui.inl. No worker is used.
#include "terrain_edit_policy.h"
#include "terrain_call_profile.h"
#if defined(SPLIT_BASE)
namespace unified_overlay {static void install();}
#endif
namespace terrain_edit {
static Changes changes;
static Obj changed_world;
// Owned only on the original UI caller thread. World boundaries discard stale identities.
static thread_local bool highlight_locked;
static thread_local Obj highlight_obj;
static thread_local unsigned highlight_hex=HexCount;
static void reset_highlight_context() {highlight_locked=false;highlight_obj=nullptr;highlight_hex=HexCount;}
// Native callbacks may arrive after the builder's TLS scope has ended.
static thread_local Obj expanded_state,expanded_tile,expanded_menu,opening_tile;
static void clear_menu_context() {expanded_state=expanded_tile=expanded_menu=nullptr;}
static void reset_ui_context() {opening_tile=nullptr;clear_menu_context();}
static bool active() {
    bool on=enabled && mod_features.convenience && mod_features.terrain_edit;
    if(!on) {reset_ui_context();}
    return on;
}
static uint8_t *tile_value(unsigned hex) {
    Obj tile=table(0xE270,hex,48399);return tile?tile+0x10:nullptr;
}
static bool occupied(unsigned hex) {
    // Same live-object predicate/table/hex as native_adapter::reserved_hex.
    for(unsigned id=1;id<=500;++id) {
        Obj unit=table(0x7E838,id,500);
        if(valid(unit) && word(unit,0x2A)==hex)return true;
    }
    // native_facilities::local_facilities and NATIVE_FUNCTIONS 22D740.
    // Include every valid building, regardless of owner, type or durability.
    for(unsigned id=1;id<=3000;++id) {
        Obj building=table(0x6DA98,id,3000);
        if(valid(building) && word(building,0x10)==hex)return true;
    }
    return false;
}
// AE5C0/AE8D0/8E5C1 cache area-pair path costs as u32 at [1B9A4A0]+C0 (500x500,
// 0 = not computed). Native C0150 zeroes it only at game start, so edited terrain
// would keep old costs all session: clear it the same way (C05A8 REP STOSB).
static constexpr unsigned char cost_cache_clear_bytes[]={
    0x48,0x8B,0x3D,0xF1,0x9E,0xAD,0x01,0x48,0x81,0xC7,0xC0,0x00,0x00,0x00,0x33,0xC0,0xB9,0x40,0x42,0x0F,0x00,0xF3,0xAA};
static void clear_cost_cache() {
#ifdef TERRAIN_EDIT_FIXTURE_COST_CACHE
    terrain_edit_fixture_cost_cache_clear();
#else
    if(memcmp(image+0xC05A8,cost_cache_clear_bytes,sizeof(cost_cache_clear_bytes)))return;
    Obj cache=pointer(image,0x1B9A4A0);
    if(cache)memset(cache+0xC0,0,500u*500u*4u);
#ifndef NO_RUNTIME_LOG
    log_event("terrain_cost_cache_clear",cache?1:0,0,0);
#endif
#endif
}
static void sync_render() {
    TERRAIN_SCOPE(TerrainSync);
#ifdef SAN14_TERRAIN_RENDER_INCLUDED
    bool install_present=false;
    bool resync=terrain_render::take_ui_work(install_present);
#ifdef SAN14_TERRAIN_TREES_INCLUDED
    resync=terrain_trees::take_ui_work(resync) || resync;
#endif
    if(resync)for(unsigned i=0;i<changes.size();++i) {
        const Change &c=changes[i];terrain_render::mark(c.hex,c.next,c.original);
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
        terrain_foam::mark(c.hex,c.next,c.original);
#endif
    }
#if defined(SPLIT_BASE)
    // UI state/load-host only, never the archive IO worker or DllMain.
    if(install_present)unified_overlay::install();
#else
    (void)install_present;
#endif
#endif
}
static void check_world() {
    TERRAIN_SCOPE(TerrainWorld);
    Obj current=world();
    if(changed_world!=current) {
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
        if(changed_world)terrain_foam::before_world_change();
#endif
        reset_highlight_context();reset_ui_context();changes.clear();changed_world=current;
#ifdef SAN14_TERRAIN_RENDER_INCLUDED
        terrain_render::before_world_change();
#ifdef SAN14_TERRAIN_TREES_INCLUDED
        terrain_trees::before_world_change();
#endif
#endif
    }
    // The world root is allocated once, so a new game keeps the same pointer.
    // Any edit whose tile no longer holds our value is stale: forget it and
    // return the render/CPU heights to the tile's actual terrain.
    unsigned dropped=changes.reconcile(tile_value,[](const Change &c,unsigned current) {
#ifdef SAN14_TERRAIN_RENDER_INCLUDED
        terrain_render::mark(c.hex,current,c.original);
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
        terrain_foam::mark(c.hex,current,c.original);
#endif
#else
        (void)c;(void)current;
#endif
    });
#ifndef NO_RUNTIME_LOG
    if(dropped)log_event("terrain_edit_reconcile",dropped,changes.size(),0);
#else
    (void)dropped;
#endif
    sync_render();
}
[[maybe_unused]] static Result apply(unsigned hex,unsigned terrain) {
    Result result=Result::Disabled;
    if(active()) {
        check_world();
        if(hex>=HexCount)result=Result::HexRange;
        else {
            uint8_t *value=tile_value(hex);
            if(!value)result=Result::MissingTile;
            else {
                unsigned old=*value;
                unsigned original=old;
                for(unsigned i=0;i<changes.size();++i)if(changes[i].hex==hex){original=changes[i].original;break;}
                result=validate(true,hex,old,terrain,false);
                if(result==Result::Applied)result=changes.edit(true,hex,old,terrain,occupied(hex));
                if(result==Result::Applied) {
                    *value=static_cast<uint8_t>(terrain);
                    clear_cost_cache();
#ifdef SAN14_TERRAIN_RENDER_INCLUDED
                    terrain_render::mark(hex,terrain,original);
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
        terrain_foam::mark(hex,terrain,original);
#endif
#else
                    (void)original;
#endif
#ifndef NO_RUNTIME_LOG
                    log_event("terrain_edit_apply",hex,old,terrain);
#endif
                    return result;
                }
            }
        }
    }
#ifndef NO_RUNTIME_LOG
    if(result!=Result::Unchanged)log_event("terrain_edit_reject",hex,static_cast<unsigned>(result),terrain);
#endif
    return result;
}
// Tile identity is resolved through the native table, never guessed Tile fields.
static unsigned tile_hex(Obj tile) {
    if(tile)for(unsigned hex=0;hex<HexCount;++hex)if(table(0xE270,hex,48399)==tile)return hex;
    return HexCount;
}
static Result apply_tile(Obj tile,unsigned terrain) {
    if(!active())return Result::Disabled;
    return apply(tile_hex(tile),terrain);
}
// Loads come through the merit load host. A new game has no such caller; stale
// edits are then dropped by check_world's reconcile against live tile values.
// Must run BEFORE native load/new-world initialization. Empty/default-off
// state does not read the world or invoke any native getter.
static void before_world_change() {
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
    terrain_foam::before_world_change();
#endif
#ifdef SAN14_TERRAIN_RENDER_INCLUDED
    // R2b: preserve render goals BEFORE CPU rollback emits original notifications.
    terrain_render::before_world_change();
#ifdef SAN14_TERRAIN_TREES_INCLUDED
    terrain_trees::before_world_change();
#endif
#endif
    reset_highlight_context(); // No getter or manager write: the old manager may already be invalid.
    reset_ui_context();
    if(changes.size() && changed_world==world())for(unsigned i=0;i<changes.size();++i) {
        const Change &c=changes[i];uint8_t *value=tile_value(c.hex);
        if(value && *value==c.next) {
            *value=c.original;
#ifdef SAN14_TERRAIN_RENDER_INCLUDED
            terrain_render::mark(c.hex,c.original,c.original);
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
        terrain_foam::mark(c.hex,c.original,c.original);
#endif
#endif
        }
    }
    changes.clear();
    changed_world=nullptr;
}
}
