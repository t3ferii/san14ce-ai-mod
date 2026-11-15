# 원작 AI 출진·야전 행동 추적 (v12.15, 2026-10-20)

목적: 원작 컴퓨터 군단이 **어떤 목적으로 출진했는지**와, 야전에서 **누가 언제 행동·목표·중계를 바꾸는지**를 같은 시간축에 기록해 출진 목적과 야전 행동의 불일치 패턴을 찾는다. 사용자 요청으로 모드 편의 DLL에 통합했다(나중에 모드가 바꾼 함수도 같은 방식으로 조사).

## 켜기·끄기 (`AI_TacticsMod\기능설정.ini`, 게임 재시작 필요)

```ini
[diagnostics]
ai_trace=1            ; 0 = 끔(기본). 1 = 기록
ai_trace_original=1   ; 1 = 모드 훅을 하나도 연결하지 않고 원작 AI만 기록 (ai_trace=1일 때만)
```

- `ai_trace=1, ai_trace_original=0`: 모드 기능은 평소대로 동작하고, 모드가 이미 연결한 노드는 그 위에 한 겹 더 감싸 기록한다(기록의 `meta ... chained_mod_hook`).
- `ai_trace_original=1`: 전투 DLL도 준비·연결하지 않는다. `[groups] combat=0` 권장.
- **주의:** 원본 모드에서는 논공·명품창고 저장 보조 파일도 연결하지 않는다. 이 상태에서 기존 세이브에 덮어 저장하지 말고 별도 슬롯을 쓴다.

## 기록 방식

- 대상: 현재 EXE의 RTTI에서 찾은 행동 트리 노드 284개 전부(`src/ai_trace_nodes.h`, `tools/gen_ai_trace_nodes.py`). Execute는 vtable 16번(기존 훅 9개로 확인).
- 각 노드의 vtable 슬롯 포인터만 바꾼다. 원작 코드 바이트 변경·새 스레드·원격 호출 없음. 원래 함수에 같은 인자를 넘기고 결과를 그대로 돌려준다. 원작 호출 중에는 잠금을 잡지 않는다.
- 군사 관련 126개 노드: 호출 전후로 부대 500(world+7E838, +10..+6F)·군세 500(world+7F8D8, +10..+8F)을 비교해 바뀐 필드를 그 노드(`node`)에, 두 노드 사이에 바뀐 것은 `between`(행동 트리 밖: 일일 처리·전투·플레이어 명령)으로 기록한다. 나머지 노드는 호출 수만 센다.
- 핵심 노드 57개(출진·목적·목표·중계·퇴각·실행·경계)는 매 호출을 기록하고, `CCommandFootholdAttackDepatureNode`는 node+60의 최종 목표 도시/관문 ID도 남긴다.
- 다른 AI 스레드와 겹친 기록은 `conc=1`로 표시한다(분석 기본값은 제외).
- 파일: `AI_TacticsMod\data\ai_trace\trace_original_YYYYMMDD_HHMMSS.csv` 또는 `trace_mod_...`. 용량 제한 없음(2026-11-10). 로그가 있는 드라이브 여유가 5GiB 미만이면 `disk_low_stop` 한 줄을 남기고 멈춘다.

열: `seq,day,thread,cause,node,next,result,conc,kind,id,commander,force,hex,troops,field,old,new`. 부대 필드: commander(+12), flags(+26), relay1~5(+2C~+34), action(+37), target_kind(+38), target_id(+3A), target_hex(+42), facility(+46), troops(+58). 이동력(+14)·병력(+16)·사기(+1A)·위치(+2A)는 변화 기준에서 빼고 맥락으로만 기록한다. 이름 없는 바이트는 `u8@0xNN`. 군세는 mode(+14)·goal(+16)·`u16@`.

## 분석

```
python -X utf8 AI_TacticsMod\tools\analyze_ai_trace.py <trace.csv> --out AI_TacticsMod\data\ai_trace\report_<이름>
```

`report.md`: 출진시킨 노드, 출진 목표가 처음 바뀐 곳(목적 불일치)과 경과 일수, 필드별로 쓴 곳, 변경이 많은 부대 예시. `summary.json`: 같은 내용 전체. 행동·목표 코드 해석(4 성 공격, 7 부대 공격, 11/12 퇴각 등)은 기존 조사 기준이며 일부는 미확정이다.

## 검증 상태

`tools/check_feature.ps1 -Feature ai-trace`: 노드 표 재생성(알려진 9개 슬롯 대조), 실제 `native_ai_trace.inl`을 가짜 이미지에 올린 모의 검사 605개(인자·결과 그대로, 게임 데이터 무변경, 노드/BT 밖 귀속, 중첩 호출, 예외 전파, 모드 훅 연쇄·외부 훅 거부, 부분 게시 원복), 분석기 실행. 정식 빌드 회귀(dll-split)·ZIP 적용/복원 130개 통과. **실제 게임 실행·기록량·진행 속도 영향은 미확인.**

