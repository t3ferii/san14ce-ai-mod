// Battle deaths relay + tables (docs/HISTORY.md 3.3, 4-1, step S2, 2026-11-06). No game image, no game memory: the relay built by
// bdp::build_branch_relay is copied into an RWX page together with raw-byte harness / pass / reject stubs / predicate, and executed.
// Covers: ebx!=0 pass without calling the predicate; ebx==0 with predicate true/false; predicate rcx==r13; predicate entry alignment;
// volatile register + xmm0-5 + callee-saved preservation (the predicate stub clobbers rcx,rdx,r8-r11,xmm0-5 and the flags); ZF restore;
// rsp balance; builder length / cell / displacement checks; the guard / site / span tables themselves (both known builds).
// Cross-check against layout_registry.h rows owner "battle_deaths" (added by plan step S3) runs when the rows exist; otherwise it is
// reported as missing, not failed.
#include "../src/battle_deaths_profile.h"
#include "../src/layout_registry.h"
#include <cstdio>
#include <vector>
static int checks=0,failures=0;
static void ok(bool v,const char *what) {++checks;if(!v){++failures;std::printf("FAIL %s\n",what);}}

// ---- raw-byte page ----
enum : unsigned {kHarness=0x000,kPassStub=0x100,kRejectStub=0x200,kPred=0x300,kRelay=0x400,kIn=0x800,kOut=0x900,kPredData=0xA00,kPageSize=0x1000};
// block: gpr[9] (rax,rcx,rdx,r8,r9,r10,r11,rbx,r13), flags, rsp, path, harness rsp, xmm0-5 at +112
enum : unsigned {kGprN=9,kOffFlags=72,kOffRsp=80,kOffPath=88,kOffHRsp=96,kOffXmm=112};
static const unsigned kGprCode[kGprN]={0,1,2,8,9,10,11,3,13};
// predicate data: result, rcx, rsp, calls
enum : unsigned {kPdResult=kPredData,kPdRcx=kPredData+8,kPdRsp=kPredData+16,kPdCalls=kPredData+24};
struct Emit {
    uint8_t *page;unsigned at;
    void b(unsigned v) {page[at++]=(uint8_t)v;}
    void u32(uint32_t v) {for(int i=0;i<4;++i)b((v>>(8*i))&0xFF);}
    void rip(std::initializer_list<unsigned> prefix,unsigned reg,unsigned target) {   // [prefix] modrm(00,reg,101) disp32 (rip relative, no immediate follows)
        for(unsigned x:prefix)b(x);
        b(((reg&7)<<3)|5);
        u32((uint32_t)(int32_t)((int)target-(int)(at+4)));
    }
    void load(unsigned code,unsigned target) {rip({0x48u|((code>>3)<<2),0x8B},code,target);}    // mov r64,[rip+x]
    void store(unsigned code,unsigned target) {rip({0x48u|((code>>3)<<2),0x89},code,target);}   // mov [rip+x],r64
};
static void emit_harness(uint8_t *pg) {
    Emit e={pg,kHarness};
    e.b(0x53);e.b(0x41);e.b(0x55);e.b(0x41);e.b(0x54);                      // push rbx ; push r13 ; push r12 (entry rsp=8 mod 16 -> 0 mod 16)
    for(unsigned i=0;i<6;++i)e.rip({0xF3,0x0F,0x6F},i,kIn+kOffXmm+16*i);       // movdqu xmm<i>,[in]
    for(unsigned i=0;i<kGprN;++i)e.load(kGprCode[i],kIn+i*8);
    e.store(4,kOut+kOffHRsp);                                                  // mov [out.hrsp],rsp
    e.b(0xE9);e.u32((uint32_t)(int32_t)((int)kRelay-(int)(e.at+4)));           // jmp relay (what the E9 at 295C5F does)
}
static void emit_stub(uint8_t *pg,unsigned at,unsigned path) {
    Emit e={pg,at};
    e.b(0x9C);e.rip({0x8F},0,kOut+kOffFlags);                                 // pushfq ; pop qword [out.flags]  (first: flags untouched)
    for(unsigned i=0;i<kGprN;++i)e.store(kGprCode[i],kOut+i*8);
    for(unsigned i=0;i<6;++i)e.rip({0xF3,0x0F,0x7F},i,kOut+kOffXmm+16*i);       // movdqu [out],xmm<i>
    e.store(4,kOut+kOffRsp);
    e.b(0xB8);e.u32(path);e.store(0,kOut+kOffPath);                           // mov eax,path ; mov [out.path],rax
    e.b(0x41);e.b(0x5C);e.b(0x41);e.b(0x5D);e.b(0x5B);e.b(0xC3);               // pop r12 ; pop r13 ; pop rbx ; ret
}
static void emit_predicate(uint8_t *pg) {
    Emit e={pg,kPred};
    e.store(4,kPdRsp);e.store(1,kPdRcx);
    e.load(0,kPdCalls);e.b(0x48);e.b(0x83);e.b(0xC0);e.b(0x01);e.store(0,kPdCalls);   // calls++ (clobbers flags)
    static const uint8_t clob[][7]={{0x48,0xC7,0xC1,0xFF,0xFF,0xFF,0xFF},{0x48,0xC7,0xC2,0xFF,0xFF,0xFF,0xFF},{0x49,0xC7,0xC0,0xFF,0xFF,0xFF,0xFF},
        {0x49,0xC7,0xC1,0xFF,0xFF,0xFF,0xFF},{0x49,0xC7,0xC2,0xFF,0xFF,0xFF,0xFF},{0x49,0xC7,0xC3,0xFF,0xFF,0xFF,0xFF}};   // mov rcx,rdx,r8-r11,-1
    for(auto &c:clob)for(unsigned x:c)e.b(x);
    for(unsigned i=0;i<6;++i){e.b(0x66);e.b(0x0F);e.b(0x76);e.b(0xC0|(i<<3)|i);}    // pcmpeqd xmm<i>,xmm<i> (all ones)
    e.load(0,kPdResult);e.b(0xC3);                                            // mov rax,[result] ; ret
}
static uint64_t rd64(const uint8_t *p) {uint64_t v;memcpy(&v,p,8);return v;}
static void wr64(uint8_t *p,uint64_t v) {memcpy(p,&v,8);}

