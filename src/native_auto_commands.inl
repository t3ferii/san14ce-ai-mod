// [game_flow] auto-commands (2026-11-09, user request): the two bottom-left toggles.
// State belongs to one save (sidecar .autocmd, end of this file; every new world starts off); the installer switches
// decide whether the module is prepared at all, and a toggle enables its pass. No settings key.
// 자동포상: the player strategy frame (44ED30) calls 17B990 every interactive frame of the player turn
// (never in the turn-start/progress phases). The redirect calls the original first, then one award pass
// per 10-day turn while the toggle is on (local17; local16 keyed it on the game day). Eligible officer: same force as the player's ruler corps,
// p+E6 != 1, loyalty p+E8 <= threshold and < 100, not busy (233BE0 == 0, 234AB0(p,0) == 0).
// The award command is the original one-per-base command (253F0 init, 18C30 ticket slot, 1F3C90 issue,
// 9E00 cleanup); a base is only touched when its corps (vtable+0x80) is the player's corps, and the
// decision table skips gold-short bases and stops when the corps has no command ticket left.
#include "auto_commands_policy.h"
#include <functional>
#include <string>
// PW25b: native_advisor_overlay.inl (briefing_overlay) is included later in this translation unit by
// native_unified_overlay.inl; the same-TU forward declaration keeps the toast call here.
namespace briefing_overlay { static bool available(); static void toast(const std::wstring &); }
// PW36a: native_advisor_briefing.inl (namespace advisor_briefing) is included later in this same translation unit.
namespace advisor_briefing { static bool add_item(const std::wstring &text); }
namespace auto_commands {
static bool search_on_state=false,reward_on_state=false;
static int reward_limit=70,popup_value=70,last_date=-1,search_turn=-1;
bool search_on() {return search_on_state;}
bool reward_on() {return reward_on_state;}
int reward_threshold() {return reward_limit;}
static Obj manager(){return fn<Obj(__fastcall *)()>(GA(GA_11040))();}
static Obj top_state() { COST_SCOPE("auto_commands:top_state");
    Obj m=manager();if(!m)return nullptr;
    const auto count=*reinterpret_cast<const unsigned long long *>(m+0x10);
    Obj states=pointer(m,0x20);return states && count && count<=256?pointer(states,static_cast<unsigned>((count-1)*8)):nullptr;
}
// Absolute game day, same arithmetic as 32BA50(context+34): (year*12+month)*30+day-31 (pure read).
static int game_day_read() { COST_SCOPE("auto_commands:game_day_read");
    Obj root=world();Obj context=root?pointer(root,0x871E8):nullptr;if(!context)return -1;
    Obj d=context+0x34;return (static_cast<int>(*reinterpret_cast<short *>(d))*12+static_cast<signed char>(d[2]))*30+static_cast<signed char>(d[3])-31;
}
#ifndef NO_RUNTIME_LOG
static unsigned log_lines;
static void log_capped(const char *kind,unsigned a=0,unsigned b=0,unsigned c=0) { COST_SCOPE("auto_commands:log_capped");
    if(log_lines>=200)return;
    ++log_lines;log_event(kind,a,b,c);
}
#endif
// PW36b (user 2026-11-09): the main screen 00_02_00_mein_pk is CGameLayout (vtable GA_13D90E0), setup slot 49
// (GA_13D9268) = GA_4452B0, destroy slot 0 = GA_43BE70. The two toggles are built inside the original setup, right
// after the native icon loop's 0x8685C0 call runs with index 0x4C (GA_44616D) or the dmode 0x52 call (GA_445FA6).
// The patched layout has 92 nodes (86-node KTID 0x24E16FBE) or 96 (90-node KTID 0x036BB544); copy roots are 86/89
// and 90/93 (research/auto_commands/build_main_buttons.py).
// PW33 (user 2026-11-09, local9): the previous icon copy drew a filter icon and crashed on ui_text. The Layout DLL
// builds both controls itself (clone_control = 92E430) with the native text-button style (hanyou_defo), owns their
// Korean text and click, and releases them in layout_dtor.
void toggle_search();   // defined below; the click functors call them
void toggle_reward();
static const char kButtonsOwner[]="auto_buttons";
static const unsigned kSetupSlot=GA(GA_13D9268),kDtorSlot=GA(GA_13D90E0),
    kNormalSite=GA(GA_44616D),kDmodeSite=GA(GA_445FA6);
static const unsigned kNodes86[2]={86,89},kNodes90[2]={90,93};   // copy roots from build_main_buttons.py
static const int kButtonStyle=3,kButtonKind=0;   // style 3 = hanyou_defo, kind 0 = general text button
// 4452B0 takes five arguments (the fifth on the stack). Forwarding only four made the original read a stale stack slot
// and crash in 86E920 on load (user 2026-11-09, local11 dump: 445327 -> 8704A1 -> 86E9A4, parent pointer -2).
static unsigned long long(__fastcall *next_layout_setup)(Obj,unsigned long long,unsigned long long,unsigned long long,unsigned long long);
static void(__fastcall *next_layout_dtor)(Obj,unsigned long long,unsigned long long,unsigned long long);
static void(__fastcall *next_control_site)(void *,unsigned,void *,unsigned);
static Obj buttons_layout,button_search,button_reward;   // UI thread only
static Obj building_layout;                              // set only while the original layout setup runs
static bool search_ready,reward_ready;                  // install() results, not settings
// PW33: the Layout DLL owns these controls (make_button), so ui_text is the only text path. The old vt+0x1D8
// fallback wrote a text id into the icon-class button and crashed on the next click; it is gone on purpose.
static void set_button_label(Obj button,const wchar_t *text) { COST_SCOPE("auto_commands:set_button_label");
    if(!button)return;
    if(layout_client::ui_text(button,text)!=0) {
#ifndef NO_RUNTIME_LOG
        static bool rejected_logged;
        if(!rejected_logged) {rejected_logged=true;log_event("auto_buttons_text_rejected",0,0,0);}
#endif
    }
}
static void refresh_button_labels() { COST_SCOPE("auto_commands:refresh_button_labels");
    if(!buttons_layout)return;
    set_button_label(button_search,search_on_state?L"탐색위임 켬":L"탐색위임");
    set_button_label(button_reward,reward_on_state?L"포상위임 켬":L"포상위임");
}
// ui_text needs an open ui scope (layout_api.h); the click and confirm paths run outside the build hook.
static void refresh_button_labels_scoped() { COST_SCOPE("auto_commands:refresh_button_labels_scoped");
    if(!buttons_layout)return;
    layout_client::UiScope scope(kButtonsOwner);
    if(scope.ok)refresh_button_labels();
}
#ifndef NO_RUNTIME_LOG
static unsigned button_flag_word(Obj button) {return button?*reinterpret_cast<unsigned *>(button+0x40):0;}
static unsigned button_flag_pair(Obj button) { COST_SCOPE("auto_commands:button_flag_pair");
    return static_cast<unsigned>(*reinterpret_cast<int *>(button+0x20)) |
           (static_cast<unsigned>(*reinterpret_cast<int *>(button+0x24))<<16);
}
#endif
static void show_toggle_toast(unsigned which,const std::wstring &text) { COST_SCOPE("auto_commands:show_toggle_toast");
    bool shown=false;
    if(mod_features.advisor_briefing && briefing_overlay::available()) {briefing_overlay::toast(text);shown=true;}
#ifndef NO_RUNTIME_LOG
    log_event("auto_toast",which,shown?1u:0u,0);
#else
    (void)which;
#endif
}
static void button_clicked(unsigned which) { COST_SCOPE("auto_commands:button_clicked");
    if(which==0)toggle_search();else toggle_reward();
    refresh_button_labels_scoped();
#ifndef NO_RUNTIME_LOG
    log_event("auto_buttons_click",which,which==0?(search_on_state?1u:0u):(reward_on_state?1u:0u),0);
#endif
}
// PW36b: the click functor runs on the input thread path, so it only queues a pending bit. reward_site drains it on
// the player strategy frame (44ED30 -> 17B990 call site) and runs button_clicked there, where UI work is allowed.
static volatile LONG pending_clicks;
static volatile LONG labels_dirty;   // set by the per-save restore (load host); drained in reward_site
static void __cdecl auto_button_click(void *,void *ctx) { COST_SCOPE("auto_commands:auto_button_click");
    const unsigned which=static_cast<unsigned>(reinterpret_cast<uintptr_t>(ctx));
    if(which<2)InterlockedOr(&pending_clicks,1L<<which);
}
static Obj make_button(Obj layout,unsigned node,unsigned which) { COST_SCOPE("auto_commands:make_button");
    Obj button=static_cast<Obj>(layout_client::ui_button(layout,static_cast<int>(node),kButtonStyle,kButtonKind));
    if(!button)return nullptr;
    set_button_label(button,which==0?L"탐색위임":L"포상위임");
    layout_client::ui_click(button,&auto_button_click,reinterpret_cast<void *>(static_cast<uintptr_t>(which)));
    return button;
}
// PW36b: controls created after the build function returned stay registered but invisible (docs/UI_ADD_GUIDE.md
// answer summary item 3), so the two toggles are made inside the original CGameLayout setup, right after the native
// icon build call (0x8685C0) that runs for index 0x4C / 0x52. The patched table has 92 or 96 nodes.
static const unsigned kCount86=92,kCount90=96;
static void build_buttons(Obj layout) { COST_SCOPE("auto_commands:build_buttons");
    const unsigned nodes=dword(layout+0x130,0x10);
    const unsigned *roots=nodes==kCount86?kNodes86:nodes==kCount90?kNodes90:nullptr;
    if(!roots)return;
    layout_client::UiScope scope(kButtonsOwner);
    if(!scope.ok)return;
    if(layout!=buttons_layout) {buttons_layout=layout;button_search=button_reward=nullptr;}
    if(search_ready && !button_search)button_search=make_button(layout,roots[0],0);
    if(reward_ready && !button_reward)button_reward=make_button(layout,roots[1],1);
    refresh_button_labels();
#ifndef NO_RUNTIME_LOG
    log_event("auto_buttons_built",nodes,button_search?1u:0u,button_reward?1u:0u);
    if(button_search)log_event("auto_buttons_flags",0,button_flag_word(button_search),button_flag_pair(button_search));
    if(button_reward)log_event("auto_buttons_flags",1,button_flag_word(button_reward),button_flag_pair(button_reward));
#endif
}
// 8685C0(layout,index,object,mode): called twice for the 0x4B/0x4C icon loop and once for dmode 0x52. Run the
// original first, then build on the last icon of the active branch. The layout owner is 4452B0's own param.
static void __fastcall control_site(void *layout,unsigned index,void *object,unsigned z) { COST_SCOPE("auto_commands:control_site");
    next_control_site(layout,index,object,z);
    if(index==0x4C || index==0x52) {
        Obj owner=building_layout?building_layout:static_cast<Obj>(layout);
        if(owner)build_buttons(owner);
    }
}
static unsigned long long __fastcall layout_setup(Obj layout,unsigned long long a,unsigned long long b,unsigned long long c,unsigned long long d) { COST_SCOPE("auto_commands:layout_setup");
    // The setup wrapper only marks the layout as "being built" for the duration of the original call, so the
    // call-site wrapper knows the CGameLayout owner even when the 8685C0 layout argument differs.
    if(layout)building_layout=layout;
    const unsigned long long result=next_layout_setup(layout,a,b,c,d);
#ifndef NO_RUNTIME_LOG
    if(layout) {
        const unsigned nodes=dword(layout+0x130,0x10);
        if(nodes!=kCount86 && nodes!=kCount90)log_event("auto_buttons_skip",nodes,0,0);
        log_event("auto_buttons_setup",nodes,button_search?1u:0u,button_reward?1u:0u);
    }
#endif
    if(layout==building_layout)building_layout=nullptr;
    return result;
}
// Before the original destroy, while the parent still exists (UI_RESOURCE_LIFECYCLE 10).
static void __fastcall layout_dtor(Obj layout,unsigned long long a,unsigned long long b,unsigned long long c) { COST_SCOPE("auto_commands:layout_dtor");
    {
        layout_client::UiScope scope(kButtonsOwner);
        if(scope.ok)layout_client::ui_release(kButtonsOwner,reinterpret_cast<void *>(a));
    }
    if(layout==building_layout)building_layout=nullptr;
    if(layout==buttons_layout) {buttons_layout=nullptr;button_search=button_reward=nullptr;}
    next_layout_dtor(layout,a,b,c);
}
static bool install_layout_hooks() { COST_SCOPE("auto_commands:install_layout_hooks");
    layout_client::Transaction tx;
    if(!layout_client::vt(kSetupSlot,reinterpret_cast<void *>(&layout_setup),reinterpret_cast<void **>(&next_layout_setup)) ||
       !layout_client::vt(kDtorSlot,reinterpret_cast<void *>(&layout_dtor),reinterpret_cast<void **>(&next_layout_dtor)) ||
       !layout_client::call(kNormalSite,reinterpret_cast<void *>(&control_site),reinterpret_cast<void **>(&next_control_site)) ||
       !layout_client::call(kDmodeSite,reinterpret_cast<void *>(&control_site),reinterpret_cast<void **>(&next_control_site))) {
        log_event("auto_buttons_register_rejected",0,0,0);return false;
    }
    tx.done=true;log_event("auto_buttons_registered");return true;
}
// 자동탐색 (PW36b, user 2026-11-09): the main-screen toggle only turns the pass on or off. It no longer queues
// CStrategyAutoSearchState - pushing that screen from the main screen crashed in its setup (727000 -> 6C80D0 ->
// 7161D0 -> 717130). The conditions are the ones the original 인재 > 탐색 > 자동탐색 screen already saved at
// save+0x17B0/0x17B8/0x17B4; search_run reads them right before 진행.
static Obj player_ctx() { COST_SCOPE("auto_commands:player_ctx");
    Obj root=world();Obj save=root?pointer(root,0x871E8):nullptr;if(!save)return nullptr;
    return fn<Obj(__fastcall *)(Obj)>(GA(GA_230270))(fn<Obj(__fastcall *)(Obj)>(GA(GA_330B90))(save));
}
void toggle_search() {
    if(search_on_state) {
        search_on_state=false;
#ifndef NO_RUNTIME_LOG
        log_event("auto_search_toggle",0,0,0);
#endif
        show_toggle_toast(0,L"주공, 탐색 위임을 거두셨으니 이제부터 탐색은 주공께서 직접 명해 주십시오.");
        return;
    }
    search_on_state=true;search_turn=-1;   // turning on mid-turn still searches at the next 진행
#ifndef NO_RUNTIME_LOG
    log_event("auto_search_toggle",1,0,0);
#endif
    show_toggle_toast(0,L"주공, 이제부터 탐색은 제가 맡겠습니다. 인재 > 탐색 > 자동탐색에 정해 두신 조건으로 매 턴 진행 직전에 "
        L"주공을 대신해 탐색을 보내겠습니다. 명령서가 모자라면 보내지 않습니다.");
}
void toggle_reward() {
    if(reward_on_state) {
        reward_on_state=false;
#ifndef NO_RUNTIME_LOG
        log_event("auto_reward_toggle",0,static_cast<unsigned>(reward_limit),0);
#endif
        show_toggle_toast(1,L"주공, 포상 위임을 거두셨으니 이제부터 포상은 주공께서 직접 내려 주십시오.");
        return;
    }
    // Original number popup (CDialogGameStateBase<CCalculatorDialog>): fix the threshold, then turn on.
    static unsigned char args[0x20];
    memset(args,0,sizeof(args));
    *reinterpret_cast<int *>(args)=960;
    *reinterpret_cast<int *>(args+4)=540;
    *reinterpret_cast<unsigned long long *>(args+8)=
        fn<unsigned long long(__fastcall *)(unsigned)>(GA(GA_30F010))(0x414);
    *reinterpret_cast<int *>(args+0x10)=0;
    *reinterpret_cast<int *>(args+0x14)=100;
    popup_value=reward_limit;
    *reinterpret_cast<int **>(args+0x18)=&popup_value;
    std::function<void(void *)> cb=[](void *) {
        reward_limit=auto_commands_policy::clamp_threshold(popup_value);
        reward_on_state=true;
        refresh_button_labels_scoped();
        show_toggle_toast(1,L"주공, 이제부터 충성 "+std::to_wstring(reward_limit)+L" 이하 장수의 포상은 제가 맡겠습니다.");
#ifndef NO_RUNTIME_LOG
        log_event("auto_reward_toggle",1,static_cast<unsigned>(reward_limit),0);
#endif
    };
    [[maybe_unused]] Obj opened=fn<Obj(__fastcall *)(Obj,const char *,void *,void *)>(GA(GA_4603F0))(manager(),
        "CDialogGameStateBase<CCalculatorDialog>",args,&cb);
#ifndef NO_RUNTIME_LOG
    log_event("auto_reward_popup",1,static_cast<unsigned>(popup_value),opened?1u:0u);
#endif
}
// One original award command: every eligible officer is written into the command's id slots.
static void issue_award(const unsigned short *ids,unsigned count,Obj base) { COST_SCOPE("auto_commands:issue_award");
    unsigned char cmd[0x18];
    fn<void(__fastcall *)(void *)>(GA(GA_253F0))(cmd);
    unsigned *handle=*reinterpret_cast<unsigned **>(cmd+8);
    const unsigned key=handle?*handle:0;
    for(unsigned i=0;i<count;++i) {
        unsigned *slot=fn<unsigned *(__fastcall *)(Obj,unsigned,int)>(GA(GA_18C30))(image+GA(GA_1B5F9E0),key,1);
        if(slot)*slot=ids[i];
    }
    *reinterpret_cast<Obj *>(cmd+0x10)=base;
    fn<void(__fastcall *)(void *)>(GA(GA_1F3C90))(cmd);
    fn<void(__fastcall *)(void *)>(GA(GA_9E00))(cmd);
}
static void reward_turn() { COST_SCOPE("auto_commands:reward_turn");
    Obj root=world();Obj save=root?pointer(root,0x871E8):nullptr;if(!save)return;
    Obj ctx=fn<Obj(__fastcall *)(Obj)>(GA(GA_230270))(fn<Obj(__fastcall *)(Obj)>(GA(GA_330B90))(save));
    if(!ctx)return;
    const unsigned char force=ctx[0x10];
    unsigned tokens=ctx[0x14];
    const unsigned char threshold=static_cast<unsigned char>(auto_commands_policy::clamp_threshold(reward_limit));
    // user 2026-11-09: one award at the ruler's base, paid from that base's gold, for eligible officers of EVERY corps
    // of the force (the native 포상 window lists them all; the old per-base pass skipped other corps, e.g. 곽유지).
    // Ruler: force object root+0xDF30+force*8 (force < 0x34), ruler person id at +0x52 (717130 reads the same).
    Obj force_object=pointer(root,0xDF30+(force<0x34?force:0u)*8);
    const unsigned ruler_id=force_object?word(force_object,0x52):0;
    Obj ruler=ruler_id && ruler_id<=6000?table(0x158,ruler_id,6000):nullptr;
    const unsigned short base_id=valid(ruler)?word(ruler,0xE2):0;
    Obj base=base_id?fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(root,base_id):nullptr;
    if(!valid(base)) {
#ifndef NO_RUNTIME_LOG
        log_capped("auto_reward_skip",0,9,ruler_id);
#endif
        return;
    }
    std::vector<auto_commands_policy::RewardCandidate> candidates;
    for(unsigned id=1;id<=6000;++id) {
        Obj p=table(0x158,id,6000);
        if(!valid(p))continue;
        if(fn<unsigned(__fastcall *)(Obj)>(GA(GA_22E870))(p)!=force)continue;
        if(p[0xE6]==1)continue;
        const unsigned char loyalty=p[0xE8];
        if(loyalty>threshold || loyalty>=100)continue;
        if(fn<int(__fastcall *)(Obj)>(GA(GA_233BE0))(p)!=0)continue;
        if(fn<int(__fastcall *)(Obj,int)>(GA(GA_234AB0))(p,0)!=0)continue;
        candidates.push_back({static_cast<unsigned short>(id),loyalty});
    }
    Obj vtable=*reinterpret_cast<Obj *>(base);
    const int gold=static_cast<int>(reinterpret_cast<int(__fastcall *)(Obj)>(*reinterpret_cast<void **>(vtable+0x90))(base));
    const std::vector<unsigned short> chosen=auto_commands_policy::reward_pick(candidates,gold,static_cast<int>(tokens));
#ifndef NO_RUNTIME_LOG
    log_capped("auto_reward_ruler",ruler_id,base_id,static_cast<unsigned>(candidates.size()));
    if(chosen.empty())log_capped("auto_reward_skip",base_id,candidates.empty()?3u:tokens<1?2u:1u,static_cast<unsigned>(candidates.size()));
    else log_capped("auto_reward_base",base_id,static_cast<unsigned>(chosen.size()),static_cast<unsigned>(gold));
#endif
    [[maybe_unused]] unsigned awarded_bases=0,awarded_officers=0;
    std::vector<unsigned short> rewarded;   // PW36a: officers the awards actually covered
    if(!chosen.empty()) {
        issue_award(chosen.data(),static_cast<unsigned>(chosen.size()),base);
        rewarded=chosen;awarded_bases=1;awarded_officers=static_cast<unsigned>(chosen.size());
    }
#ifndef NO_RUNTIME_LOG
    log_event("auto_reward_turn",awarded_bases,awarded_officers,static_cast<unsigned>(reward_limit));
#endif
    // PW36a (R8d §7-C): report the pass in the military briefing, but only when an award was really issued.
    if(awarded_officers && mod_features.advisor_briefing) {
        const auto officer_name=[](unsigned short id) -> std::wstring {
            Obj p=table(0x158,id,6000);
            if(!valid(p))return {};
            const wchar_t *name=fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(p);
            return name?std::wstring(name):std::wstring();
        };
        const unsigned n=awarded_officers;
        std::wstring first,second;
        for(size_t i=0;i<rewarded.size() && (first.empty() || second.empty());++i) {
            std::wstring name=officer_name(rewarded[i]);
            if(name.empty())continue;
            if(first.empty())first=name;else second=name;
        }
        std::wstring text=L"주공을 대신해 ";
        if(!first.empty()) {
            if(n==1)text+=first+L"에게 포상했습니다";
            else if(!second.empty())text+=first+L", "+second+(n>2?L" 외 "+std::to_wstring(n-2)+L"명":std::wstring())+L"에게 포상했습니다";
            else text+=first+L" 외 "+std::to_wstring(n-1)+L"명에게 포상했습니다";
        } else text+=L"장수 "+std::to_wstring(n)+L"명에게 포상했습니다";
        [[maybe_unused]] const bool added=advisor_briefing::add_item(text);
#ifndef NO_RUNTIME_LOG
        log_event("auto_reward_brief",n,static_cast<unsigned>(text.size()),added?1u:0u);
#endif
    }
}
static void __fastcall reward_site(Obj a) { COST_SCOPE("auto_commands::reward_site");
    fn<void(__fastcall *)(Obj)>(GA(GA_17B990))(a);
    // PW36b: the main-screen toggles queue a pending bit; drain it here, on the player strategy frame, after the
    // original call. UI work (labels, popup, toast) then runs on the game UI thread that owns this frame.
    if(const LONG clicks=InterlockedExchange(&pending_clicks,0)) {
        if(clicks&1)button_clicked(0);
        if(clicks&2)button_clicked(1);
    }
    if(InterlockedExchange(&labels_dirty,0))refresh_button_labels_scoped();
    if(!reward_on_state)return;
    Obj top=top_state();
    if(!top || pointer(top,0)!=image+GA(GA_13D9430))return;
    if(!auto_commands_policy::turn_changed(auto_commands_policy::turn_of(game_day_read()),last_date))return;   // local17: once per turn
    reward_turn();
}
static bool call_to(unsigned site,unsigned target) { COST_SCOPE("auto_commands:call_to");
    return image[site]==0xE8 && site+5+*reinterpret_cast<int *>(image+site+1)==target;
}
static CallLink reward_link;
static bool install_reward() { COST_SCOPE("auto_commands:install_reward");
    if(!call_to(GA(GA_44EDD1),GA(GA_17B990))) {log_event("auto_reward_guard_rejected");return false;}
    Obj relay=allocate_near(image+GA(GA_44EDD1));if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&reward_site));
    DWORD old;
    if(!call_prepare(reward_link,GA(GA_44EDD1),relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,32)) {
        VirtualFree(relay,0,MEM_RELEASE);log_event("auto_reward_prepare_rejected");return false;
    }
    if(!call_publish(reward_link)) {log_event("auto_reward_publish_rejected");return false;}
    log_event("auto_reward_installed");return true;
}
// One search pass at 진행: the same registry the native confirm fills (7161D0) and the same per-officer command
// pair (6DF1B0 -> 1F3F50), capped by the 명령서 the corps holds. 7205C0 never clears person+0x15E bit 0x40, so
// clear it ourselves for every officer of our list (7AAC90 treats 0x40 as excluded).
static void search_run() { COST_SCOPE("auto_commands:search_run");
    Obj root=world();Obj save=root?pointer(root,0x871E8):nullptr;if(!save)return;
    Obj ctx=player_ctx();if(!ctx)return;
    const int cost=fn<int(__fastcall *)()>(GA(GA_6D5A60))();
    const int tokens=cost>0?static_cast<int>(ctx[0x14])/cost:0;
    if(tokens<1) {
#ifndef NO_RUNTIME_LOG
        log_event("auto_search_skip",1,0,0);
#endif
        return;
    }
    Obj pool=image+GA(GA_27D4178),registry=image+GA(GA_27D4130);
    alignas(16) static unsigned char state[0x4D0];
    memset(state,0,sizeof(state));
    *reinterpret_cast<Obj *>(state+0x470)=ctx;
    Obj node=fn<Obj(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_18AC0))(pool,0,0);
    if(!node) {
#ifndef NO_RUNTIME_LOG
        log_event("auto_search_skip",2,0,0);
#endif
        return;
    }
    *reinterpret_cast<Obj *>(state+0x4B0)=image+GA(GA_13466B0);
    *reinterpret_cast<Obj *>(state+0x4B8)=node;
    const unsigned key=*reinterpret_cast<unsigned *>(node);
    Obj table_slot=image+GA(GA_27812C8);Obj old_table=*reinterpret_cast<Obj *>(table_slot);
    alignas(16) static unsigned char areas[0xBB88];
    memset(areas,0,sizeof(areas));
    *reinterpret_cast<Obj *>(table_slot)=areas;
    const auto_commands_policy::SearchSettings saved{
        *reinterpret_cast<int *>(save+0x17B0),*reinterpret_cast<int *>(save+0x17B8),*reinterpret_cast<int *>(save+0x17B4)};
    const auto_commands_policy::SearchWidgets shown=auto_commands_policy::widgets_from_save(saved);
