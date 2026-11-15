# 원작 성 공략 단계 보정(siege_stage) 인수인계 — 2026-11-10

이 문서 하나로 다음 세션이 구현을 이어받을 수 있게 썼다. 근거 설계는 `docs/COMBAT_REDESIGN.md` 9절, 원작 사실은 `San14Sim/docs/orig/ai/roles.md` 6-1절이다.

## 0. 먼저 지킬 규칙 (사용자 지시, 모두 유지)

- 한국어로 답한다. 보고에는 항상 **"미서명"**과 실게임 확인 여부를 적는다.
- 이 작업은 Claude가 직접 코딩·검토·빌드·설치한다. DeepSeek·Codex·서브에이전트·다른 세션으로 나누지 않는다(사용자가 따로 지시하면 예외).
- Computer Use를 쓰지 않는다. 실행 중인 게임 메모리를 외부 도구로 쓰지 않는다. 게임 EXE에 REA를 쓰지 않는다.
  - 원작 조사는 `San14Sim/docs/orig/`(KB)를 먼저 보고, 그다음 `python -X utf8 tools/ghidra_full.py --limit 3000 func <RVA>`·`callers <RVA>`를 쓴다.
  - 새로 확정한 사실은 KB의 해당 절에 추가한다(새 문서를 만들지 않는다).
- **"모드가 꺼지면 반드시 원작으로(또는 먼저 설치된 다른 모드 상태로) 되돌아가야 한다."**
  - 새 훅은 키가 켜졌을 때만 게시한다.
  - 끄거나 DLL을 내리면 CALL·포인터를 원래 값으로 돌린다.
  - 세이브에 아무것도 남기지 않는다.
- **"플레이어 위임군단과 플레이어 직할 군대는 컴퓨터 규칙에서 반드시 뺀다."** 대상 판정은 `computer_side_army(army)` 하나로 한다(= `computer_army`, 플레이어 세력 전체 제외).
- Git 작성자는 `t3ferii <188655834+t3ferii@users.noreply.github.com>`다. 커밋 끝에 `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`를 붙인다. GitHub에는 사용자 지시 없이 올리지 않는다.
- 설치는 게임이 꺼진 상태에서 `tools/install_local.py`로만 한다(아래 6절 절차).
- 본 체크아웃 `..\AI_TacticsMod`에서 다른 세션이 커밋하지 않은 문서는 건드리지 않는다:
  - README.md, development/CURRENT.md
  - docs/CHANGES_ALL.md, docs/EXTENSIONS.md, docs/FEATURE_SWITCHES.md, docs/README.md, docs/읽어주세요.txt
  - 루트 SplitManage.ps1, 루트 FeatureSelect.ps1, AGENTS.md
- 요청표 `..\AI_TacticsMod\development\USER_REQUESTS.md`(UTF-8, BOM 없음)의 **U39** 행을 갱신한다.
- 영구 삭제는 사용자 확인을 받는다.

## 1. 작업 위치와 상태

- 워크트리 `AI_TacticsMod_wt-rules`, 브랜치 `v1571-plus`다.
- 최근 커밋:

  | 커밋 | 내용 |
  |---|---|
  | `8a2ce99` | route_hook: 사기 85 군악대 대기, 중립 방벽 돌파 (local31) |
  | `9553a1f` | combat_cut을 원작 병참 차단 노드로, `[combat] siege_native` 스위치 추가 (local32) |
  | `8707e96` | COMBAT_REDESIGN 9절: 원작 단계 보정 설계·조사 |
  | (이 문서 커밋) | 주소 id 7개 추가, `src/siege_stage_policy.h` 초안, 이 문서 |

- 게임에 설치된 판은 **local32**(미서명)다. 실게임 확인은 아직 하지 않았다.
- 사용자 INI `..\AI_TacticsMod\기능설정.ini`(UTF-16 LE BOM)는 지금 `[groups] combat=0`(원작 데모용), `[diagnostics] ai_trace=1, ai_trace_original=1`이다. 바꾸지 않았다.

## 2. 사용자가 원하는 것 (원문 요지)

