#pragma once
// Combat AI: wall/catapult build guards (build_permission and the order cancellers), moved verbatim out of native_adapter.cpp (W6-D).
// Shared native eligibility covers both dedicated AI builders and campaign
// construction. Tile ownership alone cannot identify the acting force.
using BuildPermission = bool(__fastcall *)(Obj, Obj, Obj, int);
static BuildPermission original_build_permission = nullptr;
static volatile LONG wall_log_day[52][2];
static unsigned blocked_wall_force(Obj force, Obj definition) { COST_SCOPE("ai_wall:blocked_wall_force");
    if (!definition || (definition != table(0x73958,9,30) && definition != table(0x73958,10,30))) return 0;
    unsigned player = player_force();
    if (!player && !observer_mode()) return 0;
    for (unsigned id = 1; id < 52; ++id)
        if (force && force == table(0xDF30,id,51) && valid(force)) return id != player ? id : 0;
    return 0;
}
// 2026-10-29: only owned castle/gate regions and their first neighbours.
// Tile+12 and Area+56/+66 use the existing campaign region graph.
struct CatapultGroundCache {
    Obj root=nullptr;
    int day=0;
    unsigned player=0;
    bool ready=false,blocked[501]={};
    unsigned bases=0,seedless=0,last_seedless=0;
};
static CatapultGroundCache catapult_ground_cache;
static SRWLOCK catapult_ground_lock=SRWLOCK_INIT;
static unsigned catapult_tile_area(Obj tile) { COST_SCOPE("ai_wall:catapult_tile_area");
    if(!valid(tile))return 0;
    unsigned id=word(tile,0x12);
    if(!id || id>500)return 0;
    Obj area=fn<Obj(__fastcall *)(Obj)>(GA(GA_22B8E0))(tile);
    return valid(area) && area==table(0x6CAF0,id,500)?id:0;
}
static bool catapult_player_ground(Obj tile) { COST_SCOPE("ai_wall:catapult_player_ground");
    if(!enabled || !mod_features.ai_catapult_block)return false;
    unsigned player=player_force();
    if(!player)return false;
    unsigned region=catapult_tile_area(tile);
    if(!region)return false;
    Obj root=world();int day=game_day();
    AcquireSRWLockExclusive(&catapult_ground_lock);
    auto &cache=catapult_ground_cache;
    if(!cache.ready || cache.root!=root || cache.day!=day || cache.player!=player) {
        CatapultGroundCache next;next.root=root;next.day=day;next.player=player;next.ready=true;
        for(unsigned id=1;id<=61;++id) {
            Obj base=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(root,id);
            if(!valid(base) || word(base,0x4E)!=id)continue;
            Obj owner=fn<Obj(__fastcall *)(Obj)>(GA(GA_22E3E0))(base);
            if(!valid(owner))continue;
            auto get_id=*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(owner,0)+0x60);
            if(get_id(owner)!=player)continue;
            unsigned hex=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F190))(base);
            unsigned seed=catapult_tile_area(table(0xE270,hex,48399));
            ++next.bases;
            if(!seed){++next.seedless;next.last_seedless=(id<<16)|(hex&0xFFFF);continue;}
            next.blocked[seed]=true;
            Obj area=table(0x6CAF0,seed,500);
            for(unsigned i=0;i<8;++i) {
                unsigned n=word(area,0x56+i*2);
                if((area[0x66]&(1u<<i)) && n && n<=500 && valid(table(0x6CAF0,n,500)))
                    next.blocked[n]=true;
            }
        }
        // local19 diagnostics: one line whenever the protected region set changes (seedless bases = city hex without a region).
        bool changed=!cache.ready || cache.player!=next.player || memcmp(cache.blocked,next.blocked,sizeof next.blocked)!=0;
        if(changed) {
            unsigned count=0,first=0;
            for(unsigned i=1;i<=500;++i)if(next.blocked[i]){++count;if(!first)first=i;}
            log_event("catapult_ground_rebuilt",player,(next.bases<<24)|(next.seedless<<16)|count,next.seedless?next.last_seedless:first);
        }
        cache=next;
    }
    bool blocked=cache.blocked[region];
    ReleaseSRWLockExclusive(&catapult_ground_lock);
    return blocked;
}
static unsigned blocked_catapult_force(Obj force,Obj tile,Obj definition) { COST_SCOPE("ai_wall:blocked_catapult_force");
    if(!definition || definition!=table(0x73958,6,30))return 0;
    unsigned player=player_force();
    if(!player)return 0; // Observer mode has no player ground to protect.
    for(unsigned id=1;id<52;++id)
        if(force && force==table(0xDF30,id,51) && valid(force))
            return id!=player && catapult_player_ground(tile)?id:0;
    return 0;
}
static volatile LONG catapult_log_day[52];
static volatile LONG catapult_allow_log_day[53];
// local19 diagnostics: a catapult permission that was NOT blocked, once per force per day.
// a = force id (52 = force pointer not in the force table), b = region<<16 | 1 if the region is player ground | 2 if player.
static void log_catapult_allowed(Obj force,Obj tile,Obj definition) { COST_SCOPE("ai_wall:log_catapult_allowed");
    if(!definition || definition!=table(0x73958,6,30))return;
    unsigned id=52;
    for(unsigned i=1;i<52;++i)if(force && force==table(0xDF30,i,51)){id=i;break;}
    LONG stamp=static_cast<LONG>(game_day()+1);
    if(InterlockedExchange(&catapult_allow_log_day[id],stamp)==stamp)return;
    unsigned region=catapult_tile_area(tile),player=player_force();
    unsigned flags=(region && catapult_player_ground(tile)?1u:0u)|(id==player?2u:0u);
    log_event("ai_catapult_build_allowed",0,(id<<8)|flags,(region<<16)|(valid(tile)?fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F830))(tile)&0xFFFF:0xFFFF));
}
static bool __fastcall build_permission(Obj force, Obj tile, Obj definition, int check_policy) { COST_SCOPE("build_permission");
    unsigned owner = enabled && mod_features.ai_wall_block ? blocked_wall_force(force,definition) : 0;
    if (owner) {
        unsigned facility = definition == table(0x73958,9,30) ? 9 : 10;
        LONG stamp = static_cast<LONG>(game_day()+1);
        if (InterlockedExchange(&wall_log_day[owner][facility-9],stamp) != stamp)
            log_event("ai_wall_build_blocked",0,(owner<<8)|facility,
                      valid(tile) ? fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F830))(tile) : 48400);
        return false;
    }
    owner=enabled && mod_features.ai_catapult_block ? blocked_catapult_force(force,tile,definition) : 0;
    if(owner) {
        LONG stamp=static_cast<LONG>(game_day()+1);
        if(InterlockedExchange(&catapult_log_day[owner],stamp)!=stamp)
            log_event("ai_catapult_build_blocked",0,owner,fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F830))(tile));
        return false;
    }
    if(enabled && mod_features.ai_catapult_block)log_catapult_allowed(force,tile,definition);
    return original_build_permission(force,tile,definition,check_policy);
}
static bool cancel_wall_order(Obj army, Progress &p) { COST_SCOPE("ai_wall:cancel_wall_order");
    if (!computer_army(army) || (army[0x37] != 8 && army[0x37] != 9) ||
        (army[0x46] != 9 && army[0x46] != 10)) return false;
    unsigned facility = army[0x46], hex = word(army,0x42);
    fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
    fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,13);
    fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,255,0);
    army[0x46]=0;*reinterpret_cast<unsigned short *>(army+0x42)=48400;
    p.facility=0;p.construction_hex=48400;p.credit=false;p.build_chain=false;
    p.finished_build=false;
    fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(army);
    log_event("ai_wall_order_cancelled",army_id(army),facility,hex);
    return true;
}