#ifndef NO_RUNTIME_LOG
    // PW36b: keeps the last thing the mod does before entering the original registry build (7161D0 -> 717130) so a
    // crash dump in there can be told apart from a crash in this module's own list walk.
    log_event("auto_search_run_begin",static_cast<unsigned>(tokens),0,0);
#endif
    // user 2026-11-09 (local11 dump, fault 7178D2): 7161D0 hands 717130 one std::map per corps of the force as
    // state+0x478 + index*0x10 ({head,size}, node 0x30, key at +0x20, int at +0x28). The zeroed state had no array, so
    // 717130 read head 0. Build empty maps with the original head allocator (59170), clear (59230) and free the heads
    // (4EA20, size 0x30) after the call, exactly as 7161D0 handles its own local map of the same node type.
    struct MapHead {Obj head;unsigned long long size;};
    static const unsigned kCorpsMaps=128;
    alignas(16) static MapHead maps[kCorpsMaps];
    for(auto &map:maps) {map.head=fn<Obj(__fastcall *)()>(GA(GA_59170))();map.size=0;}
    *reinterpret_cast<MapHead **>(state+0x478)=maps;
    // user 2026-11-09 ("원작은 금방 뜨던데"): the per-officer distance 2AEC00 is a lookup in the city distance table
    // 2147F38 when it exists, and a full path search (2B0CB0) when it does not. The original 탐색 screen builds the
    // table on enter (72F2D0/730C50 -> 2CF820, constant data) and frees it on leave (722530/722AB0 -> 2FCE60). Do the
    // same around the registry build, and free it only when this pass created it.
    const bool own_distance=!pointer(image,GA(GA_2147F38));
    if(own_distance)fn<void(__fastcall *)()>(GA(GA_2CF820))();
