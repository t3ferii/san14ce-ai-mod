/* Transit through a still-hostile city region uses a tile, never its castle
 * center. A recruited protector must physically arrive before any shortcut. */
static std::vector<unsigned> campaign_transit_tiles(Obj army,unsigned region,unsigned goal) {
    (void)goal;std::vector<unsigned> result;unsigned here=word(army,0x2A);
    if(!valid(table(0x6CAF0,region,500)) || here>=48400)return result;
    // Every tile in C qualifies, irrespective of its distance to the castle,
    // beneficiary, or beneficiary's exact path. No two-hex exclusion.
    for(unsigned h=0;h<48400;++h) {
        Obj tile=table(0xE270,h,48399);if(!valid(tile) || word(tile,0x12)!=region)continue;
        unsigned owner=tile[0x14],own=army_force(army);
        if(!owner || owner==own || fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(own,owner))result.push_back(h);
    }
    std::sort(result.begin(),result.end(),[here](unsigned a,unsigned b){int da=distance(here,a),db=distance(here,b);return da!=db?da<db:a<b;});
    return result;
}
static bool campaign_transit_relay(Obj army,Progress &p,unsigned region,unsigned goal,bool plan) {
    auto tiles=campaign_transit_tiles(army,region,goal);unsigned attempts=0;
    for(unsigned h:tiles) {
        if(campaign_assign_safe(army,p,h,false,plan))return true;
        if(++attempts==12)break;
    }
    return false;
}
#ifdef SORTIE_FORMATION
static bool campaign_request_reinforcements(Obj army,Progress &p,uint64_t needed,unsigned guard_region,unsigned guard_goal);
#endif
static bool campaign_request_guard(Obj army,Progress &p,unsigned region,unsigned goal) {
    if(!mod_features.support)return false;   // [combat] support
    Obj center=table(0xE270,goal,48399);if(!valid(center) || center[0x14]==army_force(army))return false;
    Obj winner=nullptr;unsigned target=48400;float best=1.0e9f;CampaignRoute chosen;
    for(unsigned id=1;id<=500;++id) {
        Obj other=table(0x7E838,id,500);Progress &g=progress[id];
        if(g.guard_requested && g.guard_beneficiary==army_id(army) && g.guard_region==region && g.guard_goal==goal &&
           g.root==world() && g.army==other && campaign_controlled(other) && g.commander==word(other,0x12) &&
           !g.forced_return && !g.combat_target && !g.combat_city && word(other,0x16)>=1000)return true;
    }
    for(unsigned id=1;id<=500;++id) {
        Obj other=table(0x7E838,id,500);
        if(other==army || campaign_group_member(other) || !campaign_controlled(other) || army_force(other)!=army_force(army) || word(other,0x16)<1000 ||
           word(other,0x2A)>=48400 || (campaign_area_id(word(other,0x2A))!=region && distance(word(other,0x2A),word(army,0x2A))>15))continue;
        Progress *g=observe_capture(other);if(!g || g->regional_worker || g->credit || g->build_chain || g->forced_return || g->combat_target || g->combat_city || g->facility ||
            g->morale_recovering || g->guard_beneficiary || g->companion_parent || priority_mission(other,*g))continue;
        if(campaign_area_id(word(other,0x2A))==region && other[0x37]==4 && other[0x38]==5)return true;
        CampaignMovement movement=campaign_movement(other);
        // Travel-cost search selects a C tile only. Orders and actual paths
        // still come exclusively from native 26C940/1843E0.
        using Entry=std::pair<float,unsigned>;
        std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> queue;
        std::vector<float> costs(48400,1.0e9f);unsigned start=word(other,0x2A);
        costs[start]=0;queue.push({0.0f,start});
        while(!queue.empty()) {
            float lower=queue.top().first;unsigned h=queue.top().second;queue.pop();
            if(lower!=costs[h])continue;if(lower>=best)break;
            if(campaign_area_id(h)==region) {
                auto route=campaign_safe_route(other,h,true);
                if(!route.cells.empty()) {
                    float days=0;for(size_t k=1;k<route.cells.size();++k)days+=campaign_tile_days(movement,route.cells[k]);
                    if(days<best) {best=days;winner=other;target=h;chosen=route;}
                }
            }
            for(unsigned d=0;d<6;++d) {
                unsigned n=sortie::neighbor(h,d);if(n>=48400)continue;
                float cost=lower+campaign_tile_days(movement,n);
                if(cost<best && cost<costs[n]) {costs[n]=cost;queue.push({cost,n});}
            }
        }
        if(best==0)break;
    }
    if(winner && campaign_route_order(winner,chosen)) {
        Progress &g=progress[army_id(winner)];g.guard_beneficiary=static_cast<unsigned short>(army_id(army));
        g.guard_goal=static_cast<unsigned short>(goal);g.guard_hex=static_cast<unsigned short>(target);
        g.guard_region=static_cast<unsigned short>(region);g.guard_city=p.city;g.guard_requested=true;g.guard_day=game_day();
        log_event("campaign_guard_requested",army_id(army),army_id(winner),target);return true;
    }
#ifdef SORTIE_FORMATION
    return campaign_request_reinforcements(army,p,1000,region,goal);
#else
    return false;
#endif
}
static bool campaign_guard_priority(Obj army,Progress &p) {
    if(!p.guard_requested)return false;
    Obj beneficiary=table(0x7E838,p.guard_beneficiary,500),goal=table(0xE270,p.guard_goal,48399);
    Progress &b=progress[p.guard_beneficiary];
    bool needed=campaign_controlled(beneficiary) && army_force(beneficiary)==army_force(army) && b.army==beneficiary &&
        b.root==world() && b.commander==word(beneficiary,0x12) && !b.forced_return && b.city==p.guard_city &&
        valid(goal) && goal[0x14]!=army_force(army) && word(army,0x16)>=1000;
    if(!needed) {log_event("campaign_guard_released",army_id(army),p.guard_beneficiary,p.guard_region);p.guard_requested=false;p.guard_beneficiary=0;return false;}
    if(campaign_area_id(word(army,0x2A))==p.guard_region) {
        p.guard_hex=word(army,0x2A);
        CampaignRoute hold;hold.cells.push_back(p.guard_hex);return campaign_route_order(army,hold);
    }
    CampaignRoute route=campaign_safe_route(army,p.guard_hex,true);
    if(!route.cells.empty() && route.guards.empty() && campaign_route_order(army,route))return true;
    // Failed assistance is released, allowing another real helper next time.
    p.guard_requested=false;p.guard_beneficiary=0;return false;
}
static bool campaign_defensive_wait(Obj army,Progress &p) {
    struct Fort {unsigned hex;};std::vector<Fort> forts;unsigned here=word(army,0x2A),own=army_force(army);
    for(unsigned id=1;id<=3000;++id) {
        Obj b=table(0x6DA98,id,3000);
        if(!valid(b) || b[0x13]!=own || (b[0x12]!=3 && b[0x12]!=4) || word(b,0x10)>=48400 || distance(here,word(b,0x10))>15)continue;
        int maximum=fn<int(__fastcall *)(Obj)>(0x2AC120)(b);
        if(maximum>0 && word(b,0x14)>=maximum)forts.push_back({word(b,0x10)});
    }
    if(forts.empty())return false;
    struct Spot {unsigned hex,buffs;int range;};std::vector<Spot> spots;
    int cx=static_cast<int>(here%220),cy=static_cast<int>(here/220);
    for(int y=(std::max)(0,cy-12);y<=(std::min)(219,cy+12);++y)
        for(int x=(std::max)(0,cx-12);x<=(std::min)(219,cx+12);++x) {
            unsigned h=static_cast<unsigned>(y*220+x);Obj t=table(0xE270,h,48399);
            if(!valid(t) || t[0x14]!=own || word(t,0x1A) || campaign_city_danger(army,h))continue;
            unsigned buffs=0;for(const Fort &fort:forts)if(distance(h,fort.hex)<=3)++buffs;
            if(buffs)spots.push_back({h,buffs,distance(here,h)});
        }
    std::sort(spots.begin(),spots.end(),[](const Spot &a,const Spot &b){
        if(a.buffs!=b.buffs)return a.buffs>b.buffs;return a.range!=b.range?a.range<b.range:a.hex<b.hex;
    });
    unsigned attempts=0;
    for(const Spot &spot:spots) {
        CampaignRoute route=campaign_safe_route(army,spot.hex,true);
        if(!route.cells.empty() && route.guards.empty() && campaign_route_order(army,route)) {
            if(p.defensive_hex!=spot.hex)log_event("campaign_defensive_wait",army_id(army),spot.buffs,spot.hex);
            p.defensive_hex=static_cast<unsigned short>(spot.hex);return true;
        }
        if(++attempts==12)break;
    }
    return false;
}
