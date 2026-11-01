#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <array>
#include <vector>
#include "../src/terrain_edit_policy.h"
using Obj=unsigned char *;
static unsigned checks=0;
static void require(bool ok,const char *name) {++checks;if(!ok) {fprintf(stderr,"FAIL %s (%u)\n",name,checks);ExitProcess(1);}}
static bool enabled=true;
static struct {bool convenience=true,terrain_edit=false;} mod_features;
static unsigned world_reads=0,native_reads=0,io_paths=0;
static unsigned char root[2],tile_data[8][32],unit_data[64],building_data[64];
static Obj current_world=root;
static std::array<Obj,48400> tiles={};
static std::array<Obj,501> units={};
static std::array<Obj,3001> buildings={};
static Obj world() {++world_reads;return current_world;}
static Obj table(unsigned offset,unsigned id,unsigned maximum) {
    ++native_reads;if(id>maximum)return nullptr;
    if(offset==0xE270)return tiles[id];
    if(offset==0x7E838)return units[id];
    if(offset==0x6DA98)return buildings[id];
    return nullptr;
}
static bool valid(Obj obj) {++native_reads;return obj && obj[0]!=0;}
static unsigned short word(Obj obj,unsigned off) {uint16_t v;memcpy(&v,obj+off,2);return v;}
static void set_word(Obj obj,unsigned off,unsigned v) {uint16_t value=static_cast<uint16_t>(v);memcpy(obj+off,&value,2);}
#ifndef NO_RUNTIME_LOG
static unsigned apply_logs=0,reject_logs=0,load_logs=0,last_applied=0,last_failed=0;
static void fixture_log(const char *kind,unsigned a=0,unsigned b=0,unsigned=0) {
    if(!strcmp(kind,"terrain_edit_apply"))++apply_logs;
    if(!strcmp(kind,"terrain_edit_reject"))++reject_logs;
    if(!strcmp(kind,"terrain_edit_load")) {++load_logs;last_applied=a;last_failed=b;}
}
#define log_event(...) fixture_log(__VA_ARGS__)
#else
#define log_event(...) ((void)0)
#endif
// User 2026-10-29 R2: observe actual apply/restore/rollback UI goal delivery.
#define SAN14_TERRAIN_RENDER_INCLUDED 1
#define SPLIT_BASE
namespace terrain_render {
struct Mark {unsigned hex,terrain,original;};
static std::vector<Mark> marks;
static bool resync,install_needed;
static unsigned boundaries;
static void mark(unsigned h,unsigned t,unsigned o){marks.push_back({h,t,o});}
static void before_world_change(){++boundaries;}
static bool take_ui_work(bool &install){install=install_needed;install_needed=false;bool result=resync;resync=false;return result;}
}
static unsigned present_installs;
static DWORD present_install_thread;
namespace unified_overlay {static void install(){++present_installs;present_install_thread=GetCurrentThreadId();}}
static unsigned cost_cache_clears=0;
static void terrain_edit_fixture_cost_cache_clear() {++cost_cache_clears;}
#define TERRAIN_EDIT_FIXTURE_COST_CACHE 1
#include "../src/native_terrain_edit.inl"
#include "../src/native_terrain_edit_save.inl"
static bool merit_sidecar_path(const merit::SaveCoordinator::Completed &job,wchar_t *out,size_t capacity,bool) {
    ++io_paths;wchar_t cwd[MAX_PATH],hash[65];
    if(!GetCurrentDirectoryW(MAX_PATH,cwd))return false;
    for(unsigned i=0;i<32;++i)swprintf_s(hash+i*2,3,L"%02x",job.binding[i]);
    return swprintf_s(out,capacity,L"%s\\terrain_fixture.%s.meritbond",cwd,hash)>=0;
}
static void write_bytes(const wchar_t *path,const std::vector<uint8_t> &bytes) {
    HANDLE f=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    require(f!=INVALID_HANDLE_VALUE,"fixture file open");DWORD wrote=0;
    bool ok=WriteFile(f,bytes.data(),static_cast<DWORD>(bytes.size()),&wrote,nullptr)!=0 && wrote==bytes.size();CloseHandle(f);
    require(ok,"fixture file write");
}
int main() {
    using namespace terrain_edit;
    for(unsigned old=0;old<256;++old)for(unsigned next=0;next<256;++next) {
        bool a=(old>=1 && old<=14) || old==19 || old==20 || old==21 || old==24;
        bool b=(next>=1 && next<=14) || next==19 || next==20 || next==21 || next==24;
        Result expected=!a || !b?Result::Terrain:old==next?Result::Unchanged:Result::Applied;
        require(validate(true,0,old,next,false)==expected,"all terrain pairs");
    }
    require(validate(false,0,1,2,false)==Result::Disabled,"disabled");
    require(validate(true,48399,1,2,false)==Result::Applied,"last hex");
    require(validate(true,48400,1,2,false)==Result::HexRange,"sentinel");
    require(validate(true,~0u,1,2,false)==Result::HexRange,"negative hex cast");
    require(validate(true,0,1,~0u,false)==Result::Terrain,"wide terrain");
    require(validate(true,0,1,2,true)==Result::Occupied,"occupied");
    require(validate(true,0,1,1,true)==Result::Unchanged,"same occupied no-op");
    Changes list;require(list.edit(true,0,1,2,false)==Result::Applied,"insert");
    require(list.edit(true,0,2,5,false)==Result::Applied && list.size()==1 && list[0].original==1 && list[0].next==5,"preserve original");
    require(list.edit(true,0,4,1,false)==Result::Conflict && list.size()==1,"external writer conflict");
    require(list.edit(true,0,5,1,false)==Result::Applied && !list.size(),"return removes");
    for(unsigned h=0;h<MaxChanges;++h)require(list.edit(true,h,1,2,false)==Result::Applied,"capacity insert");
    require(list.edit(true,4096,1,2,false)==Result::Full,"capacity reject");
    require(list.edit(true,4095,2,3,false)==Result::Applied,"full update");
    require(list.edit(true,0,2,1,false)==Result::Applied && list.size()==4095,"full remove");
    Binding binding={};binding[0]=7;auto encoded=list.encode(binding);Changes decoded;
    require(encoded.size()==HeaderSize+4095*4 && decoded.decode(encoded.data(),encoded.size(),binding) && decoded.size()==4095,"max roundtrip");
    for(unsigned i=0;i<decoded.size();++i)require(decoded[i].hex==list[i].hex && decoded[i].next==list[i].next && decoded[i].original==list[i].original,"roundtrip entries");
    auto bad_binding=binding;bad_binding[1]=1;
    require(!decoded.decode(encoded.data(),encoded.size(),bad_binding) && decoded.size()==4095,"binding atomic reject");
    for(size_t size=0;size<HeaderSize;++size)require(!decoded.decode(encoded.data(),size,binding),"short header");
    require(!decoded.decode(nullptr,encoded.size(),binding),"null bytes");
    for(unsigned i=0;i<HeaderSize;++i) {
        auto bad=encoded;bad[i]^=0x80;
        require(!decoded.decode(bad.data(),bad.size(),binding),"corrupt header");
    }
    require(!decoded.decode(encoded.data(),encoded.size()-1,binding),"truncated item");
    auto bad=encoded;bad.push_back(0);require(!decoded.decode(bad.data(),bad.size(),binding),"trailing bytes");
    bad=encoded;bad[40]=1;bad[41]=16;require(!decoded.decode(bad.data(),bad.size(),binding),"excess count");
    bad=encoded;bad[42]=0x10;bad[43]=0xBD;require(!decoded.decode(bad.data(),bad.size(),binding),"bad item hex");
    bad=encoded;bad[44]=15;require(!decoded.decode(bad.data(),bad.size(),binding),"bad new terrain");
    bad=encoded;bad[45]=22;require(!decoded.decode(bad.data(),bad.size(),binding),"bad original terrain");
    bad=encoded;bad[44]=bad[45];require(!decoded.decode(bad.data(),bad.size(),binding),"redundant item");
    bad=encoded;bad[46]=bad[42];bad[47]=bad[43];require(!decoded.decode(bad.data(),bad.size(),binding),"duplicate item");
    Changes empty;auto zero=empty.encode(binding);require(decoded.decode(zero.data(),zero.size(),binding) && !decoded.size(),"empty roundtrip");
    for(unsigned h=0;h<8;++h) {tiles[h]=tile_data[h];tiles[h][0x10]=1;}
    // Default OFF: no world/table/native getter or sidecar path access.
    merit::SaveCoordinator::Completed job;job.name="save001.s14";job.binding=binding;
    require(apply(0,2)==Result::Disabled,"native default off");snapshot();write_sidecar(job);restore_sidecar(&job);begin_load();
    require(!world_reads && !native_reads && !io_paths,"off zero reads/io");
    mod_features.terrain_edit=true;
    // User/Claude 2026-10-29: only transient picker context survives UI callbacks.
    auto arm_context=[&]() {expanded_state=expanded_tile=expanded_menu=opening_tile=root;};
    auto context_empty=[&]() {return !expanded_state && !expanded_tile && !expanded_menu && !opening_tile;};
    arm_context();before_world_change();require(context_empty(),"empty world boundary clears picker context");
    arm_context();begin_load();require(context_empty(),"save load clears picker context");
    arm_context();restore_sidecar(nullptr);require(context_empty(),"restore clears picker context");
    arm_context();mod_features.terrain_edit=false;restore_sidecar(nullptr);
    require(context_empty(),"disabled restore also clears picker context");mod_features.terrain_edit=true;
    require(apply(48400,2)==Result::HexRange && apply(8,2)==Result::MissingTile,"native bounds/missing");
    unit_data[0]=1;set_word(unit_data,0x2A,0);units[500]=unit_data;
    require(apply(0,2)==Result::Occupied && tiles[0][0x10]==1 && !changes.size(),"unit last slot occupied");units[500]=nullptr;
    building_data[0]=1;set_word(building_data,0x10,0);buildings[3000]=building_data;
    require(apply(0,2)==Result::Occupied && tiles[0][0x10]==1,"building last slot occupied");buildings[3000]=nullptr;
    require(apply(0,2)==Result::Applied && apply(0,5)==Result::Applied && changes.size()==1 && changes[0].original==1,"native apply");
    require(terrain_render::marks.size()==2 && terrain_render::marks.back().hex==0 && terrain_render::marks.back().terrain==5 && terrain_render::marks.back().original==1,"apply mark preserves first original terrain across repeated edits");
    require(apply(0,5)==Result::Unchanged,"native same");
    require(apply(0,1)==Result::Applied && changes.size()==0 && tiles[0][0x10]==1,"native revert remove");
    require(terrain_render::marks.back().terrain==1 && terrain_render::marks.back().original==1,"return to original delivers original-pixel restore goal");
    require(apply(0,2)==Result::Applied && apply(1,3)==Result::Applied,"two changes");
    terrain_render::marks.clear();terrain_render::resync=terrain_render::install_needed=true;
    check_world();
    require(present_installs==1 && present_install_thread==GetCurrentThreadId() && terrain_render::marks.size()==2,"texture capture replays current changes and installs Present on existing UI caller");
    check_world();require(present_installs==1 && terrain_render::marks.size()==2,"unchanged UI checks neither reinstall nor replay");
    snapshot();require(write_sidecar(job),"atomic native save");
    require(save_snapshot.size()==2,"snapshot contains terrain edits only");
    wchar_t path[MAX_PATH],tmp[MAX_PATH];require(save_path(job,path,false),"terrain path");
    swprintf_s(tmp,L"%s.tmp",path);require(GetFileAttributesW(tmp)==INVALID_FILE_ATTRIBUTES,"no temporary left");
    require(wcsstr(path,L".terrain")!=nullptr,"terrain extension");
    // Preserve another writer; roll back only our own remaining value.
    terrain_render::marks.clear();unsigned previous_boundaries=terrain_render::boundaries;
    tiles[1][0x10]=4;begin_load();require(tiles[0][0x10]==1 && tiles[1][0x10]==4 && !changes.size(),"before load conditional rollback");
    require(terrain_render::marks.size()==1 && terrain_render::marks[0].hex==0 && terrain_render::marks[0].terrain==1 && terrain_render::marks[0].original==1 && terrain_render::boundaries==previous_boundaries+1,"rollback marks only restored cells and then clears world goals");
    terrain_render::marks.clear();terrain_render::resync=terrain_render::install_needed=true;
    restore_sidecar(&job);require(tiles[0][0x10]==2 && tiles[1][0x10]==4 && changes.size()==1,"restore original conflict skip");
    require(present_installs==2 && present_install_thread==GetCurrentThreadId() && terrain_render::marks.size()==2 && terrain_render::marks.back().hex==0 && terrain_render::marks.back().terrain==2 && terrain_render::marks.back().original==1,"completed save restore marks and resyncs accepted changes on UI host only");
#ifndef NO_RUNTIME_LOG
    require(last_applied==1 && last_failed==1,"restore counters");
#endif
    begin_load();tiles[1][0x10]=1;units[500]=unit_data;buildings[3000]=building_data;
    restore_sidecar(&job);require(changes.size()==2 && tiles[0][0x10]==2 && tiles[1][0x10]==3,"restore occupied persisted tiles");
    units[500]=nullptr;buildings[3000]=nullptr;
    begin_load();tiles[0][0x10]=2;restore_sidecar(&job);require(changes.size()==2,"native already new restore");
    begin_load();require(tiles[0][0x10]==1 && tiles[1][0x10]==1,"second conditional rollback");
    // The next world's same tile addresses are never changed through old records.
    require(apply(0,2)==Result::Applied,"world change setup");current_world=root+1;begin_load();
    require(tiles[0][0x10]==2 && !changes.size(),"different root no stale rollback");tiles[0][0x10]=1;current_world=root;
    // Missing sidecar, binding mismatch and malformed files never apply anything.
    Changes single;require(single.edit(true,0,1,2,false)==Result::Applied,"single save");auto saved_bytes=single.encode(binding);
    auto mismatch=single.encode(bad_binding);write_bytes(path,mismatch);restore_sidecar(&job);
    require(!changes.size() && tiles[0][0x10]==1,"native binding reject");
    saved_bytes.push_back(0);write_bytes(path,saved_bytes);restore_sidecar(&job);
    require(!changes.size() && tiles[0][0x10]==1,"native size reject");
    DeleteFileW(path);restore_sidecar(&job);require(!changes.size() && tiles[0][0x10]==1,"native missing file");
    restore_sidecar(nullptr);require(!changes.size(),"unbound load");
    // Save replacement really writes empty after returning all tiles to original.
    require(apply(0,2)==Result::Applied,"empty replacement setup");snapshot();require(write_sidecar(job),"write nonempty again");
    require(apply(0,1)==Result::Applied,"empty replacement revert");snapshot();require(write_sidecar(job),"replace with empty");
    begin_load();restore_sidecar(&job);require(!changes.size() && tiles[0][0x10]==1,"empty file clears persistence");
    DeleteFileW(path);
    mod_features.terrain_edit=false;unsigned before_reads=world_reads+native_reads,before_paths=io_paths;
    require(apply(0,2)==Result::Disabled,"off again");snapshot();write_sidecar(job);restore_sidecar(&job);begin_load();
    require(world_reads+native_reads==before_reads && io_paths==before_paths,"off after load no reads/io");
    mod_features.terrain_edit=true;mod_features.convenience=false;require(apply(0,2)==Result::Disabled,"group off");mod_features.convenience=true;
    enabled=false;require(apply(0,2)==Result::Disabled,"global off");enabled=true;
#ifndef NO_RUNTIME_LOG
    require(apply_logs>0 && reject_logs>0 && load_logs>0,"diagnostic events");
    require(cost_cache_clears>0,"successful edits clear the area-pair path cost cache");
#endif
    printf("{\"terrain_edit_checks\":%u,\"failed\":0,\"runtime_logs\":%s}\n",checks,
#ifdef NO_RUNTIME_LOG
        "false"
#else
        "true"
#endif
    );return 0;
}
