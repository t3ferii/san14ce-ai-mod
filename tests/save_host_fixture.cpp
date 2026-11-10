// Save host proof (docs/MODULARIZATION_PLAN.md F-09): atomic writer with failure injection at every step, path rule, manifest
// round trip and corruption cases, manifest published last, plugin order, load classification (absent_off, absent_failed, mismatch,
// manifest_stale, old save without manifest) and old-version logging. Real files next to the fixture executable; no game, no installed DLL.
#define SAVE_HOST_FIXTURE
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include "../src/save_host.h"
#define MODULE_TABLE_NAMES_ONLY
#include "../src/module_table_convenience.inl"   // registry rows of the sidecars service (position order)
using namespace save_host;
static unsigned checks;
static void check(bool ok,const std::string &label) {++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",label.c_str());exit(1);}}
static HMODULE module() {return GetModuleHandleW(nullptr);}
static std::vector<std::string> events;
static void record(const char *kind,unsigned a,unsigned b,unsigned c) {events.push_back(std::string(kind)+":"+std::to_string(a)+","+std::to_string(b)+","+std::to_string(c));}
static bool has_event(const std::string &prefix) {for(auto &e:events)if(e.rfind(prefix,0)==0)return true;return false;}
static std::vector<uint8_t> read_file(const wchar_t *file) {
    std::vector<uint8_t> out;HANDLE h=CreateFileW(file,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return out;
    LARGE_INTEGER n={};GetFileSizeEx(h,&n);out.resize(static_cast<size_t>(n.QuadPart));DWORD got=0;ReadFile(h,out.data(),static_cast<DWORD>(out.size()),&got,nullptr);CloseHandle(h);return out;
}
static std::vector<uint8_t> bytes_of(const char *text) {return std::vector<uint8_t>(text,text+strlen(text));}
static Binding binding_of(unsigned seed) {Binding b={};for(unsigned i=0;i<32;++i)b[i]=static_cast<uint8_t>(seed*31+i);return b;}
// ---- fake plugins: each keeps a "live" string it writes and restores from its file
struct Fake {
    const char *label;std::wstring ext;std::string live;std::string restored;SidecarCache cache;
    bool active=true,throws=false,fail=false;unsigned writes=0,restores=0;
};
static Fake fakes[3];
static std::vector<std::string> calls;
template<unsigned I> static bool fake_write(const Job &job,Written &out) {
    Fake &f=fakes[I];calls.push_back(std::string("write:")+f.label);++f.writes;
    if(f.throws)throw 1;
    if(f.fail)fail_step=2;
    bool ok=store(module(),job,f.ext.c_str(),bytes_of(f.live.c_str()),f.cache,out);
    fail_step=0;return ok;
}
template<unsigned I> static bool fake_restore(const Job *job) {
    Fake &f=fakes[I];calls.push_back(std::string("restore:")+f.label);++f.restores;f.restored.clear();
    if(!job)return false;
    wchar_t file[MAX_PATH];
    if(!path(module(),*job,f.ext.c_str(),file,MAX_PATH,false))return false;
    auto bytes=read_file(file);if(bytes.empty())return false;
    std::string text(bytes.begin(),bytes.end());
    if(text.rfind("OK:",0)!=0)return false;   // the plugin's own validation
    f.restored=text;return true;
}
template<unsigned I> static bool fake_active() {return fakes[I].active;}
template<unsigned I> static void fake_snapshot() {calls.push_back(std::string("snapshot:")+fakes[I].label);}
template<unsigned I> static void fake_begin() {calls.push_back(std::string("begin:")+fakes[I].label);}
#define FAKE(I,NAME,EXT) {NAME,EXT,1,"ok_" NAME,"fail_" NAME,&fake_active<I>,&fake_snapshot<I>,&fake_write<I>,&fake_begin<I>,&fake_restore<I>}
static const SidecarPlugin plugin_a=FAKE(0,"a",L"aaa"),plugin_b=FAKE(1,"b",L"bbb"),plugin_c=FAKE(2,"c",L"ccc");
static Plugins<4> list;
static void reset_fakes() {
    for(auto &f:fakes) {f.live.clear();f.restored.clear();f.cache.clear();f.active=true;f.throws=false;f.fail=false;f.writes=0;f.restores=0;}
    fakes[0].label="a";fakes[0].ext=L"aaa";fakes[1].label="b";fakes[1].ext=L"bbb";fakes[2].label="c";fakes[2].ext=L"ccc";
    fakes[0].live="OK:a";fakes[1].live="OK:b";fakes[2].live="OK:c";
    calls.clear();events.clear();fail_step=0;
}
static void remove_all(const Job &job) {
    for(const wchar_t *ext:{L"aaa",L"bbb",L"ccc",L"sidecars"}) {wchar_t f[MAX_PATH];if(path(module(),job,ext,f,MAX_PATH,false))DeleteFileW(f);}
}
static unsigned outcome(unsigned plugin) {
    std::string prefix="sidecar_load:"+std::to_string(plugin)+",";
    for(auto &e:events)if(e.rfind(prefix,0)==0)return static_cast<unsigned>(atoi(e.c_str()+prefix.size()));
    return 99;
}
int main() {
    // sha256 known answer
    {Binding h={};check(sha256("abc",3,h) && h[0]==0xba && h[1]==0x78 && h[31]==0xad,"sha256 of abc");}
    // path rule
    {
        Job job{"slot1",binding_of(1)};wchar_t file[MAX_PATH];
        check(path(module(),job,L"tactics",file,MAX_PATH,true),"path builds");
        std::wstring text=file;check(text.find(L"\\AI_TacticsMod\\data\\merit\\slot1.")!=std::wstring::npos && text.substr(text.size()-8)==L".tactics","path ends <name>.<hex>.<ext>");
        check(text.size()>64+10 && text[text.size()-8-1-64]==L'.',"sha256 hex between name and extension");
        Job bad{"a\\b",binding_of(1)};check(!path(module(),bad,L"x",file,MAX_PATH,false),"invalid name refused");
        Job empty{"",binding_of(1)};check(!path(module(),empty,L"x",file,MAX_PATH,false),"empty name refused");
    }
    // write_atomic: failure injection at each step leaves the destination untouched and no temp file
    {
        Job job{"atomic",binding_of(2)};wchar_t file[MAX_PATH],temporary[MAX_PATH];
        check(path(module(),job,L"zzz",file,MAX_PATH,true),"atomic path");swprintf_s(temporary,L"%s.tmp",file);DeleteFileW(file);
        check(write_atomic(file,"old",3) && read_file(file)==bytes_of("old"),"plain write");
        for(unsigned step=1;step<=4;++step) {
            fail_step=step;bool ok=write_atomic(file,"new",3);fail_step=0;
            check(!ok,"step "+std::to_string(step)+" injected failure is reported");
            check(read_file(file)==bytes_of("old"),"step "+std::to_string(step)+" keeps the old destination");
            check(!exists(temporary),"step "+std::to_string(step)+" leaves no temp file");
        }
        check(write_atomic(file,"new",3) && read_file(file)==bytes_of("new"),"replace after injected failures");
        DeleteFileW(file);
    }
    // store(): cache rule and failure state
    {
        reset_fakes();Job job{"store",binding_of(3)};Written w;SidecarCache cache;
        check(store(module(),job,L"qqq",bytes_of("x"),cache,w) && w.state==kWritten && w.len==1,"store writes");
        fail_step=1;check(store(module(),job,L"qqq",bytes_of("x"),cache,w) && w.state==kWritten,"unchanged bytes already on disk are not rewritten");
        fail_step=1;check(!store(module(),job,L"qqq",bytes_of("y"),cache,w) && w.state==kFailed,"failed write reports kFailed");fail_step=0;
        check(!cache.matches(L"x",bytes_of("y")),"failed write is not remembered");
        wchar_t file[MAX_PATH];path(module(),job,L"qqq",file,MAX_PATH,false);check(read_file(file)==bytes_of("x"),"failed rewrite kept the previous sidecar");DeleteFileW(file);
    }
    // manifest round trip and corruption
    {
        Binding archive=binding_of(4);std::vector<Entry> entries(2),back;
        memset(&entries[0],0,sizeof(Entry));memset(&entries[1],0,sizeof(Entry));
        strcpy_s(entries[0].ext,"tactics");entries[0].version=2;entries[0].state=kWritten;entries[0].len=77;entries[0].hash=binding_of(9);
        strcpy_s(entries[1].ext,"terrain");entries[1].version=1;entries[1].state=kOff;
        auto bytes=encode_manifest(archive,entries);
        check(decode_manifest(bytes,archive,back) && back.size()==2 && !strcmp(back[0].ext,"tactics") && back[0].len==77 && back[0].hash==binding_of(9) && back[1].state==kOff,"manifest round trip");
        check(!decode_manifest(bytes,binding_of(5),back),"manifest of another archive is ignored");
        auto bad=bytes;bad[10]^=1;check(!decode_manifest(bad,archive,back),"crc catches a flipped bit");
        bad=bytes;bad.pop_back();check(!decode_manifest(bad,archive,back),"truncated manifest ignored");
        bad=bytes;bad.push_back(0);check(!decode_manifest(bad,archive,back),"manifest with trailing bytes ignored");
        check(!decode_manifest(std::vector<uint8_t>(),archive,back),"empty manifest ignored");
    }
    // plugin order is by position whatever the registration order
    {
        list.clear();check(list.add(&plugin_c,30) && list.add(&plugin_a,10) && list.add(&plugin_b,20),"register");
        check(list.rows[0]==&plugin_a && list.rows[1]==&plugin_b && list.rows[2]==&plugin_c,"plugins run in position order");
    }
    // the registry rows put the sidecars in the historical order for every registration order: terrain, tactic memory, merit, warehouse, auto commands (PW36e)
    {
        std::vector<const ModuleSubscription *> rows;
        for(const auto &s:kSubscriptions)if(!strcmp(s.service,"sidecars"))rows.push_back(&s);
        check(rows.size()==5,"five sidecar plugins registered by the registry");
        static SidecarPlugin dummies[5];
        for(unsigned i=0;i<5;++i) {dummies[i]=plugin_a;dummies[i].name=rows[i]->owner;}
        std::vector<unsigned> permutation={0,1,2,3,4};unsigned cases=0;
        do {
            Plugins<8> registered;
            for(unsigned index:permutation)check(registered.add(&dummies[index],rows[index]->order),"register");
            check(std::string(registered.rows[0]->name)=="terrain_edit" && std::string(registered.rows[1]->name)=="tactic_priority" &&
                  std::string(registered.rows[2]->name)=="merit" && std::string(registered.rows[3]->name)=="awards" && std::string(registered.rows[4]->name)=="auto_commands","sidecar plugin order for permutation "+std::to_string(cases));
            ++cases;
        } while(std::next_permutation(permutation.begin(),permutation.end()));
    }
    // full save, manifest last, then every load classification
    reset_fakes();
    Job job{"cycle",binding_of(6)};remove_all(job);
    snapshot_all(list);
    check(calls==std::vector<std::string>({"snapshot:a","snapshot:b","snapshot:c"}),"snapshot order");
    calls.clear();
    save_all(list,module(),job,&record);
    check(calls==std::vector<std::string>({"write:a","write:b","write:c"}),"write order");
    {
        wchar_t manifest[MAX_PATH];check(path(module(),job,L"sidecars",manifest,MAX_PATH,false) && exists(manifest),"manifest published");
        std::vector<Entry> entries;check(read_manifest(module(),job,entries) && entries.size()==3 && entries[0].state==kWritten && !strcmp(entries[2].ext,"ccc"),"manifest lists every plugin");
        check(has_event("sidecar_manifest:1,3,0") && has_event("ok_a") && has_event("ok_c"),"write events and manifest event");
    }
    // load, everything present: all restored
    calls.clear();events.clear();begin_load_all(list);restore_all(list,module(),&job,&record);
    check(outcome(0)==kRestored && outcome(1)==kRestored && outcome(2)==kRestored && fakes[1].restored=="OK:b","round trip restores every sidecar");
    check(calls[0]=="begin:a" && calls[3]=="restore:a" && calls[5]=="restore:c","begin_load then restore in order");
    // plugin b inactive at save time -> OFF, load classifies absent_off
    reset_fakes();remove_all(job);fakes[1].active=false;
    save_all(list,module(),job,&record);
    check(fakes[1].writes==0,"inactive plugin is not written");
    events.clear();restore_all(list,module(),&job,&record);
    check(outcome(1)==kAbsentOff && outcome(0)==kRestored,"absent_off for a switched-off feature");
    // plugin c write fails -> FAILED entry, manifest still written (last), load says save_failed
    reset_fakes();remove_all(job);fakes[2].fail=true;
    save_all(list,module(),job,&record);
    check(has_event("fail_c") && has_event("sidecar_manifest:1,3,0"),"failed sidecar logged and the manifest is still published last");
    events.clear();restore_all(list,module(),&job,&record);
    check(outcome(2)==kSaveFailed && outcome(0)==kRestored,"save_failed when the manifest recorded a failed write");
    // exception in a plugin does not stop the others
    reset_fakes();remove_all(job);fakes[0].throws=true;
    save_all(list,module(),job,&record);
    check(fakes[1].writes==1 && fakes[2].writes==1 && has_event("fail_a"),"throwing plugin never stops the next one");
    // absent_failed: manifest says written, file gone
    reset_fakes();remove_all(job);save_all(list,module(),job,&record);
    {wchar_t f[MAX_PATH];path(module(),job,L"bbb",f,MAX_PATH,false);DeleteFileW(f);}
    events.clear();restore_all(list,module(),&job,&record);
    check(outcome(1)==kAbsentFailed,"absent_failed when a written sidecar vanished");
    // mismatch: manifest says written, file rejected by the plugin
    {wchar_t f[MAX_PATH];path(module(),job,L"ccc",f,MAX_PATH,false);write_atomic(f,"BAD",3);}
    events.clear();restore_all(list,module(),&job,&record);
    check(outcome(2)==kMismatch && fakes[2].restored.empty(),"mismatch when the file is rejected (plugin reset)");
    // manifest_stale: valid file that differs from the manifest hash is adopted
    {wchar_t f[MAX_PATH];path(module(),job,L"aaa",f,MAX_PATH,false);write_atomic(f,"OK:a-newer",10);}
    events.clear();restore_all(list,module(),&job,&record);
    check(outcome(0)==kManifestStale && fakes[0].restored=="OK:a-newer","manifest_stale adopts the valid file");
    // old save without manifest behaves as before
    {wchar_t f[MAX_PATH];path(module(),job,L"sidecars",f,MAX_PATH,false);DeleteFileW(f);}
    events.clear();restore_all(list,module(),&job,&record);
    check(outcome(0)==kRestored && fakes[0].restored=="OK:a-newer" && has_event("sidecar_manifest_load:0,0,0"),"no manifest: every plugin loads from its own file");
    // corrupt manifest is the same as none
    {wchar_t f[MAX_PATH];path(module(),job,L"sidecars",f,MAX_PATH,true);write_atomic(f,"garbage",7);}
    events.clear();restore_all(list,module(),&job,&record);
    check(outcome(0)==kRestored && has_event("sidecar_manifest_load:0,0,0"),"corrupt manifest ignored");
    // matching job null (no save): plugins reset
    events.clear();calls.clear();restore_all(list,module(),nullptr,&record);
    check(calls==std::vector<std::string>({"restore:a","restore:b","restore:c"}),"no matching save: every plugin still resets");
    // old versions: same save name, another archive version (expected after the slot is overwritten)
    {
        Job other{"cycle",binding_of(7)};save_all(list,module(),other,&record);
        events.clear();restore_all(list,module(),&job,&record);
        check(has_event("sidecar_old_versions:"),"files of another archive version are counted as old versions (log only)");
        check(exists([&]{static wchar_t f[MAX_PATH];path(module(),other,L"aaa",f,MAX_PATH,false);return f;}()),"old-version files are never deleted");
        remove_all(other);
    }
    remove_all(job);
    // classification table (pure)
    {
        Entry written={};written.state=kWritten;written.hash=binding_of(1);Entry off={};off.state=kOff;Entry failed={};failed.state=kFailed;
        check(classify(nullptr,false,false,Binding())==kAbsentNew && classify(nullptr,true,true,Binding())==kRestored,"classify: no entry");
        check(classify(&off,false,false,Binding())==kAbsentOff && classify(&off,true,true,Binding())==kManifestStale,"classify: off");
        check(classify(&written,false,false,Binding())==kAbsentFailed && classify(&written,true,false,Binding())==kMismatch &&
              classify(&written,true,true,binding_of(1))==kRestored && classify(&written,true,true,binding_of(2))==kManifestStale,"classify: written");
        check(classify(&failed,false,false,Binding())==kSaveFailed && classify(&failed,true,true,Binding())==kManifestStale,"classify: failed");
    }
    printf("{\"status\":\"save_host_fixture_passed\",\"checks\":%u,\"gameplay_verified\":false}\n",checks);
    return 0;
}
