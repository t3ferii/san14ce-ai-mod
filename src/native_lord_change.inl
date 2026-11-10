// [officer_ui] lord_change (2026-10-23, user request; behaviour of reference_mod SAN14_LordChange +
// SAN14_LordFix, re-derived from this EXE). New game, force selection: a native footer button
// 「반란」 left of 세력일람 picks an officer of the selected force and makes him ruler.
// 2026-10-23 user: label 「세력내 군주교체」 -> 「반란」 (text too small); no yes/no step (choosing = 결정).
// 2026-10-23: the long-press path (OS mouse thread + info view SetPerson hook) did not trigger in game
// and was replaced by this button (user: same font as 세력일람/시정/휘하무장, no overlay button).
// Original (근거: 1a4a447d 이전 기록, docs/HISTORY.md):
//   Force selection = CPersonSelectState (vtable 13E5018). Slot +08 = 5035C0 builds the footer
//   CCmnFooter (vt 146AFE8) at state+4E0 on layout cmn_footer_re_cn (KTID D8913AB2): text buttons
//   0xE/0x11/0x12 via 85FEF0 -> 85FCE0 (only those ids). Selected force = state+490.
//   85FCE0 text button: 92E430(bar+130,id,2,0,1,0) creates the hanyou button at layout node id,
//   vtable+1D8 sets its text, +80 id, +84 = 1, click functor 146D8A0{bar} via 1A3CB0(button,7FFFFFFE).
//   The functor writes bar+80 = id and fires 0x578; the state functor 13F52F0 calls state slot +70 =
//   533570(state,bar): switch(bar+80) 1/2/3/0xE/0x11/0x12; other ids do nothing.
//   Native list picker (as auto-employ): 250EA0 config, +0 targets, +18 mode 2, +1C limit, +28 title,
//   +80 selected, 45B620 builds it, 45BE60 runs it modally (in game only), 251A10 free. 45ED60 pushes
//   "CDialogOpenStateBase<CDataSelectDlgBase<Data>>" with a close callable. 28DB70(old,new,list,0) hands
//   the force to another person; 22CA00(person) force; force+10 ruler id.
//   After the start, a ruler taken from new/ancient officers keeps status Person+E6 9 or 4 and cannot
//   give orders; status 1 (+E7 0x49) fixes it.
// Mod: the ScreenLayout patch (research/lord_change/build_footer.py) adds node 24 (0x18), a copy of
// 세력일람's node one button width to the left. Right after 5035C0 builds 휘하무장 (CALL 85FEF0 at
// 503C9C), the same native sequence as 85FCE0 builds button 0x18 there; without the node nothing is
// added. Clicks with id 0x18 are taken before 533570.
// 2026-10-23: built after 5035C0 returned, the button was created (log) but never shown: 5035C0 then
// registers the footer (570440) and starts its show; controls made later stay hidden (user screenshot).
// The status fix runs on the map state's per-frame busy check (fast_menu slot), only for rulers made here.
#include "game_addresses.h"
namespace lord_change {
static const unsigned kLastButtonCall=GA(GA_5064FC),kSetText=GA(GA_862220),kClickSlot=GA(GA_13E80A8)+0x70,kClick=GA(GA_535D90);
static const unsigned kFooterVt=GA(GA_146E078),kFooterFunctorVt=GA(GA_1470930);
static const int kButton=0x18;
static unsigned short made[16];static volatile LONG made_count;
using SetText=void(__fastcall *)(Obj,int,const wchar_t *,int);
using Click=void(__fastcall *)(Obj,Obj);
static SetText next_set_text;   // Layout manager: next in the chain (original 85FEF0 unless another mod chained first)
static Click next_click;        // 533570
static Obj ruler_of(Obj force) {
    if(!valid(force))return nullptr;
    Obj r=table(0x158,word(force,0x10),6000);
    return valid(r) && fn<Obj(__fastcall *)(Obj)>(GA(GA_22E450))(r)==force?r:nullptr;
}
static void change(Obj old,Obj person) {   // choosing him in the list (결정) is the confirmation
    GameList list;list_init(list,GA(GA_13466B0));
    fn<void(__fastcall *)(Obj,Obj,GameList *,int)>(GA(GA_28F910))(old,person,&list,0);
    list_free(list);
    LONG n=made_count;if(n<16) {made[n]=word(person,0x10);InterlockedExchange(&made_count,n+1);}
    log_event("lord_change_done",word(old,0x10),word(person,0x10));
}
// The list dialog writes into these until it closes: they live until the next pick (UI thread only).
static struct Pick {bool open,lists;GameList targets,selected;Obj force,old,state;} pending;
// 2026-10-23 user: after the change the screen still showed 조조. The panel (state+480) caches the
// ruler: its face row (node 0xC, list +180 of ruler persons, 53F570 redraws the page, also used for
// paging) and the portrait/info (522580 = state slot +78 "select force": 51FAC0 info + portrait,
// 554F00 highlight, ruler voice). The old ruler is replaced in the row list and both are redrawn.
static void refresh(Obj state,Obj force,Obj old,Obj person) {
    Obj panel=pointer(state,0x480);
    if(panel)if(Obj row=fn<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(panel+0x130,0xC)) {
        for(Link *l=list_first(*reinterpret_cast<GameList *>(row+0x180));l;l=l->next)if(l->value==old)l->value=person;
        fn<void(__fastcall *)(Obj)>(GA(GA_541D90))(row);
    }
    fn<void(__fastcall *)(Obj,Obj)>(GA(GA_524DE0))(state,force);
}
static void picked(Obj) {   // dialog closed (결정 or 뒤로), game UI thread
    if(!pending.open)return;
    pending.open=false;
    Link *first=list_first(pending.selected);Obj person=first?first->value:nullptr;
    Obj old=ruler_of(pending.force);
    if(old && old==pending.old && valid(person) && person!=old && fn<Obj(__fastcall *)(Obj)>(GA(GA_22E450))(person)==pending.force) {
        change(old,person);
        if(ruler_of(pending.force)==person)refresh(pending.state,pending.force,old,person);
    }
    else log_event("lord_change_cancelled",person?1u:0u);
}
static void pick(Obj state) {   // game UI thread (state click)
    // 2026-10-23: the modal 45BE60 returned at once on this screen (log lord_change_cancelled, no
    // dialog): before the game starts there is no modal runner (state manager+48). Like 533570's
    // 세력일람, the dialog is pushed as a state (45ED60) and the result comes back through a callable.
    if(pending.open)return;
    Obj force=pointer(state,0x490),old=ruler_of(force);
    if(!old) {log_event("lord_change_no_ruler");return;}
    if(pending.lists) {list_free(pending.selected);list_free(pending.targets);pending.lists=false;}
    list_init(pending.targets,GA(GA_13466B0));list_init(pending.selected,GA(GA_13466B0));pending.lists=true;
    unsigned count=0;
    for(unsigned id=0;id<6000;++id) {
        Obj p=table(0x158,id,6000);
        if(valid(p) && p!=old && fn<Obj(__fastcall *)(Obj)>(GA(GA_22E450))(p)==force && list_add(pending.targets,p))++count;
    }
    if(!count) {log_event("lord_change_no_officer",word(force,0x10));return;}
    alignas(8) unsigned char config[0x400]={};
    fn<Obj(__fastcall *)(Obj)>(GA(GA_2528E0))(config);
    *reinterpret_cast<GameList **>(config)=&pending.targets;
    *reinterpret_cast<Obj *>(config+8)=image+GA(GA_1A34310);
    *reinterpret_cast<Obj *>(config+16)=image+GA(GA_1A2E830);
    *reinterpret_cast<unsigned *>(config+0x18)=2;   // native multi-selection, limited to one
    *reinterpret_cast<unsigned *>(config+0x1C)=1;
    *reinterpret_cast<const wchar_t **>(config+0x28)=L"반란: 새 군주 선택";
    *reinterpret_cast<GameList **>(config+0x80)=&pending.selected;
    fn<void(__fastcall *)(Obj,int)>(GA(GA_45D850))(config,0);   // builds the dialog in slot 27D3CA0[0]
    Obj dialog=pointer(image,GA(GA_27D6CF0));
    if(!dialog) {fn<void(__fastcall *)(Obj)>(GA(GA_253450))(config);log_event("lord_change_no_dialog");return;}
    // 45BE60 before its modal run: result list, then the two list resets it does when +30/+38 are empty.
    *reinterpret_cast<Obj *>(pointer(dialog,0x1A8)+0x80)=reinterpret_cast<Obj>(&pending.selected);
    {GameList t;list_init(t,GA(GA_13466B0));
     reinterpret_cast<void(__fastcall *)(Obj,GameList *)>(pointer(pointer(dialog,0),0x200))(dialog,&t);list_free(t);}
    {GameList t;list_init(t,GA(GA_13466B0));Link *first=list_first(t);Obj none=nullptr;
     fn<void(__fastcall *)(Obj,Link **,Obj *)>(GA(GA_2EF250))(pointer(dialog,0x180),&first,&none);list_free(t);}
    fn<void(__fastcall *)(Obj)>(GA(GA_253450))(config);
    pending.force=force;pending.old=old;pending.state=state;pending.open=true;
    alignas(16) unsigned char local[0x40]={};
    *reinterpret_cast<void ***>(local)=ui_callable::vtable;*reinterpret_cast<void (**)(Obj)>(local+8)=&picked;
    *reinterpret_cast<void **>(local+0x38)=local;
    Obj holder=dialog;
    fn<void(__fastcall *)(Obj,const char *,Obj *,void *)>(GA(GA_460F70))(fn<Obj(__fastcall *)()>(GA(GA_11040))(),
        "CDialogOpenStateBase<CDataSelectDlgBase<Data>>",&holder,local);
    log_event("lord_change_list_opened",count);
}
static void add_button(Obj bar) {
    // Mirrors 85FEF0/85FCE0 for text button id 0x18 (layout node 24 from the ScreenLayout patch).
    if(!bar || pointer(bar,0)!=image+kFooterVt)return;
    Obj table=bar+0x130;
    if(kButton>=static_cast<int>(dword(table,0x10))) {log_event("lord_change_no_layout_node");return;}
    if(fn<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(table,kButton))return;
    Obj button=fn<Obj(__fastcall *)(Obj,int,int,int,int,int)>(GA(GA_9309C0))(table,kButton,2,0,1,0);
    if(!button)return;
    reinterpret_cast<void(__fastcall *)(Obj,const wchar_t *)>(pointer(pointer(button,0),0x1D8))(button,L"반란");
    *reinterpret_cast<unsigned *>(button+0x84)=1;*reinterpret_cast<int *>(button+0x80)=kButton;
    alignas(16) unsigned char local[0x40]={};
    *reinterpret_cast<Obj *>(local)=image+kFooterFunctorVt;*reinterpret_cast<Obj *>(local+8)=bar;*reinterpret_cast<void **>(local+0x38)=local;
    fn<void(__fastcall *)(Obj,int,void *)>(GA(GA_1A4FB0))(button,0x7FFFFFFE,local);   // copies, then destroys local
    if(Obj control=fn<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(table,kButton))fn<void(__fastcall *)(Obj,int,int)>(GA(GA_931DA0))(control+0x130,0x11,0);
    fn<void(__fastcall *)(Obj,int,int)>(GA(GA_931DA0))(table,0x15,1);
    // Stays enabled: 5035C0 enables 0x11/0x12 by the force at setup time, but a force may be chosen
    // later. The click checks the selected force and its ruler instead.
    log_event("lord_change_button_added");
}
static void __fastcall set_text(Obj bar,int index,const wchar_t *text,int flag) { COST_SCOPE("lord_change::set_text"); COST_CRUMB("lord_change_bar",bar);   // only CALL 503C9C (휘하무장)
    next_pick(next_set_text,kSetText)(bar,index,text,flag);
    if(mod_features.lord_change)add_button(bar);
}
static void __fastcall click(Obj state,Obj bar) { COST_SCOPE("lord_change::click"); COST_CRUMB("lord_change_state",state);
    if(mod_features.lord_change && bar && dword(bar,0x80)==static_cast<unsigned>(kButton)) {pick(state);return;}
    next_pick(next_click,kClick)(state,bar);
}
// Map per-frame (fast_menu's state slot): rulers made here get an active status after the start.
static void map_tick() {
    LONG n=made_count;
    for(LONG i=0;i<n;++i) {
        Obj p=table(0x158,made[i],6000);if(!valid(p))continue;
        unsigned char status=p[0xE6];if(status!=9 && status!=4)continue;
        Obj force=fn<Obj(__fastcall *)(Obj)>(GA(GA_22E450))(p);
        if(!valid(force) || word(force,0x10)!=made[i])continue;
        *reinterpret_cast<unsigned short *>(p+0xE6)=0x4901;
        log_event("lord_change_status_fixed",made[i],status);
    }
}
static bool guards_match() {
    static const unsigned char before[]={0x41,0x8D,0x51,0x12,0x48,0x8B,0x8E,0xE0,0x04,0x00,0x00};   // lea edx,[r9+12h]; mov rcx,[rsi+4E0h]
    // The CALL target and the 533570 slot are judged by the Layout hook manager (chain / yield / refuse).
    return !ga::mcmp_at(image,kLastButtonCall-sizeof(before),before,sizeof(before)) &&
        pointer(image,kFooterFunctorVt)==image+GA(GA_862510);
}
static bool install() {
    if(!guards_match()) {log_event("lord_change_guard_rejected");return false;}
    layout_client::Transaction tx;
    if(!layout_client::call(kLastButtonCall,reinterpret_cast<void *>(&set_text),reinterpret_cast<void **>(&next_set_text)) ||
       !layout_client::vt(kClickSlot,reinterpret_cast<void *>(&click),reinterpret_cast<void **>(&next_click))) {
        log_event("lord_change_register_rejected");return false;
    }
    tx.done=true;log_event("lord_change_installed");return true;
}
}
