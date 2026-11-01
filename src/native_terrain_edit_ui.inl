// User/Claude 2026-10-29: regional domestic tiles choose officers or terrain; no development mode.
#include <stddef.h>
namespace terrain_edit {
struct MenuEntry {
    int group,child;
    const wchar_t *label;
    int enabled,style,option,reserved;
};
static_assert(sizeof(MenuEntry)==32 && offsetof(MenuEntry,label)==8 &&
    offsetof(MenuEntry,enabled)==0x10 && offsetof(MenuEntry,reserved)==0x1C,"native menu ABI");
struct PickerEntry {int group,child;const wchar_t *label;unsigned terrain;};
static constexpr PickerEntry picker_entries[]={
    {0,0,L"평지류",0},{1,0,L"습지",0},{2,0,L"숲",0},{3,0,L"산",0},
    {4,0,L"험지",0},{5,0,L"물",0},{6,0,L"급류",0},{7,0,L"닫기",0},
    {0,1,L"평지",1},{0,2,L"가도",2},{0,3,L"모래",3},
    {1,1,L"습지",4},{1,2,L"독늪",7},
    {2,1,L"숲",5},{2,2,L"밀림",6},
    {3,1,L"저산",8},{3,2,L"중산",9},{3,3,L"고산",10},
    {4,1,L"산길",11},{4,2,L"험산",19},{4,3,L"절벽",20},
    {5,1,L"여울",12},{5,2,L"강",13},{5,3,L"대하",14},
    {6,1,L"급류",21},{6,2,L"격류",24}
};
static constexpr bool picker_rows_valid() {
    unsigned top=0,children[8]={};
    for(const auto &e:picker_entries) {
        if(e.group<0 || e.group>=8 || e.child<0 || e.child>3)return false;
        if(!e.child) {if(e.group!=static_cast<int>(top++))return false;}
        else if(e.child!=static_cast<int>(++children[e.group]))return false;
    }
    return top<=8;
}
static_assert(sizeof(picker_entries)/sizeof(*picker_entries)==26 && picker_rows_valid(),"native picker row limits");
static constexpr int action_native=-1;
static thread_local wchar_t picker_labels[26][16];
static thread_local int expanded_actions[26],expanded_count;
static thread_local Obj menu_scope;
using MenuState=uint64_t(__fastcall *)(Obj);
using MenuBuild=uint64_t(__fastcall *)(Obj,int,int,uint64_t,MenuEntry *,int);
using MenuClick=uint64_t(__fastcall *)(Obj,void **);
static constexpr unsigned char menu_call_bytes[]={0xFF,0x90,0x88,0x01,0x00,0x00};
static constexpr unsigned char domestic_click_bytes[]={
    0x48,0x8B,0x12,0x48,0x8B,0x49,0x08,0xE9,0x94,0x45,0xF9,0xFF};
static constexpr unsigned char domestic_state_bytes[]={
    0x48,0x89,0x5C,0x24,0x18,0x56,0x48,0x83,0xEC,0x20,0x48,0x8B,0xF1,0x48,0x8B,0xDA,
    0x48,0x8B,0xCA,0xE8,0x98,0x31,0xBF,0xFF,0x85,0xC0,0x0F,0x84,0xDE,0x01,0x00,0x00,
    0x48,0x89,0x6C,0x24,0x30,0x48,0x8B,0xCB,0x48,0x89,0x7C,0x24,0x38,0xE8,0x1E,0xDB,0xAE,0xFF};
static constexpr unsigned char domestic_area_bytes[]={
    0x48,0x8B,0x81,0xA0,0x04,0x00,0x00,0x48,0x85,0xC0,0x74,0x26,0x48,0x83,0x3D,0x94,
    0x27,0x0C,0x02,0x00,0x74,0x1C,0x48,0x8B,0x0D,0x93,0x27,0x0C,0x02,0x8B,0x00,0x48,
    0x85,0xC9,0x74,0x0E,0x3B,0x05,0xB6,0x27,0x0C,0x02,0x73,0x06,0x48,0x8B,0x04,0xC1,
    0xEB,0x02,0x33,0xC0,0x48,0x85,0xC0,0x74,0x15,0x0F,0x1F,0x80,0x00,0x00,0x00,0x00,
    0x48,0x3B,0x10,0x74,0x0C,0x48,0x8B,0x40,0x08,0x48,0x85,0xC0,0x75,0xF2,0x33,0xC0,
    0xC3,0xB8,0x01,0x00,0x00,0x00,0xC3};
static constexpr unsigned char hex_complete_bytes[]={
    0x48,0x8B,0x41,0x08,0xC7,0x80,0x78,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0xC3};
static constexpr unsigned char hex_click_bytes[]={
    0x48,0x8B,0x12,0x48,0x8B,0x49,0x08,0xE9,0x04,0x58,0xFC,0xFF};
static constexpr unsigned char hex_return_bytes[]={0x48,0x8D,0x05,0x35,0x94,0xD2,0x00};
// 45767B..457707: exact native regional-center branch, independently checked offline.
static constexpr unsigned char hex_open_bytes[]={
    0xC7,0x87,0x78,0x04,0x00,0x00,0x01,0x00,0x00,0x00,0x48,0x89,0x5D,0xC7,
    0x0F,0x28,0x05,0xA0,0xB6,0xEE,0x00,0x0F,0x29,0x45,0xB7,0x44,0x8D,0x48,0x01,
    0x4C,0x8D,0x45,0xB7,0x48,0x8D,0x55,0x77,0x48,0x8B,0xCB,0xE8,0xB8,0x7E,0xBB,0xFF,
    0x85,0xC0,0x75,0x13,0xE8,0xDF,0x9B,0xBB,0xFF,0x48,0x8B,0x40,0x34,0x89,0x45,0x77,
    0x48,0xC1,0xE8,0x20,0x89,0x45,0x7B,0x48,0x8B,0x45,0x77,0x48,0x89,0x45,0xCF,
    0x33,0xDB,0x89,0x5D,0xD7,0x48,0x8D,0x45,0xDF,0x48,0x89,0x45,0x77,
    0x48,0x8D,0x05,0xED,0x12,0xF8,0x00,0x48,0x89,0x45,0xDF,0x48,0x89,0x7D,0xE7,
    0x48,0x8D,0x45,0xDF,0x48,0x89,0x45,0x17,0xE8,0xF0,0x99,0xBB,0xFF,0x48,0x8B,0xC8,
    0x4C,0x8D,0x4D,0xDF,0x4C,0x8D,0x45,0xC7,0x48,0x8D,0x15,0x16,0x03,0xF8,0x00,
    0xE8,0x19,0xB9,0xFD,0xFF};
static constexpr unsigned char hex_destroy_bytes[]={
    0x48,0x8B,0x4E,0x38,0x48,0x85,0xC9,0x74,0x10,0x48,0x8B,0x01,0x48,0x3B,0xCE,
    0x0F,0x95,0xC2,0xFF,0x50,0x20,0x4C,0x89,0x76,0x38};
// User/Claude 2026-10-29: click gate, native close request and null tooltip ABI.
static constexpr unsigned char domestic_input_bytes[]={
    0xE8,0x06,0x4F,0x8D,0xFF,0xBA,0x01,0x00,0x00,0x00,0x48,0x8B,0xC8,0x48,0x8B,0xF8,
    0xE8,0xC6,0x77,0xCB,0xFF,0x85,0xC0,0x74,0x26,0x48,0x8B,0xCF,0xE8,0xAA,0x7C,0xCB,
    0xFF,0x85,0xC0,0x75,0x1A,0x48,0x8B,0xD5,0x48,0x8B,0xCE};
static constexpr unsigned char hex_close_bytes[]={
    0xC7,0x87,0xA8,0x04,0x00,0x00,0x01,0x00,0x00,0x00};
static constexpr unsigned char hex_update_bytes[]={
    0xE8,0x7A,0xED,0x8D,0xFF,0x48,0x8B,0xC8,0xBA,0x05,0x00,0x00,0x00,0xE8,0x6D,0xC9,
    0x13,0x00,0x85,0xC0,0x75,0x08,0x39,0x87,0xA8,0x04,0x00,0x00,0x74,0x17,0xE8,0x7C,
    0xEE,0x8D,0xFF,0x48,0x8B,0xC8,0xE8,0xD4,0x02,0x8E,0xFF};
static constexpr unsigned char hex_hint_bytes[]={
    0x48,0x8B,0x12,0x48,0x8B,0x49,0x08,0xE9,0xE4,0xF5,0xF0,0xFF};
static constexpr unsigned char hex_hint_call_bytes[]={
    0x49,0x8B,0x8E,0xA0,0x04,0x00,0x00,0x48,0x8B,0xD6,0xE8,0x83,0xE8,0x19,0x00};
static constexpr unsigned char hex_hint_set_bytes[]={
    0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0xF9,0x48,0x8B,0xDA,
    0x48,0x8B,0x89,0x68,0x04,0x00,0x00,0x48,0x85,0xC9,0x74,0x22,0x48,0x85,0xD2,0x74,
    0x05};
// Matching decoded EXE 1fbe6be7, user/Claude 2026-10-29. Full CALL operands included.
static constexpr unsigned char highlight_select_bytes[]={
    0x48,0x8B,0xCB,0xE8,0x04,0x4C,0xAF,0xFF,0x48,0x8B,0xC8,0x48,0x8B,0xD8,0xE8,0x29,
    0x31,0xBF,0xFF,0x85,0xC0,0x74,0x2B,0xF6,0x43,0x5F,0x01,0x74,0x25,0xE8,0x2A,0xCB,
    0x8D,0xFF,0x48,0x39,0x98,0x10,0x01,0x00,0x00,0x74,0x3A,0xE8,0x1C,0xCB,0x8D,0xFF,
    0x83,0xB8,0x18,0x01,0x00,0x00,0x00,0x75,0x2C,0x48,0x89,0x98,0x10,0x01,0x00,0x00,
    0xEB,0x23,0xE8,0x05,0xCB,0x8D,0xFF,0x48,0x39,0xA8,0x10,0x01,0x00,0x00,0x74,0x15,
    0xE8,0xF7,0xCA,0x8D,0xFF,0x83,0xB8,0x18,0x01,0x00,0x00,0x00,0x75,0x07,0x48,0x89,
    0xA8,0x10,0x01,0x00,0x00,0x48};
static constexpr unsigned char highlight_lock_bytes[]={
    0xE8,0xB2,0x2B,0xB9,0xFF,0x83,0xB8,0x18,0x01,0x00,0x00,0x00,0x75,0x07,0x48,0x89,
    0xA8,0x10,0x01,0x00,0x00,0xE8,0x9D,0x2B,0xB9,0xFF,0xC7,0x80,0x18,0x01,0x00,0x00,
    0x01,0x00,0x00,0x00,0xE8,0x8E,0x2B,0xB9,0xFF,0x48,0x8B,0xC8,0xE8,0x56,0xEC,0x49,
    0x00};
static constexpr unsigned char highlight_unlock_bytes[]={
    0xE8,0xB3,0x29,0xB9,0xFF,0x89,0xB0,0x18,0x01,0x00,0x00,0xE8,0xA8,0x29,0xB9,0xFF,
    0x39,0xB0,0x18,0x01,0x00,0x00,0x75,0x07,0x48,0x89,0xB0,0x10,0x01,0x00,0x00,0xE8,
    0x94,0x29,0xB9,0xFF,0x48,0x8B,0xC8,0xE8,0x5C,0xEA,0x49,0x00};
static constexpr unsigned char hex_exit_bytes[]={
    0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x20,0x48,0x8B,0xD9,0x48,0x8B,0xFA};
static constexpr unsigned char highlight_update_bytes[]={
    0x48,0x8B,0xC4,0x55,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0x68,0x98,
    0x48,0x81,0xEC,0x40,0x01,0x00,0x00,0x48,0xC7,0x44,0x24,0x58,0xFE,0xFF,0xFF,0xFF};
static bool menu_guards_match() {
    return pointer(image,0x14410C0)==image+0x71E180 &&
        !memcmp(image+0x73C3C4,highlight_select_bytes,sizeof(highlight_select_bytes)) &&
        !memcmp(image+0x486359,highlight_lock_bytes,sizeof(highlight_lock_bytes)) &&
        !memcmp(image+0x486558,highlight_unlock_bytes,sizeof(highlight_unlock_bytes)) &&
        !memcmp(image+0x71E180,hex_exit_bytes,sizeof(hex_exit_bytes)) &&
        !memcmp(image+0x924FE0,highlight_update_bytes,sizeof(highlight_update_bytes)) &&
        pointer(image,0x144D350)==image+0x7A7DB0 &&
        pointer(image,0x144DD60)==image+0x7A92C0 &&
        !memcmp(image+0x73C385,domestic_input_bytes,sizeof(domestic_input_bytes)) &&
        !memcmp(image+0x7321CB,hex_close_bytes,sizeof(hex_close_bytes)) &&
        !memcmp(image+0x732241,hex_update_bytes,sizeof(hex_update_bytes)) &&
        !memcmp(image+0x7A92C0,hex_hint_bytes,sizeof(hex_hint_bytes)) &&
        !memcmp(image+0x6B8A8E,hex_hint_call_bytes,sizeof(hex_hint_call_bytes)) &&
        !memcmp(image+0x857320,hex_hint_set_bytes,sizeof(hex_hint_set_bytes)) &&
        !memcmp(image+0x7A7DB0,domestic_click_bytes,sizeof(domestic_click_bytes)) &&
        !memcmp(image+0x73C350,domestic_state_bytes,sizeof(domestic_state_bytes)) &&
        !memcmp(image+0x70E940,domestic_area_bytes,sizeof(domestic_area_bytes)) &&
        !memcmp(image+0x456FE0,hex_complete_bytes,sizeof(hex_complete_bytes)) &&
        pointer(image,0x13D89D0)==image+0x456C30 &&
        pointer(image,0x13D89D8)==image+0x456FE0 &&
        pointer(image,0x14410B8)==image+0x724660 &&
        pointer(image,0x144DD28)==image+0x7A8060 &&
        !memcmp(image+0x7A8060,hex_click_bytes,sizeof(hex_click_bytes)) &&
        !memcmp(image+0x7248D6,menu_call_bytes,sizeof(menu_call_bytes)) &&
        !memcmp(image+0x7248DC,hex_return_bytes,sizeof(hex_return_bytes)) &&
        !memcmp(image+0x45767B,hex_open_bytes,sizeof(hex_open_bytes)) &&
        !memcmp(image+0x433159,hex_destroy_bytes,sizeof(hex_destroy_bytes)) &&
        pointer(image,0x13D89C8)==image+0x456C30 &&
        pointer(image,0x13D89E8)==image+0x65DFE0 &&
        pointer(image,0x143C8C8)==image+0x6EB5C0;
}
static int menu_entries(MenuEntry *out,int *actions,unsigned terrain) {
    for(unsigned i=0;i<26;++i) {
        const auto &e=picker_entries[i];
        const wchar_t *label=e.label;
        if(e.terrain && e.terrain==terrain) {
            swprintf_s(picker_labels[i],L"● %s",label);label=picker_labels[i];
        }
        // User 2026-10-30: the tile's terrain must be visible without opening each group.
        if(!e.child)for(const auto &c:picker_entries)if(c.child && c.group==e.group && c.terrain==terrain) {
            swprintf_s(picker_labels[i],L"● %s (%s)",label,c.label);label=picker_labels[i];break;
        }
        out[i]={e.group,e.child,label,1,0,0,0};
        actions[i]=static_cast<int>(e.terrain); // Groups and Close carry no terrain action.
    }
    return 26;
}
// Existing native state computes the original four entries and positions first.
static uint64_t __fastcall hex_menu_state(Obj state) {
    Obj previous=menu_scope;
    if(active())check_world();
    menu_scope=state;clear_menu_context();
    uint64_t result;
    __try {result=fn<MenuState>(0x724660)(state);}
    __finally {menu_scope=previous;opening_tile=nullptr;}
    return result;
}
// Separate caller argument keeps the real gate testable without forged return addresses.
static uint64_t menu_build_from(Obj caller,Obj menu,int x,int y,uint64_t flag,MenuEntry *entries,int count) {
    if(active() && menu_scope && entries) {
        Obj target=pointer(menu_scope,0x470);
        if(caller==image+0x7248DC && count==4 && target && target==opening_tile) {
            MenuEntry extended[26];int actions[26];
            int extended_count=menu_entries(extended,actions,target[0x10]);
#ifndef NO_RUNTIME_LOG
            log_event("terrain_menu_build",static_cast<unsigned>(caller-image),count,1);
#endif
            // User crash 2026-10-29 (local44): 6EB5C0 can fire the hover/hint callback
            // while building under a resting cursor. Publish the context first so that
            // callback never reaches the native four-entry tooltip table.
            expanded_state=menu_scope;expanded_tile=target;expanded_menu=menu;
            expanded_count=extended_count;
            memcpy(expanded_actions,actions,sizeof(actions));
            uint64_t result=fn<MenuBuild>(0x6EB5C0)(menu,x,y,flag,extended,extended_count);
            if(!result)clear_menu_context();
            opening_tile=nullptr;
            return result;
        }
    }
#ifndef NO_RUNTIME_LOG
    if(menu_scope)log_event("terrain_menu_build",static_cast<unsigned>(caller-image),count,0);
#endif
    if(menu_scope && menu_scope==expanded_state)clear_menu_context();
    return fn<MenuBuild>(0x6EB5C0)(menu,x,y,flag,entries,count);
}
static uint64_t __fastcall menu_build(Obj menu,int x,int y,uint64_t flag,MenuEntry *entries,int count) { COST_SCOPE("terrain_edit::menu_build");
    return menu_build_from(static_cast<Obj>(_ReturnAddress()),menu,x,y,flag,entries,count);
}
static bool menu_matches(Obj functor,void **arg) {
    if(!functor || !arg || !*arg)return false;
    Obj state=pointer(functor,8),target=expanded_tile;
    return state && state==expanded_state && target && pointer(state,0x470)==target &&
        static_cast<Obj>(*arg)==expanded_menu;
}
static int menu_action(Obj functor,void **arg) {
    if(menu_matches(functor,arg)) {
        int index=*reinterpret_cast<int *>(static_cast<Obj>(*arg)+0x80);
        if(index>=0 && index<expanded_count)return expanded_actions[index];
    }
    return action_native;
}
static uint64_t __fastcall hex_menu_click(Obj functor,void **arg) { COST_SCOPE("terrain_edit::hex_menu_click");
#ifndef NO_RUNTIME_LOG
    if(menu_matches(functor,arg)) {
        int index=*reinterpret_cast<int *>(static_cast<Obj>(*arg)+0x80);
        log_event("terrain_menu_click",index,enabled && mod_features.convenience && mod_features.terrain_edit,1);
    }
#endif
    if(active()) {
        check_world();
        if(menu_matches(functor,arg)) {
            int action=menu_action(functor,arg);
            if(action>0)apply_tile(expanded_tile,static_cast<unsigned>(action));
            int index=*reinterpret_cast<int *>(static_cast<Obj>(*arg)+0x80);
            if(action>0 || index==7)*reinterpret_cast<int *>(pointer(functor,8)+0x4A8)=1;
            return 0; // Expanded hex callbacks, including groups, never reach native indices.
        }
    }
    return fn<MenuClick>(0x7A8060)(functor,arg);
}
static uint64_t __fastcall hex_menu_hint(Obj functor,void **arg) { COST_SCOPE("terrain_edit::hex_menu_hint");
    // User 2026-10-29 (local45 crash, odd tooltips): the hint event's [arg] is the
    // hovered column, not the built menu, so menu identity never matched and the
    // picker fell through to 6B88B0's four-entry message table. Match the state
    // and tile only, and clear through [state+4A0] exactly as 6B8A8E does.
    Obj state=functor?pointer(functor,8):nullptr;
    if(active() && state && arg) {
        bool picker=state==expanded_state && expanded_tile && pointer(state,0x470)==expanded_tile;
        int index=*arg?*reinterpret_cast<int *>(static_cast<Obj>(*arg)+0x80):0;
        // Safety net: the native table has four rows; never let it read past them.
        if(picker || index>=4) {
            if(Obj menu=pointer(state,0x4A0))fn<void(__fastcall *)(Obj,const wchar_t *)>(0x857320)(menu,nullptr);
            return 0;
        }
    }
    return fn<MenuClick>(0x7A92C0)(functor,arg);
}
struct HexParams {uint64_t tile,res18,zero;};
struct HexFunctor {uint64_t slots[8];};
static_assert(sizeof(HexParams)==0x18 && offsetof(HexParams,res18)==8 && offsetof(HexParams,zero)==0x10,
    "433020 parameters: tile, screen position, zero");
static_assert(sizeof(HexFunctor)==0x40 && offsetof(HexFunctor,slots[7])==0x38,"native small functor ABI");
// Native completion only clears [functor+8]+478. Never lend it a domestic state.
alignas(16) static unsigned char hex_completion[0x500]={};
// 73C3C4..73C428: selectable flagged object wins, otherwise use the tile's area.
static void lock_highlight(Obj tile) {
    Obj manager=fn<Obj(__fastcall *)()>(0x18F10)();
    if(*reinterpret_cast<unsigned *>(manager+0x118)!=0)return;
    Obj target=fn<Obj(__fastcall *)(Obj)>(0x230FD0)(tile);
    if(!fn<int(__fastcall *)(Obj)>(0x32F500)(target) || !(target[0x5F]&1))
        target=fn<Obj(__fastcall *)(Obj)>(0x229EA0)(tile);
    *reinterpret_cast<Obj *>(manager+0x110)=target;
    *reinterpret_cast<unsigned *>(manager+0x118)=1;
    highlight_locked=true;highlight_obj=target;highlight_hex=tile_hex(tile);
    fn<void(__fastcall *)(Obj)>(0x924FE0)(manager);
#ifndef NO_RUNTIME_LOG
    log_event("terrain_highlight",1,highlight_hex,0);
#endif
}
// Shared native exit destroys the menu and invokes completion before we release our lock.
static uint64_t __fastcall hex_menu_exit(Obj state,uint64_t rdx) { COST_SCOPE("terrain_edit::hex_menu_exit");
    uint64_t result=fn<uint64_t(__fastcall *)(Obj,uint64_t)>(0x71E180)(state,rdx);
    if(highlight_locked) {
        Obj manager=fn<Obj(__fastcall *)()>(0x18F10)();
        *reinterpret_cast<unsigned *>(manager+0x118)=0;
        if(pointer(manager,0x110)==highlight_obj)*reinterpret_cast<Obj *>(manager+0x110)=nullptr;
        fn<void(__fastcall *)(Obj)>(0x924FE0)(manager);
#ifndef NO_RUNTIME_LOG
        log_event("terrain_highlight",0,highlight_hex,0);
#endif
        reset_highlight_context();
    }
    return result;
}
static void open_hex_menu(Obj tile) {
    alignas(16) unsigned char a8[16];memcpy(a8,image+0x1342D30,sizeof(a8));
    uint64_t res18=0;
    if(fn<int(__fastcall *)(Obj,uint64_t *,void *,int)>(0x0F560)(tile,&res18,a8,1)==0) {
        Obj view=fn<Obj(__fastcall *)()>(0x11290)();memcpy(&res18,view+0x34,sizeof(res18));
    }
    HexParams params={reinterpret_cast<uint64_t>(tile),res18,0};
    HexFunctor functor={};functor.slots[0]=reinterpret_cast<uint64_t>(image+0x13D89C8);
    functor.slots[1]=reinterpret_cast<uint64_t>(hex_completion);functor.slots[7]=reinterpret_cast<uint64_t>(&functor);
    opening_tile=tile;
    // 4576F0..457702: RCX=110E0(), RDX=name, R8=&params, R9=&functor.
    // 433020 clones the small functor, then destroys its stack source with vt+20(f,0).
    lock_highlight(tile);
    fn<void(__fastcall *)(Obj,const char *,HexParams *,HexFunctor *)>(0x433020)(
        fn<Obj(__fastcall *)()>(0x110E0)(),"CStrategyAreaHexState",&params,&functor);
}
static uint64_t __fastcall domestic_click(Obj functor,void **arg) { COST_SCOPE("terrain_edit::domestic_click");
    Obj input=fn<Obj(__fastcall *)()>(0x11290)();
    if(fn<int(__fastcall *)(Obj,int)>(0x3F3B60)(input,1)==0 ||
        fn<int(__fastcall *)(Obj)>(0x3F4050)(input)!=0)
        return fn<MenuClick>(0x7A7DB0)(functor,arg);
    Obj state=functor?pointer(functor,8):nullptr;
    Obj tile=arg?static_cast<Obj>(*arg):nullptr;
    bool intervene=false;unsigned hex=HexCount;
    if(active()) {
        check_world();
        if(state && tile && valid(tile) && allowed(tile[0x10])) {
            hex=tile_hex(tile);
            if(hex<HexCount && !occupied(hex)) {
                Obj area=fn<Obj(__fastcall *)(Obj)>(0x229EA0)(tile);
                // No proven city-ID field. Native membership predicate (state, area):
                // research/decomp_cache/1fbe6be7466a6646/0070E940_c.txt:11-28.
                intervene=area && fn<int(__fastcall *)(Obj,Obj)>(0x70E940)(state,area)!=0 &&
                    fn<int(__fastcall *)(int)>(0x17A3E0)(0x3A)==0;
            }
        }
    }
#ifndef NO_RUNTIME_LOG
    if(hex==HexCount && tile)hex=tile_hex(tile);
    log_event("terrain_domestic_click",hex,
        tile && valid(tile)?tile[0x10]:0,intervene?1:0);
#endif
    if(intervene) {open_hex_menu(tile);return 0;}
    return fn<MenuClick>(0x7A7DB0)(functor,arg);
}
}
static bool terrain_edit_guards_match() {return terrain_edit::menu_guards_match();}
