// [game_flow] caravan_stay (2026-10-23, user request; behaviour of reference_mod SAN14_CaravanStay,
// re-derived from the original, no reference code). Trade members keep their own city.
// Original (REA, unpacked 1fbe6be7...):
//   1F44A0 trade send (only caller 1EA8C0, which builds the 0x88-byte trade unit): per member
//     CALL 270ED0(&list{person}, lord city id) at 1F479E moves him to the lord's city
//     (270ED0 = move persons to base: governor hand-over, district change, Person+E2 = base).
//   1DA530 dispatch arrival: CALL 32AAE0(unit, lord city of the unit's force) at 1DA742 moves every
//     member (unit+20 id list, pool 1B5C9A0) to that city. Trade unit kind (vtable+98) = 0x12.
//   293050 city handed to/from a corps: CALL 334780(unit) at 29319E cancels the dispatch of each
//     person of the city (334780 = vtable+90, +110(unit,1)).
// Mod: the send move is skipped (members stay home, like other dispatches); on arrival the original
// runs, then each member whose recorded home is still a city of the same force is moved back there
// with the same 270ED0; a trade unit is not cancelled by the corps hand-over. Other dispatch kinds
// and commands are unchanged; members who left before the switch return to the lord city (home = it).
namespace caravan_stay {
static const unsigned kSendCall=0x1F479E,kArriveCall=0x1DA742,kCancelCall=0x29319E;
static const unsigned kMove=0x270ED0,kArrive=0x32AAE0,kCancel=0x334780,kTradeKind=0x12;
static bool call_to(unsigned site,unsigned target) {
    return image[site]==0xE8 && site+5+*reinterpret_cast<int *>(image+site+1)==target;
}
static bool guards_match() {
    // Neighbouring instructions pin each site to the decompiled code path.
    static const unsigned char send[]={0x48,0x8D,0x4C,0x24,0x58},arrive[]={0x48,0x8B,0xD0,0x48,0x8B,0xCE},
        cancel[]={0x48,0x8B,0xCF};
    return call_to(kSendCall,kMove) && call_to(kArriveCall,kArrive) && call_to(kCancelCall,kCancel) &&
        !memcmp(image+kSendCall-5,send,5) && !memcmp(image+kArriveCall-6,arrive,6) &&
        !memcmp(image+kCancelCall-3,cancel,3);
}
static unsigned kind(Obj unit) {return valid(unit)?(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(unit,0)+0x98))(unit):0;}
static unsigned base_owner(Obj base) {return valid(base)?(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(base,0)+0xB0))(base):0;}
static Obj base(unsigned id) {return id<0x3E?fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),id):nullptr;}
static int __fastcall send_move(Obj list,int city) {   // result unused at 1F479E
    if(!mod_features.caravan_stay)return fn<int(__fastcall *)(Obj,int)>(kMove)(list,city);
    log_event("caravan_stay_send_kept",0,static_cast<unsigned>(city));
    return 1;
}
static bool __fastcall arrive(Obj unit,Obj city) {
    using Arrive=bool(__fastcall *)(Obj,Obj);
    if(!mod_features.caravan_stay || kind(unit)!=kTradeKind || !valid(city))return fn<Arrive>(kArrive)(unit,city);
    struct Member {Obj person;unsigned short home;} members[16];unsigned count=0;
    Obj pool=pointer(image,0x1B5C9A0);unsigned *handle=reinterpret_cast<unsigned *>(pointer(unit,0x20));
    if(pool && handle && *handle<dword(image,0x1B5C9D0))
        for(int *node=*reinterpret_cast<int **>(pool+*handle*8);node && count<16;node=*reinterpret_cast<int **>(node+2)) {
            int id=*node;if(id<1 || id>6000)continue;
            Obj person=table(0x158,static_cast<unsigned>(id),6000);
            if(valid(person))members[count++]={person,word(person,0xE2)};
        }
    bool result=fn<Arrive>(kArrive)(unit,city);
    unsigned lord=word(city,0x4E),owner=base_owner(city);
    for(unsigned i=0;i<count;++i) {
        Obj home=base(members[i].home);
        if(members[i].home==lord || !home || base_owner(home)!=owner || word(members[i].person,0xE2)!=lord)continue;
        GameList one;list_init(one,0x13436B0);
        if(list_add(one,members[i].person))fn<int(__fastcall *)(Obj,int)>(kMove)(reinterpret_cast<Obj>(&one),members[i].home);
        list_free(one);
        log_event("caravan_stay_home",word(members[i].person,0x10),members[i].home,lord);
    }
    return result;
}
static void __fastcall cancel(Obj unit) {
    if(mod_features.caravan_stay && kind(unit)==kTradeKind) {log_event("caravan_stay_corps_kept");return;}
    fn<void(__fastcall *)(Obj)>(kCancel)(unit);
}
static CallLink links[3];
static bool install() {
    if(!guards_match()) {log_event("caravan_stay_guard_rejected");return false;}
    Obj relay=allocate_near(image+kSendCall);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&send_move));
    absolute_jump(relay+32,reinterpret_cast<void *>(&arrive));
    absolute_jump(relay+64,reinterpret_cast<void *>(&cancel));
    DWORD old;
    if(!call_prepare(links[0],kSendCall,relay) || !call_prepare(links[1],kArriveCall,relay+32) ||
       !call_prepare(links[2],kCancelCall,relay+64) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,96)) {VirtualFree(relay,0,MEM_RELEASE);log_event("caravan_stay_prepare_rejected");return false;}
    for(unsigned i=0;i<3;++i)if(!call_publish(links[i])) {log_event("caravan_stay_publish_rejected",i);while(i)call_restore(links[--i]);return false;}
    log_event("caravan_stay_installed");return true;
}
}
