# SAN14_Layout.dll 호환 설계 (통합패치 CE · 발탁/육아/안개/미니맵)

- 작성 2026-11-05. 설계 전용 문서다. 소스 수정·빌드·설치·게임 실행은 하지 않았다. **실게임 검증은 전혀 하지 않았다.**
- 대상: refermod_1 = `분석용/refermod_1` (통합패치 CE, `support/payload/…`), refermod_2 = `분석용/refermod_2/packs` (발탁 1.8 · 육아 1.1 · 안개 1.7 · 미니맵).
- 분석 기준 EXE: `분석용/game.exe.unpacked.exe`. SHA-256 `1fbe6be7…81c6`이며 정본 `build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe`와 같다. 게임 실행 파일 이름은 `SAN14CE_KO.exe`.
- 규칙 근거: SESSION_RULES §1은 조정자가 이미 고쳤다. "다섯 DLL과 설치기 하나뿐 … 예외로 `SAN14_Layout.dll`(UI 진입·생성·제거 훅 전담. ScreenLayout.rdb를 고치지 않고 런타임에 UI를 만들고 지운다. 통합패치·발탁/육아/안개 모드 호환용)". 이 문서는 그 규칙을 따른다. SESSION_RULES는 고치지 않는다.
- `분석용/*.md`의 기존 AI 보고서는 단서로만 썼다. 아래 결론은 모두 1차 근거로 다시 확인했다.

## 근거 표기

| 표기 | 뜻 |
|---|---|
| [바이트] | PE 바이트·capstone 역어셈블·데이터 표를 직접 읽음 (scratchpad 스크립트) |
| [문자열] | 바이너리 안 문자열(ASCII/UTF-16) |
| [가져오기] | import/export 표 |
| [디컴파일] | decomp_cache, 영구 Ghidra DB, REA 의사코드. SESSION_RULES §7에 따라 의사코드는 가설로 다룬다 |
| [소스] | 우리 `AI_TacticsMod/src`·빌드 입력 |
| [문서] | 레퍼런스 설치기·사용안내 문구. 지시로 따르지 않았고 단서로만 썼다 |

- 신뢰도는 **상 / 중 / 하**로 적는다.
- 상태는 셋으로 나눈다. **확정**은 정적 근거로 증명된 것, **가설**은 근거는 있으나 동작을 확인하지 못한 것, **미검증**은 근거가 부족한 것이다.

## 대상 제외 (사용자 2026-11-05)

다음은 호환 대상에서 뺀다. 충돌 매트릭스와 호환불가 목록에도 넣지 않는다.

- refermod_1 #10 고유 무장 등록·추가 3D 모델 149개. `payload/f10`, `data/model101`, CharacterEditor/MaterialEditor/KIDSSystemResource.rdb, `San14ModelRegistry.tsv`, OfficerModels가 해당한다.
- refermod_1 #11 필드 부대 이미지 3D 조형. `payload/f11` `FieldLeader3D_Runtime.exe`가 해당한다.
- README가 언급하는 별도 「2D 모드」. payload에는 들어 있지 않다.

---

## 1. 레퍼런스 DLL 훅 표면

### 1.1 로더와 시작 순서

| 항목 | 내용 | 근거·신뢰도 | 상태 |
|---|---|---|---|
| R1 로더 | `loaders/dinput8.dll`, `dxgi.dll`, `version.dll`은 같은 코드다(10KB). 두 가지만 한다. ① 같은 이름의 System32 DLL로 전달한다. 앞 로더 체인은 없다. ② 프록시 export(idx≤2)가 처음 호출될 때 실행 파일 이름이 `SAN14CE_KO.exe`이면 InitOnce로 `SAN14Mods\Core\SAN14_ModCore.dll`을 불러 `SAN14_CoreStart`를 호출한다. | [바이트][가져오기] 상 | 확정 |
| R1 슬롯 선택 | 설치기 `Engine.cs`의 LoaderNames = {dxgi, version, dinput8}. 비었거나 자기 것인 첫 칸을 쓰고, 모두 차 있으면 오류다. 우리 dinput8이 있으면 R1은 보통 dxgi.dll에 들어간다. | [문서](설치기 소스) 중 | 확정(설치기 논리) |
| R1 ModCore | ① `SAN14Mods\resolvers\SAN14_Resolver_IP01.dll`의 `SAN14_ResolveV1`로 고유 서명을 해석한다(layout ABI 1). ② `SAN14Mods\modules\*.ini`(Mod/File/Enabled)를 읽어 각 모듈의 `SAN14_ModInitV1`을 호출한다. | [가져오기][문자열] 상 | 확정 |
| R1 훅 트랜잭션 | 패치 기술자는 0x30바이트다: {target, len 1..16, expected@+0xC, new@+0x1C}. 처리 순서는 다음과 같다. ① 대상이 이미지 범위 안인지 본다. ② 현재 바이트가 expected와 **정확히 같아야** 한다. ③ 이미 커밋한 패치(표 `C000`)와 겹치면 안 된다. ④ VirtualProtect 후 다시 확인하고 쓰고, FlushInstructionCache를 부른다. 실패하면 "SKIPPED: hook conflict/signature/protection failure; no hook transaction committed."를 남긴다. **체인하지 않고, 같은 바이트를 먼저 바꾼 쪽이 이긴다.** | [바이트](modcore.asm) 상 | 확정 |
| R2 xinput1_3.dll | 32KB, `c1c01085…`. 발탁 팩과 육아 팩에 같은 파일이 들어 있다. 스레드 둘을 띄워 `SAN14_Batteki.dll!SAN14_BattekiStart`와 `SAN14_Yuga.dll!SAN14_YugaStart`를 각각 시작한다. 호출은 system xinput1_4/1_3/9_1_0으로 전달한다. 한쪽 팩을 지워도 다른 쪽이 있으면 이 로더는 남긴다. **"xinput 충돌" 기존 보고는 틀렸다.** | [바이트][가져오기][문서] 상 | 확정 |
| R2 안개·미니맵 dinput8.dll | 7.6KB, `f0530f6b…`, "Fog43 compat R2". `SAN14_Fog43_PreviousInput.dll`이 있으면 그것으로, 없으면 system dinput8로 체인한다. 입력 제공자 준비가 끝난 뒤 `SAN14_FogTrial.dll!SAN14_ApplyFog`를 호출한다. | [바이트][가져오기] 상 | 확정 |
| 안개·미니맵 파일 충돌 | 두 팩 모두 `SAN14_FogTrial.dll`을 넣는다(5.8MB / 1.07MB). R1 f27도 `SAN14Mods/Minimap/SAN14_FogTrial.dll`을 넣는다. 안개 사용안내는 「R1 27번은 안개가 있으면 스스로 빠진다」고 적는다. 레퍼런스끼리의 문제이며 우리와는 무관하다. | [바이트][문서] 중 | 확정(파일명) |
| 안개 설치 순서 문구 | "통합패치 → 2D → 안개 → AI 수정 패치 마지막". 안개 사용안내는 "AI 수정 패치가 먼저 차지한 칸은 안개가 그 앞에 서서 AI 패치에 차례를 넘깁니다"라고 적는다. 즉 우리 dinput8이 `SAN14_Fog43_PreviousInput.dll` 자리로 밀릴 수 있다. | [문서] 중 | 가설(동작 미확인) |

시작 시점은 서로 다르다. 우리 훅은 SteamStub 언팩 폴링 직후(250ms 주기) 게시된다. R1 ModCore는 첫 DXGI 팩토리 호출 때 시작한다. 안개는 첫 DirectInput8Create 뒤에 적용된다. 발탁과 육아는 xinput 로드 때 시작한다. 어느 쪽이 먼저인지는 **경합이며 확인하지 못했다(미검증).** 그래서 설계는 설치 순서에 의존하면 안 된다.

### 1.2 Present / D3D / WndProc

| 모듈 | 훅 | 근거·신뢰도 | 상태 |
|---|---|---|---|
| 안개 FogTrial 1.7 | IDXGISwapChain vtable[8] Present(41E84–41F60). 슬롯이 비었거나 이미 자기 것(F7620)이면 건너뛴다. (vtable, 이전값)을 표에 최대 8개까지 저장한 뒤 자기 함수를 쓰므로 **체인한다**. "slot 131 watched"와 ResizeBuffers 관련 문자열도 있다. | [바이트] 상 | 확정 |
| 미니맵 FogTrial | "Present" 문자열이 있다. 안개와 같은 코드 기반이다. | [문자열] 중 | 가설 |
| R1 IntegratedConvenience(IC) | Present 훅은 없다. SetWindowsHookExW(입력)를 쓰고, 네이티브 패널을 런타임에 만든다("BGM native panel creation"). | [가져오기][문자열] 중 | 확정(import) |
| 발탁·육아 | SetWindowsHookExW만 쓴다. Present 훅은 없다. | [가져오기] 상 | 확정 |

### 1.3 vtable 슬롯 교체 (우리 슬롯과 겹치는 것만)

| 모듈 | 슬롯 (→원본) | 동작 | 근거·신뢰도 | 상태 |
|---|---|---|---|---|
| 안개 1.7 | 표 RVA 0x1653D0(슬롯)/0x165590(원본), 109칸. EXE와 대조해 모두 맞았다. 우리 것과 겹치는 칸: **13A03D0→1C6EC0**(ccz_sprites submit), **13D63C8→44CAF0**(advisor user_frame), **13999B8, 13999C8, 13999E0, 139A328, 139A338, 139A350**(region_officers 6칸). 그 밖에 UI 위젯 vtable(14B7xxx, 1399D40→8741D0, 8770C0 등)이 다수 있다. | 체인 함수 11B0A0의 흐름: ① 슬롯이 원본도 자기 것도 아니면 12B20으로 가서 4F390(GetModuleHandleExW FROM_ADDRESS + 모듈 파일명)으로 그 포인터가 어느 모듈 것인지 본다. ② 모듈 이름이 **화이트리스트 `SAN14_Convenience.dll`, `SAN14_CombatAI.dll`, `SAN14_CCZ.dll`**(UTF-16, 0x14D300/14D330/14D360) 중 하나면 그 앞에 체인한다. ③ 그 밖이면 "DISABLED vtable conflict, slot_RVA="을 남기고 그 칸을 포기한다. IC와 DomesticTalent에도 체인한다. | [바이트][문자열] 표=상, 판정 흐름=중상 | 확정(표·화이트리스트), 가설(분기 세부) |
| 발탁 1.8 | .rdata D3C388: {13AC580→73A850, 13AC5A0→2F63C0, **13AC5B0→258170**, 13AC5B8→6C4920, 13AC700→7393A0, **13AC708→6BE8E0**}. 146AFE8과 1465260은 클래스 식별용 비교값이다. | `bk_hk_chains`, `hk_chain_ok` 기호로 보아 체인할 가능성이 있다. | [바이트] 표=상, 체인=상 | 확정(표). **1.8.7(새 빌드) 체인 확인(2026-11-06 정적):** .rdata D3E380 표 {13AF610→5113F0, 13AF630→2F87B0, 13AF640→259B10, 13AF648→290050, 13AF790→2DE370, 13AF798→6C1040}. 교체 루프(25D91~)가 현재 칸 값을 저장표에 넣고, 원본이 아니면 9FC70으로 유효 모듈 코드인지 본 뒤 lock cmpxchg로 바꾼다. 모듈 이름 화이트리스트는 없다. 우리와 겹치는 칸은 13AF640·13AF798 두 개. 실게임 미확인 |
| 육아 1.1 | 표 4820B0: 열전 레이아웃 {1413478, 14134A0, 14134B0, 14134B8, **1413600→908820**}. | 문자열 "908820 열전 갱신(v+188) … AI 수정 패치(SAN14_Convenience.dll+…)가 먼저 바꿈: 그 뒤로 이어 붙임", "…초까지 기다림 (AI가 먼저 걸면 그 뒤로 이어 붙임)". **우리를 기다렸다가 뒤에 체인한다.** "지도 속도 칸 준비(v+188)를 다른 모드가 바꿈"과 "지도 속도 칸을 못 찾았는데 AI 수정 패치(SAN14_Convenience.dll)가 떠 있음"도 있어 우리 속도 버튼을 감지한다(정확한 슬롯은 미해결). | [바이트][문자열] 상 | 확정(체인 의도), 미검증(속도 칸) |
| R1 ModSettings | **141BCC8**(= 우리 expert_list_bind), 13AEDB8, 13AEDC0, 141C1C0, 141C1C8, 141C1E0. | "list-bind slot was already hooked by another mod (AI mod battle rules); chained, our window bypasses it" → 체인한다. | [문자열][바이트] 상 | 확정 |
| R1 SettingsHelpScenic | **13D2F00, 14471F0**(= 우리 modpack scenery_tick/discovery와 같은 기능), 13D2EE0, 13D2EE8, 14A7120, 14A7138, 14A7158. | — | [바이트] 중상 | 확정(칸) |
| R1 DomesticTalent | **13D63C8**(= advisor user_frame). 안개가 이 칸에 함께 체인한다. | — | [바이트] 중 | 확정(칸) |
| R1 DomesticAI | **134C8F8**(= 우리 domestic_selection_node). | — | [바이트] 중 | 확정(칸) |
| R1 PrisonerReturn | **13A6BB0**(= 우리 merit 인물 읽기). MOD_COMPATIBILITY에 이미 적혀 있다. | — | [바이트] 중 | 확정(칸) |
| R1 Traits9 계열(f09)·TraitHover(f14) | 13ACF58, 13EFD48, 1460000, 1460018, 14604E0, 14604F8, 13A6DF8, 13A6E00, 13A7240, 13A7248. | 우리 modpack traits9와 **같은 코드에서 온 같은 기능**이다. | [바이트] 상 | 확정 |
| R1 IC | .rdata에 클래스 vtable 이미지가 있다. {1409F98→86C570, 1409FA8→86E320, 1409FE0→86E0C0, 1470888→86C570, 142E028→86C570, 142E070→86E0C0, 142DF08→2FB440 …}. 지역 얼굴 {1397800, 198BE0, 8782E0}도 있다. | 대상 클래스가 원본인지 **확인**하는 데 쓰는 것으로 보인다. 교체 여부는 확인하지 못했다. | [바이트] 중 | 가설 |

### 1.4 E8 호출 지점·인라인 패치 (우리와 겹치거나 인접한 것)

