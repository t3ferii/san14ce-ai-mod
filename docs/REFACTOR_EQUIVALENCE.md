# 리팩토링 동등성 증명과 빠른 빌드·설치

사용자 지시(2026-11-16): 「다음 수정사항에서 우리코드 대규모 리팩토링 가자 (검사는 리팩토링 효율화 후에도 현재 우리 모드 기능을 유지하는걸 반드시 증명할 수 있어야해..) 그리고 앞으로는 검사 설치도 빨리빨리 할 수있도록 해줘」

## 1. 기능 지문 (`tools/feature_fingerprint.py`)

리팩토링 전에 기준 지문을 저장하고, 리팩토링 후 같은 방법으로 뜬 지문과 기계로 비교한다. 다르면 통과가 아니다.

| 항목 | 내용 | 비교 규칙 |
|---|---|---|
| dlls | 진단판·배포판 DLL 6개씩: 내보내는 함수, 데이터 구역 문자열(설정 섹션·키, 로그 이름, 메시지, 셰이더·자원 이름; 빌드 경로 제외), `.text` 해시 | 함수가 사라지면 실패. 문자열은 아래 "원본 문자열 규칙"(코드가 바뀐 비교) 또는 문자열 하나하나(`--same-code`)로 본다. 추가된 문자열은 목록으로 보고. `--same-code`면 코드 바이트도 같아야 함 |
| hooks | `build/development/hook_map.json` 606곳: 종류·주소·길이·소유자·기능·DLL·원래 함수·플래그·가드(소스 줄 위치 제외) | 추가·삭제·변경 모두 실패 |
| settings | `development/installer_map.json` 행(섹션·키·기본값·모듈·은퇴) | 사라지거나 바뀌면 실패 |
| features | `development/features.json` 모듈 → 검사 경로 | 모듈이나 검사가 빠지면 실패 |
| routes | 모든 검사 단계의 마지막 출력 줄(상태·검사 수·해시; 시간 값 제외) | 단계가 빠지거나 실패하면 실패. 검사 수는 늘 수 있고 줄면 실패. 다른 값이 바뀌면 실패 |

- 기준: `development/fingerprint/baseline-local199.json`. v1571-plus-local199(사용자 실게임 확인판) 소스로 만들었다. 내용은 DLL 12개, 문자열 19,910개, 훅 606곳, 설정 148개, 기능 66개, 검사 119단계다. 첫 기준 실행 때 `module-manifests`의 낡은 명세 하나를 고쳤다(MOD_DEV_LESSONS G13).
  - 첫 기준은 여러 실행의 결과 줄을 모은 것이었다. 커밋 548733a(DLL 소스는 local199와 같음, 도구만 바뀜)에서 58경로를 한 번에 다시 돌려(654초, 전부 통과) 새로 떴고, 첫 기준과 `--same-code`로 `fingerprint_equal`이 나와 교체했다. 결과 줄의 커밋 기록은 548733a 하나다.
  - 이때 `name-lookup`의 `speedup`(속도 비율)이 실행마다 달라 시간 값 목록에 넣었다. 비교 때도 시간 값을 다시 걸러, 나중에 추가한 시간 키가 저장된 지문에도 적용된다.
- 실패를 잡는지 확인(2026-11-16): 소스 복사본에서 아래 넷을 바꾸고 다시 빌드한 뒤 비교했다. 네 가지 모두 실패로 나왔다.
  - `ccz_draw_probe` 설정 키 이름 변경
  - `ccz_probe_ctx` 로그 이름 변경
  - `SAN14CczGetApi` 내보내기 제거
  - `layout_registry.h`의 speed_buttons 훅 행 삭제
  - 이때 빌드마다 바뀌는 PDB 서명(`RSDS…`)이 잡음으로 섞이는 것을 발견해 제외했다.
