# Layout P4 진단 로그 읽는 법 (2026-11-05)

`SAN14_Layout.dll`의 레이아웃 생성·소멸 관찰점과 런타임 UI 서비스를 실게임에서 확인하는 **진단 빌드 1회**용이다. 진단 빌드(`NO_RUNTIME_LOG` 없음)에서만 동작하고, 새 설정 키는 없다. 배포 빌드는 구독자가 있을 때만 관찰 훅을 건다(지금은 구독자가 없어 변화 없음). 설계는 [LAYOUT_DLL_COMPAT_DESIGN](LAYOUT_DLL_COMPAT_DESIGN.md) 4.4/4.5, 구현은 `src/native_layout_ui.inl`.

## 관찰점 (입구 우회가 아니라 호출 지점)

| 이벤트 | 지점 | 근거 |
|---|---|---|
| 생성(built) | `86E338` E8(→`92B0A0`), 86E320 몸체 안 | `92B0A0`의 호출자는 86E320 하나. 86E320(vt+0x140, 642칸 공유) 입구는 건드리지 않는다. 레퍼런스 DLL 175개에 86E320 몸체·이 호출 창 보관 없음(IC는 vtable 이미지만 보관) |
| 소멸(destroyed) | `865560` 입구 6바이트(`40 57 48 83 EC 30`) + 트램폴린 | `~CUILayout` 몸체, 모든 레이아웃 소멸자가 끝에서 부름(E8 호출자 200곳). 레퍼런스 DLL 175개에 몸체 창 보관 없음. 바이트가 다르거나 이미 E9면 등록/설치 거부, 생성 이벤트만 동작 |
| 이름 | layout_wait의 86C570 E8 3곳이 `note_layout_load(obj,name,async)`로 보고 | 86E0C0 경유 로드(516곳)는 이름이 없고 KTID로만 식별 |

콜백은 86E320/865560을 부른 게임 스레드에서, 중첩 없이(콜백 안에서 일어난 생성 이벤트는 버림) 실행된다.

## 로그 이벤트 (`log_event(이벤트, a, b, c)`, 숫자는 10진)

| 이벤트 | a / b / c | 의미 |
|---|---|---|
| `layout_events_registered` | 생성 훅 핸들 / 소멸 훅 핸들 / 0 | 관찰 훅 등록. 소멸 핸들 0이면 `layout_events_destroy_bytes`(865560 바이트 불일치)가 함께 나옴 |
| `layout_installed`·`layout_chained`·`layout_refused` | 지점 RVA / 사유 / owner 해시 | owner `layout_events`, `layout_events_destroy` |
| `layout_p4_res_head` | seq / 이름 해시(×31) / 리소스 핸들 하위 32비트 | 처음 12개 생성 이벤트의 헤더 |
| `layout_p4_res_raw`, `layout_p4_res_inner` | (seq<<8)\|g / 값 2개 / | 핸들 앞 0x40바이트, 그리고 `*(핸들+0x20)` 앞 0x40바이트. g=0..7이 dword 2개씩 |
| `layout_p4_ktid_match` | 오프셋 / 종류 / seq | 종류 1=이름 해시가 핸들에서 발견, 2=알려진 KTID(`cmn_footer_re_cn`=D8913AB2=3633396402)가 핸들에서 발견, 3=알려진 KTID가 안쪽 객체에서, 4=이름 해시가 안쪽 객체에서 |
| `layout_p4_ktid_cand` | 후보 마스크 / 같은 이름 쌍 수 / 다른 이름 쌍 수 | 같은 이름에서 같고 다른 이름에서 다른 dword. 비트 i = 오프셋 4i. 이름 해시 자리(종류 1)도 포함되므로 뺀다 |
| `layout_p4_built_cost` | 호출 수 / 평균 ns / 최대 ns | 생성 훅 우리 몫(기록·진단·콜백). 64회마다 |
| `layout_p4_orig_cost` | 호출 수 / 평균 ns / 최대 ns | 같은 호출의 원본 `92B0A0` 시간 |
| `layout_p4_counts` | 동기 생성 수 / 비동기 로드 수 / 비동기 로드가 생성 이벤트에 닿은 수 | 비동기(86C570 async≠0)가 86E320을 지나는지 |
| `layout_p4_state` | 소멸 이벤트 수 / 중첩으로 버린 수 / 살아 있는 수 + 다른 스레드 호출 수×65536 | 다른 스레드 호출 수가 0이 아니면 보고 |
| `layout_p4_destroy_cost` | 호출 수 / 평균 ns / 최대 ns | 865560 훅 우리 몫. 1024회마다 |
| `layout_p4_screen` | id / 노드 수 / 스레드 ID | id 1 속도(vt 1473DC8), 2 군주 교체 footer(146AFE8), 3 전문가(141BE88), 4 조조전 타이틀(13E4C78). 화면당 처음 2회 |
| `layout_p4_layout_xy` | id / +0x124 / +0x128 | 86C570이 저장한 x,y 원값 |
| `layout_p4_node`, `_node_xy`, `_node_wh` | (id<<16)\|노드 / 컨트롤 포인터 하위·상위 · 위치 x,y(정수) · 크기 w,h | 컨트롤이 이미 만들어진 노드만 나온다 |
| `layout_ui_*` | | `rejected`(이벤트 밖 호출), `fault`(SEH 잡음, 단계 1 로드 2 복제 3 라벨), `load_failed`, `clone_unavailable`(노드 번호), `label_unavailable` |

