// SAN14_Layout.dll stage 1 fixture (user exception 2026-11-05). Compiles the real Layout sources into
// this executable (LAYOUT_FIXTURE seam: fake module ranges + synthetic image), never touches the game.
// Covers: hook manager outcomes for vtable slots / E8 sites / entry blocks, blocklist, unhook CAS + bypass,
// publish ordering, Present chain order (fog before and after us), subscriber order, window subclass, peers,
// unpack readiness, log buffering.
#define LAYOUT_FIXTURE
#ifdef NO_RUNTIME_LOG
#error layout fixture needs the log
#endif
#include "../src/layout_main.cpp"
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
static unsigned checks;
static void check(bool value,const char *label) {++checks;if(!value){fprintf(stderr,"FAILED: %s\n",label);exit(1);}}
using namespace layout;
// ---- synthetic memory map: [0,4M) image, then fake modules, then unregistered "heap" ----
static const size_t kRegion=0x1000000,kImage=0x400000;
static Bytes *region;
static const size_t kExt=0x400000,kOurs=0x480000,kDxgi=0x4C0000,kHeap=0x500000;
struct Event {std::string kind;unsigned a,b,c;};
static std::vector<Event> events;
static void capture(const char *kind,unsigned a,unsigned b,unsigned c) {events.push_back({kind,a,b,c});}
static unsigned count_events(const char *kind) {unsigned n=0;for(auto &e:events)if(e.kind==kind)++n;return n;}
static const Event *last_event(const char *kind) {for(size_t i=events.size();i>0;--i)if(events[i-1].kind==kind)return &events[i-1];return nullptr;}
static void fresh() {
    memset(hooks,0,sizeof(hooks));hook_total=0;published=0;pool=nullptr;pool_used=0;
    memset(overlay_subs,0,sizeof(overlay_subs));memset(wnd_subs,0,sizeof(wnd_subs));overlay_count=0;wnd_count=0;
    present_attempted=0;fixture_table=nullptr;fixture_table1=nullptr;present_next=nullptr;present1_next=nullptr;
    memset(region,0,kRegion);
    image=region;image_size=kImage;self_module=GetModuleHandleW(nullptr);
    fake_count=0;
    auto add=[](size_t offset,size_t size,const wchar_t *name) {
        fake_modules[fake_count].base=region+offset;fake_modules[fake_count].size=size;wcscpy_s(fake_modules[fake_count].name,name);++fake_count;
    };
    add(kExt,0x80000,L"fog_fake.dll");add(kOurs,0x40000,L"SAN14_Convenience.dll");add(kDxgi,0x40000,L"dxgi.dll");
    fixture_root[0]=0;events.clear();ring_count=0;sink=nullptr;set_sink(capture);
}
static void publish_now() {publish_hooks();}
static void fresh_published() {fresh();publish_now();}
static Bytes *make_stub(size_t offset,void *target) {
    Bytes *at=region+offset;at[0]=0xFF;at[1]=0x25;memset(at+2,0,4);*reinterpret_cast<void **>(at+6)=target;return at;
}
// Host thunk `jmp [rip+cell]` with its cell 16 bytes behind (design C-1).
static Bytes *make_thunk(size_t offset,void *handler,void ***cell_out) {
    Bytes *at=region+offset;void **cell=reinterpret_cast<void **>(at+16);*cell=handler;
    at[0]=0xFF;at[1]=0x25;int rel=static_cast<int>(reinterpret_cast<Bytes *>(cell)-(at+6));memcpy(at+2,&rel,4);
    *cell_out=cell;return at;
}
static void **slot_at(unsigned rva) {return reinterpret_cast<void **>(image+rva);}
// Distinct bodies: the linker must not fold the replacements into one address.
static volatile int marker_a,marker_b,marker_c;
static void rep_a() {marker_a=1;}
static void rep_b() {marker_b=2;}
static void rep_c() {marker_c=3;}
static void *R(void (*f)()) {return reinterpret_cast<void *>(f);}
static int state_of(int handle) {return api.hook_state(handle);}
static void set_call(unsigned site,unsigned target_rva) {
    image[site]=0xE8;int rel=static_cast<int>(static_cast<int64_t>(target_rva)-static_cast<int64_t>(site)-5);memcpy(image+site+1,&rel,4);
}
static void set_call_to(unsigned site,const void *target) {
    image[site]=0xE8;int rel=static_cast<int>(reinterpret_cast<const Bytes *>(target)-(image+site+5));memcpy(image+site+1,&rel,4);
}
static const Bytes *call_target(unsigned site) {return image+site+5+*reinterpret_cast<int *>(image+site+1);}
// ---- Present / window fakes ----
static char order[64];
static unsigned order_n;
static void note(char c) {if(order_n<sizeof(order)-1){order[order_n++]=c;order[order_n]=0;}}
static void reset_order() {order_n=0;order[0]=0;}
static void *fog_next,*fog_next1;
static HRESULT STDMETHODCALLTYPE dxgi_present(void *,UINT,UINT) {note('D');return 0;}
static HRESULT STDMETHODCALLTYPE dxgi_present1(void *,UINT,UINT,const void *) {note('E');return 0;}
static HRESULT STDMETHODCALLTYPE fog_present(void *chain,UINT sync,UINT flags) {
    note('F');return reinterpret_cast<HRESULT (STDMETHODCALLTYPE *)(void *,UINT,UINT)>(fog_next)(chain,sync,flags);
}
static HRESULT STDMETHODCALLTYPE fog_present1(void *chain,UINT sync,UINT flags,const void *p) {
    note('G');return reinterpret_cast<HRESULT (STDMETHODCALLTYPE *)(void *,UINT,UINT,const void *)>(fog_next1)(chain,sync,flags,p);
}
static void draw1(void *,void *ctx) {note('1');if(ctx)++*static_cast<int *>(ctx);}
static void draw2(void *,void *) {note('2');}
static void draw3(void *,void *) {note('3');}
static void *vt[32],*vt1[32];
static HRESULT call_present(UINT flags) {return reinterpret_cast<HRESULT (STDMETHODCALLTYPE *)(void *,UINT,UINT)>(vt[8])(nullptr,0,flags);}
static HRESULT call_present1(UINT flags) {return reinterpret_cast<HRESULT (STDMETHODCALLTYPE *)(void *,UINT,UINT,const void *)>(vt1[22])(nullptr,0,flags,nullptr);}
static std::wstring make_temp_root() {
    wchar_t temp[MAX_PATH];GetTempPathW(MAX_PATH,temp);
    std::wstring root=std::wstring(temp)+L"san14_layout_fixture_"+std::to_wstring(GetCurrentProcessId());
    CreateDirectoryW(root.c_str(),nullptr);CreateDirectoryW((root+L"\\SAN14Mods").c_str(),nullptr);
    CreateDirectoryW((root+L"\\SAN14Mods\\modules").c_str(),nullptr);return root;
}
static void write_ini(const std::wstring &root,const wchar_t *name,int enabled) {
    std::wstring path=root+L"\\SAN14Mods\\modules\\"+name;
    FILE *f=nullptr;if(_wfopen_s(&f,path.c_str(),L"wb")||!f)check(false,"ini written");
    fprintf(f,"; fixture\r\n[Mod]\r\nId=Test\r\nFile=x.dll\r\nEnabled=%d\r\n",enabled);fclose(f);
}
static int wnd_consume_msg;
static int proc1(void *,unsigned msg,uintptr_t,intptr_t,void *) {if(msg>=WM_USER)note('a');return 0;}
static int proc2(void *,unsigned msg,uintptr_t,intptr_t,void *) {if(msg>=WM_USER)note('b');return static_cast<int>(msg)==wnd_consume_msg;}
static int proc3(void *,unsigned msg,uintptr_t,intptr_t,void *) {if(msg>=WM_USER)note('c');return 0;}
static int base_calls;
static LRESULT CALLBACK base_proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    if(msg>=WM_USER){++base_calls;note('P');}
    return DefWindowProcW(hwnd,msg,w,l);
}
static void fill_probes(Bytes *buffer,unsigned count) {
    memset(buffer,0,0x900000+0x1000);
    for(unsigned i=0;i<count;++i)memcpy(buffer+probes[i].rva,probes[i].bytes,5);
}
int main() {
    region=static_cast<Bytes *>(VirtualAlloc(nullptr,kRegion,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    check(region!=nullptr,"synthetic image allocated");
    // ===== API surface =====
    fresh();
    check(SAN14LayoutGetApi(1)==&api && SAN14LayoutGetApi(2)==nullptr && SAN14LayoutGetApi(0)==nullptr,"GetApi accepts only version 1");
    check(api.version==1 && api.size==sizeof(San14LayoutApiV1),"API size and version");
    check(api.on_layout_built(1,nullptr,nullptr,nullptr)==-1 && api.on_layout_destroyed(1,nullptr,nullptr)==-1 &&
          !api.load_sublayout(nullptr,"x",0,0) && api.attach(nullptr,0,nullptr)==-1 && !api.clone_control(nullptr,0,0,0) &&
          !api.create_label(nullptr,0) && api.set_text(nullptr,L"x")==-1 && api.set_visible(nullptr,1)==-1 &&
          api.bind_click(nullptr,nullptr,nullptr)==-1 && api.destroy(nullptr)==-1,"runtime UI entries are stage-1 stubs");
    check(api.unhook(0)==-1 && api.unhook(-5)==-1 && api.unhook(99)==-1 && api.hook_state(0)==-1 && api.hook_state(99)==-1,"invalid handles");
    {
        void *next=nullptr;
        check(api.hook_vtable(nullptr,0x1000,0x2000,R(rep_a),&next,0)==-1 && api.hook_vtable("t",0x1000,0x2000,nullptr,&next,0)==-1 &&
              api.hook_vtable("t",0x7FFFFFF0,0x2000,R(rep_a),&next,0)==-1 && api.hook_call("t",0x7FFFFFF0,0x10,R(rep_a),&next,0)==-1,"bad arguments rejected");
    }
    // ===== log buffering until the host module connects its writer =====
    fresh();
    {
        sink=nullptr;ring_count=0;events.clear();
        api.log("owner_x","layout_probe",1,2);api.log("owner_x","layout_probe",3,4);api.log("owner_x","",5,6);
        check(events.empty() && ring_count==2,"events before the sink exists are buffered (empty names dropped)");
        api.set_log_sink(capture);
        check(events.size()==2 && events[0].a==1 && events[1].b==4 && events[0].c==hash32("owner_x") && ring_count==0,"buffered events flush in order with the owner hash");
        api.log("owner_x","layout_probe",7,8);
        check(events.size()==3,"after connection events go straight to the writer");
        sink=nullptr;ring_count=0;events.clear();
        for(unsigned i=0;i<80;++i)api.log("o","layout_flood",i,0);
        api.set_log_sink(capture);
        check(events.size()==64 && events[63].a==63,"the buffer keeps the first 64 events");
    }
    // ===== vtable slots =====
    fresh();
    {
        void *next_original=nullptr,*next_late=nullptr,*next_normal=nullptr;
        *slot_at(0x1000)=image+0x2000;*slot_at(0x1010)=image+0x2010;*slot_at(0x1020)=image+0x2020;
        int late=api.hook_vtable("late",0x1010,0x2010,R(rep_b),&next_late,LAYOUT_LATE_INSTALL|LAYOUT_CHAIN_OK);
        int normal=api.hook_vtable("normal",0x1020,0x2020,R(rep_c),&next_normal,0);
        int first=api.hook_vtable("first",0x1000,0x2000,R(rep_a),&next_original,0);
        check(late>0 && normal>0 && first>0 && late!=normal && normal!=first,"handles are distinct and positive");
        check(state_of(first)==LAYOUT_STATE_PENDING && *slot_at(0x1000)==image+0x2000 && next_original==nullptr,"registration alone writes nothing");
        layout::publish();
        check(state_of(first)==LAYOUT_STATE_INSTALLED && *slot_at(0x1000)==R(rep_a) && next_original==image+0x2000,"original slot: installed, next=original");
        check(state_of(normal)==LAYOUT_STATE_INSTALLED && state_of(late)==LAYOUT_STATE_INSTALLED,"all registered hooks published together");
        unsigned seen[3],n=0;
        for(auto &e:events)if(e.kind=="layout_installed" && n<3)seen[n++]=e.a;
        check(n==3 && seen[0]==0x1020 && seen[1]==0x1000 && seen[2]==0x1010,"LATE_INSTALL hooks go after normal ones, then registration order");
        check(count_events("layout_installed")==3 && count_events("layout_refused")==0 && count_events("layout_published")==1 &&
              last_event("layout_published")->a==3,"one log line per decision, one publish line");
        check(last_event("layout_installed")->c==hash32("late"),"log carries the owner hash");
        check(api.unhook(first)==LAYOUT_UNHOOK_REMOVED && *slot_at(0x1000)==image+0x2000 && state_of(first)==LAYOUT_STATE_REMOVED,"unhook restores original when the slot is still ours");
        check(api.unhook(first)==LAYOUT_UNHOOK_INACTIVE,"second unhook is inactive");
        int after=api.hook_vtable("after",0x1030,0x2030,R(rep_a),&next_original,0);
        check(after>0 && state_of(after)!=LAYOUT_STATE_PENDING,"after publish, registration installs immediately");
    }
    fresh_published();
    {   // pending hook can be withdrawn before publish
        fresh();
        void *next=nullptr;*slot_at(0x1040)=image+0x2040;
        int h=api.hook_vtable("p",0x1040,0x2040,R(rep_a),&next,0);
        check(api.unhook(h)==LAYOUT_UNHOOK_REMOVED && state_of(h)==LAYOUT_STATE_REMOVED,"pending unhook removes the registration");
        publish_now();
        check(*slot_at(0x1040)==image+0x2040 && state_of(h)==LAYOUT_STATE_REMOVED,"a withdrawn registration is never installed");
    }
    fresh_published();
    {   // foreign / heap / other-own / already ours
        void *next=nullptr;
        Bytes *external=make_stub(kExt+0x100,R(rep_b));
        *slot_at(0x1100)=external;
        int h=api.hook_vtable("t",0x1100,0x2100,R(rep_a),&next,LAYOUT_CHAIN_OK);
        check(state_of(h)==LAYOUT_STATE_CHAINED && next==external && *slot_at(0x1100)==R(rep_a),"external module value: chained, next=that value");
        check(last_event("layout_chained")!=nullptr && last_event("layout_chained")->a==0x1100,"chained is logged");
        check(api.unhook(h)==LAYOUT_UNHOOK_REMOVED && *slot_at(0x1100)==external,"unhook restores the chained value");
        *slot_at(0x1110)=external;next=nullptr;
        h=api.hook_vtable("t",0x1110,0x2110,R(rep_a),&next,0);
        check(state_of(h)==LAYOUT_STATE_REFUSED && *slot_at(0x1110)==external && last_event("layout_refused")->b==LAYOUT_WHY_FOREIGN,"foreign value without CHAIN_OK is refused");
        h=api.hook_vtable("t",0x1110,0x2110,R(rep_a),&next,LAYOUT_CHAIN_OK|LAYOUT_REFUSE_IF_FOREIGN);
        check(state_of(h)==LAYOUT_STATE_REFUSED && *slot_at(0x1110)==external,"REFUSE_IF_FOREIGN wins over CHAIN_OK");
        void *real=reinterpret_cast<void *>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"MessageBoxW"));
        *slot_at(0x1120)=real;
        h=api.hook_vtable("t",0x1120,0x2120,R(rep_a),&next,LAYOUT_CHAIN_OK);
        check(state_of(h)==LAYOUT_STATE_CHAINED && next==real,"a real system module pointer is recognized as another module");
        *slot_at(0x1130)=region+kHeap+0x100;
        h=api.hook_vtable("t",0x1130,0x2130,R(rep_a),&next,LAYOUT_CHAIN_OK);
        check(state_of(h)==LAYOUT_STATE_REFUSED && *slot_at(0x1130)==region+kHeap+0x100 && last_event("layout_refused")->b==LAYOUT_WHY_HEAP,"heap pointer is refused even with CHAIN_OK");
        void *dynamic=VirtualAlloc(nullptr,0x1000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
        *slot_at(0x1138)=dynamic;
        h=api.hook_vtable("t",0x1138,0x2138,R(rep_a),&next,LAYOUT_CHAIN_OK);
        check(state_of(h)==LAYOUT_STATE_REFUSED && last_event("layout_refused")->b==LAYOUT_WHY_HEAP,"a real heap allocation is refused");
        VirtualFree(dynamic,0,MEM_RELEASE);
        Bytes *ours=make_stub(kOurs+0x100,R(rep_b));
        *slot_at(0x1140)=ours;
        h=api.hook_vtable("t",0x1140,0x2140,R(rep_a),&next,LAYOUT_CHAIN_OK);
        check(state_of(h)==LAYOUT_STATE_REFUSED && *slot_at(0x1140)==ours && last_event("layout_refused")->b==LAYOUT_WHY_OTHER_OWN,"a second SAN14 DLL value is refused (one owner per slot)");
        next=reinterpret_cast<void *>(0x1234);*slot_at(0x1150)=R(rep_a);
        h=api.hook_vtable("t",0x1150,0x2150,R(rep_a),&next,LAYOUT_CHAIN_OK);
        check(state_of(h)==LAYOUT_STATE_INSTALLED && *slot_at(0x1150)==R(rep_a) && next==reinterpret_cast<void *>(0x1234),"already ours: nothing is written");
        *slot_at(0x1160)=image+0x2160;*slot_at(0x1168)=image+0x2999;
        h=api.hook_vtable("t",0x1168,0x2168,R(rep_a),&next,LAYOUT_CHAIN_OK);
        check(state_of(h)==LAYOUT_STATE_CHAINED && next==image+0x2999,"another function inside the game image is treated as a foreign value");
    }
    fresh_published();
    {   // exactly one log line per decision
        void *next=nullptr;
        *slot_at(0x1170)=image+0x2170;*slot_at(0x1178)=make_stub(kExt+0x180,R(rep_b));*slot_at(0x1180)=region+kHeap+8;
        api.hook_vtable("one",0x1170,0x2170,R(rep_a),&next,0);
        api.hook_vtable("two",0x1178,0x2178,R(rep_a),&next,LAYOUT_CHAIN_OK);
        api.hook_vtable("three",0x1180,0x2180,R(rep_a),&next,0);
        check(events.size()==3 && events[0].kind=="layout_installed" && events[1].kind=="layout_chained" && events[2].kind=="layout_refused","one line each: installed, chained, refused");
    }
    fresh_published();
    {   // peer yield (R1 ini Enabled under <game>\SAN14Mods\modules)
        std::wstring root=make_temp_root();
        wcscpy_s(fixture_root,root.c_str());
        void *next=nullptr;*slot_at(0x1200)=image+0x2200;
        write_ini(root,L"SettingsHelpScenic.ini",1);
        int h=api.hook_vtable("scenery",0x1200,0x2200,R(rep_a),&next,LAYOUT_YIELD_IF_PEER|LAYOUT_CHAIN_OK);
        check(state_of(h)==LAYOUT_STATE_YIELDED && *slot_at(0x1200)==image+0x2200 && next==image+0x2200 && last_event("layout_yielded")->b==LAYOUT_WHY_PEER,"enabled R1 module of the same function: we yield");
        check(api.peer_present(L"SettingsHelpScenic.ini")==1 && api.peer_present(L"kernel32.dll")==1 && api.peer_present(L"zz_not_loaded_zz.dll")==0 &&
              api.peer_present(L"Missing.ini")==0 && api.peer_present(nullptr)==0,"peer_present: ini Enabled, loaded module, absent");
        write_ini(root,L"SettingsHelpScenic.ini",0);
        next=nullptr;
        h=api.hook_vtable("scenery",0x1200,0x2200,R(rep_a),&next,LAYOUT_YIELD_IF_PEER|LAYOUT_CHAIN_OK);
        check(state_of(h)==LAYOUT_STATE_INSTALLED && *slot_at(0x1200)==R(rep_a) && api.peer_present(L"SettingsHelpScenic.ini")==0,"disabled ini is not a peer: installed");
        *slot_at(0x1210)=image+0x2210;
        h=api.hook_vtable("unknown_owner",0x1210,0x2210,R(rep_a),&next,LAYOUT_YIELD_IF_PEER);
        check(state_of(h)==LAYOUT_STATE_INSTALLED,"unknown owner never yields");
        write_ini(root,L"SettingsHelpScenic.ini",1);
        *slot_at(0x1220)=image+0x2220;
        h=api.hook_vtable("scenery",0x1220,0x2220,R(rep_a),&next,0);
        check(state_of(h)==LAYOUT_STATE_INSTALLED,"without YIELD_IF_PEER the caller decides: installed");
        // call and block hooks yield as well
        write_ini(root,L"SceneSkip.ini",1);set_call(0x3300,0x4300);
        int c=api.hook_call("scene_yield",0x3300,0x4300,R(rep_a),&next,LAYOUT_YIELD_IF_PEER);
        check(state_of(c)==LAYOUT_STATE_YIELDED && call_target(0x3300)==image+0x4300,"E8 site yields to an enabled same-function peer");
        const Bytes entry[9]={0x40,0x55,0x53,0x56,0x57,0x48,0x83,0xEC,0x20};
        memcpy(image+0x5100,entry,9);
        int b=api.hook_block("scene_yield",0x5100,entry,9,make_stub(kOurs+0x500,R(rep_a)),LAYOUT_YIELD_IF_PEER);
        check(state_of(b)==LAYOUT_STATE_YIELDED && !memcmp(image+0x5100,entry,9),"entry block yields to an enabled same-function peer");
        DeleteFileW((root+L"\\SAN14Mods\\modules\\SettingsHelpScenic.ini").c_str());DeleteFileW((root+L"\\SAN14Mods\\modules\\SceneSkip.ini").c_str());
        RemoveDirectoryW((root+L"\\SAN14Mods\\modules").c_str());RemoveDirectoryW((root+L"\\SAN14Mods").c_str());RemoveDirectoryW(root.c_str());
        fixture_root[0]=0;
    }
    fresh_published();
    {   // slot_owner
        wchar_t name[64];
        *slot_at(0x1300)=image+0x2300;*slot_at(0x1308)=make_stub(kExt+0x200,R(rep_a));
        *slot_at(0x1310)=make_stub(kOurs+0x200,R(rep_a));*slot_at(0x1318)=region+kHeap+0x40;
        check(api.slot_owner(0x1300,name,64)>0 && !wcscmp(name,L"game"),"slot_owner: game image");
        check(api.slot_owner(0x1308,name,64)>0 && !wcscmp(name,L"fog_fake.dll"),"slot_owner: another module");
        check(api.slot_owner(0x1310,name,64)>0 && !wcscmp(name,L"SAN14_Convenience.dll"),"slot_owner: our DLL");
        check(api.slot_owner(0x1318,name,64)>0 && !wcscmp(name,L"heap"),"slot_owner: heap");
        check(api.slot_owner(0x1300,nullptr,64)==-1 && api.slot_owner(0x1300,name,1)==-1,"slot_owner argument checks");
    }
    // ===== unhook after another owner chained over us: bypass =====
    fresh_published();
    {
        void *next=nullptr;void **cell=nullptr;
        *slot_at(0x1400)=image+0x2400;
        Bytes *thunk=make_thunk(kOurs+0x300,R(rep_a),&cell);
        int h=api.hook_vtable("t",0x1400,0x2400,thunk,&next,0);
        check(state_of(h)==LAYOUT_STATE_INSTALLED && *slot_at(0x1400)==thunk && next==image+0x2400,"thunk host hook installed");
        void *over_us=make_stub(kExt+0x400,R(rep_b));
        *slot_at(0x1400)=over_us;   // fog chained over us
        check(api.unhook(h)==LAYOUT_UNHOOK_BYPASSED && state_of(h)==LAYOUT_STATE_BYPASSED && *cell==image+0x2400 &&
              *slot_at(0x1400)==over_us && count_events("layout_bypassed")==1,"CAS failure: thunk cell rewritten to next (bypass), foreign slot untouched");
        *slot_at(0x1410)=image+0x2410;
        h=api.hook_vtable("t",0x1410,0x2410,R(rep_c),&next,0);
        *slot_at(0x1410)=make_stub(kExt+0x410,R(rep_b));
        check(api.unhook(h)==LAYOUT_UNHOOK_STUCK && state_of(h)==LAYOUT_STATE_STUCK && count_events("layout_stuck")==1,"CAS failure without a host thunk: stuck, nothing written");
        h=api.hook_vtable("t",0x1420,0x2420,R(rep_c),&next,0);   // slot value 0 = heap: refused
        check(state_of(h)==LAYOUT_STATE_REFUSED && api.unhook(h)==LAYOUT_UNHOOK_INACTIVE,"refused hooks cannot be unhooked");
    }
    // ===== E8 call sites =====
    fresh_published();
    {
        void *next=nullptr;
        memset(image+0x2FF0,0xAA,0x30);
        set_call(0x3000,0x4000);
        unsigned char before[0x30];memcpy(before,image+0x2FF0,sizeof(before));
        int h=api.hook_call("c",0x3000,0x4000,R(rep_a),&next,0);
        check(state_of(h)==LAYOUT_STATE_INSTALLED && next==image+0x4000 && image[0x3000]==0xE8,"E8 to the original target: installed, next=original");
        const Bytes *relay=call_target(0x3000);
        check(in_pool(relay) && relay_target(relay)==R(rep_a) && relay[0]==0xFF && relay[1]==0x25,"E8 now reaches a Layout relay that jumps to the replacement");
        bool neighbours=true;
        for(unsigned i=0;i<sizeof(before);++i) {unsigned at=0x2FF0+i;if(at>=0x3001 && at<=0x3004)continue;if(image[at]!=before[i])neighbours=false;}
        check(neighbours,"the 16-byte block outside rel32 is preserved");
        check(api.unhook(h)==LAYOUT_UNHOOK_REMOVED && call_target(0x3000)==image+0x4000 && !memcmp(image+0x2FF0,before,sizeof(before)),"unhook restores the exact original bytes");
        Bytes *external=make_stub(kExt+0x800,R(rep_b));
        set_call_to(0x3100,external);
        h=api.hook_call("c",0x3100,0x4100,R(rep_a),&next,LAYOUT_CHAIN_OK);
        check(state_of(h)==LAYOUT_STATE_CHAINED && next==external && in_pool(call_target(0x3100)),"E8 into another module: chained, next=T");
        check(api.unhook(h)==LAYOUT_UNHOOK_REMOVED && call_target(0x3100)==external,"chained E8 unhook restores T");
        set_call_to(0x3110,external);
        h=api.hook_call("c",0x3110,0x4110,R(rep_a),&next,0);
        check(state_of(h)==LAYOUT_STATE_REFUSED && call_target(0x3110)==external,"E8 into another module without CHAIN_OK: refused");
        set_call_to(0x3120,region+kHeap+0x200);
        h=api.hook_call("c",0x3120,0x4120,R(rep_a),&next,LAYOUT_CHAIN_OK);
        check(state_of(h)==LAYOUT_STATE_REFUSED && last_event("layout_refused")->b==LAYOUT_WHY_HEAP && call_target(0x3120)==region+kHeap+0x200,"E8 into heap: refused");
        image[0x3130]=0x90;
        h=api.hook_call("c",0x3130,0x4130,R(rep_a),&next,0);
        check(state_of(h)==LAYOUT_STATE_REFUSED && last_event("layout_refused")->b==LAYOUT_WHY_BAD_SITE && image[0x3130]==0x90,"non-E8 site: refused");
        set_call(0x3140,0x4140);
        int first=api.hook_call("c",0x3140,0x4140,R(rep_a),&next,0);
        const Bytes *placed=call_target(0x3140);
        int again=api.hook_call("c",0x3140,0x4140,R(rep_a),&next,0);
        check(state_of(first)==LAYOUT_STATE_INSTALLED && state_of(again)==LAYOUT_STATE_INSTALLED && call_target(0x3140)==placed,"re-registering the same replacement is a no-op");
        int other=api.hook_call("c2",0x3140,0x4140,R(rep_b),&next,0);
        check(state_of(other)==LAYOUT_STATE_REFUSED && last_event("layout_refused")->b==LAYOUT_WHY_OTHER_OWN,"another replacement behind our relay: refused (one owner)");
        set_call(0x3150,0x4999);
        h=api.hook_call("c",0x3150,0x4150,R(rep_a),&next,LAYOUT_CHAIN_OK);
        check(state_of(h)==LAYOUT_STATE_CHAINED && next==image+0x4999,"E8 to another game function: chained");
        // rel32 crossing a 16-byte block (site offset 13): one guarded write, restorable
        set_call(0x310D+0x100,0x4200);
        unsigned char raw_before[32];memcpy(raw_before,image+0x3200,32);
        h=api.hook_call("c",0x320D,0x4200,R(rep_a),&next,0);
        check(state_of(h)==LAYOUT_STATE_INSTALLED && in_pool(call_target(0x320D)) && next==image+0x4200,"E8 site with offset > 11 installs through the guarded raw write");
        check(api.unhook(h)==LAYOUT_UNHOOK_REMOVED && !memcmp(raw_before,image+0x3200,32),"raw-write site restores exactly");
        // someone modified the site after us: bypass through our relay
        set_call(0x3300,0x4300);
        h=api.hook_call("c",0x3300,0x4300,R(rep_a),&next,0);
        const Bytes *mine=call_target(0x3300);
        set_call_to(0x3300,make_stub(kExt+0xA00,R(rep_b)));
        check(api.unhook(h)==LAYOUT_UNHOOK_BYPASSED && state_of(h)==LAYOUT_STATE_BYPASSED && relay_target(mine)==image+0x4300 && call_target(0x3300)!=mine,"E8 site changed under us: relay switches to bypass, site untouched");
    }
    // ===== entry blocks and the blocklist =====
    fresh_published();
    {
        void *dummy=nullptr;(void)dummy;
        const Bytes expect[9]={0x40,0x55,0x53,0x56,0x57,0x48,0x83,0xEC,0x20};
        Bytes *relay=make_stub(kOurs+0x600,R(rep_a));
        static const uint32_t entries[]={0x9332B0,0x843B60,0x658DF0,0x870470,0x86E920,0x8704F0,0x7FC700,0x304D70,0x306A30,0x1F1DA0,0x1F1E90,0x1F2190,0x6C6B10,0x6C6DD0};
        bool all=true;
        for(uint32_t e:entries)if(api.hook_block("b",e,expect,9,relay,0)!=-2)all=false;
        check(all && count_events("layout_refused")==14 && last_event("layout_refused")->b==LAYOUT_WHY_BLOCKED_ENTRY,"every entry on the blocklist is refused and logged");
        check(api.hook_block("b",0x1F1D98,expect,9,relay,0)==-2 && api.hook_block("b",0x1F1DA0,expect,5,relay,0)==-2,"a patch range that covers a listed entry is refused");
        check(api.hook_block("b",0x1F1DA0-0x20,expect,9,relay,0)>0,"an unlisted neighbour is accepted");
        memset(image+0x5000,0xBB,16);memcpy(image+0x5000,expect,9);
        int h=api.hook_block("b",0x5000,expect,9,relay,0);
        check(state_of(h)==LAYOUT_STATE_INSTALLED && image[0x5000]==0xE9 && image+0x5005+*reinterpret_cast<int *>(image+0x5001)==relay,"block: E9 to the relay");
        bool tail=true;for(unsigned i=5;i<9;++i)if(image[0x5000+i]!=0x90)tail=false;for(unsigned i=9;i<16;++i)if(image[0x5000+i]!=0xBB)tail=false;
        check(tail,"block: displaced bytes padded with NOP, the rest of the 16 bytes preserved");
        check(api.unhook(h)==LAYOUT_UNHOOK_REMOVED && !memcmp(image+0x5000,expect,9) && image[0x500F]==0xBB,"block unhook restores the original bytes");
        image[0x5010]=0xE9;memcpy(image+0x5011,"\x01\x02\x03\x04",4);
        h=api.hook_block("b",0x5010,expect,9,relay,0);
        check(state_of(h)==LAYOUT_STATE_REFUSED && last_event("layout_refused")->b==LAYOUT_WHY_REDIRECTED,"entry already redirected (E9): refused, not followed");
        image[0x5020]=0xFF;image[0x5021]=0x25;
        h=api.hook_block("b",0x5020,expect,9,relay,0);
        check(state_of(h)==LAYOUT_STATE_REFUSED && last_event("layout_refused")->b==LAYOUT_WHY_REDIRECTED,"entry already FF25: refused");
        memcpy(image+0x5030,expect,9);image[0x5033]^=0x11;
        h=api.hook_block("b",0x5030,expect,9,relay,0);
        check(state_of(h)==LAYOUT_STATE_REFUSED && last_event("layout_refused")->b==LAYOUT_WHY_BYTES,"entry bytes differ from the expected original: refused");
        check(api.hook_block("b",0x500C,expect,9,relay,0)==-1 && api.hook_block("b",0x5040,expect,4,relay,0)==-1 && api.hook_block("b",0x5040,expect,17,relay,0)==-1 &&
              api.hook_block("b",0x5040,nullptr,9,relay,0)==-1,"block argument limits: inside one 16-byte block, 5..16 bytes");
        memcpy(image+0x5050,expect,9);
        h=api.hook_block("b",0x5050,expect,9,relay,0);
        image[0x5051]=0x11;
        check(api.unhook(h)==LAYOUT_UNHOOK_STUCK && state_of(h)==LAYOUT_STATE_STUCK && image[0x5051]==0x11,"block changed under us: stuck, nothing written");
    }
    fresh_published();
    {   // table capacity
        void *next=nullptr;int last=0;
        for(unsigned i=0;i<kMaxHooks;++i)last=api.hook_vtable("m",0x6000+i*8,0x7000,R(rep_a),&next,0);
        check(last==static_cast<int>(kMaxHooks) && api.hook_vtable("m",0x6800,0x7000,R(rep_a),&next,0)==-3,"hook table is bounded: -3 when full");
    }
    // ===== Present: fog before and after us =====
    fresh_published();
    {
        int hits=0;
        Bytes *dxgi=make_stub(kDxgi+0x40,reinterpret_cast<void *>(&dxgi_present)),*dxgi1=make_stub(kDxgi+0x80,reinterpret_cast<void *>(&dxgi_present1));
        fog_next=dxgi;fog_next1=dxgi1;
        vt[8]=make_stub(kExt+0x900,reinterpret_cast<void *>(&fog_present));vt1[22]=make_stub(kExt+0x940,reinterpret_cast<void *>(&fog_present1));
        void *fog_slot=vt[8],*fog_slot1=vt1[22];
        fixture_table=vt;fixture_table1=vt1;
        check(api.overlay_subscribe(&draw1,&hits)==0 && api.overlay_subscribe(&draw2,nullptr)==1 && api.overlay_subscribe(&draw1,&hits)==0,"overlay subscribe: order of registration, duplicates ignored");
        check(vt[8]==reinterpret_cast<void *>(&present_hook) && vt1[22]==reinterpret_cast<void *>(&present1_hook) && present_next==fog_slot && present1_next==fog_slot1,"fog hooked first: we chain in front of it (next=fog)");
        check(count_events("layout_chained")==2 && count_events("layout_installed")==0,"Present chain decisions are logged once each");
        reset_order();call_present(0);
        check(!strcmp(order,"12FD") && hits==1,"present order: subscribers (registration order), fog, original");
        reset_order();call_present(1);
        check(!strcmp(order,"FD"),"DXGI_PRESENT_TEST skips the overlays");
        reset_order();call_present1(0);
        check(!strcmp(order,"12GE"),"Present1 uses the same subscribers and chain");
        check(api.overlay_subscribe(&draw3,nullptr)==2,"a later subscriber is appended");
        reset_order();call_present(0);
        check(!strcmp(order,"123FD"),"appended subscriber runs last");
        api.overlay_subscribe(&draw1,&hits);
        check(count_events("layout_chained")==2,"present is hooked once, not per subscriber");
    }
    // ===== external review F-06: transient Present probe failures are retried (bounded), then final =====
    fresh_published();
    {
        void *table[32]={},*table1[32]={};
        table[8]=make_stub(kDxgi+0x40,reinterpret_cast<void *>(&dxgi_present));
        present_retries=0;present_retry_at=0;fixture_present_fail=2;fixture_table=table;fixture_table1=nullptr;
        check(api.overlay_subscribe(&draw1,nullptr)==0 && present_retries==1 && present_attempted==0 && table[8]!=reinterpret_cast<void *>(&present_hook),"F-06: first transient failure re-arms the one-shot");
        present_retry_at=GetTickCount64()+60000;present_retry_tick();
        check(present_retries==1 && present_attempted==0,"F-06: no retry before the interval");
        present_retry_at=0;present_retry_tick();
        check(present_retries==2 && present_attempted==0,"F-06: retry after the interval fails again (second probe failure)");
        present_retry_at=0;present_retry_tick();
        check(present_attempted==1 && table[8]==reinterpret_cast<void *>(&present_hook) && count_events("layout_present_retry")==2,"F-06: the third attempt installs; retries logged");
        present_retry_at=0;present_retry_tick();
        check(present_retries==2,"F-06: nothing re-arms after success");
        // bounded: a probe that keeps failing is given up after kPresentRetries
        fresh_published();present_retries=0;present_retry_at=0;fixture_present_fail=100;fixture_table=table1;
        api.overlay_subscribe(&draw1,nullptr);
        for(int i=0;i<40;++i) {present_retry_at=0;present_retry_tick();}
        check(present_attempted==1 && present_retries==kPresentRetries+1 && fixture_present_fail==100-(kPresentRetries+1),"F-06: retries are bounded");
        fixture_present_fail=0;fixture_table=nullptr;
    }
    fresh_published();
    {
        Bytes *dxgi=make_stub(kDxgi+0x40,reinterpret_cast<void *>(&dxgi_present)),*dxgi1=make_stub(kDxgi+0x80,reinterpret_cast<void *>(&dxgi_present1));
        vt[8]=dxgi;vt1[22]=dxgi1;fixture_table=vt;fixture_table1=vt1;
        api.overlay_subscribe(&draw1,nullptr);api.overlay_subscribe(&draw2,nullptr);
        check(vt[8]==reinterpret_cast<void *>(&present_hook) && present_next==dxgi && count_events("layout_installed")==2 && count_events("layout_chained")==0,"slot still original: installed, next=dxgi");
        fog_next=vt[8];fog_next1=vt1[22];                     // fog hooks after us and chains to our function
        vt[8]=make_stub(kExt+0x900,reinterpret_cast<void *>(&fog_present));vt1[22]=make_stub(kExt+0x940,reinterpret_cast<void *>(&fog_present1));
        reset_order();call_present(0);
        check(!strcmp(order,"F12D"),"fog hooked after us: fog, our subscribers, original");
        reset_order();call_present1(0);
        check(!strcmp(order,"G12E"),"Present1 with fog on top");
    }
    fresh_published();
    {
        Bytes *dxgi=make_stub(kDxgi+0x40,reinterpret_cast<void *>(&dxgi_present));
        vt[8]=dxgi;vt1[22]=nullptr;fixture_table=vt;fixture_table1=nullptr;
        check(api.overlay_subscribe(nullptr,nullptr)==-1,"null overlay callback rejected");
        unsigned i=0;int result=0;
        for(;i<kMaxSubs;++i)result=api.overlay_subscribe(&draw1,reinterpret_cast<void *>(static_cast<uintptr_t>(i+100)));
        check(result==static_cast<int>(kMaxSubs)-1 && api.overlay_subscribe(&draw1,reinterpret_cast<void *>(static_cast<uintptr_t>(999)))==-3,"overlay subscribers are bounded (8)");
        fresh_published();
        vt[8]=make_stub(kOurs+0xA00,R(rep_a));fixture_table=vt;fixture_table1=nullptr;   // a second SAN14 DLL already owns Present
        api.overlay_subscribe(&draw1,nullptr);
        check(vt[8]==make_stub(kOurs+0xA00,R(rep_a)) && last_event("layout_refused")!=nullptr && last_event("layout_refused")->b==LAYOUT_WHY_OTHER_OWN,"Present owned by another SAN14 DLL: refused");
    }
    // ===== window subclass =====
    fresh_published();
    {
        bound_window=nullptr;previous_proc=nullptr;
        WNDCLASSW wc={};wc.lpfnWndProc=&base_proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"layout_fixture_wnd";
        check(RegisterClassW(&wc)!=0,"fixture window class");
        HWND w1=CreateWindowExW(0,wc.lpszClassName,L"a",WS_POPUP,0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
        HWND w2=CreateWindowExW(0,wc.lpszClassName,L"b",WS_POPUP,0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
        check(w1 && w2,"fixture windows");
        check(api.wndproc_subscribe(&proc1,nullptr)==0 && api.wndproc_subscribe(&proc2,nullptr)==1 && api.wndproc_subscribe(&proc3,nullptr)==2 &&
              api.wndproc_subscribe(&proc1,nullptr)==0 && api.wndproc_subscribe(nullptr,nullptr)==-1,"wndproc subscribe: registration order, duplicates ignored");
        check(api.wndproc_bind(nullptr)==0,"binding no window is refused");
        check(api.wndproc_bind(w1)==1 && api.wndproc_bind(w1)==1 && api.wndproc_bind(w2)==0,"one live window at a time; rebinding the same window is fine");
        check(reinterpret_cast<void *>(GetWindowLongPtrW(w1,GWLP_WNDPROC))==reinterpret_cast<void *>(&layout_wndproc) && count_events("layout_wndproc_bound")==1,"subclass installed and logged once");
        wnd_consume_msg=static_cast<int>(WM_USER+7);base_calls=0;
        reset_order();SendMessageW(w1,WM_USER+5,0,0);
        check(!strcmp(order,"abcP") && base_calls==1,"subscribers in order, then the previous procedure");
        reset_order();LRESULT r=SendMessageW(w1,WM_USER+7,0,0);
        check(!strcmp(order,"ab") && r==0 && base_calls==1,"a consuming subscriber stops later subscribers and the previous procedure");
        DestroyWindow(w1);
        check(api.wndproc_bind(w2)==1,"after the bound window is gone another window can be bound");
        reset_order();SendMessageW(w2,WM_USER+5,0,0);
        check(!strcmp(order,"abcP"),"the new window dispatches through the same subscribers");
        DestroyWindow(w2);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    }
    // ===== unpack readiness (3/4 of the probes, stable across two polls) =====
    {
        Bytes *buffer=static_cast<Bytes *>(VirtualAlloc(nullptr,0x901000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        check(buffer!=nullptr,"probe buffer");
        Readiness st={};
        fill_probes(buffer,0);
        check(!unpack_ready(buffer,st) && !unpack_ready(buffer,st),"packed image (no probe matches) is never ready");
        st={};fill_probes(buffer,12);
        check(!unpack_ready(buffer,st) && unpack_ready(buffer,st),"fully unpacked image: ready on the second identical poll");
        st={};fill_probes(buffer,9);
        check(!unpack_ready(buffer,st) && unpack_ready(buffer,st),"exactly 3/4 matching is ready");
        st={};fill_probes(buffer,8);
        check(!unpack_ready(buffer,st) && !unpack_ready(buffer,st),"below 3/4 is not ready");
        st={};fill_probes(buffer,12);unpack_ready(buffer,st);memset(buffer+probes[0].rva,0,5);
        check(!unpack_ready(buffer,st),"a changing mask resets readiness");
        check(!unpack_ready(nullptr,st) && !st.sampled,"unreadable memory is handled");
        VirtualFree(buffer,0,MEM_RELEASE);
    }
    // ===== publish line carries hook count and peer mask =====
    fresh();
    {
        void *next=nullptr;*slot_at(0x1500)=image+0x2500;
        api.hook_vtable("t",0x1500,0x2500,R(rep_a),&next,0);
        layout::publish();layout::publish();
        check(count_events("layout_published")==2 && last_event("layout_published")->a==1 && last_event("layout_published")->b==0 && *slot_at(0x1500)==R(rep_a),"publish installs registrations, repeated publish is harmless");
        check(count_events("layout_installed")==1,"a second publish does not reinstall");
    }
    printf("{\"status\":\"layout_fixture_passed\",\"checks\":%u,\"synthetic_image\":true,\"gameplay_verified\":false}\n",checks);
    return 0;
}
