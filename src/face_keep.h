#pragma once
// [performance] face_keep core: bounded, reusable face image references (UI thread only).
// No game access (native binding: native_face_keep.inl; fixture: tests/face_keep_fixture.cpp).
#include <cstddef>
namespace face_keep_core {
// User decision 2026-10-25: after another PC's ~10-turn crash report (cause unknown), keep
// the 256 cap (~600 MB estimate), but replace LRU pins and trim on physical-memory pressure.
constexpr size_t kMax=256;
// User/Claude thresholds, 2026-10-25: leave RAM headroom for the game/OS on smaller PCs.
// These are precautionary limits, not a demonstrated cause or cure of that crash.
constexpr unsigned kMemoryLoad=85;
// User decision 2026-10-25: one-second checks are too frequent; sample at most every 10 seconds.
constexpr unsigned long long kAvailPhys=1536ull*1024*1024,kMemoryIntervalMs=10000;
struct Pins {
    const void *table=nullptr;   // the manager's reference-count array; a new one starts over
    int ids[kMax];unsigned long long used[kMax],sequence=0;size_t count=0;
    void reset(const void *t) {if(t!=table) {table=t;count=0;sequence=0;}}
    bool has(int id) const {for(size_t i=0;i<count;++i)if(ids[i]==id)return true;return false;}
    bool touch(int id) {
        for(size_t i=0;i<count;++i)if(ids[i]==id) {used[i]=++sequence;return true;}
        return false;
    }
    void erase(size_t i) {--count;ids[i]=ids[count];used[i]=used[count];}
};
// Commit only after the native add-reference succeeds. A prefetch's request reference is its pin.
inline void add_pin(Pins &p,int id) {
    if(!p.touch(id) && p.count<kMax) {p.ids[p.count]=id;p.used[p.count++]=++p.sequence;}
}
// Requested jobs whose wait was skipped. Before the face is set, the original wait runs for it once.
struct Pending {
    const void *table=nullptr;int ids[kMax];const void *jobs[kMax];bool done[kMax];size_t count=0;
    void reset(const void *t) {if(t!=table) {table=t;count=0;}}
    void forget(int id) {
        for(size_t i=0;i<count;++i)if(ids[i]==id) {
            --count;ids[i]=ids[count];jobs[i]=jobs[count];done[i]=done[count];return;
        }
    }
    void add(int id,const void *job) {
        forget(id);
        size_t i=0;while(i<count && !done[i])++i;
        if(i==count) {if(count==kMax)return;++count;}
        ids[i]=id;jobs[i]=job;done[i]=false;
    }
    // Index of an unfinished request for id (and the same job still in the table), or -1.
    long waiting(int id,const void *job) const {
        for(size_t i=0;i<count;++i)if(ids[i]==id)return !done[i] && jobs[i]==job?static_cast<long>(i):-1;
        return -1;
    }
};
struct Memory {
    unsigned long long checked_ms=0;bool checked=false,low=false;
    // read(load,available) returns false on query failure; preserve the last successful state.
    template<class Read> bool sample(unsigned long long now,Read read) {
        if(checked && now-checked_ms<kMemoryIntervalMs)return false;
        checked=true;checked_ms=now;
        unsigned load=0;unsigned long long available=0;
        if(!read(load,available))return false;
        low=load>=kMemoryLoad || available<kAvailPhys;return true;
    }
};
// Callbacks execute synchronously on the caller's UI thread. An unfinished matching prefetch
// is ineligible even if its texture already exists. A stale zero ref only loses its list entry.
template<class Waiting,class Count,class Release,class Removed>
inline size_t trim(Pins &p,size_t n,unsigned reason,Waiting waiting,Count refs,Release release,Removed removed) {
    size_t dropped=0;
    while(dropped<n) {
        size_t oldest=p.count;
        for(size_t i=0;i<p.count;++i)
            if(!waiting(p.ids[i]) && (oldest==p.count || p.used[i]<p.used[oldest]))oldest=i;
        if(oldest==p.count)break;
        const int id=p.ids[oldest];
        if(refs(id)>0)release(id);   // recheck immediately before the native decrement
        p.erase(oldest);++dropped;removed(id,reason,p.count);
    }
    return dropped;
}
// Decide before adding a new reference. Reuse updates LRU on both set_face and prefetch paths.
// Pressure is sampled at most once/10 seconds; trim once per successful low-memory sample, not once
// per army in the same frame. The cached low state stops new pins until a healthy sample arrives.
template<class Read,class Waiting,class Count,class Release,class Removed>
inline bool prepare_pin(Pins &p,const void *table,int id,int refs,bool prefetch,Memory &memory,
                        unsigned long long now,Read read,Waiting waiting,Count count_of,Release release,Removed removed) {
    p.reset(table);
    if(!table || id<=0)return false;
    if(p.touch(id))return false;
    if(prefetch?refs!=0:refs<=0)return false;
    const bool sampled=memory.sample(now,read);
    if(memory.low) {
        if(sampled)trim(p,(p.count+1)/2,2,waiting,count_of,release,removed);   // odd count rounds up
        return false;
    }
    if(p.count==kMax)trim(p,1,1,waiting,count_of,release,removed);
    return p.count<kMax;   // all pins may still be waiting: leave the new face to the original
}
}
