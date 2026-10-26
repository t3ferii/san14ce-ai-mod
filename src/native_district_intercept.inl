// [combat] district_intercept_block (installer: 게임성 tab, convenience group).
// Player corps policy for NEW departures of a player-delegated corps (not the
// ruler's direct corps). Evidence: research/ai_trace/지원내정군단_요격출진경로_20261020.md
// and v12.17 re-check of bt_exce.bin: a player corps always takes the shared
// branch N711 (policy==0 OR player corps), which holds every departure node:
// CDepartureNode 34F10 (interception), CCommandAutoPaintDefenseDepartureNode
// 2C230 (via 27A40), area occupy/try-get, foothold attack (only this one checks
// the corps permission bit) and transports. All of them end in the dispatcher
// 1EC890, so the policy is judged there, from the command itself:
//   corps +17 (위임방침) = 5 지원: transport only (actions 2/3).
//   corps +17 (위임방침) = 4 내정: no military departure (move/occupy 1/14,
//       attack 4/5/7); transport and construction stay.
// +17 measured read-only in the live game (2026-10-20, 제2군단 이엄: 내정 4 -> 지원 5;
// tools/corps_policy_probe.py). UI order suggests 전체위임 1, 세력공략 2, 거점공략 3.
// v12.16/12.17 wrongly used +15 (the BT 행동설정 byte, 0 for both policies).
// Owner of the 1EC890 hook: the combat module when it is loaded (sortie_dispatch
// calls blocked() first); otherwise SAN14_Convenience.dll publishes its own
// entry hook (native_district_intercept_link.inl). Never both: the convenience module checks for the combat DLL.
namespace district_intercept {
using Dispatch=Obj(__fastcall *)(unsigned *,int);
enum Verdict {Allow,BlockSupport,BlockDomestic};
constexpr unsigned kPolicyDomestic=4,kPolicySupport=5; // corps +17
static bool military(unsigned action) {return action==1 || action==4 || action==5 || action==7 || action==14;}
#ifdef RUNTIME_COST_PROFILE
// Diagnostic build only: per corps policy and command action, how many departures
// reached the dispatcher and what the policy decided (calls column of the cost log).
static void count(unsigned policy,unsigned action,bool blocked) {
    if(policy==5 && blocked==false)switch(action) {
        case 0:{COST_SCOPE("dept_support_allow_a0");break;}
        case 1:{COST_SCOPE("dept_support_allow_a1");break;}
        case 2:{COST_SCOPE("dept_support_allow_a2");break;}
        case 3:{COST_SCOPE("dept_support_allow_a3");break;}
        case 4:{COST_SCOPE("dept_support_allow_a4");break;}
        case 5:{COST_SCOPE("dept_support_allow_a5");break;}
        case 6:{COST_SCOPE("dept_support_allow_a6");break;}
        case 7:{COST_SCOPE("dept_support_allow_a7");break;}
        case 8:{COST_SCOPE("dept_support_allow_a8");break;}
        case 9:{COST_SCOPE("dept_support_allow_a9");break;}
        case 10:{COST_SCOPE("dept_support_allow_a10");break;}
        case 11:{COST_SCOPE("dept_support_allow_a11");break;}
        case 12:{COST_SCOPE("dept_support_allow_a12");break;}
        case 13:{COST_SCOPE("dept_support_allow_a13");break;}
        case 14:{COST_SCOPE("dept_support_allow_a14");break;}
        case 15:{COST_SCOPE("dept_support_allow_a15");break;}
        default:{COST_SCOPE("dept_support_allow_other");break;}
    }
    if(policy==5 && blocked==true)switch(action) {
        case 0:{COST_SCOPE("dept_support_block_a0");break;}
        case 1:{COST_SCOPE("dept_support_block_a1");break;}
        case 2:{COST_SCOPE("dept_support_block_a2");break;}
        case 3:{COST_SCOPE("dept_support_block_a3");break;}
        case 4:{COST_SCOPE("dept_support_block_a4");break;}
        case 5:{COST_SCOPE("dept_support_block_a5");break;}
        case 6:{COST_SCOPE("dept_support_block_a6");break;}
        case 7:{COST_SCOPE("dept_support_block_a7");break;}
        case 8:{COST_SCOPE("dept_support_block_a8");break;}
        case 9:{COST_SCOPE("dept_support_block_a9");break;}
        case 10:{COST_SCOPE("dept_support_block_a10");break;}
        case 11:{COST_SCOPE("dept_support_block_a11");break;}
        case 12:{COST_SCOPE("dept_support_block_a12");break;}
        case 13:{COST_SCOPE("dept_support_block_a13");break;}
        case 14:{COST_SCOPE("dept_support_block_a14");break;}
        case 15:{COST_SCOPE("dept_support_block_a15");break;}
        default:{COST_SCOPE("dept_support_block_other");break;}
    }
    if(policy==4 && blocked==false)switch(action) {
        case 0:{COST_SCOPE("dept_domestic_allow_a0");break;}
        case 1:{COST_SCOPE("dept_domestic_allow_a1");break;}
        case 2:{COST_SCOPE("dept_domestic_allow_a2");break;}
        case 3:{COST_SCOPE("dept_domestic_allow_a3");break;}
        case 4:{COST_SCOPE("dept_domestic_allow_a4");break;}
        case 5:{COST_SCOPE("dept_domestic_allow_a5");break;}
        case 6:{COST_SCOPE("dept_domestic_allow_a6");break;}
        case 7:{COST_SCOPE("dept_domestic_allow_a7");break;}
        case 8:{COST_SCOPE("dept_domestic_allow_a8");break;}
        case 9:{COST_SCOPE("dept_domestic_allow_a9");break;}
        case 10:{COST_SCOPE("dept_domestic_allow_a10");break;}
        case 11:{COST_SCOPE("dept_domestic_allow_a11");break;}
        case 12:{COST_SCOPE("dept_domestic_allow_a12");break;}
        case 13:{COST_SCOPE("dept_domestic_allow_a13");break;}
        case 14:{COST_SCOPE("dept_domestic_allow_a14");break;}
        case 15:{COST_SCOPE("dept_domestic_allow_a15");break;}
        default:{COST_SCOPE("dept_domestic_allow_other");break;}
    }
    if(policy==4 && blocked==true)switch(action) {
        case 0:{COST_SCOPE("dept_domestic_block_a0");break;}
        case 1:{COST_SCOPE("dept_domestic_block_a1");break;}
        case 2:{COST_SCOPE("dept_domestic_block_a2");break;}
        case 3:{COST_SCOPE("dept_domestic_block_a3");break;}
        case 4:{COST_SCOPE("dept_domestic_block_a4");break;}
        case 5:{COST_SCOPE("dept_domestic_block_a5");break;}
        case 6:{COST_SCOPE("dept_domestic_block_a6");break;}
        case 7:{COST_SCOPE("dept_domestic_block_a7");break;}
        case 8:{COST_SCOPE("dept_domestic_block_a8");break;}
        case 9:{COST_SCOPE("dept_domestic_block_a9");break;}
        case 10:{COST_SCOPE("dept_domestic_block_a10");break;}
        case 11:{COST_SCOPE("dept_domestic_block_a11");break;}
        case 12:{COST_SCOPE("dept_domestic_block_a12");break;}
        case 13:{COST_SCOPE("dept_domestic_block_a13");break;}
        case 14:{COST_SCOPE("dept_domestic_block_a14");break;}
        case 15:{COST_SCOPE("dept_domestic_block_a15");break;}
        default:{COST_SCOPE("dept_domestic_block_other");break;}
    }
}
#else
static void count(unsigned,unsigned,bool) {}
#endif
#ifdef RUNTIME_COST_PROFILE
// Diagnostic build only: one CSV line per departure of a player-force officer, written at
// once (rare event; the batched cost log may never flush it). data/performance/departures_<pid>.csv
static SRWLOCK diag_lock=SRWLOCK_INIT;
static void diag_line(const char *text) {
    wchar_t path[MAX_PATH];if(!GetModuleFileNameW(nullptr,path,MAX_PATH))return;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return;
    swprintf_s(slash+1,MAX_PATH-static_cast<size_t>(slash+1-path),L"AI_TacticsMod\\data\\performance\\departures_%lu.csv",GetCurrentProcessId());
    AcquireSRWLockExclusive(&diag_lock);
    HANDLE f=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f!=INVALID_HANDLE_VALUE) {DWORD n=0;WriteFile(f,text,static_cast<DWORD>(strlen(text)),&n,nullptr);CloseHandle(f);}
    ReleaseSRWLockExclusive(&diag_lock);
}
static void diag_departure(Obj district,unsigned corps_index,const unsigned *command,int verdict) {
    if(!fn<int(__fastcall *)(Obj)>(0x233750)(district))return;
    char line[160];
    _snprintf_s(line,sizeof(line),_TRUNCATE,"%llu,%lu,corps%u,no%u,policy%u,perm%02X,person%u,action%u,kind%u,target%u,soldiers%u,verdict%d\r\n",
        GetTickCount64(),GetCurrentThreadId(),corps_index,district[0x11],district[0x17],district[0x1B],command[0],command[13],command[14],command[15],command[2],verdict);
    diag_line(line);
}
#else
static void diag_departure(Obj,unsigned,const unsigned *,int) {}
static void diag_line(const char *) {}
#endif
// Player-delegated corps with policy 지원/내정 (not the ruler's direct corps).
static bool restricted_corps(Obj district) {
    if(!valid(district) || (district[0x17]!=kPolicySupport && district[0x17]!=kPolicyDomestic))return false;
    if(!fn<int(__fastcall *)(Obj)>(0x233750)(district))return false;
    Obj root=world(),context=root?pointer(root,0x871E8):nullptr;
    if(!context)return false;
    Obj direct=fn<Obj(__fastcall *)(Obj)>(0x32E8D0)(context);
    return valid(direct) && district!=direct;
}
static Verdict judge(const unsigned *command) {
    if(!enabled || !mod_features.district_intercept_block || !command || !command[0])return Allow;
    Obj person=table(0x158,command[0],6000);
    if(!valid(person))return Allow;
    Obj district=table(0xE0D0,person[0xE0],51);
    if(!valid(district))return Allow;
    const unsigned policy=district[0x17],action=command[13];
    if(policy!=kPolicySupport && policy!=kPolicyDomestic) {diag_departure(district,person[0xE0],command,-1);return Allow;} // other policies and unknown values pass
    if(!fn<int(__fastcall *)(Obj)>(0x233750)(district))return Allow;   // player force, not computer
    Obj root=world(),context=root?pointer(root,0x871E8):nullptr;
    if(!context)return Allow;
    Obj direct=fn<Obj(__fastcall *)(Obj)>(0x32E8D0)(context);
    // Missing/unknown direct-corps context must not suppress a player's command.
    if(!valid(direct) || district==direct)return Allow;
    Verdict v=policy==kPolicySupport?(action==2 || action==3?Allow:BlockSupport):(military(action)?BlockDomestic:Allow);
    count(policy,action,v!=Allow);
    diag_departure(district,person[0xE0],command,v);
    return v;
}
static bool blocked(const unsigned *command) {
    Verdict v=judge(command);
    if(v!=Allow)log_event(v==BlockSupport?"district_support_departure_blocked":"district_domestic_departure_blocked",
                          command[0],command[13],command[14]);
    return v!=Allow;
}
}
