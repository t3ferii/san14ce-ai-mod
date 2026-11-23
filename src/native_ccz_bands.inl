// 종군 중립군(무리) 0단계 진단 빌드 (사용자 2026-11-17, docs/NEUTRAL_BANDITS.md 10.7~10.8). Sole owner of the entry links
// below: SAN14_Convenience.dll publishes them, SAN14_CCZ.dll (ccz_bands.inl) decides. Also sole owner of CALL 177E98 (tactic damage x2
// for bands, 2026-11-19), decided here in the convenience module. Installed only with [ccz] bands=1, after
// every other convenience row (battle_deaths hashes 25A880, path_fast/ccz_combat hash 22E690 at their own install). A missing
// CCZ module, a changed prologue or a failed preparation publishes nothing: the game stays original and old saves keep their
// bandits. With ai_trace_original=1 the two order setters stay with fn_trace (native_ai_trace.inl) and only seven links publish.
#include "game_addresses.h"
#include "sidecar_cache.h"
#include "save_host.h"
namespace ccz_bands {
struct BandEntry {unsigned rva,size;unsigned char bytes[6];};
static Obj relay;
static bool installed;
static SidecarCache sidecar_cache;
static std::vector<uint8_t> save_snapshot;
static void rollback() {
    for(CodeLink &l:ccz_band_links::links) {restore_link(l);l.published=false;}
}
static_assert(sizeof(ccz_band_links::links)/sizeof(ccz_band_links::links[0])==kBandHookCount,"one link per band entry");
// 2026-11-19 user "대신에 전법에의한 피해는 두배로 받았으면 좋겠어": a band army takes double troop damage from tactics. The troop damage
// effect 177B30 multiplies its per-target product by the matchup factor 16C740(context, target army, target building) (CALL 177E98,
// returned in xmm0 and used once as a plain factor). Doubling it for a band target doubles the damage before the original 9999 cap, so
// the direct hit (179DE0 at 178226), the exchange target's stage-7 share (177B30 map -> 170D80 -> output +0x22) and the result record
// (17B0E0) all carry the same amount. Band army = generic commander 4033..4100 answering its patron force 1..45 through 22E690 (the
// army-force link above; original bandits and tribes answer 46..51). Sole owner of the CALL; refused bytes = no doubling, bands stay.
static CallLink tactic_link;
static bool tactic_ready;
static float __fastcall tactic_matchup(Obj context,Obj army,Obj building) {
    const float factor=fn<float(__fastcall *)(Obj,Obj,Obj)>(GA(GA_16C740))(context,army,building);
    if(!installed || !army)return factor;
    const unsigned commander=word(army,0x12);
    if(commander<4033 || commander>4100 || !valid(army))return factor;
    const unsigned force=army_force(army);if(!force || force>45)return factor;
#ifndef NO_RUNTIME_LOG
    static volatile LONG lines;   // army = commander, value = original factor x 1000, hex = patron force
    if(InterlockedIncrement(&lines)<=40)log_event("band_tactic_double",commander,static_cast<unsigned>(factor*1000.0f),force);
#endif
    return factor*2.0f;
}
static bool tactic_guard() {
    // mov rcx,r15; mov r8,[rsp+50h]; mov rdx,[rsp+30h] at site-43, then 30 bytes of SSE arithmetic (the last one RIP-relative, not
    // compared) that leave rcx/rdx/r8 alone, then call 16C740 (177B30, the per-target matchup factor).
    static const unsigned char before[]={0x49,0x8B,0xCF,0x4C,0x8B,0x44,0x24,0x50,0x48,0x8B,0x54,0x24,0x30};
    const unsigned site=GA(GA_177E98),target=GA(GA_16C740);
    return site>43 && target && image[site]==0xE8 && site+5+*reinterpret_cast<int *>(image+site+1)==target &&
           !ga::mcmp_at(image,site-43,before,13);
}
// 2026-11-20 kind traits (user "등갑병 등 그런거 추천해준것도 다 구현해", docs/NEUTRAL_BANDITS.md 25절). Sole owner of two more CALLs:
//  - 16B86C in the exchange stage 16B730: the one-direction normal damage 16B180(from, A, x, to, y, B, &out). After the original,
//    SAN14_CCZ.dll's bands->exchange may scale the result (cavalry attacker by terrain, rattan-armour target) and confuse the target.
//  - 43EB3E in the tile-fire stage 43E660: 179DE0(kind, id, amount, 0xFF, -1) for an army standing in fire; bands->fire may raise the
//    amount (rattan armour x3). Kind 0x1B (army) only; buildings keep the original amount.
// Refused bytes = that trait stays off; the bands themselves stay.
using NormalDamage=void(__fastcall *)(Obj,float,float,Obj,float,float,int *);
static CallLink exchange_link,fire_link;
static bool exchange_ready,fire_ready;
static void __fastcall exchange_damage(Obj from,float attack,float x,Obj to,float y,float defense,int *out) {
    fn<NormalDamage>(GA(GA_16B180))(from,attack,x,to,y,defense,out);
    const CczBands *api=installed && ccz_module::api?ccz_module::api->bands:nullptr;
    if(api && api->exchange && out && *out>0 && from && to)api->exchange(from,to,out);
}
static int __fastcall fire_damage(int kind,int id,int amount,int p4,int p5) {
    const CczBands *api=installed && ccz_module::api?ccz_module::api->bands:nullptr;
    if(api && api->fire && kind==0x1B && id>=0 && id<=500 && amount>0) {
        Obj army=table(0x7E838,static_cast<unsigned>(id),500);
        if(army && valid(army))amount=api->fire(army,amount);
    }
    return fn<int(__fastcall *)(int,int,int,int,int)>(GA(GA_179DE0))(kind,id,amount,p4,p5);
}
static bool exchange_guard() {
    // mov rcx,r10; movss xmm2,[rbp+8]; mov [rsp+30h],rax; movss [rsp+28h],xmm0; movss [rsp+20h],xmm1; movss xmm1,[rbp+4];
    // mov dword [rsp+50h],0; call 16B180 (16B730, the one direction of the exchange)
    static const unsigned char before[]={0x49,0x8B,0xCA,0xF3,0x0F,0x10,0x55,0x08,0x48,0x89,0x44,0x24,0x30,0xF3,0x0F,0x11,0x44,0x24,0x28,
        0xF3,0x0F,0x11,0x4C,0x24,0x20,0xF3,0x0F,0x10,0x4D,0x04,0xC7,0x44,0x24,0x50,0x00,0x00,0x00,0x00};
    const unsigned site=GA(GA_16B86C),target=GA(GA_16B180);
    return site>sizeof(before) && target && image[site]==0xE8 && site+5+*reinterpret_cast<int *>(image+site+1)==target &&
           !ga::mcmp_at(image,site-sizeof(before),before,sizeof(before));
}
static bool fire_guard() {
    // mov dword [rsp+20h],-1; mov r9d,0FFh; mov r8d,esi; mov edx,[rsp+48h]; mov ecx,[rsp+60h]; call 179DE0 (43E660 army in fire)
    static const unsigned char before[]={0xC7,0x44,0x24,0x20,0xFF,0xFF,0xFF,0xFF,0x41,0xB9,0xFF,0x00,0x00,0x00,0x44,0x8B,0xC6,
        0x8B,0x54,0x24,0x48,0x8B,0x4C,0x24,0x60};
    const unsigned site=GA(GA_43EB3E),target=GA(GA_179DE0);
    return site>sizeof(before) && target && image[site]==0xE8 && site+5+*reinterpret_cast<int *>(image+site+1)==target &&
           !ga::mcmp_at(image,site-sizeof(before),before,sizeof(before));
}
bool install(bool dev) {
    // 2026-11-18 의용병 무리 flag portrait: the band commander's flag face becomes the CCZ class picture via the face texture entry
    // 8AD530 (only caller 8ACF20, outside face_keep's 8ACF20..8AD3C6 guard). local231 tied it to 조조전 병종 그림 being off; since
    // 2026-11-19 (user, sprites on: "부대 깃발 초상화나 왼쪽 상세에 초상화가 스프라이트 그림이 아니긴한데") it applies in both
    // picture modes. The CCZ picture data is written once by load_asset and only read afterwards, so the renderer can share it.
    const bool portrait=true;
    const BandEntry entries[kBandHookCount]={
        {GA(GA_22E690),6,{0x40,0x53,0x48,0x83,0xEC,0x20}},   // army force (unit) -> u8
        {GA(GA_301D00),6,{0x40,0x55,0x41,0x54,0x41,0x55}},   // movement painting of one army (303020 day stage 0xB)
        {GA(GA_278290),6,{0x40,0x55,0x56,0x57,0x41,0x54}},   // supply link of one army
        {GA(GA_25A880),6,{0x40,0x55,0x56,0x57,0x41,0x54}},   // army removal request
        {GA(GA_186E60),5,{0x40,0x55,0x53,0x56,0x57}},        // the single hex-owner writer
        {GA(GA_268070),6,{0x40,0x56,0x48,0x83,0xEC,0x40}},   // periodic morale loss amount
        {GA(GA_2DC550),5,{0x48,0x89,0x4C,0x24,0x08}},        // day frame movement stage (44DEE0 case 8)
        {GA(GA_2ECEA0),5,{0x48,0x89,0x5C,0x24,0x08}},        // set action
        {GA(GA_2ECF00),5,{0x48,0x89,0x5C,0x24,0x08}},        // set target
        {GA(GA_8AD530),5,{0x48,0x8B,0xC4,0x56,0x57}},        // face texture from zp1 bytes (mov rax,rsp; push rsi; push rdi)
        {GA(GA_4522B0),6,{0x40,0x55,0x56,0x57,0x41,0x54}},   // selection list -> command window (2026-11-19: bands leave the list)
        {GA(GA_230460),6,{0x40,0x53,0x48,0x83,0xEC,0x20}},   // army max morale (2026-11-20: elite bands 130)
        {GA(GA_2801C0),5,{0x48,0x89,0x54,0x24,0x10}},        // capture roll (2026-11-20: band commanders are never captured)
        {GA(GA_2CCC60),6,{0x48,0x8B,0xC4,0x55,0x41,0x54}}};  // duel generator (2026-11-22: bands stay out of duels; mov rax,rsp; push rbp; push r12)
    if(installed)return true;
    if(!ccz_module::load())return false;
    const CczBands *api=ccz_module::api->bands;
    const bool setters=!mod_features.trace_original;
    auto wanted=[&](unsigned i){return i<kBandSetAction || (i<=kBandSetTarget && setters) || (i==kBandFaceTexture && portrait) || i==kBandUnitTarget || i==kBandMaxMorale || i==kBandCapture || i==kBandDuel;};
    unsigned count=0;
    for(unsigned i=0;i<kBandHookCount;++i) {
        if(!wanted(i))continue;
        if(!entries[i].rva || !api->hooks[i] || ga::mcmp_at(image,entries[i].rva,entries[i].bytes,entries[i].size)) {
            log_event("band_guard_rejected",i,entries[i].rva);return false;
        }
        ++count;
    }
    relay=allocate_near(image+entries[0].rva);
    if(!relay) {log_event("band_prepare_rejected",0xFF);return false;}
    void *gates[kBandHookCount]={};
    for(unsigned i=0;i<kBandHookCount;++i) {
        if(!wanted(i))continue;
        const BandEntry &e=entries[i];Obj stub=relay+i*64,gate=stub+32;CodeLink &link=ccz_band_links::links[i];
        absolute_jump(stub,api->hooks[i]);memcpy(gate,e.bytes,e.size);absolute_jump(gate+e.size,image+e.rva+e.size);
        gates[i]=gate;
        if(!prepare_link(link,e.rva,0xE9,stub)) {
            for(CodeLink &l:ccz_band_links::links)l={};
            VirtualFree(relay,0,MEM_RELEASE);relay=nullptr;log_event("band_prepare_rejected",i,e.rva);return false;
        }
        for(unsigned j=5;j<e.size;++j)link.modified[e.rva-link.block+j]=0x90;
    }
    tactic_ready=false;
    if(tactic_guard()) {
        absolute_jump(relay+1024,reinterpret_cast<void *>(&tactic_matchup));
        tactic_ready=call_prepare(tactic_link,GA(GA_177E98),relay+1024);
    }
    if(!tactic_ready)log_event("band_tactic_rejected",GA(GA_177E98),tactic_guard()?2u:1u);
    exchange_ready=fire_ready=false;
    if(api->exchange && exchange_guard()) {
        absolute_jump(relay+1056,reinterpret_cast<void *>(&exchange_damage));
        exchange_ready=call_prepare(exchange_link,GA(GA_16B86C),relay+1056);
    }
    if(!exchange_ready)log_event("band_exchange_rejected",GA(GA_16B86C),exchange_guard()?2u:1u);
    if(api->fire && fire_guard()) {
        absolute_jump(relay+1088,reinterpret_cast<void *>(&fire_damage));
        fire_ready=call_prepare(fire_link,GA(GA_43EB3E),relay+1088);
    }
    if(!fire_ready)log_event("band_fire_rejected",GA(GA_43EB3E),fire_guard()?2u:1u);
    DWORD old=0;
    // bits 8..11 = [ccz] band_troops 1..3 (idle growth 300->3000 / 400->4000 / 500->5000, user 2026-11-19)
    // bits 12..15 = [ccz] band_morale 1..3 (kill/rout morale x0.5 / x1 / x1.5, user 2026-11-20)
    const unsigned flags=1u|(dev?2u:0u)|(setters?4u:0u)|(portrait?8u:0u)|((mod_features.band_troops&15u)<<8)|((mod_features.band_morale&15u)<<12);
    if(!VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,4096) || !api->configure(gates,flags)) {
        for(CodeLink &l:ccz_band_links::links)l={};
        VirtualFree(relay,0,MEM_RELEASE);relay=nullptr;log_event("band_prepare_rejected",0xFE);return false;
    }
    // Scopes and the owner writer first, the army-force answer after them, the face texture, the day driver last.
    static const unsigned order[]={kBandPaint,kBandSupply,kBandRemove,kBandPaintWrite,kBandMorale,kBandSetAction,kBandSetTarget,kBandDuel,
                                   kBandArmyForce,kBandMaxMorale,kBandCapture,kBandFaceTexture,kBandUnitTarget,kBandDayMove};
    static_assert(sizeof(order)/sizeof(order[0])==kBandHookCount,"every band entry has a publish slot");
    for(unsigned i:order) {
        if(!wanted(i))continue;
        if(!publish_link(ccz_band_links::links[i])) {rollback();log_event("band_publish_rejected",i,entries[i].rva);return false;}   // relay stays mapped
    }
    installed=true;
    band_power_fn=api->power;   // 2AAF30 (battle_rules hook): morale/kind factor of live bands
    log_event("band_installed",count,flags);
    if(tactic_ready && !call_publish(tactic_link)) {tactic_ready=false;log_event("band_tactic_rejected",GA(GA_177E98),3);}
    if(tactic_ready)log_event("band_tactic_installed",GA(GA_177E98));
    if(exchange_ready && !call_publish(exchange_link)) {exchange_ready=false;log_event("band_exchange_rejected",GA(GA_16B86C),3);}
    if(fire_ready && !call_publish(fire_link)) {fire_ready=false;log_event("band_fire_rejected",GA(GA_43EB3E),3);}
    log_event("band_traits_installed",exchange_ready?1u:0u,fire_ready?1u:0u);
    // 2026-11-19 user "조조전 꺼져있으니까 ... 종군하기 전에 생성중일땐 안보인다고 부대깃발이": an idle record has no army, so only the
    // CCZ renderer can show it. With 조조전 병종 그림 off the renderer runs for the idle flags alone (army models stay native).
    if(!mod_features.ccz_sprites) {
        const bool flags_on=ccz_sprites::install_idle_flags(true);   // depth test as sprites-on runs it (ccz_depth = ccz_sprites, off here)
        log_event("band_idle_flags",flags_on?1u:0u);
    }
    return true;
}
// Sidecar (.bands): band records of this save (idle sites and live bands with their hosts), one fixed-size table.
static bool active() {return installed;}
static void snapshot() {
    save_snapshot.clear();
    if(!installed)return;
    unsigned char bytes[8+16*24];
    unsigned length=ccz_module::api->bands->save(bytes,sizeof(bytes));
    save_snapshot.assign(bytes,bytes+length);
}
static bool write_sidecar(const save_host::Job &job,save_host::Written &out) {
    if(!installed) {out.state=save_host::kOff;return true;}
    return save_host::store(self_module,save_host::Job{job.name,job.binding},L"bands",save_snapshot,sidecar_cache,out);
}
static void begin_load() {
    sidecar_cache.clear();save_snapshot.clear();
    if(installed)ccz_module::api->bands->reset();
}
static bool restore_sidecar(const save_host::Job *job) {
    if(!installed)return false;
    bool loaded=false;
    if(job) {
        wchar_t path[MAX_PATH];
        if(save_host::path(self_module,save_host::Job{job->name,job->binding},L"bands",path,MAX_PATH,false)) {
            HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file!=INVALID_HANDLE_VALUE) {
                unsigned char bytes[8+16*24]={};DWORD got=0;LARGE_INTEGER length={};
                bool ok=GetFileSizeEx(file,&length) && length.QuadPart>=8 && length.QuadPart<=static_cast<LONGLONG>(sizeof(bytes)) &&
                    ReadFile(file,bytes,static_cast<DWORD>(length.QuadPart),&got,nullptr) && got==length.QuadPart;
                CloseHandle(file);
                loaded=ok && ccz_module::api->bands->load(bytes,got);
                if(!loaded)log_event("band_load_rejected",ok?1:0,got);
            }
        }
    }
    if(!loaded)ccz_module::api->bands->reset();   // no sidecar: the save's force-51 armies stay original bandits
    return loaded;
}
const save_host::SidecarPlugin sidecar_plugin={"bands",L"bands",1,nullptr,"band_save_failed",&active,&snapshot,&write_sidecar,
    &begin_load,&restore_sidecar};
}
