// [game_flow] ambush, combat side (user 2026-11-11, docs/AMBUSH.md). SAN14_CombatAI.dll.
// SAN14PK: a unit the player ordered to retreat is not seen by computer missions/target searches; it is still
// attacked at close range. The original reason (San14Sim/docs/orig/rules_movement.md, retreat blindness):
// the behaviour-tree nodes skip units whose action byte Army+0x37 is 11/12 (retreat), through condition 29
// = AEB40(p1,p2,args) (args[0] object handle, args[1] action value), reached only from the condition
// dispatcher C36A0 case 0x1D at the CALL C39F4.
// Effect: for a player army whose leader holds a live ticket in the shared table, condition 29 answers
// "yes" for 11/12. Nothing is written into the army; Army+0x37 keeps the real action, so the player's own
// orders, combat and the close-range engage in 61F10 (direct +0x37 read, not condition 29) stay original.
// Mobility: half, applied in sortie_movement (native_sortie.inl) on top of the native 2B2550 value.
// Switch off / DLL absent = the CALL is restored and the table is never read (off = original).
#include "ambush_shared.h"

static Obj allocate_near(Obj call,unsigned bytes);   // defined later in native_adapter.cpp

namespace ambush_hook {
static AmbushTable shared={sizeof(AmbushTable),kAmbushVersion,0,{}};
using Cond=unsigned long long(__fastcall *)(Obj,Obj,unsigned *);
using Resolve=char(__fastcall *)(Obj,unsigned,unsigned long long *,int);

// A player army whose leader has a live ticket. Pure reads plus the same validity/force getters the AI uses.
static bool active(Obj army) { COST_SCOPE("ambush:active");
    if(!mod_features.ambush || !shared.count || !army)return false;
    if(pointer(army,0)!=image+GA(GA_1345458))return false;   // Army record only (other handles go untouched)
    const unsigned leader=word(army,0x12);
    if(!leader || leader>=kAmbushPeople)return false;
    const LONG until=shared.expiry[leader];
    // local65 (user 2026-11-12: the unit panel showed the full mobility right after the order): the pending-command
    // byte +0x140 is set from the player's order until 진행 confirms it, so the old test switched the half mobility
    // (the same 2B2550 value the panel shows) off in exactly that window. It is no longer looked at.
    if(until<=0)return false;
    const unsigned player=player_force();
    if(!player || army_force(army)!=player)return false;
    return game_day()<until;
}

// First ZOC day (game threads, the army is active): once per leader and day the original ZOC score 2B7BC0(army,0) is
// read; above zero the end moves to today + 30 unless it is already earlier.
static void contact(Obj army) { COST_SCOPE("ambush:contact");
    static int checked[kAmbushPeople];
    const unsigned leader=word(army,0x12);const int today=game_day();
    if(!leader || leader>=kAmbushPeople || checked[leader]==today)return;
    checked[leader]=today;
    if(shared.contact[leader] || !GA(GA_2B7BC0))return;
    if(fn<int(__fastcall *)(Obj,Obj)>(GA(GA_2B7BC0))(army,nullptr)<=0)return;
    InterlockedExchange(&shared.contact[leader],1);   // from now on: full mobility, 60 days left at most
    const LONG until=shared.expiry[leader],want=today+kAmbushContactDays;
    if(until>want)InterlockedCompareExchange(&shared.expiry[leader],want,until);
#ifndef NO_RUNTIME_LOG
    log_event("ambush_contact",leader,static_cast<unsigned>(want),word(army,0x2A));
#endif
}
// Half mobility: only while the hidden army has not been under ZOC yet.
static bool slow(Obj army) {
    if(!active(army))return false;
    contact(army);
    return !shared.contact[word(army,0x12)];
}
// CALL C39F4 (condition dispatcher case 0x1D). The original answer always comes first.
static unsigned long long __fastcall cond(Obj a,Obj b,unsigned *args) { COST_SCOPE("ambush:cond");
    const unsigned long long r=fn<Cond>(GA(GA_AEB40))(a,b,args);
    if((r&0xFF) || !args || (args[1]!=11 && args[1]!=12) || !mod_features.ambush || !shared.count)return r;
    unsigned long long out=0;
    // local63 (user 2026-11-12: game closed on the first ambush sortie; access violation at 1137FF0 reading +0x20).
    // 1137FF0 is `return *(rcx+0x18)`: AEB40 passes its own first argument through in rcx. Calling it with no
    // argument read a stale rcx. The context is read from the same first argument here.
    if(!valid(a))return r;
    Obj context=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(a);
    if(!context || !fn<Resolve>(GA(GA_B28D20))(context,args[0],&out,0))return r;
    Obj unit=reinterpret_cast<Obj>(out);
    if(!valid(unit) || !active(unit))return r;
#ifndef NO_RUNTIME_LOG
    static int logged_day[kAmbushPeople];
    const unsigned leader=word(unit,0x12);
    if(logged_day[leader]!=game_day()) {   // once per leader and day: the AI was told "retreating"
        logged_day[leader]=game_day();
        log_event("ambush_hidden",leader,args[1],word(unit,0x2A));
    }
#endif
    return 1;
}

// local66 (user 2026-11-12 "매복기동 부대가 적군의 움직임에 영향을 안줘야해 ... 멈춰서있으면 버그"): condition 84
// AFDC0 ("an army hostile to force v3 within n hexes of the base", 25 uses in the corps table: transport sources,
// intercept candidates) has no retreat filter, so a hidden army still made nearby enemy bases count as threatened
// while nothing was sent against it. Inside its loop the army's force comes from the CALL AFFA3 -> 22E2A0 and the
// hostility answer from the CALL AFFB0 -> 2C7400; for a hidden army the answer is "not hostile".
static __declspec(thread) bool range_hidden;
static Obj __fastcall range_force(Obj army) { COST_SCOPE("ambush:range_force");
    range_hidden=valid(army) && active(army);
    return fn<Obj(__fastcall *)(Obj)>(GA(GA_22E2A0))(army);
}
static int __fastcall range_hostile(Obj own,Obj other) { COST_SCOPE("ambush:range_hostile");
    if(range_hidden) {
        range_hidden=false;
#ifndef NO_RUNTIME_LOG
        static int logged_day;if(logged_day!=game_day()){logged_day=game_day();log_event("ambush_range_hidden",0,0,0);}
#endif
        return 0;
    }
    return fn<int(__fastcall *)(Obj,Obj)>(GA(GA_2C7400))(own,other);
}
static CallLink range_links[2];
static bool range_ready;
static bool call_targets(unsigned site,unsigned target) { COST_SCOPE("ambush:call_targets");
    if(!image || !site || !target || image[site]!=0xE8)return false;
    int delta=0;memcpy(&delta,image+site+1,4);
    return image+site+5+delta==image+target;
}
static CallLink link;
static Obj relay;
static bool ready;
// 0 = prepared (or off), 1 = the CALL no longer targets AEB40, 2 = prepare failed.
static unsigned prepare() { COST_SCOPE("ambush:prepare");
    link=CallLink{};ready=false;
    if(!mod_features.ambush)return 0;
    if(!call_targets(GA(GA_C39F4),GA(GA_AEB40)))return 1;
    if(!relay)relay=allocate_near(image+GA(GA_C39F4),0x1000);
    if(!relay)return 2;
    DWORD old=0;
    if(!VirtualProtect(relay,4096,PAGE_EXECUTE_READWRITE,&old))return 2;
    absolute_jump(relay,reinterpret_cast<void *>(&cond));
    bool ok=call_prepare(link,GA(GA_C39F4),relay);
    range_links[0]=range_links[1]=CallLink{};range_ready=false;
    if(ok && call_targets(GA(GA_AFFA3),GA(GA_22E2A0)) && call_targets(GA(GA_AFFB0),GA(GA_2C7400))) {   // optional part
        absolute_jump(relay+64,reinterpret_cast<void *>(&range_force));absolute_jump(relay+128,reinterpret_cast<void *>(&range_hostile));
        range_ready=call_prepare(range_links[0],GA(GA_AFFA3),relay+64) && call_prepare(range_links[1],GA(GA_AFFB0),relay+128);
    }
    if(!VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,160))ok=false;
    if(!ok)return 2;
    ready=true;return 0;
}
static void restore() { COST_SCOPE("ambush:restore");
    call_restore(range_links[1]);call_restore(range_links[0]);range_ready=false;
    call_restore(link);ready=false;
}
static bool publish() { COST_SCOPE("ambush:publish");
    if(!mod_features.ambush)return true;   // an off switch is not a publish failure
    int delta=0;memcpy(&delta,image+link.site+1,4);
    const bool ok=ready && image[link.site]==0xE8 && delta==link.original && call_publish(link);
    if(!ok)restore();
    bool range=false;
    if(ok && range_ready) {
        range=call_publish(range_links[0]) && call_publish(range_links[1]);
        if(!range){call_restore(range_links[1]);call_restore(range_links[0]);range_ready=false;}
    }
#ifndef NO_RUNTIME_LOG
    log_event("ambush_installed",ok?1u:0u,range?1u:0u,0);
#endif
    return ok;
}
}   // namespace ambush_hook

#if defined(SPLIT_COMBAT) && !defined(SPLIT_MODULE_FIXTURE)
// The convenience module asks once at install; nullptr = wrong version or the switch is off.
// local135: the season rules' mobility change (season_mobility_shared.h); the convenience module hands it over at its season install.
extern "C" __declspec(dllexport) bool __cdecl SAN14CombatSeasonMobility(unsigned version,SeasonMobility source) {
    if(version!=kSeasonMobilityVersion)return false;
    InterlockedExchangePointer(reinterpret_cast<PVOID volatile *>(&season_mobility::source),reinterpret_cast<PVOID>(source));
    log_event("season_mobility_source",source?1u:0u,0,0);return true;
}
extern "C" __declspec(dllexport) AmbushTable *__cdecl SAN14CombatAmbush(unsigned version) { COST_SCOPE("SAN14CombatAmbush");
    return version==kAmbushVersion && mod_features.ambush && ambush_hook::ready?&ambush_hook::shared:nullptr;
}
#endif
