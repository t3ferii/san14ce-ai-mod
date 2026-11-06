# 기능별 코드 지도

세 DLL의 경계는 현재도 유지된다. `dinput8.dll`은 입력 전달·모듈 로드만, `SAN14_CombatAI.dll`은 전투 판단, `SAN14_Convenience.dll`은 나머지 기능을 소유한다. `native_adapter.cpp`의 `.inl` 포함과 `native_split_install.inl`의 조건부 게시를 함께 확인한다. 기계 판독 등록부는 `development/features.json`이며 설치 여부는 CURRENT가 기준이다.

| 기능 | 우선 읽을 코드 (src 기준) | check_feature의 Feature |
|---|---|---|
| 외교 군정 100% 초과·컴퓨터 최종 응답 뒤 추가 대응 1회 (편의 DLL) | native_diplomacy_intel.inl, diplomacy_intel.h, diplomacy_intel_profile.h, tests/diplomacy_intel_fixture.cpp; 근거 docs/DIPLOMACY_INTEL.md | diplomacy-intel |
| 경로 탐색 가속 (183A30 입구·184485 본문, 편의 DLL 전용) | native_path_fast.inl, path_fast.h, tests/path_fast_fixture.cpp, tests/verify_path_fast.py. 모든 경유 구간을 깨끗하게 탐색. 단일 구간 동등, 다중 구간 차이는 사용자 승인. 비교 모드 외에는 노드 전체 지우기 생략, 위임 직전 전체 지우기. 1843E0 입구는 전투 DLL 소유, path_count 설치와 상호 배제 | path-fast |
| 플레이어 지원·내정 군단 출진 제한·수송 출발 보완 (1EC890 판정, CALL C2F73 조건84) | native_district_intercept.inl, native_district_intercept_link.inl, native_split_install.inl, tests/district_intercept_fixture.inl, installer/InstallerGui.cs. 원작 구조: research/ai_trace/원작_수송판단_군단방침_조건84_20261020.md | district-intercept |
| 로더·입력 전달·DLL 경계 | module_loader.cpp, input_proxy.cpp, split_module_api.h, native_split_install.inl | dll-split |
| 원작 주소표·실행 시 해석기·빌드별 값·기능 차단 (패치 내성, 사용자 2026-11-05. `GA(GA_<옛RVA>)`로만 원작 주소를 쓴다. 모든 DLL이 같은 헤더를 포함하고 Layout API(`ga_resolve`/`ga_table`)로도 노출한다) | game_addresses.h, game_addresses_data.inl(생성물), game_addresses_gates.inl, tools/gen_signatures.py·gen_emit.py·addrlib.py(생성), tools/rva_rebase.py·guard_rebase.py(일괄 치환), tools/check_addresses.py, tests/addr_resolver_fixture.cpp, tests/address_guard_fixture.cpp, tests/ga_expand.py; 구현 결과 docs/PATCH_RESILIENCE.md | address-table (dll-split에 포함) |
| 훅 관리자·공용 Present vt[8]/[22]·게임 창 WndProc 소유 (SAN14_Layout.dll, 사용자 예외 2026-11-05. 편의 DLL의 native_unified_overlay.inl은 `LAYOUT_OWNS_PRESENT` 빌드에서 구독자만 맡는다. 로더가 Layout을 먼저 불러 초기화하고 마지막에 게시) | layout_api.h, layout_registry.h, layout_main.cpp, native_layout_core.inl, native_layout_present.inl, native_layout_client.inl, native_layout_ui.inl(P4: 레이아웃 생성·소멸 이벤트·런타임 UI 서비스·진단 프로브, 관찰점 86E338 E8·865560 입구), tests/layout_fixture.cpp, tests/layout_p3_fixture.cpp, tests/layout_p4_fixture.cpp, tests/verify_layout_registry.py, tests/layout_loader_mock.cpp, tests/verify_layout_loader.py; 설계 docs/LAYOUT_DLL_COMPAT_DESIGN.md | layout, dll-split |
| **UI 훅 소유 (P3, 사용자 2026-11-05 Layout 호환 설계 §8)**: 아래 owner의 훅은 `SAN14_Layout.dll` 관리자가 설치·체인·양보·묶음 롤백한다(등록부 `layout_registry.h` 97행, 몸체는 편의 DLL, ccz_sprites만 CCZ DLL). speed_buttons(1473F50·13D6698·898957 E9·1474100, R1 IC ProgressSpeedModule에 양보), lord_change, region_officers(안개 화이트리스트 6칸, R1 RegionOfficer Face≠0일 때만 양보), ccz_title, ccz_sprites(13A03D0, 화이트리스트), layout_wait(E8 3곳, 86E0C0 입구 패치 제거), fast_menu(R1 FastMenu에 양보), stat_exp(31곳), dispatch_days, damage_preview(7FA230 입구 → 호출 지점 7F5781·7FA1FD), expert_links(6566E0 입구 → 5F10F7·5F5B27·65EAAD, E8 2곳, vt 8칸), unified_dialogs, advisor_hud, recruit_ui, merit_save(13E69A0·13E60F8), merit(1413600 화이트리스트·1413488), merit_prisoner(13A6BB0, R1 PrisonerReturn에 양보, 묶음 예외), traits9(스팬 8·바이트 5, R1 Traits9에 양보), collab·scenery(양보만), domestic_selection, mediation_plus(6D7D90 → 70E9C1 블록, E8 7곳). 가져온 코드(trait_hover·stats200·imported_patches)는 `split_layout_yield`가 같은 규칙으로 양보를 먼저 정한다. | layout_registry.h, native_layout_client.inl, native_split_install.inl, 각 기능의 native_*.inl | layout, dll-split |
| 공통 객체·지문·스위치 | native_adapter.cpp, native_profile.h | core |
| 지역내정 지형 편집·격류 물살 (편의 DLL, terrain_edit에 포함: 원작 물살 35개를 옮기고 숨김, 예비 물살 없음) | native_terrain_edit.inl, native_terrain_render.inl, native_terrain_trees.inl, native_terrain_foam.inl, tests/terrain_foam_extract.py, tests/terrain_foam_fixture.cpp, tests/terrain_foam_verify.py, tests/terrain_foam_build.ps1; 근거 research/real_estate/FOAM_POOL.md 7절 | terrain-edit |
| 출진 편성·역할·병력·지원 | native_sortie.inl, native_sortie_links.inl, sortie_policy.h | sortie |
| 군세 이동·공성·정지 퇴각·현장 행동 | native_campaign.inl, native_campaign_routes.inl, native_campaign_operations.inl, native_campaign_assault.inl, campaign_policy.h | ai / campaign-revision |
| 정기 출진·합류·호위·병참·사기 | native_campaign_departures.inl, native_campaign_reinforcements.inl, native_campaign_support.inl, native_campaign_logistics.inl | campaign-revision |
| 시설 건설·장애물·피격 대응 | native_facilities.inl, native_obstacles.inl, obstacle_policy.h | ai |
| 숙련 공방·인접효과·툴팁 | native_adjacency.inl, native_expert_settings.inl, native_expert_links.inl, expert_combat_settings.h | expert-combat |
| 자동 내정·직할 군단·임명 비용·인구 | native_domestic.inl, native_population.inl | domestic |
| 지역담당관·관련개성 표시 | native_region_officers.inl, native_region_officer_traits.inl, region_officer_profile.h | region-officers |
| 조조전 탭(SAN14_CCZ.dll: 병종 그림·막기·회심) | ccz_module.cpp, ccz_module_api.h, native_ccz_bridge.inl(편의 DLL 쪽), native_ccz_sprites.inl, ccz_sprites.hlsl, ccz_sprites_vs.h, ccz_sprites_ps.h, assets/ccz/ccz_units.bin | ccz-sprites, dll-split |
| 등용 화면·자동 실행·결과 안내 | native_recruit_ui.inl, native_personnel_batch.inl, native_personnel.inl, personnel_policy.h | recruitment |
| 컴퓨터·위임 군단 등용 제한 | native_recruit_ai.inl | personnel; 연결 변경은 core |
| 부상 보호·본초학 | native_injury.inl, native_injury_links.inl | injury |
| 명품창고·수여·AI 배분·저장 | native_awards.inl, native_awards_links.inl, native_awards_save.inl, award_policy.h | awards |
| 논공 친애·열전 스크롤·저장 | native_merit.inl, native_merit_save.inl, native_merit_links.inl, native_merit_scroll.inl, merit_bond.h | merit |
| 공통 게임 안내창 | native_ui_common.inl | ui-common |
| 건물파괴 지역 전체 피해 (편의 DLL, 성공한 계략의 동일 피해량 확장) | native_building_destruction.inl, building_destruction_profile.h, tests/building_destruction_fixture.cpp | building-destruction |
| 전투력대체 예상 피해량 (편의 DLL, 대상 선택 UI) | native_damage_preview.inl, damage_preview.h, damage_preview_profile.h | damage-preview |
| 통합 인사·필터·일괄 작업·전법 | native_unified_*.inl, native_officer_*.inl, native_tactic_priority.inl, unified_personnel.h, officer_query.h | unified-personnel |
| 군사의 열흘 보고 (개발 후보) | native_advisor_briefing.inl, advisor_briefing.h, advisor_briefing_profile.h, native_advisor_overlay.inl, advisor_briefing.hlsl | advisor-briefing · [근거](ADVISOR_BRIEFING.md) |
| 개성9·툴팁·능력200·전사·명승지 | native_traits9.inl, native_modpack.inl, stat_growth.h, modpack_tables.h | traits9-captive / modpack-v16 |
| 거리·AI 진입 계산 가속 | native_progress.inl, progress_geometry.h, progress_distance_kernel.* | progress-native |
| 이름 탐색 가속 | native_name_lookup.inl, name_lookup.h | name-lookup |
| 속도 버튼·저장 압축·점령 연출·레이아웃 | native_speed_buttons.inl, native_save_codec.inl, save_codec.h, native_occupy_fast.inl, native_layout_wait.inl, native_face_timing.inl(진단 전용, cost_profile), native_appear_batch.inl, appear_batch.h, native_area_index.inl, area_index.h, native_face_keep.inl, face_keep.h, native_caravan_stay.inl, native_fast_menu.inl, native_stat_exp.inl, native_dispatch_days.inl, native_mediation_plus.inl, native_drag_learn.inl, native_lord_change.inl | dll-split (순수 압축은 tests/save_codec_fixture.cpp도 확인) |
| GUI·기능 선택·종속 관계 | installer/InstallerGui.cs, distribution/FeatureSelect.ps1, tools/build_installer.ps1 | installer-gui |
| guarded 설치·레이아웃 병합 | distribution/Manage.ps1, distribution/SplitManage.ps1 | installer; 설치기 변경 시 verify_share.py 등 추가 |
| 진행 CPU 측정 (외부 읽기 전용) | distribution/ProgressMeasure.ps1, ProgressMetrics.psm1 | progress-study |
| GPU 계측 후보 (일반 배포와 구별) | gpu_profile.cpp, gpu_profile.h | docs/GPU_RENDERING.md의 별도 절차 |
| 독립 전투 재구성 (미연결) | combat_v2/* | combat-rebuild |
| 과거 목표 퇴각 정책 (설치 안 함) | native_target_retreat.inl, native_target_retreat_profile.h | target-retreat (보관 fixture) |

위 표의 installer/distribution/tools/tests 경로는 프로젝트 기준이다. 나머지는 src 기준이다. 각 Feature의 실제 검사 묶음은 `tools/check_feature.ps1 -Feature <이름> -List`로 확인한다. 공통 adapter나 훅 소유권을 바꾸면 core/dll-split을 함께 확인한다.

새 기능은 `native_<feature>.inl`, 순수 판단은 `<feature>_policy.h`, 저장은 `native_<feature>_save.inl` 패턴을 따른다. 파일 분리가 별도 DLL을 뜻하지 않는다. 기존 `native_friendship*.inl` 등 레거시 후보가 남아 있어도 설치 경로라는 뜻은 아니다.

현재 규칙은 [COMBAT_AI](COMBAT_AI.md), [DOMESTIC](DOMESTIC.md), [스위치](FEATURE_SWITCHES.md). 원작 UI 조사는 [UI_ATLAS](UI_ATLAS.md), 함수 ABI는 [NATIVE_FUNCTIONS](NATIVE_FUNCTIONS.md)를 따른다.
