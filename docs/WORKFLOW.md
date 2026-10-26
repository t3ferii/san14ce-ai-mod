# 개발·검사·배포

작업 전에 [SESSION_RULES](SESSION_RULES.md)를 반드시 읽고 그대로 따른다. 이어서 [START](START.md)와 [CURRENT](../development/CURRENT.md) → [MODULES](MODULES.md)의 관련 행 → 필요한 소스·함수 기록만 읽는다. 설치 버전·준비 버전·개발 소스를 구별한다.

## 개발 검사

게임 루트 PowerShell에서 실행한다.

```powershell
& .\AI_TacticsMod\tools\check_feature.ps1 -Feature region-officers -List
& .\AI_TacticsMod\tools\build_split.ps1 -RuntimeDiagnostics
& .\AI_TacticsMod\tools\check_feature.ps1 -Feature dll-split -Build
```

기능별 검사를 추가하고, `dll-split -Build`는 전부 통과해야 한다. 새 동작에는 모의 검사를 추가한다. 설치기를 고쳤으면 `AI_TacticsMod/tools/build_installer.ps1`도 실행한다. 모의 검사·빌드 성공과 실게임 확인은 구별한다.

`-List`는 실행 계획만 출력한다. `-Build` 없이도 선택된 fixture 도구가 검사 실행 파일을 컴파일할 수 있다. `-Build`는 선택 검사 뒤 개발 DLL 빌드를 추가한다. 개발 DLL은 `build/development`에 쓰며 설치본·Apply·ZIP을 바꾸지 않는다. 일부 기존 fixture 결과는 `build` 바로 아래에도 기록된다.

- 공통 adapter 변경은 `core`, 소유권·연결 변경은 `dll-split`을 추가한다.
- 현재 분리 빌드는 `tools/build_split.ps1`: `native_adapter.cpp`를 `SPLIT_BASE` / `SPLIT_COMBAT`으로 각각 컴파일하고, 로더는 `module_loader.cpp` + `input_proxy.cpp`로 빌드한다.
- 공통 정의: `MERIT_CANDIDATE`, `RECRUITMENT_UI_CANDIDATE`, `HIDDEN_LOYALTY`, `INJURY_PROTECTION`, `AWARD_WAREHOUSE`, `SORTIE_FORMATION`. 배포는 `NO_RUNTIME_LOG`, 별도 계측 후보는 추가 플래그를 쓴다. include·SDK 경로는 빌드 스크립트가 기준이다.
- `.inl` 포함 순서와 매크로가 다르므로 심볼/인덱스만으로 DLL 소유권·ABI를 확정하지 않는다. 실제 컴파일 DB 없는 C++ 교차 참조의 누락도 부재 증거가 아니다.
- 기본 v7 `build_native.ps1 -InputLoader` / `package_input.py build`는 최신 기능을 누락하므로 최신 배포에 사용하지 않는다. 과거 전용 승격 도구도 현재 설치 명령으로 사용하지 않는다.

## 로컬 설치

1. 현재 manifest·설치 상태·원본 EXE·세 DLL baseline과 마지막 설치 뒤 소스 변경을 확인한다. 다른 세션의 변경을 보존하고, 공용 파일 수정이 겹치면 사용자에게 순서를 묻는다.
2. 위 빌드·필수 검사·관련 기능 검사를 마친다. 자기 소유 모듈만 설치 대상으로 정한다.
3. 게임을 끈 뒤 공용 `tools/install_local.py`로만 준비·설치한다. 게임 루트에서는 다음처럼 `AI_TacticsMod/` 접두사를 붙인다.

```text
python -B -X utf8 AI_TacticsMod/tools/install_local.py prepare <버전> --modules <자기 소유 모듈> [--gui] --note "변경 내용"
python -B -X utf8 AI_TacticsMod/tools/install_local.py install <버전>
```

`--modules`는 `combat`, `convenience`, `combat,convenience` 중 자기 소유 범위만 지정한다. 설치기를 바꿨으면 `--gui`를 붙인다. 직접 DLL을 복사하거나 세션 전용 설치 스크립트를 새로 만들지 않는다. 백업·스테이지·manifest·보호된 Apply·설치 후 검증·실패 시 원복은 공용 도구가 담당한다.

4. 설치 후 `python -X utf8 AI_TacticsMod/tools/generate_handoff.py`를 실행한다. 보고에는 **미서명**과 **실게임 확인 여부**를 적는다.

현재 설치와 준비된 버전의 도구·증거는 CURRENT에서 확인한다. 예전 release 스크립트의 baseline을 그대로 다음 버전에 재사용하지 않는다.

## 공유 배포 권한이 있는 작업

공유 ZIP은 `tools/build_share.py --payload <후보 패키지>`로 만들고 `tests/verify_share.py` 등 관련 검사를 수행한다. 같은 이름의 기존 ZIP이 있으면 중단하며, 기존 dist 파일은 보존한다. 공유 패키지를 만들더라도 로컬 설치는 위 공용 도구를 사용한다.

## 문서와 인계

문서만 수정하면 링크·파일 경로·지침·변경 차이를 확인하며 DLL 빌드/설치와 기존 ZIP 재생성을 하지 않는다. 문서 경로를 옮기면 배포 도구의 입력 경로도 확인한다. 역사 보관 ZIP은 과거 기록이며 소스/현재 설정을 대신하지 않는다.

인계 시 `python -X utf8 AI_TacticsMod/tools/generate_handoff.py`로 CURRENT를 갱신한다. 파일 해시 일치·fixture·현재 EXE 검사·실게임 관찰을 구분하며 실행하지 않은 검사를 통과했다고 기록하지 않는다. Computer Use와 Codex Bridge는 금지한다.


과거 기능·세션 전용 설치 스크립트는 기록용이며 현재 설치에 재사용하지 않는다. 공용 설치 규칙을 바꿔야 하면 전투 AI(총괄) 세션에 넘긴다. 보관 도구는 `backup/workspace_history_20261020.zip` 등 해당 기록의 원래 위치를 유지한다.

원자적 설치 쓰기는 내용 해시가 같으면 파일을 교체하지 않는다. 다른 채팅이 설치한 동일 전투 DLL의 파일 자체와 시간을 보존한다. Windows 공유/잠금/교체 제거 오류(32/33/1175)는 같은 임시 파일로 최대2회 짧게 재시도하고, 계속 실패하면 대상 경로를 포함하여 중단·원복한다. `check_feature.ps1 -Feature installer`의 `installer-atomic` 검사는 변경 없는 잠긴 DLL 유지·잠긴 변경 거부·임시 파일 정리·실제 교체를 확인한다.
