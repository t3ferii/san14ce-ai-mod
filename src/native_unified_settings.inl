#include "unified_tactic_settings.h"
namespace unified_native {
static std::wstring priority_name(int id) {
    if(id==unified::Unique)return L"고유전법";if(id==unified::Common)return L"공용전법";if(id==unified::Trade)return L"교역전법";
    return label(unified_labels::tactics,static_cast<unsigned>(id));
}
struct PriorityEditor {unified::Priorities pending;unsigned profile=0,operation=99,index=0;unified_dialogs::Dialog *dialog=nullptr;
    std::vector<int> &entries(){return profile==0?pending.martial:pending.intellect;}};
static void priority_menu(std::shared_ptr<PriorityEditor> editor);
static void priority_order(std::shared_ptr<PriorityEditor> editor);
static void priority_add(std::shared_ptr<PriorityEditor> editor) {
    Picker p;p.title=L"우선순위에 추가할 종류·개별 전법";p.multiple=false;
    for(int id:{unified::Unique,unified::Common,unified::Trade}){p.ids.push_back(static_cast<unsigned>(id+3));p.labels.push_back(priority_name(id));}
    for(unsigned id=1;id<=200;++id)if(valid(table(0x774D0,id,200))){p.ids.push_back(id+3);p.labels.push_back(priority_name(static_cast<int>(id)));}
    pick(std::move(p),[editor](bool ok,const std::vector<unsigned> &ids){if(!context_valid())return;
        if(ok && !ids.empty()){int id=static_cast<int>(ids[0])-3;auto &v=editor->entries();if(std::find(v.begin(),v.end(),id)==v.end())v.push_back(id);}priority_order(editor);});
}
static void priority_order(std::shared_ptr<PriorityEditor> editor) {
    std::vector<std::wstring> labels;std::vector<unsigned> ids;auto &entries=editor->entries();
    for(unsigned i=0;i<entries.size();++i){ids.push_back(i);labels.push_back(std::to_wstring(i+1)+L". "+priority_name(entries[i]));}editor->operation=99;
    auto request=unified_dialogs::choices(editor->profile==0?L"무력형 전법 우선순위 (무력 ≥ 지력)":L"지력형 전법 우선순위 (지력 > 무력)",labels,ids,false,{},
        [editor](bool ok,const std::vector<unsigned> &){if(!context_valid())return;editor->dialog=nullptr;
            if(!ok || editor->operation==99){priority_menu(editor);return;}unsigned operation=editor->operation,index=editor->index;
            if(operation==0){priority_add(editor);return;}auto &v=editor->entries();if(index>=v.size()){priority_order(editor);return;}
            if(operation==1){v.erase(v.begin()+index);priority_order(editor);return;}
            Picker p;p.multiple=false;p.title=L"전법 순서 이동";p.ids={0,1};p.labels={L"위로 이동",L"아래로 이동"};
            pick(std::move(p),[editor,index](bool accepted,const std::vector<unsigned> &direction){if(!context_valid())return;auto &items=editor->entries();
                if(accepted && !direction.empty() && index<items.size()){
                    if(direction[0]==0 && index>0)std::swap(items[index],items[index-1]);else if(direction[0]==1 && index+1<items.size())std::swap(items[index],items[index+1]);}priority_order(editor);});
        });
    if(!request)return;
    *reinterpret_cast<void **>(request->column+0x30)=reinterpret_cast<void *>(&unified_dialogs::row_index_compare);
    editor->dialog=request.get();request->button_names={L"추가",L"삭제",L"순서 이동"};
    request->command=[editor](unsigned operation){if(!context_valid() || !editor->dialog)return;
        unified_dialogs::List checked;unified_dialogs::collect(*editor->dialog,checked.value);Link *first=list_first(checked.value);
        if(operation!=0 && (!first || !first->value))return;editor->operation=operation;
        if(first && first->value)editor->index=reinterpret_cast<unified_dialogs::Row *>(first->value)->id;unified_dialogs::close(*editor->dialog,true);};
    unified_dialogs::open(std::move(request));
}
static void priority_menu(std::shared_ptr<PriorityEditor> editor) {
    Picker p;p.multiple=false;p.title=L"출진 전법 자동 선택 설정";p.ids={0,1,2,3,4};
    p.labels={L"무력형 우선순위",L"지력형 우선순위",editor->pending.enabled?L"자동 선택: 켬 (선택하여 끄기)":L"자동 선택: 끔 (선택하여 켜기)",L"설정 저장",L"취소"};
    pick(std::move(p),[editor](bool ok,const std::vector<unsigned> &ids){if(!ok || ids.empty() || !context_valid())return;unsigned id=ids[0];
        if(id<2){editor->profile=id;priority_order(editor);}else if(id==2){editor->pending.enabled=!editor->pending.enabled;priority_menu(editor);}
        else if(id==3){auto &v=editor->pending;
            if(v.martial.empty() || v.intellect.empty()){notice(L"두 유형 모두 우선순위를 하나 이상 지정하세요.",L"전법 우선순위");return;}
            if(!unified_config::save(v) || !tactic_priority::set(1,v.martial.data(),static_cast<unsigned>(v.martial.size()),v.intellect.data(),static_cast<unsigned>(v.intellect.size()),v.enabled?1:0))notice(L"전법 우선순위를 저장하지 못했습니다.",L"설정 저장");}
    });
}
static void edit_priorities() {auto e=std::make_shared<PriorityEditor>();e->pending=unified_config::load();priority_menu(std::move(e));}
}
