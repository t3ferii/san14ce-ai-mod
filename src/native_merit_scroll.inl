/* The window/message thread differs from the profile UI caller. Exchange
 * scalar input only; native UI calls and text vectors stay on the UI caller. */
struct MeritScroll {
    Obj profile;unsigned key;std::wstring text;std::vector<size_t> rows;
    size_t top=0;DWORD updated=0;bool visible=false;
};
static std::vector<MeritScroll> merit_scrolls;
static HHOOK merit_scroll_hook;
static DWORD merit_scroll_thread;
static HWND merit_scroll_window;
static DWORD merit_scroll_retry;
static size_t merit_scroll_active;
static constexpr size_t merit_scroll_page_rows=9;
struct MeritScrollInput {
    Obj owner=nullptr;HWND window=nullptr;DWORD updated=0;
    LONG rows=0;int edge=0;bool visible=false;
};
static SRWLOCK merit_scroll_input_lock=SRWLOCK_INIT;
static MeritScrollInput merit_scroll_input;
#ifdef TACTICS_FIXTURE
static HWND merit_scroll_fixture_foreground;
#endif
static HWND merit_scroll_foreground() {
#ifdef TACTICS_FIXTURE
    return merit_scroll_fixture_foreground;
#else
    return GetAncestor(GetForegroundWindow(),GA_ROOT);
#endif
}
static bool merit_scroll_route(MSG &msg,DWORD now,HWND foreground) {
    // WINDOW thread: no profile dereference, vectors or native game calls.
    bool consume=false;
    AcquireSRWLockExclusive(&merit_scroll_input_lock);
    MeritScrollInput &in=merit_scroll_input;
    if(enabled && in.visible && in.owner && in.window==msg.hwnd && foreground==in.window && now-in.updated<250) {
        LONG rows=0;int edge=0;
        if(msg.message==WM_MOUSEWHEEL) {
            rows=-GET_WHEEL_DELTA_WPARAM(msg.wParam)/WHEEL_DELTA*3;
            consume=GET_WHEEL_DELTA_WPARAM(msg.wParam)!=0;
        }
        if(msg.message==WM_KEYDOWN || msg.message==WM_KEYUP) {
            if(msg.wParam==VK_NEXT) {rows=8;consume=true;}
            if(msg.wParam==VK_PRIOR) {rows=-8;consume=true;}
            if(msg.wParam==VK_HOME) {edge=-1;consume=true;}
            if(msg.wParam==VK_END) {edge=1;consume=true;}
            // Releases/repeats must not leak into the native officer list.
            if(msg.message==WM_KEYUP || (msg.lParam&(1LL<<30))) {rows=0;edge=0;}
        }
        if(edge) {in.edge=edge;in.rows=0;}
        if(rows)in.rows=static_cast<LONG>((std::max)(int64_t(-32768),(std::min)(int64_t(32768),int64_t(in.rows)+rows)));
        if(consume)msg.message=WM_NULL;
    }
    ReleaseSRWLockExclusive(&merit_scroll_input_lock);
    return consume;
}
static LRESULT CALLBACK merit_scroll_message(int code,WPARAM removed,LPARAM parameter) {
    if(code>=0 && removed==PM_REMOVE && parameter)
        merit_scroll_route(*reinterpret_cast<MSG *>(parameter),GetTickCount(),merit_scroll_foreground());
    return CallNextHookEx(merit_scroll_hook,code,removed,parameter);
}
static void merit_scroll_reset_input() {
    AcquireSRWLockExclusive(&merit_scroll_input_lock);
    merit_scroll_input={};
    ReleaseSRWLockExclusive(&merit_scroll_input_lock);
}
static bool merit_scroll_install(HWND window) {
    DWORD process=0,thread=GetWindowThreadProcessId(window,&process);
    if(!thread || process!=GetCurrentProcessId())return false;
    if(merit_scroll_hook && merit_scroll_window==window && merit_scroll_thread==thread)return true;
    merit_scroll_reset_input();
    if(merit_scroll_hook)UnhookWindowsHookEx(merit_scroll_hook);
    merit_scroll_hook=nullptr;merit_scroll_window=nullptr;merit_scroll_thread=0;
    HMODULE module=self_module;
#ifdef TACTICS_FIXTURE
    module=GetModuleHandleW(nullptr);
#endif
    if(!module)return false;
    // The hook executes in another thread of THIS process; provide its module.
    HHOOK hook=SetWindowsHookExW(WH_GETMESSAGE,merit_scroll_message,module,thread);
    if(!hook)return false;
    merit_scroll_hook=hook;merit_scroll_window=window;merit_scroll_thread=thread;
    log_event("biography_scroll_window_input",thread);
    return true;
}
struct MeritScrollWindow {HWND window=nullptr;int64_t area=0;};
static BOOL CALLBACK merit_scroll_find_window(HWND window,LPARAM parameter) {
    DWORD process=0;GetWindowThreadProcessId(window,&process);RECT rect={};
    if(process!=GetCurrentProcessId() || !IsWindowVisible(window) || !GetClientRect(window,&rect))return TRUE;
    int64_t area=int64_t(rect.right-rect.left)*(rect.bottom-rect.top);
    auto &candidate=*reinterpret_cast<MeritScrollWindow *>(parameter);
    if(area>candidate.area) {candidate.window=window;candidate.area=area;}
    return TRUE;
}
static void merit_scroll_ensure_window() {
#ifndef TACTICS_FIXTURE
    if(merit_scroll_hook && IsWindow(merit_scroll_window))return;
    DWORD now=GetTickCount();if(now-merit_scroll_retry<1000)return;
    merit_scroll_retry=now;MeritScrollWindow candidate;
    EnumWindows(merit_scroll_find_window,reinterpret_cast<LPARAM>(&candidate));
    if(!candidate.window || !merit_scroll_install(candidate.window))log_event("biography_scroll_input_unavailable");
#endif
}
static void merit_scroll_stop() {
    merit_scroll_reset_input();
    if(merit_scroll_hook)UnhookWindowsHookEx(merit_scroll_hook);
    merit_scroll_hook=nullptr;merit_scroll_window=nullptr;merit_scroll_thread=0;
}
static bool merit_scroll_front() {
    // A different modal over the biography must retain its own keys.
    Obj manager=image+0x1B620B0;
    size_t count=*reinterpret_cast<size_t *>(manager+0x10);Obj states=pointer(manager,0x20);
    if(!count || count>4096 || !states)return false;
    Obj state=pointer(states,static_cast<unsigned>((count-1)*8));
    return state && strncmp(reinterpret_cast<const char *>(state+0x70),"CInfoDlgState",14)==0;
}
static bool merit_scroll_control(const std::wstring &text,size_t at) {
    auto hex=[](wchar_t c){return (c>=L'0' && c<=L'9') || (c>=L'A' && c<=L'F') || (c>=L'a' && c<=L'f');};
    return at+2<text.size() && text[at]==L'^' && hex(text[at+1]) && hex(text[at+2]);
}
static size_t merit_scroll_last(const MeritScroll &s) {
    return s.rows.size()>merit_scroll_page_rows?s.rows.size()-merit_scroll_page_rows:0;
}
static void merit_scroll_render(MeritScroll &s) {
    size_t start=s.rows[s.top],last=(std::min)(s.rows.size(),s.top+merit_scroll_page_rows);
    size_t end=last<s.rows.size()?s.rows[last]:s.text.size();
    std::wstring heading=L"[열전 "+std::to_wstring(s.top+1)+L"/"+std::to_wstring(merit_scroll_last(s)+1)+L" · 휠/PgUp·PgDn]\n";
    end=(std::min)(end,start+900-heading.size()-3);
    if(end<s.text.size() && end && s.text[end-1]>=0xD800 && s.text[end-1]<=0xDBFF)--end;
    if(end>start && s.text[end-1]==L'^')--end;
    else if(end>start+1 && s.text[end-2]==L'^')end-=2;
    std::wstring colour;
    for(size_t i=0;i<start;++i)if(merit_scroll_control(s.text,i)) {colour=s.text.substr(i,3);i+=2;}
    std::wstring page=heading+colour+s.text.substr(start,end-start);
    fn<void(__fastcall *)(Obj,unsigned,const wchar_t *)>(0x930D20)(s.profile+0x130,s.key,page.c_str());
}
static void merit_scroll_set(Obj profile,unsigned key,const std::wstring &text) {
    merit_scroll_reset_input();
    auto at=std::find_if(merit_scrolls.begin(),merit_scrolls.end(),[profile](const MeritScroll &s){return s.profile==profile;});
    if(at==merit_scrolls.end()) {
        if(merit_scrolls.size()>=16)merit_scrolls.clear();
        merit_scrolls.push_back({});at=merit_scrolls.end()-1;
    }
    MeritScroll &s=*at;s.profile=profile;s.key=key;s.text=text;s.top=0;s.rows={0};s.visible=false;
    unsigned width=0;
    for(size_t i=0;i<text.size();++i) {
        if(merit_scroll_control(text,i)) {i+=2;continue;}
        if(text[i]==L'\n') {if(i+1<text.size())s.rows.push_back(i+1);width=0;continue;}
        width+=text[i]<128?1u:2u;
        if(width>=76 && i+1<text.size() && !(text[i]>=0xD800 && text[i]<=0xDBFF)) {s.rows.push_back(i+1);width=0;}
    }
    merit_scroll_active=static_cast<size_t>(at-merit_scrolls.begin());
    merit_scroll_render(s);merit_scroll_ensure_window();
}
static void __fastcall merit_profile_tick(Obj profile) {
    fn<void(__fastcall *)(Obj)>(0x878810)(profile);
    for(size_t i=0;i<merit_scrolls.size();++i) {
        MeritScroll &s=merit_scrolls[i];if(s.profile!=profile)continue;
        s.visible=enabled && fn<unsigned(__fastcall *)(Obj)>(0x86F250)(profile)!=0 && merit_scroll_front();
        s.updated=GetTickCount();LONG rows=0;int edge=0;
        if(s.visible)merit_scroll_ensure_window();
        AcquireSRWLockExclusive(&merit_scroll_input_lock);
        if(merit_scroll_input.owner==profile) {
            if(s.visible) {rows=merit_scroll_input.rows;edge=merit_scroll_input.edge;}
            merit_scroll_input={};
        }
        if(s.visible)merit_scroll_input={profile,merit_scroll_window,s.updated,0,0,true};
        ReleaseSRWLockExclusive(&merit_scroll_input_lock);
        if(!s.visible)return;
        merit_scroll_active=i;
        if(rows || edge) {
            int64_t base=edge<0?0:edge>0?int64_t(merit_scroll_last(s)):int64_t(s.top);
            size_t top=static_cast<size_t>((std::max)(int64_t(0),(std::min)(int64_t(merit_scroll_last(s)),base+rows)));
            if(top!=s.top) {s.top=top;merit_scroll_render(s);log_event("biography_scrolled",static_cast<unsigned>(top));}
        }
        return;
    }
}
