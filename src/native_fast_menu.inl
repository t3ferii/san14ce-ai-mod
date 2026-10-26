// [performance] fast_menu (2026-10-23, user request; the "SkipInWait" part of reference_mod
// SAN14_FastMenu, re-derived from the original; its "PinSlowLayouts" is this mod's layout_keep).
// Original (REA, unpacked 1fbe6be7...): 56FF60(state), vtable slot +30 of every UI state, answers
// "still busy, ignore input": the state's own layout (+60) animating in (+110) or out (+118); phase +6C
// 1 = wait until no map layout animates in (86EEB0) then 2; phase 4 = wait for out animations (86F110)
// then 5. After a city/unit menu closes, the map states sit in phase 1 for the whole HUD slide-in.
// Mod: only for the three map states (RTTI CUserStrategyState 13D63A0, CStrategyUserCityHexState
// 143C498, CStrategyUnitTargetState 1457208) phase 1 advances to 2 at once, so the next click is taken
// while the HUD still slides in. Own-layout waits and phase 4 (menu opening) stay original.
// The same slot also runs lord_change's per-frame status fix (installed when either feature is on).
namespace lord_change {static void map_tick();}
namespace fast_menu {
static const unsigned kBusy=0x56FF60,kSlots[3]={0x13D63A0+0x30,0x143C498+0x30,0x1457208+0x30};
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
static unsigned long long __fastcall busy(Obj state) {
    if(mod_features.lord_change)lord_change::map_tick();   // game thread, map states only
    const bool waiting=state && dword(state,0x6C)==1;
#ifndef NO_RUNTIME_LOG
    if(diagnostic_logging && waiting)wait_begin(state);
#endif
    if(mod_features.fast_menu && waiting)*reinterpret_cast<int *>(state+0x6C)=2;
    const unsigned long long result=fn<unsigned long long(__fastcall *)(Obj)>(kBusy)(state);
#ifndef NO_RUNTIME_LOG
    if(diagnostic_logging && !result)wait_end(state);
#endif
    return result;
}
static bool install() {
    for(unsigned slot:kSlots)if(pointer(image,slot)!=image+kBusy) {log_event("fast_menu_guard_rejected",slot);return false;}
    unsigned done=0;
    for(;done<3;++done)if(!patch_pointer(kSlots[done],kBusy,reinterpret_cast<void *>(&busy)))break;
    if(done!=3) {while(done){--done;restore_pointer(kSlots[done],kBusy,reinterpret_cast<void *>(&busy));}return false;}
    log_event("fast_menu_installed");return true;
}
}
