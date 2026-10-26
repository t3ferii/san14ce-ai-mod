// User/Claude decision 2026-10-25: observation only, bounded per-force repetition.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <fstream>
#include <iterator>
#include <thread>
using Obj=unsigned char *;
static Obj image;
struct CodeLink {Obj site=nullptr;unsigned char original[16]={},modified[16]={};};
static bool publish_ok=true;
static bool prepare_link(CodeLink &link,unsigned at,unsigned char opcode,Obj target) {
    link.site=image+at;memcpy(link.original,link.site,16);memcpy(link.modified,link.site,16);
    link.modified[0]=opcode;const int relative=static_cast<int>(target-(link.site+5));memcpy(link.modified+1,&relative,4);return true;
}
static bool publish_link(CodeLink &link) {if(publish_ok)memcpy(link.site,link.modified,16);return publish_ok;}
static void restore_link(CodeLink &link) {memcpy(link.site,link.original,16);}
static void absolute_jump(Obj at,void *target) {const unsigned char op[]={0xFF,0x25,0,0,0,0};memcpy(at,op,6);memcpy(at+6,&target,8);}
static Obj allocate_near(Obj) {return static_cast<Obj>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));}
static bool group_on=true;
static int switch_on=1;
static bool dev_group() {return group_on;}
static int diagnostic_logging=1;
static int mod_setting(const wchar_t *,const wchar_t *,int) {return switch_on;}
struct Event {const char *name;unsigned a,b,c;};
static std::vector<Event> events;
static void log_event(const char *name,unsigned a=0,unsigned b=0,unsigned c=0) {events.push_back({name,a,b,c});}
static int today=10;
static int game_day() {return today;}
#define SPLIT_BASE
#include "../src/native_path_count.inl"
static unsigned checks;
static void check(bool yes,const char *why) {++checks;if(!yes) {fprintf(stderr,"FAIL %s\n",why);exit(1);}}
static bool force_on=true;
static bool scope() {return force_on;}
struct Node {std::uintptr_t left,right,parent;std::uint32_t cell,state,g,f;};
static_assert(sizeof(Node)==40,"native node ABI");
static Obj expected_ctx,expected_buffer,return_value;
static unsigned expected_start,expected_flags,native_calls;
static bool throw_native;
static const int *expected_goals;
static int expected_count,expected_clear;
static Obj __fastcall native_mock(Obj ctx,unsigned start,const int *goals,int count,Obj buffer,int clear,unsigned flags) {
    check(ctx==expected_ctx && start==expected_start && goals==expected_goals && count==expected_count &&
          buffer==expected_buffer && clear==expected_clear && flags==expected_flags,"seven original arguments unchanged through gate");
    check(GetLastError()==567,"incoming last error preserved");++native_calls;
    if(throw_native)RaiseException(0xE0010001,0,0,nullptr);
    SetLastError(1234);return return_value;
}
static bool exception_propagates() {
    __try {SetLastError(567);path_count::search(expected_ctx,expected_start,expected_goals,expected_count,expected_buffer,expected_clear,expected_flags);}
    __except(GetExceptionCode()==0xE0010001?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH) {return true;}
    return false;
}
int main(int argc,char **argv) {
    check(argc==2,"running code capture supplied");
    using namespace path_count_core;
    unsigned char ctx[0x30]={};int goals[]={10,20};
    const Hash key=input(ctx,5,goals,2,3);
    check(key==input(ctx,5,goals,2,3),"same inputs stable");
    check(key!=input(ctx,6,goals,2,3) && key!=input(ctx,5,goals,1,3) && key!=input(ctx,5,goals,2,4),"start count flags distinguished");
    ctx[47]=1;check(key!=input(ctx,5,goals,2,3),"all 0x30 context bytes included");ctx[47]=0;
    goals[1]=21;check(key!=input(ctx,5,goals,2,3),"all valid goals included");goals[1]=20;
    for(int count:{-1,65,2147483647}) {
        check(input(ctx,5,reinterpret_cast<const int *>(1),count,3)==input(ctx,5,nullptr,count,3),"invalid goal count never read");
    }
    check(input(ctx,5,nullptr,0,3)==input(ctx,5,goals,0,3),"zero goal count permits null");
    int max_goals[64]={};check(input(ctx,5,max_goals,64,3)!=input(ctx,5,nullptr,65,3),"64-goal boundary");
    auto f=new Force;
    f->add(key,100,7);f->add(key,100,11);f->add(key,101,13);f->add(key,100,17);
    auto t=f->flush();check(t.calls==4 && t.repeats==3 && t.same==2 && t.different==1 && t.repeat_us==41 && t.total_us==48,"repeat/result counts versus first result");
    check(f->used==0 && f->totals.calls==0,"force flush resets counters/table");
    f->add(key,101,3);t=f->flush();check(t.repeats==0,"same key in new force not repeat");
    f->add(0,0,1);f->add(0,0,2);t=f->flush();check(t.repeats==1 && t.same==1,"zero hashes and failures supported");
    for(unsigned i=0;i<capacity+5;++i)f->add(i,i,1);
    f->add(0,0,2);f->add(capacity,capacity,4);t=f->flush();
    check(t.calls==4103 && t.overflow==6 && t.repeats==1 && t.same==1,"4096 limit retains tracked keys and counts overflow");
    f->add(key,0,5,false);t=f->flush();check(t.calls==1 && t.overflow==1 && !t.repeats,"unreadable observation never creates false repeat");delete f;
    Node a={},b={};a.cell=20;b.cell=10;b.parent=reinterpret_cast<std::uintptr_t>(&a);
    auto read=[](std::uintptr_t p,std::uintptr_t &parent,std::uint32_t &cell) {auto n=reinterpret_cast<Node *>(p);parent=n->parent;cell=n->cell;return true;};
    auto r=result(reinterpret_cast<std::uintptr_t>(&b),read);
    const std::uint32_t cells[]={10,20};check(r.hash==bytes(basis,cells,sizeof(cells)) && r.nodes==2 && !r.cycle,"path hash follows parent/cell order");
    check(result(0,read).hash==0,"failed native search hash zero");
    a.parent=reinterpret_cast<std::uintptr_t>(&b);r=result(reinterpret_cast<std::uintptr_t>(&b),read);check(r.cycle && r.nodes==2,"two-node cycle terminates");
    a.parent=reinterpret_cast<std::uintptr_t>(&a);r=result(reinterpret_cast<std::uintptr_t>(&a),read);check(r.cycle && r.nodes==1,"self cycle terminates");
    auto nodes=new Node[2001]{};for(unsigned i=0;i<2000;++i)nodes[i].parent=reinterpret_cast<std::uintptr_t>(&nodes[i+1]);
    r=result(reinterpret_cast<std::uintptr_t>(nodes),read);check(r.truncated && r.nodes==2000,"2000 node limit");delete[] nodes;
    r=result(1,[](std::uintptr_t,std::uintptr_t &,std::uint32_t &) {return false;});check(r.unreadable,"unreadable result indicated");
    Day d;unsigned emits=0;Totals previous;int previous_day=-1;
    auto emit=[&](int day_id,const Totals &value) {++emits;previous=value;previous_day=day_id;};
    d.advance(10,emit);d.add(1000,false);d.advance(10,emit);d.add(2500,true);
    check(emits==0,"same day no flush");d.advance(11,emit);
    check(emits==1 && previous_day==10 && previous.calls==2 && previous.repeats==1 && previous.total_us==3500 && d.totals.calls==0,"day boundary flush and reset");
    d.add(1,false);d.advance(2,emit);check(emits==2 && previous_day==11 && previous.calls==1,"save date rewind starts new day");
    std::ifstream stream(argv[1],std::ios::binary);std::vector<unsigned char> capture((std::istreambuf_iterator<char>(stream)),{});
    check(capture.size()>path_count::entry+path_count::size-0x1000,"capture available");
    const auto code=capture.data()+path_count::entry-0x1000;check(path_count::matches(code),"full current captured function guard");
    auto changed=std::vector<unsigned char>(code,code+path_count::size);
    for(unsigned offset:{0u,6u,100u,path_count::size-1}) {changed[offset]^=1;check(!path_count::matches(changed.data()),"foreign entry/body/tail rejected");changed[offset]^=1;}
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x200000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));check(image!=nullptr,"synthetic image allocated");
    memcpy(image+path_count::entry,code,path_count::size);
    check(!path_count::install(false,scope) && !path_count::active && !strcmp(events.back().name,"path_count_root_unavailable"),"missing root wrapper fails closed");
    image[path_count::entry]^=1;check(!path_count::install(true,scope) && !strcmp(events.back().name,"path_count_guard_rejected"),"install rejects foreign code");image[path_count::entry]^=1;
    switch_on=0;check(path_count::install(true,scope) && !path_count::active,"default-off skips hook");switch_on=1;
    group_on=false;check(path_count::install(true,scope) && !path_count::active,"group-off skips hook");group_on=true;
    diagnostic_logging=0;check(path_count::install(true,scope) && !path_count::active,"logging-off skips hook");diagnostic_logging=1;
    Obj relay=image+0x1F0000;check(path_count::prepare(relay),"native prepare against captured prologue");
    check(!memcmp(image+path_count::entry,code,path_count::size),"prepare does not mutate entry");
    check(RtlAddFunctionTable(reinterpret_cast<RUNTIME_FUNCTION *>(relay+64),1,reinterpret_cast<DWORD64>(relay))!=0,"copied MOV leaf unwind registered");
    DWORD64 base=0;auto runtime=RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(relay+36),&base,nullptr);
    check(runtime && base==reinterpret_cast<DWORD64>(relay) && runtime->EndAddress==52,"unwind gate covers MOV and R11 branch");
    check(publish_link(path_count::link) && image[path_count::entry]==0xE9 && image[path_count::entry+5]==0x90 && image[path_count::entry+6]==0x90,"actual seven-byte entry jump published");
    absolute_jump(image+path_count::entry+7,reinterpret_cast<void *>(&native_mock));
    LARGE_INTEGER q;QueryPerformanceFrequency(&q);path_count::frequency=q.QuadPart;path_count::force_scope=scope;path_count::active=true;events.clear();
    expected_ctx=ctx;expected_start=5;expected_goals=goals;expected_count=2;expected_buffer=ctx;expected_clear=77;expected_flags=3;return_value=nullptr;
    auto invoke=[&]() {SetLastError(567);Obj out=reinterpret_cast<path_count::Search>(image+path_count::entry)(ctx,5,goals,2,ctx,77,3);check(out==return_value && GetLastError()==1234,"native result and last error unchanged");};
    invoke();invoke();return_value=reinterpret_cast<Obj>(&b);b.parent=0;invoke();
    check(path_count::force.totals.calls==3 && path_count::force.totals.same==1 && path_count::force.totals.different==1,"real wrapper aggregates same/different results");
    PATH_COUNT_FLUSH();check(events.size()==2 && !strcmp(events[0].name,"path_force") && events[0].a==3 && events[0].b==2 && !strcmp(events[1].name,"path_force_result") && events[1].a==1 && events[1].b==1,"force emits required two lines and resets");
    invoke();PATH_COUNT_FLUSH();check(events[2].a==1 && events[2].b==0,"next force table cleared");
    force_on=false;invoke();check(path_count::force.totals.calls==0 && path_count::day.totals.calls==5,"UI call contributes only day totals");
    today=11;invoke();check(!strcmp(events.back().name,"path_day") && events.back().a==5 && events.back().b==2,"runtime boundary emits previous day including UI calls");
    force_on=true;
    std::thread other([&]() {check(path_count::force.totals.calls==0,"new AI thread starts with empty TLS");path_count::force.add(1,1,1);PATH_COUNT_FLUSH();});other.join();
    check(path_count::force.totals.calls==0 && native_calls==6,"other force TLS isolated, native called exactly once per wrapper");
    const auto calls_before=path_count::day.totals.calls;throw_native=true;
    check(exception_propagates() && path_count::day.totals.calls==calls_before && path_count::force.totals.calls==0,"native SEH propagates through registered gate without false completed count");
    restore_link(path_count::link);check(!memcmp(image+path_count::entry,code,7),"entry rollback restores seven original bytes");
    RtlDeleteFunctionTable(reinterpret_cast<RUNTIME_FUNCTION *>(relay+64));VirtualFree(image,0,MEM_RELEASE);
    printf("{\"status\":\"path_count_fixture_passed\",\"checks\":%u,\"gameplay_verified\":false}\n",checks);
}
