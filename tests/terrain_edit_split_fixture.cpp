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
    memset(image+0x457500,0xA7,17); // Unrelated map entry must never be checked or patched.
    memcpy(image+0x73C3C4,terrain_edit::highlight_select_bytes,sizeof(terrain_edit::highlight_select_bytes));
    memcpy(image+0x486359,terrain_edit::highlight_lock_bytes,sizeof(terrain_edit::highlight_lock_bytes));
    memcpy(image+0x486558,terrain_edit::highlight_unlock_bytes,sizeof(terrain_edit::highlight_unlock_bytes));
    memcpy(image+0x71E180,terrain_edit::hex_exit_bytes,sizeof(terrain_edit::hex_exit_bytes));
    memcpy(image+0x924FE0,terrain_edit::highlight_update_bytes,sizeof(terrain_edit::highlight_update_bytes));
    memcpy(image+0x73C385,terrain_edit::domestic_input_bytes,sizeof(terrain_edit::domestic_input_bytes));
    memcpy(image+0x7321CB,terrain_edit::hex_close_bytes,sizeof(terrain_edit::hex_close_bytes));
    memcpy(image+0x732241,terrain_edit::hex_update_bytes,sizeof(terrain_edit::hex_update_bytes));
    memcpy(image+0x7A92C0,terrain_edit::hex_hint_bytes,sizeof(terrain_edit::hex_hint_bytes));
    memcpy(image+0x6B8A8E,terrain_edit::hex_hint_call_bytes,sizeof(terrain_edit::hex_hint_call_bytes));
    memcpy(image+0x857320,terrain_edit::hex_hint_set_bytes,sizeof(terrain_edit::hex_hint_set_bytes));
    memcpy(image+0x7A7DB0,terrain_edit::domestic_click_bytes,sizeof(terrain_edit::domestic_click_bytes));
    memcpy(image+0x73C350,terrain_edit::domestic_state_bytes,sizeof(terrain_edit::domestic_state_bytes));
    memcpy(image+0x70E940,terrain_edit::domestic_area_bytes,sizeof(terrain_edit::domestic_area_bytes));
    memcpy(image+0x456FE0,terrain_edit::hex_complete_bytes,sizeof(terrain_edit::hex_complete_bytes));
    memcpy(image+0x7A8060,terrain_edit::hex_click_bytes,sizeof(terrain_edit::hex_click_bytes));
    memcpy(image+0x7248D6,terrain_edit::menu_call_bytes,sizeof(terrain_edit::menu_call_bytes));
    memcpy(image+0x7248DC,terrain_edit::hex_return_bytes,sizeof(terrain_edit::hex_return_bytes));
    memcpy(image+0x45767B,terrain_edit::hex_open_bytes,sizeof(terrain_edit::hex_open_bytes));
    memcpy(image+0x433159,terrain_edit::hex_destroy_bytes,sizeof(terrain_edit::hex_destroy_bytes));
    *reinterpret_cast<Obj *>(image+0x13D89C8)=image+0x456C30;
    *reinterpret_cast<Obj *>(image+0x13D89D0)=image+0x456C30;
    *reinterpret_cast<Obj *>(image+0x13D89D8)=image+0x456FE0;
    *reinterpret_cast<Obj *>(image+0x13D89E8)=image+0x65DFE0;
    *reinterpret_cast<Obj *>(image+0x14410B8)=image+0x724660;
    *reinterpret_cast<Obj *>(image+0x144DD28)=image+0x7A8060;
}
static void prepared() {split_install.prepared=true;}
static bool unowned_untouched() {
    for(unsigned i=0;i<17;++i)if(image[0x457500+i]!=0xA7)return false;
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
    const unsigned starts[]={0x7A8060,0x7248D6,0x7248DC,0x45767B,
        0x13D89C8,0x13D89D0,0x13D89D8,0x13D89E8,0x433159,0x7A7DB0,0x73C350,0x456FE0,0x70E940,0x73C385,0x7321CB,0x732241,0x7A92C0,0x6B8A8E,0x857320,0x73C3C4,0x486359,0x486558,0x71E180,0x924FE0,0x14410C0};
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
    require(pointer(image,0x144D350)==image+0x7A7DB0,"regional slot restored on rollback");
    // All newly checked sites must also be isolated by the real publication recheck.
    for(unsigned site:{0x144D350u,0x7A7DB0u,0x73C350u,0x456FE0u,0x70E940u,0x144DD60u,0x73C385u,0x7321CBu,0x732241u,0x7A92C0u,0x6B8A8Eu,0x857320u,0x14410C0u,0x73C3C4u,0x486359u,0x486558u,0x71E180u,0x924FE0u}) {
        reset();prepared();image[site]^=1;
        require(split_publish() && !mod_features.terrain_edit && mod_features.damage_preview,"publication recheck isolates mismatch");
        for(const auto &p:split_pointers)if(p.owner==kPointerTerrain)
            require(pointer(image,p.slot)==image+p.original+(p.slot==site?1:0),"recheck preserves foreign pointer and publishes no terrain slot");
        require(unowned_untouched(),"recheck leaves original map entry");
    }
    cleanup();VirtualFree(image,0,MEM_RELEASE);
    printf("{\"terrain_edit_split_checks\":%u,\"failed\":0}\n",checks);
}
