#pragma once
// Combat AI: behaviour-tree node wrappers (apply_priority, bt_condition, cut/city/retreat nodes), moved verbatim out of native_adapter.cpp (W6-D).
// army_watch (user 2026-11-10 "장료·전풍이 남피↔북평 왕복", diagnostic build only): armies whose commander is listed in
// [diagnostics] watch_officers=663,713 log their mission state once per day and around each node wrapper whenever the
// call changed it (who changed it: the mod branch or the original). Read only; nothing is written to the game.
#ifndef NO_RUNTIME_LOG
namespace army_watch {
static unsigned officers[16],officer_count;static bool loaded;
static struct {Obj root;int day;unsigned long long state;} last[501];
static void load() {
    if(loaded)return;loaded=true;
    wchar_t path[MAX_PATH],text[256]={};
    if(!mod_ini_path(path,L"기능설정.ini"))return;
    GetPrivateProfileStringW(L"diagnostics",L"watch_officers",L"",text,256,path);
    for(wchar_t *s=text;*s && officer_count<16;) {
        while(*s && (*s<L'0' || *s>L'9'))++s;
        unsigned v=0;bool any=false;while(*s>=L'0' && *s<=L'9') {v=v*10+static_cast<unsigned>(*s-L'0');++s;any=true;}
        if(any && v)officers[officer_count++]=v;
    }
    log_event("watch_officers_loaded",officer_count,officer_count?officers[0]:0,officer_count>1?officers[1]:0);
}
static bool wanted(Obj army) {
    load();if(!valid(army))return false;
    if(AI_TRACE_TRACKED(army))return true;   // ai_trace: build departures are watched too
    if(!officer_count)return false;
    const unsigned c=word(army,0x12);
    for(unsigned i=0;i<officer_count;++i)if(officers[i]==c)return true;
    return false;
}
static unsigned long long state(Obj army) {
    Obj troops=table(0x7F8D8,word(army,0x58),500);
    const unsigned goal=valid(troops)?word(troops,0x16):0,mode=valid(troops)?word(troops,0x14):0;
    return (static_cast<unsigned long long>(army[0x37])<<56)|(static_cast<unsigned long long>(army[0x38])<<48)|
           (static_cast<unsigned long long>(word(army,0x3A))<<32)|(static_cast<unsigned long long>(word(army,0x2C))<<16)|
           (static_cast<unsigned long long>(goal&0xFF)<<8)|(mode&0xFF);
}
static void emit(const char *kind,Obj army,unsigned long long s) {
    log_event(kind,army_id(army),static_cast<unsigned>(s>>32),static_cast<unsigned>(s));
}
// Node entry: one full line per army per day (commander, troops, hex, mod progress), then the mission state.
static unsigned long long before(Obj army,Progress *p) {
    if(!wanted(army))return 0;
    const unsigned id=army_id(army);if(!id || id>500)return 0;
    const unsigned long long s=state(army);const int day=game_day();
    if(last[id].root!=world() || last[id].day!=day) {
        last[id].root=world();last[id].day=day;
        log_event("watch_army",id,word(army,0x12),(static_cast<unsigned>(word(army,0x16))<<16)|word(army,0x2A));
        if(p)log_event("watch_progress",id,p->city,(p->campaign?1u:0)|(p->forced_return?2u:0)|(p->returning?4u:0)|
            (p->regional_worker?8u:0)|(p->operation_leader?16u:0)|(p->combat_target?32u:0)|(p->combat_city?64u:0)|
            (p->home_defense_city?128u:0)|(p->facility?256u:0)|(p->credit?512u:0));
        emit("watch_state",army,s);last[id].state=s;
    } else if(s!=last[id].state) {emit("watch_state_between",army,s);last[id].state=s;}
    return s|1ull<<63|1;   // non-zero marker even for an all-zero state
}
// Node exit: logs only when this call changed the mission; kind names the node and who decided.
static void after(Obj army,unsigned long long marker,const char *kind) {
    if(!marker || !valid(army))return;
    const unsigned id=army_id(army);if(!id || id>500)return;
    const unsigned long long s=state(army);
    if(s!=last[id].state) {emit(kind,army,s);last[id].state=s;}
}
// Troops node (local29f): the work list +0x18 u16[10] and every watched member's five waypoints at
// each step of city_node (pre = before mission_v2, mod = after mission_v2, native = after 69AB0,
// end = after the mod's member pass), so the line where a capture region appears or swaps is visible.
static void troops(Obj troops,const char *tag) {
    load();if(!officer_count || !valid(troops))return;
    static volatile LONG lines;if(lines>8000)return;
    Obj members[16];unsigned count=0,tid=0;
    for(unsigned i=1;i<=500 && count<16;++i) {
        Obj a=table(0x7E838,i,500);
        if(!valid(a) || a[0x140] || table(0x7F8D8,word(a,0x58),500)!=troops || !wanted(a))continue;
        members[count++]=a;tid=word(a,0x58);
    }
    if(!count)return;
    const unsigned short *l=reinterpret_cast<const unsigned short *>(troops+0x18);
    InterlockedAdd(&lines,static_cast<LONG>(4+2*count));
    log_event(tag,tid,(static_cast<unsigned>(word(troops,0x14))<<16)|word(troops,0x16),count);
    log_event("watch_list_a",tid,(static_cast<unsigned>(l[0])<<16)|l[1],(static_cast<unsigned>(l[2])<<16)|l[3]);
    log_event("watch_list_b",tid,(static_cast<unsigned>(l[4])<<16)|l[5],(static_cast<unsigned>(l[6])<<16)|l[7]);
    log_event("watch_list_c",tid,(static_cast<unsigned>(l[8])<<16)|l[9],0);
    for(unsigned m=0;m<count;++m) {
        Obj a=members[m];
        log_event("watch_member_a",army_id(a),(static_cast<unsigned>(word(a,0x2C))<<16)|word(a,0x2E),
            (static_cast<unsigned>(word(a,0x30))<<16)|word(a,0x32));
        log_event("watch_member_b",army_id(a),(static_cast<unsigned>(word(a,0x34))<<16)|word(a,0x2A),
            (static_cast<unsigned>(a[0x37])<<24)|(static_cast<unsigned>(a[0x38])<<16)|word(a,0x3A));
    }
}
}
#define WATCH_BEFORE(army,p) const unsigned long long watch_marker=army_watch::before((army),(p))
#define WATCH_AFTER(army,kind) army_watch::after((army),watch_marker,(kind))
#define WATCH_TROOPS(t,tag) army_watch::troops((t),(tag))
#else
#define WATCH_BEFORE(army,p) ((void)0)
#define WATCH_AFTER(army,kind) ((void)0)
#define WATCH_TROOPS(t,tag) ((void)0)
#endif
static bool apply_priority(Obj army, Obj node, Obj context)
{
    COST_SCOPE("apply_priority");
    if(native_search_depth)return false;
    if (!campaign_controlled(army)) return false;
    Progress *p = observe_capture(army);
    if(p) {
        remember_campaign(army,*p);
        if(campaign_individual_priority(army,*p)) {
#ifdef SORTIE_FORMATION
            SortieGroup g;unsigned id;
            if((p->forced_return || p->combat_target || p->combat_city || p->home_defense_city || p->guard_beneficiary) && sortie_snapshot(army,g,id)) {
                g.active=false;sortie_commit(id,g);
            }
            if(p->forced_return || p->combat_target || p->combat_city) {
                AcquireSRWLockExclusive(&sortie_state_lock);sortie_escorts[army_id(army)]={};ReleaseSRWLockExclusive(&sortie_state_lock);
            }
#endif
            mod_step("ap:individual");return true;
        }
        // local45: a cut the original planned today wins over the group march and siege priorities below.
        if(mod_features.combat_cut && computer_side_army(army) && restore_cut(army,*p))return mod_step("ap:restore_cut"),true;
#ifdef SORTIE_FORMATION
        // mission_v2: route regions go into the troops work list instead (mission_route_waypoint);
        // a direct order here was reset by the original 613A0 every turn (232 of 307 in run 3).
        if(!mod_features.mission_v2 && campaign_near_capture(army,*p))return mod_step("ap:near_capture"),true;
        if(sortie_support_step(army) || sortie_step(army))return mod_step("ap:group_step"),true;
#endif
        if(!p->regional_worker && (p->operation_leader || (!p->credit && !p->build_chain && !p->facility)) && campaign_siege_priority(army,*p))return mod_step("ap:siege_priority"),true;
        if(p->operation_leader)return mod_step("ap:operation_hold"),true;
    }
#ifdef SORTIE_FORMATION
    if(sortie_support_step(army) || sortie_step(army))return mod_step("ap:group_step"),true;
#endif
    if(p && p->forced_return)return false;
    if (p) {remember_campaign(army, *p);observe_obstacle_motion(army,*p);}
    // U39: the cut is planned only by the original cut node (cut_node below); no second cut writer here.
    if(p && restore_cut(army,*p))return mod_step("ap:restore_cut"),true;
    bool cancelled = p && cancel_wall_order(army,*p);
    if(p && cancel_catapult_order(army,*p))cancelled=true;
    if(p && cancel_unqualified_neutral_order(army,*p))cancelled=true;
    if(p && !p->regional_worker && recover_idle(army,*p))return mod_step("ap:recover_idle"),true;
    if(word(army,0x16)<1000)return cancelled && mod_step("ap:cancel_order");
    if(p && try_destroy(army,*p,true)) {pause_assembly(*p);return mod_step("ap:destroy_priority"),true;}
    if(p && p->facility && continue_build(army,*p)) {pause_assembly(*p);return mod_step("ap:continue_build"),true;}
    if(p && try_destroy(army,*p)) {pause_assembly(*p);return mod_step("ap:destroy"),true;}
    if(p && (p->campaign || p->regional_worker))prepare_local_build(army,*p);
    if (p && try_build(army, *p)) {pause_assembly(*p);return mod_step("ap:build"),true;}
    // Build policy v2: a blocked build never parks the army (former "build_hold_wait" stood units
    // on open ground for weeks); the mission or the original AI continues instead.
    if(mod_features.mission_v2) {
        if(p && (p->credit || p->build_chain) && !p->facility) {p->credit=false;p->build_chain=false;}
    } else if(p && (p->credit || p->build_chain)) {
        // Former policy: keep the native AI from replacing a temporarily blocked build with retreat.
        if(army[0x37]!=13)log_event("build_hold_wait",army_id(army),army[0x37],word(army,0x2A));
        fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
        fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,13);
        fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,255,0);
        mod_step("ap:build_hold_wait");return true;
    }
    if(p && p->regional_worker) {if(campaign_regional_step(army,*p))return mod_step("ap:regional_step"),true;return cancelled && mod_step("ap:cancel_order");}
    if (p && p->campaign && mod_features.mission_v2 && native_on_mission(army)) return mod_step("ap:native_on_mission"),false;
    if (p && p->campaign && campaign_step(army, *p, node, context)) return mod_step("ap:campaign_step"),true;
    if(try_other(army, node, context))return mod_step("ap:native_alternative"),true;
    return cancelled && mod_step("ap:cancel_order");
}