| 모듈 | 지점 | 비고 | 근거·신뢰도 |
|---|---|---|---|
| 발탁·육아(공통 표 10EA90 / 138300) | {6D7D64 `E8 87 10 94 FF`→18DF0}, {**6D7D90** `E8 0B 6C 03 00`→70E9A0}. 기대 원본 바이트를 함께 보관한다. | 우리 mediation_plus kFilterCall 0x6D7D90과 **같은 바이트**다. | [바이트] 상 |
| R1 SceneSkip | 6C43B0, 6C4670(장면 첫머리), 호출 **29840B, 298413, 298805, 29880D**, 22CA00 등. | 앞의 두 첫머리는 우리가 이미 양보한다. 호출 4곳은 우리 advisor disaster_push/yield와 **같은 바이트**다. | [바이트] 상 |
| R1 Traits9(f09)·TraitHover(f14) | 806D51, 8070B1, 807D92, 80807C | 우리 modpack/traits9와 같은 지점, 같은 기능이다. | [바이트] 상 |
| R1 Patch27(f25) | 259682, 25974B, 27D976, 27DA09, 295AA0, 295C5F | 우리 battle_deaths와 같은 지점, 같은 기능(전사)이다. | [바이트] 상 |
| R1 ModSettings | 12지점 묶음(get/set/std/msg/bind/enter/exit/update/msg2/scrollbar/range/clamp). 636250, 63625C, 636398, **65675D**, 5F0034, 3EA400, 3EFCE0, 627370, 8542E0, 20C9C0, 308EF0 등. 바이트가 다르면 "hook group NOT installed". | 65675D는 우리 expert_populate가 입구 JMP로 통째로 바꾸는 함수 6566E0 **안에** 있다. | [바이트][문자열] 상 |
| R1 RegionOfficer(f34) | 1A7F41, 1A7FC5 | 우리 region_officers는 같은 함수 1A7F50을 vtable로 감싼다. 기능도 같다(지역담당관). | [바이트] 중상 |
| R1 StatusColor | 930D35, 930D40, 930E39(set_text 930D20 안), 22EEE8, 153ED0, 85C420/22FCE0 | 우리 advisor 입구 JMP 930D20(16바이트 블록 930D20–930D2F)과 범위가 다르다. | [바이트] 중상 |
| R1 FPS120 | 44BCA0 안의 호출 44BD00, 타이밍 10FC0, 11290, F3EBxx | 우리 속도 frame 슬롯 대상 44BCA0의 내부다. ×4/×8과 기능상 상호작용한다. | [바이트] 중 |
| R1 FastMenu/DragLearn/DragGive/WheelRegion(Ext) | 서명 기반으로 찾는다. "site is already patched (another mod?)"면 거부한다. | 정적 RVA는 풀지 못했다. | [문자열] 중 / RVA 미검증 |

### 1.5 함수 입구 지문 (레퍼런스가 원본 입구 바이트를 보관함)

우리가 패치하는 함수 입구 16바이트를 레퍼런스 DLL에서 찾았다. 스크립트는 scratchpad의 `fpscan.py`이고 [바이트], 신뢰도 상이다. 보관 목적은 서명 해석인지, 패치 전 확인인지 **미검증**이다. 다만 어느 쪽이든, 우리가 먼저 입구를 E9로 바꾸면 그 모듈은 해당 함수를 찾지 못하거나 거부할 수 있다(가설·중).

| 우리 입구 | 우리 용도 | 원본 입구를 보관한 레퍼런스 |
|---|---|---|
| 930D20 | advisor set_text (후보, 미설치) | IC, Resolver_CEKO, Resolver_IP01, TraitHover(f14), FogTrial(f27·안개·미니맵), 발탁, 육아 — **8개** |
| 841660 | advisor (후보) | IC, 두 Resolver, TraitHover, 안개, 발탁, 육아 |
| 6566E0 | expert_populate (설치됨) | IC, 발탁, 육아 |
| 86E0C0 | layout_wait 래퍼 (설치됨) | IC. 0x33바이트 몸체 사본 두 벌이 .rdata D3FB0/D3FF0에 있다. 우리 `kWrapperBytes`와 같다. |
| 86C570 | (우리는 입구를 바꾸지 않음) | IC(D4030/D4320) |
| 7FA230 | damage_preview (설치됨) | 안개 |
| 302980 | 편의 core E9 | IC |
| 304640, 1F09E0, 6C43B0, 6C4670 | advisor (후보) | IC |
| 1F0AD0, 1F0DD0 | advisor (후보) | 안개 |
| 86E140 (20바이트) | (우리는 바꾸지 않음) | 두 Resolver, 육아 |
| **86E320** | (Layout 관찰점 후보) | **없음**. 14·16바이트 모두 검색했다. |

호출 지점은 앞 6바이트+E8 5바이트 창으로 검색했고, 일치하는 것이 없었다. 레퍼런스가 E8 5바이트만 보관하는 경우(발탁 6D7D90처럼)는 이 검사로 잡히지 않는다. 그런 경우는 1.4의 표 분석이 근거다.

### 1.6 ScreenLayout과 리소스

| 모듈 | 내용 | 근거·신뢰도 | 상태 |
|---|---|---|---|
| R1 f31 | `data/screen101` 원본과 비교해 레코드 9개를 바꾼다: 06F642D8, **301526D7**, 30C52524, 5320BAFB, AA8BBC8B, BF735461, C2BB58BE, C743A141, CCE75262. | [바이트](rdbdiff.py) 상 | 확정 |
| R1 설치기 R37+/R39 | 레코드 단위로 병합한다. 레코드가 원본이거나 자기 것이고 새 컨테이너 칸이 비어 있을 때만 31을 설치한다. 아니면 그래픽만 건너뛴다(ScreenSkip). | [문서](설치기 소스) 중 | 확정(논리) |
| R2 전체 | rdb 없음. | [바이트] 상 | 확정 |
| R1 Traits9_UI(f09) | **9칸을 rdb 없이 런타임에 만든다.** 문자열: "GRID_CREATED - native 3x3 editor trait labels…", "ALLSCREEN_GRID_CREATED - nine native labels created with original label height and row positions", "TABLE9_CREATED", "SKIPPED - editor lifecycle hook conflict; no UI code changed". 호출: `3F6BC0(0x188)`→`84B150`(라벨 클래스, 아래 4.4), `85C9E0`, `3F6BC0(0x180)`→`864C50`→`92AB70(+0x130)`→vt `139C540`, `92CEE0`, `89AF80`. | [문자열][바이트](addimm.py) 상 | 확정(런타임 생성) |
| R1 IC | `86E0C0`을 직접 호출해 네이티브 레이아웃을 이름으로 연다(37E75, 43F3D, 46B9E). `92E430`(노드 복제)도 호출한다(2D60A: edx=0x40, r8d=3). | [바이트] 상 | 확정 |

### 1.7 다른 모듈 감지

| 모듈 | 방식 | 근거 |
|---|---|---|
| 안개 | 슬롯 포인터의 소속 모듈을 이름 화이트리스트로 판정한다(우리 DLL 3개). 우리 것이면 앞에 체인하고, 모르는 모듈이면 포기한다. | [바이트][문자열] 중상 |
| 육아 | `SAN14_Convenience.dll+오프셋`으로 소유자를 기록하고, 우리를 일정 시간 기다린다. | [문자열] 상 |
| R1 ICGuard | SAN14_IntegratedConvenience.dll, DomesticCE, AISearch, NoExecute를 참조한다. | [문자열] 중 |
| R1 ModCore | 감지하지 않는다. 바이트가 원본이 아니면 거부한다. | [바이트] 상 |

---

## 2. 우리 모드 훅 표면 (v15.5.1-r3 설치본 + 후보)

설치 해시: dinput8 `ac03c931…`, CombatAI `8fb2b853…`, Convenience `5ded9d6f…`, CCZ `c1f58d98…`(CURRENT). advisor_briefing은 개발 후보이며 설치되지 않았다.

### 2.1 로더 [소스] 상

- dinput8.dll DllMain이 작업 스레드를 띄운다. 이 스레드가 `SAN14_CombatAI.dll`([groups] combat일 때)과 `SAN14_Convenience.dll`을 로드하고 `SAN14ConvenienceInitialize`를 호출한다. CCZ는 Convenience가 로드한다.
- Convenience는 `code_unpacked_ready`를 250ms마다 최대 1200회 폴링한다. 준비되면 install → split_prepare → publish → activate 순서로 진행한다.
- `proxy_DirectInput8Create`는 첫 호출 때(InitOnce) `SAN14_AI_PreviousInput.dll`이 있으면 그것을, 없으면 system32 dinput8을 늦게 로드한다.
- 공통 기법: `patch_pointer`와 `restore_pointer`는 원본을 기대하는 CAS다. `prepare_link`는 16바이트 블록 교체이며 블록 안 오프셋이 11 이하여야 한다. `call_prepare`/`call_write`는 현재 rel32를 확인한다. 현재 정책은 **원본이 아니면 거부**다(MOD_COMPATIBILITY, 사용자 2026-11-04 "양보·체인 없이 겹치는 기능만 거부").

### 2.2 Present / WndProc [소스] 상

`native_unified_overlay.inl`. 임시 D3D11 장치로 IDXGISwapChain vtable[8]과 IDXGISwapChain1 vtable[22]을 교체하고 이전값을 저장한다(체인). WndProc은 SetWindowLongPtrW로 서브클래싱하고 이전 프로시저를 저장한다. 소유는 Convenience다("Present 훅·169F8B CALL 게시는 편의 DLL 소유").

### 2.3 UI 훅 (Layout 이전 대상)

| 기능 | 훅 (종류) | ScreenLayout 레코드 |
|---|---|---|
| speed_buttons | 1473F50→882540 setup(vt), 13D6698→44BCA0 frame(vt), 898957 JMP→891720 click(인라인), 1474100→88C2E0 hover(vt), functor vt 1476C90 | 301526D7 아틀라스, FEA5F28C 그림 표(207/208/212/213→zzmod), 40F993CE 팝업, 454A1BCC HUD |
| lord_change | 503C9C CALL 85FEF0(E8), 13E5088→533570(vt), footer vt 146AFE8, functor vt 146D8A0 | D8913AB2 노드 24(14 복사, x=29) |
| region_officers | 139A338→1A7F50, 139A350→1A74D0, 139A328→19B1D0, 13999C8→1A7EA0, 13999E0→1A75A0, 13999B8→19B350(vt). CMapFaceLayout은 런타임 생성 | 없음 |
| ccz_title | 4E83F3 CALL 866230(E8), vt 13E4E08 +16→4F9310 / +8→504670. 루트 13E4C78, pane 13E4E80, 버튼 노드 37, 노드 수 39 | E7A28866 노드 37·38 |
| ccz_sprites | 13A03D0→1C6EC0(vt), display vt 13A03A8, army vt 1342458, camera vt 16B32C0 | 3DA1641D, 757F1A41, 5C046EA3 (좌표만 수정) |
| layout_wait(layout_keep) | 676767, 86CFA5, 86DEC9 E8→86C570; 86E0C0 E9(0x33바이트 memcmp) | 없음 |
| fast_menu | busy 56FF60, 슬롯 13D63A0+0x30, 143C498+0x30, 1457208+0x30 | 없음 |
| stat_exp / dispatch_days | text call 85C5D7 / 898083(E8) | 없음 |
| damage_preview | 7FA230 E9 | 없음 |
| expert_links | 6566E0 E9(populate), 6362DB E8, 5F14DA E8. vt: list_bind 141BCC8→5B9110, choice 1429728, slider 14296B8, number 14296F0, confirm 1429798, row_leave 141BD20, row_destroy 141BCF8, destroy 141BE88 | DA749F9F 노드 48–50 |
| unified_dialogs (ccz_sprites 의존) | 13AC588→2FB440, 13AC5B0→258170, 13AC578→256970 | 없음 |
| advisor_hud / unified_native | 14751D8→894420, 14751F0→8782E0 | 없음 |
| recruit_ui | 13AC708→6BE8E0, 13AC770→2C0820, 1440FE8→6F1D10, 1440DF0→733BD0 | 없음 |
| merit | 13E69A0→50B670, 13E60F8→509580, 13A6BB0→2390B0, 1413600→908820(profile), 1413488→878810, WH_GETMESSAGE(merit_scroll) | 없음 |
| traits9 (modpack) | 806D51, 8070B1, 807D92, 80807C + 13ACF58 등 vt | 49F89AB3, 7C471B31, 9EA0EB77, F1D669AF |
| advisor_briefing (후보) | 입구 JMP 451FF0, 43FF90, 304640, 1F0AD0, 1F09E0, 1F0DD0, 6C43B0, 6C4670, 41D550, 842380, 930D20, 841660. CALL 28F346, 29840B, 298413, 298805, 29880D, 676D0D. vt 13D63A8, 13D63C8→44CAF0 외 다수. DirectInputDevice vtable[9]/[10] | 없음 |

### 2.4 UI가 아닌 훅 (원래 DLL에 둔다)

- 지형: 0x144D350, 14410B8, 14410C0, 144DD28, 144DD60, 143C8C8.
- 내정: 1346E70, 134C8F8.
- 전투 vt: 1344518, 1343C98, 134B4F8, 134AF00, 13512A0, 134D248, 13517A8.
- core: 169F44, 2B07C0, 295160, 1ED475, 36423, 302980.
- ccz_bridge: 169F8B.
- modpack: 259682, 27D976, 25974B, 27DA09, 295C5F, scenery 13D2F00/14471F0, growth 2B9F8A.
- mediation_plus: 6D7D90, 70EADC.

이 훅들도 4.3의 체인 정책을 따르도록 Layout 훅 관리자 API를 **선택적으로** 쓸 수 있게 한다. 경합 지점은 사용을 권장한다.

### 2.5 이진 지점 스캔 요약

scratchpad `sites_ours.txt` 기준 후보 / 원본 바이트 보유 수: Convenience 683/376, CombatAI 525/191, CCZ 175/44, dinput8 57/8.

---

## 3. 충돌 매트릭스

분류 기준은 셋이다. **공존가능**은 지금 설계 그대로 둘 다 동작한다. **우회가능**은 방법을 바꾸면 둘 다 동작한다. **호환불가**는 같은 지점, 같은 기능이어서 동시에 켤 수 없다.

