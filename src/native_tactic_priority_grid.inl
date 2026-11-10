// Tactic priority grid popup + per-officer 「출진 기억」 toggle: BEHAVIOUR ONLY (docs/TACTIC_PRIORITY_V2_PLAN.md 2, 3, 8, 9, 12).
// Included by native_tactic_priority.inl inside namespace tactic_priority, after publish/catalog and the state helpers (record_list, current_record).
// UI ownership (user 2026-11-07, docs/UI_RESOURCE_LIFECYCLE.md 10): SAN14_Layout.dll creates, positions, shows and destroys every UI object
// (popup frame/tabs/tiles/buttons, toggle, footer button) through layout_client::ui_*; this file only supplies the descriptors (data),
// the labels, and what a click does. It never allocates, registers or destructs a game UI object. All calls run on the update-slot
// thread (grid::tick) or the leave hook; the original click callbacks only queue a node number.
//  Tiles are grouped (tp::build_tiles: unique / 아수라진.. / 분전.. / 매성.. / 철벽.. / 대갈.. / other, siege and naval tactics hidden), each group
//  starts a new row (tp::layout_groups). Mouse-over: the tactic description text is built here from the trait_hover column formatters
//  (docs/TRAIT_HOVER_TACTIC_TEXT.md, trait_hover_profile.h tables; feature depends on trait-hover, needs mod_features.tooltip_collab) and shown by
//  Layout (ui_tooltip, the balloon trait_hover builds). Live apply: every tile click publishes (units without 출진 기억 are recomputed).
//  Popup = Layout descriptor window (870410: type 8 frame, type 2 nodes turned into buttons by 92E430), activated with vt+0x38.
//  G2 (0086.c 868E00/8678B0/867C20/87A6D0): vt+0xD8 registers the child in the parent's pool, the parent destructor only orphans children;
//     our panes are therefore released by Layout BEFORE the original leave (ui_release: vt+0x40, vt+0x28(obj,next), vt[0](obj,1)).
//  Root cause of the local15 invisible toggle: the pane was never activated (vt+0x38 = 869210 registers it with the UI manager's lists).
namespace grid {
namespace {
const int kTabMartial=1,kTabIntellect=2,kHint=3,kTile0=4;
const int kGridTop=tp::kGridTopY,kButtonW=164,kButtonH=tp::kButtonRowH,kTabW=160,kTabH=64,kToggleW=200,kToggleH=64,kHintH=tp::kLineH,kButtonBottom=tp::kButtonBottomGap;
const int kImageId=0x2BA;   // plain panel image id the original descriptors use for size-only type 2 nodes (855EE0, 86E510)
const unsigned kTabNodeA=0x29,kTabNodeB=0x2B;   // original mode tabs of the sortie page (7D67E0)
const char *const kOwner="tactic_priority";
struct Desc {int type,x,y,w,h,r14,id,r1c;const wchar_t *text;int minus1,r2c;};
static_assert(sizeof(Desc)==0x30,"descriptor entry is 0x30 bytes");
Desc make_desc(int type,int x,int y,int w,int h,int id=0,const wchar_t *text=nullptr) {
    Desc d{};d.type=type;d.x=x;d.y=y;d.w=w;d.h=h;d.id=id;d.text=text;d.minus1=-1;return d;
}
struct Popup {
    Obj pane=nullptr,main=nullptr,tab_martial=nullptr,tab_intellect=nullptr;
    std::vector<Obj> tile;std::vector<int> ids;std::vector<unsigned> shown;
    int x=0,y=0,w=0,h=0;unsigned count=0,tiles=0;int officer=-1;   // officer: type of the current record's officer, 0 martial, 1 intellect, -1 none
    bool built=false,broken=false,open=false,dirty=false;
    int mode=0;tp::Settings draft;std::wstring before;Obj record=nullptr;
    unsigned page_visible[2]={0,0};
};
struct Toggle {Obj pane=nullptr,control=nullptr,tab=nullptr;bool built=false,flag=false;unsigned tries=0;unsigned person=0;int shown=-1;int enabled=-1;};
static Popup pop;
static Toggle tog;
static Obj grid_state;
static volatile bool open_requested;
static int pending[16];static unsigned pending_count;
static volatile LONG toggle_pending;
static unsigned cancel_guard,cancel_tail,page_check;
static bool fields_logged;
static wchar_t hint_text[96];
static std::wstring hover_text;static int hover_tile=-1;static bool hover_shown;static unsigned hover_logged;
enum Reason:unsigned {ByButton=1,ByCancel,ByRecord=4,ByLeave,ByFailure};
enum Skip:unsigned {SkipNoMain=1,SkipNoTabs,SkipNoUi,SkipNoRecord,SkipBuild};
void set_node_text(Obj layout,int node,const wchar_t *text) {layout_client::ui_node_text(layout,node,text);}
void set_button_text(Obj button,const wchar_t *text) {layout_client::ui_text(button,text);}
void set_visible(Obj layout,unsigned node,int on) {layout_client::ui_node_visible(layout,static_cast<int>(node),on);}
void show_object(Obj object,int on) {layout_client::ui_show(object,on);}
void select_radio(Obj radio,bool on) {layout_client::ui_select(radio,on?1:0);}
Obj node_control(Obj layout,unsigned node) {return static_cast<Obj>(layout_client::ui_node(layout,static_cast<int>(node)));}
// Original click callbacks (game thread): only remember which node was pressed (ctx = node number).
void __cdecl popup_clicked(void *,void *ctx) {
    if(pending_count<sizeof(pending)/sizeof(pending[0]))pending[pending_count++]=static_cast<int>(reinterpret_cast<intptr_t>(ctx));
}
void __cdecl toggle_clicked(void *,void *) {InterlockedExchange(&toggle_pending,1);}
Obj make_control(Obj pane,int node,int style,int kind,void (__cdecl *click)(void *,void *),void *ctx) {
    Obj control=static_cast<Obj>(layout_client::ui_button(pane,node,style,kind));
    if(control && click)layout_client::ui_click(control,click,ctx);
    return control;
}
// Visibility/alpha follow another object (Layout does the flag arithmetic); only asked when something differs.
void follow(Obj own,Obj source) {
    if(!own || !source)return;
    if(((dword(own,0x40)^dword(source,0x40))&0xBu)==0 && dword(own,0x30)==dword(source,0x30))return;
    layout_client::ui_follow(own,source);
}
std::vector<int> &ranked() {return pop.mode?pop.draft.intellect:pop.draft.martial;}
std::wstring tile_text(unsigned index) {
    const unsigned rank=tp::rank_of(ranked(),pop.ids[index]);
    std::wstring text=rank?std::to_wstring(rank)+L". ":std::wstring();
    return text+tactic_name(pop.ids[index]);
}
// Top hint node (type 1 at (377,18,w-416,28)): only "<type> N개 지정"; the rule text lives once, under the grid (user 2026-11-07).
void update_hint() {
    const wchar_t *type=pop.mode?L"지력형":L"무력형";
    wchar_t head[48];swprintf_s(head,L"%ls %u개 지정",type,static_cast<unsigned>(ranked().size()));
    wcsncpy_s(hint_text,head,_TRUNCATE);
    set_node_text(pop.pane,kHint,hint_text);
}
// Rewrites only the tiles whose rank changed (all of them after a mode change).
void refresh_tiles(bool force) {
    unsigned changed=0;
    for(unsigned i=0;i<pop.tiles;++i) {
        const unsigned rank=tp::rank_of(ranked(),pop.ids[i]);
        if(!force && rank==pop.shown[i])continue;
        pop.shown[i]=rank;set_button_text(pop.tile[i],tile_text(i).c_str());++changed;
    }
    update_hint();
    log_event("tactic_priority_tiles",changed,pop.mode,static_cast<unsigned>(ranked().size()));
}
void apply_mode_ids() {
    pop.ids=tp::tile_ids(tiles_for(pop.mode==0,catalog()));
}
void set_mode(int mode) {
    pop.mode=mode?1:0;apply_mode_ids();
    select_radio(pop.tab_martial,pop.mode==0);select_radio(pop.tab_intellect,pop.mode==1);
    refresh_tiles(true);
    log_event("tactic_priority_mode",pop.mode,static_cast<unsigned>(ranked().size()));
}
#ifndef NO_RUNTIME_LOG
// ---- diagnostics build only (user 2026-11-07: where static evidence is uncertain, the next in-game test settles it) ----
// Raw dword dumps. log_event prints unsigned decimals: army = (region<<16)|byte offset, value = dword at offset, hex = dword at offset+4.
// Regions: 1 state+0x470..0x4F0, 2 selected +0x4C0 row (0x94), 3 page(+0x490)+0x178..+0x20C, 4 *(state+0x488)+0x140..+0x150.
// Bounded: at most kProbeDumps dumps (about 58 lines each) per screen entry, one at entry and one per selection change (index or record pointer).
const unsigned kProbeDumps=10;
static unsigned probe_dumps;static int probe_index=-2;static Obj probe_record;
static bool probe_read(const void *from,unsigned *out,unsigned count) {
    __try {for(unsigned i=0;i<count;++i)out[i]=static_cast<const unsigned *>(from)[i];return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static void dump_region(unsigned region,const void *base,unsigned bytes) {
    if(!base)return;
    unsigned words[40];unsigned count=bytes/4;if(count>40)count=40;
    if(!probe_read(base,words,count)) {log_event("tactic_priority_dump_fault",region);return;}
    for(unsigned i=0;i<count;i+=2)log_event("tactic_priority_dump",(region<<16)|(i*4),words[i],i+1<count?words[i+1]:0);
}
static void probe_state(Obj state) {
    if(!diagnostic_logging)return;
    const int index=static_cast<int>(dword(state,0x4E4));
    Obj record=current_record(state);
    if(index==probe_index && record==probe_record)return;   // two integer compares per frame
    probe_index=index;probe_record=record;
    if(probe_dumps>=kProbeDumps)return;
    ++probe_dumps;
    log_event("tactic_priority_dump_begin",probe_dumps,static_cast<unsigned>(index),static_cast<unsigned>(reinterpret_cast<uintptr_t>(record)));
    dump_region(1,state+0x470,0x80);
    dump_region(2,record,0x94);
    if(Obj page=pointer(state,0x490))dump_region(3,page+0x178,0x94);
    if(Obj tabs=pointer(state,0x488))dump_region(4,tabs+0x140,0x10);
}
// Footer click (click thread, read only): everything open_popup looks at. bits: 1 main, 2 tabs, 4 +0x4B0, 8 current record, 16 broken, 32 open, 64 built, 128 state known.
static void probe_click() {
    if(!diagnostic_logging)return;
    Obj state=grid_state;
    unsigned bits=0;int index=-1;Obj record=nullptr;
    if(state) {
        bits=128|(pointer(state,0x470)?1u:0u)|(pointer(state,0x488)?2u:0u)|(pointer(state,0x4B0)?4u:0u);
        index=static_cast<int>(dword(state,0x4E4));record=current_record(state);
    }
    if(record)bits|=8;
    if(pop.broken)bits|=16;
    if(pop.open)bits|=32;
    if(pop.built)bits|=64;
    log_event("tactic_priority_click_probe",bits,static_cast<unsigned>(index),static_cast<unsigned>(reinterpret_cast<uintptr_t>(record)));
}
static void probe_toggle(Obj pane,Obj control,Obj parent) {
    if(!diagnostic_logging)return;
    log_event("tactic_priority_toggle_probe",1,dword(pane,0x30),dword(pane,0x40));                       // pane alpha, flags
    log_event("tactic_priority_toggle_probe",2,dword(pane,0x20),dword(pane,0x24));                       // pane position
    log_event("tactic_priority_toggle_probe",3,static_cast<unsigned>(reinterpret_cast<uintptr_t>(pointer(pane,8))),
              static_cast<unsigned>(reinterpret_cast<uintptr_t>(parent)));                                // pane parent field (+8) vs the main layout
    log_event("tactic_priority_toggle_probe",4,parent?dword(parent,0x30):0,parent?dword(parent,0x40):0);   // parent alpha, flags
    log_event("tactic_priority_toggle_probe",5,dword(control,0x20),dword(control,0x24));                 // control position
    log_event("tactic_priority_toggle_probe",6,dword(control,0x30),dword(control,0x40));                 // control alpha, flags
}
#else
static void probe_state(Obj) {}
static void probe_click() {}
static void probe_toggle(Obj,Obj,Obj) {}
#endif
// ---- mouse-over description (problem 6): text from the game's own tactic list columns, shown by Layout ----
bool tip_fields(Obj entry,const wchar_t *caption[thp::kTacticFields],const wchar_t *value[thp::kTacticFields]) {
    __try {
        for(unsigned k=0;k<thp::kTacticFields;++k) {
            caption[k]=fn<const wchar_t *(__fastcall *)(unsigned)>(GA(GA_30F010))(thp::tactic_caption_id(k));
            value[k]=fn<const wchar_t *(__fastcall *)(Obj)>(thp::tactic_func_rva(k))(entry);
        }
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
std::wstring tile_tip(int id) {
    if(id==tp::kUnique || id<1 || id>200)return std::wstring();
    Obj entry=table(0x774D0,static_cast<unsigned>(id),200);
    if(!valid(entry))return std::wstring();
    const wchar_t *caption[thp::kTacticFields]={},*value[thp::kTacticFields]={};
    if(!tip_fields(entry,caption,value))return std::wstring();
    return thp::compose_tactic_text(caption,value);
}
bool mouse_position(int &px,int &py) {
    __try {
        Obj input=fn<Obj(__fastcall *)()>(GA(GA_111F0))();
        if(!input)return false;
        px=*reinterpret_cast<int *>(input+0x34);py=*reinterpret_cast<int *>(input+0x38);
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
thp::WidgetView view_of(const void *w) {
    const unsigned char *p=static_cast<const unsigned char *>(w);
    thp::WidgetView v;
    v.width=*reinterpret_cast<const int *>(p+0x28);v.height=*reinterpret_cast<const int *>(p+0x2C);
    v.flags=*reinterpret_cast<const unsigned *>(p+0x40);
    v.ox=*reinterpret_cast<const int *>(p+0x20);v.oy=*reinterpret_cast<const int *>(p+0x24);
    v.parent=*reinterpret_cast<void *const *>(p+8);
    return v;
}
int tile_under(int px,int py) {
    for(unsigned i=0;i<pop.tiles;++i)
        if(pop.tile[i] && thp::contains(pop.tile[i],px,py,false,view_of))return static_cast<int>(i);
    return -1;
}
// Called every frame while the popup is open (inside the Layout scope). Hit tests only when the mouse moved.
void hover_tick() {
    if(!mod_features.tooltip_collab || !pop.built)return;   // trait-hover feature (tooltip_collab) off: no tooltip, nothing else changes
    static int last_x=-1,last_y=-1;
    int px=0,py=0;
    if(!mouse_position(px,py))return;
    if(px!=last_x || py!=last_y || (hover_tile<0 && hover_shown)) {
        last_x=px;last_y=py;
        const int tile=tile_under(px,py);
        if(tile!=hover_tile) {
            hover_tile=tile;
            hover_text.clear();
            if(tile>=0)hover_text=tile_tip(pop.ids[static_cast<unsigned>(tile)]);
            if(hover_logged<8 && tile>=0) {
                ++hover_logged;
                log_event("tactic_priority_tooltip",1,static_cast<unsigned>(pop.ids[static_cast<unsigned>(tile)]),static_cast<unsigned>(hover_text.size()));   // stage 1: text length (0 = none)
            }
        }
    }
    if(hover_tile>=0 && !hover_text.empty()) {
        const int r=layout_client::ui_tooltip(hover_text.c_str());
        if(r<0 && hover_logged<16) {++hover_logged;log_event("tactic_priority_tooltip",2,0,0);}
        hover_shown=r>=0;
    }
    else if(hover_shown) {layout_client::ui_tooltip(nullptr);hover_shown=false;}
}
bool build_popup(Obj state) {
    COST_SCOPE("tactic_priority_popup_build");
    LARGE_INTEGER begin,end,freq;QueryPerformanceCounter(&begin);
    Obj main=pointer(state,0x470);
    if(!main)return false;
    // Anchor: the right window rectangle. 1) page 0x490 position 2) left tab minus its frame margin 3) screen centre.
    int x=0,y=0,source=3;
    Obj page=pointer(state,0x490),tab=node_control(main,kTabNodeA);
    auto sane=[](int a,int b) {return a>=0 && a<=1920 && b>=0 && b<=1080;};
    if(page && sane(static_cast<int>(dword(page,0x20)),static_cast<int>(dword(page,0x24)))) {x=static_cast<int>(dword(page,0x20));y=static_cast<int>(dword(page,0x24));source=1;}
    else if(tab && sane(static_cast<int>(dword(tab,0x20))-tp::kMargin,static_cast<int>(dword(tab,0x24)))) {x=static_cast<int>(dword(tab,0x20))-tp::kMargin;y=static_cast<int>(dword(tab,0x24));source=2;}
    log_event("tactic_priority_popup_anchor",source,static_cast<unsigned>(x),static_cast<unsigned>(y));
    // Original right window width: ~1094 when it starts near x=786, ~954 near x=835.
    const int base_w=(source==3 || x<810)?1094:954;
    pop.mode=0;
    const std::vector<tp::Tile> tile_list=tiles_for(true,catalog());
    pop.ids=tp::tile_ids(tile_list);
    std::vector<unsigned> groups;for(const auto &t:tile_list)groups.push_back(t.group);
    const unsigned tiles=static_cast<unsigned>(pop.ids.size());
    const tp::Grid layout=tp::layout_groups(groups,base_w);
    const int w=layout.w;
    const int frame_h=tp::frame_height(layout.h);   // grows with the grid; the original window height is the minimum
    if(frame_h>tp::kFrameMinH)log_event("tactic_priority_grid_overflow",static_cast<unsigned>(layout.h),tiles,static_cast<unsigned>(frame_h));
    if(source==3){x=(1920-w)/2;y=(1080-frame_h)/2;}
    if(x<0)x=0;
    if(y+frame_h>1080)y=(std::max)(0,1080-frame_h);
    log_event("tactic_priority_popup_rect",static_cast<unsigned>(x),static_cast<unsigned>(y),(static_cast<unsigned>(w)<<16)|static_cast<unsigned>(frame_h));
    std::vector<Desc> desc;
    desc.push_back(make_desc(8,0,0,w,frame_h));
    desc.push_back(make_desc(2,tp::kMargin,0,kTabW,kTabH,kImageId));
    desc.push_back(make_desc(2,tp::kMargin+154,0,kTabW,kTabH,kImageId));
    desc.push_back(make_desc(1,377,18,w-416,28,0,L" "));
    for(const auto &cell:layout.cells)desc.push_back(make_desc(2,cell.x,kGridTop+cell.y,layout.tile_w,layout.tile_h,kImageId));
    const int button_y=frame_h-kButtonH-kButtonBottom,close_x=w-tp::kMargin-kButtonW;
    desc.push_back(make_desc(2,close_x-2*(kButtonW+12),button_y,kButtonW,kButtonH,kImageId));
    desc.push_back(make_desc(2,close_x-(kButtonW+12),button_y,kButtonW,kButtonH,kImageId));
    desc.push_back(make_desc(2,close_x,button_y,kButtonW,kButtonH,kImageId));
    const int kReset=kTile0+static_cast<int>(tiles),kDefaults=kReset+1,kClose=kReset+2,kRule=kReset+3,kPolicy=kReset+4;
    const int hint_y=kGridTop+layout.h+tp::kBlockGap;   // two full-width lines under the grid, above the button row (everything fits at once, no paging)
    desc.push_back(make_desc(1,tp::kMargin,hint_y,w-2*tp::kMargin,kHintH,0,L" "));
    desc.push_back(make_desc(1,tp::kMargin,hint_y+kHintH+tp::kLineGap,w-2*tp::kMargin,kHintH,0,L" "));
    if(hint_y+2*kHintH+tp::kLineGap>button_y)log_event("tactic_priority_hint_overlap",static_cast<unsigned>(hint_y),static_cast<unsigned>(button_y),0);
    pop.count=static_cast<unsigned>(desc.size());pop.tiles=tiles;
    log_event("tactic_priority_popup_build",1,pop.count,tiles?1u:0u);
    Obj pane=static_cast<Obj>(layout_client::ui_window(main,x,y,w,frame_h,desc.data(),static_cast<int>(desc.size())));
    log_event("tactic_priority_popup_build",2,pop.count,pane?1u:0u);
    if(!pane)return false;
    pop.pane=pane;   // owned and released by Layout (ui_release)
    pop.tab_martial=make_control(pane,kTabMartial,9,1,&popup_clicked,reinterpret_cast<void *>(static_cast<intptr_t>(kTabMartial)));
    pop.tab_intellect=make_control(pane,kTabIntellect,10,1,&popup_clicked,reinterpret_cast<void *>(static_cast<intptr_t>(kTabIntellect)));
    log_event("tactic_priority_popup_build",3,pop.count,pop.tab_martial && pop.tab_intellect?1u:0u);
    if(!pop.tab_martial || !pop.tab_intellect)return false;
    set_node_text(pane,kTabMartial,L"무력형");set_node_text(pane,kTabIntellect,L"지력형");
    const int line_w=w-2*tp::kMargin;
    set_node_text(pane,kRule,tp::fit_hint({L"무력≥지력→무력형 / 지력>무력→지력형 · 누른 순서가 우선순위, 다시 누르면 해제",L"무력≥지력→무력형 / 지력>무력→지력형 · 누른 순서가 우선순위",L"누른 순서가 우선순위"},line_w).c_str());
    set_node_text(pane,kPolicy,tp::fit_hint({L"지정한 전법만 순서대로 최대 3개, 지정 없으면 원작대로 · 출진 기억 켠 무장은 기억한 진형·전법 그대로",L"지정한 전법만 순서대로 최대 3개, 지정 없으면 원작대로"},line_w).c_str());
    pop.tile.assign(tiles,nullptr);pop.shown.assign(tiles,~0u);
    bool tiles_ok=true;
    for(unsigned i=0;i<tiles;++i) {
        const int node=kTile0+static_cast<int>(i);
        Obj button=make_control(pane,node,3,0,&popup_clicked,reinterpret_cast<void *>(static_cast<intptr_t>(node)));
        if(!button){tiles_ok=false;break;}
        pop.tile[i]=button;
    }
    log_event("tactic_priority_popup_build",4,tiles,tiles_ok?1u:0u);
    if(!tiles_ok)return false;
    const struct {int node;const wchar_t *text;} buttons[]={{kReset,L"초기화"},{kDefaults,L"기본값"},{kClose,L"닫기"}};
    bool buttons_ok=true;
    for(const auto &b:buttons) {
        Obj button=make_control(pane,b.node,3,0,&popup_clicked,reinterpret_cast<void *>(static_cast<intptr_t>(b.node)));
        if(!button){buttons_ok=false;break;}
        set_button_text(button,b.text);
    }
    log_event("tactic_priority_popup_build",5,3,buttons_ok?1u:0u);
    if(!buttons_ok)return false;
    // vt+0x38: register with the UI manager (every native layout does this once its controls exist); then visible like the main layout.
    // b0f820e (local16): activate outside log_event; release builds drop log arguments, so the call never ran.
    const int activated=layout_client::ui_activate(pane);
    log_event("tactic_priority_popup_build",7,static_cast<unsigned>(activated),dword(pane,0x40));
    follow(pane,main);
    pop.main=main;pop.x=x;pop.y=y;pop.w=w;pop.h=frame_h;pop.built=true;
    QueryPerformanceCounter(&end);QueryPerformanceFrequency(&freq);
    log_event("tactic_priority_popup_build",6,pop.count,static_cast<unsigned>((end.QuadPart-begin.QuadPart)*1000000/freq.QuadPart));   // microseconds
    log_event("tactic_priority_popup_vis",0,dword(pane,0x30),dword(pane,0x40));
    return true;
}
bool save_draft() {
    wchar_t path[MAX_PATH];
    if(!mod_ini_path(path,L"기능설정.ini"))return false;
    return tactic_priority_io::save_profile(path,pop.before,tactic_priority_io::to_wide(tp::encode(pop.draft)));
}
void hide_originals(Obj state) {
    Obj main=pointer(state,0x470);
    set_visible(main,kTabNodeA,0);set_visible(main,kTabNodeB,0);
    if(tog.built)show_object(tog.pane,0);
    for(unsigned i=0;i<2;++i)if(Obj page=pointer(state,i?0x498:0x490)) {
        pop.page_visible[i]=dword(page,0x40)&1;
        show_object(page,0);   // vt+0xC8 and 8741D0 are the same function (876580)
    }
}
void skipped(unsigned reason,unsigned a=0,unsigned b=0) {log_event("tactic_priority_open_skipped",reason,a,b);}
void open_popup(Obj state) {
    if(pop.broken || pop.open)return;
    // +0x4B0 is NOT a modal flag (CStrategyUnitMakeState+0x4B0 is a persistent confirm dialog the leave destroys, 7DC200; the original update
    // only returns while it is the active layout, 7E18E0 -> 870ED0). It is logged but never gates the popup.
    Obj main=pointer(state,0x470),tabs=pointer(state,0x488);
    if(!main) {skipped(SkipNoMain);return;}
    if(!tabs) {skipped(SkipNoTabs);return;}
    Obj record=current_record(state);
    if(!record) {skipped(SkipNoRecord,dword(state,0x4E4));return;}
    layout_client::UiScope scope(kOwner);
    if(!scope.ok) {skipped(SkipNoUi);return;}
    if(!pop.built) {
        if(!build_popup(state)) {pop.broken=true;skipped(SkipBuild,pop.count);log_event("tactic_priority_popup_broken",pop.count,0,0);return;}
    }
    AcquireSRWLockShared(&settings_lock);pop.draft=settings;ReleaseSRWLockShared(&settings_lock);
    {   // siege and naval ids are not ranked any more: strip them from an older profile (saved with the next change)
        const auto ids=catalog();
        const size_t before_m=pop.draft.martial.size(),before_i=pop.draft.intellect.size();
        pop.draft.martial=visible_ranked(pop.draft.martial,true,ids);pop.draft.intellect=visible_ranked(pop.draft.intellect,false,ids);
        pop.dirty=before_m!=pop.draft.martial.size() || before_i!=pop.draft.intellect.size();
    }
    wchar_t path[MAX_PATH];pop.before=mod_ini_path(path,L"기능설정.ini")?tactic_priority_io::read_profile(path):std::wstring();
    pop.record=record;
    int mode=0;
    Obj person=table(0x158,record_person(record),6000);
    pop.officer=-1;
    if(valid(person)) {mode=tp::martial(fn<int(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_231770))(person,1,0xFFF),
                                        fn<int(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_231770))(person,2,0xFFF))?0:1;pop.officer=mode;}
    hide_originals(state);
    show_object(pop.pane,1);follow(pop.pane,main);
    log_event("tactic_priority_popup_vis",1,dword(pop.pane,0x30),dword(pop.pane,0x40));
    log_event("tactic_priority_popup_vis",2,dword(main,0x30),dword(main,0x40));
    set_mode(mode);
    pop.open=true;
    log_event("tactic_priority_popup_open",static_cast<unsigned>(mode),static_cast<unsigned>(ranked().size()));
}
void close_popup(Obj state,unsigned reason) {
    if(!pop.open)return;
    pop.open=false;
    if(pop.dirty) {
        if(save_draft())log_event("tactic_priority_saved",generation);
        else log_event("tactic_priority_save_failed",reason);
        pop.dirty=false;
    }
    log_event("tactic_priority_popup_close",reason);
    if(hover_shown) {layout_client::ui_tooltip(nullptr);hover_shown=false;hover_tile=-1;}
    if(pop.pane)show_object(pop.pane,0);
    Obj main=pointer(state,0x470);
    if(main && reason!=ByLeave) {
        set_visible(main,kTabNodeA,1);set_visible(main,kTabNodeB,1);
        if(tog.built)show_object(tog.pane,1);
        refresh_tabs(state,2);
        page_check=2;
    }
    if(reason==ByCancel)cancel_guard=1;
}
void handle_click(Obj state,int node) {
    COST_SCOPE("tactic_priority_click");
    if(!pop.open) {log_event("tactic_priority_leak",1,static_cast<unsigned>(node),0);return;}
    const int reset_node=kTile0+static_cast<int>(pop.tiles),defaults_node=reset_node+1,close_node=reset_node+2;
    if(node==kTabMartial || node==kTabIntellect) {set_mode(node==kTabIntellect);return;}
    if(node>=kTile0 && node<reset_node) {
        const unsigned index=static_cast<unsigned>(node-kTile0);
        if(!tp::toggle_rank(ranked(),pop.ids[index],pop.ids))return;
        log_event("tactic_priority_tile",static_cast<unsigned>(pop.ids[index]),tp::rank_of(ranked(),pop.ids[index]),pop.mode);
    }
    else if(node==reset_node)tp::clear(ranked());
    else if(node==defaults_node)ranked()=visible_ranked(tp::preset(pop.mode==0,catalog()),pop.mode==0,catalog());
    else if(node==close_node) {close_popup(state,ByButton);return;}
    else return;
    pop.dirty=true;refresh_tiles(false);publish(pop.draft);   // live: the apply loop recomputes units without the remember flag (tactic_priority_live_apply)
}
// Remember toggle: one-node Layout pane right of the second tab, a radio (style 10) following the tab visibility.
void build_toggle(Obj state) {
    COST_SCOPE("tactic_priority_toggle_sync");
    Obj main=pointer(state,0x470);
    if(!main || ++tog.tries>600)return;
    Obj tab=node_control(main,kTabNodeB);
    if(!tab)return;
    const int x=static_cast<int>(dword(tab,0x20))+154,y=static_cast<int>(dword(tab,0x24));
    if(x<0 || x>1920 || y<0 || y>1080) {tog.tries=601;log_event("tactic_priority_toggle_built",0,static_cast<unsigned>(x),static_cast<unsigned>(y));return;}
    layout_client::UiScope scope(kOwner);
    if(!scope.ok) {tog.tries=601;log_event("tactic_priority_toggle_built",0,3,0);return;}
    std::vector<Desc> desc{make_desc(2,0,0,kToggleW,kToggleH,kImageId)};
    Obj pane=static_cast<Obj>(layout_client::ui_window(main,x,y,kToggleW,kToggleH,desc.data(),1));
    if(!pane) {tog.tries=601;log_event("tactic_priority_toggle_built",0,1,0);return;}
    Obj control=make_control(pane,0,10,1,&toggle_clicked,nullptr);
    if(!control) {   // never leave a half-built pane: Layout removes it with the original sequence at leave
        tog.pane=pane;tog.tries=601;log_event("tactic_priority_toggle_built",0,2,0);return;
    }
    const int activated=layout_client::ui_activate(pane);
    follow(pane,tab);
    tog.pane=pane;tog.control=control;tog.tab=tab;tog.built=true;tog.shown=-1;
    log_event("tactic_priority_toggle_built",1,static_cast<unsigned>(x),static_cast<unsigned>(y));
    log_event("tactic_priority_toggle_vis",dword(pane,0x40),dword(control,0x40),dword(tab,0x40));   // pane/control/tab visibility bits after build
    log_event("tactic_priority_toggle_vis",0x100u|static_cast<unsigned>(activated&0xFF),dword(pane,0x30),GetCurrentThreadId());
    probe_toggle(pane,control,main);
}
// Wanted label state from the current record (pure reads): 2 = no officer, 1 = flag on, 0 = flag off.
void sync_toggle(Obj state,bool &work) {
    if(!tog.built)return;
    Obj tab=tog.tab;
    if(tab && (((dword(tog.pane,0x40)^dword(tab,0x40))&0xBu) || dword(tog.pane,0x30)!=dword(tab,0x30)))work=true;
    Obj record=current_record(state);
    const unsigned person=record?record_person(record):0;
    const bool flag=person && person<tactic_memory::MaxPeople && tactic_memory_flag(person);
    const int want=person?(flag?1:0):2;
    if(want!=tog.shown)work=true;
    else if(person && ((dword(tog.control,0x198)!=0)!=flag))work=true;
}
void apply_toggle(Obj state) {
    follow(tog.pane,tog.tab);
    Obj record=current_record(state);
    const unsigned person=record?record_person(record):0;
    const bool flag=person && person<tactic_memory::MaxPeople && tactic_memory_flag(person);
    const int want=person?(flag?1:0):2;   // 2 = no officer
    if(want!=tog.shown) {
        tog.shown=want;tog.person=person;tog.flag=flag;
        set_node_text(tog.pane,0,want==2?L"출진 기억":(flag?L"출진 기억 켬":L"출진 기억 끔"));
        select_radio(tog.control,flag);
        log_event("tactic_priority_toggle_sync",person,static_cast<unsigned>(want),0);
    }
    const int usable=person?1:0;   // no officer in the record (empty unit, add slot, no units): dimmed and not clickable
    if(usable!=tog.enabled) {
        tog.enabled=usable;
        const int r=layout_client::ui_enable(tog.control,usable);
        log_event("tactic_priority_toggle_enable",static_cast<unsigned>(usable),r<0?0u:1u,dword(tog.control,0x40));
    }
    else if(person && ((dword(tog.control,0x198)!=0)!=flag))select_radio(tog.control,flag);   // the radio may flip itself on a click
}
void run_toggle(Obj state) {
    Obj record=current_record(state);
    const unsigned person=record?record_person(record):0;
    if(!person || person>=tactic_memory::MaxPeople)return;
    const bool on=!tactic_memory_flag(person);
    tactic_memory_set_flag(person,on);
    log_event("tactic_priority_toggle",person,on?1u:0u,0);
    if(on)forget(record);   // next apply: restore the remembered formation and tactics at once
    tog.shown=-1;
}
void reset_all() {
    pop=Popup();tog=Toggle();hover_text.clear();hover_tile=-1;hover_shown=false;hover_logged=0;open_requested=false;pending_count=0;toggle_pending=0;cancel_guard=0;cancel_tail=0;page_check=0;fields_logged=false;
#ifndef NO_RUNTIME_LOG
    probe_dumps=0;probe_index=-2;probe_record=nullptr;
#endif
}
// Once per screen entry: which original objects exist and the selection index (diagnostic for the next in-game test).
void log_fields(Obj state) {
    Obj main=pointer(state,0x470),tabs=pointer(state,0x488),modal=pointer(state,0x4B0),page=pointer(state,0x490);
    uintptr_t first=0,last=0;const bool listed=record_list(state,first,last);
    log_event("tactic_priority_state_fields",(main?1u:0u)|(tabs?2u:0u)|(modal?4u:0u)|(page?8u:0u)|(listed?16u:0u),dword(state,0x4E4),
              listed?static_cast<unsigned>((last-first)/8):0u);
    log_event("tactic_priority_state_flags",main?dword(main,0x40):0,modal?dword(modal,0x40):0,page?dword(page,0x40):0);
    fields_logged=true;
}
// Update slot of the sortie screen. Returns true when the original update must be skipped this frame (cancel already consumed).
bool tick(Obj state) {
    if(grid_state!=state) {reset_all();grid_state=state;}
    if(!fields_logged)log_fields(state);
    probe_state(state);
    bool work=false;
    if(!tog.built && tog.tries<=600)build_toggle(state);
    sync_toggle(state,work);
    if(work || toggle_pending || open_requested || pending_count || pop.open || page_check) {
        layout_client::UiScope scope(kOwner);   // opened only when there is UI work: the closed idle frame touches nothing
        if(scope.ok) {
            if(tog.built && work)apply_toggle(state);
            if(toggle_pending) {InterlockedExchange(&toggle_pending,0);run_toggle(state);}
            if(open_requested) {open_requested=false;open_popup(state);}
            if(pending_count) {
                int nodes[16];const unsigned count=pending_count;memcpy(nodes,pending,sizeof(int)*count);pending_count=0;
                for(unsigned i=0;i<count && pop.open;++i)handle_click(state,nodes[i]);
            }
            if(pop.open) {
                follow(pop.pane,pop.main);
                hover_tick();
                if(current_record(state)!=pop.record) close_popup(state,ByRecord);
                else if(fn<unsigned char(__fastcall *)()>(GA(GA_3F6150))()) {close_popup(state,ByCancel);return true;}
                else if(Obj tab=node_control(pop.main,kTabNodeA)) {
                    if(dword(tab,0x40)&1) {log_event("tactic_priority_leak",2,0,0);set_visible(pop.main,kTabNodeA,0);set_visible(pop.main,kTabNodeB,0);}
                }
            }
            if(page_check && !pop.open && --page_check==0) {
                for(unsigned i=0;i<2;++i)if(Obj page=pointer(state,i?0x498:0x490)) {
                    if(pop.page_visible[i] && !(dword(page,0x40)&1)) {
                        show_object(page,1);
                        log_event("tactic_priority_page_restore",i,0,0);
                    }
                }
            }
        }
        else {open_requested=false;pending_count=0;toggle_pending=0;}   // owner not published: the entry points do nothing
    }
    if(!pop.open && cancel_guard) {
        // The original update would read the same cancel and leave the screen: keep skipping while it stays pressed (at most 30 frames).
        if(cancel_guard<=30 && fn<unsigned char(__fastcall *)()>(GA(GA_3F6150))()) {++cancel_guard;return true;}
        // One more skipped frame after the cancel is released: the original update may read the edge one frame later than the query does.
        if(!cancel_tail) {cancel_tail=1;return true;}
        log_event("tactic_priority_cancel_frames",cancel_guard,cancel_tail);cancel_guard=0;cancel_tail=0;
    }
    return false;
}
// Leave hook (before the original leave): save, then Layout releases popup and toggle with the original destroy sequence while the parent
// (main layout) still exists, so nothing is orphaned or left in the UI manager's lists. `next` = leave's second argument (7DC200 param_2).
void leave(Obj state,Obj next) {
    if(grid_state!=state)return;
    if(pop.open) {layout_client::UiScope scope(kOwner);close_popup(state,ByLeave);}
    {
        layout_client::UiScope scope(kOwner);
        const int released=scope.ok?layout_client::ui_release(kOwner,next):-1;
        log_event("tactic_priority_popup_destroyed",static_cast<unsigned>(released),pop.built?1u:0u,tog.built?1u:0u);
    }
    reset_all();grid_state=nullptr;
}
}   // namespace
}   // namespace grid
