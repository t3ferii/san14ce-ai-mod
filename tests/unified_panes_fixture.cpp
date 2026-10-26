// Delayed native queue completions against the production UI adapter.
// Does not load, render, attach to, or modify the game.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include "../src/unified_personnel.h"
using Obj=unsigned char *;
struct Link {Obj value;Link *next;};
struct GameList {Obj vtable;unsigned *handle;};
static std::vector<unsigned char> image_bytes(0x27D3D00);
static Obj image=image_bytes.data();static LONG enabled=1;
static unsigned sequence,checks,config_releases,destroyed;
static std::map<unsigned,std::vector<std::unique_ptr<Link>>> lists;
static void check(bool ok,const char *message){++checks;if(!ok){fprintf(stderr,"%s\n",message);ExitProcess(1);}}
static void list_init(GameList &value,unsigned){value={nullptr,new unsigned(++sequence)};lists.try_emplace(*value.handle);}
static void list_free(GameList &value){if(value.handle){lists.erase(*value.handle);delete value.handle;value.handle=nullptr;}}
static bool list_add(GameList &value,Obj p){auto &nodes=lists[*value.handle];auto node=std::make_unique<Link>();*node={p,nullptr};if(!nodes.empty())nodes.back()->next=node.get();nodes.push_back(std::move(node));return true;}
static Link *list_first(const GameList &value){auto &nodes=lists[*value.handle];return nodes.empty()?nullptr:nodes.front().get();}
static unsigned dword(Obj p,unsigned off){unsigned value;memcpy(&value,p+off,4);return value;}
static Obj pointer(Obj p,unsigned off){Obj value;memcpy(&value,p+off,8);return value;}
static void put(Obj p,unsigned off,Obj value){memcpy(p+off,&value,8);}
static void *mock_function(unsigned);
template<class T>static T fn(unsigned rva){return reinterpret_cast<T>(mock_function(rva));}
#include "../src/native_unified_dialogs.inl"
struct MockDialog {
    alignas(8) unsigned char object[0x400]={},data[0x310]={},body[0x190]={},config[0x400]={};
    Obj bodies[1];std::vector<Obj> checked;bool dead=false,left=false,old_list_dead=false,old_list_left=false;Obj parent=nullptr;unsigned frames=0,controls=0;
};
static std::vector<std::unique_ptr<MockDialog>> dialogs;
struct Job {alignas(8) unsigned char callback[64]={};unsigned kind;Obj data;};
static std::deque<std::unique_ptr<Job>> jobs;
static Obj vtable[64];
static MockDialog *find(Obj p){for(auto &d:dialogs)if(p==d->object || p==d->body || p==d->data)return d.get();return nullptr;}
static Obj __fastcall config_init(Obj p){memset(p,0,0x400);return p;}
static void __fastcall config_destroy(Obj){++config_releases;}
static Obj __fastcall manager(){return image;}
static void __fastcall leave_dialog(Obj p,Obj context){auto d=find(p);check(d && context==image+80,"detach from native UI context before destruction");if(p==d->data)d->old_list_left=true;else d->left=true;}
static void __fastcall destroy_dialog(Obj p,int){auto d=find(p);if(d && p==d->data){check(!d->old_list_dead && d->old_list_left,"original embedded list is disposed once");d->old_list_dead=true;return;}
    check(d && !d->dead && d->left,"native dialog detached and destroyed exactly once");
    if(d->old_list_dead){unified_dialogs::dispose_control(pointer(p,0x180));unified_dialogs::dispose_control(pointer(p,0x198));}
    d->dead=true;++destroyed;}
