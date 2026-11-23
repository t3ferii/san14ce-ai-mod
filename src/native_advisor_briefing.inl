// Convenience DLL; original UI callbacks only. No save writes or remote calls.
#include "game_addresses.h"
#include "advisor_briefing_profile.h"
#include "advisor_terrain_bridge.h"
#include <d3d11.h>   // local17 briefing16: direct face texture
#include <psapi.h>   // GetModuleInformation -> K32GetModuleInformation (kernel32.lib)
#include "advisor_face_files.h"   // local224: portrait heads read from the original archive files (worker thread below)
#include "advisor_ambush_bridge.h"   // [game_flow] ambush (docs/AMBUSH.md): proposal + blue count, nullptr = off
#include "ic_gate.h"   // 2026-11-18: the 9332B0 text link waits for the integrated patch's IC check
namespace advisor_briefing {
static void ambush_tick(){if(advisor_bridge::ambush)advisor_bridge::ambush->tick();}
[[maybe_unused]] static unsigned ambush_badge(){return advisor_bridge::ambush?advisor_bridge::ambush->badge():0;}
[[maybe_unused]] static bool ambush_offer_pending(){return advisor_bridge::ambush && advisor_bridge::ambush->offer_pending();}
[[maybe_unused]] static void ambush_offer_shown(){if(advisor_bridge::ambush)advisor_bridge::ambush->offer_shown();}
static void ambush_answer(bool accept){if(advisor_bridge::ambush)advisor_bridge::ambush->answer(accept);}
static unsigned ambush_tickets(){return advisor_bridge::ambush && advisor_bridge::ambush->tickets?advisor_bridge::ambush->tickets():0;}
static void ambush_status(std::wstring &out){out.clear();if(advisor_bridge::ambush && advisor_bridge::ambush->status)advisor_bridge::ambush->status(out);}
static unsigned ambush_offers(){return advisor_bridge::ambush && advisor_bridge::ambush->offers?advisor_bridge::ambush->offers():0;}
static unsigned ambush_offer_kind(){return advisor_bridge::ambush && advisor_bridge::ambush->offer_kind?advisor_bridge::ambush->offer_kind():0;}
static bool scene_yield=false;
// local5: a failing guard reports which sub-guard it was (diagnostic group, one line per failure):
// advisor_guard_failed(part, index, rva): part 1 callee prologue (index = row of the prologue list), 2 hash row (index in guards[]),
// 3 vtable slot (index in slots[]), 4 phase call opcode, 5 phase call target.
static bool guard_failed(unsigned part,unsigned index,unsigned rva) { COST_SCOPE("advisor_briefing:guard_failed");
#ifndef NO_RUNTIME_LOG
    log_event("advisor_guard_failed",part,index,rva);
#else
    (void)part;(void)index;(void)rva;
#endif
    return false;
}
static bool guards_match() { COST_SCOPE("advisor_briefing:guards_match");
    struct Prologue {unsigned rva;const unsigned char *bytes;unsigned size;};
    const Prologue prologues[]={
        {GA(GA_1F1E90),message_prologue,sizeof(message_prologue)},{GA(GA_1F1DA0),narration_prologue,sizeof(narration_prologue)},
        {GA(GA_306A30),narration_prologue,sizeof(narration_prologue)},{GA(GA_1F2190),person_prologue,sizeof(person_prologue)},
        {GA(GA_844860),event_prologue,sizeof(event_prologue)},{GA(GA_41F8C0),scenery_prologue,sizeof(scenery_prologue)},
        {GA(GA_4541E0),picture_show_bytes,sizeof(picture_show_bytes)},{GA(GA_4421F0),picture_close_bytes,sizeof(picture_close_bytes)}};
    for(unsigned i=0;i<sizeof(prologues)/sizeof(prologues[0]);++i)
        if(ga::mcmp_at(image,prologues[i].rva,prologues[i].bytes,prologues[i].size))return guard_failed(1,i,prologues[i].rva);
    scene_yield=other_mod_present(L"SAN14_SceneSkip.dll") || ga::mcmp(image,GA_6C6B10,0,scene_prologue,sizeof(scene_prologue)) || ga::mcmp(image,GA_6C6DD0,0,scene_prologue,sizeof(scene_prologue));
    // 2026-11-18: our hooks normally go in before the IP SceneSkip DLL loads, so its module ini (peer rule) counts too.
#ifdef LAYOUT_ROUTED   // guard-only fixture bindings compile this unit without the Layout client
    if(!scene_yield)scene_yield=layout_client::yield_to_peer("scene_yield",GA(GA_6C6B10));
#endif
    unsigned row=0;
    for(const auto &g:guards) {
        const unsigned index=row++;
        if(scene_yield && (g.rva==GA(GA_6C6B10) || g.rva==GA(GA_6C6DD0)))continue;   // owned by the other mod's prologue hook
        ga::ObserverSet used;   // local11: shared observer tolerance (ga::hash_ok); feature code 48 = advisor_briefing gate id
        if(!ga::hash_ok(image,g.rva,g.size,g.hash,&used))return guard_failed(2,index,g.rva);
        for(unsigned k=0;k<used.count;++k)log_event("observer_hook_tolerated",48,used.rva[k]);
    }
    unsigned slot_index=0;
    for(const auto &s:slots) {
        if(pointer(image,s.rva)!=image+s.original)return guard_failed(3,slot_index,s.rva);
        ++slot_index;
    }
    if(image[GA(GA_813A6D)]!=0xE8)return guard_failed(4,0,GA(GA_813A6D));
    int phase_call=*reinterpret_cast<int *>(image+GA(GA_813A6E));
    if(phase_call!=static_cast<int>(GA(GA_818600)-GA(GA_813A72))) {
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
        const auto &owned=diplomacy_intel::links[6];
        if(!owned.published || owned.site!=GA(GA_813A6D) || owned.redirected!=phase_call)return guard_failed(5,0,GA(GA_813A6E));
#else
        return guard_failed(5,0,GA(GA_813A6E));
#endif
    }
    return true;
}
#if (defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)) || defined(ADVISOR_BRIEFING_FIXTURE)
using namespace advisor_briefing_core;
static Inbox inbox;
#ifndef NO_RUNTIME_LOG
static volatile LONG stored_event_bytes=sizeof(Event);
// Stable inspector ABI: nine rows of {calls, tick_ms, thread_id}, all 64-bit.
static volatile LONG64 hook_activity[9][3]{};
static void hook_called(unsigned hook) { COST_SCOPE("advisor_briefing:hook_called");
    InterlockedIncrement64(&hook_activity[hook][0]);
    InterlockedExchange64(&hook_activity[hook][1],GetTickCount64());
    InterlockedExchange64(&hook_activity[hook][2],GetCurrentThreadId());
}
// Paths: diplomatic, recruitment, passive. Reasons are first failed gates,
// preserving native short-circuit evaluation and original game-call counts.
enum WaitReason {wait_top,wait_waiting,wait_parent,wait_owner,wait_active,
    wait_collecting,wait_fallback,wait_available,wait_tutorial,wait_interaction};
static volatile LONG64 wait_failures[3][10]{},wait_false[3]{};
static volatile LONG64 wait_last_state,wait_last_rva,wait_last_top_equal;
static volatile LONG64 wait_zero_writes,wait_native_returns,wait_top_changed;
// Time wait: expired immediately, original relay. Context: frames, player, covered, closed.
static volatile LONG64 time_wait_counts[2]{},player_context_counts[4]{};
static bool wait_condition(unsigned path,WaitReason reason,bool value) { COST_SCOPE("advisor_briefing:wait_condition");
    if(!value)InterlockedIncrement64(&wait_failures[path][reason]);
    return value;
}
static bool wait_owner_matches(unsigned path,Obj owner,Obj parent) { COST_SCOPE("advisor_briefing:wait_owner_matches");
    // Preserve even the original nullptr==nullptr comparison for diplomacy.
    if(owner!=parent)InterlockedIncrement64(&wait_failures[path][parent?wait_owner:wait_parent]);
    return owner==parent;
}
#endif
#ifndef NO_RUNTIME_LOG
// Rows hud/log/advice/user; columns calls, cumulative us, maximum us, layout calls.
static volatile LONG64 frame_metrics[4][4]{},layout_move_calls;
static thread_local unsigned measured_frames;
struct FrameTiming {
    unsigned row,previous;LARGE_INTEGER start{};
    explicit FrameTiming(unsigned r):row(r),previous(measured_frames){measured_frames|=1u<<row;QueryPerformanceCounter(&start);}
    ~FrameTiming(){
        LARGE_INTEGER end{},frequency{};QueryPerformanceCounter(&end);QueryPerformanceFrequency(&frequency);
        LONG64 us=frequency.QuadPart?(end.QuadPart-start.QuadPart)*1000000/frequency.QuadPart:0;
        InterlockedIncrement64(&frame_metrics[row][0]);InterlockedAdd64(&frame_metrics[row][1],us);
        if(us>frame_metrics[row][2])InterlockedExchange64(&frame_metrics[row][2],us);
        measured_frames=previous;
    }
};
#endif
static void layout_position(Obj object,int x,int y) { COST_SCOPE("advisor_briefing:layout_position");
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&layout_move_calls);
    for(unsigned i=0;i<4;++i)if(measured_frames&(1u<<i))InterlockedIncrement64(&frame_metrics[i][3]);
#endif
    fn<void(__fastcall *)(Obj,int,int)>(GA(GA_876AD0))(object,x,y);
}
static unsigned page=0;
static bool open=false;
// local222 report view: the selected row of the open tab (~0u = pick the first new message), and the messages that were new when the
// report was opened ("kind|day|text"), so opening a tab - which marks it read - keeps their dots until the report is closed.
static unsigned selected=~0u,rendered_selected=~0u;
static std::vector<std::wstring> fresh_keys;
static std::wstring fresh_key(const Event &e){return std::to_wstring(static_cast<unsigned>(e.kind))+L"|"+std::to_wstring(e.day)+L"|"+e.text;}
// Tabs: one per kind, plus 전체 (every message) last when there are two kinds or more. page == cards is 전체.
static unsigned report_pages(size_t cards){return cards>1?static_cast<unsigned>(cards)+1:static_cast<unsigned>(cards);}
static Obj report_state,simple_state;
static std::vector<Obj> simple_states;
static bool report_deferred;
static bool report_headless;
static CallLink report_results_call;
static thread_local Obj replay_results;
static unsigned rendered_generation=~0u,rendered_page=~0u,rendered_log_generation=~0u;
static bool rendered_open=false;
static bool rendered_collecting=false;
static bool rendered_city_list=false;
static bool advice=false,rendered_advice=false;   // [군사 제안] page (local68)
// local99 수집 설정: bit = Topic. One value for every save ([advisor_briefing] collect in 기능설정.ini), changed on the
// [수집 설정] page of the report window. native_item: the latest notice was left to the original because its topic is
// off, so its pictures and waits are left alone too until the next notice arrives.
static volatile LONG collect_mask=static_cast<LONG>(kTopicAll),native_item=0;   // local220: 22 topics, saved with kTopicFormat
static volatile LONG collect_mask2=static_cast<LONG>(kTopicAll2);   // local273: topics kTopicFirst.. ([advisor_briefing] collect2)
static bool settings=false,rendered_settings=false;
static unsigned settings_revision=0,rendered_settings_revision=0;
static bool collects(Topic t) {   // local243: 일기토·등용 성공·포로 always
    const unsigned bit=static_cast<unsigned>(t);
    if(always_collected(t))return true;
    if(bit<kTopicFirst)return ((static_cast<unsigned>(collect_mask)>>bit)&1u)!=0;
    return bit<static_cast<unsigned>(Topic::Count) && ((static_cast<unsigned>(collect_mask2)>>(bit-kTopicFirst))&1u)!=0;
}
static bool topic_gate(Topic t){const bool on=collects(t);InterlockedExchange(&native_item,on?0:1);return on;}
static void collect_apply() {
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
    InterlockedExchange(&scenery_collect_live,collects(Topic::Scenery)?1:0);
#endif
}
static void collect_load() {
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
    wchar_t path[MAX_PATH];
    if(mod_ini_path(path,L"기능설정.ini")) {
        InterlockedExchange(&collect_mask,static_cast<LONG>(collect_from_ini(static_cast<unsigned>(GetPrivateProfileIntW(L"advisor_briefing",L"collect",static_cast<INT>(collect_to_ini(kTopicAll)),path)))));
        // local273: no collect2 yet (0 = no format mark) = each new topic follows the topic its notices were filed under before
        InterlockedExchange(&collect_mask2,static_cast<LONG>(collect2_from_ini(static_cast<unsigned>(GetPrivateProfileIntW(L"advisor_briefing",L"collect2",0,path)),static_cast<unsigned>(collect_mask))));
    }
#endif
    collect_apply();
}
// local243: the [수집 설정] list = topic_order without the always-collected topics (일기토·등용 성공·포로).
static std::vector<Topic> settable_topics(){std::vector<Topic> out;for(Topic each:topic_order)if(!always_collected(each))out.push_back(each);return out;}
// index = the row of the [수집 설정] list (settable_topics), not the bit.
static void collect_toggle(unsigned index) {
    const std::vector<Topic> listed=settable_topics();
    if(index>=listed.size())return;
    const unsigned bit=static_cast<unsigned>(listed[index]);
    if(bit<kTopicFirst)InterlockedExchange(&collect_mask,static_cast<LONG>(static_cast<unsigned>(collect_mask)^(1u<<bit)));
    else InterlockedExchange(&collect_mask2,static_cast<LONG>(static_cast<unsigned>(collect_mask2)^(1u<<(bit-kTopicFirst))));
    ++settings_revision;collect_apply();
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
    wchar_t path[MAX_PATH],value[16],value2[16];
    if(mod_ini_path(path,L"기능설정.ini") && swprintf_s(value,L"%u",collect_to_ini(static_cast<unsigned>(collect_mask)))>0 &&
       swprintf_s(value2,L"%u",collect2_to_ini(static_cast<unsigned>(collect_mask2)))>0) {
        WritePrivateProfileStringW(L"advisor_briefing",L"collect",value,path);
        WritePrivateProfileStringW(L"advisor_briefing",L"collect2",value2,path);   // local273: written together, so a later load reads both
    }
    log_event("advisor_collect_changed",bit,static_cast<unsigned>(collect_mask)&kTopicAll,static_cast<unsigned>(collect_mask2)&kTopicAll2);   // army = topic bit
#endif
}
static std::vector<std::wstring> collect_labels() {
    std::vector<std::wstring> out;
    for(Topic each:settable_topics())out.push_back(std::wstring(topic_name(each))+(collects(each)?L"  ·  알림함":L"  ·  원작대로"));
    return out;
}
static bool offer=false,rendered_offer=false;   // ambush proposal shown in the panel instead of the report
static unsigned rendered_tickets=~0u,rendered_offers=~0u,rendered_offer_kind=~0u;static std::wstring rendered_note;
static Obj rendered_adviser;
static briefing_overlay::Model display_cache;
static Obj person_names_root;   // local222 row faces: the world the name index was built from (person_by_name)
static void release_row_faces();   // local223: forgets which faces were looked at (reset)
static int started_day=-1;
static Obj last_adviser;
static unsigned last_art=~0u;
static int portrait_serial=0;
struct Legacy {Obj object=nullptr;int x=0,y=0;bool moved=false;};
static Legacy old_log,old_advice;
// local232: the original log's lines (native_hud, drained from CMainLogLayout instead of its frame), shown in the report's 로그 tab.
// UI frame only, newest last, kept across cycles (the original log scrolls on too) and cleared by reset() and the report's 지우기.
static std::vector<Event> native_log;static unsigned log_generation;
static constexpr size_t kNativeLogKept=300;
static SRWLOCK native_log_lock=SRWLOCK_INIT;   // native_log, log_generation, native_log_glyphs: the log frame writes, the hud reads
static std::wstring native_log_glyphs;          // the new lines' characters, warmed by the hud's glyph worker
static void clear_native_log(){AcquireSRWLockExclusive(&native_log_lock);native_log.clear();native_log_glyphs.clear();++log_generation;ReleaseSRWLockExclusive(&native_log_lock);}
static Legacy speech_windows[2];
static Obj speech_owner;
static unsigned speech_serial;
static bool speech_waiting;
#ifndef NO_RUNTIME_LOG
static unsigned diagnostic_generation=~0u,last_top_rva;
static std::vector<unsigned> collected_top_rvas,uncollected_top_rvas;
static void diagnostic_cycle() { COST_SCOPE("advisor_briefing:diagnostic_cycle");
    if(diagnostic_generation==inbox.generation)return;
    diagnostic_generation=inbox.generation;last_top_rva=0;
    collected_top_rvas.clear();uncollected_top_rvas.clear();
}
static unsigned state_rva(Obj state) { COST_SCOPE("advisor_briefing:state_rva");
    auto table=state?reinterpret_cast<std::uintptr_t>(pointer(state,0)):0;
    auto base=reinterpret_cast<std::uintptr_t>(image);
    return table>=base && table-base<0x2A0B000?static_cast<unsigned>(table-base):0;
}
struct Stall {Obj state;unsigned path,rva;ULONGLONG started;};
static std::vector<Stall> stalls;
static unsigned long long stall_recent_ms[6]{},stall_max_ms[6]{};
// Paths: simple=0, person=1, narration=2, passive=3, recruitment=4, diplomatic=5.
static void stall_begin(Obj state,unsigned path) { COST_SCOPE("advisor_briefing:stall_begin");
    if(!state)return;
    for(const auto &s:stalls)if(s.state==state)return;
    stalls.push_back({state,path,state_rva(state),GetTickCount64()});
}
static void stall_end(Obj state) { COST_SCOPE("advisor_briefing:stall_end");
    for(auto it=stalls.begin();it!=stalls.end();++it)if(it->state==state) {
        auto ms=GetTickCount64()-it->started;stall_recent_ms[it->path]=ms;
        stall_max_ms[it->path]=(std::max)(stall_max_ms[it->path],ms);
        log_event("advisor_briefing_stall",it->path,static_cast<unsigned>(ms),it->rva);
        stalls.erase(it);return;
    }
}
static void collected_state(Obj state) { COST_SCOPE("advisor_briefing:collected_state");
    diagnostic_cycle();unsigned rva=state_rva(state);
    if(rva && std::find(collected_top_rvas.begin(),collected_top_rvas.end(),rva)==collected_top_rvas.end())collected_top_rvas.push_back(rva);
}
#endif
static Obj manager(){return fn<Obj(__fastcall *)()>(GA(GA_11040))();}
static Obj top_state() { COST_SCOPE("advisor_briefing:top_state");
    Obj m=manager();if(!m)return nullptr;
    const auto count=*reinterpret_cast<const unsigned long long *>(m+0x10);
    Obj states=pointer(m,0x20);return states && count && count<=256?pointer(states,static_cast<unsigned>((count-1)*8)):nullptr;
}
static bool interaction() { COST_SCOPE("advisor_briefing:interaction");
    // 56FE50 checks only the top state. A message can be above diplomacy, so
    // inspect the whole same native stack using its independently checked ABI.
    Obj m=manager();if(!m)return true;
    const auto count=*reinterpret_cast<const unsigned long long *>(m+0x10);
    Obj states=pointer(m,0x20);if(count>256 || (count && !states))return true;
    for(unsigned i=0;i<count;++i) {
        Obj state=pointer(states,i*8);if(!state)return true;
        const char *name=reinterpret_cast<const char *>(state+0x70);size_t size=0;
        for(;size<128 && name[size];++size){}
        if(size==128 || interactive_state(name,size))return true;
    }
    return false;
}
static bool clean_scene() { COST_SCOPE("advisor_briefing:clean_scene");
    if(inbox.fallback || interaction())return false;
    Obj m=manager(),states=m?pointer(m,0x20):nullptr;
    auto count=m?*reinterpret_cast<const unsigned long long *>(m+0x10):257;
    if(count>256 || (count && !states))return false;
    // Inspect only live stack entries, never an address retained after Leave.
    for(unsigned i=0;i<count;++i) {
        Obj state=pointer(states,i*8);if(!state)return false;
        Obj table=pointer(state,0);
        if(table==image+GA(GA_13A5F40) || table==image+GA(GA_13A5FD8) || table==image+GA(GA_1462A78)) {
            Obj dialog=pointer(state,0x470);if(dialog && dword(dialog,0x20)!=10000)return false;
        }
    }
    return true;
}
// 2026-11-07 user: after a player choice the rest of the diplomacy conversation follows the original flow. CTreatDiplomacyState (vtable 1460C48) is the parent;
// the player's accept/refuse is not a stacked YesNo state but the Decide window's own input (817200, phase 6: reads the window's choice fields, pushes the
// reply dialogs, then phase 9/12/13), so the choice is recognised by the player being the responder (window+190 == player force) or by a choice state
// stacked above the parent (diplomatic_choice_parent). Such a conversation is only observed (text/rows kept), never hidden, closed or auto-confirmed.
static Obj diplomatic_choice_parent;
enum DiplomacyScene {scene_ok,scene_no_parent,scene_choice,scene_phase,scene_decided};
// Parent CTreatDiplomacyState on the stack (nullptr: none); choice=true when a choice state is stacked with it.
static Obj diplomatic_stack(bool &choice) { COST_SCOPE("advisor_briefing:diplomatic_stack");
    choice=false;
    Obj m=manager();if(!m)return nullptr;
    const auto count=*reinterpret_cast<const unsigned long long *>(m+0x10);
    Obj states=pointer(m,0x20);if(!states || count>256)return nullptr;
    Obj parent=nullptr;
    for(unsigned i=0;i<count;++i) {
        Obj state=pointer(states,i*8);if(!state)return nullptr;
        const char *name=reinterpret_cast<const char *>(state+0x70);size_t n=0;for(;n<128 && name[n];++n){}
        if(n==128)return nullptr;
        std::string value(name,n);
        if(value.find("YesNo")!=std::string::npos || value.find("Select")!=std::string::npos || value.find("TreatPrisoner")!=std::string::npos || (value.find("Input")!=std::string::npos && pointer(state,0)!=image+GA(GA_14682F0)))choice=true;
        if(pointer(state,0)==image+GA(GA_1460C48))parent=state;
    }
    return parent;
}
static bool player_decided(Obj parent) { COST_SCOPE("advisor_briefing:player_decided");
    if(parent==diplomatic_choice_parent)return true;
    Obj context=world()?pointer(world(),0x871E8):nullptr;
    Obj player=context?fn<Obj(__fastcall *)(Obj)>(GA(GA_330B90))(context):nullptr;
    Obj layout=pointer(parent,0x4C0),window=layout?pointer(layout,0x188):nullptr;
    return player && window && pointer(window,0x190)==player;
}
// why: DiplomacyScene. The parent is returned for scene_ok (may be hidden/auto-confirmed) and scene_decided (observe only); nullptr otherwise.
static Obj diplomatic_scene(bool result_only,unsigned &why) { COST_SCOPE("advisor_briefing:diplomatic_scene");
    bool choice=false;Obj parent=diplomatic_stack(choice);
    if(!parent){diplomatic_choice_parent=nullptr;why=scene_no_parent;return nullptr;}
    if(choice){diplomatic_choice_parent=parent;why=scene_choice;return nullptr;}
    unsigned phase=dword(parent,0x4D8);
    if(!((phase>=12 && phase<=15) || (!result_only && (phase==8 || phase==11)))){why=scene_phase;return nullptr;}
    why=player_decided(parent)?scene_decided:scene_ok;return parent;
}
static Obj diplomatic_parent(bool result_only) { COST_SCOPE("advisor_briefing:diplomatic_parent");
    unsigned why=0;Obj parent=diplomatic_scene(result_only,why);return why==scene_ok?parent:nullptr;
}
static Obj diplomatic_observed(bool result_only) { COST_SCOPE("advisor_briefing:diplomatic_observed");
    unsigned why=0;Obj parent=diplomatic_scene(result_only,why);return why==scene_decided?parent:nullptr;
}
static unsigned force_id(){return ::player_force();}
// 2026-11-08 user: real estate development moved under the military report.
// Owned castles are city ids 1..51 whose native force matches the player; the same
// owner lookup is documented in native_campaign_reinforcements.inl (22E3E0 + vt+60).
static bool city_owned(Obj city,unsigned force) { COST_SCOPE("advisor_briefing:city_owned");
    Obj f=fn<Obj(__fastcall *)(Obj)>(GA(GA_22E3E0))(city);
    if(!valid(f))return false;
    auto id=*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(f,0)+0x60);
    return id(f)==force;
}
static bool copy(const wchar_t *s,std::wstring &out,size_t maximum);
static bool city_name(Obj city,std::wstring &out) { COST_SCOPE("advisor_briefing:city_name");
    unsigned hex=fn<unsigned short(__fastcall *)(Obj)>(GA(GA_22F190))(city);
    Obj tile=hex<48400?table(0xE270,hex,48399):nullptr;
    unsigned area=valid(tile)?word(tile,0x12):0;
    Obj place=area && area<=500?table(0x6CAF0,area,500):nullptr;
    return valid(place) && copy(reinterpret_cast<const wchar_t *>(place+0x10),out,100) && !out.empty();
}
static std::vector<unsigned> city_ids;
static std::vector<std::wstring> city_names;
static bool city_list=false;
static unsigned pending_city=0;
static ULONGLONG pending_city_at=0;
static ULONGLONG pending_city_started=0;
static std::wstring pending_city_name;
// Original camera focus used before a city domestic screen (893C00):
// 453570(centre_hex, 1, 500, -1.0, -1.0, 1).
// local235: the original log line click (671260) moves the camera the same way to the line's hex (entry +0x84); 48400 = none.
static void focus_hex(unsigned hex) {
    if(hex<48400)
        fn<unsigned long long(__fastcall *)(unsigned,int,int,double,double,int)>(GA(GA_453570))(hex,1,500,-1.0,-1.0,1);
}
static void focus_city(Obj city) { COST_SCOPE("advisor_briefing:focus_city");
    focus_hex(valid(city)?fn<unsigned short(__fastcall *)(Obj)>(GA(GA_22F190))(city):48400u);
}
// local240 (user 2026-11-19 "제 1군단을 "제"로 하지말고 세력군사로 표시해줘. 2군단 3군단은 군단장 초상화"): the person whose face a
// corps income row shows. Corps table W+0xE0D0 (data_locations.md): +0x11 order, +0x12 군단장 id. Order 1 (the lord's own corps) = the
// adviser of the HUD frame (last_adviser, as add_item); 0 = none known, the row keeps its initial.
static unsigned corps_face(Obj district) { COST_SCOPE("advisor_briefing:corps_face");
    if(!district)return 0;
    Obj root=world();if(!root)return 0;
    if(district[0x11]==1) {
        if(last_adviser)for(unsigned id=1;id<6001;++id)if(pointer(root,0x158+id*8)==last_adviser)return id;
        return 0;
    }
    const unsigned leader=word(district,0x12);
    return leader && leader<6001 && pointer(root,0x158+leader*8)?leader:0;
}
// local240: every city's name and centre hex (cities 1..51, the names the map shows), read on the UI frame thread when a report is built.
static std::vector<std::wstring> place_names;static std::vector<unsigned> place_hexes;static Obj place_root;
static void refresh_places() { COST_SCOPE("advisor_briefing:refresh_places");
#ifdef ADVISOR_BRIEFING_FIXTURE
    return;   // the fixture image has no city functions; rows then move the camera only with a record hex
#endif
    Obj root=world();if(!root || (root==place_root && !place_names.empty()))return;
    place_names.clear();place_hexes.clear();place_root=root;
    for(unsigned id=1;id<=51;++id) {
        Obj city=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(root,id);std::wstring name;
        if(!valid(city) || word(city,0x4E)!=id || !city_name(city,name))continue;
        const unsigned hex=fn<unsigned short(__fastcall *)(Obj)>(GA(GA_22F190))(city);
        if(hex<48400){place_names.push_back(name);place_hexes.push_back(hex);}
    }
    // local240 (user 2026-11-19 "재해나 날씨는 클릭해도 해당도시로 이동안하는건가?"): disaster and weather news name regions (여강, 소호 ...),
    // not only cities. Regions W+0x6CAF0 1..500: +0x10 name, +0x3A centre hex (ccz_bands 거점). A city keeps its own tile.
    for(unsigned a=1;a<=500;++a) {
        Obj area=table(0x6CAF0,a,500);std::wstring name;
        if(!area || !valid(area) || !copy(reinterpret_cast<const wchar_t *>(area+0x10),name,100) || name.empty())continue;
        if(std::find(place_names.begin(),place_names.end(),name)!=place_names.end())continue;
        const unsigned hex=word(area,0x3A);
        if(hex<48400){place_names.push_back(name);place_hexes.push_back(hex);}
    }
#ifndef NO_RUNTIME_LOG
    log_event("advisor_places",static_cast<unsigned>(place_names.size()),0,0);
#endif
}
// The place a report row moves the camera to: the message's own hex when the original record has one, else the first city its text names.
static unsigned row_place(const Event &e) {
    if(e.hex<48400)return e.hex;
    const int at=city_in_text(e.text,place_names);
    return at>=0?place_hexes[static_cast<size_t>(at)]:48400u;
}
static std::vector<unsigned> shown_log_hex;   // local235: the 로그 rows last built, top to bottom: each line's hex (48400 = none); empty off 로그
static void refresh_cities() { COST_SCOPE("advisor_briefing:refresh_cities");
    city_ids.clear();city_names.clear();
    unsigned force=force_id();if(!force)return;
    for(unsigned id=1;id<=51;++id) {
        Obj city=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(world(),id);
        if(!valid(city) || word(city,0x4E)!=id || !city_owned(city,force))continue;
        std::wstring name;if(!city_name(city,name))name=L"성 "+std::to_wstring(id);
        city_ids.push_back(id);city_names.push_back(name);
    }
}
// Direct original 지역내정 opener (the city menu's 내정 command, 689820),
// skipping the city menu. The advisor arm marks the city state on the first
// land click for the terrain picker.
static void open_city_domestic_direct(unsigned id) { COST_SCOPE("advisor_briefing:open_city_domestic_direct");
    Obj city=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(world(),id);
    if(!valid(city) || word(city,0x4E)!=id)return;
    Obj tile=fn<Obj(__fastcall *)(Obj)>(GA(GA_22EF50))(city);
    Obj area=valid(tile)?fn<Obj(__fastcall *)(Obj)>(GA(GA_22B8E0))(tile):nullptr;
    terrain_edit::advisor_city_pending=city;terrain_edit::advisor_area_pending=area;
    terrain_edit::advisor_city_armed_ms=GetTickCount64();
    struct Params {Obj city;void *second;};
    struct Functor {uint64_t slots[8];};
    alignas(16) static unsigned char completion[0x500]={};
    Functor f{};f.slots[0]=reinterpret_cast<uint64_t>(image+GA(GA_1453A78));
    f.slots[1]=reinterpret_cast<uint64_t>(completion);f.slots[7]=reinterpret_cast<uint64_t>(&f);
    Params p={city,nullptr};
    fn<void(__fastcall *)(Obj,const char *,Params *,Functor *)>(GA(GA_689820))(manager(),"CStrategyDomesticState",&p,&f);
#ifndef NO_RUNTIME_LOG
    log_event("terrain_advisor_direct",id,2,0);
#endif
}

