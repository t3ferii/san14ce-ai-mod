# 중립건물 나이·대상 횟수와 공성병기 출진 (구현 지시서)

상태(2026-11-22): **구현됨(v1571-plus 다음 local, 실게임 미확인).** 모의 검사(`tests/obstacle_fixture.inl` `wall_age_policy_tests`, save_host 9행)와 분할 DLL 컴파일만 통과했다. 사양과 달라진 점·한계는 9절. 설계는 2026-11-21 클라우드 세션에서 사용자와 정했다.

이 문서의 함수 위치는 공개 저장소 `b22e86f` 기준이다. 로컬 작업 트리가 더 새로우면 **함수 이름으로** 다시 찾는다. 줄 번호는 믿지 않는다.

## 0. 사용자 요청과 결정 (원문)

요청(2026-11-21):

> 우리 중립건물 파괴 코드가 1. 피격대응 2. 최단경로 우회코스트 3. 인접지역 5번째출진부대 강제공성. 이중에서 2/3 을 조금 고치고 싶어. 판단은 지금 좋거든? 그런데 최종 중립건물 선택할때 6헥스 이내의 target / aging 을 고려해서 공격했으면 해.
> 중립건물의 id 같은거에 우리 모드로 타겟된 횟수를 기억하고 건설되고 경과된 순을 기록하는거야 (세이브 파일에 저장). 순 - 타겟횟수 값을 봐서 가장 큰 값을 가진 중립건물을 목표로 지정해서 그걸 공격하는거지 그리고 타겟횟수 업데이트 (타겟횟수 업데이트가 적용되서 이미 그 건물을 목표한 부대가 다른 중립건물을 공격해선안돼 - 다음 파괴명령부터 반영)
> 또 저돌이나 적근접시 공격을 가진 부대가 중립건물을 공격하고 있으면 바다위에서 갈팡질팡 바보가 되는일이 생겨. 중립건물 공격하는동안 발생하는 이문제를 해결해줘.
> 또 aging이 36순(3년)이 넘는 중립건물이 있으면 충차/투석기가 출진해서 해당 aging 부쉈으면 좋겠어 (지금은 관문 목표일때만 공성병기 출진함 - 우리모드가 원작과 분리되는 요격/조타함수 생성했듯이 전선이 아닌도시에서 출진가능한 충차/투석대가 한번에 여러도시(전선에 가까운도시지만 전선이 아니어서 수송만 하던도시)에서 최대 10부대 출진해서 부쉈으면 해)

결정(2026-11-21, 제안에 대한 답):

1. 6칸 기준점 = **원래 판단이 고른 중립건물**.
2. 기록 전부터 있던 건물의 나이 = **처음 본 날부터 0순**.
3. 10부대 상한 = **세력 전체 동시 10부대**.
4. **철쇄 포함**, 부대당 **3,000명**.
5. "출발도시 30칸은 아쉬워. **전선도시에 인접한 성에서 전선이 아니면 그냥 출발시켜, 멀어도**." → 출발 성에 거리 제한 없음.

## 1. 지금 코드 (바꿀 자리)

| 경로 | 함수 | 지금 고르는 방식 | 이번 변경 |
|---|---|---|---|
| 1 피격대응 | `route_hook::hit_check` (`src/native_route_hook.inl`) | 쏜 쪽까지 벽 없는 길의 첫 중립건물 | 선택 그대로. 대상 횟수만 +1 |
| 2 최단경로 우회코스트 | `route_hook::breach_group` | 벽 없는 최단길의 첫 중립건물(`neutral_breach::breach`: 돌아가는 길 ≥ max(5칸, 1/4) 또는 길 없음) | 최종 선택을 나이·횟수로(3절) |
| 3 인접지역 5번째 부대 | `refresh_path_wall` (`src/native_obstacles.inl`) → `route_hook::assign_fifth` | 출발 성 지역·인접 지역 벽 중 2칸 안 적 없음 → 최소 우회(`wall_path::pick_waypoint`) | 최종 선택을 나이·횟수로(3절) |
| 공성 요격 | `siege_guard_hook::capable/tick/collect` (`src/native_siege_guard.inl`) | 지력 80 이상 공성 군세 부대에 플레이어 부대가 6~12칸 안이면 `+0x40 = 2`(교전) | 중립건물 공격 중인 부대 제외(4절) |
| 모드 직접 출진 | `campaign_paid_single` (`src/native_campaign_departures.inl`), `gate_intercept_tick` (`src/native_ai_nodes.inl`) | 요격·조타·수복 일꾼 출진 | 공성병기 출진 추가(5절) |

