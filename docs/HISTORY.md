# 문서·근거 이력 (docs/MODULARIZATION_PLAN.md 4장)

현행 규칙·사용법·활성 기능 문서만 `docs/`에 두고, 끝난 작업의 이력은 이 표로 남긴다(W7이 이력 문서를 정리하며 채운다). 문서 | 기간 | 요약 | 마지막 커밋.

| 문서 | 기간 | 요약 | 마지막 커밋 |
|---|---|---|---|
| 옛 기준판 1fbe6be7 분석 기록(소스 주석의 "Original (REA, unpacked 1fbe6be7…)") | 2026-10 ~ 2026-11-05 | 옛 Steam 빌드(TimeDateStamp 0x6A983C25)를 Steamless로 푼 해제본과 REA 캐리어로 확인한 원작 함수·바이트·ABI 근거. 소스 주석은 `(근거: 1a4a447d 이전 기록, docs/HISTORY.md)`로 바뀌었고 주소는 `development/addr_promote/1fbe6be7_to_1a4a447d/idmap.tsv`(옛 id → 새 id)로 옮겨 따라갈 수 있다. 해제본은 병합 뒤 `backup/`으로 옮긴다. | W1-E 재기준 (`tools/addr_promote.py`) |
| docs/BUILD_25953386_REMAP.md | 2026-11-05~06 | 새 Steam 빌드 1a4a447d 주소 이전 기록(RVA 재기준, 시그니처, 첫 실패 분석) | c132915 |
| docs/BUILD_25953386_COMBAT.md | 2026-11-05~06 | 새 빌드에서 전투 DLL 이식·가드 재확인 기록 | c132915 |
| docs/LOCAL10_PORT_PLAN.md | 2026-11-06 | local10 이식 계획(traits9 등 가드를 GA id로 전환) | c132915 |
| docs/INSTALL_LOCAL2_TRIAGE.md | 2026-11-05~06 | local2~local10 설치 실패 분류와 수정 경과 | c132915 |
| docs/EXPERT_ADJACENCY_UI_ATTEMPTS.md | 2026-10 | 숙련 설정 인접 보너스 UI 시도·한계 | c132915 |
| docs/GHIDRA_UI_CACHE_TEST.md | 2026-10-23 | Ghidra 영구 DB UI 조회 검증 시험 | c132915 |
| docs/BATTLE_DEATHS_PLAN.md | 2026-10 | 전투 전사 표시 계획(구현 완료, 현행 문서는 소스 주석) | c132915 |
| docs/TRAIT_HOVER_PLAN.md | 2026-11-06 | 특성 호버 설계 계획(구현 완료) | c132915 |
| docs/CODE_REVIEW_2026-11-06_external.md | 2026-11-06 | 외부 코드 리뷰 원문(F 항목은 MODULARIZATION_PLAN에 흡수) | dfc3c56 |
| docs/CODE_REVIEW_2026-11-06_triage.md | 2026-11-06 | 외부 리뷰 항목 분류 | 3386d5a |
| docs/CHECK_REDESIGN_PLAN.md | 2026-11-06 | 검사 재설계 계획(요약은 docs/CHECKS.md로 병합) | 9f1c5ad |
| docs/ADVISOR_BRIEFING_NATIVE_PLAN.md | 2026-11-07 | 군사 브리핑 네이티브 창 계획(포기, 오버레이 방식 채택) | 0f50ed0 |
| docs/archive/REA_PRECOMPUTE_RESEARCH.md | 2026-10 | REA 선계산 연구(ghidra_full로 대체) | c132915 |
| docs/archive/FEATURE_REQUEST.md | 2026-10 | 기능 요청 보관본 | c132915 |