static bool active(){return enabled && mod_features.advisor_briefing && world() && force_id();}
static bool tutorial() { COST_SCOPE("advisor_briefing:tutorial");
    Obj t=fn<Obj(__fastcall *)()>(GA(GA_571630))();
    Obj context=world()?pointer(world(),0x871E8):nullptr;
    // +454 is the remaining order count, not a tutorial flag (44A0F0).
    return (t && dword(t,0)) || !context;
}
static unsigned day() { COST_SCOPE("advisor_briefing:day");
    int current=game_day();return started_day>=0 && current>=started_day?
        static_cast<unsigned>((std::min)(10,current-started_day+1)):0;
}
static bool defer(bool final_report=false){return active() && (inbox.collecting || (final_report && inbox.has_cycle)) && !inbox.fallback && briefing_overlay::available() && !tutorial() && !interaction();}
static bool dialogue_ready(bool result_only){return active() && inbox.has_cycle && !inbox.fallback && briefing_overlay::available() && !tutorial() && diplomatic_parent(result_only);}
// Same gates, but the player decided in this conversation: its lines are stored for the briefing and the native dialogue is left alone.
static bool dialogue_observed(bool result_only){return active() && inbox.has_cycle && !inbox.fallback && briefing_overlay::available() && !tutorial() && diplomatic_observed(result_only);}
#ifndef NO_RUNTIME_LOG
// advisor_diplomacy_skip(path, code, phase): path 0 message page, 1 result speech, 2 input wait, 3 message page closed by simple_frame.
// code 0 hidden/auto-confirmed, 1 no parent state, 2 choice state stacked, 3 phase outside 8/11/12-15, 4 player decided (observed only), 9 another gate (feature/collecting/overlay/tutorial).
static void skip_decision(unsigned path,bool result_only,bool skipped) { COST_SCOPE("advisor_briefing:skip_decision");
    unsigned why=0;Obj parent=diplomatic_scene(result_only,why);
    log_event("advisor_diplomacy_skip",path,skipped?0:why==scene_ok?9:why,parent?dword(parent,0x4D8):0);
}
#endif
struct Picture {Obj object;int id;bool suppressed;};
static std::vector<Picture> pictures;
static CodeLink picture_links[2];
#ifndef NO_RUNTIME_LOG
// Columns suppressed/displayed; rows 0..77 are exact IDs, 78 is all other IDs.
static volatile LONG picture_count_slots=79,picture_last_id;
static volatile LONG64 picture_counts[2]{},picture_id_counts[79][2]{};
// Close columns: suppressed, displayed, no record. Cumulative for DLL lifetime.
static volatile LONG64 picture_close_counts[3]{};
#endif
// Defined with the scene hooks below (local220 native scene window).
static bool native_scene_active();
static void native_scene_pass(unsigned code);
static void __fastcall picture_show(Obj object,int id) { COST_SCOPE("advisor_briefing::picture_show"); COST_CRUMB("advisor_briefing_object",object);
    const bool in_scene=native_scene_active();   // local220: the pictures of a 원작대로 scene stay
    if(in_scene)native_scene_pass(4);
    bool suppressed=defer() && !native_item && !in_scene; // Includes !interaction(), on the native UI caller. local99: a notice left to the original keeps its picture.
    auto it=std::find_if(pictures.begin(),pictures.end(),[&](const Picture &p){return p.object==object;});
    if(it==pictures.end())pictures.push_back({object,id,suppressed});
    else *it={object,id,suppressed};
#ifndef NO_RUNTIME_LOG
    unsigned column=suppressed?0:1,bin=id>=0 && id<=77?static_cast<unsigned>(id):78;
    InterlockedIncrement64(&picture_counts[column]);InterlockedIncrement64(&picture_id_counts[bin][column]);
    InterlockedExchange(&picture_last_id,id);
#endif
    // Reproduce MOV RCX,[RCX+1B8]; XOR R8D,R8D; JMP 842A90 in C++.
    if(!suppressed)fn<void(__fastcall *)(Obj,int,int)>(GA(GA_844F70))(pointer(object,0x1B8),id,0);
}
static void __fastcall picture_close(Obj object) { COST_SCOPE("advisor_briefing::picture_close"); COST_CRUMB("advisor_briefing_object",object);
    auto it=std::find_if(pictures.begin(),pictures.end(),[&](const Picture &p){return p.object==object;});
    bool recorded=it!=pictures.end(),suppressed=recorded && it->suppressed;
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&picture_close_counts[suppressed?0:recorded?1:2]);
#endif
    if(recorded)pictures.erase(it); // Do not retain an iterator across a native call.
    // Match the most recent show, even if collecting/interaction changed since it.
    if(!suppressed)fn<void(__fastcall *)(Obj)>(GA(GA_837710))(pointer(object,0x1B8));
}
#ifndef NO_RUNTIME_LOG
static bool wait_ready_diagnostic(unsigned path,bool diplomatic=false) { COST_SCOPE("advisor_briefing:wait_ready_diagnostic");
    return wait_condition(path,wait_active,active()) &&
        wait_condition(path,wait_collecting,diplomatic?inbox.has_cycle:inbox.collecting) &&
        wait_condition(path,wait_fallback,!inbox.fallback) &&
        wait_condition(path,wait_available,briefing_overlay::available()) &&
        wait_condition(path,wait_tutorial,!tutorial()) &&
        (diplomatic?wait_condition(path,wait_parent,diplomatic_parent(true)!=nullptr):
            wait_condition(path,wait_interaction,!interaction()));
}
#endif
static bool raw_length(const wchar_t *s,size_t maximum,size_t &length) {
    if(!s)return false;
    __try {for(length=0;length<=maximum;++length)if(!s[length])return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    return false;
}
static bool copy(const wchar_t *s,std::wstring &out,size_t maximum=Inbox::text_limit) { COST_SCOPE("advisor_briefing:copy");
    size_t n=0;if(!raw_length(s,maximum,n))return false;out.assign(s,n);return true;
}
// User 2026-10-28: show the native bottom-left advice and log as in the
// original game; the bubble report stays. move() remains for restoring.
static bool hide_native_hud(){return false;}
static void move(Legacy &l,bool hide) { COST_SCOPE("advisor_briefing:move");
    if(!l.object)return;
    if(l.moved==hide) {
        // User 2026-10-28 live PID9808: the advice layout's own refresh put it
        // back at (0,0) while moved stayed true. Re-hide only on that reset;
        // two reads per frame, no per-frame 874650 call (local17 lag fix).
        if(hide && (dword(l.object,0x20)!=10000 || dword(l.object,0x24)!=10000))layout_position(l.object,10000,10000);
        return;
    }
    if(hide){l.x=static_cast<int>(dword(l.object,0x20));l.y=static_cast<int>(dword(l.object,0x24));}
    layout_position(l.object,hide?10000:l.x,hide?10000:l.y);l.moved=hide;
}
static void bind(Legacy &l,Obj p) { COST_SCOPE("advisor_briefing:bind");
    if(l.object==p)return;
    l={p,static_cast<int>(dword(p,0x20)),static_cast<int>(dword(p,0x24)),false};
}
static void restore_speech() { COST_SCOPE("advisor_briefing:restore_speech");
    bool choice=false;Obj parent=diplomatic_stack(choice);
    Obj layout=parent && parent==speech_owner?pointer(parent,0x478):nullptr;
    for(unsigned i=0;i<2;++i) {
        if(layout && pointer(layout,0x178+(i+3)*8)==speech_windows[i].object)move(speech_windows[i],false);
        speech_windows[i]={};
    }
    speech_owner=nullptr;speech_waiting=false;++speech_serial;
}
static bool replace_legacy(){return active() && !inbox.fallback && briefing_overlay::available() && !interaction();}
static void fallback() { COST_SCOPE("advisor_briefing:fallback");
    restore_speech();briefing_overlay::mask_report(false);
    inbox.fallback=true;open=false;move(old_log,false);move(old_advice,false);
    if(report_state) {
        for(unsigned offset:{0x480u,0x488u,0x490u}) {
            Obj p=pointer(report_state,offset);if(p)layout_position(p,0,0);
        }
    }
    log_event("advisor_briefing_native_fallback",static_cast<unsigned>(inbox.events.size()));
}
static void clear_scene_pending();
static void native_scene_forget();
static void reset() { COST_SCOPE("advisor_briefing:reset");
    briefing_overlay::forget_row_faces();release_row_faces();person_names_root=nullptr;   // local222: person ids of another world
    clear_scene_pending();native_scene_forget();
    restore_speech();briefing_overlay::mask_report(false);diplomatic_choice_parent=nullptr;
    move(old_log,false);move(old_advice,false);old_log={};old_advice={};
    clear_native_log();   // local232: the log of another world
    place_root=nullptr;place_names.clear();place_hexes.clear();shown_log_hex.clear();   // local240
    inbox.reset(0,0);open=false;city_list=false;pending_city=0;pending_city_name.clear();city_ids.clear();city_names.clear();page=0;report_state=simple_state=nullptr;simple_states.clear();pictures.clear();report_deferred=false;report_headless=false;
    last_adviser=nullptr;last_art=~0u;++portrait_serial;briefing_overlay::forget();
#ifndef NO_RUNTIME_LOG
    stalls.clear();
#endif
}
static void refresh_progress();
static bool store(std::vector<Event> batch,bool final_report=false) { COST_SCOPE("advisor_briefing:store");
#ifndef NO_RUNTIME_LOG
    if(stored_event_bytes!=static_cast<LONG>(sizeof(Event))){fallback();return false;}
#endif
    if(!inbox.append(std::move(batch),final_report)){fallback();return false;}
#ifndef NO_RUNTIME_LOG
    collected_state(top_state());
#endif
    refresh_progress();return true;
}
// PW36a (R8d §7-B): append one item to the current turn's briefing from another feature (auto reward).
// UI frame thread only (the reward_site path): the Inbox has no lock. Not during progress collection.
[[maybe_unused]] bool add_item(const std::wstring &text) { COST_SCOPE("advisor_briefing:add_item");
    if(!active())return false;
    if(text.empty() || text.size()>Inbox::text_limit)return false;
    if(inbox.collecting || !inbox.has_cycle || inbox.fallback)return false;
    Event e;
    e.kind=Kind::Reward;
    e.day=0;                                  // no "N일차 · " prefix in the card body
    e.text=text;
    // local229 (user 2026-11-18 "위임포상주는거 전에는 군사 초상화 떴는데 안뜨더라고"): the 군사 reports the award in the lord's stead, so
    // the row is his (report_row names a row after its speaker and takes the speaker's face). He is the person of the HUD's adviser
    // frame (layout +0x448, advice_frame), looked up in the person table; none bound yet = the row as before (topic name, no face).
    if(last_adviser)if(Obj root=world())for(unsigned id=1;id<6001;++id)if(pointer(root,0x158+id*8)==last_adviser){e.speaker=id;break;}
    std::vector<Event> batch;batch.push_back(std::move(e));
    return inbox.append(std::move(batch),/*final_report=*/true);   // ++generation -> overlay rebuilds
}
using MessageCall=void(__fastcall *)(const wchar_t *,int);
using NarrationCall=void(__fastcall *)(int,const wchar_t *);
static MessageCall original_message_call;
static NarrationCall original_narration_call;
static CodeLink message_links[3];
enum MessageReason {message_inactive,message_not_collecting,message_fallback,message_overlay,
    message_tutorial,message_interaction,message_native_mode,message_native_context,
    message_no_caller,message_bad_text,message_store_failed,message_owner,message_phase,message_params,message_fields,message_reason_count};
#ifndef NO_RUNTIME_LOG
// Rows: 1F0AD0, 1F09E0, 1F0DD0, 842380, 6C43B0, 6C4670, 41D550.
// Columns: captured, relayed. First failed gate only.
static volatile LONG message_entry_paths=10,message_reason_columns=message_reason_count;
static volatile LONG64 message_entry_counts[10][2]{},message_pass_reasons[10][message_reason_count]{};
#endif
static bool message_gate(unsigned path,MessageReason reason,bool value) { COST_SCOPE("advisor_briefing:message_gate");
#ifndef NO_RUNTIME_LOG
    if(!value)InterlockedIncrement64(&message_pass_reasons[path][reason]);
#else
    (void)path;(void)reason;
#endif
    return value;
}
static bool entry_ready(unsigned path) { COST_SCOPE("advisor_briefing:entry_ready");
    // Same gates/order as defer(); evaluate native predicates only on this UI caller.
    if(!message_gate(path,message_inactive,active()) ||
       !message_gate(path,message_not_collecting,inbox.collecting) ||
       !message_gate(path,message_fallback,!inbox.fallback) ||
       !message_gate(path,message_overlay,briefing_overlay::available()) ||
       !message_gate(path,message_tutorial,!tutorial()) ||
       !message_gate(path,message_interaction,!interaction()))return false;
    if(!message_gate(path,message_native_mode,fn<int(__fastcall *)()>(GA(GA_335110))()==0))return false;
    return true;
}
using TimeWait=void(__fastcall *)(unsigned,int *,unsigned);
static TimeWait original_time_wait;
static CodeLink time_wait_link;
// Defined with the recruitment scene hooks below; time_wait and the message hooks need them earlier.
static bool live_success_active();
static bool live_success_other_thread();
static void live_success_end(unsigned reason);
static void live_success_pass(unsigned code);
// 2026-11-01 user: duel (일기토) lines play as in the original but are still collected. 37ACF0 (duel end) clears
// CDuelManager(*2783730)+10, 448C10 tests it as "duel running"; pure read.
static bool duel_running(){Obj m=pointer(image,GA(GA_2786780));return m && dword(m,0x10)!=0;}
// local307 (user 「일기토 발생하면 군사보고좀 숨겨줘 거슬린다」): every hook below that runs on a game frame or the event thread publishes
// the duel flag to the overlay (hidden while it is set; briefing_overlay::set_duel). advisor_duel_hide (a change only): army 1 hidden /
// 0 shown again, value the hook that saw it (1 time wait, 2 message, 3 HUD frame, 4 progress, 5 event speech, 6 advice frame), hex
// the overlay's visible flag at that moment.
static void note_duel(unsigned hook) {
    const bool on=duel_running();
    if(!briefing_overlay::set_duel(on))return;
#ifndef NO_RUNTIME_LOG
    log_event("advisor_duel_hide",on?1u:0u,hook,briefing_overlay::is_visible()?1u:0u);
#else
    (void)hook;
#endif
}
static void __fastcall time_wait(unsigned milliseconds,int *result,unsigned allow_skip) { COST_SCOPE("time_wait");
    note_duel(1);
    if(native_scene_active()){native_scene_pass(1);original_time_wait(milliseconds,result,allow_skip);return;}   // local220: a 원작대로 scene waits as in the original
    const bool live=live_success_active();
    if(live)live_success_pass(1);   // the live success scene's own waits must run to completion
    bool expired=!live && result && !duel_running() && defer() && !native_item && fn<int(__fastcall *)()>(GA(GA_335110))()==0 &&
        fn<Obj(__fastcall *)(Obj)>(GA(GA_572480))(manager())!=nullptr;
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&time_wait_counts[expired?0:1]);
#endif
    // Native timeout=0; skip=1 changes disaster callers' later log/effect branches.
    if(expired){*result=0;return;}
    original_time_wait(milliseconds,result,allow_skip);
}
static void name_speaker(Event &e,Obj person) { COST_SCOPE("advisor_briefing:name_speaker");
    if(!person)return;
    e.speaker=word(person,0x10);std::wstring name;
    if(copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(person),name,100) && !name.empty())e.text=name+L"\n"+e.text;
}
#include "native_briefing_sources.inl"
// caller: the hook's return address (the original function that called the entry; native_briefing_sources.inl).
static bool capture_message(unsigned path,int speaker,const wchar_t *text,const void *caller) { COST_SCOPE("advisor_briefing:capture_message");
    note_duel(2);
    if(native_scene_active()){native_scene_pass(2);return false;}   // local220: a line of a 원작대로 scene (e.g. the scenery bonus 424B00 -> 1F1E90)
    // 2026-11-09 user (PW17/PW19), local17 briefing16: a plain message is not the scene's own dialogue; on the scene's own
    // thread it ends the window. Another thread's attempt passes natively: nothing is stored or taken over.
    if(live_success_active()) {
        if(live_success_other_thread()){live_success_pass(2);return false;}
        live_success_end(4);
    }
    if(!entry_ready(path))return false;
    Obj context=pointer(image,GA(GA_27872F0));
    if(!message_gate(path,message_native_context,!context || dword(context,0x1B0)==0) ||
       !message_gate(path,message_no_caller,fn<Obj(__fastcall *)(Obj)>(GA(GA_572480))(manager())!=nullptr))return false;
    Event e;e.speaker=static_cast<unsigned>(speaker);e.day=day();
    if(!message_gate(path,message_bad_text,copy(text,e.text) && !e.text.empty()))return false;
    // local273: the sending function decides the topic and whether the original window plays too; an unknown caller keeps the
    // wording (local99..252).
    const SourceHit source=briefing_source(caller);
    const SourceDecision decision=source_decision(source,e.text,duel_running());
    const Topic topic=decision.topic;const bool native=decision.native,no_copy=decision.no_copy;
#ifndef NO_RUNTIME_LOG
    // army = return address rva; value = topic | flags << 8 | path << 16 | known << 20 | native << 21 | no copy << 22; hex = worded topic
    log_event("advisor_topic_source",source.rva,static_cast<unsigned>(topic)|(static_cast<unsigned>(source.flags)<<8)|(path<<16)|
              (source.topic!=Topic::Count?1u<<20:0u)|(native?1u<<21:0u)|(no_copy?1u<<22:0u),static_cast<unsigned>(topic_of_text(e.text)));
#endif
    if(no_copy){InterlockedExchange(&native_item,1);return false;}   // 군량 부족: the original window only, nothing stored
    const bool capture_scene=mod_features.briefing16 && native;   // stored, and shown natively like a duel line
    if(!topic_gate(topic))return false;   // local99: topic switched off = the original window
    e.topic=static_cast<unsigned char>(topic);
    if(path==2 && speaker>=0 && speaker<6001)name_speaker(e,pointer(world(),0x158+static_cast<unsigned>(speaker)*8));
    if(!message_gate(path,message_store_failed,store({e})))return false;
    return !duel_running() && !capture_scene;   // stored; a duel or capture line is also shown natively
}
static void __fastcall message_call(const wchar_t *text,int mode) { COST_SCOPE("message_call");
    bool captured=capture_message(0,0,text,_ReturnAddress());
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[0][captured?0:1]);
#endif
    if(!captured)original_message_call(text,mode);
}
static void __fastcall narration_call(int speaker,const wchar_t *text) { COST_SCOPE("narration_call");
    bool captured=capture_message(1,speaker,text,_ReturnAddress());
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[1][captured?0:1]);
#endif
    if(!captured)original_narration_call(speaker,text);
}
static NarrationCall original_person_call;
static void __fastcall person_call(int speaker,const wchar_t *text) { COST_SCOPE("person_call");
    bool captured=capture_message(2,speaker,text,_ReturnAddress());
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[2][captured?0:1]);
#endif
    if(!captured)original_person_call(speaker,text);
}
using SceneCall=void(__fastcall *)(Obj);
using SceneryCall=void(__fastcall *)(unsigned,Obj,Obj,unsigned,unsigned);
static SceneCall original_employ_scene,original_discovery_scene;
static SceneryCall original_scenery_scene;
static CodeLink scene_links[3];
static bool scene_ready(unsigned path) { COST_SCOPE("advisor_briefing:scene_ready");
    return entry_ready(path) && message_gate(path,message_no_caller,fn<Obj(__fastcall *)(Obj)>(GA(GA_572480))(manager())!=nullptr);
}
static bool scene_person(Obj person,std::wstring &name) { COST_SCOPE("advisor_briefing:scene_person");
    return person && copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(person),name,100) && !name.empty();
}
static bool scene_object_name(Obj object,std::wstring &name) { COST_SCOPE("advisor_briefing:scene_object_name");
    return object && copy(reinterpret_cast<const wchar_t *>(object+0x10),name,100) && !name.empty();
}
static bool scene_advantage(Obj place,std::wstring &name) { COST_SCOPE("advisor_briefing:scene_advantage");
    if(!place)return false;
    Obj handle=fn<Obj(__fastcall *)(Obj)>(GA(GA_40060))(place);
    return handle && scene_object_name(fn<Obj(__fastcall *)(Obj)>(GA(GA_935F0))(handle),name);
}
static bool save_scene(unsigned path,Kind kind,Obj actor,const std::wstring &summary,const std::wstring &row) { COST_SCOPE("advisor_briefing:save_scene");
    if(!message_gate(path,message_fields,!summary.empty()))return false;
    Event e;e.kind=kind;e.day=day();e.speaker=word(actor,0x10);e.text=e.report=summary;e.row=row;
    return message_gate(path,message_store_failed,store({e}));
}
// 2026-11-08 user (FX2), 2026-11-09 user (PW17): a successful recruitment still plays its whole original
// success scene natively; only the lines of that same scene object (owner vtable GA_1444080, same envoy and
// target) are left to the game and never stored again. Every other message, same day included, ends the
// window at its first capture attempt and is collected exactly as without it, so the summary stored before
// the scene stays the only record. Other ends: day change (1), next employ/discovery scene (2),
// report_results (3), the first non-scene capture attempt on the scene's own thread (4).
struct LiveSuccess {Obj envoy,target;unsigned day,thread;bool active,speaking;};
static LiveSuccess live_success{};
#ifdef ADVISOR_BRIEFING_FIXTURE
static unsigned fixture_live_thread;   // PW19 test hook: nonzero replaces GetCurrentThreadId()
#endif
// PW19: only the thread that opened (and plays) the scene may close the window. A capture attempt on another
// thread passes natively: nothing is stored, hidden, moved or confirmed, and it counts as a live pass.
static unsigned live_scene_thread() { COST_SCOPE("advisor_briefing:live_scene_thread");
#ifdef ADVISOR_BRIEFING_FIXTURE
    if(fixture_live_thread)return fixture_live_thread;
#endif
    return static_cast<unsigned>(GetCurrentThreadId());
}
#ifndef NO_RUNTIME_LOG
// Path codes: 1 time_wait, 2 message, 3 event (scene line), 4 text, 5 speech, 6 narration/simple state.
static unsigned live_pass_codes,live_passes;
#endif
static void live_success_end(unsigned reason) { COST_SCOPE("advisor_briefing:live_success_end");
    if(!live_success.active)return;
    live_success.active=false;
#ifndef NO_RUNTIME_LOG
    // PW19: the closer carries the pass paths (bits 8+) and whether the scene itself had spoken (bit 0).
    log_event("advisor_employ_live_end",reason,live_passes,((live_pass_codes&0xFFu)<<8)|(live_success.speaking?1u:0u));
    live_pass_codes=live_passes=0;
#else
    (void)reason;
#endif
    live_success.speaking=false;
}
static bool live_success_active() { COST_SCOPE("advisor_briefing:live_success_active");
    if(!live_success.active)return false;
    if(live_success.day!=day()){live_success_end(1);return false;}   // 1 day
    return true;
}
static bool live_success_other_thread() {return live_success.active && live_success.thread!=live_scene_thread();}
static void live_success_begin(Obj envoy,Obj target) { COST_SCOPE("advisor_briefing:live_success_begin");
    if(!envoy || !target)return;
    live_success={envoy,target,day(),live_scene_thread(),true,false};
#ifndef NO_RUNTIME_LOG
    live_pass_codes=live_passes=0;
    log_event("advisor_employ_success_live",word(envoy,0x10),word(target,0x10),0);
#endif
}
// 2026-11-09 user (PW17): a line belongs to the live scene only while the recruiting scene object itself
// (vtable GA_1444080) speaks for the same envoy/target pair. The old FX2 predicate, restored.
static bool live_success_match(Obj owner) { COST_SCOPE("advisor_briefing:live_success_match");
    return owner && live_success_active() && pointer(owner,0)==image+GA(GA_1444080) &&
        pointer(owner,0x4B0)==live_success.envoy && pointer(owner,0x4B8)==live_success.target;
}
// One diagnostic per path code per live window; total_passes counts every pass-through call.
static void live_success_pass(unsigned code) { COST_SCOPE("advisor_briefing:live_success_pass");
#ifndef NO_RUNTIME_LOG
    if(!live_success.active)return;
    ++live_passes;
    const unsigned bit=1u<<code;
    if(live_pass_codes&bit)return;
    live_pass_codes|=bit;
    log_event("advisor_employ_live_pass",code,live_passes,0);
#else
    (void)code;
#endif
}
// PW19: a stored recruitment success row is remembered for its envoy/target pair and game day, so the event
// path's phase-2/3 row for the same pair is shown natively once instead of stored twice. Packed
// (day<<32)|(envoy id<<16)|target id so the scene and event hooks may run on different threads.
static volatile LONG64 scene_row_keys[4]{};
static volatile LONG scene_row_next;
static unsigned long long scene_row_key(Obj envoy,Obj target) { COST_SCOPE("advisor_briefing:scene_row_key");
    return (static_cast<unsigned long long>(day())<<32) |
        (static_cast<unsigned long long>(word(envoy,0x10))<<16) | word(target,0x10);
}
static void remember_scene_row(Obj envoy,Obj target) { COST_SCOPE("advisor_briefing:remember_scene_row");
    const LONG index=InterlockedIncrement(&scene_row_next);
    InterlockedExchange64(&scene_row_keys[static_cast<unsigned>(index)&3u],static_cast<LONG64>(scene_row_key(envoy,target)));
}
static bool scene_row_stored(Obj envoy,Obj target) { COST_SCOPE("advisor_briefing:scene_row_stored");
    if(!envoy || !target)return false;
    const unsigned long long key=scene_row_key(envoy,target);
    for(const auto &slot:scene_row_keys)if(static_cast<unsigned long long>(slot)==key)return true;
    return false;
}
static void forget_scene_rows() {for(auto &slot:scene_row_keys)slot=0;}
// local220 (user 2026-11-17 "명품이나 명승같은거 원작대로 클릭하면 원작의 일부분만 나오고 스킵되어버려.. 등용성공 패스시킨 사례를
// 보고 나머지도 조사해줘"): a scene left to the original because its topic is 「원작대로」 now plays whole. Each original scene
// entry (6C6B10 employ, 6C6DD0 discovery, 41F8C0 scenery, 6C6A70 destiny) pushes its state and parks the calling progress
// thread in 574750 (WaitForSingleObject) until that state ends; the state's frames speak on the UI thread through the same
// message/event hooks (discovery 75CA50 -> 844860 event speech, its step 1 of type 3 844390 -> 1F1E90; scenery 424A30 calls
// 6C6DD0 itself, 424B00 -> 1F1E90 bonus line). local99 sorted each of those lines again by its wording (topic_of_text), so a
// line of another topic was captured, reset native_item and the rest of the scene lost its waits and pictures.
// The window is open while the original call runs (depth) and while a state of the scene's vtables is on the manager stack
// or its pending push list (manager +30 count, +40 records of 16 bytes with the state at +8, written by 688B10). Inside it
// nothing is captured, no wait expires and no picture is hidden; a scene entered inside it (the scenery movie's discovery)
// passes natively and adds its vtable. Process-wide state, never thread-local: the opener waits on one thread, the lines come
// from another. Only a topic-off decision opens it, so all topics on behaves exactly as before. The briefing16 success
// window (live_success) is not opened for such a scene.
struct NativeScene {volatile LONG depth,count,armed;unsigned tables[4];unsigned path,passes,codes,opener,day;};
static NativeScene native_scene{};
static thread_local bool scene_topic_off;   // set by scene_gate when the topic, not another gate, refused the capture
static bool scene_gate(Topic t){if(topic_gate(t))return true;scene_topic_off=true;return false;}
static bool native_scene_listed() { COST_SCOPE("advisor_briefing:native_scene_listed");
    const LONG listed=native_scene.count,n=listed<4?listed:4;if(n<=0)return false;
    Obj m=manager();if(!m)return false;
    auto match=[&](Obj state){
        if(!state)return false;Obj table=pointer(state,0);
        for(LONG i=0;i<n;++i)if(table==image+native_scene.tables[i])return true;
        return false;
    };
    const auto live=*reinterpret_cast<const unsigned long long *>(m+0x10);Obj states=pointer(m,0x20);
    if(live<=256 && states)for(unsigned i=0;i<live;++i)if(match(pointer(states,i*8)))return true;
    const auto queued=*reinterpret_cast<const unsigned long long *>(m+0x30);Obj records=pointer(m,0x40);
    if(queued<=256 && records)for(unsigned i=0;i<queued;++i)if(match(pointer(records,i*16+8)))return true;
    return false;
}
static void native_scene_end(unsigned reason) { COST_SCOPE("advisor_briefing:native_scene_end");
    if(!InterlockedExchange(&native_scene.armed,0))return;
#ifndef NO_RUNTIME_LOG
    // reason 1 = no scene state left, 2 = the day changed (safety close); passes = calls let through; hex = pass code bits << 8 | scene path.
    log_event("advisor_native_scene_end",reason,native_scene.passes,((native_scene.codes&0xFFFFu)<<8)|(native_scene.path&0xFFu));
#else
    (void)reason;
#endif
    native_scene.passes=native_scene.codes=0;
}
static bool native_scene_active() { COST_SCOPE("advisor_briefing:native_scene_active");
    if(native_scene.depth>0)return true;
    if(!native_scene.armed)return false;
    // local222 safety close: a scene never outlives its day (a state that somehow stays listed must not stop collection for good).
    if(native_scene.day!=day()){native_scene_end(2);return false;}
    if(native_scene_listed())return true;
    native_scene_end(1);return false;
}
// Codes: 1 time_wait, 2 message/narration/person line, 3 event speech, 4 picture, 5 scene entered inside, 6 destiny/disaster inside.
static void native_scene_pass(unsigned code){++native_scene.passes;native_scene.codes|=1u<<code;}
// World reset: forget the scene vtables (a call still running keeps its depth until it returns).
static void native_scene_forget(){InterlockedExchange(&native_scene.armed,0);InterlockedExchange(&native_scene.count,0);native_scene.passes=native_scene.codes=0;}
struct NativeSceneScope {
    unsigned path;
    NativeSceneScope(unsigned scene_path,unsigned first,unsigned second=0):path(scene_path) {
        const bool nested=native_scene_active();
        if(!nested){native_scene.count=0;native_scene.passes=native_scene.codes=0;native_scene.path=path;native_scene.opener=GetCurrentThreadId();native_scene.day=day();}
        add(first);add(second);
        InterlockedExchange(&native_scene.armed,1);InterlockedIncrement(&native_scene.depth);
#ifndef NO_RUNTIME_LOG
        // army = scene path (4 employ, 5 discovery, 6 scenery, 7 destiny); value bit 0 nested, bit 1 opened on the outer opener's thread.
        log_event("advisor_native_scene",path,(nested?1u:0u)|(GetCurrentThreadId()==native_scene.opener?2u:0u),static_cast<unsigned>(native_scene.count));
#endif
    }
    ~NativeSceneScope() {
        InterlockedDecrement(&native_scene.depth);
#ifndef NO_RUNTIME_LOG
        // The original returned: 1 = its state is still listed (the call did not wait), 0 = the scene is already over.
        log_event("advisor_native_scene_return",path,native_scene.depth==0 && native_scene_listed()?1u:0u,native_scene.passes);
#endif
    }
    static void add(unsigned table) {
        if(!table)return;
        const LONG n=native_scene.count;
        for(LONG i=0;i<n;++i)if(native_scene.tables[i]==table)return;
        if(n<4){native_scene.tables[n]=table;InterlockedExchange(&native_scene.count,n+1);}
    }
};
static bool capture_employ_scene(Obj params) { COST_SCOPE("advisor_briefing:capture_employ_scene");
    if(!scene_ready(4) || !message_gate(4,message_params,params!=nullptr))return false;
    Obj actor=pointer(params,0),target=pointer(params,8);std::wstring a,b;
    if(!message_gate(4,message_fields,scene_person(actor,a) && scene_person(target,b)))return false;
    const bool success=dword(params,0x10)!=0;
    if(!scene_gate(success?Topic::Employ:Topic::EmployFail))return false;   // local220: 등용 성공 / 등용 실패
    const bool stored=save_scene(4,Kind::Recruitment,actor,recruitment_scene_summary(a,b,success),recruitment_scene_row(a,b,success));
    if(stored && success && mod_features.briefing16)remember_scene_row(actor,target);
    return stored;
}
static bool capture_discovery_scene(Obj params) { COST_SCOPE("advisor_briefing:capture_discovery_scene");
    if(!scene_ready(5) || !message_gate(5,message_params,params!=nullptr))return false;
    unsigned type=dword(params,0);if(!message_gate(5,message_phase,type<=5))return false;
    Obj actor=pointer(params,8);std::wstring a,b,place;bool fields=scene_person(actor,a);
    if(fields && type==1)fields=scene_person(pointer(params,0x20),b);
    else if(fields && type<=2)fields=scene_object_name(pointer(params,type==0?0x18:0x28),b);
    else if(fields && type<=4)fields=scene_advantage(pointer(params,0x10),b);
    else if(fields)fields=scene_object_name(pointer(params,0x30),b);
    if(!message_gate(5,message_fields,fields))return false;
    if(!scene_gate(type==0?Topic::Treasure:type==1?Topic::Talent:type==2?Topic::Goods:type<=4?Topic::Advantage:Topic::Scenery))return false;
    // Item/person summaries omit location rather than guess a city string ABI.
    Kind kind=type==0 || type==2?Kind::Treasure:type==1?Kind::Recruitment:Kind::News;
    const bool success=dword(params,0x38)!=0;
    const bool stored=save_scene(5,kind,actor,discovery_scene_summary(type,a,b,place,success),discovery_scene_row(type,a,b,place,success));
    if(stored && type==1 && success && mod_features.briefing16)remember_scene_row(actor,pointer(params,0x20));
    return stored;
}
static bool capture_scenery_scene(Obj actor,Obj scenery) { COST_SCOPE("advisor_briefing:capture_scenery_scene");
    if(!scene_ready(6))return false;
    std::wstring a,b;if(!message_gate(6,message_fields,scene_person(actor,a) && scene_object_name(scenery,b)))return false;
    if(!scene_gate(Topic::Scenery))return false;
    // 41D550's remaining arguments are display-only bonus message/amount.
    return save_scene(6,Kind::News,actor,discovery_scene_summary(5,a,b,L"",true),discovery_scene_row(5,a,b,L"",true));
}
static void __fastcall employ_scene(Obj params) { COST_SCOPE("employ_scene");
    // local220: inside a 원작대로 scene a nested scene plays natively too (nothing stored).
    if(native_scene_active()){native_scene_pass(5);NativeSceneScope scope(4,GA(GA_1444080));original_employ_scene(params);return;}
    if(live_success_active())live_success_end(2);   // 2 next scene: the previous window ends before this one
    scene_topic_off=false;
    bool captured=capture_employ_scene(params);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[4][captured?0:1]);
