#include "native_core.inl"   // settings, log, address helpers, shared state (2026-11-16 refactor)
#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG) && !defined(SPLIT_COMBAT)
extern "C" void gpu_profile_crumb(const char *name,bool entering) {
    if(costprofile::call_trace_enabled)costprofile::crumb(name,entering?costprofile::Enter:costprofile::Leave);
}
#endif
#if !defined(SPLIT_COMBAT)
// local205: game data and the shared log for the frame-pacing switches in gpu_profile.cpp (a unit without the address table).
// 0 = the frame-rate table (int frames per second [2] = 30, 60, then float seconds per frame [2]; index = FPS_SETTING, read by
// 9379F0 / 937A00 / 937A90), 1 = the variable that holds the application object (written by its constructor 11B070),
// 2 = the variable that holds the render device object (stored by 94E8F0; +0x3080 swap chain, +0x305C SyncInterval, local206).
extern "C" unsigned char *gpu_profile_game_data(unsigned which) {
    const unsigned rva=which==0?GA(GA_1A8D5F8):which==1?GA(GA_1BA2BC0):which==2?GA(GA_27DF718):0u;
    return rva?reinterpret_cast<unsigned char *>(GetModuleHandleW(nullptr))+rva:nullptr;
}
extern "C" void gpu_profile_event(const char *kind,unsigned a,unsigned b,unsigned c) {log_event(kind,a,b,c);(void)kind;(void)a;(void)b;(void)c;}
#endif
#define UNITY_SLOT_AI_SUPPORT
#include "module_unity.inl"
#undef UNITY_SLOT_AI_SUPPORT
#if !defined(SPLIT_COMBAT)
// local206: gpu_profile.cpp writes no DXGI slot. It reaches the Present edge through SAN14_Layout.dll, the owner of the shared
// Present hook (layout_client is included by the slot above in routed convenience builds); elsewhere -100 = nothing installed.
extern "C" int gpu_profile_layout_watch(void (*built)(void *,void *)) {
#if defined(LAYOUT_ROUTED)
    const San14LayoutApiV1 *api=layout_client::get();
    return api && api->on_layout_built?api->on_layout_built(0,nullptr,built,nullptr):-100;
#else
    (void)built;return -100;
#endif
}
extern "C" int gpu_profile_overlay_subscribe(void (*draw)(void *,void *)) {
#if defined(LAYOUT_ROUTED)
    const San14LayoutApiV1 *api=layout_client::get();
    return api?api->overlay_subscribe(draw,nullptr):-100;
#else
    (void)draw;return -100;
#endif
}
#endif
#include "core_services.h"   // subscription services (map-tick, domestic-tick, ui-dialogs, overlay)
#define UNITY_SLOT_CAMPAIGN
#include "module_unity.inl"
#undef UNITY_SLOT_CAMPAIGN
#define UNITY_SLOT_DEPARTURES
#include "module_unity.inl"
#undef UNITY_SLOT_DEPARTURES
#define UNITY_SLOT_EXPERT
#include "module_unity.inl"
#undef UNITY_SLOT_EXPERT

