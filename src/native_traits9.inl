// 개성 9칸 (traits9, SAN14_Convenience.dll). 2026-11-18 local235: 에디터에서 빌린 기계어 덩어리 3개(traits9_payloads.h)를
// 같은 동작의 소스로 다시 씀. 출처: SAN14CE Editor V8.3 Editor_Source san14ce/traits9ui.py·traits9abil.py·traits9list.py.
//  - 무장 상세 기본 탭·능력 탭: 원작 함수 한가운데서 "부품 번호 표 포인터"와 "반복 수 5"만 바꾸는 작은 중계(명령마다 주석).
//    레이아웃에 붙인 노드(기본 125~128, 능력 108~111, tools/traits9_layout.py)가 없으면(부품 수 부족) 원래 5칸 길.
//  - 능력 탭 번호 변환 8D0320: C++ 함수 traits9_part_of.
//  - 무장일람 개성 탭: 범용 일람 초기화 2C1360이 원작 표 두 개를 받을 때만 우리 확장본(열 213~216 = 개성6~9)으로 바꿈.
//    확장본은 준비 때 원작 표를 복사해 만들고, 새 열의 값·정렬 함수는 C++ 템플릿(traits9_list_value/sort).
//  - local236 편집 장수 일람(4CCB90, 자리 4CCBE3): 원작 1형 열 표(47행)와 탭 표 두 개를 받을 때만 확장본(열 2F~32 = 개성6~9,
//    값 210A60·정렬 355770)으로. 출처: 통합패치 SAN14_Traits9_UI.dll TABLE9(새 열 번호는 우리 규칙대로 끝 행 뒤 2F~).
//    나머지 화면 9칸은 native_traits9_screens.inl(owner traits9_screens).
//  - 5→9 값 패치 5곳(traits9_sites.h traits9_core).
// 무장 저장 레코드 모양은 그대로다(+0x118 u16[9]는 원작에 있음). 자리 표: traits9_sites.h.
#include "traits9_sites.h"
struct Trait9Patch {unsigned rva,size;unsigned char before[32];};
static Trait9Patch traits9_patches[T9_SITE_COUNT];   // before = 실행 중 이미지 바이트(traits9_copy_before)
// local10 (2026-11-06, docs/HISTORY.md 4.1): 표의 원래 바이트는 기준 빌드 값이다. RIP 변위가 빌드마다 다른 자리가 있고 Layout
// 관리자는 before를 글자 그대로 비교하므로, 가드(ga::mcmp_at이 실행 중 빌드의 원래 바이트를 판정)를 통과한 뒤 before를 이미지에서
// 복사한다. 값 패치 행은 쓰기 가능한 사본 traits9_core_rt(rva·size·after 그대로). 관리자는 아직 우리 바이트일 때만 되돌린다.
static Trait9Bytes traits9_core_rt[sizeof(traits9_core)/sizeof(traits9_core[0])];
static bool traits9_before_copied;
// 할당 하나: [0,kT9Code) 중계 코드(쓴 뒤 실행·읽기), 그 뒤 자료(읽기·쓰기, 게임이 읽음).
constexpr unsigned kT9Code=4096;
constexpr unsigned kT9Cols=213,kT9ColRec=0x60,kT9Pages=11,kT9PageRec=0x18,kT9TraitPage=6,kT9BaseCol=0x41,kT9NewCols=4;
// local236 범위 B: 편집 장수 일람(1형 표 1A38800, 열 47개 = 0~0x2D + 끝 행 0x2E, 탭 표 1A399A0·1A39A60 각 7개 + 끝, 개성 탭 5번).
constexpr unsigned kT9ECols=47,kT9EPages=7,kT9ETraitPage=5,kT9EBaseCol=0x27;
constexpr unsigned kT9BasicOff=0,kT9AbilOff=64,kT9ColsOff=128,kT9PagesOff=kT9ColsOff+(kT9Cols+kT9NewCols)*kT9ColRec,
    kT9TabOff=kT9PagesOff+(kT9Pages+1)*kT9PageRec,
    kT9EColsOff=kT9TabOff+64,kT9EPagesAOff=kT9EColsOff+(kT9ECols+kT9NewCols)*kT9ColRec,
    kT9EPagesBOff=kT9EPagesAOff+(kT9EPages+1)*kT9PageRec,kT9ETabOff=kT9EPagesBOff+(kT9EPages+1)*kT9PageRec,
    kT9DataSize=kT9ETabOff+64;