#endif
    if(scene_topic_off){NativeSceneScope scope(4,GA(GA_1444080));original_employ_scene(params);return;}   // local220: 원작대로 = the whole scene
    const bool success=mod_features.briefing16 && params && employ_pass_through(dword(params,0x10)!=0);
    if(success)live_success_begin(pointer(params,0),pointer(params,8));   // before the original scene speaks
    if(!captured || success)original_employ_scene(params);   // success: summary + the player-facing original scene
}
static void __fastcall discovery_scene(Obj params) { COST_SCOPE("discovery_scene");
    if(native_scene_active()){native_scene_pass(5);NativeSceneScope scope(5,GA(GA_144A270));original_discovery_scene(params);return;}   // local220
    if(live_success_active())live_success_end(2);   // 2 next scene: the previous window ends before this one
    scene_topic_off=false;
    bool captured=capture_discovery_scene(params);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[5][captured?0:1]);
#endif
    if(scene_topic_off){NativeSceneScope scope(5,GA(GA_144A270));original_discovery_scene(params);return;}   // local220: CSearchDiscoveryState
    const bool success=mod_features.briefing16 && params && employ_pass_through(dword(params,0)==1 && dword(params,0x38)!=0);
    if(success)live_success_begin(pointer(params,8),pointer(params,0x20));   // before the original scene speaks
    if(!captured || success)original_discovery_scene(params);   // recruited person: summary + the original scene
}
static void __fastcall scenery_scene(unsigned force,Obj actor,Obj scenery,unsigned message,unsigned amount) { COST_SCOPE("scenery_scene");
    // local220: CSearchSceneryMovieState (vtable 13D5F68) plays the movie, then calls 6C6DD0 itself (424A30), then the bonus line.
    if(native_scene_active()){native_scene_pass(5);NativeSceneScope scope(6,GA(GA_13D5F68),GA(GA_144A270));original_scenery_scene(force,actor,scenery,message,amount);return;}
    scene_topic_off=false;
    bool captured=capture_scenery_scene(actor,scenery);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[6][captured?0:1]);
#endif
    if(scene_topic_off){NativeSceneScope scope(6,GA(GA_13D5F68),GA(GA_144A270));original_scenery_scene(force,actor,scenery,message,amount);return;}
    if(!captured)original_scenery_scene(force,actor,scenery,message,amount);
}
// 사용자 2026-10-30: optional, per-caller E8 hooks. Native entries stay intact.
static constexpr unsigned scene_site_count=sizeof(scene_sites)/sizeof(*scene_sites);
static CallLink scene_calls[scene_site_count];
static Obj scene_next[scene_site_count]{};
struct ScenePending {Obj caller=nullptr;};
static thread_local ScenePending disaster_pending[2];
static void clear_scene_pending(){for(auto &pending:disaster_pending)pending={};}
template<class T> static bool scene_read(const void *at,T &value) {
    if(!at)return false;
    __try {memcpy(&value,at,sizeof(value));return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static Obj scene_call_target(unsigned index) { COST_SCOPE("advisor_briefing:scene_call_target");
    const auto &site=scene_sites[index];if(image[site.rva]!=0xE8)return nullptr;
    int delta;memcpy(&delta,image+site.rva+1,4);
    auto begin=reinterpret_cast<std::uintptr_t>(image);
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+reinterpret_cast<IMAGE_DOS_HEADER *>(image)->e_lfanew);
    auto end=begin+pe->OptionalHeader.SizeOfImage;
    auto target=reinterpret_cast<std::uintptr_t>(image+site.rva+5)+delta;
    return target && (target==begin+site.original || target<begin || target>=end)?reinterpret_cast<Obj>(target):nullptr;
}
static bool scene_call_publish(CallLink &call) { COST_SCOPE("advisor_briefing:scene_call_publish");
    int delta;memcpy(&delta,image+call.site+1,4);
    return image[call.site]==0xE8 && delta==call.original && call_publish(call);
}
static void restore_scene_calls() { COST_SCOPE("advisor_briefing:restore_scene_calls");
    for(unsigned i=scene_site_count;i;) {
        auto &call=scene_calls[--i];if(!call.published || image[call.site]!=0xE8)continue;
        int delta;memcpy(&delta,image+call.site+1,4);if(delta!=call.redirected)continue;
        if(call.raw)call_write(call.site,call.redirected,call.original);
        else {
            // Restore only our displacement, preserving edits to adjacent calls.
            CodeLink undo{};undo.block=call.site&~15u;
            memcpy(undo.original,image+undo.block,16);memcpy(undo.modified,undo.original,16);
            memcpy(undo.modified+(call.site-undo.block)+1,&call.original,4);
            replace_code16(undo.block,undo.original,undo.modified);
        }
        call.published=false;
    }
    clear_scene_pending();
}
static bool destiny_names(unsigned force,unsigned city,std::wstring &force_name,std::wstring &city_name) { COST_SCOPE("advisor_briefing:destiny_names");
    if(!force || force>51 || !city || city>61)return false;
    Obj owner=table(0xDF30,force,51),building=table(0x6DA98,city,3000);
    if(!owner || !building || !copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230750))(owner),force_name,100) || force_name.empty())return false;
    // Reuse native_ai_trace's building -> centre hex -> area-name lookup.
    unsigned hex=word(building,0x10);Obj tile=hex<48400?table(0xE270,hex,48399):nullptr;
    unsigned area=tile?word(tile,0x12):0;Obj place=area && area<=500?table(0x6CAF0,area,500):nullptr;
    return place && copy(reinterpret_cast<const wchar_t *>(place+0x10),city_name,100) && !city_name.empty();
}
static bool capture_destiny(Obj data) { COST_SCOPE("advisor_briefing:capture_destiny");
    if(!scene_ready(7))return false;
    unsigned fields[7];if(!message_gate(7,message_params,scene_read(data,fields)))return false;
    if(!message_gate(7,message_phase,fields[4]==0x5EE4 || fields[4]==0x5EE5))return false;
    std::wstring force_name,city_name;
    if(!message_gate(7,message_fields,destiny_names(fields[0],fields[1],force_name,city_name)))return false;
    if(!scene_gate(Topic::Weather))return false;
    Event event;event.kind=Kind::Weather;event.day=day();
    event.text=event.report=destiny_summary(force_name,city_name,fields[4],fields[5],fields[6]);
    return message_gate(7,message_store_failed,store({event}));
}
static void __fastcall destiny_call(Obj data) { COST_SCOPE("destiny_call");
    // local220: CDestinyRandomBonusState (6C6A70 -> 688500) has no checked vtable here, so its window is the call alone (the
    // progress thread waits in 574750 inside it).
    if(native_scene_active()){native_scene_pass(6);reinterpret_cast<SceneCall>(scene_next[0])(data);return;}
    scene_topic_off=false;
    bool captured=capture_destiny(data);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[7][captured?0:1]);
#endif
    if(scene_topic_off){NativeSceneScope scope(7,0);reinterpret_cast<SceneCall>(scene_next[0])(data);return;}
    if(!captured)reinterpret_cast<SceneCall>(scene_next[0])(data);
}
// Identical MSVC list<wchar_t*> ABI to state+478 in message_enter. 2509A0
// copies this exact input list to state+478; nodes own only borrowed pointers.
static bool copy_scene_list(Obj list,std::vector<Event> &batch,Kind kind) { COST_SCOPE("advisor_briefing:copy_scene_list");
    struct List {Obj head;unsigned long long count;};
    struct Node {Obj next,previous;const wchar_t *text;};
    List source{};if(!scene_read(list,source) || !source.head || !source.count || source.count>256)return false;
    Node head{};if(!scene_read(source.head,head))return false;Obj node=head.next,previous=source.head;
    for(unsigned long long i=0;i<source.count;++i) {
        Node value{};if(!node || node==source.head || !scene_read(node,value) || value.previous!=previous)return false;
        Event event;event.day=day();event.kind=kind;
        if(!copy(value.text,event.text) || event.text.empty())return false;
        auto category=classify(display_text(event.text));
        if(category!=Kind::News)event.kind=category;
        batch.push_back(std::move(event));previous=node;node=value.next;
    }
    return node==source.head && head.previous==previous;
}
using DisasterPush=void(__fastcall *)(Obj,Obj,Obj,Obj);
using DisasterYield=void(__fastcall *)(Obj);
template<unsigned Branch> static void __fastcall disaster_push(Obj state_manager,Obj type,Obj list,Obj callback) { COST_SCOPE("disaster_push");
    constexpr unsigned index=1+Branch*2,path=8+Branch;
    auto &pending=disaster_pending[Branch];pending={};
    bool captured=false;
    const bool in_scene=native_scene_active();   // local220: inside a 원작대로 scene the disaster list plays natively
    if(in_scene)native_scene_pass(6);
    // A push may be suppressed only with its own successfully published yield.
    if(!in_scene && scene_calls[index+1].published && scene_ready(path) && message_gate(path,message_params,type==image+GA(GA_13AFEA0)) && topic_gate(Topic::Disaster)) {
        Obj caller=fn<Obj(__fastcall *)(Obj)>(GA(GA_572480))(state_manager);
        std::vector<Event> batch;
        if(caller && message_gate(path,message_bad_text,copy_scene_list(list,batch,Kind::Disaster))) {
            captured=message_gate(path,message_store_failed,store(std::move(batch)));
            if(captured)pending.caller=caller;
        }
    }
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[path][captured?0:1]);
#endif
    if(!captured)reinterpret_cast<DisasterPush>(scene_next[index])(state_manager,type,list,callback);
}
template<unsigned Branch> static void __fastcall disaster_yield(Obj caller) { COST_SCOPE("disaster_yield");
    auto &pending=disaster_pending[Branch];bool skipped=caller && pending.caller==caller;
    pending={}; // consume exactly once, even on a mismatched continuation
    if(!skipped)reinterpret_cast<DisasterYield>(scene_next[2+Branch*2])(caller);
}
static void prepare_scene_calls(Obj relay) { COST_SCOPE("advisor_briefing:prepare_scene_calls");
    void *hooks[]={reinterpret_cast<void *>(&destiny_call),reinterpret_cast<void *>(&disaster_push<0>),reinterpret_cast<void *>(&disaster_yield<0>),reinterpret_cast<void *>(&disaster_push<1>),reinterpret_cast<void *>(&disaster_yield<1>)};
    // Existing advisor relays end at 825. [832,912) holds five 16-byte jumps.
    for(unsigned i=0;i<scene_site_count;++i) {
        scene_calls[i]={};scene_next[i]=nullptr;
        // 2026-11-18: SceneSkip patches the four disaster push/yield calls (rows 1-4) itself; they stay a pair, so skip both.
        if(scene_yield && i>=1 && i<=4){log_event("advisor_scene_call_skip",scene_sites[i].rva,scene_sites[i].original,3);continue;}
        Obj next=scene_call_target(i),at=relay+832+i*16;
        if(!next){log_event("advisor_scene_call_skip",scene_sites[i].rva,scene_sites[i].original,0);continue;}
        absolute_jump(at,hooks[i]);
        if(!call_prepare(scene_calls[i],scene_sites[i].rva,at)){log_event("advisor_scene_call_skip",scene_sites[i].rva,scene_sites[i].original,1);continue;}
        scene_next[i]=next;
    }
}
static void publish_scene_calls() { COST_SCOPE("advisor_briefing:publish_scene_calls");
    for(unsigned i=0;i<scene_site_count;++i)if(scene_next[i] && !scene_call_publish(scene_calls[i]))log_event("advisor_scene_call_skip",scene_sites[i].rva,scene_sites[i].original,2);
}
static bool prepare_message_link(CodeLink &link,unsigned rva,const unsigned char *prologue,unsigned size,Obj at,void *hook) { COST_SCOPE("advisor_briefing:prepare_message_link");
    if(size<5 || (rva&15u)+size>16 || ga::mcmp_at(image,rva,prologue,size))return false;
    absolute_jump(at,hook);memcpy(at+32,image+rva,size);absolute_jump(at+32+size,image+rva+size);
    if(!prepare_link(link,rva,0xE9,at))return false;
    // Keep the shared five-byte CodeLink contract. Only this local entry patch
    // pads the rest of its displaced whole instructions inside the same CAS16.
    memset(link.modified+(rva-link.block)+5,0x90,size-5);return true;
}
static bool prepare_picture_link(CodeLink &link,unsigned rva,const unsigned char *bytes,unsigned size,Obj at,void *hook) { COST_SCOPE("advisor_briefing:prepare_picture_link");
    if(size<5 || (rva&15u)+size>16 || ga::mcmp_at(image,rva,bytes,size))return false;
    absolute_jump(at,hook); // Only our destination; no displaced native bytes/rel32.
    if(!prepare_link(link,rva,0xE9,at))return false;
    memset(link.modified+(rva-link.block)+5,0x90,size-5);return true;
}
static void pop(){fn<void(__fastcall *)(Obj)>(GA(GA_124A0))(manager());}
static void offscreen(Obj p){if(p)layout_position(p,10000,10000);}
static std::wstring adviser_name() { COST_SCOPE("advisor_briefing:adviser_name");
    std::wstring name;
    if(last_adviser)copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(last_adviser),name,100);
    return name.empty()?L"군사":name;
}
static DiplomaticFacts report_facts(Obj owner,Obj person) { COST_SCOPE("advisor_briefing:report_facts");
    DiplomaticFacts facts;Obj context=world()?pointer(world(),0x871E8):nullptr;
    Obj player=context?fn<Obj(__fastcall *)(Obj)>(GA(GA_330B90))(context):nullptr;
    if(player && person) {
        facts.friendly=fn<Obj(__fastcall *)(Obj)>(GA(GA_22E450))(person)==player;
        facts.ruler=facts.friendly && word(person,0x10)==word(player,0x10);
    }
    Obj layout=owner?pointer(owner,0x4C0):nullptr,window=layout?pointer(layout,0x188):nullptr;
    if(window) {
        Obj from=pointer(window,0x180),to=pointer(window,0x190);
        if(player && from && to && (from==player || to==player)) {
            facts.known_sides=true;facts.incoming=to==player;Obj other=from==player?to:from;
            copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230750))(other),facts.counterpart,100);
        }
        Obj command=pointer(window,0x1D8),vtable=command?pointer(command,0):nullptr;
        if(vtable)facts.kind=(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(vtable+0x98))(command);
    }
    if(owner && dword(owner,0x4D8)>=12 && dword(owner,0x4D8)<=15) {
        unsigned result=dword(owner,0x4F8);if(result<=1)facts.result=static_cast<int>(result);
    }
    return facts;
}
static void refresh_progress() { COST_SCOPE("advisor_briefing:refresh_progress");
    // The command HUD can stop updating while days advance. Notifications push
    // their badge snapshot directly from this original UI callback instead.
    note_duel(4);
    if(!inbox.collecting)return;
    briefing_overlay::Model m;m.adviser=adviser_name();m.generation=inbox.generation;
    m.unread=inbox.unread();m.tickets=ambush_badge();m.collecting=true;m.title=L"이번 순의 소식을 모으고 있습니다";
    m.body=L"진행이 끝나면 한데 모아 보고드리겠습니다.";
    bool show=!interaction();briefing_overlay::progress_flags(show,clean_scene());
    briefing_overlay::publish(m,show);
}
// local222 row faces: names -> person ids (the speaker id covers only spoken messages). One pass over the person table per cycle; a
// name two persons share gets no face. A force ("<name>군") or unit ("<name>부대") row shows its leader's face.
static std::vector<std::pair<std::wstring,unsigned>> person_names;   // sorted by name; id 0 = a name two persons share
static int person_names_day=-2;
static void index_person_names(Obj root) { COST_SCOPE("advisor_briefing:index_person_names");
    person_names.clear();person_names_root=root;person_names_day=started_day;
    if(!root)return;
#ifndef NO_RUNTIME_LOG
    LARGE_INTEGER started_at{},ended_at{},hz{};QueryPerformanceCounter(&started_at);   // local223: the first open of a cycle pays for this pass
#endif
    std::vector<std::pair<std::wstring,unsigned>> all;
    for(unsigned id=1;id<6001;++id) {
        Obj who=pointer(root,0x158+id*8);std::wstring name;
        if(!who || !fn<int(__fastcall *)(Obj)>(GA(GA_331780))(who) || !copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(who),name,100) || name.empty())continue;
        all.emplace_back(std::move(name),id);
    }
    std::sort(all.begin(),all.end());
    for(auto &p:all){if(!person_names.empty() && person_names.back().first==p.first)person_names.back().second=0;else person_names.push_back(std::move(p));}
#ifndef NO_RUNTIME_LOG
    QueryPerformanceCounter(&ended_at);QueryPerformanceFrequency(&hz);
    log_event("advisor_person_index",static_cast<unsigned>(person_names.size()),static_cast<unsigned>((ended_at.QuadPart-started_at.QuadPart)*1000000/(hz.QuadPart?hz.QuadPart:1)));
