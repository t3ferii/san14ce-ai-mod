# 단일 프로그램 계획: 모듈화 · 검사 슬림화 · 레거시 정리 · 패치 대응

사용자 목표(2026-11-07): "모드가 커질수록 느려지는 검사들. 이런거 전부 모드들 하나 하나를 의존성있는 코드끼리 독립적으로 분리해서 모듈화하고 검사도 자동으로 충돌지점 예측해서 하고 슬림화 시키고 구버전 패치 레거시 코드 문서들 싹 정리하고 새 스팀판에 유동적으로 대응할 수 있게 만들기". xhigh 설계 에이전트 결과 요약. 리뷰 남은 항목 F-03·F-09는 이 안에 흡수. O-01(줌렉)은 이미 해결되어 제외. Fog 화이트리스트는 불변식(I-2)으로만 유지.

## 리팩토링 목표 구조 (2026-11-07): 모드 매니저처럼 작동하는 슬림화
사용자: "우리모드를 개별 모드 1 2 3 4 5 로 보고 우리 설치기는 그걸 조율하는 모드 매니저 같은 기능처럼 작동하는거야. 그러니까 새 모드를 만들어도 새로운 모드의 호환성만 우리 모듈들하고 비교분석하고 금방 쉽게 통폐합 할 수 있는구조". 보충: "또 원칙을 바꾸지말고 슬림화가 마치 그런식으로 작동해야한다는거지 모듈화해서 어떤 손해같은게 생기면 안돼 우리 모드 구조를 그런식으로 리팩토링하겠다는거야". 즉 새 원칙이 아니라 이 계획(모듈화·슬림화)이 도달할 구조의 모습이다. 기존 규칙은 그대로.
- **UI 진입점 = 레이아웃 DLL, 실기능 = 모드 (2026-11-07 사용자):** "UI 같은건 유저가 어떤 기능으로 진입하기위한 진입점이잖아? 그 버튼을 눌렀을때 실행되는 기능이 모드고. 이 UI 진입점은 레이아웃 dll에 정리하고 실 기능은 모드 체크에 있어 설치기에서 설치/해제할때는 이 두개가 같이 움직이는거고". 버튼·팝업·토글의 생성·위치·표시·해제·클릭 전달은 SAN14_Layout.dll이 맡고, 모드는 선언한 콜백 API 뒤의 동작만 가진다. 매니페스트에는 모드마다 entry_points(레이아웃 DLL에 등록되는 UI)와 behavior(모드 코드)를 함께 적고, 설치기 기능 스위치 하나가 둘을 같이 켜고 끈다.
- **손해 없음(무손실) 불변식:** 리팩토링 전후로 게임 동작·기능·설정값·ini 키·세이브 형식·DLL 구성·설치 절차가 같아야 하고, 실행 중 성능 비용(프레임·로딩·메모리)이 늘면 안 된다. 매니페스트·생성 표는 빌드·설치 시점에만 쓰이고 게임 실행 중 추가 처리는 없다. 각 웨이브 병합 조건 = 해당 검사 통과 + 생성물(설치기 카탈로그·features.json·등록 표)이 손으로 쓴 기존 값과 같음(diff 0) + 진단판 perf 기록 비교.
- **모드 = 독립 단위.** 기능 묶음 하나가 모드 하나. 모드마다 매니페스트 `modules/<id>/module.json` 한 장: id, 표시 이름·설명, 소유 파일(owns), 들어갈 DLL(host), 의존(requires), 충돌(conflicts), 차지하는 훅 자리(hook_map site), 제공·사용 API(provides/uses), ini 섹션·키, 설치기 항목, 자체 검사(fixture·route), 출처 크레딧. features.json schema 2는 이 매니페스트들을 모아 만드는 생성물로 바뀐다(손으로 쓰지 않음).
- **설치기 = 모드 매니저.** 매니페스트를 읽어 목록·토글·의존 자동 켜기·충돌 차단·ini 생성·설치 순서를 정한다. 설치기 카탈로그 3종(InstallerGui.cs, FeatureSelect.ps1 2개)은 매니페스트에서 생성(손 편집 금지). 모드가 꺼지면 그 모드의 훅·UI는 원작 상태(UI follows feature).
- **새 모드 추가 = 호환성 비교만.** 새 매니페스트 하나를 넣으면 `tools/mod_compat.py <id>`가 기존 모드 매니페스트들과만 비교: 훅 자리 겹침(VT 칸·16바이트 블록·진입부), 같은 원작 주소 쓰기, API·심볼 충돌, ini 키 중복, 리소스·UI 자리 겹침, 의존 순환. 보고서 = 충돌 지점 목록 + 영향 받는 모드의 검사만 고른 실행 목록. 통과하면 매니페스트의 host만 정해 빌드에 들어감(통폐합 = 빌드 구성 변경, 코드 재배선 아님).
- **모드 경계 규칙.** 모드끼리는 선언한 API(provides/uses)와 core 서비스(GA 주소표, layout 관리자, log_event, 설정 읽기)로만 만난다. 남의 파일 include·남의 전역·남의 mod_features 플래그 직접 읽기 금지(feature_deps D1~D5가 강제). 공유 허브 파일(native_adapter.cpp, native_split_install.inl, layout_registry.h)은 매니페스트에서 생성되는 등록 표로 줄여 간다.
- **물리 DLL 수.** 현재 규칙(DLL 5개+설치기)은 유지하고 모드는 논리 단위로 host DLL에 담는다. host 배치는 매니페스트 한 줄이라 나중에 DLL 구성을 바꿔도 모드 코드는 그대로.
- 웨이브 대응: W0 도구(feature_deps·래칫)는 모드 경계 검사 기반 / W1~W3 파일을 모드별 폴더·매니페스트로 이동 / W4 설치기 카탈로그 생성(D9) / W5 cross-DLL API 선언 / W6 허브 파일 생성화 / `mod_compat.py`는 W4와 함께.

