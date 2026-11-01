#include <array>
#include <utility>
struct SortieEntry {unsigned rva,size;unsigned char bytes[16];void *replacement;void **gate;};
static const SortieEntry sortie_entries[]={
    {0x48CE0,6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0x58},reinterpret_cast<void *>(&sortie_plan8),reinterpret_cast<void **>(&original_sortie_planners[0])},
    {0x46B80,6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0xD8},reinterpret_cast<void *>(&sortie_plan9),reinterpret_cast<void **>(&original_sortie_planners[1])},
    {0x4BA50,6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0xC8},reinterpret_cast<void *>(&sortie_plan10),reinterpret_cast<void **>(&original_sortie_planners[2])},
    {0x4ABE0,6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0x68,0x88},reinterpret_cast<void *>(&sortie_plan11),reinterpret_cast<void **>(&original_sortie_planners[3])},
    {0x44A80,6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0xD8},reinterpret_cast<void *>(&sortie_plan22),reinterpret_cast<void **>(&original_sortie_planners[4])},
    {0x1EC890,6,{0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0x6C,0x24},reinterpret_cast<void *>(&sortie_dispatch),reinterpret_cast<void **>(&original_sortie_dispatch)},
    {0x2B0290,5,{0x40,0x53,0x56,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x81,0xEC,0xF0,0x02,0x00,0x00},reinterpret_cast<void *>(&sortie_movement),reinterpret_cast<void **>(&original_sortie_movement)},
    {0x1843E0,5,{0x48,0x89,0x5C,0x24,0x18,0x48,0x89,0x54,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41},reinterpret_cast<void *>(&sortie_search),reinterpret_cast<void **>(&original_sortie_search)},
};
// User decision 2026-10-30: preserve native entries for R42d / Batteki 1.7.
static constexpr unsigned sortie_plan_sites[]={0x3007C,0x30091,0x300A6,0x30067,0x300BF};
static constexpr unsigned sortie_search_roots[]={0x44A80,0x46B80,0x48CE0,0x4ABE0,0x4BA50,0xA2410,0x1EC890,0x22CC40,0x22DA10,0x26C940,0x2EAAE0,0x2EAB40,0x2EE1E0,0x32F500};
// Depth <=8 E8 graph; direct tail JMPs cost zero, chained .pdata spans coalesced.
static constexpr unsigned sortie_search_sites[]={
    0x46425,0x4658B,0x484D7,0x4862B,0x4A5B6,0x4A70E,0x4B38D,0x4D426,
    0x4D58D,0xB5DD9,0xBB654,0x1ECCF6,0x273951,0x273B3C,0x27453C,0x276A59,
    0x2ACC57,0x2AEA76,0x2B1BF2,0x2B20C3,0x301BD9
};
// Every E8 -> 2B0290 in the matching unpacked .text (CRC 6C7F386E).
static constexpr unsigned sortie_movement_sites[]={
    0xB98F,0xB9FA,0x67DC4,0x7031B,0x7060C,0x70652,0x73026,0x734D4,0x73519,0x73D31,
    0x73D76,0x86D70,0x97396,0x973AF,0x990FB,0x9A3D4,0x9AACD,0x9B52C,0x9EBF6,0x9FFA2,
    0xA0A1A,0xA0FC4,0xA2F65,0xB6FCD,0xBF275,0xCCECB,0xCD44B,0xCD48C,0xCD624,0xCD67A,
    0xCDA94,0xCE133,0xCE154,0xCE35D,0xD074F,0xD0791,0x183CC9,0x1847F1,0x185132,0x1865AA,
    0x1CA6AF,0x25392B,0x25394D,0x266532,0x269198,0x340CB6,0x340DB9,0x923E1B,0x923E3D
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
static bool sortie_guards_match() {
    // A changed search/movement/dispatch site never disables the other calls.
    for(unsigned i=0;i<5;++i)if(sortie_call_target(sortie_plan_sites[i],sortie_entries[i].rva))return true;
    return false;
}
static bool prepare_sortie(SortieCode &code,Obj relay) {
    code={};
    original_sortie_dispatch=reinterpret_cast<SortieDispatch>(image+0x1EC890);
    original_sortie_movement=reinterpret_cast<SortieMovement>(image+0x2B0290);
    original_sortie_search=reinterpret_cast<SortieSearch>(image+0x1843E0);
    const auto searches=sortie_search_thunks(std::make_index_sequence<kSortieSearchCount>{});
    const auto movements=sortie_movement_thunks(std::make_index_sequence<49>{});
    unsigned planners=0;
    // [4096,4096+kSortieCallCount*16): 16-byte tail jumps; native entries intact.
    for(unsigned k=0;k<kSortieCallCount;++k) {
        unsigned site=k<5?sortie_plan_sites[k]:k<kSortieSearchEnd?sortie_search_sites[k-5]:sortie_movement_sites[k-kSortieSearchEnd];
        unsigned native=k<5?sortie_entries[k].rva:k<kSortieSearchEnd?0x1843E0:0x2B0290;
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
    if(!memcmp(image+e.rva,e.bytes,16)) {
        absolute_jump(stub,e.replacement);memcpy(gate,e.bytes,e.size);
        absolute_jump(gate+e.size,image+e.rva+e.size);*e.gate=gate;
        code.dispatch_ready=prepare_link(code.dispatch,e.rva,0xE9,stub);
        if(code.dispatch_ready)code.dispatch.modified[e.rva-code.dispatch.block+5]=0x90;
    }
    if(!code.dispatch_ready)log_event("sortie_call_skip",e.rva,e.rva,2);
    return planners!=0;
}
static bool publish_sortie(SortieCode &code) {
    unsigned planners=0;
    for(unsigned k=0;k<kSortieCallCount;++k)if(code.ready[k]) {
        if(sortie_call_publish(code.calls[k])) {if(k<5)++planners;}
        else log_event("sortie_call_skip",code.calls[k].site,0,3);
    }
    if(code.dispatch_ready && !publish_link(code.dispatch))log_event("sortie_call_skip",0x1EC890,0,3);
    return planners!=0;
}
static void restore_sortie(SortieCode &code) {
    restore_link(code.dispatch);
    for(unsigned k=kSortieCallCount;k>0;--k)sortie_call_restore(code.calls[k-1]);
}
