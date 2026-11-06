// SAN14_Layout.dll (user exception 2026-11-05, docs/LAYOUT_DLL_COMPAT_DESIGN.md).
// Owns hook installation (vtable slots, E8 call sites, entry blocks), the shared Present hook and the
// game-window subclass. P4 (native_layout_ui.inl): layout built/destroyed events and the runtime UI services call original UI
// functions, only from the game thread that raised the event (or from a subscriber callback). The DLL is pinned and never unloads.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <initializer_list>
#include "game_addresses.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <intrin.h>
#include <initializer_list>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "layout_api.h"
// Layout has no cost profiler (that lives in the module that owns the profile); the macro keeps the hook-scope audit
// (tests/verify_hook_scopes.py) meaningful for the hooks defined here, same as ccz_module.cpp.
#ifndef COST_SCOPE
#define COST_SCOPE(name) do {} while(0)
#endif
#ifdef LAYOUT_FIXTURE
#define LAYOUT_EXPORT
#else
#define LAYOUT_EXPORT __declspec(dllexport)
#endif
#include "native_layout_core.inl"
#include "native_layout_present.inl"
#include "native_layout_ui.inl"
namespace layout {
static int api_peer_present(const wchar_t *name) {return peer_present_impl(name);}
static void api_log(const char *owner,const char *event,uint32_t a,uint32_t b) {
    if(event && *event)emit(event,a,b,hash32(owner));
}
static void api_set_log_sink(San14LayoutLogSink next) {set_sink(next);}
static uint32_t api_ga_resolve(uint32_t id) {return id<GA_COUNT?GA(id):0;}
static const uint32_t *api_ga_table(uint32_t *count) {return ga::table(count);}
static const San14LayoutApiV1 api={sizeof(San14LayoutApiV1),san14_layout_abi,
    api_hook_vtable,api_hook_call,api_hook_block,api_unhook,api_hook_state,
    api_on_layout_built,api_on_layout_destroyed,api_load_sublayout,api_attach,api_clone_control,api_create_label,
    api_set_text,api_set_visible,api_bind_click,api_destroy,
    api_overlay_subscribe,api_wndproc_subscribe,
    api_peer_present,api_slot_owner,api_log,
    api_wndproc_bind,api_set_log_sink,api_hook_jump,api_peer_for_owner,api_hook_owner_state,api_hook_span,api_hook_bytes,
    api_note_layout_load,api_layout_unsubscribe,api_set_position,api_get_position,api_ga_resolve,api_ga_table};
// ---- unpack wait (moved from the convenience coordinator; same five-byte probes, 3/4 stable match) ----
struct Probe {unsigned rva;unsigned char bytes[5];};
// 12 prologues identical in every known build (game_addresses_data.inl ga_probes, same set as the other modules).
static Probe make_probe(unsigned i) {Probe p={};p.rva=GA(ga_probes[i].id);memcpy(p.bytes,ga_probes[i].bytes,5);return p;}
static const Probe probes[]={make_probe(0),make_probe(1),make_probe(2),make_probe(3),make_probe(4),make_probe(5),
    make_probe(6),make_probe(7),make_probe(8),make_probe(9),make_probe(10),make_probe(11)};
static const unsigned kProbeCount=sizeof(probes)/sizeof(probes[0]);
static bool read_mask(const Bytes *base,unsigned &mask) {
    mask=0;
    __try {
        for(unsigned i=0;i<kProbeCount;++i)if(!memcmp(base+probes[i].rva,probes[i].bytes,5))mask|=1u<<i;
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static unsigned popcount(unsigned mask) {unsigned n=0;for(;mask;mask>>=1)n+=mask&1;return n;}
// Ready when at least 3/4 of the probes match on two consecutive polls with the same mask.
struct Readiness {unsigned mask;bool sampled;};
static bool unpack_ready(const Bytes *base,Readiness &state) {
    unsigned mask=0;
    if(!read_mask(base,mask)) {state.sampled=false;return false;}
    bool good=popcount(mask)*4>=kProbeCount*3;
    bool ready=good && state.sampled && state.mask==mask;
    state.sampled=good;state.mask=mask;
    return ready;
}
// Unknown game build (no per-build table): the signature resolver must find 3/4 of the signature/RTTI ids on two polls in a row.
static bool resolve_guarded(const Bytes *base,unsigned *out) {
    __try {ga::resolve_image(base,reinterpret_cast<uint64_t>(base),out);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static bool ga_unknown_ready(const Bytes *base,Readiness &state) {
    LARGE_INTEGER f,a,b;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
    std::vector<unsigned> v(GA_COUNT);
    if(!resolve_guarded(base,v.data())) {state.sampled=false;return false;}
    QueryPerformanceCounter(&b);
    unsigned want=0,got=0;
    for(unsigned i=0;i<GA_COUNT;++i)if(ga_entries[i].method!=0 && ga_entries[i].method!=2 && ga_entries[i].method!=5) {++want;if(v[i])++got;}
    unsigned mask=got*4>=want*3?0xFFFu:0;
    bool ready=mask && state.sampled && state.mask==mask;
    state.sampled=mask!=0;state.mask=mask;
    if(ready) {
        ga::adopt(v.data(),GA_COUNT);
        emit("addr_resolve",static_cast<unsigned>((b.QuadPart-a.QuadPart)*10000/f.QuadPart),ga::state().resolved,ga::state().unresolved);
    }
    return ready;
}
static volatile LONG initialize_state; // 0 new, 1 running, 2 ready, 3 failed
static bool initialize(uint32_t version) {
    if(version!=san14_layout_abi)return false;
    if(InterlockedCompareExchange(&initialize_state,1,0)!=0)return initialize_state==2;
    // Pin: callbacks and hooks outlive every caller.
    HMODULE pinned=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&initialize),&pinned);
    image=reinterpret_cast<Bytes *>(GetModuleHandleW(nullptr));
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER *>(image);
    if(!image || dos->e_magic!=IMAGE_DOS_SIGNATURE) {InterlockedExchange(&initialize_state,3);return false;}
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+dos->e_lfanew);
    if(pe->Signature!=IMAGE_NT_SIGNATURE || pe->FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64) {InterlockedExchange(&initialize_state,3);return false;}
    image_size=pe->OptionalHeader.SizeOfImage;
    Readiness state={};
    for(unsigned poll=0;poll<1200;++poll) {
        if(ga::build_index()<0 && ga::state().game_host?ga_unknown_ready(image,state):unpack_ready(image,state)) {
            InterlockedExchange(&initialize_state,2);
            emit("layout_initialized",poll,0,0);
            emit("addr_build",static_cast<unsigned>(ga::build_index()+1),ga::state().resolved,ga::state().unresolved);   // 0 = unknown build
            return true;
        }
        Sleep(250);
    }
    emit("layout_unpack_timeout",0,0,0);
    InterlockedExchange(&initialize_state,3);return false;
}
// 1 Fog, 2 ModCore, 4 Batteki, 8 Yuga, 16 IntegratedConvenience, 32 Fog previous-input
static void publish() {
    unsigned mask=0;
    if(peer_present_impl(L"SAN14_FogTrial.dll"))mask|=1;
    if(peer_present_impl(L"SAN14_ModCore.dll"))mask|=2;
    if(peer_present_impl(L"SAN14_Batteki.dll"))mask|=4;
    if(peer_present_impl(L"SAN14_Yuga.dll"))mask|=8;
    if(peer_present_impl(L"SAN14_IntegratedConvenience.dll"))mask|=16;
    if(peer_present_impl(L"SAN14_Fog43_PreviousInput.dll"))mask|=32;
    if(observation_wanted())register_observation();   // P4: layout built/destroyed observation (subscribers or diagnostic probes)
    publish_hooks();
    emit("layout_published",hook_total,mask,0);
}
}
extern "C" LAYOUT_EXPORT const San14LayoutApiV1 *__cdecl SAN14LayoutGetApi(uint32_t version) {
    return version==san14_layout_abi?&layout::api:nullptr;
}
extern "C" LAYOUT_EXPORT bool __cdecl SAN14LayoutInitialize(uint32_t version) {return layout::initialize(version);}
extern "C" LAYOUT_EXPORT void __cdecl SAN14LayoutPublish() {layout::publish();}
#ifndef LAYOUT_FIXTURE
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {layout::self_module=module;DisableThreadLibraryCalls(module);}
    return TRUE;
}
#endif