- 뜨기: `python -B -X utf8 tools/run_checks.py --routes <전체> --keep-going --results build/development/<이름>.jsonl` 후 `python -B -X utf8 tools/feature_fingerprint.py capture --name <이름> --results build/development/<이름>.jsonl`.
- 비교: `python -B -X utf8 tools/feature_fingerprint.py compare development/fingerprint/baseline-local199.json development/fingerprint/<이름>.json`.
- 의도한 변경(예: 로그 이름 정리)은 `--allow 파일`(`{"allow": ["string:<문자열>", "route:<경로/단계>", ...]}`)에 이유와 함께 적어야만 통과한다.
- 자체 검사: `feature_fingerprint.py selftest`가 함수 삭제, 설정 키 이름 변경, 로그 이름 삭제, DLL 누락, 훅 행 삭제·소유자 변경, 기본값 변경, 기능 검사 누락, 검사 수 감소, 상태 변경, 단계 실패, 텍스트 결과 변경 12가지를 모두 실패로 잡는지 확인한다.
- **원본 문자열 규칙(2026-11-16, 번역 단위 분리 때 추가).** 문자열 목록은 데이터 구역의 "이어진 인쇄 가능 바이트"라서, 링커가 앞에 놓은 실수 상수·`0001..99` 숫자표·한 글자가 함께 붙는다. 코드가 바뀌면 이웃이 움직여 `'# %.1f'`가 `'?# %.1f'`로, `'W:9ranges'`가 `'W:ranges'`로 바뀌고, 실수 상수뿐인 조각(`'8@HPXb'`)은 생기거나 사라진다. C 문자열은 NUL에서 끝나므로 붙는 이웃은 앞쪽뿐이다.
  - 그래서 `--same-code`가 아닌 비교는 두 지문의 결과 커밋 소스(`git archive <커밋> src`, `.cpp .c .h .hpp .inl .inc .hlsl` 등)에서 모든 문자열 리터럴(주석·문자 리터럴·숫자 구분자 `1'000`·접두어 u8/u/L·원시 문자열 `R"x(...)x"`·이웃 리터럴 이어붙이기·X-매크로 `#name` 포함)을 뽑고, 추출기와 같은 문자 범위로 자른 조각마다 "그 조각으로 끝나는 문자열이 몇 개인가"를 센다. 기준 DLL에 있던 조각이 새 DLL에 없거나 개수가 줄면 실패다.
  - 어떤 리터럴로도 설명되지 않는 문자열(툴체인 RTTI·가져오기 이름·CRT·상수)은 강제하지 않고, 사라진 것은 `layout-only string gone`, 새로 생긴 것은 `layout-only string added` 메모로 모두 보고한다(새 가져오기 DLL·함수는 여기서 보인다). DLL마다 설명되는 비율(`literal_coverage`)도 출력한다.
  - 결과 커밋이 하나가 아니거나, 커밋이 아니거나(작업 중 트리), 소스를 못 읽으면 문자열 하나하나 비교로 돌아간다(닫힌 쪽으로 실패).
  - 판독기 검증: 설명되지 않은 14,173개를 단순 `"..."` 정규식과 대조했다. 처음엔 63개가 걸렸고 원인은 `src/season/season_engine_v26.inc`(계절 엔진 로그·한글 소식)를 읽지 않은 것이었다. 확장자를 넓힌 뒤 남은 15개는 DLL 파일 이름(링커 내보내기 이름), CRT의 `HH:mm:ss`·`ntdll`, 다른 리터럴 경계를 잘못 이은 정규식 오탐(`image`, `success` 등)뿐이다.
  - 자체 검사: `feature_fingerprint.py selftest-strings`(checks-selftest `t5b`). 판독기 사례(주석·U"" 제외, 원시·이어붙이기·u8 이스케이프·한글 와이드·X-매크로), 이웃만 바뀜 통과, 리터럴·와이드 리터럴 삭제·이름 변경·개수 감소 실패, 새로 생긴 설명 안 되는 문자열(가져오기 이름)은 실패가 아닌 목록, 결과 커밋 둘·작업 중 트리·`--same-code`면 문자열 하나하나 비교로 돌아가는지 10가지.
  - 판독기와 대조 정규식은 둘 다 `git archive`를 읽으므로, 컴파일이 실제로 읽은 `src` 파일(`/sourceDependencies` 목록 69개, 202파일)이 모두 커밋에 들어 있는지도 확인했다(빠진 파일 0).
  - 실제 빌드 빨간 검사(2026-11-16): 지형 단위의 `log_event("foam_world_keep")`를 `foam_world_kepX`로 바꿔 빌드·지문을 뜨자 실패가 정확히 하나(`diag/SAN14_Convenience.dll lost string 'foam_world_keep'`) 나왔다. 되돌려 다시 빌드한 DLL 12개는 커밋 빌드 지문과 `--same-code`로 `fingerprint_equal`(빌드 재현 가능).

