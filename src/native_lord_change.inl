// [officer_ui] lord_change (2026-10-23, user request; behaviour of reference_mod SAN14_LordChange +
// SAN14_LordFix, re-derived from this EXE). New game, force selection: a native footer button
// 「반란」 left of 세력일람 picks an officer of the selected force and makes him ruler.
// 2026-10-23 user: label 「세력내 군주교체」 -> 「반란」 (text too small); no yes/no step (choosing = 결정).
// 2026-10-23: the long-press path (OS mouse thread + info view SetPerson hook) did not trigger in game
// and was replaced by this button (user: same font as 세력일람/시정/휘하무장, no overlay button).
// Original (unpacked 1fbe6be7...):
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
namespace lord_change {
static const unsigned kLastButtonCall=0x503C9C,kSetText=0x85FEF0,kClickSlot=0x13E5018+0x70,kClick=0x533570;
static const unsigned kFooterVt=0x146AFE8,kFooterFunctorVt=0x146D8A0;
static const int kButton=0x18;
static unsigned short made[16];static volatile LONG made_count;
using SetText=void(__fastcall *)(Obj,int,const wchar_t *,int);
using Click=void(__fastcall *)(Obj,Obj);
static Obj ruler_of(Obj force) {
    if(!valid(force))return nullptr;
    Obj r=table(0x158,word(force,0x10),6000);
    return valid(r) && fn<Obj(__fastcall *)(Obj)>(0x22CA00)(r)==force?r:nullptr;
}
static void change(Obj old,Obj person) {   // choosing him in the list (결정) is the confirmation
    GameList list;list_init(list,0x13436B0);
    fn<void(__fastcall *)(Obj,Obj,GameList *,int)>(0x28DB70)(old,person,&list,0);
    list_free(list);
    LONG n=made_count;if(n<16) {made[n]=word(person,0x10);InterlockedExchange(&made_count,n+1);}
    log_event("lord_change_done",word(old,0x10),word(person,0x10));
}
// std::function callable in the game's MSVC layout (vtable: copy, move, call, target type, delete, get),
// as 565EB0/65DFE0 implement it: [0] vtable, [8] callback; stored inline in the caller's 0x40 buffer.
struct Callable {void **vtable;void (*callback)(Obj);};
static void *__fastcall callable_copy(Callable *self,Callable *where) {if(where)*where=*self;return where;}
static void __fastcall callable_call(Callable *self,Obj *arg) {self->callback(arg?*arg:nullptr);}
static void *__fastcall callable_type(Callable *) {return nullptr;}
static void __fastcall callable_delete(Callable *self,bool heap) {if(heap)fn<void(__fastcall *)(void *)>(0x3F6C50)(self);}
static void *__fastcall callable_get(Callable *self) {return &self->callback;}
static void *callable_vtable[6]={reinterpret_cast<void *>(&callable_copy),reinterpret_cast<void *>(&callable_copy),
    reinterpret_cast<void *>(&callable_call),reinterpret_cast<void *>(&callable_type),reinterpret_cast<void *>(&callable_delete),
    reinterpret_cast<void *>(&callable_get)};
// The list dialog writes into these until it closes: they live until the next pick (UI thread only).
static struct Pick {bool open,lists;GameList targets,selected;Obj force,old,state;} pending;
// 2026-10-23 user: after the change the screen still showed 조조. The panel (state+480) caches the
// ruler: its face row (node 0xC, list +180 of ruler persons, 53F570 redraws the page, also used for
// paging) and the portrait/info (522580 = state slot +78 "select force": 51FAC0 info + portrait,
// 554F00 highlight, ruler voice). The old ruler is replaced in the row list and both are redrawn.
static void refresh(Obj state,Obj force,Obj old,Obj person) {
    Obj panel=pointer(state,0x480);
    if(panel)if(Obj row=fn<Obj(__fastcall *)(Obj,int)>(0x92CEE0)(panel+0x130,0xC)) {
        for(Link *l=list_first(*reinterpret_cast<GameList *>(row+0x180));l;l=l->next)if(l->value==old)l->value=person;
        fn<void(__fastcall *)(Obj)>(0x53F570)(row);
    }
    fn<void(__fastcall *)(Obj,Obj)>(0x522580)(state,force);
}
static void picked(Obj) {   // dialog closed (결정 or 뒤로), game UI thread
    if(!pending.open)return;
    pending.open=false;
    Link *first=list_first(pending.selected);Obj person=first?first->value:nullptr;
    Obj old=ruler_of(pending.force);
    if(old && old==pending.old && valid(person) && person!=old && fn<Obj(__fastcall *)(Obj)>(0x22CA00)(person)==pending.force) {
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
    list_init(pending.targets,0x13436B0);list_init(pending.selected,0x13436B0);pending.lists=true;
    unsigned count=0;
    for(unsigned id=0;id<6000;++id) {
        Obj p=table(0x158,id,6000);
        if(valid(p) && p!=old && fn<Obj(__fastcall *)(Obj)>(0x22CA00)(p)==force && list_add(pending.targets,p))++count;
    }
    if(!count) {log_event("lord_change_no_officer",word(force,0x10));return;}
    alignas(8) unsigned char config[0x400]={};
    fn<Obj(__fastcall *)(Obj)>(0x250EA0)(config);
    *reinterpret_cast<GameList **>(config)=&pending.targets;
    *reinterpret_cast<Obj *>(config+8)=image+0x1A31330;
    *reinterpret_cast<Obj *>(config+16)=image+0x1A2B850;
    *reinterpret_cast<unsigned *>(config+0x18)=2;   // native multi-selection, limited to one
    *reinterpret_cast<unsigned *>(config+0x1C)=1;
    *reinterpret_cast<const wchar_t **>(config+0x28)=L"반란: 새 군주 선택";
    *reinterpret_cast<GameList **>(config+0x80)=&pending.selected;
    fn<void(__fastcall *)(Obj,int)>(0x45B620)(config,0);   // builds the dialog in slot 27D3CA0[0]
    Obj dialog=pointer(image,0x27D3CA0);
    if(!dialog) {fn<void(__fastcall *)(Obj)>(0x251A10)(config);log_event("lord_change_no_dialog");return;}
    // 45BE60 before its modal run: result list, then the two list resets it does when +30/+38 are empty.
    *reinterpret_cast<Obj *>(pointer(dialog,0x1A8)+0x80)=reinterpret_cast<Obj>(&pending.selected);
    {GameList t;list_init(t,0x13436B0);
     reinterpret_cast<void(__fastcall *)(Obj,GameList *)>(pointer(pointer(dialog,0),0x200))(dialog,&t);list_free(t);}
    {GameList t;list_init(t,0x13436B0);Link *first=list_first(t);Obj none=nullptr;
     fn<void(__fastcall *)(Obj,Link **,Obj *)>(0x2ECE90)(pointer(dialog,0x180),&first,&none);list_free(t);}
    fn<void(__fastcall *)(Obj)>(0x251A10)(config);
    pending.force=force;pending.old=old;pending.state=state;pending.open=true;
    alignas(16) unsigned char local[0x40]={};
    *reinterpret_cast<void ***>(local)=callable_vtable;*reinterpret_cast<void (**)(Obj)>(local+8)=&picked;
    *reinterpret_cast<void **>(local+0x38)=local;
    Obj holder=dialog;
    fn<void(__fastcall *)(Obj,const char *,Obj *,void *)>(0x45ED60)(fn<Obj(__fastcall *)()>(0x110E0)(),
        "CDialogOpenStateBase<CDataSelectDlgBase<Data>>",&holder,local);
    log_event("lord_change_list_opened",count);
}
static void add_button(Obj bar) {
    // Mirrors 85FEF0/85FCE0 for text button id 0x18 (layout node 24 from the ScreenLayout patch).
    if(!bar || pointer(bar,0)!=image+kFooterVt)return;
    Obj table=bar+0x130;
    if(kButton>=static_cast<int>(dword(table,0x10))) {log_event("lord_change_no_layout_node");return;}
    if(fn<Obj(__fastcall *)(Obj,int)>(0x92CEE0)(table,kButton))return;
    Obj button=fn<Obj(__fastcall *)(Obj,int,int,int,int,int)>(0x92E430)(table,kButton,2,0,1,0);
    if(!button)return;
    reinterpret_cast<void(__fastcall *)(Obj,const wchar_t *)>(pointer(pointer(button,0),0x1D8))(button,L"반란");
    *reinterpret_cast<unsigned *>(button+0x84)=1;*reinterpret_cast<int *>(button+0x80)=kButton;
    alignas(16) unsigned char local[0x40]={};
    *reinterpret_cast<Obj *>(local)=image+kFooterFunctorVt;*reinterpret_cast<Obj *>(local+8)=bar;*reinterpret_cast<void **>(local+0x38)=local;
    fn<void(__fastcall *)(Obj,int,void *)>(0x1A3CB0)(button,0x7FFFFFFE,local);   // copies, then destroys local
    if(Obj control=fn<Obj(__fastcall *)(Obj,int)>(0x92CEE0)(table,kButton))fn<void(__fastcall *)(Obj,int,int)>(0x92F810)(control+0x130,0x11,0);
    fn<void(__fastcall *)(Obj,int,int)>(0x92F810)(table,0x15,1);
    // Stays enabled: 5035C0 enables 0x11/0x12 by the force at setup time, but a force may be chosen
    // later. The click checks the selected force and its ruler instead.
    log_event("lord_change_button_added");
}
static void __fastcall set_text(Obj bar,int index,const wchar_t *text,int flag) {   // only CALL 503C9C (휘하무장)
    fn<SetText>(kSetText)(bar,index,text,flag);
    if(mod_features.lord_change)add_button(bar);
}
static void __fastcall click(Obj state,Obj bar) {
    if(mod_features.lord_change && bar && dword(bar,0x80)==static_cast<unsigned>(kButton)) {pick(state);return;}
    fn<Click>(kClick)(state,bar);
}
// Map per-frame (fast_menu's state slot): rulers made here get an active status after the start.
static void map_tick() {
    LONG n=made_count;
    for(LONG i=0;i<n;++i) {
        Obj p=table(0x158,made[i],6000);if(!valid(p))continue;
        unsigned char status=p[0xE6];if(status!=9 && status!=4)continue;
        Obj force=fn<Obj(__fastcall *)(Obj)>(0x22CA00)(p);
        if(!valid(force) || word(force,0x10)!=made[i])continue;
        *reinterpret_cast<unsigned short *>(p+0xE6)=0x4901;
        log_event("lord_change_status_fixed",made[i],status);
    }
}
static CallLink link;
static bool install() {
    static const unsigned char before[]={0x41,0x8D,0x51,0x12,0x48,0x8B,0x8E,0xE0,0x04,0x00,0x00};   // lea edx,[r9+12h]; mov rcx,[rsi+4E0h]
    if(image[kLastButtonCall]!=0xE8 || kLastButtonCall+5+*reinterpret_cast<int *>(image+kLastButtonCall+1)!=kSetText ||
       memcmp(image+kLastButtonCall-sizeof(before),before,sizeof(before)) || pointer(image,kClickSlot)!=image+kClick ||
       pointer(image,kFooterFunctorVt)!=image+0x8607D0) {log_event("lord_change_guard_rejected");return false;}
    Obj relay=allocate_near(image+kLastButtonCall);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&set_text));
    DWORD old;
    if(!call_prepare(link,kLastButtonCall,relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,16)) {VirtualFree(relay,0,MEM_RELEASE);log_event("lord_change_prepare_rejected");return false;}
    if(!patch_pointer(kClickSlot,kClick,reinterpret_cast<void *>(&click)))return false;
    if(!call_publish(link)) {restore_pointer(kClickSlot,kClick,reinterpret_cast<void *>(&click));log_event("lord_change_publish_rejected");return false;}
    log_event("lord_change_installed");return true;
}
}