#endif
}
static unsigned person_by_name(Obj root,const std::wstring &name) {
    if(name.empty())return 0;
    if(person_names_root!=root || person_names_day!=started_day)index_person_names(root);
    auto find=[](const std::wstring &n)->unsigned {
        auto it=std::lower_bound(person_names.begin(),person_names.end(),n,[](const std::pair<std::wstring,unsigned> &a,const std::wstring &b){return a.first<b;});
        return it!=person_names.end() && it->first==n?it->second:0;
    };
    if(const unsigned id=find(name))return id;
    for(const std::wstring tail:{std::wstring(L"부대"),std::wstring(L"군")})
        if(name.size()>tail.size()+1 && ends_with(name,tail))if(const unsigned id=find(name.substr(0,name.size()-tail.size())))return id;
    return 0;
}
static unsigned event_person(const Event &e,Obj root) {   // as the report rows: the speaker when the row is named after them, else the name
    if(!root)return 0;
    std::wstring name;
    if(e.speaker && e.speaker<6001){Obj who=pointer(root,0x158+e.speaker*8);if(!who || !copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(who),name,100))name.clear();}
    ReportRow r=report_row(e,name,e.speaker);
    if(!r.person && r.named)r.person=person_by_name(root,r.name);
    return r.person;
}
static void row_faces_tick(bool showing,const briefing_overlay::Model &m,Obj root);
#ifndef NO_RUNTIME_LOG
static void face_manager_survey(bool eligible,bool modal);   // local253 diagnostic (출진 초상화)
static void person_principle_survey();   // local258 diagnostic (exe+22B5CF crash)
#endif
void hud(Obj layout,bool eligible,bool modal) { COST_SCOPE("advisor_briefing:hud");
#ifndef NO_RUNTIME_LOG
    face_manager_survey(eligible,modal);
    person_principle_survey();
#endif
    briefing_overlay::set_extended(mod_features.briefing16);   // local17: the overlay has no feature table of its own
    note_duel(3);
#ifndef NO_RUNTIME_LOG
    FrameTiming timing(0);
#endif
    if(speech_owner && (diplomatic_parent(true)!=speech_owner || !active()))restore_speech();
    if(!active()){row_faces_tick(false,display_cache,nullptr);briefing_overlay::publish({},false);return;}
    // 2026-11-08 user: camera focus -> city domestic immediately; the military
    // message is independent and may outlive the screen transition.
    if(pending_city) {
        ULONGLONG now=GetTickCount64();
        if(now>=pending_city_at) {
            if(fn<int(__fastcall *)(Obj)>(GA(GA_5727B0))(manager())!=0 && now-pending_city_started<1500) {
                pending_city_at=now+50;   // camera transition still busy; retry on the player frame
            } else {
                unsigned id=pending_city;pending_city=0;
                briefing_overlay::toast(L"지금부터 "+pending_city_name+L"의 토목공사를 시작합니다.");
                pending_city_name.clear();
                open_city_domestic_direct(id);
            }
        }
    }
    Obj root=world();unsigned force=force_id();
    if(inbox.world && (inbox.world!=reinterpret_cast<std::uintptr_t>(root) || inbox.force!=force))reset();
    inbox.context(reinterpret_cast<std::uintptr_t>(root),force);
    bool player=fn<bool(__fastcall *)(Obj,const char *)>(GA(GA_5726B0))(manager(),"CUserStrategyState");
    // 2026-10-27 live: the native command menu auto-hides independently of
    // standby. Do not let its visibility fight the player-frame inbox publisher.
    // 56FC20 returns manager+48, the state currently being updated, NOT a
    // modal window. Live standby cycles this through CGameState/CRootState.
    // The actual top-of-stack test already excludes every standby dialog.
    bool show=!interaction() && (player || inbox.collecting);
    if(player && !inbox.collecting)ambush_tick();   // one pass per 10-day turn: monthly 상순 roll, expiry
    if(show)unified_overlay::install();
    unsigned generation=0,action=briefing_overlay::take(generation);
    std::vector<Card> cards;
    bool have_cards=action!=0,accepted_input=false;
    // local232: the original log's lines are the last tab (로그), after the topics and 전체: page report_pages(cards) when there are any.
    unsigned log_gen=0;size_t log_count=0;
    AcquireSRWLockShared(&native_log_lock);log_gen=log_generation;log_count=native_log.size();ReleaseSRWLockShared(&native_log_lock);
    auto pages_of=[&](size_t n){return report_pages(n)+(log_count?1u:0u);};
    if(action)cards=inbox.tabs();   // local229: one tab per topic (report_tab)
    if(action && page>=pages_of(cards.size()))page=0;
    if(action && show && player && !inbox.collecting && generation==inbox.generation) {
        accepted_input=true;
        // local64 (user 2026-11-12): the ambush proposal is a speech balloon at the head (10 수락 / 11 거절, 12 = its
        // body); the head click always opens the report.
        if(action==10 || action==11){if(ambush_offers())ambush_answer(action==10);}
        else if(action==12){}
        else if(action==1) {
            open=!open;offer=false;advice=false;selected=~0u;fresh_keys.clear();
            if(open){for(const auto &c:cards)for(const auto &e:c.events)if(!e.read)fresh_keys.push_back(fresh_key(e));std::sort(fresh_keys.begin(),fresh_keys.end());}
        }
        else if(action==2){open=false;city_list=false;offer=false;advice=false;settings=false;}
        else if(offer && (action==5 || action==6)){ambush_answer(action==5);offer=false;open=false;}
        else if(offer){}   // the proposal has only 수락/거절/×
        else if(action==8){advice=!advice;city_list=false;settings=false;}   // local68: [군사 제안] page next to [부동산 개발]
        else if(advice){}
        else if(action==13){settings=!settings;city_list=false;}   // local99: [수집 설정] page
        else if(settings){if(action>=100)collect_toggle(action-100);}   // a row = one topic: 알림함 <-> 원작대로
        else if(action==7){city_list=!city_list;if(city_list)refresh_cities();}
        else if(city_list && action>=100 && action<100+city_ids.size()) {
            unsigned chosen=city_ids[action-100];
            Obj city=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(world(),chosen);
            if(valid(city)) {
                pending_city=chosen;pending_city_at=GetTickCount64()+500;pending_city_started=GetTickCount64();
                pending_city_name=city_names[action-100];
                focus_city(city);
            }
            open=false;city_list=false;page=0;
            return;
        }
        else if(!city_list && action==3 && pages_of(cards.size())){const unsigned n=pages_of(cards.size());page=(page+n-1)%n;selected=~0u;}
        else if(!city_list && action==4 && pages_of(cards.size())){page=(page+1)%pages_of(cards.size());selected=~0u;}
        else if(!city_list && action==5 && page<cards.size()){inbox.erase_tab(cards[page].tab);page=0;selected=~0u;}   // 전체 has no single tab to delete
        else if(!city_list && action==5 && log_count && page==report_pages(cards.size())){clear_native_log();log_count=0;page=0;selected=~0u;}   // 로그: 지우기
        else if(!city_list && action==6){inbox.clear();clear_native_log();log_count=0;page=0;selected=~0u;fresh_keys.clear();}
        else if(!city_list && action>=40 && action<40+kReportTabs){if(static_cast<unsigned>(action-40)<pages_of(cards.size()) && page!=static_cast<unsigned>(action-40)){page=action-40;selected=~0u;}}
        else if(!city_list && action>=kMapHit) {
            // local235 (user 2026-11-18 "원작의 로그 누르면 해당도시?로 카메라 이동이 됐었나?"): as the original log line click (671260), a row
            // with a place moves the camera there and the report closes so the map shows. local243: only its [지도 이동] button does.
            const unsigned row=static_cast<unsigned>(action-kMapHit);
            if(row<shown_log_hex.size() && shown_log_hex[row]<48400) {
                const unsigned hex=shown_log_hex[row];
                focus_hex(hex);open=false;selected=~0u;
#ifndef NO_RUNTIME_LOG
                log_event("advisor_log_focus",hex,row,static_cast<unsigned>(shown_log_hex.size()));
#endif
            }
        }
        else if(!city_list && action>=1000)selected=action-1000;   // a row: its whole text and records fill the panel below
        if(!city_list && (action==5 || action==6))cards=inbox.tabs();
        if(!city_list && !offer && !advice && open && !cards.empty()) {   // the open tab is read (전체: every message; 로그: lines are read)
            if(log_count && page==report_pages(cards.size())){}
            else if(page>=cards.size()){bool any=false;for(auto &c:cards){any=any || c.unread;c.unread=0;}if(any)inbox.mark_all();}
            else if(cards[page].unread){inbox.mark_tab(cards[page].tab);cards[page].unread=0;}
        }
    }
    if(inbox.collecting){open=false;city_list=false;offer=false;advice=false;settings=false;}
    row_faces_tick(show && open && !city_list && !offer && !advice && !settings,display_cache,root);   // local222: faces of the rows on screen
    const unsigned tickets_now=ambush_tickets(),offers_now=ambush_offers(),offer_kind_now=offers_now?ambush_offer_kind():0;
    std::wstring note_now;if(open && advice)ambush_status(note_now);
    AcquireSRWLockShared(&native_log_lock);log_gen=log_generation;log_count=native_log.size();ReleaseSRWLockShared(&native_log_lock);
    if(rendered_generation==inbox.generation && rendered_log_generation==log_gen && rendered_page==page &&rendered_open==open && rendered_collecting==inbox.collecting && rendered_adviser==last_adviser && rendered_city_list==city_list && rendered_offer==offer && rendered_tickets==tickets_now && rendered_offers==offers_now && rendered_offer_kind==offer_kind_now && rendered_note==note_now && rendered_advice==advice && rendered_settings==settings && rendered_settings_revision==settings_revision && rendered_selected==selected) {
        briefing_overlay::publish(display_cache,show,accepted_input);return;
    }
    if(!have_cards)cards=inbox.tabs();if(page>=pages_of(cards.size()))page=0;
    const bool log_tab=log_count>0,logs=log_tab && page==report_pages(cards.size());   // local232: 로그 is open
    briefing_overlay::Model m;m.adviser=adviser_name();m.generation=inbox.generation;m.unread=inbox.unread();m.open=open;m.collecting=inbox.collecting;m.city_list=city_list;m.page=page;m.pages=pages_of(cards.size());
    m.every_tab=cards.size()>1?static_cast<int>(cards.size()):-2;   // 전체 (no 지우기 there); -2: none, 지우기 on every tab
    m.tickets=tickets_now;m.offer=offer;m.offers=offers_now;m.offer_kind=offer_kind_now;m.advice=advice && !offer;
    m.settings=settings && !offer && !m.advice;if(m.settings)m.city_list=true;   // the topic rows use the city grid
    if(offer && offer_kind_now==1) {   // destruction_ticket (user 2026-11-13)
        m.title=L"파괴공작 제안";
        m.body=L"주공, 우리 성이 있는 지역과 바로 맞닿은 지역에 적의 시설이 서 있습니다. 금 1000을 주시면 파괴공작을 준비해 두겠습니다.\n\n"
               L"파괴공작이 준비된 뒤 우리 성이 있는 지역과 바로 맞닿은 지역에 계략 [건물파괴]를 쓰면, 원래 판정이 실패하더라도 준비 하나를 써서 성공시킵니다. "
               L"스스로 성공한 계략에는 준비를 쓰지 않습니다. 준비는 두 개까지 갖출 수 있습니다.\n\n"
               L"금은 군주께서 머무시는 거점에서 나갑니다.";
    } else if(offer) {
        m.title=L"매복 작전 제안";
        m.body=L"주공, 매복이동으로 전황을 바꿀 수도 있을 것 같습니다. 금 1000을 주시면 준비해보겠습니다.\n\n"
               L"준비가 끝나면 출진 화면 오른쪽 아래의 [군사제안수행]를 켜고 출진하십시오. 그 부대는 적 영역(ZOC)에 닿은 날부터 60일(길어도 180일) 동안 적의 임무와 목표 탐색에 잡히지 않습니다. "
               L"가까이 붙으면 싸움은 피할 수 없고, 그동안 진형 기동력은 절반으로 줄어듭니다.\n\n"
               L"금은 군주께서 머무시는 거점에서 나갑니다.";
    } else if(advice) {
        m.title=L"군사 제안";
        m.body=note_now.empty()?std::wstring(L"지금은 올릴 계책이 없습니다."):note_now;
    } else if(m.settings) {
        m.title=L"알림함에 모을 소식";
        m.cities=collect_labels();
    } else if(city_list) {
        m.title=L"보유한 성";
        m.cities=city_names;
    } else if(cards.empty() && !log_tab) {
        m.title=inbox.collecting?L"주공, 소식을 모으고 있습니다":L"주공, 새로 전할 소식이 없습니다";
        m.body=inbox.collecting?L"이번 순의 일이 끝나면 한데 모아 보고드리겠습니다.":L"다음 진행부터 천시·재해·성장·인재와 탐색 소식을 이곳에서 확인하실 수 있습니다.";
    } else {
        // local222 report view: every message of the cycle in day order, one row each; the open tab shows its topic (local229; 전체: all).
        const bool every=!logs && page>=cards.size();
        if(!logs)refresh_places();   // local240: city names for the rows' map jump
        std::vector<Event> log_lines;   // local232: 로그 rows, the latest kReportRows shown
        if(logs){AcquireSRWLockShared(&native_log_lock);log_lines=native_log;ReleaseSRWLockShared(&native_log_lock);}
        std::vector<std::pair<const Event *,unsigned>> items;
        // local235 (user 2026-11-18 "역순으로 정렬되어야해 최신일이 제일 윗줄"): newest first, the top row is the latest line
        if(logs)for(auto it=log_lines.rbegin();it!=log_lines.rend();++it)items.emplace_back(&*it,~0u);
        else {
            for(const auto &c:cards)for(const auto &e:c.events)items.emplace_back(&e,c.tab);
            std::stable_sort(items.begin(),items.end(),[](const std::pair<const Event *,unsigned> &a,const std::pair<const Event *,unsigned> &b){return a.first->day<b.first->day;});
        }
        std::vector<std::pair<unsigned,std::wstring>> speakers;   // id -> the name as the game spells it, looked up once per id
        auto speaker_name=[&](unsigned id)->std::wstring {
            if(!id || id>=6001)return {};
            for(const auto &s:speakers)if(s.first==id)return s.second;
            std::wstring name;Obj who=pointer(root,0x158+id*8);
            if(!who || !copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(who),name,100))name.clear();
            speakers.emplace_back(id,name);return name;
        };
        std::vector<ReportRow> all_rows;std::vector<ReportRow> page_rows;std::vector<unsigned> page_hex;size_t total=0;unsigned first_day=0,last_day=0;
        std::vector<std::pair<bool,unsigned>> page_order;   // local243: (failure, day) of each page row, for the success-first order
        std::uint64_t gold=0;bool gold_known=true,any_gold=false;
        all_rows.reserve(items.size());
        unsigned faceless_named=0,faceless_unnamed=0,faceless_topics=0;   // local229 diagnostics: the open tab's rows without a person
        for(const auto &item:items) {
            const Event *e=item.first;
            const std::wstring name=speaker_name(e->speaker);
            ReportRow r=report_row(*e,name,e->speaker);
            if(!r.person && r.named)r.person=person_by_name(root,r.name);   // a name the game knows as one person: that person's face
            r.unread=!e->read || std::binary_search(fresh_keys.begin(),fresh_keys.end(),fresh_key(*e));
            if(logs || every || item.second==cards[page].tab) {
                if(!r.person){if(r.named)++faceless_named;else ++faceless_unnamed;faceless_topics|=1u<<(static_cast<unsigned>(topic_of_event(*e))&31u);}
                ++total;
                if(e->day){if(!first_day || e->day<first_day)first_day=e->day;if(e->day>last_day)last_day=e->day;}
                if(e->kind==Kind::Gold){any_gold=true;if(e->gold)gold+=e->gold;else gold_known=false;}
                // local240/243: a row with a place (the record's hex, else a city or region its text names) gets a [지도 이동] button
                const unsigned place=logs?e->hex:row_place(*e);r.place=place<48400;
                page_rows.push_back(r);page_hex.push_back(place);page_order.emplace_back(!logs && event_failed(*e),e->day);
                collect_names(e->text,m.names);if(!name.empty())add_name(m.names,name);
            }
            all_rows.push_back(std::move(r));
        }
        if(!logs) {   // local243 (user 2026-11-19 "로그처럼 ... 실패 소식은 아래에 깔고 성공 소식을 위에"): successes first, then failures, each
            // newest first (arrival order reversed, then a stable sort on failure and day). all_rows stays in day order for report_records.
            std::vector<size_t> order(page_rows.size());for(size_t i=0;i<order.size();++i)order[i]=order.size()-1-i;
            std::stable_sort(order.begin(),order.end(),[&](size_t a,size_t b){return page_order[a].first!=page_order[b].first?!page_order[a].first:page_order[a].second>page_order[b].second;});
            std::vector<ReportRow> rows;std::vector<unsigned> hexes;rows.reserve(order.size());hexes.reserve(order.size());
            for(size_t i:order){rows.push_back(std::move(page_rows[i]));hexes.push_back(page_hex[i]);}
            page_rows.swap(rows);page_hex.swap(hexes);
        }
        const bool capped=page_rows.size()>kReportRows;
        if(capped){page_rows.resize(kReportRows);page_hex.resize(kReportRows);}   // local235/243: newest first everywhere, the latest lead
        shown_log_hex=page_hex;   // local240: report rows too (48400 = no place)
        if(selected>=page_rows.size()) {
            selected=0;
            for(size_t i=0;i<page_rows.size();++i)if(page_rows[i].unread){selected=static_cast<unsigned>(i);break;}
        }
#ifndef NO_RUNTIME_LOG
        // local229 (user 2026-11-18 "초상화 안나오던거는 뭐야?"): when a tab opens, how many of its rows have no face: named (a name
        // that is not one living officer: a force, a unit, a namesake, a dead officer) and unnamed, and the topics they have (bits).
        if(!rendered_open || rendered_page!=page)log_event("advisor_row_noface",faceless_named,faceless_unnamed,faceless_topics);
        if(!rendered_open || rendered_page!=page) {   // local240: rows of the open tab with a place (map jump) / all rows / whether 로그
            unsigned placed=0;for(unsigned hex:page_hex)if(hex<48400)++placed;
            log_event("advisor_row_places",placed,static_cast<unsigned>(page_hex.size()),logs?1u:0u);
        }
#endif
        m.report=true;m.selected=selected;m.log_view=logs;   // local235: 로그 has no detail panel (the rows run to the footer)
        for(const auto &c:cards)m.tabs.push_back(std::wstring(report_tab_caption(c.tab))+L" "+std::to_wstring(c.events.size()));
        if(cards.size()>1){size_t all=0;for(const auto &c:cards)all+=c.events.size();m.tabs.push_back(L"전체 "+std::to_wstring(all));}
        if(log_tab)m.tabs.push_back(L"로그 "+std::to_wstring(log_count));
        m.facts=report_info(page_rows,total,first_day,last_day,any_gold && gold_known?gold:0);m.kinds=report_kinds(page_rows);
        if(capped)m.facts.push_back(L"표시\t최근 "+std::to_wstring(kReportRows)+L"건");
        if(!page_rows.empty()) {
            const ReportRow &chosen=page_rows[selected];
            m.detail_title=report_detail_title(chosen);m.detail_caption=report_detail_caption(chosen);
            m.detail_body=chosen.detail;
            m.records=report_records(all_rows,chosen);
        }
        if(logs) {
            m.title=L"로그 · "+std::to_wstring(items.size())+L"건";
            for(const auto &item:items){if(!m.body.empty())m.body+=L"\n";m.body+=display_text(item.first->text);}
        } else if(every) {
            m.title=L"전체 소식 · "+std::to_wstring(items.size())+L"건";
            for(const auto &c:cards){if(!m.body.empty())m.body+=L"\n\n";m.body+=card_body(c);}
        } else {
            const auto &card=cards[page];
            m.title=report_tab_title(card.tab)+L" · "+std::to_wstring(card.events.size())+L"건";
            m.body=card_body(card);
        }
        m.rows=std::move(page_rows);
    }
    display_cache=std::move(m);rendered_selected=selected;rendered_generation=inbox.generation;rendered_log_generation=log_gen;rendered_page=page;rendered_open=open;rendered_collecting=inbox.collecting;rendered_adviser=last_adviser;rendered_city_list=city_list;rendered_offer=offer;rendered_tickets=tickets_now;rendered_offers=offers_now;rendered_offer_kind=offer_kind_now;rendered_note=note_now;rendered_advice=advice;rendered_settings=settings;rendered_settings_revision=settings_revision;
    briefing_overlay::publish(display_cache,show,accepted_input);(void)layout;(void)eligible;(void)modal;
}
// interop S7 (user 2026-11-16 "1+2?④퀎"): the 20 state slots of slots[] are chained through the Layout manager (rows "advisor_briefing"
// in layout_registry.h). Every replacement calls the value the slot held before it - another module's hook or the original - never the
// original address itself. With only this mod installed slot_next[i] is image+slots[i].original, the same call as before.
static void *slot_next[20];
// Before install (or after a failed one) a slot has no chain yet: fall back to the original, exactly the old direct call.
// A next that is still the original goes through fn() (same address in the game, the mock in fixtures).
static void *slot_ptr(unsigned i) {void *next=slot_next[i];return next && next!=static_cast<void *>(image+slots[i].original)?next:fn<void *>(slots[i].original);}
template<class T> static T slot_call(unsigned i) {return reinterpret_cast<T>(slot_ptr(i));}
static unsigned long long __fastcall user_enter(Obj state,Obj argument) { COST_SCOPE("advisor_briefing::user_enter"); COST_CRUMB("advisor_briefing_state",state);
    auto result=slot_call<unsigned long long(__fastcall *)(Obj,Obj)>(0)(state,argument);
    if(active()){inbox.finish();open=false;city_list=false;page=0;}
    return result;
}
static void __fastcall user_frame(Obj state) { COST_SCOPE("advisor_briefing::user_frame"); COST_CRUMB("advisor_briefing_state",state);
#ifndef NO_RUNTIME_LOG
    FrameTiming timing(3);
#endif
#ifndef NO_RUNTIME_LOG
    hook_called(0);
#endif
    slot_call<void(__fastcall *)(Obj)>(17)(state);
    // UI-only actions must not wait for the command HUD's refresh cadence.
    // Stay on this original game/UI frame; never call native functions from
    // Present/WndProc, and do not touch state after its original frame returned.
    if(active() && top_state()==state) {
        hud(nullptr,true,false);
    }
}
static void __fastcall user_leave(Obj state,Obj argument) { COST_SCOPE("advisor_briefing::user_leave"); COST_CRUMB("advisor_briefing_state",state);
    if(active()) {
        inbox.begin(reinterpret_cast<std::uintptr_t>(world()),force_id());open=false;page=0;diplomatic_choice_parent=nullptr;
        started_day=game_day();
        forget_scene_rows();   // PW19: day() restarts with this cycle, so the duplicate-row memory does too
        refresh_progress();
        log_event("advisor_briefing_cycle_begin",force_id(),inbox.generation);
    }
    slot_call<void(__fastcall *)(Obj,Obj)>(1)(state,argument);
}
static void __fastcall game_leave(Obj state,Obj argument) { COST_SCOPE("advisor_briefing::game_leave"); COST_CRUMB("advisor_briefing_state",state);reset();slot_call<void(__fastcall *)(Obj,Obj)>(2)(state,argument);}
static void __fastcall narration(Obj state) { COST_SCOPE("advisor_briefing::narration"); COST_CRUMB("advisor_briefing_state",state);
#ifndef NO_RUNTIME_LOG
    hook_called(3);
#endif
    slot_call<void(__fastcall *)(Obj)>(3)(state);
}
static unsigned long long message_enter(Obj state,void *original,bool diplomatic) { COST_SCOPE("advisor_briefing:message_enter");
    bool accepted=false;
    const bool skip=diplomatic && dialogue_ready(false),watch=diplomatic && !skip && dialogue_observed(false);
    const bool wanted=!diplomatic || collects(Topic::Diplomacy);   // local99: topic off = the original dialog, nothing stored
#ifndef NO_RUNTIME_LOG
    if(diplomatic)skip_decision(0,false,skip);
#endif
    if((skip || watch) && wanted) {
        std::vector<Event> batch;Obj head=pointer(state,0x478),node=head?pointer(head,0):nullptr;
        Obj scene=skip?diplomatic_parent(false):diplomatic_observed(false);
        size_t count=static_cast<size_t>(*reinterpret_cast<unsigned long long *>(state+0x480));
        bool ok=head && count>0 && count<=256;
        for(size_t i=0;ok && i<count;++i) {
            if(!node || node==head){ok=false;break;}
            Event e;e.day=day();if(diplomatic)e.kind=Kind::Diplomacy;
            if(pointer(state,0)==image+GA(GA_13A5FD8))e.speaker=dword(state,0x488);
            ok=copy(reinterpret_cast<const wchar_t *>(pointer(node,0x10)),e.text) && !e.text.empty() && (diplomatic || passive_notice(e.text));
            if(ok && diplomatic){const DiplomaticFacts facts=report_facts(scene,nullptr);e.report=diplomatic_report(e.text,facts);e.row=diplomatic_row(facts);}
            if(ok)batch.push_back(std::move(e));node=pointer(node,0);
        }
        // PW19: another thread's state passes natively (no store, no takeover) and never closes the scene window.
        const bool foreign=live_success_active() && live_success_other_thread();
        accepted=ok && node==head && !foreign && store(std::move(batch),diplomatic) && skip;   // an observed (player-decided) page is stored but still shown and confirmed natively
        if(foreign)live_success_pass(6);
        else if(accepted && live_success_active())live_success_end(4);
#ifndef NO_RUNTIME_LOG
        if(accepted)stall_begin(state,diplomatic?5:pointer(state,0)==image+GA(GA_13A5FD8)?1:0);
#endif
    }
    auto result=reinterpret_cast<unsigned long long(__fastcall *)(Obj)>(original)(state);
    if(result && accepted) {
        // Create the native dialog and its completion binding first; close from
        // its first active update, never reenter the manager during Enter.
        simple_state=state;simple_states.push_back(state);offscreen(pointer(state,0x470));
    }
    return result;
}
static unsigned long long __fastcall simple_enter(Obj state){ COST_SCOPE("advisor_briefing::simple_enter"); COST_CRUMB("advisor_briefing_state",state);return message_enter(state,slot_ptr(4),false);}
static unsigned long long __fastcall diplomatic_enter(Obj state){ COST_SCOPE("advisor_briefing::diplomatic_enter"); COST_CRUMB("advisor_briefing_state",state);return message_enter(state,slot_ptr(14),true);}
static unsigned long long __fastcall person_notice_enter(Obj state){ COST_SCOPE("advisor_briefing::person_notice_enter"); COST_CRUMB("advisor_briefing_state",state);return message_enter(state,slot_ptr(18),false);}
static void simple_frame_body(Obj state) { COST_SCOPE("advisor_briefing::simple_frame"); COST_CRUMB("advisor_briefing_state",state);
    if(live_success_active()) {
        // PW17/PW19: only closing one of our own message states on the scene's own thread ends the window.
        if(live_success_other_thread()){live_success_pass(6);return;}
        const bool ours=std::find(simple_states.begin(),simple_states.end(),state)!=simple_states.end() && top_state()==state;
        if(!ours){live_success_pass(6);return;}
        live_success_end(4);
    }
#ifndef NO_RUNTIME_LOG
    hook_called(4);
#endif
    auto it=std::find(simple_states.begin(),simple_states.end(),state);
    if(it!=simple_states.end()) {
        // A completion callback may have stacked a new interaction. Never pop
        // whichever state happens to be on top in place of our own message.
        if(top_state()!=state)return;
        Obj dialog=pointer(state,0x470);
        bool diplomatic=pointer(state,0)==image+GA(GA_1462A78);
        if(!diplomatic)return; // General message states are always native.
        if(!dialogue_ready(false)) {
#ifndef NO_RUNTIME_LOG
            skip_decision(3,false,false);
#endif
            simple_states.erase(it);simple_state=nullptr;
            if(dialog)layout_position(dialog,0,0);return;
        }
        // Native +190 waits for the opening animation, sets the result and
        // starts closing the child layout (8614A2 -> 856840). Its bound event
        // then pops this state. Do not destroy it with a direct manager pop
        // while the original UI still owns animation/completion callbacks.
        if(!dialog || dword(dialog,0x178)!=0xFFFFFFFF){simple_states.erase(it);simple_state=nullptr;return;}
        Obj animation=pointer(dialog,0x188);
        if(!animation || dword(animation,0x178) || dword(animation,0x17C))return;
        if(diplomatic) {
            Obj window=pointer(dialog,0x1A8);if(!window || static_cast<int>(dword(window,0x22C))>0)return;
        }
        Obj messages=pointer(dialog,0x180);
        if(messages && *reinterpret_cast<const unsigned long long *>(messages+8)) {
            // Advance through the original page/log queue before requesting
            // close, including the diplomacy-specific percentage animation.
            fn<void(__fastcall *)(Obj)>(GA(GA_865FC0))(dialog);return;
        }
        simple_states.erase(it);simple_state=nullptr;
        // This can synchronously deliver the close callback. No dialog/state
        // access is allowed after the native acknowledgement returns.
        fn<void(__fastcall *)(Obj,unsigned)>(GA(GA_8637B0))(dialog,0x7FFFFFFD);
        return;
    }
    // Original slot is 92AD0 (void no-op). Original close event and Leave
    // 4F7170 own disposal and +48; YesNo/choices/input use other slots.
}
// Three slots (5, 15, 19) share the body; each keeps its own chain. A state that is not one of our message states goes on to the
// slot's next (originally the no-op 92AD0, so nothing changes with only this mod installed).
template<unsigned I> static void __fastcall simple_frame(Obj state) {
    const bool listed=std::find(simple_states.begin(),simple_states.end(),state)!=simple_states.end();
    simple_frame_body(state);
    if(!listed)if(auto next=slot_call<void(__fastcall *)(Obj)>(I))next(state);
}
// local273 (user-approved: the [수집 설정] switches also apply to the ten-day report lines): a report record's topic. The original's
// category byte (+0x86, the 936980 argument: 0 officers, 1 world affairs, 2 missions, 3 diplomacy, 4 plots, 5 military, 6 scouting and
// inspection) decides 외교·계략·정찰; the others take the wording, then the category's default.
static Topic report_line_topic(unsigned category,const std::wstring &text) {
    const Topic worded=topic_of_text(text);
    switch(category) {
    case 3:return worded==Topic::AttackRequest?worded:Topic::Diplomacy;
    case 4:return Topic::Plot;
    case 6:return Topic::Intel;
    default:break;
    }
    if(worded!=Topic::Other)return worded;
    return category==0?Topic::Officer:category==1?Topic::Rise:category==5?Topic::Army:Topic::Other;
}
// Report lines of switched-off topics are dropped (income tables too when 수입과 지출 is off).
static void report_filter(std::vector<Event> &batch) {
    batch.erase(std::remove_if(batch.begin(),batch.end(),[](const Event &e){return food_shortage_line(e.text) || !collects(topic_of_event(e));}),batch.end());
}
static bool record(Obj r,std::vector<Event> &batch) { COST_SCOPE("advisor_briefing:record");
    Event e;e.speaker=word(r,0x88);e.day=day();
    if(!copy(reinterpret_cast<const wchar_t *>(r+0x8A),e.text,0x55))return false;
    if(e.text.empty() && !copy(reinterpret_cast<const wchar_t *>(r+0x14),e.text,0x37))return false;
    if(!e.text.empty()){e.topic=static_cast<unsigned char>(report_line_topic(r[0x86],e.text));batch.push_back(std::move(e));}
    return true;
}
static bool pending_world_reports(std::vector<Event> &batch) { COST_SCOPE("advisor_briefing:pending_world_reports");
    Obj root=world();Obj context=root?pointer(root,0x871E8):nullptr;if(!context)return false;
    Obj district=fn<Obj(__fastcall *)(Obj)>(GA(GA_330B50))(context);
    unsigned district_id=district?fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F650))(district):0;
    for(unsigned i=0;i<2001;++i) {
        Obj r=pointer(root,0x79E90+i*8);if(!r)continue;
        unsigned flags=r[0x82],owner_id=r[0x12];if((flags&4) || !(flags&2))continue;
        Obj d=owner_id>0 && owner_id<52?pointer(root,0xE0D0+owner_id*8):nullptr;
        bool known=(flags&1) || (d && fn<int(__fastcall *)(Obj)>(GA(GA_2350F0))(d));
        bool include_detail=(flags&1) || owner_id==district_id;
        if(!known && !include_detail)continue;
        Event e;e.speaker=word(r,0x88);e.day=day();std::wstring first,second;
        if(known && !copy(reinterpret_cast<const wchar_t *>(r+0x14),first,0x37))return false;
        if(include_detail && r[0x86] && !copy(reinterpret_cast<const wchar_t *>(r+0x8A),second,0x55))return false;
        e.text=first;if(!second.empty() && second!=first){if(!e.text.empty())e.text+=L"\n";e.text+=second;}
        if(!e.text.empty()){e.topic=static_cast<unsigned char>(report_line_topic(r[0x86],e.text));batch.push_back(std::move(e));}
    }
    return true;
}
struct Text {Obj root;int id,x,y;std::wstring value;};
struct TextCapture {std::vector<Text> text;bool valid=true;Obj headless_root=nullptr;};
static thread_local TextCapture *capturing;
struct CaptureScope {
    TextCapture *previous;
    explicit CaptureScope(TextCapture &next):previous(capturing){capturing=&next;}
    ~CaptureScope(){capturing=previous;}
};
using TextSetter=void(__fastcall *)(Obj,int,const wchar_t *,unsigned long long);
static TextSetter original_text;
static CodeLink text_link;
static bool text_late_wanted;   // ic_gate: install() prepared text_link but left its publish to publish_late()
using SpeechSetter=void(__fastcall *)(Obj,int,Obj,const wchar_t *,Obj);
static SpeechSetter original_speech;
static CodeLink speech_link;
using EventSpeech=void(__fastcall *)(Obj,Obj,const wchar_t *,unsigned long long,const wchar_t *,unsigned);
static EventSpeech original_event_speech;
static CodeLink event_link;
static bool capture_event(Obj owner,Obj person,const wchar_t *text) { COST_SCOPE("advisor_briefing:capture_event");
    if(native_scene_active()){native_scene_pass(3);return false;}   // local220: CSearchDiscoveryState 75CA50 -> 844860 lines of a 원작대로 scene
    // PW17/PW19: the scene's own lines stay native and unrecorded; another owner's line ends the window first.
    if(live_success_match(owner)){live_success_pass(3);live_success.speaking=true;return false;}
    if(live_success_active()) {
        if(live_success_other_thread()){live_success_pass(3);return false;}
        live_success_end(4);
    }
    if(!entry_ready(3))return false;
    Obj table=owner?pointer(owner,0):nullptr;
    bool allowed=false;for(unsigned rva:event_owners)if(table==image+rva){allowed=true;break;}
    if(!message_gate(3,message_owner,allowed))return false;
    Event e;e.day=day();
    if(!message_gate(3,message_bad_text,copy(text,e.text) && !e.text.empty()))return false;
    if(table==image+GA(GA_1444080)) {
        unsigned phase=dword(owner,0x4C8);
        if(!message_gate(3,message_phase,phase>=1 && phase<=3 && person && person==pointer(owner,phase==1?0x4B0:0x4B8)))return false;
        e.kind=Kind::Recruitment;std::wstring target,envoy;
        Obj who=pointer(owner,0x4B8),from=pointer(owner,0x4B0);
        // PW19: the scene capture already stored this pair's success row for the day; show the line natively.
        if(phase>=2 && scene_row_stored(from,who))return false;
        if(who)copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(who),target,100);
        if(from)copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(from),envoy,100);
        if(phase==1)e.report=envoy.empty() || target.empty()?L"등용 임무의 결과를 보고드립니다.":envoy+L"의 "+target+L" 등용 임무를 보고드립니다.";
        else {
            e.report=(target.empty()?L"해당 인재":target)+L"의 등용에 "+(phase==2?L"성공했습니다.":L"실패했습니다.");
            e.row=recruitment_scene_row(envoy,target,phase==2);
        }
    }
    const bool capture_scene=mod_features.briefing16 && native_scene_line(e.text);
    if(food_shortage_line(e.text)){InterlockedExchange(&native_item,1);return false;}   // local273: 군량 부족 = the original window only
    const Topic topic=e.kind==Kind::Recruitment?(dword(owner,0x4C8)==3?Topic::EmployFail:Topic::Employ):duel_running()?Topic::Duel:topic_of_text(e.text);
    if(!topic_gate(topic))return false;   // local99; local220 phase 3 = failure
    e.topic=static_cast<unsigned char>(topic);
    name_speaker(e,person);
    if(!message_gate(3,message_store_failed,store({e})))return false;
    return !capture_scene;   // briefing16: the capture scene stays on screen; the briefing keeps the copy
}
static void __fastcall event_speech(Obj owner,Obj person,const wchar_t *text,unsigned long long flags,const wchar_t *extra,unsigned style) { COST_SCOPE("event_speech");
#ifndef NO_RUNTIME_LOG
    hook_called(6);
#endif
    note_duel(5);
    bool captured=capture_event(owner,person,text);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[3][captured?0:1]);
