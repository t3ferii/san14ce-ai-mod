#define TACTICS_FIXTURE
#define DIPLOMACY_INTEL_FIXTURE
#include "../src/native_adapter.cpp"
#include <map>
#include <string>
using namespace diplomacy_intel;
static unsigned checks,phase_calls,resolves,events,paints,shows,ai_calls;
static bool last_player=true;
static int command_kind=8,last_resolve=-1,slider_max=-1,slider_total=-1,slider_value=-1,last_lookup=-1;
static std::map<int,std::wstring> labels;
static unsigned char state_mem[0x600],view_mem[0x220],window_mem[0x250],cmd_mem[0x100],
    force_mem[0x100],target_mem[0x100],person_mem[0x200],slider_mem[0x200],cursor_mem[0x200],button_mem[0x200],child_mem[0x100];
static void *command_vt[32],*window_vt[64],*view_vt[64],*slider_vt[40];
static void check(bool b,const char *why) {++checks;if(!b){fprintf(stderr,"%s\n",why);exit(1);}}
static void point(Obj p,unsigned off,void *v) {*reinterpret_cast<void **>(p+off)=v;}
static int __fastcall mock_kind(Obj) {return command_kind;}
static int __fastcall mock_player(Obj p) {return p==target_mem?last_player:!last_player;}
static int __fastcall mock_stat(Obj p,int stat,int mask) {check(p==person_mem && stat==3 && mask==0xFFF,"defender advisor and native stat arguments");return 90;}
static unsigned __fastcall mock_enter(Obj p) {check(p==state_mem,"init forwards state");return 17;}
static void __fastcall mock_leave(Obj p,Obj a,Obj b) {check(p==state_mem && a==force_mem && b==target_mem,"leave forwards all three arguments");}
static void __fastcall mock_show(Obj,int) {++shows;}
static void __fastcall mock_window(Obj,int) {}
static void __fastcall mock_window_rate(Obj w,float) {number(w,0x218)=number(w,0x214);}
static void __fastcall mock_text(Obj p,int id,const wchar_t *s,int flag) {check(p==window_mem+0x130 && flag==1,"text uses native layout and final flag");labels[id]=s;}
static void __fastcall mock_control(Obj,int,int) {}
static Obj __fastcall mock_lookup(Obj p,int id) {check(p==window_mem+0x130,"slider lookup uses layout subobject");last_lookup=id;return slider_mem;}
static void __fastcall mock_max(Obj p,int n) {check(p==slider_mem,"slider maximum target");slider_max=n;}
static void __fastcall mock_total(Obj p,int n) {check(p==slider_mem,"slider total target");slider_total=n;}
static void __fastcall mock_value(Obj p,int n) {check(p==slider_mem,"slider selection target");slider_value=n;}
static void __fastcall mock_paint(Obj) {++paints;}
static int __fastcall mock_rate(Obj,Obj,Obj,int,int base,int amount,int defending) {return base+(defending?-amount:amount);}
static int __fastcall mock_ai(int phase,Obj cmd,int,int *value) {check(phase==11 && cmd==cmd_mem,"AI phase and command forwarded");++ai_calls;check(*value>=0 && *value<=100,"AI receives bounded probability");*value+=25;return 100;}
static int __fastcall mock_resolve(Obj f,Obj p,Obj t,int k,Obj o,int chance,int special) {
    check(f==force_mem && p==person_mem && t==target_mem && k==command_kind && o==cmd_mem && special==0,"final native arguments preserved");
    check(chance>=0 && chance<=100,"only actual probability reaches final resolver");++resolves;last_resolve=chance;return chance>=50;
}
static void __fastcall mock_event(Obj v,int value) {check(v==view_mem && value==0x7FFFFFFD,"close event arguments preserved");++events;}
static void __fastcall mock_visibility(Obj,int) {}
static void __fastcall mock_cursor(Obj) {}
static void __fastcall mock_cleanup(Obj) {}
static void __fastcall mock_phase(Obj s) {
    ++phase_calls;
    const int result=resolve(force_mem,person_mem,target_mem,command_kind,cmd_mem,number(s,0x594),0);
    number(s,0x4F8)=result;number(s,0x4D8)=result?12:13;
    event(view_mem,0x7FFFFFFD);
}
static void *fixture_function(unsigned rva) {
    switch(rva) {
    case 0x810CA0:return reinterpret_cast<void *>(&mock_enter);
    case 0x810640:return reinterpret_cast<void *>(&mock_leave);
    case 0x2337B0:return reinterpret_cast<void *>(&mock_player);
    case 0x22FCE0:return reinterpret_cast<void *>(&mock_stat);
    case 0x322B30:return reinterpret_cast<void *>(&mock_rate);
    case 0x804770:return reinterpret_cast<void *>(&mock_ai);
    case 0x324E20:return reinterpret_cast<void *>(&mock_resolve);
    case 0x815EA0:return reinterpret_cast<void *>(&mock_phase);
    case 0x805010:return reinterpret_cast<void *>(&mock_event);
    case 0x824880:case 0x841180:return reinterpret_cast<void *>(&mock_visibility);
    case 0x856500:return reinterpret_cast<void *>(&mock_cursor);
    case 0x806950:return reinterpret_cast<void *>(&mock_cleanup);
    case 0x930D20:return reinterpret_cast<void *>(&mock_text);
    case 0x92F7B0:case 0x92F810:return reinterpret_cast<void *>(&mock_control);
    case 0x92CEE0:return reinterpret_cast<void *>(&mock_lookup);
    case 0x85B160:return reinterpret_cast<void *>(&mock_max);
    case 0x85B1A0:return reinterpret_cast<void *>(&mock_total);
    case 0x85CF80:return reinterpret_cast<void *>(&mock_value);
    case 0x82C960:case 0x82C510:case 0x82CD60:case 0x82D170:return reinterpret_cast<void *>(&mock_paint);
    case 0x82D880:case 0x82DB90:case 0x82D570:return reinterpret_cast<void *>(&mock_window_rate);
    default:return nullptr;
    }
}
static void setup() {
    pending={};final_scope=nullptr;mod_features.diplomacy_overinvest=true;mod_features.diplomacy_counterturn=true;
    memset(state_mem,0,sizeof(state_mem));memset(window_mem,0,sizeof(window_mem));
    pending.state=state_mem;last_player=true;command_kind=8;
    point(state_mem,0x4C0,view_mem);point(state_mem,0x4B0,cmd_mem);point(view_mem,0x188,window_mem);
    point(window_mem,0x180,force_mem);point(window_mem,0x190,target_mem);point(window_mem,0x1D8,cmd_mem);
    point(view_mem,0,view_vt);point(window_mem,0,window_vt);point(cmd_mem,0,command_vt);point(slider_mem,0,slider_vt);
    command_vt[0x98/8]=reinterpret_cast<void *>(&mock_kind);
    view_vt[0xC8/8]=window_vt[0xC8/8]=slider_vt[0xD0/8]=reinterpret_cast<void *>(&mock_show);
    window_vt[0x1C8/8]=window_vt[0x1D0/8]=reinterpret_cast<void *>(&mock_window);
    window_vt[0x1D8/8]=reinterpret_cast<void *>(&mock_window_rate);
    point(view_mem,0x198,cursor_mem);point(window_mem,0x1F8,button_mem);point(button_mem,0x180,child_mem);
    number(state_mem,0x4D8)=8;number(window_mem,0x20C)=10000;
    point(image,0x277DF18,image+0x2000);point(image+0x2000,0x158+8,person_mem);
    *reinterpret_cast<unsigned short *>(target_mem+0x52)=1;
    number(image,0x1A48844)=80;number(image,0x1A48850)=100;
    for(unsigned a:{0x1A48548u,0x1A487ACu,0x1A485B0u,0x1A4870Cu}) {
        number(image,a)=100;number(image,a+12)=100;number(image,a+24)=1;number(image,a+44)=100;
    }
    for(unsigned a:{0x27D2564u,0x27D257Cu,0x27D2568u,0x27D2574u})number(image,a)=0;
}
int main() {
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x29FF000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));check(image!=nullptr,"synthetic image");
    Coefficients c{100,100,100,1,100,0};
    check(score(50,150,true,c)==-50,"150 points produce a 50-point defense reserve");
    check(score(-50,100,true,c)==-150,"second user investment accumulates reserve");
    check(preserve_reserve(-80,25)==-55,"AI 25-point response consumes only part of reserve");
    check(probability(-55)==0 && probability(160)==100,"final probabilities bounded");
    check(score(50,INT32_MAX,true,c)<-1000000,"large available resource avoids native intermediate overflow");
    check(score(50,0,true,c)==50 && score(50,-1,true,c)==50,"zero and negative investment");
    check(score(50,100,true,{})==50,"invalid coefficients fail closed");
    for(int base=0;base<=100;++base)for(int amount=0;amount<1000;amount+=7) {
        const double native_value=(base-(static_cast<double>(amount)/(base/100.0+1)))*10000;
        const int expected=static_cast<int>(std::round(native_value))/10000;
        check(score(base,amount,true,c)==expected,"normal-range rounding matches assembly integer division");
    }
    for(bool a:{false,true})for(bool incoming_flag:{false,true})for(bool fixed:{false,true}) {
        Round r;check(r.defer(a,incoming_flag,fixed)==(a && incoming_flag && !fixed),"counterturn eligibility matrix");
        if(r.deferred) {check(r.reopen(),"extra round opens");check(!r.defer(true,true,false),"extra cannot defer again");check(!r.reopen(),"duplicate reopen rejected");r.finish();check(!r.defer(true,true,false),"completed round cannot restart");}
    }
    setup();check(enter(state_mem)==17 && pending.state==state_mem && !pending.round.extra,"fresh negotiation resets lifetime state");
    setup();number(state_mem,0x594)=-80;number(window_mem,0x214)=-80;number(window_mem,0x210)=INT32_MAX;
    refresh<0x82CD60>(window_mem);
    check(slider_max==10000 && slider_total==10000 && slider_value==10000 && last_lookup==0x8B,"prisoner slider uses correct IDs and available balance");
    check(labels[0x7A]==L"10000" && labels[0x80]==L"/10000","prisoner selected and remaining labels");
    refresh<0x82C960>(window_mem);check(last_lookup==0x4D,"goodwill slider IDs");
    refresh<0x82C510>(window_mem);check(last_lookup==0x5B,"alliance slider IDs");
    refresh<0x82D170>(window_mem);check(last_lookup==0x5B,"attack request slider IDs");
    redraw<0x82DB90>(window_mem,-1);check(labels[0x56]==L"180%","prisoner reserve is visible");
    redraw<0x82D880>(window_mem,-1);check(labels[0x75]==L"180%","goodwill reserve is visible");
    redraw<0x82D570>(window_mem,-1);check(labels[0x83]==L"180%","alliance/request reserve is visible");
    check(rate(force_mem,person_mem,target_mem,8,-50,100,1)==-150,"native rate wrapper preserves reserve");
    const unsigned before_paint=paints;mod_features.diplomacy_overinvest=false;refresh<0x82CD60>(window_mem);check(paints==before_paint+1,"disabled investment feature uses native painter");
    check(rate(force_mem,person_mem,target_mem,8,50,10,1)==40,"disabled rate forwards to original");
    setup();last_player=false;check(!incoming(window_mem),"player-initiated negotiation excluded");
    setup();command_kind=7;check(!incoming(window_mem),"surrender excluded");
    setup();number(window_mem,0x220)=1;check(!over(window_mem),"fixed result excludes overinvestment");
    setup();final_scope=state_mem;int raw=-80;check(ai_response(11,cmd_mem,0,&raw)==100 && raw==-55,"actual AI wrapper preserves excess after native decision");final_scope=nullptr;
    setup();phase_calls=resolves=events=0;number(state_mem,0x594)=60;
    final_phase(state_mem);
    check(phase_calls==1 && resolves==0 && events==0,"first final AI phase defers both RNG and close event");
    check(pending.round.extra && number(state_mem,0x4D8)==8 && number(window_mem,0x224)==0,"extra player input restored");
    check(number(window_mem,0x210)==0 && number(window_mem,0x214)==60,"new investment starts at zero with latest probability");
    number(state_mem,0x594)=-20;number(state_mem,0x4D8)=11;final_phase(state_mem);
    check(phase_calls==1 && resolves==1 && events==1 && last_resolve==0,"extra reply has one final RNG and no further AI move");
    check(number(state_mem,0x4D8)==13 && pending.round.done,"final outcome awaits original failure application");
    setup();mod_features.diplomacy_counterturn=false;phase_calls=resolves=events=0;number(state_mem,0x594)=60;
    final_phase(state_mem);check(phase_calls==1 && resolves==1 && events==1 && !pending.round.extra,"counterturn off preserves native final stage");
    setup();number(window_mem,0x220)=1;phase_calls=resolves=events=0;final_phase(state_mem);
    check(resolves==1 && !pending.round.extra,"fixed diplomacy result gets no extra turn");
    setup();pending.round.extra=true;leave(state_mem,force_mem,target_mem);check(pending.state==nullptr && !pending.round.extra,"leave discards session state before original teardown");
    VirtualFree(image,0,MEM_RELEASE);
    printf("{\"status\":\"diplomacy_intel_fixture_verified\",\"checks\":%u,\"live_game\":false}\n",checks);
}
