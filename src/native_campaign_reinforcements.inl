/* Run only in the existing controlled AI callback, under policy_lock.
 * 42E70's first parameter is unused in the captured native body. We reuse
 * command preparation/dispatch; we do not forge a native behaviour node. */
struct CampaignReinforcementAudit {Obj root;int day;};
static CampaignReinforcementAudit campaign_reinforcement_attempt[52][52],campaign_reinforcement_source[52];
static unsigned campaign_city_owner(Obj city) {
    Obj force=valid(city)?fn<Obj(__fastcall *)(Obj)>(0x22C990)(city):nullptr;if(!valid(force))return 0;
    auto id=*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(force,0)+0x60);return id(force);
}
static unsigned campaign_incoming(Obj army,unsigned city) {
    unsigned sum=0;Obj goal=campaign_goal(city);if(!goal)return 0;
    unsigned center=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(goal);
    for(unsigned i=1;i<=500;++i) {
        Obj a=table(0x7E838,i,500);if(!valid(a) || army_force(a)!=army_force(army) ||
           word(a,0x2A)>=48400 || distance(word(a,0x2A),center)<=15)continue;
        Obj group=table(0x7F8D8,word(a,0x58),500);
        if(valid(group) && attack_mode(word(group,0x14)) && word(group,0x16)==city &&
           !progress[i].forced_return && a[0x37]!=11 && a[0x37]!=12)sum+=word(a,0x16);
    }
    return sum;
}
static unsigned campaign_reinforcement_reserve(Obj source,unsigned force) {
    return campaign_source_reserve(source,force);
}
static bool campaign_request_reinforcements(Obj army,Progress &p,uint64_t needed,unsigned guard_region,unsigned guard_goal) {
    unsigned force=army_force(army),goal=p.city;int day=game_day();Obj target=campaign_goal(goal);
    if(!force || force>51 || !target || !campaign_controlled(army) || !hostile_city(army,goal))return false;
    bool protector=guard_region!=0;
    unsigned incoming=protector?0:campaign_incoming(army,goal);if(incoming>=needed)return false;
    needed-=incoming;
    auto &audit=campaign_reinforcement_attempt[force][goal];
    if(audit.root==world() && day>=audit.day && day-audit.day<10)return false;
    audit={world(),day};
    Obj origin=fn<Obj(__fastcall *)(Obj)>(0x22A5A0)(army);unsigned target_hex=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(target);
    std::vector<Obj> sources;
    for(unsigned id=1;id<=51;++id) {
        Obj c=campaign_goal(id);if(!c || campaign_city_owner(c)!=force || distance(fn<unsigned(__fastcall *)(Obj)>(0x22D740)(c),target_hex)>60)continue;
        const auto &used=campaign_reinforcement_source[id];if(used.root==world() && used.day==day)continue;
        if(protector) {
            Obj region=table(0x6CAF0,guard_region,500);
            if(!valid(region) || distance(fn<unsigned(__fastcall *)(Obj)>(0x22D740)(c),word(region,0x3A))>15)continue;
        }
        unsigned stock=campaign::available(dword(c,0x3C));
        if(stock>=1000u)sources.push_back(c);
    }
    std::sort(sources.begin(),sources.end(),[origin,target_hex](Obj a,Obj b){
        if((a==origin)!=(b==origin))return a==origin;
        return distance(fn<unsigned(__fastcall *)(Obj)>(0x22D740)(a),target_hex)<distance(fn<unsigned(__fastcall *)(Obj)>(0x22D740)(b),target_hex);
    });
    unsigned dispatched=0,total=0;
    for(Obj source:sources) {
        unsigned stock=campaign::available(dword(source,0x3C));
        GameList people;list_init(people,0x13436B0);
        fn<void(__fastcall *)(Obj,GameList *)>(0x2D7180)(source,&people);
        fn<void(__fastcall *)(GameList *)>(0x2A0780)(&people);
        fn<void(__fastcall *)(GameList *,int,int,int)>(0xD32C0)(&people,0,1,0);
        std::vector<Obj> candidates;
        for(Link *l=list_first(people);l;l=l->next)if(sortie_auto_person(l->value) && !l->value[0x161])candidates.push_back(l->value);
        list_free(people);
        std::sort(candidates.begin(),candidates.end(),[](Obj a,Obj b){
            return fn<int(__fastcall *)(Obj,unsigned,unsigned)>(0x22FCE0)(a,0,0xFFF)>
                   fn<int(__fastcall *)(Obj,unsigned,unsigned)>(0x22FCE0)(b,0,0xFFF);
        });
        for(Obj person:candidates) {
            bool deployed=false;
            for(unsigned id=1;id<=500;++id) {
                Obj a=table(0x7E838,id,500);
                if(valid(a) && word(a,0x12)==word(person,0x10)) {deployed=true;break;}
            }
            if(deployed)continue;
            stock=campaign::available(dword(source,0x3C));if(stock<1000u)break;
            Obj district=table(0xE0D0,person[0xE0],51);int capacity=fn<int(__fastcall *)(Obj)>(0x2311A0)(person);
            unsigned formation=sortie_field_formation(person);
            if(!valid(district) || !district[0x14] || capacity<1000 || !formation)continue;
            unsigned soldiers=protector?1000:(std::min)(static_cast<unsigned>(capacity),(std::min)(stock,
                static_cast<unsigned>((std::min)(uint64_t(12000),(std::max)(uint64_t(6000),needed-total)))));
            unsigned command[30];sortie_command(nullptr,target,person,formation,soldiers,command);
            Obj land=table(0x77428,command[3],20),water=table(0x77428,command[4],20);
            if(!valid(land) || !valid(water))continue;
            auto money=*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(source,0)+0x90);
            if(money(source)<word(land,0x9A)+word(water,0x9A))continue;
            unsigned free_armies=0,free_groups=0;
            for(unsigned i=1;i<=500;++i) {if(!valid(table(0x7E838,i,500)))++free_armies;if(!valid(table(0x7F8D8,i,500)))++free_groups;}
            if(!free_armies || !free_groups)break;
            if(person[0xA9]==1)command[2]=1000;
            Obj reinforcement=person[0x161]?nullptr:sortie_issue(command,1);
            if(!valid(reinforcement))continue;
            Obj group=table(0x7F8D8,word(reinforcement,0x58),500);
            if(valid(group)) {
                *reinterpret_cast<unsigned short *>(group+0x14)=10;
                *reinterpret_cast<unsigned short *>(group+0x16)=static_cast<unsigned short>(goal);
            }
            Progress *r=observe_capture(reinforcement);if(r) {r->campaign=true;r->city=static_cast<unsigned short>(goal);}
            // Native command starts as city attack; immediately put the new
            // unit through the same morale/adjacency/capture policy.
            if(r && protector) {
                auto tiles=campaign_transit_tiles(reinforcement,guard_region,guard_goal);
                bool assigned=false;unsigned attempts=0;
                for(unsigned h:tiles) {
                    CampaignRoute route=campaign_safe_route(reinforcement,h,true);
                    if(!route.cells.empty() && route.guards.empty() && campaign_route_order(reinforcement,route)) {
                        r->guard_requested=true;r->guard_day=day;r->guard_beneficiary=static_cast<unsigned short>(army_id(army));
                        r->guard_region=static_cast<unsigned short>(guard_region);r->guard_city=p.city;
                        r->guard_goal=static_cast<unsigned short>(guard_goal);r->guard_hex=static_cast<unsigned short>(h);
                        assigned=true;break;
                    }
                    if(++attempts==12)break;
                }
                if(!assigned)sortie_move(reinforcement,word(reinforcement,0x2A));
                log_event(assigned?"campaign_guard_departed":"campaign_guard_departure_no_route",army_id(army),army_id(reinforcement),guard_region);
            } else if(r && !campaign_individual_priority(reinforcement,*r))campaign_step(reinforcement,*r);
            campaign_reinforcement_source[word(source,0x4E)]={world(),day};
            ++dispatched;total+=word(reinforcement,0x16);
            log_event("campaign_reinforcement_departed",army_id(reinforcement),goal,word(source,0x4E));
            if(total>=needed || dispatched==4)break;
        }
        if(total>=needed || dispatched==4)break;
    }
    log_event(dispatched?"campaign_reinforcement_requested":"campaign_reinforcement_unavailable",army_id(army),dispatched,goal);
    return dispatched!=0;
}