**찾아 둔 원인 후보(갈팡질팡).** `siege_guard_hook::capable()`이 빼는 부대는 예전 path_wall 잠금(`path_wall_locked/done`)뿐이다. 이 두 함수는 route_hook이 게시되면 늘 거짓이다. 그래서 route_hook이 돌파(2·3)를 맡긴 부대도 지력 80 이상이고 플레이어 부대가 가까우면 교전 옵션이 켜져 추격한다. 원작 "적 근접 시 교전"(`+0x40 == 2`, NEUTRAL_BANDITS.md: `273AC0`이 `2B76B0(부대, 3칸)`로 모아 `2903E0`이 임시 추격 목표 `+0x3C = 0x1B`를 건다)과 저돌도 같은 결과를 낸다. route_hook이 매일·군세 경로 기록 뒤 파괴 명령을 다시 쓰므로 둘이 번갈아 이긴다.

## 2. A. 중립건물 기록 (전투 DLL, `ai` 모듈)

- **대상**: 건물 표 `world+0x6DA98` id 62~3000. 종류 9 토성·10 석벽·16 철쇄, 소유 `+0x13 == 0`, 내구 `+0x14 > 0`, 칸 `+0x10 < 48400`, 완공 `+0x16 & 1`(`hostile_towers`가 쓰는 완공 비트. 근거 확인 2-①).
- **기록**: `Record {u16 hex; u8 kind; u16 count; i32 born_day;}` × 3001. 하루 한 번(route_hook `daily` 슬롯 0) 대조한다.
  - 살아 있는 중립 방벽이고 기록의 hex·kind가 같으면 그대로 둔다.
  - 다르거나 비어 있으면 `{hex, kind, count 0, born = 오늘}`로 새로 시작한다(결정 2: 기록 전부터 있던 건물도 처음 본 날부터).
  - 조건을 잃은 id(부서짐·소유 변경·다른 종류)는 비운다.
  - 오늘이 표의 마지막 대조일보다 이르면(새 게임, 사이드카 없는 옛 세이브) 먼저 전부 비운다.
- **값**:
  - 나이(순) = ⌊(오늘 − born) ÷ 10⌋. 날짜는 `game_day()`.
  - 점수 = 나이(순) − count.
- **횟수 +1**: **새 파괴 명령을 실제로 쓸 때만** 올린다.
  - 올리는 곳: `hit_check`, `breach_group`(local42 "다음 벽 즉시" 포함), `assign_fifth`, 5절 출진 배정.
  - `route_breach_reapply`, 매일 stand 재계산, 같은 부대의 명령 복구에서는 올리지 않는다.
  - 이미 배정된 부대는 자기 건물이 무너지거나 해제될 때까지 바꾸지 않는다(결정: "다음 파괴명령부터 반영").
- **순수 함수**(`src/obstacle_policy.h`에 `namespace wall_age`):
  - `reconcile(records, live list, today)`
  - `score(record, today)`
  - `pick(candidates, origin_index, today)`: 점수 최대. 같으면 원래 고른 건물, 다음은 원래 건물에서 가까운 것, 다음은 id 작은 것.
  - 게임 접근은 없다. `tests/obstacle_fixture.inl`에서 검사한다.
