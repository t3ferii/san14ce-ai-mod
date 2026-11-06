#include "game_addresses.h"
struct MeritEntry {unsigned rva,size;unsigned char bytes[7];void *replacement;};
static const MeritEntry merit_entries[]={
    {GA(GA_232EE0),5,{0x48,0x89,0x5C,0x24,0x08},reinterpret_cast<void *>(&merit_has)},
    {GA(GA_22E310),5,{0x48,0x89,0x5C,0x24,0x08},reinterpret_cast<void *>(&merit_list)},
    {GA(GA_1EC550),6,{0x41,0x56,0x48,0x83,0xEC,0x30},reinterpret_cast<void *>(&merit_batch)},
    {GA(GA_2717D0),7,{0x48,0x8B,0xC4,0x4C,0x89,0x40,0x18},reinterpret_cast<void *>(&merit_join)},
    {GA(GA_2A9BB0),5,{0x48,0x89,0x5C,0x24,0x08},reinterpret_cast<void *>(&merit_query)},
    {GA(GA_227600),5,{0x48,0x89,0x5C,0x24,0x10},reinterpret_cast<void *>(&merit_reset)},
    {GA(GA_3565D0),7,{0x48,0x8B,0x05,0x41,0x79,0x42,0x02},reinterpret_cast<void *>(&merit_effect_text)},
    {GA(GA_21B2E0),5,{0x48,0x8B,0xC4,0x41,0x54},reinterpret_cast<void *>(&merit_normalize)}
};
// links[10] = dialog toggle guard (7705F0). It has its own failure: an unresolved address, changed bytes or a failed publish only
// switches the dialog block off (merit_dialog_hook log); the confirm-time lock (links[2]) and the rest of merit keep working.
static const unsigned char merit_dialog_bytes[5]={0x40,0x53,0x55,0x56,0x57};
// links[11] = native lock display (6D9790 slot states, entry push rsi/push rdi/push r15), links[12] = auto-fill guard
// (6C40B0, entry push rdi/push r13/push r15). Each has its own failure like links[10]: only that part switches off
// (merit_lock_hook / merit_autofill_hook logs); the click guard and the confirm-time lock keep working.
static const unsigned char merit_slot_bytes[5]={0x40,0x56,0x57,0x41,0x57};
static const unsigned char merit_autofill_bytes[6]={0x40,0x57,0x41,0x55,0x41,0x57};
struct MeritCode {CodeLink links[13]{};bool dialog_ready=false,lock_ready=false,autofill_ready=false;};
// Routed builds (Layout manager): vtable slot ownership is judged by the manager, not compared here.
static bool merit_save_guards_match() {
    const unsigned char save[]={0xFF,0x10,0x48,0x83,0x7B,0x18,0x10},load[]={0xFF,0x50,0x08,0x85,0xC0};
    return !ga::mcmp(image,GA_3F7E32,0,save,sizeof(save)) && !ga::mcmp(image,GA_3FA5C4,0,load,sizeof(load))
#ifndef LAYOUT_ROUTED
        && pointer(image,GA(GA_13E69A0))==image+GA(GA_50B670) && pointer(image,GA(GA_13E60F8))==image+GA(GA_509580)
#endif
        ;
}
static bool merit_gameplay_guards_match() {
    for(const auto &entry:merit_entries)
        if(ga::mcmp_at(image,entry.rva,entry.bytes,entry.size))return false;
#ifdef LAYOUT_ROUTED
    return true;
#else
    return pointer(image,GA(GA_13A6BB0))==image+GA(GA_2390B0) && pointer(image,GA(GA_1413600))==image+GA(GA_908820) && pointer(image,GA(GA_1413488))==image+GA(GA_878810);
#endif
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
        if(entry.rva==GA(GA_3565D0)) {
            // Relocate its complete RIP-relative root-pointer load.
            intptr_t delta=reinterpret_cast<intptr_t>(image+GA(GA_277DF18))-reinterpret_cast<intptr_t>(gate+7);
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
    code.links[10]={};code.links[11]={};code.links[12]={};code.dialog_ready=code.lock_ready=code.autofill_ready=false;
#ifdef SPLIT_BASE
    if(mod_features.merit)
#endif
    {
        // Stub/gate at relay+1088/1120 (clear of the entry stubs and the two archive bridges).
        unsigned rva=GA(GA_7705F0);bool ok=rva && GA(GA_781DA0) && !ga::mcmp_at(image,rva,merit_dialog_bytes,5);
        if(ok) {
            Obj stub=relay+1088,gate=stub+32;
            absolute_jump(stub,reinterpret_cast<void *>(&merit_dialog_toggle));memcpy(gate,merit_dialog_bytes,5);
            absolute_jump(gate+5,image+rva+5);original_merit_dialog=reinterpret_cast<MeritDialogToggle>(gate);
            ok=prepare_link(code.links[10],rva,0xE9,stub);
        }
        code.dialog_ready=ok;
        log_event("merit_dialog_hook_prepare",ok?1u:0u,rva?1u:0u,GA(GA_781DA0)?1u:0u);
        // Lock display: stub relay+1152 = mov r9,rbp (the row refresh keeps its row in RBP) + jump to merit_slot_states;
        // gate relay+1184 = the five prologue bytes + jump back.
        rva=GA(GA_6D9790);
        ok=rva && GA(GA_8759A0) && GA(GA_356810) && !ga::mcmp_at(image,rva,merit_slot_bytes,5);
        if(ok) {
            Obj stub=relay+1152,gate=relay+1184;
            stub[0]=0x49;stub[1]=0x89;stub[2]=0xE9;absolute_jump(stub+3,reinterpret_cast<void *>(&merit_slot_states));
            memcpy(gate,merit_slot_bytes,5);absolute_jump(gate+5,image+rva+5);
            original_merit_slot_states=reinterpret_cast<MeritSlotStates>(gate);
            ok=prepare_link(code.links[11],rva,0xE9,stub);
        }
        code.lock_ready=ok;
        log_event("merit_lock_hook_prepare",ok?1u:0u,rva?1u:0u);
        // Auto fill: stub relay+1216, gate relay+1248 (six prologue bytes, the sixth patched byte is a NOP).
        rva=GA(GA_6C40B0);
        ok=rva && GA(GA_781DA0) && !ga::mcmp_at(image,rva,merit_autofill_bytes,6);
        if(ok) {
            Obj stub=relay+1216,gate=relay+1248;
            absolute_jump(stub,reinterpret_cast<void *>(&merit_autofill));
            memcpy(gate,merit_autofill_bytes,6);absolute_jump(gate+6,image+rva+6);
            original_merit_autofill=reinterpret_cast<MeritAutoFill>(gate);
            ok=prepare_link(code.links[12],rva,0xE9,stub);
            if(ok)code.links[12].modified[rva-code.links[12].block+5]=0x90;
        }
        code.autofill_ready=ok;
        log_event("merit_autofill_hook_prepare",ok?1u:0u,rva?1u:0u);
    }
    if(!prepare_link(code.links[8],GA(GA_3F7E32),0xE8,relay+1024) ||
       !prepare_link(code.links[9],GA(GA_3FA5C4),0xE8,relay+1056))return false;
    code.links[8].modified[7]=0x90;code.links[8].modified[8]=0x90;return true;
}
static bool publish_merit(MeritCode &code) {
    // Links 8/9 and the save/load hosts also carry the warehouse sidecar, so
    // they stay published when the merit gameplay switch is off.
    for(unsigned i=0;i<10;++i)if((mod_features.merit || i>=8) && !publish_link(code.links[i]))return false;
    if(!ROUTE_VT(GA(GA_13E69A0),GA(GA_50B670),&merit_save_host,&next_merit_save_host) ||
       !ROUTE_VT(GA(GA_13E60F8),GA(GA_509580),&merit_load_host,&next_merit_load_host))return false;
    if(!mod_features.merit)return true;
    if(code.dialog_ready) {   // own failure: dialog block off, everything else stays
        code.dialog_ready=publish_link(code.links[10]);
        log_event("merit_dialog_hook_published",code.dialog_ready?1u:0u);
    }
    if(code.lock_ready) {code.lock_ready=publish_link(code.links[11]);log_event("merit_lock_hook_published",code.lock_ready?1u:0u);}
    if(code.autofill_ready) {
        code.autofill_ready=publish_link(code.links[12]);log_event("merit_autofill_hook_published",code.autofill_ready?1u:0u);
    }
    // 13A6BB0 is its own owner (merit_prisoner): it yields to R1 PrisonerReturn without taking the rest of merit along.
    return ROUTE_VT(GA(GA_13A6BB0),GA(GA_2390B0),&merit_person_read,&next_merit_person_read) &&
           ROUTE_VT(GA(GA_1413600),GA(GA_908820),&merit_profile,&next_merit_profile) &&
           ROUTE_VT(GA(GA_1413488),GA(GA_878810),&merit_profile_tick,&next_merit_profile_tick);
}
static void restore_merit(MeritCode &code) {
    merit_scroll_stop();
#ifndef LAYOUT_ROUTED   // routed: split_rollback cancels the manager registrations
    restore_pointer(GA(GA_13E69A0),GA(GA_50B670),reinterpret_cast<void *>(&merit_save_host));
    restore_pointer(GA(GA_13E60F8),GA(GA_509580),reinterpret_cast<void *>(&merit_load_host));
    restore_pointer(GA(GA_13A6BB0),GA(GA_2390B0),reinterpret_cast<void *>(&merit_person_read));
    restore_pointer(GA(GA_1413600),GA(GA_908820),reinterpret_cast<void *>(&merit_profile));
    restore_pointer(GA(GA_1413488),GA(GA_878810),reinterpret_cast<void *>(&merit_profile_tick));
#endif
    for(auto &link:code.links)restore_link(link);
}
