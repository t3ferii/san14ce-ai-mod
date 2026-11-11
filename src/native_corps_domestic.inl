// [personnel] local56 corps_domestic (user 2026-11-11), computer corps only: never the player's direct or delegated corps
// (2350F0 = corps of the player force), off = original.
//  ai_search2   "탐색교정을 삼갤 주딱으로 출처해주고 네가 제안한 방식대로": 출처 삼갤 주딱 SAN14_AISearch v1.1 (기능확장 v9).
//    The original corps tree runs its search subtree twice a turn (N838~N856: 1st pass officers with politics >= 60; 2nd pass
//    at the end of the corps turn, every waiting officer with an 81% roll, sent to own areas with nothing left to find).
//    The original mod rewrote the six condition constants by scanning all writable process memory (v1.0 about 4 GB, v1.1 about
//    1.9 GB) and, with the 5-minute limit patched out by another pack, rescanned every 5 s when the targets were not found.
//    Here the search command node (CCommandExecutionSearchNode 54AD0, already wrapped by personnel_search_command) refuses the
//    command while the phase variable 0xED5CAFE8 (보조값0287) is >= 1 for a computer corps. No memory scan, no thread.
//  officer_first "컴퓨터군단의 모병과 훈련 내정 우선순위를 높였으면": the recruit and training officer bundles (N2518 / N2570,
//    per city after 담당관후보선정 N2507) are gated by "player corps OR progress check(corps, 30)" (condition 116, A9960). When the
//    player holds under 30% of the cities that check passes a computer corps only on turns where
//    (corps index + world+0x454 + world+0x45C) % 3 <= min(level, 2), i.e. one turn in three at the lowest level. For a computer
//    corps the next two condition-116 checks after N2507 (exactly those two gates; the decorator links its subtrees inline) pass,
//    so the two officer bundles run every turn. Gold, troop and allowance rules of the bundles stay original.
#include "game_addresses.h"
namespace corps_domestic {
static constexpr unsigned kPhaseVar=0xED5CAFE8u,kCorpsVar=0xD6A5EAAEu;
static Obj corps_of(Obj context,unsigned var) {
    Obj blackboard=context?fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context):nullptr,corps=nullptr;
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(blackboard,var,&corps,0) || !valid(corps))return nullptr;
    return corps;
}
// Demo play (game +0x3A == 0) hands the player's corps to the computer too, although 2350F0 still names them.
static bool demo_play() {Obj root=world();Obj ctx=root?pointer(root,0x871E8):nullptr;return ctx && ctx[0x3A]==0;}
static bool computer(Obj corps) {return corps && (demo_play() || fn<int(__fastcall *)(Obj)>(GA(GA_2350F0))(corps)==0);}
static bool skip_second_search(Obj context) { COST_SCOPE("corps_domestic:skip_second_search");
    if(!mod_features.ai_search2 || !context)return false;
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);int phase=0;
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,int *,int)>(GA(GA_B28BB0))(blackboard,kPhaseVar,&phase,0) || phase<1)return false;
    Obj corps=corps_of(context,kCorpsVar);
    if(!computer(corps))return false;
#ifndef NO_RUNTIME_LOG
    static volatile LONG lines;
    if(InterlockedIncrement(&lines)<=300)log_event("ai_search2_skipped",word(corps,0x10),static_cast<unsigned>(phase),0);
