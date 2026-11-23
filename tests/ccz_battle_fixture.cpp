// User 2026-11-02: execute the actual CCZ receiver/facing/effect code.
#define CCZ_NATIVE_FIXTURE
#include "../src/ccz_module.cpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include "../src/ccz_preview_palette.h"
#include "../src/ccz_appearance_labels.h"
static unsigned checks,original_calls,rolls,stat_calls;
static unsigned af=1,vf=2,next_phase=7,die_roll=0;static bool enemies=true;
static std::array<unsigned char,0x90000> root;
static std::array<unsigned char,0x200> attacker,victim,pa,pv;
static std::array<unsigned char,0x2400> context;
static std::array<unsigned char,0x40> pair,ar,output;
static std::array<unsigned char,0x800> display;
static void check(bool ok,const char *why){++checks;if(!ok){std::fprintf(stderr,"%s\n",why);std::exit(1);}}
static void point(Obj p,unsigned off,Obj value){memcpy(p+off,&value,sizeof(value));}
static void number(Obj p,unsigned off,unsigned value){memcpy(p+off,&value,4);}
static void short_value(Obj p,unsigned off,unsigned short value){memcpy(p+off,&value,2);}
static int __fastcall mock_fix(Obj,Obj,Obj){++original_calls;return static_cast<int>(next_phase);}
static int __fastcall mock_valid(Obj p){return p!=nullptr;}
static unsigned __fastcall mock_force(Obj p){return p==attacker.data()?af:vf;}
static bool __fastcall mock_enemy(unsigned a,unsigned b){return a!=b && enemies;}
static int __fastcall mock_war(Obj,unsigned stat,unsigned mask){check((stat==1 || stat==2) && mask==0xFFF,"effective stat ABI");++stat_calls;return 80;}
static int std_mode=0,std_level=0,expert_mode=0,expert_k=100;
static Obj __fastcall mock_options(){return root.data();}
static int __fastcall mock_std(Obj){return std_mode;}
static int __fastcall mock_std_level(Obj){return std_level;}
static int __fastcall mock_expert(Obj){return expert_mode;}
static Obj __fastcall mock_layout(Obj p){return p;}
static unsigned __fastcall mock_setting(Obj,unsigned id,unsigned){check(id==12,"morale setting id");return static_cast<unsigned>(expert_k);}
static unsigned ccz_fixture_roll(){++rolls;return die_roll;}
// 2026-11-20 band traits: status gauge 2F0E50, morale change 271E70, troop limit 2B4CA0, generic deletion 32D4A0 + 227D30.
static unsigned status_calls,status_kind,morale_calls,removed_world,removed_person;static int status_amount,morale_delta;
static Obj status_target,status_msg,morale_target;
static unsigned long long __fastcall mock_status(Obj to,Obj,unsigned kind,int amount,unsigned long long,Obj msg,int){
    ++status_calls;status_target=to;status_kind=kind;status_amount=amount;status_msg=msg;to[0x23]=static_cast<unsigned char>(to[0x23]+amount);return 1;}
static void __fastcall mock_morale(Obj u,int delta,int,int){++morale_calls;morale_target=u;morale_delta=delta;u[0x1A]=static_cast<unsigned char>(u[0x1A]+delta);}
static int __fastcall mock_troop_cap(Obj){return 4000;}
static unsigned long long __fastcall mock_unlist(Obj w,Obj){check(w==root.data(),"generic deletion gets the world");++removed_world;return 0;}
static void __fastcall mock_delete(Obj p){++removed_person;p[0xE6]=0;}
static void *ccz_fixture_function(unsigned rva){
    switch(rva){
    case 0x2F0E50:return reinterpret_cast<void *>(&mock_status);
    case 0x271E70:return reinterpret_cast<void *>(&mock_morale);
    case 0x2B4CA0:return reinterpret_cast<void *>(&mock_troop_cap);
    case 0x32D4A0:return reinterpret_cast<void *>(&mock_unlist);
    case 0x227D30:return reinterpret_cast<void *>(&mock_delete);
    case 0x170D80:return reinterpret_cast<void *>(&mock_fix);
    case 0x331780:return reinterpret_cast<void *>(&mock_valid);
    case 0x22E690:return reinterpret_cast<void *>(&mock_force);
    case 0x2CC630:return reinterpret_cast<void *>(&mock_enemy);
    case 0x231770:return reinterpret_cast<void *>(&mock_war);
    case 0x3EC1A0:return reinterpret_cast<void *>(&mock_options);
    case 0x3ED7C0:return reinterpret_cast<void *>(&mock_std);
    case 0x3EC480:return reinterpret_cast<void *>(&mock_std_level);
    case 0x3ED810:return reinterpret_cast<void *>(&mock_expert);
    case 0x7AB6A0:return reinterpret_cast<void *>(&mock_layout);
    case 0x3EC920:return reinterpret_cast<void *>(&mock_setting);
    default:std::fprintf(stderr,"unexpected native function %X\n",rva);std::exit(2);
    }
}
static void reset(){
    using namespace ccz_sprites;
    attacker.fill(0);victim.fill(0);pair.fill(0);ar.fill(0);output.fill(0);
    for(auto &b:buffs)b=Buff{};for(auto &u:units)u=Unit{};
    point(attacker.data(),0,image+kArmyVtable);point(victim.data(),0,image+kArmyVtable);
    short_value(attacker.data(),0x12,1);short_value(victim.data(),0x12,2);
    short_value(attacker.data(),0x16,10000);short_value(victim.data(),0x16,10000);
    attacker[0x1A]=victim[0x1A]=100;
    point(root.data(),0x7E838+8,attacker.data());point(root.data(),0x7E838+16,victim.data());
    point(root.data(),0x158+8,pa.data());point(root.data(),0x158+16,pv.data());
    point(root.data(),0x871E8,context.data());
    short_value(context.data(),0x34,200);context[0x36]=1;context[0x37]=1;
    point(pair.data(),0,ar.data());short_value(ar.data(),0,1);number(ar.data(),4,27);
    short_value(pair.data(),8,2);number(pair.data(),12,27);
    short_value(output.data(),0x20,100);short_value(output.data(),0x22,20);
    af=1;vf=2;enemies=true;next_phase=7;die_roll=0;
    units[0].army=attacker.data();units[1].army=victim.data();
    ccz_combat::configure(true,true);
}
static void unchanged(unsigned expected_next=7){
    unsigned r=rolls,s=stat_calls,o=original_calls;
    check(ccz_combat::fix(nullptr,pair.data(),output.data())==static_cast<int>(expected_next),"native next phase preserved");
    check(original_calls==o+1,"original damage correction called exactly once");
    check(word(output.data(),0x20)==100 && word(output.data(),0x22)==20,"ineligible damage unchanged");
    check(rolls==r && stat_calls==s,"ineligible exchange uses no RNG or stat calls");
    check(!ccz_sprites::units[0].crit_at && !ccz_sprites::units[1].block_at,"ineligible exchange publishes no effect");
}
// 2026-11-17 종군 중립군 stage 0: the real CCZ band hooks against mock originals (the gates the convenience module hands over).
static std::array<unsigned char,0x200> band_unit,other_unit,host_unit;
static std::array<unsigned char,0x40> band_hex;
static unsigned band_raw_other=7,band_paint_seen=0,band_writes=0,band_actions=0,band_removes=0,band_remove_seen=0,band_dirty=0;
// band_dirty puts junk above the force byte, as a register could hold it (local217 crash: 22E2A0 reads the whole EAX)
static unsigned __fastcall band_orig_force(Obj p){return band_dirty|(p==band_unit.data()?51u:p==other_unit.data()?51u:band_raw_other);}
static void __fastcall band_orig_paint(Obj p){
    band_paint_seen=ccz_bands::hook_army_force(p);   // the patched 22E690 entry inside 301D00
    ccz_bands::hook_paint_write(nullptr,band_hex.data(),51,0,0,0,0);
}
static void __fastcall band_orig_supply(Obj){}
// 2026-11-20: inside 25A880 the capture roll 2801C0 runs on the commander; the real hook must still see the band (slot not cleared yet)
static int band_remove_capture=-1;
static void __fastcall band_orig_remove(Obj p,Obj,unsigned long long,int,int){++band_removes;band_remove_seen=ccz_bands::hook_army_force(p);
    Obj person=ccz_bands::person_of(word(p,0x12));band_remove_capture=person?(ccz_bands::hook_capture(person,nullptr)?1:0):-1;}
