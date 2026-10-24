# 기능별 켜기/끄기

개발 설치기 목록은 아래 표와 기능 카탈로그를 따른다. 설치·배포본의 기능 수는 [CURRENT](../development/CURRENT.md)를 따른다. 기준은 `distribution/FeatureSelect.ps1`의 `$taskFeatureCatalog`, DLL 해석은 `src/native_adapter.cpp::load_mod_features`, 로더 해석은 `src/module_loader.cpp`다. 배포 기본값과 현재 사용자 INI 값은 다르다.

게임 시작 때 한 번 읽으므로 변경 후 재시작한다. `AI모드_설치관리자.exe`의 설정만 저장 또는 `optional_tools/AI모드_기능선택.cmd`를 사용한다. 설치기는 `관리.ps1 -NonInteractive`에 적용/복구를 위임한다. 메뉴 밖 설정은 보존한다.

## 전체 목록

| 탭 | 섹션.키 | 기본값 | 묶음 / 필요 기능 | 기능 |
|---|---|---:|---|---|
| 게임성 | stratagem.building_destruction_all | 0 | groups.convenience | 건물파괴 성공 시 선택 지역의 모든 적 군사시설·장애물·함정에 원래 피해량을 각각 적용. 성·관문·아군·동맹·중립 제외. 실패는 원작 처리, 플레이어·컴퓨터 공통. [설명](BUILDING_DESTRUCTION.md) |
| 게임성 | combat.district_intercept_block | 1 | groups.convenience | 지원·내정 군단 출진 제한 (플레이어 2군단~): 지원=수송만, 내정=요격·점령·공격 출진 금지. 컴퓨터·직할·공격 방침·직접 명령·기존 야전부대 제외. 전투 DLL이 있으면 그 1EC890 훅이, 없으면 편의 DLL이 판정 |
| 전투 | groups.combat | 1 | — | 전투 AI 교정 전체 (SAN14_CombatAI.dll) |
| 전투 | combat.sortie_formation | 1 | groups.combat | 군세 출진 편성 (병력별 2~7부대·공성 진형·1,000명 지원) |
| 전투 | combat.departures | 1 | groups.combat | 정기 선제 출진 검토 (10일마다) |
| 전투 | combat.group_advance | 1 | groups.combat | 집결 후 30일 정지 군세의 집단 진군 |
| 전투 | combat.stationary_retreat | 1 | groups.combat | 같은 자리 40일 정지 부대 퇴각 |
| 전투 | combat.morale | 1 | groups.combat | 사기 회복 우선 |
| 전투 | combat.support | 1 | groups.combat | 호위·동행 지원 요청 |
| 전투 | combat.mission_v2 | 1 | groups.combat | 임무 일관성 v2 (공성 시설만·대기 없음·원작 공격 우선) |
| 전투 | combat.ai_max_aggression | 1 | groups.combat | AI 호전성 최대 (궁극과 같은 공격 검토) |
| 전투 | combat.ai_wall_block | 1 | groups.combat | 컴퓨터 세력 방벽 건설 금지 |
| 전투 | combat.combat_cut | 1 | groups.combat | 컴퓨터 군세의 플레이어 병참선 차단 |
| 전투 | combat.hit_response | 1 | groups.combat | 피격 대응 (적 성·관문 피격 시 공성 고정, 피격 부대 보호) |
| 전투 | combat.obstacle | 1 | groups.combat | 중립 장애물 파괴 판단 |
| 전투 | combat.recall_veto | 1 | groups.combat | 갓 출진한 공격 군단의 회군 방지 |
| 편의성 | groups.convenience | 1 | — | 편의·게임성 기능 전체 (SAN14_Convenience.dll) |
| 편의성 | battle_rules.battle_rules | 1 | groups.convenience | 숙련 모드 공방 설정 + 인접지역 효과 |
| 편의성 | personnel.awards | 1 | groups.convenience | 명품창고·6슬롯·명품/관직 수여 |
| 편의성 | personnel.recruit | 1 | groups.convenience | 등용 보조 (등용 화면·자동 등용·결과 안내) |
| 편의성 | domestic.region_officers | 1 | groups.convenience | 월드맵 지역담당관 초상화 |
| 편의성 | domestic.region_officer_traits | 1 | groups.convenience + domestic.region_officers | 지역담당관 이름 옆 내정 관련 개성 표시 |
| 편의성 | extensions.battle_deaths | 1 | groups.convenience | AI 전사 연출·게임 기록 |
| 편의성 | extensions.captive_dialog | 1 | groups.convenience | 포획 대사 (플레이어 관련 포획 장면 표시) |
| 편의성 | officer_ui.loyalty | 1 | groups.convenience | 숨겨진 충성도 숫자 표시 |
| 편의성 | officer_ui.damage_preview | 1 | groups.convenience | 전투력대체 예상 피해량 |
| 편의성 | extensions.tooltip_collab | 1 | groups.convenience | 개성·전법 툴팁 + 콜라보 개성/전법 해금 |
| 편의성 | game_flow.speed_buttons | 1 | groups.convenience | 진행 속도 버튼 >>>>(×4)·>>>>>(×8) |
| 편의성 | game_flow.occupy_fast | 1 | groups.convenience | 거점 자동점령 연출 단축 (원작의 약 1/4) |
| 편의성 | game_flow.caravan_stay | 1 | groups.convenience | 교역 무장 소재 고정 (원래 도시에 남고 각자 도시로 귀환). 근거·원작 위치: src/native_caravan_stay.inl |
| 성능 | performance.fast_menu | 1 | groups.performance | 지도 메뉴 클릭 대기 감소 (메뉴를 닫은 뒤 바로 다음 클릭). 근거·원작 위치: src/native_fast_menu.inl |
| 성능 | performance.appear_batch | 1 | groups.performance | 부대 등장 묶음 처리 (전략중 뒤 0일 대기 단축) (2026-10-25) |
| 성능 | performance.face_keep | 1 | groups.performance | 부대 깃발 지휘관 얼굴 상주 (0일 대기 단축), 최대 256(사용자 결정 2026-10-25, 얼굴당 최대 약 2.5MB) |
| 성능 | performance.face_prefetch | 1 | performance.face_keep | 출진 지휘관 얼굴 미리 요청 (0일 대기 단축) (2026-10-25) |
| 편의성 | officer_ui.stat_exp | 1 | groups.convenience | 능력치 경험치 표시 (오각형 그래프 숫자 뒤 /N%). 근거·원작 위치: src/native_stat_exp.inl |
| 편의성 | officer_ui.dispatch_days | 1 | groups.convenience | 파견 남은 일수 표시 (파견 목록에 귀환까지 N일). 근거·원작 위치: src/native_dispatch_days.inl |
| 편의성 | officer_ui.mediation_plus | 1 | groups.convenience | 중개 확장 (의형제 3인, 일부다처, 이혼). 근거·원작 위치: src/native_mediation_plus.inl |
| 편의성 | officer_ui.drag_learn | 1 | groups.convenience | 전법서 습득 다중 선택 (여러 무장 × 여러 전법서 한 번에). 근거·원작 위치: src/native_drag_learn.inl |
| 편의성 | officer_ui.lord_change | 1 | groups.convenience | 시작 전 군주 변경 (세력 선택 하단 「반란」 버튼). 근거·원작 위치: src/native_lord_change.inl |
| 편의성 | extensions.scenery_skip | 1 | groups.convenience | 명승지 연출 생략 |
| 편의성 | officer_ui.unified | 0 | groups.convenience | 통합 인사·무장 검색·출진 전법 자동 선택 (시험 중) |
| 성능 | groups.performance | 1 | — | 성능 가속 전체 |
| 성능 | performance.distance | 1 | groups.performance | 거리·AI 진입 계산 가속 |
| 성능 | performance.name_lookup | 1 | groups.performance | 원작 이름 탐색 가속 |
| 성능 | performance.layout_keep | 1 | groups.performance | 화면 레이아웃 보관 (턴 전환 로딩 단축) |
| 성능 | game_flow.fast_save | 1 | groups.convenience | 빠른 자동저장 압축 |
| 성능 | progress_native.geometry | 1 | groups.performance | 거리 계산 세부 가속 |
| 게임성 | stratagem.building_destruction_all | 0 | groups.convenience | 건물파괴 지역 전체 피해 |
| 게임성 | combat.district_intercept_block | 1 | groups.convenience | 지원·내정 군단 출진 제한 (플레이어 2군단~) |
| 게임성 | personnel.merit | 1 | groups.convenience | 논공 친애 확장 |
| 게임성 | personnel.awards_ai | 1 | groups.convenience + personnel.awards | 컴퓨터 명품 자동 배분 |
| 게임성 | personnel.recruit_ai | 1 | groups.convenience | 컴퓨터·위임 군단 등용 시도 제한 |
| 게임성 | personnel.injury | 1 | groups.convenience | 부상 무장 보호 |
| 게임성 | personnel.injury_herbal | 1 | groups.convenience | 컴퓨터 세력 본초학 시정 자동 배치 |
| 게임성 | domestic.domestic | 1 | groups.convenience | 자동 내정 — 상업·농업·병영 중 낮은 개발부터 균형 개발 |
| 게임성 | domestic.domestic_direct | 0 | groups.convenience + domestic.domestic | 1군단(군주 직할) 자동 내정 |
| 게임성 | domestic.domestic_free | 1 | groups.convenience + domestic.domestic | 지역담당관 자동 임명 명령서 비용 0 |
| 게임성 | domestic.population | 1 | groups.convenience | 인구 월간 증가 4배 |
| 게임성 | extensions.traits9 | 1 | groups.convenience | 개성 9칸 |
| 게임성 | extensions.stats200 | 1 | groups.convenience | 능력치 상한 200 |
| 게임성 | extensions.stats200_growth | 1 | groups.convenience + extensions.stats200 | 능력치 100 이후 자연 성장 |
| 기타(개발용) | groups.diagnostics | 1 | — | 개발용 기능 전체 |
| 기타(개발용) | diagnostics.enabled | 0 | groups.diagnostics | 진단 로그 기록 (SAN14_*.log) |
| 기타(개발용) | diagnostics.reuse_handle | 1 | groups.diagnostics | 로그 파일 핸들 재사용 |
| 기타(개발용) | diagnostics.ai_trace | 0 | groups.diagnostics | 원작 AI 행동 추적 (CSV) |
| 기타(개발용) | diagnostics.ai_trace_original | 0 | groups.diagnostics | 원작만 추적 (모드 훅 연결 안 함) |
| 기타(개발용) | diagnostics.layout_wait | 0 | groups.diagnostics | 화면 레이아웃 대기 기록 |
| 기타(개발용) | diagnostics.day_stage | 0 | groups.diagnostics | 하루 처리 단계 기록: 하루 처리 단계별 머문 시간·계산 시간(day_stage), 진단판·진행 속도 버튼 필요 (2026-10-25) |
| 기타(개발용) | diagnostics.cost_profile | 0 | groups.diagnostics | 함수 시간 측정 (CSV): function_cost_*.csv(모드 함수), node_cost_*.csv(원작 판단 노드), 부대 깃발 얼굴 지정 1B91E0 호출 지점 7곳(face_1B91E0_*, 2026-10-25), 진단판에서만 동작 |
| 기타(개발용) | diagnostics.gpu_profile | 0 | groups.diagnostics | GPU 시간 기록 |
| 기타(개발용) | progress_native.profile | 0 | groups.diagnostics | AI 진행 함수 시간 기록 |

