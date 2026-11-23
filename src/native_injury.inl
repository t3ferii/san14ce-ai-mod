#include "game_addresses.h"
/* Existing AI/UI caller threads only. Manual dispatchers are not hooked. */
static bool injury_auto_district(Obj district) {
    if(!valid(district))return false;
    Obj root=world(),context=root?pointer(root,0x871E8):nullptr;
    return context && district!=fn<Obj(__fastcall *)(Obj)>(GA(GA_330B50))(context);
}
static bool injury_owned(Obj person,Obj force) {
    return valid(person) && person[0xE6]>=2 && person[0xE6]<=4 && personnel_force(person)==force;
}
// Herbalism government planning is a computer-force aid only. A player's
// delegated district shares the force-wide government slots, so planning
// there would overwrite the player's own personnel policy choices.
static bool injury_player_force(Obj force) {
    Obj root=world(),context=root?pointer(root,0x871E8):nullptr;
    if(!context || !valid(force))return false;
    Obj player=fn<Obj(__fastcall *)(Obj)>(GA(GA_330B90))(context);
    return valid(player) && player==force;
}
static bool injury_force_needs_herb(Obj force) {
    for(unsigned id=1;id<=6000;++id) {
        Obj p=table(0x158,id,6000);
        if(injury_owned(p,force) && !personnel::may_auto_work(p[0x161]))return true;
    }
    return false;
}
static unsigned injury_policy(Obj person) {return valid(person)?person[0x14D]:0;}
static bool injury_protected_policy(Obj person) {
    unsigned policy=injury_policy(person);return policy==39 || policy==50;
}
static bool injury_list_has(GameList &list,Obj person) {
    for(Link *l=list_first(list);l;l=l->next)if(l->value==person)return true;
    return false;
}
static unsigned injury_list_size(GameList &list) {
    unsigned n=0;for(Link *l=list_first(list);l;l=l->next)++n;return n;
}
static bool injury_replaceable(GameList &list,Obj keep,unsigned capacity) {
    while(injury_list_size(list)>=capacity) {
        Obj victim=nullptr;
        for(Link *l=list_first(list);l;l=l->next)
            if(l->value!=keep && !injury_protected_policy(l->value) &&
               (injury_policy(l->value)!=41 || injury_protected_policy(keep)))victim=l->value;
        if(!victim)return false;
        personnel_remove(list,victim);
    }
    return true;
}
// local264 (user 2026-11-19 "우리 모드에서 부상무장 있으면 본초학 넣는거 코스트 제한 지키면서 넣어야해. 모략에 있는 시정 빼고 남는
// 코스트로 본초학 넣으면 될거같아"): the government budget is the original's 232500(force,1) (30 + rank stage + 방책), spent by the
// ruler's policy and every officer of the five rows at 232350(officer,&same,0) = max(1, officer+0x182 + policy+0xA2) less the
// same-principle discount; the computer plan 263520 makes the same sums (ghidra 263520 / 232350 / 232500). An officer the herb
// code adds must fit: officers of the 모략 row (category 2, rows[2]) leave, last first, until he does; when the whole row is not
// enough, he is not added and nothing is removed.
static int injury_policy_cost(Obj person) {
    if(!valid(person))return 0;
    unsigned same=0;
    return static_cast<int>(fn<unsigned(__fastcall *)(Obj,unsigned *,int)>(GA(GA_232350))(person,&same,0));
}
static int injury_plan_cost(Obj force,GameList *rows) {
    int used=injury_policy_cost(table(0x158,word(force,0x10),6000));
    for(unsigned r=0;r<5;++r)for(Link *l=list_first(rows[r]);l;l=l->next)used+=injury_policy_cost(l->value);
    return used;
}
static bool injury_fit_budget(Obj force,GameList *rows,int extra) {
    const int budget=fn<int(__fastcall *)(Obj,int)>(GA(GA_232500))(force,1);
    int used=injury_plan_cost(force,rows)+extra;
    if(used<=budget)return true;
    Obj order[16];unsigned n=0;
    for(Link *l=list_first(rows[2]);l && n<16;l=l->next)order[n++]=l->value;
    unsigned drop=0;
    while(drop<n && used>budget)used-=injury_policy_cost(order[n-1-drop++]);
    if(used>budget) {
        log_event("injury_herb_no_budget",word(force,0x10),static_cast<unsigned>(used),static_cast<unsigned>(budget));
        return false;
    }
    for(unsigned i=0;i<drop;++i)personnel_remove(rows[2],order[n-1-i]);
    log_event("injury_herb_budget",word(force,0x10),drop,static_cast<unsigned>(budget-used));
    return true;
}
static bool injury_plan_herb(Obj force,GameList *rows,Obj advisor) {
    if(!valid(force) || !rows || injury_player_force(force) || !injury_force_needs_herb(force))return false;
    GameList &list=rows[4];
    int slots=fn<int(__fastcall *)(Obj,unsigned)>(GA(GA_22ECF0))(force,4);
    if(slots<1 || slots>7)return false;
    // Native force layout: five categories, seven officer IDs per category.
    // Preserve every existing protected provider even when native AI replans (local264: only within the budget).
    for(unsigned i=0;i<7;++i) {
        Obj p=table(0x158,word(force,0xCC+i*2),6000);
        if(injury_owned(p,force) && injury_protected_policy(p) && !injury_list_has(list,p)) {
            if(!injury_fit_budget(force,rows,injury_policy_cost(p)))continue;
            if(!injury_replaceable(list,p,static_cast<unsigned>(slots)))return false;
            list_add(list,p);
        }
    }
    for(Link *l=list_first(list);l;l=l->next)if(injury_policy(l->value)==41)return true;
    Obj selected=nullptr;unsigned best=0;
    for(unsigned id=1;id<=6000;++id) {
        Obj p=table(0x158,id,6000);
        if(!injury_owned(p,force) || injury_policy(p)!=41 || !personnel::may_auto_work(p[0x161]) ||
           id==word(force,0x10) || id==word(force,0x52) || p==advisor)continue;
        Obj definition=table(0x78D50,41,120);
        if(!valid(definition) || definition[0xA0]!=4)continue;
        unsigned level=p[0x182];
        if(!selected || level>best) {selected=p;best=level;}
    }
    // local264: the budget first (it may let 모략 officers go; nothing changes when it cannot fit), then the slot.
    if(selected && !injury_fit_budget(force,rows,injury_policy_cost(selected)))return false;
    if(!selected || !injury_replaceable(list,selected,static_cast<unsigned>(slots))) {
        log_event("injury_herb_unavailable",word(force,0x10),static_cast<unsigned>(slots),0);
        return false;
    }
    list_add(list,selected);
    log_event("injury_herb_planned",word(force,0x10),word(selected,0x10),41);
    return true;
}
// local156 (user 2026-11-14 "Ai자체가 건설 금지 내리는 옵션 ... 투석대 금지면 지세연구 Lv을 7이상 10미만까지만, 요새금지면 7미만 4이상까지만
// ... AI들 시정이 낭비되면 안되니까 ... 5 8이럴수도있으니"): the computer's automatic government plan (263520, before its rows are
// applied) keeps the 지세연구 level (effect 24, the 산천시설 unlock) at 7..9 when 투석대 is excluded (siege_no_catapult). An officer's
// level is the one 25A730 adds: min(10, 2ABF60(officer,3) + officer+0x182) when his policy (+0x14D, table world+0x78D50) carries
// the effect; the ruler (+0x10) and the advisor always count. Walking the rows in order, an officer is dropped once the level is
// already at the stop (7) or when he would lift it to the cap (10); his slot stays free (its order is not spent).
static unsigned policy_level_of(Obj person,unsigned effect) {
    if(!valid(person))return 0;
    const unsigned id=person[0x14D];Obj policy=table(0x78D50,id<0x79?id:0,0x78);
    if(!valid(policy))return 0;
    bool has=false;for(unsigned i=0;i<8;++i)if(policy[0x98+i]==effect)has=true;
    if(!has)return 0;
    const int level=fn<int(__fastcall *)(Obj,int)>(GA(GA_2ABF60))(person,3)+person[0x182];
    return level<0?0u:level>10?10u:static_cast<unsigned>(level);
}
static void ai_policy_cap(Obj force,Obj advisor,GameList *lists) {
    if(!mod_features.ai_policy_cap || !lists || !valid(force) || injury_player_force(force))return;
    const unsigned stop=7u,cap=10u;   // local158 (user "요새금지옵션은 만들면 안돼 투석대만 시정 제한"): 투석대 제외 only
    Obj ruler=table(0x158,word(force,0x10),6000),adv=advisor?advisor:table(0x158,word(force,0x52),6000);
    unsigned level=policy_level_of(ruler,24)+(adv!=ruler?policy_level_of(adv,24):0u),dropped=0;
    for(unsigned row=0;row<5;++row) {
        Obj drop[16];unsigned n=0;
        for(Link *l=list_first(lists[row]);l;l=l->next) {
            if(l->value==ruler || l->value==adv)continue;
            const unsigned add=policy_level_of(l->value,24);if(!add)continue;
            if(level>=stop || level+add>=cap) {if(n<16)drop[n++]=l->value;}
            else level+=add;
        }
        for(unsigned i=0;i<n;++i){personnel_remove(lists[row],drop[i]);++dropped;}
    }
#ifndef NO_RUNTIME_LOG
    if(dropped)log_event("ai_policy_cap",dropped,level,stop);
#endif
}
static Obj __fastcall injury_government_selection(Obj force,Obj layout,GameList *lists) { COST_SCOPE("injury_injury_government_selection");
    Obj advisor=fn<Obj(__fastcall *)(Obj,Obj,GameList *)>(GA(GA_263520))(force,layout,lists);
    if(tribe_force_object(force))return advisor;   // tribe_hands_off: no policy cap, no herb plan
    ai_policy_cap(force,advisor,lists);
    if(enabled && mod_features.injury_herbal && lists && valid(force) && injury_force_needs_herb(force)) {
        for(unsigned i=0;i<7;++i) {
            Obj p=table(0x158,word(force,0xCC+i*2),6000);
            if(p==advisor && injury_protected_policy(p)) {
                advisor=table(0x158,word(force,0x52),6000);
                if(!valid(advisor))advisor=nullptr;
                break;
            }
        }
        injury_plan_herb(force,lists,advisor);
    }
    return advisor;
}
static unsigned __fastcall injury_rumor_actors(Obj district,GameList *actors,int sort) { COST_SCOPE("injury_injury_rumor_actors");
    unsigned result=fn<unsigned(__fastcall *)(Obj,GameList *,int)>(GA(GA_6DD230))(district,actors,sort);
    if(enabled && actors) {personnel_filter_wounded(*actors);result=injury_list_size(*actors);}
    return result;
}
static void injury_synchronize_city(Obj city) {
    COST_SCOPE("injury_synchronize_city");
    if(!valid(city))return;
    auto get_district=*reinterpret_cast<Obj(__fastcall **)(Obj)>(pointer(city,0)+0x80);
    Obj district=get_district(city);
    if(!injury_auto_district(district))return;
    Obj force=table(0xDF30,district[0x10],51);
    if(!valid(force) || tribe_force_object(force))return;   // tribe_hands_off
    // v12.8: removal of wounded assignments = protection; herb planning = injury_herbal.
    if(mod_features.injury) {
    for(unsigned off:{0xCAu,0xCCu}) {
        Obj p=table(0x158,word(city,off),6000);
        if(injury_owned(p,force) && personnel_district(p)==district && !personnel::may_auto_work(p[0x161])) {
            *reinterpret_cast<unsigned short *>(city+off)=0;
            log_event("wounded_military_assignment_removed",word(p,0x10),p[0x161],off);
        }
    }
    // Remove regional assignments through native bookkeeping, so an injured
    // officer's area vector cannot keep driving automatic development.
    for(unsigned id=1;id<=6000;++id) {
        Obj p=table(0x158,id,6000);
        if(!injury_owned(p,force) || personnel_district(p)!=district || personnel::may_auto_work(p[0x161]))continue;
        if(dword(p,0x1E8))fn<int(__fastcall *)(Obj,int,int)>(GA(GA_29DE10))(p,1,0);
        for(unsigned i=0;i<3;++i)if(word(force,0x196+i*2)==id) {
            *reinterpret_cast<unsigned short *>(force+0x196+i*2)=0;force[0x19C+i]=0;
        }
    }
    }
    if(!mod_features.injury_herbal)return;
    // local264: all five rows (force+0x54, 0x1E bytes each, seven officer ids), so the budget sees them and the 모략 row (2)
    // can give way; a changed 모략 row is applied before the 인사 row (4).
    GameList rows[5];
    for(unsigned r=0;r<5;++r) {
        list_init(rows[r],GA(GA_13466B0));
        for(unsigned i=0;i<7;++i) {
            Obj p=table(0x158,word(force,0x54+r*0x1E+i*2),6000);
            if(valid(p))list_add(rows[r],p);
        }
    }
    auto row_changed=[force](GameList &list,unsigned r) {   // against the row's valid officers in order (as built above)
        Obj now[7];unsigned n=0;
        for(unsigned i=0;i<7;++i) {Obj p=table(0x158,word(force,0x54+r*0x1E+i*2),6000);if(valid(p))now[n++]=p;}
        unsigned i=0;
        for(Link *l=list_first(list);l;l=l->next,++i)if(i>=n || now[i]!=l->value)return true;
        return i!=n;
    };
    if(injury_plan_herb(force,rows,nullptr)) {
        for(unsigned r:{2u,4u})
            if(row_changed(rows[r],r))fn<void(__fastcall *)(Obj,unsigned,GameList *,int)>(GA(GA_2EFA70))(force,r,&rows[r],0);
    }
    for(unsigned r=0;r<5;++r)list_free(rows[r]);
}
static uint64_t __fastcall injury_departure_create(Obj node,Obj context,Obj subject) { COST_SCOPE("injury_departure_create");
    if(enabled) {
        Obj output=personnel_context_object(node,context,0x38);
        if(output)for(Link *l=list_first(*reinterpret_cast<GameList *>(output));l;l=l->next) {
            Obj p=valid(l->value)?table(0x158,word(l->value,0x12),6000):nullptr;
            if(valid(p) && !personnel::may_auto_work(p[0x161]) && !tribe_person(p))return 2;   // tribe_hands_off
        }
    }
    return fn<Execute>(GA(GA_365A0))(node,context,subject);
}
static uint64_t __fastcall injury_dual_destruction(Obj n,Obj c,Obj s) { COST_SCOPE("injury_dual_destruction");return personnel_healthy_command(n,c,s,GA(GA_53270),0x3C);}
static uint64_t __fastcall injury_estrange(Obj n,Obj c,Obj s) { COST_SCOPE("injury_estrange");return personnel_healthy_command(n,c,s,GA(GA_533F0),0x3C);}
static uint64_t __fastcall injury_hidden_poison(Obj n,Obj c,Obj s) { COST_SCOPE("injury_hidden_poison");return personnel_healthy_command(n,c,s,GA(GA_54110),0x3C);}

static uint64_t __fastcall injury_spy_away(Obj n,Obj c,Obj s) { COST_SCOPE("injury_spy_away");return personnel_healthy_command(n,c,s,GA(GA_54BC0),0x3C);}