수명 시험(`layout_p4_life*`)은 2026-11-08에 퇴역했다. 군주 교체 footer가 처음 만들어지는 콜백 안에서 부모 없는 서브레이아웃 로드를 시도했는데, 그 시점에는 원작 로더가 완료 플래그(`+0x12C`)를 세우지 못해 매 실행 1단계에서 멈췄다(진단 빌드 전용, 기능 영향 없음). 런타임 UI 서비스(복제·이동·글자·클릭·라벨·파괴)는 `tests/layout_p4_fixture.cpp`가 가짜 게임 객체 모델에서 검사한다.

## 사용자 확인 순서

1. 진단 빌드 설치(Claude가 지시) 후 게임 실행, 타이틀 화면에서 조조전 메뉴가 보이는지 확인 → `layout_p4_screen 4`.
2. 새 게임 → 세력 선택 화면(군주 교체 footer): `layout_p4_screen 2`가 나오는지 확인한다. 수명 시험은 퇴역했으므로 `layout_p4_life`는 없다. 크래시가 나면 `layout_ui_fault`를 보고한다.
3. 지도 화면에서 속도 버튼 영역을 연다(`layout_p4_screen 1`), 환경설정의 전문가 설정 화면을 연다(`layout_p4_screen 3`).
4. 각 화면을 열고 닫기를 2회 이상 반복한다. 소멸 수(`layout_p4_state` a)가 늘고 살아 있는 수(c의 하위 16비트)가 화면 수만큼 유지되는지 본다.
5. 로드/저장, 턴 진행 몇 번. 끝나면 게임을 종료하고 로그에서 `layout_p4`·`layout_events`·`layout_ui`·`layout_refused` 줄을 모아 보고한다.

판독: KTID 오프셋 = `layout_p4_ktid_cand`의 마지막 마스크에서 `layout_p4_ktid_match 종류 1`의 오프셋을 뺀 비트. `종류 2`가 같은 오프셋에서 나오면 확정. 성능 질문은 `layout_p4_built_cost`/`orig_cost`의 평균(ns)과 호출 수 비교(훅 몫이 원본 몫보다 작아야 한다).

모의 검사(`tests/layout_p4_fixture.cpp`)는 가짜 게임 객체 모델에서 이벤트·수명 순서를 확인할 뿐이며 실게임 확인이 아니다.
