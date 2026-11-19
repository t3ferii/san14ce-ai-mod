// 개성 9칸(traits9)이 손대는 원작 자리 (현재 빌드 1a4a447d, 2026-11-18 손으로 쓴 표).
// 예전 생성물 traits9_payloads.h(에디터 케이브 기계어 덩어리 3개를 그대로 굳힌 117KB)를 대체한다. 덩어리가 하던 일은
// native_traits9.inl이 명령마다 주석을 단 바이트 줄과 C++ 함수로 다시 만든다.
// 출처: SAN14CE Editor V8.3 Editor_Source san14ce/traits9ui.py·traits9abil.py·traits9list.py(2026-09-23, 2026-09-28 빌드 주소)와
// 그 개성 5→9 값 패치. 원작 무장 레코드 +0x118 u16[9] 저장칸은 원래 있고(원작 321FB4가 9개를 복사), 원작은 앞 5칸만 쓴다.
// tests/verify_traits9_save.py와 tests/verify_layout_registry.py가 이 두 표를 읽는다(행 모양을 바꾸면 두 검사도 함께).
struct Trait9Site {unsigned rva,size;unsigned char before[32];};
struct Trait9Bytes {unsigned rva,size;unsigned char before[16],after[16];};
enum Trait9SiteId : unsigned {
    T9_BASIC_INIT_TABLE,T9_BASIC_INIT_COUNT,T9_BASIC_SHOW,                 // 무장 상세 기본 탭 개성 표식
    T9_ABIL_MAP,T9_ABIL_INIT_TABLE,T9_ABIL_INIT_COUNT,T9_ABIL_SHOW,        // 무장 상세 능력 탭 개성 표식
    T9_LIST_INIT,                                                          // 무장일람 개성 탭 열
    T9_EDIT_LIST_INIT,                                                     // 편집 장수 일람 개성 탭 열 (local236, 범위 B)
    T9_SITE_COUNT
};
// 인라인 점프 자리 8곳 (E9 + NOP으로 덮고 우리 중계 코드로). 원래 바이트는 가드가 실행 중 이미지와 비교한다.
static const Trait9Site traits9_sites[T9_SITE_COUNT]={
    // 8E1070(무장 상세 기본 탭 초기화) 안: mov r15d,esi ; lea r12,[rbp+7]  (r12 = 스택의 부품 번호 5칸 배열)
    {GA(GA_8E1628),7,{0x44,0x8B,0xFE,0x4C,0x8D,0x65,0x07}},
    // 같은 함수 루프 끝: cmp r15d,5 ; jl 8E1630  (부품 5개마다 92DD00으로 표식을 만들고 툴팁 처리기에 등록)
    {GA(GA_8E1685),6,{0x41,0x83,0xFF,0x05,0x7C,0xA5}},
    // 909100(기본 탭 표시) 안: mov r12,rsi ; lea r15,[rsp+58h] ; mov qword [rsp+40h],5  (개성 목록을 돌며 이름·색 채우기)
    {GA(GA_909734),17,{0x4C,0x8B,0xE6,0x4C,0x8D,0x7C,0x24,0x58,0x48,0xC7,0x44,0x24,0x40,0x05,0x00,0x00,0x00}},
    // 8D0320(능력 탭 가상 함수 +0x198, 로컬 번호 → 부품 번호) 함수 전체 14바이트: movsxd rax,edx ; lea rcx,[M] ; mov eax,[rcx+rax*4] ; ret
    {GA(GA_8D0320),14,{0x48,0x63,0xC2,0x48,0x8D,0x0D,0x26,0xCE,0x1B,0x01,0x8B,0x04,0x81,0xC3}},
    // 8E1EF0(능력 탭 초기화) 안: mov r14d,r12d ; lea r15,[로컬 번호 표 19~23]
    {GA(GA_8E25D8),10,{0x45,0x8B,0xF4,0x4C,0x8D,0x3D,0xA6,0xAA,0x1A,0x01}},
    // 같은 함수 루프 끝: cmp r14d,5 ; jl 8E25F0
    {GA(GA_8E2660),6,{0x41,0x83,0xFE,0x05,0x7C,0x8A}},
    // 909DF0(능력 탭 표시) 안: lea r14,[로컬 번호 표] ; mov qword [rsp+30h],5
    {GA(GA_90A23C),16,{0x4C,0x8D,0x35,0x45,0x2E,0x18,0x01,0x48,0xC7,0x44,0x24,0x30,0x05,0x00,0x00,0x00}},
    // 2C1360(범용 일람 초기화: 호출자가 준 탭 표 +0x150, 열 정의 표 → r13) 안: mov rax,[rbp+77h] ; mov [rsi+150h],rax ; mov r13,[rbp+7Fh]
    {GA(GA_2C13B3),15,{0x48,0x8B,0x45,0x77,0x48,0x89,0x86,0x50,0x01,0x00,0x00,0x4C,0x8B,0x6D,0x7F}},
    // 4CCB90(편집 장수 일람 CCmnInfoList<CEditPersonData> 가상 +0x1E0, 2C1360과 같은 코드) 안: 같은 세 명령. 출처: 통합패치
    // SAN14_Traits9_UI.dll v2.6 TABLE9(1형 표 개성 열 0x23~0x27 뒤에 4열)와 같은 일을 범위 A의 중계 방식으로.
    {GA(GA_4CCBE3),15,{0x48,0x8B,0x45,0x77,0x48,0x89,0x86,0x50,0x01,0x00,0x00,0x4C,0x8B,0x6D,0x7F}},
};
// 5 → 9 값 바꾸기 5곳 (원작 함수 안 개성 칸 수 한계).
static const Trait9Bytes traits9_core[]={
    {GA(GA_4A90B3),8,{0x48,0xC7,0x45,0xDC,0x05,0x00,0x00,0x00},{0x48,0xC7,0x45,0xDC,0x09,0x00,0x00,0x00}},   // 4A8E30 안: mov qword [rbp-24h],5 → 9
    {GA(GA_4A96D3),8,{0x48,0xC7,0x45,0xDC,0x05,0x00,0x00,0x00},{0x48,0xC7,0x45,0xDC,0x09,0x00,0x00,0x00}},   // 4A9450(4A8E30과 같은 모양) 안: 같은 값
    {GA(GA_226FBF),4,{0x41,0x83,0xFC,0x05},{0x41,0x83,0xFC,0x09}},                                           // 226DB0 안: cmp r12d,5 → 9
    {GA(GA_2320AA),3,{0x83,0xFB,0x05},{0x83,0xFB,0x09}},                                                     // 232030(무장 n번째 개성) 안: n>4면 0 → n>8면 0
    {GA(GA_234737),4,{0x48,0x83,0xFB,0x05},{0x48,0x83,0xFB,0x09}},                                           // 2346A0(개성 보유 판정) 안: 6~9칸을 0으로 보던 한계 5 → 9
};
