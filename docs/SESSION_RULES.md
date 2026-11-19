# 세션 공통 규칙 (2026-10-22 사용자 지시, 2026-11-16 정리)

여러 채팅 세션이 같은 모드를 고친다. **모든 세션은 작업 전에 이 문서를 읽고 그대로 따른다.** 전투 AI(총괄) 세션이 편의·성능·설치를 총괄한다. 규칙을 바꿔야 하면 총괄 세션에 넘긴다. 새 기능의 설계 규칙은 [FEATURE_RULES](FEATURE_RULES.md)에 있다.

## 1. 실행 파일과 공통 동작

- 실행 파일은 DLL 6개와 설치기 하나뿐이다. **이 밖의 새 DLL·EXE·상주 도구를 만들지 않는다.**

  | 파일 | 맡는 일 |
  |---|---|
  | `dinput8.dll` | 로더만 |
  | `SAN14_Layout.dll` | UI 진입·생성·제거 훅과 훅 관리(사용자 2026-11-05 예외, `docs/LAYOUT_DLL_COMPAT_DESIGN.md`) |
  | `SAN14_CombatAI.dll` | 컴퓨터 AI 판단 |
  | `SAN14_Convenience.dll` | 그 밖의 모든 기능. 훅 가드·게시 조정 |
  | `SAN14_CCZ.dll` | 조조전 탭 기능(사용자 2026-11-01). 편의 DLL이 불러오고 훅 게시는 편의 DLL 소유 |
  | `SAN14_ModPack.dll` | 다른 모더 패치 이식(사용자 2026-11-13). 새 모더 패치는 DLL을 더 만들지 말고 여기에 넣는다. 편의 DLL이 불러오고 훅 게시는 편의 DLL 소유 |
  | `AI모드_설치관리자.exe` | 설정·설치·업데이트 |

- 훅 하나에는 소유 DLL 하나. 같은 원작 주소·vtable 슬롯을 두 DLL이 걸지 않는다. 소유자는 `docs/MODULES.md`, `src/native_split_install.inl`에서 확인한다.
- 원작 함수는 게임의 UI/AI 스레드에서만 부른다. 외부 프로세스의 메모리 쓰기·원격 호출 금지.
- 게임 효과는 원작 함수 훅(결과 바꾸기)이나 게임 스레드의 원작 함수 호출로만 만든다. 게임 메모리를 직접 고쳐 효과를 내지 않는다(사용자 2026-11-13).
- 원작 주소는 `src/game_addresses.h` 주소표 id(`GA(...)`)로만 쓴다. 기능 코드에 RVA 숫자 금지(`tools/check_addresses.py`).
- 컴퓨터 규칙은 플레이어 직할 군단과 위임 군단에 적용하지 않는다. 스위치를 끄거나 DLL이 없으면 원작 동작(또는 그 전에 깔린 모드 동작)이고, 기능이 만든 UI도 원래대로 돌아간다.

## 2. 설정은 `기능설정.ini` 하나, 관리는 설치기에서

- 새 설정 파일을 만들지 않는다. 모든 키는 `AI_TacticsMod\기능설정.ini`에 둔다.
- 새 키는 **같은 변경 안에서** `development/installer_map.json`에 행을 넣고 `python -X utf8 tools/gen_installer_catalog.py --apply`로 설치기 카탈로그(`installer/InstallerGui.cs`, `distribution/FeatureSelect.ps1`)를 다시 만든다. 설치기가 모르는 키는 중복 섹션으로 밀려 무시된다(2026-10-21 `mission_v2` 사고).
- 탭: 전투(컴퓨터 AI 교정만) / 편의성 / 성능 / 게임성 / 기타(개발용). 진단·추적·시간 기록은 기타(개발용) 탭, `groups.diagnostics` 아래.
- DLL에서는 `load_mod_features()`의 `f.키 = f.상위 && feature_key(config, L"섹션", L"키", 기본값)`. 코드 기본값은 안전한 쪽(대개 0), 사용자가 원하는 기본값은 설치기에 둔다.

