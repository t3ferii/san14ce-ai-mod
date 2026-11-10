// User 2026-11-03: trace the real call-through, not just selected values afterward.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <memory>
#include "../src/native_ui_call_trace.inl"
struct Event {const char *kind;unsigned a,b,c;};
static Event events[2048];static unsigned count,checks,calls;
static void check(bool v,const char *why){++checks;if(!v){std::fprintf(stderr,"%s\n",why);ExitProcess(1);}}
static void writer(const char *kind,unsigned a,unsigned b,unsigned c){if(count<2048)events[count++]={kind,a,b,c};}
static unsigned named(const char *kind,unsigned stage=~0u){unsigned n=0;for(unsigned i=0;i<count;++i)if(!strcmp(events[i].kind,kind) && (stage==~0u || events[i].b==stage))++n;return n;}
static uintptr_t __fastcall thirteen(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d,uintptr_t e,uintptr_t f,uintptr_t g,uintptr_t h,uintptr_t i,uintptr_t j,uintptr_t k,uintptr_t l,uintptr_t m){++calls;return a+2*b+3*c+4*d+5*e+6*f+7*g+8*h+9*i+10*j+11*k+12*l+13*m;}
static void __fastcall void_target(unsigned v){calls+=v;}
static unsigned __fastcall raise_target(unsigned){RaiseException(0xE0011420,0,0,nullptr);return 0;}
static bool outer_handler(){
    __try{ui_call_trace::call(99,0x1234,&raise_target,1u);}
    __except(EXCEPTION_EXECUTE_HANDLER){return true;}
    return false;
}
int main(){
    ui_call_trace::begin(0,&writer);
    auto result=ui_call_trace::call(12,0x2C1360,&thirteen,uintptr_t(1),uintptr_t(2),uintptr_t(3),uintptr_t(4),uintptr_t(5),uintptr_t(6),uintptr_t(7),uintptr_t(8),uintptr_t(9),uintptr_t(10),uintptr_t(11),uintptr_t(12),uintptr_t(13));
    check(result==819 && calls==1,"all 13 register/stack arguments and return preserved with one invocation");
    ui_call_trace::call(13,0x1234,&void_target,2u);check(calls==3,"void call preserves arguments and executes once");
    auto owned=ui_call_trace::run(14,[]{return std::make_unique<unsigned>(77);});check(owned && *owned==77,"C++ stage preserves move-only result");
    try {ui_call_trace::run(15,[]{throw 73;});check(false,"exception must escape");}catch(int value){check(value==73,"C++ exception is rethrown unchanged");}
    check(outer_handler(),"native exception continues to the caller's handler");
#ifndef NO_RUNTIME_LOG
    check(named("ccz_ui_enter",12)==1 && named("ccz_ui_exit",12)==1,"successful native call has exactly one entry and exit");
    check(named("ccz_ui_arg")==15,"all 13 arguments plus void and fault arguments recorded before targets");
    check(named("ccz_ui_exit",15)==0 && named("ccz_ui_cpp_throw",15)==1,"throwing C++ stage has no misleading successful exit");
    check(named("ccz_ui_fault",99)==1 && named("ccz_ui_exit",99)==0,"native fault has an entry/filter record but no successful exit");
    bool first=false,last=false;for(unsigned i=0;i<count;++i)if(!strcmp(events[i].kind,"ccz_ui_arg") && events[i].a==1){if(events[i].b==0 && events[i].c==1)first=true;if(events[i].b==12 && events[i].c==13)last=true;}
    check(first && last,"argument order includes the last stack argument");
    unsigned before=count;{ui_call_trace::Silence silence(true);ui_call_trace::call(16,0x1234,&void_target,1u);}check(count==before && calls==4,"bounded frame sampling silences logs without skipping execution");
#else
    check(!count,"shipping build emits no trace or fault events");
#endif
    ui_call_trace::end();unsigned end_count=count;ui_call_trace::call(17,0x1234,&void_target,1u);check(count==end_count,"disabled tracing forwards directly without events");
    std::printf("{\"status\":\"ui_call_trace_fixture_passed\",\"checks\":%u,\"live_game\":false}\n",checks);
}
