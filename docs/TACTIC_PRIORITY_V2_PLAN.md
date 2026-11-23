# 전법 우선순위 v2 (local15) 계획

사용자 피드백(2026-11-07, local14 실게임): 목록·다단계 대화창은 비직관적. 원작 UI를 살려 출진 화면을 바꾸지 않는 그리드 팝업, 페이지 넘김 금지(창을 키워서라도 한눈에), 원작 버튼 모양, 성능 저하 없음. 「마지막 진형·전법」은 무장정보 탭 옆 토글로 무장별 개인 규칙(전법 3개 미만에도 작동), 우선순위는 공통 규칙. xhigh 설계 에이전트 결과 요약.

## 결정
- 팝업 = 원작 "설명자 배열로 창 만들기" 870410(옛 86E060)로 만든 원작 프레임 + 원작 버튼(`9309C0` kind 0 style 3 = hanyou_defo 칸, kind 1 style 9/10 모드 탭). 오버레이 아님. 50칸까지 한 화면, 넘으면 창 확장. 페이지 없음.
- 「출진 기억」 = 무장별 토글, 탭 모양 radio(style 10)를 「무장정보」 탭 옆에. 상태는 세이브별 `.tactics` v2.
- **버그 수정**: 현재 적용 코드는 편성 중 `state+0x540`(출발 순간에만 유효)을 읽음(local14 로그 `tactic_priority_state army=3929893979`). 편성 기록 목록 `state+0x4C0..+0x4C8`로.
- 다단계 편집기 `native_tactic_priority_editor.inl` 삭제. `tactic_priority_io.h` 유지.
- 새 Layout 훅 행 없음(기존 4행 그대로).

## 1. 근거 (1a4a447d, 옛 주소)
- `870410(pane,x,y,w,h,parent,desc,count)` → `86E590`(창 초기화, parent면 `vt+0xD8`로 자식 추가) → `92DAC0(pane+0x130,desc,count,pane)`. 원작 호출 11곳. 864F80 `870410(p,610,390,700,300,0,desc,3)`(1920×1080 중앙 대화창, `92D670`으로 클릭), 80B780 전체 화면. 좌표 = 1920×1080 논리.
- 설명자 0x30바이트: `+0 type, +4 x, +8 y, +0xC w, +0x10 h, +0x18 int text_id, +0x20 wchar_t* text, +0x28 int -1`. type 0 빈, 1 글자(+0x20 또는 `30F010(+0x18)`), 2 크기만 이미지 노드(`table[1][i]`, 이후 `9309C0`으로 버튼), 3 사각형, 5/6/7 CCmnButton 글자 버튼, 8 `000_cmn_base_dialog_00` + `932940` 크기 조절(출진 화면 오른쪽 창 노드 0x28이 1094×736·954×736로 씀). count<256.
- 출진 화면 객체(두 판 동일): +0x470 메인 레이아웃 / +0x488 탭 그룹(+0x148 현재 기록, `*(*(+0x130)+0x138)+0x18` 현재 탭) / +0x490·+0x498 부대정보·무장정보 페이지 / +0x4A0 하단 바 / +0x4B0 원작 modal / +0x4C0..+0x4C8 편성 기록 포인터 목록(기록 0x94바이트, `cmd=rec+4`: [0] 인물 [2] 병력 [3] 진형 [5..7] 전법) / +0x540 출발 목록.
- 탭 생성(7D67E0, 옛 7D4430): `9309C0(main+0x130,0x29,9,1,1,0)`, `(…,0x2B,10,1,1,0)`, 글자 `9332B0(table,node,text,1)`.
- 클릭: `1A4FB0(ctrl,0x7FFFFFFE,functor)`(local14 `tactic_priority_click`로 실게임 확인). 콜백 인자 컨트롤, `+0x80` = 노드.
- 금지: 노드 범위 밖 id를 `930740` 등록(92D670이 범위 밖 읽음). kind 0 버튼(0x198바이트)에 `85D050`(+0x198 씀, radio 0x1A0만).
- GA(이름은 옛 RVA): **신규** GA_86E060(870410), GA_3F3C30(3F6150 취소 입력 조회), GA_85AD20(85D050 radio 선택), GA_855BC0(857EB0 탭 그룹 다시 고르기 +0x84=1,+0x80=idx). 기존 GA_92E430(9309C0), GA_930D20(9332B0), GA_3F6BC0/3F6C50(할당/해제), GA_864C50(866FA0 창 생성자), GA_92AB70(92D020 표 생성자), GA_1A3CB0(1A4FB0), GA_92CEE0(92F390), GA_92F810(931DA0)/GA_8741D0(876580) 표시, GA_873370(875720)/GA_873450(875800), GA_92CD00(92F1B0)/GA_92D040(92F4F0) 위치·크기. 예비 GA_84ABA0(84CF00), GA_92E1B0(930740).