- **스위치**: 대조는 `wall_age_pick || wall_age_siege`일 때만 돈다. 둘 다 꺼지면 기록·횟수를 건드리지 않는다. route_hook `wanted()`에 두 키를 더한다.

### A-2. 세이브 저장 (편의 DLL 사이드카 + 전투 DLL 공유 표)

매복(`src/ambush_shared.h`, `SAN14CombatAmbush`)과 같은 방식이다. 다만 이번에는 **전투 DLL이 표의 주인이자 쓰는 쪽**이고, 편의 DLL은 저장·복원만 한다.

- **계약 헤더** `src/wall_age_shared.h`
  - core 소유로 둔다. `ambush_shared.h`처럼 `features.json` core `owns`에 넣는다.
  - 내용: `struct WallAgeTable {unsigned size, version; SRWLOCK lock; volatile LONG epoch; LONG last_day; Record rec[3001];}`, `kWallAgeVersion = 1`, `using GetWallAgeTable = WallAgeTable *(__cdecl *)(unsigned)`.
- **전투 DLL 내보내기**: `extern "C" __declspec(dllexport) WallAgeTable *__cdecl SAN14CombatWallAge(unsigned version)`. 버전이 다르거나 두 키가 다 꺼지면 nullptr을 돌려준다. 대조·횟수 변경은 `lock`을 잡고 한다.
- **편의 DLL 플러그인**: 새 convenience 모듈(예: `wall-age-save`, `src/native_wall_age_save.inl`). `save_host::SidecarPlugin {"wallage", L"wallage", 1, ...}`, 순서 90.
  - `active`: 내보내기 표가 있을 때만.
  - `snapshot`: lock 아래에서 살아 있는 기록만 복사.
  - `write`: `"WAGE"` + 버전 1 + 패딩 3 + 결속 32바이트(매복 사이드카와 같은 머리) + `last_day` + 개수 + 기록(id u16, hex u16, kind u8, pad u8, count u16, born i32).
  - `begin_load`: lock 아래에서 표를 비우고 `epoch++`.
  - `restore`: 결속이 일치할 때만 채우고 `last_day`도 복원한다(복원 직후 2절의 '날짜가 거꾸로면 비움'에 걸리지 않게).
- **플러그인 칸**: `merit_sidecars`는 `save_host::Plugins<8>`(`src/native_merit_save.inl`)인데 이미 8개(10~80)가 차 있다. `Plugins<12>`로 늘린다.
  - 이 파일은 공용 파일이다. 다른 세션이 고치는 중이면 사용자에게 순서를 묻는다.
  - 늘리기 전에 `save_host.h`의 매니페스트 기록 수와 `tests/core_services_fixture.cpp` 같은 기대값에 고정된 칸 수가 있는지 확인한다.
- **등록**: `development/module_registry.json` sidecars 행 + `tools/gen_module_table.py`. `module_table_convenience.inl`은 생성물이라 손으로 고치지 않는다.
- **편의 DLL이 없거나 꺼졌을 때**: 기록은 메모리에만 있고 게임을 다시 시작하면 처음 본 날부터 다시 센다. 모드 없이 세이브를 불러도 원작 세이브는 그대로다.

## 3. B. 경로 2·3의 최종 선택 (`[combat] wall_age_pick`)

**판단(부술지, 처음 고르는 건물)은 그대로 둔다.** 그 건물(원래 건물) 기준 6칸 안에서 점수로 최종 건물을 바꾼다(결정 1).

