#pragma once
// Interface of the "terrain" translation unit (tu_convenience_terrain.cpp, SAN14_MULTI_TU builds; 2026-11-16 refactor).
// SAN14_MULTI_TU builds only: the unit's source includes it before the modules (each declaration is checked against its
// definition) and the main unit includes it where the modules used to be. Unity builds (fixtures, hook map, dependency scans)
// never see it, so the modules keep their own definitions there. What the rest of the convenience root uses from the unit is
// declared here and nothing else. Values another unit reads at load time are `inline` here: C++17 initializes an inline
// variable before any later definition of the same unit, so module_pointers.inl never copies an unset slot
// (tools/unit_equivalence.py audit + split_module_fixture SAN14SplitFixturePointerRows check it).
#include "save_host.h"
struct ID3D11Device;
struct ID3D11DeviceContext;

// Include markers the rest of the root tests (same conditions as in the modules; tools/gen_module_table.py checks the list).
#ifndef SPLIT_COMBAT
#define SAN14_TERRAIN_FOAM_INCLUDED 1
#endif
#define SAN14_TERRAIN_TREES_INCLUDED 1
#ifndef NO_RUNTIME_LOG
#define SAN14_WATER_DIAG_INCLUDED 1
#endif
#define SAN14_TERRAIN_RENDER_INCLUDED 1

#ifndef SPLIT_COMBAT
namespace terrain_foam {
extern bool permitted;
bool active();
void restore();
void install();
}
bool terrain_foam_guards_match();
#endif

namespace terrain_trees {
inline const unsigned Slot=GA(GA_1697960),UpdateRva=GA(GA_BB3AC0);   // split_pointers row (module_pointers.inl)
#ifndef NO_RUNTIME_LOG
extern volatile LONG64 perf_calls,perf_total_us,perf_frame_us,perf_max_us;   // read and reset by the unified overlay
#endif
extern bool permitted;
uint64_t __fastcall update(Obj s,Obj second,Obj context,Obj view,Obj fifth,unsigned sixth,unsigned seventh,unsigned eighth);
}
bool terrain_trees_guards_match();

#ifndef NO_RUNTIME_LOG
namespace water_diag {
extern bool permitted;
void restore();
}
bool water_diag_guards_match();
#endif

namespace terrain_render {
constexpr unsigned CreateTexture2DSlot=5;
extern bool permitted;
bool active();
bool needs_present();
bool hooks_active();
unsigned season_sync(int (__cdecl *kind)(int));
void heightfield_created(unsigned index,unsigned char *object,const void *vtable);
void on_present(ID3D11DeviceContext *context);
void on_device(ID3D11Device *dev);
void install_early();
void restore();
#if defined(SPLIT_MODULE_FIXTURE)
void fixture_mark_published(void **table);
#endif
namespace season_bake {
extern unsigned (__cdecl *weather_params)(float *out,int count);
}
}
bool terrain_render_guards_match();

#if defined(MERIT_CANDIDATE)
namespace terrain_edit {
struct MenuEntry;
extern unsigned flood_count;
void reset_ui_context();
unsigned flood_sync(int (__cdecl *want)(int));
uint64_t __fastcall hex_menu_state(Obj state);
uint64_t __fastcall menu_build(Obj menu,int x,int y,uint64_t flag,MenuEntry *entries,int count);
uint64_t __fastcall hex_menu_click(Obj functor,void **arg);
uint64_t __fastcall hex_menu_hint(Obj functor,void **arg);
uint64_t __fastcall hex_menu_exit(Obj state,uint64_t rdx);
uint64_t __fastcall domestic_click(Obj functor,void **arg);
extern const save_host::SidecarPlugin sidecar_plugin;
}
bool terrain_edit_guards_match();
#endif

#if defined(SPLIT_BASE)
namespace unified_overlay {void install();}   // ui unit (native_unified_overlay.inl); the terrain editor starts the overlay
#endif