- "공성 진행의 야전작전도 괜찮은데 원작함수를 고쳐서 쓰면 안 되나… 건설도 건설함수 호출 빈도나 조건을 고쳐서 우리 모드가 지향하는 점령→건설, 공성전 건설을 구현하면 좋겠다."
- "건설 빈도를 원작보다 살짝 높이고 싶다. 투석대·요새·진·망루·함정이 유지되는지 확인이 필요하다."
- 앞서 정한 공성 편성 **의도**를 유지한다(구성 그대로가 아니라 의도):
  - 관문이 목표가 아니면 공성병기 출진 금지
  - 어린·방원진 탱커 줄
  - 안행진은 가능하면 반드시 포함
  - 통솔이 높은 장수에게 병력을 더 많이 배분
  - 응원·악주 지원 군세가 3칸 안에서 버프를 주다가 탱커가 괴멸하면 퇴각(퇴각은 원작 판단으로 추정, 미확인)

## 3. 지금 코드에 있는 것 (이어받는 출발점)

- `[combat] siege_native`(임무 일관성 v2 아래, 기본 0)가 이미 있다.
  - 판정 함수: `src/native_campaign.inl`의 `siege_native_member(army,p)`.
  - 효과(`src/native_ai_nodes.inl`):
    - `apply_priority`: `campaign_siege_priority`(모드 야전 작전)와 `operation_hold`를 건너뛴다. 끝의 `campaign_step`(도시 재명령) 전에 돌아간다.
    - `cut_node`: 야전 작전 생성을 건너뛴다.
  - 모드 공성 건설(`try_build`, 공성지 3~6칸 또는 3~10칸 링, 진·요새·투석대·군악대 1개)은 **아직 그대로 돈다**.
  - 키 등록 위치: native_adapter.cpp(`siege_native`), module_flags.h, module_registry.json flags, distribution/FeatureSelect.ps1, 루트 FeatureSelect.ps1(워크트리), installer/InstallerGui.cs, development/installer_map.json. 생성기: `gen_installer_catalog.py --check`, `gen_module_manifests.py --apply`, `gen_module_table.py`.
- `src/native_route_hook.inl`(local31)은 같은 방식의 본보기다. 원작 CALL을 감싸 원작 실행 뒤에 보정하고, prepare/publish/restore로 게시하며, 끄면 원래대로 돌린다.
  - 이 패턴을 그대로 따르면 된다: `call_targets`, `allocate_near`, `absolute_jump`, `call_prepare`/`call_publish`/`call_restore`.
  - 연결은 `src/native_split_install.inl`에서 한다:
    - restore: `#ifdef SPLIT_COMBAT` 안의 `route_hook::restore();` 옆
    - prepare: `route_hook::prepare()` 옆(feature_off id 41)
    - publish: `route_hook::publish()` 옆
  - 모듈 등록은 `development/module_registry.json`에 `{"slot": "links", "file": "native_route_hook.inl", "hosts": ["combat"]}` 행 하나와 `development/features.json`의 combat 소스 목록이다. 그다음 `python -X utf8 tools/gen_module_table.py`를 돌린다.
- 예전 코드가 비켜서는 플래그는 `src/native_ai_core.inl`에 있다: `route_hook_morale`, `route_hook_breach`, `route_hook_owns`. 새 기능도 같은 자리에 `static bool siege_stage_on;`을 두면 된다.

## 4. 이번 세션에서 확정한 원작 사실 (디컴파일, 현재 Steam 빌드 1a4a447d)

### 4-1. 호출 위치·주소 (전부 `game_addresses_data.inl`에 id 있음, check_addresses 통과)

- 61F10(CAttackFootholdRelayAreaAttackNode Execute, 노드 종류 `0x40182782`)에는 직접 CALL이 없다.
  - vtable 칸은 **`GA_134E028`**(값 = image+61F10)이다. unpacked EXE 포인터 검색으로 확인했다. 비교로 1F380은 `GA_1346C98`(기존 cut_node 행)이다.
  - 서명: `uint32 61F10(Obj node, Obj ctx, Obj troops)`. cut_node와 같은 `Execute` 형식이고 반환은 1·2·3이다.
