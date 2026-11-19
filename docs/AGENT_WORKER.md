# 서브에이전트 작업자 규칙 (2026-11-16, 사용자 지시)

사용자는 Claude Code와만 대화한다. **Claude가 설계·구현·검토·보고**를 맡고, 사전 맥락이 거의 필요 없는 **간단한 코딩·검사 작업만 Sonnet 서브에이전트**가 Claude의 작업서 범위 안에서 한다(사용자 2026-11-16, 별도 지시가 있을 때까지). Codex는 더 이상 쓰지 않는다. 이 문서는 2026-10-25 Codex 작업자 규칙을 이어받았다.

작업자는 Claude 대화의 기억을 갖지 않는다. 필요한 사실은 작업서와 아래 문서에서만 얻는다.

Claude Code에서는 `subagent_type: "san14-worker"`로 부른다(사용자 설정 `~/.claude/agents/san14-worker.md`: Sonnet, 도구 Read·Grep·Glob·Edit·Write·PowerShell, 이 문서의 요약과 4절 반환 형식을 담음, 2026-11-18). 작업서에는 작업 폴더·목표·완료 조건·수정 허용 파일·검사 명령만 쓴다. 금지 명령 일부는 사용자 설정 훅이 실행 전에 막는다(SESSION_RULES 6절).

## 1. 시작할 때 읽을 것 (이 순서, 이것만)

1. [SESSION_RULES](SESSION_RULES.md) — DLL·설정·로그·빌드·설치 공통 규칙. 이 문서보다 우선한다.
2. 이 문서.
3. 작업서가 지정한 [MODULES](MODULES.md) 행·소스 파일. 새 기능 작업이면 [FEATURE_RULES](FEATURE_RULES.md).

`AGENTS.md`의 도구 선택 규칙도 따른다. research·backup·legacy·reference_mod 전체를 읽거나 색인하지 않는다. 셸은 PowerShell(절대경로)을 쓴다.

## 2. 해도 되는 것

- 작업서의 **수정 허용 파일**만 고친다. 다른 파일이 필요하면 고치지 말고 이유와 근거(파일:줄)를 보고하고 멈춘다.
- 고친 파일의 줄바꿈(CRLF/LF)·BOM을 그대로 둔다. `.ps1`은 UTF-8 BOM 유지.
- 새 훅·바이트 패치를 추가하는 작업이면 [FEATURE_RULES](FEATURE_RULES.md) 3절(원래 바이트 가드 + `split_disable`)과 모의 검사 추가를 지킨다.
- 작업서가 이름으로 지정한 검사·빌드를 **전경에서 순서대로** 실행한다. 기본 세트(작업 폴더 = 워크트리):

```powershell
& .\tools\check_feature.ps1 -Feature <기능> -List      # 실행 계획 확인
& .\tools\check_feature.ps1 -Feature <기능>             # 기능 검사
& .\tools\build_split.ps1 -RuntimeDiagnostics            # 로컬 진단 빌드 (build/development에만 씀)
& .\tools\check_feature.ps1 -Feature dll-split -Build    # 설치 전 전체 회귀
python -X utf8 tools\run_checks.py --routes <경로,...>   # 경로 묶음 실행
```

- 검사가 실패하면 원인이 명백한 기대값 갱신만 1회 스스로 고친다. 그 밖의 실패는 멈추고 실패 출력 원문과 함께 보고한다. 이번 작업에서 직접 고친 파일의 컴파일 오류·경고는 고친다.
- 원작 함수가 필요하면 `San14Sim/docs/orig` → `python -X utf8 tools/decomp_cache.py get <주소>` 순으로 본다. 의사코드는 가설이며 바이트·호출자·ABI를 따로 확인한다.

## 3. 하지 말 것

- `git commit`·`git stash`·브랜치 변경, `install_local.py prepare/install`, `generate_handoff.py`, Apply 패키지 직접 수정, dist·공유 ZIP·GitHub 배포. (커밋·설치는 Claude가 diff를 검토한 뒤 한다.)
- 게임 실행·종료, 실행 중 게임 메모리 쓰기, 원격 함수 호출, Computer Use, 게임 EXE에 REA.
- `taskkill /IM`처럼 이름으로 프로세스 일괄 종료. 자기가 띄운 PID만 끝낸다.
- 원본 EXE·세이브·`backup/`·`dist/`·`reference_mod/`·참고 바이너리 수정·실행.
- 새 DLL·EXE·설정 파일·설치 스크립트 생성.
- 다른 세션의 변경 되돌리기, 작업서에 없는 리팩터링·이름 변경·서식 정리, 문서 대량 수정.
- 같은 실패를 같은 방법으로 반복. 같은 원인으로 두 번 실패하면 멈추고 보고한다.
- 검사를 통과시키려고 기존 검사를 약화·삭제하는 것. 검사 변경이 필요하면 이유를 보고한다.
- 백그라운드 대기 프로세스 쌓기. 검사는 전경에서 끝까지 기다린다.

## 4. 반환 형식 (마지막 메시지)

```text
상태: 완료 | 일부 완료 | 중단(이유)
변경 파일: 경로 — 한 줄 요약 (각 파일)
동작 변화: 게임에서 무엇이 달라지는지 2~5줄 (없으면 "없음")
실행한 검사: 명령 — 통과/실패 수, 종료 코드 (실행 안 한 것은 "미실행")
빌드 산출물: build/development/... 경로와 SHA-256 (빌드한 경우)
범위 밖 필요사항: 고치지 않은 파일·결정이 필요한 점
미검증: 실게임·ABI·바이트 등 확인하지 못한 것
```

모의 검사 통과를 "실게임 확인"으로 쓰지 않는다. 실행하지 않은 검사를 통과로 쓰지 않는다.
