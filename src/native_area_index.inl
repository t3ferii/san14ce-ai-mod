// [performance] area_index; sole owner SAN14_Convenience.dll. Native UI/AI callers only.
// Evidence: persistent exports 2C9E30/231EB0/232000/332F80/22FB10/229EA0;
// tests/verify_area_index.py checks decoded build + recorded running bytes and Steam identity.
#include "game_addresses.h"
#include "area_index.h"
namespace area_index {
static const unsigned kCheck=GA(GA_2CC1E0);
struct Guard {unsigned rva,size;unsigned long long hash;};
static const Guard guards[]={
    {GA(GA_2CC1E0),0x124,GH(0x752061834cdd958eull)},
    {GA(GA_233850),0x99,GH(0x27e47a6c73d1aba7ull)},
    {GA(GA_335200),0x22,GH(0x52d52e608ad67f7aull)},
    {GA(GA_2315A0),0x2C,GH(0x810a4ad693aee636ull)},
    {GA(GA_22B8E0),0x2C,GH(0x5520af45e33caaa0ull)}
};
static const unsigned callers[]={GA(GA_27ABC0),GA(GA_7E707E),GA(GA_7FBAFF)};
static bool guards_match() {
    for(const auto &g:guards) {
        unsigned long long h=0xcbf29ce484222325ull;
        for(unsigned i=0;i<g.size;++i)h=(h^image[g.rva+i])*0x100000001b3ull;
        if(h!=g.hash)return false;
    }
    for(unsigned site:callers)
        if(image[site]!=0xE8 || site+5+*reinterpret_cast<int *>(image+site+1)!=kCheck)return false;
    return true;
}
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
using Check=int(__fastcall *)(Obj);
static Check original;
static SRWLOCK lock=SRWLOCK_INIT;
static area_index_core::Cache<Obj> cache;
static volatile LONG dirty;
static CodeLink link;
#ifndef NO_RUNTIME_LOG
static bool verify;
static LONGLONG frequency;
static LONGLONG ticks() {LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}
#endif
static area_index_core::Identity identity() {
    Obj g=world();
    if(!g)return {};
    return {reinterpret_cast<uintptr_t>(g),reinterpret_cast<uintptr_t>(pointer(g,0xE270)),
        reinterpret_cast<uintptr_t>(pointer(g,0x6CAE8)),reinterpret_cast<uintptr_t>(pointer(g,0x6CAF0))};
}
struct Native {
    unsigned region(Obj c) const {return word(c,0x12);}
    Obj building(Obj c) const {return fn<Obj(__fastcall *)(Obj)>(GA(GA_2315A0))(c);}
    bool valid(Obj p) const {return fn<Check>(GA(GA_331780))(p)!=0;}
    Obj type(Obj b) const {const unsigned id=b[0x12];return pointer(world(),0x73958+(id<0x1F?id:0)*8);}
    unsigned kind(Obj t) const {return t[0x2C];}
};
static int __fastcall check(Obj area) { COST_SCOPE_HOT("check");
#ifndef NO_RUNTIME_LOG
    COST_SCOPE("area_index_check");
#endif
    // Native 32F500 returns EAX=0 or 1; retain its zero result on invalid input.
    const int valid=fn<Check>(GA(GA_331780))(area);if(!valid)return valid;
    if(!TryAcquireSRWLockShared(&lock))return original(area);
    auto id=identity();
    unsigned idx=area_index_core::region(reinterpret_cast<uintptr_t>(area),id.area0);
    if(!idx || !id.world || !id.first || !id.last) {ReleaseSRWLockShared(&lock);return original(area);}
    if(InterlockedCompareExchange(&dirty,0,0) || !cache.matches(id)) {
        ReleaseSRWLockShared(&lock);
        if(!TryAcquireSRWLockExclusive(&lock))return original(area);
        id=identity();idx=area_index_core::region(reinterpret_cast<uintptr_t>(area),id.area0);
        if(!idx || !id.world || !id.first || !id.last) {ReleaseSRWLockExclusive(&lock);return original(area);}
        if(InterlockedCompareExchange(&dirty,0,0) || !cache.matches(id)) {
#ifndef NO_RUNTIME_LOG
            const LONGLONG start=ticks();
#endif
            cache.invalidate();
            try {
                Obj g=reinterpret_cast<Obj>(id.world);
                cache.rebuild(id,[g](unsigned i) {return pointer(g,0xE270+i*8);},
                    [](Obj c) {return c?word(c,0x12):0u;});
            } catch(...) {cache.invalidate();ReleaseSRWLockExclusive(&lock);return original(area);}
            if(!(id==identity())) {cache.invalidate();ReleaseSRWLockExclusive(&lock);return original(area);}
            InterlockedExchange(&dirty,0);
#ifndef NO_RUNTIME_LOG
            const LONGLONG elapsed=ticks()-start;
            log_event("area_index_rebuild",area_index_core::kCells,
                static_cast<unsigned>(elapsed*1000000/frequency));
#endif
        }
        // SRW locks cannot downgrade atomically. Recheck after reacquiring shared ownership.
        ReleaseSRWLockExclusive(&lock);
        if(!TryAcquireSRWLockShared(&lock))return original(area);
        if(InterlockedCompareExchange(&dirty,0,0) || !(id==identity()) || !cache.matches(id)) {
            ReleaseSRWLockShared(&lock);return original(area);
        }
    }
    Native native;
    const auto result=cache.evaluate(idx,area[0x6E],native);
    if(!result.usable)InterlockedExchange(&dirty,1);
    ReleaseSRWLockShared(&lock);
    if(!result.usable)return original(area);
#ifndef NO_RUNTIME_LOG
    if(verify) {
        const int expected=original(area);
        if(result.value!=expected) {log_event("area_index_mismatch",idx,result.value,expected);return expected;}
    }
#endif
    return result.value;
}
static bool prepare(Obj relay) {
    if(!guards_match())return false;
    absolute_jump(relay,reinterpret_cast<void *>(&check));
    // Eight complete, non-relative bytes: MOV RAX,RSP; PUSH RDI; SUB RSP,70h.
    // R11 branch is not an unwinder-recognized epilog; register the copied prolog.
    memcpy(relay+32,image+kCheck,8);
    relay[40]=0x49;relay[41]=0xBB;Obj continuation=image+kCheck+8;memcpy(relay+42,&continuation,8);
    relay[50]=0x41;relay[51]=0xFF;relay[52]=0xE3;
    original=reinterpret_cast<Check>(relay+32);
    auto runtime=reinterpret_cast<RUNTIME_FUNCTION *>(relay+64);
    runtime->BeginAddress=32;runtime->EndAddress=53;runtime->UnwindData=80;
    const unsigned char unwind[]={1,8,2,0,8,0xD2,4,0x70}; // alloc_small 112, push RDI
    memcpy(relay+80,unwind,sizeof(unwind));
    if(!prepare_link(link,kCheck,0xE9,relay))return false;
    memset(link.modified+5,0x90,3);return true;
}
static void restore() {restore_link(link);}
static bool publish() {if(publish_link(link))return true;restore();return false;}
static bool install() {
#ifndef NO_RUNTIME_LOG
    LARGE_INTEGER q;if(!QueryPerformanceFrequency(&q) || q.QuadPart<=0)return false;frequency=q.QuadPart;
    verify=dev_group() && mod_setting(L"diagnostics",L"area_index_verify",0)!=0;
#endif
    Obj relay=allocate_near(image+kCheck);if(!relay)return false;
    DWORD old;
    if(!prepare(relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,96)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    if(!RtlAddFunctionTable(reinterpret_cast<RUNTIME_FUNCTION *>(relay+64),1,reinterpret_cast<DWORD64>(relay))) {
        VirtualFree(relay,0,MEM_RELEASE);return false;
    }
    // Retain relay/unwind data after publication attempts: a caller may already be inside.
    if(!publish())return false;
    log_event("area_index_installed");return true;
}
#endif
}
static bool area_index_guards_match() {return area_index::guards_match();}
