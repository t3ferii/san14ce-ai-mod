#include "sortie_policy.h"
using SortiePlanner=uint64_t(__fastcall *)(Obj,Obj,Obj);
using SortieDispatch=Obj(__fastcall *)(unsigned *,int);
// Fifth argument is a TILE POINTER (R15 from entry RSP+28), not an int.
// AI-CONTRACT [caller ABI]: The fifth argument is a tile pointer captured from the native stack contract. Preserve pointer width and nonvolatile state; a fixture matching a guessed signature is not independent native evidence.
using SortieMovement=float(__fastcall *)(Obj,Obj,int,int,Obj);
static SortiePlanner original_sortie_planners[5];
static SortieDispatch original_sortie_dispatch;
static SortieMovement original_sortie_movement;
struct SortieNativePath {int *data;unsigned count,capacity;};
using SortieSearch=int(__fastcall *)(SortieNativePath *,Obj,unsigned,unsigned *,int,int,int);
static SortieSearch original_sortie_search;
struct SortieTrace {std::vector<unsigned> cells;unsigned count;};
static __declspec(thread) SortieTrace *sortie_trace_output;
static int __fastcall sortie_search(SortieNativePath *path,Obj context,unsigned from,unsigned *to,int a,int b,int c) {
    COST_SCOPE("sortie_search");
    // 1EC890 calls this even when 2B50D0 returns the no-destination sentinel.
    // Its caller accepts a failed path; 183A30 does not accept an invalid tile.
    if(!strcmp(native_phase,"sortie_dispatch") && a==1 &&
       (from>=48400 || !to || *to>=48400)) {
        log_event("dispatch_invalid_path_rejected",from,to?*to:48400,static_cast<unsigned>(c));
        return 0;
    }
    const char *previous_caller=native_search_caller;
    unsigned previous_from=native_search_from,previous_flags=native_search_flags,previous_buffer=native_search_buffer;
    native_search_caller=native_phase;native_search_from=from;
    native_search_flags=static_cast<unsigned>(b);native_search_buffer=static_cast<unsigned>(c);
    NativePhase phase("native_path_search");
    ++native_search_depth;
    int result=original_sortie_search(path,context,from,to,a,b,c);
    --native_search_depth;
    native_search_caller=previous_caller;native_search_from=previous_from;
    native_search_flags=previous_flags;native_search_buffer=previous_buffer;
    if(result && sortie_trace_output && path && path->data && path->count && path->count<=48400 &&
       path->data[0]==static_cast<int>(from)) {
        sortie_trace_output->count=path->count;
        sortie_trace_output->cells.assign(path->data,path->data+path->count);
    }
    if(result && campaign_native_trace && path && path->data && path->count<=48400)
        campaign_native_trace->assign(path->data,path->data+path->count);
    return result;
}
static __declspec(thread) Obj sortie_planner_node;
static __declspec(thread) unsigned sortie_auto_departure_scope;
static __declspec(thread) unsigned sortie_cohort_dispatch_scope;
// Dry run of a native planner (sortie_plan): every dispatch is recorded and refused, so the mod can
// see how many soldiers the original AI meant to send before anything leaves the city.
struct SortieProbe {unsigned calls,soldiers;bool foreign;};
static __declspec(thread) SortieProbe *sortie_probe;
constexpr unsigned sortie_split_threshold=40000;
struct SortieGroup {
    Obj root,group,army[7];
    unsigned short commander[7],city,center,destination,site,motion_hex;
    unsigned facing,builder,steps,battle_troops,demolisher,route_count;
    unsigned short route[6];
    int day,last_motion;
    bool active,working,column,rallied,force_march;
    unsigned short march_target[7];
};
static SortieGroup sortie_groups[501];
static SRWLOCK sortie_state_lock=SRWLOCK_INIT;
static bool sortie_auto_person(Obj person) {
    if(!valid(person) || !world())return false;
    Obj district=table(0xE0D0,person[0xE0],51),context=pointer(world(),0x871E8);
    return valid(district) && context &&
        district!=fn<Obj(__fastcall *)(Obj)>(0x32E8D0)(context);
}
static bool sortie_controlled(Obj army) {
    return valid(army) && !army[0x140] && sortie_auto_person(table(0x158,word(army,0x12),6000));
}
static Obj sortie_goal(unsigned city) {
    if(!city || city>61)return nullptr;
    Obj goal=fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),city);
    return valid(goal) && word(goal,0x4E)==city?goal:nullptr;
}
static bool sortie_hostile(Obj army,unsigned city) {
    Obj goal=sortie_goal(city),force=goal?fn<Obj(__fastcall *)(Obj)>(0x22C990)(goal):nullptr;
    if(!valid(force))return false;
    auto id=*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(force,0)+0x60);
    return fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(army_force(army),id(force))!=0;
}
static void sortie_assault(Obj army,unsigned city,bool clear) {
    if(city<=51) {offensive_order(army,city,clear);return;}
    if(clear)fn<void(__fastcall *)(Obj)>(0xA2410)(army);
    fn<void(__fastcall *)(Obj,int)>(0x2EAAE0)(army,5);
    fn<void(__fastcall *)(Obj,int,int)>(0x2EAB40)(army,6,static_cast<int>(city));
}
static unsigned sortie_support(Obj p);
static unsigned sortie_facing(unsigned center,unsigned goal);
static bool sortie_recover(Obj army,Obj group,SortieGroup &g) {
    if(!mod_features.group_advance)return false; // no mod control over native groups after departure
    if(!valid(group) || !attack_mode(word(group,0x14)) || !sortie_hostile(army,word(group,0x16)))return false;
    sortie::Officer facts[7];Obj actors[7];unsigned count=0,total=0;
    for(unsigned i=1;i<=500;++i) {
        Obj a=table(0x7E838,i,500);if(!valid(a) || !belongs_to_troops(a,group))continue;
        if(count==7 || !sortie_controlled(a) || progress[army_id(a)].regional_worker || progress[army_id(a)].siege_city)return false;
        unsigned f=a[0x1B],troops=word(a,0x16);
        Obj p=table(0x158,word(a,0x12),6000);
        unsigned support=troops<=1000?sortie_support(p):0;
        if(f!=1 && f!=3 && f!=4 && f!=7 && !(word(group,0x16)>51 && sortie::siege(f)) && !support)return false;
        facts[count]={word(a,0x12),1u<<f,troops,support,0,word(group,0x16)>51};actors[count++]=a;total+=troops;
    }
    unsigned chosen[7];if(!sortie::roster(facts,count,chosen,total))return false;
    unsigned assigned=0;
    for(unsigned i=0;i<7;++i)if(chosen[i]<count) {
        g.army[i]=actors[chosen[i]];g.commander[i]=word(g.army[i],0x12);++assigned;
    }
    if(assigned!=count)return false;
    g.root=world();g.group=group;g.city=word(group,0x16);g.center=word(g.army[6],0x2A);
    Obj destination=sortie_goal(g.city);if(!valid(g.army[6]) || !destination)return false;
    unsigned goal=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(destination);
    g.facing=sortie_facing(g.center,goal);g.destination=48400;g.site=48400;g.builder=7;
    g.day=-1;g.last_motion=game_day();g.motion_hex=g.center;g.battle_troops=total;g.active=true;
    return true;
}
static bool sortie_snapshot(Obj army,SortieGroup &out,unsigned &id,bool recover=false) {
    if(!valid(army))return false;
    id=word(army,0x58);if(!id || id>500)return false;
    AcquireSRWLockShared(&sortie_state_lock);out=sortie_groups[id];ReleaseSRWLockShared(&sortie_state_lock);
    Obj group=table(0x7F8D8,id,500);
    bool reused=out.root==world() && out.group==group;
    bool regressed=reused && out.day>game_day();
    if(out.root!=world() || out.group!=group || regressed) {
        if(!recover)return false;
        SortieGroup recovered={};
        if(!sortie_recover(army,group,recovered))return false;
        out=recovered;
        AcquireSRWLockExclusive(&sortie_state_lock);sortie_groups[id]=out;ReleaseSRWLockExclusive(&sortie_state_lock);
    }
    if(!out.active)return false;
    for(unsigned i=0;i<7;++i)if(out.army[i]==army && out.commander[i]==word(army,0x12))return true;
    return false;
}
static bool sortie_member(Obj army) {SortieGroup g;unsigned id;return sortie_snapshot(army,g,id,true) && sortie_controlled(army);}
static void sortie_commit(unsigned id,const SortieGroup &g) {
    AcquireSRWLockExclusive(&sortie_state_lock);sortie_groups[id]=g;ReleaseSRWLockExclusive(&sortie_state_lock);
}
static bool campaign_group_member(Obj army) {return sortie_member(army);}
static bool sortie_rally_arrived(const SortieGroup &g) {
    unsigned here=word(g.army[6],0x2A);Obj a=army_area(g.army[6]);
    unsigned center=valid(a)?word(a,0x3A):48400;Obj tile=table(0xE270,center,48399);
    if(!valid(tile) || tile[0x14]!=army_force(g.army[6]) || distance(here,center)>2 ||
        !campaign_adjacent_city_base(center,g.city))return false;
    for(Obj member:g.army)if(member && distance(word(member,0x2A),here)>3)return false;
    return true;
}
static bool campaign_group_marching(Obj army) {
    SortieGroup g;unsigned id;if(!sortie_snapshot(army,g,id,true))return false;
    return !g.rallied && !sortie_rally_arrived(g);
}
static unsigned sortie_support(Obj p) {
    if(p[0xA9]!=1)return 0;
    unsigned found=0;
    // Native effects use the original five slots. Do not promise an inactive
    // extended trait merely because its ID is present in storage.
    for(unsigned i=0;i<5;++i) {
        unsigned trait=word(p,0x118+i*2);
        if(trait==23)found|=1;if(trait==94)found|=2;if(trait==96)found|=4;
    }
    return (found&1)+((found>>1)&1)+((found>>2)&1);
}
static void sortie_command(Obj node,Obj target,Obj person,unsigned formation,unsigned soldiers,unsigned *cmd,bool gate_rear=false) {
    memset(cmd,0,0x78);
    for(unsigned i=8;i<=12;++i)cmd[i]=48400;
    cmd[14]=255;cmd[24]=48400;cmd[25]=48400;
    // Both vector/vtable words are native command defaults, not heap objects.
    memcpy(cmd+20,image+0x13483B0,16);
    fn<void(__fastcall *)(Obj,Obj,Obj,unsigned *,unsigned)>(0x42E70)(node,target,person,cmd,soldiers);
    if(gate_rear)formation=cmd[3];
    cmd[2]=soldiers;cmd[3]=formation;
    fn<void(__fastcall *)(Obj,Obj,unsigned *)>(0xD0090)(person,table(0x77428,formation,20),cmd);
}
static unsigned sortie_field_formation(Obj person) {
    for(unsigned f:{4u,1u,3u,2u,5u,6u,7u,14u})
        if(fn<int(__fastcall *)(Obj,unsigned)>(0x232350)(person,f))return f;
    return 0;
}
static unsigned sortie_capture_formation(Obj person) {
    if(fn<int(__fastcall *)(Obj,unsigned)>(0x232350)(person,7))return 7;
    return sortie_field_formation(person);
}
static void sortie_escort_clear(unsigned id);
// v12.17: corps policy is judged here when this module is loaded; without it the
// convenience module hooks 1EC890 itself (native_district_intercept*.inl).
static Obj __fastcall sortie_dispatch(unsigned *command,int spend) {
    COST_SCOPE("sortie_dispatch");
    NativePhase phase("sortie_dispatch");
    // Player corps policy (지원: transport only, 내정: no military departure) first.
    if(district_intercept::blocked(command))return nullptr;
    if(sortie_probe) {
        if(command && command[0]) {
            ++sortie_probe->calls;sortie_probe->soldiers+=command[2];
            if(!sortie_auto_person(table(0x158,command[0],6000)))sortie_probe->foreign=true;
        }
        return nullptr;
    }
    if(!enabled || !command ||
       !command[0])return original_sortie_dispatch(command,spend);
    Obj person=table(0x158,command[0],6000);
    if(!sortie_auto_person(person))return original_sortie_dispatch(command,spend);
    unsigned copy[30]={};memcpy(copy,command,26*sizeof(unsigned));
    Obj source=campaign_goal(word(person,0xE2));
    // The 4,000 home reserve guards siege departures. mission_v2: the original AI's small occupation,
    // interception and construction sorties keep their own soldier choice (2026-10-21: area-occupy
    // departures per call fell from 11.7% to 5.2% under the reserve).
    bool reserve=!mod_features.mission_v2 || sortie_planner_node || sortie_auto_departure_scope || sortie_cohort_dispatch_scope;
    if(valid(source) && reserve) {unsigned budget=campaign::available(dword(source,0x3C));
        if(budget<1000) {log_event("dispatch_reserve_rejected",copy[0],dword(source,0x3C),copy[15]);return nullptr;}
        copy[2]=(std::min)(copy[2],budget);}
    Obj goal=sortie_planner_node?pointer(sortie_planner_node,0x60):campaign_goal(command[14]==5?command[15]:0);
    bool weak=valid(goal) && word(goal,0x4E)<=51 && dword(goal,0x3C)<10000;
    if(sortie::siege(copy[3])) {
        if(copy[2]>8000)copy[2]=8000;
        bool city=goal?valid(goal) && word(goal,0x4E)>0 && word(goal,0x4E)<=51:command[14]==5;
        if(city) {unsigned formation=sortie_field_formation(person);
            if(formation) {copy[3]=formation;
                fn<void(__fastcall *)(Obj,Obj,unsigned *)>(0xD0090)(person,table(0x77428,formation,20),copy);
                copy[2]=command[2];if(valid(source))copy[2]=(std::min)(copy[2],campaign::available(dword(source,0x3C)));}}
    }
    // [combat] ai_wall_block: a computer departure ordered to build an earthen/stone wall (facility 9/10,
    // command[23] -> Army+46) is refused before it leaves, instead of being cancelled a turn later.
    if(mod_features.ai_wall_block && (copy[13]==8 || copy[13]==9) && (copy[23]==9 || copy[23]==10)) {
        Obj d=table(0xE0D0,person[0xE0],51);
        if(valid(d) && !fn<int(__fastcall *)(Obj)>(0x233750)(d)) {log_event("ai_wall_departure_blocked",copy[0],copy[23],copy[21]);return nullptr;}
    }
    if(person[0xA9]==1)copy[2]=(std::min)(copy[2],1000u);
    if(copy[2]<1000 || person[0x161])return nullptr;
    log_event("dispatch_native_command",copy[0],(copy[13]<<16)|copy[14],copy[15]);
    Obj army=original_sortie_dispatch(copy,spend);
    // A fresh departure can reuse the same native object and commander.
    if(valid(army) && army_id(army)>0 && army_id(army)<=500) {
        unsigned fresh=army_id(army);
        progress[fresh]={};campaign_hits[fresh]={};ranged_hits[fresh]={};
        sortie_escort_clear(fresh);
        if(Progress *p=observe_capture(army)) {p->dispatched=true;p->dispatch_day=game_day();}
    }
    if(mod_features.departures && weak && person[0xA9]!=1 && !sortie_cohort_dispatch_scope && valid(army)) {Progress *p=observe_capture(army);
        if(p) {p->city=word(goal,0x4E);p->campaign=true;p->siege_city=p->city;}}
    return army;
}
static Obj sortie_issue(unsigned *cmd,int spend) {
    ++sortie_auto_departure_scope;Obj army=sortie_dispatch(cmd,spend);--sortie_auto_departure_scope;return army;
}
// cap: soldiers the original planner meant to send (sortie_plan probe); 0 = mod's own stock rule.
static bool sortie_departure_from(Obj node,Obj target,const std::vector<Obj> &sources,unsigned cap=0) {
    COST_SCOPE("sortie_departure_from");
    if(!valid(target) || !word(target,0x4E) || word(target,0x4E)>61)return false;
    Obj target_owner=fn<Obj(__fastcall *)(Obj)>(0x22C990)(target);if(!valid(target_owner))return false;
    unsigned target_force=(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(target_owner,0)+0x60))(target_owner);
    for(Obj city:sources) {
        if(!valid(city))continue;
        Obj owner=fn<Obj(__fastcall *)(Obj)>(0x22C990)(city);if(!valid(owner))continue;
        unsigned source_force=(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(owner,0)+0x60))(owner);
        if(!fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(source_force,target_force))continue;
        unsigned stock=campaign::available(dword(city,0x3C));
        if(cap) {if(stock<sortie_split_threshold)continue;if(stock>cap)stock=cap;}
        else if(!campaign::siege_departure(stock,dword(target,0x3C)))continue;
        GameList people;list_init(people,0x13436B0);
        fn<void(__fastcall *)(Obj,GameList *)>(0x2D7180)(city,&people);
        fn<void(__fastcall *)(GameList *)>(0x2A0780)(&people);
        fn<void(__fastcall *)(GameList *,int,int,int)>(0xD32C0)(&people,0,1,0);
        sortie::Officer facts[128];Obj actors[128];unsigned count=0;
        for(Link *l=list_first(people);l && count<128;l=l->next) {
            Obj p=l->value;if(!sortie_auto_person(p) || p[0x161])continue;
            int capacity=fn<int(__fastcall *)(Obj)>(0x2311A0)(p);
            if(capacity<1000 || capacity>65535)continue;
            int score=fn<int(__fastcall *)(Obj,unsigned,unsigned)>(0x22FCE0)(p,0,0xFFF);
            unsigned support=sortie_support(p);
            if(p[0xA9]==1) {if(!support)continue;capacity=1000;}
            facts[count]={word(p,0x10),dword(p,0x12C),static_cast<unsigned>(capacity),support,score+static_cast<int>(support*200),word(target,0x4E)>51};
            actors[count++]=p;
        }
        list_free(people);
        for(unsigned i=0;i<count;++i)for(unsigned j=i+1;j<count;++j)if(facts[j].score>facts[i].score) {
            auto f=facts[i];facts[i]=facts[j];facts[j]=f;Obj p=actors[i];actors[i]=actors[j];actors[j]=p;
        }
        // 2026-10-22: officers with leadership (통솔) 70 or less stay out of the siege roster unless they
        // are support seats or the roster cannot be formed without them. score = 통솔 + support*200.
        auto leadership=[&](unsigned k){return facts[k].score-static_cast<int>(facts[k].support*200);};
        unsigned strong=0;
        for(unsigned k=0;k<count;++k)if(leadership(k)>70 || facts[k].support) {
            if(k!=strong){auto f=facts[k];Obj a=actors[k];
                for(unsigned m=k;m>strong;--m){facts[m]=facts[m-1];actors[m]=actors[m-1];}
                facts[strong]=f;actors[strong]=a;}
            ++strong;
        }
        unsigned chosen[7],capacities[7]={},soldiers[7],commands[7][30],number=0;
        if(!sortie::roster(facts,strong,chosen,stock) && !sortie::roster(facts,count,chosen,stock))continue;
        {   // At least one middle/rear seat (2,4) in 안행 (formation 3) to cover 방원/어린 fronts.
            bool wedge=false;for(unsigned i:{2u,4u,6u})if(chosen[i]<count && (facts[chosen[i]].mask&(1u<<3)))wedge=true;
            if(!wedge) {
                unsigned slot=7;int weakest=INT32_MAX;
                for(unsigned i:{2u,4u})if(chosen[i]<count && leadership(chosen[i])<weakest){weakest=leadership(chosen[i]);slot=i;}
                for(unsigned k=0;slot<7 && k<count;++k) {
                    bool used=false;for(unsigned i=0;i<7;++i)if(chosen[i]==k)used=true;
                    if(used || leadership(k)<=70 || !(facts[k].mask&(1u<<3)) || !sortie::accepts(facts[k],slot))continue;
                    log_event("sortie_wedge_seat",facts[k].id,leadership(k),slot);chosen[slot]=k;break;
                }
            }
        }
        Obj district=nullptr;
        for(unsigned i=0;i<7;++i)if(chosen[i]<count) {
            capacities[i]=facts[chosen[i]].capacity;++number;
            Obj d=table(0xE0D0,actors[chosen[i]][0xE0],51);
            if(district && district!=d) {number=0;break;}district=d;
        }
        if(!number || !valid(district))continue;
        if(district[0x14]<number) {
            for(unsigned slot:{4u,2u,5u,3u,1u})if(capacities[slot] && number>district[0x14]) {capacities[slot]=0;--number;}
            if(number<2 || number>district[0x14])continue;
        }
        unsigned front_mask=0;
        for(unsigned i=0;i<7;++i)if(capacities[i]) {
            sortie_command(node,target,actors[chosen[i]],sortie::formation(facts[chosen[i]],i),capacities[i],commands[i],
                facts[chosen[i]].gate && sortie::role(i)==1);
            // Fronts and officers with 통솔 90+ receive the larger share of the extra soldiers.
            if(commands[i][3]==1 || commands[i][3]==4 || leadership(chosen[i])>=90)front_mask|=1u<<i;
            if(sortie::siege(commands[i][3]) && capacities[i]>8000)capacities[i]=8000;
        }
        // This 1,000 is an immediately dispatched companion, not a garrison.
        // Use all stock across main+helper when a legal spare officer and a
        // second capture target exist. Otherwise keep the full main budget.
        unsigned companion_budget=0;
        if(mod_features.support && word(target,0x4E)<=51 && stock>=5500 && district[0x14]>number) {
            bool spare=false;for(unsigned k=0;k<count;++k) {
                bool used=false;for(unsigned i=0;i<7;++i)if(capacities[i] && chosen[i]==k)used=true;
                if(!used && !actors[k][0x161] && facts[k].capacity>=1000 && sortie_field_formation(actors[k]))spare=true;
            }
            unsigned city_hex=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(target),available_bases=0;
            GameList nearby;list_init(nearby);Obj tile=table(0xE270,city_hex,48399);
            if(valid(tile))fn<void(__fastcall *)(Obj,int,GameList *)>(0x2A7AA0)(tile,15,&nearby);
            for(Link *l=list_first(nearby);l;l=l->next)if(valid(l->value)) {
                unsigned id=campaign_area_id(fn<unsigned(__fastcall *)(Obj)>(0x22DDA0)(l->value));
                Obj a=table(0x6CAF0,id,500);if(!id || !valid(a) || campaign_transit_region(id))continue;
                unsigned h=word(a,0x3A);Obj base=table(0xE270,h,48399);if(!valid(base) || base[0x14]==(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(owner,0)+0x60))(owner))continue;
                for(unsigned edge=0;edge<8;++edge) {
                    unsigned n=word(a,0x56+edge*2);Obj b=table(0x6CAF0,n,500);if(!(a[0x66]&(1u<<edge)) || !n || !valid(b) || campaign_transit_region(n))continue;
                    Obj bt=table(0xE270,word(b,0x3A),48399);if(valid(bt) && bt[0x14]!=(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(owner,0)+0x60))(owner))++available_bases;
                }
            }
            list_free(nearby);
            unsigned paired[7];if(spare && available_bases && sortie::allocate(capacities,stock-1000,paired,front_mask))companion_budget=1000;
        }
        if(!sortie::allocate(capacities,stock-companion_budget,soldiers,front_mask))continue;
        unsigned cost=0,total=0;bool ready=true;
        for(unsigned i=0;i<7;++i)if(capacities[i]) {
            unsigned f=commands[i][3];commands[i][2]=soldiers[i];
            Obj land=table(0x77428,f,20),water=table(0x77428,commands[i][4],20);
            if(!valid(land) || !valid(water)) {ready=false;break;}
            cost+=word(land,0x9A)+word(water,0x9A);total+=soldiers[i];
        }
        auto money=*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(city,0)+0x90);
        unsigned free_armies=0,free_groups=0;
        for(unsigned i=1;i<=500;++i) {if(!valid(table(0x7E838,i,500)))++free_armies;if(!valid(table(0x7F8D8,i,500)))++free_groups;}
        if(!ready || money(city)<static_cast<int>(cost) || free_armies<number || free_groups<number+1)continue;
        GameList created;list_init(created);SortieGroup g={};Obj temporary[7]={};
        bool issued=true;
        for(unsigned i=0;i<7;++i)if(capacities[i]) {
            ++sortie_cohort_dispatch_scope;
            Obj army=actors[chosen[i]][0x161]?nullptr:sortie_issue(commands[i],1);
            --sortie_cohort_dispatch_scope;
            if(!valid(army)) {issued=false;break;}
            g.army[i]=army;g.commander[i]=word(army,0x12);
            temporary[i]=table(0x7F8D8,word(army,0x58),500);
            if(!list_add(created,army)) {issued=false;break;}
        }
        if(issued) {
            g.group=fn<Obj(__fastcall *)(GameList *)>(0x2D9260)(&created);
            issued=valid(g.group);
        }
        if(!issued) {
            // Native cancellation owns soldier/resource bookkeeping.
            if(valid(g.group))fn<void(__fastcall *)(Obj)>(0x9E900)(g.group);
            else for(unsigned i=0;i<7;++i)if(valid(g.army[i])) {
                Obj old=table(0x7F8D8,word(g.army[i],0x58),500);
                if(valid(old))fn<void(__fastcall *)(Obj)>(0x9E900)(old);
            }
            list_free(created);log_event("sortie_departure_failed",0,total,word(city,0x4E));return false;
        }
        list_free(created);
        for(Obj old:temporary)if(valid(old) && old!=g.group)fn<void(__fastcall *)(Obj)>(0x282540)(old);
        *reinterpret_cast<unsigned short *>(g.group+0x14)=static_cast<unsigned short>(node?dword(node,0x4C):10);
        *reinterpret_cast<unsigned short *>(g.group+0x16)=word(target,0x4E);
        *reinterpret_cast<unsigned short *>(g.group+0x2C)=g.commander[6];
        if(node && !list_add(*reinterpret_cast<GameList *>(node+0x98),g.group)) {
            fn<void(__fastcall *)(Obj)>(0x9E900)(g.group);return false;
        }
        float power=0;for(Obj army:g.army)if(army)power+=fn<float(__fastcall *)(Obj)>(0xB5EC0)(army);
        if(node)*reinterpret_cast<int *>(node+0x84)-=static_cast<int>(power);
        // group_advance=0: the formation only shapes the departure; the original AI moves each army.
        g.active=mod_features.group_advance;g.root=world();g.city=word(target,0x4E);g.center=word(g.army[6],0x2A);
        g.facing=sortie_facing(g.center,fn<unsigned(__fastcall *)(Obj)>(0x22D740)(target));
        g.destination=48400;g.site=48400;g.builder=7;g.day=-1;g.last_motion=game_day();g.motion_hex=g.center;g.battle_troops=total;
        sortie_commit(word(g.army[6],0x58),g);
        if(companion_budget) {
            Progress *lead=observe_capture(g.army[6]);if(lead) {
                lead->city=g.city;lead->campaign=true;
                unsigned h=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(target);
                if(next_region(g.army[6],*lead,h,true)==1)campaign_request_companion(g.army[6],*lead,lead->region_target);
            }
        }
        log_event(stock>=20000?"cohesive_sortie_departed":"small_cohort_departed",number,total,g.city);return true;
    }
    return false;
}
static bool sortie_departure(Obj node) {
    Obj target=pointer(node,0x60),list=pointer(node,0x78);if(!list)return false;
    std::vector<Obj> sources;for(Link *s=list_first(*reinterpret_cast<GameList *>(list));s;s=s->next)sources.push_back(s->value);
    return sortie_departure_from(node,target,sources);
}
struct SortieEscort {
    Obj root,army,leader,group;
    unsigned short commander,leader_commander,city;
    int day;
};
static SortieEscort sortie_escorts[501];
static void sortie_escort_clear(unsigned id) {
    if(!id || id>500)return;
    AcquireSRWLockExclusive(&sortie_state_lock);sortie_escorts[id]={};ReleaseSRWLockExclusive(&sortie_state_lock);
}
static bool sortie_escort_member(Obj army) {
    if(!sortie_controlled(army))return false;
    unsigned id=army_id(army);if(!id)return false;
    AcquireSRWLockShared(&sortie_state_lock);SortieEscort e=sortie_escorts[id];ReleaseSRWLockShared(&sortie_state_lock);
    if(e.root!=world() || e.army!=army || e.commander!=word(army,0x12)) {
        Obj group=table(0x7F8D8,word(army,0x58),500);
        Obj p=table(0x158,word(army,0x12),6000),leader=nullptr;
        if(!valid(group) || !attack_mode(word(group,0x14)) || word(army,0x16)>1000 || !sortie_support(p))return false;
        for(unsigned i=1;i<=500;++i) {
            Obj a=table(0x7E838,i,500);
            if(a!=army && sortie_controlled(a) && belongs_to_troops(a,group) &&
                word(a,0x16)>=3000 && (!leader || word(a,0x16)>word(leader,0x16)))leader=a;
        }
        if(!leader)return false;
        e={world(),army,leader,group,word(army,0x12),word(leader,0x12),word(group,0x16),game_day()};
        AcquireSRWLockExclusive(&sortie_state_lock);sortie_escorts[id]=e;ReleaseSRWLockExclusive(&sortie_state_lock);
    }
    return e.root==world() && e.army==army && e.commander==word(army,0x12) &&
        valid(e.leader) && word(army,0x16)>=1000 && word(e.leader,0x12)==e.leader_commander &&
        belongs_to_troops(army,e.group) && belongs_to_troops(e.leader,e.group) &&
        game_day()>=e.day && attack_mode(word(e.group,0x14)) && word(e.group,0x16)==e.city;
}
static void sortie_support_attach(Obj node,Obj group) {
    COST_SCOPE("sortie_support_attach");
    if(!valid(group) || !attack_mode(word(group,0x14)))return;
    Obj leader=nullptr,support=nullptr;
    for(unsigned i=1;i<=500;++i) {
        Obj a=table(0x7E838,i,500);
        if(!sortie_controlled(a) || !belongs_to_troops(a,group))continue;
        if(sortie_member(a))return;
        Obj p=table(0x158,word(a,0x12),6000);
        if(sortie_support(p) && word(a,0x16)<=1000)support=a;
        else if(!leader || word(a,0x16)>word(leader,0x16))leader=a;
    }
    if(!leader)return;
    if(!support) {
        Obj city=fn<Obj(__fastcall *)(Obj)>(0x22A5A0)(leader);
        if(!valid(city) || campaign::available(dword(city,0x3C))<1000)return;
        GameList candidates;list_init(candidates,0x13436B0);
        fn<void(__fastcall *)(Obj,GameList *)>(0x2D7180)(city,&candidates);
        fn<void(__fastcall *)(GameList *)>(0x2A0780)(&candidates);
        fn<void(__fastcall *)(GameList *,int,int,int)>(0xD32C0)(&candidates,0,1,0);
        Obj chosen=nullptr;unsigned traits=0;
        for(Link *l=list_first(candidates);l;l=l->next) {
            Obj p=l->value;
            if(!sortie_auto_person(p) || p[0x161] || !sortie_field_formation(p) ||
                fn<int(__fastcall *)(Obj)>(0x2311A0)(p)<1000)continue;
            bool deployed=false;
            for(unsigned i=1;i<=500;++i) {
                Obj a=table(0x7E838,i,500);if(valid(a) && word(a,0x12)==word(p,0x10)) {deployed=true;break;}
            }
            if(deployed)continue;
            unsigned score=sortie_support(p);
            if(score>traits) {chosen=p;traits=score;}
        }
        list_free(candidates);if(!chosen)return;
        Obj district=table(0xE0D0,chosen[0xE0],51);
        if(!district[0x14])return;
        unsigned command[30];sortie_command(node,pointer(node,0x60),chosen,sortie_field_formation(chosen),1000,command);
        Obj land=table(0x77428,command[3],20),water=table(0x77428,command[4],20);
        if(!valid(land) || !valid(water))return;
        auto resource=*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(city,0)+0x90);
        if(resource(city)<word(land,0x9A)+word(water,0x9A))return;
        support=chosen[0x161]?nullptr:sortie_issue(command,1);if(!valid(support))return;
        Obj temporary=table(0x7F8D8,word(support,0x58),500);
        unsigned group_id=word(leader,0x58);
        fn<void(__fastcall *)(Obj,unsigned)>(0x241C90)(support,group_id);
        if(word(support,0x58)!=group_id) {
            if(valid(temporary))fn<void(__fastcall *)(Obj)>(0x9E900)(temporary);return;
        }
        if(valid(temporary) && temporary!=group)fn<void(__fastcall *)(Obj)>(0x282540)(temporary);
        log_event("sortie_support_departed",word(chosen,0x10),1000,word(group,0x16));
    }
    unsigned id=army_id(support);if(!id)return;
    SortieEscort e={world(),support,leader,group,word(support,0x12),word(leader,0x12),word(group,0x16),game_day()};
    AcquireSRWLockExclusive(&sortie_state_lock);sortie_escorts[id]=e;ReleaseSRWLockExclusive(&sortie_state_lock);
}
static unsigned sortie_group_count(Obj node) {
    unsigned n=0;for(Link *l=list_first(*reinterpret_cast<GameList *>(node+0x98));l;l=l->next)++n;return n;
}
static uint64_t sortie_plan(Obj node,Obj context,Obj subject,unsigned index) {
    COST_SCOPE("sortie_plan");
    // [combat] sortie_formation=0: native planners only (no mod groups, so group steps stay idle).
    if(!enabled || !mod_features.sortie_formation)return original_sortie_planners[index](node,context,subject);
    Obj previous=sortie_planner_node;sortie_planner_node=node;
    // The original AI decides whether and how much to send. Only a plan of 40,000+ soldiers is
    // re-formed into the mod's front/middle/rear roster from the same source cities and target.
    unsigned before=sortie_group_count(node);
    SortieProbe probe={};sortie_probe=&probe;
    uint64_t dry=original_sortie_planners[index](node,context,subject);
    sortie_probe=nullptr;
    unsigned after=sortie_group_count(node);
    Obj target=pointer(node,0x60);unsigned goal=valid(target)?word(target,0x4E):0;
    if(probe.calls)log_event("sortie_native_plan",probe.calls,probe.soldiers,goal);
    if(after!=before)log_event("sortie_probe_group_changed",before,after,goal);
    bool created=false;
    if(probe.calls && !probe.foreign && probe.soldiers>=sortie_split_threshold && after==before) {
        Obj list=pointer(node,0x78);std::vector<Obj> sources;
        if(list)for(Link *l=list_first(*reinterpret_cast<GameList *>(list));l;l=l->next)sources.push_back(l->value);
        created=sortie_departure_from(node,target,sources,probe.soldiers);
        log_event(created?"sortie_split_departed":"sortie_split_unavailable",probe.calls,probe.soldiers,goal);
    }
    uint64_t result=created?1:probe.calls?original_sortie_planners[index](node,context,subject):dry;
    if(!created && mod_features.support)for(Link *l=list_first(*reinterpret_cast<GameList *>(node+0x98));l;l=l->next)
        sortie_support_attach(node,l->value);
    sortie_planner_node=previous;return result;
}
static uint64_t __fastcall sortie_plan8(Obj n,Obj c,Obj s) {return sortie_plan(n,c,s,0);}
static uint64_t __fastcall sortie_plan9(Obj n,Obj c,Obj s) {return sortie_plan(n,c,s,1);}
static uint64_t __fastcall sortie_plan10(Obj n,Obj c,Obj s) {return sortie_plan(n,c,s,2);}
static uint64_t __fastcall sortie_plan11(Obj n,Obj c,Obj s) {return sortie_plan(n,c,s,3);}
static uint64_t __fastcall sortie_plan22(Obj n,Obj c,Obj s) {return sortie_plan(n,c,s,4);}
static bool sortie_valid_group(const SortieGroup &g) {
    if(!valid(g.group) || game_day()<g.last_motion)return false;
    bool build_return=g.working && word(g.group,0x14)==7;
    if(!build_return && (!attack_mode(word(g.group,0x14)) || word(g.group,0x16)!=g.city))return false;
    for(unsigned i=0;i<7;++i)if(g.army[i]) {
        Obj army=g.army[i];const Progress &p=progress[army_id(army)];
        if(!sortie_controlled(army) || word(army,0x12)!=g.commander[i] || !belongs_to_troops(army,g.group) || word(army,0x16)<1000u)return false;
        if(p.army==army && p.root==world() && p.commander==word(army,0x12) &&
           (p.forced_return || p.combat_target || p.combat_city || p.home_defense_city || p.companion_parent || p.guard_beneficiary))return false;
    }
    return sortie_hostile(g.army[6],g.city);
}
static unsigned sortie_route_progress(const SortieGroup &g,unsigned slot) {
    unsigned actual=word(g.army[slot],0x2A),best=0;int error=INT32_MAX;
    for(unsigned i=1;i<g.route_count;++i) {
        unsigned a=sortie::slot_hex(g.route[i-1],slot,g.facing),b=sortie::slot_hex(g.route[i],slot,g.facing);
        if(actual==b)return i;
        int da=distance(actual,a),db=distance(actual,b),length=distance(a,b);
        int e=da+db-length;
        if(e<error || (e==error && db<distance(actual,sortie::slot_hex(g.route[best+1],slot,g.facing)))) {
            error=e;best=i-1;
        }
    }
    // The containing segment's beginning has been consumed. Its next sample
    // remains ahead; do not requeue the preceding waypoint behind the army.
    return best;
}
static float __fastcall sortie_movement(Obj army,Obj formation,int preview,int modifier,Obj tile) {
    COST_SCOPE("sortie_movement");
    float native;
    {COST_SCOPE("native_sortie_movement");native=original_sortie_movement(army,formation,preview,modifier,tile);}
    SortieGroup g;unsigned id;
    if(!enabled || !(native>0) || !sortie_snapshot(army,g,id) || !sortie_valid_group(g))return native;
    static __declspec(thread) Obj terrain_root=nullptr,neutral=nullptr;
    if(terrain_root!=world()) {terrain_root=world();neutral=nullptr;
        for(unsigned h=0;h<48400;++h) {Obj t=table(0xE270,h,48399);if(valid(t) && t[0x10]==1) {neutral=t;break;}}}
    float slowest;
    {COST_SCOPE("native_sortie_movement");slowest=original_sortie_movement(army,formation,preview,modifier,neutral);}
    // Reference principle: common base movement and common terrain speed.
    // Existing native getter/ABI supplies neutral terrain, not foreign RVAs.
    for(Obj other:g.army)if(other) {
        float base;
        {COST_SCOPE("native_sortie_movement");base=original_sortie_movement(other,fn<Obj(__fastcall *)(Obj)>(0x22B090)(other),preview,modifier,neutral);}
        if(base>0)slowest=(std::min)(slowest,base);
    }
    // Impassable terrain still returns native zero before this branch.
    return slowest>0?slowest:native;
}
static void sortie_move(Obj army,unsigned hex) {
    if(campaign_castle_locked(army))return;
    if(native_search_depth)return;
    if(army[0x37]==1 && army[0x38]==9 && word(army,0x3A)==hex && word(army,0x2C)==hex)return;
    fn<void(__fastcall *)(Obj)>(0xA2410)(army);
    fn<void(__fastcall *)(Obj,int)>(0x2EAAE0)(army,1);
    fn<void(__fastcall *)(Obj,int,int)>(0x2EAB40)(army,9,static_cast<int>(hex));
    fn<void(__fastcall *)(Obj,int,unsigned)>(0x2EE1E0)(army,0,hex);
    fn<void(__fastcall *)(Obj)>(0x26C940)(army);
}
struct SortieOrder {unsigned char bytes[0x34];unsigned flags;unsigned short future;};
static SortieOrder sortie_order(Obj army) {SortieOrder o;memcpy(o.bytes,army+0x2C,sizeof(o.bytes));o.flags=dword(army,0x1F0);o.future=word(army,0x1F8);return o;}
static void sortie_restore(Obj army,const SortieOrder &o) {memcpy(army+0x2C,o.bytes,sizeof(o.bytes));*reinterpret_cast<unsigned *>(army+0x1F0)=o.flags;*reinterpret_cast<unsigned short *>(army+0x1F8)=o.future;}
static bool sortie_trace_path(Obj army,unsigned target,SortieTrace &trace) {
    if(native_search_depth)return false;
    trace={};SortieOrder saved=sortie_order(army);SortieTrace *previous=sortie_trace_output;
    sortie_trace_output=&trace;sortie_move(army,target);fn<void(__fastcall *)(Obj)>(0x26C940)(army);
    sortie_trace_output=previous;sortie_restore(army,saved);
    if(!trace.count || trace.cells[0]!=word(army,0x2A))return false;
    for(unsigned i=1;i<trace.count;++i)if(trace.cells[i]>=48400 || distance(trace.cells[i-1],trace.cells[i])!=1)return false;
    return true;
}
static bool sortie_chain_order(Obj army,const unsigned *points,unsigned count) {
    if(campaign_castle_locked(army))return false;
    if(native_search_depth)return false;
    if(!count || count>5)return false;
    for(unsigned i=0;i<count;++i) {
        if(points[i]>=48400 || !valid(table(0xE270,points[i],48399)) ||
            !campaign_region_open(army,campaign_area_id(points[i])))return false;
    }
    bool same=army[0x37]==1 && army[0x38]==9 && word(army,0x3A)==points[count-1];
    for(unsigned i=0;i<5;++i)same=same && word(army,0x2C+i*2)==(i<count?points[i]:48400);
    if(!same || (word(army,0x2A)!=points[count-1] &&
        (word(army,0x48)>=48400 || distance(word(army,0x2A),word(army,0x48))!=1))) {
        fn<void(__fastcall *)(Obj)>(0xA2410)(army);
        fn<void(__fastcall *)(Obj,int)>(0x2EAAE0)(army,1);
        fn<void(__fastcall *)(Obj,int,int)>(0x2EAB40)(army,9,static_cast<int>(points[count-1]));
        for(unsigned i=0;i<count;++i)fn<void(__fastcall *)(Obj,int,unsigned)>(0x2EE1E0)(army,static_cast<int>(i),points[i]);
        std::vector<unsigned> trace;auto prior=campaign_native_trace;campaign_native_trace=&trace;
        fn<void(__fastcall *)(Obj)>(0x26C940)(army);campaign_native_trace=prior;
        Obj group=table(0x7F8D8,word(army,0x58),500);
        Obj city=valid(group)?sortie_goal(word(group,0x16)):nullptr;
        bool assault=city && distance(points[count-1],fn<unsigned(__fastcall *)(Obj)>(0x22D740)(city))<=2;
        for(unsigned h:trace)if(!campaign_region_open(army,campaign_area_id(h)) || (!assault && campaign_city_danger(army,h)))return false;
    }
    return word(army,0x2A)==points[count-1] ||
        (word(army,0x48)<48400 && distance(word(army,0x2A),word(army,0x48))==1);
}
// Assembly, turning and small support also use consecutive native relays.
static bool sortie_short_move(Obj army,unsigned target) {
    if(campaign_castle_locked(army))return false;
    Progress *p=observe_capture(army);
    if(p && campaign_near_capture(army,*p))return true;
    if(word(army,0x2A)==target) {sortie_move(army,target);return true;}
    // Preserve explicit city assaults; every other placement/support relay
    // uses the same safe area corridor as ordinary capture movement.
    Obj group=table(0x7F8D8,word(army,0x58),500);
    Obj city=valid(group)?sortie_goal(word(group,0x16)):nullptr;
    bool assault=city && distance(target,fn<unsigned(__fastcall *)(Obj)>(0x22D740)(city))<=2 &&
        progress[army_id(army)].region_target>=48400;
    if(!assault) {
        CampaignRoute route=campaign_safe_route(army,target,false,true);
        return campaign_route_order(army,route);
    }
    SortieTrace trace;if(!sortie_trace_path(army,target,trace) || trace.count<2)return false;
    unsigned points[5];unsigned count=campaign_relay_points(trace.cells,trace.count-1,points);
    return sortie_chain_order(army,points,count);
}
static bool sortie_route_issue(SortieGroup &g,unsigned slot) {
    if(!g.army[slot])return true;
    unsigned target=g.route_count>1?g.march_target[slot]:sortie::slot_hex(g.center,slot,g.facing);
    if(target>=48400)return false;
    return sortie_short_move(g.army[slot],target);
}
static bool sortie_route_orders(SortieGroup &g) {
    SortieOrder saved[7];for(unsigned i=0;i<7;++i)if(g.army[i])saved[i]=sortie_order(g.army[i]);
    for(unsigned i=0;i<7;++i)if(g.army[i] && !sortie_route_issue(g,i)) {
        for(unsigned j=0;j<7;++j)if(g.army[j])sortie_restore(g.army[j],saved[j]);return false;
    }
    return true;
}
static bool sortie_settled(const SortieGroup &g) {
    for(unsigned i=0;i<7;++i)if(g.army[i] && (!g.working || i!=g.builder)) {
        unsigned target=g.route_count>1?g.march_target[i]:sortie::slot_hex(g.center,i,g.facing);
        if(word(g.army[i],0x2A)!=target)return false;
    }
    return true;
}
static unsigned sortie_facing(unsigned center,unsigned goal) {
    unsigned best=0;int nearest=INT32_MAX;
    for(unsigned i=0;i<6;++i) {unsigned hex=sortie::neighbor(center,i);if(hex>=48400)continue;
        int d=distance(hex,goal);if(d<nearest) {nearest=d;best=i;}}
    return best;
}
static bool sortie_place(SortieGroup &g,unsigned center,unsigned facing) {
    SortieOrder saved[7];unsigned targets[7];
    for(unsigned i=0;i<7;++i)if(g.army[i]) {
        targets[i]=sortie::slot_hex(center,i,facing);
        if(targets[i]>=48400 || !valid(table(0xE270,targets[i],48399)))return false;
        for(unsigned j=1;j<=500;++j) {
            Obj other=table(0x7E838,j,500);if(!valid(other) || belongs_to_troops(other,g.group))continue;
            if(word(other,0x2A)==targets[i])return false;
        }
        saved[i]=sortie_order(g.army[i]);
    }
    bool possible=true;
    for(unsigned i=0;i<7;++i)if(g.army[i] && (!g.working || i!=g.builder)) {
        if(!sortie_short_move(g.army[i],targets[i])) {possible=false;break;}
    }
    if(!possible)for(unsigned i=0;i<7;++i)if(g.army[i])sortie_restore(g.army[i],saved[i]);
    return possible;
}
static void sortie_hold(SortieGroup &g) {
    for(unsigned i=0;i<7;++i)if(g.army[i] && (!g.working || i!=g.builder))
        sortie_move(g.army[i],word(g.army[i],0x2A));
}
static bool sortie_advance(SortieGroup &g,unsigned destination) {
    unsigned here=word(g.army[6],0x2A);if(here==destination)return false;
    auto corridor=campaign_safe_route(g.army[6],destination,true,true);if(corridor.cells.size()<2)return false;
    SortieGroup before=g;unsigned next=corridor.cells[1],facing=sortie_facing(here,next);
    g.facing=facing;g.route_count=2;g.route[0]=static_cast<unsigned short>(here);g.route[1]=static_cast<unsigned short>(next);
    g.center=static_cast<unsigned short>(next);g.column=false;
    for(unsigned i=0;i<7;++i)if(g.army[i])g.march_target[i]=static_cast<unsigned short>(sortie::slot_hex(next,i,facing));
    if(!sortie_route_orders(g)) {
        // A narrow passage forms a single-file column on the leader's native
        // path. Followers take the previous member's vacated physical tile.
        g=before;g.facing=facing;g.route_count=2;g.route[0]=static_cast<unsigned short>(here);g.route[1]=static_cast<unsigned short>(next);
        g.center=static_cast<unsigned short>(next);g.column=true;
        unsigned previous=next;const unsigned order[7]={0,6,1,5,2,4,3};
        for(unsigned slot:order)if(g.army[slot]) {
            g.march_target[slot]=static_cast<unsigned short>(previous);previous=word(g.army[slot],0x2A);
        }
        if(!sortie_route_orders(g)) {g=before;return false;}
    }
    ++g.steps;log_event(g.column?"cohesive_sortie_column":"cohesive_sortie_hex_barrier",word(g.army[6],0x58),1,next);return true;
}
static bool sortie_forced_group_march(Obj actor,SortieGroup &g,unsigned id) {
    struct MarchScope {MarchScope(){++campaign_group_march_depth;}~MarchScope(){--campaign_group_march_depth;}} scope;
    unsigned goal=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(sortie_goal(g.city));
    if(goal>=48400)return false;
    int day=game_day();
    if(g.day!=day || word(g.army[6],0x2A)!=g.motion_hex || (g.route_count>1 && sortie_settled(g))) {
        g.day=day;g.motion_hex=word(g.army[6],0x2A);g.destination=static_cast<unsigned short>(goal);
        // The whole formation shares the native leader path and keeps its seats.
        // A failed path preserves existing commands instead of ordering a hold.
        if(distance(g.motion_hex,goal)>1) {
            sortie_advance(g,goal);
        }
    } else if(g.route_count>1)for(unsigned i=0;i<7;++i)if(g.army[i]==actor) {sortie_route_issue(g,i);break;}
    for(unsigned i=0;i<7;++i)if(g.army[i] && sortie::role(i)==0 && distance(word(g.army[i],0x2A),goal)<=1) {
        sortie_assault(g.army[i],g.city,true);fn<void(__fastcall *)(Obj)>(0x26C940)(g.army[i]);
    }
    sortie_commit(id,g);return true;
}
static bool sortie_support_step(Obj army) {
    COST_SCOPE("sortie_support_step");
    if(!enabled || !sortie_escort_member(army))return false;unsigned id=army_id(army);
    AcquireSRWLockShared(&sortie_state_lock);SortieEscort e=sortie_escorts[id];ReleaseSRWLockShared(&sortie_state_lock);
    Obj goal=campaign_goal(e.city);if(!goal)return false;
    unsigned center=word(e.leader,0x2A),here=word(army,0x2A),goal_hex=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(goal);
    unsigned rear=sortie::neighbor(center,(sortie_facing(center,goal_hex)+3)%6);
    if(distance(here,center)<=3 && distance(here,goal_hex)>=distance(center,goal_hex)) {sortie_move(army,here);return true;}
    std::vector<unsigned> targets;
    int cx=static_cast<int>(center%220),cy=static_cast<int>(center/220);
    for(int y=(std::max)(0,cy-3);y<=(std::min)(219,cy+3);++y)
        for(int x=(std::max)(0,cx-3);x<=(std::min)(219,cx+3);++x) {
            unsigned h=static_cast<unsigned>(y*220+x);
            if(distance(h,center)<=3 && distance(h,goal_hex)>=distance(center,goal_hex))targets.push_back(h);
        }
    std::sort(targets.begin(),targets.end(),[here,rear](unsigned a,unsigned b){int da=distance(here,a)*4+distance(a,rear),db=distance(here,b)*4+distance(b,rear);return da!=db?da<db:a<b;});
    for(unsigned h:targets) {
        bool occupied=false;for(unsigned i=1;i<=500;++i) {Obj a=table(0x7E838,i,500);if(a!=army && valid(a) && word(a,0x2A)==h) {occupied=true;break;}}
        if(!occupied && sortie_short_move(army,h))return true;
    }
    sortie_move(army,here);return true;
}
static bool sortie_capture_work(SortieGroup &g,unsigned id,Progress *lead) {
    int day=game_day();
    if(g.working) {
        Progress *worker=observe_capture(g.army[g.builder]);
        if(worker && continue_build(g.army[g.builder],*worker)) {
            g.last_motion=day;if(!sortie_place(g,g.center,g.facing))sortie_hold(g);sortie_commit(id,g);return true;
        }
        if(worker && campaign_forced_siege_priority(g.army[g.builder],*worker))return true;
        if(worker) {lead->cycle_mask|=worker->cycle_mask;lead->build_chain=worker->build_chain;lead->build_center=worker->build_center;}
        g.working=false;g.builder=7;g.last_motion=day;
    }
    if(lead->credit || lead->build_chain) {
            // One builder; the whole cohort escorts its legal site.
            const unsigned builders[5]={4,2,5,1,0};
            for(unsigned slot:builders)if(g.army[slot]) {
                Progress *worker=observe_capture(g.army[slot]);if(!worker)continue;
                worker->campaign=true;worker->city=g.city;worker->credit=lead->credit;
                worker->build_chain=lead->build_chain;worker->build_center=lead->build_center;worker->cycle_mask=lead->cycle_mask;
                Progress saved_worker=*worker;SortieOrder saved_order=sortie_order(g.army[slot]);
                if(try_build(g.army[slot],*worker,g.center)) {
                    unsigned site=worker->construction_hex;
                    unsigned center=sortie::neighbor(site,(slot+g.facing+3)%6);
                    g.working=true;g.builder=slot;g.site=static_cast<unsigned short>(site);
                    if(sortie_place(g,center,g.facing)) {
                        g.center=static_cast<unsigned short>(center);lead->credit=false;
                        log_event("cohesive_sortie_build",army_id(g.army[slot]),worker->facility,site);break;
                    }
                    // Path generation has not executed construction. Restore
                    // this candidate if its escorts cannot surround the site.
                    sortie_restore(g.army[slot],saved_order);*worker=saved_worker;
                    g.working=false;g.builder=7;g.site=48400;
                }
            }
            if(g.working) {sortie_commit(id,g);return true;}
            if(!area_full(army_area(g.army[6])) || area_hostile_facilities(g.army[6],army_area(g.army[6]))) {
                sortie_hold(g);g.last_motion=day;sortie_commit(id,g);return true;
            }
            lead->credit=false;lead->build_chain=false;
        }
    return false;
}
static bool sortie_step(Obj actor) {
    COST_SCOPE("sortie_step");
    Progress *priority=observe_capture(actor);
    if(priority && campaign_forced_siege_priority(actor,*priority))return true;
    SortieGroup g;unsigned id;if(!enabled || !sortie_snapshot(actor,g,id,true))return false;
    bool recovering=false;
    for(Obj a:g.army)if(a && progress[army_id(a)].morale_recovering)recovering=true;
    if(recovering) {
        // Individual recovery owns its orders. Other seats wait without
        // overwriting the band detour, and the cohort resumes when it ends.
        Progress *p=observe_capture(actor);
        if(p && p->morale_recovering)campaign_morale_priority(actor,*p);
        else if(!(p && p->facility && continue_build(actor,*p)))sortie_move(actor,word(actor,0x2A));
        g.last_motion=game_day();sortie_commit(id,g);return true;
    }
    if(!sortie_valid_group(g)) {g.active=false;sortie_commit(id,g);return false;}
    if(g.working && word(g.group,0x14)==7) {
        *reinterpret_cast<unsigned short *>(g.group+0x14)=10;
        *reinterpret_cast<unsigned short *>(g.group+0x16)=g.city;
    }
    Progress *capture=observe_capture(g.army[6]);
    if(capture && sortie_capture_work(g,id,capture))return true;
    int day=game_day();unsigned goal_hex=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(sortie_goal(g.city));
    if(g.force_march)return sortie_forced_group_march(actor,g,id);
    if(!g.rallied && campaign_area_id(word(g.army[6],0x2A))==campaign_area_id(goal_hex)) {
        Obj standing=table(0xE270,word(g.army[6],0x2A),48399);bool together=valid(standing) && standing[0x14]==army_force(actor);
        for(Obj member:g.army)if(member && distance(word(member,0x2A),word(g.army[6],0x2A))>3)together=false;
        if(together) {g.rallied=true;g.route_count=0;g.center=word(g.army[6],0x2A);g.day=-1;g.last_motion=day;sortie_commit(id,g);}
    }
    if(!g.rallied) {
        if(sortie_rally_arrived(g)) {
            g.rallied=true;g.route_count=0;g.center=word(g.army[6],0x2A);g.day=-1;g.last_motion=day;
            sortie_commit(id,g);
        } else {
            // Travel to the adjacent base in one native route, without a
            // per-hex formation barrier. Assemble once on arrival; from
            // that base onward the existing formation march owns movement.
            if(word(g.army[6],0x2A)!=g.motion_hex) {g.motion_hex=word(g.army[6],0x2A);g.last_motion=day;}
            if(g.day!=day) {
                g.day=day;std::vector<unsigned> bases;Obj a=table(0x6CAF0,campaign_area_id(goal_hex),500);
                if(valid(a))for(unsigned i=0;i<8;++i) {
                    unsigned n=word(a,0x56+i*2);if(!(a[0x66]&(1u<<i)) || !n || n>500 || campaign_transit_region(n))continue;
                    Obj b=table(0x6CAF0,n,500);if(!valid(b))continue;unsigned h=word(b,0x3A);Obj t=table(0xE270,h,48399);
                    if(valid(t) && (!t[0x14] || t[0x14]==army_force(actor) || campaign_region_hostile(actor,n)))bases.push_back(h);
                }
                unsigned here=word(g.army[6],0x2A);
                std::sort(bases.begin(),bases.end(),[here](unsigned a,unsigned b){return distance(here,a)<distance(here,b);});
                bool issued=false;
                for(unsigned base:bases) {
                    unsigned intermediate=campaign_path_capture_hex(g.army[6],base);
                    unsigned h=intermediate<48400?intermediate:base;
                    if(!sortie_place(g,h,sortie_facing(h,goal_hex)))continue;
                    g.destination=static_cast<unsigned short>(h);g.center=static_cast<unsigned short>(h);
                    g.facing=sortie_facing(h,goal_hex);g.route_count=0;
                    Progress *p=observe_capture(g.army[6]);if(p) {p->city=g.city;p->campaign=true;
                        p->region_target=static_cast<unsigned short>(h);p->region_owner=table(0xE270,h,48399)[0x14];
                        *reinterpret_cast<unsigned *>(g.army[6]+0x1F0)|=2;}
                    issued=true;break;
                }
                if(!issued)sortie_hold(g);
                sortie_commit(id,g);
            } else if(g.destination<48400) {
                for(unsigned slot=0;slot<7;++slot)if(g.army[slot]==actor) {
                    sortie_short_move(actor,sortie::slot_hex(g.destination,slot,g.facing));
                    if(slot==6)*reinterpret_cast<unsigned *>(actor+0x1F0)|=2;
                    break;
                }
            }
            return true;
        }
    }
    bool settled=sortie_settled(g);
    if(g.day==day && g.route_count>1 && (!settled || g.working || g.demolisher)) {
        if(g.working && actor==g.army[g.builder]) {
            Progress *worker=observe_capture(actor);if(worker)continue_build(actor,*worker);
            return true;
        }
        if(g.demolisher && actor==g.army[g.demolisher-1]) {
            Progress *front=observe_capture(actor);
            if(front && front->destroy_id && distance(word(actor,0x2A),front->destroy_hex)<=1 && try_destroy(actor,*front))return true;
        }
        unsigned slot=0;while(slot<7 && g.army[slot]!=actor)++slot;
        if(slot<7) {
            if(!g.working && (g.route_count<2 || settled) && sortie::role(slot)==0 && distance(word(actor,0x2A),goal_hex)<=1) {
                sortie_assault(actor,g.city,true);fn<void(__fastcall *)(Obj)>(0x26C940)(actor);
            } else {
                bool issued=g.route_count>1?sortie_route_issue(g,slot):sortie_short_move(actor,sortie::slot_hex(g.center,slot,g.facing));
                if(!issued)sortie_move(actor,word(actor,0x2A));
            }
            if(slot==6 && g.destination<48400 && progress[army_id(actor)].region_target==g.destination)
                *reinterpret_cast<unsigned *>(actor+0x1F0)|=2;
        }
        return true;
    }
    g.day=day;
    if(word(g.army[6],0x2A)!=g.motion_hex) {g.motion_hex=word(g.army[6],0x2A);g.last_motion=day;}
    unsigned remaining=0;for(Obj army:g.army)if(army)remaining+=word(army,0x16);
    g.battle_troops=remaining;
    Progress *lead=observe_capture(g.army[6]);if(!lead)return false;
    remember_campaign(g.army[6],*lead);lead->campaign=true;lead->city=g.city;observe_obstacle_motion(g.army[6],*lead);
    if(settled || g.route_count<2) {
        g.route_count=0;g.demolisher=0;
        // Keep the original neutral obstacle authorization. One front unit
        // performs the demolition and all other seats remain its escort.
        for(unsigned slot:{0u,1u,5u})if(g.army[slot]) {
            Progress *front=observe_capture(g.army[slot]);if(!front)continue;
            front->campaign=true;front->city=g.city;observe_obstacle_motion(g.army[slot],*front);
            SortieOrder saved=sortie_order(g.army[slot]);
            if(try_destroy(g.army[slot],*front)) {
                g.demolisher=slot+1;
                if(distance(word(g.army[slot],0x2A),front->destroy_hex)<=1) {
                    for(unsigned i=0;i<7;++i)if(g.army[i] && i!=slot)
                        sortie_short_move(g.army[i],sortie::slot_hex(g.center,i,g.facing));
                    g.last_motion=day;
                } else {
                    sortie_restore(g.army[slot],saved);
                    g.destination=front->destroy_hex;sortie_advance(g,front->destroy_hex);
                    if(g.route_count>1)sortie_route_orders(g);else sortie_place(g,g.center,g.facing);
                }
                sortie_commit(id,g);return true;
            }
        }
        bool arrived=!campaign_group_marching(g.army[6]);
        if(arrived)prepare_local_build(g.army[6],*lead);
        if(arrived && sortie_capture_work(g,id,lead))return true;
        unsigned destination=goal_hex;
        SortieOrder original=sortie_order(g.army[6]);
        unsigned region=next_region(g.army[6],*lead,goal_hex,true);
        if(region==1) {destination=lead->region_target;campaign_request_companion(g.army[6],*lead,destination);}
        sortie_restore(g.army[6],original);
        unsigned intermediate=campaign_path_capture_hex(g.army[6],destination);
        if(intermediate<48400) {destination=intermediate;lead->region_target=static_cast<unsigned short>(intermediate);
            lead->region_owner=table(0xE270,intermediate,48399)[0x14];}
        g.destination=static_cast<unsigned short>(destination);
        if(region==1 || (region==0 && distance(g.center,goal_hex)>2))sortie_advance(g,destination);
    }
    if(day-g.last_motion>=20) {
        g.active=false;sortie_commit(id,g);log_event("cohesive_sortie_stalled_assault",id,20,g.city);
        for(unsigned i=0;i<7;++i)if(g.army[i]) {
            Progress &p=progress[army_id(g.army[i])];
            if(p.forced_return || p.combat_target || p.combat_city || p.facility || p.morale_recovering)continue;
            if(sortie::role(i)==0 || (sortie::role(i)==1 && word(g.army[i],0x16)>=5000)) {
                p.siege_city=g.city<=51?g.city:0;sortie_assault(g.army[i],g.city,true);fn<void(__fastcall *)(Obj)>(0x26C940)(g.army[i]);
            } else sortie_short_move(g.army[i],sortie::slot_hex(word(g.army[6],0x2A),i,g.facing));
        }
        return true;
    }
    bool issued=g.route_count>1?sortie_route_orders(g):sortie_place(g,g.center,g.facing);
    if(!issued)sortie_hold(g);
    // At the final city, native adjacent attackers retain city engagement;
    // rear units and the support officer keep their assigned escort seats.
    for(unsigned i=0;i<7;++i)if(!g.working && (g.route_count<2 || sortie_settled(g)) && g.army[i] && sortie::role(i)==0 &&
        distance(word(g.army[i],0x2A),goal_hex)<=1) {
        sortie_assault(g.army[i],g.city,true);fn<void(__fastcall *)(Obj)>(0x26C940)(g.army[i]);
    }
    if(lead->region_target<48400 && lead->region_target==g.destination)
        *reinterpret_cast<unsigned *>(g.army[6]+0x1F0)|=2;
    sortie_commit(id,g);return true;
}
