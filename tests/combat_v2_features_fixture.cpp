#include "../src/combat_v2/legacy_features.h"
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
using namespace combat_v2;
static unsigned checks;
static void check(bool b,const char *s) {++checks;if(!b){std::fprintf(stderr,"%s\n",s);std::exit(1);}}
#include "combat_v2_movement_model.h"
struct Features final:FeaturePort {
    FeatureSnapshot s;
    std::vector<FacilityOffer> offers;
    FeatureOrder last;
    unsigned cut_calls=0,publishes=0,facility_calls=0;
    bool cut_success=false,wrong_cut=false,reject=false;
    Hex refused_hex=no_hex;
    MapPort *map=nullptr;
    bool construction_started=false,construction_finished=false;
    unsigned completion_day=0;
    Features() {
        s.actor={1,10001,1,100,2};s.revision=10;s.day=10;s.automatic=s.hostile_goal=true;
        s.player_force=1;s.strategic_city=3;s.hex=tile(44,45);s.city_hex=tile(62,45);
        s.region=1;s.region_center=tile(44,45);s.money=1000;
        s.ordinary_region=s.region_friendly=true;
        s.enemies={{{1,20001,3,200,1},neighbor(s.hex,1),2000,true},
                   {{1,20002,4,201,4},neighbor(s.hex,2),2000,true}};
        for(unsigned facility : {13u,5u,4u,6u,7u,3u,11u,12u,14u})
            offers.push_back({facility,100,true,{{neighbor(s.hex,3),true,false,true}}});
    }
    bool field(const Identity &actor,FeatureSnapshot &out) override {
        out=s;
        if(map) {
            const auto it=std::find_if(map->frame.members.begin(),map->frame.members.begin()+map->frame.count,
                [&](const MemberState &m){return m.army==actor.object;});
            if(it==map->frame.members.begin()+map->frame.count)return false;
            out.actor=actor;out.day=map->frame.day;out.revision=map->frame.revision;out.hex=it->hex;
            out.build_opportunity=map->owned && !construction_started && actor.id==1;
            if(construction_started && !construction_finished && actor.id==1) {
                out.active_facility=6;out.build_hex=it->hex;out.build_valid=true;
                if(map->frame.day>=static_cast<int>(completion_day)) {construction_finished=true;out.build_completed=true;}
            }
            out.enemies.clear();
        }
        return true;
    }
    bool evaluate_cut(const FeatureSnapshot &snapshot,const std::vector<Enemy> &filtered,CutPlan &out) override {
        ++cut_calls;check(!filtered.empty(),"native cut evaluated once with nonempty candidate list");
        for(const auto &e:filtered)check(e.identity.force==snapshot.player_force,"AI victims filtered before original native predicate");
        if(!cut_success)return false;
        out.victim=wrong_cut?s.enemies[1]:filtered[0];
        out.native={1,9,tile(45,45),{tile(45,45),no_hex,no_hex,no_hex,no_hex}};return true;
    }
    std::vector<FacilityOffer> facilities(const FeatureSnapshot &) override {++facility_calls;return offers;}
    CityTarget city(unsigned id) const noexcept override {return {id,30000+id,tile(62,45)};}
    bool city_hostile(const FeatureSnapshot &,CityTarget c) const noexcept override {
        return c.city>=1 && c.city<=51 && c.object==30000+c.city && c.hex<no_hex;
    }
    bool publish_feature(const FeatureSnapshot &snapshot,const FeatureOrder &order) override {
        if(reject || (std::get_if<TileTarget>(&order.target) && std::get<TileTarget>(order.target).hex==refused_hex))return false;
        check(order.strategic_city==snapshot.strategic_city,"shared final city is never replaced by temporary enemy ID");
        if(order.assignment==Assignment::combat || order.assignment==Assignment::city_assault) {
            if(auto a=std::get_if<ArmyTarget>(&order.target))
                check(order.native.action==7 && order.native.kind==27 && order.native.goal==a->identity.id,"Army target uses corrected native action and KIND");
            else if(auto c=std::get_if<CityTarget>(&order.target))
                check(order.native.action==4 && order.native.kind==5 && order.native.goal==c->city,"City target uses city native action and KIND");
            else check(false,"combat has a strongly typed target");
        }
        if(order.siege_locked)check(order.assignment!=Assignment::capture && order.assignment!=Assignment::supply_cut,
            "locked siege cannot accept logistics or capture-chain travel");
        if(order.assignment==Assignment::construction)check(order.native.action==8 && order.native.kind==9 &&
            order.facility!=9 && order.facility!=10,"construction retains native tile action without wall building");
        last=order;++publishes;
        if(map) {
            // Model the future binding's atomic group pause + feature order.
            map->orders.clear();
            if(order.assignment==Assignment::construction && !construction_started) {
                construction_started=true;completion_day=map->frame.day+2;
            }
        }
        return true;
    }
    void hit(Target source) {s.damage={true,s.actor,s.day,source};}
};
static void group_detour() {
    MapPort map(7);Features f;f.map=&map;
    std::array<Identity,max_units> identities{};
    for(unsigned i=0;i<map.army.count;++i)identities[i]={1,map.army.members[i].army,i+1,map.army.members[i].officer,2};
    SiegeController controller(map.army,identities);
    check(!controller.may_assign(Assignment::other_move) && !controller.may_assign(Assignment::supply_cut) &&
        !controller.may_assign(Assignment::capture) && controller.may_assign(Assignment::construction),"group gate applies to all AI assignment sources");
    check(!controller.may_assign(Assignment::staging) && !controller.may_assign(Assignment::city_assault) &&
        controller.may_assign(Assignment::staging,AssignmentSource::siege_controller),
        "original AI cannot bypass fixed mission by labelling unrelated travel as staging or city attack");
    for(int day=1;day<100;++day) {
        map.frame.day=day;
        for(unsigned k=0;k<20;++k) {
            auto r=controller.tick(map,f);
            check(r!=MoveStatus::invalid_snapshot && r!=MoveStatus::released && r!=MoveStatus::native_refusal,"construction detour preserves registered siege ownership");
            map.advance();
            if(r==MoveStatus::finished)break;
        }
        if(controller.movement().stage==TravelStage::native_combat)break;
    }
    check(f.construction_started && f.construction_finished && !f.cut_calls,"siege construction accepted and original cut routine excluded");
    check(controller.movement().stage==TravelStage::native_combat && map.reason==ReleaseReason::city_assault,
        "construction resumes same base and final city, with assembly before formation travel");
    check(!controller.may_assign(Assignment::other_move),"mission lock survives entry into native city combat");
    map.frame.members[0].army=99999;
    check(controller.tick(map,f)==MoveStatus::released && !controller.may_assign(Assignment::construction) &&
        !controller.movement().active,"vanished registered army releases cohort ownership without leaving stale lock");
}
int main() {
    Features f;f.cut_success=true;f.s.build_opportunity=true;f.s.regions={{2,tile(50,45),true,true,false}};
    FieldPolicy p(f.s.actor,3,false);
    check(p.tick(f)==FeatureStatus::issued && f.last.assignment==Assignment::supply_cut && f.cut_calls==1 && !f.facility_calls,
        "player cut takes priority over construction and capture");
    const auto route=f.last.native.relays;f.cut_success=false;
    check(p.tick(f)==FeatureStatus::issued && f.cut_calls==2 && f.last.native.relays==route,
        "same-day cut mission restores original native route when fresh native test declines");
    f.s.day++;check(p.tick(f)==FeatureStatus::issued && f.last.assignment==Assignment::construction,
        "old cut expiry does not suppress later construction");
    Features wrong;wrong.cut_success=wrong.wrong_cut=true;FieldPolicy wp(wrong.s.actor,3,false);
    check(wp.tick(wrong)==FeatureStatus::native_refusal && !wrong.publishes,"native returned AI victim cannot escape player filter");
    Features observer;observer.s.player_force=0;FieldPolicy op(observer.s.actor,3,false);op.tick(observer);
    check(!observer.cut_calls,"observer/no-player context has no fabricated player cut");
    Features ordinary;ordinary.s.strategic_city=0;ordinary.s.hostile_goal=false;ordinary.s.city_hex=no_hex;ordinary.cut_success=true;
    FieldPolicy ordinary_policy(ordinary.s.actor,0,false);
    check(ordinary_policy.tick(ordinary)==FeatureStatus::issued && ordinary.last.assignment==Assignment::supply_cut,
        "original player-cut feature still operates on noncampaign computer armies");
    Features siege;siege.cut_success=true;siege.s.regions={{2,tile(50,45),true,true,false}};FieldPolicy sp(siege.s.actor,3,true);
    check(sp.tick(siege)==FeatureStatus::idle && !siege.cut_calls && !siege.publishes,
        "siege goes straight to its fixed approach and never captures another base");
    for(unsigned rank=0;rank<9;++rank) {
        constexpr unsigned order[]={6,4,7,3,5,11,12,14,13};
        Features build;build.s.build_opportunity=true;
        build.offers.erase(std::remove_if(build.offers.begin(),build.offers.end(),[&](const FacilityOffer &o){
            return std::find(order,order+rank,o.facility)!=order+rank;}),build.offers.end());
        FieldPolicy bp(build.s.actor,3,true);
        check(bp.tick(build)==FeatureStatus::issued && build.last.facility==order[rank],"facility priority includes watchtower ahead of all traps");
    }
    Features sites;sites.s.build_opportunity=true;
    for(auto &o:sites.offers)if(o.facility==6){o.sites[0].reserved=true;o.sites.push_back({no_hex,true,false,true});}
    FieldPolicy site_policy(sites.s.actor,3,false);site_policy.tick(sites);
    check(sites.last.facility==4,"native illegal or occupied high-priority construction site falls through");
    sites.s.area_full=true;sites.publishes=0;site_policy.tick(sites);
    check(sites.last.assignment==Assignment::city_assault,"native area facility cap remains enforced");
    Features completed;completed.s.build_opportunity=true;completed.s.active_facility=6;completed.s.build_completed=true;
    FieldPolicy cp(completed.s.actor,3,true);cp.tick(completed);
    check(completed.last.facility==4,"completed facility advances cycle without rebuilding same type");
    Features ongoing;ongoing.s.active_facility=7;ongoing.s.build_hex=neighbor(ongoing.s.hex,3);ongoing.s.build_valid=true;
    FieldPolicy ongoing_policy(ongoing.s.actor,3,true);ongoing_policy.tick(ongoing);
    check(ongoing.last.facility==7,"unfinished original construction is not replaced by a higher-priority new build");
    Features capture;capture.s.regions={{2,tile(50,45),true,true,false},{3,tile(51,45),true,true,false}};
    FieldPolicy capture_policy(capture.s.actor,3,false);capture_policy.tick(capture);
    check(capture.last.assignment==Assignment::capture && std::get<TileTarget>(capture.last.target).hex==tile(50,45),"ordinary hostile center selected with original score");
    capture.s.regions.push_back({4,tile(45,45),true,true,false});capture_policy.tick(capture);
    check(std::get<TileTarget>(capture.last.target).hex==tile(50,45),"accepted capture stays fixed even when a closer candidate appears mid-route");
    capture.s.regions.pop_back();
    capture.s.regions[0].hostile=false;capture.s.region=2;capture.s.region_center=tile(50,45);capture.s.hex=tile(50,45);
    capture_policy.tick(capture);
    check(capture.last.assignment==Assignment::capture && std::get<TileTarget>(capture.last.target).hex==tile(51,45),"completed capture immediately continues to next hostile base");
    Features rejected;rejected.s.regions=capture.s.regions;rejected.s.regions[0].hostile=true;rejected.refused_hex=tile(50,45);
    FieldPolicy rp(rejected.s.actor,3,false);rp.tick(rejected);
    check(std::get<TileTarget>(rejected.last.target).hex==tile(51,45),"native unreachable capture is skipped without safe-base prerequisite");
    Features hit;hit.cut_success=true;hit.hit(ArmyTarget{hit.s.enemies[0].identity});FieldPolicy hp(hit.s.actor,3,true);
    check(hp.tick(hit)==FeatureStatus::issued && hit.last.assignment==Assignment::combat && !hit.cut_calls,
        "actual hit defense outranks feature work while preserving siege city");
    hit.s.enemies[0].identity.commander++;hp.tick(hit);
    check(hit.publishes==1,"recycled enemy object cannot continue old combat command");
    Features castle;castle.hit(castle.city(4));FieldPolicy castle_policy(castle.s.actor,3,true);
    // Place the actual source city in its attack range.
    auto source=castle.city(4);source.hex=neighbor(castle.s.hex,1);castle.hit(source);
    castle_policy.tick(castle);
    check(castle.last.assignment==Assignment::combat && std::get<CityTarget>(castle.last.target).city==4 &&
        castle.last.strategic_city==3,"city damage source uses a typed temporary city without replacing final objective");
    Features forecast;forecast.hit(ArmyTarget{forecast.s.enemies[0].identity});forecast.s.damage.actual_damage=false;
    FieldPolicy fp(forecast.s.actor,3,true);check(fp.tick(forecast)==FeatureStatus::idle,"damage forecast is not an actual-hit event");
    Features stale;stale.hit(ArmyTarget{stale.s.enemies[0].identity});stale.s.damage.day-=11;
    FieldPolicy stale_policy(stale.s.actor,3,true);check(stale_policy.tick(stale)==FeatureStatus::idle,"old damage events expire at original ten-day boundary");
    Features atom;atom.s.build_opportunity=true;atom.reject=true;FieldPolicy ap(atom.s.actor,3,true);
    check(ap.tick(atom)==FeatureStatus::native_refusal && !atom.publishes,"failed native command publication cannot commit feature state");
    atom.reject=false;check(ap.tick(atom)==FeatureStatus::issued && atom.last.facility==6,"retry preserves original highest construction choice");
    group_detour();
    std::printf("{\"status\":\"combat_v2_features_fixture_passed\",\"checks\":%u,\"game_connected\":false,\"gameplay_verified\":false}\n",checks);
}