#endif
    return true;
}
// officer_first: armed by 담당관후보선정 for the corps under review; the next two condition-116 checks of that corps pass.
struct Armed {Obj world,corps;unsigned left;};
static thread_local Armed armed;
// Only a successful selection arms: a failed one ends the sequence before either gate is evaluated.
static void arm(Obj context,bool selected) {
    if(!mod_features.officer_first)return;
    Obj corps=selected?corps_of(context,kCorpsVar):nullptr;
    armed=computer(corps)?Armed{world(),corps,2}:Armed{};
}
// The dispatcher C36A0 passes four registers (A9960 reads rcx for the blackboard and r8 = {corps var, n}); all are forwarded.
using Progress=uint64_t(__fastcall *)(Obj,Obj,int *,uint64_t);
static uint64_t __fastcall progress_check(Obj a,Obj b,int *args,uint64_t d) { COST_SCOPE("corps_domestic:progress_check");
    uint64_t result=fn<Progress>(GA(GA_A9960))(a,b,args,d);
    if(!armed.left)return result;
    if(armed.world!=world() || !args || args[1]!=30) {armed=Armed{};return result;}
    Obj corps=corps_of(a,static_cast<unsigned>(args[0]));
    if(corps!=armed.corps) {armed=Armed{};return result;}
    --armed.left;
#ifndef NO_RUNTIME_LOG
    {static volatile LONG seen;if(InterlockedIncrement(&seen)<=300)log_event("officer_first_check",word(corps,0x10),armed.left,static_cast<unsigned>(result&0xFF));}
#endif
    if(result&0xFF)return result;
#ifndef NO_RUNTIME_LOG
    static volatile LONG lines;
    if(InterlockedIncrement(&lines)<=300)log_event("officer_first_pass",word(corps,0x10),armed.left,0);
#endif
    return 1;
}
static CallLink call;
static Obj relay;
static bool ready,search_slot,candidate_slot;
static bool wanted() {return mod_features.ai_search2 || mod_features.officer_first;}
// 0 = prepared (or off), 1 = the CALL no longer targets condition 116, 2 = prepare failed.
static unsigned prepare() { COST_SCOPE("corps_domestic:prepare");
    call=CallLink{};ready=false;
    if(!mod_features.officer_first)return 0;
    const unsigned site=GA(GA_C4383),target=GA(GA_A9960);
    int delta=0;
    if(!image || !site || !target || image[site]!=0xE8)return 1;
    memcpy(&delta,image+site+1,4);
    if(image+site+5+delta!=image+target)return 1;
    if(!relay)relay=allocate_near(image+site,0x1000);
    if(!relay)return 2;
    DWORD old=0;
    if(!VirtualProtect(relay,4096,PAGE_EXECUTE_READWRITE,&old))return 2;
    absolute_jump(relay,reinterpret_cast<void *>(&progress_check));
    bool ok=call_prepare(call,site,relay);
    if(!VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,32))ok=false;
    if(!ok)return 2;
    ready=true;
    return 0;
}
static void restore() { COST_SCOPE("corps_domestic:restore");
    call_restore(call);ready=false;
    if(search_slot)restore_pointer(GA(GA_134C580),GA(GA_54AD0),reinterpret_cast<void *>(&personnel_search_command));
    if(candidate_slot)restore_pointer(GA(GA_1355B18),GA(GA_7F4F0),reinterpret_cast<void *>(&personnel_officer_candidates));
    search_slot=candidate_slot=false;
}
// With injury protection on, its links already wrap both nodes with the same functions; without it, only the needed slot.
static bool publish() { COST_SCOPE("corps_domestic:publish");
    if(!wanted())return true;
    bool ok=true;
    if(mod_features.officer_first) {
        int delta=0;
        memcpy(&delta,image+call.site+1,4);
        ok=ready && image[call.site]==0xE8 && delta==call.original && call_publish(call);
    }
    const bool wrapped=mod_features.injury;
    if(ok && mod_features.ai_search2 && !wrapped)
        ok=search_slot=patch_pointer(GA(GA_134C580),GA(GA_54AD0),reinterpret_cast<void *>(&personnel_search_command));
    if(ok && mod_features.officer_first && !wrapped)
        ok=candidate_slot=patch_pointer(GA(GA_1355B18),GA(GA_7F4F0),reinterpret_cast<void *>(&personnel_officer_candidates));
    if(!ok)restore();
#ifndef NO_RUNTIME_LOG
    log_event("corps_domestic_installed",ok?1u:0u,mod_features.ai_search2,mod_features.officer_first);
#endif
    return ok;
}
}