지문은 모의 검사·정적 표가 보는 범위만 증명한다. 실게임 동작(화면, 긴 진행, 저장·불러오기)은 기존처럼 사용자 실게임 확인으로 따로 본다.

## 2. 빠른 빌드 (`tools/build_split.ps1`)

- 측정(2026-11-16, Ryzen 7 5800H 8코어/16스레드, 바꾸기 전):
  - 진단판 DLL 6개를 차례로 빌드하는 데 42.6초. 그중 SAN14_Convenience 하나가 21.7초다(앞단 6.5초, 최적화 12.7초, 최적화 CPU 시간은 단일 스레드 환산 54초).
  - 배포판 빌드와 로더 모의 검사는 62.7초(검사 `verify_loader` 12.3초 포함).
  - `dll-split` 검사 17경로는 106초이고, 그중 `split-module`이 63초다.
- 바뀐 점:
  - 한 번 실행할 때의 모든 컴파일을 하나의 병렬 풀에서 돌린다. 대상마다 obj·PDB·의존성 폴더를 따로 쓰고, 우선순위는 보통 이하다. 풀 크기는 게임 실행 중 3, 아니면 8이다.
  - `-Both`는 진단판(설치용)과 배포판(+로더 모의 검사)을 한 풀에서 만든다. 로더 모의 검사 4개도 동시에 돌린다.
  - `tools/cl_cache.py`: 컴파일러 인자와 지난 컴파일이 실제로 읽은 모든 파일(시스템 헤더 포함, `/sourceDependencies`)이 같고 출력 해시도 같으면, 그 대상은 건너뛰고 `[cached]`로 표시한다. `-NoCache`는 전부 다시 만든다.
- 증명(2026-11-16): 바꾸기 전 직렬 빌드 DLL 12개와 새 병렬 빌드 DLL 12개를 `--same-code`로 비교했다. 결과 `fingerprint_equal`(코드 바이트·문자열·내보내기 모두 동일).
- 측정(같은 PC, 바꾼 뒤):
  - `build_split -Both -NoCache`(진단판 + 배포판 + 로더 검사를 처음부터): 51.7초, 그중 컴파일 35.9초(전에는 42.6 + 62.7 = 105초).
  - Convenience 하나만 다시 빌드: 34초.
  - 소스가 같을 때: 컴파일 0초, 로더 검사까지 캐시면 1초 안팎.
  - `-RuntimeCosts` 26초, `-Fixture` 152초(호환 모의 검사들은 예전처럼 차례로 돈다). 둘 다 통과했고, 배포판 출력은 건드리지 않았다(뒤이은 `-Both`가 20개 모두 캐시).
- 검사 캐시: `check_feature.ps1 -Feature`도 입력 키가 같은 경로를 건너뛴다(`select_checks.py routes`).
  - `ccz-sprites`: 처음 109초, 다시 돌리면 7.7초.
  - `dll-split -NoCache`(콜드): 109초(바꾸기 전 106초). 한때 139초까지 느려졌던 원인 둘을 고쳤다.
    - 통과한 경로를 기록할 때마다 하위 프로세스가 입력 전체를 다시 계산하며 스케줄러를 붙잡았다 → 실행이 끝난 뒤 한 프로세스에서 기록한다.
    - 경로마다 같은 파일을 다시 읽고 정규식으로 훑었다 → 파일 하나가 내놓는 입력(경로·외부 도장·툴체인·따라갈 파일)을 크기·시각이 같은 동안 한 번만 계산한다. 외부 폴더 도장은 한 계획(`route_inputs.frozen()`) 안에서만 재사용한다.
  - `dll-split`(입력이 같을 때): 5.8초(전에는 매번 106초).
  - 키 계산(`route_inputs.py`): 58경로 138초 → 4.2초(한 계획 안), 계획 밖에서 경로 하나씩 부르면 31초.
  - 두 번 모두 입력 파일·외부 도장·툴체인 목록이 옛 코드와 같은지 58경로 전부 대조했다. 키가 바뀐 20경로는 고친 도구(`route_inputs.py`)를 입력에 포함하는 경로뿐이다.
  - 통과 기록은 실행이 끝난 뒤에 남기므로, 도중에 끊긴 실행은 이미 통과한 경로도 기록하지 않는다(다음에 다시 돈다. 거짓 통과는 아님).
