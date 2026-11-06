// Convenience DLL; original UI callbacks only. No save writes or remote calls.
#include "game_addresses.h"
#include "advisor_briefing_profile.h"
namespace advisor_briefing {
static bool scene_yield=false;
// local5: a failing guard reports which sub-guard it was (diagnostic group, one line per failure):
// advisor_guard_failed(part, index, rva): part 1 callee prologue (index = row of the prologue list), 2 hash row (index in guards[]),
// 3 vtable slot (index in slots[]), 4 phase call opcode, 5 phase call target.
static bool guard_failed(unsigned part,unsigned index,unsigned rva) {
#ifndef NO_RUNTIME_LOG
    log_event("advisor_guard_failed",part,index,rva);
#else
    (void)part;(void)index;(void)rva;
#endif
    return false;
}
static bool guards_match() {
    struct Prologue {unsigned rva;const unsigned char *bytes;unsigned size;};
    const Prologue prologues[]={
        {GA(GA_1F0AD0),message_prologue,sizeof(message_prologue)},{GA(GA_1F09E0),narration_prologue,sizeof(narration_prologue)},
        {GA(GA_304640),narration_prologue,sizeof(narration_prologue)},{GA(GA_1F0DD0),person_prologue,sizeof(person_prologue)},
        {GA(GA_842380),event_prologue,sizeof(event_prologue)},{GA(GA_41D550),scenery_prologue,sizeof(scenery_prologue)},
        {GA(GA_451FF0),picture_show_bytes,sizeof(picture_show_bytes)},{GA(GA_43FF90),picture_close_bytes,sizeof(picture_close_bytes)}};
    for(unsigned i=0;i<sizeof(prologues)/sizeof(prologues[0]);++i)
        if(ga::mcmp_at(image,prologues[i].rva,prologues[i].bytes,prologues[i].size))return guard_failed(1,i,prologues[i].rva);
    scene_yield=other_mod_present(L"SAN14_SceneSkip.dll") || ga::mcmp(image,GA_6C43B0,0,scene_prologue,sizeof(scene_prologue)) || ga::mcmp(image,GA_6C4670,0,scene_prologue,sizeof(scene_prologue));
    unsigned row=0;
    for(const auto &g:guards) {
        const unsigned index=row++;
        if(scene_yield && (g.rva==GA(GA_6C43B0) || g.rva==GA(GA_6C4670)))continue;   // owned by the other mod's prologue hook
        ga::ObserverSet used;   // local11: shared observer tolerance (ga::hash_ok); feature code 48 = advisor_briefing gate id
        if(!ga::hash_ok(image,g.rva,g.size,g.hash,&used))return guard_failed(2,index,g.rva);
        for(unsigned k=0;k<used.count;++k)log_event("observer_hook_tolerated",48,used.rva[k]);
    }
    unsigned slot_index=0;
    for(const auto &s:slots) {
        if(pointer(image,s.rva)!=image+s.original)return guard_failed(3,slot_index,s.rva);
        ++slot_index;
    }
    if(image[GA(GA_81133D)]!=0xE8)return guard_failed(4,0,GA(GA_81133D));
    int phase_call=*reinterpret_cast<int *>(image+GA(GA_81133E));
    if(phase_call!=static_cast<int>(GA(GA_815EA0)-GA(GA_811342))) {
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
        const auto &owned=diplomacy_intel::links[6];
        if(!owned.published || owned.site!=GA(GA_81133D) || owned.redirected!=phase_call)return guard_failed(5,0,GA(GA_81133E));
#else
        return guard_failed(5,0,GA(GA_81133E));
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
static void hook_called(unsigned hook) {
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
static bool wait_condition(unsigned path,WaitReason reason,bool value) {
    if(!value)InterlockedIncrement64(&wait_failures[path][reason]);
    return value;
}
static bool wait_owner_matches(unsigned path,Obj owner,Obj parent) {
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
static void layout_position(Obj object,int x,int y) {
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&layout_move_calls);
    for(unsigned i=0;i<4;++i)if(measured_frames&(1u<<i))InterlockedIncrement64(&frame_metrics[i][3]);
#endif
    fn<void(__fastcall *)(Obj,int,int)>(GA(GA_874650))(object,x,y);
}
static unsigned page=0;
static bool open=false;
static bool detail=false,rendered_detail=false;
static Obj report_state,simple_state;
static std::vector<Obj> simple_states;
static bool report_deferred;
static bool report_headless;
static CallLink report_results_call;
static thread_local Obj replay_results;
static unsigned rendered_generation=~0u,rendered_page=~0u;
static bool rendered_open=false;
static bool rendered_collecting=false;
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
static void diagnostic_cycle() {
    if(diagnostic_generation==inbox.generation)return;
    diagnostic_generation=inbox.generation;last_top_rva=0;
    collected_top_rvas.clear();uncollected_top_rvas.clear();
}
static unsigned state_rva(Obj state) {
    auto table=state?reinterpret_cast<std::uintptr_t>(pointer(state,0)):0;
    auto base=reinterpret_cast<std::uintptr_t>(image);
    return table>=base && table-base<0x29FF000?static_cast<unsigned>(table-base):0;
}
struct Stall {Obj state;unsigned path,rva;ULONGLONG started;};
static std::vector<Stall> stalls;
static unsigned long long stall_recent_ms[6]{},stall_max_ms[6]{};
// Paths: simple=0, person=1, narration=2, passive=3, recruitment=4, diplomatic=5.
static void stall_begin(Obj state,unsigned path) {
    if(!state)return;
    for(const auto &s:stalls)if(s.state==state)return;
    stalls.push_back({state,path,state_rva(state),GetTickCount64()});
}
static void stall_end(Obj state) {
    for(auto it=stalls.begin();it!=stalls.end();++it)if(it->state==state) {
        auto ms=GetTickCount64()-it->started;stall_recent_ms[it->path]=ms;
        stall_max_ms[it->path]=(std::max)(stall_max_ms[it->path],ms);
        log_event("advisor_briefing_stall",it->path,static_cast<unsigned>(ms),it->rva);
        stalls.erase(it);return;
    }
}
static void collected_state(Obj state) {
    diagnostic_cycle();unsigned rva=state_rva(state);
    if(rva && std::find(collected_top_rvas.begin(),collected_top_rvas.end(),rva)==collected_top_rvas.end())collected_top_rvas.push_back(rva);
}
#endif
static Obj manager(){return fn<Obj(__fastcall *)()>(GA(GA_110E0))();}
static Obj top_state() {
    Obj m=manager();if(!m)return nullptr;
    const auto count=*reinterpret_cast<const unsigned long long *>(m+0x10);
    Obj states=pointer(m,0x20);return states && count && count<=256?pointer(states,static_cast<unsigned>((count-1)*8)):nullptr;
}
static bool interaction() {
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
static bool clean_scene() {
    if(inbox.fallback || interaction())return false;
    Obj m=manager(),states=m?pointer(m,0x20):nullptr;
    auto count=m?*reinterpret_cast<const unsigned long long *>(m+0x10):257;
    if(count>256 || (count && !states))return false;
    // Inspect only live stack entries, never an address retained after Leave.
    for(unsigned i=0;i<count;++i) {
        Obj state=pointer(states,i*8);if(!state)return false;
        Obj table=pointer(state,0);
        if(table==image+GA(GA_13A2F00) || table==image+GA(GA_13A2F98) || table==image+GA(GA_145F9E8)) {
            Obj dialog=pointer(state,0x470);if(dialog && dword(dialog,0x20)!=10000)return false;
        }
    }
    return true;
}
static Obj diplomatic_parent(bool result_only) {
    Obj m=manager();if(!m)return nullptr;
    const auto count=*reinterpret_cast<const unsigned long long *>(m+0x10);
    Obj states=pointer(m,0x20);if(!states || count>256)return nullptr;
    Obj parent=nullptr;
    for(unsigned i=0;i<count;++i) {
        Obj state=pointer(states,i*8);if(!state)return nullptr;
        const char *name=reinterpret_cast<const char *>(state+0x70);size_t n=0;for(;n<128 && name[n];++n){}
        if(n==128)return nullptr;
        std::string value(name,n);
        if(value.find("YesNo")!=std::string::npos || value.find("Select")!=std::string::npos || value.find("TreatPrisoner")!=std::string::npos || (value.find("Input")!=std::string::npos && pointer(state,0)!=image+GA(GA_1465260)))return nullptr;
        if(pointer(state,0)==image+GA(GA_145DBB8))parent=state;
    }
    if(!parent)return nullptr;
    unsigned phase=dword(parent,0x4D8);
    return (phase>=12 && phase<=15) || (!result_only && (phase==8 || phase==11))?parent:nullptr;
}
static unsigned force_id(){return ::player_force();}
static bool active(){return enabled && mod_features.advisor_briefing && world() && force_id();}
static bool tutorial() {
    Obj t=fn<Obj(__fastcall *)()>(GA(GA_56EE40))();
    Obj context=world()?pointer(world(),0x871E8):nullptr;
    // +454 is the remaining order count, not a tutorial flag (44A0F0).
    return (t && dword(t,0)) || !context;
}
static unsigned day() {
    int current=game_day();return started_day>=0 && current>=started_day?
        static_cast<unsigned>((std::min)(10,current-started_day+1)):0;
}
static bool defer(bool final_report=false){return active() && (inbox.collecting || (final_report && inbox.has_cycle)) && !inbox.fallback && briefing_overlay::available() && !tutorial() && !interaction();}
static bool dialogue_ready(bool result_only){return active() && inbox.has_cycle && !inbox.fallback && briefing_overlay::available() && !tutorial() && diplomatic_parent(result_only);}
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
    if(!suppressed)fn<void(__fastcall *)(Obj,int,int)>(GA(GA_842A90))(pointer(object,0x1B8),id,0);
}
static void __fastcall picture_close(Obj object) { COST_SCOPE("advisor_briefing::picture_close"); COST_CRUMB("advisor_briefing_object",object);
    auto it=std::find_if(pictures.begin(),pictures.end(),[&](const Picture &p){return p.object==object;});
    bool recorded=it!=pictures.end(),suppressed=recorded && it->suppressed;
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&picture_close_counts[suppressed?0:recorded?1:2]);
#endif
    if(recorded)pictures.erase(it); // Do not retain an iterator across a native call.
    // Match the most recent show, even if collecting/interaction changed since it.
    if(!suppressed)fn<void(__fastcall *)(Obj)>(GA(GA_835230))(pointer(object,0x1B8));
}
#ifndef NO_RUNTIME_LOG
static bool wait_ready_diagnostic(unsigned path,bool diplomatic=false) {
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
static bool copy(const wchar_t *s,std::wstring &out,size_t maximum=Inbox::text_limit) {
    size_t n=0;if(!raw_length(s,maximum,n))return false;out.assign(s,n);return true;
}
// User 2026-10-28: show the native bottom-left advice and log as in the
// original game; the bubble report stays. move() remains for restoring.
static bool hide_native_hud(){return false;}
static void move(Legacy &l,bool hide) {
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
static void bind(Legacy &l,Obj p) {
    if(l.object==p)return;
    l={p,static_cast<int>(dword(p,0x20)),static_cast<int>(dword(p,0x24)),false};
}
static void restore_speech() {
    Obj parent=diplomatic_parent(false);
    Obj layout=parent && parent==speech_owner?pointer(parent,0x478):nullptr;
    for(unsigned i=0;i<2;++i) {
        if(layout && pointer(layout,0x178+(i+3)*8)==speech_windows[i].object)move(speech_windows[i],false);
        speech_windows[i]={};
    }
    speech_owner=nullptr;speech_waiting=false;++speech_serial;
}
static bool replace_legacy(){return active() && !inbox.fallback && briefing_overlay::available() && !interaction();}
static void fallback() {
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
static void reset() {
    clear_scene_pending();
    restore_speech();briefing_overlay::mask_report(false);
    move(old_log,false);move(old_advice,false);old_log={};old_advice={};
    inbox.reset(0,0);open=false;detail=false;page=0;report_state=simple_state=nullptr;simple_states.clear();pictures.clear();report_deferred=false;report_headless=false;
    last_adviser=nullptr;last_art=~0u;++portrait_serial;briefing_overlay::forget();
#ifndef NO_RUNTIME_LOG
    stalls.clear();
#endif
}
static void refresh_progress();
static bool store(std::vector<Event> batch,bool final_report=false) {
#ifndef NO_RUNTIME_LOG
    if(stored_event_bytes!=static_cast<LONG>(sizeof(Event))){fallback();return false;}
#endif
    if(!inbox.append(std::move(batch),final_report)){fallback();return false;}
#ifndef NO_RUNTIME_LOG
    collected_state(top_state());
#endif
    refresh_progress();return true;
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
static bool message_gate(unsigned path,MessageReason reason,bool value) {
#ifndef NO_RUNTIME_LOG
    if(!value)InterlockedIncrement64(&message_pass_reasons[path][reason]);
#else
    (void)path;(void)reason;
#endif
    return value;
}
static bool entry_ready(unsigned path) {
    // Same gates/order as defer(); evaluate native predicates only on this UI caller.
    if(!message_gate(path,message_inactive,active()) ||
       !message_gate(path,message_not_collecting,inbox.collecting) ||
       !message_gate(path,message_fallback,!inbox.fallback) ||
       !message_gate(path,message_overlay,briefing_overlay::available()) ||
       !message_gate(path,message_tutorial,!tutorial()) ||
       !message_gate(path,message_interaction,!interaction()))return false;
    if(!message_gate(path,message_native_mode,fn<int(__fastcall *)()>(GA(GA_332E90))()==0))return false;
    return true;
}
using TimeWait=void(__fastcall *)(unsigned,int *,unsigned);
static TimeWait original_time_wait;
static CodeLink time_wait_link;
// 2026-11-01 user: duel (일기토) lines play as in the original but are still collected. 37ACF0 (duel end) clears
// CDuelManager(*2783730)+10, 448C10 tests it as "duel running"; pure read.
static bool duel_running(){Obj m=pointer(image,GA(GA_2783730));return m && dword(m,0x10)!=0;}
static void __fastcall time_wait(unsigned milliseconds,int *result,unsigned allow_skip) { COST_SCOPE("time_wait");
    bool expired=result && !duel_running() && defer() && fn<int(__fastcall *)()>(GA(GA_332E90))()==0 &&
        fn<Obj(__fastcall *)(Obj)>(GA(GA_56FC20))(manager())!=nullptr;
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&time_wait_counts[expired?0:1]);
#endif
    // Native timeout=0; skip=1 changes disaster callers' later log/effect branches.
    if(expired){*result=0;return;}
    original_time_wait(milliseconds,result,allow_skip);
}
static void name_speaker(Event &e,Obj person) {
    if(!person)return;
    e.speaker=word(person,0x10);std::wstring name;
    if(copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_22EEC0))(person),name,100) && !name.empty())e.text=name+L"\n"+e.text;
}
static bool capture_message(unsigned path,int speaker,const wchar_t *text) {
    if(!entry_ready(path))return false;
    Obj context=pointer(image,GA(GA_27842A0));
    if(!message_gate(path,message_native_context,!context || dword(context,0x1B0)==0) ||
       !message_gate(path,message_no_caller,fn<Obj(__fastcall *)(Obj)>(GA(GA_56FC20))(manager())!=nullptr))return false;
    Event e;e.speaker=static_cast<unsigned>(speaker);e.day=day();
    if(!message_gate(path,message_bad_text,copy(text,e.text) && !e.text.empty()))return false;
    if(path==2 && speaker>=0 && speaker<6001)name_speaker(e,pointer(world(),0x158+static_cast<unsigned>(speaker)*8));
    if(!message_gate(path,message_store_failed,store({e})))return false;
    return !duel_running();   // stored; a duel line is also shown natively
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
static bool scene_ready(unsigned path) {
    return entry_ready(path) && message_gate(path,message_no_caller,fn<Obj(__fastcall *)(Obj)>(GA(GA_56FC20))(manager())!=nullptr);
}
static bool scene_person(Obj person,std::wstring &name) {
    return person && copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_22EEC0))(person),name,100) && !name.empty();
}
static bool scene_object_name(Obj object,std::wstring &name) {
    return object && copy(reinterpret_cast<const wchar_t *>(object+0x10),name,100) && !name.empty();
}
static bool scene_advantage(Obj place,std::wstring &name) {
    if(!place)return false;
    Obj handle=fn<Obj(__fastcall *)(Obj)>(GA(GA_3F5A0))(place);
    return handle && scene_object_name(fn<Obj(__fastcall *)(Obj)>(GA(GA_925B0))(handle),name);
}
static bool save_scene(unsigned path,Kind kind,Obj actor,const std::wstring &summary) {
    if(!message_gate(path,message_fields,!summary.empty()))return false;
    Event e;e.kind=kind;e.day=day();e.speaker=word(actor,0x10);e.text=e.report=summary;
    return message_gate(path,message_store_failed,store({e}));
}
static bool capture_employ_scene(Obj params) {
    if(!scene_ready(4) || !message_gate(4,message_params,params!=nullptr))return false;
    Obj actor=pointer(params,0),target=pointer(params,8);std::wstring a,b;
    if(!message_gate(4,message_fields,scene_person(actor,a) && scene_person(target,b)))return false;
    return save_scene(4,Kind::Recruitment,actor,recruitment_scene_summary(a,b,dword(params,0x10)!=0));
}
static bool capture_discovery_scene(Obj params) {
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
    return save_scene(5,kind,actor,discovery_scene_summary(type,a,b,place,dword(params,0x38)!=0));
}
static bool capture_scenery_scene(Obj actor,Obj scenery) {
    if(!scene_ready(6))return false;
    std::wstring a,b;if(!message_gate(6,message_fields,scene_person(actor,a) && scene_object_name(scenery,b)))return false;
    // 41D550's remaining arguments are display-only bonus message/amount.
    return save_scene(6,Kind::News,actor,discovery_scene_summary(5,a,b,L"",true));
}
static void __fastcall employ_scene(Obj params) { COST_SCOPE("employ_scene");
    bool captured=capture_employ_scene(params);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[4][captured?0:1]);
#endif
    if(!captured)original_employ_scene(params);
}
static void __fastcall discovery_scene(Obj params) { COST_SCOPE("discovery_scene");
    bool captured=capture_discovery_scene(params);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&message_entry_counts[5][captured?0:1]);
