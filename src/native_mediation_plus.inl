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
namespace mediation_plus {
static const unsigned kFilterCall=0x6D7D90,kPairCall=0x70EADC,kMarryCalls[2]={0x6BA697,0x7971B3},
    kSwornCalls[2]={0x6BA69F,0x7971BB},kMarryExecCall=0x73502B,kSwornExecCall=0x735035;
static const unsigned kFilter=0x70E9A0,kPair=0x711920,kMarryButton=0x796110,kSwornButton=0x78D920,
    kMarryExec=0x6D1A50,kSwornExec=0x6D0550,kEnable=0x85AAD0;
static bool call_to(unsigned site,unsigned target) {
    return image[site]==0xE8 && site+5+*reinterpret_cast<int *>(image+site+1)==target;
}
static bool guards_match() {
    return call_to(kFilterCall,kFilter) && call_to(kPairCall,kPair) && call_to(kMarryCalls[0],kMarryButton) &&
        call_to(kMarryCalls[1],kMarryButton) && call_to(kSwornCalls[0],kSwornButton) && call_to(kSwornCalls[1],kSwornButton) &&
        call_to(kMarryExecCall,kMarryExec) && call_to(kSwornExecCall,kSwornExec);
}
static unsigned short id_of(Obj p) {return word(p,0x10);}
static int age(Obj p) {return fn<int(__fastcall *)(Obj)>(0x2A5EA0)(p);}
static bool adult(Obj p) {return age(p)>=static_cast<int>(dword(image,0x1A48D70));}
static bool related(Obj a,Obj b) {
    auto check=fn<int(__fastcall *)(Obj,Obj)>(0x2C3A90);
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
static unsigned long long __fastcall pair(Obj view,Obj candidate,Obj a,Obj b) {
    unsigned long long r=fn<unsigned long long(__fastcall *)(Obj,Obj,Obj,Obj)>(kPair)(view,candidate,a,b);
    if(r || !candidate || !a)return r;
    if(b) {Obj p[3]={candidate,a,b};return sworn_ok(p,3);}
    if(candidate[0xA9]==a[0xA9]) {Obj p[2]={candidate,a};return sworn_ok(p,2);}
    return marry_ok(candidate,a);
}
static Obj ruler_of(Obj p) {
    Obj force=fn<Obj(__fastcall *)(Obj)>(0x22CA00)(p);
    return valid(force)?table(0x158,word(force,0x10),6000):nullptr;
}
static unsigned long long __fastcall filter(Obj view,Obj candidate,int slot) {
    unsigned long long r=fn<unsigned long long(__fastcall *)(Obj,Obj,int)>(kFilter)(view,candidate,slot);
    // The original hides a person who is both married and sworn before any pair check.
    if(r || !mod_features.mediation_plus || !candidate || !word(candidate,0xBA) || !word(candidate,0xBC))return r;
    if(candidate[0xE8]<100 && candidate!=ruler_of(candidate))return 0;
    short year=0;fn<void(__fastcall *)(short *,Obj)>(0x31ABE0)(&year,pointer(world(),0x871E8)+0x34);
    if(year-static_cast<int>(word(candidate,0x166))<static_cast<int>(dword(image,0x1A485C0)) && candidate!=ruler_of(candidate))return 0;
    Obj selection=pointer(view,0x4B8);if(!selection)return 0;
    Obj s0=pointer(selection,0x1C0),s1=pointer(selection,0x1C8),s2=pointer(selection,0x1D0);
    Obj a=s0,b=s2;
    if(slot==0) {if(!valid(s1) && !valid(s2))return 1;a=s1;}
    else if(slot==2)b=s1;
    else if(slot!=1)return 1;
    return pair(view,candidate,valid(a)?a:nullptr,valid(b)?b:nullptr)?1:0;
}
static void selected(Obj state,Obj out[3]) {
    for(unsigned i=0;i<3;++i) {Obj p=pointer(state,0x1C0+i*8);out[i]=valid(p)?p:nullptr;}
}
static void __fastcall marry_button(Obj state) {
    fn<void(__fastcall *)(Obj)>(kMarryButton)(state);
    Obj s[3];selected(state,s);
    if(s[0] && s[1] && !s[2] && marry_ok(s[0],s[1]))fn<void(__fastcall *)(Obj,int,int)>(kEnable)(pointer(state,0x198),2,1);
}
static void __fastcall sworn_button(Obj state) {
    fn<void(__fastcall *)(Obj)>(kSwornButton)(state);
    Obj s[3];selected(state,s);
    Obj p[3];unsigned n=0;for(Obj x:s)if(x)p[n++]=x;
    if(n>=2 && s[0] && s[1] && sworn_ok(p,n))fn<void(__fastcall *)(Obj,int,int)>(kEnable)(pointer(state,0x198),1,1);
}
static Obj player_force() {return fn<Obj(__fastcall *)(Obj)>(0x32E8D0)(pointer(world(),0x871E8));}
static bool can_pay(Obj force) {
    Obj res=valid(force)?fn<Obj(__fastcall *)(Obj)>(0x22D3A0)(force):nullptr;
    if(!res)return false;
    int gold=(*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(res,0)+0x90))(res);
    return gold>=static_cast<int>(dword(image,0x1A485A8)) && force[0x14]>=dword(image,0x1A4859C);
}
static void pay(Obj force) {   // as 6D1A50 does
    Obj res=fn<Obj(__fastcall *)(Obj)>(0x22D3A0)(force);
    int max=(*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(res,0)+0xB8))(res);
    int gold=(*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(res,0)+0x90))(res)-static_cast<int>(dword(image,0x1A485A8));
    (*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(res,0)+0x98))(res,gold<0?0:gold>max?max:gold);
    int orders=force[0x14]-static_cast<int>(dword(image,0x1A4859C));
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
static unsigned long long __fastcall marry_exec(Obj command) {
    auto original=fn<unsigned long long(__fastcall *)(Obj)>(kMarryExec);
    Obj state=pointer(command,0x4B8);Obj s[3];if(state)selected(state,s);
    if(!mod_features.mediation_plus || !state || !s[0] || !s[1] || s[2] || !married_pair(s[0],s[1]))return original(command);
    Obj man=s[0][0xA9]==0?s[0]:s[1],woman=man==s[0]?s[1]:s[0];
    Obj ui=pointer(state,0x198);
    if(ui)(*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(ui,0)+200))(ui,0);
    auto name=[](Obj p){const wchar_t *n=fn<const wchar_t *(__fastcall *)(Obj)>(0x22EEC0)(p);return n?n:L"";};
    wchar_t text[256];
    swprintf_s(text,L"%s과(와) %s을(를) 이혼시킵니다.\n금 %u, 명령서 %u 소모",
        name(man),name(woman),dword(image,0x1A485A8),dword(image,0x1A4859C));
    // 216090 runs the CCmnYesNoState modally and returns the answer (as 6D1A50 reads it).
    bool yes=fn<bool(__fastcall *)(const wchar_t *)>(0x216090)(text);
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
static unsigned long long __fastcall sworn_exec(Obj command) {
    auto original=fn<unsigned long long(__fastcall *)(Obj)>(kSwornExec);
    Obj state=pointer(command,0x4B8);Obj s[3]={};if(state)selected(state,s);
    unsigned short old[3]={};for(unsigned i=0;i<3;++i)if(s[i])old[i]=word(s[i],0xBC);
    unsigned long long r=original(command);
    if(!r || !mod_features.mediation_plus || !s[0])return r;
    unsigned short now=word(s[0],0xBC);if(!now)return r;
    for(unsigned short g:old) {if(!g || g==now)continue;
        for(unsigned id=1;id<=6000;++id) {Obj p=table(0x158,id,6000);if(valid(p) && word(p,0xBC)==g)*reinterpret_cast<unsigned short *>(p+0xBC)=static_cast<unsigned short>(now);}}
    return r;
}
static CallLink links[8];
static bool install() {
    if(!guards_match()) {log_event("mediation_plus_guard_rejected");return false;}
    Obj relay=allocate_near(image+kPairCall);if(!relay)return false;
    void *targets[6]={reinterpret_cast<void *>(&filter),reinterpret_cast<void *>(&pair),reinterpret_cast<void *>(&marry_button),
        reinterpret_cast<void *>(&sworn_button),reinterpret_cast<void *>(&marry_exec),reinterpret_cast<void *>(&sworn_exec)};
    for(unsigned i=0;i<6;++i)absolute_jump(relay+i*32,targets[i]);
    const unsigned sites[8]={kFilterCall,kPairCall,kMarryCalls[0],kMarryCalls[1],kSwornCalls[0],kSwornCalls[1],kMarryExecCall,kSwornExecCall};
    const unsigned stub[8]={0,1,2,2,3,3,4,5};
    bool ok=true;for(unsigned i=0;ok && i<8;++i)ok=call_prepare(links[i],sites[i],relay+stub[i]*32);
    DWORD old;
    if(!ok || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,192)) {
        VirtualFree(relay,0,MEM_RELEASE);return false;}
    for(unsigned i=0;i<8;++i)if(!call_publish(links[i])) {log_event("mediation_plus_publish_rejected",i);while(i)call_restore(links[--i]);return false;}
    log_event("mediation_plus_installed");return true;
}
}
