// v1.6 evidence and exact source fingerprints: research/modpack_v16/INTEGRATION.md.
// Only the convenience owner includes this unit. Battle deaths, the trait hover and traits9 are own source (native_battle_deaths.inl,
// native_trait_hover.inl, native_traits9.inl) that native_split_install.inl includes after it: this unit names none of them.
// Stats200 (F-03, docs/MODULARIZATION_PLAN.md section 6): the caps, the item-0x7B clamp relay and the growth relay are registered with the
// Layout hook manager as owner "stats200" in one all-or-none transaction; this unit writes no image bytes itself. Without the manager (the guard
// fixture builds this unit unrouted) the feature stays off.
#include "game_addresses.h"
#include "stat_growth.h"
struct ModpackBytes {unsigned rva,size;unsigned char before[16],after[16];};
#include "modpack_tables.h"
static void modpack_extended_growth(Obj person,unsigned ability) { COST_SCOPE("modpack_extended_growth");
    if(!person || ability>=5)return;
    unsigned current=person[0xEC+ability];if(current<100 || current>=200)return;
    Obj table=reinterpret_cast<Obj(__fastcall *)()>(image+GA(GA_337C40))();if(!table)return;
    Obj r99=pointer(table,0x60+99*8),r100=pointer(table,0x60+100*8);
    using Valid=int(__fastcall *)(Obj);
    auto valid=reinterpret_cast<Valid>(image+GA(GA_331750));
    if(!r99 || !r100 || !valid(r99) || !valid(r100))return;
    unsigned experience=*reinterpret_cast<unsigned *>(person+0x104+ability*4);
    unsigned a=*reinterpret_cast<unsigned *>(r99+0x10+ability*4);
    unsigned b=*reinterpret_cast<unsigned *>(r100+0x10+ability*4);
    person[0xEC+ability]=static_cast<unsigned char>(statgrowth::advance(current,experience,a,b));
}
static Obj modpack_relay;
// Item-0x7B clamp (stats200, 22FCE0/231770 "stat 4 with item 0x7B = 100"). Long form (previous build, removed at the 1a4a447d re-base): JE rel8 + 6-byte NOP, patched in
// place by the modpack_bytes row (raise to 100 only below 100). Short form (1a4a447d, local10): JE rel8 + 3-byte NOP whose next
// byte is the loop head (a jump target), so the row cannot fit; the JE target MOV ESI,100 (5 bytes, reached only from that JE)
// becomes an E9 to a relay doing the same "raise to 100 only below 100" and returning past the MOV.
static Obj modpack_clamp_relay;
static unsigned modpack_clamp_rva;                                      // the MOV ESI,100 the clamp relay replaces; 0 = not prepared
static unsigned char modpack_clamp_expect[5],modpack_growth_expect[5];  // image bytes under the two relay spans, copied once the guards passed
static unsigned modpack_clamp_short() {   // RVA of the MOV ESI,100 in the short form, else 0
    unsigned r=GA(GA_231AFB);const unsigned char nop3[]={0x0F,0x1F,0x00},mov[]={0xBE,0x64,0x00,0x00,0x00};
    if(!r || image[r]!=0x74 || ga::mcmp_at(image,r+2,nop3,3))return 0;
    unsigned t=r+2+static_cast<unsigned>(static_cast<int>(static_cast<signed char>(image[r+1])));
    return t>r+5 && t<r+0x80 && !ga::mcmp_at(image,t,mov,5)?t:0;
}
static bool prepare_modpack_clamp() {
    unsigned t=modpack_clamp_short();if(!t)return false;
    if(!modpack_clamp_relay)modpack_clamp_relay=allocate_near(image+t);
    Obj p=modpack_clamp_relay;if(!p)return false;
    const unsigned char code[]={0x83,0xFE,0x64,0x7D,0x05,0xBE,0x64,0x00,0x00,0x00};   // CMP ESI,100; JGE +5; MOV ESI,100
    memcpy(p,code,sizeof(code));absolute_jump(p+sizeof(code),image+t+5);
    DWORD old=0;
    if(!VirtualProtect(p,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),p,4096))return false;
    modpack_clamp_rva=t;memcpy(modpack_clamp_expect,image+t,sizeof(modpack_clamp_expect));
    return true;
}
static bool prepare_modpack_growth() {
    modpack_relay=allocate_near(image+GA(GA_2BC30A));if(!modpack_relay)return false;
    Obj p=modpack_relay;
    // Native RSP is aligned here. R13/R15/RSI/RDI remain native nonvolatile state.
    const unsigned char code[]={0x41,0x83,0xFD,0x64,0x72,0x3A,0x48,0x83,0xEC,0x20,
        0x48,0x89,0xF1,0x89,0xFA,0x48,0xB8};
    memcpy(p,code,sizeof(code));*reinterpret_cast<void **>(p+17)=reinterpret_cast<void *>(&modpack_extended_growth);
    const unsigned char done[]={0xFF,0xD0,0x48,0x83,0xC4,0x20};memcpy(p+25,done,sizeof(done));
    absolute_jump(p+31,image+GA(GA_2BC3C8));
    memcpy(p+64,image+GA(GA_2BC30A),4); // LEA EBX,[R13+1]
    p[68]=0xE8;*reinterpret_cast<int *>(p+69)=static_cast<int>((image+GA(GA_337C40))-(p+73));
    absolute_jump(p+73,image+GA(GA_2BC313));
    DWORD old=0;
    if(!VirtualProtect(p,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),p,4096))return false;
    memcpy(modpack_growth_expect,image+GA(GA_2BC30A),sizeof(modpack_growth_expect));
    return true;
}
// Collab record callbacks run on native deserialization threads. The initial
// loader-time record sweep from the reference is deliberately unnecessary here.
using CollabRead=uint64_t(__fastcall *)(Obj,Obj,Obj,Obj);
static void collab_unlock(Obj object,bool trait) {
    Obj root=pointer(image,GA(GA_2780F60));if(!object || !root)return;
    unsigned table=trait?0x7DD18:0x774D0,stride=trait?0xE8:0x90;
    Obj first=pointer(root,table);if(!first || object<first)return;
    uintptr_t delta=static_cast<uintptr_t>(object-first);if(delta%stride)return;
    uintptr_t id=delta/stride;if(id>0xFFFF)return;
    const unsigned short *ids=trait?collab_traits:collab_tactics;
    size_t count=trait?sizeof(collab_traits)/2:sizeof(collab_tactics)/2;
    if(std::find(ids,ids+count,static_cast<unsigned short>(id))==ids+count)return;
    unsigned vt=trait?GA(GA_13AA230):GA(GA_13A9DF8),off=trait?0xC8:0x70,flag=trait?0xBF:0x44;
    if(pointer(object,0)!=image+vt)return;
    Obj storage=pointer(object,off);if(!storage || pointer(object,off+8)-storage!=(trait?4:40))return;
    using Codec=unsigned(__fastcall *)(Obj,unsigned,unsigned,unsigned);
    unsigned before=object[flag],encoded=*reinterpret_cast<unsigned *>(storage);
    unsigned read=reinterpret_cast<Codec>(pointer(image+vt,trait?0x58:0x48))(object,encoded,1,static_cast<unsigned>(id));
    if(read!=before)return;
    unsigned after=before|(trait?0x10:2);
    unsigned value=reinterpret_cast<Codec>(pointer(image+vt,trait?0x50:0x40))(object,after,1,static_cast<unsigned>(id));
    *reinterpret_cast<unsigned *>(storage)=value&(trait?0xFFFF:0xFF);object[flag]=static_cast<unsigned char>(after);
}
extern "C" void SAN14ModpackCollabBootstrap() { COST_SCOPE("SAN14ModpackCollabBootstrap");
    Obj root=pointer(image,GA(GA_2780F60));if(!root)return;
    for(auto id:collab_traits)collab_unlock(pointer(root,0x7DD18+id*8),true);
    for(auto id:collab_tactics)collab_unlock(pointer(root,0x774D0+id*8),false);
}
// Layout manager chain of the four record/scenery slots (design 8.0 G1; null: original game function).
static void *next_collab_trait,*next_collab_tactic,*next_scenery_tick,*next_scenery_discovery;
// Same-function peers (design 8.2, decided at guard time by split_layout_yield): set = do not apply that part.
static bool modpack_yield_collab;   // trait_hover_yield lives in native_trait_hover.inl
static uint64_t __fastcall collab_trait_read(Obj a,Obj b,Obj c,Obj d) { COST_SCOPE("modpack_collab_trait_read");
    auto result=next_or<CollabRead>(next_collab_trait,GA(GA_2404F0))(a,b,c,d);
    if(static_cast<unsigned>(result) && b && *reinterpret_cast<int *>(b+0x20)==1)collab_unlock(a,true);
    return result;
}
static uint64_t __fastcall collab_tactic_read(Obj a,Obj b,Obj c,Obj d) { COST_SCOPE("modpack_collab_tactic_read");
    auto result=next_or<CollabRead>(next_collab_tactic,GA(GA_241530))(a,b,c,d);
    if(static_cast<unsigned>(result) && b && *reinterpret_cast<int *>(b+0x20)==1)collab_unlock(a,false);
    return result;
}
// Scenery's own movie and discovery state callbacks only. Keep native rewards,
// history and state teardown, including the reference's Lv.6 special sequence.
// CSearchSceneryMovieState (vtable 13D2ED8) states, tick 421990:
//   0 = 420A60: save camera (+4A0..) and fly it to the scenery hex (451360)
//   1 = 4226C0: movie cut-in   2 = 422790: result message   3 = 422930: wait
//   +574 frames, fly the camera back to the saved +4E0.. position, then pop.
// Skip: state 0 -> our result line -> state 3 with its frame counter already
// past the threshold, so 422930 takes only its native "pop and continue"
// branch: no camera flight out and none back. No per-object table (v12.15 and
// earlier kept 16 slots that were never released; after 16 discoveries the
// native camera flight and movie ran again).
static void scenery_result(Obj o) {
    int message=*reinterpret_cast<int *>(o+0x498),amount=*reinterpret_cast<int *>(o+0x49C);
    if(message && amount>0) {
        Obj context=reinterpret_cast<Obj(__fastcall *)()>(image+GA(GA_3FA4A0))();
        context=reinterpret_cast<Obj(__fastcall *)(Obj)>(image+GA(GA_1D3BC0))(context);
        Obj text=image+GA(GA_1A26440);
        reinterpret_cast<void(__fastcall *)(Obj)>(image+GA(GA_30C380))(text);
        Obj target=pointer(o,0x490);
        if(message==0x5C75) {
            Obj data=reinterpret_cast<Obj(__fastcall *)(Obj)>(image+GA(GA_22C2C0))(target);
            reinterpret_cast<void(__fastcall *)(Obj,unsigned,unsigned)>(image+GA(GA_30F950))(text,*reinterpret_cast<unsigned short *>(data+0x4E),0);
        } else if(message==0x5C76)
            reinterpret_cast<void(__fastcall *)(Obj,int,int)>(image+GA(GA_30F9A0))(text,*reinterpret_cast<int *>(o+0x480),0);
        unsigned id=reinterpret_cast<unsigned(__fastcall *)(Obj)>(image+GA(GA_22B9C0))(target);
        reinterpret_cast<void(__fastcall *)(Obj,unsigned,unsigned)>(image+GA(GA_30FA60))(text,id,0);
        reinterpret_cast<void(__fastcall *)(Obj,Obj,unsigned)>(image+GA(GA_30FCB0))(text,target+0x10,0);
        reinterpret_cast<void(__fastcall *)(Obj,int,int)>(image+GA(GA_30FDB0))(text,amount,0);
        Obj formatted=reinterpret_cast<Obj(__fastcall *)(Obj,int)>(pointer(pointer(text,0),8))(text,message);
        Obj history=reinterpret_cast<Obj(__fastcall *)()>(image+GA(GA_1970E0))();
        reinterpret_cast<void(__fastcall *)(Obj,Obj,unsigned,Obj,Obj)>(image+GA(GA_197E50))(history,context,0x1000,image+GA(GA_139F770),formatted);
    }
    ++*reinterpret_cast<int *>(o+0x470);
}
static void __fastcall scenery_tick(Obj o) { COST_SCOPE("modpack_scenery_tick");
    if(pointer(o,0)==image+GA(GA_13D5F68) && *reinterpret_cast<int *>(o+0x470)==0) {
        if(!reinterpret_cast<int(__fastcall *)(Obj)>(image+GA(GA_572670))(o))return;
        auto valid=reinterpret_cast<int(__fastcall *)(Obj)>(image+GA(GA_331780));
        if(valid(pointer(o,0x490)) && valid(pointer(o,0x488))) {
            *reinterpret_cast<int *>(o+0x470)=2;scenery_result(o); // -> 3
            *reinterpret_cast<unsigned *>(o+0x574)=0xFFFFFFFFu;     // camera return already "done"
        }
    }
    next_or<void(__fastcall *)(Obj)>(next_scenery_tick,GA(GA_423D00))(o);
}
static uint64_t __fastcall scenery_discovery(Obj o,Obj b,Obj c,Obj d) { COST_SCOPE("scenery_discovery");
    auto result=next_or<CollabRead>(next_scenery_discovery,GA(GA_724E40))(o,b,c,d);
    if(static_cast<unsigned>(result) && *reinterpret_cast<int *>(o+0x4B0)==5) {
        auto hide=reinterpret_cast<void(__fastcall *)(Obj,unsigned)>(image+GA(GA_83E450));
        hide(o,3);hide(o,4);
        reinterpret_cast<void(__fastcall *)(Obj)>(pointer(pointer(o,0),0x40))(o);
        *reinterpret_cast<int *>(o+0x50C)=2;
    }
    return result;
}
static const SplitPointer modpack_pointers[]={
    {GA(GA_13AA260),GA(GA_2404F0),reinterpret_cast<void *>(&collab_trait_read),kPointerShared,&next_collab_trait},
    {GA(GA_13A9E28),GA(GA_241530),reinterpret_cast<void *>(&collab_tactic_read),kPointerShared,&next_collab_tactic},
    {GA(GA_13D5F90),GA(GA_423D00),reinterpret_cast<void *>(&scenery_tick),kPointerShared,&next_scenery_tick},
    {GA(GA_144A278),GA(GA_724E40),reinterpret_cast<void *>(&scenery_discovery),kPointerShared,&next_scenery_discovery}
};
// Feature-specific checks. Battle deaths (native_battle_deaths.inl) and the tooltip hover (native_trait_hover.inl: trait_hover_off /
// trait_hover_yield / trait_hover_wanted()) are own source with their own guards; the collab record slots and scenery are our
// own code and stay on with tooltip_collab.
static bool modpack_guards_match(bool stats=true,bool growth=true,bool collab=true,bool scenery=true) {
    if(stats && !modpack_clamp_short())return false;
    if(stats)for(const auto &p:modpack_bytes)if(ga::mcmp_at(image,p.rva,p.before,p.size))return false;
    if(stats)for(const auto &p:modpack_data_bytes)if(ga::mcmp_at(image,p.rva,p.before,p.size))return false;
#ifndef LAYOUT_ROUTED   // routed: slot ownership is judged by the Layout hook manager
    for(unsigned i=0;i<4;++i)if(i<2?collab:scenery) {
        const auto &p=modpack_pointers[i];if(pointer(image,p.slot)!=image+p.original)return false;
    }
#else
    (void)collab;
#endif
    const unsigned char growth_bytes[]={0x41,0x8D,0x5D,0x01,0xE8,0x2D,0xB9,0x07,0x00};
    // 422930: MOV ECX,[RBX+574]; CMP ECX,R8D; JAE pop-branch (unsigned frame counter).
    const unsigned char scenery_wait[]={0x8B,0x8B,0x74,0x05,0x00,0x00,0x41,0x3B,0xC8,0x0F,0x83};
    // growth: LEA EBX,[R13+1]; CALL rel32. The rel32 differs per build, so the CALL target (GA_337C40) is judged, not the displacement.
    auto growth_ok=[&]() {
        unsigned g=GA(GA_2BC30A),c=GA(GA_337C40);
        if(!g || !c || ga::mcmp(image,GA_2BC30A,0,growth_bytes,5))return false;
        int rel;memcpy(&rel,image+g+5,4);
        return static_cast<int64_t>(g)+9+rel==static_cast<int64_t>(c);
    };
    return (!growth || growth_ok()) &&
        (!scenery || !ga::mcmp(image,GA_424CE4,0,scenery_wait,sizeof(scenery_wait)));
}
// Runtime copies of the stats200 byte rows (code rows and the data row): the manager compares `expect` literally, while the guards (ga::mcmp_at)
// judged the pristine bytes of the running build (a known build against its own original bytes, an unknown one with wildcards). So once the guards
// have passed, `before` is taken from the image (the same step as traits9_copy_before); `after` is unchanged.
static ModpackBytes modpack_rows[sizeof(modpack_bytes)/sizeof(modpack_bytes[0])];
static ModpackBytes modpack_data_rows[sizeof(modpack_data_bytes)/sizeof(modpack_data_bytes[0])];
static bool modpack_rows_ready;
static bool modpack_copy_row(const ModpackBytes &source,ModpackBytes &row) {
    row=source;
    if(!row.rva || row.size>sizeof(row.before))return false;
    memcpy(row.before,image+row.rva,row.size);return true;
}
static bool modpack_copy_before() {
    for(size_t n=0;n<sizeof(modpack_bytes)/sizeof(modpack_bytes[0]);++n)if(!modpack_copy_row(modpack_bytes[n],modpack_rows[n]))return false;
    for(size_t n=0;n<sizeof(modpack_data_bytes)/sizeof(modpack_data_bytes[0]);++n)if(!modpack_copy_row(modpack_data_bytes[n],modpack_data_rows[n]))return false;
    modpack_rows_ready=true;return true;
}
// Result: bit0 = a stats200 piece could not be prepared, bit1 = the guards no longer match (3 = both, the caller then re-checks them).
// stats/growth/collab/scenery are the four feature flags: the caller owns them, this unit never reads mod_features.
static unsigned prepare_modpack(bool stats,bool growth,bool collab,bool scenery) {
    unsigned failed=0;
    if(!modpack_guards_match(stats,growth,collab,scenery))return 3;
    if(stats && !modpack_copy_before())failed|=1;
    if(growth && !prepare_modpack_growth())failed|=1;
    if(stats && !prepare_modpack_clamp())failed|=1;
    return failed;
}
#ifdef LAYOUT_ROUTED
// Stats200 hooks (F-03): owner "stats200" is ONE manager transaction, LF_Y = YIELD_IF_PEER|ALL_OR_NONE. The clamp relay span, the code rows, the data
// row and the growth relay span stand or fall together (a refused or yielded piece takes the others back, as the former hand-written publish did),
// and a same-function peer (R1 ModPack Stats200) makes the whole owner yield. The manager writes every covering 16-byte block with one CMPXCHG16B (a
// patch longer than a block is not atomic as a whole; safety rests on the single UI-thread registration before the patched paths run) and its
// unhook puts bytes back only while they are still ours.
static bool register_stats200_code() {
    for(const auto &p:modpack_rows)
        if(layout_client::bytes("stats200",p.rva,p.before,p.after,p.size,LF_Y)<=0)return false;
    return true;
}
static bool register_stats200_data() {
    for(const auto &p:modpack_data_rows)
        if(layout_client::bytes("stats200",p.rva,p.before,p.after,p.size,LF_Y)<=0)return false;
    return true;
}
static bool register_stats200(bool growth) {
    layout_client::Transaction transaction;
    if(!modpack_rows_ready || !modpack_clamp_rva || !modpack_clamp_relay)return false;
    if(layout_client::span("stats200",modpack_clamp_rva,modpack_clamp_expect,sizeof(modpack_clamp_expect),modpack_clamp_relay,LF_Y)<=0)return false;
    log_event("stats200_clamp_relay",1);
    if(!register_stats200_code() || !register_stats200_data())return false;
    // v12.8: natural growth past 100 is a separate (dependent) switch.
    if(growth && (!modpack_relay ||
        layout_client::span("stats200",GA(GA_2BC30A),modpack_growth_expect,sizeof(modpack_growth_expect),modpack_relay,LF_Y)<=0))return false;
    log_event("stats200_rows",static_cast<unsigned>(sizeof(modpack_rows)/sizeof(modpack_rows[0])),
        static_cast<unsigned>(sizeof(modpack_data_rows)/sizeof(modpack_data_rows[0])),growth?1u:0u);
    transaction.done=true;return true;
}
// Pieces: stats200 = caps + clamp relay + growth gate (growth is its dependent switch); tooltip_collab = collab record slots 0..1 (the hover tooltip
// is native_trait_hover.inl, its first tick runs the collab sweep); scenery_skip = slots 2..3. The flags are arguments (collab already without a
// same-function peer yield).
// 2026-11-05 local3: returns the FAILED pieces (bit0 stats200 incl. growth, bit1 collab slots, bit2 scenery slots); each failed
// piece's still unpublished manager registrations are taken back here, the others stay registered. The caller switches only the failed features off.
static unsigned publish_modpack(bool stats,bool growth,bool collab,bool scenery) {
    unsigned failed=0;
    if(stats && !register_stats200(growth))failed|=1;
    const size_t groups[2][2]={{0,2},{2,4}};
    for(unsigned g=0;g<2;++g) {
        if(!(g==0?collab:scenery))continue;
        unsigned mark=layout_client::current_mark();
        bool ok=true;
        for(size_t n=groups[g][0];n<groups[g][1] && ok;++n) {
            const auto &p=modpack_pointers[n];
            if(layout_client::vt(p.slot,reinterpret_cast<void *>(p.replacement),reinterpret_cast<void **>(p.next))<=0)ok=false;
        }
        if(!ok) {layout_client::rollback_to(mark);failed|=2u<<g;}
    }
    return failed;
}
#else
// No Layout manager (only the address guard fixture builds this unit unrouted, and it never publishes): nothing is applied.
static unsigned publish_modpack(bool stats,bool,bool collab,bool scenery) {return (stats?1u:0u)|(collab?2u:0u)|(scenery?4u:0u);}
#endif