- **경로 2 `breach_group`**: `neutral_breach::breach(plan)`이 참이고 원래 건물 `plan.wall_hex`가 정해진 뒤 다음 순서로 고른다.
  1. 후보 = `wall_path::distance(hex, plan.wall_hex) <= 6`인 살아 있는 중립 방벽(원래 건물 포함). 원래 건물에서 가까운 순으로 최대 8개.
  2. 대체 건물(원래 건물이 아닌 것)은 **그 하나만 열었을 때 실제 지름길이 열려야** 한다. 사용자가 판단 품질을 유지하고 싶어 하므로 같은 벽줄만 남기려는 것이다.
     - 순수 함수에 `neutral_breach::search(..., only_wall)`를 더한다. `through=false`여도 `only_wall` 칸 하나만 통과시킨다.
     - 비용 `c_alt`가 `closed >= c_alt + max(5칸, c_alt/4)`(원래가 `closed == none`이면 `c_alt != none`)일 때만 통과한다.
     - 길찾기는 돌파 판단이 참일 때만, 군세당 하루 한 번 이 몇 번뿐이다.
  3. stand 조건은 지금과 같다. 망루·투석대 사거리 밖에서 칠 수 있는 구성원이 있어야 한다. `forced`(돌아갈 길 없음)는 원래 건물에서 정한 값을 그대로 쓴다.
  4. `wall_age::pick` → `gp.wall`, `next.hex`. breaker는 **최종 건물**에서 가장 가까운 구성원이다. 횟수 +1.
- **경로 3 `refresh_path_wall`**: `wall_path::pick_waypoint`가 원래 건물을 고른 뒤 다음 순서로 고른다.
  1. 후보 = 원래 건물 6칸 안의 살아 있는 중립 방벽(원래 건물 포함). 출발 성 인접 지역 목록에 없어도 된다(기준은 원래 건물 6칸).
  2. 각 후보는 같은 안전 조건을 통과해야 한다. 조건은 `wall_has_stand(army, hex)`, 그리고 원래 건물이 "2칸 안 적 없음"이었으면 후보도 2칸 안 적 없음이다.
  3. `wall_age::pick` → `route_hook::assign_fifth(army, 최종 hex)`. 성공하면 횟수 +1.
- **로그**(진단판): `wall_age_pick`(값 = 부대 또는 군세 id, 칸 = 최종 id | 원래 id << 16, 셋째 = 경로(2/3) | 후보 수 << 4 | 최종 점수+512 << 12)

## 4. C. 공격 중 추격 억제 (`[combat] wall_hold_chase`)

대상은 **우리 모드가 중립 방벽 공격을 맡긴 부대**다.
- route_hook 항목 kind `breach`(1·2·3 경로)
- 5절 공성병기 부대
- route_hook이 없을 때의 예전 경로: `Progress::destroy_reason` 1~3이고 현재 명령이 6/11, 대상이 중립 방벽

할 일:
1. `siege_guard_hook::capable()`에서 위 부대를 뺀다. 지금 `capable`은 `rebuild()`에서 하루 한 번만 돈다. 그날 중간에 배정된 부대도 빠지도록 `tick()`에서도 한 번 더 본다.
2. 훅 `collect`(CALL `27402F` → `2B76B0`, 원작 유일 호출, 이미 siege_guard 소유)에서 위 부대는 추격 후보를 비워 돌려준다. 원작을 radius 0으로 부르거나 0을 돌려주는데, 2-④ 근거를 보고 고른다.
   - 훅 하나에 소유자 하나이므로 새 훅을 만들지 않고 이 함수에 넣는다.
   - 지금 `prepare()`는 `siege_guard`가 꺼지면 CALL을 걸지 않는다. `siege_guard || wall_hold_chase`일 때 걸게 바꾼다. 이때 `tick` 쪽 guard 동작은 `siege_guard`일 때만 해야 한다.
3. 붙어 있는 적과의 원작 자동 교전(전투 반경)은 건드리지 않는다. 추격 목표만 막는다.
4. **저돌**: 2-③ 근거를 확인한다.
   - 같은 `27402F` 경로로 들어오면 2번만으로 끝난다.
   - 다른 호출이면 그 CALL도 siege_guard 네임스페이스에서 같은 방식으로 감싼다. 원래 바이트 가드와 `hook_sites.json` 행을 둔다.
   - 감쌀 수 없으면 구현하지 말고 근거와 함께 보고한다.
