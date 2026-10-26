#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <climits>
#include "../src/damage_preview.h"
using Obj=unsigned char *;
#define COST_SCOPE(x) ((void)0)
static Obj image;
static LONG enabled=1;
static struct {bool damage_preview=true;} mod_features;
static std::map<std::pair<unsigned,unsigned>,Obj> objects;
static unsigned checks;
static void check(bool b,const char *what) {++checks;if(!b){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);}}
static Obj pointer(Obj p,unsigned o){Obj v;std::memcpy(&v,p+o,8);return v;}
static unsigned word(Obj p,unsigned o){unsigned short v;std::memcpy(&v,p+o,2);return v;}
static unsigned dword(Obj p,unsigned o){unsigned v;std::memcpy(&v,p+o,4);return v;}
template<class T> static void put(Obj p,unsigned o,T v){std::memcpy(p+o,&v,sizeof(v));}
static bool valid(Obj p){return p!=nullptr;}
static unsigned army_force(Obj p){return dword(p,0x1dc);}
static Obj world(){return reinterpret_cast<Obj>(1);}
static Obj table(unsigned off,unsigned id,unsigned max){return id<=max?objects[{off,id}]:nullptr;}
static unsigned __fastcall object_kind(Obj p){return dword(p,8);}
static void __fastcall panel_visible(Obj,int v);
static void *dispatch(unsigned);
template<class T> static T fn(unsigned rva){return reinterpret_cast<T>(dispatch(rva));}
struct CodeLink {};
static Obj allocate_near(Obj){return nullptr;}
static void absolute_jump(Obj,void *){}
static bool prepare_link(CodeLink &,unsigned,unsigned char,Obj){return false;}
static bool publish_link(CodeLink &){return false;}
static void log_event(const char *){}
#include "../src/native_damage_preview.inl"
struct Object {
    unsigned char bytes[0x800]={};Obj p(){return bytes;}
    Object(unsigned k,Obj vt){put(bytes,0,vt);put(bytes,8,k);}
};
static Obj a,b,city,tower,hover,form,force1,force2,leader1,leader2,map_data;
static std::map<int,std::wstring> texts;
static std::map<int,int> shown;
static int visible,original_calls,damage_calls,face_calls,distance=1;
static int policy_bonus,force_bonus,resistance;
static bool construction=true;
static void __fastcall panel_visible(Obj,int v){visible=v;}
static void __fastcall native_refresh(Obj,Obj,Obj){++original_calls;}
static Obj __fastcall get_map(){put(map_data,0x110,hover);return map_data;}
static Obj __fastcall city_at(Obj,unsigned id){return objects[{0xdd38,id}];}
static Obj __fastcall get_formation(Obj){return form;}
static float __fastcall common(Obj,Obj,int k,int live){check(k==1 && live==1,"native single-target flags");return 500;}
static float __fastcall power(float,Obj unit,Obj base,Obj,int,int){return unit==a?2000.f:(base?850.f:1000.f);}
static float __fastcall defense(float,Obj,Obj base,Obj,int,int){return base?1100.f:2000.f;}
static void __fastcall damage(Obj from,float,float,Obj to,float,float,int *out){
    ++damage_calls;*out=from==a?123:45;check(from!=to,"no self-target damage");
}
static float __fastcall breach_power(Obj,Obj,int,int){return 31.9f;}
static unsigned __fastcall object_id(Obj p){return word(p,0x18);}
static unsigned __fastcall hostile(unsigned x,unsigned y){return x!=y && x && y;}
static Obj __fastcall get_force(Obj unit){return unit==a?force1:force2;}
static int __fastcall effect(Obj,unsigned key){return key==0x45?force_bonus:0;}
static int __fastcall policy(Obj,unsigned){return policy_bonus;}
static void __fastcall slots(Obj,int,int *out,int,int){std::memset(out,0,127*sizeof(int));}
static int __fastcall hex_distance(unsigned,unsigned){return distance;}
static int __fastcall is_city(Obj p){return p!=tower;}
static int __fastcall effect_type(Obj def,int slot){return def[0x42+slot*3];}
static int __fastcall coefficient(Obj def,int slot,int){return def[0x43+slot*3];}
static int __fastcall maxdur(Obj def){return static_cast<int>(word(def,0x2e));}
static Obj __fastcall leader(Obj unit){return unit==a?leader1:leader2;}
static Obj __fastcall mayor(Obj){return leader2;}
static int __fastcall has_resistance(Obj,unsigned){return resistance!=0;}
static int __fastcall get_resistance(Obj,unsigned){return resistance;}
static const wchar_t *__fastcall unit_name(Obj unit){return unit==a?L"아군부대":L"적군부대";}
static void __fastcall set_text(Obj,int part,const wchar_t *s,int){texts[part]=s;}
static void __fastcall show_part(Obj,int part,int v){shown[part]=v;}
static void __fastcall set_face(Obj,int,Obj){++face_calls;}
static void *dispatch(unsigned r){
    switch(r){
#define N(rva,f) case rva:return reinterpret_cast<void *>(&f)
    N(0x18f10,get_map);N(0x32c130,city_at);N(0x22b090,get_formation);N(0x2a8d40,common);
    N(0x2b21a0,power);N(0x2b2440,power);N(0x2a9e50,defense);N(0x16a1d0,damage);
    N(0x2aa2e0,breach_power);N(0x22dea0,object_id);N(0x2ca280,hostile);N(0x22c850,get_force);
    N(0x22c0f0,effect);N(0x41c200,policy);N(0x26a2c0,slots);N(0x2aa850,hex_distance);
    N(0x232650,is_city);N(0x18fd20,effect_type);N(0x18fda0,coefficient);N(0x18fe30,maxdur);
    N(0x401c0,leader);N(0x22ab60,mayor);N(0x2c5460,has_resistance);N(0x2b2da0,get_resistance);
    N(0x354950,unit_name);N(0x930d20,set_text);N(0x92f810,show_part);N(0x92ed60,set_face);
#undef N
    default:std::fprintf(stderr,"unexpected native call %X\n",r);std::exit(1);
    }
}
int main(int argc,char **argv){
    check(argc==2,"plain-copy argument");
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<unsigned char> pe((std::istreambuf_iterator<char>(file)),{}),mapped(0x2A00000);
    check(pe.size()>0x1000,"plain copy read");
    auto u16=[&](size_t o){unsigned short v;std::memcpy(&v,pe.data()+o,2);return v;};
    auto u32=[&](size_t o){unsigned v;std::memcpy(&v,pe.data()+o,4);return v;};
    const unsigned nt=u32(0x3c),sec=nt+24+u16(nt+20);
    for(unsigned i=0;i<u16(nt+6);++i){const unsigned at=sec+40*i,va=u32(at+12),n=u32(at+16),off=u32(at+20);check(va+n<=mapped.size() && off+n<=pe.size(),"PE section bounds");std::memcpy(mapped.data()+va,pe.data()+off,n);}
    image=mapped.data();check(damage_forecast::guards_match(),"current native profile");
    image[0x7fa230]^=1;check(!damage_forecast::guards_match(),"mismatched entry rejected");image[0x7fa230]^=1;
    Obj vt[32]={};vt[2]=reinterpret_cast<Obj>(&object_kind);vt[25]=reinterpret_cast<Obj>(&panel_visible);
    Object own(27,reinterpret_cast<Obj>(vt)),enemy(27,reinterpret_cast<Obj>(vt)),base(2,reinterpret_cast<Obj>(vt)),
        building(11,reinterpret_cast<Obj>(vt)),castle(11,reinterpret_cast<Obj>(vt)),shape(0,nullptr),towerdef(0,nullptr),
        citydef(0,nullptr),f1(0,nullptr),f2(0,nullptr),p1(0,nullptr),p2(0,nullptr),map(0,nullptr),panel(0,reinterpret_cast<Obj>(vt));
    a=own.p();b=enemy.p();city=base.p();tower=building.p();form=shape.p();force1=f1.p();force2=f2.p();leader1=p1.p();leader2=p2.p();map_data=map.p();
    put(a,0x1dc,1u);put(b,0x1dc,2u);put(a,0x12,static_cast<unsigned short>(1));put(b,0x12,static_cast<unsigned short>(2));
    put(city,0x4e,static_cast<unsigned short>(1));put(castle.p(),0x18,static_cast<unsigned short>(1));castle.bytes[0x13]=2;castle.bytes[0x12]=1;
    put(castle.p(),0x14,static_cast<unsigned short>(3000));tower[0x13]=2;tower[0x12]=5;tower[0x16]=1;put(tower,0x18,static_cast<unsigned short>(62));put(tower,0x14,static_cast<unsigned short>(300));
    towerdef.bytes[0x2c]=2;towerdef.bytes[0x42]=1;towerdef.bytes[0x43]=5;put(towerdef.p(),0x2e,static_cast<unsigned short>(300));
    wcscpy_s(reinterpret_cast<wchar_t *>(city+0x10),16,L"적성");wcscpy_s(reinterpret_cast<wchar_t *>(towerdef.p()+0x10),5,L"망루");
    objects[ {0x158,1}]=leader1;objects[{0x158,2}]=leader2;objects[{0xdd38,1}]=city;objects[{0x6da98,1}]=castle.p();
    objects[{0x73958,1}]=citydef.p();objects[{0x73958,5}]=towerdef.p();objects[{0xdf30,1}]=force1;objects[{0xdf30,2}]=force2;
    put(panel.p()+0x130,0x10,33u);damage_forecast::original_refresh=&native_refresh;
    auto native_snapshot=[&](){std::vector<unsigned char> out;for(Obj o:{a,b,city,tower,castle.p()})out.insert(out.end(),o,o+0x200);return out;};
    const auto before=native_snapshot();
    hover=b;damage_forecast::refresh(panel.p(),a,b);
    check(original_calls==1 && visible==1,"original callback retained");check(texts[31]==L"123명" && texts[32]==L"45명","directional damage, not remaining soldiers");
    for(int i=17;i<=26;++i)check(shown[i]==0,"swords hidden");
    int old=damage_calls,faces=face_calls;damage_forecast::refresh(panel.p(),a,b);
    check(damage_calls==old && face_calls==faces,"unchanged pair throttled, no portrait reload");
    hover=castle.p();damage_forecast::refresh(panel.p(),a,nullptr);
    check(visible==1 && texts[12]==L"적성" && texts[16]==L"내구 31","city hover with null native army target");
    hover=tower;damage_forecast::refresh(panel.p(),a,nullptr);
    check(texts[31]==L"0명" && texts[32]==L"45명" && texts[16]==L"내구 31","facility attack and durability separated");
    tower[0x16]=0;check(damage_forecast::facility(damage_forecast::actor(tower),damage_forecast::actor(a))==0,"unfinished tower cannot shoot");tower[0x16]=1;
    check(native_snapshot()==before,"prediction preserves unit, city and building data");
    hover=a;damage_forecast::refresh(panel.p(),a,a);check(visible==0,"friendly hover hides forecast");
    hover=nullptr;damage_forecast::refresh(panel.p(),a,nullptr);check(visible==0,"empty target hides forecast");
    hover=b;damage_forecast::refresh(panel.p(),a,b);check(visible==1,"returning hover is immediate after empty target");
    faces=face_calls;put(b,0x12,static_cast<unsigned short>(1));damage_forecast::refresh(panel.p(),a,b);
    check(face_calls>faces,"reused enemy slot with a different leader refreshes identity");
    old=original_calls;mod_features.damage_preview=false;damage_forecast::refresh(panel.p(),a,b);check(original_calls==old+1,"OFF uses original callback only");
    using namespace damage_preview;
    check(breach(31,3000,1,0,0,true)==31,"castle base breach");check(breach(31,300,2,30,0,true)==40,"facility destruction policy");
    check(breach(31,20,1,0,0,false)==19 && breach(31,20,1,0,0,true)==20,"last durability restriction");
    check(breach(31,300,3,0,-20,true)==25,"wall resistance");check(facility_attack(5,150,300)==250,"damaged tower attack");
    check(facility_attack(10,400,400)==1000 && facility_attack(10,400,0)==0,"catapult and invalid maximum");
    check(facility_damage(100,30,10,20)==120,"facility policy plus target resistance");
    check(count(-1)==0 && count(NAN)==0 && count(15000)==9999,"finite display bounds");
    std::printf("{\"status\":\"damage_preview_fixture_verified\",\"checks\":%u,\"live_game_verified\":false}\n",checks);
}
