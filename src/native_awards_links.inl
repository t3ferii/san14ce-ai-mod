#include "game_addresses.h"
struct AwardEntry {unsigned rva,size;unsigned char bytes[16];void *replacement;void **gate;};
static const AwardEntry award_entries[]={
    {GA(GA_1F0300),6,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x8b,0x49,0x08,0xe8,0xee,0xf1},reinterpret_cast<void *>(&award_transfer),reinterpret_cast<void **>(&original_award_transfer)},
    {GA(GA_92610),5,{0x48,0x89,0x54,0x24,0x10,0x53,0x48,0x83,0xec,0x30,0x48,0xc7,0x44,0x24,0x28,0xfe},reinterpret_cast<void *>(&award_iterator),reinterpret_cast<void **>(&original_award_iterator)},
    {GA(GA_6D6920),6,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9,0x48,0x89,0x6c,0x24,0x38,0x48,0x89},reinterpret_cast<void *>(&award_warehouse_items),reinterpret_cast<void **>(&original_award_items)},
    {GA(GA_6D7AA0),8,{0x48,0x8b,0xc4,0x57,0x48,0x83,0xec,0x70,0x48,0xc7,0x40,0xa8,0xfe,0xff,0xff,0xff},reinterpret_cast<void *>(&award_recipients),reinterpret_cast<void **>(&original_award_recipients)},
    {GA(GA_734C20),6,{0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x81,0x78,0x04,0x00,0x00,0x48,0x8b,0xd9},reinterpret_cast<void *>(&award_ui),reinterpret_cast<void **>(&original_award_ui)},
    {GA(GA_6CD4E0),6,{0x40,0x55,0x48,0x83,0xec,0x50,0x48,0x8b,0x81,0x78,0x04,0x00,0x00,0x48,0x8b,0xe9},reinterpret_cast<void *>(&award_manual),reinterpret_cast<void **>(&original_award_manual)},
    {GA(GA_1EDB40),8,{0x48,0x8b,0xc4,0x57,0x48,0x83,0xec,0x70,0x48,0xc7,0x40,0xa8,0xfe,0xff,0xff,0xff},reinterpret_cast<void *>(&award_post_people),reinterpret_cast<void **>(&original_post_people)},
    {GA(GA_1F1750),5,{0x40,0x53,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x83,0xec},reinterpret_cast<void *>(&award_post_batch),reinterpret_cast<void **>(&original_post_batch)},
    {GA(GA_7542C0),5,{0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x57,0x48,0x83,0xec,0x20,0x48},reinterpret_cast<void *>(&award_post_select),reinterpret_cast<void **>(&original_post_select)},
    {GA(GA_6E14F0),7,{0x4c,0x8b,0xdc,0x49,0x89,0x53,0x10,0x57,0x48,0x83,0xec,0x70,0x49,0xc7,0x43,0xb0},reinterpret_cast<void *>(&award_personal_view),reinterpret_cast<void **>(&original_award_view)},
    {GA(GA_74B120),5,{0x48,0x8b,0xc4,0x55,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x8d,0xa8,0x98,0xfb},reinterpret_cast<void *>(&award_post_picker),reinterpret_cast<void **>(&original_post_picker)},
};

struct AwardCode {CodeLink links[sizeof(award_entries)/sizeof(award_entries[0])];};
static bool award_guards_match() {
    for(const auto &entry:award_entries)if(ga::mcmp_at(image,entry.rva,entry.bytes,16))return false;
    const unsigned char ai[]={0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x30,0x33,0xC0,0x48,0x8B,0xF9,0x48};
    return pointer(image,GA(GA_13491B0))==image+GA(GA_535E0) && !ga::mcmp(image,GA_535E0,0,ai,16) &&
        pointer(image,GA(GA_134C6E8))==image+GA(GA_333320);
}
static bool prepare_awards(AwardCode &code,Obj relay) {
    if(!award_guards_match())return false;
    memcpy(award_equipped_vtable,image+GA(GA_134C6E0),32);memcpy(award_storage_vtable,image+GA(GA_134C6E0),32);
    award_equipped_vtable[1]=reinterpret_cast<void *>(&award_equipment_predicate);
    award_storage_vtable[1]=reinterpret_cast<void *>(&award_storage_predicate);
    for(unsigned i=0;i<sizeof(award_entries)/sizeof(award_entries[0]);++i) {
        const auto &entry=award_entries[i];Obj stub=relay+1728+i*64,gate=stub+32;
        absolute_jump(stub,entry.replacement);memcpy(gate,entry.bytes,entry.size);
        absolute_jump(gate+entry.size,image+entry.rva+entry.size);*entry.gate=gate;
        if(!prepare_link(code.links[i],entry.rva,0xE9,stub))return false;
        for(unsigned j=5;j<entry.size;++j)code.links[i].modified[entry.rva-code.links[i].block+j]=0x90;
    }
    return true;
}
static bool publish_awards(AwardCode &code) {
    for(auto &link:code.links)if(!publish_link(link))return false;
    return patch_pointer(GA(GA_13491B0),GA(GA_535E0),reinterpret_cast<void *>(&award_native_ai_command));
}
static void restore_awards(AwardCode &code) {
    restore_pointer(GA(GA_13491B0),GA(GA_535E0),reinterpret_cast<void *>(&award_native_ai_command));
    for(auto &link:code.links)restore_link(link);
}
