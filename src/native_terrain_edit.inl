// Convenience engine only. Call on the native UI/save host caller thread.
// Menu binding is separate in native_terrain_edit_ui.inl. No worker is used.
#include "game_addresses.h"
#include "terrain_edit_policy.h"
#include "terrain_call_profile.h"
#if defined(SPLIT_BASE)
namespace unified_overlay {static void install();}
#endif
namespace terrain_edit {
static Changes changes;
static Obj changed_world;
// local132 (user 2026-11-14 "홍수 일어나면 육지타일을 강으로 바꾸고 싶어 자연스럽게"): while the season rules' flood water stands on flat
// open land the cell is 여울 (12) or 강 (13) - walkable land in the original (cost 375 / 450) - drawn as water by terrain_render like an
// edit. Not an edit: never in the .terrain sidecar, given back when the water leaves and before any world change. Cells with an army or
// a building wait (as the editor refuses them); a cell something else changed is let go; the editor refuses a flooded cell.
static uint8_t flood_from[HexCount],flood_to[HexCount];
static unsigned flood_count;
static Obj flood_world;
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
    0x48,0x8B,0x3D,0xB1,0xBE,0xAD,0x01,0x48,0x81,0xC7,0xC0,0x00,0x00,0x00,0x33,0xC0,0xB9,0x40,0x42,0x0F,0x00,0xF3,0xAA};
