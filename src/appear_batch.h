#pragma once
// [performance] appear_batch core: one frame of the army-appearance state, no game access
// (native binding: native_appear_batch.inl; fixture: tests/appear_batch_fixture.cpp).
// S provides pending() (armies left), head() (list position), stop() (native finish condition),
// step() (the original per-frame update: one army), clear_skip() (drop the native one-frame rest),
// now() (microseconds).
namespace appear_batch_core {
template<class S> unsigned run_frame(S &s,long long budget_us) {
    // Nothing left or finishing: the original call ends the state; the object is not touched after it.
    if(!s.pending() || s.stop()) {s.step();return 1;}
    const long long begin=s.now();
    s.clear_skip();s.step();
    unsigned steps=1;
    // 2026-10-25 user: frames ran 65-90 ms with a 50 ms budget (an army started at 45 ms still ran its
    // full 15-40 ms). Another army starts only if the slowest one of this frame still fits.
    long long slowest=s.now()-begin;
    while(s.pending() && !s.stop() && s.now()-begin+slowest<=budget_us) {
        const void *head=s.head();
        const long long t=s.now();
        s.clear_skip();s.step();++steps;
        if(s.now()-t>slowest)slowest=s.now()-t;
        if(s.head()==head)break;   // the original did not advance (state inactive)
    }
    // The last army stays for the next frame's original call, which also ends the state.
    s.clear_skip();
    return steps;
}
}