## 2. 우선순위 팝업
- 위치: 오른쪽 창 사각형에 정확히 겹침. 기준점 1) `state+0x490` 페이지 `+0x20/+0x24` 2) 탭 왼쪽 `92F390(main+0x130,0x29)` `+0x20/+0x24` −(39,0) 3) 화면 중앙. 너비 판 A 1094 / 판 B 954(기준 x 786 근처 A, 835 근처 B), 높이 783. 첫 판 로그로 확인.
- 칸 164×48(원작 KTID A32B67AF 대화창 hanyou_defo), 기본 5열, 간격 x 12~24, y 10, 좌우 여백 39. 판 B 가용 높이 603 → 10행 50칸. 51~60칸 6열 너비 140(최소 108), 그 이상 창 왼쪽 확장. 예상 27~36칸. 1280×720에서 약 109×32px.
- 설명자: 0 type 8 프레임(0,0,W,H) / 1,2 type 2 모드 탭(39,0,160,64),(193,0,160,64) → `9309C0(t,i,9|10,1,1,0)` / 3 type 1 안내 글(377,18,W−416,28) / 4..4+N−1 type 2 칸(layout_grid) → `9309C0(t,i,3,0,1,1)` order 1 / 다음 3개 type 2 「초기화」「기본값」「닫기」 164×48 오른쪽 아래. 안내 예 「무력형 7개 지정 · 누른 순서가 우선순위, 다시 누르면 해제」(`9332B0` 갱신).
- 생성(UI 스레드, 화면 진입 후 처음 열 때 1회): `p=3F90E0(0x178)`, memset, `866FA0(p)`(+0x130 표가 안에서 만들어지는지 G1, 아니면 `92D020(p+0x130)`) → `870410(p,ax,ay,W,H,state+0x470,desc,n)` → 노드마다 `9309C0`, 글자 `vt+0x1D8`(칸·버튼)/`9332B0`(탭), `1A4FB0` 묶기(local14 Callable) → 보이기 맞추기(ccz_title 방식: `p+0x40` 0xB 비트·`+0x30` 투명도를 메인 레이아웃에서 복사, `875720(p,1)`, `875800`) → 보이는 상태로 생성(만들자마자 숨기면 배경 사라짐), 이후 `876580(p,0/1)`.
- 열기: 탭 0x29/0x2B `931DA0(main+0x130,n,0)` 숨김(우리 모드 탭이 같은 자리), 토글 숨김, 두 페이지 `vt+200(0)`, 모드 = 현재 무장 무력/지력, 탭 `85D050` 1/0.
- 닫기: 바뀌었으면 저장 → 숨김 → 탭 `931DA0(…,1)` → 토글 보임 → `857EB0(탭그룹,현재탭)`, 1프레임 뒤 페이지가 숨겨져 있으면 `vt+200(1)`.
- 닫는 경우: 「닫기」 저장 후 / ESC: 칸5 훅에서 `3F6150()` 참이면 닫고 그 프레임 원래 함수 건너뜀, 계속 참이면 최대 30프레임 더 / 다른 부대 선택(+0x148 변경) 저장 후 / 원작 modal(+0x4B0≠0) / 결정·뒤로: 원작대로, 출발·떠남 훅이 저장·포인터 비움.
- 클릭: 노드 = `ctrl+0x80`. 칸 → `toggle_rank`(끝에 추가, 다시 누르면 빠지고 뒤 순위 당김), 영향 칸 글자만 다시. 글자 `"%u. %s"`/이름, 전법 이름 = 전법 표 +0x10(최대 40자), 0번 칸 「고유전법」. 탭 → 모드 전환·전체 글자. 「초기화」 전부 비움, 「기본값」 기본 순서 1..N. 바꿀 때마다 `generation++`(출진 기억 꺼진 부대 즉시 재계산, 사용자가 직접 바꾼 부대는 존중).
- 수명: 화면 진입마다 처음 열 때 1회. 떠남 훅은 포인터만 비움. 부모가 자식 창을 해제하는지 G2, 아니면 떠남 훅에서 원래 함수 전에 자식 제거 + `vt[0](p,1)`.
- 실패: 기능 꺼짐·가드·GA 미해결이면 설치 안 함(원작 화면 그대로). 실행 중 생성 실패면 로그 + `popup_broken`, 이후 클릭 무시.

