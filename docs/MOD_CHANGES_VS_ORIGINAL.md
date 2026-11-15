# 원작과 다른 모드 규칙·훅 지점·UI 자료 기준

우리 모드가 원작과 다르게 만든 규칙, 원작 위에 건 훅 지점, 원작 UI 자료를 바꿀 때의 규칙을 모은다. 원작 사실은 `San14Sim/docs/orig`(라이브러리)에 두고, 모드 지침·정책·구현 기록은 여기와 `AI_TacticsMod/docs`의 기능 문서에 둔다(사용자 2026-11-15 "sam14sim은 라이브러리인데 거기에 우리 모드 지침 정리하지마").

2026-11-15에 `San14Sim/docs/orig`의 `rules_people.md` 4.2절과 "모드(원작 아님)" 줄 3개, `build_addresses.md` 8절, `ui_native.md` 6·7절과 충돌 C1~C3을 그대로 옮겼다. 주소 표기는 원문 그대로다(옛 = 1fbe6be7, 새 = 1a4a447d).

## 1. 모드가 바꾼 인사 규칙 (원작 아님)

- 플레이어 자동등용 화면·다중 대상 통합(v8 자동등용_*·등용_다중대상_통합_조사.md): 733BD0 상태 갱신(옛), 선택창 vtable `13AC578`, 6CD040(옛)이 실행 무장 여러 명을 지원(원작은 "대상 1 : 실행 여러 명"). 컴퓨터·군단 AI 등용 후보 제외 `7B360`/`2CC50`(옛, 모드가 연결한 두 노드), 부상 보호(`injury`: 부상 무장 목록 제외·본초학 시정 자동 배치) — 모드 정책이며 원작 점수식은 바꾸지 않는다(v8 컴퓨터군단_등용시도제한.md, injury/부상보호_본초학.md, GITHUB_기능상세.md 3.4).
- **모드(원작 아님)**: 논공 군주 친애(별도 저장·`22FDA0`·`234880`·`2A8D50`에 논공 관계 병합), 친애 500명 확장은 폐기된 설계(v8 논공_군주친애_구현기록.md, 친애500명_확장_구현기록.md). 개성 효과 0x6C(연계참가율)은 공감값에 합을 더함(`2A8D50` 계열).
- **모드(원작 아님)**: 명품창고(미분배 재고, 슬롯별 1개 규칙, 활동 중 수여, 외교 목록 필터), 저장 보조파일 `<native_name>.<hash>.warehouse`, 명품 전달 `1F16B0`(옛 1F0300)의 군주 군단 자원 사용(v8·awards/명품창고_활동중수여.md, research/campaign/경로_병참_교전_인구_열전.md). 슬롯·창고 규칙은 원작 규칙이 아니다.
- **모드(원작 아님)**: 논공 군주 친애 — 무공 1단계 +3000, 2단계 +3000(원작 효과표 500/500을 모드가 바꿈), 3단계는 병사 대신 현 군주와 상호 친애. 이적 `273100`(옛 2717D0) 성공 경로에서 `Person+1C2` u16(1C2·1C3 둘 다)을 0으로 초기화 → "미수여(0) → 다음 논공이 1단계". 관계 저장은 별도(`.merit` 보조파일), 전투·연계 전법 판정에도 병합(v8 논공_*).

## 2. 모드 훅 지점 (정적 판정, 실게임 미확인; 옛→새 빌드)

| 옛 | 새 | 판정 | 용도 |
|---|---|---|---|
| 169F44 | 16AEF4 | changed_aligned/same-asm | battle_rules core[0] → combat_pair_scores |
| 169F8B | 16AF3B | same-asm | CCZ 사이트 CALL → 16FDD0(170D80) |
| 2B07C0 | 2B2A90 | same-hook(첫 5바이트 push r15→r14) | contextual_adjacency |
| 2B07C5 | 2B2A95 | same-hook | +5 이음 지점 |
| 2B0B69 | 2B2E28 | same-hook | 숙련 옵션 호출 E8 → 3EA400(3EC920) |
| 22FCE0 | 231770 | same-hook | 능력 getter(awards, stat_exp, campaign, sortie, ccz_sprites) |
| 22FDAA / 230067 | 23184B / 231ACA | same-hook | 92610(→93650) 호출 뒤 반환 주소 |
| 230098 | 231AFB | 켜짐(local10) | 아이템 0x7B 상한 검사(stats200) |
| 1749A0 | 175950 | same | combat_apply_damage 진입 E9 |
| D2486 | D3516 | same | 컷 판정 E8 |
| 279190 | 27AAC0 | same | build_permission 진입 E9 |
| 68CE0 | 69AB0 | same-hook(동작 전제 변경) | 전투 city_node |
| 2B0290 | 2B2550 | same-hook | 이동 비용 5인자 ABI; path_fast는 호출만 |
| 1F9DE0 / 1FA607 | 1FB170 / 1FB9E6 | same-hook | 건설 완료·최종 피해 지점(building_destruction_all, local10 G2) |
| 322B30 | 324BC0 | same-hook(8번째 int 인자) | 외교 비율(local10 G4) |
| 82C510/82C960/82CD60/82D170 | 82EB90/82F030/82F490/82F8F0 | same-hook(두 번째 int 인자) | 외교 정보 UI 갱신 4함수 |
| 804770 | 806DA0 | same-hook | 외교 AI 보너스 `808930`계 응답 |
| 259682 / 27D976 / 25974B / 27DA09 / 295C5F | 25B022 / 27F716 / 25B0EB / 27F7A9 / 297B6F | same-hook | BattleDeaths COFF 5곳 |
| 1A47C20 | 1A4AC10 | same-asm | 데이터: 44BCA0(→44DEE0) 참조 8곳 |
| 1B95CC0 | 1B98D10 | same-asm | 데이터: 싱글톤 +0x10(ai_max_aggression) |