// [combat] ai_max_aggression. The behaviour-tree condition dispatcher is one function pointer at
// image+1B95CC0 (registry 1B95CB0 +10), set by AI init C0150 to C2610 (switch on the condition op).
// Op 0x73 (AI setting stage == arg, 3E9E60 = game AI setting 0..4) answers as stage 4 (궁극).
// Op 0x74 (corps progress check, A8A00) skips turns when stage < 2 and the force holds under the
// threshold share of bases; with stage 4 it only fails for an invalid corps, so a failure for a valid
// corps is turned into success. All other ops go to the original unchanged.
static bool patch_pointer(unsigned rva, unsigned expected, void *replacement);
using BtCondition=uint64_t(__fastcall *)(unsigned,unsigned char,Obj,Obj,int *,int);
static uint64_t __fastcall bt_condition(unsigned op,unsigned char negate,Obj context,Obj entity,int *args,int count)
{ COST_SCOPE("bt_condition");
    if(mod_features.ai_max_aggression && op==0x73 && args)return args[0]==4?1u:0u;
    auto original=fn<BtCondition>(GA(GA_C36A0));
    uint64_t result=original(op,negate,context,entity,args,count);
    if(!mod_features.ai_max_aggression)return result;
    if(op==0x74 && !(result&0xFF) && args && context) {
        Obj corps=nullptr;Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);
        if(blackboard && fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(blackboard,static_cast<unsigned>(args[0]),&corps,0) &&
           valid(corps))result=(result&~0xFFull)|1u;
    }
    return result;
}
// [combat] ai_max_aggression: CVariableGetPersonInfoNode (8E7C0, slot 13512A0) reads a person field
// into a blackboard variable (node+38 person var, +3C item, +40 output var; B26990 stores it).
// Items 13 (호전 +16A: corps leader's target-strength discount), 32 (야망 +17C: ruler's force policy)
// read as 20 (UI 5) for computer forces. Item 34 (+17E) is 계략 (stratagem preference with 외교, N2885),
// so it is left unchanged.
static uint64_t __fastcall person_info_node(Obj node,Obj context,Obj entity)
{ COST_SCOPE("person_info_node");
    uint64_t result=fn<uint64_t(__fastcall *)(Obj,Obj,Obj)>(GA(GA_8F810))(node,context,entity);
    if(!mod_features.ai_max_aggression || result!=1 || !node || !context)return result;
    unsigned item=dword(node,0x3C);
    if(item!=0x0D && item!=0x20)return result;   // 34 (+17E) is 계략, not aggression (REA 합본 2장, 2026-10-22)
    Obj person=nullptr;Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(blackboard,dword(node,0x38),&person,0) ||
       !valid(person))return result;
    Obj district=table(0xE0D0,person[0xE0],51);
    if(!valid(district) || fn<int(__fastcall *)(Obj)>(GA(GA_2350F0))(district))return result; // no corps / player force
    fn<char(__fastcall *)(Obj,unsigned,int)>(GA(GA_B29250))(blackboard,dword(node,0x40),20);
    return result;
}
// attack_recheck_fix (mission_v2). Tree 1's post-departure recheck of a foothold-attack group (#10050..#10125,
// REA 합본 16장) sums the friendly power aimed at the target into variable 0x47EAF02B, then inside the
// corps-base loop #10114 CGetFootholdFightingStrengthNode (7DC00: out var +3C, radius +40) assigns one
// base's 5-hex power into the SAME variable and #10115 CVariableMinusVariableNode (91030: A=+38 -= B=+3C)
// subtracts it from the enemy power 0xB57EAF65. The final "enemy x70% > friendly" check then compares
// against one base instead of the whole attack, sending fresh attacks home. The wrappers keep the
// original order: the base power still reduces the enemy, and the friendly sum is restored.
static constexpr unsigned kRecheckFriendly=0x47EAF02Bu,kRecheckEnemy=0xB57EAF65u;
static thread_local bool recheck_pending;
static thread_local int recheck_base_power;
// attack_eval70 / retreat_eval60 (user 2026-11-10, computer corps only). The attack review (tree cca2ce60 N1990) and the
// field retreat recheck (N10537) both run CGetFootholdFightingStrengthNode (7EBA0) with input 새공격목표 0xAD8610FE, output
// 목표요구전투력 0xB57EAF65 and radius 10, so the parameters cannot tell them apart. N2076 (CVariablePlusVariableNode 922F0:
// 공격검토기준 0x19FCD973 += 목표기초보정액 0x2E969939) runs only in the attack review, after N1990 in the same corps run: the
// radius-10 node object seen last before N2076 is N1990, every other radius-10 node is N10537 (pw36c_bt_probe.py). The node
// objects belong to the loaded tree, so the learnt pointer stays valid until the world changes. Until the first N2076 of a
// world, nothing is scaled. The original value is computed first and only the output variable is rescaled afterwards.
// The interception review is a different node and keeps the original value.
static constexpr unsigned kTargetCityVar=0xAD8610FEu,kCorpsVar=0xD6A5EAAEu,kAttackThreshold=0x19FCD973u,kBaseCorrection=0x2E969939u;
static Obj g_attack_foothold;            // N1990 node object (attack review)
static Obj g_foothold_world;             // world the pointer was learnt in
static thread_local Obj g_foothold_last; // radius-10 target node this thread ran last
static SRWLOCK g_foothold_lock=SRWLOCK_INIT;
static bool attack_eval_wanted() {return mod_features.attack_eval70 || mod_features.retreat_eval60;}
// 1 = attack review (N1990), 2 = retreat recheck (N10537), 0 = not known yet.
static unsigned foothold10_kind(Obj node) { COST_SCOPE("ai_nodes:foothold10_kind");
    Obj root=world();
    AcquireSRWLockExclusive(&g_foothold_lock);
    if(g_foothold_world!=root) {g_attack_foothold=nullptr;g_foothold_world=root;}
    unsigned kind=!g_attack_foothold?0u:node==g_attack_foothold?1u:2u;
    ReleaseSRWLockExclusive(&g_foothold_lock);
    return kind;
}
// The corps under review belongs to a computer force (never the player's direct or delegated corps).
static bool foothold_computer_corps(Obj context) { COST_SCOPE("ai_nodes:foothold_computer_corps");
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context),corps=nullptr;
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(blackboard,kCorpsVar,&corps,0) || !valid(corps))
        return false;
    return fn<int(__fastcall *)(Obj)>(GA(GA_2350F0))(corps)==0;   // 2350F0: corps of the player force
}
static void foothold_scale(Obj context,unsigned percent,const char *what) { COST_SCOPE("ai_nodes:foothold_scale");
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);int value=0;
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,int *,int)>(GA(GA_B28BB0))(blackboard,kRecheckEnemy,&value,0) || value<=0)return;
    int scaled=static_cast<int>(static_cast<long long>(value)*percent/100);
    if(scaled<1)scaled=1;
    if(scaled>=value)return;
    fn<char(__fastcall *)(Obj,unsigned,int)>(GA(GA_B29250))(blackboard,kRecheckEnemy,scaled);
