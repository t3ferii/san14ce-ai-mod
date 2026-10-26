struct MeritEntry {unsigned rva,size;unsigned char bytes[7];void *replacement;};
static const MeritEntry merit_entries[]={
    {0x232EE0,5,{0x48,0x89,0x5C,0x24,0x08},reinterpret_cast<void *>(&merit_has)},
    {0x22E310,5,{0x48,0x89,0x5C,0x24,0x08},reinterpret_cast<void *>(&merit_list)},
    {0x1EC550,6,{0x41,0x56,0x48,0x83,0xEC,0x30},reinterpret_cast<void *>(&merit_batch)},
    {0x2717D0,7,{0x48,0x8B,0xC4,0x4C,0x89,0x40,0x18},reinterpret_cast<void *>(&merit_join)},
    {0x2A9BB0,5,{0x48,0x89,0x5C,0x24,0x08},reinterpret_cast<void *>(&merit_query)},
    {0x227600,5,{0x48,0x89,0x5C,0x24,0x10},reinterpret_cast<void *>(&merit_reset)},
    {0x3565D0,7,{0x48,0x8B,0x05,0x41,0x79,0x42,0x02},reinterpret_cast<void *>(&merit_effect_text)},
    {0x21B2E0,5,{0x48,0x8B,0xC4,0x41,0x54},reinterpret_cast<void *>(&merit_normalize)}
};
struct MeritCode {CodeLink links[10];};
static bool merit_save_guards_match() {
    const unsigned char save[]={0xFF,0x10,0x48,0x83,0x7B,0x18,0x10},load[]={0xFF,0x50,0x08,0x85,0xC0};
    return !memcmp(image+0x3F7E32,save,sizeof(save)) && !memcmp(image+0x3FA5C4,load,sizeof(load)) &&
        pointer(image,0x13E69A0)==image+0x50B670 && pointer(image,0x13E60F8)==image+0x509580;
}
static bool merit_gameplay_guards_match() {
    for(const auto &entry:merit_entries)
        if(memcmp(image+entry.rva,entry.bytes,entry.size))return false;
    return pointer(image,0x13A6BB0)==image+0x2390B0 && pointer(image,0x1413600)==image+0x908820 && pointer(image,0x1413488)==image+0x878810;
}
static bool merit_guards_match() {
    return merit_save_guards_match() && merit_gameplay_guards_match();
}
static unsigned make_merit_archive_bridge(Obj at,bool save) {
    const unsigned char begin[]={0x48,0x83,0xEC,0x28,0x48,0xB8};
    memcpy(at,begin,sizeof(begin));unsigned n=sizeof(begin);
    *reinterpret_cast<void **>(at+n)=save?reinterpret_cast<void *>(&merit_file_write):
                                        reinterpret_cast<void *>(&merit_file_read);n+=8;
    const unsigned char end[]={0xFF,0xD0,0x48,0x83,0xC4,0x28};
    memcpy(at+n,end,sizeof(end));n+=sizeof(end);
    if(save) {const unsigned char cmp[]={0x48,0x83,0x7B,0x18,0x10};memcpy(at+n,cmp,sizeof(cmp));n+=sizeof(cmp);}
    else {at[n++]=0x85;at[n++]=0xC0;}
    at[n++]=0xC3;return n;
}
static bool prepare_merit(MeritCode &code,Obj relay) {
#ifdef SPLIT_BASE
    if(!merit_save_guards_match() || (mod_features.merit && !merit_gameplay_guards_match()))return false;
#else
    if(!merit_guards_match())return false;
#endif
    for(unsigned i=0;i<8;++i) {
#ifdef SPLIT_BASE
        if(!mod_features.merit)continue;
#endif
        const auto &entry=merit_entries[i];Obj stub=relay+512+i*64,gate=stub+32;
        absolute_jump(stub,entry.replacement);memcpy(gate,entry.bytes,entry.size);
        if(entry.rva==0x3565D0) {
            // Relocate its complete RIP-relative root-pointer load.
            intptr_t delta=reinterpret_cast<intptr_t>(image+0x277DF18)-reinterpret_cast<intptr_t>(gate+7);
            if(delta<INT32_MIN || delta>INT32_MAX)return false;
            int32_t relative=static_cast<int32_t>(delta);memcpy(gate+3,&relative,4);
        }
        absolute_jump(gate+entry.size,image+entry.rva+entry.size);
        if(!prepare_link(code.links[i],entry.rva,0xE9,stub))return false;
        for(unsigned j=5;j<entry.size;++j)code.links[i].modified[entry.rva-code.links[i].block+j]=0x90;
    }
    original_merit_has=reinterpret_cast<MeritHas>(relay+544);
    original_merit_list=reinterpret_cast<MeritList>(relay+608);
    original_merit_batch=reinterpret_cast<MeritBatch>(relay+672);
    original_merit_join=reinterpret_cast<MeritJoin>(relay+736);
    original_merit_query=reinterpret_cast<MeritQuery>(relay+800);
    original_merit_reset=reinterpret_cast<MeritReset>(relay+864);
    original_merit_text=reinterpret_cast<MeritText>(relay+928);
    original_merit_normalize=reinterpret_cast<MeritNormalize>(relay+992);
    make_merit_archive_bridge(relay+1024,true);make_merit_archive_bridge(relay+1056,false);
    if(!prepare_link(code.links[8],0x3F7E32,0xE8,relay+1024) ||
       !prepare_link(code.links[9],0x3FA5C4,0xE8,relay+1056))return false;
    code.links[8].modified[7]=0x90;code.links[8].modified[8]=0x90;return true;
}
static bool publish_merit(MeritCode &code) {
    // Links 8/9 and the save/load hosts also carry the warehouse sidecar, so
    // they stay published when the merit gameplay switch is off.
    for(unsigned i=0;i<10;++i)if((mod_features.merit || i>=8) && !publish_link(code.links[i]))return false;
    if(!patch_pointer(0x13E69A0,0x50B670,reinterpret_cast<void *>(&merit_save_host)) ||
       !patch_pointer(0x13E60F8,0x509580,reinterpret_cast<void *>(&merit_load_host)))return false;
    if(!mod_features.merit)return true;
    return patch_pointer(0x13A6BB0,0x2390B0,reinterpret_cast<void *>(&merit_person_read)) &&
           patch_pointer(0x1413600,0x908820,reinterpret_cast<void *>(&merit_profile)) &&
           patch_pointer(0x1413488,0x878810,reinterpret_cast<void *>(&merit_profile_tick));
}
static void restore_merit(MeritCode &code) {
    merit_scroll_stop();
    restore_pointer(0x13E69A0,0x50B670,reinterpret_cast<void *>(&merit_save_host));
    restore_pointer(0x13E60F8,0x509580,reinterpret_cast<void *>(&merit_load_host));
    restore_pointer(0x13A6BB0,0x2390B0,reinterpret_cast<void *>(&merit_person_read));
    restore_pointer(0x1413600,0x908820,reinterpret_cast<void *>(&merit_profile));
    restore_pointer(0x1413488,0x878810,reinterpret_cast<void *>(&merit_profile_tick));
    for(auto &link:code.links)restore_link(link);
}
