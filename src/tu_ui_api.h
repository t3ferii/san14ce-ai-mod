#pragma once
// Interface of the "ui" translation unit (tu_convenience_ui.cpp, SAN14_MULTI_TU builds; 2026-11-16 refactor, docs/REFACTOR_EQUIVALENCE.md
// section 4). SAN14_MULTI_TU builds only: the unit's source includes it before its modules (each declaration is checked against its
// definition) and the main unit includes it where the unit's first module used to be. Unity builds (fixtures, hook map, dependency
// scans) never see it, so the modules keep their own text there.
// Three parts: (1) shared services both units compile (stateless static helpers, C++17 inline state, types); (2) main-unit definitions
// the unit calls; (3) unit definitions the main unit calls. Nothing else crosses the cut. A default argument stays on the definition
// only, so the main unit passes every argument (ai_trace::install).
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

// (1) shared services
#include "native_layout_client.inl"
#include "native_ui_callable.inl"
#include "unified_overlay_policy.h"   // unified_overlay::eligible
#include "terrain_call_profile.h"     // terrain_profile (inline state; native_split_install.inl reads it)
#include "tu_terrain_api.h"
#include "ic_gate.h"                  // ic_gate::late (inline state; native_adapter.cpp sets it, the advisor reads it)
struct ID3D11Texture2D;
struct D3D11_TEXTURE2D_DESC;

// (2) defined by the main unit

// (3) defined by the unit
namespace briefing_overlay {
bool available();
void toast(const std::wstring &text);
void toast_for(const std::wstring &text,unsigned ms);
void publish_hidden();
bool needs_draw();
void draw(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc);
bool input(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam);
}
namespace unified_overlay {
void show(bool value);
bool take_click();
}
namespace unified_dialogs {
extern void *next_frame,*next_controls,*next_destroy;   // split_pointers rows (module_pointers.inl)
void __fastcall frame(Obj object);
void __fastcall controls(Obj object);
Obj __fastcall destroy(Obj object,unsigned flags);
}
namespace ccz_preview {
bool needs_draw();
void draw(ID3D11Device *device,ID3D11DeviceContext *context,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc,bool rgba);
}
namespace ccz_sprites {
bool install(bool hide,bool depth,bool far_keep,bool fallback);
bool install_idle_flags(bool depth);   // 의용병 무리 idle flags only (native_ccz_bridge.inl)
bool needs_present();
void on_present(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc);
}
bool ccz_sprites_guards_match();
namespace ccz_combat {bool install(bool block,bool critical);}
namespace ccz_title {bool install();}
namespace ccz_bands {bool install(bool dev);extern const save_host::SidecarPlugin sidecar_plugin;}   // 종군 중립군 (native_ccz_bands.inl)
namespace advisor_briefing {
bool install();
void hud(Obj layout,bool eligible,bool modal);
bool add_item(const std::wstring &text);
void publish_late();   // ic_gate: the 9332B0 text link after the IC check (native_adapter.cpp late worker)
}
bool advisor_briefing_guards_match();
namespace diplomacy_intel {bool install();}
bool diplomacy_intel_guards_match();
namespace season_rules {
bool can_run();
void install();
}
namespace building_destruction {
extern bool (*ticket_use)(Obj person,Obj region);     // set by ambush::install (main unit)
extern bool (*ticket_ready)(Obj person,Obj region);
bool install();
}
namespace ai_trace {bool install(bool original,bool cost);}
namespace speed_buttons {bool install();}
namespace save_codec_hook {bool install();}
namespace caravan_stay {bool install();}
namespace face_keep {
bool install();
void prefetch(Obj state);
}
namespace mediation_plus {bool install();}
namespace drag_learn {bool install();}
namespace region_officers {bool install();}
namespace layout_wait {bool install();}
namespace damage_forecast {bool install();}
namespace officer_rotation {void tick(Obj);}
#ifdef RUNTIME_COST_PROFILE
namespace face_timing {bool install();}
#endif