## 0. 수치 목표
| 지표 | 현재 | 목표 |
|---|---|---|
| 기능 1개 수정 후 `-Changed -Base main` 캐시 미스 | 3~4분(split-module 190초) | ≤60초 |
| 같은 경우 캐시 적중 | — | ≤15초(always만) |
| always 4종(manifest, address-static, hook-map, feature-deps) | W0-4 실측 | 합 ≤20초 |
| 설치 전 `-Feature dll-split -Build` | 약 6~6.5분 | ≤4분(병렬) |
| routes est_s 합 | 약 1127초 | ≤600초 |
| split-module | 190초 | ≤60초 |
| 중복 검사 쌍 / 미연결 legacy | 4 / 7+ | 0 / 0 |
| 추적 파일의 1fbe6be7 언급(legacy/research/build 제외) | 98파일 216회 | 0(docs/HISTORY.md 제외) |
| 구 이미지 의존 검사·도구 | 약 40파일(ga_expand import 32) | 0 |
| combat DLL에 컴파일되는 편의 죽은 코드 | 약 21파일(/wd4505로 숨김) | 0, /we4505 강제 |
| 새 패치 대응 | rva_remap→check_addresses→gen_signatures→b_verdicts 수작업 | `tools/patch_next.py` 1회, ≤10분(언팩·ghidra 추출 제외) |
| check_feature.ps1 분기 | 46 | 0(데이터 runner) |

공통 규칙: DLL 5개+설치기만, 새 DLL·EXE·ini 키 없음. log_event만. 게임 EXE에 REA·디스어셈블러 금지(ghidra_full 추출물·읽기 전용 언팩 EXE만). 설치는 install_local.py. 공유 파일(native_adapter.cpp, native_split_install.inl, layout_registry.h, game_addresses_data.inl, gen_signatures.py, features.json, check_feature.ps1, 설치기 카탈로그 3종)은 병렬 편집 금지.

## 1. 의존성 인벤토리
### 1.1 먼저 고칠 버그 (W0-1)
`tools/hookmap_extract.py` `active_lines` 201행: `stack.append((active, c and active, active))` → `(active, c and active, c and active)`. 비활성 그룹 안 중첩 `#if…#endif` 뒤가 활성으로 잘못 판정되어 combat 변형에 native_ai_trace.inl·native_advisor_briefing.inl이 포함됨(R10 오염). 회귀 경우를 verify_hook_map.py에 추가. R8 0 유지.

### 1.2 `tools/feature_deps.py`
입력 src/, build_split.ps1, features.json. 출력 `build/development/feature_deps/edges.{json,tsv,dot}`. 166파일 ≤5초, 내용 해시 캐시.
1) 변형: convenience(native_adapter.cpp+SPLIT_BASE+LAYOUT_ROUTED), combat(+SPLIT_COMBAT), ccz(ccz_module.cpp), layout(layout_main.cpp), 로더. 정의 = parse_flags($taskFlags)+변형 정의, 고친 active_lines.
2) 전처리(오프셋 보존): 주석·문자열 내용·지시문 줄을 같은 길이 공백으로.
3) 소유 맵: owns glob, 없으면 `?`.
4) 정의 수집(DLL 루트별 독립 테이블): namespace 스코프 함수 본문, static, struct/class/enum·열거자, using, 활성 #define. `ns::name` 정규화. 본문 없는 선언은 `decl`(다른 모듈 심볼 forward 선언 = 결합 신호).
5) 참조 해석: 토큰(멤버 접근·키워드 제외), 정규화 이름 그대로, 아니면 namespace 체인→using→전역. 후보 2+는 ambiguous, 간선 안 셈.
6) 간선 = 참조 파일 모듈 → 정의 파일 모듈(심볼·횟수·첫 위치), 자기 간선 제외.
7) 추가 간선: (a) `mod_features.<flag>` → features.json `runtime.flags` 소유 모듈 (b) hook_map site 공유(VT 칸·16바이트 블록·진입부) = 충돌 후보 (c) 다른 모듈 파일 #include.
8) 분류: core·서비스 허용, feature→feature는 depends 필요, cross-DLL은 선언 API만, dead-include(다른 DLL에 컴파일), Tarjan SCC.
9) 오탐 필터: guards/profile/draw/install/state는 정규화·유일 정의일 때만.
10) `development/feature_deps_policy.json` `{edges:[{from,to,symbols,reason,date,until_wave}],counters:{}}`, W0 기준선 래칫(새 간선·카운터 증가 = 실패), `--tighten`.
하위 명령: `scan`, `--check`(always), `explain <module>`, `impact <file> [--hunks]`(select_checks용), `tools`(도달 불가 tools/tests 목록).
규칙: D1 미선언 feature→feature(래칫→W6 끝 0) / D2 cross-DLL 심볼(→W5 0) / D3 dead-include(약 21) / D4 다른 모듈 forward 선언 / D5 남의 mod_features flag 읽기 / D6 core_hooks·Layout 밖 이미지 직접 쓰기(F-03 후 0) / D7 NEED_WHITELIST owner dll ∈ {convenience,ccz,combat} 즉시 실패 / D8 레거시 스캔(1fbe6be7, 구 이미지 경로, ga_expand 구 모드, 없는 docs 참조) / D9 depends ↔ 설치기 부모-자식(경고→W4 실패) / D10 미선언 SCC.

### 1.3 1차 표본 (중심점)
| 중심점 | 결합 | 해소 |
|---|---|---|
| native_split_install.inl | 거의 모든 기능 install/guards/prepare/publish/restore, 수작업 순서, split_feature_guards 교차 끄기(kPointerCczDialog→ccz_sprites+tactic_priority, merit_save guard→merit+awards, domestic tick→domestic+awards+injury+injury_herbal), split_layout_yield, `#include native_modpack.inl` | W4 레지스트리·의존 끄기·서비스 |
| native_adapter.cpp | recruit_ui_links, recruit_ai_links, RecruitAlert, friendship 후보, loyalty_guards_match, personnel 후보, make_domestic_cost_bridge, AI campaign(460–1383), core→ai 28심볼 | W4-6 이동 |
| native_merit_save.inl | merit↔awards/terrain-edit/tactic-priority(merit_sidecar_path), merit_bond.h→friendship_save.h/store.h | W4-5 sidecar 플러그인(F-09) |
| native_unified_overlay.inl | shared-overlay↔ccz-sprites/terrain-edit/terrain-profile, ccz_sprites::on_present·ccz_preview::draw forward 선언 | W4-3 구독 레지스트리 |
| native_unified_dialogs.inl(ccz-sprites 소유) | tactic-priority→ccz-sprites | W4-4 ui-dialogs 서비스 |
그 밖: ccz-sprites→lord-change(callable_vtable), fast-menu→lord-change(map_tick), appear-batch→face-keep, face_timing→face-keep, advisor/path-count/path-fast/dispatch-days→ai(game_day), path-fast→sortie, progress-native→path-count, personnel→sortie, recruitment/injury/awards→personnel, awards/recruitment→ui-common, tactic-priority→field-io-revision(SidecarCache), trait-hover→modpack, traits9→modpack(ModpackBytes, modpack_revert), modpack→dll-split. flag: domestic→awards/awards_ai/injury/injury_herbal, fast_menu→lord_change, merit_save→tactic_priority, trait_hover→tooltip_collab, split_pointer_enabled→ccz_sprites||tactic_priority.
SCC: S1 merit·awards·domestic·terrain-edit·tactic-priority(save host·domestic tick) / S2 personnel↔recruitment(personnel_core 또는 family) / S3 overlay·ccz·terrain(구독) / S4 ai↔expert-combat / S5 sortie↔district-intercept.
DLL 경계: combat이 편의 파일(domestic, population, personnel, recruit_ai, personnel_batch, recruit_ui, injury(+links), ui_common, awards(+save,links), terrain_foam 일부, merit(+scroll,save,links), tactic_memory_save, path_count, progress, expert_settings)을 죽은 static으로 컴파일. convenience는 ai/sortie campaign 전체 컴파일. features.json ai-trace dll이 combat으로 잘못(실제 convenience).
순서 제약: ccz_title>ccz_sprites, trait_hover 마지막, ai_trace·face_timing 끝, path_fast xor path_count. "layout_wait는 ccz_title 뒤"는 낡음(86E0C0 패치 제거).