- **로그**: `wall_chase_hold`(부대, 벽 id, 원래 반경). 부대당 하루 1줄.

## 5. D. 3년(108순) 넘은 중립건물: 공성병기 출진 (`[combat] wall_age_siege`)

요격·조타 출진처럼 모드가 직접 내보낸다. 원작 관문 전용 공성병기 출진 규칙(`sortie::siege`, `sortie_dispatch`의 성 목표 진형 교체)은 바꾸지 않는다. 이 출진은 목표가 성이 아니므로 그 교체에 걸리지 않는다. 그래도 2-②에서 확인한다.

- **세력**: 컴퓨터 세력만 한다. 플레이어 직할·위임 군단은 제외한다(`computer_side_force`). 세력당 하루 한 번, `gate_intercept_tick(own)`을 부르는 같은 자리에서 `wall_siege_tick(own)`을 부른다. 그 안에서 상순(그 달 1~10일)이 아니면 바로 돌아간다(2026-11-22).
- **대상 건물**:
  - 나이 **108순 초과**(⌊(오늘 − born)/10⌋ > 108, 3년)인 토성·석벽·**철쇄**(결정 4). 첫 판(local295)은 사양 문구 「36순(3년)」의 36을 써서 실제로는 1년이었다. 사용자 2026-11-22 「3년 유지」로 108순.
  - 그 건물 지역(`campaign_area_id(hex)`)이나 그 지역의 8 이웃 중 하나가 이 세력 성의 지역이어야 한다(`wall_path::serves`, `WallAreaTable`).
  - 망루·투석대 사거리 밖에서 칠 칸이 없으면(`tower_reach::stand`가 없음) 대상에서 뺀다.
- **출발 성**(결정 5):
  - 이 세력의 성 1~51 중 `front_city(id, own)`이 거짓이다.
  - 그 성 지역 중 하나가 이 세력의 **전선 성**(`front_city`가 참) 지역과 맞닿아 있다(지역 이웃 `+0x56[8]`, 마스크 `+0x66`, 이웃 지역 `+0x35` = 그 성).
  - **거리 제한은 없다.**
  - 출진 뒤 수비 6,000명 이상이 남아야 한다(`dword(city,0x3C) >= 9000`). `campaign::available`의 예비 규칙도 그대로 지킨다.
- **장수와 부대**:
  - 놀고 있는 장수(`campaign_paid_single`의 기존 후보 규칙)를 쓴다.
  - 진형은 `233CF0(장수, 9)`이면 충차(9), 아니면 `233CF0(장수, 10)`이면 투석(10)이다. 둘 다 없으면 다음 장수로 넘어간다.
  - 병력은 정확히 **3,000명**이다(결정 4). 철쇄는 물 위라 원작 수상 진형(`cmd[4]`)으로 탄다.
- **수**:
  - 세력 전체에서 동시에 나가 있는 이 부대가 **10 미만**일 때만 보낸다(결정 3).
  - **매달 상순에만, 세력 전체에서 2부대**(사용자 2026-11-22 「상순마다 2부대」 → 「군주 세력 전체 2부대」). 2부대는 서로 다른 성에서 1부대씩 나간다(성 하나당 한 달 1부대). 상순 첫날 못 보내면(병력·장수·길·금 부족) 상순 안에서 다음 날 다시 해 보고, 2부대가 나가면 그달은 끝이다. 상순이 지나면 모자라도 다음 달 상순을 기다린다. 날짜는 `world+0x871E8`의 `+0x37`(일), `+0x34`(연), `+0x36`(월)로 읽는다(매달 상순 교역 `trade_tick`과 같은 방식).
  - 첫 판(local295)은 매일, 성 하나당 하루 1부대씩 10부대가 찰 때까지 보내서 대상이 남아 있으면 쉬지 않고 출진했다.
  - 건물 배분은 점수 순 라운드로빈이다. 지금 붙은 이 부대 수가 가장 적은 건물에 먼저 보내고, 수가 같으면 점수가 높은 건물에 보낸다.
  - 출발 성은 건물에 가까운 순으로 고른다. 건물 하나당 상한은 따로 두지 않는다(사용자가 정하지 않음). 대상이 하나면 10부대가 모두 그곳에 간다.