#endif
    // Capture returns before opening layout, 844030, push and 571EF0 together.
    // Relay preserves all six native arguments, including stack arguments 5/6.
    if(!captured)original_event_speech(owner,person,text,flags,extra,style);
}
static void __fastcall set_text(Obj root,int id,const wchar_t *text,unsigned long long flags) { COST_SCOPE("set_text");
    if(live_success_active()) {
        if(live_success_other_thread()){live_success_pass(4);original_text(root,id,text,flags);return;}
        live_success_end(4);   // 4 other message: a plain text set is not the scene's own line
    }
    if(capturing && capturing->valid) {
        std::wstring value;
        if(!copy(text,value) || capturing->text.size()>=2048)capturing->valid=false;
        else if(!value.empty() || root==capturing->headless_root) {
            auto &v=capturing->text;auto it=std::find_if(v.begin(),v.end(),[&](const Text &t){return t.root==root && t.id==id;});
            if(it!=v.end())it->value=std::move(value);
            else {int pos[2]{0,id};if(root!=capturing->headless_root)fn<void(__fastcall *)(Obj,int *,int)>(GA(GA_92EFC0))(root,pos,id);v.push_back({root,id,pos[0],pos[1],std::move(value)});}
        }
    }
    if(!capturing || root!=capturing->headless_root)original_text(root,id,text,flags);
}
static void __fastcall diplomatic_speech(Obj owner,int slot,Obj person,const wchar_t *text,Obj extra) { COST_SCOPE("diplomatic_speech");
    if(live_success_active()) {
        if(live_success_other_thread()){live_success_pass(5);original_speech(owner,slot,person,text,extra);return;}
        live_success_end(4);   // 4 other message: diplomatic speech is not the scene's own line
    }
    const bool wanted=collects(Topic::Diplomacy);   // local99
    bool capture=wanted && dialogue_ready(true) && owner==diplomatic_parent(true) && (slot==3 || slot==4);
    const bool watch=wanted && !capture && (slot==3 || slot==4) && dialogue_observed(true) && owner==diplomatic_observed(true);
#ifndef NO_RUNTIME_LOG
    if(slot==3 || slot==4)skip_decision(1,true,capture);
#endif
    Event e;e.kind=Kind::Diplomacy;e.day=day();
    if(watch) {   // the player decided: keep the line for the briefing, show it natively
        Event seen;seen.kind=Kind::Diplomacy;seen.day=day();
        if(copy(text,seen.text) && !seen.text.empty()) {
            const DiplomaticFacts facts=report_facts(owner,person);seen.report=diplomatic_report(seen.text,facts);seen.row=diplomatic_row(facts);
            if(person) {
                seen.speaker=word(person,0x10);std::wstring name;
                if(copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(person),name,100) && !name.empty())seen.text=name+L"\n"+seen.text;
            }
            store({seen},true);
        }
    }
    if(capture) {
        capture=copy(text,e.text) && !e.text.empty();
        if(capture){const DiplomaticFacts facts=report_facts(owner,person);e.report=diplomatic_report(e.text,facts);e.row=diplomatic_row(facts);}
        if(capture && person) {
            e.speaker=word(person,0x10);std::wstring name;
            if(copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(person),name,100) && !name.empty())e.text=name+L"\n"+e.text;
        }
        if(capture)capture=store({e},true);
#ifndef NO_RUNTIME_LOG
        if(capture)stall_begin(owner,5);
#endif
    }
    if(capture && speech_owner==owner) {
        Obj layout=pointer(owner,0x478);auto &w=speech_windows[slot-3];
        if(layout && pointer(layout,0x178+slot*8)==w.object)move(w,false);
    }
    original_speech(owner,slot,person,text,extra);
    if(capture && owner==diplomatic_parent(true)) {
        if(speech_owner!=owner)restore_speech();
        speech_owner=owner;Obj layout=pointer(owner,0x478);
        if(layout){auto &w=speech_windows[slot-3];bind(w,pointer(layout,0x178+slot*8));move(w,true);}
        speech_waiting=true;++speech_serial;
    }
}
static void __fastcall diplomatic_wait(Obj state) { COST_SCOPE("advisor_briefing::diplomatic_wait"); COST_CRUMB("advisor_briefing_state",state);
#ifndef NO_RUNTIME_LOG
    hook_called(5);
    InterlockedExchange64(&wait_last_state,reinterpret_cast<LONG64>(state));
    InterlockedExchange64(&wait_last_rva,state_rva(state));
    // Read-only equivalent of 56FE10(state); never call that native function.
    InterlockedExchange64(&wait_last_top_equal,top_state()==state);
#endif
#ifndef NO_RUNTIME_LOG
    bool advance=wait_condition(0,wait_top,top_state()==state) && wait_condition(0,wait_waiting,speech_waiting) && wait_owner_matches(0,speech_owner,diplomatic_parent(true)) && wait_ready_diagnostic(0,true);
    if(!advance)InterlockedIncrement64(&wait_false[0]);
#else
    bool advance=top_state()==state && speech_waiting && speech_owner==diplomatic_parent(true) && dialogue_ready(true);
#endif
    unsigned serial=speech_serial;
#ifndef NO_RUNTIME_LOG
    if(advance)skip_decision(2,true,true);
#endif
    if(advance) {
        // CInputWaitState's native timer is at +470, threshold +8. A zero
        // timeout requests the original auto-confirm path; its animation,
        // input-release gates, pop and bound coroutine callback still run.
        *reinterpret_cast<unsigned *>(state+0x478)=0;
#ifndef NO_RUNTIME_LOG
        InterlockedIncrement64(&wait_zero_writes);
#endif
    }
    slot_call<void(__fastcall *)(Obj)>(16)(state);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&wait_native_returns);
    if(top_state()!=state)InterlockedIncrement64(&wait_top_changed);
