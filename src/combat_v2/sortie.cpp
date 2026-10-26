#include "sortie.h"
#include <algorithm>

namespace combat_v2 {
bool same_context(const Context &a, const Context &b) noexcept {
    return a.world == b.world && a.revision == b.revision && a.source == b.source &&
           a.district == b.district && a.target == b.target && a.automatic == b.automatic;
}
static bool valid_context(const Context &c) noexcept {
    return c.world && c.automatic && c.source && c.source <= 61 &&
           c.district && c.district <= 51 && c.target && c.target <= 61 && c.source != c.target;
}
static std::uint32_t unit_limit(const Resources &r) noexcept {
    // Native dispatch creates a temporary group per unit plus the final group.
    return (std::min)({static_cast<std::uint32_t>(max_units), r.command_points,
                      r.free_armies, r.free_groups ? r.free_groups - 1 : 0});
}
static bool valid_unit(const UnitPlan &u, const Context &c) noexcept {
    return u.officer() && u.officer() <= 6000 && u.minimum &&
           u.minimum <= u.soldiers() && u.soldiers() <= u.capacity && u.capacity <= 65535 &&
           u.command[3] && u.command[3] <= 20 && u.command[4] && u.command[4] <= 20 &&
           u.command[15] == c.target;
}
bool valid_plan(const Plan &p) noexcept {
    if(!valid_context(p.context) || p.count < 2 ||
       p.count > unit_limit(p.resources) || p.count > max_units || p.leader != p.units[0].officer())return false;
    std::uint64_t soldiers = 0, money = 0;
    for(std::uint32_t i = 0; i < p.count; ++i) {
        const auto &u = p.units[i];
        if(!valid_unit(u, p.context))return false;
        for(std::uint32_t j = 0; j < i; ++j)if(p.units[j].officer() == u.officer())return false;
        soldiers += u.soldiers(); money += u.money_cost;
    }
    return soldiers >= minimum_sortie_soldiers && soldiers == p.soldiers && money == p.money_cost &&
           soldiers <= p.resources.available_soldiers && money <= p.resources.money;
}
PlanResult plan_sortie(const Context &c, const Resources &r, const Candidate *input, std::size_t count) noexcept {
    PlanResult result;
    if(!valid_context(c))return result;
    result.status = PlanStatus::insufficient_candidates;
    if(!input || count < 2)return result;
    // Native officer table bounds the supplied array; reject unreasonable input.
    if(count > 6000)return result;
    result.status = PlanStatus::insufficient_resources;
    auto &p = result.plan; p.context = c; p.resources = r;
    const auto limit = unit_limit(r);
    if(limit < 2)return result;
    const auto budget = r.available_soldiers;
    if(budget < minimum_sortie_soldiers) {
        p = {}; result.status = PlanStatus::below_army_minimum; return result;
    }
    std::uint32_t minima = 0, money = 0;
    // Preserve the first feasible native priority. A high-priority singleton
    // must not hide a legal pair of later candidates under the same budget.
    for(std::size_t start = 0; start + 1 < count; ++start) {
        p.count = 0; minima = money = 0;
        for(std::size_t i = start; i < count && p.count < limit; ++i) {
            const auto &candidate = input[i];
            if(!candidate.native_eligible || !same_context(c, candidate.context))continue;
            UnitPlan u{candidate.command, (std::min)(candidate.capacity, candidate.command[2]),
                       candidate.minimum, candidate.money_cost};
            u.command[2] = u.capacity;
            // Validate the capped original proposal before allocating a smaller amount.
            if(!valid_unit(u, c))continue;
            bool duplicate = false;
            for(std::uint32_t j = 0; j < p.count; ++j)if(p.units[j].officer() == u.officer())duplicate = true;
            if(duplicate || u.minimum > budget - minima || u.money_cost > r.money - money)continue;
            u.command[2] = u.minimum;
            p.units[p.count++] = u; minima += u.minimum; money += u.money_cost;
        }
        std::uint32_t capacity = 0;
        for(std::uint32_t i = 0; i < p.count; ++i)capacity += p.units[i].capacity;
        if(p.count >= 2 && capacity >= minimum_sortie_soldiers)break;
    }
    if(p.count < 2) {p = {}; return result;}
    // Fair water filling over native capacity caps. Arithmetic is bounded by
    // seven 16-bit armies. No fixed support or siege demotion.
    std::uint32_t remaining = budget - minima;
    while(remaining) {
        std::uint32_t open = 0;
        for(std::uint32_t i = 0; i < p.count; ++i)if(p.units[i].soldiers() < p.units[i].capacity)++open;
        if(!open)break;
        const auto share = (std::max)(1u, remaining / open);
        for(std::uint32_t i = 0; i < p.count && remaining; ++i) {
            auto &u = p.units[i];
            const auto grant = (std::min)({share, remaining, u.capacity - u.soldiers()});
            u.command[2] += grant; remaining -= grant;
        }
    }
    p.soldiers = 0;
    for(std::uint32_t i = 0; i < p.count; ++i)p.soldiers += p.units[i].soldiers();
    p.money_cost = money; p.leader = p.units[0].officer();
    if(p.soldiers < minimum_sortie_soldiers) {
        p = {}; result.status = PlanStatus::below_army_minimum; return result;
    }
    result.status = valid_plan(p) ? PlanStatus::ready : PlanStatus::invalid_resources;
    return result;
}

DispatchResult SortieTransaction::execute(const Plan &p, NativePort &port) noexcept {
    DispatchResult result;
    if(used_) {result.status = DispatchStatus::already_used; return result;}
    used_ = true;
    if(!valid_plan(p))return result;
    if(!port.begin(p)) {result.status = DispatchStatus::stale_or_native_refusal; return result;}
    std::array<Spawned, max_units> created{};
    std::size_t count = 0; Handle group = 0;
    auto fail = [&](DispatchStatus status) noexcept {
        port.rollback(group, created.data(), count); port.finish(false);
        result.status = status; return result;
    };
    for(std::uint32_t i = 0; i < p.count; ++i) {
        auto &out = created[count];
        const bool dispatched = port.spawn(p.units[i], out);
        // Include partial creation in rollback even if dispatch returned false.
        if(out.army || out.temporary_group)++count;
        if(!dispatched || !out.army || out.officer != p.units[i].officer() ||
           out.soldiers != p.units[i].soldiers())return fail(DispatchStatus::spawn_failed);
        for(std::size_t j = 0; j + 1 < count; ++j)
            if(created[j].army == out.army || created[j].officer == out.officer)
                return fail(DispatchStatus::spawn_failed);
    }
    if(!port.form(p, created.data(), count, group) || !group)return fail(DispatchStatus::formation_failed);
    if(!port.verify(p, created.data(), count, group))return fail(DispatchStatus::verification_failed);
    ArmyGroup army; army.context = p.context; army.group = group; army.members = created;
    army.count = p.count; army.leader = p.leader; army.stage = TravelStage::to_approach;
    if(!port.publish(army))return fail(DispatchStatus::publication_failed);
    port.finish(true); result.status = DispatchStatus::committed; result.army = army; return result;
}
} // namespace combat_v2