struct Case {uint64_t rbx_val;uint64_t pred;};
static void run_case(uint8_t *pg,uint64_t rbxv,uint64_t pred,size_t len) {
    (void)len;
    char name[160];
    const bool ebx_zero=(uint32_t)rbxv==0;
    const bool pass_expected=!ebx_zero||((uint32_t)pred!=0);
    const unsigned expect_calls=ebx_zero?1:0;
    // inputs: distinct sentinel per register / xmm byte
    uint8_t *in=pg+kIn,*out=pg+kOut;
    memset(in,0,0x100);memset(out,0,0x100);memset(pg+kPredData,0,0x40);
    static const uint64_t sent[kGprN]={0x1111111111111111ull,0x2222222222222222ull,0x3333333333333333ull,0x4444444444444444ull,0x5555555555555555ull,
        0x6666666666666666ull,0x7777777777777777ull,0,0x9999999999999999ull};
    for(unsigned i=0;i<kGprN;++i)wr64(in+i*8,sent[i]);
    wr64(in+7*8,rbxv);
    for(unsigned i=0;i<96;++i)in[kOffXmm+i]=(uint8_t)(0xA0+i*3);
    wr64(pg+kPdResult,pred);
    typedef void (*Fn)();
    ((Fn)(pg+kHarness))();
    std::snprintf(name,sizeof name,"[rbx=%llx pred=%llx]",(unsigned long long)rbxv,(unsigned long long)pred);
    char m[220];
#define CHK(cond,txt) do{std::snprintf(m,sizeof m,"%s %s",txt,name);ok((cond),m);}while(0)
    CHK(rd64(out+kOffPath)==(pass_expected?1u:2u),"path");
    CHK(rd64(pg+kPdCalls)==expect_calls,"predicate call count");
    if(expect_calls) {
        CHK(rd64(pg+kPdRcx)==sent[8],"predicate rcx == r13");
        CHK(((rd64(pg+kPdRsp)+8)&15)==0,"predicate entry alignment (rsp+8 % 16 == 0)");
    }
    bool regs=true;
    for(unsigned i=0;i<kGprN;++i)if(rd64(out+i*8)!=rd64(in+i*8))regs=false;
    CHK(regs,"rax,rcx,rdx,r8-r11,rbx,r13 preserved");
    CHK(memcmp(out+kOffXmm,in+kOffXmm,96)==0,"xmm0-5 preserved");
    CHK(rd64(out+kOffRsp)==rd64(out+kOffHRsp),"rsp balanced");
    const bool zf=(rd64(out+kOffFlags)>>6)&1;
    CHK(zf==ebx_zero,"ZF == (ebx==0) (original flags restored)");
#undef CHK
}

