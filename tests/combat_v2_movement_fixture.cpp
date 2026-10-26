#include "../src/combat_v2/movement.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
using namespace combat_v2;
static unsigned checks;
static void check(bool b,const char *s) {++checks;if(!b){std::fprintf(stderr,"%s\n",s);std::exit(1);}}
#include "combat_v2_movement_model.h"
static void journey(unsigned count,bool narrow,bool friendly) {
    MapPort p(count);p.narrow=narrow;p.owned=friendly;p.delay=true;
    MovementController c(p.army);
    const auto initial=c.tick(p); if(initial!=MoveStatus::issued)std::fprintf(stderr,"initial count=%u narrow=%d status=%u\n",count,narrow,static_cast<unsigned>(initial));
    check(initial==MoveStatus::issued,"initial native speed approach routes issued");
    check(c.info().approach_region==1 && p.last.common_base==0,"actual region graph used and original speed retained");
    bool saw_cohesive=false,saw_column=false,saw_assembly=false;
    for(int day=1;day<300 && c.info().active;++day) {
        p.frame.day=day;
        for(unsigned callback=0;callback<20 && c.info().active;++callback) {
            const auto result=c.tick(p);
            check(result!=MoveStatus::invalid_snapshot && result!=MoveStatus::native_refusal && result!=MoveStatus::released,"journey keeps valid original mission");
            saw_cohesive=saw_cohesive || c.info().mode==MoveMode::cohesive;
            saw_column=saw_column || c.info().mode==MoveMode::column;
            saw_assembly=saw_assembly || c.info().stage==TravelStage::assemble;
            p.advance();
        }
    }
    if(c.info().active) {
        std::fprintf(stderr,"stalled count=%u narrow=%d stage=%u mode=%u\n",count,narrow,
            static_cast<unsigned>(c.info().stage),static_cast<unsigned>(c.info().mode));
        for(unsigned i=0;i<count;++i)std::fprintf(stderr,"%u: %u,%u pending=%zu\n",i,p.frame.members[i].hex%220,p.frame.members[i].hex/220,p.orders[p.frame.members[i].army].size());
    }
    check(!c.info().active && p.reason==ReleaseReason::city_assault && p.releases==1,"all members reach city approach then restore original city assault");
    check(saw_assembly && (narrow?saw_column:saw_cohesive),"formation mode includes real assembly and narrow column fallback");
}
int main() {
    for(Hex h : {tile(40,45),tile(41,45),0u,48399u})for(unsigned d=0;d<6;++d) {
        auto next=neighbor(h,d);if(next<no_hex)check(hex_distance(h,next)==1,"native parity neighbors have unit hex distance");
    }
    journey(2,false,false);journey(7,false,true);journey(7,true,false);
    MapPort fail;fail.fail=true;MovementController fc(fail.army);
    check(fc.tick(fail)==MoveStatus::native_refusal && !fc.info().approach_region && fail.orders.empty(),"atomic native refusal does not commit controller plan");
    fail.fail=false;check(fc.tick(fail)==MoveStatus::issued,"refused plan can be retried");
    for(unsigned condition=0;condition<5;++condition) {
        MapPort p;MovementController c(p.army);check(c.tick(p)==MoveStatus::issued,"guard initial plan");
        if(condition==0)p.frame.members[1].original_stop=true;
        if(condition==1)p.frame.members[0].officer++;
        if(condition==2)p.frame.hostile_city=false;
        if(condition==3)p.frame.mission_unchanged=false;
        if(condition==4)p.frame.world++;
        check(c.tick(p)==MoveStatus::released && !c.info().active && p.reason!=ReleaseReason::city_assault,"stop ownership and identities release without forced attack");
    }
    MapPort losses;MovementController lc(losses.army);check(lc.tick(losses)==MoveStatus::issued,"loss test initial plan");
    for(unsigned i=0;i<losses.frame.count;++i)losses.frame.members[i].soldiers=100;
    check(lc.tick(losses)==MoveStatus::waiting && lc.info().active,"twenty thousand threshold is not reapplied after departure");
    MapPort wall;wall.narrow=wall.gate=true;MovementController wc(wall.army);
    check(wc.tick(wall)==MoveStatus::issued,"native gate test approach");
    bool refused=false;for(int day=1;day<30;++day){wall.frame.day=day;for(unsigned k=0;k<10;++k){auto r=wc.tick(wall);refused=refused || r==MoveStatus::no_route;wall.advance();}}
    check(refused && wall.reason!=ReleaseReason::city_assault,"member-specific blocked gate cannot be bypassed by leader route");
    std::printf("{\"status\":\"combat_v2_movement_fixture_passed\",\"checks\":%u,\"game_connected\":false,\"gameplay_verified\":false}\n",checks);
}