- 노드 상태: `B27D70(ctx,node)` → 상태 구조(`GA_B27D70`). 61F10 첫 부분이 `plVar17 = B27D70(param_2,param_1)`이다.
- 단계 함수와 CALL 위치(모두 61F10 안, `E8` 직접 호출, 인수는 `(node, troops, ctx, state)`, 반환 void):

  | 함수 | 역할 | CALL 위치 |
  |---|---|---|
  | `72D30` | 건설 시작 | `GA_64F39` |
  | `720E0` | 이어 짓기 | `GA_636A0`, `GA_63819`, `GA_64E72` |
  | `72960` | 집결 칸 고르기 | `GA_64C0B` |
  | `704A0` | 집결 이동 | `GA_64F5E` |
  | `738A0` | 주변 적 공격 | 64F71, 6516A |
  | `73E60` | 진군 | 6429D, 64ECB, 64F98, 6508C, 6517F |
  | `CC160` | 경로 | 64F1B (route_hook이 이미 감쌈) |

- 주의: ai_trace 진단(`src/ai_trace_nodes.h`)도 같은 vtable 칸 `GA_134E028`을 감싼다. 둘이 동시에 켜졌을 때 `patch_pointer(slot, expected, replacement)`가 기대값 불일치로 실패하면, 이 기능만 끄고 feature_off로 기록한다. 순서·체인 방식은 cut_node 행이 ai_trace와 어떻게 공존하는지 보고 맞춘다.

### 4-2. 노드 상태 바이트

| 위치 | 뜻 |
|---|---|
| `+0x08` | 현재 경유 지역 객체 |
| `+0x10` | 다음 경유 지역 객체 |
| `+0x1C` | 건설 허용. 노드 인자 `+0x40`의 블랙보드 값이고 **매 실행 처음에 `B28BB0`으로 다시 읽음** |
| `+0x1D` | 주변 적 대응 허용(인자 `+0x44`) |
| `+0x1E` | 집결 허용(인자 `+0x48`) |
| `+0x1F` | 건설 단계: 0 없음, 1 72D30 예약, 2 건설 중, 3 중단(적 대응 뒤 재개), 4 실패 |
| `+0x20` (u32) | 건설 칸 |
| `+0x24` | 적 대응 단계(1·2 = 738A0) |
| `+0x28` | 적 대응 대상 무장 id |
| `+0x2C` | 집결 중(1) |
| `+0x30` (u32) | 집결 칸 |
| `노드상태[10]` | 위협 목록(정확한 의미 미확인) |

### 4-3. 61F10 마지막 분기 (우선순위 순)

1. `(+0x1F & 0xFB) == 0` 이고 `+0x24 == 0`, `+0x2C == 0`이면 `CC160`(경로).
2. 아니면 `+0x1F == 1`일 때 `72D30`.
3. 아니면 `+0x24`가 1·2일 때 `738A0`.
4. 아니면 `+0x2C == 1`일 때 `704A0`.

- 끝에서 성 공격 모드(8~10·22)이고 단계가 모두 0이면 구성원마다 `27D490`을 부른다.
- 현재 경유 지역(`*state+8`)이 없으면 다른 분기(738A0, 진군, 종료)로 간다.

### 4-4. 72D30 / 720E0 (건설)

- 72D30이 짓는 시설은 `world+0x73970` = 종류표 `world+0x73958`의 **3번 = 진** 하나다.
  - 종류 번호: 3 진, 4 요새, 5 망루, 6 투석대, 7 군악대, 9 토성, 10 석벽, 11~14 함정, 16 철쇄.
  - 투석대·요새·군악대·망루·함정은 원작 공성 단계로는 짓지 않는다. `22F6E0(정의)==4` 분기는 요새 승급형으로 추정하며 미확인이다.
- 72D30 동작:
  - 후보 칸: `BA100(현재 경유 지역, 대표 장수, 진 정의, &목록, 목표 세력, 군세 구역, 다음 지역)`.
  - 없으면 `+0x1F = 4`.
  - 있으면 구성원 전원(목록 `+0x40` 쪽에 묶인 부대 제외)에게 `2ECEA0(8)`, `2ECF00(9, 22F830(현재 경유 지역))`, `+0x46 = 종류`, `+0x42 = 칸`을 쓴다.
  - 그 뒤 `+0x1F = 2`, `+0x20 = 칸`.
  - **비용은 72D30 안에서 보지 않는다**(61F10 예약 조건에서만 본다).