## 3. 로그는 공용 기록기로

- 진단은 `log_event("이벤트명", 숫자, 숫자, 숫자)` 하나로 남긴다(줄마다 `day=`). 새 로그 파일·형식·기록기가 필요하면 만들지 말고 총괄 세션에 넘긴다.
- 진단 전용 코드는 `#ifndef NO_RUNTIME_LOG`로 감싼다. 배포 빌드에서 빠진다.

## 4. 빌드·검사

- 개발 중에는 바뀐 기능의 경로만 돌린다: `tools/check_feature.ps1 -Feature <이름> -List` → 그 경로. 공용 파일을 건드렸으면 그 파일을 쓰는 경로도.
- 설치·배포 직전에 한 번: `tools/check_feature.ps1 -Feature dll-split` + `tools/build_split.ps1 -Both`(진단판 + 배포판 + 로더 모의 검사를 한 병렬 풀에서; 예전 `-RuntimeDiagnostics` + `dll-split -Build`와 같은 범위). 설치기를 고쳤으면 `tools/build_installer.ps1`.
- 캐시(2026-11-16): 입력이 바뀌지 않고 이미 통과한 검사 경로·컴파일·로더 검사는 `[cached]`로 건너뛴다(입력 = 지난 실행이 읽은 파일 전체의 내용). `-NoCache`는 전부 다시 돌린다. 검사 경로의 키는 `build/` 아래 파일과 매크로 include를 보지 않으므로, 공개 배포 직전에는 `check_feature.ps1 -Feature dll-split -NoCache`와 `build_split.ps1 -Both -NoCache`로 돌린다. 리팩토링 동등성은 `docs/REFACTOR_EQUIVALENCE.md`의 기능 지문으로 증명한다.
- 새 동작에는 모의 검사를 추가한다. 검사가 실패하면 검사를 고치거나 모드를 고친다. "원래 실패하던 것"으로 넘기지 않는다.
- 모의 검사 통과는 실게임 확인이 아니다. 보고할 때 구분한다.

## 5. 설치는 공용 스크립트 하나로

- 게임을 끈 상태에서만, 본 폴더(`AI_TacticsMod`)에서 한다. 본 폴더는 `v1571-plus`에 detached 상태이니 설치 전에 작업 트리의 커밋으로 맞춘다(`git checkout -q --detach v1571-plus`). DLL을 직접 복사하지 않는다.
- `python -B -X utf8 tools/install_local.py prepare <버전> --modules <loader,layout,combat,convenience,ccz,modpack 중 자기 소유> [--gui] [--layout <폴더>] --note "무엇을"` → `... install <버전>`.
  - 다른 세션의 진행 중 코드가 섞이지 않게 **자기 소유 모듈만** 지정한다. 나머지 설치본은 그대로 남는다.
  - 설치 기록이 `installed:false`(Steam 갱신 등으로 모드 DLL이 없음)이면 여섯 모듈 모두 지정한다. 우리가 설치하지 않은 파일(version.dll 등)은 건드리지 않는다.
  - 화면 자료(`traits9_layout.bin/.json`)를 바꿀 때만 `--layout`(기존 기록을 모두 포함한 새 묶음)과 `ScreenLayout.rdb` 색인 변경을 허용한다.
  - 버전 이름: 현재 계열 + `-localN`(예: `v1571-plus-local194`).
