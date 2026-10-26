// Officer search over a read-only snapshot. Every value comes from the same
// native list column getters the officer roster uses (+0x28 value, +0x18 text),
// or the verified raw slots already used by the personnel batch actions.
// Runs only inside existing UI callbacks; nothing here changes game state.
namespace unified_native {
namespace oq = officer_query;
union NativeColumnPayload {int number;const wchar_t *text;uint64_t raw;};
struct NativeColumnValue {int kind;unsigned padding;NativeColumnPayload value;};
static_assert(sizeof(NativeColumnValue)==16,"native column value ABI (kind 0 int, 1 text, 2 hidden)");
enum class SearchSource : unsigned char {Value,Text,Name,Traits,Tactics,Formations,Friends,Link};
struct SearchField {
    uint16_t id;oq::Kind kind;SearchSource source;short column;
    const wchar_t *name,*category;int minimum,maximum;bool none_label;
};
static constexpr oq::Kind kNum=oq::Kind::Number,kLab=oq::Kind::Label,kSet=oq::Kind::Set,kTxt=oq::Kind::Text;
static const SearchField search_fields[]={
    {1,kTxt,SearchSource::Name,-1,L"이름",L"이름",0,0,false},
    {100,kNum,SearchSource::Value,8,L"통솔",L"능력",0,200,false},
    {101,kNum,SearchSource::Value,9,L"무력",L"능력",0,200,false},
    {102,kNum,SearchSource::Value,10,L"지력",L"능력",0,200,false},
    {103,kNum,SearchSource::Value,11,L"정치",L"능력",0,200,false},
    {104,kNum,SearchSource::Value,12,L"매력",L"능력",0,200,false},
    {200,kLab,SearchSource::Text,1,L"세력",L"기본·소속",0,0,false},
    {201,kLab,SearchSource::Text,2,L"군단",L"기본·소속",0,0,false},
    {202,kLab,SearchSource::Text,3,L"소속 거점",L"기본·소속",0,0,false},
    {203,kLab,SearchSource::Text,4,L"현재 소재",L"기본·소속",0,0,false},
    {204,kLab,SearchSource::Text,5,L"신분",L"기본·소속",0,0,false},
    {205,kLab,SearchSource::Text,35,L"담당",L"기본·소속",0,0,true},
    {206,kLab,SearchSource::Text,21,L"관직",L"기본·소속",0,0,true},
    {110,kNum,SearchSource::Value,6,L"충성",L"기본·소속",0,100,false},
    {111,kNum,SearchSource::Value,23,L"봉록",L"기본·소속",0,99999,false},
    {112,kNum,SearchSource::Value,18,L"상병(부상 정도)",L"기본·소속",0,10,false},
    {113,kNum,SearchSource::Value,20,L"명품 수",L"기본·소속",0,99,false},
    {210,kLab,SearchSource::Text,29,L"성별",L"개인·관계",0,0,false},
    {120,kNum,SearchSource::Value,28,L"연령",L"개인·관계",0,150,false},
    {211,kLab,SearchSource::Text,24,L"주의",L"개인·관계",0,0,false},
    {212,kLab,SearchSource::Text,30,L"부친",L"개인·관계",0,0,true},
    {213,kLab,SearchSource::Text,31,L"모친",L"개인·관계",0,0,true},
    {214,kLab,SearchSource::Text,32,L"배우자",L"개인·관계",0,0,true},
    {215,kLab,SearchSource::Text,33,L"의형제(맏형)",L"개인·관계",0,0,true},
    {303,kSet,SearchSource::Friends,-1,L"친애 무장",L"개인·관계",0,0,false},
    {300,kSet,SearchSource::Traits,-1,L"개성",L"개성",0,0,false},
    {301,kSet,SearchSource::Tactics,-1,L"전법",L"전법",0,0,false},
    {302,kSet,SearchSource::Formations,-1,L"진형",L"진형",0,0,false},
    {130,kNum,SearchSource::Value,193,L"공로",L"공로·정책",0,65535,false},
    {224,kLab,SearchSource::Text,194,L"공로 랭크",L"공로·정책",0,0,false},
    {131,kNum,SearchSource::Value,196,L"지휘병사",L"공로·정책",0,99999,false},
    {132,kNum,SearchSource::Value,198,L"정책 Lv",L"공로·정책",0,99,false},
    {222,kLab,SearchSource::Text,98,L"정책",L"공로·정책",0,0,true},
    {223,kLab,SearchSource::Text,97,L"부서",L"공로·정책",0,0,true},
    {220,kLab,SearchSource::Text,37,L"임무",L"임무·집단",0,0,true},
    {221,kLab,SearchSource::Text,86,L"임무 목표",L"임무·집단",0,0,true},
    {151,kNum,SearchSource::Value,88,L"임무 기간",L"임무·집단",0,999,false},
    {150,kNum,SearchSource::Value,34,L"미행동",L"임무·집단",0,999,false},
    {152,kNum,SearchSource::Value,204,L"출진불가 기간",L"임무·집단",0,999,false},
    {225,kLab,SearchSource::Text,185,L"집단",L"임무·집단",0,0,true},
    {140,kNum,SearchSource::Value,186,L"의리",L"내면",0,100,false},
    {141,kNum,SearchSource::Value,187,L"야망",L"내면",0,100,false},
    {142,kNum,SearchSource::Value,188,L"한조",L"내면",0,100,false},
    {143,kNum,SearchSource::Value,189,L"호전",L"내면",0,100,false},
    {144,kNum,SearchSource::Value,190,L"외교",L"내면",0,100,false},
    {145,kNum,SearchSource::Value,191,L"물자",L"내면",0,100,false},
    {146,kNum,SearchSource::Value,192,L"건설",L"내면",0,100,false},
    {304,kSet,SearchSource::Link,-1,L"연계 등급",L"연계(기준 무장)",0,0,false},
};
static const wchar_t *const search_categories[]={L"이름",L"능력",L"기본·소속",L"개인·관계",L"개성",L"전법",L"진형",
    L"공로·정책",L"임무·집단",L"내면",L"연계(기준 무장)"};
static oq::Catalog search_catalog;
static oq::Labels search_labels;
static std::vector<oq::Record> search_records;
static oq::Group search_query;
static bool search_only_selected;
static std::wstring search_status; // one-line message shown in the next roster title
static const SearchField *search_field(uint16_t id) {
    for(const auto &f:search_fields)if(f.id==id)return &f;return nullptr;
}
static void search_prepare_catalog() {
    if(!search_catalog.fields().empty())return;
    for(const auto &f:search_fields)search_catalog.add(f.id,f.kind,f.name,f.category,f.minimum,f.maximum);
}
static Obj search_column_function(int column,unsigned offset) {
    if(column<0 || column>=213)return nullptr;
    Obj get=pointer(image+0x1A2B850+static_cast<unsigned>(column)*0x60,offset);
    return get>=image+0x1000 && get<image+kTextSize+0x1000?get:nullptr;
}
static int64_t search_read_number(Obj p,int column) {
    Obj get=search_column_function(column,0x28);if(!get)return oq::kUnknown;
    NativeColumnValue v{};v.kind=2;
    reinterpret_cast<NativeColumnValue *(__fastcall *)(NativeColumnValue *,Obj)>(get)(&v,p);
    return v.kind==0?static_cast<int64_t>(v.value.number):oq::kUnknown;
}
static std::wstring search_trim(std::wstring s) {
    while(!s.empty() && iswspace(s.back()))s.pop_back();
    size_t at=0;while(at<s.size() && iswspace(s[at]))++at;return s.substr(at);
}
static std::wstring search_member_name(uint16_t field,uint32_t member) {
    const SearchField *f=search_field(field);if(!f)return L"#"+std::to_wstring(member);
    switch(f->source) {
    case SearchSource::Traits:return label(unified_labels::personalities,member);
    case SearchSource::Tactics:return label(unified_labels::tactics,member);
    case SearchSource::Formations:return label(unified_labels::formations,member);
    case SearchSource::Friends:return officer_name(table(0x158,member,6000));
    case SearchSource::Link:return unified_relations::grade_text(static_cast<int>(member));
    default:return search_labels.text(field,member);
    }
}
static oq::Record search_snapshot(Obj object) {
    oq::Record r=search_catalog.blank();r.id=word(object,0x10);r.name=officer_name(object);
    for(const auto &f:search_fields) {
        const oq::Field *field=search_catalog.find(f.id);if(!field)continue;
        switch(f.source) {
        case SearchSource::Name:break;
        case SearchSource::Value:r.numbers[field->slot]=search_read_number(object,f.column);break;
        case SearchSource::Text: {
            std::wstring text=search_trim(column_text(object,static_cast<unsigned>(f.column)));
            if(text.empty() && f.none_label)text=L"없음";
            r.labels[field->slot]=search_labels.intern(f.id,text);break;
        }
        case SearchSource::Traits: {
            auto &s=r.sets[field->slot];
            for(unsigned i=0;i<9;++i){unsigned t=word(object,0x118+i*2);
                if(t && t<sizeof(unified_labels::personalities)/sizeof(*unified_labels::personalities))s.push_back(t);}
            break;
        }
        case SearchSource::Tactics: {
            auto &s=r.sets[field->slot];for(unsigned i=0;i<10;++i){unsigned t=object[0x130+i];if(t && t<=200)s.push_back(t);}break;
        }
        case SearchSource::Formations: {
            auto &s=r.sets[field->slot];
            for(unsigned i=1;i<=14;++i)if(fn<int(__fastcall *)(Obj,unsigned)>(0x232350)(object,i))s.push_back(i);break;
        }
        case SearchSource::Friends: {
            auto &s=r.sets[field->slot];
            for(unsigned i=0;i<8;++i){unsigned id=word(object,0xC0+i*2);if(id && id<=6000 && valid(table(0x158,id,6000)))s.push_back(id);}
            break;
        }
        case SearchSource::Link:
            if(unified_relations::reference && unified_relations::comparable(object)) {
                int grade=unified_relations::get(object).native.grade;
                if(grade>=1 && grade<=5)r.sets[field->slot].push_back(static_cast<uint32_t>(grade));
            }
            break;
        }
    }
    for(auto &s:r.sets){std::sort(s.begin(),s.end());s.erase(std::unique(s.begin(),s.end()),s.end());}
    return r;
}
static void search_capture() {
    search_prepare_catalog();search_records.clear();std::set<unsigned> alive;
    for(unsigned id=1;id<=6000;++id) {
        Obj object=table(0x158,id,6000);if(!serving(object) || !alive.insert(word(object,0x10)).second)continue;
        search_records.push_back(search_snapshot(object));
    }
    for(auto i=selected.begin();i!=selected.end();)if(!alive.count(*i))i=selected.erase(i);else ++i;
}
static std::vector<unsigned> search_results() {
    std::vector<unsigned> out;
    for(size_t i:oq::evaluate(search_catalog,search_query,search_records)) {
        unsigned id=search_records[i].id;if(!search_only_selected || selected.count(id))out.push_back(id);
    }
    return out;
}
static std::wstring search_summary(size_t limit=70) {
    std::wstring s=oq::describe(search_catalog,search_query,&search_member_name);
    if(s.size()>limit)s=s.substr(0,limit)+L"…";return s;
}
static std::wstring numbered(unsigned n,const std::wstring &text) {
    wchar_t prefix[8];swprintf(prefix,8,L"%02u. ",n);return prefix+text;
}

// ---- Condition editor -------------------------------------------------------
using SearchPath=std::vector<size_t>;
static void search_open_editor(SearchPath path);
static void search_store(const SearchPath &path,int replace,oq::Condition c) {
    oq::Group *g=oq::at(search_query,path);
    if(g) {
        if(replace>=0 && static_cast<size_t>(replace)<g->conditions.size())g->conditions[static_cast<size_t>(replace)]=std::move(c);
        else g->conditions.push_back(std::move(c));
    }
    search_open_editor(g?path:SearchPath{});
}
static int64_t search_median(const oq::Field &f) {
    std::vector<int64_t> v;for(const auto &r:search_records)if(r.numbers[f.slot]!=oq::kUnknown)v.push_back(r.numbers[f.slot]);
    if(v.empty())return f.minimum;std::nth_element(v.begin(),v.begin()+v.size()/2,v.end());return v[v.size()/2];
}
static void search_number(SearchPath path,int replace,uint16_t id) {
    const oq::Field *f=search_catalog.find(id);if(!f)return;
    Picker p;p.multiple=false;p.title=f->name+L" 조건 방식";
    const wchar_t *modes[]={L"값 이상",L"값 이하",L"값과 같음",L"범위 (최솟값 ~ 최댓값)",L"값이 공개된 무장만 (비공개·미확인 제외)"};
    for(unsigned i=0;i<5;++i){p.ids.push_back(i);p.labels.push_back(numbered(i+1,modes[i]));}
    pick(std::move(p),[path,replace,id](bool ok,const std::vector<unsigned> &ids){
        if(!context_valid())return;if(!ok || ids.empty()){search_open_editor(path);return;}
        const oq::Field *info=search_catalog.find(id);if(!info)return;
        oq::Condition c;c.field=id;unsigned mode=ids[0];
        if(mode==4){c.op=oq::Op::Known;search_store(path,replace,c);return;}
        c.op=mode==0?oq::Op::AtLeast:mode==1?oq::Op::AtMost:mode==2?oq::Op::Equal:oq::Op::Between;
        int initial=static_cast<int>((std::min<int64_t>)((std::max<int64_t>)(search_median(*info),info->minimum),info->maximum));
        unified_dialogs::number(initial,info->minimum,info->maximum,[path,replace,c,id](bool accepted,int value) mutable {
            if(!context_valid())return;if(!accepted){search_open_editor(path);return;}
            c.a=value;
            if(c.op!=oq::Op::Between){search_store(path,replace,c);return;}
            const oq::Field *bounds=search_catalog.find(id);if(!bounds)return;
            unified_dialogs::number(bounds->maximum,value,bounds->maximum,[path,replace,c](bool second,int high) mutable {
                if(!context_valid())return;if(!second){search_open_editor(path);return;}
                c.b=high;search_store(path,replace,c);
            });
        });
    });
}
struct SearchChoice {uint32_t member;std::wstring text;unsigned count;};
static std::vector<SearchChoice> search_choices(const oq::Field &f) {
    std::map<uint32_t,unsigned> counts;
    for(const auto &r:search_records) {
        if(f.kind==oq::Kind::Label){if(r.labels[f.slot])++counts[r.labels[f.slot]];}
        else for(uint32_t m:r.sets[f.slot])++counts[m];
    }
    std::vector<SearchChoice> out;
    for(const auto &[member,count]:counts)out.push_back({member,search_member_name(f.id,member),count});
    std::sort(out.begin(),out.end(),[](const SearchChoice &a,const SearchChoice &b){return a.text<b.text;});
    return out;
}
static void search_values(SearchPath path,int replace,uint16_t id,std::vector<uint32_t> initial) {
    const oq::Field *f=search_catalog.find(id);if(!f)return;
    auto choices=search_choices(*f);
    if(choices.empty()){search_status=f->name+L" 값이 있는 무장이 없습니다";search_open_editor(path);return;}
    Picker p;p.multiple=true;p.title=f->name+L" — 값 여러 개 선택 가능 (괄호: 해당 무장 수)";
    for(unsigned i=0;i<choices.size();++i) {
        p.ids.push_back(i);p.labels.push_back(choices[i].text+L"  ("+std::to_wstring(choices[i].count)+L")");
        if(std::find(initial.begin(),initial.end(),choices[i].member)!=initial.end())p.initial.insert(i);
    }
    pick(std::move(p),[path,replace,id,choices](bool ok,const std::vector<unsigned> &ids){
        if(!context_valid())return;if(!ok || ids.empty()){search_open_editor(path);return;}
        oq::Condition c;c.field=id;
        for(unsigned i:ids)if(i<choices.size())c.values.push_back(choices[i].member);
        const oq::Field *info=search_catalog.find(id);if(!info)return;
        Picker mode;mode.multiple=false;mode.title=info->name+L" 조건 방식";
        if(info->kind==oq::Kind::Label) {
            mode.ids={0,1};mode.labels={numbered(1,L"선택한 값 중 하나"),numbered(2,L"선택한 값이 아님")};
        } else {
            mode.ids={0,1,2};mode.labels={numbered(1,L"선택한 것 중 하나 이상 보유"),numbered(2,L"선택한 것 모두 보유"),numbered(3,L"선택한 것 하나도 없음")};
        }
        pick(std::move(mode),[path,replace,c](bool accepted,const std::vector<unsigned> &modes) mutable {
            if(!context_valid())return;if(!accepted || modes.empty()){search_open_editor(path);return;}
            const oq::Field *chosen=search_catalog.find(c.field);if(!chosen)return;
            if(chosen->kind==oq::Kind::Label){c.op=oq::Op::AnyOf;c.negate=modes[0]==1;}
            else {c.op=modes[0]==1?oq::Op::HasAll:oq::Op::HasAny;c.negate=modes[0]==2;}
            search_store(path,replace,c);
        });
    });
}
static void search_link(SearchPath path,int replace) {
    if(!unified_relations::available()){notice(L"현재 실행 파일에서 원작 연계 함수가 확인되지 않았습니다.");search_open_editor(path);return;}
    prepare_columns();
    auto request=std::make_unique<unified_dialogs::Dialog>();auto r=request.get();
    r->title=L"연계 기준 무장 선택 — 출진창과 같은 연계 판정을 사용합니다";
    for(const auto &record:search_records){Obj object=table(0x158,record.id,6000);
        if(serving(object) && !list_add(r->source.value,object))return;}
    r->tabs={{search_tabs[0],L"소속",-1,0},{search_tabs[1],L"능력",-1,0},{search_tabs[2],L"개성",-1,0},{nullptr,nullptr,0,0}};
    *reinterpret_cast<Obj *>(r->config+0x10)=reinterpret_cast<Obj>(columns);
    *reinterpret_cast<unsigned *>(r->config+0x18)=1;*reinterpret_cast<unsigned *>(r->config+0x1C)=1;
    r->done=[r,path,replace](bool ok){
        if(!context_valid())return;
        Link *first=ok?list_first(r->output.value):nullptr;
        if(!first || !serving(first->value)){search_open_editor(path);return;}
        unified_relations::set_reference(first->value);search_capture();
        search_values(path,replace,304,{3,4,5});
    };
    unified_dialogs::open(std::move(request));
}
static void search_add(SearchPath path,int replace) {
    Picker p;p.multiple=false;p.title=L"조건 종류 선택";
    for(unsigned i=0;i<sizeof(search_categories)/sizeof(*search_categories);++i){p.ids.push_back(i);p.labels.push_back(numbered(i+1,search_categories[i]));}
    pick(std::move(p),[path,replace](bool ok,const std::vector<unsigned> &ids){
        if(!context_valid())return;if(!ok || ids.empty()){search_open_editor(path);return;}
        const wchar_t *category=search_categories[ids[0]];
        std::vector<uint16_t> members;for(const auto &f:search_fields)if(!wcscmp(f.category,category))members.push_back(f.id);
        auto field_flow=[path,replace](uint16_t id){
            const oq::Field *info=search_catalog.find(id);if(!info)return;
            if(info->kind==oq::Kind::Number)search_number(path,replace,id);
            else if(info->kind==oq::Kind::Text)unified_dialogs::input([path,replace,id](bool accepted,const std::wstring &text){
                if(!context_valid())return;std::wstring t=search_trim(text);
                if(!accepted || t.empty()){search_open_editor(path);return;}
                oq::Condition c;c.field=id;c.op=oq::Op::Contains;c.text=t;search_store(path,replace,c);});
            else if(id==304)search_link(path,replace);
            else search_values(path,replace,id,{});
        };
        if(members.size()==1){field_flow(members[0]);return;}
        Picker q;q.multiple=false;q.title=std::wstring(category)+L" — 항목 선택";
        for(unsigned i=0;i<members.size();++i){q.ids.push_back(members[i]);q.labels.push_back(numbered(i+1,search_catalog.find(members[i])->name));}
        pick(std::move(q),[path,field_flow](bool chosen,const std::vector<unsigned> &fields){
            if(!context_valid())return;if(!chosen || fields.empty()){search_open_editor(path);return;}
            field_flow(static_cast<uint16_t>(fields[0]));
        });
    });
}
static void search_condition_menu(SearchPath path,size_t index) {
    oq::Group *g=oq::at(search_query,path);if(!g || index>=g->conditions.size()){search_open_editor({});return;}
    Picker p;p.multiple=false;p.title=oq::describe(search_catalog,g->conditions[index],&search_member_name);
    p.ids={0,1,2,3};
    p.labels={numbered(1,L"조건 다시 만들기"),numbered(2,g->conditions[index].negate?L"반대 조건(아님) 끄기":L"반대 조건(아님) 켜기"),
              numbered(3,L"조건 삭제"),numbered(4,L"돌아가기")};
    pick(std::move(p),[path,index](bool ok,const std::vector<unsigned> &ids){
        if(!context_valid())return;oq::Group *group=oq::at(search_query,path);
        if(!group || index>=group->conditions.size()){search_open_editor({});return;}
        if(ok && !ids.empty()) {
            if(ids[0]==0){search_add(path,static_cast<int>(index));return;}
            if(ids[0]==1)group->conditions[index].negate=!group->conditions[index].negate;
            if(ids[0]==2)group->conditions.erase(group->conditions.begin()+static_cast<std::ptrdiff_t>(index));
        }
        search_open_editor(path);
    });
}
static void search_apply() {refresh_main();}
static void search_open_editor(SearchPath path) {
    if(!context_valid())return;
    oq::Group *g=oq::at(search_query,path);if(!g){path.clear();g=&search_query;}
    bool root=path.empty();
    size_t matches=oq::evaluate(search_catalog,search_query,search_records).size();
    Picker p;p.multiple=false;
    p.title=(root?L"검색 조건 — 현재 ":L"묶음(괄호) 편집 — 전체 ")+std::to_wstring(matches)+L"명 일치";
    if(!search_status.empty()){p.title+=L" · "+search_status;search_status.clear();}
    unsigned n=1;
    p.ids.push_back(1000);p.labels.push_back(numbered(n++,g->any?L"결합 방식: 하나라도 만족 (OR) — 선택하면 AND로":L"결합 방식: 모두 만족 (AND) — 선택하면 OR로"));
    for(size_t i=0;i<g->conditions.size();++i){p.ids.push_back(static_cast<unsigned>(i));
        p.labels.push_back(numbered(n++,L"조건: "+oq::describe(search_catalog,g->conditions[i],&search_member_name)));}
    for(size_t i=0;i<g->groups.size();++i){p.ids.push_back(500+static_cast<unsigned>(i));
        p.labels.push_back(numbered(n++,L"묶음: "+oq::describe(search_catalog,g->groups[i],&search_member_name,false)));}
    p.ids.push_back(2000);p.labels.push_back(numbered(n++,L"＋ 조건 추가"));
    p.ids.push_back(2001);p.labels.push_back(numbered(n++,g->any?L"＋ 묶음 추가 (묶음 안은 AND)":L"＋ 묶음 추가 (묶음 안은 OR)"));
    if(!root){p.ids.push_back(2004);p.labels.push_back(numbered(n++,g->negate?L"묶음 반대(아님) 끄기":L"묶음 반대(아님) 켜기"));}
    p.ids.push_back(2002);p.labels.push_back(numbered(n++,root?L"모든 조건 지우기":L"이 묶음 삭제"));
    p.ids.push_back(2003);p.labels.push_back(numbered(n++,root?L"검색 적용 — 무장 목록 보기":L"상위 조건으로 돌아가기"));
    pick(std::move(p),[path,root](bool ok,const std::vector<unsigned> &ids){
        if(!context_valid())return;
        if(!ok || ids.empty()){if(root)search_apply();else{SearchPath up=path;up.pop_back();search_open_editor(up);}return;}
        oq::Group *group=oq::at(search_query,path);if(!group){search_open_editor({});return;}
        unsigned id=ids[0];
        if(id==1000){group->any=!group->any;search_open_editor(path);}
        else if(id<500)search_condition_menu(path,id);
        else if(id<1000){SearchPath child=path;child.push_back(id-500);search_open_editor(child);}
        else if(id==2000)search_add(path,-1);
        else if(id==2001){oq::Group child;child.any=!group->any;group->groups.push_back(child);
            SearchPath next=path;next.push_back(group->groups.size()-1);search_add(next,-1);}
        else if(id==2004){group->negate=!group->negate;search_open_editor(path);}
        else if(id==2002) {
            if(root){search_query={};search_open_editor(path);}
            else{SearchPath up=path;size_t last=up.back();up.pop_back();oq::Group *parent=oq::at(search_query,up);
                if(parent && last<parent->groups.size())parent->groups.erase(parent->groups.begin()+static_cast<std::ptrdiff_t>(last));
                search_open_editor(up);}
        }
        else if(id==2003){if(root)search_apply();else{SearchPath up=path;up.pop_back();search_open_editor(up);}}
    });
}

// ---- Presets (UTF-8 text beside the other mod data) -----------------------
static std::wstring search_preset_path() {
    wchar_t module[MAX_PATH];if(!GetModuleFileNameW(self_module,module,MAX_PATH))return {};
    std::wstring path(module);auto slash=path.find_last_of(L"\\/");if(slash==std::wstring::npos)return {};
    return path.substr(0,slash)+L"\\AI_TacticsMod\\data\\officer_search_presets.txt";
}
static std::wstring search_utf16(const std::string &s) {
    if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0);
    std::wstring w(static_cast<size_t>(n),L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),w.data(),n);return w;
}
static std::string search_utf8(const std::wstring &w) {
    if(w.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,w.data(),static_cast<int>(w.size()),nullptr,0,nullptr,nullptr);
    std::string s(static_cast<size_t>(n),'\0');WideCharToMultiByte(CP_UTF8,0,w.data(),static_cast<int>(w.size()),s.data(),n,nullptr,nullptr);return s;
}
static std::vector<std::pair<std::wstring,std::wstring>> search_read_presets() {
    std::vector<std::pair<std::wstring,std::wstring>> out;auto path=search_preset_path();if(path.empty())return out;
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return out;
    std::string bytes;LARGE_INTEGER size{};
    if(GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<(1<<20)) {
        bytes.resize(static_cast<size_t>(size.QuadPart));DWORD read=0;
        if(!ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr))read=0;bytes.resize(read);
    }
    CloseHandle(file);
    std::wstring text=search_utf16(bytes),name,body;bool inside=false;
    for(const auto &line:oq::split(text,L'\n')) {
        if(!inside && line.rfind(L"#PRESET\t",0)==0){name=oq::unescape(line.substr(8));body.clear();inside=true;}
        else if(inside && line==L"#END"){out.push_back({name,body});inside=false;}
        else if(inside)body+=line+L"\n";
    }
    return out;
}
static bool search_write_presets(const std::vector<std::pair<std::wstring,std::wstring>> &presets) {
    auto path=search_preset_path();if(path.empty())return false;
    std::wstring text;for(const auto &[name,body]:presets)text+=L"#PRESET\t"+oq::escape(name)+L"\n"+body+L"#END\n";
    std::string bytes=search_utf8(text);std::wstring temp=path+L".tmp";
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;bool ok=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) && written==bytes.size();
    CloseHandle(file);
    return ok && MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