static unsigned long long __fastcall band_orig_write(Obj,Obj,unsigned,int,unsigned,int,int){++band_writes;return 1;}
static int __fastcall band_orig_morale(Obj,int){return 4;}
static void __fastcall band_orig_day(Obj,int){}
static void __fastcall band_orig_action(Obj,int){++band_actions;}
static void __fastcall band_orig_target(Obj,unsigned char,unsigned short){++band_actions;}
static void __fastcall band_orig_unit_target(Obj){}
static unsigned __fastcall band_orig_max_morale(Obj){return 100;}
static unsigned band_captures=0;
static bool __fastcall band_orig_capture(Obj,Obj){++band_captures;return true;}
// 2026-11-22: inside the duel generator 2CCC60 a live band answers its raw force 51 (the original 1..45 filter drops it)
static unsigned band_duel_seen=0;
static Obj band_duel_unit=nullptr;
static void __fastcall band_orig_duel(){band_duel_seen=ccz_bands::hook_army_force(band_duel_unit);}
static bool near_value(float a,float b){return a>b-0.001f && a<b+0.001f;}
static void band_tests(){
    using namespace ccz_bands;
    void *gates[kBandHookCount]={reinterpret_cast<void *>(&band_orig_force),reinterpret_cast<void *>(&band_orig_paint),
        reinterpret_cast<void *>(&band_orig_supply),reinterpret_cast<void *>(&band_orig_remove),reinterpret_cast<void *>(&band_orig_write),
        reinterpret_cast<void *>(&band_orig_morale),reinterpret_cast<void *>(&band_orig_day),reinterpret_cast<void *>(&band_orig_action),
        reinterpret_cast<void *>(&band_orig_target)};
    gates[kBandUnitTarget]=reinterpret_cast<void *>(&band_orig_unit_target);
    {gates[kBandMaxMorale]=nullptr;check(!configure(gates,kFlagOn|kFlagSetters),"configure needs the max morale original (2026-11-20)");}
    gates[kBandMaxMorale]=reinterpret_cast<void *>(&band_orig_max_morale);
    {gates[kBandCapture]=nullptr;check(!configure(gates,kFlagOn|kFlagSetters),"configure needs the capture original (2026-11-20)");}
    gates[kBandCapture]=reinterpret_cast<void *>(&band_orig_capture);
    {gates[kBandDuel]=nullptr;check(!configure(gates,kFlagOn|kFlagSetters),"configure needs the duel generator original (2026-11-22)");}
    gates[kBandDuel]=reinterpret_cast<void *>(&band_orig_duel);
    void *missing[kBandHookCount]={};check(!configure(missing,kFlagOn),"configure refuses missing originals");
    check(!configure(gates,kFlagOn|kFlagSetters|kFlagPortrait),"portrait mode needs the face texture original");
    check(configure(gates,kFlagOn|kFlagSetters),"configure accepts all nine originals");
    ccz_bands::reset();
    short_value(band_unit.data(),0x12,4040);short_value(other_unit.data(),0x12,4040);short_value(host_unit.data(),0x12,77);
    short_value(band_unit.data(),0x16,800);band_unit[0x10]=1;host_unit[0x10]=1;short_value(host_unit.data(),0x16,5000);
    check(hook_army_force(band_unit.data())==51,"no record: a force-51 army answers 51");
    slot_set(4040,band_unit.data(),3,13);   // 2026-11-18: the slot word also carries the band kind for the renderer
    {unsigned k=0,f=0;
     check(ccz_idle::band_of(band_unit.data(),k,f) && k==13 && f==3 && hook_army_force(band_unit.data())==3,"slot word carries kind and patron");
     check(!ccz_idle::band_of(other_unit.data(),k,f) && !ccz_idle::band_of(host_unit.data(),k,f),"other armies are no band");}
    // 2026-11-20: elite kind 13 (함진영) answers max morale 130 and reaches 2.5x at 130; others keep the original cap
    check(hook_max_morale(band_unit.data())==130 && hook_max_morale(host_unit.data())==100,"elite band max morale 130, regular army original");
    band_unit[0x1A]=130;check(near_value(power(band_unit.data()),2.5f*250.0f/330.0f),"elite band at morale 130 = 2.5x the morale-50 term");
    band_unit[0x1A]=60;check(near_value(power(band_unit.data()),1.2f*250.0f/260.0f),"band at morale 60 = 1.2x the morale-50 term");
    std_mode=1;std_level=1;check(near_value(power(band_unit.data()),1.2f*125.0f/135.0f),"standard mode level 1 uses a = 75");
    std_level=0;check(near_value(power(band_unit.data()),1.2f*250.0f/260.0f),"standard mode other level uses a = 200");
    std_mode=0;expert_mode=1;expert_k=150;check(near_value(power(band_unit.data()),1.2f*150.0f/160.0f),"expert setting 150 uses a = 100");
    expert_k=-1;check(near_value(power(band_unit.data()),1.2f*250.0f/260.0f),"expert negative setting uses a = 200");
    expert_mode=0;expert_k=100;
    check(near_value(power(host_unit.data()),1.0f),"a regular army keeps factor 1");
    check(near_value(ccz_band_kinds::power_factor(1,100),2.0f*250.0f/300.0f),"plain band at morale 100 = 2x");
    check(near_value(ccz_band_kinds::power_factor(1,120),ccz_band_kinds::power_factor(1,100)*300.0f/320.0f),"plain band curve stops at 100");
    check(near_value(ccz_band_kinds::power_factor(1,50),1.0f) && near_value(ccz_band_kinds::power_factor(1,0),0.4f*250.0f/200.0f),"morale 50 = original, 0 = 0.4x");
    check(near_value(ccz_band_kinds::power_factor(17,50),1.3f),"mountain kind x1.3");
    check(ccz_band_kinds::trait_of(3)==ccz_band_kinds::kNavy && ccz_band_kinds::trait_of(9)==ccz_band_kinds::kPlain,"kind traits");
    slot_set(4040,band_unit.data(),3);
    check(hook_max_morale(band_unit.data())==100,"kind 0 band keeps the original cap");
    check(hook_army_force(band_unit.data())==3,"live band answers its patron force");
    band_duel_unit=band_unit.data();hook_duel();
    check(band_duel_seen==51 && hook_army_force(band_unit.data())==3,"inside the duel generator a live band answers 51, after it its patron again (2026-11-22)");
    check(hook_army_force(other_unit.data())==51,"another object with the same generic commander stays 51");
    band_raw_other=9;check(hook_army_force(host_unit.data())==9,"regular army answer untouched");
    band_dirty=0xFD626D00u;
    check(hook_army_force(other_unit.data())==51 && hook_army_force(band_unit.data())==3 && hook_army_force(host_unit.data())==9,
          "the answer is the whole 32-bit force number even when the original leaves junk above the byte");
    band_dirty=0;
    // painting scope: the band's own entry answers 51, owner writes over F or F's friends are refused
    enemies=true;band_hex[0x14]=3;band_writes=0;hook_paint(band_unit.data());
    check(band_paint_seen==51 && band_writes==0,"paint scope answers 51 and keeps F's tile");
    band_hex[0x14]=5;hook_paint(band_unit.data());check(band_writes==1,"hostile owner tile is neutralised");
    enemies=false;band_hex[0x14]=6;hook_paint(band_unit.data());check(band_writes==1,"non-hostile owner tile is kept");
    band_hex[0x14]=0;hook_paint(band_unit.data());check(band_writes==2,"unowned tile follows the original write");
    check(hook_army_force(band_unit.data())==3,"scope ends with the painting call");
    band_hex[0x14]=3;hook_paint_write(nullptr,band_hex.data(),51,0,0,0,0);check(band_writes==3,"outside a band scope every write is original");
    hook_paint(host_unit.data());check(band_paint_seen==9 && band_writes==4,"regular army painting untouched");
    enemies=true;
    check(hook_morale(band_unit.data(),1)==0 && hook_morale(host_unit.data(),1)==4,"band loses no periodic morale, others do");
    band_actions=0;hook_set_action(band_unit.data(),3);hook_set_target(band_unit.data(),5,2);
    check(band_actions==0,"foreign order writes to a band are refused");
    own_order=true;hook_set_action(band_unit.data(),1);own_order=false;hook_set_action(host_unit.data(),1);
    check(band_actions==2,"own follow order and regular armies pass");
    // sidecar round trip through the real unit table
    point(root.data(),0x7E838+10*8,band_unit.data());point(root.data(),0x7E838+11*8,host_unit.data());
    bands[0]=Band{};bands[0].state=kLive;bands[0].force=3;bands[0].commander=4040;bands[0].unit_index=10;bands[0].host_index=11;
    bands[0].host_commander=77;bands[0].unit=band_unit.data();bands[0].host=host_unit.data();bands[0].site=1234;bands[0].kind=13;bands[0].rank=2;
    bands[1]=Band{};bands[1].state=kIdle;bands[1].force=2;bands[1].site=4321;bands[1].kind=4;
    unsigned char bytes[8+16*24];unsigned n=save(bytes,sizeof(bytes));
    check(n==8+2*24 && save(bytes,10)==0,"sidecar holds header plus two records and refuses a short buffer");
    ccz_bands::reset();check(hook_army_force(band_unit.data())==51 && bands[0].state==kFree,"reset forgets every band");
    check(load(bytes,n) && hook_army_force(band_unit.data())==3 && bands[1].state==kIdle && bands[1].site==4321,"reload restores live and idle bands");
    {unsigned k=0,f=0;check(bands[0].kind==13 && bands[0].rank==2 && bands[1].kind==4 && ccz_idle::band_of(band_unit.data(),k,f) && k==13,
                            "reload keeps the band kind and title (sidecar p[3]/p[14])");}
    {unsigned char old[8+24];memcpy(old,bytes,sizeof(old));old[6]=1;old[8+3]=200;   // a kind this build does not know -> 산적
     check(load(old,sizeof(old)) && bands[0].kind==0,"unknown kind loads as the legacy 산적");}
    check(load(bytes,n),"reload with kinds");
    bytes[0]^=1;check(!load(bytes,n) && hook_army_force(band_unit.data())==51,"wrong magic loads nothing");bytes[0]^=1;
    check(!load(bytes,n-1),"truncated sidecar rejected");
    short_value(band_unit.data(),0x12,4041);check(load(bytes,n) && bands[0].state==kIdle && hook_army_force(band_unit.data())==51,"reused unit slot drops the live record");
    short_value(band_unit.data(),0x12,4040);check(load(bytes,n),"reload again");
    // removal: inside 25A880 the unit answers 51; afterwards the record no longer answers F
    std::array<unsigned char,0x200> commander_person{};short_value(commander_person.data(),0x10,4040);point(root.data(),0x158+4040*8,commander_person.data());
    band_removes=0;band_captures=0;hook_remove(band_unit.data(),nullptr,0,0,4);
    check(band_removes==1 && band_remove_seen==51 && hook_army_force(band_unit.data())==51,"removal answers 51 and clears the band answer");
    check(band_remove_capture==0 && band_captures==0,"inside the removal the band commander is not captured (2801C0 answers no)");
    check(hook_capture(commander_person.data(),nullptr) && band_captures==1,"after the removal the slot is free: original capture roll");
    point(root.data(),0x158+4040*8,nullptr);
    ccz_bands::reset();
    check(hex_distance(0,1)==1 && hex_distance(0,220)==1 && hex_distance(0,48400)==~0u && hex_distance(221,221)==0,"hex distance");
    // 2026-11-18 바싹 따라붙기: cube coordinates agree with hex_distance; the mirrored tile keeps the band's offset or rings the goal
    for(unsigned t:{0u,221u,4437u,48399u,12345u,12346u}){int x,z;to_cube(t,x,z);check(from_cube(x,z)==t,"cube round trip");}
    {int x,z;to_cube(50*220+40,x,z);const int dx[6]={1,1,0,-1,-1,0},dz[6]={-1,0,1,1,0,-1};
     for(int i=0;i<6;++i)check(hex_distance(50*220+40,from_cube(x+dx[i],z+dz[i]))==1,"cube neighbours are one hex away");
     to_cube(50*220+41,x,z);for(int i=0;i<6;++i)check(hex_distance(50*220+41,from_cube(x+dx[i],z+dz[i]))==1,"cube neighbours (odd column)");}
    static std::array<unsigned char,0x40> land;land.fill(0);short_value(land.data(),0x12,1);
    for(unsigned y=40;y<75;++y)for(unsigned x=30;x<75;++x)point(root.data(),0xE270+(y*220+x)*8,land.data());
    Occupied none{};none.n=0;
    const unsigned host=50*220+40,goal=60*220+60;int hx,hz,gx,gz;to_cube(host,hx,hz);to_cube(goal,gx,gz);
    const unsigned beside=from_cube(hx+1,hz-1);
    check(mirror_tile(beside,host,goal,false,none)==from_cube(gx+1,gz-1),"a band beside its host keeps that offset at the goal");
    const unsigned two=from_cube(hx-2,hz+1);
    check(mirror_tile(two,host,goal,false,none)==from_cube(gx-2,gz+1),"two hexes off: the same two-hex offset");
    const unsigned distant=from_cube(hx+4,hz);unsigned t=mirror_tile(distant,host,goal,false,none);
    check(hex_distance(t,goal)==1,"a band far from its host goes next to the goal");
    for(int i=0;i<6;++i){const int ddx[6]={1,1,0,-1,-1,0},ddz[6]={-1,0,1,1,0,-1};
        check(hex_distance(from_cube(gx+ddx[i],gz+ddz[i]),host)>=hex_distance(t,host),"next to the goal on the host's side");}
    Occupied busy{};busy.n=1;busy.tile[0]=static_cast<unsigned short>(from_cube(gx+1,gz-1));
    t=mirror_tile(beside,host,goal,false,busy);check(t!=from_cube(gx+1,gz-1) && hex_distance(t,goal)==1,"taken offset tile: another tile next to the goal");
    t=mirror_tile(beside,host,goal,true,none);check(hex_distance(t,goal)==2,"a city or gate goal is ringed outside its seven tiles");
    check(mirror_tile(beside,host,10*220+10,false,none)==kNoHex,"no land around the goal: nothing");
}
// 2026-11-20 user "포로나 그런거 되면 야전에서 즉시 사라져야해", "청주병,서량기병에 한해서 성앞에 대기", "등갑병 등 그런거 추천해준것도
// 다 구현해", "매력으로 빨리 소집되는건 지워줘": capture refusal, kind traits on the real hooks, stray cleanup.
static std::array<unsigned char,0x200> tu[12],tp[12];
static void person_name(Obj p,const wchar_t *s){memset(p+0x12,0,14);memcpy(p+0x12,s,wcslen(s)*2);}
static void trait_tests(){
    using namespace ccz_bands;using namespace ccz_band_kinds;
    ccz_bands::reset();flags=kFlagOn|kFlagSetters;band_raw_other=9;band_dirty=0;enemies=true;
    for(unsigned i=0;i<=500;++i)point(root.data(),0x7E838+i*8,nullptr);
    for(auto &a:tu)a.fill(0);
    for(auto &a:tp)a.fill(0);
    // helpers
    check(waits_at_base(1) && waits_at_base(25) && !waits_at_base(2) && !waits_at_base(13) && !waits_at_base(0),"only 청주병 and 서량기병 wait at a base");
    check(shock_for(2,-10)==-15 && shock_for(9,10)==15 && shock_for(13,-10)==-5 && shock_for(13,10)==10 && shock_for(1,-10)==-10,"morale shock by kind");
    check(troop_cap_pct(9)==130 && troop_cap_pct(4)==70 && troop_cap_pct(1)==100,"idle full troops by kind");
    check(growth_days(5)==21 && growth_days(1)==30 && growth_days(13)==30,"days to full: 30, rabble 21 (no 매력 term)");
    check(heal_amount(0,1000,5000)==0 && heal_amount(500,1000,5000)==100 && heal_amount(2000,1000,5000)==200 && heal_amount(50,1000,5000)==50,"heal 10%, at least 100, at most the wounded");
    check(heal_amount(2000,4950,5000)==50 && heal_amount(2000,5000,5000)==0,"heal stops at the troop limit");
    check(fire_amount(21,500)==1500 && fire_amount(21,9000)==20000 && fire_amount(1,500)==500 && fire_amount(21,0)==0,"rattan armour fire x3, capped");
    check(near_value(exchange_factor(25,1,0),1.2f) && near_value(exchange_factor(7,5,0),0.8f) && near_value(exchange_factor(26,4,0),1.0f) &&
          near_value(exchange_factor(0,1,21),0.7f) && near_value(exchange_factor(25,2,21),0.84f),"exchange factor: cavalry terrain, armour target");
    {const unsigned keep_low=idle_low,keep_full=idle_full;idle_low=500;idle_full=5000;
     Band b{};b.created=0;b.kind=1;
     check(idle_top(b)==5000 && idle_troops(b,10)==2000 && idle_troops(b,30)==5000,"plain idle growth: 30 days to 5000");
     b.kind=9;check(idle_top(b)==6500 && idle_troops(b,20)<6500 && idle_troops(b,21)==6500,"rabble idle growth: 130% in 21 days");
     b.kind=4;check(idle_top(b)==3500 && idle_troops(b,30)==3500,"elite idle growth: 70%");
     idle_low=keep_low;idle_full=keep_full;}
    // capture: a live band commander answers no, everyone else asks the original
    Obj cap_person=tp[0].data();short_value(cap_person,0x10,4050);point(root.data(),0x158+4050*8,cap_person);
    Obj plain_person=tp[1].data();short_value(plain_person,0x10,77);point(root.data(),0x158+77*8,plain_person);
    Obj a=tu[0].data();short_value(a,0x12,4050);a[0x10]=1;short_value(a,0x16,2000);
    band_captures=0;
    check(hook_capture(cap_person,plain_person) && band_captures==1,"no slot yet: the original capture roll");
    slot_set(4050,a,3,25);
    check(!hook_capture(cap_person,plain_person) && band_captures==1,"live band commander is never captured");
    check(hook_capture(plain_person,cap_person) && band_captures==2,"another officer: original capture roll");
    {std::array<unsigned char,0x200> fake{};short_value(fake.data(),0x10,4050);
     check(hook_capture(fake.data(),plain_person) && band_captures==3,"an object that is not the registered person: original");}
    // exchange: cavalry attacker on its terrain, rattan-armour target, confusion
    static std::array<unsigned char,0x40> plain_hex,forest_hex;plain_hex.fill(0);forest_hex.fill(0);plain_hex[0x10]=1;forest_hex[0x10]=5;
    const unsigned ta=100*220+100;short_value(a,0x2A,static_cast<unsigned short>(ta));point(root.data(),0xE270+ta*8,plain_hex.data());
    Obj reg=tu[1].data();short_value(reg,0x12,78);reg[0x10]=1;short_value(reg,0x16,3000);
    Obj armour=tu[2].data();short_value(armour,0x12,4051);armour[0x10]=1;short_value(armour,0x16,2000);slot_set(4051,armour,3,21);
    int d=1000;exchange(a,reg,&d);check(d==1200,"서량기병 on plain: x1.2");
    point(root.data(),0xE270+ta*8,forest_hex.data());d=1000;exchange(a,reg,&d);check(d==800,"서량기병 in forest: x0.8");
    d=1000;exchange(reg,armour,&d);check(d==700,"등갑병 takes x0.7 from a normal attack");
    d=1000;exchange(armour,reg,&d);check(d==1000,"등갑병 attacking: no change");
    d=1;exchange(reg,armour,&d);check(d==1,"damage never drops below 1");
    flags=0;d=1000;exchange(reg,armour,&d);check(d==1000,"bands off: no change");flags=kFlagOn|kFlagSetters;
    Obj snake=tu[3].data();short_value(snake,0x12,4052);snake[0x10]=1;short_value(snake,0x16,2000);slot_set(4052,snake,3,24);
    Obj marker=reinterpret_cast<Obj>(0x1234567ull);point(image,0x1A4AB88,marker);
    status_calls=0;die_roll=0;reg[0x23]=0;d=1000;exchange(snake,reg,&d);
    check(d==1000 && status_calls==1 && status_target==reg && status_kind==0 && status_amount==40 && status_msg==marker && reg[0x23]==40,
          "독사대 hit: 10% roll passes, confusion +40 through 2F0E50 with the no-message sentinel");
    die_roll=100;d=1000;exchange(snake,reg,&d);check(status_calls==1,"roll 100 of 1000 fails: no confusion");
    die_roll=0;enemies=false;exchange(snake,reg,&d);check(status_calls==1,"non-hostile target: no confusion");enemies=true;
    d=1000;exchange(snake,armour,&d);check(status_calls==1 && d==700,"same patron: no confusion, armour still applies");
    reg[0x10]=0;d=1000;exchange(snake,reg,&d);check(status_calls==1,"a city record is no confusion target");reg[0x10]=1;
    point(image,0x1A4AB88,nullptr);die_roll=0;
    // fire
    check(fire(armour,500)==1500 && fire(reg,500)==500 && fire(a,500)==500,"fire x3 only on rattan armour");
    flags=0;check(fire(armour,500)==500,"bands off: fire unchanged");flags=kFlagOn|kFlagSetters;
    // 위압: one hostile army at distance 1 loses 3 morale; same patron or two hexes away keep theirs
    ccz_bands::reset();
    const unsigned tw=100*220+120;
    Obj beast=tu[4].data();short_value(beast,0x12,4054);beast[0x10]=1;short_value(beast,0x16,2000);short_value(beast,0x2A,static_cast<unsigned short>(tw));
    Obj near_enemy=tu[5].data();short_value(near_enemy,0x12,79);near_enemy[0x10]=1;short_value(near_enemy,0x16,3000);near_enemy[0x1A]=60;
    short_value(near_enemy,0x2A,static_cast<unsigned short>(tw+1));
    Obj far_enemy=tu[6].data();short_value(far_enemy,0x12,83);far_enemy[0x10]=1;short_value(far_enemy,0x16,3000);far_enemy[0x1A]=60;
    short_value(far_enemy,0x2A,static_cast<unsigned short>(tw+2));
    Obj mate=tu[7].data();short_value(mate,0x12,4056);mate[0x10]=1;short_value(mate,0x16,1000);mate[0x1A]=60;short_value(mate,0x2A,static_cast<unsigned short>(tw-1));
    point(root.data(),0x7E838+20*8,beast);point(root.data(),0x7E838+21*8,near_enemy);point(root.data(),0x7E838+22*8,far_enemy);point(root.data(),0x7E838+23*8,mate);
    slot_set(4054,beast,3,22);slot_set(4056,mate,3,1);
    bands[0]=Band{};bands[0].state=kLive;bands[0].force=3;bands[0].commander=4054;bands[0].unit=beast;bands[0].unit_index=20;bands[0].kind=22;
    morale_calls=0;daily_traits();
    check(morale_calls==1 && morale_target==near_enemy && morale_delta==-3 && near_enemy[0x1A]==57 && far_enemy[0x1A]==60 && mate[0x1A]==60,
          "맹수대: adjacent hostile army -3 morale, others untouched");
    bands[0].kind=1;daily_traits();check(morale_calls==1,"a kind without 위압 does nothing");
    // 치료: friendly armies within 2 hexes turn wounded into troops up to the commander limit
    ccz_bands::reset();
    for(unsigned i=20;i<24;++i)point(root.data(),0x7E838+i*8,nullptr);
    const unsigned th=110*220+100;
    Obj healer=tu[8].data();short_value(healer,0x12,4055);healer[0x10]=1;short_value(healer,0x16,2000);short_value(healer,0x2A,static_cast<unsigned short>(th));
    Obj r1=tu[9].data();short_value(r1,0x12,80);r1[0x10]=1;short_value(r1,0x16,3000);short_value(r1,0x18,2000);short_value(r1,0x2A,static_cast<unsigned short>(th+2));
    Obj r2=tu[10].data();short_value(r2,0x12,81);r2[0x10]=1;short_value(r2,0x16,3990);short_value(r2,0x18,50);short_value(r2,0x2A,static_cast<unsigned short>(th+1));
    Obj r3=tu[11].data();short_value(r3,0x12,82);r3[0x10]=1;short_value(r3,0x16,1000);short_value(r3,0x18,2000);short_value(r3,0x2A,static_cast<unsigned short>(th+3));
    for(unsigned i=0;i<3;++i){Obj p=tp[2+i].data();short_value(p,0x10,static_cast<unsigned short>(80+i));point(root.data(),0x158+(80+i)*8,p);}
    point(root.data(),0x7E838+30*8,healer);point(root.data(),0x7E838+31*8,r1);point(root.data(),0x7E838+32*8,r2);point(root.data(),0x7E838+33*8,r3);
    slot_set(4055,healer,3,19);
    bands[0]=Band{};bands[0].state=kLive;bands[0].force=3;bands[0].commander=4055;bands[0].unit=healer;bands[0].unit_index=30;bands[0].kind=19;
    enemies=true;daily_traits();check(word(r1,0x16)==3000 && word(r1,0x18)==2000,"오두미도: a hostile army is not healed");
    enemies=false;daily_traits();
    check(word(r1,0x16)==3200 && word(r1,0x18)==1800,"오두미도: 10% of the wounded back to troops");
    check(word(r2,0x16)==4000 && word(r2,0x18)==40,"오두미도: capped by the troop limit 2B4CA0");
    check(word(r3,0x16)==1000 && word(r3,0x18)==2000 && word(healer,0x16)==2000,"three hexes away or no wounded: untouched");
    enemies=true;
    for(unsigned i=30;i<34;++i)point(root.data(),0x7E838+i*8,nullptr);
    // stray cleanup: only former band commanders out as 재야/포로, not held by any record or slot
    ccz_bands::reset();
    struct Stray{unsigned id;unsigned char status;const wchar_t *family;bool gone;} strays[]={
        {4060,5,L"산월",true},{4061,6,L"청주병",true},{4062,5,L"엄",false},{4063,4,L"산월",false},{4064,6,L"산월",false},{4065,6,L"독사대",false}};
    for(unsigned i=0;i<6;++i){Obj p=tp[5+i].data();p[0xE6]=0;short_value(p,0x10,static_cast<unsigned short>(strays[i].id));p[0xE6]=strays[i].status;
        person_name(p,strays[i].family);point(root.data(),0x158+strays[i].id*8,p);}
    bands[0]=Band{};bands[0].state=kIdle;bands[0].commander=4064;   // 4064 still held by an idle record
    slot_set(4065,tu[0].data(),3,24);short_value(tu[0].data(),0x12,4065);   // 4065 still owns a live army
    removed_world=removed_person=0;clean_strays();
    check(removed_world==2 && removed_person==2,"two strays removed (32D4A0 + 227D30 each)");
    for(unsigned i=0;i<6;++i)check((tp[5+i][0xE6]==0)==strays[i].gone,"stray cleanup picks only free former band commanders");
    removed_world=removed_person=0;clean_strays();check(!removed_world && !removed_person,"a second pass finds nothing");
    for(unsigned i=0;i<6;++i)point(root.data(),0x158+strays[i].id*8,nullptr);
    point(root.data(),0x158+4050*8,nullptr);point(root.data(),0x158+77*8,nullptr);
    for(unsigned i=0;i<3;++i)point(root.data(),0x158+(80+i)*8,nullptr);
    point(root.data(),0xE270+ta*8,nullptr);
    ccz_bands::reset();
}
// 2026-11-18 지역 특색 (ccz_band_kinds.h): table sanity, province draws, base provinces.
static void kind_tests(){
    using namespace ccz_band_kinds;
    const unsigned labels=sizeof(ccz_appearance::class_labels)/sizeof(ccz_appearance::class_labels[0]);
    for(unsigned k=0;k<kKinds;++k) {
        check(kinds[k].cls<labels,"kind class is a known CCZ class");
        if(!kinds[k].family)continue;
        check(wcslen(kinds[k].family)>=1 && wcslen(kinds[k].family)<=6,"surname fits the 7-wchar name field");
        for(const wchar_t *r:kinds[k].ranks)check(r && wcslen(r)>=1 && wcslen(r)<=6,"title fits the 7-wchar name field");
        check(kinds[k].troops_lo>=500 && kinds[k].troops_hi>=kinds[k].troops_lo && kinds[k].troops_hi<=5000,"troop range");
        check(kinds[k].lead>=40 && kinds[k].lead<=80 && kinds[k].war>=40 && kinds[k].war<=80,"leader floor range");
    }
    check(!kinds[0].family && kinds[0].cls==48,"kind 0 is the legacy 산적 (old sidecar records)");
    for(unsigned s=0;s<23;++s)for(unsigned i=0;i<3;++i)check(pools[s][i].kind<kKinds,"pool kind exists");
    for(unsigned s=1;s<23;++s)for(unsigned r:{0u,333u,666u,999u})check(pick_kind(s,r)!=0,"every province draws a regional kind");
    check(pick_kind(7,0)==1 && pick_kind(7,999)==9,"연주: 청주병, rarely 황건적");
    check(pick_kind(8,0)==4 && pick_kind(16,0)==18 && pick_kind(11,0)==25 && pick_kind(17,999)==22,"예주 호표기, 익주 백이병, 량주 서량기병, 남중 맹수대");
    check(pick_kind(0,500)==0 && pick_kind(99,500)==0,"unknown province: 산적");
    check(state_of_base(16)==8 && state_of_base(44)==16 && state_of_base(24)==11 && state_of_base(1)==1 && state_of_base(51)==22,"city provinces");
    check(state_of_base(52)==2 && state_of_base(61)==16 && state_of_base(62)==0 && state_of_base(0)==0,"gate provinces");
    check(class_of(200)==48 && class_of(1)==120 && class_of(4)==127,"class lookup and fallback");
}
// 2026-11-18 flag portrait (ccz_portrait.h): zp1 container + GT1G0600 head + solid BC3 blocks, read back the way 30AB30 reads it
// (2026-11-19): a chunk before the last whose size entry is at least the chunk size is copied raw (that many bytes), others are zlib
// (here: stored blocks only). local231 wrote stored zlib larger than 262144 there and the game copied the zlib bytes as the picture.
static bool unpack_stored(const unsigned char *zp,size_t size,std::vector<unsigned char> &raw){
    using namespace ccz_portrait;
    if(size<kZpHead || memcmp(zp,"zp1",4))return false;
    const unsigned total=static_cast<unsigned>(dword(const_cast<unsigned char *>(zp),4)),chunk=static_cast<unsigned>(dword(const_cast<unsigned char *>(zp),8));
    const unsigned n=static_cast<unsigned>(dword(const_cast<unsigned char *>(zp),12));
    if(!chunk || !n || (zp[3]&2))return false;   // bit 2 of byte 3 (last chunk raw) is not used by the builder
    size_t p=kZpHead;raw.clear();
    for(unsigned i=0;i<n;++i) {
        const unsigned stored=static_cast<unsigned>(dword(const_cast<unsigned char *>(zp),16+i*4));
        const unsigned piece=i+1<n?chunk:(total%chunk?total%chunk:chunk);
        if(i+1<n && stored>=piece) {   // 30AC72: memcpy(out, chunk, piece)
            if(p+piece>size)return false;
            raw.insert(raw.end(),zp+p,zp+p+piece);p=align(p+stored);continue;
        }
        if(p+stored>size || dword(const_cast<unsigned char *>(zp),static_cast<unsigned>(p))!=stored-4 || zp[p+4]!=120 || zp[p+5]!=1)return false;
        size_t q=p+6;const size_t start=raw.size();bool last=false;
        while(!last) {
            last=(zp[q]&1)!=0;if(zp[q]&6)return false;
            const unsigned len=zp[q+1]|(zp[q+2]<<8),nlen=zp[q+3]|(zp[q+4]<<8);if((len^nlen)!=65535)return false;
            raw.insert(raw.end(),zp+q+5,zp+q+5+len);q+=5+len;
        }
        unsigned a=1,b=0;for(size_t k=start;k<raw.size();++k){a=(a+raw[k])%65521;b=(b+a)%65521;}
        const unsigned adler=(static_cast<unsigned>(zp[q])<<24)|(zp[q+1]<<16)|(zp[q+2]<<8)|zp[q+3];
        if(adler!=((b<<16)|a) || q+4!=p+stored)return false;
        p=align(p+stored);
    }
    return raw.size()==total && p==size;
}
static unsigned long long portrait_calls;static Obj portrait_seen;
static unsigned long long __fastcall band_orig_face(Obj,Obj bytes,int,unsigned){++portrait_calls;portrait_seen=bytes;return 1;}
static void portrait_tests(){
    using namespace ccz_portrait;
    check(face_of(1)==6187 && face_of(28)==6214 && !face_of(0) && !face_of(29) && kind_of_face(6200)==14 && !kind_of_face(6186),"portrait faces 6187~6214 = kinds 1~28");
    check(ccz_band_kinds::kKinds-1==kLastFace-kFirstFace+1,"one portrait face per band kind");
    unsigned char pic[48*48]={};unsigned pal[256]={};
    pal[3]=0xFF0000FFu;pal[5]=0xFF00FF00u;
    for(unsigned y=10;y<42;++y)for(unsigned x=14;x<34;++x)pic[y*48+x]=x<24?3:5;   // 20x32 figure: red left half, green right half
    const Picture p{pic,48,48,48,pal};
    for(unsigned kind:{9u,10u,11u}) {
        const Size s=size_of(kind);Place pl{};
        check(place(p,s,pl) && pl.scale%4==0 && pl.x0%4==0 && pl.y0%4==0 && pl.bw==20 && pl.bh==32,"picture placed on the 4-pixel grid");
        check(pl.bw*pl.scale<=s.w*7/8 && pl.bh*pl.scale<=s.h*7/8 && ((pl.scale+4)*pl.bw>s.w*7/8 || (pl.scale+4)*pl.bh>s.h*7/8),"largest multiple-of-4 scale within 7/8");
        std::vector<unsigned char> zp(zp1_size(s));
        check(build(p,s,zp.data(),zp.size()),"portrait builds");
        check(!build(p,s,zp.data(),zp.size()-1),"short buffer refused");
        std::vector<unsigned char> raw;
        check(unpack_stored(zp.data(),zp.size(),raw) && raw.size()==texture_size(s),"zp1 chunks unpack to one texture");
        check(dword(zp.data(),8)==kChunk && dword(zp.data(),12)==chunk_count(texture_size(s)),"zp1 head: chunk size and count");
        bool plain=true;for(unsigned i=0;i+1<chunk_count(texture_size(s));++i)plain=plain && dword(zp.data(),16+i*4)==kChunk;
        check(plain && dword(zp.data(),16+(chunk_count(texture_size(s))-1)*4)<kChunk,"chunks before the last are raw 262144 (30AB30 copies them), the last is zlib");
        check(!memcmp(raw.data(),"GT1G0600",8) && dword(raw.data(),8)==raw.size() && raw[36]==16 && raw[37]==91,"GT1G head: size, BC3 (91)");
        check(raw[38]==(log2u(s.w)|(log2u(s.h)<<4)) && raw[55]==1,"GT1G head: log2 size byte as the original faces");
        auto block=[&](unsigned x,unsigned y){return raw.data()+kHeader+((y/4)*(s.w/4)+x/4)*16;};
        const unsigned char *corner=block(0,0);bool clear=true;for(unsigned i=0;i<16;++i)clear=clear && !corner[i];
        check(clear,"outside the figure: transparent block");
        const unsigned char *red=block(pl.x0,pl.y0),*green=block(pl.x0+pl.bw*pl.scale-4,pl.y0+pl.bh*pl.scale-4);
        check(red[0]==255 && red[1]==255 && red[8]==0 && red[9]==248 && red[10]==0 && red[11]==248,"left point: opaque solid red (565)");
        check(green[8]==224 && green[9]==7,"right point: solid green (565)");
        char dump[MAX_PATH];   // offline check with a real zlib (origlib.bin0005.unpack_zp1): CCZ_PORTRAIT_DUMP=<folder>
        if(GetEnvironmentVariableA("CCZ_PORTRAIT_DUMP",dump,MAX_PATH-16)) {
            char path[MAX_PATH];sprintf_s(path,"%s\\portrait_%u.zp1",dump,kind);
            HANDLE f=CreateFileA(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);
            if(f!=INVALID_HANDLE_VALUE){DWORD w=0;WriteFile(f,zp.data(),static_cast<DWORD>(zp.size()),&w,nullptr);CloseHandle(f);}
        }
    }
    unsigned char empty[48*48]={};
    std::vector<unsigned char> zp(zp1_size(size_of(10)));
    check(!build(Picture{empty,48,48,48,pal},size_of(10),zp.data(),zp.size()),"an empty picture builds nothing");
    // The hook: without the portrait flag every call goes to the original with its own bytes.
    using namespace ccz_bands;
    unsigned char bytes[4]={};
    original.face_texture=&band_orig_face;const unsigned keep=flags;flags=kFlagOn;
    check(hook_face_texture(nullptr,bytes,5,0)==1 && portrait_calls==1 && portrait_seen==bytes,"no portrait flag: original call, original bytes");
    flags=kFlagOn|kFlagPortrait;
    check(hook_face_texture(nullptr,bytes,5,0)==1 && portrait_calls==2 && portrait_seen==bytes,"no manager: original call");
    flags=keep;
}
// 2026-11-17 idle band pictures: hex centres, the grid fit learnt from standing armies, ground height memory, idle site publication.
static void ghost_tests(){
    using namespace ccz_sprites;
    double cx,cz,x,z;
    ghost::centre(10*220+5,cx,cz);
    check(cx==37594.0+5*702.0 && std::fabs(cz-(13335.3+10*ghost::kRow))<1e-6,"odd column centre (no half row)");
    ghost::centre(10*220+4,cx,cz);
    check(std::fabs(cz-(13335.3+10*ghost::kRow+ghost::kRow/2))<1e-6,"even column centre is half a row lower");
    ghost::apply(cx,cz,x,z);check(x==cx && z==cz,"identity until the fit is trusted");
    std::array<unsigned char,0x200> army{};
    const unsigned first_tile=(20)*220+30;
    for(unsigned i=0;i<40;++i) {   // a turned grid: world x = centre z, world z = 200000 - centre x
        const unsigned tile=(20+i%7)*220+(30+(i*3)%11);short_value(army.data(),0x2A,static_cast<unsigned short>(tile));
        ghost::centre(tile,cx,cz);
        const float pos[3]={static_cast<float>(cz),500.0f+static_cast<float>(i),static_cast<float>(200000.0-cx)};
        Unit u{};ghost::sample(u,army.data(),pos,100000+i*2000ull,false);
    }
    check(ghost::fitted,"fit trusted after samples spread over hexes");
    ghost::centre(25*220+33,cx,cz);ghost::apply(cx,cz,x,z);
    check(std::fabs(x-cz)<2 && std::fabs(z-(200000.0-cx))<2,"fit recovers a turned grid for an unsampled hex");
    {Unit u{};u.moved=99000;short_value(army.data(),0x2A,static_cast<unsigned short>(first_tile));const float pos[3]={1,2,3};
     const double before=ghost::sn;ghost::sample(u,army.data(),pos,100000,false);check(ghost::sn==before,"an army that moved within 1.5 s gives no sample");}
    float y=0;
    check(ghost::height_at(first_tile,y) && y==500.0f,"height of the army that stood on the hex");
    check(ghost::height_at(first_tile+2*220,y),"height from a sampled hex within 6");
    check(!ghost::height_at(150*220+150,y) && y>500.0f && y<540.0f,"far hex: mean sampled height, reported as a guess");
    using namespace ccz_bands;
    ccz_bands::reset();check(!ghost::wanted(),"no idle record, no picture wanted");
    bands[0]=Band{};bands[0].state=kIdle;bands[0].site=4321;bands[1]=Band{};bands[1].state=kLive;bands[1].site=99;
    bands[0].kind=13;
    publish_idle();
    check(ccz_idle::sites[0]==4322 && ccz_idle::sites[1]==0 && ghost::wanted(),"only idle records are published (site+1)");
    check(ccz_idle::site_kinds[0]==13 && ccz_idle::site_kinds[1]==0,"the idle site carries its band kind (same class as the follower)");
    ccz_bands::reset();check(!ccz_idle::sites[0] && !ccz_idle::site_kinds[0] && !ghost::wanted(),"reset clears the published sites");
}
int main(){
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x2800000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    check(image!=nullptr,"fixture image allocated");point(image,0x2780F60,root.data());
    reset();af=vf=1;unchanged();
    reset();enemies=false;unchanged(); // treaty/coalition, different forces
    reset();af=52;unchanged();
    reset();vf=255;unchanged();
    reset();short_value(victim.data(),0x16,0);unchanged();
    reset();short_value(attacker.data(),0x16,0);unchanged();
    reset();number(pair.data(),12,5);unchanged(); // city is not a field defender
    reset();number(ar.data(),4,5);unchanged();
    reset();short_value(pair.data(),8,0);unchanged();
    reset();short_value(pair.data(),8,501);unchanged();
    reset();point(victim.data(),0,nullptr);unchanged();
    reset();short_value(pair.data(),8,1);unchanged(); // same object, different record
    reset();next_phase=6;unchanged(6);
    reset();ccz_combat::configure(false,false);unchanged();
    reset();short_value(output.data(),0x20,0);short_value(output.data(),0x22,0);
    unsigned r=rolls;ccz_combat::fix(nullptr,pair.data(),output.data());check(rolls==r,"zero damage never rolls");
    reset();ccz_combat::fix(nullptr,pair.data(),output.data());
    check(!word(output.data(),0x20) && !word(output.data(),0x22),"hostile block nullifies both damage components");
    bool guard=false,fury=false;int day=ccz_sprites::game_day_read();
    ccz_sprites::buff_read(victim.data(),day,guard,fury);check(guard,"block retains remaining charges");
    unsigned saved=ccz_sprites::buffs[0].guard;af=vf=1;short_value(output.data(),0x20,100);short_value(output.data(),0x22,20);
    r=rolls;ccz_combat::fix(nullptr,pair.data(),output.data());
    check(word(output.data(),0x20)==100 && ccz_sprites::buffs[0].guard==static_cast<int>(saved) && rolls==r,"friendly exchange cannot spend an existing block charge");
    af=1;vf=2;die_roll=999;
    for(unsigned i=0;i<7;++i){short_value(output.data(),0x20,100);short_value(output.data(),0x22,20);ccz_combat::fix(nullptr,pair.data(),output.data());check(!word(output.data(),0x20) && !word(output.data(),0x22),"remaining block charge is used only by an actual hostile hit");}
    ccz_sprites::buff_read(victim.data(),day,guard,fury);check(!guard,"eight actual blocks exhaust the charge state");
    short_value(output.data(),0x20,100);short_value(output.data(),0x22,20);ccz_combat::fix(nullptr,pair.data(),output.data());
    check(word(output.data(),0x20)==100 && word(output.data(),0x22)==20,"ninth hit with a failed roll is not blocked");
    reset();ccz_combat::configure(false,true);ccz_combat::fix(nullptr,pair.data(),output.data());
    check(word(output.data(),0x20)==150 && word(output.data(),0x22)==30 && ccz_sprites::units[0].crit_at,"hostile critical scales final components and marks actual attacker");
    reset();ccz_combat::configure(false,true);die_roll=999;
    ccz_sprites::buff_use(attacker.data(),day,false,true);
    ccz_combat::fix(nullptr,pair.data(),output.data());
    check(word(output.data(),0x20)==150 && ccz_sprites::buffs[0].fury==6,"existing critical charge works on next hostile hit without a new roll");
    reset();display.fill(0);attacker[0x1B]=1;point(attacker.data(),0x148,display.data());
    point(display.data(),0,image+ccz_sprites::kDisplayVtable);point(display.data(),0x40,attacker.data());
    float pos[3]={100,0,100},angle=0;memcpy(display.data()+0x90,pos,sizeof(pos));memcpy(display.data()+0x744,&angle,4);
    (void)ccz_sprites::capture(display.data(),nullptr);
    pos[0]+=100;angle=1.5707963f;memcpy(display.data()+0x90,pos,sizeof(pos));memcpy(display.data()+0x744,&angle,4);
    (void)ccz_sprites::capture(display.data(),nullptr);
    check(ccz_sprites::units[0].fx>0.999f,"actual capture follows native eastward movement");
    angle=3.14159265f;memcpy(display.data()+0x744,&angle,4);number(display.data(),0x60,2);
    (void)ccz_sprites::capture(display.data(),nullptr);
    check(ccz_sprites::units[0].fz<-0.999f && std::fabs(ccz_sprites::units[0].fx)<0.001f,"actual stationary attack capture faces the native target direction");
    angle=-1.5707963f;memcpy(display.data()+0x744,&angle,4);number(display.data(),0x60,0);short_value(attacker.data(),0x16,9000);
    (void)ccz_sprites::capture(display.data(),nullptr);
    check(ccz_sprites::units[0].fx<-0.999f && ccz_sprites::units[0].hurt_at,"actual stationary hurt capture follows native defence direction");
    {   // 2026-11-16 per-unit proof (user: CCZ sprites vanished together with the 3D model and never came back)
        using namespace ccz_sprites;
        auto hides=[&](){camera_seen=GetTickCount64();return capture(display.data(),nullptr);};
        ULONGLONG now=GetTickCount64();
        InterlockedExchange(&hide_model,1);fallback_enabled=false;InterlockedExchange(&covered,0);failed_cells.clear();
        InterlockedExchange64(&hide_since,static_cast<LONGLONG>(now-10000));   // the episode grace is long over
        units[0].first=now;units[0].drawn_at=0;
        check(hides(),"a new unit hides the model during its short trial before the first draw");
        units[0].first=now-5000;InterlockedExchange64(&last_submit,static_cast<LONGLONG>(now));
        check(!hides(),"after the trial a unit never drawn keeps the native model even while other units draw");
        units[0].drawn_at=GetTickCount64();
        check(hides(),"a unit drawn within 1 s hides the model");
        InterlockedExchange(&covered,1);
        check(!hides(),"a run of batches passing no pixel brings the model back without strict mode");
        InterlockedExchange(&covered,0);failed_cells[units[0].cell]=CellFail{now+60000,1};
        check(!hides(),"a unit whose picture failed to load keeps the model");
        failed_cells.clear();units[0].drawn_at=now-5000;
        check(!hides(),"a draw older than 1 s brings the model back");
        units[0].drawn_at=GetTickCount64();InterlockedExchange64(&last_submit,static_cast<LONGLONG>(now-5000));
        check(!hides(),"no batch for over 1 s brings the model back");
        InterlockedExchange64(&last_submit,static_cast<LONGLONG>(GetTickCount64()));
        units[0].cell^=1;check(hides() && units[0].drawn_at==0,"a new picture drops the old proof and starts its own trial");
        units[0].seen=GetTickCount64()-5000;units[0].first=now-5000;units[0].drawn_at=now-5000;
        check(hides() && units[0].first+1000>GetTickCount64(),"a unit back after a capture gap gets a new trial instead of a model flash");
        units[0].drawn_at=GetTickCount64();fallback_enabled=true;InterlockedExchange64(&last_draw_ok,0);units[0].first=now-5000;
        check(!hides(),"strict mode needs a passing occlusion query as well");
        InterlockedExchange64(&last_draw_ok,static_cast<LONGLONG>(GetTickCount64()));
        check(hides(),"strict mode hides once the query passed");
        fallback_enabled=false;InterlockedExchange64(&last_draw_ok,0);InterlockedExchange64(&last_submit,0);InterlockedExchange64(&hide_since,0);
    }
    float far_pos[3]{},far_yaw=0;
    check(ccz_sprites::far_position(attacker.data(),far_pos,far_yaw) && far_yaw==angle && far_pos[0]==pos[0],"opt-in far snapshot reads position and native yaw together");
    using namespace ccz_sprites;Unit u;u.fx=1;u.fz=0;u.facing=true;
    check(native_facing(u,3.14159265f) && std::fabs(u.fx)<0.001f && u.fz<-0.999f,"stationary native turn replaces last eastward march direction");
    check(native_facing(u,1.5707963f) && u.fx>0.999f && std::fabs(u.fz)<0.001f,"native quarter turn");
    check(native_facing(u,-1.048f) && u.fx<0 && u.fz>0,"negative native angle uses the live observed convention");
    float old=u.yaw;check(!native_facing(u,std::numeric_limits<float>::quiet_NaN()) && u.yaw==old,"invalid native angle cannot overwrite finite facing");
    u=Unit{};u.guard=u.fury=true;u.morale=100;
    auto frame=frame_for(u,0,false,10000);
    check(frame.row==6*48 && !frame.flash && !effect_visible(u,false,10000) && !effect_visible(u,true,10000),"remaining charges alone show neither guard pose nor critical flash/labels");
    u.state=2;check(!frame_for(u,0,false,10000).flash,"ordinary attack with unused critical charges does not flash");
    u.block_at=100;u.crit_at=100;
    check(effect_visible(u,false,100) && effect_visible(u,true,100) && frame_for(u,0,false,200).row==kSpecialRow,"actual hit has a short effect window");
    check(!effect_visible(u,false,800) && !effect_visible(u,true,800) && !frame_for(u,0,false,800).flash,"effects stop after 700ms even with charges left");
    // User 2026-11-02: execute the real identity resolver, not only a policy mirror.
    reset();std::array<unsigned char,0x100> corps{},force{};
    point(root.data(),0xE0D0+8,corps.data());point(root.data(),0xDF30+8,force.data());corps[0x10]=1;pa[0xE0]=1;
    point(root.data(),0x158+663*8,pa.data());short_value(attacker.data(),0x12,663);
    appearance=ccz_appearance::Settings{};
    for(unsigned form=0;form<21;++form)for(unsigned lead: {69u,70u,89u,90u}){
        pa[0xEC]=static_cast<unsigned char>(lead);auto actual=identify(attacker.data(),form);
        unsigned t=lead>=90?2:lead>=70?1:0,cl=kFormationClass[form][t],cell=cl*3;
        if(cl!=kNative && form<=7)cell=general_cell(663);
        check(actual.cls==cl && actual.cell==cell,"default real identity preserves prior formation/tier/general table");
    }
    for(unsigned row=0;row<21;++row)for(unsigned t=0;t<3;++t)check(appearance.mapping[row][t]==kFormationClass[row][t],"editable defaults match retained render defaults");
    for(size_t i=0;i<ccz_appearance::general_count;++i)check(ccz_appearance::generals[i].id==kGenerals[i].id && ccz_appearance::generals[i].cell==kGenerals[i].cell,"editable personal list preserves exact existing atlas cells");
    appearance.forms[ccz_appearance::general_index(663)]=6;
    for(unsigned form=0;form<8;++form){auto v=identify(attacker.data(),form);check(v.cell==(form==1 || form==2?728:kFormationClass[form][2]*3u),"Zhang Liao personal only in fish-scale/arrow; other formations generic");}
    short_value(force.data(),0x10,663);check(identify(attacker.data(),4).cell==kRulerClass[2]*3u,"masked-out ruler follows generic ruler art");
    check(identify(attacker.data(),1).cell==728,"checked personal art still wins over ruler");
    short_value(force.data(),0x10,0);short_value(force.data(),0x52,663);
    check(identify(attacker.data(),4).cell==kStrategistClass[2]*3u,"masked-out strategist follows generic strategist art");
    appearance.low=50;appearance.high=100;appearance.mapping[4]={{0,1,2}};
    short_value(force.data(),0x52,0);
    appearance.mapping[1]={{255,255,255}};
    check(identify(attacker.data(),1).cell==728 && identify(attacker.data(),1).cls!=kNative,"checked personal art independent of generic native-model selection");
    appearance.forms[ccz_appearance::general_index(663)]=0;
    check(identify(attacker.data(),1).cls==kNative,"unchecked personal art follows the configured native-model selection");
    appearance.forms[ccz_appearance::general_index(663)]=6;
    for(unsigned lead:{49u,50u,99u,100u}){pa[0xEC]=static_cast<unsigned char>(lead);check(identify(attacker.data(),4).cell==(lead>=100?6u:lead>=50?3u:0u),"custom class and leadership boundaries applied by real identity");}
    std::wstring encoded=ccz_appearance::encode(appearance);ccz_appearance::Settings decoded;
    check(ccz_appearance::decode(encoded.c_str(),decoded) && ccz_appearance::encode(decoded)==encoded,"complete profile roundtrip");
    for(const auto &bad: {std::wstring(),encoded+L",0",encoded+L",",L"3"+encoded.substr(1),encoded.substr(0,encoded.rfind(L',')),L"-"+encoded,encoded+L"junk"}){
        auto before=ccz_appearance::encode(decoded);check(!ccz_appearance::decode(bad.c_str(),decoded) && ccz_appearance::encode(decoded)==before,"bad/newer/truncated profile rejected without partial mutation");
    }
    units[0].army=attacker.data();units[0].cell=728;camera_seen=123;buffs[0].guard=3;
    reset_appearance_captures();
    check(!units[0].army && camera_seen==0 && buffs[0].guard==3,"profile reload drops old/far cached pictures without changing combat charges");
    check(catalog.open(L"../../assets/ccz/ccz_catalog.bin"),"full two-source catalog opens lazily");
    for(unsigned source=0;source<2;++source){
        auto entry=std::find_if(catalog.entries.begin(),catalog.entries.end(),[source](const ccz_catalog::Entry &e){return e.source==source;});check(entry!=catalog.entries.end(),"source has a selectable shape");
        std::vector<unsigned char> raw,atlas;check(catalog.decode(entry->cell,raw) && decode(entry->cell,atlas),"actual world atlas decoder reads either source on demand");
        size_t at=0;for(auto group:{std::array<unsigned,3>{11,48,0},{12,64,kAttackRow},{5,48,kSpecialRow}})for(unsigned pose=0;pose<group[0];++pose)for(unsigned y=0;y<group[1];++y){check(!memcmp(raw.data()+at,atlas.data()+(group[2]+pose*group[1]+y)*64,group[1]),"world atlas pixels match extracted animation");at+=group[1];}
        unsigned reference[kRows][256];build_palette(catalog.palettes[source].data(),catalog.palettes[source].data()+768,reference);
        for(unsigned i=1;i<256;++i){unsigned a=ccz_catalog::representative_colour(catalog.palettes[source].data(),i),v=reference[kRedHare][i];unsigned b=(v&0xFF00FF00u)|((v&255)<<16)|((v>>16)&255);check(a==b || (std::abs(static_cast<int>(a&255)-static_cast<int>(b&255))<=1 && std::abs(static_cast<int>((a>>8)&255)-static_cast<int>((b>>8)&255))<=1 && std::abs(static_cast<int>((a>>16)&255)-static_cast<int>((b>>16)&255))<=1),"preview colours match actual game material correction/red reference");}
    }
    ccz_appearance::Settings legacy;auto old_profile=ccz_appearance::encode(legacy);old_profile[0]=L'1';check(ccz_appearance::decode(old_profile.c_str(),legacy) && legacy.low==70 && legacy.high==90,"existing version1 appearance settings migrate without loss");
    appearance.mapping[4][2]=static_cast<unsigned short>(ccz_appearance::code(catalog.entries.back().cell));pa[0xEC]=100;
    check(identify(attacker.data(),4).cell==catalog.entries.back().cell,"real identity supports explicit original-source cells");
    // User 2026-11-04: retain 512 distinct GPU atlases, then evict only one on the 513th.
    ID3D11Device *gpu=nullptr;ID3D11DeviceContext *gpu_context=nullptr;D3D_FEATURE_LEVEL gpu_level;
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&gpu,&gpu_level,&gpu_context)),"world cache WARP device");
    check(create_resources(gpu),"actual world renderer GPU resources");
    std::array<ID3D11ShaderResourceView *,kTextures> held{};
    for(unsigned i=0;i<held.size();++i){held[i]=cell_view(catalog.entries[i].cell);check(held[i]!=nullptr,"distinct world atlas allocated on demand");held[i]->AddRef();}
    for(unsigned i=0;i<held.size();++i)check(cell_view(catalog.entries[i].cell)==held[i],"all world atlases retained without decoding/replacing");
    check(cell_view(catalog.entries[kTextures].cell)!=nullptr,"next world atlas can still load");
    unsigned retained=0;for(unsigned i=0;i<held.size();++i)for(const auto &t:textures)if(t.cell==catalog.entries[i].cell && t.view==held[i])++retained;
    check(retained==kTextures-1,"one replacement when the cache is full");
    for(auto view:held)view->Release();release_resources();gpu_context->Release();gpu->Release();
    band_tests();
    trait_tests();
    kind_tests();
    portrait_tests();
    ghost_tests();
    VirtualFree(image,0,MEM_RELEASE);
    std::printf("{\"status\":\"ccz_battle_fixture_passed\",\"checks\":%u,\"live_game\":false}\n",checks);return 0;
}
