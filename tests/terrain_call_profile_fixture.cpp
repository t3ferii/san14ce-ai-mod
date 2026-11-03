// User 2026-11-02: compare native-call wall/CPU cost in one zoom run, no extra A/B/C runs.
#define TERRAIN_CALL_PROFILE_FIXTURE
#include "../src/terrain_call_profile.h"
#include <string>
#include <vector>
#include <thread>
#include <stdexcept>
static unsigned checks;
static void check(bool pass,const char *label){++checks;if(!pass)throw std::runtime_error(label);}
static uint64_t __fastcall callback(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,
    uintptr_t e,uintptr_t f,uintptr_t g,uintptr_t h){return (a+b+c+d+e+f+g)^h;}
#ifndef NO_RUNTIME_LOG
struct Record{std::string key;unsigned a,b,c;};
static std::vector<Record> records;
static void sink(const char *key,unsigned a,unsigned b,unsigned c){records.push_back({key,a,b,c});}
static terrain_profile::Stamp fake={};
static unsigned samples;
static terrain_profile::Stamp clock_sample(){++samples;return fake;}
#endif
int main(){try {
    using namespace terrain_profile;
    uintptr_t last=0x123456789ABCDEF0ull;
    check(invoke(Id::TextureNative,callback,uintptr_t(1),uintptr_t(2),uintptr_t(3),uintptr_t(4),uintptr_t(5),uintptr_t(6),uintptr_t(7),last)==(28^last),"8 slots / full RAX preserved");
    Function<decltype(&callback),Id::TextureNative> forwarded{callback};
    check(static_cast<decltype(&callback)>(forwarded)==callback,"original function pointer retained");
    check(forwarded(1,2,3,4,5,6,7,last)==(28^last),"callable original shim forwards arguments");
    int value=7;int &reference=invoke(Id::TextureNative,[&]() -> int & {return value;});
    reference=11;check(value==11,"reference return retained");
    invoke(Id::TextureNative,[&](){value=19;});check(value==19,"void return retained");
#ifndef NO_RUNTIME_LOG
    fixture_stamp=clock_sample;fake={100,1000,10000,true,true};
    {Scope disabled(Id::FoamTime);}
    check(samples==0,"OFF has no clock queries");
    initialize(true,sink);check(running,"real Windows CPU/time initialization");
    frequency=1000000;records.clear();
    {Scope outer(Id::FoamTime);
        fake={120,1100,11000,true,true};
        {Scope inner(Id::FoamTimeNative);fake={200,1300,13000,true,true};}
        fake={300,1400,14000,true,true};
    }
    const Row &outer=rows[static_cast<unsigned>(Id::FoamTime)];
    const Row &inner=rows[static_cast<unsigned>(Id::FoamTimeNative)];
    check(outer.calls==1 && inner.calls==1,"nested invocation counts");
    check(outer.wall==200 && inner.wall==80,"inclusive elapsed time");
    check(outer.self_wall==120 && inner.self_wall==80,"nested wall cost not double counted");
    check(outer.cpu==400 && inner.cpu==200 && outer.self_cpu==200,"CPU and exclusive CPU accounting");
    check(outer.peak_cpu==400 && outer.peak_start==1,"wall peak retains CPU from the same invocation");
    check(outer.cycles==4000 && inner.cycles==2000,"cycle accounting independent of CPU quantization");
    check(records.empty(),"no per-call log IO");
    fake={400,1500,15000,false,false};{Scope unavailable(Id::MapNative);fake={450,1700,16000,false,false};}
    const Row &missing=rows[static_cast<unsigned>(Id::MapNative)];
    check(missing.calls==1 && missing.cpu_samples==0 && missing.cycle_samples==0,"unavailable CPU counters explicitly tracked");
    check(missing.wall==50 && missing.cpu==0,"wall cost retained if CPU API fails");
    fake={500,2000,20000,true,true};
    try{invoke(Id::TextureNative,[]() -> int {throw std::runtime_error("forwarded");});}catch(const std::runtime_error &){}
    check(current==nullptr,"unwinding restores parent scope");
    fake={600,2200,22000,true,true};
    {Scope spanning(Id::Readback);InterlockedIncrement(&frame);fake={800,2400,24000,true,true};}
    check(rows[static_cast<unsigned>(Id::Readback)].first_frame==1 && rows[static_cast<unsigned>(Id::Readback)].last_frame==2,"spanning call has start/completion frame IDs");
    fixture_stamp=nullptr;
    auto worker=[](){for(unsigned i=0;i<50;++i){Scope scope(Id::FoamRenderNative);}};
    std::thread a(worker),b(worker);a.join();b.join();
    check(rows[static_cast<unsigned>(Id::FoamRenderNative)].calls==100,"concurrent callbacks do not lose counts");
    check(rows[static_cast<unsigned>(Id::FoamRenderNative)].thread_id==0xFFFFFFFFu,"multi-thread aggregate explicitly marked");
    initialize(true,sink);records.clear();
    Stamp actual=read_stamp();previous_wall=actual.wall-frequency/5;report_wall=previous_wall;
    checkpoint();
    bool wall=false,cpu=false,window=false,call=false;
    for(const auto &r:records){wall|=r.key=="terrain_frame";cpu|=r.key=="terrain_frame_cpu";window|=r.key=="terrain_cpu_window";
        if(r.key=="terrain_calls_foam_render_E19690")call=r.b==100;}
    check(wall && cpu && window && call,"slow frame emits matching call/CPU/frame records");
    check(rows[static_cast<unsigned>(Id::FoamRenderNative)].calls==0,"reported counters reset");
    records.clear();{Frame test_present(false);}
    check(records.empty() && present_depth==0,"DXGI_PRESENT_TEST does not advance frame");
    {Frame first(true);{Frame nested(true);check(present_depth==2,"nested Present depth");}}
    check(present_depth==0,"Present depth restored");
    Stamp before=read_stamp();Sleep(2);Stamp after=read_stamp();
    check(after.wall>before.wall && after.cpu_ok && after.cycles_ok,"real wall/thread CPU/cycle counters readable");
#endif
    printf("{\"terrain_call_profile_checks\":%u,\"failed\":0,\"game_executed\":false}\n",checks);return 0;
}catch(const std::exception &e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
