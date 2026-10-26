#pragma once
#include <algorithm>
#include <array>
#include <cwctype>
#include <set>
#include <string>
#include <vector>

namespace unified {
enum Facet { City, Ability, Formation, Tactic, Policy, Trait, Merit, Doctrine,
             Spouse, Brotherhood, Inner, Link, FacetCount };
struct Officer {
    unsigned id=0;
    std::wstring name;
    std::array<int,5> abilities{};
    unsigned merit=0;
    std::array<std::vector<std::wstring>,FacetCount> facets;
    std::array<unsigned,10> tactics{};
};
struct Criterion {
    std::vector<std::wstring> values;
    bool all=false;
};
struct Filter {
    std::wstring name;
    std::array<Criterion,FacetCount> facets;
    std::array<int,5> minimum{};
    std::array<int,5> maximum{{200,200,200,200,200}};
    unsigned merit_min=0,merit_max=65535;
};
inline std::wstring folded(std::wstring value) {
    std::transform(value.begin(),value.end(),value.begin(),
        [](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
    return value;
}
inline bool matches(const Officer &person,const Filter &filter) {
    if(folded(person.name).find(folded(filter.name))==std::wstring::npos)return false;
    for(unsigned i=0;i<5;++i)
        if(person.abilities[i]<filter.minimum[i] || person.abilities[i]>filter.maximum[i])return false;
    if(person.merit<filter.merit_min || person.merit>filter.merit_max)return false;
    for(unsigned i=0;i<FacetCount;++i) {
        const auto &rule=filter.facets[i];if(rule.values.empty())continue;
        unsigned found=0;
        for(const auto &value:rule.values)
            if(std::find(person.facets[i].begin(),person.facets[i].end(),value)!=person.facets[i].end())++found;
        if((rule.all && found!=rule.values.size()) || (!rule.all && !found))return false;
    }
    return true;
}
inline bool knows(const Officer &person,unsigned tactic) {
    return tactic && std::find(person.tactics.begin(),person.tactics.end(),tactic)!=person.tactics.end();
}
struct LearningPair {unsigned officer,book,tactic;};
struct Book {unsigned id,tactic,stock;};
struct LearningPlan {
    std::vector<LearningPair> work;
    unsigned known=0,stock_missing=0,capacity_missing=0;
};
inline LearningPlan plan_learning(const std::vector<Officer> &people,const std::vector<Book> &books) {
    LearningPlan plan;auto stock=books;std::set<unsigned> handled;
    for(const auto &person:people) {
        if(!handled.insert(person.id).second)continue;
        auto learned=person.tactics;
        for(auto &book:stock) {
            if(!book.tactic)continue;
            if(std::find(learned.begin(),learned.end(),book.tactic)!=learned.end()) {++plan.known;continue;}
            if(!book.stock) {++plan.stock_missing;continue;}
            auto slot=std::find(learned.begin(),learned.end(),0u);
            if(slot==learned.end()) {++plan.capacity_missing;continue;}
            --book.stock;*slot=book.tactic;
            plan.work.push_back({person.id,book.id,book.tactic});
        }
    }
    return plan;
}
enum class TacticKind { Unique, Common, Trade };
struct TacticInfo {unsigned id;TacticKind kind;};
// Positive values select a specific tactic. Negative values select a category.
constexpr int Unique=-1,Common=-2,Trade=-3;
inline int category(TacticKind kind) {
    return kind==TacticKind::Unique?Unique:kind==TacticKind::Common?Common:Trade;
}
inline std::array<unsigned,3> select_tactics(const Officer &person,
        const std::vector<TacticInfo> &catalog,const std::vector<int> &priority) {
    std::array<unsigned,3> result{};unsigned count=0;
    auto append=[&](unsigned id) {
        if(count>=3 || !knows(person,id) || std::find(result.begin(),result.end(),id)!=result.end())return;
        result[count++]=id;
    };
    for(int entry:priority) {
        if(entry>0)append(static_cast<unsigned>(entry));
        else for(unsigned id:person.tactics) {
            auto info=std::find_if(catalog.begin(),catalog.end(),[&](const TacticInfo &t){return t.id==id;});
            if(info!=catalog.end() && category(info->kind)==entry)append(id);
        }
    }
    for(unsigned id:person.tactics)append(id);
    return result;
}
struct Priorities {
    bool enabled=false;
    std::vector<int> martial{Unique,Common,Trade};
    std::vector<int> intellect{Unique,Common,Trade};
    const std::vector<int> &for_person(const Officer &person) const {
        return person.abilities[1]>=person.abilities[2]?martial:intellect;
    }
};
}
