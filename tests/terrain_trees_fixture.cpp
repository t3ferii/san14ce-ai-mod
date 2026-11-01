// User/Claude 2026-10-29: standalone fixture instrumentation declaration.
#ifndef COST_SCOPE
#define COST_SCOPE(name) do {} while(0)
#endif
// User/Claude 2026-10-29: real tree queue/wrapper, synthetic objects and setters.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <vector>
using Obj=unsigned char *;
static Obj image;
static bool enabled=true;
static struct {bool convenience=true,terrain_edit=true;} mod_features;
static unsigned checks,set_calls,native_calls,captures,applies,last_cells,last_writes;
static DWORD callback_thread;
static bool in_callback,expect_work;
static void require(bool b,const char *label) {++checks;if(!b){fprintf(stderr,"FAIL %s (%u)\n",label,checks);ExitProcess(1);}}
static Obj pointer(Obj p,unsigned off) {return *reinterpret_cast<Obj *>(p+off);}
static void put(Obj p,unsigned off,Obj v) {memcpy(p+off,&v,sizeof(v));}
static void number(Obj p,unsigned off,unsigned v) {memcpy(p+off,&v,sizeof(v));}
static void *fixture_function(unsigned);
template<class T> static T fn(unsigned r) {return reinterpret_cast<T>(fixture_function(r));}
#ifndef NO_RUNTIME_LOG
static void log_event(const char *kind,unsigned a,unsigned b,unsigned c) {
    if(!strcmp(kind,"terrain_trees_capture")){++captures;require(b==1 && c==0,"capture log payload");}
    if(!strcmp(kind,"terrain_trees_apply")){++applies;last_cells=a;last_writes=b;require(c==0,"apply log payload");}
}
#endif
#include "../src/native_terrain_trees.inl"
static bool terrain_render_fixture_site(void **,void *) {return true;}
#define TERRAIN_RENDER_FIXTURE
#include "../src/native_terrain_render.inl"
static unsigned char s[0x7400],r[0x98],s2[0x7400],r2[0x98];
static std::vector<unsigned char> pixels(1024*1024),other_pixels(1024*1024),baseline;
static Obj expected_s;
static constexpr uint64_t Result=0xFEDCBA9876543210ull;
static uint64_t __fastcall native_update(Obj a,Obj b,Obj c,Obj d,Obj e,unsigned f,unsigned g,unsigned h) {
    require(in_callback && GetCurrentThreadId()==callback_thread,"native callback thread preserved");
    require(a==expected_s && b==image+11 && c==image+23 && d==image+37 && e==image+53 &&
        f==0x80000001u && g==0xFEDCBA98u && h==0x7FFFFFFFu,"all eight arguments preserved, including three masks");
    if(expect_work){require(set_calls>0 && !terrain_trees::dirty_count,"pending writes completed before original callback");expect_work=false;}
    ++native_calls;return Result;
}
static void __fastcall native_set(Obj map,uint64_t packed,unsigned char value) {
    require(in_callback && GetCurrentThreadId()==callback_thread,"setter runs only on native callback thread");
    require(map==r || map==r2,"setter receives validated resource map");
    unsigned x=static_cast<unsigned>(packed),z=static_cast<unsigned>(packed>>32);
    require(x<1024 && z<1024,"packed x/z order and range");
    pointer(map,0x58)[static_cast<size_t>(z)*1024+x]=value;++set_calls;
}
static void *fixture_function(unsigned rva) {
    if(rva==terrain_trees::UpdateRva)return reinterpret_cast<void *>(&native_update);
    if(rva==terrain_trees::SetterRva)return reinterpret_cast<void *>(&native_set);
    require(false,"unexpected native call");return nullptr;
}
static void bind(Obj object,Obj map,Obj data) {
    memset(object,0,0x7400);memset(map,0,0x98);
    put(object,0,image+0x1694928);number(object,0x7084,1024);number(object,0x7088,1024);
    number(object,0x708C,128);number(object,0x7090,128);object[0x70B4]=1;put(object,0x70D0,map);
    put(map,0,image+0x1692770);number(map,0x14,1024);number(map,0x18,1024);put(map,0x58,data);
}
static void callback(Obj object=s) {
    expected_s=object;callback_thread=GetCurrentThreadId();in_callback=true;
    require(terrain_trees::update(object,image+11,image+23,image+37,image+53,0x80000001u,0xFEDCBA98u,0x7FFFFFFFu)==Result,"opaque native return preserved");
    in_callback=false;
}
static void reset() {
    using namespace terrain_trees;
    enabled=true;mod_features={true,true};permitted=true;
    memset(goals,0,sizeof(goals));memset(has_goal,0,sizeof(has_goal));clear_queue_locked();
    captured_s=captured_r=captured_data=nullptr;original_resource.clear();world_transition=resync_needed=false;
    set_calls=native_calls=captures=applies=last_cells=last_writes=0;
    std::fill(pixels.begin(),pixels.end(),static_cast<unsigned char>(255));std::fill(other_pixels.begin(),other_pixels.end(),static_cast<unsigned char>(255));
    bind(s,r,pixels.data());bind(s2,r2,other_pixels.data());
}
static constexpr unsigned Hex=100*220+100;
static void sample(unsigned char value,unsigned count) {
    auto c=terrain_trees::center(Hex);unsigned x=static_cast<unsigned>(c.x/200),z=static_cast<unsigned>(c.z/200);
    for(unsigned i=0;i<count;++i)pixels[(z-6+i/11)*1024+x-5+i%11]=value;
}
static void check_hex(unsigned hex,int wanted,const std::vector<unsigned char> &original) {
    // Whole-map comparison proves that every neighbouring cell stays unchanged.
    auto c=terrain_trees::center(hex);int cx=static_cast<int>(c.x/200),cz=static_cast<int>(c.z/200);
    bool okay=true;unsigned owned=0;
    for(unsigned z=0;z<1024;++z)for(unsigned x=0;x<1024;++x) {
        bool own=abs(static_cast<int>(x)-cx)<=4 && abs(static_cast<int>(z)-cz)<=4 && terrain_trees::owns_cell(hex,x,z);
        unsigned char value=own && wanted>=0?static_cast<unsigned char>(wanted):original[z*1024+x];
        if(pixels[z*1024+x]!=value)okay=false;
        if(own)++owned;
    }
    require(okay,"target cells match; all other cells unchanged");require(owned>=12 && owned<=17,"hex owns expected 200-world cells");
}
static void validation_tests() {
    for(unsigned off:{0u,0x7084u,0x7088u,0x708Cu,0x7090u,0x70B4u,0x70D0u}) {
        reset();s[off]^=0x10;terrain_trees::mark(Hex,5,1);callback();
        require(set_calls==0 && terrain_trees::original_resource.empty() && terrain_trees::dirty_count==1,"invalid placement object leaves queue untouched");
    }
    for(unsigned off:{0u,0x14u,0x18u,0x58u}) {
        reset();if(off==0x58)put(r,off,nullptr);else r[off]^=0x10;
        terrain_trees::mark(Hex,5,1);callback();require(set_calls==0 && terrain_trees::original_resource.empty(),"invalid resource object unprocessed");
    }
    reset();terrain_trees::mark(Hex,5,1);callback(nullptr);callback(reinterpret_cast<Obj>(1));
    require(set_calls==0 && native_calls==2,"null/unreadable object passes through original without setter");
    reset();s[0x70B4]=0;terrain_trees::mark(Hex,5,1);callback();require(set_calls==0,"initial scan must finish first");
    s[0x70B4]=1;callback();require(set_calls>0,"queued goal survives initial scan");
}
static void rule_tests() {
    reset();sample(4,70);baseline=pixels;terrain_trees::mark(Hex,5,1);
    require(set_calls==0,"UI mark never calls setter");expect_work=true;callback();check_hex(Hex,4,baseline);
    require(native_calls==1 && !terrain_trees::dirty_count,"queue drains before one original invocation");
    require(terrain_trees::original_resource==baseline,"original snapshot retained after writes");
#ifndef NO_RUNTIME_LOG
    require(captures==1 && applies==1 && last_cells==1 && last_writes>0,"capture once and apply once per nonempty frame");
#endif
    terrain_trees::mark(Hex,1,1);callback();check_hex(Hex,-1,baseline);
    require(terrain_trees::original_resource==baseline,"restoration does not recapture edited map");
    reset();sample(6,70);sample(5,30);baseline=pixels;terrain_trees::mark(Hex,6,1);callback();check_hex(Hex,5,baseline);
    reset();sample(0,60);baseline=pixels;terrain_trees::mark(Hex,6,1);callback();check_hex(Hex,0,baseline);
    for(unsigned target:{5u,6u}) {reset();baseline=pixels;terrain_trees::mark(Hex,target,1);callback();check_hex(Hex,target==5?0:5,baseline);}
    reset();sample(6,50);baseline=pixels;terrain_trees::mark(Hex,1,5);callback();check_hex(Hex,255,baseline);
    terrain_trees::mark(Hex,5,5);callback();check_hex(Hex,-1,baseline);
    reset();sample(4,60);baseline=pixels;terrain_trees::mark(Hex,3,1);callback();check_hex(Hex,-1,baseline);
    for(unsigned value:{0u,5u,6u}) {
        reset();sample(static_cast<unsigned char>(value),60);baseline=pixels;
        terrain_trees::mark(Hex,5,1);callback();check_hex(Hex,static_cast<int>(value),baseline);
    }
    reset();sample(5,60);callback();std::fill(pixels.begin(),pixels.end(),static_cast<unsigned char>(0));
    require(terrain_trees::nearby_value(Hex,6)==5,"mode uses pristine snapshot rather than edited live bytes");
    reset();sample(6,55);baseline=pixels;terrain_trees::mark(Hex,6,5);callback();check_hex(Hex,5,baseline);
    terrain_trees::mark(Hex,5,5);callback();check_hex(Hex,-1,baseline);
    reset();terrain_trees::mark(Hex,5,1);terrain_trees::mark(Hex,6,1);require(terrain_trees::dirty_count==1,"latest goal deduplicated");
    callback();require(pixels[static_cast<unsigned>(terrain_trees::center(Hex).z/200)*1024+static_cast<unsigned>(terrain_trees::center(Hex).x/200)]==5,"latest jungle target wins");
}
static void lifecycle_tests() {
    reset();baseline=pixels;terrain_trees::mark(Hex,5,1);terrain_trees::mark(Hex+1,6,1);callback();
    auto snapshot=terrain_trees::original_resource;
    terrain_trees::before_world_change();terrain_trees::mark(Hex,1,1);callback();
    require(terrain_trees::has_goal[Hex] && terrain_trees::goals[Hex].terrain==5 && terrain_trees::dirty_count==0,"load rollback keeps targets and pauses queue");
    require(terrain_trees::take_ui_work(false),"load host requests complete sidecar replay");
    terrain_trees::mark(Hex,5,1);callback();check_hex(Hex,0,baseline);
    require(terrain_trees::original_resource==snapshot,"same-object load preserves pristine snapshot");
    require(terrain_trees::take_ui_work(true),"renderer resync also resets tree goals");callback();require(pixels==baseline,"missing sidecar goals restore original after save switch");
    for(bool boundary:{false,true}) {
        reset();baseline=pixels;terrain_trees::mark(Hex,5,1);callback();terrain_trees::take_ui_work(true);
        if(boundary)terrain_trees::before_world_change();
        terrain_trees::take_ui_work(true);callback();
        require(pixels==baseline,"pending restoration survives another resync or load before callback");
    }
    reset();terrain_trees::mark(Hex,6,1);callback();other_pixels.assign(other_pixels.size(),4);auto original2=other_pixels;
    callback(s2);require(terrain_trees::captured_s==s2 && terrain_trees::captured_r==r2 && terrain_trees::original_resource==original2,"new S/R captured and targets reapplied");
    require(other_pixels!=original2,"replacement scene receives retained target");
    reset();terrain_trees::mark(Hex,5,1);callback();other_pixels.assign(other_pixels.size(),6);auto replacement=other_pixels;
    put(r,0x58,other_pixels.data());callback();require(terrain_trees::original_resource==replacement,"buffer replacement also refreshes snapshot");
    reset();for(unsigned i=0;i<130;++i)terrain_trees::mark(Hex+i,5,1);
    callback();require(terrain_trees::dirty_count==66,"first callback processes at most 64 hexes");
#ifndef NO_RUNTIME_LOG
    require(last_cells==64 && applies==1,"one bounded frame log");
#endif
    callback();require(terrain_trees::dirty_count==2,"second callback also capped at 64");callback();require(!terrain_trees::dirty_count,"remaining goals processed");
    reset();std::thread ui([](){for(unsigned i=0;i<1000;++i)terrain_render::mark(Hex,i%2?6:5,1);});ui.join();
    require(set_calls==0 && terrain_trees::dirty_count==1,"render mark forwards UI work under tree SRW lock without native call");
    callback();require(!terrain_trees::dirty_count,"callback drains work from different UI thread");
    for(unsigned gate=0;gate<4;++gate) {
        reset();if(gate==0)enabled=false;if(gate==1)mod_features.convenience=false;if(gate==2)mod_features.terrain_edit=false;if(gate==3)terrain_trees::permitted=false;
        terrain_trees::mark(Hex,5,1);callback();require(!set_calls && native_calls==1 && !terrain_trees::dirty_count,"feature gate passes original through without tree work");
    }
}
static void geometry_tests() {
    for(unsigned hex:{Hex,Hex+1,0u,48399u}) {
        auto c=terrain_trees::center(hex);unsigned cx=static_cast<unsigned>(c.x/200),cz=static_cast<unsigned>(c.z/200);
        bool okay=true;
        for(unsigned z=cz-3;z<=cz+3;++z)for(unsigned x=cx-3;x<=cx+3;++x) {
            double best=1e99;unsigned nearest=48400;
            for(unsigned h=0;h<48400;++h) {
                auto n=terrain_trees::center(h);double dx=x*200.0+100-n.x,dz=z*200.0+100-n.z,d=dx*dx+dz*dz;
                if(d<best){best=d;nearest=h;}
            }
            if(terrain_trees::owns_cell(hex,x,z)!=(nearest==hex))okay=false;
        }
        require(okay,"cell ownership agrees with independent whole-grid nearest centre, both parities and map edges");
    }
}
int main() {
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x1700000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));require(image!=nullptr,"synthetic image");
    validation_tests();rule_tests();lifecycle_tests();geometry_tests();
    printf("{\"terrain_trees_checks\":%u,\"failed\":0}\n",checks);
}
