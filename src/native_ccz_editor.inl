// Native game list/numeric editors; no HWND or Windows dialog.
#include "ccz_appearance_io.h"
#include "ccz_appearance_labels.h"
#include "native_ui_call_trace.inl"
namespace ccz_settings {
enum Step:unsigned {Root=1,Tier,Gallery,Generals,Masks,Low,High,Cancel,Save};
struct Session {
    ccz_appearance::Settings value;std::wstring before,notice;wchar_t path[MAX_PATH]={};
    ccz_catalog::Reader catalog;unsigned pending=Root,step=Root,formation=1,tier=0,general=0;
    unsigned long long preview_key=~0ull;unified_dialogs::Dialog *dialog=nullptr;
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
static bool layout_existing_list(unified_dialogs::Dialog &r){
    // Keep native ownership and delegates. The frame queue can still refer to
    // these controls: never replace or destroy them from inside on_frame.
    Obj list=pointer(r.object,0x180),bar=pointer(r.object,0x198);if(!list)return false;
    ui_call_trace::memory(ui_call_trace::Attach,r.object+0x180,8);
    ui_call_trace::memory(ui_call_trace::Attach+1000,r.object+0x198,8);
    ui_call_trace::call(ui_call_trace::Move,0x874650,fn<void(__fastcall *)(Obj,int,int)>(0x874650),r.object,0,0);
    ui_call_trace::call(ui_call_trace::Width,0x875F00,fn<void(__fastcall *)(Obj,int)>(0x875F00),r.object,1920);
    ui_call_trace::call(ui_call_trace::Height,0x8740F0,fn<void(__fastcall *)(Obj,int)>(0x8740F0),r.object,1080);
    ui_call_trace::call(ui_call_trace::Move,0x874650,fn<void(__fastcall *)(Obj,int,int)>(0x874650),list,80,150);
    Obj config=pointer(r.object,0x1A8);if(!config)config=r.config;
    int viewport=1000+(((*reinterpret_cast<unsigned *>(config+0x18)>=2) && *reinterpret_cast<unsigned *>(config+0x100))?48:0);
    ui_call_trace::call(ui_call_trace::Width,0x875F00,fn<void(__fastcall *)(Obj,int)>(0x875F00),list,viewport);
    ui_call_trace::call(ui_call_trace::Height,0x8740F0,fn<void(__fastcall *)(Obj,int)>(0x8740F0),list,740);
    if(bar){
        ui_call_trace::call(ui_call_trace::Move,0x874650,fn<void(__fastcall *)(Obj,int,int)>(0x874650),bar,80+viewport+16,230);
        ui_call_trace::call(ui_call_trace::Height,0x8740F0,fn<void(__fastcall *)(Obj,int)>(0x8740F0),bar,660);
    }
    ui_call_trace::call(ui_call_trace::Refresh,0x2FC280,fn<void(__fastcall *)(Obj)>(0x2FC280),r.object);return true;
}
static void queue(unsigned step){if(session){session->dialog=nullptr;session->pending=step;session->preview_key=~0ull;}ccz_preview::hide();}
static bool pick(const std::wstring &title,const std::vector<std::wstring> &names,const std::vector<unsigned> &ids,bool multiple,const std::set<unsigned> &initial,unified_dialogs::PickDone done){
    auto r=ui_call_trace::run(ui_call_trace::Choices,[&]{return unified_dialogs::choices(title,names,ids,multiple,initial,std::move(done));});if(!r)return false;*reinterpret_cast<unsigned *>(r->column+0x10)=1000;
    // 2C0820: positive A4 computes both body viewport and scrollbar from column
    // widths. Changing only the outer CUI width left its inner body at 1400px.
    *reinterpret_cast<unsigned *>(r->config+0xA4)=1;
    if(session->step==Gallery){
        // 2F0A80: A0=0 suppresses the single-select automatic +190 accept call.
        *reinterpret_cast<unsigned *>(r->config+0xA0)=0;
        r->button_names[0]=L"결정";auto dialog=r.get();
        r->command=[dialog](unsigned button){if(button==0)unified_dialogs::close(*dialog,true);};
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
    if(!session)return;if(!ok || chosen.empty()){queue(Cancel);return;}unsigned id=chosen[0];
    if(id<23){session->formation=id;if(id>10 && id!=19 && id<21){session->notice=L"수상·특수 진형의 원작 모형은 유지합니다";queue(Root);}else queue(Tier);}
    else if(id==100)queue(Low);else if(id==101)queue(High);else if(id==102)queue(Generals);
    else if(id==103){session->value=ccz_appearance::Settings{};session->notice=L"기본값 복원됨 · 저장하면 적용됩니다";queue(Root);}
    else if(id==104)queue(Save);
    else if(id==105){session->value.forms[ccz_appearance::general_index(663)]=6;session->notice=L"장료: 어린진·봉시진만 고유 그림";queue(Root);}
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
        const wchar_t *extra[]={L"통솔 첫 경계 변경",L"통솔 두 번째 경계 변경",L"고유 무장 적용 진형",L"기본값 복원",L"저장하고 닫기",L"장료 예시 적용"};for(unsigned i=0;i<6;++i){ids.push_back(100+i);names.push_back(extra[i]);}
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
    if(!session || unified_dialogs::active || !ui_call_trace::call(ui_call_trace::Ready,0x56FE10,fn<int(__fastcall *)(Obj)>(0x56FE10),state))return;
    ui_call_trace::run(ui_call_trace::Release,[]{unified_dialogs::release_closed();});unsigned step=session->pending;if(!step)return;
    if(step==Cancel || step==Save){finish(step==Save);return;}ui_call_trace::run(ui_call_trace::Step,[&]{open_step(step);});
}
static void abandon(){if(!session)return;unified_dialogs::abandon();ccz_preview::hide();session.reset();ui_call_trace::end();}
}
