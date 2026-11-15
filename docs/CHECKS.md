# 검사 체계 (변경 기반 선택 + 캐시)

설계 근거: [HISTORY.md](HISTORY.md). 이 문서는 사용법 요약이다. 모든 검사는 원작 EXE 분석·게임 실행·설치 없이 돌고 `build/development` 안에만 쓴다.

## 실행

```powershell
# 개발 중: 바뀐 파일 → 소유 모듈 → 그 모듈 검사만 (게임 루트에서는 AI_TacticsMod\ 접두)
& .\AI_TacticsMod\tools\check_feature.ps1 -Changed -List     # 계획만(표 + JSON)
& .\AI_TacticsMod\tools\check_feature.ps1 -Changed           # 계획 실행, 통과 시 캐시 기록
& .\AI_TacticsMod\tools\check_feature.ps1 -Changed -Base main   # 작업본(worktree): 분기점 이후 전부
& .\AI_TacticsMod\tools\check_feature.ps1 -Changed -Paths src/a.h,src/b.cpp   # git 상태 대신 지정 파일
& .\AI_TacticsMod\tools\check_feature.ps1 -Changed -NoCache  # 캐시 무시
# 모듈 하나를 직접 지정 (캐시 없음, 기존 동작)
& .\AI_TacticsMod\tools\check_feature.ps1 -Feature tactic-priority [-List] [-Build]
# 설치·배포 전 필수 (캐시 없음)
& .\AI_TacticsMod\tools\check_feature.ps1 -Feature dll-split -Build
```

- `-Feature`와 `-Changed`는 둘 중 하나만. `-Base/-Paths/-NoCache`는 `-Changed` 전용.
- 변경 파일 = `git diff --name-only [merge-base]` + untracked. 주 폴더에서는 다른 세션의 미커밋 수정도 변경으로 잡힌다(안전하지만 느림) → 작업본 또는 `-Paths`.
- `-Changed -Build`는 선택 모듈 중 `build` 값이 `split`이 하나라도 있으면 `build_split.ps1`(통합 `native` 빌드 종류는 D-1로 퇴역, `build`는 `split|none`). `-Feature -Build`도 `features.json`의 `build`를 먼저 보고 없으면 기존 목록을 쓴다.
- `-Changed -List` 출력: 표 + 마지막에 JSON(`base, changed, modules[{name,reason}], routes[{name,action,key,est_s,last_s,reason}], unowned, warnings`). `action`은 `run | cached | subsumed`. 소유자 없는 파일은 경고와 함께 `dll-split`로 대체된다.
- 도구 직접 호출: `python -B -X utf8 tools/select_checks.py plan --changed [--base REV] [--paths a,b] [--no-cache] [--json]`, `key <route>`, `record <route> --key K --seconds S`, `explain <file>`, `python -B -X utf8 tools/route_inputs.py <route>`(입력 목록·키).

## 실행기 `tools/run_checks.py` (W3, 2026-11-07)

`check_feature.ps1`은 선택(`select_checks.py`)과 출력만 하고, 실행은 `development/features.json`의 `routes.<이름>.steps`를 `tools/run_checks.py`가 한다(ps1 분기 0개, 하드코딩 빌드 종류 목록 없음).

```powershell
& .\AI_TacticsMod\tools\check_feature.ps1 -Feature dll-split -Build            # 설치 전 필수, 병렬 실행(기본 4칸)
& .\AI_TacticsMod\tools\check_feature.ps1 -Changed -Base main -Jobs 2 -Priority idle   # 칸 수·우선순위 지정
python -B -X utf8 AI_TacticsMod/tools/run_checks.py --routes split-module,native --list   # 단계 그래프만 보기
python -B -X utf8 AI_TacticsMod/tools/run_checks.py --routes split-module --keep-going     # 실패해도 나머지 계속
```

