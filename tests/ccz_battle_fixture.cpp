// User 2026-11-02: execute the actual CCZ receiver/facing/effect code.
#define CCZ_NATIVE_FIXTURE
#include "../src/ccz_module.cpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include "../src/ccz_preview_palette.h"
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
static unsigned char __fastcall mock_force(Obj p){return static_cast<unsigned char>(p==attacker.data()?af:vf);}
static bool __fastcall mock_enemy(unsigned a,unsigned b){return a!=b && enemies;}
static int __fastcall mock_war(Obj,unsigned stat,unsigned mask){check(stat==1 && mask==0xFFF,"effective war ABI");++stat_calls;return 80;}
static unsigned ccz_fixture_roll(){++rolls;return die_roll;}
static void *ccz_fixture_function(unsigned rva){
    switch(rva){
    case 0x16FDD0:return reinterpret_cast<void *>(&mock_fix);
    case 0x32F500:return reinterpret_cast<void *>(&mock_valid);
    case 0x22CC40:return reinterpret_cast<void *>(&mock_force);
    case 0x2CA280:return reinterpret_cast<void *>(&mock_enemy);
    case 0x22FCE0:return reinterpret_cast<void *>(&mock_war);
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
int main(){
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x2800000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    check(image!=nullptr,"fixture image allocated");point(image,0x277DF18,root.data());
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
    for(unsigned i=0;i<4;++i){short_value(output.data(),0x20,100);short_value(output.data(),0x22,20);ccz_combat::fix(nullptr,pair.data(),output.data());check(!word(output.data(),0x20) && !word(output.data(),0x22),"remaining block charge is used only by an actual hostile hit");}
    ccz_sprites::buff_read(victim.data(),day,guard,fury);check(!guard,"five actual blocks exhaust the charge state");
    short_value(output.data(),0x20,100);short_value(output.data(),0x22,20);ccz_combat::fix(nullptr,pair.data(),output.data());
    check(word(output.data(),0x20)==100 && word(output.data(),0x22)==20,"sixth hit with a failed roll is not blocked");
    reset();ccz_combat::configure(false,true);ccz_combat::fix(nullptr,pair.data(),output.data());
    check(word(output.data(),0x20)==150 && word(output.data(),0x22)==30 && ccz_sprites::units[0].crit_at,"hostile critical scales final components and marks actual attacker");
    reset();ccz_combat::configure(false,true);die_roll=999;
    ccz_sprites::buff_use(attacker.data(),day,false,true);
    ccz_combat::fix(nullptr,pair.data(),output.data());
    check(word(output.data(),0x20)==150 && ccz_sprites::buffs[0].fury==3,"existing critical charge works on next hostile hit without a new roll");
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
    VirtualFree(image,0,MEM_RELEASE);
    std::printf("{\"status\":\"ccz_battle_fixture_passed\",\"checks\":%u,\"live_game\":false}\n",checks);return 0;
}
