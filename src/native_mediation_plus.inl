// [officer_ui] mediation_plus (2026-10-23, user request; behaviour of reference_mod SAN14_MediationPlus,
// re-derived from the original): 중개 adds a third member to an existing sworn pair, lets a married man
// marry an unmarried woman (일부다처), and [혼인] on a married couple divorces them after a confirmation.
// Original (REA, unpacked 1fbe6be7...). Mediation view = state, selection +4B8 -> +1C0/+1C8/+1D0 persons,
// buttons +198. Person +10 id, +A9 sex (0 male, 1 female), +BA spouse, +BC sworn group id, +D0 u16[8]
// relatives, +E8 loyalty, +166 service start year.
//   70E9A0(view,person,slot) candidate filter, called at 6D7D90: married AND sworn -> hidden; loyalty
//     < 100 or service < [1A485C0] years -> only the ruler; then 711920(view,cand,a,b) at 70EADC:
//     relatives (2C3A90 both ways, +D0 lists), sex, both unmarried/unsworn, age >= [1A48D70] (2A5EA0).
//   796110 marriage button / 78D920 sworn button (85AAD0(+198, 2|1, enabled)), each called at
//     6BA697/7971B3 and 6BA69F/7971BB.
//   6D1A50 marriage (called 73502B): confirm (formatter 1A23440, text 58AC, 216090 shows it, result
//     56FC20(1100E0())+58), pays [1A485A8] gold (resources 22D3A0(force) vtable +90/+98/+B8) and
//     [1A4859C] orders (force +14), sets both +BA, refreshes (+4B8 vtable +190, 0x7FFFFFFD).
//   6D0550 sworn (called 735035): same costs, writes the leader id to +BC of the 2-3 members.
// Mod: every original check and command runs first; only what it rejects is re-judged by the extended
// rules, and the sworn result is completed by moving the old group's other members to the new id.
#include "game_addresses.h"
namespace mediation_plus {
static const unsigned kPairCall=GA(GA_70EADC),kMarryCalls[2]={GA(GA_6BA697),GA(GA_7971B3)},
    kSwornCalls[2]={GA(GA_6BA69F),GA(GA_7971BB)},kMarryExecCall=GA(GA_73502B),kSwornExecCall=GA(GA_735035);
static const unsigned kBlock=GA(GA_70E9C1),kPair=GA(GA_711920),kMarryButton=GA(GA_796110),kSwornButton=GA(GA_78D920),
    kMarryExec=GA(GA_6D1A50),kSwornExec=GA(GA_6D0550),kEnable=GA(GA_85AAD0);
static bool call_to(unsigned site,unsigned target) {
    return image[site]==0xE8 && site+5+*reinterpret_cast<int *>(image+site+1)==target;
}
// The CALL targets are judged by the Layout hook manager (chain over other mods, refuse foreign bytes at 70E9C1);
// here only the opcodes and the original 70E9C1 bytes are required before anything is registered.
static bool guards_match() {
    for(unsigned site:{kPairCall,kMarryCalls[0],kMarryCalls[1],kSwornCalls[0],kSwornCalls[1],kMarryExecCall,kSwornExecCall})
        if(image[site]!=0xE8)return false;
    return !ga::mcmp_at(image,kBlock,layout_mediation_expect,sizeof(layout_mediation_expect));
}
static unsigned short id_of(Obj p) {return word(p,0x10);}
static int age(Obj p) {return fn<int(__fastcall *)(Obj)>(GA(GA_2A5EA0))(p);}
static bool adult(Obj p) {return age(p)>=static_cast<int>(dword(image,GA(GA_1A48D70)));}
static bool related(Obj a,Obj b) {
    auto check=fn<int(__fastcall *)(Obj,Obj)>(GA(GA_2C3A90));
    if(check(a,b) || check(b,a))return true;
    for(unsigned i=0;i<8;++i)if(word(a,0xD0+i*2)==id_of(b) || word(b,0xD0+i*2)==id_of(a))return true;
    return false;
}
static bool married_pair(Obj a,Obj b) {return word(a,0xBA)==id_of(b) || word(b,0xBA)==id_of(a);}
// Members of the union of the given persons' sworn groups; more than 8 -> 99.
static unsigned sworn_union(Obj *people,unsigned n,Obj *out) {
    unsigned count=0;
    auto add=[&](Obj p) {for(unsigned i=0;i<count;++i)if(out[i]==p)return;if(count<8)out[count]=p;++count;};
    for(unsigned i=0;i<n;++i)add(people[i]);
    for(unsigned i=0;i<n;++i) {unsigned short g=word(people[i],0xBC);if(!g)continue;
        for(unsigned id=1;id<=6000;++id) {Obj p=table(0x158,id,6000);if(valid(p) && word(p,0xBC)==g)add(p);}}
    return count>8?99:count;
}
// 2-3 same-sex persons may form or join one sworn group of at most three.
static bool sworn_ok(Obj *people,unsigned n) {
    if(!mod_features.mediation_plus || n<2)return false;
    for(unsigned i=0;i<n;++i)for(unsigned j=i+1;j<n;++j)
        if(!people[i] || people[i]==people[j] || people[i][0xA9]!=people[j][0xA9])return false;
    unsigned short g=word(people[0],0xBC);bool same=g!=0;
    for(unsigned i=1;i<n;++i)same=same && word(people[i],0xBC)==g;
    if(same)return false;
    Obj all[8];unsigned count=sworn_union(people,n,all);
    if(count<2 || count>3)return false;
    for(unsigned i=0;i<count;++i)for(unsigned j=i+1;j<count;++j) {
        unsigned short gi=word(all[i],0xBC);
        if((!gi || gi!=word(all[j],0xBC)) && related(all[i],all[j]))return false;
    }
    return true;
}
// A man (married or not) and an unmarried woman, adults, not related; or a couple to divorce.
static bool marry_ok(Obj a,Obj b) {
    if(!mod_features.mediation_plus || !a || !b || a==b || a[0xA9]==b[0xA9])return false;
    if(married_pair(a,b))return true;
    Obj man=a[0xA9]==0?a:b,woman=man==a?b:a;
    return !word(woman,0xBA) && word(man,0xBA) && adult(man) && adult(woman) && !related(man,woman);
}
static unsigned long long (__fastcall *next_pair)(Obj,Obj,Obj,Obj);   // Layout manager: next of each site (original unless chained)
static void (__fastcall *next_marry_button[2])(Obj);
static void (__fastcall *next_sworn_button[2])(Obj);
static unsigned long long (__fastcall *next_marry_exec)(Obj);
static unsigned long long (__fastcall *next_sworn_exec)(Obj);
static unsigned long long __fastcall pair(Obj view,Obj candidate,Obj a,Obj b) { COST_SCOPE("mediation_plus::pair");
    unsigned long long r=next_pick(next_pair,kPair)(view,candidate,a,b);
    if(r || !candidate || !a)return r;
    if(b) {Obj p[3]={candidate,a,b};return sworn_ok(p,3);}
    if(candidate[0xA9]==a[0xA9]) {Obj p[2]={candidate,a};return sworn_ok(p,2);}
    return marry_ok(candidate,a);
}
// 6D7D90 (the filter CALL) now belongs to batteki/yuga (design 8.4, 8.7); the one condition that differed from the
// original filter -- married AND sworn candidates are hidden -- is lifted at 70E9C1 by a block hook instead (relay
// below). The rest of 70E9A0 (loyalty, service years, only-the-ruler rule) is the original code; the pair rule is
// still extended at 70EADC.
static void selected(Obj state,Obj out[3]) {
    for(unsigned i=0;i<3;++i) {Obj p=pointer(state,0x1C0+i*8);out[i]=valid(p)?p:nullptr;}
}
static void marry_button_after(Obj state) { COST_SCOPE("mediation_plus::marry_button");
    Obj s[3];selected(state,s);
    if(s[0] && s[1] && !s[2] && marry_ok(s[0],s[1]))fn<void(__fastcall *)(Obj,int,int)>(kEnable)(pointer(state,0x198),2,1);
}
static void sworn_button_after(Obj state) { COST_SCOPE("mediation_plus::sworn_button");
    Obj s[3];selected(state,s);
    Obj p[3];unsigned n=0;for(Obj x:s)if(x)p[n++]=x;
    if(n>=2 && s[0] && s[1] && sworn_ok(p,n))fn<void(__fastcall *)(Obj,int,int)>(kEnable)(pointer(state,0x198),1,1);
}
static Obj player_force() {return fn<Obj(__fastcall *)(Obj)>(GA(GA_32E8D0))(pointer(world(),0x871E8));}
static bool can_pay(Obj force) {
    Obj res=valid(force)?fn<Obj(__fastcall *)(Obj)>(GA(GA_22D3A0))(force):nullptr;
    if(!res)return false;
    int gold=(*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(res,0)+0x90))(res);
    return gold>=static_cast<int>(dword(image,GA(GA_1A485A8))) && force[0x14]>=dword(image,GA(GA_1A4859C));
}
static void pay(Obj force) {   // as 6D1A50 does
    Obj res=fn<Obj(__fastcall *)(Obj)>(GA(GA_22D3A0))(force);
    int max=(*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(res,0)+0xB8))(res);
    int gold=(*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(res,0)+0x90))(res)-static_cast<int>(dword(image,GA(GA_1A485A8)));
    (*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(res,0)+0x98))(res,gold<0?0:gold>max?max:gold);
    int orders=force[0x14]-static_cast<int>(dword(image,GA(GA_1A4859C)));
    force[0x14]=static_cast<unsigned char>(orders<0?0:orders>200?200:orders);
}
static void divorce(Obj man,Obj woman) {
    if(word(woman,0xBA)==id_of(man))*reinterpret_cast<unsigned short *>(woman+0xBA)=static_cast<unsigned short>(0);
    if(word(man,0xBA)!=id_of(woman))return;
    unsigned short next=0;   // a man with other wives keeps one of them as his spouse
    for(unsigned id=1;id<=6000 && !next;++id) {Obj p=table(0x158,id,6000);
        if(valid(p) && p!=woman && word(p,0xBA)==id_of(man))next=static_cast<unsigned short>(id);}
    *reinterpret_cast<unsigned short *>(man+0xBA)=static_cast<unsigned short>(next);
}
static unsigned long long __fastcall marry_exec(Obj command) { COST_SCOPE("mediation_plus::marry_exec");
    auto original=next_pick(next_marry_exec,kMarryExec);
    Obj state=pointer(command,0x4B8);Obj s[3];if(state)selected(state,s);
    if(!mod_features.mediation_plus || !state || !s[0] || !s[1] || s[2] || !married_pair(s[0],s[1]))return original(command);
    Obj man=s[0][0xA9]==0?s[0]:s[1],woman=man==s[0]?s[1]:s[0];
    Obj ui=pointer(state,0x198);
    if(ui)(*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(ui,0)+200))(ui,0);
    auto name=[](Obj p){const wchar_t *n=fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_22EEC0))(p);return n?n:L"";};
    wchar_t text[256];
    swprintf_s(text,L"%s과(와) %s을(를) 이혼시킵니다.\n금 %u, 명령서 %u 소모",
        name(man),name(woman),dword(image,GA(GA_1A485A8)),dword(image,GA(GA_1A4859C)));
    // 216090 runs the CCmnYesNoState modally and returns the answer (as 6D1A50 reads it).
    bool yes=fn<bool(__fastcall *)(const wchar_t *)>(GA(GA_216090))(text);
    Obj force=player_force();
    if(yes && can_pay(force)) {
        pay(force);divorce(man,woman);
        (*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(state,0)+400))(state,0x7FFFFFFD);
        log_event("mediation_divorce",id_of(man),id_of(woman));
        return 1;
    }
    *reinterpret_cast<int *>(state+0x180)=0;
    if(ui)(*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(ui,0)+200))(ui,1);
    return 0;
}
static unsigned long long __fastcall sworn_exec(Obj command) { COST_SCOPE("mediation_plus::sworn_exec");
    auto original=next_pick(next_sworn_exec,kSwornExec);
    Obj state=pointer(command,0x4B8);Obj s[3]={};if(state)selected(state,s);
    unsigned short old[3]={};for(unsigned i=0;i<3;++i)if(s[i])old[i]=word(s[i],0xBC);
    unsigned long long r=original(command);
    if(!r || !mod_features.mediation_plus || !s[0])return r;
    unsigned short now=word(s[0],0xBC);if(!now)return r;
    for(unsigned short g:old) {if(!g || g==now)continue;
        for(unsigned id=1;id<=6000;++id) {Obj p=table(0x158,id,6000);if(valid(p) && word(p,0xBC)==g)*reinterpret_cast<unsigned short *>(p+0xBC)=static_cast<unsigned short>(now);}}
    return r;
}
// Two call sites share each button body; every site keeps its own next.
template<unsigned I> static void __fastcall marry_button(Obj state) { COST_SCOPE("mediation_plus::marry_button_site");next_pick(next_marry_button[I],kMarryButton)(state);marry_button_after(state);}
template<unsigned I> static void __fastcall sworn_button(Obj state) { COST_SCOPE("mediation_plus::sworn_button_site");next_pick(next_sworn_button[I],kSwornButton)(state);sworn_button_after(state);}
// Relay for the 70E9C1 block (design 8.4): `cmp byte [flag],0 ; jne pass ; cmp word [rdx+0BCh],0 ; jne reject ;
// pass: jmp [70E9CB] ; reject: jmp [70EA11]`. RDX is the candidate person at 70E9B2 and stays untouched; the first
// instruction of 70E9CB reads [rdx+0E8h] only. With the flag set a married and sworn candidate is no longer hidden.
static Obj build_filter_relay() {
    Obj relay=allocate_near(image+GA(GA_70E9C0));if(!relay)return nullptr;
    layout_build_mediation_relay(relay,mod_features.mediation_plus,image+GA(GA_70E9CB),image+GA(GA_70EA11));
    DWORD old=0;
    if(!VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,64)) {
        VirtualFree(relay,0,MEM_RELEASE);return nullptr;
    }
    return relay;
}
static bool install() {
    if(!guards_match()) {log_event("mediation_plus_guard_rejected");return false;}
    Obj relay=build_filter_relay();if(!relay)return false;
    layout_client::Transaction tx;
    // 70E9C1 first: foreign bytes there refuse the block and the manager rolls the whole owner back (REFUSE_IF_FOREIGN).
    // 6D7D90 is left to batteki/yuga; the seven remaining E8 sites chain.
    if(layout_client::block(layout_mediation_row.owner,layout_mediation_row.rva,layout_mediation_expect,layout_mediation_row.len,relay,layout_mediation_row.flags)<=0 ||
       layout_client::call(kPairCall,reinterpret_cast<void *>(&pair),reinterpret_cast<void **>(&next_pair))<=0 ||
       layout_client::call(kMarryCalls[0],reinterpret_cast<void *>(&marry_button<0>),reinterpret_cast<void **>(&next_marry_button[0]))<=0 ||
       layout_client::call(kMarryCalls[1],reinterpret_cast<void *>(&marry_button<1>),reinterpret_cast<void **>(&next_marry_button[1]))<=0 ||
       layout_client::call(kSwornCalls[0],reinterpret_cast<void *>(&sworn_button<0>),reinterpret_cast<void **>(&next_sworn_button[0]))<=0 ||
       layout_client::call(kSwornCalls[1],reinterpret_cast<void *>(&sworn_button<1>),reinterpret_cast<void **>(&next_sworn_button[1]))<=0 ||
       layout_client::call(kMarryExecCall,reinterpret_cast<void *>(&marry_exec),reinterpret_cast<void **>(&next_marry_exec))<=0 ||
       layout_client::call(kSwornExecCall,reinterpret_cast<void *>(&sworn_exec),reinterpret_cast<void **>(&next_sworn_exec))<=0) {
        log_event("mediation_plus_register_rejected");return false;
    }
    tx.done=true;log_event("mediation_plus_installed");return true;
}
}
