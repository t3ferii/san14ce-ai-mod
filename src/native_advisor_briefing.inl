// Convenience DLL; original UI callbacks only. No save writes or remote calls.
#include "game_addresses.h"
#include "advisor_briefing_profile.h"
#include "advisor_terrain_bridge.h"
#include <d3d11.h>   // local17 briefing16: direct face texture
#include <psapi.h>   // GetModuleInformation -> K32GetModuleInformation (kernel32.lib)
namespace advisor_briefing {
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
static Obj report_state,simple_state;
static std::vector<Obj> simple_states;
static bool report_deferred;
static bool report_headless;
static CallLink report_results_call;
static thread_local Obj replay_results;
static unsigned rendered_generation=~0u,rendered_page=~0u;
static bool rendered_open=false;
static bool rendered_collecting=false;
static bool rendered_city_list=false;
static Obj rendered_adviser;
static briefing_overlay::Model display_cache;
static int started_day=-1;
static Obj last_adviser;
static unsigned last_art=~0u;
static int portrait_serial=0;
struct Legacy {Obj object=nullptr;int x=0,y=0;bool moved=false;};
static Legacy old_log,old_advice;
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
static void focus_city(Obj city) { COST_SCOPE("advisor_briefing:focus_city");
    unsigned hex=valid(city)?fn<unsigned short(__fastcall *)(Obj)>(GA(GA_22F190))(city):0;
    if(hex<48400)
        fn<unsigned long long(__fastcall *)(unsigned,int,int,double,double,int)>(GA(GA_453570))(hex,1,500,-1.0,-1.0,1);
}
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
static void __fastcall picture_show(Obj object,int id) { COST_SCOPE("advisor_briefing::picture_show"); COST_CRUMB("advisor_briefing_object",object);
    bool suppressed=defer(); // Includes !interaction(), on the native UI caller.
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
static void reset() { COST_SCOPE("advisor_briefing:reset");
    clear_scene_pending();
    restore_speech();briefing_overlay::mask_report(false);diplomatic_choice_parent=nullptr;
    move(old_log,false);move(old_advice,false);old_log={};old_advice={};
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
[[maybe_unused]] static bool add_item(const std::wstring &text) { COST_SCOPE("advisor_briefing:add_item");
    if(!active())return false;
    if(text.empty() || text.size()>Inbox::text_limit)return false;
    if(inbox.collecting || !inbox.has_cycle || inbox.fallback)return false;
    Event e;
    e.kind=Kind::Reward;
    e.day=0;                                  // no "N일차 · " prefix in the card body
    e.text=text;
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
static void __fastcall time_wait(unsigned milliseconds,int *result,unsigned allow_skip) { COST_SCOPE("time_wait");
    const bool live=live_success_active();
    if(live)live_success_pass(1);   // the live success scene's own waits must run to completion
    bool expired=!live && result && !duel_running() && defer() && fn<int(__fastcall *)()>(GA(GA_335110))()==0 &&
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
static bool capture_message(unsigned path,int speaker,const wchar_t *text) { COST_SCOPE("advisor_briefing:capture_message");
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
    const bool capture_scene=mod_features.briefing16 && capture_scene_line(e.text);   // stored, and shown natively like a duel line
    if(path==2 && speaker>=0 && speaker<6001)name_speaker(e,pointer(world(),0x158+static_cast<unsigned>(speaker)*8));
    if(!message_gate(path,message_store_failed,store({e})))return false;
    return !duel_running() && !capture_scene;   // stored; a duel or capture line is also shown natively
}
static void __fastcall message_call(const wchar_t *text,int mode) { COST_SCOPE("message_call");
    bool captured=capture_message(0,0,text);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[0][captured?0:1]);
#endif
    if(!captured)original_message_call(text,mode);
}
static void __fastcall narration_call(int speaker,const wchar_t *text) { COST_SCOPE("narration_call");
    bool captured=capture_message(1,speaker,text);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[1][captured?0:1]);
#endif
    if(!captured)original_narration_call(speaker,text);
}
static NarrationCall original_person_call;
static void __fastcall person_call(int speaker,const wchar_t *text) { COST_SCOPE("person_call");
    bool captured=capture_message(2,speaker,text);
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
static bool capture_employ_scene(Obj params) { COST_SCOPE("advisor_briefing:capture_employ_scene");
    if(!scene_ready(4) || !message_gate(4,message_params,params!=nullptr))return false;
    Obj actor=pointer(params,0),target=pointer(params,8);std::wstring a,b;
    if(!message_gate(4,message_fields,scene_person(actor,a) && scene_person(target,b)))return false;
    const bool success=dword(params,0x10)!=0;
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
    // 41D550's remaining arguments are display-only bonus message/amount.
    return save_scene(6,Kind::News,actor,discovery_scene_summary(5,a,b,L"",true),discovery_scene_row(5,a,b,L"",true));
}
static void __fastcall employ_scene(Obj params) { COST_SCOPE("employ_scene");
    if(live_success_active())live_success_end(2);   // 2 next scene: the previous window ends before this one
    const bool success=mod_features.briefing16 && params && employ_pass_through(dword(params,0x10)!=0);
    bool captured=capture_employ_scene(params);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[4][captured?0:1]);
#endif
    if(success)live_success_begin(pointer(params,0),pointer(params,8));   // before the original scene speaks
    if(!captured || success)original_employ_scene(params);   // success: summary + the player-facing original scene
}
static void __fastcall discovery_scene(Obj params) { COST_SCOPE("discovery_scene");
    if(live_success_active())live_success_end(2);   // 2 next scene: the previous window ends before this one
    const bool success=mod_features.briefing16 && params && employ_pass_through(dword(params,0)==1 && dword(params,0x38)!=0);
    bool captured=capture_discovery_scene(params);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[5][captured?0:1]);
#endif
    if(success)live_success_begin(pointer(params,8),pointer(params,0x20));   // before the original scene speaks
    if(!captured || success)original_discovery_scene(params);   // recruited person: summary + the original scene
}
static void __fastcall scenery_scene(unsigned force,Obj actor,Obj scenery,unsigned message,unsigned amount) { COST_SCOPE("scenery_scene");
    bool captured=capture_scenery_scene(actor,scenery);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[6][captured?0:1]);
#endif
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
    Event event;event.kind=Kind::Weather;event.day=day();
    event.text=event.report=destiny_summary(force_name,city_name,fields[4],fields[5],fields[6]);
    return message_gate(7,message_store_failed,store({event}));
}
static void __fastcall destiny_call(Obj data) { COST_SCOPE("destiny_call");
    bool captured=capture_destiny(data);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[7][captured?0:1]);
#endif
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
    // A push may be suppressed only with its own successfully published yield.
    if(scene_calls[index+1].published && scene_ready(path) && message_gate(path,message_params,type==image+GA(GA_13AFEA0))) {
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
    if(!inbox.collecting)return;
    briefing_overlay::Model m;m.adviser=adviser_name();m.generation=inbox.generation;
    m.unread=inbox.unread();m.collecting=true;m.title=L"이번 순의 소식을 모으고 있습니다";
    m.body=L"진행이 끝나면 한데 모아 보고드리겠습니다.";
    bool show=!interaction();briefing_overlay::progress_flags(show,clean_scene());
    briefing_overlay::publish(m,show);
}
static void hud(Obj layout,bool eligible,bool modal) { COST_SCOPE("advisor_briefing:hud");
    briefing_overlay::set_extended(mod_features.briefing16);   // local17: the overlay has no feature table of its own
#ifndef NO_RUNTIME_LOG
    FrameTiming timing(0);
#endif
    if(speech_owner && (diplomatic_parent(true)!=speech_owner || !active()))restore_speech();
    if(!active()){briefing_overlay::publish({},false);return;}
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
    if(show)unified_overlay::install();
    unsigned generation=0,action=briefing_overlay::take(generation);
    std::vector<Card> cards;
    bool have_cards=action!=0,accepted_input=false;
    if(action)cards=inbox.cards();
    if(action && page>=cards.size())page=0;
    if(action && show && player && !inbox.collecting && generation==inbox.generation) {
        accepted_input=true;
        if(action==1)open=!open;
        else if(action==2){open=false;city_list=false;}
        else if(action==7){city_list=!city_list;if(city_list)refresh_cities();}
        else if(action>=100 && action<100+city_ids.size()) {
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
        else if(!city_list && action==3 && !cards.empty())page=(page+static_cast<unsigned>(cards.size())-1)%static_cast<unsigned>(cards.size());
        else if(!city_list && action==4 && !cards.empty())page=(page+1)%static_cast<unsigned>(cards.size());
        else if(!city_list && action==5 && !cards.empty()){inbox.erase(cards[page].kind);page=0;}
        else if(!city_list && action==6){inbox.clear();page=0;}
        if(!city_list && (action==5 || action==6))cards=inbox.cards();
        if(!city_list && open && !cards.empty() && cards[page].unread){inbox.mark(cards[page].kind);cards[page].unread=0;}
    }
    if(inbox.collecting){open=false;city_list=false;}
    if(rendered_generation==inbox.generation && rendered_page==page && rendered_open==open && rendered_collecting==inbox.collecting && rendered_adviser==last_adviser && rendered_city_list==city_list) {
        briefing_overlay::publish(display_cache,show,accepted_input);return;
    }
    if(!have_cards)cards=inbox.cards();if(page>=cards.size())page=0;
    briefing_overlay::Model m;m.adviser=adviser_name();m.generation=inbox.generation;m.unread=inbox.unread();m.open=open;m.collecting=inbox.collecting;m.city_list=city_list;m.page=page;m.pages=static_cast<unsigned>(cards.size());
    if(city_list) {
        m.title=L"보유한 성";
        m.cities=city_names;
    } else if(cards.empty()) {
        m.title=inbox.collecting?L"주공, 소식을 모으고 있습니다":L"주공, 새로 전할 소식이 없습니다";
        m.body=inbox.collecting?L"이번 순의 일이 끝나면 한데 모아 보고드리겠습니다.":L"다음 진행부터 천시·재해·성장·인재와 탐색 소식을 이곳에서 확인하실 수 있습니다.";
    } else {
        const auto &card=cards[page];
        m.title=std::wstring(caption(card.kind))+L" · "+std::to_wstring(card.events.size())+L"건";
        m.body=card_body(card);
        for(const auto &e:card.events) {
            collect_names(e.text,m.names);
            if(e.speaker && e.speaker<6001) {
                Obj who=pointer(root,0x158+e.speaker*8);std::wstring name;
                if(who && copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(who),name,100))add_name(m.names,name);
            }
        }
    }
    display_cache=std::move(m);rendered_generation=inbox.generation;rendered_page=page;rendered_open=open;rendered_collecting=inbox.collecting;rendered_adviser=last_adviser;rendered_city_list=city_list;
    briefing_overlay::publish(display_cache,show,accepted_input);(void)layout;(void)eligible;(void)modal;
}
static unsigned long long __fastcall user_enter(Obj state,Obj argument) { COST_SCOPE("advisor_briefing::user_enter"); COST_CRUMB("advisor_briefing_state",state);
    auto result=fn<unsigned long long(__fastcall *)(Obj,Obj)>(GA(GA_44C340))(state,argument);
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
    fn<void(__fastcall *)(Obj)>(GA(GA_44ED30))(state);
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
    fn<void(__fastcall *)(Obj,Obj)>(GA(GA_44B090))(state,argument);
}
static void __fastcall game_leave(Obj state,Obj argument) { COST_SCOPE("advisor_briefing::game_leave"); COST_CRUMB("advisor_briefing_state",state);reset();fn<void(__fastcall *)(Obj,Obj)>(GA(GA_44ACC0))(state,argument);}
static void __fastcall narration(Obj state) { COST_SCOPE("advisor_briefing::narration"); COST_CRUMB("advisor_briefing_state",state);
#ifndef NO_RUNTIME_LOG
    hook_called(3);
#endif
    fn<void(__fastcall *)(Obj)>(GA(GA_865170))(state);
}
static unsigned long long message_enter(Obj state,unsigned original,bool diplomatic) { COST_SCOPE("advisor_briefing:message_enter");
    bool accepted=false;
    const bool skip=diplomatic && dialogue_ready(false),watch=diplomatic && !skip && dialogue_observed(false);
#ifndef NO_RUNTIME_LOG
    if(diplomatic)skip_decision(0,false,skip);
#endif
    if(skip || watch) {
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
    auto result=fn<unsigned long long(__fastcall *)(Obj)>(original)(state);
    if(result && accepted) {
        // Create the native dialog and its completion binding first; close from
        // its first active update, never reenter the manager during Enter.
        simple_state=state;simple_states.push_back(state);offscreen(pointer(state,0x470));
    }
    return result;
}
static unsigned long long __fastcall simple_enter(Obj state){ COST_SCOPE("advisor_briefing::simple_enter"); COST_CRUMB("advisor_briefing_state",state);return message_enter(state,GA(GA_1F29B0),false);}
static unsigned long long __fastcall diplomatic_enter(Obj state){ COST_SCOPE("advisor_briefing::diplomatic_enter"); COST_CRUMB("advisor_briefing_state",state);return message_enter(state,GA(GA_812FF0),true);}
static unsigned long long __fastcall person_notice_enter(Obj state){ COST_SCOPE("advisor_briefing::person_notice_enter"); COST_CRUMB("advisor_briefing_state",state);return message_enter(state,GA(GA_1F2A60),false);}
static void __fastcall simple_frame(Obj state) { COST_SCOPE("advisor_briefing::simple_frame"); COST_CRUMB("advisor_briefing_state",state);
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
static bool record(Obj r,std::vector<Event> &batch) { COST_SCOPE("advisor_briefing:record");
    Event e;e.speaker=word(r,0x88);e.day=day();
    if(!copy(reinterpret_cast<const wchar_t *>(r+0x8A),e.text,0x55))return false;
    if(e.text.empty() && !copy(reinterpret_cast<const wchar_t *>(r+0x14),e.text,0x37))return false;
    if(!e.text.empty())batch.push_back(std::move(e));return true;
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
        if(!e.text.empty())batch.push_back(std::move(e));
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
using SpeechSetter=void(__fastcall *)(Obj,int,Obj,const wchar_t *,Obj);
static SpeechSetter original_speech;
static CodeLink speech_link;
using EventSpeech=void(__fastcall *)(Obj,Obj,const wchar_t *,unsigned long long,const wchar_t *,unsigned);
static EventSpeech original_event_speech;
static CodeLink event_link;
static bool capture_event(Obj owner,Obj person,const wchar_t *text) { COST_SCOPE("advisor_briefing:capture_event");
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
    const bool capture_scene=mod_features.briefing16 && capture_scene_line(e.text);
    name_speaker(e,person);
    if(!message_gate(3,message_store_failed,store({e})))return false;
    return !capture_scene;   // briefing16: the capture scene stays on screen; the briefing keeps the copy
}
static void __fastcall event_speech(Obj owner,Obj person,const wchar_t *text,unsigned long long flags,const wchar_t *extra,unsigned style) { COST_SCOPE("event_speech");
#ifndef NO_RUNTIME_LOG
    hook_called(6);
#endif
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
    bool capture=dialogue_ready(true) && owner==diplomatic_parent(true) && (slot==3 || slot==4);
    const bool watch=!capture && (slot==3 || slot==4) && dialogue_observed(true) && owner==diplomatic_observed(true);
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
    fn<void(__fastcall *)(Obj)>(GA(GA_83E300))(state);
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
        Event e;e.kind=Kind::Income;e.day=day();
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
    if(!ok || !store(std::move(batch),true)) {
        // Keep native controls and completion semantics on unknown data. The
        // caller-local replay protects irreversible result writers on retry.
        for(const auto &r:consumed)r.first[0x82]=(r.first[0x82]&0xF9)|(r.second&6);
        fallback();replay_results=action;
        result=fn<unsigned long long(__fastcall *)(Obj)>(GA(GA_67DB40))(state);
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
    std::vector<Event> batch;bool capture=defer(true) && pending_world_reports(batch);
    if(capture)briefing_overlay::mask_report(true);
    auto result=fn<unsigned long long(__fastcall *)(Obj)>(GA(GA_67DB40))(state);
    report_state=state;report_deferred=false;
    if(!capture || !result){briefing_overlay::mask_report(false);return result;}
    Obj action=pointer(state,0x488);unsigned n=action?dword(action,0x16280):262;
    bool ok=n<=261;
    for(unsigned i=0;ok && i<n;++i)ok=record(action+0x1B00+i*0x140,batch);
    if(ok && dword(action,0x16284))ok=record(action+0x16140,batch);
    if(!ok){fallback();return result;}
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
        fn<void(__fastcall *)(Obj)>(GA(GA_67DD20))(state);return;
    }
    if(state==report_state && report_deferred) {
        if(top_state()!=state)return;
        if(!defer(true)){report_deferred=false;fallback();fn<void(__fastcall *)(Obj)>(GA(GA_67DD20))(state);return;}
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
            Event e;e.kind=Kind::Income;e.day=day();
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
    fn<void(__fastcall *)(Obj)>(GA(GA_67DD20))(state);
}
static void __fastcall report_leave(Obj state,Obj context) { COST_SCOPE("advisor_briefing::report_leave"); COST_CRUMB("advisor_briefing_state",state);
#ifndef NO_RUNTIME_LOG
    ReportTiming timing(3);
#endif
    bool owned=state==report_state;
    if(owned){report_state=nullptr;report_deferred=false;report_headless=false;}
    fn<void(__fastcall *)(Obj,Obj)>(GA(GA_67D8A0))(state,context);
    if(owned && !report_state)briefing_overlay::mask_report(false);
}
static void __fastcall report_wait(Obj state) { COST_SCOPE("advisor_briefing::report_wait"); COST_CRUMB("advisor_briefing_state",state);if(defer(true) && top_state()==state){*reinterpret_cast<unsigned *>(state+0x470)=0;pop();return;}fn<void(__fastcall *)(Obj)>(GA(GA_67DEE0))(state);}
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
static void __fastcall log_frame(Obj layout) { COST_SCOPE("advisor_briefing::log_frame"); COST_CRUMB("advisor_briefing_layout",layout);
#ifndef NO_RUNTIME_LOG
    FrameTiming timing(1);
#endif
#ifndef NO_RUNTIME_LOG
    hook_called(1);
#endif
    bind(old_log,layout);fn<void(__fastcall *)(Obj)>(GA(GA_671E90))(layout);move(old_log,hide_native_hud() && replace_legacy());
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
    if(old_log.object==layout){move(old_log,false);old_log={};}
    fn<void(__fastcall *)(Obj,Obj)>(GA(GA_671AB0))(layout,context);
}
// U6 (user 2026-11-09 "캡처 말고 군사 초상화를 직접 불러오는 방식"): the face manager (GA_27D73E0) entry of the art,
// 8AA550(manager,10,art) -> index into manager+0x178, holds the face as a 512x512 BC3 texture one level below its
// +0x10 / +0x08 object at +0xD8. Evidence: local11/12 PW26 dumps advisor_face_<art>_0100001B (depth 1, slot 0x1B) were
// 관우 for art 2 and 제갈량 for art 45. Every pointer is read under SEH and the object must be a d3d11 COM object of that
// exact shape, so a wrong guess only falls back to the capture path. Returns an AddRef'd texture or null.
static uintptr_t face_d3d_low,face_d3d_high;
static ID3D11Texture2D *face_texture_from(Obj object) {
    if(!object)return nullptr;
    __try {
        const uintptr_t vt=*reinterpret_cast<uintptr_t *>(object);
        if(vt<face_d3d_low || vt>=face_d3d_high)return nullptr;
        IUnknown *unknown=reinterpret_cast<IUnknown *>(object);
        ID3D11Texture2D *texture=nullptr;ID3D11ShaderResourceView *view=nullptr;
        if(SUCCEEDED(unknown->QueryInterface(__uuidof(ID3D11ShaderResourceView),reinterpret_cast<void **>(&view))) && view) {
            ID3D11Resource *resource=nullptr;view->GetResource(&resource);view->Release();
            if(resource){resource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&texture));resource->Release();}
        } else unknown->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&texture));
        if(!texture)return nullptr;
        D3D11_TEXTURE2D_DESC desc;ZeroMemory(&desc,sizeof(desc));texture->GetDesc(&desc);
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
    for(unsigned offset:{0x10u,0x8u}) {
        Obj start=face_slot(e,offset);
        if(ID3D11Texture2D *texture=start?face_texture_from(face_slot(start,0xD8)):nullptr){where=offset;return texture;}
    }
    where=4;return nullptr;
}
static void __fastcall advice_frame(Obj layout) { COST_SCOPE("advisor_briefing::advice_frame"); COST_CRUMB("advisor_briefing_layout",layout);
#ifndef NO_RUNTIME_LOG
    FrameTiming timing(2);
#endif
#ifndef NO_RUNTIME_LOG
    hook_called(2);
#endif
    bind(old_advice,layout);fn<void(__fastcall *)(Obj)>(GA(GA_88E4B0))(layout);
    if(!active()){move(old_advice,false);return;}
    Obj adviser=pointer(layout,0x448);unsigned art=adviser?word(adviser,0xA6):~0u;
    if(adviser!=last_adviser || art!=last_art){last_adviser=adviser;last_art=art;++portrait_serial;}
    bool needs_portrait=replace_legacy() && adviser && !briefing_overlay::captured(portrait_serial);
    if(needs_portrait && mod_features.briefing16) {   // U6 (local17 briefing16): try the original face texture (at most every 250 ms); the capture below stays the fallback
        static ULONGLONG face_tried_at;static int face_offered=-1;
        const ULONGLONG now=GetTickCount64();
        if(face_offered!=portrait_serial && (!face_tried_at || now-face_tried_at>=250)) {
            face_tried_at=now;unsigned where=0;
            if(ID3D11Texture2D *texture=face_texture_for(art,where)){briefing_overlay::offer_face(texture,portrait_serial);face_offered=portrait_serial;}
#ifndef NO_RUNTIME_LOG
            static unsigned face_logs;
            if(face_logs<60){++face_logs;log_event("advisor_face_lookup",static_cast<unsigned>(portrait_serial),art,where);}
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
    fn<void(__fastcall *)(Obj,Obj)>(GA(GA_895A10))(layout,context);
}
static void *replacements[]={reinterpret_cast<void *>(&user_enter),reinterpret_cast<void *>(&user_leave),reinterpret_cast<void *>(&game_leave),reinterpret_cast<void *>(&narration),reinterpret_cast<void *>(&simple_enter),reinterpret_cast<void *>(&simple_frame),reinterpret_cast<void *>(&report_enter),reinterpret_cast<void *>(&report_frame),reinterpret_cast<void *>(&report_leave),reinterpret_cast<void *>(&report_wait),reinterpret_cast<void *>(&log_frame),reinterpret_cast<void *>(&log_leave),reinterpret_cast<void *>(&advice_frame),reinterpret_cast<void *>(&advice_leave),reinterpret_cast<void *>(&diplomatic_enter),reinterpret_cast<void *>(&simple_frame),reinterpret_cast<void *>(&diplomatic_wait),reinterpret_cast<void *>(&user_frame),reinterpret_cast<void *>(&person_notice_enter),reinterpret_cast<void *>(&simple_frame)};
static bool install() { COST_SCOPE("advisor_briefing:install");
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
    bool published=publish_link(text_link) && publish_link(speech_link);
    for(unsigned i=0;i<3 && published;++i)published=publish_link(message_links[i]);
    published=published && publish_link(event_link);
    for(unsigned i=0;i<3 && published;++i)if(i>=2 || !scene_yield)published=publish_link(scene_links[i]);
    if(scene_yield)log_event("advisor_scene_yield",other_mod_present(L"SAN14_SceneSkip.dll")?1:2,0,0);
    for(unsigned i=0;i<2 && published;++i)published=publish_link(picture_links[i]);
    published=published && publish_link(time_wait_link);
    published=published && scene_call_publish(report_results_call);
    if(published)publish_scene_calls();
    if(published)for(;n<sizeof(slots)/sizeof(*slots);++n)if(!patch_pointer(slots[n].rva,slots[n].original,replacements[n]))break;
    if(n!=sizeof(slots)/sizeof(*slots)) {
        call_restore(report_results_call);restore_scene_calls();
        while(n){--n;restore_pointer(slots[n].rva,slots[n].original,replacements[n]);}restore_link(time_wait_link);for(unsigned i=2;i;)restore_link(picture_links[--i]);for(unsigned i=3;i;)restore_link(scene_links[--i]);restore_link(event_link);for(unsigned i=3;i;)restore_link(message_links[--i]);restore_link(speech_link);restore_link(text_link);return false;
    }
#ifndef NO_RUNTIME_LOG
    log_event("advisor_briefing_entry_schema",message_entry_paths,message_reason_columns);
    log_event("advisor_briefing_picture_schema",picture_count_slots);
#endif
    log_event("advisor_briefing_installed");return true;
}
#endif
}
static bool advisor_briefing_guards_match(){return advisor_briefing::guards_match();}
