// Convenience DLL owns this player sortie UI feature and its settings.
// Only the native CStrategyUnitMakeState's pending records are changed.
#include "unified_tactic_settings.h"
#include <map>
namespace tactic_priority {
static SRWLOCK settings_lock=SRWLOCK_INIT;
static unified::Priorities settings;
static unsigned generation;
struct Row {unsigned person,formation,generation;};
static Obj active_state,active_world;
static std::map<Obj,Row> seen;
static void load() {
    settings=unified_config::load();generation=1;
}
static void clear() {seen.clear();active_state=active_world=nullptr;}
static bool set(unsigned version,const int *martial,unsigned count_m,const int *intellect,unsigned count_i,int enable) {
    if(version!=1 || !martial || !intellect || !count_m || !count_i || count_m>203 || count_i>203 || (enable!=0 && enable!=1))return false;
    unified::Priorities next;next.enabled=enable!=0;
    next.martial.assign(martial,martial+count_m);next.intellect.assign(intellect,intellect+count_i);
    for(const auto *list:{&next.martial,&next.intellect})for(int id:*list)if(!unified_config::valid_entry(id))return false;
    AcquireSRWLockExclusive(&settings_lock);settings=std::move(next);++generation;ReleaseSRWLockExclusive(&settings_lock);return true;
}
static void apply(Obj state) {
    unified::Priorities preference;unsigned revision;
    AcquireSRWLockShared(&settings_lock);preference=settings;revision=generation;ReleaseSRWLockShared(&settings_lock);
    if(!enabled || !preference.enabled){clear();return;}
    Obj root=world();if(!root)return;
    if(active_state!=state || active_world!=root){clear();active_state=state;active_world=root;}
    Obj begin=pointer(state,0x540),end=pointer(state,0x548);
    uintptr_t first=reinterpret_cast<uintptr_t>(begin),last=reinterpret_cast<uintptr_t>(end);
    if(!begin || last<first || (last-first)%8 || (last-first)/8>6000)return;
    Obj options=pointer(root,0x871E8),player=options?fn<Obj(__fastcall *)(Obj)>(0x32E910)(options):nullptr;
    if(!valid(player))return;
    const auto catalog=unified_config::catalog();std::set<Obj> present;
    for(size_t offset=0;offset<last-first;offset+=8) {
        Obj row=pointer(begin,static_cast<unsigned>(offset));if(!row)continue;present.insert(row);
        // 7EBBBC..7EBBC4 passes row+4 to native departure 1EC890.
        Obj command=row+4;unsigned id=dword(command,0),formation=dword(command,12);
        if(!id || id>6000 || formation>20)continue;
        auto previous=seen.find(row);
        if(previous!=seen.end() && previous->second.person==id && previous->second.formation==formation && previous->second.generation==revision)continue;
        Obj person=table(0x158,id,6000);if(!valid(person) || personnel_force(person)!=player)continue;
        unified::Officer data;data.id=id;
        data.abilities[1]=fn<int(__fastcall *)(Obj,unsigned,unsigned)>(0x22FCE0)(person,1,0xFFF);
        data.abilities[2]=fn<int(__fastcall *)(Obj,unsigned,unsigned)>(0x22FCE0)(person,2,0xFFF);
        for(unsigned i=0;i<10;++i) {
            unsigned tactic=person[0x130+i];Obj entry=table(0x774D0,tactic,200);
            // Same formation compatibility bit test as native D0192..D01A9.
            if(tactic && tactic<=200 && valid(entry) && (dword(entry,0x6C)&(1u<<formation)))data.tactics[i]=tactic;
        }
        auto chosen=unified::select_tactics(data,catalog,preference.for_person(data));
        if(chosen[0])for(unsigned i=0;i<3;++i)*reinterpret_cast<unsigned *>(command+20+i*4)=chosen[i];
        seen[row]={id,formation,revision};
    }
    for(auto it=seen.begin();it!=seen.end();)if(!present.count(it->first))it=seen.erase(it);else ++it;
}
static void __fastcall update(Obj state) {
    apply(state);fn<void(__fastcall *)(Obj)>(0x7DF540)(state);
}
static uint64_t __fastcall leave(Obj state,Obj next) {
    if(active_state==state)clear();return fn<uint64_t(__fastcall *)(Obj,Obj)>(0x7D9E50)(state,next);
}
}
