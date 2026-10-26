// Read-only world-map portraits. Inherit the native region-label transform;
// never construct CDomesticAreaBtnBase or a domestic command/callback.
// Current-build evidence: research/area_officer_overlay/README.md.
#include <unordered_map>
#include <array>
#include <string>
#include <climits>
namespace region_officers {
static void clear_trait_cache();
// Trait text = 4 black copies offset one pixel diagonally (outline), then the
// white text on top. CCmnText has no verified edge setter; copies are created
// once per label and only re-texted when the officer's traits change.
constexpr unsigned kTraitLayers=5,kTraitMain=kTraitLayers-1;
constexpr int kTraitFont=18; // native CCmnText default (86CEC0); was 14
struct Panel {
    Obj face=nullptr,parent=nullptr,text=nullptr;
    Obj outline[kTraitMain]={};
    bool outline_visible=true; // native controls start visible
    unsigned text_revision=0;
    int text_x=0,text_y=0;
    bool text_visible=false,text_attempted=false,text_begun=false,text_wanted=false;
    unsigned person=~0u,art=~0u,force=0;
    int width=0,height=0,x=0,y=0;
    bool visible=false,attempted=false,begun=false;
};
// Explicit UI-thread teardown only; DLL/CRT detach must not call the game.
static auto &panels=*new std::unordered_map<Obj,Panel>;
static volatile LONG installed;
static ULONGLONG last_creation;
// One expensive native UI operation (face/text resource or portrait texture)
// per native frame. The spender's next callback marks one full label pass;
// a timeout recovers if that label is no longer framed.
static Obj heavy_layout;
static ULONGLONG heavy_time;
static bool heavy_spent;
// v12.19: a native portrait load (1B91E0) is synchronous and measured 5.7 ms on
// average, up to 31 ms (data/performance cost log, 162 loads). It cannot be made
// cheaper, so heavy steps are spaced: after each one, the next waits twice its
// measured duration. A 25 ms load is followed by ~50 ms of normal frames instead
// of another load in the very next frame.
static LONGLONG heavy_resume;
static LONGLONG qpc_now() {LARGE_INTEGER q;QueryPerformanceCounter(&q);return q.QuadPart;}
static LONGLONG (*heavy_clock)()=&qpc_now; // fixture replaces it with a manual clock
struct HeavyTimer {
    LONGLONG begin;
    HeavyTimer():begin(heavy_clock()) {}
    ~HeavyTimer() {LONGLONG end=heavy_clock();heavy_resume=end+2*(end-begin);}
};
static bool heavy_budget(Obj layout) {
    if(heavy_clock()<heavy_resume)return false;
    // Spent budgets also stop a second heavy step inside the same callback.
    if(heavy_spent && GetTickCount64()-heavy_time<100)return false;
    heavy_spent=true;heavy_layout=layout;heavy_time=GetTickCount64();
    return true;
}
using Frame=void(__fastcall *)(Obj);
// CCapitalInfoLayout (region label) +1F0 = Area. CFootholdInfoLayout (city label)
// +1F0 holds the base id at +4E; native 22D500/22A7B0 resolve it as building
// (world+6DA98) +10 centre hex -> tile +12 area (world+6CAF0). Cities only (<0x34),
// matching 1ACC40's city/gate split. The city's own area has its own officer.
constexpr unsigned kFootholdVtable=0x13999B8;
static Obj label_area(Obj root,Obj layout,bool city) {
    Obj data=pointer(layout,0x1F0);
    if(!city || !data)return data;
    unsigned base=word(data,0x4E);
    if(!base || base>=0x34)return nullptr;
    Obj building=pointer(root,0x6DA98+base*8);
    unsigned hex=building?word(building,0x10):0xFFFF;
    Obj tile=hex<48400?pointer(root,0xE270+hex*8):nullptr;
    unsigned id=tile?word(tile,0x12):0;
    return id && id<=500?pointer(root,0x6CAF0+id*8):nullptr;
}
using Leave=void(__fastcall *)(Obj,Obj);
using Destroy=Obj(__fastcall *)(Obj,unsigned);
static void show_control(Obj control,bool &visible,bool value) {
    if(control && visible!=value){fn<void(__fastcall *)(Obj,int)>(0x8741D0)(control,value?1:0);visible=value;}
}
static void show_outline(Panel &p,bool value);
static void show(Panel &p,bool value) {
    show_control(p.face,p.visible,value);
    show_outline(p,value && p.text_wanted);
    show_control(p.text,p.text_visible,value && p.text_wanted);
}
static void release(Obj layout,Obj context) {
    if(layout==heavy_layout){heavy_layout=nullptr;heavy_spent=false;}
    auto it=panels.find(layout);if(it==panels.end())return;
    Obj face=it->second.face,text=it->second.text;
    Obj outline[kTraitMain];memcpy(outline,it->second.outline,sizeof(outline));
    bool begun=it->second.begun,text_begun=it->second.text_begun;panels.erase(it);
    if(text) {
        if(text_begun)fn<Frame>(0x869850)(text);
        fn<Leave>(0x877DB0)(text,context);fn<Destroy>(0x865F30)(text,1);
    }
    for(Obj copy:outline)if(copy) {
        if(text_begun)fn<Frame>(0x869850)(copy);
        fn<Leave>(0x877DB0)(copy,context);fn<Destroy>(0x865F30)(copy,1);
    }
    if(panels.empty())clear_trait_cache();
    if(face) {
        if(begun)fn<Frame>(0x869820)(face);
        fn<Leave>(0x8782E0)(face,context);
        fn<Destroy>(0x198BE0)(face,1);
    }
}
struct ViewCache {
    Obj root=nullptr,camera=nullptr,popup=nullptr,controller=nullptr;
    unsigned long long distance=0;
    unsigned minimum=0;
    float zoom=0;
    bool ready=false,zoom_ready=false;
};
static ViewCache view_cache;
static bool native_visible(Obj layout) {
    // 86F250 visibility semantics, read-only: follow actual parent flags.
    for(unsigned depth=0;layout && depth<30;++depth) {
        if((dword(layout,0x40)&3)!=3)return false;
        layout=pointer(layout,8);
    }
    return !layout;
}
static bool world_view(Obj &root,unsigned &force) {
    root=world();force=0;
    if(!enabled || !mod_features.region_officers || !root)return false;
    Obj context=pointer(root,0x871E8);
    if(!context || !(force=context[0x3A]) || force>51)return false;
    unsigned mode=dword(image,0x21431FC);
    if(mode>2 && mode!=8)return false;
    auto &v=view_cache;
    if(v.root!=root || !v.ready) {
        v=ViewCache{};v.root=root;
        v.camera=fn<Obj(__fastcall *)()>(0x11170)();
        v.popup=fn<Obj(__fastcall *)()>(0x110E0)();
        v.ready=v.camera!=nullptr;
    }
    if(!v.ready)return false;
    Obj controller=pointer(v.camera,0x280);
    unsigned long long distance=0;
    unsigned minimum=controller?dword(controller,0x24):0;
    memcpy(&distance,controller?controller+0x90:v.camera+0x230,sizeof(distance));
    if(!v.zoom_ready || controller!=v.controller || distance!=v.distance || minimum!=v.minimum) {
        v.zoom=fn<float(__fastcall *)(Obj)>(0xEDE0)(v.camera);
        v.controller=controller;v.distance=distance;v.minimum=minimum;v.zoom_ready=true;
    }
    // Continuous native stages 0..4. Stage five hides during its transition.
    if(!(v.zoom>=0.0f && v.zoom<=4.0f))return false;
    // 56FC20 is exactly MOV RAX,[RCX+48]; RET. No repeated getter call.
    return !v.popup || !pointer(v.popup,0x48);
}
static bool create(Panel &p,Obj parent,unsigned force) { COST_SCOPE("ro_heavy_create_face");HeavyTimer timer;
    p.attempted=true;
    Obj face=fn<Obj(__fastcall *)(size_t)>(0x3F6BC0)(0x178);
    if(!face)return false;
    fn<Obj(__fastcall *)(Obj)>(0x864C50)(face);
    *reinterpret_cast<Obj *>(face)=image+0x1342530;
    fn<void(__fastcall *)(Obj)>(0x92AB70)(face+0x130);
    *reinterpret_cast<Obj *>(face)=image+0x1397800; // CMapFaceLayout, not a button
    Obj owner=table(0xDF30,force,51);
    unsigned color=fn<unsigned(__fastcall *)(Obj,int)>(0x2AD010)(owner,0);
    fn<int(__fastcall *)(Obj,int,int,Obj,unsigned)>(0x1B2E80)(face,0,0,parent,color);
    p.face=face;p.parent=parent;p.force=force;p.visible=true;
    // Native dimensions and transform preserve the original portrait aspect.
    int w=static_cast<int>(dword(face,0x28)),h=static_cast<int>(dword(face,0x2C));
    if(w<=0 || h<=0 || !pointer(face,0x138))return false;
    p.width=(w*3+2)/4;p.height=(h*3+2)/4;
    fn<void(__fastcall *)(Obj,int,int)>(0x875B00)(face,p.width,p.height);
    fn<Frame>(0x866E80)(face);p.begun=true;
    show(p,false);
    return true;
}
#include "native_region_officer_traits.inl"
static void update(Obj layout) {
    if(layout==heavy_layout)heavy_spent=false; // a full label pass has elapsed
    auto it=panels.find(layout);
    Obj root=nullptr;unsigned force=0;
    bool eligible=world_view(root,force);
    const bool city=pointer(layout,0)==image+kFootholdVtable;
    Obj area=eligible?label_area(root,layout,city):nullptr;
    eligible=eligible && area;
    Obj tile=nullptr;
    if(eligible) {
        unsigned center=word(area,0x3A);
        tile=center<48400?pointer(root,0xE270+center*8):nullptr;
        eligible=tile && tile[0x14]==force;
    }
    Obj parent=pointer(layout,8);
    int x=static_cast<int>(dword(layout,0x20)),y=static_cast<int>(dword(layout,0x24));
    int w=static_cast<int>(dword(layout,0x28));
    int h=static_cast<int>(dword(layout,0x2C));
    // Culling precedes asset creation/person reads. These are native UI units.
    eligible=eligible && parent && x+w>=0 && x<1920 && y>=32 && y<1056
        && native_visible(layout);
    parent=layout; // Native child inherits late projection, visibility and reparenting.
    if(!eligible) {if(it!=panels.end())show(it->second,false);return;}
    if(it==panels.end()) {
        // Spread first-time resource creation across frames; no per-frame rebuild.
        ULONGLONG now=GetTickCount64();
        if(last_creation && now-last_creation<32)return;
        if(!heavy_budget(layout))return;
        last_creation=now;it=panels.emplace(layout,Panel{}).first;
    }
    Panel &p=it->second;
    if(p.face && p.parent!=parent) {release(layout,nullptr);return;}
    if(!p.attempted && !create(p,parent,force)) {
        // A failed resource stays hidden and is not retried every frame.
        show(p,false);return;
    }
    if(!p.face || !p.width || !p.height)return;
    unsigned id=word(area,0x36);
    Obj person=id && id<=6000?pointer(root,0x158+id*8):nullptr;
    unsigned art=person?word(person,0xA6):0;
    // A real portrait loads native image parts; defer it past this frame's
    // other heavy work and keep the previous face hidden until it lands.
    bool changed=id!=p.person || art!=p.art || force!=p.force;
    if(changed && person && !heavy_budget(layout)) {show(p,false);return;}
    if(changed) { COST_SCOPE("ro_heavy_set_portrait");HeavyTimer timer;
        fn<void(__fastcall *)(Obj,Obj)>(0x1B91E0)(p.face,person);
        Obj owner=pointer(root,0xDF30+force*8);
        unsigned color=person?fn<unsigned(__fastcall *)(Obj,int)>(0x2AD010)(owner,0):0xFF808080;
        fn<void(__fastcall *)(Obj,int,unsigned)>(0x92FA60)(p.face+0x130,5,color);
        p.person=id;p.art=art;p.force=force;
    }
    // Local offset only. Never copy the label's screen position: its map
    // controller may project it after this callback or stop framing it hidden.
    x=-p.width-4;y=(h-p.height)/2;
    if(x!=p.x || y!=p.y) {
        fn<void(__fastcall *)(Obj,int,int)>(0x874650)(p.face,x,y);p.x=x;p.y=y;
    }
    update_traits(p,root,person,layout,city?x:INT_MIN,(h+p.height)/2+2);
    show(p,true);
}
static void __fastcall frame(Obj layout) { COST_SCOPE("ro_frame");
    fn<Frame>(0x1A7F50)(layout);
    if(InterlockedCompareExchange(&installed,0,0)){update(layout);}
}
static void __fastcall leave(Obj layout,Obj context) { COST_SCOPE("ro_leave");
    release(layout,context);fn<Leave>(0x1A74D0)(layout,context);
}
static Obj __fastcall destroy(Obj layout,unsigned flags) {
    release(layout,nullptr);return fn<Destroy>(0x19B1D0)(layout,flags);
}
// City labels: same panel logic, the class's own originals.
static void __fastcall city_frame(Obj layout) { COST_SCOPE("ro_city_frame");
    fn<Frame>(0x1A7EA0)(layout);
    if(InterlockedCompareExchange(&installed,0,0)){update(layout);}
}
static void __fastcall city_leave(Obj layout,Obj context) { COST_SCOPE("ro_city_leave");
    release(layout,context);fn<Leave>(0x1A75A0)(layout,context);
}
static Obj __fastcall city_destroy(Obj layout,unsigned flags) {
    release(layout,nullptr);return fn<Destroy>(0x19B350)(layout,flags);
}
struct Hook {unsigned slot,original;void *replacement;};
static const Hook hooks[]={
    {0x139A338,0x1A7F50,reinterpret_cast<void *>(&frame)},
    {0x139A350,0x1A74D0,reinterpret_cast<void *>(&leave)},
    {0x139A328,0x19B1D0,reinterpret_cast<void *>(&destroy)},
    {0x13999C8,0x1A7EA0,reinterpret_cast<void *>(&city_frame)},
    {0x13999E0,0x1A75A0,reinterpret_cast<void *>(&city_leave)},
    {0x13999B8,0x19B350,reinterpret_cast<void *>(&city_destroy)}
};
#include "region_officer_profile.h"
static bool guards_match() {
    for(const auto &h:hooks)if(pointer(image,h.slot)!=image+h.original)return false;
    for(const auto &p:profile) {
        unsigned long long hash=0xCBF29CE484222325ull;
        for(unsigned i=0;i<p.size;++i)hash=(hash^image[p.rva+i])*0x100000001B3ull;
        if(hash!=p.hash)return false;
    }
    return true;
}
static bool install() {
    if(InterlockedCompareExchange(&installed,0,0) || !guards_match())return false;
    panels.reserve(512);
    unsigned published=0;
    for(const auto &h:hooks) {
        if(!patch_pointer(h.slot,h.original,h.replacement)) {
            while(published) {const auto &p=hooks[--published];restore_pointer(p.slot,p.original,p.replacement);}
            return false;
        }
        ++published;
    }
    InterlockedExchange(&installed,1);return true;
}
}