// crc32_run continues a running (not yet inverted) CRC state; crc32 is the whole-buffer value.
static unsigned crc32_run(unsigned crc, const unsigned char *data, unsigned size)
{
    for (unsigned i = 0; i < size; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}
static unsigned crc32(const unsigned char *data, unsigned size) {return ~crc32_run(~0u, data, size);}
#define UNITY_SLOT_PERSONNEL_POLICY
#include "module_unity.inl"
#undef UNITY_SLOT_PERSONNEL_POLICY
#if !defined(SPLIT_COMBAT) && (defined(PERSONNEL_CANDIDATE) || defined(RECRUITMENT_UI_CANDIDATE))
static bool personnel_healthy_actor(Obj node,Obj context,unsigned field);
static Obj personnel_context_object(Obj node,Obj context,unsigned field);
#endif
#define UNITY_SLOT_DOMESTIC
#include "module_unity.inl"
#undef UNITY_SLOT_DOMESTIC

// Text size / CRC of the running build come from the address table (user 2026-11-05 patch resilience); fixtures and
// unknown builds use the reference build's values. kTextSize/kTextCrc32 (native_profile.h) stay as the reference-build constants.
static unsigned text_size_now() {int b=ga::build_index();return ga_builds[b<0?0:b].text_size;}
static unsigned text_crc_now() {int b=ga::build_index();return ga_builds[b<0?0:b].text_crc;}
[[maybe_unused]] static bool code_ready()
{
    __try {
        return crc32(image + 0x1000, text_size_now()) == text_crc_now();
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// local11 (2026-11-06, docs/HISTORY.md section 18): the whole-text CRC with the displaced prefix of every OBSERVED entry
// (ga::observers: E9 rel32 leaving the image + NOP padding, San14Sim's version.dll) replaced by the original bytes. Without observers it is
// exactly crc32(text). Decision: the observers are taken out of the comparison (text_exact stays true) instead of weakening text_exact into
// a second "exact except observers" flag: every dependent (the observer-aware guards of battle_deaths, trait_hover) keeps one meaning,
// "the text is the known build", and a text that differs anywhere else still falls back to the compat rule below.
static unsigned crc32_text_observed(ga::ObserverSet &set)
{
    ga::observers(image, set);
    constexpr unsigned base = 0x1000;
    const unsigned size = text_size_now();
    unsigned order[GA_OBSERVER_COUNT], n = 0;
    for (unsigned k = 0; k < set.count; ++k) {    // prefixes inside .text, ascending
        if (set.rva[k] < base || set.rva[k] - base + set.len[k] > size) continue;
        unsigned j = n++;
        while (j && set.rva[order[j - 1]] > set.rva[k]) {order[j] = order[j - 1]; --j;}
        order[j] = k;
    }
    unsigned crc = ~0u, pos = 0;
    for (unsigned q = 0; q < n; ++q) {
        const unsigned k = order[q], off = set.rva[k] - base;
        if (off < pos) continue;                  // entries are distinct functions: never overlap, never counted twice
        crc = crc32_run(crc, image + base + pos, off - pos);
        crc = crc32_run(crc, set.bytes[k], set.len[k]);
        pos = off + set.len[k];
    }
    crc = crc32_run(crc, image + base + pos, size - pos);
    return ~crc;
}
static ga::ObserverSet text_observers;   // observers taken out of the last readiness CRC (logged once the text is accepted)
// Steam may still be decoding .text. A mismatched CRC alone is never readiness.
static bool text_exact=true;
static constexpr unsigned code_poll_limit=1200;
struct CodeReadiness {unsigned polls,crc,mask;bool sampled;};
static CodeReadiness code_readiness;
// Readiness probes: five-byte prologues of 12 table ids whose bytes are identical in every known build (game_addresses_data.inl).
struct DecodeProbe {unsigned rva;unsigned char bytes[5];};
static DecodeProbe make_probe(unsigned i) {DecodeProbe p={};p.rva=GA(ga_probes[i].id);memcpy(p.bytes,ga_probes[i].bytes,5);return p;}
static const DecodeProbe decode_probes[]={
    make_probe(0),make_probe(1),make_probe(2),make_probe(3),make_probe(4),make_probe(5),
    make_probe(6),make_probe(7),make_probe(8),make_probe(9),make_probe(10),make_probe(11)
};
static unsigned decode_matches(unsigned mask) {
    unsigned n=0;for(;mask;mask>>=1)n+=mask&1;return n;
}
static bool code_sample_ready(CodeReadiness &s,unsigned crc,unsigned mask) {
    if(s.polls++>=code_poll_limit)return false;
    constexpr unsigned total=sizeof(decode_probes)/sizeof(decode_probes[0]);
    unsigned matches=decode_matches(mask);
    if(crc==text_crc_now()) {
        text_exact=true;
        for(unsigned k=0;k<text_observers.count;++k)log_event("observer_hook_tolerated",0,text_observers.rva[k]);   // feature 0 = whole-text CRC
        return true;
    }
    bool ready=matches*4>=total*3 && s.sampled && s.crc==crc && s.mask==mask;
    s.sampled=matches*4>=total*3;s.crc=crc;s.mask=mask;
    if(ready) {text_exact=false;log_event("text_crc_mismatch_compat",matches,total);}
    return ready;
}
// Unknown game build (no per-build table): ready once the signature resolver finds 3/4 of the signature/RTTI ids on two
// consecutive polls; the resolved table is then adopted for every feature (docs/PATCH_RESILIENCE.md).
static bool ga_unknown_ready(CodeReadiness &s) {
    if(s.polls++>=code_poll_limit)return false;
    LARGE_INTEGER f,a,b;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
    std::vector<unsigned> v(GA_COUNT);
    ga::resolve_image(image,reinterpret_cast<uint64_t>(image),v.data());
    QueryPerformanceCounter(&b);
    unsigned want=0,got=0;
    for(unsigned i=0;i<GA_COUNT;++i)if(ga_entries[i].method!=0 && ga_entries[i].method!=2 && ga_entries[i].method!=5) {++want;if(v[i])++got;}
    unsigned mask=got*4>=want*3?0xFFFu:0;
    bool ready=mask && s.sampled && s.mask==mask;
    s.sampled=mask!=0;s.crc=0;s.mask=mask;
    if(ready) {
        text_exact=false;ga::adopt(v.data(),GA_COUNT);
        log_event("addr_resolve",static_cast<unsigned>((b.QuadPart-a.QuadPart)*10000/f.QuadPart),ga::state().resolved,ga::state().unresolved);
    }
    return ready;
}
static bool code_unpacked_ready(CodeReadiness &s) {
    __try {
        if(ga::build_index()<0 && ga::state().game_host)return ga_unknown_ready(s);
        unsigned mask=0;
        for(unsigned i=0;i<sizeof(decode_probes)/sizeof(decode_probes[0]);++i)
            if(!ga::mcmp_at(image,decode_probes[i].rva,decode_probes[i].bytes,5))mask|=1u<<i;
        // Avoid hashing encrypted text on each poll. Strict text matches every probe.
        if(decode_matches(mask)*4<3*(sizeof(decode_probes)/sizeof(decode_probes[0])))
            return code_sample_ready(s,0,mask);
        return code_sample_ready(s,crc32_text_observed(text_observers),mask);
    } __except(EXCEPTION_EXECUTE_HANDLER) {s.sampled=false;++s.polls;return false;}
}
#ifndef SPLIT_COMBAT   // recruitment hook tables: the combat DLL never publishes them (W5-1)
#ifdef RECRUITMENT_UI_CANDIDATE
struct RecruitUILink {unsigned slot,original;void *replacement;void **next;};
static const RecruitUILink recruit_ui_links[]={
    {GA(GA_1444070),GA(GA_6F3FF0),reinterpret_cast<void *>(&recruit_layout_init),&next_recruit_layout_init},
    {GA(GA_1443E78),GA(GA_735F60),reinterpret_cast<void *>(&recruit_state_update),&next_recruit_state_update},
    {GA(GA_13AF800),GA(GA_2C2BA0),reinterpret_cast<void *>(&recruit_selector_init),&next_recruit_selector_init},
    {GA(GA_13AF798),GA(GA_6C1040),reinterpret_cast<void *>(&recruit_selector_close),&next_recruit_selector_close}
};
static bool recruit_ui_guards_match() {
#ifndef LAYOUT_ROUTED   // routed: slot ownership is judged by the Layout hook manager
    for(const auto &link:recruit_ui_links)if(pointer(image,link.slot)!=image+link.original)return false;
#endif
    return true;
}
static bool publish_recruit_ui() {
    for(const auto &link:recruit_ui_links)if(!ROUTE_VT(link.slot,link.original,link.replacement,link.next))return false;
    return true;
}
static void restore_recruit_ui() {
#ifndef LAYOUT_ROUTED
    for(const auto &link:recruit_ui_links)restore_pointer(link.slot,link.original,link.replacement);
#endif
}
#endif
// Publish only the two recruitment AI nodes in the integrated build. The
// broader, uninstalled PERSONNEL_CANDIDATE also changes unrelated commands.
#if defined(PERSONNEL_CANDIDATE) || defined(RECRUITMENT_UI_CANDIDATE)
struct RecruitAILink {unsigned slot,original;Execute replacement;const unsigned char *entry;};
static const unsigned char recruit_ai_target_entry[]={0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0xC7,0x44,0x24,0x28,0xFE,0xFF,0xFF,0xFF,0x48};
static const unsigned char recruit_ai_command_entry[]={0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x40,0x48,0x8B,0xF9,0x48,0xC7,0x44};
static const RecruitAILink recruit_ai_links[]={
    {GA(GA_134FE18),GA(GA_7C300),&recruit_ai_targets,recruit_ai_target_entry},
    {GA(GA_13493D8),GA(GA_2D720),&recruit_ai_command,recruit_ai_command_entry}
};
static bool recruit_ai_guards_match() {
    for(const auto &link:recruit_ai_links)
        if(pointer(image,link.slot)!=image+link.original || ga::mcmp_at(image,link.original,link.entry,16))return false;
    return true;
}
static bool publish_recruit_ai() {
    for(const auto &link:recruit_ai_links)
        if(!patch_pointer(link.slot,link.original,reinterpret_cast<void *>(link.replacement)))return false;
    return true;
}
static void restore_recruit_ai() {
    for(const auto &link:recruit_ai_links)
        restore_pointer(link.slot,link.original,reinterpret_cast<void *>(link.replacement));
}
#endif
#endif   // !SPLIT_COMBAT
#ifndef SPLIT_COMBAT   // recruitment hook tables: the combat DLL never publishes them (W5-1)
#ifdef RECRUITMENT_UI_CANDIDATE
struct RecruitAlertEntry {unsigned rva;unsigned char block[16];void *replacement;};
static const RecruitAlertEntry recruit_alert_entries[]={
    {GA(GA_2E1534),{0xD5,0x48,0x8B,0xCF,0xE8,0xF7,0xF4,0xF0,0xFF,0x85,0xC0,0x74,0x3B,0x48,0x85,0xF6},reinterpret_cast<void *>(&recruit_alert_candidate)},
    {GA(GA_2E12DB),{0x8D,0x45,0x50,0x48,0x8D,0x55,0x48,0x48,0x8D,0x4D,0xE0,0xE8,0xB0,0x3F,0xFD,0xFF},reinterpret_cast<void *>(&recruit_alert_search)},
    {GA(GA_2B5601),{0xBF,0xE8,0xFA,0xDF,0xF7,0xFF,0x49,0x8B,0xF7,0x4C,0x89,0x7D,0x9F,0x8B,0x45,0xBF},reinterpret_cast<void *>(&recruit_alert_location)}
};
struct RecruitAlertCode {CodeLink links[3];};
static bool recruit_alert_guards_match() {
    for(const auto &entry:recruit_alert_entries)
        if(ga::mcmp_at(image,(entry.rva&~15u),entry.block,16))return false;
    return true;
}
static bool prepare_recruit_alert(RecruitAlertCode &code,Obj relay) {
    if(!recruit_alert_guards_match())return false;
    for(unsigned i=0;i<3;++i) {
        Obj stub=relay+2944+i*32;absolute_jump(stub,recruit_alert_entries[i].replacement);
        if(!prepare_link(code.links[i],recruit_alert_entries[i].rva,0xE8,stub))return false;
    }
    return true;
}
static bool publish_recruit_alert(RecruitAlertCode &code) {
    for(auto &link:code.links)if(!publish_link(link))return false;
    return true;
}
static void restore_recruit_alert(RecruitAlertCode &code) {for(auto &link:code.links)restore_link(link);}
#endif
#endif   // !SPLIT_COMBAT
#define UNITY_SLOT_LINKS
#include "module_unity.inl"
#undef UNITY_SLOT_LINKS
#if defined(PERSONNEL_CANDIDATE) || defined(HIDDEN_LOYALTY)
[[maybe_unused]] static bool loyalty_guards_match() {
    const unsigned char numeric[]={0x48,0x0F,0x4E,0xC2},text[]={0x0F,0x4F,0xD8};
    return !ga::mcmp(image,GA_34CB96,0,numeric,sizeof(numeric)) && !ga::mcmp(image,GA_35EF25,0,text,sizeof(text));
}
#endif
#ifdef PERSONNEL_CANDIDATE
struct PersonnelNodeLink {unsigned slot,original;Execute replacement;};
static const PersonnelNodeLink personnel_links[]={
    {GA(GA_134FE18),GA(GA_7C300),&personnel_recruitment_targets},
    {GA(GA_134A340),GA(GA_39FF0),&personnel_employ_actors},
    {GA(GA_134A550),GA(GA_3DCD0),&personnel_search_actors},
    {GA(GA_13493D8),GA(GA_2D720),&personnel_employ_command},
    {GA(GA_134C580),GA(GA_54AD0),&personnel_search_command},
    {GA(GA_1355B18),GA(GA_7F4F0),&personnel_officer_candidates},
    {GA(GA_1349B20),GA(GA_35A70),&personnel_departure},
    {GA(GA_1349BC8),GA(GA_362D0),&personnel_transport},
    {GA(GA_134AA50),GA(GA_3E750),&personnel_recruit_officer},
    {GA(GA_13487B8),GA(GA_3EF50),&personnel_training_officer},
    {GA(GA_134AF48),GA(GA_3EBC0),&personnel_spy_officer}
};
static bool personnel_guards_match() {
    if(!loyalty_guards_match())return false;
    for(const auto &link:personnel_links)if(pointer(image,link.slot)!=image+link.original)return false;
    return true;
}
static bool publish_personnel_nodes() {
    for(const auto &link:personnel_links)
        if(!patch_pointer(link.slot,link.original,reinterpret_cast<void *>(link.replacement)))return false;
    return true;
}
static void restore_personnel_nodes() {
    for(const auto &link:personnel_links)
        restore_pointer(link.slot,link.original,reinterpret_cast<void *>(link.replacement));
}
#endif
#include <array>
#define UNITY_SLOT_MODULES
#include "module_unity.inl"
#undef UNITY_SLOT_MODULES
#if defined(SPLIT_MODULE_FIXTURE) || defined(TACTICS_FIXTURE)
#include "native_area_index.inl" // guard-only binding for synthetic images
#include "native_path_fast.inl" // guard-only binding for synthetic images
#include "native_diplomacy_intel.inl" // guard-only binding for synthetic images
#include "native_advisor_briefing.inl" // guard-only binding for synthetic images
#endif
#if defined(SPLIT_BASE) || defined(SPLIT_COMBAT)
#define UNITY_SLOT_UI_HOSTS
#include "module_unity.inl"
#undef UNITY_SLOT_UI_HOSTS
#include "native_split_install.inl"
#else
#include "../tests/native_install_fixture.inl"
#endif

#ifdef SPLIT_BASE
// 2026-11-18 (ic_gate.h): with the integrated patch's IC installed, the combat publish (sortie/route CALLs inside IC's
// check ranges), the deferred convenience rows and the advisor 9332B0 link wait for IC's start-up check on this worker.
// Publishing is CAS writes only (no game function call); Layout installs rows added after its publish right away.
static const SplitCombatApi *ic_late_combat;
static void ic_late_publish(unsigned how,unsigned ms) {
    // how: 1 check done, 2 no export, 3 IC not loaded in 60 s, 4 still checking at 10 min, 5 no worker thread (published at once)
    log_event("ic_gate_ready",how,ms,0);
    if(const SplitCombatApi *api=ic_late_combat) {
        const bool combat_installed=api->publish();
        if(!combat_installed)api->rollback();
        else api->activate();
        log_event("combat_installed",combat_installed?1:0,1);   // second word 1 = published by the IC late worker
    }
#if !defined(SPLIT_MODULE_FIXTURE)
    split_run_late_modules();
    advisor_briefing::publish_late();
    // Diagnostic (route_hook 44E50D sits inside the day-progress frame 44DEE0): 1 = the frame slot 13D9728 still holds the
    // original function after IC, 0 = IC 6 replaced the frame and our route call is bypassed while that is on.
    log_event("ic_day_frame_slot",pointer(image,GA(GA_13D9728))==image+GA(GA_44DEE0)?1u:0u,0,0);
#endif
    InterlockedExchange(&ic_gate::late,0);
}
static DWORD WINAPI ic_late_worker(LPVOID) {
    unsigned ms=0;
    const unsigned how=ic_gate::wait(ms);
    ic_late_publish(how,ms);
    return 0;
}
static bool install() {
    const SplitCombatApi *api=split_combat_api;
    const bool valid_api=api && api->size==sizeof(SplitCombatApi) && api->abi==split_module_abi &&
        api->prepare && api->publish && api->rollback && api->activate && api->departure_scope;
    if(!valid_api) {log_event("combat_module_abi_rejected");split_combat_api=nullptr;}
    load_mod_features();
    const bool combat=valid_api && !mod_features.trace_original;
#if !defined(SPLIT_MODULE_FIXTURE)
    const bool ic_late=ic_gate::installed();
#else
    const bool ic_late=false;
#endif
    if(ic_late)InterlockedExchange(&ic_gate::late,1);
    // Prepare both owners before either publishes; failures belong to one owner.
    const bool combat_prepared=combat && api->prepare(image);
    const bool convenience_prepared=split_prepare();
    bool combat_installed=false,convenience_installed=false;
    if(combat_prepared && !ic_late) {
        combat_installed=api->publish();
        if(!combat_installed)api->rollback();
    } else if(combat && !combat_prepared)api->rollback();
    if(convenience_prepared) {
        convenience_installed=split_publish();
        if(!convenience_installed)split_rollback();
    } else split_rollback();
    if(combat_installed)api->activate();
    if(convenience_installed)split_activate();
    if(!(ic_late && combat_prepared))log_event("combat_installed",combat_installed?1:0);
    log_event("convenience_installed",convenience_installed?1:0);
    log_event("split_modules_installed",split_module_abi,(combat_installed?1u:0u)|(convenience_installed?2u:0u),ic_late?1u:0u);
    if(ic_late) {
        ic_late_combat=combat_prepared?api:nullptr;
        log_event("ic_gate_wait",combat_prepared?1u:0u,convenience_installed?1u:0u,0);
        HANDLE worker=CreateThread(nullptr,0,ic_late_worker,nullptr,0,nullptr);
        if(worker)CloseHandle(worker);
        else ic_late_publish(5,GetLastError());
        return combat_prepared || convenience_installed;
    }
    return combat_installed || convenience_installed;
}
#endif
#ifdef SPLIT_COMBAT
static bool __cdecl combat_module_prepare(Obj host) { COST_SCOPE("combat_module_prepare");
#ifndef SPLIT_MODULE_FIXTURE
    if(host!=reinterpret_cast<Obj>(GetModuleHandleW(nullptr)))return false;
#endif
    image=host;initialize_log_settings();
    // The host has finished its unpacking wait. Reuse the identical strict/compat
    // predicate here, with a second 250 ms sample for mismatched text (ABI stays 1).
    CodeReadiness readiness={};
    if(!code_unpacked_ready(readiness)) {
        Sleep(250);if(!code_unpacked_ready(readiness))return false;
    }
    crash_image_size=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+reinterpret_cast<IMAGE_DOS_HEADER *>(image)->e_lfanew)->OptionalHeader.SizeOfImage;
    auto dll=reinterpret_cast<Obj>(self_module);
    crash_module_size=reinterpret_cast<IMAGE_NT_HEADERS64 *>(dll+reinterpret_cast<IMAGE_DOS_HEADER *>(dll)->e_lfanew)->OptionalHeader.SizeOfImage;
#ifndef NO_RUNTIME_LOG
    if(diagnostic_logging)crash_prepare_unwind();
#endif
    if(diagnostic_logging)AddVectoredExceptionHandler(0,observe_native_exception);
    return split_prepare();
}
static void __cdecl combat_departure_scope(int delta) { COST_SCOPE("combat_departure_scope");
    if(delta>0)++sortie_auto_departure_scope;
    else if(delta<0 && sortie_auto_departure_scope)--sortie_auto_departure_scope;
}
#ifndef NO_RUNTIME_LOG // diagnostic builds only; release export list stays fixed
// Optional diagnostic export (not part of SplitCombatApi): size 0 enables labels; otherwise copies and
// clears this thread's labels. Called by ai_trace on the AI thread that ran the traced node.
extern "C" __declspec(dllexport) unsigned __cdecl SAN14CombatTraceStep(char *out,unsigned size) { COST_SCOPE("SAN14CombatTraceStep");
    if(!size) {InterlockedExchange(&mod_step_wanted,1);return 0;}
    unsigned n=mod_step_length<size?mod_step_length:size-1;
    if(out) {memcpy(out,mod_step_text,n);out[n]=0;}
    mod_step_length=0;mod_step_text[0]=0;return n;
}
#endif
extern "C" __declspec(dllexport) const SplitCombatApi *__cdecl SAN14CombatGetApi(unsigned abi) { COST_SCOPE("SAN14CombatGetApi");
    static const SplitCombatApi api={sizeof(SplitCombatApi),split_module_abi,combat_module_prepare,
        split_publish,split_rollback,split_activate,combat_departure_scope};
    return abi==split_module_abi?&api:nullptr;
}
#else
static DWORD WINAPI initialize(LPVOID)
{
    initialize_log_settings();
    image = reinterpret_cast<Obj>(GetModuleHandleW(nullptr));
    wchar_t file[MAX_PATH]; GetModuleFileNameW(nullptr, file, MAX_PATH);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER *>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    auto pe = reinterpret_cast<IMAGE_NT_HEADERS64 *>(image + dos->e_lfanew);
    if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) { log_event("unsupported_executable"); return 0; }
    if (ga::find_build(pe->FileHeader.TimeDateStamp) < 0) log_event("addr_unknown_build", pe->FileHeader.TimeDateStamp);   // signature resolver path (user 2026-11-05)
    crash_image_size=pe->OptionalHeader.SizeOfImage;
    auto module_image=reinterpret_cast<Obj>(self_module);
    auto module_dos=reinterpret_cast<IMAGE_DOS_HEADER *>(module_image);
    auto module_pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(module_image+module_dos->e_lfanew);
    crash_module_size=module_pe->OptionalHeader.SizeOfImage;
    for (unsigned attempt = 0; attempt < 1200; ++attempt) {
        bool ready=code_unpacked_ready(code_readiness);
        if (ready) {
#ifndef NO_RUNTIME_LOG
            if(diagnostic_logging)crash_prepare_unwind();
#endif
            if(diagnostic_logging && AddVectoredExceptionHandler(0,observe_native_exception))log_event("exception_diagnostic_ready");
            bool installed=install();
#ifdef AWARD_WAREHOUSE
            if(installed)log_event("award_warehouse_hooks_installed",10,6,0);
#endif
#ifdef INJURY_PROTECTION
            if(installed)log_event("injury_protection_hooks_installed",14,41,0);
#endif
#ifdef SORTIE_FORMATION
            if(installed)log_event("cohesive_sortie_hooks_installed",40000,5,8);
#endif
#ifdef RECRUITMENT_UI_CANDIDATE
            log_event(installed ? "merit_recruitment_ui_candidate_hooks_installed" : "installation_rejected");
#else
            log_event(installed ? "campaign_v7_domestic_hooks_installed" : "installation_rejected");
#endif
            return 0;
        }
        Sleep(250);
    }
    log_event("native_code_timeout");
    return 0;
}
#endif
#ifdef SPLIT_BASE
extern "C" __declspec(dllexport) bool __cdecl SAN14ConvenienceInitialize(unsigned abi,const SplitCombatApi *api) { COST_SCOPE("SAN14ConvenienceInitialize");
    if(abi!=split_module_abi || !api || api->size!=sizeof(SplitCombatApi) || api->abi!=split_module_abi ||
       !api->prepare || !api->publish || !api->rollback || !api->activate || !api->departure_scope)return false;
    static volatile LONG started=0;
    if(InterlockedCompareExchange(&started,1,0)!=0)return false;
    split_combat_api=api;
    initialize(nullptr);
    return true;
}
#endif
#ifdef SPLIT_MODULE_FIXTURE
static bool __cdecl split_fixture_prepare(Obj host,const SplitCombatApi *api) {
#ifdef SPLIT_COMBAT
    (void)api;return combat_module_prepare(host);
#else
    image=host;split_combat_api=api;
    CodeReadiness readiness={};
    if(!code_unpacked_ready(readiness)) {Sleep(250);if(!code_unpacked_ready(readiness))return false;}
    return split_prepare();
#endif
}
// Each root exposes only what it compiles (W5): the fixture reads the sortie scope from the combat root and the personnel departure wrapper
// from the base root; the other root's entry is never called.
static unsigned __cdecl split_fixture_scope() {
#ifdef SPLIT_COMBAT
    return sortie_auto_departure_scope;
#else
    return 0;
#endif
}
static uint64_t __cdecl split_fixture_departure() {
#ifdef SPLIT_BASE
    return personnel_departure(nullptr,nullptr,nullptr);
#else
    return 0;
#endif
}
static void __cdecl split_fixture_failure(unsigned step) {split_fixture_fail_after=step;}
extern "C" __declspec(dllexport) bool __cdecl SAN14SplitFixtureInstall(Obj host,const SplitCombatApi *api) {
#ifdef SPLIT_BASE
    image=host;split_combat_api=api;
    CodeReadiness readiness={};
    if(!code_unpacked_ready(readiness)) {Sleep(250);if(!code_unpacked_ready(readiness))return false;}
    return install();
#else
    (void)host;(void)api;return false;
#endif
}
#ifdef SPLIT_BASE
extern "C" __declspec(dllexport) void *__cdecl SAN14SharedControlsFixture(unsigned action,Obj object,void *next) {
    if(action==0)return reinterpret_cast<void *>(&unified_dialogs::controls);
    (void)next;unified_dialogs::controls(object);return nullptr;
}
#endif
#ifdef SPLIT_BASE
// 2026-11-16 refactor: a split_pointers row copies terrain-unit values at load (tu_terrain_api.h); 1 = the row holds the
// resolved values, i.e. the other unit's initializer ran first in this DLL (cross-unit initialization order).
extern "C" __declspec(dllexport) unsigned __cdecl SAN14SplitFixturePointerRows() {
    for(const auto &p:split_pointers)if(p.replacement==reinterpret_cast<void *>(&terrain_trees::update))
        return p.slot && p.slot==GA(GA_1697960) && p.original==GA(GA_BB3AC0)?1u:0u;
    return 0;
}
#endif
extern "C" __declspec(dllexport) void __cdecl SAN14SplitFixtureDisable(unsigned mask) {split_fixture_disabled=mask;}
extern "C" __declspec(dllexport) unsigned __cdecl SAN14SplitFixtureFeatures() {
    return (mod_features.merit?2u:0u)|(mod_features.domestic?4u:0u)|
        (mod_features.population?8u:0u)|(mod_features.battle_rules?16u:0u)|(mod_features.advisor_briefing?32u:0u)|
        (mod_features.recruit?64u:0u)|(mod_features.injury?128u:0u)|(mod_features.loyalty?256u:0u)|(mod_features.awards?512u:0u)|
        (mod_features.recruit_ai?1024u:0u);   // local3: optional features whose own publication failure is tested separately
}
extern "C" __declspec(dllexport) const SplitFixtureApi *__cdecl SAN14SplitFixtureGetApi() {
    static const SplitFixtureApi api={split_fixture_prepare,split_publish,split_rollback,split_activate,
        split_fixture_scope,split_fixture_departure,split_fixture_failure};return &api;
}
#endif
extern "C" __declspec(dllexport) void AIInitialize() { COST_SCOPE("AIInitialize"); /* import anchor */ }
#ifndef TACTICS_FIXTURE
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        self_module = module;
#ifdef RUNTIME_COST_PROFILE
#ifdef NO_RUNTIME_LOG
        costprofile::initialize(self_module);
#else
        if(dev_group() && mod_setting(L"diagnostics",L"cost_profile",0))costprofile::initialize(self_module);
#endif
#endif
#ifndef NO_RUNTIME_LOG
        GetModuleFileNameW(self_module, log_path, MAX_PATH);
        wchar_t *separator = wcsrchr(log_path, L'\\');
        if (separator) wcscpy_s(separator + 1, MAX_PATH - static_cast<size_t>(separator + 1 - log_path),
#ifdef SPLIT_COMBAT
            L"SAN14_CombatAI.log");
#else
            L"SAN14_AITactics.log");
#endif
        WIN32_FILE_ATTRIBUTE_DATA log_info{};
        if (separator && GetFileAttributesExW(log_path, GetFileExInfoStandard, &log_info) &&
            (log_info.nFileSizeHigh || log_info.nFileSizeLow >= 16u * 1024u * 1024u)) {
            wchar_t old_path[MAX_PATH];
            if (wcscpy_s(old_path, log_path) == 0) {
                wchar_t *extension = wcsrchr(old_path, L'.');
                if (extension && wcscpy_s(extension, MAX_PATH - static_cast<size_t>(extension - old_path), L".old.log") == 0)
                    MoveFileExW(log_path, old_path, MOVEFILE_REPLACE_EXISTING);
            }
        }
#endif
    }
    return TRUE;
}
#endif