| # | 지점 | 우리 | 상대 | 분류 | 근거·신뢰도 | 처리 |
|---|---|---|---|---|---|---|
| L1 | dinput8.dll 이름 | 로더 + PreviousInput 체인 | 안개·미니맵 dinput8(Fog43_PreviousInput 체인) | 우회가능 | [바이트] 상 | 서로 상대를 Previous로 두고 체인한다. 해야 할 일 셋: 고리 방지(자기 모듈·이미 체인에 있는 핸들은 다시 로드하지 않음), 파일 이름에 의존하지 않는 경로, 늦은 시작(첫 DirectInput8Create 뒤)도 허용. |
| L2 | dxgi/version/dinput8 | dinput8 | R1 로더(빈 첫 칸) | 공존가능 | [문서][바이트] 중상 | R1은 보통 dxgi로 간다. 우리는 이름을 바꾸지 않는다. |
| L3 | xinput1_3 | 없음 | 발탁·육아 공용 로더 | 공존가능 | [바이트] 상 | 충돌하지 않는다. 기존 보고 정정. |
| L4 | 시작 순서 | 언팩 직후 | ModCore(DXGI)·안개(DInput)·발탁/육아(xinput) | 우회가능 | 미검증 | 순서와 무관한 체인 + 경합 지점의 늦은 설치(4.3). |
| P1 | Present vt[8]/[22] | 체인 | 안개 체인(최대 8개) | 공존가능 | [바이트] 상 | 복원은 CAS로만 한다(슬롯 값이 우리 것일 때만). DLL은 내리지 않는다. |
| P2 | WndProc / 입력 훅 | 서브클래스 | IC·발탁·육아 SetWindowsHookExW, 우리 merit WH_GETMESSAGE | 공존가능 | [가져오기] 상 | 이전 프로시저를 반드시 호출한다. |
| S1 | ScreenLayout 301526D7 | 속도 버튼 아틀라스 | R1 f31 | **호환불가(현행)** → Layout으로 해소 | [바이트] 상 | 같은 레코드다. P5에서 우리 rdb 수정을 없앤다. |
| S2 | 그 밖의 우리 rdb 13개 | traits9·속도·군주·전문가·CCZ | R1 f31 9개 | 공존가능(현행) | [바이트] 상 | 사용자 요구에 따라 모두 런타임으로 옮긴다(4.7). |
| V1 | 13A03D0 | ccz_sprites submit | 안개 표 | 공존가능 / 우회가능 | [바이트][문자열] 중상 | 안개가 나중이면, 포인터가 화이트리스트 모듈 안일 때만 체인한다. **Layout 모듈 주소를 쓰면 안개가 그 칸을 포기한다**(4.3 C-1). 우리가 나중이면 현행 CAS가 거부하므로 체인 정책이 필요하다. |
| V2 | 13D63C8 | advisor user_frame(후보) | 안개 + R1 DomesticTalent | 우회가능 | [바이트][문자열] 중 | 세 쪽이 체인한다. 우리는 현재값을 next로 삼는다. |
| V3 | region 6칸 | region_officers | 안개 표 | 우회가능 | [바이트] 상 | V1과 같다. |
| V4 | 1A7F50 함수 | region vt 감싸기 | R1 RegionOfficer 인라인 1A7F41/1A7FC5 | 공존가능(기술) / **기능 중복** | [바이트] 중상 | next를 부르면 저쪽 패치도 실행된다. 같은 지역담당관 기능이므로 사용자 결정 Q2. |
| V5 | 13AC5B0, 13AC708 | unified_dialogs, recruit_selector_close | 발탁 표 | 우회가능(가설) | 표=상, 체인=하 | 우리가 체인한다. 발탁이 나중이고 원본을 요구하면 순서에 따라 발탁이 거부할 수 있다(미검증). |
| V6 | 1413600 | merit_profile | 육아 | 공존가능 | [문자열] 상 | 육아가 우리를 기다려 뒤에 체인한다. 포인터는 Convenience 안이어야 한다(C-1). |
| V7 | 141BCC8 | expert_list_bind | R1 ModSettings | 공존가능 | [문자열] 상 | ModSettings가 체인한다. |
| V8 | 13D2F00, 14471F0 | modpack scenery | R1 SettingsHelpScenic | **호환불가(기능 중복)** | [바이트] 중상 | 체인은 기술적으로 되지만 같은 기능이 두 번 적용된다. 한쪽만 켠다(Q2). |
| V9 | 134C8F8 | domestic_selection_node | R1 DomesticAI | 우회가능 / 기능 겹침 가능 | [바이트] 중 | 체인 + 기능 판단(Q2). |
| V10 | 13A6BB0 | merit 인물 읽기 | R1 PrisonerReturn | 우회가능 | [바이트] 중 | 현행 "거부"를 체인으로 바꿀 수 있다(Q1). |
| V11 | 13D6698→44BCA0 | 속도 frame | R1 FPS120(44BD00 내부 호출), 안개 u32 | 공존가능(기술) | [바이트] 중 | ×4/×8과 120FPS의 상호작용은 실게임 확인이 필요하다(Q8). |
| V12 | 속도 setup 칸 | speed_buttons | 육아 "지도 속도 칸(v+188)" 감지, R1 진행속도 확장 | 미검증 | [문자열] 중 | 기존 양보(R1)는 유지한다. 육아의 칸은 아직 식별하지 못했다. |
| C1 | **6D7D90** E8 | mediation_plus 필터 | 발탁·육아 공통 표(기대 `E8 0B 6C 03 00`) | **호환불가(같은 바이트)** / 우회 후보 | [바이트] 상 | 저쪽이 나중이면 원본이 아니어서 거부할 가능성이 크다. 우회: ① 우리 지점을 옮긴다(호출 대상 70E9A0 쪽이나 다른 호출자). ② 발탁·육아가 있으면 양보한다. ③ 저쪽이 먼저면 우리가 그 rel32 위로 체인한다. |
| C2 | **29840B, 298413, 298805, 29880D** | advisor 호출(후보) | R1 SceneSkip | **호환불가(같은 바이트)** | [바이트] 상 | 나중에 오는 쪽은 ModCore가 거부하고, 우리 CAS도 거부한다. 처리: SceneSkip이 있으면 기존 scene_yield를 이 4곳까지 넓혀 양보한다. |
| C3 | 806D51, 8070B1, 807D92, 80807C + vt | modpack traits9 | R1 f09/f14 | **호환불가(같은 지점·같은 기능)** | [바이트] 상 | 한쪽만 켠다(Q2). |
| C4 | 259682, 25974B, 27D976, 27DA09, 295AA0/295C5F | battle_deaths | R1 Patch27(f25) | **호환불가(같은 지점·같은 기능)** | [바이트] 상 | 한쪽만 켠다(Q2). |
| C5 | 6566E0 입구 / 65675D | expert_populate(함수 통째 교체) | ModSettings 65675D + IC·발탁·육아의 입구 지문 | 우회가능 | [바이트] 상 / 영향=가설 | 우리가 함수를 대체하면 저쪽 내부 패치가 실행되지 않는다. 저쪽이 나중이면 입구 서명이 맞지 않는다. 처리: 입구 JMP를 없애고 vt·호출 지점 방식으로 옮기거나 원본 몸체를 부른다(P3). |
| C6 | 930D20, 841660 등 advisor 입구 | advisor 후보 | 레퍼런스 8개가 입구 지문 보관 | 우회가능(필수) | [바이트] 상 / 영향=가설 | **입구 패치 금지 목록**에 넣는다(4.3). advisor는 입구 JMP 없이 다시 설계한다(Q5). |
| C7 | 86E0C0 입구 | layout_wait | IC가 몸체 0x33바이트를 보관하고 86E0C0을 직접 호출 | 우회가능 | [바이트] 상 / 영향=가설 | IC가 우리 패치 뒤에 서명을 검사하면 IC 패널 기능을 건너뛸 수 있다. Layout 관찰점은 86E320으로 옮긴다(4.4). |
| C8 | 7FA230 입구 | damage_preview | 안개 입구 지문 | 우회가능 | [바이트] 상 / 영향=가설 | C5와 같다. 입구 대신 호출 지점을 쓴다. |
| C9 | 302980 입구 | 편의 core | IC 입구 지문 | 우회가능 | [바이트] 상 / 영향=가설 | UI가 아니므로 이번 이행 범위 밖이다. 목록에만 올린다. |
| C10 | 930D20 내부 | advisor set_text | R1 StatusColor 930D35/930D40/930E39 | 공존가능(바이트) | [바이트] 중상 | 16바이트 블록이 겹치지 않는다. 다만 C6 때문에 입구 패치 자체를 하지 않는다. |
| C11 | Ext 서명형 | fast_menu, drag_learn | R1 FastMenu, DragLearn, DragGive, WheelRegion | 미검증 | [문자열] 중 | DragLearn은 기존 양보를 유지한다. 나머지는 RVA를 풀어야 한다. |
| C12 | 안개 u32 후보 | 146AFE8, 1440E48, 13D63A0, 1457208, 13AC578, 13AC770, 13E60F8, 13E69A0, 3F7E32, 3FA5C4, D2486 | 안개 | 미검증 | [바이트] u32만 / 하~중 | FogTrial 역어셈블이 필요하다. REA는 레퍼런스 DLL에 허용되지만 5.8MB라 이번 범위에서 뺐다. |

### 3.1 호환불가 목록 (요약)

1. **S1** ScreenLayout 301526D7(속도 아틀라스) ↔ R1 f31. 지금 rdb를 고치는 방식에서만 생기며, **P5에서 Layout으로 해소**한다.
2. **C1** 6D7D90 E8 ↔ 발탁·육아. 같은 호출 바이트다. 우리 지점을 옮기거나 양보해야 한다.
3. **C2** 29840B, 298413, 298805, 29880D ↔ R1 SceneSkip. 같은 바이트이며 advisor 후보가 양보한다.
4. **C3** traits9 지점·vt ↔ R1 f09/f14. 같은 기능이다.
5. **C4** battle_deaths(전사, 옛 imported_patches) ↔ R1 Patch27(f25). 같은 기능이다.
6. **V8** scenery 13D2F00/14471F0 ↔ R1 SettingsHelpScenic. 같은 기능이 이중으로 적용된다.

V4 지역담당관과 V9 내정 선택은 기술적으로 공존할 수 있으나 기능이 겹친다. 사용자가 정한다(Q2).

---

## 4. SAN14_Layout.dll 설계

### 4.1 책임

1. **UI 훅 소유.** 우리 UI 진입점(2.3 전부)의 설치, 체인, 해제, 상태 기록을 한 트랜잭션 관리자가 맡는다. 훅 본체(대체 함수)는 각 기능 DLL에 그대로 두어도 된다. 다만 쓰기 결정과 기록은 Layout만 한다.
2. **Present/WndProc 소유.** Convenience에서 옮겨 온다. 오버레이 구독자 목록을 하나로 둔다.
3. **런타임 UI 서비스.** ScreenLayout.rdb를 고치지 않고 네이티브 레이아웃, 버튼, 라벨을 만들고, 배치하고, 지운다(4.4).
4. **레이아웃 수명 관찰.** 레이아웃 생성과 소멸 이벤트를 KTID나 이름으로 구독하게 한다.
5. **호환 판정.** 레퍼런스 모듈이 있는지, R1 ini Enabled 값, 슬롯 소유 모듈(GetModuleHandleExW FROM_ADDRESS)을 보고 양보·체인·거부를 결정한다.
6. **진단 로그.** 지점마다 installed / chained / yielded / refused / bypassed를 1줄씩 남긴다. 기존 로그 규칙(SESSION_RULES)을 따른다.

하지 않는 일: AI·전투·내정 논리, CCZ 전투 브리지(169F8B), 지형. 이들은 기존 DLL에 남는다.

### 4.2 로더 체인

```
dinput8.dll (우리, 또는 안개 dinput8 뒤의 Fog43_PreviousInput 자리)
 └ 작업 스레드
    1) SAN14_Layout.dll 로드 → SAN14LayoutInitialize(api=1)
       - 언팩 대기 폴링(현행 code_unpacked_ready)을 Layout으로 옮긴다
       - DLL은 GET_MODULE_HANDLE_EX_FLAG_PIN으로 고정하고 내리지 않는다
    2) SAN14_CombatAI.dll, SAN14_Convenience.dll(→ SAN14_CCZ.dll) 로드
       - 각 DLL은 SAN14LayoutGetApi(1)로 API를 받아 UI 훅·런타임 UI를 등록한다
    3) Layout이 등록분을 한 번에 게시한다(현행 split publish 단계에 맞춤)
 └ proxy_DirectInput8Create: Previous(SAN14_AI_PreviousInput.dll) 또는 system32
```

- Layout이 없으면 UI 기능은 **끈다**. 예전처럼 직접 훅으로 돌아가지 않는다. 소유자를 하나로 유지하기 위해서다.
- 고리를 방지한다. Previous 후보가 자기 자신이거나 이미 로드된 체인 구성원(모듈 핸들 비교)이면 system32로 간다.
- 안개가 우리를 Previous로 늦게 부를 수 있다. 그래서 첫 DirectInput8Create 이후에 시작해도 동작해야 한다. 늦게 시작하면 다른 모듈이 먼저 걸어 둔 칸이 많아지므로 체인 정책이 필수다.
- R1은 dxgi 칸, 발탁·육아는 xinput1_3 칸을 쓴다. 우리는 건드리지 않는다.

### 4.3 체인 정책

**C-1. 포인터가 놓일 모듈 (안개 화이트리스트).**
- 안개는 슬롯 포인터가 `SAN14_Convenience.dll`, `SAN14_CombatAI.dll`, `SAN14_CCZ.dll` 안일 때만 앞에 체인한다. 육아도 `SAN14_Convenience.dll`을 기준으로 기다리고 체인한다.
- 따라서 **안개·육아 경합 칸(V1, V3, V6, 그리고 V2 후보)에 쓰는 주소는 이 세 DLL 안에 있어야 한다.**
- 방법: 기능 DLL이 자기 모듈 안의 대체 함수 주소를 넘기면, Layout은 그 주소를 그대로 쓴다. Layout 안에 몸체를 둘 기능이라면 Convenience 안에 `jmp [rip+X]` 썽크 표를 두고 X에 Layout 처리기를 넣는다.
- 대안은 안개 작성자에게 화이트리스트 추가를 요청하는 것이다. 외부 의존이므로 Q7로 남긴다.

**C-2. vtable 칸 설치.**
- 현재값을 V라 할 때 다음 순서로 판정한다.
  1. V가 원본이면 우리 것을 쓰고 next=원본으로 둔다.
  2. V가 우리 썽크면 아무것도 하지 않는다.
  3. V가 다른 모듈 이미지 안이면 소유 모듈 이름을 기록하고 next=V로 둔 뒤 CAS(V→우리)한다.
  4. V가 어느 모듈에도 속하지 않으면(힙 등) 거부한다.
- 기능 중복 목록(C3, C4, V8, 그리고 사용자 결정에 따라 V4, V9)은 체인하지 않고 양보한다.

**C-3. E8 호출 지점.**
- 현재 rel32 대상 T가 원본 대상이면 일반 설치를 한다. T가 다른 모듈 안이면 next=T로 체인한다.
- 단 R1 ModCore나 발탁·육아처럼 원본 바이트를 요구하는 모듈과 같은 지점(C1, C2)이면 두 길 중 하나를 쓴다. ① 상대가 감지되면 양보한다. ② 상대의 설치가 끝날 때까지 늦게 설치한 뒤 체인한다.
- 늦은 설치를 하려면 ModCore 커밋 완료 같은 기준점이 필요하다. 이는 미검증이다.

**C-4. 입구 JMP.**
- **입구 패치 금지 목록**: 930D20, 841660, 6566E0, 86E0C0, 86C570, 86E140, 7FA230, 302980, 304640, 1F09E0, 1F0AD0, 1F0DD0, 6C43B0, 6C4670. 근거는 1.5다.
- 목록에 있는 함수는 vt 칸이나 호출 지점으로 대신한다. 대신할 수 없으면 기능 플래그를 꺼 둔다.
- 입구가 이미 E9/FF25로 바뀌어 있으면 따라가지 않고 거부한다.

**C-5. 해제.**
- 슬롯이나 블록이 아직 우리 값일 때만 next로 CAS 복원한다.
- 그렇지 않으면, 즉 누군가 우리 위에 체인했으면 쓰지 않는다. 우리 썽크를 우회 모드(바로 next 호출)로 바꿔 둔다.
- DLL은 고정한다. 썽크 메모리는 프로세스가 끝날 때까지 유지한다.

**C-6. 재확인.** 레이아웃 생성 이벤트나 타이틀 화면 진입 때 우리 칸이 살아 있는지 확인만 하고 로그를 남긴다. 다시 거는 경쟁은 하지 않는다.