## 해석과 종속 관계

- 키 우선순위: `[해당 섹션] 키` → 이전 `[features] 키` → 상속값/기본값. `recruit_ai`, `injury_herbal`, `captive_dialog`는 키가 없는 옛 파일에서 각각 recruit/injury/traits9 값을 상속한다. **상속은 필수 종속과 다르다.**
- 필수 종속은 DLL에서 부모와 AND한다: awards_ai→awards, domestic_free/direct→domestic, region_officer_traits→region_officers, stats200_growth→stats200. 설치기는 체크할 때 필요한 묶음/부모도 켠다.
- combat=0이면 전투 DLL을 로드하지 않는다. 세 묶음이 모두 0이면 기능 모듈을 로드하지 않는다. 성능만 켜도 편의 DLL을 로드한다. convenience=0이면 그 DLL은 편의 기능을 게시하지 않고 필요한 조정·성능 경로만 맡는다.
- 내정 틱은 자동 내정·명품 배분·본초학이 공유한다. 내부에서 각각 스위치를 판정한다. 논공/명품 보조 저장 연결은 보존한다.
- 숙련 공방/인접효과와 툴팁/콜라보 해금은 각각 같은 코드 경로라 한 항목이다. 자료 설치와 기능 훅 선택은 별도이며 개성9·속도 레이아웃 자료가 남는 것만으로 기능이 켜지지는 않는다.
- 숨은 `performance.timer_1ms`는 메뉴에 포함하지 않으며 기본 꺼짐이다. 거리/이름 탐색 하위 값도 기능설정.ini에서 읽으며, 키가 없을 때만 이전 progress_native.ini를 참조한다.
- 설치기 없는 INI에서 `officer_ui.unified`가 누락되면 DLL 자체 fallback은 true다. 배포 설치기는 명시적 기본값 0을 기록한다. 위 표는 **설치기 기본값**이다.

