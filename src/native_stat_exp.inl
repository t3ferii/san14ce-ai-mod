// [officer_ui] stat_exp (2026-10-23, user request; behaviour of reference_mod SAN14_StatExp, re-derived
// from the original). The pentagon chart number of each base stat gets "/N%": progress to the next point.
// Original (REA, unpacked 1fbe6be7...):
//   85C420(chart,stat,value,base) formats "%d" / "%d^02+%d^00" ... and writes it with
//   CALL 930D20(chart+130, element, text, 1) at 85C5D7 (85C5C3 is the argument setup label).
//   Three windows fill the chart per stat: 22FCE0(person,stat,0xFFF) then 85C420(chart,stat,...):
//   officer detail 9073F6.., ability tab 907F79.., sortie officer info 7F5A8C.. (5 pairs each).
//   Stat experience (2B9D50 grows it): Person+104+i*4 cumulative, base Person+EC+i (100 = end);
//   need(L) = 242890(3359E0()+60[L], i), as 2B14B0 uses it.
// Mod: the 22FCE0 call of each pair remembers the person, the 85C420 call the stat, and the text call
// appends ^05/N%^00 (green). Only these 30 call sites and that one text call are linked.
namespace stat_exp {
static const unsigned kStat=0x22FCE0,kChart=0x85C420,kText=0x930D20,kTextCall=0x85C5D7;
static const unsigned kWindows[3]={0x9073F6,0x907F79,0x7F5A8C};
static unsigned stat_sites[15],chart_sites[15];
static thread_local Obj current_person;
static thread_local int current_stat=-1;
static bool call_to(unsigned site,unsigned target) {
    return image[site]==0xE8 && site+5+*reinterpret_cast<int *>(image+site+1)==target;
}
// Each window: five (22FCE0, 85C420) pairs within 0x110 bytes, alternating, in stat order.
static bool find_sites() {
    for(unsigned w=0;w<3;++w) {
        unsigned stats=0,charts=0;
        for(unsigned at=kWindows[w];at<kWindows[w]+0x110 && charts<5;++at) {
            if(call_to(at,kStat) && stats==charts)stat_sites[w*5+stats++]=at;
            else if(call_to(at,kChart) && charts+1==stats)chart_sites[w*5+charts++]=at;
        }
        if(stats!=5 || charts!=5)return false;
    }
    return true;
}
static bool guards_match() {return call_to(kTextCall,kText) && find_sites();}
static int percent(Obj person,int stat) {
    if(!valid(person) || stat<0 || stat>4)return -1;
    unsigned level=person[0xEC+stat];if(level>=100)return -1;
    Obj table=fn<Obj(__fastcall *)()>(0x3359E0)();if(!table)return -1;
    auto need=[&](unsigned l,int &out)->bool {
        if(!l) {out=0;return true;}
        Obj entry=pointer(table,0x60+l*8);
        if(!entry || !fn<int(__fastcall *)(Obj)>(0x32F4D0)(entry))return false;
        out=fn<int(__fastcall *)(Obj,int)>(0x242890)(entry,stat);return true;
    };
    int low=0,high=0;if(!need(level,low) || !need(level+1,high) || high<=low)return -1;
    long long have=static_cast<long long>(dword(person,0x104+stat*4))-low;
    long long p=have*100/(high-low);return p<0?0:p>99?99:static_cast<int>(p);
}
static int __fastcall stat_value(Obj person,int stat,int flags) {
    current_person=person;
    return fn<int(__fastcall *)(Obj,int,int)>(kStat)(person,stat,flags);
}
static void __fastcall chart(Obj object,int stat,unsigned value,unsigned long long base) {
    current_stat=stat;
    fn<void(__fastcall *)(Obj,int,unsigned,unsigned long long)>(kChart)(object,stat,value,base);
    current_stat=-1;current_person=nullptr;
}
static void __fastcall text(Obj set,int element,const wchar_t *value,int flag) {
    auto original=fn<void(__fastcall *)(Obj,int,const wchar_t *,int)>(kText);
    int p=mod_features.stat_exp && current_stat>=0?percent(current_person,current_stat):-1;
    if(p<0 || !value)return original(set,element,value,flag);
    wchar_t buffer[160];
    if(swprintf_s(buffer,L"%s^05/%d%%^00",value,p)<0)return original(set,element,value,flag);
    original(set,element,buffer,flag);
}
static CallLink links[31];
static bool install() {
    if(!guards_match()) {log_event("stat_exp_guard_rejected");return false;}
    Obj relay=allocate_near(image+kTextCall);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&stat_value));
    absolute_jump(relay+32,reinterpret_cast<void *>(&chart));
    absolute_jump(relay+64,reinterpret_cast<void *>(&text));
    unsigned n=0;bool ok=call_prepare(links[n++],kTextCall,relay+64);
    for(unsigned i=0;ok && i<15;++i)
        ok=call_prepare(links[n++],stat_sites[i],relay) && call_prepare(links[n++],chart_sites[i],relay+32);
    DWORD old;
    if(!ok) log_event("stat_exp_prepare_rejected",n);
    if(!ok || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,96)) {
        VirtualFree(relay,0,MEM_RELEASE);return false;}
    for(unsigned i=0;i<n;++i)if(!call_publish(links[i])) {log_event("stat_exp_publish_rejected",i);while(i)call_restore(links[--i]);return false;}
    log_event("stat_exp_installed",n);return true;
}
}
