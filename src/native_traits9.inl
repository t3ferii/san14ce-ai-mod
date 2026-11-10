// Current-build reference manifests; native save record layout is unchanged.
struct Trait9Site {unsigned rva,size,stub;unsigned char before[32];};
// One byte patch of the frozen payload (src/traits9_payloads.h traits9_core); the units no longer share a row type with stats200.
struct Trait9Bytes {unsigned rva,size;unsigned char before[16],after[16];};
struct Trait9Reloc {unsigned data,offset,absolute,target,addend;};
struct Trait9Manifest {
    const unsigned char *code;size_t code_length;const unsigned char *data;size_t data_length;
    unsigned code_size,data_size;const Trait9Site *sites;size_t site_count;const Trait9Reloc *relocs;size_t reloc_count;
};
#include "traits9_payloads.h"
struct Trait9Patch {unsigned rva,size;unsigned char before[32],after[32];};
struct Trait9State {Obj allocation;Trait9Patch patches[5];};
static Trait9State traits9_states[3];
// local10 (2026-11-06, docs/HISTORY.md 4.1): the manifests carry the reference build's bytes. Three sites hold a RIP displacement
// that differs per build, and the Layout manager compares `before` literally, so after the guards have passed (ga::mcmp_at
// judges the pristine bytes of the running build) the `before` of every site and core row is copied from the image. The core rows are
// the writable copy `traits9_core_rt` (rva, size and `after` unchanged); the manager's unhook puts bytes back only while they are still ours.
static Trait9Bytes traits9_core_rt[sizeof(traits9_core)/sizeof(traits9_core[0])];
static bool traits9_before_copied;
// The captive_dialog feature (either-force capture scene, former gate 72 / key extensions.captive_dialog) was removed on 2026-11-06 by the
// user (the 1a4a447d original already shows the scene for either force): an old ini key is simply never read.
static Obj traits9_allocate(unsigned size) {
    uintptr_t base=reinterpret_cast<uintptr_t>(image);
    for(uintptr_t offset=0x3000000;offset<0x70000000;offset+=0x10000) {
        Obj result=static_cast<Obj>(VirtualAlloc(reinterpret_cast<void *>((base+offset)&~uintptr_t(0xFFFF)),size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if(result)return result;
    }
    return nullptr;
}
static bool traits9_guards_match() {
    unsigned index=0;
    for(const auto &p:traits9_core) {
        if(ga::mcmp_at(image,p.rva,p.before,p.size)) {log_event("traits9_guard_failed",index);return false;}
        ++index;
    }
    for(const auto &m:trait9_manifests)for(size_t i=0;i<m.site_count;++i) {
        if(ga::mcmp_at(image,m.sites[i].rva,m.sites[i].before,m.sites[i].size)) {log_event("traits9_guard_failed",index);return false;}
        ++index;
    }
    return true;
}
// Copies the pristine bytes of the running image into the runtime `before` copies. Only called right after traits9_guards_match().
static bool traits9_copy_before() {
    unsigned sites=0,core=0;
    for(size_t n=0;n<sizeof(traits9_core)/sizeof(traits9_core[0]);++n) {
        traits9_core_rt[n]=traits9_core[n];
        if(!traits9_core_rt[n].rva || traits9_core_rt[n].size>sizeof(traits9_core_rt[n].before))return false;
        memcpy(traits9_core_rt[n].before,image+traits9_core_rt[n].rva,traits9_core_rt[n].size);++core;
    }
    for(size_t n=0;n<3;++n) {
        const auto &m=trait9_manifests[n];
        if(m.site_count>5)return false;
        for(size_t i=0;i<m.site_count;++i) {
            auto &p=traits9_states[n].patches[i];
            if(!m.sites[i].rva || m.sites[i].size<5 || m.sites[i].size>sizeof(p.before))return false;
            p.rva=m.sites[i].rva;p.size=m.sites[i].size;memcpy(p.before,image+p.rva,p.size);++sites;
        }
    }
    traits9_before_copied=true;
    log_event("traits9_before_copied",sites,core);
    return true;
}
static bool prepare_traits9() {
    if(!mod_features.traits9)return true;
    if(!traits9_guards_match() || !traits9_copy_before())return false;
    for(size_t n=0;n<3;++n) {
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
            p.rva=site.rva;p.size=site.size;   // p.before: image bytes (traits9_copy_before)
            memset(p.after,0x90,site.size);p.after[0]=0xE9;
            intptr_t delta=(code+site.stub)-(image+site.rva+5);if(delta<INT32_MIN || delta>INT32_MAX)return false;
            *reinterpret_cast<int *>(p.after+1)=static_cast<int>(delta);
        }
        DWORD old=0;
        if(!VirtualProtect(code,m.code_size,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),code,m.code_size))return false;
    }
    return true;
}
// 2026-11-05 local3 (user: a failed optional feature switches only itself off): publish_traits9 returns the FAILED part
// (bit0 = nine slots); a failed part is taken back here (its manager registrations / written bytes) and every other feature stays
// as published. local10: the captive dialog part (bit1) was removed with the feature.
#ifdef LAYOUT_ROUTED
// Design 8.1 rows 45-46 (user 2026-11-05 Layout compatibility design): the eight inline JMP spans and the five 5->9
// byte patches are registered with the manager as owner "traits9" (YIELD_IF_PEER, never chained: R1 Traits9 writes the
// same bytes). Spans longer than 16 bytes or crossing a block are written block by block (hook_span). The manager compares
// `before` literally, so it gets the bytes copied from the image (traits9_copy_before), never the reference build's literals.
static unsigned publish_traits9(bool slots) {
    if(!slots)return 0;
    unsigned mark=layout_client::current_mark();
    bool ok=traits9_before_copied;
    for(const auto &p:traits9_core_rt)
        if(ok && layout_client::bytes("traits9",p.rva,p.before,p.after,p.size,LF_Y)<=0)ok=false;
    for(size_t n=0;n<3 && ok;++n)for(size_t i=0;i<trait9_manifests[n].site_count && ok;++i) {
        const auto &p=traits9_states[n].patches[i];
        if(layout_client::span("traits9",p.rva,p.before,p.size,traits9_states[n].allocation+trait9_manifests[n].sites[i].stub,LF_Y)<=0)ok=false;
    }
    if(!ok) {layout_client::rollback_to(mark);return 1;}
    return 0;
}
#else
// No Layout manager (only the address guard fixture builds this unit unrouted, and it never publishes): the nine slots are not applied.
static unsigned publish_traits9(bool slots) {return slots?1u:0u;}
#endif