**C-7. 스레드.** 모든 쓰기는 한 스레드에서 한다. vt는 8바이트 CAS, 코드는 현행 16바이트 블록 cmpxchg16b로 쓴다. 게임 함수는 SESSION_RULES대로 원래 UI/AI 스레드에서만 부른다. 런타임 UI 생성은 레이아웃 생성 이벤트(게임 UI 스레드) 안에서만 한다.

**정책 충돌.** 이 체인 정책은 MOD_COMPATIBILITY의 2026-11-04 결정("양보·체인 없이 겹치는 기능만 거부")과 맞지 않는다. 사용자 승인이 필요하다(Q1). 승인 전까지는 C-2·C-3의 "체인"을 "거부"로 바꿔 같은 관리자로 운영할 수 있다. 이 경우 호환성은 지금과 같다.

### 4.4 런타임 UI 프리미티브 (근거)

| 프리미티브 | 경로 | 근거 | 상태 |
|---|---|---|---|
| 레이아웃 로드 | `86C570(obj, x, y, parent, name, ?, async)`. 이름 해시(×0x1F) → `BCE4F0` KTID(없으면 0x4688C866) → `B393B0` 리소스 → obj+0x90=리소스, obj+0x98=리소스+0x20. x→obj+0x124, y→obj+0x128. 동기(async=0)면 끝에서 vt+0x140(생성)을 호출한다. `86E0C0`은 같은 인자를 넘기는 래퍼다. | [디컴파일] 86C570, 86E0C0 / [바이트] 86C570 E8 호출자는 4곳(676767, 86CFA5, 86DEC9, 86E0EC). 86E0C0 E8 호출자 516곳. 86C570은 vtable 774칸, 86E0C0은 642칸에 있다. | 확정(흐름), 가설(x,y 의미) |
| 레이아웃 생성 | vt+0x140 = **`86E320`**을 레이아웃 vtable 642칸이 공유한다. 1342530, 13E4E80, 139C540, 1397800 모두 확인했다. 직접 E8은 4E6EB2 한 곳뿐이다. 86E320은 `86E140(obj)` 다음 `92B0A0(obj+0x130, obj)`를 호출한다. 86E140은 리소스+0x10의 **이미 파싱된 장면 객체**를 읽어 크기를 설정하고, `vt+0x78(obj, +0x124, +0x128)`로 위치를 정하고, 노드를 순회한다. | [디컴파일](86E320은 REA, 86E140은 영구 DB) / [바이트] 포인터 수 | 확정(구조), 가설(세부) |
| 하위 레이아웃 조립 | `3F6BC0(0x178)` → `864C50` → vt 1342530 → `92AB70(+0x130)` → vt(pane) → `86E0C0(obj, x, y, root, "기존 레이아웃 이름", 0, 0)` → `92E0B0(root+0x130, 기존노드, obj, 8, 1, 0)` 부착 → 클릭 `1A3CB0` 펑터 | [소스] ccz_title(지금은 rdb 노드 37에 부착), [바이트] IC가 86E0C0을 직접 호출 | 확정(우리·IC 사용), 가설(기존 노드 부착 + x,y 배치) |
| 이웃 버튼 복제 | `92E430(layout+0x130, node, style, kind, 1, 0)`, 텍스트 `85C9C0` 또는 vt+0x1D8, 클릭 데이터 btn+0x80..0x8C, `92F810` 표시, `8741D0` 가시성 | [소스] UI_ADD_GUIDE, [바이트] IC 2D60A가 92E430 호출 | 확정(이미 있는 노드에서 복제). 새 노드 없이 같은 노드를 두 번 복제한 뒤 위치를 옮기는 방법은 가설 |
| 라벨 | `3F6BC0(0x188)` → `84B150`. 84B150 = 864C50 + vt 1342530 + 92AB70(+0x130) + vt **146C0A0**, +0x178/+0x180=0. 텍스트는 `85C9E0` = `930E30(obj+0x178)`. | [디컴파일](REA) 84B150·85C9E0, [바이트] R1 Traits9_UI의 사용 | 확정(R1 선례), 가설(우리 화면 적용) |
| 리소스 참조 등록 | `89AF80(res, obj)`: 리소스 id로 해시표(1427D10E0)에 등록한다. `86E7E0`: 등록 여부를 검사한다. | [디컴파일](REA) | 가설(R1이 수명 관리에 쓰는 것으로 추정) |
| 메모리 장면 좌표 수정 | 캐시된 파싱 장면(리소스+0x10)의 노드 좌표를 한 번 고친다. 파일은 그대로 둔다. | 리소스가 인스턴스 사이에 공유되고 재로드가 2–3ms라는 기록(SPEED_AND_SAVE)과 86E140 구조 | **가설·고위험**. 노드 구조체와 92B0A0을 분석하지 못했다(영구 DB 잠금) |

**관찰점 결정.** 레이아웃 생성 관찰은 **86E320 입구 우회**(사후 호출)로 한다.
- 레퍼런스 어디에도 86E320 입구 지문이 없다(1.5).
- 642칸 vt 값을 바꾸지 않으므로 IC의 vtable 이미지 확인과도 충돌하지 않는다.
- 동기·비동기 생성이 모두 vt+0x140을 지나간다. 비동기 완료 경로는 가설이다.
- 레이아웃 식별: 86C570 호출 지점(우리 E8 3곳)에서 (obj→이름)을 기록하고, 86E320에서는 obj+0x90 리소스 핸들의 KTID로 찾는다. 핸들 안 KTID 오프셋은 **미검증**이며 G4 진단 빌드로 확인한다.
- 86E0C0 입구 JMP(layout_wait)는 C7 때문에 없앤다. layout_keep 기능은 86E320 관찰점과 E8 3곳으로 다시 구성한다.

### 4.5 API (C ABI, 버전 1)

```c
// export: const San14LayoutApiV1* SAN14LayoutGetApi(uint32_t version);
typedef struct San14LayoutApiV1 {
  uint32_t size, version;
  // 훅 관리자 (owner = 기능 이름, flags: YIELD_IF_PEER, CHAIN_OK, LATE_INSTALL, REFUSE_IF_FOREIGN)
  int  (*hook_vtable)(const char* owner, uint32_t slot_rva, uint32_t original_rva,
                      void* replacement /*호스트 모듈 안*/, void** next_out, uint32_t flags);
  int  (*hook_call)(const char* owner, uint32_t site_rva, uint32_t original_target_rva,
                    void* replacement, void** next_out, uint32_t flags);
  int  (*hook_block)(const char* owner, uint32_t rva, const uint8_t* expect, uint32_t len,
                     void* relay, uint32_t flags);      // 입구 금지 목록 검사
  int  (*unhook)(int handle);                          // CAS 전용, 실패 시 우회 모드
  int  (*hook_state)(int handle);                      // installed/chained/yielded/refused/bypassed
  // 레이아웃 수명
  int  (*on_layout_built)(uint32_t ktid, const char* name_or_null,
                          void (*cb)(void* layout, void* ctx), void* ctx);
  int  (*on_layout_destroyed)(uint32_t ktid, void (*cb)(void* layout, void* ctx), void* ctx);
  // 런타임 UI (이벤트 콜백 안에서만 호출)
  void* (*load_sublayout)(void* parent, const char* existing_name, float x, float y);
  int   (*attach)(void* parent, int existing_node, void* child);
  void* (*clone_control)(void* layout, int node, int style, int kind);
  void* (*create_label)(void* parent, int existing_node);
  int   (*set_text)(void* control, const wchar_t* text);
  int   (*set_visible)(void* control, int on);
  int   (*bind_click)(void* control, void (*cb)(void* control, void* ctx), void* ctx);
  int   (*destroy)(void* created);                     // 우리가 만든 것만, UI_RESOURCE_LIFECYCLE 순서
  // 오버레이
  int  (*overlay_subscribe)(void (*draw)(void* swapchain, void* ctx), void* ctx);
  int  (*wndproc_subscribe)(int (*proc)(void* hwnd, unsigned msg, uintptr_t w, intptr_t l, void* ctx), void* ctx);
  // 호환
  int  (*peer_present)(const wchar_t* module_or_r1_ini);
  int  (*slot_owner)(uint32_t rva, wchar_t* out, int cap);
  void (*log)(const char* owner, const char* event, uint32_t a, uint32_t b);
} San14LayoutApiV1;
```

### 4.6 소유권 이전 목록

| 현재 소유 | 항목 | Layout 이후 | 몸체 위치 |
|---|---|---|---|
| Convenience | Present vt[8]/[22], WndProc | Layout | Layout (안개 Present 체인은 화이트리스트 없음) |
| Convenience | speed_buttons(4칸 + 898957) | Layout 관리자 | Convenience |
| Convenience | lord_change(503C9C, 13E5088) | Layout 관리자 | Convenience |
| Convenience | region_officers 6칸 | Layout 관리자 | **Convenience(C-1)** |
| Convenience/CCZ | ccz_title(4E83F3, vt 13E4E08) | Layout 관리자 + 런타임 UI | CCZ 또는 Convenience |
| Convenience/CCZ | ccz_sprites 13A03D0 | Layout 관리자 | **CCZ/Convenience(C-1)** |
| Convenience | layout_wait(E8 3곳, 86E0C0) | Layout 관찰점(86E320 + E8 3곳). 86E0C0은 없앤다 | Layout |
| Convenience | fast_menu 3칸, stat_exp, dispatch_days, damage_preview(7FA230 → 호출 지점으로 이동) | Layout 관리자 | Convenience |
| Convenience | expert_links(6566E0 → 다른 방식, 6362DB, 5F14DA, vt 8칸) | Layout 관리자 | Convenience |
| Convenience | unified_dialogs, advisor_hud/unified_native, recruit_ui, merit(1413600은 C-1) | Layout 관리자 | Convenience |
| Convenience | traits9 UI | 양보 또는 런타임 라벨(Q4) | Convenience |
| 후보 | advisor_briefing | 입구 JMP 없이 다시 설계(Q5) | Convenience |

### 4.7 기존 ScreenLayout 기반 기능의 처리 (14개 레코드)

| 기능 | 레코드 | Layout 이후 | 상태 |
|---|---|---|---|
| traits9 개성 9칸 | 49F89AB3, 7C471B31, 9EA0EB77, F1D669AF | R1 f09가 있으면 **양보**(C3). 없으면 R1 Traits9_UI처럼 `84B150` 라벨 9개를 런타임에 만든다. 이식 전에는 원작 레이아웃으로 둔다. | 가설(Q4) |
| 진행 속도 4·5번 버튼 | 301526D7, FEA5F28C(207/208/212/213), 40F993CE, 454A1BCC | 아틀라스와 그림 표를 고치지 않는다. 기존 속도 버튼 노드를 `92E430`으로 복제하고 "×4"/"×8" 텍스트를 붙이거나, Present 오버레이로 그린다. R1 진행속도 확장이 있을 때의 양보는 유지한다. S1 해소. | 가설(Q3) |
| 군주 교체 버튼 | D8913AB2 노드 24 | 503C9C 이후 footer 생성 이벤트에서 노드 14를 복제하고 x+29로 옮긴다. 위치를 옮길 수 없으면 하위 레이아웃 pane(x,y 인자)으로 만든다. | 가설 |
| 전문가 인접 프리셋 | DA749F9F 노드 48–50 | 같은 방식으로 복제한다. ModSettings 체인(V7)과 함께 동작하는지 확인이 필요하다. | 가설 |
| 조조전 타이틀 버튼 | E7A28866 노드 37·38 | pane은 이미 런타임으로 만든다. 노드 37 대신 기존 마지막 버튼 노드에 부착하고 x,y로 배치한다. kCount 검사는 39에서 원본 수로 바꾼다. | 가설 |
| CCZ 부대 깃발 좌표 | 3DA1641D, 757F1A41, 5C046EA3 | 좌표만 고친 것이다(CCZ_SPRITES.md). 둘 중 하나로 옮긴다. ① 생성 이벤트에서 해당 노드 컨트롤의 위치를 옮긴다(위치 setter 필요). ② 메모리 장면 좌표를 수정한다(고위험). 조사 전까지 이 3개는 **이행 마지막**에 둔다. | 미검증 |

- 설치기는 `--layout` 단계를 없앤다. 이 규칙 변경은 SESSION_RULES §5 담당(주도 세션)에게 넘긴다.
- 기존 설치본은 해시를 비교해 **우리 값인 레코드만** 원본으로 되돌린다. R1 f31의 레코드는 건드리지 않는다.

---

## 5. 단계별 이행 계획과 게이트

체크 범위는 메모리 규칙을 따른다. 개발 중에는 영향받는 기능의 검사만 하고, 설치나 배포 직전에 dll-split 전체 회귀를 1회 한다. 진단은 한 빌드로 묶는다.

| 단계 | 내용 | 게이트 (정적·모의) | 게이트 (사용자 실게임) |
|---|---|---|---|
| P0 | 이 문서. 사용자 결정 Q1–Q8. MOD_COMPATIBILITY 갱신은 주도 세션 몫 | 결정 기록 | — |
| P1 | Layout 골격: 로더 순서, API v1, 훅 관리자(C-1–C-7), 호환 판정, 로그. 기능 이동 없음 | 모의 이미지 검사: 원본, 외부 모듈, 자기 것, 힙 포인터 슬롯과 E8 지점의 CAS·체인·거부·우회 해제. 입구 금지 목록이 거부되는지. export·import 표, 새 위험 import 없음 | 우리만 설치: 기존 기능 무변화, 로그에 refused/bypassed 없음 |
| P2 | Present/WndProc 이전 | 안개 체인을 흉내 낸 모의 검사(앞·뒤 순서). CAS 복원 | 우리+안개, 설치 순서 둘 다: 오버레이와 안개 표시 |
| P3 | UI 훅을 관리자로 이전. 몸체는 Convenience/CCZ에 둔다. 7FA230·6566E0 입구 패치를 없앤다 | 정적: 경합 칸에 쓰는 주소가 화이트리스트 모듈 안인지(C-1) 기록 검사. 기능별 기존 검사 | 우리+안개, 우리+발탁·육아, 우리+R1: 각 기능 화면. 레퍼런스 로그에 우리 때문에 생긴 SKIPPED/DISABLED가 없는지 |
| P4 | 86E320 관찰점 + 런타임 UI 서비스. **진단 빌드 1회**로 묶어 확인: obj+0x90 KTID 오프셋, x,y·vt+0x78 위치 의미, 같은 노드 복제 뒤 이동, 84B150 라벨 부착, 비동기 생성 경로 | 모의: 이벤트 디스패치·수명 순서 | 대상 화면(속도, 군주, 전문가, 조조전 타이틀)을 열고 닫기. 크래시와 누수(UI_RESOURCE_LIFECYCLE) 확인 |
| P5 | 레코드 14개 제거. lord_change, 전문가, ccz_title, 속도, traits9 순서로 런타임 이식하고 CCZ 깃발은 마지막. 설치기 `--layout` 제거와 복원 | 정적: 설치 후 ScreenLayout.rdb에서 우리 KTID가 원본(또는 R1 값)과 바이트 단위로 같은지. 설치 전 dll-split 전체 회귀 1회 | 각 기능 화면 + R1 f31 그래픽 표시 |
| P6 | 호환 매트릭스 실측 | 로그 수집 도구(읽기 전용) | 조합 4가지(우리+R1, 우리+발탁·육아, 우리+안개, 전부) × 설치 순서 2가지. 선언한 양보 외에 SKIPPED/DISABLED가 없을 것 |