## 게임 용어 기록 (v12.13~, 부대상세 옵션은 v12.15)

모든 부대 줄 끝에 `context` 열: `무장|세력|병력|사기|위치|명령 목표|잠정목표|적접근시|자주퇴각|추격|사후명령`. 예: `장비|유비군|병3000|사기80|위치 소패|진군 태산|적접근시 공격`. 하루 한 번 성·관문 61곳의 소유·수비병 변화를 `city` 줄로 기록한다(첫날 전체).

실행 중 게임을 읽기 전용(PROCESS_VM_READ)으로 대조해 확인한 위치(2026-10-20, 장비·미축 부대상세 화면):
- 무장 +34 = 성명+자, 자 +2E (이름 = 앞부분) / 세력 +10 = 군주 무장
- 지역 +10 이름, 타일(world+E270) +12 지역 / +14 소유, 건물(world+6DA98, 도시1~51·관문52~61) +10 중심 칸 / +13 소유
- 부대 +3C/+3E 잠정목표(0xFF 없음), **+40 적 접근 시(1 무시, 2 공격; 창을 닫을 때 저장)**, **+26 비트 0x04 자주퇴각 / 0x08 추격**, **+41 사후명령(1 퇴각, 2 대기; 퇴각 선택 시 자주퇴각 자동 해제)**. 사용자 부대 동도나에서 설정을 하나씩 바꿔 확정(`tools/army_option_probe.py`).
- 수비병은 원작 도시 조회 32C130으로 얻은 City+3C(AI 스레드, 하루 61회)

분석 보고서에 "부대 이야기"(출진 → 변경마다 그 시점 상태 → 결말)와 "성·관문 소유 변화"가 추가됐다. 날짜는 원작 달력(연×360+월×30+일)으로 환산한다.

## 전투 DLL 결정과 합치기 (2026-10-20)

진단 빌드(`tools/build_split.ps1 -RuntimeDiagnostics`, `diagnostics.ini enabled=1`)의 `SAN14_CombatAI.log`는 줄마다 `day=`(AI 스레드가 마지막으로 읽은 게임일)를 남긴다. 부대 번호는 추적 CSV의 `id`와 같은 슬롯(22DA10)이다.

```
python -X utf8 AI_TacticsMod\tools\merge_combat_trace.py <trace_mod.csv> <SAN14_CombatAI.log> --out <폴더>
python -X utf8 AI_TacticsMod\tools\merge_combat_trace.py <trace_mod.csv> <log> --out x --story <슬롯> <출진일>
```

모드가 덮어쓴 노드(`CSetRelayHexEvadeFlameHexNode`=first_army_node, `CCutLogisticsNode`, `CExecuteFootholdOccupyNode`=city_node, `CArmySetRetreatNode`)의 변경은 같은 날 같은 부대의 모드 이벤트가 있으면 `mod:<이벤트>`로, 없으면 "원작/모드 미구분"으로 표시한다.

첫 실행(게임일 75150~77810, `data/ai_trace/run1_20261020`, 요약 `data/ai_trace/merge_20261020`): 40일 정지 퇴각 576건 중 328건은 직전 40일에 `build_skip_native_check`가 반복됐다. 점령 크레딧 유지 → 원작 건설 검사 거부 → apply_priority의 로그 없는 대기 유지가 원작 이동 명령을 10일마다 덮어썼다. 수정: 두 번 연속 거부(10일 간격)되면 크레딧·건설 연쇄 해제(`build_released_after_rejections`), 대기 유지 시 `build_hold_wait` 기록. 실게임 효과 미확인.

## 임무 장부와 모드 판단 이름표 (2026-10-20)

- 진단 빌드 전투 DLL은 `SAN14CombatTraceStep`(배포 빌드에는 없음)을 노출한다. 추적기가 노드 호출마다 그 호출 중 모드가 거친 판단 단계(`army_node:mod;ap:build`, 로그 이벤트명 포함)를 받아 node/call 행의 `next` 열에 쓴다. 비어 있으면 원작만 작성했거나 이름표 없는 경로다.
- `tools/mission_ledger.py <trace> --out <폴더>`: 군세 mode가 0이 아닌 동안을 임무 하나로 묶어 임무 종류별 첫·끝 단계, 단계 전환, 작성자, 임무 변경 작성자를 낸다. `--mission <군세ID> <시작일>`은 임무 하나의 단계·중계 기록.
- 중계: 원작 부대 생성(2D28D0)은 출진 명령 [8..12]의 다섯 칸을 2EE1E0으로 중계1~5에 등록한다. 원작만 돌린 320일 기록에서 원작 노드는 중계1 하나(현재 위치에서 중앙값 9칸)만 쓰고 최종 칸은 +42가 갖는다. 2~5칸을 연속으로 채우는 것은 주로 모드 대열 경로다. 한 턴 이동 범위(부대 마우스오버 흰 테두리) 함수는 아직 확인하지 않음.

