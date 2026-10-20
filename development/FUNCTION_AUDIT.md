# 현재 소스 조사 범위와 함수 색인

기준: v12.19의 게임 DLL과 v12.20 설명·배포 구성. 현재 설치기44개 항목, split 빌드 정의·연결 등록표·기능 게이트·정책 및 기존 함수 기록을 대조했다.
일반 설명은 [README](../README.md), 원작 연결 함수와 변경 조건은 [기술 설명](../docs/GITHUB_기능상세.md)에 있다.

## 근거와 한계

- `FUNCTION_INDEX.csv`: 파일·행·함수 정의 후보·기능 주제·범위·본문 지문. `source_audit.json`: 모든 src 파일 지문과 정의 후보.
- 정의 후보는 정규식과 괄호 대응으로 수집했다. 조건부 컴파일과 fixture 분기를 포함하며 템플릿·연산자·생성자 등을 완전히 파싱하는 컴파일러 색인이 아니다.
- 핵심 실행 함수와 정책 조건을 수동 대조한 설명과, 위치를 자동 추출한 보조함수 색인을 구분한다. 모든 보조함수의 의미·ABI·도달성을 개별 검증 완료했다는 뜻은 아니다.
- 실제 compile_commands.json이 없어 C++ LSP의 교차 참조 완전성을 주장하지 않는다. 원작 전체 EXE를 새로 역분석하거나 게임을 실행한 검사가 아니다.
- 툴팁·전사 코드는 지문이 고정된 참고 DLL의 COFF 변환물을 정적으로 포함한다. `tools/import_modpack_objects.py`와 `research/modpack_v16/INTEGRATION.md`가 근거이며 모든 내부 기계어 함수를 이번에 재분석한 것은 아니다.

## 수동 대조한 핵심 경로

| 경로 | 확인한 판단·변경 |
|---|---|
| first_army_node / cut_node / city_node / retreat_node | 개별 우선순위·병참·원작 군세 재판정·10일 회군 억제·소규모 임무 유지 |
| sortie_plan* / sortie_dispatch / sortie_departure_from / sortie_movement / sortie_step | 편성·예산·공성8000·지원1000·최저 중립지형 속도·대열과 지역 작업 |
| campaign_proactive_departures / campaign_request_reinforcements / campaign_request_companion | 10일 출진·4000 유보·지역 일꾼·원군·동행 |
| campaign_individual_priority / campaign_forced_siege_priority / campaign_morale_priority | 40일 정지 귀환·실제 성 피해와30일 집단 진군 잠금·사기 회복 |
| district_intercept::judge / restricted_corps / transport_safety | 방침+17(4내정/5지원)·행동별 제한·직할 제외·거점15칸 수송 예외 |
| balanced_development / synchronize_domestic / domestic_command_cost / population_growth | 균형 개발·1군단 옵션·자동 임명 비용·양의 월증가4배 |
| personnel_recruitment_reason / recruit_reason_label / personnel_auto_employ / recruit_ai_* | 관계·충성·금지·가족 예외·사자·명령서·메뉴 알림 |
| injury_* / personnel_healthy_command | 부상 임무·담당관·컴퓨터 시정 본초학 |
| merit_effects / merit_batch / merit_has / merit_list / merit_profile / merit_normalize | 무공3000/3000/0·논공 상호친애·원작 슬롯 추적·열전 스크롤 |
| award_full_synchronize / award_transfer / award_ai_city / award_post_* | 명품 중복·활동 제한·수여·세력단위 배분·관직 한도 |
| native_split_install / native_modpack / native_traits9 | 기능별 게시·모듈 경계·개성/능력·포획·전사·명승지·콜라보 |

## 파일별 탐색표

현재 src 107개 파일, 정의 후보 1075개. 이 수는 검증 통과 수가 아니다.

