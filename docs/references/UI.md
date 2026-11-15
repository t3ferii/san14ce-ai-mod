# 게임 화면 확장 참고

2026-10-15 추가: 편집 이벤트 대사 재사용·출전자 선택·D3D11 대진표 overlay는 [무술대회 v30 상세](MARTIAL_TOURNAMENT_V30.md)의 기능별 줄과 제약을 사용한다. 원본의 `version.dll` 프록시 구조를 가져오지 않으며, 현재 주소·ABI는 재검증한다.

자동등용 버튼/열/알림은 우리 `native_recruit_ui.inl`·`native_ui_common.inl`이 우선이다. **Tkinter 에디터 GUI는 게임 화면 UI가 아니다.** 게임 화면 근거는 아래 자료를 읽는다.

| 기능 | 대표 자료 | 확인한 구현 방식 |
|---|---|---|
| 개성5→9칸 | [traits9ui.py](../../../reference_mod/Editor_Source/san14ce/traits9ui.py):1–32, [traitlayout.py](../../../reference_mod/Editor_Source/san14ce/traitlayout.py) | 레이아웃 노드125~128 추가 + 초기화/표시 루프9칸화. 부품 수129 미만이면5칸으로 복귀 |
| 일람 열/탭 | [traits9list.py](../../../reference_mod/Editor_Source/san14ce/traits9list.py):26–29, [화면 조사](../../../reference_mod/Modding_DOCS/memory/TRAITS9_SCREEN.md) | 탭/열 표를 복사·확장하고 게임의 표 조회 경로 연결 |
| hover 설명 | [trait_hover.h](../../../reference_mod/SAN14CE_TraitHover_Standalone_v1_0/SAN14CE_TraitHover_Standalone_v1_0/source/trait_hover.h), [hover_routes.h](../../../reference_mod/SAN14CE_TraitHover_Standalone_v1_0/SAN14CE_TraitHover_Standalone_v1_0/source/hover_routes.h) | 목록/본문 vtable 타입 및 formatter→개성ID를 구분. 소유 화면별 상태·좌표/열 식별 후 설명 조합 |
| C 화면 확장 예 | [traits9_ui.c](<../../../reference_mod/SAN14CE_4Mods_v1_3/01_포획대사/source/reference/ui_base/traits9_ui.c>) | 화면 owner별 labels[9]·registered/active 상태. 전역 버튼 상태 하나로 모든 창을 처리하지 않음 |
| 포획 대사 | [captive_dialog.c](../../../reference_mod/SAN14CE_CaptiveDialog_Standalone_v1_0/SAN14CE_CaptiveDialog_Standalone_v1_0/source/captive_dialog.c):34–50 | 포로 세력 또는 포획 세력이 플레이어이면 원작 대사 경로 사용. 대사 UI를 새로 작성한 것이 아님 |
| 관계 탭 | [RelationTab manifest](../../../reference_mod/SAN14CE_RelationTab_RC2_4_HoverPin_Install/SAN14CE_RelationTab_RC2_4_HoverPin/package.manifest.json) | ‘CPersonData 목록·선택창, 원래 관계 탭 제외’라는 지원 범위. 제공 설명이며 구현 소스 없음 |
| 출진 프리셋 | [RC25 호환 설명](../../../reference_mod/SAN14CE_RC25_Groups_AutoSave/SAN14CE_SortiePresets_RC25_Groups_AutoSave/COMPATIBILITY_KO.txt) | 기존 전역5훅 유지, 이름 입력 객체의 사설 vtable을 일시 적용한다는 설명. 구현 소스 없음 |

에디터 소스의 09-28 값: 초기화 `0x8DF198/0x8DF1F5`, 표시 `0x907294`; 일람 `SITE=0x2BF033`, 탭표 `0x1A30830`, 열표 `0x1A2B850`.머리말의 `0x8DE7D8/0x906864` 등은 **09-10 기록**이다.

반면 standalone C 툴팁/포획 코드는 타임스탬프 `0x6A6D9F72`만 허용한다. `PLAYER_RVA=0x232CD0`, `CALL_RVA=0x2E042D`, `RELAY_RVA=0x133F040`를 현 게임에 그대로 연결하지 않는다. [빌드 차이](COMPATIBILITY.md)를 먼저 확인한다.

재사용할 설계: 화면 생성→부품 확인→표시/활성화→선택→결정/취소→상태 해제의 생명주기를 모두 연결한다. 모델/레이아웃과 formatter를 같이 바꾸고, 화면 재진입·빈 선택·취소를 검사한다. 다른 패키지의 설치 DLL을 우리 DLL 위에 복사해 통합하는 방식은 사용하지 않는다.