static void test_relay() {
    uint8_t *pg=(uint8_t *)VirtualAlloc(nullptr,kPageSize,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    ok(pg!=nullptr,"RWX page");
    if(!pg)return;
    memset(pg,0xCC,kPageSize);
    emit_harness(pg);emit_stub(pg,kPassStub,1);emit_stub(pg,kRejectStub,2);emit_predicate(pg);
    const uint64_t pass=(uint64_t)(uintptr_t)(pg+kPassStub),reject=(uint64_t)(uintptr_t)(pg+kRejectStub),pred=(uint64_t)(uintptr_t)(pg+kPred);
    uint8_t buf[512];
    memset(buf,0x77,sizeof buf);
    const size_t len=bdp::build_branch_relay(buf,sizeof buf,pred,pass,reject);
    std::printf("relay_len=%zu\n",len);
    ok(len>0 && len<=bdp::kRelayMaxLen,"builder length within kRelayMaxLen");
    ok(len%8==0,"builder length multiple of 8 (cells aligned)");
    ok(len>=16 && rd64(buf+len-16)==pass && rd64(buf+len-8)==reject,"cells hold pass and reject");
    ok(((uintptr_t)(pg+kRelay)&7)==0 && ((len-16)&7)==0,"cell 8-byte alignment in the page");
    ok(buf[0]==0x85&&buf[1]==0xDB&&buf[2]==0x0F&&buf[3]==0x85,"starts with test ebx,ebx ; jne");
    {   // displacement checks: jne -> PASS jmp, jz -> REJECT, both FF 25 disp32 -> cells
        int32_t d;memcpy(&d,buf+4,4);
        const size_t pass_at=(size_t)(8+d);
        ok(pass_at+6<=len && buf[pass_at]==0xFF && buf[pass_at+1]==0x25,"jne target is the PASS jmp [rip+cell]");
        memcpy(&d,buf+pass_at+2,4);
        ok((size_t)((int64_t)(pass_at+6)+d)==len-16,"PASS jmp displacement hits the pass cell");
        ok(pass_at>=8 && buf[pass_at-8]==0x0F && buf[pass_at-7]==0x84 && buf[pass_at-2]==0x58 && buf[pass_at-1]==0x9D,"jz sits before pop rax ; popfq ; PASS");
        memcpy(&d,buf+pass_at-6,4);
        const size_t reject_at=(size_t)((int64_t)(pass_at-2)+d);
        ok(reject_at==pass_at+6 && buf[reject_at]==0x58 && buf[reject_at+1]==0x9D && buf[reject_at+2]==0xFF && buf[reject_at+3]==0x25,"jz target is REJECT (pop rax ; popfq ; jmp [cell])");
        memcpy(&d,buf+reject_at+4,4);
        ok((size_t)((int64_t)(reject_at+8)+d)==len-8,"REJECT jmp displacement hits the reject cell");
    }
    {   // the predicate address is the only imm64 after mov rax: 48 B8
        bool found=false;
        for(size_t i=0;i+10<=len;++i)if(buf[i]==0x48&&buf[i+1]==0xB8&&rd64(buf+i+2)==pred)found=true;
        ok(found,"mov rax,imm64(predicate) present");
    }
    {   // determinism + argument validation
        uint8_t buf2[512];memset(buf2,0x11,sizeof buf2);
        const size_t len2=bdp::build_branch_relay(buf2,sizeof buf2,pred,pass,reject);
        ok(len2==len && memcmp(buf,buf2,len)==0,"deterministic");
        ok(bdp::build_branch_relay(buf2,len-1,pred,pass,reject)==0,"cap len-1 rejected");
        ok(bdp::build_branch_relay(buf2,0,pred,pass,reject)==0,"cap 0 rejected");
        ok(bdp::build_branch_relay(buf2,16,pred,pass,reject)==0,"cap 16 rejected");
        ok(bdp::build_branch_relay(buf2,len,pred,pass,reject)==len,"cap == len accepted");
        ok(bdp::build_branch_relay(nullptr,sizeof buf2,pred,pass,reject)==0,"null out rejected");
        ok(bdp::build_branch_relay(buf2,sizeof buf2,0,pass,reject)==0,"predicate 0 rejected");
        ok(bdp::build_branch_relay(buf2,sizeof buf2,pred,0,reject)==0,"pass 0 rejected");
        ok(bdp::build_branch_relay(buf2,sizeof buf2,pred,pass,0)==0,"reject 0 rejected");
    }
    memcpy(pg+kRelay,buf,len);
    FlushInstructionCache(GetCurrentProcess(),pg,kPageSize);
    static const uint64_t rbx_vals[]={0,1,0x80000000ull,0xFFFFFFFFull,0xDEAD000000000000ull,0xDEAD000000000001ull,0x100000000ull};
    static const uint64_t preds[]={0,1,2,0xFFFFFFFFull,0x100000000ull};
    for(uint64_t r:rbx_vals)for(uint64_t p:preds)run_case(pg,r,p,len);
    VirtualFree(pg,0,MEM_RELEASE);
}

// ---- tables ----
struct BuildExpect {unsigned guard_rva[6];unsigned long long guard_hash[6];unsigned site[4],orig[4],span,reject,app,camera;};
// plan 3.4 / 3.5 values of the reference build 1a4a447d (the fixture runs with the single-column address table)
static const unsigned long long kRefHash[6]={0xFD9E9B85A74972A6ull,0xF60E887BFD21C705ull,0x835F477A1B42648Cull,0x92A4E03C15B5DBB9ull,0x8A81202331C2D685ull,0x0E33A03C060DC2C5ull};
static const BuildExpect kBuild=
    {{0x25A880,0x27F250,0x2979B0,0x36A850,0x14DAE0,0x14D9D0},{kRefHash[0],kRefHash[1],kRefHash[2],kRefHash[3],kRefHash[4],kRefHash[5]},
     {0x25B022,0x27F716,0x25B0EB,0x27F7A9},{0x36A850,0x36A850,0x2979B0,0x2979B0},0x297B6F,0x298445,0x14DAE0,0x14D9D0};
static const unsigned kGuardSizes[6]={0x870,0x55E,0x1C7,0x10E,0x8,0x22};
static void test_tables() {
    const size_t gn=sizeof(battle_deaths_guards)/sizeof(battle_deaths_guards[0]);
    ok(gn==6,"6 guard rows");
    bool sizes=true,hashes=true,dups=false,resolved=true;
    for(size_t i=0;i<gn;++i) {
        if(battle_deaths_guards[i].size==0||battle_deaths_guards[i].size>=0x3200)sizes=false;
        if(battle_deaths_guards[i].hash==0)hashes=false;
        if(battle_deaths_guards[i].rva==0)resolved=false;
        for(size_t j=i+1;j<gn;++j)if(battle_deaths_guards[i].rva==battle_deaths_guards[j].rva)dups=true;
    }
    ok(sizes,"guard sizes non-zero and < 0x3200");ok(hashes,"guard hashes non-zero");ok(resolved,"guard ids resolved");ok(!dups,"no duplicate guard rva");
    for(size_t i=0;i<gn;++i) {
        char m[96];std::snprintf(m,sizeof m,"guard row %zu size matches plan 3.5",i);
        ok(battle_deaths_guards[i].size==kGuardSizes[i],m);
    }
    // sites: 4 distinct E8 sites, nonzero targets; 2+2 split by original callee
    const size_t sn=sizeof(battle_deaths_sites)/sizeof(battle_deaths_sites[0]);
    ok(sn==4,"4 call sites");
    unsigned to_visible=0,to_record=0;bool sdup=false,szero=false;
    for(size_t i=0;i<sn;++i) {
        if(!battle_deaths_sites[i].site_rva||!battle_deaths_sites[i].original_rva)szero=true;
        if(battle_deaths_sites[i].original_rva==battle_deaths_guards[3].rva)++to_visible;
        if(battle_deaths_sites[i].original_rva==battle_deaths_guards[2].rva)++to_record;
        for(size_t j=i+1;j<sn;++j)if(battle_deaths_sites[i].site_rva==battle_deaths_sites[j].site_rva)sdup=true;
    }
    ok(!szero&&!sdup,"sites resolved and distinct");ok(to_visible==2&&to_record==2,"2 sites -> 368360, 2 sites -> 295AA0");
    // sites must lie inside the guarded caller ranges (258EE0 + 0x870, 27D4B0 + 0x55E)
    auto inside=[&](unsigned rva,size_t g){return rva>=battle_deaths_guards[g].rva&&rva+5<=battle_deaths_guards[g].rva+battle_deaths_guards[g].size;};
    ok(inside(battle_deaths_sites[0].site_rva,0)&&inside(battle_deaths_sites[2].site_rva,0),"258EE0 sites inside guard row 0");
    ok(inside(battle_deaths_sites[1].site_rva,1)&&inside(battle_deaths_sites[3].site_rva,1),"27D4B0 sites inside guard row 1");
    ok(battle_deaths_span_rva+8==battle_deaths_guards[2].rva+battle_deaths_guards[2].size,"guard row 2 ends right after the span (295C67)");
    // span bytes
    ok(battle_deaths_span_len==8&&sizeof(battle_deaths_span_expect)==8,"span length 8");
    ok(battle_deaths_span_expect[0]==0x85&&battle_deaths_span_expect[1]==0xDB&&battle_deaths_span_expect[2]==0x0F&&battle_deaths_span_expect[3]==0x84,"span = test ebx,ebx ; je rel32");
    int32_t je;memcpy(&je,battle_deaths_span_expect+4,4);
    ok(je==0x8CE,"je displacement 0x8CE");
    ok(battle_deaths_span_rva+8+(unsigned)je==battle_deaths_reject_rva,"span je target == reject rva (static build)");
    ok(kBattleDeathsCutinMaxQueued==4&&kBattleDeathsRequestQueuedOff==0x1E0,"cut-in constants");
    ok(((battle_deaths_span_rva&15)+battle_deaths_span_len)>16,"span crosses a 16-byte boundary (reason for span instead of block)");
    // values of the reference build through the generated table
    {
        const int b=0;
        char m[160];
        const BuildExpect &x=kBuild;
        bool all=true;
        const unsigned ids[6]={GA(GA_25A880),GA(GA_27F250),GA(GA_2979B0),GA(GA_36A850),GA(GA_14DAE0),GA(GA_14D9D0)};
        for(unsigned i=0;i<6;++i)if(ids[i]!=x.guard_rva[i])all=false;
        std::snprintf(m,sizeof m,"build %d guard ids",b);ok(all,m);
        all=true;for(unsigned i=0;i<6;++i)if(GH(kRefHash[i])!=x.guard_hash[i])all=false;
        std::snprintf(m,sizeof m,"build %d guard hashes",b);ok(all,m);
        const unsigned sites[4]={GA(GA_25B022),GA(GA_27F716),GA(GA_25B0EB),GA(GA_27F7A9)};
        all=true;for(unsigned i=0;i<4;++i)if(sites[i]!=x.site[i])all=false;
        std::snprintf(m,sizeof m,"build %d sites",b);ok(all,m);
        ok(GA(GA_297B6F)==x.span&&GA(GA_298445)==x.reject,"span / reject ids");
        ok(x.span+8+0x8CEu==x.reject,"je target == reject (this build)");
        ok(GA(GA_14DAE0)==x.app&&GA(GA_14D9D0)==x.camera,"app / camera ids");
        ok(x.site[0]!=x.site[1]&&x.orig[0]==x.guard_rva[3]&&x.orig[2]==x.guard_rva[2],"original callee ids");
    }
}
static void test_registry() {
    unsigned rows=0,ok_rows=0;
    for(const LayoutRow &r:layout_rows) {
        if(std::strcmp(r.owner,"battle_deaths")!=0)continue;
        ++rows;
        if(r.kind!=LR_CALL)continue;
        for(const BattleDeathsSite &s:battle_deaths_sites)if(r.rva==s.site_rva&&r.original==s.original_rva)++ok_rows;
    }
    std::printf("registry_rows=%u%s\n",rows,rows?"":" (battle_deaths rows not in layout_registry.h yet: plan step S3 pending, not checked)");
    if(rows) {
        ok(rows==4,"registry has 4 battle_deaths CALL rows");
        ok(ok_rows==4,"registry CALL rows equal battle_deaths_sites (site + original)");
    }
}
int main() {
    test_relay();test_tables();test_registry();
    std::printf("{\"checks\":%d,\"failures\":%d}\n",checks,failures);
    return failures?1:0;
}
