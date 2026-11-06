// User 2026-11-02: actual title wrapper + actual hidden Windows controls, isolated INI only.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using Obj=unsigned char *;
static Obj image;static HMODULE self_module;
static unsigned checks,initializes,frames,leaves,allocations,registered,patches,fail_at,reloads;
static bool remove_in_frame;static wchar_t ini_path[MAX_PATH];
static std::array<unsigned char,0x700> fixture_state,fixture_root,fixture_new_root,fixture_object,fixture_peer;
static std::array<unsigned char,0x300> child,peer_child;
static bool fixture_bad_root,fixture_duplicate_last,fixture_new_root_state,callsite_prepare_fail,callsite_publish_fail,fixture_callsite_published;
static Obj inserted;static void (*callback)(Obj);static std::wstring fixture_caption;
static void check(bool v,const char *why){++checks;if(!v){std::fprintf(stderr,"%s\n",why);std::exit(1);}}
static void point(Obj p,unsigned off,Obj v){memcpy(p+off,&v,8);}
static void number(Obj p,unsigned off,unsigned v){memcpy(p+off,&v,4);}
static unsigned dword(Obj p,unsigned off){unsigned v;memcpy(&v,p+off,4);return v;}
static Obj pointer(Obj p,unsigned off){Obj v;memcpy(&v,p+off,8);return v;}
static unsigned trace_events,trace_found,trace_missing,trace_queries,trace_x,trace_y;
static void log_event(const char *event,unsigned=0,unsigned value=0,unsigned extra=0){
    if(strncmp(event,"ccz_title_",10)==0)++trace_events;
    if(!strcmp(event,"ccz_title_binding")){if(value)++trace_found;else ++trace_missing;}
    if(!strcmp(event,"ccz_title_node_xy_bits")){trace_x=value;trace_y=extra;}
}
struct CallLink {bool published=false;};
static Obj allocate_near(Obj){return static_cast<Obj>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));}
static void absolute_jump(Obj at,void *target){const unsigned char op[]={0xFF,0x25,0,0,0,0};memcpy(at,op,sizeof(op));memcpy(at+6,&target,8);}
static bool call_prepare(CallLink &c,unsigned,Obj){c={};fixture_callsite_published=false;return !callsite_prepare_fail;}
static bool call_publish(CallLink &c){if(callsite_publish_fail)return false;c.published=true;fixture_callsite_published=true;return true;}
static void call_restore(CallLink &c){c.published=false;fixture_callsite_published=false;}
static bool mod_ini_path(wchar_t *out,const wchar_t *){return wcscpy_s(out,MAX_PATH,ini_path)==0;}
namespace ccz_module {static void reload_appearance(){++reloads;}}
namespace lord_change {
struct Callable {void **vtable;void(*callback)(Obj);};
static unsigned forwarded[6];static Obj received;
static void *__fastcall copy(void *self,void *out){++forwarded[0];if(out)memcpy(out,self,sizeof(Callable));return out;}
static void *__fastcall move(void *self,void *out){++forwarded[1];if(out)memcpy(out,self,sizeof(Callable));return out;}
static void __fastcall invoke(void *self,Obj *arg){++forwarded[2];static_cast<Callable *>(self)->callback(arg?*arg:nullptr);}
static void *__fastcall type(void *){++forwarded[3];return nullptr;}
static void __fastcall erase(void *,bool heap){check(!heap,"inline forwarding never changes heap ownership");++forwarded[4];}
static void *__fastcall get(void *self){++forwarded[5];return &static_cast<Callable *>(self)->callback;}
static void record(Obj value){received=value;}
static void *callable_vtable[]={reinterpret_cast<void *>(&copy),reinterpret_cast<void *>(&move),reinterpret_cast<void *>(&invoke),reinterpret_cast<void *>(&type),reinterpret_cast<void *>(&erase),reinterpret_cast<void *>(&get)};
}
static void *function(unsigned rva);
template<class T> static T fn(unsigned rva){return reinterpret_cast<T>(function(rva));}
static bool patch_pointer(unsigned slot,unsigned original,void *replacement){
    ++patches;if(patches==fail_at)return false;
    if(slot==0x13E4E10)check(pointer(image,0x13E4E18)!=image+0x4F9310 && pointer(image,0x13E4E30)!=image+0x50B800,"publish native initialization only after both lifetime handlers");
    if(slot==0x13E4E10)check(fixture_callsite_published,"publish initialize only after callsite publication");
    if(pointer(image,slot)!=image+original)return false;point(image,slot,reinterpret_cast<Obj>(replacement));return true;
}
static void restore_pointer(unsigned slot,unsigned original,void *replacement){if(pointer(image,slot)==replacement)point(image,slot,image+original);}
namespace ccz_settings { static void tick(Obj){} static bool show(){return true;} static void abandon(){} }
#define COST_SCOPE(name) do {} while(0)
// 2026-11-05 user Layout compatibility design 8: ccz_title::install registers its three vtable slots and the 4E83F3 CALL
// with the SAN14_Layout.dll hook manager (layout_client). This stub plays the manager and keeps the former checks:
// registration order (initialize last, after both lifetime handlers and the CALL), refusal of non-original slots,
// failure injection, and the roll-back of a failed install (layout_client::Transaction).
#include "../src/layout_registry.h"
#include <vector>
template<class T> static T next_pick(T next,unsigned rva){return next?next:fn<T>(rva);}
namespace layout_client {
static std::vector<std::pair<unsigned,unsigned>> done_slots;
static bool call_done;
static const LayoutRow *row_for(unsigned rva){for(unsigned i=0;i<layout_row_count;++i)if(layout_rows[i].rva==rva)return &layout_rows[i];return nullptr;}
static int vt(unsigned slot,void *replacement,void **,uint32_t=0){
    const LayoutRow *row=row_for(slot);if(!row || !patch_pointer(slot,row->original,replacement))return 0;
    done_slots.push_back({slot,row->original});return 1;
}
static int call(unsigned,void *,void **,uint32_t=0){
    if(callsite_prepare_fail || callsite_publish_fail)return 0;
    fixture_callsite_published=true;call_done=true;return 1;
}
struct Transaction {
    size_t mark;bool had_call,done;
    Transaction():mark(done_slots.size()),had_call(call_done),done(false){}
    ~Transaction(){
        if(done)return;
        while(done_slots.size()>mark){auto s=done_slots.back();restore_pointer(s.first,s.second,pointer(image,s.first));done_slots.pop_back();}
        if(call_done && !had_call){call_done=false;fixture_callsite_published=false;}
    }
};
}
#include "../src/native_ccz_title.inl"
static unsigned long long __fastcall original_initialize(Obj s){
    number(fixture_peer.data(),0x40,3);
    check(s==fixture_state.data(),"title initialize this");++initializes;Obj root=fixture_bad_root?fixture_peer.data():(fixture_new_root_state?fixture_new_root.data():fixture_root.data());point(s,0x4B8,root);
    const int ids[]={13,20,22,24,26,28,30,32};for(int id:ids)ccz_title::attach_after_native(root,id,fixture_peer.data(),-1);
    if(fixture_duplicate_last)ccz_title::attach_after_native(root,32,fixture_peer.data(),-1);
    return 0x1122334455667788ull;
}
static void __fastcall original_leave(Obj s,Obj arg){check(arg==fixture_peer.data(),"native leave argument preserved");++leaves;check(ccz_title::current==nullptr && !ccz_title::button,"clear mod references before native destruction");memset(s,0xDD,fixture_state.size());}
static void __fastcall original_frame(Obj s,Obj a,Obj b){check(a==fixture_object.data() && b==fixture_peer.data(),"original frame arguments preserved");++frames;if(remove_in_frame)ccz_title::leave(s,fixture_peer.data());}
static Obj __fastcall allocate(size_t n){check(n==0x178,"native title pane allocation size");++allocations;return fixture_object.data();}
static void __fastcall noop(Obj){}
// User 2026-11-02: title button visibility must recover after the logo phase.
static unsigned invalidations;
static unsigned alpha_updates;
static void __fastcall set_alpha(Obj p,int value){
    check((p==fixture_object.data() || p==child.data()) && value>=0 && value<=255,"native alpha setter ABI and range");
    number(p,0x30,static_cast<unsigned>(value));++alpha_updates;
}
static void __fastcall invalidate(Obj p,int recursive){
    check((p==fixture_object.data() || p==child.data() || p==fixture_peer.data()) && recursive==1,"native recursive visibility invalidation ABI");
    number(p,0x10,~0u);++invalidations;
}
static unsigned long long __fastcall load(Obj p,int x,int y,Obj parent,const char *name,int flag,unsigned long long extra){check(p==fixture_object.data() && x==0 && y==0 && (parent==fixture_root.data() || parent==fixture_new_root.data()) && !strcmp(name,"02_01_menu_btn_offset_00_cn") && flag==0 && extra==0,"exact seven-argument native template loading ABI");return 1;}
static Obj __fastcall create(Obj table,int node,int style,int a,int b,int c){check(table==fixture_object.data()+0x130 && node==2 && style==0x1A && a==0 && b==1 && c==0,"original six-argument menu button builder ABI");return child.data();}
static void __fastcall insert(Obj table,int id,Obj p,int kind,int active,int extra){check((table==fixture_root.data()+0x130 || table==fixture_new_root.data()+0x130) && id==37 && p==fixture_object.data() && kind==8 && active==1 && extra==0,"exact title layout attach ABI");inserted=p;}
static Obj __fastcall find(Obj table,int id){
    if(table==fixture_root.data()+0x130 || table==fixture_new_root.data()+0x130)return id==37?inserted:id==32?fixture_peer.data():nullptr;
    if(table==fixture_object.data()+0x130 && id==2)return child.data();
    if(table==fixture_peer.data()+0x130 && id==2)return peer_child.data();return nullptr;
}
static void __fastcall text(Obj p,const wchar_t *s){check(p==child.data(),"native text target");fixture_caption=s;}
static void __fastcall setup(Obj p,int value){check(p==fixture_object.data() && value==0,"native pane setup ABI");}
static void __fastcall destroy(Obj,int){check(false,"unexpected native pane destruction");}
static void __fastcall register_pane(Obj p,int id,Obj o,int layer){
    if(id==37){check((p==fixture_root.data() || p==fixture_new_root.data()) && o==fixture_object.data() && layer==-1,"native owns extra menu pane");++registered;return;}
    check(id==13 || id==20 || id==22 || id==24 || id==26 || id==28 || id==30 || id==32,"native original title row id preserved");
    check(layer==-1 && o==fixture_peer.data(),"native original row attach arguments preserved");
}
static void __fastcall bind(Obj p,int event,void *local){check(p==child.data() && event==0x7FFFFFFE && pointer(static_cast<Obj>(local),56)==local,"native inline callable buffer ABI");memcpy(&callback,static_cast<Obj>(local)+8,sizeof(callback));}
static std::array<unsigned char,0x180> trace_node;
static std::array<unsigned char,0x80> trace_matrix,trace_resource,trace_model;
static bool trace_lookup_missing;
static Obj __fastcall lookup_node(Obj model,int id){
    check(model==trace_model.data() && id==37,"same native node lookup ABI as 866230");++trace_queries;
    return trace_lookup_missing?nullptr:trace_node.data();
}
static void *function(unsigned rva){
    switch(rva){
    case 0x504670:return reinterpret_cast<void *>(&original_initialize);
    case 0x4F9310:return reinterpret_cast<void *>(&original_leave);
    case 0x50B800:return reinterpret_cast<void *>(&original_frame);
    case 0x3F6BC0:return reinterpret_cast<void *>(&allocate);
    case 0x864C50:case 0x92AB70:return reinterpret_cast<void *>(&noop);
    case 0x86E0C0:return reinterpret_cast<void *>(&load);
    case 0x92E430:return reinterpret_cast<void *>(&create);
    case 0x92E0B0:return reinterpret_cast<void *>(&insert);
    case 0x92CEE0:return reinterpret_cast<void *>(&find);
    case 0x866230:return reinterpret_cast<void *>(&register_pane);
    case 0x873370:return reinterpret_cast<void *>(&invalidate);
    case 0x873450:return reinterpret_cast<void *>(&set_alpha);
    case 0x1A3CB0:return reinterpret_cast<void *>(&bind);
    case 0xAD1A10:return reinterpret_cast<void *>(&lookup_node);
    default:check(false,"unexpected game function");return nullptr;
    }
}
static bool title_slots_original(){
    return pointer(image,ccz_title::kVtable+8)==image+0x504670 && pointer(image,ccz_title::kVtable+16)==image+0x4F9310 && pointer(image,ccz_title::kVtable+40)==image+0x50B800;
}
int wmain(int argc,wchar_t **argv){
    check(argc==4,"fixture takes running text, isolated ini, preview bitmap");self_module=GetModuleHandleW(nullptr);wcscpy_s(ini_path,argv[2]);
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x2800000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));check(image!=nullptr,"synthetic image");
    FILE *f=nullptr;_wfopen_s(&f,argv[1],L"rb");check(f!=nullptr,"retained actual running code");fread(image+0x1000,1,0x1300000,f);fclose(f);
    point(image,ccz_title::kVtable+8,image+0x504670);point(image,ccz_title::kVtable+16,image+0x4F9310);point(image,ccz_title::kVtable+40,image+0x50B800);
    check(ccz_title::guards_match(),"all guards match independent retained running code");
    for(auto &g:ccz_title::guards){image[g.rva]^=1;check(!ccz_title::guards_match(),"foreign native code rejected");image[g.rva]^=1;}
    for(unsigned fail:{1u,2u}){fail_at=fail;callsite_prepare_fail=false;callsite_publish_fail=false;patches=0;check(!ccz_title::install(),"injected lifetime publication failure rejected");check(ccz_title::guards_match() && title_slots_original(),"earlier title slots rolled back");}
    fail_at=0;callsite_prepare_fail=true;callsite_publish_fail=false;patches=0;check(!ccz_title::install(),"callsite preparation failure rejected");check(ccz_title::guards_match() && title_slots_original(),"prepare failure leaves title slots unchanged");
    callsite_prepare_fail=false;callsite_publish_fail=true;patches=0;check(!ccz_title::install(),"callsite publication failure rejected");check(ccz_title::guards_match() && title_slots_original() && !fixture_callsite_published,"callsite failure rolls back title slots");
    callsite_publish_fail=false;fail_at=3;patches=0;check(!ccz_title::install(),"initialize publication failure rejected");check(ccz_title::guards_match() && !fixture_callsite_published,"initialize failure rolls back callsite and title slots");
    fail_at=0;patches=0;check(ccz_title::install(),"title hooks publish");
    check(pointer(image,ccz_title::kVtable+8)==reinterpret_cast<Obj>(&ccz_title::initialize) && pointer(image,ccz_title::kVtable+16)==reinterpret_cast<Obj>(&ccz_title::leave),"title hooks single owner");
    point(fixture_root.data(),0,image+ccz_title::kRoot);number(fixture_root.data()+0x130,0x10,37);
    check(ccz_title::initialize(fixture_state.data())==0x1122334455667788ull && !allocations,"missing layout keeps original initialize result and allocates nothing");
    number(fixture_root.data()+0x130,0x10,39);point(image+ccz_title::kPane,0,reinterpret_cast<Obj>(&destroy));point(image+ccz_title::kPane,0xA0,reinterpret_cast<Obj>(&setup));
    point(child.data(),0,image+0x1400000);point(image+0x1400000,0x1D8,reinterpret_cast<Obj>(&text));
    fixture_bad_root=true;check(ccz_title::initialize(fixture_state.data())==0x1122334455667788ull && registered==0 && !ccz_title::button,"foreign title root does not receive ninth menu");fixture_bad_root=false;point(fixture_state.data(),0x4B8,fixture_root.data());
    fixture_duplicate_last=true;check(ccz_title::initialize(fixture_state.data())==0x1122334455667788ull && fixture_caption==L"조조전" && registered==1,"duplicate last row does not duplicate ninth menu in one initialization");fixture_duplicate_last=false;
    point(fixture_new_root.data(),0,image+ccz_title::kRoot);number(fixture_new_root.data()+0x130,0x10,39);registered=0;fixture_new_root_state=true;check(ccz_title::initialize(fixture_state.data())==0x1122334455667788ull && registered==1,"new title root gets one menu after prior initialization");fixture_new_root_state=false;point(fixture_state.data(),0x4B8,fixture_root.data());
    number(fixture_state.data(),0x470,0x13);ccz_title::intro_visibility(fixture_state.data());invalidations=0;
    number(fixture_peer.data(),0x40,3);number(peer_child.data(),0x40,3);number(fixture_object.data(),0x40,0);number(child.data(),0x40,0);
    number(fixture_object.data(),0x10,0);number(child.data(),0x10,0);
    ccz_title::frame(fixture_state.data(),fixture_object.data(),fixture_peer.data());check(dword(fixture_object.data(),0x40)==3 && dword(child.data(),0x40)==3,"visible/enabled flags satisfy native visibility predicate");
    check(invalidations==2 && dword(fixture_object.data(),0x10)==~0u && dword(child.data(),0x10)==~0u,"cached hidden state invalidated when title becomes visible");
    ccz_title::sync(fixture_object.data(),fixture_peer.data());check(invalidations==2,"unchanged visibility does not traverse descendants every frame");
    number(fixture_peer.data(),0x40,0);number(peer_child.data(),0x40,0);ccz_title::sync(fixture_object.data(),fixture_peer.data());ccz_title::sync(child.data(),peer_child.data());check(dword(fixture_object.data(),0x40)==0 && dword(child.data(),0x40)==0,"hidden native menu propagates to added pane and button");
    number(fixture_peer.data(),0x40,3);number(peer_child.data(),0x40,3);ccz_title::sync(fixture_object.data(),fixture_peer.data());ccz_title::sync(child.data(),peer_child.data());check(dword(fixture_object.data(),0x40)==3 && dword(child.data(),0x40)==3,"visible native menu restores added pane and button");
    // User 2026-11-03 live evidence: visible flags and render binding, alpha zero.
    number(fixture_peer.data(),0x40,7);number(fixture_object.data(),0x40,7);
    number(peer_child.data(),0x40,7);number(child.data(),0x40,7);
    number(fixture_peer.data(),0x30,255);number(peer_child.data(),0x30,255);
    number(fixture_object.data(),0x30,0);number(child.data(),0x30,0);alpha_updates=0;
    ccz_title::sync(fixture_object.data(),fixture_peer.data());ccz_title::sync(child.data(),peer_child.data());
    check(alpha_updates==2 && dword(fixture_object.data(),0x30)==255 && dword(child.data(),0x30)==255,"alpha recovers even when visibility flags were already equal");
    number(fixture_peer.data(),0x30,64);ccz_title::sync(fixture_object.data(),fixture_peer.data());check(dword(fixture_object.data(),0x30)==64,"added row follows native partial fade");
    number(fixture_peer.data(),0x30,0);ccz_title::sync(fixture_object.data(),fixture_peer.data());check(!dword(fixture_object.data(),0x30),"native alpha hide remains effective");
    number(fixture_peer.data(),0x30,255);ccz_title::sync(fixture_object.data(),fixture_peer.data());ccz_title::sync(fixture_object.data(),fixture_peer.data());
    check(dword(fixture_object.data(),0x30)==255 && alpha_updates==5,"native alpha restores once without redundant setter calls");
    // User 2026-11-02: diagnose a valid control with a missing render binding.
    point(fixture_root.data(),0x98,trace_resource.data());point(trace_resource.data(),0x10,trace_model.data());
    point(trace_node.data(),0xE8,trace_matrix.data());number(trace_matrix.data(),0x30,0x3F800000);number(trace_matrix.data(),0x34,0x40000000);
    trace_queries=trace_found=trace_missing=0;unsigned events_before=trace_events;
    ccz_title::trace_binding(9,fixture_root.data(),37,fixture_object.data());trace_lookup_missing=true;
    ccz_title::trace_binding(9,fixture_root.data(),37,fixture_object.data());