// 기본 탭 표식 부품 번호: 원작 60~64 + 레이아웃 노드 125~128. 부품 목록(+0x10) 수가 129 미만이면 원래 5칸.
static const unsigned kT9BasicIds[9]={60,61,62,63,64,125,126,127,128};
constexpr unsigned kT9BasicNeed=129;
// 능력 탭 로컬 번호: 원작 19~23 + 0x100+108~111(0x100 이상 = 부품 번호 그대로, traits9_part_of). 부품 112개 미만이면 원래 5칸.
static const unsigned kT9AbilIds[9]={19,20,21,22,23,0x100+108,0x100+109,0x100+110,0x100+111};
constexpr unsigned kT9AbilNeed=112;
// 일람 개성 탭 열 목록(원작 [0,3D~41,-1]) 확장본과 새 열 머리글(게임 문자열 표의 "개성6"~"개성9", 0x3E2 = 개성1에 이어짐).
static const int kT9TabIds[11]={0,0x3D,0x3E,0x3F,0x40,0x41,0xD5,0xD6,0xD7,0xD8,-1};
static const unsigned kT9Headers[kT9NewCols]={0x3E7,0x3E8,0x3E9,0x3EA};
// 편집 장수 일람 개성 탭 열 목록(원작 [0,23~27,-1]) 확장본: 새 열 2F~32 = 개성6~9(머리글은 무장일람과 같은 문자열).
static const int kT9ETabIds[11]={0,0x23,0x24,0x25,0x26,0x27,0x2F,0x30,0x31,0x32,-1};
static Obj traits9_mem;
static unsigned traits9_stub[T9_SITE_COUNT];
static bool traits9_list_ready,traits9_edit_list_ready;
static const int *traits9_map_table;   // 능력 탭 M[로컬] = 부품 번호 (원래 8D0320의 lea rcx,[M])

