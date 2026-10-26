// Current-build reference manifests; native save record layout is unchanged.
struct Trait9Site {unsigned rva,size,stub;unsigned char before[32];};
struct Trait9Reloc {unsigned data,offset,absolute,target,addend;};
struct Trait9Manifest {
    const unsigned char *code;size_t code_length;const unsigned char *data;size_t data_length;
    unsigned code_size,data_size;const Trait9Site *sites;size_t site_count;const Trait9Reloc *relocs;size_t reloc_count;
};
#include "traits9_payloads.h"
struct Trait9Patch {unsigned rva,size;unsigned char before[32],after[32];bool published;};
struct Trait9State {Obj allocation;Trait9Patch patches[5];};
static Trait9State traits9_states[3];
static Trait9Patch captive_patch;
static Obj captive_relay;
static unsigned traits9_core_published;
static int __fastcall captive_either_force(Obj prisoner,Obj capturing) { COST_SCOPE("traits9_captive_either_force");
    auto player=reinterpret_cast<int(__fastcall *)(Obj)>(image+captive_original);
    return (prisoner && player(prisoner)) || (capturing && player(capturing));
}
static Obj traits9_allocate(unsigned size) {
    uintptr_t base=reinterpret_cast<uintptr_t>(image);
    for(uintptr_t offset=0x3000000;offset<0x70000000;offset+=0x10000) {
        Obj result=static_cast<Obj>(VirtualAlloc(reinterpret_cast<void *>((base+offset)&~uintptr_t(0xFFFF)),size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if(result)return result;
    }
    return nullptr;
}
static bool traits9_guards_match(bool slots=true,bool captive=true) {
    if(slots)for(const auto &p:traits9_core)if(memcmp(image+p.rva,p.before,p.size))return false;
    if(slots)for(const auto &m:trait9_manifests)for(size_t i=0;i<m.site_count;++i)
        if(memcmp(image+m.sites[i].rva,m.sites[i].before,m.sites[i].size))return false;
    return !captive || !memcmp(image+captive_site,captive_before,5);
}
static bool prepare_traits9() {
    if(!traits9_guards_match(mod_features.traits9,mod_features.captive_dialog))return false;
    if(mod_features.traits9)for(size_t n=0;n<3;++n) {
        const auto &m=trait9_manifests[n];auto &s=traits9_states[n];
        if(m.site_count>5)return false;
        s.allocation=traits9_allocate(m.code_size+m.data_size);if(!s.allocation)return false;
        Obj code=s.allocation,data=code+m.code_size;
        memcpy(code,m.code,m.code_length);memcpy(data,m.data,m.data_length);
        for(size_t i=0;i<m.reloc_count;++i) {
            const auto &r=m.relocs[i];Obj at=(r.data?data:code)+r.offset;
            Obj target=(r.target==0?image:r.target==1?code:data)+r.addend;
            if(r.absolute) *reinterpret_cast<uint64_t *>(at)=reinterpret_cast<uint64_t>(target);
            else {
                intptr_t delta=target-(at+4);if(delta<INT32_MIN || delta>INT32_MAX)return false;
                *reinterpret_cast<int *>(at)=static_cast<int>(delta);
            }
        }
        for(size_t i=0;i<m.site_count;++i) {
            const auto &site=m.sites[i];auto &p=s.patches[i];
            if(site.size<5 || site.size>32)return false;
            p.rva=site.rva;p.size=site.size;memcpy(p.before,site.before,site.size);
            memset(p.after,0x90,site.size);p.after[0]=0xE9;
            intptr_t delta=(code+site.stub)-(image+site.rva+5);if(delta<INT32_MIN || delta>INT32_MAX)return false;
            *reinterpret_cast<int *>(p.after+1)=static_cast<int>(delta);
        }
        DWORD old=0;
        if(!VirtualProtect(code,m.code_size,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),code,m.code_size))return false;
    }
    if(!mod_features.captive_dialog)return true;
    captive_relay=traits9_allocate(4096);if(!captive_relay)return false;
    // Original reference relay passes the capturing force from native R15.
    const unsigned char force[]={0x4C,0x89,0xFA};memcpy(captive_relay,force,3);
    absolute_jump(captive_relay+3,reinterpret_cast<void *>(&captive_either_force));
    captive_patch.rva=captive_site;captive_patch.size=5;
    memcpy(captive_patch.before,captive_before,5);captive_patch.after[0]=0xE8;
    intptr_t delta=captive_relay-(image+captive_site+5);if(delta<INT32_MIN || delta>INT32_MAX)return false;
    *reinterpret_cast<int *>(captive_patch.after+1)=static_cast<int>(delta);
    DWORD old=0;
    return VirtualProtect(captive_relay,4096,PAGE_EXECUTE_READ,&old) && FlushInstructionCache(GetCurrentProcess(),captive_relay,4096);
}
// v12.8: captive_dialog (either-force capture scene) publishes independently of the nine slots.
static bool publish_traits9(bool slots,bool captive) {
    if(captive) {
        captive_patch.published=true;
        if(!modpack_write(captive_patch.rva,captive_patch.before,captive_patch.after,5))return false;
    }
    if(!slots)return true;
    for(const auto &p:traits9_core) {
        ++traits9_core_published;
        if(!modpack_write(p.rva,p.before,p.after,p.size))return false;
    }
    for(size_t n=0;n<3;++n)for(size_t i=0;i<trait9_manifests[n].site_count;++i) {
        auto &p=traits9_states[n].patches[i];p.published=true;
        if(!modpack_write(p.rva,p.before,p.after,p.size))return false;
    }
    return true;
}
static void restore_traits9() {
    if(captive_patch.published)modpack_write(captive_patch.rva,captive_patch.after,captive_patch.before,5);
    for(size_t n=0;n<3;++n)for(size_t i=0;i<trait9_manifests[n].site_count;++i) {
        const auto &p=traits9_states[n].patches[i];if(p.published)modpack_write(p.rva,p.after,p.before,p.size);
    }
    for(unsigned i=0;i<traits9_core_published;++i) {
        const auto &p=traits9_core[i];modpack_write(p.rva,p.after,p.before,p.size);
    }
    traits9_core_published=0;
    // Retain gates that a native UI callback could already have entered.
}
