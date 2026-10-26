/* Area+56: eight u16 adjacency IDs, Area+66: enabled adjacency bits.
 * Reuse the exact graph used by native 2B07C0. Tile+12 selects its area. */
static unsigned campaign_area_id(unsigned hex) {
    Obj tile=table(0xE270,hex,48399);
    unsigned id=valid(tile)?word(tile,0x12):0;
    return id && id<=500 && valid(table(0x6CAF0,id,500))?id:0;
}
// AI-CONTRACT [graph]: Adjacency is enabled Area edges, not geometric tile proximity. Preserve force hostility and valid-area filtering; an apparent route is not permission to issue a movement command.
static bool campaign_edge(unsigned from,unsigned to) {
    Obj area=table(0x6CAF0,from,500);if(!from || !to || !valid(area))return false;
    for(unsigned i=0;i<8;++i)if((area[0x66]&(1u<<i)) && word(area,0x56+i*2)==to)return true;
    return false;
}
static bool campaign_fire_avoid(Obj army) {(void)army;return false;}
static bool campaign_city_danger(Obj army,unsigned hex) {(void)army;(void)hex;return false;}
static bool campaign_region_hostile(Obj army,unsigned id) {
    Obj area=table(0x6CAF0,id,500);if(!valid(area))return false;
    Obj center=table(0xE270,word(area,0x3A),48399);if(!valid(center))return false;
    unsigned owner=center[0x14],own=army_force(army);
    return owner!=own && (!owner || fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(own,owner));
}
static bool campaign_region_open(Obj army,unsigned id) {
    Obj area=table(0x6CAF0,id,500);if(!valid(area))return false;
    Obj tile=table(0xE270,word(area,0x3A),48399);if(!valid(tile))return false;
    return !tile[0x14] || tile[0x14]==army_force(army) ||
        fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(army_force(army),tile[0x14]);
}
// Siege travel uses native paths; no source-to-city region graph or forced region centers.
static Obj campaign_guard(Obj army,unsigned region,unsigned goal) {
    for(unsigned i=1;i<=500;++i) {
        Obj other=table(0x7E838,i,500);
        if(other==army || !campaign_controlled(other) || army_force(other)!=army_force(army) || word(other,0x16)<1000 ||
           campaign_area_id(word(other,0x2A))!=region)continue;
        Progress *p=observe_capture(other);
        if(!p || p->returning || p->forced_return || p->facility || p->combat_target || p->morale_recovering ||
           game_day()-p->motion_day>=campaign_stationary_days || (p->guard_beneficiary &&
           (p->guard_beneficiary!=army_id(army) || p->guard_goal!=goal)))continue;
        const CampaignHit &hit=campaign_hits[i];
        if(hit.present && hit.root==world() && hit.victim==other && game_day()>=hit.day && game_day()-hit.day<=10)continue;
        return other;
    }
    return nullptr;
}
struct CampaignRoute {std::vector<unsigned> cells,regions;std::vector<Obj> guards;};
// 2D96C5 -> 269150 supplies native daily movement points. 2D970A
// charges BE120(2318B0(next tile)) for a hex, not one point per hex.
struct CampaignMovement {Obj army,formation;float daily,here;};
static CampaignMovement campaign_movement(Obj army) {
    Obj formation=fn<Obj(__fastcall *)(Obj)>(0x22B090)(army);
    return {army,formation,static_cast<float>(fn<int(__fastcall *)(Obj)>(0x269150)(army)),
        fn<float(__fastcall *)(Obj,Obj,int,int,Obj)>(0x2B0290)(army,formation,1,0,nullptr)};
}
static float campaign_tile_days(const CampaignMovement &m,unsigned hex) {
    Obj tile=table(0xE270,hex,48399);
    if(!(m.daily>0) || !(m.here>0) || !valid(tile))return 1.0e9f;
    float there=fn<float(__fastcall *)(Obj,Obj,int,int,Obj)>(0x2B0290)(m.army,m.formation,1,0,tile);
    Obj terrain=fn<Obj(__fastcall *)(Obj)>(0x2318B0)(tile);
    if(!(there>0) || !valid(terrain))return 1.0e9f;
    int cost=fn<int(__fastcall *)(Obj)>(0xBE120)(terrain);
    return cost>0?static_cast<float>(cost)*m.here/(m.daily*there):1.0e9f;
}
static float campaign_tile_days(Obj army,unsigned hex) {return campaign_tile_days(campaign_movement(army),hex);}
static unsigned campaign_relay_points(const std::vector<unsigned> &cells,unsigned end,unsigned *points) {
    if(cells.size()<2 || !end || end>=cells.size())return 0;
    unsigned count=(std::min)(5u,end);
    for(unsigned i=0;i<count;++i)points[i]=cells[((i+1)*end+count-1)/count];
    return count;
}
static __declspec(thread) std::vector<unsigned> *campaign_native_trace;
static bool campaign_generated_path(Obj army,const CampaignRoute &route) {
    if(native_search_depth)return false;
    fn<void(__fastcall *)(Obj)>(0x26C940)(army);
    return !route.cells.empty() && (word(army,0x2A)==route.cells.back() ||
        (word(army,0x48)<48400 && distance(word(army,0x2A),word(army,0x48))==1));
}
// Legacy API name retained for callers; the game alone searches the path.
// Probe on the existing AI thread, restore every command field afterwards.
static CampaignRoute campaign_safe_route(Obj army,unsigned target,bool shortcut,bool cohort=false) {
    COST_SCOPE("campaign_safe_route");
    if(campaign_castle_locked(army))return {};
    NativePhase phase("campaign_path_probe");
    (void)shortcut;(void)cohort;CampaignRoute result;unsigned from=word(army,0x2A);
    if(native_search_depth)return result;
    if(from>=48400 || target>=48400 || !valid(table(0xE270,target,48399)))return result;
    if(from==target) {result.cells.push_back(from);return result;}
    unsigned char saved[0x34];memcpy(saved,army+0x2C,sizeof(saved));
    unsigned flags=dword(army,0x1F0);unsigned short future=word(army,0x1F8);
    auto previous=campaign_native_trace;campaign_native_trace=&result.cells;
    fn<void(__fastcall *)(Obj)>(0xA2410)(army);
    fn<void(__fastcall *)(Obj,int)>(0x2EAAE0)(army,1);
    fn<void(__fastcall *)(Obj,int,int)>(0x2EAB40)(army,9,static_cast<int>(target));
    fn<void(__fastcall *)(Obj)>(0x26C940)(army);
    campaign_native_trace=previous;
    bool legal=word(army,0x48)<48400 && distance(from,word(army,0x48))==1 &&
        !result.cells.empty() && result.cells.front()==from && result.cells.back()==target;
    memcpy(army+0x2C,saved,sizeof(saved));*reinterpret_cast<unsigned *>(army+0x1F0)=flags;
    *reinterpret_cast<unsigned short *>(army+0x1F8)=future;
    if(!legal)return {};
    for(size_t i=1;i<result.cells.size();++i)if(distance(result.cells[i-1],result.cells[i])!=1)return {};
    for(unsigned h:result.cells) {unsigned r=campaign_area_id(h);
        if(r && std::find(result.regions.begin(),result.regions.end(),r)==result.regions.end())result.regions.push_back(r);}
    return result;
}
static bool campaign_route_order(Obj army,const CampaignRoute &route) {
    COST_SCOPE("campaign_route_order");
    if(campaign_castle_locked(army))return false;
    if(native_search_depth)return false;
    if(route.cells.empty())return false;unsigned target=route.cells.back();
    if(army[0x37]==1 && army[0x38]==9 && word(army,0x3A)==target &&
       (word(army,0x2A)==target || (word(army,0x48)<48400 && distance(word(army,0x2A),word(army,0x48))==1)))return true;
    unsigned char saved[0x34];memcpy(saved,army+0x2C,sizeof(saved));unsigned flags=dword(army,0x1F0);unsigned short future=word(army,0x1F8);
    fn<void(__fastcall *)(Obj)>(0xA2410)(army);
    fn<void(__fastcall *)(Obj,int)>(0x2EAAE0)(army,1);
    fn<void(__fastcall *)(Obj,int,int)>(0x2EAB40)(army,9,static_cast<int>(target));
    if(campaign_generated_path(army,route))return true;
    memcpy(army+0x2C,saved,sizeof(saved));*reinterpret_cast<unsigned *>(army+0x1F0)=flags;*reinterpret_cast<unsigned short *>(army+0x1F8)=future;return false;
}
static bool campaign_request_guard(Obj army,Progress &p,unsigned region,unsigned goal);
static bool campaign_assign_safe(Obj army,Progress &p,unsigned target,bool shortcut,bool plan=false) {
    COST_SCOPE("campaign_assign_safe");
    if(!plan && (p.campaign || p.regional_worker) && !p.companion_parent && !p.guard_requested) {
        unsigned crossing=campaign_path_capture_hex(army,target);
        if(crossing<48400 && !region_reserved(crossing,army))target=crossing;
    }
    CampaignRoute route=campaign_safe_route(army,target,shortcut,plan);
    if(route.cells.empty() || (!plan && !campaign_route_order(army,route)))return false;
    p.region_target=static_cast<unsigned short>(target);p.region_owner=table(0xE270,target,48399)[0x14];
    if(!plan) {
        *reinterpret_cast<unsigned *>(army+0x1F0)|=2;
        campaign_request_companion(army,p,target);
        for(unsigned region:route.regions)if(region!=campaign_area_id(target) && campaign_region_hostile(army,region))
            campaign_request_guard(army,p,region,target);
    }
    log_event("campaign_native_direct_target",army_id(army),p.city,target);return true;
}

// Construction movement is also nonassault movement. Keep its native action
// and full work site while the first five relays describe the safe approach.
static bool campaign_build_order(Obj army,unsigned facility,unsigned hex) {
    if(native_search_depth)return false;
    unsigned char saved[0x34];memcpy(saved,army+0x2C,sizeof(saved));unsigned flags=dword(army,0x1F0);unsigned short future=word(army,0x1F8);
    fn<void(__fastcall *)(Obj)>(0xA2410)(army);
    fn<void(__fastcall *)(Obj,int)>(0x2EAAE0)(army,8);
    fn<void(__fastcall *)(Obj,int,int)>(0x2EAB40)(army,9,static_cast<int>(hex));
    army[0x46]=static_cast<unsigned char>(facility);*reinterpret_cast<unsigned short *>(army+0x42)=static_cast<unsigned short>(hex);
    CampaignRoute route;route.cells.push_back(hex);
    if(campaign_generated_path(army,route))return true;
    memcpy(army+0x2C,saved,sizeof(saved));*reinterpret_cast<unsigned *>(army+0x1F0)=flags;*reinterpret_cast<unsigned short *>(army+0x1F8)=future;return false;
}
