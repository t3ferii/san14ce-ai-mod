/* Existing AI/UI caller threads only. Manual dispatchers are not hooked. */
static bool injury_auto_district(Obj district) {
    if(!valid(district))return false;
    Obj root=world(),context=root?pointer(root,0x871E8):nullptr;
    return context && district!=fn<Obj(__fastcall *)(Obj)>(0x32E8D0)(context);
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
    Obj player=fn<Obj(__fastcall *)(Obj)>(0x32E910)(context);
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
static bool injury_plan_herb(Obj force,GameList &list,Obj advisor) {
    if(!valid(force) || injury_player_force(force) || !injury_force_needs_herb(force))return false;
    int slots=fn<int(__fastcall *)(Obj,unsigned)>(0x22D2A0)(force,4);
    if(slots<1 || slots>7)return false;
    // Native force layout: five categories, seven officer IDs per category.
    // Preserve every existing protected provider even when native AI replans.
    for(unsigned i=0;i<7;++i) {
        Obj p=table(0x158,word(force,0xCC+i*2),6000);
        if(injury_owned(p,force) && injury_protected_policy(p) && !injury_list_has(list,p)) {
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
    if(!selected || !injury_replaceable(list,selected,static_cast<unsigned>(slots))) {
        log_event("injury_herb_unavailable",word(force,0x10),static_cast<unsigned>(slots),0);
        return false;
    }
    list_add(list,selected);
    log_event("injury_herb_planned",word(force,0x10),word(selected,0x10),41);
    return true;
}
static Obj __fastcall injury_government_selection(Obj force,Obj layout,GameList *lists) { COST_SCOPE("injury_injury_government_selection");
    Obj advisor=fn<Obj(__fastcall *)(Obj,Obj,GameList *)>(0x261B70)(force,layout,lists);
    if(enabled && lists && valid(force) && injury_force_needs_herb(force)) {
        for(unsigned i=0;i<7;++i) {
            Obj p=table(0x158,word(force,0xCC+i*2),6000);
            if(p==advisor && injury_protected_policy(p)) {
                advisor=table(0x158,word(force,0x52),6000);
                if(!valid(advisor))advisor=nullptr;
                break;
            }
        }
        injury_plan_herb(force,lists[4],advisor);
    }
    return advisor;
}
static unsigned __fastcall injury_rumor_actors(Obj district,GameList *actors,int sort) { COST_SCOPE("injury_injury_rumor_actors");
    unsigned result=fn<unsigned(__fastcall *)(Obj,GameList *,int)>(0x6DAF20)(district,actors,sort);
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
    if(!valid(force))return;
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
        if(dword(p,0x1E8))fn<int(__fastcall *)(Obj,int,int)>(0x29BF00)(p,1,0);
        for(unsigned i=0;i<3;++i)if(word(force,0x196+i*2)==id) {
            *reinterpret_cast<unsigned short *>(force+0x196+i*2)=0;force[0x19C+i]=0;
        }
    }
    }
    if(!mod_features.injury_herbal)return;
    GameList list;list_init(list,0x13436B0);
    for(unsigned i=0;i<7;++i) {
        Obj p=table(0x158,word(force,0xCC+i*2),6000);
        if(valid(p))list_add(list,p);
    }
    if(injury_plan_herb(force,list,nullptr)) {
        bool changed=injury_list_size(list)>7;unsigned i=0;
        for(Link *l=list_first(list);l;l=l->next,++i)
            if(i>=7 || word(force,0xCC+i*2)!=word(l->value,0x10))changed=true;
        if(changed)fn<void(__fastcall *)(Obj,unsigned,GameList *,int)>(0x2ED6B0)(force,4,&list,0);
    }
    list_free(list);
}
static uint64_t __fastcall injury_departure_create(Obj node,Obj context,Obj subject) {
    if(enabled) {
        Obj output=personnel_context_object(node,context,0x38);
        if(output)for(Link *l=list_first(*reinterpret_cast<GameList *>(output));l;l=l->next) {
            Obj p=valid(l->value)?table(0x158,word(l->value,0x12),6000):nullptr;
            if(valid(p) && !personnel::may_auto_work(p[0x161]))return 2;
        }
    }
    return fn<Execute>(0x35A40)(node,context,subject);
}
static uint64_t __fastcall injury_dual_destruction(Obj n,Obj c,Obj s) {return personnel_healthy_command(n,c,s,0x52600,0x3C);}
static uint64_t __fastcall injury_estrange(Obj n,Obj c,Obj s) {return personnel_healthy_command(n,c,s,0x52780,0x3C);}
static uint64_t __fastcall injury_hidden_poison(Obj n,Obj c,Obj s) {return personnel_healthy_command(n,c,s,0x534A0,0x3C);}

static uint64_t __fastcall injury_spy_away(Obj n,Obj c,Obj s) {return personnel_healthy_command(n,c,s,0x53F40,0x3C);}