#ifndef NO_RUNTIME_LOG
    static volatile LONG lines;
    if(InterlockedIncrement(&lines)<=300)log_event(what,static_cast<unsigned>(value),static_cast<unsigned>(scaled),percent);
#else
    (void)what;
#endif
}
// no_tribe_target (local47, user 2026-11-11 "이민족 성은 어떤 경우에도 목표로 못 삼게"): the original never lets a
// non-player force take a tribe castle (285DB0, rules_combat C12), yet a low enemy estimate makes computer corps attack
// one. When the target of the attack review / retreat recheck (radius-10 strength node, input 새공격목표) is a castle of a
// tribe force (46..50), the required strength is raised to a value no corps reaches: the review drops the target and a
// corps already on its way retreats through the original recheck. Computer corps only; off = original value.
static constexpr int kTribeStrength=0x3FFFFFFF;
static bool tribe_city(Obj city) { COST_SCOPE("ai_nodes:tribe_city");
    if(!valid(city))return false;
    Obj force=fn<Obj(__fastcall *)(Obj)>(GA(GA_22E3E0))(city);
    if(!valid(force))return false;
    const unsigned id=(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(force,0)+0x60))(force);
    return id>=46 && id<=50;
}
static bool tribe_target_block(Obj context) { COST_SCOPE("ai_nodes:tribe_target_block");
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context),city=nullptr;
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(blackboard,kTargetCityVar,&city,0) || !tribe_city(city))
        return false;
    fn<char(__fastcall *)(Obj,unsigned,int)>(GA(GA_B29250))(blackboard,kRecheckEnemy,kTribeStrength);