- 설치 후 본 폴더에서 `python -X utf8 tools/generate_handoff.py`로 `development/CURRENT.md`를 갱신한다. 보고에 "미서명"과 실게임 확인 여부를 적는다.
- 작업 트리에서는 위 순서를 한 번에: `powershell -File tools/local_install.ps1 -Version <버전> -Modules <자기 소유> [-Gui] -Note "무엇을"`(정합성 경로 → 빌드(캐시) → 본 폴더 동기화·빌드 복사 → `install_local.py prepare/install` → handoff → CURRENT.md 커밋 → 본 폴더 detach, 단계별 시간 출력). 검사는 먼저 돌린다.
- 인계·설치 직전 점검은 `powershell -File tools/local_install.ps1 -CheckOnly [-Gui]` 한 번(커밋 확인 → 미추적 소스 경고 → 정합성 경로 → `check_feature.ps1 -Feature dll-split` → `build_split.ps1 -Both`, 설치·본 폴더 변경 없음, 마지막에 인계 초안 한 줄). Claude Code 스킬 `san14-handoff`가 등록 누락 확인부터 인계까지 이 순서를 안내한다(2026-11-18).
- `development/installer_map.json`(설치기 항목·설명)을 바꾼 설치는 반드시 `-Gui`. 없으면 게임 폴더 설치기가 옛 빌드로 남아 새 항목이 안 보인다(MOD_DEV_LESSONS G29). `install_local.py`가 설치기에 없는 키가 있으면 거절한다.
- 세션 전용 설치 스크립트를 만들지 않는다(`tools/local_install.ps1`은 공용 `install_local.py`를 그대로 부르는 공용 순서다).

## 6. 동시 작업

- 시작할 때 `development/CURRENT.md`의 설치 버전과 그 뒤 바뀐 소스를 확인한다. 다른 세션의 변경을 되돌리지 않는다.
- 공용 파일(`src/native_adapter.cpp`, `src/native_split_install.inl`, 설치기 카탈로그, `distribution/*.ps1`)은 최소 범위로 고친다. 다른 세션이 같은 곳을 고치는 중이면 사용자에게 순서를 묻고, 정해진 뒤 진행한다.
- 원본 게임 EXE·사용자 세이브·기존 백업·참고 모드는 건드리지 않는다. Computer Use를 쓰지 않는다. 게임 실행·입력·화면 확인은 사용자가 한다.
- `taskkill /IM`처럼 이름으로 프로세스를 죽이지 않는다. 자기가 띄운 PID만 끝낸다.
- Claude Code 사용자 설정 훅(`~/.claude/settings.json` PreToolUse → `~/.claude/hooks/san14_guard.py`, 자체 시험 `san14_guard_test.py`)이 이름으로 프로세스 종료, `git push`(사용자가 요청했을 때만 명령에 `SAN14_ALLOW_PUSH` 표시), t3ferii + noreply가 아닌 커밋, 참고 자료·원본 EXE·세이브·백업의 수정·삭제·덮어쓰기, 원작 EXE(복사본 포함)에 REA를 실행 전에 막는다(2026-11-18). 막히면 규칙대로 고쳐 다시 실행하고, 훅을 끄거나 우회하지 않는다.

## 7. 원작 근거 찾기

- 순서: [원작 통합 조사본](../../San14Sim/docs/orig/README.md) → `python -X utf8 tools/decomp_cache.py get <주소>` → `tools/ghidra_full.py func|callers|callees|xref|str|vtable <인자>` → asm·바이트가 필요하면 `tools/ghidra_persistent.ps1 -Action Export -Address A,B,C`. 명령·색인 설명은 [GHIDRA_PERSISTENT](GHIDRA_PERSISTENT.md).
- **원작 EXE에는 REA를 쓰지 않는다**(사용자 2026-11-05, 매번 20~30분 전체 재분석). REA는 추출본에 없는 다른 바이너리(참고 모드 DLL 등)에만 쓰고, 주소는 한 번의 `batch_decompile`로 묶는다. 끝나면 `tools/decomp_cache.py harvest`.
- 현재 기준판은 Steam buildid 25953386 `1a4a447d…`. 옛 `1fbe6be7…`은 비교용(`--build 1fbe6be7`)으로만 쓴다. 의사코드는 가설이다. 훅을 걸 때는 바이트·호출자·ABI를 따로 확인한다.
