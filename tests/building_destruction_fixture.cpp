#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
#include <fstream>
#include <climits>
using Obj=unsigned char *;
static Obj image;
static LONG enabled=1;
static struct {bool convenience=true,building_destruction_all=true;} mod_features;
static unsigned checks;
static void check(bool b,const char *s){++checks;if(!b){std::fprintf(stderr,"FAIL %s\n",s);std::exit(1);}}
template<class T> static void put(Obj p,unsigned off,T value){std::memcpy(p+off,&value,sizeof(value));}
static Obj pointer(Obj p,unsigned off){Obj v;std::memcpy(&v,p+off,8);return v;}
static unsigned word(Obj p,unsigned off){unsigned short v;std::memcpy(&v,p+off,2);return v;}
static bool valid(Obj p){return p && p[8]!=0;}
static std::map<std::pair<unsigned,unsigned>,Obj> objects;
static Obj table(unsigned off,unsigned id,unsigned max){return id<=max?objects[{off,id}]:nullptr;}
static void *dispatch(unsigned);
template<class T> static T fn(unsigned rva){return reinterpret_cast<T>(dispatch(rva));}
static void log_event(const char *,unsigned=0,unsigned=0,unsigned=0){}
struct CodeLink {unsigned rva,block;unsigned char original[16],modified[16];bool published;};
static void absolute_jump(Obj p,void *t){p[0]=0xff;p[1]=0x25;std::memset(p+2,0,4);put(p,6,t);}
static Obj allocate_near(Obj){return static_cast<Obj>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));}
static bool prepare_link(CodeLink &l,unsigned r,unsigned char op,Obj to){
    l={};l.rva=r;l.block=r&~15u;const unsigned o=r-l.block;if(o>11)return false;
    const intptr_t delta=to-(image+r+5);if(delta<INT32_MIN || delta>INT32_MAX)return false;
    std::memcpy(l.original,image+l.block,16);std::memcpy(l.modified,l.original,16);
    l.modified[o]=op;put(l.modified,o+1,static_cast<int>(delta));return true;
}
static unsigned publications,fail_publication;
static bool publish_link(CodeLink &l){
    ++publications;if(publications==fail_publication)return false;
    if(std::memcmp(image+l.block,l.original,16))return false;
    std::memcpy(image+l.block,l.modified,16);l.published=true;return true;
}
static void restore_link(CodeLink &l){if(l.published)std::memcpy(image+l.block,l.original,16);}
#include "../src/native_building_destruction.inl"
struct Object {unsigned char b[0x100]={};Obj p(){return b;} Object(){b[8]=1;}};
static Object command,other_command,region,other_region,force;
static Object buildings[14],tiles[14],definitions[5];
static unsigned mode,native_calls,extra_writes;
static bool native_done;
static int __fastcall enemy(unsigned a,unsigned b){return a==1 && (b==2 || b==3);}
static Obj __fastcall area(Obj tile){return pointer(tile,0x20);}
static void __fastcall durability(Obj b,int left){check(native_done,"extra writer runs after native completion");++extra_writes;put(b,0x14,static_cast<unsigned short>(left));}
static void __fastcall native_remove(Obj b,Obj effect,int flag){check(native_done && !effect && flag==0,"native removal ABI and caller phase");++extra_writes;b[8]=0;}
static int __fastcall original(Obj cmd){
    ++native_calls;
    if(cmd==other_command.p()) {building_destruction::capture(cmd,buildings[1].p(),region.p(),999,force.p());return 77;}
    if(mode==0)return 0;
    building_destruction::capture(cmd,buildings[1].p(),region.p(),500,force.p());
    check(word(buildings[2].p(),0x14)==900,"capture performs no gameplay writes");
    if(mode==3)building_destruction::capture(cmd,buildings[1].p(),region.p(),999,force.p());
    if(mode==4)buildings[2].b[0x13]=4;
    if(mode==5)put(tiles[2].p(),0x20,other_region.p());
    if(mode==6)buildings[2].b[0x12]=3;
    if(mode==7)check(building_destruction::complete(other_command.p())==77,"nested command keeps native return");
    if(mode==8)mod_features.building_destruction_all=false;
    if(mode==9)objects[{0x6DA98,2}]=buildings[12].p();
    buildings[1].b[8]=0; // native primary was only 100 HP; raw roll is still 500
    if(mode==2)buildings[4].b[8]=0; // native bonus trap removed first
    native_done=true;return mode==2?2:1;
}
static void *dispatch(unsigned r){
    switch(r){case 0x1F9DE0:return reinterpret_cast<void *>(&original);
    case 0x2CA280:return reinterpret_cast<void *>(&enemy);case 0x229EA0:return reinterpret_cast<void *>(&area);
    case 0x1F42B0:return reinterpret_cast<void *>(&durability);case 0x294B90:return reinterpret_cast<void *>(&native_remove);
    default:check(false,"unexpected native call");return nullptr;}
}
static void reset(){
    objects.clear();enabled=1;mod_features={true,true};mode=1;native_calls=extra_writes=0;native_done=false;
    building_destruction::current=nullptr;
    for(unsigned c=0;c<5;++c){definitions[c]=Object{};definitions[c].b[0x2C]=static_cast<unsigned char>(c);objects[{0x73958,c}]=definitions[c].p();}
    force=Object{};put(force.p(),0x10,static_cast<unsigned short>(850));objects[{0xDF30,1}]=force.p();
    for(unsigned i=1;i<14;++i){
        buildings[i]=Object{};tiles[i]=Object{};Obj b=buildings[i].p();
        b[0x12]=2;b[0x13]=2;put(b,0x10,static_cast<unsigned short>(100+i));put(b,0x14,static_cast<unsigned short>(900));
        put(tiles[i].p(),0x20,region.p());objects[{0x6DA98,i}]=b;objects[{0xE270,100+i}]=tiles[i].p();
    }
    put(buildings[1].p(),0x14,static_cast<unsigned short>(100));
    buildings[3].b[0x12]=3;put(buildings[3].p(),0x14,static_cast<unsigned short>(200)); // hostile wall
    buildings[4].b[0x12]=4; // hostile trap
    buildings[5].b[0x13]=1;buildings[6].b[0x13]=4;buildings[7].b[0x13]=0; // own, ally, neutral
    put(tiles[8].p(),0x20,other_region.p());buildings[9].b[0x12]=1; // another area, city
    buildings[10].b[8]=0;put(buildings[11].p(),0x14,static_cast<unsigned short>(0));
    buildings[12].b[0x13]=4;buildings[13].b[0x13]=3; // second hostile force
}
static void behavior(){
    reset();check(building_destruction::complete(command.p())==1,"preserves native success return");
    check(native_calls==1 && !building_destruction::current,"one native roll and scoped context cleanup");
    check(word(buildings[2].p(),0x14)==400,"full raw 500 damage despite primary being only 100 HP");
    check(!valid(buildings[3].p()),"lethal wall damage uses native removal");
    check(word(buildings[4].p(),0x14)==400 && word(buildings[13].p(),0x14)==400,"trap and another enemy force each receive full damage");
    for(unsigned id:{5u,6u,7u,8u,9u,12u})check(word(buildings[id].p(),0x14)==900,"own/allied/neutral/other region/city untouched");
    check(extra_writes==4,"four extras; no duplicate primary damage");
    reset();mode=2;check(building_destruction::complete(command.p())==2,"bonus result preserved");check(extra_writes==3,"already removed bonus trap not touched twice");
    reset();mode=0;check(building_destruction::complete(command.p())==0 && extra_writes==0,"failure preserves native behavior only");
    for(unsigned option=0;option<3;++option){reset();mode=0;if(option==0)enabled=0;else if(option==1)mod_features.convenience=false;else mod_features.building_destruction_all=false;
        check(building_destruction::complete(command.p())==0 && native_calls==1 && extra_writes==0,"each OFF gate stays native");}
    reset();mode=3;building_destruction::complete(command.p());check(word(buildings[2].p(),0x14)==400,"duplicate capture ignored");
    for(unsigned change:{4u,5u,6u,9u}){reset();mode=change;building_destruction::complete(command.p());check(word(buildings[2].p(),0x14)==900,"changed owner/region/type/slot skipped after native callback");}
    reset();mode=7;building_destruction::complete(command.p());check(native_calls==2 && word(buildings[2].p(),0x14)==400,"reentrant native command cannot replace parent capture");
    reset();mode=8;building_destruction::complete(command.p());check(extra_writes==0,"switch disabled during native callback prevents expansion");
    for(unsigned ruler:{0u,7u,51u,850u,6000u}) {
        reset();put(force.p(),0x10,static_cast<unsigned short>(ruler));
        check(building_destruction::complete(command.p())==1 && word(buildings[2].p(),0x14)==400,"ruler PERSON id never controls the caster force identity");
    }
    for(unsigned id=1;id<=51;++id) {
        reset();objects.erase({0xDF30,1});objects[{0xDF30,id}]=force.p();
        building_destruction::Context c={};c.command=command.p();building_destruction::current=&c;
        building_destruction::capture(command.p(),buildings[1].p(),region.p(),500,force.p());
        check(c.captured && c.force==id,"all native force table slots resolve independently of ruler id");
        building_destruction::current=nullptr;
    }
    reset();objects.erase({0xDF30,1});building_destruction::Context orphan={};orphan.command=command.p();building_destruction::current=&orphan;
    building_destruction::capture(command.p(),buildings[1].p(),region.p(),500,force.p());check(!orphan.captured,"force object absent from native table rejected");building_destruction::current=nullptr;
    reset();building_destruction::capture(command.p(),buildings[1].p(),region.p(),500,force.p());check(extra_writes==0 && !building_destruction::current,"unscoped native capture is harmless");
    for(int damage:{0,-1}){reset();building_destruction::Context c={};c.command=command.p();building_destruction::current=&c;
        building_destruction::capture(command.p(),buildings[1].p(),region.p(),damage,force.p());check(!c.captured,"nonpositive roll rejected");building_destruction::current=nullptr;}
}
static void bridge(){
    reset();Obj code=allocate_near(nullptr);check(code!=nullptr,"bridge allocation");
    unsigned n=building_destruction::make_capture_bridge(code);
    Obj entry=code+512;
    const unsigned char driver[]={0x55,0x41,0x54,0x41,0x55,0x48,0x81,0xEC,0,1,0,0,
        0x48,0x8D,0xAC,0x24,0x80,0,0,0,0x48,0x89,0x4D,0xE0,0x48,0x89,0x55,0xA0,
        0x48,0x8B,0x84,0x24,0x40,1,0,0,0x48,0x89,0x45,0xC0,0x4D,0x8B,0xE8,0x44,0x89,0xCA,
        0xC7,0x44,0x24,0x78,0x78,0x56,0x34,0x12,0xF9,0x48,0xB8};
    std::memcpy(entry,driver,sizeof(driver)-3);unsigned at=sizeof(driver)-3;
    static const unsigned char pattern[16]={1,9,4,8,5,3,2,7,6,11,12,15,13,10,14,16};
    entry[at++]=0x48;entry[at++]=0xB8;put(entry,at,pattern);at+=8;
    for(unsigned xmm=0;xmm<6;++xmm){const unsigned char load[]={0xF3,0x0F,0x6F,static_cast<unsigned char>(xmm*8)};std::memcpy(entry+at,load,4);at+=4;}
    entry[at++]=0xF9;entry[at++]=0x48;entry[at++]=0xB8;put(entry,at,code);at+=8;
    entry[at++]=0xFF;entry[at++]=0xD0;
    // Sixth argument is an output buffer at native RSP+148 after our frame.
    const unsigned char output[]={0x4C,0x8B,0x9C,0x24,0x48,1,0,0};std::memcpy(entry+at,output,8);at+=8;
    for(unsigned xmm=0;xmm<6;++xmm){const unsigned char save[]={0xF3,0x41,0x0F,0x7F,static_cast<unsigned char>(0x43+xmm*8),static_cast<unsigned char>(xmm*16)};std::memcpy(entry+at,save,6);at+=6;}

    const unsigned char tail[]={0x0F,0x92,0xC1,0x0F,0xB6,0xC9,0x48,0xC1,0xE1,0x20,
        0x44,0x89,0xE0,0x48,0x09,0xC8,0x48,0x81,0xC4,0,1,0,0,0x41,0x5D,0x41,0x5C,0x5D,0xC3};
    std::memcpy(entry+at,tail,sizeof(tail));DWORD old;check(VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old)!=0,"executable bridge");FlushInstructionCache(GetCurrentProcess(),code,4096);
    building_destruction::Context c={};c.command=command.p();building_destruction::current=&c;
    unsigned char xmm_result[96]={};
    using Driver=unsigned long long(__fastcall *)(Obj,Obj,Obj,int,Obj,Obj);
    const auto result=reinterpret_cast<Driver>(entry)(command.p(),buildings[1].p(),region.p(),500,force.p(),xmm_result);
    building_destruction::current=nullptr;
    check(result==0x112345678ull,"real bridge preserves carry and loads original stack into R12D");
    check(c.captured && c.damage==500 && c.force==1 && c.region==region.p() && c.count==4,"real bridge forwards all five arguments");
    for(unsigned xmm=0;xmm<6;++xmm)check(!std::memcmp(xmm_result+xmm*16,pattern,16),"real bridge preserves volatile XMM registers");
    check(n<512 && extra_writes==0,"bridge fits relay and capture does not apply writes");
    std::ofstream("capture_bridge.bin",std::ios::binary).write(reinterpret_cast<char *>(code),n);VirtualFree(code,0,MEM_RELEASE);
}
static void guards(const char *path){
    std::ifstream f(path,std::ios::binary);std::vector<unsigned char> data((std::istreambuf_iterator<char>(f)),{});check(data.size()>0x1000,"plain PE fixture loaded");
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x4000000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));check(image!=nullptr,"mapped PE fixture allocated");
    const unsigned pe=*reinterpret_cast<unsigned *>(data.data()+0x3C);Obj file=data.data();
    unsigned section=pe+24+word(file,pe+20);const unsigned count=word(file,pe+6);
    for(unsigned i=0;i<count;++i,section+=40){unsigned rva=*reinterpret_cast<unsigned *>(file+section+12),size=*reinterpret_cast<unsigned *>(file+section+16),raw=*reinterpret_cast<unsigned *>(file+section+20);
        if(raw+size<=data.size() && rva+size<=0x4000000)std::memcpy(image+rva,file+raw,size);}
    check(building_destruction::guards_match(),"all native guard hashes match checked plain CE");
    for(const auto &g:building_destruction::guards){image[g.rva]^=1;check(!building_destruction::guards_match(),"each changed dependency rejects hook");image[g.rva]^=1;}
    unsigned char before[16];std::memcpy(before,image+0x1FA600,16);publications=0;fail_publication=2;
    check(!building_destruction::install(),"second publication failure rejects feature");
    check(!std::memcmp(before,image+0x1FA600,16) && !building_destruction::installed,"first publication rolled back");
    publications=fail_publication=0;check(building_destruction::install(),"two scoped CALL hooks published");
    check(image[0x1F91F1]==0xE8 && image[0x1FA607]==0xE8 && image[0x1FA60C]==0x90 && image[0x1FA60E]==0x90,"correct hook footprints and NOP tail");
    check(building_destruction::install() && publications==2,"installation idempotent");VirtualFree(image,0,MEM_RELEASE);
}
int main(int argc,char **argv){check(argc==2,"plain executable path supplied");behavior();bridge();guards(argv[1]);
    std::printf("{\"status\":\"building_destruction_fixture_verified\",\"checks\":%u,\"live_game_verified\":false}\n",checks);}
