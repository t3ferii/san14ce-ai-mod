#include "game_addresses.h"
/* One owner: SAN14_Convenience, alongside its existing combat-pair hook. */
struct ExpertCall {unsigned site,size;unsigned char bytes[8];void *replacement;unsigned char opcode;};
static const ExpertCall expert_calls[]={
    {GA(GA_2B0B69),5,{0xE8,0x92,0x98,0x13,0x00},reinterpret_cast<void *>(&expert_native_adjacency_option),0xE8},
    {GA(GA_6566E0),6,{0x40,0x57,0x41,0x56,0x41,0x57},reinterpret_cast<void *>(&expert_populate),0xE9},
    {GA(GA_6362DB),5,{0xE8,0x60,0x89,0xF8,0xFF},reinterpret_cast<void *>(&expert_number_current),0xE8},
    {GA(GA_2A8D40),7,{0x48,0x8B,0xC4,0x44,0x89,0x48,0x20},reinterpret_cast<void *>(&expert_unit_power),0xE9},
    {GA(GA_173B44),5,{0xE8,0x37,0x7D,0xFF,0xFF},reinterpret_cast<void *>(&expert_field_stats),0xE8},
    {GA(GA_5F14DA),5,{0xE8,0x51,0xCF,0x33,0x00},reinterpret_cast<void *>(&expert_number_factory),0xE8}
};
static constexpr unsigned expert_call_count=sizeof(expert_calls)/sizeof(expert_calls[0]);
struct ExpertPointer {unsigned slot,original;void *replacement;void **next;};
static const ExpertPointer expert_pointers[]={
    {GA(GA_141BE88),GA(GA_5A73A0),reinterpret_cast<void *>(&expert_destroy),&next_expert_destroy},
    {GA(GA_141BCC8),GA(GA_5B9110),reinterpret_cast<void *>(&expert_list_bind),&next_expert_list_bind},
    {GA(GA_141BCF8),GA(GA_5A7450),reinterpret_cast<void *>(&expert_row_destroy),&next_expert_row_destroy},
    {GA(GA_141BD20),GA(GA_6475C0),reinterpret_cast<void *>(&expert_row_leave),&next_expert_row_leave},
    {GA(GA_14296B8),GA(GA_65E810),reinterpret_cast<void *>(&expert_slider_event),&next_expert_slider},
    {GA(GA_1429728),GA(GA_65EDD0),reinterpret_cast<void *>(&expert_choice_event),&next_expert_choice},
    {GA(GA_14296F0),GA(GA_65F890),reinterpret_cast<void *>(&expert_number_event),&next_expert_number_event},
    {GA(GA_1429798),GA(GA_65E8C0),reinterpret_cast<void *>(&expert_number_confirm),&next_expert_number_confirm}
};
#ifdef LAYOUT_ROUTED
// User 2026-11-05 Layout compatibility design 8.3: in production the expert UI hooks go through the SAN14_Layout.dll
// manager. The 6566E0 entry patch is replaced by its three callers; 6362DB / 5F14DA are E8 registrations; all eight
// vtable slots are chained/yielded by the manager. Only the three non-UI links (2B0B69, 2A8D40, 173B44) stay here.
static bool expert_routed_site(unsigned site) {return site==GA(GA_6566E0) || site==GA(GA_6362DB) || site==GA(GA_5F14DA);}
#else
static bool expert_routed_site(unsigned) {return false;}
#endif
struct ExpertCode {CodeLink links[expert_call_count];Obj relay;};
static bool expert_guards_match() {
    for(const auto &e:expert_calls) {
#ifdef LAYOUT_ROUTED
        // The two routed E8 sites are judged by the manager (chain/refuse); the 6566E0 entry must stay original.
        if(e.site==GA(GA_6362DB) || e.site==GA(GA_5F14DA))continue;
#endif
        if(ga::mcmp_at(image,e.site,e.bytes,e.size))return false;
    }
#ifndef LAYOUT_ROUTED
    for(const auto &e:expert_pointers)if(pointer(image,e.slot)!=image+e.original)return false;
#endif
    return true;
}
static bool prepare_expert(ExpertCode &code) {
    if(!expert_guards_match())return false;
    code.relay=allocate_near(image+GA(GA_5B919E));if(!code.relay)return false;
    bool ready=true;
    for(unsigned i=0;i<expert_call_count && ready;++i) {
        absolute_jump(code.relay+i*32,expert_calls[i].replacement);
        if(expert_routed_site(expert_calls[i].site))continue;
        ready=prepare_link(code.links[i],expert_calls[i].site,expert_calls[i].opcode,code.relay+i*32);
    }
    memcpy(code.relay+256,expert_calls[1].bytes,6);
    absolute_jump(code.relay+262,image+GA(GA_6566E6));
    original_expert_populate=reinterpret_cast<ExpertPopulate>(code.relay+256);
    memcpy(code.relay+512,expert_calls[3].bytes,7);
    absolute_jump(code.relay+519,image+GA(GA_2A8D47));
    original_expert_unit_power=reinterpret_cast<ExpertUnitPower>(code.relay+512);
    if(ready)for(unsigned i=0;i<expert_call_count;++i)
        if(!expert_routed_site(expert_calls[i].site))
            for(unsigned n=5;n<expert_calls[i].size;++n)code.links[i].modified[n]=0x90;
    DWORD old=0;
    if(!ready || !VirtualProtect(code.relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),code.relay,4096)) {
        VirtualFree(code.relay,0,MEM_RELEASE);code={};return false;
    }
    return true;
}
#ifdef LAYOUT_ROUTED
static bool register_expert_routed() {
    // Calls first (populate sites, number_current, number_factory), then the vtable slots (same order as the former links).
    if(layout_client::call(GA(GA_5F10F7),reinterpret_cast<void *>(&expert_populate_site<0>),&next_expert_populate[0])<=0 ||
       layout_client::call(GA(GA_5F5B27),reinterpret_cast<void *>(&expert_populate_site<1>),&next_expert_populate[1])<=0 ||
       layout_client::call(GA(GA_65EAAD),reinterpret_cast<void *>(&expert_populate_site<2>),&next_expert_populate[2])<=0 ||
       layout_client::call(GA(GA_6362DB),reinterpret_cast<void *>(&expert_number_current),&next_expert_number_current)<=0 ||
       layout_client::call(GA(GA_5F14DA),reinterpret_cast<void *>(&expert_number_factory),&next_expert_number_factory)<=0)return false;
    for(const auto &e:expert_pointers)
        if(layout_client::vt(e.slot,e.replacement,e.next)<=0)return false;
    return true;
}
#endif
static bool publish_expert(ExpertCode &code) {
    for(unsigned i=0;i<expert_call_count;++i)
        if(!expert_routed_site(expert_calls[i].site) && !publish_link(code.links[i]))return false;
#ifdef LAYOUT_ROUTED
    return register_expert_routed();
#else
    for(const auto &e:expert_pointers)if(!patch_pointer(e.slot,e.original,e.replacement))return false;
    return true;
#endif
}
static void restore_expert(ExpertCode &code) {
#ifndef LAYOUT_ROUTED
    for(const auto &e:expert_pointers)restore_pointer(e.slot,e.original,e.replacement);
#endif
    for(unsigned i=expert_call_count;i>0;--i)if(!expert_routed_site(expert_calls[i-1].site))restore_link(code.links[i-1]);
    // A native UI callback may already have entered the relay. Retain it,
    // matching the existing split-module rollback lifetime rule.
}
