// [officer_ui] dispatch_days (2026-10-23, user request; behaviour of reference_mod SAN14_DispatchDays,
// re-derived from the original). The right-side dispatch list shows "N일" after the destination:
// days until the officer is back (remaining mission + return trip).
// Original (REA, unpacked 1fbe6be7...):
//   897FA0(row) person = row+178, unit = 22AA70(person); destination text CALL 930D20(row+130,3,text)
//   at 898083; gauge = unit vtable+A8 (end-start days) - vtable+B0 (today-start), as the game draws it.
//   Return trip (1F6720 builds it): days = 231510(vtable+118(unit) tile, 22D500(home base) tile, list, 0).
//   Units without a return leg: trade 13A28B8 (enters the city on its last day), person move 13A1D30
//   (arrival city is the new home), return trip 13A1E78 (already returning).
// Mod: that text call gets " N일" appended: the remaining mission days (end - today).
// 2026-10-23: the return-trip estimate was removed. vtable+118 is the position getter only on the
// return-trip unit 1F6720 builds; on search/recruit units it is another method, and calling it
// crashed the game in 32F500 right after a dispatch (user reports, WER offset 32F509).
namespace dispatch_days {
static const unsigned kTextCall=0x898083,kText=0x930D20;
static const unsigned kNoReturn[3]={0x13A28B8,0x13A1D30,0x13A1E78};
struct Cache {Obj unit;int day,days;};
static Cache cache[16];
static bool guards_match() {
    static const unsigned char before[]={0x41,0x8D,0x51,0x02};   // lea edx,[r9+2] (element 3)
    return image[kTextCall]==0xE8 && kTextCall+5+*reinterpret_cast<int *>(image+kTextCall+1)==kText &&
        !memcmp(image+kTextCall-4,before,4);
}
static int virtual_int(Obj o,unsigned slot) {return (*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(o,0)+slot))(o);}
[[maybe_unused]] static int return_days(Obj unit,Obj person) {
    for(unsigned vt:kNoReturn)if(pointer(unit,0)==image+vt)return 0;
    int today=game_day();
    for(auto &c:cache)if(c.unit==unit && c.day==today)return c.days;
    int days=0;
    Obj home=word(person,0xE2)<0x3E?fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),word(person,0xE2)):nullptr;
    if(valid(home)) {
        Obj from=(*reinterpret_cast<Obj(__fastcall **)(Obj)>(pointer(unit,0)+0x118))(unit);
        Obj to=fn<Obj(__fastcall *)(Obj)>(0x22D500)(home);
        if(from && to) {
            GameList list;list_init(list);
            days=fn<int(__fastcall *)(Obj,Obj,GameList *,int)>(0x231510)(from,to,&list,0);
            list_free(list);if(days<0)days=0;
        }
    }
    static unsigned next;cache[next++%16]={unit,today,days};
    return days;
}
static void __fastcall text(Obj set,int element,const wchar_t *value,int flag) {
    auto original=fn<void(__fastcall *)(Obj,int,const wchar_t *,int)>(kText);
    if(!mod_features.dispatch_days || !value)return original(set,element,value,flag);
    Obj person=pointer(set-0x130,0x178);
    if(!valid(person))return original(set,element,value,flag);
    Obj unit=fn<Obj(__fastcall *)(Obj)>(0x22AA70)(person);
    if(!unit)return original(set,element,value,flag);
    int left=virtual_int(unit,0xA8)-virtual_int(unit,0xB0);if(left<0)left=0;
    int days=left;   // remaining mission days only (2026-10-23: return estimate removed, see header)
    wchar_t buffer[96];
    if(swprintf_s(buffer,L"%s %d일",value,days)<0)return original(set,element,value,flag);
    original(set,element,buffer,flag);
}
static CallLink link;
static bool install() {
    if(!guards_match()) {log_event("dispatch_days_guard_rejected");return false;}
    Obj relay=allocate_near(image+kTextCall);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&text));
    DWORD old;
    if(!call_prepare(link,kTextCall,relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,16)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    if(!call_publish(link))return false;
    log_event("dispatch_days_installed");return true;
}
}
