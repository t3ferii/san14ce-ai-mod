// Order ownership on existing AI caller threads. No new hooks or disk I/O.
// Actual hostile castle damage locks a castle attack. A rallied cohort
// stationary for thirty days locks its shared formation advance.
static Obj campaign_assault_goal(unsigned city) {
    if(!city || city>61)return nullptr;
    Obj goal=fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),city);
    return valid(goal) && word(goal,0x4E)==city?goal:nullptr;
}
static bool campaign_assault_hostile(Obj army,unsigned city) {
    Obj goal=campaign_assault_goal(city),force=goal?fn<Obj(__fastcall *)(Obj)>(0x22C990)(goal):nullptr;
    if(!valid(force))return false;
    unsigned owner=(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(force,0)+0x60))(force);
    return fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(army_force(army),owner)!=0;
}
static bool campaign_commit_castle(Obj army,Progress &p,unsigned city,unsigned reason) {
    if(!campaign_controlled(army) || word(army,0x16)<1000 || p.forced_return || p.returning || !campaign_assault_hostile(army,city))return false;
    p.forced_siege_city=static_cast<unsigned short>(city);p.forced_siege_reason=static_cast<unsigned char>(reason);
    p.city=p.siege_city=p.combat_city=static_cast<unsigned short>(city);p.campaign=true;
    p.facility=0;p.construction_hex=48400;p.credit=false;p.build_chain=false;p.regional_worker=false;
    p.region_target=48400;p.near_capture_hex=48400;p.guard_beneficiary=0;p.guard_requested=false;
    p.combat_target=0;p.home_defense_city=0;p.companion_target=0;p.companion_parent=0;
    p.morale_recovering=false;p.morale_building=false;p.morale_saved=false;
    p.operation_leader=0;p.operation_city=0;
    army[0x46]=0;*reinterpret_cast<unsigned short *>(army+0x42)=48400;
    *reinterpret_cast<unsigned *>(army+0x1F0)&=~10u;
    Obj native_group=table(0x7F8D8,word(army,0x58),500);
    if(valid(native_group) && (word(native_group,0x14)!=10 || word(native_group,0x16)!=city)) {
        *reinterpret_cast<unsigned short *>(native_group+0x14)=10;
        *reinterpret_cast<unsigned short *>(native_group+0x16)=static_cast<unsigned short>(city);
    }
    // Skip reissuing an already correct order; the native path owns travel.
    unsigned action=city<=51?4:5,kind=city<=51?5:6;
    if(army[0x37]!=action || army[0x38]!=kind || word(army,0x3A)!=city) {
        fn<void(__fastcall *)(Obj)>(0xA2410)(army);
        fn<void(__fastcall *)(Obj,int)>(0x2EAAE0)(army,static_cast<int>(action));
        fn<void(__fastcall *)(Obj,int,int)>(0x2EAB40)(army,static_cast<int>(kind),static_cast<int>(city));
        fn<void(__fastcall *)(Obj)>(0x26C940)(army);
    }
#ifdef SORTIE_FORMATION
    AcquireSRWLockExclusive(&sortie_state_lock);sortie_escorts[army_id(army)]={};ReleaseSRWLockExclusive(&sortie_state_lock);
#endif
    return true;
}
static void campaign_prepare_group_march(Obj a,Progress &m,unsigned city) {
    bool was_building=m.facility || a[0x37]==8 || a[0x37]==9;
    m.city=m.forced_siege_city=static_cast<unsigned short>(city);m.forced_siege_reason=3;
    m.campaign=true;m.siege_city=0;m.combat_city=0;m.combat_target=0;
    m.facility=0;m.construction_hex=48400;m.credit=false;m.build_chain=false;
    m.region_target=48400;m.near_capture_hex=48400;m.guard_beneficiary=0;
    m.morale_recovering=false;m.morale_building=false;m.morale_saved=false;
    a[0x46]=0;*reinterpret_cast<unsigned short *>(a+0x42)=48400;*reinterpret_cast<unsigned *>(a+0x1F0)&=~10u;
    // Even a failed formation path must leave a valid native order after
    // cancelling construction. Its current tile is always a real destination.
    if(was_building) {
        unsigned here=word(a,0x2A);
        fn<void(__fastcall *)(Obj)>(0xA2410)(a);
        fn<void(__fastcall *)(Obj,int)>(0x2EAAE0)(a,1);
        fn<void(__fastcall *)(Obj,int,int)>(0x2EAB40)(a,9,static_cast<int>(here));
        fn<void(__fastcall *)(Obj,int,unsigned)>(0x2EE1E0)(a,0,here);
    }
    Obj group=table(0x7F8D8,word(a,0x58),500);
    if(valid(group)) {*reinterpret_cast<unsigned short *>(group+0x14)=10;*reinterpret_cast<unsigned short *>(group+0x16)=static_cast<unsigned short>(city);}
}
static bool campaign_forced_siege_priority(Obj army,Progress &p) {
    if(native_search_depth || !campaign_controlled(army) || word(army,0x16)<1000 || p.forced_return || p.returning)return false;
    const CampaignHit &hit=campaign_hits[army_id(army)];
    bool castle_hit=hit.present && hit.root==world() && hit.victim==army && hit.commander==word(army,0x12) && hit.force==army_force(army) &&
        hit.source_city && game_day()>=hit.city_day && game_day()-hit.city_day<=10 && campaign_assault_hostile(army,hit.source_city);
    unsigned locked=castle_hit?hit.source_city:p.forced_siege_reason==1?p.forced_siege_city:0;
    if(locked && campaign_assault_hostile(army,locked)) {
#ifdef SORTIE_FORMATION
        SortieGroup g;unsigned id;if(sortie_snapshot(army,g,id)) {g.active=false;sortie_commit(id,g);}
#endif
        if(p.operation_leader && campaign_operation_identity(army,p))campaign_operations[p.operation_leader].active=false;
        return campaign_commit_castle(army,p,locked,1);
    }
    if(p.forced_siege_reason==3 && p.forced_siege_city && campaign_assault_hostile(army,p.forced_siege_city)) {
#ifdef SORTIE_FORMATION
        SortieGroup g;unsigned id;
        if(sortie_snapshot(army,g,id) && g.force_march) {
            bool valid_members=valid(g.group) && valid(g.army[6]);
            for(unsigned i=0;i<7;++i)if(g.army[i]) {
                Obj a=g.army[i];Progress *m=observe_capture(a);
                if(!campaign_controlled(a) || word(a,0x12)!=g.commander[i] || !belongs_to_troops(a,g.group) || word(a,0x16)<1000 || word(a,0x2A)>=48400 ||
                    !m || m->returning || m->forced_return || m->forced_siege_reason==1)valid_members=false;
            }
            if(valid_members) {for(Obj a:g.army)if(a)campaign_prepare_group_march(a,progress[army_id(a)],g.city);
                return sortie_forced_group_march(army,g,id);}
        }
#endif
        if(campaign_operation_identity(army,p) && campaign_operations[p.operation_leader].force_march) {
            CampaignOperation &o=campaign_operations[p.operation_leader];bool valid_members=true;
            for(unsigned i=0;i<o.count;++i)if(o.army[i]) {Progress *m=observe_capture(o.army[i]);
                if(!m || !campaign_operation_identity(o.army[i],*m) || m->returning || m->forced_return || word(o.army[i],0x16)<1000 || word(o.army[i],0x2A)>=48400 || m->forced_siege_reason==1)valid_members=false;}
            if(valid_members) {for(unsigned i=0;i<o.count;++i)if(o.army[i]) {campaign_prepare_group_march(o.army[i],progress[army_id(o.army[i])],o.city);o.group_mode[i]=10;o.group_goal[i]=static_cast<unsigned short>(o.city);}
                return campaign_operation_group_march(o);}
        }
    }
    if(p.forced_siege_city) {p.forced_siege_city=0;p.forced_siege_reason=0;p.combat_city=0;p.siege_city=0;}
    // [combat] group_advance=0: no new 30-day stationary group advance (marches already started continue).
    if(!mod_features.group_advance)return false;
    Obj members[7]={};unsigned count=0,city=0,operation_id=0;bool rallied=false;
#ifdef SORTIE_FORMATION
    unsigned group_id=0;SortieGroup g={};
    if(sortie_snapshot(army,g,group_id) && sortie_valid_group(g)) {
        if(g.force_march)return false;
        city=g.city;rallied=g.rallied || sortie_rally_arrived(g);
        for(Obj member:g.army)if(member)members[count++]=member;
    } else group_id=0;
#endif
    if(!count && campaign_operation_identity(army,p)) {
        operation_id=p.operation_leader;const CampaignOperation &o=campaign_operations[operation_id];
        if(o.force_march)return false;
        city=o.city;rallied=o.phase!=campaign::approach;bool leader_included=false;
        for(unsigned i=0;i<o.count;++i)if(o.army[i] && campaign_operation_identity(o.army[i],progress[army_id(o.army[i])])) {
            Obj a=o.army[i],native_group=o.group[i];const Progress &m=progress[army_id(a)];
            if(!m.facility && (word(native_group,0x14)!=o.group_mode[i] || word(native_group,0x16)!=o.group_goal[i]))continue;
            members[count++]=a;if(i==o.leader)leader_included=true;
        }
        if(!leader_included)return false;
    }
    if(!count || !rallied || !campaign_assault_hostile(army,city))return false;
    for(unsigned i=0;i<count;++i) {
        Progress *m=observe_capture(members[i]);
        if(!m || !campaign_controlled(members[i]) || word(members[i],0x16)<1000 || word(members[i],0x2A)>=48400 || m->forced_return || m->returning ||
           campaign_castle_locked(members[i]) || game_day()-m->motion_day<30)return false;
    }
    // One transition restores the shared final goal and issues a formation
    // advance. Only the authorized group movement writer bypasses this lock.
    for(unsigned i=0;i<count;++i) {
        campaign_prepare_group_march(members[i],progress[army_id(members[i])],city);
    }
#ifdef SORTIE_FORMATION
    if(group_id) {
        g.force_march=true;g.rallied=true;g.working=false;g.builder=7;g.demolisher=0;g.route_count=0;g.day=-1;
        sortie_commit(group_id,g);return sortie_forced_group_march(army,g,group_id);
    }
#endif
    if(operation_id) {
        CampaignOperation &o=campaign_operations[operation_id];
        for(unsigned i=0;i<o.count;++i)if(o.army[i]) {
            bool included=false;for(unsigned n=0;n<count;++n)if(o.army[i]==members[n])included=true;
            if(!included) {campaign_operation_drop(o,i,operation_id);continue;}
            o.group_mode[i]=10;o.group_goal[i]=static_cast<unsigned short>(city);
        }
        o.force_march=true;o.capturing=false;o.builder=7;o.decision_day=-1;
        return campaign_operation_group_march(o);
    }
    return false;
}
