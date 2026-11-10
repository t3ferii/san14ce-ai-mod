// AI battle death cut-in and game record (docs/HISTORY.md S4, 2026-11-06). Own source re-implementation of the ModPack v1.6
// BattleDeaths blob; no blob machine code runs. Registered with the Layout manager as owner "battle_deaths": one span (295C5F, relay)
// first, then 4 CALL rows (2 x 368360 cut-in visibility, 2 x 295AA0 death record). Everything runs on the game's own UI/AI thread
// inside those hooks; activation only registers. Pure byte tables and the relay builder come from battle_deaths_profile.h (bdp, fixture-tested).
// Included by native_split_install.inl (SPLIT_MODPACK), after native_modpack.inl and before native_trait_hover.inl.
#include <string.h>
#include <stdint.h>
#include "battle_deaths_profile.h"

namespace battle_deaths {
using Visible=uint64_t(__fastcall *)(Obj request,Obj position);                                                // 368360
using Record=uint64_t(__fastcall *)(Obj,Obj,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);   // 295AA0 (9 arguments)
struct Scope {Obj person;unsigned site;};
static thread_local Scope scope;          // the death being recorded by this thread (295AA0 call in progress)
static volatile LONG active;              // set once everything is registered
static bool installed;
static void *next_visible[2],*next_record[2];

// ------------------------------------------------------------------------------------------------------------------------
// diagnostics (bounded: once per key or a small total cap)
// ------------------------------------------------------------------------------------------------------------------------
#ifndef NO_RUNTIME_LOG
static volatile LONG cutin_seen[2][3],record_seen[2],forced_seen,fault_seen[3];
static bool take(volatile LONG &counter,LONG cap) {return InterlockedIncrement(&counter)<=cap;}
#endif
[[maybe_unused]] static unsigned person_id(Obj person) {   // person+0x10 (u16), 0xFFFF when unreadable
    __try {return *reinterpret_cast<const unsigned short *>(person+0x10);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return 0xFFFF;}
}
[[maybe_unused]] static int queued_of(Obj request) {       // request+0x1E0, -1 when unreadable
    __try {return request?*reinterpret_cast<const int *>(request+kBattleDeathsRequestQueuedOff):0;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return -1;}
}
static void note_cutin([[maybe_unused]] unsigned site,[[maybe_unused]] unsigned result,[[maybe_unused]] Obj request) {
#ifndef NO_RUNTIME_LOG
    if(site<2 && result<3 && take(cutin_seen[site][result],16))log_event("battle_deaths_cutin",site+1,result,static_cast<unsigned>(queued_of(request)));
#endif
}
static void note_record([[maybe_unused]] unsigned site,[[maybe_unused]] Obj person,[[maybe_unused]] uint64_t result) {
#ifndef NO_RUNTIME_LOG
    if(site<2 && take(record_seen[site],16))log_event("battle_deaths_record",site+1,person_id(person),static_cast<unsigned>(result));
#endif
}
static void note_forced([[maybe_unused]] unsigned site,[[maybe_unused]] Obj person) {
#ifndef NO_RUNTIME_LOG
    if(take(forced_seen,32))log_event("battle_deaths_record_forced",site,person_id(person),0);
#endif
}
static void note_fault([[maybe_unused]] unsigned path,[[maybe_unused]] unsigned code) {
#ifndef NO_RUNTIME_LOG
    if(path<3 && take(fault_seen[path],1))log_event("battle_deaths_fault",path,code,0);
#endif
}

// ------------------------------------------------------------------------------------------------------------------------
// replacement functions
// ------------------------------------------------------------------------------------------------------------------------
// 368360 without its screen test: the original's own leading conditions (request not too busy, a camera exists), nothing about the
// position. __try only lives here (C2712).
static unsigned force_visible_seh(Obj request) {
    __try {
        if(request && *reinterpret_cast<const int *>(request+kBattleDeathsRequestQueuedOff)>=static_cast<int>(kBattleDeathsCutinMaxQueued))return 0;
        Obj app=fn<Obj(__fastcall *)()>(GA(GA_14DAE0))();
        Obj camera=app?fn<Obj(__fastcall *)(Obj)>(GA(GA_14D9D0))(app):nullptr;
        return camera!=nullptr?1u:0u;
    } __except(EXCEPTION_EXECUTE_HANDLER) {note_fault(1,GetExceptionCode());return 0;}
}
// Sites 258EE0 / 27D4B0: the original answer wins; only a "not on screen" answer is overridden.
template<unsigned I> static uint64_t __fastcall visible(Obj request,Obj position) { COST_SCOPE("battle_deaths::visible");
    const uint64_t result=next_or<Visible>(next_visible[I],GA(GA_36A850))(request,position);
    if(result!=0) {note_cutin(I,1,request);return result;}
    if(!active)return result;
    const unsigned forced=force_visible_seh(request);
    note_cutin(I,forced?2u:0u,request);
    return forced;
}
// Sites 25974B / 27DA09: remembers the dying person for the duration of the 295AA0 call (scope_match reads it from the 295C5F relay).
struct ScopeRestore {
    Scope saved;
    ScopeRestore(Obj person,unsigned site):saved(scope) {if(active)scope={person,site};}
    ~ScopeRestore() {scope=saved;}
};
template<unsigned I> static uint64_t __fastcall record(Obj person,Obj killer,uint64_t a3,uint64_t a4,uint64_t a5,uint64_t a6,uint64_t a7,uint64_t a8,uint64_t a9) { COST_SCOPE("battle_deaths::record");
    ScopeRestore guard(person,I+1);
    const uint64_t result=next_or<Record>(next_record[I],GA(GA_2979B0))(person,killer,a3,a4,a5,a6,a7,a8,a9);
    note_record(I,person,result);
    return result;
}
// Called by the 295C5F relay when the original test found ebx==0 (rcx = r13 = the person being recorded): non-zero = write the record anyway.
static int __fastcall scope_match(Obj person) { COST_SCOPE("battle_deaths::scope_match");
    if(!person || !active)return 0;
    const Scope current=scope;
    if(current.person!=person)return 0;
    note_forced(current.site,person);
    return 1;
}

// ------------------------------------------------------------------------------------------------------------------------
// guards (plan 3.5) and installation (plan 3.2)
// ------------------------------------------------------------------------------------------------------------------------
static bool guard_fail(unsigned stage,unsigned row,unsigned rva) {
    static bool logged;
    if(!logged) {logged=true;log_event("battle_deaths_guard_rejected",stage,row,rva);}
    return false;
}
static bool guards_ok() {
    const size_t guard_count=sizeof(battle_deaths_guards)/sizeof(battle_deaths_guards[0]);
    const size_t site_count=sizeof(battle_deaths_sites)/sizeof(battle_deaths_sites[0]);
    // 4: every table id resolved in this build
    unsigned row=0;
    for(size_t i=0;i<guard_count;++i,++row)if(!battle_deaths_guards[i].rva)return guard_fail(4,row,0);
    for(size_t i=0;i<site_count;++i,++row)if(!battle_deaths_sites[i].site_rva || !battle_deaths_sites[i].original_rva)return guard_fail(4,row,battle_deaths_sites[i].site_rva);
    if(!battle_deaths_span_rva || !battle_deaths_reject_rva || !battle_deaths_app_rva || !battle_deaths_camera_rva)return guard_fail(4,row,0);
    // 1: original function hashes (observed entries tolerated like the other hash guards)
    for(size_t i=0;i<guard_count;++i) {
        ga::ObserverSet used;
        if(!ga::hash_ok(image,battle_deaths_guards[i].rva,battle_deaths_guards[i].size,battle_deaths_guards[i].hash,&used))
            return guard_fail(1,static_cast<unsigned>(i),battle_deaths_guards[i].rva);
        for(unsigned k=0;k<used.count;++k)log_event("observer_hook_tolerated",57,used.rva[k]);
    }
    // 2: the four hook sites are still CALLs to the expected original function
    for(size_t i=0;i<site_count;++i) {
        const unsigned site=battle_deaths_sites[i].site_rva;
        int rel=0;memcpy(&rel,image+site+1,4);
        if(image[site]!=0xE8 || static_cast<long long>(site)+5+rel!=static_cast<long long>(battle_deaths_sites[i].original_rva))
            return guard_fail(2,static_cast<unsigned>(i),site);
    }
    // 3: the 295C5F branch (plain memcmp: ga::mcmp_at would compare with the build's original bytes and ignore the expectation) and its skip target
    const unsigned span=battle_deaths_span_rva;
    if(memcmp(image+span,layout_battle_deaths_expect,sizeof(layout_battle_deaths_expect))!=0)
        return guard_fail(3,0,span);
    int rel=0;memcpy(&rel,image+span+4,4);
    if(static_cast<long long>(span)+8+rel!=static_cast<long long>(battle_deaths_reject_rva))return guard_fail(3,1,span);
    // 5: 295AA0 prologue: 7 pushes, lea rbp,[rsp-100h], sub rsp,imm32 with imm % 16 == 0 (the relay relies on a 16-aligned rsp at 295C5F)
    static const unsigned char pushes[12]={0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    const unsigned entry=GA(GA_2979B0);
    int frame=0;memcpy(&frame,image+entry+0x17,4);
    if(memcmp(image+entry,pushes,sizeof(pushes))!=0 || image[entry+0x14]!=0x48 || image[entry+0x15]!=0x81 || image[entry+0x16]!=0xEC ||
       frame<=0 || (frame&15)!=0)return guard_fail(5,0,entry);
    return true;
}
// Row k of the replacement table = battle_deaths_sites[k].
static void *replacement_for(size_t site_index) {
    switch(site_index) {
    case 0:return reinterpret_cast<void *>(&visible<0>);
    case 1:return reinterpret_cast<void *>(&visible<1>);
    case 2:return reinterpret_cast<void *>(&record<0>);
    case 3:return reinterpret_cast<void *>(&record<1>);
    }
    return nullptr;
}
static void **next_for(size_t site_index) {
    switch(site_index) {
    case 0:return &next_visible[0];
    case 1:return &next_visible[1];
    case 2:return &next_record[0];
    case 3:return &next_record[1];
    }
    return nullptr;
}
static void register_rejected(unsigned row,int handle) {
    static bool logged;
    if(!logged) {logged=true;log_event("battle_deaths_register_rejected",row,handle<0?1u:0u,static_cast<unsigned>(handle<0?-handle:0));}
}
// Relay for the span, then the span, then the four CALL rows in one Transaction (all or nothing).
static bool install() {
    if(installed)return false;
    uint8_t code[bdp::kRelayMaxLen];
    const size_t length=bdp::build_branch_relay(code,sizeof(code),reinterpret_cast<uint64_t>(&scope_match),
        reinterpret_cast<uint64_t>(image+battle_deaths_span_rva+layout_battle_deaths_span.len),reinterpret_cast<uint64_t>(image+battle_deaths_reject_rva));
    if(!length) {register_rejected(0,0);return false;}
    Obj relay=allocate_near(image+battle_deaths_span_rva);
    if(!relay) {register_rejected(0,0);return false;}
    memcpy(relay,code,length);
    DWORD old=0;
    if(!VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,length)) {
        VirtualFree(relay,0,MEM_RELEASE);register_rejected(0,0);return false;
    }
    layout_client::Transaction tx;
    // Once the span is registered the relay is never released, even when a later row fails: the manager may already have published it.
    const int span=layout_client::span(layout_battle_deaths_span.owner,layout_battle_deaths_span.rva,layout_battle_deaths_expect,
        layout_battle_deaths_span.len,relay,layout_battle_deaths_span.flags);
    if(span<=0) {register_rejected(0,span);return false;}
    const size_t site_count=sizeof(battle_deaths_sites)/sizeof(battle_deaths_sites[0]);
    for(size_t i=0;i<site_count;++i) {
        void *rep=replacement_for(i);void **next=next_for(i);
        const int handle=rep && next?layout_client::call(battle_deaths_sites[i].site_rva,rep,next):0;
        if(handle<=0) {register_rejected(static_cast<unsigned>(i)+1,handle);return false;}
    }
    tx.done=true;installed=true;
    InterlockedExchange(&active,1);
    log_event("battle_deaths_installed",static_cast<unsigned>(site_count),1,0);
    return true;
}
}   // namespace battle_deaths
static bool battle_deaths_guards_ok() {return battle_deaths::guards_ok();}
static bool battle_deaths_install() {return battle_deaths::install();}