static void __fastcall close_dialog(Obj p,unsigned key){memcpy(p+0x178,&key,4);}
static Obj __fastcall allocate_dialog(size_t size){check(size==0x310 || size==0x1C8,"panes allocate native lists and scrollbars, never another fullscreen dialog");auto d=std::make_unique<MockDialog>();Obj p=d->object;dialogs.push_back(std::move(d));return p;}
static Obj __fastcall construct_dialog(Obj p){put(p,0,reinterpret_cast<Obj>(vtable));return p;}
static unsigned __fastcall initialize_dialog(Obj p,Obj config){
    auto d=find(p);memcpy(d->config,config,0x400);put(d->object,0,reinterpret_cast<Obj>(vtable));
    put(d->object,0x1A8,d->config);put(d->object,0x180,d->data);d->bodies[0]=d->body;
    put(d->data,0,reinterpret_cast<Obj>(vtable));
    put(d->data,0x130,reinterpret_cast<Obj>(d->bodies));unsigned count=1;memcpy(d->data+0x20C,&count,4);
    auto initial=reinterpret_cast<GameList *>(pointer(config,0x30));
    for(Link *l=list_first(*initial);l;l=l->next)d->checked.push_back(l->value);
    return 1;
}
static void __fastcall create_dialog(Obj config,int channel){
    Obj slot=image+0x27D3CA0+channel*8;if(Obj old=pointer(slot,0)){leave_dialog(old,image+80);destroy_dialog(old,1);}
    auto d=std::make_unique<MockDialog>();Obj p=d->object;dialogs.push_back(std::move(d));initialize_dialog(p,config);put(slot,0,p);
}
static void __fastcall enter_dialog(Obj p,Obj parent){find(p)->parent=parent;}
static void __fastcall move_dialog(Obj p,int x,int y){memcpy(p+0x20,&x,4);memcpy(p+0x24,&y,4);}
static void __fastcall width_dialog(Obj p,int width){memcpy(p+0x28,&width,4);}
static void __fastcall size_dialog(Obj p,int width,int height){width_dialog(p,width);memcpy(p+0x2C,&height,4);}
static void __fastcall height_dialog(Obj p,int height){memcpy(p+0x2C,&height,4);}
static unsigned __fastcall initialize_list(Obj p,int x,int y,int width,int height,Obj parent,Link **begin,Link **end,unified_dialogs::Tab *tabs,Obj columns,Obj selection,unsigned,Obj callback) {
    auto d=find(p);check(d && !*end && tabs && columns && !pointer(callback,56),"native list ABI receives bounded iterators, metadata and empty callable");
    check(width>0 && height>80 && x>=0 && x+width<=1920 && y>=0 && y+height<=1080,"native pane rectangle stays inside one viewport");
    move_dialog(p,x,y);size_dialog(p,width,height);d->parent=parent;d->bodies[0]=d->body;
    put(p,0x130,reinterpret_cast<Obj>(d->bodies));unsigned count=1;memcpy(p+0x20C,&count,4);
    for(Link *l=*begin;l;l=l->next)check(l->value!=nullptr,"source has actual native row objects");
    check(dword(selection,0)<=2,"native selection mode retains its original protocol");return 1;
}
static void __fastcall initialize_checks(Obj p,Link **begin,Link **end) {auto d=find(p);check(!*end,"native initial checks have a bounded end iterator");for(Link *l=*begin;l;l=l->next)d->checked.push_back(l->value);}
static unsigned __fastcall initialize_scrollbar(Obj p,int x,int y,int height,Obj parent){move_dialog(p,x,y);size_dialog(p,24,height);find(p)->parent=parent;return 1;}
static void __fastcall attach_scrollbar(Obj body){check(find(body)!=nullptr,"scrollbar attaches to its own native list body");}
static void __fastcall bind_event(Obj p,unsigned event,Obj callback){check(find(p) && event==0x514 && find(pointer(callback,8)) && pointer(callback,0)==image+0x13AEFA0,"native check-change delegate keeps the one main dialog as owner");}
static void __fastcall count_refresh(Obj){}
static void __fastcall frame_dialog(Obj p){++find(p)->frames;}
static void __fastcall controls_dialog(Obj p){++find(p)->controls;}
static void __fastcall collect_dialog(Obj body,GameList *out){auto d=find(body);check(d && !d->dead,"collect checks from native tab body");
    auto &old=lists[*out->handle];old.clear();for(Obj p:d->checked)list_add(*out,p);}
