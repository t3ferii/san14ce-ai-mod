// User/Claude 2026-10-29/30: 격류 foam follows terrain edits by moving the 35
// startup KIDS foam effects (hash 05641EE9). User 2026-10-30: no reserve pool; the
// list parser/early/enumeration hooks were removed (scenario-start lag, never matched).
// Release feature under [features] terrain_edit; logging stays diagnostic-only.
#pragma once
#ifndef SPLIT_COMBAT
#define SAN14_TERRAIN_FOAM_INCLUDED 1
#include <cmath>
namespace terrain_foam {
static constexpr unsigned HexCount=48400,SourceCount=35,Count=SourceCount;
// Hidden scale stays positive: E1B160 divides by the previous scale.
static constexpr float S_HIDE=0.001f;
struct Source {unsigned id,owner;float pos[3],quat[4],scale[3];};
static constexpr Source sources[SourceCount]={
    {0x00EC810Eu,31194u,{1.59322687500000000e+05f,5.00000000000000000e+02f,1.28075593750000000e+05f},{0.00000000000000000e+00f,-8.53201091289520264e-01f,0.00000000000000000e+00f,-5.21582067012786865e-01f},{1.20000004768371582e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0x042A7950u,29854u,{1.45495828125000000e+05f,5.00000000000000000e+02f,1.23142960937500000e+05f},{0.00000000000000000e+00f,-7.67916798591613770e-01f,0.00000000000000000e+00f,-6.40549480915069580e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,1.50000000000000000e+00f}},
    {0x07687192u,31166u,{1.39994484375000000e+05f,4.99999969482421875e+02f,1.28347554687500000e+05f},{-1.36286601559731935e-07f,-9.02585268020629883e-01f,6.50053593176380673e-08f,-4.30511057376861572e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,1.60000002384185791e+00f}},
    {0x0A1F048Bu,30077u,{1.47593468750000000e+05f,5.00000000000000000e+02f,1.23338812500000000e+05f},{0.00000000000000000e+00f,-5.92661976814270020e-01f,0.00000000000000000e+00f,-8.05450856685638428e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0x0AA669D4u,30977u,{1.61639296875000000e+05f,5.00000000000000000e+02f,1.27137382812500000e+05f},{0.00000000000000000e+00f,-7.59421467781066895e-01f,0.00000000000000000e+00f,-6.50598466396331787e-01f},{1.00000000000000000e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0x0D5CFCCDu,30952u,{1.44403734375000000e+05f,4.99999298095703125e+02f,1.27135109375000000e+05f},{-6.05133365638721443e-08f,-6.96749031543731689e-01f,6.30917966759625415e-08f,-7.17315077781677246e-01f},{1.50000000000000000e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0x109AF50Fu,31179u,{1.49078640625000000e+05f,5.00000000000000000e+02f,1.27334914062500000e+05f},{0.00000000000000000e+00f,-6.03099584579467773e-01f,0.00000000000000000e+00f,-7.97665596008300781e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0x13D8ED51u,25487u,{1.68857390625000000e+05f,5.00000000000000000e+02f,1.06888718750000000e+05f},{0.00000000000000000e+00f,-8.36571812629699707e-01f,0.00000000000000000e+00f,-5.47857463359832764e-01f},{1.50000000000000000e+00f,2.00000000000000000e+00f,2.50000000000000000e+00f}},
    {0x1716E593u,29438u,{1.62424343750000000e+05f,5.00000000000000000e+02f,1.21911726562500000e+05f},{0.00000000000000000e+00f,-9.47526335716247559e-01f,0.00000000000000000e+00f,-3.19676905870437622e-01f},{1.00000000000000000e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0x6CE87DF3u,33792u,{1.30232828125000000e+05f,5.00001495361328125e+02f,1.37671218750000000e+05f},{-8.31371878007303167e-08f,9.01461482048034668e-01f,2.70304703064994101e-08f,4.32859420776367188e-01f},{1.39999997615814209e+00f,1.39999997615814209e+00f,2.00000000000000000e+00f}},
    {0x6F9F10ECu,32032u,{1.29983914062500000e+05f,5.00001495361328125e+02f,1.31133687500000000e+05f},{-7.49258362020555069e-08f,-8.60304832458496094e-01f,4.50394921358565625e-08f,-5.09779870510101318e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,1.60000002384185791e+00f}},
    {0x70267635u,34225u,{1.25456460937500000e+05f,5.00000427246093750e+02f,1.39322406250000000e+05f},{-1.99188804117511609e-07f,-7.98635303974151611e-01f,4.28039186317619169e-07f,-6.01814985275268555e-01f},{1.50000000000000000e+00f,1.50000000000000000e+00f,1.50000000000000000e+00f}},
    {0x72DD092Eu,31378u,{1.34418250000000000e+05f,5.00000854492187500e+02f,1.29209382812500000e+05f},{-6.39243182831705781e-08f,-7.35516130924224854e-01f,5.96332299096502538e-08f,-6.77507281303405762e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,1.60000002384185791e+00f}},
    {0x761B0170u,30747u,{1.54715250000000000e+05f,5.00000000000000000e+02f,1.25642375000000000e+05f},{0.00000000000000000e+00f,-6.54029965400695801e-01f,0.00000000000000000e+00f,-7.56468057632446289e-01f},{1.00000000000000000e+00f,1.00000000000000000e+00f,2.00000000000000000e+00f}},
    {0x7958F9B2u,31169u,{1.42062156250000000e+05f,4.99999755859375000e+02f,1.27561890625000000e+05f},{-6.44422755158302607e-08f,-7.41400420665740967e-01f,5.90731232819052821e-08f,-6.71063005924224854e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0x7C0F8CABu,26582u,{1.65621937500000000e+05f,5.00000000000000000e+02f,1.11359078125000000e+05f},{0.00000000000000000e+00f,-9.98968958854675293e-01f,0.00000000000000000e+00f,-4.53913658857345581e-02f},{1.50000000000000000e+00f,1.50000000000000000e+00f,2.00000000000000000e+00f}},
    {0x7C96F1F4u,30095u,{1.60287734375000000e+05f,5.00000000000000000e+02f,1.23295960937500000e+05f},{8.41229486070460553e-09f,8.37241768836975098e-01f,9.50837097946077847e-09f,5.46832859516143799e-01f},{1.00000000000000000e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0x7F4D84EDu,28562u,{1.65312796875000000e+05f,5.00000000000000000e+02f,1.18497992187500000e+05f},{0.00000000000000000e+00f,-9.57265615463256836e-01f,0.00000000000000000e+00f,-2.89210498332977295e-01f},{1.50000000000000000e+00f,1.00000000000000000e+00f,1.50000000000000000e+00f}},
    {0x828B7D2Fu,25924u,{1.66669437500000000e+05f,5.00000000000000000e+02f,1.08488164062500000e+05f},{0.00000000000000000e+00f,-9.43208873271942139e-01f,0.00000000000000000e+00f,-3.32200646400451660e-01f},{1.50000000000000000e+00f,2.00000000000000000e+00f,2.50000000000000000e+00f}},
    {0x85C97571u,31847u,{1.55100187500000000e+05f,5.00000000000000000e+02f,1.29775742187500000e+05f},{0.00000000000000000e+00f,-7.42557883262634277e-01f,0.00000000000000000e+00f,-6.69782042503356934e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,1.50000000000000000e+00f}},
    {0x8880086Au,30760u,{1.63966234375000000e+05f,5.00000000000000000e+02f,1.26125492187500000e+05f},{0.00000000000000000e+00f,-7.94626057147979736e-01f,0.00000000000000000e+00f,-6.07098579406738281e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0x89076DB3u,30080u,{1.49873015625000000e+05f,5.00000000000000000e+02f,1.23927578125000000e+05f},{0.00000000000000000e+00f,-5.92661976814270020e-01f,0.00000000000000000e+00f,-8.05450856685638428e-01f},{1.00000000000000000e+00f,1.00000000000000000e+00f,2.00000000000000000e+00f}},
    {0x8BBE00ACu,27242u,{1.65571593750000000e+05f,5.00000000000000000e+02f,1.13781445312500000e+05f},{4.25521804459094710e-08f,9.99657332897186279e-01f,4.25521804459094710e-08f,2.61769499629735947e-02f},{1.29999995231628418e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0x8EFBF8EEu,30304u,{1.52319453125000000e+05f,5.00000000000000000e+02f,1.24689953125000000e+05f},{0.00000000000000000e+00f,-5.92661976814270020e-01f,0.00000000000000000e+00f,-8.05450856685638428e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0x9239F130u,31631u,{1.57334421875000000e+05f,5.00000000000000000e+02f,1.29404664062500000e+05f},{0.00000000000000000e+00f,-7.26113557815551758e-01f,0.00000000000000000e+00f,-6.87574744224548340e-01f},{1.20000004768371582e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0x9577E972u,28123u,{1.66122718750000000e+05f,5.00000000000000000e+02f,1.16220375000000000e+05f},{0.00000000000000000e+00f,-9.99964714050292969e-01f,0.00000000000000000e+00f,8.41006450355052948e-03f},{1.29999995231628418e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0xEB4981D2u,29440u,{1.63942781250000000e+05f,5.00000000000000000e+02f,1.21604664062500000e+05f},{0.00000000000000000e+00f,-9.49733078479766846e-01f,0.00000000000000000e+00f,-3.13059449195861816e-01f},{1.00000000000000000e+00f,1.00000000000000000e+00f,1.50000000000000000e+00f}},
    {0xEE877A14u,33355u,{1.32176000000000000e+05f,5.00000000000000000e+02f,1.35815000000000000e+05f},{-1.97915099420242768e-07f,9.39692735671997070e-01f,1.07669059445925086e-07f,3.42020153999328613e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0xF13E0D0Du,32470u,{1.28446039062500000e+05f,5.00001708984375000e+02f,1.32889906250000000e+05f},{-8.28981612244206190e-08f,-9.50248718261718750e-01f,2.77549538907351234e-08f,-3.11491996049880981e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0xF1C57256u,34229u,{1.27816648437500000e+05f,5.00001220703125000e+02f,1.38919140625000000e+05f},{-8.25605894760883530e-08f,7.66044437885284424e-01f,-5.78095473713347019e-08f,6.42787694931030273e-01f},{1.39999997615814209e+00f,1.39999997615814209e+00f,1.39999997615814209e+00f}},
    {0xF47C054Fu,31594u,{1.32017015625000000e+05f,5.00001129150390625e+02f,1.29707578125000000e+05f},{-7.37211891532751906e-08f,-8.46665561199188232e-01f,4.69853844720091729e-08f,-5.32125234603881836e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,1.79999995231628418e+00f}},
    {0xF7B9FD91u,30956u,{1.46700468750000000e+05f,4.99999053955078125e+02f,1.27201960937500000e+05f},{-6.05133365638721443e-08f,-6.96749031543731689e-01f,6.30917966759625415e-08f,-7.17315077781677246e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,1.79999995231628418e+00f}},
    {0xFAF7F5D3u,30323u,{1.65917328125000000e+05f,5.00000000000000000e+02f,1.24751898437500000e+05f},{0.00000000000000000e+00f,-9.27965760231018066e-01f,0.00000000000000000e+00f,-3.72664242982864380e-01f},{1.29999995231628418e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0xFDAE88CCu,29220u,{1.63970921875000000e+05f,5.00000000000000000e+02f,1.20384796875000000e+05f},{0.00000000000000000e+00f,-9.71367776393890381e-01f,0.00000000000000000e+00f,-2.37579181790351868e-01f},{1.00000000000000000e+00f,1.29999995231628418e+00f,2.00000000000000000e+00f}},
    {0xFE35EE15u,25270u,{1.70677984375000000e+05f,5.00000000000000000e+02f,1.06162265625000000e+05f},{0.00000000000000000e+00f,-6.89289927482604980e-01f,0.00000000000000000e+00f,-7.24485635757446289e-01f},{2.00000000000000000e+00f,2.00000000000000000e+00f,1.50000000000000000e+00f}},
};
static constexpr unsigned BodyFingerprint=0xDF764CC0u; // TERRAIN_FOAM_BODY_FINGERPRINT
struct Hook {unsigned slot,rva;unsigned char bytes[16];void *wrapper;bool published;};
using Create=bool(__fastcall *)(Obj,Obj,Obj,Obj,Obj);
using Complete=bool(__fastcall *)(Obj,Obj,Obj,Obj,unsigned *,unsigned char *);
using Destroy=uint64_t(__fastcall *)(Obj,Obj,Obj,Obj);
// Opaque integer register/stack slots: preserve all 8 input slots and full RAX.
// E1B7B0 reads its time float from the low 32 bits of entry RSP+40 (slot8).
using Callback=uint64_t(__fastcall *)(Obj,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
using Setter=void(__fastcall *)(Obj,const float *);
static bool __fastcall create(Obj,Obj,Obj,Obj,Obj);
static bool __fastcall complete(Obj,Obj,Obj,Obj,unsigned *,unsigned char *);
static uint64_t __fastcall destroy(Obj,Obj,Obj,Obj);
static uint64_t __fastcall time_update(Obj,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
static uint64_t __fastcall render(Obj,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t,uintptr_t);
static Hook hooks[]={
    {0x16DA5C8,0xE17CA0,{0x40,0x55,0x53,0x56,0x41,0x55,0x41,0x57,0x48,0x8D,0xAC,0x24,0x50,0xFE,0xFF,0xFF},reinterpret_cast<void *>(&create),false},
    {0x16DA5E8,0xE1B580,{0x48,0x89,0x5C,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41,0x57,0x48,0x8D,0x6C,0x24},reinterpret_cast<void *>(&complete),false},
    {0x16DA5D0,0xE18280,{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x6C,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57},reinterpret_cast<void *>(&destroy),false},
    {0x16DA1A0,0xE1B7B0,{0xF3,0x0F,0x10,0x44,0x24,0x40,0xF3,0x0F,0x59,0x05,0x3A,0x31,0x59,0x00,0x4C,0x89},reinterpret_cast<void *>(&time_update),false},
    {0x16DA170,0xE19690,{0x48,0x89,0x5C,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57},reinterpret_cast<void *>(&render),false}
};
static constexpr unsigned SetterRvas[]={0xE1AAE0,0xE1ADD0,0xE1B160};
static constexpr unsigned char SetterBytes[][16]={
    {0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x48,0x89,0x78,0x18,0x4C},
    {0x48,0x8B,0xC4,0x48,0x89,0x58,0x10,0x48,0x89,0x78,0x18,0x55,0x48,0x8D,0x68,0x98},
    {0x48,0x8B,0xC4,0x48,0x89,0x78,0x18,0x55,0x48,0x8D,0xA8,0x48,0xFF,0xFF,0xFF,0x48}
};
static bool permitted=true,installed=false,transition=false;
static SRWLOCK lock=SRWLOCK_INIT;
static volatile LONG watch=0;
static bool any_time=false,ui_logged=false;
static unsigned generation=1,operation=0;
struct Effect {
    Obj object,handle;
    float pos[3],quat[4],scale[3]; // Captured original transform, never recomputed from a hidden object.
    unsigned hex;
    bool ready,used,hidden,job,time_seen,render_seen,restore_job;
};
static Effect effects[Count]={};
struct Goal {unsigned char next,original;bool known,queued,waiting;};
static Goal goals[HexCount]={};
static unsigned short dirty[HexCount];
static unsigned head=0,dirty_count=0;
static bool active() {return installed && permitted && enabled && mod_features.convenience && mod_features.terrain_edit;}
static bool readable(Obj p,size_t n) {
    uintptr_t at=reinterpret_cast<uintptr_t>(p),end=at+n;
    if(!at || end<at)return false;
    while(at<end) {
        MEMORY_BASIC_INFORMATION m={};
        if(!VirtualQuery(reinterpret_cast<void *>(at),&m,sizeof(m)) || m.State!=MEM_COMMIT ||
            (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
            !(m.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
        uintptr_t next=reinterpret_cast<uintptr_t>(m.BaseAddress)+m.RegionSize;
        if(next<=at)return false;at=next;
    }
    return true;
}
static bool guards_match() {
    for(const auto &h:hooks)if(pointer(image,h.slot)!=(h.published?reinterpret_cast<Obj>(h.wrapper):image+h.rva) ||
        memcmp(image+h.rva,h.bytes,16))return false;
    for(unsigned i=0;i<3;++i)if(memcmp(image+SetterRvas[i],SetterBytes[i],16))return false;
    return true;
}
static void update_watch_locked() {
    LONG bits=dirty_count?4:0;
    for(const auto &e:effects)if(e.object) {
        if(!e.time_seen)bits|=1;
        if(!e.render_seen)bits|=2;
        if(e.job)bits|=4;
    }
    InterlockedExchange(&watch,bits);
}
static void enqueue_locked(unsigned hex) {
    if(!goals[hex].queued) {dirty[(head+dirty_count)%HexCount]=static_cast<unsigned short>(hex);++dirty_count;goals[hex].queued=true;}
    InterlockedOr(&watch,4);
}
// UI/load host: numbers only. No dereference, lookup, or native call on effects.
static void mark(unsigned hex,unsigned next,unsigned original) {
    if(hex>=HexCount || next>255 || original>255 || !active())return;
    AcquireSRWLockExclusive(&lock);
    if(!ui_logged) {ui_logged=true;log_event("foam_ui_thread",GetCurrentThreadId(),generation,0);}
    if(!(transition && next==original)) {
        transition=false;
        goals[hex].known=true;goals[hex].next=static_cast<unsigned char>(next);goals[hex].original=static_cast<unsigned char>(original);
        enqueue_locked(hex);
        log_event("foam_mark",hex,next,original);
    }
    ReleaseSRWLockExclusive(&lock);
}
static void before_world_change() {
    AcquireSRWLockExclusive(&lock);
    // User/Claude 2026-10-29: effects survive save loads. Only E18280 removes
    // identities. Reset numeric goals now; restore transforms on their native
    // callbacks before replaying the new save's goals. No effect dereference here.
    ++generation;transition=true;ui_logged=false;
    memset(goals,0,sizeof(goals));head=dirty_count=0;
    unsigned retained=0,restores=0;
    for(unsigned i=0;i<Count;++i) {
        Effect &e=effects[i];if(!e.object)continue;
        ++retained;
        e.restore_job=e.restore_job || e.job || e.used || e.hidden || e.hex!=sources[i].owner;
        e.used=false;e.hidden=false;e.hex=sources[i].owner;e.job=e.restore_job;
        if(e.restore_job)++restores;
    }
    update_watch_locked();
    log_event("foam_world_keep",retained,restores,generation);
    ReleaseSRWLockExclusive(&lock);
}
static int find_locked(Obj p) {
#ifdef TERRAIN_FOAM_FIXTURE_LOOKUP
    TERRAIN_FOAM_FIXTURE_LOOKUP();
#endif
    for(unsigned i=0;i<Count;++i)if(effects[i].object==p && p)return static_cast<int>(i);
    return -1;
}
static bool positive(const float *v) {return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]) && v[0]>0 && v[1]>0 && v[2]>0;}
static void capture(Obj handle) {
    Obj p=handle?pointer(handle,0x20):nullptr;
    if(!readable(p,0xF0) || pointer(p,0)!=image+0x16DA140)return;
    float pos[3];memcpy(pos,p+0x18,sizeof(pos));
    for(unsigned i=0;i<Count;++i) {
        double d=0;for(unsigned k=0;k<3;++k){double delta=pos[k]-sources[i].pos[k];d+=delta*delta;}
        if(d>1.0 || !std::isfinite(d))continue;
        AcquireSRWLockExclusive(&lock);
        log_event("foam_create",i,GetCurrentThreadId(),generation);
        Effect &e=effects[i];
        if(!e.object) {
            e.object=p;e.handle=handle;e.hex=sources[i].owner;
            memcpy(e.pos,p+0x18,12);memcpy(e.quat,p+0x24,16);memcpy(e.scale,p+0x34,12);
            update_watch_locked();
        }
        ReleaseSRWLockExclusive(&lock);return;
    }
}
static bool __fastcall create(Obj a,Obj b,Obj c,Obj d,Obj e) {
    bool result=fn<Create>(0xE17CA0)(a,b,c,d,e);
    if(result && active())capture(d);
    return result;
}
static bool __fastcall complete(Obj a,Obj b,Obj c,Obj handle,unsigned *state,unsigned char *flag) {
    bool result=fn<Complete>(0xE1B580)(a,b,c,handle,state,flag);
    if(!active())return result;
    AcquireSRWLockExclusive(&lock);
    int index=find_locked(handle?pointer(handle,0x20):nullptr);
    if(index>=0) {
        Effect &e=effects[index];Obj runtime=pointer(e.object,0xA0);
        log_event("foam_ready",static_cast<unsigned>(index),result?1:0,state?*state:~0u);
        if(!e.ready && result && state && *state==2 && runtime && positive(e.scale)) {
            e.ready=true;
            // Retry only outstanding numeric goals when a donor becomes ready.
            for(unsigned h=0;h<HexCount;++h)if(goals[h].waiting){goals[h].waiting=false;enqueue_locked(h);}
        }
        update_watch_locked();
    }
    ReleaseSRWLockExclusive(&lock);return result;
}
static uint64_t __fastcall destroy(Obj a,Obj b,Obj c,Obj handle) {
    AcquireSRWLockExclusive(&lock);
    int index=find_locked(handle?pointer(handle,0x20):nullptr);
    if(index>=0) {log_event("foam_destroy",static_cast<unsigned>(index),generation,GetCurrentThreadId());effects[index]={};update_watch_locked();}
    ReleaseSRWLockExclusive(&lock);
    return fn<Destroy>(0xE18280)(a,b,c,handle);
}
static void plan_locked() {
    while(dirty_count) {
        unsigned h=dirty[head];head=(head+1)%HexCount;--dirty_count;
        Goal &g=goals[h];g.queued=false;g.waiting=false;
        bool found=false;
        for(auto &e:effects)if(e.object && e.hex==h) {found=true;e.hidden=g.next!=24;e.job=true;}
        if(g.next!=24 || found)continue;
        // User 2026-10-30: 35 startup effects only. Borrow the last ready, unused one
        // whose original tile is untouched or still 격류.
        int donor=-1;
        for(unsigned i=0;i<Count;++i) {
            const Effect &e=effects[i];unsigned owner=sources[i].owner;
            if(e.object && e.ready && !e.used && !e.hidden && e.hex==owner && (!goals[owner].known || goals[owner].next==24))donor=static_cast<int>(i);
        }
        if(donor<0) {g.waiting=true;log_event("foam_no_donor",h,generation,0);continue;}
        Effect &e=effects[donor];e.used=true;e.hex=h;e.hidden=false;e.job=true;
        log_event("foam_move_plan",static_cast<unsigned>(donor),sources[donor].owner,h);
    }
}
static void apply_locked(unsigned index,bool from_time) {
    Effect &e=effects[index];
    if(!e.job || !e.ready || !pointer(e.object,0xA0))return;
    if(!positive(reinterpret_cast<const float *>(e.object+0x34))) {e.job=e.restore_job=false;log_event("foam_bad_scale",index,generation,0);return;}
    alignas(16) float pos[4]={e.pos[0],e.pos[1],e.pos[2],1.0f};
    alignas(16) float quat[4]={e.quat[0],e.quat[1],e.quat[2],e.quat[3]};
    alignas(16) float scale[4]={e.scale[0],e.scale[1],e.scale[2],1.0f};
    if(e.hex<HexCount && e.hex!=sources[index].owner) {
        unsigned row=e.hex/220,col=e.hex%220;constexpr double h=810.5997779422346;
        pos[0]=static_cast<float>(37594.0+col*702.0);
        pos[2]=static_cast<float>(13335.3+row*h+(col%2==0?h/2:0));
    }
    if(e.hidden)scale[0]=scale[1]=scale[2]=S_HIDE;
    unsigned op=++operation;(void)op;(void)from_time;
    log_event(from_time?"foam_consume_time":"foam_consume_render",op,index,GetCurrentThreadId());
    log_event("foam_set_target",op,e.hex,e.hidden?1:0);
    // Each changed property goes directly from current to target once; no zero/reset round trip.
    if(memcmp(e.object+0x18,pos,12))fn<Setter>(0xE1AAE0)(e.object,pos);
    if(memcmp(e.object+0x24,quat,16))fn<Setter>(0xE1ADD0)(e.object,quat);
    if(memcmp(e.object+0x34,scale,12))fn<Setter>(0xE1B160)(e.object,scale);
    if(e.restore_job)log_event("foam_restore_done",op,index,generation);
    e.job=e.restore_job=false;
}
static bool restoration_pending_locked() {
    for(const auto &e:effects)if(e.object && e.restore_job)return true;
    return false;
}
static void observe(Obj object,bool from_time) {
    if(!active())return;
    AcquireSRWLockExclusive(&lock);
    int index=find_locked(object);
    if(index>=0) {
        Effect &e=effects[index];bool &seen=from_time?e.time_seen:e.render_seen;
        if(!seen) {seen=true;log_event(from_time?"foam_time_thread":"foam_render_thread",static_cast<unsigned>(index),GetCurrentThreadId(),generation);}
        if(from_time)any_time=true;
        bool consume=from_time || !any_time;
        // Restoration must also run while old rollback marks are suppressed.
        if(e.restore_job && consume)apply_locked(static_cast<unsigned>(index),from_time);
        if(!transition && !restoration_pending_locked()) {
            plan_locked();
            if(consume)apply_locked(static_cast<unsigned>(index),from_time);
        }
        update_watch_locked();
    }
    ReleaseSRWLockExclusive(&lock);
}
static uint64_t __fastcall time_update(Obj a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e,uintptr_t f,uintptr_t g,uintptr_t h) {
    if(watch)observe(a,true);
    return fn<Callback>(0xE1B7B0)(a,b,c,d,e,f,g,h);
}
static uint64_t __fastcall render(Obj a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e,uintptr_t f,uintptr_t g,uintptr_t h) {
    if(watch)observe(a,false);
    return fn<Callback>(0xE19690)(a,b,c,d,e,f,g,h);
}
static void restore() {
    installed=false;before_world_change();
    for(auto &h:hooks)if(h.published) {
        auto slot=reinterpret_cast<void **>(image+h.slot);DWORD old=0,ignored=0;
        if(VirtualProtect(slot,8,PAGE_READWRITE,&old)) {
            InterlockedCompareExchangePointer(slot,image+h.rva,h.wrapper);
            VirtualProtect(slot,8,old,&ignored);h.published=false;
        }
    }
}
static void install() {
    if(installed || !permitted || !enabled || !mod_features.convenience || !mod_features.terrain_edit)return;
    if(!guards_match()) {permitted=false;log_event("foam_guard_mismatch",31,0,0);return;}
    for(auto &h:hooks) {
        auto slot=reinterpret_cast<void **>(image+h.slot);DWORD old=0,ignored=0;
        bool ok=VirtualProtect(slot,8,PAGE_READWRITE,&old)!=0;
        if(ok) {
            h.published=InterlockedCompareExchangePointer(slot,h.wrapper,image+h.rva)==image+h.rva;
            ok=h.published;VirtualProtect(slot,8,old,&ignored);
        }
        if(!ok) {restore();permitted=false;log_event("foam_hook_failed",h.slot,0,0);return;}
    }
    installed=true;log_event("foam_installed",Count,GetCurrentThreadId(),generation);
}
}
static bool terrain_foam_guards_match() {return terrain_foam::guards_match();}
#endif
