# Codex 작업자 규칙 (2026-10-25, 사용자 지시)

사용자는 Claude Code와만 대화한다. **Claude가 설계·작업 분할·검토·보고**를 맡고, **Codex는 Claude가 준 작업서 범위 안에서 코드 작성·모의 검사·빌드, 허용된 경우 설치**까지 한다. Codex는 Claude Code가 `codex exec`로 실행하는 별도 Codex 세션이다. 이것은 금지된 **Codex Bridge와 다른 경로**이며, Bridge 금지는 그대로 유지한다.

Codex 세션은 Claude 대화·Codex 앱 채팅의 기억을 갖지 않는다. 필요한 사실은 작업서와 아래 문서에서만 얻는다.

## 1. 시작할 때 읽을 것 (이 순서, 이것만)

1. [SESSION_RULES](SESSION_RULES.md) — 세 DLL·설정·로그·빌드·설치 공통 규칙. 이 문서보다 우선한다.
2. 이 문서.
3. 작업서가 지정한 [MODULES](MODULES.md) 행·소스 파일·[NATIVE_FUNCTIONS](NATIVE_FUNCTIONS.md) 기록.

`AGENTS.md`의 도구 선택 규칙도 따른다. 전체 research·backup·legacy·reference_mod를 읽거나 색인하지 않는다.

## 2. 해도 되는 것

- 작업서의 **수정 허용 파일**만 고친다. 다른 파일이 필요하면 고치지 말고 이유와 근거(파일:줄)를 보고하고 멈춘다.
- **다른 패치 호환(v13)**: 새 훅·바이트 패치·vtable 교체를 추가하면 그 원작 자리의 원래 바이트 검사(`<기능>_guards_match()`)를 만들고, `src/native_split_install.inl`의 기능 비활성 목록에 `if(f.<기능> && !<기능>_guards_match())split_disable(f.<기능>,<새 코드>);`를 추가한다. 통합패치 등이 그 자리를 고쳤으면 그 기능만 꺼지고 나머지는 동작해야 한다. `tests/split_module_fixture.cpp`에 일치·불일치 경우를 추가한다. 전체 `.text` CRC 일치를 기능 동작 조건으로 쓰지 않는다.
- 새 동작에는 모의 검사(fixture)를 추가한다. 새 설정 키는 SESSION_RULES 2절의 세 카탈로그를 같은 변경에서 함께 고친다.
- 작업서가 지정한 검사·빌드를 실행한다. 기본 세트(작업 폴더 = 게임 루트):

```powershell
& .\AI_TacticsMod\tools\check_feature.ps1 -Feature <기능> -List      # 실행 계획 확인
& .\AI_TacticsMod\tools\check_feature.ps1 -Feature <기능>             # 기능 검사
& .\AI_TacticsMod\tools\build_split.ps1 -RuntimeDiagnostics            # 로컬 진단 빌드 (build/development에만 씀)
& .\AI_TacticsMod\tools\check_feature.ps1 -Feature dll-split -Build    # 필수, 전부 통과해야 함
& .\AI_TacticsMod\tools\build_installer.ps1                            # 설치기·카탈로그를 고친 경우만
```

- 원작 함수 의사코드가 필요하면 `python -X utf8 AI_TacticsMod/tools/decomp_cache.py get <주소>` → 없으면 [GHIDRA_PERSISTENT](GHIDRA_PERSISTENT.md)의 `ghidra_persistent.ps1 -Action Export`(읽기 전용)만 쓴다. 의사코드는 가설이며 바이트·호출자·ABI를 따로 확인한다.

## 3. 하지 말 것

- 작업서에 **설치 허용(버전·모듈)**이 없으면 `install_local.py prepare/install`, `generate_handoff.py`를 실행하지 않는다. 허용된 경우에도 5절 절차만 따른다. Apply 패키지 직접 수정·dist·공유 ZIP·GitHub 배포는 하지 않는다.
- 게임 실행·종료, 실행 중 게임 메모리 쓰기, 원격 함수 호출, Computer Use, Codex Bridge.
- 원본 EXE·세이브·`backup/`(설치 스크립트가 쓰는 것 제외)·`dist/`·`reference_mod/`·참고 바이너리 수정·실행.
- 새 DLL·EXE·설정 파일·로그 파일·설치 스크립트 생성.
- 다른 세션의 변경 되돌리기, 작업서에 없는 리팩터링·이름 변경·서식 정리, 문서 대량 수정.
- 같은 실패를 같은 방법으로 반복. 검사가 두 번 같은 원인으로 실패하면 멈추고 보고한다.
- 검사를 통과시키려고 기존 검사를 약화·삭제하는 것. 검사 변경이 필요하면 이유를 보고한다.
- 샌드박스 오류를 권한 확대·다른 셸로 우회하는 것. 오류 원문을 보고한다.

## 4. 반환 형식 (마지막 메시지)

```text
상태: 완료 | 일부 완료 | 중단(이유)
변경 파일: 경로 — 한 줄 요약 (각 파일)
동작 변화: 게임에서 무엇이 달라지는지 2~5줄
실행한 검사: 명령 — 통과/실패 수, 종료 코드 (실행 안 한 것은 "미실행")
빌드 산출물: build/development/... 경로와 SHA-256 (빌드한 경우)
범위 밖 필요사항: 고치지 않은 파일·결정이 필요한 점
미검증: 실게임·ABI·바이트 등 확인하지 못한 것
```

설치했다면 반환에 `설치: <버전> / 모듈 / install 결과 / CURRENT 갱신 여부`를 추가한다.

## 5. 설치 (작업서가 허용한 경우만)

1. 게임 본체 `SAN14CE_KO.exe`가 실행 중이 아닌지 확인한다(install_local.py도 같은 기준). 실행 중이면 설치하지 않고 보고한다. 다른 모드 설치기(예: 통합패치 `SAN14CE_Patch101.exe`)는 게임이 아니므로 막지 않는다(사용자 2026-11-04).
2. 작업서의 버전 이름과 모듈만 쓴다(자기 소유 모듈만, 임의 확대 금지).
3. 게임 루트에서 `python -B -X utf8 AI_TacticsMod/tools/install_local.py prepare <버전> --modules <모듈> [--gui] --note "<내용>"` → `... install <버전>` → `python -X utf8 AI_TacticsMod/tools/generate_handoff.py`.
4. 실패하면 재시도하지 말고 스크립트 출력 원문을 보고한다(스크립트가 자동 원복한다). 설치 결과는 **미서명**, 실게임 미확인으로 보고한다.

Codex 실행 환경 메모: Claude는 게임 루트를 작업 폴더로 `codex exec -m gpt-6.1-terra`(전체 위임, 샌드박스 없음)를 띄운다. 모델은 GPT-6.1 Terra로 고정한다(사용자 지시 2026-11-04: Luna에서 교체. Sol은 토큰 사용량이 커서 쓰지 않는다). 작업 범위는 Claude 작업서가 정하므로 Codex는 범위를 넓히지 않는다. 권한 제한이 없으므로 이 문서의 금지 사항과 작업서의 수정 허용 파일이 유일한 경계다. 작업서에 없는 파일 삭제·이동은 하지 말고 보고한다.

모의 검사 통과를 "실게임 확인"으로 쓰지 않는다. 실행하지 않은 검사를 통과로 쓰지 않는다.
