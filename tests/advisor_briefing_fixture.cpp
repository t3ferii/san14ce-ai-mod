#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../src/advisor_briefing.h"
namespace briefing_overlay {
using advisor_briefing_core::Rect;
struct Model {std::wstring adviser=L"군사",title=L"열흘 보고",body;unsigned unread=0,page=0,pages=0,generation=0;bool collecting=false,open=false,detail=false;};
static bool gpu_ready=true;static unsigned action=0,ticket=0;static Model displayed;
static bool available(){return gpu_ready;}
static unsigned take(unsigned &g){g=ticket;unsigned a=action;action=0;return a;}
static void publish(const Model &m,bool){displayed=m;}
static void forget(){action=0;}
static bool captured(int){return true;}
static void request_portrait(Rect,int){}
}
namespace unified_overlay {static void install(){}}
#define TACTICS_FIXTURE
#define ADVISOR_BRIEFING_FIXTURE
#include "../src/native_adapter.cpp"
#include <array>
using namespace advisor_briefing;
static unsigned checks,pops,original_notices,original_enters,original_leaves,report_enters,report_ticks,camera_restores,report_completions,text_calls;
static int calendar_day=1000;static bool income_numbers=true;
static bool second_district=false;
static unsigned tutorial_flag=0;
static bool dialog_ready=true,native_close_pending=false;
static bool synchronous_close=false;
static unsigned acknowledgements=0;
static std::array<unsigned char,0x90000> root;
static std::array<unsigned char,0x2400> context;
static std::array<unsigned char,0x700> state_a,state_b;
static std::array<unsigned char,0x200> force_mem,district_mem,dialog_mem,background_mem,income_child,world_report;
static std::array<unsigned char,0x3A8> income_mem;
static std::array<unsigned char,0x200> second_district_mem;
static std::array<unsigned char,0x200> animation_mem;
static std::array<unsigned char,0x40> manager_mem;
static Obj manager_stack[4];
static std::array<unsigned char,0x162F0> action_mem;
static void *force_vt[32];
static void check(bool b,const char *why){++checks;if(!b){fprintf(stderr,"%s\n",why);exit(1);}}
static void point(Obj p,unsigned offset,void *value){*reinterpret_cast<void **>(p+offset)=value;}
static void number(Obj p,unsigned offset,unsigned value){*reinterpret_cast<unsigned *>(p+offset)=value;}
static Obj __fastcall mock_force(Obj){return force_mem.data();}
static Obj __fastcall mock_district(Obj){return district_mem.data();}
static unsigned __fastcall mock_id(Obj){return 1;}
static int __fastcall mock_valid(Obj p){return p!=nullptr;}
static Obj __fastcall mock_tutorial(){return reinterpret_cast<Obj>(&tutorial_flag);}
static Obj __fastcall mock_manager(){return manager_mem.data();}
static int __fastcall mock_day(Obj){return calendar_day;}
static bool __fastcall mock_player_state(Obj,const char *){return true;}
static void __fastcall mock_pop(Obj){++pops;}
static void mock_native_close_event();
static void __fastcall mock_acknowledge(Obj dialog,unsigned result) {
    ++acknowledgements;if(!dialog_ready)return;
    number(dialog,0x178,result);native_close_pending=true;
    if(synchronous_close){mock_native_close_event();number(dialog,0x178,0);point(state_a.data(),0x470,nullptr);}
}
static void mock_native_close_event() {
    check(native_close_pending,"only native close completion can run the bound callback");native_close_pending=false;mock_pop(mock_manager());
}
static void __fastcall mock_original_notice(Obj){++original_notices;}
static unsigned long long __fastcall mock_user_enter(Obj,Obj){++original_enters;return 31;}
static void __fastcall mock_user_leave(Obj,Obj){++original_leaves;}
static void __fastcall mock_move(Obj p,int x,int y){number(p,0x20,static_cast<unsigned>(x));number(p,0x24,static_cast<unsigned>(y));}
static unsigned long long __fastcall mock_simple_enter(Obj p){point(p,0x470,dialog_mem.data());point(dialog_mem.data(),0x188,animation_mem.data());number(dialog_mem.data(),0x178,0xFFFFFFFF);++original_enters;return 1;}
static unsigned long long __fastcall mock_report_enter(Obj p) {
    ++report_enters;point(p,0x480,background_mem.data());point(p,0x488,action_mem.data());
    number(p,0x470,0);number(p,0x474,0);world_report[0x82]|=4;
    number(action_mem.data(),0x16280,1);
    wcscpy_s(reinterpret_cast<wchar_t *>(action_mem.data()+0x1B00+0x8A),0x55,L"조운이 금 500을 발견했습니다.");
    return 7;
}
static void __fastcall mock_original_text(Obj,int,const wchar_t *,unsigned long long){++text_calls;}
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
static const wchar_t *__fastcall mock_name(Obj){return L"제갈량";}
static const wchar_t *__fastcall mock_label(unsigned id){return id==916?L"제8군단":L"제1군단";}
static void __fastcall mock_income_values(Obj,Obj district) {
    if(income_numbers){set_text(income_child.data()+0x130,10,L"2000",1);set_text(income_child.data()+0x130,10,district==second_district_mem.data()?L"750":L"2500",1);}
}
static void *fixture_function(unsigned rva) {
    switch(rva) {
    case 0x32E910:return reinterpret_cast<void *>(&mock_force);
    case 0x32E8D0:return reinterpret_cast<void *>(&mock_district);
    case 0x32F500:case 0x233750:return reinterpret_cast<void *>(&mock_valid);
    case 0x22DBC0:return reinterpret_cast<void *>(&mock_id);
    case 0x56EE40:return reinterpret_cast<void *>(&mock_tutorial);
    case 0x110E0:return reinterpret_cast<void *>(&mock_manager);
    case 0x12540:return reinterpret_cast<void *>(&mock_pop);
    case 0x861480:return reinterpret_cast<void *>(&mock_acknowledge);
    case 0x32BA50:return reinterpret_cast<void *>(&mock_day);
    case 0x56FE50:return reinterpret_cast<void *>(&mock_player_state);
    case 0x44A0F0:return reinterpret_cast<void *>(&mock_user_enter);
    case 0x448E40:case 0x448A70:case 0x67B1E0:return reinterpret_cast<void *>(&mock_user_leave);
    case 0x862E40:case 0x67B7C0:return reinterpret_cast<void *>(&mock_original_notice);
    case 0x1F15F0:return reinterpret_cast<void *>(&mock_simple_enter);
    case 0x874650:return reinterpret_cast<void *>(&mock_move);
    case 0x67B420:return reinterpret_cast<void *>(&mock_report_enter);
    case 0x67C990:return reinterpret_cast<void *>(&mock_phase);
    case 0x67D1F0:return reinterpret_cast<void *>(&mock_income_values);
    case 0x30CF80:return reinterpret_cast<void *>(&mock_label);
    case 0x67B600:return reinterpret_cast<void *>(&mock_report_tick);
    case 0x92CB10:return reinterpret_cast<void *>(&mock_position);
    case 0x22EEC0:return reinterpret_cast<void *>(&mock_name);
    default:return nullptr;
    }
}
static void cycle() {
    mod_features.advisor_briefing=true;enabled=1;briefing_overlay::gpu_ready=true;inbox.fallback=false;
    manager_stack[0]=state_a.data();point(manager_mem.data(),0x20,manager_stack);*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=1;
    user_leave(state_a.data(),nullptr);check(inbox.collecting && inbox.events.empty(),"accepted progress starts one new empty cycle");
}
int main(int argc,char **argv) {
    check(argc==2,"independent captured text supplied");
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x29FF000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));check(image!=nullptr,"synthetic image allocated");
    FILE *f=nullptr;check(fopen_s(&f,argv[1],"rb")==0,"retained capture opened");
    check(fread(image+0x1000,1,kTextSize,f)==kTextSize,"retained capture loaded");fclose(f);
    for(const auto &s:slots)point(image,s.rva,image+s.original);
    check(guards_match(),"native original code and slots accepted");
    for(const auto &g:guards)for(unsigned offset:{0u,g.size-1}){image[g.rva+offset]^=1;check(!guards_match(),"changed dependency rejects briefing");image[g.rva+offset]^=1;}
    for(const auto &s:slots){image[s.rva]^=1;check(!guards_match(),"foreign slot rejects briefing");image[s.rva]^=1;}
    point(image,0x277DF18,root.data());point(root.data(),0x871E8,context.data());point(root.data(),0xE0D0+8,district_mem.data());
    // User 2026-10-26: normal live game has 15/16 orders at context+454.
    // That value must not disable notification collection or report skipping.
    number(context.data(),0x454,15);
    point(force_mem.data(),0,force_vt);force_vt[0x60/8]=reinterpret_cast<void *>(&mock_id);original_text=&mock_original_text;
    // User 2026-10-26: live face 484x512, world matrix 0.3125; the raw-size
    // <512 gate and raw anchor produced no avatar at 1280x720.
    float face_matrix[16]{};face_matrix[0]=face_matrix[5]=0.3125f;face_matrix[10]=face_matrix[15]=1;
    face_matrix[12]=-864.375f;face_matrix[13]=-424;Rect face_rect;
    check(portrait_bounds(484,512,0x11,face_matrix,face_rect),"real 512-high adviser texture is accepted");
    check(face_rect.x==20 && face_rect.y==884 && face_rect.w==151.25f && face_rect.h==160,"capture uses the scaled world rectangle instead of unscaled anchor and texture dimensions");
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
    cycle();point(state_a.data(),0x4B0,const_cast<wchar_t *>(L"홍수가 발생했습니다."));number(state_a.data(),0x4B8,1);calendar_day+=9;
    unsigned before=pops;narration(state_a.data());check(pops==before+1 && inbox.events.size()==1 && inbox.events[0].day==10,"passive narration is copied before its native pop");
    check(briefing_overlay::displayed.collecting && briefing_overlay::displayed.unread==1,"badge updates during progress even when command HUD has no frames");
    briefing_overlay::gpu_ready=false;before=pops;unsigned original=original_notices;narration(state_a.data());check(pops==before && original_notices==original+1,"renderer failure preserves the original message");
    briefing_overlay::gpu_ready=true;tutorial_flag=1;narration(state_a.data());check(original_notices==original+2,"actual tutorial messages remain native with normal order count");tutorial_flag=0;
    // User 2026-10-26: refusing a money demand crashed local4. Choice class
    // separation alone did not protect the subsequent shared message classes.
    check(!passive_notice(L"거절하셨으니 돌아가겠소.") && !passive_notice(L"등용 요구를 거절합니다.") && !passive_notice(L"돌아가겠소."),"refusal and unknown dialogue remain native, even with a category word");
    check(interactive_state("CStrategyDiplomacyState",22) && interactive_state("CTreatPrisonerState",18) && !interactive_state("CProgressState",14),"diplomacy and prisoner interactions are protected independently of message wording");
    cycle();strcpy_s(reinterpret_cast<char *>(state_b.data()+0x70),128,"CTreatDiplomacyState");
    manager_stack[0]=state_b.data();manager_stack[1]=state_a.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=2;
    before=pops;original=original_notices;narration(state_a.data());
    check(interaction() && pops==before && original_notices==original+1 && inbox.events.empty(),"a diplomacy parent below the notification protects its entire continuation");
    report_enter(state_a.data());report_frame(state_a.data());report_wait(state_a.data());
    check(pops==before && !report_deferred && inbox.events.empty(),"report and report-wait bypass cannot pop a diplomacy continuation");report_leave(state_a.data(),nullptr);
    cycle();struct Node {Node *next,*previous;const wchar_t *value;};Node head{},a{},b{};head.next=&a;head.previous=&b;a={&b,&head,L"천시가 변했습니다."};b={&head,&a,L"통솔이 올랐습니다."};
    point(state_a.data(),0x478,&head);*reinterpret_cast<unsigned long long *>(state_a.data()+0x480)=2;
    before=pops;check(simple_enter(state_a.data())==1 && pops==before && inbox.events.size()==2,"simple dialog Enter queues all pages and does not reenter manager pop");
    check(dword(dialog_mem.data(),0x20)==10000,"captured native message is moved out of display");
    dialog_ready=false;number(animation_mem.data(),0x178,1);simple_frame(state_a.data());check(pops==before && dword(dialog_mem.data(),0x178)==0xFFFFFFFF && !simple_states.empty(),"opening animation cannot be bypassed by direct destruction");
    dialog_ready=true;number(animation_mem.data(),0x178,0);unsigned ack=acknowledgements;simple_frame(state_a.data());simple_frame(state_a.data());
    check(pops==before && native_close_pending && acknowledgements==ack+1,"native acknowledgement starts closing once and never pops from the mod frame");
    mock_native_close_event();check(pops==before+1,"original bound close event completes the simple message exactly once");
    check(dword(dialog_mem.data(),0x178)==0x7FFFFFFD,"automatic acknowledgement returns native final-page success instead of constructor FFFFFFFF");
    cycle();simple_enter(state_a.data());before=pops;synchronous_close=true;simple_frame(state_a.data());synchronous_close=false;
    check(pops==before+1 && simple_states.empty() && !pointer(state_a.data(),0x470),"synchronous native completion may dispose the dialog without a later mod dereference");
    cycle();manager_stack[0]=state_b.data();manager_stack[1]=state_a.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=2;
    before=pops;simple_enter(state_a.data());simple_frame(state_a.data());
    check(pops==before && simple_states.empty() && inbox.events.empty() && dword(dialog_mem.data(),0x178)==0xFFFFFFFF,"even a passive-looking simple dialog under diplomacy keeps native Enter and completion");
    cycle();simple_enter(state_a.data());before=pops;manager_stack[0]=state_b.data();simple_frame(state_a.data());
    check(pops==before && !simple_states.empty(),"a queued message cannot pop a different top state");
    manager_stack[0]=state_b.data();manager_stack[1]=state_a.data();*reinterpret_cast<unsigned long long *>(manager_mem.data()+0x10)=2;
    simple_frame(state_a.data());check(pops==before && simple_states.empty() && dword(dialog_mem.data(),0x20)==0 && dword(dialog_mem.data(),0x178)==0xFFFFFFFF,"new diplomacy interaction cancels auto-close and restores the native message unchanged");
    cycle();point(root.data(),0x79E90,world_report.data());world_report[0x12]=1;world_report[0x82]=2;world_report[0x86]=1;
    wcscpy_s(reinterpret_cast<wchar_t *>(world_report.data()+0x8A),0x55,L"홍수가 발생했습니다.");
    user_enter(state_a.data(),nullptr);check(!inbox.collecting && inbox.has_cycle,"native report follows player Enter in the real lifecycle");
    second_district=true;
    unsigned entered=report_enters;check(report_enter(state_a.data())==7 && report_enters==entered+1,"original report construction applies once and preserves result");
    check((world_report[0x82]&4) && inbox.events.size()==2 && report_deferred,"pre-consumption world log and generated report both retained");
    before=pops;report_frame(state_a.data());check(camera_restores==1 && report_completions==1 && pops==before+1,"native report finalization and camera restoration still execute");
    auto income=std::find_if(inbox.events.begin(),inbox.events.end(),[](const Event &e){return e.kind==Kind::Income;});
    check(income!=inbox.events.end() && has(income->text,L"2500") && !has(income->text,L"2000"),"income capture retains final values instead of stale intermediate text");
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
    game_leave(state_a.data(),nullptr);check(!inbox.world && inbox.events.empty(),"game teardown clears all world references");
    printf("{\"status\":\"advisor_briefing_fixture_passed\",\"checks\":%u,\"live_game\":false}\n",checks);return 0;
}
