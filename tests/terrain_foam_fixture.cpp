#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <cmath>
#include <vector>
#include <fstream>
#include <iterator>
// User 2026-10-30: release feature; the same fixture runs with and without NO_RUNTIME_LOG.
using Obj=unsigned char *;
static Obj image;
static bool enabled=true;
static struct {bool convenience=true,terrain_edit=true;unsigned terrain_diag_mode=0;} mod_features;
static unsigned checks,lookups,native_calls,set_calls,consumer_time,consumer_render,ready_logs,creates,destroys;
static bool in_callback=false,native_reached=false,ready_success=true,ready_runtime=true;
static Obj create_object;
static constexpr uint64_t Result=0xFEDCBA9876543210ull;
static void require(bool value,const char *label) {++checks;if(!value){fprintf(stderr,"FAIL %u: %s\n",checks,label);ExitProcess(1);}}
static Obj pointer(Obj p,unsigned offset) {Obj value;memcpy(&value,p+offset,8);return value;}
static void put(Obj p,unsigned offset,Obj value) {memcpy(p+offset,&value,8);}
static void number(Obj p,unsigned offset,unsigned value) {memcpy(p+offset,&value,4);}
static void log_event(const char *name,unsigned=0,unsigned=0,unsigned=0) {
    if(!strcmp(name,"foam_consume_time"))++consumer_time;
    if(!strcmp(name,"foam_consume_render"))++consumer_render;
    if(!strcmp(name,"foam_ready"))++ready_logs;
}
static void *function(unsigned);
template<class T> static T fn(unsigned rva) {return reinterpret_cast<T>(function(rva));}
#define TERRAIN_FOAM_FIXTURE_LOOKUP() (++lookups)
#include "../src/native_terrain_foam.inl"
static unsigned char objects[terrain_foam::Count][0xF0],handles[terrain_foam::Count][0x28];
static bool __fastcall original_create(Obj a,Obj b,Obj c,Obj handle,Obj properties) {
    require(a==image+1 && b==image+2 && c==image+3 && properties==image+4,"create five arguments");
    put(handle,0x20,create_object);++creates;return true;
}
static bool __fastcall original_complete(Obj a,Obj b,Obj c,Obj handle,unsigned *state,unsigned char *flag) {
    require(a==image+1 && b==image+2 && c==image+3,"complete registers");
    *flag=1;
    if(ready_success){*state=2;put(pointer(handle,0x20),0xA0,ready_runtime?image+100:nullptr);}
    return ready_success;
}
static uint64_t __fastcall original_destroy(Obj a,Obj b,Obj c,Obj handle) {
    require(a==image+1 && b==image+2 && c==image+3,"destroy arguments");
    for(const auto &e:terrain_foam::effects)require(e.object!=pointer(handle,0x20),"destroy removes identity before original");
    ++destroys;return Result;
}
static uint64_t __fastcall original_callback(Obj,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e,uintptr_t f,uintptr_t g,uintptr_t h) {
    require(in_callback,"original callback context");
    require(b==0x100000002ull && c==0x200000003ull && d==0x300000004ull && e==0x400000005ull && f==0x500000006ull && g==0x600000007ull && h==0x7003F800000ull,"all eight ABI slots, float time bits, wide masks preserved");
    native_reached=true;++native_calls;return Result;
}
static void setter(Obj object,const float *values,unsigned offset) {
    require(in_callback && !native_reached,"setter before original on callback thread");
    require((reinterpret_cast<uintptr_t>(values)&15)==0,"setter float4 aligned 16 bytes");
    for(unsigned i=0;i<4;++i)require(std::isfinite(values[i]),"finite float4");
    if(offset==0x18)require(values[3]==1.0f,"position w=1");
    if(offset==0x34)require(values[0]>0 && values[1]>0 && values[2]>0 && values[3]==1.0f,"positive scale and padding");
    memcpy(object+offset,values,offset==0x24?16:12);++set_calls;
}
static void __fastcall set_pos(Obj p,const float *v){setter(p,v,0x18);}
static void __fastcall set_quat(Obj p,const float *v){setter(p,v,0x24);}
static void __fastcall set_scale(Obj p,const float *v){setter(p,v,0x34);}
static void *function(unsigned rva) {
    switch(rva) {
    case 0xE17CA0:return reinterpret_cast<void *>(&original_create);
    case 0xE1B580:return reinterpret_cast<void *>(&original_complete);
    case 0xE18280:return reinterpret_cast<void *>(&original_destroy);
    case 0xE1B7B0:case 0xE19690:return reinterpret_cast<void *>(&original_callback);
    case 0xE1AAE0:return reinterpret_cast<void *>(&set_pos);
    case 0xE1ADD0:return reinterpret_cast<void *>(&set_quat);
    case 0xE1B160:return reinterpret_cast<void *>(&set_scale);
    default:require(false,"unexpected native function");return nullptr;
    }
}
static void callback(unsigned i,bool time=true) {
    in_callback=true;native_reached=false;
    auto wrapper=time?terrain_foam::time_update:terrain_foam::render;
    require(wrapper(objects[i],0x100000002ull,0x200000003ull,0x300000004ull,0x400000005ull,0x500000006ull,0x600000007ull,0x7003F800000ull)==Result,"full opaque RAX preserved");
    in_callback=false;
}
static void add(unsigned i,bool ready=true) {
    using namespace terrain_foam;
    memset(objects[i],0,sizeof(objects[i]));put(objects[i],0,image+0x16DA140);
    memcpy(objects[i]+0x18,sources[i].pos,12);memcpy(objects[i]+0x24,sources[i].quat,16);memcpy(objects[i]+0x34,sources[i].scale,12);
    create_object=objects[i];
    require(create(image+1,image+2,image+3,handles[i],image+4),"original create result");
    require(effects[i].object==objects[i] && !effects[i].ready,"position matched 35-entry source table");
    if(ready){unsigned state=0;unsigned char flag=0;require(complete(image+1,image+2,image+3,handles[i],&state,&flag),"ready result");require(effects[i].ready && state==2,"ready requires state2 and A0");}
}
int main(int argc,char **argv) {
    using namespace terrain_foam;
    require(argc==2 && argv[1],"KIDS evidence path");
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x1800000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));require(image!=nullptr,"synthetic image allocation");
    // User 2026-11-02: zoom fix replaces per-constructor VirtualQuery with a fault-guarded snapshot.
    {
        CaptureFields sample={};unsigned char handle[0x28]={},object[0xF0]={};
        require(!capture_fields(nullptr,sample),"null handle rejected");
        require(!capture_fields(handle,sample),"null object rejected");
        put(handle,0x20,object);
        require(!capture_fields(handle,sample),"foreign object vtable rejected");
        put(object,0,image+0x16DA140);
        memcpy(object+0x18,sources[0].pos,12);memcpy(object+0x24,sources[0].quat,16);memcpy(object+0x34,sources[0].scale,12);
        require(capture_fields(handle,sample) && sample.object==object,"valid native-created identity retained");
        require(!memcmp(sample.pos,sources[0].pos,12) && !memcmp(sample.quat,sources[0].quat,16) && !memcmp(sample.scale,sources[0].scale,12),"all original transforms snapshotted exactly");
        SYSTEM_INFO info={};GetSystemInfo(&info);
        Obj pages=static_cast<Obj>(VirtualAlloc(nullptr,info.dwPageSize*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        require(pages!=nullptr,"protected-memory fixture allocated");
        DWORD old=0;require(VirtualProtect(pages+info.dwPageSize,info.dwPageSize,PAGE_NOACCESS,&old)!=0,"second page inaccessible");
        require(!capture_fields(pages+info.dwPageSize,sample),"inaccessible handle rejected without crash");
        put(handle,0x20,pages+info.dwPageSize);require(!capture_fields(handle,sample),"inaccessible object rejected without crash");
        Obj truncated=pages+info.dwPageSize-0x24;put(truncated,0,image+0x16DA140);
        put(handle,0x20,truncated);require(!capture_fields(handle,sample),"readable identity with inaccessible transform rejected");
        require(VirtualFree(pages,0,MEM_RELEASE)!=0,"protected-memory fixture released");
    }
    for(auto &h:hooks){put(image,h.slot,image+h.rva);memcpy(image+h.rva,h.bytes,16);}
    for(unsigned i=0;i<3;++i)memcpy(image+SetterRvas[i],SetterBytes[i],16);
    require(guards_match(),"all original qwords and function bytes match");
    for(auto &h:hooks) {
        image[h.rva]^=1;require(!guards_match(),"each mismatched function rejected");image[h.rva]^=1;
        put(image,h.slot,image+h.rva+1);require(!guards_match(),"each mismatched slot rejected");put(image,h.slot,image+h.rva);
    }
    for(unsigned i=0;i<3;++i){image[SetterRvas[i]]^=1;require(!guards_match(),"setter fingerprint mismatch rejected");image[SetterRvas[i]]^=1;}
    mod_features.terrain_edit=false;install();require(!installed,"terrain edit off no publication");mod_features.terrain_edit=true;
    image[hooks[0].rva]^=1;install();require(!permitted && !installed,"guard mismatch disables foam only");
    require(mod_features.terrain_edit && mod_features.convenience,"guard mismatch leaves other feature flags intact");
    for(auto &h:hooks)require(pointer(image,h.slot)==image+h.rva,"mismatch leaves every slot untouched");
    image[hooks[0].rva]^=1;permitted=true;install();require(installed && guards_match(),"install checked slots");
    for(auto &h:hooks)require(pointer(image,h.slot)==reinterpret_cast<Obj>(h.wrapper),"only requested slots published");
    add(0,false);
    unsigned state=0;unsigned char flag=0;ready_success=false;
    require(!complete(image+1,image+2,image+3,handles[0],&state,&flag) && !effects[0].ready,"not ready on failed completion");
    ready_success=true;ready_runtime=false;complete(image+1,image+2,image+3,handles[0],&state,&flag);
    require(state==2 && !effects[0].ready,"state2 success without A0 is not ready");ready_success=false;ready_runtime=true;
    mark(sources[0].owner,0,24);unsigned before=set_calls;callback(0,false);require(set_calls==before,"unready effect setters deferred");
    ready_success=true;complete(image+1,image+2,image+3,handles[0],&state,&flag);
    before=consumer_render;callback(0,false);require(consumer_render==before+1,"render fallback before any foam time observation");
    require(*reinterpret_cast<float *>(objects[0]+0x34)==S_HIDE,"original hidden with nonzero scale");
    mark(sources[0].owner,24,24);callback(0);
    require(!memcmp(objects[0]+0x34,sources[0].scale,12),"hidden original restores original scale");
    for(unsigned i=1;i<35;++i)add(i);
    // User/Claude 2026-10-29: save load does not recreate the startup effects.
    // Preserve object/handle/ready/A0/original transforms and observation flags.
    Effect startup[Count];memcpy(startup,effects,sizeof(startup));unsigned startup_creates=creates;
    before=set_calls;unsigned old_generation=generation;
    before_world_change();require(generation==old_generation+1 && !dirty_count && set_calls==before,"load clears numeric work without calling setters");
    for(unsigned i=0;i<SourceCount;++i) {
        const Effect &e=effects[i],&old=startup[i];
        require(e.object==old.object && e.handle==old.handle && e.ready==old.ready && pointer(e.object,0xA0)==image+100,"load retains live identities, ready and A0");
        require(!memcmp(e.pos,old.pos,12) && !memcmp(e.quat,old.quat,16) && !memcmp(e.scale,old.scale,12),"load retains original transforms");
        require(e.hex==sources[i].owner && e.time_seen==old.time_seen && e.render_seen==old.render_seen && !e.job && !e.restore_job,"unmodified effects and observations survive load");
    }
    require(creates==startup_creates && any_time,"no recreation needed and known time consumer retained");
    // The last ready unused source is chosen; edited original owners cannot donate.
    mark(sources[34].owner,0,24);callback(34);mark(12345,24,0);callback(33);
    require(effects[33].used && effects[33].hex==12345 && !effects[34].used,"last eligible donor selected");
    require(*reinterpret_cast<float *>(objects[33]+0x18)==static_cast<float>(37594.0+(12345%220)*702.0),"moved to target center x");
    require(*reinterpret_cast<float *>(objects[33]+0x1C)==sources[33].pos[1],"original y preserved");
    unsigned r=12345/220,c=12345%220;
    require(*reinterpret_cast<float *>(objects[33]+0x20)==static_cast<float>(13335.3+r*810.5997779422346+(c%2==0?810.5997779422346/2:0)),"center z including even-column half offset");
    require(effects[33].job==false,"only matching callback consumes job");
    mark(12345,0,0);callback(33);require(*reinterpret_cast<float *>(objects[33]+0x34)==S_HIDE,"moved effect hides");
    mark(12345,24,0);callback(33);require(!memcmp(objects[33]+0x34,sources[33].scale,12),"moved effect restores");
    mark(12346,24,0);callback(0);require(effects[32].hex==12346 && effects[32].job,"used donor never chosen again");
    before=set_calls;callback(0);require(set_calls==before,"other object's callback cannot consume job");callback(32);
    // Complete both observations: the hot path must stop doing object lookups.
    for(unsigned i=0;i<35;++i){callback(i);callback(i,false);}
    require(watch==0,"observations complete and no pending work");
    unsigned lookup_before=lookups;before=set_calls;callback(0);callback(0,false);
    require(lookups==lookup_before && set_calls==before,"idle wrappers no lookup or setter");
    mark(sources[0].owner,0,24);require(effects[0].job==false,"UI mark stores numbers only");
    require(destroy(image+1,image+2,image+3,handles[0])==Result,"destroy RAX preserved");
    require(!effects[0].object && !effects[0].job,"destroy invalidates object and job");
    // Second load: old moved/hidden effects restore first, even with no mark.
    // A repeated boundary before callbacks must not lose restoration work.
    Effect edited[Count];memcpy(edited,effects,sizeof(edited));
    before=set_calls;before_world_change();require((watch&4) && !dirty_count && set_calls==before,"load arms restoration watch without native access");
    require(!effects[0].object,"only the earlier destroy removed an identity");
    for(unsigned i=1;i<SourceCount;++i) {
        const Effect &e=effects[i],&old=edited[i];
        require(e.object==old.object && e.handle==old.handle && e.ready && pointer(e.object,0xA0)==image+100,"edited effect identities survive another load");
        require(!memcmp(e.pos,old.pos,12) && !memcmp(e.quat,old.quat,16) && !memcmp(e.scale,old.scale,12) && e.time_seen==old.time_seen && e.render_seen==old.render_seen,"original transforms and callback observations retained");
        require(!e.used && !e.hidden && e.hex==sources[i].owner,"load resets only assignment metadata");
        require(e.job==(i>=32) && e.restore_job==(i>=32),"only the two moved and one hidden effects require restoration");
    }
    mark(12345,0,0);require(!dirty_count,"rollback notifications suppressed during world boundary");
    before_world_change();require(effects[32].restore_job && effects[33].restore_job && effects[34].restore_job && (watch&4),"repeated load preserves unfinished restores");
    // Consume one restore while transition is still true, before a load-resync mark.
    callback(32);require(transition && !effects[32].job && !memcmp(objects[32]+0x18,sources[32].pos,12),"callback restores during transition without any mark");
    mark(sources[31].owner,0,24);before=set_calls;callback(1);
    require(dirty_count==1 && set_calls==before,"new-save planning waits for remaining restores");
    callback(33);require(dirty_count==1 && !effects[33].job,"another restore finishes before replanning");callback(34);
    for(unsigned i=32;i<35;++i)require(!memcmp(objects[i]+0x18,sources[i].pos,12) && !memcmp(objects[i]+0x24,sources[i].quat,16) && !memcmp(objects[i]+0x34,sources[i].scale,12) && !effects[i].restore_job,"moved and hidden objects restored to original complete transform");
    require(!dirty_count && effects[31].job && !effects[31].restore_job,"new save goal planned after restoration barrier");callback(31);
    require(*reinterpret_cast<float *>(objects[31]+0x34)==S_HIDE,"hide still works after loading without recreation");
    mark(sources[31].owner,24,24);callback(31);require(!memcmp(objects[31]+0x34,sources[31].scale,12),"show still works after loading");
    mark(12347,24,0);callback(34);require(effects[34].used && effects[34].hex==12347 && creates==startup_creates,"donor use resets and movement works after load without new effects");
    // A load must not re-enable render fallback after time was observed.
    add(0);mark(sources[0].owner,0,24);before=consumer_render;callback(0,false);
    require(consumer_render==before && effects[0].job,"preserved time observation keeps render read-only after load");callback(0);
    mark(sources[0].owner,24,24);callback(0);before=consumer_render;
    mark(sources[0].owner,0,24);callback(0,false);require(consumer_render==before && effects[0].job,"render cannot consume after time callback observed");callback(0);
    for(unsigned i=0;i<10000;++i){mark(sources[0].owner,(i%2)?24:0,24);callback(0);}
    require(!memcmp(objects[0]+0x34,sources[0].scale,12),"10000 hide restore requests preserve absolute targets");
    restore();require(!installed,"restore disables experiment");
    require(creates==36 && destroys==1 && consumer_time>0 && ready_logs>0,"lifecycle and consumer evidence present");
    printf("{\"ok\":true,\"checks\":%u,\"native_calls\":%u,\"setters\":%u}\n",checks,native_calls,set_calls);
    VirtualFree(image,0,MEM_RELEASE);return 0;
}
