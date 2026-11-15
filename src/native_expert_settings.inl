#include "game_addresses.h"
/* CSetUpScenarioSeniorLayout only: the scenario's expert settings dialog.
 * Current values remain in its native option object and follow the native
 * commit/cancel and save/load copies. No file IO occurs in these callbacks.
 * Evidence and native ABI: research/expert_settings/implementation.md. */
#include <array>
#include <map>
struct ExpertRow { alignas(8) unsigned char bytes[0x60]; };
static std::map<Obj,std::array<ExpertRow,4>> expert_rows;
struct ExpertChoices {int selected[2]={-1,-1};};
struct ExpertPresetPanel {Obj layout=nullptr,button=nullptr;};
static std::map<Obj,ExpertChoices> expert_choices;
static std::map<Obj,ExpertPresetPanel> expert_preset_panels;
static constexpr unsigned expert_choice_key=0x70000000u;
static __declspec(thread) Obj expert_binding_layout;
static __declspec(thread) unsigned expert_binding_depth;
static __declspec(thread) Obj expert_popup_layout;
static __declspec(thread) unsigned expert_popup_id;
using ExpertPopulate=void(__fastcall *)(Obj,Obj,Obj,Obj);
static ExpertPopulate original_expert_populate;
// Layout manager (design 8.0 G1): the next function of each registered hook. Routed builds always have it set by the
// manager; split fixtures (no Layout) leave it null and the original RVA is used, as before.
static void *next_expert_destroy,*next_expert_list_bind,*next_expert_row_destroy,*next_expert_row_leave,*next_expert_slider,
    *next_expert_choice,*next_expert_number_event,*next_expert_number_confirm,*next_expert_number_current,*next_expert_number_factory;