- 한 번의 실행 전체가 하나의 풀(`--jobs`, 기본 min(4, CPU), 상한 4)을 공유한다. 자식 프로세스는 BELOW_NORMAL(`-Priority idle`이면 IDLE; 환경변수 `SAN14_CHECK_PRIORITY`).
- 단계(step) 종류: `{"py","args"}`(python -B -X utf8) / `{"ps1","args"}`(powershell -File) / `{"exe","args","cwd","save"}`(`save`는 마지막 stdout 줄을 BOM 없이 저장) / `{"cl":{flags,defs,inc,src,out,dll,link},"cwd"}`(MSVC 경로·`flags:"split"`은 `tools/build_split.ps1`에서 읽음, 오브젝트는 `cwd/obj/<out>/`로 분리). 인자 `@split_image`는 build_split.ps1이 fixture exe에 넘기는 이미지 경로.
- 단계 키: `id`, `needs`(같은 경로의 단계 id, 기본 = 바로 앞 단계, `[]` = 바로 시작), `lock`(같은 lock 단계는 동시에 안 돎), `prio`(경로 안 시작 순서, 클수록 먼저), `cwd`. 경로 키: `parallel_safe`(true인 경로만 풀을 나눠 쓴다. 아니거나 없으면 그 경로 단계가 도는 동안 다른 것이 시작하지 않는다), `est_s`, `budget_s`(없으면 est_s; 마지막 실행이 1.5배를 넘으면 `-List`·계획에 경고).
- 순서: always 경로 먼저 → 단계 `prio` → 추정 시간 긴 순. 실패하면 그 경로의 남은 단계는 건너뛰고 새 단계를 더 시작하지 않는다(`--keep-going`이면 다른 경로 계속). 통과한 경로만 캐시에 기록한다(계획 JSON의 키, 실행 중 입력이 바뀌면 기록 안 함).
- 병렬 안전 근거: `parallel_safe:true`인 경로는 서로 다른 소스 이름·출력 이름만 쓴다(`native`·`sortie`는 둘 다 `tests/native_fixture.cpp`를 `build/`에서 컴파일하므로 컴파일 단계에 lock `native-fixture-obj`). `split-module`의 layout fixture 실행 3개는 모듈 폴더 상태를 공유해 겹치면 실패하므로(`no FPS120 module`) lock `layout-runtime`. 새 경로를 병렬로 하려면 출력 파일·임시 폴더·전역 이름 충돌부터 확인한다.
- 자체 검사: `python -B -X utf8 AI_TacticsMod/tests/verify_run_checks.py`(단계 그래프, 칸 수 상한, lock, 단독 경로, fail-fast, save, 캐시 기록, cl 명령 조립, 실제 features.json 검증).

## 항상 도는 검사

`features.json`의 `always`. `manifest`(`tools/check_manifest.py`, 소유 매니페스트 정합), `address-static`(`tools/check_addresses.py --skip-cpp`), `hook-map`(`tools/hook_map.py`), `feature-deps`(`tools/feature_deps.py --check`, 모듈 간 의존 래칫: 새 간선·카운터 증가·새 도달 불가 파일·D7 = 실패, 기준선 `development/feature_deps_policy.json`, 자체 검사 `tests/verify_feature_deps.py`; 캐시는 도구 내부 내용 해시 캐시를 쓰므로 route는 `cache:false`). 이들은 `-Changed`에서 변경 파일과 무관하게 항상 포함되고 먼저 실행된다. `manifest`는 `development/features.json`이 바뀌면 `always`가 없어도 선택된다.

## 매니페스트 키 (development/features.json)

기존 `sources`·`tests`는 그대로, 새 키는 덧붙이기. 키가 없으면 `owns = sources`, 캐시 없음, `always = []`로 동작한다.

| 위치 | 키 | 뜻 |
|---|---|---|
| 최상위 | `schema`, `ownership_scope`, `ownership_ignore` | 소유 검사 대상 경로·무시 glob |
| 최상위 | `always` | 항상 도는 경로 |
| 최상위 | `routes{이름:{est_s,budget_s,cache,inputs,external,subsumes,steps,parallel_safe,runner,args,python,covered_by}}` | 경로 메타. `cache:true`만 캐시. `subsumes`는 다른 경로가 포함하는 검사(`split-module`⊃`layout-registry`, `address-table`⊃`address-static`). `steps`는 실행 단계(위 실행기 절), `runner/args/python`은 단계 하나짜리 옛 형식 |
| 최상위 | `unrouted_checks`, `router`, `pending_routes` | 미연결 검사 분류, 파일→라우팅 규칙(`src/layout_registry.h`: `registry-owner`) |
| 모듈 | `status` | active/candidate/legacy/alias/aggregate/tooling |
| 모듈 | `owns` | 배타 소유 glob(`**` 지원). 변경 파일 → 모듈 판정 |
| 모듈 | `hook_owners` | 이 모듈이 소유한 Layout 행 owner. `layout_registry.h` 행 변경을 모듈로 보냄 |
| 모듈 | `depends` | 의존 모듈. 바뀐 모듈에 의존하는 모듈도 선택(역폐포) |
| 모듈 | `hub` | true면 바뀌어도 의존 모듈로 전파하지 않음 |
| 모듈 | `dll`, `build`, `alias_of` | DLL 구분, `split|native|none`, 별칭 원본 |

