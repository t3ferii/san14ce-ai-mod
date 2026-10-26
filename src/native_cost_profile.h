#pragma once
// Local investigation only. No game calls, memory probes or per-call file I/O.
// Release builds do not define RUNTIME_COST_PROFILE and omit this entire writer.
#ifdef RUNTIME_COST_PROFILE
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
namespace costprofile {
constexpr unsigned maximum=192;
static SRWLOCK registry_lock=SRWLOCK_INIT;
static const char *names[maximum];static unsigned registered;
static wchar_t file_path[MAX_PATH];static LONGLONG frequency;
// Diagnostic builds compile the writer but start it only for [diagnostics] cost_profile=1 (2026-10-23);
// the RuntimeCosts build (NO_RUNTIME_LOG) always starts it. Release builds compile none of it.
static bool on;
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
struct ThreadState {Entry values[maximum];Frame frames[128];unsigned depth;ULONGLONG window,sequence,writes;};
static __declspec(thread) ThreadState state;
static void initialize(HMODULE module) {
    LARGE_INTEGER q;QueryPerformanceFrequency(&q);frequency=q.QuadPart;on=frequency>0;
    if(!GetModuleFileNameW(module,file_path,MAX_PATH)) {file_path[0]=0;return;}
    wchar_t *slash=wcsrchr(file_path,L'\\');if(!slash) {file_path[0]=0;return;}
    swprintf_s(slash+1,MAX_PATH-static_cast<size_t>(slash+1-file_path),
#ifdef SPLIT_COMBAT
        L"AI_TacticsMod\\data\\performance\\function_cost_Combat_%lu.csv",
#else
        L"AI_TacticsMod\\data\\performance\\function_cost_Convenience_%lu.csv",
#endif
        GetCurrentProcessId());
}
static void flush() {
    if(state.depth || !file_path[0] || frequency<=0)return;
    char buffer[24576];size_t used=0;ULONGLONG now=GetTickCount64();
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
        if(!active)return;
        if(!state.window)state.window=GetTickCount64();state.frames[state.depth++]={};
        LARGE_INTEGER q;QueryPerformanceCounter(&q);begin=q.QuadPart;
    }
    ~Scope() {
        if(!active)return;
        LARGE_INTEGER q;QueryPerformanceCounter(&q);LONGLONG elapsed=q.QuadPart-begin;
        Entry &e=state.values[slot];++e.calls;e.total+=elapsed;
        e.self+=(elapsed>state.frames[frame].children?elapsed-state.frames[frame].children:0);
        if(elapsed>e.maximum)e.maximum=elapsed;
        --state.depth;if(state.depth)state.frames[state.depth-1].children+=elapsed;
        else if(GetTickCount64()-state.window>=1000)flush();
    }
};
}
#define COST_JOIN_INNER(a,b) a##b
#define COST_JOIN(a,b) COST_JOIN_INNER(a,b)
#define COST_SCOPE_INDEX(name,n) static const costprofile::Key COST_JOIN(cost_key_,n)(name); costprofile::Scope COST_JOIN(cost_scope_,n)(COST_JOIN(cost_key_,n))
#define COST_SCOPE(name) COST_SCOPE_INDEX(name,__COUNTER__)
#else
#define COST_SCOPE(name) do {} while(0)
#endif