static void *next_expert_populate[3];
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
namespace briefing_overlay { static bool available(); static void toast(const std::wstring &); }
#endif
template<class T> static T expert_next(void *next,unsigned rva) {return next?reinterpret_cast<T>(next):fn<T>(rva);}
// local101 (user 2026-11-13): 설정3 gets two rows of the mod's own battle rules. 63 = 연계 거리 (shown in the user's
// count, own hex included: 3..5 = distance 2..4), 64 = 안행 사거리 (2..3). They are not option-object keys.
// local105 (user 2026-11-13 "왜 기능설정 ini에 저장하는거야? 세이브 파일에 저장해야지" + "결정 버튼 눌러야 바뀌게"): the values
// belong to the save (sidecar <save>.<sha256>.ranges, native_expert_links.inl); a load without that sidecar and a fresh
// start use the original 2/2. A new-game dialog writes the live value at once (the game starts from it); an in-game
// view (normal or senior, expert_ingame) only keeps a pending value, which its 확인 button applies; 뒤로 drops it.
namespace range_rules {
static volatile LONG link_range=2,wedge_range=2;
static LONG pending[2]={0,0};   // in-game view: pending link (2..4) / wedge (2..3), 0 = none
static void clear_pending() {pending[0]=pending[1]=0;}
static bool commit_pending() {
    bool changed=false;
    if(pending[0] && InterlockedExchange(&link_range,pending[0])!=pending[0])changed=true;
    if(pending[1] && InterlockedExchange(&wedge_range,pending[1])!=pending[1])changed=true;
    log_event("range_rules_commit",static_cast<unsigned>(link_range),static_cast<unsigned>(wedge_range),changed?1u:0u);
    clear_pending();return changed;
}
}
static bool expert_range_id(unsigned id) {return id==63 || id==64;}
static bool expert_setting_id(unsigned id) {return id==4 || id==5 || id==61 || id==62 || expert_range_id(id);}
static unsigned expert_range_value(unsigned id) {
    const LONG p=range_rules::pending[id==63?0:1];
    const LONG v=p?p:(id==63?range_rules::link_range:range_rules::wedge_range);
    return id==63?static_cast<unsigned>(v)+1:static_cast<unsigned>(v);
}
static unsigned expert_range_standard(unsigned id) {return id==63?3u:2u;}
static bool expert_range_store(unsigned id,int value,bool pending=false) {
    const LONG v=id==63?(std::max)(3,(std::min)(5,value))-1:(std::max)(2,(std::min)(3,value));
    if(pending) {
        LONG &slot=range_rules::pending[id==63?0:1];
        if(slot!=v) {slot=v;log_event("expert_range_pending",id,expert_range_value(id),0);}
        return true;
    }
    volatile LONG &live=id==63?range_rules::link_range:range_rules::wedge_range;
    if(InterlockedExchange(&live,v)!=v)log_event("expert_range_set",id,expert_range_value(id),0);
    return true;
}
// local102 (user 2026-11-13 "슬라이더가 범위가 이상하네 최대로 해도 쪼끔밖에 안움직여"): the borrowed native bar runs 0..200,
// so the bar position is spread over it: 연계 거리 3/4/5 = 0/100/200, 안행 사거리 2/3 = 0/200. The number box shows the
// real value; a typed number inside the real range is taken as it is, anything else as a bar position.
static unsigned expert_range_position(unsigned id) {
    return id==63?(expert_range_value(id)-3)*100:(expert_range_value(id)-2)*200;
}
static int expert_range_from_position(unsigned id,int position) {
    return id==63?(position<67?3:position<134?4:5):(position<100?2:3);
}
static int expert_range_from_number(unsigned id,int number) {
    const int low=id==63?3:2,high=id==63?5:3;
    return number>=low && number<=high?number:expert_range_from_position(id,number);
}
static bool expert_base_id(unsigned id) {return id==61 || id==62;}
static unsigned expert_force_key(unsigned id) {return id==4 || id==61?4:5;}
// User 2026-11-09: in the in-game settings view (system menu, layout mode 1)
// only the four rows above stay editable while expert_ingame is on. An accepted
// change also writes the live option object that combat and the game's own save
// path use (image+GA(GA_1A49C98)); no other key and no other byte is touched.
static bool expert_in_game(Obj layout) {return mod_features.expert_ingame && dword(layout,0x19C)==1;}
#ifndef NO_RUNTIME_LOG
static unsigned expert_ingame_writes;
#endif
static bool expert_write_ui(Obj layout,unsigned id,int value,unsigned encoded) {
    // PW30 (user 2026-11-09): every edit changes only the view copy. 확인 copies the two option dwords to the live
    // object (expert_ingame_commit); 뒤로 therefore leaves the live settings untouched.
    *reinterpret_cast<unsigned *>(expert_option(layout+0x1C8,expert_force_key(id)))=encoded;
#ifndef NO_RUNTIME_LOG
    if(expert_in_game(layout) && expert_ingame_writes<100) {
        ++expert_ingame_writes;
        log_event("expert_ingame_write",id,static_cast<unsigned>(value),encoded);
    }
#else
    (void)value;
#endif
    return true;
}
static const wchar_t *expert_tooltip(unsigned id) {
    switch(id) {
    case 4:return L"완전 점령 지역 1개당 공/방 증가율. 범위 0~200%.\n"
        L"자세력 기본값은 모든 난이도에서 동일합니다.\n"
        L"소 4% / 중 25% / 대 46%. 오른쪽 버튼으로 고른 뒤 표준으로 적용.";
    case 5:return L"원작 타세력(소/중/대): 초급~상급 4% / 25% / 46%\n"
        L"특급 16% / 39.2% / 62.5% · 궁극 24% / 49% / 74%\n"
        L"오른쪽 버튼으로 고른 뒤 표준으로 적용. 컴퓨터끼리는 4% 고정.";
    case 61:return L"자세력 기본 공/방: 50=0.5배, 100=1배, 200=2배.\n"
        L"공/방 = 원래 공/방 × (기본 설정 + 인접 설정 × 점령 수) ÷ 100\n"
        L"범위 50~200, 표준 100. 원작 인접효과 기본값은 아래 항목 참고.";
    case 62:return L"플레이어와 교전하는 타세력의 기본 공/방 배율입니다.\n"
        L"공/방 = 원래 공/방 × (기본 설정 + 인접 설정 × 점령 수) ÷ 100\n"
        L"범위 50~200, 표준 100=1배. 컴퓨터끼리는 기본 100·인접 4 고정.";
    case 63:return L"친애·의형제 등 연계 효과를 주고받는 거리입니다. 자기 칸을 포함해 셉니다.\n"
        L"3헥스 = 원작(2칸 떨어진 곳까지), 5헥스 = 4칸 떨어진 곳까지. 범위 3~5, 표준 3.\n"
        L"플레이어와 컴퓨터 모두에 적용되며, 바꾸는 즉시 저장됩니다.";
    case 64:return L"안행진의 공격 사거리입니다. 2 = 원작, 3 = 한 칸 더 멀리서 교전·전법 사용.\n"
        L"범위 2~3, 표준 2. 이동 명령은 지금처럼 2칸까지 다가갑니다.\n"
        L"플레이어와 컴퓨터 모두에 적용되며, 바꾸는 즉시 저장됩니다.";
    default:return nullptr;
    }
}
static bool expert_layout_valid(Obj layout) {
    return layout && pointer(layout,0)==image+GA(GA_141EF18);
}
static expert_combat::Settings expert_ui_settings(Obj layout,unsigned id) {
    return expert_combat::decode(dword(expert_option(layout+0x1C8,expert_force_key(id)),0));
}
static bool expert_ui_store(Obj layout,unsigned id,int value) {
    if(expert_range_id(id))return expert_layout_valid(layout) && expert_range_store(id,value,expert_in_game(layout));
    if(!expert_layout_valid(layout) || !expert_setting_id(id) || value<0 || value>200 ||
       (expert_base_id(id) && value<50))return false;
    auto settings=expert_ui_settings(layout,id);
    if(expert_base_id(id))settings.base=static_cast<unsigned>(value);
    else {settings.region=static_cast<unsigned>(value);settings.tenth=0;}
    return expert_write_ui(layout,id,value,expert_combat::encode(settings));
}
static unsigned expert_ui_value(Obj layout,unsigned id) {
    if(expert_range_id(id))return expert_range_value(id);
    auto settings=expert_ui_settings(layout,id);
    return expert_base_id(id)?settings.base:settings.region;
}
static bool expert_apply_preset(Obj layout,unsigned id) {
    auto found=expert_choices.find(layout);
    if(!expert_layout_valid(layout) || (id!=4 && id!=5) || found==expert_choices.end())return false;
    int choice=found->second.selected[id-4];
    unsigned difficulty=fn<unsigned(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_3EC920))(layout+0x1C8,43,0);
    if(choice<0 || choice>2 || difficulty>4 || !(dword(layout,0x19C)==0 || expert_in_game(layout)))return false;
    unsigned rate=expert_combat::preset_tenths(id,difficulty,static_cast<unsigned>(choice));
    auto settings=expert_ui_settings(layout,id);settings.region=rate/10;settings.tenth=rate%10;
    return expert_write_ui(layout,id,static_cast<int>(rate),expert_combat::encode(settings));
}
static void expert_create_presets(Obj row,Obj pane) {
    // Create beside the numeric control while its numeric pane is being built,
    // before 5F15C4 creates the other (initially hidden) choice panes.
    // Layout v6: node48 is a 112-wide copy of the standard button node12.
    // Other banks (v4 51 nodes, v5 63 nodes with 48-wide copies) create nothing.
    if(!enabled || !pane || dword(pane+0x130,0x10)!=53)return;
    // local4~7: radio objects (kind 1, 84B2D0) never drew in this pane.
    // local8~10: 48-wide hanyou_defo copies drew dark (no original layout uses
    // it below 112); local9's style 5 crashed on click (ADC009). 2026-10-25 user
    // decision: one standard-sized plain button (style 3, kind 0, like 5F150C)
    // cycles 소->중->대. The object is only 0x198 bytes: no radio setters (85AD20).
    Obj button=fn<Obj(__fastcall *)(Obj,int,int,int,int,int)>(GA(GA_9309C0))(pane+0x130,48,3,0,1,0);
    if(button) {
        expert_preset_panels[row].button=button;
        alignas(8) unsigned char callback[64]={};
        *reinterpret_cast<Obj *>(callback)=image+GA(GA_142C6C8);
        *reinterpret_cast<Obj *>(callback+8)=row;
        *reinterpret_cast<Obj *>(callback+56)=callback;
        fn<void(__fastcall *)(Obj,int,Obj)>(GA(GA_1A4FB0))(button,0x7FFFFFFE,callback);
        // No hide here. local9 runtime: after create-hide then bind-show the
        // skin parts 1..14 stayed hidden (flag 00, standard 11) and only the
        // label drew. Rows that are not adjacency rows are hidden at bind.
    }
    log_event("expert_preset_controls_created",button?1:0,53,1);
}
static Obj __fastcall expert_number_factory(Obj table,int id,int style,int kind,int layer,int order) { COST_SCOPE("expert_number_factory");
    Obj number=expert_next<Obj(__fastcall *)(Obj,int,int,int,int,int)>(next_expert_number_factory,GA(GA_9309C0))(table,id,style,kind,layer,order);
    Obj pane=table?pointer(table,0x18):nullptr;
    Obj row=pane?pointer(pane,8):nullptr;
    if(number && id==7 && row && pointer(row,0)==image+GA(GA_141ED88) && pointer(row,0x178)==pane)
        expert_create_presets(row,pane);
    return number;
}
static void expert_preset_controls(Obj layout,Obj row,unsigned id,Obj help,int mode) {
    auto &panel=expert_preset_panels[row];panel.layout=layout;
    bool regional=id==4 || id==5;
    Obj standard=pointer(row,0x1A0);
    int choice=regional?expert_choices[layout].selected[id-4]:-1;
    Obj button=panel.button;
    bool complete=button!=nullptr;
    const wchar_t *labels[]={L"소",L"중",L"대"};
    if(button) {
        fn<void(__fastcall *)(Obj,int)>(GA(GA_876580))(button,regional?1:0);
        if(regional) {
            // Unselected on entry; each click sends the next choice 소->중->대->소.
            fn<void(__fastcall *)(Obj,const wchar_t *)>(GA(GA_85ECF0))(button,choice>=0?labels[choice]:L"소·중·대");
            *reinterpret_cast<unsigned *>(button+0x80)=choice<0?0:(choice+1)%3;
            *reinterpret_cast<unsigned *>(button+0x84)=1;
            *reinterpret_cast<unsigned *>(button+0x88)=expert_choice_key+id;
            *reinterpret_cast<unsigned *>(button+0x8C)=1;
            fn<void(__fastcall *)(Obj,int)>(GA(GA_14140))(button,mode==0);
            if(help)fn<void(__fastcall *)(Obj,Obj,const wchar_t *,const wchar_t *)>(GA(GA_89D3F0))(
                help,button,L"누를 때마다 소→중→대로 바뀝니다. 표준을 누르면 현재 난이도의 인접효과를 적용합니다.",nullptr);
        }
    }
    if(regional && standard)fn<void(__fastcall *)(Obj,int)>(GA(GA_14140))(standard,mode==0 && complete && choice>=0);
    if(regional) {
        auto settings=expert_ui_settings(layout,id);
        if(settings.tenth && pointer(row,0x198)) {
            wchar_t text[24];swprintf_s(text,L"%u.%u",settings.region,settings.tenth);
            fn<void(__fastcall *)(Obj,const wchar_t *)>(GA(GA_85ECF0))(pointer(row,0x198),text);
        }
    }
}
static int __fastcall expert_number_current(Obj list,unsigned id) { COST_SCOPE("expert_expert_number_current");
    if(expert_popup_layout)return static_cast<int>(expert_ui_value(expert_popup_layout,expert_popup_id));
    return expert_next<int(__fastcall *)(Obj,unsigned)>(next_expert_number_current,GA(GA_5C15D0))(list,id);
}
static void expert_project_rows(Obj layout,float scroll=-1.0f) {
    const unsigned page=expert_layout_valid(layout)?dword(layout,0x1AC):0;
    if(!enabled || !expert_layout_valid(layout) || (page!=1 && page!=2))return;
    // Native rows: layout+0x8D0, 0x60 each; a page starts after the row counts (layout+0x17C+page*4) of the pages before
    // it (658DF0). 설정2 = page 1 (8 rows at +0xC30), 설정3 = page 2 (7 rows right after). The counts are checked for 설정3.
    if(page==2 && (dword(layout,0x17C)!=9 || dword(layout,0x180)!=8 || dword(layout,0x184)!=7)) {
        static bool told;if(!told) {told=true;log_event("expert_rows_unknown_pages",dword(layout,0x17C),dword(layout,0x180),dword(layout,0x184));}
        return;
    }
    Obj list=pointer(layout,0x8C0);
    if(scroll<0.0f)scroll=*reinterpret_cast<float *>(list+0x170);
    try {
        auto &rows=expert_rows[layout];
        const wchar_t *names[]={L"자세력 기본 공/방",L"타세력 기본 공/방",L"연계 거리 (3~5헥스)",L"안행 사거리 (2~3)"};
        for(unsigned i=0;i<4;++i) {
            memcpy(rows[i].bytes,layout+0xD50+(i<2?i:0)*0x60,0x60);
            *reinterpret_cast<unsigned *>(rows[i].bytes)=61+i;
            *reinterpret_cast<const wchar_t **>(rows[i].bytes+0x18)=names[i];
        }
        GameList projected;list_init(projected,GA(GA_1424600));
        bool complete=projected.handle!=nullptr;
        if(page==1)for(unsigned i=0;i<8 && complete;++i) {
            if(i==3)complete=list_add(projected,rows[0].bytes) && list_add(projected,rows[1].bytes);
            if(complete)complete=list_add(projected,layout+0xC30+i*0x60);
        }
        else for(unsigned i=0;i<7 && complete;++i) {   // 설정3: 연계 거리 under 연계 효과, 안행 사거리 last
            complete=list_add(projected,layout+0xF30+i*0x60);
            if(complete && i==1)complete=list_add(projected,rows[2].bytes);
            if(complete && i==6)complete=list_add(projected,rows[3].bytes);
        }
        if(complete) {
            fn<void(__fastcall *)(Obj,GameList *)>(GA(GA_58B6F0))(list,&projected);
            *reinterpret_cast<unsigned *>(list+0x1B8)=page==1?10:9;
            fn<void(__fastcall *)(Obj,float,int)>(GA(GA_877F70))(list,scroll,1);
        }
        list_free(projected);
    } catch(...) {log_event("expert_settings_rows_failed");}
}
#ifndef NO_RUNTIME_LOG
static std::map<Obj,unsigned char> expert_ingame_views;
#endif
static void expert_populate_after(Obj layout) {
    if(enabled && expert_layout_valid(layout)) {
#ifndef NO_RUNTIME_LOG
        if(!expert_ingame_views[layout]) {
            expert_ingame_views[layout]=1;
            unsigned mode=dword(layout,0x19C);
            log_event("expert_ingame_view",mode,
                static_cast<unsigned>(mode==0 || expert_in_game(layout)?4:0),0);
        }
#endif
        expert_project_rows(layout,*reinterpret_cast<float *>(layout+0x1B4));
    }
}
// Split fixtures: the entry-jump relay of 6566E0 still calls this.
static void __fastcall expert_populate(Obj layout,Obj second,Obj third,Obj fourth) { COST_SCOPE("expert_populate");
    original_expert_populate(layout,second,third,fourth);
    expert_populate_after(layout);
}
// Layout (user 2026-11-05 Layout compatibility design 8.3): the 6566E0 entry patch is replaced by its three callers
// 5F10F7 / 5F5B27 / 65EAAD; each site keeps its own next and projects the rows after the original population.
template<unsigned I> static void __fastcall expert_populate_site(Obj layout,Obj second,Obj third,Obj fourth) { COST_SCOPE("expert_populate");
    reinterpret_cast<ExpertPopulate>(next_expert_populate[I])(layout,second,third,fourth);
    expert_populate_after(layout);
}
static void __fastcall expert_bind_row(Obj row,Obj source,Obj help,int preset,int mode);
static void __fastcall expert_list_bind(Obj list,Obj item,unsigned index) { COST_SCOPE("expert_expert_list_bind");
    Obj layout=pointer(list,0x1C0);
    if(!enabled || !expert_layout_valid(layout)) {
        expert_next<void(__fastcall *)(Obj,Obj,unsigned)>(next_expert_list_bind,GA(GA_5BBA10))(list,item,index);return;
    }
    Obj source=fn<Obj(__fastcall *)(Obj,Obj)>(GA(GA_86D620))(list,item);
    Obj row=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_86DAA0))(list,index);
    if(!source || !row)return;
    using Get=unsigned(__fastcall *)(Obj,unsigned,unsigned);
    unsigned id=dword(source,0);
    *reinterpret_cast<unsigned *>(source+4)=expert_setting_id(id)?expert_ui_value(layout,id):
        fn<Get>(GA(GA_3EC920))(layout+0x1C8,id,0);
    Obj previous=expert_binding_layout;
    expert_binding_layout=layout;
    expert_bind_row(row,source,pointer(layout,0x8C8),static_cast<int>(fn<Get>(GA(GA_3EC920))(layout+0x1C8,43,0)),
                    static_cast<int>(dword(layout,0x19C)));
    expert_binding_layout=previous;
}
static void __fastcall expert_bind_row(Obj row,Obj source,Obj help,int preset,int mode) {
    using Bind=void(__fastcall *)(Obj,Obj,Obj,int,int);
    Obj layout=expert_binding_layout;
    unsigned id=source?dword(source,0):0;
    if(!enabled || !expert_layout_valid(layout) || !expert_setting_id(id)) {
        auto found=expert_preset_panels.find(row);
        if(found!=expert_preset_panels.end() && found->second.button)
            fn<void(__fastcall *)(Obj,int)>(GA(GA_876580))(found->second.button,0);
        fn<Bind>(GA(GA_643720))(row,source,help,preset,mode);return;
    }
    ExpertRow display;memcpy(display.bytes,source,0x60);
    // Native key 4 supplies 50..200 controls; key 7 supplies 0..200.
    // Both are numeric rows, so all existing keyboard/controller behavior stays.
    *reinterpret_cast<unsigned *>(display.bytes)=expert_base_id(id)?4:7;
    unsigned value=expert_ui_value(layout,id);
    *reinterpret_cast<unsigned *>(display.bytes+4)=expert_range_id(id)?expert_range_position(id):value;
    // User 2026-11-09: in the in-game view these four rows behave exactly like
    // the new-game dialog; every other row keeps the native read-only mode.
    int row_mode=expert_in_game(layout)?0:mode;
    ++expert_binding_depth;
    fn<Bind>(GA(GA_643720))(row,display.bytes,help,preset,row_mode);
    *reinterpret_cast<unsigned *>(row+0x220)=id;
    *reinterpret_cast<unsigned *>(row+0x224)=value;
    fn<void(__fastcall *)(Obj,int)>(GA(GA_85EB90))(pointer(row,0x190),expert_range_id(id)?0:expert_base_id(id)?100:4);
    if(expert_range_id(id) && pointer(row,0x198)) {
        wchar_t text[8];swprintf_s(text,L"%u",value);
        fn<void(__fastcall *)(Obj,const wchar_t *)>(GA(GA_85ECF0))(pointer(row,0x198),text);
    }
    --expert_binding_depth;
    expert_preset_controls(layout,row,id,help,row_mode || dword(source,0xC)?1:0);
    if(help) {
        Obj slider=pointer(row,0x190);
        // 89B110 resolves a message ID then calls 89B1C0 with UTF-16 text.
        // Reuse that text overload for the same five native hover targets.
        for(Obj control:{pointer(slider,0x1C0),pointer(slider,0x1C8),
                         pointer(row,0x178),pointer(row,0x198),pointer(row,0x1A0)})
            if(control)fn<void(__fastcall *)(Obj,Obj,const wchar_t *,const wchar_t *)>(GA(GA_89D3F0))(
                help,control,expert_tooltip(id),nullptr);
    }
}
static void __fastcall expert_choice_event(Obj callback,Obj *event) { COST_SCOPE("expert_choice_event");
    Obj layout=pointer(callback,8),data=event?*event:nullptr;
    if(enabled && expert_layout_valid(layout)) {
        if(expert_binding_depth)return;
        if(data && (dword(data,0x88)==expert_choice_key+4 || dword(data,0x88)==expert_choice_key+5)) {
            unsigned id=dword(data,0x88)-expert_choice_key,choice=dword(data,0x80);
            if(choice<=2 && (dword(layout,0x19C)==0 || expert_in_game(layout))) {
                expert_choices[layout].selected[id-4]=static_cast<int>(choice);
                expert_project_rows(layout);
            }
            return;
        }
    }
    expert_next<void(__fastcall *)(Obj,Obj *)>(next_expert_choice,GA(GA_661470))(callback,event);
}
static void __fastcall expert_slider_event(Obj callback,Obj *event) { COST_SCOPE("expert_slider_event");
    Obj layout=pointer(callback,8),data=event?*event:nullptr;
    if(enabled && expert_layout_valid(layout)) {
        if(expert_binding_depth)return;
        if(data && expert_setting_id(dword(data,0x88))) {
            const unsigned id=dword(data,0x88);
            if(!expert_range_id(id)) {expert_ui_store(layout,id,static_cast<int>(dword(data,0x80)));return;}
            expert_ui_store(layout,id,expert_range_from_position(id,static_cast<int>(dword(data,0x80))));
            wchar_t text[8];swprintf_s(text,L"%u",expert_range_value(id));
            for(auto &entry:expert_preset_panels)
                if(entry.second.layout==layout && dword(entry.first,0x220)==id && pointer(entry.first,0x198))
                    fn<void(__fastcall *)(Obj,const wchar_t *)>(GA(GA_85ECF0))(pointer(entry.first,0x198),text);
            return;
        }
    }
    expert_next<void(__fastcall *)(Obj,Obj *)>(next_expert_slider,GA(GA_660E20))(callback,event);
}
static void __fastcall expert_number_event(Obj callback,Obj *event,Obj third,Obj fourth) { COST_SCOPE("expert_number_event");
    Obj layout=pointer(callback,8),data=event?*event:nullptr;
    unsigned id=data?dword(data,0x88):0;
    if(!enabled || !expert_layout_valid(layout) || !expert_setting_id(id)) {
        expert_next<void(__fastcall *)(Obj,Obj *,Obj,Obj)>(next_expert_number_event,GA(GA_661ED0))(callback,event,third,fourth);return;
    }
    if(static_cast<int>(dword(data,0x80))==-1000) {
        if(expert_range_id(id))expert_ui_store(layout,id,static_cast<int>(expert_range_standard(id)));
        else if(expert_base_id(id))expert_ui_store(layout,id,100);
        else if(!expert_apply_preset(layout,id))return;
        expert_project_rows(layout);return;
    }
    // The native number popup copies its range synchronously, then its
    // confirmation callback reads the selected key from layout+1A4.
    alignas(8) unsigned char mapped[0x90]={};
    memcpy(mapped,data,sizeof(mapped));
    *reinterpret_cast<unsigned *>(mapped+0x88)=expert_base_id(id)?4:7;
    Obj previous_layout=expert_popup_layout;unsigned previous_id=expert_popup_id;
    expert_popup_layout=layout;expert_popup_id=id;
    fn<void(__fastcall *)(Obj,Obj,Obj,Obj)>(GA(GA_638B00))(layout,mapped,third,fourth);
    expert_popup_layout=previous_layout;expert_popup_id=previous_id;
    *reinterpret_cast<unsigned *>(layout+0x1A4)=id;
}
static void __fastcall expert_number_confirm(Obj callback) { COST_SCOPE("expert_number_confirm");
    Obj layout=pointer(callback,8);
    unsigned id=layout?dword(layout,0x1A4):0;
    if(enabled && expert_layout_valid(layout) && expert_setting_id(id)) {
        const int typed=static_cast<int>(dword(layout,0x1A8));
        expert_ui_store(layout,id,expert_range_id(id)?expert_range_from_number(id,typed):typed);
        expert_project_rows(layout);return;
    }
    expert_next<void(__fastcall *)(Obj)>(next_expert_number_confirm,GA(GA_660F00))(callback);
}
static void expert_release_panel(Obj row) {
    // The pane's native layout registry owns rendering and control teardown.
    expert_preset_panels.erase(row);
}
static void __fastcall expert_row_leave(Obj row,Obj context) { COST_SCOPE("expert_row_leave");
    // Native 6475C0 destroys the numeric panel, including its native children.
    expert_release_panel(row);
    expert_next<void(__fastcall *)(Obj,Obj)>(next_expert_row_leave,GA(GA_649CD0))(row,context);
}
static Obj __fastcall expert_row_destroy(Obj row,unsigned flags) { COST_SCOPE("expert_row_destroy");
    expert_release_panel(row);
    return expert_next<Obj(__fastcall *)(Obj,unsigned)>(next_expert_row_destroy,GA(GA_5A9A60))(row,flags);
}
// PW30 (user 2026-11-09): the in-game view decides with a 확인 button. Native facts: CSetUpScenarioSeniorState vtable
// 0x141F248; slot 1 (0x141F250) = 609100 enter (builds the rows at state+0x470 and the footer at state+0x488;
// *(u32*)(state+0x490)==1 is the in-game mode) and slot 5 (0x141F270) = 60EF50 frame (switches on the command code
// at state+0x498; case 8 leaves the view). Inside 609100 the footer call at 609543 is 862280(footer,1,1,-1).
// 862280(footer,id,kind,text) creates/shows a round footer button; a footer click writes the command code to
// state+0x498 and the clicked id to footer+0x80. PW32 (user 2026-11-09): we add the native round slot
// 862280(footer,2,0,-1), called directly (not through the redirect), and treat clicked id 2 as 확인; then we
// copy option dwords 4 and 5 from rows+0x1C8 to the live object image+GA(GA_1A49C98) and set command 8 like 뒤로.
static void *next_expert_ingame_build,*next_expert_ingame_frame,*next_expert_footer_add;
static Obj expert_ingame_building;                       // state whose slot-1 enter is running
struct ExpertIngameFooter {Obj state=nullptr;Obj footer=nullptr;};
static ExpertIngameFooter expert_ingame_footer;
static bool expert_ingame_installed;
#ifndef NO_RUNTIME_LOG
static unsigned expert_ingame_cmd_lines;
static unsigned expert_ingame_cmd_last=~0u;
#endif
static bool expert_ingame_commit(Obj rows) {
    if(!rows || !expert_layout_valid(rows))return false;
    Obj view=rows+0x1C8;
    unsigned v4=dword(expert_option(view,4),0),v5=dword(expert_option(view,5),0);
    *reinterpret_cast<unsigned *>(expert_option(image+GA(GA_1A49C98),4))=v4;
    *reinterpret_cast<unsigned *>(expert_option(image+GA(GA_1A49C98),5))=v5;
    range_rules::commit_pending();   // local105: 연계 거리 / 안행 사거리 apply with 확인 too
#ifndef NO_RUNTIME_LOG
    log_event("expert_ingame_commit",v4,v5,0);
#endif
    return true;
}
static unsigned long long __fastcall expert_ingame_build(Obj state) {
    expert_ingame_building=state;
    range_rules::clear_pending();
    unsigned long long result=expert_next<unsigned long long(__fastcall *)(Obj)>(next_expert_ingame_build,GA(GA_609100))(state);
    expert_ingame_building=nullptr;
    return result;
}
static void __fastcall expert_footer_button(Obj footer,unsigned id,int kind,int text) {
    expert_next<void(__fastcall *)(Obj,unsigned,int,int)>(next_expert_footer_add,GA(GA_862280))(footer,id,kind,text);
    bool made=false;
    Obj state=expert_ingame_building;
    if(mod_features.expert_ingame && state && footer && footer==pointer(state,0x488) && dword(state,0x490)==1 &&
       !(expert_ingame_footer.state==state && expert_ingame_footer.footer==footer)) {
        fn<void(__fastcall *)(Obj,unsigned,int,int)>(GA(GA_862280))(footer,2,0,-1);
        expert_ingame_footer={state,footer};
        made=true;
    }
#ifndef NO_RUNTIME_LOG
    log_event("expert_ingame_confirm_button",made?1u:0u,0,0);
#else
    (void)made;
#endif
}
static void __fastcall expert_ingame_frame(Obj state,Obj second,Obj third,Obj fourth) {
    // Forget a pair as soon as a different state frames; never dereference a forgotten state.
    if(expert_ingame_footer.state && expert_ingame_footer.state!=state)expert_ingame_footer={};
    if(expert_ingame_footer.state==state && state) {
        Obj footer=expert_ingame_footer.footer;
        unsigned command=dword(state,0x498),clicked=footer?dword(footer,0x80):0;
        if(command) {
#ifndef NO_RUNTIME_LOG
            unsigned packed=(command&0xFFFFu)|(clicked<<16);
            if(packed!=expert_ingame_cmd_last && expert_ingame_cmd_lines<200) {
                expert_ingame_cmd_last=packed;++expert_ingame_cmd_lines;
                log_event("expert_ingame_cmd",command,clicked,0);
            }
#endif
            if(clicked==2) {
                expert_ingame_commit(pointer(state,0x470));
                *reinterpret_cast<unsigned *>(state+0x498)=8;
                *reinterpret_cast<unsigned *>(footer+0x80)=0;
                expert_ingame_footer={};
            }
        }
    }
    expert_next<void(__fastcall *)(Obj,Obj,Obj,Obj)>(next_expert_ingame_frame,GA(GA_60EF50))(state,second,third,fourth);
}
static bool expert_ingame_install() {
    if(!mod_features.expert_ingame || expert_ingame_installed)return true;
    layout_client::Transaction tx;
    if(layout_client::vt(GA(GA_141F250),reinterpret_cast<void *>(&expert_ingame_build),
                         reinterpret_cast<void **>(&next_expert_ingame_build))<=0 ||
       layout_client::call(GA(GA_609543),reinterpret_cast<void *>(&expert_footer_button),
                           reinterpret_cast<void **>(&next_expert_footer_add))<=0 ||
       layout_client::vt(GA(GA_141F270),reinterpret_cast<void *>(&expert_ingame_frame),
                         reinterpret_cast<void **>(&next_expert_ingame_frame))<=0) {
        log_event("expert_ingame_register_rejected",0,0,0);return false;
    }
    tx.done=true;expert_ingame_installed=true;log_event("expert_ingame_registered",0,0,0);
    return true;
}
static Obj __fastcall expert_destroy(Obj layout,unsigned flags) { COST_SCOPE("expert_destroy");
    // PW30: the dialog that owns the remembered 확인 pair is going away; never keep (or dereference) it.
    if(expert_ingame_footer.state && pointer(expert_ingame_footer.state,0x470)==layout)expert_ingame_footer={};
    if(expert_in_game(layout))range_rules::clear_pending();   // 뒤로 (or 확인 after its commit): nothing stays pending
    // Discard borrowed pointers before native teardown of the pane registry.
    for(auto it=expert_preset_panels.begin();it!=expert_preset_panels.end();)
        if(it->second.layout==layout){Obj row=it->first;++it;expert_release_panel(row);}else ++it;
    Obj result=expert_next<Obj(__fastcall *)(Obj,unsigned)>(next_expert_destroy,GA(GA_5A9950))(layout,flags);
    expert_rows.erase(layout);expert_choices.erase(layout);
#ifndef NO_RUNTIME_LOG
    expert_ingame_views.erase(layout);
#endif
    return result;
}
// local102 (user 2026-11-13 "통상모드 숙련모드 설정3에다가 만들자"): the normal-mode dialog (vtable 141E920, built by
// 5F4470) has five pages of fixed choice rows; a page takes rows 0..8 (623900 refuses index 9+) and 설정3 (layout+0x198)
// uses 0..6. Rows 7 (연계 거리 3/4/5) and 8 (안행 사거리 2/3) are added right after the original build. A row is
// described by index, title, five help message ids, up to five choice labels and five unlock keys (0x14 = none)
// (59E8B0). 644070(layout, options, read) binds every row to the option block through 58BE30(page, view, read, row,
// &choice): read 0 shows the choice, otherwise it reads the pressed button. Its nine callers bind our two rows to
// range_rules the same way. The help line borrows 연계 효과's message. In the in-game view the rows are read-only like
// the original ones.
// local105 (user 2026-11-13 "숙련모드 바꾸듯이 통상모드 설정3에서 우리 설정만 바꿀 수 있게"): with expert_ingame on, the in-game
// view (layout+0x1B8 != 0) builds and binds our two rows with view 0, so only they stay clickable (623900 sets button flag 8
// and 58BE30 disables the unselected buttons only for view != 0). The view has no read pass of 644070 (frame 60F210 reads
// only for commands 1..3), so the state's frame (CSetUpScenarioState slot 5) reads the pressed buttons of rows 7/8 each
// frame. local105b (user "결정 버튼 눌러야 바뀌게"): the read value is only pending; the view gets the native round footer
// button 862280(footer,2,0,-1) right after its 뒤로 (CALL 609CC5 = 862280(footer,1,1,-1); the in-game branch skips the
// new-game buttons 3/2), and a click on it (clicked id 2 at footer+0x80) applies the pending values. The click's command
// is cleared before the original frame sees it (command 2 would start a game); the view stays open, 뒤로 leaves it.
static void *next_normal_build,*next_normal_bind[9],*next_normal_frame,*next_normal_footer;
static Obj normal_building_layout,normal_footer;   // in-game layout of the enter in progress; its 확인 footer
static bool normal_layout(Obj layout) {return layout && pointer(layout,0)==image+GA(GA_141E920);}
static bool normal_editable(Obj layout) {return mod_features.expert_ingame && dword(layout,0x1B8)!=0;}
static int normal_view(Obj layout) {return normal_editable(layout)?0:static_cast<int>(dword(layout,0x1B8));}
static bool normal_rows(Obj layout) {
    Obj page=pointer(layout,0x198);
    return page && pointer(page,0x90+7*8) && pointer(page,0x90+8*8);
}
static void normal_add_row(Obj page,Obj layout,int index,const wchar_t *title,const wchar_t *const *labels,unsigned count) {
    alignas(8) unsigned char desc[0xB0]={};
    *reinterpret_cast<int *>(desc)=index;
    *reinterpret_cast<const wchar_t **>(desc+8)=title;
    for(unsigned i=0;i<5;++i) {
        *reinterpret_cast<unsigned *>(desc+0x10+i*4)=10151;   // help message of 연계 효과
        *reinterpret_cast<unsigned *>(desc+0x50+i*4)=0x14;    // no unlock key
    }
    for(unsigned i=0;i<count && i<5;++i)*reinterpret_cast<const wchar_t **>(desc+0x28+i*8)=labels[i];
    fn<void(__fastcall *)(Obj,Obj,int,Obj)>(GA(GA_623900))(page,layout,normal_view(layout),desc);
}
static unsigned long long __fastcall normal_build(Obj layout,Obj second,Obj third,Obj fourth) {
    unsigned long long result=reinterpret_cast<unsigned long long(__fastcall *)(Obj,Obj,Obj,Obj)>(next_normal_build)(layout,second,third,fourth);
    if(enabled && normal_layout(layout)) {
        Obj page=pointer(layout,0x198);
        const bool fits=page && pointer(page,0x90+6*8) && !pointer(page,0x90+7*8) && !pointer(page,0x90+8*8);
        if(fits) {
            static const wchar_t *const link[]={L"3헥스",L"4헥스",L"5헥스"},*const wedge[]={L"2",L"3"};
            normal_add_row(page,layout,7,L"연계 거리",link,3);
            normal_add_row(page,layout,8,L"안행 사거리",wedge,2);
        }
        log_event("expert_normal_rows",fits?1u:0u,normal_rows(layout)?1u:0u,dword(layout,0x1B8));
        normal_building_layout=normal_editable(layout) && normal_rows(layout)?layout:nullptr;
        if(normal_building_layout)range_rules::clear_pending();
    }
    return result;
}
static void normal_bind(Obj layout,int read) {
    if(!enabled || !normal_layout(layout) || !normal_rows(layout))return;
    Obj page=pointer(layout,0x198);
    const int view=normal_view(layout);
    int link=static_cast<int>(range_rules::link_range)-2,wedge=static_cast<int>(range_rules::wedge_range)-2;
    auto bind=fn<void(__fastcall *)(Obj,int,int,int,int *)>(GA(GA_58BE30));
    bind(page,view,read,7,&link);bind(page,view,read,8,&wedge);
    if(read) {
        const bool pending=normal_editable(layout);
        if(link>=0 && link<=2)expert_range_store(63,link+3,pending);
        if(wedge>=0 && wedge<=1)expert_range_store(64,wedge+2,pending);
    }
}
template<unsigned I> static void __fastcall normal_bind_site(Obj layout,Obj options,unsigned long long read,Obj fourth) {
    reinterpret_cast<void(__fastcall *)(Obj,Obj,unsigned long long,Obj)>(next_normal_bind[I])(layout,options,read,fourth);
    normal_bind(layout,static_cast<int>(read));
}
[[maybe_unused]] static void __fastcall normal_footer_button(Obj footer,unsigned id,int kind,int text) {
    reinterpret_cast<void(__fastcall *)(Obj,unsigned,int,int)>(next_normal_footer)(footer,id,kind,text);
    if(enabled && normal_building_layout && footer && id==1) {
        fn<void(__fastcall *)(Obj,unsigned,int,int)>(GA(GA_862280))(footer,2,0,-1);
        normal_footer=footer;normal_building_layout=nullptr;
        log_event("expert_normal_confirm_button",1,0,0);
    }
}
[[maybe_unused]] static void __fastcall normal_frame(Obj state,Obj second,Obj third,Obj fourth) {
    Obj layout=state?pointer(state,0x480):nullptr;
    if(enabled && normal_layout(layout) && normal_editable(layout) && normal_rows(layout)) {
        normal_bind(layout,1);
        Obj footer=pointer(state,0x488);
        if(footer && footer==normal_footer && dword(state,0x490) && dword(footer,0x80)==2) {
            *reinterpret_cast<unsigned *>(state+0x490)=0;
            *reinterpret_cast<unsigned *>(footer+0x80)=0;
            const bool changed=range_rules::commit_pending();
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
            if(changed)briefing_overlay::toast(L"연계 거리·안행 사거리를 적용했습니다. 이 세이브에 함께 저장됩니다.");
#else
            (void)changed;
#endif
            // local109 (user 2026-11-13 "숙련모드는 잘닫히는데 통상모드가 안닫혀"): after applying, the window closes as the
            // senior 확인 does, by pressing the original footer button 1 (돌아가기; the frame's cancel key uses the same
            // 8592D0(footer, 1)). The pending values are already applied, so 돌아가기 drops nothing.
            fn<void(__fastcall *)(Obj,unsigned)>(GA(GA_8592D0))(footer,1);
            log_event("expert_normal_confirm_close",changed?1u:0u,0,0);
        }
    } else if(normal_footer && (!state || pointer(state,0x488)!=normal_footer))normal_footer=nullptr;
    reinterpret_cast<void(__fastcall *)(Obj,Obj,Obj,Obj)>(next_normal_frame)(state,second,third,fourth);
}
#ifdef LAYOUT_ROUTED
static void normal_install() {
    // The bind sites first: rows are only added when every bind site is ours, so no row is ever left unbound.
    unsigned bound=0;
    if(layout_client::call(GA(GA_5F841A),reinterpret_cast<void *>(&normal_bind_site<0>),&next_normal_bind[0])>0)++bound;
    if(layout_client::call(GA(GA_609A18),reinterpret_cast<void *>(&normal_bind_site<1>),&next_normal_bind[1])>0)++bound;
    if(layout_client::call(GA(GA_60F2E8),reinterpret_cast<void *>(&normal_bind_site<2>),&next_normal_bind[2])>0)++bound;
    if(layout_client::call(GA(GA_60F333),reinterpret_cast<void *>(&normal_bind_site<3>),&next_normal_bind[3])>0)++bound;
    if(layout_client::call(GA(GA_60F365),reinterpret_cast<void *>(&normal_bind_site<4>),&next_normal_bind[4])>0)++bound;
    if(layout_client::call(GA(GA_64477C),reinterpret_cast<void *>(&normal_bind_site<5>),&next_normal_bind[5])>0)++bound;
    if(layout_client::call(GA(GA_6447A7),reinterpret_cast<void *>(&normal_bind_site<6>),&next_normal_bind[6])>0)++bound;
    if(layout_client::call(GA(GA_6447D8),reinterpret_cast<void *>(&normal_bind_site<7>),&next_normal_bind[7])>0)++bound;
    if(layout_client::call(GA(GA_644816),reinterpret_cast<void *>(&normal_bind_site<8>),&next_normal_bind[8])>0)++bound;
    const bool built=bound==9 && layout_client::call(GA(GA_609980),reinterpret_cast<void *>(&normal_build),&next_normal_build)>0;
    log_event(built?"expert_normal_registered":"expert_normal_rejected",bound,0,0);
    if(built && mod_features.expert_ingame) {
        bool framed=layout_client::vt(GA(GA_141EAD8),reinterpret_cast<void *>(&normal_frame),&next_normal_frame)>0;
        if(framed && layout_client::call(GA(GA_609CC5),reinterpret_cast<void *>(&normal_footer_button),&next_normal_footer)<=0)framed=false;
        log_event("expert_normal_ingame",framed?1u:0u,0,0);
    }
}
#endif
