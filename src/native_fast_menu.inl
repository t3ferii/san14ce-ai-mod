// [performance] fast_menu (2026-10-23, user request; the "SkipInWait" part of reference_mod
// SAN14_FastMenu, re-derived from the original; its "PinSlowLayouts" is this mod's layout_keep).
// Original (근거: 1a4a447d 이전 기록, docs/HISTORY.md): 56FF60(state), vtable slot +30 of every UI state, answers
// "still busy, ignore input": the state's own layout (+60) animating in (+110) or out (+118); phase +6C
// 1 = wait until no map layout animates in (86EEB0) then 2; phase 4 = wait for out animations (86F110)
// then 5. After a city/unit menu closes, the map states sit in phase 1 for the whole HUD slide-in.
// Mod: only for the three map states (RTTI CUserStrategyState 13D63A0, CStrategyUserCityHexState
// 143C498, CStrategyUnitTargetState 1457208) phase 1 advances to 2 at once, so the next click is taken
// while the HUD still slides in. Own-layout waits and phase 4 (menu opening) stay original.
// The same slot also runs lord_change's per-frame status fix (installed when either feature is on).
#include "game_addresses.h"
namespace fast_menu {
static const unsigned kBusy=GA(GA_5727C0),kSlots[3]={GA(GA_13D9430)+0x30,GA(GA_143F520)+0x30,GA(GA_145A298)+0x30};
static unsigned long long (__fastcall *next_busy[3])(Obj);   // Layout manager: next of each slot (original 56FF60 unless another mod chained first)
#ifndef NO_RUNTIME_LOG
// Diagnostics (2026-10-25, user: "is it really faster?"): per map state, the time from the first
// phase-1 frame (menu closed, HUD slide-in wait) to the first frame 56FF60 returns 0 (input taken
// again). log fast_menu_wait army=fast_menu value=microseconds hex=state vtable RVA. Compare
// fast_menu=0/1 on the same menu close; the hook's call count is no evidence of an effect.
// The hook exists with fast_menu=0 only while lord_change=1 (same slot).
struct Wait {Obj state; LONGLONG start;};
static Wait waits[3];
static void wait_begin(Obj state) {
    Wait *free_slot=nullptr;
    for(auto &w:waits) {if(w.state==state)return; if(!w.state && !free_slot)free_slot=&w;}
    if(!free_slot)return;
    LARGE_INTEGER now;QueryPerformanceCounter(&now);free_slot->state=state;free_slot->start=now.QuadPart;
}
static void wait_end(Obj state) {
    for(auto &w:waits)if(w.state==state) {
        LARGE_INTEGER now,hz;QueryPerformanceCounter(&now);QueryPerformanceFrequency(&hz);
        const LONGLONG us=(now.QuadPart-w.start)*1000000/hz.QuadPart;
        log_event("fast_menu_wait",mod_features.fast_menu?1u:0u,static_cast<unsigned>(us),
                  static_cast<unsigned>(pointer(state,0)-image));
        w.state=nullptr;return;
    }
}
#endif
#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG)
static void record_state(Obj state) {
    uintptr_t table=0;SIZE_T read=0;
    COST_CRUMB("fast_menu_state",state);
    if(state && ReadProcessMemory(GetCurrentProcess(),state,&table,sizeof(table),&read) && read==sizeof(table))
        COST_CRUMB("fast_menu_vt",reinterpret_cast<void *>(table-reinterpret_cast<uintptr_t>(image)));
}
#endif
template<unsigned I> static unsigned long long __fastcall busy(Obj state) { COST_SCOPE("fast_menu::busy");
#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG)
    if(costprofile::call_trace_enabled)record_state(state);
#endif
    services::run_map_tick();   // game thread, map states only (subscribers: lord_change status fix)
    const bool waiting=state && dword(state,0x6C)==1;
#ifndef NO_RUNTIME_LOG
    if(diagnostic_logging && waiting)wait_begin(state);
#endif
    if(mod_features.fast_menu && waiting)*reinterpret_cast<int *>(state+0x6C)=2;
    const unsigned long long result=next_pick(next_busy[I],kBusy)(state);
#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG)
    if(costprofile::call_trace_enabled)record_state(state);
#endif
#ifndef NO_RUNTIME_LOG
    if(diagnostic_logging && !result)wait_end(state);
#endif
    return result;
}
static bool install() {
    // 8.7: R1 FastMenu does the same busy-slot job. The phase skip yields; lord_change's per-frame fix needs the slot
    // anyway, so with lord_change on the hooks stay (without the YIELD flag) and only the skip is dropped.
    const bool peer=layout_client::yield_to_peer("fast_menu",kSlots[0]);
    if(peer)mod_features.fast_menu=false;
    const bool ticks=services::map_tick.any();   // lord_change subscribes its per-frame status fix
    if(peer && !ticks)return true;
    const uint32_t drop=ticks?LAYOUT_YIELD_IF_PEER:0;
    layout_client::Transaction tx;
    if(!layout_client::vt(kSlots[0],reinterpret_cast<void *>(&busy<0>),reinterpret_cast<void **>(&next_busy[0]),drop) ||
       !layout_client::vt(kSlots[1],reinterpret_cast<void *>(&busy<1>),reinterpret_cast<void **>(&next_busy[1]),drop) ||
       !layout_client::vt(kSlots[2],reinterpret_cast<void *>(&busy<2>),reinterpret_cast<void **>(&next_busy[2]),drop)) {
        log_event("fast_menu_register_rejected");return false;
    }
    tx.done=true;log_event("fast_menu_installed");return true;
}
}
