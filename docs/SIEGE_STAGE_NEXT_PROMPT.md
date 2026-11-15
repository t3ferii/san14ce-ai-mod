# 다음 세션 시작 프롬프트 (siege_stage 구현)

아래를 새 세션에 그대로 붙여 넣는다. 작업 폴더는 `AI_TacticsMod_wt-rules` 워크트리(브랜치 `v1571-plus`)다.

---

삼국지14 AI_TacticsMod 전투 DLL 작업을 이어서 해줘. 한국어로 답해.

먼저 이 순서로 읽어:
1. `docs/SESSION_RULES.md`
2. `AGENTS.md`
3. `docs/SIEGE_STAGE_HANDOFF.md` ← 이번 작업의 인수인계(규칙·확정 사실·구현 계획·검사·설치 절차 전부)
4. `docs/COMBAT_REDESIGN.md` 9절
5. `San14Sim/docs/orig/ai/roles.md` 6-1절

할 일: 인수인계 문서 5절 계획대로 `[combat] siege_native`가 켜졌을 때 원작 성 공략 단계(61F10)를 고쳐 쓰는 `src/native_siege_stage.inl`을 구현해.
- 61F10 노드 칸(`GA_134E028`)을 감싸서, 원작 실행 전에 노드 상태 `+0x1F = 1`로 공성 시설 건설을 예약한다(원작보다 조금 자주).
- 72D30·720E0 뒤에 시설 종류(투석대·요새·군악대·진)와 칸(성에서 3~6칸, siege_ring10이면 3~10칸)을 모드 규칙으로 고친다.
- 72960 뒤에 집결 칸을 성에서 4~6칸으로 고친다.
- 704A0 뒤에 구성원 자리를 나눈다(어린·방원 탱커는 성 쪽, 응원·악주 지원은 2칸 뒤).
- 게시되면 모드 쪽 공성 건설은 비켜선다.

주소 id는 이미 등록돼 있어: `GA_B27D70`, `GA_64C0B`, `GA_64F5E`, `GA_64F39`, `GA_636A0`, `GA_63819`, `GA_64E72`, `GA_134E028`. 순수 판단 초안은 `src/siege_stage_policy.h`에 있어.

지켜야 할 것:
- 컴퓨터 세력만 대상으로 한다(`computer_side_army`). 플레이어 직할과 위임 군단은 반드시 뺀다.
- 끄거나 DLL을 내리면 포인터와 CALL을 원래대로 돌려 원작(또는 먼저 설치된 모드 상태)이 되게 한다. 세이브에 아무것도 남기지 않는다.
- 서브에이전트·다른 세션·DeepSeek·Codex·Computer Use를 쓰지 않는다. 게임 EXE에 REA를 쓰지 않는다(ghidra_full.py와 San14Sim/docs/orig만).
- 새로 확정한 원작 사실은 San14Sim/docs/orig 해당 절에 추가한다.
- 커밋 작성자는 t3ferii <188655834+t3ferii@users.noreply.github.com>, 끝에 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. GitHub에는 올리지 않는다.
- 본 체크아웃 `..\AI_TacticsMod`에서 다른 세션이 커밋하지 않은 문서는 건드리지 않는다(목록은 인수인계 0절).

픽스처, `check_feature.ps1 -Feature campaign-revision`, `-Feature dll-split -Build`, `build_split.ps1 -RuntimeDiagnostics`까지 통과시켜. 그다음 게임이 꺼진 걸 확인하고 인수인계 6절 절차로 local33을 로컬 설치해. 마지막으로 `..\AI_TacticsMod\development\USER_REQUESTS.md` U39 행을 갱신하고 보고해(미서명, 실게임 확인 여부, 시험 방법).

중간에 내 확인이 필요한 결정(예: 망루·함정을 공성지에서도 지을지)이 나오면 그때만 물어보고, 나머지는 끝까지 진행해.