static void clear_cost_cache() { COST_SCOPE("terrain_edit:clear_cost_cache");
#ifdef TERRAIN_EDIT_FIXTURE_COST_CACHE
    terrain_edit_fixture_cost_cache_clear();
#else
    if(ga::mcmp(image,GA_C1638,0,cost_cache_clear_bytes,sizeof(cost_cache_clear_bytes)))return;
    Obj cache=pointer(image,GA(GA_1B9D4F0));
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
    if(resync && flood_count)for(unsigned hex=0;hex<HexCount;++hex)if(flood_from[hex])terrain_render::mark(hex,flood_to[hex],flood_from[hex]);
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
                if(result==Result::Applied && flood_from[hex])result=Result::Occupied;   // local132: under flood water
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
static void flood_mark(unsigned hex,unsigned terrain,unsigned original) {
#ifdef SAN14_TERRAIN_RENDER_INCLUDED
    terrain_render::mark(hex,terrain,original);
#ifdef SAN14_TERRAIN_FOAM_INCLUDED
    terrain_foam::mark(hex,terrain,original);
#endif
#else
    (void)hex;(void)terrain;(void)original;
#endif
}
static void flood_release(unsigned hex,bool restore) {
    if(!flood_from[hex])return;
    if(restore) {uint8_t *value=tile_value(hex);if(value && *value==flood_to[hex]) {*value=flood_from[hex];flood_mark(hex,flood_from[hex],flood_from[hex]);}}
    flood_from[hex]=flood_to[hex]=0;--flood_count;
}
// local137 (user 2026-11-14 "결빙타일 우리가 새로 만들어서 설명 쓰면 되잖아"): the 「결빙」 terrain. The original terrain table (fixdata
// table 8) has 31 rows and the map uses 1..24; rows 25..30 are empty (name 「지형」, cost 0). The terrain objects (world+0x73860, 0x48
// bytes each, laid out one after another - 22FB60 numbers them by (object - first) / 0x48) carry the row from +0x10 (the name, UTF-16,
// twice at +0x10 / +0x18; cost +0x2A; flags +0x2C; morale +0x40; fire +0x41; kind +0x42). Object 25 takes 평지's values (the bytes that
// count the row - equal to 1 / 2 / 3 in objects 1 / 2 / 3 - are left alone), no fire, and the name 「결빙」, so the cell panel and every
// rule read it as frozen ground. Checked first: the objects are contiguous, row 25 is still the empty row, 평지's name is where expected.
// Off: row 25 is given back byte for byte.
static constexpr unsigned kIceTerrain=25;
static uint8_t ice_saved[0x48];static bool ice_patched;static Obj ice_object;
static bool ice_terrain(bool on) {
    Obj plain=table(0x73860,1,30),road=table(0x73860,2,30),sand=table(0x73860,3,30),ice=table(0x73860,kIceTerrain,30);
    if(!on) {
        if(ice_patched && ice==ice_object && valid(ice)) {memcpy(ice,ice_saved,sizeof(ice_saved));log_event("season_ice_terrain",0,0,0);}
        ice_patched=false;return false;
    }
    if(!valid(plain) || !valid(road) || !valid(sand) || !valid(ice))return false;
    static const wchar_t name[4]=L"결빙\0",plain_name[2]={L'평',L'지'};
    if(ice_patched && ice==ice_object && !memcmp(ice+0x10,name,sizeof(name)))return true;
    static bool refused;
    if(road!=plain+0x48 || ice!=plain+0x48*(kIceTerrain-1) || memcmp(plain+0x10,plain_name,sizeof(plain_name)) || word(ice,0x2A)!=0 || word(plain,0x2A)==0) {
        if(!refused) {refused=true;log_event("season_ice_terrain_refused",static_cast<unsigned>(road-plain),word(ice,0x2A),*reinterpret_cast<const unsigned *>(plain+0x10));}
        return false;
    }
    memcpy(ice_saved,ice,sizeof(ice_saved));ice_object=ice;
    for(unsigned o=0x10;o<0x48;++o)if(!(plain[o]==1 && road[o]==2 && sand[o]==3))ice[o]=plain[o];
    memcpy(ice+0x10,name,sizeof(name));memcpy(ice+0x18,name,sizeof(name));ice[0x41]=0;   // ice does not burn
    ice_patched=true;log_event("season_ice_terrain",1,word(ice,0x2A),ice[0x2C]);
    return true;
}
// Game thread, right after the season rules' day. want(cell) -> 0 / 12 / 13 (flood on 평지..습지) / 1 (local133: ice on 대하 14);
// nullptr gives every cell back. Returns cells changed.
// local135 (user 2026-11-14 "지형이 바뀌거나 원래대로 돌아올때 양쪽다 건물은 파괴하자. 야전부대도 무시하고"): field armies no longer
// hold a cell back (the game reads the new terrain itself); a building on a cell that changes either way (owner any, neutral too) is
// removed first with the original removal 294B90 (GA_296AA0, the call 건물파괴 makes at durability 0). Only facility kinds 2..4 of the
// type table 73958 (+0x2C) are removed - never cities or gates; a cell with any other building keeps its terrain. The player's own
// terrain edits are never touched.
static unsigned flood_sync(int (__cdecl *want)(int)) {
    if(!want && !flood_count) {if(ice_patched)ice_terrain(false);return 0;}
    if(flood_count && flood_world!=world()) {for(unsigned hex=0;hex<HexCount;++hex)if(flood_from[hex]){flood_from[hex]=flood_to[hex]=0;}flood_count=0;}
    static uint8_t blocked[HexCount];memset(blocked,0,sizeof(blocked));   // 2 player edit, 3 removable facility, 4 city / gate / other
    static uint16_t building_on[HexCount];
    for(unsigned id=1;id<=3000;++id) {
        Obj building=table(0x6DA98,id,3000);if(!valid(building) || word(building,0x10)>=HexCount)continue;
        const unsigned hex=word(building,0x10);Obj definition=building[0x12]<=30?table(0x73958,building[0x12],30):nullptr;
        const bool facility=valid(definition) && definition[0x2C]>=2 && definition[0x2C]<=4;
        if(blocked[hex]!=4) {blocked[hex]=facility?3:4;building_on[hex]=static_cast<uint16_t>(id);}
    }
    for(unsigned i=0;i<changes.size();++i)blocked[changes[i].hex]=2;   // the player's own edit: never flooded
    const unsigned remove=GA(GA_296AA0);unsigned changed=0,broken=0;
#ifdef SAN14_TERRAIN_RENDER_INCLUDED
    terrain_render::begin_batch();   // local142: the heights of all changed cells at once, at the end
#endif
    const unsigned ice_id=want && ice_terrain(true)?kIceTerrain:1u;   // local137: 「결빙」 when row 25 took it, else 평지
    auto clear=[&](unsigned hex)->bool {   // true when nothing stands on the cell any more
        if(blocked[hex]==4)return false;
        if(blocked[hex]!=3)return true;
        Obj building=table(0x6DA98,building_on[hex],3000);
        if(!valid(building) || word(building,0x10)!=hex) {blocked[hex]=0;return true;}
        if(!remove)return false;
        fn<void(__fastcall *)(Obj,Obj,int)>(remove)(building,nullptr,0);++broken;
        Obj again=table(0x6DA98,building_on[hex],3000);
        if(valid(again) && word(again,0x10)==hex)return false;   // still standing: leave the terrain as it is
        blocked[hex]=0;return true;
    };
    // local143 (user 2026-11-14 "평지를 습지로 습지를 여울로 여울을 강으로 강을 대하로 ... 건축물이 너무 자주 부숴지면 안되니까"): the
    // rules step a flooded cell up 평지/가도/모래 -> 습지 4 -> 여울 12 -> 강 13 -> 대하 14 (a river cell too). A building is removed
    // only when its cell becomes water (12/13/14, the ice forms) or comes back from it; a cell turning 습지 or drying from 습지 keeps it.
    auto wet=[](unsigned t) {return t==12 || t==13 || t==14;};
    for(unsigned hex=0;hex<HexCount;++hex) {
        int goal=want && blocked[hex]!=2?want(static_cast<int>(hex)):0;
        if(goal==1)goal=static_cast<int>(ice_id);
        const bool ice=goal==1 || goal==static_cast<int>(kIceTerrain);
        const bool water=goal==4 || wet(static_cast<unsigned>(goal)) || ice;
        if(flood_from[hex]) {
            uint8_t *value=tile_value(hex);
            if(!value || *value!=flood_to[hex]) {flood_release(hex,false);continue;}
            if(water && goal==flood_to[hex])continue;
            const unsigned next=water?static_cast<unsigned>(goal):flood_from[hex];
            const bool razes=ice || wet(next) || flood_to[hex]==1 || flood_to[hex]==kIceTerrain || (wet(flood_to[hex]) && !wet(next) && next!=4);
            if(razes && !clear(hex))continue;
            if(blocked[hex]==4)continue;
            if(water && goal==flood_from[hex]) {flood_release(hex,true);++changed;continue;}
            if(water) {*value=static_cast<uint8_t>(goal);flood_to[hex]=static_cast<uint8_t>(goal);flood_mark(hex,goal,flood_from[hex]);++changed;continue;}
            flood_release(hex,true);++changed;
        } else if(water && blocked[hex]!=2 && blocked[hex]!=4) {
            uint8_t *value=tile_value(hex);
            if(!value || *value==goal || (ice?*value!=14:!((*value>=1 && *value<=4) || *value==12 || *value==13)))continue;
            if((ice || wet(static_cast<unsigned>(goal))) && !clear(hex))continue;
            if(!flood_count)flood_world=world();
            flood_from[hex]=*value;flood_to[hex]=static_cast<uint8_t>(goal);*value=static_cast<uint8_t>(goal);++flood_count;flood_mark(hex,goal,flood_from[hex]);++changed;
        }
    }
#ifdef SAN14_TERRAIN_RENDER_INCLUDED
    terrain_render::end_batch();
#if !defined(NO_RUNTIME_LOG)
    {   // local156 probe: the first iced cell (hex = y*220+x), its game value and from-terrain, its render state
        static unsigned probes;
        if(changed && probes<400)for(unsigned hex=0;hex<HexCount;++hex)if(flood_from[hex] && flood_to[hex]==kIceTerrain) {
            uint8_t *value=tile_value(hex);int height=0;const unsigned state=terrain_render::probe_cell(hex,&height);
            log_event("season_ice_probe",hex,(value?*value:255u)|(static_cast<unsigned>(flood_from[hex])<<8)|(static_cast<unsigned>(height&0xFFFF)<<16),state);
            ++probes;break;
        }
    }
#endif
#endif
    if(changed)clear_cost_cache();
    if(broken)log_event("season_terrain_building_broken",broken,flood_count,0);
    if(!want && !flood_count)ice_terrain(false);   // every cell given back: row 25 is the original row again
    return changed;
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
    if(flood_count) {   // local132: flood water given back to the old world before it goes
        const bool same=flood_world==world();
#ifdef SAN14_TERRAIN_RENDER_INCLUDED
        terrain_render::begin_batch();
#endif
        for(unsigned hex=0;hex<HexCount;++hex)if(flood_from[hex])flood_release(hex,same);
#ifdef SAN14_TERRAIN_RENDER_INCLUDED
        terrain_render::end_batch();
#endif
        flood_count=0;flood_world=nullptr;
    }
}
}