선택 규칙: 소유 모듈 → `depends` 역폐포(hub 제외) → **영향 확장(3-1)**: 바뀐 `src/` 파일의 정의를 쓰는 모듈(`feature_deps.py impact`: 심볼·플래그·hook site 간선, 추적 파일은 바뀐 줄이 든 정의 단위로 좁힘, `--paths`는 파일 전체)을 추가. hub 모듈은 영향으로 선택하지 않는다(어댑터·설치기·core는 모든 기능을 include·install로 참조하므로 그 경로는 설치 전 `-Feature dll-split`의 몫). 스캐너가 실패하면 경고만 내고 소유자·depends 선택을 유지 → `tests` ∪ `always` → `subsumes` 제거 → 캐시 키 같으면 `cached` → 정적·est_s 오름차순. `src/layout_registry.h`는 `git diff -U0`의 ± 줄 `{"owner",`로 해당 `hook_owners` 모듈에만 보내고, 행이 아닌 줄이 바뀌거나 owner를 못 찾으면 `layout`. `docs/*.md`는 경로가 없다.

## 캐시

- 위치 `build/development/check_cache/<route>.json`(작업본마다 따로). `-Feature`는 캐시를 쓰지 않는다.
- 키 = sha256(입력 파일 내용 + 외부 도장 + 분기 블록 텍스트 + routes 항목 + 툴체인 도장). 입력은 `route_inputs.py`가 ps1 분기에서 따라간다: 중첩 ps1, C/C++ `#include "..."` 폐포, Python 로컬 import(소스에 `'src`가 있으면 `src/**`). `research/`·`legacy/`·`build/ui_native_analysis/`·`loader_split/*_payloads.h`는 내용 대신 크기·mtime 도장. 툴체인은 `$taskVc`/`$taskSdk` 문자열과 `cl.exe` 크기·mtime.
- `cache:true`는 `routes`가 명시한 경로(fixture 컴파일·실행, 정적 검사)만. installer-*, layout-restore, progress, 설치된 게임 파일을 읽는 검사는 false.
- 통과했을 때만 기록하고, 실행 후 키를 다시 계산해 실행 전 키와 같을 때만 저장한다(실행 중 입력이 바뀌면 `result not cached`).
- 한계: `route_inputs`가 입력을 놓치면 오래된 캐시가 통과할 수 있다(그래서 캐시는 명시 경로만, 설치 전 `-Feature dll-split -Build`는 캐시 없음). `build_*.ps1`이 읽는 파일이 문자열이 아닌 방식(변수 조합만)으로 지정되면 잡히지 않는다. 의심되면 `-NoCache`.

## 훅 지도 규칙 요약 (패키지 A, `tools/hook_map.py`)

소스에서 정적으로 지점(VT 칸·COM·CALL/JMP·블록·스팬·바이트)을 뽑아 규칙 위반을 찾는다. 요약(자세히는 계획서 4절): R1 VT·COM 칸 owner 공유 / R2 바이트 범위 겹침 / R3 같은 16바이트 블록에 다른 owner / R4 블록 제약 / R5 차단 입구 / R6 관찰점 접두부 겹침 / R7 외부 모드 겹침과 relations / R8 화이트리스트 등록 / R9 owner별 등록 파일 / R10 두 DLL이 같은 지점 / R11 Layout 용량 / R12 종류 일관성 / R13 EXE 바이트 검증(`--no-exe`로 끔) / R14 가드. 겹침 결정은 `development/hook_policy.json`·`foreign_hooks.json`에 이유·날짜. 새 모듈 작성 전 `tools/hook_map.py --probe GA_<site>`.

## 중복 검사 쌍과 퇴역 (W3-4, 2026-11-07, 계획서 D-2 일괄 퇴역일)

중복 쌍(같은 단언을 두 경로가 하는 경우):