static bool cancel_catapult_order(Obj army,Progress &p) { COST_SCOPE("ai_wall:cancel_catapult_order");
    if(!enabled || !mod_features.ai_catapult_block || !computer_army(army) ||
       (army[0x37]!=8 && army[0x37]!=9) || army[0x46]!=6)return false;
    unsigned hex=word(army,0x42);
    if(!blocked_catapult_force(table(0xDF30,army_force(army),51),table(0xE270,hex,48399),table(0x73958,6,30))) {
        static volatile LONG kept_log_day[52];   // local19 diagnostics: order left alone, once per force per day
        unsigned f=army_force(army);
        LONG stamp=static_cast<LONG>(game_day()+1);
        if(f<52 && InterlockedExchange(&kept_log_day[f],stamp)!=stamp)
            log_event("ai_catapult_order_kept",army_id(army),(f<<16)|catapult_tile_area(table(0xE270,hex,48399)),hex);
        return false;
    }
    fn<void(__fastcall *)(Obj)>(GA(GA_A3370))(army);
    fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(army,13);
    fn<void(__fastcall *)(Obj,int,int)>(GA(GA_2ECF00))(army,255,0);
    army[0x46]=0;*reinterpret_cast<unsigned short *>(army+0x42)=48400;
    p.facility=0;p.construction_hex=48400;p.credit=false;p.build_chain=false;
    p.finished_build=false;
    fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(army);
    log_event("ai_catapult_order_cancelled",army_id(army),6,hex);
    return true;
}