#ifndef NO_RUNTIME_LOG
    LARGE_INTEGER build_from,build_to,build_hz;QueryPerformanceCounter(&build_from);
#endif
    fn<void(__fastcall *)(Obj,Obj,int,int,int)>(GA(GA_7161D0))(state,state+0x4B0,shown.scope,shown.basis,shown.region);
    if(own_distance)fn<void(__fastcall *)()>(GA(GA_2FCE60))();
#ifndef NO_RUNTIME_LOG
    QueryPerformanceCounter(&build_to);QueryPerformanceFrequency(&build_hz);
    log_event("auto_search_build_us",static_cast<unsigned>((build_to.QuadPart-build_from.QuadPart)*1000000/build_hz.QuadPart),own_distance?1u:0u,0);
#endif
    for(auto &map:maps) {
        fn<void(__fastcall *)(MapHead *)>(GA(GA_59230))(&map);
        fn<void(__fastcall *)(Obj,unsigned long long,unsigned long long)>(GA(GA_4EA20))(map.head,1,0x30);
        map.head=nullptr;
    }
    struct Node {Obj person;Node *next;};
    Node *head=nullptr;
    const unsigned key_limit=dword(image,GA(GA_27D4170));
    Obj heads=pointer(image,GA(GA_27D4140));
    if(heads && key<key_limit)head=*reinterpret_cast<Node **>(heads+key*8);
    unsigned count=0;
    for(Node *p=head;p;p=p->next)++count;
    const unsigned take=static_cast<unsigned>(auto_commands_policy::search_take(static_cast<int>(count),tokens));
    unsigned issued=0;
    Node *p=head;
    for(unsigned i=0; p && i<take; ++i,p=p->next) {
        if(!p->person)continue;
        const unsigned pid=word(p->person,0x10);
        if(pid>6000)continue;
        const unsigned long long area=*reinterpret_cast<unsigned long long *>(areas+pid*8);
        Obj target=fn<Obj(__fastcall *)(Obj,Obj,unsigned long long)>(GA(GA_6DF1B0))(ctx,p->person,area);
        if(!target)continue;
        struct Pair {unsigned long long target,person;} pair{
            reinterpret_cast<unsigned long long>(target),reinterpret_cast<unsigned long long>(p->person)};
        fn<void(__fastcall *)(void *,Obj)>(GA(GA_1F3F50))(&pair,fn<Obj(__fastcall *)(Obj)>(GA(GA_235300))(ctx));
        ++issued;
#ifndef NO_RUNTIME_LOG
        log_capped("auto_search_officer",pid,static_cast<unsigned>(area),0);
#endif
    }
    for(Node *q=head;q;q=q->next)if(q->person)q->person[0x15E]&=static_cast<unsigned char>(~0x40);
    fn<void(__fastcall *)(Obj,unsigned)>(GA(GA_186D0))(registry,key);
    fn<void(__fastcall *)(Obj,Obj)>(GA(GA_18790))(pool,node);
    *reinterpret_cast<Obj *>(table_slot)=old_table;