static void __fastcall enqueue(Obj,Obj,Obj data,Obj callback){
    auto job=std::make_unique<Job>();job->kind=0;job->data=pointer(data,0);
    auto object=reinterpret_cast<unified_dialogs::Completion *>(callback);
    unified_dialogs::copy_completion(object,reinterpret_cast<unified_dialogs::Completion *>(job->callback));jobs.push_back(std::move(job));
}
static void __fastcall enqueue_number(Obj,Obj,Obj data,Obj callback){enqueue(nullptr,nullptr,reinterpret_cast<Obj>(&data),callback);jobs.back()->kind=1;}
static void __fastcall enqueue_text(Obj,Obj,Obj data,Obj callback){enqueue(nullptr,nullptr,reinterpret_cast<Obj>(&data),callback);jobs.back()->kind=2;}
static void *mock_function(unsigned rva){
    switch(rva){case 0x250EA0:return reinterpret_cast<void *>(&config_init);case 0x251A10:return reinterpret_cast<void *>(&config_destroy);
    case 0x45B620:return reinterpret_cast<void *>(&create_dialog);case 0x45ED60:return reinterpret_cast<void *>(&enqueue);
    case 0x45E1E0:return reinterpret_cast<void *>(&enqueue_number);case 0x464140:return reinterpret_cast<void *>(&enqueue_text);
    case 0x6DD290:return reinterpret_cast<void *>(&collect_dialog);case 0x110E0:case 0x14CB00:return reinterpret_cast<void *>(&manager);
    case 0x3F6BC0:return reinterpret_cast<void *>(&allocate_dialog);case 0x250760:return reinterpret_cast<void *>(&construct_dialog);
    case 0x2C0820:return reinterpret_cast<void *>(&initialize_dialog);case 0x86E100:return reinterpret_cast<void *>(&enter_dialog);
    case 0x874650:return reinterpret_cast<void *>(&move_dialog);case 0x875F00:return reinterpret_cast<void *>(&width_dialog);
    case 0x2FB440:return reinterpret_cast<void *>(&frame_dialog);case 0x256970:return reinterpret_cast<void *>(&destroy_dialog);
    case 0x258170:return reinterpret_cast<void *>(&controls_dialog);
    case 0x2504D0:case 0x84B300:return reinterpret_cast<void *>(&construct_dialog);
    case 0x2BEFE0:return reinterpret_cast<void *>(&initialize_list);case 0x2EE480:return reinterpret_cast<void *>(&initialize_checks);
    case 0x8740F0:return reinterpret_cast<void *>(&height_dialog);case 0x867930:return reinterpret_cast<void *>(&attach_scrollbar);
    case 0x1A3CB0:return reinterpret_cast<void *>(&bind_event);case 0x2FC280:return reinterpret_cast<void *>(&count_refresh);
    default:return nullptr;}
}
static void finish(bool accepted,int number=0){
    check(!jobs.empty(),"native request remained queued after the caller returned");auto job=std::move(jobs.front());jobs.pop_front();
    unsigned char state[16]={};unsigned result=accepted?0x7FFFFFFD:0x7FFFFFFE;memcpy(state+8,&result,4);
    if(job->kind==0){auto d=find(job->data);check(d && !d->dead,"queued dialog source is alive");
        auto out=reinterpret_cast<GameList *>(pointer(d->config,0x80));for(Obj p:d->checked)list_add(*out,p);}
    else if(job->kind==1){auto data=reinterpret_cast<unified_dialogs::Number::Data *>(job->data);check(number>=data->minimum && number<=data->maximum,"calculator retains bounds and output pointer");*data->value=number;}
    else {auto data=reinterpret_cast<unified_dialogs::Text::Data *>(job->data);*data->output="\xEC\xA1\xB0\xEC\xA1\xB0";}
    Obj child=state;unified_dialogs::complete(reinterpret_cast<unified_dialogs::Completion *>(job->callback),&child);
    unified_dialogs::complete(reinterpret_cast<unified_dialogs::Completion *>(job->callback),&child);
}
int main(){
    vtable[0]=reinterpret_cast<Obj>(&destroy_dialog);vtable[0x28/8]=reinterpret_cast<Obj>(&leave_dialog);
    vtable[0x190/8]=reinterpret_cast<Obj>(&close_dialog);put(image,0x210,image+80);
    vtable[0x10/8]=reinterpret_cast<Obj>(&frame_dialog);vtable[0x38/8]=reinterpret_cast<Obj>(&controls_dialog);
    vtable[0x188/8]=reinterpret_cast<Obj>(&initialize_scrollbar);
    unsigned calls=0;std::vector<unsigned> result;
    {std::vector<std::wstring> labels={L"낙양",L"허창"};std::vector<unsigned> ids={1,2};
        check(unified_dialogs::pick(L"소속",labels,ids,true,{2},[&](bool ok,const std::vector<unsigned> &selected){check(ok,"accepted native result");++calls;result=selected;}),"native choice request created");}
    check(calls==0 && jobs.size()==1,"opening a native dialog never synchronously consumes its result");
    check(unified_dialogs::retained.size()==1,"retain dialog metadata after caller-local labels are destroyed");
    auto d=find(pointer(image+0x27D3CA0,0));auto source=reinterpret_cast<GameList *>(pointer(d->config,0));
    auto first=reinterpret_cast<unified_dialogs::Row *>(list_first(*source)->value);
    check(first->label==L"낙양" && first->id==1,"queued rows own their Unicode text");finish(true);
    check(calls==1 && result==std::vector<unsigned>{2},"one completion dispatch with selected IDs");
    check(!unified_dialogs::active,"completion restores parent request");
    unified_dialogs::release_closed();check(unified_dialogs::retained.empty() && destroyed==1 && config_releases==1,"release only after state completion");
    bool canceled=false;unified_dialogs::pick(L"취소",{L"항목"},{7},false,{},[&](bool ok,const std::vector<unsigned> &ids){canceled=!ok && ids.empty();});finish(false);
    check(canceled,"cancel never reports a selected action");unified_dialogs::release_closed();
    unified_dialogs::pick(L"소유권",{L"항목"},{9},false,{},[](bool,const std::vector<unsigned> &){});finish(false);
    Obj reused=pointer(image+0x27D3CA0,0);auto foreign=find(reused);
    put(foreign->config,8,image+64);unsigned before=destroyed;
    unified_dialogs::release_closed();
    check(destroyed==before && pointer(image+0x27D3CA0,0)==reused,"do not destroy a reused address owned by another native dialog");
    leave_dialog(reused,image+80);destroy_dialog(reused,1);put(image+0x27D3CA0,0,nullptr);
    int chosen=-1;unified_dialogs::number(50,0,200,[&](bool ok,int value){if(ok)chosen=value;});
    check(chosen==-1,"calculator result is asynchronous");finish(true,175);check(chosen==175,"calculator keeps result storage alive");unified_dialogs::release_closed();
    unified_dialogs::number(50,0,200,[&](bool ok,int value){if(ok)chosen=value;});finish(false,100);
    check(chosen==175,"canceled calculator does not commit its pending value");unified_dialogs::release_closed();
    std::wstring text;unified_dialogs::input([&](bool ok,const std::wstring &value){if(ok)text=value;});finish(true);
    check(text==L"조조","native input completion decodes Korean UTF-8");unified_dialogs::release_closed();
    check(unified_dialogs::decode("\xC1\xB6\xC1\xB6")==L"조조","legacy Korean input fallback");
    unsigned stale_calls=0;unified_dialogs::number(1,0,200,[&](bool,int){++stale_calls;});unified_dialogs::abandon();
    unified_dialogs::release_closed();check(unified_dialogs::retained.size()==1,"world abandonment retains undelivered callback storage");finish(true,12);
    check(stale_calls==0,"abandoned world/session ignores delayed callbacks");unified_dialogs::release_closed();
    check(unified_dialogs::retained.empty(),"release abandoned storage after its delayed callback has arrived");
    check(sizeof(unified_dialogs::Number::Data)==32 && offsetof(unified_dialogs::Number::Data,value)==24,"native calculator record ABI");
    auto main=unified_dialogs::choices(L"무장 일람",{L"조조"},{1},true,{},[](bool,const std::vector<unsigned> &){});
    main->sidebar=unified_dialogs::choices(L"검색 조건",{L"소속 : 전체",L"능력 : 전체"},{0,1},false,{0},[](bool,const std::vector<unsigned> &){});
    auto raw=main.get();check(unified_dialogs::open(std::move(main)),"open two native panes together");
    auto main_window=find(raw->object),side_window=find(raw->sidebar->object);
    check(jobs.size()==1 && unified_dialogs::active==raw,"sidebar shares main modal state without an extra popup");
    check(!pointer(image+0x27D3CA0,8),"sidebar does not occupy the filter popup channel");
    auto roster=pointer(raw->object,0x180);
    check(dword(raw->object,0x20)==0 && dword(raw->object,0x28)==1920,"one host spans the viewport instead of centering another fullscreen window");
    check(side_window->parent==raw->object && raw->sidebar->standalone_list && dword(side_window->object,0x20)==1344 && dword(side_window->object,0x28)==496,"condition pane is a bounded native list directly inside the host");
    check(dword(roster,0x20)==64 && dword(roster,0x28)==1200 && dword(roster,0x20)+dword(roster,0x28)<dword(side_window->object,0x20),"roster is actually narrowed and leaves a visible gap before the search pane");
    check(dword(roster,0x24)==dword(side_window->object,0x24) && dword(roster,0x2C)==dword(side_window->object,0x2C),"both panes share one top edge and height");
    check(pointer(roster,0x138)!=pointer(raw->sidebar->object,0x138),"each pane has its own scrollbar");
    unified_dialogs::frame(raw->object);check(main_window->frames==1 && side_window->frames==1,"both panes receive native frame processing");
    unified_dialogs::controls(raw->object);check(main_window->controls==1 && side_window->controls==1,"both panes register their native list controls");
    {unified_dialogs::List output;unified_dialogs::collect_list(*raw->sidebar,output.value);
        auto row=reinterpret_cast<unified_dialogs::Row *>(list_first(output.value)->value);
        check(row->id==0 && row->label==L"소속 : 전체","side condition selection uses native checked rows");}
    auto parent_job=std::move(jobs.front());jobs.pop_front();
    unified_dialogs::pick(L"소속 선택",{L"허창"},{2},true,{},[](bool,const std::vector<unsigned> &){});
    unified_dialogs::frame(raw->object);check(side_window->frames==1,"condition popup suspends background sidebar processing");
    unified_dialogs::controls(raw->object);check(side_window->controls==1,"condition popup suspends background native control registration");
    finish(false);jobs.push_back(std::move(parent_job));check(unified_dialogs::active==raw,"condition popup returns to the two-pane parent");
    unified_dialogs::frame(raw->object);check(side_window->frames==2,"sidebar resumes after popup closes");
    finish(false);unified_dialogs::release_closed();
    check(side_window->dead && main_window->dead && unified_dialogs::retained.empty(),"both native panes detach and release after closing");
    check(lists.empty(),"all owned game-list handles released");
    printf("{\"status\":\"unified_engine_fixture_passed\",\"checks\":%u,\"game_attached\":false,\"rendering_verified\":false}\n",checks);
}
