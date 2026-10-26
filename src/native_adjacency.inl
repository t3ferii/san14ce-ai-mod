/* Keep native unit stats and change only their regional multiplier.
 * 2A8D40 serves unit estimates as well as the combat score stage; 16B880
 * supplies the other native regional consumer with its actual opponent. */
using Adjacency = float(__fastcall *)(Obj,int,int *);
static Adjacency original_adjacency;
#include "expert_combat_settings.h"
struct CombatScope { unsigned first,second; bool fixed,player_battle,first_unit,second_unit; };
static __declspec(thread) CombatScope combat_scope;
struct ExpertUnitScope { Obj army;bool active; };
static __declspec(thread) ExpertUnitScope expert_unit_scope;
using ExpertUnitPower=float(__fastcall *)(Obj,Obj,int,int);
static ExpertUnitPower original_expert_unit_power;
static Obj expert_option(Obj options,unsigned key) {
    return options+(key==4?0x24C:0x264);
}
static unsigned __fastcall expert_native_adjacency_option(Obj options,unsigned key,unsigned preset) { COST_SCOPE("battle_expert_native_adjacency_option");
    unsigned value=fn<unsigned(__fastcall *)(Obj,unsigned,unsigned)>(0x3EA400)(options,key,preset);
    // Keep the native regional count and force traits. The packed settings
    // themselves are interpreted only for a unit in an actual player battle.
    return (key==4 || key==5) && expert_combat::tagged(value)?100:value;
}
static void log_bonus(const char *kind,unsigned force,unsigned opponent,int score,float native,float used) {
#ifdef NO_RUNTIME_LOG
    (void)kind;(void)force;(void)opponent;(void)score;(void)native;(void)used;
#else
    if(!diagnostic_logging)return;
    AcquireSRWLockExclusive(&log_lock);
    {
        char line[256];SYSTEMTIME t;GetLocalTime(&t);
        int n=sprintf_s(line,"%04u-%02u-%02u %02u:%02u:%02u %s force=%u opponent=%u score=%d native=%.6f used=%.6f\r\n",
            t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,kind,force,opponent,score,native,used);
        if(n>0)log_append.write(log_path,line,static_cast<DWORD>(n),reuse_log_handle!=0);
    }
    ReleaseSRWLockExclusive(&log_lock);
#endif
}
static unsigned combat_record_force(Obj record) {
    if(!record)return 0;
    unsigned id=word(record,0),kind=dword(record,4);
    if(kind==27) {
        Obj army=table(0x7E838,id,500);
        return valid(army)?army_force(army):0;
    }
    if(kind==11 && id>0 && id<=3000) {
        Obj building=table(0x6DA98,id,3000);
        return valid(building)?building[0x13]:0;
    }
    if(kind==5 || kind==6) {
        if(kind==6) {if(!id || id>10)return 0;id+=51;}
        else if(!id || id>51)return 0;
        Obj goal=fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),id);
        Obj owner=valid(goal)?fn<Obj(__fastcall *)(Obj)>(0x22C990)(goal):nullptr;
        if(!valid(owner))return 0;
        return (*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(owner,0)+0x60))(owner);
    }
    return 0;
}
static float __fastcall expert_unit_power(Obj army,Obj city,int parts,int kind) { COST_SCOPE("battle_expert_unit_power");
    ExpertUnitScope previous=expert_unit_scope;
    expert_unit_scope={army,true};
    float result=original_expert_unit_power(army,city,parts,kind);
    expert_unit_scope=previous;
    return result;
}
static unsigned expert_field_force(Obj army,Obj city) {
    if(valid(army))return army_force(army);
    Obj owner=valid(city)?fn<Obj(__fastcall *)(Obj)>(0x22C990)(city):nullptr;
    return valid(owner)?(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(owner,0)+0x60))(owner):0;
}
static Obj __fastcall expert_field_stats(Obj context,Obj entries) { COST_SCOPE("battle_expert_field_stats");
    CombatScope previous=combat_scope;
    ExpertUnitScope previous_unit=expert_unit_scope;
    if(enabled && context) {
        combat_scope={};
        Obj army=pointer(context,0x98),opponent=pointer(context,0xA8);
        unsigned a=expert_field_force(army,pointer(context,0xA0));
        unsigned b=expert_field_force(opponent,pointer(context,0xB0)),player=player_force();
        if(a>0 && a<46 && b>0 && b<46) {
            bool enemies=a!=b && fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(a,b)!=0;
            combat_scope={a,b,enemies && a!=player && b!=player,
                enemies && player && (a==player || b==player),valid(army),valid(opponent)};
        }
        expert_unit_scope={army,true};
    }
    Obj result=fn<Obj(__fastcall *)(Obj,Obj)>(0x16B880)(context,entries);
    expert_unit_scope=previous_unit;combat_scope=previous;
    return result;
}
static void __fastcall combat_pair_scores(Obj pair,Obj history,Obj output) { COST_SCOPE("battle_combat_pair_scores");
    CombatScope previous=combat_scope;
    combat_scope={};
    unsigned player=player_force();
    if(enabled && pair && (player || observer_mode())) {
        combat_scope.first=combat_record_force(pointer(pair,0));
        combat_scope.second=combat_record_force(pair+8);
        unsigned a=combat_scope.first,b=combat_scope.second;
        bool enemies=a>0 && a<46 && b>0 && b<46 && a!=b &&
            fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(a,b)!=0;
        combat_scope.fixed=enemies && a!=player && b!=player;
        combat_scope.player_battle=enemies && player && (a==player || b==player);
        combat_scope.first_unit=pointer(pair,0) && dword(pointer(pair,0),4)==27;
        combat_scope.second_unit=dword(pair+8,4)==27;
    }
    fn<void(__fastcall *)(Obj,Obj,Obj)>(0x16A030)(pair,history,output);
    combat_scope=previous;
}
static float __fastcall contextual_adjacency(Obj tile,int force,int *occupation_score) { COST_SCOPE("battle_contextual_adjacency");
    // Native code retains unit/officer/formation stats and exact occupation.
    float native=original_adjacency(tile,force,occupation_score);
    if(!enabled || !occupation_score || !valid(tile))return native;
    int score=*occupation_score;
    if(score<0 || score>900)return native;
    unsigned opponent=static_cast<unsigned>(force)==combat_scope.first?combat_scope.second:combat_scope.first;
    bool first=static_cast<unsigned>(force)==combat_scope.first;
    bool member=first || static_cast<unsigned>(force)==combat_scope.second;
    if(combat_scope.fixed && member)return 1.0f+static_cast<float>(score)*0.0004f;
    bool unit=expert_unit_scope.active?
        valid(expert_unit_scope.army) && army_force(expert_unit_scope.army)==static_cast<unsigned>(force):
        combat_scope.player_battle && member && (first?combat_scope.first_unit:combat_scope.second_unit);
    bool applicable=!combat_scope.first || (combat_scope.player_battle && member);
    if(unit && applicable &&
       dword(image,0x1A47AF8)==0) {
        unsigned key=static_cast<unsigned>(force)==player_force()?4:5;
        unsigned value=dword(expert_option(image+0x1A46CA8,key),0);
        if(expert_combat::encoded(value)) {
            auto settings=expert_combat::decode(value);
            // Native already contains 1 + score*(4 + native force traits)/10000.
            return native+expert_combat::multiplier(settings,score)-(1.0f+static_cast<float>(score)*0.0004f);
        }
    }
    static volatile LONG player_samples,outside_samples;
    bool pair=combat_scope.first && combat_scope.second;
    volatile LONG *counter=pair?&player_samples:&outside_samples;
    if(InterlockedIncrement(counter)<=12)
        log_bonus(pair?"adjacency_native_pair_preserved":"adjacency_outside_pair_preserved",static_cast<unsigned>(force),opponent,score,native,native);
    return native;
}