#endif
    if(!captured)original_discovery_scene(params);
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
static Obj scene_call_target(unsigned index) {
    const auto &site=scene_sites[index];if(image[site.rva]!=0xE8)return nullptr;
    int delta;memcpy(&delta,image+site.rva+1,4);
    auto begin=reinterpret_cast<std::uintptr_t>(image);
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+reinterpret_cast<IMAGE_DOS_HEADER *>(image)->e_lfanew);
    auto end=begin+pe->OptionalHeader.SizeOfImage;
    auto target=reinterpret_cast<std::uintptr_t>(image+site.rva+5)+delta;
    return target && (target==begin+site.original || target<begin || target>=end)?reinterpret_cast<Obj>(target):nullptr;
}
static bool scene_call_publish(CallLink &call) {
    int delta;memcpy(&delta,image+call.site+1,4);
    return image[call.site]==0xE8 && delta==call.original && call_publish(call);
}
static void restore_scene_calls() {
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
static bool destiny_names(unsigned force,unsigned city,std::wstring &force_name,std::wstring &city_name) {
    if(!force || force>51 || !city || city>61)return false;
    Obj owner=table(0xDF30,force,51),building=table(0x6DA98,city,3000);
    if(!owner || !building || !copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_22ECC0))(owner),force_name,100) || force_name.empty())return false;
    // Reuse native_ai_trace's building -> centre hex -> area-name lookup.
    unsigned hex=word(building,0x10);Obj tile=hex<48400?table(0xE270,hex,48399):nullptr;
    unsigned area=tile?word(tile,0x12):0;Obj place=area && area<=500?table(0x6CAF0,area,500):nullptr;
    return place && copy(reinterpret_cast<const wchar_t *>(place+0x10),city_name,100) && !city_name.empty();
}
static bool capture_destiny(Obj data) {
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
static bool copy_scene_list(Obj list,std::vector<Event> &batch,Kind kind) {
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
    if(scene_calls[index+1].published && scene_ready(path) && message_gate(path,message_params,type==image+GA(GA_13ACE30))) {
        Obj caller=fn<Obj(__fastcall *)(Obj)>(GA(GA_56FC20))(state_manager);
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
static void prepare_scene_calls(Obj relay) {
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
static void publish_scene_calls() {
    for(unsigned i=0;i<scene_site_count;++i)if(scene_next[i] && !scene_call_publish(scene_calls[i]))log_event("advisor_scene_call_skip",scene_sites[i].rva,scene_sites[i].original,2);
}
static bool prepare_message_link(CodeLink &link,unsigned rva,const unsigned char *prologue,unsigned size,Obj at,void *hook) {
    if(size<5 || (rva&15u)+size>16 || ga::mcmp_at(image,rva,prologue,size))return false;
    absolute_jump(at,hook);memcpy(at+32,image+rva,size);absolute_jump(at+32+size,image+rva+size);
    if(!prepare_link(link,rva,0xE9,at))return false;
    // Keep the shared five-byte CodeLink contract. Only this local entry patch
    // pads the rest of its displaced whole instructions inside the same CAS16.
    memset(link.modified+(rva-link.block)+5,0x90,size-5);return true;
}
static bool prepare_picture_link(CodeLink &link,unsigned rva,const unsigned char *bytes,unsigned size,Obj at,void *hook) {
    if(size<5 || (rva&15u)+size>16 || ga::mcmp_at(image,rva,bytes,size))return false;
    absolute_jump(at,hook); // Only our destination; no displaced native bytes/rel32.
    if(!prepare_link(link,rva,0xE9,at))return false;
    memset(link.modified+(rva-link.block)+5,0x90,size-5);return true;
}
static void pop(){fn<void(__fastcall *)(Obj)>(GA(GA_12540))(manager());}
static void offscreen(Obj p){if(p)layout_position(p,10000,10000);}
static std::wstring adviser_name() {
    std::wstring name;
    if(last_adviser)copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_22EEC0))(last_adviser),name,100);
    return name.empty()?L"군사":name;
}
static DiplomaticFacts report_facts(Obj owner,Obj person) {
    DiplomaticFacts facts;Obj context=world()?pointer(world(),0x871E8):nullptr;
    Obj player=context?fn<Obj(__fastcall *)(Obj)>(GA(GA_32E910))(context):nullptr;
    if(player && person) {
        facts.friendly=fn<Obj(__fastcall *)(Obj)>(GA(GA_22CA00))(person)==player;
        facts.ruler=facts.friendly && word(person,0x10)==word(player,0x10);
    }
    Obj layout=owner?pointer(owner,0x4C0):nullptr,window=layout?pointer(layout,0x188):nullptr;
    if(window) {
        Obj from=pointer(window,0x180),to=pointer(window,0x190);
        if(player && from && to && (from==player || to==player)) {
            facts.known_sides=true;facts.incoming=to==player;Obj other=from==player?to:from;
            copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_22ECC0))(other),facts.counterpart,100);
        }
        Obj command=pointer(window,0x1D8),vtable=command?pointer(command,0):nullptr;
        if(vtable)facts.kind=(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(vtable+0x98))(command);
    }
    if(owner && dword(owner,0x4D8)>=12 && dword(owner,0x4D8)<=15) {
        unsigned result=dword(owner,0x4F8);if(result<=1)facts.result=static_cast<int>(result);
    }
    return facts;
}
static void refresh_progress() {
    // The command HUD can stop updating while days advance. Notifications push
    // their badge snapshot directly from this original UI callback instead.
    if(!inbox.collecting)return;
    briefing_overlay::Model m;m.adviser=adviser_name();m.generation=inbox.generation;
    m.unread=inbox.unread();m.collecting=true;m.title=L"이번 순의 소식을 모으고 있습니다";
    m.body=L"진행이 끝나면 한데 모아 보고드리겠습니다.";
    bool show=!interaction();briefing_overlay::progress_flags(show,clean_scene());
    briefing_overlay::publish(m,show);
}
static void hud(Obj layout,bool eligible,bool modal) {
#ifndef NO_RUNTIME_LOG
    FrameTiming timing(0);
#endif
    if(speech_owner && (diplomatic_parent(true)!=speech_owner || !active()))restore_speech();
    if(!active()){briefing_overlay::publish({},false);return;}
    Obj root=world();unsigned force=force_id();
    if(inbox.world && (inbox.world!=reinterpret_cast<std::uintptr_t>(root) || inbox.force!=force))reset();
    inbox.context(reinterpret_cast<std::uintptr_t>(root),force);
    bool player=fn<bool(__fastcall *)(Obj,const char *)>(GA(GA_56FE50))(manager(),"CUserStrategyState");
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
        else if(action==2)open=false;
        else if(action==3 && !cards.empty())page=(page+static_cast<unsigned>(cards.size())-1)%static_cast<unsigned>(cards.size());
        else if(action==4 && !cards.empty())page=(page+1)%static_cast<unsigned>(cards.size());
        else if(action==5 && !cards.empty()){inbox.erase(cards[page].kind);page=0;}
        else if(action==6){inbox.clear();page=0;}
        else if(action==7)detail=!detail;
        if(action==3 || action==4 || action==5 || action==6)detail=false;
        if(action==5 || action==6)cards=inbox.cards();
        if(open && !cards.empty() && cards[page].unread){inbox.mark(cards[page].kind);cards[page].unread=0;}
    }
    if(inbox.collecting)open=false;
    if(rendered_generation==inbox.generation && rendered_page==page && rendered_open==open && rendered_collecting==inbox.collecting && rendered_adviser==last_adviser && rendered_detail==detail) {
        briefing_overlay::publish(display_cache,show,accepted_input);return;
    }
    if(!have_cards)cards=inbox.cards();if(page>=cards.size())page=0;
    briefing_overlay::Model m;m.adviser=adviser_name();m.generation=inbox.generation;m.unread=inbox.unread();m.open=open;m.detail=detail;m.collecting=inbox.collecting;m.page=page;m.pages=static_cast<unsigned>(cards.size());
    if(cards.empty()) {
        m.title=inbox.collecting?L"주공, 소식을 모으고 있습니다":L"주공, 새로 전할 소식이 없습니다";
        m.body=inbox.collecting?L"이번 순의 일이 끝나면 한데 모아 보고드리겠습니다.":L"다음 진행부터 천시·재해·성장·인재와 탐색 소식을 이곳에서 확인하실 수 있습니다.";
    } else {
        const auto &card=cards[page];
        m.title=std::wstring(caption(card.kind))+L" · "+std::to_wstring(card.events.size())+L"건";
        const bool complete_gold=std::all_of(card.events.begin(),card.events.end(),[](const Event &e){return e.gold!=0;});
        if(card.kind==Kind::Gold && card.gold && complete_gold)m.body=L"발견한 금 합계 "+std::to_wstring(card.gold)+L"\n\n";
        for(const auto &e:card.events) {
            m.body+=L"• "+(e.day?std::to_wstring(e.day)+L"일차 · ":L"")+(detail?display_text(e.text):compact(e))+(card.kind==Kind::Gold && !detail?L"\n":L"\n\n");
            collect_names(e.text,m.names);
            if(e.speaker && e.speaker<6001) {
                Obj who=pointer(root,0x158+e.speaker*8);std::wstring name;
                if(who && copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_22EEC0))(who),name,100))add_name(m.names,name);
            }
        }
    }
    display_cache=std::move(m);rendered_generation=inbox.generation;rendered_page=page;rendered_open=open;rendered_collecting=inbox.collecting;rendered_adviser=last_adviser;rendered_detail=detail;
    briefing_overlay::publish(display_cache,show,accepted_input);(void)layout;(void)eligible;(void)modal;
}
static unsigned long long __fastcall user_enter(Obj state,Obj argument) { COST_SCOPE("advisor_briefing::user_enter"); COST_CRUMB("advisor_briefing_state",state);
    auto result=fn<unsigned long long(__fastcall *)(Obj,Obj)>(GA(GA_44A0F0))(state,argument);
    if(active()){inbox.finish();open=false;detail=false;page=0;}
    return result;
}
static void __fastcall user_frame(Obj state) { COST_SCOPE("advisor_briefing::user_frame"); COST_CRUMB("advisor_briefing_state",state);
#ifndef NO_RUNTIME_LOG
    FrameTiming timing(3);
#endif
#ifndef NO_RUNTIME_LOG
    hook_called(0);
#endif
    fn<void(__fastcall *)(Obj)>(GA(GA_44CAF0))(state);
    // UI-only actions must not wait for the command HUD's refresh cadence.
    // Stay on this original game/UI frame; never call native functions from
    // Present/WndProc, and do not touch state after its original frame returned.
    if(active() && top_state()==state) {
        hud(nullptr,true,false);
    }
}
static void __fastcall user_leave(Obj state,Obj argument) { COST_SCOPE("advisor_briefing::user_leave"); COST_CRUMB("advisor_briefing_state",state);
    if(active()) {
        inbox.begin(reinterpret_cast<std::uintptr_t>(world()),force_id());open=false;page=0;
        started_day=game_day();
        refresh_progress();
        log_event("advisor_briefing_cycle_begin",force_id(),inbox.generation);
    }
    fn<void(__fastcall *)(Obj,Obj)>(GA(GA_448E40))(state,argument);
}
static void __fastcall game_leave(Obj state,Obj argument) { COST_SCOPE("advisor_briefing::game_leave"); COST_CRUMB("advisor_briefing_state",state);reset();fn<void(__fastcall *)(Obj,Obj)>(GA(GA_448A70))(state,argument);}
static void __fastcall narration(Obj state) { COST_SCOPE("advisor_briefing::narration"); COST_CRUMB("advisor_briefing_state",state);
#ifndef NO_RUNTIME_LOG
    hook_called(3);
#endif
    fn<void(__fastcall *)(Obj)>(GA(GA_862E40))(state);
}
static unsigned long long message_enter(Obj state,unsigned original,bool diplomatic) {
    bool accepted=false;
    if(diplomatic && dialogue_ready(false)) {
        std::vector<Event> batch;Obj head=pointer(state,0x478),node=head?pointer(head,0):nullptr;
        size_t count=static_cast<size_t>(*reinterpret_cast<unsigned long long *>(state+0x480));
        bool ok=head && count>0 && count<=256;
        for(size_t i=0;ok && i<count;++i) {
            if(!node || node==head){ok=false;break;}
            Event e;e.day=day();if(diplomatic)e.kind=Kind::Diplomacy;
            if(pointer(state,0)==image+GA(GA_13A2F98))e.speaker=dword(state,0x488);
            ok=copy(reinterpret_cast<const wchar_t *>(pointer(node,0x10)),e.text) && !e.text.empty() && (diplomatic || passive_notice(e.text));
            if(ok && diplomatic)e.report=diplomatic_report(e.text,report_facts(diplomatic_parent(false),nullptr));
            if(ok)batch.push_back(std::move(e));node=pointer(node,0);
        }
        accepted=ok && node==head && store(std::move(batch),diplomatic);
#ifndef NO_RUNTIME_LOG
        if(accepted)stall_begin(state,diplomatic?5:pointer(state,0)==image+GA(GA_13A2F98)?1:0);
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
static unsigned long long __fastcall simple_enter(Obj state){ COST_SCOPE("advisor_briefing::simple_enter"); COST_CRUMB("advisor_briefing_state",state);return message_enter(state,GA(GA_1F15F0),false);}
static unsigned long long __fastcall diplomatic_enter(Obj state){ COST_SCOPE("advisor_briefing::diplomatic_enter"); COST_CRUMB("advisor_briefing_state",state);return message_enter(state,GA(GA_8108C0),true);}
static unsigned long long __fastcall person_notice_enter(Obj state){ COST_SCOPE("advisor_briefing::person_notice_enter"); COST_CRUMB("advisor_briefing_state",state);return message_enter(state,GA(GA_1F16A0),false);}
static void __fastcall simple_frame(Obj state) { COST_SCOPE("advisor_briefing::simple_frame"); COST_CRUMB("advisor_briefing_state",state);
#ifndef NO_RUNTIME_LOG
    hook_called(4);
#endif
    auto it=std::find(simple_states.begin(),simple_states.end(),state);
    if(it!=simple_states.end()) {
        // A completion callback may have stacked a new interaction. Never pop
        // whichever state happens to be on top in place of our own message.
        if(top_state()!=state)return;
        Obj dialog=pointer(state,0x470);
        bool diplomatic=pointer(state,0)==image+GA(GA_145F9E8);
        if(!diplomatic)return; // General message states are always native.
        if(!dialogue_ready(false)) {
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
            fn<void(__fastcall *)(Obj)>(GA(GA_863C70))(dialog);return;
        }
        simple_states.erase(it);simple_state=nullptr;
        // This can synchronously deliver the close callback. No dialog/state
        // access is allowed after the native acknowledgement returns.
        fn<void(__fastcall *)(Obj,unsigned)>(GA(GA_861480))(dialog,0x7FFFFFFD);
        return;
    }
    // Original slot is 92AD0 (void no-op). Original close event and Leave
    // 4F7170 own disposal and +48; YesNo/choices/input use other slots.
}
static bool record(Obj r,std::vector<Event> &batch) {
    Event e;e.speaker=word(r,0x88);e.day=day();
    if(!copy(reinterpret_cast<const wchar_t *>(r+0x8A),e.text,0x55))return false;
    if(e.text.empty() && !copy(reinterpret_cast<const wchar_t *>(r+0x14),e.text,0x37))return false;
    if(!e.text.empty())batch.push_back(std::move(e));return true;
}
static bool pending_world_reports(std::vector<Event> &batch) {
    Obj root=world();Obj context=root?pointer(root,0x871E8):nullptr;if(!context)return false;
    Obj district=fn<Obj(__fastcall *)(Obj)>(GA(GA_32E8D0))(context);
    unsigned district_id=district?fn<unsigned(__fastcall *)(Obj)>(GA(GA_22DBC0))(district):0;
    for(unsigned i=0;i<2001;++i) {
        Obj r=pointer(root,0x79E90+i*8);if(!r)continue;
        unsigned flags=r[0x82],owner_id=r[0x12];if((flags&4) || !(flags&2))continue;
        Obj d=owner_id>0 && owner_id<52?pointer(root,0xE0D0+owner_id*8):nullptr;
        bool known=(flags&1) || (d && fn<int(__fastcall *)(Obj)>(GA(GA_233750))(d));
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
static bool capture_event(Obj owner,Obj person,const wchar_t *text) {
    if(!entry_ready(3))return false;
    Obj table=owner?pointer(owner,0):nullptr;
    bool allowed=false;for(unsigned rva:event_owners)if(table==image+rva){allowed=true;break;}
    if(!message_gate(3,message_owner,allowed))return false;
    Event e;e.day=day();
    if(!message_gate(3,message_bad_text,copy(text,e.text) && !e.text.empty()))return false;
    if(table==image+GA(GA_1440FF8)) {
        unsigned phase=dword(owner,0x4C8);
        if(!message_gate(3,message_phase,phase>=1 && phase<=3 && person && person==pointer(owner,phase==1?0x4B0:0x4B8)))return false;
        e.kind=Kind::Recruitment;std::wstring target,envoy;
        Obj who=pointer(owner,0x4B8),from=pointer(owner,0x4B0);
        if(who)copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_22EEC0))(who),target,100);
        if(from)copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_22EEC0))(from),envoy,100);
        if(phase==1)e.report=envoy.empty() || target.empty()?L"등용 임무의 결과를 보고드립니다.":envoy+L"의 "+target+L" 등용 임무를 보고드립니다.";
        else e.report=(target.empty()?L"해당 인재":target)+L"의 등용에 "+(phase==2?L"성공했습니다.":L"실패했습니다.");
    }
    name_speaker(e,person);
    return message_gate(3,message_store_failed,store({e}));
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
    if(capturing && capturing->valid) {
        std::wstring value;
        if(!copy(text,value) || capturing->text.size()>=2048)capturing->valid=false;
        else if(!value.empty() || root==capturing->headless_root) {
            auto &v=capturing->text;auto it=std::find_if(v.begin(),v.end(),[&](const Text &t){return t.root==root && t.id==id;});
            if(it!=v.end())it->value=std::move(value);
            else {int pos[2]{0,id};if(root!=capturing->headless_root)fn<void(__fastcall *)(Obj,int *,int)>(GA(GA_92CB10))(root,pos,id);v.push_back({root,id,pos[0],pos[1],std::move(value)});}
        }
    }
    if(!capturing || root!=capturing->headless_root)original_text(root,id,text,flags);
}
static void __fastcall diplomatic_speech(Obj owner,int slot,Obj person,const wchar_t *text,Obj extra) { COST_SCOPE("diplomatic_speech");
    bool capture=dialogue_ready(true) && owner==diplomatic_parent(true) && (slot==3 || slot==4);
    Event e;e.kind=Kind::Diplomacy;e.day=day();
    if(capture) {
        capture=copy(text,e.text) && !e.text.empty();
        if(capture)e.report=diplomatic_report(e.text,report_facts(owner,person));
        if(capture && person) {
            e.speaker=word(person,0x10);std::wstring name;
            if(copy(fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_22EEC0))(person),name,100) && !name.empty())e.text=name+L"\n"+e.text;
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
    if(advance) {
        // CInputWaitState's native timer is at +470, threshold +8. A zero
        // timeout requests the original auto-confirm path; its animation,
        // input-release gates, pop and bound coroutine callback still run.
        *reinterpret_cast<unsigned *>(state+0x478)=0;
#ifndef NO_RUNTIME_LOG
        InterlockedIncrement64(&wait_zero_writes);
#endif
    }
    fn<void(__fastcall *)(Obj)>(GA(GA_83BE30))(state);
#ifndef NO_RUNTIME_LOG
    InterlockedIncrement64(&wait_native_returns);
    if(top_state()!=state)InterlockedIncrement64(&wait_top_changed);
#endif
    if(advance && serial==speech_serial && top_state()!=state)speech_waiting=false;
}
// Only the 676870 -> 67AE80 call is redirected. A native UI fallback replays
// the prepared scalar/string payload, rather than applying report results twice.
static void __fastcall report_results(Obj action) { COST_SCOPE("report_results");
    if(!replay_results){fn<void(__fastcall *)(Obj)>(GA(GA_67AE80))(action);return;}
    for(unsigned i=0;i<262;++i) {
        unsigned offset=i<261?0x1B00+i*0x140:0x16140;
        memcpy(action+offset+0x10,replay_results+offset+0x10,0x130);
    }
    memcpy(action+0x16280,replay_results+0x16280,8);
}
struct ReportRecords {
    std::vector<unsigned char> bytes;
    ReportRecords():bytes(0x16290) {
        for(unsigned i=0;i<262;++i)fn<Obj(__fastcall *)(Obj)>(GA(GA_2213C0))(bytes.data()+(i<261?0x1B00+i*0x140:0x16140));
    }
    ~ReportRecords(){for(unsigned i=262;i;) {--i;fn<void(__fastcall *)(Obj)>(GA(GA_2220A0))(bytes.data()+(i<261?0x1B00+i*0x140:0x16140));}}
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
static bool report_districts(std::vector<Obj> &districts) {
    Obj root=world(),context=pointer(root,0x871E8);
    Obj force=fn<Obj(__fastcall *)(Obj)>(GA(GA_32E910))(context);
    if(!force)return false;
    for(unsigned i=0;i<52;++i) {
        Obj district=pointer(root,0xE0D0+i*8);
        if(!district || !fn<int(__fastcall *)(Obj)>(GA(GA_32F500))(district))continue;
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
static bool headless_income(std::vector<Event> &batch,const std::vector<Obj> &districts) {
    // 67D1F0 uses only +378 of its UI owner. The zero-node layout makes its
    // optional 92EF10 visibility update a no-op (92CEE0's count guard).
    // Text setters are captured without creating nodes, querying coordinates,
    // updating textures or forwarding to the real UI setter.
    alignas(8) unsigned char income[0x380]{},child[0x178]{};
    *reinterpret_cast<Obj *>(income+0x378)=child;
    for(Obj district:districts) {
        TextCapture values;values.headless_root=child+0x130;
        {CaptureScope scope(values);fn<void(__fastcall *)(Obj,Obj)>(GA(GA_67D1F0))(income,district);}
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
        if(!copy(fn<const wchar_t *(__fastcall *)(unsigned)>(GA(GA_30CF80))(district_labels[district[0x11]]),e.text,100) || e.text.empty())return false;
        e.report=income_summary(e.text,cells);
        // Preserve the original formatted cell strings in their row order for
        // detail view, without exposing internal control IDs in the report.
        std::stable_sort(cells.begin(),cells.end(),[](const auto &a,const auto &b){return a.first<b.first;});
        for(const auto &c:cells)e.text+=L"\n"+c.second;
        if(e.text.size()>Inbox::text_limit || e.report.size()>Inbox::text_limit)return false;
        batch.push_back(std::move(e));
    }
    return true;
}
static bool headless_gate(unsigned reason) {
#ifndef NO_RUNTIME_LOG
    log_event("advisor_report_headless_gate",reason,inbox.generation);
#endif
    (void)reason;return false;
}
static bool try_headless_report(Obj state,unsigned long long &result) {
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
    fn<void(__fastcall *)(Obj)>(GA(GA_67AE80))(action); // original result/flag writers, exactly once
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
        result=fn<unsigned long long(__fastcall *)(Obj)>(GA(GA_67B420))(state);
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
    auto result=fn<unsigned long long(__fastcall *)(Obj)>(GA(GA_67B420))(state);
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
        fn<void(__fastcall *)(Obj)>(GA(GA_67B600))(state);return;
    }
    if(state==report_state && report_deferred) {
        if(top_state()!=state)return;
        if(!defer(true)){report_deferred=false;fallback();fn<void(__fastcall *)(Obj)>(GA(GA_67B600))(state);return;}
        report_deferred=false;
        TextCapture capture;
        // Preserve creation and disposal of the original income controls too.
        {CaptureScope scope(capture);fn<void(__fastcall *)(Obj,int)>(GA(GA_67C990))(state,1);}
        Obj income=pointer(state,0x490);std::vector<Event> batch;
        // Income values are filled by the district-selection callback, after
        // the constructor has created the headings. Run that same display-only
        // formatter for every district actually listed by the original UI.
        bool ok=income && capture.valid;
        for(unsigned i=0;ok && i<8;++i) {
            Obj district=pointer(income,0x288+i*8);if(!district)continue;
            if(district[0x11]>=sizeof(district_labels)/sizeof(*district_labels)){ok=false;break;}
            TextCapture values=capture;
            {CaptureScope scope(values);fn<void(__fastcall *)(Obj,Obj)>(GA(GA_67D1F0))(income,district);}
            Obj root=pointer(income,0x378);if(!root){ok=false;break;}
            values.text.erase(std::remove_if(values.text.begin(),values.text.end(),[&](const Text &t){return t.root!=root+0x130;}),values.text.end());
            bool number=false;for(const auto &t:values.text)for(wchar_t c:t.value)if(c>=L'0' && c<=L'9')number=true;
            if(!values.valid || !number){ok=false;break;}
            std::stable_sort(values.text.begin(),values.text.end(),[](const Text &a,const Text &b){return a.y!=b.y?a.y<b.y:a.x<b.x;});
            Event e;e.kind=Kind::Income;e.day=day();
            if(!copy(fn<const wchar_t *(__fastcall *)(unsigned)>(GA(GA_30CF80))(district_labels[district[0x11]]),e.text,100)){ok=false;break;}
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
        fn<void(__fastcall *)(Obj,int)>(GA(GA_67C990))(state,2); // original camera restoration
        fn<void(__fastcall *)(Obj,int)>(GA(GA_67C990))(state,3); // original report completion phase
        *reinterpret_cast<unsigned *>(state+0x474)=3; // no visible bg_out delay
        // Native 67B600 phase 3 applies its own final audio/camera cleanup and
        // manager pop. Leave 67B1E0 still disposes all controls and calls +48.
    }
    fn<void(__fastcall *)(Obj)>(GA(GA_67B600))(state);
}
static void __fastcall report_leave(Obj state,Obj context) { COST_SCOPE("advisor_briefing::report_leave"); COST_CRUMB("advisor_briefing_state",state);
#ifndef NO_RUNTIME_LOG
    ReportTiming timing(3);
#endif
    bool owned=state==report_state;
    if(owned){report_state=nullptr;report_deferred=false;report_headless=false;}
    fn<void(__fastcall *)(Obj,Obj)>(GA(GA_67B1E0))(state,context);
    if(owned && !report_state)briefing_overlay::mask_report(false);
}
static void __fastcall report_wait(Obj state) { COST_SCOPE("advisor_briefing::report_wait"); COST_CRUMB("advisor_briefing_state",state);if(defer(true) && top_state()==state){*reinterpret_cast<unsigned *>(state+0x470)=0;pop();return;}fn<void(__fastcall *)(Obj)>(GA(GA_67B7C0))(state);}
#ifndef NO_RUNTIME_LOG
static void diagnose_uncollected_top() {
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
#endif
static void __fastcall log_frame(Obj layout) { COST_SCOPE("advisor_briefing::log_frame"); COST_CRUMB("advisor_briefing_layout",layout);
#ifndef NO_RUNTIME_LOG
    FrameTiming timing(1);
#endif
#ifndef NO_RUNTIME_LOG
    hook_called(1);
#endif
    bind(old_log,layout);fn<void(__fastcall *)(Obj)>(GA(GA_66F7C0))(layout);move(old_log,hide_native_hud() && replace_legacy());
    // Saved player-turn and YesNo snapshots both show this existing hook running.
    // Only read native state here; Present/WndProc consume the published decision.
    if(active() && !inbox.collecting){
        Obj top=top_state();bool player=top && pointer(top,0)==image+GA(GA_13D63A0) && !interaction();
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
#endif
}
static void __fastcall log_leave(Obj layout,Obj context) { COST_SCOPE("advisor_briefing::log_leave"); COST_CRUMB("advisor_briefing_layout",layout);
    if(old_log.object==layout){move(old_log,false);old_log={};}
    fn<void(__fastcall *)(Obj,Obj)>(GA(GA_66F3E0))(layout,context);
}
static void __fastcall advice_frame(Obj layout) { COST_SCOPE("advisor_briefing::advice_frame"); COST_CRUMB("advisor_briefing_layout",layout);
#ifndef NO_RUNTIME_LOG
    FrameTiming timing(2);
#endif
#ifndef NO_RUNTIME_LOG
    hook_called(2);
#endif
    bind(old_advice,layout);fn<void(__fastcall *)(Obj)>(GA(GA_88C1C0))(layout);
    if(!active()){move(old_advice,false);return;}
    Obj adviser=pointer(layout,0x448);unsigned art=adviser?word(adviser,0xA6):~0u;
    if(adviser!=last_adviser || art!=last_art){last_adviser=adviser;last_art=art;++portrait_serial;}
    bool needs_portrait=replace_legacy() && adviser && !briefing_overlay::captured(portrait_serial);
    move(old_advice,hide_native_hud() && replace_legacy() && !needs_portrait);
    if(needs_portrait && fn<unsigned(__fastcall *)(Obj)>(GA(GA_86F250))(layout)) {
        int pos[2]{},size[2]{};fn<void(__fastcall *)(Obj,int *,int)>(GA(GA_92CB10))(layout+0x130,pos,5);fn<void(__fastcall *)(Obj,int *,int)>(GA(GA_92CF90))(layout+0x130,size,5);
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
    fn<void(__fastcall *)(Obj,Obj)>(GA(GA_893720))(layout,context);
}
static void *replacements[]={reinterpret_cast<void *>(&user_enter),reinterpret_cast<void *>(&user_leave),reinterpret_cast<void *>(&game_leave),reinterpret_cast<void *>(&narration),reinterpret_cast<void *>(&simple_enter),reinterpret_cast<void *>(&simple_frame),reinterpret_cast<void *>(&report_enter),reinterpret_cast<void *>(&report_frame),reinterpret_cast<void *>(&report_leave),reinterpret_cast<void *>(&report_wait),reinterpret_cast<void *>(&log_frame),reinterpret_cast<void *>(&log_leave),reinterpret_cast<void *>(&advice_frame),reinterpret_cast<void *>(&advice_leave),reinterpret_cast<void *>(&diplomatic_enter),reinterpret_cast<void *>(&simple_frame),reinterpret_cast<void *>(&diplomatic_wait),reinterpret_cast<void *>(&user_frame),reinterpret_cast<void *>(&person_notice_enter),reinterpret_cast<void *>(&simple_frame)};
static bool install() {
    if(!guards_match())return false;
    Obj relay=allocate_near(image+GA(GA_930D20));if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&set_text));memcpy(relay+32,image+GA(GA_930D20),5);absolute_jump(relay+37,image+GA(GA_930D25));
    absolute_jump(relay+128,reinterpret_cast<void *>(&diplomatic_speech));memcpy(relay+160,image+GA(GA_841660),5);absolute_jump(relay+165,image+GA(GA_841665));
    bool messages_prepared=prepare_message_link(message_links[0],GA(GA_1F0AD0),message_prologue,sizeof(message_prologue),relay+256,reinterpret_cast<void *>(&message_call)) &&
        prepare_message_link(message_links[1],GA(GA_1F09E0),narration_prologue,sizeof(narration_prologue),relay+320,reinterpret_cast<void *>(&narration_call)) &&
        prepare_message_link(message_links[2],GA(GA_1F0DD0),person_prologue,sizeof(person_prologue),relay+384,reinterpret_cast<void *>(&person_call)) &&
        prepare_message_link(event_link,GA(GA_842380),event_prologue,sizeof(event_prologue),relay+448,reinterpret_cast<void *>(&event_speech)) &&
        (scene_yield || prepare_message_link(scene_links[0],GA(GA_6C43B0),scene_prologue,sizeof(scene_prologue),relay+512,reinterpret_cast<void *>(&employ_scene))) &&
        (scene_yield || prepare_message_link(scene_links[1],GA(GA_6C4670),scene_prologue,sizeof(scene_prologue),relay+576,reinterpret_cast<void *>(&discovery_scene))) &&
        prepare_message_link(scene_links[2],GA(GA_41D550),scenery_prologue,sizeof(scenery_prologue),relay+640,reinterpret_cast<void *>(&scenery_scene));
    if(!scene_yield){original_employ_scene=reinterpret_cast<SceneCall>(relay+544);original_discovery_scene=reinterpret_cast<SceneCall>(relay+608);}
    original_scenery_scene=reinterpret_cast<SceneryCall>(relay+672);
    original_message_call=reinterpret_cast<MessageCall>(relay+288);
    original_narration_call=reinterpret_cast<NarrationCall>(relay+352);
    original_person_call=reinterpret_cast<NarrationCall>(relay+416);
    original_event_speech=reinterpret_cast<EventSpeech>(relay+480);
    original_speech=reinterpret_cast<SpeechSetter>(relay+160);
    original_text=reinterpret_cast<TextSetter>(relay+32);DWORD old;
    bool prepared=messages_prepared &&
        prepare_picture_link(picture_links[0],GA(GA_451FF0),picture_show_bytes,sizeof(picture_show_bytes),relay+704,reinterpret_cast<void *>(&picture_show)) &&
        prepare_picture_link(picture_links[1],GA(GA_43FF90),picture_close_bytes,sizeof(picture_close_bytes),relay+736,reinterpret_cast<void *>(&picture_close)) &&
        prepare_message_link(time_wait_link,GA(GA_304640),narration_prologue,sizeof(narration_prologue),relay+768,reinterpret_cast<void *>(&time_wait)) &&
        prepare_link(text_link,GA(GA_930D20),0xE9,relay) && prepare_link(speech_link,GA(GA_841660),0xE9,relay+128);
    original_time_wait=reinterpret_cast<TimeWait>(relay+800);
    prepare_scene_calls(relay);
    absolute_jump(relay+1024,reinterpret_cast<void *>(&report_results));
    prepared=prepared && call_prepare(report_results_call,GA(GA_676D0D),relay+1024);
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
