#include "game_addresses.h"
#include <array>
#include <utility>
struct SortieEntry {unsigned rva,size;unsigned char bytes[16];void *replacement;void **gate;};
static const SortieEntry sortie_entries[]={
    {GA(GA_49900),6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0x58},reinterpret_cast<void *>(&sortie_plan8),reinterpret_cast<void **>(&original_sortie_planners[0])},
    {GA(GA_478A0),6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0xD8},reinterpret_cast<void *>(&sortie_plan9),reinterpret_cast<void **>(&original_sortie_planners[1])},
    {GA(GA_4C6A0),6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0xC8},reinterpret_cast<void *>(&sortie_plan10),reinterpret_cast<void **>(&original_sortie_planners[2])},
    {GA(GA_4B830),6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0x68,0x88},reinterpret_cast<void *>(&sortie_plan11),reinterpret_cast<void **>(&original_sortie_planners[3])},
    {GA(GA_45790),6,{0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0xA8,0xD8},reinterpret_cast<void *>(&sortie_plan22),reinterpret_cast<void **>(&original_sortie_planners[4])},
    {GA(GA_1EDC30),6,{0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0x6C,0x24},reinterpret_cast<void *>(&sortie_dispatch),reinterpret_cast<void **>(&original_sortie_dispatch)},
    {GA(GA_2B2550),6,{0x40,0x57,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x81,0xEC,0xF8,0x02,0x00,0x00,0x48},reinterpret_cast<void *>(&sortie_movement),reinterpret_cast<void **>(&original_sortie_movement)},
    {GA(GA_185710),5,{0x48,0x89,0x5C,0x24,0x18,0x48,0x89,0x54,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41},reinterpret_cast<void *>(&sortie_search),reinterpret_cast<void **>(&original_sortie_search)},
};
// User decision 2026-10-30: preserve native entries for R42d / Batteki 1.7.
static const unsigned sortie_plan_sites[]={GA(GA_30BDC),GA(GA_30BF1),GA(GA_30C06),GA(GA_30BC7),GA(GA_30C1F)};
static const unsigned sortie_search_roots[]={GA(GA_45790),GA(GA_478A0),GA(GA_49900),GA(GA_4B830),GA(GA_4C6A0),GA(GA_A3370),GA(GA_1EDC30),GA(GA_22E690),GA(GA_22F4A0),GA(GA_26E260),GA(GA_2ECEA0),GA(GA_2ECF00),GA(GA_2F05A0),GA(GA_331780)};
// Depth <=8 E8 graph; direct tail JMPs cost zero, chained .pdata spans coalesced.
static const unsigned sortie_search_sites[]={
    GA(GA_47145),GA(GA_472AB),GA(GA_491E1),GA(GA_4932B),GA(GA_4B246),GA(GA_4B3AA),GA(GA_4BFDD),GA(GA_4E076),
    GA(GA_4E1DD),GA(GA_B6E69),GA(GA_BC6E4),GA(GA_1EE096),GA(GA_275281),GA(GA_27546C),GA(GA_275E6C),GA(GA_278389),
    GA(GA_2AEF87),GA(GA_2B0DA6),GA(GA_2B3EB2),GA(GA_2B4383),GA(GA_303FC9)
};
// Every E8 -> 2B0290 in the matching unpacked .text (CRC 6C7F386E).
static const unsigned sortie_movement_sites[]={
    GA(GA_B8EF),GA(GA_B95A),GA(GA_68A74),GA(GA_712BB),GA(GA_715AC),GA(GA_715F2),GA(GA_74006),GA(GA_744B4),GA(GA_744F9),GA(GA_74D11),
    GA(GA_74D56),GA(GA_87DA0),GA(GA_982F6),GA(GA_9830F),GA(GA_9A05B),GA(GA_9B334),GA(GA_9BA2D),GA(GA_9C48C),GA(GA_9FB56),GA(GA_A0F02),
    GA(GA_A197A),GA(GA_A1F24),GA(GA_A3EC5),GA(GA_B805D),GA(GA_C0305),GA(GA_CDF5B),GA(GA_CE4DB),GA(GA_CE51C),GA(GA_CE6B4),GA(GA_CE70A),
    GA(GA_CEB24),GA(GA_CF1C3),GA(GA_CF1E4),GA(GA_CF3ED),GA(GA_D17DF),GA(GA_D1821),GA(GA_184FF9),GA(GA_185B21),GA(GA_186462),GA(GA_18793A),
    GA(GA_1CBA11),GA(GA_25537B),GA(GA_25539D),GA(GA_267EE2),GA(GA_26AB48),GA(GA_342F96),GA(GA_343099),GA(GA_9262EB),GA(GA_92630D)
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
// local149 tribe_terrain_path (user 2026-11-14 "이민족 사이진형이나 장사 진형같은 경우는 숲이나 저산 중산 고산으로 이동 안하면 벌점을
// 줘서 숲 이나 산으로 이동하도록 유도 ... 중계경로를 가도로 잡는 경우가 있어"): 장사 (6) and 사이 (14) get 1.5x on terrain 5, 6, 8..11
// (숲·밀림·저산·중산·고산·산길; 22F7A0 rule, San14Sim combat_formulas.h). Only at the two path-cost CALLs (184FF9 in the A* 184D60,
// 18793A in its cost scaler 187880, cost = terrain cost / this mobility) and only for a computer army with a hostile army within
// 15 hexes (with none it takes the road as before): any other land hex costs
// kTribePathFactor^-1 times as much there, so the route keeps to forest and mountains. Real daily movement, the panel and the
// player (direct or delegated) are untouched; water, castle / gate / port hexes and other formations too. Off = native value.
static constexpr float kTribePathFactor=0.3f;
static constexpr unsigned char kTribeMoraleFloor=50;   // local153: below this morale the route is the native one
static bool tribe_path_site(unsigned k) {
    const unsigned site=sortie_movement_sites[k];
    return site==GA(GA_184FF9) || site==GA(GA_18793A);
}
// local149 (user "장사나 사이진형도 가도로 이동은 해야해 그런데 적이 주변 15칸에 있으면 경로 계산할때 그렇게 적용"): only while a hostile
// army (2CC630) is within kTribeFoeReach hexes of the army; answered once per army and game day (a stale answer is harmless).
static constexpr int kTribeFoeReach=15;
static bool tribe_foe_near(Obj army) {
    static volatile LONG day_of[501];static volatile LONG answer[501];
    const unsigned id=army_id(army);if(!id || id>500)return false;
    const LONG day=game_day()+1;
    if(day_of[id]==day)return answer[id]!=0;
    const unsigned own=army_force(army),here=word(army,0x2A);bool near_foe=false;
    if(here<48400)for(unsigned i=1;i<=500 && !near_foe;++i) {
        Obj o=table(0x7E838,i,500);if(!valid(o) || o==army)continue;
        const unsigned other=army_force(o);
        if(!other || other==own || word(o,0x2A)>=48400 || distance(here,word(o,0x2A))>kTribeFoeReach)continue;
        near_foe=fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(own,other)!=0;
    }
    InterlockedExchange(&answer[id],near_foe?1:0);InterlockedExchange(&day_of[id],day);
    return near_foe;
}
static float tribe_path(float value,Obj army,Obj formation,Obj tile) {
    if(!mod_features.tribe_terrain_path || !(value>0) || !valid(tile) || !valid(formation) || !valid(army))return value;
    const unsigned id=fn<unsigned(__fastcall *)(Obj)>(GA(GA_22F7A0))(formation);
    if(id!=6 && id!=14)return value;
    const unsigned terrain=tile[0x10];
    if(terrain==5 || terrain==6 || (terrain>=8 && terrain<=11))return value;                       // the 1.5x ground
    if(terrain==12 || terrain==13 || terrain==14 || terrain==24 || (terrain>=16 && terrain<=18))return value;   // water, bases
    // local153 (user 2026-11-14 "사이/장사진형 사기가 떨어지면 평지로 내려와야하는데"): forest and hills drain more morale every 5 days
    // (terrain +0x40: plain / road 1, forest 2, 고산 / 산길 3), so under kTribeMoraleFloor (army +0x1A) the army takes the native route.
    if(army[0x1A]<kTribeMoraleFloor)return value;
    return computer_side_army(army) && tribe_foe_near(army)?value*kTribePathFactor:value;
}
template<unsigned K> static float __fastcall sortie_movement_site(Obj a,Obj f,int p,int m,Obj t) {
    // local67 (user 2026-11-12 "계속 기동력 그대로 표시되는데"): every native 2B2550 call goes through these 49 call-site
    // thunks, not through sortie_movement(), so the ambush half was never applied to real movement or to the panel.
    const float value=sortie_movement_next(sortie_movement_nexts[K],a,f,p,m,t);
    const float v=season_mobility::apply(value>0 && ambush_hook::slow(a)?value*0.5f:value,a,t);   // local68: half only until the first ZOC day; local135 snow / ice
    return tribe_path_site(K)?tribe_path(v,a,f,t):v;
}
template<size_t... K> static auto sortie_search_thunks(std::index_sequence<K...>) {
    return std::array<SortieSearch,sizeof...(K)>{{&sortie_search_site<K>...}};
}
template<size_t... K> static auto sortie_movement_thunks(std::index_sequence<K...>) {
    return std::array<SortieMovement,sizeof...(K)>{{&sortie_movement_site<K>...}};
}
struct SortieCode {CodeLink dispatch{};CallLink calls[kSortieCallCount]{};bool ready[kSortieCallCount]{};bool dispatch_ready=false;};
static Obj sortie_call_target(unsigned site,unsigned native) { COST_SCOPE("sortie_links:sortie_call_target");
    if(image[site]!=0xE8)return nullptr;
    int delta;memcpy(&delta,image+site+1,4);
    uintptr_t target=reinterpret_cast<uintptr_t>(image+site+5)+delta;
    auto pe=reinterpret_cast<IMAGE_NT_HEADERS64 *>(image+reinterpret_cast<IMAGE_DOS_HEADER *>(image)->e_lfanew);
    uintptr_t begin=reinterpret_cast<uintptr_t>(image),end=begin+pe->OptionalHeader.SizeOfImage;
    return target==begin+native || target<begin || target>=end?reinterpret_cast<Obj>(target):nullptr;
}
// Validate exactly this CALL, including raw calls crossing a 16-byte block.
static bool sortie_call_publish(CallLink &c) { COST_SCOPE("sortie_links:sortie_call_publish");
    int delta;memcpy(&delta,image+c.site+1,4);
    return image[c.site]==0xE8 && delta==c.original && call_publish(c);
}
static void sortie_call_restore(CallLink &c) { COST_SCOPE("sortie_links:sortie_call_restore");
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
static bool sortie_guards_match() { COST_SCOPE("sortie_links:sortie_guards_match");
    if(!sortie_addr_ok)return true;   // family switched off: nothing of it is prepared or published
    // A changed search/movement/dispatch site never disables the other calls.
    for(unsigned i=0;i<5;++i)if(sortie_call_target(sortie_plan_sites[i],sortie_entries[i].rva))return true;
    return false;
}
static bool prepare_sortie(SortieCode &code,Obj relay) { COST_SCOPE("sortie_links:prepare_sortie");
    code={};
    if(!sortie_addr_ok)return true;
    original_sortie_dispatch=reinterpret_cast<SortieDispatch>(image+GA(GA_1EDC30));
    original_sortie_movement=reinterpret_cast<SortieMovement>(image+GA(GA_2B2550));
    original_sortie_search=reinterpret_cast<SortieSearch>(image+GA(GA_185710));
    const auto searches=sortie_search_thunks(std::make_index_sequence<kSortieSearchCount>{});
    const auto movements=sortie_movement_thunks(std::make_index_sequence<49>{});
    unsigned planners=0;
    // [4096,4096+kSortieCallCount*16): 16-byte tail jumps; native entries intact.
    for(unsigned k=0;k<kSortieCallCount;++k) {
        unsigned site=k<5?sortie_plan_sites[k]:k<kSortieSearchEnd?sortie_search_sites[k-5]:sortie_movement_sites[k-kSortieSearchEnd];
        unsigned native=k<5?sortie_entries[k].rva:k<kSortieSearchEnd?GA(GA_185710):GA(GA_2B2550);
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
static bool publish_sortie(SortieCode &code) { COST_SCOPE("sortie_links:publish_sortie");
    if(!sortie_addr_ok)return true;
    unsigned planners=0;
    for(unsigned k=0;k<kSortieCallCount;++k)if(code.ready[k]) {
        if(sortie_call_publish(code.calls[k])) {if(k<5)++planners;}
        else log_event("sortie_call_skip",code.calls[k].site,0,3);
    }
    if(code.dispatch_ready && !publish_link(code.dispatch))log_event("sortie_call_skip",GA(GA_1EDC30),0,3);
    return planners!=0;
}
static void restore_sortie(SortieCode &code) { COST_SCOPE("sortie_links:restore_sortie");
    restore_link(code.dispatch);
    for(unsigned k=kSortieCallCount;k>0;--k)sortie_call_restore(code.calls[k-1]);
}
