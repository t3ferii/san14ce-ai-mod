// The actual AI trace module against a fake image/world; no game process access.
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <array>
using Obj=unsigned char *;
static Obj image;
static HMODULE self_module;
static unsigned checks;
static void check(bool v,const char *label){++checks;if(!v){fprintf(stderr,"%s\n",label);exit(1);}}
static Obj pointer(Obj p,unsigned o){return *reinterpret_cast<Obj *>(p+o);}
static Obj world(){return pointer(image,0x277DF18);}
static Obj table(unsigned offset,unsigned id,unsigned maximum){Obj r=world();return r && id<=maximum?pointer(r,offset+id*8):nullptr;}
static int fixture_day;
static unsigned char fixture_city[0x80];
static Obj __fastcall city_lookup(Obj,unsigned id){return id==12?fixture_city:nullptr;}
static int __fastcall day_getter(Obj){return fixture_day;}
static unsigned __fastcall force_getter(Obj army){return army[0x08];}
static void *function(unsigned r){
    if(r==0x32BA50)return reinterpret_cast<void *>(&day_getter);
    if(r==0x22CC40)return reinterpret_cast<void *>(&force_getter);
    if(r==0x32C130)return reinterpret_cast<void *>(&city_lookup);
    check(false,"unexpected native call");return nullptr;
}
template<class T>static T fn(unsigned r){return reinterpret_cast<T>(function(r));}
#define AI_TRACE_FIXTURE 1
#include "../src/native_ai_trace.inl"
// Fake world: armies and troops tables with real offsets.
static std::vector<unsigned char> world_bytes(0x88000),armies(501*0x100),troops(501*0x100);
static Obj army(unsigned id){return armies.data()+id*0x100;}
static Obj group(unsigned id){return troops.data()+id*0x100;}
static void set_army(unsigned id,bool present){*reinterpret_cast<Obj *>(world_bytes.data()+0x7E838+id*8)=present?army(id):nullptr;}
static void put16(Obj p,unsigned o,unsigned v){*reinterpret_cast<unsigned short *>(p+o)=static_cast<unsigned short>(v);}
// Scripted node behaviour; arguments and result are checked.
static unsigned script;static Obj seen_node,seen_context,seen_subject;static unsigned nested_target;
static uint64_t __fastcall mock_exec(Obj node,Obj context,Obj subject){
    seen_node=node;seen_context=context;seen_subject=subject;
    switch(script){
    case 1: set_army(7,true);put16(army(7),0x12,321);army(7)[0x37]=4;army(7)[0x38]=5;put16(army(7),0x3A,12);put16(army(7),0x58,3);army(7)[0x08]=9;return 1;
    case 2: army(7)[0x37]=7;army(7)[0x38]=27;put16(army(7),0x3A,44);return 1;
    case 3: put16(army(7),0x2C,1234);put16(group(3),0x14,10);put16(group(3),0x16,12);return 2;
    case 4: script=2;return ai_trace::replacement[nested_target](node,context,subject); // nested hooked node
    case 5: RaiseException(0xE0001234,0,0,nullptr);return 0;
    default:return 3;
    }
}
// A hook already published by a mod DLL on one slot: the trace must wrap it, not replace it.
static unsigned mod_hook_calls;
static uint64_t __fastcall mod_hook(Obj node,Obj context,Obj subject){++mod_hook_calls;return mock_exec(node,context,subject);}
static uint64_t __fastcall foreign_hook(Obj,Obj,Obj){return 0;}
static bool raises(unsigned i,Obj node,Obj context,Obj subject){
    __try{script=5;ai_trace::replacement[i](node,context,subject);}__except(EXCEPTION_EXECUTE_HANDLER){return true;}
    return false;
}
static unsigned index_of(unsigned execute){for(unsigned i=0;i<ai_trace::kNodes;++i)if(trace_nodes[i].execute==execute)return i;check(false,"node");return 0;}
static std::string read_all(const char *path){std::ifstream f(path,std::ios::binary);std::stringstream s;s<<f.rdbuf();return s.str();}
int main(int argc,char **argv){
    check(argc==2,"output path argument");
    std::vector<unsigned char> bytes(0x2800000);image=bytes.data();
    *reinterpret_cast<Obj *>(image+0x277DF18)=world_bytes.data();
    for(unsigned id=1;id<=500;++id)*reinterpret_cast<Obj *>(world_bytes.data()+0x7F8D8+id*8)=group(id);
    static unsigned char game_context[0x80]{};*reinterpret_cast<Obj *>(world_bytes.data()+0x871E8)=game_context;
    for(const auto &n:trace_nodes)*reinterpret_cast<Obj *>(image+n.slot)=image+n.execute;
    check(ai_trace::kNodes==284,"all RTTI nodes listed");
    *reinterpret_cast<Obj *>(image+trace_nodes[5].slot)=image+1;check(!ai_trace::guards_match(),"changed slot rejects install");
    *reinterpret_cast<Obj *>(image+trace_nodes[5].slot)=image+trace_nodes[5].execute;
    MultiByteToWideChar(CP_UTF8,0,argv[1],-1,ai_trace::output_path,MAX_PATH);
    unsigned target_army=index_of(0x611C0);
    ai_trace::accept_hook=[](const void *p){return p==reinterpret_cast<const void *>(&mod_hook);};
    *reinterpret_cast<void **>(image+trace_nodes[target_army].slot)=reinterpret_cast<void *>(&foreign_hook);
    check(!ai_trace::guards_match(),"another program's hook rejects the trace");
    *reinterpret_cast<void **>(image+trace_nodes[target_army].slot)=reinterpret_cast<void *>(&mod_hook);
    check(ai_trace::guards_match(),"a mod hook on a slot is accepted for chaining");
    ai_trace::fail_after=100;check(!ai_trace::install(true),"partial publication fails");
    for(unsigned i=0;i<ai_trace::kNodes;++i)
        check(pointer(image,trace_nodes[i].slot)==(i==target_army?reinterpret_cast<Obj>(&mod_hook):image+trace_nodes[i].execute),"failed publication restores every slot, mod hook included");
    ai_trace::fail_after=0;
    check(ai_trace::install(true),"all node slots publish");check(!ai_trace::install(true),"double install rejected");
    check(ai_trace::chained[target_army] && ai_trace::originals[target_army]==&mod_hook,"mod hook is chained");
    for(unsigned i=0;i<ai_trace::kNodes;++i){
        check(pointer(image,trace_nodes[i].slot)==reinterpret_cast<Obj>(ai_trace::replacement[i]),"slot owns its wrapper");
        if(i!=target_army)ai_trace::originals[i]=&mock_exec; // fixture only: the fake image has no code
    }
    unsigned departure=index_of(0x2F2B0),relay=index_of(0x6DFF0),list=index_of(0x1FE20),purpose=index_of(0x6EF20),unwatched=index_of(0x83C20);
    unsigned char target_city[0x60]{};put16(target_city,0x4E,12);unsigned char node[0x80]{};*reinterpret_cast<Obj *>(node+0x60)=target_city;
    unsigned char context[8]{},subject[8]{};
    // Game-term tables: person 321 장비(자 익덕), force 9 ruled by 유비, hex 0 in 태산, city 12 = 복양.
    static unsigned char zhang[0x80]{},liu[0x80]{},force9[0x40]{},tile0[0x20]{},tile5[0x20]{},area96[0x40]{},area92[0x40]{},building12[0x20]{};
    wcscpy_s(reinterpret_cast<wchar_t *>(zhang+0x34),20,L"장비익덕");wcscpy_s(reinterpret_cast<wchar_t *>(zhang+0x2E),3,L"익덕");
    wcscpy_s(reinterpret_cast<wchar_t *>(liu+0x34),20,L"유비현덕");wcscpy_s(reinterpret_cast<wchar_t *>(liu+0x2E),3,L"현덕");
    auto slot=[&](unsigned offset,void *p){*reinterpret_cast<void **>(world_bytes.data()+offset)=p;};
    slot(0x158+321*8,zhang);slot(0x158+952*8,liu);put16(force9,0x10,952);slot(0xDF30+9*8,force9);
    put16(tile0,0x12,96);slot(0xE270,tile0);put16(tile5,0x12,92);slot(0xE270+5*8,tile5);
    wcscpy_s(reinterpret_cast<wchar_t *>(area96+0x10),10,L"태산");slot(0x6CAF0+96*8,area96);
    wcscpy_s(reinterpret_cast<wchar_t *>(area92+0x10),10,L"복양");slot(0x6CAF0+92*8,area92);
    put16(building12,0x10,5);building12[0x13]=9;slot(0x6DA98+12*8,building12);
    put16(fixture_city,0x4E,12);*reinterpret_cast<unsigned *>(fixture_city+0x3C)=20000;
    fixture_day=100;
    auto call=[&](unsigned i,unsigned s){script=s;return ai_trace::replacement[i](node,context,subject);};
    std::vector<unsigned char> before_world=world_bytes,before_armies=armies;
    script=0;check(call(list,0)==3,"result passes through unchanged");
    check(seen_node==node && seen_context==context && seen_subject==subject,"arguments pass through unchanged");
    check(world_bytes==before_world && armies==before_armies,"tracer writes no game data");
    check(call(departure,1)==1,"departure result");
    put16(army(7),0x3A,13);                      // changed outside any node (daily processing)
    check(call(target_army,2)==1 && mod_hook_calls==1,"target node result through the chained mod hook");
    check(call(relay,3)==2,"relay node result");
    nested_target=target_army;army(7)[0x37]=4;check(call(purpose,4)==1,"nested call returns inner result");
    bool raised=raises(list,node,context,subject);
    check(raised && ai_trace::depth==0 && ai_trace::active==0,"exception propagates and restores depth");
    check(!trace_nodes[unwatched].watch && trace_nodes[list].watch,"domestic/list utility nodes are count-only");
    put16(army(7),0x46,9);check(call(unwatched,0)==3,"count-only node still passes through");
    *reinterpret_cast<unsigned *>(fixture_city+0x3C)=25000;building12[0x13]=7;
    fixture_day=101;check(call(list,0)==3,"next day");
    set_army(7,false);check(call(list,0)==3,"army removal observed");
    AcquireSRWLockExclusive(&ai_trace::lock);ai_trace::flush();ReleaseSRWLockExclusive(&ai_trace::lock);
    CloseHandle(ai_trace::file);ai_trace::file=INVALID_HANDLE_VALUE;
    std::string csv=read_all(argv[1]);
    auto has=[&](const char *s){return csv.find(s)!=std::string::npos;};
    check(has("seq,day,thread,cause,node"),"csv header");
    check(has(",node,CCommandFootholdAttackDepatureNode,,1,0,army,7,321,9,0,3,spawn,0,0"),"departure spawn attributed to departure node");
    check(has(",call,CCommandFootholdAttackDepatureNode,,1,0,node,0,0,0,0,0,departure_target,0,12"),"departure target recorded");
    check(has(",between,CCommandFootholdAttackDepatureNode,CArmySetTargetArmyNode,,0,army,7,321,9,0,3,target_id,12,13"),"outside-node change attributed between two nodes");
    check(has(",node,CArmySetTargetArmyNode,,1,0,army,7,321,9,0,3,action,4,7"),"action change attributed to target node");
    check(has(",node,CArmySetTargetArmyNode,,1,0,army,7,321,9,0,3,target_id,13,44"),"target change attributed");
    check(has(",node,CSetRelayHexListNode,,2,0,army,7,321,9,0,3,relay1,0,1234"),"relay change attributed");
    check(has(",node,CSetRelayHexListNode,,2,0,troops,3,0,0,0,3,mode,0,10"),"troops purpose mode attributed");
    check(has(",node,CTroopsSetPurposeNode,,1,0,army,7,321,9,0,3,action,4,7"),"nested change attributed to top-level node only");
    check(!has(",node,CArmySetTargetArmyNode,,1,0,army,7,321,9,0,3,action,4,7\n") || has("CTroopsSetPurposeNode"),"nested inner node not double-counted");
    check(has(",count,"),"per-day counts written");
    check(has(",spawn,0,0,장비|유비군|병0|사기0|위치 태산|도시공격 도시 복양"),"sortie line carries officer, force, place and target names");
    check(has("departure_target,0,12,목표 복양"),"departure target named");
    check(has(",target_id,13,44,장비|유비군|병0|사기0|위치 태산|부대공격 부대44"),"changed target described in game terms");
    check(has(",city,start,,,0,city,12,0,9,5,0,garrison,0,20000,복양|소유 유비군|수비병20000"),"first day lists every city");
    check(has(",city,start,,,0,city,12,0,7,5,0,owner,9,7,복양|소유 세력없음|수비병25000"),"owner change recorded next day");
    check(has(",troops,3,0,0,0,3,mode,0,10,성공략 목표12(복양)"),"troops mission named");
    check(has(",meta,start,,,0,mode,0,0,0,0,0,original,0,0"),"trace mode recorded");
    check(has(",meta,CArmySetTargetArmyNode,,,0,node,0,0,0,0,0,chained_mod_hook,0,1"),"chained mod hook recorded");
    check(has(",gone,0,0"),"army removal recorded");
    check(has(",count,CListRemoveAreaNode,,1,0,node,"),"count-only node counted");
    check(has(",between,CTroopsSetPurposeNode,CGetMyselfArmyNode,,0,army,7,321,9,0,3,facility,0,9"),"change during a count-only node is attributed between watched nodes");
    printf("{\"status\":\"ai_trace_fixture_passed\",\"checks\":%u,\"nodes\":%u,\"game_attached\":false}\n",checks,ai_trace::kNodes);
}
