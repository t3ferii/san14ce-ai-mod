# 전법 우선순위 · 출진 기억 (tactic_priority) v2

설계 원문: `docs/TACTIC_PRIORITY_V2_PLAN.md`. 이 문서는 사용자 동작과 기술 요약. 모의 검사·파일 검사와 실게임 확인(2026-11-08 사용자: 팝업·기억·`.tactics` 재로드 복원)을 마쳤다. (v1 목록·다단계 편집기는 폐기, `native_tactic_priority_editor.inl` 삭제.)

## 1. 사용자 동작
- 설치기 `편의성`의 「출진 화면 전법 우선순위·출진 기억」(`[officer_ui] tactic_priority`, 설치기 기본 켬, 코드 기본값 꺼짐).
- 출진 편성 화면(`CStrategyUnitMakeState`) 우측 하단 「전법 우선순위」 버튼(매복 「군사제안수행」 토글 바로 아래, 1920×1080 기준 1560~1880 × 1024~1072, 토글이 없어도 같은 자리)을 누르면 오른쪽 창 자리에 그리드 팝업이 겹쳐 열린다(페이지 없음, 칸 수에 따라 5~6열, 많으면 왼쪽으로 확장).
- 위쪽 모드 탭 「무력형」「지력형」(무력 ≥ 지력인 무장은 무력형). 칸 = 「고유전법」과 공용·교역 전법. 누른 순서가 우선순위(칸에 `1.`, `2.` 표시), 다시 누르면 해제되고 뒤 순위가 당겨진다. 순위가 없는 칸은 기본 순서로 뒤에 붙는다.
- 버튼: 「초기화」(현재 모드 순위 비움), 「기본값」(기본 순서로 전부 지정), 「닫기」. ESC·다른 부대 선택·원작 대화창·화면 이탈에도 닫힌다. 바꿀 때마다 즉시 적용, 닫을 때 `기능설정.ini`에 저장.
- 「무장정보」 탭 오른쪽 「출진 기억 켬/끔」 토글(무장별, 세이브별): 켜면 그 무장은 마지막 출진 진형과 전법 3개를 되살린다(보유 전법 수와 무관, 못 쓰는 것은 건너뛰고 남는 칸은 공통 순서). 끄면 현재 선택은 그대로 두고 다음 배치부터 공통 규칙.
- 칸에 마우스를 올리면 나오는 전법 설명은 편의성 「개성·전법 툴팁 + 콜라보…」(`extensions.tooltip_collab`, trait-hover)가 켜져 있을 때만 표시된다. 그 기능이 꺼져 있어도 순위 지정·출진 기억·자동 선택은 그대로 동작한다(소프트 의존, 설치기 `Requires` 아님).
- 공통 규칙(출진 기억이 꺼진 무장): 쓸 수 있는 전법이 4개 이상일 때만 공통 순서로 3개를 자동 선택. 사용자가 직접 바꾼 선택은 존중한다.

## 2. 설정
- `[officer_ui] tactic_priority=1|0` 기능 스위치.
- `[tactic_priority] profile=v=2;martial=U,137,...;intellect=...` ASCII 2047자 이하, 값은 순위 id 목록(비어 있을 수 있음). 깨졌거나 비면 기본 순서. v1(`v=1;restore=..;` 전체 목록)은 읽어서 변환하고(`tactic_priority_profile_migrated`) `restore`는 버린다. 사용자가 저장할 때만 v2로 쓴다. 설치기는 원시값을 보존한다.