## 3. 「출진 기억」 토글
- 첫 갱신(update 훅 `grid::tick`)에서 생성(처음 계획은 화면 진입 훅 7DE5BD였고 그 CALL 훅은 2026-11-22 삭제): 1노드 창(type 2) + `9309C0(t,0,10,1,1,0)`, 위치 탭 오른쪽 + (154,0), 200×64(최소 너비 `button_styles.py` 확인). 탭 컨트롤 못 찾으면 첫 update로 미룸.
- 글자 「출진 기억 켬」/「출진 기억 끔」, 켜짐 `85D050(1)`. radio가 스스로 바꿀 수 있어 매 프레임 `+0x198` 비교 후 다르면 다시 씀. 탭 오른쪽 컨트롤의 0xB 비트·투명도 매 프레임 따라감. 기록에 무장 없으면 「출진 기억」만, 클릭 무시.
- 켜면 그 무장 플래그 on + 기억 바로 적용 + `857EB0`. 끄면 현재 선택은 그대로, 다음 배치부터 공통 규칙.

## 4. 적용 루프 (`native_tactic_priority.inl`)
- 매 프레임 `state+0x4C0..+0x4C8` 기록. 기록 포인터 키로 마지막 인물·진형·generation.
- 인물 변경: 플래그 on + 기억 있음 → 진형 복원(`formation_usable`·공성 병력 규칙) → 기억 전법 중 보유·진형 호환 먼저 → 남은 칸 공통 순서. 전법 수 무관, 못 쓰는 것 건너뜀. 플래그 off → 쓸 수 있는 전법 4개 이상일 때만 공통 순서 3개.
- 진형 변경 → 재선택. generation 변경 → 플래그 off 부대만. 사용자 수동 선택은 classify로 존중.
- 현재 기록(`*(탭그룹+0x148)`)에 썼으면 `857EB0(탭그룹,현재탭)`.
- 출발 기록(+0x540) 유지, 기록 시 플래그 보존.

## 5. 형식
- 프로필 v2: `v=2;martial=<순위 id 목록|비어 있음>;intellect=<…>`, ASCII ≤2047, 실제 순서 = 순위 + 나머지 기본 순서. v1 읽어 변환(`ranked_from_full`로 기본과 같은 꼬리 제거), `restore` 버림 + `tactic_priority_profile_migrated` 1회. 사용자가 저장할 때만 v2로 씀. 설치기는 원시값 보존(문구만 변경).
- 사이드카 v2: `"TPRM\2\0\0\0"` + binding 32 + 6000×{진형, 전법 3, flags} = 30040바이트, flags bit0만(다른 비트 = 범위 오류 4). 24040+`\1`은 v1 → flags 0. 크기·magic 엇갈림 = 2. 쓸 땐 항상 v2, `empty()`는 flags도. 함수 이름 그대로 → native_merit_save.inl·verify_traits9_save.py 변경 없음.

## 6. 편집기 제거
editor 파일 삭제, native_adapter.cpp include, native_tactic_priority.inl 전방 선언·`open_requested`/`editor_active`, address_guard_fixture.cpp stub 삭제. native_split_install.inl `split_disable(f.tactic_priority,39)`(ccz 대화창 포인터 조건) 삭제, split_pointer_enabled는 `ccz_sprites`만. 하단 바 버튼은 팝업 열기용으로 유지, 글자 작음은 862010이 옆 버튼 style(0xE 또는 2)을 따르게 맞추고 `tactic_priority_footer_style` 기록.

