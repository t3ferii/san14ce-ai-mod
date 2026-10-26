// Runs the actual adapter and copied native filter instructions on private
// fixture pages. No attachment, writes or native calls in a running game.
#define TARGET_RETREAT_FIXTURE
#define TACTICS_FIXTURE
#define main baseline_native_main
#include "native_fixture.cpp"
#undef main
static bool enemy_invalid,own_safety_retreat,throw_daily;
static unsigned daily_calls;
static int __fastcall target_invalid_mock(Obj) {return enemy_invalid?1:0;}
static void __fastcall daily_mock(Obj army) {
    ++daily_calls;
    army[0x26]^=0x40; // Original changes unrelated flags: must survive wrapper.
    if(throw_daily)RaiseException(0xE0140091,0,0,nullptr);
    Obj enemy=table(0x7E838,word(army,0x3A),500);
    if(own_safety_retreat || ((army[0x26]&8)==0 && enemy &&
       (enemy[0x37]==11 || enemy[0x37]==12)))army[0x37]=11;
}
static void verify_daily_exception(Obj army) {
    bool caught=false;
    __try {target_retreat_daily(army);}
    __except(GetExceptionCode()==0xE0140091?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH) {caught=true;}
    check(caught,"exception forwarded");
}
static unsigned nesting;
static void __fastcall nested_daily_mock(Obj army) {
    check((army[0x26]&8)!=0,"nested caller sees scoped bit");
    if(!nesting) {++nesting;target_retreat_daily(army);--nesting;}
    check((army[0x26]&8)!=0,"inner restoration preserves outer scope");
}
static void reset_armies(Obj army,Obj enemy) {
    memset(army+0x12,0,0x13E);memset(enemy+0x12,0,0x13E);
    army[0x150]=2;enemy[0x150]=3;
    army[0x37]=7;army[0x38]=27;put16(army,0x3A,2);army[0x3C]=0xFF;
    enemy[0x37]=11;put16(army,0x16,3500);put16(enemy,0x16,3000);
    put16(army,0x2A,100);put16(enemy,0x2A,101);
    enemy_invalid=false;own_safety_retreat=false;throw_daily=false;
}
using Filter=unsigned(__fastcall *)(Obj,unsigned);
static unsigned make_filter(Obj memory,unsigned rva,bool patched) {
    // R14 is restored; EDX models the downstream actual-home-region result.
    // Copy original MOVZX/LEA/CMP/JBE and SUB/CMP/JA machine instructions,
    // relocating only their branch targets to this private test function.
    const unsigned char entry[]={0x41,0x56,0x49,0x89,0xCE};
    memcpy(memory,entry,sizeof(entry));
    memcpy(memory+5,image+rva,16); // end of original near JBE
    memcpy(memory+21,image+rva+16,12); // SUB CL,2; CMP CL,1; near JA
    if(patched) {memory[10]=0xB0;memory[11]=2;memory[12]=0x90;}
    const unsigned char tail[]={0x85,0xD2,0x75,0x0A, // at actual home -> reject
        0xB8,1,0,0,0,0x41,0x5E,0xC3, // eligible
        0x90,0x90,0x31,0xC0,0x41,0x5E,0xC3};
    memcpy(memory+33,tail,sizeof(tail));
    int32_t reject=47-21,eligible=37-33;
    memcpy(memory+17,&reject,4);memcpy(memory+29,&eligible,4);
    return 33+sizeof(tail);
}
int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    setup();BIND(0x233440,target_invalid_mock);
    Obj army=object(1),enemy=object(2);slot(0x7E838,1,army);slot(0x7E838,2,enemy);
    army_ids[army]=1;army_ids[enemy]=2;reset_armies(army,enemy);
    original_target_retreat_daily=daily_mock;target_retreat_active=1;
    for(unsigned action=0;action<256;++action) {
        reset_armies(army,enemy);enemy[0x37]=static_cast<unsigned char>(action);
        check(target_retreat_nearby_enemy(army)==(action==11 || action==12),"only enemy retreat commands trigger nearby protection");
        target_retreat_daily(army);
        check(army[0x37]==7 && army[0x26]==0x40,"command alone keeps attack and restores scoped flag");
    }
    reset_armies(army,enemy);army[0x150]=1;check(!target_retreat_nearby_enemy(army),"player directly controlled excluded");
    reset_armies(army,enemy);army[0x23]=1;check(!target_retreat_nearby_enemy(army),"native cancellation special state preserved");
    reset_armies(army,enemy);army[0x25]=1;check(!target_retreat_nearby_enemy(army),"native secondary special state preserved");
    reset_armies(army,enemy);army[0x37]=11;target_retreat_daily(army);check(army[0x37]==11,"existing own retreat is not undone");
    reset_armies(army,enemy);put16(army,0x3A,501);check(!target_retreat_nearby_enemy(army),"invalid target ID excluded");
    reset_armies(army,enemy);army[0x140]=1;check(!target_retreat_nearby_enemy(army),"direct pending order excluded");
    reset_armies(army,enemy);enemy_invalid=true;check(!target_retreat_nearby_enemy(army),"native invalid enemy excluded");
    reset_armies(army,enemy);put16(enemy,0x16,0);check(!target_retreat_nearby_enemy(army),"destroyed target excluded");
    reset_armies(army,enemy);put16(enemy,0x10,0);check(!target_retreat_nearby_enemy(army),"inactive target excluded");put16(enemy,0x10,2);
    reset_armies(army,enemy);enemy[0x150]=2;check(!target_retreat_nearby_enemy(army),"nonhostile target excluded");
    for(unsigned dist=0;dist<=12;++dist) {
        reset_armies(army,enemy);put16(enemy,0x2A,100+dist);
        check(target_retreat_nearby_enemy(army)==(dist<=6),"bounded proximity");
    }
    for(unsigned count:{0u,999u,1000u,65535u}) {
        reset_armies(army,enemy);put16(army,0x16,count);
        check(target_retreat_nearby_enemy(army)==(count>=1000),"depleted return not blocked");
    }
    reset_armies(army,enemy);put16(army,0x2A,48400);check(!target_retreat_nearby_enemy(army),"unknown own hex excluded");
    reset_armies(army,enemy);put16(enemy,0x2A,48400);check(!target_retreat_nearby_enemy(army),"unknown enemy hex excluded");
    reset_armies(army,enemy);army[0x3C]=27;put16(army,0x3E,3);check(!target_retreat_nearby_enemy(army),"unrelated secondary validity preserved");
    put16(army,0x3E,2);check(target_retreat_nearby_enemy(army),"same secondary target protected");
    for(unsigned flags=0;flags<256;++flags) {
        reset_armies(army,enemy);army[0x26]=static_cast<unsigned char>(flags);target_retreat_daily(army);
        check(army[0x26]==(flags^0x40),"every original flag value restored without erasing native changes");
    }
    reset_armies(army,enemy);own_safety_retreat=true;target_retreat_daily(army);
    check(army[0x37]==11 && army[0x26]==0x40,"own safety retreat takes precedence");
    reset_armies(army,enemy);throw_daily=true;verify_daily_exception(army);
    check(army[0x26]==0x40,"SEH restores scoped bit and preserves native flags");
    reset_armies(army,enemy);original_target_retreat_daily=nested_daily_mock;target_retreat_daily(army);
    check(army[0x26]==0,"nested wrapper restores original outer bit");original_target_retreat_daily=daily_mock;
    reset_armies(army,enemy);target_retreat_active=0;target_retreat_daily(army);
    check(army[0x37]==11 && army[0x26]==0x40,"inactive wrapper forwards original behavior");
    for(const auto &guard:target_retreat_guards)memcpy(image+guard.rva,guard.bytes,guard.size);
    check(target_retreat_guards_match(),"full live-matched signatures accepted");
    Obj relay=static_cast<Obj>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));check(relay!=nullptr,"relay allocated");
    TargetRetreatCode code;check(prepare_target_retreat(code,relay),"three atomic links prepared");
    for(const auto &guard:target_retreat_guards) {
        for(unsigned i=0;i<guard.size;++i) {
            image[guard.rva+i]^=1;check(!target_retreat_guards_match(),"every guarded byte mismatch rejects");image[guard.rva+i]^=1;
        }
    }
    Obj filters=static_cast<Obj>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));check(filters!=nullptr,"private filters allocated");
    const unsigned sites[]={0x6A265,0x6A943};
    for(unsigned i=0;i<2;++i)for(unsigned mode=0;mode<2;++mode)make_filter(filters+i*128+mode*64,sites[i],mode!=0);
    DWORD old;check(VirtualProtect(filters,4096,PAGE_EXECUTE_READ,&old)!=0,"filters executable");
    check(FlushInstructionCache(GetCurrentProcess(),filters,256)!=0,"filters flushed");
    for(unsigned i=0;i<2;++i)for(unsigned action=0;action<256;++action)for(unsigned home=0;home<2;++home) {
        enemy[0x37]=static_cast<unsigned char>(action);
        auto original=reinterpret_cast<Filter>(filters+i*128),patched=reinterpret_cast<Filter>(filters+i*128+64);
        unsigned before=original(enemy,home),after=patched(enemy,home);
        check(before==((action==11 || action==12 || ((action==2 || action==3) && home))?0u:1u),"actual original command filter");
        check(after==(((action==2 || action==3) && home)?0u:1u),"patched machine code preserves actual home exclusion");
    }
    for(unsigned failure=0;failure<3;++failure) {
        CodeLink *link=failure<2?&code.filters[failure]:&code.daily;
        link->original[15]^=1;check(!publish_target_retreat(code),"publication failure rejected");link->original[15]^=1;
        check(!target_retreat_active,"failure leaves wrapper inactive");
        for(const CodeLink *entry:{&code.daily,&code.filters[0],&code.filters[1]})
            check(!memcmp(image+entry->block,entry->original,16),"every published prefix restored");
    }
    // Finish the guarded native prolog on our private image, then unwind it.
    const unsigned char daily_tail[]={0xFE,0x41,0x1F,0x48,0x81,0xC4,0xE0,9,0,0,0x5D,0xC3};
    memcpy(image+0x2721A2,daily_tail,sizeof(daily_tail));
    check(VirtualProtect(image,0x2800000,PAGE_EXECUTE_READ,&old)!=0,"fixture image executable");
    check(FlushInstructionCache(GetCurrentProcess(),image+0x272190,64)!=0,"private native continuation flushed");
    check(VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old)!=0,"relay executable");
    auto runtime=reinterpret_cast<RUNTIME_FUNCTION *>(relay+96);
    check(RtlAddFunctionTable(runtime,1,reinterpret_cast<DWORD64>(relay))!=0,"gate unwind registered");
    for(unsigned offset:{75u,85u}) {
        DWORD64 stack[]={0x123456,0xABCDEF};CONTEXT context={};context.ContextFlags=CONTEXT_FULL;
        context.Rip=reinterpret_cast<DWORD64>(relay+offset);context.Rsp=reinterpret_cast<DWORD64>(stack);context.Rbp=0x998877;
        DWORD64 base=0,establisher=0;void *handler=nullptr;
        check(RtlLookupFunctionEntry(context.Rip,&base,nullptr)==runtime,"unwind lookup");
        RtlVirtualUnwind(0,base,context.Rip,runtime,&context,&handler,&establisher,nullptr);
        check(context.Rbp==stack[0] && context.Rip==stack[1] && context.Rsp==reinterpret_cast<DWORD64>(stack+2),"partial prolog restores caller RBP and return");
    }
    check(publish_target_retreat(code),"all three atomic code links published");
    reset_armies(army,enemy);auto daily=reinterpret_cast<TargetRetreatDaily>(image+0x272190);
    daily(army);check(army[0x1F]==1 && army[0x26]==0,"native gate preserves argument and stack");
    restore_target_retreat(code);check(!target_retreat_active,"rollback disabled");
    for(const CodeLink *entry:{&code.daily,&code.filters[0],&code.filters[1]})check(!memcmp(image+entry->block,entry->original,16),"complete native bytes restored");
    printf("{\"status\":\"target_retreat_fixture_passed\",\"checks\":%u,\"native_filter_cases\":1024,\"publication_failure_prefixes\":3,\"game_attached\":false}\n",checks);
    return 0;
}