두 번째 실행(게임일 75150~82060, `trace_mod_20261020_211105.csv`, 결과 `data/ai_trace/run2_20261020`): 40일 정지 퇴각이 1,000일당 216.5→116.4건, 그중 직전 건설 거부 반복 비율이 57%→31%(시기·길이가 달라 직접 비교는 참고용). 크레딧 해제 416건. 임무 장부(이름표 포함): 성공략 656건 중 415건이 퇴각으로 끝남. 성공략 단계 작성 원작48%·모드36%·BT밖15%, 모드 쪽은 ap:group_step·ap:individual(강제 귀환·정지 퇴각)·campaign_native_direct_target·cohesive_sortie_column·건설 순.

## 함수 호출 기록·야전 부대 일일 결과 (local30, 2026-11-10, 진단판 전용)

사용자 요청: "출진부터 야전지휘까지 관여하는 모든 원작함수에 호출기", "원인을 모르던 함수들까지 추적", "야전에 출진중인 부대들 결과를 기록". 진단판(`build_split.ps1 -RuntimeDiagnostics`)에서 `ai_trace_original=1`일 때만 켜진다. 정식판에는 코드가 없다.

- **fn_trace**: 아래 원작 함수 입구 5~9바이트를 JMP로 바꿔 감싼다(노드 슬롯 방식과 달리 **원작 코드 바이트를 프로세스 안에서 바꾼다**). 디코드한 입구 바이트(`tools/pelib.py`)가 다르면 그 함수만 건너뛰고 `meta,fn_trace,<이름>,<RVA>,0`을 남긴다. 결과 `meta ... hooked,n,전체`. 정수 인자 8개를 그대로 넘기고 반환값을 그대로 돌려준다.
- 행: `cause`=`fn_in`(진입, 결정 함수만) / `fn`(복귀), `node`=그 스레드에서 실행 중인 가장 안쪽 BT 노드, `next`=함수 이름, `result`=반환값, `field`=`args`, `old`=호출한 곳 RVA, `new`=중첩 깊이, `context`=해석한 인자 + 복귀 후 부대 상태(`army_context`, 경유, 목표칸, 행동비트) + 군세 임무·목록.
- 인자 표기: 부대N, 군세N, 지역(중심칸), 칸, 정수, `out`/결과부대, rec[13개 정수](출진 명령), 상태[현재/다음 중계 지역, 단계 바이트](61F10 상태), 노드 RTTI 이름, `x`=자동 판별(부대/군세 슬롯이면 번호, 아니면 16진).
- 명령 쓰기: 2ECEA0 행동, 2ECF00 목표, 2F05A0 중계, CF570 중계 앞에 넣기, A3370 중계 비우기, 2EC570 퇴각, A33B0/A33E0 군세 목록 비우기/한 칸 지우기.
- 결정: CC160 군세 부대 경로 배정, B7DC0 지역까지 경로, D1760 병참단절 계획, C5AC0 유리위치, CD5F0 인접거점, CED10 병참회복, C0390 경로→목록, 27D490 시설 넘김, 43A50/1EDC30 출진 명령 작성/실행.
- 기능 미확정(이번 기록으로 확정할 대상): 61F10 단계 함수 720E0·73E60·738A0·72D30·704A0·72960, 부대 노드 1F760(비트 0x20)·1FEE0(비트 0x40), 65A60 보조 CCDF0·CAA60, 일일 2E6E00.
- **field status**(매일, 그날 첫 BT 노드 직전 = 전날 명령·이동의 결과): 지도 위 모든 부대 한 줄. `old`/`new` = 어제/오늘 위치 칸, context = 무장|세력|병|사기|위치|명령|`이동 N칸`|`병력변화 ±N`|경유 지역|목표칸|행동비트·대상부대|군세 임무·목표|목록.
- 부대 상태에 +0x1F0 행동비트(`ai_bits`)·+0x1F4 대상 부대(`ai_target_army`), 군세 목록(`work_list`, +18 u16[10]) 변경 줄이 추가됐다.
- 시험: `[groups] combat=0`, `[diagnostics] ai_trace=1, ai_trace_original=1, cost_profile=0`. 세이브는 별도 슬롯. **실게임 미검증**(함수 입구 패치의 안정성 포함).