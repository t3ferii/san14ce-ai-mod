# 기능별 켜기/끄기

설치기 목록은 묶음 포함 **44개**다. 설치·배포본의 기능 수는 CURRENT를 따른다. 기준은 `distribution/FeatureSelect.ps1`의 `$taskFeatureCatalog`, DLL 해석은 `src/native_adapter.cpp::load_mod_features`, 로더 해석은 `src/module_loader.cpp`다. 배포 기본값과 현재 사용자 INI 값은 다르다.

게임 시작 때 한 번 읽으므로 변경 후 재시작한다. `AI모드_설치관리자.exe`의 설정만 저장 또는 `optional_tools/AI모드_기능선택.cmd`를 사용한다. 설치기는 `관리.ps1 -NonInteractive`에 적용/복구를 위임한다. 메뉴 밖 설정은 보존한다.

## 전체 목록

| 탭 | 섹션.키 | 기본값 | 묶음 / 필요 기능 | 기능 |
|---|---|---:|---|---|
| 게임성 | combat.district_intercept_block | 1 | groups.convenience | 지원·내정 군단 출진 제한 (플레이어 2군단~): 지원=수송만, 내정=요격·점령·공격 출진 금지. 컴퓨터·직할·공격 방침·직접 명령·기존 야전부대 제외. 전투 DLL이 있으면 그 1EC890 훅이, 없으면 편의 DLL이 판정 |
| 전투 | groups.combat | 1 | — | 전투 AI 교정 전체 (SAN14_CombatAI.dll) |
| 전투 | combat.sortie_formation | 1 | groups.combat | 군세 출진 편성 (병력별 2~7부대·공성 진형·1,000명 지원) |
| 전투 | combat.departures | 1 | groups.combat | 정기 선제 출진 검토 (10일마다) |
| 전투 | combat.group_advance | 1 | groups.combat | 집결 후 30일 정지 군세의 집단 진군 |
| 전투 | combat.stationary_retreat | 1 | groups.combat | 같은 자리 40일 정지 부대 퇴각 |
| 전투 | combat.morale | 1 | groups.combat | 사기 회복 우선 |
| 전투 | combat.support | 1 | groups.combat | 호위·동행 지원 요청 |
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
| 편의성 | extensions.tooltip_collab | 1 | groups.convenience | 개성·전법 툴팁 + 콜라보 개성/전법 해금 |
| 편의성 | game_flow.speed_buttons | 1 | groups.convenience | 진행 속도 버튼 >>>>(×4)·>>>>>(×8) |
| 편의성 | game_flow.occupy_fast | 1 | groups.convenience | 거점 자동점령 연출 단축 (원작의 약 1/4) |
| 편의성 | extensions.scenery_skip | 1 | groups.convenience | 명승지 연출 생략 |
| 편의성 | officer_ui.unified | 0 | groups.convenience | 통합 인사·무장 검색·출진 전법 자동 선택 (시험 중) |
| 성능 | groups.performance | 1 | — | 성능 가속 전체 |
| 성능 | performance.distance | 1 | groups.performance | 거리·AI 진입 계산 가속 |
| 성능 | performance.name_lookup | 1 | groups.performance | 원작 이름 탐색 가속 |
| 성능 | performance.layout_keep | 1 | groups.performance | 화면 레이아웃 보관 (턴 전환 로딩 단축) |
| 성능 | game_flow.fast_save | 1 | groups.convenience | 빠른 자동저장 압축 |
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

## 해석과 종속 관계

- 키 우선순위: `[해당 섹션] 키` → 이전 `[features] 키` → 상속값/기본값. `recruit_ai`, `injury_herbal`, `captive_dialog`는 키가 없는 옛 파일에서 각각 recruit/injury/traits9 값을 상속한다. **상속은 필수 종속과 다르다.**
- 필수 종속은 DLL에서 부모와 AND한다: awards_ai→awards, domestic_free/direct→domestic, region_officer_traits→region_officers, stats200_growth→stats200. 설치기는 체크할 때 필요한 묶음/부모도 켠다.
- combat=0이면 전투 DLL을 로드하지 않는다. 세 묶음이 모두 0이면 기능 모듈을 로드하지 않는다. 성능만 켜도 편의 DLL을 로드한다. convenience=0이면 그 DLL은 편의 기능을 게시하지 않고 필요한 조정·성능 경로만 맡는다.
- 내정 틱은 자동 내정·명품 배분·본초학이 공유한다. 내부에서 각각 스위치를 판정한다. 논공/명품 보조 저장 연결은 보존한다.
- 숙련 공방/인접효과와 툴팁/콜라보 해금은 각각 같은 코드 경로라 한 항목이다. 자료 설치와 기능 훅 선택은 별도이며 개성9·속도 레이아웃 자료가 남는 것만으로 기능이 켜지지는 않는다.
- 숨은 `performance.timer_1ms`는 메뉴의 44개에 포함하지 않으며 기본 꺼짐이다. `progress_native.ini`의 거리/이름 탐색 하위 스위치도 함께 적용된다.
- 설치기 없는 INI에서 `officer_ui.unified`가 누락되면 DLL 자체 fallback은 true다. 배포 설치기는 명시적 기본값 0을 기록한다. 위 표는 **설치기 기본값**이다.

## 검사와 확인 범위

`tests/verify_feature_switches.py`, `tests/verify_installer_gui.py`, `tests/installer_gui_roundtrip.ps1`, `tests/verify_loader.py` 및 `check_feature.ps1 -Feature dll-split` 경로가 관련 검사다. 실제 게임의 모든 조합은 확인하지 않았다. 파일/fixture의 과거 통과 수치는 CURRENT와 후보 기록에서 구분한다.

문서 목차 · [자동 내정](DOMESTIC.md) · 사용안내
