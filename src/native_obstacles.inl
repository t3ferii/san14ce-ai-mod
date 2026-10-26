// Evidence: research/v9. This callback observes the native DAMAGE application,
// not the earlier score/forecast stage. It only records; AI nodes issue orders.
using ApplyDamage=uint64_t(__fastcall *)(Obj,Obj,Obj);
static ApplyDamage original_damage;
struct RangedHit {
    Obj root, victim, attacker;
    unsigned short victim_commander, attacker_commander, from, to;
    unsigned char victim_force;
    int day;
    bool present;
};
static RangedHit ranged_hits[501];
struct CampaignHit {Obj root,victim,attacker;unsigned short commander,attacker_commander;unsigned char force;int day;bool present;unsigned short source_city;int city_day;};
static CampaignHit campaign_hits[501];
static bool obstacle_between(unsigned from,unsigned to,unsigned wall) {
    if(from>=48400 || to>=48400 || wall>=48400 || from==to || from==wall || to==wall)return false;
    int total=distance(from,to),a=distance(from,wall),b=distance(wall,to);
    return total>1 && a>0 && b>0 && a<total && b<total && a+b<=total+1;
}
static bool player_ranged_army(Obj attacker) {
    unsigned player=player_force();
    if(!player || !valid(attacker) || army_force(attacker)!=player)return false;
    Obj active=fn<Obj(__fastcall *)(Obj)>(0x22B090)(attacker);
    // Active formation getter chooses land/water from the ACTUAL terrain.
    return active && (active==table(0x77428,3,20) || active==table(0x77428,13,20));
}
static uint64_t __fastcall combat_apply_damage(Obj manager,Obj pair,Obj output) {
    if(!enabled || in_policy)return original_damage(manager,pair,output);
    Obj source=pair?pointer(pair,0):nullptr;
    unsigned aid=source && dword(source,4)==27?word(source,0):0;
    unsigned vid=pair && dword(pair+8,4)==27?word(pair+8,0):0;
    Obj attacker=aid?table(0x7E838,aid,500):nullptr,victim=vid?table(0x7E838,vid,500):nullptr;
    unsigned source_kind=source?dword(source,4):0;
    unsigned source_city=source && (source_kind==5 || source_kind==6)?word(source,0):0;
    bool castle_kind=source_kind==5?source_city<=51:source_kind==6 && source_city>51 && source_city<=61;
    Obj firing_city=source_city && castle_kind?fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),source_city):nullptr;
    Obj firing_force=valid(firing_city) && word(firing_city,0x4E)==source_city?fn<Obj(__fastcall *)(Obj)>(0x22C990)(firing_city):nullptr;
    unsigned city_force=valid(firing_force)?(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(firing_force,0)+0x60))(firing_force):0;
    bool city_hit=campaign_controlled(victim) && city_force && fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(army_force(victim),city_force);
    // [combat] hit_response=0: record nothing, so no hit-driven lock/escort/obstacle response.
    bool engaged=enabled && mod_features.hit_response && !in_policy && campaign_controlled(victim) && valid(attacker) &&
        fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(army_force(victim),army_force(attacker))!=0;
    engaged=engaged || (mod_features.hit_response && city_hit);
    bool watch=enabled && !in_policy && engaged && player_ranged_army(attacker) &&
        fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(army_force(victim),army_force(attacker))!=0;
    unsigned before=engaged?word(victim,0x16):0,commander=engaged?word(victim,0x12):0;
    unsigned attacking_commander=engaged && valid(attacker)?word(attacker,0x12):0,owner=engaged?army_force(victim):0;
    Obj saved_root=engaged?world():nullptr;
    unsigned from=watch?word(attacker,0x2A):48400,to=watch?word(victim,0x2A):48400;
    uint64_t result=original_damage(manager,pair,output);
    if(engaged && world()==saved_root && table(0x7E838,vid,500)==victim && valid(victim) &&
       word(victim,0x12)==commander && army_force(victim)==owner && word(victim,0x16)<before) {
        AcquireSRWLockExclusive(&policy_lock);
        const CampaignHit previous=campaign_hits[vid];
        bool same=previous.root==saved_root && previous.victim==victim && previous.commander==commander && previous.force==owner && game_day()>=previous.day;
        campaign_hits[vid]={saved_root,victim,attacker,static_cast<unsigned short>(commander),
            static_cast<unsigned short>(attacking_commander),static_cast<unsigned char>(owner),game_day(),true,
            static_cast<unsigned short>(city_hit?source_city:same?previous.source_city:0),city_hit?game_day():same?previous.city_day:0};
        if(watch)ranged_hits[vid]={saved_root,victim,attacker,static_cast<unsigned short>(commander),
            static_cast<unsigned short>(attacking_commander),static_cast<unsigned short>(from),
            static_cast<unsigned short>(to),static_cast<unsigned char>(owner),game_day(),true};
        ReleaseSRWLockExclusive(&policy_lock);
    }
    return result;
}
static bool ranged_hit_ready(Obj army,int max_age=10) {
    unsigned id=army_id(army);if(!id)return false;
    const RangedHit &hit=ranged_hits[id];int elapsed=game_day()-hit.day;
    if(!hit.present || hit.root!=world() || hit.victim!=army || hit.victim_commander!=word(army,0x12) ||
       hit.victim_force!=army_force(army) || elapsed<0 || elapsed>max_age || !player_ranged_army(hit.attacker) ||
       word(hit.attacker,0x12)!=hit.attacker_commander)return false;
    return true;
}
static bool ranged_wall(Obj army,Obj b,int max_age=10) {
    if(!ranged_hit_ready(army,max_age))return false;
    const RangedHit &hit=ranged_hits[army_id(army)];unsigned wall=word(b,0x10);
    return distance(word(army,0x2A),wall)<=10 && obstacle_between(hit.from,hit.to,wall) &&
        obstacle_between(word(hit.attacker,0x2A),word(army,0x2A),wall);
}
static unsigned neutral_wall_reason(Obj army,const Progress &p,Obj b) {
    if(!mod_features.obstacle)return 0;
    if(b[0x13]!=0 || (b[0x12]!=9 && b[0x12]!=10 && b[0x12]!=16))return 0;
    if(ranged_wall(army,b))return 2;
    if(!p.returning && p.campaign && obstacle_oscillating(p.obstacle) &&
       obstacle_between(word(army,0x2A),p.obstacle.goal,word(b,0x10))) {
        for(unsigned i=1;i<p.obstacle.count;++i)
            if(distance(p.obstacle.hex[0],p.obstacle.hex[i])>12)return 0;
        return 1;
    }
    return 0;
}
static bool neutral_mission_valid(Obj army,const Progress &p,Obj b) {
    // Authorization is latched for at most 20 days so approaching the obstacle
    // does not itself erase the oscillation that justified the attack.
    if(p.destroy_reason==2)return ranged_wall(army,b,20);
    return p.destroy_reason==1 && p.campaign && !p.returning &&
        p.destroy_goal==p.obstacle.goal && p.destroy_progress==p.progress_day &&
        obstacle_between(word(army,0x2A),p.destroy_goal,word(b,0x10));
}
static void observe_obstacle_motion(Obj army,Progress &p) {
    if(p.returning || !p.campaign || word(army,0x16)<3000 || word(army,0x2A)>=48400) {
        p.obstacle.count=0;return;
    }
    Obj goal=fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),p.city);
    unsigned hex=valid(goal)?fn<unsigned(__fastcall *)(Obj)>(0x22D740)(goal):48400;
    if(hex>=48400) {p.obstacle.count=0;return;}
    obstacle_sample(p.obstacle,game_day(),word(army,0x2A),hex,
                    distance(word(army,0x2A),hex),p.progress_day);
}