| 파일 | 정의 후보 | 기능 주제 | 조사 범위 |
|---|---:|---|---|
| src/ai_trace_nodes.h | 0 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/award_policy.h | 5 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/campaign_policy.h | 6 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/combat_v2/hex.h | 5 | 정책·자료구조·저장 형식·공통 보조 함수 | 미연결 전투 재구축 후보 |
| src/combat_v2/legacy_features.cpp | 9 | 정책·자료구조·저장 형식·공통 보조 함수 | 미연결 전투 재구축 후보 |
| src/combat_v2/legacy_features.h | 1 | 정책·자료구조·저장 형식·공통 보조 함수 | 미연결 전투 재구축 후보 |
| src/combat_v2/movement.cpp | 16 | 정책·자료구조·저장 형식·공통 보조 함수 | 미연결 전투 재구축 후보 |
| src/combat_v2/movement.h | 2 | 정책·자료구조·저장 형식·공통 보조 함수 | 미연결 전투 재구축 후보 |
| src/combat_v2/native_contract.h | 0 | 정책·자료구조·저장 형식·공통 보조 함수 | 미연결 전투 재구축 후보 |
| src/combat_v2/sortie.cpp | 7 | 정책·자료구조·저장 형식·공통 보조 함수 | 미연결 전투 재구축 후보 |
| src/combat_v2/sortie.h | 2 | 정책·자료구조·저장 형식·공통 보조 함수 | 미연결 전투 재구축 후보 |
| src/expert_combat_settings.h | 5 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/friendship_save.h | 9 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/friendship_store.h | 16 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/gpu_profile.cpp | 31 | 정책·자료구조·저장 형식·공통 보조 함수 | 진단/조건부 컴파일; 일반 기능과 구분 |
| src/gpu_profile.h | 0 | 정책·자료구조·저장 형식·공통 보조 함수 | 진단/조건부 컴파일; 일반 기능과 구분 |
| src/graphics_proxy.cpp | 2 | 정책·자료구조·저장 형식·공통 보조 함수 | 이전 설계/독립 후보; 최신 split 등록 대상 아님 |
| src/input_proxy.cpp | 2 | 원래 입력 DLL 전달 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/log_append.h | 2 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/merit_bond.h | 20 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/modpack_tables.h | 0 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/module_loader.cpp | 8 | 설정별 DLL 로드·ABI·입력 전달 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/name_lookup.h | 0 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_adapter.cpp | 93 | 설정·원작 자료 접근·전투 우선순위·스레드·연결 공통부 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_adjacency.inl | 9 | 공방·인접 지역 배율 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_ai_trace.inl | 37 | 정책·자료구조·저장 형식·공통 보조 함수 | 진단/조건부 컴파일; 일반 기능과 구분 |
| src/native_awards.inl | 36 | 명품 6슬롯·창고·수여·관직·보조 저장 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_awards_links.inl | 4 | 명품 6슬롯·창고·수여·관직·보조 저장 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_awards_save.inl | 4 | 명품 6슬롯·창고·수여·관직·보조 저장 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_campaign.inl | 24 | 지역 점령·집결·원군 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_campaign_assault.inl | 5 | 지역 점령·집결·원군 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_campaign_departures.inl | 3 | 지역 점령·집결·원군 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_campaign_logistics.inl | 5 | 지역 점령·집결·원군 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_campaign_operations.inl | 21 | 지역 점령·집결·원군 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_campaign_reinforcements.inl | 5 | 지역 점령·집결·원군 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_campaign_routes.inl | 16 | 지역 점령·집결·원군 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_campaign_support.inl | 11 | 지역 점령·집결·원군 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_cost_profile.h | 4 | 정책·자료구조·저장 형식·공통 보조 함수 | 진단/조건부 컴파일; 일반 기능과 구분 |
| src/native_crash_diagnostic.inl | 5 | 정책·자료구조·저장 형식·공통 보조 함수 | 진단/조건부 컴파일; 일반 기능과 구분 |
| src/native_district_intercept.inl | 10 | 플레이어 위임 군단 방침별 출진·수송 출발지 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_district_intercept_link.inl | 5 | 플레이어 위임 군단 방침별 출진·수송 출발지 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_domestic.inl | 9 | 균형 개발·담당관 임명·명령서 비용 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_expert_links.inl | 4 | 숙련 설정 UI·입력·저장 값·계수 연결 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_expert_settings.inl | 16 | 숙련 설정 UI·입력·저장 값·계수 연결 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_facilities.inl | 11 | 시설 건설·파괴·병참 임무 복원 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_friendship.inl | 13 | 정책·자료구조·저장 형식·공통 보조 함수 | 이전 설계/독립 후보; 최신 split 등록 대상 아님 |
| src/native_friendship_save.inl | 11 | 정책·자료구조·저장 형식·공통 보조 함수 | 이전 설계/독립 후보; 최신 split 등록 대상 아님 |
| src/native_injury.inl | 18 | 부상 임무 제한·담당관 해제·본초학 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_injury_links.inl | 4 | 부상 임무 제한·담당관 해제·본초학 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_layout_wait.inl | 5 | 화면자료 유지·선택 진단 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_merit.inl | 17 | 무공 병력·논공 친애·열전·보조 저장 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_merit_links.inl | 5 | 무공 병력·논공 친애·열전·보조 저장 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_merit_save.inl | 10 | 무공 병력·논공 친애·열전·보조 저장 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_merit_scroll.inl | 14 | 무공 병력·논공 친애·열전·보조 저장 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_modpack.inl | 16 | 능력200·성장·툴팁·콜라보·전사·명승지 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_name_lookup.inl | 3 | 이름 탐색 가속 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_obstacles.inl | 8 | 피해 관찰·중립 방벽 자격 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_occupy_fast.inl | 5 | 자동점령 카메라·효과 대기 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_officer_info.inl | 26 | 시험 무장 목록·검색·스냅샷 | 시험 기능; 설치기 기본 꺼짐 |
| src/native_officer_panel.inl | 12 | 시험 무장 목록·검색·스냅샷 | 시험 기능; 설치기 기본 꺼짐 |
| src/native_officer_search.inl | 27 | 시험 무장 목록·검색·스냅샷 | 시험 기능; 설치기 기본 꺼짐 |
| src/native_officer_snapshot.inl | 9 | 시험 무장 목록·검색·스냅샷 | 시험 기능; 설치기 기본 꺼짐 |
| src/native_personnel.inl | 22 | 인물 관계·건강 검사·자동등용 실행 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_personnel_batch.inl | 4 | 인물 관계·건강 검사·자동등용 실행 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_population.inl | 1 | 원작 월간 양의 인구 증가분 4배 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_profile.h | 0 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_progress.inl | 12 | 거리 계산·AI 진입 가속 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_progress_profile.h | 0 | 거리 계산·AI 진입 가속 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_recruit_ai.inl | 5 | 등용 화면·알림·컴퓨터 등용 제한 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_recruit_ui.inl | 23 | 등용 화면·알림·컴퓨터 등용 제한 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_region_officer_traits.inl | 9 | 담당관·관련개성 표시·분산 생성 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_region_officers.inl | 18 | 담당관·관련개성 표시·분산 생성 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_save_codec.inl | 4 | 저장 압축 연결·검증·원작 대체 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_sortie.inl | 50 | 출진 명령·편성·대열·역할·이동속도 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_sortie_links.inl | 4 | 출진 명령·편성·대열·역할·이동속도 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_speed_buttons.inl | 15 | ×4/×8·호버·진행 프레임 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_split_install.inl | 9 | DLL별 연결 소유권·검사·게시·복구 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_tactic_priority.inl | 6 | 시험 플레이어 출진 전법 우선순위 | 시험 기능; 설치기 기본 꺼짐 |
| src/native_target_retreat.inl | 7 | 정책·자료구조·저장 형식·공통 보조 함수 | 이전 설계/독립 후보; 최신 split 등록 대상 아님 |
| src/native_target_retreat_profile.h | 0 | 정책·자료구조·저장 형식·공통 보조 함수 | 이전 설계/독립 후보; 최신 split 등록 대상 아님 |
| src/native_traits9.inl | 6 | 개성9칸·포획 대사 연결 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_ui_common.inl | 1 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/native_unified_actions.inl | 17 | 시험 통합 인사·검색·일괄 작업 | 시험 기능; 설치기 기본 꺼짐 |
| src/native_unified_dialogs.inl | 33 | 시험 통합 인사·검색·일괄 작업 | 시험 기능; 설치기 기본 꺼짐 |
| src/native_unified_overlay.inl | 12 | 시험 통합 인사·검색·일괄 작업 | 시험 기능; 설치기 기본 꺼짐 |
| src/native_unified_personnel.inl | 29 | 시험 통합 인사·검색·일괄 작업 | 시험 기능; 설치기 기본 꺼짐 |
| src/native_unified_posts_trade.inl | 10 | 시험 통합 인사·검색·일괄 작업 | 시험 기능; 설치기 기본 꺼짐 |
| src/native_unified_relations.inl | 6 | 시험 통합 인사·검색·일괄 작업 | 시험 기능; 설치기 기본 꺼짐 |
| src/native_unified_settings.inl | 5 | 시험 통합 인사·검색·일괄 작업 | 시험 기능; 설치기 기본 꺼짐 |
| src/obstacle_policy.h | 2 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/officer_query.h | 22 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/personnel_policy.h | 8 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/progress_distance_kernel.asm | 0 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/progress_distance_kernel.h | 0 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/progress_geometry.h | 2 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/region_officer_profile.h | 0 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/save_codec.h | 8 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/sidecar_cache.h | 4 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/sortie_policy.h | 9 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/split_module_api.h | 0 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/stat_growth.h | 2 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/tactics_policy.c | 3 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/tactics_policy.h | 0 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/unified_labels.h | 0 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/unified_overlay_policy.h | 6 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/unified_personnel.h | 6 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
| src/unified_tactic_settings.h | 7 | 정책·자료구조·저장 형식·공통 보조 함수 | 현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단 |