| docs/MODULES.md 수동 지도 | ~2026-11-07 | W7에서 features.json 기반 자동 생성으로 교체. 옛 상세 주소·후보 목록은 과거 지도이며 현재 모듈 소유권 근거로 쓰지 않는다. | 2b7ab40 |
| 삭제된 Luna-Sol 작업 흐름 링크 | ~2026-11-07 | 삭제된 작업 흐름을 가리키던 AGENTS/START의 잔여 안내를 제거. 현재 Claude 작업서·CODEX_WORKER 규칙은 유지. | bc3f35c |
| docs/TACTIC_PRIORITY_PLAN.md | ~2026-11-07 | 전법 우선순위 v1 목록/대화창 설계는 퇴역. 현행 동작은 TACTIC_PRIORITY, v2 설계는 TACTIC_PRIORITY_V2_PLAN. | 8042ac3 |
| docs/GHIDRA_PERSISTENT.md | ~2026-11-07 | 옛 빌드 분석 완료·재열기·Verify 안내를 현행 기준판 조회 안내로 교체. | 4376187 |
| tools/prepare_traits9.py | ~2026-11-07 | 참고 모드 Python 실행 기반 코드/화면 생성기 퇴역. 고정 traits9_payloads·기존 traits9 인덱스/청크 검사를 쓰도록 대체. | 4376187 |
| src/traits9_payloads.h | ~2026-11-18 | 에디터 케이브 기계어 덩어리 3개(무장 상세·능력 탭·무장 목록 개성 열)를 굳힌 117KB 생성물 퇴역. local235에 같은 동작의 C++(`native_traits9.inl`)와 자리 표(`traits9_sites.h`)로 대체, 출처(에디터 소스 traits9ui/abil/list) 유지. | c3ba8be |
| src/native_friendship.inl 및 native_friendship_save.inl, friendship_store.h/save.h | ~2026-11-07 | 거절된 500명 친애 후보 전체·설치 브리지·후보 전용 fixture 제거. 논공 공용 자료형은 save_binding.h에 보존. | 3a528a2 |
| docs/SIEGE_STAGE_HANDOFF.md | ~2026-11-10 | 원작 공성 단계(siege_stage) 인계. 기능은 local52에서 삭제됨. 2026-11-16 문서 정리로 내림. | 6398d14 |
| docs/SIEGE_STAGE_NEXT_PROMPT.md | ~2026-11-10 | 원작 공성 단계(siege_stage) 시작 프롬프트. 기능은 local52에서 삭제됨. 2026-11-16 문서 정리로 내림. | 6398d14 |
| development/HANDOFF_U35_LAG.md | ~2026-11-10 | U35 렉 작업 인계(local29 설치로 끝남). 2026-11-16 문서 정리로 내림. | 6f5790f |
| development/V15_1_CCZ_FOLLOWUP.md | ~2026-11-06 | v15.1 후속·최종 줌아웃 렉 조사 기록(2026-11-02). 2026-11-16 문서 정리로 내림. | c132915 |
| development/TERRAIN_ZOOM_CPU.md | ~2026-11-06 | v15.1 후속·최종 줌아웃 렉 조사 기록(2026-11-02). 2026-11-16 문서 정리로 내림. | c132915 |
| development/TERRAIN_ZOOM_FIX_1.md | ~2026-11-06 | v15.1 후속·최종 줌아웃 렉 조사 기록(2026-11-02). 2026-11-16 문서 정리로 내림. | c132915 |
| docs/INSTALLATION_AUDIT.md | ~2026-11-06 | v12.22-rc3 설치·배포 대조. 2026-11-16 문서 정리로 내림. | c132915 |
| docs/INSTALLER_SPEED.md | ~2026-11-06 | v12 설치·원본 복구 지연 수정 기록. 2026-11-16 문서 정리로 내림. | c132915 |
| docs/MOD_COMPATIBILITY.md | ~2026-11-10 | 통합패치 R42g·발탁 1.8 옛 버전 호환 메모(사용자 2026-11-16 「타모드 구버전은 버려」). 2026-11-16 문서 정리로 내림. | 4acb7c6 |
| docs/W1_VERIFIERS.md | ~2026-11-07 | W1 옛 이미지 의존 검사 목록(작업 끝남). 2026-11-16 문서 정리로 내림. | 71454d3 |
| src/native_ccz_sprites.inl.bak_local14_20261101_225352 | ~2026-11-06 | 추적되던 백업 파일(소스 아님). 2026-11-16 문서 정리로 내림. | c132915 |
| docs/CODEX_WORKER.md | 2026-10-25 ~ 2026-11-16 | Codex 작업자 규칙. 사용자 2026-11-16 「더이상 코덱스 사용하지 않으니」로 docs/AGENT_WORKER.md(서브에이전트 작업자 규칙)로 바꿈. | c132915 |

W7의 옛 해제본 보존 위치: `backup/w7_retired_20261107/editor83_plain/`(game.exe·game.exe.unpacked.exe·unpack.log·verification.json). 이동 전후 SHA-256 4개 일치, 기록 `build/development/w7_artifact_archive.json`. 조회 도구의 기본 대상은 현행 기준판이고, 과거 빌드를 명시한 영구 DB 조회만 이 보존 사본을 읽는다.

## 공식 패치 이전(옛 빌드 1fbe6be7) 자료 보관 위치 (2026-11-16)

현재 기준판은 Steam 빌드 1a4a447d다. 옛 빌드 자료는 지우지 않고 아래 위치에 그대로 보관한다. 도구가 아직 읽는 것은 옮기지 않는다.

| 자료 | 위치 | 크기 | 보관 이유 |
|---|---|---|---|
| 옛 빌드 원작 전체 내보내기 | `research/ghidra_full/1fbe6be7466a6646` | 약 306 MB, 1,879개 | `tools/ghidra_full.py --ref`(옛 빌드 기준 참고 모드 상수), `tools/ghidra_index.py`(두 빌드 vtable 대응), `tools/ghidra_persistent.ps1`가 읽는다 |
| 옛 빌드 의사코드 캐시 | `research/decomp_cache/1fbe6be7466a6646` | 약 5.9 MB, 1,321개 | 옛 근거 대조용 |
| 옛 빌드 해제본 EXE | `backup/w7_retired_20261107/editor83_plain/` | — | 위 W7 기록 |
| 주소 이전 기록 | `development/addr_promote/1fbe6be7_to_1a4a447d/` (git) | 약 0.4 MB | `src/game_addresses_data.inl` 생성 근거 |
| 모드 초기 판 조사 | `research/v4` ~ `research/v9` | 약 59 MB | 초기 판 근거, 미래 기능 조사용 |

`research`·`backup`은 git 밖(메인 폴더)이다. 옮기거나 지우려면 위 도구의 옛 빌드 경로를 먼저 정리한다.
