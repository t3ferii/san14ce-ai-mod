// Convenience DLL only. All native calls run on the existing diplomacy UI thread.
// Evidence and state/ABI map: docs/DIPLOMACY_INTEL.md.
#include "diplomacy_intel.h"
#include "diplomacy_intel_profile.h"
#include <cstddef>
namespace diplomacy_intel {
// Same-mod patches inside guarded ranges (user 2026-10-28, live PID9808: both
// failed the guard and disabled overinvest/counterturn). Each window must hold
// either the original bytes or that module's own known patch; it is hashed as
// the original. Any other change still disables this feature.
struct KnownPatch {unsigned rva,size;unsigned char original[8],patched[8];bool any_rel32_jump;};
static const KnownPatch known_patches[]={
    // modpack ability 200 cap (modpack_tables.h 0x230098) inside 22FCE0.
    {0x230098,8,{0x74,0x48,0x66,0x0F,0x1F,0x44,0x00,0x00},{0x75,0x06,0x83,0xFE,0x64,0x7C,0x43,0x90},false},
    // advisor_briefing text relay: first MOV of 930D20 replaced by JMP rel32.
    {0x930D20,5,{0x48,0x89,0x5C,0x24,0x08},{0xE9},true},
};
static unsigned char guarded_byte(unsigned rva,bool &ok) {
    for(const auto &k:known_patches)if(rva>=k.rva && rva<k.rva+k.size) {
        const unsigned char *p=image+k.rva;
        if(!memcmp(p,k.original,k.size))return k.original[rva-k.rva];
        if(k.any_rel32_jump?p[0]==0xE9:!memcmp(p,k.patched,k.size))return k.original[rva-k.rva];
        ok=false;return image[rva];
    }
    return image[rva];
}
static bool guards_match() {
    if(memcmp(image+final_tail,final_tail_bytes,sizeof(final_tail_bytes)))return false;
    for(const auto &g:guards) {
        unsigned long long h=0xcbf29ce484222325ull;bool ok=true;
        for(unsigned i=0;i<g.size;++i)h=(h^guarded_byte(g.rva+i,ok))*0x100000001b3ull;
        if(!ok || h!=g.hash)return false;
    }
    for(const auto &s:slots)if(pointer(image,s.rva)!=image+s.original)return false;
    for(const auto &s:sites)if(image[s.rva]!=0xE8 || s.rva+5+*reinterpret_cast<int *>(image+s.rva+1)!=s.original)return false;
    return true;
}
#if (defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)) || defined(DIPLOMACY_INTEL_FIXTURE)
using namespace diplomacy_intel_core;
using Rate=int(__fastcall *)(Obj,Obj,Obj,int,int,int,int);
using Resolve=int(__fastcall *)(Obj,Obj,Obj,int,Obj,int,int);
struct FinalArgs {Obj force=nullptr,person=nullptr,target=nullptr,object=nullptr;int kind=0;};
struct Pending {Obj state=nullptr;Round round;FinalArgs args;DWORD thread=0;
#ifndef NO_RUNTIME_LOG
    int last_phase=-1,last_amount=-1,last_start=-2147483647,last_display=-2147483647,last_score=-2147483647;
    int rate_base=-2147483647,rate_amount=-1,rate_kind=-1,rate_defending=-1;
#endif
};
static Pending pending;
static CodeLink final_tail_link;
// Native 815EA0 has already consumed the resolver result at this point. RAX
// and flags are dead; RSI is the state. No call, stack change, or native local
// is introduced. Always retain the original container cleanup at 816806.
static void build_final_tail_gate(Obj at,Obj resume,Obj cleanup) {
    constexpr size_t deferred=offsetof(Pending,round)+offsetof(Round,deferred);
    static_assert(deferred<128,"short field displacement");
    const unsigned char code[]={0x48,0xB8,0,0,0,0,0,0,0,0,
        0x48,0x39,0x30,0x75,0x14,0x80,0x78,static_cast<unsigned char>(deferred),0,0x74,0x0E};
    memcpy(at,code,sizeof(code));Obj owner=reinterpret_cast<Obj>(&pending);memcpy(at+2,&owner,8);
    absolute_jump(at+21,cleanup);
    memcpy(at+35,final_tail_bytes,sizeof(final_tail_bytes));
    absolute_jump(at+40,resume);
}
static thread_local Obj final_scope;
static Obj view() {return pending.state?pointer(pending.state,0x4C0):nullptr;}
static Obj window() {Obj v=view();return v?pointer(v,0x188):nullptr;}
static int &number(Obj p,unsigned offset) {return *reinterpret_cast<int *>(p+offset);}
static int kind(Obj cmd) {
    return cmd?(*reinterpret_cast<int(__fastcall **)(Obj)>(pointer(cmd,0)+0x98))(cmd):0;
}
#ifndef NO_RUNTIME_LOG
// Native negotiation continuations run on a different thread from the input
// controls. Scope diagnostics to the original state/resolver call, not just the
// thread that entered the dialog. This does not schedule or move any game call.
static thread_local Obj trace_scope;
struct TraceScope {
    Obj previous;
    explicit TraceScope(Obj state):previous(trace_scope){trace_scope=state;}
    ~TraceScope(){trace_scope=previous;}
};
static bool trace_owner(){return pending.state && (pending.thread==GetCurrentThreadId() || trace_scope==pending.state || final_scope==pending.state);}
#endif
static int native_resolve(Obj force,Obj person,Obj target,int k,Obj object,int value,int special){
#ifndef NO_RUNTIME_LOG
    TraceScope scope(pending.state);
    if(trace_owner())log_event("diplomacy_resolve_enter",k,value,special);
#endif
    int result=fn<Resolve>(0x324E20)(force,person,target,k,object,value,special);
#ifndef NO_RUNTIME_LOG
    if(trace_owner())log_event("diplomacy_resolve_return",k,value,result);
#endif
    return result;
}
static void trace_snapshot(Obj state){
#ifndef NO_RUNTIME_LOG
    if(state!=pending.state || !trace_owner() || pending.round.done)return;
    Obj v=pointer(state,0x4C0),w=v?pointer(v,0x188):nullptr;if(!w)return;
    int phase=number(state,0x4D8),amount=number(w,0x210),start=number(w,0x214),display=number(w,0x218),score=number(state,0x594);
    if(phase==pending.last_phase && amount==pending.last_amount && start==pending.last_start && display==pending.last_display && score==pending.last_score)return;
    pending.last_phase=phase;pending.last_amount=amount;pending.last_start=start;pending.last_display=display;pending.last_score=score;
    log_event("diplomacy_trace_input",phase,amount,number(w,0x20C));
    log_event("diplomacy_trace_scores",start,display,score);
    log_event("diplomacy_trace_spent",number(state,0x59C),pending.round.extra,kind(pointer(w,0x1D8)));
#else
    (void)state;
#endif
}
#ifndef NO_RUNTIME_LOG
static int __fastcall native_roll(int bound,int mode,int force,int person,int target,int threshold){
    // Same six arguments, exactly one original draw, unchanged return value.
    int roll=fn<int(__fastcall *)(int,int,int,int,int,int)>(0x3FB600)(bound,mode,force,person,target,threshold);
    if(trace_owner()){
        log_event("diplomacy_rng_roll",roll,threshold,roll<threshold);
        log_event("diplomacy_rng_seed",force,person,target);
        log_event("diplomacy_rng_stage",number(pending.state,0x4D8),pending.round.extra,bound);
    }
    return roll;
}
#endif
static void show(Obj p,int on) {if(p)(*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(p,0)+0xC8))(p,on);}
static void window_call(Obj w,unsigned slot,int arg) {
    (*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(w,0)+slot))(w,arg);
}
static bool incoming(Obj w) {
    if(!w || w!=window())return false;
    const int k=kind(pointer(w,0x1D8));
    if(k!=4 && k!=5 && k!=8 && k!=9)return false;
    auto is_player=fn<int(__fastcall *)(Obj)>(0x2337B0);
    return !is_player(pointer(w,0x180)) && is_player(pointer(w,0x190));
}
static bool over(Obj w) {
    return mod_features.diplomacy_overinvest && incoming(w) && number(w,0x220)==0;
}
static unsigned __fastcall enter(Obj state) {
    pending={};
    unsigned r=fn<unsigned(__fastcall *)(Obj)>(0x810CA0)(state);
    pending.state=state;pending.thread=GetCurrentThreadId();return r;
}
static void __fastcall leave(Obj state,Obj second,Obj third) {
    if(pending.state==state)pending={};
    fn<void(__fastcall *)(Obj,Obj,Obj)>(0x810640)(state,second,third);
}
// Reproduce the native amount coefficient selection from the ASSEMBLY, not the
// incomplete Ghidra SSE pseudocode. Numeric balance tables remain game-owned.
static Coefficients coefficients(Obj person,Obj target,int k,bool defending) {
    if(defending)person=table(0x158,word(target,0x52),6000);
    if(!person)return {};
    int stat=fn<int(__fastcall *)(Obj,int,int)>(0x22FCE0)(person,3,0xFFF);
    unsigned weight=stat>=number(image,0x1A48844)?0x1A48850:
        stat>=number(image,0x1A4885C)?0x1A48868:stat>=number(image,0x1A48874)?0x1A48880:0x1A4888C;
    unsigned a=k==4?0x1A48548:k==5?0x1A487AC:k==8?0x1A485B0:k==9?0x1A4870C:0;
    unsigned floor_rva=k==4?0x27D2564:k==5?0x27D257C:k==8?0x27D2568:0x27D2574;
    if(!a)return {};
    return {static_cast<double>(number(image,weight)),static_cast<double>(number(image,a)),
        static_cast<double>(number(image,a+12)),static_cast<double>(number(image,a+24)),
        static_cast<double>(number(image,a+44)),static_cast<double>(number(image,floor_rva))};
}
static int __fastcall rate(Obj force,Obj person,Obj target,int k,int base,int amount,int defending) {
    Obj w=window();
    bool modified=over(w) && target==pointer(w,0x190) && force==pointer(w,0x180) && defending;
    Coefficients c=modified?coefficients(person,target,k,true):Coefficients{};
    int result=modified && usable(c)?score(base,amount,true,c):fn<Rate>(0x322B30)(force,person,target,k,base,amount,defending);
#ifndef NO_RUNTIME_LOG
    if(trace_owner() && (pending.rate_base!=base || pending.rate_amount!=amount || pending.rate_kind!=k || pending.rate_defending!=defending)){
        pending.rate_base=base;pending.rate_amount=amount;pending.rate_kind=k;pending.rate_defending=defending;
        log_event("diplomacy_rate_input",base,amount,defending);log_event("diplomacy_rate_output",k,result,modified && usable(c));
    }
#endif
    return result;
}
static int __fastcall ai_response(int phase,Obj command,int spent,int *value) {
    int raw=*value;
    const bool keep=over(window()) && final_scope==pending.state && command==pointer(pending.state,0x4B0);
    if(keep)*value=probability(raw);
    int amount=fn<int(__fastcall *)(int,Obj,int,int *)>(0x804770)(phase,command,spent,value);
    if(keep)*value=preserve_reserve(raw,*value);
#ifndef NO_RUNTIME_LOG
    if(trace_owner())log_event("diplomacy_ai_response",raw,*value,amount);
#endif
    return amount;
}
static int __fastcall resolve(Obj force,Obj person,Obj target,int k,Obj object,int value,int special) {
    Obj w=window();
    if(final_scope==pending.state && w && pending.round.defer(mod_features.diplomacy_counterturn,incoming(w),number(w,0x220)!=0)) {
        pending.args={force,person,target,object,k};log_event("diplomacy_resolve_deferred",k,value,number(pending.state,0x594));return 0;
    }
    return native_resolve(force,person,target,k,object,probability(value),special);
}
// Native final stage sends this close event before its result is applied on the
// next frame. Suppress only that one event while returning the final player turn.
static void __fastcall event(Obj layout,int value) {
    if(layout==view() && value==0x7FFFFFFD && final_scope==pending.state && pending.round.deferred)return;
    fn<void(__fastcall *)(Obj,int)>(0x805010)(layout,value);
}
static void restore_extra_ui(Obj state,bool initial) {
    if(state!=pending.state || !pending.round.extra || number(state,0x4D8)!=8)return;
    if(Obj actors=pointer(state,0x478))for(int slot:{3,4}) {
        int was_out=number(actors,0x1DC+slot*4);
        if((initial || was_out) && pointer(actors,0x178+slot*8)) {
            // Native face-in cancels the matching pending face-out animation.
            fn<void(__fastcall *)(Obj,int)>(0x83BFD0)(actors,slot);
            log_event("diplomacy_actor_rearm",slot,was_out,number(actors,0x1DC+slot*4));
        }
    }
    Obj v=view(),bar=v?pointer(v,0x198):nullptr;
    if(bar) {
        int mode=number(bar,0x188);
        if(initial || mode==1 || mode==2) {
            // Same resume entry as native 814380. 856500 deliberately skips
            // hidden modes 1/2; 856620 handles their full/fade-in restoration.
            fn<void(__fastcall *)(Obj)>(0x856620)(bar);
            if(initial || number(bar,0x188)!=mode)
                log_event("diplomacy_footer_rearm",mode,number(bar,0x188),number(bar,0x178));
        }
    }
}
static void __fastcall frame(Obj state,Obj second,Obj third,Obj fourth) {
#ifndef NO_RUNTIME_LOG
    TraceScope scope(state);
#endif
    trace_snapshot(state);
    fn<void(__fastcall *)(Obj,Obj,Obj,Obj)>(0x811210)(state,second,third,fourth);
    // Message completion can arrive after rearm; repair only the extra input
    // phase. A completed/ordinary negotiation must keep native exit behavior.
    restore_extra_ui(state,false);
}
static void rearm(Obj state) {
    Obj w=window(),v=view();
    pending.round.reopen();
    number(state,0x4D8)=8;number(state,0x4F8)=0;
    number(w,0x178)=-1;number(w,0x210)=0;number(w,0x214)=number(state,0x594);
    number(w,0x224)=0;
    // Same user-side setup as native phase 7 -> 8; resources have already been
    // debited by phase 11. No refund, duplicated debit, new AI move or save data.
    if(Obj button=pointer(w,0x1F8)) {
        // Native 814380 ORs this bit in both objects before phase 8.
        number(button,0x40)|=2;
        if(Obj child=pointer(button,0x180))number(child,0x40)|=2;
    }
    show(v,1);show(w,1);
    fn<void(__fastcall *)(Obj,int)>(0x824880)(state,1);
    window_call(w,0x1C8,1);window_call(w,0x1D0,0);
    (*reinterpret_cast<void(__fastcall **)(Obj,float)>(pointer(w,0)+0x1D8))(w,-1.0f);
    restore_extra_ui(state,true);
    fn<void(__fastcall *)(Obj,int)>(0x841180)(pointer(state,0x478),1);
    log_event("diplomacy_counterturn",static_cast<unsigned>(number(state,0x594)));
}
static void finish_extra(Obj state) {
    FinalArgs a=pending.args;pending.round.finish();
    if(Obj w=window()){
        log_event("diplomacy_final_scores",number(state,0x594),number(w,0x218),number(w,0x214));
        log_event("diplomacy_final_spent",number(state,0x59C),number(w,0x210),number(w,0x20C));
    }
    const int result=native_resolve(a.force,a.person,a.target,a.kind,a.object,probability(number(state,0x594)),0);
    number(state,0x4F8)=result?1:0;number(state,0x4D8)=result?12:13;
    fn<void(__fastcall *)(Obj,int)>(0x824880)(state,0);
    event(view(),0x7FFFFFFD);
    fn<void(__fastcall *)(Obj)>(0x806950)(state);
    fn<void(__fastcall *)(Obj,int)>(0x841180)(pointer(state,0x478),0);
    log_event("diplomacy_counterturn_done",result);
}
static void __fastcall final_phase(Obj state) {
    if(state!=pending.state || !incoming(window())) {fn<void(__fastcall *)(Obj)>(0x815EA0)(state);return;}
    if(pending.round.extra) {finish_extra(state);return;}
    Obj saved=final_scope;final_scope=state;
    fn<void(__fastcall *)(Obj)>(0x815EA0)(state);
    if(pending.round.deferred)rearm(state);
    final_scope=saved;
}
static void text(Obj w,int id,const wchar_t *value) {fn<void(__fastcall *)(Obj,int,const wchar_t *,int)>(0x930D20)(w+0x130,id,value,1);}
template<unsigned Original> static void __fastcall refresh(Obj w) {
    if(!over(w) || number(pending.state,0x4D8)!=8 || number(w,0x224)!=0 || number(w,0x208)>=number(w,0x20C)) {
        fn<void(__fastcall *)(Obj)>(Original)(w);return;
    }
    // Native 82C510/82C960/82CD60/82D170 amount-controls branch, using the
    // available resource as maximum instead of 3228F0's 100%-threshold amount.
    const int available=number(w,0x20C);
    constexpr int amount_id=Original==0x82C960?0x3C:Original==0x82CD60?0x7A:0x4A;
    constexpr int total_id=Original==0x82C960?0x42:Original==0x82CD60?0x80:0x50;
    constexpr int slider_id=Original==0x82C960?0x4D:Original==0x82CD60?0x8B:0x5B;
    constexpr int marker_id=Original==0x82C960?0x49:Original==0x82CD60?0x87:0x57;
    int selected=number(w,0x210);if(selected<0)selected=0;if(selected>available)selected=available;
    number(w,0x210)=selected;number(w,0x21C)=available;
    fn<void(__fastcall *)(Obj,int,int)>(0x92F7B0)(w+0x130,amount_id,1);
    wchar_t value[64];swprintf_s(value,L"%d",selected);text(w,amount_id,value);
    swprintf_s(value,L"/%d",available);text(w,total_id,value);
    Obj slider=fn<Obj(__fastcall *)(Obj,int)>(0x92CEE0)(w+0x130,slider_id);
    if(!slider)return;
    (*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(slider,0)+0xD0))(slider,1);
    fn<void(__fastcall *)(Obj,int)>(0x85B160)(slider,available);
    fn<void(__fastcall *)(Obj,int)>(0x85B1A0)(slider,available);
    fn<void(__fastcall *)(Obj,int)>(0x85CF80)(slider,selected);
    fn<void(__fastcall *)(Obj,int,int)>(0x92F810)(w+0x130,marker_id,0);
}
template<unsigned Original> static void __fastcall redraw(Obj w,float value) {
    fn<void(__fastcall *)(Obj,float)>(Original)(w,value);
    if(!over(w))return;
    int raw=number(w,0x218);if(raw>=0)return;
    constexpr int defense_id=Original==0x82D880?0x75:Original==0x82DB90?0x56:0x83;
    wchar_t label[64];swprintf_s(label,L"%d%%",100-raw);text(w,defense_id,label);
}
static void *replacements[]={reinterpret_cast<void *>(&frame),reinterpret_cast<void *>(&enter),reinterpret_cast<void *>(&leave),reinterpret_cast<void *>(&event),
    reinterpret_cast<void *>(&refresh<0x82C960>),reinterpret_cast<void *>(&redraw<0x82D880>),
    reinterpret_cast<void *>(&refresh<0x82C510>),reinterpret_cast<void *>(&redraw<0x82D570>),
    reinterpret_cast<void *>(&refresh<0x82CD60>),reinterpret_cast<void *>(&redraw<0x82DB90>),
    reinterpret_cast<void *>(&refresh<0x82D170>),reinterpret_cast<void *>(&redraw<0x82D570>)};