- **출진 방법**: `campaign_paid_single`에 공성 모드를 더한다(예: 인자 `siege_wall = 벽 id`).
  - 일꾼 출진으로 낸다(`cmd[13]=1` 이동, `cmd[14]=9` 칸, `cmd[15]` = 벽 칸). 적 부대를 돌아가는 중계점은 수복·요격과 같이 `retake_path` → `campaign_relay_points`로 만든다.
  - 군세 종류 10(원작 군세 분기가 돌지 않음)을 쓴다.
  - `Progress`에 `wall_siege`(벽 id)와 `work_source`를 둔다. `regional_worker = true`이다.
  - `retake_tree_hook::retake_group`이 이 부대도 모드 소유로 인정하게 한다. `p.intercept`처럼 스위치를 확인하는 분기다. 그래야 원작 군세 판단표가 이 부대를 돌리지 않는다(MOD_DEV_LESSONS 「거점 수복 부대」).
- **보유**:
  - 출진 직후 route_hook 항목을 만든다. kind `breach`에 `siege` 표시를 붙이고 군세 목표 검사는 하지 않는다. stand는 `wall_stand`이고 `breach_write`로 쓴다. 대상 횟수 +1.
  - `daily`·`after`에서 다른 돌파 항목과 똑같이 다시 쓴다.
  - 그만두는 조건: 벽 무너짐, 스위치 끔, 퇴각(행동 11·12), 병력 1,000 미만, stand가 위험해짐(`forced` 아님).
  - 미션 단계(`campaign` 단계 분기에서 `p.intercept → intercept_step` 옆)에 `wall_siege_step`을 둔다. 벽이 무너지면 30칸 안의 다른 대상(위 배분 규칙)으로 다시 배정하고, 없으면 출발 성으로 귀환한다. 귀환은 요격 부대의 귀환 흐름을 재사용한다.
  - 귀환 중에는 이 부대를 10부대 수에 넣지 않는다.
- **로그**:
  - `wall_siege_departed`(세력 | 출발 성 << 8, 부대 | 진형 << 12 | 벽 id << 16, 벽 칸)
  - `wall_siege_skip`(세력, 사유: 1 대상 없음 · 2 출발 성 없음 · 3 상한 10 · 4 장수·진형 없음 · 5 병력·수비 · 6 길 없음 · 7 원작 출진 거부, 대상 수 | 출발 성 수 << 16): 세력당 하루 1줄
  - `wall_siege_retarget`, `wall_siege_home`, `wall_siege_installed`

## 6. 설정 (기능설정.ini, 같은 변경에서 `installer_map.json` + `gen_installer_catalog.py --apply`)

| 키 | 탭/묶음 | requires | 코드 기본값 | 설치기 기본값 |
|---|---|---|---|---|
| `[combat] wall_age_pick` | 전투 | `combat.path_wall` | 0 | 1 |
| `[combat] wall_hold_chase` | 전투 | `combat.obstacle` | 0 | 1 |
| `[combat] wall_age_siege` | 전투 | `combat.obstacle` | 0 | 1 |

- `load_mod_features()`: `f.wall_age_pick = f.path_wall && feature_key(...)`, 나머지는 `f.obstacle && ...`.
- 설치기 설명은 기존 항목처럼 원작/변경/효과/필요 네 줄로 쓴다.
- 기록 저장은 별도 키를 두지 않는다. 편의 DLL 플러그인은 전투 표가 있으면 동작한다(서비스 = 쓰는 기능 스위치들의 OR).
- 설치기 항목이 바뀌므로 설치는 `-Gui`로 해야 한다(MOD_DEV_LESSONS G29).

