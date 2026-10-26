static std::vector<Obj> sortie_people;
static unsigned sortie_issued,sortie_cancelled,sortie_deleted,sortie_skills,sortie_fail_at;
static Obj sortie_city,sortie_district;
static unsigned sortie_last_command[30];
static unsigned sortie_blocked_target=48400;
static unsigned sortie_search_calls;
static unsigned sortie_mock_gate_override=~0u;
static void __fastcall sortie_mock_people(Obj,GameList *out) {for(Obj p:sortie_people)list_add(*out,p);}
static void __fastcall sortie_mock_filter(GameList *) {}
static void __fastcall sortie_mock_filter2(GameList *,int,int,int) {}
static Obj __fastcall sortie_mock_manual(Obj) {return district;}
static int __fastcall sortie_mock_capacity(Obj p) {return static_cast<int>(dword(p,0x1D0));}
static int __fastcall sortie_mock_ability(Obj p,unsigned,unsigned) {return p[0xEC];}
static int __fastcall sortie_mock_owned(Obj p,unsigned f) {return (dword(p,0x12C)&(1u<<f))!=0;}
static void __fastcall sortie_mock_defaults(Obj,Obj target,Obj person,unsigned *cmd,unsigned troops) {
    bool gate=word(target,0x4E)>51;
    cmd[0]=word(person,0x10);cmd[2]=troops;cmd[3]=gate && (dword(person,0x12C)&(1u<<9))?9:1;
    if(gate && sortie_mock_gate_override!=~0u)cmd[3]=sortie_mock_gate_override;
    cmd[4]=11;cmd[13]=gate?5:4;cmd[14]=gate?6:5;cmd[15]=word(target,0x4E);
}
static void __fastcall sortie_mock_skills(Obj,Obj f,unsigned *cmd) {++sortie_skills;cmd[5]=word(f,0x10);}
static Obj __fastcall sortie_mock_dispatch(unsigned *cmd,int spend) {
    memcpy(sortie_last_command,cmd,sizeof(sortie_last_command));
    if(spend!=1)abort();
    if(sortie_fail_at && sortie_issued+1==sortie_fail_at)return nullptr;
    unsigned n=sortie_issued++,id=60+n;
    Obj army=object(1);army_ids[army]=id;army[0x1A]=100;slot(0x7E838,id,army);army[0x150]=2;
    put16(army,0x12,cmd[0]);put16(army,0x16,cmd[2]);army[0x1B]=static_cast<unsigned char>(cmd[3]);
    put16(army,0x58,100+n);put16(army,0x2A,22050);put16(army,0x48,48400);
    Obj t=object(1);slot(0x7F8D8,100+n,t);
    *reinterpret_cast<unsigned *>(sortie_city+0x3C)-=cmd[2];--sortie_district[0x14];
    *reinterpret_cast<float *>(army+0x180)=10.0f-static_cast<float>(n);
    return army;
}
static void __fastcall sortie_mock_delete(Obj t) {++sortie_deleted;put16(t,0x10,0);}
static void __fastcall sortie_mock_cancel(Obj t) {
    ++sortie_cancelled;
    for(unsigned i=1;i<=500;++i) {Obj a=table(0x7E838,i,500);if(valid(a) && belongs_to_troops(a,t)) {
        *reinterpret_cast<unsigned *>(sortie_city+0x3C)+=word(a,0x16);put16(a,0x10,0);
    }}
    put16(t,0x10,0);
}
static Obj __fastcall sortie_mock_group(GameList *list) {
    Obj group=object(1);slot(0x7F8D8,150,group);
    for(Link *l=list_first(*list);l;l=l->next)put16(l->value,0x58,150);
    return group;
}
static Obj __fastcall sortie_mock_origin(Obj) {return sortie_city;}
static void __fastcall sortie_mock_join(Obj army,unsigned group) {put16(army,0x58,group);}
static unsigned __fastcall sortie_mock_goal_hex(Obj) {return 25350;}
static float __fastcall sortie_mock_power(Obj army) {return static_cast<float>(word(army,0x16));}
static float __fastcall sortie_mock_speed(Obj army,Obj,int,int,Obj) {return *reinterpret_cast<float *>(army+0x180);}
static Obj __fastcall sortie_mock_formation(Obj army) {return table(0x77428,army[0x1B],20);}
static int sortie_hex_distance(int a,int b) {
    int ax=a%220,ay=a/220-(ax+(ax&1))/2,bx=b%220,by=b/220-(bx+(bx&1))/2;
    int dx=ax-bx,dy=ay-by;return (abs(dx)+abs(dy)+abs(dx+dy))/2;
}
static int __fastcall sortie_mock_search(SortieNativePath *out,Obj,unsigned from,unsigned *to,int,int,int) {
    ++sortie_search_calls;
    static int cells[48400];out->data=cells;out->count=0;out->capacity=48400;
    if(!path_available || *to==sortie_blocked_target)return 0;
    cells[out->count++]=static_cast<int>(from);
    while(from!=*to && out->count<48400) {
        unsigned best=48400;int range=sortie_hex_distance(from,*to);
        for(unsigned d=0;d<6;++d) {
            unsigned h=sortie::neighbor(from,d);int candidate=sortie_hex_distance(h,*to);
            if(h<48400 && candidate<range) {range=candidate;best=h;}
        }
        if(best>=48400)break;cells[out->count++]=static_cast<int>(best);from=best;
    }
    return out->count>1;
}
static void __fastcall sortie_mock_path(Obj army) {
    unsigned from=word(army,0x2A),to=word(army,0x2C);
    if(to>=48400)to=word(army,0x3A);
    SortieNativePath out={};int result=sortie_search(&out,nullptr,from,&to,1,1,0);
    put16(army,0x48,result && out.count>1?static_cast<unsigned>(out.data[1]):48400);
}
#include "campaign_reinforcement_fixture.inl"
#include "campaign_departure_fixture.inl"
static void sortie_tests() {
    for(unsigned x:{0u,1u,100u,101u,218u,219u})for(unsigned y:{0u,1u,100u,218u,219u})
        for(unsigned d=0;d<6;++d) {
            unsigned h=y*220+x,n=sortie::neighbor(h,d);
            check(n==48400 || sortie::neighbor(n,(d+3)%6)==h,"even/odd column and edge neighbor reciprocity");
        }
    sortie::Officer facts[7];unsigned selected[7],capacity[7],allocation[7];
    sortie::Officer swift_pair[2]={{1,1u<<7,8000,0,90},{2,1u<<7,8000,0,80}};
    check(sortie::roster(swift_pair,2,selected,16000) && selected[0]<2 && selected[6]<2 &&
          sortie::formation(swift_pair[selected[0]],0)==7 && sortie::formation(swift_pair[selected[6]],6)==7,
        "formation-seven-only officers can fill front and center seats without exclusion");
    for(unsigned i=0;i<7;++i) {facts[i]={i+1,0x1E,10000,i==3?3u:0u,80};capacity[i]=10000;}
    check(sortie::roster(facts,7,selected),"overlapping role masks produce unique seven officer matching");
    bool used[7]={};for(unsigned i=0;i<7;++i) {check(!used[selected[i]],"commander cannot occupy two seats");used[selected[i]]=true;}
    check(sortie::allocate(capacity,40000,allocation),"40k boundary can be split");
    unsigned sum=0;for(unsigned i=0;i<7;++i) {sum+=allocation[i];check(allocation[i]>=(i==3?1000u:sortie::role(i)==0?3000u:1500u) && allocation[i]<=capacity[i],"native capacity respected");}
    check(allocation[3]==1000,"support receives exactly one thousand soldiers");
    check(sum==40000 && allocation[0]>allocation[6],"soldiers conserved with heavier front line");
    check(sortie::allocate(capacity,39999,allocation),"39999 mobilizes without a forty thousand cliff");
    for(unsigned i=0;i<7;++i)capacity[i]=i==0 || i==1 || i==6?20000:0;
    check(sortie::allocate(capacity,40000,allocation) && allocation[3]==0,"three unit cohort does not require support or seven units");
    sortie::Officer pair[2]={{1,(1u<<1)|(1u<<4),20000,0,90},{2,(1u<<1)|(1u<<4),20000,0,80}};
    check(sortie::roster(pair,2,selected) && selected[0]<2 && selected[6]<2 && sortie::formation(pair[selected[6]],6)==4,
        "two twenty thousand defensive units can move together without a mandatory ranged or seventh unit");
    check(sortie::roster(facts,7,selected,12000),"twelve thousand selects a smaller roster");
    unsigned seats=0;for(unsigned i=0;i<7;++i) {capacity[i]=selected[i]<7?10000:0;seats+=capacity[i]!=0;}
    check(seats==4 && selected[3]<7 && selected[2]==128 && selected[4]==128 && selected[5]==128,
        "small army retains two front, one middle and one support instead of seven underfilled units");
    check(sortie::allocate(capacity,12000,allocation),"small army allocation remains valid");
    sum=0;for(unsigned i=0;i<7;++i)sum+=allocation[i];
    check(sum==12000 && allocation[3]==1000 && allocation[0]>allocation[6],"small budget conserves soldiers and protects a lighter rear");
    for(auto &p:facts)p.support=0;
    check(sortie::roster(facts,7,selected) && selected[3]==128,"missing female support still forms a field cohort");
    BIND(0x32E8D0,sortie_mock_manual);BIND(0x2D7180,sortie_mock_people);BIND(0x2A0780,sortie_mock_filter);
    BIND(0xD32C0,sortie_mock_filter2);BIND(0x2311A0,sortie_mock_capacity);BIND(0x22FCE0,sortie_mock_ability);
    BIND(0x232350,sortie_mock_owned);BIND(0x42E70,sortie_mock_defaults);BIND(0xD0090,sortie_mock_skills);
    BIND(0x282540,sortie_mock_delete);BIND(0x2D9260,sortie_mock_group);BIND(0x9E900,sortie_mock_cancel);
    BIND(0xB5EC0,sortie_mock_power);
    original_sortie_dispatch=sortie_mock_dispatch;original_sortie_movement=sortie_mock_speed;original_sortie_search=sortie_mock_search;
    {NativePhase dispatch_phase("sortie_dispatch");SortieNativePath path={};unsigned target=48400;
        unsigned calls=sortie_search_calls;
        check(!sortie_search(&path,nullptr,22050,&target,1,1,2) && sortie_search_calls==calls,
            "native dispatch rejects no-destination sentinel before touching the native search buffer");
        target=22051;
        check(!sortie_search(&path,nullptr,48400,&target,1,1,2) && sortie_search_calls==calls,
            "native dispatch rejects invalid source tile before native search");
        sortie_search(&path,nullptr,22050,&target,1,1,2);
        check(sortie_search_calls==calls+1,"valid native departure path still reaches the original function");
    }
    sortie_city=object(2);putptr(sortie_city,0,pointer(origin,0));put16(sortie_city,0x4E,2);
    sortie_district=object(2);sortie_district[0x14]=20;slot(0xE0D0,2,sortie_district);
    Obj node=object(1);*reinterpret_cast<unsigned *>(node+0x88)=6000;*reinterpret_cast<unsigned *>(node+0x4C)=8;
    *reinterpret_cast<int *>(node+0x84)=1;putptr(node,0x60,goal);put16(goal,0x4E,1);
    GameList sources,outputs;list_init(sources);list_init(outputs);list_add(sources,sortie_city);
    putptr(node,0x78,&sources);memcpy(node+0x98,&outputs,sizeof(outputs));
    for(unsigned f=1;f<=14;++f)slot(0x77428,f,object(f));
    for(unsigned i=0;i<7;++i) {
        Obj p=object(200+i);p[0xE0]=2;p[0xEC]=80;*reinterpret_cast<unsigned *>(p+0x12C)=0x1E;
        *reinterpret_cast<unsigned *>(p+0x1D0)=10000;slot(0x158,200+i,p);sortie_people.push_back(p);
    }
    Obj support=sortie_people[3];support[0xA9]=1;put16(support,0x118,23);put16(support,0x11A,94);put16(support,0x11C,96);
    check(sortie_support(support)==3,"three support traits and native female field decoded");
    support[0xA9]=0;check(sortie_support(support)==0,"male traits do not fill the female support seat");support[0xA9]=1;
    campaign_reinforcement_tests();
    campaign_departure_tests();
    BIND(0x22C990,reinforcement_owner);
    *reinterpret_cast<unsigned *>(sortie_city+0x3C)=4499;
    check(!sortie_departure(node) && sortie_issued==0,"under minimum deployable roster retains native departure");
    *reinterpret_cast<unsigned *>(sortie_city+0x3C)=15000;
    check(!sortie_departure(node) && sortie_issued==0 && dword(sortie_city,0x3C)==15000,
        "less than twenty thousand available waits before forming a siege cohort");
    for(unsigned i=0;i<7;++i)slot(0x7E838,60+i,nullptr);sortie_issued=0;sortie_district[0x14]=20;
    *reinterpret_cast<unsigned *>(sortie_city+0x3C)=46000;
    putptr(node,0x60,sortie_city);
    check(!sortie_departure(node) && sortie_issued==0 && dword(sortie_city,0x3C)==46000 && sortie_district[0x14]==20,
        "friendly relief target cannot trigger the custom offensive cohort or spend resources");
    putptr(node,0x60,goal);
    support[0x161]=1;
    check(sortie_departure(node) && sortie_issued==6,"wounded support excluded while six units can mobilize 40k");
    SortieGroup six=sortie_groups[150];check(six.army[3]==nullptr && six.army[6] && six.army[0],"variable roster tracks actual seats");
    check(dword(sortie_city,0x3C)==4000,"cohort retains exactly four thousand city soldiers");
    for(Obj a:six.army)if(a)check(word(a,0x12)!=203,"injured commander never dispatched");
    sortie_mock_cancel(six.group);sortie_groups[150]={};
    for(unsigned i=0;i<7;++i)slot(0x7E838,60+i,nullptr);
    sortie_issued=0;support[0x161]=0;sortie_district[0x14]=1;
    check(!sortie_departure(node) && sortie_issued==0,"fewer than two command points falls through before mutation");
    sortie_district[0x14]=6;
    check(sortie_departure(node) && sortie_issued==6,"six command points selects an affordable six-unit roster instead of rejecting the whole army");
    SortieGroup affordable=sortie_groups[150];sortie_mock_cancel(affordable.group);sortie_groups[150]={};
    for(unsigned i=0;i<7;++i)slot(0x7E838,60+i,nullptr);sortie_issued=0;
    sortie_district[0x14]=20;money=0;
    for(unsigned f=1;f<=14;++f)put16(table(0x77428,f,20),0x9A,100);
    check(!sortie_departure(node),"aggregate formation costs checked before creating first unit");money=10000;
    sortie_fail_at=3;unsigned cancellations=sortie_cancelled;
    check(!sortie_departure(node) && sortie_issued==2 && sortie_cancelled==cancellations+2,"partial creation cancels native temporary groups");
    check(dword(sortie_city,0x3C)==46000,"partial cancellation returns actual soldiers");
    sortie_fail_at=0;sortie_issued=0;sortie_district[0x14]=20;
    for(unsigned i=0;i<7;++i)slot(0x7E838,60+i,nullptr);
    check(sortie_departure(node) && sortie_issued==7,"native creation and group registration for seven eligible officers");
    SortieGroup g=sortie_groups[150];check(g.commander[3]==203 && word(g.group,0x14)==8 && word(g.group,0x16)==1,"female support role and strategic campaign retained");
    check(sortie_member(g.army[6]),"delegated district is eligible even if its force is player owned");
    Obj p=table(0x158,word(g.army[6],0x12),6000);unsigned char saved_d=p[0xE0];p[0xE0]=1;slot(0xE0D0,1,district);district[0x14]=20;
    check(!sortie_member(g.army[6]),"direct player district cannot be controlled");p[0xE0]=saved_d;
    BIND(0x22B090,sortie_mock_formation);
    float slowest=sortie_mock_speed(g.army[6],nullptr,0,0,nullptr);
    for(Obj a:g.army)if(a && sortie_mock_speed(a,nullptr,0,0,nullptr)<slowest)slowest=sortie_mock_speed(a,nullptr,0,0,nullptr);
    check(sortie_movement(g.army[0],nullptr,0,0,nullptr)==slowest,"movement reduced to actual cohort minimum without boosting slow units");
    check(sortie_movement(g.army[0],nullptr,1,0,nullptr)==slowest,"preview and actual group movement share the same common speed");
    unsigned command[30]={};command[0]=200;command[2]=3000;command[3]=9;command[4]=11;
    sortie_planner_node=node;unsigned saved_stock=dword(sortie_city,0x3C);
    sortie_dispatch(command,1);check(!sortie::siege(sortie_last_command[3]) && command[3]==9,"siege demotion rebuilds skills on a private command copy");
    command[2]=20000;
    put16(goal,0x4E,52);sortie_dispatch(command,1);
    check(sortie_last_command[3]==9 && sortie_last_command[2]==8000 && command[2]==20000,"final gate retains native siege choice with eight thousand limit and immutable input");
    put16(goal,0x4E,1);sortie_planner_node=nullptr;sortie_people[0][0xE0]=1;sortie_dispatch(command,1);sortie_people[0][0xE0]=2;check(sortie_last_command[3]==9,"direct player district dispatcher is untouched");
    check(sortie_last_command[2]==20000,"direct human command keeps native siege soldiers");
    {   // sortie_plan dry run (2026-10-21): native dispatches are counted and refused, nothing is issued.
        unsigned probe_command[30]={};probe_command[0]=200;probe_command[2]=12000;probe_command[3]=1;
        memset(sortie_last_command,0,sizeof(sortie_last_command));
        SortieProbe probe={};sortie_probe=&probe;
        Obj first=sortie_dispatch(probe_command,1),second=sortie_dispatch(probe_command,1);
        sortie_people[0][0xE0]=1;sortie_dispatch(probe_command,1);sortie_people[0][0xE0]=2;sortie_probe=nullptr;
        check(!first && !second && probe.calls==3 && probe.soldiers==36000 && probe.foreign && sortie_last_command[2]==0,
            "planner dry run records native dispatches (and a player-corps officer) without issuing any army");
    }
    sortie_auto_departure_scope=1;command[14]=6;sortie_dispatch(command,1);
    check(sortie_last_command[2]==8000 && sortie_last_command[3]==9,"ordinary automatic command scope also caps gate siege soldiers");
    sortie_auto_departure_scope=0;
    *reinterpret_cast<unsigned *>(sortie_city+0x3C)=30000;
    unsigned enemy_stock=dword(goal,0x3C);*reinterpret_cast<unsigned *>(goal+0x3C)=9999;
    command[13]=4;command[14]=5;command[15]=1;sortie_planner_node=node;
    sortie_dispatch(command,1);
    check(!sortie::siege(sortie_last_command[3]),"a weak ordinary castle cannot bypass field-formation priority");
    *reinterpret_cast<unsigned *>(sortie_city+0x3C)=30000;
    unsigned field_mask=dword(sortie_people[0],0x12C);*reinterpret_cast<unsigned *>(sortie_people[0]+0x12C)=1u<<9;
    sortie_dispatch(command,1);
    check(sortie_last_command[3]==9 && sortie_last_command[2]==8000,"siege-only commander against a weak castle still obeys eight thousand cap");
    *reinterpret_cast<unsigned *>(sortie_people[0]+0x12C)=field_mask;*reinterpret_cast<unsigned *>(goal+0x3C)=enemy_stock;sortie_planner_node=nullptr;
    *reinterpret_cast<unsigned *>(sortie_city+0x3C)=saved_stock;
    unsigned intercept_before=sortie_issued,intercept_stock=dword(sortie_city,0x3C);unsigned char intercept_points=sortie_district[0x14];
    command[3]=7;command[2]=8000;command[13]=7;command[14]=27;command[15]=1;
    sortie_dispatch(command,1);
    check(sortie_last_command[3]==7 && sortie_last_command[2]==8000 && sortie_last_command[13]==7 && sortie_last_command[14]==27 && command[3]==7,
        "automatic native interception retains formation seven and its ordinary troop count");
    *reinterpret_cast<unsigned *>(sortie_people[0]+0x12C)=1u<<7;unsigned no_field_issued=sortie_issued;
    check(valid(sortie_dispatch(command,1)) && sortie_issued==no_field_issued+1,"formation-seven-only officer is no longer barred from a native interceptor departure");
    *reinterpret_cast<unsigned *>(sortie_people[0]+0x12C)=field_mask;
    sortie_people[0][0xE0]=1;sortie_dispatch(command,1);sortie_people[0][0xE0]=2;
    check(sortie_last_command[3]==7,"player direct formation-seven orders retain native behavior");
    for(unsigned n=intercept_before;n<sortie_issued;++n) {slot(0x7E838,60+n,nullptr);slot(0x7F8D8,100+n,nullptr);}
    sortie_issued=intercept_before;*reinterpret_cast<unsigned *>(sortie_city+0x3C)=intercept_stock;sortie_district[0x14]=intercept_points;
    for(unsigned i=67;i<80;++i)slot(0x7E838,i,nullptr);
    // Native order generator, geometry and cadence; fail one path after other
    // seats have been planned to verify all-or-nothing order restoration.
    BIND(0x2AA850,sortie_hex_distance);BIND(0x26C940,sortie_mock_path);
    Obj corridor_area=object(10);put16(corridor_area,0x3A,22050);slot(0x6CAF0,10,corridor_area);
    for(unsigned y=96;y<=125;++y)for(unsigned x=44;x<=58;++x) {
        Obj t=object(y*220+x);t[0x14]=2;put16(t,0x12,10);tile_areas[t]=corridor_area;slot(0xE270,y*220+x,t);
    }
    g.center=22050;g.facing=0;
    *reinterpret_cast<float *>(g.army[6]+0x180)=1.25f;
    check(sortie_movement(g.army[0],nullptr,0,0,nullptr)==1.25f,"actual movement adapts to a newly slower member rather than a fixed tile budget");
    for(unsigned i=0;i<7;++i)if(g.army[i])put16(g.army[i],0x2A,sortie::slot_hex(g.center,i,g.facing));
    check(sortie_place(g,sortie::neighbor(g.center,0),0),"one common center generates distinct front rear and support destinations");
    unsigned old_target=word(g.army[0],0x3A);path_available=false;
    check(!sortie_place(g,sortie::neighbor(g.center,2),2) && word(g.army[0],0x3A)==old_target,"failed member route restores every native order");path_available=true;
    sortie_district[0x14]=0;
    check(sortie_valid_group(g),"running out of city command points does not dissolve an existing army");
    unsigned char saved_formation=g.army[0][0x1B];g.army[0][0x1B]=7;
    check(sortie_valid_group(g),"formation seven remains eligible in a running siege cohort");g.army[0][0x1B]=saved_formation;
    g.army[0][0x37]=13;check(sortie_valid_group(g),"native idle after waypoint arrival does not dissolve the formation");g.army[0][0x37]=1;
    p[0x161]=1;check(sortie_valid_group(g),"already deployed injured commander is not removed from a cohort");p[0x161]=0;
    sortie_district[0x14]=20;
    BIND(0x22D740,sortie_mock_goal_hex);fixture_day=100;
    // An outward march uses one full native destination; only the final
    // adjacent base activates the per-hex formation behavior tested below.
    Obj castle_area=object(11);put16(castle_area,0x3A,25350);castle_area[0x34]=1;
    put16(castle_area,0x56,10);castle_area[0x66]=1;slot(0x6CAF0,11,castle_area);
    put16(corridor_area,0x56,11);corridor_area[0x66]=1;
    Obj castle_tile=table(0xE270,25350,48399);put16(castle_tile,0x12,11);
    put16(corridor_area,0x3A,22054);
    for(Obj actor:g.army)if(actor)put16(actor,0x2A,22050);
    g.rallied=false;g.day=-1;g.last_motion=100;g.motion_hex=22050;g.route_count=0;sortie_commit(150,g);
    check(campaign_group_marching(g.army[6]),"departure cannot build before reaching a castle-adjacent base");
    check(sortie_step(g.army[6]) && word(g.army[6],0x3A)==22054 &&
        sortie_groups[150].route_count==0 && distance(word(g.army[6],0x2A),word(g.army[6],0x3A))>1,
        "outward cohort moves to the full adjacent-base destination without a first-hex barrier");
    put16(g.army[6],0x2A,22054);
    check(!sortie_rally_arrived(sortie_groups[150]),"leader arrival alone cannot authorize construction while escorts are far behind");
    for(unsigned i=0;i<7;++i)if(g.army[i])put16(g.army[i],0x2A,sortie::slot_hex(22054,i,0));
    check(sortie_rally_arrived(sortie_groups[150]) && !campaign_group_marching(g.army[6]),
        "whole cohort at an owned castle-adjacent base can start its shared construction phase");
    put16(corridor_area,0x3A,22050);put16(castle_tile,0x12,10);slot(0x6CAF0,11,nullptr);
    corridor_area[0x66]=0;
    g.rallied=true;g.center=22050;
    Progress *march_lead=observe_capture(g.army[6]);march_lead->region_target=48400;
    // No building opportunity in this fixture area; region candidates empty.
    region_tiles.clear();put16(area,0x3A,100);table(0xE270,100,48399)[0x14]=3;
    cap_reached=true;legal_facilities=0;
    for(unsigned i=0;i<7;++i)if(g.army[i])put16(g.army[i],0x2A,sortie::slot_hex(g.center,i,g.facing));
    g.facing=sortie_facing(g.center,sortie_mock_goal_hex(goal));
    for(unsigned i=0;i<7;++i)if(g.army[i])put16(g.army[i],0x2A,sortie::slot_hex(g.center,i,g.facing));
    g.steps=0;g.route_count=0;g.day=-1;g.last_motion=100;g.motion_hex=g.center;g.demolisher=0;
    sortie_commit(150,g);
    check(sortie_step(g.army[6]),"cohort native AI callback creates a waypoint chain");g=sortie_groups[150];
    check(g.route_count==2 && g.steps==1,"shared native march takes one adjacent hex and tracks every role seat");
    check(sortie_hex_distance(g.route[0],g.route[1])==1,"common path begins with exactly one native adjacent hex");
    SortieGroup original_march=g;
    unsigned blocked_hex=sortie::slot_hex(g.center,0,g.facing);Obj blocked_tile=table(0xE270,blocked_hex,48399);
    sortie_blocked_target=blocked_hex;SortieGroup narrow=g;narrow.center=narrow.route[0];narrow.route_count=0;
    check(sortie_advance(narrow,narrow.destination) && narrow.steps>g.steps,
        "unavailable rear ring tile uses a forward column instead of freezing the whole formation");
    sortie_blocked_target=48400;(void)blocked_tile;
    // All seven units may depart from the castle tile together, without
    // first occupying seven exact ring offsets. Turning also travels forward.
    for(Obj a:g.army)if(a)put16(a,0x2A,22050);
    g.center=22050;g.route_count=0;g.day=-1;g.facing=(g.facing+3)%6;g.steps=0;
    sortie_commit(150,g);
    check(sortie_step(g.army[6]) && sortie_groups[150].steps==1 && sortie_groups[150].center!=22050,
        "unassembled seven-unit castle departure moves forward while changing facing");
    g=original_march;sortie_commit(150,g);
    for(unsigned i=0;i<7;++i)if(g.army[i])put16(g.army[i],0x2A,sortie::slot_hex(g.route[0],i,g.facing));
    // Arrival tolerance is one native turn, not exact waypoint equality.
    for(unsigned i=0;i<7;++i)if(g.army[i])put16(g.army[i],0x2A,sortie::slot_hex(g.center,i,g.facing));
    unsigned tail=sortie::slot_hex(g.center,3,g.facing);
    for(unsigned k=0;k<3;++k)tail=sortie::neighbor(tail,(g.facing+3)%6);
    put16(g.army[3],0x2A,tail);
    check(!sortie_settled(g),"shared next hex waits for its assigned rear seat to arrive");
    for(unsigned k=0;k<18;++k)tail=sortie::neighbor(tail,(g.facing+3)%6);
    put16(g.army[3],0x2A,tail);
    check(!sortie_settled(g),"a rear more than one native turn behind prevents unbounded formation separation");
    for(unsigned i=0;i<7;++i)if(g.army[i])put16(g.army[i],0x2A,sortie::slot_hex(g.route[0],i,g.facing));
    check(sortie_route_orders(g),"original spaced order is restored after departure and lag experiments");

    for(unsigned slot=0;slot<7;++slot)if(g.army[slot]) {
        unsigned previous=word(g.army[slot],0x2A);
        for(unsigned k=0;k+1<g.route_count;++k) {
            unsigned relay=word(g.army[slot],0x3A);
            check(relay==sortie::slot_hex(g.route[k+1],slot,g.facing) && sortie_hex_distance(previous,relay)<=1,
                "each role receives the next shared center or holds its current seat");
            previous=relay;
        }
    }
    unsigned queued=g.steps;
    unsigned between=sortie::neighbor(sortie::slot_hex(g.route[1],0,g.facing),g.facing);
    put16(g.army[0],0x2A,between);
    check(sortie_route_issue(g,0) && word(g.army[0],0x3A)==g.march_target[0],
        "army between spaced samples continues to the next sample instead of marching back to the previous one");
    put16(g.army[0],0x2A,sortie::slot_hex(g.route[0],0,g.facing));put16(g.army[0],0x48,word(g.army[0],0x2A));
    check(sortie_route_orders(g) && word(g.army[0],0x48)!=word(g.army[0],0x2A),
        "identical march order with a stale self next-hop regenerates a real native first step");
    check(sortie_step(g.army[0]) && sortie_groups[150].steps==queued && word(g.army[0],0x3A)==g.march_target[0],
        "another callback preserves the full five relay order without queuing an extra chunk");
    put16(g.army[6],0x2A,sortie::slot_hex(g.route[1],6,g.facing));
    check(sortie_movement(g.army[6],nullptr,0,0,nullptr)>0.0f,
        "one earlier waypoint arrival never stops the entire formation");
    for(unsigned slot=0;slot<7;++slot)if(g.army[slot])put16(g.army[slot],0x2A,sortie::slot_hex(g.route[1],slot,g.facing));
    check(sortie_movement(g.army[0],nullptr,0,0,nullptr)==1.25f,
        "movement resumes at the actual minimum once every member reaches that relay");
    for(unsigned slot=0;slot<7;++slot)if(g.army[slot])put16(g.army[slot],0x2A,sortie::slot_hex(g.center,slot,g.facing));
    unsigned endpoint=g.center;
    check(sortie_step(g.army[6]) && sortie_groups[150].steps>queued && sortie_groups[150].route[0]==endpoint,
        "native arrival can queue the next relay chain on the same day with no artificial daily or turn tile cap");
    g=sortie_groups[150];
    // Complete this chunk for the shared construction check.
    for(unsigned slot=0;slot<7;++slot)if(g.army[slot])put16(g.army[slot],0x2A,sortie::slot_hex(g.center,slot,g.facing));
    g.route_count=0;fixture_day=110;
    // An unfinished fortress is a shared mission, including the cached worker callback.
    g.working=true;g.builder=4;g.day=-1;g.site=static_cast<unsigned short>(sortie::slot_hex(g.center,4,g.facing));g.last_motion=110;
    Progress *worker=observe_capture(g.army[4]);worker->facility=4;worker->construction_hex=g.site;
    Obj work_tile=table(0xE270,g.site,48399);put16(work_tile,0x1A,1);
    building[0x12]=4;building[0x13]=2;put16(building,0x14,500);
    sortie_commit(150,g);unsigned before_center=g.center;
    check(sortie_step(g.army[6]) && sortie_groups[150].working && sortie_groups[150].center==before_center,
        "whole army waits around its sole unfinished builder");
    check(sortie_step(g.army[4]) && g.army[4][0x37]==8,"same day worker callback cannot overwrite construction with marching");
    put16(work_tile,0x1A,0);worker->facility=0;g.working=false;g.builder=7;g.active=false;sortie_commit(150,g);
    g.active=true;g.army[4][0x46]=0;g.army[4][0x37]=1;
    for(unsigned i=0;i<7;++i)if(g.army[i])put16(g.army[i],0x2A,sortie::slot_hex(g.center,i,g.facing));
    unsigned regional=sortie::neighbor(g.center,3);Obj regional_tile=table(0xE270,regional,48399);
    Obj regional_area=object(15);put16(regional_area,0x3A,regional);regional_tile[0x14]=3;
    tile_areas[regional_tile]=regional_area;region_tiles={regional_tile};
    slot(0x6CAF0,15,regional_area);put16(regional_tile,0x12,15);
    put16(corridor_area,0x56,15);corridor_area[0x66]=1;
    put16(regional_area,0x56,10);regional_area[0x66]=1;
    Obj onward_castle=object(16);put16(onward_castle,0x3A,25350);onward_castle[0x34]=1;
    slot(0x6CAF0,16,onward_castle);put16(castle_tile,0x12,16);
    put16(regional_area,0x58,16);regional_area[0x66]=3;
    Progress *campaign=observe_capture(g.army[6]);campaign->region_target=48400;
    g.day=-1;sortie_commit(150,g);fixture_day=111;
    check(sortie_step(g.army[6]) && sortie_groups[150].destination==regional &&
        (dword(g.army[6],0x1F0)&2),"native-path capture target retains its occupation flag without a region-chain plan");
    put16(castle_tile,0x12,10);slot(0x6CAF0,16,nullptr);
    g=sortie_groups[150];region_tiles.clear();tile_areas.erase(regional_tile);regional_tile[0x14]=2;put16(regional_tile,0x12,10);
    Obj previous_army=current_army;current_army=g.army[6];unsigned retreats=original_nodes;
    BIND(0xBF7A40,mock_blackboard);BIND(0xB26470,mock_variable);
    auto retreat_result=retreat_node(node,node,current_army);
    check(retreat_result==1 && original_nodes==retreats,
        "healthy field army bypasses native retreat and retains active field work");
    current_army=previous_army;
    g.active=false;sortie_commit(150,g);
    // After a restart, reconstruct the large field formation from native group members.
    sortie_groups[150]={};SortieGroup recovered;unsigned recovered_id=0;
    check(sortie_snapshot(g.army[6],recovered,recovered_id,true) && recovered_id==150,
        "large saved field group can recover without external sidecar state");
    recovered.active=false;sortie_commit(150,recovered);
    SortieGroup priority_group=recovered;priority_group.active=true;sortie_commit(150,priority_group);
    Obj priority_army=priority_group.army[0];unsigned previous_soldiers=word(priority_army,0x16);
    put16(priority_army,0x16,2000);Progress *individual=observe_capture(priority_army);
    individual->motion_day=fixture_day-39;Obj previous_current=current_army;current_army=priority_army;
    unsigned cohort_original=original_nodes;
    retreat_node(node,node,priority_army);
    check(!individual->forced_return && original_nodes==cohort_original+1,
        "a two-thousand rear seat follows the native <=3,000 retreat decision (2026-10-21)");
    individual->motion_day=fixture_day-40;
    individual->credit=false;individual->build_chain=false;individual->facility=0;individual->morale_recovering=false;
    check(apply_priority(priority_army,node,node) && individual->forced_return && priority_army[0x37]==11 && !sortie_groups[150].active,
        "individual four turn noncombat stationary retreat overrides and releases a large cohort");
    put16(priority_army,0x16,previous_soldiers);individual->forced_return=false;individual->returning=false;individual->motion_day=fixture_day;
    current_army=previous_current;
    // A stalled cohort retains native group membership and formation targets.
    auto policy_states=std::vector<Progress>(progress,progress+501);
    SortieGroup stalled=priority_group;stalled.active=true;stalled.rallied=true;stalled.force_march=false;stalled.day=-1;stalled.route_count=0;
    put16(stalled.group,0x14,10);put16(stalled.group,0x16,stalled.city);
    for(Obj member:stalled.army)if(member) {Progress *state=observe_capture(member);state->forced_return=false;state->returning=false;
        state->combat_city=0;state->combat_target=0;state->forced_siege_city=0;state->facility=0;state->credit=false;state->build_chain=false;
        state->morale_recovering=false;state->motion_day=fixture_day-29;}
    sortie_commit(150,stalled);
    check(!campaign_forced_siege_priority(stalled.army[6],progress[army_id(stalled.army[6])]),"cohesive group does not force march at twenty-nine days");
    for(Obj member:stalled.army)if(member)progress[army_id(member)].motion_day=fixture_day-30;
    check(campaign_forced_siege_priority(stalled.army[6],progress[army_id(stalled.army[6])]) && sortie_groups[150].active && sortie_groups[150].force_march &&
        sortie_groups[150].destination==sortie_mock_goal_hex(goal),"thirty days restores the shared final castle destination without dissolving the cohort");
    for(Obj member:stalled.army)if(member)check(campaign_castle_locked(member) && progress[army_id(member)].forced_siege_reason==3 && word(member,0x58)==150,
        "thirty-day movement lock preserves every native cohort member and its assigned formation");
    std::copy(policy_states.begin(),policy_states.end(),progress);priority_group.active=false;sortie_commit(150,priority_group);
    // Retain one ordinary attacker and release every previously deployed support candidate.
    Obj lead=g.army[6];for(unsigned i=60;i<=65;++i)slot(0x7E838,i,nullptr);
    BIND(0x22A5A0,sortie_mock_origin);BIND(0x241C90,sortie_mock_join);
    *reinterpret_cast<unsigned *>(sortie_city+0x3C)=7000;sortie_district[0x14]=20;
    unsigned before_support=sortie_issued;
    unsigned char lead_formation=lead[0x1B];lead[0x1B]=7;
    sortie_support_attach(node,g.group);
    check(sortie_issued==before_support+1,"formation seven can trigger an ordinary female support departure");lead[0x1B]=lead_formation;
    sortie_support_attach(node,g.group);
    check(sortie_issued==before_support+1 && sortie_last_command[2]==1000 && dword(sortie_city,0x3C)==6000,
        "small native attack gets one thousand support soldiers without consuming its garrison reserve");
    Obj escort=table(0x7E838,60+before_support,500);
    check(escort && belongs_to_troops(escort,g.group) && sortie_escort_member(escort),"new support joins the actual native attack group");
    check(sortie_support_step(escort) && word(escort,0x3A)<48400 && sortie_hex_distance(word(escort,0x3A),word(lead,0x2A))<=3,
        "small army support follows consecutive relay points toward its strongest attacker's rear tile");
    put16(escort,0x16,499);check(!sortie_escort_member(escort),"heavily depleted support can use the original retreat policy");put16(escort,0x16,1000);
    sortie_escorts[army_id(escort)]={};check(sortie_escort_member(escort),"saved one thousand support escort also reconstructs from its native group");
    unsigned after_support=sortie_issued;sortie_support_attach(node,g.group);
    check(sortie_issued==after_support,"existing support is not dispatched twice");
    lead[0x1B]=7;
    check(sortie_escort_member(escort) && sortie_support_step(escort),"cached support continues when its target uses formation seven");
    sortie_escorts[army_id(escort)]={};
    check(sortie_escort_member(escort),"support recovery accepts a formation-seven target in a saved native group");lead[0x1B]=lead_formation;
    for(unsigned i=60;i<85;++i)slot(0x7E838,i,nullptr);
    sortie_issued=0;sortie_district[0x14]=20;*reinterpret_cast<unsigned *>(sortie_city+0x3C)=46000;
    for(Obj person:sortie_people)*reinterpret_cast<unsigned *>(person+0x12C)|=(1u<<9);
    put16(goal,0x4E,52);
    check(sortie_departure(node),"forty thousand gate army can still form around native siege rear units");
    unsigned gate_command[30];Obj field_person=sortie_people[0];
    *reinterpret_cast<unsigned *>(field_person+0x12C)&=~(1u<<9);
    sortie_command(node,goal,field_person,3,3000,gate_command,true);
    check(gate_command[3]==1,"gate rear respects native field choice when the native planner prefers it");
    sortie_mock_gate_override=7;sortie_command(node,goal,field_person,3,3000,gate_command,true);
    check(gate_command[3]==7,"native gate rear formation seven is no longer overridden");
    sortie_mock_gate_override=~0u;
    *reinterpret_cast<unsigned *>(field_person+0x12C)|=(1u<<9);
    SortieGroup gate_group=sortie_groups[150];unsigned engines=0;
    for(unsigned i=0;i<7;++i)if(gate_group.army[i] && sortie::siege(gate_group.army[i][0x1B])) {
        ++engines;check(word(gate_group.army[i],0x16)<=8000,"every gate siege unit is capped at eight thousand");
    }
    check(engines>0 && sortie_valid_group(gate_group),"gate priority is preserved while native group remains valid");
    sortie_assault(gate_group.army[0],52,true);
    check(gate_group.army[0][0x37]==5 && gate_group.army[0][0x38]==6 && word(gate_group.army[0],0x3A)==52,
        "gate engagement uses its distinct native attack action and target kind");
    sortie_groups[150]={};memset(sortie_escorts,0,sizeof(sortie_escorts));put16(goal,0x4E,1);
    BIND(0x22A5A0,mock_origin);BIND(0x22C990,mock_goal_force);BIND(0x22D740,mock_goal_hex);fixture_day=120;legal_facilities=~0u;
    for(unsigned i=60;i<85;++i)slot(0x7E838,i,nullptr);
    for(unsigned i=100;i<125;++i)slot(0x7F8D8,i,nullptr);slot(0x7F8D8,150,nullptr);
    list_free(sources);list_free(outputs);
    BIND(0x2AA850,mock_distance);BIND(0x26C940,mock_finalize);BIND(0x22B090,mock_active_formation);
    // Per-entry publication failure leaves earlier changes recoverable.
    for(const auto &e:sortie_entries)memcpy(image+e.rva,e.bytes,16);
    Obj relay=allocate_near(image+0x48CE0);SortieCode code;
    check(relay && prepare_sortie(code,relay),"all eight guarded entry trampolines prepared");
    image[sortie_entries[3].rva]=0xCC;
    check(!publish_sortie(code),"partial hook publication fails on changed native entry");
    restore_sortie(code);
    for(unsigned i=0;i<3;++i)check(memcmp(image+sortie_entries[i].rva,sortie_entries[i].bytes,16)==0,"earlier entries rolled back");
    memcpy(image+sortie_entries[3].rva,sortie_entries[3].bytes,16);
    VirtualFree(relay,0,MEM_RELEASE);
}

