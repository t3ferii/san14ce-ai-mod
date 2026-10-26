// Sole owner SAN14_Convenience.dll. Diagnostic observation; native arguments/results unchanged.
#ifndef NO_RUNTIME_LOG
#include "path_count.h"
namespace path_count {
static constexpr unsigned entry=0x183A30,size=0x9A2;
static constexpr unsigned char prologue[]={0x48,0x8B,0xC4,0x44,0x89,0x48,0x20};
static bool matches(const unsigned char *code) {
    return !memcmp(code,prologue,sizeof(prologue)) &&
        path_count_core::bytes(path_count_core::basis,code,size)==0x11cbf08ff66c9febull;
}
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
static bool guards_match() {return matches(image+entry);}
using Search=Obj(__fastcall *)(Obj,unsigned,const int *,int,Obj,int,unsigned);
static Search original;
static CodeLink link;
static bool active;
static bool (*force_scope)();
static LONGLONG frequency;
static SRWLOCK day_lock=SRWLOCK_INIT;
static path_count_core::Day day;
static thread_local path_count_core::Force force;
static unsigned scalar(std::uint64_t n) {return n>UINT32_MAX?UINT32_MAX:static_cast<unsigned>(n);}
static LONGLONG ticks() {LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}
static void emit_day(int,const path_count_core::Totals &t) {
    // Deferred until the first observed call/root completion on a different day.
    // Logger day= is the boundary day; counters belong to the preceding observed day.
    log_event("path_day",scalar(t.calls),scalar(t.repeats),scalar(t.total_us/1000));
}
static bool input_key(Obj ctx,unsigned start,const int *goals,int count,unsigned flags,path_count_core::Hash &key) {
    __try {key=path_count_core::input(ctx,start,goals,count,flags);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static bool read_node(std::uintptr_t node,std::uintptr_t &parent,std::uint32_t &cell) {
    __try {
        memcpy(&parent,reinterpret_cast<void *>(node+0x10),sizeof(parent));
        memcpy(&cell,reinterpret_cast<void *>(node+0x18),sizeof(cell));return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static Obj __fastcall search(Obj ctx,unsigned start,const int *goals,int count,Obj buffer,int clear_bytes,unsigned flags) {
    const DWORD incoming_error=GetLastError();
    path_count_core::Hash key=0;
    const bool readable=input_key(ctx,start,goals,count,flags,key);
    const bool in_force=force_scope && force_scope();
    const int stamp=game_day();
    const LONGLONG begin=ticks();
    SetLastError(incoming_error);
    Obj output=original(ctx,start,goals,count,buffer,clear_bytes,flags);
    const DWORD native_error=GetLastError();
    const LONGLONG elapsed=ticks()-begin;
    const auto us=static_cast<std::uint64_t>(static_cast<double>(elapsed)*1000000.0/static_cast<double>(frequency));
    bool repeat=false;
    if(in_force) {
        const auto result=path_count_core::result(reinterpret_cast<std::uintptr_t>(output),read_node);
        const auto before=force.totals.repeats;
        force.add(key,result.hash,us,readable && !result.unreadable);
        repeat=force.totals.repeats!=before;
    }
    AcquireSRWLockExclusive(&day_lock);
    day.advance(stamp,emit_day);day.add(us,repeat);
    ReleaseSRWLockExclusive(&day_lock);
    SetLastError(native_error);return output;
}
static void flush() {
    if(!active)return;
    const DWORD native_error=GetLastError();
    const auto t=force.flush();
    log_event("path_force",scalar(t.calls),scalar(t.repeats),scalar(t.repeat_us));
    log_event("path_force_result",scalar(t.same),scalar(t.different),scalar(t.total_us));
    if(t.overflow)log_event("path_force_overflow",scalar(t.overflow));
    const int stamp=game_day();
    AcquireSRWLockExclusive(&day_lock);day.advance(stamp,emit_day);ReleaseSRWLockExclusive(&day_lock);
    SetLastError(native_error);
}
static bool prepare(Obj relay) {
    if(!guards_match())return false;
    absolute_jump(relay,reinterpret_cast<void *>(&search));
    // Two complete home-space MOVs; no relative operand, stack change or nonvolatile register.
    memcpy(relay+32,image+entry,sizeof(prologue));
    relay[39]=0x49;relay[40]=0xBB;Obj continuation=image+entry+sizeof(prologue);memcpy(relay+41,&continuation,8);
    relay[49]=0x41;relay[50]=0xFF;relay[51]=0xE3;
    original=reinterpret_cast<Search>(relay+32);
    auto runtime=reinterpret_cast<RUNTIME_FUNCTION *>(relay+64);
    runtime->BeginAddress=32;runtime->EndAddress=52;runtime->UnwindData=80;
    const unsigned char unwind[]={1,7,0,0};memcpy(relay+80,unwind,sizeof(unwind));
    if(!prepare_link(link,entry,0xE9,relay))return false;
    memset(link.modified+5,0x90,2);return true;
}
static bool install(bool root_ready,bool (*scope)()) {
    if(!dev_group() || !diagnostic_logging || !mod_setting(L"diagnostics",L"path_count",0))return true;
    if(!guards_match()) {log_event("path_count_guard_rejected");return false;}
    // Never log under loader lock in a TLS destructor. Fail closed without the existing root wrapper.
    if(!root_ready) {log_event("path_count_root_unavailable");return false;}
    LARGE_INTEGER q;if(!QueryPerformanceFrequency(&q) || q.QuadPart<=0)return false;frequency=q.QuadPart;
    force_scope=scope;
    Obj relay=allocate_near(image+entry);if(!relay)return false;
    DWORD old;
    if(!prepare(relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,96)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    if(!RtlAddFunctionTable(reinterpret_cast<RUNTIME_FUNCTION *>(relay+64),1,reinterpret_cast<DWORD64>(relay))) {
        VirtualFree(relay,0,MEM_RELEASE);return false;
    }
    active=true;
    // Retain gate/unwind storage after publication attempts for any in-flight original caller.
    if(!publish_link(link)) {active=false;restore_link(link);return false;}
    log_event("path_count_installed");return true;
}
#endif
}
#endif
#if !defined(NO_RUNTIME_LOG) && defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
#define PATH_COUNT_FLUSH() path_count::flush()
#else
#define PATH_COUNT_FLUSH() ((void)0)
#endif