- 번역 단위 분리 뒤(2026-11-16, 커밋 03633e2, 지형 단위 하나):
  - `build_split -Both -NoCache`: 59.1초(컴파일 벽시계 43.7초, 풀 8). 풀 안에서는 주 Convenience 단위 35초, 지형 단위 9.3초였다. 단독으로 재면(`/Bt+ /d2cgsummary`) 주 단위 21.0초(앞단 6.2초, 코드 생성 14.8초 벽시계에 CPU 75.7초 = 이미 약 5스레드, 1만 1,530개 함수에 고르게 퍼짐), 지형 단위 4.0초(앞단 2.4초)다. 35초는 풀 경합이다. 콜드 빌드는 CPU에 묶여 있어 단위를 더 나눠도 빨라지지 않는다(51.7초 → 59.1초, 단위마다 앞단이 더해짐).
  - 고친 뒤 다시 빌드(`-Both`, 진단판 + 배포판): 주 단위 모듈 하나(주석만) 28초(컴파일 26초), 지형 파일 하나 10초(컴파일 7.5초). 나누기 전에는 지형 파일 수정도 주 단위 수정과 같았다. 되돌려 다시 빌드한 DLL은 커밋 빌드와 `--same-code`로 같다.
  - 검사 캐시 구멍(2026-11-16 확인 → 같은 날 고침): 도구·문서만 바꾼 커밋 뒤에도 `dll-split`이 124초 걸렸다. 경로 입력(`tools/route_inputs.py`)이 실행하지 않는 파일까지 따라갔다. 원인과 규칙(자체검사 `verify_select_checks.py` route_inputs 12건, 옛 코드로 돌리면 11건 실패):
    - 주석·docstring에 이름만 나온 스크립트는 입력도 아니고 따라가지도 않는다(`build_native_fixture.ps1`의 "DLLs come only from tools/build_split.ps1"가 빌드 스크립트 전체를 끌어왔다). 주석에 나온 데이터 파일(`os.path.join`으로 여는 json)은 입력으로 남긴다.
    - cl 단계는 `build_split.ps1`을 플래그·툴체인을 읽는 데이터로만 쓴다(그 스크립트의 `-Fixture` 검사들은 따라가지 않음). 데이터 단계 경로는 실행기 `run_checks.py` 자체 내용만 입력이다.
    - powershell을 띄우지 않는 .py가 이름을 적은 .ps1, 인터프리터(`sys.executable` 등)를 띄우지 않는 .py가 이름을 적은 .py는 데이터다(내용만 입력, 참조는 안 따라감). 거꾸로 `os.path.join(A.TOOLS, 'build_addr_fixtures.ps1')`처럼 이름만으로 실행하는 스크립트는 예전엔 놓쳤는데(과수집이 우연히 덮음) 이제 따라간다.
    - 폴더를 통째로 가리키면(`feature_deps.py`의 `'development/'`) 그 안 파일은 내용만 입력이고 실행한 것으로 보지 않는다.
    - 명시 규칙(`DATA_READ`): `run_checks.py`는 `features.json`에서 이 경로 항목만 읽고(이미 키에 있음) `select_checks.py`는 통과 기록에만 쓴다. `gen_module_table.py`는 `CURRENT.md`를 링크로만 적는다(설치마다 `module-registry`가 다시 돌던 원인).
    - 원래 못 보던 입력 두 가지를 더 잡는다: `os.path.join(A.ROOT, 'build', 'ui_native_analysis', ...)`처럼 조각으로 만든 외부 경로(`check_addresses.py`가 읽는 언팩 EXE)와, `split-module`의 `@split_image` 인자가 가리키는 모의 이미지.
    - 결과: 입력 619개이던 경로 대부분이 113~318개, 주소 검사·`patch-next`는 513~529개(`addr_promote.py check`를 실제로 실행하고 `tests/` 폴더를 내용으로 읽음). 빠진 입력·외부 도장은 경로마다 어디서 왔는지 따라가 실제로 읽지 않음을 확인했다. `terrain-render`·`path-count`·`path-fast`·`terrain-trees`·`water-diag`·`combat-rebuild`는 검사 스크립트가 `features.json`을 직접 읽어서 여전히 그 파일 전체를 키로 잡는다(검사 단계를 고치면 다시 돈다). 문서를 고쳐도 다시 도는 `dll-split` 경로는 `manifest`(1.5초, 추적 파일 목록 검사)뿐이다. 새 키로 한 번 전부 돈 뒤(125초) `docs/` 문서만 고치고 `dll-split -Build`: 8.5초(전에는 124초).
  - 전체 58경로: 612초, 전부 통과.
