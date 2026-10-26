// User decision 2026-10-25: fixed map region IDs, same facility result, no daily rebuild.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <thread>
#include <atomic>
#include "../src/area_index.h"
using Obj=unsigned char *;
static unsigned checks;
static void expect(bool v,const char *why) {++checks;if(!v) {fprintf(stderr,"%s\n",why);exit(1);}}
static Obj image;
static Obj pointer(Obj p,unsigned o) {return *reinterpret_cast<Obj *>(p+o);}
static unsigned short word(Obj p,unsigned o) {return *reinterpret_cast<unsigned short *>(p+o);}
static void put(Obj p,unsigned o,Obj v) {memcpy(p+o,&v,sizeof(v));}
static void putword(Obj p,unsigned o,unsigned v) {const auto n=static_cast<unsigned short>(v);memcpy(p+o,&n,2);}
static Obj world() {return pointer(image,0x277DF18);}
static unsigned rebuilds,mismatches,original_calls,last_rebuild_cells;
static bool enabled_verify;
static int mod_setting(const wchar_t *,const wchar_t *,int) {return enabled_verify?1:0;}
static bool dev_group() {return true;}
static void log_event(const char *name,unsigned a=0,unsigned=0,unsigned=0) {
    if(!strcmp(name,"area_index_rebuild")) {++rebuilds;last_rebuild_cells=a;}
    if(!strcmp(name,"area_index_mismatch"))++mismatches;
}
#define COST_SCOPE(name) ((void)0)
static unsigned valid_calls,building_calls;
static bool track_order,ordered=true;
static uintptr_t previous_cell;
static int __fastcall mock_valid(Obj p) {++valid_calls;return p && p[8]?1:0;}
static Obj __fastcall mock_building(Obj c) {
    ++building_calls;const unsigned id=word(c,0x1A);
    if(track_order) {const auto address=reinterpret_cast<uintptr_t>(c);if(address<=previous_cell)ordered=false;previous_cell=address;}
    return pointer(world(),0x6DA98+(id<0xBB9?id:0)*8);
}
template<class F> static F fn(unsigned rva) {
    return reinterpret_cast<F>(rva==0x32F500?reinterpret_cast<void *>(&mock_valid):reinterpret_cast<void *>(&mock_building));
}
// Installation primitives are mocked here; real image guards are covered by split_module_fixture.
struct CodeLink {unsigned char modified[16];};
static Obj allocate_near(Obj) {return nullptr;}
static void absolute_jump(Obj,void *) {}
static bool prepare_link(CodeLink &,unsigned,unsigned char,Obj) {return true;}
static bool publish_link(CodeLink &) {return true;}
static void restore_link(CodeLink &) {}
#define SPLIT_BASE
#include "../src/native_area_index.inl"