---

## 6. 사용자 결정 (2026-11-05 확정: "권장안대로 진행", 우리 모드를 호환 기준으로 새로 설계)

- **Q1 확정**: UI 칸은 C-2·C-3대로 체인하고, 기능이 겹치면 양보한다. MOD_COMPATIBILITY의 2026-11-04 "체인 없이 거부" 결정을 이 설계 범위(Layout 관리 훅)에서 대체한다.
- **Q2 확정**: 아래 기능 중복 대상은 상대 모듈이 감지되고 켜져 있으면 우리가 양보한다(설치하지 않고 `yielded` 로그).
- **Q3 확정**: 기존 속도 버튼 노드를 `92E430`으로 복제하고 "×4"/"×8" 텍스트를 붙인다. zzmod 전용 그림은 쓰지 않는다. 복제·이동이 진단에서 실패하면 Present 오버레이로 그린다.
- **Q4 확정**: R1 f09가 없을 때 `84B150` 라벨 9개를 런타임에 만든다. R1이 있으면 양보한다.
- **Q5 확정**: advisor_briefing은 입구 패치 없이 다시 설계한다. 다만 아직 후보 기능이라 이번 P1–P4 범위에서는 빼고, 입구 패치 금지 검사만 걸어 둔다.
- **Q6 확정**: 지문 대상 입구(6566E0, 86E0C0, 7FA230)는 P3에서 먼저 바꾼다.
- **Q7 확정**: 경합 칸 포인터는 Convenience/CCZ 안의 몸체 또는 썽크로 둔다(C-1). 안개 작성자에게 요청하지 않는다.
- **Q8 확정**: R1 FPS120과 ×4/×8은 함께 허용한다. R1 진행속도 확장이 켜져 있으면 ×4/×8 버튼은 양보한다. FPS120이 감지되면 프레임당 시작 횟수 상한(현행 최대 2회)을 그대로 두고 로그만 남긴다.

### 원래 질문 (기록)

- **Q1** 정책 변경: "체인 없이 거부"(2026-11-04)에서 "UI 칸은 체인, 기능 중복은 양보"로 바꿀지.
- **Q2** 기능 중복 처리. 대상은 traits9/특성 툴팁(f09/f14), 전사(f25), scenery(SettingsHelpScenic), 지역담당관(f34), 내정 선택(DomesticAI), 포로 귀환(PrisonerReturn), 6D7D90 중재 필터(발탁·육아), 장면 생략(SceneSkip)이다. 권장: 상대가 감지되고 켜져 있으면 우리가 양보한다.
- **Q3** 속도 버튼 ×4/×8 모양: 전용 그림(zzmod)을 포기하고 복제 버튼에 텍스트를 붙일지, Present 오버레이로 그릴지.
- **Q4** traits9 9칸: R1이 없을 때 런타임으로 이식할지, 기능을 뺄지.
- **Q5** advisor_briefing(후보): 입구 패치 12곳을 쓰지 않고 다시 설계하면 범위와 시간이 늘어나는데 받아들일지.
- **Q6** 이미 설치된 지문 대상 입구(6566E0, 86E0C0, 7FA230; 302980은 UI가 아님): P3에서 먼저 바꿀지, 실제 증상이 보일 때까지 둘지.
- **Q7** C-1 대응: Convenience 썽크 호스트로 갈지, 안개 작성자에게 `SAN14_Layout.dll` 화이트리스트 추가를 요청할지.
- **Q8** R1 FPS120과 ×4/×8을 함께 쓸 때 허용하는 동작.

## 7. 미검증·한계

- **실게임 검증 없음.** 시작 순서 경합, 체인 실제 동작, 레퍼런스 로그를 하나도 보지 않았다.
- 레퍼런스가 입구 16바이트를 어디에 쓰는지 확인하지 못했다(서명 해석인지 확인용인지, 와일드카드를 허용하는지). 그래서 C5–C9의 영향은 가설이다.
- 발탁의 체인 의미(bk_hk_chains)와 원본이 아닐 때의 동작, 육아의 속도 칸(v+188) 식별, 안개 u32 후보(C12), R1 Ext 서명형 DLL의 RVA, IC 1.7MB의 전체 패치 목록(.s14sync/.p1fix)은 풀지 못했다.
- 86C570/86E0C0의 x,y 의미, vt+0x78, obj+0x90 KTID 위치, `92B0A0`(노드→컨트롤 생성), 비동기 생성 경로를 확인하지 못했다. `92B0A0`은 영구 DB를 Codex 작업이 잠그고 있어 내보내지 않았다.
- 메모리 장면 좌표 수정과 새 노드 삽입은 근거가 부족하다. 설계에서는 마지막 수단으로 둔다.
- 도구 사용 기록:
  - 게임 EXE에 대한 REA는 조정자의 비용 공지 전에 batch_decompile 1회(86E320, 84B150, 85C9E0, 89AF80, 86E7E0)만 했고, 세션은 close_binary로 닫았다. 그 결과는 harvest하지 않았다(프로젝트 파일 생성 금지).
  - 영구 DB Export는 1회(86E140) 했다. 이 도구가 `research/decomp_cache`에 캐시 파일 2개를 자동으로 추가했다(last_cache_added: 2).

## 부록: 근거 스크립트·출력 (scratchpad, 읽기 전용 분석)

