// [officer_ui] tactic_priority v2 (docs/TACTIC_PRIORITY_V2_PLAN.md). Convenience DLL.
// Sortie setup screen = CStrategyUnitMakeState (layout 06_01_01_unit_setup): footer button 「전법 우선순위」 (footer node 24) opens the
// grid popup (native_tactic_priority_grid.inl); a per-officer 「출진 기억」 toggle sits right of the mode tabs. Per-frame choice of 3 tactics:
// officers with the remember flag restore their last sortie formation + EXACTLY the remembered tactics (empty slots stay empty), the others
// get the usable tactics of the player's ranked list for their type, in ranked order, at most 3, unranked slots empty (empty ranked list or
// nothing usable = the original choice is kept). Recording at departure keeps the flag.
// Original (docs/TACTIC_PRIORITY_V2_PLAN.md 12): sortie records = pointer array state+0x4C0..0x4C8 (edited while composing; +0x540 is another list,
// only valid at departure), 0x94 bytes each and the record IS the departure command struct (no 4-byte header; local15 read rec+4 and found nothing):
// rec[0] person, [2] troops, [3] formation, [5..7] tactics (7C71E0, 7F0670, 1EDC30). state+0x470 main layout, +0x488 tab group (+0x148 = current record
// pointer, 7F0670), +0x4E4 selected index, +0x490/+0x498 pages (+0x490 page+0x178 = live edit buffer, copied into the record by 7EA2D0),
// +0x4A0 footer (CCmnFooter, text buttons via 85FEF0, built in enter 7DD7C0), +0x4B0 persistent confirm dialog (not a modal flag).
// All UI objects belong to SAN14_Layout.dll (layout_client::ui_*); this module keeps behaviour, labels and the apply loop.
#include "game_addresses.h"
#include "tactic_priority_profile.h"
#include "tactic_priority_io.h"
#include "trait_hover_profile.h"   // pure tactic text tables shared with trait_hover (mouse-over text of the grid tiles)
#include <map>
namespace tactic_priority {
static SRWLOCK settings_lock=SRWLOCK_INIT;
static tp::Settings settings;
static unsigned generation=1;
using SetText=void(__fastcall *)(Obj,int,const wchar_t *,int);
using Update=void(__fastcall *)(Obj);
using Leave=uint64_t(__fastcall *)(Obj,Obj);
using Dispatch=Obj(__fastcall *)(unsigned *,int);
static SetText next_set_text;   // Layout manager: next in chain for CALL 7DB950+ (85FEF0)
static Update next_update;
static Leave next_leave;
static Dispatch next_dispatch;  // 1EC890
// Addresses are written as GA(GA_x) at each use: resolved at call time, after the build table is selected (external review F-01).
static const int kButton=0x18;
static std::vector<int> catalog() {   // known tactic ids of the running world (empty before a world exists)
    std::vector<int> ids;
    for(unsigned t=1;t<=200;++t)if(valid(table(0x774D0,t,200)))ids.push_back(static_cast<int>(t));
    return ids;
}
static void publish(const tp::Settings &next) {
    AcquireSRWLockExclusive(&settings_lock);settings=next;++generation;ReleaseSRWLockExclusive(&settings_lock);
}
static void load_settings() {
    tp::Settings loaded;const auto ids=catalog();
    wchar_t path[MAX_PATH];std::wstring text;
    if(mod_ini_path(path,L"기능설정.ini"))text=tactic_priority_io::read_profile(path);
    std::string ascii;for(wchar_t c:text)ascii+=c<0x80?static_cast<char>(c):'?';
    int version=0;bool restore_v1=false;
    if(!tp::decode(ascii,ids,loaded,&version,&restore_v1)) {
        if(!text.empty())log_event("tactic_priority_profile_invalid");
        loaded=tp::Settings();   // empty ranked lists = default order
    }
    else if(version==1)log_event("tactic_priority_profile_migrated",1,restore_v1?1u:0u);   // v1 full lists were converted, the restore switch is dropped; the file is rewritten only when the player saves
    publish(loaded);
}
// Per sortie record state (keyed by the record pointer): what was seen last frame, our own pending write, manual change flag.
struct Seen {tp::Memo memo;unsigned wrote;unsigned written[3];unsigned stamp;bool manual,player;};
static std::map<uintptr_t,Seen> seen;
static Obj active_state,active_world;
static tp::Settings cached;static std::vector<int> ranked_martial,ranked_intellect;   // cached ranked ids that are visible tiles
static unsigned cached_generation,frame_stamp;
static void reset() {seen.clear();active_state=active_world=nullptr;}
static void forget(Obj record) {seen.erase(reinterpret_cast<uintptr_t>(record));}
// Records of the screen being composed: state+0x4C0..0x4C8 (8-byte record pointers). Returns false when the list looks invalid.
static bool record_list(Obj state,uintptr_t &first,uintptr_t &last) {
    first=reinterpret_cast<uintptr_t>(pointer(state,0x4C0));last=reinterpret_cast<uintptr_t>(pointer(state,0x4C8));
    return first && last>=first && !((last-first)%8) && (last-first)/8<=64;
}
static unsigned record_person(Obj record) {return record?dword(record,0):0;}
// Selected unit: the list entry at +0x4E4 (the unit list control's selection, 7F04D0), else the tab group's +0x148 when it is in the list.
static Obj current_record(Obj state) {
    uintptr_t first=0,last=0;
    if(!record_list(state,first,last) || first==last)return nullptr;
    const unsigned count=static_cast<unsigned>((last-first)/8),index=dword(state,0x4E4);
    if(index<count)return *reinterpret_cast<Obj *>(first+index*8ull);
    Obj tabs=pointer(state,0x488),at=tabs?pointer(tabs,0x148):nullptr;
    for(uintptr_t i=first;at && i<last;i+=8)if(*reinterpret_cast<Obj *>(i)==at)return at;
    return nullptr;
}
// Live edit buffer of the formation page (+0x490 page+0x178, 0x94 bytes): the original copies it over the record on its own edits (7EA2D0).
static unsigned *edit_buffer(Obj state,unsigned person) {
    Obj page=pointer(state,0x490);
    if(!page)return nullptr;
    unsigned *buffer=reinterpret_cast<unsigned *>(page+0x178);
    return buffer[0]==person?buffer:nullptr;
}
// 857EB0 re-selects the current tab so the page shows what was written into the record directly (kind: 1 apply, 2 popup closed).
static void refresh_tabs(Obj state,unsigned kind) {
    Obj tabs=pointer(state,0x488),a=tabs?pointer(tabs,0x130):nullptr,b=a?pointer(a,0x138):nullptr;
    if(!b)return;
    const unsigned tab=dword(b,0x18);
    fn<void(__fastcall *)(Obj,unsigned)>(GA(GA_857EB0))(tabs,tab);
    log_event("tactic_priority_refresh",kind,tab,0);
}
static std::wstring tactic_name(int id) {
    if(id==tp::kUnique)return L"고유전법";
    std::wstring name;
    Obj entry=table(0x774D0,static_cast<unsigned>(id),200);
    if(valid(entry)) {
        const wchar_t *text=reinterpret_cast<const wchar_t *>(entry+0x10);   // inline name, same field trait_hover compares
        size_t n=0;while(n<40 && text[n])++n;
        name.assign(text,n);
    }
    if(name.empty())name=L"전법 #"+std::to_wstring(id);
    return name;
}
// Grid tiles: default list sorted into the fixed groups; siege and naval tactics are not tiles and never ranked.
static std::vector<tp::Tile> tiles_for(bool martial,const std::vector<int> &ids) {
    return tp::build_tiles(tp::default_list(martial,ids),[](int id) {return tactic_name(id);});
}
static std::vector<int> visible_ranked(const std::vector<int> &ranked,bool martial,const std::vector<int> &ids) {
    const std::vector<int> tiles=tp::tile_ids(tiles_for(martial,ids));
    std::vector<int> out;
    for(int id:ranked)if(tp::listed(tiles,id))out.push_back(id);
    return out;
}
#include "native_tactic_priority_grid.inl"
static void __cdecl clicked(void *,void *) {grid::open_requested=true;log_event("tactic_priority_click");grid::probe_click();}
static void add_button(Obj bar,int index,int flag) {
    // Same sequence as lord_change::add_button (85FEF0/85FCE0 for a text button), node 24 of cmn_footer_re_cn; the Layout DLL builds it
    // (clone_control = 92E430) only while this feature is published.
    if(!bar || pointer(bar,0)!=image+GA(GA_146E078))return;
    if(kButton>=static_cast<int>(dword(bar+0x130,0x10))) {log_event("tactic_priority_no_layout_node",dword(bar+0x130,0x10));return;}
    layout_client::UiScope scope("tactic_priority");
    if(!scope.ok) {log_event("tactic_priority_no_ui",1);return;}
    if(layout_client::ui_node(bar,kButton))return;
    // Same style and kind as the neighbouring text buttons 862010 builds (style 2, kind 2 when the 85FEF0 flag is set, else 0).
    const int kind=flag?2:0;
    Obj button=static_cast<Obj>(layout_client::ui_button(bar,kButton,2,kind));
    if(!button)return;
    log_event("tactic_priority_footer_style",static_cast<unsigned>(index),static_cast<unsigned>(flag),static_cast<unsigned>(kind));
    layout_client::ui_text(button,L"전법 우선순위");
    layout_client::ui_click(button,&clicked,nullptr);
    layout_client::ui_node_visible(button,0x11,0);
    layout_client::ui_node_visible(bar,0x15,1);
    log_event("tactic_priority_button_added");
}
static void __fastcall set_text(Obj bar,int index,const wchar_t *text,int flag) { COST_SCOPE("tactic_priority::set_text"); COST_CRUMB("tactic_priority_bar",bar);   // only CALL 7DC20D (last text button of enter)
    next_pick(next_set_text,GA(GA_862220))(bar,index,text,flag);   // the button must exist before enter ends (later ones stay hidden)
    if(mod_features.tactic_priority)add_button(bar,index,flag);
}
static bool formation_usable(Obj person,unsigned f) {
    Obj form=table(0x77428,f,20);if(!valid(form))return false;
    return form[0x7E]==2?fn<unsigned(__fastcall *)(Obj)>(GA(GA_43660))(person)==f:fn<int(__fastcall *)(Obj,unsigned)>(GA(GA_233CF0))(person,f)!=0;
}
static std::vector<unsigned> eligible_tactics(Obj person,unsigned formation) {
    std::vector<unsigned> out;
    for(unsigned i=0;i<tp::kSlots;++i) {
        const unsigned t=fn<unsigned(__fastcall *)(Obj,unsigned)>(GA(GA_22BD60))(person,i);
        Obj entry=t&&t<=200?table(0x774D0,t,200):nullptr;
        // Same formation compatibility bit as the native command builder.
        if(valid(entry) && (dword(entry,0x6C)&(1u<<formation)) && !tp::contains(out,t))out.push_back(t);
    }
    return out;
}
#ifndef NO_RUNTIME_LOG
static Obj catalog_world;
static void log_catalog() {   // once per world: every known tactic id, its guessed kind and how many officers hold it
    unsigned holders[201]={};
    for(unsigned id=1;id<6000;++id) {
        Obj p=table(0x158,id,6000);if(!valid(p))continue;
        for(unsigned i=0;i<tp::kSlots;++i){unsigned t=fn<unsigned(__fastcall *)(Obj,unsigned)>(GA(GA_22BD60))(p,i);if(t && t<=200)++holders[t];}
    }
    for(unsigned t=1;t<=200;++t)if(valid(table(0x774D0,t,200)))log_event("tactic_priority_catalog",t,static_cast<unsigned>(tp::kind_of(static_cast<int>(t))),holders[t]);
}
#endif
static void apply(Obj state) {
    Obj root=world();if(!root || !mod_features.tactic_priority){if(active_state)reset();return;}
    uintptr_t first=0,last=0;
    if(!record_list(state,first,last))return;
    if(active_state!=state || active_world!=root) {
        reset();active_state=state;active_world=root;cached_generation=0;
        log_event("tactic_priority_state",static_cast<unsigned>((last-first)/8),dword(state,0x538));
#ifndef NO_RUNTIME_LOG
        if(diagnostic_logging && catalog_world!=root){catalog_world=root;log_catalog();}
#endif
    }
    unsigned revision;bool changed=false;
    const bool had_settings=cached_generation!=0;
    AcquireSRWLockShared(&settings_lock);revision=generation;
    if(revision!=cached_generation){cached=settings;changed=true;}
    ReleaseSRWLockShared(&settings_lock);
    if(changed) {
        const auto ids=catalog();
        ranked_martial=visible_ranked(cached.martial,true,ids);
        ranked_intellect=visible_ranked(cached.intellect,false,ids);
        cached_generation=revision;
    }
    const bool live=changed && had_settings;   // a ranking edit (popup click) reached a screen that is already composed
    unsigned live_changed=0,live_skipped=0;
    Obj current=current_record(state);bool refresh=false;
    Obj player=nullptr;bool player_read=false;
    ++frame_stamp;
    for(uintptr_t at=first;at<last;at+=8) {
        Obj row=*reinterpret_cast<Obj *>(at);if(!row)continue;
        unsigned *cmd=reinterpret_cast<unsigned *>(row);   // the record is the command struct itself
        const unsigned id=cmd[0];unsigned formation=cmd[3];
        if(!id || id>=6000 || formation>tp::kMaxFormation)continue;
        unsigned tactics[3]={cmd[5],cmd[6],cmd[7]};
        auto it=seen.find(reinterpret_cast<uintptr_t>(row));
        if(it!=seen.end()) {   // fast path: nothing changed for this record, only integer compares
            Seen &s=it->second;s.stamp=frame_stamp;
            if(s.memo.person==id && s.memo.formation==formation && (s.memo.generation==revision || s.manual || !s.player)) {
                if(s.wrote) {   // our own write must survive the original update
                    --s.wrote;
                    if(!std::equal(tactics,tactics+3,s.written)) {log_event("tactic_priority_overwritten",id);s.wrote=0;std::copy(tactics,tactics+3,s.memo.tactics);}
                }
                else {
                    // The player edits the page buffer (page+0x178) first; the record only follows when the original copies it (7EA2D0).
                    if(row==current && !s.manual)if(unsigned *buffer=edit_buffer(state,id))
                        if(!std::equal(tactics,tactics+3,buffer+5)) {s.manual=true;log_event("tactic_priority_manual",id,buffer[5]|(buffer[6]<<8)|(buffer[7]<<16));}
                    if(!std::equal(tactics,tactics+3,s.memo.tactics)) {s.manual=true;std::copy(tactics,tactics+3,s.memo.tactics);}   // the player picked tactics by hand
                }
                s.memo.generation=revision;
                continue;
            }
        }
        const tp::Event event=tp::classify(it!=seen.end()?&it->second.memo:nullptr,id,formation,revision);
        Seen &s=seen[reinterpret_cast<uintptr_t>(row)];
        if(it==seen.end() || event==tp::Event::Person) {s=Seen();s.stamp=frame_stamp;}
        s.stamp=frame_stamp;
        if(event==tp::Event::Formation)s.manual=false;
        if(event==tp::Event::Person) {
            log_event("tactic_priority_record",static_cast<unsigned>((at-first)/8),id,formation);
            log_event("tactic_priority_row_fields",id,cmd[2],(cmd[5]&0xFF)|((cmd[6]&0xFF)<<8)|((cmd[7]&0xFF)<<16));   // first frame this row is seen: person, troops, tactics
        }
        auto remember_view=[&]{s.memo.person=id;s.memo.formation=formation;s.memo.generation=revision;std::copy(tactics,tactics+3,s.memo.tactics);};
        if(!player_read) {
            Obj options=pointer(root,0x871E8);player=options?fn<Obj(__fastcall *)(Obj)>(GA(GA_330B90))(options):nullptr;player_read=true;
        }
        Obj person=table(0x158,id,6000);
        bool restored=false;
        s.player=valid(player) && valid(person) && fn<Obj(__fastcall *)(Obj)>(GA(GA_22E450))(person)==player;
        if(!s.player) {remember_view();continue;}
        const bool flag=tactic_memory_flag(id);
        tactic_memory::Record rec;const bool have=tactic_memory_lookup(id,rec);
        bool use_memory=flag && have;
        if(event==tp::Event::Settings && (flag || s.manual)) {if(flag)++live_skipped;remember_view();continue;}   // generation changes only recompute units that follow the shared rule
        if(use_memory && event==tp::Event::Person) {
            const unsigned want=rec.formation;
            if(want>tp::kMaxFormation || !formation_usable(person,want))log_event("tactic_priority_restore_skipped",id,2);
            else if(want>=8 && want<=10 && cmd[2]>8000)log_event("tactic_priority_restore_skipped",id,3);   // siege formation, troop cap
            else if(want!=formation) {
                cmd[3]=want;formation=want;
                fn<void(__fastcall *)(Obj,Obj,unsigned *)>(GA(GA_D1120))(person,table(0x77428,formation,20),cmd);
                restored=true;
                log_event("tactic_priority_restore",id,formation,1);
            }
        }
        else if(flag && !have && event==tp::Event::Person)log_event("tactic_priority_restore_skipped",id,1);
        const std::vector<unsigned> eligible=eligible_tactics(person,formation);
        log_event("tactic_priority_applied",id,formation,static_cast<unsigned>(eligible.size()));
        const bool martial=tp::martial(fn<int(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_231770))(person,1,0xFFF),
                                       fn<int(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_231770))(person,2,0xFFF));
        bool write=false;tp::Three chosen{};
        if(use_memory) {
            if(rec.formation==formation) {
                // Remember = exactly the remembered tactics in their slots (0 = the original "no tactic" value), dropping only unusable ones.
                tp::Three remembered{};for(unsigned i=0;i<3;++i)remembered[i]=rec.tactics[i]==0xFF?0u:rec.tactics[i];
                chosen=tp::select_remembered(remembered,eligible);write=true;
                log_event("tactic_priority_restore",id,formation,2);
            }
            else log_event("tactic_priority_kept",id,4,static_cast<unsigned>(eligible.size()));   // formation could not be restored: the original choice stays
        }
        else {
            const std::vector<int> &ranked=martial?ranked_martial:ranked_intellect;
            if(ranked.empty())log_event("tactic_priority_kept",id,1,static_cast<unsigned>(eligible.size()));   // nothing ranked for this type: original
            else {
                chosen=tp::select(eligible,ranked);
                if(tp::any(chosen))write=true;
                else log_event("tactic_priority_kept",id,2,static_cast<unsigned>(eligible.size()));   // no ranked tactic usable here (siege formation, ship...): original
            }
        }
        if(write) {
            for(unsigned i=0;i<3;++i){cmd[5+i]=chosen[i];tactics[i]=chosen[i];s.written[i]=chosen[i];}
            s.wrote=3;s.manual=false;
            if(row==current)refresh=true;
            if(event==tp::Event::Settings)++live_changed;
            log_event("tactic_priority_chosen",chosen[0],chosen[1],chosen[2]);
        }
        if(row==current && (write || restored)) {   // the page edit buffer must carry the same values or its next copy (7EA2D0) would undo them
            if(unsigned *buffer=edit_buffer(state,id)) {buffer[3]=cmd[3];for(unsigned i=0;i<3;++i)buffer[5+i]=cmd[5+i];refresh=true;}
            else log_event("tactic_priority_buffer_miss",id,pointer(state,0x490)?1u:0u,0);
        }
        remember_view();
    }
    for(auto it=seen.begin();it!=seen.end();)if(it->second.stamp!=frame_stamp)it=seen.erase(it);else ++it;
    if(live)log_event("tactic_priority_live_apply",live_changed,live_skipped,0);
    if(refresh)refresh_tabs(state,1);
}
#ifndef NO_RUNTIME_LOG
static unsigned perf_frames;static unsigned long long perf_total,perf_max;static LONGLONG perf_frequency;
#endif
static void __fastcall update(Obj state) { COST_SCOPE("tactic_priority_update"); COST_CRUMB("tactic_priority_state",state);
    bool skip=false;
    if(mod_features.tactic_priority) {
#ifndef NO_RUNTIME_LOG
        LARGE_INTEGER begin={};if(diagnostic_logging)QueryPerformanceCounter(&begin);
#endif
        apply(state);
        skip=grid::tick(state);
#ifndef NO_RUNTIME_LOG
        if(diagnostic_logging) {
            LARGE_INTEGER end,freq;QueryPerformanceCounter(&end);
            if(!perf_frequency){QueryPerformanceFrequency(&freq);perf_frequency=freq.QuadPart;}
            const unsigned long long ns=static_cast<unsigned long long>(end.QuadPart-begin.QuadPart)*1000000000ull/static_cast<unsigned long long>(perf_frequency);
            perf_total+=ns;if(ns>perf_max)perf_max=ns;
            if(++perf_frames>=1800) {
                log_event("tactic_priority_perf",perf_frames,static_cast<unsigned>(perf_total/perf_frames),static_cast<unsigned>(perf_max));
                perf_frames=0;perf_total=perf_max=0;
            }
        }
#endif
    }
    if(!skip)next_pick(next_update,GA(GA_7E18E0))(state);
}
static uint64_t __fastcall leave(Obj state,Obj next) { COST_SCOPE("tactic_priority_leave");
    grid::leave(state,next);   // Layout releases our panes with the original sequence BEFORE the original leave destroys their parent
    if(active_state==state)reset();
    return next_pick(next_leave,GA(GA_7DC200))(state,next);
}
// Departure (CALL 7EBBC4 -> 1EC890): remember formation and tactics of every officer that really leaves.
static Obj __fastcall depart(unsigned *cmd,int spend) { COST_SCOPE("tactic_priority::depart");
    const unsigned id=cmd?cmd[0]:0,f=cmd?cmd[3]:0,t[3]={cmd?cmd[5]:0,cmd?cmd[6]:0,cmd?cmd[7]:0};
    Obj army=next_pick(next_dispatch,GA(GA_1EDC30))(cmd,spend);
    if(mod_features.tactic_priority && army) {
        tactic_memory_record(id,f,t);
        log_event("tactic_priority_recorded",id,f,(t[0]&0xFF)|((t[1]&0xFF)<<8)|((t[2]&0xFF)<<16));
    }
    return army;
}
static bool guards_match() {
    static const unsigned char bar_before[]={0x41,0x8D,0x51,0x12,0x49,0x8B,0x8E,0xA0,0x04,0x00,0x00};   // lea edx,[r9+12h]; mov rcx,[r14+4A0h]
    static const unsigned char depart_before[]={0x48,0x8B,0x0C,0x30,0x48,0x83,0xC1,0x04};            // mov rcx,[rax+rsi]; add rcx,4
    if(!GA(GA_7DE5BD) || !GA(GA_7EE1C4) || !GA(GA_145B8D8) || !GA(GA_145B8C0)) {log_event("tactic_priority_guard_rejected",3);return false;}
    // CALL targets and the vtable slots are judged by the Layout hook manager (chain / yield / refuse).
    if(ga::mcmp_at(image,GA(GA_7DE5BD)-sizeof(bar_before),bar_before,sizeof(bar_before))) {log_event("tactic_priority_guard_rejected",1);return false;}
    if(ga::mcmp_at(image,GA(GA_7EE1C4)-sizeof(depart_before),depart_before,sizeof(depart_before))) {log_event("tactic_priority_guard_rejected",2);return false;}
    return true;
}
static bool install() {
    load_settings();
    if(!guards_match())return false;
    layout_client::Transaction tx;
    if(!layout_client::call(GA(GA_7DE5BD),reinterpret_cast<void *>(&set_text),reinterpret_cast<void **>(&next_set_text)) ||
       !layout_client::vt(GA(GA_145B8D8),reinterpret_cast<void *>(&update),reinterpret_cast<void **>(&next_update)) ||
       !layout_client::vt(GA(GA_145B8C0),reinterpret_cast<void *>(&leave),reinterpret_cast<void **>(&next_leave)) ||
       !layout_client::call(GA(GA_7EE1C4),reinterpret_cast<void *>(&depart),reinterpret_cast<void **>(&next_dispatch))) {
        log_event("tactic_priority_register_rejected");return false;
    }
    tx.done=true;log_event("tactic_priority_installed");return true;
}
}
