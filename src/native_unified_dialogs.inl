// Native requests are asynchronous. Keep source lists, columns, output pointers,
// callbacks and text alive until the game has left the corresponding state.
#include <functional>
#include <memory>
#include <string>
namespace unified_dialogs {
struct List {
    GameList value;
    List(){list_init(value,0x13436B0);}
    ~List(){list_free(value);}
    List(const List &)=delete;List &operator=(const List &)=delete;
};
struct Request;
struct Completion {void **vtable;Request *request;};
static void *completion_vtable[6];
struct Request {
    Request *parent=nullptr;
    bool completed=false,delivered=false;
    std::function<void(bool)> done;
    virtual ~Request()=default;
};
static Request *active;
// Explicit UI-thread disposal only. CRT/DLL detach must never call the game.
// The process reclaims this small owner if it exits with a dialog still open.
static auto &retained=*new std::vector<std::unique_ptr<Request>>;
static Completion *__fastcall copy_completion(Completion *self,Completion *out){if(out)*out=*self;return out;}
static void __fastcall complete(Completion *self,Obj *state) {
    Request *r=self->request;if(!r)return;r->delivered=true;if(r->completed)return;
    r->completed=true;active=r->parent;
    bool accepted=state && *state && dword(*state,8)==0x7FFFFFFD;
    auto callback=r->done;if(callback)callback(accepted);
}
static void initialize_completion() {
    completion_vtable[0]=reinterpret_cast<void *>(&copy_completion);
    completion_vtable[1]=reinterpret_cast<void *>(&copy_completion);
    completion_vtable[2]=reinterpret_cast<void *>(&complete);
    completion_vtable[3]=reinterpret_cast<void *>(fn<Obj(__fastcall *)()>(0x7AEC20));
    completion_vtable[4]=reinterpret_cast<void *>(fn<void(__fastcall *)(Obj,char)>(0x90B5D0));
    completion_vtable[5]=reinterpret_cast<void *>(fn<Obj(__fastcall *)(Obj)>(0x7FEB90));
}
template<class T>static T *retain(std::unique_ptr<T> value) {
    T *r=value.get();r->parent=active;active=r;retained.push_back(std::move(value));return r;
}
static void completion_storage(unsigned char *storage,Request *r) {
    initialize_completion();memset(storage,0,64);
    *reinterpret_cast<Completion *>(storage)={completion_vtable,r};
    *reinterpret_cast<Obj *>(storage+56)=storage;
}
struct Tab {const int *columns;const wchar_t *name;int message,padding;};
static_assert(sizeof(Tab)==24,"native list tab ABI");
struct Row {unsigned id;std::wstring label;};
static const wchar_t *__fastcall row_text(Obj p){return reinterpret_cast<Row *>(p)->label.c_str();}
static int __fastcall row_compare(Obj a,Obj b){return wcscmp(row_text(a),row_text(b));}
static int __fastcall row_index_compare(Obj a,Obj b) {
    unsigned x=reinterpret_cast<Row *>(a)->id,y=reinterpret_cast<Row *>(b)->id;return (x>y)-(x<y);
}
struct Dialog:Request {
    alignas(8) unsigned char config[0x400]={};
    alignas(8) unsigned char column[0x60]={};
    std::wstring title;
    std::vector<Row> rows;
    std::vector<Tab> tabs;
    int column_ids[2]={0,-1};
    List source,initial,output;
    Obj object=nullptr;
    Obj scrollbar=nullptr;
    bool standalone_list=false;
    unsigned channel=0;
    std::function<void(unsigned)> command;
    std::array<std::wstring,3> button_names;
    // Called once per UI frame while this dialog exists; argument: it is the top request.
    std::function<void(Dialog &,bool)> on_frame;
    // A native child dialog, without another modal state or global channel.
    std::unique_ptr<Dialog> sidebar;
    bool initialized=false;
    Dialog(){fn<Obj(__fastcall *)(Obj)>(0x250EA0)(config);initialized=true;}
    ~Dialog(){if(initialized)fn<void(__fastcall *)(Obj)>(0x251A10)(config);}
};
static bool owns(const Dialog &r) {
    if(!r.object || pointer(image+0x27D3CA0,r.channel*8)!=r.object)return false;
    Obj config=pointer(r.object,0x1A8);
    return config && pointer(config,8)==reinterpret_cast<Obj>(const_cast<Tab *>(r.tabs.data())) &&
        pointer(config,0x28)==reinterpret_cast<Obj>(const_cast<wchar_t *>(r.title.c_str()));
}
struct Command {void **vtable;Dialog *dialog;unsigned index;};
static void *command_vtable[6];
static Command *__fastcall copy_command(Command *self,Command *out){if(out)*out=*self;return out;}
static void __fastcall command_click(Command *self,Obj *) {
    if(enabled && self->dialog && !self->dialog->completed && self->dialog->command)
        self->dialog->command(self->index);
}
static void set_commands(Dialog &r) {
    command_vtable[0]=reinterpret_cast<void *>(&copy_command);command_vtable[1]=reinterpret_cast<void *>(&copy_command);
    command_vtable[2]=reinterpret_cast<void *>(&command_click);
    command_vtable[3]=reinterpret_cast<void *>(fn<Obj(__fastcall *)()>(0x7AEC20));
    command_vtable[4]=reinterpret_cast<void *>(fn<void(__fastcall *)(Obj,char)>(0x90B5D0));
    command_vtable[5]=reinterpret_cast<void *>(fn<Obj(__fastcall *)(Obj)>(0x7FEB90));
    for(unsigned i=0;i<3;++i)if(!r.button_names[i].empty()) {
        Obj record=r.config+0x310+i*0x50;
        *reinterpret_cast<const wchar_t **>(record)=r.button_names[i].c_str();
        *reinterpret_cast<Command *>(record+8)={command_vtable,&r,i};
        *reinterpret_cast<Obj *>(record+0x40)=record+8;
    }
}
static void configure(Dialog &r) {
    *reinterpret_cast<GameList **>(r.config)=&r.source.value;
    *reinterpret_cast<Obj *>(r.config+8)=reinterpret_cast<Obj>(r.tabs.data());
    *reinterpret_cast<const wchar_t **>(r.config+0x28)=r.title.c_str();
    *reinterpret_cast<GameList **>(r.config+0x30)=&r.initial.value;
    *reinterpret_cast<GameList **>(r.config+0x80)=&r.output.value;
    *reinterpret_cast<unsigned *>(r.config+0x8C)=0;
    set_commands(r);
}
static void dispose_sidebar(Dialog &r) {
    if(!r.sidebar || !r.sidebar->object)return;
    Obj object=r.sidebar->object;r.sidebar->object=nullptr;
    Obj application=fn<Obj(__fastcall *)()>(0x14CB00)();
    Obj context=application?pointer(application,0x210):nullptr;
    auto leave=*reinterpret_cast<void(__fastcall **)(Obj,Obj)>(pointer(object,0)+0x28);
    leave(object,context);
    auto destroy=*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(object,0));destroy(object,1);
    if(r.sidebar->scrollbar){
        Obj bar=r.sidebar->scrollbar;r.sidebar->scrollbar=nullptr;
        auto detach=*reinterpret_cast<void(__fastcall **)(Obj,Obj)>(pointer(bar,0)+0x28);detach(bar,context);
        auto erase=*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(bar,0));erase(bar,1);
    }
}
struct PaneBounds {int x,y,width,height;};
static constexpr PaneBounds roster_bounds{64,200,1200,690},condition_bounds{1344,200,496,690};
static void dispose_control(Obj object) {
    if(!object)return;
    Obj application=fn<Obj(__fastcall *)()>(0x14CB00)();
    Obj context=application?pointer(application,0x210):nullptr;
    auto leave=*reinterpret_cast<void(__fastcall **)(Obj,Obj)>(pointer(object,0)+0x28);leave(object,context);
    auto destroy=*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(object,0));destroy(object,1);
}
static Obj make_data_list(Dialog &r,Obj parent,PaneBounds bounds) {
    Obj object=fn<Obj(__fastcall *)(size_t)>(0x3F6BC0)(0x310);if(!object)return nullptr;
    object=fn<Obj(__fastcall *)(Obj)>(0x2504D0)(object);
    const unsigned settings[][2]={{0x254,0xAC},{0x268,0x110},{0x26C,0x114},{0x270,0x118},{0x274,0x11C},{0x308,0x178}};
    for(const auto &setting:settings)*reinterpret_cast<unsigned *>(object+setting[0])=dword(r.config,setting[1]);
    Link *begin=list_first(r.source.value),*end=nullptr;
    alignas(8) unsigned char callback[64]={};
    unsigned flags=dword(r.config,0x18)>1 && dword(r.config,0x100)?1u:0u;
    // Exact CDataList::Init ABI used by 2C0820; no second fullscreen dialog.
    fn<unsigned(__fastcall *)(Obj,int,int,int,int,Obj,Link **,Link **,Tab *,Obj,Obj,unsigned,Obj)>(0x2BEFE0)
        (object,bounds.x,bounds.y,bounds.width,bounds.height,parent,&begin,&end,r.tabs.data(),
         pointer(r.config,0x10),r.config+0x18,flags,callback);
    Obj bodies=pointer(object,0x130);unsigned count=dword(object,0x20C);
    for(unsigned i=0;bodies && i<count;++i)if(Obj body=pointer(bodies,i*8))
        *reinterpret_cast<uint64_t *>(body+0x170)=*reinterpret_cast<uint64_t *>(r.config+0x18);
    begin=list_first(r.initial.value);
    fn<void(__fastcall *)(Obj,Link **,Link **)>(0x2EE480)(object,&begin,&end);
    fn<void(__fastcall *)(Obj,Obj)>(0x86E100)(object,parent);
    return object;
}
static Obj make_scrollbar(Obj list,Obj parent,PaneBounds bounds) {
    Obj object=fn<Obj(__fastcall *)(size_t)>(0x3F6BC0)(0x1C8);if(!object)return nullptr;
    object=fn<Obj(__fastcall *)(Obj)>(0x84B300)(object);
    auto initialize=*reinterpret_cast<unsigned(__fastcall **)(Obj,int,int,int,Obj)>(pointer(object,0)+0x188);
    initialize(object,bounds.x+bounds.width+8,bounds.y+80,bounds.height-80,parent);
    *reinterpret_cast<Obj *>(list+0x138)=object;
    Obj bodies=pointer(list,0x130);unsigned index=dword(list,0x208),count=dword(list,0x20C);
    if(bodies && index<count)if(Obj body=pointer(bodies,index*8))fn<void(__fastcall *)(Obj)>(0x867930)(body);
    fn<void(__fastcall *)(Obj,Obj)>(0x86E100)(object,parent);
    return object;
}
static bool attach_sidebar(Dialog &r) {
    if(!r.sidebar)return true;
    Dialog &side=*r.sidebar;configure(side);
    Obj left=make_data_list(r,r.object,roster_bounds);if(!left)return false;
    Obj left_bar=make_scrollbar(left,r.object,roster_bounds);
    if(!left_bar){dispose_control(left);return false;}
    side.standalone_list=true;side.object=make_data_list(side,r.object,condition_bounds);
    if(!side.object){dispose_control(left);dispose_control(left_bar);return false;}
    side.scrollbar=make_scrollbar(side.object,r.object,condition_bounds);
    if(!side.scrollbar){dispose_sidebar(r);dispose_control(left);dispose_control(left_bar);return false;}
    Obj old_list=pointer(r.object,0x180),old_bar=pointer(r.object,0x198);
    *reinterpret_cast<Obj *>(r.object+0x180)=left;*reinterpret_cast<Obj *>(r.object+0x198)=left_bar;
    dispose_control(old_list);dispose_control(old_bar);
    // One viewport-sized host, two bounded siblings. Never center another
    // fullscreen dialog inside the roster or inherit its fullscreen backdrop.
    fn<void(__fastcall *)(Obj,int,int)>(0x874650)(r.object,0,0);
    fn<void(__fastcall *)(Obj,int)>(0x875F00)(r.object,1920);
    fn<void(__fastcall *)(Obj,int)>(0x8740F0)(r.object,1080);
    alignas(8) unsigned char changed[64]={};
    *reinterpret_cast<Obj *>(changed)=image+0x13AEFA0;
    *reinterpret_cast<Obj *>(changed+8)=r.object;*reinterpret_cast<Obj *>(changed+56)=changed;
    fn<void(__fastcall *)(Obj,unsigned,Obj)>(0x1A3CB0)(left,0x514,changed);
    fn<void(__fastcall *)(Obj)>(0x2FC280)(r.object);
    return true;
}
static void process_control(Obj object,unsigned offset) {
    if(object){auto process=*reinterpret_cast<void(__fastcall **)(Obj)>(pointer(object,0)+offset);process(object);}
}
static void __fastcall frame(Obj object) {
    fn<void(__fastcall *)(Obj)>(0x2FB440)(object);
    Dialog *owner=nullptr;
    for(auto &request:retained)if(auto r=dynamic_cast<Dialog *>(request.get()))
        if(!r->completed && r->object==object){owner=r;break;}
    if(!owner)return;
    if(owner->sidebar && owner->sidebar->object && active==owner) {
        process_control(owner->sidebar->object,0x10);process_control(owner->sidebar->scrollbar,0x10);
    }
    // May open pickers (retained grows): call after the scan, never inside it.
    if(owner->on_frame && owns(*owner))owner->on_frame(*owner,active==owner);
}
static void __fastcall controls(Obj object) {
    fn<void(__fastcall *)(Obj)>(0x258170)(object);
    for(auto &request:retained)if(auto r=dynamic_cast<Dialog *>(request.get()))
        if(!r->completed && r->object==object && r->sidebar && r->sidebar->object && active==r) {
            process_control(r->sidebar->object,0x38);process_control(r->sidebar->scrollbar,0x38);break;
        }
}
static Obj __fastcall destroy(Obj object,unsigned flags) {
    for(auto &request:retained)if(auto r=dynamic_cast<Dialog *>(request.get()))
        if(r->object==object){dispose_sidebar(*r);break;}
    return fn<Obj(__fastcall *)(Obj,unsigned)>(0x256970)(object,flags);
}
static Dialog *parent_dialog(Request *r) {
    for(;r;r=r->parent)if(auto dialog=dynamic_cast<Dialog *>(r))return dialog;return nullptr;
}
static bool open(std::unique_ptr<Dialog> request) {
    Dialog *r=request.get();Dialog *parent=parent_dialog(active);
    r->channel=parent?parent->channel+1:0;
    // Over a native screen (e.g. the officer roster) start at the first free slot.
    if(!parent)while(r->channel<8 && pointer(image+0x27D3CA0,r->channel*8))++r->channel;
    if(r->channel>=8)return false;
    configure(*r);retain(std::move(request));
    fn<void(__fastcall *)(Obj,int)>(0x45B620)(r->config,static_cast<int>(r->channel));
    r->object=pointer(image+0x27D3CA0,r->channel*8);
    if(!r->object){r->completed=r->delivered=true;active=r->parent;return false;}
    attach_sidebar(*r);
    Obj modal=r->object;
    alignas(8) unsigned char callback[64];completion_storage(callback,r);
    fn<void(__fastcall *)(Obj,Obj,Obj,Obj)>(0x45ED60)
        (fn<Obj(__fastcall *)()>(0x110E0)(),image+0x13F13E8,reinterpret_cast<Obj>(&modal),callback);
    return true;
}
static void collect_list(Dialog &r,GameList &out) {
    if(r.completed || !r.object)return;
    Obj list=r.standalone_list?r.object:pointer(r.object,0x180);if(!list)return;
    unsigned index=dword(list,0x208),count=dword(list,0x20C);
    Obj bodies=pointer(list,0x130);if(!bodies || index>=count)return;
    Obj body=pointer(bodies,index*8);if(body)fn<void(__fastcall *)(Obj,GameList *)>(0x6DD290)(body,&out);
}
static void collect(Dialog &r,GameList &out) {
    if(r.completed || !owns(r))return;collect_list(r,out);
}
static void close(Dialog &r,bool accept) {
    if(r.completed || !owns(r))return;
    collect(r,r.output.value);
    auto finish=*reinterpret_cast<void(__fastcall **)(Obj,unsigned)>(pointer(r.object,0)+0x190);
    finish(r.object,accept?0x7FFFFFFD:0x7FFFFFFE);
}
using PickDone=std::function<void(bool,const std::vector<unsigned> &)>;
static std::unique_ptr<Dialog> choices(const std::wstring &title,const std::vector<std::wstring> &labels,
                 const std::vector<unsigned> &ids,bool multiple,const std::set<unsigned> &initial,PickDone done) {
    if(labels.size()!=ids.size())return nullptr;
    auto r=std::make_unique<Dialog>();Dialog *raw=r.get();r->title=title;r->rows.reserve(ids.size());
    for(unsigned i=0;i<ids.size();++i)r->rows.push_back({ids[i],labels[i]});
    for(auto &row:r->rows) {
        if(!list_add(r->source.value,reinterpret_cast<Obj>(&row)))return nullptr;
        if(initial.count(row.id) && !list_add(r->initial.value,reinterpret_cast<Obj>(&row)))return nullptr;
    }
    memcpy(r->column,image+0x1A2B850+3*0x60,0x60);
    *reinterpret_cast<unsigned *>(r->column)=0;
    *reinterpret_cast<const wchar_t **>(r->column+8)=L"선택 항목";
    *reinterpret_cast<unsigned *>(r->column+0x10)=1200;
    *reinterpret_cast<void **>(r->column+0x18)=reinterpret_cast<void *>(&row_text);
    *reinterpret_cast<void **>(r->column+0x20)=nullptr;*reinterpret_cast<void **>(r->column+0x28)=nullptr;
    *reinterpret_cast<void **>(r->column+0x30)=reinterpret_cast<void *>(&row_compare);
    *reinterpret_cast<void **>(r->column+0x38)=nullptr;*reinterpret_cast<void **>(r->column+0x40)=nullptr;
    *reinterpret_cast<int *>(r->column+0x48)=-1;
    *reinterpret_cast<Obj *>(r->config+0x10)=r->column;
    *reinterpret_cast<unsigned *>(r->config+0x18)=multiple?2:1;
    *reinterpret_cast<unsigned *>(r->config+0x1C)=static_cast<unsigned>(multiple?ids.size():1);
    r->tabs={{r->column_ids,L"목록",-1,0},{nullptr,nullptr,0,0}};
    r->done=[raw,done=std::move(done)](bool accepted){
        std::vector<unsigned> result;
        if(accepted)for(Link *l=list_first(raw->output.value);l;l=l->next)
            if(l->value)result.push_back(reinterpret_cast<Row *>(l->value)->id);
        done(accepted,result);
    };
    return r;
}
static bool pick(const std::wstring &title,const std::vector<std::wstring> &labels,
                 const std::vector<unsigned> &ids,bool multiple,const std::set<unsigned> &initial,PickDone done) {
    auto r=choices(title,labels,ids,multiple,initial,std::move(done));return r && open(std::move(r));
}
struct Number:Request {
    struct Data {int x=650,y=350;Obj style;int minimum,maximum;int *value;} data;
    int value;
    explicit Number(int initial):value(initial){data.style=image+0x13A0FF0;data.value=&value;}
};
static bool number(int initial,int minimum,int maximum,std::function<void(bool,int)> done) {
    if(minimum>maximum || initial<minimum || initial>maximum)return false;
    auto request=std::make_unique<Number>(initial);Number *r=request.get();r->data.minimum=minimum;r->data.maximum=maximum;
    r->done=[r,done=std::move(done)](bool ok){done(ok,r->value);};retain(std::move(request));
    alignas(8) unsigned char callback[64];completion_storage(callback,r);
    fn<void(__fastcall *)(Obj,Obj,Obj,Obj)>(0x45E1E0)
        (fn<Obj(__fastcall *)()>(0x110E0)(),image+0x13EAC48,reinterpret_cast<Obj>(&r->data),callback);
    return true;
}
struct Text:Request {
    struct Data {int x=0,y=0,width=1920,height=1080;const char *initial="";int maximum=64,unit=1;
        const char **output;int flags=0,mode=1,limit=1,padding=0;std::string extra;int allow=1,padding2=0;} data;
    const char *value=nullptr;
    Text(){data.output=&value;}
};
static_assert(offsetof(Text::Data,extra)==0x38 && offsetof(Text::Data,allow)==0x58,"native text input ABI");
static std::wstring decode(const char *text) {
    if(!text)return {};
    int length=static_cast<int>(strnlen(text,8192));if(length>=8192)return {};
    UINT page=CP_UTF8;int count=MultiByteToWideChar(page,MB_ERR_INVALID_CHARS,text,length,nullptr,0);
    if(!count && length){page=949;count=MultiByteToWideChar(page,0,text,length,nullptr,0);}
    std::wstring value(static_cast<size_t>(count),L'\0');
    if(count)MultiByteToWideChar(page,page==CP_UTF8?MB_ERR_INVALID_CHARS:0,text,length,value.data(),count);return value;
}
static bool input(std::function<void(bool,const std::wstring &)> done) {
    auto request=std::make_unique<Text>();Text *r=request.get();
    r->done=[r,done=std::move(done)](bool ok){done(ok,ok?decode(r->value):L"");};retain(std::move(request));
    alignas(8) unsigned char callback[64];completion_storage(callback,r);
    fn<void(__fastcall *)(Obj,Obj,Obj,Obj)>(0x464140)
        (fn<Obj(__fastcall *)()>(0x110E0)(),image+0x13EA120,reinterpret_cast<Obj>(&r->data),callback);
    return true;
}
static void abandon() {
    for(auto &r:retained){r->completed=true;r->done={};}active=nullptr;
}
// Call only after the map state has resumed and all our child states have left.
static void release_closed() {
    if(active)return;
    for(auto &r:retained)if(r->delivered)if(auto dialog=dynamic_cast<Dialog *>(r.get())) {
        Obj slot=image+0x27D3CA0+dialog->channel*8;
        if(owns(*dialog)) {
            Obj application=fn<Obj(__fastcall *)()>(0x14CB00)();
            Obj context=application?pointer(application,0x210):nullptr;if(!context)return;
            Obj object=dialog->object;
            dispose_sidebar(*dialog);
            // Same native detach-before-destroy sequence as 45B620.
            auto leave=*reinterpret_cast<void(__fastcall **)(Obj,Obj)>(pointer(object,0)+0x28);
            leave(object,context);
            auto destroy=*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(object,0));destroy(object,1);
            *reinterpret_cast<Obj *>(slot)=nullptr;
        }
    }
    // Abandoning a world invalidates actions immediately, but a delayed native
    // callback still owns its raw context. Keep that tombstone until delivered.
    retained.erase(std::remove_if(retained.begin(),retained.end(),[](const auto &r){return r->delivered;}),retained.end());
}
}