## 7. 작업 패키지
순서 WP-4a → WP-1·WP-3·WP-2 병행(고정 API) → 병합 WP-1 → WP-3 → WP-2 → WP-4b(`--no-ff`).
- WP-4a(코디네이터): gen_signatures `_tactic_priority_ids()`에 신규 4개, 재생성, 필요 시 b_verdicts. `-Feature address-table`.
- WP-1 엔진(`tactic_priority_profile.h`, `tests/tactic_priority_fixture.cpp`): `Settings{martial,intellect}`(순위만, restore 제거), `effective(ranked,defaults)`, `rank_of`(1부터, 없으면 0), `toggle_rank(ranked,id,tiles)`, `clear`, `preset`(=default_list), `layout_grid(n,avail_w,avail_h)`→`{cols,rows,tile_w,tile_h,gap_x,gap_y,w,h,cells[],expanded}`, `encode`(v2), `decode(text,catalog,out,version,restore_v1)`, `ranked_from_full`, `select`, `select_remembered(remembered[3],eligible,effective)`. fixture: 순위 연산, 그리드(27/36/50 5열, 55 6열 140, 70 expanded, ≥108, 경계·겹침), 프로필 v2·v1 변환, 선택(≤3 덮어쓰지 않음, ≥4 순서, select_remembered 0~10개).
- WP-3 기억 v2(`tactic_memory_store.h`, `native_tactic_memory_save.inl`, `tests/tactic_memory_fixture.cpp`): `Record{formation,tactics[3],flags}`, `kFlagRemember=1`, `EncodedSizeV1=24040`, `EncodedSize=30040`, `decode(…,&version)`(1~4), `merge_record`, `tactic_memory_flag(person)`, `tactic_memory_set_flag(person,on)`, `tactic_memory_record`(플래그 보존). fixture + `-Feature merit`.
- WP-2 네이티브 UI·훅: 새 `native_tactic_priority_grid.inl`(native_tactic_priority.inl이 include), native_tactic_priority.inl 수정, editor 삭제, native_adapter.cpp 한 줄, address_guard_fixture.cpp. 0단계 ghidra 읽기: G1 866FA0이 +0x130 표 생성? G2 부모가 자식 해제? G3 type 8 경로 막는 조건? 완료: 빌드, `-Feature tactic-priority -Build`, hook_map(4행 그대로), layout-registry(261행).
- WP-4b(코디네이터): native_split_install.inl, features.json(editor→grid), check_feature.ps1, check_addresses.py, 설치기 문구(「출진 화면 전법 우선순위·출진 기억」, 새 키 없음), TACTIC_PRIORITY.md, GA 재생성. 최종 `-Feature dll-split -Build`, install_local.

## 8. 성능 예산
닫힘: `tactic_priority_update` 평균 ≤15µs, 적용 프레임 최대 0.2ms. 열림: ESC 조회 ≤5µs 추가, 클릭 ≤1ms. 생성: 팝업 ≤5ms 1회, 토글 ≤1ms 1회. 닫혀 있을 때 추가 그리기는 토글 1개. COST_SCOPE `tactic_priority_update`/`_popup_build`/`_click`/`_toggle_sync`, 진단판 `tactic_priority_perf(frames,avg_ns,max_ns)` 1800프레임마다.

## 9. 진단 이벤트
생성 `_popup_build(stage,n,ok)`, `_popup_anchor(src,x,y)`, `_popup_rect`, `_popup_vis(대상,alpha,bits40)` / 열기·닫기 `_popup_open(mode,ranked)`, `_popup_close(reason 1~6)`, `_cancel_frames`, `_leak(1 기록/2 탭/3 modal)`, `_page_restore` / 조작 `_tile`, `_mode`, `_saved`, `_save_failed`, `_refresh`, `_record(i,person,formation)` / 토글 `_toggle_built`, `_toggle(person,on)`, `_toggle_sync`, `_footer_style` / 기존 `_applied`/`_chosen`/`_kept`/`_overwritten`/`_restore`/`_restore_skipped`/`_catalog` / 기억 `tactic_memory_load_restored(version,count)`, `_load_rejected`, `_migrated`. (모든 이름 앞에 `tactic_priority`)

## 10. 위험
870410 경로 첫 사용(G1~G3·진단) / type 8 프레임 원작 정적 사용례 없음 → 실패 시 type 3 사각형 또는 오른쪽 창 그대로 두고 칸만 / 클릭 누출(숨김·바깥 닫기·modal 감지, `_leak`) / ESC 연속 참(30프레임) / `857EB0`만으로 페이지 복귀 안 될 수 있음(`vt+200(1)`) / 기록 직접 쓰기 반영 확인(`_refresh`) / radio 자체 상태 변경(매 프레임 맞춤) / 전법 종류 구간(131~135 보유자 각 20명, `_catalog`) / 글꼴에 `.`·숫자 없으면 「1위」 / 예전 DLL 롤백 시 v2 거부 후 v1로 덮어써 플래그 소실 / 판 A/B 너비 오판 / 원작 함수는 훅·클릭 콜백(UI 스레드)에서만.

