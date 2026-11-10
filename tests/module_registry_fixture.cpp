// W4-0/4-1 proof (docs/MODULARIZATION_PLAN.md): the generated activation table keeps the historical split_activate order
// (tests/data/feature_order_7b343e3.txt, extracted from main 7b343e3 before the refactor), and the generated off-closure turns off
// exactly the flags the former hand-written re-AND lines turned off, for every single flag and every pair of flags.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <set>
#include <fstream>
#include "../src/core_module.h"
#define MODULE_TABLE_NAMES_ONLY
#include "../src/module_table_convenience.inl"
#include "../src/module_flags.h"
struct ModFeatures {
#define X(n) bool n=true;
    MODULE_FLAG_LIST(X)
#undef X
};
static unsigned closure_logs;
#define log_event(...) do {++closure_logs;} while(0)
#include "../src/module_closure.inl"
static unsigned checks;
static void check(bool value,const char *label) {++checks;if(!value){fprintf(stderr,"FAIL: %s\n",label);exit(1);}}
// Frozen copy of the pre-W4 end of split_feature_guards (main 7b343e3): the only hand-written dependency closure.
static void legacy_closure(ModFeatures &f) {
    f.domestic_free=f.domestic && f.domestic_free;f.domestic_direct=f.domestic && f.domestic_direct;
    f.awards_ai=f.awards && f.awards_ai;f.stats200_growth=f.stats200 && f.stats200_growth;
    // local17 (2026-11-10): the new child keys follow their parents (development/module_registry.json requires).
    f.awards_horse=f.awards_ai && f.awards_horse;f.awards_v16=f.awards_ai && f.awards_v16;f.merit_bond16=f.merit && f.merit_bond16;
    f.briefing16=f.advisor_briefing && f.briefing16;f.expert_ingame=f.battle_rules && f.expert_ingame;
    f.drag_learn_multi=f.drag_learn && f.drag_learn_multi;f.sprite_fallback=f.ccz_sprites && f.sprite_fallback;
}
static std::set<std::string> off_set(const ModFeatures &f) {
    std::set<std::string> out;
    for(const auto &flag:kFeatureFlags)if(!(f.*flag.member))out.insert(flag.name);
    return out;
}
int main(int argc,char **argv) {
    if(argc<2) {fprintf(stderr,"usage: module_registry_fixture <feature_order_baseline.txt>\n");return 2;}
    std::ifstream in(argv[1]);std::string line;std::vector<std::string> baseline;
    while(std::getline(in,line)) {
        while(!line.empty() && (line.back()=='\r' || line.back()==' '))line.pop_back();
        if(!line.empty() && line[0]!='#')baseline.push_back(line);
    }
    const size_t count=sizeof(kModuleNames)/sizeof(kModuleNames[0]);
    check(count==baseline.size(),"table row count equals the baseline order");
    unsigned table_hash=kModuleHashSeed,baseline_hash=kModuleHashSeed;
    for(size_t i=0;i<count;++i) {
        check(baseline[i]==kModuleNames[i],("order row "+std::to_string(i)+" "+baseline[i]).c_str());
        table_hash=module_name_hash(kModuleNames[i],table_hash);baseline_hash=module_name_hash(baseline[i].c_str(),baseline_hash);
    }
    check(table_hash==baseline_hash,"feature_order hash identical to the pre-W4 order");
    // 4-1: off-closure of every single flag and every pair, old versus new.
    const size_t flags=sizeof(kFeatureFlags)/sizeof(kFeatureFlags[0]);
    unsigned compared=0,differing_children=0;
    for(size_t a=0;a<flags;++a)for(size_t b=a;b<flags;++b) {
        ModFeatures old_state,new_state;
        old_state.*kFeatureFlags[a].member=false;old_state.*kFeatureFlags[b].member=false;
        new_state=old_state;
        legacy_closure(old_state);feature_off_closure(new_state);
        ++compared;
        if(off_set(old_state)!=off_set(new_state))++differing_children;
        check(off_set(old_state)==off_set(new_state),(std::string("closure differs for ")+kFeatureFlags[a].name+"+"+kFeatureFlags[b].name).c_str());
    }
    // A chain through two levels still reaches the end (no such chain exists today; the loop must not depend on table order).
    check(closure_logs>0,"closure logged the dependent switch-offs");
    printf("{\"status\":\"module_registry_fixture_passed\",\"checks\":%u,\"rows\":%zu,\"order_hash\":\"%08x\",\"flags\":%zu,"
           "\"closure_cases\":%u,\"closure_differences\":%u,\"gameplay_verified\":false}\n",checks,count,table_hash,flags,compared,differing_children);
    return 0;
}