static void sortie_abi_tests() {
    const unsigned char planner_tail[]={0x48,0x89,0xC8,0x48,0x31,0xD0,0x4C,0x31,0xC0,0x41,0x5C,0x5D,0xC3};
    const unsigned char dispatch_tail[]={0x48,0x89,0xC8,0x48,0x31,0xD0,0x41,0x5C,0x5F,0x5E,0x5D,0xC3};
    const unsigned char speed_tail[]={0x48,0x89,0x11,0x44,0x89,0x41,0x08,0x44,0x89,0x49,0x0C,
        0x48,0x8B,0x44,0x24,0x40,0xF3,0x0F,0x10,0x00,0x41,0x5D,0x5E,0x5B,0xC3};
    const unsigned char search_tail[]={0x8B,0x44,0x24,0x28,0x03,0x44,0x24,0x30,0x03,0x44,0x24,0x38,0xC3};
    enabled=0;
    for(unsigned i=0;i<8;++i) {
        const auto &e=sortie_entries[i];DWORD old=0;
        check(VirtualProtect(image+(e.rva&~0xFFFu),0x1000,PAGE_EXECUTE_READWRITE,&old)!=0,
            "synthetic sortie native tail is executable");
        const unsigned char *tail=i<5?planner_tail:i==5?dispatch_tail:i==6?speed_tail:search_tail;
        size_t length=i<5?sizeof(planner_tail):i==5?sizeof(dispatch_tail):i==6?sizeof(speed_tail):sizeof(search_tail);
        memcpy(image+e.rva+e.size,tail,length);FlushInstructionCache(GetCurrentProcess(),image+e.rva,64);
        if(i<5) {
            auto installed=reinterpret_cast<SortiePlanner>(image+e.rva);
            Obj a=reinterpret_cast<Obj>(0x11223344ull),b=reinterpret_cast<Obj>(0x55667788ull),c=reinterpret_cast<Obj>(0x123456789ABCDEF0ull);
            uint64_t expected=reinterpret_cast<uint64_t>(a)^reinterpret_cast<uint64_t>(b)^reinterpret_cast<uint64_t>(c);
            check(original_sortie_planners[i](a,b,c)==expected && installed(a,b,c)==expected,
                "all five planner gates and installed jumps preserve three arguments and 64 bit return");
        } else if(i==5) {
            unsigned command[30]={};auto installed=reinterpret_cast<SortieDispatch>(image+e.rva);
            Obj expected=reinterpret_cast<Obj>(reinterpret_cast<uintptr_t>(command)^7);
            check(original_sortie_dispatch(command,7)==expected && installed(command,7)==expected,
                "departure dispatcher preserves command pointer spend argument and all four pushed registers");
        } else if(i==6) {
            uint64_t forwarded[2]={};float value=3.75f;Obj form=reinterpret_cast<Obj>(0x123456789ABCDEF0ull);
            auto installed=reinterpret_cast<SortieMovement>(image+e.rva);
            check(original_sortie_movement(reinterpret_cast<Obj>(forwarded),form,7,13,reinterpret_cast<Obj>(&value))==value &&
                forwarded[0]==reinterpret_cast<uint64_t>(form) && forwarded[1]==(13ull<<32|7),
                "movement gate preserves fifth tile pointer and four register arguments with float return");
            forwarded[0]=forwarded[1]=0;
            check(installed(reinterpret_cast<Obj>(forwarded),form,7,13,reinterpret_cast<Obj>(&value))==value &&
                forwarded[0]==reinterpret_cast<uint64_t>(form) && forwarded[1]==(13ull<<32|7),
                "installed movement jump and callback preserve native fifth pointer ABI");
        }
        else {
            auto installed=reinterpret_cast<SortieSearch>(image+e.rva);SortieNativePath path={};unsigned target=1;
            check(original_sortie_search(&path,nullptr,0,&target,7,11,13)==31 &&
                installed(&path,nullptr,0,&target,7,11,13)==31,
                "path capture trampoline and installed callback forward all seven arguments including three stack arguments");
        }
    }
    enabled=1;
}