static Obj traits9_allocate(unsigned size) {
    uintptr_t base=reinterpret_cast<uintptr_t>(image);
    for(uintptr_t offset=0x3000000;offset<0x70000000;offset+=0x10000) {
        Obj result=static_cast<Obj>(VirtualAlloc(reinterpret_cast<void *>((base+offset)&~uintptr_t(0xFFFF)),size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        if(result)return result;
    }
    return nullptr;
}
static bool traits9_guards_match() {
    unsigned index=0;
    for(const auto &p:traits9_core) {
        if(ga::mcmp_at(image,p.rva,p.before,p.size)) {log_event("traits9_guard_failed",index);return false;}
        ++index;
    }
    for(const auto &s:traits9_sites) {
        if(ga::mcmp_at(image,s.rva,s.before,s.size)) {log_event("traits9_guard_failed",index);return false;}
        ++index;
    }
    return true;
}
// 실행 중 이미지의 원래 바이트를 before 사본으로. traits9_guards_match() 바로 뒤에만 부른다.
static bool traits9_copy_before() {
    unsigned core=0;
    for(size_t n=0;n<sizeof(traits9_core)/sizeof(traits9_core[0]);++n) {
        traits9_core_rt[n]=traits9_core[n];
        if(!traits9_core_rt[n].rva || traits9_core_rt[n].size>sizeof(traits9_core_rt[n].before))return false;
        memcpy(traits9_core_rt[n].before,image+traits9_core_rt[n].rva,traits9_core_rt[n].size);++core;
    }
    for(unsigned i=0;i<T9_SITE_COUNT;++i) {
        auto &p=traits9_patches[i];const auto &s=traits9_sites[i];
        if(!s.rva || s.size<5 || s.size>sizeof(p.before))return false;
        p.rva=s.rva;p.size=s.size;memcpy(p.before,image+p.rva,p.size);
    }
    traits9_before_copied=true;
    log_event("traits9_before_copied",T9_SITE_COUNT,core);
    return true;
}
// 게임 표 안의 64비트 주소 → 우리 이미지 포인터. 실행 중 게임은 재배치되어 있어 그대로 이미지 안이고, 주소 가드 검사처럼 재배치 없이
// 올린 이미지는 기본 주소(ImageBase) 기준 값이다. 둘 다 아니면 nullptr.
static const unsigned char *traits9_image_ptr(uint64_t v) {
    const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64 *>(image+reinterpret_cast<const IMAGE_DOS_HEADER *>(image)->e_lfanew);
    const uint64_t base=reinterpret_cast<uintptr_t>(image),size=nt->OptionalHeader.SizeOfImage,pref=nt->OptionalHeader.ImageBase;
    if(v>=base && v<base+size)return reinterpret_cast<const unsigned char *>(v);
    if(v>=pref && v<pref+size)return image+(v-pref);
    return nullptr;
}
// RIP 상대 명령의 대상: 자리 rva+at 위치의 rel32, 명령 끝 = rva+end.
static const unsigned char *traits9_rip(unsigned rva,unsigned at,unsigned end) {
    int d;memcpy(&d,image+rva+at,4);return image+rva+end+d;
}

// ---- 능력 탭 번호 변환 (원작 8D0320 자리, 탭 객체 vtable +0x198). 0x100 이상 로컬 번호는 0x100을 뺀 값을 부품 번호로 그대로
// 돌려준다(레이아웃 노드 108~111). 나머지는 원래 표 M 조회. 가상 호출이라 호출자는 휘발 레지스터를 모두 버린다고 본다.
static int traits9_part_of(void *,int local) {
    if(local>=0x100)return local-0x100;
    return traits9_map_table[local];
}

// ---- 무장일람 새 열(개성6~9)의 값·정렬 함수. 원작 개성 열 값 함수 35DE90과 같은 일에 칸 번호만 다르다.
using T9TraitAt=int(*)(void *officer,int slot);                    // 232030: 무장 n번째 개성 번호 (asm처럼 eax 전체를 씀)
using T9TraitValid=int(*)(void *trait);                            // 331780: 개성 객체 유효
using T9TraitSort=intptr_t(*)(void *a,void *b,int slot,int zero,int ascending);   // 355BF0: 공용 개성 열 비교
template<int K> static void *traits9_list_value(void *officer) {
    Obj root=pointer(image,GA(GA_2780F60));
    const int id=reinterpret_cast<T9TraitAt>(image+GA(GA_232030))(officer,K);
    // 개성 객체 표(232B×356), 0x163 넘는 번호는 0번. asm처럼 부호 있는 비교(번호는 u16이라 음수는 없음).
    Obj trait=pointer(root,0x7DD18+(id>0x163?0u:static_cast<unsigned>(id)*8u));
    if(reinterpret_cast<T9TraitValid>(image+GA(GA_331780))(trait))return trait+0x10;   // 개성 이름
    return *reinterpret_cast<void **>(image+GA(GA_1A4AB88));       // 이름 없음 문자열
}
template<int K,int Ascending> static intptr_t traits9_list_sort(void *a,void *b) {
    return reinterpret_cast<T9TraitSort>(image+GA(GA_355BF0))(a,b,K,0,Ascending);
}
// 원작 무장일람 표 두 개(열 정의 213개 × 0x60, 탭 11개 × 0x18 + 끝)를 복사하고 새 열 4개와 개성 탭 목록을 채운다.
// 원작 모양이 예상과 다르면(다른 모드가 바꿈 등) 일람만 원래대로 둔다.
static bool traits9_build_list(unsigned char *data) {
    const unsigned char *cols=image+GA(GA_1A2E830),*pages=image+GA(GA_1A33810);
    auto qword=[](const unsigned char *p) {uint64_t v;memcpy(&v,p,8);return v;};
    for(unsigned i:{0x3Du,0x3Eu,0x3Fu,0x40u,0x41u,kT9Cols-1})
        if(qword(cols+i*kT9ColRec)!=i) {log_event("traits9_list_skip",1,i);return false;}
    if(qword(pages+kT9Pages*kT9PageRec)!=0) {log_event("traits9_list_skip",2,0);return false;}
    static const int expect[7]={0,0x3D,0x3E,0x3F,0x40,0x41,-1};
    const unsigned char *ids=traits9_image_ptr(qword(pages+kT9TraitPage*kT9PageRec));
    if(!ids || memcmp(ids,expect,sizeof expect)) {log_event("traits9_list_skip",3,0);return false;}
    unsigned header;memcpy(&header,cols+kT9BaseCol*kT9ColRec+0x48,4);
    if(header!=0x3E6) {log_event("traits9_list_skip",4,header);return false;}   // 개성5 열 머리글
    unsigned char *out_cols=data+kT9ColsOff,*out_pages=data+kT9PagesOff;int *tab=reinterpret_cast<int *>(data+kT9TabOff);
    memcpy(out_cols,cols,kT9Cols*kT9ColRec);memcpy(out_pages,pages,(kT9Pages+1)*kT9PageRec);
    void *const value[kT9NewCols]={reinterpret_cast<void *>(&traits9_list_value<5>),reinterpret_cast<void *>(&traits9_list_value<6>),
        reinterpret_cast<void *>(&traits9_list_value<7>),reinterpret_cast<void *>(&traits9_list_value<8>)};
    void *const up[kT9NewCols]={reinterpret_cast<void *>(&traits9_list_sort<5,1>),reinterpret_cast<void *>(&traits9_list_sort<6,1>),
        reinterpret_cast<void *>(&traits9_list_sort<7,1>),reinterpret_cast<void *>(&traits9_list_sort<8,1>)};
    void *const down[kT9NewCols]={reinterpret_cast<void *>(&traits9_list_sort<5,0>),reinterpret_cast<void *>(&traits9_list_sort<6,0>),
        reinterpret_cast<void *>(&traits9_list_sort<7,0>),reinterpret_cast<void *>(&traits9_list_sort<8,0>)};
    for(unsigned n=0;n<kT9NewCols;++n) {
        unsigned char *r=out_cols+(kT9Cols+n)*kT9ColRec;
        memcpy(r,cols+kT9BaseCol*kT9ColRec,kT9ColRec);              // 개성5 열 레코드를 본뜸
        const uint64_t number=kT9Cols+n;memcpy(r,&number,8);         // +0 열 번호 213~216
        memcpy(r+0x48,&kT9Headers[n],4);                            // +48 머리글 문자열 번호
        memcpy(r+0x18,&value[n],8);memcpy(r+0x30,&up[n],8);memcpy(r+0x38,&down[n],8);   // +18 값, +30 오름, +38 내림
    }
    memcpy(tab,kT9TabIds,sizeof kT9TabIds);
    memcpy(out_pages+kT9TraitPage*kT9PageRec,&tab,8);               // 개성 탭(6번) 열 목록 → 확장본
    return true;
}

// ---- 편집 장수 일람(local236 범위 B, 출처: 통합패치 SAN14_Traits9_UI.dll v2.6 TABLE9를 읽기 전용으로 조사해 다시 씀).
// 원작 값 함수 3610D0(개성1)~361390(개성5)과 같은 일: 편집 무장 자료의 K번째 개성(210A60 = +0x84 u16[9], 칸 한계 없음).
using T9EditTraitAt=int(*)(void *person,int slot);                 // 210A60
using T9EditSort=intptr_t(*)(void *a,void *b,int slot,int ascending);   // 355770: 편집 일람 개성 열 비교 (355420 = (a,b,0,1))
template<int K> static void *traits9_edit_value(void *person) {
    Obj root=pointer(image,GA(GA_2780F60));
    const int id=reinterpret_cast<T9EditTraitAt>(image+GA(GA_210A60))(person,K);
    Obj trait=pointer(root,0x7DD18+(id>0x163?0u:static_cast<unsigned>(id)*8u));
    if(reinterpret_cast<T9TraitValid>(image+GA(GA_331780))(trait))return trait+0x10;
    return *reinterpret_cast<void **>(image+GA(GA_1A4AB88));
}
template<int K,int Ascending> static intptr_t traits9_edit_sort(void *a,void *b) {
    return reinterpret_cast<T9EditSort>(image+GA(GA_355770))(a,b,K,Ascending);
}
// 원작 1형 표(열 47행 × 0x60, 끝 행 0x2E = 번호 0x2E·+10 -1)와 탭 표 두 개(각 7개 + 끝)를 복사해 새 열 4개(2F~32)를 끝 행 뒤에
// 붙이고 두 탭 표의 개성 탭(5번)을 확장 목록으로. 무장일람과 같은 방식(원작 끝 행은 그대로 두고 그 뒤에 번호로 찾는 행을 더함).
static bool traits9_build_edit_list(unsigned char *data) {
    const unsigned char *cols=image+GA(GA_1A38800);
    const unsigned char *pages[2]={image+GA(GA_1A399A0),image+GA(GA_1A39A60)};
    auto qword=[](const unsigned char *p) {uint64_t v;memcpy(&v,p,8);return v;};
    auto dword=[](const unsigned char *p) {unsigned v;memcpy(&v,p,4);return v;};
    for(unsigned i:{0x23u,0x24u,0x25u,0x26u,0x27u,kT9ECols-1})
        if(qword(cols+i*kT9ColRec)!=i) {log_event("traits9_edit_list_skip",1,i);return false;}
    if(dword(cols+(kT9ECols-1)*kT9ColRec+0x10)!=0xFFFFFFFFu) {log_event("traits9_edit_list_skip",2,0);return false;}
    if(dword(cols+kT9EBaseCol*kT9ColRec+0x48)!=0x3E6) {log_event("traits9_edit_list_skip",3,dword(cols+kT9EBaseCol*kT9ColRec+0x48));return false;}
    static const int expect[7]={0,0x23,0x24,0x25,0x26,0x27,-1};
    for(unsigned t=0;t<2;++t) {
        if(qword(pages[t]+kT9EPages*kT9PageRec)!=0) {log_event("traits9_edit_list_skip",4,t);return false;}
        const unsigned char *ids=traits9_image_ptr(qword(pages[t]+kT9ETraitPage*kT9PageRec));
        if(!ids || memcmp(ids,expect,sizeof expect)) {log_event("traits9_edit_list_skip",5,t);return false;}
    }
    unsigned char *out_cols=data+kT9EColsOff;int *tab=reinterpret_cast<int *>(data+kT9ETabOff);
    memcpy(out_cols,cols,kT9ECols*kT9ColRec);
    void *const value[kT9NewCols]={reinterpret_cast<void *>(&traits9_edit_value<5>),reinterpret_cast<void *>(&traits9_edit_value<6>),
        reinterpret_cast<void *>(&traits9_edit_value<7>),reinterpret_cast<void *>(&traits9_edit_value<8>)};
    void *const up[kT9NewCols]={reinterpret_cast<void *>(&traits9_edit_sort<5,1>),reinterpret_cast<void *>(&traits9_edit_sort<6,1>),
        reinterpret_cast<void *>(&traits9_edit_sort<7,1>),reinterpret_cast<void *>(&traits9_edit_sort<8,1>)};
    void *const down[kT9NewCols]={reinterpret_cast<void *>(&traits9_edit_sort<5,0>),reinterpret_cast<void *>(&traits9_edit_sort<6,0>),
        reinterpret_cast<void *>(&traits9_edit_sort<7,0>),reinterpret_cast<void *>(&traits9_edit_sort<8,0>)};
    for(unsigned n=0;n<kT9NewCols;++n) {
        unsigned char *r=out_cols+(kT9ECols+n)*kT9ColRec;
        memcpy(r,cols+kT9EBaseCol*kT9ColRec,kT9ColRec);             // 개성5 열 레코드를 본뜸
        const uint64_t number=kT9ECols+n;memcpy(r,&number,8);        // +0 열 번호 2F~32
        memcpy(r+0x48,&kT9Headers[n],4);
        memcpy(r+0x18,&value[n],8);memcpy(r+0x30,&up[n],8);memcpy(r+0x38,&down[n],8);
    }
    memcpy(tab,kT9ETabIds,sizeof kT9ETabIds);
    const unsigned offs[2]={kT9EPagesAOff,kT9EPagesBOff};
    for(unsigned t=0;t<2;++t) {
        memcpy(data+offs[t],pages[t],(kT9EPages+1)*kT9PageRec);
        memcpy(data+offs[t]+kT9ETraitPage*kT9PageRec,&tab,8);
    }
    return true;
}

// ---- 중계 코드 쓰기. 원작 코드 한가운데 자리라 레지스터 그대로 이어 간다(각 자리 주석의 레지스터 뜻은 traits9_sites.h).
struct T9Emit {
    Obj base,p;bool ok=true;
    void b(std::initializer_list<unsigned char> v) {for(unsigned char c:v)*p++=c;}
    void u32(unsigned v) {memcpy(p,&v,4);p+=4;}
    void rel(const void *target) {   // rel32: 다음 명령 기준
        const intptr_t d=static_cast<const unsigned char *>(target)-(p+4);
        if(d<INT32_MIN || d>INT32_MAX)ok=false;
        const int v=static_cast<int>(d);memcpy(p,&v,4);p+=4;
    }
    unsigned char *jcc(unsigned char op) {*p++=op;*p++=0;return p-1;}   // 짧은 조건 분기, 착지는 land
    void land(unsigned char *at) {*at=static_cast<unsigned char>(p-(at+1));}
    void jmp(const void *target) {b({0xE9});rel(target);}
    void jl(const void *target) {b({0x0F,0x8C});rel(target);}
    unsigned start() {while((p-base)&15)*p++=0xCC;return static_cast<unsigned>(p-base);}
};
static bool traits9_emit(unsigned char *data) {
    T9Emit e{traits9_mem,traits9_mem};
    const unsigned char *basic=data+kT9BasicOff,*abil=data+kT9AbilOff;
    auto back=[](unsigned id) {return image+traits9_sites[id].rva+traits9_sites[id].size;};
    // 루프 머리 = 원래 jl rel8의 대상 (기본 8E1630, 능력 8E25F0)
    auto loop=[](unsigned id) {return image+traits9_sites[id].rva+6+static_cast<signed char>(image[traits9_sites[id].rva+5]);};
    const unsigned char *local_table=traits9_rip(traits9_sites[T9_ABIL_INIT_TABLE].rva,6,10);   // 원래 로컬 번호 표(19~23)
    traits9_map_table=reinterpret_cast<const int *>(traits9_rip(traits9_sites[T9_ABIL_MAP].rva,6,10));

    // 기본 탭 초기화 (r14 = 부품 목록, r12 = 부품 번호 표, r15d = 칸)
    traits9_stub[T9_BASIC_INIT_TABLE]=e.start();
    e.b({0x44,0x8B,0xFE});                                   // mov r15d,esi              (원래 첫 명령)
    e.b({0x4C,0x8D,0x25});e.rel(basic);                       // lea r12,[9칸 표]
    e.b({0x41,0x81,0x7E,0x10});e.u32(kT9BasicNeed);           // cmp dword [r14+10h],129
    {auto ok=e.jcc(0x7D);                                     // jge 9칸
     e.b({0x4C,0x8D,0x65,0x07});                              // lea r12,[rbp+7]           (원래: 5칸 스택 배열)
     e.land(ok);}
    e.jmp(back(T9_BASIC_INIT_TABLE));
    traits9_stub[T9_BASIC_INIT_COUNT]=e.start();
    e.b({0x41,0x81,0x7E,0x10});e.u32(kT9BasicNeed);           // cmp dword [r14+10h],129
    {auto five=e.jcc(0x7C);                                   // jl 5칸
     e.b({0x41,0x83,0xFF,0x09});e.jl(loop(T9_BASIC_INIT_COUNT));e.jmp(back(T9_BASIC_INIT_COUNT));   // cmp r15d,9 ; jl 루프 ; 나감
     e.land(five);}
    e.b({0x41,0x83,0xFF,0x05});e.jl(loop(T9_BASIC_INIT_COUNT));e.jmp(back(T9_BASIC_INIT_COUNT));    // cmp r15d,5 (원래)
    // 기본 탭 표시 (rbx = 부품 목록, r15 = 부품 번호 표, [rsp+40h] = 칸 수)
    traits9_stub[T9_BASIC_SHOW]=e.start();
    e.b({0x4C,0x8B,0xE6});                                   // mov r12,rsi               (원래 첫 명령)
    e.b({0x81,0x7B,0x10});e.u32(kT9BasicNeed);                // cmp dword [rbx+10h],129
    {auto orig=e.jcc(0x7C);                                   // jl 원래
     e.b({0x4C,0x8D,0x3D});e.rel(basic);                      // lea r15,[9칸 표]
     e.b({0x48,0xC7,0x44,0x24,0x40});e.u32(9);                // mov qword [rsp+40h],9
     e.jmp(back(T9_BASIC_SHOW));
     e.land(orig);}
    e.b({0x4C,0x8D,0x7C,0x24,0x58});                          // lea r15,[rsp+58h]         (원래)
    e.b({0x48,0xC7,0x44,0x24,0x40});e.u32(5);                 // mov qword [rsp+40h],5     (원래)
    e.jmp(back(T9_BASIC_SHOW));

    // 능력 탭 번호 변환: 함수 전체를 C++로 (jmp [abs])
    traits9_stub[T9_ABIL_MAP]=e.start();
    e.b({0xFF,0x25});e.u32(0);
    {void *fn=reinterpret_cast<void *>(&traits9_part_of);memcpy(e.p,&fn,8);e.p+=8;}
    // 능력 탭 초기화 (rsi = 부품 목록, r15 = 로컬 번호 표, r14d = 칸)
    traits9_stub[T9_ABIL_INIT_TABLE]=e.start();
    e.b({0x45,0x8B,0xF4});                                   // mov r14d,r12d             (원래 첫 명령)
    e.b({0x4C,0x8D,0x3D});e.rel(abil);                        // lea r15,[9칸 표]
    e.b({0x81,0x7E,0x10});e.u32(kT9AbilNeed);                 // cmp dword [rsi+10h],112
    {auto ok=e.jcc(0x7D);                                     // jge 9칸
     e.b({0x4C,0x8D,0x3D});e.rel(local_table);                // lea r15,[원래 표]
     e.land(ok);}
    e.jmp(back(T9_ABIL_INIT_TABLE));
    traits9_stub[T9_ABIL_INIT_COUNT]=e.start();
    e.b({0x81,0x7E,0x10});e.u32(kT9AbilNeed);                 // cmp dword [rsi+10h],112
    {auto five=e.jcc(0x7C);
     e.b({0x41,0x83,0xFE,0x09});e.jl(loop(T9_ABIL_INIT_COUNT));e.jmp(back(T9_ABIL_INIT_COUNT));     // cmp r14d,9
     e.land(five);}
    e.b({0x41,0x83,0xFE,0x05});e.jl(loop(T9_ABIL_INIT_COUNT));e.jmp(back(T9_ABIL_INIT_COUNT));      // cmp r14d,5 (원래)
    // 능력 탭 표시 (r15 = 부품 목록, r14 = 로컬 번호 표, [rsp+30h] = 칸 수)
    traits9_stub[T9_ABIL_SHOW]=e.start();
    e.b({0x41,0x81,0x7F,0x10});e.u32(kT9AbilNeed);            // cmp dword [r15+10h],112
    {auto orig=e.jcc(0x7C);
     e.b({0x4C,0x8D,0x35});e.rel(abil);                       // lea r14,[9칸 표]
     e.b({0x48,0xC7,0x44,0x24,0x30});e.u32(9);                // mov qword [rsp+30h],9
     e.jmp(back(T9_ABIL_SHOW));
     e.land(orig);}
    e.b({0x4C,0x8D,0x35});e.rel(traits9_rip(traits9_sites[T9_ABIL_SHOW].rva,3,7));   // lea r14,[원래 표] (원래 변위 그대로)
    e.b({0x48,0xC7,0x44,0x24,0x30});e.u32(5);                 // mov qword [rsp+30h],5     (원래)
    e.jmp(back(T9_ABIL_SHOW));

    // 무장일람 초기화 (rsi = 일람 객체). 원작 표 두 개가 넘어올 때만 확장본으로. rcx는 원래도 이 뒤에서 쓰지 않는 자리(에디터 판과 같음).
    traits9_stub[T9_LIST_INIT]=e.start();
    e.b({0x48,0x8B,0x45,0x77});                               // mov rax,[rbp+77h]         (원래: 탭 표)
    e.b({0x48,0x8D,0x0D});e.rel(image+GA(GA_1A33810));        // lea rcx,[원작 탭 표]
    e.b({0x48,0x39,0xC8});                                    // cmp rax,rcx
    {auto j=e.jcc(0x75);e.b({0x48,0x8D,0x05});e.rel(data+kT9PagesOff);e.land(j);}   // jne ; lea rax,[확장 탭 표]
    e.b({0x48,0x89,0x86,0x50,0x01,0x00,0x00});                // mov [rsi+150h],rax        (원래)
    e.b({0x4C,0x8B,0x6D,0x7F});                               // mov r13,[rbp+7Fh]         (원래: 열 정의 표)
    e.b({0x48,0x8D,0x0D});e.rel(image+GA(GA_1A2E830));        // lea rcx,[원작 열 정의 표]
    e.b({0x49,0x39,0xCD});                                    // cmp r13,rcx
    {auto j=e.jcc(0x75);e.b({0x4C,0x8D,0x2D});e.rel(data+kT9ColsOff);e.land(j);}    // jne ; lea r13,[확장 열 표]
    e.jmp(back(T9_LIST_INIT));

    // 편집 장수 일람 초기화 (4CCB90, 같은 레지스터). 열 표가 원작 1형 표이고 탭 표가 원작 두 개 중 하나일 때만 둘 다 확장본으로
    // (탭 표만 바꾸면 다른 열 표에 없는 2F~32를 찾게 되므로 열 표를 먼저 본다). r13 적재를 앞당겨도 두 원래 명령은 서로 무관하다.
    traits9_stub[T9_EDIT_LIST_INIT]=e.start();
    e.b({0x48,0x8B,0x45,0x77});                               // mov rax,[rbp+77h]         (원래: 탭 표)
    e.b({0x4C,0x8B,0x6D,0x7F});                               // mov r13,[rbp+7Fh]         (원래: 열 정의 표)
    e.b({0x48,0x8D,0x0D});e.rel(image+GA(GA_1A38800));        // lea rcx,[원작 1형 열 표]
    e.b({0x49,0x39,0xCD});                                    // cmp r13,rcx
    {auto done1=e.jcc(0x75);                                  // jne 그대로
     e.b({0x48,0x8D,0x0D});e.rel(image+GA(GA_1A399A0));       // lea rcx,[원작 탭 표 A]
     e.b({0x48,0x39,0xC8});                                   // cmp rax,rcx
     auto try_b=e.jcc(0x75);                                  // jne B 검사
     e.b({0x48,0x8D,0x05});e.rel(data+kT9EPagesAOff);         // lea rax,[확장 탭 표 A]
     auto swap1=e.jcc(0xEB);                                  // jmp 열 표 바꿈
     e.land(try_b);
     e.b({0x48,0x8D,0x0D});e.rel(image+GA(GA_1A39A60));       // lea rcx,[원작 탭 표 B]
     e.b({0x48,0x39,0xC8});                                   // cmp rax,rcx
     auto done2=e.jcc(0x75);                                  // jne 그대로
     e.b({0x48,0x8D,0x05});e.rel(data+kT9EPagesBOff);         // lea rax,[확장 탭 표 B]
     e.land(swap1);
     e.b({0x4C,0x8D,0x2D});e.rel(data+kT9EColsOff);           // lea r13,[확장 열 표]
     e.land(done1);e.land(done2);}
    e.b({0x48,0x89,0x86,0x50,0x01,0x00,0x00});                // mov [rsi+150h],rax        (원래)
    e.jmp(back(T9_EDIT_LIST_INIT));
    if(!e.ok || e.p-e.base>static_cast<intptr_t>(kT9Code)) {log_event("traits9_emit_failed",static_cast<unsigned>(e.p-e.base),e.ok);return false;}
    log_event("traits9_emitted",static_cast<unsigned>(e.p-e.base),0);
    return true;
}
static bool prepare_traits9() {
    if(!mod_features.traits9)return true;
    if(!traits9_guards_match() || !traits9_copy_before())return false;
    traits9_mem=traits9_allocate(kT9Code+kT9DataSize);if(!traits9_mem)return false;
    unsigned char *data=traits9_mem+kT9Code;
    memcpy(data+kT9BasicOff,kT9BasicIds,sizeof kT9BasicIds);memcpy(data+kT9AbilOff,kT9AbilIds,sizeof kT9AbilIds);
    traits9_list_ready=traits9_build_list(data);
    traits9_edit_list_ready=traits9_build_edit_list(data);
    if(!traits9_emit(data))return false;
    DWORD old=0;
    if(!VirtualProtect(traits9_mem,kT9Code,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),traits9_mem,kT9Code))return false;
    return true;
}
// 2026-11-05 local3 (사용자: 실패한 선택 기능은 자기만 꺼짐): publish_traits9는 실패한 부분을 돌려준다(bit0 = 9칸). 실패하면 여기서
// 관리자 등록을 거둬들이고 다른 기능은 그대로. local10: 포획 대화 부분(bit1)은 기능과 함께 없앰.
#ifdef LAYOUT_ROUTED
// 설계 8.1 45~46행: 값 패치 5곳과 인라인 점프 8곳을 owner "traits9"(YIELD_IF_PEER, 잇지 않음: 통합패치 Traits9가 같은 바이트를 씀)로
// 관리자에 등록. 16바이트보다 길거나 블록을 넘는 점프는 블록마다 쓴다(hook_span). before는 이미지에서 복사한 값.
static unsigned publish_traits9(bool slots) {
    if(!slots)return 0;
    unsigned mark=layout_client::current_mark();
    bool ok=traits9_before_copied;
    for(const auto &p:traits9_core_rt)
        if(ok && layout_client::bytes("traits9",p.rva,p.before,p.after,p.size,LF_Y)<=0)ok=false;
    for(unsigned i=0;i<T9_SITE_COUNT && ok;++i) {
        if(i==T9_LIST_INIT && !traits9_list_ready)continue;      // 원작 일람 표가 예상과 다름: 일람만 원래대로
        if(i==T9_EDIT_LIST_INIT && !traits9_edit_list_ready)continue;   // 편집 장수 일람도 같은 규칙
        const auto &p=traits9_patches[i];
        if(layout_client::span("traits9",p.rva,p.before,p.size,traits9_mem+traits9_stub[i],LF_Y)<=0)ok=false;
    }
    if(!ok) {layout_client::rollback_to(mark);return 1;}
    return 0;
}
#else
// Layout 관리자 없음(주소 가드 검사만 이 단위를 라우팅 없이 빌드하고, 게시하지 않음): 9칸을 걸지 않는다.
static unsigned publish_traits9(bool slots) {return slots?1u:0u;}
#endif
