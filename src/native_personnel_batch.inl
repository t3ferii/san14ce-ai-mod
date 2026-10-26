/* Automatic recruitment executor. Its caller must be the native UI thread,
 * after the player has accepted a multiple-target selection dialog. No
 * background thread or remote call may invoke this routine. */
struct PersonnelEmployCommand {Obj target,actor;unsigned mode;unsigned padding;};
struct PersonnelBatchResult {
    unsigned sent=0,blocked=0,no_actor=0,no_orders=0,failed=0,duplicates=0,advised_against=0;
};
static Obj personnel_order_district(Obj actor) {
    // Match 1F5DE0 exactly: the issuing city is Person+E2, and that city's
    // virtual +80 returns the district whose command count is at +14.
    Obj city=fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),word(actor,0xE2));
    if(!valid(city))return nullptr;
    auto district=*reinterpret_cast<Obj(__fastcall **)(Obj)>(pointer(city,0)+0x80);
    Obj result=district(city);return valid(result)?result:nullptr;
}
static bool personnel_order_available(Obj actor,unsigned reserved_orders=0,Obj reserved_district=nullptr) {
    Obj district=personnel_order_district(actor);
    int cost=fn<int(__fastcall *)()>(0x6D3660)();
    unsigned reserved=district==reserved_district?reserved_orders:0;
    return district && cost>=0 && district[0x14]>=reserved &&
        static_cast<unsigned>(cost)<=district[0x14]-reserved;
}
static bool personnel_recommended(Obj force,Obj target,Obj actor) {
    unsigned recommended=0;int score=-1;unsigned parameters[32]={};
    // ABI checked at 74DF68..74DF96: R8 recommendation, R9 score,
    // stack arg 6 actor ID, arg 7 target ID. The action-2 branch uses
    // native recruitment advice, including the strategist's accuracy.
    fn<Obj(__fastcall *)(Obj,int,unsigned *,int *,unsigned *,int,int,int,Obj,int)>
        (0x2F36B0)(force,2,&recommended,&score,parameters,
                  word(actor,0x10),word(target,0x10),0,nullptr,0);
    return recommended!=0;
}
[[maybe_unused]] static PersonnelBatchResult personnel_auto_employ(Obj district,GameList &selected,
    const Obj *excluded_actors=nullptr,unsigned excluded_count=0,
    unsigned reserved_orders=0,Obj reserved_district=nullptr) {
    PersonnelBatchResult result;
    Obj force=valid(district)?table(0xDF30,district[0x10],51):nullptr;
    if(!enabled || !valid(force)) {result.failed=1;return result;}
    bool reserved[6001]={},handled[6001]={};
    for(unsigned i=0;excluded_actors && i<excluded_count;++i) {
        Obj actor=excluded_actors[i];
        if(valid(actor) && word(actor,0x10)<=6000)reserved[word(actor,0x10)]=true;
    }
    unsigned scanned=0;
    for(Link *target_link=list_first(selected);target_link && scanned<6000;
        target_link=target_link->next,++scanned) {
        Obj target=target_link->value;
        if(!valid(target) || word(target,0x10)>6000) {++result.failed;continue;}
        unsigned target_id=word(target,0x10);
        if(handled[target_id]) {++result.duplicates;continue;}
        handled[target_id]=true;
        GameList candidates;list_init(candidates,0x13436B0);
        fn<unsigned(__fastcall *)(Obj,GameList *)>(0x6DA8A0)(district,&candidates);
        personnel_filter_wounded(candidates);
        Obj chosen=nullptr;bool allowed=false,budget=false;
        unsigned actor_count=0;
        for(Link *actor_link=list_first(candidates);actor_link && actor_count<6000;
            actor_link=actor_link->next,++actor_count) {
            Obj actor=actor_link->value;
            if(!valid(actor) || word(actor,0x10)>6000 || reserved[word(actor,0x10)] ||
               (word(actor,0x15E)&1) || personnel_force(actor)!=force)continue;
            unsigned reason=personnel_recruitment_reason(target,district,actor);
            if(reason==personnel::MissingContext || personnel::recruitment_blocked(reason))continue;
            PersonnelEmployCommand command={target,actor,0,0};
            if(!fn<int(__fastcall *)(PersonnelEmployCommand *)>(0x1EFE60)(&command))continue;
            allowed=true;
            if(!personnel_order_available(actor,reserved_orders,reserved_district))continue;
            budget=true;
            // User decision 2026-10-25: dispatch only a recommended actor,
            // with no non-recommended fallback. 2F36B0 action=2 combines
            // 2784B0 with strategist accuracy (rva_002F36B0.c:91-131);
            // the native low-chance advice sets recommended=0.
            bool recommended=personnel_recommended(force,target,actor);
            if(!chosen && recommended)chosen=actor;
        }
        list_free(candidates);
        if(!chosen) {
            if(!allowed && personnel::recruitment_blocked(
                   personnel_recruitment_reason(target,district,nullptr)))++result.blocked;
            else if(allowed && !budget)++result.no_orders;
            else if(allowed && budget)++result.advised_against;
            else ++result.no_actor;
            continue;
        }
        // Native 1ED920 sets busy at 1F5DE0 and consumes its own district's
        // cost once per dispatch; do not deduct orders separately.
        // Validate again after advice before dispatch. A void native
        // function's return register must never be interpreted as success.
        PersonnelEmployCommand command={target,chosen,0,0};
        if(!personnel_order_available(chosen,reserved_orders,reserved_district) ||
           !fn<int(__fastcall *)(PersonnelEmployCommand *)>(0x1EFE60)(&command)) {
            ++result.failed;continue;
        }
        unsigned actor_id=word(chosen,0x10);
        reserved[actor_id]=true;
        int player_command=fn<int(__fastcall *)(Obj)>(0x233960)(district);
        fn<void(__fastcall *)(PersonnelEmployCommand *,int)>(0x1ED920)(&command,player_command);
        if(word(chosen,0x15E)&1) {
            ++result.sent;log_event("auto_employ_sent",target_id,actor_id,1);
        } else {++result.failed;log_event("auto_employ_native_rejected",target_id,actor_id);}
    }
    return result;
}
