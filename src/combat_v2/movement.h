#pragma once
#include "sortie.h"
#include "hex.h"
#include <vector>

namespace combat_v2 {
enum class MoveMode { individual, assemble, cohesive, column };
enum class MoveStatus { issued, waiting, finished, released, no_route, native_refusal, invalid_snapshot };
enum class ReleaseReason { city_assault, original_stop, identity_changed };
struct MemberState {
    Handle army = 0;
    unsigned officer = 0, soldiers = 0;
    Hex hex = no_hex;
    unsigned base_movement = 0;
    bool original_stop = false; // retreat/combat/native stop must take precedence
};
struct FieldSnapshot {
    Handle world = 0, group = 0;
    unsigned target = 0, city_region = 0;
    Hex city_hex = no_hex;
    std::uint64_t revision = 0;
    int day = 0;
    bool hostile_city = false, automatic = false, mission_unchanged = false;
    std::array<MemberState,max_units> members{};
    unsigned count = 0;
    // Original game's neutral movement scale, supplied by the future binding.
    // Equal cost and minimum base are used only after assembly. No reference
    // build's unknown G/P addresses or formula are copied into this library.
    double points_per_base_day = 0;
    unsigned flat_cost = 0;
};
struct Approach {
    unsigned region = 0;
    Hex center = no_hex;
    bool ordinary_base = false;
};
struct MoveOrder {
    Handle army = 0;
    std::vector<Hex> path; // full individual route or ONE admitted cohesive edge
    unsigned capture_region = 0;
};
struct MoveBatch {
    MoveMode mode = MoveMode::individual;
    unsigned common_base = 0, common_cost = 0;
    std::vector<MoveOrder> orders;
};

// Unconnected original-AI boundary. None of these callbacks are installed.
// cell_open/edge_open validate original passability, zero-speed terrain,
// native gates/water restrictions and FOREIGN occupied hexes for that member.
// Cohort occupancy is checked by the controller scheduler. Native execution
// must also preserve real collision/stop rules; no teleport or wall bypass.
// travel_cost is positive original member-specific travel time (roads included).
// region_adjacent MUST read the actual Area+56/+66 graph, not a radius guess.
// publish rechecks snapshot revision/identities, saves all affected commands,
// validates generated native paths, and applies all orders + movement policy
// atomically, or restores everything on failure. In cohesive/column mode the
// binding must normalize actual movement to common_base/common_cost; this
// library's credit scheduler alone is not a replacement for that binding.
// release restores the original strategic city mission for city_assault only;
// other reasons release ownership without overwriting a retreat/combat order.
class MovementPort {
public:
    virtual ~MovementPort() = default;
    virtual bool snapshot(const ArmyGroup &, FieldSnapshot &) noexcept = 0;
    virtual std::vector<Approach> approaches(const FieldSnapshot &) = 0;
    virtual unsigned region_of(Hex) const noexcept = 0;
    virtual bool region_adjacent(unsigned city, unsigned base) const noexcept = 0;
    virtual bool owned_by_group(unsigned region) const noexcept = 0;
    virtual bool cell_open(unsigned member, Hex) const noexcept = 0;
    virtual bool edge_open(unsigned member, Hex from, Hex to) const noexcept = 0;
    virtual unsigned travel_cost(unsigned member, Hex from, Hex to) const noexcept = 0;
    virtual bool publish(const FieldSnapshot &, const MoveBatch &) noexcept = 0;
    virtual void release(const ArmyGroup &, ReleaseReason) noexcept = 0;
};

struct MovementInfo {
    TravelStage stage = TravelStage::to_approach;
    MoveMode mode = MoveMode::individual;
    unsigned approach_region = 0;
    Hex approach_hex = no_hex, rally_center = no_hex;
    bool active = false;
};
class MovementController {
    struct State {
        MovementInfo info;
        unsigned facing = 0;
        int day = 0;
        bool planned = false, pending = false;
        double credit = 0;
        std::array<double,max_units> member_credit{};
        std::array<Hex,max_units> endpoints{}, pending_hex{};
        std::array<std::vector<Hex>,max_units> routes;
        std::array<std::size_t,max_units> cursor{};
        std::vector<Hex> centers;
        std::size_t center_index = 0;
    } state_;
    ArmyGroup army_;
    MoveStatus update(MovementPort &,const FieldSnapshot &,State &,MoveBatch &);
public:
    explicit MovementController(const ArmyGroup &) noexcept;
    void resume_after_detour() noexcept;
    void relinquish() noexcept {state_.info.active=false;}
    MoveStatus tick(MovementPort &) noexcept;
    MovementInfo info() const noexcept {return state_.info;}
};
} // namespace combat_v2
