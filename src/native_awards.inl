/* Logical storage retains native ruler ownership for capture/succession and
 * vanilla save compatibility. Equipped-item iterators hide stored items.
 * Storage flags are bound to the exact native archive hash, on host threads. */
#include "award_policy.h"
#include <algorithm>
#include <map>
static awards::Inventory award_storage,award_observed;
static std::array<bool,awards::MaxItems> award_observed_found;
static Obj award_world;
static bool award_initialized,award_syncing;
static int award_ai_days[52];
static Obj award_selected_item;
static Obj award_selected_person;
static unsigned award_ai_transfer;
static volatile LONG award_reset_epoch;
#ifdef TACTICS_FIXTURE
static unsigned award_full_sync_runs;
#endif
using AwardTransfer=int(__fastcall *)(Obj *);
using AwardList=unsigned(__fastcall *)(Obj,GameList *);
using AwardIterator=Obj(__fastcall *)(Obj,Obj);
using AwardOne=void(__fastcall *)(Obj);
using AwardState=int(__fastcall *)(Obj);
static AwardTransfer original_award_transfer;
static AwardList original_award_items,original_award_recipients;
using PostPeople=void(__fastcall *)(Obj,GameList *);
using PostSelect=int(__fastcall *)(Obj,Obj,Obj);
using PostPicker=Obj(__fastcall *)(Obj);
static PostPicker original_post_picker;
static PostPeople original_post_people;
static PostSelect original_post_select;
static AwardIterator original_award_iterator;
static AwardIterator original_award_view;
static AwardOne original_post_batch,original_award_ui;
static AwardState original_award_manual;
static void *award_equipped_vtable[4],*award_storage_vtable[4];
static unsigned award_item_id(Obj item) {
    Obj root=world(),first=root?pointer(root,0x77B18):nullptr;
    uintptr_t p=reinterpret_cast<uintptr_t>(item),b=reinterpret_cast<uintptr_t>(first);
    if(!first || p<b || (p-b)%128 || (p-b)/128>=awards::MaxItems)return awards::MaxItems;
    return static_cast<unsigned>((p-b)/128);
}
static std::vector<Obj> award_items() {
    COST_SCOPE("award_items");
    std::vector<Obj> result;Obj root=world();if(!root)return result;
    auto &list=*reinterpret_cast<GameList *>(root+0x108);
    for(Link *l=list_first(list);l && result.size()<awards::MaxItems;l=l->next)
        if(valid(l->value) && award_item_id(l->value)<awards::MaxItems)result.push_back(l->value);
    return result;
}
static unsigned award_mask(Obj item) {return awards::slots(item[0x45],item[0x46],item[0x47]);}
static unsigned award_force_id(Obj person) {return valid(person)?fn<unsigned(__fastcall *)(Obj)>(0x22CE20)(person):0;}
static Obj award_ruler(unsigned force) {COST_SCOPE("award_ruler");Obj f=table(0xDF30,force,51);return valid(f)?table(0x158,word(f,0x10),6000):nullptr;}
static bool award_stored(Obj item) {
    unsigned id=award_item_id(item);return id<awards::MaxItems && award_storage[id] &&
        (award_storage[id]==word(item,0x4A) || item==award_selected_item);
}
static void award_put_storage(Obj item,unsigned force) {
    Obj ruler=award_ruler(force);unsigned id=award_item_id(item);
    if(!valid(ruler) || id>=awards::MaxItems)return;
    unsigned owner=word(ruler,0x10);*reinterpret_cast<uint16_t *>(item+0x4A)=static_cast<uint16_t>(owner);
    award_storage[id]=award_observed[id]=static_cast<uint16_t>(owner);
}
static bool award_deployed(Obj p) {return valid(fn<Obj(__fastcall *)(Obj)>(0x22AA70)(p));}
static bool award_active(Obj p) {
    return award_deployed(p) || fn<int(__fastcall *)(Obj,int)>(0x233110)(p,0)!=0 ||
        fn<int(__fastcall *)(Obj)>(0x232240)(p)!=0 || (word(p,0x15E)&0x4001)!=0 || dword(p,0x1E8)!=0;
}
static bool award_serving(Obj p,unsigned force) {
    return valid(p) && p[0xE6]>=1 && p[0xE6]<=4 && award_force_id(p)==force;
}
static void award_reset(bool initialized=false) {
    InterlockedIncrement(&award_reset_epoch);
    award_storage.fill(0);award_observed.fill(0);award_observed_found.fill(false);award_world=world();award_initialized=initialized;
    for(auto &d:award_ai_days)d=-1;
}
// Compare exact inputs rather than a hash or a time-based throttle. Native
// capture/succession can change ownership without entering a mod write hook.
struct AwardSyncInput {
    Obj item,person,ruler;unsigned id,owner,force,mask,score,status,stored,observed,ruler_id;
    bool found,observed_found,alive,ruler_alive;
    bool operator==(const AwardSyncInput &b) const {
        return item==b.item && person==b.person && ruler==b.ruler && id==b.id && owner==b.owner &&
            force==b.force && mask==b.mask && score==b.score && status==b.status && stored==b.stored &&
            observed==b.observed && ruler_id==b.ruler_id && found==b.found && observed_found==b.observed_found &&
            alive==b.alive && ruler_alive==b.ruler_alive;
    }
};
struct AwardSyncCache {
    struct Owner {unsigned stamp=0,force=0,status=0;Obj person=nullptr;bool alive=false;};
    struct Ruler {unsigned stamp=0,id=0;Obj person=nullptr;bool alive=false;};
    Owner owners[6001];Ruler rulers[52];unsigned stamp=0;
    std::vector<AwardSyncInput> previous,current;Obj root=nullptr,selected=nullptr;
    LONG epoch=0;bool ready=false;
};
static void award_capture_inputs(const std::vector<Obj> &items,AwardSyncCache &cache) {
    COST_SCOPE("award_capture_inputs");
    if(++cache.stamp==0) {for(auto &c:cache.owners)c.stamp=0;for(auto &c:cache.rulers)c.stamp=0;++cache.stamp;}
    cache.current.clear();cache.current.reserve(items.size());
    for(Obj item:items) {
        unsigned id=award_item_id(item),owner=word(item,0x4A);
        AwardSyncCache::Owner invalid;auto &c=owner<=6000?cache.owners[owner]:invalid;
        if(c.stamp!=cache.stamp) {
            c.stamp=cache.stamp;c.person=table(0x158,owner,6000);c.alive=valid(c.person);
            c.force=c.alive?award_force_id(c.person):0;c.status=c.alive?c.person[0xE6]:0;
        }
        Obj ruler=nullptr;unsigned ruler_id=0;bool ruler_alive=false;
        if(c.force && c.force<=51) {auto &r=cache.rulers[c.force];
            if(r.stamp!=cache.stamp) {r.stamp=cache.stamp;r.person=award_ruler(c.force);
                r.alive=valid(r.person);r.id=r.alive?word(r.person,0x10):0;}
            ruler=r.person;ruler_id=r.id;ruler_alive=r.alive;
        }
        cache.current.push_back({item,c.person,ruler,id,owner,c.force,award_mask(item),
            static_cast<unsigned>(item[0x46])+item[0x48],c.status,award_storage[id],award_observed[id],ruler_id,
            (item[0x4F]&1)!=0,award_observed_found[id],c.alive,ruler_alive});
    }
}
static void award_full_synchronize(std::vector<Obj> &items) {
        COST_SCOPE("award_full_synchronize");
#ifdef TACTICS_FIXTURE
        ++award_full_sync_runs;
#endif
        for(Obj item:items) {
            unsigned id=award_item_id(item),owner=word(item,0x4A);
            if(item==award_selected_item && owner==0)continue;
            Obj person=table(0x158,owner,6000);unsigned force=award_force_id(person);
            bool ruler=force && force<=51 && person==award_ruler(force);
            if(!(item[0x4F]&1) || !force || !award_serving(person,force))award_storage[id]=0;
            else if(!award_initialized || award_observed[id]!=owner || !award_observed_found[id])award_storage[id]=ruler?static_cast<uint16_t>(owner):0;
            else if(award_storage[id] && !ruler)award_storage[id]=0;
            award_observed[id]=static_cast<uint16_t>(owner);
            award_observed_found[id]=(item[0x4F]&1)!=0;
        }
        award_initialized=true;
        // Keep the strongest per slot; excess equipment returns to its force.
        std::sort(items.begin(),items.end(),[](Obj a,Obj b) {
            int sa=a[0x46]+a[0x48],sb=b[0x46]+b[0x48];
            return sa!=sb?sa>sb:award_item_id(a)<award_item_id(b);
        });
        std::array<unsigned,6001> used={};
        for(Obj item:items) {
            unsigned owner=word(item,0x4A),mask=award_mask(item);if(!mask || award_stored(item) || owner>6000)continue;
            Obj p=table(0x158,owner,6000);unsigned force=award_force_id(p);
            if(!force || !award_serving(p,force))continue;
            if(used[owner]&mask)award_put_storage(item,force);else used[owner]|=mask;
        }
}
static void award_synchronize() {
    COST_SCOPE("award_synchronize");
    if(!enabled || award_syncing || !world())return;
    award_syncing=true;
    try {
        if(award_world!=world())award_reset();
        static thread_local AwardSyncCache cache;
        auto items=award_items();award_capture_inputs(items,cache);
        if(!cache.ready || cache.root!=world() || cache.epoch!=award_reset_epoch ||
           cache.selected!=award_selected_item || cache.previous!=cache.current) {
            cache.ready=false;award_full_synchronize(items);
            // Normalization can move excess items to the ruler. Capture its
            // final state in native list order, avoiding another false miss.
            items=award_items();award_capture_inputs(items,cache);cache.previous.swap(cache.current);
            cache.root=world();cache.epoch=award_reset_epoch;cache.selected=award_selected_item;cache.ready=true;
        }
    } catch(...) {log_event("warehouse_sync_failed");}
    award_syncing=false;
}
static void award_owned_items(Obj iterator,std::vector<Obj> &items) {
    COST_SCOPE("award_owned_items");
    items.clear();Link *l=reinterpret_cast<Link *>(pointer(iterator,8));Obj end=pointer(iterator,16);
    unsigned visited=0;
    for(;l && reinterpret_cast<Obj>(l)!=end && visited++<awards::MaxItems;l=l->next)
        if(fn<int(__fastcall *)(Obj,Link *)>(0x333320)(iterator,l) && valid(l->value) &&
           award_item_id(l->value)<awards::MaxItems)items.push_back(l->value);
}
// Effect queries already have a native owner-filtered iterator. Reconcile only
// those items; unrelated officers are handled when queried or at the existing
// full save/award/AI boundaries. Officers with no items require no ruler lookup.
struct AwardReadState {std::vector<AwardSyncInput> previous;Obj selected=nullptr;bool ready=false;};
struct AwardReadCache {
    AwardSyncCache scratch;std::vector<Obj> items;std::map<Obj,AwardReadState> people;
    Obj root=nullptr;LONG epoch=0;
};
static void award_synchronize_person(Obj person,Obj iterator) {
    COST_SCOPE("award_synchronize_person");
    if(!enabled || award_syncing || !world() || !valid(person))return;
    award_syncing=true;
    try {
        if(award_world!=world())award_reset();
        static thread_local AwardReadCache cache;
        if(cache.root!=world() || cache.epoch!=award_reset_epoch || cache.people.size()>6000) {
            cache.people.clear();cache.root=world();cache.epoch=award_reset_epoch;
        }
        award_owned_items(iterator,cache.items);
        auto &state=cache.people[person];award_capture_inputs(cache.items,cache.scratch);
        if(!state.ready || state.selected!=award_selected_item || state.previous!=cache.scratch.current) {
            state.ready=false;
            if(!cache.items.empty())award_full_synchronize(cache.items);
            award_owned_items(iterator,cache.items);award_capture_inputs(cache.items,cache.scratch);
            state.previous=cache.scratch.current;state.selected=award_selected_item;state.ready=true;
        }
    } catch(...) {log_event("warehouse_sync_failed");}
    award_syncing=false;
}
static bool award_allowed(Obj person,Obj item) {
    unsigned mask=award_mask(item);int current=0;
    for(Obj old:award_items())if(old!=item && !award_stored(old) && word(old,0x4A)==word(person,0x10) &&
        (award_mask(old)&mask))current=(std::max)(current,int(old[0x46]));
    return awards::may_award(award_active(person),mask,item[0x46],current);
}
static bool award_ui_allowed(Obj person,Obj item) {
    return (!award_stored(item) && person==award_ruler(award_force_id(person))) || award_allowed(person,item);
}
static int __fastcall award_transfer(Obj *pair) { COST_SCOPE("awards_award_transfer");
    if(!enabled || !pair || !valid(pair[0]) || !valid(pair[1]))return original_award_transfer(pair);
    award_synchronize();Obj person=pair[0],item=pair[1];unsigned id=award_item_id(item);
    if(id>=awards::MaxItems)return original_award_transfer(pair);
    unsigned force=award_force_id(person),source=award_force_id(table(0x158,award_stored(item)?award_storage[id]:word(item,0x4A),6000));
    bool same=force && force==source;
    bool recall=same && !award_stored(item) && person==award_ruler(force) && !award_ai_transfer;
    if(same && !recall && (!award_serving(person,force) || !award_allowed(person,item))) {
        log_event("active_item_award_blocked",word(person,0x10),award_mask(item),item[0x46]);return 0;
    }
    auto items=award_items();std::vector<std::pair<Obj,uint16_t>> displaced;
    if(same && !recall)for(Obj old:items)
        if(old!=item && !award_stored(old) && word(old,0x4A)==word(person,0x10) && (award_mask(old)&award_mask(item))) {
            displaced.emplace_back(old,word(old,0x4A));award_put_storage(old,force);
        }
    int result=original_award_transfer(pair);
    if(!result)for(auto &old:displaced) {
        *reinterpret_cast<uint16_t *>(old.first+0x4A)=old.second;
        unsigned oldid=award_item_id(old.first);award_storage[oldid]=0;award_observed[oldid]=old.second;
    }
    if(result) {
        award_observed[id]=word(item,0x4A);award_storage[id]=0;
        if(recall || (!same && person==award_ruler(force)))award_put_storage(item,force);
        log_event(recall?"item_returned_to_warehouse":"item_awarded",word(person,0x10),id,force);
    }
    return result;
}
static int __fastcall award_equipment_predicate(Obj iterator,Link *link) { COST_SCOPE("awards_award_equipment_predicate");
    return fn<int(__fastcall *)(Obj,Link *)>(0x333320)(iterator,link) && !award_stored(link->value);
}
static int __fastcall award_storage_predicate(Obj iterator,Link *link) { COST_SCOPE("awards_award_storage_predicate");
    return fn<int(__fastcall *)(Obj,Link *)>(0x333320)(iterator,link) && award_stored(link->value);
}
static void award_filter_iterator(Obj out,bool storage) {
    COST_SCOPE("award_filter_iterator");
    *reinterpret_cast<void ***>(out)=storage?award_storage_vtable:award_equipped_vtable;
    auto pred=storage?&award_storage_predicate:&award_equipment_predicate;
    Link *l=reinterpret_cast<Link *>(pointer(out,8));Obj end=pointer(out,16);
    while(l && reinterpret_cast<Obj>(l)!=end && !pred(out,l))l=l->next;
    *reinterpret_cast<Link **>(out+8)=l;
}
static Obj __fastcall award_iterator(Obj person,Obj out) { COST_SCOPE("awards_award_iterator");
    COST_SCOPE("award_iterator");
    uintptr_t caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-reinterpret_cast<uintptr_t>(image);
    // Only effect/personal views and diplomacy change. Native death, capture,
    // succession and force destruction retain the native ownership iterator.
    bool storage=caller==0x6DB54B || caller==0x6DB6D5 || caller==0x6D9AAF;
    bool equipped=caller==0x22D455 || caller==0x22FDAA || caller==0x230067 ||
        caller==0x2B4D19 || caller==0x349B1F || caller==0x6E153B;
    Obj result;
    {COST_SCOPE("native_award_iterator");result=original_award_iterator(person,out);}
    if(enabled && (storage || equipped)) {
        award_synchronize_person(person,out);award_filter_iterator(out,storage);
    }
    return result;
}
static Obj __fastcall award_personal_view(Obj person,Obj out) { COST_SCOPE("awards_award_personal_view");
    if(enabled)award_synchronize();
    Obj result=original_award_view(person,out);if(enabled) {
        award_filter_iterator(out,false);award_filter_iterator(out+32,false);
    }
    return result;
}
static unsigned __fastcall award_warehouse_items(Obj district,GameList *out) { COST_SCOPE("awards_award_warehouse_items");
    unsigned count=original_award_items(district,out);if(!enabled || !out || !valid(district))return count;
    award_synchronize();Link *l=list_first(*out);
    while(l) {if(valid(award_selected_person) && !award_ui_allowed(award_selected_person,l->value)) {personnel_remove(*out,l->value);l=list_first(*out);}else l=l->next;}
    // The management selector includes equipped items for native recall and
    // redistribution. Diplomacy's separate iterator exposes storage only.
    for(Obj item:award_items())if((item[0x4F]&1) && award_force_id(table(0x158,award_stored(item)?award_storage[award_item_id(item)]:word(item,0x4A),6000))==district[0x10] &&
        (!valid(award_selected_person) || award_ui_allowed(award_selected_person,item))) {
        bool found=false;for(Link *it=list_first(*out);it;it=it->next)if(it->value==item)found=true;
        if(!found)list_add(*out,item);
    }
    count=0;for(l=list_first(*out);l;l=l->next)++count;return count;
}
static unsigned __fastcall award_recipients(Obj district,GameList *out) { COST_SCOPE("awards_award_recipients");
    unsigned count=original_award_recipients(district,out);if(!enabled || !out || !valid(district))return count;
    for(unsigned id=1;id<=6000;++id) {
        Obj p=table(0x158,id,6000);if(!award_serving(p,district[0x10]) || p[0x161]>=2)continue;
        if(award_selected_item && !award_ui_allowed(p,award_selected_item))continue;
        bool found=false;for(Link *l=list_first(*out);l;l=l->next)if(l->value==p)found=true;
        if(!found)list_add(*out,p);
    }
    if(award_selected_item) {
        Link *l=list_first(*out);while(l) {if(!award_ui_allowed(l->value,award_selected_item)) {personnel_remove(*out,l->value);l=list_first(*out);}else l=l->next;}
    }
    count=0;for(Link *l=list_first(*out);l;l=l->next)++count;return count;
}
static void __fastcall award_ui(Obj state) { COST_SCOPE("awards_award_ui");
    if(!enabled) {original_award_ui(state);return;}
    award_synchronize();Obj layout=pointer(state,0x478),item=layout?pointer(layout,0x188):nullptr;
    Obj person=layout?pointer(layout,0x190):nullptr;unsigned mode=layout?dword(layout,0x180):0;
    Obj previous=award_selected_item,previous_person=award_selected_person;
    award_selected_item=valid(item)?item:nullptr;award_selected_person=valid(person)?person:nullptr;
    uint16_t owner=valid(item)?word(item,0x4A):0;
    bool stored=valid(item) && award_stored(item);
    // The original selector removes Item.owner. A stored item has no equipped
    // owner, allowing the ruler to receive one item per slot as well.
    Obj owner_person=table(0x158,owner,6000),owner_ruler=award_ruler(award_force_id(owner_person));
    bool clear_owner=mode==1 && valid(item) && (stored || (valid(owner_ruler) && owner_person==owner_ruler));
    if(clear_owner)*reinterpret_cast<uint16_t *>(item+0x4A)=0;
    original_award_ui(state);
    if(clear_owner && word(item,0x4A)==0)*reinterpret_cast<uint16_t *>(item+0x4A)=owner;
    Obj selected=layout?pointer(layout,0x188):nullptr;
    if(mode==2 && valid(person) && !pointer(layout,0x190) && valid(selected) && award_stored(selected) && person==award_ruler(award_force_id(person))) {
        *reinterpret_cast<Obj *>(layout+0x190)=person;
        fn<void(__fastcall *)(Obj)>(0x79DFB0)(layout);
    }
    award_selected_item=previous;award_selected_person=previous_person;
}
static int __fastcall award_manual(Obj state) {
    if(enabled) {
        Obj layout=pointer(state,0x478),item=layout?pointer(layout,0x188):nullptr,person=layout?pointer(layout,0x190):nullptr;
        if(valid(item) && valid(person) && (award_stored(item) || person!=award_ruler(award_force_id(person))) && !award_allowed(person,item)) {
            native_ui::officer_notice(word(person,0x10),L"활동 중에는 능력치가 더 높은 명품만 수여할 수 있습니다.");return 0;
        }
    }
    return original_award_manual(state);
}
static void __fastcall award_post_people(Obj force,GameList *out) {
    original_post_people(force,out);if(!enabled || !out || !valid(force))return;
    // Resolve identity by table equality; no assumption about Force+10 (ruler).
    unsigned forceid=0;for(unsigned f=1;f<=51;++f)if(table(0xDF30,f,51)==force)forceid=f;
    if(!forceid)return;
    for(unsigned id=1;id<=6000;++id) {
        Obj p=table(0x158,id,6000);if(!award_serving(p,forceid) || p[0xE6]==1 || p[0xE6]==6 || p[0x1BC]==1)continue;
        bool found=false;for(Link *l=list_first(*out);l;l=l->next)if(l->value==p)found=true;
        if(!found)list_add(*out,p);
    }
}
static unsigned award_post_capacity(unsigned id) {
    Obj post=table(0x784F8,id,255);if(!valid(post))return 0;
    Obj begin=pointer(post,0x40),end=pointer(post,0x48);
    if(!begin || reinterpret_cast<uintptr_t>(end)-reinterpret_cast<uintptr_t>(begin)!=20)return word(post,0x34);
    // Serializer 23F380 encodes the plain +34 value into this vector using
    // vtable+50. Decode its first word with +58, as +48 decodes the attributes.
    auto get=*reinterpret_cast<uint16_t(__fastcall **)(Obj,uint16_t,unsigned,unsigned)>(pointer(post,0)+0x58);
    return get(post,word(begin,0),13,id);
}
static Obj __fastcall award_post_picker(Obj state) {
    if(!enabled || !state)return original_post_picker(state);
    Obj layout=pointer(state,0x4C0),post=layout?pointer(layout,0x1B8):nullptr;
    if(!valid(post))return original_post_picker(state);
    unsigned id=fn<unsigned(__fastcall *)(Obj)>(0x22DF80)(post);
    Obj occupant=fn<Obj(__fastcall *)(Obj)>(0x6DC0A0)(post),force=pointer(state,0x470);
    // 74B120 copies +498 into modal config+38 (disabled rows), independently
    // of the initial command/AI candidate list handled by 1EDB40.
    auto &blocked=*reinterpret_cast<GameList *>(state+0x498);
    GameList saved=blocked;list_init(blocked);
    for(Link *l=list_first(saved);l;l=l->next) {
        Obj p=l->value;
        bool allow=valid(p) && award_serving(p,award_force_id(p)) &&
            table(0xDF30,award_force_id(p),51)==force && p[0xE6]!=1 && p[0xE6]!=6 && p[0x1BC]!=1 &&
            award_active(p) && awards::may_post(true,award_deployed(p),id,award_post_capacity(id),award_post_capacity(p[0xE7])) &&
            (!valid(occupant) || occupant==p || !award_deployed(occupant));
        if(!allow)list_add(blocked,p);
    }
    Obj result=original_post_picker(state);
    list_free(blocked);blocked=saved;return result;
}
static int __fastcall award_post_select(Obj state,Obj person,Obj post) {
    if(enabled && valid(person) && valid(post)) {
        unsigned id=fn<unsigned(__fastcall *)(Obj)>(0x22DF80)(post);
        Obj occupant=fn<Obj(__fastcall *)(Obj)>(0x6DC0A0)(post);
        if(!awards::may_post(award_active(person),award_deployed(person),id,award_post_capacity(id),award_post_capacity(person[0xE7])) ||
            (valid(occupant) && occupant!=person && award_deployed(occupant))) {
            native_ui::officer_notice(word(person,0x10),L"활동 중에는 지휘병사가 같거나 늘어나는 관직만 부여할 수 있습니다.");return 0;
        }
    }
    return original_post_select(state,person,post);
}
static void award_guard_post_tree(Obj node,Obj head,unsigned &count) {
    if(!node || node==head || node[0x19] || ++count>6000)return;
    award_guard_post_tree(pointer(node,0),head,count);
    Obj person=pointer(node,0x20);unsigned post=dword(node,0x28);
    Obj occupant=nullptr;
    if(valid(person) && post<=255 && post!=73)for(unsigned id=1;id<=6000;++id) {
        Obj p=table(0x158,id,6000);
        if(p!=person && award_serving(p,award_force_id(person)) && p[0xE7]==post && award_deployed(p)){occupant=p;break;}
    }
    if(valid(person) && post<=255 && (!awards::may_post(award_active(person),award_deployed(person),post,award_post_capacity(post),award_post_capacity(person[0xE7])) ||
        (valid(occupant) && occupant!=person && award_deployed(occupant)))) {
        *reinterpret_cast<unsigned *>(node+0x28)=person[0xE7];log_event("active_post_reduction_blocked",word(person,0x10),post,person[0xE7]);
    }
    award_guard_post_tree(pointer(node,0x10),head,count);
}
static void __fastcall award_post_batch(Obj map) {
    if(enabled && map && pointer(map,8)) {Obj head=pointer(map,8);unsigned count=0;award_guard_post_tree(pointer(head,8),head,count);}
    original_post_batch(map);
}
static uint64_t __fastcall award_native_ai_command(Obj node,Obj context,Obj subject) {
    if(enabled) {Obj item=personnel_context_object(node,context,0x3C);if(valid(item) && award_mask(item))return 2;}
    return fn<Execute>(0x535E0)(node,context,subject);
}
static void award_ai_city(Obj city) {
    COST_SCOPE("award_ai_city");
    NativePhase phase("ai_award_distribution");
    award_synchronize();if(!valid(city))return;
    Obj district=(*reinterpret_cast<Obj(__fastcall **)(Obj)>(pointer(city,0)+0x80))(city);
    if(!automated_domestic_district(district))return;
    unsigned force=valid(district)?district[0x10]:0;if(!force || force>51)return;
    int day=game_day();if(day<0 || award_ai_days[force]==day)return;
    auto items=award_items();
    std::sort(items.begin(),items.end(),[](Obj a,Obj b) {int sa=a[0x46]+a[0x48],sb=b[0x46]+b[0x48];return sa!=sb?sa>sb:award_item_id(a)<award_item_id(b);});
    std::array<bool,awards::MaxItems> processed={};
    for(unsigned slot=0;slot<6;++slot) {
        std::vector<Obj> people;
        for(unsigned id=1;id<=6000;++id) {Obj p=table(0x158,id,6000);if(award_serving(p,force))people.push_back(p);}
        unsigned ability=slot<4?slot:0;
        std::sort(people.begin(),people.end(),[ability](Obj a,Obj b) {
            int sa=fn<int(__fastcall *)(Obj,unsigned,unsigned)>(0x22FCE0)(a,ability,0xFFD),sb=fn<int(__fastcall *)(Obj,unsigned,unsigned)>(0x22FCE0)(b,ability,0xFFD);
            return sa!=sb?sa>sb:word(a,0x10)<word(b,0x10);
        });
        size_t next=0;
        for(Obj item:items) {
            unsigned iid=award_item_id(item);
            if(processed[iid] || !award_stored(item) || !(item[0x4F]&1) || !(award_mask(item)&(1u<<slot)) ||
               award_force_id(table(0x158,word(item,0x4A),6000))!=force)continue;
            processed[iid]=true;
            while(next<people.size()) {
                Obj person=people[next++];if(word(item,0x4A)==word(person,0x10) && !award_stored(item))break;
                if(!award_allowed(person,item))continue;
                bool equipped=false;
                for(Obj old:items)if(!award_stored(old) && word(old,0x4A)==word(person,0x10) &&
                   (award_mask(old)&(1u<<slot)) && (slot<4?old[0x46]>=item[0x46]:old[0x48]>=item[0x48])) {equipped=true;break;}
                if(equipped)continue;
                // Native 1F0300 spends the force's ruling district resources,
                // not the recipient's physical/detachment district (40160).
                Obj resources=fn<Obj(__fastcall *)(Obj)>(0x22E7E0)(fn<Obj(__fastcall *)(Obj)>(0x22CA00)(person));
                int cost=fn<int(__fastcall *)()>(0x6D3670)();if(!valid(resources) || cost<0 || resources[0x14]<cost)continue;
                Obj pair[]={person,item};++award_ai_transfer;int success=award_transfer(pair);--award_ai_transfer;
                if(success)break;
            }
        }
    }
    award_ai_days[force]=day;
}
