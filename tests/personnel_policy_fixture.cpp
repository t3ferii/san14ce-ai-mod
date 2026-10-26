#include "../src/personnel_policy.h"
#include <stdio.h>
#include <stdlib.h>
using namespace personnel;
static unsigned checks;
static void check(bool condition,const char *description) {
    ++checks;if(!condition) {fprintf(stderr,"FAILED: %s\n",description);exit(1);}
}
int main() {
    RecruitmentFacts hated={true,true,false,false,false};
    check(recruitment_blocked(recruitment_reason(hated)),"target hates recruiting ruler");
    RecruitmentFacts spouse={true,false,true,false,false};
    check(recruitment_reason(spouse)==ForeignSpouse,"foreign spouse prevents generic envoy");
    RecruitmentFacts sibling={true,false,false,true,false};
    check(recruitment_reason(sibling)==ForeignSwornSibling,"foreign sworn sibling prevents generic envoy");
    RecruitmentFacts all={true,true,true,true,false};
    check(recruitment_reason(all)==7,"multiple rejection reasons are retained for screen");
    all.envoy_is_spouse_or_sworn_sibling=true;
    check(recruitment_reason(all)==FamilyEnvoyException && !recruitment_blocked(recruitment_reason(all)),"family envoy overrides hard conditions");
    all.context_known=false;
    check(recruitment_reason(all)==MissingContext && !recruitment_blocked(recruitment_reason(all)),"unknown context preserves native judgement");
    check(may_auto_work(0),"healthy officer allowed");
    for(unsigned degree=1;degree<=3;++degree)check(!may_auto_work(degree),"light moderate severe injury excluded");
    Candidate a[]={{11,true,true,0},{12,true,true,1},{13,false,true,0}};
    Candidate b[]={{11,true,true,0},{21,true,true,1},{12,false,true,0}};
    Candidate c[]={{11,true,true,0},{21,true,true,1},{31,false,true,0}};
    unsigned assigned[3]={};
    assigned[0]=pick_unique_envoy(a,3,assigned,0);
    assigned[1]=pick_unique_envoy(b,3,assigned,1);
    assigned[2]=pick_unique_envoy(c,3,assigned,2);
    check(assigned[0]==11 && assigned[1]==21 && assigned[2]==31,"multi-target selection reserves envoy before next recommendation");
    check(pick_unique_envoy(a,3,assigned,3)==12,"second recommended envoy preferred over nonrecommended native first rank");
    Candidate none[]={{11,true,false,0},{21,true,true,0}};
    check(!pick_unique_envoy(none,2,assigned,3),"no available unused envoy leaves target unassigned");
    check(!pick_unique_envoy(nullptr,0,assigned,3),"empty recommendations do not fabricate envoy");
    Officer people[]={{2,1,{90,30,40,50},true},{3,1,{80,95,35,50},true},
                      {4,1,{70,50,99,90},true},{5,2,{100,100,100,100},true}};
    Item items[]={{1,1,0,ItemClass::Leadership,10,true},{2,1,0,ItemClass::Leadership,5,true},
                  {3,1,0,ItemClass::War,10,true},{4,1,0,ItemClass::Intelligence,10,true},
                  {5,1,0,ItemClass::Horse,1,true},{6,1,0,ItemClass::Politics,10,true}};
    auto give=[&](unsigned i) {unsigned p=best_recipient(items[i],people,4,items,6);items[i].owner=p;items[i].in_storage=false;return p;};
    check(give(0)==2,"leadership item assigned to best leadership own-force officer");
    check(give(1)==3,"next leadership item assigned to next officer instead of duplicate");
    check(give(2)==3,"war item assigned by war independent of leadership possession");
    check(give(3)==4,"intelligence item assigned by intelligence");
    check(give(4)==2,"horse priority follows leadership");
    check(give(5)==4,"politics priority follows politics");
    Item replacement={7,1,0,ItemClass::War,15,true};
    check(displaced_by(items[2],replacement,3),"same category goes to storage before new award");
    check(!displaced_by(items[1],replacement,3),"different ability item preserved on replacement");
    Item trinket={8,1,3,ItemClass::LoyaltyOnly,0,false};
    Item trinket2={9,1,0,ItemClass::LoyaltyOnly,0,true};
    check(!displaced_by(trinket,trinket2,3),"loyalty-only valuables remain stackable");
    check(!best_recipient(trinket2,people,4,items,6),"ability allocator does not invent loyalty-item priority");
    printf("{\"status\":\"personnel_rules_fixture_passed\",\"checks\":%u,\"native_hooks_installed\":false,\"gameplay_verified\":false}\n",checks);
}
