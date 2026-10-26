static Obj __fastcall reinforcement_goal(Obj,unsigned id) {return id==2?sortie_city:goal;}
static Obj __fastcall reinforcement_owner(Obj city) {return city==sortie_city?table(0xDF30,2,51):player;}
static Obj __fastcall reinforcement_manual(Obj) {return sortie_district;}
static void campaign_reinforcement_tests() {
    BIND(0x32C130,reinforcement_goal);BIND(0x22C990,reinforcement_owner);
    BIND(0x22D740,revision_city_hex);BIND(0x2AA850,revision_distance);
    put16(sortie_city,0x3A,22050);put16(goal,0x3A,22074);
    Obj requester=object(59);army_ids[requester]=59;requester[0x150]=2;requester[0x1A]=100;
    put16(requester,0x12,240);put16(requester,0x16,10000);put16(requester,0x2A,22070);put16(requester,0x58,59);
    Obj officer=object(240);officer[0xE0]=3;slot(0x158,240,officer);slot(0xE0D0,3,object(3));slot(0x7E838,59,requester);
    Obj group=object(59);put16(group,0x14,10);put16(group,0x16,1);slot(0x7F8D8,59,group);
    auto p=observe_capture(requester);remember_campaign(requester,*p);
    auto reset=[&]() {
        for(unsigned i=60;i<65;++i) {slot(0x7E838,i,nullptr);slot(0x7F8D8,100+i-60,nullptr);}
        campaign_reinforcement_attempt[2][1]={};campaign_reinforcement_source[2]={};
        sortie_issued=0;sortie_fail_at=0;sortie_district[0x14]=20;money=10000;
        *reinterpret_cast<unsigned *>(sortie_city+0x3C)=46000;
        for(Obj person:sortie_people)person[0x161]=0;
    };
    reset();
    check(campaign_reinforcement_reserve(sortie_city,2)==4000,"reinforcement retains the fixed four thousand garrison");
    check(campaign_request_reinforcements(requester,*p,12000) && sortie_issued==2 && dword(sortie_city,0x3C)==30000,
        "shortage requests two real paid native reinforcements from a different district and conserves soldiers");
    for(unsigned i=60;i<62;++i) {
        Obj a=table(0x7E838,i,500),g=table(0x7F8D8,word(a,0x58),500);
        check(word(g,0x14)==10 && word(g,0x16)==1 && progress[i].campaign && progress[i].city==1,
            "each reinforcement receives the existing city campaign and regional movement policy");
    }
    unsigned before=sortie_issued;
    check(!campaign_request_reinforcements(requester,*p,12000) && sortie_issued==before,"same-turn and already incoming reinforcements prevent duplicate dispatch");
    fixture_day+=10;
    check(!campaign_request_reinforcements(requester,*p,12000) && sortie_issued==before,"next turn still waits for sufficient reinforcements en route");fixture_day-=10;
    reset();for(Obj person:sortie_people)person[0x161]=1;
    check(!campaign_request_reinforcements(requester,*p,6000) && !sortie_issued,"wounded source officers never become reinforcement commanders");
    reset();sortie_district[0x14]=0;
    check(!campaign_request_reinforcements(requester,*p,6000) && !sortie_issued,"reinforcement cannot bypass district command-point cost");
    reset();BIND(0x32E8D0,reinforcement_manual);
    check(!campaign_request_reinforcements(requester,*p,6000) && !sortie_issued,"directly controlled source district is excluded");BIND(0x32E8D0,sortie_mock_manual);
    reset();*reinterpret_cast<unsigned *>(sortie_city+0x3C)=999;
    check(!campaign_request_reinforcements(requester,*p,6000) && !sortie_issued,"under one thousand available stays in city");
    reset();*reinterpret_cast<unsigned *>(sortie_city+0x3C)=7000;
    check(campaign_request_reinforcements(requester,*p,6000) && sortie_issued==1 && dword(sortie_city,0x3C)==4000,
        "exactly three thousand deployable reinforces while preserving four thousand");
    reset();sortie_fail_at=1;
    check(!campaign_request_reinforcements(requester,*p,6000) && dword(sortie_city,0x3C)==46000,
        "failed native departure does not fabricate incoming troops or deduct city soldiers");
    reset();
    Obj ownarea=object(10),transit=object(11);put16(ownarea,0x3A,22050);put16(transit,0x3A,22060);
    put16(ownarea,0x56,11);ownarea[0x66]=1;put16(transit,0x56,10);transit[0x66]=1;transit[0x34]=1;
    slot(0x6CAF0,10,ownarea);slot(0x6CAF0,11,transit);
    for(unsigned y=96;y<=105;++y)for(unsigned x=45;x<=76;++x) {
        Obj tile=object(y*220+x);put16(tile,0x12,x<56?10:11);tile[0x14]=x<56?2:3;slot(0xE270,y*220+x,tile);
    }
    check(campaign_request_reinforcements(requester,*p,1000,11,22074) && sortie_issued==1 && dword(sortie_city,0x3C)==45000,
        "missing city transit helper dispatches exactly one thousand real soldiers while retaining reserve");
    Obj protector=table(0x7E838,60,500);
    check(progress[60].guard_requested && word(protector,0x16)==1000 && protector[0x37]==1,
        "new paid protector receives a safe transit assignment rather than a castle assault");
    check(!campaign_guard(requester,11,22074),"requested protector cannot authorize transit before physical arrival");
    put16(protector,0x2A,progress[60].guard_hex);
    check(campaign_guard(requester,11,22074)==protector,"physical arrival makes the actual protector available");
    fixture_day+=20;
    check(campaign_guard_priority(protector,progress[60]) && progress[60].guard_requested,
        "protector continues holding C until B ownership changes, even after twenty days");fixture_day-=20;
    reset();
    Obj primary=object(12),secondary=object(13);put16(primary,0x3A,22070);put16(secondary,0x3A,22074);
    put16(primary,0x56,13);primary[0x66]=1;put16(secondary,0x56,12);secondary[0x66]=1;
    slot(0x6CAF0,12,primary);slot(0x6CAF0,13,secondary);
    put16(table(0xE270,22070,48399),0x12,12);put16(table(0xE270,22074,48399),0x12,13);
    table(0xE270,22070,48399)[0x14]=3;table(0xE270,22074,48399)[0x14]=3;
    p->region_target=22070;p->companion_target=0;p->companion_parent=0;p->campaign=true;p->city=1;
    *reinterpret_cast<unsigned *>(sortie_city+0x3C)=5000;
    check(campaign_request_companion(requester,*p,22070) && sortie_issued==1 && dword(sortie_city,0x3C)==4000,
          "capture 1+1 dispatches exactly one thousand above the fixed four thousand garrison");
    Obj companion=table(0x7E838,p->companion_target,500);Progress &cp=progress[p->companion_target];
    check(word(companion,0x16)==1000 && cp.companion_parent==59 && cp.region_target==22074 && word(companion,0x3A)==22074,
          "paired helper receives the primary base's adjacent base, not another copy of primary goal");
    check(campaign_request_companion(requester,*p,22070) && sortie_issued==1,
          "repeated primary evaluation does not dispatch a duplicate companion");
    reset();slot(0x7E838,59,nullptr);slot(0x7F8D8,59,nullptr);
    BIND(0x32C130,mock_goal);BIND(0x22C990,mock_goal_force);BIND(0x22D740,mock_goal_hex);BIND(0x2AA850,mock_distance);
}
