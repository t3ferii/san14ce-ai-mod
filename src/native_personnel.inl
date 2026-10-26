/* Candidate bindings. Included only in an isolated personnel fixture until
 * install guards, all requested screens and saved ownership are validated.
 * All helper calls are synchronous native-thread calls, never remote calls. */
#include "personnel_policy.h"

static Obj personnel_context_object(Obj node,Obj context,unsigned field) {
    Obj value=nullptr,store=fn<Obj(__fastcall *)(Obj)>(0xBF7A40)(context);
    if(!fn<unsigned char(__fastcall *)(Obj,unsigned,Obj *,int)>(0xB26470)
        (store,dword(node,field),&value,0))return nullptr;
    return value;
}
static bool personnel_healthy_actor(Obj node,Obj context,unsigned field) {
    Obj actor=personnel_context_object(node,context,field);
    return !valid(actor) || personnel::may_auto_work(actor[0x161]);
}
static Obj personnel_district(Obj officer) {
    return valid(officer)?table(0xE0D0,officer[0xE0],51):nullptr;
}
static Obj personnel_force(Obj officer) {
    return valid(officer)?fn<Obj(__fastcall *)(Obj)>(0x22CA00)(officer):nullptr;
}
static bool personnel_foreign_family(Obj relative,Obj own_force) {
    if(!valid(relative) || relative[0xE6]==7 || relative[0xE6]==9)return false;
    Obj force=personnel_force(relative);
    // A living relative in no force is also absent from the recruiting force.
    return !valid(force) || force!=own_force;
}
static bool personnel_family_envoy(Obj target,Obj envoy) {
    if(!valid(target) || !valid(envoy) || target==envoy)return false;
    return word(target,0xBA)==word(envoy,0x10) ||
           (word(target,0xBC)>0 && word(target,0xBC)<=6000 &&
            word(target,0xBC)==word(envoy,0xBC));
}
static unsigned personnel_recruitment_reason(Obj target,Obj district,Obj envoy) {
    personnel::RecruitmentFacts facts={};
    Obj force=valid(district)?table(0xDF30,district[0x10],51):nullptr;
    Obj ruler=valid(force)?table(0x158,word(force,0x10),6000):nullptr;
    facts.context_known=valid(target) && valid(force) && valid(ruler);
    if(!facts.context_known)return personnel::MissingContext;
    Obj current_force=personnel_force(target);
    Obj current_ruler=valid(current_force)?table(0x158,word(current_force,0x10),6000):nullptr;
    // Direction matters: the target likes its current ruler. The reciprocal
    // ruler->target relation alone must not add this warning.
    facts.likes_current_ruler=valid(current_force) && current_force!=force && valid(current_ruler) &&
       fn<int(__fastcall *)(Obj,Obj)>(0x232EE0)(target,current_ruler)!=0;
    facts.high_loyalty=target[0xE8]>=100;
    // Native 2786DD compares the banned ruler's Person ID, not a force or
    // district ID. It does not gate on the byte timer at +164. Let native
    // expiry/clearing own that field, including a pending zero-timer record.
    facts.service_ban=word(target,0x162)!=0 && word(target,0x162)==word(ruler,0x10);
    if(facts.high_loyalty || facts.service_ban)return personnel::recruitment_reason(facts);
    facts.envoy_is_spouse_or_sworn_sibling=personnel_family_envoy(target,envoy);
    if(facts.envoy_is_spouse_or_sworn_sibling)return personnel::recruitment_reason(facts);
    facts.dislikes_ruler=fn<int(__fastcall *)(Obj,Obj)>(0x232A00)(target,ruler)!=0;
    facts.foreign_spouse=personnel_foreign_family(table(0x158,word(target,0xBA),6000),force);
    // BC is a group ID, not the only sibling's Person ID. The native helper
    // enumerates every other group member; checking just its anchor loses
    // the third brother and the spouse/envoy exception.
    GameList siblings;list_init(siblings,0x13436B0);
    fn<unsigned(__fastcall *)(Obj,GameList *)>(0x2B5620)(target,&siblings);
    for(Link *link=list_first(siblings);link;link=link->next)
        if(personnel_foreign_family(link->value,force)) {facts.foreign_sworn_sibling=true;break;}
    list_free(siblings);
    return personnel::recruitment_reason(facts);
}
static void personnel_remove(GameList &list,Obj officer) {
    fn<void(__fastcall *)(GameList *,Obj *)>(0x335060)(&list,&officer);
}
static void personnel_filter_wounded(GameList &list) {
    // Remove may free every duplicate entry. Restart from the head after
    // removal instead of following a possibly freed saved next pointer.
    Link *link=list_first(list);
    while(link) {
        Obj person=link->value;
        if(valid(person) && !personnel::may_auto_work(person[0x161])) {
            personnel_remove(list,person);link=list_first(list);
        } else link=link->next;
    }
}
static uint64_t personnel_actor_node(Obj node,Obj context,Obj subject,unsigned original_rva,unsigned output_field=0x38) {
    uint64_t result=fn<Execute>(original_rva)(node,context,subject);
    if(enabled && result==1) {
        Obj list=personnel_context_object(node,context,output_field);
        if(list)personnel_filter_wounded(*reinterpret_cast<GameList *>(list));
    }
    return result;
}
static uint64_t __fastcall personnel_employ_actors(Obj node,Obj context,Obj subject) {
    return personnel_actor_node(node,context,subject,0x39490);
}
static uint64_t __fastcall personnel_search_actors(Obj node,Obj context,Obj subject) {
    return personnel_actor_node(node,context,subject,0x3D170);
}
static uint64_t __fastcall personnel_officer_candidates(Obj node,Obj context,Obj subject) {
    return personnel_actor_node(node,context,subject,0x7E550,0x3C);
}
static uint64_t personnel_healthy_command(Obj node,Obj context,Obj subject,unsigned original_rva,unsigned actor_field) {
    if(enabled && !personnel_healthy_actor(node,context,actor_field)) {
        Obj actor=personnel_context_object(node,context,actor_field);
        log_event("wounded_auto_command_blocked",word(actor,0x10),actor[0x161],original_rva);
        return 2;
    }
    return fn<Execute>(original_rva)(node,context,subject);
}
static uint64_t __fastcall personnel_departure(Obj node,Obj context,Obj subject) {
#ifdef SPLIT_BASE
    // The dispatcher lives in the combat DLL; update its TLS on this same
    // original AI caller thread, including exception unwinding.
    if(split_combat_api)split_combat_api->departure_scope(1);
    uint64_t result;
    __try {result=personnel_healthy_command(node,context,subject,0x34F10,0x38);}
    __finally {if(split_combat_api)split_combat_api->departure_scope(-1);}
    return result;
#elif defined(SORTIE_FORMATION)
    ++sortie_auto_departure_scope;
    uint64_t result=personnel_healthy_command(node,context,subject,0x34F10,0x38);
    --sortie_auto_departure_scope;return result;
#else
    return personnel_healthy_command(node,context,subject,0x34F10,0x38);
#endif
}
static uint64_t __fastcall personnel_transport(Obj node,Obj context,Obj subject) {
    return personnel_healthy_command(node,context,subject,0x35770,0x38);
}
static uint64_t __fastcall personnel_recruit_officer(Obj node,Obj context,Obj subject) {
    return personnel_healthy_command(node,context,subject,0x3DBF0,0x3C);
}
static uint64_t __fastcall personnel_training_officer(Obj node,Obj context,Obj subject) {
    return personnel_healthy_command(node,context,subject,0x3E490,0x3C);
}
static uint64_t __fastcall personnel_spy_officer(Obj node,Obj context,Obj subject) {
    return personnel_healthy_command(node,context,subject,0x3E100,0x3C);
}
static uint64_t __fastcall personnel_recruitment_targets(Obj node,Obj context,Obj subject) {
    uint64_t result=fn<Execute>(0x7B360)(node,context,subject);
    if(!enabled || result!=1)return result;
    Obj district=personnel_context_object(node,context,0x38);
    Obj output=personnel_context_object(node,context,0x3C);
    if(!valid(district) || !output)return result;
    GameList envoys;list_init(envoys,0x13436B0);
    fn<unsigned(__fastcall *)(Obj,GameList *)>(0x6DA8A0)(district,&envoys);
    personnel_filter_wounded(envoys);
    GameList &targets=*reinterpret_cast<GameList *>(output);
    Link *link=list_first(targets);
    while(link) {
        Obj target=link->value;
        unsigned reason=personnel_recruitment_reason(target,district,nullptr);
        if(personnel::recruitment_blocked(reason)) {
            bool exception=false;
            if(!personnel::recruitment_hard_blocked(reason))
                for(Link *actor=list_first(envoys);actor;actor=actor->next)
                    if(personnel_family_envoy(target,actor->value)) {exception=true;break;}
            if(!exception) {
                log_event("recruitment_target_blocked",word(target,0x10),reason,district[0x10]);
                personnel_remove(targets,target);link=list_first(targets);continue;
            }
        }
        link=link->next;
    }
    list_free(envoys);
    return result;
}
static uint64_t __fastcall personnel_employ_command(Obj node,Obj context,Obj subject) {
    if(enabled) {
        Obj actor=personnel_context_object(node,context,0x38);
        Obj target=personnel_context_object(node,context,0x3C);
        if(valid(actor) && valid(target)) {
            unsigned reason=personnel_recruitment_reason(target,personnel_district(actor),actor);
            if(!personnel::may_auto_work(actor[0x161]) || personnel::recruitment_blocked(reason)) {
                log_event("recruitment_command_blocked",word(target,0x10),reason,word(actor,0x10));
                return 2; // fail this AI node before allocating/spending a native order
            }
        }
    }
    return fn<Execute>(0x2CC50)(node,context,subject);
}
static uint64_t __fastcall personnel_search_command(Obj node,Obj context,Obj subject) {
    if(enabled) {
        Obj actor=personnel_context_object(node,context,0x3C);
        if(valid(actor) && !personnel::may_auto_work(actor[0x161]))return 2;
    }
    return fn<Execute>(0x53E50)(node,context,subject);
}