- 61F10 건설 중(2) 처리:
  - `+0x20` 칸에 건물이 아직 없으면 `27A8D0(&장수목록, 칸, 진 정의)`로 다시 허가를 확인한다. 실패하면 4, 성공하면 `720E0`.
  - 건물이 있으면:
    - 자기 세력이고 짓는 중(`+0x16 & 1 == 0`)이면 `720E0`.
    - 완성이면 4로 끝낸다.
- 720E0 동작:
  - 칸에 건물이 없으면 구성원 전원에게 `2ECEA0(8)`+`2ECF00(9,칸)`을 쓰고 `+0x46 = 진`으로 **다시 쓴다**.
  - 건물이 있으면 `2ECEA0(9)`+`2ECF00(11, 건물 id)`(같이 짓기)를 쓴다.
- 원작이 `+0x1F = 1`(예약)로 만드는 조건(61F10 64944 부근). 모두 만족해야 한다:
  - `+0x1C` 참, `+0x1F == 0`, `+0x2C != 1`
  - 대표 부대(`22FC40`)가 현재 경유 지역 중심에서 **10칸 미만**(`cmp 0xB`)
  - 그 지역 건물 조건
  - 요새 정의(`world+0x73978`) 비용(`2AD6C0`) ≤ 자금(시작 도시 vtable +0x90)
  - `BA100` 후보 있음
  - 그래서 드물다(원작 데모 47순에 72D30 4회).
- `+0x1F`를 지우는 곳:
  - 적 대응이 시작될 때(2 → 3, 집결 중이면 0)
  - 위협 목록이 차 있고 전력 판정(`local_res20`, 노드 인자 `+0x4C` = 1.5와 비교)이 0일 때(0으로 두고 `73E60` 진군)
  - 4 → 0(`+0x1C`가 참일 때)
  - 예약(1)을 미리 넣으면 위협 분기 말고는 지우는 곳이 없다. 위협 분기가 지우는 것은 원작 판단으로 받아들인다.

### 4-5. 72960 / 704A0 (집결)

- 72960 호출 조건: `+0x1E`, `+0x2C == 0`, `+0x1F == 0`, 현재 경유 지역이 성·관문 지역 아님(`234CE0 == 0`), 대표 부대가 현재 경유 지역 안에 있음.
- 72960 집결 칸 고르기:
  - 대표 부대 칸에서 다음 경유 지역 중심(없으면 목표 성 `22F190`)까지 `185710`으로 길을 찾는다.
  - 현재 경유 지역을 벗어나기 직전 칸을 `+0x30`에 쓴다.
  - 대표 부대 `+0x41 == 1`이거나 칸이 없으면 `0xBD10`.
  - 칸이 유효하면 61F10이 `+0x2C = 1`로 둔다.
- 704A0:
  - 구성원마다 `2ECEA0(1)`+`2ECF00(9, +0x30)`을 쓴다.
  - 멀면 `C0390` 경로를 구해 `A3370`·`2F05A0`으로 경유점을 쓰고, 부대 `+0x48 = 0xBD10`.
- 다 모인 판정: 61F10이 구성원마다 `+0x48` 칸 타일이 유효하고 `2C89F0(…)`이면 아직 이동 중으로 본다. 이동 중인 부대가 없으면 `+0x2C = 0`이다.
  - 그래서 구성원마다 목표 칸을 다르게 줘도 다 도착해서 멈추면 집결이 끝난다(추정, 실측 필요).

## 5. 구현 계획 (설계 9-2를 코드 단위로)

새 파일 `src/native_siege_stage.inl`(combat DLL, links 슬롯, `native_route_hook.inl` 뒤)에 `namespace siege_stage`를 둔다. 게시 조건은 `mod_features.siege_native`(mission_v2 아래)다. 순수 판단은 `src/siege_stage_policy.h`(초안 작성됨, 아직 아무 곳에서도 include하지 않음)에 둔다.

