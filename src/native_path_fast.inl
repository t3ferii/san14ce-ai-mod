// Sole owner SAN14_Convenience.dll, original UI/AI caller threads only.
// Evidence/alias closure: tests/verify_path_fast.py; NATIVE_FUNCTIONS path_fast.
#include "game_addresses.h"
#include "path_fast.h"
#include <unordered_map>
namespace path_fast {
static const unsigned entry=GA(GA_184D60),debug_rva=GA(GA_2144AD4);
struct Guard {unsigned rva,size;unsigned long long hash;};
static const Guard guards[]={
    {GA(GA_185720),0x18D,GH(0x5d024e1b2e4243f2ull)},
    {GA(GA_184D60),0x9A2,GH(0x88d3021063886adbull)},
    {GA(GA_18C1F0),0x132,GH(0x73037cef8a22e554ull)},
    {GA(GA_18C330),0x206,GH(0x6e1089a529c4039dull)},
    {GA(GA_18F0D0),0x52,GH(0x909bf8195fc49b3eull)},
    {GA(GA_2A9910),0x148,GH(0x067b596681cb609cull)},
    {GA(GA_2B0A20),0x8B,GH(0x9318de018e96c9ffull)},
    {GA(GA_187880),0x1FB,GH(0x32136e69b532a049ull)},
    {GA(GA_18B410),0x33E,GH(0x50ce7d777678aae0ull)},
    {GA(GA_2CC630),0x155,GH(0xba1eb2f0312805a3ull)},
    {GA(GA_2315A0),0x2C,GH(0x810a4ad693aee636ull)},
    {GA(GA_331780),0x24,GH(0xb302042a0360860cull)},
    {GA(GA_233FF0),0x27,GH(0x301c3f1a6a4e81b6ull)},
    {GA(GA_234570),0x1E,GH(0x1650bb4029e0387full)},
    {GA(GA_EDF0),0x2A,GH(0x7c8aced62620390aull)},   // 2026-11-05 local3: was the raw RVA 0xEE90
    {GA(GA_2C8D00),0x57,GH(0xba7fb119e102708cull)},
    {GA(GA_2C8C00),0xBB,GH(0xdf37fdfec013e0ccull)},
    {GA(GA_188D50),0xC1,GH(0x1a2e52d4933a2507ull)},
    {GA(GA_2ACB90),0x9E,GH(0x293ef7107333cbbdull)},
    {GA(GA_2B11B0),0x23,GH(0x0bbc93973ca1bab9ull)},
    {GA(GA_22E690),0x8B,GH(0x80c21153000a3f45ull)},
    {GA(GA_2340D0),0x80,GH(0x795950c2811f0cceull)},
    {GA(GA_32E3B0),0xCD,GH(0x57f2276ebfa149f1ull)},
    {GA(GA_22F190),0x4D,GH(0x23b7579e3ac79a5eull)},
    {GA(GA_2C5B00),0x6F,GH(0x08842eb4300a6d18ull)},
    {GA(GA_BF1B0),0x8A,GH(0x305daa1a8c4a5bcbull)},
    {GA(GA_222C60),0x35,GH(0x86ce517d9fa2f392ull)},
    {GA(GA_2F1930),0x4C,GH(0x0f293337a8c9f51full)},
    {GA(GA_2B255F),0x52E,GH(0x45b58c4305966c9dull)},
    {GA(GA_2239F0),0xF,GH(0x0d09aea93a8c38e1ull)},
    {GA(GA_33FA10),0x50,GH(0xf8ac6bb7ff1510cfull)},
    {GA(GA_3F9170),0x58,GH(0x07413233f378ff9aull)},
    {GA(GA_4EA20),0x68,GH(0x74c1d9567f125ce9ull)},
    {GA(GA_186020),0x1B5,GH(0xc2f0ea357a64e83eull)},
    {GA(GA_186950),0x1B5,GH(0x2aace30a7ccdcef6ull)},
    {GA(GA_1544B0),0x43,GH(0xc354942479ccec97ull)},
    {GA(GA_14AFC0),0x9B,GH(0x81a4bed8aa7f09daull)}
};
// AI-CONTRACT [hook boundary]: 183A30/184485 belong to convenience; 1843E0 entry belongs to combat. Preserve mutual exclusion with path_count and native-search reentrancy guards; delegation must restore native node state.
static const unsigned clear_site=GA(GA_1857B5);
static constexpr unsigned char clear_bytes_expected[]={0xF3,0xAA};
// 사용자 결정 2026-10-30: 통합패치·발탁장수 공존을 위해 첫머리 훅을 호출 자리로 이동
// Combat now owns these two CALLs inside fingerprinted path helpers.
static const unsigned movement_calls[]={GA(GA_184FF9),GA(GA_18793A)};
static bool movement_call_match(unsigned site) {
    if(image[site]!=0xE8)return false;
    int delta;memcpy(&delta,image+site+1,4);
    uintptr_t target=reinterpret_cast<uintptr_t>(image+site+5)+delta;
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+reinterpret_cast<IMAGE_DOS_HEADER *>(image)->e_lfanew);
    uintptr_t begin=reinterpret_cast<uintptr_t>(image),end=begin+pe->OptionalHeader.SizeOfImage;
    return target==begin+GA(GA_2B2550) || target<begin || target>=end;
}
// local10 G2 3.1: the node buffer slots (slot_of) are image+GA_1BBAB50. The id is a data address, so its value is confirmed here by the
// LEA r8,[rax+disp32] 0x17 bytes before the REP STOSB that clear_site NOPs (same instruction stream in both builds; the 1843F0 guard pins the range).
static bool node_base_ok() {
    const unsigned cs=GA(GA_1857B5);static const unsigned char lea[]={0x4C,0x8D,0x80};
    return cs && !memcmp(image+cs-0x17,lea,3) && static_cast<unsigned>(*reinterpret_cast<const int *>(image+cs-0x14))==GA(GA_1BBAB50);
}
static bool guards_match() {
    for(unsigned site:movement_calls)if(!movement_call_match(site))return false;
    for(const auto &g:guards) {
        unsigned long long h=0xcbf29ce484222325ull;
        for(unsigned i=0;i<g.size;++i) {
            unsigned at=g.rva+i;unsigned char value=image[at];
            for(unsigned site:movement_calls)if(at>site && at<site+5) {
                int native=static_cast<int>(GA(GA_2B2550)-site-5);
                value=reinterpret_cast<unsigned char *>(&native)[at-site-1];
            }
            h=(h^value)*0x100000001b3ull;
        }
        if(h!=g.hash)return false;
    }
    if(!node_base_ok())return false;
    // Independent confirmation of the debug-global RIP displacement, not its value.
    return !ga::mcmp_at(image,clear_site,clear_bytes_expected,sizeof(clear_bytes_expected)) && image[GA(GA_185098)]==0x83 && GA(GA_18509F)+*reinterpret_cast<int *>(image+GA(GA_18509A))==debug_rva;
}
static float movement(Obj army,Obj formation,int preview,int modifier,Obj tile) {
    // The native search used this CALL. Follow its combat-owned thunk so the
    // convenience DLL reads the combat DLL's cohort state, including its next.
    if(movement_call_match(GA(GA_184FF9))) {
        int delta;memcpy(&delta,image+GA(GA_184FFA),4);
        auto next=reinterpret_cast<float(__fastcall *)(Obj,Obj,int,int,Obj)>(image+GA(GA_184FFE)+delta);
        return next(army,formation,preview,modifier,tile);
    }
#if defined(SORTIE_FORMATION) && !defined(SPLIT_BASE)
    // The sortie cohort state lives in the combat DLL (reached above through its thunk). The convenience root has no cohort
    // state of its own, so its former local sortie_movement always returned the native value: call the native function (W5-2, D2).
    return sortie_movement(army,formation,preview,modifier,tile);
#else
    return fn<float(__fastcall *)(Obj,Obj,int,int,Obj)>(GA(GA_2B2550))(army,formation,preview,modifier,tile);
#endif
}
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
using path_fast_core::Node;
using Path=Node *(__fastcall *)(Obj,unsigned,const int *,int,Node *,int,unsigned);
using Unary=int(__fastcall *)(Obj);

