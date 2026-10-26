#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// New combat code. No legacy combat includes, DLL entry points, hooks or UI.
// Native candidates and resources must be collected on the original AI caller
// thread by a future binding. Planning itself performs no game operations.
namespace combat_v2 {
constexpr std::size_t max_units = 7;
constexpr std::uint32_t minimum_sortie_soldiers = 20000;
using Handle = std::uintptr_t;
using Command = std::array<std::uint32_t, 30>;

struct Context {
    Handle world = 0;
    std::uint64_t revision = 0; // binding's candidate/resource snapshot generation
    std::uint32_t source = 0, district = 0, target = 0;
    bool automatic = false;
};
bool same_context(const Context &a, const Context &b) noexcept;

struct Resources {
    std::uint32_t available_soldiers = 0, money = 0;
    std::uint32_t command_points = 0, free_armies = 0, free_groups = 0;
};

struct Candidate {
    Context context;
    Command command{}; // private copy of the original native command
    std::uint32_t capacity = 0;
    std::uint32_t minimum = 0; // native legal floor; no old role-specific floors
    std::uint32_t money_cost = 0; // original land + water formation costs
    bool native_eligible = false;
};

struct UnitPlan {
    Command command{};
    std::uint32_t capacity = 0, minimum = 0, money_cost = 0;
    std::uint32_t officer() const noexcept { return command[0]; }
    std::uint32_t soldiers() const noexcept { return command[2]; }
};

struct Plan {
    Context context;
    Resources resources;
    std::array<UnitPlan, max_units> units{};
    std::uint32_t count = 0, soldiers = 0, money_cost = 0;
    std::uint32_t leader = 0; // first accepted native candidate, not a new rank rule
};

enum class PlanStatus {
    ready, invalid_context, invalid_resources, insufficient_candidates,
    insufficient_resources, below_army_minimum
};
struct PlanResult { PlanStatus status = PlanStatus::invalid_context; Plan plan; };

// Keep native candidate priority. Capacity is also capped at the native
// command's proposed soldiers. This module never increases that proposal or
// changes formation/skills/target. Reject duplicate officer records.
PlanResult plan_sortie(const Context &, const Resources &,
                      const Candidate *, std::size_t count) noexcept;
bool valid_plan(const Plan &) noexcept;

struct Spawned {
    Handle army = 0, temporary_group = 0;
    std::uint32_t officer = 0, soldiers = 0;
};
enum class TravelStage { to_approach, assemble, cohesive_approach, native_combat };
struct ArmyGroup {
    Context context;
    Handle group = 0;
    std::array<Spawned, max_units> members{};
    std::uint32_t count = 0, leader = 0;
    TravelStage stage = TravelStage::to_approach;
};

// Explicit boundary for a future original-game binding. NOT connected here.
// All methods execute synchronously on the existing AI caller thread.
// begin revalidates world/candidate identities, available soldiers, money, command
// points, native eligibility, free objects and the unchanged native commands.
// It reserves the transaction context only; it must not spend game resources.
// A false begin must release any partial admission context itself.
// spawn uses original dispatch with normal resource charging. If it partially
// creates anything, it MUST return those handles in out even when returning
// false. form may also return a group handle on failure.
// verify checks actual native membership, officer/army identity and resources.
// publish registers the complete group with the original AI parent. It must
// undo any partial result-list/power bookkeeping when returning false.
// rollback follows CURRENT ownership (grouping may have partially moved units),
// invokes native cancellation/refunds exactly once per owned object, and never
// cancels an unrelated group. This library never adds soldiers/money manually.
// finish releases transaction context. Successful finish may dispose only
// verified empty temporary groups. All callbacks are noexcept and mandatory.
class NativePort {
public:
    virtual ~NativePort() = default;
    virtual bool begin(const Plan &) noexcept = 0;
    virtual bool spawn(const UnitPlan &, Spawned &out) noexcept = 0;
    virtual bool form(const Plan &, const Spawned *, std::size_t, Handle &out) noexcept = 0;
    virtual bool verify(const Plan &, const Spawned *, std::size_t, Handle) noexcept = 0;
    virtual bool publish(const ArmyGroup &) noexcept = 0;
    virtual void rollback(Handle, const Spawned *, std::size_t) noexcept = 0;
    virtual void finish(bool committed) noexcept = 0;
};

enum class DispatchStatus {
    committed, invalid_plan, stale_or_native_refusal, spawn_failed,
    formation_failed, verification_failed, publication_failed, already_used
};
struct DispatchResult { DispatchStatus status = DispatchStatus::invalid_plan; ArmyGroup army; };

// One transaction object per attempt. Repeated calls cannot dispatch twice.
// A successful result owns the actual group/member identities needed by the
// future AI movement controller, independently of the player movement UI.
class SortieTransaction {
    bool used_ = false;
public:
    DispatchResult execute(const Plan &, NativePort &) noexcept;
};
} // namespace combat_v2
