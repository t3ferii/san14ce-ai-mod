#include "legacy_features.h"
#include <limits>

namespace combat_v2 {
bool same_identity(const Identity &a,const Identity &b) noexcept {
    return a.world==b.world && a.object==b.object && a.id==b.id && a.commander==b.commander && a.force==b.force;
}
namespace {
bool live(const Enemy &e,Handle world) noexcept {
    return e.identity.world==world && e.identity.object && e.identity.id && e.identity.id<=500 &&
        e.identity.commander && e.identity.force && e.soldiers && e.hex<no_hex;
}
bool nearby_enemy(const FeatureSnapshot &s,const ArmyTarget &target) noexcept {
    for(const auto &e:s.enemies)if(live(e,s.actor.world) && same_identity(e.identity,target.identity) &&
        e.hostile && hex_distance(s.hex,e.hex)<=6)return true;
    return false;
}
NativeCommand command(Target t) {
    // Exact fixed native pair from the corrected legacy code. In particular,
    // Army is action 7/kind 27, NEVER city action 4/kind 5.
    if(auto city=std::get_if<CityTarget>(&t))return {4,5,city->city,{no_hex,no_hex,no_hex,no_hex,no_hex}};
    if(auto army=std::get_if<ArmyTarget>(&t))return {7,27,army->identity.id,{no_hex,no_hex,no_hex,no_hex,no_hex}};
    if(auto tile=std::get_if<TileTarget>(&t))return {1,9,tile->hex,{no_hex,no_hex,no_hex,no_hex,no_hex}};
    return {};
}
}
bool FieldPolicy::may_assign(Assignment a,AssignmentSource source) const noexcept {
    if(!siege_)return true;
    if(a==Assignment::construction || a==Assignment::combat)return true;
    return source==AssignmentSource::siege_controller &&
        (a==Assignment::staging || a==Assignment::city_assault || a==Assignment::hold);
}
FeatureStatus FieldPolicy::tick(FeaturePort &port) noexcept {
    try {
        FeatureSnapshot s;
        if(!port.field(actor_,s))return FeatureStatus::released;
        if(!actor_.world || !actor_.object || !actor_.id || actor_.id>500 || !actor_.commander ||
            !actor_.force || actor_.force>51 || !same_identity(actor_,s.actor) || !s.automatic || s.original_stop ||
            s.hex>=no_hex)return FeatureStatus::released;
        if(siege_ && (!s.hostile_goal || s.strategic_city!=city_ || s.city_hex>=no_hex))return FeatureStatus::released;
        // Commit policy state only after atomic native command acceptance.
        FieldPolicy next=*this;
        // The original player-cut feature also applies to noncampaign AI
        // armies. Only dedicated siege owns an immutable strategic city.
        if(!siege_)next.city_=s.strategic_city;
        const auto result=next.update(port,s);
        if(result!=FeatureStatus::native_refusal)*this=std::move(next);
        return result;
    } catch(...) {return FeatureStatus::native_refusal;}
}
FeatureStatus FieldPolicy::update(FeaturePort &port,const FeatureSnapshot &s) {
    auto issue=[&](FeatureOrder order) {
        if(!may_assign(order.assignment,AssignmentSource::owned_feature))return FeatureStatus::native_refusal;
        order.strategic_city=city_;order.siege_locked=siege_;
        if(!port.publish_feature(s,order))return FeatureStatus::native_refusal;
        return FeatureStatus::issued;
    };
    // Transfer of campaign_individual_priority's actual-hit freshness and
    // nearest-live-hostile-within-six rule. No five-turn retreat/morale/guard.
    const auto &hit=s.damage;
    if(hit.actual_damage && same_identity(hit.victim,s.actor) && hit.day>damage_day_ &&
        s.day>=hit.day && static_cast<std::int64_t>(s.day)-hit.day<=10) {
        damage_day_=hit.day;
        if(const auto city=std::get_if<CityTarget>(&hit.source);city && port.city_hostile(s,*city))combat_=*city;
        else if(auto source=std::get_if<ArmyTarget>(&hit.source);source && nearby_enemy(s,*source)) {
            unsigned nearest=7;
            for(const auto &e:s.enemies)if(live(e,s.actor.world) && e.hostile) {
                const auto range=hex_distance(s.hex,e.hex);
                if(range<nearest) {nearest=range;combat_=ArmyTarget{e.identity};}
            }
        }
    }
    bool valid_combat=false;
    if(auto enemy=std::get_if<ArmyTarget>(&combat_))valid_combat=nearby_enemy(s,*enemy);
    if(auto city=std::get_if<CityTarget>(&combat_))valid_combat=port.city_hostile(s,*city) &&
        city->hex<no_hex && hex_distance(s.hex,city->hex)<=6;
    if(valid_combat)return issue({Assignment::combat,combat_,command(combat_)});
    if(!std::holds_alternative<std::monostate>(combat_))combat_={};

    // Original try_cut/cut_player_predicate/remember_cut/restore_cut. Filter
    // before native selection, call once, keep its original route unchanged.
    if(!siege_ && s.player_force && s.player_force!=s.actor.force) {
        std::vector<Enemy> filtered;
        for(const auto &e:s.enemies)if(live(e,s.actor.world) && e.identity.force==s.player_force && e.hostile)filtered.push_back(e);
        CutPlan plan;
        if(!filtered.empty() && port.evaluate_cut(s,filtered,plan)) {
            bool found=false;
            for(const auto &e:filtered)if(same_identity(e.identity,plan.victim.identity))found=true;
            if(!found)return FeatureStatus::native_refusal;
            cut_day_=s.day;cut_={Assignment::supply_cut,ArmyTarget{plan.victim.identity},plan.native,0,plan.victim.identity};
            return issue(cut_);
        }
        if(cut_day_==s.day)for(const auto &e:filtered)if(same_identity(e.identity,cut_.cut_victim))return issue(cut_);
    }

    // Existing continue_build/try_build/local_facilities, with watchtower (5)
    // inserted at the user-requested position. No safe-route or threat gate.
    if(cycle_region_!=s.region) {cycle_region_=s.region;cycle_mask_=0;}
    if(s.active_facility) {
        if(s.build_completed) {
            if(s.active_facility<31)cycle_mask_|=1u<<s.active_facility;
            if(s.active_facility>=11 && s.active_facility<=14)cycle_mask_|=15u<<11;
        } else if(s.build_valid && s.build_hex<no_hex && s.active_facility!=9 && s.active_facility!=10) {
            FeatureOrder order{Assignment::construction,TileTarget{s.build_hex},command(TileTarget{s.build_hex}),s.active_facility};
            order.native.action=8;return issue(order);
        }
    }
    if(s.build_opportunity && s.ordinary_region && s.region_friendly && !s.area_full &&
        s.region_center<no_hex && hex_distance(s.hex,s.region_center)<11) {
        unsigned seen=cycle_mask_|s.local_facilities;if(seen&(15u<<11))seen|=15u<<11;
        const auto offers=port.facilities(s);
        constexpr unsigned order[]={6,4,7,3,5,11,12,14,13};
        for(unsigned facility:order) {
            if(seen&(1u<<facility))continue;
            for(const auto &offer:offers) {
                if(offer.facility!=facility || !offer.definition_valid || offer.cost>s.money)continue;
                for(const auto &site:offer.sites) {
                    if(site.hex>=no_hex || !site.friendly || site.reserved || !site.native_legal)continue;
                    FeatureOrder build{Assignment::construction,TileTarget{site.hex},command(TileTarget{site.hex}),facility};
                    build.native.action=8;return issue(build);
                }
            }
        }
    }
    // Dedicated siege never enters cut, capture-chain or an alternative move.
    // Its only strategic travel is supplied by MovementController.
    if(siege_)return FeatureStatus::idle;
    if(!city_ || !s.hostile_goal || s.city_hex>=no_hex)return FeatureStatus::idle;
    // Transfer next_region's ordinary hostile center and scored nearby
    // candidates. The graph corridor/guard/safe assignment branch is OMITTED.
    std::vector<RegionOffer> regions=s.regions;
    if(s.ordinary_region && s.region_hostile && s.region_center<no_hex)
        regions.insert(regions.begin(),{s.region,s.region_center,true,true,false});
    // Keep the previously accepted hostile center until it is actually owned
    // or its original physical admission fails. Do not pick a new closest
    // region every callback while the original capture is still in progress.
    if(capture_region_) {
        for(const auto &r:regions)if(r.region==capture_region_ && r.center==capture_hex_ && r.ordinary && r.hostile) {
            if(issue({Assignment::capture,TileTarget{r.center},command(TileTarget{r.center})})==FeatureStatus::issued)
                return FeatureStatus::issued;
            break;
        }
        capture_region_=0;capture_hex_=no_hex;
    }
    std::stable_sort(regions.begin(),regions.end(),[&](const RegionOffer &a,const RegionOffer &b) {
        auto score=[&](const RegionOffer &r)->std::uint64_t {
            if(r.region==s.region && s.region_hostile)return 0;
            return 4ull*hex_distance(s.hex,r.center)+hex_distance(s.city_hex,r.center)+(r.reserved?200u:0u)+1;
        };
        return score(a)<score(b);
    });
    for(const auto &r:regions) {
        if(!r.ordinary || !r.hostile || !r.region || r.center>=no_hex || hex_distance(s.city_hex,r.center)>15)continue;
        const auto result=issue({Assignment::capture,TileTarget{r.center},command(TileTarget{r.center})});
        // Native physical admission may reject a site. Try the next one, as
        // the original capture candidate loop did, with no safety predicate.
        if(result==FeatureStatus::issued) {capture_region_=r.region;capture_hex_=r.center;return result;}
    }
    const auto goal=port.city(city_);
    if(goal.city==city_ && port.city_hostile(s,goal))return issue({Assignment::city_assault,goal,command(goal)});
    return FeatureStatus::idle;
}
SiegeController::SiegeController(const ArmyGroup &a,const std::array<Identity,max_units> &identities):army_(a),movement_(a) {
    valid_=a.context.automatic && a.count>=2 && a.count<=max_units;
    for(unsigned i=0;valid_ && i<a.count;++i) {
        const auto &id=identities[i];
        valid_=id.world==a.context.world && id.object==a.members[i].army && id.commander==a.members[i].officer &&
            id.id && id.id<=500 && id.force && id.force<=51 && id.force==identities[0].force;
        for(unsigned j=0;j<i;++j)if(id.id==identities[j].id)valid_=false;
        if(valid_)policies_.emplace_back(id,a.context.target,true);
    }
}
bool SiegeController::may_assign(Assignment a,AssignmentSource source) const noexcept {
    return valid_ && active_ && !policies_.empty() && policies_.front().may_assign(a,source);
}
MoveStatus SiegeController::tick(MovementPort &movement,FeaturePort &features) noexcept {
    if(!valid_ || !active_)return MoveStatus::released;
    bool work=false;
    for(auto &policy:policies_) {
        const auto result=policy.tick(features);
        if(result==FeatureStatus::released) {
            movement.release(army_,ReleaseReason::original_stop);movement_.relinquish();active_=false;return MoveStatus::released;
        }
        if(result==FeatureStatus::native_refusal)return MoveStatus::native_refusal;
        work=work || result==FeatureStatus::issued || result==FeatureStatus::working;
        if(work)detour_=true;
    }
    if(work) {detour_=true;return MoveStatus::waiting;}
    if(detour_) {movement_.resume_after_detour();detour_=false;}
    return movement_.tick(movement);
}
} // namespace combat_v2