static Path original;
static CodeLink link,clear_link;
static bool active;
static volatile LONG skip_clear;
static thread_local bool busy;
// local71: true while the current search was asked by 9DA10 (its CALL 185710 returns to 9DAD7), which reads the cost only.
static bool focus_enabled;
static thread_local bool focus_now;
__declspec(noinline) static bool focus_caller() {
    void *frames[2]={};   // skip this function and path(): [0] returns into 185710, [1] into the caller of 185710
    return RtlCaptureStackBackTrace(2,2,frames,nullptr)==2 && frames[1]==image+GA(GA_9DAD7);
}
// 2026-10-26 memory leak fix (user report: commit grew ~2.4GiB per 2 min at x8, 65.9GB crash).
// The original runs each force AI on a NEW thread, so a per-thread `new` (2.9MB) was never freed.
// One storage per native node-buffer slot instead: 1843E0 holds that slot's critical section
// (141BB3D08[slot]+10, slot<3) around its 183A30 calls, so a slot is never shared concurrently.
// At most three are ever allocated and they live for the process.
// AI-CONTRACT [lifetime]: Storage ownership is per native buffer slot, not per thread. Native force-AI threads are short-lived; thread_local heap ownership caused the v13.1/v13.2 leak. Preserve slot critical-section evidence before changing reuse.
static path_fast_core::Storage *volatile slot_storage[3];
static thread_local path_fast_core::Storage *storage;   // current call's slot storage (no ownership)
// local72 per-search cell memo (user 2026-11-12): a search evaluates a cell once per neighbour it is reached from (up to six
// times) with the same answer. The first answer (blocked, or the adjusted cost) is kept until that search ends; nothing
// outlives the search. One table per native slot, under the same slot lock as the storage.
// local265 (user 2026-11-19, performance proposal 4.4-1): searches with a unit too. Decompiled (ghidra_full, current EXE):
// 18B410 with its unit branch (2C89F0, 2C7420 ring into its own stack buffer), 2B0A20, 2C8C00, 2C8D00 and 188D50 only read
// the cell, the unit / force tables and their arguments (cell and constants of the search) and write nothing; 187880 divides
// by a mobility (2B2550) it keeps per terrain-type key (233310) in the search's aux tree, set on the first cell of that type
// and never changed afterwards. Between allowed() and adjust() the search does not use the cost (path_fast.h search), so a
// kept adjusted value is the answer a second evaluation would give. [diagnostics] path_fast_verify compares with the original.
// local267 (user 2026-11-19, local266 verify run): withdrawn. 11 of 21,897 compared searches differed, all with a unit (flags 1,
// force 6, each on its own AI thread); e.g. one cell cost 12 in the original and 7 from the memo (one evaluation earlier in the
// same search). Whether a later evaluation reads state another thread changed meanwhile or something above is wrong is not
// settled; the memo again serves searches without a unit only (no mismatch among those in the same run).
// local268 (user 2026-11-19 "페이블 조언 다해봐", Fable advice 4): the memo also serves unit searches whose per-cell answers
// have no flag-1 terms: flags & 0xC without 0x11 (2C8C00 decides, 187880 returns the cost unchanged, no 188D50 term).
// Only while trial_enabled (below), so the next verify run judges it before it is kept.
struct Memo {unsigned generation=0;unsigned stamp[path_fast_core::cells]={};std::int32_t value[path_fast_core::cells];};
static constexpr std::int32_t memo_blocked=(std::numeric_limits<std::int32_t>::min)();
static Memo *volatile slot_memo[3];
static thread_local Memo *memo;
static bool memo_enabled,bounded_enabled;
// local268 trial (user 2026-11-19 "페이블 조언 다해봐 그리고 또 재보자"): the new equal-answer speed-ups (flag-8 FIFO search,
// start-region probe, unit memo above) run only in diagnostic builds with [diagnostics] path_fast_verify != 0, so every run
// that uses them also compares samples with the original (MOD_DEV_LESSONS I). A build without diagnostics never uses them.
static bool trial_enabled;
#ifndef NO_RUNTIME_LOG
// local268 shadow (Fable advice on the local266 mismatches): does a cell's answer change within one unit flag-1 search? On a
// sample of those searches (memo off for them) every re-evaluation of a cell is compared with its first one: 1 allowed
// changed, 2 cost after the allowed rules changed, 4 the 187880 output changed. Nothing is reused; the search is unchanged.
struct Shadow {unsigned generation=0;unsigned stamp[path_fast_core::cells]={};unsigned char state[path_fast_core::cells];
               std::int32_t cost[path_fast_core::cells],out[path_fast_core::cells];};