1. **노드 감싸기(예약)**: `patch_pointer(GA(GA_134E028), GA(GA_61F10), &execute)`. restore는 `restore_pointer`.
   - `execute(node, ctx, troops)`: 원작 실행 **전에** `state = B27D70(ctx,node)`를 얻는다.
   - 아래가 모두 맞으면 `state[0x1F] = 1`로 둔다:
     - `state[0x1F]==0 && state[0x24]==0 && state[0x2C]==0`
     - 군세가 공격 모드(`attack_mode(word(troops,0x14))`)이고 목표(`word(troops,0x16)`, 1..61)가 적 성(`hostile_city`)
     - 대표(가장 강한 컴퓨터 측 구성원)가 `computer_side_army`
     - 대표와 목표 성 칸(`22F190(campaign_goal)`) 거리 ≤ 링 최대(siege_ring10 켜짐 10, 아니면 6) + 4
     - 이번 출진에서 이 군세·목표로 지은 적이 없음(메모리 표 `sites[troops id] = {world, goal, hex, type}`)
     - 하루 1회만 시도
   - 그다음 원작을 실행하고 결과를 그대로 돌려준다. 깊이 가드를 둔다.
2. **72D30 뒤(CALL 64F39)**: 원작 실행 뒤 `state[0x1F]==2`이고 우리 대상이면 대표 부대로 모드 시설 선택을 한 번 돌린다.
   - 임시 Progress로 `try_build`를 쓴다:
     - `Progress tmp = *observe_capture(lead);`
     - `tmp.credit = true; tmp.build_chain = false; tmp.facility = 0; tmp.build_center = 48400; tmp.build_retry_day = 0; tmp.regional_worker = false; tmp.operation_city = 0;`
   - 성공하면 `type = tmp.facility`, `hex = tmp.construction_hex`.
   - 원작이 행동 8을 준 구성원마다 `2ECEA0(8)`, `2ECF00(9,hex)`, `+0x46 = type`, `word +0x42 = hex`를 쓴다. 그리고 `dword(state+0x20) = hex`, `sites[tid]`를 기록한다.
   - 실패하면 원작의 진 건설을 그대로 두고 `sites[tid]`에 진으로 기록한다(건설 빈도는 원작 이상으로 유지).
   - `try_build`는 대표 부대에 직접 명령을 쓴다(`set_build`). 그래서 대표도 같은 명령이 되니 문제없다. 진짜 Progress는 건드리지 않는다.
3. **720E0 뒤(CALL 636A0/63819/64E72)**: 원작 실행 뒤 `state[0x1F]==2`이고 `dword(state+0x20) == sites[tid].hex`, 종류가 3이 아니며, 그 칸에 아직 건물이 없을 때(`word(tile,0x1A)==0`) 처리한다.
   - 행동 8이고 `+0x42 == hex`인 구성원의 `+0x46`을 우리 종류로 되돌린다.
   - 건물이 생기면 원작이 9/11로 같이 짓게 하므로 손대지 않는다.
4. **72960 뒤(CALL 64C0B)**: `siege_stage_policy::rally(+0x30, 목표 성 칸, 링 후보, distance, ok)`가 none이 아니면 `dword(state+0x30)`를 바꾼다.
   - ok: 타일 유효, 지형 비용 `BF1B0(233310(tile)) > 0`, 건물 없음, `!campaign_mountain`.
   - 링 후보: 성 주변 ±7칸 상자 안에서 거리 4..6인 칸.
5. **704A0 뒤(CALL 64F5E)**: `state[0x2C]==1`이면 구성원(컴퓨터 측, 지도 위, 행동 1·종류 9·대상 = +0x30)을 army id 순으로 모은다.
   - 역할은 `campaign_army_role(army)`(front/middle/support/siege)를 그대로 넘긴다.
   - `near` = `sortie::neighbor(c,d)` 6칸, `far` = 이웃의 이웃 중 c에서 거리 2인 칸이다.
   - `siege_stage_policy::slots`로 자리를 정하고, 자리가 c가 아니면 `2ECF00(army,9,slot)`만 다시 쓴다.
   - 경유점은 원작 것을 그대로 둔다(자리는 c에서 1~2칸).
