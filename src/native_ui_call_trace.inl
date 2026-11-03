// Diagnostic call-through wrappers. No hook, recovery, scheduling or ABI change.
#pragma once
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>
namespace ui_call_trace {
enum Stage:unsigned {Show=10,Catalog=11,Profile=12,Ready=20,Release=21,Step=22,
    Choices=30,Open=31,EditorFrame=40,Attach=41,OldList=42,OldBar=43,Changed=44,
    Refresh=45,Collect=46,Preview=47,Overlay=48,
    CallbackInvoke=60,CallbackCopy=61,CallbackMove=62,CallbackType=63,CallbackDelete=64,CallbackGet=65,Clicked=66,TitleFrame=67,
    Create=100,Enqueue=101,Configure=102,Retain=103,Sidebar=104,Completion=105,
    ListAlloc=200,ListCtor=201,ListInit=202,ListChecks=203,ListEnter=204,
    BarAlloc=210,BarCtor=211,BarInit=212,BarBind=213,BarEnter=214,
    DisposeManager=220,DisposeLeave=221,DisposeDestroy=222,
    DialogFrame=230,DialogControls=231,Move=240,Width=241,Height=242};
using Writer=void(*)(const char *,unsigned,unsigned,unsigned);
#ifndef NO_RUNTIME_LOG
static Writer writer;static uintptr_t image_base;static LONG enabled,sequence;
static thread_local unsigned muted;
static unsigned samples[256];
static bool active(){return !muted && InterlockedCompareExchange(&enabled,0,0)!=0;}
static void emit(const char *s,unsigned a,unsigned b=0,unsigned c=0){if(writer)writer(s,a,b,c);}
static void bits(const char *s,unsigned id,uintptr_t value){emit(s,id,static_cast<unsigned>(value),static_cast<unsigned>(value>>32));}
static void begin(uintptr_t base,Writer out){writer=out;image_base=base;sequence=0;for(auto &n:samples)n=0;InterlockedExchange(&enabled,1);emit("ccz_ui_trace_start",1,GetCurrentThreadId(),0);bits("ccz_ui_image",0,base);}
static void start(uintptr_t base,Writer out){if(!InterlockedCompareExchange(&enabled,0,0))begin(base,out);}
static void end(){if(active())emit("ccz_ui_trace_end",static_cast<unsigned>(sequence));InterlockedExchange(&enabled,0);}
struct Silence {bool value;explicit Silence(bool v):value(v){if(value)++muted;}~Silence(){if(value)--muted;}};
static bool sample(unsigned stage,unsigned count){return active() && stage<256 && samples[stage]++<count;}
static unsigned target(uintptr_t address){return address>=image_base && address-image_base<0x3000000?static_cast<unsigned>(address-image_base):0;}
struct Stamp {unsigned id=0,stage=0;LARGE_INTEGER time{};ULONG64 cycles=0;};
static Stamp enter(unsigned stage,unsigned rva){
    Stamp s;if(!active())return s;
    s.id=static_cast<unsigned>(InterlockedIncrement(&sequence));s.stage=stage;
    // The kernel-backed common writer completes before the target is invoked.
    emit("ccz_ui_enter",s.id,stage,rva);
    QueryThreadCycleTime(GetCurrentThread(),&s.cycles);QueryPerformanceCounter(&s.time);return s;
}
static void leave(const Stamp &s,uintptr_t result=0){
    if(!s.id)return;LARGE_INTEGER now,freq;ULONG64 cycles=0;
    QueryPerformanceCounter(&now);QueryThreadCycleTime(GetCurrentThread(),&cycles);QueryPerformanceFrequency(&freq);
    emit("ccz_ui_exit",s.id,s.stage,0);bits("ccz_ui_result",s.id,result);
    emit("ccz_ui_us",s.id,s.stage,static_cast<unsigned>((now.QuadPart-s.time.QuadPart)*1000000/freq.QuadPart));
    emit("ccz_ui_cycles",s.id,s.stage,static_cast<unsigned>(cycles-s.cycles));
}
template<class T>static uintptr_t word(const T &value){
    if constexpr(std::is_pointer_v<T>)return reinterpret_cast<uintptr_t>(value);
    else if constexpr(std::is_integral_v<T> || std::is_enum_v<T>)return static_cast<uintptr_t>(value);
    else return 0;
}
static void argument(unsigned id,unsigned index,uintptr_t value){emit("ccz_ui_arg",id,index,static_cast<unsigned>(value));emit("ccz_ui_arg_hi",id,index,static_cast<unsigned>(value>>32));}
static void memory(unsigned stage,const void *data,unsigned bytes){
    if(!active())return;bits("ccz_ui_data_ptr",stage,reinterpret_cast<uintptr_t>(data));
    if(!data){emit("ccz_ui_data_null",stage);return;}
    auto p=static_cast<const unsigned char *>(data);
    for(unsigned offset=0;offset+4<=bytes && offset<128;offset+=4){unsigned v;memcpy(&v,p+offset,4);emit("ccz_ui_data",stage,offset,v);}
}
static int fault(unsigned id,unsigned stage,EXCEPTION_POINTERS *p){
    emit("ccz_ui_fault",id,stage,p->ExceptionRecord->ExceptionCode);
    bits("ccz_ui_fault_ip",id,reinterpret_cast<uintptr_t>(p->ExceptionRecord->ExceptionAddress));
    if(p->ExceptionRecord->NumberParameters>=2)bits("ccz_ui_fault_address",id,p->ExceptionRecord->ExceptionInformation[1]);
    bits("ccz_ui_fault_rcx",id,p->ContextRecord->Rcx);bits("ccz_ui_fault_rdx",id,p->ContextRecord->Rdx);
    bits("ccz_ui_fault_r8",id,p->ContextRecord->R8);bits("ccz_ui_fault_r9",id,p->ContextRecord->R9);
    return EXCEPTION_CONTINUE_SEARCH; // Observe; never swallow or resume a failed call.
}
template<class F,class... A>static auto guarded(unsigned id,unsigned stage,F f,A... args)->decltype(f(args...)){
    static_assert((std::is_trivially_copyable_v<A> && ...));
    __try{return f(args...);}
    __except(fault(id,stage,GetExceptionInformation())){__assume(0);}
}
template<class F,class... A>static auto call(unsigned stage,unsigned rva,F f,A... args)->decltype(f(args...)){
    if(!active())return f(args...);
    Stamp s=enter(stage,rva);bits("ccz_ui_target",s.id,reinterpret_cast<uintptr_t>(f));unsigned index=0;(argument(s.id,index++,word(args)),...);
    // Start the measurement after argument logging.
    QueryThreadCycleTime(GetCurrentThread(),&s.cycles);QueryPerformanceCounter(&s.time);
    if constexpr(std::is_void_v<decltype(f(args...))>){guarded(s.id,stage,f,args...);leave(s);}
    else {auto result=guarded(s.id,stage,f,args...);leave(s,word(result));return result;}
}
template<class F>static auto run(unsigned stage,F &&f)->decltype(f()){
    Stamp s=enter(stage,0);
    try {
        if constexpr(std::is_void_v<decltype(f())>){f();leave(s);}
        else {auto result=f();leave(s,word(result));return result;}
    }catch(...){if(s.id)emit("ccz_ui_cpp_throw",s.id,stage,0);throw;}
}
#else
static void begin(uintptr_t,Writer){}static void start(uintptr_t,Writer){}static void end(){}
struct Silence {explicit Silence(bool){}};
static bool sample(unsigned,unsigned){return false;}
static unsigned target(uintptr_t){return 0;}
static void memory(unsigned,const void *,unsigned){}
template<class F,class... A>static auto call(unsigned,unsigned,F f,A... args)->decltype(f(args...)){return f(args...);}
template<class F>static auto run(unsigned,F &&f)->decltype(f()){return f();}
#endif
}