static CallLink links[sizeof(sites)/sizeof(sites[0])];
static bool install() {
    if(!guards_match())return false;
    Obj relay=allocate_near(image+sites[0].rva);if(!relay)return false;
    void *targets[]={reinterpret_cast<void *>(&rate),reinterpret_cast<void *>(&rate),reinterpret_cast<void *>(&rate),
        reinterpret_cast<void *>(&rate),reinterpret_cast<void *>(&ai_response),reinterpret_cast<void *>(&resolve),reinterpret_cast<void *>(&final_phase)
#ifndef NO_RUNTIME_LOG
        ,reinterpret_cast<void *>(&native_roll)
#endif
    };
    constexpr unsigned call_count=sizeof(sites)/sizeof(sites[0]);
    bool ok=true;
    for(unsigned i=0;i<call_count && ok;++i) {absolute_jump(relay+32*i,targets[i]);ok=call_prepare(links[i],sites[i].rva,relay+32*i);}
    build_final_tail_gate(relay+256,image+final_resume,image+final_cleanup);
    ok=ok && prepare_link(final_tail_link,final_tail,0xE9,relay+256);
    if(ok)ok=memcmp(final_tail_link.original+(final_tail-final_tail_link.block),final_tail_bytes,sizeof(final_tail_bytes))==0;
    DWORD old;
    if(!ok || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,310)) {
        VirtualFree(relay,0,MEM_RELEASE);return false;
    }
    unsigned nc=0,np=0;
    if(!publish_link(final_tail_link)){VirtualFree(relay,0,MEM_RELEASE);log_event("diplomacy_intel_publish_rejected");return false;}
    for(;nc<call_count;++nc)if(!call_publish(links[nc]))break;
    constexpr unsigned slot_count=sizeof(slots)/sizeof(slots[0]);
    if(nc==call_count)for(;np<slot_count;++np)if(!patch_pointer(slots[np].rva,slots[np].original,replacements[np]))break;
    if(nc!=call_count || np!=slot_count) {
        while(np) {--np;restore_pointer(slots[np].rva,slots[np].original,replacements[np]);}
        while(nc)call_restore(links[--nc]);
        restore_link(final_tail_link);
        log_event("diplomacy_intel_publish_rejected");return false;
    }
    log_event("diplomacy_intel_installed",mod_features.diplomacy_overinvest,mod_features.diplomacy_counterturn);return true;
}
#endif
}
static bool diplomacy_intel_guards_match() {return diplomacy_intel::guards_match();}
