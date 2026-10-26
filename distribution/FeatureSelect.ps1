# Feature selection for AI_TacticsMod\기능설정.ini (UTF-16, read by the DLLs at game start).
# Dot-sourced by 관리.ps1. Each DLL reads [section] key, then the legacy [features] key,
# then its default, so files written by older installers keep their values.
# Items that share game code are one switch (see each note); they cannot be split.
# v12.8: Requires = parent feature that must also be on (DLL ANDs it); Inherit = the switch an
# older file's missing key falls back to. Tab is the GUI installer tab (installer/InstallerGui.cs).
$taskFeatureCatalog = @(
    @{Group='convenience'; Section='diplomacy'; Key='diplomacy_overinvest'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='외교 군정 100% 초과 투입'; Note='컴퓨터가 시작한 친선·동맹·포로반환·공격요청 교섭에서 보유 군정까지 투입합니다. 100%를 넘긴 방어 효과는 상대의 추가 투입 후에도 유지됩니다.'}
    @{Group='convenience'; Section='diplomacy'; Key='diplomacy_counterturn'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='외교 군정 마지막 추가 대응'; Note='컴퓨터의 마지막 응답 뒤 플레이어가 한 번 더 군정을 투입할 수 있습니다. 추가 대응 후에는 바로 판정합니다.'}
    @{Group=''; Section='groups'; Key='combat'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='전투 AI 교정 전체 (SAN14_CombatAI.dll)'; Note='출진 편성·군세 이동·공성·병참·퇴각을 하나의 진행 상태로 관리합니다. 끄면 전투 DLL을 불러오지 않습니다.'}
    @{Group='combat'; Section='combat'; Key='sortie_formation'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='군세 출진 편성 (병력별 2~7부대·공성 진형·1,000명 지원)'; Note='컴퓨터 출진 판단 때 모드 편성으로 군세를 꾸립니다. 끄면 원작 출진 편성을 그대로 씁니다.'}
    @{Group='combat'; Section='combat'; Key='departures'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='정기 선제 출진 검토 (10일마다)'; Note='모드가 지휘하는 컴퓨터 세력이 여유 병력으로 주변 적 거점에 출진할지 검토합니다.'}
    @{Group='combat'; Section='combat'; Key='group_advance'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='집결 후 30일 정지 군세의 집단 진군'; Note='대열과 최종 목표를 유지해 함께 진군하고 이 명령을 잠급니다. 끄면 새 집단 진군을 시작하지 않습니다.'}
    @{Group='combat'; Section='combat'; Key='stationary_retreat'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='같은 자리 40일 정지 부대 퇴각'; Note='4턴 동안 움직이지 못한 부대를 귀환시킵니다.'}
    @{Group='combat'; Section='combat'; Key='morale'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='사기 회복 우선'; Note='사기가 낮은 군세가 공격보다 회복을 먼저 합니다.'}
    @{Group='combat'; Section='combat'; Key='support'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='호위·동행 지원 요청'; Note='경로 위 거점 호위와 공성 동행 부대를 다른 아군 부대에 요청합니다.'}
    @{Group='combat'; Section='combat'; Key='mission_v2'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='임무 일관성 v2 (공성 시설만·대기 없음·원작 공격 우선)'; Note='공성 목표 성 8칸 안에서만 공성 시설 1개를 짓고 건설 대기로 세우지 않습니다. 원작이 목표 성을 공격 중이면 모드 공성 진행이 끼어들지 않고, 공성 중인 부대는 40일 정지 퇴각에서 제외합니다. 경로상 적 지역을 원작 군세 작업 목록에 넣고, 최대 병력 90%·여유 4만 이상 도시는 연결된 적 성으로 대열 출진합니다.'}
    @{Group='combat'; Section='combat'; Key='ai_max_aggression'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='AI 호전성 최대 (궁극과 같은 공격 검토)'; Note='원작 판단표의 AI 설정 단계를 궁극(4)으로 읽게 하고, 군단 진행 검사의 단계별 턴 건너뛰기를 없앱니다. 다른 원작 판단은 그대로 둡니다. 플레이어 위임 군단에도 적용됩니다.'}
    @{Group='combat'; Section='combat'; Key='ai_wall_block'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='컴퓨터 세력 방벽 건설 금지'; Note='컴퓨터 세력이 방벽·관문을 짓지 않습니다.'}
    @{Group='combat'; Section='combat'; Key='combat_cut'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='컴퓨터 군세의 플레이어 병참선 차단'; Note='모드가 지휘하는 컴퓨터 군세가 플레이어 부대만 골라 병참을 끊습니다. 끄면 이 차단 행동을 하지 않습니다.'}
    @{Group='combat'; Section='combat'; Key='hit_response'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='피격 대응 (적 성·관문 피격 시 공성 고정, 피격 부대 보호)'; Note='끄면 피격 기록을 남기지 않아 피격에 따른 공성 고정·호위 제외·원거리 피격 장애물 판단이 모두 꺼집니다.'}
    @{Group='combat'; Section='combat'; Key='obstacle'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='중립 장애물 파괴 판단'; Note='왕복 정체 경로의 중립 방벽만 파괴합니다(플레이어 원거리 공격 경로 사유는 ‘‘피격 대응’’도 켜져 있어야 함). 끄면 원작 명령을 그대로 둡니다.'}
    @{Group='combat'; Section='combat'; Key='recall_veto'; Default=1; Requires=''; Inherit=''; Tab='전투'; Label='갓 출진한 공격 군단의 회군 방지'; Note='출진 10일 안의 3,000명 이상 공격 군단을 원작 군단 AI가 바로 되돌리지 못하게 합니다.'}
    @{Group=''; Section='groups'; Key='convenience'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='편의·게임성 기능 전체 (SAN14_Convenience.dll)'; Note='끄면 편의성·게임성 탭과 성능 탭의 빠른 저장이 모두 꺼집니다. 논공·명품창고 저장 자료는 보존됩니다.'}
    @{Group='convenience'; Section='battle_rules'; Key='battle_rules'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='숙련 모드 공방 설정 + 인접지역 효과'; Note='숙련 설정의 값을 조정하는 기능이며 무장을 추가하지 않습니다. 같은 계산을 공유해 하나로 묶었습니다. 바꾼 세이브는 켠 채로 이어 하세요.'}
    @{Group='convenience'; Section='personnel'; Key='awards'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='명품창고·6슬롯·명품/관직 수여'; Note='플레이어가 하던 명품 관리를 편하게 합니다. 꺼도 창고 저장 자료는 보존됩니다.'}
    @{Group='convenience'; Section='personnel'; Key='recruit'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='등용 보조 (등용 화면·자동 등용·결과 안내)'; Note='플레이어 등용 화면과 실행이 같은 판정을 써서 하나로 묶었습니다.'}
    @{Group='convenience'; Section='domestic'; Key='region_officers'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='월드맵 지역담당관 초상화'; Note='표시 전용. 공석은 빈 초상화·회색 테두리. 임명은 지역내정에서만 가능합니다.'}
    @{Group='convenience'; Section='domestic'; Key='region_officer_traits'; Default=1; Requires='domestic.region_officers'; Inherit=''; Tab='편의성'; Label='지역담당관 이름 옆 내정 관련 개성 표시'; Note='표시 전용.'}
    @{Group='convenience'; Section='extensions'; Key='battle_deaths'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='AI 전사 연출·게임 기록'; Note='컴퓨터끼리의 전투에서 일어난 전사를 보여 주고 기록만 합니다.'}
    @{Group='convenience'; Section='extensions'; Key='captive_dialog'; Default=1; Requires=''; Inherit='extensions.traits9'; Tab='편의성'; Label='포획 대사 (플레이어 관련 포획 장면 표시)'; Note='포로나 포획한 쪽 중 하나가 플레이어 세력이면 원작 포획 대화를 보여 줍니다.'}
    @{Group='convenience'; Section='officer_ui'; Key='loyalty'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='숨겨진 충성도 숫자 표시'; Note=''}
    @{Group='convenience'; Section='officer_ui'; Key='damage_preview'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='전투력대체 예상 피해량'; Note='적 대상 선택 시 칼 등급 대신 양쪽의 1회 병사 피해를 표시합니다. 성·관문·시설의 내구 피해도 표시합니다. 현재 조건의 단일 교전 예상이며 전법·미래 상태 변화는 제외합니다. 전투 규칙은 바꾸지 않습니다.'}
    @{Group='convenience'; Section='extensions'; Key='tooltip_collab'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='개성·전법 툴팁 + 콜라보 개성/전법 해금'; Note='콜라보 해금이 툴팁 초기화에 실려 하나로 묶었습니다.'}
    @{Group='convenience'; Section='game_flow'; Key='speed_buttons'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='진행 속도 버튼 >>>>(×4)·>>>>>(×8)'; Note='기존 >>> 대비 배수. 게임 옵션을 60fps로 두면 ×4는 매 프레임 진행, ×8은 하루 처리가 끝난 프레임에 한 번 더 진행합니다(30fps는 최대 ×4).'}
    @{Group='convenience'; Section='game_flow'; Key='occupy_fast'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='거점 자동점령 연출 단축 (원작의 약 1/4)'; Note=''}
    @{Group='convenience'; Section='game_flow'; Key='advisor_briefing'; Default=0; Requires=''; Inherit=''; Tab='편의성'; Label='군사의 열흘 보고 (개발 후보)'; Note='지난 10일의 소식을 좌하단 군사 알림함에 모읍니다. 다음 진행 때 비워지며, 외교·처분 등 선택 창은 유지합니다. 실게임 확인 전 기본 OFF.'}
    @{Group='convenience'; Section='game_flow'; Key='caravan_stay'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='교역 무장 소재 고정 (원래 도시에 남고 각자 도시로 귀환)'; Note=''}
    @{Group='convenience'; Section='officer_ui'; Key='stat_exp'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='능력치 경험치 표시 (오각형 그래프 숫자 뒤 /N%)'; Note=''}
    @{Group='convenience'; Section='officer_ui'; Key='dispatch_days'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='파견 남은 일수 표시 (파견 목록에 귀환까지 N일)'; Note=''}
    @{Group='convenience'; Section='officer_ui'; Key='mediation_plus'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='중개 확장 (의형제 3인, 일부다처, 이혼)'; Note=''}
    @{Group='convenience'; Section='officer_ui'; Key='drag_learn'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='전법서 습득 다중 선택 (여러 무장 × 여러 전법서 한 번에)'; Note=''}
    @{Group='convenience'; Section='officer_ui'; Key='lord_change'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='시작 전 군주 변경 (세력 선택 하단 「반란」 버튼)'; Note=''}
    @{Group='convenience'; Section='extensions'; Key='scenery_skip'; Default=1; Requires=''; Inherit=''; Tab='편의성'; Label='명승지 연출 생략'; Note=''}
    @{Group='convenience'; Section='officer_ui'; Key='unified'; Default=0; Requires=''; Inherit=''; Tab='편의성'; Label='통합 인사·무장 검색·출진 전법 자동 선택 (시험 중)'; Note=''}
    @{Group=''; Section='groups'; Key='performance'; Default=1; Requires=''; Inherit=''; Tab='성능'; Label='성능 가속 전체'; Note='원작과 같은 결과를 더 빠르게 계산합니다.'}
    @{Group='performance'; Section='performance'; Key='distance'; Default=1; Requires=''; Inherit=''; Tab='성능'; Label='거리·AI 진입 계산 가속'; Note=''}
    @{Group='performance'; Section='performance'; Key='name_lookup'; Default=1; Requires=''; Inherit=''; Tab='성능'; Label='원작 이름 탐색 가속'; Note=''}
    @{Group='performance'; Section='performance'; Key='layout_keep'; Default=1; Requires=''; Inherit=''; Tab='성능'; Label='화면 레이아웃 보관 (턴 전환 로딩 단축)'; Note=''}
    @{Group='performance'; Section='performance'; Key='fast_menu'; Default=1; Requires=''; Inherit=''; Tab='성능'; Label='지도 메뉴 클릭 대기 감소 (메뉴를 닫은 뒤 바로 다음 클릭)'; Note=''}
    @{Group='performance'; Section='performance'; Key='appear_batch'; Default=1; Requires=''; Inherit=''; Tab='성능'; Label='부대 등장 묶음 처리 (전략중 뒤 0일 대기 단축)'; Note='전략중이 끝난 뒤 출진 부대의 지도 깃발·정보 패널을 한 프레임에 여러 개 만듭니다(원작은 프레임당 1개, 무거우면 1프레임 쉼). 부대·순서·결과는 원작과 같고 등장 표시만 빨라집니다.'}
    @{Group='performance'; Section='performance'; Key='area_index'; Default=1; Requires=''; Inherit=''; Tab='성능'; Label='지역 칸 목록 캐시 (AI 건설 검사 가속)'; Note='컴퓨터 AI가 시설 건설 자리를 고를 때 지역 시설 수를 셀 때마다 맵 전체(48,400칸)를 훑던 것을, 지역별 칸 목록을 미리 만들어 두고 그 지역 칸만 셉니다. 결과는 원작과 같고 전략중 AI 계산이 빨라집니다.'}
    @{Group='performance'; Section='performance'; Key='path_fast'; Default=1; Requires=''; Inherit=''; Tab='성능'; Label='경로 탐색 가속'; Note='원작 부대 경로 탐색(A*)의 다음 후보 목록을 힙으로 바꿔 빠르게 합니다. 경유지가 하나인 경로는 원작과 같은 결과입니다. 경유지가 여럿인 경로는 원작의 이전 구간 상태가 섞이던 동작을 없애 구간마다 새로 찾습니다(원작과 조금 다를 수 있음). 끄면 원작으로 돌아갑니다.'}
    @{Group='performance'; Section='performance'; Key='face_keep'; Default=1; Requires=''; Inherit=''; Tab='성능'; Label='부대 깃발 지휘관 얼굴 상주 (0일 대기 단축)'; Note='출진 부대 깃발에 불러온 지휘관 얼굴 그림을 남겨, 자주 출진하는 지휘관의 얼굴을 다시 불러오는 시간을 줄입니다. 최대 256명(약 600MB 이하), 상한이면 오래 안 쓴 얼굴부터 교체, 메모리가 부족하면(사용률 85% 이상 또는 남은 램 1.5GB 미만) 절반을 해제하고 새로 붙잡지 않음. 적재 대기 중인 얼굴은 해제하지 않습니다. 표시와 게임 결과는 원작과 같습니다.'}
    @{Group='performance'; Section='performance'; Key='face_prefetch'; Default=1; Requires='performance.face_keep'; Inherit=''; Tab='성능'; Label='출진 지휘관 얼굴 미리 요청 (0일 대기 단축)'; Note='부대가 나타나기 전에 출진 지휘관 얼굴을 한꺼번에 요청해, 얼굴마다 한 프레임씩 기다리던 시간을 겹칩니다. 깃발에 붙이기 직전에는 원작과 같이 적재 완료를 확인합니다. 부대 등장 묶음 처리가 켜져 있을 때 동작하며, 미리 요청한 얼굴은 상주 상한에 포함됩니다.'}
    @{Group='convenience'; Section='game_flow'; Key='fast_save'; Default=1; Requires=''; Inherit=''; Tab='성능'; Label='빠른 자동저장 압축'; Note='원작 형식 그대로 저장하며, 실패 시 원작 압축을 사용합니다. 편의 DLL이 담당합니다.'}
    @{Group='performance'; Section='progress_native'; Key='geometry'; Default=1; Requires=''; Inherit=''; Tab='성능'; Label='거리 계산 세부 가속'; Note='거리·AI 진입 가속 안에서 기하 계산을 같은 결과로 더 빠르게 처리합니다.'}
    @{Group='convenience'; Section='stratagem'; Key='building_destruction_all'; Default=0; Requires=''; Inherit=''; Tab='게임성'; Label='건물파괴 지역 전체 피해'; Note='건물파괴 성공 시 선택 지역의 모든 적 군사시설·장애물·함정에 원작에서 계산된 피해량을 각각 적용합니다. 성·관문·아군·동맹·중립 건물은 제외하며, 실패 시에는 원작 처리를 유지합니다. 플레이어와 컴퓨터 모두 적용됩니다.'}
    @{Group='convenience'; Section='combat'; Key='district_intercept_block'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='지원·내정 군단 출진 제한 (플레이어 2군단~)'; Note='플레이어가 위임한 군단의 방침이 지원이면 수송 출진만, 내정이면 요격·지역 점령·공격 출진을 막습니다(수송·건설은 허용). 컴퓨터 군단·직할 1군단·공격 방침·직접 명령·이미 출전한 부대는 그대로입니다. 전투 AI 교정 없이도 적용됩니다.'}
    @{Group='convenience'; Section='personnel'; Key='merit'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='논공 친애 확장'; Note='꺼도 기존 논공 기록·명품창고 저장은 보존됩니다.'}
    @{Group='convenience'; Section='personnel'; Key='awards_ai'; Default=1; Requires='personnel.awards'; Inherit=''; Tab='게임성'; Label='컴퓨터 명품 자동 배분'; Note='컴퓨터 세력·플레이어 위임 군단이 창고 명품을 능력에 맞게 배분합니다. 군주 직할 군단(1군단)은 제외.'}
    @{Group='convenience'; Section='personnel'; Key='recruit_ai'; Default=1; Requires=''; Inherit='personnel.recruit'; Tab='게임성'; Label='컴퓨터·위임 군단 등용 시도 제한'; Note='관계·혐오 등으로 성공할 수 없는 등용 시도를 막습니다.'}
    @{Group='convenience'; Section='personnel'; Key='injury'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='부상 무장 보호'; Note='컴퓨터·위임 군단이 부상 무장을 출진·임무·유언비어에 쓰지 않습니다. 플레이어 직접 명령은 그대로입니다.'}
    @{Group='convenience'; Section='personnel'; Key='injury_herbal'; Default=1; Requires=''; Inherit='personnel.injury'; Tab='게임성'; Label='컴퓨터 세력 본초학 시정 자동 배치'; Note='부상 무장이 있는 컴퓨터 세력만 적용(플레이어 세력의 시정은 건드리지 않음).'}
    @{Group='convenience'; Section='domestic'; Key='domestic'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='자동 내정 — 상업·농업·병영 중 낮은 개발부터 균형 개발'; Note='플레이어가 위임한 2군단 이후와 컴퓨터 세력에 적용됩니다. 군주 직할 1군단은 아래 ‘‘1군단 자동 내정’’을 켰을 때만 적용됩니다.'}
    @{Group='convenience'; Section='domestic'; Key='domestic_direct'; Default=0; Requires='domestic.domestic'; Inherit=''; Tab='게임성'; Label='1군단(군주 직할) 자동 내정'; Note='켜면 플레이어가 직접 내정하는 1군단 지역도 매일 균형 개발 항목으로 바꿉니다(직접 정한 개발 항목을 덮어씀). 명령서 비용 0은 적용되지 않습니다.'}
    @{Group='convenience'; Section='domestic'; Key='domestic_free'; Default=1; Requires='domestic.domestic'; Inherit=''; Tab='게임성'; Label='지역담당관 자동 임명 명령서 비용 0'; Note='플레이어 2군단 이후·컴퓨터 세력의 자동 임명에만 적용. 1군단과 플레이어 수동 임명은 원작 비용 그대로입니다.'}
    @{Group='convenience'; Section='domestic'; Key='population'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='인구 월간 증가 4배'; Note=''}
    @{Group='convenience'; Section='extensions'; Key='traits9'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='개성 9칸'; Note='끄면 원작 5칸만 적용됩니다 (세이브 값 보존).'}
    @{Group='convenience'; Section='extensions'; Key='stats200'; Default=1; Requires=''; Inherit=''; Tab='게임성'; Label='능력치 상한 200'; Note=''}
    @{Group='convenience'; Section='extensions'; Key='stats200_growth'; Default=1; Requires='extensions.stats200'; Inherit=''; Tab='게임성'; Label='능력치 100 이후 자연 성장'; Note=''}
    @{Group=''; Section='groups'; Key='diagnostics'; Default=1; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='개발용 기능 전체'; Note='진단 로그·추적·시간 기록 같은 개발용 기능의 묶음입니다. 끄면 아래 항목이 모두 꺼집니다. 일반 플레이에는 필요 없습니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='enabled'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='진단 로그 기록 (SAN14_*.log)'; Note='모드 판단과 오류를 게임 폴더 로그 파일에 남깁니다. 배포용(로그 제외) DLL에서는 효과가 없습니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='reuse_handle'; Default=1; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='로그 파일 핸들 재사용'; Note='로그를 쓸 때 파일을 매번 다시 열지 않아 기록 비용을 줄입니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='ai_trace'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='원작 AI 행동 추적 (CSV)'; Note='원작 AI 판단 노드가 부대·군세 목표를 바꾸는 과정을 data/ai_trace에 기록합니다. 기록량이 많습니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='ai_trace_original'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='원작만 추적 (모드 훅 연결 안 함)'; Note='모드 기능을 연결하지 않고 원작 AI만 기록합니다. 이 상태로 기존 세이브에 덮어쓰지 마세요.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='layout_wait'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='화면 레이아웃 대기 기록'; Note='턴 전환 때 화면 레이아웃 로딩을 기다린 시간을 기록합니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='day_stage'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='하루 처리 단계 기록'; Note='진행 중 하루 처리 단계가 바뀔 때마다 그 단계에 머문 시간과 실제 계산 시간을 진단 로그(day_stage)에 남깁니다. 전략중 뒤 1일 시작 대기 조사용이며 진행 속도 버튼이 켜져 있어야 기록됩니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='cost_profile'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='함수 시간 측정 (CSV)'; Note='모드 함수와 원작 AI 판단 노드마다 호출 수·시간을 data/performance에 기록합니다. 측정 중에는 약간 느려집니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='area_index_verify'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='지역 칸 캐시 원작 결과 대조'; Note='진단판에서 매 지역 시설 수 검사 결과를 원작 전체 순회와 대조합니다. 불일치 시 원작 결과를 사용하며 측정 중에는 느려집니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='path_fast_verify'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='경로 탐색 가속 결과 비교'; Note='0=끔, 1=매번, N=N번에 한 번. 1은 원작 결과를 유지하며 깨끗한 버퍼와 잔존 상태를 구분합니다. N>1은 전체 호출 순번마다 표본 구간을 깨끗하게 비교하고, 같으면 새 결과·다르면 원작 결과를 씁니다. 1일 때만 지우기 생략을 끕니다. 불일치 상세는 하루 16건, 새 구현만 실행한 시간도 기록합니다. N은 기능설정.ini에서 지정합니다.'}
    @{Group='diagnostics'; Section='diagnostics'; Key='path_count'; Default=0; Requires=''; Inherit=''; Tab='기타(개발용)'; Label='경로 탐색 호출 횟수 기록'; Note='세력 AI가 경로 탐색을 몇 번 부르고 같은 출발지·목적지·조건을 되풀이하는지 진단 로그에 남깁니다. 게임 동작은 바뀌지 않습니다.'}
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
        foreach ($taskSource in @(@('features',$taskItem.Key), @($taskItem.Section,$taskItem.Key))) {
            if ($Ini.ContainsKey($taskSource[0]) -and $Ini[$taskSource[0]].Contains($taskSource[1])) {
                $taskNumber = 0
                if ([int]::TryParse($Ini[$taskSource[0]][$taskSource[1]], [ref]$taskNumber)) { $taskValue = if ($taskItem.Key -eq 'path_fast_verify') { [Math]::Max(0, $taskNumber) } else { [int]($taskNumber -ne 0) } }
            }
        }
        $taskValues[$taskItem.Section + '.' + $taskItem.Key] = $taskValue
    }
    return $taskValues
}
function Write-FeatureIni([string]$Path, $Values, $Ini) {
    $taskLines = New-Object Collections.Generic.List[string]
    $taskLines.Add('; AI 모드 기능 켜기/끄기. 1 = 켜기, 0 = 끄기. 게임을 다시 시작하면 적용됩니다.')
    $taskLines.Add('; 관리 도구: optional_tools\AI모드_기능선택.cmd 또는 AI모드_적용.cmd 실행 중 선택 화면.')
    $taskLines.Add('; 함께 묶인 항목은 같은 게임 코드를 공유하므로 따로 끌 수 없습니다.')
    $taskSections = New-Object Collections.Generic.List[string]
    foreach ($taskItem in $taskFeatureCatalog) { if (-not $taskSections.Contains($taskItem.Section)) { $taskSections.Add($taskItem.Section) } }
    foreach ($taskSection in $taskSections) {
        $taskLines.Add(''); $taskLines.Add('[' + $taskSection + ']')
        foreach ($taskItem in ($taskFeatureCatalog | Where-Object { $_.Section -eq $taskSection })) {
            $taskComment = '; ' + $taskItem.Label
            if ($taskItem.Note) { $taskComment += ' — ' + $taskItem.Note }
            $taskLines.Add($taskComment)
            $taskLines.Add($taskItem.Key + '=' + $Values[$taskItem.Section + '.' + $taskItem.Key])
        }
    }
    # Sections the menu does not own (diagnostics, hidden keys) are kept as they were.
    $taskOwned = @{}; foreach ($taskItem in $taskFeatureCatalog) { $taskOwned[$taskItem.Section + '.' + $taskItem.Key] = $true }
    foreach ($taskSection in $Ini.Keys) {
        if ($taskSection -eq 'features') { continue }
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
        $taskOn = $Values[$taskItem.Section + '.' + $taskItem.Key] -ne 0
        $taskParentOff = $taskItem.Group -and $Values['groups.' + $taskItem.Group] -eq 0
        $taskMark = if ($taskOn) { '[켬]' } else { '[끔]' }
        $taskIndent = if ($taskItem.Group) { '    ' } else { '' }
        $taskColor = if ($taskParentOff) { 'DarkGray' } elseif (-not $taskItem.Group) { 'Yellow' } elseif ($taskOn) { 'Green' } else { 'Gray' }
        if (-not $taskItem.Group) { Write-Host '' }
        Write-Host ('{0}{1,2}. {2} {3}' -f $taskIndent, $taskNumber, $taskMark, $taskItem.Label) -ForegroundColor $taskColor
        if ($taskItem.Note) { Write-Host ('{0}       {1}' -f $taskIndent, $taskItem.Note) -ForegroundColor DarkGray }
    }
    Write-Host ''
    Write-Host '번호를 입력하면 켬/끔이 바뀝니다 (여러 개: 4 7 12). 회색 항목은 묶음이 꺼져 있어 동작하지 않습니다.'
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
        if ($taskInput -ieq 'a') { foreach ($taskItem in $taskFeatureCatalog) { $Values[$taskItem.Section + '.' + $taskItem.Key] = 1 }; continue }
        if ($taskInput -ieq 'd') { foreach ($taskItem in $taskFeatureCatalog) { $Values[$taskItem.Section + '.' + $taskItem.Key] = $taskItem.Default }; continue }
        foreach ($taskToken in ($taskInput -split '[\s,]+')) {
            $taskIndex = 0
            if ([int]::TryParse($taskToken, [ref]$taskIndex) -and $taskIndex -ge 1 -and $taskIndex -le $taskFeatureCatalog.Count) {
                $taskItem = $taskFeatureCatalog[$taskIndex-1]; $taskName = $taskItem.Section + '.' + $taskItem.Key
                $Values[$taskName] = [int]($Values[$taskName] -eq 0)
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
