// Focused graph/actual-hit/monthly-growth tests on an isolated native world.
static int __fastcall revision_distance(int a,int b) {
    int ax=a%220,ay=a/220-(ax+(ax&1))/2,bx=b%220,by=b/220-(bx+(bx&1))/2;
    int dx=ax-bx,dy=ay-by;return (abs(dx)+abs(dy)+abs(dx+dy))/2;
}
static unsigned __fastcall revision_city_hex(Obj city) {return word(city,0x3A);}
static Obj revision_siege_goal;
static Obj __fastcall revision_home_goal(Obj,unsigned id) {return id==9?goal:id==8?revision_siege_goal:nullptr;}
static Obj __fastcall revision_home_owner(Obj city) {return city==revision_siege_goal?player:goal_owner;}
static uint64_t __fastcall revision_population(Obj city) {
    for(unsigned id=1;id<=500;++id) {
        Obj a=table(0x6CAF0,id,500);if(!valid(a) || a[0x35]!=word(city,0x4E))continue;
        int value=word(a,0x52)+static_cast<short>(word(a,0x70));
        put16(a,0x52,static_cast<unsigned>((std::max)(0,(std::min)(60000,value))));
    }
    return 0x123456789ABCDEF0ull;
}
static void campaign_revision_tests() {
    Obj saved_root=root,saved_goal=goal,revision_own_force=table(0xDF30,2,51);int saved_day=fixture_day;
    auto saved_progress=std::vector<Progress>(progress,progress+501);
    auto saved_hits=std::vector<CampaignHit>(campaign_hits,campaign_hits+501);
    root=static_cast<Obj>(calloc(1,0x90000));putptr(image,0x277DF18,root);putptr(root,0x871E8,player);
    goal=object(9);*reinterpret_cast<unsigned *>(goal+0x3C)=1000000;put16(goal,0x4E,9);put16(goal,0x3A,22074);fixture_day=100;
    memset(progress,0,sizeof(progress));memset(campaign_hits,0,sizeof(campaign_hits));
    BIND(0x2AA850,revision_distance);BIND(0x22D740,revision_city_hex);
    BIND(0x26C940,mock_finalize);BIND(0x2B0290,mock_route_speed);BIND(0x22B090,mock_active_formation);
    Obj areas[5]={};
    for(unsigned id=1;id<=4;++id) {areas[id]=object(id);slot(0x6CAF0,id,areas[id]);}
    put16(areas[1],0x3A,22050);put16(areas[2],0x3A,22060);put16(areas[3],0x3A,22070);
    put16(areas[4],0x3A,22074);areas[4][0x34]=1;
    for(unsigned id=1;id<=4;++id) {
        unsigned n=0;if(id>1)put16(areas[id],0x56+2*n++,id-1);
        if(id<4)put16(areas[id],0x56+2*n++,id+1);areas[id][0x66]=static_cast<unsigned char>((1u<<n)-1);
    }
    for(unsigned y=95;y<=105;++y)for(unsigned x=45;x<=78;++x) {
        unsigned h=y*220+x,id=x<56?1:x<65?2:x<74?3:4;
        Obj tile=object(h);put16(tile,0x12,id);tile[0x14]=id==1?2:3;slot(0xE270,h,tile);
    }
    Obj a=object(1),guard=object(2),enemy=object(3),closer=object(4);
    for(unsigned id=1;id<=4;++id) {
        Obj unit=id==1?a:id==2?guard:id==3?enemy:closer;
        army_ids[unit]=id;unit[0x1A]=100;slot(0x7E838,id,unit);unit[0x150]=id<3?2:3;
        put16(unit,0x12,id);put16(unit,0x16,5000);put16(unit,0x58,id);
        slot(0x158,id,object(id));Obj t=object(id);put16(t,0x14,10);put16(t,0x16,9);slot(0x7F8D8,id,t);
        put16(unit,0x2A,id==1?22050:id==2?22061:id==3?22054:22051);
    }
    // Hidden guards/enemies cannot satisfy a shortcut or obstruct its path.
    slot(0x7E838,2,nullptr);slot(0x7E838,3,nullptr);slot(0x7E838,4,nullptr);
    auto native_corridor=campaign_safe_route(a,22070,true,true);
    check(!native_corridor.cells.empty() && native_corridor.cells.back()==22070,
          "siege cohort keeps the original full target instead of forcing region centers");
    check(!campaign_safe_route(a,22070,true).cells.empty(),"ordinary enemy region traversal no longer requires a redundant city protector");
    region_tiles={table(0xE270,22060,48399),table(0xE270,22070,48399)};
    Progress *p=observe_capture(a);remember_campaign(a,*p);
    check(next_region(a,*p,22074)==1 && p->region_target==22060,"closest city-surrounding base is chosen directly");
    unsigned prev=word(a,0x2A);
    for(unsigned i=0;i<5;++i)check(word(a,0x2C+i*2)==48400,"direct native target contains no forced intermediate waypoint");(void)prev;
    check(word(a,0x3A)==22060,"independent capture endpoint remains the real regional target beyond five hexes");
    areas[2][0x34]=1;
    check(!campaign_safe_route(a,22070,true).cells.empty(),"native A-B route remains available before a C protector arrives");
    slot(0x7E838,2,guard);p->region_target=48400;
    check(campaign_assign_safe(a,*p,22070,true) && p->region_target==22070 && progress[2].guard_beneficiary==1,
        "present C army permits A-B capture and receives a real hold command");
    check(campaign_individual_priority(guard,progress[2]) && word(guard,0x3A)==22061,
        "C guard remains on its occupied tile while B is uncaptured");
    table(0xE270,22070,48399)[0x14]=2;
    check(!campaign_individual_priority(guard,progress[2]) && !progress[2].guard_beneficiary,"B ownership releases logistics guard");
    table(0xE270,22070,48399)[0x14]=3;p->region_target=48400;areas[2][0x34]=0;
    check(next_region(a,*p,22074,true)==1 && p->region_target==22060,"large army directly picks its closest city-surrounding capture goal");
    put16(goal,0x3A,22281); // city fire zone intersects the tempting straight road
    p->fire_policy_turn=fixture_day/10;p->fire_avoid=true; // test the avoidance half of the new policy
    CampaignRoute safe=campaign_safe_route(a,22070,true);
    check(!safe.cells.empty(),"original path is retained with castle avoidance disabled");
    check(!campaign_city_danger(a,22281),"castle two-hex exclusion is completely disabled");
    put16(goal,0x3A,22070);
    unsigned char original_order[0x34];memcpy(original_order,a+0x2C,sizeof(original_order));
    check(set_build(a,4,22070) && a[0x37]==8 && a[0x38]==9,
        "native construction is allowed inside the former castle exclusion radius");
    check(!campaign_safe_route(a,22070,true).cells.empty(),"native capture route may enter castle range");
    put16(goal,0x3A,22074);
    check(set_build(a,4,22052) && a[0x37]==8 && word(a,0x42)==22052 && word(a,0x2C)==48400,
        "construction preserves native action, work site and original path generation");
    a[0x46]=0;put16(a,0x42,48400);p->facility=0;
    areas[1][0x66]=0;
    check(!campaign_safe_route(a,22070,true,true).cells.empty(),
          "missing region adjacency no longer holds a siege army when the native path exists");
    areas[1][0x66]=1;
    areas[1][0x66]=0;
    check(!campaign_safe_route(a,22060,true).cells.empty(),"a missing graph edge does not wall off native-passable physically adjacent land");
    areas[1][0x66]=1;
    std::vector<unsigned> sample;
    for(unsigned i=0;i<=37;++i)sample.push_back(22050+i);
    unsigned points[5],n=campaign_relay_points(sample,37,points);
    check(n==5 && points[4]==22087 && points[0]>22051,"five samples span the full thirty-seven-hex path without a five-hex endpoint cap");
    for(unsigned i=1;i<5;++i)check(points[i]>points[i-1],"spaced samples stay ordered and unique");
    check(campaign_relay_points(sample,3,points)==3 && points[2]==22053,"a short path uses only its actual three unique relays");
    check(campaign_relay_points(sample,0,points)==0,"arrived armies do not receive self-point waypoint loops");
    slot(0x7E838,3,enemy);slot(0x7E838,4,closer);
    unsigned char pair[32]={},source[16]={};put16(source,0,3);*reinterpret_cast<unsigned *>(source+4)=27;
    putptr(pair,0,source);put16(pair+8,0,1);*reinterpret_cast<unsigned *>(pair+12)=27;
    original_damage=mock_damage;apply_actual_damage=false;combat_apply_damage(nullptr,pair,nullptr);
    check(!campaign_hits[1].present,"attempted attack with no actual troop loss does not invent engagement");
    unsigned before_hit_action=a[0x37],before_hit_kind=a[0x38],before_hit_target=word(a,0x3A);
    apply_actual_damage=true;combat_apply_damage(nullptr,pair,nullptr);
    check(!campaign_individual_priority(a,*p) && !p->combat_target && a[0x37]==before_hit_action && a[0x38]==before_hit_kind && word(a,0x3A)==before_hit_target,
        "actual damage records combat timing without adding interception or replacing the native mission");
    // Native 3B7D0's city scan selects actions 4/5 before resolving the
    // target and calling its district vtable slot. Reproduce that contract:
    // the previous action4/kind27 must fail even though both objects are valid.
    auto city_scan_safe=[](unsigned action,unsigned kind) {
        return action!=4 && action!=5 ? true : kind==(action==4?5u:6u);
    };
    check(!city_scan_safe(4,27),"old army target with city assault would enter the crashing native city scan");
    check(city_scan_safe(a[0x37],a[0x38]),"damage observation does not manufacture an invalid city or army target");
    fixture_day=101;put16(source,0,9);*reinterpret_cast<unsigned *>(source+4)=5;
    combat_apply_damage(nullptr,pair,nullptr);
    check(campaign_hits[1].source_city==9 && campaign_individual_priority(a,*p) &&
          p->combat_city==9 && p->forced_siege_city==9 && !p->combat_target && a[0x37]==4 && a[0x38]==5 && word(a,0x3A)==9,
          "actual castle damage commits only to its hostile castle with a valid city target kind");
    p->facility=7;p->credit=true;p->build_chain=true;p->morale_recovering=true;p->morale_building=true;
    mock_action(a,8);mock_target(a,9,22060);a[0x46]=7;
    check(apply_priority(a,nullptr,nullptr) && a[0x37]==4 && a[0x38]==5 && word(a,0x3A)==9 &&
          !p->facility && !p->build_chain && !p->morale_recovering && !p->morale_building,
          "committed castle attack cannot be overwritten by construction or morale recovery");
    put16(source,0,3);*reinterpret_cast<unsigned *>(source+4)=27;
    combat_apply_damage(nullptr,pair,nullptr);
    check(campaign_hits[1].source_city==9 && campaign_individual_priority(a,*p) && !p->combat_target && word(a,0x3A)==9,
          "subsequent army damage cannot erase or replace the castle-only attack target");
    check(p->city==9,"damage response preserves the strategic city separately from tactical target kind");
    CampaignRoute conflicting;conflicting.cells={word(a,0x2A),22060};
    p->credit=true;p->build_chain=true;a[0x1A]=50;
    check(!try_build(a,*p) && !try_cut(a) && !restore_cut(a,*p) && !try_destroy(a,*p) && !campaign_route_order(a,conflicting) && !campaign_morale_priority(a,*p) &&
        campaign_safe_route(a,22060,false).cells.empty() && a[0x37]==4 && a[0x38]==5 && word(a,0x3A)==9,
        "competing low-level construction, cut, route probe and morale writers cannot overwrite a committed castle order");
    a[0x1A]=100;p->credit=false;p->build_chain=false;
    put16(goal,0x4E,53);put16(source,0,53);*reinterpret_cast<unsigned *>(source+4)=6;
    p->forced_siege_city=0;p->combat_city=0;p->siege_city=0;
    combat_apply_damage(nullptr,pair,nullptr);
    check(campaign_individual_priority(a,*p) && a[0x37]==5 && a[0x38]==6 && word(a,0x3A)==53,
        "actual gate damage uses native gate action five and gate kind six, never a city or army mismatch");
    put16(goal,0x4E,9);p->city=9;
    p->forced_siege_city=0;p->forced_siege_reason=0;p->siege_city=0;campaign_hits[1].source_city=0;
    p->combat_city=0;p->combat_target=4;p->combat_commander=word(closer,0x12);
    put16(source,0,3);*reinterpret_cast<unsigned *>(source+4)=27;
    put16(closer,0x16,0);slot(0x7E838,4,nullptr);slot(0x7E838,3,nullptr);
    check(!campaign_individual_priority(a,*p) && !p->combat_target && p->city==9,
        "defeated target releases battle priority and retains strategic city");
    p->region_target=48400;check(next_region(a,*p,22074)==1,"capture resumes after the battle without replacing the strategic goal");
    slot(0x7E838,3,enemy);put16(a,0x16,1010);fixture_day=110;combat_apply_damage(nullptr,pair,nullptr);
    check(!campaign_individual_priority(a,*p) && word(a,0x16)==1000 && !p->forced_return,
        "exactly one thousand survivors keep their native mission after damage");
    Obj saved_origin=origin;origin=object(9);put16(origin,0x4E,9);put16(origin,0x3A,22050);
    put16(a,0x2A,22052);put16(a,0x16,1009);fixture_day=120;combat_apply_damage(nullptr,pair,nullptr);
    check(campaign_individual_priority(a,*p) && p->forced_return && a[0x37]==11 && a[0x38]==5 && word(a,0x3A)==9,
        "999 survivors receive native retreat command to origin city");
    put16(a,0x12,5);put16(a,0x16,6000);p=observe_capture(a);
    check(!p->forced_return && p->motion_day==120,"reused army identity does not inherit retreat or stationary history");
    put16(a,0x2A,22053);fixture_day=130;p=observe_capture(a);
    a[0x37]=1;a[0x38]=9;put16(a,0x3A,22053);
    fixture_day=169;observe_capture(a);check(!campaign_individual_priority(a,*p),"thirty-nine stationary days do not force retreat");
    fixture_day=170;observe_capture(a);
    check(campaign_individual_priority(a,*p) && p->forced_return && a[0x37]==11,
        "forty consecutive noncombat stationary days force retreat despite healthy troop count");
    fixture_day=180;put16(a,0x12,6);p=observe_capture(a);fixture_day=219;put16(a,0x2A,22054);observe_capture(a);
    fixture_day=220;observe_capture(a);check(!campaign_individual_priority(a,*p),"moving to another hex restarts the stationary turn count");
    slot(0x7E838,3,enemy);put16(enemy,0x2A,22055);
    for(unsigned activity=0;activity<7;++activity) {
        fixture_day=260+50*activity;put16(a,0x12,7+activity);p=observe_capture(a);
        p->motion_day=fixture_day-40;p->credit=false;p->build_chain=false;
        a[0x37]=1;a[0x38]=9;put16(a,0x3A,22054);
        if(activity==0) {a[0x37]=7;a[0x38]=27;put16(a,0x3A,3);}
        if(activity==1)p->morale_recovering=true;
        if(activity==2) {p->morale_recovering=true;p->morale_building=true;p->facility=7;}
        if(activity==3) {p->facility=4;p->credit=true;p->build_chain=true;}
        if(activity==4)campaign_hits[1]={root,a,enemy,word(a,0x12),word(enemy,0x12),2,fixture_day,true,0};
        if(activity==5) {a[0x37]=4;a[0x38]=5;put16(a,0x3A,9);}
        if(activity==6) {put16(goal,0x4E,52);a[0x37]=5;a[0x38]=6;put16(a,0x3A,52);}
        check(campaign_individual_priority(a,*p) && p->forced_return && a[0x37]==11 && p->motion_day==fixture_day-40,
            "forty unchanged-tile days retreat regardless of attack, morale, work or fresh damage");
        check(!p->morale_recovering && !p->morale_building,"stationary retreat clears recovery ownership so it cannot block return");
        put16(goal,0x4E,9);campaign_hits[1]={};
    }
    Obj previous_owner=goal_owner;goal_owner=revision_own_force;slot(0xDF30,2,revision_own_force);
    revision_siege_goal=object(8);put16(revision_siege_goal,0x4E,8);put16(revision_siege_goal,0x3A,22090);
    BIND(0x32C130,revision_home_goal);BIND(0x22C990,revision_home_owner);
    Obj home_group=table(0x7F8D8,word(a,0x58),500);put16(home_group,0x14,10);put16(home_group,0x16,8);
    *reinterpret_cast<unsigned *>(goal+0x3C)=4999;slot(0x7E838,3,enemy);put16(enemy,0x16,5000);put16(enemy,0x2A,22084);
    fixture_day=342;p->motion_day=fixture_day;p->combat_target=0;p->combat_city=0;p->siege_city=0;
    p->forced_return=false;p->returning=false;p->facility=0;p->morale_recovering=false;
    p->campaign=true;p->city=8;p->credit=false;p->build_chain=false;a[0x37]=1;a[0x38]=9;
    check(!campaign_individual_priority(a,*p) && !p->home_defense_city,"distant siege army twenty hexes away cannot intercept a threatened home");
    put16(a,0x2A,22074);p->motion_day=fixture_day;
    check(!campaign_individual_priority(a,*p) && !p->home_defense_city && a[0x37]==1,
          "even a nearby siege army cannot receive a removed custom home-interception order");
    *reinterpret_cast<unsigned *>(goal+0x3C)=5000;fixture_day=343;p->motion_day=fixture_day;
    check(!campaign_individual_priority(a,*p) && !p->home_defense_city,"equal enemy and friendly stock does not invent a source reserve or interception");
    *reinterpret_cast<unsigned *>(goal+0x3C)=4999;put16(enemy,0x2A,22085);fixture_day=344;p->motion_day=fixture_day;
    check(!campaign_individual_priority(a,*p),"enemy at eleven hexes does not trigger the ten-hex home defense condition");
    put16(enemy,0x2A,22084);fixture_day=345;a[0x37]=1;a[0x38]=9;p->campaign=false;
    check(!campaign_individual_priority(a,*p) && !p->home_defense_city,"ordinary deployed non-siege army does not become a relief interceptor");
    p->campaign=true;a[0x1B]=7;
    check(!campaign_individual_priority(a,*p) && !p->home_defense_city,"formation seven cannot receive proactive home interception");
    a[0x1B]=4;p->regional_worker=true;
    check(!campaign_individual_priority(a,*p),"capture worker cannot be recruited for home interception");p->regional_worker=false;
    put16(home_group,0x14,1);
    check(!campaign_individual_priority(a,*p),"native defensive group is never reinterpreted as an offensive siege cohort");
    put16(home_group,0x14,10);put16(a,0x2A,22063);a[0x37]=1;
    check(!campaign_individual_priority(a,*p),"army eleven hexes from home cannot receive local interception");
    put16(a,0x2A,22064);
    check(!campaign_individual_priority(a,*p),"enemy twenty hexes from army cannot lure it across the defended city");
    goal_owner=previous_owner;BIND(0x32C130,mock_goal);BIND(0x22C990,mock_goal_force);
    // Culture is already part of native deltas: small/large positive deltas
    // both scale fourfold; losses, unrelated cities and the native cap stay intact.
    original_population_growth=revision_population;
    for(unsigned id=1;id<=4;++id) {areas[id][0x35]=id==4?8:9;put16(areas[id],0x52,id==3?59990:1000);}
    put16(areas[1],0x70,10);put16(areas[2],0x70,25);put16(areas[3],0x70,100);put16(areas[4],0x70,20);
    check(population_growth(goal)==0x123456789ABCDEF0ull,"monthly growth preserves native 64 bit return");
    check(word(areas[1],0x52)==1040 && word(areas[2],0x52)==1100,"normal and culture-boosted monthly increases both multiply by four");
    check(word(areas[3],0x52)==60000 && word(areas[4],0x52)==1000,"population cap and other city populations preserved");
    put16(areas[1],0x70,static_cast<unsigned>(-10));put16(areas[2],0x70,0);population_growth(goal);
    check(word(areas[1],0x52)==1030 && word(areas[2],0x52)==1100,"population losses and zero increase are never multiplied");
    enabled=0;put16(areas[1],0x70,10);population_growth(goal);check(word(areas[1],0x52)==1040,"disabled growth hook forwards unchanged native behavior");enabled=1;
    region_tiles.clear();origin=saved_origin;root=saved_root;goal=saved_goal;fixture_day=saved_day;putptr(image,0x277DF18,root);
    std::copy(saved_progress.begin(),saved_progress.end(),progress);std::copy(saved_hits.begin(),saved_hits.end(),campaign_hits);
    BIND(0x2AA850,mock_distance);BIND(0x22D740,mock_goal_hex);
}