#ifndef NO_RUNTIME_LOG
    check(trace_queries==2 && trace_found==1 && trace_missing==1,"diagnostics distinguishes control existence from render-node lookup failure");
    check(trace_x==0x3F800000 && trace_y==0x40000000 && trace_events>events_before,"diagnostics retains raw matrix position bits");
#else
    check(!trace_queries && trace_events==events_before,"shipping build performs no diagnostic node calls or snapshots");
#endif
    point(fixture_root.data(),0x98,nullptr);trace_lookup_missing=false;
    // User 2026-11-02: menu rows must not leak between startup logo screens.
    number(fixture_state.data(),0x470,2);number(fixture_peer.data(),0x40,0x83);number(fixture_object.data(),0x40,0x43);
    ccz_title::intro_visibility(fixture_state.data());
    check(dword(fixture_peer.data(),0x40)==0x82 && dword(fixture_object.data(),0x40)==0x42,"intro hides only menu visibility bit and keeps unrelated flags");
    number(fixture_peer.data(),0x40,0x8A);ccz_title::intro_visibility(fixture_state.data());
    number(fixture_state.data(),0x470,12);ccz_title::intro_visibility(fixture_state.data());
    check(dword(fixture_peer.data(),0x40)==0x8B && dword(fixture_object.data(),0x40)==0x43,"native menu-in restores initial visibility and preserves intervening pause changes");
    number(fixture_peer.data(),0x40,0);ccz_title::intro_visibility(fixture_state.data());check(!dword(fixture_peer.data(),0x40),"native hide after menu-in is preserved");
    number(fixture_state.data(),0x470,0x13);number(child.data(),0x40,3);
