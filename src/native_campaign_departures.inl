/* Periodic departures use the existing AI callback, native candidate list,
 * command preparation and paid dispatcher. No fabricated behavior node. */
struct CampaignDepartureAudit {Obj root;int day;};
static CampaignDepartureAudit campaign_departure_force[52],campaign_departure_city[52];
static Obj campaign_paid_single(Obj source,Obj target,unsigned hex,unsigned wanted,bool worker) {
    COST_SCOPE("campaign_paid_single");
    unsigned budget=campaign::available(dword(source,0x3C));if(budget<(worker?1000u:3000u))return nullptr;
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
        if(!worker && person[0xA9]==1)continue;
        bool deployed=false;for(unsigned i=1;i<=500;++i) {Obj a=table(0x7E838,i,500);
            if(valid(a) && word(a,0x12)==word(person,0x10)) {deployed=true;break;}}
        if(deployed)continue;
        Obj district=table(0xE0D0,person[0xE0],51);unsigned formation=worker?sortie_capture_formation(person):sortie_field_formation(person);
        int capacity=fn<int(__fastcall *)(Obj)>(0x2311A0)(person);
        if(!valid(district) || !district[0x14] || !formation || capacity<(worker?1000:3000))continue;
        unsigned soldiers=worker?1000:(std::min)(budget,(std::min)(wanted,static_cast<unsigned>(capacity)));
        unsigned cmd[30];sortie_command(nullptr,target,person,formation,soldiers,cmd);
        if(worker) {cmd[13]=1;cmd[14]=9;cmd[15]=hex;}
        Obj land=table(0x77428,cmd[3],20),water=table(0x77428,cmd[4],20);
        if(!valid(land) || !valid(water))continue;
        auto money=*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(source,0)+0x90);
        if(money(source)<word(land,0x9A)+word(water,0x9A))continue;
        unsigned free_armies=0,free_groups=0;
        for(unsigned i=1;i<=500;++i) {if(!valid(table(0x7E838,i,500)))++free_armies;if(!valid(table(0x7F8D8,i,500)))++free_groups;}
        if(!free_armies || !free_groups)return nullptr;
        Obj army=sortie_issue(cmd,1);if(!valid(army))continue;
        Obj group=table(0x7F8D8,word(army,0x58),500);
        if(valid(group)) { *reinterpret_cast<unsigned short *>(group+0x14)=10;
            *reinterpret_cast<unsigned short *>(group+0x16)=word(target,0x4E);}
        Progress *p=observe_capture(army);if(!p)return army;
        if(worker) {
            p->siege_city=0;p->campaign=false;p->regional_worker=true;p->work_source=word(source,0x4E);
            p->region_target=static_cast<unsigned short>(hex);
            campaign_assign_safe(army,*p,hex,true);
            log_event("home_capture_worker_departed",army_id(army),1000,hex);
        } else {
            p->campaign=true;p->city=word(target,0x4E);p->siege_city=p->city;
            offensive_order(army,p->city,true);fn<void(__fastcall *)(Obj)>(0x26C940)(army);
            log_event("weak_city_single_departed",army_id(army),word(army,0x16),p->city);
        }
        return army;
    }
    return nullptr;
}
static bool campaign_city_connected(Obj source,Obj target,unsigned force) {
    unsigned from=campaign_area_id(fn<unsigned(__fastcall *)(Obj)>(0x22D740)(source));
    unsigned to=campaign_area_id(fn<unsigned(__fastcall *)(Obj)>(0x22D740)(target));
    unsigned enemy=campaign_city_owner(target);if(!from || !to)return false;
    bool seen[501]={};std::vector<unsigned> queue={from};seen[from]=true;
    for(size_t k=0;k<queue.size();++k) {
        if(queue[k]==to)return true;Obj a=table(0x6CAF0,queue[k],500);if(!valid(a))continue;
        for(unsigned i=0;i<8;++i) {
            unsigned n=word(a,0x56+i*2);if(!(a[0x66]&(1u<<i)) || !n || n>500 || seen[n])continue;
            Obj b=table(0x6CAF0,n,500);if(!valid(b))continue;Obj t=table(0xE270,word(b,0x3A),48399);
            if(!valid(t) || (t[0x14] && t[0x14]!=force && t[0x14]!=enemy))continue;
            seen[n]=true;queue.push_back(n);
        }
    }
    return false;
}
// mission_v2 (user decision 2026-10-21, option A): a city holding 90%+ of its maximum garrison and
// 40,000+ spare soldiers sends the mod's front/middle/rear siege formation to the nearest hostile
// city linked by region adjacency. Everything else stays with the original departure judgment.
#ifndef NO_RUNTIME_LOG
// Diagnostic build only, read-only: every 30 days, each owned city's non-owned neighbour regions with
// the facts the original corps occupation filter uses (corps tree N201/N2218-N2228): neighbour owner,
// port (232BF0) / base core (232680), and its FIRST neighbour's port/core status and ownership.
// value = area | owner<<9 | port<<15 | core<<16 | first_special<<17 | first_neutral<<18; hex = centre.
static void campaign_frontier_diagnostic() {
    static volatile LONG last_day=-1000;int day=game_day();
    if(!diagnostic_logging || day-last_day<30 || day<last_day+0)return;
    if(InterlockedExchange(&last_day,day)==day)return;
    auto owner_of=[](Obj area)->unsigned {Obj t=valid(area)?table(0xE270,word(area,0x3A),48399):nullptr;return valid(t)?t[0x14]:255;};
    for(unsigned id=1;id<=500;++id) {
        Obj a=table(0x6CAF0,id,500);if(!valid(a))continue;
        unsigned own=owner_of(a);if(!own || own>51)continue;
        for(unsigned e=0;e<8;++e) {
            unsigned n=word(a,0x56+e*2);if(!(a[0x66]&(1u<<e)) || !n || n>500)continue;
            Obj b=table(0x6CAF0,n,500);if(!valid(b))continue;
            unsigned other=owner_of(b);if(other==own)continue;
            unsigned port=fn<int(__fastcall *)(Obj)>(0x232BF0)(b)?1:0,core=fn<int(__fastcall *)(Obj)>(0x232680)(b)?1:0;
            unsigned special=0,neutral=0;
            for(unsigned f=0;f<8;++f) {unsigned m=word(b,0x56+f*2);if(!(b[0x66]&(1u<<f)) || !m || m>500)continue;
                Obj c=table(0x6CAF0,m,500);if(!valid(c))break;
                special=(fn<int(__fastcall *)(Obj)>(0x232BF0)(c) || fn<int(__fastcall *)(Obj)>(0x232680)(c))?1:0;
                neutral=owner_of(c)==0?1:0;break;}
            log_event("frontier_area",a[0x35],n|(other<<9)|(port<<15)|(core<<16)|(special<<17)|(neutral<<18),word(b,0x3A));
        }
    }
}
#else
static void campaign_frontier_diagnostic() {}
#endif
static void campaign_saturated_departures(Obj actor) {
    COST_SCOPE("campaign_saturated_departures");
    if(!mod_features.mission_v2 || !mod_features.sortie_formation)return;
    unsigned force=army_force(actor);int day=game_day();if(!force || force>51 || !campaign_controlled(actor))return;
    auto &audit=campaign_departure_force[force];
    if(audit.root==world() && day>=audit.day && day-audit.day<10)return;
    audit={world(),day};
    for(unsigned id=1;id<=51;++id) {
        Obj source=campaign_goal(id);if(!source || campaign_city_owner(source)!=force)continue;
        unsigned stock=dword(source,0x3C);
        int maximum=(*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(source,0)+200))(source);
        if(maximum<=0 || static_cast<uint64_t>(stock)*10<static_cast<uint64_t>(maximum)*9 ||
           campaign::available(stock)<sortie_split_threshold)continue;
        auto &used=campaign_departure_city[id];if(used.root==world() && day>=used.day && day-used.day<30)continue;
        Obj target=nullptr;int best=INT32_MAX;unsigned source_hex=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(source);
        for(unsigned city=1;city<=51;++city) {
            Obj g=campaign_goal(city);if(!g || !hostile_city(actor,city))continue;
            int range=distance(source_hex,fn<unsigned(__fastcall *)(Obj)>(0x22D740)(g));
            if(range>60 || !campaign_city_connected(source,g,force))continue;
            uint64_t incoming=0;for(unsigned i=1;i<=500;++i) {
                Obj a=table(0x7E838,i,500);if(!valid(a) || army_force(a)!=force || a[0x37]==11 || a[0x37]==12)continue;
                Obj group=table(0x7F8D8,word(a,0x58),500);
                if(valid(group) && attack_mode(word(group,0x14)) && word(group,0x16)==city)incoming+=word(a,0x16);
            }
            if(incoming>=(std::max)(3000u,dword(g,0x3C)*3/2))continue;
            if(range<best) {best=range;target=g;}
        }
        if(!target)continue;
        used={world(),day};
        bool sent=sortie_departure_from(nullptr,target,{source},campaign::available(stock));
        log_event(sent?"saturated_city_departed":"saturated_city_unavailable",id,stock,word(target,0x4E));
    }
}
static void campaign_proactive_departures(Obj actor) {
    COST_SCOPE("campaign_proactive_departures");
    if(!mod_features.departures)return;   // [combat] departures
    unsigned force=army_force(actor);int day=game_day();if(!force || force>51 || !campaign_controlled(actor))return;
    auto &audit=campaign_departure_force[force];
    if(audit.root==world() && day>=audit.day && day-audit.day<10)return;
    audit={world(),day};
    for(unsigned id=1;id<=51;++id) {
        Obj source=campaign_goal(id);if(!source || campaign_city_owner(source)!=force || campaign::available(dword(source,0x3C))<1000)continue;
        auto &used=campaign_departure_city[id];if(used.root==world() && day>=used.day && day-used.day<10)continue;
        used={world(),day};
        // Up to two independent 1,000-soldier workers per city per turn.
        for(unsigned n=0;n<2;++n) {
            auto bases=campaign_home_bases(actor,id);if(bases.empty())break;
            if(!campaign_paid_single(source,source,bases[0],1000,true))break;
        }
        Obj target=nullptr;int score=INT32_MAX;unsigned source_hex=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(source);
        for(unsigned city=1;city<=51;++city) {
            Obj g=campaign_goal(city);if(!g || !hostile_city(actor,city))continue;
            unsigned budget=campaign::available(dword(source,0x3C));bool weak=dword(g,0x3C)<10000;
            if(budget<(weak?3000u:20000u))continue;
            if(!weak && !campaign::siege_departure(budget,dword(g,0x3C)))continue;
            int range=distance(source_hex,fn<unsigned(__fastcall *)(Obj)>(0x22D740)(g));
            if(range>60 || !campaign_city_connected(source,g,force))continue;
            uint64_t incoming=0;for(unsigned i=1;i<=500;++i) {
                Obj a=table(0x7E838,i,500);if(!valid(a) || army_force(a)!=force || a[0x37]==11 || a[0x37]==12)continue;
                Obj group=table(0x7F8D8,word(a,0x58),500);
                if(valid(group) && attack_mode(word(group,0x14)) && word(group,0x16)==city)incoming+=word(a,0x16);
            }
            if(incoming>=(std::max)(3000u,dword(g,0x3C)*3/2))continue;
            int value=range+(weak?0:100);if(value<score) {score=value;target=g;}
        }
        if(!target)continue;
        if(campaign::siege_departure(campaign::available(dword(source,0x3C)),dword(target,0x3C)))sortie_departure_from(nullptr,target,{source});
        else if(dword(target,0x3C)<10000)campaign_paid_single(source,target,48400,
            (std::max)(3000u,dword(target,0x3C)*3/2),false);
        else sortie_departure_from(nullptr,target,{source});
    }
}
