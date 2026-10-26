#pragma once
#include <stdint.h>
#include <stddef.h>

// Rules only. Native bindings and screen/save integration must separately be
// validated; compiling this header alone never modifies the game.
namespace personnel {
enum RecruitmentReason : unsigned {
    Eligible=0, DislikedRuler=1, ForeignSpouse=2, ForeignSwornSibling=4,
    FamilyEnvoyException=8, MissingContext=16, HighLoyalty=32, LikesCurrentRuler=64,
    ServiceBan=128
};
struct RecruitmentFacts {
    bool context_known,dislikes_ruler,foreign_spouse,foreign_sworn_sibling;
    bool envoy_is_spouse_or_sworn_sibling;
    bool likes_current_ruler=false,high_loyalty=false,service_ban=false;
};
inline unsigned recruitment_reason(const RecruitmentFacts &f) {
    if(!f.context_known)return MissingContext;
    unsigned reason=f.likes_current_ruler?LikesCurrentRuler:0u;
    // Native 2784B0 checks these before every family exception. A current
    // ruler friendship below 100 is information, not a hard restriction.
    if(f.high_loyalty || f.service_ban)
        return reason | (f.high_loyalty?HighLoyalty:0u) | (f.service_ban?ServiceBan:0u);
    if(f.envoy_is_spouse_or_sworn_sibling)return reason|FamilyEnvoyException;
    return reason | (f.dislikes_ruler?DislikedRuler:0u) |
           (f.foreign_spouse?ForeignSpouse:0u) |
           (f.foreign_sworn_sibling?ForeignSwornSibling:0u);
}
inline bool recruitment_blocked(unsigned reason) {
    return (reason&(DislikedRuler|ForeignSpouse|ForeignSwornSibling|HighLoyalty|ServiceBan))!=0;
}
inline bool recruitment_hard_blocked(unsigned reason) {return (reason&(HighLoyalty|ServiceBan))!=0;}
enum class ItemClass : unsigned { LoyaltyOnly,Leadership,War,Intelligence,Politics,Life,Horse };
struct Item { unsigned id,force,owner;ItemClass category;int strength;bool in_storage; };
struct Officer { unsigned id,force;int ability[4];bool valid; };
inline int category_priority(const Officer &p,ItemClass kind) {
    switch(kind) {
    case ItemClass::Leadership:case ItemClass::Horse:return p.ability[0];
    case ItemClass::War:return p.ability[1];
    case ItemClass::Intelligence:return p.ability[2];
    case ItemClass::Politics:return p.ability[3];
    case ItemClass::Life:return p.ability[0]; // proposed fallback; native category not yet bound
    case ItemClass::LoyaltyOnly:return 0;
    }
    return 0;
}
inline unsigned best_recipient(const Item &item,const Officer *people,size_t people_count,
                               const Item *items,size_t item_count) {
    if(!item.in_storage || item.category==ItemClass::LoyaltyOnly)return 0;
    unsigned winner=0;int best=-1;
    for(size_t p=0;p<people_count;++p) {
        const Officer &person=people[p];
        if(!person.valid || !person.id || person.force!=item.force)continue;
        bool already_equipped=false;
        for(size_t j=0;j<item_count;++j)
            if(!items[j].in_storage && items[j].owner==person.id && items[j].force==item.force &&
               items[j].category==item.category) {already_equipped=true;break;}
        int score=category_priority(person,item.category);
        if(!already_equipped && (score>best || (score==best && (!winner || person.id<winner)))) {
            best=score;winner=person.id;
        }
    }
    return winner;
}
// Caller performs native transfer atomically after validating force membership.
// A loyalty-only item never displaces a stat item or another loyalty-only item.
inline bool displaced_by(const Item &old_item,const Item &new_item,unsigned recipient) {
    return old_item.id!=new_item.id && !old_item.in_storage && old_item.owner==recipient &&
           old_item.force==new_item.force && old_item.category==new_item.category &&
           new_item.category!=ItemClass::LoyaltyOnly;
}
struct Candidate { unsigned id;bool recommended,available;int native_rank; };
inline unsigned pick_unique_envoy(const Candidate *candidates,size_t count,
                                 const unsigned *reserved,size_t reserved_count) {
    unsigned winner=0;int best_rank=0;bool best_recommended=false;
    for(size_t i=0;i<count;++i) {
        const Candidate &c=candidates[i];if(!c.id || !c.available)continue;
        bool used=false;
        for(size_t j=0;j<reserved_count;++j)if(reserved[j]==c.id) {used=true;break;}
        if(used)continue;
        if(!winner || (c.recommended && !best_recommended) ||
           (c.recommended==best_recommended && (c.native_rank<best_rank ||
            (c.native_rank==best_rank && c.id<winner)))) {
            winner=c.id;best_rank=c.native_rank;best_recommended=c.recommended;
        }
    }
    return winner;
}
inline bool may_auto_work(unsigned injury) {return injury==0;}
}
