#pragma once
// [game_flow] ambush (user 2026-11-11, docs/AMBUSH.md): one table shared by the two DLLs.
// SAN14_Convenience.dll owns the state (tickets, sidecar, UI) and is the only writer (game UI thread).
// SAN14_CombatAI.dll owns the effects and only reads it: export SAN14CombatAmbush(kAmbushVersion).
// expiry[leader person id] = first absolute game day (32DD00) on which the ambush is over; 0 = none.
#include <windows.h>
static const unsigned kAmbushVersion=2,kAmbushPeople=6001;
// local67 (user 2026-11-12): at most 120 days from the departure; from the first day the army is under ZOC (the
// original 2B7BC0 score > 0: hostile armies within 3 hexes or hostile-owned hexes around it) 30 more days at most.
// The combat module shortens expiry[] on that day (compare-exchange, only ever to an earlier day).
// local68 (user 2026-11-12): 180 days at most, 60 from the first ZOC day; the mobility is halved only until that day
// (contact[leader] = 1 from then on, written by the combat module, cleared with the entry).
static const int kAmbushMaxDays=180,kAmbushContactDays=60;
struct AmbushTable {
    unsigned size,version;
    volatile LONG count;                 // entries with a non-zero expiry (readers skip all work at 0)
    volatile LONG expiry[kAmbushPeople];
    volatile LONG contact[kAmbushPeople];
};
using GetAmbushTable=AmbushTable *(__cdecl *)(unsigned version);
