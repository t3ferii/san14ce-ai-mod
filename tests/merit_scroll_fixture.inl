/* Real Win32 pump in a second thread. No game process or visible window.
 * Catches the regression that hand-crafted MSG-only tests missed. */
struct ScrollWindowFixture {HANDLE ready,ack;HWND window=nullptr;LONG keys=0,other=0,wheels=0;};
static LRESULT CALLBACK scroll_fixture_proc(HWND window,UINT msg,WPARAM key,LPARAM data) {
    auto *state=reinterpret_cast<ScrollWindowFixture *>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {
        state=static_cast<ScrollWindowFixture *>(reinterpret_cast<CREATESTRUCTW *>(data)->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(state));
    }
    if(state) {
        if(msg==WM_KEYDOWN || msg==WM_KEYUP) {
            if(key==VK_NEXT || key==VK_PRIOR || key==VK_HOME || key==VK_END)InterlockedIncrement(&state->keys);
            else InterlockedIncrement(&state->other);
        }
        if(msg==WM_MOUSEWHEEL)InterlockedIncrement(&state->wheels);
        if(msg==WM_APP+1) {SetEvent(state->ack);return 0;}
    }
    if(msg==WM_CLOSE) {DestroyWindow(window);return 0;}
    if(msg==WM_DESTROY) {PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,msg,key,data);
}
static DWORD WINAPI scroll_fixture_thread(void *parameter) {
    auto &state=*static_cast<ScrollWindowFixture *>(parameter);
    WNDCLASSW cls={};cls.lpfnWndProc=scroll_fixture_proc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"SAN14ScrollPrivateFixture";
    RegisterClassW(&cls);
    state.window=CreateWindowExW(0,cls.lpszClassName,L"",WS_OVERLAPPED,0,0,320,200,nullptr,nullptr,cls.hInstance,&state);
    SetEvent(state.ready);MSG msg={};
    while(GetMessageW(&msg,nullptr,0,0)>0) {TranslateMessage(&msg);DispatchMessageW(&msg);}
    UnregisterClassW(cls.lpszClassName,cls.hInstance);return 0;
}
static void scroll_fixture_send(ScrollWindowFixture &state,UINT msg,WPARAM key,LPARAM data=0) {
    expect(PostMessageW(state.window,msg,key,data)!=0);
    expect(PostMessageW(state.window,WM_APP+1,0,0)!=0);
    expect(WaitForSingleObject(state.ack,2000)==WAIT_OBJECT_0);
}
static void test_scroll_input(Obj profile,Obj front) {
    // Rendering remains on this thread; queued messages belong to the other.
    ScrollWindowFixture state={CreateEventW(nullptr,FALSE,FALSE,nullptr),CreateEventW(nullptr,FALSE,FALSE,nullptr)};
    expect(state.ready && state.ack);DWORD thread=0;HANDLE worker=CreateThread(nullptr,0,scroll_fixture_thread,&state,0,&thread);
    expect(worker && WaitForSingleObject(state.ready,2000)==WAIT_OBJECT_0 && state.window);
    expect(thread!=GetCurrentThreadId());expect(merit_scroll_install(state.window));
    expect(merit_scroll_thread==thread && merit_scroll_window==state.window);
    merit_scroll_fixture_foreground=state.window;
    merit_scroll_set(profile,0,std::wstring(4096,L'가')+L"열전 끝");merit_profile_tick(profile);
    scroll_fixture_send(state,WM_KEYDOWN,VK_NEXT);merit_profile_tick(profile);
    expect(merit_scrolls.back().top==8 && state.keys==0);
    scroll_fixture_send(state,WM_KEYDOWN,VK_NEXT,1LL<<30);
    scroll_fixture_send(state,WM_KEYUP,VK_NEXT);merit_profile_tick(profile);
    expect(merit_scrolls.back().top==8 && state.keys==0);
    scroll_fixture_send(state,WM_KEYDOWN,VK_END);merit_profile_tick(profile);
    expect(profile_label.find(L"열전 끝")!=std::wstring::npos && merit_scrolls.back().top==merit_scroll_last(merit_scrolls.back()));
    scroll_fixture_send(state,WM_KEYDOWN,VK_HOME);merit_profile_tick(profile);expect(merit_scrolls.back().top==0);
    WPARAM wheel=static_cast<WPARAM>(static_cast<unsigned short>(-WHEEL_DELTA))<<16;
    scroll_fixture_send(state,WM_MOUSEWHEEL,wheel);merit_profile_tick(profile);
    expect(merit_scrolls.back().top==3 && state.wheels==0);
    scroll_fixture_send(state,WM_KEYDOWN,'A');expect(state.other==1);
    // Foreground, modal, disabled and stale scopes must leave native keys alone.
    merit_scroll_fixture_foreground=nullptr;scroll_fixture_send(state,WM_KEYDOWN,VK_NEXT);expect(state.keys==1);
    merit_scroll_fixture_foreground=state.window;
    strcpy_s(reinterpret_cast<char *>(front+0x70),128,"CStrategyEmployState");merit_profile_tick(profile);
    scroll_fixture_send(state,WM_KEYDOWN,VK_NEXT);expect(state.keys==2);
    strcpy_s(reinterpret_cast<char *>(front+0x70),128,"CInfoDlgState");merit_profile_tick(profile);
    enabled=0;scroll_fixture_send(state,WM_KEYDOWN,VK_NEXT);expect(state.keys==3);enabled=1;
    AcquireSRWLockExclusive(&merit_scroll_input_lock);merit_scroll_input.updated=GetTickCount()-1000;ReleaseSRWLockExclusive(&merit_scroll_input_lock);
    scroll_fixture_send(state,WM_KEYDOWN,VK_NEXT);expect(state.keys==4);
    merit_profile_tick(profile);scroll_fixture_send(state,WM_KEYDOWN,VK_END);
    // A new person must not inherit pending input from the old biography.
    merit_scroll_set(profile,0,L"새 무장 열전");merit_profile_tick(profile);expect(merit_scrolls.back().top==0);
    // Control directives and surrogate pairs are indivisible row boundaries.
    std::wstring text=std::wstring(37,L'가')+L"^05"+L"\xD83D\xDE00"+L"^00"+std::wstring(700,L'나');
    merit_scroll_set(profile,0,text);
    for(size_t row:merit_scrolls.back().rows) {
        expect(!(row && text[row]>=0xDC00 && text[row]<=0xDFFF));
        expect(!(row && text[row-1]==L'^') && !(row>1 && text[row-2]==L'^'));
    }
    for(size_t top=0;top<=merit_scroll_last(merit_scrolls.back());++top) {
        merit_scrolls.back().top=top;merit_scroll_render(merit_scrolls.back());expect(profile_label.size()<=900);
    }
    merit_scroll_stop();expect(!merit_scroll_hook && !merit_scroll_window);
    scroll_fixture_send(state,WM_KEYDOWN,VK_NEXT);expect(state.keys==5);
    expect(PostMessageW(state.window,WM_CLOSE,0,0)!=0);expect(WaitForSingleObject(worker,2000)==WAIT_OBJECT_0);
    CloseHandle(worker);CloseHandle(state.ready);CloseHandle(state.ack);merit_scroll_fixture_foreground=nullptr;
}
