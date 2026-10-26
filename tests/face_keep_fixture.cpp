// Mock face_keep policy (2026-10-25 user): keep cap 256 after another PC's ~10-turn
// crash report (cause unknown), add LRU replacement and physical-memory-pressure trimming.
// User decision 2026-10-25: memory checks move from one second to 10 seconds; test both boundaries.
// Fake manager: 8A6DE0 (count 0 -> load, +1) and 8A7EA0 (-1, count 0 -> unload).
#include "../src/face_keep.h"
#include <cstdio>
#include <vector>
using namespace face_keep_core;
struct Manager {
    std::vector<int> counts;std::vector<const void *> jobs;unsigned loads=0,unloads=0;
    explicit Manager(size_t n):counts(n,0),jobs(n,nullptr) {}
    void add(int id) {if(counts[id]==0)++loads;++counts[id];}
    void release(int id) {if(--counts[id]==0)++unloads;}
};
struct Event {int id;unsigned reason;size_t remaining;};
struct Keep {
    Pins pins;Pending pending;Memory memory;
    unsigned load=0,queries=0;unsigned long long available=kAvailPhys,now=0;
    bool query_ok=true;
    std::vector<int> releases;std::vector<Event> events;
    bool prepare(Manager &m,const void *table,int id,int refs,bool ahead=false) {
        pending.reset(table);
        return prepare_pin(pins,table,id,refs,ahead,memory,now,
            [this](unsigned &l,unsigned long long &a) {++queries;l=load;a=available;return query_ok;},
            [this,&m](int i) {return pending.waiting(i,m.jobs[i])>=0;},
            [&m](int i) {return m.counts[i];},
            [this,&m](int i) {releases.push_back(i);m.release(i);},
            [this](int i,unsigned reason,size_t count) {pending.forget(i);events.push_back({i,reason,count});});
    }
    bool request(Manager &m,int id,const void *job=nullptr) {
        if(!prepare(m,m.counts.data(),id,m.counts[id],true))return false;
        m.add(id);add_pin(pins,id);
        if(job) {m.jobs[id]=job;pending.add(id,job);}
        return true;
    }
};
static int checks=0,failures=0;
static void ok(bool v,const char *what) {++checks;if(!v){++failures;std::printf("FAIL %s\n",what);}}
// One army flag: original sets the face (5 nodes, one image), later destroys the flag.
static void flag(Manager &m,Keep &k,int id,bool keep=true) {
    m.add(id);
    if(keep && k.prepare(m,m.counts.data(),id,m.counts[id])) {m.add(id);add_pin(k.pins,id);}
    m.release(id);
}
static void fill(Manager &m,Keep &k) {for(int id=1;id<=static_cast<int>(kMax);++id)flag(m,k,id);}
int main() {
    {   // same commander in three turns: one load, never unloaded
        Manager m(1000);Keep k;
        for(int turn=0;turn<3;++turn)flag(m,k,42);
        ok(m.loads==1 && m.unloads==0,"kept face: one load over three turns");
        ok(m.counts[42]==1,"exactly one extra reference");
        ok(k.pins.sequence==3,"every repeated set_face updates last-use sequence");
    }
    {   // original behaviour for comparison
        Manager m(1000);Keep k;
        for(int turn=0;turn<3;++turn)flag(m,k,42,false);
        ok(m.loads==3 && m.unloads==3,"original: load and unload every turn");
    }
    {   // never pins an image that is not loaded (count 0): no load from the mod
        Manager m(1000);Keep k;
        ok(!k.prepare(m,m.counts.data(),7,0) && m.loads==0,"count 0 is not pinned");
        ok(!k.prepare(m,m.counts.data(),0,3),"id 0 is not pinned");
        ok(!k.prepare(m,nullptr,7,3),"no table, no pin");
        ok(k.queries==0 && k.releases.empty(),"invalid candidates do not query or evict");
    }
    {   // User decision 2026-10-25: cap stays 256; replace LRU instead of refusing new faces.
        Manager m(2000);Keep k;
        for(int id=1;id<=400;++id)flag(m,k,id);
        ok(kMax==256 && k.pins.count==kMax,"at most kMax=256 pins (latest user decision)");
        ok(m.counts[kMax]==1 && m.counts[kMax+1]==1,"new faces replace old pins at the cap");
        ok(m.counts[144]==0 && m.counts[145]==1 && m.counts[400]==1,"only the 256 most recent faces remain pinned");
        ok(k.releases.size()==144 && k.releases.front()==1 && k.releases.back()==144,"one LRU release per replacement");
        ok(k.events.front().reason==1 && k.events.front().remaining==255,"cap release reason and post-release count");
        ok(m.loads==400 && m.unloads==144,"LRU drops exactly one reference and unloads at zero");
    }
    {   // new manager/count array: starts over; never release references from the old table
        Manager a(100),b(100);Keep k;
        flag(a,k,5);ok(k.pins.count==1,"pinned in the first game");
        flag(b,k,5);ok(k.pins.count==1 && b.counts[5]==1,"new table: pinned again, list restarted");
        ok(k.releases.empty() && a.counts[5]==1,"new table: no release call against old resources");
    }
    {   // existing prefetch contract: skipped wait runs once, wrong job never matches
        Manager m(1000);Keep k;int job[3];
        ok(k.request(m,9,&job[0]),"unloaded face may be requested");
        ok(!k.request(m,9),"requested once only");
        ok(!k.prepare(m,m.counts.data(),8,2,true),"a loaded face is not requested (face_keep pins it)");
        ok(k.pending.waiting(9,&job[0])==0,"set_face waits for the requested job");
        k.pending.done[0]=true;ok(k.pending.waiting(9,&job[0])==-1,"wait runs once");
        k.pending.add(4,&job[1]);ok(k.pending.waiting(4,&job[2])==-1,"a different job in the table is not waited on");
        m.add(9);m.release(9);ok(m.loads==1 && m.unloads==0 && m.counts[9]==1,"requested face stays as a pin");
        k.pending.reset(nullptr);ok(k.pending.count==0,"new table drops requests");
        // User decision 2026-10-25: cap remains 256; prefetch replaces an eligible LRU pin.
        Manager full(1000);Keep f;fill(full,f);
        ok(f.request(full,999) && f.pins.count==kMax,"prefetch replaces a pin at the cap");
        ok(f.releases.size()==1 && f.releases[0]==1 && full.counts[999]==1,"prefetch replacement owns one reference");
    }
    {   // set_face reuse changes the eviction victim
        Manager m(1000);Keep k;fill(m,k);flag(m,k,1);flag(m,k,257);
        ok(k.releases.size()==1 && k.releases[0]==2,"recently reused face is not the LRU victim");
        ok(m.counts[1]==1 && m.counts[2]==0 && m.counts[257]==1,"reuse preserves hot face and pins newcomer");
    }
    {   // prefetch reuse refreshes LRU, even though it needs no new request
        Manager m(1000);Keep k;fill(m,k);
        const auto before=k.pins.sequence;
        ok(!k.request(m,1) && k.pins.sequence==before+1,"prefetch reuse refreshes the use sequence");
        flag(m,k,257);ok(k.releases[0]==2,"prefetch reuse changes the eviction victim");
    }
    {   // User decision 2026-10-25: no requery before 10 seconds; requery at the 10-second boundary.
        Memory memory;unsigned queries=0,load=0;
        auto read=[&](unsigned &l,unsigned long long &a) {++queries;l=load;a=kAvailPhys;return true;};
        ok(kMemoryIntervalMs==10000,"memory interval is exactly 10 seconds");
        ok(memory.sample(0,read) && queries==1 && !memory.low,"initial memory query at zero");
        load=85;
        ok(!memory.sample(1000,read) && queries==1 && !memory.low,"one second: no memory requery");
        ok(!memory.sample(9999,read) && queries==1 && !memory.low,"before 10 seconds: no memory requery");
        ok(memory.sample(10000,read) && queries==2 && memory.low,"at 10 seconds: memory is requeried");
    }
    {   // 85% load -> halve once per sampled 10 seconds; cache pressure until recovery.
        Manager m(1000);Keep k;fill(m,k);k.load=85;k.now=10000;flag(m,k,257);
        ok(k.pins.count==128 && k.releases.size()==128,"memory load 85: half the pins released");
        ok(!k.pins.has(257) && m.counts[257]==0,"pressure: no new pin, original releases new face");
        ok(k.releases.front()==1 && k.releases.back()==128,"memory trim follows LRU order");
        ok(k.events.front().reason==2 && k.events.front().remaining==255 && k.events.back().remaining==128,"memory reason and decreasing pin counts");
        for(int id=258;id<268;++id)flag(m,k,id);
        ok(k.pins.count==128 && k.queries==2,"same 10-second interval: no repeated query or half-trim");
        k.load=0;k.now=19999;flag(m,k,268);
        ok(!k.pins.has(268) && k.queries==2,"cached pressure blocks before 10 seconds elapse");
        k.now=20000;flag(m,k,269);
        ok(k.pins.has(269) && k.pins.count==129 && k.queries==3,"at 10 seconds: healthy requery resumes new pins");
    }
    {   // exact RAM threshold; pressure can occur before the cap
        Manager m(1000);Keep k;k.load=84;k.available=kAvailPhys;
        flag(m,k,1);flag(m,k,2);flag(m,k,3);
        ok(k.pins.count==3,"84 percent and exactly 1.5 GiB available are healthy");
        k.now=10000;k.available=kAvailPhys-1;flag(m,k,4);
        ok(k.pins.count==1 && k.releases.size()==2 && k.releases[0]==1 && k.releases[1]==2,"below 1.5 GiB trims half, odd count rounds up");
        ok(m.counts[4]==0 && !k.pins.has(4),"low available RAM alone blocks new pins");
        k.now=20000;flag(m,k,5);
        ok(k.pins.count==0 && k.releases.back()==3,"persistent pressure can release the last pin");
    }
    {   // unfinished matching job is protected even though its reference count is nonzero
        Manager m(1000);Keep k;int job;
        ok(k.request(m,1,&job),"start unfinished prefetch");
        for(int id=2;id<=256;++id)flag(m,k,id);
        flag(m,k,257);
        ok(k.releases[0]==2 && k.pins.has(1) && m.counts[1]==1,"cap eviction skips the oldest unfinished prefetch");
        const long i=k.pending.waiting(1,&job);k.pending.done[i]=true;
        flag(m,k,258);
        ok(k.releases.back()==1 && !k.pins.has(1),"finished prefetch becomes eligible for release");
        ok(k.pending.waiting(1,&job)==-1,"eviction forgets its pending record");
    }
    {   // all pending: no safe victim, leave new face to the original
        Manager m(1000);Keep k;int job;
        for(int id=1;id<=256;++id)k.request(m,id,&job);
        flag(m,k,257);
        ok(k.pins.count==kMax && k.releases.empty() && m.counts[257]==0,"all waiting: no release or new pin at cap");
        k.load=85;k.now=10000;flag(m,k,258);
        ok(k.pins.count==kMax && k.releases.empty() && m.counts[258]==0,"pressure never releases unfinished jobs even if half cannot be freed");
    }
    {   // skip waiting but still trim half when enough other entries are eligible
        Manager m(1000);Keep k;int job;
        k.request(m,1,&job);for(int id=2;id<=256;++id)flag(m,k,id);
        k.load=85;k.now=10000;flag(m,k,257);
        ok(k.pins.count==128 && k.pins.has(1) && k.releases.front()==2 && k.releases.back()==129,"memory LRU skips pending while trimming half");
        ok(k.pending.waiting(1,&job)>=0,"memory trim preserves pending wait record");
    }
    {   // replaced job no longer matches the unfinished record
        Manager m(1000);Keep k;int job[2];k.request(m,1,&job[0]);
        for(int id=2;id<=256;++id)flag(m,k,id);
        m.jobs[1]=&job[1];flag(m,k,257);
        ok(k.releases[0]==1,"nonmatching job does not protect stale pending record");
    }
    {   // double-release guard: stale zero/negative references only leave the list
        for(int stale=0;stale>=-1;--stale) {
            Manager m(1000);Keep k;fill(m,k);m.counts[1]=stale;flag(m,k,257);
            ok(k.releases.empty() && m.counts[1]==stale,"nonpositive ref: never call original release");
            ok(!k.pins.has(1) && k.pins.has(257) && k.pins.count==kMax,"stale pin is removed and replaced");
        }
    }
    {   // reload: discard pins AND waiting jobs without calling the old table
        Manager a(1000),b(1000);Keep k;int job;fill(a,k);k.pending.add(1,&job);
        k.load=85;k.now=10000;flag(b,k,257);
        ok(k.pins.count==0 && k.pending.count==0 && k.releases.empty(),"table change under pressure resets bookkeeping without release");
        ok(a.counts[1]==1 && a.counts[256]==1,"old table reference counts untouched");
    }
    {   // completed entries must not exhaust the bounded Pending table over many turns
        Manager m(1000);Keep k;int job;bool all_requested=true,all_waited=true;
        for(int id=1;id<=800;++id) {
            all_requested=k.request(m,id,&job) && all_requested;
            const long i=k.pending.waiting(id,&job);
            if(i>=0)k.pending.done[i]=true;else all_waited=false;
        }
        ok(all_requested && all_waited,"800 sequential prefetches retain one-time wait tracking");
        ok(k.pins.count==kMax && k.pending.count<=kMax && k.releases.size()==800-kMax,"long-run pin/pending counts stay bounded");
    }
    {   // failed query is throttled too; preserve previously observed pressure
        Manager m(1000);Keep k;k.query_ok=false;flag(m,k,1);flag(m,k,2);
        ok(k.queries==1 && k.pins.count==2,"failed first query does not retry per face");
        k.now=10000;k.query_ok=true;k.load=85;flag(m,k,3);
        k.now=20000;k.query_ok=false;flag(m,k,4);
        ok(k.memory.low && !k.pins.has(4) && k.pins.count==1,"query failure preserves cached low state without repeating trim");
    }
    std::printf("{\"status\":\"%s\",\"checks\":%d,\"failures\":%d,\"gameplay_verified\":false}\n",failures?"failed":"face_keep_fixture_passed",checks,failures);
    return failures?1:0;
}
