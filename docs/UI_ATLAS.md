# 게임 UI 정적 지도

원작 UI에 버튼·메뉴를 추가하거나 화면 접근·콜백·부품 ID를 찾을 때 [UI atlas](../research/ui_atlas/README.md)를 먼저 읽는다. 추출 기준과 검증 결과는 해당 폴더의 `binary_manifest.json`, `resource_summary.json`, `verification.json`이다.

조회: `python -X utf8 AI_TacticsMod/research/ui_atlas/atlas.py search 화계` 또는 `bundle 'CArmyButtonMenu@@'`. 경로는 게임 루트 기준이다. 모든 JSONL/디컴파일을 통째로 읽지 않는다.

인덱스는 전체 RTTI 9,697개 중 UI 후보 3,531개, 설치 LCSK 1,085개와 부품 45,444개를 포함한다. 도시/부대/메인 HUD/공통 대화창의 계약과 확장 제약을 별도로 기록했다. 후보 분류/가상 슬롯/리소스 이름만으로 실제 클릭 경로·ABI가 입증되는 것은 아니다. 실제 게임 화면 관찰은 수행하지 않았다.

이 조사는 사용자가 명시적으로 승인한 REA 정적 분석이다. DLL 설치·UI 구현 변경을 뜻하지 않는다. UI 관련 모드 연결은 편의 DLL이 소유하고, 기존 게임 함수는 원래 UI 스레드에서만 호출한다.

새 버튼을 추가할 때의 조사 순서와 시간 낭비 사례: [UI_ADD_GUIDE](UI_ADD_GUIDE.md).
