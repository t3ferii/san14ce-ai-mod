/* Shared tactical plans for newly dispatched and independently deployed
 * armies. No native Troops merge, troop redistribution, remote calls or new
 * hooks: policy_lock and the existing AI caller own every decision/order. */
struct CampaignOperation {
    Obj root,army[7],group[7];
    unsigned short commander[7],group_mode[7],group_goal[7],old_city[7];
    unsigned char old_order[7][0x34];unsigned old_flags[7];
    bool old_campaign[7];campaign::Role role[7];
    unsigned force,city,count,leader,facing,enemy,builder;
    unsigned center,planned_center,last_hex,last_enemy,base_count,best_range;
    int started,phase_day,decision_day,last_progress,buff_day;
    campaign::Phase phase;
    bool active,fort_attempted,band_attempted,capturing,force_march;
};
static CampaignOperation campaign_operations[501];
static bool campaign_operation_identity(Obj army,const Progress &p) {
    if(!p.operation_leader || p.operation_leader>500)return false;
    const CampaignOperation &o=campaign_operations[p.operation_leader];
    if(!o.active || o.root!=world() || o.city!=p.operation_city)return false;
    for(unsigned i=0;i<o.count;++i)if(o.army[i]==army && o.commander[i]==word(army,0x12) &&
        o.force==army_force(army) && o.group[i]==table(0x7F8D8,word(army,0x58),500))return true;
    return false;
}
static bool campaign_operation_member(Obj army,const Progress &p) {return campaign_operation_identity(army,p);}
static unsigned campaign_support_traits(Obj army) {
    Obj person=table(0x158,word(army,0x12),6000);if(!valid(person))return 0;
    unsigned count=0;for(unsigned i=0;i<5;++i) {
        unsigned trait=word(person,0x118+i*2);if(trait==23 || trait==94 || trait==96)++count;
    }
    return count;
}
static campaign::Role campaign_army_role(Obj army) {
    if(campaign_support_traits(army) && word(army,0x16)<5000)return campaign::support;
    if(sortie::siege(army[0x1B]))return campaign::siege;
    if(army[0x1B]==3 || word(army,0x16)<3000)return campaign::middle;
    return campaign::front;
}
static bool campaign_available(Obj army,Progress &p,unsigned city) {
    if(p.regional_worker || campaign_group_member(army) || !campaign_controlled(army) || word(army,0x16)<1000 || word(army,0x2A)>=48400 ||
       p.forced_return || p.returning || p.combat_target || p.combat_city || p.home_defense_city || p.companion_parent || p.guard_beneficiary || p.facility || p.credit || p.build_chain || p.morale_recovering ||
       army[0x1A]<=70 || army[0x37]==11 || army[0x37]==12 || army[0x37]==7 || priority_mission(army,p) ||
       campaign_operation_identity(army,p) || game_day()<p.operation_retry_day)return false;
    if(p.campaign) {
        if(p.city!=city || p.credit || p.build_chain)return false;
        if(p.region_target<48400 && campaign_region_hostile(army,campaign_area_id(p.region_target)))return false;
        Obj goal=campaign_goal(city);
        return goal && distance(word(army,0x2A),fn<unsigned(__fastcall *)(Obj)>(0x22D740)(goal))<=8;
    }
    // A completed regional capture/idle field unit may join. Active movement
    // without confirmed capture evidence, defensive missions and other city
    // offensives are not stolen from the native planner.
    Obj group=table(0x7F8D8,word(army,0x58),500);if(!valid(group))return false;
    unsigned mode=word(group,0x14);if(mode>=1 && mode<=5)return false;
    Obj region=army_area(army);Obj tile=valid(region)?table(0xE270,word(region,0x3A),48399):nullptr;
    return targetless(army) || (p.built_once && army[0x37]==1 && army[0x38]==9 &&
        valid(tile) && tile[0x14]==army_force(army));
}
static void campaign_operation_detach(CampaignOperation &o,unsigned i,unsigned id) {
    Obj a=o.army[i];if(!valid(a) || word(a,0x12)!=o.commander[i] || army_force(a)!=o.force)return;
    unsigned member_id=army_id(a);if(!member_id)return; Progress &p=progress[member_id];if(p.operation_leader!=id)return;
    p.operation_leader=0;p.operation_city=0;p.siege_city=0;p.operation_retry_day=game_day()+20;
    if(p.forced_siege_city) {p.city=p.siege_city=p.forced_siege_city;p.campaign=true;return;}
    p.campaign=o.old_campaign[i];p.city=o.old_city[i];
    // Preserve a newer native assignment or urgent individual order.
    Obj g=table(0x7F8D8,word(a,0x58),500);
    if(p.forced_return || p.returning || p.combat_target || p.combat_city || p.home_defense_city || p.companion_parent || p.guard_beneficiary || p.morale_recovering || p.facility ||
       !valid(g) || g!=o.group[i] || word(g,0x16)!=o.group_goal[i] || word(g,0x14)!=o.group_mode[i])return;
    fn<void(__fastcall *)(Obj)>(0xA2410)(a);memcpy(a+0x2C,o.old_order[i],sizeof(o.old_order[i]));
    for(unsigned j=0;j<5;++j)*reinterpret_cast<unsigned short *>(a+0x2C+j*2)=48400;
    *reinterpret_cast<unsigned *>(a+0x1F0)=o.old_flags[i];
    fn<void(__fastcall *)(Obj)>(0x26C940)(a);
}
static void campaign_operation_release(unsigned id,const char *reason) {
    CampaignOperation &o=campaign_operations[id];o.active=false;
    if(o.root==world())for(unsigned i=0;i<o.count;++i)campaign_operation_detach(o,i,id);
    log_event(reason,id,o.city,o.count);
}
static void campaign_operation_drop(CampaignOperation &o,unsigned i,unsigned id) {
    campaign_operation_detach(o,i,id);o.army[i]=nullptr;
}
static unsigned campaign_operation_facing(unsigned center,unsigned goal) {
    unsigned best=0;int range=48400;
    for(unsigned d=0;d<6;++d) {unsigned h=sortie::neighbor(center,d);if(h<48400 && distance(h,goal)<range) {range=distance(h,goal);best=d;}}
    return best;
}
static uint64_t campaign_operation_defenders(Obj army,unsigned city,unsigned center) {
    Obj goal=campaign_goal(city);uint64_t count=goal?dword(goal,0x3C):0;
    for(unsigned id=1;id<=500;++id) {
        Obj e=table(0x7E838,id,500);if(!valid(e) || word(e,0x2A)>=48400 || e[0x37]==11 || e[0x37]==12 ||
            !fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(army_force(army),army_force(e)))continue;
        int range=distance(word(e,0x2A),center);
        bool incoming=e[0x37]==4 && e[0x38]==5 && word(e,0x3A)==city;
        if(range<=8 || (incoming && range<=15))count+=word(e,0x16);
    }
    return count;
}
static unsigned campaign_field_city(Obj army,Progress &p) {
    if(p.campaign && hostile_city(army,p.city))return p.city;
    unsigned best=0;int score=INT32_MAX;
    for(unsigned id=1;id<=51;++id) {
        Obj goal=campaign_goal(id);if(!goal || !hostile_city(army,id) || !campaign_available(army,p,id))continue;
        unsigned center=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(goal);if(center>=48400)continue;
        int range=distance(word(army,0x2A),center);if(range>15)continue;
        int value=range*100+static_cast<int>((std::min)(dword(goal,0x3C),100000u)/100);
        if(dword(goal,0x3C)<6000)value-=600;
        if(value<score) {score=value;best=id;}
    }
    return best;
}
static bool campaign_operation_create(Obj army,Progress &p) {
    COST_SCOPE("campaign_operation_create");
    unsigned city=campaign_field_city(army,p);Obj goal=campaign_goal(city);if(!goal || !campaign_available(army,p,city))return false;
    unsigned center=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(goal);if(center>=48400 || distance(word(army,0x2A),center)>8)return false;
    std::vector<Obj> candidates;
    for(unsigned id=1;id<=500;++id) {
        Obj a=table(0x7E838,id,500);if(!valid(a) || army_force(a)!=army_force(army) || word(a,0x2A)>=48400 ||
            distance(word(a,0x2A),center)>15 || distance(word(a,0x2A),word(army,0x2A))>10)continue;
        Progress *m=observe_capture(a);if(!m)continue;remember_campaign(a,*m);
        if(campaign_available(a,*m,city))candidates.push_back(a);
    }
    std::sort(candidates.begin(),candidates.end(),[](Obj a,Obj b) {
        auto ra=campaign_army_role(a),rb=campaign_army_role(b);
        if(ra!=rb)return ra<rb;
        return word(a,0x16)!=word(b,0x16)?word(a,0x16)>word(b,0x16):army_id(a)<army_id(b);
    });
    // Keep a practical front/middle/rear mix when several native groups are nearby.
    std::vector<Obj> selected;unsigned roles[4]={};
    for(Obj a:candidates) {
        auto role=campaign_army_role(a);unsigned limit=role==campaign::front?3:role==campaign::middle?3:1;
        if(roles[role]>=limit || selected.size()==7)continue;
        selected.push_back(a);++roles[role];
    }
    if(std::find(selected.begin(),selected.end(),army)==selected.end())return false;
    candidates=std::move(selected);
    Obj leader=nullptr;uint64_t attack=0;
    for(Obj a:candidates)if(campaign::assaults(campaign_army_role(a),word(a,0x16))) {
        if(!leader && campaign_army_role(a)==campaign::front)leader=a;
        if(attack<0xFFFFFFFFu)attack+=word(a,0x16);
    }
    if(!leader)return false;
    uint64_t enemy=campaign_operation_defenders(leader,city,center);
    if(!campaign::enough(attack,enemy)) {
#ifdef SORTIE_FORMATION
        if(p.campaign)campaign_request_reinforcements(army,p,(enemy*3+1)/2-attack);
#endif
        return false;
    }
    unsigned id=army_id(leader);CampaignOperation o={};o.root=world();o.force=army_force(leader);o.city=city;
    o.started=o.phase_day=o.last_progress=game_day();o.decision_day=-1;o.buff_day=-1;o.builder=7;
    o.best_range=static_cast<unsigned>(distance(word(leader,0x2A),center));
    o.phase=campaign::approach;o.center=o.planned_center=o.last_hex=word(leader,0x2A);o.active=true;
    for(Obj a:candidates) {
        if(o.count==7)break;unsigned i=o.count++;Progress &m=progress[army_id(a)];
        o.army[i]=a;o.commander[i]=word(a,0x12);o.group[i]=table(0x7F8D8,word(a,0x58),500);
        o.group_mode[i]=word(o.group[i],0x14);o.group_goal[i]=word(o.group[i],0x16);o.role[i]=campaign_army_role(a);
        o.old_city[i]=m.city;o.old_campaign[i]=m.campaign;memcpy(o.old_order[i],a+0x2C,sizeof(o.old_order[i]));o.old_flags[i]=dword(a,0x1F0);
        if(a==leader)o.leader=i;
    }
    attack=0;for(unsigned i=0;i<o.count;++i)if(campaign::assaults(o.role[i],word(o.army[i],0x16)))attack+=word(o.army[i],0x16);
    if(!campaign::enough(attack,enemy))return false;
    campaign_operations[id]=o;
    for(unsigned i=0;i<o.count;++i) {
        Obj a=o.army[i];Progress &m=progress[army_id(a)];m.operation_leader=static_cast<unsigned short>(id);
        m.operation_city=static_cast<unsigned short>(city);m.city=static_cast<unsigned short>(city);m.campaign=true;
#ifdef SORTIE_FORMATION
        SortieGroup g;unsigned old;if(sortie_snapshot(a,g,old)) {g.active=false;sortie_commit(old,g);}
#endif
    }
    log_event("campaign_field_formed",id,o.count,city);return true;
}
static bool campaign_operation_safe(Obj army,unsigned target,unsigned city) {
    if(target>=48400)return false;
    Obj goal=campaign_goal(city);if(!goal)return false;
    Obj tile=table(0xE270,target,48399);if(!valid(tile) || word(tile,0x1A))return false;
    for(unsigned id=1;id<=500;++id) {Obj a=table(0x7E838,id,500);if(a!=army && valid(a) && word(a,0x2A)==target)return false;}
    return true;
}
static bool campaign_operation_move(Obj army,unsigned target,unsigned city) {
    if(!campaign_operation_safe(army,target,city))return false;
    CampaignRoute route=campaign_safe_route(army,target,true);if(route.cells.empty() || !route.guards.empty())return false;
    return campaign_route_order(army,route);
}
static void campaign_operation_positions(CampaignOperation &o,unsigned center) {
    unsigned front=0,middle=0,support=0;
    for(unsigned i=0;i<o.count;++i)if(o.army[i]) {
        Obj a=o.army[i];Progress &p=progress[army_id(a)];if(p.facility || p.morale_recovering || p.combat_target || p.combat_city || p.home_defense_city || p.companion_parent || p.guard_beneficiary)continue;
        unsigned target=center;
        if(o.role[i]==campaign::front) {
            if(i!=o.leader)target=sortie::neighbor(center,(o.facing+(front++?5:1))%6);
        } else if(o.role[i]==campaign::middle || o.role[i]==campaign::siege) {
            target=sortie::neighbor(center,(o.facing+(middle==0?2:middle==1?4:3))%6);++middle;
        } else {
            target=sortie::neighbor(center,(o.facing+3)%6);target=sortie::neighbor(target,(o.facing+3+(support++?1:0))%6);
        }
        if(!campaign_operation_move(a,target,o.city)) {
            // Narrow corridors permit a short column; rear units still never
            // receive castle-fire movement or a self-targeted city assault.
            unsigned here=word(a,0x2A),best=48400;int range=distance(here,target);
            for(unsigned d=0;d<6;++d) {unsigned h=sortie::neighbor(here,d);if(h<48400 && distance(h,target)<range && campaign_operation_safe(a,h,o.city)) {best=h;range=distance(h,target);}}
            bool moved=best<48400 && campaign_operation_move(a,best,o.city);
            if(!moved && !campaign::assaults(o.role[i],word(a,0x16))) {
                // A blocked rear route must not leave an old castle assault
                // command active. Wait safely while the shared plan retries.
                fn<void(__fastcall *)(Obj)>(0xA2410)(a);fn<void(__fastcall *)(Obj,int)>(0x2EAAE0)(a,13);
                fn<void(__fastcall *)(Obj,int,int)>(0x2EAB40)(a,255,0);fn<void(__fastcall *)(Obj)>(0x26C940)(a);
            }
        }
    }
}
static bool campaign_operation_fort(Obj army) {
    for(unsigned id=1;id<=3000;++id) {Obj b=table(0x6DA98,id,3000);
        if(!valid(b) || b[0x13]!=army_force(army) || (b[0x12]!=3 && b[0x12]!=4) ||
            word(b,0x10)>=48400 || distance(word(army,0x2A),word(b,0x10))>3)continue;
        int maximum=fn<int(__fastcall *)(Obj)>(0x2AC120)(b);if(maximum>0 && word(b,0x14)>=maximum)return true;
    }
    return false;
}
static bool campaign_operation_prepare(CampaignOperation &o) {
    if(o.builder<o.count && o.army[o.builder]) {
        Obj b=o.army[o.builder];Progress &p=progress[army_id(b)];
        if(continue_build(b,p))return true;
        if(campaign_forced_siege_priority(b,p))return true;
        Obj group=table(0x7F8D8,word(b,0x58),500);
        if(p.finished_build && group==o.group[o.builder] && valid(group) && word(group,0x14)==7) {
            bool consistent=true;
            for(unsigned id=1;id<=500;++id) {
                Obj a=table(0x7E838,id,500);if(!valid(a) || !belongs_to_troops(a,group))continue;
                bool member=false;
                for(unsigned i=0;i<o.count;++i)if(o.army[i]==a && o.commander[i]==word(a,0x12) && campaign_operation_identity(a,progress[id]))member=true;
                if(!member)consistent=false;
            }
            if(consistent) {
                *reinterpret_cast<unsigned short *>(group+0x14)=o.group_mode[o.builder];
                *reinterpret_cast<unsigned short *>(group+0x16)=o.group_goal[o.builder];
            }
        }
        o.builder=7;o.last_progress=game_day();
    }
    bool protected_front=true;
    Obj target=campaign_goal(o.city);unsigned target_hex=target?fn<unsigned(__fastcall *)(Obj)>(0x22D740)(target):48400;
    for(unsigned i=0;i<o.count;++i)if(o.army[i] && campaign::required(o.role[i],word(o.army[i],0x16)) && distance(word(o.army[i],0x2A),target_hex)<=5)
        protected_front=protected_front && campaign_operation_fort(o.army[i]);
    if(protected_front) {if(o.buff_day<0)o.buff_day=game_day();return game_day()-o.buff_day<10;}
    o.buff_day=-1;
    Obj goal=campaign_goal(o.city);bool strong=goal && dword(goal,0x3C)>=6000;
    if(!strong || game_day()-o.phase_day>=20)return false;
    unsigned preferred=0;
    if(!o.fort_attempted) {o.fort_attempted=true;preferred=4;}
    else if(!o.band_attempted) {o.band_attempted=true;preferred=7;}
    if(!preferred)return false;
    for(unsigned i=0;i<o.count;++i)if(o.army[i] && i!=o.leader) {
        Obj a=o.army[i];Progress &p=progress[army_id(a)];if(o.role[i]==campaign::support || p.facility)continue;
        bool credit=p.credit,chain=p.build_chain;unsigned short saved=p.build_center;
        Obj area=army_area(a);if(!valid(area))continue;
        p.credit=true;p.build_chain=true;p.build_center=word(area,0x3A);
        if(try_build(a,p,o.center,preferred)) {o.builder=i;log_event("campaign_prepare_build",army_id(a),preferred,o.city);return true;}
        p.credit=credit;p.build_chain=chain;p.build_center=saved;
    }
    return false;
}
static bool campaign_operation_capture_work(CampaignOperation &o) {
    Obj leader=o.army[o.leader];Progress &lead=progress[army_id(leader)];
    if(o.capturing && o.builder<o.count && o.army[o.builder]) {
        Obj worker=o.army[o.builder];Progress &p=progress[army_id(worker)];
        if(continue_build(worker,p)) {o.last_progress=game_day();campaign_operation_positions(o,word(leader,0x2A));return true;}
        if(campaign_forced_siege_priority(worker,p))return true;
        lead.build_chain=p.build_chain;lead.cycle_mask|=p.cycle_mask;lead.build_center=p.build_center;
        Obj group=table(0x7F8D8,word(worker,0x58),500);
        if(valid(group) && group==o.group[o.builder] && word(group,0x14)==7) {
            *reinterpret_cast<unsigned short *>(group+0x14)=o.group_mode[o.builder];
            *reinterpret_cast<unsigned short *>(group+0x16)=o.group_goal[o.builder];
        }
        o.builder=7;o.capturing=false;
    }
    if(!lead.credit && !lead.build_chain)return false;
    Obj area=army_area(leader);
    if(valid(area) && area_full(area) && !area_hostile_facilities(leader,area)) {
        for(unsigned i=0;i<o.count;++i)if(o.army[i]) {Progress &p=progress[army_id(o.army[i])];
            if(p.build_center==lead.build_center) {p.credit=false;p.build_chain=false;}}
        return false;
    }
    for(unsigned i=0;i<o.count;++i)if(o.army[i] && o.role[i]!=campaign::support) {
        Obj worker=o.army[i];Progress &p=progress[army_id(worker)];
        p.credit=lead.credit;p.build_chain=lead.build_chain;p.build_center=lead.build_center;p.cycle_mask=lead.cycle_mask;
        if(try_build(worker,p)) {o.builder=i;o.capturing=true;lead.credit=false;
            o.last_progress=game_day();campaign_operation_positions(o,word(leader,0x2A));return true;}
    }
    o.last_progress=game_day();campaign_operation_positions(o,word(leader,0x2A));return true;
}
static bool campaign_operation_group_march(CampaignOperation &o) {
    struct MarchScope {MarchScope(){++campaign_group_march_depth;}~MarchScope(){--campaign_group_march_depth;}} scope;
    Obj leader=o.army[o.leader],goal=campaign_goal(o.city);if(!valid(leader) || !goal)return false;
    unsigned here=word(leader,0x2A),target=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(goal);
    if(here>=48400 || target>=48400)return false;
    if(o.decision_day==game_day() && o.last_hex==here)return true;
    o.decision_day=game_day();o.last_hex=here;o.facing=campaign_operation_facing(here,target);
    CampaignRoute route=campaign_safe_route(leader,target,true,true);
    if(route.cells.size()>1)campaign_operation_positions(o,route.cells[1]);
    if(distance(here,target)<=1)for(unsigned i=0;i<o.count;++i)if(o.army[i] && campaign::assaults(o.role[i],word(o.army[i],0x16))) {
        offensive_order(o.army[i],o.city,true);fn<void(__fastcall *)(Obj)>(0x26C940)(o.army[i]);
    }
    return true;
}
static bool campaign_operation_step(unsigned id) {
    COST_SCOPE("campaign_operation_step");
    if(!id || id>500)return false;
    CampaignOperation &o=campaign_operations[id];int day=game_day();Obj goal=campaign_goal(o.city);
    if(!o.active)return false;
    if(o.root!=world() || day<o.started) {o.active=false;return false;}
    Obj leader=o.army[o.leader];
    Progress *committed=valid(leader)?observe_capture(leader):nullptr;
    if(committed && campaign_forced_siege_priority(leader,*committed))return true;
    if(!goal || !valid(leader) || !hostile_city(leader,o.city)) {campaign_operation_release(id,"campaign_field_goal_finished");return false;}
    for(unsigned i=0;i<o.count;++i)if(o.army[i]) {
        Obj a=o.army[i];if(!valid(a) || !army_id(a)) {if(i==o.leader) {campaign_operation_release(id,"campaign_field_reassess");return false;}o.army[i]=nullptr;continue;}
        Progress &p=progress[army_id(a)];Obj group=table(0x7F8D8,word(a,0x58),500);
        bool changed=group!=o.group[i] || !valid(group) ||
            ((word(group,0x16)!=o.group_goal[i] || word(group,0x14)!=o.group_mode[i]) && !(i==o.builder && p.facility));
        if(!campaign_controlled(a) || word(a,0x12)!=o.commander[i] || army_force(a)!=o.force ||
            word(a,0x16)<1000 || p.forced_return || p.returning || p.combat_target || p.combat_city || p.home_defense_city || p.companion_parent || p.guard_beneficiary || changed ||
            (p.morale_recovering && !campaign::required(o.role[i],word(a,0x16)))) {
            if(i==o.leader) {campaign_operation_release(id,"campaign_field_reassess");return false;}
            campaign_operation_drop(o,i,id);
        }
    }
    unsigned center=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(goal),here=word(leader,0x2A);
    if(center>=48400 || here>=48400) {campaign_operation_release(id,"campaign_field_reassess");return false;}
    unsigned range_to_city=static_cast<unsigned>(distance(here,center));
    if(campaign_operation_capture_work(o))return true;
    if(o.force_march)return campaign_operation_group_march(o);
    if(range_to_city<o.best_range) {o.best_range=range_to_city;o.last_progress=day;}
    // Several AI nodes can query the same member on one day. Preserve the
    // shared orders until actual movement or a new day requires another step.
    uint64_t enemy=campaign_operation_defenders(leader,o.city,center),attack=0,ready_attack=0;
    for(unsigned i=0;i<o.count;++i)if(o.army[i]) {
        Obj a=o.army[i];Progress &p=progress[army_id(a)];
        if(!campaign::assaults(o.role[i],word(a,0x16)) || p.facility || p.morale_recovering || a[0x1A]<=70)continue;
        attack+=word(a,0x16);if(distance(word(a,0x2A),center)<=5)ready_attack+=word(a,0x16);
    }
    if(o.phase!=campaign::assault && !campaign::enough(attack,enemy) && day-o.last_progress<20) {campaign_operation_release(id,"campaign_field_insufficient");return false;}
    auto bases=campaign_enemy_bases(leader,o.city);
    if(bases.size()!=o.base_count || enemy<o.last_enemy) {o.last_progress=day;o.base_count=static_cast<unsigned>(bases.size());}
    o.last_enemy=static_cast<unsigned>((std::min)(enemy,uint64_t(0xFFFFFFFFu)));
    if(o.decision_day==day && o.last_hex==here)return true;
    o.last_hex=here;
    if(day!=o.decision_day) {
        o.decision_day=day;o.center=here;o.enemy=0;
        o.facing=campaign_operation_facing(here,center);o.planned_center=here;
    }
    // Reuse existing obstacle evidence and native damage, with one front
    // clearing the obstruction while the remaining units escort it.
    for(unsigned i=0;i<o.count;++i)if(o.army[i] && o.role[i]==campaign::front) {
        Obj a=o.army[i];Progress &p=progress[army_id(a)];observe_obstacle_motion(a,p);
        if(!try_destroy(a,p))continue;
        unsigned char order[0x34];memcpy(order,a+0x2C,sizeof(order));unsigned flags=dword(a,0x1F0);
        campaign_operation_positions(o,here);
        memcpy(a+0x2C,order,sizeof(order));*reinterpret_cast<unsigned *>(a+0x1F0)=flags;
        fn<void(__fastcall *)(Obj)>(0x26C940)(a);return true;
    }
    // Native AI alone selects and orders interceptions against field armies.
    unsigned crossing=campaign_path_capture_hex(leader,center);
    if(crossing<48400) {
        CampaignRoute route=campaign_safe_route(leader,crossing,true);
        if(route.cells.size()>1) {
            o.planned_center=route.cells[1];campaign_operation_positions(o,o.planned_center);
            log_event("campaign_field_path_capture",id,crossing,o.city);return true;
        }
    }
    if(bases.size()>2 && o.phase!=campaign::assault) {
        unsigned target=bases[0];for(unsigned h:bases)if(distance(here,h)<distance(here,target))target=h;
        CampaignRoute route=campaign_safe_route(leader,target,true);
        if(!route.cells.empty() && route.guards.empty() && route.cells.size()>1)o.planned_center=route.cells[1];
        campaign_operation_positions(o,o.planned_center);
    } else {
        if(o.phase==campaign::approach) {o.phase=campaign::rally;o.phase_day=day;log_event("campaign_role_rally",id,o.city,center);}
        if(o.phase==campaign::rally && campaign::enough(ready_attack,enemy)) {
            o.phase=campaign::prepare;o.phase_day=day;log_event("campaign_core_ready",id,static_cast<unsigned>(ready_attack),o.city);
        }
        if(o.phase==campaign::prepare) {
            campaign_operation_positions(o,here);
            if(!campaign_operation_prepare(o)) {o.phase=campaign::assault;log_event("campaign_role_assault",id,o.city,static_cast<unsigned>(ready_attack));}
            if(!o.active)return false;
        }
        if(o.phase==campaign::assault) {
            campaign_operation_positions(o,here);
            for(unsigned i=0;i<o.count;++i)if(o.army[i]) {
                Obj a=o.army[i];Progress &p=progress[army_id(a)];if(p.facility || p.morale_recovering ||
                    !campaign::assaults(o.role[i],word(a,0x16)) || distance(word(a,0x2A),center)>5)continue;
                p.siege_city=static_cast<unsigned short>(o.city);offensive_order(a,o.city,true);fn<void(__fastcall *)(Obj)>(0x26C940)(a);
            }
            return true;
        }
        if(o.phase==campaign::rally) {
            CampaignRoute route=campaign_rally_route(leader,center);
            if(route.cells.size()>1) {
                unsigned end=1;
                o.planned_center=route.cells[end];
            }
            campaign_operation_positions(o,o.planned_center);
            if(day-o.phase_day>=20) {o.phase=campaign::assault;o.phase_day=day;}
        }
    }
    return true;
}
static bool campaign_siege_priority(Obj army,Progress &p) {
    if(campaign_operation_identity(army,p))return campaign_operation_step(p.operation_leader);
    p.operation_leader=0;p.operation_city=0;
    if(!campaign_operation_create(army,p))return false;
    return campaign_operation_step(p.operation_leader);
}