## 2. 목표 구조
- 모듈 = `<f>.h` + `native_<f>.inl`(+`_save.inl`). `src/core_module.h`:
```cpp
struct ModuleCtx { const unsigned char* image; const ModFeatures* f; };
struct ModuleDesc { const char* name; unsigned char off_id, dll;
  bool (*wanted)(const ModFeatures&); bool (*guards_match)(const unsigned char*);
  bool (*prepare)(ModuleCtx&); bool (*publish)(ModuleCtx&); bool (*install)(ModuleCtx&); void (*uninstall)();
  const unsigned char *needs, *services; };
```
  모듈마다 `tests/<f>_fixture.cpp`(core_fixture_stub.h + 모듈만, 1a4a447d fixture 이미지).
- 레지스트리: features.json schema 3(`runtime {ns,flags,off_id,after,services}`, `sidecar {ext,plugin}`, `status:"service"`). `tools/gen_module_table.py` → `src/module_table_{convenience,combat}.inl`(커밋). 순서 = depends∪after 위상 정렬, 동순위는 현재 split_activate 순서. 순환이면 생성 실패. manifest 경로가 재생성 일치 확인.
- 의존 끄기 폐포: load_mod_features 직후 위상 순서로, 하드 의존이 꺼지면 같이 끔(`reason=dep:<id>`). split_feature_guards 수작업 대체. 서비스는 구독자 있을 때만 설치, 서비스 guard 실패 = 구독자 전부 끔.
- 코어 서비스: log / addr / world(world, table, valid, fn) / core_hooks(patch_pointer, CodeLink, CallLink, replace_code16, allocate_near, absolute_jump) / layout_client / slots(SplitPointer) / map-tick / domestic-tick(GA 295160) / game-day / ui-dialogs / ui-callable / overlay(그리기 순서 상수 보존) / save-host / settings.
- DLL별 포함 목록 `src/generated/modules_<dll>.inl`(features.json dll), combat에 편의 모듈 없음, cross-DLL은 API 구조체만(CczApi abi 2 방식), 마지막에 `/wd4505`→`/we4505`.
- 불변식: I-1 한 훅 한 소유 DLL(hook_map 0 위반, 정책 예외만) / I-2 NEED_WHITELIST 대체 코드는 Convenience·CCZ·CombatAI에만(core 329 거부 + R8 + D7) / I-3 UI는 기능을 따른다(UI_RESOURCE_LIFECYCLE §8) / I-4 기존 sidecar 형식 불변(새 것은 매니페스트 하나) / I-5 설정은 기능설정.ini, 새 키 없음, 카탈로그 3종 동시 변경.

## 3. 검사
- 선택: owns+registry 행 라우팅 → 영향 = depends 역폐포 ∪ feature_deps 심볼 간선 ∪ hook_map site 간선(충돌 예측, `-List`에 `conflict: A shares <site> with B`). hub·서비스는 hunk를 감싸는 정의 단위로 심볼 참조 모듈만, 정의 밖 변경은 그 파일 의존 전체. `hook_map.py --diff <base>`로 바뀐 site만 재평가.
- 실행: routes `parallel_safe`, `-Jobs N`; 46분기 → routes runner/args; route `budget_s`, last_s > 1.5×budget 경고.
- split-module 슬림화: compat 23회를 모듈 경로로(`--ccz`→ccz-sprites, `--merit/--save`→merit·save-host, `--off`→통합). split-module엔 통합 단언(위상 순서, 전체 install/uninstall, 의존 끄기 행렬, SplitPointer 체인, Layout Transaction·rollback)만. verify_callsite_compat(약 2분, 구 이미지)은 R13/R14 대체분 퇴역, 의미 검사만 현재 이미지로.
- 퇴역: 중복 4쌍(verify_layout_registry 1·3↔R13·R5, verify_modpack_static 겹침↔R1·R2, check_observer_hooks c4c↔R6, address-table↔address-static), 미연결 legacy(verify_session_release, verify_hook_scopes, verify_input_loader, verify_input_update, verify_package, 후보 검사), dll-split의 target-retreat 등 흡수분, 바이트 대조만 하는 ga_expand 검증기, `.bak` 파일, 도달 불가 tests.

