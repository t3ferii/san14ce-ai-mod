/* Computer/delegated district recruitment only. These two behavior-tree
 * callbacks are separate from manual UI 6CD040 -> 1ED920(command,1).
 * Called synchronously on the game's existing AI thread. */
static unsigned recruit_ai_reason(Obj target,Obj district,Obj envoy) {
    unsigned reason=personnel_recruitment_reason(target,district,envoy);
    if(reason & personnel::MissingContext)return reason;
    Obj force=table(0xDF30,district[0x10],51);
    Obj ruler=valid(force)?table(0x158,word(force,0x10),6000):nullptr;
    // The requested AI policy never spends an envoy on a hated recruiting
    // ruler, including a spouse/sworn sibling envoy. Manual UI is unchanged.
    if(valid(ruler) && fn<int(__fastcall *)(Obj,Obj)>(0x232A00)(target,ruler))
        reason|=personnel::DislikedRuler;
    return reason;
}
static bool recruit_ai_blocked(unsigned reason) {
    return (reason & personnel::MissingContext) || personnel::recruitment_blocked(reason);
}
static bool recruit_ai_family_available(Obj target,Obj district,GameList &envoys) {
    Obj force=table(0xDF30,district[0x10],51);
    unsigned count=0;
    for(Link *link=list_first(envoys);link && count<6000;link=link->next,++count) {
        Obj actor=link->value;
#ifdef INJURY_PROTECTION
        if(valid(actor) && !personnel::may_auto_work(actor[0x161]))continue;
#endif
        if(!valid(actor) || personnel_force(actor)!=force || (word(actor,0x15E)&1) ||
           !personnel_family_envoy(target,actor) || recruit_ai_blocked(recruit_ai_reason(target,district,actor)))continue;
        PersonnelEmployCommand command={target,actor,0,0};
        if(fn<int(__fastcall *)(PersonnelEmployCommand *)>(0x1EFE60)(&command))return true;
    }
    return false;
}
static uint64_t __fastcall recruit_ai_targets(Obj node,Obj context,Obj subject) {
    NativePhase phase("ai_recruit_targets");
    uint64_t result=fn<Execute>(0x7B360)(node,context,subject);
    if(!enabled || result!=1)return result;
    Obj district=personnel_context_object(node,context,0x38);
    Obj output=personnel_context_object(node,context,0x3C);
    if(!valid(district) || !output)return result;
    GameList envoys={};bool envoys_ready=false;
    GameList &targets=*reinterpret_cast<GameList *>(output);
    Link *link=list_first(targets);
    while(link) {
        Obj target=link->value;
        unsigned reason=recruit_ai_reason(target,district,nullptr);
        if(recruit_ai_blocked(reason)) {
            bool exception=false;
            if(!(reason & (personnel::MissingContext|personnel::DislikedRuler)) &&
               !personnel::recruitment_hard_blocked(reason)) {
                if(!envoys_ready) {
                    list_init(envoys,0x13436B0);
                    fn<unsigned(__fastcall *)(Obj,GameList *)>(0x6DA8A0)(district,&envoys);
                    envoys_ready=true;
                }
                exception=recruit_ai_family_available(target,district,envoys);
            }
            if(!exception) {
                log_event("ai_recruit_target_blocked",valid(target)?word(target,0x10):0,reason,district[0x10]);
                personnel_remove(targets,target);
                // Removal may release every duplicate link for this target.
                link=list_first(targets);continue;
            }
        }
        link=link->next;
    }
    if(envoys_ready)list_free(envoys);
    return result;
}
static uint64_t __fastcall recruit_ai_command(Obj node,Obj context,Obj subject) {
    NativePhase phase("ai_recruit_command");
    if(enabled) {
        Obj actor=personnel_context_object(node,context,0x38);
        Obj target=personnel_context_object(node,context,0x3C);
        if(!valid(actor) || !valid(target))return 2;
#ifdef INJURY_PROTECTION
        if(!personnel::may_auto_work(actor[0x161])) {
            log_event("wounded_auto_command_blocked",word(actor,0x10),actor[0x161],0x2CC50);
            return 2;
        }
#endif
        unsigned reason=recruit_ai_reason(target,personnel_district(actor),actor);
        if(recruit_ai_blocked(reason)) {
            log_event("ai_recruit_command_blocked",word(target,0x10),reason,word(actor,0x10));
            return 2; // no 1ED920 dispatch, order allocation or actor busy flag
        }
    }
    return fn<Execute>(0x2CC50)(node,context,subject);
}
