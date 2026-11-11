// W4-2..4-6 proof (docs/MODULARIZATION_PLAN.md): the services keep the historical call orders whatever order the modules register in.
// The expected sequences are frozen from main 7b343e3 (hard-coded calls before W4):
//   map-tick        fast_menu busy slot: lord_change::map_tick
//   domestic-tick   native_domestic.inl domestic_tick: pre awards, injury, domestic, officer_rotation (local57, appended last); post domestic
//   overlay draw    native_unified_overlay.inl draw_locked: terrain_render, ccz_sprites, ccz_preview, then briefing_overlay
//   overlay gate    draw(): briefing_overlay::needs_draw, terrain_render::needs_present, ccz_sprites::needs_present, ccz_preview::needs_draw
// The (service, owner, order, gate order) rows come from the generated registry table, so a changed constant fails here.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
using Obj=unsigned char *;
#include "../src/core_module.h"
#include "../src/core_services.h"
#define MODULE_TABLE_NAMES_ONLY
#include "../src/module_table_convenience.inl"
static unsigned checks;
static void check(bool value,const std::string &label) {++checks;if(!value){fprintf(stderr,"FAIL: %s\n",label.c_str());exit(1);}}
static std::vector<std::string> calls;
template<unsigned I> static void note() {calls.push_back(std::to_string(I));}
template<unsigned I> static void note_city(Obj) {calls.push_back(std::to_string(I));}
template<unsigned I> static void note_state(Obj) {calls.push_back(std::to_string(I));}
static bool yes() {return true;}
// Registers the rows of one service in the given permutation and returns the resulting owners in call order.
template<class S,class Make> static std::vector<std::string> registered(const char *service,S &list,std::vector<unsigned> permutation,Make make) {
    std::vector<const ModuleSubscription *> rows;
    for(const auto &s:kSubscriptions)if(!strcmp(s.service,service))rows.push_back(&s);
    list.clear();
    for(unsigned index:permutation)check(make(list,*rows[index]),"capacity");
    std::vector<std::string> out;
    for(unsigned i=0;i<list.count;++i)out.push_back(list.rows[i].owner);
    return out;
}
static unsigned rows_of(const char *service) {unsigned n=0;for(const auto &s:kSubscriptions)if(!strcmp(s.service,service))++n;return n;}
template<class S,class Make> static void all_permutations(const char *service,S &list,const std::vector<std::string> &expected,Make make,const char *what) {
    const unsigned n=rows_of(service);
    check(n==expected.size(),std::string(what)+": subscriber count");
    std::vector<unsigned> permutation(n);for(unsigned i=0;i<n;++i)permutation[i]=i;
    unsigned cases=0;
    do {
        check(registered(service,list,permutation,make)==expected,std::string(what)+": order for permutation "+std::to_string(cases));
        ++cases;
    } while(std::next_permutation(permutation.begin(),permutation.end()));
}
int main() {
    // map-tick
    all_permutations("map_tick",services::map_tick,{"lord_change"},[](auto &l,const ModuleSubscription &s){return l.add(s.owner,&note<1>,yes,s.order);},"map-tick order");
    // appear-prefetch (W6-F): face_keep is the only subscriber; the former appear_batch::update called face_keep::prefetch directly
    all_permutations("appear_prefetch",services::appear_prefetch,{"face_keep"},[](auto &l,const ModuleSubscription &s){return l.add(s.owner,&note_state<1>,yes,s.order);},"appear-prefetch order");
    {
        static unsigned prefetched;static bool on;
        services::appear_prefetch.clear();
        check(services::appear_prefetch.add("face_keep",[](Obj){++prefetched;},[]{return on;},10),"appear-prefetch add");
        services::run_appear_prefetch(nullptr);check(prefetched==0,"appear-prefetch inactive predicate: no call");
        on=true;services::run_appear_prefetch(nullptr);check(prefetched==1,"appear-prefetch live predicate: one call per frame");
        services::reset_all();check(services::appear_prefetch.count==0,"reset_all clears appear-prefetch");
    }
    // domestic-tick: pre order awards, injury, domestic, officer_rotation
    all_permutations("domestic_tick",services::domestic_tick,{"awards","injury","domestic","officer_rotation"},
        [](auto &l,const ModuleSubscription &s){return l.add(s.owner,services::DomesticTick{&note_city<1>,nullptr},yes,s.order);},"domestic-tick order");
    // overlay draw order
    all_permutations("overlay",services::overlay,{"terrain_render","ccz_sprites","ccz_preview","advisor_briefing"},
        [](auto &l,const ModuleSubscription &s){return l.add(s.owner,services::OverlaySub{0,s.gate_order,nullptr,nullptr,nullptr,nullptr},yes,s.order);},"overlay draw order");
    // overlay entry gate order (probe order of the old short-circuit chain), independent of the registration order
    {
        std::vector<std::pair<unsigned,std::string>> gate;
        for(const auto &s:kSubscriptions)if(!strcmp(s.service,"overlay"))gate.push_back({s.gate_order,s.owner});
        std::sort(gate.begin(),gate.end());
        std::vector<std::string> owners;for(auto &g:gate)owners.push_back(g.second);
        check(owners==std::vector<std::string>({"advisor_briefing","terrain_render","ccz_sprites","ccz_preview"}),"overlay gate order");
    }
    // ui-dialogs: the CCZ editor is the only user
    all_permutations("ui_dialogs",services::ui_dialogs,{"ccz_sprites"},[](auto &l,const ModuleSubscription &s){return l.add(s.owner,nullptr,yes,s.order);},"ui-dialogs users");
    // equal orders keep registration order; any() follows the live predicate; overflow is refused
    {
        services::Subscribers<services::MapTick,2> list;
        static bool on=false;
        check(list.add("a",&note<1>,[]{return on;},5) && list.add("b",&note<2>,[]{return on;},5),"two entries fit");
        check(!list.add("c",&note<3>,nullptr,1),"capacity refused");
        check(std::string(list.rows[0].owner)=="a" && std::string(list.rows[1].owner)=="b","equal order keeps registration order");
        check(!list.any(),"inactive predicate");on=true;check(list.any(),"active predicate");
        list.clear();check(list.count==0 && !list.any(),"clear");
    }
    // the hash printed by svc_install matches the owner order
    unsigned expected_hash=kModuleHashSeed;
    for(const char *owner:{"terrain_render","ccz_sprites","ccz_preview","advisor_briefing"})expected_hash=module_name_hash(owner,expected_hash);
    std::vector<unsigned> identity={0,1,2,3};
    registered("overlay",services::overlay,identity,[](auto &l,const ModuleSubscription &s){return l.add(s.owner,services::OverlaySub{0,s.gate_order,nullptr,nullptr,nullptr,nullptr},yes,s.order);});
    check(services::overlay.order_hash()==expected_hash,"overlay svc_install hash");
    printf("{\"status\":\"core_services_fixture_passed\",\"checks\":%u,\"overlay_hash\":\"%08x\",\"gameplay_verified\":false}\n",checks,expected_hash);
    return 0;
}
