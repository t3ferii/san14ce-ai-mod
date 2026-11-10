#pragma once
// Battle deaths (docs/HISTORY.md 3.5, 2026-11-06): address-table ids, function-hash guards, span bytes and hook sites.
// Hashes are the FNV-1a values of the reference build 1a4a447d (src/game_addresses.h GH()).
// bdp::build_branch_relay (plan 3.3, step S2) builds the pure-byte relay that the 295C5F span jumps to.
#include "game_addresses.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct BattleDeathsGuard {unsigned rva,size;unsigned long long hash;};
static const BattleDeathsGuard battle_deaths_guards[]={
    {GA(GA_25A880),0x870,GH(0xFD9E9B85A74972A6ull)},   // 0 entry .. call at 25974B
    {GA(GA_27F250),0x55E,GH(0xF60E887BFD21C705ull)},   // 1 entry .. call at 27DA09
    {GA(GA_2979B0),0x1C7,GH(0x835F477A1B42648Cull)},   // 2 prologue .. 295C67
    {GA(GA_36A850),0x10E,GH(0x92A4E03C15B5DBB9ull)},   // 3 whole function (cut-in visibility)
    {GA(GA_14DAE0),0x8,GH(0x8A81202331C2D685ull)},     // 4 whole function
    {GA(GA_14D9D0),0x22,GH(0x0E33A03C060DC2C5ull)},    // 5 whole function
};

// 295C5F: test ebx,ebx ; je +0x8CE (identical in both builds)
static const unsigned char battle_deaths_span_expect[8]={0x85,0xDB,0x0F,0x84,0xCE,0x08,0x00,0x00};
static const unsigned battle_deaths_span_len=8;

// Hook sites: 4 CALL sites (E8 rel32) and the original callee each one must still target.
struct BattleDeathsSite {unsigned site_rva,original_rva;};
static const BattleDeathsSite battle_deaths_sites[]={
    {GA(GA_25B022),GA(GA_36A850)},   // 258EE0 battle death, cut-in visibility
    {GA(GA_27F716),GA(GA_36A850)},   // 27D4B0 death, cut-in visibility
    {GA(GA_25B0EB),GA(GA_2979B0)},   // 258EE0 death record call (scope)
    {GA(GA_27F7A9),GA(GA_2979B0)},   // 27D4B0 death record call (scope)
};
static const unsigned battle_deaths_span_rva=GA(GA_297B6F);        // span start (E9 + NOP)
static const unsigned battle_deaths_reject_rva=GA(GA_298445);      // je target = skip the record block
static const unsigned battle_deaths_app_rva=GA(GA_14DAE0);         // app getter used by the cut-in visibility fix
static const unsigned battle_deaths_camera_rva=GA(GA_14D9D0);      // camera getter

static const unsigned kBattleDeathsCutinMaxQueued=4;   // request object +0x1E0 must be < 4
static const unsigned kBattleDeathsRequestQueuedOff=0x1E0;