static Shadow *volatile slot_shadow[3];
static thread_local Shadow *shadow_slot;
#endif
struct Native;
struct SearchCells {
    unsigned visits=0,hits=0,region=0;LONGLONG region_ticks=0;
    unsigned probes=0;bool probe_closed=false,began=false,broken=false;LONGLONG setup_ticks=0;   // local268
    unsigned drift_cells=0,drift_bits=0,drift_cell=0;std::int32_t drift_old=0,drift_new=0;bool shadowed=false;
    void add(const Native &n);
};
#ifndef NO_RUNTIME_LOG
static unsigned verify;
static LONGLONG frequency;
struct Diagnostic {
    unsigned start=0,goal=0,flags=0,ctx10=0,ctx28=0,ctx2c=0;
    unsigned key=0,trial=0;   // local268: ctx+0x20 key id; trial bits 1 uniform, 2 probed, 4 probe closed, 8 unit memo, 16 rerun
    int valid=0,force=0;
    Node *original=nullptr,*actual=nullptr;
    path_fast_core::Comparison difference;
};
struct Totals {
    int day=-1;
    unsigned fast=0,delegated=0,compared=0,mismatches=0,multi=0,lines=0,details=0,only_fast=0,all_compared=0,slow_lines=0;
    LONGLONG only_ticks=0;
    unsigned trial_compared=0,trial_mismatch=0,trial_uniform=0,trial_memo=0;   // local268
    unsigned drift_sampled=0,drift_searches=0,drift_cells=0,drift_lines=0;
};
static volatile LONG64 book_capture,book_record;   // local268 bookkeeping ticks (path_fast_book_day)
static SRWLOCK day_lock=SRWLOCK_INIT;
static Totals totals;
// local265 diagnostics (user 2026-11-19, performance proposal 4.4-2): fast searches by kind = flags & 0xFF | 0x100 with a unit
// (ctx+0). Kinds add up over days and are logged when 1000 searches have gathered, one pair per kind:
// path_fast_kind (kind | searches over 5 ms << 16, searches, us) and path_fast_kind_cells (kind, neighbour cells in thousands,
// memo answers per mille). A search of 5 ms or more logs path_fast_slow (start | goal << 16, us, kind | 0x200 found |
// 0x400 cost-only heuristic) and path_fast_slow2 (cells | memo per mille << 22, caller frames 1 and 2 as in path_caller_stack),
// at most 16 a day. Diagnostic builds only; nothing here changes a search.
// local266: path_fast_kind_fail (kind, failed | start region closed within 2048 cells << 16, us of the failed ones); the slow
// line's third field adds 0x800 when the start region was closed and the region size << 12 (see start_region).
// local268: path_fast_kind_split (kind | searches the setup helper rejected << 16, probes | probe "no route" exits << 16, us
// in prepare + begin); path_fast_drift_day / path_fast_drift / path_fast_drift2 (see Shadow); path_fast_book_day (us in
// caller_capture, us in record + dup_note, searches); path_fast_verify_trial (compared searches that used a trial speed-up,
// mismatches among them, of those compared: uniform | unit memo << 16).
struct SearchInfo {unsigned start,goal,flags;bool unit,found,focus;unsigned visits,hits,region;const SearchCells *cells;};
struct KindTotal {unsigned key,calls,slow,failed,closed,rejected,probes,probe_closed;unsigned long long visits,hits;LONGLONG ticks,failed_ticks,setup_ticks;};
static constexpr unsigned kStartRegionCap=2048;   // local266, see start_region
static constexpr unsigned kKinds=16,kKindFlush=1000,kKindRest=(1u<<16)-1,kCellsField=(1u<<22)-1;   // rest key / 16-bit cap, 22-bit cells
static KindTotal kinds[kKinds];static unsigned kind_used,kind_calls;   // under day_lock
// local22 diagnostics (U33, user 2026-11-10 "? ?? ??? ?? ???"): who asks for the paths. Each call keeps six frames
// above 184D60 (EXE rva; a SAN14 DLL frame is 0x80000000|dll<<24|offset: 1 combat, 2 convenience, 3 layout, 4 ccz).
// Days with more than 300 calls log their ten costliest stacks: path_caller_day (calls, us, frame0) and
// path_caller_stack (frame1..3), path_caller_stack2 (frame4, frame5, rank). Diagnostic builds only.
static unsigned microseconds(LONGLONG elapsed);
static constexpr unsigned kCallerFrames=6,kCallerSlots=96;
struct CallerStack {unsigned frame[kCallerFrames];unsigned calls;LONGLONG ticks;};
static CallerStack caller_stacks[kCallerSlots];
static unsigned caller_used;
static thread_local unsigned caller_now[kCallerFrames];
struct DllRange {uintptr_t begin=0,end=0;};
static DllRange dll_ranges[5];
static void caller_ranges() {
    const wchar_t *names[5]={nullptr,L"SAN14_CombatAI.dll",L"SAN14_Convenience.dll",L"SAN14_Layout.dll",L"SAN14_CCZ.dll"};
    for(unsigned i=1;i<5;++i) {
        HMODULE m=GetModuleHandleW(names[i]);if(!m)continue;
        auto dos=reinterpret_cast<IMAGE_DOS_HEADER *>(m);auto pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(reinterpret_cast<unsigned char *>(m)+dos->e_lfanew);
        dll_ranges[i].begin=reinterpret_cast<uintptr_t>(m);dll_ranges[i].end=dll_ranges[i].begin+pe->OptionalHeader.SizeOfImage;
    }
}
static unsigned caller_code(void *address) {
    const uintptr_t a=reinterpret_cast<uintptr_t>(address),base=reinterpret_cast<uintptr_t>(image);
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+reinterpret_cast<IMAGE_DOS_HEADER *>(image)->e_lfanew);
    if(a>=base && a<base+pe->OptionalHeader.SizeOfImage)return static_cast<unsigned>(a-base);
    for(unsigned i=1;i<5;++i)if(dll_ranges[i].begin && a>=dll_ranges[i].begin && a<dll_ranges[i].end)
        return 0x80000000u|(i<<24)|static_cast<unsigned>((a-dll_ranges[i].begin)&0xFFFFFF);
    return 0xFFFFFFFFu;
}
__declspec(noinline) static void caller_capture() {
    if(!diagnostic_logging){caller_now[0]=0;return;}
    void *frames[kCallerFrames+2]={};
    const USHORT got=RtlCaptureStackBackTrace(2,kCallerFrames,frames,nullptr);   // skip path() and its COST_SCOPE caller frame
    for(unsigned i=0;i<kCallerFrames;++i)caller_now[i]=i<got?caller_code(frames[i]):0;
}
static void caller_add(LONGLONG elapsed) {   // under day_lock
    if(!caller_now[0])return;
    for(unsigned i=0;i<caller_used;++i)if(!memcmp(caller_stacks[i].frame,caller_now,sizeof caller_now)) {
        ++caller_stacks[i].calls;caller_stacks[i].ticks+=elapsed;return;
    }
    if(caller_used<kCallerSlots) {
        auto &c=caller_stacks[caller_used++];memcpy(c.frame,caller_now,sizeof caller_now);c.calls=1;c.ticks=elapsed;
    }
}
static void caller_flush(unsigned day_calls) {   // under day_lock, at the day boundary
    if(day_calls>300 && caller_used) {
        for(unsigned rank=0;rank<10 && rank<caller_used;++rank) {
            unsigned best=rank;
            for(unsigned i=rank+1;i<caller_used;++i)if(caller_stacks[i].ticks>caller_stacks[best].ticks)best=i;
            std::swap(caller_stacks[rank],caller_stacks[best]);
            const auto &c=caller_stacks[rank];
            log_event("path_caller_day",c.calls,microseconds(c.ticks),c.frame[0]);
            log_event("path_caller_stack",c.frame[1],c.frame[2],c.frame[3]);
            log_event("path_caller_stack2",c.frame[4],c.frame[5],rank);
        }
    }
    caller_used=0;
}
static volatile LONG64 calls;
static LONGLONG ticks() {LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}
static unsigned microseconds(LONGLONG elapsed) {
    const double us=frequency>0?static_cast<double>(elapsed)*1000000.0/static_cast<double>(frequency):0;
    return us>=UINT32_MAX?UINT32_MAX:static_cast<unsigned>(us);
}
// local25b diagnostic (user 2026-11-10 "진행~전략턴 전부 고쳐"): would a same-day result cache be exact? Per game day each
// fast search is keyed by its inputs (start, goal, flags, the context fields the search reads) and its result chain is
// hashed. same = an earlier search of that day had the same key and the same result, diff = same key, other result.
// The top callers of "same" searches are logged with the frame of caller_now[1].
// local265 (performance proposal 4.4-6): only with [diagnostics] path_count != 0; its hashing sat inside the measured scope.
static bool dup_enabled;
struct DupCaller {unsigned frame,same,diff;};
static std::unordered_map<std::uint64_t,std::uint64_t> dup_seen;   // under day_lock
static DupCaller dup_callers[16];static unsigned dup_used,dup_calls,dup_same,dup_diff,dup2_same,dup2_diff;
// Second key without the unit/person pointers (ctx+0, ctx+8): could a table keyed by places and conditions only be exact?
static std::unordered_map<std::uint64_t,std::uint64_t> dup2_seen;
// local268 (Fable advice 6): per kind, and a third key = the second plus a hash of the 27D4140 keyed cell list the search
// reads (ctx+0x20 id): path_dup_kind (kind, same, diff) with the full key, path_dup_kind_list (kind, same, diff) with the
// place + list key, for kinds with at least 100 searches that day.
static std::unordered_map<std::uint64_t,std::uint64_t> dup3_seen;
struct DupKind {unsigned key,calls,same,diff,list_same,list_diff;};
static DupKind dup_kinds[16];static unsigned dup_kind_used;
static std::uint64_t dup_mix(std::uint64_t h,std::uint64_t v) {return (h^v)*0x100000001b3ull;}
static void dup_flush() {   // under day_lock
    if(dup_calls) {
        log_event("path_dup_day",dup_calls,dup_same,dup_diff);
        log_event("path_dup_place_day",static_cast<unsigned>(dup2_seen.size()),dup2_same,dup2_diff);
        for(unsigned i=0;i<dup_used;++i)if(dup_callers[i].same+dup_callers[i].diff>20)
            log_event("path_dup_caller",dup_callers[i].frame,dup_callers[i].same,dup_callers[i].diff);
        for(unsigned i=0;i<dup_kind_used;++i)if(dup_kinds[i].calls>=100) {
            log_event("path_dup_kind",dup_kinds[i].key,dup_kinds[i].same,dup_kinds[i].diff);
            log_event("path_dup_kind_list",dup_kinds[i].key,dup_kinds[i].list_same,dup_kinds[i].list_diff);
        }
    }
    dup_seen.clear();dup2_seen.clear();dup3_seen.clear();dup_used=dup_calls=dup_same=dup_diff=dup2_same=dup2_diff=0;dup_kind_used=0;
}
static void dup_note(Obj ctx,unsigned start,unsigned goal,unsigned flags,const Node *output) {   // under day_lock
    std::uint64_t key=0xcbf29ce484222325ull;
    for(std::uint64_t v:{static_cast<std::uint64_t>(start),static_cast<std::uint64_t>(goal),static_cast<std::uint64_t>(flags),
            reinterpret_cast<std::uint64_t>(pointer(ctx,0)),reinterpret_cast<std::uint64_t>(pointer(ctx,8)),
            static_cast<std::uint64_t>(dword(ctx,0x10)),reinterpret_cast<std::uint64_t>(pointer(ctx,0x20)),
            static_cast<std::uint64_t>(dword(ctx,0x28)),static_cast<std::uint64_t>(dword(ctx,0x2C))})key=dup_mix(key,v);
    std::uint64_t result=0x84222325ull;unsigned guard=0;
    for(const Node *n=output;n && guard<path_fast_core::cells;n=n->parent,++guard)
        result=dup_mix(dup_mix(result,n->cell),static_cast<std::uint32_t>(n->g));
    ++dup_calls;
    int kind=0;
    {std::uint64_t k2=0xcbf29ce484222325ull;
        for(std::uint64_t v:{static_cast<std::uint64_t>(start),static_cast<std::uint64_t>(goal),static_cast<std::uint64_t>(flags),
                static_cast<std::uint64_t>(dword(ctx,0x10)),reinterpret_cast<std::uint64_t>(pointer(ctx,0x20)),
                static_cast<std::uint64_t>(dword(ctx,0x28)),static_cast<std::uint64_t>(dword(ctx,0x2C))})k2=dup_mix(k2,v);
        auto j=dup2_seen.find(k2);
        if(j==dup2_seen.end()) {if(dup2_seen.size()<200000)dup2_seen.emplace(k2,result);}
        else if(j->second==result)++dup2_same;else {++dup2_diff;j->second=result;}
        // local268: place key + the keyed cell list (unlocked read, as 18C080 / 18C540 read it)
        const unsigned kind_key=(flags&0xFF)|(pointer(ctx,0)?0x100u:0u);
        unsigned slot=0;while(slot<dup_kind_used && dup_kinds[slot].key!=kind_key)++slot;
        if(slot==dup_kind_used && dup_kind_used<16)dup_kinds[dup_kind_used++]=DupKind{kind_key,0,0,0,0,0};
        DupKind *dk=slot<dup_kind_used?&dup_kinds[slot]:nullptr;
        if(dk)++dk->calls;
        std::uint64_t k3=k2;
        if(!(flags&0xC)) {
            Obj list_key=pointer(ctx,0x20);
            const unsigned id_key=list_key?dword(list_key,0):0x14000;
            Obj lists=pointer(image,GA(GA_27D4140));
            unsigned n=0;
            if(lists && id_key<dword(image,GA(GA_27D4170)))
                for(Obj item=pointer(lists,id_key*8);item && n<4096;item=pointer(item,8),++n)
                    k3=dup_mix(k3,reinterpret_cast<std::uint64_t>(pointer(item,0)));
            k3=dup_mix(k3,n);
        }
        auto j3=dup3_seen.find(k3);
        if(j3==dup3_seen.end()) {if(dup3_seen.size()<200000)dup3_seen.emplace(k3,result);}
        else if(j3->second==result) {if(dk)++dk->list_same;}
        else {if(dk)++dk->list_diff;j3->second=result;}
        auto it=dup_seen.find(key);
        if(it==dup_seen.end()) {if(dup_seen.size()<200000)dup_seen.emplace(key,result);}
        else if(it->second==result) {++dup_same;kind=1;}
        else {++dup_diff;kind=2;it->second=result;}
        if(dk && kind==1)++dk->same;
        if(dk && kind==2)++dk->diff;
        if(!kind)return;}
    const unsigned frame=caller_now[kCallerFrames-1];   // deepest captured frame: tells search/recruit (1F7AF0) from supply etc.
    for(unsigned i=0;i<dup_used;++i)if(dup_callers[i].frame==frame) {(kind==1?dup_callers[i].same:dup_callers[i].diff)++;return;}
    if(dup_used<16)dup_callers[dup_used++]=DupCaller{frame,kind==1?1u:0u,kind==2?1u:0u};
}
// local71 cost-only searches (user 2026-11-12): 9DA10 (CVariableGetMoveCostNode) keeps only the total cost of its search
// and has no unit, so no goal-specific rule applies. For that caller the heuristic is hex distance x the cheapest positive
// terrain cost instead of hex distance x 1: still a lower bound, so the minimum cost is the same while far fewer cells are
// expanded. A cell cheaper than that bound repeats the search with the original heuristic.
// [diagnostics] path_count=2 runs both searches, returns the original-heuristic one and logs path_focus_* per day.
static unsigned focus_check,focus_calls,focus_mismatch,focus_bad;static LONGLONG focus_ticks,focus_plain_ticks;
static unsigned bound_calls,bound_mismatch;static LONGLONG bound_ticks,bound_plain_ticks;
static void focus_flush() {   // under day_lock
    if(focus_calls) {
        log_event("path_focus_day",focus_calls,focus_mismatch,focus_bad);
        log_event("path_focus_time_day",microseconds(focus_ticks),microseconds(focus_plain_ticks),0);
    }
    focus_calls=focus_mismatch=focus_bad=0;focus_ticks=focus_plain_ticks=0;
    if(bound_calls) {
        log_event("path_bound_day",bound_calls,bound_mismatch,0);
        log_event("path_bound_time_day",microseconds(bound_ticks),microseconds(bound_plain_ticks),0);
    }
    bound_calls=bound_mismatch=0;bound_ticks=bound_plain_ticks=0;
}
static unsigned permille(unsigned long long part,unsigned long long whole) {return whole?static_cast<unsigned>(part*1000/whole):0;}
static void kind_flush() {   // under day_lock, at a day boundary
    if(kind_calls<kKindFlush)return;
    for(unsigned i=0;i<kind_used;++i) {
        const KindTotal &k=kinds[i];
        log_event("path_fast_kind",k.key|(k.slow<kKindRest?k.slow:kKindRest)<<16,k.calls,microseconds(k.ticks));
        log_event("path_fast_kind_cells",k.key,static_cast<unsigned>(k.visits/1000),permille(k.hits,k.visits));
        if(k.failed)log_event("path_fast_kind_fail",k.key,(k.failed<kKindRest?k.failed:kKindRest)|(k.closed<kKindRest?k.closed:kKindRest)<<16,
                              microseconds(k.failed_ticks));
        log_event("path_fast_kind_split",k.key|(k.rejected<kKindRest?k.rejected:kKindRest)<<16,
                  (k.probes<kKindRest?k.probes:kKindRest)|(k.probe_closed<kKindRest?k.probe_closed:kKindRest)<<16,microseconds(k.setup_ticks));
    }
    kind_used=kind_calls=0;
}
static void kind_add(const SearchInfo &info,LONGLONG elapsed) {   // under day_lock
    const unsigned key=(info.flags&0xFF)|(info.unit?0x100u:0u);
    unsigned k=0;while(k<kind_used && kinds[k].key!=key)++k;
    if(k==kind_used) {
        if(k<kKinds) {kinds[k]=KindTotal{};kinds[k].key=key;++kind_used;}
        else {k=kKinds-1;kinds[k].key=kKindRest;}   // more kinds than slots: the last slot gathers the rest
    }
    KindTotal &t=kinds[k];++t.calls;++kind_calls;t.ticks+=elapsed;t.visits+=info.visits;t.hits+=info.hits;
    const bool closed=info.region && info.region<=kStartRegionCap;
    if(!info.found) {++t.failed;t.failed_ticks+=elapsed;if(closed)++t.closed;}
    if(info.cells) {
        t.probes+=info.cells->probes;t.setup_ticks+=info.cells->setup_ticks;
        if(!info.found && !info.cells->began)++t.rejected;
        if(info.cells->probe_closed)++t.probe_closed;
    }
    const unsigned us=microseconds(elapsed);
    if(us<5000)return;
    ++t.slow;
    if(totals.slow_lines>=16)return;
    ++totals.slow_lines;
    log_event("path_fast_slow",info.start|info.goal<<16,us,key|(info.found?0x200u:0u)|(info.focus?0x400u:0u)|(closed?0x800u:0u)|
              (info.region<kKindRest>>4?info.region:kKindRest>>4)<<12);
    log_event("path_fast_slow2",(info.visits<kCellsField?info.visits:kCellsField)|permille(info.hits,info.visits)<<22,caller_now[1],caller_now[2]);
}
static void record(bool fast,bool compared,bool mismatch,bool dirty,std::uint64_t call,
                   LONGLONG elapsed=0,const Diagnostic *detail=nullptr,Node *buffer=nullptr,const SearchInfo *info=nullptr) {
    AcquireSRWLockExclusive(&day_lock);
    const int today=game_day();
    if(today!=totals.day) {
        if(totals.day!=-1) {
            // Logger day= is the boundary day; counters belong to the preceding observed day.
            log_event("path_fast_day",totals.fast,totals.delegated,0);
            if(verify)log_event("path_fast_verify_day",totals.compared,totals.mismatches,totals.multi);
            log_event("path_fast_time_day",totals.only_fast,microseconds(totals.only_ticks),totals.all_compared);
            if(verify && totals.trial_compared)
                log_event("path_fast_verify_trial",totals.trial_compared,totals.trial_mismatch,
                          (totals.trial_uniform<kKindRest?totals.trial_uniform:kKindRest)|(totals.trial_memo<kKindRest?totals.trial_memo:kKindRest)<<16);
            if(totals.drift_sampled)log_event("path_fast_drift_day",totals.drift_sampled,totals.drift_searches,totals.drift_cells);
            log_event("path_fast_book_day",microseconds(InterlockedExchange64(&book_capture,0)),microseconds(InterlockedExchange64(&book_record,0)),
                      totals.fast+totals.delegated);
            caller_flush(totals.fast+totals.delegated);
            dup_flush();focus_flush();kind_flush();
        }
        else {caller_used=0;dup_flush();focus_flush();}
        totals={};totals.day=today;
    }
    if(fast)++totals.fast;else ++totals.delegated;
    if(fast && !compared) {++totals.only_fast;totals.only_ticks+=elapsed;if(info)kind_add(*info,elapsed);}
    caller_add(elapsed);
    if(compared)++totals.all_compared;
    if(compared && !dirty)++totals.compared;
    if(compared && detail && detail->trial) {
        ++totals.trial_compared;if(mismatch)++totals.trial_mismatch;
        if(detail->trial&1)++totals.trial_uniform;
        if(detail->trial&8)++totals.trial_memo;
    }
    if(info && info->cells && info->cells->shadowed) {
        const SearchCells &c=*info->cells;
        ++totals.drift_sampled;
        if(c.drift_cells) {
            ++totals.drift_searches;totals.drift_cells+=c.drift_cells;
            if(totals.drift_lines<16) {
                ++totals.drift_lines;
                log_event("path_fast_drift",info->start|info->goal<<16,c.drift_cell|c.drift_bits<<16,static_cast<unsigned>(c.drift_old));
                log_event("path_fast_drift2",static_cast<unsigned>(c.drift_new),(info->flags&0xFF)|(info->unit?0x100u:0u),c.drift_cells);
            }
        }
    }
    if(mismatch && dirty)++totals.multi;
    if(mismatch && !dirty) {
        ++totals.mismatches;
        if(totals.lines<64 && detail) {
            ++totals.lines;log_event("path_fast_mismatch",static_cast<unsigned>(call),detail->difference.original_length,detail->difference.new_length);
        }
    }
    if(mismatch && detail && totals.details<16) {
        ++totals.details;
        log_event("path_fast_mm_key",detail->key,detail->trial,static_cast<unsigned>(call));
        path_fast_core::mismatch_detail(call,detail->start,detail->goal,detail->flags,detail->ctx10,detail->ctx28,detail->ctx2c,
            detail->valid,detail->force,detail->original,detail->actual,buffer,*storage,detail->difference,
            [](const char *name,auto a,auto b,auto c) {
                log_event(name,static_cast<unsigned>(a),static_cast<unsigned>(b),static_cast<unsigned>(c));
            },[](unsigned id,unsigned direction) {
                // Guarded 2A7720 is pure integer geometry; no terrain/aux callbacks for diagnostics.
                return fn<unsigned(__fastcall *)(unsigned,unsigned)>(GA(GA_2A9910))(id,direction);
            });
    }
    ReleaseSRWLockExclusive(&day_lock);
}
#endif
// local25 neighbour table (see Native::neighbour). Filled once on the first search by the original 2A9910; searches on
// other threads keep calling the original until the fill is complete and published.
static unsigned neighbour_table[path_fast_core::cells*6];
static volatile LONG neighbour_ready=0,neighbour_filling=0;
static void neighbour_fill() {
    if(neighbour_ready || InterlockedCompareExchange(&neighbour_filling,1,0)!=0)return;
    auto next=fn<unsigned(__fastcall *)(unsigned,unsigned)>(GA(GA_2A9910));
    for(unsigned id=0;id<path_fast_core::cells;++id)
        for(unsigned d=1;d<=6;++d)neighbour_table[id*6+d-1]=next(id,d);
    InterlockedExchange(&neighbour_ready,1);
}
struct Native {
    Obj ctx,army=nullptr,a0=nullptr,a8=nullptr;
    unsigned start=0,goal=0,flags=0,local_b0=0,c4=0,c8=1;
    int force=0,valid=0;
    float scale=1.0f;
    bool focus=false,bad=false;int boost=0;   // local71 cost-only heuristic (see focus_caller)
    Memo *seen=nullptr;unsigned seen_generation=0,last_id=0;bool hit=false;   // local72 per-search cell memo
    unsigned visits=0,hits=0;   // local265 diagnostics: neighbour cells reached / answered from the memo
    bool began=false;           // local266 diagnostics: prepare() accepted the search and begin() ran
    bool bounded=false;int limit=0;   // local72: 1861E0 rules (no key list / +0x28 / +0x2C checks) and its cost limit
    bool over(std::int32_t g) const {return bounded && g>limit;}
    // local268 trial (see trial_enabled; path_fast.h uniform / probe_after)
    bool trial=trial_enabled,no_uniform=false,broken=false;
    unsigned probe_count=0,probe_region=0;
    long long setup_ticks=0;   // diagnostics: prepare() + begin()
    bool uniform() const {return trial && !no_uniform && (flags&0xC) && !(flags&5);}
    void uniform_failed() {broken=true;}
    unsigned probe_after() const {
        if(!trial)return 0;
        const bool unit=pointer(ctx,0)!=nullptr;
        if(uniform())return unit?1024u:4096u;   // local266: 0x0108 failures were all shut in (461/461)
        return unit?0u:4096u;
    }
    void probed(unsigned region) {++probe_count;if(region)probe_region=region;}
#ifndef NO_RUNTIME_LOG
    Shadow *shadow=nullptr;unsigned shadow_generation=0;bool shadow_store=false,shadow_compare=false;
    unsigned drift_cells=0,drift_bits=0,drift_cell=0;std::int32_t drift_old=0,drift_new=0;
    void drift(unsigned id,unsigned bits,std::int32_t before,std::int32_t now) {
        if(!drift_cells++) {drift_cell=id;drift_old=before;drift_new=now;}
        drift_bits|=bits;
    }
    void shadow_allowed(unsigned id,bool ok,std::int32_t cost) {
        if(shadow->stamp[id]!=shadow_generation) {
            shadow->stamp[id]=shadow_generation;shadow->state[id]=ok?1:0;shadow->cost[id]=cost;shadow_store=ok;return;
        }
        if(((shadow->state[id]&1)!=0)!=ok)drift(id,1,shadow->state[id]&1,ok?1:0);
        else if(ok && shadow->cost[id]!=cost)drift(id,2,shadow->cost[id],cost);
        shadow_compare=ok && (shadow->state[id]&2);
    }
    void shadow_adjust(unsigned id,std::int32_t out) {
        if(shadow_store) {shadow->out[id]=out;shadow->state[id]|=2;}
        else if(shadow_compare && shadow->out[id]!=out)drift(id,4,shadow->out[id],out);
        shadow_store=shadow_compare=false;
    }
#endif
    Obj aux[2]={};
    // local25b: the native callbacks of the per-cell loop, resolved once per search instead of per call (same targets).
    Obj world_root=nullptr;
    int (__fastcall *f_18B410)(Obj,Obj,int,unsigned,unsigned,const unsigned *,int)=nullptr;
    int (__fastcall *f_2CC630)(int,unsigned)=nullptr;
    Obj (__fastcall *f_2315A0)(Obj)=nullptr;
    Unary f_331780=nullptr,f_233FF0=nullptr,f_234570=nullptr;
    Obj (__fastcall *f_EDF0)(Obj)=nullptr;
    int (__fastcall *f_2C8C00)(unsigned,int)=nullptr;
    int (__fastcall *f_2C8D00)(unsigned)=nullptr;
    int (__fastcall *f_188D50)(Obj,Obj)=nullptr;
    int (__fastcall *f_187880)(int,unsigned,Obj,Obj,Obj,Obj,Obj *)=nullptr;
    int (__fastcall *f_2ACB80)(unsigned,unsigned)=nullptr;
    int (__fastcall *f_2B0A20)(Obj,int,unsigned)=nullptr;
    explicit Native(Obj context):ctx(context) {
        world_root=world();
        f_18B410=fn<decltype(f_18B410)>(GA(GA_18B410));f_2CC630=fn<decltype(f_2CC630)>(GA(GA_2CC630));
        f_2315A0=fn<decltype(f_2315A0)>(GA(GA_2315A0));f_331780=fn<Unary>(GA(GA_331780));
        f_233FF0=fn<Unary>(GA(GA_233FF0));f_234570=fn<Unary>(GA(GA_234570));f_EDF0=fn<decltype(f_EDF0)>(GA(GA_EDF0));
        f_2C8C00=fn<decltype(f_2C8C00)>(GA(GA_2C8C00));f_2C8D00=fn<decltype(f_2C8D00)>(GA(GA_2C8D00));
        f_188D50=fn<decltype(f_188D50)>(GA(GA_188D50));f_187880=fn<decltype(f_187880)>(GA(GA_187880));
        f_2ACB80=fn<decltype(f_2ACB80)>(GA(GA_2ACB80));f_2B0A20=fn<decltype(f_2B0A20)>(GA(GA_2B0A20));
    }
    bool prepare(Node *nodes,unsigned from,unsigned to,unsigned bits) {
#ifndef NO_RUNTIME_LOG
        const LONGLONG t0=diagnostic_logging?ticks():0;
        struct Setup {Native &n;LONGLONG t0;~Setup() {if(t0)n.setup_ticks+=ticks()-t0;}} setup{*this,t0};
#endif
        start=from;goal=to;flags=bits;
        // local267: searches without a unit; local268 trial: also unit searches without flag-1 terms (see the Memo comment)
        if(memo && (!pointer(ctx,0) || (trial && (flags&0xC) && !(flags&0x11)))) {
            seen=memo;
            if(!++seen->generation) {memset(seen->stamp,0,sizeof(seen->stamp));seen->generation=1;}
            seen_generation=seen->generation;
#ifndef NO_RUNTIME_LOG
            shadow=nullptr;
#endif
        }
#ifndef NO_RUNTIME_LOG
        else if(shadow) {
            if(!++shadow->generation) {memset(shadow->stamp,0,sizeof(shadow->stamp));shadow->generation=1;}
            shadow_generation=shadow->generation;
        }
#endif
        int x,y;
        fn<void(__fastcall *)(unsigned,int *,int *)>(GA(GA_2B11B0))(start,&x,&y);
        army=pointer(ctx,0);
        valid=fn<Unary>(GA(GA_331780))(army);
        if(!valid) {
            Obj other=pointer(ctx,8);
            if(fn<Unary>(GA(GA_331780))(other)) {
                other=pointer(ctx,8); // 183BB7 reload after the validity callback
                force=(*reinterpret_cast<int(__fastcall **)(Obj)>(*reinterpret_cast<Obj *>(other)+0x60))(other);
            }
        } else {
            force=fn<Unary>(GA(GA_22E690))(pointer(ctx,0));
            army=pointer(ctx,0); // 183ACC reload, preserving callback side effects
            local_b0=army[0x10]==2;
            const unsigned first=army[0x1B],second=army[0x1C];
            a0=pointer(world(),0x77428+(first<0x15?first:0)*8);
            a8=pointer(world(),0x77428+(second<0x15?second:0)*8);
            if((force!=0x33 && static_cast<unsigned>(force)-0x2E>4) || fn<Unary>(GA(GA_2340D0))(army)) {
                army=pointer(ctx,0); // 183B42
                const unsigned officer=word(army,0x12);
                Obj person=pointer(world(),officer<0x1771?0x158+officer*8:0x73A50);
                const unsigned county=word(army,0x2A);
                Obj region=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(world(),word(person,0xE2));
                c4=county==static_cast<unsigned>(fn<Unary>(GA(GA_22F190))(region));
            }
            if(fn<int(__fastcall *)(Obj,int)>(GA(GA_2C5B00))(pointer(ctx,0),0x66))c8=0;
        }
        if(dword(ctx,0x10))local_b0=1;
        Node *root=nullptr;
        return fn<int(__fastcall *)(unsigned,int,unsigned,unsigned,const unsigned *,int,Node *,Node **)>(GA(GA_18C330))
            (flags,force,local_b0,start,&goal,1,nodes,&root)!=0;
    }
    void begin() {
#ifndef NO_RUNTIME_LOG
        const LONGLONG t0=diagnostic_logging?ticks():0;
        struct Setup {Native &n;LONGLONG t0;~Setup() {if(t0)n.setup_ticks+=ticks()-t0;}} setup{*this,t0};
#endif
        began=true;
        aux[0]=fn<Obj(__fastcall *)(Obj *)>(GA(GA_18F0D0))(aux);
        const int base=fn<Unary>(GA(GA_BF1B0))(pointer(world(),0x73870));
        const __m128 numerator=_mm_cvtepi32_ps(_mm_cvtsi32_si128(base));
        __m128 denominator=numerator;
        if((flags&5) && pointer(ctx,0)) {
            alignas(16) unsigned char object[32];
            fn<Obj(__fastcall *)(Obj)>(GA(GA_222C60))(object);
            fn<void(__fastcall *)(Obj,int)>(GA(GA_2F1930))(object,2);
            army=pointer(ctx,0); // 183C8A reload after constructor/setter callbacks
            const unsigned id=army[0x1B];
            Obj terrain_type=pointer(world(),0x77428+(id<0x15?id:0)*8);
            const float speed=movement(army,terrain_type,0,0,object);
            // COMISS numerator,speed; CMOVBE selects numerator (including NaN).
            if(!_mm_comile_ss(numerator,_mm_set_ss(speed)))denominator=_mm_set_ss(speed);
            fn<void(__fastcall *)(Obj)>(GA(GA_2239F0))(object);
        }
        scale=_mm_cvtss_f32(_mm_div_ss(numerator,denominator));
        if(focus && flags==1 && !pointer(ctx,0) && !dword(ctx,0x10) && !dword(ctx,0x28) && !dword(ctx,0x2C)) {
            // Cheapest positive cost 2B0A20 can return (its marker term only adds): BF1B0 over the terrain-kind table.
            for(unsigned kind=0;kind<0x1F;++kind) {
                Obj type=pointer(world(),0x73860+kind*8);if(!type)continue;
                const int low=fn<Unary>(GA(GA_BF1B0))(type);
                if(low>0 && (!boost || low<boost))boost=low;
            }
        }
    }
    // local25: 2A9910 is pure integer geometry (cell, direction -> cell or BD10). Its answers for every cell and the six
    // directions are read once from the original function itself and then served from this table (same values).
    unsigned neighbour(unsigned id,unsigned direction) {
        if(id<path_fast_core::cells && direction>=1 && direction<=6 && neighbour_ready)return neighbour_table[id*6+direction-1];
        return fn<unsigned(__fastcall *)(unsigned,unsigned)>(GA(GA_2A9910))(id,direction);
    }
    Obj tile(unsigned id) {
        last_id=id;hit=seen && id<path_fast_core::cells && id!=start && seen->stamp[id]==seen_generation;
#ifndef NO_RUNTIME_LOG
        ++visits;hits+=hit?1u:0u;
        shadow_store=shadow_compare=false;
#endif
        return pointer(world_root,0xE270+(id<path_fast_core::cells?id:0)*8);
    }
    int terrain(Obj cell) {return hit?0:f_2B0A20(cell,force,c8);}
    bool allowed(unsigned id,Obj cell,std::int32_t &cost) {
        if(hit) {
            if(seen->value[id]==memo_blocked)return false;
            cost=seen->value[id];return true;
        }
        const bool ok=allowed_raw(id,cell,cost);
        if(!ok && seen && id<path_fast_core::cells) {seen->stamp[id]=seen_generation;seen->value[id]=memo_blocked;}
#ifndef NO_RUNTIME_LOG
        if(shadow && id<path_fast_core::cells)shadow_allowed(id,ok,cost);
#endif
        return ok;
    }
    bool allowed_raw(unsigned id,Obj cell,std::int32_t &cost) {
        if(!(flags&0xC)) {
            if(!f_18B410(pointer(ctx,0),cell,force,flags&0x40,c4,&goal,1))return false;
            if(bounded) {
                // 1861E0: no key list, +0x28 or +0x2C checks; its local_b0 rule is never reached (bounded_cost requires 0).
            } else {
            if(local_b0 && f_2CC630(force,cell[0x14]))return false;
            Obj key=pointer(ctx,0x20);
            const unsigned id_key=key?dword(key,0):0x14000;
            Obj lists=pointer(image,GA(GA_27D4140));
            if(lists && id_key<dword(image,GA(GA_27D4170)))
                for(Obj item=pointer(lists,id_key*8);item;item=pointer(item,8))
                    if(pointer(item,0)==cell)return false;
            if(dword(ctx,0x28) && word(cell,0x16))return false;
            if(dword(ctx,0x2C) && cell[0x10]==7)cost=path_fast_core::add(cost,10000);
            }
            if(!(flags&0x40)) {
                Obj building=f_2315A0(cell);
                if(f_331780(building) && !f_233FF0(building) && !f_234570(building)) {
                    Obj type=f_EDF0(building);
                    if(type[0x2C]==2 || type[0x2C]==3)return false;
                }
            }
        } else if(!f_2C8C00(id,force))return false;
        if((flags&0x10) && id!=goal && f_2C8D00(id))return false;
        if((flags&0x21)==1 && valid && f_2C8D00(id))
            cost=path_fast_core::add(cost,f_188D50(cell,pointer(ctx,0)));
        return true;
    }
    int adjust(int cost,Obj cell) {
        if(hit)return cost;   // the kept value is already adjusted
        if(boost && cost<boost)bad=true;   // the bound would not hold: the caller repeats without it
        const int out=f_187880(cost,flags,cell,pointer(ctx,0),a0,a8,aux);
        if(seen && last_id<path_fast_core::cells && last_id!=start) {seen->stamp[last_id]=seen_generation;seen->value[last_id]=out;}
#ifndef NO_RUNTIME_LOG
        if(shadow && last_id<path_fast_core::cells && last_id!=start)shadow_adjust(last_id,out);
#endif
        return out;
    }
    void coordinate(unsigned) {}   // local25: 2B11B0 only writes id%220 / id/220 to its two outputs, which were unused
    int heuristic(unsigned id) {
        const int distance=f_2ACB80(id,start);
        if(boost)return distance*boost;
        return _mm_cvttss_si32(_mm_mul_ss(_mm_cvtepi32_ps(_mm_cvtsi32_si128(distance)),_mm_set_ss(scale)));
    }
    void success() {*reinterpret_cast<unsigned *>(image+debug_rva)=0;}
    void cleanup() {
        Obj root=aux[0],item=pointer(root,8);
        while(!item[0x19]) {
            fn<void(__fastcall *)(Obj *,Obj)>(GA(GA_33FA10))(aux,pointer(item,0x10));
            Obj next=pointer(item,0);
            fn<void(__fastcall *)(Obj)>(GA(GA_3F9170))(item);item=next;
        }
        root=aux[0];*reinterpret_cast<Obj *>(root+8)=root;
        *reinterpret_cast<Obj *>(root)=root;*reinterpret_cast<Obj *>(root+0x10)=root;aux[1]=nullptr;
        fn<void(__fastcall *)(Obj,int,unsigned)>(GA(GA_4EA20))(root,1,0x30);
    }
};
static int slot_of(const Node *buffer) {
    for(unsigned slot=0;slot<3;++slot)
        if(reinterpret_cast<const unsigned char *>(buffer)==image+GA(GA_1BBAB50)+slot*path_fast_core::node_bytes)return static_cast<int>(slot);
    return -1;
}
static bool known_slot(const Node *buffer) {return slot_of(buffer)>=0;}
static bool acquire_storage(const Node *buffer) {
    const int slot=slot_of(buffer);
    if(slot<0) {storage=nullptr;return false;}   // unknown buffer: no lock guarantee, use the original
    path_fast_core::Storage *s=slot_storage[slot];
    if(!s) {
        try {s=new path_fast_core::Storage;}catch(...) {storage=nullptr;return false;}
        auto *previous=static_cast<path_fast_core::Storage *>(InterlockedCompareExchangePointer(
            reinterpret_cast<PVOID volatile *>(&slot_storage[slot]),s,nullptr));
        if(previous) {delete s;s=previous;}
    }
    storage=s;
    Memo *m=slot_memo[slot];
    if(!m && memo_enabled) {
        try {m=new Memo;}catch(...) {m=nullptr;}
        if(m) {
            auto *previous=static_cast<Memo *>(InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile *>(&slot_memo[slot]),m,nullptr));
            if(previous) {delete m;m=previous;}
        }
    }
    memo=memo_enabled?m:nullptr;
