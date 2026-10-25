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
| 성능 | performance.area_index | 1 | groups.performance | 지역 칸 목록 캐시 (AI 건설 검사 가속): 원작 2C9E30과 같은 시설 수 판정, 맵 고정 지역 목록만 캐시 (2026-10-25) |
| 성능 | performance.face_keep | 1 | groups.performance | 부대 깃발 지휘관 얼굴 상주 (0일 대기 단축). 최대 256명(약 600MB 이하), 상한이면 오래 안 쓴 얼굴부터 교체, 메모리가 부족하면(사용률 85% 이상 또는 남은 램 1.5GB 미만) 절반을 해제하고 새로 붙잡지 않음. 적재 대기 중인 얼굴 제외, 메모리 확인 최대 10초 1회(사용자 결정 2026-10-25, 다른 PC 약 10턴 튕김 보고·원인 미확정, 상한 256 유지) |
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
| 기타(개발용) | diagnostics.area_index_verify | 0 | groups.diagnostics | 진단판 매 호출 원작 결과 대조, 불일치 기록 후 원작 결과 반환 (2026-10-25) |
| 기타(개발용) | diagnostics.path_count | 0 | groups.diagnostics | 경로 탐색 호출 횟수 기록: 진단 로그 ON 및 기존 세력 AI 시간 래퍼 필요(cost_profile=1), 원작 경로·결과 변경 없음 (2026-10-25) |
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

경로 탐색 가속(사용자/Claude 2026-10-25): 성능 탭 `performance.path_fast`는 코드 기본 0, 설치기 기본 1이며 `groups.performance`와 AND한다. 편의 DLL은 `183A30` 입구를 교체하고 모든 경유 구간을 깨끗한 세대 번호 저장소에서 탐색한다. 원작 `1843E0`과 전투 래퍼가 구간을 연결한다. 경유지 하나는 원작과 같아야 하며, 다중 경유지는 원작의 두 번째 구간부터 앞 1,210노드(0xBD10바이트)만 지우던 잔존 상태를 제거하므로 결과 차이를 사용자가 승인했다. 내부 goal_count!=1·null 버퍼·clear_bytes!=0xBD10·칸 범위 오류·디버그 표시·재진입·저장소 확보 실패는 원작 위임이다. 끄면 원작이다.

일반 모드 및 표본 비교(N>1)에서는 `184485`의 노드 버퍼 0x1D8A80바이트 지우기만 2바이트 NOP로 건너뛴다. 결과 버퍼 0x14A0바이트 지우기와 전투 DLL 소유 `1843E0` 입구는 유지한다. 새 구현은 원작 노드 슬롯에 결과 체인의 40바이트 레코드만 기록한다. 지우기 생략 후 원작에 위임할 때에는 알려진 슬롯 전체를 먼저 지운다. 임의의 외부 포인터를 슬롯으로 가정하지 않는다. 함수 지문·패치 지점 가드가 다르면 이 기능만 ID23으로 끈다.

기타(개발용)의 `diagnostics.path_fast_verify`는 **0=끔, 1=매번, N=N번에 한 번**(사용자 결정 2026-10-25). 기본 0, `groups.diagnostics` 종속인 진단판 전용이며 N은 기능설정.ini에서 지정한다. GUI·관리 스크립트 저장은 정수 N을 보존한다. 전역 64비트 호출 순번은 원작 위임·재진입도 포함하며, 지원되는 호출 중 순번이 N의 배수인 호출만 비교한다. 따라서 위임 조건에 걸린 N번째 호출은 비교 수에 포함되지 않는다. 음수는 끔으로 처리한다.

- N=1: 지우기 생략 패치를 설치하지 않는다. 기존대로 원작을 원래 버퍼에서 먼저 실행하여 원작 포인터를 그대로 반환한다. 직전 48,400개 상태 바이트가 모두 0이면 정확 비교, 잔존 상태가 있으면 다중 경유 분류를 유지한다.
- N>1: 지우기 생략 패치를 설치한다. 비표본 호출은 일반 가속과 같고, 표본 호출은 슬롯 **전체 0x1D8A80바이트**를 먼저 지운 뒤 원작을 원래 버퍼에서, 새 구현을 모드 저장소에서 실행한다. 모든 경유 구간을 깨끗하게 시작하는 같은 조건이므로 표본은 전부 정확 비교이다. 성공 여부·체인 칸 순서·각 노드 g가 같으면 새 체인을 원래 버퍼에 기록하여 반환하고, 다르면 원작 결과·버퍼를 보존한다.

진단 로그(모두 공용 `log_event`, 숫자는 순서대로 army/value/hex 필드):

- 기존 `path_fast_mismatch`(순번, 원작 길이, 새 길이)는 깨끗한 불일치 하루 최대 64줄. `path_fast_verify_day`는 정확 비교 수·불일치 수·N=1 잔존 상태 차이 수, `path_fast_day`는 새 탐색 수·원작 위임 수·0이다.
- 불일치 상세는 분류와 관계없이 하루 최대 **16건**이다. `path_fast_mm`=순번/출발/목적, `_flags`=flags/ctx+10/ctx+28, `_ctx`=ctx+2C/army 유효 여부/새 탐색 Native.force, `_cost`=원작 머리 g/새 머리 g/(원작길이<<16)|새길이, `_diff`=첫 차이 인덱스/원작칸/새칸. 인덱스는 원작 1843E0의 머리→parent 소비 순서이며 g만 다른 칸도 차이이다. ctx 값은 원작 실행 직전, 유효 여부·force는 새 탐색에서 이미 얻은 값을 사용한다.
- `path_fast_mm_orig`·`path_fast_mm_new`는 각각 최대 64칸의 인덱스/칸/g. 두 첫 차이 칸마다 방향 1~6의 `path_fast_mm_nb`=이웃칸/(원작상태<<24)|(g&FFFFFF)/(새상태<<24)|(g&FFFFFF). 새 저장소의 이전 세대는 논리적 0으로 읽고, 없는 체인·지도 밖 이웃은 BD10, 해당 이웃의 상태·비용은 0이다. 진단 때문에 비용 도우미를 다시 실행하지 않는다. g의 음수도 기존 기록기의 unsigned 비트값으로 남긴다.
- `path_fast_time_day`는 **새 구현만 실행한 호출 수/그 호출들의 합 µs/비교 호출 수**이다. QPC로 탐색 준비·탐색·체인 기록만 재며 위임·비교·로그 기록은 시간 합에서 제외한다. 합친 tick을 µs로 변환하며 UINT32_MAX를 넘으면 해당 값으로 포화한다. 기존 `COST_SCOPE("path_fast")`는 비교와 진단을 포함하고 비용 키 수는 늘리지 않는다.

