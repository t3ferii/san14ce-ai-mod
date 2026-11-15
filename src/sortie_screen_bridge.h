#pragma once
// Owned by tactic-priority: the sortie setup screen (CStrategyUnitMakeState) has one owner of its update slot, leave
// slot and departure CALL (layout_registry.h rows 258-261). Another feature on the same screen plugs in here instead of
// registering the same sites again. The ambush module ([군사제안수행] toggle) sets it at install; nullptr = nothing.
// Its panes are created under the tactic_priority UI owner, so tactic_priority's leave releases them with its own.
namespace sortie_bridge {
using Obj=unsigned char *;
struct Hooks {
    void (*update)(Obj state);                       // before the original update, after tactic_priority's own pass
    void (*leave)(Obj state,void *next,bool released);   // released = tactic_priority already released its owner's panes
    void (*depart)(unsigned person,Obj army);        // an officer really left (army != nullptr)
};
inline const Hooks *extra=nullptr;
}
