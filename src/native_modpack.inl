// v1.6 evidence and exact source fingerprints: research/modpack_v16/INTEGRATION.md.
// Only the convenience owner includes this unit. Battle deaths is own source now (native_battle_deaths.inl).
#include "game_addresses.h"
#include "stat_growth.h"
struct ModpackBytes {unsigned rva,size;unsigned char before[16],after[16];};
#include "modpack_tables.h"
static bool modpack_write(unsigned rva,const void *before,const void *after,unsigned size) {
    if(ga::mcmp_at(image,rva,before,size))return false;
    DWORD old=0,ignored=0;
    if(!VirtualProtect(image+rva,size,PAGE_EXECUTE_READWRITE,&old))return false;
    memcpy(image+rva,after,size);
    bool ok=FlushInstructionCache(GetCurrentProcess(),image+rva,size)!=FALSE;
    if(!VirtualProtect(image+rva,size,old,&ignored))ok=false;
    return ok;
}
// Undo of OUR OWN write: plain memcmp against the bytes we wrote. ga::mcmp_at ignores its expected argument on a known build
// (it compares with that build's original bytes), so it must never judge "is my patch still there" (LOCAL10_PORT_PLAN G1-a/b).
static bool modpack_revert(unsigned rva,const void *after,const void *before,unsigned size) {
    if(!rva || memcmp(image+rva,after,size))return false;
    DWORD old=0,ignored=0;
    if(!VirtualProtect(image+rva,size,PAGE_EXECUTE_READWRITE,&old))return false;
    memcpy(image+rva,before,size);
    bool ok=FlushInstructionCache(GetCurrentProcess(),image+rva,size)!=FALSE;
    if(!VirtualProtect(image+rva,size,old,&ignored))ok=false;
    return ok;
}
static void modpack_extended_growth(Obj person,unsigned ability) { COST_SCOPE("modpack_extended_growth");
    if(!person || ability>=5)return;
    unsigned current=person[0xEC+ability];if(current<100 || current>=200)return;
    Obj table=reinterpret_cast<Obj(__fastcall *)()>(image+GA(GA_3359E0))();if(!table)return;
    Obj r99=pointer(table,0x60+99*8),r100=pointer(table,0x60+100*8);
    using Valid=int(__fastcall *)(Obj);
    auto valid=reinterpret_cast<Valid>(image+GA(GA_32F4D0));
    if(!r99 || !r100 || !valid(r99) || !valid(r100))return;
    unsigned experience=*reinterpret_cast<unsigned *>(person+0x104+ability*4);
    unsigned a=*reinterpret_cast<unsigned *>(r99+0x10+ability*4);
    unsigned b=*reinterpret_cast<unsigned *>(r100+0x10+ability*4);
    person[0xEC+ability]=static_cast<unsigned char>(statgrowth::advance(current,experience,a,b));
}
static Obj modpack_relay;
static CodeLink modpack_growth_link;
// Item-0x7B clamp (stats200, 22FCE0/231770 "stat 4 with item 0x7B = 100"). Long form (1fbe6be7): JE rel8 + 6-byte NOP, patched in
// place by the modpack_bytes row (raise to 100 only below 100). Short form (1a4a447d, local10): JE rel8 + 3-byte NOP whose next
// byte is the loop head (a jump target), so the row cannot fit; the JE target MOV ESI,100 (5 bytes, reached only from that JE)
// becomes an E9 to a relay doing the same "raise to 100 only below 100" and returning past the MOV.
static CodeLink modpack_clamp_link;
static Obj modpack_clamp_relay;
static bool modpack_clamp_row(const ModpackBytes &p) {return p.rva==GA(GA_230098);}
static unsigned modpack_clamp_short() {   // RVA of the MOV ESI,100 in the short form, else 0
    unsigned r=GA(GA_230098);const unsigned char nop3[]={0x0F,0x1F,0x00},mov[]={0xBE,0x64,0x00,0x00,0x00};
    if(!r || image[r]!=0x74 || ga::mcmp_at(image,r+2,nop3,3))return 0;
    unsigned t=r+2+static_cast<unsigned>(static_cast<int>(static_cast<signed char>(image[r+1])));
    return t>r+5 && t<r+0x80 && !ga::mcmp_at(image,t,mov,5)?t:0;
}
// Long form: the row's before bytes, judged by plain memcmp (the guard hash of the function pins the window; mcmp_at would
// always pass on build 1 and let the row overwrite a short form that another mod changed at the JE target).
static bool modpack_clamp_long(const ModpackBytes &p) {return !memcmp(image+p.rva,p.before,p.size);}
static bool modpack_stats_row_ok(const ModpackBytes &p) {
    return modpack_clamp_row(p)?(modpack_clamp_short() || modpack_clamp_long(p)):!ga::mcmp_at(image,p.rva,p.before,p.size);
}
static bool prepare_modpack_clamp() {
    unsigned t=modpack_clamp_short();if(!t)return true;          // long form: the byte row does it
    if(!modpack_clamp_relay)modpack_clamp_relay=allocate_near(image+t);
    Obj p=modpack_clamp_relay;if(!p)return false;
    const unsigned char code[]={0x83,0xFE,0x64,0x7D,0x05,0xBE,0x64,0x00,0x00,0x00};   // CMP ESI,100; JGE +5; MOV ESI,100
    memcpy(p,code,sizeof(code));absolute_jump(p+sizeof(code),image+t+5);
    DWORD old=0;
    return prepare_link(modpack_clamp_link,t,0xE9,p) &&
        VirtualProtect(p,4096,PAGE_EXECUTE_READ,&old) && FlushInstructionCache(GetCurrentProcess(),p,4096);
}
static unsigned modpack_stats_published;
static bool prepare_modpack_growth() {
    modpack_relay=allocate_near(image+GA(GA_2B9F8A));if(!modpack_relay)return false;
    Obj p=modpack_relay;
    // Native RSP is aligned here. R13/R15/RSI/RDI remain native nonvolatile state.
    const unsigned char code[]={0x41,0x83,0xFD,0x64,0x72,0x3A,0x48,0x83,0xEC,0x20,
        0x48,0x89,0xF1,0x89,0xFA,0x48,0xB8};
    memcpy(p,code,sizeof(code));*reinterpret_cast<void **>(p+17)=reinterpret_cast<void *>(&modpack_extended_growth);
    const unsigned char done[]={0xFF,0xD0,0x48,0x83,0xC4,0x20};memcpy(p+25,done,sizeof(done));
    absolute_jump(p+31,image+GA(GA_2BA048));
    memcpy(p+64,image+GA(GA_2B9F8A),4); // LEA EBX,[R13+1]
    p[68]=0xE8;*reinterpret_cast<int *>(p+69)=static_cast<int>((image+GA(GA_3359E0))-(p+73));
    absolute_jump(p+73,image+GA(GA_2B9F93));
    DWORD old=0;
    return prepare_link(modpack_growth_link,GA(GA_2B9F8A),0xE9,p) &&
        VirtualProtect(p,4096,PAGE_EXECUTE_READ,&old) && FlushInstructionCache(GetCurrentProcess(),p,4096);
}
// Collab record callbacks run on native deserialization threads. The initial
// loader-time record sweep from the reference is deliberately unnecessary here.
using CollabRead=uint64_t(__fastcall *)(Obj,Obj,Obj,Obj);
static void collab_unlock(Obj object,bool trait) {
    Obj root=pointer(image,GA(GA_277DF18));if(!object || !root)return;
    unsigned table=trait?0x7DD18:0x774D0,stride=trait?0xE8:0x90;
    Obj first=pointer(root,table);if(!first || object<first)return;
    uintptr_t delta=static_cast<uintptr_t>(object-first);if(delta%stride)return;
    uintptr_t id=delta/stride;if(id>0xFFFF)return;
    const unsigned short *ids=trait?collab_traits:collab_tactics;
    size_t count=trait?sizeof(collab_traits)/2:sizeof(collab_tactics)/2;
    if(std::find(ids,ids+count,static_cast<unsigned short>(id))==ids+count)return;
    unsigned vt=trait?GA(GA_13A71F0):GA(GA_13A6DB8),off=trait?0xC8:0x70,flag=trait?0xBF:0x44;
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
    Obj root=pointer(image,GA(GA_277DF18));if(!root)return;
    for(auto id:collab_traits)collab_unlock(pointer(root,0x7DD18+id*8),true);
    for(auto id:collab_tactics)collab_unlock(pointer(root,0x774D0+id*8),false);
}
// Layout manager chain of the four record/scenery slots (design 8.0 G1; null: original game function).
static void *next_collab_trait,*next_collab_tactic,*next_scenery_tick,*next_scenery_discovery;
// Same-function peers (design 8.2, decided at guard time by split_layout_yield): set = do not apply that part.
static bool modpack_yield_collab;   // trait_hover_yield lives in native_trait_hover.inl
static uint64_t __fastcall collab_trait_read(Obj a,Obj b,Obj c,Obj d) { COST_SCOPE("modpack_collab_trait_read");
    auto result=next_or<CollabRead>(next_collab_trait,GA(GA_23EB50))(a,b,c,d);
    if(static_cast<unsigned>(result) && b && *reinterpret_cast<int *>(b+0x20)==1)collab_unlock(a,true);
    return result;
}
static uint64_t __fastcall collab_tactic_read(Obj a,Obj b,Obj c,Obj d) { COST_SCOPE("modpack_collab_tactic_read");
    auto result=next_or<CollabRead>(next_collab_tactic,GA(GA_23FB90))(a,b,c,d);
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
        Obj context=reinterpret_cast<Obj(__fastcall *)()>(image+GA(GA_3F7FA0))();
        context=reinterpret_cast<Obj(__fastcall *)(Obj)>(image+GA(GA_1D2A20))(context);
        Obj text=image+GA(GA_1A23440);
        reinterpret_cast<void(__fastcall *)(Obj)>(image+GA(GA_30A2F0))(text);
        Obj target=pointer(o,0x490);
        if(message==0x5C75) {
            Obj data=reinterpret_cast<Obj(__fastcall *)(Obj)>(image+GA(GA_22A880))(target);
            reinterpret_cast<void(__fastcall *)(Obj,unsigned,unsigned)>(image+GA(GA_30D8C0))(text,*reinterpret_cast<unsigned short *>(data+0x4E),0);
        } else if(message==0x5C76)
            reinterpret_cast<void(__fastcall *)(Obj,int,int)>(image+GA(GA_30D910))(text,*reinterpret_cast<int *>(o+0x480),0);
        unsigned id=reinterpret_cast<unsigned(__fastcall *)(Obj)>(image+GA(GA_229F80))(target);
        reinterpret_cast<void(__fastcall *)(Obj,unsigned,unsigned)>(image+GA(GA_30D9D0))(text,id,0);
        reinterpret_cast<void(__fastcall *)(Obj,Obj,unsigned)>(image+GA(GA_30DC20))(text,target+0x10,0);
        reinterpret_cast<void(__fastcall *)(Obj,int,int)>(image+GA(GA_30DD20))(text,amount,0);
        Obj formatted=reinterpret_cast<Obj(__fastcall *)(Obj,int)>(pointer(pointer(text,0),8))(text,message);
        Obj history=reinterpret_cast<Obj(__fastcall *)()>(image+GA(GA_195DE0))();
        reinterpret_cast<void(__fastcall *)(Obj,Obj,unsigned,Obj,Obj)>(image+GA(GA_196B50))(history,context,0x1000,image+GA(GA_139C730),formatted);
    }
    ++*reinterpret_cast<int *>(o+0x470);
}
static void __fastcall scenery_tick(Obj o) { COST_SCOPE("modpack_scenery_tick");
    if(pointer(o,0)==image+GA(GA_13D2ED8) && *reinterpret_cast<int *>(o+0x470)==0) {
        if(!reinterpret_cast<int(__fastcall *)(Obj)>(image+GA(GA_56FE10))(o))return;
        auto valid=reinterpret_cast<int(__fastcall *)(Obj)>(image+GA(GA_32F500));
        if(valid(pointer(o,0x490)) && valid(pointer(o,0x488))) {
            *reinterpret_cast<int *>(o+0x470)=2;scenery_result(o); // -> 3
            *reinterpret_cast<unsigned *>(o+0x574)=0xFFFFFFFFu;     // camera return already "done"
        }
    }
    next_or<void(__fastcall *)(Obj)>(next_scenery_tick,GA(GA_421990))(o);
}
static uint64_t __fastcall scenery_discovery(Obj o,Obj b,Obj c,Obj d) { COST_SCOPE("scenery_discovery");
    auto result=next_or<CollabRead>(next_scenery_discovery,GA(GA_7229C0))(o,b,c,d);
    if(static_cast<unsigned>(result) && *reinterpret_cast<int *>(o+0x4B0)==5) {
        auto hide=reinterpret_cast<void(__fastcall *)(Obj,unsigned)>(image+GA(GA_83BF80));
        hide(o,3);hide(o,4);
        reinterpret_cast<void(__fastcall *)(Obj)>(pointer(pointer(o,0),0x40))(o);
        *reinterpret_cast<int *>(o+0x50C)=2;
    }
    return result;
}
static const SplitPointer modpack_pointers[]={
    {GA(GA_13A7220),GA(GA_23EB50),reinterpret_cast<void *>(&collab_trait_read),kPointerShared,&next_collab_trait},
    {GA(GA_13A6DE8),GA(GA_23FB90),reinterpret_cast<void *>(&collab_tactic_read),kPointerShared,&next_collab_tactic},
    {GA(GA_13D2F00),GA(GA_421990),reinterpret_cast<void *>(&scenery_tick),kPointerShared,&next_scenery_tick},
    {GA(GA_14471F0),GA(GA_7229C0),reinterpret_cast<void *>(&scenery_discovery),kPointerShared,&next_scenery_discovery}
};
// Feature-specific checks. Battle deaths (native_battle_deaths.inl) and the tooltip hover (native_trait_hover.inl: trait_hover_off /
// trait_hover_yield / trait_hover_wanted()) are own source with their own guards; the collab record slots and scenery are our
// own code and stay on with tooltip_collab.
static bool modpack_guards_match(bool stats=true,bool growth=true,bool collab=true,bool scenery=true) {
    if(stats)for(const auto &p:modpack_bytes)if(!modpack_stats_row_ok(p))return false;
#ifndef LAYOUT_ROUTED   // routed: slot ownership is judged by the Layout hook manager
    for(unsigned i=0;i<4;++i)if(i<2?collab:scenery) {
        const auto &p=modpack_pointers[i];if(pointer(image,p.slot)!=image+p.original)return false;
    }
#else
    (void)collab;
#endif
    const unsigned char growth_bytes[]={0x41,0x8D,0x5D,0x01,0xE8,0x4D,0xBA,0x07,0x00};
    // 422930: MOV ECX,[RBX+574]; CMP ECX,R8D; JAE pop-branch (unsigned frame counter).
    const unsigned char scenery_wait[]={0x8B,0x8B,0x74,0x05,0x00,0x00,0x41,0x3B,0xC8,0x0F,0x83};
    // growth: LEA EBX,[R13+1]; CALL rel32. The rel32 differs per build, so the CALL target (GA_3359E0) is judged, not the displacement.
    auto growth_ok=[&]() {
        unsigned g=GA(GA_2B9F8A),c=GA(GA_3359E0);
        if(!g || !c || ga::mcmp(image,GA_2B9F8A,0,growth_bytes,5))return false;
        int rel;memcpy(&rel,image+g+5,4);
        return static_cast<int64_t>(g)+9+rel==static_cast<int64_t>(c);
    };
    return (!growth || growth_ok()) &&
        (!scenery || !ga::mcmp(image,GA_422974,0,scenery_wait,sizeof(scenery_wait)));
}
// Result: bit0 = a stats200 relay could not be prepared, bit1 = the guards no longer match (3 = both, the caller then re-checks them).
static unsigned prepare_modpack() {
    const auto &f=mod_features;
    unsigned failed=0;
    if(!modpack_guards_match(f.stats200,f.stats200_growth,f.tooltip_collab,f.scenery_skip))return 3;
    if(f.stats200_growth && !prepare_modpack_growth())failed|=1;
    if(f.stats200 && !prepare_modpack_clamp())failed|=1;
    return failed;
}
static void modpack_restore_stats() {
    restore_link(modpack_growth_link);
    for(unsigned i=0;i<modpack_stats_published;++i) {
        const auto &p=modpack_bytes[i];
        if(modpack_clamp_row(p) && modpack_clamp_link.rva) {
            if(modpack_clamp_link.published) {restore_link(modpack_clamp_link);modpack_clamp_link.published=false;}
            continue;   // short form: never write the long-form bytes over it
        }
        modpack_revert(p.rva,p.after,p.before,p.size);
    }
    modpack_stats_published=0;
}
// Pointer rows [from,to) of modpack_pointers (collab 0..1, scenery 2..3). Routed: the manager registrations are cancelled by
// split_rollback / layout_client::rollback_to.
static void modpack_restore_pointers([[maybe_unused]] size_t from,[[maybe_unused]] size_t to) {
#ifndef LAYOUT_ROUTED
    for(size_t n=from;n<to;++n)restore_pointer(modpack_pointers[n].slot,modpack_pointers[n].original,modpack_pointers[n].replacement);
#endif
}
static void restore_modpack() {
    modpack_restore_pointers(0,sizeof(modpack_pointers)/sizeof(modpack_pointers[0]));
    modpack_restore_stats();
}
// Pieces: stats200 = caps + growth gate; tooltip_collab = collab record slots 0..1 (the hover tooltip is native_trait_hover.inl, its first tick runs the collab sweep);
// scenery_skip = slots 2..3.
// 2026-11-05 local3: returns the FAILED pieces (bit0 stats200 incl. growth, bit1 collab slots, bit2 scenery slots); each failed
// piece is already taken back here, the others stay published. The caller switches only the failed features off.
static unsigned publish_modpack() {
    unsigned failed=0;
    if(mod_features.stats200) {
        bool ok=true;
        for(const auto &p:modpack_bytes) {
            // Include writes whose protection/cache finalization may fail.
            ++modpack_stats_published;
            if(modpack_clamp_row(p) && modpack_clamp_short()) {
                if(!modpack_clamp_link.rva || !publish_link(modpack_clamp_link)) {ok=false;break;}
                log_event("stats200_clamp_relay",1);
            } else if(modpack_clamp_row(p) && !modpack_clamp_long(p)) {ok=false;break;}
            else if(!modpack_write(p.rva,p.before,p.after,p.size)) {ok=false;break;}
        }
        // v12.8: natural growth past 100 is a separate (dependent) switch.
        if(ok && mod_features.stats200_growth && !publish_link(modpack_growth_link))ok=false;
        if(!ok) {modpack_restore_stats();failed|=1;}
    }
    const size_t groups[2][2]={{0,2},{2,4}};
    for(unsigned g=0;g<2;++g) {
        if(!(g==0?mod_features.tooltip_collab && !modpack_yield_collab:mod_features.scenery_skip))continue;
        unsigned mark=layout_client::current_mark();
        bool ok=true;
        for(size_t n=groups[g][0];n<groups[g][1] && ok;++n) {
            const auto &p=modpack_pointers[n];
            if(!ROUTE_VT(p.slot,p.original,p.replacement,p.next))ok=false;
        }
        if(!ok) {modpack_restore_pointers(groups[g][0],groups[g][1]);layout_client::rollback_to(mark);failed|=2u<<g;}
    }
    return failed;
}
#include "native_battle_deaths.inl"
#include "native_trait_hover.inl"
#include "native_traits9.inl"
