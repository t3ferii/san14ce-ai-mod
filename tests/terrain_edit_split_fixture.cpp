// User/Claude 2026-10-29: six regional/hex slots including highlight exit; city/map untouched.
#define TACTICS_FIXTURE
#define SPLIT_BASE
#include "../src/native_adapter.cpp"
static void *fixture_function(unsigned) {return nullptr;}
static unsigned checks;
static void require(bool b,const char *name) {++checks;if(!b){fprintf(stderr,"FAIL %s\n",name);ExitProcess(1);}}
static void cleanup() {
    split_rollback();
    split_install={};
}
static void reset() {
    cleanup();memset(&mod_features,0,sizeof(mod_features));
    mod_features.convenience=mod_features.terrain_edit=mod_features.damage_preview=true;
    enabled=0;terrain_trees::permitted=true;
    memcpy(image+terrain_trees::UpdateRva,terrain_trees::update_bytes,16);
    memcpy(image+terrain_trees::SetterRva,terrain_trees::setter_bytes,16);
    for(const auto &p:split_pointers)if(p.owner==kPointerTerrain)*reinterpret_cast<Obj *>(image+p.slot)=image+p.original;
    *reinterpret_cast<Obj *>(image+0x143C4A0)=image+0x1111;
    *reinterpret_cast<Obj *>(image+0x14508B0)=image+0x2222;
    memset(image+0x7A86C0,0xA6,26);memset(image+0x730B93,0xA6,13);
    memset(image+0x459730,0xA7,17); // Unrelated map entry must never be checked or patched.
    memcpy(image+0x73E654,terrain_edit::highlight_select_bytes,sizeof(terrain_edit::highlight_select_bytes));
    memcpy(image+0x488959,terrain_edit::highlight_lock_bytes,sizeof(terrain_edit::highlight_lock_bytes));
    memcpy(image+0x488B58,terrain_edit::highlight_unlock_bytes,sizeof(terrain_edit::highlight_unlock_bytes));
    memcpy(image+0x720530,terrain_edit::hex_exit_bytes,sizeof(terrain_edit::hex_exit_bytes));
    memcpy(image+0x927490,terrain_edit::highlight_update_bytes,sizeof(terrain_edit::highlight_update_bytes));
    memcpy(image+0x73E615,terrain_edit::domestic_input_bytes,sizeof(terrain_edit::domestic_input_bytes));
    memcpy(image+0x73455B,terrain_edit::hex_close_bytes,sizeof(terrain_edit::hex_close_bytes));
    memcpy(image+0x7345D1,terrain_edit::hex_update_bytes,sizeof(terrain_edit::hex_update_bytes));
    memcpy(image+0x7AB260,terrain_edit::hex_hint_bytes,sizeof(terrain_edit::hex_hint_bytes));
    memcpy(image+0x6BB1EE,terrain_edit::hex_hint_call_bytes,sizeof(terrain_edit::hex_hint_call_bytes));
    memcpy(image+0x859610,terrain_edit::hex_hint_set_bytes,sizeof(terrain_edit::hex_hint_set_bytes));
    memcpy(image+0x7A9D40,terrain_edit::domestic_click_bytes,sizeof(terrain_edit::domestic_click_bytes));
    memcpy(image+0x73E5E0,terrain_edit::domestic_state_bytes,sizeof(terrain_edit::domestic_state_bytes));
    memcpy(image+0x710C50,terrain_edit::domestic_area_bytes,sizeof(terrain_edit::domestic_area_bytes));
    memcpy(image+0x4590A0,terrain_edit::hex_complete_bytes,sizeof(terrain_edit::hex_complete_bytes));
    memcpy(image+0x7A9F40,terrain_edit::hex_click_bytes,sizeof(terrain_edit::hex_click_bytes));
    memcpy(image+0x726D56,terrain_edit::menu_call_bytes,sizeof(terrain_edit::menu_call_bytes));
    memcpy(image+0x726D5C,terrain_edit::hex_return_bytes,sizeof(terrain_edit::hex_return_bytes));
    memcpy(image+0x4598AB,terrain_edit::hex_open_bytes,sizeof(terrain_edit::hex_open_bytes));
    memcpy(image+0x4354C9,terrain_edit::hex_destroy_bytes,sizeof(terrain_edit::hex_destroy_bytes));
    *reinterpret_cast<Obj *>(image+0x13DBA58)=image+0x458DC0;
    *reinterpret_cast<Obj *>(image+0x13DBA60)=image+0x458DC0;
    *reinterpret_cast<Obj *>(image+0x13DBA68)=image+0x4590A0;
    *reinterpret_cast<Obj *>(image+0x13DBA78)=image+0x90DE00;
    *reinterpret_cast<Obj *>(image+0x1444140)=image+0x726AE0;
    *reinterpret_cast<Obj *>(image+0x1450DB0)=image+0x7A9F40;
}
static void prepared() {split_install.prepared=true;}
static bool unowned_untouched() {
    for(unsigned i=0;i<17;++i)if(image[0x459730+i]!=0xA7)return false;
    return pointer(image,0x143C4A0)==image+0x1111 && pointer(image,0x14508B0)==image+0x2222;
}
int main() {
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x2800000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));require(image!=nullptr,"image");
    reset();require(terrain_edit_guards_match(),"matching slots and fingerprints");
    split_feature_guards();require(mod_features.terrain_edit && mod_features.damage_preview,"matching split guard preserves both features");
    unsigned slots=0;
    for(const auto &p:split_pointers)if(p.owner==kPointerTerrain) {
        ++slots;
        reset();require(split_pointer_enabled(p),"terrain slot enabled independently");
        mod_features.terrain_edit=false;require(!split_pointer_enabled(p),"off publishes no terrain slot");
        reset();mod_features.trace_original=true;require(!split_pointer_enabled(p),"trace original skips terrain");
        reset();mod_features.convenience=false;require(!split_pointer_enabled(p),"group off skips terrain");
        reset();*reinterpret_cast<Obj *>(image+p.slot)=image+0x1234;split_feature_guards();
        if(p.slot==terrain_trees::Slot)require(mod_features.terrain_edit && !terrain_trees::permitted && mod_features.damage_preview,"foreign placement slot disables trees only (user 2026-10-29)");
        else require(!mod_features.terrain_edit && mod_features.damage_preview,"foreign slot disables only terrain");
        require(pointer(image,p.slot)==image+0x1234,"foreign pointer preserved");
    }
    require(slots==7,"user 2026-10-29: six existing terrain slots plus placement update");
    // User/Claude 2026-10-29: tree start/slot fingerprints isolate placement,
    // including the actual publisher's second guard pass.
    for(unsigned site:{terrain_trees::Slot,terrain_trees::UpdateRva,terrain_trees::SetterRva}) {
        unsigned length=site==terrain_trees::Slot?8:16;
        for(unsigned b=0;b<length;++b) {
            reset();prepared();image[site+b]^=0x80;
            require(split_publish() && !terrain_trees::permitted && mod_features.terrain_edit && mod_features.damage_preview,"tree fingerprint mismatch isolates trees at publication");
            for(const auto &p:split_pointers)if(p.owner==kPointerTerrain) {
                if(p.slot==terrain_trees::Slot)require(pointer(image,p.slot)!=reinterpret_cast<Obj>(p.replacement),"mismatched tree hook never published");
                else require(pointer(image,p.slot)==reinterpret_cast<Obj>(p.replacement),"other terrain hooks still published");
            }
            require(unowned_untouched(),"tree rejection preserves map/city slots");
        }
    }
    const unsigned starts[]={0x7A9F40,0x726D56,0x726D5C,0x4598AB,
        0x13DBA58,0x13DBA60,0x13DBA68,0x13DBA78,0x4354C9,0x7A9D40,0x73E5E0,0x4590A0,0x710C50,0x73E615,0x73455B,0x7345D1,0x7AB260,0x6BB1EE,0x859610,0x73E654,0x488959,0x488B58,0x720530,0x927490,0x1444148};
    const unsigned sizes[]={12,6,7,140,8,8,8,8,25,12,50,15,87,43,10,43,12,15,33,102,49,44,16,32,8};
    for(unsigned i=0;i<sizeof(starts)/sizeof(*starts);++i)for(unsigned b=0;b<sizes[i];++b) {
        reset();image[starts[i]+b]^=0x80;split_feature_guards();
        require(!mod_features.terrain_edit && mod_features.damage_preview,"every fingerprint byte mismatch isolates terrain");
        require(unowned_untouched(),"guard rejection preserves map and city slots");
    }
    reset();prepared();require(split_publish(),"actual split publisher accepts terrain slots only");
    require(unowned_untouched(),"actual publication never patches map entry or either city menu slot");
    for(const auto &p:split_pointers)if(p.owner==kPointerTerrain)require(pointer(image,p.slot)==reinterpret_cast<Obj>(p.replacement),"actual pointer replacement");
    split_rollback();
    for(const auto &p:split_pointers)if(p.owner==kPointerTerrain)require(pointer(image,p.slot)==image+p.original,"actual rollback restores slot");
    require(unowned_untouched(),"rollback preserves map and city slots");
    require(pointer(image,0x14503D8)==image+0x7A9D40,"regional slot restored on rollback");
    // All newly checked sites must also be isolated by the real publication recheck.
    for(unsigned site:{0x14503D8u,0x7A9D40u,0x73E5E0u,0x4590A0u,0x710C50u,0x1450DE8u,0x73E615u,0x73455Bu,0x7345D1u,0x7AB260u,0x6BB1EEu,0x859610u,0x1444148u,0x73E654u,0x488959u,0x488B58u,0x720530u,0x927490u}) {
        reset();prepared();image[site]^=1;
        require(split_publish() && !mod_features.terrain_edit && mod_features.damage_preview,"publication recheck isolates mismatch");
        for(const auto &p:split_pointers)if(p.owner==kPointerTerrain)
            require(pointer(image,p.slot)==image+p.original+(p.slot==site?1:0),"recheck preserves foreign pointer and publishes no terrain slot");
        require(unowned_untouched(),"recheck leaves original map entry");
    }
    cleanup();VirtualFree(image,0,MEM_RELEASE);
    printf("{\"terrain_edit_split_checks\":%u,\"failed\":0}\n",checks);
}
