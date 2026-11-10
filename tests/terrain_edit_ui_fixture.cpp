// User/Claude 2026-10-29: standalone fixture instrumentation declaration.
#ifndef COST_SCOPE
#define COST_SCOPE(name) do {} while(0)
#endif
// User/Claude 2026-10-29: regional domestic dispatch depends on terrain, occupancy and native area membership.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/terrain_edit_policy.h"
using Obj=unsigned char *;
static unsigned checks,reads,build_calls,hex_click_calls,domestic_calls,open_calls,hex_logs,hint_calls,hint_sets,input_checks,suppress_checks;
static void require(bool b,const char *label) {++checks;if(!b){fprintf(stderr,"FAIL %s\n",label);ExitProcess(1);}}
static bool enabled=true;
static struct {bool convenience=true,terrain_edit=true;} mod_features;
static unsigned char root[2],tile_data[0x60],other_tile[0x60],unit_data[0x60],building_data[0x60],area_data[0x40];
static unsigned char hex_state[0x4C0],other_state[0x4C0],domestic_state[0x4C0],ui_functor[16],domestic_functor[16],popup[0x88],other_popup[0x88],view_data[0x50];
static unsigned char highlight_manager[0x120],highlight_object[0x60];
static Obj highlight_candidate=highlight_object,exit_replacement;
static int highlight_selectable=1;
static unsigned highlight_gets,highlight_updates,highlight_exits,highlight_logs,last_highlight_value,last_highlight_hex;
static bool highlight_trace,highlight_manager_valid=true,exit_changes_world;
static unsigned highlight_order[32],highlight_steps;
static void highlight_step(unsigned step) {if(highlight_trace){require(highlight_steps<32,"highlight order capacity");highlight_order[highlight_steps++]=step;}}
static Obj image,current_world=root;
static Obj world() {return current_world;}
static Obj pointer(Obj p,unsigned off) {return *reinterpret_cast<Obj *>(p+off);}
static void put(Obj p,unsigned off,Obj v) {memcpy(p+off,&v,sizeof(v));}
static unsigned short word(Obj p,unsigned off) {return *reinterpret_cast<unsigned short *>(p+off);}
[[maybe_unused]] static unsigned dword(Obj p,unsigned off) {return *reinterpret_cast<unsigned *>(p+off);}
static void set_word(Obj p,unsigned off,unsigned v) {auto w=static_cast<unsigned short>(v);memcpy(p+off,&w,2);}
static bool occupied_fixture,building_fixture,invalid_tile,missing_area,blocked;
static int membership_result=1,input_pressed=1,input_suppressed;
static unsigned membership_calls;
static bool valid(Obj obj) {return ((obj==tile_data || obj==other_tile) && !invalid_tile) || (occupied_fixture && obj==unit_data) || (building_fixture && obj==building_data);}
static Obj table(unsigned off,unsigned id,unsigned) {
    ++reads;
    if(off==0xE270 && id==48399)return tile_data;
    if(off==0xE270 && id==17)return other_tile;
    if(off==0x7E838 && id==500 && occupied_fixture)return unit_data;
    if(off==0x6DA98 && id==3000 && building_fixture)return building_data;
    return nullptr;
}
static unsigned last_hex,last_hex_terrain,last_open;
[[maybe_unused]] static void fixture_log(const char *kind,unsigned a=0,unsigned b=0,unsigned c=0) {
    if(!strcmp(kind,"terrain_domestic_click")){++hex_logs;last_hex=a;last_hex_terrain=b;last_open=c;}
    if(!strcmp(kind,"terrain_highlight")){++highlight_logs;last_highlight_value=a;last_highlight_hex=b;require(c==0,"highlight diagnostic final argument");}
}
#define log_event(...) fixture_log(__VA_ARGS__)
static void *fixture_function(unsigned);
template<class T> static T fn(unsigned r) {return reinterpret_cast<T>(fixture_function(r));}
static unsigned cost_cache_clears=0;
static void terrain_edit_fixture_cost_cache_clear() {++cost_cache_clears;}
#define TERRAIN_EDIT_FIXTURE_COST_CACHE 1
#include "../src/native_terrain_edit.inl"
#define COST_CRUMB(name,value) do {} while(0)
#include "../src/native_terrain_edit_ui.inl"
using namespace terrain_edit;
static constexpr unsigned expected_terrains[]={1,2,3,4,7,5,6,8,9,10,11,19,20,12,13,14,21,24};
static const wchar_t *const expected_names[]={L"평지",L"가도",L"모래",L"습지",L"독늪",L"숲",L"밀림",L"저산",L"중산",L"고산",L"산길",L"험산",L"절벽",L"여울",L"강",L"대하",L"급류",L"격류"};
static MenuEntry original[33],copied[33];
static MenuEntry *passed;
static HexFunctor retained_functor;
static int passed_count,passed_x,passed_y,native_count=4;
static uint64_t passed_flag,build_result=77,position_value=0xFEDCBA9876543210ull,fallback_value=0x8877665544332211ull;
static Obj passed_menu,passed_functor,passed_domestic,passed_tile;
static void **passed_arg;
static bool in_scope,raise_state,position_success=true,immediate_build;
static Obj native_caller;
static uint64_t __fastcall original_build(Obj menu,int x,int y,uint64_t flag,MenuEntry *entries,int count) {
    ++build_calls;passed=entries;passed_count=count;passed_x=x;passed_y=y;passed_flag=flag;passed_menu=menu;
    if(entries)memcpy(copied,entries,static_cast<size_t>(count)*sizeof(MenuEntry));return build_result;
}
static uint64_t __fastcall original_hex_click(Obj f,void **a) {++hex_click_calls;passed_functor=f;passed_arg=a;return 92;}
static uint64_t __fastcall original_hint(Obj f,void **a) {++hint_calls;passed_functor=f;passed_arg=a;return 94;}
static void __fastcall original_hint_set(Obj menu,const wchar_t *text) {
    ++hint_sets;require((menu==popup || menu==other_popup) && !text,"857320 gets the emitting menu and null text");
}
static int __fastcall original_input(Obj input,int button) {
    ++input_checks;require(input==view_data && button==1,"click predicate input object and button 1");return input_pressed;
}
static int __fastcall original_suppressed(Obj input) {
    ++suppress_checks;require(input==view_data,"suppression predicate uses same input");return input_suppressed;
}
static uint64_t __fastcall original_state(Obj s) {
    put(s,0x4A0,popup);*reinterpret_cast<int *>(s+0x4A8)=0;
    in_scope=menu_scope==s;
    if(raise_state)RaiseException(0xE0123456,0,0,nullptr);
    return menu_build_from(native_caller,popup,-7,18,0x123456789ABCDEF0ull,original,native_count);
}
static uint64_t __fastcall original_domestic(Obj f,void **a) {
    ++domestic_calls;passed_domestic=f;passed_tile=a?static_cast<Obj>(*a):nullptr;passed_arg=a;return 93;
}
static uint64_t click_tile(Obj tile) {
    Obj argument=tile;auto arg=reinterpret_cast<void **>(&argument);
    unsigned before=domestic_calls;auto result=domestic_click(domestic_functor,arg);
    if(domestic_calls!=before)require(result==93 && passed_domestic==domestic_functor && passed_arg==arg && passed_tile==tile,
        "native regional callback preserves functor, argument address, tile and result");
    else require(result==0,"intercepted regional callback returns without native call");
    return result;
}
static Obj __fastcall original_area(Obj t) {highlight_step(3);require(t==tile_data || t==other_tile,"area gets exact registered tile");return missing_area?nullptr:area_data;}
static int __fastcall original_membership(Obj state,Obj area) {
    ++membership_calls;require((state==domestic_state || state==other_state) && area==area_data,"70E940 receives exact state and area");
    return state==domestic_state?membership_result:0;
}
static int __fastcall original_blocked(int key) {require(key==0x3A,"native UI gate key");return blocked?1:0;}
static int __fastcall original_position(Obj t,uint64_t *out,void *a8,int flag) {
    require(t==tile_data || t==other_tile,"position gets clicked tile");
    require(out && flag==1 && !memcmp(a8,image+0x1345D30,16),"F560 output, copied 16 bytes and fourth argument");
    require(dword(domestic_state,0x478)==0xA5A5A5A5,"domestic state+478 never changed");
    *out=position_value;return position_success?1:0;
}
static Obj __fastcall original_view() {return view_data;}
static Obj __fastcall original_manager() {return root;}
static Obj __fastcall original_highlight_manager() {++highlight_gets;require(highlight_manager_valid,"no stale manager access");return highlight_manager;}
static Obj __fastcall original_highlight_object(Obj t) {require(t==tile_data || t==other_tile,"highlight exact tile");highlight_step(1);return highlight_candidate;}
static int __fastcall original_highlight_predicate(Obj o) {require(o==highlight_candidate,"highlight native object predicate");highlight_step(2);return highlight_selectable;}
static void __fastcall original_highlight_update(Obj manager) {require(manager==highlight_manager,"highlight updater manager ABI");++highlight_updates;highlight_step(4);}
static uint64_t __fastcall original_exit(Obj state,uint64_t rdx) {
    require(state==hex_state || state==other_state,"exit receives original state");
    require(rdx==0xFEDCBA9876543210ull,"exit preserves all 64 bits of RDX");
    ++highlight_exits;highlight_step(6);
    if(highlight_trace && highlight_locked)require(dword(highlight_manager,0x118)==1,"native exit runs before our unlock");
    if(exit_replacement)put(highlight_manager,0x110,exit_replacement);
    if(exit_changes_world)before_world_change();
    return 0x8877665544332211ull;
}
static void __fastcall original_open(Obj manager,const char *name,HexParams *params,HexFunctor *functor) {
    ++open_calls;highlight_step(5);
    require(manager==root && !strcmp(name,"CStrategyAreaHexState"),"433020 manager and class name");
    Obj t=reinterpret_cast<Obj>(params->tile);
    require(t==opening_tile && (t==tile_data || t==other_tile),"TLS opening tile equals params[0]");
    require(params->res18==(position_success?position_value:fallback_value) && params->zero==0,"full 64-bit position and zero params");
    require(functor->slots[0]==reinterpret_cast<uint64_t>(image+0x13DBA58) &&
        functor->slots[1]==reinterpret_cast<uint64_t>(hex_completion) &&
        functor->slots[7]==reinterpret_cast<uint64_t>(functor),"functor vtable, DLL static dummy and self at +38");
    for(unsigned i=2;i<7;++i)require(!functor->slots[i],"functor unused slots zero");
    if(immediate_build) {put(hex_state,0x470,t);native_caller=image+0x726D5C;native_count=4;hex_menu_state(hex_state);}
    require((functor->slots[1]&15)==0,"completion dummy aligned to 16");
    retained_functor=*functor; // Native clone outlives this stack source.
    // Native consumes the source functor and destroys with its delete flag false.
    functor->slots[7]=0;
}
static void *fixture_function(unsigned r) {
    switch(r) {
    case 0x18E70:return reinterpret_cast<void *>(&original_highlight_manager);
    case 0x232A30:return reinterpret_cast<void *>(&original_highlight_object);
    case 0x331780:return reinterpret_cast<void *>(&original_highlight_predicate);
    case 0x927490:return reinterpret_cast<void *>(&original_highlight_update);
    case 0x720530:return reinterpret_cast<void *>(&original_exit);
    case 0x726AE0:return reinterpret_cast<void *>(&original_state);
    case 0x6ED8A0:return reinterpret_cast<void *>(&original_build);
    case 0x7A9D40:return reinterpret_cast<void *>(&original_domestic);
    case 0x7A9F40:return reinterpret_cast<void *>(&original_hex_click);
    case 0x7AB260:return reinterpret_cast<void *>(&original_hint);
    case 0x859610:return reinterpret_cast<void *>(&original_hint_set);
    case 0x3F6080:return reinterpret_cast<void *>(&original_input);
    case 0x3F6570:return reinterpret_cast<void *>(&original_suppressed);
    case 0x710C50:return reinterpret_cast<void *>(&original_membership);
    case 0x22B8E0:return reinterpret_cast<void *>(&original_area);
    case 0x17B390:return reinterpret_cast<void *>(&original_blocked);
    case 0xF4C0:return reinterpret_cast<void *>(&original_position);
    case 0x111F0:return reinterpret_cast<void *>(&original_view);
    case 0x11040:return reinterpret_cast<void *>(&original_manager);
    case 0x435390:return reinterpret_cast<void *>(&original_open);
    default:return nullptr;
    }
}
static void select(int index) {
    Obj p=popup;*reinterpret_cast<int *>(popup+0x80)=index;
    require(hex_menu_click(ui_functor,reinterpret_cast<void **>(&p))==0,"expanded picker callback consumed");
}
static void open_picker(Obj tile=tile_data) {
    native_caller=image+0x726D5C;native_count=4;opening_tile=tile;
    put(hex_state,0x470,tile);put(ui_functor,8,hex_state);
    require(hex_menu_state(hex_state)==77 && passed_count==26,"recorded tile replaces four native entries");
}
static void check_rows() {
    unsigned top=0,children[8]={};bool used[8][4]={};
    for(int i=0;i<passed_count;++i) {
        auto &e=copied[i];
        require(e.group>=0 && e.group<8 && e.child>=0 && e.child<=3,"every native row within bounds");
        require(!used[e.group][e.child],"no duplicate group/child row");used[e.group][e.child]=true;
        if(!e.child)++top;else ++children[e.group];
    }
    require(top==8,"exactly eight top rows");
    for(unsigned n:children)require(n<=3,"at most three child rows");
}
// User/Claude 2026-10-29: retain yellow native highlight until every native exit route.
static void check_highlight() {
    changes.clear();current_world=root;check_world();
    highlight_trace=true;position_success=true;immediate_build=false;
    auto reset_highlight=[]() {
        reset_highlight_context();memset(highlight_manager,0,sizeof(highlight_manager));
        highlight_candidate=highlight_object;highlight_selectable=1;highlight_object[0x5F]=1;
        exit_replacement=nullptr;exit_changes_world=false;highlight_steps=0;
    };
    auto exit_menu=[](Obj state=hex_state) {
        require(hex_menu_exit(state,0xFEDCBA9876543210ull)==0x8877665544332211ull,"exit retains native RAX result");
    };
    for(unsigned selection=0;selection<5;++selection) {
        reset_highlight();
        highlight_object[0x5F]=selection==1?0:selection==2?2:3;
        highlight_selectable=selection==3?0:-2;
        if(selection==4){highlight_candidate=nullptr;highlight_selectable=0;}
        Obj expected=selection==0?highlight_object:area_data;
        unsigned updates=highlight_updates,logs=highlight_logs;
        open_hex_menu(tile_data);
        require(highlight_locked && highlight_obj==expected && highlight_hex==48399,"lock records exact native object/area and hex");
        require(pointer(highlight_manager,0x110)==expected && dword(highlight_manager,0x118)==1,"lock zero writes target then dword one");
        require(highlight_updates==updates+1 && highlight_order[0]==1 && highlight_order[1]==2 &&
            highlight_order[highlight_steps-2]==4 && highlight_order[highlight_steps-1]==5,"object predicate and updater precede 433020");
#ifndef NO_RUNTIME_LOG
        require(highlight_logs==logs+1 && last_highlight_value==1 && last_highlight_hex==48399,"lock diagnostic exact hex");
#else
        require(highlight_logs==logs,"release has no lock diagnostics");
#endif
        unsigned step=highlight_steps;exit_menu();
        require(highlight_order[step]==6 && highlight_order[step+1]==4,"native exit precedes unlock updater");
        require(!highlight_locked && !highlight_obj && highlight_hex==HexCount && !pointer(highlight_manager,0x110) && !dword(highlight_manager,0x118),"owned highlight fully cleared");
        require(highlight_updates==updates+2,"one update each for lock and unlock");
#ifndef NO_RUNTIME_LOG
        require(last_highlight_value==0 && last_highlight_hex==48399,"unlock diagnostic saved hex");
#endif
    }
    // An existing native owner, including non-one lock values, must be untouched.
    const unsigned foreign_locks[]={1,7};
    for(unsigned lock:foreign_locks) {
        reset_highlight();put(highlight_manager,0x110,area_data);*reinterpret_cast<unsigned *>(highlight_manager+0x118)=lock;
        unsigned updates=highlight_updates;open_hex_menu(tile_data);
        require(!highlight_locked && !highlight_obj && dword(highlight_manager,0x118)==lock && pointer(highlight_manager,0x110)==area_data,"foreign lock unchanged");
        require(highlight_steps==1 && highlight_order[0]==5 && highlight_updates==updates,"foreign lock skips target getters and updater");
        unsigned gets=highlight_gets;exit_menu(other_state);
        require(highlight_gets==gets && highlight_updates==updates && highlight_steps==2 && highlight_order[1]==6,"unowned shared exit calls native only");
    }
    reset_highlight();open_hex_menu(other_tile);Obj first=highlight_obj;
    open_hex_menu(tile_data);require(highlight_locked && highlight_obj==first && highlight_hex==17,"reopen while locked preserves our identity");
    exit_replacement=area_data;exit_menu();
    require(pointer(highlight_manager,0x110)==area_data && dword(highlight_manager,0x118)==0 && !highlight_locked,"native exit replacement target retained");
    // Selection and Close request the native exit; right cancel reaches the same shared exit.
    for(unsigned route=0;route<3;++route) {
        reset_highlight();immediate_build=true;tile_data[0x10]=1;open_hex_menu(tile_data);
        if(route<2)select(route==0?13:7);
        else *reinterpret_cast<unsigned *>(hex_state+0x4A8)=1;
        require(highlight_locked && dword(highlight_manager,0x118)==1 && dword(hex_state,0x4A8)==1,"selection/close/cancel keeps lock through close request");
        exit_menu();require(!highlight_locked && !dword(highlight_manager,0x118),"all three exit routes release owned lock");
    }
    immediate_build=false;
    reset_highlight();open_hex_menu(tile_data);mod_features.terrain_edit=false;active();
    require(highlight_locked,"disabled feature retains ownership until exit");exit_menu();mod_features.terrain_edit=true;
    // Native completion may cross the world boundary; never touch a stale manager after it.
    reset_highlight();open_hex_menu(tile_data);exit_changes_world=true;
    unsigned updates=highlight_updates;exit_menu();
    require(!highlight_locked && highlight_updates==updates && dword(highlight_manager,0x118)==1,"world boundary during native completion discards ownership");
    for(unsigned boundary=0;boundary<2;++boundary) {
        reset_highlight();open_hex_menu(tile_data);
        unsigned gets=highlight_gets;updates=highlight_updates;unsigned logs=highlight_logs;
        highlight_manager_valid=false;
        if(boundary==0)before_world_change();else {current_world=root+1;check_world();}
        require(!highlight_locked && !highlight_obj && highlight_hex==HexCount,"world boundary resets all highlight records");
        require(dword(highlight_manager,0x118)==1 && pointer(highlight_manager,0x110)==highlight_object,"world boundary does not write manager");
        exit_menu();require(highlight_gets==gets && highlight_updates==updates && highlight_logs==logs,"post-boundary exit avoids stale getters writes updates and logs");
        highlight_manager_valid=true;current_world=root;check_world();
    }
    reset_highlight();highlight_trace=false;
#ifdef NO_RUNTIME_LOG
    require(!highlight_logs,"release strips all highlight diagnostics");
#endif
}
int main() {
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x1500000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));require(image!=nullptr,"image");
    put(domestic_functor,8,domestic_state);put(ui_functor,8,hex_state);put(hex_state,0x470,tile_data);
    *reinterpret_cast<unsigned *>(domestic_state+0x478)=0xA5A5A5A5;
    *reinterpret_cast<int *>(domestic_state+0x4B8)=0x54454431;   // user 2026-11-08: only the advisor-opened city state enables terrain edit
    for(unsigned char value:hex_completion)require(value==0,"DLL static completion buffer initially zero");
    memcpy(image+0x4590A0,hex_complete_bytes,sizeof(hex_complete_bytes));
    DWORD old;require(VirtualProtect(image+0x4590A0,sizeof(hex_complete_bytes),PAGE_EXECUTE_READ,&old)!=0,"fixture completion code protection");
    require(FlushInstructionCache(GetCurrentProcess(),image+0x4590A0,sizeof(hex_complete_bytes))!=0,"fixture completion instruction cache");
    memcpy(view_data+0x34,&fallback_value,8);for(unsigned i=0;i<16;++i)image[0x1345D30+i]=static_cast<unsigned char>(i*13);
    for(int i=0;i<4;++i)original[i]={i,0,L"native",i&1,7,8,9};
    check_world();tile_data[0x10]=1;other_tile[0x10]=2;
    native_caller=image+0x726D5C;native_count=4;
    require(hex_menu_state(hex_state)==77 && passed==original && passed_count==4,"unrecorded native hex menu unchanged");
    open_picker();require(in_scope && !menu_scope && !opening_tile,"scope restored and pending tile consumed");
    require(passed_menu==popup && passed_x==-7 && passed_y==18 && passed_flag==0x123456789ABCDEF0ull,"all six builder arguments retained");check_rows();
    require(!wcscmp(copied[7].label,L"닫기"),"close label");
    for(int i=0;i<26;++i) {
        auto &entry=copied[i];require(entry.enabled==1 && !entry.style && !entry.option && !entry.reserved,"picker fields");
        require(entry.group==picker_entries[i].group && entry.child==picker_entries[i].child,"exact picker group and child order");
    }
    for(unsigned i=0;i<18;++i) {
        tile_data[0x10]=static_cast<unsigned char>(expected_terrains[i]);open_picker();unsigned marked=0;
        for(unsigned j=0;j<18;++j) {
            wchar_t label[32];swprintf_s(label,j==i?L"● %s":L"%s",expected_names[j]);
            require(!wcscmp(copied[8+j].label,label),"only recorded tile's current terrain marked");
            if(copied[8+j].label[0]==L'●')++marked;
        }
        require(marked==1,"exactly one marker");
        for(int g=0;g<8;++g) {   // User 2026-10-30: the group row names the current terrain too.
            wchar_t label[32];
            if(g==picker_entries[8+i].group)swprintf_s(label,L"● %s (%s)",picker_entries[g].label,expected_names[i]);
            else swprintf_s(label,L"%s",picker_entries[g].label);
            require(!wcscmp(copied[g].label,label),"only the current terrain's group row is marked with its name");
        }
        tile_data[0x10]=1;other_tile[0x10]=2;changes.clear();open_picker();unsigned before=hex_click_calls;
        select(static_cast<int>(8+i));
        require(tile_data[0x10]==expected_terrains[i] && other_tile[0x10]==2 && hex_click_calls==before,"each leaf applies only recorded tile");
    }
    tile_data[0x10]=1;other_tile[0x10]=1;changes.clear();open_picker(other_tile);select(13);
    require(other_tile[0x10]==5 && tile_data[0x10]==1,"second registered tile gets its own edit");
    Obj p=popup;auto arg=reinterpret_cast<void **>(&p);
    for(int i=-1;i<=27;++i) {
        tile_data[0x10]=1;changes.clear();open_picker();*reinterpret_cast<int *>(popup+0x80)=i;
        unsigned before=hex_click_calls;require(hex_menu_click(ui_functor,arg)==0 && hex_click_calls==before,"all expanded callbacks consumed");
        require(tile_data[0x10]==(i>=8 && i<26?expected_terrains[i-8]:1),"groups, close and invalid indices do not edit");
        require(dword(hex_state,0x4A8)==static_cast<unsigned>(i==7 || (i>=8 && i<26)),
            "only Close or leaf action requests native state close; groups and invalid indices stay open");
        before=hint_calls;unsigned sets=hint_sets;
        require(hex_menu_hint(ui_functor,arg)==0 && hint_calls==before && hint_sets==sets+1,
            "expanded hover never calls native four-message table, including invalid indices");
    }
    // Occupancy is checked again on apply, even when a unit/building arrives after opening.
    for(unsigned kind=0;kind<2;++kind) {
        tile_data[0x10]=1;changes.clear();open_picker();occupied_fixture=kind==0;building_fixture=kind==1;
        set_word(unit_data,0x2A,48399);set_word(building_data,0x10,48399);select(13);
        require(tile_data[0x10]==1 && !changes.size(),"late unit/building prevents edit");occupied_fixture=building_fixture=false;
    }
    require(apply_tile(area_data,2)==Result::HexRange,"unregistered tile not editable");
    // User/Claude 2026-10-29: hover/rejected input forwards before feature work or diagnostics.
    for(unsigned condition=0;condition<4;++condition) {
        open_picker();opening_tile=tile_data;
        input_pressed=condition<2?0:2;input_suppressed=(condition&1)?-3:1;
        unsigned native_before=domestic_calls,opened_before=open_calls,logs_before=hex_logs;
        unsigned reads_before=reads,member_before=membership_calls,suppress_before=suppress_checks;
        require(click_tile(tile_data)==93,"false click input delegates native result");
        require(domestic_calls==native_before+1 && open_calls==opened_before && hex_logs==logs_before,
            "false click input invokes original once without intervention or logging");
        require(reads==reads_before && membership_calls==member_before && opening_tile==tile_data && expanded_state==hex_state,
            "false input leaves feature context and terrain checks untouched");
        require(suppress_checks==suppress_before+(input_pressed?1:0),"click predicates short circuit like native");
    }
    input_pressed=-2;input_suppressed=0;
    unsigned negative_open=open_calls;click_tile(tile_data);
    require(open_calls==negative_open+1,"any nonzero button result with zero suppression accepts click");
    input_pressed=1;
    for(unsigned immediate=0;immediate<2;++immediate)for(unsigned pos=0;pos<2;++pos) {
        immediate_build=immediate!=0;position_success=pos!=0;tile_data[0x10]=1;
        unsigned before=domestic_calls,opened=open_calls;click_tile(tile_data);
        require(domestic_calls==before && open_calls==opened+1 && tile_data[0x10]==1,"regional click opens picker without edit");
        *reinterpret_cast<int *>(domestic_state+0x4B8)=0;
        before=domestic_calls;opened=open_calls;click_tile(tile_data);
        require(domestic_calls==before+1 && open_calls==opened,"ordinary regional development keeps the original land click (no advisor marker)");
        terrain_edit::advisor_city_pending=domestic_state;terrain_edit::advisor_city_armed_ms=GetTickCount64();
        put(domestic_state,0x470,domestic_state);
        before=domestic_calls;opened=open_calls;click_tile(tile_data);
        require(domestic_calls==before && open_calls==opened+1 && dword(domestic_state,0x4B8)==0x54454431,"the advisor-armed city gets the marker on its first land click");
        terrain_edit::advisor_city_pending=nullptr;
        *reinterpret_cast<int *>(domestic_state+0x4B8)=0x54454431;
        if(!immediate) {
            require(opening_tile==tile_data,"deferred state retains exact tile");
            put(hex_state,0x470,tile_data);native_caller=image+0x726D5C;native_count=4;hex_menu_state(hex_state);
        }
        require(!opening_tile && expanded_tile==tile_data,"state consumes pending tile");
    }
    hex_completion[0x477]=0x12;hex_completion[0x47C]=0x34;
    *reinterpret_cast<unsigned *>(hex_completion+0x478)=0xDEADBEEF;
    reinterpret_cast<void(__fastcall *)(HexFunctor *)>(image+0x4590A0)(&retained_functor);
    require(dword(hex_completion,0x478)==0 && hex_completion[0x477]==0x12 && hex_completion[0x47C]==0x34,"actual completion only clears static dummy dword");
    require(dword(domestic_state,0x478)==0xA5A5A5A5,"completion leaves regional state untouched");immediate_build=false;
    for(unsigned t=0;t<256;++t) {
        tile_data[0x10]=static_cast<unsigned char>(t);unsigned before=domestic_calls,opened=open_calls;
        click_tile(tile_data);
        require(domestic_calls==before+(allowed(t)?0:1) && open_calls==opened+(allowed(t)?1:0),"all 256 terrain kinds intercept or delegate");
        require(tile_data[0x10]==t,"opening never edits terrain");
    }
    // Native membership is authoritative; no guessed state or area city-ID field is read.
    tile_data[0x10]=1;area_data[0x35]=255;membership_result=2;
    unsigned opened=open_calls;click_tile(tile_data);require(open_calls==opened+1,"any nonzero 70E940 result accepts area without guessed city field");membership_result=1;
    for(unsigned condition=0;condition<12;++condition) {
        tile_data[0x10]=1;invalid_tile=condition==0;missing_area=condition==1;blocked=condition==2;membership_result=condition==3?0:1;
        occupied_fixture=condition==4;building_fixture=condition==5;set_word(unit_data,0x2A,48399);set_word(building_data,0x10,48399);
        enabled=condition!=6;mod_features.convenience=condition!=7;mod_features.terrain_edit=condition!=8;
        put(domestic_functor,8,condition==10?nullptr:condition==11?other_state:domestic_state);
        Obj tile=condition==9?nullptr:tile_data;unsigned before=domestic_calls,old_open=open_calls;
        click_tile(tile);require(domestic_calls==before+1 && open_calls==old_open,"invalid/missing/blocked/outside/unit/building/off/null/other-state delegates");
        enabled=mod_features.convenience=mod_features.terrain_edit=true;invalid_tile=missing_area=blocked=occupied_fixture=building_fixture=false;membership_result=1;
        put(domestic_functor,8,domestic_state);
    }
    // A unit/building on a different tile does not block this one.
    occupied_fixture=building_fixture=true;set_word(unit_data,0x2A,17);set_word(building_data,0x10,17);
    opened=open_calls;click_tile(tile_data);require(open_calls==opened+1,"other hex occupancy ignored");occupied_fixture=building_fixture=false;
    for(unsigned condition=0;condition<10;++condition) {
        tile_data[0x10]=1;opening_tile=tile_data;menu_scope=hex_state;put(hex_state,0x470,tile_data);
        enabled=condition!=0;mod_features.convenience=condition!=1;mod_features.terrain_edit=condition!=2;
        if(condition==3)menu_scope=nullptr;if(condition==6)opening_tile=nullptr;
        if(condition==7)put(hex_state,0x470,other_tile);if(condition==8)put(hex_state,0x470,nullptr);
        MenuEntry *input=condition==9?nullptr:original;int count=condition==5?13:4;
        menu_build_from(image+(condition==4?0x730B99:0x726D5C),popup,2,3,4,input,count);
        require(passed==input && passed_count==count,"only exact pending tile/caller/count/context expands");
        enabled=mod_features.convenience=mod_features.terrain_edit=true;
    }
    open_picker();
    for(unsigned condition=0;condition<4;++condition) {
        open_picker();*reinterpret_cast<int *>(popup+0x80)=13;put(other_state,0x4A0,other_popup);
        put(ui_functor,8,condition==0?other_state:hex_state);put(hex_state,0x470,condition==1?other_tile:tile_data);
        p=condition==2?other_popup:popup;if(condition==3)clear_menu_context();
        require(hex_menu_click(ui_functor,arg)==92 && passed_functor==ui_functor && passed_arg==arg,"callback identity mismatch forwards exact arguments");
        require(!dword(hex_state,0x4A8) && !dword(other_state,0x4A8),"foreign callback never requests either state close");
        unsigned sets=hint_sets,before=hint_calls;
        *reinterpret_cast<int *>(p+0x80)=2;   // native four-row index
        if(condition==2)   // another column of the same picker state: hint events carry the column
            require(hex_menu_hint(ui_functor,arg)==0 && hint_calls==before && hint_sets==sets+1,"picker column hint clears by state and tile");
        else require(hex_menu_hint(ui_functor,arg)==94 && passed_functor==ui_functor && passed_arg==arg &&
            hint_calls==before+1 && hint_sets==sets,"foreign hint preserves exact original arguments and result");
        // local44 crash: rows past the native four never reach the 4-entry table.
        *reinterpret_cast<int *>(p+0x80)=13;sets=hint_sets;before=hint_calls;
        require(hex_menu_hint(ui_functor,arg)==0 && hint_calls==before && hint_sets==sets+1,"out-of-table hint row clears instead of native read");
    }
    p=popup;put(ui_functor,8,hex_state);put(hex_state,0x470,tile_data);
    unsigned null_sets=hint_sets;
    require(hex_menu_hint(nullptr,arg)==94 && !passed_functor && passed_arg==arg,"null hint functor delegates");
    require(hex_menu_hint(ui_functor,nullptr)==94 && passed_functor==ui_functor && !passed_arg,"null hint arg delegates");
    p=nullptr;require(hex_menu_hint(ui_functor,arg)==94 && passed_arg==arg,"null column without picker context delegates");
    require(hint_sets==null_sets,"null hint paths never set tooltip");p=popup;
    opening_tile=tile_data;build_result=0;hex_menu_state(hex_state);require(!expanded_state && !opening_tile,"failed state build clears callback and pending tile");build_result=77;
    Obj previous=other_state;menu_scope=previous;opening_tile=tile_data;hex_menu_state(hex_state);require(menu_scope==previous,"nested scope restored");
    opening_tile=tile_data;raise_state=true;
    __try {hex_menu_state(hex_state);} __except(EXCEPTION_EXECUTE_HANDLER) {}
    raise_state=false;require(menu_scope==previous && !opening_tile,"exception restores scope and clears pending tile");
    menu_scope=hex_state;opening_tile=tile_data;
    require(menu_build(popup,1,2,3,original,4)==77 && passed==original,"real wrapper rejects forged caller");menu_scope=nullptr;
    for(unsigned condition=0;condition<3;++condition) {
        tile_data[0x10]=1;open_picker();opening_tile=tile_data;
        enabled=condition!=0;mod_features.convenience=condition!=1;mod_features.terrain_edit=condition!=2;
        unsigned before=domestic_calls;click_tile(tile_data);
        require(!opening_tile && !expanded_state && domestic_calls==before+1,"all disable paths clear picker context");
        enabled=mod_features.convenience=mod_features.terrain_edit=true;
    }
    enabled=false;unsigned before=domestic_calls;
    require(domestic_click(nullptr,nullptr)==93 && domestic_calls==before+1 && !passed_domestic && !passed_arg,"disabled null callback delegates");enabled=true;
    changes.clear();open_picker();opening_tile=tile_data;before_world_change();require(!expanded_state && !opening_tile,"world boundary clears empty picker context");
    check_world();open_picker();opening_tile=tile_data;current_world=root+1;click_tile(tile_data);
    require(!expanded_state,"unexpected world change discards old callback binding");current_world=root;check_world();
    tile_data[0x10]=1;click_tile(tile_data);
#ifndef NO_RUNTIME_LOG
    require(hex_logs && last_hex==48399 && last_hex_terrain==1 && last_open==1,"intervention diagnostic exact hex and terrain");
#endif
    blocked=true;click_tile(tile_data);blocked=false;
#ifdef NO_RUNTIME_LOG
    require(hex_logs==0,"release strips diagnostics");
#else
    require(last_open==0 && last_hex==48399 && last_hex_terrain==1,"native delegation diagnostic");
#endif
    require(membership_calls>0 && input_checks>0 && suppress_checks>0,"native membership and click predicates exercised");
    check_highlight();
    VirtualFree(image,0,MEM_RELEASE);printf("{\"terrain_edit_ui_checks\":%u,\"failed\":0}\n",checks);
}
