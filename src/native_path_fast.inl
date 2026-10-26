// Sole owner SAN14_Convenience.dll, original UI/AI caller threads only.
// Evidence/alias closure: tests/verify_path_fast.py; NATIVE_FUNCTIONS path_fast.
#include "path_fast.h"
namespace path_fast {
static constexpr unsigned entry=0x183A30,debug_rva=0x2141A84;
struct Guard {unsigned rva,size;unsigned long long hash;};
static const Guard guards[]={
    {0x1843F0,0x18D,0x2bc2a29fa00bcd58ull},
    {0x183A30,0x9A2,0x11cbf08ff66c9febull},
    {0x18AE60,0x132,0x73037cef8a22e554ull},
    {0x18AFA0,0x206,0x55ae3e62de7508d0ull},
    {0x18DD40,0x52,0x3b589a2996fa3f21ull},
    {0x2A7720,0x148,0x1d6d4c9e36e1ff90ull},
    {0x2AE6F0,0x8B,0x15208ddabb0f2913ull},
    {0x1864F0,0x1FB,0x2afa522fc7bc2fa3ull},
    {0x18A080,0x33E,0x0fbd138cba30277aull},
    {0x2CA280,0x155,0x8ab0c33d562dec08ull},
    {0x22FB10,0x2C,0x99a5ed6df7e7ff2eull},
    {0x32F500,0x24,0xb302042a0360860cull},
    {0x232650,0x27,0x8ea28a06f23303dbull},
    {0x232BD0,0x1E,0x954452f9468380c5ull},
    {0xEE90,0x2A,0x0f0f24adc0fc7152ull},
    {0x2C6950,0x57,0xee7d5726d0edb259ull},
    {0x2C6850,0xBB,0x27ce261532697d4aull},
    {0x1879C0,0xC1,0x2eb18e55c7274edcull},
    {0x2AA860,0x9E,0x293ef7107333cbbdull},
    {0x2AEE80,0x23,0x0bbc93973ca1bab9ull},
    {0x22CC40,0x8B,0x66e1d780bcf40acdull},
    {0x232730,0x80,0x1ce31f2eb2b32750ull},
    {0x32C130,0xCD,0x9b56391fd58202edull},
    {0x22D740,0x4D,0xd762f2a729878b2eull},
    {0x2C3780,0x6F,0x7cdaad4223582c75ull},
    {0xBE120,0x8A,0xcb7a49e55d10b330ull},
    {0x2211F0,0x35,0x3be6c62024a0e8c6ull},
    {0x2EF540,0x4C,0x1c10338fc64faa6cull},
    {0x2B02A0,0x515,0x2dc04f9cfef07249ull},
    {0x221FC0,0xF,0xd797f102380b2965ull},
    {0x3E2610,0x50,0xd4a8e9910122c16cull},
    {0x3F6C50,0x58,0xe25ef5e2ea6bf1ccull},
    {0x4DE80,0x68,0xab8858ea0c64e76cull},
    {0x184CF0,0x1B5,0x68da7c8c17b914bfull},
    {0x185620,0x1B5,0x3e728d11fce15e42ull},
    {0x1534D0,0x43,0xae2dd00988a7678aull},
    {0x149FE0,0x9B,0xe3f577fb94671d27ull}
};
// AI-CONTRACT [hook boundary]: 183A30/184485 belong to convenience; 1843E0 entry belongs to combat. Preserve mutual exclusion with path_count and native-search reentrancy guards; delegation must restore native node state.
static constexpr unsigned clear_site=0x184485;
static constexpr unsigned char clear_bytes_expected[]={0xF3,0xAA};
static bool guards_match() {
    for(const auto &g:guards) {
        unsigned long long h=0xcbf29ce484222325ull;
        for(unsigned i=0;i<g.size;++i)h=(h^image[g.rva+i])*0x100000001b3ull;
        if(h!=g.hash)return false;
    }
    // Independent confirmation of the debug-global RIP displacement, not its value.
    return !memcmp(image+clear_site,clear_bytes_expected,sizeof(clear_bytes_expected)) && image[0x183D68]==0x83 && 0x183D6F+*reinterpret_cast<int *>(image+0x183D6A)==debug_rva;
}
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)
using path_fast_core::Node;
using Path=Node *(__fastcall *)(Obj,unsigned,const int *,int,Node *,int,unsigned);
using Unary=int(__fastcall *)(Obj);
static Path original;
static CodeLink link,clear_link;
static bool active;
static volatile LONG skip_clear;
static thread_local bool busy;
// 2026-10-26 memory leak fix (user report: commit grew ~2.4GiB per 2 min at x8, 65.9GB crash).
// The original runs each force AI on a NEW thread, so a per-thread `new` (2.9MB) was never freed.
// One storage per native node-buffer slot instead: 1843E0 holds that slot's critical section
// (141BB3D08[slot]+10, slot<3) around its 183A30 calls, so a slot is never shared concurrently.
// At most three are ever allocated and they live for the process.
// AI-CONTRACT [lifetime]: Storage ownership is per native buffer slot, not per thread. Native force-AI threads are short-lived; thread_local heap ownership caused the v13.1/v13.2 leak. Preserve slot critical-section evidence before changing reuse.
static path_fast_core::Storage *volatile slot_storage[3];
static thread_local path_fast_core::Storage *storage;   // current call's slot storage (no ownership)
#ifndef NO_RUNTIME_LOG
static unsigned verify;
static LONGLONG frequency;
struct Diagnostic {
    unsigned start=0,goal=0,flags=0,ctx10=0,ctx28=0,ctx2c=0;
    int valid=0,force=0;
    Node *original=nullptr,*actual=nullptr;
    path_fast_core::Comparison difference;
};
struct Totals {
    int day=-1;
    unsigned fast=0,delegated=0,compared=0,mismatches=0,multi=0,lines=0,details=0,only_fast=0,all_compared=0;
    LONGLONG only_ticks=0;
};
static SRWLOCK day_lock=SRWLOCK_INIT;
static Totals totals;
static volatile LONG64 calls;
static LONGLONG ticks() {LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}
static unsigned microseconds(LONGLONG elapsed) {
    const double us=frequency>0?static_cast<double>(elapsed)*1000000.0/static_cast<double>(frequency):0;
    return us>=UINT32_MAX?UINT32_MAX:static_cast<unsigned>(us);
}
static void record(bool fast,bool compared,bool mismatch,bool dirty,std::uint64_t call,
                   LONGLONG elapsed=0,const Diagnostic *detail=nullptr,Node *buffer=nullptr) {
    AcquireSRWLockExclusive(&day_lock);
    const int today=game_day();
    if(today!=totals.day) {
        if(totals.day!=-1) {
            // Logger day= is the boundary day; counters belong to the preceding observed day.
            log_event("path_fast_day",totals.fast,totals.delegated,0);
            if(verify)log_event("path_fast_verify_day",totals.compared,totals.mismatches,totals.multi);
            log_event("path_fast_time_day",totals.only_fast,microseconds(totals.only_ticks),totals.all_compared);
        }
        totals={};totals.day=today;
    }
    if(fast)++totals.fast;else ++totals.delegated;
    if(fast && !compared) {++totals.only_fast;totals.only_ticks+=elapsed;}
    if(compared)++totals.all_compared;
    if(compared && !dirty)++totals.compared;
    if(mismatch && dirty)++totals.multi;
    if(mismatch && !dirty) {
        ++totals.mismatches;
        if(totals.lines<64 && detail) {
            ++totals.lines;log_event("path_fast_mismatch",static_cast<unsigned>(call),detail->difference.original_length,detail->difference.new_length);
        }
    }
    if(mismatch && detail && totals.details<16) {
        ++totals.details;
        path_fast_core::mismatch_detail(call,detail->start,detail->goal,detail->flags,detail->ctx10,detail->ctx28,detail->ctx2c,
            detail->valid,detail->force,detail->original,detail->actual,buffer,*storage,detail->difference,
            [](const char *name,auto a,auto b,auto c) {
                log_event(name,static_cast<unsigned>(a),static_cast<unsigned>(b),static_cast<unsigned>(c));
            },[](unsigned id,unsigned direction) {
                // Guarded 2A7720 is pure integer geometry; no terrain/aux callbacks for diagnostics.
                return fn<unsigned(__fastcall *)(unsigned,unsigned)>(0x2A7720)(id,direction);
            });
    }
    ReleaseSRWLockExclusive(&day_lock);
}
#endif
struct Native {
    Obj ctx,army=nullptr,a0=nullptr,a8=nullptr;
    unsigned start=0,goal=0,flags=0,local_b0=0,c4=0,c8=1;
    int force=0,valid=0;
    float scale=1.0f;
    Obj aux[2]={};
    explicit Native(Obj context):ctx(context) {}
    bool prepare(Node *nodes,unsigned from,unsigned to,unsigned bits) {
        start=from;goal=to;flags=bits;
        int x,y;
        fn<void(__fastcall *)(unsigned,int *,int *)>(0x2AEE80)(start,&x,&y);
        army=pointer(ctx,0);
        valid=fn<Unary>(0x32F500)(army);
        if(!valid) {
            Obj other=pointer(ctx,8);
            if(fn<Unary>(0x32F500)(other)) {
                other=pointer(ctx,8); // 183BB7 reload after the validity callback
                force=(*reinterpret_cast<int(__fastcall **)(Obj)>(*reinterpret_cast<Obj *>(other)+0x60))(other);
            }
        } else {
            force=fn<Unary>(0x22CC40)(pointer(ctx,0));
            army=pointer(ctx,0); // 183ACC reload, preserving callback side effects
            local_b0=army[0x10]==2;
            const unsigned first=army[0x1B],second=army[0x1C];
            a0=pointer(world(),0x77428+(first<0x15?first:0)*8);
            a8=pointer(world(),0x77428+(second<0x15?second:0)*8);
            if((force!=0x33 && static_cast<unsigned>(force)-0x2E>4) || fn<Unary>(0x232730)(army)) {
                army=pointer(ctx,0); // 183B42
                const unsigned officer=word(army,0x12);
                Obj person=pointer(world(),officer<0x1771?0x158+officer*8:0x73A50);
                const unsigned county=word(army,0x2A);
                Obj region=fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),word(person,0xE2));
                c4=county==static_cast<unsigned>(fn<Unary>(0x22D740)(region));
            }
            if(fn<int(__fastcall *)(Obj,int)>(0x2C3780)(pointer(ctx,0),0x66))c8=0;
        }
        if(dword(ctx,0x10))local_b0=1;
        Node *root=nullptr;
        return fn<int(__fastcall *)(unsigned,int,unsigned,unsigned,const unsigned *,int,Node *,Node **)>(0x18AFA0)
            (flags,force,local_b0,start,&goal,1,nodes,&root)!=0;
    }
    void begin() {
        aux[0]=fn<Obj(__fastcall *)(Obj *)>(0x18DD40)(aux);
        const int base=fn<Unary>(0xBE120)(pointer(world(),0x73870));
        const __m128 numerator=_mm_cvtepi32_ps(_mm_cvtsi32_si128(base));
        __m128 denominator=numerator;
        if((flags&5) && pointer(ctx,0)) {
            alignas(16) unsigned char object[32];
            fn<Obj(__fastcall *)(Obj)>(0x2211F0)(object);
            fn<void(__fastcall *)(Obj,int)>(0x2EF540)(object,2);
            army=pointer(ctx,0); // 183C8A reload after constructor/setter callbacks
            const unsigned id=army[0x1B];
            Obj terrain_type=pointer(world(),0x77428+(id<0x15?id:0)*8);
            const float speed=fn<float(__fastcall *)(Obj,Obj,int,int,Obj)>(0x2B0290)(army,terrain_type,0,0,object);
            // COMISS numerator,speed; CMOVBE selects numerator (including NaN).
            if(!_mm_comile_ss(numerator,_mm_set_ss(speed)))denominator=_mm_set_ss(speed);
            fn<void(__fastcall *)(Obj)>(0x221FC0)(object);
        }
        scale=_mm_cvtss_f32(_mm_div_ss(numerator,denominator));
    }
    unsigned neighbour(unsigned id,unsigned direction) {return fn<unsigned(__fastcall *)(unsigned,unsigned)>(0x2A7720)(id,direction);}
    Obj tile(unsigned id) {return pointer(world(),0xE270+(id<path_fast_core::cells?id:0)*8);}
    int terrain(Obj cell) {return fn<int(__fastcall *)(Obj,int,unsigned)>(0x2AE6F0)(cell,force,c8);}
    bool allowed(unsigned id,Obj cell,std::int32_t &cost) {
        if(!(flags&0xC)) {
            if(!fn<int(__fastcall *)(Obj,Obj,int,unsigned,unsigned,const unsigned *,int)>(0x18A080)
                (pointer(ctx,0),cell,force,flags&0x40,c4,&goal,1))return false;
            if(local_b0 && fn<int(__fastcall *)(int,unsigned)>(0x2CA280)(force,cell[0x14]))return false;
            Obj key=pointer(ctx,0x20);
            const unsigned id_key=key?dword(key,0):0x14000;
            Obj lists=pointer(image,0x27D10F0);
            if(lists && id_key<dword(image,0x27D1120))
                for(Obj item=pointer(lists,id_key*8);item;item=pointer(item,8))
                    if(pointer(item,0)==cell)return false;
            if(dword(ctx,0x28) && word(cell,0x16))return false;
            if(dword(ctx,0x2C) && cell[0x10]==7)cost=path_fast_core::add(cost,10000);
            if(!(flags&0x40)) {
                Obj building=fn<Obj(__fastcall *)(Obj)>(0x22FB10)(cell);
                if(fn<Unary>(0x32F500)(building) && !fn<Unary>(0x232650)(building) && !fn<Unary>(0x232BD0)(building)) {
                    Obj type=fn<Obj(__fastcall *)(Obj)>(0xEE90)(building);
                    if(type[0x2C]==2 || type[0x2C]==3)return false;
                }
            }
        } else if(!fn<int(__fastcall *)(unsigned,int)>(0x2C6850)(id,force))return false;
        if((flags&0x10) && id!=goal && fn<int(__fastcall *)(unsigned)>(0x2C6950)(id))return false;
        if((flags&0x21)==1 && valid && fn<int(__fastcall *)(unsigned)>(0x2C6950)(id))
            cost=path_fast_core::add(cost,fn<int(__fastcall *)(Obj,Obj)>(0x1879C0)(cell,pointer(ctx,0)));
        return true;
    }
    int adjust(int cost,Obj cell) {
        return fn<int(__fastcall *)(int,unsigned,Obj,Obj,Obj,Obj,Obj *)>(0x1864F0)
            (cost,flags,cell,pointer(ctx,0),a0,a8,aux);
    }
    void coordinate(unsigned id) {int x,y;fn<void(__fastcall *)(unsigned,int *,int *)>(0x2AEE80)(id,&x,&y);}
    int heuristic(unsigned id) {
        const int distance=fn<int(__fastcall *)(unsigned,unsigned)>(0x2AA850)(id,start);
        return _mm_cvttss_si32(_mm_mul_ss(_mm_cvtepi32_ps(_mm_cvtsi32_si128(distance)),_mm_set_ss(scale)));
    }
    void success() {*reinterpret_cast<unsigned *>(image+debug_rva)=0;}
    void cleanup() {
        Obj root=aux[0],item=pointer(root,8);
        while(!item[0x19]) {
            fn<void(__fastcall *)(Obj *,Obj)>(0x3E2610)(aux,pointer(item,0x10));
            Obj next=pointer(item,0);
            fn<void(__fastcall *)(Obj)>(0x3F6C50)(item);item=next;
        }
        root=aux[0];*reinterpret_cast<Obj *>(root+8)=root;
        *reinterpret_cast<Obj *>(root)=root;*reinterpret_cast<Obj *>(root+0x10)=root;aux[1]=nullptr;
        fn<void(__fastcall *)(Obj,int,unsigned)>(0x4DE80)(root,1,0x30);
    }
};
static int slot_of(const Node *buffer) {
    for(unsigned slot=0;slot<3;++slot)
        if(reinterpret_cast<const unsigned char *>(buffer)==image+0x1BB7B00+slot*path_fast_core::node_bytes)return static_cast<int>(slot);
    return -1;
}
static bool known_slot(const Node *buffer) {return slot_of(buffer)>=0;}
static bool acquire_storage(const Node *buffer) {
    const int slot=slot_of(buffer);
    if(slot<0) {storage=nullptr;return false;}   // unknown buffer: no lock guarantee, use the original
    path_fast_core::Storage *s=slot_storage[slot];
    if(!s) {
        try {s=new path_fast_core::Storage;}catch(...) {storage=nullptr;return false;}
        auto *previous=static_cast<path_fast_core::Storage *>(InterlockedCompareExchangePointer(
            reinterpret_cast<PVOID volatile *>(&slot_storage[slot]),s,nullptr));
        if(previous) {delete s;s=previous;}
    }
    storage=s;return true;
}
static Node *delegate(Obj ctx,unsigned start,const int *goals,int count,Node *buffer,int clear_bytes,unsigned flags) {
    // The only direct caller supplies these static slots. Unknown buffers retain
    // the native clear_bytes contract; never assume an arbitrary pointer owns 1.9MB.
    path_fast_core::clear_delegate(buffer,known_slot(buffer),skip_clear!=0);
    return original(ctx,start,goals,count,buffer,clear_bytes,flags);
}
static Node *path_body(Obj ctx,unsigned start,const int *goals,int count,Node *buffer,int clear_bytes,unsigned flags) {
    const DWORD incoming_error=GetLastError();
#ifndef NO_RUNTIME_LOG
    const auto call=static_cast<std::uint64_t>(InterlockedIncrement64(&calls));
#endif
    if(count!=1 || !buffer || clear_bytes!=0xBD10 || !ctx || !goals || !path_fast_core::eligible(count,buffer,clear_bytes,dword(image,debug_rva),
            start,goals?static_cast<unsigned>(*goals):path_fast_core::cells,busy) || !acquire_storage(buffer)) {
        SetLastError(incoming_error);
        Node *output=delegate(ctx,start,goals,count,buffer,clear_bytes,flags);
        const DWORD error=GetLastError();
#ifndef NO_RUNTIME_LOG
        record(false,false,false,false,call);
#endif
        SetLastError(error);return output;
    }
    busy=true;
    Node *output=nullptr;
#ifndef NO_RUNTIME_LOG
    bool compared=false,mismatch=false,dirty=false;
    Diagnostic detail;
    LONGLONG elapsed=0;
#endif
    __try {
#ifndef NO_RUNTIME_LOG
        if(path_fast_core::verify_call(verify,call)) {
            detail.start=start;detail.goal=static_cast<unsigned>(*goals);detail.flags=flags;
            detail.ctx10=dword(ctx,0x10);detail.ctx28=dword(ctx,0x28);detail.ctx2c=dword(ctx,0x2C);
            if(verify==1)dirty=!path_fast_core::clean(buffer);
            else {
                // Sampled comparison shares the new clean-per-segment meaning:
                // clear ALL nodes before the original, even on later waypoints.
                // Thus every sampled segment is an exact comparison, never "multi".
                memset(buffer,0,path_fast_core::node_bytes);
            }
            SetLastError(incoming_error);
            output=original(ctx,start,goals,count,buffer,clear_bytes,flags);
            const DWORD native_error=GetLastError();
            Native native(ctx);
            Node *actual=path_fast_core::search(*storage,start,detail.goal,flags,native);
            detail.original=output;detail.actual=actual;detail.valid=native.valid;detail.force=native.force;
            detail.difference=path_fast_core::compare(output,actual);
            mismatch=detail.difference.mismatch;compared=true;
            // N=1 always returns the original; samples publish new chains only on equality.
            output=path_fast_core::verified_result(verify,mismatch,output,actual,buffer);
            SetLastError(native_error);
        } else
#endif
        {
#ifndef NO_RUNTIME_LOG
            const LONGLONG begin=ticks();
#endif
            SetLastError(incoming_error);
            Native native(ctx);
            Node *result=path_fast_core::search(*storage,start,static_cast<unsigned>(*goals),flags,native);
            output=path_fast_core::materialize(buffer,result);
#ifndef NO_RUNTIME_LOG
            const DWORD result_error=GetLastError();
            elapsed=ticks()-begin;
            SetLastError(result_error);
#endif
        }
#ifndef NO_RUNTIME_LOG
        const DWORD result_error=GetLastError();
        record(true,compared,mismatch,dirty,call,elapsed,&detail,buffer);
        SetLastError(result_error);
#endif
    } __finally {busy=false;}
    const DWORD error=GetLastError();
    SetLastError(error);return output;
}
static Node *__fastcall path(Obj ctx,unsigned start,const int *goals,int count,Node *buffer,int clear_bytes,unsigned flags) {
    const DWORD error=GetLastError();
    COST_SCOPE("path_fast");
    SetLastError(error);return path_body(ctx,start,goals,count,buffer,clear_bytes,flags);
}
static bool prepare(Obj relay) {
    if(!guards_match())return false;
    absolute_jump(relay,reinterpret_cast<void *>(&path));
    // 183A30: MOV RAX,RSP / MOV [RAX+20h],R9D; same gate as path_count.
    memcpy(relay+32,image+entry,7);
    relay[39]=0x49;relay[40]=0xBB;Obj continuation=image+entry+7;memcpy(relay+41,&continuation,8);
    relay[49]=0x41;relay[50]=0xFF;relay[51]=0xE3;
    original=reinterpret_cast<Path>(relay+32);
    auto runtime=reinterpret_cast<RUNTIME_FUNCTION *>(relay+64);
    runtime->BeginAddress=32;runtime->EndAddress=52;runtime->UnwindData=80;
    const unsigned char unwind[]={1,7,0,0};memcpy(relay+80,unwind,sizeof(unwind));
    if(!prepare_link(link,entry,0xE9,relay))return false;
    memset(link.modified+5,0x90,2);
    // REP affects only RCX/RDI and memory. Both registers and RAX are killed
    // before their next read; TEST R12,R12 replaces flags at 18448E.
    const unsigned char nop[]={0x90,0x90};
    return prepare_bytes(clear_link,0x184485,nop,sizeof(nop));
}
static bool install() {
#ifndef NO_RUNTIME_LOG
    const int interval=dev_group()?mod_setting(L"diagnostics",L"path_fast_verify",0):0;
    verify=interval>0?static_cast<unsigned>(interval):0;
    LARGE_INTEGER q;if(!QueryPerformanceFrequency(&q) || q.QuadPart<=0)return false;frequency=q.QuadPart;
#endif
    Obj relay=allocate_near(image+entry);if(!relay)return false;
    DWORD old;
    if(!prepare(relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,96)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    if(!RtlAddFunctionTable(reinterpret_cast<RUNTIME_FUNCTION *>(relay+64),1,reinterpret_cast<DWORD64>(relay))) {
        VirtualFree(relay,0,MEM_RELEASE);return false;
    }
    if(!publish_link(link)) {restore_link(link);return false;}
#ifndef NO_RUNTIME_LOG
    if(path_fast_core::elide_clear(verify))
#endif
    {
        InterlockedExchange(&skip_clear,1);
        if(!publish_link(clear_link)) {
            restore_link(clear_link);InterlockedExchange(&skip_clear,0);restore_link(link);return false;
        }
    }
    active=true;log_event("path_fast_installed",skip_clear?1:0);return true;
}
#endif
}
static bool path_fast_guards_match() {return path_fast::guards_match();}