## 11. 예비안 (페이지 없음)
F1 네이티브 직접: `alloc(0x198)+84CF00+vt+0x188(x,y,w,h,main table[3],3)`, 메인 표 `930740` 등록은 노드 범위 안 숙주 노드(예 0x40 `Null_guide_kaijo`) id. F2 오버레이(두 네이티브 실패 시만): 백버퍼에서 원작 hanyou_defo 버튼을 잡아 9분할, GDI Noto Serif KR, 확신도 모양 약 70%·글꼴 약 50%.

## 12. local15 실패 분석 (2026-11-07, 정적 근거 `research/ghidra_full/1a4a447d14e68c40/c/`, 주소는 새 빌드)
증상 → 원인 → 근거 (함수는 파일의 `// ==== FUNC` 머리 기준).

### 12.1 편성 기록(+0x4C0) 구조 (task a) — 적용 루프가 한 건도 못 잡은 원인
- 기록 = 0x94바이트이고 **기록이 곧 출발 명령 구조체다**. 맨 앞에 4바이트 머리말이 없다. 사람 번호가 `rec[0]`이다. local15는 `rec+4`를 명령으로 읽어 `rec[1]`을 사람 번호로 봐서 필터에서 전부 탈락했다(`army=2`만 찍히고 `record/applied` 없음).
  - `007F.c` FUN_1407f0670(기록 삭제): `iVar2 = **(int **)(list[idx])`(+0이 사람 번호), `thunk_FUN_1403f9170(rec,0x94)`로 해제, 다시 찾을 때 `iVar2 == *(int *)*puVar8`.
  - `007E.c` 출발 확인 직전 목록 만들기(10040~10062): `*(int *)*puVar5 < 0x1771`이면 `DAT_142780f60+0x158+id*8`(인물 표, 우리 `table(0x158,id,6000)`) → +0이 인물.
  - `007C.c` FUN_1407c71e0(기록 후처리): `param_1[0]` 인물(0x158 표), `param_1[2]` 병력, `param_1[3]` 진형(`0x77428` 표, <0x15).
  - `001E.c` FUN_1401edc30(출발 실행, 우리 `depart` 훅의 원래 함수): `*param_1 < 0x1771` 인물, 같은 구조체. 출발 목록(+0x540)의 항목은 `item+4`가 이 구조체다(`007E.c` FUN_1407ee130 `FUN_1401edc30(*(list+..)+4,1)`). 기록 목록과 출발 목록은 다른 구조체이고 `+4`는 출발 목록 항목에만 해당한다. 예전 계획의 `cmd=rec+4`는 이 둘을 섞은 것이었다.
  - 새 기록 생성(`007E.c` 7376~7400, `FUN_1403f90e0(0x94)` 후 기본값 채움): `+0x14..+0x1C`는 0(전법 3칸), `+0x20..`은 0xbd10(없음 값).
- 최종 오프셋: 인물 +0, 병력 +8, 진형 +0xC, 전법 +0x14/+0x18/+0x1C(dword 3개). 코드는 `cmd=row`로 읽는다. 전법 칸 위치는 `1EDC30`/기본값 채움과 기존 `depart` 로그(`tactic_priority_recorded`)가 쓰는 `cmd[5..7]`과 일치한다는 정적 근거까지만 확인했고, 실게임 확인은 `tactic_priority_row_fields` 로그로 한다.

### 12.2 현재 선택 부대 (task b) — 토글이 한 번만 동기화되고 사람 0이던 원인
- `+0x4E4`는 부대 목록 컨트롤(+0x480)의 선택 번호다: `007F.c` FUN_1407f04d0 `*(int *)(state+0x4e4) = *(int *)(*(state+0x480)+0x80)`. 같은 함수가 `+0x488`(탭 그룹)`+0x148`에 쓰는 값은 전역 핸들 표(`DAT_1427d4140[id]`)에서 꺼낸 값인데, FUN_1407f0670이 목록 컨트롤에 기록 포인터를 핸들 풀로 넣고(`FUN_140018d50 ... *puVar7 = uVar3`) 같은 함수 끝에서 `+0x148 = list[+0x4E4]`(포인터 그대로)를 쓴다. 즉 `+0x148` = `list[+0x4E4]` = 현재 기록 포인터이고 포인터는 맞았다. 사람 번호를 `rec+4`로 읽어 0이 나온 것이 토글 `person=0 want=2`의 원인이다.
- 코드: `current_record` = `list[+0x4E4]`(범위 안일 때), 아니면 `+0x148`이 목록 안에 있을 때만.

