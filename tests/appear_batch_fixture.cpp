// Mock check of appear_batch_core::run_frame (2026-10-25 user: several armies per frame).
// The fake step mirrors 44AC80: rest flag -> clear and return; finish condition or empty list -> end;
// otherwise one army (fixed cost) and the rest flag when it took >= the native 22 ms budget.
#include "../src/appear_batch.h"
#include <cstdio>
#include <vector>
struct Fake {
    std::vector<int> cost;     // microseconds per army
    size_t at=0;int skip=0;bool ended=false,inactive=false,finish=false;long long clock=0;
    unsigned steps_after_end=0,armies=0;
    bool pending() const {return at<cost.size();}
    const void *head() const {return pending()?&cost[at]:nullptr;}
    bool stop() const {return finish;}
    void clear_skip() {if(ended)++steps_after_end;skip=0;}
    long long now() const {return clock;}
    void step() {
        if(ended) {++steps_after_end;return;}
        clock+=50;
        if(skip) {skip=0;return;}
        if(inactive)return;
        if(finish || !pending()) {ended=true;return;}
        clock+=cost[at];++armies;if(cost[at]>=22000)skip=1;++at;
    }
};
static int checks=0,failures=0;
static void ok(bool v,const char *what) {++checks;if(!v){++failures;std::printf("FAIL %s\n",what);}}
int main() {
    {   // cheap armies: many per frame within 12 ms, never a rested frame
        Fake f;f.cost.assign(100,3000);unsigned frames=0;
        while(!f.ended && frames<1000){appear_batch_core::run_frame(f,12000);++frames;}
        ok(f.ended && f.armies==100,"all armies appear and the state ends");
        ok(frames>=30 && frames<=36,"three armies per frame: a fourth would pass the budget");
        ok(f.steps_after_end==0,"no access after the native end");
    }
    {   // the original: one army per frame (budget 0 still runs the first step)
        Fake f;f.cost.assign(10,3000);unsigned frames=0;
        while(!f.ended && frames<1000){appear_batch_core::run_frame(f,0);++frames;}
        ok(f.armies==10 && frames==11,"zero budget = one army per frame + ending frame");
    }
    {   // slow armies: no rested frames (the native would rest after each one)
        Fake f;f.cost.assign(10,30000);unsigned frames=0;
        while(!f.ended && frames<1000){appear_batch_core::run_frame(f,12000);++frames;}
        ok(f.armies==10 && frames==11,"slow armies: one per frame, no rest");
        ok(f.skip==0,"rest flag cleared");
    }
    {   // native finish condition: no extra step, the original call ends the state
        Fake f;f.cost.assign(5,1000);f.finish=true;
        unsigned s=appear_batch_core::run_frame(f,12000);
        ok(s==1 && f.ended && f.armies==0,"finish condition: single original call");
        ok(f.steps_after_end==0,"no access after finish");
    }
    {   // finish raised during the frame stops the batch
        Fake f;f.cost.assign(5,1000);
        appear_batch_core::run_frame(f,12000);
        ok(f.armies==5 && !f.ended,"last army stays; end left to the next original call");
        appear_batch_core::run_frame(f,12000);ok(f.ended,"next frame ends the state");
    }
    {   // predictive stop (2026-10-25 user): the frame never runs past budget + one army
        Fake f;f.cost.assign(40,20000);unsigned frames=0;long long worst=0;
        while(!f.ended && frames<1000){const long long b=f.clock;appear_batch_core::run_frame(f,50000);if(f.clock-b>worst)worst=f.clock-b;++frames;}
        ok(f.armies==40,"all armies appear");
        ok(worst<=50000+200,"20 ms armies: at most two per 50 ms frame (no third at 40 ms)");
    }
    {   // inactive state: the original does nothing, the loop stops at once
        Fake f;f.cost.assign(5,1000);f.inactive=true;
        unsigned s=appear_batch_core::run_frame(f,12000);
        ok(s==2 && f.armies==0,"no progress -> stop after one retry");
    }
    {   // a rest requested before install is not taken
        Fake f;f.cost.assign(3,1000);f.skip=1;
        appear_batch_core::run_frame(f,12000);ok(f.armies==3,"stale rest flag ignored");
    }
    std::printf("{\"status\":\"%s\",\"checks\":%d,\"failures\":%d,\"gameplay_verified\":false}\n",failures?"failed":"appear_batch_fixture_passed",checks,failures);
    return failures?1:0;
}
