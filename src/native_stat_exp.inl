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
#include "game_addresses.h"
#include <utility>
namespace stat_exp {
static const unsigned kStat=GA(GA_22FCE0),kChart=GA(GA_85C420),kText=GA(GA_930D20),kTextCall=GA(GA_85C5D7);
static const unsigned kWindows[3]={GA(GA_9073F6),GA(GA_907F79),GA(GA_7F5A8C)};
static unsigned stat_sites[15],chart_sites[15];
using StatRead=int(__fastcall *)(Obj,int,int);
using ChartDraw=void(__fastcall *)(Obj,int,unsigned,unsigned long long);
using TextSet=void(__fastcall *)(Obj,int,const wchar_t *,int);
// Layout manager: next of each site. A call site has its own chain, so every site keeps its own next.
static StatRead next_stat[15];static ChartDraw next_chart[15];static TextSet next_text;
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
static bool guards_match() {return image[kTextCall]==0xE8 && find_sites();}   // the text CALL target is judged by the Layout hook manager
static int percent(Obj person,int stat) {
    if(!valid(person) || stat<0 || stat>4)return -1;
    unsigned level=person[0xEC+stat];if(level>=100)return -1;
    Obj table=fn<Obj(__fastcall *)()>(GA(GA_3359E0))();if(!table)return -1;
    auto need=[&](unsigned l,int &out)->bool {
        if(!l) {out=0;return true;}
        Obj entry=pointer(table,0x60+l*8);
        if(!entry || !fn<int(__fastcall *)(Obj)>(GA(GA_32F4D0))(entry))return false;
        out=fn<int(__fastcall *)(Obj,int)>(GA(GA_242890))(entry,stat);return true;
    };
    int low=0,high=0;if(!need(level,low) || !need(level+1,high) || high<=low)return -1;
    long long have=static_cast<long long>(dword(person,0x104+stat*4))-low;
    long long p=have*100/(high-low);return p<0?0:p>99?99:static_cast<int>(p);
}
template<unsigned I> static int __fastcall stat_value(Obj person,int stat,int flags) { COST_SCOPE("stat_exp::stat_value");
    current_person=person;
    return next_pick(next_stat[I],kStat)(person,stat,flags);
}
template<unsigned I> static void __fastcall chart(Obj object,int stat,unsigned value,unsigned long long base) { COST_SCOPE("stat_exp::chart");
    current_stat=stat;
    next_pick(next_chart[I],kChart)(object,stat,value,base);
    current_stat=-1;current_person=nullptr;
}
static void __fastcall text(Obj set,int element,const wchar_t *value,int flag) { COST_SCOPE("stat_exp::text");
    auto original=next_pick(next_text,kText);   // design 8.0 G1: never the original 930D20 directly (StatusColor and others sit behind it)
    int p=mod_features.stat_exp && current_stat>=0?percent(current_person,current_stat):-1;
    if(p<0 || !value)return original(set,element,value,flag);
    // Design 8.1 row 24: R1 StatusColor tells the chart text call by the return address 85C5DC inside 930D20. Every
    // path ends in a plain tail call of `next` and the text lives in a UI-thread static buffer (no escaping stack
    // address), so the optimizer keeps the original return address on the stack. Effect on StatusColor: not verified.
    static thread_local wchar_t buffer[160];
    if(swprintf_s(buffer,L"%s^05/%d%%^00",value,p)<0)return original(set,element,value,flag);
    return original(set,element,buffer,flag);
}
template<unsigned I> static bool register_pair() {
    return layout_client::call(stat_sites[I],reinterpret_cast<void *>(&stat_value<I>),reinterpret_cast<void **>(&next_stat[I]))>0 &&
           layout_client::call(chart_sites[I],reinterpret_cast<void *>(&chart<I>),reinterpret_cast<void **>(&next_chart[I]))>0;
}
template<unsigned... I> static bool register_pairs(std::integer_sequence<unsigned,I...>) {return (register_pair<I>() && ...);}
static bool install() {
    if(!guards_match()) {log_event("stat_exp_guard_rejected");return false;}
    // Registry sites are fixed RVAs (design 8.1); the scan above must agree with them or the feature stays off.
    for(unsigned i=0;i<15;++i)if(!layout_client::row_for(stat_sites[i]) || !layout_client::row_for(chart_sites[i])) {
        log_event("stat_exp_site_unregistered",i);return false;
    }
    layout_client::Transaction tx;
    // Text call first, then the 15 (22FCE0, 85C420) pairs; the manager rolls the whole owner back on any refusal.
    if(layout_client::call(kTextCall,reinterpret_cast<void *>(&text),reinterpret_cast<void **>(&next_text))<=0 ||
       !register_pairs(std::make_integer_sequence<unsigned,15>())) {log_event("stat_exp_register_rejected");return false;}
    tx.done=true;log_event("stat_exp_installed",31);return true;
}
}