struct Cell {unsigned char data[0x30]{};};
struct Area {unsigned char data[0x98]{};};
struct Building {unsigned char data[0x30]{};};
struct Type {unsigned char data[0x40]{};};
static std::vector<Cell> cells(area_index_core::kCells);
static std::vector<Area> areas(502);
static std::vector<Building> buildings(3001);
static std::vector<Type> types(31);
static std::vector<unsigned char> global_data(0x74058);
// Independent native-style full scan (no cache policy/Native helper).
static int full_scan(Obj area) {
    ++original_calls;
    if(!area || !area[8])return 0;
    Obj g=world(),base=pointer(g,0x6CAF0);
    const intptr_t idx=(reinterpret_cast<intptr_t>(area)-reinterpret_cast<intptr_t>(base))/0x98;
    unsigned count=0;
    for(unsigned i=0;i<48400;++i) {
        Obj c=pointer(g,0xE270+i*8);
        if(static_cast<uint32_t>(idx-1)>=500 || word(c,0x12)!=idx)continue;
        const unsigned bid=word(c,0x1A);
        Obj b=pointer(g,0x6DA98+(bid<3001?bid:0)*8);
        if(!b || !b[8])continue;
        const unsigned tid=b[0x12];Obj t=pointer(g,0x73958+(tid<31?tid:0)*8);
        if(!t || !t[8])continue;
        const unsigned k=t[0x2C];if(k==2 || k==3 || k==4)++count;
    }
    return area[0x6E]<=count?1:0;
}
static int __fastcall mock_original(Obj area) {return full_scan(area);}
static int __fastcall wrong_original(Obj) {return 7;}
static void setup() {
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x277DF20,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    expect(image!=nullptr,"fake image allocated");put(image,0x277DF18,global_data.data());
    Obj g=world();
    for(unsigned i=0;i<48400;++i)put(g,0xE270+i*8,cells[i].data);
    for(auto &a:areas)a.data[8]=1;
    areas[1].data[9]=1; // mock-valid unaligned pointer exercises range fallback
    for(unsigned i=0;i<501;++i)put(g,0x6CAF0+i*8,areas[i].data);
    for(unsigned i=0;i<3001;++i)put(g,0x6DA98+i*8,buildings[i].data);
    for(unsigned i=0;i<31;++i)put(g,0x73958+i*8,types[i].data);
    area_index::original=mock_original;
    LARGE_INTEGER q;expect(QueryPerformanceFrequency(&q)!=0,"fixture clock available");area_index::frequency=q.QuadPart;
}
static void compare_all() {
    for(unsigned i=1;i<=500;++i)expect(area_index::check(areas[i].data)==full_scan(areas[i].data),"all regions equal independent native full scan");
}
int main(int argc,char **argv) {
    setup();std::mt19937 random(20261025);
    expect(!area_index_guards_match(),"empty synthetic text does not pass native guards");
    expect(argc==2,"existing captured code supplied only as data");
    FILE *file=nullptr;expect(fopen_s(&file,argv[1],"rb")==0,"captured code data opened");
    const auto copied=fread(image+0x1000,1,0x277CF20,file);fclose(file);
    expect(copied>0x7F9634 && area_index_guards_match(),"captured synthetic image passes five function and three caller guards");
    expect(!area_index::install(),"mock allocation failure does not publish a hook");
    Obj relay=static_cast<Obj>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    expect(relay && area_index::prepare(relay),"real entry preparation accepts captured native bytes");
    expect(!memcmp(relay+32,image+0x2C9E30,8),"entry bridge preserves complete original prolog");
    Obj continuation=nullptr;memcpy(&continuation,relay+42,8);
    expect(continuation==image+0x2C9E38 && !memcmp(relay+40,"\x49\xBB",2) && !memcmp(relay+50,"\x41\xFF\xE3",3),"bridge continues via volatile R11 after original eight bytes");
    alignas(16) DWORD64 stack[32]{};stack[14]=0x12345678;stack[15]=0x98765432;
    CONTEXT context{};context.Rsp=reinterpret_cast<DWORD64>(stack);context.Rip=reinterpret_cast<DWORD64>(relay+40);
    void *handler=nullptr;DWORD64 establisher=0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER,reinterpret_cast<DWORD64>(relay),context.Rip,
        reinterpret_cast<RUNTIME_FUNCTION *>(relay+64),&context,&handler,&establisher,nullptr);
    expect(context.Rdi==0x12345678 && context.Rip==0x98765432 && context.Rsp==reinterpret_cast<DWORD64>(stack+16),"Windows unwinder restores copied PUSH RDI / SUB RSP,70h and return address");
    VirtualFree(relay,0,MEM_RELEASE);area_index::original=mock_original;
    for(auto &c:cells)putword(c.data,0x12,random()%503);
    for(unsigned layout=0;layout<64;++layout) {
        for(auto &c:cells)putword(c.data,0x1A,random()%3010);
        for(auto &b:buildings) {b.data[8]=static_cast<unsigned char>(random()%2);b.data[0x12]=static_cast<unsigned char>(random()%40);}
        for(auto &t:types) {t.data[8]=static_cast<unsigned char>(random()%2);t.data[0x2C]=static_cast<unsigned char>(random()%256);}
        // Include every native class and both sides of the type table's fallback boundary.
        types[0].data[8]=1;types[0].data[0x2C]=2;types[1].data[8]=1;types[1].data[0x2C]=3;
        types[2].data[8]=1;types[2].data[0x2C]=4;
        for(auto &a:areas)a.data[0x6E]=static_cast<unsigned char>(random()%100);
        compare_all();
    }
    expect(rebuilds==1 && last_rebuild_cells==48400,"64 random layouts/32000 region results use one full map rebuild");
    const uintptr_t first=reinterpret_cast<uintptr_t>(areas[0].data);
    for(unsigned i:{0u,1u,500u,501u})expect(area_index_core::region(first+i*0x98,first)==(i==1 || i==500?i:0),"region index boundary");
    expect(!area_index_core::region(first-1,first) && !area_index_core::region(first+0x99,first),"outside and unaligned pointers rejected");
    expect(!area_index_core::region(first,0),"missing area base rejected");
    for(Obj a:{areas[0].data,areas[501].data,areas[1].data+1}) {
        const unsigned before=original_calls;
        expect(area_index::check(a)==full_scan(a) && original_calls==before+2,"invalid area uses original");
    }
    const unsigned before_invalid=original_calls;
    expect(area_index::check(nullptr)==0 && original_calls==before_invalid,"native invalid input returns zero without fallback");
    areas[1].data[8]=0;expect(area_index::check(areas[1].data)==0,"invalid area object preserves native validity");areas[1].data[8]=1;
    for(unsigned day=0;day<365;++day)area_index::check(areas[1].data);
    expect(rebuilds==1,"date changes do not rebuild fixed map region data");
    // Each identity component changes independently, then remains stable for many calls.
    auto loaded=global_data;put(image,0x277DF18,loaded.data());
    unsigned want=rebuilds+1;compare_all();expect(rebuilds==want,"changed world rebuilds once");
    std::swap(cells.front(),cells.back()); // pointers stay fixed: region mismatch is handled below
    for(unsigned offset:{0xE270u,0x6CAE8u,0x6CAF0u}) {
        Obj old=pointer(world(),offset);
        if(offset==0x6CAF0)put(world(),offset,areas[1].data);
        else put(world(),offset,offset==0xE270?cells.back().data:cells.front().data);
        want=rebuilds+1;area_index::check(areas[2].data);area_index::check(areas[2].data);
        expect(rebuilds==want,"changed first/last/area0 pointer rebuilds exactly once");
        put(world(),offset,old);area_index::check(areas[2].data);
    }
    // Recover from a changed region ID without identity changes, on its old area's next use.
    area_index::cache.invalidate();area_index::check(areas[1].data);
    Obj changed=nullptr;for(auto &c:cells)if(word(c.data,0x12)==1) {changed=c.data;break;}
    expect(changed!=nullptr,"old region contains a cell");putword(changed,0x12,500);
    unsigned before=original_calls;want=rebuilds;
    expect(area_index::check(areas[1].data)==full_scan(areas[1].data) && original_calls==before+2,"stale region returns native result for this call");
    expect(area_index::dirty==1 && rebuilds==want,"stale list is invalidated without rebuilding inside shared read");
    compare_all();expect(rebuilds==want+1 && !area_index::dirty,"next use rebuilds once and restores all region results");
    // Count/limit boundaries and unsigned class arithmetic; facility counts are never cached.
    for(auto &c:cells)putword(c.data,0x1A,0);
    buildings[0].data[8]=0;buildings[1].data[8]=1;buildings[1].data[0x12]=1;
    types[1].data[8]=1;types[1].data[0x2C]=2;
    unsigned n=0;for(auto &c:cells)if(word(c.data,0x12)==1 && n<10) {putword(c.data,0x1A,1);++n;}
    expect(n==10,"ten facilities prepared");
    for(unsigned limit:{0u,9u,10u,11u,255u}) {areas[1].data[0x6E]=static_cast<unsigned char>(limit);expect(area_index::check(areas[1].data)==(limit<=10?1:0),"same/one-away/byte threshold boundary");}
    for(unsigned kind=0;kind<256;++kind) {types[1].data[0x2C]=static_cast<unsigned char>(kind);areas[1].data[0x6E]=10;expect(area_index::check(areas[1].data)==(kind>=2 && kind<=4?1:0),"all byte kinds use unsigned native 2/3/4 test");}
    area_index::verify=true;area_index::original=wrong_original;
    expect(area_index::check(areas[1].data)==7 && mismatches==1,"diagnostic mismatch logs and returns exact original value");
    area_index::verify=false;area_index::original=mock_original;
    // Real wrapper's lock paths: rebuild owner never blocks another caller; warmed readers coexist.
    std::atomic<bool> entered{false},go{false};
    std::thread writer([&] {AcquireSRWLockExclusive(&area_index::lock);entered=true;while(!go.load())SwitchToThread();ReleaseSRWLockExclusive(&area_index::lock);});
    while(!entered.load())SwitchToThread();before=original_calls;
    expect(area_index::check(areas[1].data)==full_scan(areas[1].data) && original_calls==before+2,"another rebuild owner falls back to original");go=true;writer.join();
    entered=false;go=false;
    std::thread reader([&] {AcquireSRWLockShared(&area_index::lock);entered=true;while(!go.load())SwitchToThread();ReleaseSRWLockShared(&area_index::lock);});
    while(!entered.load())SwitchToThread();before=original_calls;
    area_index::check(areas[1].data);expect(original_calls==before,"another shared reader uses cached list");
    InterlockedExchange(&area_index::dirty,1);area_index::check(areas[1].data);
    expect(original_calls==before+1,"rebuild blocked by shared reader uses original");go=true;reader.join();
    want=rebuilds+1;area_index::check(areas[1].data);expect(rebuilds==want,"rebuild succeeds after shared reader releases");
    valid_calls=building_calls=0;track_order=true;area_index::check(areas[1].data);track_order=false;
    expect(building_calls>0 && building_calls<48400 && valid_calls>=building_calls,"warm native wrapper visits only that region and retains native validity calls");
    expect(ordered,"cached cells retain original full map traversal order");
    VirtualFree(image,0,MEM_RELEASE);
    printf("{\"status\":\"area_index_fixture_passed\",\"checks\":%u,\"random_layouts\":64,\"region_comparisons\":32000,\"gameplay_verified\":false}\n",checks);
}