## 7. 구현 전에 확인할 원작 근거 (소유자 PC, SESSION_RULES 7절 순서, 원작 EXE에 REA 금지)

결과는 `docs/NATIVE_FUNCTIONS.md`에 기록하고, 원작 사실이면 `San14Sim/docs/orig`에 둔다. 가정과 다르면 해당 부분은 구현하지 말고 근거와 함께 사용자에게 보고한다.

1. 건물 `+0x16 & 1`이 완공 표시인가(`hostile_towers` 주석 근거 재확인).
2. 공성 진형 출진:
   - `233CF0(장수, 9/10)`이 진형 사용 가능만 뜻하는가.
   - `43A50`·`D1120`·출진 처리(`1EC890` 경로)에 충차·투석 출진을 막는 다른 조건(기술·정책·병기 수 등)이 있는가.
   - 목표 종류 9(칸) 이동 명령으로 충차·투석 부대가 나가는가.
   - 그 부대에 `2ECEA0(6)` + `2ECF00(0xB, 벽 id)`가 원작 시설 공격으로 통하는가.
3. `273AC0`의 저돌 분기: 어느 함수로 개성 저돌을 확인하고, 추격 목표를 어디서 거는가. `27402F`의 `2B76B0`을 지나는가.
4. `2B76B0`가 0을 돌려주거나 radius 0일 때 `273AC0`이 추격 목표를 걸지 않는가. 목록 객체를 비워야 하는가.
5. `+0x40` 값 0·1·2의 뜻(가능하면). 이번 설계는 `+0x40`을 쓰지 않는다.

## 8. 검사·문서·인계

- **모의 검사**(`tests/obstacle_fixture.inl`, 필요하면 새 fixture):
  - `wall_age::reconcile`: 새 건물, 같은 건물, 다른 건물로 교체, 사라짐, 날짜 역행
  - 점수와 동점 규칙
  - 6칸 후보
  - `only_wall` 길찾기(대체 건물이 지름길을 여는 경우와 못 여는 경우)
  - 공성 배분 라운드로빈과 상한 10
  - 사이드카 인코딩 왕복(결속 불일치 거절)
  - 추격 억제 대상 판정
- **경로**:
  - `tools/check_feature.ps1 -Feature ai -List` → 그 경로
  - 편의 쪽 새 모듈 경로
  - `python -X utf8 tools/run_checks.py --routes manifest,module-manifests,feature-deps --keep-going`
  - `hook-map`
  - 마지막에 `powershell -File tools/local_install.ps1 -CheckOnly -Gui`
- **문서**(같은 변경에 넣는다):
  - 이 문서 상태를 '구현됨(local 번호, 실게임 미확인)'으로 바꾼다.
  - `docs/COMBAT_REDESIGN.md` 8절에 항목을 추가한다.
  - `docs/MOD_CHANGES_VS_ORIGINAL.md`, `docs/FEATURE_SWITCHES.md`를 고친다.
  - `development/features.json`·`module_registry.json`을 고친 뒤 `tools/gen_module_table.py`를 돌린다.
- **인계**: 커밋한 뒤 `san14-handoff`의 `handoff draft:` 줄을 사용자나 설치 세션에 넘긴다. 바뀐 모듈은 combat과 convenience이고, 설치기도 바뀌므로 `-Gui`가 필요하다. 보고에는 모의 검사·빌드 결과와 실게임 미확인을 나눠 적는다.

## 9. 구현 메모 (2026-11-22, 사양과 다른 점·한계)