#endif
    if(advance && serial==speech_serial && top_state()!=state)speech_waiting=false;
}
// Only the 676870 -> 67AE80 call is redirected. A native UI fallback replays
// the prepared scalar/string payload, rather than applying report results twice.
static void __fastcall report_results(Obj action) { COST_SCOPE("report_results");
    live_success_end(3);   // 3 report: 군사보고 closes any live success record
    if(!replay_results){fn<void(__fastcall *)(Obj)>(GA(GA_67D540))(action);return;}
    for(unsigned i=0;i<262;++i) {
        unsigned offset=i<261?0x1B00+i*0x140:0x16140;
        memcpy(action+offset+0x10,replay_results+offset+0x10,0x130);
    }
    memcpy(action+0x16280,replay_results+0x16280,8);
}
struct ReportRecords {
    std::vector<unsigned char> bytes;
    ReportRecords():bytes(0x16290) {
        for(unsigned i=0;i<262;++i)fn<Obj(__fastcall *)(Obj)>(GA(GA_222E30))(bytes.data()+(i<261?0x1B00+i*0x140:0x16140));
    }
    ~ReportRecords(){for(unsigned i=262;i;) {--i;fn<void(__fastcall *)(Obj)>(GA(GA_223AD0))(bytes.data()+(i<261?0x1B00+i*0x140:0x16140));}}
};
#ifndef NO_RUNTIME_LOG
struct ReportTiming {
    unsigned phase;LARGE_INTEGER start{},frequency{};FILETIME created{},exited{},kernel{},user{};bool cpu_valid;
    explicit ReportTiming(unsigned p):phase(p) {
        QueryPerformanceCounter(&start);QueryPerformanceFrequency(&frequency);
        cpu_valid=GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user)!=FALSE;
    }
    static unsigned long long ticks(FILETIME t){return (static_cast<unsigned long long>(t.dwHighDateTime)<<32)|t.dwLowDateTime;}
    ~ReportTiming() {
        LARGE_INTEGER end{};FILETIME k{},u{};QueryPerformanceCounter(&end);
        bool valid=cpu_valid && GetThreadTimes(GetCurrentThread(),&created,&exited,&k,&u);
        unsigned wall=frequency.QuadPart?static_cast<unsigned>((end.QuadPart-start.QuadPart)*1000000/frequency.QuadPart):0;
        unsigned cpu=valid?static_cast<unsigned>((ticks(k)+ticks(u)-ticks(kernel)-ticks(user))/10):0;
        // phase 1=Enter, 2=headless completion, 3=Leave/callback;
        // value/hex are wall/CPU us. Nested phases must not be summed.
        // Thread CPU is quantized; valid=1 with zero still means < one tick.
        log_event("advisor_report_cost",phase,wall,cpu);
        log_event("advisor_report_cpu_valid",phase,valid?1:0,0);
    }
};
#endif
static bool report_districts(std::vector<Obj> &districts) { COST_SCOPE("advisor_briefing:report_districts");
    Obj root=world(),context=pointer(root,0x871E8);
    Obj force=fn<Obj(__fastcall *)(Obj)>(GA(GA_330B90))(context);
    if(!force)return false;
    for(unsigned i=0;i<52;++i) {
        Obj district=pointer(root,0xE0D0+i*8);
        if(!district || !fn<int(__fastcall *)(Obj)>(GA(GA_331780))(district))continue;
        unsigned owner=district[0x10];
        if(owner>=52 || pointer(root,0xDF30+owner*8)!=force)continue;
        if(district[0x11]>=sizeof(district_labels)/sizeof(*district_labels))return false;
        districts.push_back(district);
    }
    if(districts.empty() || districts.size()>8)return false;
    std::sort(districts.begin(),districts.end(),[](Obj a,Obj b){return a[0x11]<b[0x11];});
    for(size_t i=1;i<districts.size();++i)if(districts[i-1][0x11]==districts[i][0x11])return false;
    return true;
}
static bool headless_income(std::vector<Event> &batch,const std::vector<Obj> &districts) { COST_SCOPE("advisor_briefing:headless_income");
    // 67D1F0 uses only +378 of its UI owner. The zero-node layout makes its
    // optional 92EF10 visibility update a no-op (92CEE0's count guard).
    // Text setters are captured without creating nodes, querying coordinates,
    // updating textures or forwarding to the real UI setter.
    alignas(8) unsigned char income[0x380]{},child[0x178]{};
    *reinterpret_cast<Obj *>(income+0x378)=child;
    for(Obj district:districts) {
        TextCapture values;values.headless_root=child+0x130;
        {CaptureScope scope(values);fn<void(__fastcall *)(Obj,Obj)>(GA(GA_67F910))(income,district);}
        if(!values.valid)return false;
        std::vector<std::pair<int,std::wstring>> cells;
        for(const auto &t:values.text)if(t.root==values.headless_root)cells.push_back({t.id,t.value});
        // Native 67E32E explicitly clears military delta (38) when not
        // applicable. Retain that setter call and its empty final value;
        // a missing call or an empty balance is still a collection failure.
        for(int base:{10,19,28,37})for(int j=0;j<4;++j) {
            int id=base+j;
            auto cell=std::find_if(cells.begin(),cells.end(),[&](const auto &c){return c.first==id;});
            if(cell==cells.end() || (cell->second.empty() && id!=38)) {
#ifndef NO_RUNTIME_LOG
                log_event("advisor_report_income_missing",district[0x11],id,static_cast<unsigned>(cells.size()));
#endif
                return false;
            }
        }
        Event e;e.kind=Kind::Income;e.day=day();e.speaker=corps_face(district);
        if(!copy(fn<const wchar_t *(__fastcall *)(unsigned)>(GA(GA_30F010))(district_labels[district[0x11]]),e.text,100) || e.text.empty())return false;
        e.report=income_summary(e.text,cells);
        // Keep the original formatted cell strings in their row order alongside the
        // summary; the report shows the summary and never exposes internal control IDs.
        std::stable_sort(cells.begin(),cells.end(),[](const auto &a,const auto &b){return a.first<b.first;});
        for(const auto &c:cells)e.text+=L"\n"+c.second;
        if(e.text.size()>Inbox::text_limit || e.report.size()>Inbox::text_limit)return false;
        batch.push_back(std::move(e));
    }
    return true;
}
static bool headless_gate(unsigned reason) { COST_SCOPE("advisor_briefing:headless_gate");
#ifndef NO_RUNTIME_LOG
    log_event("advisor_report_headless_gate",reason,inbox.generation);
#endif
    (void)reason;return false;
}
static bool try_headless_report(Obj state,unsigned long long &result) { COST_SCOPE("advisor_briefing:try_headless_report");
    if(!report_results_call.published)return headless_gate(1);
    if(!defer(true))return headless_gate(2);
    // 671950 initializes phase=4 and background=null. It does NOT initialize
    // +488/+490; those belong to the skipped UI phases. Do not interpret
    // allocator bytes there as owned controls on a freshly constructed state.
    if(dword(state,0x470)!=4 || pointer(state,0x480))return headless_gate(3);
    std::vector<Event> batch;std::vector<Obj> districts;
    if(!pending_world_reports(batch))return headless_gate(4);
    if(!report_districts(districts))return headless_gate(5);
    // Reserve a full native result array before any result-producing call.
    if(inbox.events.size()+batch.size()+districts.size()+262>Inbox::limit)return headless_gate(6);
#ifndef NO_RUNTIME_LOG
    if(stored_event_bytes!=static_cast<LONG>(sizeof(Event)))return false;
    LARGE_INTEGER started{},frequency{};QueryPerformanceCounter(&started);QueryPerformanceFrequency(&frequency);
#endif
    ReportRecords records;Obj action=records.bytes.data();
    // Native 676870 consumes pending report bits before generating results.
    // Preserve that ordering, and undo only those bits for native UI fallback.
    std::vector<std::pair<Obj,unsigned char>> consumed;Obj root=world();
    for(unsigned i=0;i<2001;++i) {
        Obj r=pointer(root,0x79E90+i*8);
        if(r && !(r[0x82]&4) && (r[0x82]&2)) {
            consumed.push_back({r,r[0x82]});r[0x82]=(r[0x82]&0xFD)|4;
        }
    }
    fn<void(__fastcall *)(Obj)>(GA(GA_67D540))(action); // original result/flag writers, exactly once
    unsigned n=dword(action,0x16280);bool ok=n<=261;
    for(unsigned i=0;ok && i<n;++i)ok=record(action+0x1B00+i*0x140,batch);
    if(ok && dword(action,0x16284))ok=record(action+0x16140,batch);
    // Income is calculated AFTER the original result writers, just as native
    // phase 1 does; pre-result balances must never appear in the inbox.
    unsigned failed_stage=ok?0:1;
    if(ok){ok=headless_income(batch,districts);if(!ok)failed_stage=2;}
    if(ok)report_filter(batch);   // local273
    if(!ok || !store(std::move(batch),true)) {
        // Keep native controls and completion semantics on unknown data. The
        // caller-local replay protects irreversible result writers on retry.
        for(const auto &r:consumed)r.first[0x82]=(r.first[0x82]&0xF9)|(r.second&6);
        fallback();replay_results=action;
        result=slot_call<unsigned long long(__fastcall *)(Obj)>(6)(state);
        replay_results=nullptr;report_state=state;report_deferred=false;
#ifndef NO_RUNTIME_LOG
        log_event("advisor_report_collect_fail",failed_stage?failed_stage:3,n,static_cast<unsigned>(districts.size()));
        log_event("advisor_report_headless_fallback",n);
#endif
        (void)failed_stage;
        return true;
    }
    *reinterpret_cast<Obj *>(state+0x488)=nullptr;
    *reinterpret_cast<Obj *>(state+0x490)=nullptr;
    report_state=state;report_headless=true;report_deferred=false;
    *reinterpret_cast<unsigned *>(state+0x478)=0xFF; // no camera/audio transition was started
    *reinterpret_cast<unsigned *>(state+0x470)=3;
    *reinterpret_cast<unsigned *>(state+0x474)=3;
    result=1; // native Enter succeeds; manager finishes pushing before first frame
#ifndef NO_RUNTIME_LOG
    LARGE_INTEGER ended{};QueryPerformanceCounter(&ended);
    log_event("advisor_report_headless",n,static_cast<unsigned>(inbox.events.size()),frequency.QuadPart?static_cast<unsigned>((ended.QuadPart-started.QuadPart)*1000000/frequency.QuadPart):0);
#endif
    return true;
}
static unsigned long long __fastcall report_enter(Obj state) { COST_SCOPE("advisor_briefing::report_enter"); COST_CRUMB("advisor_briefing_state",state);
#ifndef NO_RUNTIME_LOG
    ReportTiming timing(1);
#endif
    unsigned long long headless_result=0;
    if(try_headless_report(state,headless_result))return headless_result;
    // Native reports are pushed after CUserStrategyState has already entered.
    // Accept only this final native report into the completed cycle.
    std::vector<Event> batch;bool capture=collects(Topic::Income) && defer(true) && pending_world_reports(batch);
    if(capture)briefing_overlay::mask_report(true);
    auto result=slot_call<unsigned long long(__fastcall *)(Obj)>(6)(state);
    report_state=state;report_deferred=false;
    if(!capture || !result){briefing_overlay::mask_report(false);return result;}
    Obj action=pointer(state,0x488);unsigned n=action?dword(action,0x16280):262;
    bool ok=n<=261;
    for(unsigned i=0;ok && i<n;++i)ok=record(action+0x1B00+i*0x140,batch);
    if(ok && dword(action,0x16284))ok=record(action+0x16140,batch);
    if(!ok){fallback();return result;}
    report_filter(batch);   // local273
    // A transaction: ordinary result generation/flag updates ran once above.
    if(!store(std::move(batch),true))return result;
    offscreen(pointer(state,0x480));offscreen(action);report_deferred=true;return result;
}
static void __fastcall report_frame(Obj state) { COST_SCOPE("advisor_briefing::report_frame"); COST_CRUMB("advisor_briefing_state",state);
#ifndef NO_RUNTIME_LOG
    hook_called(8);
#endif
    if(state==report_state && report_headless) {
        if(top_state()!=state)return;
#ifndef NO_RUNTIME_LOG
        ReportTiming timing(2);
#endif
        // Original phase 3 owns final audio cleanup, manager pop, Leave and
        // the bound parent callback. Never pop/reenter the manager from Enter.
        report_headless=false;
#ifndef NO_RUNTIME_LOG
        log_event("advisor_report_headless_finish",inbox.generation);
#endif
        slot_call<void(__fastcall *)(Obj)>(7)(state);return;
    }
    if(state==report_state && report_deferred) {
        if(top_state()!=state)return;
        if(!defer(true)){report_deferred=false;fallback();slot_call<void(__fastcall *)(Obj)>(7)(state);return;}
        report_deferred=false;
        TextCapture capture;
        // Preserve creation and disposal of the original income controls too.
        {CaptureScope scope(capture);fn<void(__fastcall *)(Obj,int)>(GA(GA_67F0B0))(state,1);}
        Obj income=pointer(state,0x490);std::vector<Event> batch;
        // Income values are filled by the district-selection callback, after
        // the constructor has created the headings. Run that same display-only
        // formatter for every district actually listed by the original UI.
        bool ok=income && capture.valid;
        for(unsigned i=0;ok && i<8;++i) {
            Obj district=pointer(income,0x288+i*8);if(!district)continue;
            if(district[0x11]>=sizeof(district_labels)/sizeof(*district_labels)){ok=false;break;}
            TextCapture values=capture;
            {CaptureScope scope(values);fn<void(__fastcall *)(Obj,Obj)>(GA(GA_67F910))(income,district);}
            Obj root=pointer(income,0x378);if(!root){ok=false;break;}
            values.text.erase(std::remove_if(values.text.begin(),values.text.end(),[&](const Text &t){return t.root!=root+0x130;}),values.text.end());
            bool number=false;for(const auto &t:values.text)for(wchar_t c:t.value)if(c>=L'0' && c<=L'9')number=true;
            if(!values.valid || !number){ok=false;break;}
            std::stable_sort(values.text.begin(),values.text.end(),[](const Text &a,const Text &b){return a.y!=b.y?a.y<b.y:a.x<b.x;});
            Event e;e.kind=Kind::Income;e.day=day();e.speaker=corps_face(district);
            if(!copy(fn<const wchar_t *(__fastcall *)(unsigned)>(GA(GA_30F010))(district_labels[district[0x11]]),e.text,100)){ok=false;break;}
            std::vector<std::pair<int,std::wstring>> cells;for(const auto &t:values.text)cells.push_back({t.id,t.value});
            e.report=income_summary(e.text,cells);
            e.text+=L"\n";int row=values.text.empty()?0:values.text.front().y;
            for(const auto &t:values.text) {
                if(e.text.size()+t.value.size()+2>Inbox::text_limit){ok=false;break;}
                if(t.y!=row){e.text+=L"\n";row=t.y;}
                e.text+=t.value+L"  ";
            }
            if(ok)batch.push_back(std::move(e));
        }
        if(!ok || batch.empty()) {
            // Unknown income encoding: retain the original report screen.
            fallback();layout_position(pointer(state,0x480),0,0);return;
        }
        if(!store(std::move(batch),true))return;
        offscreen(pointer(state,0x490));
        fn<void(__fastcall *)(Obj,int)>(GA(GA_67F0B0))(state,2); // original camera restoration
        fn<void(__fastcall *)(Obj,int)>(GA(GA_67F0B0))(state,3); // original report completion phase
        *reinterpret_cast<unsigned *>(state+0x474)=3; // no visible bg_out delay
        // Native 67B600 phase 3 applies its own final audio/camera cleanup and
        // manager pop. Leave 67B1E0 still disposes all controls and calls +48.
    }
    slot_call<void(__fastcall *)(Obj)>(7)(state);
}
static void __fastcall report_leave(Obj state,Obj context) { COST_SCOPE("advisor_briefing::report_leave"); COST_CRUMB("advisor_briefing_state",state);
#ifndef NO_RUNTIME_LOG
    ReportTiming timing(3);
#endif
    bool owned=state==report_state;
    if(owned){report_state=nullptr;report_deferred=false;report_headless=false;}
    slot_call<void(__fastcall *)(Obj,Obj)>(8)(state,context);
    if(owned && !report_state)briefing_overlay::mask_report(false);
}
static void __fastcall report_wait(Obj state) { COST_SCOPE("advisor_briefing::report_wait"); COST_CRUMB("advisor_briefing_state",state);if(defer(true) && top_state()==state){*reinterpret_cast<unsigned *>(state+0x470)=0;pop();return;}slot_call<void(__fastcall *)(Obj)>(9)(state);}
#ifndef NO_RUNTIME_LOG
static void diagnose_uncollected_top() { COST_SCOPE("advisor_briefing:diagnose_uncollected_top");
    if(!active() || !inbox.collecting)return;
    diagnostic_cycle();Obj state=top_state();unsigned rva=state_rva(state);
    if(rva==last_top_rva)return;last_top_rva=rva;
    if(!rva || std::find(collected_top_rvas.begin(),collected_top_rvas.end(),rva)!=collected_top_rvas.end())return;
    const char *name=reinterpret_cast<const char *>(state+0x70);size_t size=0;for(;size<128 && name[size];++size){}
    if(size==128)return;std::string value(name,size);
    for(const char *map:{"CStrategyState","CUserStrategyState","COtherStrategyState","CProgressState","CTurnState","CGameState","CRootState"})if(value==map)return;
    if(std::find(uncollected_top_rvas.begin(),uncollected_top_rvas.end(),rva)!=uncollected_top_rvas.end())return;
    uncollected_top_rvas.push_back(rva);
    log_event("advisor_briefing_uncollected_top",rva,day(),0);
}
// local21 diagnostics (U33, user 2026-11-10 "탐색 결과마다 렉"): every native direction state (CDirectionPlayerState,
// built by 3693E0 from 36E0C0; 36DBE0 plays the queued directions during progress) anywhere on the state stack.
// open: day, auto_search key, stack depth. close: ms on the stack, the longest gap between two frames of this hook
// while it was open (= the freeze), and how many directions opened in that game day. Diagnostic build only.
// GA_13B7268 = vtable of CDirectionPlayerState (address table, resolved by RTTI on a patched build).
struct DirectionTrack {Obj state=nullptr;ULONGLONG opened=0,last=0,gap=0;int day=-1;unsigned per_day=0;int count_day=-1;};
static DirectionTrack direction_track;
static void diagnose_direction(Obj m,Obj states,unsigned long long count) { COST_SCOPE("advisor_briefing:diagnose_direction");
    auto &d=direction_track;ULONGLONG now=GetTickCount64();
    Obj found=nullptr;
    if(m && count<=256 && states)for(unsigned n=0;n<count;++n){Obj s=pointer(states,n*8);if(s && pointer(s,0)==image+GA(GA_13B7268)){found=s;break;}}
    if(d.state && d.state!=found) {
        log_event("advisor_direction_close",static_cast<unsigned>(now-d.opened),static_cast<unsigned>(d.gap),d.per_day);
        d.state=nullptr;
    }
    if(found && !d.state) {
        int today=static_cast<int>(day());
        if(today!=d.count_day){d.count_day=today;d.per_day=0;}
        ++d.per_day;
        d.state=found;d.opened=now;d.gap=0;d.day=today;
        log_event("advisor_direction_open",static_cast<unsigned>(today),mod_features.auto_search?1u:0u,static_cast<unsigned>(count));
    }
    if(d.state && d.last && now-d.last>d.gap)d.gap=now-d.last;
    d.last=now;
}
#endif
// local232 (user 2026-11-18 "좌하단의 원작군사조언 치우자. 로그도 치우고 / 군사보고 체크하면 저두개 자동으로 치우고 로그도 군사보고에
// 탭하나 만들어서 거기에 넣어줘 / 여기서 치운다고 하는건 … 원작의 함수시작점을 조사해서 아예 리소스가 0으로 처리되게끔 가로채서
// 실행조차 안시키는걸 말해"; "군사보고 기본기능으로 넣자 이거 따로 모드체크 안만들고"): with the report on, the main layout's (vtable
// 13D90E0) bottom-left advice (CMainAdviceLayout, main+0x1E0) and log (CMainLogLayout, main+0x190) are hidden with both toggles
// (main+0x1D0 advice, +0x1D8 log; vt+0xC8 = 876580, the visible bit is +0x40 bit 0) and their frames do not run: the log frame 671E90
// (slot 10) is replaced by drain_native_log, which takes the queued lines exactly as 671E90 retires one (filter 66B3C0, 4FB20 into
// log+0x180, 4F9C0 off the queue log+0x190, +0x1EC=0) into the report's 로그 tab; the advice frame 88E4B0 (slot 12: +0x700 8A2380 and the
// 10-second advice rotation 88F890; it never writes the adviser +0x448) is skipped. No setting of its own: the report on = removed.
// The main layout still creates both (4452B0, once per strategy screen): its mode buttons 456270/456320 call into both toggles and both
// panels without a null check, 43CB80 reads the toggles for their tooltips, and 4465A0 hands the advice panel to the strategy screen for
// the eight rebuilds that local233 skips (advice_refresh).
struct NativeHud {
    Obj main=nullptr,part[4]{};bool hid[4]{};unsigned reshown[4]{};
    // the adviser's face for the report badge: 1 face file offered, 2 resident texture offered, 3 given up (logged)
    int face_serial=-1;unsigned face_path=0;ULONGLONG face_started=0,face_tried=0,resident_from=0,hold_started=0;
    bool hold=false;   // the advice panel is shown (and its frame runs) until the face is taken: the old capture path, at most 4 s
    unsigned drained=0,filtered=0,bad_text=0,lines_logged=0,stops_logged=0,skips_logged=0,advice_skipped=0,advice_skipped_shown=0;
};
static NativeHud native_hud;
static constexpr unsigned kNativeParts[4]={0x190,0x1E0,0x1D0,0x1D8};   // log, advice, advice toggle, log toggle
static bool remove_native(){return active() && !inbox.fallback && briefing_overlay::available() && !tutorial();}
// The log layout's parent, only when it is the main layout holding this log; else the original path (logged once per reason).
static Obj native_main(Obj log) {
    Obj main=pointer(log,0x8);unsigned why=0;
    if(!main)why=1;
    else if(pointer(main,0)!=image+GA(GA_13D90E0))why=2;
    else if(pointer(main,0x190)!=log)why=3;
    if(!why)return main;
#ifndef NO_RUNTIME_LOG
    if(remove_native() && !(native_hud.skips_logged&(1u<<why))){native_hud.skips_logged|=1u<<why;log_event("advisor_native_hud_skip",why);}
#endif
    return nullptr;
}
static bool hud_part_shown(Obj p){return p && (dword(p,0x40)&1);}
static void hud_part_show(Obj p,int on){(*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(p,0)+0xC8))(p,on);}
static void native_hide(Obj main) {
    NativeHud &h=native_hud;
    if(h.main!=main){h.main=main;for(unsigned i=0;i<4;++i){h.part[i]=nullptr;h.hid[i]=false;}}
    for(unsigned i=0;i<4;++i) {
        Obj p=pointer(main,kNativeParts[i]);
        if(!p)continue;
        if(h.part[i]!=p){h.part[i]=p;h.hid[i]=false;}
        if(i==1 && h.hold){if(!hud_part_shown(p))hud_part_show(p,1);continue;}   // the face hold: the panel draws its face once more
        if(!hud_part_shown(p))continue;
        if(h.hid[i]) {
            ++h.reshown[i];
#ifndef NO_RUNTIME_LOG
            if(h.reshown[i]<=5)log_event("advisor_native_reshow",i,h.reshown[i]);   // the game showed it again (a mode switch, a refresh)
#endif
        }
        hud_part_show(p,0);h.hid[i]=true;
    }
    move(old_log,true);move(old_advice,!h.hold);   // off screen too, so a re-show by the game draws nothing until the next frame hides it
}
// Back to the original HUD (report off, fallback, tutorial): the toggles, then the panel of the mode the log toggle holds (+0x40 bit 0x100).
static void native_restore(Obj main) {
    NativeHud &h=native_hud;
    if(!h.main)return;
    if(main==h.main) {
        Obj toggle=pointer(main,0x1D8);const bool log_mode=toggle && (dword(toggle,0x40)&0x100);unsigned mask=0;
        const bool panels=h.hid[0] || h.hid[1] || h.hold;
        for(unsigned i=0;i<4;++i) {
            Obj p=pointer(main,kNativeParts[i]);
            if(!p || p!=h.part[i])continue;
            // toggles: as hidden; log mode: the log if it was shown (456320 shows it only below a limit), advice mode: the advice
            const bool show=i>=2?h.hid[i]:i==0?log_mode && h.hid[0]:!log_mode && panels;
            if(show && !hud_part_shown(p)){hud_part_show(p,1);mask|=1u<<i;}
            else if(i==1 && log_mode && h.hold && hud_part_shown(p)){hud_part_show(p,0);mask|=0x10;}   // shown by the face hold
        }
#ifndef NO_RUNTIME_LOG
        log_event("advisor_native_restore",mask,log_mode?1u:0u,h.reshown[0]+h.reshown[1]+h.reshown[2]+h.reshown[3]);
#endif
    }
    move(old_log,false);move(old_advice,false);
    h.main=nullptr;for(unsigned i=0;i<4;++i){h.part[i]=nullptr;h.hid[i]=false;}h.hold=false;
}
// One frame of the original log's work without its drawing: every queued line (at most 64 a frame) goes to native_log.
// stop: 0 empty, 1 no queue handle, 2 no handle pool, 3 handle out of range, 4 null entry at the head, 5 more next frame.
static void drain_native_log(Obj log) { COST_SCOPE("advisor_briefing:drain_native_log");
    GameList &queue=*reinterpret_cast<GameList *>(log+0x190);
    std::vector<Event> got;std::wstring chars;unsigned stop=0,taken=0;
    for(;;) {
        if(!queue.handle){stop=1;break;}
        if(!pointer(image,GA(GA_27D4138)) || !pointer(image,GA(GA_27D4140))){stop=2;break;}
        if(*queue.handle>=dword(image,GA(GA_27D4170))){stop=3;break;}
        Link *node=list_first(queue);
        if(!node)break;
        Obj entry=node->value;
        if(!entry){stop=4;break;}
        if(taken>=64){stop=5;break;}
        // 66B3C0: all six category switches off = every line; else a valid entry (331780) of a switched-on category (+0x86).
        bool keep=true;
        if(dword(log,0x1F0) || dword(log,0x200) || dword(log,0x1F8) || dword(log,0x1FC) || dword(log,0x1F4) || dword(log,0x204)) {
            static constexpr unsigned flag[6]={0x204,0x1F0,0x200,0x1F8,0x1FC,0x1F4};   // default, categories 1..5
            const unsigned category=*reinterpret_cast<const unsigned char *>(entry+0x86);
            keep=fn<int(__fastcall *)(Obj)>(GA(GA_331780))(entry) && dword(log,flag[category>=1 && category<=5?category:0]);
        }
        if(keep) {
            Event e;
            if(copy(reinterpret_cast<const wchar_t *>(entry+0x14),e.text,56) && !e.text.empty()) {
                const unsigned person=word(entry,0x88);
                e.kind=classify(e.text);e.read=true;e.day=0;e.speaker=person && person<6001?person:0;   // no day: the log runs across cycles
                // local235: the line's place as the original click reads it (671260): +0x82 bit 0x10, then the hex at +0x84 (48400 = none)
                if(*reinterpret_cast<const unsigned char *>(entry+0x82)&0x10){const unsigned hex=word(entry,0x84);if(hex<48400)e.hex=hex;}
#ifndef NO_RUNTIME_LOG
                if(native_hud.lines_logged<20){++native_hud.lines_logged;log_event("advisor_log_line",*reinterpret_cast<const unsigned char *>(entry+0x86),person,static_cast<unsigned>(e.text.size())+(e.hex<48400?1000u*(1u+e.hex):0u));}   // local235: c = text length (at most 56) + 1000*(hex+1) when the line has a place
#endif
                chars+=e.text;got.push_back(std::move(e));
            } else ++native_hud.bad_text;
        } else ++native_hud.filtered;
        Obj value=entry;   // retired as 671E90 retires a line: into log+0x180, then off the queue
        fn<void(__fastcall *)(Obj,Obj *)>(GA(GA_4FB20))(log+0x180,&value);
        fn<void(__fastcall *)(Obj)>(GA(GA_4F9C0))(log+0x190);
        *reinterpret_cast<int *>(log+0x1EC)=0;
        ++taken;
    }
    native_hud.drained+=taken;
    if(!got.empty()) {
        AcquireSRWLockExclusive(&native_log_lock);
        for(auto &e:got)native_log.push_back(std::move(e));
        if(native_log.size()>kNativeLogKept)native_log.erase(native_log.begin(),native_log.end()-static_cast<std::ptrdiff_t>(kNativeLogKept));
        native_log_glyphs+=chars;++log_generation;
        ReleaseSRWLockExclusive(&native_log_lock);
    }
#ifndef NO_RUNTIME_LOG
    if(stop && stop!=5 && native_hud.stops_logged<10){++native_hud.stops_logged;log_event("advisor_native_log_stop",stop,taken,native_hud.drained);}
    if(taken && (native_hud.drained/100)!=((native_hud.drained-taken)/100))log_event("advisor_native_log",native_hud.drained,native_hud.filtered,native_hud.bad_text);
#endif
}
// local233 (user 2026-11-18 "원작에서 조언 로그 딱히 그부분 읽는거 못느꼈는데 어디서 무슨용도로 읽어?"): besides its frame, the original
// rebuilds the advice list after eight strategy events (lord change 28F910 and seven state handlers): each takes the advice layout from
// the strategy screen (4420E0: CUserStrategyState +0x478 -> +0xA0, cached there by 4465A0) and calls 88F670, which clears the list, runs
// the 11.5 KB builder 8894B0 (it also picks the adviser +0x448 with 2B6490 of the player's force) and shows the first line (88F890).
// While the report has the advice those eight calls do nothing: the adviser is picked here the same way, and the panel's next own frame
// (the report gives it back, or the face hold) rebuilds the list once. The two calls at creation (885270, 4465A0) still build it.
namespace advice_refresh {
using Refresh=void(__fastcall *)(Obj,Obj,Obj,Obj);
static constexpr unsigned kSites=8;
static CallLink links[kSites];
static bool stale;        // game thread: a rebuild was skipped since the last one
static Obj adviser;       // game thread: the adviser picked at the last skipped rebuild (nullptr: none found, +0x448 is read)
static unsigned skipped,rebuilt;
static Obj pick_adviser() {   // 8894B0's pick: 2B6490(230270(330B90(options)),0,0), each step valid
    auto valid=[](Obj o){return o && fn<int(__fastcall *)(Obj)>(GA(GA_331780))(o);};
    Obj root=world(),options=root?pointer(root,0x871E8):nullptr;
    Obj lord=options?fn<Obj(__fastcall *)(Obj)>(GA(GA_330B90))(options):nullptr;
    if(!valid(lord))return nullptr;
    Obj force=fn<Obj(__fastcall *)(Obj)>(GA(GA_230270))(lord);
    Obj picked=force?fn<Obj(__fastcall *)(Obj,Obj,int,Obj)>(GA(GA_2B6490))(force,nullptr,0,nullptr):nullptr;
    return valid(picked)?picked:nullptr;
}
static void __fastcall refresh(Obj panel,Obj a2,Obj a3,Obj a4) { COST_SCOPE("advice_refresh:refresh");
    if(panel && native_hud.main && native_hud.part[1]==panel && remove_native()) {
        stale=true;adviser=pick_adviser();++skipped;
#ifndef NO_RUNTIME_LOG
        if(skipped<=3 || skipped%50==0)log_event("advisor_native_advice_rebuild_skip",skipped,adviser?word(adviser,0xA6):~0u,rebuilt);
#endif
        return;
    }
    fn<Refresh>(GA(GA_88F670))(panel,a2,a3,a4);
    if(panel && panel==native_hud.part[1]){stale=false;adviser=nullptr;}
}
// The panel's own frame is about to run (88E4B0): first the rebuild that was skipped.
static void rebuild(Obj panel) { COST_SCOPE("advice_refresh:rebuild");
    if(!stale)return;
    stale=false;adviser=nullptr;++rebuilt;
    fn<Refresh>(GA(GA_88F670))(panel,nullptr,nullptr,nullptr);
#ifndef NO_RUNTIME_LOG
    log_event("advisor_native_advice_rebuild",rebuilt,skipped);
#endif
}
static Obj current(Obj panel){return stale && adviser?adviser:pointer(panel,0x448);}
static void install() { COST_SCOPE("advice_refresh:install");
    const unsigned rebuild_sites[kSites]={GA(GA_28FCE4),GA(GA_60D702),GA(GA_6254A7),GA(GA_6CD573),GA(GA_6D5987),GA(GA_7216CF),GA(GA_7222AF),GA(GA_83CB38)};
    const unsigned target=GA(GA_88F670);
    bool ok=target && GA(GA_2B6490) && GA(GA_230270) && GA(GA_330B90) && GA(GA_331780);unsigned bad=0;
    for(unsigned i=0;ok && i<kSites;++i) {
        int delta=0;if(rebuild_sites[i])memcpy(&delta,image+rebuild_sites[i]+1,4);
        if(!rebuild_sites[i] || image[rebuild_sites[i]]!=0xE8 || rebuild_sites[i]+5+static_cast<unsigned>(delta)!=target){ok=false;bad=i+1;}
    }
    Obj relay=ok?allocate_near(image+rebuild_sites[0]):nullptr;
    if(!relay){log_event("advisor_native_advice_rebuild_rejected",ok?1u:0u,bad,0);return;}
    absolute_jump(relay,reinterpret_cast<void *>(&refresh));
    DWORD old;unsigned n=0;
    ok=VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) && FlushInstructionCache(GetCurrentProcess(),relay,16);
    for(;ok && n<kSites;++n)ok=call_prepare(links[n],rebuild_sites[n],relay);
    for(unsigned i=0;ok && i<kSites;++i)ok=call_publish(links[i]);
    if(!ok){for(unsigned i=kSites;i;)call_restore(links[--i]);VirtualFree(relay,0,MEM_RELEASE);}
    log_event(ok?"advisor_native_advice_rebuild_installed":"advisor_native_advice_rebuild_rejected",2,n,0);
}
}   // namespace advice_refresh
static void native_adviser_tick(Obj advice_layout);   // after the face files
static void __fastcall log_frame(Obj layout) { COST_SCOPE("advisor_briefing::log_frame"); COST_CRUMB("advisor_briefing_layout",layout);
#ifndef NO_RUNTIME_LOG
    FrameTiming timing(1);
#endif
#ifndef NO_RUNTIME_LOG
    hook_called(1);
#endif
    bind(old_log,layout);
    Obj main=native_main(layout);
    if(main && remove_native()) {
        // local232: 671E90 does not run; only its reset branch (+0x208 set, no +0x1D8: 66D990, +0x208=0) is left to it.
        if(dword(layout,0x208) && !pointer(layout,0x1D8))slot_call<void(__fastcall *)(Obj)>(10)(layout);
        else drain_native_log(layout);
        if(Obj advice_layout=pointer(main,0x1E0)){bind(old_advice,advice_layout);native_adviser_tick(advice_layout);}
        native_hide(main);
    } else {
        native_restore(main);
        slot_call<void(__fastcall *)(Obj)>(10)(layout);move(old_log,hide_native_hud() && replace_legacy());
    }
    // Saved player-turn and YesNo snapshots both show this existing hook running.
    // Only read native state here; Present/WndProc consume the published decision.
    if(active() && !inbox.collecting){
        Obj top=top_state();bool player=top && pointer(top,0)==image+GA(GA_13D9430) && !interaction();
#ifndef NO_RUNTIME_LOG
        InterlockedIncrement64(&player_context_counts[0]);
        InterlockedIncrement64(&player_context_counts[player?1:2]);
        if(!player && open)InterlockedIncrement64(&player_context_counts[3]);
#endif
        if(!player){open=false;rendered_open=false;display_cache.open=false;}
        briefing_overlay::player_context(player);
    }
    // UI-thread flags only: no strings/model rebuild on every log frame.
    if(active() && inbox.has_cycle) {
        bool show=!interaction();
        briefing_overlay::progress_flags(inbox.collecting && show,clean_scene(),inbox.collecting);
    }
#ifndef NO_RUNTIME_LOG
    // Compare addresses with the live stack; never read a removed state.
    Obj m=manager(),states=m?pointer(m,0x20):nullptr;
    auto count=m?*reinterpret_cast<const unsigned long long *>(m+0x10):257;
    if(count<=256 && (!count || states))for(size_t i=0;i<stalls.size();) {
        Obj state=stalls[i].state;bool found=false;
        for(unsigned n=0;n<count;++n)if(pointer(states,n*8)==state){found=true;break;}
        if(found)++i;else stall_end(state);
    }
    diagnose_uncollected_top();
    diagnose_direction(m,states,count);
#endif
}
static void __fastcall log_leave(Obj layout,Obj context) { COST_SCOPE("advisor_briefing::log_leave"); COST_CRUMB("advisor_briefing_layout",layout);
    // local232: the HUD is being torn down (sibling order unknown): forget it without calling into its parts.
    if(native_hud.main && native_hud.part[0]==layout){native_hud.main=nullptr;for(unsigned i=0;i<4;++i){native_hud.part[i]=nullptr;native_hud.hid[i]=false;}native_hud.hold=false;}
    if(old_log.object==layout){move(old_log,false);old_log={};}
    slot_call<void(__fastcall *)(Obj,Obj)>(11)(layout,context);
}
// U6 (user 2026-11-09 "캡처 말고 군사 초상화를 직접 불러오는 방식"): the face manager (GA_27D73E0) entry of the art,
// 8AA550(manager,10,art) -> index into manager+0x178, holds the face as a 512x512 BC3 texture one level below its
// +0x10 / +0x08 object at +0xD8. Evidence: local11/12 PW26 dumps advisor_face_<art>_0100001B (depth 1, slot 0x1B) were
// 관우 for art 2 and 제갈량 for art 45. Every pointer is read under SEH and the object must be a d3d11 COM object of that
// exact shape, so a wrong guess only falls back to the capture path. Returns an AddRef'd texture or null.
static uintptr_t face_d3d_low,face_d3d_high;
// local36 diagnostic: why the 512 face was not taken, per probed slot (+0x10, +0x08): 0 no object, 1 not a d3d11 object,
// 2 no texture interface, else format<<24 | width<<12 | height; 0xFFFFFFFF = slot object missing.
static unsigned face_last_shape[2];
static unsigned face_probe_shape;
static ID3D11Texture2D *face_texture_from(Obj object) {
    face_probe_shape=0;
    if(!object)return nullptr;
    __try {
        const uintptr_t vt=*reinterpret_cast<uintptr_t *>(object);
        face_probe_shape=1;
        if(vt<face_d3d_low || vt>=face_d3d_high)return nullptr;
        face_probe_shape=2;
        IUnknown *unknown=reinterpret_cast<IUnknown *>(object);
        ID3D11Texture2D *texture=nullptr;ID3D11ShaderResourceView *view=nullptr;
        if(SUCCEEDED(unknown->QueryInterface(__uuidof(ID3D11ShaderResourceView),reinterpret_cast<void **>(&view))) && view) {
            ID3D11Resource *resource=nullptr;view->GetResource(&resource);view->Release();
            if(resource){resource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&texture));resource->Release();}
        } else unknown->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&texture));
        if(!texture)return nullptr;
        D3D11_TEXTURE2D_DESC desc;ZeroMemory(&desc,sizeof(desc));texture->GetDesc(&desc);
        face_probe_shape=(static_cast<unsigned>(desc.Format&0xFF)<<24)|((desc.Width&0xFFF)<<12)|(desc.Height&0xFFF);
        if(desc.Width==512 && desc.Height==512 && desc.Format>=DXGI_FORMAT_BC3_TYPELESS && desc.Format<=DXGI_FORMAT_BC3_UNORM_SRGB)return texture;
        texture->Release();
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    return nullptr;
}
static Obj face_slot(Obj base,unsigned offset) {
    __try {Obj value=*reinterpret_cast<Obj *>(base+offset);return value;} __except(EXCEPTION_EXECUTE_HANDLER) {return nullptr;}
}
static ID3D11Texture2D *face_texture_for(unsigned art,unsigned &where) { COST_SCOPE("advisor_briefing:face_texture_for");
    where=0;
    if(!face_d3d_high) {
        HMODULE module=GetModuleHandleW(L"d3d11.dll");MODULEINFO info{};
        if(!module || !GetModuleInformation(GetCurrentProcess(),module,&info,sizeof(info)) || !info.lpBaseOfDll)return nullptr;
        face_d3d_low=reinterpret_cast<uintptr_t>(info.lpBaseOfDll);face_d3d_high=face_d3d_low+info.SizeOfImage;
    }
    Obj manager=pointer(image,GA(GA_27D73E0));if(!manager){where=1;return nullptr;}
    const int id=fn<int(__fastcall *)(Obj,int,int)>(GA(GA_8AA550))(manager,10,static_cast<int>(art));
    if(id<0){where=2;return nullptr;}
    Obj t=face_slot(manager,0x178);Obj e=t?face_slot(t,static_cast<unsigned>(id)*8):nullptr;if(!e){where=3;return nullptr;}
    unsigned probe=0;
    for(unsigned offset:{0x10u,0x8u}) {
        Obj start=face_slot(e,offset);face_probe_shape=0;
        ID3D11Texture2D *texture=start?face_texture_from(face_slot(start,0xD8)):nullptr;
        face_last_shape[probe++]=start?face_probe_shape:0xFFFFFFFFu;
        if(texture){where=offset;return texture;}
    }
    where=4;return nullptr;
}
// local223 (user 2026-11-17 "처음켰을땐 렉이좀 심해 끊김도 있고", "전령이 고순 초상화로 고정"): a row face is taken only when the game
// already holds it: its reference count is above 0 (+0x180 table) and its texture is made (+0x178[id] -> +0x10 object -> +0xD8, a
// d3d11 512 BC3 texture). No load is asked for. A load (8A8FA0 at count 0) runs on this thread and waits for its job, about a frame per
// face, and a face the game has not drawn yet still has no texture after it (+0xD8 is not a d3d11 object until the original binds the
// image to a node it draws), so local222 stalled the first open and then timed out (log where 104). The +0x08 slot, the load job, is
// never read: a finished job can be freed and its memory reused by another face's load (the messenger row showed 고순).
static int face_refs(Obj manager,int id) {
    __try {
        Obj t=*reinterpret_cast<Obj *>(manager+0x180);if(!t)return -1;
        const int first=*reinterpret_cast<int *>(t+8),n=*reinterpret_cast<int *>(t+0xC);
        const int *counts=*reinterpret_cast<int *const *>(t+0x10);
        if(!counts || id<first || id>=first+n)return -1;
        return counts[id-first];
    } __except(EXCEPTION_EXECUTE_HANDLER) {return -1;}
}
static ID3D11Texture2D *resident_face(unsigned art,unsigned &where) { COST_SCOPE("advisor_briefing:resident_face");
    where=0;
    if(!face_d3d_high) {
        HMODULE module=GetModuleHandleW(L"d3d11.dll");MODULEINFO info{};
        if(!module || !GetModuleInformation(GetCurrentProcess(),module,&info,sizeof(info)) || !info.lpBaseOfDll)return nullptr;
        face_d3d_low=reinterpret_cast<uintptr_t>(info.lpBaseOfDll);face_d3d_high=face_d3d_low+info.SizeOfImage;
    }
    Obj manager=pointer(image,GA(GA_27D73E0));if(!manager){where=1;return nullptr;}
    const int id=fn<int(__fastcall *)(Obj,int,int)>(GA(GA_8AA550))(manager,10,static_cast<int>(art));   // table lookup only
    if(id<0){where=2;return nullptr;}
    if(face_refs(manager,id)<=0){where=5;return nullptr;}   // not held by the game: no texture to take
    Obj t=face_slot(manager,0x178);Obj e=t?face_slot(t,static_cast<unsigned>(id)*8):nullptr;if(!e){where=3;return nullptr;}
    Obj object=face_slot(e,0x10);
    ID3D11Texture2D *texture=object?face_texture_from(face_slot(object,0xD8)):nullptr;
    if(!texture){where=4;return nullptr;}
    where=0x10;return texture;
}
#ifndef NO_RUNTIME_LOG
// local253 diagnostic (user 2026-11-19: 출진부대설정 portraits all blank; fine in v16.3.1). The sortie rows set faces with the original
// 85AE70 -> 932DD0(node+0x178, kind 10, face), which no mod hooks. Every 3 s on the UI thread this reads the face manager: ids the game
// holds (count > 0 in +0x180), of those how many have their texture made (+0x178[id] -> +0x10 -> +0xD8 a 512 BC3 d3d11 texture) and
// how many have an entry but no texture yet. face_survey(held, textured | entries<<16, untextured | eligible<<28 | modal<<29) and,
// when held ids lack a texture, face_survey_missing(first three such ids, packed 3x10 bits). Read only: no game function is called.
static void face_manager_survey(bool eligible,bool modal) {
    static ULONGLONG next;const ULONGLONG now=GetTickCount64();
    if(now<next)return;next=now+3000;
    if(!face_d3d_high) {
        HMODULE module=GetModuleHandleW(L"d3d11.dll");MODULEINFO info{};
        if(!module || !GetModuleInformation(GetCurrentProcess(),module,&info,sizeof(info)) || !info.lpBaseOfDll)return;
        face_d3d_low=reinterpret_cast<uintptr_t>(info.lpBaseOfDll);face_d3d_high=face_d3d_low+info.SizeOfImage;
    }
    Obj manager=pointer(image,GA(GA_27D73E0));if(!manager)return;
    Obj counts_table=face_slot(manager,0x180),entries=face_slot(manager,0x178);
    if(!counts_table || !entries)return;
    int first=0,n=0;
    __try {first=*reinterpret_cast<int *>(counts_table+8);n=*reinterpret_cast<int *>(counts_table+0xC);} __except(EXCEPTION_EXECUTE_HANDLER) {return;}
    if(n<=0 || n>200000)return;
    unsigned held=0,textured=0,with_entry=0,untextured=0,missing[3]={};
    for(int id=first;id<first+n;++id) {
        if(face_refs(manager,id)<=0)continue;
        ++held;
        Obj e=face_slot(entries,static_cast<unsigned>(id)*8);if(!e)continue;
        ++with_entry;
        Obj object=face_slot(e,0x10);
        ID3D11Texture2D *texture=object?face_texture_from(face_slot(object,0xD8)):nullptr;
        if(texture){texture->Release();++textured;}
        else {if(untextured<3)missing[untextured]=static_cast<unsigned>(id)&0x3FF;++untextured;}
    }
    log_event("face_survey",held,textured|(with_entry<<16),untextured|(eligible?1u<<28:0u)|(modal?1u<<29:0u));
    if(untextured)log_event("face_survey_missing",missing[0]|missing[1]<<10|missing[2]<<20,untextured,static_cast<unsigned>(n));
}
// local258 diagnostic (user 2026-11-19 "튕겼는데 게임이": c0000005 at exe+22B5CF). 22B360(force) tallies the force's officers in
// status 2..4 (+0xE6) by principle (+0x14C, 1..6) into a 6-slot stack array indexed principle-1, so an officer whose principle is 0
// or above 6 writes outside it. No mod code writes +0x14C or +0xE6. Every 10 s on the UI thread this reads persons 0..6000 and logs
// person_principle_bad(id, principle | status<<8 | force<<16, count | in_force<<16) when the counts change: up to three such officers,
// those of a force 1..50 first (22B360 counts only its own force's officers: person +0xE0 corps slot -> corps table W+0xE0D0 -> +0x10
// force, the lookup 22E870 makes; force 51 = 도적 incl. the band commanders 4033~4100, 0 = none). in_force = how many sit in 1..50.
// Read only: no game function is called.
static void person_principle_survey() {
    static ULONGLONG next;static unsigned last=~0u;const ULONGLONG now=GetTickCount64();
    if(now<next)return;next=now+10000;
    Obj root=world();if(!root)return;
    unsigned bad=0,in_force=0,shown=0,ids[3]={},vals[3]={},spare_ids[3]={},spare_vals[3]={},spare=0;
    for(unsigned id=0;id<0x1771;++id) {
        unsigned char principle=0,status=0,force=0;
        __try {
            const unsigned char *person=*reinterpret_cast<unsigned char *const *>(root+(id+0x2B)*8);
            if(!person)continue;
            status=person[0xE6];principle=person[0x14C];
            const unsigned slot=person[0xE0];
            const unsigned char *corps=*reinterpret_cast<unsigned char *const *>(root+0xE0D0+(slot<0x34?slot:0)*8);
            force=corps?corps[0x10]:0;
        } __except(EXCEPTION_EXECUTE_HANDLER) {return;}
        if(status<2 || status>4 || (principle>=1 && principle<=6))continue;
        const unsigned value=principle|status<<8|static_cast<unsigned>(force)<<16;
        ++bad;
        if(force>=1 && force<=50){++in_force;if(shown<3){ids[shown]=id;vals[shown]=value;++shown;}}
        else if(spare<3){spare_ids[spare]=id;spare_vals[spare]=value;++spare;}
    }
    const unsigned key=bad|in_force<<16;
    if(key==last)return;
    last=key;
    if(!bad){log_event("person_principle_bad",0,0,0);return;}
    for(unsigned i=0;i<spare && shown<3;++i){ids[shown]=spare_ids[i];vals[shown]=spare_vals[i];++shown;}
    for(unsigned i=0;i<shown;++i)log_event("person_principle_bad",ids[i],vals[i],key);
}
#endif
// local224 face files (user 2026-11-17 "무장목록같은거 보면 아무리 목록이 길어도 초상화 그때그때 자연스럽게 불러오는데 원작의 그 함수를
//찾아서 우리도"): the original list gets a face texture by binding the image to a node it draws (8744D0 -> 8A8FA0, and the renderer
// makes the texture only for drawn images: local223 logs had faces the game holds without a texture, where 4). The report draws no
// native node, so the heads come from the archive files instead. A worker thread of ours builds the face index once per run (the
// sprite tables of 0002/ScreenLayout.rdb, about 7,000 small reads, 0.3 s in Python) and decodes the asked faces (one ~110 KB entry
// each). It only reads files: no game memory, no game function, no log_event (the log reads the game day; the hud logs for it).
// The hud asks by face number (person +0xA6) and takes the decoded heads (face_file). A face not in the files (a custom face) or a
// failed index falls back to the game's resident texture (resident_face).
// Faces 6000..6214 (extra portraits a new officer can take) come from 0005\000..002.bin instead (pack = the file); local233: 6215..6244
// from DLC\0005\003.bin, the 19 age faces (2179..2197, face_NNNNb_ctr) and the two tables whose texture the index keeps in another bank.
struct FaceSpot {std::uint32_t bank=0;int sub=-1;std::uint64_t offset=0,size=0;unsigned page=0;int pack=-1;};
struct FaceFileDone {unsigned art=0;bool ok=false;std::vector<std::uint8_t> pixels;};
static SRWLOCK face_file_lock=SRWLOCK_INIT;
static std::vector<unsigned> face_file_wanted;      // under face_file_lock: asked, not taken by the worker yet
static std::vector<FaceFileDone> face_file_done;    // under face_file_lock: decoded, not taken by the hud yet
static volatile LONG face_file_state;               // 0 not started, 1 indexing, 2 ready, 3 failed
static std::map<unsigned,FaceSpot> face_file_index; // written by the worker before state 2, read-only after
static unsigned face_file_index_ms;                 // written by the worker before state 2/3 (the hud logs it)
static unsigned face_file_moved;                    // local233: tables whose texture the index keeps elsewhere (992, 5012); as above
static std::wstring face_file_dir;                  // the archive folder ending in '\' (game folder\0002\); fixtures set it
static HANDLE face_file_wake;
// local228: the original list frame (face_files::frame_from_archive), RGBA; written by the worker before face_file_state 2/3, handed to
// the overlay once by the hud (face_frame_offered, hud only). Not found: the overlay keeps drawing its own frame.
static std::vector<std::uint8_t> face_frame_rgba;
static unsigned face_frame_w,face_frame_h;
static bool face_frame_ok,face_frame_offered;
struct FaceBank {std::uint32_t bank;int sub;HANDLE file;};
static std::vector<FaceBank> face_file_banks;       // worker only, open for the run
static HANDLE face_file_open(const std::wstring &path) {
    HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_RANDOM_ACCESS,nullptr);
    return h==INVALID_HANDLE_VALUE?nullptr:h;
}
static HANDLE face_file_bank(std::uint32_t bank,int sub) {
    for(const auto &b:face_file_banks)if(b.bank==bank && b.sub==sub)return b.file;
    HANDLE h=face_file_open(face_file_dir+face_files::bank_file(L"ScreenLayout",bank,sub));
    face_file_banks.push_back({bank,sub,h});
    return h;
}
static HANDLE face_file_packs[face_files::kPortraitFiles];   // worker only: 0005\000..003.bin, open for the run
static bool face_file_pack_tried[face_files::kPortraitFiles];
static HANDLE face_file_pack(unsigned file) {
    if(file>=face_files::kPortraitFiles)return nullptr;
    if(!face_file_packs[file] && !face_file_pack_tried[file]) {
        face_file_pack_tried[file]=true;wchar_t name[16]{};swprintf_s(name,L"%03u.bin",file);
        face_file_packs[file]=face_file_open(face_file_dir+L"..\\0005\\"+name);
        if(!face_file_packs[file])face_file_packs[file]=face_file_open(face_file_dir+L"..\\DLC\\0005\\"+name);   // local233: 003 is a DLC file
    }
    return face_file_packs[file];
}
static bool face_file_read(HANDLE h,std::uint64_t offset,std::uint64_t size,std::vector<std::uint8_t> &out) {
    if(!h || !size || size>face_files::kEntryLimit)return false;
    out.resize(static_cast<size_t>(size));
    OVERLAPPED at{};at.Offset=static_cast<DWORD>(offset);at.OffsetHigh=static_cast<DWORD>(offset>>32);DWORD got=0;
    return ReadFile(h,out.data(),static_cast<DWORD>(size),&got,&at) && got==size;
}
static unsigned face_file_build_index() {   // worker: the number of faces found (0 = none)
    std::vector<std::uint8_t> bytes;
    {
        HANDLE h=face_file_open(face_file_dir+L"ScreenLayout.rdb");LARGE_INTEGER size{};
        const bool ok=h && GetFileSizeEx(h,&size) && size.QuadPart>0 && size.QuadPart<(32ll<<20) && face_file_read(h,0,static_cast<std::uint64_t>(size.QuadPart),bytes);
        if(h)CloseHandle(h);
        if(!ok)return 0;
    }
    std::vector<face_files::Entry> entries;
    if(!face_files::parse_index(bytes.data(),bytes.size(),entries))return 0;
    // local228: the original list window frame for the report panel (one sprite table and about 20 chunks of its atlas)
    face_frame_ok=face_files::frame_from_archive(entries,[](const face_files::Entry &e,std::vector<std::uint8_t> &raw){return face_file_read(face_file_bank(e.bank,e.sub),e.offset,e.size,raw);},face_frame_rgba,face_frame_w,face_frame_h);
    std::vector<const face_files::Entry *> body;
    for(const auto &e:entries)if(e.body)body.push_back(&e);
    std::sort(body.begin(),body.end(),[](const face_files::Entry *a,const face_files::Entry *b){return a->bank!=b->bank?a->bank<b->bank:a->sub!=b->sub?a->sub<b->sub:a->offset<b->offset;});
    std::vector<std::uint8_t> raw,table;std::vector<std::pair<unsigned,unsigned>> faces;
    std::map<std::uint32_t,const face_files::Entry *> textures;   // local233: G1T entries by ktid, for the textures the index keeps elsewhere
    for(const auto *e:body)if(e->type==face_files::kTypeG1T)textures.emplace(e->ktid,e);
    unsigned moved=0;
    for(size_t i=1;i<body.size();++i) {   // a small sprite table stored right after a G1T in the same bank file (some banks pad entries to 16 bytes)
        const face_files::Entry &e=*body[i],&prev=*body[i-1];
        if(e.type!=face_files::kTypeSprite || e.dsize>4+104*4 || prev.bank!=e.bank || prev.sub!=e.sub || e.offset<prev.offset+prev.size)continue;
        const std::uint64_t gap=e.offset-(prev.offset+prev.size);
        const face_files::Entry *texture=prev.type==face_files::kTypeG1T && gap<16?&prev:nullptr;
        face_files::Entry stored;
        if(!texture && gap>=0x38 && gap<=face_files::kEntryLimit) {
            // local233: faces 992 and 5012. The bytes before the table hold an older copy of its texture that the index does not list; the
            // index has the same ktid in another bank. The listed copy is the one the game reads; the stored one only when none is listed.
            face_files::StoredHead head;
            if(!face_file_read(face_file_bank(e.bank,e.sub),prev.offset+prev.size,0x30,raw) || !face_files::stored_head(raw.data(),raw.size(),head) || head.type!=face_files::kTypeG1T || head.total>gap || gap-head.total>=16)continue;
            const auto listed=textures.find(head.ktid);
            if(listed!=textures.end())texture=listed->second;
            else {stored.bank=e.bank;stored.sub=e.sub;stored.offset=prev.offset+prev.size;stored.size=head.total;texture=&stored;}
        }
        if(!texture)continue;
        if(!face_file_read(face_file_bank(e.bank,e.sub),e.offset,e.size,raw) || !face_files::decompress_entry(raw.data(),raw.size(),table))continue;
        faces.clear();face_files::sprite_faces(table.data(),table.size(),faces);
        if(texture!=&prev && !faces.empty())++moved;
        for(const auto &f:faces){FaceSpot s;s.bank=texture->bank;s.sub=texture->sub;s.offset=texture->offset;s.size=texture->size;s.page=f.second;face_file_index[f.first]=s;}
    }
    face_file_moved=moved;
    std::vector<face_files::Portrait> busts;
    for(unsigned file=0;file<face_files::kPortraitFiles;++file) {   // the 0005 busts; a missing file only loses its span
        const HANDLE h=face_file_pack(file);
        if(!face_file_read(h,0,4,table))continue;
        const std::uint64_t count=face_files::u32(table.data());
        if(count>4096 || !face_file_read(h,0,4+8*count,table) || !face_files::portrait_entries(table.data(),table.size(),file,busts))continue;
        for(const auto &b:busts){FaceSpot s;s.pack=static_cast<int>(file);s.offset=b.offset;s.size=b.size;face_file_index.emplace(b.face,s);}
    }
    return static_cast<unsigned>(face_file_index.size());
}
static bool face_file_decode(unsigned art,std::vector<std::uint8_t> &pixels) {   // worker
    const auto it=face_file_index.find(art);if(it==face_file_index.end())return false;
    std::vector<std::uint8_t> raw,g1t;const FaceSpot &s=it->second;
    if(s.pack>=0)return face_file_read(face_file_pack(static_cast<unsigned>(s.pack)),s.offset,s.size,raw) && face_files::unpack_zp1(raw.data(),raw.size(),g1t) && face_files::head_from_g1t(g1t.data(),g1t.size(),0,pixels);
    return face_file_read(face_file_bank(s.bank,s.sub),s.offset,s.size,raw) && face_files::decompress_entry(raw.data(),raw.size(),g1t) && face_files::head_from_g1t(g1t.data(),g1t.size(),s.page,pixels);
}
static void face_file_index_now() {   // worker
    const ULONGLONG started=GetTickCount64();
    const unsigned faces=face_file_build_index();
    face_file_index_ms=static_cast<unsigned>(GetTickCount64()-started);
    InterlockedExchange(&face_file_state,faces?2:3);
}
// local229 glyph warm-up (briefing_overlay::warm_glyphs): the hud queues characters - the panels' fixed words when the worker starts,
// then the new characters of each stored message - and the worker draws them between faces, so the first report raster finds the
// font files loaded and the glyphs cached. A face asked meanwhile goes first.
struct GlyphAsk {std::wstring chars;unsigned kinds=0;};
static std::vector<GlyphAsk> glyph_wanted;              // under face_file_lock
static volatile LONG glyph_done_chars,glyph_done_ms,glyph_done_runs;   // worker adds, the hud logs
static constexpr unsigned kGlyphAllKinds=0xFFu,kGlyphReportKinds=(1u<<0)|(1u<<5)|(1u<<6)|(1u<<7);   // cached_font kinds: body, report body/head/small
static void face_file_serve() {   // worker: decode what was asked, oldest first; warm the asked glyphs when no face waits
    for(;;) {
        unsigned art=0;bool any=false,warm=false;GlyphAsk glyph;
        AcquireSRWLockExclusive(&face_file_lock);
        if(!face_file_wanted.empty()){art=face_file_wanted.front();face_file_wanted.erase(face_file_wanted.begin());any=true;}
        else if(!glyph_wanted.empty()){glyph=std::move(glyph_wanted.front());glyph_wanted.erase(glyph_wanted.begin());warm=true;}
        ReleaseSRWLockExclusive(&face_file_lock);
        if(warm) {
            const ULONGLONG from=GetTickCount64();const unsigned drawn=briefing_overlay::warm_glyphs(glyph.chars,glyph.kinds);
            InterlockedExchangeAdd(&glyph_done_chars,static_cast<LONG>(drawn));InterlockedExchangeAdd(&glyph_done_ms,static_cast<LONG>(GetTickCount64()-from));InterlockedIncrement(&glyph_done_runs);
            continue;
        }
        if(!any)return;
        FaceFileDone done;done.art=art;done.ok=face_file_decode(art,done.pixels);
        AcquireSRWLockExclusive(&face_file_lock);face_file_done.push_back(std::move(done));ReleaseSRWLockExclusive(&face_file_lock);
    }
}
#ifndef ADVISOR_BRIEFING_FIXTURE
static DWORD WINAPI face_file_worker(void *) {
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
    face_file_serve();   // local229: the fixed words first (queued by face_file_start): the font files load here, not on the render thread
    face_file_index_now();
    for(;;){WaitForSingleObject(face_file_wake,INFINITE);face_file_serve();}   // local229: also after a failed index (glyphs; no face is asked then)
}
#endif
// hud: queue characters for the worker (kinds: cached_font kinds to warm); chars already queued once this run are skipped.
static std::vector<bool> glyph_seen;   // hud only, by UTF-16 unit
static void glyph_take(std::wstring &out,const std::wstring &text) {
    if(glyph_seen.size()<65536)glyph_seen.assign(65536,false);
    for(wchar_t c:text){if(c<=L' ' || glyph_seen[static_cast<unsigned short>(c)])continue;glyph_seen[static_cast<unsigned short>(c)]=true;out+=c;}
}
static void glyph_ask(std::wstring chars,unsigned kinds) {
    if(chars.empty())return;
    AcquireSRWLockExclusive(&face_file_lock);
    for(size_t i=0;i<chars.size();i+=96)glyph_wanted.push_back({chars.substr(i,96),kinds});   // small asks: a face waits at most one of them
    ReleaseSRWLockExclusive(&face_file_lock);
#ifndef ADVISOR_BRIEFING_FIXTURE
    if(face_file_wake)SetEvent(face_file_wake);
#endif
}
// The panels' fixed words: titles, labels, buttons, topic names and tab captions, digits and marks.
static std::wstring glyph_seed() {
    std::wstring out;
    glyph_take(out,L"0123456789,.·~()[]:;!?%+-/…「」『』\"'");
    for(const wchar_t *w:{L"주공, 새로 전할 소식이 없습니다",L"주공, 소식을 모으고 있습니다",L"이번 순의 일이 끝나면 한데 모아 보고드리겠습니다.",
        L"다음 진행부터 천시·재해·성장·인재와 탐색 소식을 이곳에서 확인하실 수 있습니다.",L"전체 소식 건 새 소식 기간 일차 금 합계 표시 최근 인물 구분 내용",
        L"지우기 모두 지우기 군사 제안 수집 설정 보고로 부동산 개발 수락 거절 알림함에 모을 소식 원작대로 보유한 성 지금은 올릴 계책이 없습니다.",
        L"매복 작전 제안 파괴공작 제안 포상 위임 주공을 대신해 에게 포상했습니다 외 명 장수 로그 위치 지도 이동 내정 ›"})glyph_take(out,w);
    for(unsigned i=0;i<kTopicListed;++i){glyph_take(out,topic_name(topic_order[i]));glyph_take(out,report_tab_caption(static_cast<unsigned>(topic_order[i])));}
    for(unsigned k=0;k<=static_cast<unsigned>(Kind::Reward);++k){glyph_take(out,caption(static_cast<Kind>(k)));glyph_take(out,tab_caption(static_cast<Kind>(k)));}
    return out;
}
static void face_file_start() {   // hud, once per run
    if(InterlockedCompareExchange(&face_file_state,1,0)!=0)return;
    glyph_ask(glyph_seed(),kGlyphAllKinds);   // local229: served first by the worker
    if(face_file_dir.empty()) {
        wchar_t exe[MAX_PATH]{};const DWORD n=GetModuleFileNameW(nullptr,exe,MAX_PATH);
        const std::wstring path(exe,n<MAX_PATH?n:0);const size_t slash=path.find_last_of(L"\\/");
        face_file_dir=(slash==std::wstring::npos?std::wstring():path.substr(0,slash+1))+L"0002\\";
    }
#ifdef ADVISOR_BRIEFING_FIXTURE
    face_file_index_now();   // fixtures run the worker's steps inline (face_file_serve after asking)
#else
    face_file_wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    HANDLE thread=face_file_wake?CreateThread(nullptr,0,face_file_worker,nullptr,0,nullptr):nullptr;
    if(thread)CloseHandle(thread);else InterlockedExchange(&face_file_state,3);
#endif
}
// hud side
enum class FaceFile {Ready,Pending,None};
static constexpr size_t kFaceHeadsKept=64;   // decoded heads kept for offers, oldest dropped (46 KB each); the overlay keeps its own copies
static std::vector<std::pair<unsigned,std::vector<std::uint8_t>>> face_file_heads;   // hud only, oldest first
static std::vector<unsigned> face_file_pending,face_file_failed;                    // hud only
static bool face_file_logged;
static bool has_art(const std::vector<unsigned> &v,unsigned art){return std::find(v.begin(),v.end(),art)!=v.end();}
static void face_file_collect() {
    const LONG state=InterlockedCompareExchange(&face_file_state,0,0);
    if(!face_file_logged && state>=2) {
        face_file_logged=true;
#ifndef NO_RUNTIME_LOG
        log_event("advisor_face_index",static_cast<unsigned>(face_file_index.size()),face_file_index_ms,static_cast<unsigned>(state));   // faces, ms, 2 ready / 3 failed
        if(state==2) {   // local233: age faces found (of 19), tables read through a moved texture, DLC busts 6215..6244 (of 30)
            unsigned ages=0,dlc=0;
            for(const auto &f:face_files::kAgeFaces)if(face_file_index.count(f[1]))++ages;
            for(unsigned face=6215;face<6245;++face)if(face_file_index.count(face))++dlc;
            log_event("advisor_face_index_parts",ages,face_file_moved,dlc);
        }
#endif
    }
    if(!face_frame_offered && state>=2) {   // local228
        face_frame_offered=true;
        if(face_frame_ok)briefing_overlay::offer_frame(face_frame_rgba.data(),face_frame_w,face_frame_h);
#ifndef NO_RUNTIME_LOG
        log_event("advisor_frame_file",face_frame_ok?1u:0u,face_frame_w,face_frame_h);   // 1 the original frame / 0 our own, its size
#endif
    }
#ifndef NO_RUNTIME_LOG
    {   // local229: the worker's glyph warm-up so far (characters, ms, asks), each time it has caught up with the asks (at most 40 lines)
        static LONG logged_runs;static unsigned glyph_logs;
        const LONG runs=InterlockedCompareExchange(&glyph_done_runs,0,0);
        if(runs!=logged_runs && glyph_logs<40) {
            AcquireSRWLockExclusive(&face_file_lock);const bool idle=glyph_wanted.empty();ReleaseSRWLockExclusive(&face_file_lock);
            if(idle){logged_runs=runs;++glyph_logs;log_event("advisor_glyph_warm",static_cast<unsigned>(InterlockedCompareExchange(&glyph_done_chars,0,0)),static_cast<unsigned>(InterlockedCompareExchange(&glyph_done_ms,0,0)),static_cast<unsigned>(runs));}
        }
    }
#endif
    std::vector<FaceFileDone> got;
    AcquireSRWLockExclusive(&face_file_lock);got.swap(face_file_done);ReleaseSRWLockExclusive(&face_file_lock);
    for(auto &d:got) {
        face_file_pending.erase(std::remove(face_file_pending.begin(),face_file_pending.end(),d.art),face_file_pending.end());
        if(!d.ok) {
            face_file_failed.push_back(d.art);
#ifndef NO_RUNTIME_LOG
            log_event("advisor_face_file_fail",d.art,0,0);
#endif
            continue;
        }
        if(face_file_heads.size()>=kFaceHeadsKept)face_file_heads.erase(face_file_heads.begin());
        face_file_heads.emplace_back(d.art,std::move(d.pixels));
    }
}
static FaceFile face_file(unsigned art,const std::vector<std::uint8_t> *&pixels) {
    pixels=nullptr;
    if(!InterlockedCompareExchange(&face_file_state,0,0))face_file_start();
    const LONG state=InterlockedCompareExchange(&face_file_state,0,0);
    if(state==1)return FaceFile::Pending;
    if(state!=2 || !face_file_index.count(art) || has_art(face_file_failed,art))return FaceFile::None;
    for(auto it=face_file_heads.rbegin();it!=face_file_heads.rend();++it)if(it->first==art){pixels=&it->second;return FaceFile::Ready;}
    if(!has_art(face_file_pending,art)) {
        face_file_pending.push_back(art);
        AcquireSRWLockExclusive(&face_file_lock);face_file_wanted.push_back(art);ReleaseSRWLockExclusive(&face_file_lock);
#ifndef ADVISOR_BRIEFING_FIXTURE
        if(face_file_wake)SetEvent(face_file_wake);
#endif
    }
    return FaceFile::Pending;
}
// local232: the report badge's adviser face while the advice panel is hidden (its frame does not run): the adviser (+0x448) is read here,
// on the log frame. The face file first, then the resident texture (250 ms, briefing16), then the hold: the panel is shown again and its
// frame runs with the old face retry and capture (advice_frame) until the face is taken, at most 4 s; then it is hidden anyway.
static void native_adviser_tick(Obj advice_layout) { COST_SCOPE("advisor_briefing:native_adviser");
    Obj adviser=advice_refresh::current(advice_layout);const unsigned art=adviser?word(adviser,0xA6):~0u;   // local233: picked at a skipped rebuild
    if(adviser!=last_adviser || art!=last_art){last_adviser=adviser;last_art=art;++portrait_serial;}
    NativeHud &h=native_hud;const ULONGLONG now=GetTickCount64();
    if(h.face_serial!=portrait_serial){h.face_serial=portrait_serial;h.face_path=0;h.face_started=now;h.face_tried=0;h.hold_started=0;h.hold=false;}
    if(!adviser || briefing_overlay::captured(portrait_serial)) {
#ifndef NO_RUNTIME_LOG
        if(h.hold)log_event("advisor_native_face",static_cast<unsigned>(portrait_serial),art,5);   // 5: taken during the hold
#endif
        h.hold=false;return;
    }
    if(h.face_path==3)return;
    const std::vector<std::uint8_t> *pixels=nullptr;
    const FaceFile file=face_file(art,pixels);
    if(file==FaceFile::Ready) {
        if(h.face_path!=1 && briefing_overlay::offer_face_pixels(pixels->data(),pixels->size(),portrait_serial)) {
            h.face_path=1;h.hold=false;
#ifndef NO_RUNTIME_LOG
            log_event("advisor_native_face",static_cast<unsigned>(portrait_serial),art,1);
#endif
        }
        return;
    }
    if(h.face_path==1 || h.face_path==2)return;   // offered: the overlay takes it on the next present
    if(file==FaceFile::Pending && now-h.face_started<4000)return;
    if(mod_features.briefing16 && !h.hold_started) {
        if(!h.face_tried || now-h.face_tried>=250) {
            if(!h.face_tried)h.resident_from=now;
            h.face_tried=now;unsigned where=0;
            if(ID3D11Texture2D *texture=face_texture_for(art,where)) {
                briefing_overlay::offer_face(texture,portrait_serial);h.face_path=2;
#ifndef NO_RUNTIME_LOG
                log_event("advisor_native_face",static_cast<unsigned>(portrait_serial),art,2);
#endif
                return;
            }
        }
        if(now-h.resident_from<1000)return;   // a second of resident looks before the panel comes back
    }
    if(!h.hold_started) {
        h.hold_started=now;h.hold=true;
#ifndef NO_RUNTIME_LOG
        log_event("advisor_native_face",static_cast<unsigned>(portrait_serial),art,file==FaceFile::None?4u:0x14u);   // 4: the hold (0x14: file still pending)
#endif
    } else if(h.hold && now-h.hold_started>=4000) {
        h.hold=false;h.face_path=3;
#ifndef NO_RUNTIME_LOG
        log_event("advisor_native_face",static_cast<unsigned>(portrait_serial),art,3);   // 3: no face for this adviser
#endif
    }
}
// Which faces to look at, one look per frame: the selected row and the rows on screen, then the other rows of the open report, then the
// persons of messages stored since. store() does nothing for faces: messages arrive on several game threads (and so does the hud), so
// the hud walks the new inbox messages itself (32 per frame) and queues their persons. A queued person is looked at every
// face_queue_gap_ms for face_queue_life_ms, because the message is stored before (or instead of) the original window that may draw the
// face. A report row the game does not hold is looked at again after face_recheck_ms. A face handed over stays in the overlay for the
// session (kRowFaces kept, least recently drawn replaced).
struct QueuedFace {unsigned person=0,art=0,where=0;ULONGLONG until=0,next=0;};
static std::vector<QueuedFace> face_queue;    // hud frame only
static std::vector<ULONGLONG> face_look_at;   // by person id: when a report row last looked at it
static ULONGLONG face_recheck_ms=2000,face_queue_gap_ms=300,face_queue_life_ms=8000;   // fixtures change them
static unsigned face_scan_generation=~0u;static size_t face_scan_next=0;                // the inbox messages already queued
static unsigned face_row_looks,face_row_hits,face_row_misses[8];static bool face_rows_shown;   // per report opening, logged when it closes
static ID3D11Texture2D *(*row_face_source)(unsigned,unsigned &)=resident_face;   // fixtures substitute their own texture
static void flush_face_misses() {
#ifndef NO_RUNTIME_LOG
    if(face_row_looks) {   // hex: where 3 | where 4 <<8 | where 5 <<16 | (where 1, 2, 6) <<24, each up to 255
        auto c=[](unsigned n){return n<255?n:255u;};
        log_event("advisor_row_face_misses",face_row_hits,face_row_looks,c(face_row_misses[3])|c(face_row_misses[4])<<8|c(face_row_misses[5])<<16|c(face_row_misses[1]+face_row_misses[2]+face_row_misses[6])<<24);
    }
#endif
    face_row_looks=face_row_hits=0;std::fill(std::begin(face_row_misses),std::end(face_row_misses),0u);
}
static void release_row_faces(){flush_face_misses();face_look_at.clear();face_queue.clear();face_scan_generation=~0u;face_scan_next=0;face_rows_shown=false;}
// One look. where: resident_face's (1 no manager, 2 no id, 3 no entry, 4 no texture, 5 not held), 6 not a living person, 7 the overlay
// is busy, 8 the face file is being read, 0x10 handed over (resident texture), 0x20 the overlay has it already, 0x40 handed over (face file).
struct FaceLook {unsigned art=0,where=0;};
static FaceLook look_face(unsigned person,Obj root) {
    FaceLook r;
    if(briefing_overlay::row_face_ready(person)){r.where=0x20;return r;}
    Obj who=pointer(root,0x158+person*8);
    if(!who || !fn<int(__fastcall *)(Obj)>(GA(GA_331780))(who)){r.where=6;return r;}
    r.art=word(who,0xA6);
    const std::vector<std::uint8_t> *pixels=nullptr;   // local224: the face file first, the resident texture for a face not in the files
    const FaceFile file=face_file(r.art,pixels);
    if(file==FaceFile::Pending){r.where=8;return r;}
    if(file==FaceFile::Ready) {
        if(!briefing_overlay::offer_row_face_pixels(pixels->data(),pixels->size(),person)){r.where=7;return r;}
        r.where=0x40;return r;
    }
    ID3D11Texture2D *texture=row_face_source(r.art,r.where);
    if(!texture)return r;
    if(!briefing_overlay::offer_row_face(texture,person)){texture->Release();r.where=7;return r;}
    r.where=0x10;return r;
}
// A report row's face; true when a look was spent (one per frame).
static bool look_row_face(unsigned person,Obj root,ULONGLONG now) {
    if(!person || person>=6001)return false;
    if(face_look_at.size()<6001)face_look_at.assign(6001,0);
    ULONGLONG &at=face_look_at[person];
    if(at && now-at<face_recheck_ms)return false;
    at=now;
    const FaceLook r=look_face(person,root);
    if(r.where==0x20)return false;
    if(r.where==8){at=0;return false;}   // local224: the file is being read: asked again next frame, no look spent
    if(r.where==7){at=0;return true;}   // the overlay is busy: next frame
    ++face_row_looks;
    if(r.where==0x10 || r.where==0x40) {
        ++face_row_hits;
#ifndef NO_RUNTIME_LOG
        if(r.where==0x10)log_event("advisor_row_face",person,r.art,r.where);   // file faces: advisor_row_face_copy has them
#endif
    }
    else ++face_row_misses[r.where<8?r.where:0];
    return true;
}
static void queue_face(unsigned person,ULONGLONG now) {
    if(!person || person>=6001 || briefing_overlay::row_face_ready(person))return;
    for(auto &q:face_queue)if(q.person==person){q.until=now+face_queue_life_ms;return;}
    if(face_queue.size()<256){QueuedFace q;q.person=person;q.until=now+face_queue_life_ms;face_queue.push_back(q);}
}
static void row_faces_tick(bool showing,const briefing_overlay::Model &m,Obj root) { COST_SCOPE("advisor_briefing:row_faces");
    if(!root)return;
    face_file_collect();
    if(!InterlockedCompareExchange(&face_file_state,0,0))face_file_start();   // local224: index the face files early, before the first report
    const ULONGLONG now=GetTickCount64();
    // New messages: after a world change the stored ones are left to the report rows; a new cycle starts from its first message.
    if(face_scan_generation!=inbox.generation){face_scan_next=face_scan_generation==~0u?inbox.events.size():0;face_scan_generation=inbox.generation;}
    if(face_scan_next>inbox.events.size())face_scan_next=inbox.events.size();   // deleted from the report
    std::wstring glyphs;   // local229: the new characters of these messages, warmed by the worker before the report opens
    for(unsigned n=0;n<32 && face_scan_next<inbox.events.size();++n) {
        const Event &e=inbox.events[face_scan_next++];
        glyph_take(glyphs,display_text(e.text));glyph_take(glyphs,display_text(e.report));
        queue_face(event_person(e,root),now);
    }
    {   // local232: the original log's new lines (the report's 로그 tab)
        std::wstring log_chars;
        AcquireSRWLockExclusive(&native_log_lock);log_chars.swap(native_log_glyphs);ReleaseSRWLockExclusive(&native_log_lock);
        if(!log_chars.empty())glyph_take(glyphs,display_text(log_chars));
    }
    glyph_ask(std::move(glyphs),kGlyphReportKinds);
    if(face_rows_shown && !showing)flush_face_misses();
    face_rows_shown=showing;
    for(size_t i=0;i<face_queue.size();) {
        const QueuedFace &q=face_queue[i];
        const bool ready=briefing_overlay::row_face_ready(q.person),expired=now>=q.until;
#ifndef NO_RUNTIME_LOG
        if(expired && !ready)log_event("advisor_row_face_queue_miss",q.person,q.art,q.where);   // where of the last look (0: never looked at)
#endif
        if(ready || expired)face_queue.erase(face_queue.begin()+static_cast<std::ptrdiff_t>(i));else ++i;
    }
    if(briefing_overlay::row_face_busy())return;
    if(showing && m.report && !m.rows.empty()) {
        if(m.selected<m.rows.size() && look_row_face(m.rows[m.selected].person,root,now))return;
        const long first=briefing_overlay::row_view_first();
        if(first>=0)for(size_t i=static_cast<size_t>(first);i<m.rows.size() && i<static_cast<size_t>(first)+(m.log_view?13u:8u);++i)if(look_row_face(m.rows[i].person,root,now))return;
        for(const auto &r:m.rows)if(look_row_face(r.person,root,now))return;   // the rest of the report, in the background
    }
    for(size_t i=0;i<face_queue.size();++i) {
        QueuedFace &q=face_queue[i];
        if(q.next>now)continue;
        const FaceLook r=look_face(q.person,root);
        q.art=r.art;q.where=r.where;q.next=now+face_queue_gap_ms;
        if(r.where==0x10 || r.where==0x40 || r.where==0x20 || r.where==6) {
#ifndef NO_RUNTIME_LOG
            if(r.where==0x10)log_event("advisor_row_face",q.person,r.art,0x100|r.where);   // 0x110: from the stored-message queue
            if(r.where==6)log_event("advisor_row_face_queue_miss",q.person,0,6);
#endif
            face_queue.erase(face_queue.begin()+static_cast<std::ptrdiff_t>(i));
        }
        return;   // one look per frame
    }
}
static void __fastcall advice_frame(Obj layout) { COST_SCOPE("advisor_briefing::advice_frame"); COST_CRUMB("advisor_briefing_layout",layout);
#ifndef NO_RUNTIME_LOG
    FrameTiming timing(2);
#endif
#ifndef NO_RUNTIME_LOG
    hook_called(2);
#endif
    note_duel(6);
    bind(old_advice,layout);
    if(native_hud.main && native_hud.part[1]==layout && !native_hud.hold && remove_native()) {
        // local232: 88E4B0 does not run while the report has the advice (the adviser and his face are read on the log frame).
        ++native_hud.advice_skipped;if(hud_part_shown(layout))++native_hud.advice_skipped_shown;
#ifndef NO_RUNTIME_LOG
        if(native_hud.advice_skipped==1 || native_hud.advice_skipped==600)log_event("advisor_native_advice_skip",native_hud.advice_skipped,native_hud.advice_skipped_shown);
#endif
        return;
    }
    advice_refresh::rebuild(layout);   // local233: the list (and adviser) a skipped rebuild left behind, once
    slot_call<void(__fastcall *)(Obj)>(12)(layout);
    if(!active()){move(old_advice,false);return;}
    Obj adviser=pointer(layout,0x448);unsigned art=adviser?word(adviser,0xA6):~0u;
    if(adviser!=last_adviser || art!=last_art){last_adviser=adviser;last_art=art;++portrait_serial;}
    bool needs_portrait=replace_legacy() && adviser && !briefing_overlay::captured(portrait_serial);
    // U6 (local17 briefing16): the original face texture replaces the capture fallback. local36 (user 2026-11-10 "초상화가
    // 예전방식"): the 512 face is often not resident on the first tries, and the old loop stopped as soon as the capture
    // was accepted, so the capture stayed. Keep retrying until the face is offered for this serial (250 ms for 5 s, then
    // 1 s); take_face_offer replaces an accepted capture. No per-art cache: the game may reuse a texture for another face.
    static int face_offered=-1;
    if(replace_legacy() && adviser && mod_features.briefing16 && face_offered!=portrait_serial) {
        static ULONGLONG face_tried_at,face_started_at;static int face_started_serial=-1;
        const ULONGLONG now=GetTickCount64();
        if(face_started_serial!=portrait_serial){face_started_serial=portrait_serial;face_started_at=now;face_tried_at=0;}
        const ULONGLONG gap=now-face_started_at<5000?250:1000;
        if(!face_tried_at || now-face_tried_at>=gap) {
            face_tried_at=now;unsigned where=0;
            if(ID3D11Texture2D *texture=face_texture_for(art,where)){briefing_overlay::offer_face(texture,portrait_serial);face_offered=portrait_serial;}
#ifndef NO_RUNTIME_LOG
            static unsigned face_logs;
            if(face_logs<80 && (where!=4 || now-face_started_at<5000)){++face_logs;log_event("advisor_face_lookup",static_cast<unsigned>(portrait_serial),art,where);}
            if(where==4){static unsigned shape_logs;if(shape_logs<20){++shape_logs;log_event("advisor_face_shape",art,face_last_shape[0],face_last_shape[1]);}}
#endif
        }
    }
    move(old_advice,hide_native_hud() && replace_legacy() && !needs_portrait);
    if(needs_portrait && fn<unsigned(__fastcall *)(Obj)>(GA(GA_871600))(layout)) {
        int pos[2]{},size[2]{};fn<void(__fastcall *)(Obj,int *,int)>(GA(GA_92EFC0))(layout+0x130,pos,5);fn<void(__fastcall *)(Obj,int *,int)>(GA(GA_92F440))(layout+0x130,size,5);
        // The call above refreshes the node's native matrix. Use its complete
        // world scale, including the parent's transform, rather than texel size.
        Obj nodes=pointer(layout,0x138),node=nodes && dword(layout,0x140)>5?pointer(nodes,5*8):nullptr;
        Obj matrix=node?pointer(node,pointer(node,0xD0)?0xE0:0xE8):nullptr;
        Rect bounds;float transform[16];
        if(matrix){memcpy(transform,matrix,sizeof(transform));if(portrait_bounds(size[0],size[1],node[0x10C],transform,bounds))briefing_overlay::request_portrait(bounds,portrait_serial);}
    }
}
static void __fastcall advice_leave(Obj layout,Obj context) { COST_SCOPE("advisor_briefing::advice_leave"); COST_CRUMB("advisor_briefing_layout",layout);
    if(old_advice.object==layout){move(old_advice,false);old_advice={};last_adviser=nullptr;last_art=~0u;}
    slot_call<void(__fastcall *)(Obj,Obj)>(13)(layout,context);
}
static void *replacements[]={reinterpret_cast<void *>(&user_enter),reinterpret_cast<void *>(&user_leave),reinterpret_cast<void *>(&game_leave),reinterpret_cast<void *>(&narration),reinterpret_cast<void *>(&simple_enter),reinterpret_cast<void *>(&simple_frame<5>),reinterpret_cast<void *>(&report_enter),reinterpret_cast<void *>(&report_frame),reinterpret_cast<void *>(&report_leave),reinterpret_cast<void *>(&report_wait),reinterpret_cast<void *>(&log_frame),reinterpret_cast<void *>(&log_leave),reinterpret_cast<void *>(&advice_frame),reinterpret_cast<void *>(&advice_leave),reinterpret_cast<void *>(&diplomatic_enter),reinterpret_cast<void *>(&simple_frame<15>),reinterpret_cast<void *>(&diplomatic_wait),reinterpret_cast<void *>(&user_frame),reinterpret_cast<void *>(&person_notice_enter),reinterpret_cast<void *>(&simple_frame<19>)};
// local65 (user 2026-11-12 "계략이 카메라 포커스 가져가는거 스킵하고 싶어 군사보고에 어차피 기록되는데"), part of briefing16.
// The stratagem effects 1FB170 (kind 0x14 건물파괴), 1FC530 (kind 14) and 1FD930 (kind 13) call
// 2DC840(tile,ms,state,zoom) when the player is involved: it saves the camera (singleton 110D0) into state, moves to the
// target tile and waits ms; the effect ends with 2EB1F0(state) = restore + wait state[0] ms. The replacement saves the
// camera exactly the same way but neither moves nor waits, and stores 0 ms, so the closing restore is instant and lands
// where the camera already is. Results, messages and the briefing copy are untouched. briefing16 off = original calls.
namespace plot_camera {
using Focus=void(__fastcall *)(unsigned,unsigned,unsigned *,float);
static void __fastcall focus(unsigned tile,unsigned ms,unsigned *state,float zoom) { COST_SCOPE("plot_camera:focus");
    Obj camera=mod_features.briefing16 && state && advisor_briefing::collects(advisor_briefing_core::Topic::Plot)?fn<Obj(__fastcall *)()>(GA(GA_110D0))():nullptr;
    if(!camera){fn<Focus>(GA(GA_2DC840))(tile,ms,state,zoom);return;}
    float *out=reinterpret_cast<float *>(state);
    for(unsigned i=0;i<4;++i)out[4+i]=static_cast<float>(*reinterpret_cast<double *>(camera+0xA0+i*8));
    double height=*reinterpret_cast<double *>(camera+0x1A8);
    if(height==*reinterpret_cast<double *>(image+GA(GA_1345CF0)))height=*reinterpret_cast<double *>(camera+0xE0);
    memcpy(state+8,&height,8);memcpy(state+10,camera+0xF8,8);memcpy(state+12,camera+0x110,8);
    state[14]=1;state[0]=0;   // saved; 0 ms: 2EB1F0 restores at once and does not wait
#ifndef NO_RUNTIME_LOG
    static unsigned logged;if(logged<32){++logged;log_event("plot_camera_skipped",tile,ms,0);}
#endif
}
static CallLink links[3];
static void install() { COST_SCOPE("plot_camera:install");
    if(!mod_features.briefing16)return;
    const unsigned sites[3]={GA(GA_1FBB50),GA(GA_1FCA60),GA(GA_1FDB73)},target=GA(GA_2DC840);
    bool ok=target && GA(GA_110D0) && GA(GA_1345CF0);
    for(unsigned site:sites) {
        int delta=0;if(ok && site)memcpy(&delta,image+site+1,4);
        ok=ok && site && image[site]==0xE8 && site+5+static_cast<unsigned>(delta)==target;
    }
    Obj relay=ok?allocate_near(image+sites[0]):nullptr;
    if(!relay){log_event("plot_camera_rejected",ok?1u:0u,0,0);return;}
    absolute_jump(relay,reinterpret_cast<void *>(&focus));
    DWORD old;unsigned n=0;
    ok=VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) && FlushInstructionCache(GetCurrentProcess(),relay,16);
    for(;ok && n<3;++n)ok=call_prepare(links[n],sites[n],relay);
    for(unsigned i=0;ok && i<3;++i)ok=call_publish(links[i]);
    if(!ok)for(unsigned i=0;i<3;++i)call_restore(links[i]);
    log_event(ok?"plot_camera_installed":"plot_camera_rejected",2,n,0);
}
}   // namespace plot_camera
bool install() { COST_SCOPE("advisor_briefing:install");
    if(!guards_match())return false;
    Obj relay=allocate_near(image+GA(GA_9332B0));if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&set_text));memcpy(relay+32,image+GA(GA_9332B0),5);absolute_jump(relay+37,image+GA(GA_9332B5));
    absolute_jump(relay+128,reinterpret_cast<void *>(&diplomatic_speech));memcpy(relay+160,image+GA(GA_843B60),5);absolute_jump(relay+165,image+GA(GA_843B65));
    bool messages_prepared=prepare_message_link(message_links[0],GA(GA_1F1E90),message_prologue,sizeof(message_prologue),relay+256,reinterpret_cast<void *>(&message_call)) &&
        prepare_message_link(message_links[1],GA(GA_1F1DA0),narration_prologue,sizeof(narration_prologue),relay+320,reinterpret_cast<void *>(&narration_call)) &&
        prepare_message_link(message_links[2],GA(GA_1F2190),person_prologue,sizeof(person_prologue),relay+384,reinterpret_cast<void *>(&person_call)) &&
        prepare_message_link(event_link,GA(GA_844860),event_prologue,sizeof(event_prologue),relay+448,reinterpret_cast<void *>(&event_speech)) &&
        (scene_yield || prepare_message_link(scene_links[0],GA(GA_6C6B10),scene_prologue,sizeof(scene_prologue),relay+512,reinterpret_cast<void *>(&employ_scene))) &&
        (scene_yield || prepare_message_link(scene_links[1],GA(GA_6C6DD0),scene_prologue,sizeof(scene_prologue),relay+576,reinterpret_cast<void *>(&discovery_scene))) &&
        prepare_message_link(scene_links[2],GA(GA_41F8C0),scenery_prologue,sizeof(scenery_prologue),relay+640,reinterpret_cast<void *>(&scenery_scene));
    if(!scene_yield){original_employ_scene=reinterpret_cast<SceneCall>(relay+544);original_discovery_scene=reinterpret_cast<SceneCall>(relay+608);}
    original_scenery_scene=reinterpret_cast<SceneryCall>(relay+672);
    original_message_call=reinterpret_cast<MessageCall>(relay+288);
    original_narration_call=reinterpret_cast<NarrationCall>(relay+352);
    original_person_call=reinterpret_cast<NarrationCall>(relay+416);
    original_event_speech=reinterpret_cast<EventSpeech>(relay+480);
    original_speech=reinterpret_cast<SpeechSetter>(relay+160);
    original_text=reinterpret_cast<TextSetter>(relay+32);DWORD old;
    bool prepared=messages_prepared &&
        prepare_picture_link(picture_links[0],GA(GA_4541E0),picture_show_bytes,sizeof(picture_show_bytes),relay+704,reinterpret_cast<void *>(&picture_show)) &&
        prepare_picture_link(picture_links[1],GA(GA_4421F0),picture_close_bytes,sizeof(picture_close_bytes),relay+736,reinterpret_cast<void *>(&picture_close)) &&
        prepare_message_link(time_wait_link,GA(GA_306A30),narration_prologue,sizeof(narration_prologue),relay+768,reinterpret_cast<void *>(&time_wait)) &&
        prepare_link(text_link,GA(GA_9332B0),0xE9,relay) && prepare_link(speech_link,GA(GA_843B60),0xE9,relay+128);
    original_time_wait=reinterpret_cast<TimeWait>(relay+800);
    prepare_scene_calls(relay);
    absolute_jump(relay+1024,reinterpret_cast<void *>(&report_results));
    prepared=prepared && call_prepare(report_results_call,GA(GA_6793CD),relay+1024);
    if(!prepared || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,1040)){VirtualFree(relay,0,MEM_RELEASE);return false;}
    unsigned n=0;
    // 2026-11-18 (ic_gate.h): 9332B0 lies inside IC's start-up check ranges; with IC installed it goes in from publish_late().
    const bool text_late=ic_gate::pending();
    bool published=(text_late || publish_link(text_link)) && publish_link(speech_link);
    for(unsigned i=0;i<3 && published;++i)published=publish_link(message_links[i]);
    published=published && publish_link(event_link);
    for(unsigned i=0;i<3 && published;++i)if(i>=2 || !scene_yield)published=publish_link(scene_links[i]);
    if(scene_yield)log_event("advisor_scene_yield",other_mod_present(L"SAN14_SceneSkip.dll")?1:2,0,0);
    for(unsigned i=0;i<2 && published;++i)published=publish_link(picture_links[i]);
    published=published && publish_link(time_wait_link);
    published=published && scene_call_publish(report_results_call);
    if(published)publish_scene_calls();
