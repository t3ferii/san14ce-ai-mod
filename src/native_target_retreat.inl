// A retreat command alone does not make a nearby enemy cease to exist.
// Keep original damage, morale, depleted-unit and safety decisions intact.
#include "native_target_retreat_profile.h"
using TargetRetreatDaily=void(__fastcall *)(Obj);
static TargetRetreatDaily original_target_retreat_daily;
static volatile LONG target_retreat_active;
static bool target_retreat_nearby_enemy(Obj army) {
    if(!campaign_controlled(army) || army[0x37]!=7 || army[0x38]!=27 ||
       army[0x23] || army[0x25] || word(army,0x16)<1000)return false;
    unsigned id=word(army,0x3A);
    if(!id || id>500)return false;
    // Do not hide an unrelated secondary target's disappearance/invalidity.
    if(army[0x3C]!=0xFF && (army[0x3C]!=27 || word(army,0x3E)!=id))return false;
    Obj enemy=table(0x7E838,id,500);
    if(!valid(enemy) || enemy==army || !word(enemy,0x16) ||
       fn<int(__fastcall *)(Obj)>(0x233440)(enemy))return false;
    if(enemy[0x37]!=11 && enemy[0x37]!=12)return false;
    if(!fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(army_force(army),army_force(enemy)))return false;
    unsigned own_hex=word(army,0x2A),enemy_hex=word(enemy,0x2A);
    return own_hex<48400 && enemy_hex<48400 && distance(own_hex,enemy_hex)<=6;
}
static void __fastcall target_retreat_daily(Obj army) {
    bool trace=diagnostic_logging && army && army[0x38]==27;
    unsigned before_order=trace?(army[0x37]+(static_cast<unsigned>(army[0x38])<<8)):0;
    unsigned before_target=trace?word(army,0x3A):0,enemy_action=255;
    if(trace && before_target>0 && before_target<=500) {
        Obj enemy=table(0x7E838,before_target,500);
        if(valid(enemy))enemy_action=enemy[0x37];
    }
    bool protect=target_retreat_active && target_retreat_nearby_enemy(army);
    unsigned char previous=protect?army[0x26]:0;
    // Native bit 8 is tested only at the two command-based target cancellation
    // clauses in the guarded function. Scope it to this caller, restore only
    // this bit, and preserve all other original flag changes even on SEH.
    unsigned target=protect?word(army,0x3A):0;
    if(protect)army[0x26]|=8;
    __try {original_target_retreat_daily(army);}
    __finally {if(protect)army[0x26]=static_cast<unsigned char>((army[0x26]&~8u)|(previous&8));}
    if(protect) {
        log_event(army[0x37]==7 && army[0x38]==27 && word(army,0x3A)==target?
            "target_retreat_nearby_attack_kept":"target_retreat_other_native_decision",army_id(army),target,word(army,0x2A));
    }
    if(trace && (before_order!=army[0x37]+(static_cast<unsigned>(army[0x38])<<8) || before_target!=word(army,0x3A))) {
        log_event("target_retreat_order_before",army_id(army),before_order,before_target);
        log_event("target_retreat_order_after",army_id(army),army[0x37]+(static_cast<unsigned>(army[0x38])<<8),word(army,0x3A));
        log_event("target_retreat_enemy_action_before",army_id(army),enemy_action,before_target);
    }
}
struct TargetRetreatCode {CodeLink daily,filters[2];};
static bool target_retreat_guards_match() {
    for(const auto &guard:target_retreat_guards)if(memcmp(image+guard.rva,guard.bytes,guard.size))return false;
    return true;
}
static bool prepare_target_retreat(TargetRetreatCode &code,Obj relay) {
    if(!target_retreat_guards_match())return false;
    absolute_jump(relay,reinterpret_cast<void *>(&target_retreat_daily));
    // MOV RAX,RSP; PUSH RBP; LEA RBP,[RAX-8E8h], complete 11-byte prolog.
    // RAX and argument registers survive the branch back to the native SUB.
    memcpy(relay+64,image+0x272190,11);
    relay[75]=0x49;relay[76]=0xBB;Obj continuation=image+0x27219B;
    memcpy(relay+77,&continuation,8);relay[85]=0x41;relay[86]=0xFF;relay[87]=0xE3;
    original_target_retreat_daily=reinterpret_cast<TargetRetreatDaily>(relay+64);
    auto runtime=reinterpret_cast<RUNTIME_FUNCTION *>(relay+96);
    runtime->BeginAddress=64;runtime->EndAddress=88;runtime->UnwindData=112;
    const unsigned char unwind[]={1,11,1,0,4,0x50,0,0};
    memcpy(relay+112,unwind,sizeof(unwind));
    if(!prepare_link(code.daily,0x272190,0xE9,relay))return false;
    memset(code.daily.modified+5,0x90,6);
    // Original LEA EAX,[RCX-11]; CMP AL,1; JBE rejects actions 11/12.
    // MOV AL,2; NOP removes only that test, retaining ECX for the following
    // action 2/3 + actual home-region exclusion. EAX is overwritten before
    // later use on both paths. All validity/range/risk scoring stays native.
    const unsigned char command_is_not_disappearance[]={0xB0,2,0x90};
    return prepare_bytes(code.filters[0],0x6A26A,command_is_not_disappearance,3) &&
           prepare_bytes(code.filters[1],0x6A948,command_is_not_disappearance,3);
}
static void restore_target_retreat(TargetRetreatCode &code) {
    InterlockedExchange(&target_retreat_active,0);
    restore_link(code.daily);restore_link(code.filters[1]);restore_link(code.filters[0]);
}
static bool publish_target_retreat(TargetRetreatCode &code) {
    // Filters first, daily wrapper last; no partially enabled policy.
    for(auto &filter:code.filters)if(!publish_link(filter)) {restore_target_retreat(code);return false;}
    if(!publish_link(code.daily)) {restore_target_retreat(code);return false;}
    InterlockedExchange(&target_retreat_active,1);return true;
}
static bool install_target_retreat() {
    wchar_t config[MAX_PATH];
    if(!GetModuleFileNameW(self_module,config,MAX_PATH))return false;
    wchar_t *slash=wcsrchr(config,L'\\');if(!slash)return false;*slash=0;
    if(wcscat_s(config,L"\\AI_TacticsMod\\target_retreat.ini"))return false;
    if(!GetPrivateProfileIntW(L"target_retreat",L"enabled",1,config)) {log_event("target_retreat_disabled");return true;}
    if(!target_retreat_guards_match()) {log_event("target_retreat_guard_rejected");return false;}
    Obj relay=allocate_near(image+0x272190);if(!relay)return false;
    TargetRetreatCode code;
    if(!prepare_target_retreat(code,relay)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    DWORD old;
    if(!VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,120) ||
       !RtlAddFunctionTable(reinterpret_cast<RUNTIME_FUNCTION *>(relay+96),1,reinterpret_cast<DWORD64>(relay))) {
        VirtualFree(relay,0,MEM_RELEASE);return false;
    }
    // Retain gate and unwind metadata after any publication attempt: another
    // original game thread could already be inside the wrapper/gate.
    if(!publish_target_retreat(code)) {log_event("target_retreat_publish_rejected");return false;}
    log_event("target_retreat_hooks_installed",0,3,6);return true;
}
