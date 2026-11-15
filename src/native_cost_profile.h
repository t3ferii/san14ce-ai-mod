#pragma once
// Local investigation only. No game calls, memory probes or per-call file I/O.
// Release builds do not define RUNTIME_COST_PROFILE and omit this entire writer.
#ifdef RUNTIME_COST_PROFILE
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
namespace costprofile {
constexpr unsigned maximum=1024;
static SRWLOCK registry_lock=SRWLOCK_INIT;
static const char *names[maximum];static unsigned registered;
static wchar_t file_path[MAX_PATH];static LONGLONG frequency;
// Diagnostic builds compile the writer but start it only for [diagnostics] cost_profile=1 (2026-10-23);
// the RuntimeCosts build (NO_RUNTIME_LOG) always starts it. Release builds compile none of it.
static bool on;
enum CrumbDirection { Leave=0, Enter=1, Call=2 };
struct Crumb {uint64_t tick;DWORD thread;const char *name;LONG enter;uintptr_t value;volatile LONG repeat;};
struct CrumbRing {volatile LONG thread;volatile LONG index;Crumb entries[256];};
static CrumbRing crumb_rings[64];
static volatile LONG crumbs_enabled;
static __declspec(thread) CrumbRing *thread_ring;
static __declspec(thread) bool pair_pending;
inline void set_crumbs_enabled(bool value) {InterlockedExchange(&crumbs_enabled,value?1:0);}
static CrumbRing *get_crumb_ring() {
    if(thread_ring)return thread_ring;
    LONG id=static_cast<LONG>(GetCurrentThreadId());
    for(unsigned i=0;i<63;++i) {
        LONG owner=InterlockedCompareExchange(&crumb_rings[i].thread,id,0);
        if(owner==0 || owner==id)return thread_ring=&crumb_rings[i];
    }
    return thread_ring=&crumb_rings[63];
}
static bool same_crumb(const Crumb &e,const char *name,LONG direction,uintptr_t value,DWORD thread) {
    return e.thread==thread && e.enter==direction && e.value==value && e.name && name && !strcmp(e.name,name);
}
static void crumb(const char *name,LONG direction,uintptr_t value=0) {
    if(!InterlockedCompareExchange(&crumbs_enabled,0,0))return;
    CrumbRing *ring=get_crumb_ring();DWORD thread=GetCurrentThreadId();
    LONG end=InterlockedCompareExchange(&ring->index,0,0);
    if(ring!=&crumb_rings[63] && end>0) {
        Crumb &last=ring->entries[static_cast<unsigned>(end)&255];
        if(pair_pending) {
            pair_pending=false;
            if(direction==Leave && same_crumb(last,name,Leave,value,thread)) {InterlockedIncrement(&last.repeat);return;}
        }
        if(same_crumb(last,name,direction,value,thread)) {InterlockedIncrement(&last.repeat);return;}
        if(direction==Enter && end>1) {
            Crumb &previous=ring->entries[static_cast<unsigned>(end-1)&255];
            if(same_crumb(previous,name,Enter,value,thread) && same_crumb(last,name,Leave,value,thread)) {
                InterlockedIncrement(&previous.repeat);pair_pending=true;return;
            }
        }
    }
    LONG sequence=InterlockedIncrement(&ring->index);
    Crumb &entry=ring->entries[static_cast<unsigned>(sequence)&255];
    entry.tick=GetTickCount64();entry.thread=thread;entry.name=name;entry.enter=direction;entry.value=value;entry.repeat=1;
}
struct Key {
    unsigned slot;
    explicit Key(const char *name) {
        AcquireSRWLockExclusive(&registry_lock);
        for(slot=0;slot<registered;++slot)if(!strcmp(names[slot],name))break;
        if(slot==registered && registered<maximum)names[registered++]=name;
        ReleaseSRWLockExclusive(&registry_lock);
    }
};
struct Entry {uint64_t calls;LONGLONG total,self,maximum;};
struct Frame {LONGLONG children;};
// local159 long-frame record: self time of every scope inside the running depth-0 scope, per key, for this one invocation.
struct ThreadState {Entry values[maximum];Frame frames[128];unsigned depth;ULONGLONG window,sequence,writes;
    LONGLONG invocation[maximum];unsigned short touched[256];unsigned touched_count;};
static __declspec(thread) ThreadState state;
static wchar_t stall_path[MAX_PATH];
inline void initialize(HMODULE module) {
    LARGE_INTEGER q;QueryPerformanceFrequency(&q);frequency=q.QuadPart;on=frequency>0;
    if(!GetModuleFileNameW(module,file_path,MAX_PATH)) {file_path[0]=0;return;}
    wchar_t *slash=wcsrchr(file_path,L'\\');if(!slash) {file_path[0]=0;return;}
    wcscpy_s(stall_path,file_path);
    swprintf_s(slash+1,MAX_PATH-static_cast<size_t>(slash+1-file_path),
#ifdef SPLIT_COMBAT
        L"AI_TacticsMod\\data\\performance\\function_cost_Combat_%lu.csv",
#else
        L"AI_TacticsMod\\data\\performance\\function_cost_Convenience_%lu.csv",
#endif
        GetCurrentProcessId());
    wchar_t *stall_slash=stall_path+(slash-file_path);
    swprintf_s(stall_slash+1,MAX_PATH-static_cast<size_t>(stall_slash+1-stall_path),
#ifdef SPLIT_COMBAT
        L"AI_TacticsMod\\data\\performance\\function_stall_Combat_%lu.csv",
#else
        L"AI_TacticsMod\\data\\performance\\function_stall_Convenience_%lu.csv",
#endif
        GetCurrentProcessId());
}
static void note_child(unsigned slot,LONGLONG self) {
    if(slot>=maximum)return;
    if(!state.invocation[slot] && state.touched_count<256)state.touched[state.touched_count++]=static_cast<unsigned short>(slot);
    state.invocation[slot]+=self?self:1;
}
// A depth-0 scope of 50 ms or more (user 2026-11-14 순-boundary stutter): one line with the root, its time, its own time and the
// ten largest self times inside it (name=ms). Written at once (rare); the per-second CSV keeps the totals.
static void long_root(unsigned slot,LONGLONG elapsed,LONGLONG self) {
    if(stall_path[0] && frequency>0 && elapsed*20>=frequency && slot<maximum) {
        char line[2048];size_t used=0;
        int n=_snprintf_s(line,sizeof(line),_TRUNCATE,"%llu,%lu,%s,%.3f,%.3f",GetTickCount64(),GetCurrentThreadId(),names[slot],
            1000.0*elapsed/frequency,1000.0*self/frequency);
        if(n>0)used=static_cast<size_t>(n);
        bool taken[256]={};
        for(unsigned k=0;k<10 && used<sizeof(line)-64;++k) {
            int best=-1;
            for(unsigned i=0;i<state.touched_count;++i)if(!taken[i] && (best<0 || state.invocation[state.touched[i]]>state.invocation[state.touched[best]]))best=static_cast<int>(i);
            if(best<0)break;
            taken[best]=true;const unsigned s=state.touched[best];
            n=_snprintf_s(line+used,sizeof(line)-used,_TRUNCATE,",%s=%.3f",names[s],1000.0*state.invocation[s]/frequency);
            if(n<0)break;used+=static_cast<size_t>(n);
        }
        if(used<sizeof(line)-2) {line[used++]='\r';line[used++]='\n';}
        HANDLE file=CreateFileW(stall_path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE) {DWORD written=0;WriteFile(file,line,static_cast<DWORD>(used),&written,nullptr);CloseHandle(file);}
    }
    for(unsigned i=0;i<state.touched_count;++i)state.invocation[state.touched[i]]=0;
    state.touched_count=0;
}
static void flush() {
    if(state.depth || !file_path[0] || frequency<=0)return;
    char buffer[maximum*256];size_t used=0;ULONGLONG now=GetTickCount64();
    unsigned count;AcquireSRWLockShared(&registry_lock);count=registered;ReleaseSRWLockShared(&registry_lock);
    for(unsigned i=0;i<count;++i) {const Entry &e=state.values[i];if(!e.calls)continue;
        int n=_snprintf_s(buffer+used,sizeof(buffer)-used,_TRUNCATE,
            "%llu,%lu,%llu,%s,%llu,%.6f,%.6f,%.6f,%llu\r\n",now,GetCurrentThreadId(),state.sequence,names[i],
            e.calls,1000.0*e.total/frequency,1000.0*e.self/frequency,1000.0*e.maximum/frequency,now-state.window);
        if(n<0)return;used+=static_cast<size_t>(n);
    }
    if(!used)return;
    HANDLE file=CreateFileW(file_path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    DWORD written=0;BOOL ok=WriteFile(file,buffer,static_cast<DWORD>(used),&written,nullptr);CloseHandle(file);
    if(ok && written==used) {memset(state.values,0,sizeof(state.values));state.window=now;++state.sequence;++state.writes;}
}
struct Scope {
    unsigned slot,frame;LONGLONG begin;bool active;
    explicit Scope(const Key &key):slot(key.slot),frame(state.depth),begin(0),active(frequency>0 && slot<maximum && state.depth<128) {
        crumb(key.slot<maximum?names[key.slot]:"unknown",true);
        if(!active)return;
        if(!state.window)state.window=GetTickCount64();state.frames[state.depth++]={};
        LARGE_INTEGER q;QueryPerformanceCounter(&q);begin=q.QuadPart;
    }
    ~Scope() {
        crumb(slot<maximum?names[slot]:"unknown",false);
        if(!active)return;
        LARGE_INTEGER q;QueryPerformanceCounter(&q);LONGLONG elapsed=q.QuadPart-begin;
        Entry &e=state.values[slot];++e.calls;e.total+=elapsed;
        const LONGLONG self=elapsed>state.frames[frame].children?elapsed-state.frames[frame].children:0;
        e.self+=self;
        if(elapsed>e.maximum)e.maximum=elapsed;
        --state.depth;if(state.depth) {state.frames[state.depth-1].children+=elapsed;note_child(slot,self);}
        else {long_root(slot,elapsed,self);if(GetTickCount64()-state.window>=1000)flush();}
    }
};
}
#define COST_JOIN_INNER(a,b) a##b
#define COST_JOIN(a,b) COST_JOIN_INNER(a,b)
#define COST_SCOPE_INDEX(name,n) static const costprofile::Key COST_JOIN(cost_key_,n)(name); costprofile::Scope COST_JOIN(cost_scope_,n)(COST_JOIN(cost_key_,n))
#define COST_SCOPE(name) COST_SCOPE_INDEX(name,__COUNTER__)
#ifndef NO_RUNTIME_LOG
#define COST_CRUMB(name,value) costprofile::crumb(name,costprofile::Enter,reinterpret_cast<uintptr_t>(value))
#else
#define COST_CRUMB(name,value) do {} while(0)
#endif
#else
#define COST_SCOPE(name) do {} while(0)
#define COST_CRUMB(name,value) do {} while(0)
#endif
