#pragma once
// Core services (docs/MODULARIZATION_PLAN.md W4-2..4-6). A service is a small ordered list that modules join through the generated
// registry (development/module_registry.json "subscriptions"), instead of the adapter naming each module. The lists are filled once
// at prepare time (before any hook is published), cleared on rollback, and only read afterwards: the per-call cost is the same
// loop-over-a-few-entries that the hard-coded calls were. Call order = (order, registration order); modules that need an explicit
// position carry an `order` constant (tests/core_services_fixture.cpp proves the historical sequences for every registration order).
#include <windows.h>
#include "core_module.h"
using Obj=unsigned char *;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct D3D11_TEXTURE2D_DESC;
namespace services {
// A flag that is switched off (feature_guard_mismatch id) when the service's own guard fails: "service guard failure = all
// subscribers off".
struct Target {bool *flag;unsigned id;};
template<class Fn,unsigned N=8> struct Subscribers {
    struct Row {const char *owner;Fn fn;bool (*active)();unsigned order;};
    Row rows[N]={};unsigned count=0;
    Target fails[N]={};unsigned fail_count=0;
    // Stable insert by order: equal orders keep registration order.
    bool add(const char *owner,Fn fn,bool (*active)(),unsigned order) {
        if(count>=N)return false;
        unsigned at=count;
        while(at>0 && rows[at-1].order>order) {rows[at]=rows[at-1];--at;}
        rows[at]={owner,fn,active,order};++count;return true;
    }
    bool on_fail(bool *flag,unsigned id) {if(fail_count>=N)return false;fails[fail_count++]={flag,id};return true;}
    bool live(unsigned i) const {return !rows[i].active || rows[i].active();}
    bool any() const {for(unsigned i=0;i<count;++i)if(live(i))return true;return false;}
    // FNV over the owners in call order (svc_install log, compared with the fixture).
    unsigned order_hash() const {unsigned h=kModuleHashSeed;for(unsigned i=0;i<count;++i)h=module_name_hash(rows[i].owner,h);return h;}
    void clear() {count=0;fail_count=0;}
};
// map-tick: per-frame work on the game thread inside the map states' busy slot (installed by fast_menu).
using MapTick=void (*)();
static Subscribers<MapTick> map_tick;
inline void run_map_tick() {for(unsigned i=0;i<map_tick.count;++i)if(map_tick.live(i))map_tick.rows[i].fn();}
// domestic-tick: GA 297070 (the per-city domestic tick). pre runs before the native tick, post after it for the subscribers whose
// pre ran. Failing guard of the hook switches all subscribers off.
struct DomesticTick {void (*pre)(Obj);void (*post)(Obj);};
static Subscribers<DomesticTick> domestic_tick;
// appear-prefetch: frame hook of the army-appearance update (appear_batch) before its batch runs; face_keep subscribes to fetch the faces of the
// armies about to appear. Without a live subscriber the call does nothing (as the former direct call, which returned at once with face_prefetch off).
using AppearPrefetch=void (*)(Obj);
static Subscribers<AppearPrefetch> appear_prefetch;
inline void run_appear_prefetch(Obj state) {for(unsigned i=0;i<appear_prefetch.count;++i)if(appear_prefetch.live(i))appear_prefetch.rows[i].fn(state);}
// ui-dialogs: users of the native dialog hooks (frame/controls/destroy slots of native_unified_dialogs.inl); the hooks are
// published only while a user is live. fn unused.
static Subscribers<void (*)()> ui_dialogs;
// progress-flush: runs on the AI thread right after the timed original AI root (progress_native_ai); path_count flushes its per-thread
// counters here.
using ProgressFlush=void (*)();
static Subscribers<ProgressFlush> progress_flush;
inline void run_progress_flush() {for(unsigned i=0;i<progress_flush.count;++i)if(progress_flush.live(i))progress_flush.rows[i].fn();}
// overlay: the shared Present overlay. gate = cheap "has something to draw" probe that lets draw() skip the frame; needs = per frame
// draw probe (null = always), draw = the work. flags: kContext needs a device context, kWindow binds the game window and records
// the buffer size first, kPerfTerrain is timed as terrain_us. input = window procedure hook (true = consumed).
enum : unsigned {kOverlayContext=1,kOverlayWindow=2,kOverlayPerfTerrain=4};
// Draw order constants (historical order of draw_locked).
enum : unsigned {kOverlayTerrain=10,kOverlaySprites=20,kOverlayPreview=30,kOverlayBriefing=40};
// Entry-gate probe order constants (historical order of the gate in draw(): briefing, terrain, sprites, preview).
enum : unsigned {kGateBriefing=10,kGateTerrain=20,kGateSprites=30,kGatePreview=40};
struct OverlaySub {
    unsigned flags;unsigned gate_order;
    bool (*gate)();bool (*needs)();
    void (*draw)(ID3D11Device *,ID3D11DeviceContext *,ID3D11Texture2D *,const D3D11_TEXTURE2D_DESC &,bool);
    bool (*input)(HWND,UINT,WPARAM,LPARAM);
};
static Subscribers<OverlaySub> overlay;
inline void reset_all() {map_tick.clear();domestic_tick.clear();appear_prefetch.clear();progress_flush.clear();ui_dialogs.clear();overlay.clear();}
}
