#include "../src/friendship_store.h"
#include "../src/friendship_save.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace friendship;
static unsigned checks;
static void expect(bool ok) {++checks;if(!ok) {std::fprintf(stderr,"Friendship check %u failed\n",checks);std::exit(1);}}
static Native read_native(unsigned owner,void *context) {
    auto *base=static_cast<std::map<unsigned,Native> *>(context);
    auto at=base->find(owner);return at==base->end()?Native{}:at->second;
}
int main() {
    Store store;Native original={2,3,4,5,6,7,8,9},empty={};
    expect(store.list(1,original).size()==8);
    for(unsigned id=10;id<=501;++id)expect(store.add(1,id,original)==AddResult::Added);
    auto all=store.list(1,original);expect(all.size()==500 && all.front()==2 && all.back()==501);
    expect(store.add(1,502,original)==AddResult::Full);
    expect(store.add(1,500,original)==AddResult::Existing);
    expect(store.add(1,2,original)==AddResult::Existing);
    expect(store.add(1,1,original)==AddResult::Invalid);
    expect(store.add(0,2,empty)==AddResult::Invalid);
    expect(store.add(1,6001,original)==AddResult::Invalid);
    expect(store.contains(1,2,original) && store.contains(1,501,original));
    expect(!store.contains(1,502,original) && !store.contains(1,1,original));
    // Reject the mutual update before adding either direction if one side
    // is full. Existing native relations still satisfy the mutual update.
    expect(!store.add_mutual(1,502,original,empty));
    expect(!store.contains(502,1,empty));
    expect(store.add_mutual(1,2,original,empty));
    expect(store.contains(2,1,empty) && store.list(1,original).size()==500);
    expect(store.remove(1,501));expect(store.add(1,502,original)==AddResult::Added);
    expect(!store.remove(1,999));
    // Strict save binding; failed imports leave existing relationships intact.
    Binding binding={};binding[0]=17;std::map<unsigned,Native> native={{1,original}};
    auto encoded=store.encode(binding);Store loaded;
    expect(loaded.decode(encoded.data(),encoded.size(),binding,&read_native,&native));
    expect(loaded.list(1,original)==store.list(1,original) && loaded.contains(2,1,empty));
    Binding wrong=binding;wrong[0]++;
    expect(!loaded.decode(encoded.data(),encoded.size(),wrong,&read_native,&native));
    expect(loaded.contains(2,1,empty));
    auto damaged=encoded;damaged[55]^=1;
    expect(!loaded.decode(damaged.data(),damaged.size(),binding,&read_native,&native));
    for(size_t size=0;size<encoded.size();++size)
        expect(!loaded.decode(encoded.data(),size,binding,&read_native,&native));
    expect(!loaded.decode(nullptr,encoded.size(),binding,&read_native,&native));
    auto trailing=encoded;trailing.push_back(0);
    expect(!loaded.decode(trailing.data(),trailing.size(),binding,&read_native,&native));
    // Malformed native duplicates do not consume extra slots or create self.
    Native duplicated={2,2,1,0,3,6001,0,0};Store clean;
    expect(clean.list(1,duplicated)==std::vector<uint16_t>({2,3}));
    expect(clean.add_mutual(1,4,duplicated,empty));
    expect(clean.list(1,duplicated).size()==3 && clean.contains(4,1,empty));
    // Exact capacity with no original friends is also 500, never 508.
    clean.clear();for(unsigned id=2;id<=501;++id)expect(clean.add(1,id,empty)==AddResult::Added);
    expect(clean.list(1,empty).size()==500 && clean.add(1,502,empty)==AddResult::Full);
    // Native dislikes and later fields are adjacent but untouched by the store.
    unsigned char person[0x200];std::memset(person,0xA5,sizeof(person));
    std::memcpy(person+0xC0,original.data(),sizeof(original));
    unsigned char before[sizeof(person)];std::memcpy(before,person,sizeof(person));
    Native live;std::memcpy(live.data(),person+0xC0,sizeof(live));Store isolated;
    for(unsigned id=10;id<=501;++id)expect(isolated.add(1,id,live)==AddResult::Added);
    expect(std::memcmp(before,person,sizeof(person))==0);
    // Native host jobs may overlap or fail. Only the exact completed job
    // may commit its own snapshot/hash; a load cancels an outstanding save.
    SaveCoordinator jobs;SaveCoordinator::Completed completed;
    const std::string name="svdexceccKO00.s14",other="svdexceccKO01.s14";
    expect(SaveCoordinator::valid_name(name));
    for(const std::string &bad:{std::string("../x.s14"),std::string("x/y.s14"),std::string("x\\y.s14"),
                               std::string(".s14"),std::string("x.s14.tmp"),std::string(61,'a')+".s14"})
        expect(!SaveCoordinator::valid_name(bad));
    uint64_t epoch=jobs.begin_save(name,isolated);
    expect(epoch && !jobs.save_token(other) && jobs.save_token(name)==epoch);
    expect(!jobs.observe_save(epoch,other,binding,true));
    expect(!jobs.observe_save(epoch,name,binding,false));
    expect(!jobs.finish_save(epoch,true,completed));
    epoch=jobs.begin_save(name,isolated);isolated.clear();
    expect(jobs.observe_save(epoch,name,binding,true));
    expect(jobs.finish_save(epoch,true,completed));
    expect(completed.store.list(1,live).size()==500 && completed.binding==binding && completed.name==name);
    expect(!jobs.finish_save(epoch,true,completed));
    epoch=jobs.begin_save(name,clean);expect(jobs.observe_save(epoch,name,binding,true));
    expect(!jobs.finish_save(epoch,false,completed));
    uint64_t stale=jobs.begin_save(name,clean),loading=jobs.begin_load(name);
    expect(!jobs.observe_save(stale,name,binding,true) && !jobs.finish_save(stale,true,completed));
    expect(!jobs.observe_load(loading,name,binding,false));
    expect(!jobs.observe_load(loading,other,binding,true));
    expect(jobs.observe_load(loading,name,binding,true));
    expect(jobs.finish_load(loading,true,completed) && completed.store.list(1,empty).empty());
    loading=jobs.begin_load(name);uint64_t newer=jobs.begin_load(other);
    expect(!jobs.observe_load(loading,name,binding,true) && !jobs.finish_load(loading,true,completed));
    expect(jobs.observe_load(newer,other,wrong,true) && !jobs.finish_load(newer,false,completed));
    epoch=jobs.begin_save(name,clean);expect(jobs.observe_save(epoch,name,binding,true));
    expect(!jobs.begin_load("bad/slot.s14") && !jobs.finish_save(epoch,true,completed));
    std::printf("{\"status\":\"friendship_500_fixture_passed\",\"checks\":%u,\"capacity\":500,\"native_save_hook_verified\":false,\"installed\":false}\n",checks);
}
