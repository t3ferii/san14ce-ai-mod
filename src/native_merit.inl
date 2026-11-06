#include "game_addresses.h"
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
// Layout manager chain of the merit UI/save slots (design 8.0 G1; null: original game function).
static void *next_merit_person_read,*next_merit_profile,*next_merit_profile_tick,*next_merit_save_host,*next_merit_load_host;
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
    Obj force=fn<Obj(__fastcall *)(Obj)>(GA(GA_22CA00))(person);
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
    if(!enabled || !mod_features.merit_friendship || !valid(person) || !valid(other))return original_merit_has(person,other);
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
    if(!enabled || !mod_features.merit_friendship || templates || in_merit_profile || in_merit_normalize || !valid(person) || !out)return original_merit_list(person,out,templates);
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
// User 2026-11-06: officers holding BOTH counts are an abnormal state (original game and this mod never create it; only another
// mod or an old save can). They must not get worse: the winning route (larger, tie civil) may still grow, the losing route keeps
// its current value and never grows. Existing values are never lowered.
static bool merit_route_winner_civil(unsigned had_military,unsigned had_civil,unsigned military,unsigned civil) {
    if(had_military && had_civil)return merit_civil_route(had_military,had_civil);
    return had_civil?true:had_military?false:merit_civil_route(military,civil);
}
static void merit_route_lock(const std::vector<MeritBefore> &before) {
    // 1EC550 writes node+20 (low nibble military, high civil) to the person.
    for(const auto &old:before) {
        unsigned char &packed=old.node[0x20];unsigned military=packed&15,civil=packed>>4;
        unsigned had_military=old.person[0x1C2],had_civil=old.person[0x1C3];
        bool civil_route=merit_route_winner_civil(had_military,had_civil,military,civil);
        if(civil_route)military=had_military;else civil=had_civil;
        if(civil_route && civil<had_civil)civil=had_civil;
        if(!civil_route && military<had_military)military=had_military;
        unsigned char locked=static_cast<unsigned char>(military|civil<<4);
        if(locked!=packed) {log_event("merit_route_locked",old.id,packed,locked);packed=locked;}
    }
}
// Both counts at once is logged once per officer and loaded world (load time and first query), to trace where it comes from.
static Obj merit_dual_world;static unsigned char merit_dual_seen[6001];
static void merit_dual_note(Obj person) {
#ifndef NO_RUNTIME_LOG
    if(!valid(person) || !person[0x1C2] || !person[0x1C3])return;
    unsigned id=word(person,0x10);if(id==0 || id>6000)return;
    Obj current=world();if(current!=merit_dual_world) {memset(merit_dual_seen,0,sizeof(merit_dual_seen));merit_dual_world=current;}
    if(merit_dual_seen[id])return;merit_dual_seen[id]=1;
    log_event("merit_dual_route_found",id,person[0x1C2],person[0x1C3]);
#else
    (void)person;
#endif
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
// Merit dialog (user 2026-11-06, local7): the confirm-time lock above only drops the opposite route AFTER gold and orders are spent.
// The dialog data object keeps the chosen level per officer in a std::map at +0x198 (key person id, value byte node+0x20: low nibble
// military, high nibble civil; same map type 6CBC60 reads). The button handler (old 7705F0, same bytes in 1a4a447d at 7726B0) maps
// the clicked button id to one nibble, writes the map entry and calls the layout refresh (old 781DA0) once. We wrap that handler:
// it runs untouched, then any click that RAISED a route the officer may not follow is undone in the map and the refresh runs again,
// so the picked level, the cost preview and the buttons stay consistent. local8 adds the native lock display below
// (merit_slot_states); this click guard stays as the fallback when that display is off.
using MeritDialogToggle=void(__fastcall *)(Obj,Obj);
using MeritDialogRefresh=void(__fastcall *)(Obj);
static MeritDialogToggle original_merit_dialog;
struct MeritDialogRow {Obj node;unsigned id;unsigned char packed;};
static bool merit_map_rows(Obj map,std::vector<MeritDialogRow> &rows) {
    // MSVC std::map: head pointer at map+0, head[0] = leftmost, node: parent +8, right +0x10, isnil +0x19, key +0x1C, value +0x20.
    rows.clear();Obj head=pointer(map,0);if(!head)return false;
    Obj node=pointer(head,0);size_t visited=0;
    while(node && node!=head) {
        if(++visited>7000)return false;
        rows.push_back({node,dword(node,0x1C),node[0x20]});
        Obj right=pointer(node,0x10);if(!right)return false;
        if(!right[0x19]) {
            node=right;size_t steps=0;
            while(pointer(node,0) && !pointer(node,0)[0x19]) {if(++steps>7000)return false;node=pointer(node,0);}
        } else {
            Obj parent=pointer(node,8);size_t steps=0;
            while(parent && !parent[0x19] && node==pointer(parent,0x10)) {if(++steps>7000)return false;node=parent;parent=pointer(parent,8);}
            node=parent;
        }
    }
    return node==head;
}
// May this officer follow `civil` route? actual = Person+1C2/1C3, chosen = dialog level before the click.
static bool merit_dialog_route_allowed(bool civil,unsigned actual_military,unsigned actual_civil,unsigned chosen_military,unsigned chosen_civil) {
    if(actual_military && actual_civil)return civil==merit_civil_route(actual_military,actual_civil);
    if(chosen_military && chosen_civil)return civil==merit_civil_route(chosen_military,chosen_civil);
    return civil?!(actual_military || chosen_military):!(actual_civil || chosen_civil);
}
// Returns the number of undone raises (route bit 1 military, 2 civil are logged per officer).
static unsigned merit_dialog_enforce(const std::vector<MeritDialogRow> &before,std::vector<MeritDialogRow> &after) {
    unsigned blocked=0;
    for(auto &row:after) {
        unsigned char old=0;
        for(const auto &b:before)if(b.id==row.id) {old=b.packed;break;}
        unsigned char now=row.node[0x20];if(now==old)continue;
        Obj person=table(0x158,row.id,6000);if(!valid(person) || word(person,0x10)!=row.id)continue;
        unsigned am=person[0x1C2],ac=person[0x1C3],om=old&15,oc=old>>4,nm=now&15,nc=now>>4,route=0;
        if(nm>om && !merit_dialog_route_allowed(false,am,ac,om,oc)) {nm=om;route|=1;}
        if(nc>oc && !merit_dialog_route_allowed(true,am,ac,om,oc)) {nc=oc;route|=2;}
        if(!route)continue;
        row.node[0x20]=static_cast<unsigned char>(nm|nc<<4);++blocked;
        log_event("merit_dialog_blocked",row.id,route,now);
    }
    return blocked;
}
static void __fastcall merit_dialog_toggle(Obj data,Obj event) { COST_SCOPE("merit_dialog_toggle");
    std::vector<MeritDialogRow> before,after;bool watch=enabled && data;
    if(watch)try {watch=merit_map_rows(data+0x198,before);}catch(...) {watch=false;}
    original_merit_dialog(data,event);
    if(!watch)return;
    try {
        if(merit_map_rows(data+0x198,after) && merit_dialog_enforce(before,after)) {
            auto refresh=fn<MeritDialogRefresh>(GA(GA_781DA0));if(refresh)refresh(data);
        }
    } catch(...) {log_event("merit_dialog_guard_failed");}
}
// Native lock display (user 2026-11-06, local8; docs/INSTALL_LOCAL2_TRIAGE.md 16). The row refresh (old 8A3140 / new 8A5380, the
// only caller) asks 6D9790 (new 6DBAA0)(dialog, person, int out[6]) for six slot states, military 0..2 then civil 3..5:
// 0 = grey button (not selectable), 1 = selectable/picked, 2 = held, 3 = locked (lock icon row+0x218, lock text row+0x1E8 and
// an empty disabled button; natively used for tiers above the officer's rank, text "unlock: <rank>" from 356810(slot+1)).
// The live dialog read 2026-11-06 confirmed the layout (row+8 = dialog, row+0x178 = person, row widgets at +0x180/+0x1B8/
// +0x1E8/+0x218, +0x10 = 1 shown / -1 hidden). The wrapper runs the native call, then gives state 3 to every not-held slot of a
// route the officer may not raise. The lock text stays native (local9: our own text wrapped in the 140-wide label).
// The row arrives in RBP (8A5380 keeps it there; the stub moves it to R9) and is only used when row+8/+0x178 match the call.
using MeritSlotStates=void(__fastcall *)(Obj,Obj,int *);
using MeritLabelText=void(__fastcall *)(Obj,const wchar_t *);
using MeritTierText=const wchar_t *(__fastcall *)(int);
using MeritAutoFill=void(__fastcall *)(Obj,int);
static MeritSlotStates original_merit_slot_states;
static MeritAutoFill original_merit_autofill;
static bool merit_dialog_entry(Obj data,unsigned id,unsigned char &packed) {
    std::vector<MeritDialogRow> rows;packed=0;if(!merit_map_rows(data+0x198,rows))return false;
    for(const auto &row:rows)if(row.id==id) {packed=row.packed;break;}
    return true;
}
// Returns the slots locked for display (bit i = slot i) and rewrites their states; texts are handled by the caller. Held slots
// keep state 2; slots already picked in the dialog (a stale pick) stay native so the player can still undo them.
static unsigned merit_lock_states(unsigned am,unsigned ac,unsigned char chosen,int *states) {
    unsigned cm=chosen&15,cc=chosen>>4,locked=0;
    bool lock_m=!merit_dialog_route_allowed(false,am,ac,cm,cc),lock_c=!merit_dialog_route_allowed(true,am,ac,cm,cc);
    for(unsigned i=0;i<6;++i) {
        bool route=i<3?lock_m:lock_c;unsigned tier=i<3?i:i-3,picked=i<3?cm:cc;
        if(route && states[i]!=2 && tier>=picked) {states[i]=3;locked|=1u<<i;}
    }
    return locked;
}
static void __fastcall merit_slot_states(Obj data,Obj person,int *states,Obj row) { COST_SCOPE("merit_slot_states");
    original_merit_slot_states(data,person,states);
    if(!enabled || !data || !states || !valid(person))return;
    try {
        if(!row || pointer(row,8)!=data || pointer(row,0x178)!=person)return;   // not the 8A5380 call shape: native display
        unsigned char chosen=0;if(!merit_dialog_entry(data,word(person,0x10),chosen))return;
        merit_lock_states(person[0x1C2],person[0x1C3],chosen,states);   // native lock text kept (user 2026-11-06: ours wrapped)
    } catch(...) {log_event("merit_lock_display_failed");}
}
// Auto fill (old 6C40B0 / new 6C6810 (dialog, mode); mode 4 military first, 3 by stats, otherwise civil first) writes
// Person+1C2/1C3 plus one tier on ONE route for every officer and refreshes (783F50). It ignores the held route, so an officer
// holding military got a civil tier under "civil first" (user 2026-11-06 live game). After it runs, a raise of a route the officer
// may not follow (held route first, both held: winner, nothing held: the chosen priority stays) moves to the allowed route;
// the shape check (exactly held + one tier on one route) keeps the native sum limit, then the refresh recomputes the cost.
static unsigned merit_autofill_fix(Obj data,int mode) {
    std::vector<MeritDialogRow> rows;if(!merit_map_rows(data+0x198,rows))return 0;
    unsigned fixed=0;
    for(auto &row:rows) {
        Obj person=table(0x158,row.id,6000);if(!valid(person) || word(person,0x10)!=row.id)continue;
        unsigned am=person[0x1C2],ac=person[0x1C3],nm=row.packed&15,nc=row.packed>>4,route=0;
        if(nm==am && nc==ac+1 && !merit_dialog_route_allowed(true,am,ac,0,0)) {nc=ac;nm=am+1;route=2;}
        else if(nm==am+1 && nc==ac && !merit_dialog_route_allowed(false,am,ac,0,0)) {nm=am;nc=ac+1;route=1;}
        if(!route || nm>15 || nc>15)continue;
        row.node[0x20]=static_cast<unsigned char>(nm|nc<<4);++fixed;
        log_event("merit_autofill_fixed",row.id,route,static_cast<unsigned>(mode));
    }
    return fixed;
}
static void __fastcall merit_autofill(Obj data,int mode) { COST_SCOPE("merit_autofill");
    original_merit_autofill(data,mode);
    if(!enabled || !data)return;
    try {
        if(merit_autofill_fix(data,mode)) {auto refresh=fn<MeritDialogRefresh>(GA(GA_781DA0));if(refresh)refresh(data);}
    } catch(...) {log_event("merit_autofill_guard_failed");}
}
static unsigned __fastcall merit_batch(Obj command) { COST_SCOPE("merit_batch");
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
            if(mod_features.merit_friendship && now.force==old.force && now.ruler==old.ruler &&
                merit_bonds.award(old.id,old.grade,merit_context,nullptr)) {merit_use_native_slot(old.id,now.ruler);++awards;}
        }
    } catch(...) {log_event("merit_bond_award_failed");}
    ReleaseSRWLockExclusive(&merit_lock);
    if(awards)log_event("merit_bond_awarded",awards);return result;
}
static uint64_t __fastcall merit_join(Obj person,Obj district,Obj city,Obj event,
                                    int flag,uint64_t context,int mode) { COST_SCOPE("merit_join");
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
// local5/local6: merit_effects() checks the 7 effect rows at world()+0x85DF0 (24-byte rows in one contiguous 7-element array that the
// world constructor allocates identically in both builds, see docs/INSTALL_LOCAL2_TRIAGE.md 14; types and values come from the
// scenario/save archive, never from code). Fixed by structure: row pointers contiguous, one vtable, +8 == 0, types 0,1,1,S,2,2,3,
// rows 0 and 4..6 values 0,1,1,1. S (row 3 = stage 3 military effect) tells the shape:
//   shape A (S == 1, troops; old build and old saves) : row 3 value 1000 (original) or 0 (changed by this mod)
//   shape B (S in merit_new_stage3_types, value 1)     : patch 25953386 stage 3 = city administration level +1, never rewritten
// Rows 1..2 (stage 1/2 troops) are set to the chosen troops (merit_troops_level) in both shapes; accepted current values are the
// original 500, the patched 1000 and any selectable value (older saves keep earlier choices). Row 3 of shape A follows
// merit_friendship: on = 0 troops (friendship replaces it), off = 1000 (original). Failing step (merit_effect_reject_step):
// 1 no world, 2 row pointer null / not contiguous / other vtable / nonzero +8 / wrong type byte (second value = row index),
// 3 row 1..3 value not a known table, 4 row 0 / 4..6 value. Diagnostic builds dump all 7 rows once on rejection.
static constexpr unsigned merit_troops_choices[]={1500,2000,2500,3000};
static constexpr unsigned merit_new_stage3_types[]={2,3};
static volatile LONG merit_effect_reject_step=0,merit_effect_reject_index=0,merit_effect_applied_logged=0,merit_effect_new_original=0;
static bool merit_effect_reject(unsigned step,unsigned index) {
    InterlockedExchange(&merit_effect_reject_step,step);InterlockedExchange(&merit_effect_reject_index,index);return false;
}
static bool merit_troops_known(unsigned value) {
    if(value==500 || value==1000)return true;
    for(unsigned choice:merit_troops_choices)if(value==choice)return true;
    return false;
}
#ifndef NO_RUNTIME_LOG
static void merit_effect_dump(Obj current) {
    static volatile LONG dumped;if(InterlockedExchange(&dumped,1))return;
    Obj base=pointer(current,0x85DF0);
    for(unsigned i=0;i<7;++i) {
        Obj row=pointer(current,0x85DF0+i*8);
        if(!row){log_event("merit_effect_record",i,0xFFFFFFFFu,0);continue;}
        unsigned long long vt=reinterpret_cast<unsigned long long>(pointer(row,0));
        unsigned long long img=reinterpret_cast<unsigned long long>(image);
        log_event("merit_effect_record",i,static_cast<unsigned>(base?row-base:0),static_cast<unsigned>(vt-img));
        log_event("merit_effect_record_value",i|(*reinterpret_cast<uint64_t *>(row+8)!=0?0x100u:0u),row[0x10],word(row,0x12));
    }
}
#endif
// 0 rejected, 1 shape A (troops stage 3), 2 shape B (non-troop stage 3). Rows are updated to the configured policy on success.
static unsigned merit_effect_state() {
    Obj current=world();if(!current){merit_effect_reject(1,0);return 0;}
    const unsigned types[]={0,1,1,1,2,2,3};
    Obj records[7];
    auto fail=[&](unsigned step,unsigned index)->unsigned {
#ifndef NO_RUNTIME_LOG
        merit_effect_dump(current);
#endif
        merit_effect_reject(step,index);return 0;};
    for(unsigned i=0;i<7;++i) {
        records[i]=pointer(current,0x85DF0+i*8);
        bool bad=!records[i] || (i && records[i]!=records[0]+i*24) ||
            pointer(records[i],0)!=pointer(records[0],0) || *reinterpret_cast<uint64_t *>(records[i]+8)!=0;
        if(!bad) {
            unsigned type=records[i][0x10];bool ok=type==types[i];
            if(i==3 && !ok)for(unsigned t:merit_new_stage3_types)if(type==t)ok=true;
            bad=!ok;
        }
        if(bad)return fail(2,i);
    }
    const bool shape_a=records[3][0x10]==1;
    const unsigned stage3=word(records[3],0x12);
    if(!merit_troops_known(word(records[1],0x12)) || !merit_troops_known(word(records[2],0x12)) ||
        (shape_a?(stage3!=1000 && stage3!=0):stage3!=1))return fail(3,word(records[1],0x12));
    if(word(records[0],0x12)!=0 ||
        word(records[4],0x12)!=1 || word(records[5],0x12)!=1 || word(records[6],0x12)!=1)return fail(4,0);
    const unsigned troops=merit_troops_choices[(mod_features.merit_troops_level-1)&3];
    const unsigned want3=mod_features.merit_friendship?0u:1000u;
    *reinterpret_cast<uint16_t *>(records[1]+0x12)=static_cast<uint16_t>(troops);
    *reinterpret_cast<uint16_t *>(records[2]+0x12)=static_cast<uint16_t>(troops);
    if(shape_a)*reinterpret_cast<uint16_t *>(records[3]+0x12)=static_cast<uint16_t>(want3);
    if(!shape_a && !InterlockedExchange(&merit_effect_new_original,1))
        log_event("merit_effect_table_new_original",records[3][0x10],stage3,troops);
    if(!InterlockedExchange(&merit_effect_applied_logged,1))
        log_event("merit_effect_table_applied",shape_a?1:2,troops,mod_features.merit_friendship?1:0);
    return shape_a?1:2;
}
static bool merit_effects() {return merit_effect_state()!=0;}
static uint64_t __fastcall merit_query(Obj person,unsigned effect) {
    COST_SCOPE("merit_query");
    if(enabled && person && person[0x1C2] && person[0x1C3])merit_dual_note(person);
    if(enabled) {
        AcquireSRWLockExclusive(&merit_effect_lock);unsigned state=merit_effect_state();
        ReleaseSRWLockExclusive(&merit_effect_lock);
        if(!state) {static volatile LONG warned;if(!InterlockedExchange(&warned,1))log_event("merit_effect_table_rejected",merit_effect_reject_step,merit_effect_reject_index);}
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
static const wchar_t *__fastcall merit_effect_text(int stage) { COST_SCOPE("merit_effect_text");
    unsigned state=0;
    if(enabled) {
        AcquireSRWLockExclusive(&merit_effect_lock);state=merit_effect_state();ReleaseSRWLockExclusive(&merit_effect_lock);
    }
    if(state && stage==3 && mod_features.merit_friendship) {
        if(state==1)return L"군주와 상호 친애";   // shape A: the troops effect is replaced
        // shape B: native "city administration level +1" stays, friendship is added to it.
        static thread_local std::wstring combined;
        const wchar_t *native=original_merit_text(stage);
        combined=native?native:L"";if(!combined.empty())combined+=L" / ";combined+=L"군주와 상호 친애";
        return combined.c_str();
    }
    return original_merit_text(stage);
}
static void __fastcall merit_reset(Obj person) { COST_SCOPE("merit_reset");
    unsigned id=word(person,0x10);bool owned=enabled && merit_is_world_person(person,id);
    original_merit_reset(person);
    if(owned) {
        AcquireSRWLockExclusive(&merit_lock);merit_bonds.erase_person(id,merit_remove_native_slot);ReleaseSRWLockExclusive(&merit_lock);
    }
}
static unsigned char __fastcall merit_person_read(Obj person,Obj archive) { COST_SCOPE("merit_person_read");
    unsigned old=word(person,0x10);bool reading=dword(archive,0x20)==1;
    bool old_owned=enabled && reading && merit_is_world_person(person,old);
    using Read=unsigned char(__fastcall *)(Obj,Obj);
    unsigned char result=next_or<Read>(next_merit_person_read,GA(GA_2390B0))(person,archive);
    if(enabled && reading && result) {
        unsigned id=word(person,0x10);bool new_owned=merit_is_world_person(person,id);
        if(new_owned && person[0x1C2] && person[0x1C3])merit_dual_note(person);
        if(old_owned || new_owned) {
            AcquireSRWLockExclusive(&merit_lock);
            if(old_owned)merit_bonds.erase_person(old);if(new_owned)merit_bonds.erase_person(id);
            ReleaseSRWLockExclusive(&merit_lock);
        }
    }
    return result;
}
#include "native_merit_scroll.inl"
static void __fastcall merit_profile(Obj profile,Obj person) { COST_SCOPE("merit_profile");
    for(auto &state:merit_scrolls)if(state.profile==profile)state.visible=false;
    merit_scroll_reset_input();
    // Render the eight ordinary native friends with their original slots.
    // Gameplay enumeration outside this scope still includes merit bonds.
    struct ProfileScope {ProfileScope(){++in_merit_profile;}~ProfileScope(){--in_merit_profile;}};
    {ProfileScope scope;next_or<void(__fastcall *)(Obj,Obj)>(next_merit_profile,GA(GA_908820))(profile,person);}
    if(!enabled || !mod_features.merit_friendship || !valid(person) || person[0xE6]==7 || person[0xE6]==9)return;
    try {
        // Match the native biography visibility predicates.
        if(fn<int(__fastcall *)(Obj)>(GA(GA_2323D0))(person) || fn<int(__fastcall *)(Obj)>(GA(GA_232400))(person))return;
        const wchar_t *original=fn<const wchar_t *(__fastcall *)(unsigned)>(GA(GA_30C020))(word(person,0x10));
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
            const wchar_t *name=fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_22EEC0))(other);
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
