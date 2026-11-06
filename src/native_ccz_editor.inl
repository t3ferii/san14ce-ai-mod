// Native game list/numeric editors; no HWND or Windows dialog.
#include "game_addresses.h"
#include "ccz_appearance_io.h"
#include "ccz_appearance_labels.h"
#include "native_ui_call_trace.inl"
namespace ccz_settings {
enum Step:unsigned {Root=1,Tier,Gallery,Generals,Masks,Low,High,Cancel,Save};
struct Session {
    ccz_appearance::Settings value;std::wstring before,notice;wchar_t path[MAX_PATH]={};
    ccz_catalog::Reader catalog;unsigned pending=Root,step=Root,formation=1,tier=0,general=0;
    unsigned long long preview_key=~0ull;unified_dialogs::Dialog *dialog=nullptr;
    bool save_requested=false;
};
static std::unique_ptr<Session> session;
static unsigned preview_cell(unsigned code){return code==255?~0u:ccz_appearance::cell(code);}
static std::wstring label(unsigned code){
    if(code==255)return L"원작 모형";
    unsigned cell=ccz_appearance::cell(code),local=cell%4096;std::wstring text=cell>=4096?L"조조전 · ":L"신조조전 · ";
    if(cell<4096 && local<570)text+=ccz_appearance::class_labels[local/3];
    else {bool named=false;if(cell<4096)for(size_t i=0;i<ccz_appearance::general_count;++i)if(ccz_appearance::generals[i].cell==local){text+=ccz_appearance::general_labels[i];named=true;break;}if(!named)text+=L"조형";}
    return text+L" ["+std::to_wstring(local)+L"]";
}
static std::wstring short_label(unsigned code){
    if(code==255)return L"원작 모형";
    std::wstring full=label(code);size_t begin=full.find(L" · ")+3,end=full.rfind(L" [");
    std::wstring name=full.substr(begin,end-begin);
    if(name==L"조형")name+=L" "+std::to_wstring(ccz_appearance::cell(code)%4096);
    return name;
}
static std::wstring range_label(const Session &s,unsigned tier){
    if(tier==0)return s.value.low?L"통솔 0 ~ "+std::to_wstring(s.value.low-1):L"1단계 · 미사용";
    if(tier==1)return L"통솔 "+std::to_wstring(s.value.low)+L" ~ "+std::to_wstring(s.value.high-1);
    return L"통솔 "+std::to_wstring(s.value.high)+L" 이상";
}
static ccz_preview::Picture card(Session &s,unsigned code,const std::wstring &badge){
    auto p=ccz_preview::picture(s.catalog,preview_cell(code),short_label(code));p.badge=badge;
    if(code==255)p.detail=L"원작 3D 모형 유지";
    else {unsigned cell=ccz_appearance::cell(code);p.detail=(cell>=4096?L"조조전":L"신조조전")+std::wstring(L" · 조형 ")+std::to_wstring(cell%4096);}
    return p;
}
static void preview(Session &s,unsigned selected){
    unsigned long long key=(static_cast<unsigned long long>(s.step)<<32)|selected;if(key==s.preview_key)return;s.preview_key=key;
    std::vector<ccz_preview::Picture> out;std::wstring title;
    if(s.step==Gallery){title=ccz_appearance::formation_labels[s.formation];out.push_back(card(s,selected,range_label(s,s.tier)));}
    else if(s.step==Generals || s.step==Masks){size_t i=s.step==Masks?s.general:ccz_appearance::general_index(selected);if(i>=ccz_appearance::general_count)return;title=ccz_appearance::general_labels[i];out.push_back(card(s,ccz_appearance::code(ccz_appearance::generals[i].cell),L"고유 무장 조형"));}
    else {unsigned f=s.step==Root && selected<23?selected:s.formation;if(f>=23)return;title=ccz_appearance::formation_labels[f];for(unsigned t=0;t<3;++t)out.push_back(card(s,s.value.mapping[f][t],range_label(s,t)));}
    std::wstring hint=s.notice.empty()?L"통솔 경계 "+std::to_wstring(s.value.low)+L" / "+std::to_wstring(s.value.high)+L" · 목록에서 항목을 선택하세요":s.notice;
    ccz_preview::set(std::move(title),std::move(out),std::move(hint));
}
static bool gallery_footer(unified_dialogs::Dialog &r){
    Obj footer=pointer(r.object,0x188);if(!footer)return false;
    // The native mode2 footer setup (2C09E0..2C0AAD), independent of list mode.
    // Keep the original footer/list/page objects alive. Only enable its native
    // round button slots; single-select must not inherit mode2's full-capacity lock.
    auto add=fn<void(__fastcall *)(Obj,unsigned,int,int)>(GA(GA_85FF50));
    auto bind=fn<void(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_85C780));
    add(footer,5,0,-1);add(footer,4,1,-1);
    bind(footer,5,0x7FFFFFFD);bind(footer,4,0x7FFFFFFE);
    // Publish the new slots before the modal entrance/resume sees the footer.
    for(unsigned id:{5u,4u}){
        Obj control=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_92CEE0))(footer+0x130,id);
        if(!control)return false;
        *reinterpret_cast<unsigned *>(control+0x40)=(dword(control,0x40)|1u)&~8u;
        fn<void(__fastcall *)(Obj,int)>(GA(GA_873450))(control,255);
        fn<void(__fastcall *)(Obj,int)>(GA(GA_873370))(control,1);
    }
    if(Obj old=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_92CEE0))(footer+0x130,1)){
        *reinterpret_cast<unsigned *>(old+0x40)|=8;
        fn<void(__fastcall *)(Obj,int)>(GA(GA_873450))(old,0);
        fn<void(__fastcall *)(Obj,int)>(GA(GA_873370))(old,1);
    }
    return true;
}
static void root_footer(unified_dialogs::Dialog &r){
    if(!gallery_footer(r))return;
    Obj footer=pointer(r.object,0x188);
    Obj save=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_92CEE0))(footer+0x130,5);
    if(!save)return;
    // Reuse the shared command functor ABI, but bind it to the round button.
    // Root row clicks still navigate immediately; footer confirmation saves
    // the complete draft independently of the currently highlighted row.
    alignas(8) unsigned char callback[64]={};
    *reinterpret_cast<unified_dialogs::Command *>(callback)={unified_dialogs::command_vtable,&r,0};
    *reinterpret_cast<Obj *>(callback+56)=callback;
    fn<void(__fastcall *)(Obj,unsigned)>(GA(GA_869BE0))(save+0x70,0x7FFFFFFE);
    fn<void(__fastcall *)(Obj,unsigned,Obj)>(GA(GA_1A3CB0))(save,0x7FFFFFFE,callback);
}
static bool layout_existing_list(unified_dialogs::Dialog &r){
    // Keep native ownership and delegates. The frame queue can still refer to
    // these controls: never replace or destroy them from inside on_frame.
    Obj list=pointer(r.object,0x180),bar=pointer(r.object,0x198);if(!list)return false;
    ui_call_trace::memory(ui_call_trace::Attach,r.object+0x180,8);
    ui_call_trace::memory(ui_call_trace::Attach+1000,r.object+0x198,8);
    ui_call_trace::call(ui_call_trace::Move,GA(GA_874650),fn<void(__fastcall *)(Obj,int,int)>(GA(GA_874650)),r.object,0,0);
    ui_call_trace::call(ui_call_trace::Width,GA(GA_875F00),fn<void(__fastcall *)(Obj,int)>(GA(GA_875F00)),r.object,1920);
    ui_call_trace::call(ui_call_trace::Height,GA(GA_8740F0),fn<void(__fastcall *)(Obj,int)>(GA(GA_8740F0)),r.object,1080);
    ui_call_trace::call(ui_call_trace::Move,GA(GA_874650),fn<void(__fastcall *)(Obj,int,int)>(GA(GA_874650)),list,80,90);
    Obj config=pointer(r.object,0x1A8);if(!config)config=r.config;
    int viewport=1000+(((*reinterpret_cast<unsigned *>(config+0x18)>=2) && *reinterpret_cast<unsigned *>(config+0x100))?48:0);
    ui_call_trace::call(ui_call_trace::Width,GA(GA_875F00),fn<void(__fastcall *)(Obj,int)>(GA(GA_875F00)),list,viewport);
    ui_call_trace::call(ui_call_trace::Height,GA(GA_8740F0),fn<void(__fastcall *)(Obj,int)>(GA(GA_8740F0)),list,600);
    // Keep the existing page identity/delegates and constrain its actual hit/paint
    // viewport too; the original title frame ends at y720, above the footer.
    Obj pages=pointer(list,0x130);unsigned index=dword(list,0x208),count=dword(list,0x20C);
    if(pages && index<count){if(Obj body=pointer(pages,index*8)){
        // 86C860 sets the actual clipping drawable (+A0)->size(+F0/F2).
        // The outer CUI height alone leaves that old clipping rectangle intact.
        if(Obj clip=pointer(body,0xA0))
            ui_call_trace::call(ui_call_trace::Height,GA(GA_9303B0),fn<void(__fastcall *)(Obj,int,int)>(GA(GA_9303B0)),clip,viewport,552);
        ui_call_trace::call(ui_call_trace::Width,GA(GA_875F00),fn<void(__fastcall *)(Obj,int)>(GA(GA_875F00)),body,viewport);
        ui_call_trace::call(ui_call_trace::Height,GA(GA_8740F0),fn<void(__fastcall *)(Obj,int)>(GA(GA_8740F0)),body,552);
    }}
    if(bar){
        ui_call_trace::call(ui_call_trace::Move,GA(GA_874650),fn<void(__fastcall *)(Obj,int,int)>(GA(GA_874650)),bar,80+viewport+16,170);
        ui_call_trace::call(ui_call_trace::Height,GA(GA_8740F0),fn<void(__fastcall *)(Obj,int)>(GA(GA_8740F0)),bar,520);
        // 851930 owns a second length at +1A4. 84CE60 lays out the track,
        // thumb and bottom arrow from that length and the updated outer bounds.
        *reinterpret_cast<unsigned *>(bar+0x1A4)=520;
        ui_call_trace::call(ui_call_trace::Refresh,GA(GA_84CE60),fn<void(__fastcall *)(Obj)>(GA(GA_84CE60)),bar);
    }
    ui_call_trace::call(ui_call_trace::Refresh,GA(GA_2FC280),fn<void(__fastcall *)(Obj)>(GA(GA_2FC280)),r.object);return true;
}
static void queue(unsigned step){if(session){session->dialog=nullptr;session->pending=step;session->preview_key=~0ull;}ccz_preview::hide();}
static bool pick(const std::wstring &title,const std::vector<std::wstring> &names,const std::vector<unsigned> &ids,bool multiple,const std::set<unsigned> &initial,unified_dialogs::PickDone done){
    auto r=ui_call_trace::run(ui_call_trace::Choices,[&]{return unified_dialogs::choices(title,names,ids,multiple,initial,std::move(done));});if(!r)return false;*reinterpret_cast<unsigned *>(r->column+0x10)=1000;
    // 2C0820: positive A4 computes both body viewport and scrollbar from column
    // widths. Changing only the outer CUI width left its inner body at 1400px.
    *reinterpret_cast<unsigned *>(r->config+0xA4)=1;
    // 2C0CC4: no one-item tab selector. 2C0F53 -> 2BF781: the explicit
    // empty hover mode uses 45C5F0 rather than the default 2435B0 callback,
    // so 303CB0 never constructs the unrelated officer portrait popup.
    *reinterpret_cast<unsigned *>(r->config+0x9C)=0;
    *reinterpret_cast<unsigned *>(r->config+0x114)=1;
    if(session->step==Root){
        auto dialog=r.get();
        r->command=[dialog](unsigned button){
            if(button || !session || session->dialog!=dialog)return;
            session->save_requested=true;unified_dialogs::close(*dialog,false);
        };
        r->on_created=&root_footer;
    }
    if(session->step==Gallery){
        // Preserve native single selection: mode2/max1 locks all other rows
        // when the saved initial shape is selected. Configure the round footer separately.
        *reinterpret_cast<unsigned *>(r->config+0x18)=1;
        *reinterpret_cast<unsigned *>(r->config+0x1C)=1;
        *reinterpret_cast<unsigned *>(r->config+0x20)=1;
        *reinterpret_cast<unsigned *>(r->config+0x88)=1;
        *reinterpret_cast<unsigned *>(r->config+0xF8)=0; // no multi-select 'select all' command
        *reinterpret_cast<unsigned *>(r->config+0x100)=0;
        *reinterpret_cast<unsigned *>(r->config+0xA0)=0;
        r->on_created=[](unified_dialogs::Dialog &d){
            bool ok=gallery_footer(d);log_event("ccz_footer_created",ok,5,4);
        };
    }
    *reinterpret_cast<const wchar_t **>(r->column+8)=session->step==Root?L"진형별 병종":session->step==Tier?L"통솔 구간별 병종":session->step==Gallery?L"조형 선택":session->step==Masks?L"적용 진형":L"고유 무장";
    auto configured=std::make_shared<bool>(false);
    auto frames=std::make_shared<unsigned>(0);
    r->on_frame=[configured,frames](unified_dialogs::Dialog &d,bool top){
      ui_call_trace::Silence silence(++*frames>3);
      ui_call_trace::run(ui_call_trace::EditorFrame,[&]{
        if(!session || session->dialog!=&d || !top){ccz_preview::hide();return;}
        if(!*configured){*configured=true;if(!ui_call_trace::run(ui_call_trace::Attach,[&]{return layout_existing_list(d);})){session->notice=L"선택창 배치 실패";unified_dialogs::close(d,false);return;}}
        unified_dialogs::List selected;ui_call_trace::run(ui_call_trace::Collect,[&]{unified_dialogs::collect(d,selected.value);});
        unsigned id=session->step==Gallery?session->value.mapping[session->formation][session->tier]:session->step==Generals?ccz_appearance::generals[0].id:session->formation;
        if(session->step==Gallery && id<190)id=ccz_appearance::code(ccz_appearance::cell(id));
        for(Link *l=list_first(selected.value);l;l=l->next)if(l->value)id=reinterpret_cast<unified_dialogs::Row *>(l->value)->id;
        ui_call_trace::run(ui_call_trace::Preview,[&]{preview(*session,id);});ui_call_trace::run(ui_call_trace::Overlay,[]{unified_overlay::install();});
      });
    };
    session->dialog=r.get();if(ui_call_trace::run(ui_call_trace::Open,[&]{return unified_dialogs::open(std::move(r));}))return true;session->dialog=nullptr;queue(Cancel);return false;
}
static void finish(bool save){
    if(!session)return;
    if(save && !ccz_appearance::save_profile(session->path,session->before,session->value)){session->notice=L"저장 실패: 다른 설정창 또는 파일 쓰기 권한을 확인하세요";queue(Root);return;}
    if(save){ccz_module::reload_appearance();log_event("ccz_appearance_saved",session->value.low,session->value.high,2);}
    ccz_preview::hide();session.reset();ui_call_trace::end();
}
static void root_done(bool ok,const std::vector<unsigned> &chosen){
    if(!session)return;
    if(session->save_requested){session->save_requested=false;queue(Save);return;}
    if(!ok || chosen.empty()){queue(Cancel);return;}unsigned id=chosen[0];
    if(id<23){session->formation=id;if(id>10 && id!=19 && id<21){session->notice=L"수상·특수 진형의 원작 모형은 유지합니다";queue(Root);}else queue(Tier);}
    else if(id==100)queue(Low);else if(id==101)queue(High);else if(id==102)queue(Generals);
    else if(id==103){session->value=ccz_appearance::Settings{};session->notice=L"기본값 복원됨 · 저장하면 적용됩니다";queue(Root);}
    else if(id==104)queue(Save);
}
static void open_step(unsigned step){
    if(!session)return;auto &s=*session;s.step=step;s.pending=0;s.preview_key=~0ull;
    std::vector<std::wstring> names;std::vector<unsigned> ids;std::set<unsigned> initial;std::wstring title;
    if(step==Low || step==High){
        ccz_preview::hide();if(!unified_dialogs::number(static_cast<int>(step==Low?s.value.low:s.value.high),0,255,[step](bool ok,int value){if(!session)return;auto &v=session->value;if(ok){unsigned n=static_cast<unsigned>(value);if((step==Low && n<v.high) || (step==High && n>v.low)){if(step==Low)v.low=n;else v.high=n;}else session->notice=L"첫 통솔 경계는 두 번째 경계보다 작아야 합니다";}queue(Root);}))queue(Root);return;
    }
    if(step==Root){
        title=L"조조전 설정";
        for(unsigned f=0;f<23;++f){std::wstring name=ccz_appearance::formation_labels[f];for(unsigned t=0;t<3;++t)name+=(t?L" / ":L" : ")+short_label(s.value.mapping[f][t]);names.push_back(name);ids.push_back(f);}
        const wchar_t *extra[]={L"통솔 첫 경계 변경",L"통솔 두 번째 경계 변경",L"고유 무장 적용 진형",L"기본값 복원",L"저장하고 닫기"};for(unsigned i=0;i<5;++i){ids.push_back(100+i);names.push_back(extra[i]);}
        initial.insert(s.formation);pick(title,names,ids,false,initial,&root_done);
    }else if(step==Tier){
        title=L"병종 설정";for(unsigned t=0;t<3;++t){ids.push_back(t);names.push_back(range_label(s,t)+L" : "+short_label(s.value.mapping[s.formation][t]));}
        pick(title,names,ids,false,{},[](bool ok,const std::vector<unsigned>&chosen){if(!session)return;if(ok && !chosen.empty()){session->tier=chosen[0];queue(Gallery);}else queue(Root);});
    }else if(step==Gallery){
        title=L"조형 선택";
        for(const auto &e:s.catalog.entries){unsigned code=ccz_appearance::code(e.cell);ids.push_back(code);names.push_back(label(code));}ids.push_back(255);names.push_back(L"원작 3D 모형");unsigned code=s.value.mapping[s.formation][s.tier];initial.insert(code<190?ccz_appearance::code(ccz_appearance::cell(code)):code);
        pick(title,names,ids,false,initial,[](bool ok,const std::vector<unsigned>&chosen){if(session && ok && !chosen.empty())session->value.mapping[session->formation][session->tier]=static_cast<unsigned short>(chosen[0]);queue(Tier);});
    }else if(step==Generals){
        title=L"고유 무장";for(size_t i=0;i<ccz_appearance::general_count;++i){ids.push_back(ccz_appearance::generals[i].id);names.push_back(ccz_appearance::general_labels[i]);}
        pick(title,names,ids,false,{},[](bool ok,const std::vector<unsigned>&chosen){if(session && ok && !chosen.empty()){session->general=static_cast<unsigned>(ccz_appearance::general_index(chosen[0]));queue(Masks);}else queue(Root);});
    }else if(step==Masks){
        title=L"적용 진형";for(unsigned f=0;f<8;++f){ids.push_back(f);names.push_back(ccz_appearance::formation_labels[f]);if(s.value.forms[s.general]&(1u<<f))initial.insert(f);}
        pick(title,names,ids,true,initial,[](bool ok,const std::vector<unsigned>&chosen){if(session && ok){unsigned bits=0;for(unsigned f:chosen)if(f<8)bits|=1u<<f;session->value.forms[session->general]=static_cast<unsigned char>(bits);}queue(Generals);});
    }
}
static bool show(){
    if(session || unified_dialogs::active)return false;
    ui_call_trace::start(reinterpret_cast<uintptr_t>(image),[](const char *kind,unsigned a,unsigned b,unsigned c){log_event(kind,a,b,c);});
    auto next=ui_call_trace::run(ui_call_trace::Show,[]{return std::make_unique<Session>();});wchar_t path[MAX_PATH];
    if(!ui_call_trace::run(ui_call_trace::Catalog,[&]{return mod_ini_path(next->path,L"기능설정.ini") && mod_ini_path(path,L"assets\\ccz\\ccz_catalog.bin") && next->catalog.open(path);})){log_event("ccz_catalog_missing");ui_call_trace::end();return false;}
    ui_call_trace::run(ui_call_trace::Profile,[&]{next->before=ccz_appearance::read_profile(next->path);ccz_appearance::decode(next->before.c_str(),next->value);});session=std::move(next);return true;
}
static void tick(Obj state){
    if(!session || unified_dialogs::active || !ui_call_trace::call(ui_call_trace::Ready,GA(GA_56FE10),fn<int(__fastcall *)(Obj)>(GA(GA_56FE10)),state))return;
    ui_call_trace::run(ui_call_trace::Release,[]{unified_dialogs::release_closed();});unsigned step=session->pending;if(!step)return;
    if(step==Cancel || step==Save){finish(step==Save);return;}ui_call_trace::run(ui_call_trace::Step,[&]{open_step(step);});
}
static void abandon(){if(!session)return;unified_dialogs::abandon();ccz_preview::hide();session.reset();ui_call_trace::end();}
}
