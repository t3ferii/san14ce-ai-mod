static std::map<Obj,unsigned> support_tile_ids;
static unsigned __fastcall support_hex(Obj tile) {return support_tile_ids.at(tile);}
static void __fastcall support_candidates(Obj,Obj,Obj,GameList *out,Obj,Obj,Obj) {
    list_add(*out,table(0xE270,22061,48399));
}
static Obj __fastcall support_building(Obj tile) {return table(0x6DA98,word(tile,0x1A),3000);}
static void campaign_support_tests() {
    Obj saved_root=root,saved_goal=goal,saved_area=area,saved_current=current_army;
    int saved_day=fixture_day,saved_money=money;bool saved_cap=cap_reached;unsigned saved_legal=legal_facilities;
    auto saved_progress=std::vector<Progress>(progress,progress+501);
    auto saved_hits=std::vector<CampaignHit>(campaign_hits,campaign_hits+501);
    Obj force=table(0xDF30,2,51);
    Obj band_definition=table(0x73958,7,30);
    root=static_cast<Obj>(calloc(1,0x90000));putptr(image,0x277DF18,root);putptr(root,0x871E8,player);slot(0xDF30,2,force);
    slot(0x73958,7,band_definition);
    memset(progress,0,sizeof(progress));memset(campaign_hits,0,sizeof(campaign_hits));fixture_day=100;
    goal=object(9);put16(goal,0x4E,9);put16(goal,0x3A,22074);*reinterpret_cast<unsigned *>(goal+0x3C)=10000;
    BIND(0x2AA850,revision_distance);BIND(0x22D740,revision_city_hex);BIND(0x22FB10,support_building);BIND(0xB9070,support_candidates);BIND(0x22DDA0,support_hex);
    area=object(1);put16(area,0x3A,22060);put16(area,0x56,2);area[0x66]=1;slot(0x6CAF0,1,area);
    Obj c=object(2);put16(c,0x3A,22074);put16(c,0x56,1);put16(c,0x58,3);c[0x66]=3;c[0x34]=1;slot(0x6CAF0,2,c);
    Obj b=object(3);put16(b,0x3A,22084);put16(b,0x56,2);b[0x66]=1;slot(0x6CAF0,3,b);
    for(unsigned y=94;y<=106;++y)for(unsigned x=44;x<=104;++x) {
        unsigned h=y*220+x;Obj tile=object(h);tile[0x10]=1;tile[0x14]=x<72?2:3;
        put16(tile,0x12,x<72?1:x<82?2:3);slot(0xE270,h,tile);support_tile_ids[tile]=h;
    }
    Obj a=object(1),ally=object(2),enemy=object(3),distant=object(4),groups[5]={};
    for(unsigned id=1;id<=4;++id) {
        Obj unit=id==1?a:id==2?ally:id==3?enemy:distant;army_ids[unit]=id;slot(0x7E838,id,unit);
        unit[0x150]=id==3?1:2;unit[0x1A]=100;put16(unit,0x12,id);put16(unit,0x16,id==3?2000:id==4?50000:9000);
        put16(unit,0x2A,id==1?22060:id==2?22068:id==3?22075:22100);put16(unit,0x58,id);
        groups[id]=object(id);put16(groups[id],0x14,10);put16(groups[id],0x16,9);slot(0x7F8D8,id,groups[id]);slot(0x158,id,object(id));
        mock_clear(unit);mock_action(unit,1);mock_target(unit,9,22069);
        Progress *p=observe_capture(unit);remember_campaign(unit,*p);p->fire_policy_turn=10;p->fire_avoid=true;
    }
    Progress &p=progress[1];current_army=a;
    Obj band=object(22063);band[0x12]=7;band[0x13]=2;put16(band,0x14,1000);slot(0x6DA98,1,band);
    put16(groups[1],0x14,6);p.campaign=false;a[0x1A]=91;
    check(!campaign_morale_priority(a,p),"ordinary mission at 91 morale does not detour");
    a[0x1A]=90;put16(a,0x2C,22061);
    check(campaign_morale_priority(a,p) && p.morale_recovering && p.morale_band==1 && a[0x1A]==90,
        "90 morale visits an owned completed band within five hexes without changing morale");
    check(revision_distance(p.morale_hex,22063)<=1 && p.morale_hex!=22063,"recovery waits on a band-adjacent walkable tile");
    put16(a,0x2A,p.morale_hex);a[0x1A]=99;
    check(campaign_morale_priority(a,p) && p.morale_recovering && a[0x1A]==99,"recovery continues to 100 rather than ending at 99");
    a[0x1A]=100;
    check(campaign_morale_priority(a,p) && !p.morale_recovering && a[0x37]==1 && a[0x38]==9 && word(a,0x3A)==22069 && word(a,0x2C)==48400,
        "full morale resumes the original typed mission and discards stale pre-detour waypoints");
    put16(a,0x2A,22060);a[0x1A]=90;campaign_morale_priority(a,p);put16(groups[1],0x16,8);a[0x1A]=100;
    check(!campaign_morale_priority(a,p) && !p.morale_saved,"a new group goal cancels old recovery even at completion");
    put16(groups[1],0x16,9);mock_action(a,1);mock_target(a,9,22069);a[0x1A]=90;band[0x13]=1;legal_facilities=0;
    check(!campaign_morale_priority(a,p),"enemy music band cannot be used for recovery");band[0x13]=2;put16(band,0x14,999);
    check(!campaign_morale_priority(a,p),"unfinished music band cannot supply a recovery effect");put16(band,0x14,1000);
    put16(band,0x10,22066);
    check(!campaign_morale_priority(a,p),"ordinary mission does not detour to a band outside five hexes");
    a[0x1A]=70;
    check(campaign_morale_priority(a,p) && p.morale_recovering,"70 morale can use a farther band along its route");
    band[0x13]=1;cap_reached=true;
    check(campaign_morale_priority(a,p) && !p.morale_recovering,"destroyed or captured recovery band releases the detour");
    money=10000;cap_reached=false;legal_facilities=1u<<7;put16(a,0x2A,22060);a[0x1A]=92;
    table(0xE270,22060,48399)[0x10]=10;
    check(campaign_morale_priority(a,p) && p.morale_building && p.facility==7 && a[0x1A]==92,
        "high mountain triggers priority legal music-band construction even above 90 morale");
    check(a[0x37]==8 && a[0x46]==7 && word(a,0x42)==22061,"band construction uses the original native build action and site");
    a[0x1A]=100;
    check(campaign_morale_priority(a,p) && p.morale_building && p.facility==7,"morale reaching 100 during construction does not abandon the unfinished recovery band");
    a[0x1A]=92;
    Obj built=object(22061);built[0x12]=7;built[0x13]=2;put16(built,0x14,1000);slot(0x6DA98,2,built);put16(table(0xE270,22061,48399),0x1A,2);
    put16(groups[1],0x14,7);table(0xE270,22060,48399)[0x10]=1;
    check(campaign_morale_priority(a,p) && !p.morale_building && p.morale_band==2 && word(groups[1],0x14)==6,
        "completed recovery construction restores ordinary group assignment and stays for real recovery");
    a[0x1A]=100;campaign_morale_priority(a,p);a[0x1A]=70;put16(a,0x16,999);
    check(!campaign_morale_priority(a,p),"fewer than 1000 survivors retain retreat priority over morale detours");
    put16(a,0x16,9000);a[0x1A]=100;put16(groups[1],0x14,10);remember_campaign(a,p);
    CampaignStrength strength=campaign_strength(a,22074);
    check(strength.own==18000 && strength.enemy==12000,"15-hex totals include hostile city and field troops and exclude a farther friendly army");
    p.siege_city=0;progress[2].siege_city=0;*reinterpret_cast<unsigned *>(goal+0x3C)=1000000;
    put16(a,0x2A,22060);put16(ally,0x2A,22068);mock_clear(a);mock_clear(ally);mock_action(ally,13);mock_target(ally,255,0);
    p.region_target=48400;region_tiles={table(0xE270,22084,48399)};
    check(campaign_request_guard(a,p,2,22084) && progress[2].guard_requested,
        "hostile city-region transit actively recruits an available nearby protector");
    check(!campaign_safe_route(a,22084,true).cells.empty(),"primary keeps native direct B route while protector travels to C");
    check(campaign_transit_relay(a,p,2,22084,false) && campaign_area_id(p.region_target)==2 && p.region_target!=22074,
        "intermediate city uses a transit tile instead of its castle center");
    put16(ally,0x2A,progress[2].guard_hex);table(0xE270,word(ally,0x2A),48399)[0x14]=2;
    check(campaign_guard_priority(ally,progress[2]) && word(ally,0x3A)==word(ally,0x2A),"arrived protector holds an actual C-region tile");
    unsigned main_hex=word(a,0x2A);put16(a,0x2A,22084);fixture_day+=20;
    check(campaign_guard_priority(ally,progress[2]) && progress[2].guard_requested,
          "C tile guard remains needed after primary crosses C and reaches still-hostile B, including after twenty days");
    put16(a,0x2A,main_hex);fixture_day-=20;
    auto all_c=campaign_transit_tiles(ally,2,22084);
    check(std::find(all_c.begin(),all_c.end(),22074u)!=all_c.end(),"C castle-adjacent tiles are valid automatic-recapture protection tiles");
    Obj remote=object(24272);put16(remote,0x12,2);remote[0x14]=3;slot(0xE270,24272,remote);
    all_c=campaign_transit_tiles(ally,2,22084);
    check(std::find(all_c.begin(),all_c.end(),24272u)!=all_c.end(),"all C tiles qualify, including one distant from the exact A-B road");
    progress[2].morale_recovering=true;
    check(!campaign_safe_route(a,22084,true).cells.empty(),"primary route is independent of protector recovery status");
    progress[2].morale_recovering=false;
    check(campaign_assign_safe(a,p,22084,true),"physically present C protector permits A-C-B capture route");
    progress[2].guard_beneficiary=0;progress[2].guard_requested=false;mock_action(ally,4);mock_target(ally,5,9);
    check(campaign_assign_safe(a,p,22084,true) && ally[0x37]==4 && !progress[2].guard_beneficiary,
        "an active castle assault already occupying C protects transit without losing its siege command");
    progress[2].guard_beneficiary=1;progress[2].guard_requested=true;
    table(0xE270,22084,48399)[0x14]=2;
    check(!campaign_guard_priority(ally,progress[2]) && !progress[2].guard_beneficiary,"completed B capture releases requested protector");
    Obj fort=object(22059),camp=object(22064);
    for(Obj f:{fort,camp}) {f[0x13]=2;put16(f,0x14,1000);}
    fort[0x12]=4;camp[0x12]=3;slot(0x6DA98,3,fort);slot(0x6DA98,4,camp);
    check(campaign_defensive_wait(a,p) && revision_distance(p.defensive_hex,22059)<=3 && revision_distance(p.defensive_hex,22064)<=3,
        "blocked primary moves into overlapping complete fort and camp defense radii");
    unsigned avoiding=0;
    for(unsigned i=0;i<1000;++i) {
        fixture_day=100+i*10;bool choice=campaign_fire_avoid(a);if(choice)++avoiding;
        check(campaign_fire_avoid(a)==choice,"all path probes share one fire-avoidance decision in the same turn");
    }
    check(avoiding==0,"castle fire avoidance remains disabled across all turns");
    region_tiles.clear();root=saved_root;goal=saved_goal;area=saved_area;current_army=saved_current;fixture_day=saved_day;
    money=saved_money;cap_reached=saved_cap;legal_facilities=saved_legal;putptr(image,0x277DF18,root);
    std::copy(saved_progress.begin(),saved_progress.end(),progress);std::copy(saved_hits.begin(),saved_hits.end(),campaign_hits);
    BIND(0x2AA850,mock_distance);BIND(0x22D740,mock_goal_hex);BIND(0x22FB10,mock_building);BIND(0xB9070,mock_candidates);BIND(0x22DDA0,mock_id);
}