static void search_save_preset() {
    if(search_query.empty()){notice(L"저장할 검색 조건이 없습니다.",L"검색 프리셋");return;}
    unified_dialogs::input([](bool ok,const std::wstring &text){
        if(!ok || !context_valid())return;std::wstring name=search_trim(text);if(name.empty())return;
        auto presets=search_read_presets();
        std::wstring body=oq::save(search_catalog,search_labels,search_query);
        bool replaced=false;for(auto &p:presets)if(p.first==name){p.second=body;replaced=true;}
        if(!replaced){if(presets.size()>=30)presets.erase(presets.begin());presets.push_back({name,body});}
        notice(search_write_presets(presets)?L"\""+name+L"\" 검색 조건을 저장했습니다.":L"검색 조건을 저장하지 못했습니다.",L"검색 프리셋");
    });
}
static void search_load_preset() {
    auto presets=search_read_presets();
    if(presets.empty()){notice(L"저장된 검색 조건이 없습니다.",L"검색 프리셋");return;}
    Picker p;p.multiple=false;p.title=L"검색 프리셋 불러오기";
    for(unsigned i=0;i<presets.size();++i){p.ids.push_back(i);p.labels.push_back(numbered(i+1,presets[i].first));}
    pick(std::move(p),[presets](bool ok,const std::vector<unsigned> &ids){
        if(!ok || ids.empty() || ids[0]>=presets.size() || !context_valid())return;
        oq::Group loaded;unsigned dropped=0;
        if(!oq::load(search_catalog,search_labels,presets[ids[0]].second,loaded,dropped)){notice(L"검색 조건 형식이 올바르지 않습니다.",L"검색 프리셋");return;}
        search_query=std::move(loaded);
        search_status=L"\""+presets[ids[0]].first+L"\" 불러옴";
        if(dropped)search_status+=L" (없는 값 조건 "+std::to_wstring(dropped)+L"개 제외)";
        refresh_main();
    });
}
}