## 3. 원작 주소 (1fbe6be7 / 1a4a447d)
| 항목 | 1fbe6be7 | 1a4a447d |
|---|---|---|
| vtable `CStrategyUnitMakeState` | 1458820 | 145B8B0 |
| 칸2 leave 슬롯 → 함수 | 1458830 → 7D9E50 | 145B8C0 → 7DC200 |
| 칸5 update 슬롯 → 함수 | 1458848 → 7DF540 | 145B8D8 → 7E18E0 |
| 하단 바 텍스트 버튼 CALL(enter 안, 2026-11-22부터 훅하지 않음) | 7DC20D → 85FEF0 | 7DE5BD → 862220 |
| 출발 CALL | 7EBBC4 → 1EC890 | 7EE1C4 → 1EDC30 |
- 상태 객체(두 판 동일): +0x470 메인 레이아웃, +0x488 탭 그룹(+0x148 현재 기록, `*(*(+0x130)+0x138)+0x18` 현재 탭), +0x490/+0x498 부대정보·무장정보 페이지, +0x4A0 하단 바, +0x4B0 원작 modal, **+0x4C0..+0x4C8 편성 기록 포인터 목록**(기록 0x94바이트, `cmd=rec+4`: [0] 인물 [2] 병력 [3] 진형 [5..7] 전법), +0x540 출발 목록(출발 순간에만 유효).
- 팝업은 원작 「설명자 배열로 창 만들기」 `870410`(옛 `86E060`) 경로: 설명자 type 8 프레임(`000_cmn_base_dialog_00`), type 2 노드 → `9309C0`으로 탭(style 9/10, kind 1)·칸(style 3, kind 0). 신규 GA: `GA_86E060`, `GA_3F3C30`(취소 입력 조회 `3F6150`), `GA_85AD20`(radio 선택 `85D050`), `GA_855BC0`(탭 그룹 다시 고르기 `857EB0`).
- 게이트 근거(ghidra export `research/ghidra_full/1a4a447d14e68c40`): G1 `866FA0`은 +0x130 표를 만들지 않는다 → vtable 1342530 → `92D020(pane+0x130)` → 최종 vtable 13E4E80(`native_ccz_title.inl` 선례). G2 `868E00`(vt+0xD8)은 부모별 풀에 등록만 하고 부모 소멸자는 자식을 해제하지 않는다 → 떠날 때 숨기고 포인터만 버린다(방문마다 작은 누수, 이중 해제 방지). G3 `92DAC0`의 조건은 표가 비어 있고 개수<256이고 설명자가 있는 것뿐.
- 모든 주소는 GA 식별자로, 주소표는 `gen_signatures.py all`이 만든다(`_tactic_priority_ids()`). W8(2026-11-07)부터 이 파일들도 `GA_<1a4a447d RVA>`를 직접 쓴다(`game_addresses_compat.h` 삭제).

## 4. Layout 행 (owner `tactic_priority`, 3행; 하단 바 CALL `GA_7DE5BD` 행은 2026-11-22 버튼을 자체 창으로 옮기며 삭제)
| 지점 | 종류 | 대상 |
|---|---|---|
| GA_145B8D8 | VT | GA_7E18E0 |
| GA_145B8C0 | VT | GA_7DC200 |
| GA_7EE1C4 | CALL | GA_1EDC30 |
플래그 모두 `LF_C`. SAN14_Convenience.dll 소유, SAN14_Layout.dll 재빌드 불필요. 새 행 없음. ccz 대화창 포인터(13AC588 등)는 더 이상 쓰지 않는다(`split_pointer_enabled`는 `ccz_sprites`만).

## 5. 세이브별 기억 사이드카 v2
- 확장자 `.tactics`. 30040바이트: `"TPRM\2\0\0\0"` 8 + binding 32 + 6000인 × 5(진형 1 + 전법 3 + flags 1). flags bit0 = 출진 기억, 다른 비트는 범위 오류.
- v1(24040바이트, `"TPRM\1\0\0\0"`)은 flags 0으로 읽고(`tactic_memory_migrated`) 쓸 때는 항상 v2. 크기·magic 엇갈림 = 거부.
- 값: 진형 0..20 또는 0xFF, 전법 1..200 또는 0xFF. 하나라도 어긋나면 통째로 거부하고 기억을 비운다.
- 기록은 일반 출발 CALL에서만, `merge_record`가 flags를 보존한다. 함수 이름(`tactic_memory_record/lookup/snapshot/write_sidecar/restore_sidecar`)은 그대로, 추가 `tactic_memory_flag/set_flag`.
- 위험: 예전 DLL로 되돌리면 v2를 거부하고 v1으로 덮어써 플래그가 사라진다.

