// All personnel UI is drawn and dispatched by the game engine.
#include "unified_personnel.h"
#include "unified_labels.h"
#include "officer_query.h"
#include <set>
#include <sstream>
#include "native_unified_overlay.inl"
#include "native_unified_dialogs.inl"
#include "native_unified_relations.inl"
namespace advisor_briefing {
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
static void hud(Obj layout,bool eligible,bool modal);
#else
static void hud(Obj,bool,bool){}
#endif
}
namespace unified_native {
static constexpr const wchar_t *actions[]={L"집단편제",L"의형제",L"관직수여",L"명품수여",
    L"이동",L"다중 전법 습득",L"교역"};
static Obj hud_layout;
static DWORD ui_thread;
static Obj opened_world,opened_force;
static unsigned force_id;
static bool showing,reopen_pending,open_pending;
static std::vector<unsigned> visible;   // officer ids listed in the open roster
static std::set<unsigned> selected;     // officer ids checked across searches
static unified_dialogs::Dialog *main_dialog;
static std::wstring copy_text(const wchar_t *text) {
    return text?std::wstring(text,wcsnlen(text,2048)):std::wstring();
}
static std::wstring officer_name(Obj p) {
    return valid(p)?copy_text(fn<const wchar_t *(__fastcall *)(Obj)>(0x22EEC0)(p)):L"없음";
}
static std::wstring column_name(unsigned id) {
    if(id>=213)return L"";
    Obj col=image+0x1A2B850+id*0x60;
    int message=static_cast<int>(dword(col,0x48));
    return message>=0?copy_text(fn<const wchar_t *(__fastcall *)(int)>(0x30CF80)(message)):
        copy_text(reinterpret_cast<const wchar_t *>(pointer(col,8)));
}
static std::wstring column_text(Obj p,unsigned id) {
    if(id>=213)return L"";
    Obj get=pointer(image+0x1A2B850+id*0x60,0x18);
    if(get<image+0x1000 || get>=image+kTextSize+0x1000)return L"";
    return copy_text(reinterpret_cast<const wchar_t *(__fastcall *)(Obj)>(get)(p));
}
template<size_t N> static std::wstring label(const wchar_t *const (&names)[N],unsigned id) {
    return id<N?names[id]:L"#"+std::to_wstring(id);
}
static Obj player_force() {
    Obj root=world(),options=root?pointer(root,0x871E8):nullptr;
    return options?fn<Obj(__fastcall *)(Obj)>(0x32E910)(options):nullptr;
}
static bool context_valid() {
    return enabled && GetCurrentThreadId()==ui_thread && world()==opened_world &&
        valid(opened_force) && player_force()==opened_force;
}
static bool serving(Obj p) {
    return valid(p) && p[0xE6]>=1 && p[0xE6]<=4 && personnel_force(p)==opened_force;
}
static Obj order_district() {
    return context_valid()?fn<Obj(__fastcall *)(Obj)>(0x22E7E0)(opened_force):nullptr;
}
static unsigned orders() {Obj district=order_district();return valid(district)?district[0x14]:0;}
static void set_orders(unsigned count) {
    Obj district=order_district();if(valid(district))district[0x14]=static_cast<unsigned char>((std::min)(count,200u));
}
static std::wstring city_name(unsigned id) {
    if(id>=1 && id<=51)return label(unified_labels::cities,id);
    if(id>=52 && id<=61)return label(unified_labels::gates,id);
    return L"출진·이동 중";
}
static void add_value(unified::Officer &p,unsigned facet,std::wstring value) {
    auto &values=p.facets[facet];
    if(!value.empty() && std::find(values.begin(),values.end(),value)==values.end())values.push_back(std::move(value));
}
static unified::Officer snapshot(Obj object) {
    unified::Officer p;p.id=word(object,0x10);p.name=officer_name(object);p.merit=word(object,0x1BE);
    for(unsigned i=0;i<5;++i)
        p.abilities[i]=fn<int(__fastcall *)(Obj,unsigned,unsigned)>(0x22FCE0)(object,i,0xFFF);
    // Native affiliation columns distinguish home and current location.
    add_value(p,unified::City,city_name(word(object,0xE4)));
    for(unsigned i=1;i<=14;++i)if(fn<int(__fastcall *)(Obj,unsigned)>(0x232350)(object,i))
        add_value(p,unified::Formation,label(unified_labels::formations,i));
    for(unsigned i=0;i<10;++i) {
        unsigned id=object[0x130+i];p.tactics[i]=id<=200?id:0;
        if(p.tactics[i])add_value(p,unified::Tactic,label(unified_labels::tactics,id));
    }
    add_value(p,unified::Policy,label(unified_labels::policies,object[0x14D]));
    for(unsigned i=0;i<9;++i) {
        unsigned trait=word(object,0x118+i*2);
        if(trait && trait<sizeof(unified_labels::personalities)/sizeof(*unified_labels::personalities))
            add_value(p,unified::Trait,label(unified_labels::personalities,trait));
    }
    add_value(p,unified::Doctrine,label(unified_labels::doctrines,object[0x14C]));
    Obj spouse=table(0x158,word(object,0xBA),6000),brother=table(0x158,word(object,0xBC),6000);
    add_value(p,unified::Spouse,valid(spouse)?officer_name(spouse):L"없음");
    add_value(p,unified::Brotherhood,valid(brother)?officer_name(brother):L"없음");
    for(unsigned i=186;i<=192;++i)add_value(p,unified::Inner,column_name(i)+L": "+column_text(object,i));
    if(unified_relations::comparable(object))add_value(p,unified::Link,unified_relations::text(object));
    return p;
}
struct Picker {
    std::wstring title;
    std::vector<std::wstring> labels;
    std::vector<unsigned> ids;
    std::set<unsigned> initial;
    bool multiple=true;
};
static Obj native_city(unsigned id);
static bool owned_city(Obj city);
static void execute_action(unsigned action);
static void edit_priorities();
static void open_main();
static std::wstring joined(const std::vector<std::wstring> &values) {
    std::wstring result;for(const auto &s:values){if(!result.empty())result+=L" · ";result+=s;}return result;
}
static bool pick(Picker picker,unified_dialogs::PickDone done) {
    return unified_dialogs::pick(picker.title,picker.labels,picker.ids,picker.multiple,picker.initial,std::move(done));
}
static void notice(const std::wstring &text,const std::wstring &title=L"통합 인사") {
    Picker p;p.title=title;p.multiple=false;
    std::wistringstream lines(text);std::wstring line;
    while(std::getline(lines,line))if(!line.empty()){p.ids.push_back(static_cast<unsigned>(p.ids.size()));p.labels.push_back(line);}
    if(p.labels.empty()){p.ids.push_back(0);p.labels.push_back(L"확인");}
    pick(std::move(p),[](bool,const std::vector<unsigned> &){});
}
static void sync_selection() {
    if(!main_dialog || main_dialog->completed)return;
    unified_dialogs::List checked;unified_dialogs::collect(*main_dialog,checked.value);
    for(unsigned id:visible)selected.erase(id);
    for(Link *l=list_first(checked.value);l;l=l->next)if(serving(l->value))selected.insert(word(l->value,0x10));
}
static bool info_request_refilter();
static void refresh_main() {
    if(info_request_refilter())return; // officer roster panel: rebuild its list instead
    if(!context_valid() || !main_dialog || main_dialog->completed)return;
    reopen_pending=true;unified_dialogs::close(*main_dialog,true);
}
static void action_menu() {
    Picker p;p.title=L"선택 무장 "+std::to_wstring(selected.size())+L"명 — 일괄 명령";p.multiple=false;
    for(unsigned i=0;i<7;++i){p.ids.push_back(i);p.labels.push_back(std::to_wstring(i+1)+L". "+actions[i]);}
    pick(std::move(p),[](bool ok,const std::vector<unsigned> &ids){if(ok && !ids.empty() && context_valid())execute_action(ids[0]);});
}
template<unsigned Slot>static const wchar_t *__fastcall trait_text(Obj p) {
    unsigned id=word(p,0x118+Slot*2);
    return id<sizeof(unified_labels::personalities)/sizeof(*unified_labels::personalities)?unified_labels::personalities[id]:L"";
}
template<unsigned Slot>static int __fastcall trait_compare(Obj a,Obj b){return wcscmp(trait_text<Slot>(a),trait_text<Slot>(b));}
static unsigned char columns[223][0x60];
static bool columns_ready;
static void prepare_columns() {
    if(columns_ready)return;
    memcpy(columns,image+0x1A2B850,213*0x60);
    const wchar_t *names[]={L"개성1",L"개성2",L"개성3",L"개성4",L"개성5",L"개성6",L"개성7",L"개성8",L"개성9",L"연계"};
    void *text[]={reinterpret_cast<void *>(&trait_text<0>),reinterpret_cast<void *>(&trait_text<1>),reinterpret_cast<void *>(&trait_text<2>),
        reinterpret_cast<void *>(&trait_text<3>),reinterpret_cast<void *>(&trait_text<4>),reinterpret_cast<void *>(&trait_text<5>),
        reinterpret_cast<void *>(&trait_text<6>),reinterpret_cast<void *>(&trait_text<7>),reinterpret_cast<void *>(&trait_text<8>),reinterpret_cast<void *>(&unified_relations::text)};
    void *compare[]={reinterpret_cast<void *>(&trait_compare<0>),reinterpret_cast<void *>(&trait_compare<1>),reinterpret_cast<void *>(&trait_compare<2>),
        reinterpret_cast<void *>(&trait_compare<3>),reinterpret_cast<void *>(&trait_compare<4>),reinterpret_cast<void *>(&trait_compare<5>),
        reinterpret_cast<void *>(&trait_compare<6>),reinterpret_cast<void *>(&trait_compare<7>),reinterpret_cast<void *>(&trait_compare<8>),nullptr};
    for(unsigned i=0;i<10;++i){Obj c=columns[213+i];memcpy(c,columns[3],0x60);
        *reinterpret_cast<unsigned *>(c)=213+i;*reinterpret_cast<const wchar_t **>(c+8)=names[i];
        *reinterpret_cast<unsigned *>(c+0x10)=i==9?132:108;
        *reinterpret_cast<void **>(c+0x18)=text[i];*reinterpret_cast<void **>(c+0x20)=nullptr;
        *reinterpret_cast<void **>(c+0x28)=nullptr;*reinterpret_cast<void **>(c+0x30)=compare[i];
        *reinterpret_cast<void **>(c+0x38)=nullptr;*reinterpret_cast<void **>(c+0x40)=nullptr;*reinterpret_cast<int *>(c+0x48)=-1;
    }
    // Match the original sortie Link descriptor's native value/sort protocol.
    Obj link=columns[222];
    *reinterpret_cast<unsigned *>(link+0x10)=132;*reinterpret_cast<unsigned *>(link+0x14)=0x20000008;
    *reinterpret_cast<unsigned *>(link+0x20)=1;
    *reinterpret_cast<void **>(link+0x28)=reinterpret_cast<void *>(&unified_relations::sort_value);
    // One native roster: keep original widths except the long policy text.
    *reinterpret_cast<unsigned *>(columns[101]+0x10)=600;
    columns_ready=true;
}
static const int search_tabs[12][14]={
    {0,1,2,3,4,5,35,6,21,-1}, {0,8,9,10,11,12,18,20,-1},
    {0,213,214,215,216,217,218,219,220,221,-1}, {0,51,52,53,54,55,-1}, {0,56,57,58,59,60,-1},
    {0,40,41,42,43,44,45,46,47,48,49,50,-1}, {0,29,28,24,30,31,32,33,-1},
    {0,37,86,88,34,204,-1}, {0,194,193,196,197,198,-1}, {0,97,98,26,101,-1},
    {0,186,187,188,189,190,191,192,-1}, {0,222,30,31,32,33,-1}
};
static const wchar_t *const search_tab_names[12]={L"소속",L"능력",L"개성",L"전법1",L"전법2",L"진형",
    L"개인",L"임무",L"공로",L"정책",L"내면",L"연계"};
}
#include "native_officer_search.inl"
#include "native_officer_info.inl"
#include "native_officer_snapshot.inl"
namespace unified_native {
static void more_menu() {
    Picker p;p.multiple=false;p.title=L"무장 검색 — 더 보기";
    std::vector<std::wstring> rows={L"검색 결과 전체 선택 ("+std::to_wstring(visible.size())+L"명)",L"선택 모두 해제",
        search_only_selected?L"선택한 무장만 보기: 켜짐 → 끄기":L"선택한 무장만 보기: 꺼짐 → 켜기",
        L"검색 조건 저장 (이름 입력)",L"저장한 검색 조건 불러오기",L"검색 조건 초기화",L"출진 전법 우선순위 설정",
        L"검색기(OfficerSearch.exe)용 전체 무장 스냅샷 저장"};
    for(unsigned i=0;i<rows.size();++i){p.ids.push_back(i);p.labels.push_back(numbered(i+1,rows[i]));}
    pick(std::move(p),[](bool ok,const std::vector<unsigned> &ids){
        if(!ok || ids.empty() || !context_valid())return;
        switch(ids[0]) {
        case 0:selected.insert(visible.begin(),visible.end());refresh_main();break;
        case 1:selected.clear();search_only_selected=false;refresh_main();break;
        case 2:search_only_selected=!search_only_selected;refresh_main();break;
        case 3:search_save_preset();break;
        case 4:search_load_preset();break;
        case 5:search_query={};search_only_selected=false;unified_relations::set_reference(nullptr);refresh_main();break;
        case 6:edit_priorities();break;
        default: {
            int n=snapshot_export();
            search_status=n>=0?L"스냅샷 저장: 무장 "+std::to_wstring(n)+L"명 (AI_TacticsMod\\data)":L"스냅샷을 저장하지 못했습니다";
            refresh_main();break;
        }
        }
    });
}
static void main_frame(unified_dialogs::Dialog &r,bool top);
static void open_main() {
    if(!context_valid())return;prepare_columns();search_capture();
    // Right-hand panel filters (keyword, trait/formation/tactic tokens) plus the condition tree.
    visible.clear();
    for(const auto &record:search_records)
        if(info_pass(record) && (!search_only_selected || selected.count(record.id)))visible.push_back(record.id);
    info.records=search_records;info.total=static_cast<unsigned>(search_records.size());
    info.matched=static_cast<unsigned>(visible.size());info.multi=true;info.dirty=true;
    auto request=std::make_unique<unified_dialogs::Dialog>();auto r=request.get();
    for(unsigned id:visible) {
        Obj p=table(0x158,id,6000);if(!list_add(r->source.value,p))return;
        if(selected.count(id) && !list_add(r->initial.value,p))return;
    }
    unsigned hidden=0;for(unsigned id:selected)if(std::find(visible.begin(),visible.end(),id)==visible.end())++hidden;
    r->title=L"무장 검색  결과 "+std::to_wstring(visible.size())+L"명 · 선택 "+std::to_wstring(selected.size())+
        L"명 · 명령서 "+std::to_wstring(orders());
    if(hidden)r->title+=L" (목록 밖 선택 "+std::to_wstring(hidden)+L"명)";
    if(!search_query.empty())r->title+=L"  |  "+search_summary();
    if(search_only_selected)r->title+=L"  [선택만]";
    if(!search_status.empty()){r->title+=L"  · "+search_status;search_status.clear();}
    for(unsigned i=0;i<12;++i)r->tabs.push_back({search_tabs[i],search_tab_names[i],-1,0});r->tabs.push_back({nullptr,nullptr,0,0});
    *reinterpret_cast<Obj *>(r->config+0x10)=reinterpret_cast<Obj>(columns);
    *reinterpret_cast<unsigned *>(r->config+0x18)=2;*reinterpret_cast<unsigned *>(r->config+0x1C)=6000;
    // Left pane: list width = first nine columns, leaving the right side for the filter panel.
    *reinterpret_cast<unsigned *>(r->config+0xA4)=9;
    r->on_frame=main_frame;
    r->button_names={L"검색 조건",L"일괄 명령",L"더 보기"};
    r->command=[](unsigned event){if(!context_valid())return;sync_selection();
        if(event==0)search_open_editor({});else if(event==1)action_menu();else more_menu();};
    r->done=[](bool){main_dialog=nullptr;showing=false;officer_panel::show(false,{});officer_panel::take();};
    main_dialog=r;showing=unified_dialogs::open(std::move(request));if(!showing)main_dialog=nullptr;
}
// 통합 인사: native list on the left (nine columns), filter panel on the right,
// the same panel as 정보 > 무장. Panel clicks are handled here on the UI thread.
static void main_action(unsigned action) {
    using namespace officer_panel;
    if(action==Multi)return; // this roster is always multi-select
    if(action==Commands) {
        sync_selection();
        if(selected.empty()){info.status=L"명령할 무장을 체크해 주세요";info.dirty=true;return;}
        action_menu();return;
    }
    info_action(action);
}
static void main_frame(unified_dialogs::Dialog &r,bool top) {
    if(!enabled || !mod_features.unified || &r!=main_dialog){officer_panel::show(false,{});return;}
    Obj list=pointer(r.object,0x180);
    if(!top || !list){officer_panel::show(false,{});return;}
    unified_overlay::install();
    int x=info_absolute(list,0x20),y=info_absolute(list,0x24),w=static_cast<int>(dword(list,0x28)),h=static_cast<int>(dword(list,0x2C));
    unified_overlay::Bounds where{x+w+44,y,officer_panel::kWidth,h};
    if(where.x+where.width>1912)where.x=1912-where.width;
    officer_panel::show(w>0 && h>200,where);
    // 연계 follows the checked/focused officer in the list.
    unified_dialogs::List chosen;unified_dialogs::collect(r,chosen.value);
    Obj reference=nullptr;for(Link *l=list_first(chosen.value);l;l=l->next)if(valid(l->value))reference=l->value;
    if(reference!=info.reference){info.reference=reference;unified_relations::set_reference(reference);}
    unsigned action=officer_panel::take();if(action)main_action(action);
    if(info.refilter && unified_dialogs::active==&r){info.refilter=false;sync_selection();refresh_main();}
    info.matched=static_cast<unsigned>(visible.size());info_publish();
}
static void __fastcall frame(Obj layout) {
    fn<void(__fastcall *)(Obj)>(0x894420)(layout);if(!enabled){unified_overlay::show(false);return;}
    ui_thread=GetCurrentThreadId();
    Obj manager=fn<Obj(__fastcall *)()>(0x110E0)();
    Obj modal=manager?fn<Obj(__fastcall *)(Obj)>(0x56FC20)(manager):nullptr;
    bool active=unified_overlay::eligible(world()!=nullptr,valid(player_force()),fn<unsigned(__fastcall *)(Obj)>(0x86F250)(layout)!=0,modal!=nullptr);
    advisor_briefing::hud(layout,active,modal!=nullptr);
    if(!mod_features.unified){unified_overlay::show(false);return;}
    if(!active){unified_overlay::show(false);return;}
    if(opened_world && (opened_world!=world() || opened_force!=player_force())) {
        unified_dialogs::abandon();main_dialog=nullptr;showing=false;reopen_pending=false;
        opened_world=opened_force=nullptr;force_id=0;unified_relations::set_reference(nullptr);
        search_query={};search_labels.clear();search_records.clear();selected.clear();search_only_selected=false;
    }
    unified_dialogs::release_closed();
    hud_layout=layout;unified_overlay::install();
    if(unified_overlay::take_click())open_pending=true;
    if((open_pending || reopen_pending) && !showing && !unified_dialogs::active) {
        bool fresh=open_pending;open_pending=false;reopen_pending=false;
        opened_world=world();opened_force=player_force();force_id=0;
        for(unsigned i=1;i<=51;++i)if(table(0xDF30,i,51)==opened_force)force_id=i;
        if(force_id){(void)fresh;open_main();}
    }
    unified_overlay::show(!showing && !open_pending && !reopen_pending && !unified_dialogs::active);
}
static void __fastcall leave(Obj layout,Obj context) {
    briefing_overlay::publish({},false);
    if(hud_layout==layout){hud_layout=nullptr;unified_overlay::show(false);unified_overlay::take_click();}
    fn<void(__fastcall *)(Obj,Obj)>(0x8782E0)(layout,context);
}
}