static bool campaign_request_companion(Obj army,Progress &p,unsigned target) {
    if(!mod_features.support)return false;   // [combat] support
    if(!p.campaign || !hostile_city(army,p.city) || p.companion_parent)return false;
    if(p.companion_target && p.companion_target<=500) {
        Obj a=table(0x7E838,p.companion_target,500);Progress &m=progress[p.companion_target];
        if(campaign_controlled(a) && word(a,0x12)==p.companion_commander && m.companion_parent==army_id(army) &&
           m.region_target<48400 && campaign_region_hostile(a,campaign_area_id(m.region_target)))return true;
    }
    unsigned primary=campaign_area_id(target),secondary=0;int range=INT32_MAX;
    Obj area=table(0x6CAF0,primary,500);if(!valid(area))return false;
    for(unsigned i=0;i<8;++i) {
        unsigned id=word(area,0x56+i*2);if(!(area[0x66]&(1u<<i)) || !id || id>500 || campaign_transit_region(id) || !campaign_region_hostile(army,id))continue;
        unsigned h=word(table(0x6CAF0,id,500),0x3A);if(h>=48400 || region_reserved(h,army))continue;
        int d=distance(word(army,0x2A),h);if(d<range) {range=d;secondary=h;}
    }
    if(!secondary)return false;
    for(unsigned id=1;id<=51;++id) {
        Obj source=campaign_goal(id);if(!source || campaign_city_owner(source)!=army_force(army) || campaign::available(dword(source,0x3C))<1000 ||
           distance(fn<unsigned(__fastcall *)(Obj)>(0x22D740)(source),target)>60)continue;
        GameList people;list_init(people,0x13436B0);fn<void(__fastcall *)(Obj,GameList *)>(0x2D7180)(source,&people);
        fn<void(__fastcall *)(GameList *)>(0x2A0780)(&people);fn<void(__fastcall *)(GameList *,int,int,int)>(0xD32C0)(&people,0,1,0);
        std::vector<Obj> candidates;for(Link *l=list_first(people);l;l=l->next)if(sortie_auto_person(l->value) && !l->value[0x161])candidates.push_back(l->value);
        list_free(people);
        for(Obj person:candidates) {
            Obj district=table(0xE0D0,person[0xE0],51);unsigned f=sortie_capture_formation(person);
            if(!f || !valid(district) || !district[0x14] || fn<int(__fastcall *)(Obj)>(0x2311A0)(person)<1000)continue;
            bool deployed=false;for(unsigned j=1;j<=500;++j) {Obj a=table(0x7E838,j,500);if(valid(a) && word(a,0x12)==word(person,0x10)) {deployed=true;break;}}
            if(deployed)continue;
            unsigned command[30];sortie_command(nullptr,campaign_goal(p.city),person,f,1000,command);
            Obj land=table(0x77428,command[3],20),water=table(0x77428,command[4],20);
            if(!valid(land) || !valid(water) || person[0x161])continue;
            auto money=*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(source,0)+0x90);
            if(money(source)<word(land,0x9A)+word(water,0x9A))continue;
            Obj helper=sortie_issue(command,1);if(!valid(helper))continue;
            Obj group=table(0x7F8D8,word(helper,0x58),500);if(valid(group)) { *reinterpret_cast<unsigned short *>(group+0x14)=10;*reinterpret_cast<unsigned short *>(group+0x16)=p.city;}
            Progress *m=observe_capture(helper);if(!m)return false;
            m->siege_city=0;m->city=p.city;m->campaign=true;m->companion_parent=static_cast<unsigned short>(army_id(army));
            p.companion_target=static_cast<unsigned short>(army_id(helper));p.companion_commander=word(helper,0x12);
            if(!campaign_assign_safe(helper,*m,secondary,true)) {m->region_target=static_cast<unsigned short>(secondary);}
            log_event("campaign_capture_pair_departed",army_id(army),army_id(helper),secondary);return true;
        }
    }
    return false;
}
