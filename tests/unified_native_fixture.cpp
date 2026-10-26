// Exercises the actual pending-command callback against isolated native objects.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <map>
#include <set>
#include <vector>
#include <cstring>
using Obj=unsigned char *;
static LONG enabled=1;
static unsigned char storage[8192],state_data[0x560],row_data[64],person_data[0x200],force_data[0x60],options_data[32];
static unsigned char tactic_data[201][0x80];
static Obj fixture_root=storage,fixture_person=person_data,fixture_player=force_data;
static unsigned original_updates,original_leaves;
static Obj world(){return fixture_root;}
static unsigned dword(Obj p,unsigned off){unsigned v;memcpy(&v,p+off,4);return v;}
static Obj pointer(Obj p,unsigned off){Obj v;memcpy(&v,p+off,8);return v;}
static void put_pointer(Obj p,unsigned off,Obj value){memcpy(p+off,&value,8);}
static bool valid(Obj p){return p!=nullptr;}
static Obj table(unsigned off,unsigned id,unsigned){return off==0x158 && id==1?fixture_person:off==0x774D0 && id<=200?tactic_data[id]:nullptr;}
static Obj personnel_force(Obj p){return p==fixture_person?fixture_player:nullptr;}
static Obj __fastcall mock_player(Obj){return fixture_player;}
static int __fastcall mock_stat(Obj,unsigned index,unsigned){return index==1?95:70;}
static void __fastcall mock_update(Obj){++original_updates;}
static uint64_t __fastcall mock_leave(Obj,Obj){++original_leaves;return 77;}
template<class T>static T fn(unsigned rva){
    void *value=rva==0x32E910?reinterpret_cast<void *>(&mock_player):rva==0x22FCE0?reinterpret_cast<void *>(&mock_stat):
        rva==0x7DF540?reinterpret_cast<void *>(&mock_update):rva==0x7D9E50?reinterpret_cast<void *>(&mock_leave):nullptr;
    return reinterpret_cast<T>(value);
}
#include "../src/native_tactic_priority.inl"
static unsigned checks;
static void check(bool ok,const char *name){++checks;if(!ok){fprintf(stderr,"%s\n",name);ExitProcess(1);}}
static void put(unsigned offset,unsigned value){memcpy(row_data+4+offset,&value,4);}
static unsigned tactic(unsigned slot){return dword(row_data+4,20+slot*4);}
int main(){
    // world() is offset-independent here; only options is needed by this callback.
    std::vector<unsigned char> large_world(0x87200);fixture_root=large_world.data();
    put_pointer(fixture_root,0x871E8,options_data);
    Obj rows[]={row_data};put_pointer(state_data,0x540,reinterpret_cast<Obj>(rows));
    put_pointer(state_data,0x548,reinterpret_cast<Obj>(rows+1));
    put(0,1);put(12,1);
    fixture_person[0x130]=1;fixture_person[0x131]=137;fixture_person[0x132]=17;fixture_person[0x133]=6;
    for(unsigned id:{1u,137u,17u,6u}){unsigned bits=id==137?4u:6u;memcpy(tactic_data[id]+0x6C,&bits,4);}
    const int martial[]={137,17,6,1},intellect[]={1,6,17};
    check(tactic_priority::set(1,martial,4,intellect,3,1),"accept valid settings");
    tactic_priority::update(state_data);
    check(tactic(0)==17 && tactic(1)==6 && tactic(2)==1,"skip incompatible formation while following priority");
    put(20,1);tactic_priority::update(state_data);
    check(tactic(0)==1,"subsequent frames preserve the fixture_player's manual selection");
    put(12,2);tactic_priority::update(state_data);
    check(tactic(0)==137,"formation change recalculates available tactics");
    check(original_updates==3,"original update called exactly once per callback");
    check(!tactic_priority::set(1,martial,4,intellect,3,2),"reject malformed enable value");
    const int invalid[]={201};check(!tactic_priority::set(1,invalid,1,intellect,3,1),"reject out of range tactic");
    check(tactic_priority::leave(state_data,nullptr)==77 && original_leaves==1 && tactic_priority::seen.empty(),"leaving the window clears row cache and forwards original result");
    check(tactic_priority::set(1,martial,4,intellect,3,0),"disable priority");put(20,6);
    tactic_priority::update(state_data);check(tactic(0)==6,"disabled feature preserves native pending command");
    std::vector<int> decoded;
    check(unified_config::decode(L"-1,137,17,137,-2",decoded) && decoded.size()==4,"decode persisted priorities and remove duplicates");
    check(!unified_config::decode(L"137,201",decoded) && !unified_config::decode(L"",decoded),"reject malformed persisted priorities");
    // The game's batch-post reader traverses the MSVC release std::map ABI.
    struct PostMap{Obj unused=nullptr;std::map<Obj,unsigned> entries;} posts;
    posts.entries.emplace(fixture_person,42);Obj map=reinterpret_cast<Obj>(&posts),head=pointer(map,8),node=pointer(head,8);
    check(sizeof(posts)==24 && dword(map,16)==1 && pointer(node,32)==fixture_person && dword(node,40)==42,
          "MSVC map header, count and node pair match the native post command ABI");
    printf("{\"status\":\"unified_native_fixture_passed\",\"checks\":%u,\"gameplay_verified\":false}\n",checks);
}
