static Obj __fastcall priority_area(Obj tile) {return table(0x6CAF0,word(tile,0x12),500);}
static int __fastcall priority_city_area(Obj a) {return valid(a) && a[0x34]==1;}
static int __fastcall priority_port_area(Obj a) {return valid(a) && a[0x34]==2;}
static bool priority_empty_candidates;
static unsigned priority_site;
static void __fastcall priority_candidates(Obj,Obj,Obj,GameList *out,Obj,Obj,Obj) {
    if(!priority_empty_candidates)list_add(*out,table(0xE270,priority_site,48399));
}
static void __fastcall priority_nearby(Obj center,int radius,GameList *out) {
    for(unsigned h=22048;h<22079;++h) {Obj t=table(0xE270,h,48399);
        if(valid(t) && revision_distance(static_cast<int>(word(center,0x10)),static_cast<int>(h))<=radius)list_add(*out,t);}
}
static Obj __fastcall priority_building(Obj tile) {return table(0x6DA98,word(tile,0x1A),3000);}
static Obj priority_search_army;
static unsigned priority_search_calls;
static bool priority_search_blocked;
static int __fastcall priority_reentrant_search(SortieNativePath *,Obj,unsigned,unsigned *to,int,int,int) {
    ++priority_search_calls;
    unsigned before=finalized;SortieOrder saved=sortie_order(priority_search_army);
    CampaignRoute intended;intended.cells={word(priority_search_army,0x2A),*to};
    priority_search_blocked=native_search_depth==1 &&
        campaign_safe_route(priority_search_army,*to,false).cells.empty() &&
        !campaign_route_order(priority_search_army,intended) &&
        !campaign_build_order(priority_search_army,4,*to) &&
        !sortie_chain_order(priority_search_army,to,1) &&
        !apply_priority(priority_search_army,nullptr,nullptr);
    sortie_move(priority_search_army,*to);
    SortieOrder after=sortie_order(priority_search_army);
    priority_search_blocked=priority_search_blocked && before==finalized &&
        memcmp(saved.bytes,after.bytes,sizeof(saved.bytes))==0 &&
        saved.flags==after.flags && saved.future==after.future;
    return 0;
}
static void campaign_priority_tests(Obj actor,Obj home_area,Obj neighbor,Obj target_area) {
    auto bindings=functions;auto states=std::vector<Progress>(progress,progress+501);
    unsigned soldier=word(actor,0x16),morale=actor[0x1A];Obj group=object(159);slot(0x7F8D8,159,group);
    put16(actor,0x58,159);put16(group,0x14,10);put16(group,0x16,2);actor[0x1A]=100;
    BIND(0x229EA0,priority_area);BIND(0x232680,priority_city_area);BIND(0x232BF0,priority_port_area);
    BIND(0xB9070,priority_candidates);BIND(0x2A7AA0,priority_nearby);BIND(0x22FB10,priority_building);
    SortieSearch saved_search=original_sortie_search;original_sortie_search=priority_reentrant_search;
    priority_search_army=actor;priority_search_calls=0;priority_search_blocked=false;
    SortieNativePath ignored={};unsigned nested_target=22064;
    check(!sortie_search(&ignored,nullptr,word(actor,0x2A),&nested_target,1,1,0) &&
        priority_search_calls==1 && priority_search_blocked && native_search_depth==0,
        "an active native search rejects nested mod probes and orders without changing army fields or search buffers");
    original_sortie_search=saved_search;
    unsigned old_legal=legal_facilities;bool old_cap=cap_reached;legal_facilities=~0u;cap_reached=false;
    Obj second=object(14),third=object(15),port=object(16);
    put16(second,0x3A,22064);put16(second,0x56,15);second[0x66]=1;
    put16(third,0x3A,22066);put16(port,0x3A,22058);port[0x34]=2;
    slot(0x6CAF0,14,second);slot(0x6CAF0,15,third);slot(0x6CAF0,16,port);
    put16(neighbor,0x58,14);neighbor[0x66]=3;
    for(unsigned h=22050;h<=22074;++h) {Obj t=table(0xE270,h,48399);
        put16(t,0x12,h<22056?10:h<22062?12:h<22066?14:h<22072?15:13);t[0x14]=h<22056?2:1;}
    auto choices=campaign_home_bases(actor,2);
    check(choices.size()==2 && choices[0]==22060 && choices[1]==22064,
        "home direct neighbors precede second-ring capture and exclude third-ring expansion");
    table(0xE270,22060,48399)[0x14]=2;choices=campaign_home_bases(actor,2);
    check(choices.size()==1 && choices[0]==22064,"owned first-ring base still exposes its hostile second-ring neighbor");
    put16(actor,0x16,1000);progress[59]={};Progress *p=observe_capture(actor);
    p->regional_worker=true;p->work_source=2;p->region_target=22064;p->region_owner=1;
    table(0xE270,22064,48399)[0x14]=2;table(0xE270,22065,48399)[0x14]=2;
    put16(actor,0x2A,22064);observe_capture(actor);
    check(p->credit && p->build_chain && !p->campaign,"independent worker observes mission capture between callbacks outside an offensive campaign");
    BIND(0xBF7A40,mock_blackboard);BIND(0xB26470,mock_variable);
    Obj saved_current=current_army;current_army=actor;unsigned worker_original=original_nodes;
    Progress worker_state=*p;SortieOrder worker_order=sortie_order(actor);
    retreat_node(actor,actor,actor);
    check(original_nodes==worker_original+1 && !p->forced_return,
        "a thousand-soldier capture worker follows the native <=3,000 retreat decision (2026-10-21)");
    *p=worker_state;sortie_restore(actor,worker_order);current_army=saved_current;
    for(unsigned f:{6u,4u,7u,3u,5u})slot(0x73958,f,object(f));priority_site=22065;priority_empty_candidates=false;
    check(apply_priority(actor,nullptr,nullptr) && p->facility==6 && actor[0x37]==8,
        "captured home base starts highest-priority construction before the next movement order");
    Obj facility=object(22065);facility[0x12]=6;facility[0x13]=2;put16(facility,0x14,1000);
    slot(0x6DA98,400,facility);put16(table(0xE270,22065,48399),0x1A,400);
    check(!continue_build(actor,*p) && p->finished_build && p->build_chain,
        "worker completing one facility retains the base until native capacity is full");
    check(campaign_regional_step(actor,*p) && p->region_target==22064,"worker cannot leave its unfinished captured-base obligation");
    cap_reached=true;check(!try_build(actor,*p) && !p->build_chain,"full allied capacity releases construction");cap_reached=false;
    check(campaign_regional_step(actor,*p) && p->region_target==22066,"completed capacity selects the next neighboring capture");
    table(0xE270,22066,48399)[0x14]=2;table(0xE270,22067,48399)[0x14]=2;
    put16(actor,0x2A,22066);observe_capture(actor);priority_site=22067;
    check(p->credit && try_build(actor,*p) && p->facility==6,"next occupation grants a fresh build phase and repeats capture build capture build");
    p->facility=0;p->credit=true;p->build_chain=true;actor[0x46]=0;actor[0x37]=1;priority_empty_candidates=true;
    check(try_build(actor,*p),"peaceful home-region construction falls back to local land when offensive placement offers no candidates");
    p->facility=0;p->credit=true;p->build_chain=true;actor[0x46]=0;actor[0x37]=1;money=0;
    check(!try_build(actor,*p),"local-site fallback cannot bypass native construction cost");money=10000;
    progress[59]={};put16(actor,0x3A,22066);actor[0x37]=1;actor[0x38]=9;p=observe_capture(actor);remember_campaign(actor,*p);
    prepare_local_build(actor,*p);
    check(p->regional_worker && p->work_source==2 && p->build_chain && !p->credit,
        "a saved one-thousand home capture order recovers its build review without fabricating a capture");
    // These regions are on the real native route, well beyond one hex.
    put16(actor,0x2A,22050);put16(actor,0x16,6000);actor[0x37]=13;actor[0x38]=255;put16(group,0x16,1);
    for(unsigned y=96;y<=105;++y)for(unsigned x=45;x<=76;++x) {
        Obj t=table(0xE270,y*220+x,48399);put16(t,0x12,x<56?10:x<62?16:x<66?14:x<72?15:13);
        t[0x14]=x<56?2:1;
    }
    table(0xE270,22066,48399)[0x14]=2;
    table(0xE270,22064,48399)[0x14]=1;table(0xE270,22066,48399)[0x14]=2;
    check(campaign_path_capture_hex(actor,22074)==22058,"native route captures the intervening port even when its center is eight hexes away");
    {   // mission_v2: the crossed hostile region becomes the head of the troops work list (+18), once.
        unsigned short saved_mode=word(group,0x14),saved_goal=word(group,0x16),saved_list[10];
        memcpy(saved_list,group+0x18,sizeof(saved_list));
        put16(group,0x14,10);put16(group,0x16,1);for(unsigned i=0;i<10;++i)put16(group,0x18+i*2,48400);
        mission_route_waypoint(group);mission_route_waypoint(group);
        unsigned listed=0;for(unsigned i=0;i<10;++i)if(word(group,0x18+i*2)==22058)++listed;
        check(word(group,0x18)==22058 && listed==1 && word(group,0x16)==1,
            "mission_v2: the crossed port heads the native work list once and the castle goal is kept");
        put16(group,0x14,saved_mode);put16(group,0x16,saved_goal);memcpy(group+0x18,saved_list,sizeof(saved_list));
    }
    progress[59]={};p=observe_capture(actor);p->campaign=true;p->city=1;
    check(campaign_near_capture(actor,*p) && actor[0x38]==9 && word(actor,0x3A)==22058 && p->city==1 && word(group,0x16)==1,
        "temporary port occupation preserves the final city goal and its target type");
    table(0xE270,22058,48399)[0x14]=2;
    check(campaign_path_capture_hex(actor,22074)==22064,"after the port is secured the next hostile region on the native route is selected");
    table(0xE270,22064,48399)[0x14]=2;
    check(campaign_path_capture_hex(actor,22074)==48400,"secured route permits the final city approach without treating the castle as a capture tile");
    // Native construction can use occupied land in an enemy city's region.
    progress[59]={};put16(actor,0x2A,22073);table(0xE270,22073,48399)[0x14]=2;
    table(0xE270,22072,48399)[0x14]=2;priority_site=22072;priority_empty_candidates=false;
    p=observe_capture(actor);p->campaign=true;p->city=1;prepare_local_build(actor,*p);
    check(p->build_chain && p->build_center==22074 && table(0xE270,22074,48399)[0x14]==1,
        "enemy castle ownership does not block building on already occupied siege ground");
    p->siege_city=1;
    check(!campaign_individual_priority(actor,*p),"a direct weak-city siege latch does not overwrite its pending construction review");
    Obj enemy_army=object(90);army_ids[enemy_army]=90;enemy_army[0x150]=1;put16(enemy_army,0x16,5000);put16(enemy_army,0x2A,22075);slot(0x7E838,90,enemy_army);
    legal_facilities=1u<<4;
    check(player_threat(actor) && try_build(actor,*p) && p->facility==4,
        "siege ground reviews a legal fortress when catapult construction is unavailable despite nearby player troops");
    p->facility=0;actor[0x46]=0;p->cycle_mask=~0u;p->build_chain=true;p->build_retry_day=fixture_day;
    check(!try_build(actor,*p) && p->facility==0 && !p->credit && !p->build_chain,
        "once every facility type stands the capture obligation is released instead of repeating a type (2026-10-21)");
    p->motion_day=fixture_day-50;p->forced_return=false;p->siege_city=0;
    campaign_individual_priority(actor,*p);
    check(p->forced_return && p->motion_day==fixture_day-50,
        "construction cannot extend the forty-day stationary deadline");
    p->forced_return=false;p->returning=false;p->motion_day=fixture_day;
    slot(0x7E838,90,nullptr);
    // A nearby front/builder must not receive a castle command while building.
    actor[0x46]=0;p->facility=0;p->credit=false;p->build_chain=false;p->siege_city=0;
    Obj builder=object(58);army_ids[builder]=58;builder[0x150]=2;builder[0x1A]=100;builder[0x1B]=1;
    put16(builder,0x12,240);put16(builder,0x16,5000);put16(builder,0x2A,22075);put16(builder,0x58,159);
    builder[0x37]=8;builder[0x38]=9;builder[0x46]=4;put16(builder,0x42,22072);put16(builder,0x3A,22072);slot(0x7E838,58,builder);
    Obj fort=object(22072);fort[0x12]=4;fort[0x13]=2;put16(fort,0x14,400);slot(0x6DA98,401,fort);put16(table(0xE270,22072,48399),0x1A,401);
    Progress *bp=observe_capture(builder);bp->campaign=true;bp->city=1;
    SortieGroup saved_group=sortie_groups[159],shared={};shared.root=root;shared.group=group;shared.city=1;shared.active=true;
    shared.rallied=true;shared.working=true;shared.builder=0;shared.center=22073;shared.motion_hex=22073;shared.last_motion=fixture_day;shared.day=-1;
    shared.army[0]=builder;shared.commander[0]=240;shared.army[6]=actor;shared.commander[6]=240;sortie_groups[159]=shared;
    check(sortie_step(builder) && builder[0x37]==8 && builder[0x46]==4 && actor[0x37]!=4,
        "optional construction at the castle is restored and does not automatically force an assault");
    shared=sortie_groups[159];shared.day=fixture_day;shared.route_count=2;shared.march_target[6]=22073;sortie_groups[159]=shared;
    check(sortie_step(actor) && actor[0x37]!=4,"same-day callback preserves ordinary cohort construction without a castle hit");
    sortie_groups[159]=saved_group;slot(0x7E838,58,nullptr);slot(0x6DA98,401,nullptr);put16(table(0xE270,22072,48399),0x1A,0);
    // Departure restrictions use a private native command copy.
    Obj first=sortie_people[0];unsigned char saved_female=first[0xA9];first[0xA9]=0;
    unsigned mask=dword(first,0x12C);*reinterpret_cast<unsigned *>(first+0x12C)=mask|(1u<<7);
    *reinterpret_cast<unsigned *>(sortie_city+0x3C)=12000;sortie_issued=0;unsigned cmd[30];sortie_command(nullptr,goal,first,7,8000,cmd);
    unsigned before_dispatch_paths=finalized;
    Obj swift=sortie_issue(cmd,1);
    check(finalized==before_dispatch_paths,
        "swift dispatch defers regional path generation until the native caller has finished its group");
    check(valid(swift) && word(swift,0x16)==8000 && cmd[2]==8000 && !progress[60].regional_worker,
        "formation seven keeps eight thousand soldiers and its native non-worker mission");
    check(swift[0x1B]==7 && sortie_last_command[13]==4 && sortie_last_command[14]==5,
        "formation seven is no longer prohibited from typed city attacks");
    *reinterpret_cast<unsigned *>(first+0x12C)=mask;first[0xA9]=saved_female;
    slot(0x7E838,60,nullptr);slot(0x7F8D8,100,nullptr);sortie_issued=0;
    // Restore the original isolated departure map for the older checks.
    for(unsigned y=96;y<=105;++y)for(unsigned x=45;x<=76;++x) {
        Obj t=table(0xE270,y*220+x,48399);t[0x14]=x<56?2:1;put16(t,0x12,x<56?10:x<72?12:13);put16(t,0x1A,0);
    }
    neighbor[0x66]=1;put16(neighbor,0x58,0);slot(0x6CAF0,14,nullptr);slot(0x6CAF0,15,nullptr);slot(0x6CAF0,16,nullptr);slot(0x6DA98,400,nullptr);
    put16(actor,0x2A,22050);put16(actor,0x16,soldier);actor[0x1A]=static_cast<unsigned char>(morale);put16(actor,0x58,0);slot(0x7F8D8,159,nullptr);
    functions=bindings;std::copy(states.begin(),states.end(),progress);legal_facilities=old_legal;cap_reached=old_cap;
    (void)home_area;(void)target_area;
}
