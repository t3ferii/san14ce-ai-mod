#define TACTICS_FIXTURE
#define DIPLOMACY_INTEL_FIXTURE
#include "../src/native_adapter.cpp"
#include <map>
#include <string>
#include <fstream>
using namespace diplomacy_intel;
static unsigned checks,phase_calls,resolves,events,paints,shows,ai_calls;
static unsigned cleanups;static bool actors_live;
static bool simulate_ai;static int simulated_spent;
static bool footer_visible,portrait_visible[2],late_ui_exit;static int hidden_mode=1;static unsigned footer_reopens,portrait_reopens;
static unsigned (__fastcall *tail_gate)(Obj);
static bool last_player=true;
static int command_kind=8,last_resolve=-1,slider_max=-1,slider_total=-1,slider_value=-1,last_lookup=-1;
static std::map<int,std::wstring> labels;
static unsigned char state_mem[0x600],view_mem[0x220],window_mem[0x250],cmd_mem[0x100],
    force_mem[0x100],target_mem[0x100],person_mem[0x200],slider_mem[0x200],cursor_mem[0x200],actors_mem[0x240],portrait3_mem[0x200],portrait4_mem[0x200],button_mem[0x200],child_mem[0x100];
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
static int rate_reduced_seen;
static int __fastcall mock_rate(Obj,Obj,Obj,int,int base,int amount,int defending,int reduced) {rate_reduced_seen=reduced;return base+(defending?-amount:amount);}   // local10 G4: 8-argument form
static int __fastcall mock_ai(int phase,Obj cmd,int,int *value) {check(phase==11 && cmd==cmd_mem,"AI phase and command forwarded");++ai_calls;check(*value>=0 && *value<=100,"AI receives bounded probability");*value+=25;return 100;}
static int forced_roll,rng_calls;
#ifndef NO_RUNTIME_LOG
static bool probe_resolver_thread;
#endif
static int __fastcall mock_rng(int bound,int mode,int force,int person,int target,int threshold){check(bound==100 && mode==1 && force==10 && person==20 && target==30 && threshold>=0 && threshold<=100,"all six original RNG arguments unchanged");++rng_calls;return forced_roll;}
static int __fastcall mock_resolve(Obj f,Obj p,Obj t,int k,Obj o,int chance,int special) {
    check(f==force_mem && p==person_mem && t==target_mem && k==command_kind && o==cmd_mem && special==0,"final native arguments preserved");
    check(chance>=0 && chance<=100,"only actual probability reaches final resolver");++resolves;last_resolve=chance;
#ifndef NO_RUNTIME_LOG
    if(probe_resolver_thread){check(trace_owner(),"resolver continuation keeps diagnostic ownership on its original game thread");return native_roll(100,1,10,20,30,chance)<chance;}
#endif
    return chance>=50;
}
#ifndef NO_RUNTIME_LOG
static DWORD WINAPI probe_thread(void *){
    check(!trace_owner(),"unrelated worker thread is not traced outside the original call scope");
    {TraceScope scope(state_mem);check(trace_owner(),"native frame scope includes continuation thread");}
    check(!trace_owner(),"native frame scope is restored on return");
    int result=native_resolve(force_mem,person_mem,target_mem,command_kind,cmd_mem,33,0);
    check(result==1 && !trace_owner(),"scoped resolver records the original result and leaves no diagnostic ownership behind");return 0;
}
#endif
static void __fastcall mock_event(Obj v,int value) {check(v==view_mem && value==0x7FFFFFFD,"close event arguments preserved");++events;}
static void __fastcall mock_visibility(Obj,int) {}
static void __fastcall mock_generic_footer(Obj p){if(number(p,0x180)==0 && number(p,0x188)!=1 && number(p,0x188)!=2)footer_visible=true;}
static void __fastcall mock_resume_footer(Obj p){check(p==cursor_mem,"native phase-seven footer resume target");if(number(p,0x180)==0){footer_visible=true;++footer_reopens;number(p,0x178)=1;number(p,0x188)=0;number(p,0x80)=2;}}
static void __fastcall mock_resume_portrait(Obj p,int slot){check(p==actors_mem && (slot==3 || slot==4) && actors_live,"only live negotiation portrait slots restored");number(p,0x1DC+slot*4)=0;portrait_visible[slot-3]=true;++portrait_reopens;}
static void hide_ui(){footer_visible=false;number(cursor_mem,0x188)=hidden_mode;for(int slot:{3,4}){number(actors_mem,0x1DC+slot*4)=1;portrait_visible[slot-3]=false;}}
static void __fastcall mock_native_frame(Obj s,Obj,Obj,Obj){check(s==state_mem,"original diplomacy frame forwarded");if(late_ui_exit){hide_ui();late_ui_exit=false;}}
static void __fastcall mock_cleanup(Obj) {++cleanups;actors_live=false;footer_visible=false;portrait_visible[0]=portrait_visible[1]=false;}
static void __fastcall mock_phase(Obj s) {
    ++phase_calls;hide_ui();
    number(button_mem,0x40)&=~2;number(child_mem,0x40)&=~2;
    if(simulate_ai){int raw=number(s,0x594);simulated_spent+=ai_response(11,cmd_mem,0,&raw);number(s,0x594)=raw;}
    const int result=resolve(force_mem,person_mem,target_mem,command_kind,cmd_mem,number(s,0x594),0);
    number(s,0x4F8)=result;number(s,0x4D8)=result?12:13;
    // Actual generated machine gate, followed by the native UI teardown model.
    if(tail_gate(s)==1){event(view_mem,0x7FFFFFFD);mock_cleanup(s);}
}
static void *fixture_function(unsigned rva) {
    switch(rva) {
    case 0x8133D0:return reinterpret_cast<void *>(&mock_enter);
    case 0x812D70:return reinterpret_cast<void *>(&mock_leave);
    case 0x235150:return reinterpret_cast<void *>(&mock_player);
    case 0x231770:return reinterpret_cast<void *>(&mock_stat);
    case 0x324BC0:return reinterpret_cast<void *>(&mock_rate);
    case 0x806DA0:return reinterpret_cast<void *>(&mock_ai);
    case 0x3FDAF0:return reinterpret_cast<void *>(&mock_rng);
    case 0x326ED0:return reinterpret_cast<void *>(&mock_resolve);
    case 0x818600:return reinterpret_cast<void *>(&mock_phase);
    case 0x807710:return reinterpret_cast<void *>(&mock_event);
    case 0x826F70:case 0x843680:return reinterpret_cast<void *>(&mock_visibility);
    case 0x856500:return reinterpret_cast<void *>(&mock_generic_footer);
    case 0x858910:return reinterpret_cast<void *>(&mock_resume_footer);
    case 0x83E4A0:return reinterpret_cast<void *>(&mock_resume_portrait);
    case 0x813940:return reinterpret_cast<void *>(&mock_native_frame);
    case 0x809060:return reinterpret_cast<void *>(&mock_cleanup);
    case 0x9332B0:return reinterpret_cast<void *>(&mock_text);
    case 0x931D40:case 0x931DA0:return reinterpret_cast<void *>(&mock_control);
    case 0x92F390:return reinterpret_cast<void *>(&mock_lookup);
    case 0x85D490:return reinterpret_cast<void *>(&mock_max);
    case 0x85D4D0:return reinterpret_cast<void *>(&mock_total);
    case 0x85F2B0:return reinterpret_cast<void *>(&mock_value);
    case 0x82F030:case 0x82EB90:case 0x82F490:case 0x82F8F0:return reinterpret_cast<void *>(&mock_paint);
    case 0x82FD50:case 0x8300B0:case 0x830410:return reinterpret_cast<void *>(&mock_window_rate);
    default:return nullptr;
    }
}
static void setup() {
    actors_live=true;cleanups=0;simulate_ai=false;simulated_spent=0;footer_visible=true;portrait_visible[0]=portrait_visible[1]=true;late_ui_exit=false;footer_reopens=portrait_reopens=0;hidden_mode=1;memset(cursor_mem,0,sizeof(cursor_mem));memset(actors_mem,0,sizeof(actors_mem));
    pending={};final_scope=nullptr;mod_features.diplomacy_overinvest=true;mod_features.diplomacy_counterturn=true;
    memset(state_mem,0,sizeof(state_mem));memset(window_mem,0,sizeof(window_mem));
    pending.state=state_mem;pending.thread=GetCurrentThreadId();last_player=true;command_kind=8;point(state_mem,0x478,actors_mem);point(actors_mem,0x190,portrait3_mem);point(actors_mem,0x198,portrait4_mem);
    point(state_mem,0x4C0,view_mem);point(state_mem,0x4B0,cmd_mem);point(view_mem,0x188,window_mem);
    point(window_mem,0x180,force_mem);point(window_mem,0x190,target_mem);point(window_mem,0x1D8,cmd_mem);
    point(view_mem,0,view_vt);point(window_mem,0,window_vt);point(cmd_mem,0,command_vt);point(slider_mem,0,slider_vt);
    command_vt[0x98/8]=reinterpret_cast<void *>(&mock_kind);
    view_vt[0xC8/8]=window_vt[0xC8/8]=slider_vt[0xD0/8]=reinterpret_cast<void *>(&mock_show);
    window_vt[0x1C8/8]=window_vt[0x1D0/8]=reinterpret_cast<void *>(&mock_window);
    window_vt[0x1D8/8]=reinterpret_cast<void *>(&mock_window_rate);
    point(view_mem,0x198,cursor_mem);point(window_mem,0x1F8,button_mem);point(button_mem,0x180,child_mem);
    number(state_mem,0x4D8)=8;number(window_mem,0x20C)=10000;
    point(image,0x2780F60,image+0x2000);point(image+0x2000,0x158+8,person_mem);
    *reinterpret_cast<unsigned short *>(target_mem+0x52)=1;
    number(image,0x1A4B830)=80;number(image,0x1A4B83C)=100;
    for(unsigned a:{0x1A4B534u,0x1A4B79Cu,0x1A4B59Cu,0x1A4B700u}) {
        number(image,a)=100;number(image,a+12)=100;number(image,a+24)=1;number(image,a+44)=100;
    }
    for(unsigned a:{0x27D55B8u,0x27D55D0u,0x27D55BCu,0x27D55C4u})number(image,a)=0;
}
int main(int argc,char **argv) {
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x2A0B000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));check(image!=nullptr,"synthetic image");
    Obj gate=static_cast<Obj>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));check(gate!=nullptr,"tail gate memory");
    // Save RSI, pass the native state in RSI, then jump into the real gate.
    gate[0]=0x56;gate[1]=0x48;gate[2]=0x89;gate[3]=0xCE;absolute_jump(gate+4,gate+64);
    const unsigned char normal[]={0x89,0xD0,0x5E,0xC3},skip[]={0xB8,2,0,0,0,0x5E,0xC3};
    memcpy(gate+128,normal,sizeof(normal));memcpy(gate+160,skip,sizeof(skip));
    build_final_tail_gate(gate+64,gate+128,gate+160);
    DWORD prior;check(VirtualProtect(gate,4096,PAGE_EXECUTE_READ,&prior)!=0 && FlushInstructionCache(GetCurrentProcess(),gate,4096)!=0,"executable gate protected/flushed");
    tail_gate=reinterpret_cast<unsigned(__fastcall *)(Obj)>(gate);
    setup();check(tail_gate(state_mem)==1,"normal gate restores displaced MOV EDX,1");
    pending.round.deferred=true;check(tail_gate(state_mem)==2,"only the deferred current state skips UI teardown");
    check(tail_gate(window_mem)==1,"another state cannot skip native teardown");pending.round.deferred=false;
    Coefficients c{100,100,100,1,100,0};
    check(score(50,150,true,c)==-50,"150 points produce a 50-point defense reserve");
    check(score(50,150,true,c,70.0)==-20 && score(50,150,true,c,0.0)==-50,"324BC0 8th argument scales the amount term by 70/100 before the coefficient ratio");
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
    refresh<GA_82F490>(window_mem,0);
    check(slider_max==10000 && slider_total==10000 && slider_value==10000 && last_lookup==0x8B,"prisoner slider uses correct IDs and available balance");
    check(labels[0x7A]==L"10000" && labels[0x80]==L"/10000","prisoner selected and remaining labels");
    refresh<GA_82F030>(window_mem,0);check(last_lookup==0x4D,"goodwill slider IDs");
    refresh<GA_82EB90>(window_mem,0);check(last_lookup==0x5B,"alliance slider IDs");
    refresh<GA_82F8F0>(window_mem,0);check(last_lookup==0x5B,"attack request slider IDs");
    redraw<GA_8300B0>(window_mem,-1);check(labels[0x56]==L"180%","prisoner reserve is visible");
    redraw<GA_82FD50>(window_mem,-1);check(labels[0x75]==L"180%","goodwill reserve is visible");
    redraw<GA_830410>(window_mem,-1);check(labels[0x83]==L"180%","alliance/request reserve is visible");
    check(rate(force_mem,person_mem,target_mem,8,-50,100,1,0)==-150,"native rate wrapper preserves reserve");
    {   // local10 G4: the 8-argument form (synthetic global holding 70)
        number(image,0x3000)=70;rate_reduce=0x3000;rate_reduced_seen=-1;
        check(rate(force_mem,person_mem,target_mem,8,-50,100,1,1)==-120,"reduced investment term (70 percent) in the modified rate");
        check(rate(force_mem,person_mem,target_mem,8,-50,100,1,0)==-150,"unreduced call keeps the full term");
        mod_features.diplomacy_overinvest=false;rate_reduced_seen=-1;
        check(rate(force_mem,person_mem,target_mem,8,50,10,1,1)==40 && rate_reduced_seen==1,"unmodified rate forwards the 8th argument to the original");
        mod_features.diplomacy_overinvest=true;number(image,0x3000)=0;
        check(rate(force_mem,person_mem,target_mem,8,-50,100,1,1)==-150 && rate_reduced_seen==1,"non-positive reduction falls back to the native call");
    }
    const unsigned before_paint=paints;mod_features.diplomacy_overinvest=false;refresh<GA_82F490>(window_mem,0);check(paints==before_paint+1,"disabled investment feature uses native painter");
    check(rate(force_mem,person_mem,target_mem,8,50,10,1,0)==40,"disabled rate forwards to original");
    setup();last_player=false;check(!incoming(window_mem),"player-initiated negotiation excluded");
    setup();command_kind=7;check(!incoming(window_mem),"surrender excluded");
    setup();number(window_mem,0x220)=1;check(!over(window_mem),"fixed result excludes overinvestment");
    setup();final_scope=state_mem;int raw=-80;check(ai_response(11,cmd_mem,0,&raw)==100 && raw==-55,"actual AI wrapper preserves excess after native decision");final_scope=nullptr;
    setup();phase_calls=resolves=events=0;number(state_mem,0x594)=60;
    final_phase(state_mem);
    check(phase_calls==1 && resolves==0 && events==0,"first final AI phase defers both RNG and close event");
    check(pending.round.extra && number(state_mem,0x4D8)==8 && number(window_mem,0x224)==0,"extra player input restored");
    check(actors_live && !cleanups && (number(button_mem,0x40)&2) && (number(child_mem,0x40)&2),"extra turn preserves both negotiators and native decision-button visibility");
    check(number(window_mem,0x210)==0 && number(window_mem,0x214)==60,"new investment starts at zero with latest probability");
    number(state_mem,0x594)=-20;number(state_mem,0x4D8)=11;final_phase(state_mem);
    check(phase_calls==1 && resolves==1 && events==1 && last_resolve==0,"extra reply has one final RNG and no further AI move");
    check(number(state_mem,0x4D8)==13 && pending.round.done,"final outcome awaits original failure application");
    check(!actors_live && cleanups==1,"actors close once after the player's final decision");
    // User 2026-11-03: actual footer modes 1/2 and late portrait exits were absent from the old mock.
    for(int k:{4,5,8,9}) for(int mode:{1,2}) {
        setup();command_kind=k;hidden_mode=mode;hide_ui();mock_generic_footer(cursor_mem);
        check(!footer_visible,"regression: generic footer open skips both native hidden modes");
        phase_calls=resolves=events=ai_calls=0;simulate_ai=true;number(state_mem,0x594)=-80;
        final_phase(state_mem);
        check(footer_visible && portrait_visible[0] && portrait_visible[1] && actors_live,"extra turn restores both round buttons and both negotiators for every incoming kind");
        unsigned opens=portrait_reopens,footer_opens=footer_reopens;late_ui_exit=true;frame(state_mem,nullptr,nullptr,nullptr);
        check(footer_visible && portrait_visible[0] && portrait_visible[1] && portrait_reopens==opens+2 && footer_reopens==footer_opens+1,"late message completion cannot remove extra-turn UI");
        opens=portrait_reopens;footer_opens=footer_reopens;frame(state_mem,nullptr,nullptr,nullptr);
        check(portrait_reopens==opens && footer_reopens==footer_opens,"steady extra input does not replay UI entrance animations");
        number(state_mem,0x4D8)=11;final_phase(state_mem);opens=portrait_reopens;footer_opens=footer_reopens;
        frame(state_mem,nullptr,nullptr,nullptr);
        check(!actors_live && !footer_visible && !portrait_visible[0] && !portrait_visible[1] && portrait_reopens==opens && footer_reopens==footer_opens && cleanups==1 && resolves==1,"final choice exits once and frame guard never resurrects completed UI");
    }
    for(int k:{4,5,8,9}) {
        setup();command_kind=k;simulate_ai=true;number(state_mem,0x594)=-80;
        phase_calls=resolves=events=ai_calls=0;
        final_phase(state_mem);
        check(ai_calls==1 && simulated_spent==100 && number(state_mem,0x594)==-55,"AI spends once and absorbs only part of the 180-percent defense reserve");
        check(pending.round.extra && actors_live && (number(button_mem,0x40)&2) && number(window_mem,0x214)==-55,"all four incoming negotiation kinds retain actors, decision button and excess on the extra turn");
        number(state_mem,0x594)=-155;number(state_mem,0x4D8)=11;final_phase(state_mem);
        check(phase_calls==1 && ai_calls==1 && simulated_spent==100 && resolves==1 && last_resolve==0,"extra overinvestment resolves once with bounded probability and no repeated AI spend");
        check(cleanups==1 && events==1 && !actors_live && pending.round.done,"each negotiation completes its UI exactly once after the player's decision");
    }
