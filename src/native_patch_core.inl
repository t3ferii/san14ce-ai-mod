#pragma once
// Patch services of every native_adapter.cpp translation unit (2026-11-16 refactor, docs/REFACTOR_EQUIVALENCE.md section 4):
// pointer slots, 16-byte code swaps, CALL rel32 links and near allocation, moved verbatim from native_adapter.cpp. Every helper is a
// stateless `static` marked [[maybe_unused]]: a unit that calls one compiles its own copy, a unit that does not gets none (no C4505),
// and the single-unit combat build keeps the code it had. State-holding services stay out of this file (they would need `inline`).
[[maybe_unused]] static bool patch_pointer(unsigned rva, unsigned expected, void *replacement)
{
    auto slot = reinterpret_cast<void **>(image + rva);
    DWORD old = 0;
    if (*slot != image + expected || !VirtualProtect(slot, 8, PAGE_READWRITE, &old)) return false;
    void *previous = InterlockedCompareExchangePointer(slot, replacement, image + expected);
    DWORD restored = 0; VirtualProtect(slot, 8, old, &restored);
    return previous == image + expected;
}
[[maybe_unused]] static void restore_pointer(unsigned rva, unsigned original, void *replacement)
{
    auto slot = reinterpret_cast<void **>(image + rva);
    DWORD old = 0, restored = 0;
    if (!VirtualProtect(slot, 8, PAGE_READWRITE, &old)) return;
    InterlockedCompareExchangePointer(slot, image + original, replacement);
    VirtualProtect(slot, 8, old, &restored);
}
[[maybe_unused]] static bool other_mod_present(const wchar_t *name) {
    if(GetModuleHandleW(name))return true;
    wchar_t path[MAX_PATH],folder[MAX_PATH];if(!GetModuleFileNameW(nullptr,path,MAX_PATH))return false;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return false;*slash=0;
    if(wcscpy_s(folder,MAX_PATH,path) || wcscat_s(folder,MAX_PATH,L"\\") || wcscat_s(folder,MAX_PATH,name))return false;
    DWORD attr=GetFileAttributesW(folder);return attr!=INVALID_FILE_ATTRIBUTES && !(attr&FILE_ATTRIBUTE_DIRECTORY);
}
[[maybe_unused]] static bool replace_code16(unsigned rva,const unsigned char *expected, const unsigned char *replacement)
{
    /* CALL crosses an eight-byte boundary: publish the enclosing aligned
     * sixteen bytes with CMPXCHG16B so no thread can decode a partial target. */
    if (!IsProcessorFeaturePresent(PF_COMPARE_EXCHANGE128)) return false;
    auto block = reinterpret_cast<volatile LONG64 *>(image + rva);
    alignas(16) LONG64 comparison[2], desired[2];
    memcpy(comparison, expected, 16); memcpy(desired, replacement, 16);
    DWORD old = 0, restored = 0;
    if (!VirtualProtect(image + rva, 16, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool result = _InterlockedCompareExchange128(block, desired[1], desired[0], comparison) != 0;
    VirtualProtect(image + rva, 16, old, &restored);
    FlushInstructionCache(GetCurrentProcess(), image + rva, 16);
    return result;
}
[[maybe_unused]] static bool replace_call(const unsigned char *expected,const unsigned char *replacement) {
    return replace_code16(GA(GA_D3510),expected,replacement);
}
struct CodeLink {
    unsigned rva,block;
    unsigned char original[16],modified[16];
    bool published;
};
[[maybe_unused]] static void absolute_jump(Obj at, void *target) {
    at[0]=0xFF;at[1]=0x25;memset(at+2,0,4);
    *reinterpret_cast<void **>(at+6)=target;
}
// local11: a patch that would overwrite the displaced prefix of an observed entry (San14Sim's E9 hook) is a real conflict: the observer keeps
// the entry, the feature that wanted it stays off (its own prepare failure follows). feature 0 = the hook preparer does not know its feature.
[[maybe_unused]] static bool observer_conflict(unsigned rva,unsigned count) {
    unsigned entry=0;
    if(!ga::observer_overlaps(image,rva,count,&entry))return false;
    log_event("observer_hook_conflict",0,entry);
    return true;
}
[[maybe_unused]] static bool prepare_link(CodeLink &link,unsigned rva,unsigned char opcode,Obj relay) {
    link={};link.rva=rva;link.block=rva&~15u;
    if(observer_conflict(rva,5))return false;
    unsigned offset=rva-link.block;if(offset>11)return false;
    intptr_t delta=reinterpret_cast<intptr_t>(relay)-reinterpret_cast<intptr_t>(image+rva+5);
    if(delta<INT32_MIN || delta>INT32_MAX)return false;
    memcpy(link.original,image+link.block,16);memcpy(link.modified,link.original,16);
    link.modified[offset]=opcode;memcpy(link.modified+offset+1,&delta,4);return true;
}
[[maybe_unused]] static bool prepare_bytes(CodeLink &link,unsigned rva,const unsigned char *bytes,unsigned count) {
    link={};link.rva=rva;link.block=rva&~15u;
    if(observer_conflict(rva,count))return false;
    unsigned offset=rva-link.block;if(offset+count>16)return false;
    memcpy(link.original,image+link.block,16);memcpy(link.modified,link.original,16);
    memcpy(link.modified+offset,bytes,count);return true;
}
// CALL rel32 redirection for any call site. Inside one 16-byte block the swap is the atomic
// replace_code16; a call whose rel32 crosses a block boundary (site offset > 11) is written once
// at install, before the patched UI/command path can run, and restored the same way.
struct CallLink {CodeLink link;unsigned site=0;int original=0,redirected=0;bool raw=false,published=false;};
[[maybe_unused]] static bool call_prepare(CallLink &c,unsigned site,Obj relay) {
    c=CallLink{};c.site=site;
    if(image[site]!=0xE8)return false;
    intptr_t delta=reinterpret_cast<intptr_t>(relay)-reinterpret_cast<intptr_t>(image+site+5);
    if(delta<INT32_MIN || delta>INT32_MAX)return false;
    c.original=*reinterpret_cast<int *>(image+site+1);c.redirected=static_cast<int>(delta);
    c.raw=(site&15u)>11;
    return c.raw || prepare_link(c.link,site,0xE8,relay);
}
static bool publish_link(CodeLink &link);
[[maybe_unused]] static bool call_write(unsigned site,int expected,int value) {
    DWORD old=0,restored=0;
    if(*reinterpret_cast<int *>(image+site+1)!=expected || !VirtualProtect(image+site,5,PAGE_EXECUTE_READWRITE,&old))return false;
    memcpy(image+site+1,&value,4);VirtualProtect(image+site,5,old,&restored);
    FlushInstructionCache(GetCurrentProcess(),image+site,5);return true;
}
[[maybe_unused]] static bool call_publish(CallLink &c) {
    if(!c.raw) {   // two sites in one block: build the swap from the block as it is now
        unsigned offset=c.site-c.link.block;
        memcpy(c.link.original,image+c.link.block,16);memcpy(c.link.modified,c.link.original,16);
        c.link.modified[offset]=0xE8;memcpy(c.link.modified+offset+1,&c.redirected,4);
    }
    c.published=c.raw?call_write(c.site,c.original,c.redirected):publish_link(c.link);return c.published;
}
static void restore_link(CodeLink &link);
[[maybe_unused]] static void call_restore(CallLink &c) {
    if(!c.published)return;
    if(c.raw)call_write(c.site,c.redirected,c.original);else restore_link(c.link);
    c.published=false;
}
[[maybe_unused]] static bool publish_link(CodeLink &link) {
    link.published=replace_code16(link.block,link.original,link.modified);return link.published;
}
[[maybe_unused]] static void restore_link(CodeLink &link) {
    if(link.published)replace_code16(link.block,link.modified,link.original);
}
[[maybe_unused]] static Obj allocate_near(Obj call,unsigned bytes=0x1000)
{
    SYSTEM_INFO info; GetSystemInfo(&info);
    uintptr_t start = reinterpret_cast<uintptr_t>(call) & ~(uintptr_t(info.dwAllocationGranularity) - 1);
    for (uintptr_t offset = info.dwAllocationGranularity; offset < 0x70000000; offset += info.dwAllocationGranularity) {
        for (int sign : {1, -1}) {
            uintptr_t address = sign > 0 ? start + offset : (start > offset ? start - offset : 0);
            if (!address) continue;
            Obj memory = static_cast<Obj>(VirtualAlloc(reinterpret_cast<void *>(address), bytes,
                                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            if (memory) return memory;
        }
    }
    return nullptr;
}
