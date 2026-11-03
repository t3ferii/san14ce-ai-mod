#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../src/advisor_briefing.h"
namespace briefing_overlay {
using advisor_briefing_core::Rect;
struct Model {std::wstring adviser=L"군사",title=L"열흘 보고",body;unsigned unread=0,page=0,pages=0,generation=0;bool collecting=false,open=false,detail=false;std::vector<std::wstring> names;};
static bool gpu_ready=true,shown=false;static unsigned action=0,ticket=0;static Model displayed;
static bool available(){return gpu_ready;}
static bool is_visible(){return gpu_ready && shown;}
static unsigned take(unsigned &g){g=ticket;unsigned a=action;action=0;return a;}
static void publish(const Model &m,bool show,bool=false){displayed=m;shown=show;}
static void forget(){action=0;}
static int captured_serial=-1,requested_serial=-1;static unsigned portrait_requests;
static bool captured(int serial){return captured_serial==serial;}
static void request_portrait(Rect,int serial){requested_serial=serial;++portrait_requests;}
static bool scene_clean=false;
static void progress_flags(bool show,bool clean,bool update=true){scene_clean=clean;if(update)shown=show;}
static bool report_mask=false;
static void mask_report(bool value){report_mask=value;}
static unsigned context_calls;
static void player_context(bool player){++context_calls;shown=player;if(!player){displayed.open=false;action=0;}}
}
namespace unified_overlay {static void install(){}}
#define TACTICS_FIXTURE
#define ADVISOR_BRIEFING_FIXTURE
#include "../src/native_adapter.cpp"
#include <array>
using namespace advisor_briefing;
static unsigned checks,pops,original_notices,original_enters,original_leaves,report_enters,report_ticks,camera_restores,report_completions,text_calls;
static int calendar_day=1000;static bool income_numbers=true;
static bool income_blank_military=false,income_omit_delta=false,income_blank_balance=false;
static bool second_district=false;
static unsigned tutorial_flag=0;
static int native_message_mode=0;
static bool caller_available=true;
static unsigned time_wait_relays,last_wait_ms,last_wait_skip;
static int *last_wait_result;
static void __fastcall mock_time_wait(unsigned ms,int *result,unsigned skip){++time_wait_relays;last_wait_ms=ms;last_wait_result=result;last_wait_skip=skip;}
static unsigned entry_relays[2]{},entry_pushes=0,entry_yields=0,entry_suspends=0;
static const wchar_t *relayed_text=nullptr;
static int relayed_mode=0,relayed_speaker=0;
static std::array<unsigned char,0x1B4> native_message_context;
static int __fastcall mock_message_mode(){return native_message_mode;}
static void entry_native_effects() {
    Obj c=pointer(image,0x27842A0);
    if(!native_message_mode && (!c || !dword(c,0x1B0)) && caller_available){++entry_suspends;++entry_pushes;++entry_yields;}
}
static void __fastcall mock_message_call(const wchar_t *text,int mode){++entry_relays[0];relayed_text=text;relayed_mode=mode;entry_native_effects();}
static void __fastcall mock_narration_call(int speaker,const wchar_t *text){++entry_relays[1];relayed_text=text;relayed_speaker=speaker;entry_native_effects();}
static unsigned scene_relays[3]{},scene_native_windows;
static unsigned native_picture_shows,native_picture_closes;
static Obj native_picture_target;
static int native_picture_id,native_picture_repeat;
static void __fastcall mock_picture_show(Obj p,int id,int repeat){++native_picture_shows;native_picture_target=p;native_picture_id=id;native_picture_repeat=repeat;}
static void __fastcall mock_picture_close(Obj p){++native_picture_closes;native_picture_target=p;}
static Obj relayed_scene_params;
static unsigned relayed_scene_force,relayed_scene_message,relayed_scene_amount;
static Obj relayed_scene_actor,relayed_scene_scenery;
static void scene_native_effects(){if(!native_message_mode && caller_available){++entry_pushes;++entry_yields;++scene_native_windows;}}
static void __fastcall mock_employ_scene(Obj p){++scene_relays[0];relayed_scene_params=p;scene_native_effects();}
static void __fastcall mock_discovery_scene(Obj p){++scene_relays[1];relayed_scene_params=p;scene_native_effects();}
static void __fastcall mock_scenery_scene(unsigned force,Obj actor,Obj scenery,unsigned message,unsigned amount){++scene_relays[2];relayed_scene_force=force;relayed_scene_actor=actor;relayed_scene_scenery=scenery;relayed_scene_message=message;relayed_scene_amount=amount;scene_native_effects();}
static std::array<unsigned char,0x100> discovery_params,scene_item,scene_extra_item,scene_scenery,scene_ground;
static Obj __fastcall mock_scene_handle(Obj p){return p;}
static Obj __fastcall mock_scene_ground(Obj){return scene_ground.data();}
static bool dialog_ready=true,native_close_pending=false;
static bool synchronous_close=false;
static unsigned acknowledgements=0;
static unsigned person_acknowledgements=0;
static unsigned original_frames=0;static bool modal_active=false,frame_replaces_state=false,player_standby=true;
static std::array<unsigned char,0x90000> root;
static std::array<unsigned char,0x2400> context;
static std::array<unsigned char,0x700> state_a,state_b,state_c;
static std::array<unsigned char,0x200> force_mem,district_mem,dialog_mem,background_mem,income_child,world_report;
static std::array<unsigned char,0x3A8> income_mem;
static std::array<unsigned char,0x200> second_district_mem;
static std::array<unsigned char,0x200> animation_mem;
static std::array<unsigned char,0x240> diplomacy_window;
static std::array<unsigned char,0x240> diplomacy_view;
static std::array<unsigned char,0x200> other_force,own_person,other_person;
static std::array<unsigned char,0x80> command_mem;
static void *command_vt[20];
static std::array<unsigned char,0x470> event_layout;
static std::array<unsigned char,0x200> speech_left,speech_right;
static bool wait_ready=true,wait_reenter=false;
static unsigned native_pages=0,native_speeches=0,native_waits=0;
static unsigned native_recruit_speeches=0;
static bool mask_seen_before_native_report=false;
static unsigned result_applications,record_constructions,record_destructions;
static unsigned income_after_application;
static bool results_saw_consumed;
static bool invalid_report_record;
static unsigned long long recruitment_flags=0;static const wchar_t *recruitment_extra=nullptr;static unsigned recruitment_style=0;
static std::array<unsigned char,0x40> manager_mem;
static Obj manager_stack[4];
static std::array<unsigned char,0x162F0> action_mem;
static void *force_vt[32];
static void check(bool b,const char *why){++checks;if(!b){fprintf(stderr,"%s\n",why);exit(1);}}
static void point(Obj p,unsigned offset,void *value){*reinterpret_cast<void **>(p+offset)=value;}
static void number(Obj p,unsigned offset,unsigned value){*reinterpret_cast<unsigned *>(p+offset)=value;}
struct Node {Node *next,*previous;const wchar_t *value;};
static Node head{},a{},b{};
static void reset_pages(Obj state,const wchar_t *first=L"천시가 변했습니다.",const wchar_t *second=L"통솔이 올랐습니다.") {
    head.next=&a;head.previous=&b;a={&b,&head,first};b={&head,&a,second};
    point(state,0x478,&head);*reinterpret_cast<unsigned long long *>(state+0x480)=2;
}
static void __fastcall mock_page(Obj dialog) {
    Obj messages=pointer(dialog,0x180);auto &n=*reinterpret_cast<unsigned long long *>(messages+8);
    if(n){Obj first=pointer(pointer(messages,0),0);Obj next=pointer(first,0);point(pointer(messages,0),0,next);--n;++native_pages;}
}
static Obj __fastcall mock_force(Obj){return force_mem.data();}
static Obj __fastcall mock_person_force(Obj person){return person==other_person.data()?other_force.data():force_mem.data();}
static const wchar_t *__fastcall mock_force_name(Obj force){return force==other_force.data()?L"유도":L"조조";}
static unsigned __fastcall mock_command_kind(Obj){return 7;}
static Obj __fastcall mock_district(Obj){return district_mem.data();}
static unsigned __fastcall mock_id(Obj){return 1;}
static int __fastcall mock_valid(Obj p){return p!=nullptr;}
static Obj __fastcall mock_tutorial(){return reinterpret_cast<Obj>(&tutorial_flag);}
static Obj __fastcall mock_manager(){return manager_mem.data();}
static int __fastcall mock_day(Obj){return calendar_day;}
static bool __fastcall mock_player_state(Obj,const char *){return player_standby;}
static void __fastcall mock_pop(Obj){++pops;}
static Obj __fastcall mock_modal(Obj){return caller_available?(modal_active?dialog_mem.data():state_a.data()):nullptr;}
static void __fastcall mock_user_frame(Obj){++original_frames;if(frame_replaces_state)manager_stack[0]=state_b.data();}
static void mock_native_close_event();
static void __fastcall mock_acknowledge(Obj dialog,unsigned result) {
    ++acknowledgements;if(!dialog_ready)return;
    number(dialog,0x178,result);native_close_pending=true;
    if(synchronous_close){mock_native_close_event();number(dialog,0x178,0);point(state_a.data(),0x470,nullptr);}
}
static void __fastcall mock_person_acknowledge(Obj dialog,unsigned result){++person_acknowledgements;mock_acknowledge(dialog,result);}
static void mock_native_close_event() {
    check(native_close_pending,"only native close completion can run the bound callback");native_close_pending=false;mock_pop(mock_manager());
}
static void __fastcall mock_original_notice(Obj){++original_notices;}
static unsigned long long __fastcall mock_user_enter(Obj,Obj){++original_enters;return 31;}
static void __fastcall mock_user_leave(Obj,Obj){++original_leaves;}
static unsigned native_moves;
static void __fastcall mock_move(Obj p,int x,int y){++native_moves;number(p,0x20,static_cast<unsigned>(x));number(p,0x24,static_cast<unsigned>(y));}
static unsigned long long __fastcall mock_simple_enter(Obj p){point(p,0x470,dialog_mem.data());point(dialog_mem.data(),0x188,animation_mem.data());point(dialog_mem.data(),0x180,p+0x478);point(dialog_mem.data(),0x1A8,diplomacy_window.data());number(dialog_mem.data(),0x178,0xFFFFFFFF);mock_page(dialog_mem.data());++original_enters;return 1;}
static unsigned long long __fastcall mock_person_enter(Obj p){auto result=mock_simple_enter(p);point(dialog_mem.data(),0x190,animation_mem.data());number(dialog_mem.data(),0x188,5012);return result;}
static void __fastcall mock_speech(Obj,int,Obj,const wchar_t *,Obj){++native_speeches;}
static Obj relayed_owner,relayed_person;
static void __fastcall mock_recruit_speech(Obj owner,Obj person,const wchar_t *text,unsigned long long flags,const wchar_t *extra,unsigned style){++entry_pushes;++entry_yields;++native_recruit_speeches;relayed_owner=owner;relayed_person=person;relayed_text=text;recruitment_flags=flags;recruitment_extra=extra;recruitment_style=style;}
static void __fastcall mock_wait(Obj state) {
    ++native_waits;
    if(wait_ready && dword(state,0x478)==0) {
        ++pops;--*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10);
        if(wait_reenter){wait_reenter=false;diplomatic_speech(state_b.data(),4,nullptr,L"다음 교섭을 기약하겠소.",nullptr);}
    }
}
static void __fastcall mock_report_results(Obj action) {
    results_saw_consumed=(world_report[0x82]&6)==4;
    ++result_applications;number(action,0x16280,1);number(action,0x16284,0);
    if(invalid_report_record)std::fill(action+0x1B00+0x8A,action+0x1B00+0x136,static_cast<unsigned char>(0x41));
    else wcscpy_s(reinterpret_cast<wchar_t *>(action+0x1B00+0x8A),0x55,L"조운이 금 500을 발견했습니다.");
}
static Obj __fastcall mock_record_construct(Obj record){++record_constructions;memset(record,0,0x140);return record;}
static void __fastcall mock_record_destroy(Obj){++record_destructions;}
static unsigned long long __fastcall mock_report_enter(Obj p) {
    mask_seen_before_native_report=briefing_overlay::report_mask;
    ++report_enters;point(p,0x480,background_mem.data());point(p,0x488,action_mem.data());
    number(p,0x470,0);number(p,0x474,0);world_report[0x82]|=4;
    report_results(action_mem.data());
    return 7;
}
static void __fastcall mock_original_text(Obj,int,const wchar_t *,unsigned long long){++text_calls;}
static unsigned __fastcall mock_visible(Obj){return 1;}
static void __fastcall mock_size(Obj,int *out,int){out[0]=484;out[1]=512;}
static void __fastcall mock_position(Obj,int *out,int id){out[0]=id*10;out[1]=id*20;}
static void __fastcall mock_phase(Obj p,int phase) {
    number(p,0x470,static_cast<unsigned>(phase));
    if(phase==1) {
        point(p,0x490,income_mem.data());point(income_mem.data(),0x288,district_mem.data());point(income_mem.data(),0x378,income_child.data());
        point(income_mem.data(),0x290,second_district?second_district_mem.data():nullptr);second_district_mem[0x11]=8;
        set_text(income_child.data()+0x130,9,L"금 수입",1);
    }
    if(phase==2)++camera_restores;
    if(phase==3)++report_completions;
}
static void __fastcall mock_report_tick(Obj p){++report_ticks;if(dword(p,0x470)==3)++pops;}
static const wchar_t *__fastcall mock_name(Obj person){return person==own_person.data()?L"조조":person==other_person.data()?L"유도":L"제갈량";}
static const wchar_t *__fastcall mock_label(unsigned id){return id==916?L"제8군단":L"제1군단";}
static void __fastcall mock_income_values(Obj income,Obj district) {
    income_after_application=result_applications;
    Obj target=pointer(income,0x378)+0x130;
    if(income_numbers){
        set_text(target,10,L"2000",1);set_text(target,10,district==second_district_mem.data()?L"750":L"2500",1);
        for(int base:{10,19,28,37}) {
            if(base!=10)set_text(target,base,L"110000",1);
            if(base!=37 || !income_omit_delta)set_text(target,base+1,L"(^03-3655^00)",1);
            set_text(target,base+2,L"0",1);set_text(target,base+3,L"3655",1);
        }
        if(income_blank_military) {
            // Native 67E32E branch: blank delta, localized N/A income/cost.
            set_text(target,38,L"",1);set_text(target,39,L"해당 없음",1);set_text(target,40,L"해당 없음",1);
        }
        if(income_blank_balance)set_text(target,10,L"",1);
    }
}
static void *fixture_function(unsigned rva) {
    switch(rva) {
    case 0x842A90:return reinterpret_cast<void *>(&mock_picture_show);
    case 0x835230:return reinterpret_cast<void *>(&mock_picture_close);
    case 0x32E910:return reinterpret_cast<void *>(&mock_force);
    case 0x32E8D0:return reinterpret_cast<void *>(&mock_district);
    case 0x32F500:case 0x233750:return reinterpret_cast<void *>(&mock_valid);
    case 0x22DBC0:return reinterpret_cast<void *>(&mock_id);
    case 0x332E90:return reinterpret_cast<void *>(&mock_message_mode);
    case 0x56EE40:return reinterpret_cast<void *>(&mock_tutorial);
    case 0x110E0:return reinterpret_cast<void *>(&mock_manager);
    case 0x12540:return reinterpret_cast<void *>(&mock_pop);
    case 0x861480:return reinterpret_cast<void *>(&mock_acknowledge);
    case 0x863C70:return reinterpret_cast<void *>(&mock_page);
    case 0x83BE30:return reinterpret_cast<void *>(&mock_wait);
    case 0x32BA50:return reinterpret_cast<void *>(&mock_day);
    case 0x56FE50:return reinterpret_cast<void *>(&mock_player_state);
    case 0x56FC20:return reinterpret_cast<void *>(&mock_modal);
    case 0x44CAF0:return reinterpret_cast<void *>(&mock_user_frame);
    case 0x44A0F0:return reinterpret_cast<void *>(&mock_user_enter);
    case 0x448E40:case 0x448A70:case 0x67B1E0:return reinterpret_cast<void *>(&mock_user_leave);
    case 0x862E40:case 0x67B7C0:case 0x66F7C0:return reinterpret_cast<void *>(&mock_original_notice);
    case 0x1F15F0:case 0x8108C0:return reinterpret_cast<void *>(&mock_simple_enter);
    case 0x874650:return reinterpret_cast<void *>(&mock_move);
    case 0x67B420:return reinterpret_cast<void *>(&mock_report_enter);
    case 0x67AE80:return reinterpret_cast<void *>(&mock_report_results);
    case 0x2213C0:return reinterpret_cast<void *>(&mock_record_construct);
    case 0x2220A0:return reinterpret_cast<void *>(&mock_record_destroy);
    case 0x67C990:return reinterpret_cast<void *>(&mock_phase);
    case 0x67D1F0:return reinterpret_cast<void *>(&mock_income_values);
    case 0x30CF80:return reinterpret_cast<void *>(&mock_label);
    case 0x67B600:return reinterpret_cast<void *>(&mock_report_tick);
    case 0x86F250:return reinterpret_cast<void *>(&mock_visible);
    case 0x92CF90:return reinterpret_cast<void *>(&mock_size);
    case 0x66F3E0:case 0x893720:return reinterpret_cast<void *>(&mock_user_leave);
    case 0x92CB10:return reinterpret_cast<void *>(&mock_position);
    case 0x22EEC0:return reinterpret_cast<void *>(&mock_name);
    case 0x3F5A0:return reinterpret_cast<void *>(&mock_scene_handle);
    case 0x925B0:return reinterpret_cast<void *>(&mock_scene_ground);
    case 0x22ECC0:return reinterpret_cast<void *>(&mock_force_name);
    case 0x22CA00:return reinterpret_cast<void *>(&mock_person_force);
    case 0x842380:return reinterpret_cast<void *>(&mock_recruit_speech);
    case 0x88C1C0:return reinterpret_cast<void *>(&mock_original_notice);
    case 0x1F16A0:return reinterpret_cast<void *>(&mock_person_enter);
    case 0x861450:return reinterpret_cast<void *>(&mock_person_acknowledge);
    default:return nullptr;
    }
}
static void cycle() {
    // Each synthetic cycle discards the prior mock native stack/dialogs.
    // The real manager cannot begin Progress while that modal state is live.
    simple_states.clear();simple_state=nullptr;
    point(state_a.data(),0,nullptr);point(state_a.data(),0x470,nullptr);
    strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CProgressState");
#ifndef NO_RUNTIME_LOG
    stalls.clear();
#endif
    native_message_mode=0;caller_available=true;point(image,0x27842A0,nullptr);
    mod_features.advisor_briefing=true;enabled=1;briefing_overlay::gpu_ready=true;inbox.fallback=false;
    manager_stack[0]=state_a.data();point(manager_mem.data(),0x20,manager_stack);*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=1;
    user_leave(state_a.data(),nullptr);check(inbox.collecting && inbox.events.empty(),"accepted progress starts one new empty cycle");
}
static unsigned destiny_relays,disaster_push_relays[2]{},disaster_yield_relays[2]{};
static Obj destiny_argument,disaster_arguments[4]{};
static void __fastcall mock_destiny(Obj data){++destiny_relays;destiny_argument=data;scene_native_effects();}
template<unsigned Branch> static void __fastcall mock_disaster_push(Obj m,Obj t,Obj l,Obj c){++disaster_push_relays[Branch];disaster_arguments[0]=m;disaster_arguments[1]=t;disaster_arguments[2]=l;disaster_arguments[3]=c;++entry_pushes;}
template<unsigned Branch> static void __fastcall mock_disaster_yield(Obj){++disaster_yield_relays[Branch];++entry_yields;}
static void scene_site_checks() {
    // 사용자 2026-10-30: every E8 site accepts native or external target independently.
    reinterpret_cast<IMAGE_DOS_HEADER *>(image)->e_lfanew=0x80;
    reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+0x80)->OptionalHeader.SizeOfImage=0x29FF000;
    Obj relay=allocate_near(image+0x298413);check(relay!=nullptr,"scene caller relays allocated near the image");
    unsigned char entries[scene_site_count][16],original[scene_site_count][5];
    for(unsigned i=0;i<scene_site_count;++i){memcpy(entries[i],image+scene_sites[i].original,16);memcpy(original[i],image+scene_sites[i].rva,5);}
    auto redirect=[&](unsigned i,Obj target){int delta=static_cast<int>(target-(image+scene_sites[i].rva+5));image[scene_sites[i].rva]=0xE8;memcpy(image+scene_sites[i].rva+1,&delta,4);};
    for(unsigned index=0;index<scene_site_count;++index) {
        prepare_scene_calls(relay);publish_scene_calls();
        for(unsigned i=0;i<scene_site_count;++i)check(scene_calls[i].published && !memcmp(entries[i],image+scene_sites[i].original,16),"native E8 hooks publish without changing any original function entry");
        restore_scene_calls();
        for(unsigned i=0;i<scene_site_count;++i)check(!memcmp(original[i],image+scene_sites[i].rva,5),"only original per-site E8 displacement restored");
        redirect(index,relay+2048+index*16);prepare_scene_calls(relay);
        check(scene_next[index]==relay+2048+index*16,"external preinstalled target retained in its own next slot");publish_scene_calls();restore_scene_calls();
        check(scene_call_target(index)==relay+2048+index*16,"uninstall restores the external chain target");memcpy(image+scene_sites[index].rva,original[index],5);
        for(bool wrong_opcode:{false,true}) {
            redirect(index,image+0x123450);if(wrong_opcode)image[scene_sites[index].rva]=0xE9;
            prepare_scene_calls(relay);publish_scene_calls();
            for(unsigned i=0;i<scene_site_count;++i)check(scene_calls[i].published==(i!=index),"invalid in-image target/opcode skips that site only");
            restore_scene_calls();memcpy(image+scene_sites[index].rva,original[index],5);
        }
        prepare_scene_calls(relay);publish_scene_calls();redirect(index,relay+3000);
        restore_scene_calls();check(scene_call_target(index)==relay+3000,"subsequent foreign E8 remains owned by its writer during restore");
        memcpy(image+scene_sites[index].rva,original[index],5);
        prepare_scene_calls(relay);publish_scene_calls();image[scene_sites[index].rva]=0x90;
        restore_scene_calls();check(image[scene_sites[index].rva]==0x90,"restore preserves a foreign non-E8 opcode");memcpy(image+scene_sites[index].rva,original[index],5);
    }
    // The two calls at 298805/29880D share a CAS16 block; restore keeps neighbour edits.
    prepare_scene_calls(relay);publish_scene_calls();unsigned char adjacent=image[0x29880A];image[0x29880A]^=1;
    restore_scene_calls();check(image[0x29880A]==static_cast<unsigned char>(adjacent^1),"restoring individual calls preserves a later adjacent instruction edit");image[0x29880A]=adjacent;
    prepare_scene_calls(relay);int foreign=0;memcpy(image+scene_sites[0].rva+1,&foreign,4);publish_scene_calls();
    check(!scene_calls[0].published && scene_calls[1].published,"target changed between preparation and publication skips only the raced site");restore_scene_calls();memcpy(image+scene_sites[0].rva,original[0],5);
    VirtualFree(relay,0,MEM_RELEASE);for(auto &call:scene_calls)call={};
}
static void scene_collection_checks() {
    scene_next[0]=reinterpret_cast<Obj>(&mock_destiny);
    scene_next[1]=reinterpret_cast<Obj>(&mock_disaster_push<0>);scene_next[2]=reinterpret_cast<Obj>(&mock_disaster_yield<0>);
    scene_next[3]=reinterpret_cast<Obj>(&mock_disaster_push<1>);scene_next[4]=reinterpret_cast<Obj>(&mock_disaster_yield<1>);
    std::array<unsigned char,0x20> building{},tile{},place{};
    point(root.data(),0xDF30+8,force_mem.data());point(root.data(),0x6DA98+8,building.data());
    *reinterpret_cast<unsigned short *>(building.data()+0x10)=1;point(root.data(),0xE270+8,tile.data());
    *reinterpret_cast<unsigned short *>(tile.data()+0x12)=1;point(root.data(),0x6CAF0+8,place.data());wcscpy_s(reinterpret_cast<wchar_t *>(place.data()+0x10),8,L"진류");
    unsigned data[]={1,1,0x2A,20,0x5EE4,1200,7000};
    for(unsigned message:{0x5EE4u,0x5EE5u}) {
        cycle();data[4]=message;unsigned pushes=entry_pushes,yields=entry_yields,calls=destiny_relays;
        destiny_call(reinterpret_cast<Obj>(data));
        check(inbox.events.size()==1 && inbox.events[0].kind==Kind::Weather && inbox.events[0].text==destiny_summary(L"조조",L"진류",message,1200,7000),"native gold/grain donation fields become an exact Weather summary");
        check(destiny_relays==calls && entry_pushes==pushes && entry_yields==yields && data[5]==1200 && data[6]==7000,"donation capture changes no rewards/params and never pushes or yields");
    }
    check(has(destiny_summary(L"조조",L"진류",0x5EE4,123,456),L"123금") && has(destiny_summary(L"조조",L"진류",0x5EE5,789,999),L"789군량"),"gold and grain wording independently verified");
    for(unsigned failure=0;failure<7;++failure) {
        cycle();data[4]=0x5EE4;data[0]=1;data[1]=1;place[0x10]=static_cast<unsigned char>(0xC8);
        Obj input=reinterpret_cast<Obj>(data);
        if(failure==0)point(root.data(),0xDF30+8,nullptr);
        if(failure==1)point(root.data(),0x6DA98+8,nullptr);
        if(failure==2)point(root.data(),0x6CAF0+8,nullptr);
        if(failure==3)data[4]=0x5EE3;
        if(failure==4)input=nullptr;
        if(failure==5)input=reinterpret_cast<Obj>(1);
        if(failure==6)data[1]=10000;
        unsigned calls=destiny_relays;destiny_call(input);
        check(destiny_relays==calls+1 && destiny_argument==input && inbox.events.empty(),"missing name/invalid donation payload relays original data and stores nothing");
        point(root.data(),0xDF30+8,force_mem.data());point(root.data(),0x6DA98+8,building.data());point(root.data(),0x6CAF0+8,place.data());
    }
    data[0]=data[1]=1;data[4]=0x5EE4;
    auto push=[&](unsigned branch,Obj list,Obj type=nullptr){if(!type)type=image+0x13ACE30;if(branch)disaster_push<1>(manager_mem.data(),type,list,state_c.data());else disaster_push<0>(manager_mem.data(),type,list,state_c.data());};
    auto yield=[&](unsigned branch,Obj caller){if(branch)disaster_yield<1>(caller);else disaster_yield<0>(caller);};
    for(unsigned branch=0;branch<2;++branch) {
        cycle();reset_pages(state_a.data(),branch?L"진류에 메뚜기가 발생했습니다.":L"진류에 역병이 발생했습니다.",L"진류의 군량 피해를 입었습니다.");
        scene_calls[2+branch*2].published=true;number(state_a.data(),0x58,0);
        unsigned pushes=entry_pushes,yields=entry_yields;Obj list=state_a.data()+0x478;
        push(branch,list);check(inbox.events.size()==2 && inbox.events[0].kind==Kind::Disaster && entry_pushes==pushes && disaster_pending[branch].caller==state_a.data(),"both disaster branches copy complete original list before suppressing state push");
        check(dword(state_a.data(),0x58)==0,"suppressed window preserves native zero closed result");
        inbox.finish();yield(1-branch,state_a.data());check(entry_yields==yields+1 && disaster_pending[branch].caller==state_a.data(),"other branch cannot consume a pending suppression");
        yield(branch,state_a.data());check(entry_yields==yields+1 && !disaster_pending[branch].caller,"paired yield stays suppressed even if collecting changes, then consumes its TLS flag");
        yield(branch,state_a.data());check(entry_yields==yields+2,"next unpaired yield runs native once");
        for(unsigned failure=0;failure<7;++failure) {
            cycle();reset_pages(state_a.data());scene_calls[2+branch*2].published=failure!=0;
            Obj input=state_a.data()+0x478,type=image+0x13ACE30;
            if(failure==1)a.value=nullptr;
            if(failure==2)b.next=&a;
            if(failure==3)*reinterpret_cast<unsigned long long *>(state_a.data()+0x480)=300;
            if(failure==4)input=reinterpret_cast<Obj>(1);
            if(failure==5)type=image+0x13ACE31;
            if(failure==6)inbox.events.assign(Inbox::limit,{Kind::News,L"full"});
            auto stored=inbox.events.size();pushes=entry_pushes;yields=entry_yields;
            push(branch,input,type);yield(branch,state_a.data());
            check(entry_pushes==pushes+1 && entry_yields==yields+1 && !disaster_pending[branch].caller && inbox.events.size()==stored,"unavailable yield/invalid list/template/full inbox atomically falls back to native push AND yield");
            check(disaster_arguments[0]==manager_mem.data() && disaster_arguments[1]==type && disaster_arguments[2]==input && disaster_arguments[3]==state_c.data(),"fallback preserves all four native push arguments");
        }
        cycle();reset_pages(state_a.data());scene_calls[2+branch*2].published=true;push(branch,state_a.data()+0x478);yields=entry_yields;
        yield(branch,state_b.data());check(entry_yields==yields+1 && !disaster_pending[branch].caller,"different caller consumes no suppression and clears stale flag");
    }
    for(unsigned gate=0;gate<9;++gate) {
        cycle();reset_pages(state_a.data());scene_calls[2].published=scene_calls[4].published=true;
        if(gate==0)inbox.finish();if(gate==1)enabled=0;if(gate==2)inbox.fallback=true;if(gate==3)briefing_overlay::gpu_ready=false;
        if(gate==4)tutorial_flag=1;if(gate==5)strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CTreatDiplomacyState");
        if(gate==6)native_message_mode=1;if(gate==7)caller_available=false;if(gate==8)mod_features.advisor_briefing=false;
        unsigned calls=destiny_relays,pushes=entry_pushes,yields=entry_yields;destiny_call(reinterpret_cast<Obj>(data));
        push(0,state_a.data()+0x478);yield(0,state_a.data());push(1,state_a.data()+0x478);yield(1,state_a.data());
        check(destiny_relays==calls+1 && entry_pushes>=pushes+2 && entry_yields>=yields+2 && inbox.events.empty(),"all protected/noncollecting cases preserve donation and BOTH disaster native paths");
        tutorial_flag=0;
    }
    cycle();clear_scene_pending();for(auto &call:scene_calls)call={};
    point(root.data(),0xDF30+8,nullptr);point(root.data(),0x6DA98+8,nullptr);point(root.data(),0xE270+8,nullptr);point(root.data(),0x6CAF0+8,nullptr);
#ifndef NO_RUNTIME_LOG
    check(message_entry_paths==10 && message_entry_counts[7][0]>=2 && message_entry_counts[8][0]>=1 && message_entry_counts[9][0]>=1,"donation/disaster paths extend existing captured/relayed diagnostic schema");
#endif
}
static void time_wait_checks() {
    // 사용자 2026-10-28: expired=0, never skip=1; no push/yield/pop.
    cycle();int result=9;unsigned relays=time_wait_relays,pushes=entry_pushes,yields=entry_yields,popped=pops;
    time_wait(2100,&result,1);
    check(result==0 && time_wait_relays==relays && entry_pushes==pushes && entry_yields==yields && pops==popped,"progress wait returns timeout zero without native push, yield or pop");
    // The disaster callers retain their original (result==0) log/effect branch.
    check(result==0,"2D1D10 and 2E00C0 preserve timeout-dependent later presentation");
    for(unsigned gate=0;gate<9;++gate){
        cycle();result=9;
        if(gate==0)inbox.finish();if(gate==1)enabled=0;if(gate==2)inbox.fallback=true;
        if(gate==3)briefing_overlay::gpu_ready=false;if(gate==4)tutorial_flag=1;
        if(gate==5)strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CTreatDiplomacyState");
        if(gate==6)native_message_mode=1;if(gate==7)caller_available=false;
        int *out=gate==8?nullptr:&result;
        time_wait(500,out,gate%2);
        check(time_wait_relays==++relays && result==9 && last_wait_ms==500 && last_wait_result==out && last_wait_skip==gate%2,"every failed wait gate relays untouched arguments and leaves result intact");
        enabled=1;inbox.fallback=false;briefing_overlay::gpu_ready=true;tutorial_flag=0;
    }
    // All six diplomatic caller families share interaction(), including a
    // time-dialog top with a diplomatic parent beneath it.
    for(unsigned caller:{0x814380u,0x814AD0u,0x814FF0u,0x815970u,0x815EA0u,0x8168A0u}){
        (void)caller;cycle();result=7;
        strcpy_s(reinterpret_cast<char *>(state_b.data()+0x70),128,"CTreatDiplomacyState");
        strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CDialogGameStateBase<CCmnWaitTimeDialog>");
        manager_stack[0]=state_b.data();manager_stack[1]=state_a.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=2;
        time_wait(2100,&result,1);check(time_wait_relays==++relays && result==7,"diplomatic interaction under wait dialog always keeps original wait");
    }
#ifndef NO_RUNTIME_LOG
    check(time_wait_counts[0]>0 && time_wait_counts[1]>=15,"time wait timeout and relay counters available");
#endif
    cycle();inbox.finish();open=true;display_cache.open=true;rendered_open=true;
    briefing_overlay::displayed.open=true;briefing_overlay::shown=true;briefing_overlay::action=1;
    point(state_a.data(),0,image+0x13A4308);strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CCmnYesNoState");
    unsigned called=briefing_overlay::context_calls;log_frame(dialog_mem.data());
    check(briefing_overlay::context_calls==called+1 && !open && !rendered_open && !display_cache.open && !briefing_overlay::displayed.open && !briefing_overlay::shown && !briefing_overlay::action,"log frame closes native/cache/model and cancels pending input under YesNo");
    point(state_a.data(),0,image+0x13D63A0);strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CUserStrategyState");
    log_frame(dialog_mem.data());check(briefing_overlay::shown && !open && !briefing_overlay::displayed.open,"player log frame restores head with panel closed");
    cycle();called=briefing_overlay::context_calls;log_frame(dialog_mem.data());
    check(briefing_overlay::context_calls==called,"progress collection bypasses covered-player reset");
#ifndef NO_RUNTIME_LOG
    check(player_context_counts[0]>0 && player_context_counts[1]>0 && player_context_counts[2]>0 && player_context_counts[3]>0,"all player context gates counted");
#endif
}
static void picture_checks() {
    // 사용자 2026-10-28 설계 전환 4단계: suppress only paired event pictures.
    std::array<unsigned char,0x1C0> object{},other{};
    Obj first_picture=object.data(),second_picture=other.data();point(first_picture,0x1B8,scene_item.data());point(second_picture,0x1B8,scene_extra_item.data());
    cycle();pictures.clear();unsigned shows=native_picture_shows,closes=native_picture_closes;
#ifndef NO_RUNTIME_LOG
    auto suppressed=picture_counts[0],displayed=picture_counts[1],id10=picture_id_counts[10][0];
#endif
    picture_show(first_picture,10);
    check(native_picture_shows==shows && pictures.size()==1 && pictures[0].object==first_picture && pictures[0].id==10 && pictures[0].suppressed,"progress picture never calls 842A90 and records the exact suppressed owner/ID");
    inbox.finish();picture_close(first_picture);
    check(native_picture_closes==closes && pictures.empty(),"suppressed close never calls 835230 even after returning to player turn, and consumes record");
    picture_close(first_picture);
    check(native_picture_closes==++closes && native_picture_target==scene_item.data(),"unrecorded close calls native target(object+1B8) once");
    picture_show(first_picture,77);
    check(native_picture_shows==++shows && native_picture_target==scene_item.data() && native_picture_id==77 && native_picture_repeat==0 && !pictures[0].suppressed,"player turn displays with exact target/ID and third argument zero");
    inbox.collecting=true;picture_close(first_picture);
    check(native_picture_closes==++closes && pictures.empty(),"displayed picture closes natively even if collection began after showing");
    strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CTreatDiplomacyState");
    picture_show(first_picture,3);picture_close(first_picture);
    check(native_picture_shows==++shows && native_picture_closes==++closes && pictures.empty(),"diplomacy during progress preserves original show and close");
    strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CProgressState");
    picture_show(first_picture,1);picture_show(second_picture,2);picture_close(first_picture);
    check(native_picture_shows==shows && native_picture_closes==closes && pictures.size()==1 && pictures[0].object==second_picture,"independent owners cannot consume each other's suppressed record");
    inbox.collecting=false;picture_show(second_picture,4);picture_close(second_picture);
    check(native_picture_shows==++shows && native_picture_closes==++closes && pictures.empty(),"latest displayed state replaces suppressed state for the same owner");
    picture_show(first_picture,5);inbox.collecting=true;picture_show(first_picture,6);picture_close(first_picture);
    check(native_picture_shows==++shows && native_picture_closes==closes && pictures.empty(),"latest suppressed state replaces displayed state for the same owner");
    for(unsigned gate=0;gate<5;++gate) {
        enabled=gate==0?0:1;mod_features.advisor_briefing=gate!=1;inbox.fallback=gate==2;
        briefing_overlay::gpu_ready=gate!=3;tutorial_flag=gate==4?1:0;
        picture_show(first_picture,7);picture_close(first_picture);
        check(native_picture_shows==++shows && native_picture_closes==++closes,"disabled/fallback/unavailable/tutorial picture remains native");
    }
    enabled=1;mod_features.advisor_briefing=true;inbox.fallback=false;briefing_overlay::gpu_ready=true;tutorial_flag=0;
    picture_show(first_picture,-1);picture_close(first_picture);picture_show(first_picture,999);picture_close(first_picture);
    check(native_picture_shows==shows && native_picture_closes==closes && pictures.empty(),"unknown signed picture IDs are safely suppressed and paired as well");
#ifndef NO_RUNTIME_LOG
    check(picture_counts[0]==suppressed+6 && picture_counts[1]==displayed+9 && picture_id_counts[10][0]==id10+1 && picture_id_counts[78][0]>=2 && picture_last_id==999,"diagnostics count suppression/display and exact/other IDs without changing behavior");
#endif
    picture_show(first_picture,10);reset();check(pictures.empty(),"world reset drops picture identity records without native close");
    cycle();
}
static void scene_entry_checks() {
    // 사용자 2026-10-28 정정: caller-local result application after return is allowed.
    std::array<unsigned char,0x18> employ_params{};
    point(employ_params.data(),0,own_person.data());point(employ_params.data(),8,other_person.data());
    point(discovery_params.data(),8,own_person.data());point(discovery_params.data(),0x10,scene_ground.data());
    point(discovery_params.data(),0x18,scene_item.data());point(discovery_params.data(),0x20,other_person.data());
    point(discovery_params.data(),0x28,scene_extra_item.data());point(discovery_params.data(),0x30,scene_scenery.data());number(discovery_params.data(),0x38,1);
    wcscpy_s(reinterpret_cast<wchar_t *>(scene_item.data()+0x10),100,L"적토마");
    wcscpy_s(reinterpret_cast<wchar_t *>(scene_extra_item.data()+0x10),100,L"고서");
    wcscpy_s(reinterpret_cast<wchar_t *>(scene_scenery.data()+0x10),100,L"태산");
    wcscpy_s(reinterpret_cast<wchar_t *>(scene_ground.data()+0x10),100,L"형주");
    auto invoke=[&](unsigned path){if(path==0)employ_scene(employ_params.data());else if(path==1)discovery_scene(discovery_params.data());else scenery_scene(17,own_person.data(),scene_scenery.data(),0x5C76,123);};
    auto captured=[&](unsigned path,const std::wstring &expected) {
        auto n=inbox.events.size();auto saved_employ=employ_params;auto saved_discovery=discovery_params;auto saved_actor=own_person;auto saved_target=other_person;
        unsigned relays=scene_relays[path],pushes=entry_pushes,yields=entry_yields,windows=scene_native_windows,popped=pops;
        invoke(path);
        check(inbox.events.size()==n+1 && inbox.events.back().text==expected && inbox.events.back().report==expected,"scene entry stores exactly one summary; detail is the same safe text");
        check(scene_relays[path]==relays && entry_pushes==pushes && entry_yields==yields && scene_native_windows==windows && pops==popped,"captured scene calls no original window/nameplate, push, 571EF0 or pop");
        check(saved_employ==employ_params && saved_discovery==discovery_params && saved_actor==own_person && saved_target==other_person,"scene summary never writes params or either game person");
    };
    cycle();
    for(unsigned success:{0u,1u}) {
        number(employ_params.data(),0x10,success);
        captured(0,success?L"조조가 유도 등용에 성공했습니다.":L"조조가 유도 등용에 실패했습니다.");
        unsigned applied=0;Obj applied_target=nullptr;
        // A minimal caller retains its predetermined result and applies it once.
        auto caller=[&](){bool result=success!=0;Obj target=pointer(employ_params.data(),8);employ_scene(employ_params.data());if(result){++applied;applied_target=target;}};
        caller();check(applied==success && (!success || applied_target==other_person.data()),"caller-local recruitment result still applies once after bypassed scene");
    }
    const wchar_t *found[]={L"적토마",L"유도",L"고서",L"형주",L"형주",L"태산"};
    for(unsigned type=0;type<=5;++type) {
        number(discovery_params.data(),0,type);captured(1,discovery_scene_summary(type,L"조조",found[type],L"",true));
        check(inbox.events.back().kind==(type==0 || type==2?Kind::Treasure:type==1?Kind::Recruitment:Kind::News),"each discovery type has the correct independent summary group");
    }
    number(discovery_params.data(),0,1);number(discovery_params.data(),0x38,0);
    captured(1,L"조조가 유도를 발견했으며, 등용에 실패했습니다.");
    captured(2,L"조조가 명승 태산을 발견했습니다.");
    check(recruitment_scene_summary(L"부손",L"조루",true)==L"부손이 조루 등용에 성공했습니다." && discovery_scene_summary(0,L"부손",L"청룡언월도",L"",true)==L"부손이 명품 청룡언월도를 발견했습니다.","summary particles follow Korean name endings");
    auto passed=[&](unsigned path,MessageReason reason) {
        unsigned old=scene_relays[path];auto events=inbox.events.size();
#ifndef NO_RUNTIME_LOG
        auto failures=message_pass_reasons[path+4][reason],count=message_entry_counts[path+4][1];
#else
        (void)reason;
#endif
        invoke(path);
        check(scene_relays[path]==old+1 && inbox.events.size()==events,"disallowed scene relays once without a partial summary");
        check(path==2?(relayed_scene_force==17 && relayed_scene_actor==own_person.data() && relayed_scene_scenery==scene_scenery.data() && relayed_scene_message==0x5C76 && relayed_scene_amount==123):relayed_scene_params==(path==0?employ_params.data():discovery_params.data()),"relay preserves params identity or all five scenery arguments including stack amount");
#ifndef NO_RUNTIME_LOG
        check(message_pass_reasons[path+4][reason]==failures+1 && message_entry_counts[path+4][1]==count+1,"scene diagnostic records the first failed gate and relay count");
#endif
    };
    number(discovery_params.data(),0,0);
    for(unsigned path=0;path<3;++path) {
        cycle();enabled=0;passed(path,message_inactive);enabled=1;
        inbox.finish();passed(path,message_not_collecting);inbox.collecting=true;
        inbox.fallback=true;passed(path,message_fallback);inbox.fallback=false;
        briefing_overlay::gpu_ready=false;passed(path,message_overlay);briefing_overlay::gpu_ready=true;
        tutorial_flag=1;passed(path,message_tutorial);tutorial_flag=0;
        strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CTreatDiplomacyState");passed(path,message_interaction);
        strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CProgressState");
        native_message_mode=1;passed(path,message_native_mode);native_message_mode=0;
        caller_available=false;passed(path,message_no_caller);caller_available=true;
        inbox.events.resize(Inbox::limit);passed(path,message_store_failed);check(inbox.fallback,"failed summary storage restores native scene through fallback");
    }
    cycle();point(employ_params.data(),8,nullptr);passed(0,message_fields);point(employ_params.data(),8,other_person.data());
    point(discovery_params.data(),0x18,nullptr);passed(1,message_fields);point(discovery_params.data(),0x18,scene_item.data());
    for(unsigned type:{6u,~0u}){number(discovery_params.data(),0,type);passed(1,message_phase);}
    for(unsigned path=0;path<2;++path) {
        auto n=inbox.events.size();unsigned before=scene_relays[path];
        if(path)discovery_scene(nullptr);else employ_scene(nullptr);
        check(scene_relays[path]==before+1 && relayed_scene_params==nullptr && inbox.events.size()==n,"null params fail closed and preserve the native relay argument");
    }
    auto n=inbox.events.size();unsigned before=scene_relays[2];scenery_scene(17,nullptr,scene_scenery.data(),0x5C76,123);
    check(scene_relays[2]==before+1 && relayed_scene_actor==nullptr && inbox.events.size()==n,"missing scenery speaker retains native scene");
    reset();
}
static void portrait_sample_checks() {
    // User 2026-10-30: terrain -> face -> face; two captures, >=500ms apart.
    constexpr unsigned pitch=88*4+16;
    std::vector<unsigned char> pixels(pitch*88,0);PortraitSample face,terrain,black;
    check(!portrait_sample_from_pixels(pixels.data(),pitch,88,88,black),"black portrait remains rejected");
    std::fill(pixels.begin(),pixels.end(),static_cast<unsigned char>(80));
    check(!portrait_sample_from_pixels(pixels.data(),pitch,88,88,black),"flat portrait remains rejected");
    for(unsigned y=0;y<88;++y)for(unsigned x=0;x<88;++x){auto *p=pixels.data()+y*pitch+x*4;p[0]=static_cast<unsigned char>(30+x);p[1]=static_cast<unsigned char>(50+y);p[2]=100;p[3]=0;}
    check(portrait_sample_from_pixels(pixels.data(),pitch,88,88,face),"padded RGB source with zero alpha produces a CPU sample");
    for(unsigned y=0;y<88;++y)for(unsigned x=0;x<88;++x){auto *p=pixels.data()+y*pitch+x*4;p[0]=180;p[1]=static_cast<unsigned char>(140+y);p[2]=static_cast<unsigned char>(30+x);}
    check(portrait_sample_from_pixels(pixels.data(),pitch,88,88,terrain),"varied terrain passes the original validity test");
    unsigned diff=0;Rect r{10,10,88,88};PortraitConfirmation confirm;
    check(!confirm.observe(terrain,1,r,1000,diff),"first valid terrain candidate is never accepted");
    check(!confirm.observe(face,1,r,1500,diff) && confirm.at==1500 && diff>10,"different second candidate replaces terrain without acceptance");
    check(confirm.observe(face,1,r,2000,diff) && diff==0,"second and third stable faces are accepted");
    confirm.reset();check(!confirm.present,"explicit sample reset clears confirmation");
    check(!confirm.observe(face,1,r,3000,diff),"first face waits for confirmation");
    check(!confirm.observe(terrain,1,r,3499,diff) && confirm.at==3000 && diff==0,"before 500ms candidate is neither compared nor stored");
    check(confirm.observe(face,1,r,3500,diff),"identical face confirms at exactly 500ms");
    check(!confirm.observe(face,2,r,4000,diff) && confirm.revision==2 && confirm.at==4000,"revision change resets sample even for identical pixels");
    check(!confirm.observe(face,2,{12,10,88,88},4500,diff) && confirm.at==4500,"source rect change resets sample");
    check(confirm.observe(face,2,{12.9f,10,88,88},5000,diff),"rounded source rect within one pixel confirms");
    check(!confirm.observe(face,2,{12,10,90,88},5500,diff),"source dimension change resets sample");
    auto altered=face;altered.cells[100][1]=255;
    check(!portrait_samples_stable(face,altered,diff) && diff<=10,"local cell maximum rejects a change hidden by channel means");
    altered=face;for(auto &cell:altered.cells)for(auto &channel:cell)channel+=10;
    check(portrait_samples_stable(face,altered,diff) && diff==10,"channel mean difference of ten is allowed");
    for(auto &cell:altered.cells)++cell[0];
    check(!portrait_samples_stable(face,altered,diff) && diff==11,"channel mean above ten is rejected");
    check(!confirm.observe(face,2,{12,10,90,88},5400,diff) && confirm.at==5500,"capture older than CPU sample cannot confirm through unsigned underflow");
}
int main(int argc,char **argv) {
    portrait_sample_checks();
    check(argc==2,"independent captured text supplied");
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x29FF000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));check(image!=nullptr,"synthetic image allocated");
    FILE *f=nullptr;check(fopen_s(&f,argv[1],"rb")==0,"retained capture opened");
    check(fread(image+0x1000,1,kTextSize,f)==kTextSize,"retained capture loaded");fclose(f);
    for(const auto &s:slots)point(image,s.rva,image+s.original);
    check(guards_match(),"native original code and slots accepted");
    scene_site_checks();
    for(const auto &g:guards)for(unsigned offset:{0u,g.size-1}){image[g.rva+offset]^=1;check(!guards_match(),"changed dependency rejects briefing");image[g.rva+offset]^=1;}
    for(const auto &s:slots){image[s.rva]^=1;check(!guards_match(),"foreign slot rejects briefing");image[s.rva]^=1;}
    image[0x81133D]^=1;check(!guards_match(),"foreign opcode at diplomacy-owned phase call rejects briefing");image[0x81133D]^=1;
    image[0x81133E]^=1;check(!guards_match(),"unknown phase-call redirection is not accepted as a local counterturn");image[0x81133E]^=1;
    // Entry fingerprints fail closed before either hook is posted.
    for(unsigned entry:{0x304640u,0x1F0AD0u,0x1F09E0u,0x1F0DD0u,0x842380u,0x6C43B0u,0x6C4670u,0x41D550u}) {
        image[entry+16]^=1;
        check(!advisor_briefing::install() && !time_wait_link.published && !message_links[0].published && !message_links[1].published && !message_links[2].published && !event_link.published && !scene_links[0].published && !scene_links[1].published && !scene_links[2].published && image[0x1F0AD0]==0x48 && image[0x1F09E0]==0x48,"whole-function mismatch prevents all entry publications including time wait");
        image[entry+16]^=1;
    }
    Obj entry_relay=allocate_near(image+0x1F0AD0);check(entry_relay!=nullptr,"entry relay allocation");
    CodeLink probe_link{};
    check(!prepare_message_link(probe_link,0x1F0AD0,narration_prologue,sizeof(narration_prologue),entry_relay,reinterpret_cast<void *>(&message_call)) && !probe_link.published,"wrong prologue cannot prepare or publish a relay");
    check(!prepare_message_link(probe_link,0x1F0AD0,message_prologue,4,entry_relay,reinterpret_cast<void *>(&message_call)) && !probe_link.published,"invalid displaced length cannot prepare or publish a relay");
    for(unsigned entry:{0x451FF0u,0x43FF90u}) {
        unsigned length=entry==0x451FF0?sizeof(picture_show_bytes):sizeof(picture_close_bytes);
        for(unsigned i=0;i<length;++i) {
            image[entry+i]^=1;
            check(!advisor_briefing::install() && !picture_links[0].published && !picture_links[1].published && !message_links[0].published,"any full tail-wrapper byte mismatch publishes no picture or existing entry hook");
            image[entry+i]^=1;
        }
    }
    for(unsigned target:{0x842A90u,0x835230u}) {
        image[target+0x20]^=1;
        check(!advisor_briefing::install() && !picture_links[0].published && !picture_links[1].published && !message_links[0].published,"target prefix FNV mismatch prevents every hook publication");
        image[target+0x20]^=1;
    }
    check(advisor_briefing::install(),"all guarded entry links and existing slots publish together");
    Obj wait_relay=reinterpret_cast<Obj>(original_time_wait);
    check(time_wait_link.published && image[0x304640]==0xE9 && !memcmp(wait_relay,narration_prologue,sizeof(narration_prologue)) && pointer(wait_relay,sizeof(narration_prologue)+6)==image+0x30464B,"time wait relay displaces eleven whole non-relative bytes and resumes at exact native instruction");
    for(unsigned i=0;i<2;++i) {
        const auto &link=picture_links[i];Obj at=image+link.rva+5+*reinterpret_cast<int *>(image+link.rva+1);
        unsigned size=i?sizeof(picture_close_bytes):sizeof(picture_show_bytes);
        check(link.published && image[link.rva]==0xE9 && at[0]==0xFF && at[1]==0x25 && pointer(at,6)==reinterpret_cast<Obj>(i?reinterpret_cast<void *>(&picture_close):reinterpret_cast<void *>(&picture_show)),"picture entry jumps to our wrapper without a relocated original tail jump");
        for(unsigned n=5;n<size;++n)check(image[link.rva+n]==0x90,"complete native tail wrapper is replaced and NOP padded");
        check(link.original[15]==link.modified[15],"picture patch preserves adjacent function/alignment byte");
    }
    for(unsigned i=0;i<7;++i) {
        const unsigned char *prologue=i==0?message_prologue:i==1?narration_prologue:i==2?person_prologue:i==3?event_prologue:i<6?scene_prologue:scenery_prologue;
        unsigned size=i==0?sizeof(message_prologue):i==1?sizeof(narration_prologue):i==2?sizeof(person_prologue):i==3?sizeof(event_prologue):i<6?sizeof(scene_prologue):sizeof(scenery_prologue);
        Obj relay=i==0?reinterpret_cast<Obj>(original_message_call):i==1?reinterpret_cast<Obj>(original_narration_call):i==2?reinterpret_cast<Obj>(original_person_call):i==3?reinterpret_cast<Obj>(original_event_speech):i==4?reinterpret_cast<Obj>(original_employ_scene):i==5?reinterpret_cast<Obj>(original_discovery_scene):reinterpret_cast<Obj>(original_scenery_scene);
        const auto &link=i<3?message_links[i]:i==3?event_link:scene_links[i-4];unsigned offset=link.rva-link.block;
        check(link.published && image[link.rva]==0xE9 && !memcmp(relay,prologue,size),"published entry relay preserves every displaced whole instruction");
        check(relay[size]==0xFF && relay[size+1]==0x25 && pointer(relay,size+6)==image+link.rva+size,"absolute return jump resumes at exact entry+8 or entry+11 without clobbering RAX/RSP");
        for(unsigned n=5;n<size;++n)check(image[link.rva+n]==0x90,"entry trailing displaced bytes are NOP padded inside atomic 16-byte patch");
        for(unsigned n=0;n<16;++n)if(n<offset || n>=offset+size)check(link.original[n]==link.modified[n],"entry publication preserves unrelated bytes in the atomic block");
    }
    // Existing five-byte MOV relays still patch exactly five bytes.
    for(CodeLink *link:{&text_link,&speech_link}) {
        unsigned offset=link->rva-link->block;
        for(unsigned n=0;n<16;++n)if(n<offset || n>=offset+5)check(link->original[n]==link->modified[n],"existing five-byte CodeLink bytes outside redirection remain unchanged");
    }
    for(unsigned n=0;n<sizeof(slots)/sizeof(*slots);++n)restore_pointer(slots[n].rva,slots[n].original,replacements[n]);
    restore_link(time_wait_link);for(unsigned i=2;i;)restore_link(picture_links[--i]);for(unsigned i=3;i;)restore_link(scene_links[--i]);restore_link(event_link);for(unsigned i=3;i;)restore_link(message_links[--i]);
    call_restore(report_results_call);restore_link(speech_link);restore_link(text_link);
    check(guards_match(),"rollback restores both full entry functions, five-byte links and native slots");
    // A publication conflict on the second entry must roll the first one back.
    CodeLink pair[2]{};
    check(prepare_message_link(pair[0],0x1F0AD0,message_prologue,sizeof(message_prologue),entry_relay,reinterpret_cast<void *>(&message_call)) &&
          prepare_message_link(pair[1],0x1F09E0,narration_prologue,sizeof(narration_prologue),entry_relay+64,reinterpret_cast<void *>(&narration_call)),"both entry relays prepare before any publication");
    image[0x1F09E0+12]^=1;
    check(publish_link(pair[0]) && !publish_link(pair[1]),"second entry CAS conflict is detected");
    restore_link(pair[1]);restore_link(pair[0]);image[0x1F09E0+12]^=1;
    check(guards_match(),"failed pair publication leaves both entries native after rollback");
    VirtualFree(entry_relay,0,MEM_RELEASE);
    point(image,0x277DF18,root.data());point(root.data(),0x871E8,context.data());point(root.data(),0xE0D0+8,district_mem.data());
    // User 2026-10-26: normal live game has 15/16 orders at context+454.
    // That value must not disable notification collection or report skipping.
    number(context.data(),0x454,15);
    point(force_mem.data(),0,force_vt);force_vt[0x60/8]=reinterpret_cast<void *>(&mock_id);original_text=&mock_original_text;original_speech=&mock_speech;original_message_call=&mock_message_call;original_narration_call=&mock_narration_call;original_person_call=&mock_narration_call;original_event_speech=&mock_recruit_speech;
    original_employ_scene=&mock_employ_scene;original_discovery_scene=&mock_discovery_scene;original_scenery_scene=&mock_scenery_scene;original_time_wait=&mock_time_wait;
    scene_collection_checks();
    // User 2026-10-26: live face 484x512, world matrix 0.3125; the raw-size
    // <512 gate and raw anchor produced no avatar at 1280x720.
    float face_matrix[16]{};face_matrix[0]=face_matrix[5]=0.3125f;face_matrix[10]=face_matrix[15]=1;
    face_matrix[12]=-864.375f;face_matrix[13]=-424;Rect face_rect;
    check(portrait_bounds(484,512,0x11,face_matrix,face_rect),"real 512-high adviser texture is accepted");
    check(std::fabs(face_rect.x-(20+151.25f*0.075f))<0.01f && face_rect.y==884 && std::fabs(face_rect.w-151.25f*0.85f)<0.01f && std::fabs(face_rect.h-136)<0.01f,"capture uses the scaled world rectangle, top 85% centred (2026-10-31: no speech bubble edge)");
    face_matrix[12]=-1500;check(!portrait_bounds(484,512,0x11,face_matrix,face_rect),"offscreen slide animation waits for a complete portrait");
    face_matrix[12]=-864.375f;face_matrix[1]=0.5f;check(!portrait_bounds(484,512,0x11,face_matrix,face_rect),"unknown rotated source uses fallback avatar");face_matrix[1]=0;
    Legacy animated{dialog_mem.data(),-200,0,false};number(dialog_mem.data(),0x20,0);move(animated,true);move(animated,false);
    check(dword(dialog_mem.data(),0x20)==0,"legacy restoration retains the current animated position instead of initial -200");
    Inbox local;local.begin(1,1);
    check(local.append({{Kind::News,L"유비가 금 200을 발견했습니다."},{Kind::News,L"관우가 금 300을 발견했습니다."},{Kind::News,L"조운이 금 500을 발견했습니다."}}),"gold reports accepted atomically");
    check(local.cards().size()==1 && local.cards()[0].gold==1000 && local.unread()==3,"three gold findings aggregate to 1000 while preserving officer details");
    check(compact(local.events[0])==L"유비  ·  200" && local.events[0].text==L"유비가 금 200을 발견했습니다.","compact finding shows officer and amount while original remains intact");
    local.mark(Kind::Gold);check(local.unread()==0 && local.events.size()==3,"reading retains reports");
    local.finish();check(!local.append({{Kind::News,L"outside cycle"}}) && local.events.size()==3,"player standby freezes the completed cycle");
    local.begin(1,1);check(local.events.empty(),"next progress discards the previous cycle");
    local.append({{Kind::News,L"통솔이 상승했습니다."},{Kind::News,L"역병이 발생했습니다."},{Kind::News,L"등용에 성공했습니다."},{Kind::News,L"명품을 발견했습니다."}});
    check(local.cards().size()==4,"growth disaster recruitment treasure stay distinct");local.erase(Kind::Growth);check(local.events.size()==3,"delete affects only the chosen group");
    local.context(1,2);check(local.events.empty() && !local.collecting,"hotseat force change clears private reports");
    local.begin(1,2);check(!local.append({{Kind::News,std::wstring(Inbox::text_limit+1,L'x')}}) && local.fallback && local.events.empty(),"oversized text cannot partially consume a native message");
    local.begin(1,2);std::vector<Event> capacity(Inbox::limit,{Kind::News,L"한 줄"});check(local.append(capacity),"bounded complete cycle supported");
    check(!local.append({{Kind::News,L"overflow"}}) && local.events.size()==Inbox::limit && local.fallback,"overflow preserves stored reports and returns to native UI");
    check(gold_amount(L"통솔 100, 10일에 금을 발견")==0 && gold_amount(L"금 1,200을 발견")==1200,"unrelated numbers never inflate the gold total");
    for(int bw:{1280,1920,2560,3440})for(int bh:{720,1080,1440}) {
        double scale=(std::min)(bw/1920.0,bh/1080.0);int x=static_cast<int>((bw-1920*scale)/2+74*scale),y=static_cast<int>((bh-1080*scale)/2+1000*scale);
        float vx=0,vy=0;check(client_point(x,y,bw,bh,bw,bh,vx,vy) && vx>72 && vx<76 && vy>998 && vy<1002,"portrait input follows letterboxing");
    }
    cycle();calendar_day+=9;
    unsigned before=pops,original=original_notices;
    const wchar_t *entry_text=L"군정을 입수했습니다.";
    auto captured_entry=[&](unsigned path) {
        auto events=inbox.events.size();unsigned calls=entry_relays[path],pushes=entry_pushes,yields=entry_yields,suspends=entry_suspends;
        if(path)narration_call(5012,entry_text);else message_call(entry_text,1);
        check(inbox.events.size()==events+1 && inbox.events.back().text==entry_text && inbox.events.back().day==10 && inbox.events.back().speaker==(path?5012u:0u),"message entry stores one full event and original speaker/day");
        check(entry_relays[path]==calls && entry_pushes==pushes && entry_yields==yields && entry_suspends==suspends && pops==before,"capture creates no window/push, calls neither 83A450 nor 571EF0 and never pops or yields caller");
    };
    for(unsigned path=0;path<2;++path)captured_entry(path);
    check(briefing_overlay::displayed.collecting && briefing_overlay::displayed.unread==2,"badge updates during progress without command HUD frames");
    point(image,0x27842A0,native_message_context.data());
    for(unsigned path=0;path<2;++path)captured_entry(path);
    auto passed_entry=[&](unsigned path,MessageReason reason,const wchar_t *text=L"원작 통지") {
        auto events=inbox.events.size();unsigned calls=entry_relays[path];
#ifndef NO_RUNTIME_LOG
        LONG64 failures=message_pass_reasons[path][reason],passes=message_entry_counts[path][1];
#else
        (void)reason;
#endif
        if(path)narration_call(-1,text);else message_call(text,7);
        check(entry_relays[path]==calls+1 && relayed_text==text && (path?relayed_speaker==-1:relayed_mode==7) && inbox.events.size()==events,"failed capture calls the original relay once with unchanged ABI arguments");
#ifndef NO_RUNTIME_LOG
        check(message_pass_reasons[path][reason]==failures+1 && message_entry_counts[path][1]==passes+1,"relay diagnostics record exactly the first failed gate");
#endif
    };
    for(unsigned path=0;path<2;++path) {
        enabled=0;passed_entry(path,message_inactive);enabled=1;
        inbox.finish();passed_entry(path,message_not_collecting);inbox.collecting=true;
        inbox.fallback=true;passed_entry(path,message_fallback);inbox.fallback=false;
        briefing_overlay::gpu_ready=false;passed_entry(path,message_overlay);briefing_overlay::gpu_ready=true;
        tutorial_flag=1;passed_entry(path,message_tutorial);tutorial_flag=0;
        strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CSelectState");passed_entry(path,message_interaction);
        strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CProgressState");
        native_message_mode=1;passed_entry(path,message_native_mode);native_message_mode=0;
        number(native_message_context.data(),0x1B0,1);passed_entry(path,message_native_context);number(native_message_context.data(),0x1B0,0);
        caller_available=false;passed_entry(path,message_no_caller);caller_available=true;
        passed_entry(path,message_bad_text,nullptr);passed_entry(path,message_bad_text,L"");
        passed_entry(path,message_bad_text,reinterpret_cast<const wchar_t *>(1));
        std::wstring overlong(Inbox::text_limit+1,L'?');passed_entry(path,message_bad_text,overlong.c_str());
        // Full inbox: store itself triggers fallback, then this same call stays native.
        auto saved=std::move(inbox.events);inbox.events.assign(Inbox::limit,{Kind::News,L"?"});
        unsigned pushes=entry_pushes,yields=entry_yields;
        passed_entry(path,message_store_failed);
        check(inbox.fallback && entry_pushes==pushes+1 && entry_yields==yields+1,"store failure restores native push and caller coroutine yield for that same call");
        inbox.events=std::move(saved);inbox.fallback=false;
    }
    narration(state_a.data());check(pops==before && original_notices==original+1,"narration state bypassing entries always runs its original frame without collection/pop");
    // User 2026-10-26: refusing a money demand crashed local4. Choice class
    // separation alone did not protect the subsequent shared message classes.
    // User 2026-10-27: every passive progress notice is redirected, including
    // text that previously failed the category whitelist. Choices stay native.
    check(passive_notice(L"공손강군이 군정을 입수했다고 합니다.") && passive_notice(L"가후의 건물파괴가 성공했습니다.") && passive_notice(L"강릉에서 부손님이 등방님을 발견했습니다."),"military points, plot outcomes and discoveries do not require a category keyword to be collected");
    // 사용자 결정 2026-10-28: 클릭형은 모두 수집; state types, not ?, protect choices.
    check(passive_notice(L"이 제안을 받아들이시겠습니까?") && passive_notice(L"무슨 일이냐!？") && !passive_notice(L""),"user 2026-10-28: question punctuation does not exclude click-only notices; empty payloads remain native");
    check(classify(L"강릉에서 우금님이 조루님을 발견했습니다.")==Kind::Recruitment && gold_amount(L"우금이 금 1200을 발견했습니다.")==1200,"the officer name 우금 is neither a money token nor a gold discovery category");
    std::vector<std::wstring> names;collect_names(L"^02가후^00가 ^05진창^00에서 ^02순욱^00과 ^02가후^00에게 금 ^02+1000^00을 보고",names);
    check(names.size()==2 && names[0]==L"가후" && names[1]==L"순욱","person markers create distinct name tokens without coloring places or positive numbers");
    Event plot;plot.text=L"^02가후^00, ^05진창^00에서 건물파괴 성공\n성공이로구나. 진의 내구를 대폭 저하시켰으니.";
    check(has(compact(plot),L"가후가 진창에서 건물파괴에 성공했습니다.") && has(compact(plot),L"주공, 임무가 성공해 다행입니다.") && !has(compact(plot),L"성공이로구나") && has(plot.text,L"성공이로구나"),"plot summary uses the adviser's voice while preserving the monarch's original quote");
    check(interactive_state("CStrategyDiplomacyState",22) && interactive_state("CTreatPrisonerState",18) && !interactive_state("CProgressState",14),"diplomacy and prisoner interactions are protected independently of message wording");
    cycle();strcpy_s(reinterpret_cast<char *>(state_b.data()+0x70),128,"CTreatDiplomacyState");
    manager_stack[0]=state_b.data();manager_stack[1]=state_a.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=2;
    before=pops;original=original_notices;narration(state_a.data());
    check(interaction() && pops==before && original_notices==original+1 && inbox.events.empty(),"a diplomacy parent below the notification protects its entire continuation");
    report_enter(state_a.data());report_frame(state_a.data());report_wait(state_a.data());
    check(pops==before && !report_deferred && inbox.events.empty(),"report and report-wait bypass cannot pop a diplomacy continuation");report_leave(state_a.data(),nullptr);
    // 사용자 2026-10-28 설계 전환: 직접 pop으로 CProgressState 소실, 원작 유지.
    // Replace immediate-close assertions with native pass-through for all message states.
    for(unsigned type=0;type<2;++type) {
        cycle();reset_pages(state_a.data());point(state_a.data(),0,image+(type?0x13A2F98:0x13A2F00));
        number(dialog_mem.data(),0x20,21);number(dialog_mem.data(),0x24,34);
        before=pops;unsigned entered_before=original_enters;unsigned acks=acknowledgements;
        check((type?person_notice_enter(state_a.data()):simple_enter(state_a.data()))==1 && original_enters==entered_before+1,"general/person Enter always invokes the native implementation");
        for(bool ready:{false,true}){dialog_ready=ready;number(animation_mem.data(),0x178,ready?0:1);simple_frame(state_a.data());simple_frame(state_a.data());}
        check(inbox.events.empty() && pops==before && simple_states.empty() && acknowledgements==acks && dword(dialog_mem.data(),0x178)==0xFFFFFFFF && dword(dialog_mem.data(),0x20)==21 && dword(dialog_mem.data(),0x24)==34,"entry-bypassing message is never collected, moved, acknowledged or popped by mod");
    }
    dialog_ready=true;unsigned ack=acknowledgements;
    cycle();reset_pages(state_a.data());
    manager_stack[0]=state_b.data();manager_stack[1]=state_a.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=2;
    log_frame(dialog_mem.data());check(!briefing_overlay::shown && !briefing_overlay::scene_clean,"diplomacy log frame hides bubble and forbids clean scene capture");
    manager_stack[0]=state_a.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=1;
    log_frame(dialog_mem.data());check(briefing_overlay::shown && briefing_overlay::scene_clean,"next log frame restores visibility and clean flag without another store");
    // User 2026-11-02: final reports collect without building/hiding native UI.
    {
        cycle();for(unsigned offset:{0x480u,0x488u,0x490u})point(state_a.data(),offset,nullptr);number(state_a.data(),0x470,4);
        point(state_a.data(),0x488,reinterpret_cast<Obj>(static_cast<std::uintptr_t>(0xDEADBEEF)));point(state_a.data(),0x490,reinterpret_cast<Obj>(static_cast<std::uintptr_t>(1)));
        point(root.data(),0xDF30+8,force_mem.data());point(root.data(),0xE0D0+8,district_mem.data());
        point(root.data(),0xE0D0+16,second_district_mem.data());
        district_mem[0x10]=second_district_mem[0x10]=1;district_mem[0x11]=0;second_district_mem[0x11]=8;
        point(root.data(),0x79E90,world_report.data());world_report[0x82]=3;world_report[0x86]=1;
        wcscpy_s(reinterpret_cast<wchar_t *>(world_report.data()+0x14),0x37,L"홍수가 발생했습니다.");
        wcscpy_s(reinterpret_cast<wchar_t *>(world_report.data()+0x8A),0x55,L"군량 피해가 발생했습니다.");
        report_results_call.published=true;user_enter(state_a.data(),nullptr);
        unsigned builds=report_enters,applied=result_applications,setters=text_calls,constructed=record_constructions,destroyed=record_destructions;
        unsigned cameras=camera_restores;briefing_overlay::mask_report(false);
        check(report_enter(state_a.data())==1 && report_enters==builds && report_headless,"headless Enter succeeds without native report construction");
        check(result_applications==applied+1 && record_constructions==constructed+262 && record_destructions==destroyed+262,"native result generation once and all initialized record lifetimes balanced");
        check(results_saw_consumed && income_after_application==applied+1,"pending flags are consumed before native result writers and income reads their resulting state");
        check(text_calls==setters && !briefing_overlay::report_mask && !pointer(state_a.data(),0x480) && !pointer(state_a.data(),0x488) && !pointer(state_a.data(),0x490),"headless numeric formatter creates no controls, forwards no setters and needs no render mask");
        check((world_report[0x82]&7)==5 && std::count_if(inbox.events.begin(),inbox.events.end(),[](const Event &e){return e.kind==Kind::Income;})==2,"native report consumption bits and both districts preserved");
        check(std::any_of(inbox.events.begin(),inbox.events.end(),[](const Event &e){return e.kind==Kind::Income && has(e.report,L"금\t2,500\t(-3,655)\t0\t3,655");}),"headless numeric values agree with UI capture formatting");
        manager_stack[0]=state_b.data();unsigned ticks=report_ticks;report_frame(state_a.data());
        check(report_ticks==ticks && report_headless,"covered headless state cannot finish another modal");
        manager_stack[0]=state_a.data();unsigned popped=pops;report_frame(state_a.data());
        check(pops==popped+1 && !report_headless && camera_restores==cameras,"first active frame uses native completion without camera restoration or a second report phase");
        report_leave(state_a.data(),nullptr);check(!report_state,"headless Leave retains original lifecycle");
        // User live failure 2026-11-02: native non-applicable military delta
        // is explicitly empty. A previous numeric setter must be cleared.
        cycle();for(unsigned offset:{0x480u,0x488u,0x490u})point(state_a.data(),offset,nullptr);number(state_a.data(),0x470,4);
        world_report[0x82]=3;income_blank_military=true;builds=report_enters;applied=result_applications;
        check(report_enter(state_a.data())==1 && report_headless && !inbox.fallback && report_enters==builds && result_applications==applied+1,"native blank military delta keeps the UI-free route and applies results once");
        check(std::count_if(inbox.events.begin(),inbox.events.end(),[](const Event &e){return e.kind==Kind::Income;})==2 && std::any_of(inbox.events.begin(),inbox.events.end(),[](const Event &e){return e.kind==Kind::Gold;}) && std::any_of(inbox.events.begin(),inbox.events.end(),[](const Event &e){return e.kind==Kind::Disaster;}),"blank military cell cannot discard final mission, world report or multi-district income");
        check(std::all_of(inbox.events.begin(),inbox.events.end(),[](const Event &e){return e.kind!=Kind::Income || has(e.report,L"군정\t110,000\t\t해당 없음\t해당 없음");}),"explicit blank overwrites stale numeric delta and preserves localized N/A strings without invented zero");
        report_frame(state_a.data());report_leave(state_a.data(),nullptr);income_blank_military=false;
        for(unsigned invalid=0;invalid<2;++invalid) {
            cycle();for(unsigned offset:{0x480u,0x488u,0x490u})point(state_a.data(),offset,nullptr);number(state_a.data(),0x470,4);
            income_omit_delta=invalid==0;income_blank_balance=invalid==1;builds=report_enters;applied=result_applications;
            check(report_enter(state_a.data())==7 && inbox.fallback && report_enters==builds+1 && result_applications==applied+1,"missing setter or explicitly emptied balance still restores native UI without repeated effects");
            report_leave(state_a.data(),nullptr);income_omit_delta=income_blank_balance=false;
        }
        for(unsigned mode=0;mode<5;++mode) {
            cycle();for(unsigned offset:{0x480u,0x488u,0x490u})point(state_a.data(),offset,nullptr);number(state_a.data(),0x470,4);
            if(mode==0)second_district_mem[0x11]=0;
            if(mode==1)briefing_overlay::gpu_ready=false;
            if(mode==2)tutorial_flag=1;
            if(mode==3)strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CDiplomacyState");
            if(mode==4)report_results_call.published=false;
            applied=result_applications;unsigned long long answer=0;
            check(!try_headless_report(state_a.data(),answer) && result_applications==applied,"unsafe/unsupported headless preflight performs no result writes");
            second_district_mem[0x11]=8;briefing_overlay::gpu_ready=true;tutorial_flag=0;report_results_call.published=true;
        }
        cycle();for(unsigned offset:{0x480u,0x488u,0x490u})point(state_a.data(),offset,nullptr);number(state_a.data(),0x470,4);
        invalid_report_record=true;applied=result_applications;builds=report_enters;action_mem[0x1B00]=0x7E;
        check(report_enter(state_a.data())==7 && result_applications==applied+1 && report_enters==builds+1 && inbox.fallback && !replay_results,"bad generated text restores native UI with one result application through caller-local replay");
        check(action_mem[0x1B00]==0x7E,"fallback replay preserves each native record header and reference ownership");
        invalid_report_record=false;report_leave(state_a.data(),nullptr);
        cycle();for(unsigned offset:{0x480u,0x488u,0x490u})point(state_a.data(),offset,nullptr);number(state_a.data(),0x470,4);
        income_numbers=false;applied=result_applications;builds=report_enters;
        check(report_enter(state_a.data())==7 && result_applications==applied+1 && report_enters==builds+1 && inbox.fallback,"unknown numeric format restores native UI without repeating result writers");
        income_numbers=true;report_leave(state_a.data(),nullptr);
        report_results_call.published=false;point(root.data(),0xE0D0+16,nullptr);
    }
    Inbox merge;merge.begin(1,1);Event early;early.day=1;early.text=L"건물파괴에 성공한 것 같군. 진창에 있는 진의 내구를 줄였군.";
    Event report;report.day=10;report.text=L"가후, 진창에서 건물파괴 성공";merge.append({early});merge.append({report},true);
    check(merge.events.size()==1 && merge.events[0].day==1 && has(merge.events[0].text,early.text.c_str()) && has(merge.events[0].text,report.text.c_str()),"same-cycle mission and location merge with earliest day and both originals");
    merge.begin(1,1);merge.append({early});report.text=L"가후, 장안에서 건물파괴 성공";merge.append({report},true);check(merge.events.size()==2,"different mission locations remain separate");
    cycle();point(root.data(),0x79E90,world_report.data());world_report[0x12]=1;world_report[0x82]=2;world_report[0x86]=1;
    wcscpy_s(reinterpret_cast<wchar_t *>(world_report.data()+0x8A),0x55,L"홍수가 발생했습니다.");
    user_enter(state_a.data(),nullptr);check(!inbox.collecting && inbox.has_cycle,"native report follows player Enter in the real lifecycle");
    second_district=true;
    unsigned entered=report_enters;check(report_enter(state_a.data())==7 && report_enters==entered+1,"original report construction applies once and preserves result");
    check((world_report[0x82]&4) && inbox.events.size()==2 && report_deferred,"pre-consumption world log and generated report both retained");
    before=pops;report_frame(state_a.data());check(camera_restores==1 && report_completions==1 && pops==before+1,"native report finalization and camera restoration still execute");
    auto income=std::find_if(inbox.events.begin(),inbox.events.end(),[](const Event &e){return e.kind==Kind::Income;});
    check(income!=inbox.events.end() && has(income->text,L"2500") && !has(income->text,L"2000"),"income capture retains final values instead of stale intermediate text");
    check(mask_seen_before_native_report && briefing_overlay::report_mask,"report masking begins before native constructor controls can be presented");
    check(has(income->report,L"항목\t보유량\t변동\t수입\t지출") && has(income->report,L"금\t2,500\t(-3,655)\t0\t3,655"),"income summary preserves native field identity as five fixed columns, with grouped signed numbers");
    check(std::count_if(inbox.events.begin(),inbox.events.end(),[](const Event &e){return e.kind==Kind::Income;})==2 && has(inbox.events.back().text,L"제8군단") && has(inbox.events.back().text,L"750"),"every listed district including district eight is formatted separately after the constructor only supplied headings");
    second_district=false;
    report_leave(state_a.data(),nullptr);check(!report_state,"native report Leave and completion path retained");
    before=pops;report_wait(state_a.data());check(pops==before+1,"final native report wait is bypassed after player Enter");
    before=original_notices;narration(state_a.data());check(original_notices==before+1,"ordinary player standby narration stays native after final report");
    cycle();income_numbers=false;report_enter(state_a.data());unsigned completed=report_completions;report_frame(state_a.data());
    check(inbox.fallback && report_completions==completed && dword(background_mem.data(),0x20)==0,"unknown income representation restores original report instead of discarding it");report_leave(state_a.data(),nullptr);income_numbers=true;
    cycle();inbox.append({{Kind::News,L"지난 순 소식"}});check(user_enter(state_a.data(),nullptr)==31 && !inbox.collecting && inbox.events.size()==1,"return to player turn retains exactly the completed cycle");
    briefing_overlay::ticket=inbox.generation;briefing_overlay::action=1;hud(nullptr,true,false);check(open && inbox.unread()==0,"portrait opens and marks displayed group read");
    briefing_overlay::ticket=inbox.generation-1;briefing_overlay::action=6;hud(nullptr,true,false);check(inbox.events.size()==1,"stale input from old cycle cannot delete new reports");
    briefing_overlay::ticket=inbox.generation;briefing_overlay::action=6;hud(nullptr,true,false);check(inbox.events.empty(),"clear button deletes this cycle only");
    cycle();store({{Kind::News,L"유비가 금 200을 발견했습니다."},{Kind::News,L"관우가 금을 발견했습니다."}});user_enter(state_a.data(),nullptr);hud(nullptr,true,false);
    check(!has(briefing_overlay::displayed.body,L"발견한 금 합계") && has(briefing_overlay::displayed.body,L"관우가 금을"),"unknown amounts stay visible and a partial sum is never presented as the full total");
    cycle();store({{Kind::News,L"통솔이 성장했습니다."}});user_enter(state_a.data(),nullptr);open=false;
    briefing_overlay::ticket=inbox.generation;briefing_overlay::action=1;unsigned frames=original_frames;user_frame(state_a.data());
    check(original_frames==frames+1 && open && !briefing_overlay::action && inbox.unread()==0,"next player frame opens and reads the bubble without a command-HUD update");
    briefing_overlay::ticket=inbox.generation;briefing_overlay::action=2;user_frame(state_a.data());check(!open,"next player frame closes the bubble");
    // User 2026-10-27: command-menu auto-hide and inbox publishing must agree.
    hud(nullptr,false,false);check(briefing_overlay::shown,"hidden command menu cannot hide the standby inbox");
    briefing_overlay::ticket=inbox.generation;briefing_overlay::action=1;hud(nullptr,false,false);
    check(open && briefing_overlay::shown,"a click remains actionable when the native command menu is hidden");
    for(unsigned i=0;i<8;++i){user_frame(state_a.data());hud(nullptr,false,false);check(briefing_overlay::shown && open,"alternating native HUD and player frame cannot flicker or close the inbox");}
    briefing_overlay::shown=false;user_frame(state_a.data());check(briefing_overlay::shown,"standby frame restores an inbox hidden by the previous publisher");
    player_standby=false;hud(nullptr,true,false);check(!briefing_overlay::shown,"another native state still hides the inbox even when the command menu is visible");player_standby=true;
    open=false;
    // User 2026-10-27, local8 read-only evidence: manager+48 cycles through
    // CGameState/CRootState during normal standby; it is not a modal window.
    modal_active=true;briefing_overlay::ticket=inbox.generation;briefing_overlay::action=1;user_frame(state_a.data());
    check(open && briefing_overlay::shown,"a nonzero currently-updating state cannot hide or block standby input");
    for(unsigned i=0;i<8;++i){hud(nullptr,true,true);user_frame(state_a.data());check(open && briefing_overlay::shown,"native parent-frame bookkeeping cannot clear the inbox between mouse down and up");}
    modal_active=false;open=false;
    player_standby=false;briefing_overlay::ticket=inbox.generation;briefing_overlay::action=1;hud(nullptr,true,false);
    check(!open && !briefing_overlay::shown,"a real dialog above the player state blocks input even with manager+48 empty");player_standby=true;
    frame_replaces_state=true;briefing_overlay::ticket=inbox.generation;briefing_overlay::action=1;user_frame(state_a.data());
    check(!open && briefing_overlay::action==1,"an original frame that leaves the player state cannot dispatch into its replacement");frame_replaces_state=false;briefing_overlay::action=0;
    cycle();store({{Kind::Weather,L"천시가 변했습니다."},{Kind::Gold,L"유비가 금 50을 발견했습니다."},{Kind::Disaster,L"홍수가 발생했습니다."}});user_enter(state_a.data(),nullptr);
    briefing_overlay::ticket=inbox.generation;briefing_overlay::action=1;hud(nullptr,true,false);
    briefing_overlay::ticket=inbox.generation;briefing_overlay::action=4;hud(nullptr,true,false);
    check(page==1 && inbox.unread()==1 && has(briefing_overlay::displayed.title,L"뜻밖의 수확") && has(briefing_overlay::displayed.body,L"50"),"page navigation reuses its grouped snapshot while keeping new-page content and unread counts current");
    briefing_overlay::ticket=inbox.generation;briefing_overlay::action=4;hud(nullptr,true,false);briefing_overlay::ticket=inbox.generation;briefing_overlay::action=3;hud(nullptr,true,false);
    check(page==1 && inbox.unread()==0,"forward/back navigation preserves read state without rebuilding every grouped event multiple times");
    briefing_overlay::ticket=inbox.generation;briefing_overlay::action=5;hud(nullptr,true,false);
    check(inbox.events.size()==2 && page==0 && !has(briefing_overlay::displayed.title,L"뜻밖의 수확"),"deleting the selected group rebuilds its snapshot instead of retaining a removed page");
    // User 2026-10-26/27: collect completed diplomacy dialogue while keeping
    // decisions and the original page, animation and coroutine callbacks.
    check(display_text(L"^02유비^00가 ^05금^00 1,200을 발견했습니다.")==L"유비가 금 1,200을 발견했습니다.","native formatting controls do not appear as literal caret codes in the bubble");
    Inbox colored;colored.begin(1,1);colored.append({{Kind::News,L"^02유비^00가 금 ^051,200^00을 발견했습니다."}});
    check(colored.events[0].gold==1200 && compact(colored.events[0])==L"유비  ·  1200" && has(colored.events[0].text,L"^02"),"presentation removes controls without changing retained source text or gold totals");
    cycle();point(state_b.data(),0,image+0x145DBB8);number(state_b.data(),0x4D8,11);
    manager_stack[0]=state_b.data();point(state_a.data(),0,image+0x145F9E8);
    strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"CDialogGameStateBase<CCmnDlgMsgDialog_TreatDiplomacy>");
    reset_pages(state_a.data(),L"거절의 뜻은 알겠소.",L"그럼 다음을 기약하겠소.");
    unsigned pages=native_pages;check(diplomatic_enter(state_a.data())==1 && inbox.events.size()==2 && inbox.events[0].kind==Kind::Diplomacy,"message-only diplomacy pages are retained even when their text is a refusal");
    manager_stack[1]=state_a.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=2;
    before=pops;ack=acknowledgements;number(diplomacy_window.data(),0x22C,1);simple_frame(state_a.data());
    check(acknowledgements==ack && native_pages==pages+1 && pops==before,"diplomacy percentage animation must finish before advancing its message");
    number(diplomacy_window.data(),0x22C,0);simple_frame(state_a.data());simple_frame(state_a.data());
    check(native_pages==pages+2 && native_close_pending && pops==before,"diplomacy pages use original consumption and original closing without direct pop");mock_native_close_event();
    cycle();point(state_b.data(),0x478,event_layout.data());point(event_layout.data(),0x190,speech_left.data());point(event_layout.data(),0x198,speech_right.data());
    point(state_b.data(),0x4C0,diplomacy_view.data());point(diplomacy_view.data(),0x188,diplomacy_window.data());
    point(diplomacy_window.data(),0x180,other_force.data());point(diplomacy_window.data(),0x190,force_mem.data());point(diplomacy_window.data(),0x1D8,command_mem.data());point(command_mem.data(),0,command_vt);command_vt[19]=reinterpret_cast<void *>(&mock_command_kind);
    *reinterpret_cast<unsigned short *>(force_mem.data()+0x10)=100;*reinterpret_cast<unsigned short *>(own_person.data()+0x10)=100;*reinterpret_cast<unsigned short *>(other_person.data()+0x10)=200;
    number(speech_left.data(),0x20,40);number(speech_left.data(),0x24,30);number(state_b.data(),0x4D8,13);manager_stack[0]=state_b.data();
    unsigned spoken=native_speeches;diplomatic_speech(state_b.data(),3,own_person.data(),L"그런 교환에는 응할 수 없소.",nullptr);
    check(native_speeches==spoken+1 && inbox.events.size()==1 && inbox.events[0].kind==Kind::Diplomacy && speech_waiting && dword(speech_left.data(),0x20)==10000,"final result speech is copied and hidden only after the original display setup ran");
    check(has(compact(inbox.events[0]),L"주군께서 유도 측의 교환 요청을 거절하셨습니다.") && has(inbox.events[0].text,L"조조\n그런 교환에는 응할 수 없소."),"own ruler dialogue becomes an adviser report while the full original remains available");
    DiplomaticFacts facts;facts.counterpart=L"유도";facts.kind=7;facts.result=0;facts.known_sides=true;facts.incoming=true;facts.friendly=facts.ruler=true;
    check(has(diplomatic_report(L"곤란하오.",facts),L"주군께서") && !has(diplomatic_report(L"곤란하오.",facts),L"곤란하오"),"briefing does not impersonate the player's ruler");
    facts.incoming=false;check(has(diplomatic_report(L"유감이군.",facts),L"제안하신 교환 교섭이 성사되지"),"a failed outgoing request is not reported as the player refusing an incoming one");
    facts.incoming=true;facts.result=1;check(has(diplomatic_report(L"거절할 까닭이 없소.",facts),L"받아들이셨습니다"),"a known successful result takes precedence over an ambiguous quoted keyword");
    facts.friendly=facts.ruler=false;auto reaction=diplomatic_report(L"나를 이토록 무시하다니. 일단은 물러나겠소.",facts);
    check(has(reaction,L"유도 측이 불만을 표하고 물러났습니다.") && !has(reaction,L"관계"),"reported displeasure does not invent a numerical diplomatic relationship loss");
    point(state_a.data(),0,image+0x1465260);strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"EventView::CInputWaitState");
    manager_stack[1]=state_a.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=2;number(state_a.data(),0x478,0x7FFFFFFF);
    before=pops;wait_ready=false;diplomatic_wait(state_a.data());check(pops==before && speech_waiting,"native input/animation gates still control result speech continuation");
    wait_ready=true;wait_reenter=true;diplomatic_wait(state_a.data());
    check(pops==before+1 && speech_waiting && inbox.events.size()==2,"a synchronous next speech callback cannot lose its newly queued confirmation");
    manager_stack[1]=state_a.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=2;number(state_a.data(),0x478,0x7FFFFFFF);diplomatic_wait(state_a.data());
    check(pops==before+2 && !speech_waiting,"each result input wait completes through the original native frame once");
    number(state_b.data(),0x4D8,8);hud(nullptr,true,false);check(dword(speech_left.data(),0x20)==40 && !speech_owner,"leaving the result phase restores still-owned native speech layouts");
    auto retained=inbox.events.size();diplomatic_speech(state_b.data(),3,nullptr,L"조건을 선택해 주시오.",nullptr);
    manager_stack[1]=state_a.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=2;number(state_a.data(),0x478,0x7FFFFFFF);before=pops;diplomatic_wait(state_a.data());
    check(inbox.events.size()==retained && pops==before && dword(state_a.data(),0x478)==0x7FFFFFFF,"pre-decision speech and unowned input waits remain untouched");
    number(state_b.data(),0x4D8,13);strcpy_s(reinterpret_cast<char *>(state_c.data()+0x70),128,"CDialogGameStateBase<CCmnDlgMsgYesNo>");
    manager_stack[1]=state_c.data();manager_stack[2]=state_a.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=3;
    diplomatic_speech(state_b.data(),4,nullptr,L"다시 결정하시겠소?",nullptr);diplomatic_wait(state_a.data());
    check(inbox.events.size()==retained && pops==before && dword(state_a.data(),0x478)==0x7FFFFFFF,"a real decision nested in a result scene cannot be auto-confirmed");
    // 사용자 2026-10-28 설계 전환 2단계: replace recruitment/passive display,
    // hide/restore and timer auto-advance assertions with entry-only collection.
    cycle();before=pops;unsigned pushes=entry_pushes,yields=entry_yields;
    point(root.data(),0x158+5012*8,other_person.data());
    person_call(5012,L"손권군이 군정을 입수했습니다.");
    check(inbox.events.size()==1 && has(inbox.events.back().text,L"유도\n손권군") && inbox.events.back().speaker==200,"person entry retains native PersonID/name and text");
    check(pops==before && entry_pushes==pushes && entry_yields==yields,"person capture creates neither window nor coroutine yield/pop");
    for(bool collect:{false,true}) {
        inbox.collecting=collect;native_message_mode=collect?1:0;unsigned calls=entry_relays[1];
        person_call(5012,L"native person");check(entry_relays[1]==calls+1 && relayed_speaker==5012 && relayed_text==std::wstring(L"native person"),"player turn/native display mode relays the exact person arguments");
    }
    native_message_mode=0;
    {   // 사용자 2026-11-01: duel lines are collected and still shown natively; duel timers are not expired.
        cycle();std::array<unsigned char,0x20> duel{};*reinterpret_cast<unsigned *>(duel.data()+0x10)=1;point(image,0x2783730,duel.data());
        unsigned calls=entry_relays[1];person_call(5012,L"일기토다!");
        check(inbox.events.size()==1 && has(inbox.events.back().text,L"일기토다!") && entry_relays[1]==calls+1,"duel person line stored and relayed");
        unsigned waits=time_wait_relays;int result=7;time_wait(500,&result,1);check(time_wait_relays==waits+1,"duel wait relays to the native timer instead of expiring");
        point(image,0x2783730,nullptr);
    }
    const wchar_t *extra=L"extra";
    auto event_call=[&](Obj person=other_person.data(),const wchar_t *text=L"무슨 일이냐!？") {event_speech(state_b.data(),person,text,0x123456789ABCull,extra,7);};
    auto check_event_capture=[&]() {
        auto count=inbox.events.size();unsigned calls=native_recruit_speeches,prev_push=entry_pushes,prev_yield=entry_yields,prev_pop=pops,moves=native_moves;
        event_call();
        check(inbox.events.size()==count+1 && inbox.events.back().speaker==200 && has(inbox.events.back().text,L"유도\n"),"allowed owner stores original speaker and full text at entry");
        check(native_recruit_speeches==calls && entry_pushes==prev_push && entry_yields==prev_yield && pops==prev_pop && native_moves==moves,"captured EventView never displays, pushes, yields, moves layouts or pops");
    };
    auto check_event_relay=[&]() {
        auto count=inbox.events.size();unsigned calls=native_recruit_speeches;
        event_call();
        check(inbox.events.size()==count && native_recruit_speeches==calls+1 && relayed_owner==state_b.data() && relayed_person==other_person.data() && relayed_text==std::wstring(L"무슨 일이냐!？") && recruitment_flags==0x123456789ABCull && recruitment_extra==extra && recruitment_style==7,"protected EventView relays all six arguments unchanged and stores nothing");
    };
    for(unsigned rva:event_owners) {
        cycle();manager_stack[0]=state_b.data();point(state_b.data(),0,image+rva);strcpy_s(reinterpret_cast<char *>(state_b.data()+0x70),128,"CAllowedNoticeState");
        point(state_b.data(),0x4B0,own_person.data());point(state_b.data(),0x4B8,other_person.data());number(state_b.data(),0x4C8,2);
        check_event_capture();
        inbox.finish();check_event_relay();inbox.collecting=true;
        native_message_mode=1;check_event_relay();native_message_mode=0;
        enabled=0;check_event_relay();enabled=1;
        tutorial_flag=1;check_event_relay();tutorial_flag=0;
        briefing_overlay::gpu_ready=false;check_event_relay();briefing_overlay::gpu_ready=true;
        inbox.fallback=true;check_event_relay();inbox.fallback=false;
    }
    cycle();manager_stack[0]=state_b.data();point(state_b.data(),0,image+0x1440FF8);
    for(unsigned phase:{1u,2u,3u}) {
        number(state_b.data(),0x4C8,phase);Obj person=phase==1?own_person.data():other_person.data();
        event_call(person,L"등용 대사");
        check(inbox.events.back().kind==Kind::Recruitment && has(inbox.events.back().report,phase==1?L"등용 임무":phase==2?L"성공했습니다.":L"실패했습니다."),"recruitment phase and adviser summary survive entry interception");
    }
    number(state_b.data(),0x4C8,4);check_event_relay();number(state_b.data(),0x4C8,1);check_event_relay();
    // An owner's name cannot grant collection; only its independently verified vtable can.
    for(unsigned rva:{0x1465410u,0x13A2EA8u,0x145DBB8u,0x1300000u}){point(state_b.data(),0,image+rva);check_event_relay();}
    point(state_b.data(),0,image+0x14471E8);
    for(const char *name:{"CStrategyDiplomacyState","CTreatPrisonerState","CCmnDlgMsgYesNoState","CSelectState","CInputState","CTutorialState","CStrategyFirstExplainTalkState"}) {
        strcpy_s(reinterpret_cast<char *>(state_c.data()+0x70),128,name);manager_stack[1]=state_c.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=2;
        check_event_relay();unsigned calls=entry_relays[1];person_call(5012,L"protected person");check(entry_relays[1]==calls+1,"interaction protects the new person entry");
    }
    *reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=1;
    auto count=inbox.events.size();unsigned calls=native_recruit_speeches;
    for(const wchar_t *text:{static_cast<const wchar_t *>(nullptr),L"",reinterpret_cast<const wchar_t *>(1)})event_call(other_person.data(),text);
    check(inbox.events.size()==count && native_recruit_speeches==calls+3,"null, empty and unreadable text retain native EventView");
    auto saved_events=std::move(inbox.events);inbox.events.assign(Inbox::limit,{Kind::News,L"full"});check_event_relay();
    check(inbox.fallback,"store failure restores native EventView on the same call");inbox.events=std::move(saved_events);inbox.fallback=false;
    // Removed collectors must no longer shorten arbitrary native InputWait states.
    speech_waiting=false;point(state_a.data(),0,image+0x1465260);strcpy_s(reinterpret_cast<char *>(state_a.data()+0x70),128,"EventView::CInputWaitState");
    manager_stack[1]=state_a.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=2;number(state_a.data(),0x478,0x7FFFFFFF);
    before=pops;diplomatic_wait(state_a.data());check(pops==before && dword(state_a.data(),0x478)==0x7FFFFFFF,"non-diplomatic wait has no recruitment/passive auto-confirm path");
    // Legacy coordinates move only on a transition; stable frames stay put.
    cycle();std::array<unsigned char,0x500> advice_mem{};std::array<unsigned char,0x200> face_node{};Obj face_nodes[6]{};float matrix[16]{};
    matrix[0]=matrix[5]=.3125f;matrix[10]=matrix[15]=1;matrix[12]=-864.375f;matrix[13]=-424;
    face_nodes[5]=face_node.data();face_node[0x10C]=0x11;point(face_node.data(),0xE8,matrix);point(advice_mem.data(),0x138,face_nodes);number(advice_mem.data(),0x140,6);
    point(advice_mem.data(),0x448,other_person.data());old_log={};old_advice={};briefing_overlay::captured_serial=portrait_serial+1;
    // User 2026-10-28: the native advice and log stay visible as in the
    // original game; the mod never moves them, and the portrait still works.
    unsigned moves=native_moves;
    for(unsigned i=0;i<100;++i){log_frame(dialog_mem.data());advice_frame(advice_mem.data());}
    check(native_moves==moves && !old_log.moved && !old_advice.moved,"native advice/log stay in place for 100 frames");
    number(advice_mem.data(),0x20,0);number(advice_mem.data(),0x24,0);advice_frame(advice_mem.data());
    check(native_moves==moves && dword(advice_mem.data(),0x20)==0 && dword(advice_mem.data(),0x24)==0,"native advice refresh position is left as is");
    number(other_person.data(),0xA6,word(other_person.data(),0xA6)+1);unsigned requests=briefing_overlay::portrait_requests;advice_frame(advice_mem.data());
    check(native_moves==moves && briefing_overlay::requested_serial==portrait_serial && briefing_overlay::portrait_requests>requests,"changed adviser art still requests portrait without moves");
    fallback();for(unsigned i=0;i<10;++i){log_frame(dialog_mem.data());advice_frame(advice_mem.data());}
    check(native_moves==moves,"fallback frames never move native layouts");
    inbox.fallback=false;enabled=0;log_frame(dialog_mem.data());advice_frame(advice_mem.data());enabled=1;
    log_leave(dialog_mem.data(),nullptr);advice_leave(advice_mem.data(),nullptr);
    check(native_moves==moves && !old_log.object && !old_advice.object,"native teardown forgets both layouts without moves");