## 6. 로그 이벤트 (`log_event`, 앞에 `tactic_priority`)
- 설치: `_installed`, `_guard_rejected(2 출발/3 미해결; 1 하단 바는 2026-11-22 삭제)`, `_register_rejected`, feature_off 39.
- 버튼 자리 내력(2026-11-22): 하단 바 노드 24(다른 모드의 하단 버튼과 겹침) → 「출진부대설정」 제목 오른쪽 (440,23) 164×48(사용자 「어색하다」) → 지금 자리 320×48. 로그 이름의 `head`는 첫 이동 때 이름이다.
- 화면: `_head_built(1 성공·0 실패, x<<16|y 또는 실패 단계 1 창/2 컨트롤/3 UI 소유 불가, w<<16|h)`, `_head_rect(1 창/2 컨트롤/3 메인 레이아웃, x, y)`, `_head_vis(창 bits40, 컨트롤 bits40, activate)`, `_click`, `_state(records,mode)`, `_record(i,person,formation)`, `_profile_invalid`, `_profile_migrated`.
- 팝업 생성: `_popup_build(stage 1..6,n,ok)`(6 = 생성 µs), `_popup_anchor(src,x,y)`, `_popup_rect(x,y,w<<16|h)`, `_popup_vis(대상,alpha,bits40)`, `_popup_broken`, `_popup_orphan`.
- 팝업 조작: `_popup_open(mode,ranked)`, `_popup_close(reason 1 닫기/2 ESC/3 modal/4 다른 부대/5 이탈)`, `_cancel_frames`, `_leak(1 닫힌 팝업 클릭/2 원작 탭 노출/3 modal)`, `_page_restore`, `_tile(id,rank,mode)`, `_mode`, `_tiles`, `_saved(generation)`, `_save_failed`, `_refresh(kind 1 적용/2 팝업 닫음,tab)`.
- 토글: `_toggle_built(ok,x,y)`, `_toggle(person,on)`, `_toggle_sync(person,state)`.
- 선택: `_applied(person,formation,eligible)`, `_chosen(t0,t1,t2)`, `_kept`, `_overwritten`, `_restore(person,formation,1 진형/2 전법)`, `_restore_skipped(1 기록 없음/2 진형 불가/3 공성 병력/4 전법 대체)`, `_recorded`, `_catalog(id,kind,holders)`, `_perf(frames,avg_ns,max_ns)` 1800프레임마다.
- 기억 파일: `tactic_memory_save_written`, `_save_write_failed`, `_load_restored(version,count)`, `_load_none`, `_load_rejected(1 크기/2 magic/3 binding/4 범위/5 읽기)`, `_migrated`, `_world_reset`, `tactic_memory_unavailable`.

## 7. 검사
`tools/check_feature.ps1 -Feature tactic-priority -Build`(두 fixture + 분할 모듈 + merit + address-table), `-Feature merit`, `-Feature address-table`. 최종 `dll-split -Build`.

## 8. 위험 (실게임 확인 기록)
- 870410 경로 첫 사용: type 8 프레임·설명자 이미지 id(0x2BA)·칸/탭 버튼 모양, 팝업 위치(판 A 1094/판 B 954)·글자 잘림. 안 보이면 `_popup_build`·`_popup_anchor`·`_popup_rect`·`_popup_vis` 로그부터.
- 클릭 누출(원작 탭·기록 클릭), ESC 연속 참(최대 30프레임 건너뜀), `857EB0`만으로 페이지가 돌아오지 않을 가능성(`_page_restore`), 기록 직접 쓰기 반영(`_refresh`).
- radio 스스로 상태 변경(매 프레임 맞춤), 토글 위치·가시성(탭 따라감).
- 전법 종류 구간(공용 1..24, 교역 25·26·136..139)은 옛 조사값이라 `_catalog`로 확인. 글꼴에 `.`·숫자가 없으면 「1위」 표기로 바꿔야 한다.
- 팝업 pane은 떠날 때 해제하지 않아 방문마다 작은 누수가 생긴다(G2).

## local17 (2026-11-07, 2026-11-08 사용자 실게임 확인: 팝업·기억·재로드 복원)
- 선택 규칙: 출진 기억 켬+기억 있음 = 기억한 진형과 정확히 기억한 전법(빈 칸 0 유지, 진형에서 못 쓰는 것만 뺌). 기억 없음/끔 = 무력형/지력형 순위 중 쓸 수 있는 것만 순위 순서로 최대 3개, 미지정 추가 없음, 순위가 비었거나 쓸 수 있는 것이 없으면 원작 선택 유지(`tactic_priority_kept` reason 1/2/4).
- 칸 하나 누를 때마다 즉시 반영(`tactic_priority_live_apply(changed,skipped_remember)`), 「적용」 버튼 없음. 「출진 기억」은 기록에 무장이 없으면 비활성: 입력 비트(+0x40 bit 4)를 끄면 원작 CCmnButton 갱신(860830)이 비활성 상태로 그린다(`layout_ui_enable`, `tactic_priority_toggle_enable`).
- 팝업 높이는 격자에 맞춰 늘어난다(`tactic_priority_grid_overflow` = 격자 높이, 칸 수, 프레임 높이). 힌트는 노드 폭에 맞춰 줄인다.
- ESC: 원작 취소 처리는 갱신 슬롯(7E18E0) 하나뿐(vtable 145B8B0 슬롯 5). 팝업이 열린 프레임은 원래 갱신을 건너뛰고 닫은 뒤에도 누르고 있는 동안(최대 30프레임)+1프레임 건너뛴다(`tactic_priority_cancel_frames`).