#ifdef LAYOUT_ROUTED
    const unsigned slot_mark=layout_client::current_mark();
#endif
    if(published)for(;n<sizeof(slots)/sizeof(*slots);++n)if(!ROUTE_VT(slots[n].rva,slots[n].original,replacements[n],&slot_next[n]))break;
    if(n!=sizeof(slots)/sizeof(*slots)) {
        call_restore(report_results_call);restore_scene_calls();
#ifdef LAYOUT_ROUTED
        layout_client::rollback_to(slot_mark);(void)n;
#else
        while(n){--n;restore_pointer(slots[n].rva,slots[n].original,replacements[n]);}
#endif
        restore_link(time_wait_link);for(unsigned i=2;i;)restore_link(picture_links[--i]);for(unsigned i=3;i;)restore_link(scene_links[--i]);restore_link(event_link);for(unsigned i=3;i;)restore_link(message_links[--i]);restore_link(speech_link);restore_link(text_link);return false;
    }
#ifndef NO_RUNTIME_LOG
    log_event("advisor_briefing_entry_schema",message_entry_paths,message_reason_columns);
    log_event("advisor_briefing_picture_schema",picture_count_slots);
#endif
    plot_camera::install();   // optional part of briefing16; its failure leaves the report as it is
    advice_refresh::install();   // local233: optional; its failure leaves the advice rebuilds to the original
    collect_load();   // local99: topics chosen on the [수집 설정] page
    text_late_wanted=text_late;
    log_event("advisor_briefing_installed");return true;
}
// Late worker (native_adapter.cpp, after the IC check): the one deferred link. A CAS on its 16-byte block, no game call.
// A refusal leaves only the 9332B0 text setter original; the rest of the briefing stays installed.
void publish_late() { COST_SCOPE("advisor_briefing:publish_late");
    if(!text_late_wanted)return;
    text_late_wanted=false;
    const bool ok=publish_link(text_link);
    log_event(ok?"advisor_text_late":"advisor_text_late_rejected",GA(GA_9332B0),0,0);
}
#endif
}
bool advisor_briefing_guards_match(){return advisor_briefing::guards_match();}
