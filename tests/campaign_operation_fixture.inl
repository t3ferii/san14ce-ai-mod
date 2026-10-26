// Exercise actual operation/order code in an isolated native world. These
// simulations validate policy and command types, not the live game's ABI.
static void __fastcall operation_candidates(Obj,Obj,Obj,GameList *out,Obj,Obj,Obj) {
    list_add(*out,table(0xE270,22071,48399));
}
static Obj operation_other_city;
static Obj __fastcall operation_city(Obj,unsigned id) {return id==8?operation_other_city:goal;}
static void campaign_operation_tests() {
    Obj saved_root=root,saved_goal=goal,saved_area=area;int saved_day=fixture_day;
    auto saved_functions=functions;
    auto saved_progress=std::vector<Progress>(progress,progress+501);
    auto saved_operations=std::vector<CampaignOperation>(campaign_operations,campaign_operations+501);
    auto saved_hits=std::vector<CampaignHit>(campaign_hits,campaign_hits+501);
    Obj ownforce=table(0xDF30,2,51),fort_definition=table(0x73958,4,30),band_definition=table(0x73958,7,30);
    root=static_cast<Obj>(calloc(1,0x90000));putptr(image,0x277DF18,root);putptr(root,0x871E8,player);
    slot(0xDF30,2,ownforce);slot(0x73958,4,fort_definition);slot(0x73958,7,band_definition);
    goal=object(9);put16(goal,0x4E,9);put16(goal,0x3A,22074);
    BIND(0x2AA850,revision_distance);BIND(0x22D740,revision_city_hex);
    BIND(0x22FB10,support_building);BIND(0x26C940,mock_finalize);
    area=object(1);put16(area,0x3A,22060);put16(area,0x56,2);area[0x66]=1;slot(0x6CAF0,1,area);
    Obj cityarea=object(2);put16(cityarea,0x3A,22074);put16(cityarea,0x56,1);cityarea[0x66]=1;cityarea[0x34]=1;slot(0x6CAF0,2,cityarea);
    for(unsigned y=93;y<=108;++y)for(unsigned x=45;x<=85;++x) {
        unsigned h=y*220+x;Obj tile=object(h);tile[0x10]=1;tile[0x14]=2;
        put16(tile,0x12,x<72?1:2);slot(0xE270,h,tile);
        support_tile_ids[tile]=h;
    }
    table(0xE270,22074,48399)[0x14]=1;
    Obj units[5]={},groups[5]={};
    for(unsigned i=1;i<=4;++i) {
        units[i]=object(i);army_ids[units[i]]=i;slot(0x7E838,i,units[i]);
        put16(units[i],0x12,i);put16(units[i],0x58,i);units[i][0x150]=2;
        groups[i]=object(i);slot(0x7F8D8,i,groups[i]);slot(0x158,i,object(i));
    }
    put16(table(0x158,3,6000),0x118,23);
    auto reset=[&]() {
        fixture_day=100;memset(progress,0,sizeof(progress));memset(campaign_operations,0,sizeof(campaign_operations));memset(campaign_hits,0,sizeof(campaign_hits));
        *reinterpret_cast<unsigned *>(goal+0x3C)=2979;
        for(unsigned i=1;i<=4;++i) {
            slot(0x7E838,i,units[i]);
            Obj a=units[i];a[0x1A]=100;a[0x1B]=i==2?3:1;put16(a,0x16,i==1?12000:i==2?4470:i==3?1000:5000);
            put16(a,0x2A,i==1?22070:i==2?22069:i==3?22068:22059);
            put16(groups[i],0x14,6);put16(groups[i],0x16,1);
            mock_clear(a);mock_action(a,1);mock_target(a,9,22060);
            Progress *p=observe_capture(a);p->built_once=true;
        }
        slot(0x7E838,4,nullptr);slot(0x6DA98,1,nullptr);
        put16(cityarea,0x56,1);cityarea[0x66]=1;
    };
    reset();
    check(campaign::reserve(15000,0)==4000 && campaign::reserve(46000,0)==4000 && campaign::reserve(15000,4000)==4000,
        "fixed garrison does not scale with enemy threat");
    operation_other_city=object(8);put16(operation_other_city,0x4E,8);put16(operation_other_city,0x3A,22080);
    *reinterpret_cast<unsigned *>(operation_other_city+0x3C)=50000;BIND(0x32C130,operation_city);
    check(campaign_operation_defenders(units[1],9,22074)==2979,
        "a nearby hostile city's fifty thousand stored troops do not inflate the current target's immediate defenders");
    BIND(0x32C130,mock_goal);
    check(campaign_siege_priority(units[3],progress[3]),"completed regional support can initiate a plan across independent native groups");
    check(campaign_operations[1].count==3 && campaign_operations[1].phase==campaign::assault && units[1][0x37]==4 && word(units[1],0x3A)==9,
        "Yuan Shao 12000 attacks a 2979 city without a forty thousand requirement");
    check(units[2][0x37]==1 && units[3][0x37]==1 && !progress[2].siege_city && !progress[3].siege_city,
        "4470 ranged middle and 1000 support stay outside castle fire instead of joining the assault");
    for(unsigned i=1;i<=3;++i) {
        check(word(units[i],0x16)==(i==1?12000:i==2?4470:1000) && word(units[i],0x58)==i && word(groups[i],0x14)==6,
            "field enrollment preserves soldiers and native group membership");
        if(i>1)check(revision_distance(word(units[i],0x3A),22074)>2,"rear orders remain outside castle range");
    }
    unsigned finalized_before=finalized;
    check(campaign_siege_priority(units[2],progress[2]) && finalized==finalized_before,"same-day member callback preserves shared commands");
    CampaignHit &hit=campaign_hits[3];hit={};hit.present=true;hit.root=root;hit.victim=units[3];hit.commander=3;hit.force=2;hit.day=100;
    progress[3].combat_day=90;
    check(!campaign_individual_priority(units[3],progress[3]) && !progress[3].combat_target,"support receiving a hit does not abandon its shared rear role");
    campaign_operation_release(1,"fixture_release");
    for(unsigned i=1;i<=3;++i)check(!progress[i].operation_leader && !progress[i].campaign && units[i][0x37]==1 && word(units[i],0x3A)==22060,
        "operation release resumes each independent original regional order");
    reset();progress[2].built_once=false;put16(groups[3],0x14,3);
    check(campaign_operation_create(units[1],progress[1]) && campaign_operations[1].count==1 && !progress[2].operation_leader && !progress[3].operation_leader,
        "unfinished capture and native defense stay with their original planner");
    reset();*reinterpret_cast<unsigned *>(goal+0x3C)=20000;
    check(!campaign_siege_priority(units[1],progress[1]) && units[1][0x37]==1 && !progress[1].forced_return && !progress[1].operation_leader,
        "insufficient independent field strength retains existing jobs without forced retreat");
    reset();put16(table(0x7F8D8,2,500),0x14,10);put16(table(0x7F8D8,2,500),0x16,9);
    mock_action(units[2],4);mock_target(units[2],5,9);progress[2].credit=false;path_available=false;
    check(campaign_siege_priority(units[1],progress[1]) && units[2][0x37]==13,
        "blocked small middle cancels its previous city assault rather than continuing an unsafe order");path_available=true;
    reset();put16(units[2],0x16,5000);
    check(campaign_siege_priority(units[1],progress[1]) && units[2][0x37]==4,"middle at five thousand joins the city assault");
    reset();put16(units[1],0x2A,22060);put16(units[2],0x2A,22059);put16(units[3],0x2A,22058);
    check(!campaign_siege_priority(units[1],progress[1]) && !progress[1].operation_leader && units[1][0x37]==1,"distant sufficient core keeps regional work before opportunistic siege enrollment");
    fixture_day=120;
    check(!campaign_siege_priority(units[1],progress[1]) && !campaign_operations[1].active && !progress[1].forced_return,
        "two-turn unsuccessful rally releases original jobs instead of waiting indefinitely");
    check(!campaign_siege_priority(units[1],progress[1]),"rally cooldown prevents immediate recreation of the failed plan");
    reset();put16(groups[1],0x14,10);put16(groups[1],0x16,9);remember_campaign(units[1],progress[1]);progress[1].credit=true;
    check(!campaign_available(units[1],progress[1],9),"unspent campaign construction credit is not stolen by a siege plan");
    progress[1].credit=false;progress[1].build_chain=true;
    check(!campaign_available(units[1],progress[1],9),"ongoing regional build sequence keeps priority over siege enrollment");
    progress[1].build_chain=false;progress[1].region_target=22060;table(0xE270,22060,48399)[0x14]=1;
    check(!campaign_available(units[1],progress[1],9),"unfinished hostile regional capture keeps priority over siege enrollment");
    table(0xE270,22060,48399)[0x14]=2;progress[1].region_target=48400;
    check(campaign_available(units[1],progress[1],9),"completed field work near a weakened city becomes eligible naturally");
    reset();Obj fort=object(22067);fort[0x12]=4;fort[0x13]=2;put16(fort,0x14,1000);slot(0x6DA98,1,fort);
    check(campaign_siege_priority(units[1],progress[1]) && campaign_operations[1].phase==campaign::prepare && units[1][0x37]==1,
        "an existing completed fortress holds assault core for real defense-buff exposure");
    fixture_day=110;
    check(campaign_siege_priority(units[1],progress[1]) && units[1][0x37]==4,"one native turn of fort exposure releases the assault");
    reset();*reinterpret_cast<unsigned *>(goal+0x3C)=6000;
    BIND(0xB9070,operation_candidates);BIND(0x22DDA0,support_hex);
    check(campaign_siege_priority(units[1],progress[1]) && campaign_operations[1].builder==1 && progress[2].facility==4 && units[2][0x37]==8,
        "a stronger city prepares one legal fortress through the original native construction command");
    check(campaign_siege_priority(units[2],progress[2]) && units[2][0x37]==8,"builder callback preserves the shared construction order");
    Obj prepared=object(22071);prepared[0x12]=4;prepared[0x13]=2;put16(prepared,0x14,1000);slot(0x6DA98,1,prepared);
    put16(table(0xE270,22071,48399),0x1A,1);put16(groups[2],0x14,7);put16(groups[2],0x16,22071);fixture_day=110;
    check(campaign_siege_priority(units[1],progress[1]) && !progress[2].facility && word(groups[2],0x14)==6 && word(groups[2],0x16)==1 && progress[2].operation_leader==1,
        "completed construction restores the builder's original native group assignment and keeps its field role");
    put16(table(0xE270,22071,48399),0x1A,0);reset();*reinterpret_cast<unsigned *>(goal+0x3C)=6000;campaign_siege_priority(units[1],progress[1]);
    fixture_day=120;
    check(campaign_siege_priority(units[1],progress[1]) && campaign_operations[1].active && progress[2].facility==4 && units[2][0x37]==8,
        "twenty-day construction no longer dissolves a cohort before its thirty-day stationary advance");
    BIND(0xB9070,mock_candidates);BIND(0x22DDA0,mock_id);
    reset();slot(0x7E838,4,units[4]);units[4][0x150]=1;put16(units[4],0x2A,22066);put16(units[4],0x16,1000);
    check(campaign_siege_priority(units[1],progress[1]) && units[1][0x37]!=7 && !campaign_operations[1].enemy,
        "field enemy at four hexes does not create a custom interception or replace the city operation");
    reset();slot(0x7E838,4,units[4]);units[4][0x150]=1;put16(units[4],0x2A,22066);put16(units[4],0x16,1000);put16(units[3],0x2A,22060);
    check(campaign_siege_priority(units[1],progress[1]) && units[1][0x37]!=7 && !campaign_operations[1].enemy && units[3][0x37]==1,
        "nearby enemy cannot redirect the front or rear into the removed field-interception plan");
    units[4][0x150]=2;reset();
    for(unsigned i=3;i<=5;++i) {
        Obj region=object(i);unsigned h=22060-i;put16(region,0x3A,h);slot(0x6CAF0,i,region);
        table(0xE270,h,48399)[0x14]=1;put16(cityarea,0x56+(i-2)*2,i);cityarea[0x66]|=static_cast<unsigned char>(1u<<(i-2));
    }
    check(campaign_enemy_bases(units[1],9).size()==3 && campaign_siege_priority(units[1],progress[1]) && units[1][0x37]==1,
        "three target-owned adjacent bases trigger deterministic capture preparation");
    for(unsigned i=3;i<=5;++i)table(0xE270,22060-i,48399)[0x14]=2;
    fixture_day=110;
    check(campaign_siege_priority(units[1],progress[1]) && units[1][0x37]==4,"adjacent ownership reduction switches the existing plan to city assault");
    reset();campaign_siege_priority(units[1],progress[1]);put16(groups[2],0x16,4);fixture_day=110;
    mock_action(units[2],4);mock_target(units[2],5,4);
    campaign_siege_priority(units[1],progress[1]);
    check(!progress[2].operation_leader && word(units[2],0x3A)==4,"a new native assignment detaches a member without restoring its obsolete order");
    reset();campaign_siege_priority(units[1],progress[1]);put16(units[1],0x12,4);observe_capture(units[1]);
    check(!campaign_operation_identity(units[1],progress[1]),"reused army commander cannot inherit the previous operation");put16(units[1],0x12,1);
    put16(table(0xE270,22071,48399),0x1A,0);
    reset();campaign_siege_priority(units[1],progress[1]);campaign_operations[1].phase=campaign::rally;
    for(unsigned i=1;i<=3;++i) {mock_action(units[i],1);mock_target(units[i],9,22060);}
    fixture_day=129;check(!campaign_forced_siege_priority(units[1],progress[1]),"twenty-nine stationary days do not force a group advance");
    fixture_day=130;put16(units[2],0x2A,22068);observe_capture(units[2]);
    check(!campaign_forced_siege_priority(units[1],progress[1]),"actual member movement postpones the thirty-day group advance");
    fixture_day=159;check(!campaign_forced_siege_priority(units[3],progress[3]),"twenty-nine days after the last movement still retain the plan");
    fixture_day=160;check(campaign_forced_siege_priority(units[3],progress[3]) && campaign_operations[1].active && campaign_operations[1].force_march && units[1][0x37]==1,
        "thirty-day deadline advances the existing shared operation rather than creating individual castle attacks");
    for(unsigned i=1;i<=3;++i)check(campaign_castle_locked(units[i]) && progress[i].forced_siege_reason==3 && word(groups[i],0x14)==10 && word(groups[i],0x16)==9,
        "thirty-day group march locks competing commands while retaining the final strategic castle");
    CampaignRoute replacement;replacement.cells={word(units[2],0x2A),22065};
    check(!campaign_route_order(units[2],replacement) && units[2][0x37]==1 && word(units[2],0x3A)!=22065,
        "a subsequent ordinary movement command cannot overwrite the thirty-day group advance");
    put16(groups[2],0x14,7);put16(groups[2],0x16,22065);progress[2].facility=4;progress[2].build_chain=true;
    check(campaign_forced_siege_priority(units[2],progress[2]) && word(groups[2],0x14)==10 && word(groups[2],0x16)==9 && !progress[2].facility && campaign_group_march_depth==0,
        "native group goal rewrites are repaired and the authorized movement scope never leaks to ordinary writers");
    path_available=false;progress[2].facility=4;mock_action(units[2],8);mock_target(units[2],9,22071);units[2][0x46]=4;
    check(campaign_forced_siege_priority(units[2],progress[2]) && units[2][0x37]==1 && units[2][0x38]==9 && word(units[2],0x3A)<48400 && units[2][0x46]==0,
        "failed march path after native construction overwrite leaves a valid movement order instead of a zero-facility build command");path_available=true;
    reset();campaign_siege_priority(units[1],progress[1]);
    Obj repeat_build=object(22071);repeat_build[0x12]=4;repeat_build[0x13]=2;put16(repeat_build,0x14,1000);
    slot(0x6DA98,1,repeat_build);put16(table(0xE270,22071,48399),0x1A,1);
    for(unsigned n=0;n<4;++n) {Progress &worker=progress[2];worker.facility=4;worker.construction_hex=22071;
        check(!continue_build(units[2],worker) && !campaign_forced_siege_priority(units[2],worker) && !campaign_castle_locked(units[2]),
            "third and later completed builds do not commit, lock or cancel the siege construction plan");}
    put16(table(0xE270,22071,48399),0x1A,0);
    root=saved_root;goal=saved_goal;area=saved_area;fixture_day=saved_day;functions=saved_functions;putptr(image,0x277DF18,root);
    std::copy(saved_progress.begin(),saved_progress.end(),progress);std::copy(saved_operations.begin(),saved_operations.end(),campaign_operations);
    std::copy(saved_hits.begin(),saved_hits.end(),campaign_hits);
}
