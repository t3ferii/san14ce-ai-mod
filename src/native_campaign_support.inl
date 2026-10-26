/* Existing AI caller thread and policy_lock only. No morale writes: recovery
 * comes from a real owned music band. Reuse native construction and safe paths. */
static bool campaign_mountain(unsigned hex) {
    Obj tile=table(0xE270,hex,48399);
    return valid(tile) && (tile[0x10]==9 || tile[0x10]==10 || tile[0x10]==19);
}
static void campaign_morale_save(Obj army,Progress &p) {
    if(p.morale_saved)return;
    memcpy(p.morale_order,army+0x2C,sizeof(p.morale_order));p.morale_flags=dword(army,0x1F0);
    Obj group=table(0x7F8D8,word(army,0x58),500);
    p.morale_group_mode=valid(group)?word(group,0x14):0;
    p.morale_group_goal=valid(group)?word(group,0x16):0;
    p.morale_saved=true;
}
static void campaign_morale_finish(Obj army,Progress &p) {
    if(p.morale_saved) {
        fn<void(__fastcall *)(Obj)>(0xA2410)(army);
        memcpy(army+0x2C,p.morale_order,sizeof(p.morale_order));
        // Old waypoints start at the pre-detour hex. Replan from the band's
        // position to the original typed target instead of walking backward.
        for(unsigned i=0;i<5;++i)*reinterpret_cast<unsigned short *>(army+0x2C+i*2)=48400;
        *reinterpret_cast<unsigned *>(army+0x1F0)=p.morale_flags;
        fn<void(__fastcall *)(Obj)>(0x26C940)(army);
    }
    p.morale_recovering=false;p.morale_saved=false;p.morale_building=false;
    p.morale_band=0;p.morale_hex=48400;
    log_event(army[0x1A]>=100?"morale_recovered_resume":"morale_detour_cancelled",army_id(army),army[0x1A],p.city);
}
static bool campaign_band(Obj army,unsigned id,bool complete=true) {
    Obj b=table(0x6DA98,id,3000);
    if(!valid(b) || b[0x12]!=7 || b[0x13]!=army_force(army) || !word(b,0x14) || word(b,0x10)>=48400)return false;
    int maximum=fn<int(__fastcall *)(Obj)>(0x2AC120)(b);
    return maximum>0 && (!complete || word(b,0x14)>=maximum);
}
static CampaignRoute campaign_band_route(Obj army,unsigned band_hex) {
    COST_SCOPE("campaign_band_route");
    CampaignRoute best;size_t length=48401;
    unsigned here=word(army,0x2A);
    if(distance(here,band_hex)<=1 && here!=band_hex && !campaign_mountain(here) && !campaign_city_danger(army,here)) {
        best.cells.push_back(here);return best;
    }
    for(unsigned d=0;d<6;++d) {
        unsigned hex=sortie::neighbor(band_hex,d);Obj tile=table(0xE270,hex,48399);
        if(!valid(tile) || tile[0x14]!=army_force(army) || campaign_mountain(hex))continue;
        CampaignRoute route=campaign_safe_route(army,hex,true);
        // Recovery must not borrow an unassigned logistics guard.
        if(!route.cells.empty() && route.guards.empty() && route.cells.size()<length) {length=route.cells.size();best=std::move(route);}
    }
    return best;
}
static unsigned campaign_morale_goal(Obj army,const Progress &p) {
    if(p.campaign) {Obj c=campaign_goal(p.city);if(c)return fn<unsigned(__fastcall *)(Obj)>(0x22D740)(c);}
    if(army[0x38]==9 && word(army,0x3A)<48400)return word(army,0x3A);
    for(unsigned i=0;i<5;++i)if(word(army,0x2C+i*2)<48400)return word(army,0x2C+i*2);
    return word(army,0x2A);
}
static bool campaign_morale_priority(Obj army,Progress &p) {
    COST_SCOPE("campaign_morale_priority");
    if(!mod_features.morale)return false;   // [combat] morale
    if(campaign_castle_locked(army))return false;
    if(p.forced_return || word(army,0x16)<1000 || army[0x140] || army[0x37]==11 || army[0x37]==12)return false;
    unsigned morale=army[0x1A],here=word(army,0x2A);
    bool urgent=morale<=70 || campaign_mountain(here);
    for(unsigned i=0;i<5 && !urgent;++i) {
        unsigned h=word(army,0x2C+i*2);if(h<48400 && campaign_mountain(h))urgent=true;
    }
    if(!p.morale_recovering && (morale>=100 || (!urgent && morale>90)))return false;
    // A genuinely new group assignment supersedes a saved ordinary mission.
    Obj group=table(0x7F8D8,word(army,0x58),500);
    if(p.morale_saved && valid(group) &&
       (word(group,0x16)!=p.morale_group_goal || (word(group,0x14)!=p.morale_group_mode &&
        !(attack_mode(word(group,0x14)) && attack_mode(p.morale_group_mode)))) &&
       !(p.morale_building && word(group,0x14)==7)) {
        p.morale_recovering=false;p.morale_saved=false;p.morale_building=false;return false;
    }
    if(morale>=100 && !p.morale_building) {campaign_morale_finish(army,p);return true;}
    if(p.morale_building && p.facility==7) {
        if(continue_build(army,p))return true;
        p.morale_building=false;
        p.credit=p.morale_build_credit;p.build_chain=p.morale_build_chain;
        p.build_center=p.morale_build_center;p.cycle_mask=p.morale_build_cycle|(1u<<7);
        if(valid(group) && word(group,0x14)==7 && p.morale_group_mode) {
            *reinterpret_cast<unsigned short *>(group+0x14)=p.morale_group_mode;
            *reinterpret_cast<unsigned short *>(group+0x16)=p.morale_group_goal;
            p.campaign=attack_mode(p.morale_group_mode) && hostile_city(army,p.morale_group_goal);
            if(p.campaign)p.city=p.morale_group_goal;
        }
        if(morale>=100) {campaign_morale_finish(army,p);return true;}
    }
    CampaignRoute selected;unsigned chosen=0;size_t best=48401;
    unsigned goal=campaign_morale_goal(army,p);
    for(unsigned id=1;id<=3000;++id) {
        if(!campaign_band(army,id))continue;
        unsigned hex=word(table(0x6DA98,id,3000),0x10);int range=distance(here,hex);
        bool retained=p.morale_recovering && id==p.morale_band;
        if(!retained && range>5 && (!urgent || range>15 || distance(hex,goal)+range>distance(here,goal)+5))continue;
        CampaignRoute route=campaign_band_route(army,hex);
        if(!route.cells.empty() && (retained || route.cells.size()<best)) {
            selected=std::move(route);chosen=id;best=selected.cells.size();if(retained)break;
        }
    }
    if(chosen) {
        campaign_morale_save(army,p);
        if(!campaign_route_order(army,selected))return false;
        bool started=!p.morale_recovering || p.morale_band!=chosen;
        p.morale_recovering=true;p.morale_band=static_cast<unsigned short>(chosen);
        p.morale_hex=static_cast<unsigned short>(selected.cells.back());p.siege_city=0;
        if(started)log_event("morale_band_detour",army_id(army),morale,p.morale_hex);
        return true;
    }
    // Ordinary missions only visit existing bands. Urgent units build a band
    // before other facilities, through the original cost/site/permission checks.
    if((urgent || morale<=90) && !p.facility && !p.morale_building) {
        Obj area=army_area(army);
        if(valid(area) && !(local_facilities(army,area)&(1u<<7))) {
            campaign_morale_save(army,p);
            bool credit=p.credit,chain=p.build_chain;unsigned short center=p.build_center;
            unsigned cycle=p.cycle_mask;
            p.build_chain=true;p.build_center=word(area,0x3A);
            if(try_build(army,p,48400,7)) {
                p.morale_build_credit=credit;p.morale_build_chain=chain;p.morale_build_center=center;p.morale_build_cycle=cycle;
                p.morale_recovering=true;p.morale_building=true;p.morale_hex=p.construction_hex;p.morale_band=0;p.siege_city=0;
                log_event("morale_band_build",army_id(army),morale,p.construction_hex);return true;
            }
            p.credit=credit;p.build_chain=chain;p.build_center=center;
        }
    }
    // Destroyed/unreachable band: stop pinning the old detour and restore its
    // mission. Never hold forever at a position without a recovery effect.
    if(p.morale_recovering) {campaign_morale_finish(army,p);return true;}
    p.morale_saved=false;return false;
}
struct CampaignStrength {uint64_t own=0,enemy=0;};
static bool campaign_request_reinforcements(Obj army,Progress &p,uint64_t needed,unsigned guard_region=0,unsigned guard_goal=48400);
static std::vector<unsigned> campaign_enemy_bases(Obj army,unsigned city) {
    std::vector<unsigned> result;Obj goal=campaign_goal(city);
    Obj force=goal?fn<Obj(__fastcall *)(Obj)>(0x22C990)(goal):nullptr;
    if(!valid(force))return result;
    auto id=*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(force,0)+0x60);
    unsigned owner=id(force),center=fn<unsigned(__fastcall *)(Obj)>(0x22D740)(goal);
    Obj area=table(0x6CAF0,campaign_area_id(center),500);if(!valid(area))return result;
    for(unsigned i=0;i<8;++i) {
        unsigned region=word(area,0x56+i*2);Obj next=table(0x6CAF0,region,500);
        if(!(area[0x66]&(1u<<i)) || !region || !valid(next) || campaign_transit_region(region))continue;
        unsigned hex=word(next,0x3A);Obj tile=table(0xE270,hex,48399);
        if(valid(tile) && tile[0x14]==owner && std::find(result.begin(),result.end(),hex)==result.end())result.push_back(hex);
    }
    (void)army;return result;
}
static CampaignStrength campaign_strength(Obj army,unsigned center) {
    CampaignStrength result;unsigned own=army_force(army);
    for(unsigned id=1;id<=500;++id) {
        Obj a=table(0x7E838,id,500);if(!valid(a) || word(a,0x2A)>=48400 || distance(word(a,0x2A),center)>15)continue;
        unsigned force=army_force(a);
        if(force==own)result.own+=word(a,0x16);
        else if(fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(own,force))result.enemy+=word(a,0x16);
    }
    unsigned city=progress[army_id(army)].city;Obj goal=campaign_goal(city);
    if(goal && hostile_city(army,city))result.enemy+=dword(goal,0x3C);
    return result;
}
static CampaignRoute campaign_rally_route(Obj army,unsigned center) {
    COST_SCOPE("campaign_rally_route");
    CampaignRoute best;size_t length=48401;unsigned here=word(army,0x2A);
    if(distance(here,center)>=3 && distance(here,center)<=5) {best.cells.push_back(here);return best;}
    // Radius 3..5 keeps assembly outside castle fire. Enumerate a bounded hex
    // box, then use the existing adjacency/occupancy/native speed path checks.
    int cx=static_cast<int>(center%220),cy=static_cast<int>(center/220);
    std::vector<unsigned> candidates;
    for(int y=(std::max)(0,cy-5);y<=(std::min)(219,cy+5);++y)
        for(int x=(std::max)(0,cx-5);x<=(std::min)(219,cx+5);++x) {
            unsigned h=static_cast<unsigned>(y*220+x);int radius=distance(h,center);if(radius<3 || radius>5)continue;
            Obj t=table(0xE270,h,48399);if(!valid(t) || t[0x14]!=army_force(army) || campaign_mountain(h))continue;
            candidates.push_back(h);
        }
    std::sort(candidates.begin(),candidates.end(),[here](unsigned a,unsigned b){return distance(here,a)<distance(here,b);});
    for(unsigned h:candidates) {
        CampaignRoute route=campaign_safe_route(army,h,true);
        if(!route.cells.empty() && route.guards.empty())return route;
    }
    (void)length;
    return best;
}
// Implemented after the sortie helpers; all native orders remain on AI callbacks.
static bool campaign_siege_priority(Obj army,Progress &p);
static unsigned campaign_source_reserve(Obj source,unsigned force) {(void)force;return valid(source)?campaign::reserve(dword(source,0x3C),0):0;}
