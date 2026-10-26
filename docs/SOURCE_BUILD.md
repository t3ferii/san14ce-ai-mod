# 소스 수정·빌드

2026-10-26 작업 폴더에서 추출한 개발 소스다. 정식 v13.3 배포 DLL의 정확한 재현 소스로 보장하지 않는다. 군사의 열흘 보고·숙련 설정 UI 등 실게임 검증이 남은 개발 후보와 이전 설계도 포함되어 있다. 설치·사용은 기존 [릴리스](https://github.com/t3ferii/san14ce-ai-mod/releases)를 따른다.

원하는 기능을 fork해서 수정할 수 있다. 프로젝트가 권리를 가진 독자 코드의 수정·재배포 허용은 [LICENSE](../LICENSE), 외부 구성요소의 예외는 [THIRD_PARTY](../THIRD_PARTY.md)를 따른다. AI 작업자는 [AI_EDITING](AI_EDITING.md)과 루트 [AGENTS](../AGENTS.md)부터 읽는다.

## 외부 자료 없는 첫 검사

Windows x64, Visual Studio의 C++ 데스크톱 개발 도구와 Windows 10/11 SDK가 필요하다. 저장소 루트에서 PowerShell로 실행한다. 이 경로는 컴파일러를 자동 탐색하며 게임 실행·원본 파일 접근·설치를 하지 않는다.

```powershell
& .\tools\build_public_smoke.ps1
# 전투 AI DLL까지 외부 payload 없이 빌드하려면:
& .\tools\build_public_smoke.ps1 -Combat
```

`build/development/public_smoke`에 기존 로더 dinput8.dll과 경로 탐색·무장 검색 fixture를 빌드한다. `-Combat`을 붙이면 같은 폴더에 실제 `SAN14_CombatAI.dll`도 빌드하며 원래 split 전투 빌드와 같은 release 매크로·링크 라이브러리를 쓴다. 이 미서명 산출물은 자동 설치하지 않는다. 편의 DLL의 외부 payload는 이 전투 빌드에 필요하지 않다. 두 DLL 전체 빌드나 ABI·실게임 검증과는 구별한다. `src/combat_v2`는 별도 미연결 후보다.

## 전체 split 빌드

기존 스크립트는 작업 폴더를 `게임 폴더/AI_TacticsMod`로 가정한다. 게임 폴더가 아닌 곳에서 순수 정책을 수정할 수 있으나 전체 준비·검사에는 아래 배치가 필요하다. clone을 기존 모드 폴더에 덮어쓰지 말고 별도 checkout으로 작업한 뒤 필요한 자료를 준비한다.

```text
<game>/SAN14CE_KO.exe                         # 사용자가 보유한 지원 원본
<game>/0002/ScreenLayout.rdb 및 해당 bank      # 사용자의 원본 게임 자료
<game>/reference_mod/Editor_Source/san14ce/   # 별도 확보한 에디터 생성 소스
<game>/AI_TacticsMod/                         # 이 저장소
```

전체 빌드에는 [SOURCE_DEPENDENCIES](SOURCE_DEPENDENCIES.md)의 미포함 파일이 필요하다. 기존 `tools/build_*.ps1` 대부분은 작성자의 MSVC/SDK 경로를 상수로 지정하므로 자신의 설치 경로에 맞춰 `$taskVc`, `$taskSdk`, `$taskVersion` 또는 `$taskSdkVersion`을 먼저 수정한다. 기록된 환경은 MSVC 14.51.36231, Windows SDK 10.0.26100.0, x64, C++17이다. 다른 버전 호환성은 미검증이다. Python 준비 도구는 Python 3.10+ 문법과 capstone을 사용한다.

```powershell
python -m pip install capstone
& .\tools\check_feature.ps1 -Feature dll-split -List
& .\tools\build_split.ps1 -RuntimeDiagnostics
& .\tools\check_feature.ps1 -Feature dll-split -Build
& .\tools\build_installer.ps1
```

진단판은 `build/development/loader_split_diagnostic`, 일반판은 `build/development/loader_split`에 세 DLL을 만든다. 설치기는 `build/development/installer`에 생성된다. 필수 split 검사는 fixture와 일반판 빌드를 포함하며 원작 dump·외부 payload도 필요하다. 준비 도구는 `research/traits9`와 `build`에 파생 자료를 쓴다. `build_native.ps1 -InputLoader`의 과거 통합 경로를 최신 split 배포 대신 쓰지 않는다.

## 설치와 검증 범위

위 명령은 설치하지 않는다. `tools/install_local.py`는 이미 정상 설치된 기존 패키지의 manifest·백업 상태·같은 원본 EXE 지문을 전제로 하며 신규 checkout 설치기가 아니다. 최초 사용자는 릴리스의 설치기를 이용한다. 이후 개발 모듈 설치는 게임이 꺼진 상태에서 [SESSION_RULES](SESSION_RULES.md)와 [WORKFLOW](WORKFLOW.md)의 공용 prepare/install 절차만 사용한다. 과거 기능별 설치 도구는 helper 호환·기록용이며 직접 실행하지 않는다.

공개 스냅샷에는 사용자 설정·실제 세이브·로그·백업·원작 EXE·원시 메모리 dump·타 모드 DLL이 없다. 루트 기능설정.ini는 distribution의 배포 템플릿 복사본이다. 소스·도구가 존재하는 것과 모든 기능이 현재 설치/활성화되거나 실게임 검증되었다는 것은 다르다. 검증 결과는 [SOURCE_VALIDATION](SOURCE_VALIDATION.md)에 별도로 기록한다.
