# UI 리소스의 적용·기능 OFF·설치 해제

2026-11-03 사용자 요구. 조조전과 배속에서 반복된 실수는 DLL 훅을 끄면 디스크 UI 자료도 복원된다고 가정한 것이다.

## 앞으로 지킬 기준

1. UI 변경은 훅뿐 아니라 RDB 색인·bank·아틀라스·그림 표·레이아웃과 설정 키·소유 DLL을 함께 기록한다.
2. 기능 OFF와 상위 그룹 OFF는 해당 기능의 리소스 참조도 복원한다. 설치기는 설정 선택 이후 적용 계획을 만든다.
3. 모드 전체 해제는 설정값과 무관하게 우리 화면 패치를 모두 설치 전 기준으로 되돌린다. 다른 모드가 변경한 참조·공유 bank는 보존한다. DLL 삭제만으로 해제했다고 안내하지 않는다.
4. original_entry는 신뢰할 수 있는 패치 전 백업으로 검증한다. 이미 모드 bank를 가리키는 현재 자료를 원본으로 채택하지 않는다. 조조전 군단 초상화의 bank28 잔존이 이 실패였다.
5. 게임 종료 후 기존 guarded 관리자 경로로만 리소스를 쓴다. RDB 전체 교체·스냅샷 덮어쓰기·해시 검사 해제로 해결하지 않는다.
6. 설정만 저장이 리소스를 동기화하지 못하는 상태라면 종료 후 설치/적용이 필요함을 알린다. INI 직접 수정과 게임 재시작만으로 색인이 복원된다고 설명하지 않는다.
7. 관련 최소 검사는 ON→OFF→ON, 상위 그룹 OFF, 전체 해제, 관계없는 참조 보존이다. 파일·모의 검사와 실게임 화면 확인을 구분하며 무관한 전체 검사를 반복하지 않는다.

## 이번에 확인한 소유 범위

| 기능 | 활성 조건 | 리소스 KTID |
|---|---|---|
| 조조전 지면 부착 UI | groups.convenience && ccz.ccz_sprites | 3DA1641D,757F1A41,5C046EA3 |
| ×4·×8 배속 | groups.convenience && game_flow.speed_buttons | 301526D7 아틀라스,FEA5F28C 그림 표,40F993CE 팝업,454A1BCC HUD |

배속은 원작 그림 뒤에 새 버튼 자료를 추가했다. 훅만 OFF면 추가 노드·그림이 남으므로 4개를 함께 원래 참조로 되돌린다. 다른 UI 패치는 유지한다.

전체 해제 대상은 traits9_layout.json.records 전체(현재14개)다. 기능별 OFF와 전체 해제를 혼동하지 않는다. 이 수정에서 조조전3개·배속4개의 설정별 선택을 처리했으며, 다른 기능의 OFF까지 실게임 확인했다고 주장하지 않는다. 새 UI 기능은 소유 리소스와 OFF/해제 경로를 함께 등록한다.

## 구현과 검사

- distribution/SplitManage.ps1의 Get-TraitLayoutPlan: 이전 참조 복원→설정별 선택→안전한 bank 연결. Reset-TraitLayout은 전체 해제에서도 사용한다.
- distribution/Manage.ps1: 동일 설치 패키지·게임 종료 상태의 Select는 guarded Apply를 재사용한다.
- tests/verify_ccz_layout_switch.ps1: CCZ/배속 조합·복원·다른 자료 보존 검사.
- tests/verify_ccz_select.py: 관리자 Select의 실제 ON/OFF·전체 해제를 임시 게임 폴더에서 검사.
- 배속 근거: research/speed_buttons/build_patch.py. 참고용이며 과거 실험 도구로 실제 게임을 직접 수정하지 않는다.
- 조조전 원본 참조 문제: [CCZ_LAYOUT_SWITCH](CCZ_LAYOUT_SWITCH.md). 다른 UI 실패 기록: [UI_FAILURES_AND_RECIPES](UI_FAILURES_AND_RECIPES.md).
