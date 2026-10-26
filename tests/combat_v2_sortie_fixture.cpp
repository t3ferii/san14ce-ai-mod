#include "../src/combat_v2/sortie.h"
#include "../src/combat_v2/native_contract.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <vector>
using namespace combat_v2;
static unsigned checks;
static void check(bool value, const char *label) {
    ++checks; if(!value) {std::fprintf(stderr, "%s\n", label); std::exit(1);}
}
static Context context() {return {1, 42, 1, 2, 3, true};}
static Resources resources() {return {70000, 10000, 10, 20, 20};}
static std::vector<Candidate> candidates(unsigned count) {
    std::vector<Candidate> out;
    for(unsigned i = 0; i < count; ++i) {
        Candidate c; c.context = context(); c.capacity = 10000; c.minimum = 1000;
        c.money_cost = 100; c.native_eligible = true;
        for(unsigned j = 0; j < c.command.size(); ++j)c.command[j] = 1000 + j;
        c.command[0] = 100 + i; c.command[2] = 10000;
        c.command[3] = i % 3 == 0 ? 9 : 1; // native siege selection retained
        c.command[4] = 11; c.command[15] = c.context.target; out.push_back(c);
    }
    return out;
}
// Independent native resource/ownership model. Failure may occur after spend
// or after only some group memberships changed. Never reads game memory.
struct ModelPort final : NativePort {
    struct Live {UnitPlan unit; Handle group;};
    Context now = context(); Resources original = resources(), available = original;
    std::map<Handle, Live> armies;
    std::set<Handle> groups;
    std::set<std::uint32_t> busy;
    Handle next = 1000, published = 0;
    unsigned spawned = 0, fail_spawn = 0, begins = 0, rollbacks = 0, finishes = 0;
    bool stale = false, fail_form = false, fail_verify = false, fail_publish = false;
    bool wrong_officer = false, manual_thread = false;
    bool begin(const Plan &p) noexcept override {
        ++begins;
        if(manual_thread || stale || !same_context(now, p.context) || !p.context.automatic ||
           available.available_soldiers != p.resources.available_soldiers ||
           available.money != p.resources.money || available.command_points != p.resources.command_points ||
           available.free_armies != p.resources.free_armies || available.free_groups != p.resources.free_groups)return false;
        for(unsigned i = 0; i < p.count; ++i)if(busy.count(p.units[i].officer()))return false;
        return p.soldiers <= available.available_soldiers && p.money_cost <= available.money;
    }
    bool spawn(const UnitPlan &u, Spawned &out) noexcept override {
        ++spawned;
        if(u.soldiers() > available.available_soldiers || u.money_cost > available.money ||
           !available.command_points || !available.free_armies || !available.free_groups)return false;
        const Handle a = next++, g = next++;
        armies.emplace(a, Live{u, g}); groups.insert(g); busy.insert(u.officer());
        available.available_soldiers -= u.soldiers(); available.money -= u.money_cost;
        --available.command_points; --available.free_armies; --available.free_groups;
        out = {a, g, wrong_officer ? u.officer() + 1 : u.officer(), u.soldiers()};
        return fail_spawn != spawned; // failure AFTER creation/spending
    }
    bool form(const Plan &, const Spawned *list, std::size_t count, Handle &out) noexcept override {
        if(!available.free_groups)return false;
        out = next++; groups.insert(out); --available.free_groups;
        for(std::size_t i = 0; i < count; ++i) {
            armies.at(list[i].army).group = out;
            if(fail_form && i == count / 2)return false;
        }
        return true;
    }
    bool verify(const Plan &p, const Spawned *list, std::size_t count, Handle g) noexcept override {
        if(fail_verify || !groups.count(g) || count != p.count)return false;
        for(std::size_t i = 0; i < count; ++i) {
            const auto &a = armies.at(list[i].army);
            if(a.group != g || a.unit.officer() != p.units[i].officer() ||
               a.unit.soldiers() != p.units[i].soldiers())return false;
        }
        return available.available_soldiers == original.available_soldiers - p.soldiers && available.money == original.money - p.money_cost;
    }
    bool publish(const ArmyGroup &g) noexcept override {
        published = g.group;
        if(fail_publish) {published = 0; return false;} // partial parent bookkeeping undone
        return true;
    }
    void rollback(Handle final_group, const Spawned *list, std::size_t count) noexcept override {
        ++rollbacks;
        std::set<Handle> owned;
        if(final_group)owned.insert(final_group);
        for(std::size_t i = 0; i < count; ++i) {
            owned.insert(list[i].temporary_group);
            const auto it = armies.find(list[i].army);
            if(it != armies.end())owned.insert(it->second.group);
        }
        // Model native cancellation by current group ownership; a moved unit
        // is refunded once, irrespective of its old temporary-group handle.
        for(Handle g : owned) {
            for(auto it = armies.begin(); it != armies.end();) {
                if(it->second.group != g) {++it; continue;}
                available.available_soldiers += it->second.unit.soldiers();
                available.money += it->second.unit.money_cost;
                ++available.command_points; ++available.free_armies;
                busy.erase(it->second.unit.officer()); it = armies.erase(it);
            }
            if(groups.erase(g))++available.free_groups;
        }
    }
    void finish(bool committed) noexcept override {
        ++finishes;
        if(!committed)return;
        for(auto it = groups.begin(); it != groups.end();) {
            bool occupied = false;
            for(const auto &entry : armies)if(entry.second.group == *it)occupied = true;
            if(occupied)++it;
            else {it = groups.erase(it); ++available.free_groups;}
        }
    }
    bool restored() const noexcept {
        return available.available_soldiers == original.available_soldiers && available.money == original.money &&
               available.command_points == original.command_points && available.free_armies == original.free_armies &&
               available.free_groups == original.free_groups && !published && finishes == 1;
    }
};