### 12.3 값을 써야 하는 곳 (task c)
- `007E.c` FUN_1407ea2d0: `+0x490` 페이지의 `+0x178..+0x20B`(0x94바이트)를 `record[+0x4E4]`로 통째로 복사한 뒤 `874EF0`, `7F3AB0`, `7FC2D0`(화면 갱신)을 부른다. 호출자는 직접 호출이 없다(`calls.tsv`에 호출 행 없음, 페이지 편집 이벤트의 함수 포인터로 추정). 그러므로 **페이지 편집 버퍼가 화면의 살아 있는 원본**이고, 기록에만 쓰면 사용자가 진형·전법을 만질 때 버퍼가 덮어쓴다.
- 코드: 현재 선택 기록이면 기록과 편집 버퍼(`page+0x178`, 첫 dword가 같은 인물일 때만) 둘 다 쓰고 기존 `refresh_tabs`(탭 재선택)로 페이지가 다시 읽게 한다.

### 12.4 팝업이 안 열린 원인 (task d)
- 가장 유력한 원인(정적으로 확정하지는 못함, 진단 로그로 확인): `open_popup`이 `pointer(state,0x4B0)` 비어 있지 않음으로 거절했다. `+0x4B0`는 모달 표시가 아니라 **화면이 들고 있는 확인 대화 객체**다. `007D.c` FUN_1407dc200(떠남)이 비어 있지 않으면 `vt+0x40`, `vt+0x28`, 소멸자로 파괴한다. 출발 확인에서만 `007E.c` 10074~10080(`vt+0x200`으로 내용 설정, `vt+0x188(…,1)`로 보임)에 쓰인다. 원작 갱신 `007E.c` FUN_1407e18e0은 `+0x4B0 != 0 && FUN_140870ed0()`(= `870B90`: 싱글턴 층 목록에 그 레이아웃이 활성으로 있는지)일 때만 입력 처리를 건너뛴다. `!= 0`만으로 모달로 보면 항상 거부한다.
- 코드: `+0x4B0`는 열기 조건에서 뺐다. 로그 `tactic_priority_state_fields/state_flags`와 `open_skipped reason`으로 확인한다.

### 12.5 토글이 안 보인 원인 (task e)
- 원작은 새 레이아웃을 만든 뒤 항상 **`vt+0x38`** 을 부른다: 진입 `007D.c` FUN_1407ddd00(컨트롤마다 `(**(code **)(*(longlong *)param_1[0x95] + 0x38))()`), `006F.c` FUN_1406f3dc0. `vt+0x38` = `FUN_140869210`(`0086.c`): 표 초기화 `92D3B0`, `92D330(table,0/1/2)`, **`FUN_140869120(싱글턴, 레이아웃)`(UI 관리자 층 목록 등록)**. 짝은 `vt+0x40`=`FUN_14086bbd0`→`FUN_14086b600`(층 목록에서 제거). 우리 팝업·토글 창은 `870410`+컨트롤만 만들고 `vt+0x38`을 한 번도 부르지 않아 관리자 목록에 없었다(그려지지도 갱신되지도 않음). `native_ccz_title.inl`은 부모 표에 `92E0B0`/`866230`(= `8685C0`)로 붙여 부모 쪽 경로로 보인다. 우리는 부모 표에 빈 노드가 없어 `vt+0x38`을 직접 부른다.
- 부모 인자: `870410`→`86E590`은 `FUN_140876940(pane,parent)`로 부모 포인터를 넣고 `parent vt+0xD8(parent,pane)`(`868E00`, 부모 풀에 등록), `pane vt+0x70(parent+0x14)`로 층을 맞춘다. 렌더 노드 `871B70`은 `+0xA0==0`이면 전역 루트 밑에 만든다 → 위치는 화면 좌표이고 부모 표 연결은 필요 없다.
- 스레드: 클릭은 20048(입력), 갱신은 처음 20048 이후 44116(60fps). 갱신 슬롯 `145B8D8`을 어느 스레드가 부르는지는 정적으로 확정하지 못했다. 로딩 스레드에서 레이아웃을 만드는 것은 원작에도 있다(`HISTORY.md` 109줄: 생성 스레드 여러 개). 생성 스레드는 `tactic_priority_toggle_vis`(0x100|activate결과, alpha, GetCurrentThreadId)로 기록한다.