#ifndef NO_RUNTIME_LOG
    log_event("auto_search_run",issued,count,static_cast<unsigned>(tokens));
#endif
}
// Progress block of 44ED30: the original call 453310(0) at 44F281, one search pass first while the toggle is on.
// local14 WPR (user 2026-11-09 "탐색위임 끄니까 끊기는거 사라지긴한다"): this site runs on every day of 진행, and each
// pass rebuilt the 7161D0 registry (path search per officer, ~300 ms main-thread stall). Search once per 10-day turn.
static int __fastcall search_site(Obj a) { COST_SCOPE("auto_commands::search_site");
    if(mod_features.auto_search && search_on_state &&
       auto_commands_policy::turn_changed(auto_commands_policy::turn_of(game_day_read()),search_turn))search_run();
    return fn<int(__fastcall *)(Obj)>(GA(GA_453310))(a);
}
static CallLink search_link;
static bool install_search() { COST_SCOPE("auto_commands:install_search");
    // PW36b: only the 진행 call site is replaced. The queued push, the 734620 frame and the 7205C0 leave slots are
    // gone with the main-screen auto-search screen path (that push crashed in its setup).
    if(!call_to(GA(GA_44F281),GA(GA_453310))) {log_event("auto_search_guard_rejected");return false;}
    Obj relay=allocate_near(image+GA(GA_44F281));if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&search_site));
    DWORD old;
    if(!call_prepare(search_link,GA(GA_44F281),relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,32)) {
        VirtualFree(relay,0,MEM_RELEASE);log_event("auto_search_prepare_rejected");return false;
    }
    if(!call_publish(search_link)) {log_event("auto_search_publish_rejected");return false;}
    log_event("auto_search_installed");return true;
}
bool install() {
    bool ok=true;
    // PW36b: the 17B990 frame site also drains the button clicks, so it is needed whenever either toggle is on;
    // the award pass itself still runs only for auto_reward (reward_ready).
    if(mod_features.auto_reward || mod_features.auto_search) {const bool site=install_reward();if(mod_features.auto_reward)reward_ready=site;ok=site && ok;}
    if(mod_features.auto_search) {search_ready=install_search();ok=search_ready && ok;}
    // Layout hooks only when at least one toggle is enabled (rule 1): both off leaves the original screen untouched.
    if(mod_features.auto_search || mod_features.auto_reward)ok=install_layout_hooks() && ok;
    return ok;
}
#if defined(SPLIT_BASE)
// user 2026-11-09: "세이브가 달라져도 새 시나리오를 시작해도 그대로 켜져 있다" - the two toggles and the loyalty limit
// belong to one save. Sidecar <save>.<sha256>.autocmd = "ACMD" + version 1 + 3 pad + 32-byte save binding + search,
// reward, limit, pad (44 bytes). Every load (new scenario included: restore(nullptr)) starts off at limit 70.
static const size_t kSideSize=44;
static unsigned char saved_search,saved_reward,saved_limit=70;
static SidecarCache side_cache;
static void side_reset() {search_on_state=false;reward_on_state=false;reward_limit=70;search_turn=-1;last_date=-1;}
static void side_snapshot() { COST_SCOPE("auto_commands:side_snapshot");
    saved_search=search_on_state?1:0;saved_reward=reward_on_state?1:0;
    saved_limit=static_cast<unsigned char>(auto_commands_policy::clamp_threshold(reward_limit));
}
static bool side_write(const merit::SaveCoordinator::Completed &job,save_host::Written &out) { COST_SCOPE("auto_commands:side_write");
    std::vector<uint8_t> bytes(kSideSize,0);
    memcpy(bytes.data(),"ACMD",4);bytes[4]=1;memcpy(bytes.data()+8,job.binding.data(),32);
    bytes[40]=saved_search;bytes[41]=saved_reward;bytes[42]=saved_limit;
    return save_host::store(self_module,save_host::Job{job.name,job.binding},L"autocmd",bytes,side_cache,out);
}
static bool side_restore(const merit::SaveCoordinator::Completed *job) { COST_SCOPE("auto_commands:side_restore");
    side_reset();bool restored=false;
    if(job) {
        wchar_t path[MAX_PATH];
        if(save_host::path(self_module,save_host::Job{job->name,job->binding},L"autocmd",path,MAX_PATH,false)) {
            HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file!=INVALID_HANDLE_VALUE) {
                unsigned char bytes[kSideSize]={};DWORD got=0;LARGE_INTEGER length={};
                if(GetFileSizeEx(file,&length) && length.QuadPart==static_cast<LONGLONG>(kSideSize) &&
                   ReadFile(file,bytes,static_cast<DWORD>(kSideSize),&got,nullptr) && got==kSideSize &&
                   !memcmp(bytes,"ACMD",4) && bytes[4]==1 && !memcmp(bytes+8,job->binding.data(),32)) {
                    search_on_state=bytes[40]!=0 && mod_features.auto_search;
                    reward_on_state=bytes[41]!=0 && mod_features.auto_reward;
                    reward_limit=auto_commands_policy::clamp_threshold(bytes[42]);
                    restored=true;
                }
                CloseHandle(file);
            }
        }
    }
    InterlockedExchange(&labels_dirty,1);   // the load host is no UI frame: reward_site redraws the labels
#ifndef NO_RUNTIME_LOG
    log_event("auto_commands_load",search_on_state?1u:0u,reward_on_state?1u:0u,static_cast<unsigned>(reward_limit)|(restored?0x100u:0u));
#endif
    return restored;
}
static const save_host::SidecarPlugin sidecar_plugin={"auto_commands",L"autocmd",1,"auto_commands_save_written","auto_commands_save_write_failed",
    []() {return mod_features.auto_search || mod_features.auto_reward;},
    []() {side_snapshot();},
    [](const save_host::Job &job,save_host::Written &out) {return side_write(*static_cast<const merit::SaveCoordinator::Completed *>(job.source),out);},
    []() {side_cache.clear();side_reset();},
    [](const save_host::Job *job) {return side_restore(job?static_cast<const merit::SaveCoordinator::Completed *>(job->source):nullptr);}};
#endif
}
#if defined(TACTICS_FIXTURE)
// Unit-test shell only: the pure policy checks travel with the module source so the fixture build compiles them.
#include "../tests/auto_commands_fixture.inl"
#endif
