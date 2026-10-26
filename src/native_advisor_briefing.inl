// Convenience DLL; original UI callbacks only. No save writes or remote calls.
#include "advisor_briefing_profile.h"
namespace advisor_briefing {
static bool guards_match() {
    for(const auto &g:guards) {
        unsigned long long h=0xcbf29ce484222325ull;
        for(unsigned i=0;i<g.size;++i)h=(h^image[g.rva+i])*0x100000001b3ull;
        if(h!=g.hash)return false;
    }
    for(const auto &s:slots)if(pointer(image,s.rva)!=image+s.original)return false;
    return true;
}
#if (defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)) || defined(ADVISOR_BRIEFING_FIXTURE)
using namespace advisor_briefing_core;
static Inbox inbox;
static unsigned page=0;
static bool open=false;
static bool detail=false,rendered_detail=false;
static Obj report_state,simple_state;
static std::vector<Obj> simple_states;
static bool report_deferred;
static unsigned rendered_generation=~0u,rendered_page=~0u;
static bool rendered_open=false;
static bool rendered_collecting=false;
static Obj rendered_adviser;
static briefing_overlay::Model display_cache;
static int started_day=-1;
static Obj last_adviser;
static unsigned last_art=~0u;
static int portrait_serial=0;
struct Legacy {Obj object=nullptr;int x=0,y=0;bool moved=false;};
static Legacy old_log,old_advice;
static Obj manager(){return fn<Obj(__fastcall *)()>(0x110E0)();}
static Obj top_state() {
    Obj m=manager();if(!m)return nullptr;
    const auto count=*reinterpret_cast<const unsigned long long *>(m+0x10);
    Obj states=pointer(m,0x20);return states && count && count<=256?pointer(states,static_cast<unsigned>((count-1)*8)):nullptr;
}
static bool interaction() {
    // 56FE50 checks only the top state. A message can be above diplomacy, so
    // inspect the whole same native stack using its independently checked ABI.
    Obj m=manager();if(!m)return true;
    const auto count=*reinterpret_cast<const unsigned long long *>(m+0x10);
    Obj states=pointer(m,0x20);if(count>256 || (count && !states))return true;
    for(unsigned i=0;i<count;++i) {
        Obj state=pointer(states,i*8);if(!state)return true;
        const char *name=reinterpret_cast<const char *>(state+0x70);size_t size=0;
        for(;size<128 && name[size];++size){}
        if(size==128 || interactive_state(name,size))return true;
    }
    return false;
}
static unsigned force_id(){return ::player_force();}
static bool active(){return enabled && mod_features.advisor_briefing && world() && force_id();}
static bool tutorial() {
    Obj t=fn<Obj(__fastcall *)()>(0x56EE40)();
    Obj context=world()?pointer(world(),0x871E8):nullptr;
    // +454 is the remaining order count, not a tutorial flag (44A0F0).
    return (t && dword(t,0)) || !context;
}
static unsigned day() {
    int current=game_day();return started_day>=0 && current>=started_day?
        static_cast<unsigned>((std::min)(10,current-started_day+1)):0;
}
static bool defer(bool final_report=false){return active() && (inbox.collecting || (final_report && inbox.has_cycle)) && !inbox.fallback && briefing_overlay::available() && !tutorial() && !interaction();}
static bool raw_length(const wchar_t *s,size_t maximum,size_t &length) {
    if(!s)return false;
    __try {for(length=0;length<=maximum;++length)if(!s[length])return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
    return false;
}
static bool copy(const wchar_t *s,std::wstring &out,size_t maximum=Inbox::text_limit) {
    size_t n=0;if(!raw_length(s,maximum,n))return false;out.assign(s,n);return true;
}
static void move(Legacy &l,bool hide) {
    if(!l.object || l.moved==hide)return;
    if(hide){l.x=static_cast<int>(dword(l.object,0x20));l.y=static_cast<int>(dword(l.object,0x24));}
    fn<void(__fastcall *)(Obj,int,int)>(0x874650)(l.object,hide?10000:l.x,hide?10000:l.y);l.moved=hide;
}
static void bind(Legacy &l,Obj p) {
    if(l.object==p)return;
    l={p,static_cast<int>(dword(p,0x20)),static_cast<int>(dword(p,0x24)),false};
}
static bool replace_legacy(){return active() && !inbox.fallback && briefing_overlay::available() && !interaction();}
static void fallback() {
    inbox.fallback=true;open=false;move(old_log,false);move(old_advice,false);
    if(report_state) {
        for(unsigned offset:{0x480u,0x488u,0x490u}) {
            Obj p=pointer(report_state,offset);if(p)fn<void(__fastcall *)(Obj,int,int)>(0x874650)(p,0,0);
        }
    }
    log_event("advisor_briefing_native_fallback",static_cast<unsigned>(inbox.events.size()));
}
static void reset() {
    move(old_log,false);move(old_advice,false);old_log={};old_advice={};
    inbox.reset(0,0);open=false;detail=false;page=0;report_state=simple_state=nullptr;simple_states.clear();report_deferred=false;
    last_adviser=nullptr;last_art=~0u;++portrait_serial;briefing_overlay::forget();
}
static void refresh_progress();
static bool store(std::vector<Event> batch,bool final_report=false) {
    if(!inbox.append(std::move(batch),final_report)){fallback();return false;}
    refresh_progress();return true;
}
static void pop(){fn<void(__fastcall *)(Obj)>(0x12540)(manager());}
static void offscreen(Obj p){if(p)fn<void(__fastcall *)(Obj,int,int)>(0x874650)(p,10000,10000);}
static std::wstring adviser_name() {
    std::wstring name;
    if(last_adviser)copy(fn<const wchar_t *(__fastcall *)(Obj)>(0x22EEC0)(last_adviser),name,100);
    return name.empty()?L"군사":name;
}
static void refresh_progress() {
    // The command HUD can stop updating while days advance. Notifications push
    // their badge snapshot directly from this original UI callback instead.
    if(!inbox.collecting)return;
    briefing_overlay::Model m;m.adviser=adviser_name();m.generation=inbox.generation;
    m.unread=inbox.unread();m.collecting=true;m.title=L"이번 순의 소식을 모으고 있습니다";
    m.body=L"진행이 끝나면 한데 모아 보고드리겠습니다.";
    briefing_overlay::publish(m,true);
}
static void hud(Obj layout,bool eligible,bool modal) {
    if(!active()){briefing_overlay::publish({},false);return;}
    Obj root=world();unsigned force=force_id();
    if(inbox.world && (inbox.world!=reinterpret_cast<std::uintptr_t>(root) || inbox.force!=force))reset();
    inbox.context(reinterpret_cast<std::uintptr_t>(root),force);
    bool player=fn<bool(__fastcall *)(Obj,const char *)>(0x56FE50)(manager(),"CUserStrategyState");
    bool show=!modal && !interaction() && ((eligible && player) || inbox.collecting);
    if(show)unified_overlay::install();
    unsigned generation=0,action=briefing_overlay::take(generation);
    std::vector<Card> cards;
    if(action)cards=inbox.cards();
    if(action && page>=cards.size())page=0;
    if(action && player && !inbox.collecting && generation==inbox.generation) {
        if(action==1)open=!open;
        else if(action==2)open=false;
        else if(action==3 && !cards.empty())page=(page+static_cast<unsigned>(cards.size())-1)%static_cast<unsigned>(cards.size());
        else if(action==4 && !cards.empty())page=(page+1)%static_cast<unsigned>(cards.size());
        else if(action==5 && !cards.empty()){inbox.erase(cards[page].kind);page=0;}
        else if(action==6){inbox.clear();page=0;}
        else if(action==7)detail=!detail;
        if(action==3 || action==4 || action==5 || action==6)detail=false;
        cards=inbox.cards();
        if(open && !cards.empty() && cards[page].unread){inbox.mark(cards[page].kind);cards=inbox.cards();}
    }
    if(inbox.collecting)open=false;
    if(rendered_generation==inbox.generation && rendered_page==page && rendered_open==open && rendered_collecting==inbox.collecting && rendered_adviser==last_adviser && rendered_detail==detail) {
        briefing_overlay::publish(display_cache,show);return;
    }
    cards=inbox.cards();if(page>=cards.size())page=0;
    briefing_overlay::Model m;m.adviser=adviser_name();m.generation=inbox.generation;m.unread=inbox.unread();m.open=open;m.detail=detail;m.collecting=inbox.collecting;m.page=page;m.pages=static_cast<unsigned>(cards.size());
    if(cards.empty()) {
        m.title=inbox.collecting?L"주공, 소식을 모으고 있습니다":L"주공, 새로 전할 소식이 없습니다";
        m.body=inbox.collecting?L"이번 순의 일이 끝나면 한데 모아 보고드리겠습니다.":L"다음 진행부터 천시·재해·성장·인재와 탐색 소식을 이곳에서 확인하실 수 있습니다.";
    } else {
        const auto &card=cards[page];
        m.title=std::wstring(caption(card.kind))+L"  "+std::to_wstring(page+1)+L" / "+std::to_wstring(cards.size());
        const bool complete_gold=std::all_of(card.events.begin(),card.events.end(),[](const Event &e){return e.gold!=0;});
        if(card.kind==Kind::Gold && card.gold && complete_gold)m.body=L"발견한 금 합계 "+std::to_wstring(card.gold)+L"\n\n";
        m.body+=std::to_wstring(card.events.size())+L"건의 소식을 모았습니다.\n\n";
        for(const auto &e:card.events)m.body+=L"• "+(e.day?std::to_wstring(e.day)+L"일차 · ":L"")+(detail?e.text:compact(e))+L"\n\n";
    }
    display_cache=std::move(m);rendered_generation=inbox.generation;rendered_page=page;rendered_open=open;rendered_collecting=inbox.collecting;rendered_adviser=last_adviser;rendered_detail=detail;
    briefing_overlay::publish(display_cache,show);(void)layout;
}
static unsigned long long __fastcall user_enter(Obj state,Obj argument) {
    auto result=fn<unsigned long long(__fastcall *)(Obj,Obj)>(0x44A0F0)(state,argument);
    if(active()){inbox.finish();open=false;detail=false;page=0;}
    return result;
}
static void __fastcall user_leave(Obj state,Obj argument) {
    if(active()) {
        inbox.begin(reinterpret_cast<std::uintptr_t>(world()),force_id());open=false;page=0;
        started_day=game_day();
        refresh_progress();
        log_event("advisor_briefing_cycle_begin",force_id(),inbox.generation);
    }
    fn<void(__fastcall *)(Obj,Obj)>(0x448E40)(state,argument);
}
static void __fastcall game_leave(Obj state,Obj argument) {reset();fn<void(__fastcall *)(Obj,Obj)>(0x448A70)(state,argument);}
static void __fastcall narration(Obj state) {
    if(defer() && top_state()==state) {
        Event e;e.speaker=dword(state,0x4B8);e.day=day();
        if(copy(reinterpret_cast<const wchar_t *>(pointer(state,0x4B0)),e.text) && passive_notice(e.text) && store({e})) {
            // Same native state pop as 862E40 after its passive InputWait loop.
            // Leave 839CA0 still runs the parent completion functor at +48.
            pop();return;
        }
    }
    fn<void(__fastcall *)(Obj)>(0x862E40)(state);
}
static unsigned long long __fastcall simple_enter(Obj state) {
    bool accepted=false;
    if(defer()) {
        std::vector<Event> batch;Obj head=pointer(state,0x478),node=head?pointer(head,0):nullptr;
        size_t count=static_cast<size_t>(*reinterpret_cast<unsigned long long *>(state+0x480));
        bool ok=head && count>0 && count<=256;
        for(size_t i=0;ok && i<count;++i) {
            if(!node || node==head){ok=false;break;}
            Event e;e.day=day();ok=copy(reinterpret_cast<const wchar_t *>(pointer(node,0x10)),e.text) && passive_notice(e.text);
            if(ok)batch.push_back(std::move(e));node=pointer(node,0);
        }
        accepted=ok && node==head && store(std::move(batch));
    }
    auto result=fn<unsigned long long(__fastcall *)(Obj)>(0x1F15F0)(state);
    if(result && accepted) {
        // Create the native dialog and its completion binding first; close from
        // its first active update, never reenter the manager during Enter.
        simple_state=state;simple_states.push_back(state);offscreen(pointer(state,0x470));
    }
    return result;
}
static void __fastcall simple_frame(Obj state) {
    auto it=std::find(simple_states.begin(),simple_states.end(),state);
    if(it!=simple_states.end()) {
        // A completion callback may have stacked a new interaction. Never pop
        // whichever state happens to be on top in place of our own message.
        if(top_state()!=state)return;
        Obj dialog=pointer(state,0x470);
        if(!defer()) {
            simple_states.erase(it);simple_state=nullptr;
            if(dialog)fn<void(__fastcall *)(Obj,int,int)>(0x874650)(dialog,0,0);return;
        }
        // Native +190 waits for the opening animation, sets the result and
        // starts closing the child layout (8614A2 -> 856840). Its bound event
        // then pops this state. Do not destroy it with a direct manager pop
        // while the original UI still owns animation/completion callbacks.
        if(!dialog || dword(dialog,0x178)!=0xFFFFFFFF){simple_states.erase(it);simple_state=nullptr;return;}
        Obj animation=pointer(dialog,0x188);
        if(!animation || dword(animation,0x178) || dword(animation,0x17C))return;
        simple_states.erase(it);simple_state=nullptr;
        // This can synchronously deliver the close callback. No dialog/state
        // access is allowed after the native acknowledgement returns.
        fn<void(__fastcall *)(Obj,unsigned)>(0x861480)(dialog,0x7FFFFFFD);
        return;
    }
    // Original slot is 92AD0 (void no-op). Original close event and Leave
    // 4F7170 own disposal and +48; YesNo/choices/input use other slots.
}
static bool record(Obj r,std::vector<Event> &batch) {
    Event e;e.speaker=word(r,0x88);e.day=day();
    if(!copy(reinterpret_cast<const wchar_t *>(r+0x8A),e.text,0x55))return false;
    if(e.text.empty() && !copy(reinterpret_cast<const wchar_t *>(r+0x14),e.text,0x37))return false;
    if(!e.text.empty())batch.push_back(std::move(e));return true;
}
static bool pending_world_reports(std::vector<Event> &batch) {
    Obj root=world();Obj context=root?pointer(root,0x871E8):nullptr;if(!context)return false;
    Obj district=fn<Obj(__fastcall *)(Obj)>(0x32E8D0)(context);
    unsigned district_id=district?fn<unsigned(__fastcall *)(Obj)>(0x22DBC0)(district):0;
    for(unsigned i=0;i<2001;++i) {
        Obj r=pointer(root,0x79E90+i*8);if(!r)continue;
        unsigned flags=r[0x82],owner_id=r[0x12];if((flags&4) || !(flags&2))continue;
        Obj d=owner_id>0 && owner_id<52?pointer(root,0xE0D0+owner_id*8):nullptr;
        bool known=(flags&1) || (d && fn<int(__fastcall *)(Obj)>(0x233750)(d));
        bool include_detail=(flags&1) || owner_id==district_id;
        if(!known && !include_detail)continue;
        Event e;e.speaker=word(r,0x88);e.day=day();std::wstring first,second;
        if(known && !copy(reinterpret_cast<const wchar_t *>(r+0x14),first,0x37))return false;
        if(include_detail && r[0x86] && !copy(reinterpret_cast<const wchar_t *>(r+0x8A),second,0x55))return false;
        e.text=first;if(!second.empty() && second!=first){if(!e.text.empty())e.text+=L"\n";e.text+=second;}
        if(!e.text.empty())batch.push_back(std::move(e));
    }
    return true;
}
struct Text {Obj root;int id,x,y;std::wstring value;};
struct TextCapture {std::vector<Text> text;bool valid=true;};
static thread_local TextCapture *capturing;
struct CaptureScope {
    TextCapture *previous;
    explicit CaptureScope(TextCapture &next):previous(capturing){capturing=&next;}
    ~CaptureScope(){capturing=previous;}
};
using TextSetter=void(__fastcall *)(Obj,int,const wchar_t *,unsigned long long);
static TextSetter original_text;
static CodeLink text_link;
static void __fastcall set_text(Obj root,int id,const wchar_t *text,unsigned long long flags) {
    if(capturing && capturing->valid) {
        std::wstring value;
        if(!copy(text,value) || capturing->text.size()>=2048)capturing->valid=false;
        else if(!value.empty()) {
            auto &v=capturing->text;auto it=std::find_if(v.begin(),v.end(),[&](const Text &t){return t.root==root && t.id==id;});
            if(it!=v.end())it->value=std::move(value);
            else {int pos[2]{};fn<void(__fastcall *)(Obj,int *,int)>(0x92CB10)(root,pos,id);v.push_back({root,id,pos[0],pos[1],std::move(value)});}
        }
    }
    original_text(root,id,text,flags);
}
static unsigned long long __fastcall report_enter(Obj state) {
    // Native reports are pushed after CUserStrategyState has already entered.
    // Accept only this final native report into the completed cycle.
    std::vector<Event> batch;bool capture=defer(true) && pending_world_reports(batch);
    auto result=fn<unsigned long long(__fastcall *)(Obj)>(0x67B420)(state);
    report_state=state;report_deferred=false;
    if(!capture || !result)return result;
    Obj action=pointer(state,0x488);unsigned n=action?dword(action,0x16280):262;
    bool ok=n<=261;
    for(unsigned i=0;ok && i<n;++i)ok=record(action+0x1B00+i*0x140,batch);
    if(ok && dword(action,0x16284))ok=record(action+0x16140,batch);
    if(!ok){fallback();return result;}
    // A transaction: ordinary result generation/flag updates ran once above.
    if(!store(std::move(batch),true))return result;
    offscreen(pointer(state,0x480));offscreen(action);report_deferred=true;return result;
}
static void __fastcall report_frame(Obj state) {
    if(state==report_state && report_deferred) {
        if(top_state()!=state)return;
        if(!defer(true)){report_deferred=false;fallback();fn<void(__fastcall *)(Obj)>(0x67B600)(state);return;}
        report_deferred=false;
        TextCapture capture;
        // Preserve creation and disposal of the original income controls too.
        {CaptureScope scope(capture);fn<void(__fastcall *)(Obj,int)>(0x67C990)(state,1);}
        Obj income=pointer(state,0x490);std::vector<Event> batch;
        // Income values are filled by the district-selection callback, after
        // the constructor has created the headings. Run that same display-only
        // formatter for every district actually listed by the original UI.
        bool ok=income && capture.valid;
        for(unsigned i=0;ok && i<8;++i) {
            Obj district=pointer(income,0x288+i*8);if(!district)continue;
            if(district[0x11]>=sizeof(district_labels)/sizeof(*district_labels)){ok=false;break;}
            TextCapture values=capture;
            {CaptureScope scope(values);fn<void(__fastcall *)(Obj,Obj)>(0x67D1F0)(income,district);}
            Obj root=pointer(income,0x378);if(!root){ok=false;break;}
            values.text.erase(std::remove_if(values.text.begin(),values.text.end(),[&](const Text &t){return t.root!=root+0x130;}),values.text.end());
            bool number=false;for(const auto &t:values.text)for(wchar_t c:t.value)if(c>=L'0' && c<=L'9')number=true;
            if(!values.valid || !number){ok=false;break;}
            std::stable_sort(values.text.begin(),values.text.end(),[](const Text &a,const Text &b){return a.y!=b.y?a.y<b.y:a.x<b.x;});
            Event e;e.kind=Kind::Income;e.day=day();
            if(!copy(fn<const wchar_t *(__fastcall *)(unsigned)>(0x30CF80)(district_labels[district[0x11]]),e.text,100)){ok=false;break;}
            e.text+=L"\n";int row=values.text.empty()?0:values.text.front().y;
            for(const auto &t:values.text) {
                if(e.text.size()+t.value.size()+2>Inbox::text_limit){ok=false;break;}
                if(t.y!=row){e.text+=L"\n";row=t.y;}
                e.text+=t.value+L"  ";
            }
            if(ok)batch.push_back(std::move(e));
        }
        if(!ok || batch.empty()) {
            // Unknown income encoding: retain the original report screen.
            fallback();fn<void(__fastcall *)(Obj,int,int)>(0x874650)(pointer(state,0x480),0,0);return;
        }
        if(!store(std::move(batch),true))return;
        offscreen(pointer(state,0x490));
        fn<void(__fastcall *)(Obj,int)>(0x67C990)(state,2); // original camera restoration
        fn<void(__fastcall *)(Obj,int)>(0x67C990)(state,3); // original report completion phase
        *reinterpret_cast<unsigned *>(state+0x474)=3; // no visible bg_out delay
        // Native 67B600 phase 3 applies its own final audio/camera cleanup and
        // manager pop. Leave 67B1E0 still disposes all controls and calls +48.
    }
    fn<void(__fastcall *)(Obj)>(0x67B600)(state);
}
static void __fastcall report_leave(Obj state,Obj context) {
    if(state==report_state){report_state=nullptr;report_deferred=false;}
    fn<void(__fastcall *)(Obj,Obj)>(0x67B1E0)(state,context);
}
static void __fastcall report_wait(Obj state) {if(defer(true) && top_state()==state){*reinterpret_cast<unsigned *>(state+0x470)=0;pop();return;}fn<void(__fastcall *)(Obj)>(0x67B7C0)(state);}
static void __fastcall log_frame(Obj layout) {
    bind(old_log,layout);move(old_log,false);fn<void(__fastcall *)(Obj)>(0x66F7C0)(layout);move(old_log,replace_legacy());
}
static void __fastcall log_leave(Obj layout,Obj context) {
    if(old_log.object==layout){move(old_log,false);old_log={};}
    fn<void(__fastcall *)(Obj,Obj)>(0x66F3E0)(layout,context);
}
static void __fastcall advice_frame(Obj layout) {
    bind(old_advice,layout);move(old_advice,false);fn<void(__fastcall *)(Obj)>(0x88C1C0)(layout);
    if(!active())return;
    Obj adviser=pointer(layout,0x448);unsigned art=adviser?word(adviser,0xA6):~0u;
    if(adviser!=last_adviser || art!=last_art){last_adviser=adviser;last_art=art;++portrait_serial;}
    if(!briefing_overlay::captured(portrait_serial) && adviser && fn<unsigned(__fastcall *)(Obj)>(0x86F250)(layout)) {
        int pos[2]{},size[2]{};fn<void(__fastcall *)(Obj,int *,int)>(0x92CB10)(layout+0x130,pos,5);fn<void(__fastcall *)(Obj,int *,int)>(0x92CF90)(layout+0x130,size,5);
        // The call above refreshes the node's native matrix. Use its complete
        // world scale, including the parent's transform, rather than texel size.
        Obj nodes=pointer(layout,0x138),node=nodes && dword(layout,0x140)>5?pointer(nodes,5*8):nullptr;
        Obj matrix=node?pointer(node,pointer(node,0xD0)?0xE0:0xE8):nullptr;
        Rect bounds;float transform[16];
        if(matrix){memcpy(transform,matrix,sizeof(transform));if(portrait_bounds(size[0],size[1],node[0x10C],transform,bounds))briefing_overlay::request_portrait(bounds,portrait_serial);}
    } else move(old_advice,replace_legacy());
}
static void __fastcall advice_leave(Obj layout,Obj context) {
    if(old_advice.object==layout){move(old_advice,false);old_advice={};last_adviser=nullptr;last_art=~0u;}
    fn<void(__fastcall *)(Obj,Obj)>(0x893720)(layout,context);
}
static void *replacements[]={reinterpret_cast<void *>(&user_enter),reinterpret_cast<void *>(&user_leave),reinterpret_cast<void *>(&game_leave),reinterpret_cast<void *>(&narration),reinterpret_cast<void *>(&simple_enter),reinterpret_cast<void *>(&simple_frame),reinterpret_cast<void *>(&report_enter),reinterpret_cast<void *>(&report_frame),reinterpret_cast<void *>(&report_leave),reinterpret_cast<void *>(&report_wait),reinterpret_cast<void *>(&log_frame),reinterpret_cast<void *>(&log_leave),reinterpret_cast<void *>(&advice_frame),reinterpret_cast<void *>(&advice_leave)};
static bool install() {
    if(!guards_match())return false;
    Obj relay=allocate_near(image+0x930D20);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&set_text));memcpy(relay+32,image+0x930D20,5);absolute_jump(relay+37,image+0x930D25);
    original_text=reinterpret_cast<TextSetter>(relay+32);DWORD old;
    if(!prepare_link(text_link,0x930D20,0xE9,relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,64)){VirtualFree(relay,0,MEM_RELEASE);return false;}
    unsigned n=0;
    if(publish_link(text_link))for(;n<sizeof(slots)/sizeof(*slots);++n)if(!patch_pointer(slots[n].rva,slots[n].original,replacements[n]))break;
    if(n!=sizeof(slots)/sizeof(*slots)) {
        while(n){--n;restore_pointer(slots[n].rva,slots[n].original,replacements[n]);}restore_link(text_link);return false;
    }
    log_event("advisor_briefing_installed");return true;
}
#endif
}
static bool advisor_briefing_guards_match(){return advisor_briefing::guards_match();}
