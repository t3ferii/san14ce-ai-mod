/* One owner: SAN14_Convenience, alongside its existing combat-pair hook. */
struct ExpertCall {unsigned site,size;unsigned char bytes[8];void *replacement;unsigned char opcode;};
static const ExpertCall expert_calls[]={
    {0x2B0B69,5,{0xE8,0x92,0x98,0x13,0x00},reinterpret_cast<void *>(&expert_native_adjacency_option),0xE8},
    {0x6566E0,6,{0x40,0x57,0x41,0x56,0x41,0x57},reinterpret_cast<void *>(&expert_populate),0xE9},
    {0x6362DB,5,{0xE8,0x60,0x89,0xF8,0xFF},reinterpret_cast<void *>(&expert_number_current),0xE8},
    {0x2A8D40,7,{0x48,0x8B,0xC4,0x44,0x89,0x48,0x20},reinterpret_cast<void *>(&expert_unit_power),0xE9},
    {0x173B44,5,{0xE8,0x37,0x7D,0xFF,0xFF},reinterpret_cast<void *>(&expert_field_stats),0xE8},
    {0x5F14DA,5,{0xE8,0x51,0xCF,0x33,0x00},reinterpret_cast<void *>(&expert_number_factory),0xE8}
};
static constexpr unsigned expert_call_count=sizeof(expert_calls)/sizeof(expert_calls[0]);
struct ExpertPointer {unsigned slot,original;void *replacement;};
static const ExpertPointer expert_pointers[]={
    {0x141BE88,0x5A73A0,reinterpret_cast<void *>(&expert_destroy)},
    {0x141BCC8,0x5B9110,reinterpret_cast<void *>(&expert_list_bind)},
    {0x141BCF8,0x5A7450,reinterpret_cast<void *>(&expert_row_destroy)},
    {0x141BD20,0x6475C0,reinterpret_cast<void *>(&expert_row_leave)},
    {0x14296B8,0x65E810,reinterpret_cast<void *>(&expert_slider_event)},
    {0x1429728,0x65EDD0,reinterpret_cast<void *>(&expert_choice_event)},
    {0x14296F0,0x65F890,reinterpret_cast<void *>(&expert_number_event)},
    {0x1429798,0x65E8C0,reinterpret_cast<void *>(&expert_number_confirm)}
};
struct ExpertCode {CodeLink links[expert_call_count];Obj relay;};
static bool expert_guards_match() {
    for(const auto &e:expert_calls)if(memcmp(image+e.site,e.bytes,e.size))return false;
    for(const auto &e:expert_pointers)if(pointer(image,e.slot)!=image+e.original)return false;
    return true;
}
static bool prepare_expert(ExpertCode &code) {
    if(!expert_guards_match())return false;
    code.relay=allocate_near(image+0x5B919E);if(!code.relay)return false;
    bool ready=true;
    for(unsigned i=0;i<expert_call_count && ready;++i) {
        absolute_jump(code.relay+i*32,expert_calls[i].replacement);
        ready=prepare_link(code.links[i],expert_calls[i].site,expert_calls[i].opcode,code.relay+i*32);
    }
    memcpy(code.relay+256,expert_calls[1].bytes,6);
    absolute_jump(code.relay+262,image+0x6566E6);
    original_expert_populate=reinterpret_cast<ExpertPopulate>(code.relay+256);
    memcpy(code.relay+512,expert_calls[3].bytes,7);
    absolute_jump(code.relay+519,image+0x2A8D47);
    original_expert_unit_power=reinterpret_cast<ExpertUnitPower>(code.relay+512);
    if(ready)for(unsigned i=0;i<expert_call_count;++i)
        for(unsigned n=5;n<expert_calls[i].size;++n)code.links[i].modified[n]=0x90;
    DWORD old=0;
    if(!ready || !VirtualProtect(code.relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),code.relay,4096)) {
        VirtualFree(code.relay,0,MEM_RELEASE);code={};return false;
    }
    return true;
}
static bool publish_expert(ExpertCode &code) {
    for(auto &link:code.links)if(!publish_link(link))return false;
    for(const auto &e:expert_pointers)if(!patch_pointer(e.slot,e.original,e.replacement))return false;
    return true;
}
static void restore_expert(ExpertCode &code) {
    for(const auto &e:expert_pointers)restore_pointer(e.slot,e.original,e.replacement);
    for(unsigned i=expert_call_count;i>0;--i)restore_link(code.links[i-1]);
    // A native UI callback may already have entered the relay. Retain it,
    // matching the existing split-module rollback lifetime rule.
}
