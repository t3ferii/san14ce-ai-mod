// SAN14_Layout.dll hook manager (design 4.3 C-1..C-7). User exception 2026-11-05.
// Every code or vtable write goes through this file: vtable slots are written with an 8-byte CAS,
// code with a 16-byte CMPXCHG16B block (a CALL whose rel32 crosses the block is one guarded write),
// and every decision is logged once as installed / chained / yielded / refused / bypassed.
// Nothing here calls a game function; all writes run on the thread that calls the API.
#include "game_addresses.h"
#include "compat_report.h"   // user 2026-11-09: refusals/yields also go to ai행동교정모드호환성.log
namespace layout {
using Bytes=unsigned char;
static Bytes *image;
static size_t image_size;
static HMODULE self_module;
static volatile LONG published;
static SRWLOCK table_lock=SRWLOCK_INIT;
// ---- log: host module's common log_event writer, buffered until it is connected ----
static SRWLOCK log_lock=SRWLOCK_INIT;
static San14LayoutLogSink sink;
#ifndef NO_RUNTIME_LOG
struct Buffered {char kind[40];unsigned a,b,c;};
static Buffered ring[64];
static unsigned ring_count;
#endif
static unsigned hash32(const char *text) {
    unsigned h=2166136261u;
    for(;text && *text;++text)h=(h^static_cast<unsigned char>(*text))*16777619u;
    return h;
}
static void emit(const char *kind,unsigned a,unsigned b,unsigned c) {
#ifndef NO_RUNTIME_LOG
    AcquireSRWLockExclusive(&log_lock);
    if(sink)sink(kind,a,b,c);
    else if(ring_count<sizeof(ring)/sizeof(ring[0])) {
        auto &r=ring[ring_count++];strncpy_s(r.kind,sizeof(r.kind),kind,_TRUNCATE);r.a=a;r.b=b;r.c=c;
    }
    ReleaseSRWLockExclusive(&log_lock);
#else
    (void)kind;(void)a;(void)b;(void)c;
#endif
}
static void set_sink(San14LayoutLogSink next) {
#ifndef NO_RUNTIME_LOG
    AcquireSRWLockExclusive(&log_lock);
    sink=next;
    if(sink) {
        for(unsigned i=0;i<ring_count;++i)sink(ring[i].kind,ring[i].a,ring[i].b,ring[i].c);
        ring_count=0;
    }
    ReleaseSRWLockExclusive(&log_lock);
#else
    sink=next;
#endif
}
// ---- who owns a pointer ----
enum class Kind {None,Game,Self,Ours,Foreign};
struct Where {Kind kind;wchar_t name[64];};
#ifdef LAYOUT_FIXTURE
struct FakeModule {const Bytes *base;size_t size;wchar_t name[64];};
static FakeModule fake_modules[8];
static unsigned fake_count;
static wchar_t fixture_root[MAX_PATH];
#endif
static bool is_our_dll(const wchar_t *name) {
    static const wchar_t *const names[]={L"SAN14_Convenience.dll",L"SAN14_CombatAI.dll",L"SAN14_CCZ.dll"};
    for(const wchar_t *n:names)if(!_wcsicmp(name,n))return true;
    return false;
}
static Where module_of(const void *p) {
    Where w={Kind::None,{}};
    if(!p)return w;
    auto at=static_cast<const Bytes *>(p);
#ifdef LAYOUT_FIXTURE
    for(unsigned i=0;i<fake_count;++i)
        if(at>=fake_modules[i].base && at<fake_modules[i].base+fake_modules[i].size) {
            wcscpy_s(w.name,fake_modules[i].name);
            w.kind=is_our_dll(w.name)?Kind::Ours:Kind::Foreign;return w;
        }
#endif
    if(image && at>=image && at<image+image_size) {w.kind=Kind::Game;wcscpy_s(w.name,L"game");return w;}
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        static_cast<LPCWSTR>(p),&module) || !module)return w;
    if(module==self_module) {w.kind=Kind::Self;wcscpy_s(w.name,L"SAN14_Layout.dll");return w;}
    if(module==GetModuleHandleW(nullptr)) {w.kind=Kind::Game;wcscpy_s(w.name,L"game");return w;}
    wchar_t path[MAX_PATH];
    DWORD length=GetModuleFileNameW(module,path,MAX_PATH);
    if(!length || length>=MAX_PATH) {w.kind=Kind::Foreign;wcscpy_s(w.name,L"?");return w;}
    const wchar_t *base=wcsrchr(path,L'\\');base=base?base+1:path;
    wcsncpy_s(w.name,base,_TRUNCATE);
    w.kind=is_our_dll(w.name)?Kind::Ours:Kind::Foreign;
    return w;
}
enum class Verdict {Original,Ours,Foreign,Heap,OtherOwn};
// C-2: original -> install; our own value -> nothing to do; another module image -> chain
// candidate; heap/unmapped -> refuse; another SAN14 DLL's value (a second owner) -> refuse.
static Verdict classify(const void *value,const void *original,const void *replacement,const wchar_t *original_module) {
    if(original && value==original)return Verdict::Original;
    if(replacement && value==replacement)return Verdict::Ours;
    Where w=module_of(value);
    switch(w.kind) {
    case Kind::None:return Verdict::Heap;
    case Kind::Self:case Kind::Ours:return Verdict::OtherOwn;
    case Kind::Game:return Verdict::Foreign;
    default:break;
    }
    if(original_module && !_wcsicmp(w.name,original_module))return Verdict::Original;
    return Verdict::Foreign;
}
// ---- compatibility detection ----
static bool game_directory(wchar_t *out) {
#ifdef LAYOUT_FIXTURE
    if(fixture_root[0])return !wcscpy_s(out,MAX_PATH,fixture_root);
#endif
    DWORD length=GetModuleFileNameW(nullptr,out,MAX_PATH);
    if(!length || length>=MAX_PATH)return false;
    wchar_t *slash=wcsrchr(out,L'\\');if(!slash)return false;*slash=0;return true;
}
// R1 (integrated patch) modules are switched by <game>\SAN14Mods\modules\<Name>.ini [Mod] Enabled.
static int peer_present_impl(const wchar_t *name) {
    if(!name || !*name)return 0;
    size_t length=wcslen(name);
    if(length>4 && !_wcsicmp(name+length-4,L".ini")) {
        wchar_t path[MAX_PATH];
        if(!game_directory(path) || wcscat_s(path,L"\\SAN14Mods\\modules\\") || wcscat_s(path,name))return 0;
        if(GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)return 0;
        return GetPrivateProfileIntW(L"Mod",L"Enabled",0,path)!=0?1:0;
    }
    return GetModuleHandleW(name)?1:0;
}
// Same-function peers (Q2/Q8, design 8.2). An owner yields when ANY of its rules holds; a rule holds when ALL its
// conditions hold. Conditions read files under the game folder only (ini keys, file existence) or a loaded module.
// R1 module inis are <game>\SAN14Mods\modules\<Name>.ini [Mod] Enabled; a missing file/key counts as the default.
// Owner names are fixed by the P3 registry (src/layout_registry.h); unknown owners never yield.
enum class Cond {Ini,File,Module};
struct PeerCond {Cond kind;const wchar_t *path;const wchar_t *section;const wchar_t *key;int fallback;};
struct PeerRule {const char *owner;unsigned count;PeerCond conds[3];};
#define R1_MOD(file) {Cond::Ini,L"SAN14Mods\\modules\\" file,L"Mod",L"Enabled",0}
#define R1_BRIDGE(key) {Cond::Ini,L"SAN14Mods\\modules\\ModPackBridge.ini",L"ModPack",key,0}
static const PeerRule peer_rules[]={
    {"traits9",1,{R1_MOD(L"Traits9Check.ini")}},
    {"trait_hover",2,{R1_MOD(L"ModPackBridge.ini"),R1_BRIDGE(L"TraitHover")}},
    {"collab",2,{R1_MOD(L"ModPackBridge.ini"),R1_BRIDGE(L"Collab")}},
    {"stats200",2,{R1_MOD(L"ModPackBridge.ini"),R1_BRIDGE(L"Stats200")}},
    {"battle_deaths",2,{R1_MOD(L"Patch27.ini"),{Cond::Ini,L"SAN14Mods\\Patch27\\Options.ini",L"Features",L"Patch27Features",0}}},
    {"scenery",1,{R1_MOD(L"SettingsHelpScenic.ini")}},
    {"region_officers",2,{R1_MOD(L"RegionOfficer.ini"),{Cond::Ini,L"SAN14Mods\\modules\\RegionOfficer.ini",L"RegionOfficer",L"Face",0}}},
    {"domestic_selection",1,{R1_MOD(L"DomesticAI.ini")}},
    {"merit_prisoner",1,{R1_MOD(L"PrisonerReturn.ini")}},
    {"scene_yield",1,{R1_MOD(L"SceneSkip.ini")}},
    {"scene_yield",1,{{Cond::Module,L"SAN14_SceneSkip.dll",nullptr,nullptr,0}}},
    // Q8: R1 progress-speed extension (Integrated Convenience ProgressSpeedModule) patches the same 898957 tail jump.
    {"speed_buttons",2,{R1_MOD(L"IntegratedConvenience_GreenlandCompat.ini"),{Cond::Ini,L"San14ModSettings.ini",L"Convenience",L"ProgressSpeedModule",0}}},
    // Q8: R1 FPS120 may coexist; only recorded (owner is never given the YIELD flag).
    {"fps120_note",1,{R1_MOD(L"FPS120.ini")}},
    // 8.7: R1 FastMenu (ExtBridge) does the same busy-slot / layout-pin job.
    {"fast_menu",3,{{Cond::Ini,L"SAN14Mods\\Ext\\SAN14_FastMenu.ini",L"FastMenu",L"Enabled",0},R1_MOD(L"ExtBridge.ini"),
        {Cond::Ini,L"SAN14Mods\\modules\\ExtBridge.ini",L"Ext",L"FastMenu",0}}},
    {"layout_keep",3,{{Cond::Ini,L"SAN14Mods\\Ext\\SAN14_FastMenu.ini",L"FastMenu",L"PinSlowLayouts",0},R1_MOD(L"ExtBridge.ini"),
        {Cond::Ini,L"SAN14Mods\\modules\\ExtBridge.ini",L"Ext",L"FastMenu",0}}},
    // U35 (2026-11-10): R1 IC memcmp's the 870470 wrapper body (design C7), so the wrapper E8 yields to any R1 install.
    {"layout_keep_wrap",1,{{Cond::File,L"SAN14Mods",nullptr,nullptr,0}}},
    // U35 local29: preserve prior R1 portrait ownership/fingerprints conservatively.
    {"face_keep_portrait",1,{{Cond::File,L"SAN14Mods",nullptr,nullptr,0}}},
    // mediation_plus needs no rule: its 70E9C1 block hook refuses when the bytes are not original (8.4, 8.7).
    // 8.2 file-existence form for the Batteki / Yuga mediation rule (xinput1_3 load order makes GetModuleHandle unreliable).
    // No registry row uses it: with 6D7D90 handed over only the 70E9C1 byte check remains; kept for a fallback that yields 6D7D90.
    {"mediation_fallback",2,{{Cond::File,L"SAN14_Batteki.dll",nullptr,nullptr,0},{Cond::Ini,L"San14ModSettings.ini",L"Batteki",L"Mediation",1}}},
    {"mediation_fallback",3,{{Cond::File,L"SAN14_Yuga.dll",nullptr,nullptr,0},{Cond::Ini,L"San14ModSettings.ini",L"Yuga",L"Enable",1},
        {Cond::Ini,L"San14ModSettings.ini",L"Yuga",L"Mediation",1}}},
};
#undef R1_MOD
#undef R1_BRIDGE
static bool cond_holds(const PeerCond &c) {
    if(c.kind==Cond::Module)return GetModuleHandleW(c.path)!=nullptr;
    wchar_t path[MAX_PATH];
    if(!game_directory(path) || wcscat_s(path,L"\\") || wcscat_s(path,c.path))return false;
    if(c.kind==Cond::File)return GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES;
    return GetPrivateProfileIntW(c.section,c.key,c.fallback,path)!=0;
}
static bool owner_has_peer(const char *owner) {
    for(const auto &r:peer_rules) {
        if(strcmp(owner,r.owner))continue;
        bool all=true;
        for(unsigned i=0;i<r.count && all;++i)all=cond_holds(r.conds[i]);
        if(all)return true;
    }
    return false;
}
// The other mod behind a yield: its module DLL or its R1 ini (for the compat log only).
static const wchar_t *peer_source(const char *owner) {
    for(const auto &r:peer_rules) {
        if(strcmp(owner,r.owner))continue;
        bool all=true;
        for(unsigned i=0;i<r.count && all;++i)all=cond_holds(r.conds[i]);
        if(!all)continue;
        for(unsigned i=0;i<r.count;++i)if(r.conds[i].kind==Cond::Module)return r.conds[i].path;
        return r.conds[0].path;
    }
    return nullptr;
}
// ---- code / pointer writes ----
static bool protect_write(void *at,size_t size,bool (*op)(void *,const void *,const void *),const void *expected,const void *desired) {
    DWORD old=0,restored=0;
    if(!VirtualProtect(at,size,PAGE_EXECUTE_READWRITE,&old))return false;
    bool ok=op(at,expected,desired);
    VirtualProtect(at,size,old,&restored);
    FlushInstructionCache(GetCurrentProcess(),at,size);
    return ok;
}
static bool op_cas8(void *at,const void *expected,const void *desired) {
    void *want=*static_cast<void *const *>(expected),*put=*static_cast<void *const *>(desired);
    return InterlockedCompareExchangePointer(static_cast<void *volatile *>(at),put,want)==want;
}
static bool op_cas16(void *at,const void *expected,const void *desired) {
    alignas(16) LONG64 comparison[2],value[2];
    memcpy(comparison,expected,16);memcpy(value,desired,16);
    return _InterlockedCompareExchange128(static_cast<volatile LONG64 *>(at),value[1],value[0],comparison)!=0;
}
static bool op_raw(void *at,const void *expected,const void *desired) {
    if(memcmp(at,expected,4))return false;
    memcpy(at,desired,4);return true;
}
static bool cas_pointer(void **slot,void *expected,void *desired) {
    return protect_write(slot,sizeof(void *),op_cas8,&expected,&desired);
}
static bool cas_block16(Bytes *block,const Bytes *expected,const Bytes *desired) {
    if(!IsProcessorFeaturePresent(PF_COMPARE_EXCHANGE128))return false;
    return protect_write(block,16,op_cas16,expected,desired);
}
// Multi-block range write (hook_span / hook_bytes): every covering 16-byte block is one CMPXCHG16B; when a later
// block fails the earlier ones are put back. Patches longer than one block are not atomic as a whole (the
// callers register them before the patched path runs; same as the former modpack_write).
#ifdef LAYOUT_FIXTURE
static void (*range_write_probe)(unsigned block);   // fixture seam: runs after each block CAS (inject a foreign write)
#endif
// stuck (optional): set when a later block failed AND restoring an earlier block also failed (review F-02): the bytes are then neither
// ours nor the original, and the caller must not treat the failure as a clean refusal.
static bool range_write(Bytes *at,const Bytes *expected,const Bytes *put,unsigned len,bool *stuck=nullptr) {
    if(stuck)*stuck=false;
    Bytes *first=reinterpret_cast<Bytes *>(reinterpret_cast<uintptr_t>(at)&~uintptr_t(15));
    Bytes *last=reinterpret_cast<Bytes *>(reinterpret_cast<uintptr_t>(at+len-1)&~uintptr_t(15));
    unsigned blocks=static_cast<unsigned>((last-first)/16)+1,done=0;
    if(blocks>3)return false;
    Bytes before[3][16],after[3][16];
    for(unsigned b=0;b<blocks;++b) {
        Bytes *block=first+b*16;memcpy(before[b],block,16);memcpy(after[b],block,16);
        for(unsigned i=0;i<16;++i) {
            Bytes *p=block+i;
            if(p>=at && p<at+len) {
                unsigned n=static_cast<unsigned>(p-at);
                if(*p!=expected[n])return false;
                after[b][i]=put[n];
            }
        }
    }
    for(;done<blocks;++done) {
        if(!cas_block16(first+done*16,before[done],after[done]))break;
#ifdef LAYOUT_FIXTURE
        if(range_write_probe)range_write_probe(done);
#endif
    }
    if(done==blocks)return true;
    while(done>0) {
        --done;
        // Never overwrite foreign bytes: the CAS puts the original back only while the block still holds our patch.
        if(!cas_block16(first+done*16,after[done],before[done]) && stuck)*stuck=true;
    }
    return false;
}
// ---- relay pool near the image (E8 sites reach +-2 GB) ----
static Bytes *pool;
static unsigned pool_used;
static const unsigned kPoolSize=0x1000,kRelay=16;
static Bytes *allocate_near(Bytes *anchor,unsigned bytes) {
    SYSTEM_INFO info;GetSystemInfo(&info);
    uintptr_t start=reinterpret_cast<uintptr_t>(anchor)&~(uintptr_t(info.dwAllocationGranularity)-1);
    for(uintptr_t offset=info.dwAllocationGranularity;offset<0x70000000;offset+=info.dwAllocationGranularity) {
        for(int sign:{1,-1}) {
            uintptr_t address=sign>0?start+offset:(start>offset?start-offset:0);
            if(!address)continue;
            void *memory=VirtualAlloc(reinterpret_cast<void *>(address),bytes,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
            if(memory)return static_cast<Bytes *>(memory);
        }
    }
    return nullptr;
}
static bool in_pool(const void *p) {
    auto at=static_cast<const Bytes *>(p);return pool && at>=pool && at<pool+kPoolSize;
}
static Bytes *pool_relay(void *target) {
    if(!pool) {pool=allocate_near(image,kPoolSize);pool_used=0;}
    if(!pool || pool_used+kRelay>kPoolSize)return nullptr;
    Bytes *relay=pool+pool_used;pool_used+=kRelay;
    relay[0]=0xFF;relay[1]=0x25;memset(relay+2,0,4);
    *reinterpret_cast<void **>(relay+6)=target;
    FlushInstructionCache(GetCurrentProcess(),relay,kRelay);
    return relay;
}
static void *relay_target(const Bytes *relay) {return *reinterpret_cast<void *const *>(relay+6);}
// ---- hook table ----
enum HookKind : unsigned {kVtable=1,kCall,kBlock,kAbsSlot};
struct Hook {
    HookKind kind;
    volatile LONG state;
    char owner[32];
    uint32_t rva,original_rva,flags,len,reason;
    void *replacement;void **next_out;void *next;
    void *previous;               // slot value / rel32 target to restore on unhook
    Bytes *site;                  // call site or block start
    Bytes *relay;                 // call: pool relay; block: host relay
    void **slot;                  // vtable / abs slot address
    int old_rel,new_rel;
    Bytes opcode;                 // call/jump kind: 0xE8 CALL or 0xE9 JMP rel32
    Bytes expect[32],patch[32];   // block/span/bytes hooks: original bytes and replacement bytes
    bool raw;                     // hook_bytes: patch is applied as given (no E9 relay)
    const wchar_t *original_module;
};
static const unsigned kMaxHooks=384;   // 261 registry rows (105 + 156 trait_hover) + 13 traits9 patches + 15 stats200 registrations + 70E9C1 + Present, with room for peers
static Hook hooks[kMaxHooks];
static unsigned hook_total;
static const char *state_event(LONG state) {
    switch(state) {
    case LAYOUT_STATE_INSTALLED:return "layout_installed";
    case LAYOUT_STATE_CHAINED:return "layout_chained";
    case LAYOUT_STATE_YIELDED:return "layout_yielded";
    case LAYOUT_STATE_REFUSED:return "layout_refused";
    case LAYOUT_STATE_BYPASSED:return "layout_bypassed";
    case LAYOUT_STATE_REMOVED:return "layout_removed";
    case LAYOUT_STATE_STUCK:return "layout_stuck";
    default:return "layout_pending";
    }
}
static unsigned hook_address(const Hook &h) {
    return h.kind==kAbsSlot?static_cast<unsigned>(reinterpret_cast<uintptr_t>(h.slot)):h.rva;
}
// Slot value / call target a hook contended with, for naming the other module in the compat log.
static const void *hooked_value(const Hook &h) {
    if(h.kind==kVtable || h.kind==kAbsSlot)return h.slot?*h.slot:nullptr;
    if(h.kind==kCall && image && h.rva+5<=image_size && (image[h.rva]==0xE8 || image[h.rva]==0xE9))
        return image+h.rva+5+*reinterpret_cast<int *>(image+h.rva+1);
    return nullptr;
}
static void report_decision(const Hook &h,LONG state,uint32_t why) {
    wchar_t owner[32],other[64];
    unsigned length=0;
    for(;length<31 && h.owner[length];++length)owner[length]=static_cast<wchar_t>(static_cast<unsigned char>(h.owner[length]));
    owner[length]=0;
    const wchar_t *other_name=nullptr;
    if(state==LAYOUT_STATE_YIELDED)other_name=peer_source(h.owner);
    else {
        Where w=module_of(hooked_value(h));
        if(w.kind==Kind::Foreign || w.kind==Kind::Ours) {
            wcsncpy_s(other,w.name,_TRUNCATE);
            other_name=other;
        }
    }
    compat_report::add(owner,hook_address(h),why,other_name);
}
static void set_state(Hook &h,LONG state,uint32_t why) {
    h.reason=why;InterlockedExchange(&h.state,state);
    emit(state_event(state),hook_address(h),why,hash32(h.owner));
    // Refusals and yields caused by another mod / a changed original go to the one compat log.
    if(state==LAYOUT_STATE_REFUSED || state==LAYOUT_STATE_YIELDED)report_decision(h,state,why);
}
static void write_next(Hook &h,void *next) {
    h.next=next;if(h.next_out)*h.next_out=next;
}
// Slot hooks (vtable slot in the EXE, or an absolute COM slot such as the swap-chain Present).
static void install_slot(Hook &h) {
    void **slot=h.slot;
    // C-1: slots that other mods guard by module ownership (fog 109-slot table, yuga VirtualQuery) must point into one of our DLLs.
    if((h.flags&LAYOUT_NEED_WHITELIST) && h.kind==kVtable && module_of(h.replacement).kind!=Kind::Ours) {set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_NOT_WHITELISTED);return;}
    const void *original=h.kind==kVtable?image+h.original_rva:nullptr;
    void *value=*slot;
    if((h.flags&LAYOUT_YIELD_IF_PEER) && owner_has_peer(h.owner)) {write_next(h,value);set_state(h,LAYOUT_STATE_YIELDED,LAYOUT_WHY_PEER);return;}
    Verdict verdict=classify(value,original,h.replacement,h.original_module);
    switch(verdict) {
    case Verdict::Ours:
        h.previous=h.next_out?*h.next_out:nullptr;h.next=h.previous;
        set_state(h,LAYOUT_STATE_INSTALLED,LAYOUT_WHY_NONE);return;
    case Verdict::Heap:set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_HEAP);return;
    case Verdict::OtherOwn:set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_OTHER_OWN);return;
    case Verdict::Foreign:
        if(!(h.flags&LAYOUT_CHAIN_OK) || (h.flags&LAYOUT_REFUSE_IF_FOREIGN)) {set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_FOREIGN);return;}
        break;
    case Verdict::Original:break;
    }
    h.previous=value;
    write_next(h,value);                 // before the slot can reach the replacement
    if(!cas_pointer(slot,value,h.replacement)) {set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_CAS);return;}
    set_state(h,verdict==Verdict::Foreign?LAYOUT_STATE_CHAINED:LAYOUT_STATE_INSTALLED,LAYOUT_WHY_NONE);
}
static bool block_rel_write(Bytes *site,int expected,int value) {
    // CALL at block offset <= 11: the whole instruction is inside one 16-byte block -> CMPXCHG16B.
    Bytes *block=reinterpret_cast<Bytes *>(reinterpret_cast<uintptr_t>(site)&~uintptr_t(15));
    unsigned offset=static_cast<unsigned>(site-block);
    if(offset<=11) {
        Bytes current[16],desired[16];
        memcpy(current,block,16);
        if(memcmp(current+offset+1,&expected,4))return false;
        memcpy(desired,current,16);memcpy(desired+offset+1,&value,4);
        return cas_block16(block,current,desired);
    }
    // rel32 crosses the block: one guarded write at install time, before the patched path can run.
    Bytes want[4],put[4];memcpy(want,&expected,4);memcpy(put,&value,4);
    return protect_write(site+1,4,op_raw,want,put);
}
static void install_call(Hook &h) {
    Bytes *site=image+h.rva;
    if(h.rva+5>image_size || *site!=h.opcode) {set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_BAD_SITE);return;}
    int rel=*reinterpret_cast<int *>(site+1);
    Bytes *target=site+5+rel;
    if((h.flags&LAYOUT_YIELD_IF_PEER) && owner_has_peer(h.owner)) {write_next(h,target);set_state(h,LAYOUT_STATE_YIELDED,LAYOUT_WHY_PEER);return;}
    Verdict verdict;
    if(in_pool(target)) verdict=relay_target(target)==h.replacement?Verdict::Ours:Verdict::OtherOwn;
    else verdict=classify(target,image+h.original_rva,nullptr,nullptr);
    switch(verdict) {
    case Verdict::Ours:set_state(h,LAYOUT_STATE_INSTALLED,LAYOUT_WHY_NONE);return;
    case Verdict::Heap:set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_HEAP);return;
    case Verdict::OtherOwn:set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_OTHER_OWN);return;
    case Verdict::Foreign:
        if(!(h.flags&LAYOUT_CHAIN_OK) || (h.flags&LAYOUT_REFUSE_IF_FOREIGN)) {set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_FOREIGN);return;}
        break;
    case Verdict::Original:break;
    }
    Bytes *relay=pool_relay(h.replacement);
    if(!relay) {set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_RELAY);return;}
    intptr_t delta=reinterpret_cast<intptr_t>(relay)-reinterpret_cast<intptr_t>(site+5);
    if(delta<INT32_MIN || delta>INT32_MAX) {set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_RANGE);return;}
    h.site=site;h.relay=relay;h.old_rel=rel;h.new_rel=static_cast<int>(delta);h.previous=target;
    write_next(h,target);
    if(!block_rel_write(site,rel,h.new_rel)) {set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_CAS);return;}
    set_state(h,verdict==Verdict::Foreign?LAYOUT_STATE_CHAINED:LAYOUT_STATE_INSTALLED,LAYOUT_WHY_NONE);
}
static void install_block(Hook &h) {
    Bytes *at=image+h.rva;
    if((h.flags&LAYOUT_YIELD_IF_PEER) && owner_has_peer(h.owner)) {set_state(h,LAYOUT_STATE_YIELDED,LAYOUT_WHY_PEER);return;}
    // C-4: never follow an entry that is already E9/FF25/EB; the bytes must be the original ones.
    if(at[0]==0xE9 || at[0]==0xEB || (at[0]==0xFF && at[1]==0x25)) {set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_REDIRECTED);return;}
    if(memcmp(at,h.expect,h.len)) {set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_BYTES);return;}
    Bytes *target=static_cast<Bytes *>(h.replacement);
    intptr_t delta=reinterpret_cast<intptr_t>(target)-reinterpret_cast<intptr_t>(at+5);
    if(delta<INT32_MIN || delta>INT32_MAX) {   // G5-b: the host relay is out of rel32 reach -> absolute jump through the pool
        target=pool_relay(h.replacement);
        if(!target) {set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_RELAY);return;}
        delta=reinterpret_cast<intptr_t>(target)-reinterpret_cast<intptr_t>(at+5);
        if(delta<INT32_MIN || delta>INT32_MAX) {set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_RANGE);return;}
    }
    memcpy(h.patch,h.expect,h.len);
    h.patch[0]=0xE9;int rel=static_cast<int>(delta);memcpy(h.patch+1,&rel,4);
    for(unsigned i=5;i<h.len;++i)h.patch[i]=0x90;
    h.site=at;h.relay=target;
    bool stuck=false;
    if(!range_write(at,h.expect,h.patch,h.len,&stuck)) {
        if(stuck) {   // F-02: a rollback CAS failed; keep the hook out of PENDING (never reinstalled) and say so
            set_state(h,LAYOUT_STATE_STUCK,LAYOUT_WHY_CAS);emit("layout_range_rollback_failed",h.rva,h.len,hash32(h.owner));
        } else set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_CAS);
        return;
    }
    set_state(h,LAYOUT_STATE_INSTALLED,LAYOUT_WHY_NONE);
}
// hook_bytes: a plain byte patch (value constants, short compare immediates). Same classification as a block hook.
static void install_bytes(Hook &h) {
    Bytes *at=image+h.rva;
    if((h.flags&LAYOUT_YIELD_IF_PEER) && owner_has_peer(h.owner)) {set_state(h,LAYOUT_STATE_YIELDED,LAYOUT_WHY_PEER);return;}
    if(memcmp(at,h.expect,h.len)) {set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_BYTES);return;}
    h.site=at;
    bool stuck=false;
    if(!range_write(at,h.expect,h.patch,h.len,&stuck)) {
        if(stuck) {   // F-02: a rollback CAS failed; keep the hook out of PENDING (never reinstalled) and say so
            set_state(h,LAYOUT_STATE_STUCK,LAYOUT_WHY_CAS);emit("layout_range_rollback_failed",h.rva,h.len,hash32(h.owner));
        } else set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_CAS);
        return;
    }
    set_state(h,LAYOUT_STATE_INSTALLED,LAYOUT_WHY_NONE);
}
// 0: no sibling failed, 1: a sibling was refused, 2: siblings only yielded (the whole owner yields).
static int owner_failed(const Hook &self) {
    if(!(self.flags&LAYOUT_ALL_OR_NONE))return 0;
    int result=0;
    for(unsigned i=0;i<hook_total;++i) {
        const Hook &o=hooks[i];
        if(&o==&self || strcmp(o.owner,self.owner) || !(o.flags&LAYOUT_ALL_OR_NONE))continue;
        if(o.state==LAYOUT_STATE_REFUSED || o.state==LAYOUT_STATE_STUCK)return 1;
        if(o.state==LAYOUT_STATE_YIELDED)result=2;
    }
    return result;
}
static int unhook_locked(Hook &h);
// G4: one owner is one feature. When any of its hooks was refused or yielded, the others are taken back (or
// switched to bypass) and registrations that arrive later are refused. layout_present* owners are single hooks.
static void rollback_group(const Hook &failed) {
    if(!(failed.flags&LAYOUT_ALL_OR_NONE))return;
    unsigned count=0;
    for(unsigned i=0;i<hook_total;++i) {
        Hook &o=hooks[i];
        if(&o==&failed || strcmp(o.owner,failed.owner) || !(o.flags&LAYOUT_ALL_OR_NONE))continue;
        if(o.state==LAYOUT_STATE_INSTALLED || o.state==LAYOUT_STATE_CHAINED) {unhook_locked(o);++count;}
        else if(o.state==LAYOUT_STATE_PENDING) {
            if(failed.state==LAYOUT_STATE_YIELDED)set_state(o,LAYOUT_STATE_YIELDED,LAYOUT_WHY_PEER);   // the whole owner yields
            else set_state(o,LAYOUT_STATE_REFUSED,LAYOUT_WHY_GROUP);
        }
    }
    if(count)emit("layout_group_rollback",count,failed.reason,hash32(failed.owner));
}
static void install_hook(Hook &h) {
    if(int failed=owner_failed(h)) {
        if(failed==2)set_state(h,LAYOUT_STATE_YIELDED,LAYOUT_WHY_PEER);
        else set_state(h,LAYOUT_STATE_REFUSED,LAYOUT_WHY_GROUP);
        return;
    }
    if(h.kind==kCall)install_call(h);
    else if(h.kind==kBlock)h.raw?install_bytes(h):install_block(h);
    else install_slot(h);
    if(h.state==LAYOUT_STATE_REFUSED || h.state==LAYOUT_STATE_YIELDED || h.state==LAYOUT_STATE_STUCK)rollback_group(h);
}
// Entry JMP blocklist (design 4.3 C-4 / 1.5): other mods hold fingerprints of these original entries.
static const uint32_t blocked_entries[]={GA(GA_9332B0),GA(GA_843B60),GA(GA_658DF0),GA(GA_870470),GA(GA_86E920),GA(GA_8704F0),GA(GA_7FC700),GA(GA_304D70),
    GA(GA_306A30),GA(GA_1F1DA0),GA(GA_1F1E90),GA(GA_1F2190),GA(GA_6C6B10),GA(GA_6C6DD0)};