| 쌍 | 처리 | 근거 |
|---|---|---|
| `address-table` ⊃ `address-static` | **한 번만 돈다**: `address-table`이 `subsumes:["address-static"]`(`-Changed`에서 둘 다 선택되면 `address-static` subsumed) | `address-static` = `check_addresses.py --skip-cpp` = `address-table`의 b 검사 + c4(observer) 부분집합 |
| `layout-registry` ⊂ `split-module` | 이미 subsumed(W0) | split-module의 `v_registry` 단계가 같은 검사 |
| verify_layout_registry 검사 1·3 ↔ hook-map R13·R5 | **유지**(0.4초, split-module 안) | 단언 제거는 R13·R5 동치 증명이 필요해 보류, 비용이 무시할 만함 |
| verify_modpack_static 겹침 ↔ hook-map R1·R2 | **유지**(trait-hover 경로 안) | 위와 같음 |
| check_observer_hooks c4c ↔ hook-map R6 | **유지**(address-static 안, 1초 미만) | 위와 같음 |

퇴역한 legacy 검사(git 이력에 남음; `unrouted_checks`·모듈 `sources`에서 제거, 미연결 legacy 5 → 0, orphan-failing 1건은 분류만 유지):

| 퇴역 | 무엇이었나 | 커버하는 현행 검사 / 근거 |
|---|---|---|
| `tests/verify_session_release.py` | 분리 전 v12 릴리스 바이너리 증명. 로그 이름을 부분 문자열로 비교해 오탐, 파일 머리말에 "현재 흐름에서 돌리지 말 것" | 릴리스 DLL export·ABI는 `tests/verify_split_exports.py`(build_split.ps1 본 빌드 단계), 로더는 `verify_loader.py`·`verify_layout_loader.py`, 기능은 dll-split 전 경로. 어느 경로에도 연결된 적 없음 |
| `tests/verify_input_loader.py` | 옛 단일 DLL 입력 로더(`SAN14_AIInput.dll`·`input_fixture.exe`)가 비지원 EXE에서 `unsupported_executable`을 남기는지 | 그 산출물을 만드는 경로가 없음(`build_native.ps1 -InputFixture/-InputLoader` 옛 변형). 현행 로더는 `module_loader.cpp`+`input_proxy.cpp`이고 `verify_loader.py`(순수 로더 fixture·모의 DLL)가 대체. `unsupported_executable` 로그는 어댑터의 PE 검사(`native_adapter.cpp`)에 남아 있고 이를 단언하던 검사만 사라짐(옛 로더 전용 구성) |
| `tests/verify_input_update.py` | `package_input.py`(옛 단일 DLL 설치기)의 보존·롤백 | 현행 설치/롤백은 `installer-atomic`·`installer-layout`·`layout-restore`·`verify_local_install_rollback.py`(후보, 수동)와 `install_local.py` |
| `tests/verify_package.py` | 옛 로더 fixture 패치(`package_mod.patch_existing_graphics_import`) + 옛 빌드 `.text` 재배치 0 확인(텍스트 크기 20179374 = 구 빌드) | 구 빌드 전용 단언(Steam 최신만 대상, 구 빌드 정리는 W1·W7 몫). 현행 주소·가드 검증은 `address-table`·`hook-map` R13/R14 |

미퇴역으로 남긴 것: `verify_callsite_compat.py`(split-module `v_callsite`, 14초 병렬 단계) — 구 이미지 기반이라 R13/R14로 대체할 계획이나 의미 검사를 현재 이미지로 옮기는 1-E(fixture 이미지 전환) 뒤로 미룸. 그 전에 지우면 sortie 호출 지점 표(49/21/79개) 단언이 사라진다.

## SESSION_RULES §4 제안 문구 (제안만, 총괄 세션·사용자 승인 후 반영)

> 개발 중: `tools/check_feature.ps1 -Changed`(작업본은 `-Changed -Base main`). 설치·배포 전: `-Feature dll-split -Build` 1회(캐시 없음). 새 모듈·훅은 features.json owns·hook_owners·tests·depends에 등록하고 `tools/hook_map.py --probe`로 먼저 확인. 겹침 결정은 hook_policy.json·foreign_hooks.json에 이유·날짜. 기능마다 교차 검사 스크립트를 새로 만들지 않는다.

## 자체 검사

경로 `checks-selftest`(`checks` 모듈 변경 시 선택, 다섯 검사를 병렬 단계로 실행): `verify_check_manifest.py`, `verify_select_checks.py`, `verify_run_checks.py`, `verify_feature_deps.py`, `verify_hook_map.py`. `verify_select_checks.py`는 W3에서 영향 확장(주입 함수·hub 제외·실패 경고·기본 영향 생략)과 예산 경고 검사를 더했다.