#ifndef NO_RUNTIME_LOG
    for(unsigned row=0;row<4;++row)check(frame_metrics[row][0]>0 && frame_metrics[row][1]>=frame_metrics[row][2],"QPC scopes count calls and cumulative/max microseconds");
    // User 2026-10-28: native HUD kept, so log/advice frames attribute no moves.
    check(layout_move_calls==native_moves && frame_metrics[1][3]==0 && frame_metrics[2][3]==0,"all mod layout calls are counted; log/advice frames move nothing");
    cycle();manager_stack[0]=state_b.data();point(state_b.data(),0,image+0x1302000);strcpy_s(reinterpret_cast<char *>(state_b.data()+0x70),128,"CUnknownNoticeState");
    log_frame(dialog_mem.data());log_frame(dialog_mem.data());check(uncollected_top_rvas.size()==1,"uncollected-state diagnostics still deduplicate per cycle");
    auto monotonic=frame_metrics[1][0];reset();check(frame_metrics[1][0]==monotonic,"reset preserves cumulative performance diagnostics");
#endif
    time_wait_checks();picture_checks();scene_entry_checks();
    game_leave(state_a.data(),nullptr);check(!inbox.world && inbox.events.empty(),"game teardown clears all world references");
    // 사용자 결정 2026-10-28 드래그 헤드: pure UI math, no game calls.
    HeadMotion ui_head;ui_head.press(74,1000);check(!ui_head.move(81.9f,1000) && ui_head.release(),"under eight virtual pixels is a tap");
    ui_head.press(74,1000);check(ui_head.move(82,1000) && !ui_head.release(),"eight virtual pixels starts a drag, never a tap");
    ui_head.cancel();ui_head.press(ui_head.x,ui_head.y);ui_head.move(1700,-400);ui_head.release();
    check(ui_head.target_x==1846 && ui_head.y==110,"nearest right edge and upper y clamp");
    for(int frame_index=0;frame_index<22;++frame_index){ui_head.advance(1.0/60);check(ui_head.x>=74 && ui_head.x<=1846 && ui_head.y>=110 && ui_head.y<=1000,"snap spring stays within allowed screen bounds");}
    check(!ui_head.snapping && ui_head.x==1846 && ui_head.y==110,"damped spring converges within 367ms");
    Rect right_panel=panel_rect(ui_head.x);check(right_panel.x+right_panel.w==ui_head.x-68 && right_panel.x>=16 && right_panel.x+right_panel.w<=1904,"right snapped panel is on the head's left and inside screen");
    for(float hx:{74.0f,600.0f,959.0f,960.0f,1400.0f,1846.0f}){Rect panel=panel_rect(hx);check(panel.x>=16 && panel.x+panel.w<=1904 && panel.y==30,"dynamic panel clamps both edges without changing size");for(unsigned id=2;id<=7;++id){Rect item=control(id,panel);check(panel.contains(item.x,item.y) && panel.contains(item.x+item.w-1,item.y+item.h-1),"dynamic controls stay within their drawn panel");}}
    ScrollMotion ui_scroll;ui_scroll.wheel(80,2000);check(ui_scroll.shown==0 && ui_scroll.target==80,"wheel changes target without an immediate position jump");ui_scroll.advance(.12,2000);check(ui_scroll.shown>71.9 && ui_scroll.shown<72.1,"scroll reaches ninety percent in 120ms");
    ui_scroll.press(300);ui_scroll.move(305,.016,2000);check(!ui_scroll.dragging,"body motion below six pixels remains a click");ui_scroll.move(306,.016,2000);check(ui_scroll.dragging,"six pixels begins finger scrolling");ui_scroll.move(900,.016,2000);check(ui_scroll.shown>=-48 && ui_scroll.shown<0,"overscroll uses a bounded rubber band");ui_scroll.release();
    for(int frame_index=0;frame_index<300;++frame_index)ui_scroll.advance(1.0/60,2000);
    check(ui_scroll.shown==0 && ui_scroll.velocity==0 && !ui_scroll.active(),"released overscroll returns to the boundary and stops");
    check(ease_back(0)==0 && ease_back(1)==1 && ease_back(.7)>1,"opening easing has exact endpoints and a subtle overshoot");
    printf("{\"status\":\"advisor_briefing_fixture_passed\",\"checks\":%u,\"live_game\":false}\n",checks);return 0;
}
