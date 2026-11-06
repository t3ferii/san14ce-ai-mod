#include "game_addresses.h"
#include <array>
#include <utility>
struct SortieEntry {unsigned rva,size;unsigned char bytes[16];void *replacement;void **gate;};
static const SortieEntry sortie_entries[]={
    {GA(GA_48CE0),6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0x58},reinterpret_cast<void *>(&sortie_plan8),reinterpret_cast<void **>(&original_sortie_planners[0])},
    {GA(GA_46B80),6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0xD8},reinterpret_cast<void *>(&sortie_plan9),reinterpret_cast<void **>(&original_sortie_planners[1])},
    {GA(GA_4BA50),6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0xC8},reinterpret_cast<void *>(&sortie_plan10),reinterpret_cast<void **>(&original_sortie_planners[2])},
    {GA(GA_4ABE0),6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0x68,0x88},reinterpret_cast<void *>(&sortie_plan11),reinterpret_cast<void **>(&original_sortie_planners[3])},
    {GA(GA_44A80),6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0xD8},reinterpret_cast<void *>(&sortie_plan22),reinterpret_cast<void **>(&original_sortie_planners[4])},
    {GA(GA_1EC890),6,{0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0x6C,0x24},reinterpret_cast<void *>(&sortie_dispatch),reinterpret_cast<void **>(&original_sortie_dispatch)},
    {GA(GA_2B0290),5,{0x40,0x53,0x56,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x81,0xEC,0xF0,0x02,0x00,0x00},reinterpret_cast<void *>(&sortie_movement),reinterpret_cast<void **>(&original_sortie_movement)},
    {GA(GA_1843E0),5,{0x48,0x89,0x5C,0x24,0x18,0x48,0x89,0x54,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41},reinterpret_cast<void *>(&sortie_search),reinterpret_cast<void **>(&original_sortie_search)},
};
// User decision 2026-10-30: preserve native entries for R42d / Batteki 1.7.
static const unsigned sortie_plan_sites[]={GA(GA_3007C),GA(GA_30091),GA(GA_300A6),GA(GA_30067),GA(GA_300BF)};
static const unsigned sortie_search_roots[]={GA(GA_44A80),GA(GA_46B80),GA(GA_48CE0),GA(GA_4ABE0),GA(GA_4BA50),GA(GA_A2410),GA(GA_1EC890),GA(GA_22CC40),GA(GA_22DA10),GA(GA_26C940),GA(GA_2EAAE0),GA(GA_2EAB40),GA(GA_2EE1E0),GA(GA_32F500)};
// Depth <=8 E8 graph; direct tail JMPs cost zero, chained .pdata spans coalesced.
static const unsigned sortie_search_sites[]={
    GA(GA_46425),GA(GA_4658B),GA(GA_484D7),GA(GA_4862B),GA(GA_4A5B6),GA(GA_4A70E),GA(GA_4B38D),GA(GA_4D426),
    GA(GA_4D58D),GA(GA_B5DD9),GA(GA_BB654),GA(GA_1ECCF6),GA(GA_273951),GA(GA_273B3C),GA(GA_27453C),GA(GA_276A59),
    GA(GA_2ACC57),GA(GA_2AEA76),GA(GA_2B1BF2),GA(GA_2B20C3),GA(GA_301BD9)
};
// Every E8 -> 2B0290 in the matching unpacked .text (CRC 6C7F386E).
static const unsigned sortie_movement_sites[]={
    GA(GA_B98F),GA(GA_B9FA),GA(GA_67DC4),GA(GA_7031B),GA(GA_7060C),GA(GA_70652),GA(GA_73026),GA(GA_734D4),GA(GA_73519),GA(GA_73D31),
    GA(GA_73D76),GA(GA_86D70),GA(GA_97396),GA(GA_973AF),GA(GA_990FB),GA(GA_9A3D4),GA(GA_9AACD),GA(GA_9B52C),GA(GA_9EBF6),GA(GA_9FFA2),
    GA(GA_A0A1A),GA(GA_A0FC4),GA(GA_A2F65),GA(GA_B6FCD),GA(GA_BF275),GA(GA_CCECB),GA(GA_CD44B),GA(GA_CD48C),GA(GA_CD624),GA(GA_CD67A),
    GA(GA_CDA94),GA(GA_CE133),GA(GA_CE154),GA(GA_CE35D),GA(GA_D074F),GA(GA_D0791),GA(GA_183CC9),GA(GA_1847F1),GA(GA_185132),GA(GA_1865AA),
    GA(GA_1CA6AF),GA(GA_25392B),GA(GA_25394D),GA(GA_266532),GA(GA_269198),GA(GA_340CB6),GA(GA_340DB9),GA(GA_923E1B),GA(GA_923E3D)
};
static_assert(sizeof(sortie_movement_sites)/sizeof(unsigned)==49,"all native movement calls");
static constexpr unsigned kSortieSearchCount=sizeof(sortie_search_sites)/sizeof(unsigned);
static constexpr unsigned kSortieSearchEnd=5+kSortieSearchCount;
static constexpr unsigned kSortieCallCount=kSortieSearchEnd+sizeof(sortie_movement_sites)/sizeof(unsigned);
static_assert(4096+kSortieCallCount*16<=8192,"sortie stubs fit allocated relay");
static SortieSearch sortie_search_nexts[kSortieSearchCount];
static SortieMovement sortie_movement_nexts[49];
template<unsigned K> static int __fastcall sortie_search_site(SortieNativePath *p,Obj c,unsigned f,unsigned *t,int a,int b,int d) {
    return sortie_search_next(sortie_search_nexts[K],p,c,f,t,a,b,d);
}
template<unsigned K> static float __fastcall sortie_movement_site(Obj a,Obj f,int p,int m,Obj t) {
    return sortie_movement_next(sortie_movement_nexts[K],a,f,p,m,t);
}
template<size_t... K> static auto sortie_search_thunks(std::index_sequence<K...>) {
    return std::array<SortieSearch,sizeof...(K)>{{&sortie_search_site<K>...}};
}
template<size_t... K> static auto sortie_movement_thunks(std::index_sequence<K...>) {
    return std::array<SortieMovement,sizeof...(K)>{{&sortie_movement_site<K>...}};
}
struct SortieCode {CodeLink dispatch{};CallLink calls[kSortieCallCount]{};bool ready[kSortieCallCount]{};bool dispatch_ready=false;};
static Obj sortie_call_target(unsigned site,unsigned native) {
    if(image[site]!=0xE8)return nullptr;
    int delta;memcpy(&delta,image+site+1,4);
    uintptr_t target=reinterpret_cast<uintptr_t>(image+site+5)+delta;
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+reinterpret_cast<IMAGE_DOS_HEADER *>(image)->e_lfanew);
    uintptr_t begin=reinterpret_cast<uintptr_t>(image),end=begin+pe->OptionalHeader.SizeOfImage;
    return target==begin+native || target<begin || target>=end?reinterpret_cast<Obj>(target):nullptr;
}
// Validate exactly this CALL, including raw calls crossing a 16-byte block.
static bool sortie_call_publish(CallLink &c) {
    int delta;memcpy(&delta,image+c.site+1,4);
    return image[c.site]==0xE8 && delta==c.original && call_publish(c);
}
static void sortie_call_restore(CallLink &c) {
    if(!c.published || image[c.site]!=0xE8)return;
    int delta;memcpy(&delta,image+c.site+1,4);if(delta!=c.redirected)return;
    if(c.raw)call_write(c.site,c.redirected,c.original);
    else {
        // Restore only our five bytes; preserve later edits to neighbouring calls.
        CodeLink undo{};undo.block=c.site&~15u;
        memcpy(undo.original,image+undo.block,16);memcpy(undo.modified,undo.original,16);
        memcpy(undo.modified+(c.site-undo.block)+1,&c.original,4);
        replace_code16(undo.block,undo.original,undo.modified);
    }
    c.published=false;
}
// Set false by combat_address_gates (game_addresses_gates.inl): an id the sortie family needs is unresolved in this build.
static bool sortie_addr_ok=true;
static bool sortie_guards_match() {
    if(!sortie_addr_ok)return true;   // family switched off: nothing of it is prepared or published
    // A changed search/movement/dispatch site never disables the other calls.
    for(unsigned i=0;i<5;++i)if(sortie_call_target(sortie_plan_sites[i],sortie_entries[i].rva))return true;
    return false;
}
static bool prepare_sortie(SortieCode &code,Obj relay) {
    code={};
    if(!sortie_addr_ok)return true;
    original_sortie_dispatch=reinterpret_cast<SortieDispatch>(image+GA(GA_1EC890));
    original_sortie_movement=reinterpret_cast<SortieMovement>(image+GA(GA_2B0290));
    original_sortie_search=reinterpret_cast<SortieSearch>(image+GA(GA_1843E0));
    const auto searches=sortie_search_thunks(std::make_index_sequence<kSortieSearchCount>{});
    const auto movements=sortie_movement_thunks(std::make_index_sequence<49>{});
    unsigned planners=0;
    // [4096,4096+kSortieCallCount*16): 16-byte tail jumps; native entries intact.
    for(unsigned k=0;k<kSortieCallCount;++k) {
        unsigned site=k<5?sortie_plan_sites[k]:k<kSortieSearchEnd?sortie_search_sites[k-5]:sortie_movement_sites[k-kSortieSearchEnd];
        unsigned native=k<5?sortie_entries[k].rva:k<kSortieSearchEnd?GA(GA_1843E0):GA(GA_2B0290);
        Obj next=sortie_call_target(site,native),stub=relay+4096+k*16;
        if(!next) {log_event("sortie_call_skip",site,native,0);continue;}
        void *replacement=k<5?sortie_entries[k].replacement:k<kSortieSearchEnd?reinterpret_cast<void *>(searches[k-5]):reinterpret_cast<void *>(movements[k-kSortieSearchEnd]);
        absolute_jump(stub,replacement);
        if(!call_prepare(code.calls[k],site,stub)) {log_event("sortie_call_skip",site,native,1);continue;}
        if(k<5) {original_sortie_planners[k]=reinterpret_cast<SortiePlanner>(next);++planners;}
        else if(k<kSortieSearchEnd)sortie_search_nexts[k-5]=reinterpret_cast<SortieSearch>(next);
        else sortie_movement_nexts[k-kSortieSearchEnd]=reinterpret_cast<SortieMovement>(next);
        code.ready[k]=true;
    }
    // 1EC890 retains its original six-byte entry gate at [2432,2496).
    const auto &e=sortie_entries[5];Obj stub=relay+2432,gate=stub+32;
    if(!ga::mcmp_at(image,e.rva,e.bytes,16)) {
        absolute_jump(stub,e.replacement);memcpy(gate,e.bytes,e.size);
        absolute_jump(gate+e.size,image+e.rva+e.size);*e.gate=gate;
        code.dispatch_ready=prepare_link(code.dispatch,e.rva,0xE9,stub);
        if(code.dispatch_ready)code.dispatch.modified[e.rva-code.dispatch.block+5]=0x90;
    }
    if(!code.dispatch_ready)log_event("sortie_call_skip",e.rva,e.rva,2);
    return planners!=0;
}
static bool publish_sortie(SortieCode &code) {
    if(!sortie_addr_ok)return true;
    unsigned planners=0;
    for(unsigned k=0;k<kSortieCallCount;++k)if(code.ready[k]) {
        if(sortie_call_publish(code.calls[k])) {if(k<5)++planners;}
        else log_event("sortie_call_skip",code.calls[k].site,0,3);
    }
    if(code.dispatch_ready && !publish_link(code.dispatch))log_event("sortie_call_skip",GA(GA_1EC890),0,3);
    return planners!=0;
}
static void restore_sortie(SortieCode &code) {
    restore_link(code.dispatch);
    for(unsigned k=kSortieCallCount;k>0;--k)sortie_call_restore(code.calls[k-1]);
}