`python -B -X utf8 AI_TacticsMod/tests/verify_select_checks.py` — `build/development/select_selftest/`에 임시 git 저장소(+worktree)를 만들어 전파·hub·hunk 라우팅·docs만 변경·untracked·소유자 없음·`--paths`·`--base`·`subsumes`·캐시 적중/무효/실행 중 변경·include 폐포·외부 도장·worktree를 확인한다.

## 시간 기준선 (2026-11-07)

W0-4 실측(작업본 `wt/mod-w0`, 같은 PC). 개선 목표는 docs/MODULARIZATION_PLAN.md 0장.

| 항목 | 실측 | 비고 |
|---|---|---|
| always `manifest` | 1.0초 | `tools/check_manifest.py` |
| always `address-static` | 1.8초 | `check_addresses.py --skip-cpp` |
| always `hook-map` | 1.8초 | `hook_map.py` (R13 포함) |
| always `feature-deps` | 캐시 미스 4.4초 / 적중 0.9초 | 파일별 스캔 + D8·도달성; 내용 해시 캐시 |
| always 4종 합 | 적중 약 5.5초 / 미스 약 9초 | 목표 합 20초 이하 달성 |
| `check_feature.ps1 -Changed -Base main -List` | 64.7초 | 거의 전부 `select_checks`의 캐시 키 계산. `route_inputs.py manifest` 단독 55~67초(주 폴더도 동일, 입력 4267파일: `check_manifest.py`의 src 문자열 때문에 `src/**`를 따라가고 문서 참조로 `backup/`·`data/`까지 끌려 들어감). 목표 "적중 15초 이하"를 막는 1순위 병목이므로 W3-1/3-2에서 키 계산이 필요 없는 always 경로는 키를 만들지 않는다 |
| `-Feature dll-split -Build` | 357초 시점에 중단(exit 1) | `address-table`(`check_addresses.py` 전체)에서 실패해 빌드 단계까지 못 감. 완주 시간은 미측정 |

`-Feature dll-split -Build` 중단 원인은 이번 변경과 무관한 기존 실패다(주 폴더에서도 재현): `(a) src/game_addresses_data.inl (2067 ids) is stale against addr_table.json (2071 ids): run tools/gen_signatures.py all`. 주 폴더의 `addr_table.json`(research/rva_map, 추적 안 됨)이 data.inl보다 4개 많은 상태이므로 data.inl 재생성 또는 표 정리 뒤 재측정이 필요하다. 그 전까지 설치 전 전체 빌드는 이전 기록(약 6~6.5분)을 기준으로 둔다.

`feature_deps.py` 첫 기준선(2026-11-07): D1 75간선/211심볼, D2 16, D3 32파일(.inl 22), D4 6쌍, D5 6, D6 직접 패치 호출 62, D7 0, D10 4개 SCC, 훅 공유 4쌍, 구 빌드 해시 언급 88파일/196회, 구 이미지 경로 43파일, 구 확장기 참조 40파일, 없는 docs 참조 18, 도달 불가 tools/tests 34(+문서에만 언급 4).

- 2026-11-07: `route_inputs.py`가 backup/·data/·dist/를 내용 해시하지 않고 크기·시각 도장만 찍도록 바꿈. `manifest` 입력 계산 65초 → 8초, `-Changed -List` 65~331초 → 12초.

## W3 시간표 (2026-11-07, 작업본 `wt/mod-w3`, 같은 PC, 4칸)

