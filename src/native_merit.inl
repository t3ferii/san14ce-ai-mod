/* Isolated merit candidate: original eight friends stay native. No remote
 * game calls; these wrappers execute synchronously on native caller threads. */
#include "merit_bond.h"
using MeritHas=int(__fastcall *)(Obj,Obj);
using MeritList=unsigned(__fastcall *)(Obj,GameList *,int);
using MeritBatch=unsigned(__fastcall *)(Obj);
using MeritJoin=uint64_t(__fastcall *)(Obj,Obj,Obj,Obj,int,uint64_t,int);
using MeritQuery=uint64_t(__fastcall *)(Obj,unsigned);
using MeritReset=void(__fastcall *)(Obj);
using MeritNormalize=void(__fastcall *)(Obj);
using MeritText=const wchar_t *(__fastcall *)(int);
static MeritHas original_merit_has;
static MeritList original_merit_list;
static MeritBatch original_merit_batch;
static MeritJoin original_merit_join;
static MeritQuery original_merit_query;
static MeritReset original_merit_reset;
static MeritText original_merit_text;
static MeritNormalize original_merit_normalize;
static merit::Store merit_bonds;
static SRWLOCK merit_lock=SRWLOCK_INIT,merit_effect_lock=SRWLOCK_INIT;
static Obj merit_world;
static __declspec(thread) unsigned in_merit_profile;
static __declspec(thread) unsigned in_merit_normalize;
static void merit_remove_native_slot(const merit::Bond &bond) {
    if(!bond.native_slot)return;
    Obj person=table(0x158,bond.officer,6000);
    unsigned offset=0xC0+(bond.native_slot-1)*2;
    if(valid(person) && word(person,0x10)==bond.officer && word(person,offset)==bond.ruler)
        *reinterpret_cast<uint16_t *>(person+offset)=0;
}
static void merit_use_native_slot(unsigned id,unsigned ruler) {
    Obj person=table(0x158,id,6000);if(!valid(person))return;
    for(unsigned i=0;i<8;++i)if(word(person,0xC0+i*2)==ruler)return;
    for(unsigned i=0;i<8;++i)if(!word(person,0xC0+i*2)) {
        *reinterpret_cast<uint16_t *>(person+0xC0+i*2)=static_cast<uint16_t>(ruler);
        merit_bonds.set_native_slot(id,i+1);return;
    }
}
static merit::Context merit_context(unsigned id,void *) {
    COST_SCOPE("merit_context");
    Obj person=table(0x158,id,6000);merit::Context out;
    if(!friendship::valid_id(id) || !valid(person) || word(person,0x10)!=id ||
        person[0xE6]==7 || person[0xE6]==9)return out;
    out.alive=true;out.grade=person[0x1C2];
    Obj district=table(0xE0D0,person[0xE0],51);
    if(!valid(district))return out;
    unsigned force_id=district[0x10];
    Obj force=fn<Obj(__fastcall *)(Obj)>(0x22CA00)(person);
    if(force_id==0 || force_id>51 || !valid(force) || table(0xDF30,force_id,51)!=force)return out;
    out.force=force_id;out.ruler=word(force,0x10);return out;
}
static void merit_check_world() {
    Obj current=world();if(current!=merit_world) {merit_bonds.clear();merit_world=current;}
}
static bool merit_is_world_person(Obj person,unsigned id) {
    // Also safe on the native archive worker: storage identity only.
    Obj current=world();return current && friendship::valid_id(id) &&
        pointer(current,0x158+id*8)==person;
}
static int __fastcall merit_has(Obj person,Obj other) {
    COST_SCOPE("merit_has");
    if(!enabled || !valid(person) || !valid(other))return original_merit_has(person,other);
    bool found=false;
    AcquireSRWLockExclusive(&merit_lock);
    try {
        merit_check_world();
        merit_bonds.prune_pair(word(person,0x10),word(other,0x10),merit_context,nullptr,merit_remove_native_slot);
        found=merit_bonds.contains(word(person,0x10),word(other,0x10),merit_context,nullptr);
    }
    catch(...) {found=false;}
    ReleaseSRWLockExclusive(&merit_lock);return found?1:original_merit_has(person,other);
}
static unsigned __fastcall merit_list(Obj person,GameList *out,int templates) {
    COST_SCOPE("merit_list");
    // Scenario/editor templates are not active, awarded officers.
    if(!enabled || templates || in_merit_profile || in_merit_normalize || !valid(person) || !out)return original_merit_list(person,out,templates);
    std::vector<uint16_t> ids;
    AcquireSRWLockExclusive(&merit_lock);
    try {
        merit_check_world();merit_bonds.prune_person(word(person,0x10),merit_context,nullptr,merit_remove_native_slot);
        ids=merit_bonds.list(word(person,0x10),merit_context,nullptr);
    }
    catch(...) {ReleaseSRWLockExclusive(&merit_lock);return original_merit_list(person,out,templates);}
    ReleaseSRWLockExclusive(&merit_lock);
    unsigned count=original_merit_list(person,out,templates);
    bool present[6001]={};unsigned visited=0;
    for(Link *it=list_first(*out);it && visited++<6000;it=it->next)
        if(valid(it->value)) {unsigned id=word(it->value,0x10);if(id<=6000)present[id]=true;}
    for(unsigned id:ids) {
        Obj other=table(0x158,id,6000);if(!valid(other))continue;
        if(!present[id] && list_add(*out,other)) {present[id]=true;++count;}
    }
    return count;
}
static void __fastcall merit_normalize(Obj person) {
    COST_SCOPE("merit_normalize");
    // 21B2E0 compacts the returned list back into native Person+C0..CF.
    // Supplemental bonds must never become permanent ordinary friendships.
    struct Scope {Scope(){++in_merit_normalize;}~Scope(){--in_merit_normalize;}} scope;
    original_merit_normalize(person);
    if(enabled && valid(person)) {
        AcquireSRWLockExclusive(&merit_lock);
        unsigned id=word(person,0x10);const merit::Bond *bond=merit_bonds.bond(id);
        if(bond && bond->native_slot) {
            unsigned slot=0;for(unsigned i=0;i<8;++i)if(word(person,0xC0+i*2)==bond->ruler) {slot=i+1;break;}
            merit_bonds.set_native_slot(id,slot);
        }
        ReleaseSRWLockExclusive(&merit_lock);
    }
}
struct MeritBefore {Obj person,node;unsigned id,grade,force,ruler;};
// Merit routes (2026-10-23, user request): native keeps two independent counts,
// Person+1C2 military (rows 1..3, command troops) and +1C3 civil (rows 4..6,
// districts/policy), and 2A9BB0 adds both. One officer follows one route: the
// route he already has (scenario/earlier award) wins, the other stays off.
// Both counts at once (older saves, or both chosen from zero): the larger
// count wins, a tie keeps the civil route.
static bool merit_civil_route(unsigned military,unsigned civil) {return civil && civil>=military;}
static void merit_route_lock(const std::vector<MeritBefore> &before) {
    // 1EC550 writes node+20 (low nibble military, high nibble civil) to the person.
    for(const auto &old:before) {
        unsigned char &packed=old.node[0x20];unsigned military=packed&15,civil=packed>>4;
        unsigned had_military=old.person[0x1C2],had_civil=old.person[0x1C3];
        if(had_military && had_civil)continue;
        bool civil_route=had_civil?true:had_military?false:merit_civil_route(military,civil);
        if(civil_route)military=had_military;else civil=had_civil;
        unsigned char locked=static_cast<unsigned char>(military|civil<<4);
        if(locked!=packed) {log_event("merit_route_locked",old.id,packed,locked);packed=locked;}
    }
}
static unsigned merit_route_part(unsigned first,unsigned count,unsigned effect) {
    // Same row walk as 2A9BB0 for one route.
    Obj current=world();unsigned total=0;if(!current)return 0;
    for(unsigned i=0;i<count;++i) {
        unsigned row=first+i;Obj record=pointer(current,0x85DF0+(row<7?row:0)*8);
        if(record && record[0x10]==effect)total+=word(record,0x12);
    }
    return total;
}
static std::vector<MeritBefore> merit_batch_snapshot(Obj command) {
    std::vector<MeritBefore> result;
    if(!command)return result;
    size_t count=*reinterpret_cast<size_t *>(command+8);
    Obj head=pointer(command,0);
    if(!head || count==0 || count>6000)return result;
    Obj node=pointer(head,0);result.reserve(count);
    size_t visited=0;
    for(;node && node!=head && visited<count;++visited) {
        unsigned id=dword(node,0x1C);Obj person=table(0x158,id,6000);
        if(valid(person) && word(person,0x10)==id) {
            auto context=merit_context(id,nullptr);
            result.push_back({person,node,id,context.grade,context.force,context.ruler});
        }
        // Native MSVC RB-tree successor; bounded walks guard corrupt cycles.
        Obj right=pointer(node,0x10);
        if(!right)return {};
        if(!right[0x19]) {
            node=right;size_t steps=0;
            while(pointer(node,0) && !pointer(node,0)[0x19]) {
                if(++steps>count)return {};node=pointer(node,0);
            }
        } else {
            Obj parent=pointer(node,8);size_t steps=0;
            while(parent && !parent[0x19] && node==pointer(parent,0x10)) {
                if(++steps>count)return {};node=parent;parent=pointer(parent,8);
            }
            node=parent;
        }
    }
    if(node!=head || visited!=count)return {};
    return result;
}
static unsigned __fastcall merit_batch(Obj command) {
    std::vector<MeritBefore> before;
    if(enabled)try {before=merit_batch_snapshot(command);merit_route_lock(before);}catch(...) {before.clear();}
    unsigned result=original_merit_batch(command);
    if(!enabled || before.empty())return result;
    unsigned awards=0;
    AcquireSRWLockExclusive(&merit_lock);
    try {
        merit_check_world();merit_bonds.prune(merit_context,nullptr,merit_remove_native_slot);
        for(const auto &old:before) {
            if(table(0x158,old.id,6000)!=old.person)continue;
            auto now=merit_context(old.id,nullptr);
            // Native can apply part of a batch before returning failure.
            // Only a real, same-force transition to military grade 3 awards.
            if(now.force==old.force && now.ruler==old.ruler &&
                merit_bonds.award(old.id,old.grade,merit_context,nullptr)) {merit_use_native_slot(old.id,now.ruler);++awards;}
        }
    } catch(...) {log_event("merit_bond_award_failed");}
    ReleaseSRWLockExclusive(&merit_lock);
    if(awards)log_event("merit_bond_awarded",awards);return result;
}
static uint64_t __fastcall merit_join(Obj person,Obj district,Obj city,Obj event,
                                    int flag,uint64_t context,int mode) {
    unsigned id=person?word(person,0x10):0;
    bool owned=enabled && merit_is_world_person(person,id);
    uint64_t result=original_merit_join(person,district,city,event,flag,context,mode);
    // The native success path zeros both bytes at Person+1C2. Never grant a
    // free grade 1 and never reset anything on a failed recruitment attempt.
    if(owned && result && person[0x1C2]==0) {
        AcquireSRWLockExclusive(&merit_lock);merit_check_world();merit_bonds.erase_person(id,merit_remove_native_slot);
        ReleaseSRWLockExclusive(&merit_lock);log_event("merit_bond_join_reset",id);
    }
    return result;
}
static bool merit_effects() {
    Obj current=world();if(!current)return false;
    const unsigned types[]={0,1,1,1,2,2,3},values[]={0,3000,3000,0,1,1,1};
    Obj records[7];
    for(unsigned i=0;i<7;++i) {
        records[i]=pointer(current,0x85DF0+i*8);
        if(!records[i] || (i && records[i]!=records[0]+i*24) ||
            pointer(records[i],0)!=pointer(records[0],0) ||
            *reinterpret_cast<uint64_t *>(records[i]+8)!=0 || records[i][0x10]!=types[i])return false;
    }
    bool original=word(records[1],0x12)==500 && word(records[2],0x12)==500 && word(records[3],0x12)==1000;
    bool modified=word(records[1],0x12)==3000 && word(records[2],0x12)==3000 && word(records[3],0x12)==0;
    if((!original && !modified) || word(records[0],0x12)!=0 ||
        word(records[4],0x12)!=1 || word(records[5],0x12)!=1 || word(records[6],0x12)!=1)return false;
    if(original)for(unsigned i=1;i<4;++i)
        *reinterpret_cast<uint16_t *>(records[i]+0x12)=static_cast<uint16_t>(values[i]);
    return true;
}
static uint64_t __fastcall merit_query(Obj person,unsigned effect) {
    COST_SCOPE("merit_query");
    if(enabled) {
        AcquireSRWLockExclusive(&merit_effect_lock);bool applied=merit_effects();
        ReleaseSRWLockExclusive(&merit_effect_lock);
        if(!applied) {static volatile LONG warned;if(!InterlockedExchange(&warned,1))log_event("merit_effect_table_rejected");}
    }
    uint64_t result;
    {COST_SCOPE("native_merit_query");result=original_merit_query(person,effect);}
    if(enabled && result && valid(person) && person[0x1C2] && person[0x1C3]) {
        unsigned military=person[0x1C2],civil=person[0x1C3];
        unsigned off=merit_civil_route(military,civil)?merit_route_part(1,military,effect):merit_route_part(4,civil,effect);
        result=off>=result?0:result-off;
    }
    return result;
}
static const wchar_t *__fastcall merit_effect_text(int stage) {
    bool applied=false;
    if(enabled) {
        AcquireSRWLockExclusive(&merit_effect_lock);applied=merit_effects();ReleaseSRWLockExclusive(&merit_effect_lock);
    }
    if(applied && stage==3)return L"군주와 상호 친애";
    return original_merit_text(stage);
}
static void __fastcall merit_reset(Obj person) {
    unsigned id=word(person,0x10);bool owned=enabled && merit_is_world_person(person,id);
    original_merit_reset(person);
    if(owned) {
        AcquireSRWLockExclusive(&merit_lock);merit_bonds.erase_person(id,merit_remove_native_slot);ReleaseSRWLockExclusive(&merit_lock);
    }
}
static unsigned char __fastcall merit_person_read(Obj person,Obj archive) {
    unsigned old=word(person,0x10);bool reading=dword(archive,0x20)==1;
    bool old_owned=enabled && reading && merit_is_world_person(person,old);
    unsigned char result=fn<unsigned char(__fastcall *)(Obj,Obj)>(0x2390B0)(person,archive);
    if(enabled && reading && result) {
        unsigned id=word(person,0x10);bool new_owned=merit_is_world_person(person,id);
        if(old_owned || new_owned) {
            AcquireSRWLockExclusive(&merit_lock);
            if(old_owned)merit_bonds.erase_person(old);if(new_owned)merit_bonds.erase_person(id);
            ReleaseSRWLockExclusive(&merit_lock);
        }
    }
    return result;
}
#include "native_merit_scroll.inl"
static void __fastcall merit_profile(Obj profile,Obj person) {
    for(auto &state:merit_scrolls)if(state.profile==profile)state.visible=false;
    merit_scroll_reset_input();
    // Render the eight ordinary native friends with their original slots.
    // Gameplay enumeration outside this scope still includes merit bonds.
    struct ProfileScope {ProfileScope(){++in_merit_profile;}~ProfileScope(){--in_merit_profile;}};
    {ProfileScope scope;fn<void(__fastcall *)(Obj,Obj)>(0x908820)(profile,person);}
    if(!enabled || !valid(person) || person[0xE6]==7 || person[0xE6]==9)return;
    try {
        // Match the native biography visibility predicates.
        if(fn<int(__fastcall *)(Obj)>(0x2323D0)(person) || fn<int(__fastcall *)(Obj)>(0x232400)(person))return;
        const wchar_t *original=fn<const wchar_t *(__fastcall *)(unsigned)>(0x30C020)(word(person,0x10));
        if(!original)return;
        // Native 930E30 has a bounded stack text buffer. Keep the complete
        // biography intact in UI state and render bounded scrolling pages.
        size_t length=wcsnlen(original,32769);if(length>32768)return;
        std::wstring biography(original,length);std::vector<uint16_t> ids;
        AcquireSRWLockExclusive(&merit_lock);
        try {
            merit_check_world();merit_bonds.prune_person(word(person,0x10),merit_context,nullptr);
            ids=merit_bonds.list(word(person,0x10),merit_context,nullptr);
        } catch(...) {ReleaseSRWLockExclusive(&merit_lock);throw;}
        ReleaseSRWLockExclusive(&merit_lock);
        std::wstring label=L"논공 친애: ";
        if(ids.empty())label+=L"없음";
        unsigned displayed=0;
        for(unsigned id:ids) {
            Obj other=table(0x158,id,6000);if(!valid(other))continue;
            const wchar_t *name=fn<const wchar_t *(__fastcall *)(Obj)>(0x22EEC0)(other);
            if(!name)continue;
            size_t n=wcsnlen(name,64);if(n==64)continue;
            // Bounded text, with explicit total when an unusually large
            // custom scenario exceeds the display capacity. Store is full.
            if(label.size()+n+2>600)break;
            if(displayed)label+=L", ";label.append(name,n);++displayed;
        }
        if(displayed<ids.size())label+=L" 외 "+std::to_wstring(ids.size()-displayed)+L"명";
        if(ids.size()>1)label+=L" (총 "+std::to_wstring(ids.size())+L"명)";
        label+=L"\n\n";label+=biography;
        auto key_fn=*reinterpret_cast<unsigned(__fastcall **)(Obj,unsigned)>(pointer(profile,0)+0x198);
        unsigned key=key_fn(profile,0x30);Obj graph=profile+0x130;
        if(key>=dword(graph,0x10) || !pointer(graph,8) || !pointer(pointer(graph,8),key*8))return;
        // Native 930D20 copies into the descriptor and live text widget.
        merit_scroll_set(profile,key,label);
    } catch(...) {log_event("merit_profile_label_failed");}
}
