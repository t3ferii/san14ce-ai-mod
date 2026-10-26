// [performance] appear_batch (2026-10-25, user request): faster 0-day army appearance after 전략중.
// Evidence (REA carrier REA_Analysis/turnstart_20261025, unpacked 1fbe6be7..., measurements in
// data/diagnostics/turn_start_wait_20261025.md): CAppearArmyState (vtable 13D7298) slot 5 (+0x28) =
// 44AC80, called once per frame on the UI thread (571F90). Each call: +480 set -> clear it and return
// (a rested frame); otherwise, while 332E90()==0 and the list +478 is not empty, one army runs through
// 25C070(army,0xBD10) (map flag + army info layouts) and the list advances; an army that took
// >= 22 ms (332E40) sets +480. Empty list or 332E90()!=0 -> 12540 ends the state. One army per frame
// plus rests made the progress thread wait 1.5-5 s per turn in 영웅난무.
// Mod: the original update runs several times in one frame within kBudgetUs, without rested frames.
// Armies, order and every native call are the original ones; the state is still ended by the
// original call of a later frame. UI thread only (the original caller).
#include "appear_batch.h"
namespace face_keep {static void prefetch(Obj state);}   // native_face_keep.inl: [performance] face_prefetch
namespace appear_batch {
static const unsigned kSlot=0x13D72C0,kUpdate=0x44AC80,kStop=0x332E90;
static const unsigned long long kUpdateHash=0x2b3ba4c85e82964eull,kStopHash=0xa2521e4dd50f954full;
// 2026-10-25 user: 12 -> 100 -> 50 ms. One army costs 15-40 ms here (measured): 12 ms still gave one
// army per frame; 100 ms (3-5 per frame, ~10 fps) felt choppy; 50 ms gives about 2-3 per frame (~20 fps).
static constexpr long long kBudgetUs=50000;
using Update=void(__fastcall *)(Obj,unsigned long long,unsigned long long,unsigned long long);
static LONGLONG frequency;
static unsigned long long fingerprint(unsigned rva,unsigned size) {
    unsigned long long h=0xcbf29ce484222325ull;
    for(unsigned i=0;i<size;++i)h=(h^image[rva+i])*0x100000001b3ull;
    return h;
}
struct Native {
    Obj state;unsigned long long a,b,c;
    bool pending() const {return pointer(state,0x478)!=nullptr;}
    const void *head() const {return pointer(state,0x478);}
    bool stop() const {return fn<int(__fastcall *)()>(kStop)()!=0;}
    void step() {fn<Update>(kUpdate)(state,a,b,c);}
    void clear_skip() {*reinterpret_cast<int *>(state+0x480)=0;}
    long long now() const {LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart*1000000/frequency;}
};
static void __fastcall update(Obj state,unsigned long long a,unsigned long long b,unsigned long long c) { COST_SCOPE("appear_batch_frame");
    Native native{state,a,b,c};
    // Faces of the armies still to appear are requested first, inside this frame's budget.
    const long long begin=native.now();
    face_keep::prefetch(state);
    const long long left=kBudgetUs-(native.now()-begin);
    const unsigned steps=appear_batch_core::run_frame(native,left>0?left:0);
    if(steps>1)log_event("appear_batch",steps);
}
static bool install() {
    LARGE_INTEGER q;
    if(pointer(image,kSlot)!=image+kUpdate || fingerprint(kUpdate,0xEF)!=kUpdateHash || fingerprint(kStop,0x39)!=kStopHash ||
       !QueryPerformanceFrequency(&q) || q.QuadPart<=0) {log_event("appear_batch_guard_rejected");return false;}
    frequency=q.QuadPart;
    if(!patch_pointer(kSlot,kUpdate,reinterpret_cast<void *>(&update)))return false;
    log_event("appear_batch_installed");return true;
}
}
