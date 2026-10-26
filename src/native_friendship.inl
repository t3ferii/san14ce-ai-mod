/* Supplemental friendship bindings, not published by the current installer.
 * Save/load publication and the expanded relationship screen remain required
 * before these callbacks can be enabled in a distributed game DLL. */
#include "friendship_store.h"
using FriendAdd=void(__fastcall *)(Obj,Obj);
using FriendHas=int(__fastcall *)(Obj,Obj);
using FriendList=unsigned(__fastcall *)(Obj,GameList *,int);
using FriendOne=void(__fastcall *)(Obj);
static FriendAdd original_friend_add,original_friend_remove;
static FriendHas original_friend_has;
static FriendList original_friend_list;
static FriendOne original_friend_cleanup,original_friend_normalize,original_friend_reset;
static friendship::Store expanded_friends;
static SRWLOCK friendship_lock=SRWLOCK_INIT;
static Obj friendship_world;
static friendship::Native friendship_native(Obj officer) {
    friendship::Native values={};
    if(valid(officer))memcpy(values.data(),officer+0xC0,sizeof(values));
    return values;
}
static void friendship_check_world() {
    // Invoked while the relationship lock is held on a native game thread.
    // A same-address load additionally requires an explicit successful-load
    // publication; pointer comparison alone is never a save identity check.
    Obj current=world();
    if(current!=friendship_world) {expanded_friends.clear();friendship_world=current;}
}
[[maybe_unused]] static int __fastcall friendship_has(Obj officer,Obj other) {
    int native=original_friend_has(officer,other);
    if(native || !enabled || !valid(officer) || !valid(other))return native;
    unsigned owner=word(officer,0x10),target=word(other,0x10);
    if(!friendship::valid_id(owner) || !friendship::valid_id(target))return native;
    AcquireSRWLockExclusive(&friendship_lock);friendship_check_world();
    bool found=expanded_friends.contains(owner,target,friendship_native(officer));
    ReleaseSRWLockExclusive(&friendship_lock);return found?1:0;
}
[[maybe_unused]] static void __fastcall friendship_add(Obj officer,Obj other) {
    if(!enabled || !valid(officer) || !valid(other)) {original_friend_add(officer,other);return;}
    unsigned owner=word(officer,0x10),target=word(other,0x10);
    friendship::AddResult result=friendship::AddResult::Invalid;
    AcquireSRWLockExclusive(&friendship_lock);
    try {
        friendship_check_world();auto native=friendship_native(officer);
        result=expanded_friends.can_add(owner,target,native);
        if(result==friendship::AddResult::Added) {
            // Keep the original eight-slot operation for a free native slot.
            // The native function has no calls, so this cannot recursively
            // acquire the predicate lock. Overflow is kept outside Person.
            original_friend_add(officer,other);
            native=friendship_native(officer);
            if(!expanded_friends.contains(owner,target,native))
                result=expanded_friends.add(owner,target,native);
        }
    } catch(...) {result=friendship::AddResult::Invalid;}
    ReleaseSRWLockExclusive(&friendship_lock);
    if(result==friendship::AddResult::Full)log_event("friendship_500_capacity_reached",owner,target);
}
[[maybe_unused]] static void __fastcall friendship_remove(Obj officer,Obj other) {
    original_friend_remove(officer,other);
    if(!enabled || !valid(officer) || !valid(other))return;
    AcquireSRWLockExclusive(&friendship_lock);friendship_check_world();
    expanded_friends.remove(word(officer,0x10),word(other,0x10));
    ReleaseSRWLockExclusive(&friendship_lock);
}
[[maybe_unused]] static bool friendship_mutual(Obj first,Obj second) {
    if(!enabled || !valid(first) || !valid(second))return false;
    unsigned first_id=word(first,0x10),second_id=word(second,0x10);bool success=false;
    AcquireSRWLockExclusive(&friendship_lock);
    try {
        friendship_check_world();
        success=expanded_friends.add_mutual(first_id,second_id,
            friendship_native(first),friendship_native(second));
        if(success) {original_friend_add(first,second);original_friend_add(second,first);}
    } catch(...) {success=false;}
    ReleaseSRWLockExclusive(&friendship_lock);return success;
}
[[maybe_unused]] static unsigned __fastcall friendship_list(Obj officer,GameList *out,int templates) {
    unsigned count=original_friend_list(officer,out,templates);
    if(!enabled || !valid(officer) || !out)return count;
    std::vector<uint16_t> ids;
    AcquireSRWLockExclusive(&friendship_lock);
    try {friendship_check_world();ids=expanded_friends.list(word(officer,0x10),friendship_native(officer));}
    catch(...) {ReleaseSRWLockExclusive(&friendship_lock);return count;}
    ReleaseSRWLockExclusive(&friendship_lock);
    for(unsigned id:ids) {
        // Preserve the native configuration-screen conversion for custom
        // templates 5101..5200 instead of confusing them with runtime Person.
        Obj other=templates && id>=5101 && id<5201?
            table(0x84F30,id-5101,99):table(0x158,id,6000);
        if(!valid(other))continue;
        bool exists=false;unsigned visited=0;
        for(Link *link=list_first(*out);link && visited++<6000;link=link->next)
            if(link->value==other){exists=true;break;}
        if(!exists && list_add(*out,other))++count;
    }
    return count;
}
static void friendship_prune(Obj officer,bool custom_aware) {
    if(!enabled || !valid(officer))return;
    AcquireSRWLockExclusive(&friendship_lock);
    try {
        friendship_check_world();unsigned owner=word(officer,0x10);
        auto ids=expanded_friends.list(owner,friendship_native(officer));
        for(unsigned id:ids) {
            Obj other=table(0x158,id,6000);
            // Match the two different native maintenance predicates.
            bool remove=!valid(other);
            if(!remove)remove=custom_aware && !(id>=3001 && id<3151)?other[0xE6]==9:
                                            (other[0xE6]==7 || other[0xE6]==9);
            if(remove)expanded_friends.remove(owner,id);
        }
    } catch(...) {ReleaseSRWLockExclusive(&friendship_lock);return;}
    ReleaseSRWLockExclusive(&friendship_lock);
}
[[maybe_unused]] static void __fastcall friendship_cleanup(Obj officer) {
    original_friend_cleanup(officer);friendship_prune(officer,false);
}
[[maybe_unused]] static void __fastcall friendship_normalize(Obj officer) {
    original_friend_normalize(officer);
    if(enabled && valid(officer)) {
        unsigned id=word(officer,0x10);
        // Native 2E2330 leaves the 3001..3150 template owners alone.
        if(!(id>=3001 && id<3151))friendship_prune(officer,true);
    }
}
static bool friendship_is_world_person(Obj officer,unsigned id) {
    // This identity check only reads native storage. It is also usable by
    // the game's own load worker without calling a gameplay getter.
    Obj current=world();
    return current && friendship::valid_id(id) && pointer(current,0x158+id*8)==officer;
}
[[maybe_unused]] static void __fastcall friendship_reset(Obj officer) {
    unsigned old_id=word(officer,0x10);
    bool matched=enabled && friendship_is_world_person(officer,old_id);
    original_friend_reset(officer);
    if(matched) {
        AcquireSRWLockExclusive(&friendship_lock);expanded_friends.erase_person(old_id);
        ReleaseSRWLockExclusive(&friendship_lock);
    }
}
[[maybe_unused]] static unsigned char __fastcall friendship_person_read(Obj officer,Obj archive) {
    unsigned old_id=word(officer,0x10);bool reading=dword(archive,0x20)==1;
    bool old_match=enabled && reading && friendship_is_world_person(officer,old_id);
    unsigned char result=fn<unsigned char(__fastcall *)(Obj,Obj)>(0x2390B0)(officer,archive);
    if(enabled && reading && result) {
        unsigned new_id=word(officer,0x10);
        bool new_match=friendship_is_world_person(officer,new_id);
        if(old_match || new_match) {
            AcquireSRWLockExclusive(&friendship_lock);
            if(old_match)expanded_friends.erase_person(old_id);
            if(new_match)expanded_friends.erase_person(new_id);
            ReleaseSRWLockExclusive(&friendship_lock);
        }
    }
    return result;
}