### 12.6 원작 소멸 순서 (task 3)
- 원작이 자기 소유 레이아웃을 없애는 순서(`007D.c` FUN_1407dc200 떠남, `0093.c` FUN_140933ba0): `vt+0x40(obj)` → `vt+0x28(obj,next)` → `vt[0](obj,1)`. `next`는 떠남의 두 번째 인자(`param_2`).
  - `vt+0x40` = `86BBD0`: `86B600`(싱글턴 층 목록에서 제거, 포커스 슬롯 +0x170/+0x178 비움) + `92E660`(표 정지).
  - `vt+0x28` = `87A690`: `933BA0(obj+0x130)`(표가 가진 컨트롤을 각각 `vt+0x28`, `vt[0](ctrl,1)`로 소멸) + `879E50`(→`87A6D0`: 부모의 `vt+0xE0`(`86B300`, 부모 풀에서 자신 제거) 호출 후 자식들의 부모를 0으로).
  - `vt[0](obj,1)` = `428DE0`: `8678B0`(~CUILayout) 후 `3F9170(obj,0x178)` 해제.
  - 등록 `vt+0xD8`=`868E00`(풀에 추가), 짝 `vt+0xE0`=`86B300`(풀에서 제거). 부모 소멸(`87A6D0`)은 자식을 해제하지 않고 고아로만 만든다 → 자식은 부모보다 먼저 위 순서로 없애야 한다.
- 구현: 떠남 훅이 원래 떠남보다 먼저 Layout `ui_release("tactic_priority",next)`를 부른다(Layout이 이 순서로 모든 창을 소멸하고 컨트롤 추적을 정리). `popup_orphan`은 없어졌고 `tactic_priority_popup_destroyed(released,popupBuilt,toggleBuilt)`를 남긴다. 푸터 버튼은 푸터(+0x4A0)가 원작 떠남에서 소멸시킨다.

### 12.7 UI 소유 구조 (task 4, 사용자 2026-11-07)
- UI 진입점(우측 하단 버튼 — 2026-11-22 전에는 푸터 버튼, 팝업 창·탭·칸·버튼, 「출진 기억」 토글)의 생성·위치·보임·소멸은 SAN14_Layout.dll이 맡는다(`layout_api.h`의 `ui_begin/ui_end/ui_create_window/ui_activate/ui_node/ui_set_node_text/ui_set_node_visible/ui_select/ui_sync_visibility/ui_release`와 기존 `clone_control/set_text/bind_click/set_visible`). 기능 모듈(`native_tactic_priority*.inl`)은 서술자(데이터)·글자·클릭이 하는 일·적용 루프만 갖는다.
- 연결 고리 = 기능 스위치: `ui_begin("tactic_priority")`는 그 owner의 레지스트리 훅이 INSTALLED/CHAINED일 때만 성공한다. 기능이 꺼졌거나 게시되지 않았거나 거절·양보되면 UI는 만들어지지 않고 원작 화면이 그대로다. 새 설정 키 없음.
- 남은 것(기능 모듈에 둔 이유): 훅 자체(갱신·떠남 vt, 출발 CALL; 푸터 버튼용 CALL 7DC20D/7DE5BD는 2026-11-22 삭제)는 `layout_registry.h` 행의 owner가 `tactic_priority`이고 한 훅 한 소유자 규칙 때문에 몸체가 이 모듈에 있다. 갱신·떠남 훅이 UI 호출 시점을 정한다(Layout 이벤트가 아니라 게임이 부르는 시점). 부모 화면 객체의 필드 읽기(`+0x470` 등)와 원본 페이지·탭 숨김은 Layout의 `ui_show/ui_node_visible`을 통해서만 한다. 원작 `refresh_tabs`(탭 재선택, 기록 읽기 갱신)는 UI 객체 생성·소멸이 아니라 원작 상태 갱신이라 기능 모듈에 남겼다.
