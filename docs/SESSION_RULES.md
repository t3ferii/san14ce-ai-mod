# 세션 공통 규칙 (2026-10-22, 사용자 지시)

여러 채팅 세션이 같은 모드를 동시에 고친다. **모든 세션은 작업 전에 이 문서를 읽고 그대로 따른다.** 전투 AI(총괄) 세션이 편의·성능·설치를 총괄한다. 다른 세션은 아래 규칙을 지키고, 규칙을 바꿔야 하면 총괄 세션에 넘긴다.

## 1. 실행 파일은 세 DLL과 설치기 하나뿐

- `dinput8.dll`(로더만), `SAN14_CombatAI.dll`(컴퓨터 AI 판단), `SAN14_Convenience.dll`(그 밖의 모든 기능), `AI모드_설치관리자.exe`(설정·설치). **새 DLL·EXE·상주 도구를 만들지 않는다.**
- 새 기능은 해당 DLL 안의 모듈(`src/native_<기능>.inl`)로 추가하고 `development/features.json`과 `tools/check_feature.ps1` 경로에 등록한다.
- 훅 하나에는 소유 DLL 하나. 같은 원작 주소·vtable 슬롯을 두 DLL이 걸지 않는다. 기존 소유자는 `docs/MODULES.md`, `src/native_split_install.inl`의 `split_pointers`로 확인한다.
- 원작 함수는 원래 UI/AI 스레드에서만 부른다. 외부 프로세스의 메모리 쓰기·원격 호출 금지.

## 2. 설정은 `기능설정.ini` 하나, 관리는 설치기에서

- 새 설정 파일(.ini/.json)을 만들지 않는다. 모든 키는 `AI_TacticsMod\기능설정.ini`에 둔다.
- 새 키를 추가하는 변경에는 **같은 변경 안에서** 설치기 카탈로그 세 곳을 함께 고친다: `installer/InstallerGui.cs`, `distribution/FeatureSelect.ps1`, 게임 폴더용 `FeatureSelect.ps1`(설치 시 distribution에서 복사됨). 설치기가 모르는 키는 설치기 실행 때 중복 섹션으로 밀려 무시된다(2026-10-21 `mission_v2` 사고).
- 탭: 전투(컴퓨터 AI 교정만) / 편의성 / 성능 / 게임성 / 기타(개발용). 진단·추적·시간 기록 같은 개발용 값은 `기타(개발용)` 탭, 묶음 `groups.diagnostics` 아래에 둔다.
- DLL에서 읽는 법: 기능 스위치는 `load_mod_features()`의 `feature_key(config, L"섹션", L"키", 기본값)`. 상위 항목이 있으면 `f.키 = f.상위 && feature_key(...)` 형태(설치기 검사가 이 형태를 확인한다). 개발용·하위 호환 값은 `mod_setting(섹션, 키, 기본값, 예전파일)`.
- 코드 기본값은 안전한 쪽(대개 0)으로, 사용자가 원하는 값은 설치기 기본값과 설치 시 설정 이전으로 맞춘다.

## 3. 로그는 공용 기록기로

- 진단 기록은 `log_event("이벤트명", 숫자, 숫자, 숫자)` 하나로 남긴다(`SAN14_CombatAI.log`/편의 DLL 로그, 줄마다 `day=`). 새 로그 파일·형식·기록기를 만들지 않는다.
- 공용 기록기로 표현할 수 없는 새 로그 파일·CSV·기록 형식이 필요하면 임의로 추가하지 않고 전투 AI(총괄) 세션에 넘긴다. 기존 기록과 참고 자료는 보존한다.
- 배포 빌드(`NO_RUNTIME_LOG`)에서는 기록 코드가 빠진다. 진단 전용 코드와 노출 함수는 `#ifndef NO_RUNTIME_LOG`로 감싼다(배포 노출 목록은 검사로 고정).

## 4. 빌드·검사

