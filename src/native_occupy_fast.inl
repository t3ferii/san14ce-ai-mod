// Shorter auto-capture presentation (CEncirclingOccupationMovieState, vtable 13D3F80).
// Evidence: REA_Analysis/occupy_20261018/README.md. Slot 5 (42B7E0) runs phase +470:
//   0 camera move to the tile, 500..2000 ms (call 451360 at 42B507)
//   3 wait until the occupation effect (+578) frame +74 > 0x46 (cues at 1,5,9,13,0x21)
//   6 wait 26 frames (counter +5C0) or a tile effect frame > 0x19
//   7 wait 31 frames (counter +5C0) or a tile effect frame > 0x1E
// v12.8: each wait is cut to about a quarter (half of v11.31): camera ms/4,
// effect frames 0x0E..0x1F skipped (no cue there), counters x4. Message layouts (+5B0/+5B8) keep their own
// animations; the original code always runs on the original caller thread.
// [features] occupy_fast (convenience group, default on).
namespace occupy_fast {
static const unsigned kUpdateSlot=0x13D3FA8,kUpdate=0x42B7E0,kCameraCall=0x42B507,kCamera=0x451360;
static const unsigned long long kUpdateHash=0x8c495743e2765e0bull,kCameraPhaseHash=0xc085e89a45bc1993ull,
    kEffectPhaseHash=0xbcee66f750a7d08full,kTilePhaseHash=0xe383719e7dba05c2ull,kLastPhaseHash=0x546e9bb3037ce9a8ull;
static const unsigned kEffectEnd=0x47,kEffectShort=0x24;   // after the last cue at 0x21
static const unsigned kGapStart=0x0E,kGapEnd=0x20;           // between cues 13 and 0x21
using Update=void(__fastcall *)(Obj,unsigned long long,unsigned long long,unsigned long long);
using Camera=unsigned long long(__fastcall *)(unsigned,int,int,double,double,int);
static unsigned long long fingerprint(unsigned rva,unsigned size) {
    unsigned long long h=0xcbf29ce484222325ull;
    for(unsigned i=0;i<size;++i)h=(h^image[rva+i])*0x100000001b3ull;
    return h;
}
static bool guards_match() {
    return pointer(image,kUpdateSlot)==image+kUpdate && image[kCameraCall]==0xE8 &&
        kCameraCall+5+*reinterpret_cast<int *>(image+kCameraCall+1)==kCamera &&
        fingerprint(kUpdate,0xE8)==kUpdateHash && fingerprint(0x42B310,0x217)==kCameraPhaseHash &&
        fingerprint(0x42C190,0xF7)==kEffectPhaseHash && fingerprint(0x42BB90,0x38C)==kTilePhaseHash &&
        fingerprint(0x426DA0,0x157)==kLastPhaseHash;
}
static unsigned long long __fastcall camera(unsigned tile,int mode,int ms,double a,double b,int flag) {
    return fn<Camera>(kCamera)(tile,mode,ms>500?ms/4:(ms>125?125:ms),a,b,flag);
}
static void __fastcall update(Obj state,unsigned long long a,unsigned long long b,unsigned long long c) {
    const unsigned phase=dword(state,0x470);
    int *frame=nullptr;int saved=0;
    if(phase==3) {
        // Present the effect as finished once the last cue has played.
        if(Obj effect=pointer(state,0x578)) {
            int *f=reinterpret_cast<int *>(effect+0x74);
            if(*f>=static_cast<int>(kEffectShort) && *f<static_cast<int>(kEffectEnd)){frame=f;saved=*f;*f=kEffectEnd;}
            // No cue between 13 and 0x21: continue just before the last cue.
            else if(*f>=static_cast<int>(kGapStart) && *f<static_cast<int>(kGapEnd))*f=kGapEnd;
        }
    }
    else if(phase==6 || phase==7)*reinterpret_cast<int *>(state+0x5C0)+=3;   // counts four per frame
    fn<Update>(kUpdate)(state,a,b,c);
    if(frame && *frame==static_cast<int>(kEffectEnd))*frame=saved;   // effect object still alive in phase 4
}
static CodeLink camera_link;
static bool install() {
    if(!guards_match()) {log_event("occupy_fast_guard_rejected");return false;}
    Obj relay=allocate_near(image+kCameraCall);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&camera));
    DWORD old;
    if(!prepare_link(camera_link,kCameraCall,0xE8,relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,16)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    if(!publish_link(camera_link))return false;
    if(!patch_pointer(kUpdateSlot,kUpdate,reinterpret_cast<void *>(&update))) {restore_link(camera_link);return false;}
    log_event("occupy_fast_installed");return true;
}
}