- 전체 58경로(캐시 없이 한 번에): 전에는 516·535·543초, 548733a 재기준 실행은 654초였다. 경로 명령과 스케줄러 동작은 같다(검사 경로는 `build_split.ps1`을 부르지 않는다). 늘어난 몫은 CPU를 많이 쓰는 모의 검사에 몰렸다(543초 실행 대비 `terrain-render` 69→98초, `ccz-sprites` 37→50초). 예전 실행끼리도 `path-fast`가 20~38초로 흔들렸다. 원인은 확인하지 못했다. 다음 속도 항목으로 남긴다.

## 3. 한 번에 설치 (`tools/local_install.ps1`)

`powershell -File tools/local_install.ps1 -Version v1571-plus-localN -Modules ccz [-Gui] -Note "..."` (worktree에서, 게임 종료 상태, 커밋 후)

정합성 경로(features.json `always` + `module-registry`) → 빌드(캐시) → 본 폴더 동기화(생성 파일 되돌림, worktree HEAD로 detach) → 빌드 복사(소스보다 새 시각) → `install_local.py prepare/install` → `generate_handoff.py` → CURRENT.md를 worktree에 커밋 → 본 폴더를 그 커밋으로 detach 순서로 진행한다. 단계별 시간을 출력한다. 기능 검사(`dll-split` 등)는 이 스크립트가 돌리지 않으니 먼저 돌린다(SESSION_RULES 4절).

- local200(2026-11-16, ccz 하나, 빌드 캐시 상태): 22초.

## 4. 리팩토링 순서 (계획)

1. 기준 지문 고정(완료 시 이 문서에 기록) → 도구만 바꾼 변경은 `--same-code`로 증명.
2. 큰 번역 단위의 파일 간 심볼 목록부터 만든다(공유 static, 정적 초기화·훅 설치 순서, `thread_local`, `/we4505`가 걸릴 공유 static 도우미).
   - 2026-11-16 측정: `native_adapter.cpp` 하나가 include로 187파일(`.inl` 111개), 46,097줄을 한 번에 컴파일한다. 큰 것은 `game_addresses_data.inl` 3,682줄, `native_advisor_briefing.inl` 1,959줄, `native_campaign.inl` 1,561줄, `native_terrain_render.inl` 1,410줄, `native_advisor_overlay.inl` 1,210줄, `native_season_bake.inl` 1,107줄, `native_ai_nodes.inl` 1,098줄, `native_sortie.inl` 1,068줄이다.
   - 다른 번역 단위는 작다(`ccz_module.cpp` 7,389줄, `layout_main.cpp` 6,409줄, `modpack_dll.cpp` 4,365줄, 나머지 400줄 이하).
