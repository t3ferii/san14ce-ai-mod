#include "campaign_priority_fixture.inl"
static void campaign_departure_tests() {
    check(!campaign::siege_departure(19999,0),"under twenty thousand available does not create a siege cohort");
    check(campaign::siege_departure(20000,25000),"twenty thousand can depart against exactly twenty-five thousand");
    check(!campaign::siege_departure(20000,25001) && !campaign::siege_departure(20000,27000),"twenty thousand waits against twenty-seven thousand");
    check(campaign::siege_departure(39999,44999) && !campaign::siege_departure(39999,45000),"intermediate budget uses the inclusive five thousand margin");
    check(campaign::siege_departure(40000,0xFFFFFFFFu),"forty thousand departs regardless of target garrison");
    check(campaign::siege_departure(campaign::available(70000),3540),"Jianye seventy thousand against Wu 3540 passes the troop condition");
    auto saved_functions=functions;
    auto saved_progress=std::vector<Progress>(progress,progress+501);
    unsigned stock=dword(sortie_city,0x3C),enemy=dword(goal,0x3C);int day=fixture_day;
    BIND(0x32C130,reinforcement_goal);BIND(0x22C990,reinforcement_owner);
    BIND(0x22D740,revision_city_hex);BIND(0x2AA850,revision_distance);BIND(0x26C940,mock_finalize);
    put16(sortie_city,0x3A,22050);put16(goal,0x3A,22074);
    Obj first=sortie_people[0];unsigned char female=first[0xA9];unsigned home=word(first,0xE2);
    put16(first,0xE2,2);sortie_planner_node=nullptr;
    auto reset=[&]() {
        for(unsigned i=60;i<70;++i) {slot(0x7E838,i,nullptr);slot(0x7F8D8,100+i-60,nullptr);progress[i]={};}
        sortie_issued=0;sortie_fail_at=0;sortie_district[0x14]=20;money=10000;
    };
    reset();*reinterpret_cast<unsigned *>(sortie_city+0x3C)=5000;
    Obj worker=campaign_paid_single(sortie_city,sortie_city,22070,1000,true);
    check(valid(worker) && word(worker,0x16)==1000 && dword(sortie_city,0x3C)==4000,
        "independent capture worker uses exactly one thousand above the four thousand garrison");
    check(worker && progress[army_id(worker)].regional_worker && !progress[army_id(worker)].siege_city,
        "capture worker retains its regional mission instead of entering city assault");
    check(!campaign_paid_single(sortie_city,sortie_city,22074,1000,true) && sortie_issued==1,
        "a second worker cannot drain the fixed garrison");
    reset();*reinterpret_cast<unsigned *>(sortie_city+0x3C)=10000;
    unsigned capture_command[30];sortie_command(nullptr,goal,first,7,6000,capture_command);
    unsigned original_capture[30];memcpy(original_capture,capture_command,sizeof(original_capture));
    Obj parked=sortie_issue(capture_command,1);
    check(valid(parked) && sortie_last_command[13]==4 && sortie_last_command[14]==5 &&
          sortie_last_command[15]==1 && sortie_last_command[2]==6000,
        "formation seven castle departure retains native assault and is not parked or capped");
    check(!memcmp(original_capture,capture_command,sizeof(original_capture)) &&
          !progress[army_id(parked)].regional_worker,
        "formation alone never changes a native castle assault into a capture worker");
    reset();*reinterpret_cast<unsigned *>(sortie_city+0x3C)=10000;put16(sortie_city,0x3A,48400);
    check(valid(sortie_issue(capture_command,1)) && sortie_issued==1,
        "formation seven no longer has a mod-specific source parking requirement");
    put16(sortie_city,0x3A,22050);
    reset();*reinterpret_cast<unsigned *>(sortie_city+0x3C)=10000;*reinterpret_cast<unsigned *>(goal+0x3C)=9999;
    Obj solo=campaign_paid_single(sortie_city,goal,48400,8000,false);
    check(valid(solo) && sortie_issued==1 && word(solo,0x16)==6000 && dword(sortie_city,0x3C)==4000,
        "weak city gets a single native departure without the twenty thousand cohort gate");
    check(solo && progress[army_id(solo)].siege_city==1 && solo[0x37]==4 && solo[0x38]==5 && word(solo,0x3A)==1,
        "weak-city solo attack retains a typed city goal instead of a regional capture detour");
    reset();*reinterpret_cast<unsigned *>(sortie_city+0x3C)=4000;
    check(!campaign_paid_single(sortie_city,goal,48400,8000,false) && sortie_issued==0,
        "weak city does not override the four thousand garrison floor");
    reset();*reinterpret_cast<unsigned *>(sortie_city+0x3C)=10000;first[0xA9]=1;
    unsigned cmd[30];sortie_command(nullptr,goal,first,1,6000,cmd);
    Obj woman=sortie_issue(cmd,1);
    check(valid(woman) && word(woman,0x16)==1000 && cmd[2]==6000 && dword(sortie_city,0x3C)==9000,
        "automatic female departure is capped at one thousand without modifying the caller command");
    reset();Obj saved_root=root;
    Obj ownforce=table(0xDF30,2,51);Obj formations[21]={};for(unsigned i=1;i<=20;++i)formations[i]=table(0x77428,i,20);
    root=static_cast<Obj>(calloc(1,0x90000));putptr(image,0x277DF18,root);putptr(root,0x871E8,player);
    slot(0xDF30,2,ownforce);slot(0xE0D0,2,sortie_district);
    for(unsigned i=1;i<=20;++i)slot(0x77428,i,formations[i]);
    for(Obj person:sortie_people)slot(0x158,word(person,0x10),person);
    Obj actor=object(59);army_ids[actor]=59;actor[0x150]=2;put16(actor,0x12,240);put16(actor,0x16,6000);put16(actor,0x2A,22050);
    Obj commander=object(240);commander[0xE0]=2;slot(0x158,240,commander);slot(0x7E838,59,actor);
    Obj home_area=object(10),neighbor=object(12),target_area=object(13);
    put16(home_area,0x3A,22050);put16(home_area,0x56,12);home_area[0x66]=1;home_area[0x34]=1;
    put16(neighbor,0x3A,22060);put16(neighbor,0x56,13);neighbor[0x66]=1;
    put16(target_area,0x3A,22074);put16(target_area,0x56,12);target_area[0x66]=1;target_area[0x34]=1;
    slot(0x6CAF0,10,home_area);slot(0x6CAF0,12,neighbor);slot(0x6CAF0,13,target_area);
    for(unsigned y=96;y<=105;++y)for(unsigned x=45;x<=76;++x) {
        unsigned h=y*220+x;Obj tile=object(h);tile[0x14]=x<56?2:1;put16(tile,0x12,x<56?10:x<72?12:13);slot(0xE270,h,tile);
    }
    campaign_priority_tests(actor,home_area,neighbor,target_area);
    auto saved_force=campaign_departure_force[2],saved_city=campaign_departure_city[2];
    campaign_departure_force[2]={};campaign_departure_city[2]={};first[0xA9]=0;
    *reinterpret_cast<unsigned *>(sortie_city+0x3C)=28000;*reinterpret_cast<unsigned *>(goal+0x3C)=9999;
    campaign_proactive_departures(actor);
    check(sortie_issued>2 && progress[60].regional_worker && word(table(0x7E838,60,500),0x16)==1000 && sortie_groups[150].active,
        "one AI callback issues a home worker and a real siege cohort even when the target has under ten thousand");
    unsigned issued=sortie_issued;campaign_proactive_departures(actor);
    check(sortie_issued==issued,"repeated callbacks cannot duplicate periodic city departures in the same turn");
    reset();campaign_departure_force[2]={};campaign_departure_city[2]={};sortie_groups[150]={};
    *reinterpret_cast<unsigned *>(sortie_city+0x3C)=28000;*reinterpret_cast<unsigned *>(goal+0x3C)=10000;
    campaign_proactive_departures(actor);
    check(sortie_issued>2 && sortie_groups[150].active && dword(sortie_city,0x3C)==4000,
        "twenty-eight thousand city also proactively forms a real paid cohort and preserves four thousand");
    sortie_groups[150]={};campaign_departure_force[2]=saved_force;campaign_departure_city[2]=saved_city;
    root=saved_root;putptr(image,0x277DF18,root);
    reset();first[0xA9]=female;put16(first,0xE2,home);
    *reinterpret_cast<unsigned *>(sortie_city+0x3C)=stock;*reinterpret_cast<unsigned *>(goal+0x3C)=enemy;
    std::copy(saved_progress.begin(),saved_progress.end(),progress);functions=saved_functions;fixture_day=day;
}
