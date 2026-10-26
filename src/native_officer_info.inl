// The native officer roster (정보 > 무장, CInfoDetailPersonDlg vtable 14AB2C0)
// gains the right-hand filter panel. The native setup (5CC120 -> 2C0820)
// still builds the list, its events and scrollbar; we only hand it a filtered
// source list, our tab/column tables and a narrower width. Refiltering calls
// the dialog's own list slot (+0x288 = 8DF7E0) like its native scope buttons.
namespace unified_native {
static constexpr unsigned kInfoVtable=0x14AB2C0,kInfoTabs=0x1A30830;
struct InfoToken {uint16_t field;uint32_t member;};
struct InfoState {
    Obj dialog=nullptr,world=nullptr,argument=nullptr,reference=nullptr;
    GameList *source=nullptr;
    GameList filtered{},initial{}; // created on first use: no game call during DLL load
    bool lists=false;
    bool building=false,multi=false,refilter=false,dirty=true;
    unsigned scope=0,matched=0,total=0;
    std::wstring keyword,status;
    std::vector<InfoToken> all,any,exclude;
    std::vector<oq::Record> records;
    std::vector<Obj> objects;
    std::vector<unified_dialogs::Tab> tabs;
};
static InfoState info;
static void info_lists() {
    if(info.lists)return;list_init(info.filtered,0x13436B0);list_init(info.initial,0x13436B0);info.lists=true;
}
static void info_tabs() {
    if(!info.tabs.empty())return;
    // Native officer tabs: column array, caption message id. Keep order/captions.
    for(unsigned i=0;i<16;++i) {
        auto tab=*reinterpret_cast<const unified_dialogs::Tab *>(image+kInfoTabs+i*sizeof(unified_dialogs::Tab));
        if(!tab.columns)break;
        if(i==6)tab.columns=search_tabs[2];  // personality: nine slots
        info.tabs.push_back(tab);
    }
    info.tabs.push_back({search_tabs[11],L"연계",-1,0});
    info.tabs.push_back({nullptr,nullptr,-1,0});
}
static bool info_token_has(const oq::Record &r,const InfoToken &t) {
    const oq::Field *f=search_catalog.find(t.field);if(!f || f->kind!=oq::Kind::Set)return false;
    const auto &s=r.sets[f->slot];return std::binary_search(s.begin(),s.end(),t.member);
}
static bool info_keyword(const oq::Record &r) {
    if(info.keyword.empty())return true;
    std::wstring key=oq::folded(info.keyword);
    auto contains=[&](const std::wstring &s){return oq::folded(s).find(key)!=std::wstring::npos;};
    if((info.scope==0 || info.scope==1) && contains(r.name))return true;
    const uint16_t fields[]={300,302,301};
    for(unsigned i=0;i<3;++i) {
        if(info.scope!=0 && info.scope!=i+2)continue;
        const oq::Field *f=search_catalog.find(fields[i]);if(!f)continue;
        for(uint32_t m:r.sets[f->slot])if(contains(search_member_name(fields[i],m)))return true;
    }
    return false;
}
static bool info_pass(const oq::Record &r) {
    if(!info_keyword(r))return false;
    for(const auto &t:info.all)if(!info_token_has(r,t))return false;
    if(!info.any.empty() && std::none_of(info.any.begin(),info.any.end(),[&](const InfoToken &t){return info_token_has(r,t);}))return false;
    for(const auto &t:info.exclude)if(info_token_has(r,t))return false;
    return oq::matches(search_catalog,search_query,r);
}
static void info_filter() {
    search_prepare_catalog();info.records.clear();info.objects.clear();
    std::set<Obj> seen;
    for(Link *l=info.source?list_first(*info.source):nullptr;l;l=l->next) {
        Obj p=l->value;if(!valid(p) || !seen.insert(p).second)continue;
        info.objects.push_back(p);info.records.push_back(search_snapshot(p));
    }
    info_lists();list_free(info.filtered);list_init(info.filtered,0x13436B0);
    info.matched=0;info.total=static_cast<unsigned>(info.records.size());
    for(size_t i=0;i<info.records.size();++i)if(info_pass(info.records[i])) {
        if(!list_add(info.filtered,info.objects[i]))break;++info.matched;
    }
    search_records=info.records;info.dirty=true;
}
static Obj info_list() {return info.dialog?pointer(info.dialog,0x180):nullptr;}
static void info_collect(GameList &out) {
    Obj list=info_list();if(!list)return;
    unsigned index=dword(list,0x208),count=dword(list,0x20C);Obj bodies=pointer(list,0x130);
    if(!bodies || index>=count)return;Obj body=pointer(bodies,index*8);
    if(body)fn<void(__fastcall *)(Obj,GameList *)>(0x6DD290)(body,&out);
}
static void info_sync_selection() {
    if(!info.multi)return;info_lists();
    unified_dialogs::List checked;info_collect(checked.value);
    for(Link *l=list_first(info.filtered);l;l=l->next)if(valid(l->value))selected.erase(word(l->value,0x10));
    for(Link *l=list_first(checked.value);l;l=l->next)if(serving(l->value))selected.insert(word(l->value,0x10));
}
using InfoSetup=void(__fastcall *)(Obj,Obj *,unsigned,unsigned,int,unsigned,int,unsigned,unsigned);
static void __fastcall info_setup(Obj dialog,Obj *desc,unsigned a,unsigned b,int c,unsigned d,int e,unsigned f,unsigned g) {
    bool ours=enabled && dialog && desc && pointer(dialog,0)==image+kInfoVtable && desc[2]==image+kInfoTabs && desc[0];
    if(ours) {
        if(info.world!=world()) {
            info.scope=0;info.keyword.clear();info.all.clear();info.any.clear();info.exclude.clear();info.multi=false;
            search_query={};search_labels.clear();info.world=world();
        }
        if(info.dialog!=dialog){info.dialog=dialog;info.reference=nullptr;unified_relations::set_reference(nullptr);}
        if(desc[0]!=reinterpret_cast<Obj>(&info.filtered)) {
            info.source=reinterpret_cast<GameList *>(desc[0]);info.argument=desc[1];
        }
        prepare_columns();info_tabs();info_filter();
        desc[0]=reinterpret_cast<Obj>(&info.filtered);
        desc[2]=reinterpret_cast<Obj>(info.tabs.data());
        desc[3]=reinterpret_cast<Obj>(columns);
        info.building=true;
    }
    fn<InfoSetup>(0x5CC120)(dialog,desc,a,b,c,d,e,f,g);
    info.building=false;
}
using InfoListCreate=void(__fastcall *)(Obj,Obj);
static InfoListCreate info_list_original; // relocated 2C0820 entry; set when the hook is prepared
static void __fastcall info_list_create(Obj dialog,Obj config) {
    bool ours=info.building && dialog==info.dialog;
    if(ours) {
        // 2C0820 sizes the list from the first N columns of tab 0 when +A4>0.
        *reinterpret_cast<unsigned *>(config+0xA4)=9;
        if(info.multi){*reinterpret_cast<unsigned *>(config+0x18)=2;*reinterpret_cast<unsigned *>(config+0x1C)=6000;}
    }
    if(info_list_original)info_list_original(dialog,config);else fn<InfoListCreate>(0x2C0820)(dialog,config);
    Obj list=ours?pointer(dialog,0x180):nullptr;
    if(list && info.multi) {
        info_lists();list_free(info.initial);list_init(info.initial,0x13436B0);
        for(Link *l=list_first(info.filtered);l;l=l->next)
            if(valid(l->value) && selected.count(word(l->value,0x10)))list_add(info.initial,l->value);
        Link *begin=list_first(info.initial),*end=nullptr;
        if(begin)fn<void(__fastcall *)(Obj,Link **,Link **)>(0x2EE480)(list,&begin,&end);
    }
}
static bool info_request_refilter() {
    if(!info.dialog)return false;info.refilter=true;info.dirty=true;return true;
}
static void info_refresh() {
    if(!info.dialog || !info.source || pointer(info.dialog,0)!=image+kInfoVtable)return;
    info_sync_selection();
    auto show=*reinterpret_cast<void(__fastcall **)(Obj,Obj,Obj)>(pointer(info.dialog,0)+0x288);
    show(info.dialog,reinterpret_cast<Obj>(info.source),info.argument);
}
static std::wstring info_tokens_text(const std::vector<InfoToken> &tokens) {
    std::wstring s;for(const auto &t:tokens){if(!s.empty())s+=L", ";s+=search_member_name(t.field,t.member);}return s;
}
static void info_publish() {
    if(!info.dirty)return;info.dirty=false;
    officer_panel::Model m;m.scope=info.scope;m.keyword=info.keyword;
    m.all=info_tokens_text(info.all);m.any=info_tokens_text(info.any);m.exclude=info_tokens_text(info.exclude);
    m.advanced=search_query.empty()?L"":search_summary(48);m.matched=info.matched;m.total=info.total;
    m.multi=info.multi;m.selected=static_cast<unsigned>(selected.size());m.status=info.status;
    officer_panel::set_model(m);
}
static void info_pick_tokens(std::vector<InfoToken> *target,const wchar_t *caption) {
    struct Choice {InfoToken token;std::wstring text;unsigned count;};
    std::vector<Choice> choices;
    const uint16_t fields[]={300,302,301};const wchar_t *kinds[]={L"개성",L"진형",L"전법"};
    for(unsigned k=0;k<3;++k) {
        const oq::Field *f=search_catalog.find(fields[k]);if(!f)continue;
        std::map<uint32_t,unsigned> counts;for(const auto &r:info.records)for(uint32_t m:r.sets[f->slot])++counts[m];
        std::vector<Choice> part;
        for(const auto &[m,n]:counts)part.push_back({{fields[k],m},std::wstring(kinds[k])+L": "+search_member_name(fields[k],m),n});
        std::sort(part.begin(),part.end(),[](const Choice &x,const Choice &y){return x.text<y.text;});
        choices.insert(choices.end(),part.begin(),part.end());
    }
    if(choices.empty()){info.status=L"선택할 개성·진형·전법이 없습니다";info.dirty=true;return;}
    Picker p;p.multiple=true;p.title=std::wstring(caption)+L" — 개성·진형·전법 여러 개 선택 (괄호: 무장 수)";
    for(unsigned i=0;i<choices.size();++i) {
        p.ids.push_back(i);p.labels.push_back(choices[i].text+L"  ("+std::to_wstring(choices[i].count)+L")");
        for(const auto &t:*target)if(t.field==choices[i].token.field && t.member==choices[i].token.member)p.initial.insert(i);
    }
    pick(std::move(p),[target,choices](bool ok,const std::vector<unsigned> &ids){
        if(!ok || !context_valid())return;target->clear();
        for(unsigned i:ids)if(i<choices.size())target->push_back(choices[i].token);
        info.refilter=true;info.dirty=true;
    });
}
// Roster presets: panel fields plus the advanced condition tree.
static std::wstring info_save_text() {
    std::wstring s=L"K\t"+std::to_wstring(info.scope)+L"\t"+oq::escape(info.keyword)+L"\n";
    auto list=[&](wchar_t tag,const std::vector<InfoToken> &v){for(const auto &t:v)s+=std::wstring(1,tag)+L"\t"+std::to_wstring(t.field)+L"\t"+std::to_wstring(t.member)+L"\n";};
    list(L'A',info.all);list(L'O',info.any);list(L'X',info.exclude);
    if(!search_query.empty())s+=L"Q\n"+oq::save(search_catalog,search_labels,search_query);
    return s;
}
static bool info_load_text(const std::wstring &text) {
    unsigned scope=0;std::wstring keyword;std::vector<InfoToken> all,any,exclude;oq::Group query;
    auto lines=oq::split(text,L'\n');size_t q=lines.size();
    try {
        for(size_t i=0;i<lines.size();++i) {
            auto t=oq::split(lines[i],L'\t');if(t[0].empty())continue;
            if(t[0]==L"Q"){q=i+1;break;}
            if(t[0]==L"K" && t.size()>=3){scope=std::stoul(t[1])%5;keyword=oq::unescape(t[2]);}
            else if((t[0]==L"A" || t[0]==L"O" || t[0]==L"X") && t.size()>=3) {
                InfoToken token{static_cast<uint16_t>(std::stoul(t[1])),static_cast<uint32_t>(std::stoul(t[2]))};
                if(token.field!=300 && token.field!=301 && token.field!=302)continue;
                (t[0]==L"A"?all:t[0]==L"O"?any:exclude).push_back(token);
            }
        }
    } catch(...) {return false;}
    if(q<lines.size()) {
        std::wstring rest;for(size_t i=q;i<lines.size();++i)rest+=lines[i]+L"\n";
        unsigned dropped=0;if(!oq::load(search_catalog,search_labels,rest,query,dropped))return false;
    }
    info.scope=scope;info.keyword=keyword;info.all=all;info.any=any;info.exclude=exclude;search_query=query;return true;
}
static std::wstring info_preset_path() {
    auto path=search_preset_path();auto slash=path.find_last_of(L"\\/");
    return slash==std::wstring::npos?path:path.substr(0,slash)+L"\\officer_roster_presets.txt";
}
static void info_presets() {
    auto read_all=[]() {
        std::vector<std::pair<std::wstring,std::wstring>> out;auto path=info_preset_path();
        HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        if(file==INVALID_HANDLE_VALUE)return out;
        std::string bytes;LARGE_INTEGER size{};
        if(GetFileSizeEx(file,&size) && size.QuadPart>0 && size.QuadPart<(1<<20)) {
            bytes.resize(static_cast<size_t>(size.QuadPart));DWORD got=0;
            if(!ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&got,nullptr))got=0;bytes.resize(got);
        }
        CloseHandle(file);std::wstring name,body;bool inside=false;
        for(const auto &line:oq::split(search_utf16(bytes),L'\n')) {
            if(!inside && line.rfind(L"#PRESET\t",0)==0){name=oq::unescape(line.substr(8));body.clear();inside=true;}
            else if(inside && line==L"#END"){out.push_back({name,body});inside=false;}
            else if(inside)body+=line+L"\n";
        }
        return out;
    };
    auto presets=read_all();
    Picker p;p.multiple=false;p.title=L"무장 선별 프리셋 — 같은 이름으로 저장하면 덮어씁니다";
    p.ids.push_back(1000);p.labels.push_back(numbered(1,L"현재 조건 저장 (이름 입력)"));
    for(unsigned i=0;i<presets.size();++i){p.ids.push_back(i);p.labels.push_back(numbered(i+2,L"불러오기: "+presets[i].first));}
    for(unsigned i=0;i<presets.size();++i){p.ids.push_back(2000+i);p.labels.push_back(numbered(static_cast<unsigned>(presets.size())+i+2,L"삭제: "+presets[i].first));}
    pick(std::move(p),[presets,read_all](bool ok,const std::vector<unsigned> &ids) mutable {
        if(!ok || ids.empty() || !context_valid())return;unsigned id=ids[0];
        auto write_all=[](const std::vector<std::pair<std::wstring,std::wstring>> &all) {
            std::wstring text;for(const auto &[name,body]:all)text+=L"#PRESET\t"+oq::escape(name)+L"\n"+body+L"#END\n";
            std::string bytes=search_utf8(text);auto path=info_preset_path();std::wstring temp=path+L".tmp";
            HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
            DWORD written=0;bool done=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr) && written==bytes.size();
            CloseHandle(file);return done && MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
        };
        if(id==1000) {
            unified_dialogs::input([read_all,write_all](bool accepted,const std::wstring &text){
                if(!accepted || !context_valid())return;std::wstring name=search_trim(text);if(name.empty())return;
                auto all=read_all();std::wstring body=info_save_text();bool replaced=false;
                for(auto &e:all)if(e.first==name){e.second=body;replaced=true;}
                if(!replaced){if(all.size()>=30)all.erase(all.begin());all.push_back({name,body});}
                info.status=write_all(all)?L"\""+name+L"\" 저장함":L"프리셋을 저장하지 못했습니다";info.dirty=true;
            });
        } else if(id>=2000 && id-2000<presets.size()) {
            std::wstring name=presets[id-2000].first;presets.erase(presets.begin()+static_cast<std::ptrdiff_t>(id-2000));
            info.status=write_all(presets)?L"\""+name+L"\" 삭제함":L"프리셋을 삭제하지 못했습니다";info.dirty=true;
        } else if(id<presets.size()) {
            info.status=info_load_text(presets[id].second)?L"\""+presets[id].first+L"\" 적용":L"프리셋 형식이 올바르지 않습니다";
            info.refilter=true;info.dirty=true;
        }
    });
}
static void info_action(unsigned action) {
    using namespace officer_panel;
    switch(action) {
    case Advanced:search_records=info.records;search_open_editor({});break;
    case ScopeAll:case ScopeName:case ScopeTrait:case ScopeFormation:case ScopeTactic:
        info.scope=action-ScopeAll;if(!info.keyword.empty())info.refilter=true;info.dirty=true;break;
    case Keyword:unified_dialogs::input([](bool ok,const std::wstring &text){
        if(!ok || !context_valid())return;info.keyword=search_trim(text);info.refilter=true;info.dirty=true;});break;
    case PickAll:info_pick_tokens(&info.all,L"모두 보유");break;
    case PickAny:info_pick_tokens(&info.any,L"하나 이상");break;
    case PickExclude:info_pick_tokens(&info.exclude,L"제외");break;
    case Presets:info_presets();break;
    case Clear:info.keyword.clear();info.all.clear();info.any.clear();info.exclude.clear();search_query={};
        info.status.clear();info.refilter=true;info.dirty=true;break;
    case Multi:info_sync_selection();info.multi=!info.multi;info.refilter=true;info.dirty=true;break;
    case Commands:
        info_sync_selection();
        if(!info.multi){unified_dialogs::List one;info_collect(one.value);selected.clear();
            for(Link *l=list_first(one.value);l;l=l->next)if(serving(l->value))selected.insert(word(l->value,0x10));}
        if(selected.empty()){info.status=L"명령할 아군 무장을 선택해 주세요";info.dirty=true;break;}
        action_menu();break;
    default:break;
    }
}
static int info_absolute(Obj node,unsigned offset) {
    int total=0;for(unsigned depth=0;node && depth<30;++depth){total+=static_cast<int>(dword(node,offset));node=pointer(node,8);}return total;
}
static void __fastcall info_frame(Obj object) {
    fn<void(__fastcall *)(Obj)>(0x2FB440)(object);
    if(!enabled || !mod_features.unified || object!=info.dialog)return;
    ui_thread=GetCurrentThreadId();opened_world=world();opened_force=player_force();force_id=0;
    for(unsigned i=1;i<=51;++i)if(table(0xDF30,i,51)==opened_force)force_id=i;
    unified_dialogs::release_closed();unified_overlay::install();
    Obj list=info_list();
    if(!list){officer_panel::show(false,{});return;}
    int x=info_absolute(list,0x20),y=info_absolute(list,0x24),w=static_cast<int>(dword(list,0x28)),h=static_cast<int>(dword(list,0x2C));
    unified_overlay::Bounds where{x+w+44,y,officer_panel::kWidth,h};
    if(where.x+where.width>1912)where.x=1912-where.width;
    bool busy=unified_dialogs::active!=nullptr;
    officer_panel::show(!busy && w>0 && h>200,where);
    // Link column follows the officer selected in the list, like the sortie screen.
    unified_dialogs::List chosen;info_collect(chosen.value);
    Link *first=list_first(chosen.value);Obj reference=first && valid(first->value)?first->value:nullptr;
    if(reference!=info.reference){info.reference=reference;unified_relations::set_reference(reference);}
    if(!busy) {
        unsigned action=officer_panel::take();if(action)info_action(action);
        if(info.refilter && !unified_dialogs::active){info.refilter=false;info_refresh();}
    }
    info_publish();
}
static Obj __fastcall info_destroy(Obj object,unsigned flags) {
    if(object==info.dialog) {
        officer_panel::show(false,{});officer_panel::take();
        info.dialog=nullptr;info.source=nullptr;info.argument=nullptr;info.reference=nullptr;info.refilter=false;
        unified_relations::set_reference(nullptr);
    }
    return fn<Obj(__fastcall *)(Obj,unsigned)>(0x8C6150)(object,flags);
}
static void __fastcall info_leave(Obj object,Obj context) {
    if(object==info.dialog){officer_panel::show(false,{});officer_panel::take();}
    fn<void(__fastcall *)(Obj,Obj)>(0x8FD320)(object,context);
}
}
