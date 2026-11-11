#include "game_addresses.h"
/* Verified native facility attack orders: action 6, kind 11, building ID.
 * Area cap is the native 2C9E30 check (classes 2/3/4 vs Area+6E).
 * Never remove a facility directly; its movement and damage remain native. */
static bool area_full(Obj area) { COST_SCOPE("facilities:area_full");
    return valid(area) && fn<bool(__fastcall *)(Obj)>(GA(GA_2CC1E0))(area);
}
static bool area_hostile_facilities(Obj army,Obj area) { COST_SCOPE("facilities:area_hostile_facilities");
    unsigned own=army_force(army);
    for(unsigned i=1;i<=3000;++i) {
        Obj b=table(0x6DA98,i,3000);
        if(!valid(b) || !word(b,0x14) || b[0x13]==own || b[0x12]<3 || b[0x12]>14 || word(b,0x10)>=48400)continue;
        if(b[0x13] && !fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,b[0x13]))continue;
        Obj t=table(0xE270,word(b,0x10),48399);
        if(valid(t) && fn<Obj(__fastcall *)(Obj)>(GA(GA_22B8E0))(t)==area)return true;
    }
    return false;
}
[[maybe_unused]] static bool player_threat(Obj army) { COST_SCOPE("facilities:player_threat");
    unsigned player=player_force(),own=army_force(army);
    if(!player || !fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,player))return false;
    for(unsigned i=1;i<=500;++i) {
        Obj enemy=table(0x7E838,i,500);
        if(valid(enemy) && army_force(enemy)==player && word(enemy,0x2A)<48400 &&
           distance(word(army,0x2A),word(enemy,0x2A))<=6)return true;
    }
    return false;
}
static unsigned local_facilities(Obj army, Obj area) { COST_SCOPE("facilities:local_facilities");
    unsigned mask=0,owner=army_force(army);
    for(unsigned i=1;i<=3000;++i) {
        Obj b=table(0x6DA98,i,3000);
        if(!valid(b) || b[0x13]!=owner || b[0x12]>=31 || word(b,0x10)>=48400)continue;
        Obj tile=table(0xE270,word(b,0x10),48399);
        if(valid(tile) && fn<Obj(__fastcall *)(Obj)>(GA(GA_22B8E0))(tile)==area)mask|=1u<<b[0x12];
    }
    for(unsigned i=1;i<=500;++i) {
        Progress &other=progress[i];Obj member=table(0x7E838,i,500);
        if(member==army || !computer_army(member) || army_force(member)!=owner || other.army!=member ||
           other.commander!=word(member,0x12) || !other.facility || other.facility>=31 || other.construction_hex>=48400)continue;
        Obj tile=table(0xE270,other.construction_hex,48399);
        if(valid(tile) && fn<Obj(__fastcall *)(Obj)>(GA(GA_22B8E0))(tile)==area)mask|=1u<<other.facility;
    }
    if(mask & (15u<<11))mask|=15u<<11;
    return mask;
}
static void remember_cut(Obj army, Progress &p) { COST_SCOPE("facilities:remember_cut");
    if(!player_force())return;
    unsigned id=dword(army,0x1F4);Obj target=table(0x7E838,id,500);
    if(valid(target) && army_force(target)==player_force()) {
        p.cut_target=static_cast<unsigned short>(id);p.cut_commander=word(target,0x12);
        p.cut_day=game_day();
        p.cut_action=army[0x37];p.cut_kind=army[0x38];p.cut_goal=word(army,0x3A);
        for(unsigned j=0;j<5;++j)p.cut_route[j]=word(army,0x2C+j*2);
    }
}
static bool restore_cut(Obj army, Progress &p) { COST_SCOPE("facilities:restore_cut");
    if(campaign_castle_locked(army))return false;
    if(!player_force())return false;
    Obj target=table(0x7E838,p.cut_target,500);
    if(!p.cut_target || p.cut_day!=game_day() || !valid(target) || army_force(target)!=player_force() ||
       word(target,0x12)!=p.cut_commander)return false;
    bool have=false,same=army[0x37]==p.cut_action && army[0x38]==p.cut_kind && word(army,0x3A)==p.cut_goal;
    for(unsigned j=0;j<5;++j) {
        have=have || p.cut_route[j]<48400;same=same && word(army,0x2C+j*2)==p.cut_route[j];
    }
    if(!have)return false;
    if(!same) {
        fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
        fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,p.cut_action);
        fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,p.cut_kind,p.cut_goal);
        for(unsigned j=0;j<5;++j)fn<void(__fastcall *)(Obj,int,unsigned)>(GA(GA_2F05A0))(army,static_cast<int>(j),p.cut_route[j]);
        fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(army);
        log_event("cut_priority_restored",army_id(army),p.cut_target,word(army,0x2A));
    }
    *reinterpret_cast<unsigned *>(army+0x1F0)|=8;
    *reinterpret_cast<unsigned *>(army+0x1F4)=p.cut_target;return true;
}
static bool priority_mission(Obj army, const Progress &p) { COST_SCOPE("facilities:priority_mission");
    if(!player_force())return p.destroy_id!=0;
    Obj cut=table(0x7E838,p.cut_target,500);
    if(p.cut_target && p.cut_day==game_day() && valid(cut) && army_force(cut)==player_force() &&
       word(cut,0x12)==p.cut_commander)return true;
    unsigned id=dword(army,0x1F4);cut=table(0x7E838,id,500);
    return ((dword(army,0x1F0)&8) && id && valid(cut) && army_force(cut)==player_force()) || p.destroy_id!=0;
}
static bool destructible(Obj army, Obj b) { COST_SCOPE("facilities:destructible");
    if(!valid(b) || !word(b,0x14) || word(b,0x10)>=48400)return false;
    unsigned type=b[0x12],owner=b[0x13];
    if(!owner)return type==9 || type==10 || type==16;
    if(type<3 || type>14)return false; // preserve enemy-owned facility scope
    return owner!=army_force(army) && fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(army_force(army),owner)!=0;
}
static bool destroy_order(Obj army, Progress &p, unsigned id, Obj b) { COST_SCOPE("facilities:destroy_order");
    if(path_wall_done(army,6))return false;   // PW22: a finished army takes no mod order
    bool changed=army[0x37]!=6 || army[0x38]!=11 || word(army,0x3A)!=id;
    if(changed) {
        fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
        fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,6);
        fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,11,static_cast<int>(id));
        fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(army);
    }
    if(p.destroy_id!=id || p.destroy_hex!=word(b,0x10)) {
        bool path_wall=path_wall_fresh(p,b);
        if(path_wall) {
            // PW22: the order becomes a lock until the building falls.
            p.path_wall_lock_id=static_cast<unsigned short>(id);
            p.path_wall_lock_hex=word(b,0x10);
            p.path_wall_lock_day=game_day();
            p.path_wall_done=false;
        }
        p.destroy_day=game_day();
        p.destroy_reason=static_cast<unsigned char>(b[0x13]?0:path_wall?3:neutral_wall_reason(army,p,b));
        p.destroy_goal=p.obstacle.goal;p.destroy_progress=p.progress_day;
        log_event(b[0x13]?"destroy_enemy_facility":path_wall?"destroy_path_wall":"destroy_neutral_wall",
                  army_id(army),id,word(b,0x10));
    }
    p.destroy_id=static_cast<unsigned short>(id);p.destroy_hex=word(b,0x10);
    p.destroy_type=b[0x12];p.destroy_owner=b[0x13];p.did_demolition=true;
    p.finished_demolition=false;return true;
}
// PW22: keep the locked path_wall demolition alive or release it. Runs from the
// existing per-army try_destroy pass; returns true when the lock was released.
static bool path_wall_lock_step(Obj army, Progress &p) { COST_SCOPE("facilities:path_wall_lock_step");
    if(!p.path_wall_lock_id)return false;
    bool alive=!army[0x140] && p.army==army && p.root==world() &&
        p.commander==word(army,0x12) && p.force==army_force(army);
    Obj b=alive?table(0x6DA98,p.path_wall_lock_id,3000):nullptr;
    bool valid_b=alive && valid(b);
    bool same=valid_b && word(b,0x10)==p.path_wall_lock_hex && b[0x12]==p.destroy_type && !b[0x13];
    unsigned reason=wall_lock::release_reason(mod_features.path_wall && !route_hook_breach,alive,valid_b,same,
        valid_b?static_cast<unsigned>(word(b,0x14)):0);
    if(reason) {path_wall_release(army,p,reason);return true;}
    int day=game_day();
    if(p.path_wall_lock_day!=day &&
       wall_lock::reissue(wall_lock::locked,army[0x37],army[0x38],word(army,0x3A),p.path_wall_lock_id,true)) {
        p.path_wall_lock_day=day;
        static volatile LONG reissue_lines;
        if(InterlockedIncrement(&reissue_lines)<=200)
            log_event("path_wall_lock_reissue",army_id(army),p.path_wall_lock_id,
                      (static_cast<unsigned>(army[0x37])<<8)|static_cast<unsigned>(army[0x38]));
        destroy_order(army,p,p.path_wall_lock_id,b);
    }
    return false;
}
static bool try_destroy(Obj army, Progress &p, bool emergency_only=false) { COST_SCOPE("facilities:try_destroy");
    if(campaign_castle_locked(army))return false;
    if(p.path_wall_done)return false;              // PW22: the original AI decides now
    if(path_wall_lock_step(army,p))return false;   // PW22: released in this pass
    bool have_hit=ranged_hit_ready(army);
    if(emergency_only && !have_hit && !(p.destroy_id && p.destroy_reason==2))return false;
    Obj area=army_area(army);int day=game_day();
    bool full=area_full(area),drifting=day-p.progress_day>=60; // enemy-owned legacy condition only
    bool loop_ready=p.campaign && !p.returning && obstacle_oscillating(p.obstacle);
    if(!emergency_only)refresh_path_wall(army,p);
    bool path_ready=p.path_wall_hex && p.path_wall_hex<48400;
    if(!p.destroy_id && !have_hit && !loop_ready && !path_ready && !full && !drifting)return false;
    unsigned failed=0;
    if(p.destroy_id) {
        Obj b=table(0x6DA98,p.destroy_id,3000);
        if(emergency_only && (!valid(b) || p.destroy_reason!=2 || b[0x13]))return false;
        bool same=valid(b) && word(b,0x10)==p.destroy_hex && b[0x12]==p.destroy_type && b[0x13]==p.destroy_owner;
        bool eligible=same && (b[0x13]?true:neutral_mission_valid(army,p,b));
        bool locked=p.path_wall_lock_id && p.path_wall_lock_id==p.destroy_id; // PW22: no 20-day expiry
        if(eligible && destructible(army,b) && day>=p.destroy_day && (locked || day-p.destroy_day<20))return destroy_order(army,p,p.destroy_id,b);
        if(!same || !word(b,0x14)) {
            p.progress_day=day;
            p.finished_demolition=true;p.return_deadline_day=day+10;
            log_event("facility_destroyed_resume",army_id(army),p.destroy_id,p.destroy_hex);
        } else failed=p.destroy_id;
        if(!p.destroy_owner && army[0x37]==6 && army[0x38]==11 && word(army,0x3A)==p.destroy_id) {
            fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
            fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,13);
            fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,255,0);
            fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(army);
        }
        p.destroy_id=0;p.destroy_hex=48400;p.destroy_reason=0;
    }
    int best=INT32_MAX;unsigned chosen=0;Obj target=nullptr;
    for(unsigned i=1;i<=3000;++i) {
        Obj b=table(0x6DA98,i,3000);if(i==failed || !destructible(army,b))continue;
        bool path_wall=!b[0x13] && path_wall_fresh(p,b);
        unsigned hex=word(b,0x10);int range=distance(word(army,0x2A),hex);
        if(range>10 && !path_wall)continue;
        unsigned reason=b[0x13]?0:neutral_wall_reason(army,p,b);
        if(!b[0x13]) {if(!reason || (emergency_only && reason!=2))continue;}
        else if(emergency_only || (!full && !drifting))continue;
        Obj tile=table(0xE270,hex,48399);
        bool local=valid(tile) && valid(area) && fn<Obj(__fastcall *)(Obj)>(GA(GA_22B8E0))(tile)==area;
        if(b[0x13] && !drifting && !local)continue;
        // At the cap, first clear something that actually occupies this area's slots.
        int score=path_wall?-150:range*4+(local?0:100)+(b[0x13]==player_force()?0:2)-(reason==2?200:0);
        if(score<best) {best=score;chosen=i;target=b;}
    }
    if(!target)return false;
    if(!target[0x13] && p.destroy_id!=chosen) {
        unsigned reason=neutral_wall_reason(army,p,target),log_army=army_id(army),log_hex=word(target,0x10);
        log_event(reason==3?"destroy_path_wall":reason==2?"neutral_wall_ranged_hit":"neutral_wall_10_turn_oscillation",
                  log_army,chosen,log_hex);
    }
    return destroy_order(army,p,chosen,target);
}
static bool cancel_unqualified_neutral_order(Obj army,Progress &p) { COST_SCOPE("facilities:cancel_unqualified_neutral_order");
    // Old saves can already contain the former cap/idle demolition command.
    // [combat] obstacle=0 leaves native neutral-wall orders untouched.
    if(!mod_features.obstacle || army[0x37]!=6 || army[0x38]!=11)return false;
    if(route_hook_owns && route_hook_owns(army))return false;   // U39: a route_hook breach order
    unsigned id=word(army,0x3A);Obj b=table(0x6DA98,id,3000);
    if(!id || !valid(b) || b[0x13] || (b[0x12]!=9 && b[0x12]!=10 && b[0x12]!=16))return false;
    bool recorded=p.destroy_id==id && p.destroy_hex==word(b,0x10) && p.destroy_type==b[0x12] &&
        !p.destroy_owner && game_day()>=p.destroy_day && game_day()-p.destroy_day<20 &&
        neutral_mission_valid(army,p,b);
    if(recorded || neutral_wall_reason(army,p,b))return false;
    fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
    fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,13);
    fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,255,0);
    fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(army);
    log_event("neutral_wall_unqualified_order_cancelled",army_id(army),id,word(b,0x10));
    return true;
}
