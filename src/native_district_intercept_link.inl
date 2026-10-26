// Convenience-owned 1EC890 entry hook for the corps policy, only when the combat
// module is absent (see native_district_intercept.inl).
namespace district_intercept {
// Convenience-owned entry hook, only when the combat module is absent.
static Dispatch original_dispatch;
static Obj __fastcall dispatch(unsigned *command,int spend) { COST_SCOPE("dept_dispatch_all");
    if(blocked(command))return nullptr; // native callers treat nullptr as a failed departure
    return original_dispatch(command,spend);
}
static const unsigned char kEntry[16]={0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0x6C,0x24};
static bool combat_module_loaded() {return GetModuleHandleW(L"SAN14_CombatAI.dll")!=nullptr;}
static bool prepare(CodeLink &link,Obj relay) {
    if(combat_module_loaded()) {diag_line("0,0,entry_hook_skipped_combat_module_loaded\r\n");return false;}
    if(memcmp(image+0x1EC890,kEntry,sizeof(kEntry))) {diag_line("0,0,entry_hook_skipped_bytes_differ\r\n");return false;}
    Obj stub=relay+3328,gate=stub+32;                       // same entry layout as the combat sortie gate
    absolute_jump(stub,reinterpret_cast<void *>(&dispatch));
    memcpy(gate,kEntry,6);absolute_jump(gate+6,image+0x1EC896);
    original_dispatch=reinterpret_cast<Dispatch>(gate);
    if(!prepare_link(link,0x1EC890,0xE9,stub))return false;
    diag_line("0,0,entry_hook_prepared\r\n");
    link.modified[0x1EC890-link.block+5]=0x90;              // the sixth displaced byte
    return true;
}
// Transport sources (v12.19). Native condition 84 = AEE60(context, subject, args):
// "no hostile army within args[3] hexes of object args[1] (kind args[0])"
// (decoded: 2AE130 collects armies around the hex, 22C850 -> 2C5050 tests hostility).
// The corps BT uses (5 city, 15) to admit a city to its transport-source list (N1861),
// and to the spare-troop city list 0565. A player 지원/내정 corps may not intercept
// (policy above), so the enemy near its city stays and the native rule excluded that
// city from transports forever. For exactly those corps and that (5, 15) use only,
// the condition is true; radius 10, hex/gate kinds and every other corps stay native.
// Hooked through the single CALL in condition dispatcher C2610's case 0x54 (C2F73).
using Condition=unsigned char(__fastcall *)(Obj,Obj,const unsigned *,unsigned);
static unsigned char __fastcall transport_safety(Obj context,Obj subject,const unsigned *args,unsigned extra) {
    if(enabled && mod_features.district_intercept_block && args && args[0]==5 && args[3]==15) {
        Obj corps=fn<Obj(__fastcall *)(Obj,Obj)>(0xB60E0)(context,subject);   // same lookup as condition 26
        if(restricted_corps(corps)) {COST_SCOPE("transport_safety_override");return 1;}
    }
    return fn<Condition>(0xAEE60)(context,subject,args,extra);
}
static const unsigned char kSafetyBlock[16]={0x49,0x8B,0xCA,0xE8,0xE8,0xBE,0xFE,0xFF,0x0F,0xB6,0xD0,0x48,0x83,0xC4,0x20,0x5D};
static const unsigned char kSafetyEntry[10]={0x40,0x57,0x41,0x56,0x41,0x57,0x48,0x83,0xEC,0x40};
static const unsigned char kCorpsEntry[10]={0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20};
static bool prepare_safety(CodeLink &link,Obj relay) {
    if(memcmp(image+0xC2F70,kSafetyBlock,16) || memcmp(image+0xAEE60,kSafetyEntry,10) || memcmp(image+0xB60E0,kCorpsEntry,10))return false;
    absolute_jump(relay+3392,reinterpret_cast<void *>(&transport_safety));
    return prepare_link(link,0xC2F73,0xE8,relay+3392);
}
}