// Review F-07: the protected window is the first kEntryWindow bytes of the entry (the other mods' fingerprints start there and their
// lengths differ per function, so the common first 16-byte line is the floor), tested as a half-open interval overlap in 64 bits.
static const uint64_t kEntryWindow=16;
static bool blocked_range(uint32_t rva,uint32_t len) {
    const uint64_t begin=rva,end=begin+len;
    for(uint32_t entry:blocked_entries)if(entry && begin<uint64_t(entry)+kEntryWindow && uint64_t(entry)<end)return true;
    return false;
}
// Review F-01: an unresolved table id reads 0 and a static GA() constant frozen before ga::adopt/resolve_image stays 0. RVA 0 is the PE header.
static bool unresolved_rva(const char *owner,uint32_t rva) {
    if(rva)return false;
    emit("layout_refused",0,LAYOUT_WHY_BAD_SITE,hash32(owner?owner:""));return true;
}
static int add_hook(const char *owner,HookKind kind,uint32_t rva,uint32_t original_rva,void *replacement,void **next_out,
    uint32_t flags,uint32_t len,const Bytes *expect,void **slot,const wchar_t *original_module,Bytes opcode=0xE8,bool raw=false) {
    if(!owner || !*owner || !replacement || !image)return -1;
    AcquireSRWLockExclusive(&table_lock);
    if(hook_total>=kMaxHooks) {ReleaseSRWLockExclusive(&table_lock);return -3;}
    Hook &h=hooks[hook_total];
    memset(&h,0,sizeof(h));h.kind=kind;strncpy_s(h.owner,sizeof(h.owner),owner,_TRUNCATE);
    h.rva=rva;h.original_rva=original_rva;h.flags=flags;h.len=len;h.replacement=replacement;h.next_out=next_out;
    h.slot=slot;h.original_module=original_module;h.opcode=opcode;h.raw=raw;if(raw && len)memcpy(h.patch,replacement,len<sizeof(h.patch)?len:sizeof(h.patch));
    if(expect && len)memcpy(h.expect,expect,len<sizeof(h.expect)?len:sizeof(h.expect));
    int handle=static_cast<int>(++hook_total);
    if(InterlockedCompareExchange(&published,0,0))install_hook(h);   // else installed by publish_hooks
    ReleaseSRWLockExclusive(&table_lock);
    return handle;
}
// Registered hooks are installed together: normal ones in registration order, then LATE_INSTALL ones.
static void publish_hooks() {
    AcquireSRWLockExclusive(&table_lock);
    if(InterlockedExchange(&published,1)==0) {
        for(unsigned pass=0;pass<2;++pass)
            for(unsigned i=0;i<hook_total;++i) {
                Hook &h=hooks[i];
                if(h.state!=LAYOUT_STATE_PENDING || ((h.flags&LAYOUT_LATE_INSTALL)!=0)!=(pass==1))continue;
                install_hook(h);
            }
    }
    ReleaseSRWLockExclusive(&table_lock);
}
static int api_hook_vtable(const char *owner,uint32_t slot_rva,uint32_t original_rva,void *replacement,void **next_out,uint32_t flags) {
    if(!image || slot_rva+8>image_size || original_rva>=image_size || unresolved_rva(owner,slot_rva) || unresolved_rva(owner,original_rva))return -1;
    return add_hook(owner,kVtable,slot_rva,original_rva,replacement,next_out,flags,0,nullptr,
        reinterpret_cast<void **>(image+slot_rva),nullptr);
}
static int api_hook_call(const char *owner,uint32_t site_rva,uint32_t original_target_rva,void *replacement,void **next_out,uint32_t flags) {
    if(!image || site_rva+5>image_size || original_target_rva>=image_size || unresolved_rva(owner,site_rva) || unresolved_rva(owner,original_target_rva))return -1;
    return add_hook(owner,kCall,site_rva,original_target_rva,replacement,next_out,flags,0,nullptr,nullptr,nullptr);
}
static int api_hook_block(const char *owner,uint32_t rva,const uint8_t *expect,uint32_t len,void *relay,uint32_t flags) {
    if(!owner || !*owner || !expect || !relay || len<5 || len>16)return -1;
    if(blocked_range(rva,len)) {                       // C-4: refuse before any registration
        emit("layout_refused",rva,LAYOUT_WHY_BLOCKED_ENTRY,hash32(owner));return -2;
    }
    if(!image || rva+len>image_size || (rva&15u)+len>16 || unresolved_rva(owner,rva))return -1;
    return add_hook(owner,kBlock,rva,0,relay,nullptr,flags,len,expect,nullptr,nullptr);
}
// P3: spans longer than 16 bytes or crossing blocks (E9 + NOP fill, up to 32 bytes), and raw byte patches.
static int api_hook_span(const char *owner,uint32_t rva,const uint8_t *expect,uint32_t len,void *relay,uint32_t flags) {
    if(!owner || !*owner || !expect || !relay || len<5 || len>32)return -1;
    if(blocked_range(rva,len)) {emit("layout_refused",rva,LAYOUT_WHY_BLOCKED_ENTRY,hash32(owner));return -2;}
    if(!image || rva+len>image_size || unresolved_rva(owner,rva))return -1;
    return add_hook(owner,kBlock,rva,0,relay,nullptr,flags,len,expect,nullptr,nullptr);
}
static int api_hook_bytes(const char *owner,uint32_t rva,const uint8_t *expect,const uint8_t *patch,uint32_t len,uint32_t flags) {
    if(!owner || !*owner || !expect || !patch || len<1 || len>32)return -1;
    if(blocked_range(rva,len)) {emit("layout_refused",rva,LAYOUT_WHY_BLOCKED_ENTRY,hash32(owner));return -2;}
    if(!image || rva+len>image_size || unresolved_rva(owner,rva))return -1;
    return add_hook(owner,kBlock,rva,0,const_cast<uint8_t *>(patch),nullptr,flags,len,expect,nullptr,nullptr,0xE8,true);
}
// An absolute COM slot (swap-chain Present): installed at once, chained after anything already there.
static int hook_abs_slot(const char *owner,void **slot,void *replacement,void **next_out,uint32_t flags,const wchar_t *original_module) {
    if(!slot)return -1;
    return add_hook(owner,kAbsSlot,0,0,replacement,next_out,flags,0,nullptr,slot,original_module);
}
// C-5: restore only while the slot/site still holds our value; otherwise switch to bypass.
static int unhook_locked(Hook &h) {
    int result;
    LONG state=h.state;
    if(state==LAYOUT_STATE_PENDING) {set_state(h,LAYOUT_STATE_REMOVED,LAYOUT_WHY_NONE);return LAYOUT_UNHOOK_REMOVED;}
    if(state!=LAYOUT_STATE_INSTALLED && state!=LAYOUT_STATE_CHAINED)return LAYOUT_UNHOOK_INACTIVE;
    bool restored=false;
    if(h.kind==kVtable || h.kind==kAbsSlot)
        restored=h.previous && cas_pointer(h.slot,h.replacement,h.previous);
    else if(h.kind==kCall)
        restored=h.site && block_rel_write(h.site,h.new_rel,h.old_rel);
    else
        restored=range_write(h.site,h.patch,h.expect,h.len);
    if(restored) {set_state(h,LAYOUT_STATE_REMOVED,LAYOUT_WHY_NONE);return LAYOUT_UNHOOK_REMOVED;}
    bool bypass=false;
    if(h.kind==kCall && h.relay && h.next)
        bypass=cas_pointer(reinterpret_cast<void **>(h.relay+6),h.replacement,h.next);   // relay is ours
    else if((h.kind==kVtable || h.kind==kAbsSlot) && h.next) {
        // The host owns a `jmp [rip+cell]` thunk (C-1): rewrite its cell to the next function.
        auto thunk=static_cast<Bytes *>(h.replacement);
        if(thunk[0]==0xFF && thunk[1]==0x25) {
            void **cell=reinterpret_cast<void **>(thunk+6+*reinterpret_cast<int *>(thunk+2));
            bypass=cas_pointer(cell,*cell,h.next);
        }
    }
    if(bypass) {set_state(h,LAYOUT_STATE_BYPASSED,LAYOUT_WHY_CAS);result=LAYOUT_UNHOOK_BYPASSED;}
    else {set_state(h,LAYOUT_STATE_STUCK,LAYOUT_WHY_CAS);result=LAYOUT_UNHOOK_STUCK;}
    return result;
}
static int api_unhook(int handle) {
    if(handle<=0)return LAYOUT_UNHOOK_INVALID;
    AcquireSRWLockExclusive(&table_lock);
    int result=LAYOUT_UNHOOK_INVALID;
    if(static_cast<unsigned>(handle)<=hook_total)result=unhook_locked(hooks[handle-1]);
    ReleaseSRWLockExclusive(&table_lock);
    return result;
}
// E9 tail-jump site (898957): same classification and chain logic as an E8 call site, opcode 0xE9.
static int api_hook_jump(const char *owner,uint32_t site_rva,uint32_t original_target_rva,void *replacement,void **next_out,uint32_t flags) {
    if(!image || site_rva+5>image_size || original_target_rva>=image_size || unresolved_rva(owner,site_rva) || unresolved_rva(owner,original_target_rva))return -1;
    return add_hook(owner,kCall,site_rva,original_target_rva,replacement,next_out,flags,0,nullptr,nullptr,nullptr,0xE9);
}
static int api_peer_for_owner(const char *owner) {return owner && *owner && owner_has_peer(owner)?1:0;}
// Worst state of an owner's hooks: REFUSED, YIELDED, STUCK, BYPASSED, REMOVED, CHAINED, INSTALLED, PENDING; 0 when none.
static int api_hook_owner_state(const char *owner) {
    if(!owner)return 0;
    static const int order[]={LAYOUT_STATE_REFUSED,LAYOUT_STATE_YIELDED,LAYOUT_STATE_STUCK,LAYOUT_STATE_BYPASSED,
        LAYOUT_STATE_REMOVED,LAYOUT_STATE_CHAINED,LAYOUT_STATE_INSTALLED,LAYOUT_STATE_PENDING};
    int best=-1;bool any=false;
    AcquireSRWLockShared(&table_lock);
    for(unsigned i=0;i<hook_total;++i) {
        if(strcmp(hooks[i].owner,owner))continue;
        any=true;int rank=0;
        for(;rank<8;++rank)if(order[rank]==hooks[i].state)break;
        if(best<0 || rank<best)best=rank;
    }
    ReleaseSRWLockShared(&table_lock);
    return any?order[best]:0;
}
static int api_hook_state(int handle) {
    if(handle<=0 || static_cast<unsigned>(handle)>hook_total)return -1;
    return static_cast<int>(InterlockedCompareExchange(&hooks[handle-1].state,0,0));
}
static int api_slot_owner(uint32_t rva,wchar_t *out,int cap) {
    if(!image || !out || cap<2 || rva+8>image_size)return -1;
    Where w=module_of(*reinterpret_cast<void **>(image+rva));
    const wchar_t *name=w.kind==Kind::None?L"heap":w.name;
    if(wcscpy_s(out,static_cast<size_t>(cap),name))return -1;
    return static_cast<int>(wcslen(out));
}
}