#ifndef NO_RUNTIME_LOG
    static volatile LONG lines;
    if(InterlockedIncrement(&lines)<=300)log_event("tribe_target_blocked",word(city,0x4E),0,0);
#endif
    return true;
}
static uint64_t __fastcall foothold_strength_node(Obj node,Obj context,Obj entity)
{ COST_SCOPE("foothold_strength_node");
    auto original=fn<uint64_t(__fastcall *)(Obj,Obj,Obj)>(GA(GA_7EBA0));
    if((attack_eval_wanted() || mod_features.no_tribe_target) && node && context && dword(node,0x38)==kTargetCityVar &&
       dword(node,0x3C)==kRecheckEnemy && dword(node,0x40)==10) {
        g_foothold_last=node;
        unsigned kind=foothold10_kind(node);
        uint64_t result=original(node,context,entity);
        if(mod_features.no_tribe_target && foothold_computer_corps(context) && tribe_target_block(context))return result;
        if(kind==1 && mod_features.attack_eval70 && foothold_computer_corps(context))foothold_scale(context,70,"attack_eval70");
        if(kind==2 && mod_features.retreat_eval60 && foothold_computer_corps(context))foothold_scale(context,60,"retreat_eval60");
        return result;
    }
    if(!mod_features.attack_recheck_fix || !node || !context || dword(node,0x3C)!=kRecheckFriendly || dword(node,0x40)!=5)
        return original(node,context,entity);
    recheck_pending=false;
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);int sum=0;
    auto read=fn<char(__fastcall *)(Obj,unsigned,int *,int)>(GA(GA_B28BB0));
    bool had=blackboard && read(blackboard,kRecheckFriendly,&sum,0);
    uint64_t result=original(node,context,entity);
    int base=0;
    if(had && read(blackboard,kRecheckFriendly,&base,0)) {
        fn<char(__fastcall *)(Obj,unsigned,int)>(GA(GA_B29250))(blackboard,kRecheckFriendly,sum);
        recheck_base_power=base;recheck_pending=true;
    }
    return result;
}
// N2076 (attack_eval70 / retreat_eval60 only; the row is published only when one of them is on): learn which radius-10
// target node is the attack review. No blackboard write.
static uint64_t __fastcall attack_threshold_node(Obj node,Obj context,Obj entity)
{ COST_SCOPE("attack_threshold_node");
    uint64_t result=fn<uint64_t(__fastcall *)(Obj,Obj,Obj)>(GA(GA_922F0))(node,context,entity);
    if(!attack_eval_wanted() || !node || dword(node,0x38)!=kAttackThreshold || dword(node,0x3C)!=kBaseCorrection)return result;
    Obj last=g_foothold_last;g_foothold_last=nullptr;
    if(!last)return result;
    Obj root=world();
    AcquireSRWLockExclusive(&g_foothold_lock);
    bool learnt=g_attack_foothold!=last;
    g_attack_foothold=last;g_foothold_world=root;
    ReleaseSRWLockExclusive(&g_foothold_lock);
#ifndef NO_RUNTIME_LOG
    if(learnt)log_event("attack_review_node_learnt",0,0,0);
#else
    (void)learnt;
#endif
    return result;
}
static uint64_t __fastcall variable_minus_node(Obj node,Obj context,Obj entity)
{ COST_SCOPE("variable_minus_node");
    auto original=fn<uint64_t(__fastcall *)(Obj,Obj,Obj)>(GA(GA_92080));
    if(!recheck_pending || !node || !context || dword(node,0x38)!=kRecheckEnemy || dword(node,0x3C)!=kRecheckFriendly)
        return original(node,context,entity);
    recheck_pending=false;
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);int sum=0;
    if(!blackboard || !fn<char(__fastcall *)(Obj,unsigned,int *,int)>(GA(GA_B28BB0))(blackboard,kRecheckFriendly,&sum,0))
        return original(node,context,entity);
    auto write=fn<char(__fastcall *)(Obj,unsigned,int)>(GA(GA_B29250));
    write(blackboard,kRecheckFriendly,recheck_base_power);   // enemy -= this base's power (original intent)
    uint64_t result=original(node,context,entity);
    write(blackboard,kRecheckFriendly,sum);                  // the friendly attack sum survives the loop
    log_event("attack_recheck_sum_kept",0,static_cast<unsigned>(sum),static_cast<unsigned>(recheck_base_power));
    return result;
}
static volatile LONG bt_condition_state; // 0 pending, 1 installed, 2 unexpected pointer
static void ensure_bt_condition()
{ COST_SCOPE("ai_nodes:ensure_bt_condition");
    if(!mod_features.ai_max_aggression || bt_condition_state)return;
    void *current=*reinterpret_cast<void **>(image+GA(GA_1B98D10));
    if(!current)return; // AI not initialised yet
    if(patch_pointer(GA(GA_1B98D10),GA(GA_C36A0),reinterpret_cast<void *>(&bt_condition))) {
        InterlockedExchange(&bt_condition_state,1);log_event("ai_max_aggression_installed",0,0,0);
    } else if(InterlockedCompareExchange(&bt_condition_state,2,0)==0)
        log_event("ai_max_aggression_unexpected_pointer",0,static_cast<unsigned>(reinterpret_cast<Obj>(current)-image),0);
}
static uint64_t __fastcall first_army_node(Obj node, Obj context, Obj entity)
{
    COST_SCOPE("first_army_node");
    NativePhase phase("army_priority");
    ensure_bt_condition();
    auto original = fn<Execute>(GA(GA_201C0));
    if (!enabled || in_policy || native_search_depth) return original(node, context, entity);
    Obj army = nullptr;
    Obj blackboard = fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);
    fn<void(__fastcall *)(Obj, unsigned, Obj *, int)>(GA(GA_B28D20))(blackboard, dword(node, 0x38), &army, 0);
    ++in_policy;
    AcquireSRWLockExclusive(&policy_lock);
    Progress *p=campaign_controlled(army)?observe_capture(army):nullptr;
    WATCH_BEFORE(army,p);