| 항목 | 전 | 후 | 비고 |
|---|---|---|---|
| `-Changed -Base main -List` | 12초(65초 → 도장 수정 후) | 12.2초 | 키 계산이 대부분. 영향 확장은 src 변경이 있을 때만 `feature_deps` 분석(캐시 적중 1초, 미스 4~10초) |
| `-Changed` 캐시 미스(말단 기능 src 하나, `-Paths src/native_name_lookup.inl -NoCache`와 같은 선택: always 4 + 기능 + `split-module`) | 약 3.8분(추정: 계획 12 + always 9 + 기능 15 + split-module 190) | **77.2초**(계획 8.6 + 실행 68.6, split-module 60초가 지배) | 목표 60초 미달. 병목 = split-module의 CPU 합 약 215초 ÷ 4칸 |
| 같은 경우 캐시 적중 | 계획 12 + feature-deps 1 = 약 13초(추정) | **10.1초**(always 3종·기능·split-module 캐시, feature-deps만 실행) | 목표 15초 이하 달성 |
| `split-module` 단독 | 190.6초(측정, `build_split.ps1 -Fixture` + callsite 포함) | **54~66초**(60.2초, 65.7초, dll-split 안에서 54초) | 같은 컴파일 6개·실행 25개·검사 단언, 컴파일은 단계 병렬, compat 23회는 동시 4개 |
| `-Feature dll-split -Build` | 432초(측정) | **173.6초**(검사 108초 + 본 빌드 65초; 앞선 실행 187.3초) | 목표 4분 이하 달성. 본 빌드(`build_split.ps1`)는 아직 직렬 |
| `check_feature.ps1` switch 분기 | 46 + 빌드 종류 하드코딩 목록 | 0 | 경로 51개 전부 steps/runner 데이터 |

남은 병목(후속 웨이브 후보): split-module의 compat 23회는 매번 5.3초(공통 앞부분 3.5초 반복, 합 약 120 CPU초)이고 `v_callsite`가 14초. compat를 모듈 경로로 나누면 말단 기능 수정의 `-Changed`가 60초 이하가 되지만, 현재 split-module을 가진 모든 모듈이 23개 전부를 보호받으므로 어느 모드가 어느 기능 가드를 보호하는지 모듈별로 증명한 뒤에만 줄일 수 있다(불변식: 보호 집합 축소 금지). 본 빌드 직렬 구간 약 65초는 `loader_split`의 `.obj` 이름 충돌(fixture는 `cwd/obj/<out>/`로 이미 분리) 때문에 지금은 검사 뒤에 둔다.

## W8 시간 측정 (2026-11-08, 주 폴더, 4칸)

W8은 코드 동작을 바꾸지 않는 재기준·엄격 모드·측정 웨이브다. 계획 목표는 docs/MODULARIZATION_PLAN.md 0장.

| 항목 | 실측 | 비고 |
|---|---|---|
| always `feature-deps` | 0.7초(적중) / 4.1초(미스) | 170파일; compat 헤더가 빠져 171→170 |
| `-Changed -Base main -List` | 12초대(변경 없음) | W3과 동일, 이번에 재측정하지 않음 |
| `-Feature dll-split -Build` | **140.8초** | 검사 89.1초(17경로) + 분리 DLL 5개 빌드. 목표 4분 이하 유지 |
| `-Feature tactic-priority` | 70.7초 | split-module 61 + address-table 38(병렬) + merit 10 + tactic 7 |
| `-Changed -Paths src/native_tactic_priority.inl -NoCache` | 93.2초 | 목표 60초 미달. split-module 68초가 지배, address-table 15초 추가 |
| 같은 경우 캐시 적중 | 17.1초 | 목표 15초에 2초 초과(계획·키 계산 약 16초, 실행 1초) |
| split-module 단독(위 실행 안) | 57~68초 | W3의 54~66초와 같은 범위 |

`-Feature dll-split -Build` 산출물(`build/development`, 미설치): dinput8 `44b5bb14…`, CombatAI `2a875fd5…`, Convenience `61359969…`, CCZ `abe961d6…`, Layout `e008f795…`. `module-registry` 순서 해시 `d0c6cb0b`(W4-0과 동일), 38행·75플래그, `save-host` 179검사. 설치·배포·실게임 실행 없음.

## W9 검사 (2026-11-08, 모드 매니저 1·2단계)

| 항목 | 실측 | 비고 |
|---|---|---|
| `module-manifests`(always) | 약 1초 | 매니페스트 61개 일치, 설치기 카탈로그 94개 일치, `mod_compat --selftest` 9 |
| `installer-gui` | 2.6초 | GUI/PS 카탈로그 452, 기능 스위치 77 |
| `build_installer.ps1` | 약 25초 | 정적 478 + 왕복 822 |
| `installer_preflight.py` | 1초 | 현재 기능설정 → 37모듈, 0문제 |
| `-Feature dll-split -Build` | 144.1초 | always 경로에 매니페스트/카탈로그 검사 포함 |

런타임 소스·DLL 코드는 바꾸지 않았다. 설치기 카탈로그만 `development/installer_map.json`에서 생성되므로, 다음 배포 빌드에서 설치기 EXE만 새로 만들어지고 게임 DLL은 같은 코드다.
