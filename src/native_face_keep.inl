// [performance] face_keep (2026-10-25, user request): army flag commander faces stay loaded.
// Evidence (REA carriers REA_Analysis/facepipe_20261025 + facekeep_20261025, unpacked 1fbe6be7...,
// measurements in data/diagnostics/turn_start_wait_20261025.md): the army flag panel (1B3450) sets the
// commander face at CALL 1B91E0(face node, person) 1B3523. 1B91E0 reads person+A6 (face) and sets five
// nodes through 9307B0 -> 930840, which takes resource id 8A83A0(manager 27D4390, kind 10, face) and
// adds a reference with 8A6DE0 (count 0 -> synchronous load 8AAD70: B393B0 job + while(!14D90)Sleep(0)).
// Releasing (8A7EA0) unloads at count 0 (8ACB40). After the 100 ms appear_batch this load was about half
// of the UI thread's CPU in the 0-day wait (ETL 457831bd).
// Mod: after the original 1B91E0, a face whose image is loaded (count > 0) gets one more reference via
// 8A6DE0, once per id, at most face_keep_core::kMax (256). User decision 2026-10-25 after another PC's
// ~10-turn crash report (cause unknown): retain 256, replace LRU pins and trim under RAM pressure.
// Frequently used commanders keep the no-reload benefit. A new manager/count array starts over.
// UI thread only (the original caller). Owns CALL 1B3523 (face_timing skips that site when on).
// [performance] face_prefetch (2026-10-25, user request; REA carrier REA_Analysis/faceasync_20261025):
// a new face costs one frame (face_set: 16.7 ms median) because 8AAD70 waits for its job at CALL 14D90
// 8AB11B (+ Sleep(0)/14D90 loop to 8AB13D). At the start of each frame of CAppearArmyState (appear_batch)
// the remaining list (+478: node{army,next}) is walked: army+12 = commander (person table 277DF18+158),
// face as above; a face not loaded (count 0) is requested through 8A6DE0 with that one wait answered
// "done" (job created, entry +178[id] = {.., +8 job, +10 texture}, count 1 = the pin) so many jobs run
// together. 8A8B80 only binds an image whose job result (+20) exists, so before 1B91E0 sets a requested
// face the skipped original wait `while(!14D90(job,0,*manager)) Sleep(0)` runs once (usually done).
#include "face_keep.h"
#ifndef NO_RUNTIME_LOG
#include <psapi.h>
#endif
namespace face_keep {
static const unsigned kSite=0x1B3523,kFace=0x1B91E0,kAdd=0x8A6DE0,kId=0x8A83A0,kRelease=0x8A7EA0,kManager=0x27D4390;
static const unsigned long long kFaceHash=0x48b0735f99258031ull,kAddHash=0x0aeca3e5d3d8f863ull,
    kIdHash=0x3f79d63ec71cc249ull,kReleaseHash=0xe9fe6c32e37f87d3ull;
static const int kFaceKind=10;
using Face=void(__fastcall *)(Obj,Obj);
static face_keep_core::Pins pins;   // UI thread only
static const unsigned kPumpSite=0x8AB11B,kPump=0x14D90,kLoad=0x8AAD70,kPersons=0x277DF18;
static const unsigned long long kLoadHash=0x9ddcb888371c8c54ull;   // 8AAD70..8AB216 with its chunks
static constexpr long long kPrefetchUs=6000;   // request time per frame (part of the appear budget)
using Pump=unsigned long long(__fastcall *)(Obj,unsigned long long,Obj);
static face_keep_core::Pending pending;      // UI thread only
static face_keep_core::Memory memory;
static Obj pin_manager;
static bool prefetching,pump_installed;
static Obj requested;
static LONGLONG prefetch_hz;
static unsigned long long __fastcall pump(Obj job,unsigned long long b,Obj system) {
    if(prefetching) {requested=job;return 1;}   // this load's wait is run later by set_face
    return fn<Pump>(kPump)(job,b,system);
}
static Obj entry_of(Obj manager,int id) {Obj t=pointer(manager,0x178);return t?pointer(t,static_cast<unsigned>(id)*8u):nullptr;}
static unsigned long long fingerprint(unsigned rva,unsigned size) {
    unsigned long long h=0xcbf29ce484222325ull;
    for(unsigned i=0;i<size;++i)h=(h^image[rva+i])*0x100000001b3ull;
    return h;
}
// 8A6DE0/8A7EA0: counts at manager+180 -> {+8 first id, +C id count, +10 int array}.
static const int *count_of(Obj manager,int id,const void **table) {
    *table=nullptr;if(!manager)return nullptr;
    Obj t=pointer(manager,0x180);if(!t)return nullptr;
    const int first=static_cast<int>(dword(t,8)),n=static_cast<int>(dword(t,0xC));
    int *counts=reinterpret_cast<int *>(pointer(t,0x10));*table=counts;
    if(!counts || id<first || id>=first+n)return nullptr;
    return counts+(id-first);
}
static void sync_table(Obj manager,const void *table) {
    if(manager!=pin_manager) {
        pin_manager=manager;pins.table=nullptr;pins.count=0;pins.sequence=0;
        pending.table=nullptr;pending.count=0;
    }
    // Old resources belong to the old table: drop bookkeeping only, never call its release.
    pins.reset(table);pending.reset(table);
}
static bool prepare_pin(Obj manager,const void *table,int id,int refs,bool prefetch) {
    sync_table(manager,table);
    return face_keep_core::prepare_pin(pins,table,id,refs,prefetch,memory,GetTickCount64(),
        [](unsigned &load,unsigned long long &available) {
            MEMORYSTATUSEX m{};m.dwLength=sizeof(m);
            if(!GlobalMemoryStatusEx(&m))return false;
            load=m.dwMemoryLoad;available=m.ullAvailPhys;return true;
        },
        [manager](int candidate) {
            Obj e=entry_of(manager,candidate);Obj job=e?pointer(e,8):nullptr;
            return pending.waiting(candidate,job)>=0;
        },
        [manager,table](int candidate) {
            const void *current=nullptr;const int *n=count_of(manager,candidate,&current);
            return current==table && n?*n:0;
        },
        [manager](int candidate) {
            // 930840 -> 872110 -> 872120 CALL 8721A2 confirms RCX=manager, EDX=id,
            // void return, same ABI as 8A6DE0. kReleaseHash guards the full function.
            fn<void(__fastcall *)(Obj,int)>(kRelease)(manager,candidate);
        },
        [](int candidate,unsigned reason,size_t count) {
            pending.forget(candidate);
#ifndef NO_RUNTIME_LOG
            log_event("face_keep_release",static_cast<unsigned>(candidate),reason,static_cast<unsigned>(count));
#else
            (void)reason;(void)count;
#endif
        });
}
#ifndef NO_RUNTIME_LOG
static void log_memory() {
    PROCESS_MEMORY_COUNTERS_EX m{};m.cb=sizeof(m);
    if(K32GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&m),sizeof(m)))
        log_event("face_keep_pins",static_cast<unsigned>(pins.count),static_cast<unsigned>(m.PrivateUsage>>20),
                  static_cast<unsigned>(m.WorkingSetSize>>20));
}
#endif
#ifndef NO_RUNTIME_LOG
// Diagnostics (2026-10-25, user: check the "one frame round trip" hypothesis): every call of the army
// flag face setter: face_set army=1 if the image was not loaded before (a load ran) value=microseconds
// hex=GetTickCount64 low 32 bits. Compare value with the 16.7 ms frame period.
static LONGLONG clock_hz;
#endif
static int face_id(Obj manager,Obj person) {
    if(!manager || !person || !fn<int(__fastcall *)(Obj)>(0x32F500)(person))return 0;   // as 1B91E0
    const int face=static_cast<int>(*reinterpret_cast<unsigned short *>(person+0xA6));
    return fn<int(__fastcall *)(Obj,int,int)>(kId)(manager,kFaceKind,face);   // table lookup only
}
// Called by appear_batch at the start of each frame of CAppearArmyState, before its armies.
static void prefetch(Obj state) { COST_SCOPE("face_prefetch");
    if(!mod_features.face_prefetch || !pump_installed || !state)return;
    Obj manager=pointer(image,kManager),persons=pointer(image,kPersons);
    if(!manager || !persons)return;
    if(!prefetch_hz) {LARGE_INTEGER hz;QueryPerformanceFrequency(&hz);prefetch_hz=hz.QuadPart;}
    LARGE_INTEGER a,b;QueryPerformanceCounter(&a);unsigned n=0;
    for(Obj node=pointer(state,0x478);node;node=pointer(node,8)) {
        QueryPerformanceCounter(&b);if((b.QuadPart-a.QuadPart)*1000000/prefetch_hz>=kPrefetchUs)break;
        Obj army=pointer(node,0);if(!army)continue;
        const unsigned who=*reinterpret_cast<unsigned short *>(army+0x12);
        Obj person=who<0x1771?pointer(persons,0x158+who*8):pointer(persons,0x73A50);   // as 25C070
        const int id=face_id(manager,person);
        const void *table=nullptr;const int *refs=count_of(manager,id,&table);
        if(!prepare_pin(manager,table,id,refs?*refs:-1,true))continue;
        prefetching=true;requested=nullptr;
        fn<void(__fastcall *)(Obj,int)>(kAdd)(manager,id);   // 8AAD70 without its wait; count 0 -> 1
        prefetching=false;
        face_keep_core::add_pin(pins,id);++n;
        Obj e=entry_of(manager,id);
        if(requested && e && pointer(e,8)==requested)pending.add(id,requested);
    }
    if(n) {QueryPerformanceCounter(&b);log_event("face_prefetch",n,static_cast<unsigned>((b.QuadPart-a.QuadPart)*1000000/prefetch_hz));}
}
// The original wait of a requested face, run once before the face is set (no empty image).
static void finish_request(Obj manager,int id) {
    const void *table=nullptr;count_of(manager,id,&table);sync_table(manager,table);
    if(!pending.count)return;
    Obj e=entry_of(manager,id);Obj job=e?pointer(e,8):nullptr;
    const long i=job?pending.waiting(id,job):-1;
    if(i<0)return;
    LARGE_INTEGER a,b;QueryPerformanceCounter(&a);
    Obj system=pointer(manager,0);
    while(!static_cast<unsigned char>(fn<Pump>(kPump)(job,0,system)))Sleep(0);
    pending.done[i]=true;
    QueryPerformanceCounter(&b);
    log_event("face_prefetch_wait",static_cast<unsigned>(id),static_cast<unsigned>((b.QuadPart-a.QuadPart)*1000000/(prefetch_hz?prefetch_hz:1)));
}
static void __fastcall set_face(Obj node,Obj person) { COST_SCOPE("face_1B91E0_1B3523");
    Obj manager=pointer(image,kManager);
    const int id=face_id(manager,person);
    const bool valid=id!=0;
    if(valid && mod_features.face_prefetch)finish_request(manager,id);
#ifndef NO_RUNTIME_LOG
    const void *seen=nullptr;const int *before=valid && diagnostic_logging?count_of(manager,id,&seen):nullptr;
    const bool loads=before && *before==0;
    LARGE_INTEGER a{},b{};if(diagnostic_logging)QueryPerformanceCounter(&a);
#endif
    fn<Face>(kFace)(node,person);
#ifndef NO_RUNTIME_LOG
    if(diagnostic_logging && valid) {
        QueryPerformanceCounter(&b);
        if(!clock_hz) {LARGE_INTEGER hz;QueryPerformanceFrequency(&hz);clock_hz=hz.QuadPart;}
        log_event("face_set",loads?1u:0u,static_cast<unsigned>((b.QuadPart-a.QuadPart)*1000000/clock_hz),
                  static_cast<unsigned>(GetTickCount64()));
    }
#endif
    if(!mod_features.face_keep || !valid)return;
    const void *table=nullptr;const int *refs=count_of(manager,id,&table);
    if(!prepare_pin(manager,table,id,refs?*refs:0,false))return;
    fn<void(__fastcall *)(Obj,int)>(kAdd)(manager,id);   // count > 0: reference only, no load
    face_keep_core::add_pin(pins,id);
#ifndef NO_RUNTIME_LOG
    if(pins.count%32==1)log_memory();
#endif
}
static CallLink link,pump_link;
static bool installed;
static bool install() {
    if(image[kSite]!=0xE8 || kSite+5+*reinterpret_cast<int *>(image+kSite+1)!=kFace || fingerprint(kFace,0x58)!=kFaceHash ||
       fingerprint(kAdd,0x5A)!=kAddHash || fingerprint(kId,0xE0)!=kIdHash || fingerprint(kRelease,0x4A)!=kReleaseHash) {
        log_event("face_keep_guard_rejected");return false;
    }
    Obj relay=allocate_near(image+kSite);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&set_face));
    DWORD old;
    if(!call_prepare(link,kSite,relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,16)) {VirtualFree(relay,0,MEM_RELEASE);log_event("face_keep_prepare_rejected");return false;}
    if(!call_publish(link)) {log_event("face_keep_publish_rejected");return false;}
    installed=true;log_event("face_keep_installed");
    if(mod_features.face_prefetch) {
        // 8AB11B: CALL 14D90; TEST AL,AL; JNE 8AB13D (skip the Sleep(0)/14D90 loop at 8AB124..8AB13B).
        static const unsigned char kWait[]={0x84,0xC0,0x75,0x19,0x33,0xC9,0xFF,0x15};
        if(image[kPumpSite]!=0xE8 || kPumpSite+5+*reinterpret_cast<int *>(image+kPumpSite+1)!=kPump ||
           memcmp(image+kPumpSite+5,kWait,sizeof(kWait)) || fingerprint(kLoad,0x8AB216-0x8AAD70)!=kLoadHash) {
            log_event("face_prefetch_guard_rejected");return true;
        }
        Obj stub=allocate_near(image+kPumpSite);if(!stub)return true;
        absolute_jump(stub,reinterpret_cast<void *>(&pump));
        if(!call_prepare(pump_link,kPumpSite,stub) || !VirtualProtect(stub,4096,PAGE_EXECUTE_READ,&old) ||
           !FlushInstructionCache(GetCurrentProcess(),stub,16)) {VirtualFree(stub,0,MEM_RELEASE);log_event("face_prefetch_prepare_rejected");return true;}
        if(!call_publish(pump_link)) {log_event("face_prefetch_publish_rejected");return true;}
        pump_installed=true;log_event("face_prefetch_installed");
    }
    return true;
}
}
