// Native command structures were recovered from the supported captured image.
// These functions run inside the map callback's modal UI, never a worker.
namespace unified_native {
struct NativeList {
    GameList value;
    NativeList(){list_init(value,0x13436B0);}
    ~NativeList(){list_free(value);}
    NativeList(const NativeList &)=delete;
    NativeList &operator=(const NativeList &)=delete;
};
struct BatchResult {unsigned changed=0,skipped=0,failed=0,no_orders=0;};
static Obj native_city(unsigned id) {return fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),id);}
static bool owned_city(Obj city) {
    if(!valid(city))return false;
    Obj district=fn<Obj(__fastcall *)(Obj)>(0x22C990)(city);
    return valid(district) && district[0x10]==force_id;
}
static Picker city_picker() {
    Picker p;p.title=L"이동할 거점 선택";p.multiple=false;
    for(unsigned id=1;id<=61;++id)if(owned_city(native_city(id))) {p.ids.push_back(id);p.labels.push_back(city_name(id));}
    return p;
}
static void confirm(unsigned action,unsigned count,unsigned cost,const std::wstring &details,std::function<void(bool)> done) {
    Picker picker;picker.multiple=false;
    picker.title=std::wstring(actions[action])+L" — 대상 "+std::to_wstring(count)+L"명 · 명령서 최대 "+std::to_wstring(cost)+L"개";
    picker.ids={1,0};picker.labels={L"취소",L"실행"};picker.initial.insert(1);
    std::wistringstream lines(details);std::wstring line;
    while(std::getline(lines,line))if(!line.empty()){picker.ids.push_back(static_cast<unsigned>(picker.ids.size()));picker.labels.push_back(line);}
    pick(std::move(picker),[done=std::move(done)](bool ok,const std::vector<unsigned> &ids){done(ok && ids.size()==1 && ids[0]==0);});
}
static std::vector<unified::Officer> chosen_people() {
    std::vector<unified::Officer> result;
    for(unsigned id:selected){Obj p=table(0x158,id,6000);if(serving(p))result.push_back(snapshot(p));}
    return result;
}
static std::wstring names_of(const std::vector<unified::Officer> &chosen) {
    std::wstring result;
    for(unsigned i=0;i<chosen.size() && i<15;++i){if(i)result+=i%5?L", ":L"\n";result+=chosen[i].name;}
    if(chosen.size()>15)result+=L" 외 "+std::to_wstring(chosen.size()-15)+L"명";
    return result;
}
static bool idle(Obj p) {
    return serving(p) && !award_active(p) && p[0x161]==0;
}
static int treasury_gold() {
    Obj city=fn<Obj(__fastcall *)(Obj)>(0x22D3A0)(opened_force);
    if(!valid(city))return -1;
    auto get=*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(city,0)+0x90);return get(city);
}
static bool spend_gold(int amount) {
    Obj city=fn<Obj(__fastcall *)(Obj)>(0x22D3A0)(opened_force);
    int current=treasury_gold();if(!valid(city) || amount<0 || current<amount)return false;
    auto set=*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(city,0)+0x98);set(city,current-amount);return true;
}
static int learning_gold(Obj person) {
    // Same arithmetic as 1F08AC..1F0958, before it changes book stock.
    int base=static_cast<int>(dword(image,0x1A4805C));
    int modifier=fn<int(__fastcall *)(Obj,unsigned,int)>(0x26A9D0)
        (person,0x4C,static_cast<int>(dword(image,0x1A480D4)));
    int ignored=0,foreign=fn<int(__fastcall *)(Obj,unsigned,int *,int)>(0x2AD780)(opened_force,6,&ignored,0);
    int value=base+base*modifier/100+static_cast<int>(dword(image,0x1A48BD8))*base*foreign/100;
    if(opened_force[0x12]&8)value+=static_cast<int>(dword(image,0x1A487B4))*base/100;
    return value;
}
static BatchResult move_people(const std::vector<unified::Officer> &chosen,unsigned city_id) {
    BatchResult result;Obj target=native_city(city_id);
    for(const auto &person:chosen) {
        if(!context_valid() || !owned_city(target)){++result.failed;continue;}
        Obj p=table(0x158,person.id,6000),source=serving(p)?native_city(word(p,0xE4)):nullptr;
        if(!idle(p) || !owned_city(source)){++result.skipped;continue;}
        if(word(p,0xE4)==city_id){++result.skipped;continue;}
        unsigned before=orders();if(!before){++result.no_orders;continue;}
        NativeList list;if(!list_add(list.value,p)){++result.failed;continue;}
        struct Move {GameList people;Obj source,target;} command{list.value,source,target};
        if(!fn<int(__fastcall *)(Move *)>(0x1EFF20)(&command)){++result.skipped;continue;}
        fn<void(__fastcall *)(Move *,int)>(0x1F1270)(&command,1);
        if(award_active(p) || word(p,0xE4)==city_id) {set_orders(before-1);++result.changed;}
        else ++result.failed;
    }
    return result;
}
static BatchResult learn(const std::vector<unified::Officer> &chosen,const std::vector<unsigned> &ids) {
    BatchResult result;NativeList books;
    fn<unsigned(__fastcall *)(Obj,GameList *)>(0x6DB7E0)(opened_force,&books.value);
    std::vector<Obj> objects;std::vector<unified::Book> stock;
    for(Link *l=list_first(books.value);l && objects.size()<30;l=l->next) {
        Obj book=l->value;if(!valid(book))continue;
        unsigned id=fn<unsigned(__fastcall *)(Obj)>(0x22DD70)(book);
        if(std::find(ids.begin(),ids.end(),id)==ids.end() || id<1 || id>30)continue;
        objects.push_back(book);stock.push_back({id,book[0x52],word(opened_force,0x120+id*2)});
    }
    std::vector<unified::Officer> eligible;
    for(const auto &person:chosen) {
        if(idle(table(0x158,person.id,6000)))eligible.push_back(person);
        else result.skipped+=static_cast<unsigned>(stock.size());
    }
    auto plan=unified::plan_learning(eligible,stock);
    result.skipped+=plan.known+plan.stock_missing+plan.capacity_missing;
    for(const auto &task:plan.work) {
        if(!context_valid()){++result.failed;continue;}
        Obj person=table(0x158,task.officer,6000),book=nullptr;
        for(Obj item:objects)if(fn<unsigned(__fastcall *)(Obj)>(0x22DD70)(item)==task.book)book=item;
        if(!idle(person) || !valid(book)){++result.skipped;continue;}
        auto state=snapshot(person);
        if(unified::knows(state,task.tactic) || !word(opened_force,0x120+task.book*2)) {++result.skipped;continue;}
        unsigned before=orders();if(!before){++result.no_orders;continue;}
        int gold=learning_gold(person);if(gold<0 || treasury_gold()<gold){++result.skipped;continue;}
        // 1F0520 accepts {recipient, book, optional replaced tactic, force}.
        Obj command[4]={person,book,nullptr,opened_force};
        int accepted=fn<int(__fastcall *)(Obj *)>(0x1F0520)(command);
        if(accepted && unified::knows(snapshot(person),task.tactic)){set_orders(before-1);++result.changed;}
        else ++result.failed;
    }
    return result;
}
static BatchResult give_items(const std::vector<unified::Officer> &chosen,const std::vector<unsigned> &ids) {
    BatchResult result;award_synchronize();auto items=award_items();unsigned index=0;
    // One physical item has one recipient. Pair distinct chosen items/officers.
    for(const auto &person:chosen) {
        if(index>=ids.size()){++result.skipped;continue;}
        unsigned item_id=ids[index++];Obj item=nullptr;
        for(Obj value:items)if(award_item_id(value)==item_id)item=value;
        Obj p=table(0x158,person.id,6000);
        if(!context_valid() || !serving(p) || !valid(item) || !award_stored(item) || award_storage[item_id]!=force_id || !award_allowed(p,item)) {++result.skipped;continue;}
        unsigned before=orders();if(!before){++result.no_orders;continue;}
        Obj pair[2]={p,item};
        if(award_transfer(pair)){set_orders(before-1);++result.changed;}else ++result.failed;
    }
    return result;
}
static BatchResult oath(const std::vector<unified::Officer> &chosen) {
    BatchResult result;unsigned count=static_cast<unsigned>(chosen.size());
    if(count<2 || count>3){result.skipped=count;return result;}
    std::vector<Obj> members;
    for(const auto &person:chosen) {
        Obj p=table(0x158,person.id,6000);
        if(!idle(p) || valid(table(0x158,word(p,0xBC),6000))){result.skipped=count;return result;}
        for(Obj other:members)if(fn<int(__fastcall *)(Obj,Obj)>(0x232A00)(p,other) ||
            fn<int(__fastcall *)(Obj,Obj)>(0x232A00)(other,p)) {result.skipped=count;return result;}
        members.push_back(p);
    }
    if(orders()<count){result.no_orders=count;return result;}
    // Native 6D0831..6D0868 stores the oldest member's ID in all BC words.
    std::sort(members.begin(),members.end(),[](Obj a,Obj b){
        int x=fn<int(__fastcall *)(Obj)>(0x229E30)(a),y=fn<int(__fastcall *)(Obj)>(0x229E30)(b);
        return x!=y?x<y:word(a,0x10)<word(b,0x10);
    });
    int cost=static_cast<int>(dword(image,0x1A485A8));
    if(!spend_gold(cost)){result.skipped=count;return result;}
    unsigned before=orders();uint16_t representative=word(members.front(),0x10);
    for(Obj p:members)*reinterpret_cast<uint16_t *>(p+0xBC)=representative;
    set_orders(before-count);result.changed=count;return result;
}
static BatchResult group(const std::vector<unified::Officer> &chosen,unsigned slot) {
    BatchResult result;unsigned count=static_cast<unsigned>(chosen.size());
    if(slot>=10 || count>50 || !context_valid()){result.skipped=count;return result;}
    if(orders()<count){result.no_orders=count;return result;}
    Obj options=pointer(world(),0x871E8);if(!options){result.failed=count;return result;}
    for(const auto &p:chosen)if(!serving(table(0x158,p.id,6000))){result.failed=count;return result;}
    // 713640: ten native preset groups, 50 u16 IDs per group, starting at EBA*2.
    Obj output=options+0x1D74+slot*100;bool changed=false;
    for(unsigned i=0;i<50;++i)if(word(output,i*2)!=(i<count?chosen[i].id:0))changed=true;
    if(!changed){result.skipped=count;return result;}
    // 713640 rebuilds the native group dialog from these presets on opening.
    // 6DD290 is the checked-row collector, not a force-group cache accessor.
    unsigned leader=word(image,0x27D3670+slot*2);
    if(std::none_of(chosen.begin(),chosen.end(),[&](const auto &p){return p.id==leader;}))leader=chosen.front().id;
    *reinterpret_cast<uint16_t *>(image+0x27D3670+slot*2)=static_cast<uint16_t>(leader);
    for(unsigned i=0;i<50;++i)*reinterpret_cast<uint16_t *>(output+i*2)=static_cast<uint16_t>(i<count?chosen[i].id:0);
    set_orders(orders()-count);result.changed=count;return result;
}
static BatchResult give_posts(const std::vector<unified::Officer> &,const std::vector<unsigned> &);
static BatchResult trade(const std::vector<unified::Officer> &,unsigned);
static Picker post_picker();
static Picker trade_picker();
static void choose_trade_resources(std::function<void(bool)> done);
static std::wstring trade_summary(unsigned count);
struct ActionRequest {
    unsigned action,estimated;
    std::vector<unified::Officer> chosen;
    std::vector<unsigned> ids;
    std::wstring description;
};
static void confirm_action(std::shared_ptr<ActionRequest> request) {
    auto &r=*request;
    if(r.action==0)r.description+=L"\n선택한 집단의 기존 편제를 교체합니다.";
    if(r.action==1)r.description+=L"\n원작의 의형제 결의 금 비용도 소모합니다.";
    if(r.action==5)r.description+=L"\n전법서·금도 소모합니다. 이미 습득했거나 10칸이 찬 무장은 제외합니다.";
    if(r.action==6)r.description+=trade_summary(static_cast<unsigned>(r.chosen.size()));
    confirm(r.action,static_cast<unsigned>(r.chosen.size()),r.estimated,r.description,[request](bool accepted){
        if(!accepted || !context_valid())return;
        auto &r=*request;BatchResult result;
        switch(r.action) {
        case 0:result=group(r.chosen,r.ids[0]);break;
        case 1:result=oath(r.chosen);break;
        case 2:result=give_posts(r.chosen,r.ids);break;
        case 3:result=give_items(r.chosen,r.ids);break;
        case 4:result=move_people(r.chosen,r.ids[0]);break;
        case 5:result=learn(r.chosen,r.ids);break;
        case 6:result=trade(r.chosen,r.ids[0]);break;
        }
        std::wstring report=L"완료 "+std::to_wstring(result.changed)+L"건 · 제외 "+std::to_wstring(result.skipped)+
            L"건 · 명령서 부족 "+std::to_wstring(result.no_orders)+L"건 · 실패 "+std::to_wstring(result.failed)+L"건";
        if(result.changed)unified_relations::set_reference(unified_relations::reference);
        notice(report,actions[r.action]);refresh_main();
    });
}
static void execute_action(unsigned action) {
    if(!context_valid() || action>=7)return;
    auto request=std::make_shared<ActionRequest>();request->action=action;request->chosen=chosen_people();
    if(request->chosen.empty()){notice(L"작업할 무장을 선택해 주세요.");return;}
    request->estimated=static_cast<unsigned>(request->chosen.size());request->description=names_of(request->chosen);
    Picker picker;
    if(action==0){picker.title=L"편제할 집단 선택 — 최대 50명";picker.multiple=false;
        for(unsigned i=0;i<10;++i){picker.ids.push_back(i);picker.labels.push_back(L"집단 "+std::to_wstring(i+1));}}
    else if(action==1) {
        if(request->chosen.size()<2 || request->chosen.size()>3){notice(L"의형제는 2~3명을 선택해 주세요.",L"의형제");return;}
        confirm_action(request);return;
    }
    else if(action==2)picker=post_picker();
    else if(action==3) {
        picker.title=L"명품 선택 — 목록 순서대로 무장 한 명에 하나씩 수여";award_synchronize();
        for(Obj item:award_items())if(award_stored(item) && award_storage[award_item_id(item)]==force_id) {
            picker.ids.push_back(award_item_id(item));picker.labels.push_back(copy_text(reinterpret_cast<const wchar_t *>(item+0x10)));
        }
    }
    else if(action==4)picker=city_picker();
    else if(action==5) {
        picker.title=L"전법서 복수 선택 — 이미 습득한 전법 제외";NativeList books;
        fn<unsigned(__fastcall *)(Obj,GameList *)>(0x6DB7E0)(opened_force,&books.value);
        for(Link *l=list_first(books.value);l && picker.ids.size()<30;l=l->next)if(valid(l->value)) {
            unsigned id=fn<unsigned(__fastcall *)(Obj)>(0x22DD70)(l->value);if(id<1 || id>30)continue;
            unsigned count=word(opened_force,0x120+id*2);if(!count)continue;
            picker.ids.push_back(id);picker.labels.push_back(label(unified_labels::books,id)+L" × "+std::to_wstring(count));
        }
    }
    else picker=trade_picker();
    if(picker.ids.empty()){notice(L"선택할 수 있는 항목이 없습니다.",actions[action]);return;}
    auto labels=picker.labels;auto ids=picker.ids;
    pick(std::move(picker),[request,labels=std::move(labels),ids=std::move(ids)](bool ok,const std::vector<unsigned> &chosen){
        if(!ok || chosen.empty() || !context_valid())return;request->ids=chosen;
        if(request->action==5)request->estimated*=static_cast<unsigned>(chosen.size());
        request->description+=L"\n";
        for(unsigned id:chosen)for(unsigned i=0;i<ids.size();++i)if(ids[i]==id)request->description+=labels[i]+L"\n";
        if(request->action==6)choose_trade_resources([request](bool accepted){if(accepted && context_valid())confirm_action(request);});
        else confirm_action(request);
    });
}
}
