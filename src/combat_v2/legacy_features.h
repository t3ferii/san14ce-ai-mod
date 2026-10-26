#pragma once
#include "movement.h"
#include <variant>

namespace combat_v2 {
// Transferred feature logic from native_adapter.cpp/native_facilities.inl/
// native_campaign.inl. Game access is an explicit, currently unbound port.
struct Identity {
    Handle world=0,object=0;
    unsigned id=0,commander=0,force=0;
};
bool same_identity(const Identity &,const Identity &) noexcept;
struct Enemy {Identity identity;Hex hex=no_hex;unsigned soldiers=0;bool hostile=false;};
struct CityTarget {unsigned city=0;Handle object=0;Hex hex=no_hex;};
struct ArmyTarget {Identity identity;};
struct TileTarget {Hex hex=no_hex;};
using Target=std::variant<std::monostate,CityTarget,ArmyTarget,TileTarget>;
enum class Assignment {supply_cut,capture,construction,combat,staging,city_assault,hold,other_move};
enum class AssignmentSource {original_ai,siege_controller,owned_feature};
struct NativeCommand {
    unsigned action=0,kind=0,goal=0;
    std::array<Hex,5> relays{{no_hex,no_hex,no_hex,no_hex,no_hex}};
};
struct FeatureOrder {
    Assignment assignment=Assignment::hold;
    Target target;
    NativeCommand native;
    unsigned facility=0;
    Identity cut_victim;
    unsigned strategic_city=0; // NEVER an Army ID
    bool siege_locked=false;
};
struct CutPlan {Enemy victim;NativeCommand native;};
struct BuildSite {Hex hex=no_hex;bool friendly=false,reserved=false,native_legal=false;};
struct FacilityOffer {
    unsigned facility=0,cost=0;
    bool definition_valid=false;
    std::vector<BuildSite> sites; // B9070 order; 278FA0 eligibility
};
struct RegionOffer {
    unsigned region=0;Hex center=no_hex;
    bool ordinary=false,hostile=false,reserved=false;
};
struct DamageEvent {
    bool actual_damage=false;
    Identity victim;
    int day=0;
    Target source; // binding records actual Army OR city damage source
};
struct FeatureSnapshot {
    Identity actor;
    std::uint64_t revision=0;
    int day=0;
    bool automatic=false,original_stop=false,hostile_goal=false;
    unsigned player_force=0,strategic_city=0,region=0,money=0,local_facilities=0;
    Hex hex=no_hex,city_hex=no_hex,region_center=no_hex;
    bool ordinary_region=false,region_hostile=false,region_friendly=false,area_full=false;
    bool build_opportunity=false; // real capture credit OR original friendly offensive area
    unsigned active_facility=0;
    Hex build_hex=no_hex;
    bool build_valid=false,build_completed=false; // original costs/qualification/terrain rechecked
    DamageEvent damage;
    std::vector<Enemy> enemies;
    std::vector<RegionOffer> regions; // original city-centered radius-15 list, plus current region
};
// No safe-route/danger/guard APIs exist here. Native admission covers physical
// passability, real occupancy, costs, terrain and qualification only.
class FeaturePort {
public:
    virtual ~FeaturePort()=default;
    // False means the registered actor cannot be located/validated; release
    // custom ownership instead of keeping a dead/recycled army locked.
    virtual bool field(const Identity &,FeatureSnapshot &) =0;
    // Preserve old D06D0 + C0C80 behavior: invoke the native predicate ONCE,
    // with only these player victims. Evaluation must restore temporary owner
    // changes/commands before returning, including rejection/exception.
    virtual bool evaluate_cut(const FeatureSnapshot &,const std::vector<Enemy> &player_only,CutPlan &)=0;
    virtual std::vector<FacilityOffer> facilities(const FeatureSnapshot &)=0;
    virtual CityTarget city(unsigned id) const noexcept=0;
    virtual bool city_hostile(const FeatureSnapshot &,CityTarget) const noexcept=0;
    // Atomic command publication on the existing AI caller thread. Revalidate
    // actor/victim world/object/id/commander, revision, target KIND and all
    // native gates. Preserve the Troops strategic_city field for Army combat.
    // Use original command setters + original path generation; NO legacy
    // campaign_safe_route, castle-radius avoidance, guard/safe-base checks.
    // When siege_locked, atomically pause the cohort's travel while preserving
    // already-owned construction/combat work. Reject all unrelated movement
    // from EVERY AI assignment entry point, including native defaults.
    virtual bool publish_feature(const FeatureSnapshot &,const FeatureOrder &)=0;
};
enum class FeatureStatus {idle,issued,working,native_refusal,released};
class FieldPolicy {
    Identity actor_;
    unsigned city_;
    bool siege_;
    int damage_day_=-1,cut_day_=-1;
    unsigned cycle_region_=0,cycle_mask_=0;
    unsigned capture_region_=0;
    Hex capture_hex_=no_hex;
    Target combat_;
    FeatureOrder cut_;
    FeatureStatus update(FeaturePort &,const FeatureSnapshot &);
public:
    FieldPolicy(Identity actor,unsigned city,bool siege) noexcept:actor_(actor),city_(city),siege_(siege) {}
    bool may_assign(Assignment,AssignmentSource=AssignmentSource::original_ai) const noexcept;
    FeatureStatus tick(FeaturePort &) noexcept;
};
// Owns only a receipt from the new sortie transaction. Unit policies keep the
// permanent city and temporary combat target separate for the whole mission.
class SiegeController {
    ArmyGroup army_;
    MovementController movement_;
    std::vector<FieldPolicy> policies_;
    bool detour_=false;
    bool valid_=false,active_=true;
public:
    SiegeController(const ArmyGroup &,const std::array<Identity,max_units> &);
    bool may_assign(Assignment,AssignmentSource=AssignmentSource::original_ai) const noexcept;
    MoveStatus tick(MovementPort &,FeaturePort &) noexcept;
    MovementInfo movement() const noexcept {return movement_.info();}
};
} // namespace combat_v2
