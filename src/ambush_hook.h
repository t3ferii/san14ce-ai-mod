#pragma once
// [game_flow] ambush (docs/AMBUSH.md): the calls the combat AI makes into the ambush links. They are defined in native_ambush_combat.inl,
// later in the same combat unit; the AI files include this header instead of declaring them (feature_deps D4, 2026-11-16).
namespace ambush_hook {static bool active(Obj army);static void contact(Obj army);static bool slow(Obj army);}
