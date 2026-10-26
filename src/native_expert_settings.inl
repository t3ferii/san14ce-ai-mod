/* CSetUpScenarioSeniorLayout only: the scenario's expert settings dialog.
 * Current values remain in its native option object and follow the native
 * commit/cancel and save/load copies. No file IO occurs in these callbacks.
 * Evidence and native ABI: research/expert_settings/implementation.md. */
#include <array>
#include <map>
struct ExpertRow { alignas(8) unsigned char bytes[0x60]; };
// AI-CONTRACT [UI lifetime]: Map keys are native layout identities. Preserve lifecycle cleanup and commit/cancel/save semantics; row bytes and screen layout records must agree. Successful static checks do not prove rendering/click behavior.
static std::map<Obj,std::array<ExpertRow,2>> expert_rows;
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
static bool expert_setting_id(unsigned id) {return id==4 || id==5 || id==61 || id==62;}
static bool expert_base_id(unsigned id) {return id==61 || id==62;}
static unsigned expert_force_key(unsigned id) {return id==4 || id==61?4:5;}
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
    default:return nullptr;
    }
}
static bool expert_layout_valid(Obj layout) {
    return layout && pointer(layout,0)==image+0x141BE88;
}
static expert_combat::Settings expert_ui_settings(Obj layout,unsigned id) {
    return expert_combat::decode(dword(expert_option(layout+0x1C8,expert_force_key(id)),0));
}
static bool expert_ui_store(Obj layout,unsigned id,int value) {
    if(!expert_layout_valid(layout) || !expert_setting_id(id) || value<0 || value>200 ||
       (expert_base_id(id) && value<50))return false;
    auto settings=expert_ui_settings(layout,id);
    if(expert_base_id(id))settings.base=static_cast<unsigned>(value);
    else {settings.region=static_cast<unsigned>(value);settings.tenth=0;}
    *reinterpret_cast<unsigned *>(expert_option(layout+0x1C8,expert_force_key(id)))=expert_combat::encode(settings);
    return true;
}
static unsigned expert_ui_value(Obj layout,unsigned id) {
    auto settings=expert_ui_settings(layout,id);
    return expert_base_id(id)?settings.base:settings.region;
}
static bool expert_apply_preset(Obj layout,unsigned id) {
    auto found=expert_choices.find(layout);
    if(!expert_layout_valid(layout) || (id!=4 && id!=5) || found==expert_choices.end())return false;
    int choice=found->second.selected[id-4];
    unsigned difficulty=fn<unsigned(__fastcall *)(Obj,unsigned,unsigned)>(0x3EA400)(layout+0x1C8,43,0);
    if(choice<0 || choice>2 || difficulty>4 || dword(layout,0x19C)!=0)return false;
    unsigned rate=expert_combat::preset_tenths(id,difficulty,static_cast<unsigned>(choice));
    auto settings=expert_ui_settings(layout,id);settings.region=rate/10;settings.tenth=rate%10;
    *reinterpret_cast<unsigned *>(expert_option(layout+0x1C8,id))=expert_combat::encode(settings);
    return true;
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
    Obj button=fn<Obj(__fastcall *)(Obj,int,int,int,int,int)>(0x92E430)(pane+0x130,48,3,0,1,0);
    if(button) {
        expert_preset_panels[row].button=button;
        alignas(8) unsigned char callback[64]={};
        *reinterpret_cast<Obj *>(callback)=image+0x1429638;
        *reinterpret_cast<Obj *>(callback+8)=row;
        *reinterpret_cast<Obj *>(callback+56)=callback;
        fn<void(__fastcall *)(Obj,int,Obj)>(0x1A3CB0)(button,0x7FFFFFFE,callback);
        // No hide here. local9 runtime: after create-hide then bind-show the
        // skin parts 1..14 stayed hidden (flag 00, standard 11) and only the
        // label drew. Rows that are not adjacency rows are hidden at bind.
    }
    log_event("expert_preset_controls_created",button?1:0,53,1);
}
static Obj __fastcall expert_number_factory(Obj table,int id,int style,int kind,int layer,int order) {
    Obj number=fn<Obj(__fastcall *)(Obj,int,int,int,int,int)>(0x92E430)(table,id,style,kind,layer,order);
    Obj pane=table?pointer(table,0x18):nullptr;
    Obj row=pane?pointer(pane,8):nullptr;
    if(number && id==7 && row && pointer(row,0)==image+0x141BCF8 && pointer(row,0x178)==pane)
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
        fn<void(__fastcall *)(Obj,int)>(0x8741D0)(button,regional?1:0);
        if(regional) {
            // Unselected on entry; each click sends the next choice 소->중->대->소.
            fn<void(__fastcall *)(Obj,const wchar_t *)>(0x85C9C0)(button,choice>=0?labels[choice]:L"소·중·대");
            *reinterpret_cast<unsigned *>(button+0x80)=choice<0?0:(choice+1)%3;
            *reinterpret_cast<unsigned *>(button+0x84)=1;
            *reinterpret_cast<unsigned *>(button+0x88)=expert_choice_key+id;
            *reinterpret_cast<unsigned *>(button+0x8C)=1;
            fn<void(__fastcall *)(Obj,int)>(0x141E0)(button,mode==0);
            if(help)fn<void(__fastcall *)(Obj,Obj,const wchar_t *,const wchar_t *)>(0x89B1C0)(
                help,button,L"누를 때마다 소→중→대로 바뀝니다. 표준을 누르면 현재 난이도의 인접효과를 적용합니다.",nullptr);
        }
    }
    if(regional && standard)fn<void(__fastcall *)(Obj,int)>(0x141E0)(standard,mode==0 && complete && choice>=0);
    if(regional) {
        auto settings=expert_ui_settings(layout,id);
        if(settings.tenth && pointer(row,0x198)) {
            wchar_t text[24];swprintf_s(text,L"%u.%u",settings.region,settings.tenth);
            fn<void(__fastcall *)(Obj,const wchar_t *)>(0x85C9C0)(pointer(row,0x198),text);
        }
    }
}
static int __fastcall expert_number_current(Obj list,unsigned id) { COST_SCOPE("expert_expert_number_current");
    if(expert_popup_layout)return static_cast<int>(expert_ui_value(expert_popup_layout,expert_popup_id));
    return fn<int(__fastcall *)(Obj,unsigned)>(0x5BEC40)(list,id);
}
static void expert_project_rows(Obj layout,float scroll=-1.0f) {
    if(!enabled || !expert_layout_valid(layout) || dword(layout,0x1AC)!=1)return;
    Obj list=pointer(layout,0x8C0);
    if(scroll<0.0f)scroll=*reinterpret_cast<float *>(list+0x170);
    try {
        auto &rows=expert_rows[layout];
        const wchar_t *names[]={L"자세력 기본 공/방",L"타세력 기본 공/방"};
        for(unsigned i=0;i<2;++i) {
            memcpy(rows[i].bytes,layout+0xD50+i*0x60,0x60);
            *reinterpret_cast<unsigned *>(rows[i].bytes)=61+i;
            *reinterpret_cast<const wchar_t **>(rows[i].bytes+0x18)=names[i];
        }
        GameList projected;list_init(projected,0x1421570);
        bool complete=projected.handle!=nullptr;
        for(unsigned i=0;i<8 && complete;++i) {
            if(i==3)complete=list_add(projected,rows[0].bytes) && list_add(projected,rows[1].bytes);
            if(complete)complete=list_add(projected,layout+0xC30+i*0x60);
        }
        if(complete) {
            fn<void(__fastcall *)(Obj,GameList *)>(0x466220)(list,&projected);
            *reinterpret_cast<unsigned *>(list+0x1B8)=10;
            fn<void(__fastcall *)(Obj,float,int)>(0x875BC0)(list,scroll,1);
        }
        list_free(projected);
    } catch(...) {log_event("expert_settings_rows_failed");}
}
static void __fastcall expert_populate(Obj layout,Obj second,Obj third,Obj fourth) {
    original_expert_populate(layout,second,third,fourth);
    if(enabled && expert_layout_valid(layout))expert_project_rows(layout,*reinterpret_cast<float *>(layout+0x1B4));
}
static void __fastcall expert_bind_row(Obj row,Obj source,Obj help,int preset,int mode);
static void __fastcall expert_list_bind(Obj list,Obj item,unsigned index) { COST_SCOPE("expert_expert_list_bind");
    Obj layout=pointer(list,0x1C0);
    if(!enabled || !expert_layout_valid(layout)) {
        fn<void(__fastcall *)(Obj,Obj,unsigned)>(0x5B9110)(list,item,index);return;
    }
    Obj source=fn<Obj(__fastcall *)(Obj,Obj)>(0x86B270)(list,item);
    Obj row=fn<Obj(__fastcall *)(Obj,unsigned)>(0x86B6F0)(list,index);
    if(!source || !row)return;
    using Get=unsigned(__fastcall *)(Obj,unsigned,unsigned);
    unsigned id=dword(source,0);
    *reinterpret_cast<unsigned *>(source+4)=expert_setting_id(id)?expert_ui_value(layout,id):
        fn<Get>(0x3EA400)(layout+0x1C8,id,0);
    Obj previous=expert_binding_layout;
    expert_binding_layout=layout;
    expert_bind_row(row,source,pointer(layout,0x8C8),static_cast<int>(fn<Get>(0x3EA400)(layout+0x1C8,43,0)),
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
            fn<void(__fastcall *)(Obj,int)>(0x8741D0)(found->second.button,0);
        fn<Bind>(0x640F20)(row,source,help,preset,mode);return;
    }
    ExpertRow display;memcpy(display.bytes,source,0x60);
    // Native key 4 supplies 50..200 controls; key 7 supplies 0..200.
    // Both are numeric rows, so all existing keyboard/controller behavior stays.
    *reinterpret_cast<unsigned *>(display.bytes)=expert_base_id(id)?4:7;
    unsigned value=expert_ui_value(layout,id);
    *reinterpret_cast<unsigned *>(display.bytes+4)=value;
    ++expert_binding_depth;
    fn<Bind>(0x640F20)(row,display.bytes,help,preset,mode);
    *reinterpret_cast<unsigned *>(row+0x220)=id;
    *reinterpret_cast<unsigned *>(row+0x224)=value;
    fn<void(__fastcall *)(Obj,int)>(0x85C860)(pointer(row,0x190),expert_base_id(id)?100:4);
    --expert_binding_depth;
    expert_preset_controls(layout,row,id,help,mode || dword(source,0xC)?1:0);
    if(help) {
        Obj slider=pointer(row,0x190);
        // 89B110 resolves a message ID then calls 89B1C0 with UTF-16 text.
        // Reuse that text overload for the same five native hover targets.
        for(Obj control:{pointer(slider,0x1C0),pointer(slider,0x1C8),
                         pointer(row,0x178),pointer(row,0x198),pointer(row,0x1A0)})
            if(control)fn<void(__fastcall *)(Obj,Obj,const wchar_t *,const wchar_t *)>(0x89B1C0)(
                help,control,expert_tooltip(id),nullptr);
    }
}
static void __fastcall expert_choice_event(Obj callback,Obj *event) {
    Obj layout=pointer(callback,8),data=event?*event:nullptr;
    if(enabled && expert_layout_valid(layout)) {
        if(expert_binding_depth)return;
        if(data && (dword(data,0x88)==expert_choice_key+4 || dword(data,0x88)==expert_choice_key+5)) {
            unsigned id=dword(data,0x88)-expert_choice_key,choice=dword(data,0x80);
            if(choice<=2 && dword(layout,0x19C)==0) {
                expert_choices[layout].selected[id-4]=static_cast<int>(choice);
                expert_project_rows(layout);
            }
            return;
        }
    }
    fn<void(__fastcall *)(Obj,Obj *)>(0x65EDD0)(callback,event);
}
static void __fastcall expert_slider_event(Obj callback,Obj *event) {
    Obj layout=pointer(callback,8),data=event?*event:nullptr;
    if(enabled && expert_layout_valid(layout)) {
        if(expert_binding_depth)return;
        if(data && expert_setting_id(dword(data,0x88))) {
            expert_ui_store(layout,dword(data,0x88),static_cast<int>(dword(data,0x80)));return;
        }
    }
    fn<void(__fastcall *)(Obj,Obj *)>(0x65E810)(callback,event);
}
static void __fastcall expert_number_event(Obj callback,Obj *event,Obj third,Obj fourth) {
    Obj layout=pointer(callback,8),data=event?*event:nullptr;
    unsigned id=data?dword(data,0x88):0;
    if(!enabled || !expert_layout_valid(layout) || !expert_setting_id(id)) {
        fn<void(__fastcall *)(Obj,Obj *,Obj,Obj)>(0x65F890)(callback,event,third,fourth);return;
    }
    if(static_cast<int>(dword(data,0x80))==-1000) {
        if(expert_base_id(id))expert_ui_store(layout,id,100);
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
    fn<void(__fastcall *)(Obj,Obj,Obj,Obj)>(0x636250)(layout,mapped,third,fourth);
    expert_popup_layout=previous_layout;expert_popup_id=previous_id;
    *reinterpret_cast<unsigned *>(layout+0x1A4)=id;
}
static void __fastcall expert_number_confirm(Obj callback) {
    Obj layout=pointer(callback,8);
    unsigned id=layout?dword(layout,0x1A4):0;
    if(enabled && expert_layout_valid(layout) && expert_setting_id(id)) {
        expert_ui_store(layout,id,static_cast<int>(dword(layout,0x1A8)));
        expert_project_rows(layout);return;
    }
    fn<void(__fastcall *)(Obj)>(0x65E8C0)(callback);
}
static void expert_release_panel(Obj row) {
    // The pane's native layout registry owns rendering and control teardown.
    expert_preset_panels.erase(row);
}
static void __fastcall expert_row_leave(Obj row,Obj context) {
    // Native 6475C0 destroys the numeric panel, including its native children.
    expert_release_panel(row);
    fn<void(__fastcall *)(Obj,Obj)>(0x6475C0)(row,context);
}
static Obj __fastcall expert_row_destroy(Obj row,unsigned flags) {
    expert_release_panel(row);
    return fn<Obj(__fastcall *)(Obj,unsigned)>(0x5A7450)(row,flags);
}
static Obj __fastcall expert_destroy(Obj layout,unsigned flags) {
    // Discard borrowed pointers before native teardown of the pane registry.
    for(auto it=expert_preset_panels.begin();it!=expert_preset_panels.end();)
        if(it->second.layout==layout){Obj row=it->first;++it;expert_release_panel(row);}else ++it;
    Obj result=fn<Obj(__fastcall *)(Obj,unsigned)>(0x5A73A0)(layout,flags);
    expert_rows.erase(layout);expert_choices.erase(layout);
    return result;
}
