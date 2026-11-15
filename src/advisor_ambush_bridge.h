#pragma once
// Owned by advisor-briefing (same pattern as advisor_terrain_bridge.h). The ambush module (installer Requires
// game_flow.advisor_briefing) plugs its monthly tick, blue head count and one-time proposal into the report here at
// install. nullptr = no ambush: the report and the head stay exactly as before (off = original).
namespace advisor_bridge {
struct AmbushHooks {
    void (*tick)();               // player frame, outside the progress phase
    unsigned (*badge)();          // blue count left of the head = pending proposals only (0 = none)
    bool (*offer_pending)();      // first head click of this turn opens the proposal
    void (*offer_shown)();
    void (*answer)(bool accept);  // 수락 / 거절
    unsigned (*offers)();         // pending proposals: the report shows a [군사 제안] button that reopens one (local63)
    unsigned (*tickets)();        // ready tickets (blue head count)
    void (*status)(std::wstring &out);   // one report line: armies on an ambush + ready tickets; empty = none (local67)
    unsigned (*offer_kind)();     // kind of the pending proposal: 0 ambush, 1 building destruction (destruction_ticket)
};
inline const AmbushHooks *ambush=nullptr;
}
