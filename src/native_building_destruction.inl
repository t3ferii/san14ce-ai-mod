// Convenience-only. Called synchronously by the native type-20 stratagem caller.
// Capture the successful, unclamped roll, then expand AFTER native presentation,
// primary damage and the optional trap removal have finished.
namespace building_destruction {
using Complete=int(__fastcall *)(Obj);
struct Target {Obj object;unsigned id,hex,type,owner;};
struct Context {
    Obj command,region;
    unsigned force,count;
    int damage;
    bool captured;
    Target targets[3000];
};
// AI-CONTRACT [nested calls]: This pointer is a borrowed per-call context, not persistent target ownership. Preserve restoration on nested/failed calls, native failure semantics and expansion only after successful primary completion; targets exclude cities/gates.
static thread_local Context *current;
static volatile LONG installed;
static bool active() {return enabled && mod_features.convenience && mod_features.building_destruction_all;}
static bool hostile(Obj b,unsigned force,Obj region) {
    if(!valid(b) || !word(b,0x14) || !b[0x13] || b[0x13]>51 || b[0x13]==force || b[0x12]>30)return false;
    Obj definition=table(0x73958,b[0x12],30);
    if(!valid(definition) || definition[0x2C]<2 || definition[0x2C]>4)return false; // never cities/gates
    const unsigned hex=word(b,0x10);
    if(hex>=48400 || !fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(force,b[0x13]))return false;
    Obj tile=table(0xE270,hex,48399);
    return valid(tile) && fn<Obj(__fastcall *)(Obj)>(0x229EA0)(tile)==region;
}
static void __fastcall capture(Obj command,Obj primary,Obj region,int damage,Obj force) {
    Context *c=current;
    if(!active() || !c || c->command!=command || c->captured || damage<=0 ||
       !valid(primary) || !valid(region) || !valid(force))return;
    // Force+10 is the ruler PERSON id, not the force id. Resolve the identity
    // against the native force table (same convention as native_awards).
    unsigned own=0;
    for(unsigned id=1;id<=51;++id)if(table(0xDF30,id,51)==force) {own=id;break;}
    if(!own) {log_event("building_destruction_force_unknown",word(force,0x10));return;}
    c->captured=true;c->region=region;c->force=own;c->damage=damage;
    for(unsigned id=1;id<=3000;++id) {
        Obj b=table(0x6DA98,id,3000);
        if(b==primary || !hostile(b,own,region))continue;
        c->targets[c->count++]={b,id,word(b,0x10),b[0x12],b[0x13]};
    }
    log_event("building_destruction_capture",own,c->count,static_cast<unsigned>(damage));
}
static void apply(const Context &c) {
    if(!active() || !c.captured || !valid(c.region) || !valid(table(0xDF30,c.force,51)))return;
    unsigned hits=0;
    for(unsigned i=0;i<c.count;++i) {
        const Target &t=c.targets[i];
        Obj b=table(0x6DA98,t.id,3000);
        // Native callbacks may remove the bonus trap or change ownership. Do not
        // damage a replacement, newly friendly object, or another region.
        if(b!=t.object || !valid(b) || word(b,0x10)!=t.hex || b[0x12]!=t.type || b[0x13]!=t.owner ||
           !hostile(b,c.force,c.region))continue;
        const int remaining=static_cast<int>(word(b,0x14))-c.damage;
        if(remaining<=0)fn<void(__fastcall *)(Obj,Obj,int)>(0x294B90)(b,nullptr,0);
        else fn<void(__fastcall *)(Obj,int)>(0x1F42B0)(b,remaining);
        ++hits;
    }
    log_event("building_destruction_area",c.force,hits,static_cast<unsigned>(c.damage));
}
static int __fastcall complete(Obj command) {
    if(!active() || current)return fn<Complete>(0x1F9DE0)(command);
    Context context={};context.command=command;
    struct Scope {Scope(Context &c){current=&c;} ~Scope(){current=nullptr;}} scope(context);
    const int result=fn<Complete>(0x1F9DE0)(command);
    apply(context);
    log_event("building_destruction_result",static_cast<unsigned>(result),context.count,context.captured?1u:0u);
    return result;
}
// CALL at 1FA607 pushes eight bytes; the original RSP was 16-byte aligned.
// Save flags, volatile GPRs and XMM0..5, reserve shadow/arg5, then reproduce both
// displaced instructions with the native [RSP+78] adjusted to [RSP+80].
static unsigned make_capture_bridge(Obj at) {
    unsigned n=0;
    const unsigned char push[]={0x9C,0x50,0x51,0x52,0x41,0x50,0x41,0x51,0x41,0x52,0x41,0x53,
                                0x48,0x81,0xEC,0x98,0,0,0};
    memcpy(at+n,push,sizeof(push));n+=sizeof(push);
    for(unsigned xmm=0;xmm<6;++xmm) {
        const unsigned char save[]={0xF3,0x0F,0x7F,static_cast<unsigned char>(0x44+xmm*8),0x24,static_cast<unsigned char>(48+xmm*16)};
        // XMM5 uses disp32 because +80 cannot be encoded as positive disp8.
        if(xmm==5) {const unsigned char last[]={0xF3,0x0F,0x7F,0xAC,0x24,0x80,0,0,0};memcpy(at+n,last,sizeof(last));n+=sizeof(last);}
        else {memcpy(at+n,save,sizeof(save));n+=sizeof(save);}
    }
    const unsigned char args[]={0x48,0x8B,0x45,0xC0,0x48,0x89,0x44,0x24,0x20, // arg5 = native force
                                0x41,0x89,0xD1,0x4D,0x8B,0xC5, // R9D = full damage; R8 = region R13
                                0x48,0x8B,0x55,0xA0,0x48,0x8B,0x4D,0xE0,0x48,0xB8};
    memcpy(at+n,args,sizeof(args));n+=sizeof(args);
    *reinterpret_cast<void **>(at+n)=reinterpret_cast<void *>(&capture);n+=8;
    at[n++]=0xFF;at[n++]=0xD0;
    for(unsigned xmm=0;xmm<6;++xmm) {
        const unsigned char load[]={0xF3,0x0F,0x6F,static_cast<unsigned char>(0x44+xmm*8),0x24,static_cast<unsigned char>(48+xmm*16)};
        if(xmm==5) {const unsigned char last[]={0xF3,0x0F,0x6F,0xAC,0x24,0x80,0,0,0};memcpy(at+n,last,sizeof(last));n+=sizeof(last);}
        else {memcpy(at+n,load,sizeof(load));n+=sizeof(load);}
    }
    const unsigned char pop[]={0x48,0x81,0xC4,0x98,0,0,0,0x41,0x5B,0x41,0x5A,0x41,0x59,0x41,0x58,0x5A,0x59,0x58,0x9D,
                               0x89,0x55,0xF0,0x44,0x8B,0xA4,0x24,0x80,0,0,0,0xC3};
    memcpy(at+n,pop,sizeof(pop));return n+sizeof(pop);
}
#include "building_destruction_profile.h"
static bool guards_match() {
    for(const auto &g:guards) {
        unsigned long long h=0xCBF29CE484222325ull;
        for(unsigned i=0;i<g.size;++i)h=(h^image[g.rva+i])*0x100000001B3ull;
        if(h!=g.hash)return false;
    }
    return true;
}
static bool install() {
    if(InterlockedCompareExchange(&installed,0,0))return true;
    if(!guards_match()) {log_event("building_destruction_guard_rejected");return false;}
    Obj relay=allocate_near(image+0x1F91F1);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&complete));
    const unsigned length=make_capture_bridge(relay+64);
    CodeLink capture_link,complete_link;DWORD old;
    if(!prepare_link(capture_link,0x1FA607,0xE8,relay+64) ||
       !prepare_link(complete_link,0x1F91F1,0xE8,relay) ||
       !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,64+length)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    memset(capture_link.modified+12,0x90,3); // 1FA60C..1FA60E: tail of displaced MOV
    if(!publish_link(capture_link)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    if(!publish_link(complete_link)) {
        restore_link(capture_link);
        // Retain executable memory: another caller may already be in the stub.
        log_event("building_destruction_publish_rejected");return false;
    }
    InterlockedExchange(&installed,1);log_event("building_destruction_installed");return true;
}
}