#ifndef NO_RUNTIME_LOG
    shadow_slot=nullptr;
    if(trial_enabled && diagnostic_logging) {
        Shadow *h=slot_shadow[slot];
        if(!h) {
            try {h=new Shadow;}catch(...) {h=nullptr;}
            if(h) {
                auto *previous=static_cast<Shadow *>(InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile *>(&slot_shadow[slot]),h,nullptr));
                if(previous) {delete h;h=previous;}
            }
        }
        shadow_slot=h;
    }
#endif
    return true;
}
static Node *delegate(Obj ctx,unsigned start,const int *goals,int count,Node *buffer,int clear_bytes,unsigned flags) {
    // The only direct caller supplies these static slots. Unknown buffers retain
    // the native clear_bytes contract; never assume an arbitrary pointer owns 1.9MB.
    path_fast_core::clear_delegate(buffer,known_slot(buffer),skip_clear!=0);
    return original(ctx,start,goals,count,buffer,clear_bytes,flags);
}
inline void SearchCells::add(const Native &n) {
        visits+=n.visits;hits+=n.hits;probes+=n.probe_count;setup_ticks+=n.setup_ticks;began=n.began;broken=broken || n.broken;
        if(n.probe_region) {region=n.probe_region;probe_closed=true;}
#ifndef NO_RUNTIME_LOG
        if(n.shadow) {shadowed=true;drift_cells+=n.drift_cells;drift_bits|=n.drift_bits;
            if(n.drift_cells && !drift_cell) {drift_cell=n.drift_cell;drift_old=n.drift_old;drift_new=n.drift_new;}}
#endif
}
#ifndef NO_RUNTIME_LOG
// local266 diagnostics (user 2026-11-19): a search without a route spreads over everything reachable from the goal (about
// 20 ms for a whole-map failure, local265 path_fast_slow). Is the start shut in? After such a search the cells reachable from
// the start through allowed cells are counted, up to kStartRegionCap, with the same per-cell answers (mostly from the memo).
// The route exists exactly when start and goal share that region (the goal is never tested, the start is exempt), so a
// region closed within the cap means the search could have answered "no route" after that many cells. 0 = not measured,
// kStartRegionCap + 1 = still open at the cap. Time and LastError are kept out of the search's own figures.
// local268: the count is path_fast_core::closed_region (slot storage, no 48 KB vector per call); a search the probe already
// ended keeps the probe's answer.
static void measure_region(SearchCells *cells,Native &n,unsigned start,unsigned goal) {
    if(!cells || !diagnostic_logging || cells->probe_closed || !n.began || start>=path_fast_core::cells)return;
    const DWORD error=GetLastError();const LONGLONG t0=ticks();
    const unsigned region=path_fast_core::closed_region(*storage,start,goal,n.flags,n,kStartRegionCap);
    cells->region=region?region:kStartRegionCap+1;
    cells->region_ticks=ticks()-t0;SetLastError(error);
}
#endif
static Node *run_search(Obj ctx,unsigned start,unsigned goal,unsigned flags,Node *buffer,SearchCells *cells=nullptr,bool shadowed=false) {
#ifndef NO_RUNTIME_LOG
    if(focus_now && focus_check) {
        Native quick(ctx);quick.focus=true;
        const LONGLONG t0=ticks();
        Node *fast_result=path_fast_core::search(*storage,start,goal,flags,quick);
        const int quick_g=fast_result?fast_result->g:-1;
        const LONGLONG t1=ticks();
        Native plain(ctx);
        Node *result=path_fast_core::search(*storage,start,goal,flags,plain);
        const LONGLONG t2=ticks();
        const int plain_g=result?result->g:-1;
        if(cells) {cells->add(quick);cells->add(plain);}
        AcquireSRWLockExclusive(&day_lock);
        ++focus_calls;focus_ticks+=t1-t0;focus_plain_ticks+=t2-t1;
        if(quick.bad)++focus_bad;
        else if(quick_g!=plain_g && ++focus_mismatch<=16) {
            log_event("path_focus_mismatch",start,goal,static_cast<unsigned>(quick_g));
            log_event("path_focus_mismatch2",static_cast<unsigned>(plain_g),static_cast<unsigned>(quick.boost),0);
        }
        ReleaseSRWLockExclusive(&day_lock);
        return path_fast_core::materialize(buffer,result);
    }
#endif
    Native native(ctx);native.focus=focus_now;
#ifndef NO_RUNTIME_LOG
    if(shadowed)native.shadow=shadow_slot;
#else
    (void)shadowed;
#endif
    Node *result=path_fast_core::search(*storage,start,goal,flags,native);
    if(cells)cells->add(native);
    if(native.bad || native.broken) {
        // local71 bound broken, or local268 a uniform search met a step other than 10000: repeat the ordinary way
        Native plain(ctx);plain.no_uniform=true;result=path_fast_core::search(*storage,start,goal,flags,plain);
        if(cells)cells->add(plain);
#ifndef NO_RUNTIME_LOG
        if(!result)measure_region(cells,plain,start,goal);
#endif
    }
#ifndef NO_RUNTIME_LOG
    else if(!result)measure_region(cells,native,start,goal);
#endif
    return path_fast_core::materialize(buffer,result);
}
static Node *path_body(Obj ctx,unsigned start,const int *goals,int count,Node *buffer,int clear_bytes,unsigned flags) {
    const DWORD incoming_error=GetLastError();
#ifndef NO_RUNTIME_LOG
    const auto call=static_cast<std::uint64_t>(InterlockedIncrement64(&calls));
#endif
    if(count!=1 || !buffer || clear_bytes!=0xBD10 || !ctx || !goals || !path_fast_core::eligible(count,buffer,clear_bytes,dword(image,debug_rva),
            start,goals?static_cast<unsigned>(*goals):path_fast_core::cells,busy) || !acquire_storage(buffer)) {
        SetLastError(incoming_error);
        Node *output=delegate(ctx,start,goals,count,buffer,clear_bytes,flags);
        const DWORD error=GetLastError();
#ifndef NO_RUNTIME_LOG
        record(false,false,false,false,call);
#endif
        SetLastError(error);return output;
    }
    busy=true;
    neighbour_fill();
    Node *output=nullptr;
#ifndef NO_RUNTIME_LOG
    bool compared=false,mismatch=false,dirty=false;
    Diagnostic detail;
    LONGLONG elapsed=0;
    SearchCells cells;
#endif
    __try {
#ifndef NO_RUNTIME_LOG
        if(path_fast_core::verify_call(verify,call)) {
            detail.start=start;detail.goal=static_cast<unsigned>(*goals);detail.flags=flags;
            detail.ctx10=dword(ctx,0x10);detail.ctx28=dword(ctx,0x28);detail.ctx2c=dword(ctx,0x2C);
            if(verify==1)dirty=!path_fast_core::clean(buffer);
            else {
                // Sampled comparison shares the new clean-per-segment meaning:
                // clear ALL nodes before the original, even on later waypoints.
                // Thus every sampled segment is an exact comparison, never "multi".
                memset(buffer,0,path_fast_core::node_bytes);
            }
            SetLastError(incoming_error);
            output=original(ctx,start,goals,count,buffer,clear_bytes,flags);
            const DWORD native_error=GetLastError();
            Native native(ctx);
            Node *actual=path_fast_core::search(*storage,start,detail.goal,flags,native);
            if(native.broken) {Native plain(ctx);plain.no_uniform=true;actual=path_fast_core::search(*storage,start,detail.goal,flags,plain);}
            detail.trial=(native.uniform()?1u:0u)|(native.probe_count?2u:0u)|(native.probe_region?4u:0u)|(native.seen && pointer(ctx,0)?8u:0u)|
                         (native.broken?16u:0u);
            {Obj key=pointer(ctx,0x20);detail.key=key?dword(key,0):0x14000;}
            detail.original=output;detail.actual=actual;detail.valid=native.valid;detail.force=native.force;
            detail.difference=path_fast_core::compare(output,actual);
            mismatch=detail.difference.mismatch;compared=true;
            // N=1 always returns the original; samples publish new chains only on equality.
            output=path_fast_core::verified_result(verify,mismatch,output,actual,buffer);
            SetLastError(native_error);
        } else
#endif
        {
#ifndef NO_RUNTIME_LOG
            const LONGLONG begin=ticks();
#endif
            SetLastError(incoming_error);
#ifndef NO_RUNTIME_LOG
            // local268 shadow sample: one in eight unit flag-1 searches (the memo stays off for them)
            const bool shadowed=shadow_slot && diagnostic_logging && (call&7)==5 && (flags&1) && pointer(ctx,0);
            output=run_search(ctx,start,static_cast<unsigned>(*goals),flags,buffer,&cells,shadowed);
#else
            output=run_search(ctx,start,static_cast<unsigned>(*goals),flags,buffer);
#endif
#ifndef NO_RUNTIME_LOG
            const DWORD result_error=GetLastError();
            elapsed=ticks()-begin-cells.region_ticks;   // local266: the start-region count is not part of the search
            SetLastError(result_error);
#endif
        }
#ifndef NO_RUNTIME_LOG
        const DWORD result_error=GetLastError();
        const LONGLONG book0=ticks();
        const SearchInfo info{start,static_cast<unsigned>(*goals),flags,pointer(ctx,0)!=nullptr,output!=nullptr,focus_now,cells.visits,cells.hits,cells.region,&cells};
        record(true,compared,mismatch,dirty,call,elapsed,&detail,buffer,&info);
        if(!compared && diagnostic_logging && dup_enabled) {AcquireSRWLockExclusive(&day_lock);dup_note(ctx,start,static_cast<unsigned>(*goals),flags,output);ReleaseSRWLockExclusive(&day_lock);}
        InterlockedExchangeAdd64(&book_record,ticks()-book0);
        SetLastError(result_error);
#endif
    } __finally {busy=false;}
    const DWORD error=GetLastError();
    SetLastError(error);return output;
}
static Node *__fastcall path(Obj ctx,unsigned start,const int *goals,int count,Node *buffer,int clear_bytes,unsigned flags) { COST_SCOPE_HOT("path");
    const DWORD error=GetLastError();
#ifndef NO_RUNTIME_LOG
    if(diagnostic_logging) {const LONGLONG c0=ticks();caller_capture();InterlockedExchangeAdd64(&book_capture,ticks()-c0);}
    else caller_now[0]=0;
#endif
    COST_SCOPE("path_fast");
    focus_now=focus_enabled && flags==1 && count==1 && ctx && !pointer(ctx,0) && focus_caller();
    SetLastError(error);return path_body(ctx,start,goals,count,buffer,clear_bytes,flags);
}
// local72 cost limit queries (user 2026-11-12): conditions 43/55 (AF520, AF830) call 186950 -> 1861E0, the original search
// with a cost limit, and read only "found" and the total cost. Without a unit and with +0x10 = 0 the answer is "minimum
// cost <= limit" and that cost, so the fast search with the cost-only heuristic and the same limit gives the same pair.
// The four CALLs are redirected; anything else goes to the original. Follows [performance] path_fast.
// [diagnostics] path_count=2 also runs the original, returns its answer and logs path_bound_* per day.
using Bounded=int(__fastcall *)(Obj,Obj,unsigned,const int *,int,unsigned,unsigned,int);
static Node *bounded_search(Obj ctx,unsigned start,unsigned goal,int limit) {
    Native native(ctx);native.focus=true;native.bounded=true;native.limit=limit;
    Node *result=path_fast_core::search(*storage,start,goal,1,native);
    if(native.bad) {Native plain(ctx);plain.bounded=true;plain.limit=limit;result=path_fast_core::search(*storage,start,goal,1,plain);}
    return result;
}
static int __fastcall bounded_cost(Obj result,Obj ctx,unsigned start,const int *goals,int count,unsigned flags,unsigned slot,int limit) {
    auto native_call=fn<Bounded>(GA(GA_186950));
    Obj section=slot==2?pointer(image,GA(GA_1BB6D58)+slot*8):nullptr;
    if(!active || !bounded_enabled || !section || count!=1 || flags!=1 || !result || !ctx || !goals || pointer(ctx,0) || dword(ctx,0x10) ||
       start>=path_fast_core::cells || static_cast<unsigned>(*goals)>=path_fast_core::cells || busy)
        return native_call(result,ctx,start,goals,count,flags,slot,limit);
    const DWORD error=GetLastError();
    auto lock=reinterpret_cast<LPCRITICAL_SECTION>(section+0x10);
    Node *buffer=reinterpret_cast<Node *>(image+GA(GA_1BBAB50)+slot*path_fast_core::node_bytes);
    bool found=false,done=false;int cost=0;unsigned cells_out=0;
#ifndef NO_RUNTIME_LOG
    const LONGLONG t0=ticks();
#endif
    EnterCriticalSection(lock);
    __try {
        if(acquire_storage(buffer)) {
            busy=true;
            neighbour_fill();
            Node *node=bounded_search(ctx,start,static_cast<unsigned>(*goals),limit);
            auto list=reinterpret_cast<unsigned *>(image+GA(GA_1BB6D70)+slot*0x14A0);
            memset(list,0,0x14A0);
            *reinterpret_cast<Obj *>(result)=reinterpret_cast<Obj>(list);*reinterpret_cast<unsigned long long *>(result+8)=0;
            if(node) {
                found=true;cost=node->g;
                for(const Node *n=node;n && cells_out<0x14A0/4;n=n->parent)list[cells_out++]=n->cell;
                *reinterpret_cast<int *>(result+8)=static_cast<int>(cells_out);*reinterpret_cast<int *>(result+0xC)=cost;
            }
            done=true;
        }
    } __finally {busy=false;LeaveCriticalSection(lock);}
    SetLastError(error);
    if(!done)return native_call(result,ctx,start,goals,count,flags,slot,limit);
#ifndef NO_RUNTIME_LOG
    if(focus_check) {
        const LONGLONG t1=ticks();
        const int native_found=native_call(result,ctx,start,goals,count,flags,slot,limit);
        const LONGLONG t2=ticks();
        const int native_cost=*reinterpret_cast<int *>(result+0xC);
        AcquireSRWLockExclusive(&day_lock);
        ++bound_calls;bound_ticks+=t1-t0;bound_plain_ticks+=t2-t1;
        if(((native_found!=0)!=found || (found && native_cost!=cost)) && ++bound_mismatch<=16) {
            log_event("path_bound_mismatch",start,static_cast<unsigned>(*goals),static_cast<unsigned>(limit));
            log_event("path_bound_mismatch2",static_cast<unsigned>(cost),static_cast<unsigned>(native_cost),(found?1u:0u)|(native_found?2u:0u));
        }
        ReleaseSRWLockExclusive(&day_lock);
        return native_found;
    }
#endif
    return found?1:0;
}
static CallLink bounded_links[4];
static void bounded_install() {
    if(!bounded_enabled)return;
    const unsigned sites[4]={GA(GA_AF70A),GA(GA_AF7D5),GA(GA_AFA45),GA(GA_AFB11)},target=GA(GA_186950);
    bool ok=target && GA(GA_1BB6D58) && GA(GA_1BB6D70) && GA(GA_1BBAB50);
    for(unsigned site:sites) {
        int delta=0;if(ok && site)memcpy(&delta,image+site+1,4);
        ok=ok && site && image[site]==0xE8 && site+5+static_cast<unsigned>(delta)==target;
    }
    Obj relay=ok?allocate_near(image+sites[0]):nullptr;
    if(!relay) {bounded_enabled=false;log_event("path_bound_rejected",ok?1u:0u,0,0);return;}
    absolute_jump(relay,reinterpret_cast<void *>(&bounded_cost));
    DWORD old;unsigned n=0;
    ok=VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) && FlushInstructionCache(GetCurrentProcess(),relay,16);
    for(;ok && n<4;++n)ok=call_prepare(bounded_links[n],sites[n],relay);
    for(unsigned i=0;ok && i<4;++i)ok=call_publish(bounded_links[i]);
    if(!ok) {for(unsigned i=0;i<4;++i)call_restore(bounded_links[i]);bounded_enabled=false;}
    log_event(ok?"path_bound_installed":"path_bound_rejected",2,n,0);
}
static bool prepare(Obj relay) {
    if(!guards_match())return false;
    absolute_jump(relay,reinterpret_cast<void *>(&path));
    // 183A30: MOV RAX,RSP / MOV [RAX+20h],R9D; same gate as path_count.
    memcpy(relay+32,image+entry,7);
    relay[39]=0x49;relay[40]=0xBB;Obj continuation=image+entry+7;memcpy(relay+41,&continuation,8);
    relay[49]=0x41;relay[50]=0xFF;relay[51]=0xE3;
    original=reinterpret_cast<Path>(relay+32);
    auto runtime=reinterpret_cast<RUNTIME_FUNCTION *>(relay+64);
    runtime->BeginAddress=32;runtime->EndAddress=52;runtime->UnwindData=80;
    const unsigned char unwind[]={1,7,0,0};memcpy(relay+80,unwind,sizeof(unwind));
    if(!prepare_link(link,entry,0xE9,relay))return false;
    memset(link.modified+5,0x90,2);
    // REP affects only RCX/RDI and memory. Both registers and RAX are killed
    // before their next read; TEST R12,R12 replaces flags at 18448E.
    const unsigned char nop[]={0x90,0x90};
    return prepare_bytes(clear_link,GA(GA_1857B5),nop,sizeof(nop));
}
static bool install() {
#ifndef NO_RUNTIME_LOG
    const int interval=dev_group()?mod_setting(L"diagnostics",L"path_fast_verify",0):0;
    verify=interval>0?static_cast<unsigned>(interval):0;
    LARGE_INTEGER q;if(!QueryPerformanceFrequency(&q) || q.QuadPart<=0)return false;frequency=q.QuadPart;
    caller_ranges();
    focus_check=dev_group() && mod_setting(L"diagnostics",L"path_count",0)==2?1u:0u;
    dup_enabled=dev_group() && mod_setting(L"diagnostics",L"path_count",0)!=0;
    trial_enabled=verify!=0;   // local268 trial: only with sampled comparison (see trial_enabled)
#endif
    Obj relay=allocate_near(image+entry);if(!relay)return false;
    DWORD old;
    if(!prepare(relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,96)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    if(!RtlAddFunctionTable(reinterpret_cast<RUNTIME_FUNCTION *>(relay+64),1,reinterpret_cast<DWORD64>(relay))) {
        VirtualFree(relay,0,MEM_RELEASE);return false;
    }
    if(!publish_link(link)) {restore_link(link);return false;}
#ifndef NO_RUNTIME_LOG
    if(path_fast_core::elide_clear(verify))
#endif
    {
        InterlockedExchange(&skip_clear,1);
        if(!publish_link(clear_link)) {
            restore_link(clear_link);InterlockedExchange(&skip_clear,0);restore_link(link);return false;
        }
    }
    // local72: the three cost-only accelerations follow [performance] path_fast itself (no keys of their own).
    focus_enabled=GA(GA_9DAD7)!=0;memo_enabled=true;bounded_enabled=true;
    active=true;bounded_install();
    log_event("path_fast_installed",skip_clear?1:0,(focus_enabled?1u:0u)|(memo_enabled?2u:0u)|(bounded_enabled?4u:0u)|(trial_enabled?8u:0u));return true;
}
#endif
}
static bool path_fast_guards_match() {return path_fast::guards_match();}