하루 합계는 다음 관측일의 첫 호출에서 직전 관측일 값으로 출력된다(`day=`는 경계일). 마지막 날은 다음 호출이 없으면 기록되지 않을 수 있다.

`path_count`도 `183A30`을 쓰므로 `path_fast` 설치가 성공하면 `path_count_skipped_path_fast`만 남기고 진단 훅을 설치하지 않는다. 이번 구현에서는 path_fast 내부에 path_count 집계를 연결하지 않았다. path_fast 설치가 실패하거나 꺼져 있으면 기존 path_count 설치 경로를 유지한다. 원작 비교는 느려지며 실게임 동등성·성능 확인은 모의 검사와 별개다.


모든 설정은 `기능설정.ini` 하나에서 읽는다(키가 없을 때만 예전 `diagnostics.ini`/`progress_native.ini`). 묶음 `groups.diagnostics`(기본 1) 아래: `diagnostics.enabled`(진단 로그, 기본 0), `diagnostics.reuse_handle`(1), `diagnostics.ai_trace`(0), `diagnostics.ai_trace_original`(0), `diagnostics.layout_wait`(0), `diagnostics.gpu_profile`(0), `progress_native.profile`(0). 성능 탭에 `progress_native.geometry`(1). 묶음이 꺼지면 DLL이 아래 값을 모두 끈 것으로 읽는다.

## 함께 쓸 수 없는 기능 (2026-10-22)

`combat.mission_v2`가 켜져 있으면 `combat.departures`(정기 선제 출진), `combat.group_advance`(집결 후 집단 진군), `combat.support`(호위·동행 지원)는 설치기에서 회색으로 막히고 DLL도 꺼진 것으로 읽는다(설치기 `Catalog.ExcludedBy`, DLL `load_mod_features`). 이유: 모드 자체 출진·원작 군세 재조종·1,000명 호위는 "출진은 원작 판단, 출발 후 이동은 원작" 원칙과 3천 이하 원작 퇴각에 충돌한다.

경로 탐색 진단(`diagnostics.path_count`, 2026-10-25): 편의 DLL 진단판에서만 설치한다. `groups.diagnostics=1`, 진단 로그 ON, `path_count=1`이 모두 필요하고, 기존 `43E030` 시간 래퍼가 설치되어 있어야 한다(`performance.distance` 및 `progress_native.enabled` ON, `diagnostics.cost_profile=1` 또는 `progress_native.profile=1`). 래퍼가 없으면 `path_count_root_unavailable`을 남기고 설치하지 않는다. TLS 소멸자에서 로그를 쓰면 Windows loader lock 안에서 파일·잠금을 다룰 수 있어, 원작 반환 직후의 명시적 flush를 선택했다. 입력·결과는 해시로만 비교하며 게임 캐시·명령·탐색은 바꾸지 않는다.

- `path_force`: army=호출 수, value=같은 입력 반복 수, hex=반복 호출의 원작 실행 µs 합.
- `path_force_result`: army=같은 입력·같은 결과 수, value=같은 입력·다른 결과 수, hex=전체 원작 실행 µs 합. 각 입력의 첫 결과를 기준으로 비교한다. 결과 키는 경로 칸 번호만 포함하며 `g/f` 비용 값 자체의 일치까지 입증하지 않는다.
- `path_force_overflow`: army=키 4,096개 초과 또는 읽지 못한 관측 수. 기존 키는 계속 비교하고, 저장하지 못한 키의 반복은 세지 못한다. 숫자는 공용 로그의 unsigned 범위에서 포화한다.
- `path_day`: army=AI·UI 전체 호출 수, value=세력 실행 안에서 확인된 반복 수, hex=전체 원작 실행 ms 합. 비AI 호출은 하루 합계에만 넣는다. 날짜 변경을 다음 경로 호출/세력 종료에서 감지하여 직전 관측일 합계를 기록하므로 로그 `day=`는 경계일이다. 다음 관측 없는 마지막 날·호출 없는 날의 기록은 보장하지 않는다.

입력 키는 ctx 앞 0x30바이트·출발 칸·목적 칸 배열·개수·flags의 FNV64다. 목적 개수가 0..64 밖이면 배열을 읽지 않고 개수만 반영한다. 결과는 부모(+0x10)를 따라 칸 번호(+0x18)를 최대 2,000개 읽으며 고리·잘림을 구분한다. 실패 포인터는 결과 키 0이다. ctx 포인터 뒤 객체·세계 상태와 해시 충돌까지 동일성을 보장하는 것은 아니다. 원작 예외는 그대로 전파하고 진단 읽기 실패만 격리한다. 훅 바이트가 다르면 `path_count_guard_rejected`로 이 진단만 건너뛴다.
