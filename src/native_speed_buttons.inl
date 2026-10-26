#include <climits>
// Two extra progress-speed buttons: >>>> (4x of >>>) and >>>>> (8x of >>>), v12.8.
// Evidence: REA_Analysis/speed_20261018/README.md, research/speed_buttons/.
// - CChangeSpeedLayout (vtable 1473DC8) slot 49 = 882540 builds buttons 2/4/6.
//   After it returns, the same native sequence adds buttons 8/10 into the
//   patched layout nodes 8..11 (ScreenLayout patch; skipped if absent).
// - Clicks reach 891720 only through the std::function tail jump at 898957.
//   Our buttons select native speed 2 (>>>) and remember the multiplier.
// - Day progress 44BCA0 (vtable slot 13D6698) advances one step when
//   counter(+470) % interval == 0 at phase(+484) 0. interval = 2*(fps/30) at >>>,
//   so one day start per frame is only 2x at 30 fps / 4x at 60 fps. A fractional
//   accumulator sets that counter to 0/1, and when the native day already
//   finished inside this frame (idle again at phase 0) the original frame is run
//   once more: at most two day starts per frame (8x needs 60 fps).
//   The saved speed setting stays 0..2. All calls run on the original callers.
namespace speed_buttons {
static const unsigned kSetupSlot=0x1473F50,kSetup=0x882540,kFrameSlot=0x13D6698,kFrame=0x44BCA0,
    kClickJump=0x898957,kClick=0x891720,kInterval=0x1A47C20,kIntervalTable=0x13D4058,kFunctorVtable=0x1476C90;
static const unsigned long long kHoverHash=0x0d966f721c05901full,kSetupHash=0x850f2a47e4909555ull,kClickHash=0x78315913ae6f537aull,kFrameHash=0x6b2f959c87e2df69ull;
static const unsigned char kDoCall[16]={0x48,0x8B,0x12,0x48,0x8B,0x49,0x08,0xE9,0xC4,0x8D,0xFF,0xFF,0xCC,0xCC,0xCC,0xCC};
static const int kIntervals[3]={8,2,1};
// The game reads only the original 698 entries of sprite table FEA5F28C. The
// ScreenLayout patch repoints the grey states of >> and >>> (used only by this
// popup, by index): 207/212 = normal, 208/213 = checked art of >>>> / >>>>>.
// Button state nodes: 3 hover, 5/6 checked (selected speed), 8/9 and 11 disabled.
static const int kNormal[2]={207,212},kChecked[2]={208,213},kGrey[2]={202,203};   // spd1_3/_4
static const unsigned kHoverSlot=0x1474100,kHover=0x88C2E0;           // CChangeSpeed host (vtable 14740F8) slot 1
static Obj extra_buttons[2];                                           // UI thread only
static volatile LONG mode;                   // 0 native, 1 = x4, 2 = x8 (of >>>)
static double accumulator;                   // day-progress thread only
using Setup=unsigned long long(__fastcall *)(Obj,unsigned long long,unsigned long long,unsigned long long);
using Click=void(__fastcall *)(Obj,Obj);
using Frame=void(__fastcall *)(Obj);
static unsigned long long fingerprint(unsigned rva,unsigned size) {
    unsigned long long h=0xcbf29ce484222325ull;
    for(unsigned i=0;i<size;++i)h=(h^image[rva+i])*0x100000001b3ull;
    return h;
}
static bool guards_match() {
    return pointer(image,kSetupSlot)==image+kSetup && pointer(image,kFrameSlot)==image+kFrame && pointer(image,kHoverSlot)==image+kHover &&
        !memcmp(image+0x898950,kDoCall,sizeof(kDoCall)) && !memcmp(image+kIntervalTable,kIntervals,sizeof(kIntervals)) &&
        fingerprint(kSetup,0x4C8)==kSetupHash && fingerprint(kClick,0xBB)==kClickHash && fingerprint(kFrame,0x9C4)==kFrameHash && fingerprint(kHover,0xF6)==kHoverHash;
}
static int native_speed() {return fn<int(__fastcall *)(Obj)>(0x3E9E90)(fn<Obj(__fastcall *)()>(0x3E9C90)());}
static void select(Obj layout,int index) {
    if(Obj group=pointer(layout,0x178))fn<void(__fastcall *)(Obj,int)>(0x5210E0)(group,index);
}
static void add_button(Obj layout,unsigned k) {
    // Mirrors one iteration of 882540 for button id 8/10 (layout nodes 8..11).
    Obj table=layout+0x130;const int id=8+2*static_cast<int>(k);
    if(id>=static_cast<int>(dword(table,0x10)) || fn<Obj(__fastcall *)(Obj,int)>(0x92CEE0)(table,id))return;
    Obj group=pointer(layout,0x178),box=pointer(layout,0x1A0);
    if(!group || !box)return;
    Obj button=fn<Obj(__fastcall *)(size_t)>(0x3F6BC0)(0x1A0);
    if(!button)return;
    button=fn<Obj(__fastcall *)(Obj)>(0x84B2D0)(button);
    reinterpret_cast<void(__fastcall *)(Obj,int,int,Obj,int)>(pointer(pointer(button,0),0x190))(button,0,0,layout,6);
    const int states[6]={3,5,6,8,9,11},sprites[6]={kChecked[k],kChecked[k],kChecked[k],kGrey[0],kGrey[0],kGrey[1]};
    for(int i=0;i<6;++i)fn<void(__fastcall *)(Obj,int,int,Obj,int,int)>(0x92F520)(button+0x130,states[i],sprites[i],button,1,0);
    fn<void(__fastcall *)(Obj,int,Obj,int,int,int)>(0x92E0B0)(table,id,button,8,1,0);
    alignas(16) unsigned char local[0x40]={},handler[0x40]={};
    Obj vtable=image+kFunctorVtable;
    *reinterpret_cast<Obj *>(local)=vtable;*reinterpret_cast<Obj *>(local+8)=layout;*reinterpret_cast<void **>(local+0x38)=local;
    void *impl=reinterpret_cast<void *(__fastcall *)(void *,void *)>(pointer(vtable,0))(local,handler);
    *reinterpret_cast<void **>(handler+0x38)=impl;
    fn<void(__fastcall *)(Obj,int,void *)>(0x867300)(button+0x70,0x7FFFFFFE,handler);
    reinterpret_cast<void(__fastcall *)(void *,bool)>(pointer(vtable,0x20))(local,false);
    fn<void(__fastcall *)(Obj,Obj)>(0x4502D0)(group,button);
    fn<void(__fastcall *)(Obj,Obj)>(0x89AF80)(box,button);
    // Layout image node (9/11) references spd3_0 by name; set its repointed normal image.
    fn<void(__fastcall *)(Obj,int,int,Obj,int,int)>(0x92F520)(table,id+1,kNormal[k],layout,1,0);
    extra_buttons[k]=button;
}
// Native >> and >>> give their grey (disabled) sprites to the new buttons and use
// the grey of > instead.
static void retarget_native(Obj layout) {
    auto set=fn<void(__fastcall *)(Obj,int,int,Obj,int,int)>(0x92F520);
    for(int k=0;k<2;++k) if(Obj b=pointer(layout,0x188+8*k)) {
        set(b+0x130,8,kGrey[0],b,1,0);set(b+0x130,9,kGrey[0],b,1,0);set(b+0x130,11,kGrey[1],b,1,0);
    }
}
static unsigned long long __fastcall setup(Obj layout,unsigned long long a,unsigned long long b,unsigned long long c) { COST_SCOPE("speed_setup");
    unsigned long long result=fn<Setup>(kSetup)(layout,a,b,c);
    if(enabled) {
        extra_buttons[0]=extra_buttons[1]=nullptr;retarget_native(layout);
        add_button(layout,0);add_button(layout,1);
        // 5210E0 ran before our buttons existed: run it again so all five get their
        // enabled/checked state. It re-fires the click of the selected button.
        const LONG m=mode;const int speed=native_speed();
        select(layout,m && speed==2?2+static_cast<int>(m):speed);
    }
    return result;
}
static void __fastcall click(Obj layout,Obj button) { COST_SCOPE("speed_click");
    const int id=static_cast<int>(dword(button,0x80));
    if(enabled && (id==8 || id==10)) {
        InterlockedExchange(&mode,id==8?1:2);
        // The radio group (4502D0, event 100000) already checks this button. Do not
        // call 5210E0 here: it re-fires this click.
        if(Obj fastest=pointer(layout,0x190))fn<Click>(kClick)(layout,fastest);   // native speed 2 (>>>)
        return;
    }
    if(id==2 || id==4 || id==6)InterlockedExchange(&mode,0);
    fn<Click>(kClick)(layout,button);
}
static bool idle(Obj manager) {
    return !dword(manager,0x484) && !dword(manager,0x474) && !dword(manager,0x478) && !dword(manager,0x47C) && !dword(manager,0x480);
}
#ifndef NO_RUNTIME_LOG
// Diagnostics (2026-10-25, user: wait between 전략중 and day 1 with many forces). Every change of the
// day manager's state, read at frame end: day_stage army=new state value=µs in the previous state
// hex=GetTickCount64 low 32 bits (ms; same clock as the function_cost CSV), then day_stage_cpu
// army=44BCA0 calls value=µs spent inside 44BCA0 during that state (CPU-bound vs waiting/not called).
// state = phase(+484) low byte | nonzero +474/+478/+47C/+480 as bits 8..11. Day-progress thread only.
// Switch: [diagnostics] day_stage=1 (기타(개발용) tab), read once at install.
static bool stage_on;
static unsigned stage_last=~0u,stage_frames;
static LONGLONG stage_since,stage_inside,stage_hz;
static unsigned stage_us(LONGLONG ticks) {
    const LONGLONG us=ticks*1000000/stage_hz;return us>0xFFFFFFFFll?0xFFFFFFFFu:static_cast<unsigned>(us);
}
static void stage_note(Obj manager,LONGLONG inside) {
    if(!stage_hz) {LARGE_INTEGER hz;QueryPerformanceFrequency(&hz);stage_hz=hz.QuadPart;}
    const unsigned s=(dword(manager,0x484)&0xFF)|(dword(manager,0x474)?0x100:0)|(dword(manager,0x478)?0x200:0)|
                     (dword(manager,0x47C)?0x400:0)|(dword(manager,0x480)?0x800:0);
    stage_inside+=inside;++stage_frames;
    if(s==stage_last)return;
    LARGE_INTEGER now;QueryPerformanceCounter(&now);
    if(stage_last!=~0u) {
        log_event("day_stage",s,stage_us(now.QuadPart-stage_since),static_cast<unsigned>(GetTickCount64()));
        log_event("day_stage_cpu",stage_frames,stage_us(stage_inside),stage_last);
    }
    stage_last=s;stage_since=now.QuadPart;stage_inside=0;stage_frames=0;
}
#endif
static void run_frame(Obj manager) {
#ifndef NO_RUNTIME_LOG
    if(stage_on && diagnostic_logging) {
        LARGE_INTEGER a,b;QueryPerformanceCounter(&a);fn<Frame>(kFrame)(manager);QueryPerformanceCounter(&b);
        stage_note(manager,b.QuadPart-a.QuadPart);return;
    }
#endif
    fn<Frame>(kFrame)(manager);
}
static void __fastcall frame(Obj manager) { COST_SCOPE("speed_frame");
    const LONG m=mode;bool again=false;
    if(enabled && m && idle(manager) && native_speed()==2) {
        const int interval=*reinterpret_cast<int *>(image+kInterval);
        if(interval>=2) {
            accumulator+=(m==1?4.0:8.0)/interval;
            if(accumulator>2.0)accumulator=2.0;   // never bank more than this frame can use
            if(accumulator>=1.0){accumulator-=1.0;*reinterpret_cast<int *>(manager+0x470)=0;again=accumulator>=1.0;}
            else *reinterpret_cast<int *>(manager+0x470)=1;
        }
    }
    run_frame(manager);
    // Second start only from the same state as a native frame start: the day
    // that just began already completed and the manager is idle at phase 0.
    if(again && enabled && mode==m && idle(manager) && native_speed()==2) {
        accumulator-=1.0;*reinterpret_cast<int *>(manager+0x470)=0;
        run_frame(manager);
    }
}
// 88C2E0 keeps the popup open only while the hovered control is the popup or one
// of its three native buttons (popup+180..190); anything else closes it at once
// (decoded: 88C373..88C3CF). Our two buttons sit at the popup's edge, so a cursor
// a few pixels off them closed the popup. Safety margin (v12.17): the popup also
// stays open while the cursor is within kHoverMargin UI units of the popup or our
// buttons, or for kHoverGraceMs after it last was on a recognised control. In
// those cases the hovered control temporarily stands in the third button slot,
// exactly like our own buttons do. Read-only geometry; no native call added.
static constexpr int kHoverMargin=28;
static constexpr ULONGLONG kHoverGraceMs=300;
static ULONGLONG hover_seen;                                           // UI thread only
static bool absolute_rect(Obj control,int &x,int &y,int &w,int &h) {
    if(!control)return false;
    w=static_cast<int>(dword(control,0x28));h=static_cast<int>(dword(control,0x2C));
    x=0;y=0;Obj c=control;
    for(unsigned depth=0;c && depth<30;++depth) {x+=static_cast<int>(dword(c,0x20));y+=static_cast<int>(dword(c,0x24));c=pointer(c,8);}
    return !c && w>0 && h>0 && w<1920 && h<1080;
}
// Cursor in the native 1920x1080 UI space of the window this UI thread owns.
static bool cursor_ui(int &x,int &y) {
    HWND window=GetActiveWindow();POINT pt;RECT rc;
    if(!window || !GetCursorPos(&pt) || !ScreenToClient(window,&pt) || !GetClientRect(window,&rc))return false;
    double cw=rc.right-rc.left,ch=rc.bottom-rc.top;if(cw<16 || ch<16)return false;
    double scale=(std::min)(cw/1920.0,ch/1080.0),ox=(cw-1920.0*scale)/2,oy=(ch-1080.0*scale)/2;
    x=static_cast<int>((pt.x-ox)/scale);y=static_cast<int>((pt.y-oy)/scale);return true;
}
static bool near_popup(Obj popup) {
    int left=INT_MAX,top=INT_MAX,right=INT_MIN,bottom=INT_MIN,x,y,w,h;
    for(Obj c:{popup,extra_buttons[0],extra_buttons[1]})
        if(absolute_rect(c,x,y,w,h)) {left=(std::min)(left,x);top=(std::min)(top,y);right=(std::max)(right,x+w);bottom=(std::max)(bottom,y+h);}
    int cx,cy;
    return left<right && cursor_ui(cx,cy) && cx>=left-kHoverMargin && cx<right+kHoverMargin && cy>=top-kHoverMargin && cy<bottom+kHoverMargin;
}
static void __fastcall hover(Obj host) { COST_SCOPE("speed_hover");
    Obj popup=enabled?pointer(host,0x180):nullptr;Obj swapped=nullptr,saved=nullptr;bool stand_in=false;
    if(popup && (extra_buttons[0] || extra_buttons[1]) && (dword(popup,0x40)&3)==3) {
        Obj hovered=fn<Obj(__fastcall *)(Obj)>(0x86B3B0)(fn<Obj(__fastcall *)()>(0x10FC0)());
        ULONGLONG now=GetTickCount64();
        bool ours=hovered && (hovered==extra_buttons[0] || hovered==extra_buttons[1]);
        bool native=hovered && (hovered==popup || hovered==pointer(host,0x188) || hovered==pointer(popup,0x180) ||
                                hovered==pointer(popup,0x188) || hovered==pointer(popup,0x190));
        if(ours || native)hover_seen=now;
        stand_in=ours || (!native && (now-hover_seen<kHoverGraceMs || near_popup(popup)));
        if(stand_in) {swapped=hovered;saved=pointer(popup,0x190);*reinterpret_cast<Obj *>(popup+0x190)=hovered;}
    }
    fn<void(__fastcall *)(Obj)>(kHover)(host);
    if(stand_in && pointer(popup,0x190)==swapped)*reinterpret_cast<Obj *>(popup+0x190)=saved;
}
static CodeLink click_link;
static bool installed;
static bool install() {
    if(installed || !guards_match()) {log_event("speed_buttons_guard_rejected");return false;}
#ifndef NO_RUNTIME_LOG
    stage_on=dev_group() && mod_setting(L"diagnostics",L"day_stage",0)!=0;
#endif
    Obj relay=allocate_near(image+kClickJump);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&click));
    DWORD old;
    if(!prepare_link(click_link,kClickJump,0xE9,relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,16)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    if(!publish_link(click_link)) return false;     // relay kept: a UI thread may have observed it
    if(!patch_pointer(kFrameSlot,kFrame,reinterpret_cast<void *>(&frame))) {restore_link(click_link);return false;}
    if(!patch_pointer(kSetupSlot,kSetup,reinterpret_cast<void *>(&setup))) {
        restore_pointer(kFrameSlot,kFrame,reinterpret_cast<void *>(&frame));restore_link(click_link);return false;
    }
    if(!patch_pointer(kHoverSlot,kHover,reinterpret_cast<void *>(&hover))) {
        restore_pointer(kSetupSlot,kSetup,reinterpret_cast<void *>(&setup));
        restore_pointer(kFrameSlot,kFrame,reinterpret_cast<void *>(&frame));restore_link(click_link);return false;
    }
    installed=true;log_event("speed_buttons_installed");return true;
}
}