3. 기능 묶음 단위로 번역 단위를 나눈다. 한 단계마다 전체 검사 + 지문 비교(코드 바이트는 바뀌어도 되고 나머지 항목은 같아야 함)를 거친다. 나눈 단위는 병렬·증분 컴파일되어, 기능 하나를 고칠 때 다시 빌드할 범위가 줄어든다.
   - **지형 단위(완료, 커밋 03633e2).** `SAN14_MULTI_TU` 빌드(`build_split`, 분리 모듈 경로)는 `native_adapter.cpp`와 `src/tu_convenience_terrain.cpp`(지형 물보라, 나무, 물 진단, 렌더러 + 계절 굽기, 지형 편집기)를 따로 컴파일해 링크한다. 단일 단위 빌드(모의 검사·훅 지도·스캔)는 구조가 그대로이고, `tu_terrain_api.h`는 MULTI_TU에서만 보인다. 공용 정의는 `native_core.inl`(설정·ModFeatures·로그·GA, 이름공간 static은 `inline`)로 옮겼다.
   - 증명:
     - `tools/unit_equivalence.py`: 4가지 변형에서 단일 단위와 분리 단위의 전처리 결과가 같고, 다른 줄은 인터페이스 헤더로 옮긴 정의 2줄 × 4뿐이다(초기값을 바꾸면 실패하는 빨간 검사 확인). 동적 초기화(`??__E`)되는 변수를 다른 오브젝트가 읽지 않는지, 오브젝트 수(기본 모의 2개, 전투 1개)도 본다.
     - 전체 58경로 통과(612초).
     - 지문: `development/fingerprint/refactor-terrain.json`을 기준과 비교하면, 원본 문자열 규칙으로 실패는 `tactic-priority/s0` 하나다. `check_log_side_effects.py`가 훑은 파일 수가 198 → 201(분리로 생긴 소스 파일 3개, 적중 0)로 바뀐 것이고, `development/fingerprint/allow-refactor-terrain.json`에 이유와 함께 허용했다. 결과 `fingerprint_equal`. 링커 배치만 바뀐 문자열은 사라진 18개(실수 상수, 숫자표+글자, 3글자 와이드 `'W:9평지류'`)와 생긴 17개(실수 상수, 코드 바이트 조각)이고 모두 편의 DLL이다. 전투·조조전 DLL에는 설명 안 되는 문자열 변화가 없다(새 가져오기 없음).
     - 코드 크기: 진단판 전투 DLL `.text`가 634,205 → 647,901바이트(+2.2%), 배포판은 +384바이트다. 기준 커밋을 임시 작업트리에서 빌드해 MAP을 비교하니 늘어난 몫은 전부 `native_adapter.obj` 안이고, `ai_trace::daily`(+3.2KB), `gate_intercept_tick`, `sortie_departure_from`, `prepare_sortie` 등 호출하는 쪽 함수가 커졌다(람다 이름만 바뀐 정렬 함수는 같은 크기로 짝이 맞음). `native_core.inl`로 옮기며 `static`에서 `inline`이 된 도우미(`write_log_event`, `game_day`, `valid`, `table`, `army_force`, `observer_mode`, `manager_lock` 등)를 MSVC가 `inline` 표시 때문에 더 많이 펼친 것이다. 진단 로그 코드가 많은 진단판에서 주로 커졌다. 동작 차이는 아니고, 코드 크기를 원래대로 두려면 지역 static이 없는 도우미만 `static`(단위마다 사본)으로 되돌리는 방법이 있다.
     - 진단판 조조전 DLL은 크기가 같은 채 코드 바이트만 바뀌었다. MAP에서 바뀐 기호 8개는 모두 `ccz_sprites` 정렬 람다의 이름 해시(같은 크기로 짝)다. 조조전이 읽는 소스 중 바뀐 것은 `native_cost_profile.h`(진단판만 포함, 이름공간 static → inline, 링키지만)와 `game_addresses.h`(GA_PROFILE 때만 쓰는 정의 2줄의 static → inline)뿐이다. 배포판 조조전도 `game_addresses.h`를 읽지만 코드 바이트가 같으므로, 진단판 차이는 `native_cost_profile.h`에서 온다. 읽는 값은 바뀌지 않았다. 전투 DLL 진단판도 같은 헤더를 포함한다. (MAP 비교의 기준 빌드는 다른 폴더에서 만들어 `.text`가 기준 지문과 같지 않았다. 다음 MAP 비교는 두 커밋을 같은 경로에서 빌드한다.)
   - 다음 단위 평가(2026-11-16): 주 단위에 남은 모듈 61개(16,952줄)를 둘로 나누는 경우를 결합도 목록(설치 허브는 주 단위에 고정, 공용 코어는 양쪽)으로 따져 봤다. 어느 균형으로 잘라도 단위를 건너는 기호가 136~172개였다. 이름만으로 맞춘 변수 연결(`rows`, `screen` 등, 한정 이름 사용 0건)을 걸러 내면 함수 약 80~115개와 변수·형식 몇 개(병합 사이드카 등)다. 이득은 주 단위 모듈 수정 뒤 재빌드 28초가 약 절반이 되는 것뿐이고(추정, 미측정), 콜드 빌드·검사·설치 시간은 그대로다. 검사(`dll-split` 124초, 전체 612초)가 빌드보다 큰 대기라서, 계속 나눌지 검사 속도로 갈지는 사용자 결정으로 남겼다. 나눈다면 먼저 훅 패치 도우미를 `native_patch_core.inl`로 옮긴다(바이트 그대로 잘라 붙이기, 이름공간 변수와 지역 static이 있는 함수만 `inline`, `addrlib.py` 허용 목록 위치도 함께).
   - **패치 코어(완료).** 상태 없는 훅 패치 도우미(`patch_pointer`·`restore_pointer`·`other_mod_present`·`replace_code16`·`replace_call`·`CodeLink`/`prepare_link`/`prepare_bytes`/`publish_link`/`restore_link`·`CallLink`/`call_*`·`allocate_near`)를 `native_adapter.cpp`에서 `native_patch_core.inl`로 바이트 그대로 옮겼다(`native_core.inl` 끝에서 포함). 모두 `[[maybe_unused]] static`이라 부르는 단위만 사본을 갖는다. 상태가 있는 읽기 준비(`code_unpacked_ready`·`text_exact`·`code_readiness`)와 CRC는 주 단위에 남았다. 증명: 전투·편의 DLL의 배포판·진단판 `.text` 크기가 그대로다. 진단판 전투 DLL은 MAP 기호 2,011개의 크기·순서가 같고, 역어셈블 명령 167,816개가 주소 피연산자만 빼면 같다(바뀐 2,522바이트는 데이터 배치가 옮겨 간 RIP 상대 주소). 편의 DLL은 기호 6,390개의 크기가 같고 배치 순서만 바뀌었다. 기준은 같은 경로에서 다시 빌드한 HEAD다(설치된 local201 진단판과 바이트까지 같음). `dll-split -Build` 통과(110초).
    - **ui 단위(완료, 커밋 9cd343b).** 모듈 표로 설치되는 UI·지도 도우미 22개를 `src/tu_convenience_ui.cpp`로 옮겼다(통합 오버레이·대화상자, 군사 브리핑 오버레이·HUD, 외교 정보, 조조전 다리·편집기·제목·미리보기, 관리 순환, 배속 단추, 레이아웃 대기, 저장 코덱, 상단 체류, 얼굴 유지·측정, 중재, 끌어 배우기, 지역 무장, 피해 예보, 건물 파괴, AI 추적, 모드팩 다리). 손으로 연결하는 공적·등용·인사 노드는 주 단위에 남겼다. 건너는 기호가 가장 적은 경계다.
      - 인터페이스 `tu_ui_api.h`: (1) 두 단위가 함께 컴파일하는 공용 헤더(레이아웃 클라이언트, UI 호출 표, 오버레이 정책, 지형 측정). (3) 주 단위가 부르는 ui 정의 44개(설치·훅·오버레이 그리기 함수와 다음 포인터 변수). 정의에서는 `static`만 뗐다. 기본 인수는 정의에만 두므로 주 단위는 인수를 모두 넘긴다(`ai_trace::install`). `briefing_overlay::publish_hidden()` 한 줄(`publish({},false)`와 같음)을 두어 Model 형식이 단위 밖으로 나가지 않는다. 주 단위 모듈 두 곳(`auto_commands`, `expert_settings`)의 앞선 선언에서도 `static`을 뗐다.
      - 공유 상태는 DLL당 한 벌이다. C++17 `inline`으로 바꾼 것: 레이아웃 클라이언트 상태(`found`·`once`·`handles`·`handle_count`), GA()로 로드 시 초기화되는 `layout_registry.h` 표 5개(`inline const`), `core_services.h` 구독 목록 6개, UI 호출 vtable. 이 클라이언트의 `inline` 함수(`current_mark`, `Transaction`)가 상태를 읽으므로, `static`으로 두면 링커가 고른 사본 하나가 한 단위의 상태만 보는 ODR 위반이 된다. 표를 글자로 찾는 검사 2개는 두 표기를 다 받는다.
      - 감사·검증: `unit_equivalence.py audit`은 한 DLL의 오브젝트 둘이 같은 내부 링키지 기호를 쓰기 가능한 구역(.data/.bss/.tls)에 가지면 실패한다. 익명 이름공간 해시는 정규화하고 `$`·`??_C@` 기호는 뺀다. 기본 실행은 작은 오브젝트 둘로 사본을 잡고 `inline`은 통과시키는 자체검사부터 한다. `build_split.ps1`은 빌드한 비모의 변형마다 이 감사를 돌린다(오브젝트가 같으면 캐시). 빨간 검사로, 레이아웃 클라이언트를 `static`인 채 빌드하면 진단판에서 9건(상태 4 + 표 5)으로 실패한다. 생성기 검증기(`gen_module_table.py`)는 두 단위가 함께 읽는 헤더의 이름공간 `static` 변수와, 지역 static·COST_SCOPE가 있는 `static` 함수를 막는다. 다만 `static const` 동적 초기화 표는 못 보므로 판정은 오브젝트 감사가 한다.
      - 생성기: 단위 인터페이스 include를 첫 모듈의 `#if` 밖에 둔다. 첫 모듈(관리 순환)에 `!SPLIT_MODULE_FIXTURE` 조건이 있어서 모의 변형의 주 단위가 선언을 하나도 못 봤던 것을 고쳤다. 단위 동등성 검사도 함께 고쳤다. 모의 변형은 군사 브리핑·외교 정보를 단위에서 빼므로, 그때는 주 단위가 같은 텍스트로 컴파일하는지 본다.
      - 증명: unit_equivalence가 4변형에서 통과했다(파일 564개 비교, 인터페이스로 옮긴 정의 8줄). 오브젝트 감사는 배포판·진단판·모의 모두 통과했고, 분리 모의 호환 사례도 전부 통과했다. 조조전 DLL은 레이아웃 헤더를 함께 읽는데, 크기가 같고 명령 43,891개의 종류·개수도 같다. 차이는 로드 시 표 초기화 순서가 바뀐 32곳뿐이다. 깨끗한 커밋에서 전체 58경로가 통과했다(585초). 지문 `development/fingerprint/refactor-ui.json`은 기준과 `fingerprint_equal`이다(원본 문자열 규칙, 허용 2건 `allow-refactor-ui.json`: 소스 파일 수 198→204, 선택 검사 자체검사 37→49). 편의 DLL의 레이아웃 전용 문자열 58개가 사라지고 57개가 생겼다. 상수 이웃 바이트, 람다 RTTI 이름 해시, 원래 쓰던 CRT `sin`/`exp` 이름표의 배치가 바뀐 것이고, 새 가져오기는 없다. 텍스트를 읽는 검사 2개(`terrain_render_verify.py` 해시 고정, `verify_advisor_briefing.py`)는 뗀 `static`을 따라가게 고쳤다.
      - 재빌드 시간(`build_split -Both`, 진단판+배포판, 캐시 상태): 주 단위 모듈(공적) 수정 23.8초, ui 모듈(군사 브리핑) 수정 23.5초(둘 다 나누기 전 28초), 지형 모듈 13.8초, 공용 코어(`native_core.inl`) 36.5초(단위 전부 + 전투 DLL). 단위 하나의 컴파일은 12~14초이고, 링크·감사·내보내기 검사가 약 7초를 더한다. 각 단위가 공용 헤더를 따로 컴파일하므로 더 나눠도 이득이 작다. 다음 시간 단축은 검사 쪽이다.