#ifndef NO_RUNTIME_LOG
    // User 2026-11-03: a 67-percent defense still permits the incoming side's 33-percent success.
    setup();rng_calls=0;
    for(int value:{0,32,33,99}){forced_roll=value;int result=native_roll(100,1,10,20,30,33);check(result==value,"diagnostic wrapper forwards exactly one original draw without rerolling or changing it");check((result<33)==(value<=32),"original strict random threshold preserves both possible outcomes at 33 percent");}
    check(rng_calls==4,"RNG wrapper consumes no extra randomness");
    probe_resolver_thread=true;forced_roll=21;int before=rng_calls;HANDLE worker=CreateThread(nullptr,0,&probe_thread,nullptr,0,nullptr);check(worker!=nullptr,"isolated continuation-thread regression starts");check(WaitForSingleObject(worker,10000)==WAIT_OBJECT_0,"continuation-thread regression completes");CloseHandle(worker);probe_resolver_thread=false;check(rng_calls==before+1,"cross-thread diagnostics still consume exactly one original RNG draw");
#endif
    setup();mod_features.diplomacy_counterturn=false;phase_calls=resolves=events=0;number(state_mem,0x594)=60;
    final_phase(state_mem);check(phase_calls==1 && resolves==1 && events==1 && !pending.round.extra,"counterturn off preserves native final stage");
    check(!actors_live && cleanups==1,"disabled counterturn retains ordinary native UI teardown");
    setup();number(window_mem,0x220)=1;phase_calls=resolves=events=0;final_phase(state_mem);
    check(resolves==1 && !pending.round.extra,"fixed diplomacy result gets no extra turn");
    setup();pending.round.extra=true;leave(state_mem,force_mem,target_mem);check(pending.state==nullptr && !pending.round.extra,"leave discards session state before original teardown");
    check(argc==2,"retained original text argument");
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);check(static_cast<bool>(input),"retained text opens");
    auto size=input.tellg();check(size>0 && size<0x1342000,"retained text size");input.seekg(0);input.read(reinterpret_cast<char *>(image+0x1000),size);check(static_cast<bool>(input),"retained native text loaded");
    for(const auto &slot:slots)point(image,slot.rva,image+slot.original);
    check(guards_match(),"real native functions, callers, tail and slots match before installation");
    {   // 324BC0 is the 8-argument form: cmp dword [rsp+0x68],0 at +0xA9, movd xmm0,[rip+disp] at +0xC0
        const unsigned r=GA(GA_324BC0),g=rate_reduce_rva();
        check(g==0x1A4B884,"8-argument form decodes the percentage global (its value 70 is read from the data section by tests/verify_diplomacy_intel.py; the fixture image holds .text only)");
        image[r+0xC1]^=1;check(rate_reduce_rva()==0,"changed movd opcode rejects the 8-argument form");image[r+0xC1]^=1;
        check(rate_reduce_rva()==g && guards_match(),"form restored");
    }
    image[final_tail]^=1;check(!diplomacy_intel::install() && !final_tail_link.published,"foreign tail prevents every hook publication");image[final_tail]^=1;
    check(diplomacy_intel::install() && final_tail_link.published && image[final_tail]==0xE9,"actual native tail gate and existing hooks publish together");
    Obj installed_relay=image+sites[0].rva+5+links[0].redirected;
    for(unsigned i=0;i<16;++i)if(i<final_tail-final_tail_link.block || i>=final_tail-final_tail_link.block+5)
        check(final_tail_link.original[i]==final_tail_link.modified[i],"tail publication preserves adjacent native instructions");
    for(unsigned i=0;i<sizeof(slots)/sizeof(slots[0]);++i)restore_pointer(slots[i].rva,slots[i].original,replacements[i]);
    for(unsigned i=sizeof(sites)/sizeof(sites[0]);i;)call_restore(links[--i]);restore_link(final_tail_link);
    check(guards_match() && !memcmp(image+final_tail,final_tail_bytes,sizeof(final_tail_bytes)),"restoring hooks restores full native fingerprints and UI tail");
    VirtualFree(installed_relay,0,MEM_RELEASE);
    VirtualFree(gate,0,MEM_RELEASE);VirtualFree(image,0,MEM_RELEASE);
    printf("{\"status\":\"diplomacy_intel_fixture_verified\",\"checks\":%u,\"live_game\":false}\n",checks);
}
