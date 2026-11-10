// CTitleState: native ninth menu row, original UI thread only. Existing IDs stay unchanged.
// Layout E7A28866: 37 -> 39 nodes, node37 clones menu row32, node38 clones its art33.
// ABI comes from retained 504670/4E81D0/50B800/4F9310 C+asm and running .text checks.
#include "game_addresses.h"
#include "native_ui_call_trace.inl"
namespace ccz_title {
const unsigned kVtable=GA(GA_13E7E98),kRoot=GA(GA_13E7D08),kPane=GA(GA_13E7F10),kButton=37,kCount=39;
static Obj current,pane,button,initializing;static bool requested,visibility_logged,initializing_button_added;
static unsigned logged_phase=~0u;
static constexpr int menu_ids[]={13,20,22,24,26,28,30,32,37};
static Obj intro_rows[9]={};static unsigned intro_visible[9]={};
using Initialize=unsigned long long(__fastcall *)(Obj);
using Frame=void(__fastcall *)(Obj,Obj,Obj);
using Leave=void(__fastcall *)(Obj,Obj);
using Attach=void(__fastcall *)(Obj,int,Obj,int);
// Layout manager: next of each registered hook (original unless another mod chained first).
static Initialize next_initialize;static Leave next_leave;static Frame next_frame;static Attach next_attach;
struct Guard {unsigned rva,size;unsigned long long hash;};
const Guard guards[]={
    {GA(GA_506ED0),0x4B,GH(0x6D4E7756D1B610FDull)},{GA(GA_50DEC0),0x8EC,GH(0x42C5F566CA400955ull)},{GA(GA_4FBB70),0x395,GH(0x7564701720EF57D5ull)},
    {GA(GA_4EAA10),64,GH(0x9A30B2A4A2B432BFull)},{GA(GA_930640),64,GH(0x52CE0FE803B4D1DEull)},
    {GA(GA_870470),64,GH(0x6122B6C28963243Bull)},{GA(GA_9309C0),64,GH(0x9E958B8BA0592979ull)},
    {GA(GA_8685C0),64,GH(0x252F19DCF7D4ED48ull)},{GA(GA_1A4FB0),64,GH(0xE43B2ECC7CA86245ull)},
    {GA(GA_875720),64,GH(0x969C21695CF7490Aull)},{GA(GA_AD44E0),64,GH(0xC00A304A0AB2CAD1ull)},
    {GA(GA_875800),64,GH(0xCB505E563FFF6C99ull)}};
#ifndef NO_RUNTIME_LOG
// Bounded construction/phase snapshots on the native caller thread. Raw float
// fields are logged as their uint32 bits; no new recorder or live memory writer.
struct TraceCall {
    unsigned rva,id;LARGE_INTEGER begin;ULONG64 cycles;
    TraceCall(unsigned r,unsigned i):rva(r),id(i),cycles(0){QueryThreadCycleTime(GetCurrentThread(),&cycles);QueryPerformanceCounter(&begin);}
    ~TraceCall(){
        LARGE_INTEGER end,freq;ULONG64 finish=0;QueryPerformanceCounter(&end);QueryThreadCycleTime(GetCurrentThread(),&finish);QueryPerformanceFrequency(&freq);
        log_event("ccz_title_call_us",rva,id,static_cast<unsigned>((end.QuadPart-begin.QuadPart)*1000000/freq.QuadPart));
        log_event("ccz_title_call_cycles",rva,id,static_cast<unsigned>(finish-cycles));
    }
};
static unsigned vtable_rva(Obj object){
    if(!object)return 0;uintptr_t value=reinterpret_cast<uintptr_t>(pointer(object,0)),base=reinterpret_cast<uintptr_t>(image);
    return value>=base && value-base<0x3000000?static_cast<unsigned>(value-base):~0u;
}
static void trace_object(unsigned stage,unsigned id,Obj object){
    unsigned key=(stage<<16)|id;
    log_event("ccz_title_object",key,static_cast<unsigned>(reinterpret_cast<uintptr_t>(object)),static_cast<unsigned>(reinterpret_cast<uintptr_t>(object)>>32));
    if(!object)return;
    log_event("ccz_title_flags",key,dword(object,0x40),vtable_rva(object));
    log_event("ccz_title_xy",key,dword(object,0x20),dword(object,0x24)); // 874600 position setter
    log_event("ccz_title_cache",key,dword(object,0x10),dword(object,0x30));
    Obj parent=pointer(object,8);
    for(unsigned depth=1;parent && depth<=4;++depth){
        log_event("ccz_title_parent",key,depth,dword(parent,0x40));
        log_event("ccz_title_parent_xy",key,dword(parent,0x20),dword(parent,0x24));
        if(pointer(parent,8)==parent)break;parent=pointer(parent,8);
    }
}
static void trace_binding(unsigned stage,Obj root,unsigned id,Obj object){
    trace_object(stage,id,object);
    Obj resource=root?pointer(root,0x98):nullptr;
    Obj model=resource?pointer(resource,0x10):nullptr;
    // Same read-only node lookup used inside 866230, on its original UI thread.
    Obj node=model?fn<Obj(__fastcall *)(Obj,int)>(GA(GA_AD44E0))(model,id):nullptr;
    unsigned key=(stage<<16)|id;
    log_event("ccz_title_binding",key,node?1:0,object?dword(object,0x80):0);
    if(node){
        log_event("ccz_title_node",key,dword(node,0x104),dword(node,0x10C));
        log_event("ccz_title_node_size",key,dword(node,0xF0),dword(node,0xF4));
        log_event("ccz_title_node_scale",key,dword(node,0xF8),dword(node,0xD0));
        Obj matrix=pointer(node,0xD0)?pointer(node,0xE0):pointer(node,0xE8);
        if(matrix)log_event("ccz_title_node_xy_bits",key,dword(matrix,0x30),dword(matrix,0x34));
    }
}
static void trace_phase(Obj root,unsigned phase,unsigned sample){
    unsigned stage=0x100+phase+(sample<<8);
    trace_object(stage,0xFFFF,root);
    constexpr int ids[]={3,5,8,9,10,11,12,13,20,22,24,26,28,30,32,34,35,36,37};
    for(int id:ids)trace_binding(stage,root,id,fn<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(root+0x130,id));
    if(button)trace_object(stage,0xFFFE,button);
}
#else
struct TraceCall {TraceCall(unsigned,unsigned){}};
static void trace_binding(unsigned,Obj,unsigned,Obj){}
static void trace_phase(Obj,unsigned,unsigned){}
#endif
static unsigned phase_frames;
static bool guards_match(){
    // Slot/CALL ownership is judged by the Layout hook manager; the function bodies are fingerprinted below.
    for(const auto &g:guards){unsigned long long h=0xCBF29CE484222325ull;for(unsigned i=0;i<g.size;++i)h=(h^image[g.rva+i])*0x100000001B3ull;if(h!=g.hash)return false;}
    return true;
}
static void start_click_trace(){ui_call_trace::start(reinterpret_cast<uintptr_t>(image),[](const char *kind,unsigned a,unsigned b,unsigned c){log_event(kind,a,b,c);});}
#ifndef NO_RUNTIME_LOG
// Forward all six callable operations unchanged; trace even before show() runs.
static void *__fastcall callback_copy(void *self,void *where){ COST_SCOPE("callback_copy");return ui_call_trace::call(ui_call_trace::CallbackCopy,0,reinterpret_cast<void *(__fastcall *)(void *,void *)>(ui_callable::vtable[0]),self,where);}
static void *__fastcall callback_move(void *self,void *where){ COST_SCOPE("callback_move");return ui_call_trace::call(ui_call_trace::CallbackMove,0,reinterpret_cast<void *(__fastcall *)(void *,void *)>(ui_callable::vtable[1]),self,where);}
static void __fastcall callback_invoke(void *self,Obj *arg){ COST_SCOPE("callback_invoke");start_click_trace();ui_call_trace::memory(ui_call_trace::CallbackInvoke,self,16);ui_call_trace::call(ui_call_trace::CallbackInvoke,0,reinterpret_cast<void(__fastcall *)(void *,Obj *)>(ui_callable::vtable[2]),self,arg);}
static void *__fastcall callback_type(void *self){ COST_SCOPE("callback_type");return ui_call_trace::call(ui_call_trace::CallbackType,0,reinterpret_cast<void *(__fastcall *)(void *)>(ui_callable::vtable[3]),self);}
static void __fastcall callback_delete(void *self,bool heap){ COST_SCOPE("callback_delete");ui_call_trace::call(ui_call_trace::CallbackDelete,0,reinterpret_cast<void(__fastcall *)(void *,bool)>(ui_callable::vtable[4]),self,heap);}
static void *__fastcall callback_get(void *self){ COST_SCOPE("callback_get");return ui_call_trace::call(ui_call_trace::CallbackGet,0,reinterpret_cast<void *(__fastcall *)(void *)>(ui_callable::vtable[5]),self);}
static void *callback_vtable[]={reinterpret_cast<void *>(&callback_copy),reinterpret_cast<void *>(&callback_move),reinterpret_cast<void *>(&callback_invoke),reinterpret_cast<void *>(&callback_type),reinterpret_cast<void *>(&callback_delete),reinterpret_cast<void *>(&callback_get)};
#endif
static void clicked(Obj control){
    start_click_trace();
    ui_call_trace::run(ui_call_trace::Clicked,[&]{
        log_event("ccz_title_clicked",control==button,current?dword(current,0x470):~0u,control?dword(control,0x40):0);
        if(current && control==button && dword(current,0x470)==0x13 && !(dword(control,0x40)&8))requested=true;
    });
}
static void destroy_pane(Obj object){if(object)reinterpret_cast<void(__fastcall *)(Obj,int)>(pointer(pointer(object,0),0))(object,1);}
static bool add_button(Obj state){
    Obj root=pointer(state,0x4B8);if(!root || pointer(root,0)!=image+kRoot || dword(root+0x130,0x10)!=kCount){log_event("ccz_title_layout_missing");return false;}
    Obj object=fn<Obj(__fastcall *)(size_t)>(GA(GA_3F90E0))(0x178);if(!object)return false;
    memset(object,0,0x178);fn<void(__fastcall *)(Obj)>(GA(GA_866FA0))(object);
    *reinterpret_cast<Obj *>(object)=image+GA(GA_1345530);fn<void(__fastcall *)(Obj)>(GA(GA_92D020))(object+0x130);*reinterpret_cast<Obj *>(object)=image+kPane;
    {TraceCall trace(GA(GA_870470),kButton);fn<unsigned long long(__fastcall *)(Obj,int,int,Obj,const char *,int,unsigned long long)>(GA(GA_870470))(object,0,0,root,"02_01_menu_btn_offset_00_cn",0,0);}
    trace_binding(1,root,kButton,object);
    Obj control;
    {TraceCall trace(GA(GA_9309C0),kButton);control=fn<Obj(__fastcall *)(Obj,int,int,int,int,int)>(GA(GA_9309C0))(object+0x130,2,0x1A,0,1,0);}
    if(!control){destroy_pane(object);return false;}
    {TraceCall trace(GA(GA_930640),kButton);fn<void(__fastcall *)(Obj,int,Obj,int,int,int)>(GA(GA_930640))(root+0x130,kButton,object,8,1,0);}
    trace_binding(2,root,kButton,object);
    if(fn<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(root+0x130,kButton)!=object){destroy_pane(object);return false;}
    *reinterpret_cast<unsigned *>(control+0x80)=kButton;*reinterpret_cast<unsigned *>(control+0x84)=1;
    reinterpret_cast<void(__fastcall *)(Obj,const wchar_t *)>(pointer(pointer(control,0),0x1D8))(control,L"조조전");
    alignas(16) unsigned char local[64]={};*reinterpret_cast<void ***>(local)=ui_callable::vtable;
#ifndef NO_RUNTIME_LOG
    *reinterpret_cast<void ***>(local)=callback_vtable;
#endif
    *reinterpret_cast<void (**)(Obj)>(local+8)=&clicked;*reinterpret_cast<void **>(local+56)=local;
    fn<void(__fastcall *)(Obj,int,void *)>(GA(GA_1A4FB0))(control,0x7FFFFFFE,local);
    reinterpret_cast<void(__fastcall *)(Obj,int)>(pointer(pointer(object,0),0xA0))(object,0);
    // Match native construction: input stays paused until the title menu resumes.
    *reinterpret_cast<unsigned *>(object+0x40)|=8;
    *reinterpret_cast<unsigned *>(control+0x40)|=8;
    {TraceCall trace(GA(GA_8685C0),kButton);fn<void(__fastcall *)(Obj,int,Obj,int)>(GA(GA_8685C0))(root,kButton,object,-1);}
    trace_binding(3,root,kButton,object);
    current=state;pane=object;button=control;requested=false;
    log_event("ccz_title_button_added",kButton,kCount,0);return true;
}
// The ninth row must be attached while the native title constructor is still
// building its output tree. The original 866230 call remains first.
static void __fastcall attach_after_native(Obj root,int id,Obj object,int order){ COST_SCOPE("attach_after_native");
    {TraceCall trace(GA(GA_8685C0),id);next_pick(next_attach,GA(GA_8685C0))(root,id,object,order);}
    if(initializing && root==pointer(initializing,0x4B8))trace_binding(4,root,id,object);
    if(id!=32 || !initializing || initializing_button_added)return;
    if(root!=pointer(initializing,0x4B8) || pointer(root,0)!=image+kRoot)return;
    if(add_button(initializing))initializing_button_added=true;
}
static void intro_visibility(Obj state);
static unsigned long long __fastcall initialize(Obj state){ COST_SCOPE("initialize");
    visibility_logged=false;logged_phase=~0u;phase_frames=0;initializing=state;initializing_button_added=false;
    memset(intro_rows,0,sizeof(intro_rows));
    unsigned long long result;
    {TraceCall trace(GA(GA_506ED0),0);result=next_pick(next_initialize,GA(GA_506ED0))(state);}
    if(current==state)trace_phase(pointer(state,0x4B8),dword(state,0x470),0);
    if(current==state)intro_visibility(state);
    initializing=nullptr;return result;
}
static void __fastcall leave(Obj state,Obj arg){ COST_SCOPE("leave");
    if(current==state){ccz_settings::abandon();current=pane=button=nullptr;requested=false;memset(intro_rows,0,sizeof(intro_rows));}
    next_pick(next_leave,GA(GA_4FBB70))(state,arg);   // native owns the extra pane through its layout manager
}
// 86F250 requires both low visibility bits. The old 0xA mask copied the
// enabled/focus bits but dropped bit 0, so the dynamically added pane/button
// could be registered and clickable while remaining invisible.
static void sync(Obj own,Obj source){
    if(!own || !source)return;
    unsigned before=dword(own,0x40),after=(before&~0xBu)|(dword(source,0x40)&0xBu);
    if(before!=after){
        *reinterpret_cast<unsigned *>(own+0x40)=after;
        // Native 86F250 caches inherited visibility at +10. Flag changes must
        // invalidate descendants as well (873370), just as native parent changes do.
        fn<void(__fastcall *)(Obj,int)>(GA(GA_875720))(own,1);
    }
    // 2026-11-03 live trace: native row32 alpha=255, added row37 alpha=0
    // despite flags=7 and valid render nodes. Both were initialized with A0(0),
    // but the ninth row has no native alpha animation. Use the original setter
    // so inherited alpha/render caches follow menu fades and modal transitions.
    if(dword(own,0x30)!=dword(source,0x30))
        fn<void(__fastcall *)(Obj,int)>(GA(GA_875800))(own,static_cast<int>(dword(source,0x30)));
}
// Native phase 12 starts title_out/menu_in (50B800); phases below it are
// logos/loading. Suppress menu rows only, never the title root/logo layers.
// Restore just our visibility bit before native menu transitions can change it.
static void intro_visibility(Obj state){
    Obj root=pointer(state,0x4B8);if(!root || pointer(root,0)!=image+kRoot)return;
    bool intro=dword(state,0x470)<12;
    for(unsigned i=0;i<9;++i){
        Obj row=fn<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(root+0x130,menu_ids[i]);
        if(!row)continue;
        if(intro){
            if(!intro_rows[i]){intro_rows[i]=row;intro_visible[i]=dword(row,0x40)&1;}
            if(dword(row,0x40)&1){
                *reinterpret_cast<unsigned *>(row+0x40)&=~1u;
                fn<void(__fastcall *)(Obj,int)>(GA(GA_875720))(row,1);
            }
        }else if(intro_rows[i]==row){
            *reinterpret_cast<unsigned *>(row+0x40)=(dword(row,0x40)&~1u)|intro_visible[i];
            fn<void(__fastcall *)(Obj,int)>(GA(GA_875720))(row,1);intro_rows[i]=nullptr;
        }
    }
}
static void __fastcall frame(Obj state,Obj a,Obj b){ COST_SCOPE("frame");
    if(current==state)intro_visibility(state);
    if(ui_call_trace::sample(ui_call_trace::TitleFrame,4))ui_call_trace::call(ui_call_trace::TitleFrame,GA(GA_50DEC0),next_pick(next_frame,GA(GA_50DEC0)),state,a,b);
    else next_pick(next_frame,GA(GA_50DEC0))(state,a,b);
    if(current!=state || !pane || !button)return;
    Obj root=pointer(state,0x4B8);if(!root)return;
    Obj peer=fn<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(root+0x130,32);
    sync(pane,peer);if(peer)sync(button,fn<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(peer+0x130,2));
    intro_visibility(state);
    if(!visibility_logged && peer){
        visibility_logged=true;log_event("ccz_title_visibility_flags",dword(peer,0x40),dword(pane,0x40),button?dword(button,0x40):0);
    }
    unsigned phase=dword(state,0x470);
    if(logged_phase!=phase){
        logged_phase=phase;phase_frames=0;
        log_event("ccz_title_phase",phase,dword(root,0x40),peer?dword(peer,0x40):0);
        log_event("ccz_title_row_flags",phase,dword(pane,0x40),dword(button,0x40));
        trace_phase(root,phase,1);
    }
    ++phase_frames;
    if(phase==19 && (phase_frames==2 || phase_frames==8 || phase_frames==30))trace_phase(root,phase,phase_frames);
    ccz_settings::tick(state);
    if(!requested)return;requested=false;
    if(dword(state,0x470)!=0x13 || (dword(button,0x40)&8))return;
    ui_call_trace::run(ui_call_trace::Show,[]{return ccz_settings::show();});   // next native title frame opens the asynchronous game editor
}
static bool install(){
    if(!guards_match()){log_event("ccz_title_guard_rejected");return false;}
    const unsigned kAttachSite=GA(GA_4EAC33);
    layout_client::Transaction tx;
    // Lifetime handlers first; the initialize slot last, so native initialization only creates our objects once
    // everything else is in place (Layout installs in registration order). The CALL target is judged by Layout.
    if(!layout_client::vt(kVtable+16,reinterpret_cast<void *>(&leave),reinterpret_cast<void **>(&next_leave)) ||
       !layout_client::vt(kVtable+40,reinterpret_cast<void *>(&frame),reinterpret_cast<void **>(&next_frame)) ||
       !layout_client::call(kAttachSite,reinterpret_cast<void *>(&attach_after_native),reinterpret_cast<void **>(&next_attach)) ||
       !layout_client::vt(kVtable+8,reinterpret_cast<void *>(&initialize),reinterpret_cast<void **>(&next_initialize))) {
        log_event("ccz_title_register_rejected");return false;
    }
    tx.done=true;log_event("ccz_title_installed");log_event("ccz_title_trace_revision",3,kCount,4);return true;
}
}