// ---- relay (docs/HISTORY.md 3.3) ----
// Entered by the E9 at 295C5F (original: test ebx,ebx ; je skip). Entry rsp is 16-aligned (295AA0 pushes 7 registers, then sub rsp,0x200).
// ebx!=0 -> pass untouched. ebx==0 -> save flags/volatile regs/xmm0-5, call predicate(rcx=r13), restore everything, then pass (predicate
// non-zero) or reject with the original ZF=1. Both exits are `jmp [rip+cell]`: no register is needed and no flag is changed.
// Returned bytes: code, 0xCC padding to 8, pass cell (8), reject cell (8): pass cell = len-16, reject cell = len-8.
namespace bdp {
static const size_t kRelayMaxLen=256;
struct RelayAsm {
    uint8_t *p;size_t cap,n;bool bad;
    void b(unsigned v) {if(n<cap)p[n]=(uint8_t)v;else bad=true;++n;}
    void bytes(const uint8_t *s,size_t k) {for(size_t i=0;i<k;++i)b(s[i]);}
    void u32(uint32_t v) {for(int i=0;i<4;++i)b((v>>(8*i))&0xFF);}
    void u64(uint64_t v) {for(int i=0;i<8;++i)b((unsigned)((v>>(8*i))&0xFF));}
    void patch32(size_t at,uint32_t v) {if(at+4<=cap)for(int i=0;i<4;++i)p[at+i]=(uint8_t)((v>>(8*i))&0xFF);else bad=true;}
};
static inline size_t build_branch_relay(uint8_t *out,size_t cap,uint64_t predicate,uint64_t pass,uint64_t reject) {
    if(!out||!predicate||!pass||!reject)return 0;
    RelayAsm a={out,cap,0,false};
    a.b(0x85);a.b(0xDB);                                   // test ebx,ebx
    a.b(0x0F);a.b(0x85);const size_t jne_at=a.n;a.u32(0);  // jne PASS
    a.b(0x9C);                                             // pushfq
    a.b(0x50);a.b(0x51);a.b(0x52);                         // push rax,rcx,rdx
    a.b(0x41);a.b(0x50);a.b(0x41);a.b(0x51);a.b(0x41);a.b(0x52);a.b(0x41);a.b(0x53);   // push r8,r9,r10,r11 (64 bytes pushed in total)
    static const uint8_t sub_rsp[7]={0x48,0x81,0xEC,0x80,0x00,0x00,0x00};              // sub rsp,80h (shadow 0x20 + xmm0-5 at +0x20..+0x7F)
    a.bytes(sub_rsp,7);
    for(unsigned i=0;i<6;++i) {a.b(0xF3);a.b(0x0F);a.b(0x7F);a.b(0x44|(i<<3));a.b(0x24);a.b(0x20+16*i);}   // movdqu [rsp+20h+16*i],xmm<i>
    a.b(0x4C);a.b(0x89);a.b(0xE9);                         // mov rcx,r13
    a.b(0x48);a.b(0xB8);a.u64(predicate);                  // mov rax,imm64
    a.b(0xFF);a.b(0xD0);                                   // call rax
    a.b(0x85);a.b(0xC0);                                   // test eax,eax
    for(unsigned i=0;i<6;++i) {a.b(0xF3);a.b(0x0F);a.b(0x6F);a.b(0x44|(i<<3));a.b(0x24);a.b(0x20+16*i);}   // movdqu xmm<i>,[rsp+20h+16*i]
    static const uint8_t lea_rsp[8]={0x48,0x8D,0xA4,0x24,0x80,0x00,0x00,0x00};         // lea rsp,[rsp+80h] (keeps the predicate ZF)
    a.bytes(lea_rsp,8);
    a.b(0x41);a.b(0x5B);a.b(0x41);a.b(0x5A);a.b(0x41);a.b(0x59);a.b(0x41);a.b(0x58);   // pop r11,r10,r9,r8
    a.b(0x5A);a.b(0x59);                                   // pop rdx,rcx
    a.b(0x0F);a.b(0x84);const size_t jz_at=a.n;a.u32(0);   // jz REJECT
    a.b(0x58);a.b(0x9D);                                   // pop rax ; popfq
    const size_t pass_at=a.n;
    a.b(0xFF);a.b(0x25);const size_t pass_disp=a.n;a.u32(0);       // PASS: jmp [rip+pass_cell]
    const size_t reject_at=a.n;
    a.b(0x58);a.b(0x9D);                                   // REJECT: pop rax ; popfq (original ZF=1)
    a.b(0xFF);a.b(0x25);const size_t reject_disp=a.n;a.u32(0);     // jmp [rip+reject_cell]
    while(a.n&7)a.b(0xCC);
    const size_t pass_cell=a.n;a.u64(pass);
    const size_t reject_cell=a.n;a.u64(reject);
    if(a.bad||a.n>kRelayMaxLen)return 0;
    a.patch32(jne_at,(uint32_t)(int32_t)((int64_t)pass_at-(int64_t)(jne_at+4)));
    a.patch32(jz_at,(uint32_t)(int32_t)((int64_t)reject_at-(int64_t)(jz_at+4)));
    a.patch32(pass_disp,(uint32_t)(int32_t)((int64_t)pass_cell-(int64_t)(pass_disp+4)));
    a.patch32(reject_disp,(uint32_t)(int32_t)((int64_t)reject_cell-(int64_t)(reject_disp+4)));
    return a.bad?0:a.n;
}
}   // namespace bdp