- 빌드: `tools/build_split.ps1 -RuntimeDiagnostics`(로컬 진단판). 설치기를 고쳤으면 `tools/build_installer.ps1`(정적 검사+설정 왕복 검사).
- 필수 검사: `tools/check_feature.ps1 -Feature dll-split -Build` 전부 통과. 기능별 경로는 `-Feature <이름> -List`로 고른다.
- 새 동작에는 모의 검사를 추가한다. 기존 검사를 바꿀 때는 사용자 결정(날짜)을 검사 설명에 적는다.
- 모의 검사 통과는 실게임 확인이 아니다. 보고할 때 구분한다.

## 5. 설치는 공용 스크립트 하나로

- 게임을 끈 상태에서만. 직접 DLL을 복사하지 않는다(설치 기록·manifest가 어긋난다).
- `python -B -X utf8 tools/install_local.py prepare <버전> --modules combat|convenience|combat,convenience [--gui] --note "무엇을"` → `... install <버전>`.
  - 지정한 모듈만 새 빌드로 바꾸고 나머지 설치본은 그대로 둔다. 다른 세션의 진행 중 코드가 섞여 나가지 않게 **자기 소유 모듈만** 지정한다.
  - ScreenLayout 화면 자료(`traits9_layout.bin/.json`)를 바꿀 때는 `--layout <폴더>`(기존 기록을 모두 포함한 새 묶음, 2026-10-23 군주교체 버튼). 이때만 설치 후 `ScreenLayout.rdb` 색인 변경을 허용한다.
  - 버전 이름: `v12.21-<주제>-localN`. 백업·스테이지·manifest·보호된 Apply·검증·실패 시 자동 원복을 스크립트가 한다.
- 설치 후 `python -X utf8 tools/generate_handoff.py`로 `development/CURRENT.md` 갱신. 배포는 미서명이며 보고에 "미서명"이라고 적는다.
- 세션 전용 설치 스크립트를 새로 만들지 않는다(기존 `install_damage_preview.py`, `install_building_destruction_local.py`, `install_mission_v2_local.py`는 기록용).

## 6. 동시 작업

- 시작할 때 `development/CURRENT.md`의 설치 버전과, 마지막 설치 이후 바뀐 소스를 확인한다. 다른 세션의 변경을 되돌리지 않는다.
- 공용 파일(`src/native_adapter.cpp`, `src/native_split_install.inl`, 설치기 카탈로그, `distribution/*.ps1`)을 고칠 때는 최소 범위로, 다른 세션이 같은 곳을 고치는 중이면 사용자에게 순서를 묻고 해당 수정은 순서가 정해진 뒤 진행한다. 다른 세션의 변경을 되돌려 충돌을 해결하지 않는다.
- 원본 게임 EXE·사용자 세이브·기존 백업·참고 모드는 건드리지 않는다.

## 7. 원작 함수 분석은 캐시 먼저 (2026-10-23, 사용자 지시)

- REA(Ghidra) 분석은 세션마다 EXE 전체를 다시 불러와 몇 분이 걸린다. 디컴파일·어셈블리가 필요하면 먼저 `python -X utf8 tools/decomp_cache.py get <주소>`를 실행한다. 주소는 `85FCE0`이나 `0x14085FCE0`처럼 적는다. 저장본이 있으면 REA를 부르지 않는다.
- 저장본이 없을 때만 REA를 부르고, 필요한 주소는 한 번의 `batch_decompile`로 묶는다. 작업이 끝나면 `python -X utf8 tools/decomp_cache.py harvest`로 대화 기록의 REA 결과를 `research/decomp_cache/<EXE 해시>/`에 모은다. 같은 결과는 다시 저장하지 않는다.
- 캐시는 분석한 실행 파일의 SHA-256별로 나뉜다. 기준판은 `1fbe6be7…`이다. 의사코드는 가설이다. 훅을 걸 때는 바이트·호출자·ABI를 따로 확인한다.
