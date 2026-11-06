// SAN14_Layout.dll P4 fixture (user 2026-11-05 Layout compatibility design 4.4/4.5, docs/LAYOUT_P4_DIAG.md).
// Compiles the real Layout sources (LAYOUT_FIXTURE seam) against a synthetic image plus a FAKE game object model
// (game_resolver replaces 3F6BC0/864C50/86E0C0/92E430/92E0B0/92CEE0/84B150/85C9E0/8741D0/1A3CB0 and the vtable slots), so the
// real event dispatch, reentrancy guard, creation table, destroy order and the one-build probes run without any game code.
// Covers: observation registration policy, E8 chain over a foreign module, entry-byte mismatch / redirected entry for 865560,
// trampoline execution, subscribe/unsubscribe/KTID/name matching, nested-event drop, async counting, callback fault isolation,
// live-table overflow, every runtime UI service (accepted inside a callback, rejected outside), creation table and destroy
// order, refusal to touch objects we did not create, owner purge, and the diagnostic probes (KTID offset candidates, screen
// dump, lifetime test). A passing run is NOT in-game verification.
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
static const size_t kRegion=0x2000000,kImage=0x1600000;
static Bytes *region;
static const size_t kExt=0x1600000,kOurs=0x1680000,kDxgi=0x16C0000;
struct Event {std::string kind;unsigned a,b,c;};
static std::vector<Event> events;
static void capture(const char *kind,unsigned a,unsigned b,unsigned c) {events.push_back({kind,a,b,c});}
static unsigned count_events(const char *kind) {unsigned n=0;for(auto &e:events)if(e.kind==kind)++n;return n;}
static const Event *last_event(const char *kind) {for(size_t i=events.size();i>0;--i)if(events[i-1].kind==kind)return &events[i-1];return nullptr;}
static const Event *find_event(const char *kind,unsigned a) {for(auto &e:events)if(e.kind==kind && e.a==a)return &e;return nullptr;}
// ---- fake game object model ----
static const unsigned kPaneVt=0x13E4E80,kFooterVt=0x146AFE8,kSpeedVt=0x1473DC8,kPlainVt=0x1450000,kBtnVt=0x14A0000,kLabelVt=0x14A1000;
static const int kNodes=24;
static unsigned alloc_count;
static void *alloc_list[32768];
static void *fake_alloc(size_t n) {void *p=calloc(1,n<0x400?0x400:n);alloc_list[alloc_count++]=p;return p;}
static unsigned build_counter,b0a0_calls,foreign_calls,pane_dtors,label_dtors,click_count,load_x,load_y,attach_kind,last_style,last_kind;
static std::wstring button_text,label_text;
static std::string dtor_order;
static void *click_buf,*click_ctx_seen,*click_control_seen;
static bool label_init_fails;
static volatile unsigned long long *orig_counter;
static void set_slot(unsigned vt,unsigned slot,void *fn) {*reinterpret_cast<void **>(region+vt+slot*8)=fn;}
static Bytes **nodes_of(Bytes *table) {return *reinterpret_cast<Bytes ***>(table+8);}
static Bytes **ctrls_of(Bytes *table) {return *reinterpret_cast<Bytes ***>(table+0x20);}
static void build_table(Bytes *o) {
    Bytes *table=o+0x130;
    *reinterpret_cast<int *>(table+0x10)=kNodes;
    Bytes **nodes=static_cast<Bytes **>(fake_alloc(kNodes*8)),**ctrls=static_cast<Bytes **>(fake_alloc(kNodes*8));
    for(int i=0;i<kNodes;++i) {
        nodes[i]=static_cast<Bytes *>(fake_alloc(0x100));
        int *n=reinterpret_cast<int *>(nodes[i]);n[0]=i*10;n[1]=5;n[2]=100;n[3]=40;
    }
    *reinterpret_cast<Bytes ***>(table+8)=nodes;*reinterpret_cast<Bytes ***>(table+0x20)=ctrls;
}
// resource handle: [0] const, [3] free, [4] name hash, [6] KTID (offset 0x18), [9] build counter (changes every build)
static Bytes *make_handle(unsigned ktid,unsigned hash,unsigned d3) {
    unsigned *h=static_cast<unsigned *>(fake_alloc(0x100));
    h[0]=0x11111111;h[1]=0x22222222;h[3]=d3;h[4]=hash;h[6]=ktid;h[9]=++build_counter;
    *reinterpret_cast<Bytes **>(h+8)=static_cast<Bytes *>(fake_alloc(0x100));
    return reinterpret_cast<Bytes *>(h);
}
static Bytes *make_game_layout(unsigned vt_rva,unsigned ktid=0xBEEF0001u,unsigned d3=0) {
    Bytes *o=static_cast<Bytes *>(fake_alloc(0x400));
    *reinterpret_cast<Bytes **>(o)=image+vt_rva;build_table(o);
    *reinterpret_cast<Bytes **>(o+0x90)=make_handle(ktid,0,d3);*reinterpret_cast<Bytes **>(o+0x98)=o+1;*reinterpret_cast<unsigned *>(o+0x12C)=1;
    return o;
}
static Bytes *light_object() {Bytes *o=static_cast<Bytes *>(fake_alloc(0x400));*reinterpret_cast<Bytes **>(o)=image+kPlainVt;return o;}
static const Bytes *call_target(unsigned site) {return image+site+5+*reinterpret_cast<int *>(image+site+1);}
static void set_call_to(unsigned site,const void *target) {
    image[site]=0xE8;int rel=static_cast<int>(static_cast<const Bytes *>(target)-(image+site+5));memcpy(image+site+1,&rel,4);
}
static Bytes *make_stub(size_t offset,void *target) {
    Bytes *at=region+offset;at[0]=0xFF;at[1]=0x25;memset(at+2,0,4);*reinterpret_cast<void **>(at+6)=target;return at;
}
static void run_86E320(Bytes *o) {   // what 86E320 does after 86E140: call 92B0A0(obj+0x130,obj) through the (possibly hooked) site
    reinterpret_cast<void (__fastcall *)(Bytes *,Bytes *)>(const_cast<Bytes *>(call_target(0x86E338)))(o+0x130,o);
}
static void destroy_game(Bytes *o) {reinterpret_cast<void (__fastcall *)(Bytes *,unsigned long long,unsigned long long,unsigned long long)>(image+0x865560)(o,1,2,3);}
static Bytes *__fastcall f_alloc(size_t n) {return static_cast<Bytes *>(fake_alloc(n));}
static Bytes *__fastcall f_ctor(Bytes *o) {return o;}
static Bytes *__fastcall f_tablector(Bytes *t) {memset(t,0,0x48);return t;}
static unsigned long long __fastcall f_load(Bytes *pane,int x,int y,Bytes *,const char *name,int,unsigned long long) {
    if(!strcmp(name,"missing"))return 1;   // the wrapper returns 1 either way; failure shows in obj+0x12C
    *reinterpret_cast<int *>(pane+0x124)=x;*reinterpret_cast<int *>(pane+0x128)=y;load_x=static_cast<unsigned>(x);load_y=static_cast<unsigned>(y);
    build_table(pane);
    *reinterpret_cast<Bytes **>(pane+0x90)=make_handle(name_hash31(name)^0x5A5A0000u,name_hash31(name),0);
    *reinterpret_cast<Bytes **>(pane+0x98)=pane+1;*reinterpret_cast<unsigned *>(pane+0x12C)=1;
    run_86E320(pane);
    return 1;
}
static void __fastcall f_92B0A0(Bytes *,Bytes *) {++b0a0_calls;}
static Bytes *__fastcall f_92CEE0(Bytes *t,int idx) {
    if(idx<0 || idx>=*reinterpret_cast<int *>(t+0x10) || !ctrls_of(t))return nullptr;
    return ctrls_of(t)[idx];
}
static Bytes *__fastcall f_92E430(Bytes *t,int idx,int style,int kind,int,int) {
    if(idx<0 || idx>=*reinterpret_cast<int *>(t+0x10) || !nodes_of(t)[idx])return nullptr;   // like the game: node must still be unbound
    Bytes *c=static_cast<Bytes *>(fake_alloc(0x400));
    *reinterpret_cast<Bytes **>(c)=image+kBtnVt;
    const int *n=reinterpret_cast<int *>(nodes_of(t)[idx]);
    *reinterpret_cast<int *>(c+0x20)=n[0];*reinterpret_cast<int *>(c+0x24)=n[1];*reinterpret_cast<int *>(c+0x28)=n[2];*reinterpret_cast<int *>(c+0x2C)=n[3];
    *reinterpret_cast<int *>(c+0x80)=idx;
    nodes_of(t)[idx]=nullptr;ctrls_of(t)[idx]=c;last_style=static_cast<unsigned>(style);last_kind=static_cast<unsigned>(kind);
    return c;
}
static void __fastcall f_92E0B0(Bytes *t,int idx,Bytes *child,int kind,int,int) {
    if(idx<0 || idx>=*reinterpret_cast<int *>(t+0x10) || ctrls_of(t)[idx])return;
    ctrls_of(t)[idx]=child;attach_kind=static_cast<unsigned>(kind);
    if(nodes_of(t)[idx]) {const int *n=reinterpret_cast<int *>(nodes_of(t)[idx]);*reinterpret_cast<int *>(child+0x20)=n[0];*reinterpret_cast<int *>(child+0x24)=n[1];}
}
static unsigned long long __fastcall f_92CD00(Bytes *node) {const int *n=reinterpret_cast<int *>(node);return (static_cast<unsigned long long>(static_cast<unsigned>(n[1]))<<32)|static_cast<unsigned>(n[0]);}
static unsigned long long __fastcall f_92D040(Bytes *node) {const int *n=reinterpret_cast<int *>(node);return (static_cast<unsigned long long>(static_cast<unsigned>(n[3]))<<32)|static_cast<unsigned>(n[2]);}
static Bytes *__fastcall f_84B150(Bytes *l) {*reinterpret_cast<Bytes **>(l)=image+kLabelVt;return l;}
static void __fastcall f_label_init(Bytes *l,int,int,int,int,Bytes *,int) {if(!label_init_fails)*reinterpret_cast<Bytes **>(l+0x178)=static_cast<Bytes *>(fake_alloc(0x40));}
static void __fastcall f_85C9E0(Bytes *,const wchar_t *text) {label_text=text;}
static void __fastcall f_btn_settext(Bytes *,const wchar_t *text) {button_text=text;}
static void __fastcall f_setpos(Bytes *c,int x,int y) {*reinterpret_cast<int *>(c+0x20)=x;*reinterpret_cast<int *>(c+0x24)=y;}
static void __fastcall f_8741D0(Bytes *c,int on) {unsigned &f=*reinterpret_cast<unsigned *>(c+0x40);f=(f&~1u)|(on?1u:0u);}
static void __fastcall f_1A3CB0(Bytes *,int,void *local) {
    void *f=*reinterpret_cast<void **>(static_cast<Bytes *>(local)+0x38);
    void **vt=*reinterpret_cast<void ***>(f);
    click_buf=fake_alloc(0x100);
    reinterpret_cast<void *(__fastcall *)(void *,void *)>(vt[0])(f,click_buf);
    reinterpret_cast<void (__fastcall *)(void *,bool)>(vt[4])(f,false);
}
static void fire_click(Bytes *control) {
    void **vt=*reinterpret_cast<void ***>(click_buf);
    reinterpret_cast<void (__fastcall *)(void *,Bytes **)>(vt[2])(click_buf,&control);
}
static void __fastcall f_pane_dtor(Bytes *o,int) {
    Bytes *table=o+0x130;
    if(ctrls_of(table))for(int i=*reinterpret_cast<int *>(table+0x10)-1;i>=0;--i)if(ctrls_of(table)[i])dtor_order+='c';
    dtor_order+='P';++pane_dtors;
    reinterpret_cast<void (__fastcall *)(Bytes *,unsigned long long,unsigned long long,unsigned long long)>(image+0x865560)(o,0,0,0);
}
static void __fastcall f_label_dtor(Bytes *,int) {++label_dtors;}
static void __fastcall foreign_build(Bytes *,Bytes *) {++foreign_calls;}
static void *resolver(unsigned rva) {
    switch(rva) {
    case 0x3F6BC0:return reinterpret_cast<void *>(&f_alloc);
    case 0x864C50:return reinterpret_cast<void *>(&f_ctor);
    case 0x92AB70:return reinterpret_cast<void *>(&f_tablector);
    case 0x86E0C0:return reinterpret_cast<void *>(&f_load);
    case 0x92CEE0:return reinterpret_cast<void *>(&f_92CEE0);
    case 0x92E430:return reinterpret_cast<void *>(&f_92E430);
    case 0x92E0B0:return reinterpret_cast<void *>(&f_92E0B0);
    case 0x92CD00:return reinterpret_cast<void *>(&f_92CD00);
    case 0x92D040:return reinterpret_cast<void *>(&f_92D040);
    case 0x84B150:return reinterpret_cast<void *>(&f_84B150);
    case 0x85C9E0:return reinterpret_cast<void *>(&f_85C9E0);
    case 0x8741D0:return reinterpret_cast<void *>(&f_8741D0);
    case 0x1A3CB0:return reinterpret_cast<void *>(&f_1A3CB0);
    default:return nullptr;
    }
}
// ---- fresh state ----
static void reset_ui_state() {
    memset(live,0,sizeof(live));live_count=0;memset(pending,0,sizeof(pending));pending_next=0;
    memset(layout_subs,0,sizeof(layout_subs));memset(created,0,sizeof(created));created_count=0;
    pending_count=0;nested_dropped=0;async_loads=0;async_built=0;sync_built=0;destroyed_events=0;table_full=0;other_thread_calls=0;ui_rejected=0;first_thread=0;
    dispatch_depth=0;next_built=nullptr;destroy_trampoline=nullptr;built_handle=0;destroy_handle=0;observation_registered=0;subscriber_total=0;
    ktid_offset=~0u;fixture_probe=false;life_done=false;
    memset(name_dumps,0,sizeof(name_dumps));res_seq=0;memset(screen_seen,0,sizeof(screen_seen));ktid_match_logs=0;ktid_stat_logs=0;same_total=0;diff_total=0;
    memset(same_eq,0,sizeof(same_eq));memset(diff_eq,0,sizeof(diff_eq));
    built_calls=0;built_orig_ns=0;built_hook_ns=0;built_orig_max=0;built_hook_max=0;destroy_calls=0;destroy_ns=0;destroy_max=0;
    b0a0_calls=0;foreign_calls=0;pane_dtors=0;label_dtors=0;click_count=0;attach_kind=0;button_text.clear();label_text.clear();dtor_order.clear();
    click_buf=nullptr;label_init_fails=false;alloc_count=0;build_counter=0;
}
static void fresh_world() {
    for(unsigned i=0;i<alloc_count;++i)free(alloc_list[i]);
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
    reset_ui_state();
    game_resolver=&resolver;
    // image code: 92B0A0 stub -> fake, the E8 inside 86E320, and the 865560 entry (push rdi; sub rsp,30h; inc [counter]; add rsp,30h; pop rdi; ret)
    make_stub(0x92B0A0,reinterpret_cast<void *>(&f_92B0A0));
    set_call_to(0x86E338,image+0x92B0A0);
    orig_counter=reinterpret_cast<unsigned long long *>(region+0x1F00000);
    Bytes *entry=image+0x865560;
    static const Bytes head[6]={0x40,0x57,0x48,0x83,0xEC,0x30};
    memcpy(entry,head,6);
    entry[6]=0x48;entry[7]=0xFF;entry[8]=0x05;
    int rel=static_cast<int>(reinterpret_cast<Bytes *>(const_cast<unsigned long long *>(orig_counter))-(entry+6+7));memcpy(entry+9,&rel,4);
    entry[13]=0x48;entry[14]=0x83;entry[15]=0xC4;entry[16]=0x30;entry[17]=0x5F;entry[18]=0xC3;
    for(unsigned vt:{kPaneVt,kFooterVt,kSpeedVt,kPlainVt}) {set_slot(vt,0,reinterpret_cast<void *>(&f_pane_dtor));set_slot(vt,15,reinterpret_cast<void *>(&f_setpos));}
    set_slot(kBtnVt,15,reinterpret_cast<void *>(&f_setpos));set_slot(kBtnVt,59,reinterpret_cast<void *>(&f_btn_settext));
    set_slot(kLabelVt,0,reinterpret_cast<void *>(&f_label_dtor));set_slot(kLabelVt,39,reinterpret_cast<void *>(&f_label_init));
    publish_hooks();   // published: later registrations install at once
}
static int state_of(int handle) {return api.hook_state(handle);}
// ---- subscriber callbacks ----
struct Hit {unsigned n;Bytes *last;void *ctx;};
static Hit hit_wild,hit_name,hit_ktid,hit_both,hit_wrong,hit_dead,hit_late;
static void cb_hit(void *layout,void *ctx) {Hit *h=static_cast<Hit *>(ctx);++h->n;h->last=static_cast<Bytes *>(layout);h->ctx=ctx;}
static void cb_fault(void *,void *) {*static_cast<volatile int *>(nullptr)=1;}
static void click_cb(void *control,void *ctx) {++click_count;click_control_seen=control;click_ctx_seen=ctx;}
struct Svc {
    void *pane,*pane3,*missing,*btn,*btn_again,*btn_oob,*label,*label_again,*label_node_used,*pane2;
    int dx,pos_x,pos_y,moved_x,moved_y,set_text_ok,untracked_text,null_text,bind,attach_ok,attach_again,attach_untracked,attach_btn,
        attach_taken,vis,vis_game,vis_junk,d_btn,d_label,d_game,d_pane,d_pane_again,d_pane2,d_pane3,d_null;
    unsigned created_after_load,created_after_missing,created_before_destroy,created_after,pane_dtors_after_missing,nested_before,nested_after,
        flags_after_hide,load_x,load_y,attach_kind;
    std::string order;bool label_node;
};
static Svc S;
static Bytes *svc_game_layout;
static void cb_services(void *layout,void *) {
    Bytes *game=static_cast<Bytes *>(layout);
    S.nested_before=static_cast<unsigned>(nested_dropped);
    S.pane=api.load_sublayout(game,"cmn_footer_re_cn",3.4f,5.6f);
    S.load_x=load_x;S.load_y=load_y;S.nested_after=static_cast<unsigned>(nested_dropped);
    S.created_after_load=created_count;
    S.missing=api.load_sublayout(game,"missing",0,0);
    S.pane_dtors_after_missing=pane_dtors;S.created_after_missing=created_count;
    S.btn=api.clone_control(S.pane,14,2,0);
    S.btn_again=api.clone_control(S.pane,14,2,0);
    S.btn_oob=api.clone_control(S.pane,99,2,0);
    int x=0,y=0;
    api.get_position(S.btn,&x,&y);S.pos_x=x;S.pos_y=y;
    api.set_position(S.btn,x+29,y);api.get_position(S.btn,&S.moved_x,&S.moved_y);S.dx=S.moved_x-x;
    S.set_text_ok=api.set_text(S.btn,L"X1");
    S.untracked_text=api.set_text(game,L"nope");S.null_text=api.set_text(S.btn,nullptr);
    S.bind=api.bind_click(S.btn,&click_cb,reinterpret_cast<void *>(7));
    if(S.bind==0)fire_click(static_cast<Bytes *>(S.btn));
    S.label=api.create_label(S.pane,17);
    S.label_node=S.label && *reinterpret_cast<Bytes **>(static_cast<Bytes *>(S.label)+0x178)!=nullptr;
    api.set_text(S.label,L"L1");
    S.label_again=api.create_label(S.pane,17);
    S.label_node_used=api.create_label(S.pane,14);
    S.pane2=api.load_sublayout(game,"inner",0,0);
    S.attach_ok=api.attach(game,5,S.pane2);S.attach_kind=attach_kind;
    S.attach_again=api.attach(game,5,S.pane2);   // same child, same node: idempotent
    S.pane3=api.load_sublayout(game,"inner",0,0);
    S.attach_taken=api.attach(game,5,S.pane3);   // node already holds another control
    S.attach_untracked=api.attach(game,6,game);
    S.attach_btn=api.attach(game,6,S.btn);
    S.vis=api.set_visible(S.btn,0);S.flags_after_hide=*reinterpret_cast<unsigned *>(static_cast<Bytes *>(S.btn)+0x40)&1u;
    S.vis_game=api.set_visible(game,1);
    unsigned char junk[16]={};S.vis_junk=api.set_visible(junk,1);
    S.created_before_destroy=created_count;
    S.d_btn=api.destroy(S.btn);S.d_label=api.destroy(S.label);S.d_game=api.destroy(game);S.d_null=api.destroy(nullptr);
    size_t before=dtor_order.size();
    S.d_pane=api.destroy(S.pane);
    S.order=dtor_order.substr(before);
    S.d_pane_again=api.destroy(S.pane);
    S.d_pane2=api.destroy(S.pane2);
    S.d_pane3=api.destroy(S.pane3);
    S.created_after=created_count;
}
static void cb_owner(void *layout,void *) {
    svc_game_layout=static_cast<Bytes *>(layout);
    api.clone_control(layout,3,2,0);
}
int main() {
    region=static_cast<Bytes *>(VirtualAlloc(nullptr,kRegion,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    check(region!=nullptr,"synthetic image allocated");
    // ===== API surface and pure helpers =====
    fresh_world();
    check(api.size==sizeof(San14LayoutApiV1) && api.note_layout_load && api.layout_unsubscribe && api.set_position && api.get_position,"appended P4 entries present");
    check(name_hash31("cmn_footer_re_cn")==0xf65ecaeeu && name_hash31("inner")==0xc84b672au && name_hash31("")==0,"name hash x31 (86C570 loop) known values");
    check(api.layout_unsubscribe(0)==-1 && api.layout_unsubscribe(99)==-1 && api.layout_unsubscribe(1)==-1,"unsubscribe of nothing is -1");
    api.note_layout_load(nullptr,"x",0);api.note_layout_load(region,nullptr,0);
    check(api.on_layout_built(0,nullptr,nullptr,nullptr)==-1 && api.on_layout_destroyed(0,nullptr,nullptr)==-1,"null callback rejected");
    check(api.on_layout_built(0,"0123456789012345678901234567890123456789012345678",cb_hit,&hit_wild)==-1,"name of 48+ chars rejected");
    check(observation_registered==0 && hook_total==0,"rejected subscriptions register nothing");
    // ===== registration policy =====
    fresh_world();
    check(!observation_wanted() && !probe_enabled(),"no subscriber, no probe: observation not wanted (production default)");
    layout::publish();
    check(observation_registered==0 && hook_total==0 && count_events("layout_events_registered")==0,"publish without subscribers installs no observation hook");
    int late=api.on_layout_built(0,nullptr,cb_hit,&hit_late);
    check(late>0 && observation_registered==1 && built_handle>0 && destroy_handle>0,"late subscription registers both hooks");
    check(state_of(built_handle)==LAYOUT_STATE_INSTALLED && state_of(destroy_handle)==LAYOUT_STATE_INSTALLED,"both hooks installed at once (manager already published)");
    check(call_target(0x86E338)!=image+0x92B0A0 && image[0x865560]==0xE9 && image[0x865565]==0x90,"E8 inside 86E320 redirected, 865560 entry is E9 + NOP");
    check(api.hook_owner_state("layout_events")==LAYOUT_STATE_INSTALLED && api.hook_owner_state("layout_events_destroy")==LAYOUT_STATE_INSTALLED,"owner states");
    memset(&hit_late,0,sizeof(hit_late));
    Bytes *g1=make_game_layout(kPlainVt);
    run_86E320(g1);
    check(hit_late.n==1 && hit_late.last==g1 && hit_late.ctx==&hit_late,"built callback gets layout and ctx");
    check(b0a0_calls==1,"original 92B0A0 still runs (next)");
    check(sync_built==1 && live_count==1,"tracked after build");
    destroy_game(g1);
    check(*orig_counter==1,"destroy hook runs the original 865560 body through the trampoline");
    check(live_count==0 && destroyed_events==1,"destroy removes the live entry");
    destroy_game(light_object());
    check(*orig_counter==2 && destroyed_events==1,"destroying an object never built: original still runs, no event");
    check(api.layout_unsubscribe(late)==0 && api.layout_unsubscribe(late)==-1,"unsubscribe once");
    run_86E320(make_game_layout(kPlainVt));
    check(hit_late.n==1,"unsubscribed callback no longer called");
    // ===== matching =====
    fresh_world();
    ktid_offset=0x18;
    memset(&hit_wild,0,sizeof(Hit));memset(&hit_name,0,sizeof(Hit));memset(&hit_ktid,0,sizeof(Hit));memset(&hit_both,0,sizeof(Hit));memset(&hit_wrong,0,sizeof(Hit));memset(&hit_dead,0,sizeof(Hit));
    int hw=api.on_layout_built(0,nullptr,cb_hit,&hit_wild);
    check(api.on_layout_built(0,nullptr,cb_hit,&hit_wild)==hw,"same subscription returns the same handle");
    check(api.on_layout_built(0,"title_a",cb_hit,&hit_name)>0 && api.on_layout_built(0x11110001u,nullptr,cb_hit,&hit_ktid)>0 &&
          api.on_layout_built(0x11110001u,"title_a",cb_hit,&hit_both)>0 && api.on_layout_built(0x11110001u,"other",cb_hit,&hit_wrong)>0 &&
          api.on_layout_destroyed(0x11110001u,cb_hit,&hit_dead)>0,"filters accepted");
    Bytes *o1=make_game_layout(kPlainVt,0x11110001u),*o2=make_game_layout(kPlainVt,0x22220002u),*o3=make_game_layout(kPlainVt,0x11110001u);
    api.note_layout_load(o1,"title_a",0);run_86E320(o1);
    check(hit_wild.n==1 && hit_name.n==1 && hit_ktid.n==1 && hit_both.n==1 && hit_wrong.n==0,"name+KTID object matches wildcard, name, KTID and both, not the wrong name");
    api.note_layout_load(o2,"title_a",0);run_86E320(o2);
    check(hit_wild.n==2 && hit_name.n==2 && hit_ktid.n==1 && hit_both.n==1,"same name, other KTID: no KTID match");
    run_86E320(o3);
    check(hit_wild.n==3 && hit_name.n==2 && hit_ktid.n==2 && hit_both.n==1,"no name note: name filter does not match, KTID does");
    run_86E320(o1);
    check(hit_name.n==3 && hit_both.n==2,"rebuild keeps the name recorded at the first build");
    check(live_count==3,"three live layouts");
    destroy_game(o2);destroy_game(o3);
    check(hit_dead.n==1 && hit_dead.last==o3 && live_count==1,"destroyed event filtered by KTID");
    destroy_game(o3);
    check(hit_dead.n==1,"second destroy of the same object raises nothing");
    destroy_game(o1);
    check(hit_dead.n==2 && live_count==0,"destroy of the remaining layout");
    ktid_offset=~0u;
    Bytes *o4=make_game_layout(kPlainVt,0x11110001u);run_86E320(o4);
    check(hit_ktid.n==3 && hit_wild.n==5,"unknown KTID offset: KTID filters never match");
    // ===== subscription table =====
    fresh_world();
    int last=0;for(unsigned i=0;i<kSubMax;++i)last=api.on_layout_built(1000+i,nullptr,cb_hit,&hit_wild);
    check(last>0 && api.on_layout_built(5000,nullptr,cb_hit,&hit_wild)==-3,"33rd subscription: table full");
    check(api.layout_unsubscribe(last)==0 && api.on_layout_built(5000,nullptr,cb_hit,&hit_wild)==last,"a freed slot is reused");
    // ===== reentrancy, async, fault isolation =====
    fresh_world();
    memset(&hit_wild,0,sizeof(Hit));memset(&hit_late,0,sizeof(Hit));
    api.on_layout_built(0,nullptr,cb_fault,nullptr);
    api.on_layout_built(0,nullptr,cb_hit,&hit_wild);
    Bytes *a1=make_game_layout(kPlainVt);
    api.note_layout_load(a1,"async_one",1);
    run_86E320(a1);
    check(count_events("layout_event_callback_fault")==1 && hit_wild.n==1,"a faulting callback is isolated, later subscribers still run");
    check(async_loads==1 && async_built==1 && sync_built==0,"async load noted and counted when it reaches the built event");
    check(dispatch_depth==0,"depth restored after a fault");
    check(api.load_sublayout(a1,"cmn_footer_re_cn",0,0)==nullptr && ui_rejected>=1,"services refuse to run outside an event callback");
    {   // a load note for an object that dies unbuilt is dropped, so a later object at the same address is not misnamed
        Bytes *dead=light_object();
        api.note_layout_load(dead,"never_built",1);
        check(pending_count==1,"pending note counted");
        destroy_game(dead);
        check(pending_count==0,"destroy clears the stale note");
        api.note_layout_load(a1,"consumed",0);
        check(pending_count==1,"second note counted");
        run_86E320(a1);
        check(pending_count==0 && hit_wild.n==2,"the build consumes its note");
    }
    // ===== runtime UI services inside a callback =====
    fresh_world();
    check(api.load_sublayout(nullptr,"x",0,0)==nullptr && api.attach(nullptr,0,nullptr)==-1 && api.destroy(region)==-1 && api.set_visible(region,1)==-1,"outside an event everything is refused");
    S=Svc();
    Bytes *game=make_game_layout(kPlainVt);
    api.on_layout_built(0,nullptr,cb_services,nullptr);
    run_86E320(game);
    check(S.pane!=nullptr && S.created_after_load==1 && S.load_x==3 && S.load_y==6,"load_sublayout builds a tracked pane, float position rounded to ints");
    check(S.nested_after==S.nested_before+1,"the nested built event of our own pane is dropped (reentrancy guard)");
    check(S.missing==nullptr && S.pane_dtors_after_missing==1 && S.created_after_missing==1,"failed load destroys the half-built pane and tracks nothing");
    check(S.btn!=nullptr && S.btn_again==nullptr && S.btn_oob==nullptr && last_style==2 && last_kind==0,"clone_control: once per unbound node, bad node refused, game ABI args passed");
    check(count_events("layout_ui_clone_unavailable")==2,"clone failures are logged with the node");
    check(S.pos_x==140 && S.pos_y==5 && S.dx==29 && S.moved_y==5,"set_position moves a cloned control by +29");
    check(S.set_text_ok==0 && button_text==L"X1" && S.untracked_text==-1 && S.null_text==-1,"set_text on our button only");
    check(S.bind==0 && click_count==1 && click_ctx_seen==reinterpret_cast<void *>(7) && click_control_seen==S.btn,"bind_click: callable copied by 1A3CB0 calls back with control and ctx");
    check(S.label!=nullptr && S.label_node && label_text==L"L1" && S.label_again==nullptr && S.label_node_used==nullptr,"create_label: attached once, text node reached, bound nodes refused");
    check(S.pane2!=nullptr && S.pane3!=nullptr && S.attach_ok==0 && S.attach_kind==8 && S.attach_again==0 && S.attach_taken==-1 && S.attach_untracked==-1 && S.attach_btn==-1,"attach: only our panes, kind 8, a node already holding a control refuses another");
    check(S.vis==0 && S.flags_after_hide==0 && S.vis_game==0 && S.vis_junk==-1,"set_visible works on game objects, refuses non-game memory");
    check(S.created_before_destroy==5,"creation table before destroy: pane, button, label, attached pane2, loose pane3");
    check(S.d_btn==LAYOUT_DESTROY_DEFERRED && S.d_label==LAYOUT_DESTROY_DEFERRED,"controls are hidden and left to their owner");
    check(S.d_game==LAYOUT_DESTROY_REJECTED && S.d_null==LAYOUT_DESTROY_REJECTED,"destroy refuses objects we did not create");
    check(S.d_pane==LAYOUT_DESTROY_DONE && S.order=="ccP" && S.d_pane_again==LAYOUT_DESTROY_REJECTED,"pane destroy: controls before the pane, forgotten afterwards");
    check(S.d_pane2==LAYOUT_DESTROY_DONE && S.d_pane3==LAYOUT_DESTROY_DONE && S.created_after==0,"creation table empty after every created object is destroyed");
    check(live_count==1 && destroyed_events==0,"our own panes never appear as live layouts");
    destroy_game(game);
    check(destroyed_events==1,"the game layout itself is a normal live layout");
    // owner purge: controls cloned into a game layout are forgotten when that layout dies
    fresh_world();
    api.on_layout_built(0,nullptr,cb_owner,nullptr);
    Bytes *owner=make_game_layout(kPlainVt);
    run_86E320(owner);
    check(created_count==1 && created[0].kind==CK_BUTTON && created[0].owner==owner,"control cloned into a game layout is tracked with its owner");
    destroy_game(owner);
    check(created_count==0,"owner destruction purges the controls it owned");
    // ===== live table overflow =====
    fresh_world();
    api.on_layout_built(0,nullptr,cb_hit,&hit_wild);
    memset(&hit_wild,0,sizeof(Hit));
    std::vector<Bytes *> many;
    for(unsigned i=0;i<kLiveMax+8;++i) {many.push_back(light_object());run_86E320(many.back());}
    check(hit_wild.n==kLiveMax+8 && live_count==kLiveMax && table_full==8,"live table full: events still delivered, overflow counted");
    for(Bytes *m:many)destroy_game(m);
    check(destroyed_events==kLiveMax && live_count==0 && *orig_counter==kLiveMax+8,"only tracked layouts raise destroyed events, every original body runs");
    // ===== foreign chain / refusals =====
    fresh_world();
    Bytes *foreign=make_stub(kExt+0x100,reinterpret_cast<void *>(&foreign_build));
    set_call_to(0x86E338,foreign);
    memset(&hit_wild,0,sizeof(Hit));
    api.on_layout_built(0,nullptr,cb_hit,&hit_wild);
    check(state_of(built_handle)==LAYOUT_STATE_CHAINED && static_cast<void *>(next_built)==foreign,"E8 already pointing into another module: chained, next is that function");
    run_86E320(make_game_layout(kPlainVt));
    check(foreign_calls==1 && b0a0_calls==0 && hit_wild.n==1,"chained: foreign function runs first, our event still fires");
    fresh_world();
    image[0x865562]=0x00;
    memset(&hit_wild,0,sizeof(Hit));
    api.on_layout_built(0,nullptr,cb_hit,&hit_wild);
    check(built_handle>0 && destroy_handle==0 && count_events("layout_events_destroy_bytes")==1,"865560 bytes differ (other EXE / patched): destroy hook not registered");
    Bytes *nb=make_game_layout(kPlainVt);run_86E320(nb);
    check(hit_wild.n==1 && live_count==0,"built events work, nothing is tracked without destroy observation");
    fresh_world();
    published=0;   // registered while pending, another mod patches the entry before the manager publishes
    api.on_layout_built(0,nullptr,cb_hit,&hit_wild);
    check(destroy_handle>0 && state_of(destroy_handle)==LAYOUT_STATE_PENDING,"registered but not yet installed");
    image[0x865560]=0xE9;
    publish_hooks();
    check(state_of(destroy_handle)==LAYOUT_STATE_REFUSED && !destroy_observed() && state_of(built_handle)==LAYOUT_STATE_INSTALLED,"entry redirected by another mod before publish: refused, never followed");
    // ===== diagnostic probes =====
    fresh_world();
    fixture_probe=true;
    layout::publish();
    check(observation_registered==1 && state_of(built_handle)==LAYOUT_STATE_INSTALLED && state_of(destroy_handle)==LAYOUT_STATE_INSTALLED,"diagnostic build registers the observation hooks without subscribers");
    // KTID offset candidates: handle dword 6 (offset 0x18) is the only one equal for one name and different across names
    static const char *const names[]={"title_a","title_b"};
    for(int round=0;round<3;++round)for(const char *nm:names) {
        Bytes *o=static_cast<Bytes *>(fake_alloc(0x400));*reinterpret_cast<Bytes **>(o)=image+kPlainVt;
        unsigned h=name_hash31(nm);
        *reinterpret_cast<Bytes **>(o+0x90)=make_handle(h^0x77770000u,h,0);
        api.note_layout_load(o,nm,0);run_86E320(o);
    }
    const Event *cand=last_event("layout_p4_ktid_cand");
    check(cand && cand->a==((1u<<4)|(1u<<6)) && cand->b>=2 && cand->c>=2,"KTID offset candidates are the name-dependent dwords 4 (the name hash) and 6 (offset 0x18)");
    { bool hash_at_16=false;for(auto &e:events)if(e.kind=="layout_p4_ktid_match" && e.a==16 && e.b==1)hash_at_16=true;check(hash_at_16,"the name-hash match identifies dword 4, leaving dword 6 as the KTID"); }
    check(ktid_match_logs<=64,"match lines are bounded");
    check(count_events("layout_p4_res_head")==6 && count_events("layout_p4_res_raw")==48 && count_events("layout_p4_res_inner")==48,"raw handle dumps for the first events");
    check(count_events("layout_p4_ktid_match")>0,"name hash found inside the handle is reported");
    {
        Bytes *o=static_cast<Bytes *>(fake_alloc(0x400));*reinterpret_cast<Bytes **>(o)=image+kPlainVt;
        *reinterpret_cast<Bytes **>(o+0x90)=make_handle(0x1234,name_hash31("cmn_footer_re_cn"),0xD8913AB2u);   // known name->KTID pair sits in dword 3
        api.note_layout_load(o,"cmn_footer_re_cn",0);run_86E320(o);
        bool found=false;for(auto &e:events)if(e.kind=="layout_p4_ktid_match" && e.a==12 && e.b==2)found=true;
        check(found,"known name->KTID pair match reports offset 12");
    }
    // screen dump of the target screens (first two builds per screen only)
    Bytes *speed=make_game_layout(kSpeedVt);
    f_92E430(speed+0x130,3,0,0,1,0);f_92E430(speed+0x130,7,0,0,1,0);
    run_86E320(speed);run_86E320(speed);run_86E320(speed);
    const Event *screen=find_event("layout_p4_screen",1);
    check(screen && screen->b==kNodes && count_events("layout_p4_screen")==2,"speed screen: node count logged, capped at two builds");
    check(find_event("layout_p4_node",(1u<<16)|3) && find_event("layout_p4_node",(1u<<16)|7) && !find_event("layout_p4_node",(1u<<16)|5),"only bound nodes are listed");
    const Event *xy=find_event("layout_p4_node_xy",(1u<<16)|3);
    check(xy && xy->b==30 && xy->c==5 && find_event("layout_p4_node_wh",(1u<<16)|3)->b==100,"node control position and size read back");
    // lifetime test on the lord footer
    size_t order_before=dtor_order.size();
    Bytes *footer=make_game_layout(kFooterVt);
    run_86E320(footer);
    auto life=[&](unsigned stage)->const Event * {for(auto &e:events)if(e.kind=="layout_p4_life" && e.a==stage)return &e;return nullptr;};
    check(life(1) && life(1)->b==1 && life(1)->c==kNodes,"lifetime 1: private footer pane loaded");
    check(life(2) && life(2)->b==1,"lifetime 2: node 14 cloned");
    check(life(3) && life(3)->b==1 && life(3)->c==29,"lifetime 3: moved by 29");
    check(life(4)->b==1 && life(5)->b==1 && life(6)->b==1,"lifetime 4-6: text, click, visibility");
    check(life(7)->b==1 && life(7)->c==1 && life(8)->b==1,"lifetime 7-8: label created with its text node, text set");
    check(life(9) && life(9)->b==0 && life(9)->c==0 && dtor_order.substr(order_before)=="ccP","lifetime 9: pane destroyed (controls first), creation table empty");
    check(created_count==0 && life_done,"nothing of the test is left behind");
    run_86E320(make_game_layout(kFooterVt));
    check(count_events("layout_p4_life")==9,"the lifetime test runs once");
    for(unsigned i=0;i<70;++i)run_86E320(light_object());
    check(count_events("layout_p4_built_cost")>=1 && last_event("layout_p4_built_cost")->a%64==0 && count_events("layout_p4_orig_cost")>=1 && count_events("layout_p4_counts")>=1,"periodic 86E320 cost lines");
    destroy_game(footer);
    check(destroyed_events==1,"footer destroyed event");
    printf("{\"status\":\"layout_p4_fixture_passed\",\"checks\":%u,\"synthetic_image\":true,\"fake_game_model\":true,\"gameplay_verified\":false}\n",checks);
    return 0;
}
