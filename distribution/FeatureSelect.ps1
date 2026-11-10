# Feature selection for AI_TacticsMod\기능설정.ini (UTF-16, read by the DLLs at game start).
# Dot-sourced by 관리.ps1. Each DLL reads [section] key, then the legacy [features] key,
# then its default, so files written by older installers keep their values.
# Items that share game code are one switch (see each note); they cannot be split.
# Registered raw settings (not 0/1 switches): edited by the game's first-menu 「조조전」 dialog.
# Missing/empty profile keeps current art, 70/90 thresholds and all existing personal formations.
$taskRawSettings = @('ccz_appearance.profile','tactic_priority.profile')
# v12.8: Requires = parent feature that must also be on (DLL ANDs it); Inherit = the switch an
# older file's missing key falls back to. Tab is the GUI installer tab (installer/InstallerGui.cs).
$taskFeatureCatalog = @(
    @{Group='convenience'; Section='real_estate'; Key='terrain_edit'; Default=1; Requires='game_flow.advisor_briefing'; Inherit=''; Tab='편의성'; Label='부동산 계획: 지형 변경'; Note='군사의 열흘 보고 창에 「부동산 개발」 버튼을 추가합니다. 보유한 성 목록에서 성을 고르면 그 성의 지역내정으로 들어가고, 땅을 클릭해 바닥·높이·나무·절벽·수면을 무료로 바꿀 수 있습니다. 일반 지역내정의 땅클릭은 원작 그대로이며, 이 버튼으로 들어왔을 때만 지형 변경이 열립니다. 끄면 버튼과 지형 변경이 모두 사라집니다.'}
    @{Group='convenience'; Section='diplomacy'; Key='diplomacy_overinvest'; Default=0; Requires=''; Inherit=''; Tab='게임성'; Retired=$true; Label='외교 군정 100% 초과 투입'; Note='컴퓨터가 시작한 친선·동맹·포로반환·공격요청 교섭에서 보유 군정까지 투입합니다. 100%를 넘긴 방어 효과는 상대의 추가 투입 후에도 유지됩니다.'}
    @{Group='convenience'; Section='diplomacy'; Key='diplomacy_counterturn'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='외교 군정 마지막 추가 대응'; Note='컴퓨터의 마지막 응답 뒤 플레이어가 한 번 더 군정을 투입할 수 있습니다. 추가 대응 후에는 바로 판정합니다.'}
    @{Group=''; Section='groups'; Key='combat'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='전투 AI 교정 전체 (SAN14_CombatAI.dll)'; Note='출진 편성·군세 이동·공성·병참·퇴각을 하나의 진행 상태로 관리합니다. 끄면 전투 DLL을 불러오지 않습니다.'}
    @{Group='combat'; Section='combat'; Key='sortie_formation'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='군세 출진 편성 (병력별 2~7부대·공성 진형·1,000명 지원)'; Note='컴퓨터가 출진할 때 병력에 맞춰 2~7부대, 공성 진형, 1,000명 지원 부대를 편성합니다. 출진 자체의 판단은 원작이며, 끄면 원작 편성을 씁니다.'}
    @{Group='combat'; Section='combat'; Key='departures'; Default=1; Requires=''; Inherit=''; Tab='전투'; Retired=$true; Label='정기 선제 출진 검토 (10일마다)'; Note='임무 일관성 v2와 충돌해 사용이 중지된 기능입니다.'}
    @{Group='combat'; Section='combat'; Key='group_advance'; Default=1; Requires=''; Inherit=''; Tab='전투'; Retired=$true; Label='집결 후 30일 정지 군세의 집단 진군'; Note='임무 일관성 v2와 충돌해 사용이 중지된 기능입니다.'}
    @{Group='combat'; Section='combat'; Key='stationary_retreat'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='같은 자리 40일 정지 부대 퇴각'; Note='같은 자리에서 4턴(40일) 동안 움직이지 못한 부대를 귀환시킵니다. 공성 중인 부대 등 예외는 원작 규칙을 따르며, 끄면 원작처럼 계속 머뭅니다.'}
    @{Group='combat'; Section='combat'; Key='morale'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='사기 회복 우선'; Note='사기가 낮은 군세가 무리하게 공격하기보다 사기 회복을 먼저 선택하게 합니다. 전투 자체의 규칙과 피해 계산은 원작 그대로입니다.'}
    @{Group='combat'; Section='combat'; Key='support'; Default=1; Requires=''; Inherit=''; Tab='전투'; Retired=$true; Label='호위·동행 지원 요청'; Note='임무 일관성 v2와 충돌해 사용이 중지된 기능입니다.'}
    @{Group='combat'; Section='combat'; Key='mission_v2'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='임무 일관성 v2 (공성 시설만·대기 없음·원작 공격 우선)'; Note='공성 목표 성 10칸 안 자기 땅에서, 성에서 3~6칸(성 공격 범위 밖) 지점에만 공성 시설 1개를 짓고 건설 대기로 세우지 않습니다. 원작이 목표 성을 공격 중이면 모드 공성 진행이 끼어들지 않고, 공성 중인 부대는 40일 정지 퇴각에서 제외합니다. 경로상 적 지역을 원작 군세 작업 목록에 넣고, 최대 병력 90%·여유 4만 이상 도시는 연결된 적 성으로 대열 출진합니다.'}
    @{Group='combat'; Section='combat'; Key='siege_stall_near'; Default=0; Requires='combat.mission_v2'; Inherit=''; Tab='전투'; Label='공성 정체: 성 1칸 안만 공성 중'; Note='컴퓨터 세력만. 성 1칸 안(또는 최근 타격)일 때만 공성 진행으로 봅니다. 끄면 이전 판.'}
    @{Group='combat'; Section='combat'; Key='siege_ring10'; Default=0; Requires='combat.mission_v2'; Inherit=''; Tab='전투'; Label='공성 시설 링 3~10칸'; Note='컴퓨터 세력만. 공성 시설을 성에서 3~10칸에 짓고, 군악대는 부대에서 6칸 안에 짓습니다. 끄면 3~6칸(이전 판).'}
    @{Group='combat'; Section='combat'; Key='builder_ruler_fill'; Default=0; Requires='combat.mission_v2'; Inherit=''; Tab='전투'; Label='건설 성향 군주(내면 건설 5)의 시설 채우기'; Note='켜면 내면 건설이 5인 컴퓨터 군주의 부대가 공성지 제한 없이 지역이 가득 찰 때까지 시설을 짓습니다(이전 동작). 끄면 다른 군주처럼 공성 목표 근처에 1개만 짓습니다. 임무 일관성 v2가 켜져 있어야 동작합니다.'}
    @{Group='combat'; Section='combat'; Key='ai_max_aggression'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='AI 호전성 최대 (궁극과 같은 공격 검토)'; Note='원작 판단표의 AI 설정 단계를 궁극(4)으로 읽게 하고, 군단 진행 검사의 단계별 턴 건너뛰기를 없앱니다. 다른 원작 판단은 그대로 둡니다. 플레이어 위임 군단에도 적용됩니다.'}
    @{Group='combat'; Section='combat'; Key='ai_wall_block'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='컴퓨터 세력 방벽 건설 금지'; Note='컴퓨터 세력이 방벽·관문을 새로 짓지 않게 합니다. 플레이어 건설과 이미 지어진 시설은 그대로이며, 끄면 원작 AI가 다시 지을 수 있습니다.'}
    @{Group='combat'; Section='combat'; Key='ai_catapult_block'; Default=0; Requires=''; Inherit=''; Tab='전투'; Label='플레이어 성 주변 컴퓨터 투석대 건설 불가'; Note='켜면 플레이어 성·관문이 있는 지역과 바로 붙은 지역(점령자 상관없음)에서 컴퓨터 세력이 투석대를 지을 수 없습니다. 그곳에서는 시정 Lv7처럼 요새가 최고 시설이 되어, 컴퓨터 부대는 요새 등 다른 시설을 짓거나 공성을 계속합니다. 그 밖의 지역, 컴퓨터끼리의 전투, 플레이어의 투석대는 원작 그대로입니다.'}
    @{Group='combat'; Section='combat'; Key='combat_cut'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='컴퓨터 군세의 플레이어 병참선 차단'; Note='모드가 지휘하는 컴퓨터 군세가 플레이어 부대만 골라 병참을 끊습니다. 끄면 이 차단 행동을 하지 않습니다.'}
    @{Group='combat'; Section='combat'; Key='hit_response'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='피격 대응 (적 성·관문 피격 시 공성 고정, 피격 부대 보호)'; Note='끄면 피격 기록을 남기지 않아 피격에 따른 공성 고정·호위 제외·원거리 피격 장애물 판단이 모두 꺼집니다.'}
    @{Group='combat'; Section='combat'; Key='obstacle'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='중립 장애물 파괴 판단'; Note='왕복 정체 경로의 중립 방벽만 파괴합니다. 플레이어 원거리 공격 경로 사유는 「피격 대응」도 켜져 있어야 하며, 끄면 원작 명령을 그대로 둡니다.'}
    @{Group='combat'; Section='combat'; Key='path_wall'; Default=0; Requires='combat.obstacle'; Inherit=''; Tab='전투'; Label='장애물 돌파(출발 도시 옆 중립 방벽)'; Note='컴퓨터 세력만. 출발 도시 옆 중립 방벽을 5번째 부대가 부숩니다. 끄면 이전 판 장애물 규칙.'}
    @{Group='combat'; Section='combat'; Key='recall_veto'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='갓 출진한 공격 군단의 회군 방지'; Note='출진한 지 10일 안이고 병력 3,000명 이상인 공격 군단을 원작 군단 AI가 바로 회군시키지 못하게 합니다. 10일이 지나면 원작 판단을 따릅니다.'}
    @{Group=''; Section='groups'; Key='convenience'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='편의·게임성 기능 전체 (SAN14_Convenience.dll)'; Note='끄면 편의성·게임성 탭과 게임렉제거(성능) 탭의 빠른 저장이 모두 꺼집니다. 논공·명품창고 저장 자료는 보존됩니다.'}
    @{Group='convenience'; Section='battle_rules'; Key='battle_rules'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='숙련 모드 공방 설정 + 인접지역 효과'; Note='숙련 설정의 값을 조정하는 기능이며 무장을 추가하지 않습니다. 같은 계산을 공유해 하나로 묶었습니다. 바꾼 세이브는 켠 채로 이어 하세요.'}
    @{Group='convenience'; Section='battle_rules'; Key='expert_ingame'; Default=0; Requires='battle_rules.battle_rules'; Inherit=''; Tab='편의성'; Label='숙련 설정 게임 중 변경'; Note='게임 중 설정 화면에서 숙련 네 줄을 바꾸고 확인으로 적용합니다. 끄면 원작(읽기 전용).'}
    @{Group='convenience'; Section='personnel'; Key='awards'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='명품창고·6슬롯·명품/관직 수여'; Note='명품창고를 6칸으로 넓히고 명품·관직 수여를 편하게 합니다. 창고에 저장된 자료는 꺼도 보존되며, 원작의 명품·관직 규칙 자체는 없애지 않습니다.'}
    @{Group='convenience'; Section='personnel'; Key='recruit'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='등용 보조 (등용 화면·자동 등용·결과 안내)'; Note='등용 화면과 자동 등용이 같은 판정을 쓰도록 묶고 결과를 안내합니다. 성공 확률과 조건은 원작 계산을 따르며, 끄면 원작 등용 화면만 씁니다.'}
    @{Group='convenience'; Section='domestic'; Key='region_officers'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='월드맵 지역담당관 초상화'; Note='월드맵 도시·지역에 담당관 초상화를 표시합니다. 공석은 빈 초상화와 회색 테두리로 보이고, 임명은 지역내정에서만 할 수 있습니다(표시 전용).'}
    @{Group='convenience'; Section='domestic'; Key='region_officer_traits'; Default=1; Requires='domestic.region_officers'; Inherit=''; Tab='편의성'; Label='지역담당관 이름 옆 내정 관련 개성 표시'; Note='지역담당관 이름 옆에 내정에 도움이 되는 개성을 표시합니다. 표시 전용이며, 위의 「월드맵 지역담당관 초상화」를 켜야 함께 보입니다.'}
    @{Group='convenience'; Section='extensions'; Key='battle_deaths'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='AI 전사 연출·게임 기록'; Note='컴퓨터끼리의 전투에서 일어난 전사를 보여 주고 기록만 합니다. 출처: 삼갤 통합패치(180.71·175.115 유동)'}
    @{Group='convenience'; Section='officer_ui'; Key='loyalty'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='숨겨진 충성도 숫자 표시'; Note='무장의 숨겨진 충성도를 숫자로 표시합니다. 원작에서 아이콘으로만 보이던 값을 숫자로 확인할 수 있고, 충성도 계산과 등용 판정은 바꾸지 않습니다(표시 전용).'}
    @{Group='convenience'; Section='officer_ui'; Key='damage_preview'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='전투력대체 예상 피해량'; Note='적 대상 선택 시 칼 등급 대신 양쪽의 1회 병사 피해를 표시합니다. 성·관문·시설의 내구 피해도 표시합니다. 현재 조건의 단일 교전 예상이며 전법·미래 상태 변화는 제외합니다. 전투 규칙은 바꾸지 않습니다.'}
    @{Group='convenience'; Section='extensions'; Key='tooltip_collab'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='개성·전법 툴팁 + 콜라보 개성/전법 해금'; Note='콜라보 해금이 툴팁 초기화에 실려 하나로 묶었습니다. 출처: 삼갤 통합패치(180.71·175.115 유동)'}
    @{Group='convenience'; Section='game_flow'; Key='speed_buttons'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='진행 속도 버튼 >>>>(×4)·>>>>>(×8)'; Note='OFF이면 ×4·×8 버튼과 배치도 원작으로 복원합니다. 게임 종료 후 저장/적용하세요. 기존 >>> 대비 배수. 게임 옵션을 60fps로 두면 ×4는 매 프레임 진행, ×8은 하루 처리가 끝난 프레임에 한 번 더 진행합니다(30fps는 최대 ×4).'}
    @{Group='convenience'; Section='game_flow'; Key='occupy_fast'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='거점 자동점령 연출 단축 (원작의 약 1/4)'; Note='거점을 점령할 때 나오는 연출을 원작의 약 1/4 길이로 줄입니다. 점령 결과·수치·턴 진행은 바뀌지 않으며, 끄면 원작 연출을 씁니다.'}
    @{Group='convenience'; Section='game_flow'; Key='advisor_briefing'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='군사의 열흘 보고'; Note='지난 10일의 소식을 좌하단 군사 알림함에 모읍니다. 다음 진행 때 비워지며, 외교·처분 등 선택 창은 유지합니다.'}
    @{Group='convenience'; Section='game_flow'; Key='briefing16'; Default=0; Requires='game_flow.advisor_briefing'; Inherit=''; Tab='편의성'; Label='군사보고 변경'; Note='등용 성공·포박 장면은 원작대로 보여 주고, 군사 초상화는 원작 얼굴 그림을 씁니다. 알림 15초. 끄면 이전 판.'}
    @{Group='convenience'; Section='game_flow'; Key='caravan_stay'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='교역 무장 소재 고정 (원래 도시에 남고 각자 도시로 귀환)'; Note='교역하러 간 무장이 원래 도시에 남아 있고, 교역이 끝나면 각자 자기 도시로 돌아가게 합니다. 교역 목록과 이동만 바뀌며 다른 외교·내정은 원작 그대로입니다. 출처: 삼갤 주딱'}
    @{Group='convenience'; Section='game_flow'; Key='auto_search'; Default=0; Requires=''; Inherit=''; Tab='편의성'; Label='탐색 위임 (화면 왼쪽 아래 버튼)'; Note='플레이어 전용. 왼쪽 아래 탐색위임 버튼을 켜면 군사가 인재 > 탐색 > 자동탐색에 정해 둔 조건으로 턴마다 한 번, 진행 직전에 탐색을 보냅니다. 켜짐 상태는 세이브마다 따로 저장됩니다. 명령서가 모자라면 보내지 않습니다. 두 위임을 모두 끄면 버튼과 화면 자료가 빠져 원작 화면입니다.'}
    @{Group='convenience'; Section='game_flow'; Key='auto_reward'; Default=0; Requires=''; Inherit=''; Tab='편의성'; Label='포상 위임 (화면 왼쪽 아래 버튼)'; Note='플레이어 전용. 왼쪽 아래 포상위임 버튼을 켜면 충성도 기준을 입력받고, 턴마다 한 번 군사가 기준 이하 장수(모든 군단)에게 군주가 있는 성의 금으로 원작 포상(1명당 금 100, 명령서 1)을 줍니다. 금이 모자라면 충성이 낮은 장수부터 줍니다. 켜짐 상태는 세이브마다 따로 저장됩니다. 두 위임을 모두 끄면 버튼과 화면 자료가 빠져 원작 화면입니다.'}
    @{Group='convenience'; Section='officer_ui'; Key='stat_exp'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='능력치 경험치 표시 (오각형 그래프 숫자 뒤 /N%)'; Note='무장 능력치 오각형 그래프의 숫자 뒤에 현재 경험치와 다음 성장까지의 %를 표시합니다. 표시 전용이며 능력치 계산은 원작 그대로입니다. 출처: 삼갤 주딱'}
    @{Group='convenience'; Section='officer_ui'; Key='dispatch_days'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='파견 남은 일수 표시 (파견 목록에 귀환까지 N일)'; Note='파견 목록에서 각 무장이 돌아오기까지 남은 일수를 N일로 표시합니다. 표시 전용이며 파견 기간·결과는 원작 그대로입니다. 출처: 삼갤 주딱'}
    @{Group='convenience'; Section='officer_ui'; Key='mediation_plus'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='중개 확장 (의형제 3인, 일부다처, 이혼)'; Note='중개 화면에서 의형제를 3명까지 맺고, 일부다처와 이혼을 선택할 수 있게 확장합니다. 원작 중개 조건과 비용은 그대로이며, 끄면 원작 선택지만 나옵니다. 출처: 삼갤 주딱'}
    @{Group='convenience'; Section='officer_ui'; Key='drag_learn'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='전법서 습득 다중 선택 (여러 무장 × 여러 전법서 한 번에)'; Note='전법서 습득 화면에서 여러 무장과 여러 전법서를 한 번에 선택할 수 있게 합니다. 실제 습득 가능 여부·비용·결과는 원작 판정을 따릅니다. 출처: 삼갤 주딱'}
    @{Group='convenience'; Section='officer_ui'; Key='drag_learn_multi'; Default=0; Requires='officer_ui.drag_learn'; Inherit=''; Tab='편의성'; Label='학습 창 여러 명 표시'; Note='여러 명을 골라 학습할 때 확인 창에 외 N명과 전법서·명령서 합계를 보여 줍니다. 끄면 이전 판.'}
    @{Group='convenience'; Section='officer_ui'; Key='lord_change'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='시작 전 군주 변경 (세력 선택 하단 「반란」 버튼)'; Note='세력 선택 화면 아래에 「반란」 버튼을 추가해 게임을 시작하기 전에 군주를 바꿀 수 있습니다. 시작한 뒤에는 원작 군주 교체 규칙만 적용됩니다. 출처: 삼갤 주딱'}
    @{Group='convenience'; Section='officer_ui'; Key='tactic_priority'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='출진 화면 전법 우선순위·출진 기억'; Note='출진 편성 화면에 「전법 우선순위」 버튼을 추가합니다. 무력형·지력형별로 전법 순서를 정하고, 무장별 「출진 기억」을 켜면 마지막 출진의 진형과 전법을 다음 출진에 되살립니다. 칸에 마우스를 올리면 나오는 전법 설명은 편의성의 「개성·전법 툴팁 + 콜라보…」가 켜져 있을 때만 표시되며, 그 기능이 꺼져 있어도 순위 지정과 출진 기억은 그대로 동작합니다. 끄면 원작 출진 화면으로 돌아갑니다.'}
    @{Group='convenience'; Section='extensions'; Key='scenery_skip'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='명승지 연출 생략'; Note='명승지에 들어갈 때 나오는 연출(카메라·안내)을 생략합니다. 게임 결과·능력치·외교에는 영향이 없고, 끄면 원작 연출을 씁니다. 출처: 112.151 유동(삼봉이정도전)의 「11종 선택패치」'}
    @{Group='convenience'; Section='officer_ui'; Key='unified'; Default=0; Requires=''; Inherit=''; Tab='편의성'; Retired=$true; Label='통합 인사·무장 검색·출진 전법 자동 선택'; Note='개발 중지로 제거된 기능입니다.'}
    @{Group=''; Section='groups'; Key='performance'; Default=1; Requires=''; Inherit=''; Tab='게임렉제거(성능)'; Label='성능 가속 전체'; Note='원작과 같은 결과를 더 빠르게 계산합니다.'}
    @{Group='performance'; Section='performance'; Key='distance'; Default=1; Requires=''; Inherit=''; Tab='게임렉제거(성능)'; Label='거리·AI 진입 계산 가속'; Note='AI가 거리와 진입 가능성을 계산하는 부분을 원작과 같은 결과로 더 빠르게 계산합니다. 전투·내정 판정은 바뀌지 않으며, 끄면 원작 계산을 씁니다.'}
    @{Group='performance'; Section='performance'; Key='name_lookup'; Default=1; Requires=''; Inherit=''; Tab='게임렉제거(성능)'; Label='원작 이름 탐색 가속'; Note='무장 이름을 찾는 원작 탐색을 캐시해 반복 검색을 줄입니다. 화면 표시와 판정 결과는 원작과 같고, 끄면 원작 탐색을 씁니다.'}
    @{Group='performance'; Section='performance'; Key='layout_keep'; Default=1; Requires=''; Inherit=''; Tab='게임렉제거(성능)'; Label='화면 레이아웃 보관 (턴 전환 로딩 단축)'; Note='턴이 바뀔 때 화면 레이아웃을 보관해 다시 만드는 로딩을 줄입니다. 화면 구성과 표시는 원작과 같고, 끄면 매번 새로 만듭니다.'}
    @{Group='performance'; Section='performance'; Key='fast_menu'; Default=1; Requires=''; Inherit=''; Tab='게임렉제거(성능)'; Label='지도 메뉴 클릭 대기 감소 (메뉴를 닫은 뒤 바로 다음 클릭)'; Note='지도 메뉴를 닫은 뒤 다음 클릭이 먹히기까지의 대기를 줄입니다. 메뉴 항목과 동작은 원작 그대로이며, 끄면 원작 대기로 돌아갑니다.'}
    @{Group='performance'; Section='performance'; Key='appear_batch'; Default=1; Requires=''; Inherit=''; Tab='게임렉제거(성능)'; Label='부대 등장 묶음 처리 (전략중 뒤 0일 대기 단축)'; Note='전략중이 끝난 뒤 출진 부대의 지도 깃발·정보 패널을 한 프레임에 여러 개 만듭니다(원작은 프레임당 1개, 무거우면 1프레임 쉼). 부대·순서·결과는 원작과 같고 등장 표시만 빨라집니다.'}
    @{Group='performance'; Section='performance'; Key='area_index'; Default=1; Requires=''; Inherit=''; Tab='게임렉제거(성능)'; Label='지역 칸 목록 캐시 (AI 건설 검사 가속)'; Note='컴퓨터 AI가 시설 건설 자리를 고를 때 지역 시설 수를 셀 때마다 맵 전체(48,400칸)를 훑던 것을, 지역별 칸 목록을 미리 만들어 두고 그 지역 칸만 셉니다. 결과는 원작과 같고 전략중 AI 계산이 빨라집니다.'}
    @{Group='performance'; Section='performance'; Key='path_fast'; Default=1; Requires=''; Inherit=''; Tab='게임렉제거(성능)'; Label='경로 탐색 가속'; Note='원작 부대 경로 탐색(A*)의 다음 후보 목록을 힙으로 바꿔 빠르게 합니다. 경유지가 하나인 경로는 원작과 같은 결과입니다. 경유지가 여럿인 경로는 원작의 이전 구간 상태가 섞이던 동작을 없애 구간마다 새로 찾습니다(원작과 조금 다를 수 있음). 끄면 원작으로 돌아갑니다.'}
    @{Group='performance'; Section='performance'; Key='face_keep'; Default=1; Requires=''; Inherit=''; Tab='게임렉제거(성능)'; Label='작은 무장 얼굴 상주 (깃발·선택·전략 화면)'; Note='부대 깃발과 무장 선택·전략 화면에서 불러온 작은 얼굴 그림을 같은 보관함에 남겨 다시 불러오는 시간을 줄입니다. 최대 256명(약 600MB 이하), 상한이면 오래 안 쓴 얼굴부터 교체, 메모리가 부족하면(사용률 85% 이상 또는 남은 램 1.5GB 미만) 절반을 해제하고 새로 붙잡지 않음. 적재 대기 중인 얼굴은 해제하지 않습니다. 표시와 게임 결과는 원작과 같습니다.'}
    @{Group='performance'; Section='performance'; Key='face_prefetch'; Default=1; Requires='performance.face_keep'; Inherit=''; Tab='게임렉제거(성능)'; Label='출진 지휘관 얼굴 미리 요청 (0일 대기 단축)'; Note='부대가 나타나기 전에 출진 지휘관 얼굴을 한꺼번에 요청해, 얼굴마다 한 프레임씩 기다리던 시간을 겹칩니다. 깃발에 붙이기 직전에는 원작과 같이 적재 완료를 확인합니다. 부대 등장 묶음 처리가 켜져 있을 때 동작하며, 미리 요청한 얼굴은 상주 상한에 포함됩니다.'}
    @{Group='performance'; Section='performance'; Key='timer_1ms'; Default=0; Requires=''; Inherit=''; Tab='게임렉제거(성능)'; Retired=$true; Label='1ms 타이머'; Note='측정상 효과가 없어 제거되었습니다.'}
    @{Group='convenience'; Section='game_flow'; Key='fast_save'; Default=1; Requires=''; Inherit=''; Tab='게임렉제거(성능)'; Label='빠른 자동저장 압축'; Note='자동저장을 원작과 같은 형식으로 더 빠르게 압축합니다. 실패하면 원작 저장으로 돌아가며, 세이브 호환성은 그대로입니다.'}
    @{Group='performance'; Section='progress_native'; Key='geometry'; Default=1; Requires='performance.distance'; Inherit=''; Tab='게임렉제거(성능)'; Label='거리 계산 세부 가속'; Note='거리·AI 진입 가속 안에서 기하 계산을 같은 결과로 더 빠르게 처리합니다. 위의 「거리·AI 진입 계산 가속」을 켜야 함께 동작합니다.'}
    @{Group='convenience'; Section='stratagem'; Key='building_destruction_all'; Default=0; Requires=''; Inherit=''; Tab='게임성'; Label='건물파괴 지역 전체 피해'; Note='건물파괴 성공 시 선택 지역의 모든 적 군사시설·장애물·함정에 원작에서 계산된 피해량을 각각 적용합니다. 성·관문·아군·동맹·중립 건물은 제외하며, 실패 시에는 원작 처리를 유지합니다. 플레이어와 컴퓨터 모두 적용됩니다.'}
    @{Group=''; Section='groups'; Key='ccz'; Default=1; Requires=''; Inherit=''; Tab='조조전'; Label='조조전 기능 전체 (SAN14_CCZ.dll)'; Note='끄면 조조전 탭 기능을 모두 쓰지 않으며 SAN14_CCZ.dll도 불러오지 않습니다. 편의·게임성 기능 전체(편의성 탭)가 켜져 있어야 동작합니다.'}
    @{Group='ccz'; Section='ccz'; Key='ccz_sprites'; Default=1; Requires=''; Inherit=''; Tab='조조전'; Label='조조전 병종 그림 (필드 부대)'; Note='OFF이면 부대 UI 배치도 원작으로 복원합니다. 필드의 부대를 조조전 병종 그림으로 그립니다. 옷만 세력색(파랑·초록·보라 같은 차가운 색은 빨강~주황 대비색)으로 칠하고 말·얼굴은 원래 색이며, 대비되는 테두리를 두르고 이동·공격·대기 동작과 방향이 바뀝니다. 신조조전 장수 92명(전용 병종 17명: 조조=위무제·여포=무신·관우=군신 등, 나머지 75명은 신조조전 개인 그림)은 기본값은 군주·군사 병종보다 고유 그림 우선입니다. 게임 첫 메뉴의 「조조전」에서 진형별 병종·통솔 경계·무장별 고유 그림 적용 진형을 바꿀 수 있습니다. 그림 출처: 조조전(KOEI) 6.6·신조조전 7.0. 그림 사용 시 3D 모형 숨김과 지형 가림도 함께 적용합니다.'}
    @{Group='ccz'; Section='ccz'; Key='sprite_fallback'; Default=0; Requires='ccz.ccz_sprites'; Inherit=''; Tab='조조전'; Label='그림을 못 읽으면 원작 모델'; Note='조조전 그림을 못 읽거나 칸이 모자라면 원작 부대 모델을 보여 줍니다. 끄면 이전 판.'}
    @{Group='ccz'; Section='ccz'; Key='ccz_hide_model'; Default=0; Requires='ccz.ccz_sprites'; Inherit=''; Tab='조조전'; Retired=$true; Label='조조전 그림 부대의 원래 3D 모형 숨김'; Note='병종 그림에 통합되었습니다.'}
    @{Group='ccz'; Section='ccz'; Key='ccz_depth'; Default=0; Requires='ccz.ccz_sprites'; Inherit=''; Tab='조조전'; Retired=$true; Label='조조전 그림 지형 가림 처리'; Note='병종 그림에 통합되었습니다.'}
    @{Group='ccz'; Section='ccz'; Key='ccz_far_keep'; Default=0; Requires='ccz.ccz_sprites'; Inherit=''; Tab='조조전'; Retired=$true; Label='원거리 부대 그림 유지'; Note='더 이상 필요 없어 제거되었습니다.'}
    @{Group='ccz'; Section='ccz'; Key='ccz_block'; Default=1; Requires=''; Inherit=''; Tab='조조전'; Label='조조전식 막기'; Note='막기: 맞는 장수의 무력이 높을수록 일반 공격을 완전히 막아 1명만 잃습니다(5% + 무력 차 x 0.4, 1~10%). 한 번 발동하면 그 공격을 포함해 받는 일반 공격 5번을 막습니다. 하루 동안 일반 공격을 주고받지 않으면 상태가 끝납니다. 일반 공격만 대상이며 예상 피해 표시에는 반영되지 않습니다. 조조전 그림을 켜면 막기 자세·섬광이 보입니다.'}
    @{Group='ccz'; Section='ccz'; Key='ccz_critical'; Default=1; Requires=''; Inherit=''; Tab='조조전'; Label='조조전식 회심의 일격'; Note='회심의 일격: 공격 장수의 무력·부대 사기가 높을수록 일반 공격 피해가 1.5배가 됩니다(5% + 무력 차 x 0.3 + 사기 차 x 0.2, 1~10%). 한 번 발동하면 그 공격을 포함해 다음 일반 공격 5번이 모두 회심이 됩니다. 하루 동안 일반 공격을 주고받지 않으면 상태가 끝납니다. 일반 공격만 대상이며 예상 피해 표시에는 반영되지 않습니다. 조조전 그림을 켜면 막기 자세·섬광이 보입니다.'}
    @{Group='convenience'; Section='combat'; Key='district_intercept_block'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='지원·내정 군단 출진 제한 (플레이어 2군단~)'; Note='플레이어가 위임한 군단의 방침이 지원이면 수송 출진만, 내정이면 요격·지역 점령·공격 출진을 막습니다(수송·건설은 허용). 컴퓨터 군단·직할 1군단·공격 방침·직접 명령·이미 출전한 부대는 그대로입니다. 전투 AI 교정 없이도 적용됩니다.'}
    @{Group='combat'; Section='combat'; Key='attack_eval70'; Default=0; Requires=''; Inherit=''; Tab='전투'; Label='공격 검토: 목표 성 전투력 70%'; Note='컴퓨터 세력만(플레이어 직할·위임 군단 제외). 원작 공격 검토에서 목표 성 전투력을 70%로 평가합니다. 끄면 원작.'}
    @{Group='combat'; Section='combat'; Key='retreat_eval60'; Default=0; Requires=''; Inherit=''; Tab='전투'; Label='야전 퇴각 재판정: 적 60%'; Note='컴퓨터 세력만(플레이어 직할·위임 군단 제외). 야전 부대 퇴각 재판정에서 적을 60%로 평가합니다. 요격 평가는 원작. 끄면 원작.'}
    @{Group='combat'; Section='combat'; Key='siege_guard'; Default=0; Requires=''; Inherit=''; Tab='전투'; Label='공성군 요격 5칸(지력 80 이상)'; Note='컴퓨터 공성군 야전 부대만. 지휘관 지력 80 이상이면 플레이어 부대를 5칸 안에서 요격합니다. 세이브에 남지 않습니다. 끄면 원작 3칸.'}
    @{Group='combat'; Section='combat'; Key='delegated_exclude'; Default=0; Requires=''; Inherit=''; Tab='전투'; Label='플레이어 위임 군단은 컴퓨터 출진·공성 규칙에서 제외'; Note='켜면 플레이어 세력의 위임 군단은 모드의 출진 편성·공성·지원·장애물 규칙을 받지 않고 원작 위임 AI대로 움직입니다. 컴퓨터 세력은 그대로입니다. 위임 군단 내정·등용 자동화는 이 스위치와 무관합니다. 끄면 이전 판(위임 군단도 같은 규칙).'}
    @{Group='convenience'; Section='personnel'; Key='merit'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='논공 친애 확장'; Note='논공 친애 관련 추가 효과를 켭니다. 아래의 무공 단계별 병력·군주 친애 옵션과 함께 동작하며, 꺼도 기존 논공 기록과 명품창고 저장은 보존됩니다.'}
    @{Group='convenience'; Section='personnel'; Key='merit_bond16'; Default=0; Requires='personnel.merit'; Inherit=''; Tab='게임성'; Label='논공 친애 확장'; Note='군주가 논공 3단계면 군주의 부·모·배우자·의형제도 논공 친애 대상이 됩니다. 끄면 이전 판.'}
    @{Group='convenience'; Section='personnel'; Key='merit_troops_level'; Default=4; Requires='personnel.merit'; Inherit=''; Tab='게임성'; Label='무공 1·2단계 병력 (1500~3000)'; Note='원작은 1단계·2단계 각 500(새 패치 1000)입니다. 1500/2000/2500/3000 중에서 고르며 기본은 3000입니다. 번호를 누르면 값이 차례로 바뀌고, 번호=값(1~4)으로 바로 지정할 수 있습니다.'}
    @{Group='convenience'; Section='personnel'; Key='merit_friendship'; Default=1; Requires='personnel.merit'; Inherit=''; Tab='게임성'; Label='논공 친애 (무공 3단계 군주 상호 친애)'; Note='켜면 무공 3단계에 군주와의 상호 친애를 원작 효과에 더해 줍니다. 옛 빌드는 3단계 병력 +1000이 0으로 바뀌고, 새 패치 빌드는 원작 시정 Lv+1이 그대로 남습니다. 끄면 3단계는 원작 그대로입니다.'}
    @{Group='convenience'; Section='personnel'; Key='awards_ai'; Default=1; Requires='personnel.awards'; Inherit=''; Tab='게임성'; Label='컴퓨터 명품 자동 배분'; Note='컴퓨터 세력·플레이어 위임 군단이 창고 명품을 능력에 맞게 배분합니다. 군주 직할 군단(1군단)은 제외.'}
    @{Group='convenience'; Section='personnel'; Key='awards_horse'; Default=0; Requires='personnel.awards_ai'; Inherit=''; Tab='게임성'; Label='퇴각확실 명마 우선'; Note='컴퓨터 세력만. 퇴각확실 명마를 군주·지정 무장에게 먼저 줍니다. 끄면 이전 판.'}
    @{Group='convenience'; Section='personnel'; Key='awards_v16'; Default=0; Requires='personnel.awards_ai'; Inherit=''; Tab='게임성'; Label='명품 배분 변경'; Note='컴퓨터 세력만(플레이어 위임 군단은 원작). 명령서 없이 배분하고 지역담당관 해임을 허용합니다. 끄면 이전 판.'}
    @{Group='convenience'; Section='personnel'; Key='recruit_ai'; Default=1; Requires=''; Inherit='personnel.recruit'; Tab='게임성'; Label='컴퓨터·위임 군단 등용 시도 제한'; Note='컴퓨터와 위임 군단이 관계·혐오 등으로 성공할 수 없는 등용 시도를 하지 않게 합니다. 플레이어의 직접 등용은 그대로입니다.'}
    @{Group='convenience'; Section='personnel'; Key='injury'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='부상 무장 보호'; Note='컴퓨터·위임 군단이 부상 무장을 출진·임무·유언비어에 쓰지 않습니다. 플레이어 직접 명령은 그대로입니다.'}
    @{Group='convenience'; Section='personnel'; Key='injury_herbal'; Default=1; Requires=''; Inherit='personnel.injury'; Tab='게임성'; Label='컴퓨터 세력 본초학 시정 자동 배치'; Note='부상 무장이 있는 컴퓨터 세력만 적용(플레이어 세력의 시정은 건드리지 않음).'}
    @{Group='convenience'; Section='domestic'; Key='domestic'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='자동 내정 — 상업·농업·병영 중 낮은 개발부터 균형 개발'; Note='플레이어가 위임한 2군단 이후와 컴퓨터 세력에 적용됩니다. 상업·농업·병영 중 개발이 낮은 항목부터 균형 있게 올리며, 군주 직할 1군단은 아래 「1군단 자동 내정」을 켰을 때만 적용됩니다.'}
    @{Group='convenience'; Section='domestic'; Key='domestic_direct'; Default=0; Requires='domestic.domestic'; Inherit=''; Tab='게임성'; Label='1군단(군주 직할) 자동 내정'; Note='켜면 플레이어가 직접 내정하는 1군단 지역도 매일 균형 개발 항목으로 바꿉니다(직접 정한 개발 항목을 덮어씀). 명령서 비용 0은 적용되지 않습니다.'}
    @{Group='convenience'; Section='domestic'; Key='domestic_free'; Default=1; Requires='domestic.domestic'; Inherit=''; Tab='게임성'; Label='지역담당관 자동 임명 명령서 비용 0'; Note='플레이어 2군단 이후·컴퓨터 세력의 자동 임명에만 적용. 1군단과 플레이어 수동 임명은 원작 비용 그대로입니다.'}
    @{Group='convenience'; Section='domestic'; Key='population'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='인구 월간 증가 배율'; Note='매달 늘어나는 인구(원작 증가분)를 아래 배율만큼 늘립니다. 지역 인구 상한 60,000은 그대로입니다.'}
    @{Group='convenience'; Section='domestic'; Key='population_rate'; Default=4; Requires='domestic.population'; Inherit=''; Tab='게임성'; Label='인구 증가 배율 (1~4배)'; Note='1=원작과 같음, 2~4=그 배수. 번호를 누르면 1→2→3→4로 바뀌고, 번호=값으로 바로 지정할 수 있습니다.'}
    @{Group='convenience'; Section='extensions'; Key='traits9'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='개성 9칸'; Note='무장 개성을 원작 5칸에서 9칸까지 늘립니다. 세이브에 저장된 개성 값은 보존되며, 끄면 원작 5칸만 표시·적용합니다.'}
    @{Group='convenience'; Section='extensions'; Key='stats200'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='능력치 상한 200'; Note='무장 능력치 상한을 원작 100에서 200으로 늘립니다. 100을 넘는 성장과 보정이 반영되며, 끄면 새 성장은 원작 100 상한을 따릅니다.'}
    @{Group='convenience'; Section='extensions'; Key='stats200_growth'; Default=1; Requires='extensions.stats200'; Inherit=''; Tab='게임성'; Label='능력치 100 이후 자연 성장'; Note='능력치가 100 이상일 때도 경험치로 조금씩 자연 성장하게 합니다. 200에서 멈추며, 위의 「능력치 상한 200」을 켜야 함께 동작합니다. 끄면 100 이후에는 성장하지 않습니다.'}
    @{Group=''; Section='groups'; Key='diagnostics'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='개발용 기능 전체'; Note='진단 로그·추적·시간 기록 같은 개발용 기능의 묶음입니다. 끄면 아래 항목이 모두 꺼집니다. 일반 플레이에는 필요 없습니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='enabled'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='진단 로그 기록 (SAN14_*.log)'; Note='모드 판단과 오류를 게임 폴더 로그 파일에 남깁니다. 배포용(로그 제외) DLL에서는 효과가 없습니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='terrain_diag_mode'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='지형 편집 렉 분리 A/B/C'; Note='0=기준(물살·렌더러 ON), 1=A(물살 OFF), 2=B(렌더러·높이 캡처 OFF), 3=C(둘 다 OFF). A/B/C는 진단 전용입니다. terrain_edit=1에서 줌 렉 비교만 하고 실제 지형 편집·저장에는 사용하지 마세요.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='reuse_handle'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='로그 파일 핸들 재사용'; Note='로그를 쓸 때 파일을 매번 다시 열지 않아 기록 비용을 줄입니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='ai_trace'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='원작 AI 행동 추적 (CSV)'; Note='원작 AI 판단 노드가 부대·군세 목표를 바꾸는 과정을 data/ai_trace에 기록합니다. 기록량이 많습니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='ai_trace_original'; Default=0; Requires='diagnostics.ai_trace'; Inherit=''; Tab='기타(개발용)'; Label='원작만 추적 (모드 훅 연결 안 함)'; Note='모드 기능을 연결하지 않고 원작 AI만 기록합니다. 이 상태로 기존 세이브에 덮어쓰지 마세요.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='layout_wait'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='화면 레이아웃 대기 기록'; Note='턴 전환 때 화면 레이아웃 로딩을 기다린 시간을 기록합니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='day_stage'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='하루 처리 단계 기록'; Note='진행 중 하루 처리 단계가 바뀔 때마다 그 단계에 머문 시간과 실제 계산 시간을 진단 로그(day_stage)에 남깁니다. 전략중 뒤 1일 시작 대기 조사용이며 진행 속도 버튼이 켜져 있어야 기록됩니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='cost_profile'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='함수 시간 측정 (CSV)'; Note='모드 함수와 원작 AI 판단 노드마다 호출 수·시간을 data/performance에 기록합니다. 측정 중에는 약간 느려집니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='call_trace'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='함수 호출 추적 (개발용)'; Note='진단 빌드에서 게임 함수 호출·그래픽 그리기·경로 탐색을 크래시 기록에 남깁니다. 게임이 느려지므로 평소에는 끕니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='area_index_verify'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='지역 칸 캐시 원작 결과 대조'; Note='진단판에서 매 지역 시설 수 검사 결과를 원작 전체 순회와 대조합니다. 불일치 시 원작 결과를 사용하며 측정 중에는 느려집니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='path_fast_verify'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='경로 탐색 가속 결과 비교'; Note='0=끔, 1=매번, N=N번에 한 번. 1은 원작 결과를 유지하며 깨끗한 버퍼와 잔존 상태를 구분합니다. N>1은 전체 호출 순번마다 표본 구간을 깨끗하게 비교하고, 같으면 새 결과·다르면 원작 결과를 씁니다. 1일 때만 지우기 생략을 끕니다. 불일치 상세는 하루 16건, 새 구현만 실행한 시간도 기록합니다. N은 기능설정.ini에서 지정합니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='path_count'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='경로 탐색 호출 횟수 기록'; Note='세력 AI가 경로 탐색을 몇 번 부르고 같은 출발지·목적지·조건을 되풀이하는지 진단 로그에 남깁니다. 게임 동작은 바뀌지 않습니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='water_diag'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='물살 렌더 진단'; Note='지형 렌더의 물살 처리 과정을 진단 로그에 남깁니다. 관찰 전용입니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='gpu_profile'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='GPU 시간 기록'; Note='그래픽 처리 시간을 data 폴더에 기록합니다. 관찰 전용입니다.'}
    @{Group='diagnostics'; Section='progress_native'; Key='profile'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='AI 진행 함수 시간 기록'; Note='원작 AI 진행 함수별 호출 시간을 진단 로그에 남깁니다.'}
)
function Read-FeatureIni([string]$Path) {
    $taskIni = @{}
    if (-not [IO.File]::Exists($Path)) { return $taskIni }
    $taskSection = ''
    foreach ($taskLine in ([IO.File]::ReadAllText($Path) -split "`r?`n")) {
        $taskText = $taskLine.Trim()
        if ($taskText -match '^\[(.+)\]$') { $taskSection = $Matches[1].Trim().ToLowerInvariant(); if (-not $taskIni.ContainsKey($taskSection)) { $taskIni[$taskSection] = [ordered]@{} }; continue }
        if ($taskText -eq '' -or $taskText.StartsWith(';') -or $taskSection -eq '') { continue }
        $taskAt = $taskText.IndexOf('=')
        if ($taskAt -gt 0) { $taskIni[$taskSection][$taskText.Substring(0,$taskAt).Trim().ToLowerInvariant()] = $taskText.Substring($taskAt+1).Trim() }
    }
    return $taskIni
}
function Get-FeatureValues($Ini) {
    $taskValues = [ordered]@{}
    # Inheriting keys are resolved after the switches they fall back to.
    foreach ($taskItem in (@($taskFeatureCatalog | Where-Object { -not $_.Inherit }) + @($taskFeatureCatalog | Where-Object { $_.Inherit }))) {
        $taskValue = $taskItem.Default
        if ($taskItem.Inherit -and $taskValues.Contains($taskItem.Inherit)) { $taskValue = $taskValues[$taskItem.Inherit] }
        $taskSources=@(@('features',$taskItem.Key), @($taskItem.Section,$taskItem.Key))
        if($taskItem.Section -eq 'performance' -and $taskItem.Key -eq 'fast_menu'){$taskSources=@(@('game_flow','fast_menu'))+$taskSources}
        foreach ($taskSource in $taskSources) {
            if ($Ini.ContainsKey($taskSource[0]) -and $Ini[$taskSource[0]].Contains($taskSource[1])) {
                $taskNumber = 0
                if ([int]::TryParse($Ini[$taskSource[0]][$taskSource[1]], [ref]$taskNumber)) { $taskValue = if ($taskItem.Key -eq 'path_fast_verify') { [Math]::Max(0, $taskNumber) } elseif ($taskItem.Key -eq 'terrain_diag_mode') { [Math]::Max(0, [Math]::Min(3, $taskNumber)) } elseif ($taskItem.Key -eq 'population_rate' -or $taskItem.Key -eq 'merit_troops_level') { [Math]::Max(1, [Math]::Min(4, $taskNumber)) } else { [int]($taskNumber -ne 0) } }
            }
        }
        $taskValues[$taskItem.Section + '.' + $taskItem.Key] = if ($taskItem.Retired) { 0 } else { $taskValue }
    }
    return $taskValues
}
function Write-FeatureIni([string]$Path, $Values, $Ini) {
    # user 2026-11-03: known obsolete keys only; preserve every unknown/raw key.
    if($Ini.Contains('game_flow')){$Ini['game_flow'].Remove('fast_menu')}
    if($Ini.Contains('performance')){$Ini['performance'].Remove('strategy_batch')}
    $taskLines = New-Object Collections.Generic.List[string]
    $taskLines.Add('; AI 모드 기능 켜기/끄기. 1 = 켜기, 0 = 끄기. 게임을 다시 시작하면 적용됩니다.')
    $taskLines.Add('; 관리 도구: optional_tools\AI모드_기능선택.cmd 또는 AI모드_적용.cmd 실행 중 선택 화면.')
    $taskLines.Add('; 함께 묶인 항목은 같은 게임 코드를 공유하므로 따로 끌 수 없습니다.')
    $taskSections = New-Object Collections.Generic.List[string]
    foreach ($taskItem in $taskFeatureCatalog) { if (-not $taskSections.Contains($taskItem.Section)) { $taskSections.Add($taskItem.Section) } }
    $taskOwned = @{}; foreach ($taskItem in $taskFeatureCatalog) { $taskOwned[$taskItem.Section + '.' + $taskItem.Key] = $true }
    foreach ($taskSection in $taskSections) {
        $taskLines.Add(''); $taskLines.Add('[' + $taskSection + ']')
        foreach ($taskItem in ($taskFeatureCatalog | Where-Object { $_.Section -eq $taskSection })) {
            $taskComment = '; ' + $taskItem.Label
            if ($taskItem.Note) { $taskComment += ' — ' + $taskItem.Note }
            $taskLines.Add($taskComment)
            if ($taskItem.Retired) { $Values[$taskItem.Section + '.' + $taskItem.Key] = 0 }
            $taskLines.Add($taskItem.Key + '=' + $Values[$taskItem.Section + '.' + $taskItem.Key])
        }
        # Hidden/newer keys of an owned section stay inside it: the DLL reads only the first
        # [section] of a name, so a second header would silently lose them (2026-11-01).
        if ($Ini.Contains($taskSection)) {
            foreach ($taskKey in @($Ini[$taskSection].Keys | Where-Object { -not $taskOwned.ContainsKey($taskSection + '.' + $_) })) { $taskLines.Add($taskKey + '=' + $Ini[$taskSection][$taskKey]) }
        }
    }
    # Sections the menu does not own (diagnostics, hidden keys) are kept as they were.
    foreach ($taskSection in $Ini.Keys) {
        if ($taskSection -eq 'features' -or $taskSections.Contains($taskSection)) { continue }
        $taskExtra = @($Ini[$taskSection].Keys | Where-Object { -not $taskOwned.ContainsKey($taskSection + '.' + $_) })
        if ($taskExtra.Count -eq 0) { continue }
        $taskLines.Add(''); $taskLines.Add('[' + $taskSection + ']')
        foreach ($taskKey in $taskExtra) { $taskLines.Add($taskKey + '=' + $Ini[$taskSection][$taskKey]) }
    }
    $taskText = ($taskLines -join "`r`n") + "`r`n"
    $taskTemp = $Path + '.aitactics.tmp'
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $Path))
    [IO.File]::WriteAllText($taskTemp, $taskText, [Text.Encoding]::Unicode)
    try {
        if ([IO.File]::Exists($Path)) { [IO.File]::Replace($taskTemp, $Path, [NullString]::Value) } else { [IO.File]::Move($taskTemp, $Path) }
    } finally { if ([IO.File]::Exists($taskTemp)) { [IO.File]::Delete($taskTemp) } }
}
function Show-FeatureMenu($Values) {
    Write-Host ''
    Write-Host '===== 설치할 기능 선택 =====' -ForegroundColor Cyan
    $taskNumber = 0
    foreach ($taskItem in $taskFeatureCatalog) {
        $taskNumber++
        if ($taskItem.Retired) { continue } # user 2026-11-03: hidden legacy keys retain OFF migration
        $taskValue = $Values[$taskItem.Section + '.' + $taskItem.Key]
        $taskOn = $taskValue -ne 0
        $taskParentOff = $taskItem.Group -and $Values['groups.' + $taskItem.Group] -eq 0
        $taskMark = if ($taskItem.Retired) { '[사용 중지]' } elseif ($taskItem.Key -eq 'terrain_diag_mode') { @('[0 기준]','[1 A]','[2 B]','[3 C]')[$taskValue] } elseif ($taskItem.Key -eq 'population_rate') { '[' + $taskValue + '배]' } elseif ($taskItem.Key -eq 'merit_troops_level') { '[' + @(1500,2000,2500,3000)[$taskValue-1] + ']' } elseif ($taskOn) { '[켬]' } else { '[끔]' }
        $taskIndent = if ($taskItem.Group) { '    ' } else { '' }
        $taskColor = if ($taskItem.Retired -or $taskParentOff) { 'DarkGray' } elseif (-not $taskItem.Group) { 'Yellow' } elseif ($taskOn) { 'Green' } else { 'Gray' }
        if (-not $taskItem.Group) { Write-Host '' }
        Write-Host ('{0}{1,2}. {2} {3}' -f $taskIndent, $taskNumber, $taskMark, $taskItem.Label) -ForegroundColor $taskColor
        if ($taskItem.Note) { Write-Host ('{0}       {1}' -f $taskIndent, $taskItem.Note) -ForegroundColor DarkGray }
    }
    Write-Host ''
    Write-Host '번호를 입력하면 켬/끔이 바뀝니다 (여러 개: 4 7 12). 지형 A/B/C 항목(0→1→2→3)과 인구 증가 배율(1→2→3→4)은 번호를 누를 때 순환하며 번호=값으로 바로 지정할 수 있습니다.'
    Write-Host '사용 중지 항목은 선택할 수 없고, 묶음이 꺼진 회색 항목은 동작하지 않습니다.'
    Write-Host 'Enter = 이대로 저장   A = 모두 켜기   D = 배포 기본값   Q = 바꾸지 않고 진행'
}
# Returns $null when the user keeps the current file unchanged (Q), otherwise the values.
function Select-Features($Values) {
    while ($true) {
        Show-FeatureMenu $Values
        $taskInput = Read-Host '선택'
        if ($null -eq $taskInput) { return $Values }
        $taskInput = $taskInput.Trim()
        if ($taskInput -eq '') { return $Values }
        if ($taskInput -ieq 'q') { return $null }
        if ($taskInput -ieq 'a') { foreach ($taskItem in $taskFeatureCatalog) { $Values[$taskItem.Section + '.' + $taskItem.Key] = if ($taskItem.Retired -or $taskItem.Key -eq 'terrain_diag_mode') { 0 } elseif ($taskItem.Key -eq 'population_rate' -or $taskItem.Key -eq 'merit_troops_level') { $taskItem.Default } else { 1 } }; continue }
        if ($taskInput -ieq 'd') { foreach ($taskItem in $taskFeatureCatalog) { $Values[$taskItem.Section + '.' + $taskItem.Key] = if ($taskItem.Retired) { 0 } else { $taskItem.Default } }; continue }
        foreach ($taskToken in ($taskInput -split '[\s,]+')) {
            $taskIndex = 0
            $taskMode = 0; $taskHasMode = $false
            if ($taskToken -match '^(\d+)=(\d+)$') { $taskIndex=[int]$Matches[1]; $taskMode=[int]$Matches[2]; $taskHasMode=$true }
            elseif (-not [int]::TryParse($taskToken, [ref]$taskIndex)) { $taskIndex=0 }
            if ($taskIndex -ge 1 -and $taskIndex -le $taskFeatureCatalog.Count) {
                $taskItem = $taskFeatureCatalog[$taskIndex-1]; $taskName = $taskItem.Section + '.' + $taskItem.Key
                if ($taskItem.Retired) { $Values[$taskName] = 0; Write-Host ($taskItem.Label + ': 사용 중지') -ForegroundColor DarkGray; continue }
                if ($taskItem.Key -eq 'terrain_diag_mode') {
                    if ($taskHasMode -and ($taskMode -lt 0 -or $taskMode -gt 3)) { Write-Host 'terrain_diag_mode는 0~3만 가능합니다.' -ForegroundColor Red; continue }
                    $Values[$taskName] = if ($taskHasMode) { $taskMode } else { ($Values[$taskName]+1)%4 }
                    if ($Values[$taskName] -ne 0) { $Values['groups.diagnostics']=1 }
                } elseif ($taskItem.Key -eq 'merit_troops_level') {
                    if ($taskHasMode -and ($taskMode -lt 1 -or $taskMode -gt 4)) { Write-Host '무공 병력은 1~4(1500/2000/2500/3000)만 가능합니다.' -ForegroundColor Red; continue }
                    $Values[$taskName] = if ($taskHasMode) { $taskMode } else { $Values[$taskName]%4+1 }
                } elseif ($taskItem.Key -eq 'population_rate') {
                    if ($taskHasMode -and ($taskMode -lt 1 -or $taskMode -gt 4)) { Write-Host '인구 증가 배율은 1~4만 가능합니다.' -ForegroundColor Red; continue }
                    $Values[$taskName] = if ($taskHasMode) { $taskMode } else { $Values[$taskName]%4+1 }
                } elseif ($taskHasMode) { Write-Host ('번호=값 직접 지정은 지형 A/B/C·인구 증가 배율·무공 병력 항목만 지원합니다: ' + $taskToken) -ForegroundColor Red }
                else { $Values[$taskName] = [int]($Values[$taskName] -eq 0) }
            } elseif ($taskToken) { Write-Host ('알 수 없는 입력: ' + $taskToken) -ForegroundColor Red }
        }
    }
}
# Interactive: menu then write. Non-interactive: keep values, rewrite in the current
# layout (older files gain the new keys with their effective values).
function Invoke-FeatureSelection([string]$Path, [bool]$Interactive) {
    $taskIni = Read-FeatureIni $Path
    $taskValues = Get-FeatureValues $taskIni
    if ($Interactive) {
        $taskChosen = Select-Features $taskValues
        if ($null -eq $taskChosen) {
            if ([IO.File]::Exists($Path)) { Write-Host '기능 설정을 바꾸지 않았습니다.'; return $false }
            $taskChosen = Get-FeatureValues @{}
        }
        $taskValues = $taskChosen
    }
    Write-FeatureIni $Path $taskValues $taskIni
    $taskOn = @($taskFeatureCatalog | Where-Object { $_.Group -and $taskValues[$_.Section + '.' + $_.Key] -ne 0 -and $taskValues['groups.' + $_.Group] -ne 0 }).Count
    Write-Host ('기능 설정을 저장했습니다: AI_TacticsMod\기능설정.ini (세부 기능 {0}개 켜짐). 게임을 다시 시작하면 적용됩니다.' -f $taskOn)
    return $true
}