명령어열 동일 46곳 목록·local10 G2~G5 전 항목은 BUILD_25953386_REMAP.md §5.1. **새 원본 전용 변화**: `27D490`(공격 부대 구성원을 적 요새 공격 action 6/kind 11로 재배정), `2A3E30` — 옛 빌드엔 없음(BC §3 충돌 2).

## 3. UI 자료의 소유·복원 기준 (원본: UI_RESOURCE_LIFECYCLE.md)

원작 UI 자료를 바꾸는 모드의 의무이지만 원작 자료를 다루는 사실도 포함한다:
* UI 변경은 훅뿐 아니라 RDB 색인·bank·아틀라스·그림 표·레이아웃과 설정 키·소유 DLL을 함께 기록한다. DLL 훅 OFF가 디스크 UI 자료를 되돌리지 않는다.
* 원본 참조 보존: `original_entry`는 패치 전 백업으로 검증한다. 조조전 부대 UI 3개 KTID `3DA1641D`, `757F1A41`, `5C046EA3`의 복원 참조는 각각 `8a34e@15ee`(부대 깃발·사기), `8bf87@1030`(부대 초상화·병력), `8d5cc@7d5`(군단 초상화)이다. 배속 4개: `301526D7` 아틀라스, `FEA5F28C` 그림 표, `40F993CE` 팝업, `454A1BCC` HUD. (CCZ_LAYOUT_SWITCH.md:9-18, UI_RESOURCE_LIFECYCLE.md:20-28)
* 기능이 설치되지 않으면(가드 불일치·주소 미해결·DLL 롤백·소유 DLL 부재) 그 기능의 UI도 원작처럼 보이지 않아야 한다(사용자 2026-11-05).
* 설치 후 `ScreenLayout.rdb` 색인 변경은 설치기의 guarded Apply 경로로만 허용한다.

## 4. 모드 UI 문서 경로 표

| 주제 | 경로(AI_TacticsMod/…) |
|---|---|
| 원작 UI에 버튼 추가(정답·진단 순서) | `docs/UI_ADD_GUIDE.md` (먼저 읽기), 사례 `docs/EXPERT_ADJACENCY_UI_ATTEMPTS.md`, `research/lord_change/build_footer.py`, `research/expert_settings/build_preset_layout.py` |
| UI 정적 지도 | `docs/UI_ATLAS.md`, `research/ui_atlas/README.md`·`atlas.py`·`verification.json`·`insertion_recipes.json` |
| UI 리소스 수명·OFF·해제 | `docs/UI_RESOURCE_LIFECYCLE.md`, `docs/CCZ_LAYOUT_SWITCH.md`, `distribution/SplitManage.ps1`(`Get-TraitLayoutPlan`) |
| UI 실패 기록과 해결(옛 빌드 RVA) | `docs/UI_FAILURES_AND_RECIPES.md` |
| 통합 인사·무장 검색(모드) | `docs/UNIFIED_PERSONNEL.md`, `research/unified_personnel/relation_reference/README.md`(출진 무장 선택창의 원작 연계 판정) |
| 외교 군정(모드) | `docs/DIPLOMACY_INTEL.md` |
| 군사 열흘 보고·초상화(모드) | `docs/ADVISOR_BRIEFING.md` |
| 개성·전법·계략 툴팁 | `docs/TRAIT_HOVER_PLAN.md`, 참고 `docs/references/UI.md` |
| 레이아웃 DLL(런타임 UI 생성) | `docs/LAYOUT_DLL_COMPAT_DESIGN.md` |
| 호환·EXE 지문 | `docs/references/COMPATIBILITY.md`, `research/compatibility/배포_EXE지문과참고모드.md` |
| 자동등용 화면 | `research/v8/자동등용_화면_연결.md` |
| 렌더·성능 | `docs/GPU_RENDERING.md`, `research/performance/진행_처리와개선제안.md` |
| 지형 객체(나무·물·거품) | `research/real_estate/` (PLAN, TREES, WATER, FOAM_POOL; 정적 조사, 옛 빌드) |
| Ghidra 화면으로 열기·새 빌드 DB | `docs/GHIDRA_PERSISTENT.md:63` |

## 5. 모드 문서 사이 충돌 (미정리)

| # | 충돌 | 내용 | 처리 |
|---|---|---|---|
| C1 | 진입 버튼 방식 | UNIFIED_PERSONNEL.md:7: 진입 버튼은 게임 D3D11 뒷버퍼에 그린다(무술대회 v30 방식) / UI_ADD_GUIDE.md:55, 98: D3D 오버레이(통합 인사 버튼)는 거절된 방식, 원작 네이티브 버튼으로 | UI_ADD_GUIDE(2026-10-23~25, 사용자 기준) 채택. UNIFIED_PERSONNEL은 2026-10-16 설계가 남은 것으로 추정 — 모드 쪽 정리 필요 |
| C2 | 실행 중 UI 숨김 | UI_RESOURCE_LIFECYCLE.md 기준 8 "실행 중 숨김은 구현하지 않았다: 생성 관찰점에 컨트롤 없음" / LAYOUT_DLL_COMPAT_DESIGN.md: Layout DLL이 런타임에 UI를 만들고 지운다 | 미해결(구현 상태 차이, 원작 구조 사실 아님) |
| C3 | 공통창 구조 | UI_ATLAS 문서의 `45BE60`은 동기 대기가 아님 / 일부 모드 코드가 동기 모달로 가정(UI_FAILURES §2 성공 순서와 불일치한 시도) | UI_FAILURES §2의 성공 순서 채택 |