6. **모드 쪽 비켜서기**: publish에 성공하면 `siege_stage_on = true`로 둔다. `apply_priority`에서 cancel_* 필터 직후에 아래를 넣어 모드 공성 건설과 원작 건설이 겹치지 않게 한다.
   ```
   if(p && siege_stage_on && siege_native_member(army,*p)) {
       if(p->facility==7 && continue_build(army,*p)) return mod_step("ap:continue_build"),true;   // route_hook 군악대
       if(p->facility && p->facility!=7) {p->facility=0;p->credit=false;p->build_chain=false;}
       return cancelled && mod_step("ap:siege_stage");
   }
   ```
7. **연결**:
   - prepare: CALL 6곳 `call_targets` 확인, 포인터 칸 값 확인, relay 페이지(`allocate_near(image+GA(GA_64F39))`)를 만든다.
     - relay +0 72D30 뒤, +32 720E0 뒤(3곳 공용), +64 72960 뒤, +96 704A0 뒤.
   - publish: CALL 6개 → 포인터 순서. 하나라도 실패하면 전부 restore하고 `mod_features.siege_native = false`, `log_event("feature_off",42,why)`.
   - restore: 포인터와 CALL을 원래대로 돌리고 표를 비우고 `siege_stage_on = false`.
   - `native_split_install.inl`에서 route_hook 세 곳 옆에 넣는다.
   - `module_registry.json`에 links 행을 추가하고, `features.json`의 combat 목록에 `src/native_siege_stage.inl`·`src/siege_stage_policy.h`를 넣고, `gen_module_table.py`를 돌린다.
8. **로그(`log_event`)**: `siege_stage_reserve`(tid, 거리, 목표), `siege_stage_build`(tid, 종류, 칸), `siege_stage_build_native`(우리 선택 실패, 진 유지), `siege_stage_type_fix`, `siege_stage_rally`(tid, 원래 칸, 새 칸), `siege_stage_slots`(tid, 인원, c), `siege_stage_installed`.
9. **설치관리자 문구**: siege_native 설명을 실제 동작에 맞춰 고친다. 4곳(installer_map.json, FeatureSelect.ps1 2개, InstallerGui.cs)을 같은 문자열로 바꾸고 `gen_installer_catalog.py --check`를 돌린다.
   - 예: "컴퓨터 세력만. 공격 군세는 원작 성 공략 단계를 따르되, 성 근처에서 공성 시설 건설을 원작보다 자주 예약하고 종류(투석대·요새·군악대·진)와 칸(성에서 3~6칸)을 모드 규칙으로 고칩니다. 집결 칸을 성에서 4~6칸으로, 구성원 자리를 탱커 앞·지원 뒤로 정합니다. 끄면 이전 판 임무 일관성."

## 6. 검사·빌드·설치 순서

1. `python -X utf8 tools/check_addresses.py`. 새 16진 상수가 걸리면 `tools/addrlib.py` 허용 목록에 이유와 함께 추가한다.
2. 픽스처: `tests/obstacle_fixture.inl`(native 경로에 포함)에 `siege_stage_policy` 시험을 추가한다.
   - rally: 10칸 밖이면 그대로, 4~6칸 안이면 그대로, 2칸이면 링으로 이동, ok 거부 칸 회피.
   - slots: front는 성 쪽 이웃, support는 뒤 2칸, 중복 없음, 막힌 칸 회피.
   - `goal`·`start` 같은 이름은 전역과 겹쳐 C4459 오류가 난다. `from`·`to`·`cut`처럼 다른 이름을 쓴다.
3. `powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_feature.ps1 -Feature campaign-revision`
4. `powershell -NoProfile -ExecutionPolicy Bypass -File tools/check_feature.ps1 -Feature dll-split -Build`(설치 전 전체 회귀). 출력을 `Select-Object -First N`으로 자르면 종료 코드가 255로 보이니 `-Last`를 쓴다.
5. `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_split.ps1 -RuntimeDiagnostics`, `tools/build_installer.ps1`.
   - 설치관리자 검사가 "game-folder PS catalog equals distribution"으로 실패하면 워크트리 루트 `FeatureSelect.ps1`을 `distribution/FeatureSelect.ps1`과 같게 복사한다.
