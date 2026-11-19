// 개성 9칸 범위 B: 모든 화면 9칸 (owner "traits9_screens", SAN14_Convenience.dll, 2026-11-18 local236).
// 출처: 통합패치(san14ce-integrated-patch) SAN14_Traits9_UI.dll v2.6. 그 DLL은 실행하지 않고 읽기 전용으로 조사해
// (research/traits9_v2/ui_dll_inventory.md, 디컴파일 research/ghidra_refs/SAN14_Traits9_UI_49fb15b4e757d7a4, 현재 빌드 주소는 같은
// DLL 새 판 7A82493F의 표) 같은 동작을 소스로 다시 썼다. 배치 계산은 traits9_screens.h(게임 함수 없음, 주소 가드 검사가 검사).
//  - 갱신: 화면 4종 가상 +0x188, 정보 패널 갱신 8A6690 호출 34곳, 일기토 갱신 호출 3곳, 장수 편집 갱신 543650 호출·점프.
//    원래 함수가 끝난 뒤, 개성이 6개 이상이거나 6번째 이후 칸이 쓰이면 원작 라벨 5개를 재서 같은 부모에 원작 표식(CCmnMrker)
//    9개를 만들어 채우고 원작 5개를 숨긴다. 아니면 우리 9개를 숨긴다(원작이 그린 그대로).
//  - 열기(+0x38)·숨김(+0x40)·해제(+0x28): 화면 5종과 장수 편집 2종. 일기토 2종은 열기만 여기서 걸고, 숨김·해제 칸은
//    trait_hover가 가진 칸이라(한 칸에 주인 하나) trait_hover의 숨김·해제가 공용 허브 services::duel_grid(core_services.h)로
//    우리 함수를 부르고, 우리는 같은 허브로 trait_hover에 툴팁 대상을 알린다(모듈끼리 직접 부르지 않음). trait_hover가 없으면 일기토는 그리지 않음.
//  - 유닛 편성 탭 2곳: 개성 표식 9개 초기화 호출(call [r10+138h])을 span으로 바꿔 2열×5행으로 놓는다(원작은 6~9번째가 1~4번째에 겹침).
// 모든 게임 호출은 게임 UI 스레드의 위 걸이 안에서만 한다. 화면에서 예외가 나면 그 화면만 끈다. 그리지 못한 화면은
// traits9_screen_skip(화면, 이유, 값)을 화면·이유마다 한 번 남긴다. 이유: 1 부품 없음, 2 기록 가득/모르는 화면, 3 원작 라벨 배치가
// 예상 밖(측정 실패), 4 표식 생성 실패, 5 예외, 6 trait_hover 없음(일기토), 7 주소 미해결.
// 화면 번호: 0 수집, 1 부대 기본, 2 부대 능력, 3 지역, 4 정보 패널, 5 일기토 대결, 6 일기토 게이지, 7 장수 편집, 8 유닛 편성 탭,
// 9 무장 상세 기본 탭, 10 무장 상세 능력 탭.
//  - local237 무장 상세 두 탭(CInfoDetail[Shogu]PersonDefault 909100 / PersonPerson 909DF0, 통합패치 2AD0·2B40): 범위 A가 레이아웃에
//    붙인 노드(기본 125~128, 능력 108~111)가 이 화면에 있으면 A가 9칸을 그리므로 여기서는 그리지 않고, 노드가 없을 때만(다른 레이아웃·
//    레이아웃 미설치) 위와 같은 방식으로 9칸. 어느 쪽이든 traits9_detail_seen(탭, 개성 수 | A 노드<<8, 무장 번호)을 경우마다 한 번 남긴다.
#include <array>
#include <utility>
#include "traits9_screens.h"
namespace traits9_screens {
using Slot=uint64_t(__fastcall *)(Obj,uint64_t,uint64_t,uint64_t);
using LabelInit=uint64_t(__fastcall *)(Obj,int,int,int,int,Obj,int);   // CCmnMrker 가상 +0x138 (8534E0): x,y,폭,높이,부모,0
enum Screen : unsigned {SC_COLLECTION,SC_ARMY_DEFAULT,SC_ARMY_PARAM,SC_AREA,SC_UTIL,SC_DUEL_VS,SC_DUEL_GAUGE,SC_EDITOR,SC_UNIT_TAB,
    SC_DETAIL_BASIC,SC_DETAIL_ABIL,SC_COUNT};
enum Why : unsigned {WHY_PARTS=1,WHY_RECORD,WHY_MEASURE,WHY_CREATE,WHY_FAULT,WHY_NO_HOVER,WHY_ADDRESS,WHY_COUNT};
enum Role : unsigned char {RO_UPDATE,RO_UTIL,RO_DUEL_VS,RO_DUEL_GAUGE,RO_EDIT,RO_SHOW,RO_HIDE,RO_CLOSE};
static const unsigned kMaxRows=96,kRows=82,kRecs=128,kKnownMax=16;

// 화면별 고정값 (통합패치 26B0 호출 인자): 패널, 개성 거르기(0 전부, 1 정보 패널, 2 부대, 3 일기토), 라벨 +0x80 값(0 칸 순서,
// 1 개성 번호, 2 칸 순서+2), 부품 번호를 화면 가상 +0x198로 바꾸는지, 툴팁 주인 칸(0 = 없음), 원작 개성 라벨 부품 5개.
struct Spec {unsigned char screen,panel,mode,label;bool translate;unsigned short hover_off;int parts[5];};
static const Spec kCollection={SC_COLLECTION,0,0,0,true,0x1B8,{19,20,21,22,23}};
static const Spec kArmyDefault={SC_ARMY_DEFAULT,0,2,2,false,0x190,{105,106,107,108,109}};
static const Spec kArmyParam={SC_ARMY_PARAM,0,2,2,false,0x190,{105,106,107,108,109}};
static const Spec kArea={SC_AREA,0,0,0,false,0x180,{92,93,94,95,96}};
static const Spec kUtil={SC_UTIL,0,1,1,false,0x1D0,{28,33,37,41,45}};
static const Spec kDuelVs={SC_DUEL_VS,0,3,0,false,0,{10,12,14,16,18}};
static const Spec kDuelGauge[2]={{SC_DUEL_GAUGE,0,3,0,false,0,{13,15,17,19,21}},{SC_DUEL_GAUGE,1,3,0,false,0,{39,41,43,45,47}}};
static const int kEditorParts[5]={0x1F,0x21,0x23,0x25,0x27};
// local237 무장 상세 기본 탭(통합패치 2AD0: 부품 60~64, 툴팁 주인 +0x180)·능력 탭(2B40: 로컬 19~23을 가상 +0x198로, +0x1B8).
// a_node = 범위 A가 레이아웃에 붙인 마지막 노드(있으면 A의 9칸 길).
static const Spec kDetailBasic={SC_DETAIL_BASIC,0,0,0,false,0x180,{60,61,62,63,64}};
static const Spec kDetailAbil={SC_DETAIL_ABIL,0,0,0,true,0x1B8,{19,20,21,22,23}};
static const int kDetailANode[2]={128,111};

// 화면 객체마다 기록 하나(통합패치처럼 열 때 잡고 해제 때 놓는다). 패널 = 우리 표식 9개, 숨긴 원작 5개, 툴팁 주인, 켜짐, 실패.
struct Panel {Obj grid[9];Obj orig[5];Obj hover;int on,failed;};
struct Rec {Obj owner;int open;Panel panel[2];};
struct Traits {Obj obj[9];int slot[9],flag[9],id[9];unsigned n;};
struct Site {unsigned rva;Role role;unsigned char screen;};
struct RowInfo {Role role;unsigned char screen;unsigned original;};
static Rec recs[kRecs];
static RowInfo row_info[kMaxRows];
static void *next_row[kMaxRows];
static unsigned row_count,span_count,known_vt[kKnownMax],known_count;
static bool detail_seen[2][10][2];
static bool installed,lifecycle_off,screen_off[SC_COUNT],logged[SC_COUNT][WHY_COUNT],created_logged[SC_COUNT],unit_logged;
static Obj unit_mem;

template<class T> static T virt(Obj o,unsigned off) {return reinterpret_cast<T>(pointer(pointer(o,0),off));}
static unsigned vt_of(Obj o) {return o?static_cast<unsigned>(reinterpret_cast<uintptr_t>(pointer(o,0))-reinterpret_cast<uintptr_t>(image)):0;}
static void show(Obj o,int on) {if(o)virt<void(__fastcall *)(Obj,int)>(o,0xC8)(o,on);}
static int i32(Obj o,unsigned off) {return *reinterpret_cast<int *>(o+off);}
static t9s::Rect rect_of(Obj o) {return {i32(o,0x20),i32(o,0x24),i32(o,0x28),i32(o,0x2C)};}
static void skip(unsigned screen,unsigned why,unsigned value) {
    if(screen<SC_COUNT && why<WHY_COUNT && !logged[screen][why]) {logged[screen][why]=true;log_event("traits9_screen_skip",screen,why,value);}
}

// ---- 기록
static Rec *find(Obj owner) {
    if(!owner)return nullptr;
    for(auto &r:recs)if(r.owner==owner)return &r;
    return nullptr;
}
// 해제 칸을 우리가 가진 화면 종류만 기록을 잡는다(통합패치 1D70의 화면 표와 같은 이유: 해제를 못 받으면 표식을 지울 수 없음).
static bool known(Obj owner) {
    const unsigned vt=vt_of(owner);
    for(unsigned i=0;i<known_count;++i)if(known_vt[i]==vt)return true;
    return false;
}
static Rec *acquire(Obj owner) {
    if(Rec *r=find(owner))return r;
    if(!owner || !known(owner))return nullptr;
    for(auto &r:recs)if(!r.owner) {memset(&r,0,sizeof r);r.owner=owner;return &r;}
    return nullptr;
}
// 툴팁 주인 새로 고침(통합패치 1000): 지금 마우스가 그 주인 위면 끄고 다시 켜 새 표식이 툴팁 대상에 들어가게 한다.
static void hover_refresh(Obj hover) {
    Obj w=hover?pointer(hover,0x10):nullptr;
    if(!w)return;
    Obj m=fn<Obj(__fastcall *)()>(GA(GA_10F20))();
    if(m && fn<int(__fastcall *)(Obj,Obj)>(GA(GA_870B90))(m,w)) {
        virt<void(__fastcall *)(Obj)>(w,0x40)(w);
        virt<void(__fastcall *)(Obj)>(w,0x38)(w);
    }
}
// 열기·숨김(통합패치 1E50): 표식 9개가 다 있고 실패하지 않은 패널만 켠다(가상 +0x38), 켠 패널만 끈다(+0x40).
static void panels(Rec *rec,bool on) {
    if(!rec)return;
    rec->open=on?1:0;
    for(auto &p:rec->panel) {
        if(on) {
            if(p.on || !p.grid[8] || p.failed)continue;
            for(Obj g:p.grid)virt<void(__fastcall *)(Obj)>(g,0x38)(g);
            p.on=1;hover_refresh(p.hover);
        } else if(p.on) {
            for(Obj g:p.grid)virt<void(__fastcall *)(Obj)>(g,0x40)(g);
            p.on=0;
        }
    }
}
static void hide_grid(Rec *rec,unsigned panel) {
    if(!rec || panel>1)return;
    for(Obj g:rec->panel[panel].grid)show(g,0);
}
// 해제(통합패치 5EE0): 끄고, 표식마다 해제(+0x28, 원래 인자)와 삭제(+0, 1), 기록 비움.
static void release(Rec *rec,uint64_t arg) {
    if(!rec)return;
    panels(rec,false);
    for(auto &p:rec->panel)
        for(Obj &g:p.grid) {
            Obj o=g;
            if(!o)continue;
            g=nullptr;
            virt<void(__fastcall *)(Obj,uint64_t)>(o,0x28)(o,arg);
            virt<void(__fastcall *)(Obj,int)>(o,0)(o,1);
        }
    memset(rec,0,sizeof *rec);
}

// ---- 개성 모으기 (통합패치 2080). 칸 0~8의 개성 번호 1~0x163, 유효한 개성만. 거르기별로 원작 화면이 쓰는 판정을 그대로.
static void collect(Obj person,Obj owner,unsigned mode,int extra,Traits &t) {
    t.n=0;
    Obj root=world();
    if(!root || !valid(person))return;
    for(int i=0;i<9;++i) {
        const int id=fn<int(__fastcall *)(Obj,int)>(GA(GA_232030))(person,i);
        if(static_cast<unsigned>(id-1)>=0x163u)continue;
        Obj obj=pointer(root,0x7DD18+static_cast<unsigned>(id)*8u);
        if(!valid(obj))continue;
        int flag=1;
        if(mode==1 || mode==3) {                                          // 정보 패널·일기토: 89D800(구분, 번호)가 보이는 개성만
            if(!fn<int(__fastcall *)(int,int)>(GA(GA_89D800))(extra,id) || (mode==3 && id==0x57))continue;
            if(mode==1)
                if(Obj f=pointer(owner,0x1C0)) {Obj arg[2]={obj,nullptr};flag=virt<int(__fastcall *)(Obj,Obj *)>(f,0x10)(f,arg);}
        } else if(mode==2) {                                              // 부대: 1707A0(개성, 3 또는 1), 색 = 8A0880(부대, 칸)
            const int k=fn<int(__fastcall *)(Obj)>(GA(GA_234560))(owner)?3:1;
            if(!fn<int(__fastcall *)(Obj,int)>(GA(GA_1707A0))(obj,k))continue;
            flag=fn<int(__fastcall *)(Obj,int)>(GA(GA_8A0880))(owner,i);
        }
        t.obj[t.n]=obj;t.slot[t.n]=i;t.flag[t.n]=flag;t.id[t.n]=id;++t.n;
    }
}

// ---- 표식 9개 만들기 (일반 2280, 편집 1420). 원작 라벨 5개의 부모가 모두 이 화면이어야 한다. 실패 이유를 돌려준다(0 = 성공).
static unsigned create(Rec *rec,unsigned panel,Obj const orig[5],Obj hover,bool editor) {
    Obj owner=rec->owner;
    t9s::Rect in[5],out[9];
    for(unsigned i=0;i<5;++i) {
        if(pointer(orig[i],8)!=owner)return WHY_MEASURE;
        in[i]=rect_of(orig[i]);
    }
    if(editor?!t9s::editor_grid(in,out):!t9s::measure_grid(in,out))return WHY_MEASURE;
    Panel &p=rec->panel[panel];
    for(unsigned i=0;i<9;++i) {
        Obj g=fn<Obj(__fastcall *)(size_t)>(GA(GA_3F90E0))(0x188);   // 원작 할당 + 표식 생성자
        if(!g)return WHY_CREATE;
        fn<void(__fastcall *)(Obj)>(GA(GA_84D4B0))(g);
        p.grid[i]=g;                                                    // 실패해도 남겨 해제 때 지운다(통합패치와 같음)
        virt<LabelInit>(g,0x138)(g,out[i].x,out[i].y,out[i].w,out[i].h,owner,0);
        show(g,0);
        if(!pointer(g,0x178))return WHY_CREATE;
    }
    for(unsigned i=0;i<5;++i)p.orig[i]=orig[i];
    p.hover=hover;
    if(hover)
        for(unsigned i=0;i<9;++i) {
            Obj g=p.grid[i];
            *reinterpret_cast<int *>(g+0x84)=1;                         // 툴팁 허용
            if(editor)*reinterpret_cast<int *>(g+0x80)=static_cast<int>(i);
            fn<void(__fastcall *)(Obj,Obj)>(GA(GA_89D1B0))(hover,g);
        }
    if(rec->open)panels(rec,true);
    return 0;
}

// ---- 화면 하나 갱신 (통합패치 26B0). 9칸을 보이면 그 패널을 돌려준다.
static Panel *update_panel(Obj self,const Spec &s,Obj person,Obj owner,int extra,Traits &t) {
    collect(person,owner,s.mode,extra,t);
    Rec *rec=find(self);
    Obj hover=s.hover_off?pointer(self,s.hover_off):nullptr;
    if(!t9s::wants_grid(t.n,t.slot) || (s.hover_off && !hover)) {hide_grid(rec,s.panel);return nullptr;}
    Obj orig[5];
    for(unsigned i=0;i<5;++i) {
        int id=s.parts[i];
        if(s.translate)id=virt<int(__fastcall *)(Obj,int)>(self,0x198)(self,id);
        orig[i]=fn<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(self+0x130,id);
        if(!orig[i]) {hide_grid(rec,s.panel);skip(s.screen,WHY_PARTS,static_cast<unsigned>(s.parts[i]));return nullptr;}
    }
    if(!rec && !(rec=acquire(self))) {skip(s.screen,WHY_RECORD,vt_of(self));return nullptr;}
    Panel &p=rec->panel[s.panel];
    if(p.failed)return nullptr;
    if(!p.grid[8]) {
        const unsigned why=create(rec,s.panel,orig,hover,false);
        if(why) {p.failed=1;hide_grid(rec,s.panel);skip(s.screen,why,vt_of(self));return nullptr;}
        if(!created_logged[s.screen]) {created_logged[s.screen]=true;log_event("traits9_screen_grid",s.screen,s.panel,vt_of(self));}
    }
    for(unsigned i=0;i<9;++i) {
        Obj g=p.grid[i];
        if(i>=t.n) {show(g,0);continue;}
        *reinterpret_cast<int *>(g+0x80)=s.label==1?t.id[i]:s.label==2?static_cast<int>(i)+2:static_cast<int>(i);
        Obj obj=t.obj[i];
        fn<void(__fastcall *)(Obj,const wchar_t *)>(GA(GA_85ED10))(g,reinterpret_cast<const wchar_t *>(obj+0x10));
        const unsigned char type=obj[0xBE];
        if(s.mode==2)fn<void(__fastcall *)(Obj,unsigned char,int,int)>(GA(GA_8596A0))(g,type,t.flag[i],1);
        else if(s.mode==1 && !t.flag[i])fn<void(__fastcall *)(Obj,int)>(GA(GA_859750))(g,0);
        else fn<void(__fastcall *)(Obj,unsigned char,int)>(GA(GA_859660))(g,type,1);
        show(g,1);
    }
    for(Obj o:orig)show(o,0);
    return &p;
}
// 무장 번호 → 무장 (통합패치 1FD0): 6000 이하는 무장 표, 넘으면 빈 무장 칸.
static Obj person_by_id(unsigned id) {
    Obj root=world();
    if(!root)return nullptr;
    return id<0x1771?pointer(root,(id+0x2B)*8):pointer(root,0x73A50);
}
static void note_duel(Obj self,unsigned panel,const Panel *p,const Traits &t) {
    if(!p || !services::duel_grid.note)return;
    int ids[9]={};
    for(unsigned i=0;i<t.n && i<9;++i)ids[i]=t.id[i];
    services::duel_grid.note(self,panel,p->grid,ids);   // trait_hover: 툴팁 대상을 우리 표식으로
}
// local237 무장 상세 탭: A 노드가 있으면 A가 그리므로 우리 9칸은 숨기기만, 없으면 9칸. 진단: (탭, 개성 수, A 노드)마다 한 번
// traits9_detail_seen(화면, 개성 수 | A 노드 0x100 | 마지막 개성 칸<<16, 6번째 개성 번호).
// local238 진단(읽기만): 같은 무장의 사실무장 편집 기록과 비교. 원작은 새 게임을 시작할 때 2064A0이 편집 관리자(208DD0) +0x4B8 표
// 1100칸 중 +0xB0 bit0이 켜진 기록을 20B640으로 무장에 덮어쓴다(개성 +0x84 u16[9]을 243300으로 9칸 다, 210A60과 같은 칸).
// traits9_detail_edit(무장 번호, 무장 개성 수 | 편집 기록 개성 수<<8 | 기록 +0xB0<<16, 기록 있음). 무장 32명까지, 값이 바뀔 때만.
// 진단 전용이라 공개 빌드(NO_RUNTIME_LOG)에서는 빈 함수.
#ifdef NO_RUNTIME_LOG
static void detail_edit_note(Obj,unsigned) {}
#else
static unsigned short detail_edit_pid[32];
static unsigned detail_edit_val[32];
static unsigned detail_edit_count;
static void detail_edit_note(Obj person,unsigned n) {
    if(!valid(person))return;
    const unsigned pid=word(person,0x10);
    unsigned slot=0;
    while(slot<detail_edit_count && detail_edit_pid[slot]!=pid)++slot;
    if(slot==detail_edit_count) {
        if(detail_edit_count>=32)return;
        detail_edit_pid[detail_edit_count]=static_cast<unsigned short>(pid);detail_edit_val[detail_edit_count]=~0u;++detail_edit_count;
    } else if((detail_edit_val[slot]&0xFFu)==n)return;            // 갱신마다 표를 훑지 않는다: 무장 개성 수가 바뀔 때만
    Obj mgr=fn<Obj(__fastcall *)()>(GA(GA_208DD0))();             // 관리자 자체는 가상 표 객체인지 모르므로 null만 본다
    unsigned edit_n=0,flag=0,found=0;
    // 원작 20DF20과 같은 칸: 번호 1..1000 → pid-1, 2001..2088 → pid-0x3E9. local238은 valid()(가상 +0x18)로 걸러 기록을 못 찾았다
    // (2064A0은 null과 가상 +8만 본다). found: 1 = 칸에 기록 있음, 2 = 그 기록 번호도 같음.
    const unsigned idx=pid-1u<1000u?pid-1u:(pid-0x7D1u<0x58u?pid-0x3E9u:~0u);
    if(mgr && idx!=~0u)
        if(Obj rec=pointer(mgr,0x4B8+idx*8u)) {
            found=(static_cast<unsigned>(i32(rec,8))&0xFFFFu)==pid?2:1;
            flag=rec[0xB0];
            for(unsigned s=0;s<9;++s)if(static_cast<unsigned>(word(rec,0x84+s*2u)-1)<0x163u)++edit_n;
        }
    const unsigned val=n|edit_n<<8|flag<<16|found<<24;
    if(detail_edit_val[slot]==val)return;
    detail_edit_val[slot]=val;
    log_event("traits9_detail_edit",pid,val&0xFFFFFFu,found);
}
#endif
static void detail_body(unsigned screen,Obj self,Obj person) {
    const unsigned k=screen==SC_DETAIL_BASIC?0u:1u;
    const bool a_node=fn<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(self+0x130,kDetailANode[k])!=nullptr;
    Traits t;
    if(a_node) {collect(person,person,0,0,t);hide_grid(find(self),0);}
    else update_panel(self,k?kDetailAbil:kDetailBasic,person,person,0,t);
    const unsigned n=t.n<9?t.n:9u;
    if(!detail_seen[k][n][a_node?1:0]) {
        detail_seen[k][n][a_node?1:0]=true;
        log_event("traits9_detail_seen",screen,n|(a_node?0x100u:0u)|(n?static_cast<unsigned>(t.slot[n-1])<<16:0u),n>5?static_cast<unsigned>(t.id[5]):0u);
    }
    detail_edit_note(person,n);
}
static void screen_body(unsigned role,unsigned screen,Obj self,uint64_t a,uint64_t b) {
    Traits t;
    Obj a2=reinterpret_cast<Obj>(a);
    switch(role) {
    case RO_UPDATE:
        if(screen==SC_DETAIL_BASIC || screen==SC_DETAIL_ABIL)detail_body(screen,self,a2);
        else if(screen==SC_COLLECTION)update_panel(self,kCollection,a2,a2,0,t);
        else if(screen==SC_ARMY_DEFAULT || screen==SC_ARMY_PARAM)
            update_panel(self,screen==SC_ARMY_DEFAULT?kArmyDefault:kArmyParam,a2?person_by_id(word(a2,0x12)):nullptr,a2,0,t);
        else if(screen==SC_AREA)update_panel(self,kArea,a2?person_by_id(word(a2,0x36)):nullptr,a2,0,t);
        break;
    case RO_UTIL:update_panel(self,kUtil,pointer(self,0x178),self,i32(self,0x1D8),t);break;
    case RO_DUEL_VS:note_duel(self,0,update_panel(self,kDuelVs,a2,a2,0x10,t),t);break;
    case RO_DUEL_GAUGE: {
        Obj a3=reinterpret_cast<Obj>(b);
        auto person_of=[](Obj side) {return side?fn<Obj(__fastcall *)(Obj)>(GA(GA_22B770))(side):nullptr;};
        note_duel(self,0,update_panel(self,kDuelGauge[0],person_of(a2),a2,0x10,t),t);
        note_duel(self,1,update_panel(self,kDuelGauge[1],person_of(a3),a3,0x10,t),t);
        break;
    }
    default:break;
    }
}
// 장수 편집 탭 (통합패치 16A0): 편집 무장(가상 +0x1A0)의 개성 9칸(210A60), 원작 2열×3행 → 3열×3행. 툴팁 주인 = 화면 +0x1F0.
static void editor_body(Obj self) {
    Rec *rec=find(self);
    Obj person=virt<Obj(__fastcall *)(Obj)>(self,0x1A0)(self);
    Obj root=world();
    Traits t;t.n=0;
    if(person && root)
        for(int i=0;i<9;++i) {
            const int id=fn<int(__fastcall *)(Obj,int)>(GA(GA_210A60))(person,i);
            if(static_cast<unsigned>(id-1)>=0x163u)continue;
            Obj obj=pointer(root,0x7DD18+static_cast<unsigned>(id)*8u);
            if(!valid(obj))continue;
            t.obj[t.n]=obj;t.slot[t.n]=i;t.flag[t.n]=1;t.id[t.n]=id;++t.n;
        }
    if(!t9s::wants_grid(t.n,t.slot)) {hide_grid(rec,0);return;}
    Obj orig[5];
    for(unsigned i=0;i<5;++i) {
        orig[i]=fn<Obj(__fastcall *)(Obj,int)>(GA(GA_92F390))(self+0x130,kEditorParts[i]);
        if(!orig[i]) {hide_grid(rec,0);skip(SC_EDITOR,WHY_PARTS,static_cast<unsigned>(kEditorParts[i]));return;}
    }
    if(!rec && !(rec=acquire(self))) {skip(SC_EDITOR,WHY_RECORD,vt_of(self));return;}
    Panel &p=rec->panel[0];
    if(p.failed)return;
    if(!p.grid[8]) {
        Obj hover=pointer(self,0x1F0);
        const unsigned why=hover?create(rec,0,orig,hover,true):WHY_MEASURE;
        if(why) {p.failed=1;hide_grid(rec,0);skip(SC_EDITOR,why,vt_of(self));return;}
        if(!created_logged[SC_EDITOR]) {created_logged[SC_EDITOR]=true;log_event("traits9_screen_grid",SC_EDITOR,0,vt_of(self));}
    }
    for(unsigned i=0;i<9;++i) {
        Obj g=p.grid[i];
        if(i>=t.n) {show(g,0);continue;}
        *reinterpret_cast<int *>(g+0x80)=t.slot[i];
        Obj obj=t.obj[i];
        fn<void(__fastcall *)(Obj,unsigned char,int)>(GA(GA_859660))(g,obj[0xBE],1);
        fn<void(__fastcall *)(Obj,const wchar_t *)>(GA(GA_85ED10))(g,reinterpret_cast<const wchar_t *>(obj+0x10));
        show(g,1);
    }
    for(Obj o:orig)show(o,0);
}
static void screen_guarded(unsigned role,unsigned screen,Obj self,uint64_t a,uint64_t b) {
    if(screen>=SC_COUNT || screen_off[screen] || !self)return;
    __try {
        if(role==RO_EDIT)editor_body(self);
        else screen_body(role,screen,self,a,b);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {screen_off[screen]=true;skip(screen,WHY_FAULT,GetExceptionCode());}
}
static void lifecycle_guarded(unsigned role,Obj self,uint64_t arg) {
    if(lifecycle_off || !self)return;
    __try {
        if(role==RO_SHOW)panels(acquire(self),true);
        else if(role==RO_HIDE)panels(find(self),false);
        else release(find(self),arg);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {lifecycle_off=true;log_event("traits9_screen_lifecycle_fault",role,GetExceptionCode(),vt_of(self));}
}
// trait_hover가 가진 일기토 숨김(+0x40)·해제(+0x28) 칸에서 불린다.
static void duel_hide(Obj self) {lifecycle_guarded(RO_HIDE,self,0);}
static void duel_close(Obj self,uint64_t arg) {lifecycle_guarded(RO_CLOSE,self,arg);}

// ---- 걸이 본체. 행 번호 K마다 같은 본체를 부르는 함수 하나(관리자가 행마다 다음 함수를 따로 준다).
static uint64_t dispatch(unsigned k,Obj self,uint64_t a,uint64_t b,uint64_t c) { COST_SCOPE("traits9_screens");
    const RowInfo &r=row_info[k];
    Slot next=next_or<Slot>(next_row[k],r.original);
    switch(r.role) {
    case RO_SHOW: {const uint64_t v=next(self,a,b,c);lifecycle_guarded(RO_SHOW,self,0);return v;}
    case RO_HIDE:lifecycle_guarded(RO_HIDE,self,0);return next(self,a,b,c);
    case RO_CLOSE:lifecycle_guarded(RO_CLOSE,self,a);return next(self,a,b,c);
    default: {const uint64_t v=next(self,a,b,c);screen_guarded(r.role,r.screen,self,a,b);return v;}
    }
}
template<unsigned K> static uint64_t __fastcall hook(Obj self,uint64_t a,uint64_t b,uint64_t c) {return dispatch(K,self,a,b,c);}
template<unsigned... I> static constexpr std::array<Slot,sizeof...(I)> make_hooks(std::integer_sequence<unsigned,I...>) {return {{&hook<I>...}};}
static constexpr std::array<Slot,kMaxRows> kHooks=make_hooks(std::make_integer_sequence<unsigned,kMaxRows>{});

// ---- 유닛 편성 탭 (통합패치 39C0). 탭 객체 +Off의 표식 배열에서 이 표식의 위치 u를 찾고, 이미 놓은 배열[0]·배열[2]로 2열×5행 자리를 계산.
static void unit_adjust_impl(Obj label,Obj parent,unsigned off,int &x,int &y,int &w,int h) {
    for(unsigned u=0;u<9;++u) {
        if(pointer(parent,off+u*8)!=label)continue;
        Obj first=pointer(parent,off),second=pointer(parent,off+0x10);
        t9s::Rect rf{},rs{};
        if(first)rf=rect_of(first);
        if(second)rs=rect_of(second);
        int nx=x,ny=y,nw=w;
        if(!t9s::unit_tab(u,first?&rf:nullptr,second?&rs:nullptr,nx,ny,nw,h)) {skip(SC_UNIT_TAB,WHY_MEASURE,u);return;}
        x=nx;y=ny;w=nw;
        if(!unit_logged) {unit_logged=true;log_event("traits9_screen_grid",SC_UNIT_TAB,off,static_cast<unsigned>(w));}
        return;
    }
}
static void unit_adjust(Obj label,Obj parent,unsigned off,int &x,int &y,int &w,int h) {
    __try {unit_adjust_impl(label,parent,off,x,y,w,h);}
    __except(EXCEPTION_EXECUTE_HANDLER) {screen_off[SC_UNIT_TAB]=true;skip(SC_UNIT_TAB,WHY_FAULT,GetExceptionCode());}
}
template<unsigned Off> static uint64_t __fastcall unit_label_init(Obj label,int x,int y,int w,int h,Obj parent,int flag) {
    if(label && parent && !screen_off[SC_UNIT_TAB])unit_adjust(label,parent,Off,x,y,w,h);
    return virt<LabelInit>(label,0x138)(label,x,y,w,h,parent,flag);
}
// 원래 7바이트 call qword [r10+138h] (41 FF 92 38 01 00 00) 두 곳: 7D4B72(+0x178 배열, 앞에 lea rsi,[rdi+178h]),
// 7D5815(+0x220 배열, 앞에 lea rsi,[rdi+220h]).
static const unsigned char kUnitCall[7]={0x41,0xFF,0x92,0x38,0x01,0x00,0x00};
// 중계 32바이트: call [rip+5] ; jmp 자리+7 ; dq C++ 함수. rsp는 원래 호출 자리와 같으므로 스택 인자(높이·부모·0)도 그대로 보인다.
static bool emit_unit_relays(const unsigned sites[2]) {
    unit_mem=traits9_allocate(64);
    if(!unit_mem)return false;
    void *const target[2]={reinterpret_cast<void *>(&unit_label_init<0x178>),reinterpret_cast<void *>(&unit_label_init<0x220>)};
    for(unsigned k=0;k<2;++k) {
        unsigned char *p=unit_mem+k*32;
        memset(p,0xCC,32);
        p[0]=0xFF;p[1]=0x15;const int d=5;memcpy(p+2,&d,4);
        const intptr_t back=(image+sites[k]+7)-(p+11);
        if(back<INT32_MIN || back>INT32_MAX)return false;
        p[6]=0xE9;const int rel=static_cast<int>(back);memcpy(p+7,&rel,4);
        memcpy(p+11,&target[k],8);
    }
    DWORD old=0;
    return VirtualProtect(unit_mem,64,PAGE_EXECUTE_READ,&old) && FlushInstructionCache(GetCurrentProcess(),unit_mem,64);
}

// ---- 자리 표 (layout_registry.h owner "traits9_screens" 82행과 같은 순서·같은 수). 화면 번호는 묶음별 건너뛰기에 쓴다.
static unsigned build_sites(Site *s) {
    unsigned n=0;
    auto add=[&](unsigned rva,Role role,Screen sc) {if(n<kMaxRows)s[n++]={rva,role,static_cast<unsigned char>(sc)};};
    add(GA(GA_1417938),RO_UPDATE,SC_COLLECTION);add(GA(GA_14177D8),RO_CLOSE,SC_COLLECTION);
    add(GA(GA_14177E8),RO_SHOW,SC_COLLECTION);add(GA(GA_14177F0),RO_HIDE,SC_COLLECTION);
    add(GA(GA_14ABD78),RO_UPDATE,SC_ARMY_DEFAULT);add(GA(GA_14ABC18),RO_CLOSE,SC_ARMY_DEFAULT);
    add(GA(GA_14ABC28),RO_SHOW,SC_ARMY_DEFAULT);add(GA(GA_14ABC30),RO_HIDE,SC_ARMY_DEFAULT);
    add(GA(GA_14ABF10),RO_UPDATE,SC_ARMY_PARAM);add(GA(GA_14ABDB0),RO_CLOSE,SC_ARMY_PARAM);
    add(GA(GA_14ABDC0),RO_SHOW,SC_ARMY_PARAM);add(GA(GA_14ABDC8),RO_HIDE,SC_ARMY_PARAM);
    add(GA(GA_14AB6B8),RO_UPDATE,SC_AREA);add(GA(GA_14AB558),RO_CLOSE,SC_AREA);
    add(GA(GA_14AB568),RO_SHOW,SC_AREA);add(GA(GA_14AB570),RO_HIDE,SC_AREA);
    add(GA(GA_147B130),RO_CLOSE,SC_UTIL);add(GA(GA_147B140),RO_SHOW,SC_UTIL);add(GA(GA_147B148),RO_HIDE,SC_UTIL);
    const unsigned util[34]={GA(GA_6F4992),GA(GA_74E0C6),GA(GA_75D683),GA(GA_75D68F),GA(GA_77AAD1),GA(GA_77B51D),GA(GA_77B991),
        GA(GA_77C435),GA(GA_77C5A6),GA(GA_77F5EA),GA(GA_77FD8C),GA(GA_781388),GA(GA_7815FC),GA(GA_782B56),GA(GA_782F99),GA(GA_7939B3),
        GA(GA_793AB3),GA(GA_793B63),GA(GA_793C4A),GA(GA_793CCE),GA(GA_793DB3),GA(GA_793E53),GA(GA_793F0C),GA(GA_793F9A),GA(GA_79401A),
        GA(GA_79408A),GA(GA_7940FA),GA(GA_794217),GA(GA_7993B5),GA(GA_79E4BE),GA(GA_7A013F),GA(GA_7A0757),GA(GA_7A079D),GA(GA_8A0759)};
    for(unsigned rva:util)add(rva,RO_UTIL,SC_UTIL);
    add(GA(GA_809C37),RO_DUEL_VS,SC_DUEL_VS);add(GA(GA_809DE0),RO_DUEL_VS,SC_DUEL_VS);add(GA(GA_14630A0),RO_SHOW,SC_DUEL_VS);
    add(GA(GA_80B37D),RO_DUEL_GAUGE,SC_DUEL_GAUGE);add(GA(GA_1463580),RO_SHOW,SC_DUEL_GAUGE);
    add(GA(GA_13EC300),RO_CLOSE,SC_EDITOR);add(GA(GA_13EC310),RO_SHOW,SC_EDITOR);add(GA(GA_13EC318),RO_HIDE,SC_EDITOR);
    add(GA(GA_13ED218),RO_CLOSE,SC_EDITOR);add(GA(GA_13ED228),RO_SHOW,SC_EDITOR);add(GA(GA_13ED230),RO_HIDE,SC_EDITOR);
    add(GA(GA_4E958B),RO_EDIT,SC_EDITOR);add(GA(GA_542E53),RO_EDIT,SC_EDITOR);
    // local237 무장 상세: 기본 탭 909100(가상 +0x188 + 장수용 90C300 안 호출), 능력 탭 909DF0(같음), 네 화면의 해제·열기·숨김
    add(GA(GA_14AE900),RO_UPDATE,SC_DETAIL_BASIC);add(GA(GA_90C315),RO_UPDATE,SC_DETAIL_BASIC);
    add(GA(GA_14AE7A0),RO_CLOSE,SC_DETAIL_BASIC);add(GA(GA_14AE7B0),RO_SHOW,SC_DETAIL_BASIC);add(GA(GA_14AE7B8),RO_HIDE,SC_DETAIL_BASIC);
    add(GA(GA_14AEAF8),RO_CLOSE,SC_DETAIL_BASIC);add(GA(GA_14AEB08),RO_SHOW,SC_DETAIL_BASIC);add(GA(GA_14AEB10),RO_HIDE,SC_DETAIL_BASIC);
    add(GA(GA_14164C8),RO_UPDATE,SC_DETAIL_ABIL);add(GA(GA_90C3F5),RO_UPDATE,SC_DETAIL_ABIL);
    add(GA(GA_1416368),RO_CLOSE,SC_DETAIL_ABIL);add(GA(GA_1416378),RO_SHOW,SC_DETAIL_ABIL);add(GA(GA_1416380),RO_HIDE,SC_DETAIL_ABIL);
    add(GA(GA_14AECC0),RO_CLOSE,SC_DETAIL_ABIL);add(GA(GA_14AECD0),RO_SHOW,SC_DETAIL_ABIL);add(GA(GA_14AECD8),RO_HIDE,SC_DETAIL_ABIL);
    return n;
}
static const LayoutRow *registry_row(unsigned rva) {
    for(unsigned i=0;i<layout_row_count;++i)
        if(layout_rows[i].rva==rva && layout_rows[i].owner && !strcmp(layout_rows[i].owner,"traits9_screens"))return &layout_rows[i];
    return nullptr;
}
static unsigned registry_count() {
    unsigned n=0;
    for(unsigned i=0;i<layout_row_count;++i)if(layout_rows[i].owner && !strcmp(layout_rows[i].owner,"traits9_screens"))++n;
    return n;
}

// 활성화 때 한 번(trait_hover 다음). 주소가 풀리지 않은 화면 묶음은 빼고 로그, 나머지는 한 거래로 등록(하나라도 거절되면 전부 취소).
static bool install() {
    if(installed || !mod_features.traits9)return false;
    const unsigned need[]={GA(GA_2780F60),GA(GA_331780),GA(GA_232030),GA(GA_92F390),GA(GA_3F90E0),GA(GA_84D4B0),GA(GA_85ED10),
        GA(GA_859660),GA(GA_8596A0),GA(GA_859750),GA(GA_89D1B0),GA(GA_10F20),GA(GA_870B90),GA(GA_89D800),GA(GA_234560),GA(GA_1707A0),
        GA(GA_8A0880),GA(GA_22B770),GA(GA_210A60)};
    for(unsigned i=0;i<sizeof(need)/sizeof(need[0]);++i)if(!need[i]) {log_event("traits9_screens_off",1,i,0);return false;}
    Site s[kMaxRows];
    const unsigned n=build_sites(s);
    if(n!=kRows || registry_count()!=kRows) {log_event("traits9_screens_off",2,n,registry_count());return false;}
    const bool hover=services::duel_grid.note!=nullptr;   // trait_hover가 설치됨(일기토 숨김·해제 칸 주인, 앞 순서로 활성화)
    bool group_ok[SC_COUNT];
    for(bool &g:group_ok)g=true;
    if(!hover) {group_ok[SC_DUEL_VS]=group_ok[SC_DUEL_GAUGE]=false;skip(SC_DUEL_VS,WHY_NO_HOVER,0);skip(SC_DUEL_GAUGE,WHY_NO_HOVER,0);}
    for(unsigned i=0;i<n;++i)if(!s[i].rva || !registry_row(s[i].rva)) {
        if(group_ok[s[i].screen])skip(s[i].screen,WHY_ADDRESS,i);
        group_ok[s[i].screen]=false;
    }
    const unsigned unit[2]={GA(GA_7D4B72),GA(GA_7D5815)};
    if(!unit[0] || !unit[1] || ga::mcmp_at(image,unit[0],kUnitCall,7) || ga::mcmp_at(image,unit[1],kUnitCall,7)) {
        group_ok[SC_UNIT_TAB]=false;skip(SC_UNIT_TAB,WHY_ADDRESS,0);
    }
    // 기록을 잡을 수 있는 화면 종류 = 해제 칸을 가진 vtable (해제 칸 - 0x28; 일기토는 열기 칸 - 0x38, trait_hover가 해제를 줄 때만)
    known_count=0;
    for(unsigned i=0;i<n;++i)
        if(group_ok[s[i].screen] && s[i].role==RO_CLOSE && known_count<kKnownMax)known_vt[known_count++]=s[i].rva-0x28;
    if(hover) {   // 해제 칸 11개 + 일기토 2개 ≤ kKnownMax
        if(group_ok[SC_DUEL_VS] && known_count<kKnownMax)known_vt[known_count++]=GA(GA_14630A0)-0x38;
        if(group_ok[SC_DUEL_GAUGE] && known_count<kKnownMax)known_vt[known_count++]=GA(GA_1463580)-0x38;
    }
    memset(recs,0,sizeof recs);
    layout_client::Transaction tx;
    row_count=span_count=0;
    unsigned mask=0;
    for(unsigned i=0;i<n;++i) {
        if(!group_ok[s[i].screen])continue;
        const LayoutRow *row=registry_row(s[i].rva);
        if(row_count>=kMaxRows)return false;
        const unsigned k=row_count;
        row_info[k]={s[i].role,s[i].screen,row->original};
        void *rep=reinterpret_cast<void *>(kHooks[k]);
        const int handle=row->kind==LR_VT?layout_client::vt(row->rva,rep,&next_row[k]):
            row->kind==LR_CALL?layout_client::call(row->rva,rep,&next_row[k]):layout_client::jump(row->rva,rep,&next_row[k]);
        if(handle<=0) {log_event("traits9_screens_register_rejected",k,handle<0?static_cast<unsigned>(-handle):0u,row->rva);return false;}
        ++row_count;mask|=1u<<s[i].screen;
    }
    if(group_ok[SC_UNIT_TAB]) {
        if(!emit_unit_relays(unit)) {log_event("traits9_screens_off",3,0,0);return false;}
        for(unsigned k=0;k<2;++k) {
            if(layout_client::span("traits9_screens",unit[k],kUnitCall,7,unit_mem+k*32,LF_Y)<=0) {
                log_event("traits9_screens_register_rejected",kMaxRows+k,0,unit[k]);return false;
            }
            ++span_count;
        }
        mask|=1u<<SC_UNIT_TAB;
    }
    tx.done=true;installed=true;
    if(hover) {services::duel_grid.hide=&duel_hide;services::duel_grid.close=&duel_close;}
    log_event("traits9_screens_installed",row_count,span_count,mask);
    return true;
}
}   // namespace traits9_screens
static bool traits9_screens_install() {return traits9_screens::install();}