#ifdef SORTIE_FORMATION
    if(p)campaign_proactive_departures(army);
    if(p)campaign_saturated_departures(army);
    campaign_frontier_diagnostic();
#endif
    bool changed=p && campaign_individual_priority(army,*p);
    bool urgent=p && (p->forced_return || p->combat_target || p->combat_city || p->home_defense_city || campaign_castle_locked(army));
    if(!changed && !urgent && p && !p->credit && !p->build_chain && !p->facility && !campaign_group_member(army) && !p->regional_worker && node && context && fn<Execute>(GA(GA_20040))(node,context,army)==1) {
        log_event("native_supply_restore",army_id(army),0,word(army,0x2A));changed=true;
    }
    if(!changed)changed=apply_priority(army,node,context);
    ReleaseSRWLockExclusive(&policy_lock);
    --in_policy;
    mod_step(changed?"army_node:mod":"army_node:native");
    WATCH_AFTER(army,changed?"watch_army_node_mod":"watch_army_node_pre");
    const uint64_t result=changed ? 1 : original(node, context, entity);
    if(!changed)WATCH_AFTER(army,"watch_army_node_native");
    return result;
}
static uint64_t __fastcall cut_node(Obj node, Obj context, Obj army)
{
    COST_SCOPE("cut_node");
    NativePhase phase("army_cut");
    if (!enabled || in_policy || native_search_depth) return fn<Execute>(GA(GA_1F380))(node, context, army);
    Obj supplied = army;
    army = fn<Obj(__fastcall *)(Obj, Obj)>(GA(GA_B7100))(context, supplied);
    if (!campaign_controlled(army)) return fn<Execute>(GA(GA_1F380))(node, context, supplied);
    ++in_policy;
    AcquireSRWLockExclusive(&policy_lock);
    Progress *p=observe_capture(army);
    WATCH_BEFORE(army,p);
    bool changed=p && campaign_individual_priority(army,*p);
    bool urgent=p && (p->forced_return || p->combat_target || p->combat_city || p->home_defense_city || campaign_castle_locked(army));
    // local45 (user 2026-11-10 "모든 컴퓨터 군세가 기본적으로 병참단절을 노려야"): the original cut node used to run only
    // after every mod priority (group march, siege priority, operation hold, sortie member) had passed, so a computer
    // group member almost never reached it (no cut_player in the 11-10 logs). It now runs first for a computer-side
    // army that has no urgent duty; the original decides as before (통솔 or 지력 > 64, troops mode 8 excluded, D1760
    // route) and the mod still narrows the victims to player armies. No cut found: the mod priorities follow.
    bool cut_tried=false;
    if(!changed && !urgent && mod_features.combat_cut && computer_side_army(army)) {
        cut_tried=true;
        ++in_cut;
        changed=fn<Execute>(GA(GA_1F380))(node,context,supplied)==1;
        --in_cut;
        if(changed)log_event("cut_player",army_id(army),dword(army,0x1F4),word(army,0x2A));
    }
    if(!urgent && p && !(cut_tried && changed) && !campaign_group_member(army) && !p->regional_worker && node && context && fn<Execute>(GA(GA_20040))(node,context,army)==1)changed=true;
    if(!changed && p && !campaign_group_member(army) && !p->regional_worker)changed=campaign_siege_priority(army,*p);
    if(!changed && p && p->operation_leader)changed=true;
    if(!changed && p && p->regional_worker) {apply_priority(army,node,context);changed=true;}
#ifdef SORTIE_FORMATION
    if(!changed && (sortie_member(army) || sortie_escort_member(army)))changed=true;
#endif
    // U39 combat_cut (docs/COMBAT_REDESIGN.md): the original cut node plans and orders (its 64 ability gate, mode 8
    // exclusion, D1760 route, 26E260). For a computer-side army the mod only narrows D1760's victim list to player
    // armies (C1D10 wrapper at D3516, active while in_cut). Player direct armies, delegated corps and combat_cut=0
    // run the node unchanged = original.
    if(!changed && !cut_tried && !(p && p->forced_return)) {
        const bool filter=mod_features.combat_cut && computer_side_army(army) && !campaign_castle_locked(army);
        if(filter)++in_cut;
        changed=fn<Execute>(GA(GA_1F380))(node,context,supplied)==1;
        if(filter)--in_cut;
        if(changed && filter)log_event("cut_player",army_id(army),dword(army,0x1F4),word(army,0x2A));
    }
    if(changed && p && (dword(army,0x1F0)&8))remember_cut(army,*p);
    ReleaseSRWLockExclusive(&policy_lock);
    --in_policy;
    mod_step(changed?"cut_node:mod":"cut_node:native_skip");
    WATCH_AFTER(army,changed?"watch_cut_node_mod":"watch_cut_node_skip");
    return changed ? 1 : 2;
}
static uint64_t __fastcall city_node(Obj node, Obj context, Obj troops)
{
    COST_SCOPE("city_node");
    NativePhase phase("troops_city");
    auto original = fn<Execute>(GA(GA_69AB0));
    if (!enabled || in_policy || native_search_depth || !valid(troops)) return original(node, context, troops);
    ++in_policy;
    AcquireSRWLockExclusive(&policy_lock);
    /* Record any capture/build state before the original clears waypoint lists. */
    for (unsigned i = 1; i <= 500; ++i) {
        Obj army = table(0x7E838, i, 500);
        if (campaign_controlled(army) && belongs_to_troops(army, troops)) {
            Progress *p=observe_capture(army);
            if(p) remember_campaign(army,*p);
        }
    }
    unsigned short before_mode=word(troops,0x14),before_goal=word(troops,0x16);
    WATCH_TROOPS(troops,"watch_troops_pre");
    if(mod_features.mission_v2)mission_route_waypoint(troops);
    WATCH_TROOPS(troops,"watch_troops_mod");
    uint64_t result = original(node, context, troops);
    WATCH_TROOPS(troops,"watch_troops_native");
    unsigned short after_mode=word(troops,0x14);
    if(before_mode!=after_mode || before_goal!=word(troops,0x16)) {
        // Native Troops AI re-judges a group every day. A group the mod has
        // just dispatched must not be recalled before it leaves its city.
        Obj lead=nullptr;unsigned soldiers=0,fresh=0,members=0;int day=game_day();
        for(unsigned i=1;i<=500;++i) {
            Obj army=table(0x7E838,i,500);
            if(!valid(army) || !belongs_to_troops(army,troops) || !campaign_controlled(army))continue;
            ++members;soldiers+=word(army,0x16);
            Progress *p=observe_capture(army);
            if(p && p->dispatched && day>=p->dispatch_day && day-p->dispatch_day<10 && !p->forced_return)++fresh;
            if(!lead || word(army,0x16)>word(lead,0x16))lead=army;
        }
        log_event("troops_mode_change",members,(static_cast<unsigned>(before_mode)<<16)|after_mode,
            (static_cast<unsigned>(before_goal)<<16)|word(troops,0x16));
        if(mod_features.recall_veto && lead && fresh && fresh==members && soldiers>=3000 && attack_mode(before_mode) && !attack_mode(after_mode) &&
           before_goal && before_goal<=61 && hostile_city(lead,before_goal)) {
            *reinterpret_cast<unsigned short *>(troops+0x14)=before_mode;
            *reinterpret_cast<unsigned short *>(troops+0x16)=before_goal;
            log_event("fresh_troops_recall_vetoed",army_id(lead),soldiers,before_goal);
        } else if(lead && attack_mode(before_mode) && !attack_mode(after_mode)) {
            // Diagnostics (2026-11-10): an attack group the original turned back and why the veto did not apply.
            unsigned why=(mod_features.recall_veto?0u:1u)|(fresh?0u:2u)|(fresh==members?0u:4u)|(soldiers>=3000?0u:8u)|
                         (before_goal && before_goal<=61?0u:16u)|(before_goal && before_goal<=61 && hostile_city(lead,before_goal)?0u:32u);
            log_event("troops_recall_allowed",army_id(lead),(why<<24)|(fresh<<16)|members,soldiers);
        }
    }
    mod_step("city_node:native_then_members");
    for (unsigned i = 1; i <= 500; ++i) {
        Obj army = table(0x7E838, i, 500);
        if (valid(army) && belongs_to_troops(army, troops)) {
            if(campaign_controlled(army))apply_priority(army,node,context);
        }
    }
    WATCH_TROOPS(troops,"watch_troops_end");
    ReleaseSRWLockExclusive(&policy_lock);
    --in_policy;
    return result;
}
static uint64_t __fastcall retreat_node(Obj node, Obj context, Obj entity)
{
    COST_SCOPE("retreat_node");
    NativePhase phase("army_retreat");
    auto original=fn<Execute>(GA(GA_61C90));
    if(!enabled || in_policy || native_search_depth)return original(node,context,entity);
    Obj army=nullptr;
    Obj blackboard=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context);
    fn<void(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(blackboard,dword(node,0x38),&army,0);
    if(!campaign_controlled(army))return original(node,context,entity);
    ++in_policy;AcquireSRWLockExclusive(&policy_lock);
    Progress *p=observe_capture(army);if(p)remember_campaign(army,*p);
    WATCH_BEFORE(army,p);
    bool changed=false;
    // <=3,000 soldiers: the original retreat decision stands (user decision 2026-10-21); the mod only
    // keeps its own forced return, which also heads home.
    bool small=word(army,0x16)<=3000;
    if(p && !small)changed=campaign_individual_priority(army,*p);
    if(p && p->forced_return && !changed)changed=recovery_return(army,*p);
#ifdef SORTIE_FORMATION
    if(!changed && !small && p && !p->forced_return && !p->returning && word(army,0x16)>=1000) {
        SortieGroup cohort;unsigned cohort_id=0;
        bool cohort_work=sortie_snapshot(army,cohort,cohort_id,true) && sortie_valid_group(cohort);
        if(p->regional_worker || cohort_work) {
            changed=apply_priority(army,node,context);
            // The native <=3,000 retreat gate must not cancel an active rear or
            // support seat and turn its entire freshly formed group home.
            if(!changed && cohort_work && sortie_valid_group(cohort))changed=true;
        }
        // A 1,000-soldier support escort is below the native <=3,000 retreat
        // gate. While its leader and campaign group are valid it keeps its
        // rear seat instead of turning home on the turn after departure.
        if(!changed && !p->regional_worker && !cohort_work && sortie_escort_member(army))changed=sortie_support_step(army);
        // Same gate for other small units the mod dispatched with an explicit
        // mission: 1,000 protectors, capture companions, guards and female
        // commanders capped at 1,000. Missionless/depleted units still use the
        // native retreat below; campaign_individual_priority keeps forced returns.
        bool mission=(p->campaign && p->city && hostile_city(army,p->city)) || p->companion_parent ||
            p->guard_beneficiary || p->operation_leader;
        if(!changed && mission && !p->regional_worker && !cohort_work && word(army,0x16)<=3000) {
            changed=campaign_individual_priority(army,*p);
            if(!changed)changed=apply_priority(army,node,context);
            if(!changed && p->campaign && hostile_city(army,p->city))changed=campaign_step(army,*p,node,context);
            if(changed)log_event("small_mission_retreat_vetoed",army_id(army),word(army,0x16),word(army,0x2A));
        }
    }
#endif
    if(!changed && p && word(army,0x16)>3000)changed=campaign_individual_priority(army,*p);
    if(!changed && p && word(army,0x16)>3000)changed=campaign_siege_priority(army,*p);
    if(!changed && p && word(army,0x16)>3000 && p->operation_leader)changed=true;
    if(!changed && p && word(army,0x16)>3000) {
        if(!p->campaign) {
            unsigned city=0;int nearest=INT32_MAX;
            for(unsigned i=1;i<=500;++i) {
                Obj ally=table(0x7E838,i,500);if(ally==army || !campaign_controlled(ally) || army_force(ally)!=army_force(army))continue;
                Progress *a=observe_capture(ally);if(!a)continue;remember_campaign(ally,*a);
                int range=distance(word(army,0x2A),word(ally,0x2A));
                if(a->campaign && hostile_city(army,a->city) && !tribe_goal(a->city) && range<nearest) {nearest=range;city=a->city;}
            }
            if(city) {p->city=static_cast<unsigned short>(city);p->campaign=true;}
        }
        changed=apply_priority(army,node,context);
        if(!changed) {
            unsigned city=campaign_field_city(army,*p);
            if(city) {p->city=static_cast<unsigned short>(city);p->campaign=true;changed=campaign_step(army,*p,node,context);}
        }
        // 모드 일거리·원작 대체 행동이 모두 없으면 원작 퇴각을 허용
        // (사용자 결정 2026-10-29: 목표 상실 후 40일 제자리 대기 방지).
        if(!changed)changed=try_other(army,node,context);
    }
    if(!changed && p)log_event("native_retreat_allowed",army_id(army),word(army,0x16),word(army,0x2A));
    mod_step(changed?"retreat_node:mod_veto":"retreat_node:native");
    WATCH_AFTER(army,changed?"watch_retreat_node_mod":"watch_retreat_node_pre");
    ReleaseSRWLockExclusive(&policy_lock);--in_policy;
    const uint64_t result=changed ? 1 : original(node,context,entity);
    if(!changed)WATCH_AFTER(army,"watch_retreat_node_native");
    return result;
}

