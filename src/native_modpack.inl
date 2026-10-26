// v1.6 evidence and exact source fingerprints: research/modpack_v16/INTEGRATION.md.
// Only the convenience owner includes this unit. Imported native code is linked
// as COFF; no additional DLL, PE loader, DllMain or worker is used.
#include "stat_growth.h"
struct ImportedSection {unsigned offset,size,protection;};
struct ImportedCode {Obj base;unsigned size,entry,exceptions,count;const ImportedSection *sections;size_t section_count;};
#include "modpack_payloads.h"
struct ModpackBytes {unsigned rva,size;unsigned char before[16],after[16];};
#include "modpack_tables.h"
static bool imported_ready;
struct ImportedPointer {unsigned slot;Obj original,modified;};
static ImportedPointer imported_pointers[sizeof(hover_slots)/sizeof(unsigned)];
struct ImportedPatch {unsigned rva,size;unsigned char original[8],modified[8];};
static ImportedPatch imported_patches[]={{0x259682,5,{},{}},{0x27D976,5,{},{}},
    {0x25974B,5,{},{}},{0x27DA09,5,{},{}},{0x295C5F,8,{},{}}};
// AI-CONTRACT [external provenance]: Payload code is imported from fingerprinted third-party DLLs, not reconstructed C++. Keep exception-table registration, section protection and expected hashes; project permission does not relicense the external payload.
static bool prepare_imported(const ImportedCode &code) {
    if(!RtlAddFunctionTable(reinterpret_cast<PRUNTIME_FUNCTION>(code.base+code.exceptions),code.count,
                            reinterpret_cast<DWORD64>(code.base)-0x1000))return false;
    for(size_t n=0;n<code.section_count;++n) {
        const auto &s=code.sections[n];DWORD old=0;
        if(s.size && !VirtualProtect(code.base+s.offset,s.size,s.protection,&old))return false;
    }
    return FlushInstructionCache(GetCurrentProcess(),code.base,code.size)!=FALSE;
}
static bool modpack_write(unsigned rva,const void *before,const void *after,unsigned size) {
    if(memcmp(image+rva,before,size))return false;
    DWORD old=0,ignored=0;
    if(!VirtualProtect(image+rva,size,PAGE_EXECUTE_READWRITE,&old))return false;
    memcpy(image+rva,after,size);
    bool ok=FlushInstructionCache(GetCurrentProcess(),image+rva,size)!=FALSE;
    if(!VirtualProtect(image+rva,size,old,&ignored))ok=false;
    return ok;
}
static void modpack_extended_growth(Obj person,unsigned ability) {
    if(!person || ability>=5)return;
    unsigned current=person[0xEC+ability];if(current<100 || current>=200)return;
    Obj table=reinterpret_cast<Obj(__fastcall *)()>(image+0x3359E0)();if(!table)return;
    Obj r99=pointer(table,0x60+99*8),r100=pointer(table,0x60+100*8);
    using Valid=int(__fastcall *)(Obj);
    auto valid=reinterpret_cast<Valid>(image+0x32F4D0);
    if(!r99 || !r100 || !valid(r99) || !valid(r100))return;
    unsigned experience=*reinterpret_cast<unsigned *>(person+0x104+ability*4);
    unsigned a=*reinterpret_cast<unsigned *>(r99+0x10+ability*4);
    unsigned b=*reinterpret_cast<unsigned *>(r100+0x10+ability*4);
    person[0xEC+ability]=static_cast<unsigned char>(statgrowth::advance(current,experience,a,b));
}
static Obj modpack_relay;
static CodeLink modpack_growth_link;
static unsigned modpack_stats_published;
static bool prepare_modpack_growth() {
    modpack_relay=allocate_near(image+0x2B9F8A);if(!modpack_relay)return false;
    Obj p=modpack_relay;
    // Native RSP is aligned here. R13/R15/RSI/RDI remain native nonvolatile state.
    const unsigned char code[]={0x41,0x83,0xFD,0x64,0x72,0x3A,0x48,0x83,0xEC,0x20,
        0x48,0x89,0xF1,0x89,0xFA,0x48,0xB8};
    memcpy(p,code,sizeof(code));*reinterpret_cast<void **>(p+17)=reinterpret_cast<void *>(&modpack_extended_growth);
    const unsigned char done[]={0xFF,0xD0,0x48,0x83,0xC4,0x20};memcpy(p+25,done,sizeof(done));
    absolute_jump(p+31,image+0x2BA048);
    memcpy(p+64,image+0x2B9F8A,4); // LEA EBX,[R13+1]
    p[68]=0xE8;*reinterpret_cast<int *>(p+69)=static_cast<int>((image+0x3359E0)-(p+73));
    absolute_jump(p+73,image+0x2B9F93);
    DWORD old=0;
    return prepare_link(modpack_growth_link,0x2B9F8A,0xE9,p) &&
        VirtualProtect(p,4096,PAGE_EXECUTE_READ,&old) && FlushInstructionCache(GetCurrentProcess(),p,4096);
}
// Collab record callbacks run on native deserialization threads. The initial
// loader-time record sweep from the reference is deliberately unnecessary here.
using CollabRead=uint64_t(__fastcall *)(Obj,Obj,Obj,Obj);
static void collab_unlock(Obj object,bool trait) {
    Obj root=pointer(image,0x277DF18);if(!object || !root)return;
    unsigned table=trait?0x7DD18:0x774D0,stride=trait?0xE8:0x90;
    Obj first=pointer(root,table);if(!first || object<first)return;
    uintptr_t delta=static_cast<uintptr_t>(object-first);if(delta%stride)return;
    uintptr_t id=delta/stride;if(id>0xFFFF)return;
    const unsigned short *ids=trait?collab_traits:collab_tactics;
    size_t count=trait?sizeof(collab_traits)/2:sizeof(collab_tactics)/2;
    if(std::find(ids,ids+count,static_cast<unsigned short>(id))==ids+count)return;
    unsigned vt=trait?0x13A71F0:0x13A6DB8,off=trait?0xC8:0x70,flag=trait?0xBF:0x44;
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
extern "C" void SAN14ModpackCollabBootstrap() {
    Obj root=pointer(image,0x277DF18);if(!root)return;
    for(auto id:collab_traits)collab_unlock(pointer(root,0x7DD18+id*8),true);
    for(auto id:collab_tactics)collab_unlock(pointer(root,0x774D0+id*8),false);
}
static uint64_t __fastcall collab_trait_read(Obj a,Obj b,Obj c,Obj d) { COST_SCOPE("modpack_collab_trait_read");
    auto result=reinterpret_cast<CollabRead>(image+0x23EB50)(a,b,c,d);
    if(static_cast<unsigned>(result) && b && *reinterpret_cast<int *>(b+0x20)==1)collab_unlock(a,true);
    return result;
}
static uint64_t __fastcall collab_tactic_read(Obj a,Obj b,Obj c,Obj d) { COST_SCOPE("modpack_collab_tactic_read");
    auto result=reinterpret_cast<CollabRead>(image+0x23FB90)(a,b,c,d);
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
        Obj context=reinterpret_cast<Obj(__fastcall *)()>(image+0x3F7FA0)();
        context=reinterpret_cast<Obj(__fastcall *)(Obj)>(image+0x1D2A20)(context);
        Obj text=image+0x1A23440;
        reinterpret_cast<void(__fastcall *)(Obj)>(image+0x30A2F0)(text);
        Obj target=pointer(o,0x490);
        if(message==0x5C75) {
            Obj data=reinterpret_cast<Obj(__fastcall *)(Obj)>(image+0x22A880)(target);
            reinterpret_cast<void(__fastcall *)(Obj,unsigned,unsigned)>(image+0x30D8C0)(text,*reinterpret_cast<unsigned short *>(data+0x4E),0);
        } else if(message==0x5C76)
            reinterpret_cast<void(__fastcall *)(Obj,int,int)>(image+0x30D910)(text,*reinterpret_cast<int *>(o+0x480),0);
        unsigned id=reinterpret_cast<unsigned(__fastcall *)(Obj)>(image+0x229F80)(target);
        reinterpret_cast<void(__fastcall *)(Obj,unsigned,unsigned)>(image+0x30D9D0)(text,id,0);
        reinterpret_cast<void(__fastcall *)(Obj,Obj,unsigned)>(image+0x30DC20)(text,target+0x10,0);
        reinterpret_cast<void(__fastcall *)(Obj,int,int)>(image+0x30DD20)(text,amount,0);
        Obj formatted=reinterpret_cast<Obj(__fastcall *)(Obj,int)>(pointer(pointer(text,0),8))(text,message);
        Obj history=reinterpret_cast<Obj(__fastcall *)()>(image+0x195DE0)();
        reinterpret_cast<void(__fastcall *)(Obj,Obj,unsigned,Obj,Obj)>(image+0x196B50)(history,context,0x1000,image+0x139C730,formatted);
    }
    ++*reinterpret_cast<int *>(o+0x470);
}
static void __fastcall scenery_tick(Obj o) { COST_SCOPE("modpack_scenery_tick");
    if(pointer(o,0)==image+0x13D2ED8 && *reinterpret_cast<int *>(o+0x470)==0) {
        if(!reinterpret_cast<int(__fastcall *)(Obj)>(image+0x56FE10)(o))return;
        auto valid=reinterpret_cast<int(__fastcall *)(Obj)>(image+0x32F500);
        if(valid(pointer(o,0x490)) && valid(pointer(o,0x488))) {
            *reinterpret_cast<int *>(o+0x470)=2;scenery_result(o); // -> 3
            *reinterpret_cast<unsigned *>(o+0x574)=0xFFFFFFFFu;     // camera return already "done"
        }
    }
    reinterpret_cast<void(__fastcall *)(Obj)>(image+0x421990)(o);
}
static uint64_t __fastcall scenery_discovery(Obj o,Obj b,Obj c,Obj d) {
    auto result=reinterpret_cast<CollabRead>(image+0x7229C0)(o,b,c,d);
    if(static_cast<unsigned>(result) && *reinterpret_cast<int *>(o+0x4B0)==5) {
        auto hide=reinterpret_cast<void(__fastcall *)(Obj,unsigned)>(image+0x83BF80);
        hide(o,3);hide(o,4);
        reinterpret_cast<void(__fastcall *)(Obj)>(pointer(pointer(o,0),0x40))(o);
        *reinterpret_cast<int *>(o+0x50C)=2;
    }
    return result;
}
static const SplitPointer modpack_pointers[]={
    {0x13A7220,0x23EB50,reinterpret_cast<void *>(&collab_trait_read)},
    {0x13A6DE8,0x23FB90,reinterpret_cast<void *>(&collab_tactic_read)},
    {0x13D2F00,0x421990,reinterpret_cast<void *>(&scenery_tick)},
    {0x14471F0,0x7229C0,reinterpret_cast<void *>(&scenery_discovery)}
};
// Feature-specific checks: imported hover/death installers have no complete
// current-game hook-site fingerprints here, so the coordinator disables those
// two features in compat. Strict mode retains the existing imported installers.
static bool modpack_guards_match(bool stats=true,bool growth=true,bool collab=true,bool scenery=true) {
    if(stats)for(const auto &p:modpack_bytes)if(memcmp(image+p.rva,p.before,p.size))return false;
    for(unsigned i=0;i<4;++i)if(i<2?collab:scenery) {
        const auto &p=modpack_pointers[i];if(pointer(image,p.slot)!=image+p.original)return false;
    }
    const unsigned char growth_bytes[]={0x41,0x8D,0x5D,0x01,0xE8,0x4D,0xBA,0x07,0x00};
    // 422930: MOV ECX,[RBX+574]; CMP ECX,R8D; JAE pop-branch (unsigned frame counter).
    const unsigned char scenery_wait[]={0x8B,0x8B,0x74,0x05,0x00,0x00,0x41,0x3B,0xC8,0x0F,0x83};
    return (!growth || !memcmp(image+0x2B9F8A,growth_bytes,sizeof(growth_bytes))) &&
        (!scenery || !memcmp(image+0x422974,scenery_wait,sizeof(scenery_wait)));
}
static bool prepare_modpack() {
    const auto &f=mod_features;
    if(!modpack_guards_match(f.stats200,f.stats200_growth,f.tooltip_collab,f.scenery_skip))return false;
    if((f.tooltip_collab || f.battle_deaths) && !imported_ready) {
        if(!prepare_imported(hover_code) || !prepare_imported(deaths_code))return false;
        imported_ready=true;
    }
    return !f.stats200_growth || prepare_modpack_growth();
}
static void restore_modpack() {
    for(const auto &p:imported_pointers)if(p.original && p.modified && pointer(image,p.slot)==p.modified) {
        DWORD old=0,ignored=0;
        if(VirtualProtect(image+p.slot,8,PAGE_READWRITE,&old)) {
            InterlockedCompareExchangePointer(reinterpret_cast<void *volatile *>(image+p.slot),p.original,p.modified);
            VirtualProtect(image+p.slot,8,old,&ignored);
        }
    }
    for(const auto &p:imported_patches)if(p.modified[0])modpack_write(p.rva,p.modified,p.original,p.size);
    for(const auto &p:modpack_pointers)restore_pointer(p.slot,p.original,p.replacement);
    restore_link(modpack_growth_link);
    for(unsigned i=0;i<modpack_stats_published;++i) {
        const auto &p=modpack_bytes[i];modpack_write(p.rva,p.after,p.before,p.size);
    }
    modpack_stats_published=0;
}
// Pieces: stats200 = caps + growth gate; tooltip_collab = imported tooltip code
// (its UI init runs the collab first unlock) + collab record slots 0..1;
// scenery_skip = slots 2..3; battle_deaths = imported death-scene code.
static bool publish_modpack() {
    if(mod_features.stats200) {
        for(const auto &p:modpack_bytes) {
            // Include writes whose protection/cache finalization may fail.
            ++modpack_stats_published;
            if(!modpack_write(p.rva,p.before,p.after,p.size))return false;
        }
        // v12.8: natural growth past 100 is a separate (dependent) switch.
        if(mod_features.stats200_growth && !publish_link(modpack_growth_link))return false;
    }
    for(size_t n=0;n<sizeof(modpack_pointers)/sizeof(modpack_pointers[0]);++n) {
        const auto &p=modpack_pointers[n];
        if(!(n<2?mod_features.tooltip_collab:mod_features.scenery_skip))continue;
        if(!patch_pointer(p.slot,p.original,p.replacement))return false;
    }
    return true;
}
static bool activate_modpack() {
    using Apply=unsigned(__cdecl *)();
    for(size_t n=0;n<sizeof(hover_slots)/sizeof(unsigned);++n)
        imported_pointers[n]={hover_slots[n],pointer(image,hover_slots[n]),nullptr};
    for(auto &p:imported_patches)memcpy(p.original,image+p.rva,p.size);
    unsigned hover=1,deaths=1;
    if(mod_features.tooltip_collab) {
        hover=reinterpret_cast<Apply>(hover_code.base+hover_code.entry)();
        for(auto &p:imported_pointers)p.modified=pointer(image,p.slot);
    }
    if(mod_features.battle_deaths) {
        deaths=reinterpret_cast<Apply>(deaths_code.base+deaths_code.entry)();
        for(auto &p:imported_patches)memcpy(p.modified,image+p.rva,p.size);
    }
    return hover!=0 && deaths!=0;
}
#include "native_traits9.inl"
