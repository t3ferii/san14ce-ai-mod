// Only deterministic, world-independent original geometry is accelerated.
// No game object, RNG, native order or worker scheduling is touched.
#include "progress_geometry.h"
#include "native_progress_profile.h"
#include "progress_distance_kernel.h"
using ProgressDistance=int(__fastcall *)(int,int);
using ProgressAi=void(__fastcall *)(Obj);
static ProgressDistance original_progress_distance;
static ProgressAi original_progress_ai;
static progress_geometry::Grid progress_grid;
static volatile LONG progress_native_active;
static bool progress_native_profile=false;
static bool progress_native_geometry=true;
static LARGE_INTEGER progress_clock_frequency;
using ProgressPhase=void(__fastcall *)(Obj,Obj);
struct ProgressSample {LONGLONG ticks[3];unsigned calls[3];};
static thread_local ProgressSample *progress_sample;
static unsigned progress_microseconds(LONGLONG ticks) {
    double elapsed=static_cast<double>(ticks)*1000000.0/static_cast<double>(progress_clock_frequency.QuadPart);
    return elapsed>4294967295.0?UINT32_MAX:static_cast<unsigned>(elapsed);
}
static void progress_phase(unsigned phase,unsigned rva,Obj manager,Obj subject) {
    ProgressSample *sample=progress_sample;
    if(!progress_native_active || !progress_native_profile || !sample) {fn<ProgressPhase>(rva)(manager,subject);return;}
    LARGE_INTEGER begin,end;QueryPerformanceCounter(&begin);
    fn<ProgressPhase>(rva)(manager,subject);
    QueryPerformanceCounter(&end);sample->ticks[phase]+=end.QuadPart-begin.QuadPart;++sample->calls[phase];
}
static void __fastcall progress_force(Obj manager,Obj subject) {COST_SCOPE("native_force_ai");progress_phase(0,0xD3D50,manager,subject);}
static void __fastcall progress_district(Obj manager,Obj subject) {COST_SCOPE("native_district_ai");progress_phase(1,0xD3CE0,manager,subject);}
static void __fastcall progress_army(Obj manager,Obj subject) {COST_SCOPE("native_army_ai");progress_phase(2,0xD3C70,manager,subject);}
static void progress_cost_original_ai(Obj context) {COST_SCOPE("native_ai_root");original_progress_ai(context);}
static void __fastcall progress_native_ai(Obj context) {
    if(!progress_native_active || !progress_native_profile) {original_progress_ai(context);return;}
    ProgressSample sample={};ProgressSample *previous=progress_sample;progress_sample=&sample;
    LARGE_INTEGER begin,end;QueryPerformanceCounter(&begin);
    __try {progress_cost_original_ai(context);}
    __finally {progress_sample=previous;}
    QueryPerformanceCounter(&end);
    PATH_COUNT_FLUSH();
    // Scalar lines only after the root call. No per-subject IO in the timed AI.
    log_event("progress_original_ai_microseconds",0,progress_microseconds(end.QuadPart-begin.QuadPart),GetCurrentThreadId());
    const char *names[]={"progress_force_microseconds","progress_district_microseconds","progress_army_microseconds"};
    for(unsigned phase=0;phase<3;++phase)log_event(names[phase],sample.calls[phase],progress_microseconds(sample.ticks[phase]),GetCurrentThreadId());
}
struct ProgressNativeCode {CodeLink distance,ai,phases[3];};
static bool progress_native_guards_match() {
    return !memcmp(image+0x2AA850,progress_distance_bytes,sizeof(progress_distance_bytes)) &&
           !memcmp(image+0x43E030,progress_ai_entry_bytes,sizeof(progress_ai_entry_bytes)) &&
           !memcmp(image+0x43E0A0,progress_force_call_bytes,16) &&
           !memcmp(image+0x43E120,progress_district_call_bytes,16) &&
           !memcmp(image+0x43E4C0,progress_army_call_bytes,16);
}
static bool prepare_progress_native(ProgressNativeCode &code,Obj relay) {
    if(!progress_native_guards_match())return false;
    // Distance: MOV R10D,EDX (3) + MOV EAX,imm32 (5).
    // AI: MOV RAX,RSP (3) + PUSH RBP (1) + PUSH R12 (2).
    // Complete instructions, no relative operands; FF25 jumps preserve GPRs.
    static_assert(sizeof(progress_kernel_bytes)<=128,"Distance relay/gate overlap");
    memcpy(relay,progress_kernel_bytes,sizeof(progress_kernel_bytes));
    memcpy(relay+128,progress_distance_bytes,8);absolute_jump(relay+136,image+0x2AA858);
    original_progress_distance=reinterpret_cast<ProgressDistance>(relay+128);
    Obj coordinates=reinterpret_cast<Obj>(progress_grid.coordinates);
    Obj original=relay+128;
    memcpy(relay+progress_kernel_coordinates_offset,&coordinates,sizeof(coordinates));
    memcpy(relay+progress_kernel_original_offset,&original,sizeof(original));
    absolute_jump(relay+192,reinterpret_cast<void *>(&progress_native_ai));
    memcpy(relay+224,progress_ai_entry_bytes,6);
    // A memory JMP FF25 is recognized as an epilog by the x64 unwinder. These
    // PUSHs are not undone here, so use a volatile-register branch instead.
    relay[230]=0x49;relay[231]=0xBB;
    Obj continuation=image+0x43E036;memcpy(relay+232,&continuation,8);
    relay[240]=0x41;relay[241]=0xFF;relay[242]=0xE3;
    original_progress_ai=reinterpret_cast<ProgressAi>(relay+224);
    if(!prepare_link(code.distance,0x2AA850,0xE9,relay) || !prepare_link(code.ai,0x43E030,0xE9,relay+192))return false;
    memset(code.distance.modified+5,0x90,3);code.ai.modified[5]=0x90;
    // The original gate executes two nonvolatile PUSHs. Register their unwind
    // information so exceptions/profiling inside this small prolog are valid.
    auto runtime=reinterpret_cast<RUNTIME_FUNCTION *>(relay+256);
    runtime->BeginAddress=224;runtime->EndAddress=243;runtime->UnwindData=272;
    const unsigned char unwind[]={1,6,2,0,6,0xC0,4,0x50};
    memcpy(relay+272,unwind,sizeof(unwind));
    const unsigned calls[]={0x43E0A5,0x43E122,0x43E4C8};
    const ProgressPhase wrappers[]={progress_force,progress_district,progress_army};
    for(unsigned phase=0;phase<3;++phase) {
        Obj stub=relay+288+phase*32;absolute_jump(stub,reinterpret_cast<void *>(wrappers[phase]));
        if(!prepare_link(code.phases[phase],calls[phase],0xE8,stub))return false;
    }
    return true;
}
static void restore_progress_native(ProgressNativeCode &code) {
    InterlockedExchange(&progress_native_active,0);
    for(unsigned phase=3;phase>0;--phase)restore_link(code.phases[phase-1]);
    restore_link(code.ai);restore_link(code.distance);
}
static bool publish_progress_native(ProgressNativeCode &code) {
    if(progress_native_geometry && !publish_link(code.distance)) {restore_progress_native(code);return false;}
    if(progress_native_profile) {
        if(!publish_link(code.ai)) {restore_progress_native(code);return false;}
        for(unsigned phase=0;phase<3;++phase)if(!publish_link(code.phases[phase])) {restore_progress_native(code);return false;}
    }
    InterlockedExchange(&progress_native_active,1);return true;
}
static bool install_progress_native() {
    // 기능설정.ini first ([progress_native], [diagnostics] reuse_handle), then progress_native.ini.
    if(!mod_setting(L"progress_native",L"enabled",1,L"progress_native.ini")) {log_event("progress_native_disabled");return true;}
#ifndef NO_RUNTIME_LOG
    InterlockedExchange(&reuse_log_handle,mod_setting(L"diagnostics",L"reuse_handle",
        mod_setting(L"progress_native",L"reuse_log_handle",1,L"progress_native.ini"))!=0?1:0);
#endif
    log_event("progress_log_handle_reuse",0,reuse_log_handle?1:0);
    progress_native_profile=diagnostic_logging && dev_group() && mod_setting(L"progress_native",L"profile",0,L"progress_native.ini")!=0;
#ifdef RUNTIME_COST_PROFILE
    if(costprofile::on)progress_native_profile=true;
#endif
    progress_native_geometry=mod_setting(L"progress_native",L"geometry",1,L"progress_native.ini")!=0;
    if(!progress_native_guards_match()) {log_event("progress_native_guard_rejected");return false;}
    if(!QueryPerformanceFrequency(&progress_clock_frequency) || progress_clock_frequency.QuadPart<=0)return false;
    progress_grid.initialize();
    Obj relay=allocate_near(image+0x2AA850);if(!relay)return false;
    ProgressNativeCode code;
    if(!prepare_progress_native(code,relay)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    DWORD old;
    if(!VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    if(!FlushInstructionCache(GetCurrentProcess(),relay,384)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    if(!RtlAddFunctionTable(reinterpret_cast<RUNTIME_FUNCTION *>(relay+256),1,reinterpret_cast<DWORD64>(relay))) {
        VirtualFree(relay,0,MEM_RELEASE);return false;
    }
    // After any publication attempt retain the tiny relay: a game thread may
    // already be inside it. AI falls back to its gate; the distance kernel is
    // pure/equivalent even if an in-flight call finishes after entry rollback.
    if(!publish_progress_native(code)) {log_event("progress_native_publish_rejected");return false;}
    log_event("progress_native_hooks_installed",progress_native_geometry?48400:0,progress_native_profile?1:0,(progress_native_geometry?1:0)+(progress_native_profile?4:0));return true;
}
