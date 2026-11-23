# UI 리소스의 적용·기능 OFF·설치 해제

2026-11-03 사용자 요구. 조조전과 배속에서 반복된 실수는 DLL 훅을 끄면 디스크 UI 자료도 복원된다고 가정한 것이다.

## 앞으로 지킬 기준

1. UI 변경은 훅뿐 아니라 RDB 색인·bank·아틀라스·그림 표·레이아웃과 설정 키·소유 DLL을 함께 기록한다.
2. 기능 OFF와 상위 그룹 OFF는 해당 기능의 리소스 참조도 복원한다. 설치기는 설정 선택 이후 적용 계획을 만든다.
3. 모드 전체 해제는 설정값과 무관하게 우리 화면 패치를 모두 설치 전 기준으로 되돌린다. 다른 모드가 변경한 참조·공유 bank는 보존한다. DLL 삭제만으로 해제했다고 안내하지 않는다.
4. original_entry는 신뢰할 수 있는 패치 전 백업으로 검증한다. 이미 모드 bank를 가리키는 현재 자료를 원본으로 채택하지 않는다. 조조전 군단 초상화의 bank28 잔존이 이 실패였다.
5. 게임 종료 후 기존 guarded 관리자 경로로만 리소스를 쓴다. RDB 전체 교체·스냅샷 덮어쓰기·해시 검사 해제로 해결하지 않는다.
6. 설정만 저장이 리소스를 동기화하지 못하는 상태라면 종료 후 설치/적용이 필요함을 알린다. INI 직접 수정과 게임 재시작만으로 색인이 복원된다고 설명하지 않는다.
8. (2026-11-05 사용자, v15.6-local2 실패) **실행 중에 기능이 빠져도 UI는 원작을 따른다.** 설정 OFF뿐 아니라 가드 불일치·주소 미해결·DLL 롤백·소유 DLL 부재로 기능이 설치되지 않으면, 그 기능의 UI(추가 노드·버튼·행)도 보이지 않거나 원작 동작이어야 한다. 눌리지 않는 버튼이나 빈 칸을 남기지 않는다.
   - 실행 중: SAN14_Layout.dll이 기능별 소유 노드 표를 갖고, 레이아웃 생성 관찰(86E338)에서 소유 기능이 게시되지 않았으면 추가 노드를 숨긴다(원작 노드는 건드리지 않음).
   - 설치 때: 설치기는 현재 EXE 빌드에서 꺼지는 기능(빌드 게이트·expected_disabled)의 RDB 레코드를 적용하지 않는다.
   - 최종: 추가 UI는 런타임 생성(Layout P5)으로 옮겨 디스크 RDB 의존을 없앤다. 내용을 바꾼 레코드(조조전 지면 부착 등)는 런타임 생성 전까지 설치 때 판정으로만 막을 수 있다.
   - **구현 상태 (local3, 2026-11-05, 모의·파일 검사까지):** 설치 때 부분은 `distribution/SplitManage.ps1`의 `Get-GameBuildLayoutOff`/`Get-TraitLayoutPlan`이 맡는다(EXE TimeDateStamp 표. 지금은 1a4a447d에서 traits9 레코드 4개를 적용하지 않는다. 표의 근거는 `expected_disabled.json`). 실행 중 숨김은 **구현하지 않았다**: 생성 관찰점에 컨트롤이 없고, 컨트롤 생성 뒤 숨기는 원작 경로를 원작 코드로 확인하지 못했다. 시점 탐색 로그(`layout_p4_node_timing`)만 넣었다(HISTORY.md 12절).
9. 관련 최소 검사는 ON→OFF→ON, 상위 그룹 OFF, 전체 해제, 관계없는 참조 보존이다. 파일·모의 검사와 실게임 화면 확인을 구분하며 무관한 전체 검사를 반복하지 않는다.

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

## 10. UI 진입점은 Layout DLL이 소유한다 (2026-11-07 사용자, 모든 새 모드의 표준)

용어: **UI = 사용자가 기능으로 들어가는 진입점**(버튼, 창, 토글). **기능(모드) = 그 진입점을 눌렀을 때 실제로 도는 동작**.

1. **소유.** `SAN14_Layout.dll`(`src/layout_*`)이 모든 UI 진입점의 **생성·위치·보임·소멸**과 클릭을 소유 기능으로 넘기는 일을 맡는다. 기능 모듈(Convenience 등)은 동작(클릭이 하는 일, 데이터, 글자, 적용 루프)만 갖고, 게임 UI 객체를 직접 만들거나 부수지 않는다. 기능 모듈은 `layout_client::ui_*`(`src/native_layout_client.inl`)로 요청하고, 서술자(위치·크기·종류, 데이터)와 글자·콜백을 건넨다.
2. **연결 = 기능 스위치.** `layout_client::UiScope scope("<owner>")`는 그 owner의 `layout_registry.h` 훅이 게시(installed/chained)돼 있을 때만 열린다. 기능이 꺼졌거나 게시 전이거나 거절·양보되면 `scope.ok==false`이고 UI는 만들어지지 않는다. 원작 화면은 그대로다. 새 설정 키를 만들지 않는다. 설치기가 기능을 설치·제거하면 UI도 같이 움직인다. 이는 이 문서 8항(실행 중 기능이 빠져도 UI는 원작)을 런타임 생성 UI에 적용한 구현이다.
3. **추적.** Layout은 scope 안에서 만든 객체를 owner 이름과 함께 기록한다(`Created.feature`). 소멸은 `ui_release(owner,next)` 하나다: 창마다 원작 순서 `vt+0x40(obj)`, `vt+0x28(obj,next)`, `vt[0](obj,1)`(`docs/TACTIC_PRIORITY_V2_PLAN.md` 12.6, 근거 `007D.c` FUN_1407dc200·`0093.c` FUN_140933ba0)로 없애고 자식 컨트롤 추적을 지운다. 부모 소멸은 자식을 해제하지 않고 고아로만 만드므로, 화면 떠남 훅이 **원래 떠남보다 먼저** 부른다.
4. **원작 순서 요구.** 새 레이아웃은 컨트롤을 다 만든 뒤 `vt+0x38`(`ui_activate`, FUN_140869210)을 불러 UI 관리자 층 목록에 등록해야 보이고 갱신된다. 숨김 처리·포인터 포기·누수로 정리하지 않는다.
5. **훅은 소유자 하나.** 훅 몸체는 기능 모듈에 있고(한 훅 한 소유자, `layout_registry.h` owner), UI를 부르는 시점만 그 훅(갱신·떠남·원작 생성 CALL)이 정한다. 훅 시점에서 Layout의 UI 서비스를 부른다.
6. **새 UI를 만들 때 체크리스트.** (a) 기능 모듈에 UI 생성·소멸 호출이 없는가 (b) 모든 호출이 `UiScope` 안인가 (c) 떠남 훅에서 `ui_release`를 부르는가 (d) 기능 OFF·미게시 때 `scope.ok==false`로 아무것도 안 만드는가 (e) 새 Layout 서비스가 필요하면 `layout_api.h`에 덧붙이고(크기 검사) `tests/layout_*` 모의 검사에 올린다.
7. **tactic_priority 적용 예.** 우측 하단 버튼(2026-11-22 전에는 푸터 버튼)·팝업(창·탭·칸·버튼)·「출진 기억」 토글 전부 위 방식. 기능 모듈에 남은 것: 훅 몸체, 서술자 데이터, 글자, 클릭 동작, 원작 `refresh_tabs`(상태 갱신). 상세 `docs/TACTIC_PRIORITY_V2_PLAN.md` 12.7.