`C:\Users\<user>\AppData\Local\Temp\claude\3ae8b4f9f7459f37\771904b7-eace-46b2-aa72-4af4fd3420ec\scratchpad\` 아래에 있다.

- 스크립트: inv.py, sites.py, cross.py, dis.py, xref.py, rawref.py, constref.py, rdbdiff.py, addimm.py, fpscan.py.
- 출력: inv_small.json, inv_big.json, inv_ours.json, sites_small.txt, sites_big.txt, sites_ours.txt, cross.txt, r1_dinput8.asm, modcore.asm, our_rdata_consts.txt, big_strings.txt.

---

## 8. P3 훅별 판정표 (2026-11-05)

P3 구현자는 이 절을 보고 바로 코딩한다. 판단 근거는 §6의 사용자 결정(Q1, Q2, Q6, Q7, Q8)과 이번 1차 증거다. 이번 조사에서 코드는 고치지 않았다.

근거는 네 가지로 표기한다.
- **[바이트]**: 원작 언팩본 `분석용/game.exe.unpacked.exe`의 원본 바이트, 또는 레퍼런스 DLL을 capstone으로 역어셈블한 결과.
- **[문자열]**: 레퍼런스 DLL·ini·설치기 소스 안의 문자열.
- **[Ghidra]**: `ghidra_full.py`의 callers/xref 결과, 또는 영구 DB Export.
- **[소스]**: 우리 `src/*.inl`.

스캔 표기에서 `iN`은 즉값·u32로 N번 참조했다는 뜻이고, `oN`은 원본 바이트 창을 N개 보관했다는 뜻이다. "참조 없음"은 레퍼런스 DLL 14개를 스캔했을 때 즉값·u32·원본 바이트 창이 모두 0건이라는 뜻이다.

### 8.0 공통 규칙 (모든 행에 적용)

- **G1 next 호출.** 모든 대체 함수는 관리자가 넘겨준 `next`(h.next_out)를 부른다. 원본 주소를 직접 부르면 체인이 끊긴다. 지금 원본을 직접 부르는 곳은 다음과 같고, P3에서 모두 `next`로 바꾼다.
  - `merit_person_read`: 2390B0 직접 호출.
  - `stat_exp::text`: 930D20 직접 호출.
  - `damage_preview` refresh: 원본 7FA230을 먼저 부르는 부분.
  - `expert_populate`: 원본 6566E0을 부르는 부분.
  - 그 밖에 `fn<...>(k원본)` 꼴로 원본을 부르는 대체 함수 전부.
- **G2 vt 교체값의 위치.** vt 교체값(`replacement`)은 기능 DLL(SAN14_Convenience.dll 또는 SAN14_CCZ.dll) 안의 함수로 한다. SAN14_Layout.dll 안의 썽크는 쓰지 않는다(C-1, Q7). 표의 C-1 열은 다음 기준으로 적었다.
  - "필수": 안개가 그 칸을 109칸 표에 두고 화이트리스트로 검사하거나, 육아가 VirtualQuery·Convenience로 검사하는 칸.
  - "권장": 안개가 u32만 보관하고 용도는 확인하지 못한 칸, 또는 경합이 없는 칸.
  - E8 훅은 core의 pool relay(이미지 ±2GB, 힙)를 쓴다. E8 대상이 이미지 안에 있는지 검사하는 레퍼런스는 찾지 못했다.
- **G3 블록을 가로지르는 E8.** 블록 오프셋이 11보다 크면 rel32가 16바이트 블록을 넘는다. 이런 지점은 core가 설치할 때 4바이트를 보호 쓰기 한다(`block_rel_write`). 해당 경로가 돌기 전, 즉 시작 시점에 설치해야 한다. 대상은 503C9C(오프셋 C), 70EADC(C), 7FA1FD(D), 65EAAD(D), 2E111D(D)이다 [바이트].
- **G4 기능 묶음.** owner 하나에 속한 훅 가운데 하나라도 refused나 yielded가 되면 그 owner의 훅을 전부 되돌린다. 지금의 `guards_match` 의미를 유지하는 것이다. 예외는 **merit_prisoner**(13A6BB0 한 칸)다. 이 칸은 merit 묶음에서 분리해서, 양보해도 merit의 나머지 칸은 설치한다.
- **G5 core 확장 4건** (P3 전에 Layout core에 추가).
  - (a) **E9 rel32 재지정 종류.** 898957은 원본이 `E9 C4 8D FF FF`(→891720)인 꼬리 점프다 [바이트]. `hook_block`은 E9로 시작하는 바이트를 거부하고(C-4), `hook_call`은 E8만 받는다. 따라서 `install_call`과 같은 분류·체인 논리로 opcode E9를 받는 종류가 필요하다.
  - (b) **`hook_block` relay.** 지금 `hook_block`은 `replacement`로 곧바로 E9를 쓰므로, `replacement`가 ±2GB 밖이면 RANGE로 거부된다. 8.4의 70E9C1처럼 pool relay를 거치게 한다.
  - (c) **ini 키 조건이 붙은 peer 규칙.** 파일, 섹션, 키, 기본값을 지정하고 여러 조건을 AND로 묶을 수 있어야 한다. 8.2의 조건을 표현하는 데 필요하다.
  - (d) **파일 존재 규칙.** 1단계의 `GetModuleHandleW`는 로드 시점과 경합하므로, 발탁·육아는 게임 폴더의 파일로 감지한다.

### 8.1 훅별 판정

플래그 약어: **C** = CHAIN_OK, **Y** = YIELD_IF_PEER, **L** = LATE_INSTALL, **R** = REFUSE_IF_FOREIGN. 종류: vt = vtable 칸, E8 = 호출 rel32, E9 = 점프 rel32, blk = `hook_block`, imp = 가져온 코드가 직접 씀. 블록 오프셋은 E8·E9의 시작 위치다.

| # | owner (peer 키) | 지점 RVA | 종류 | 원래 대상 | 플래그 | 경합 상대와 근거 | C-1 |
|---|---|---|---|---|---|---|---|
| 1 | speed_buttons | 1473F50 | vt | 882540 | Y C | 육아가 [671AE8]+0x180 객체의 vtable+0x188을 VirtualQuery(MEM_IMAGE, 실행 가능, 게임 밖)와 `GetModuleHandleW("SAN14_Convenience.dll")`로 검사하고 "지도 속도 칸 준비(v+188)를 다른 모드가 바꿈"을 남긴다 [바이트][문자열]. 1473DC8+0x188=1473F50이지만 객체가 같은지는 가설이다. IC는 같은 vtable의 다른 칸(1473DD8, 1473DF0)만 참조한다. | **필수**(육아) |
| 2 | speed_buttons | 13D6698 | vt | 44BCA0 | Y C | R1 FPS120: 44BCA0 몸체 안의 44BD00 호출을 고친다. 공존 가능(V11). 안개 u32 i3, 용도 미검증. | 권장 |
| 3 | speed_buttons | 898957 (블록 오프셋 7) | **E9** (G5-a) | 891720 | Y C | **IC 진행 배속(ProgressSpeedModule)이 같은 5바이트를 고친다** [바이트]. IC는 54165에서 `lea rax,[rdx+0x898957]`로 길이 5 패치를 걸고, 6ED7D에서 5바이트를 비교하고, 464EF에서 `add rax,0x891720; jmp rax`로 돌아간다. 감지해서 양보한다(Q8). | 해당 없음 |
| 4 | speed_buttons | 1474100 | vt | 88C2E0 | Y C | 참조 없음 | 권장 |
| – | speed_buttons | 1476C90 | 훅 아님 | – | – | 우리 functor 객체의 vtable로만 쓴다. | – |
| 5 | lord_change | 503C9C (C) | E8 | 85FEF0 | C | 참조 없음 | – |
| 6 | lord_change | 13E5088 (=13E5018+0x70) | vt | 533570 | C | 참조 없음 | 권장 |
| – | lord_change | 146AFE8, 146D8A0 | 훅 아님 | – | – | 클래스 식별과 functor에만 쓴다. IC·안개·발탁·육아도 146AFE8을 식별용으로만 참조한다. | – |
| 7–12 | region_officers | 139A338→1A7F50, 139A350→1A74D0, 139A328→19B1D0, 13999C8→1A7EA0, 13999E0→1A75A0, 13999B8→19B350 | vt | 표 참조 | C, 8.2 조건부 Y | 안개 109칸 표에 6칸이 모두 있다 [바이트]. R1 RegionOfficer(f34)는 139A328과 13999B8을 i1로 참조하고 인라인 1A7F41/1A7FC5를 고친다. 기능은 "빈 지역 담당관 이름 빨간색" 경고이고, `Face=0`("초상화는 이번 판 없음")이다 [문자열]. 우리 기능(세계지도 초상화와 특성 글)과 다르다. | **필수**(안개) |
| 13 | ccz_title | 4E83F3 (3) | E8 | 866230 | C | 참조 없음 | – |
| 14 | ccz_title | 13E4E10 | vt | 504670 | C | 참조 없음 | 권장 |
| 15 | ccz_title | 13E4E18 | vt | 4F9310 | C | IC i1, 용도 미검증 | 권장 |
| 16 | ccz_sprites | 13A03D0 | vt | 1C6EC0 | C | 안개 109칸 표 [바이트] | **필수**(CCZ 안) |
| – | ccz_sprites | 13A03A8, 1342458, 16B32C0 | 훅 아님 | – | – | 객체 식별에만 쓴다. | – |
| 17–19 | layout_wait | 676767 (7), 86CFA5 (5), 86DEC9 (9) | E8 | 86C570 | C | 세 지점 모두 참조 없음. IC는 86C570 몸체 0x2EA바이트를 통째로 보관한다(D4030/D4320). 호출 지점만 바꾸므로 그 지문은 그대로 맞는다 [바이트]. | – |
| 20 | layout_keep | 86E0C0 입구 | 입구 E9 | – | **제거** | IC가 0x3B바이트를 보관한다(D3FB0/D3FF0). 대체 지점이 없다(8.3). | – |
| 21–23 | fast_menu | 13D63D0, 143C4C8, 1457238 | vt | 56FF60 | C | 세 칸 모두 참조 없음. IC가 56FF60을 즉값 i4로 참조하는데 용도는 미검증이다. R1 SAN14_FastMenu(f30, 서명형, RVA 미검증)가 같은 기능을 한다(8.2 후보). | 권장 |
| 24 | stat_exp | 85C5D7 (7) | E8 | 930D20 | C | 지점은 참조 없음. 다만 **R1 StatusColor가 930D20 내부 훅(930D35/930D40/930E39)에서 복귀 주소가 image+85C5DC인지로 차트 글 호출자를 가려낸다** [바이트]. 우리 relay는 인수를 고친 뒤 `next`로 꼬리 점프해서 [rsp]=85C5DC를 유지해야 한다. 지금의 `text()`처럼 스택 버퍼를 넘기고 일반 CALL을 하면 StatusColor의 차트 분기가 빠진다(효과 미검증). 버퍼는 UI 스레드 정적 버퍼로 바꾼다. | – |
| 25 | stat_exp | 9073F6 907406 907421 907434 90744F 907462 90747D 907490 9074AB 9074BE 907F79 907F89 907FA5 907FB8 907FD4 907FE7 908003 908016 908032 908045 7F5A8C 7F5A9C 7F5AB5 7F5AC6 7F5ADF 7F5AF0 7F5B09 7F5B1A 7F5B33 7F5B44 | E8 | 22FCE0 / 85C420 | C | 30곳 모두 참조 없음. StatusColor는 22FCE0과 85C420의 입구 바이트를 보관하지만 우리는 입구를 건드리지 않는다. | – |
| 26 | dispatch_days | 898083 (3) | E8 | 930D20 | C | 참조 없음. 복귀 주소 898088은 StatusColor의 식별 대상이 아니다. | – |
| 27 | damage_preview | 7FA230 입구 E9 → **제거**. 대체: 7F5781 (1), 7FA1FD (D) | E8 | 7FA230 | C | 두 지점 모두 참조 없음. 안개는 7FA230 입구 창만 보관한다(8.3). | – |
| 28 | expert_links | 6566E0 입구 E9 → **제거**. 대체: 5F10F7 (7), 5F5B27 (7), 65EAAD (D) | E8 | 6566E0 | C | 세 지점 모두 참조 없음(8.3). | – |
| 29 | expert_links | 6362DB (B) | E8 | 5BEC40 | C | 참조 없음 | – |
| 30 | expert_links | 5F14DA (A) | E8 | 92E430 | C | 참조 없음 | – |
| 31 | expert_links | 141BCC8 | vt | 5B9110 | C | R1 ModSettings가 이 칸을 체인한다(6A4D 비교) [바이트][문자열]. | 권장 |
| 32 | expert_links | 1429728, 14296B8, 14296F0, 1429798, 141BD20, 141BCF8, 141BE88 | vt | §2.3 | C | 참조 없음 | 권장 |
| 33 | unified_dialogs | 13AC588 | vt | 2FB440 | C | 참조 없음 | 권장 |
| 34 | unified_dialogs | 13AC5B0 | vt | 258170 | C | 발탁 표(D3C388). 발탁이 나중에 설치되면 거부할 수 있다(V5, 미검증). | 권장 |
| 35 | unified_dialogs | 13AC578 | vt | 256970 | C | 발탁 표 + 안개 u32(용도 미검증) | 권장 |
| 36 | advisor_hud | 14751D8, 14751F0 | vt | 894420, 8782E0 | C | 참조 없음 | 권장 |
| 37 | recruit_ui | 13AC708 | vt | 6BE8E0 | C | 발탁 표 | 권장 |
| 38 | recruit_ui | 13AC770 | vt | 2C0820 | C | 안개 u32 i5(용도 미검증) | 권장 |
| 39 | recruit_ui | 1440FE8 | vt | 6F1D10 | C | 발탁 표, 육아 i1 | 권장 |
| 40 | recruit_ui | 1440DF0 | vt | 733BD0 | C | 참조 없음 | 권장 |
| 41 | merit | 13E69A0, 13E60F8 | vt | 50B670, 509580 | C | 안개 u32 i3(용도 미검증). 창고 sidecar도 이 칸을 쓰므로 merit가 꺼져 있어도 설치한다(현행 유지). | 권장 |
| 42 | **merit_prisoner** | 13A6BB0 | vt | 2390B0 | Y C | R1 PrisonerReturn(f21)의 훅은 image+2390B0을 직접 부르고 이전 값을 체인하지 않는다. ModCore 트랜잭션이라 우리가 먼저 설치하면 저쪽이 SKIPPED된다 [바이트]. 기능은 서로 다르다. 우리 칸은 아카이브를 읽은 뒤 merit_bonds를 지우는 일만 한다 [소스]. G4 예외로 묶음에서 분리한다. | 권장 |
| 43 | merit | 1413600 | vt | 908820 | C | 육아가 우리 설치를 기다렸다가 뒤에 체인한다(V6) [문자열]. | **필수**(Convenience) |
| 44 | merit | 1413488 | vt | 878810 | C | 참조 없음 | 권장 |
| – | merit_scroll | WH_GETMESSAGE | 훅 아님 | – | – | Layout 관리 대상이 아니다(P2 공존). | – |
| 45 | traits9 | 8E1628, 8E1685, 909734, 8D0320, 8E25D8, 8E2660, 90A23C, 2C13B3 (1a4a447d; 옛 8DF198 …) | blk(인라인 JMP) | – | Y | 참조 없음. 길이와 기대 바이트는 `src/traits9_sites.h` 값을 쓴다(local235: 옛 고정 생성물 `traits9_payloads.h` 대체, 중계 코드는 `native_traits9.inl`이 실행 때 만든다). | – |
| 46 | traits9 | 4A6CF3, 4A7313, 22558F, 23064A, 232D97 (5→9) | 바이트 패치 | – | Y | R1 SAN14_Traits9.dll이 4A6CF3과 22558F를 보관하고(o2/o1, i2) "ALREADY_APPLIED"를 남긴다 [바이트][문자열]. 같은 패치다. | – |
| 47 | traits9 | 2E111D (D) | E8 | 2337B0 | Y C | IC i1 o1, 용도 미검증(8.5 N3). **행은 2026-11-06 local10에서 제거됐고, W8(2026-11-08)에서 주소표 id도 삭제했다.** | – |
| 48 | **trait_hover** | E8 806D51, 8070B1, 807D92, 80807C + `hover_slots[]` 약 150칸(13ACF58, 13EFD48, 1460000, 1460018, 14604E0, 14604F8 등) | imp | – | Y (가져온 apply 전체를 실행하지 않음) | R1 SAN14_TraitHover.dll과 v2가 E8 4곳을 i10으로 참조한다. 13ACF58은 R1 SAN14_Traits9_UI도 참조한다. 우리 가져온 코드는 R1 f14 비-v2(sha 8cfad1a6…)와 바이트가 같다 [바이트]. **§2.3의 "traits9 (modpack)" 행은 실제로 f14 hover다.** | – |
| 49 | **collab** | 13A7220, 13A6DE8 | vt | – | Y | R1 SAN14Mods\ModPack\SAN14_Collab.dll(f09). ModPackBridge의 `Collab=1`로 켜진다 [문자열]. | – |
| 50 | scenery (§2.4) | 13D2F00, 14471F0 | vt | 421990, 7229C0 | Y | R1 SettingsHelpScenic과 그 v2가 두 칸을 참조한다 [바이트]. | – |
| 51 | battle_deaths (옛 imported_patches, §2.4) | 259682, 27D976, 25974B, 27DA09, 295C5F | imp | – | Y | R1 Patch27이 이 5곳과 295AA0을 쓴다. IC는 295AA0 입구 창을 보관한다(o4). | – |
| 52 | domestic_selection (§2.4) | 1346E70, 134C8F8 | vt | – | Y C | R1 DomesticAI는 "다른 모드가 AI 노드를 감싸면 그 모드가 원본을 부를 때만 규칙이 돈다"고 남긴다 [문자열]. | 권장 |
| 53 | mediation_plus (§2.4) | **6D7D90 → 70E9C1 blk 10바이트로 이전**(8.4). 70EADC (C) →711920, 6BA697/7971B3 →796110, 6BA69F/7971BB →78D920, 73502B →6D1A50, 735035 →6D0550 | blk + E8 | – | C (70E9C1만 R) | 735035는 FogTrial이 i1로 참조하고 용도는 미검증이다. 나머지는 참조 없음. | – |
| 54 | scene_yield (advisor_briefing 후보) | §2.3 advisor 행 | – | – | Y | P1–P4 범위 밖이다(Q5). 감지만 8.2에 둔다. | – |

플래그를 고른 원칙은 다음과 같다.
- 경합 상대가 없거나 체인하는 상대만 있으면 **C**.
- 기능이 겹치는 상대(Q2·Q8)가 있으면 **Y**를 붙인다.
- **R**은 70E9C1에만 쓴다. 원본 10바이트가 아니면 남의 패치이므로 체인하지 않는다(`hook_block`은 원래 바이트가 맞지 않으면 거부한다).
- **L**은 쓰지 않는다. 지금 LATE_INSTALL은 같은 publish 안의 순서만 정하고 ModCore 커밋을 기다리지 않는다. 그래서 ModCore 모듈보다 늦게 설치되는 것을 보장하지 못한다. ModCore 커밋 대기를 구현하면 13A6BB0을 Y 대신 C+L로 바꿀 수 있다.

### 8.2 양보 대상 감지 (Q2·Q8, 실제 파일 확인)

`<game>`은 게임 폴더다. 경로는 R1 설치기 `분석용/refermod_1/support/manifest101.xml`의 target 값, `installer101/Engine.cs`·`Spec.cs`, payload의 ini, 그리고 각 DLL의 문자열로 확인했다. R1 모듈 ini는 모두 `[Mod] Id/File/Enabled=1/CoreABI=65536` 형식이다.

| owner | 상대 | 감지 조건 (모두 AND) | 1단계 peer_rules와 차이 | 근거 |
|---|---|---|---|---|
| traits9 | R1 f09/f13 Traits9 | `<game>\SAN14Mods\modules\Traits9Check.ini` `[Mod] Enabled≠0` | 그대로 | manifest: Traits9.dll, Traits9_UI.dll, Traits9Check.dll/.ini가 `when="9|13"`. 적용 주체는 IC의 `San14ModSettings.ini [Modules] Traits9Module`(Spec: 9 && !12) 또는 MeritTraits(13)이다. |
| **trait_hover** | R1 f14 TraitHover | `<game>\SAN14Mods\modules\ModPackBridge.ini` `[Mod] Enabled≠0` AND `[ModPack] TraitHover≠0` | **새 규칙.** 1단계의 traits9→Traits9Check.ini로는 이 지점을 덮지 못한다. 14번만 설치하면(`14&!9&!13`) Traits9Check.ini가 없다. | Engine.cs가 `[ModPack] Stats200=on17 TraitHover=on14 Collab=on9 BattleDeaths=0 Field3D=on11`을 쓴다. ModPackBridge 문자열은 `SAN14Mods\modules\ModPackBridge.ini`, `TraitHover`, `SAN14Mods\ModPack\SAN14_TraitHover.dll`, export `SAN14_ApplyTraitHover`. manifest의 TraitHover 조건은 비-v2가 `14&!9&!13`, v2가 `14&9|14&13`. |
| **collab** | R1 f09 Collab | 같은 ini, `[Mod] Enabled≠0` AND `[ModPack] Collab≠0` | **새 규칙** | manifest: `SAN14Mods/ModPack/SAN14_Collab.dll` `when="9"`; Engine.cs `Collab=on9` |
| battle_deaths (옛 imported_patches) | R1 f25 Patch27 | `<game>\SAN14Mods\modules\Patch27.ini` `[Mod] Enabled≠0` AND `<game>\SAN14Mods\Patch27\Options.ini` `[Features] Patch27Features≠0` | **조건 추가.** Patch27.ini와 DLL은 `when="1|25"`이라, R1 1번만 설치해도 깔린다. 그때 Patch27은 설정 페이지만 띄우고 게임 훅은 걸지 않는다. 1단계 규칙만으로는 잘못 양보한다. | Engine.cs `Patch27IniRel="SAN14Mods/Patch27/Options.ini"`, `Patch27Features = on[24]`. Patch27 문자열 "features off (item 25 not installed); settings pages only". 키 `BattleDeathNotices`(섹션 미검증)는 조건에서 뺀다. |
| scenery | R1 SettingsHelpScenic | `<game>\SAN14Mods\modules\SettingsHelpScenic.ini` `[Mod] Enabled≠0` | 그대로 | manifest `when="base"`(DLL은 `base&!22`, v2는 `base&22`). R1이 깔려 있으면 늘 양보한다. 저쪽의 생략 ON/OFF는 IC 설정에서 런타임으로 읽는다("integrated settings unavailable"). 키는 미검증이고, 후보는 4번 항목이 쓰는 `[Convenience] ScenicRewardOnly/ScenicNoFade`이다. |
| region_officers | R1 f34 RegionOfficer | `<game>\SAN14Mods\modules\RegionOfficer.ini` `[Mod] Enabled≠0` AND `[RegionOfficer] Face≠0` | **조건 추가(사용자 확인 필요).** Q2를 글자 그대로 따르면 Enabled만 보고 양보한다. 하지만 Face=0(R1 기본값)일 때 저쪽은 초상화를 그리지 않고 빈 담당관 이름만 빨갛게 한다. Enabled만 보면 우리 초상화 기능을 이유 없이 잃는다. 권장은 Face≠0일 때만 양보하고, 그 밖에는 체인하는 것이다(1A7F50을 next로 부르면 저쪽 인라인도 실행된다). | payload ini `[RegionOfficer] Warning=1 Face=0`. 문자열 "2 sites hooked (vacant area officer = red name color…)", "초상화는 이번 판 없음". |
| domestic_selection | R1 f22 DomesticAI | `<game>\SAN14Mods\modules\DomesticAI.ini` `[Mod] Enabled≠0` | 그대로 | manifest `when="22"`. payload에 `[DomesticAI] AiRotate=1`이 있다. AiRotate=0일 때 노드를 건드리지 않는지는 미검증이라 조건에 넣지 않는다. |
| merit_prisoner | R1 f21 PrisonerReturn | `<game>\SAN14Mods\modules\PrisonerReturn.ini` `[Mod] Enabled≠0` | 그대로. 양보 범위는 **13A6BB0 한 칸**이다(G4 예외). | manifest `when="21"`. 양보하면 아카이브를 읽을 때의 merit_bonds 정리만 빠진다. |
| scene_yield | R1 f04 SceneSkip | `<game>\SAN14Mods\modules\SceneSkip.ini` `[Mod] Enabled≠0` OR (현행) `GetModuleHandleW(L"SAN14_SceneSkip.dll")` OR 6C43B0/6C4670 바이트 불일치 | 1단계 ini 규칙 + 현행 OR 유지 | manifest `when="4"`. SceneSkip 문자열 `SAN14Mods\modules\SceneSkip.ini`, `Disaster`(San14ModSettings.ini, 섹션 미검증), "site bytes differ or taken by another mod" |
| mediation_plus | 발탁 1.8 / 육아 1.1 | **70E9C1로 옮기면 감지가 필요 없다(8.4).** 옮기지 않으면 6D7D90만 양보하고 조건은 다음과 같다. (`<game>\SAN14_Batteki.dll` 파일 존재 AND `<game>\San14ModSettings.ini [Batteki] Mediation≠0`, 키가 없으면 1) OR (`<game>\SAN14_Yuga.dll` 파일 존재 AND `[Yuga] Enable≠0` AND `[Yuga] Mediation≠0`, 키가 없으면 1). | **바꿈.** GetModuleHandle을 파일 검사로 바꾼다(xinput1_3 로드 시점과 경합). 발탁의 `Enabled`는 "새 발견" 스위치라 조건이 아니다. | 발탁 사용안내: 게임 폴더 파일 `xinput1_3.dll`·`SAN14_Batteki.dll`; `[Batteki] Enabled=1 ; 0이면 새 발견을 하지 않음`, `Mediation=1 ; 중개 무장 목록에 발탁장수를 넣음`. 육아 사용안내: "[Yuga], 없으면 모두 켬", `Enable 모드 전체`, `Mediation 중개 후보에 아이`. 키 문자열은 두 DLL에서 확인했다. |
| speed_buttons (Q8) | R1 IC 진행 배속 확장 | `<game>\SAN14Mods\modules\IntegratedConvenience_GreenlandCompat.ini` `[Mod] Enabled≠0` AND `<game>\San14ModSettings.ini` `[Convenience] ProgressSpeedModule≠0` | **새 규칙.** 1단계에서 빠졌던 "R1 진행속도 확장"이 이것이다. 키가 없으면 꺼짐으로 본다(설치기는 6번을 고를 때 1을 쓴다). IC의 기본값은 미검증이다. | Spec.cs 6번 → `Convenience\tProgressSpeedModule=1`. IC 모듈 ini는 payload/base(`File=SAN14_IntegratedConvenience.dll`). IC 안의 UTF-16 문자열 "ProgressSpeedModule", "배속 확장", "San14ModSettings.ini", "Convenience". |
| (로그만) speed_buttons | R1 f19 FPS120 | `<game>\SAN14Mods\modules\FPS120.ini` `[Mod] Enabled≠0` | 새 규칙(양보 아님). Q8대로 로그만 남긴다. | manifest `when="19"` |
| traits9 (local234) | 통합패치 R43.42 9번 | `<game>\SAN14Mods\modules\Mod09_Traits9Check.ini` `[Mod] Enabled≠0` | **이름 추가.** R43.42에서 Traits9Check.ini가 이 이름으로 바뀌었다. 옛 이름 줄도 둔다. | R43.42 `installer101/Spec.cs` |
| stats200 (local234) | 통합패치 R43.42 17번 Stats200New | `<game>\SAN14Mods\modules\Stats200New.ini` `[Mod] Enabled≠0` | **새 줄.** R43.42의 ModPackBridge.ini는 늘 `Stats200=0`을 쓴다(Engine.cs:269). 옛 ModPackBridge 줄로는 양보하지 못한다. | R43.42 `src/stats200_new.cpp`, Engine.cs |
| expert_ingame (local234) | 통합패치 1번 ModSettings | `ModSettings.ini` `[Mod] Enabled≠0` AND `<game>\San14ModSettings.ini [Modules] ModSettingsUI≠0`(없으면 1) | **새 규칙.** ModSettings는 141F250/141F270이 원작이어야 한다. 141ED58은 우리가 먼저 걸고 ModSettings가 뒤에 잇는다(그대로 둔다). 양보하면 게임 중 확인 버튼 두 칸만 빠진다. | R43.42 `src/mod_settings.cpp` 584-615 |
| tactic_priority (local234) | 통합패치 16번 SortiePresets | `SortiePresets.ini` `[Mod] Enabled≠0` | **새 규칙.** 출진 화면 슬롯 145B8C0/145B8D8이 같다. install 첫머리에서 양보하므로 화면 훅이 없고, 거기에 붙는 ambush(군사제안수행)도 `ambush_screen_missing`으로 함께 꺼진다. | R43.42 `우리자리.tsv` |
| drag_learn (local234) | 통합패치 30번 DragLearn | `DragLearn.ini` `[Mod] Enabled≠0` | **새 규칙.** 7423B5, 74A574, 74A5E9, 74C5E8, 74C62A가 같다. | R43.42 `우리자리.tsv` |
| scene_yield (local234) | 통합패치 4번 SceneSkip | 위 SceneSkip ini 규칙을 advisor `guards_match`에서도 본다 | **넓힘.** 우리가 SceneSkip DLL보다 먼저 걸리므로 DLL 감지만으로는 늦다. 양보하면 첫머리 두 곳과 함께 재해 push/yield 호출 4곳(29A31B, 29A323, 29A715, 29A71D)도 건너뛴다(짝이라 4곳 모두). | C2 행 |
| (후보, Q2 밖) fast_menu, layout_keep | R1 f30 FastMenu | `<game>\SAN14Mods\Ext\SAN14_FastMenu.ini` `[FastMenu] Enabled≠0` AND `<game>\SAN14Mods\modules\ExtBridge.ini` `[Mod] Enabled≠0` AND `[Ext] FastMenu≠0`. layout_keep에는 `[FastMenu] PinSlowLayouts≠0`도 붙인다. | Q2에 없다. 기능이 겹치므로 **양보할지 사용자가 정한다.** | payload ini `[FastMenu] Enabled=1 SkipOutWait=1 PinSlowLayouts=1 SkipInWait=1`. ExtBridge.ini 주석 "R39 남의 DLL 불러오기 다리 (26·27·28·30 FastMenu)". |

### 8.2.1 통합편의(IC) 확인 뒤에 걸기 (local234, 2026-11-18)

통합패치의 `SAN14_IntegratedConvenience.dll`(1~7번)은 시작할 때 원작 함수 몸체 2,346곳을 원래 모양과 비교하고(최대 약 3분), 하나라도 다르면 1~7번을 모두 끈다. 그 구간 안을 고치는 우리 자리는 IC 확인이 끝난 뒤에 건다(`src/ic_gate.h`).

- 감지는 통합패치 `ic_after_restore.h`와 같다. `<game>\SAN14_IntegratedConvenience.dll`이 있고, `SAN14Mods\modules\IntegratedConvenience_GreenlandCompat.ini`가 있으며 `[Mod] Enabled`(없으면 1)가 0이 아니어야 한다.
- 기다리는 방법도 같다. 모듈이 60초 안에 올라오기를 기다리고, export `SAN14_WaitConvenienceV1`이 0이 아닐 때까지 최대 10분 묻는다. 모듈이 없거나 export가 없거나 시간이 넘으면 그냥 건다(예전 동작).
- 늦게 거는 것은 네 가지다. ① 전투 DLL 공개 전체(출진 호출 184FF9·26AB48·303FC9, route_hook 44E50D). ② convenience 표의 path_fast·path_count·ccz_bands 줄(표 순서대로). ③ 군사 브리핑의 9332B0 글자 연결 한 곳. ④ 로그 `ic_gate_ready(how, ms)`, `ic_gate_row_deferred/late`, `advisor_text_late`, `combat_installed(1,1)`.
- 184FF9는 IC 후크 36과 같은 호출이다. IC 확인 뒤에도 원작 그대로면 건너뛰고(`sortie_call_skip …,4`), IC가 이미 바꿨으면 그 뒤에 잇는다(`sortie_call_chained_late`). 우리가 먼저 걸면 IC가 자기 후크를 거부하기 때문이다.
- 나머지는 평소대로 바로 건다. 전체를 늦추지 않는 이유는 3FA342(논공 저장) 때문이다. 통합패치 MeritTraits는 우리가 먼저일 때만 그 뒤에 잇는다.
- 남은 충돌은 둘이다. DomesticTalent(22번)는 13D9458(군사 브리핑 user frame)이 원작이어야 해서, 우리가 먼저면 팝업을 띄운다. IC 6번이 하루 진행 프레임 슬롯 13D9728을 바꾸면 그 안의 route_hook 44E50D가 우회될 수 있다(진단 로그 `ic_day_frame_slot`, 1 = 원작 그대로).

### 8.3 Q6 입구 패치 대체

| 입구 | 호출자와 포인터 [Ghidra][바이트] | 대체 | 판정 |
|---|---|---|---|
| **7FA230** (damage_preview) | E8 호출자는 7F5781(7F55D0 안, `E8 AA 4A 00 00`, 블록 오프셋 1)과 7FA1FD(7FA170 안, `E8 2E 00 00 00`, 블록 오프셋 D, 블록을 가로지름) 둘뿐이다. 7FA230을 가리키는 포인터나 그 밖의 xref는 없다. 두 지점을 참조하는 레퍼런스도 없다. | E8 7F5781 + 7FA1FD, `hook_call`, owner damage_preview, C | **대체 가능.** 호출이 모두 이 두 곳을 지나므로 입구 패치와 범위가 같다. 원본 7FA230을 고치지 않고 부르므로 RDX null 조기 반환을 보존하던 bridge가 필요 없어진다. 대신 우리 후처리는 RDX가 null인지 직접 검사한다. 안개의 7FA230 입구 지문도 그대로 맞는다. |
| **6566E0** (expert_links) | E8 호출자는 셋이다. 5F10F7(5F0AE0 안, 오프셋 7), 5F5B27(5F5A70 안, 오프셋 7), 65EAAD(65EAA0 안, 오프셋 D, 블록을 가로지름). 65EAA0은 6566E0을 부르고 656D90을 부르는 31바이트 functor이고, vtable 1429750의 칸 2(칸 주소 1429760, 참조 없음)다. 포인터 xref는 없다. 세 지점을 참조하는 레퍼런스도 없다. R1 ModSettings는 4F39에서 `mov eax,0x6566E0; add rax,base; call rax` 뒤 656D90을 불러 **자기 창을 직접** 채우고, 65675D(E9, 길이 0xE)와 627370(길이 9)을 고친다 [바이트]. | E8 5F10F7 + 5F5B27 + 65EAAD, owner expert_links, C. 대체 함수는 `next`를 부른 뒤 `expert_layout_valid`이면 `expert_project_rows`를 한다(현행 순서 그대로). | **대체 가능.** 65EAAD 대신 vt 1429760을 쓰면 656D90 다음에 행을 투영하게 되어 순서가 바뀐다. 그래서 E8을 권장한다. 이점은 셋이다. ModSettings 창(직접 호출)에는 우리 투영이 들어가지 않는다. 65675D 내부 패치가 계속 실행된다. IC(0x22바이트), MeritResolver·발탁·육아의 입구 지문이 그대로 맞는다. |
| **86E0C0** (layout_keep) | 원작 EXE 안에서 86E0C0을 가리키는 vtable 포인터가 642개, E8 호출자가 676개다(86E320도 포인터 642개). IC는 86E0C0 몸체 0x3B바이트(내부 호출 86E0EC 포함)와 86C570 몸체 0x2EA바이트(86C707→BCE4F0, 86C730→B393B0 포함)를 보관한다 [바이트]. 676720, 86CF80, 86DEA0은 호출자가 1–2곳인 개별 로더다. | 없음 | **대체 불가.** vt 642칸이나 E8 676곳으로는 같은 범위를 덮을 수 없다. P3에서 할 일은 셋이다. ① 86E0C0의 E9를 없앤다(blocked entry). ② layout_wait의 E8 3곳(676767, 86CFA5, 86DEC9)은 남긴다. ③ **`layout_keep` 기능 플래그를 기본 끔으로 한다.** P4에서 86E320 관찰점이 생기면 다시 켤지 정한다. 다만 86E320도 포인터가 642개라 관찰 방식은 아직 정하지 않았다. R1 FastMenu의 PinSlowLayouts가 같은 일을 하므로, R1이 있으면 기능이 빠지지 않는다. |

### 8.4 6D7D90 (발탁·육아와 같은 call 바이트): 지점을 옮길 수 있다

증거 [바이트][Ghidra Export 70E9A0, 6D7D90]는 다섯 가지다.
- 6D7D90은 `E8 0B 6C 03 00`(→70E9A0)이고 블록 오프셋은 0이다. 70E9A0의 호출자는 6D7D90 하나이고 포인터는 0개다.
- 발탁의 275DB 부근 코드는 지점이 6D7D90이고, 길이가 5이고, 첫 바이트가 E8일 때 rel32를 풀어 image+6D7D95+rel을 계산한다. 그 값으로 9DB50(소유 모듈 진단)을 부른다. 발탁 문자열 "첫 바이트가 다름(다른 판이거나 다른 모드가 고침)"을 보면, **발탁은 원본이 아니면 거부한다.** 표에는 6D7D90(10EA90)과 70E9A0(10EB00)이 u32로 있다. 육아도 6D7D90 i2, 70E9A0 i1, 6D7D95를 가진다.
- 70E9A0의 몸체는 다음과 같다.

  ```
  70E9B2 TEST RDX,RDX / 70E9B5 JZ 70EA11
  70E9B7 CMP word [RDX+0xBA],0 / 70E9BF JZ 70E9CB
  70E9C1 CMP word [RDX+0xBC],0 / 70E9C9 JNZ 70EA11
  70E9CB ...
  ```

  우리 `filter()`가 다른 것은 이 BA/BC 동시 조건 하나뿐이다. 나머지(충성·군주, 사관 연수, 칸 선택, 70EADC→711920)는 70E9A0과 같거나 이미 70EADC 훅이 덮는다.
- 70E9C0 블록의 원본은 `0A 66 83 BA BC 00 00 00 00 75 46 0F B6 82 E8 00`이다. 70E9C1부터 10바이트 `66 83 BA BC 00 00 00 00 75 46`을 보관하는 레퍼런스는 없다. 70E9C1, 70E9C0, 70E9C9, 70E9CB, 70EA11을 즉값으로 가진 레퍼런스도 없다. 70E9C2–70E9CA 안으로 들어오는 분기도 없다.

**판정: 옮길 수 있다.** 6D7D90 E8 훅을 없애고 다음을 건다.
- `hook_block(rva=0x70E9C1, len=10, expect=66 83 BA BC 00 00 00 00 75 46)`, owner mediation_plus, flags R.
  - 블록 70E9C0의 오프셋 1이고, 1+10 ≤ 16이라 블록 하나 안에 들어간다.
  - core가 `E9 rel32` 뒤에 NOP 5개를 쓴다. 70E9C0 바이트는 바뀌지 않는다.
- relay는 이미지 ±2GB 안의 pool에 둔다(G5-b). RDX는 70E9B2부터 그대로 사람 객체다. 70E9CB의 첫 명령 `movzx eax, byte [rdx+0xE8]`은 플래그를 읽지 않는다.

  ```
  cmp  byte [rip+mediation_on],0   ; relay 페이지 안의 플래그, Convenience가 설정
  jne  pass
  cmp  word [rdx+0BCh],0           ; 원래 명령 재현
  jne  reject
  pass:   jmp qword [rip+p_70E9CB]
  reject: jmp qword [rip+p_70EA11]
  ```

- 남는 6D7D90 이외 E8 7곳은 사이트마다 C로 둔다. 묶음 롤백(G4)은 지금 `guards_match` 의미대로 유지한다.

이렇게 하면 6D7D90은 발탁·육아가 가져간다. 순서와 상관없이 둘 다 설치된다.
- 발탁·육아가 자기 6D7D90 훅에서 원본 70E9A0을 부르면 우리 70E9C1 패치도 그 안에서 실행된다. 이 경우는 가설이다. 근거는 두 DLL의 표에 70E9A0이 있다는 점뿐이다.
- 부르지 않으면 그 경로에서만 우리 완화가 빠진다. 충돌은 없다.

**대안(옮기지 않을 때)**: 6D7D90 한 곳만 양보한다(8.2 mediation_plus 조건). 그러면 혼인 + 의형제 동시 후보만 빠지고, 나머지 7곳은 설치한다. 이때 G4 예외로 6D7D90을 묶음에서 분리해야 한다.

### 8.5 호환불가 목록 갱신 (§3.1 대체)

1. **S1** ScreenLayout 301526D7 ↔ R1 f31: 변함없음. P5에서 Layout으로 해소한다.
2. **C1** 6D7D90 ↔ 발탁·육아: **8.4를 채택하면 우회가능**(70E9C1)으로 바뀐다. 채택하지 않으면 6D7D90만 양보한다.
3. **C2** 29840B, 298413, 298805, 29880D ↔ SceneSkip: 변함없음. advisor 후보이고 범위 밖이며 scene_yield로 양보한다.
4. **C3를 셋으로 나눈다.**
   - C3a traits9(8DF198 등 + core 바이트) ↔ R1 Traits9(f09/f13): 양보.
   - C3b trait_hover(806D51 계열 + hover_slots) ↔ R1 f14 TraitHover: 양보. 감지 규칙은 새로 정했다.
   - C3c collab(13A7220, 13A6DE8) ↔ R1 Collab(f09): 양보.
5. **C4** battle_deaths(옛 imported_patches) ↔ Patch27(f25): 양보. Patch27Features 조건을 추가했다.
6. **V8** scenery ↔ SettingsHelpScenic: 양보.
7. **신규 N1**: 898957 E9 ↔ IC ProgressSpeedModule. 같은 5바이트다. speed_buttons 묶음 전체가 양보한다(Q8).
8. **신규 N2**: stats200(바이트 패치, UI 아님) ↔ ModPackBridge `Stats200=on17`. 같은 기능이고 바이트가 같은지는 미검증이다. Q2에 없으므로 양보할지 사용자가 정한다.
9. **(2026-11-06 captive_dialog 삭제로 해소됨, 행 98→97)** **신규 N3**: traits9 captive 2E111D ↔ IC(i1 o1). IC가 이 지점을 어디에 쓰는지 미검증이다. traits9가 양보하지 않는 구성(R1 9·13번 없음, IC 있음)에서만 생긴다.

호환불가에서 빠지거나 분류가 바뀐 항목은 다음과 같다.
- **V4** region_officers ↔ RegionOfficer: 기능이 다르다(Face=0) → **공존가능**. Face≠0일 때만 양보한다. 사용자 확인이 필요하다.
- **V10** 13A6BB0 ↔ PrisonerReturn: 순서에 따라 갈린다 → P3는 이 칸만 양보하고, ModCore 커밋 대기가 생기면 체인으로 바꾼다.
- **C5** 6566E0, **C8** 7FA230: 8.3 대체로 해소한다.
- **C7** 86E0C0: E9를 없애고 layout_keep을 끄는 것으로 해소한다(기능 축소).
- 주의할 공존 항목은 둘이다.
  - stat_exp ↔ StatusColor: 복귀 주소 문제가 있어 꼬리 점프로 해소한다. 효과는 미검증이다.
  - fast_menu/layout_keep ↔ R1 FastMenu: 기능이 겹치고 Q2에 없다.

### 8.6 미검증

- **실게임 검증 0건.** 체인 동작, 양보 로그, 레퍼런스 로그를 하나도 보지 않았다.
- 시작 순서: ModCore(DXGI), 발탁·육아(xinput1_3), 안개(dinput8), 우리 로더 가운데 누가 먼저 설치하는지 모른다. LATE_INSTALL은 ModCore 커밋을 기다리지 않는다.
- 발탁·육아가 6D7D90 훅에서 원본 70E9A0을 부르는지 모른다(8.4 효과). 발탁에서 Mediation=0일 때 6D7D90을 건드리지 않는지도 모른다.
- SettingsHelpScenic이 읽는 생략 키. 후보는 `[Convenience] ScenicRewardOnly/ScenicNoFade`.
- SceneSkip `Disaster` 키의 섹션. Patch27 `BattleDeathNotices` 키의 섹션과 효과.
- IC ProgressSpeedModule의 기본값(키가 없을 때)과 런타임에 토글되는지.
- 육아 v+188 객체가 CChangeSpeedLayout(1473DC8)인지.
- 안개 u32의 용도: 13D6698, 13AC578, 13AC770, 13E69A0, 13E60F8.
- IC가 13E4E18(i1), 56FF60(i4), 2E111D(i1 o1)를 어디에 쓰는지.
- StatusColor 차트 분기가 빠졌을 때 실제로 생기는 증상.
- R1 FastMenu(f30)의 실제 RVA. 서명형이라 풀지 못했다.
- DomesticAI `AiRotate=0`일 때의 동작. Stats200이 우리 stats200과 바이트가 같은지.
- traits9 인라인 JMP 8곳이 16바이트 블록 안에 들어가는지(`hook_block` 조건).
- 735035의 FogTrial i1 용도.
- 86E320 관찰 방식(P4).
- 도구 사용 기록.
  - 원작 EXE에는 REA를 쓰지 않았다.
  - 영구 DB Export를 1회(70E9A0, 6D7D90) 했고, 이 도구가 `research/decomp_cache`에 캐시 항목 3개를 자동으로 추가했다(last_cache_added: 3).
  - 레퍼런스 DLL은 실행하지 않고 capstone과 pefile로 정적으로만 읽었다.

근거 스크립트와 출력은 `C:\Users\<user>\AppData\Local\Temp\claude\3ae8b4f9f7459f37\771904b7-eace-46b2-aa72-4af4fd3420ec\scratchpad\p3\`에 있다.
- 스크립트: scan.py, immref.py, strref.py, around.py, iccopy.py, sitebytes.py.
- 입력과 출력: names.txt, names2.txt, scan_out.txt, ic_speed.txt.

### 8.7 판정표 후속 결정 (2026-11-05, 조정자. 사용자 "권장안대로 진행" 범위 안)
- 지역담당관: R1 RegionOfficer가 켜져 있고 `Face≠0`일 때만 양보한다. `Face=0`(기본)이면 공존(체인)한다.
- R1 FastMenu, Stats200(ModPackBridge): Q2 원칙대로 상대가 켜져 있으면 우리가 양보한다.
- 6D7D90: 8.4안을 채택한다. 6D7D90은 발탁·육아에 넘기고 우리 필터는 70E9C1 `hook_block`(REFUSE_IF_FOREIGN)으로 옮긴다. 70E9C1 바이트가 다르면 해당 기능만 끈다.
- 86E0C0: 입구 E9를 없앤다. E8 3곳은 유지하고, layout_keep 중 86E0C0 입구가 필요한 부분은 동작하지 않게 한다(설정 키 기본값은 이번에 바꾸지 않는다).

## 9. P4 결과 반영 (2026-11-05, 조정자 승인)
- 생성 관찰은 86E320 입구 대신 그 안의 `86E338` E8(→92B0A0, 호출자 86E320 하나)로 한다. 소멸 관찰은 `865560`(~CUILayout) 입구 6바이트 hook_block. 근거·로그는 `docs/LAYOUT_P4_DIAG.md`.
- **복제 전제 수정**: 원작 92E430은 아직 컨트롤이 없는 노드만 만들고 그 칸을 비운다. 이미 버튼이 된 노드는 복제할 수 없다. P5의 군주 교체·전문가·속도 버튼 이식은 하위 pane(`load_sublayout`+`attach`)이나 컨트롤 생성자 직접 호출로 다시 설계한다(§4.7의 "노드 복제" 문구 대체).
- destroy는 테이블 등록 버튼·라벨을 개별 해제하지 못한다. 숨김 후 소유 레이아웃 소멸 때 정리(DEFERRED). pane만 즉시 파괴.
- **local3 (2026-11-05)**: 살아 있는 레이아웃 추적표 `kLiveMax`는 512칸 선형 배열에서 8192칸 해시 표(주소 키, 선형 탐사, 뒤로 당기기 삭제)로 바뀌었다. 진단 빌드는 추적 안 된 소멸을 `layout_p4_destroy_unmatched`, `load_sublayout` 실패 상세를 `layout_ui_load_detail`, 화면 노드의 컨트롤 생성 시점(미연결 노드 수의 변화)을 `layout_p4_node_timing`으로 남긴다(그리기 스레드에서 읽기만). 생성 이벤트 시점(86E338)에는 컨트롤이 없고(`92CEE0`가 전부 null), 생성 스레드가 여러 개라는 관찰도 12절에 기록했다.
- **주의**: 2026-11-05 15:21 Steam이 게임을 갱신했다(buildid 25953386, SAN14CE*.exe·0000/0001/0004/0010 변경). 이 설계·추출본·모든 RVA는 기준판 `1fbe6be7…`(이전 빌드) 기준이다. 새 빌드에서 RVA가 유지되는지 확인 전에는 P4 진단 빌드를 포함해 설치하지 않는다.

## 10. trait_hover 등록 (2026-11-06, local12)
- owner `trait_hover`는 ModPack v1.6 TraitHover 블롭 대신 우리 소스가 등록한다(`src/native_trait_hover.inl`, 표 `src/layout_registry.h`). 8절 표의 48번 행에서 말한 "가져온 코드(imp)"는 더 이상 쓰지 않는다. E8 806D51, 8070B1, 807D92, 80807C는 일기토 라벨 CALL 행이 됐고, `hover_slots[]` 약 150칸은 VT 행 152개가 됐다.
- 등록 행: VT 152 + CALL 4 = 156행. 전체 등록 행은 97행에서 253행이 됐다.
- 용량: `kMaxHooks`(`src/native_layout_core.inl`)와 `kMaxHandles`(`src/native_layout_client.inl`)를 384로 올렸다.
- 플래그는 `LF_CY`이고 한 트랜잭션으로 묶는다. 한 행이라도 거절·양보되면 owner 전체를 되돌린다(G4).
- 양보 규칙(R1 ModPackBridge `TraitHover`)은 8절과 같다.
- 모의 검사와 파일 검사까지만 했다. 실게임 미확인.

## 11. battle_deaths 등록 (2026-11-06, local13)
- owner `imported_patches`는 `battle_deaths`로 바뀌었다. ModPack v1.6 BattleDeaths 블롭 대신 우리 소스가 등록한다(`src/native_battle_deaths.inl`, 표 `src/layout_registry.h`). 위 표와 충돌표의 `imported_patches` 표기는 모두 이 owner를 뜻한다.
- 등록 행: CALL 4 + span 1(295C5F). 전체 등록 행은 253행에서 **257행**이 됐다.
- 양보 규칙(R1 Patch27)은 8절과 같다. 모의 검사와 파일 검사까지만 했다. 실게임 미확인.


## 12. tactic_priority 등록 (2026-11-06)
- owner `tactic_priority`(`src/native_tactic_priority.inl`, 표 `src/layout_registry.h`). 등록 행 4개로 전체는 257행에서 **261행**이 됐다. `docs/TACTIC_PRIORITY.md` 4장. SAN14_Layout.dll 재빌드 불필요(`kMaxHooks`/`kMaxHandles` 384 안).

| 행 | owner | 지점 | 종류 | 원래 대상 | 플래그 | 비고 |
|---|---|---|---|---|---|---|
| ~~258~~ | tactic_priority | 7DC20D (`CStrategyUnitMakeState` enter 안, 하단 바 텍스트 버튼 CALL) | E8 | 85FEF0 | C | 2026-11-22 삭제: 버튼을 우측 하단 자체 창(「군사제안수행」 토글 아래)으로 옮김(다른 모드 하단 버튼과 겹침). 1a4a447d는 7DE5BD |
| 259 | tactic_priority | 1458848 (update, 칸5) | vt | 7DF540 | C | 1a4a447d는 145B8D8 → 7E18E0 |
| 260 | tactic_priority | 1458830 (leave, 칸2) | vt | 7D9E50 | C | 1a4a447d는 145B8C0 → 7DC200 |
| 261 | tactic_priority | 7EBBC4 (출발 7EBB30 안) | E8 | 1EC890 | C | 출발 기록. 1a4a447d는 7EE1C4 |

- 모의 검사와 파일 검사까지만 했다. 실게임 미확인.


## 13. 기능 UI 소유 서비스 (2026-11-07, tactic_priority local16)
- 사용자 규칙: UI(진입점)의 생성·위치·보임·소멸은 Layout DLL, 기능 모듈은 동작만. 연결은 기능 스위치(`docs/UI_RESOURCE_LIFECYCLE.md` 10항).
- `layout_api.h` 끝에 덧붙인 항목(크기 검사): `ui_begin(owner)/ui_end()`, `ui_create_window`, `ui_activate`(vt+0x38), `ui_node`, `ui_set_node_text`, `ui_set_node_visible`, `ui_select`, `ui_sync_visibility`, `ui_release(owner,next)`. 구현 `src/native_layout_ui.inl`(`svc_ui_*`), 클라이언트 `src/native_layout_client.inl`(`UiScope`, `ui_*`).
- `ui_begin`은 `api_hook_owner_state(owner)`가 INSTALLED/CHAINED일 때만 성공한다(스레드별 scope, 같은 owner 중첩 가능). scope 안에서는 기존 `clone_control/set_text/bind_click/set_visible`이 레이아웃 이벤트 밖에서도 동작하고(`in_event()` = 이벤트 깊이 또는 scope), 만든 객체는 owner 이름으로 `Created.feature`에 기록된다(`kCreatedMax` 256).
- `ui_release`는 원작 소멸 순서(`vt+0x40`, `vt+0x28(obj,next)`, `vt[0](obj,1)`)로 owner의 창을 없앤다. 근거 `docs/TACTIC_PRIORITY_V2_PLAN.md` 12.6.
- 새 훅 행 없음(등록 행 261 그대로). `SAN14_Layout.dll` 재빌드 필요(API 크기가 커져 예전 Layout과 새 Convenience 조합은 UI 서비스를 못 찾아 UI 없이 원작대로 돈다: `UiScope.ok==false`).
- 모의 검사: `tests/layout_p4_fixture.cpp`의 "UI ownership scopes" 구간(게시 안 된 owner 거절, scope 중첩·owner 검사·해제). 창 생성·활성·소멸의 실제 동작은 모의 이미지에 원작 코드가 없어 실게임 확인 대상이다.