int main() {
    auto people = candidates(12); const auto untouched = people;
    auto result = plan_sortie(context(), resources(), people.data(), people.size());
    check(result.status == PlanStatus::ready && result.plan.count == 7, "native candidates bounded at seven");
    check(result.plan.soldiers == 70000 && result.plan.leader == 100, "all available soldiers and native candidate leader preserved");
    for(unsigned i = 0; i < result.plan.count; ++i)for(unsigned j = 0; j < 30; ++j)
        if(j != 2)check(result.plan.units[i].command[j] == people[i].command[j], "formation skills and original target preserved");
    for(unsigned i = 0; i < people.size(); ++i)check(people[i].command == untouched[i].command, "input native commands unchanged");
    for(unsigned n = 2; n <= 7; ++n) {
        auto budget = resources(); budget.command_points = n;
        auto p = plan_sortie(context(), budget, people.data(), people.size());
        check(p.status == PlanStatus::ready && p.plan.count == n, "two through seven native command points accepted");
        ModelPort port; port.original = port.available = budget;
        SortieTransaction transaction; auto issued = transaction.execute(p.plan, port);
        check(issued.status == DispatchStatus::committed && issued.army.count == n, "actual model armies grouped");
        check(issued.army.group && issued.army.leader == p.plan.leader &&
              issued.army.stage == TravelStage::to_approach, "identity receipt starts at approach movement stage");
        check(port.armies.size() == n && port.groups.size() == 1 && port.published == issued.army.group,
              "temporary groups disposed only after complete publication");
        check(port.available.available_soldiers == budget.available_soldiers - p.plan.soldiers,
              "original resource accounting conserved");
        check(transaction.execute(p.plan, port).status == DispatchStatus::already_used && port.spawned == n,
              "same transaction cannot dispatch twice");
        SortieTransaction duplicate; const auto before = port.spawned;
        check(duplicate.execute(p.plan, port).status == DispatchStatus::stale_or_native_refusal && port.spawned == before,
              "fresh transaction also rejects already spent stale native plan");
    }
    for(unsigned points = 0; points < 2; ++points) {
        auto r = resources(); r.command_points = points;
        check(plan_sortie(context(), r, people.data(), people.size()).status != PlanStatus::ready,
              "one unit is not a formed army group");
    }
    for(unsigned stock : {0u,19999u,20000u,39999u,40000u}) {
        auto r = resources(); r.available_soldiers = stock;
        auto p = plan_sortie(context(), r, people.data(), people.size());
        check((p.status == PlanStatus::ready) == (stock >= 20000), "inclusive twenty thousand available threshold");
        if(stock >= 20000)check(p.plan.soldiers == stock, "no reserve deduction from available soldiers");
    }
    auto r = resources();
    r = resources(); r.money = 250;
    auto money = plan_sortie(context(), r, people.data(), people.size());
    check(money.status == PlanStatus::ready && money.plan.count == 2 && money.plan.money_cost == 200,
          "native formation money limits group before creation");
    r.free_groups = 2;
    check(plan_sortie(context(), r, people.data(), people.size()).status != PlanStatus::ready,
          "final group object reserved in addition to unit temporary groups");
    auto human = context(); human.automatic = false;
    check(plan_sortie(human, resources(), people.data(), people.size()).status == PlanStatus::invalid_context,
          "manual player departure refused");
    auto mixed = candidates(7); mixed[0].native_eligible = false; mixed[1].context.district = 3;
    mixed[2].command[15] = 5; mixed[3].command[0] = mixed[4].command[0];
    auto filtered = plan_sortie(context(), resources(), mixed.data(), mixed.size());
    check(filtered.status == PlanStatus::ready && filtered.plan.count == 3 && filtered.plan.leader == mixed[3].command[0],
          "native refusals mixed districts wrong targets and duplicate officers excluded");
    auto capped = candidates(2); capped[0].capacity = 2000; capped[1].command[2] = 3000;
    auto caps = plan_sortie(context(), resources(), capped.data(), capped.size());
    check(caps.status != PlanStatus::ready,
          "cannot form twenty thousand within native capacity and proposal bounds");
    auto costly = candidates(3); costly[0].minimum = 20000; costly[0].capacity = costly[0].command[2] = 30000;
    auto pair_budget = resources(); pair_budget.available_soldiers = 20000;
    auto pair = plan_sortie(context(), pair_budget, costly.data(), costly.size());
    check(pair.status == PlanStatus::ready && pair.plan.count == 2 && pair.plan.leader == 101,
          "unaffordable high priority singleton does not hide a later legal pair");
    costly[0].minimum = 1000; costly[0].money_cost = 250;
    pair_budget.money = 250;
    pair = plan_sortie(context(), pair_budget, costly.data(), costly.size());
    check(pair.status == PlanStatus::ready && pair.plan.count == 2 && pair.plan.leader == 101,
          "native money costs can also skip an isolated high priority candidate");

    // Exhaust all seven post-spend failure positions and partially regrouped
    // ownership. Assert resources, object slots and unrelated armies survive.
    for(unsigned failure = 1; failure <= 7; ++failure) {
        ModelPort port; port.fail_spawn = failure;
        UnitPlan unrelated; unrelated.command[0] = 999;
        port.armies.emplace(99999, ModelPort::Live{unrelated, 99998}); port.groups.insert(99998);
        SortieTransaction transaction; auto failed = transaction.execute(result.plan, port);
        check(failed.status == DispatchStatus::spawn_failed && port.restored(), "post-spend creation failure refunded exactly once");
        check(port.armies.size() == 1 && port.armies.count(99999) && port.groups.size() == 1,
              "rollback leaves unrelated native objects intact");
    }
    for(unsigned failure = 0; failure < 3; ++failure) {
        ModelPort port; port.fail_form = failure == 0; port.fail_verify = failure == 1; port.fail_publish = failure == 2;
        SortieTransaction transaction; auto failed = transaction.execute(result.plan, port);
        const DispatchStatus expected[] = {DispatchStatus::formation_failed, DispatchStatus::verification_failed,
                                           DispatchStatus::publication_failed};
        check(failed.status == expected[failure] && port.restored() && port.armies.empty() && port.groups.empty(),
              "partial grouping or registration failure restores all model resources and ownership");
    }
    ModelPort wrong; wrong.wrong_officer = true; SortieTransaction wrong_tx;
    check(wrong_tx.execute(result.plan, wrong).status == DispatchStatus::spawn_failed && wrong.restored(),
          "wrong native commander rejected and partial object retained for rollback");
    for(unsigned failure = 0; failure < 4; ++failure) {
        ModelPort port;
        if(failure == 0)port.now.revision++;
        if(failure == 1)port.available.available_soldiers--;
        if(failure == 2)port.busy.insert(100);
        if(failure == 3)port.manual_thread = true;
        SortieTransaction transaction;
        check(transaction.execute(result.plan, port).status == DispatchStatus::stale_or_native_refusal &&
              !port.spawned && !port.rollbacks, "changed snapshot or native admission refused before spending");
    }
    auto invalid = result.plan; invalid.units[0].command[2]++;
    ModelPort invalid_port; SortieTransaction invalid_tx;
    check(invalid_tx.execute(invalid, invalid_port).status == DispatchStatus::invalid_plan && !invalid_port.begins,
          "tampered plan rejected before native context admission");
    // Independent budget properties across resource boundaries.
    for(unsigned budget = 0; budget <= 50000; budget += 137) {
        auto bounded = resources(); bounded.available_soldiers = budget;
        const auto p = plan_sortie(context(), bounded, people.data(), people.size());
        if(p.status != PlanStatus::ready) {check(budget < 20000, "only budgets below twenty thousand fail"); continue;}
        std::uint64_t total = 0;
        std::set<unsigned> unique;
        for(unsigned i = 0; i < p.plan.count; ++i) {
            check(p.plan.units[i].soldiers() >= 1000 && p.plan.units[i].soldiers() <= 10000,
                  "allocation lies within independently supplied native bounds");
            total += p.plan.units[i].soldiers(); unique.insert(p.plan.units[i].officer());
        }
        check(total <= budget && total == p.plan.soldiers && unique.size() == p.plan.count && p.plan.count >= 2 && p.plan.count <= 7,
              "soldiers identities group size and available soldiers conserved across budgets");
    }
    std::printf("{\"status\":\"combat_v2_sortie_fixture_passed\",\"checks\":%u,\"game_connected\":false,\"gameplay_verified\":false}\n", checks);
}