6. 설치(게임 종료 확인 후). 본 체크아웃 `..\AI_TacticsMod`에서 한다:
   - 워크트리의 `distribution/SplitManage.ps1`, `distribution/FeatureSelect.ps1`, `installer/InstallerGui.cs`, `build/development/loader_split_diagnostic/*.dll`, `build/development/installer/*.exe`를 본 체크아웃 같은 경로로 복사한다.
   - 복사한 DLL의 수정 시각을 지금으로 바꾼다(`install_local.py`의 "Stale build" 검사: DLL 시각 ≥ 소스 시각).
   - `python -X utf8 tools\install_local.py prepare local33 --modules loader,layout,combat,convenience,ccz --gui --layout research/auto_commands/layout_patch_local17 --note "..."`
   - `python -X utf8 tools\install_local.py install local33`
   - 본 체크아웃에서 `git checkout -- distribution/SplitManage.ps1 distribution/FeatureSelect.ps1 installer/InstallerGui.cs`
   - 설치된 `..\SAN14_CombatAI.dll`의 해시가 빌드 DLL과 같은지 확인한다.
7. 커밋하고, U39 행을 갱신하고, 한국어로 보고한다(미서명, 실게임 미확인 여부, 시험 방법: 설치관리자에서 전투 그룹 + 임무 일관성 v2 + "공성 진행을 원작에 맡기기" 켜기, 로그 키 목록).

## 7. 남은 미확인·위험

- 진 이외 종류로 바꾼 칸에서 61F10이 건설 중 확인을 **진 정의**로 한다(`27A8D0(…, world+0x73970)`). 진이 불가하고 다른 종류만 가능한 칸이면 `+0x1F = 4`로 건설이 버려질 수 있다.
  - 대안 A: 그 칸을 진도 허가되는 칸으로만 고른다(`27A8D0`을 진 정의로 한 번 더 확인).
  - 대안 B: 61F10 실행 동안만 `world+0x73970`을 고른 정의로 바꿨다가 되돌린다.
  - 둘 다 실측으로 고른다. A가 더 안전하다(전역 값을 바꾸지 않음).
- 61F10이 다른 군세 처리 중에 `world+0x73970`을 읽는 다른 경로가 있는지 확인하지 않았다(대안 B를 쓸 때만 문제).
- `노드상태[10]`(위협 목록)과 `local_res20`의 정확한 의미를 모른다. 예약이 위협 분기에 지워지면 원작 판단으로 받아들인다.
- 다 모인 판정(`+0x48`, `2C89F0`)이 자리를 나눈 뒤에도 끝나는지는 실측이 필요하다. 끝나지 않으면 704A0 뒤 보정을 끄고 72960 뒤 보정만 남긴다(키 하나로 나누지 말고 로그로 판단).
- ai_trace 진단이 같은 vtable 칸을 감싸는 경우의 공존 여부를 확인해야 한다.
- 망루(5)·함정(11~14)은 지금도 공성지에서는 짓지 않는다(지역 담당 부대·건설 성향 군주만). 사용자가 공성지에도 원하면 `try_build`의 공성지 종류 필터(`native_ai_build.inl` 68행 근처, 3·4·6·7만 허용)를 바꿔야 한다. 이 결정은 사용자에게 묻는다.
- 점령 → 건설은 지금 모드 방식(`mission_route_waypoint`로 원작 목록에 경로 지역 추가 → 원작이 점령 → 모드 `try_build`)이고 바꾸지 않는다.

## 8. 이번 세션에서 확인한 관련 사실 (참고)

- combat_cut은 이제 원작 병참 차단 노드(1F380)가 계획·명령한다. 모드는 `in_cut` 동안 `C1D10` 후보만 플레이어 부대로 좁힌다(D3516 CALL은 core 링크로 항상 게시됨).
- route_hook(local31)이 감싸는 CALL: 64F1B·667C7 → CC160, 667C0 → CAA60, 44E50D → 2E6E00. siege_stage는 이것과 겹치지 않는다(다른 CALL 위치). 같은 군세에서 사기 대기·방벽 돌파 부대는 `route_hook_owns`로 구분된다. siege_stage가 그 부대를 건설·자리 배정에서 빼야 하는지 확인한다(권장: `route_hook_owns(a)`가 참이면 뺀다).
- 원작 데모(전투 그룹 끔) 기록: `trace_original_20261110_172820.csv`. 72D30 4회, 720E0 6회, 72960 4회, 704A0 17회, 738A0 10회, 73E60 144회.