## 검사와 확인 범위

`tests/verify_feature_switches.py`, `tests/verify_installer_gui.py`, `tests/installer_gui_roundtrip.ps1`, `tests/verify_loader.py` 및 `check_feature.ps1 -Feature dll-split` 경로가 관련 검사다. 실제 게임의 모든 조합은 확인하지 않았다. 파일/fixture의 과거 통과 수치는 CURRENT와 후보 기록에서 구분한다.

[문서 목차](README.md) · [자동 내정](DOMESTIC.md) · [사용안내](../사용안내.md)

## 기타(개발용) 탭 (2026-10-22)

모든 설정은 `기능설정.ini` 하나에서 읽는다(키가 없을 때만 예전 `diagnostics.ini`/`progress_native.ini`). 묶음 `groups.diagnostics`(기본 1) 아래: `diagnostics.enabled`(진단 로그, 기본 0), `diagnostics.reuse_handle`(1), `diagnostics.ai_trace`(0), `diagnostics.ai_trace_original`(0), `diagnostics.layout_wait`(0), `diagnostics.gpu_profile`(0), `progress_native.profile`(0). 성능 탭에 `progress_native.geometry`(1). 묶음이 꺼지면 DLL이 아래 값을 모두 끈 것으로 읽는다.

## 함께 쓸 수 없는 기능 (2026-10-22)

`combat.mission_v2`가 켜져 있으면 `combat.departures`(정기 선제 출진), `combat.group_advance`(집결 후 집단 진군), `combat.support`(호위·동행 지원)는 설치기에서 회색으로 막히고 DLL도 꺼진 것으로 읽는다(설치기 `Catalog.ExcludedBy`, DLL `load_mod_features`). 이유: 모드 자체 출진·원작 군세 재조종·1,000명 호위는 "출진은 원작 판단, 출발 후 이동은 원작" 원칙과 3천 이하 원작 퇴각에 충돌한다.
