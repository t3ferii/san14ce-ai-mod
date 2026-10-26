// Corps policy at the departure dispatcher 1EC890 (combat sortie_dispatch, or the
// convenience entry hook without the combat module). Never invokes the game process.
static bool district_fixture_player=true;
static Obj district_fixture_direct;
static unsigned district_fixture_dispatches;
static int __fastcall district_fixture_member(Obj) {return district_fixture_player?1:0;}
static Obj __fastcall district_fixture_controlled(Obj) {return district_fixture_direct;}
static Obj __fastcall district_fixture_dispatch(unsigned *,int) {++district_fixture_dispatches;return nullptr;}
static Obj district_fixture_context_corps;static unsigned district_fixture_safety_calls;
static Obj __fastcall district_fixture_corps(Obj,Obj) {return district_fixture_context_corps;}
static unsigned char __fastcall district_fixture_enemy_near(Obj,Obj,const unsigned *,unsigned) {++district_fixture_safety_calls;return 0;}
static void district_intercept_tests() {
    auto saved_functions=functions;
    const auto saved_features=mod_features;
    LONG saved_enabled=enabled;
    Obj old_options=pointer(root,0x871E8),old_district=table(0xE0D0,50,51);
    Obj actor=object(5999),corps=object(50),direct=object(49),options=object(1);
    Obj old_actor=table(0x158,5999,6000);
    actor[0xE0]=50;corps[0x17]=5;
    slot(0xE0D0,50,corps);slot(0x158,5999,actor);putptr(root,0x871E8,options);
    BIND(0x233750,district_fixture_member);BIND(0x32E8D0,district_fixture_controlled);
    enabled=1;mod_features.district_intercept_block=true;district_fixture_direct=direct;
    unsigned cmd[30]={};cmd[0]=5999;cmd[2]=12000;cmd[13]=7;cmd[14]=27;cmd[15]=77;
    using district_intercept::blocked;
    auto with=[&](unsigned action,unsigned kind){cmd[13]=action;cmd[14]=kind;return blocked(cmd);};
    // 지원 (1): transport only.
    corps[0x17]=5;
    check(with(7,27),"support corps: new interception blocked");
    check(with(1,9),"support corps: area move/occupy blocked");
    check(with(4,5) && with(5,6),"support corps: city/gate attack blocked");
    check(with(6,11),"support corps: construction blocked (transport only)");
    check(!with(2,5) && !with(3,6),"support corps: transport to city/gate allowed");
    // 내정 (2): no military departure; transport and construction stay.
    corps[0x17]=4;
    check(with(7,27) && with(1,9) && with(4,5) && with(5,6) && with(14,9),"domestic corps: interception, occupy and attacks blocked");
    check(!with(6,11) && !with(2,5) && !with(3,6),"domestic corps: construction and transport allowed");
    for(unsigned policy:{4u,5u}) {
        corps[0x17]=static_cast<unsigned char>(policy);
        district_fixture_player=false;check(!with(7,27),"computer corps keeps native departures");district_fixture_player=true;
        district_fixture_direct=corps;check(!with(7,27),"player direct corps keeps native departures");district_fixture_direct=direct;
    }
    corps[0x17]=3;check(!with(7,27) && !with(1,9),"foothold-attack policy preserved");corps[0x17]=1;check(!with(7,27),"full delegation preserved");corps[0x15]=1;corps[0x17]=2;check(!with(7,27),"BT action byte +15 is not the policy");corps[0x15]=0;
    corps[0x17]=9;check(!with(7,27),"unknown policy fails open");
    corps[0x17]=5;cmd[13]=7;cmd[14]=27;
    mod_features.district_intercept_block=false;check(!blocked(cmd),"feature off restores native departures");
    mod_features.district_intercept_block=true;enabled=0;check(!blocked(cmd),"inactive module does not block");enabled=1;
    district_fixture_direct=nullptr;check(!blocked(cmd),"missing direct corps fails open");district_fixture_direct=direct;
    putptr(root,0x871E8,nullptr);check(!blocked(cmd),"missing player options fail open");putptr(root,0x871E8,options);
    cmd[0]=0;check(!blocked(cmd),"invalid actor fails open");cmd[0]=5999;
    slot(0xE0D0,50,nullptr);check(!blocked(cmd),"invalid actor corps fails open");slot(0xE0D0,50,corps);
    check(!blocked(nullptr),"null command fails open");
    // Transport sources: condition 84 (5 city, 15 hexes) is true for restricted player corps only.
    BIND(0xB60E0,district_fixture_corps);BIND(0xAEE60,district_fixture_enemy_near);
    district_fixture_context_corps=corps;
    {const unsigned city15[4]={5,1,2,15},city10[4]={5,1,2,10},gate15[4]={6,1,2,15};
    using district_intercept::transport_safety;
    for(unsigned policy:{4u,5u}) {
        corps[0x17]=static_cast<unsigned char>(policy);district_fixture_safety_calls=0;
        check(transport_safety(nullptr,nullptr,city15,0)==1 && district_fixture_safety_calls==0,"restricted player corps city is a transport source despite nearby enemies");
        check(transport_safety(nullptr,nullptr,city10,0)==0 && transport_safety(nullptr,nullptr,gate15,0)==0 && district_fixture_safety_calls==2,"radius 10 and other object kinds stay native");
    }
    corps[0x17]=3;check(transport_safety(nullptr,nullptr,city15,0)==0,"other policies keep the native safety rule");
    corps[0x17]=5;district_fixture_player=false;check(transport_safety(nullptr,nullptr,city15,0)==0,"computer corps keep the native safety rule");district_fixture_player=true;
    district_fixture_direct=corps;check(transport_safety(nullptr,nullptr,city15,0)==0,"direct corps keeps the native safety rule");district_fixture_direct=direct;
    mod_features.district_intercept_block=false;check(transport_safety(nullptr,nullptr,city15,0)==0,"switch off keeps the native safety rule");mod_features.district_intercept_block=true;
    district_fixture_context_corps=nullptr;check(transport_safety(nullptr,nullptr,city15,0)==0,"missing context corps fails to native");}
    corps[0x17]=5;
    // Both owners of 1EC890 judge before the native dispatcher and never modify the command.
    unsigned saved_cmd[30];memcpy(saved_cmd,cmd,sizeof(cmd));
    auto saved_dispatch=original_sortie_dispatch;original_sortie_dispatch=district_fixture_dispatch;
    auto saved_entry=district_intercept::original_dispatch;district_intercept::original_dispatch=district_fixture_dispatch;
    district_fixture_dispatches=0;
    check(sortie_dispatch(cmd,1)==nullptr && district_fixture_dispatches==0,"combat dispatcher blocks before the native dispatcher");
    check(district_intercept::dispatch(cmd,1)==nullptr && district_fixture_dispatches==0,"convenience entry hook blocks before the native dispatcher");
    cmd[13]=2;cmd[14]=5;
    check(district_intercept::dispatch(cmd,1)==nullptr && district_fixture_dispatches==1,"allowed transport forwards exactly once");
    cmd[13]=7;cmd[14]=27;corps[0x17]=3;
    check(district_intercept::dispatch(cmd,1)==nullptr && district_fixture_dispatches==2,"attack corps forwards exactly once");
    corps[0x17]=5;
    check(memcmp(saved_cmd,cmd,sizeof(cmd))==0,"command is never modified");
    original_sortie_dispatch=saved_dispatch;district_intercept::original_dispatch=saved_entry;
    functions=saved_functions;mod_features=saved_features;enabled=saved_enabled;
    slot(0xE0D0,50,old_district);slot(0x158,5999,old_actor);putptr(root,0x871E8,old_options);
}