#ifndef NO_RUNTIME_LOG
    lord_change::Callable source{ccz_title::callback_vtable,&lord_change::record},copied{},moved{};
    check(ccz_title::callback_copy(&source,&copied)==&copied && copied.vtable==source.vtable && copied.callback==source.callback,"diagnostic copy forwards inline callable unchanged");
    check(ccz_title::callback_move(&source,&moved)==&moved && moved.callback==source.callback,"diagnostic move forwards unchanged");
    Obj argument=child.data();ccz_title::callback_invoke(&source,&argument);check(lord_change::received==argument,"diagnostic invoke preserves native argument indirection");
    check(ccz_title::callback_type(&source)==nullptr,"diagnostic type query preserves original null result");
    ccz_title::callback_delete(&source,false);check(ccz_title::callback_get(&source)==&source.callback,"diagnostic target query preserves original pointer");
    for(unsigned n:lord_change::forwarded)check(n==1,"each of six callable operations is forwarded exactly once");
#endif
    callback(child.data());check(ccz_title::requested,"native button queues editing on title frame");ccz_title::requested=false;
    number(child.data(),0x40,10);callback(child.data());check(!ccz_title::requested,"paused title cannot open another settings dialog");
    remove_in_frame=true;ccz_title::frame(fixture_state.data(),fixture_object.data(),fixture_peer.data());check(!ccz_title::current && leaves==1 && frames==2,"frame transition destroys title without post-native fixture_state access");
    VirtualFree(image,0,MEM_RELEASE);std::printf("{\"status\":\"ccz_settings_fixture_passed\",\"checks\":%u,\"live_game\":false}\n",checks);return 0;
}
