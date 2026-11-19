// Convenience-only. Called synchronously by the native type-20 stratagem caller.
// Capture the successful, unclamped roll, then expand AFTER native presentation,
// primary damage and the optional trap removal have finished.
#include "game_addresses.h"
namespace building_destruction {
using Complete=int(__fastcall *)(Obj);
struct Target {Obj object;unsigned id,hex,type,owner;};
// local92 (user 2026-11-12 "계략 > 건물파괴 데미지를 1.5배로"): [stratagem] building_destruction_boost. A successful roll
// counts 1.5 times: the original applies its own damage to the chosen building, the missing half is taken off that
// building afterwards (the same original durability setter / removal), and the area option spreads the 1.5 value.
struct Context {
    Obj command,region,primary;
    unsigned force,count,primary_id,primary_hex,primary_type,primary_owner;
    int damage;
    bool captured;
    Target targets[3000];
};
// AI-CONTRACT [nested calls]: This pointer is a borrowed per-call context, not persistent target ownership. Preserve restoration on nested/failed calls, native failure semantics and expansion only after successful primary completion; targets exclude cities/gates.
static thread_local Context *current;
static volatile LONG installed;
static bool active() {return enabled && mod_features.convenience && (mod_features.building_destruction_all || mod_features.building_destruction_boost);}
// [game_flow] destruction_ticket (user 2026-11-13 "플레이어 1차 인접선에는 계략 건물파괴를 무조건 성공하는 티켓을 군사제안에 넣자
// 원작에선 자기땅이 아니면 실패해"). The effect function 1FB170 asks its result grade at CALL 1FB293 -> 1ECD20(caster person,
// target region, cost): 0 failure, 1 success, 2 great success. A failure (0) of a player officer against a region next to
// the player's land becomes a success (1) when the ambush module hands over one ticket (ticket_use, set by
// ambush::install). Every other answer, caster and region keeps the original grade; the roll itself is the original's.
bool (*ticket_use)(Obj person,Obj region);
bool (*ticket_ready)(Obj person,Obj region);   // same test as ticket_use, nothing spent (local128)
static bool ticket_active() {return enabled && mod_features.convenience && mod_features.destruction_ticket;}
static int __fastcall ticket_roll(Obj person,Obj region,unsigned cost) { COST_SCOPE("building_destruction::ticket_roll");
    const int grade=static_cast<int>(static_cast<unsigned char>(fn<char(__fastcall *)(Obj,Obj,unsigned)>(GA(GA_1ECD20))(person,region,cost)));
    if(grade!=0 || !ticket_active() || !ticket_use || !ticket_use(person,region))return grade;
    log_event("destruction_ticket_success",valid(person)?word(person,0x10):0u,cost,0);
    return 1;
}
static bool hostile(Obj b,unsigned force,Obj region) {
    if(!valid(b) || !word(b,0x14) || !b[0x13] || b[0x13]>51 || b[0x13]==force || b[0x12]>30)return false;
    Obj definition=table(0x73958,b[0x12],30);
    if(!valid(definition) || definition[0x2C]<2 || definition[0x2C]>4)return false; // never cities/gates
    const unsigned hex=word(b,0x10);
    if(hex>=48400 || !fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(force,b[0x13]))return false;
    Obj tile=table(0xE270,hex,48399);
    return valid(tile) && fn<Obj(__fastcall *)(Obj)>(GA(GA_22B8E0))(tile)==region;
}
static void __fastcall capture(Obj command,Obj primary,Obj region,int damage,Obj force) { COST_SCOPE("building_destruction::capture");
    Context *c=current;
    if(!active() || !c || c->command!=command || c->captured || damage<=0 ||
       !valid(primary) || !valid(region) || !valid(force))return;
    // Force+10 is the ruler PERSON id, not the force id. Resolve the identity
    // against the native force table (same convention as native_awards).
    unsigned own=0;
    for(unsigned id=1;id<=51;++id)if(table(0xDF30,id,51)==force) {own=id;break;}
    if(!own) {log_event("building_destruction_force_unknown",word(force,0x10));return;}
    // local10 G2 3.2: the capture bridge reads its arguments from per-build frame slots; a wrong slot shows up here as an owner that is
    // not a hostile of our force, and the roll is then not expanded.
    if(own==primary[0x13] || !fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,primary[0x13])) {
        log_event("building_destruction_capture_mismatch",own,primary[0x13]);return;
    }
    c->captured=true;c->region=region;c->force=own;c->damage=damage;
    c->primary=primary;c->primary_hex=word(primary,0x10);c->primary_type=primary[0x12];c->primary_owner=primary[0x13];
    for(unsigned id=1;id<=3000;++id)if(table(0x6DA98,id,3000)==primary) {c->primary_id=id;break;}
    for(unsigned id=1;mod_features.building_destruction_all && id<=3000;++id) {
        Obj b=table(0x6DA98,id,3000);
        if(b==primary || !hostile(b,own,region))continue;
        c->targets[c->count++]={b,id,word(b,0x10),b[0x12],b[0x13]};
    }
    log_event("building_destruction_capture",own,c->count,static_cast<unsigned>(damage));
}
static void apply(const Context &c) {
    if(!active() || !c.captured || !valid(c.region) || !valid(table(0xDF30,c.force,51)))return;
    unsigned hits=0;
    const bool boost=mod_features.building_destruction_boost;
    const int spread=boost?c.damage+c.damage/2:c.damage;
    if(boost && c.primary_id) {   // the half the original did not apply
        Obj b=table(0x6DA98,c.primary_id,3000);
        if(b==c.primary && valid(b) && word(b,0x14) && word(b,0x10)==c.primary_hex && b[0x12]==c.primary_type && b[0x13]==c.primary_owner) {
            const int remaining=static_cast<int>(word(b,0x14))-c.damage/2;
            if(remaining<=0)fn<void(__fastcall *)(Obj,Obj,int)>(GA(GA_296AA0))(b,nullptr,0);
            else fn<void(__fastcall *)(Obj,int)>(GA(GA_1F5680))(b,remaining);
            log_event("building_destruction_boost",c.force,static_cast<unsigned>(c.damage/2),c.primary_id);
        }
    }
    for(unsigned i=0;i<c.count;++i) {
        const Target &t=c.targets[i];
        Obj b=table(0x6DA98,t.id,3000);
        // Native callbacks may remove the bonus trap or change ownership. Do not
        // damage a replacement, newly friendly object, or another region.
        if(b!=t.object || !valid(b) || word(b,0x10)!=t.hex || b[0x12]!=t.type || b[0x13]!=t.owner ||
           !hostile(b,c.force,c.region))continue;
        const int remaining=static_cast<int>(word(b,0x14))-spread;
        if(remaining<=0)fn<void(__fastcall *)(Obj,Obj,int)>(GA(GA_296AA0))(b,nullptr,0);
        else fn<void(__fastcall *)(Obj,int)>(GA(GA_1F5680))(b,remaining);
        ++hits;
    }
    if(mod_features.building_destruction_all)log_event("building_destruction_area",c.force,hits,static_cast<unsigned>(spread));
}
static int __fastcall complete(Obj command) { COST_SCOPE("building_destruction::complete");
    if(!active() || current)return fn<Complete>(GA(GA_1FB170))(command);
    Context context={};context.command=command;
    struct Scope {Scope(Context &c){current=&c;} ~Scope(){current=nullptr;}} scope(context);
    const int result=fn<Complete>(GA(GA_1FB170))(command);
    apply(context);
    log_event("building_destruction_result",static_cast<unsigned>(result),context.count,context.captured?1u:0u);
    return result;
}
// CALL at GA_1FB9E6 pushes eight bytes; the original RSP was 16-byte aligned (both builds: five pushes, SUB RSP multiple of 16).
// Save flags, volatile GPRs and XMM0..5, reserve shadow/arg5, then reproduce the displaced instructions.
// The two displaced ADDs run first (before the flag save; they use RBP only), EDX is then the final damage; the following native MOV
// stays in place, so the bridge only returns; force [rbp-8], primary [rbp-58].
static unsigned make_capture_bridge(Obj at) {
    unsigned n=0;
    {const unsigned char head[]={0x03,0x55,0x90,0x03,0x55,0xE8};memcpy(at+n,head,sizeof(head));n+=sizeof(head);}
    const unsigned char push[]={0x9C,0x50,0x51,0x52,0x41,0x50,0x41,0x51,0x41,0x52,0x41,0x53,
                                0x48,0x81,0xEC,0x98,0,0,0};
    memcpy(at+n,push,sizeof(push));n+=sizeof(push);
    for(unsigned xmm=0;xmm<6;++xmm) {
        const unsigned char save[]={0xF3,0x0F,0x7F,static_cast<unsigned char>(0x44+xmm*8),0x24,static_cast<unsigned char>(48+xmm*16)};
        // XMM5 uses disp32 because +80 cannot be encoded as positive disp8.
        if(xmm==5) {const unsigned char last[]={0xF3,0x0F,0x7F,0xAC,0x24,0x80,0,0,0};memcpy(at+n,last,sizeof(last));n+=sizeof(last);}
        else {memcpy(at+n,save,sizeof(save));n+=sizeof(save);}
    }
    const unsigned char args[]={0x48,0x8B,0x45,0xF8,0x48,0x89,0x44,0x24,0x20, // arg5 = native force
                                0x41,0x89,0xD1,0x4D,0x8B,0xC5, // R9D = full damage; R8 = region R13
                                0x48,0x8B,0x55,0xA8,0x48,0x8B,0x4D,0xE0,0x48,0xB8};
    memcpy(at+n,args,sizeof(args));n+=sizeof(args);
    *reinterpret_cast<void **>(at+n)=reinterpret_cast<void *>(&capture);n+=8;
    at[n++]=0xFF;at[n++]=0xD0;
    for(unsigned xmm=0;xmm<6;++xmm) {
        const unsigned char load[]={0xF3,0x0F,0x6F,static_cast<unsigned char>(0x44+xmm*8),0x24,static_cast<unsigned char>(48+xmm*16)};
        if(xmm==5) {const unsigned char last[]={0xF3,0x0F,0x6F,0xAC,0x24,0x80,0,0,0};memcpy(at+n,last,sizeof(last));n+=sizeof(last);}
        else {memcpy(at+n,load,sizeof(load));n+=sizeof(load);}
    }
    const unsigned char pop[]={0x48,0x81,0xC4,0x98,0,0,0,0x41,0x5B,0x41,0x5A,0x41,0x59,0x41,0x58,0x5A,0x59,0x58,0x9D};
    memcpy(at+n,pop,sizeof(pop));n+=sizeof(pop);
    at[n++]=0xC3;
    return n;
}
#include "building_destruction_profile.h"
// local128 (user 2026-11-13 "그냥 조언을 바꾸자 미리 준비한 책략으로 성공한다고" / "원작 조언함수"): the stratagem window's advisor line comes
// from the original advice function 2F5AA0(strategist ctx, action, *recommend, *grade, params, person id, target id, ...): it
// formats one message into a UTF-16 buffer and returns it (callers measure it with AC4B30, short*). Action 0x14 (건물파괴) asks
// the same grade function 1ECD20 as the effect, with the region id (< 0x1F5, area table) as the target. Entry hook: the first
// instruction is exactly five bytes (4C 89 4C 24 20, mov [rsp+20],r9), copied to the gate. The original runs first; only when the
// ticket would be spent on that officer and region (ticket_ready) the returned line is replaced and the advisor recommends.
static const unsigned char kAdviceEntry[16]={0x4C,0x89,0x4C,0x24,0x20,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
using Advice=Obj(__fastcall *)(Obj,int,unsigned *,int *,void *,int,int,int,unsigned long long,int);
static Advice original_advice;
static Obj __fastcall advice(Obj strategist,int action,unsigned *recommend,int *grade,void *params,int person,int target,int p8,unsigned long long p9,int p10) { COST_SCOPE("building_destruction::advice");
    Obj text=original_advice(strategist,action,recommend,grade,params,person,target,p8,p9,p10);
    if(action!=0x14 || !text || !ticket_active() || !ticket_ready || person<=0 || person>6000 || target<=0 || target>500)return text;
    Obj who=table(0x158,static_cast<unsigned>(person),6000),area=table(0x6CAF0,static_cast<unsigned>(target),500);
    if(!valid(who) || !valid(area) || !ticket_ready(who,area))return text;
    wcscpy_s(reinterpret_cast<wchar_t *>(text),128,L"이번에는 미리 준비해 둔 책략이 있으니 반드시 성공할 것입니다.");
    if(recommend)*recommend=1;
    static unsigned logged;if(logged<20){++logged;log_event("destruction_ticket_advice",static_cast<unsigned>(person),static_cast<unsigned>(target),0);}
    return text;
}
static bool advice_site_ok() {return GA(GA_2F5AA0) && !ga::mcmp(image,GA_2F5AA0,0,kAdviceEntry,sizeof(kAdviceEntry));}
// The ticket CALL (GA_1FB293, E8 rel32 -> 1ECD20; 1FB170 is pinned whole by the guard) is a plain call replacement: the
// stub at relay+2048 jumps to ticket_roll with the original three arguments; the caller reads EAX.
static bool ticket_site_ok() {
    const unsigned site=GA(GA_1FB293),target=GA(GA_1ECD20);
    if(!site || !target || image[site]!=0xE8)return false;
    int delta=0;memcpy(&delta,image+site+1,4);
    return site+5+static_cast<unsigned>(delta)==target;
}
bool install() {
    if(InterlockedCompareExchange(&installed,0,0))return true;
    const bool spread=active(),ticket=ticket_active();
    if(!spread && !ticket)return false;
    if(!guards_match()) {log_event("building_destruction_guard_rejected");log_event("building_destruction_guard_failed",guard_failed_row);return false;}
    const bool shape=!spread || capture_shape_ok(),site=!ticket || ticket_site_ok(),advise=ticket && advice_site_ok();
    log_event("building_destruction_form",(shape?2u:0u)|(site?4u:0u)|(advise?8u:0u));
    if(!shape || !site)return false;
    Obj relay=allocate_near(image+GA(GA_1FA581));if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&complete));
    const unsigned length=make_capture_bridge(relay+64);
    absolute_jump(relay+2048,reinterpret_cast<void *>(&ticket_roll));
    if(advise) {   // stub at +2304 -> advice; gate at +2560 = the five displaced bytes + jump back to 2F5AA5
        absolute_jump(relay+2304,reinterpret_cast<void *>(&advice));
        memcpy(relay+2560,image+GA(GA_2F5AA0),5);absolute_jump(relay+2565,image+GA(GA_2F5AA0)+5);
        original_advice=reinterpret_cast<Advice>(relay+2560);
    }
    CodeLink capture_link{},complete_link{},ticket_link{},advice_link{};DWORD old;
    const bool advice_ready=advise && prepare_link(advice_link,GA(GA_2F5AA0),0xE9,relay+2304);
    if((spread && (!prepare_link(capture_link,GA(GA_1FB9E6),0xE8,relay+64) ||
                   !prepare_link(complete_link,GA(GA_1FA581),0xE8,relay))) ||
       (ticket && !prepare_link(ticket_link,GA(GA_1FB293),0xE8,relay+2048)) ||
       !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,4096)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    (void)length;
    if(spread) {   // the CALL covers five bytes of the displaced code; the sixth byte becomes a NOP
        const unsigned off=GA(GA_1FB9E6)-capture_link.block;
        memset(capture_link.modified+off+5,0x90,1);
    }
    if(ticket && !publish_link(ticket_link)) {VirtualFree(relay,0,MEM_RELEASE);log_event("destruction_ticket_publish_rejected");return false;}
    // The advisor line is optional: a failed publish keeps the ticket itself (logged, original advice stays).
    if(advice_ready && !publish_link(advice_link))log_event("destruction_ticket_advice_rejected");
    else if(advice_ready)log_event("destruction_ticket_advice_installed");
    if(spread) {
        if(!publish_link(capture_link)) {
            restore_link(ticket_link);   // retain the relay: a caller may already be in the ticket stub
            return false;
        }
        if(!publish_link(complete_link)) {
            restore_link(capture_link);restore_link(ticket_link);
            // Retain executable memory: another caller may already be in the stub.
            log_event("building_destruction_publish_rejected");return false;
        }
    }
    InterlockedExchange(&installed,1);log_event("building_destruction_installed",spread?1u:0u,ticket?1u:0u);return true;
}
}