- **7절 원작 근거는 모두 가정과 같았다**(NATIVE_FUNCTIONS 「중립건물 나이·공성병기 출진」).
  - `+0x16 & 1`은 완공이다.
  - `233CF0(장수, 9/10)`은 진형 사용 가능 판정이다. 칸 목표 일꾼 출진으로 충차·투석이 나간다.
  - `273AC0`의 교전 분기는 `+0x40 == 2`일 때 `27402F`에서 `2B76B0(부대, 3, &목록)`을 부르고, 0보다 클 때만 `2903E0`이 추격 목표(`+0x3C = 0x1B`)를 건다. 0을 돌려주면 추격하지 않는다. 목록은 비어 있으면 된다.
  - 저돌은 별도 분기가 아니다. 출진 명령을 만들 때 개성 1(저돌)이면 교전 옵션 `cmd[17] = 2`가 된다(컴퓨터 `43A50` → `A2C60`, 플레이어 쪽 `2622A0` → `2EE640`). 그 뒤로는 같은 `27402F` 경로를 지난다. 그래서 4절 2번 하나로 저돌과 교전 옵션 모두 막는다. 모드 출진도 `43A50`을 지나므로 저돌이 그대로 붙는다.
  - 이미 걸린 추격은 `+0x25 == 0`일 때 `26E260`이 지운다(`+0x3C = 0xFF`, `+0x3E = 0`, `+0x48 = 0xBD10`, `303EA0`).
- **추격 억제 대상**: route_hook `breach` 항목이 살아 있고 그 벽이 서 있는 부대(1·2·3 경로, 공성병기 모두)다.
  - 예전 경로도 본다: `Progress::destroy_reason` 1~3이고 명령이 6/11, 대상이 서 있는 중립 방벽인 부대다. `path_wall`을 끄면 진동 판정(1)·path_wall(3)이, `hit_response`를 끄면 피격 대응(2)이 이 경로로 남는다.
  - 배정할 때 `breach_write`의 `26E260`이 이미 걸린 추격을 지운다. 매일 `daily`에서 한 번 더 지운다(`wall_chase_cleared`).
  - 억제 구간 = 모드가 그 부대에 중립 방벽 공격 명령을 준 때부터 벽이 무너지거나 명령이 끝날 때까지다. 공성병기가 성에서 벽 6칸 앞까지 행군하는 동안, 귀환하는 동안, 다음 벽으로 옮기는 동안은 공격 중이 아니므로 저돌·교전이 원작대로다. 원작 컴퓨터는 중립 방벽을 스스로 부수지 않는다.
- **공성병기 출진 순서**: 사양은 "출진 직후 항목을 만든다"였다. 구현은 아래와 같다.
  - 부대는 벽 칸이 아니라 **칠 칸**(stand, 망루·투석대 사거리 밖)을 목표로 일꾼 출진한다. 적 부대를 돌아가는 중계점을 그대로 쓴다.
  - 벽에서 6칸 안에 들어오거나 이동 명령이 끝나면 `wall_siege_step`이 breach 항목을 만든다. 출진하자마자 항목을 쓰면 출진할 때 만든 중계점이 지워지기 때문이다.
  - 대상 횟수는 출진할 때 +1 한다. 다른 벽으로 다시 배정할 때도 +1 한다. 같은 벽에 다시 쓸 때는 올리지 않는다.
  - 행군 중에 피격 대응 돌파나 사기 회복 정지(route_hook 항목)가 먼저 있거나 부대가 특수 상태(`+0x140`)이면, 다시 배정하거나 귀환하지 않고 기다린다.
- **세이브 뒤 한계**: `Progress`와 route_hook 항목은 메모리에만 있다. 불러온 뒤에는 출진해 있던 공성병기·요격 부대가 모드 소유를 잃고 원작 군세 판단을 따른다. 나이·횟수 기록(사이드카 `wallage`)만 돌아온다.
- **추가 export**: `SAN14CombatWallAge`(전투 DLL)가 공유 표를 내준다. export 목록 검사 3곳(`verify_split_exports.py`, `prepare_public_release.py`, `install_local.py`)에 넣었다.