## 4. 레거시 정리: 1a4a447d 재기준
`tools/addr_promote.py`를 만들어 지금 1a4a447d로 승격하고, 이후 패치마다 재사용(5장 promote).
| 대상 | 현재 | 재기준 후 |
|---|---|---|
| GA id | `GA_<1fbe6be7 RVA>`, 2067개 | `GA_<1a4a447d RVA>`, 같은 새 RVA는 병합, 새 값 0 id는 목록화 후 삭제/결정 |
| guard 리터럴 | 구 바이트 + ga_expb 번역(74384바이트) | 현재 바이트 리터럴, 기준 빌드는 memcmp, 미지 빌드는 와일드카드 |
| GH | 구 FNV + ga_hash_pairs[273] | 현재 FNV |
| ga_builds | [0]=1fbe6be7, [1]=1a4a447d | [0]=1a4a447d만, 구 열·expb·hash_pairs 삭제 |
| 시그니처 | 구 빌드 디코드 기준 | 1a4a447d 이미지·ghidra_full에서 재도출, 현재 이미지 안 유일성 |
| gen_signatures/addrlib | OLD/NEW 쌍 + b_verdicts 수작업 | `--ref <build> [--target <build>]`, 대상 없으면 단일 열, b_verdicts 폐기 |
| fixture 이미지 | legacy/FogMod_Source/.../running_code_text.bin, research/AI_Analysis/native_analysis_only.bin | `tools/make_fixture_image.py` → `build/ui_native_analysis/fixture/1a4a447d_text.bin`(+헤더 json) |
| ga_expand 검증기 32개 | 구 RVA·구 이미지 | v2(이행기) id→1a4a447d 값, v3(승격 후) 거의 항등 |
| 이중 형식 코드 | building_destruction form 1/2, adjacency_floor_mode 0/1, diplomacy_intel 7인자, modpack clamp long form 등 | 구 분기·fixture 경우 삭제 |
| expected_disabled.json, SplitManage | rva_map 쌍 의존, 주석 낡음 | patch_next 보고서가 빌드 게이트 표 생성 |
| 일회성 도구 | rva_remap, rva_rebase, guard_rebase, check_addresses 쌍 비교 | 퇴역, check_addresses 재작성 |
| traits9 생성기 | 매 빌드 reference_mod/Editor_Source 읽기 | 생성물을 `src/traits9_payloads.h`로 고정, modpack_payloads.h 복사 제거 |
| 근거 주석 | "Original (REA, unpacked 1fbe6be7...)" 약 30곳 | "(근거: 1a4a447d 이전 기록, docs/HISTORY.md)" |
등가성 증명: `tests/ga_equiv_dump.cpp`가 모든 GA·mcmp·GH 지점의 (순번, RVA, 기대 바이트, 해시 범위)를 덤프. 승격 전(select_build(1)) = 승격 후(build 0), id 이름 제거 정규형 일치. R13/R14 현재 EXE + GH 이중 출처(현재 EXE 직접 FNV, ga_hash_pairs) 대조.
문서·도구: 현행 규칙·사용법·활성 기능·사용자 문서만 유지, 이력 문서(BUILD_25953386_*, LOCAL10_PORT_PLAN, INSTALL_LOCAL2_TRIAGE, EXPERT_ADJACENCY_UI_ATTEMPTS, GHIDRA_UI_CACHE_TEST, BATTLE_DEATHS_PLAN, TRAIT_HOVER_PLAN, CHECK_REDESIGN_PLAN→CHECKS 병합, archive/*, CODE_REVIEW_* 처리 후)는 `docs/HISTORY.md`(문서|기간|요약|마지막 커밋)로 남기고 삭제. 소스 주석 참조 먼저 수정(D8). MODULES.md는 features.json 생성 표. tools 188개는 `feature_deps.py tools` 도달성으로 퇴역 후보 묶음 확인. features.json에서 release-history·legacy-candidates(friendship Binding을 src/save_binding.h로 추출 후)·target-retreat 삭제. 통합 빌드·후보 빌드는 D-1.

## 5. `tools/patch_next.py`
| 단계 | 내용 | 산출물 |
|---|---|---|
| detect | 설치 EXE 헤더(TimeDateStamp, SizeOfImage, sha256)·appmanifest buildid 읽기 전용 비교 | build/development/patch_next/<sha8>/build.json |
| (사용자) | Steamless 언팩 → build/ui_native_analysis/editor83_b<buildid>/, ghidra_full 추출 | |
| resolve | `gen_signatures --ref <현재> --target <new>` + C++ 런타임 해석기(resolve_probe) 오프라인 대조 | addr_table.json, 불일치 |
| guards | 모든 mcmp·GH를 대상 이미지에서 평가, ga_gate로 기능별 집계 | guards.tsv |
| hookmap | R13 대상 EXE, `--diff ref target` | hookmap_diff.json |
| report | OK / AUTO / ATTENTION(file:line) / BLOCKED, 빌드 게이트 표 | report.md/json |
| emit | data.inl 2열, address-static·모듈 fixture(대상 이미지) | 커밋 후보 |
| promote | 실게임 확인 뒤 addr_promote로 대상 승격 | 커밋 후보 |
리허설: 구 1fbe6be7 EXE를 "다음 빌드"로 넣어 ATTENTION이 b_verdicts와 맞는지, ref=target이면 전부 OK.

## 6. 리뷰 항목
- F-03 Stats200: modpack_bytes 14행(GA_142CBE0 16바이트는 데이터 행), 릴레이 2개(GA_230098 clamp 단축형 E9, GA_2B9F8A growth). 코드 행 → layout_registry.h owner `stats200`(Convenience), `layout_client::bytes(...,LF_Y)`를 Transaction 하나로(ALL_OR_NONE, traits9 방식; 미지 빌드는 와일드카드 통과 후 실제 바이트 사본). 데이터 행은 분리, range_write가 비코드 허용하면 data 행, 아니면 core_hooks `write_data16_cas`. 릴레이 → layout_client::jump/call. modpack_revert → rollback/uninstall. traits9→modpack 결합 소멸, verify_modpack_static 겹침 퇴역. 한계 명시: CAS16 블록 단위 원자성, 안전 근거는 시작 시 단일 UI 스레드 1회 설치. Layout 없으면 꺼지고 기록. fixture: 선행 패치 YIELD, 직전 변경 REFUSED, 실패 후 복원, 2스레드 블록 원자성.
- F-09 save host: `src/save_host.h` `SidecarPlugin{name,ext,version,active,snapshot,encode,decode,reset}`(features.json sidecar로 생성), `save_host::write_atomic`(중복 4곳 삭제), `save_host::path(binding,ext)`. 매니페스트 `<name>.<sha256hex>.sidecars` = magic "S14M" | u16 v1 | archive sha256 | u16 count | count×{ext[12], u16 plugin_version, u8 state WRITTEN/OFF/EMPTY/FAILED, u32 len, sha256} | crc32, 모든 sidecar 뒤 마지막 게시. 로드: 매니페스트 없음 → 기존 동작 / OFF+없음 → absent_off / WRITTEN+없음·불일치 → reset, absent_failed/mismatch / 파일 자체 유효·매니페스트와 다름 → 파일 채택 manifest_stale / FAILED → reset. 원작 .s14는 이미 기록되므로 감지·구분이지 원자성 아님(문서 명시). 이전 해시 sidecar 수 로그만(`sidecar_old_versions`, 삭제 안 함). fixture `tests/save_host_fixture.cpp` 단계별 실패 주입. .tactics는 W4-5 save_host 플러그인.

## 7. 웨이브 (각 단계 커밋, `-Changed` 녹색, 병합 지점마다 주 폴더 `dll-split -Build`)
- W0 기반(직렬): 0-1 active_lines 버그 / 0-2 feature_deps.py + 기준선 정책 + ai-trace dll 수정 + always에 feature-deps / 0-3 D8·tools 도달성 기준선 / 0-4 시간 기준선(CHECKS.md).
- W1 구빌드 제거·재기준: 1-A 검증기 32개 분류 / 1-B fixture 이미지 전환(소스 변경 없음) / 1-C ga_equiv_dump 기준선 / 1-D gen_signatures `--ref/--target`(출력 동일) / 1-E addr_promote codemod(동결 창; tactic 파일은 `src/game_addresses_compat.h`로 격리) / 1-F 이중 형식 삭제 / 1-G 쌍 산출물 의존 제거, PATCH_RESILIENCE 재작성, worktree.ps1, traits9 고정.
- W2 patch_next(병렬, 등록만 직렬): 2-1 detect/resolve/guards/report(ref=target 전부 OK) / 2-2 hookmap diff·emit·promote / 2-3 구 EXE 리허설 / 2-4 구 산출물 처리(D-4).
  - 2026-11-07 W2 완료 기록(상세·리허설 수치는 docs/PATCH_RESILIENCE.md): 2-1 `tools/patch_next.py`(detect/resolve/guards/report, 기준=대상 자체 검사 `patch-next` 경로 24초, id 전부 OK) + `tools/pelib.py`(디코더·PE·이미지 레지스트리) + `gen_signatures.py`/`gen_emit.py` 단일 열 재작성(기준판만으로 다시 만든 표 = 커밋된 표 바이트 동일) / 2-2 `hook_map.py --diff REF TARGET`, 후보 표 emit(열 둘), `promote`(addr_promote plan/apply 래퍼, `--confirmed`) / 2-3 옛 1fbe6be7 해제본으로 리허설(W1-E 수작업 대응과 1,902개 중 1,898개 일치, 나머지 3개 중 2개는 ATTENTION으로 잡힘) / 2-4 옛 산출물 이동 목록은 보고서에 있음(backup/은 추적 안 됨, 주 세션이 병합 뒤 이동). `rva_remap.py` 퇴역(삭제), `addrlib.py`는 소스 상수 스캔 전용. 부수 수정: `addr_promote`가 기록 폴더를 다시 쓰지 않음.
- W3 검사 슬림화(직렬, W2와 병행): 3-1 영향 확장 / 3-2 병렬 runner·예산 / 3-3 분기 데이터화 / 3-4 퇴역 / 3-5 split-module ≤60초.
  - 2026-11-07 W3 완료 기록(측정은 docs/CHECKS.md 끝 표): 3-1 영향 확장(hub 제외) / 3-2 `tools/run_checks.py`(4칸) / 3-3 분기 46→0 / 3-4 legacy 4건 퇴역·address-table⊃address-static / 3-5 split-module 190→약 60초. 미달: `-Changed` 미스 77초(목표 60), 중복 쌍 3건 유지(저비용), `verify_callsite_compat` 유지(1-E 뒤).
- W4 서비스·레지스트리·save host(직렬, 서비스마다 커밋): 4-0 기술자·생성 표(설치 순서 로그 해시 동일) / 4-1 의존 끄기 폐포(끄기 집합 동일) / 4-2 map-tick / 4-3 overlay 구독 / 4-4 ui-dialogs·ui-callable / 4-5 save host+매니페스트(F-09) / 4-6 domestic-tick·game-day·slots, adapter 기능 코드 0 / 4-7 낡은 순서 제약 제거.
  - 2026-11-07 W4 완료 기록(브랜치 wt/mod-w4): 레지스트리는 features.json schema 3 대신 별도 `development/module_registry.json`(flags/requires/rows/subscriptions)이고 `tools/gen_module_table.py`가 `src/module_table_convenience.inl`·`module_closure.inl`·`module_flags.h`·`module_overlay_subscriptions.inl`을 생성(`--check`는 module-registry 경로). 4-0 split_activate 38행을 행 표로(순서 해시 `d0c6cb0b` = 기존 순서, `tests/data/feature_order_7b343e3.txt`) / 4-1 `feature_off_closure`가 옛 재AND 4줄과 모든 플래그·플래그 쌍에서 같은 끄기 집합(`tests/module_registry_fixture.cpp`) / 4-2 map-tick / 4-3 overlay(그리기 순서 terrain·sprites·preview·briefing, 진입 검사 순서 briefing·terrain·sprites·preview 상수) / 4-4 ui-dialogs 사용자 목록·ui-callable(`native_ui_callable.inl`) / 4-6 domestic-tick(awards→injury→domestic pre, domestic post; guard 실패 시 domestic·awards·injury·herbal 순 끄기)·game-day(어댑터 코어로 이동), 각 서비스 순서는 `tests/core_services_fixture.cpp`가 등록 순서 전체 순열로 증명 / 4-5 `src/save_host.h`(원자 쓰기 1벌, path 규칙, 플러그인 4개 terrain 10·tactic 20·merit 30·warehouse 40, 매니페스트 `<name>.<sha256>.sidecars` 마지막 게시, 로드 분류·고아 로그, `tests/save_host_fixture.cpp` 단계별 실패 주입) / 4-7 낡은 `layout_wait after ccz_title` 제거(순서 동일). 미완·결정: SplitPointer 행(slots)은 hook_map 추출기가 `native_split_install.inl` 본문 행을 읽으므로 손으로 둔 채 owner enum 중 CczDialog만 ui_dialogs 서비스로 바꿨고(나머지 owner enum 유지), 행 생성은 hook_map 갱신(W5)과 함께. 어댑터 `integrated` 레거시 경로는 같은 구독을 정적 초기화로 직접 등록(D-1에서 삭제). 수용된 개선(코디네이터 승인, 의도적 편차): (1) sidecar 쓰기·복원 예외를 플러그인별로 격리(`save_host::save_all/restore_all`의 플러그인마다 try/catch; 이전에는 merit 예외가 warehouse 쓰기를 건너뛰었고 tactic·terrain만 개별 처리), (2) 적재 중 `finish_load` 예외 시 terrain 포함 모든 플러그인이 `restore(nullptr)`로 reset(이전에는 terrain만 reset 안 됨), (3) 매니페스트 파일 1개 추가(sidecar 형식 불변). 검증기 보강: `terrain_edit_fixture`는 `save_host::path_calls`(SAVE_HOST_FIXTURE)로 옛 `io_paths` 스텁과 같은 경로 IO 카운터를 유지, `terrain_render_verify`는 터레인 구독 행(관찰→업로드 순서, 게이트 probe, context 요구 없음, 필터 없음)과 오버레이 소스 해시를 고정, `verify_traits9_save`는 merit_save·awards_save를 W4-5 검토본 바이트 해시로 고정.
- W5 DLL 경계(직렬): 5-1 생성 포함 목록(D3 0) / 5-2 cross-DLL API만(D2 0) / 5-3 /we4505.
  - 2026-11-07 W5 완료 기록(브랜치 wt/mod-w5): 5-1 `development/module_registry.json`의 `unity`(슬롯·파일·hosts·cond)가 `src/module_unity{,_combat,_convenience,_integrated}.inl`을 생성하고(`gen_module_table.py`가 hosts를 features.json 소유 DLL과 대조) `native_adapter.cpp`는 슬롯마다 `#define UNITY_SLOT_x` / `module_unity.inl` / `#undef`로 끌어 쓴다. 전투 루트는 편의 모듈 파일을 하나도 컴파일하지 않고(D3 32→0, 전투 DLL 1.16MB→0.56MB), 편의 루트는 전투 소유 모듈 파일을 컴파일하지 않는다(어댑터 본문의 전투 AI 코드는 `#ifndef SPLIT_BASE` 울타리, W6-D에서 모듈 파일로 이동). 소유 보정: native_adjacency.inl→expert-combat(편의만 게시), sidecar_cache.h→편의, log_append.h→core. 두 루트가 설계상 같이 컴파일하는 무상태 정책 하나(`native_district_intercept.inl`)만 `unity.shared`에 사유와 함께 선언(feature_deps가 읽음). 5-2 D2 9→0: 위 소유 보정과 포함 분리로 소멸, 남은 하나(path_fast의 로컬 sortie_movement 사본)는 편의 루트에서 원작 이동력 함수를 직접 부르도록 바꿈(편의 루트에는 코호트 상태가 없어 결과 동일). SplitPointer 행·owner enum·owner 게이트는 레지스트리 `pointers`에서 `src/module_pointers.inl`로 생성되고 hook_map이 그 파일을 읽는다(598지점·전투 88·편의 178 불변). 5-3 `/we4505`: 운영·진단·비용 프로파일 DLL 빌드 전부(분리 fixture 빌드만 `/wd4505` 유지). 헤더/클라이언트 라이브러리 함수는 `inline`, 진단 로그나 fixture만 쓰는 함수는 `[[maybe_unused]]`, 완전한 죽은 코드(campaign_edge, sortie_route_progress, ccz_module.cpp의 patch_pointer/restore_pointer)는 삭제. 단독 DLL 시나리오 `split_module_fixture --combat-alone / --base-alone`(상대 DLL 없이 준비·게시·롤백). 병렬 fixture 간헐 실패 원인: `layout_p3_fixture`의 임시 폴더가 프로세스 ID만으로 이름 붙고 지워지지 않아 같은 ID를 받은 다음 실행이 이전 실행의 FPS120.ini를 보고 "no FPS120 module"을 냈다(호출마다 새 폴더+종료 시 삭제로 수정). 설계 결정(2026-11-07, 사용자 확인 필요 시 뒤집을 수 있음): hub 소유 어댑터 본문의 전투 AI 코드 이동은 W6-D로 미룸.
- W6 family 이전(family별 병렬): A 인사(personnel, recruitment, injury, awards, domestic, merit, ui-common) / B CCZ / C 맵·메뉴(fast-menu, lord-change, speed-buttons, layout-wait) / D 경로·AI(path-fast, path-count, progress-native, dispatch-days, sortie, district-intercept, ai, expert-combat) / E modpack(F-03, traits9, trait-hover, battle-deaths) / F 턴 시작(appear-batch, face-keep, convenience-misc) / G 지형 / H 독립 소형.
  - 2026-11-07 W6 A/B/C/E/F 완료 기록(브랜치 wt/mod-w6a, 커밋 W6-A·W6-F·W6-E): 도구 보정 — `feature_deps.py`는 .inl을 포함하지 않는 독립 .cpp(gpu_profile.cpp, graphics_proxy.cpp)의 파일 범위 정의를 다른 파일의 참조 해석에서 뺀다(내부 연결 심볼이라 이름으로 부를 수 없음; `context`·`text`·`current` 같은 일반 이름이 legacy-candidates 간선 30여 개를 만들던 오탐 제거). A: 공용 기반(`sidecar_cache.h`, `friendship_save.h`, `friendship_store.h`)은 core 소유, `native_recruit_ai.inl`은 recruitment 소유(personnel↔recruitment 순환 소멸), awards→{personnel,domestic,ui-common}·domestic→{personnel,injury}는 기반 계층으로 `depends` 선언, merit 스크롤의 `profile` 매개변수·멤버를 `screen`으로 개명(region-officers 표 이름과 겹치던 오탐). F: 코어 서비스 `appear_prefetch`(`core_services.h`, svc_install id 6)로 appear_batch→face_keep 직접 호출·전방 선언(D4) 제거, `native_face_timing.inl`은 face-keep 소유. E(F-03): `native_modpack.inl`이 이미지 바이트를 직접 쓰지 않는다 — modpack_bytes 코드 12행+데이터 1행(`modpack_data_bytes`)+클램프 릴레이 span+성장 릴레이 span이 Layout 관리자의 owner `stats200` 하나로 `layout_client::Transaction`(LF_Y = YIELD_IF_PEER|ALL_OR_NONE) 안에서 등록되고(`before`는 가드 통과 뒤 이미지에서 복사한 사본, traits9와 같은 방식), `modpack_write`·`modpack_revert`·`prepare_link`·`modpack_restore_stats`는 삭제(되돌리기 = 관리자 unhook; 우리 바이트일 때만 되돌린다). 릴레이 자리는 E8/E9 분기가 아니라 일반 명령 5바이트라 `layout_client::jump/call`이 아니라 `span`(E9 + NOP 채움)을 쓴다. 한계: 16바이트 블록 단위 CAS(블록 여러 개에 걸친 행은 전체로는 원자적이지 않음), 안전 근거는 시작 시 단일 UI 스레드 1회 등록. Layout이 없으면 꺼진다(비라우팅 구성은 아무것도 적용하지 않음; 주소 가드 fixture만 해당). traits9는 자기 행 형식(`Trait9Bytes`)과 라우팅 전용 게시만 가지며 modpack 의존이 사라졌다. `native_modpack.inl`은 다른 기능 단위(battle_deaths, trait_hover, traits9)를 include하지 않고(허브 `native_split_install.inl`이 순서대로 포함) `tooltip_collab`을 읽지 않는다(플래그는 `prepare_modpack(stats,growth,collab,scenery)`·`publish_modpack(...)` 인자). trait-hover는 modpack-v16에 선언 의존(collab 부트스트랩·yield 상태). D6 modpack 5→0. `verify_modpack_static.py`의 포인터 겹침 스캔은 퇴역(hook_map R1/R2가 대신하고 stats200은 관리자 등록), `verify_layout_registry.py`의 직접 패치 금지 목록에 modpack·traits9 추가, `tests/layout_p3_fixture.cpp`에 실제 modpack 표로 만든 stats200 묶음 시나리오(한 번에 설치, 성장 제외, 행 하나 변경 시 전체 거부, 이미 리다이렉트된 클램프 거부, R1 Stats200 공존 시 묶음 양보, unhook은 남의 변경을 덮지 않음) 추가. 남은 것: D6(awards 2, injury 3, merit 11, convenience-misc 6, face-keep 3, appear-batch 1, ccz-sprites 1)은 원작 직접 패치 호출 그대로(코어 훅 서비스·Layout 행 필요, 주소표 사용자 비트맵 재생성이 따른다), terrain-edit→merit(`merit::SaveCoordinator::Completed`, family G)와 B·C는 미선언 간선이 없어 코드 변경이 없다.
  - 2026-11-07 W6-B 기록(브랜치 wt/mod-w6b, D·G·H): D 어댑터의 전투 AI 본문 전부를 `native_ai_core/build/wall/nodes.inl`(ai 소유, 전투 DLL 호스트, 유니티 슬롯 ai_support·campaign·departures)로 원문 이동하고 `#ifndef SPLIT_BASE` 울타리와 어댑터의 정책 헤더 4개 include를 없앴다(편의 루트는 campaign/obstacle/sortie/tactics 정책을 컴파일하지 않음, 전투·편의 DLL 크기 555,520/1,793,536 바이트로 이동 전과 같음). ai↔sortie는 공유 심볼 75개(Progress·그룹 상태·경로/적대 질의)라 인터페이스로 가르는 대신 한 모듈 가족으로 합쳤다(`sortie`=ai의 alias, SCC·D1·D4·D5 0). progress-native→path-count는 코어 서비스 `progress_flush`(core_services.h + 레지스트리 구독)로, terrain-edit→merit는 sidecar 작업을 `save_host::Job`으로, sidecar_cache.h는 dll-split(save host) 소유로, merit→region-officers 이름 충돌은 `guard_ranges` 이름 변경으로 풀었다. feature_deps는 이름 없는 namespace 정의를 내부 연결로 보게 고쳐 gpu_profile.cpp 때문의 legacy-candidates 거짓 간선(D1 55→20)을 없앴다. 전투 DLL 모듈은 ModuleDesc 표가 없고(편의 DLL만 표 구동) 모듈별 core_fixture_stub 픽스처는 만들지 않았다(기존 모듈 픽스처 유지). 미해결: d6:ai 5(bt_condition의 patch_pointer 2줄은 귀속만 바뀜).
- W7 레거시 도구·문서·빌드 변형: HISTORY.md, MODULES.md 생성, 도구 퇴역 묶음, 모듈 삭제, D-1. D8 0, 도달 불가 0.
  - 2026-11-07 W7 재검토·보완: D-1·도구/문서 퇴역을 실제 코드 기준으로 확인. 거절된 500명 친애 후보 구현·저장·게시 브리지·전용 fixture를 전부 제거했고, 현행 논공이 쓰는 Binding·작업 추적기는 `src/save_binding.h`에 분리(저장 형식·type identity 동일). 옛 통합 설치 코드는 제품 어댑터에서 `tests/native_install_fixture.inl`로 옮겨 기존 원자 게시/실패 복원 검사를 보존했다. `prepare_traits9.py` 퇴역: 고정 `traits9_payloads.h`와 기존 인덱스/청크 파서로 검사하고, 이전 생성기 보고서·설치 manifest·옛 idmap에 대한 검사 의존을 제거했다. origlib 의존은 없음(사용자: 아직 불완전하므로 이번 계획에 도입하지 않음). 수동 `MODULES.md`는 등록부 기반 생성/불일치 검사로 전환. 삭제 전 추적파일을 유령 소스로 세던 manifest/주소/의존 도구를 고치고, 소스 include는 따라가되 주석·문서 문자열은 도달 경로에서 제외했다. 낡은 v1 전법 계획·영구 DB 안내는 HISTORY에 남기고 현행 안내로 교체. 과거 해제본 4파일은 `backup/w7_retired_20261107/editor83_plain/`으로 이동, 해시 보존 확인. 도달 불가·끊어진 문서 링크·미소유 소스 0, D1/2/3/4/5/7/10 0. 이전의 “옛 빌드 문자열 수 = W7 미완료” 판정은 사용자 의도와 맞지 않아 철회: 역사 조회·이전 이전표·참고 모드 근거 문자열은 보존하며 실제 퇴역 코드/빌드 의존과 구별한다. 원시 문자열 수는 D8 진단 출력에 계속 공개하고 숨기지 않는다. 검증 로그 `build/development/w7_completed_split.log`, `w7_cleanup_selftests.log`, `w7_retirement_checks.log`; 설치/배포/실게임 실행은 아직 없음.
- W8 전법 우선순위(마지막): 모듈화, compat 헤더 제거, .tactics 플러그인, 엄격 모드(D1·D10 0), 시간 목표 측정.
  - 2026-11-08 W8 완료 기록(주 폴더): 전법 우선순위 6파일(`native_tactic_priority.inl`, `native_tactic_priority_grid.inl`, `tactic_priority_profile.h`, `tactic_memory_store.h`, `native_tactic_memory_save.inl`, `tactic_priority_io.h`)이 `GA_<1a4a447d RVA>`를 직접 쓰도록 20개 별칭을 치환하고, `src/game_addresses_compat.h`·`development/addr_promote/compat_files.txt`·`addr_promote compat` 하위 명령·`check_addresses (b3)`·`hookmap_extract` 별칭 해석을 삭제했다. hook_map은 553지점·소유 65·경고 0으로 그대로이고 Layout 전법 행 4개도 새 id로 같은 주소를 가리킨다(전법 우선순위는 이미 `layout_client::ui_*`/`Transaction`으로 Layout DLL에 UI를 맡기고 있어 추가 모듈 이동은 없었다). `.tactics` 플러그인은 W4-5에서 이미 save_host 플러그인(순서 20, TPRM v2, manifest)으로 들어가 있었고 형식 불변(I-4) 확인만 했다(`save-host` 179·`tactic_memory` 40 검사). 엄격 모드: `feature_deps_policy.json`의 `strict_rules: ["D1","D10"]`가 기준선과 무관하게 위반을 실패시키고 자체 검사 31개가 이를 증명한다(현행 D1 0·D10 0, D2~D5 0). 시간은 docs/CHECKS.md W8 표: `-Feature dll-split -Build` 140.8초(목표 4분 이내), `-Changed` 캐시 미스 93.2초·적중 17.1초(목표 60/15 미달, split-module 지배). 주소표 정리: 미사용 거부 id 2개를 삭제하고 `GA_2B2E28`의 mid 기준을 새 컨테이너 `GA_2B2CC9`로 다시 도출해 2,057→2,056 id 전부 해석(해석기 `denied_by_policy 0`, `unresolved 0`). 진단 정상화: 매 실행 실패하던 Layout 수명 프로브(`probe_lifetime` → `layout_p4_life`/`layout_ui_load_failed`)를 퇴역하고(`layout_p4_fixture`는 footer 생성·소멸만 검사), 정상 동작인 이전 해시 sidecar 수 로그를 `sidecar_orphan`→`sidecar_old_versions`로 바꿨다. D9는 registry schema 한계로 여전히 미평가다. 설치·배포·실게임 실행 없음.

- W9 모드 매니저(2026-11-08 시작): `docs/MOD_MANAGER_DESIGN.md`(런타임 비용 0 불변식, 매니페스트 schema 1, 호환성 검사, 설치기 2단계), `development/modules/<id>/module.json` 61개(중앙 등록부에서 생성·`gen_module_manifests.py --check/--apply`), `development/module_catalog.json`(기계 판독용), `tools/mod_compat.py`(의존성·순환, 훅 자리/같은 주소/16바이트 블록, ini·UI·API 중복, 영향 검사 출력, `--selftest` 9), `module-manifests`를 always 검사에 추가했다. 2단계: `development/installer_map.json` 94항목(키→모듈)에서 설치기 GUI/PS 카탈로그를 생성(문구 차이는 `label_ps`/`note_ps`로 보존), 매니페스트 `installer.items`·`ini` 자동 채움, `gen_installer_catalog --check`로 카탈로그·D9(설치기 Requires ↔ 모듈 depends) 검사, `installer_preflight.py`로 현재 기능설정 검사(0문제). 런타임 소스·DLL 무변경(`gen_module_table --check`, `feature_deps` D1/D10 0·경고 0, hook_map 553/0, `dll-split -Build` 통과). 3단계: GUI가 `ModuleOf`/`ModuleDepends`/`ModuleConflicts`/`ModulePrimary`로 모듈 의존성 자동 켜기·충돌 거부·의존 모듈 경고를 수행한다(기존 Requires/묶음 규칙 유지, installer_gui 484·왕복 827). 남은 후보: 중앙 등록부 생성 방향 전환, 외부 패키지 매니페스트 가져오기.

## 8. 결정
| ID | 내용 | 결정(2026-11-07) |
|---|---|---|
| D-1 | 통합 빌드·후보 빌드 퇴역(AGENTS "회귀 비교용 유지" 변경) | 사용자 확정(2026-11-07): "분리 빌드로 가 옛날방식은 정리". W7에서 통합·후보 빌드 경로·검사 삭제, 산출물은 backup/ 이동, 같은 커밋에서 AGENTS.md 문장 교체 |
| D-2 | 중복·legacy 검사 퇴역 날짜 | W3-4 일괄(사용자 목표 "슬림화"에 따름) |
| D-3 | traits9 생성기 | 생성물 src 고정, 생성기 퇴역 |
| D-4 | 추적 안 되는 구 산출물(구 EXE, running_code_text.bin, rva_map, native_analysis_only.bin) | 삭제가 아니라 W2-3 리허설 뒤 backup/ 이동. 사용자 확정(2026-11-07): "옛산출물들은 BACKUP폴더에 넣자" — 영구 삭제 없음 |
| D-5 | 고아 sidecar | 로그만 |
| D-6 | GA id 명명 | 현재 RVA, 패치마다 promote |
| D-7 | W1-E 동결 창 | 전법 우선순위 UI 병합 직후 |

## 9. 위험·실게임 확인·진단
위험: 대량 rename 충돌(동결 창, compat 격리, rename 맵) / guard 리터럴 오류(등가성 덤프+R13/R14+GH 이중 출처) / 유니티 빌드 이름 가림(단계마다 두 DLL 컴파일, 내보내기 대조) / 설치 순서 변화(순서 로그 해시, 명시 after) / /we4505(DLL별 마지막) / feature_deps 오탐(정책+래칫) / 캐시 누락(생성 표·fixture 이미지 도장, 설치 전 캐시 없음) / 병렬 출력 충돌(parallel_safe) / stats200 Layout 의존 / 매니페스트 혼합(파일 자체 유효성 우선).
실게임(사용자): W1-E 후 정상 진입·addr_build 기준·unresolved 0·feature_off 동일 / W4-0·4-1 feature_order 해시 동일 / W4-2 fast_menu·lord_change 조합 / W4-3 그리기 겹침 순서 / W4-4 ccz_sprites OFF+tactic_priority ON / W4-5 sidecar 왕복·구 세이브·absent_off / W5 단독 DLL 설치 / W6-E 아이템 0x7B 상한·성장·traits9 공존 / W8 전법 UI·.tactics 왕복(2026-11-08 사용자 확인: 팝업·기억·재로드 복원, 거부 0).
진단 log_event(groups.diagnostics, NO_RUNTIME_LOG 제거): feature_order, feature_off(reason 확장), svc_install, overlay_sub, sidecar_write, sidecar_manifest, sidecar_load, sidecar_old_versions, stats200_rows, addr_build/addr_unresolved, addr_unknown_summary.
