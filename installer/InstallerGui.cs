// AI 모드 설치 관리자 (WinForms, .NET Framework 4.x — Windows 10/11 기본 포함).
// 기능 선택은 <게임>\AI_TacticsMod\기능설정.ini만 바꾸고, 파일 설치·해제는 기존 guarded
// 설치기(AI_TacticsMod\관리.ps1 -NonInteractive)에 맡긴다. 해시·백업·원본 검사 로직을
// 이 EXE에 복제하지 않는다. 게임 폴더가 패키지(이 EXE가 있는 곳)와 다르면
// AI_TacticsMod\package_files.txt에 적힌 배포 파일만 게임 폴더로 복사한다.
// 항목 목록은 distribution/FeatureSelect.ps1과 같아야 하며 tests/verify_installer_gui.py가
// 섹션·키·기본값·묶음·종속 관계를 대조한다.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Threading;
using System.Web.Script.Serialization;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;

[assembly: System.Reflection.AssemblyTitle("SAN14 AI 모드 설치 관리자")]
[assembly: System.Reflection.AssemblyProduct("AI_TacticsMod")]
[assembly: System.Reflection.AssemblyVersion("15.4.0.0")]

namespace AiTacticsInstaller {

sealed class Feature {
    public string Tab, Group, Section, Key, Requires, Inherit, Label, Note;
    public int Default;
    public string Id { get { return Section + "." + Key; } }
    public bool IsMaster { get { return Group == ""; } }
    public bool IsChild { get { return Requires != ""; } }
    // group: 필요한 묶음 스위치(groups.*), requires: 먼저 켜져야 하는 상위 기능 Id,
    // inherit: 키가 없는 이전 설정 파일에서 값을 물려받을 원래 묶음 기능 Id.
    public Feature(string tab, string group, string section, string key, int def, string requires, string inherit, string label, string note) {
        Tab = tab; Group = group; Section = section; Key = key; Default = def; Requires = requires; Inherit = inherit; Label = label; Note = note;
    }
}

static class Catalog {
    public static readonly string[] Tabs = { "게임렉제거(성능)", "전투", "편의성", "게임성", "조조전", "기타(개발용)" };
    public static readonly Feature[] Items = {
        new Feature("편의성", "convenience", "real_estate", "terrain_edit", 1, "game_flow.advisor_briefing", "", "부동산 계획: 지형 변경", "군사의 열흘 보고 창에 「부동산 개발」 버튼을 추가합니다. 보유한 성 목록에서 성을 고르면 그 성의 지역내정으로 들어가고, 땅을 클릭해 바닥·높이·나무·절벽·수면을 무료로 바꿀 수 있습니다. 일반 지역내정의 땅클릭은 원작 그대로이며, 이 버튼으로 들어왔을 때만 지형 변경이 열립니다. 끄면 버튼과 지형 변경이 모두 사라집니다."),
        new Feature("게임성", "convenience", "diplomacy", "diplomacy_overinvest", 0, "", "", "외교 군정 100% 초과 투입", "컴퓨터가 시작한 친선·동맹·포로반환·공격요청 교섭에서 보유 군정까지 투입합니다. 100%를 넘긴 방어 효과는 상대의 추가 투입 후에도 유지됩니다."),
        new Feature("게임성", "convenience", "diplomacy", "diplomacy_counterturn", 1, "", "", "외교 군정 마지막 추가 대응", "컴퓨터의 마지막 응답 뒤 플레이어가 한 번 더 군정을 투입할 수 있습니다. 추가 대응 후에는 바로 판정합니다."),
        new Feature("전투", "", "groups", "combat", 1, "", "", "전투 AI 교정 전체 (SAN14_CombatAI.dll)", "출진 편성·군세 이동·공성·병참·퇴각을 하나의 진행 상태로 관리합니다. 끄면 전투 DLL을 불러오지 않습니다."),
        new Feature("전투", "combat", "combat", "sortie_formation", 1, "", "", "군세 출진 편성 (병력별 2~7부대·공성 진형·1,000명 지원)", "컴퓨터가 출진할 때 병력에 맞춰 2~7부대, 공성 진형, 1,000명 지원 부대를 편성합니다. 여무장과 통솔 60 이하 무장은 1,000명으로 지원 출진하고, 통솔 60을 넘는 범위 개성(효장·용장·명경·독려 등) 무장은 출진 명단에 들며, 주장과 친애인 무장을 먼저 고릅니다. 출진 자체의 판단은 원작이며, 끄면 원작 편성을 씁니다."),
        new Feature("전투", "combat", "combat", "departures", 1, "", "", "정기 선제 출진 검토 (10일마다)", "임무 일관성 v2와 충돌해 사용이 중지된 기능입니다."),
        new Feature("전투", "combat", "combat", "group_advance", 1, "", "", "집결 후 30일 정지 군세의 집단 진군", "임무 일관성 v2와 충돌해 사용이 중지된 기능입니다."),
        new Feature("전투", "combat", "combat", "stationary_retreat", 1, "", "", "같은 자리 40일 정지 부대 퇴각", "같은 자리에서 4턴(40일) 동안 움직이지 못한 부대를 귀환시킵니다. 공성 중인 부대 등 예외는 원작 규칙을 따르며, 끄면 원작처럼 계속 머뭅니다."),
        new Feature("전투", "combat", "combat", "morale", 1, "", "", "사기 회복 우선", "컴퓨터 세력만. 사기 85 이하인 부대가(70 이하이거나 산지면 더 먼 군악대도 찾음) 가까운 자기 군악대 옆에서 사기 100까지 기다리고, 없으면 원작 건설 절차로 군악대를 짓습니다. 원작이 경로를 정한 직후에만 명령을 고치며, 전투 규칙과 피해 계산은 원작 그대로입니다."),
        new Feature("전투", "combat", "combat", "support", 1, "", "", "호위·동행 지원 요청", "임무 일관성 v2와 충돌해 사용이 중지된 기능입니다."),
        new Feature("전투", "combat", "combat", "mission_v2", 1, "", "", "임무 일관성 v2 (공성 시설만·대기 없음·원작 공격 우선)", "공성 목표 성 10칸 안 자기 땅에서, 성에서 3~6칸(성 공격 범위 밖) 지점에만 공성 시설 1개를 짓고 건설 대기로 세우지 않습니다. 원작이 목표 성을 공격 중이면 모드 공성 진행이 끼어들지 않고, 공성 중인 부대는 40일 정지 퇴각에서 제외합니다. 경로상 적 지역을 원작 군세 작업 목록에 넣고, 최대 병력 90%·여유 4만 이상 도시는 연결된 적 성으로 대열 출진합니다."),
        new Feature("전투", "combat", "combat", "siege_stall_near", 0, "combat.mission_v2", "", "공성 정체: 성 1칸 안만 공성 중", "컴퓨터 세력만. 성 1칸 안(또는 최근 타격)일 때만 공성 진행으로 봅니다. 끄면 이전 판."),
        new Feature("전투", "combat", "combat", "siege_native", 0, "", "", "공성 진행을 원작에 맡기기", "공성 진행은 다시 모드 임무 일관성이 맡아 제거되었습니다."),
        new Feature("전투", "combat", "combat", "siege_ring10", 0, "combat.mission_v2", "", "공성 시설 링 3~10칸", "컴퓨터 세력만. 공성 시설을 성에서 3~10칸에 짓고, 군악대는 부대에서 6칸 안에 짓습니다. 끄면 3~6칸(이전 판)."),
        new Feature("전투", "combat", "combat", "siege_no_catapult", 0, "combat.mission_v2", "", "공성 건설에서 투석대 제외", "컴퓨터 세력만. 공성 시설을 지을 때 투석대를 짓지 않습니다. 투석대 자리는 망루로 채우고, 망루도 제외하면 그 자리는 비워 둡니다. 끄면 투석대를 먼저 시도합니다."),
        new Feature("전투", "combat", "combat", "siege_no_watchtower", 0, "combat.mission_v2", "", "공성 건설에서 망루 제외", "컴퓨터 세력만. 투석대를 짓지 못할 때 대신 짓던 망루를 짓지 않습니다. 투석대도 제외하면 그 자리는 비워 두고 함정 단계로 넘어갑니다. 끄면 망루를 짓습니다."),
        new Feature("전투", "combat", "combat", "siege_no_trap", 0, "combat.mission_v2", "", "공성 건설에서 함정 제외", "컴퓨터 세력만. 공성 지역의 시설 자리(투석대·망루, 요새·진, 군악대)가 다 찬 뒤 부대마다 짓던 함정(불·맹화·낙석·구덩이·화선)을 짓지 않습니다. 끄면 함정을 짓습니다."),
        new Feature("전투", "combat", "combat", "builder_ruler_fill", 0, "combat.mission_v2", "", "건설 성향 군주(내면 건설 5)의 시설 채우기", "켜면 내면 건설이 5인 컴퓨터 군주의 부대가 공성지 제한 없이 지역이 가득 찰 때까지 시설을 짓습니다(이전 동작). 끄면 다른 군주처럼 공성 목표 근처에 1개만 짓습니다. 임무 일관성 v2가 켜져 있어야 동작합니다."),
        new Feature("전투", "combat", "combat", "ai_max_aggression", 1, "", "", "AI 호전성 최대 (궁극과 같은 공격 검토)", "원작 판단표의 AI 설정 단계를 궁극(4)으로 읽게 하고, 군단 진행 검사의 단계별 턴 건너뛰기를 없앱니다. 다른 원작 판단은 그대로 둡니다. 플레이어 위임 군단에도 적용됩니다."),
        new Feature("전투", "combat", "combat", "ai_wall_block", 1, "", "", "컴퓨터 세력 방벽 건설 금지", "컴퓨터 세력이 방벽·관문을 새로 짓지 않게 합니다. 플레이어 건설과 이미 지어진 시설은 그대로이며, 끄면 원작 AI가 다시 지을 수 있습니다."),
        new Feature("전투", "combat", "combat", "ai_catapult_block", 0, "", "", "플레이어 성 주변 컴퓨터 투석대 건설 불가", "켜면 플레이어 성·관문이 있는 지역과 바로 붙은 지역(점령자 상관없음)에서 컴퓨터 세력이 투석대를 지을 수 없습니다. 그곳에서는 시정 Lv7처럼 요새가 최고 시설이 되어, 컴퓨터 부대는 요새 등 다른 시설을 짓거나 공성을 계속합니다. 그 밖의 지역, 컴퓨터끼리의 전투, 플레이어의 투석대는 원작 그대로입니다."),
        new Feature("전투", "combat", "combat", "combat_cut", 1, "", "", "컴퓨터 군세의 플레이어 병참선 차단", "모드가 지휘하는 컴퓨터 군세가 플레이어 부대만 골라 병참을 끊습니다. 끄면 이 차단 행동을 하지 않습니다."),
        new Feature("전투", "combat", "combat", "hit_response", 1, "", "", "피격 대응 (적 성·관문 피격 시 공성 고정, 피격 부대 보호)", "끄면 피격 기록을 남기지 않아 피격에 따른 공성 고정·호위 제외·원거리 피격 장애물 판단이 모두 꺼집니다."),
        new Feature("전투", "combat", "combat", "obstacle", 1, "", "", "중립 장애물 파괴 판단", "왕복 정체 경로의 중립 방벽만 파괴합니다. 플레이어 원거리 공격 경로 사유는 「피격 대응」도 켜져 있어야 하며, 끄면 원작 명령을 그대로 둡니다."),
        new Feature("전투", "combat", "combat", "path_wall", 0, "combat.obstacle", "", "중립 방벽 돌파(최단 경로를 막을 때만)", "컴퓨터 세력만. 토성·석벽·철쇄가 목표 성까지의 최단 경로를 막고, 방벽이 없다고 친 최단 경로보다 돌아가는 길이 5칸·4분의 1 이상 길거나 길이 없을 때 가장 가까운 부대 하나가 부숩니다. 또 출신 성마다 출진하는 5번째 부대는 그 성 옆 중립 방벽 하나를 부수러 갑니다. 명령은 원작이 경로를 정할 때마다 다시 걸어 방벽이 무너질 때까지 유지합니다. 끄면 이전 판 장애물 규칙."),
        new Feature("전투", "combat", "combat", "supply_relay", 1, "", "", "목표 변경 시 병참 중계점(지역담당관 자동점령 대비)", "컴퓨터 세력만. 적 담당관이 있는 적 지역 안에 우리 부대가 하나뿐인데 목표가 바뀌어 한 순 안에 그 지역을 떠나게 되면, 턴 처리 때 담당관이 칸을 되찾아 병참이 끊깁니다. 목표가 바뀔 때만 뒤쪽 아군 지역의 아군 칸 하나를 첫 경유점으로 넣어 병참을 이은 뒤 새 목표로 갑니다. 목표는 원작 그대로입니다. 끄면 원작."),
        new Feature("전투", "combat", "combat", "no_tribe_target", 1, "", "", "이민족 성 공격 금지", "컴퓨터 세력만. 원작에서 플레이어가 아닌 세력은 이민족 성을 차지할 수 없으므로, 원작 공격 검토와 모드의 목표 선택 모두에서 이민족 성을 목표로 삼지 않습니다. 이미 향하던 군단은 원작 재검토에서 물러납니다. 끄면 원작."),
        new Feature("전투", "combat", "combat", "recall_veto", 1, "", "", "갓 출진한 공격 군단의 회군 방지", "출진한 지 10일 안이고 병력 3,000명 이상인 공격 군단을 원작 군단 AI가 바로 회군시키지 못하게 합니다. 10일이 지나면 원작 판단을 따릅니다."),
        new Feature("편의성", "", "groups", "convenience", 1, "", "", "편의·게임성 기능 전체 (SAN14_Convenience.dll)", "끄면 편의성·게임성 탭과 게임렉제거(성능) 탭의 빠른 저장이 모두 꺼집니다. 논공·명품창고 저장 자료는 보존됩니다."),
        new Feature("편의성", "convenience", "battle_rules", "battle_rules", 1, "", "", "숙련 모드 공방 설정 + 인접지역 효과", "숙련 설정의 값을 조정하는 기능이며 무장을 추가하지 않습니다. 같은 계산을 공유해 하나로 묶었습니다. 바꾼 세이브는 켠 채로 이어 하세요."),
        new Feature("편의성", "convenience", "battle_rules", "expert_ingame", 0, "battle_rules.battle_rules", "", "숙련 설정 게임 중 변경", "게임 중 설정 화면에서 숙련 네 줄을 바꾸고 확인으로 적용합니다. 끄면 원작(읽기 전용)."),
        new Feature("편의성", "convenience", "personnel", "awards", 1, "", "", "명품창고·6슬롯·명품/관직 수여", "명품창고를 6칸으로 넓히고 명품·관직 수여를 편하게 합니다. 창고 명품은 소유자가 「세력창고」로 표시되고, 군주도 슬롯마다 가장 강한 명품 1개만 착용하고 나머지는 세력창고로 갑니다(컴퓨터 자동분배는 군주 착용품을 건드리지 않음). 창고에 저장된 자료는 꺼도 보존되며, 원작의 명품·관직 규칙 자체는 없애지 않습니다."),
        new Feature("편의성", "convenience", "personnel", "recruit", 1, "", "", "등용 보조 (등용 화면·자동 등용·결과 안내)", "등용 화면과 자동 등용이 같은 판정을 쓰도록 묶고 결과를 안내합니다. 성공 확률과 조건은 원작 계산을 따르며, 끄면 원작 등용 화면만 씁니다."),
        new Feature("편의성", "convenience", "domestic", "region_officers", 1, "", "", "월드맵 지역담당관 초상화", "월드맵 도시·지역에 담당관 초상화를 표시합니다. 공석은 빈 초상화와 회색 테두리로 보이고, 임명은 지역내정에서만 할 수 있습니다(표시 전용)."),
        new Feature("편의성", "convenience", "domestic", "region_officer_traits", 1, "domestic.region_officers", "", "지역담당관 이름 옆 내정 관련 개성 표시", "지역담당관 이름 옆에 내정에 도움이 되는 개성을 표시합니다. 표시 전용이며, 위의 「월드맵 지역담당관 초상화」를 켜야 함께 보입니다."),
        new Feature("편의성", "convenience", "extensions", "battle_deaths", 1, "", "", "AI 전사 연출·게임 기록", "컴퓨터끼리의 전투에서 일어난 전사를 보여 주고 기록만 합니다. 출처: 삼갤 통합패치(180.71·175.115 유동)"),
        new Feature("편의성", "convenience", "officer_ui", "loyalty", 1, "", "", "숨겨진 충성도 숫자 표시", "무장의 숨겨진 충성도를 숫자로 표시합니다. 원작에서 아이콘으로만 보이던 값을 숫자로 확인할 수 있고, 충성도 계산과 등용 판정은 바꾸지 않습니다(표시 전용)."),
        new Feature("편의성", "convenience", "officer_ui", "damage_preview", 1, "", "", "전투력대체 예상 피해량", "적 대상 선택 시 칼 등급 대신 양쪽의 1회 병사 피해를 표시합니다. 성·관문·시설의 내구 피해도 표시합니다. 현재 조건의 단일 교전 예상이며 전법·미래 상태 변화는 제외합니다. 전투 규칙은 바꾸지 않습니다."),
        new Feature("편의성", "convenience", "extensions", "tooltip_collab", 1, "", "", "개성·전법 툴팁 + 콜라보 개성/전법 해금", "콜라보 해금이 툴팁 초기화에 실려 하나로 묶었습니다. 출처: 삼갤 통합패치(180.71·175.115 유동)"),
        new Feature("편의성", "convenience", "game_flow", "speed_buttons", 1, "", "", "진행 속도 버튼 >>>>(×4)·>>>>>(×8)", "OFF이면 ×4·×8 버튼과 배치도 원작으로 복원합니다. 게임 종료 후 저장/적용하세요. 기존 >>> 대비 배수. 게임 옵션을 60fps로 두면 ×4는 매 프레임 진행, ×8은 하루 처리가 끝난 프레임에 한 번 더 진행합니다(30fps는 최대 ×4)."),
        new Feature("편의성", "convenience", "game_flow", "occupy_fast", 1, "", "", "거점 자동점령 연출 단축 (원작의 약 1/4)", "거점을 점령할 때 나오는 연출을 원작의 약 1/4 길이로 줄입니다. 점령 결과·수치·턴 진행은 바뀌지 않으며, 끄면 원작 연출을 씁니다."),
        new Feature("편의성", "convenience", "game_flow", "advisor_briefing", 1, "", "", "군사의 열흘 보고", "지난 10일의 소식을 좌하단 군사 알림함에 모읍니다. 다음 진행 때 비워지며, 외교·처분 등 선택 창은 유지합니다."),
        new Feature("편의성", "convenience", "game_flow", "briefing16", 0, "game_flow.advisor_briefing", "", "군사보고 변경", "등용 성공·포박 장면은 원작대로 보여 주고, 군사 초상화는 원작 얼굴 그림을 씁니다. 알림 15초. 끄면 이전 판."),
        new Feature("편의성", "convenience", "game_flow", "caravan_stay", 1, "", "", "교역 무장 소재 고정 (원래 도시에 남고 각자 도시로 귀환)", "교역하러 간 무장이 원래 도시에 남아 있고, 교역이 끝나면 각자 자기 도시로 돌아가게 합니다. 교역 목록과 이동만 바뀌며 다른 외교·내정은 원작 그대로입니다. 출처: 삼갤 주딱"),
        new Feature("편의성", "convenience", "game_flow", "auto_search", 0, "", "", "탐색 위임 (화면 왼쪽 아래 버튼)", "플레이어 전용. 왼쪽 아래 탐색위임 버튼을 켜면 군사가 인재 > 탐색 > 자동탐색에 정해 둔 조건으로 턴마다 한 번, 진행 직전에 탐색을 보냅니다. 켜짐 상태는 세이브마다 따로 저장됩니다. 명령서가 모자라면 보내지 않습니다. 두 위임을 모두 끄면 버튼과 화면 자료가 빠져 원작 화면입니다."),
        new Feature("편의성", "convenience", "game_flow", "auto_reward", 0, "", "", "포상 위임 (화면 왼쪽 아래 버튼)", "플레이어 전용. 왼쪽 아래 포상위임 버튼을 켜면 충성도 기준을 입력받고, 턴마다 한 번 군사가 기준 이하 장수(모든 군단)에게 군주가 있는 성의 금으로 원작 포상(1명당 금 100, 명령서 1)을 줍니다. 금이 모자라면 충성이 낮은 장수부터 줍니다. 켜짐 상태는 세이브마다 따로 저장됩니다. 두 위임을 모두 끄면 버튼과 화면 자료가 빠져 원작 화면입니다."),
        new Feature("편의성", "convenience", "officer_ui", "stat_exp", 1, "", "", "능력치 경험치 표시 (오각형 그래프 숫자 뒤 /N%)", "무장 능력치 오각형 그래프의 숫자 뒤에 현재 경험치와 다음 성장까지의 %를 표시합니다. 표시 전용이며 능력치 계산은 원작 그대로입니다. 출처: 삼갤 주딱"),
        new Feature("편의성", "convenience", "officer_ui", "dispatch_days", 1, "", "", "파견 남은 일수 표시 (파견 목록에 귀환까지 N일)", "파견 목록에서 각 무장이 돌아오기까지 남은 일수를 N일로 표시합니다. 표시 전용이며 파견 기간·결과는 원작 그대로입니다. 출처: 삼갤 주딱"),
        new Feature("편의성", "convenience", "officer_ui", "mediation_plus", 1, "", "", "중개 확장 (의형제 3인, 일부다처, 이혼)", "중개 화면에서 의형제를 3명까지 맺고, 일부다처와 이혼을 선택할 수 있게 확장합니다. 원작 중개 조건과 비용은 그대로이며, 끄면 원작 선택지만 나옵니다. 출처: 삼갤 주딱"),
        new Feature("편의성", "convenience", "officer_ui", "drag_learn", 1, "", "", "전법서 습득 다중 선택 (여러 무장 × 여러 전법서 한 번에)", "전법서 습득 화면에서 여러 무장과 여러 전법서를 한 번에 선택할 수 있게 합니다. 실제 습득 가능 여부·비용·결과는 원작 판정을 따릅니다. 출처: 삼갤 주딱"),
        new Feature("편의성", "convenience", "officer_ui", "drag_learn_multi", 0, "officer_ui.drag_learn", "", "학습 창 여러 명 표시", "여러 명을 골라 학습할 때 확인 창에 외 N명과 전법서·명령서 합계를 보여 줍니다. 끄면 이전 판."),
        new Feature("편의성", "convenience", "officer_ui", "lord_change", 1, "", "", "시작 전 군주 변경 (세력 선택 하단 「반란」 버튼)", "세력 선택 화면 아래에 「반란」 버튼을 추가해 게임을 시작하기 전에 군주를 바꿀 수 있습니다. 시작한 뒤에는 원작 군주 교체 규칙만 적용됩니다. 출처: 삼갤 주딱"),
        new Feature("편의성", "convenience", "officer_ui", "tactic_priority", 1, "", "", "출진 화면 전법 우선순위·출진 기억", "출진 편성 화면에 「전법 우선순위」 버튼을 추가합니다. 무력형·지력형별로 전법 순서를 정하고, 무장별 「출진 기억」을 켜면 마지막 출진의 진형과 전법을 다음 출진에 되살립니다. 칸에 마우스를 올리면 나오는 전법 설명은 편의성의 「개성·전법 툴팁 + 콜라보…」가 켜져 있을 때만 표시되며, 그 기능이 꺼져 있어도 순위 지정과 출진 기억은 그대로 동작합니다. 끄면 원작 출진 화면으로 돌아갑니다."),
        new Feature("편의성", "convenience", "extensions", "scenery_skip", 1, "", "", "명승지 연출 생략", "명승지에 들어갈 때 나오는 연출(카메라·안내)을 생략합니다. 게임 결과·능력치·외교에는 영향이 없고, 끄면 원작 연출을 씁니다. 출처: 112.151 유동(삼봉이정도전)의 「11종 선택패치」"),
        new Feature("편의성", "convenience", "officer_ui", "unified", 0, "", "", "통합 인사·무장 검색·출진 전법 자동 선택", "개발 중지로 제거된 기능입니다."),
        new Feature("게임렉제거(성능)", "", "groups", "performance", 1, "", "", "성능 가속 전체", "원작과 같은 결과를 더 빠르게 계산합니다."),
        new Feature("게임렉제거(성능)", "performance", "performance", "distance", 1, "", "", "거리·AI 진입 계산 가속", "AI가 거리와 진입 가능성을 계산하는 부분을 원작과 같은 결과로 더 빠르게 계산합니다. 전투·내정 판정은 바뀌지 않으며, 끄면 원작 계산을 씁니다."),
        new Feature("게임렉제거(성능)", "performance", "performance", "name_lookup", 1, "", "", "원작 이름 탐색 가속", "무장 이름을 찾는 원작 탐색을 캐시해 반복 검색을 줄입니다. 화면 표시와 판정 결과는 원작과 같고, 끄면 원작 탐색을 씁니다."),
        new Feature("게임렉제거(성능)", "performance", "performance", "layout_keep", 1, "", "", "화면 레이아웃 보관 (턴 전환 로딩 단축)", "턴이 바뀔 때 화면 레이아웃을 보관해 다시 만드는 로딩을 줄입니다. 화면 구성과 표시는 원작과 같고, 끄면 매번 새로 만듭니다."),
        new Feature("게임렉제거(성능)", "performance", "performance", "fast_menu", 1, "", "", "지도 메뉴 클릭 대기 감소 (메뉴를 닫은 뒤 바로 다음 클릭)", "지도 메뉴를 닫은 뒤 다음 클릭이 먹히기까지의 대기를 줄입니다. 메뉴 항목과 동작은 원작 그대로이며, 끄면 원작 대기로 돌아갑니다."),
        new Feature("게임렉제거(성능)", "performance", "performance", "appear_batch", 1, "", "", "부대 등장 묶음 처리 (전략중 뒤 0일 대기 단축)", "전략중이 끝난 뒤 출진 부대의 지도 깃발·정보 패널을 한 프레임에 여러 개 만듭니다(원작은 프레임당 1개, 무거우면 1프레임 쉼). 부대·순서·결과는 원작과 같고 등장 표시만 빨라집니다."),
        new Feature("게임렉제거(성능)", "performance", "performance", "area_index", 1, "", "", "지역 칸 목록 캐시 (AI 건설 검사 가속)", "컴퓨터 AI가 시설 건설 자리를 고를 때 지역 시설 수를 셀 때마다 맵 전체(48,400칸)를 훑던 것을, 지역별 칸 목록을 미리 만들어 두고 그 지역 칸만 셉니다. 결과는 원작과 같고 전략중 AI 계산이 빨라집니다."),
        new Feature("게임렉제거(성능)", "performance", "performance", "path_fast", 1, "", "", "경로 탐색 가속", "원작 부대 경로 탐색(A*)의 다음 후보 목록을 힙으로 바꿔 빠르게 합니다. 경유지가 하나인 경로는 원작과 같은 결과입니다. 경유지가 여럿인 경로는 원작의 이전 구간 상태가 섞이던 동작을 없애 구간마다 새로 찾습니다(원작과 조금 다를 수 있음). 끄면 원작으로 돌아갑니다."),
        new Feature("게임렉제거(성능)", "performance", "performance", "face_keep", 1, "", "", "작은 무장 얼굴 상주 (깃발·선택·전략 화면)", "부대 깃발과 무장 선택·전략 화면에서 불러온 작은 얼굴 그림을 같은 보관함에 남겨 다시 불러오는 시간을 줄입니다. 최대 256명(약 600MB 이하), 상한이면 오래 안 쓴 얼굴부터 교체, 메모리가 부족하면(사용률 85% 이상 또는 남은 램 1.5GB 미만) 절반을 해제하고 새로 붙잡지 않음. 적재 대기 중인 얼굴은 해제하지 않습니다. 표시와 게임 결과는 원작과 같습니다."),
        new Feature("게임렉제거(성능)", "performance", "performance", "face_prefetch", 1, "performance.face_keep", "", "출진 지휘관 얼굴 미리 요청 (0일 대기 단축)", "부대가 나타나기 전에 출진 지휘관 얼굴을 한꺼번에 요청해, 얼굴마다 한 프레임씩 기다리던 시간을 겹칩니다. 깃발에 붙이기 직전에는 원작과 같이 적재 완료를 확인합니다. 부대 등장 묶음 처리가 켜져 있을 때 동작하며, 미리 요청한 얼굴은 상주 상한에 포함됩니다."),
        new Feature("게임렉제거(성능)", "performance", "performance", "timer_1ms", 0, "", "", "1ms 타이머", "측정상 효과가 없어 제거되었습니다."),
        new Feature("게임렉제거(성능)", "convenience", "game_flow", "fast_save", 1, "", "", "빠른 자동저장 압축", "자동저장을 원작과 같은 형식으로 더 빠르게 압축합니다. 실패하면 원작 저장으로 돌아가며, 세이브 호환성은 그대로입니다."),
        new Feature("게임렉제거(성능)", "performance", "progress_native", "geometry", 1, "performance.distance", "", "거리 계산 세부 가속", "거리·AI 진입 가속 안에서 기하 계산을 같은 결과로 더 빠르게 처리합니다. 위의 「거리·AI 진입 계산 가속」을 켜야 함께 동작합니다."),
        new Feature("게임성", "convenience", "stratagem", "building_destruction_all", 0, "", "", "건물파괴 지역 전체 피해", "건물파괴 성공 시 선택 지역의 모든 적 군사시설·장애물·함정에 원작에서 계산된 피해량을 각각 적용합니다. 성·관문·아군·동맹·중립 건물은 제외하며, 실패 시에는 원작 처리를 유지합니다. 플레이어와 컴퓨터 모두 적용됩니다."),
        new Feature("조조전", "", "groups", "ccz", 1, "", "", "조조전 기능 전체 (SAN14_CCZ.dll)", "끄면 조조전 탭 기능을 모두 쓰지 않으며 SAN14_CCZ.dll도 불러오지 않습니다. 편의·게임성 기능 전체(편의성 탭)가 켜져 있어야 동작합니다."),
        new Feature("조조전", "ccz", "ccz", "ccz_sprites", 1, "", "", "조조전 병종 그림 (필드 부대)", "OFF이면 부대 UI 배치도 원작으로 복원합니다. 필드의 부대를 조조전 병종 그림으로 그립니다. 옷만 세력색(파랑·초록·보라 같은 차가운 색은 빨강~주황 대비색)으로 칠하고 말·얼굴은 원래 색이며, 대비되는 테두리를 두르고 이동·공격·대기 동작과 방향이 바뀝니다. 신조조전 장수 92명(전용 병종 17명: 조조=위무제·여포=무신·관우=군신 등, 나머지 75명은 신조조전 개인 그림)은 기본값은 군주·군사 병종보다 고유 그림 우선입니다. 게임 첫 메뉴의 「조조전」에서 진형별 병종·통솔 경계·무장별 고유 그림 적용 진형을 바꿀 수 있습니다. 그림 출처: 조조전(KOEI) 6.6·신조조전 7.0. 그림 사용 시 3D 모형 숨김과 지형 가림도 함께 적용합니다."),
        new Feature("조조전", "ccz", "ccz", "sprite_fallback", 0, "ccz.ccz_sprites", "", "그림을 못 읽으면 원작 모델", "조조전 그림을 못 읽거나 칸이 모자라면 원작 부대 모델을 보여 줍니다. 끄면 이전 판."),
        new Feature("조조전", "ccz", "ccz", "ccz_hide_model", 0, "ccz.ccz_sprites", "", "조조전 그림 부대의 원래 3D 모형 숨김", "병종 그림에 통합되었습니다."),
        new Feature("조조전", "ccz", "ccz", "ccz_depth", 0, "ccz.ccz_sprites", "", "조조전 그림 지형 가림 처리", "병종 그림에 통합되었습니다."),
        new Feature("조조전", "ccz", "ccz", "ccz_far_keep", 0, "ccz.ccz_sprites", "", "원거리 부대 그림 유지", "더 이상 필요 없어 제거되었습니다."),
        new Feature("조조전", "ccz", "ccz", "ccz_block", 1, "", "", "조조전식 막기", "막기: 맞는 장수의 무력이 높을수록 일반 공격을 완전히 막아 1명만 잃습니다(5% + 무력 차 x 0.4, 1~10%). 한 번 발동하면 그 공격을 포함해 받는 일반 공격 5번을 막습니다. 하루 동안 일반 공격을 주고받지 않으면 상태가 끝납니다. 일반 공격만 대상이며 예상 피해 표시에는 반영되지 않습니다. 조조전 그림을 켜면 막기 자세·섬광이 보입니다."),
        new Feature("조조전", "ccz", "ccz", "ccz_critical", 1, "", "", "조조전식 회심의 일격", "회심의 일격: 공격 장수의 무력·부대 사기가 높을수록 일반 공격 피해가 1.5배가 됩니다(5% + 무력 차 x 0.3 + 사기 차 x 0.2, 1~10%). 한 번 발동하면 그 공격을 포함해 다음 일반 공격 5번이 모두 회심이 됩니다. 하루 동안 일반 공격을 주고받지 않으면 상태가 끝납니다. 일반 공격만 대상이며 예상 피해 표시에는 반영되지 않습니다. 조조전 그림을 켜면 막기 자세·섬광이 보입니다."),
        new Feature("게임성", "convenience", "combat", "district_intercept_block", 1, "", "", "지원·내정 군단 출진 제한 (플레이어 2군단~)", "플레이어가 위임한 군단의 방침이 지원이면 수송 출진만, 내정이면 요격·지역 점령·공격 출진을 막습니다(수송·건설은 허용). 컴퓨터 군단·직할 1군단·공격 방침·직접 명령·이미 출전한 부대는 그대로입니다. 전투 AI 교정 없이도 적용됩니다."),
        new Feature("전투", "combat", "combat", "attack_eval70", 0, "", "", "공격 검토: 목표 성 전투력 70%", "컴퓨터 세력만(플레이어 직할·위임 군단 제외). 원작 공격 검토에서 목표 성 전투력을 70%로 평가합니다. 끄면 원작."),
        new Feature("전투", "combat", "combat", "retreat_eval60", 0, "", "", "야전 퇴각 재판정: 적 60%", "컴퓨터 세력만(플레이어 직할·위임 군단 제외). 야전 부대 퇴각 재판정에서 적을 60%로 평가합니다. 요격 평가는 원작. 끄면 원작."),
        new Feature("전투", "combat", "combat", "siege_guard", 0, "", "", "공성군 요격 6칸(지력 80 이상)", "컴퓨터 공성군 야전 부대만. 지휘관 지력 80 이상이면 플레이어 부대를 6칸 안에서 요격합니다. 세이브에 남지 않습니다. 끄면 원작 3칸."),
        new Feature("전투", "combat", "combat", "delegated_exclude", 0, "", "", "플레이어 위임 군단은 컴퓨터 출진·공성 규칙에서 제외", "켜면 플레이어 세력의 위임 군단은 모드의 출진 편성·공성·지원·장애물 규칙을 받지 않고 원작 위임 AI대로 움직입니다. 컴퓨터 세력은 그대로입니다. 위임 군단 내정·등용 자동화는 이 스위치와 무관합니다. 끄면 이전 판(위임 군단도 같은 규칙)."),
        new Feature("게임성", "convenience", "personnel", "merit", 1, "", "", "논공 친애 확장", "논공 친애 관련 추가 효과를 켭니다. 아래의 무공 단계별 병력·군주 친애 옵션과 함께 동작하며, 꺼도 기존 논공 기록과 명품창고 저장은 보존됩니다."),
        new Feature("게임성", "convenience", "personnel", "merit_bond16", 0, "personnel.merit", "", "논공 친애 확장", "군주가 논공 3단계면 군주의 부·모·배우자·의형제도 논공 친애 대상이 됩니다. 끄면 이전 판."),
        new Feature("게임성", "convenience", "personnel", "merit_troops_level", 4, "personnel.merit", "", "무공 1·2단계 병력 (1500~3000)", "원작은 1단계·2단계 각 500(새 패치 1000)입니다. 1500/2000/2500/3000 중에서 고르며 기본은 3000입니다. 번호를 누르면 값이 차례로 바뀌고, 번호=값(1~4)으로 바로 지정할 수 있습니다."),
        new Feature("게임성", "convenience", "personnel", "merit_friendship", 1, "personnel.merit", "", "논공 친애 (무공 3단계 군주 상호 친애)", "켜면 무공 3단계에 군주와의 상호 친애를 원작 효과에 더해 줍니다. 옛 빌드는 3단계 병력 +1000이 0으로 바뀌고, 새 패치 빌드는 원작 시정 Lv+1이 그대로 남습니다. 끄면 3단계는 원작 그대로입니다."),
        new Feature("게임성", "convenience", "personnel", "awards_ai", 1, "personnel.awards", "", "컴퓨터 명품 자동 배분", "컴퓨터 세력이 창고 명품을 능력에 맞게 배분합니다. 플레이어 세력(직할·위임 군단)은 제외."),
        new Feature("게임성", "convenience", "personnel", "awards_horse", 0, "personnel.awards_ai", "", "퇴각확실 명마 우선", "컴퓨터 세력만. 퇴각확실 명마를 군주·지정 무장에게 먼저 줍니다. 끄면 이전 판."),
        new Feature("게임성", "convenience", "personnel", "awards_v16", 0, "personnel.awards_ai", "", "명품 배분 변경", "컴퓨터 세력만(플레이어 위임 군단은 원작). 명령서 없이 배분하고 지역담당관 해임을 허용합니다. 끄면 이전 판."),
        new Feature("게임성", "convenience", "personnel", "recruit_ai", 1, "", "personnel.recruit", "컴퓨터·위임 군단 등용 시도 제한", "컴퓨터와 위임 군단이 관계·혐오 등으로 성공할 수 없는 등용 시도를 하지 않게 합니다. 플레이어의 직접 등용은 그대로입니다."),
        new Feature("게임성", "convenience", "personnel", "injury", 1, "", "", "부상 무장 보호", "컴퓨터·위임 군단이 부상 무장을 출진·임무·유언비어에 쓰지 않습니다. 플레이어 직접 명령은 그대로입니다."),
        new Feature("게임성", "convenience", "personnel", "injury_herbal", 1, "", "personnel.injury", "컴퓨터 세력 본초학 시정 자동 배치", "부상 무장이 있는 컴퓨터 세력만 적용(플레이어 세력의 시정은 건드리지 않음)."),
        new Feature("게임성", "convenience", "personnel", "ai_search2", 1, "", "", "컴퓨터 군단 2차 탐색 생략", "컴퓨터 군단이 한 턴에 두 번 돌리는 인재 탐색 중 군단 차례 끝의 2차 탐색(대기 무장 거의 전원을 이미 다 찾은 우리 지역에 보내는 것)을 하지 않습니다. 1차 탐색과 플레이어 직할·위임 군단은 그대로입니다. 원본 모드의 메모리 전체 검색·반복 재검색(렉 원인)은 없이 원작 탐색 실행 노드에서 거절합니다. 끄면 원작. 출처: 삼갤 주딱 (SAN14_AISearch v1.1)"),
        new Feature("게임성", "convenience", "personnel", "officer_first", 1, "", "", "컴퓨터 군단 모병·훈련 담당관 매 턴 검토", "원작은 플레이어 도시 점유율이 30% 미만이면 컴퓨터 군단의 모병·훈련 담당관 임명 검토를 난이도에 따라 3턴 중 1~2턴만 합니다. 컴퓨터 군단은 이 두 검토를 매 턴 하게 합니다(금·병력·행동력 조건은 원작 그대로). 플레이어 직할·위임 군단은 그대로입니다. 끄면 원작."),
        new Feature("게임성", "convenience", "personnel", "officer_rotation", 1, "", "", "컴퓨터 세력 전선·후방 무장 맞교환·친애 편성 (매월 상순)", "컴퓨터 세력만 적용(플레이어 세력의 직할·위임 군단은 그대로). 매월 상순 한 번, 세력마다 전선 도시(적대 세력 지역과 맞닿은 도시, 동맹·정전·속국은 전선 아님)의 통솔 낮은 무장과 후방 도시의 통솔 높은 무장을 최대 4쌍까지 서로 맞바꿔 이동시킵니다. 전선 도시는 병력으로 낼 수 있는 부대 수(병력 5000당 1부대, 5~10) 만큼의 통솔 상위 무장으로 평가하고, 그 안의 친애 한 쌍마다 통솔 10, 범위 개성(응원·악주·명경·독려·효장 등) 무장이 있으면 8을 더해 가장 크게 좋아지는 교환부터 고릅니다(친애 무장은 통솔 차 10 이내면 우선). 통솔 70 이상 주장과 친애이거나 범위 개성을 가진 무장은 통솔이 낮아도 후방으로 보내지 않습니다. 새 게임 시작 직후에는 위협이 큰 전선 도시부터 돌아가며 한 번에 편성하고, 적 세력이 망해 전선이 사라진 도시는 6개월 동안 매달 4명씩 전선으로 보냅니다(3명은 남김)(통솔 차이 10 이상, 도시별 인원 수는 그대로, 이동 비용은 원작과 같이 군단 명령서 1, 남은 명령서가 없어도 이동, 한 번 옮긴 무장은 3개월 동안 다시 옮기지 않음, 그 사이 원작 AI가 다른 도시로 다시 보낸 무장은 원작 판단을 존중해 12개월 동안 건드리지 않음). 군주는 움직이지 않고, 군단장이 옮기면 군단이 풀릴 경우 그 군단의 통솔 높은 무장에게 먼저 군단장을 넘깁니다. 끄면 원작."),
        new Feature("게임성", "convenience", "domestic", "domestic", 1, "", "", "자동 내정 — 상업·농업·병영 중 낮은 개발부터 균형 개발", "플레이어가 위임한 2군단 이후와 컴퓨터 세력에 적용됩니다. 상업·농업·병영 중 개발이 낮은 항목부터 균형 있게 올리며, 군주 직할 1군단은 아래 「1군단 자동 내정」을 켰을 때만 적용됩니다."),
        new Feature("게임성", "convenience", "domestic", "domestic_direct", 0, "domestic.domestic", "", "1군단(군주 직할) 자동 내정", "켜면 플레이어가 직접 내정하는 1군단 지역도 매일 균형 개발 항목으로 바꿉니다(직접 정한 개발 항목을 덮어씀). 명령서 비용 0은 적용되지 않습니다."),
        new Feature("게임성", "convenience", "domestic", "domestic_free", 1, "domestic.domestic", "", "지역담당관 자동 임명 명령서 비용 0", "플레이어 2군단 이후·컴퓨터 세력의 자동 임명에만 적용. 1군단과 플레이어 수동 임명은 원작 비용 그대로입니다."),
        new Feature("게임성", "convenience", "domestic", "population", 1, "", "", "인구 월간 증가 배율", "매달 늘어나는 인구(원작 증가분)를 아래 배율만큼 늘립니다. 지역 인구 상한 60,000은 그대로입니다."),
        new Feature("게임성", "convenience", "domestic", "population_rate", 4, "domestic.population", "", "인구 증가 배율 (1~4배)", "1=원작과 같음, 2~4=그 배수. 번호를 누르면 1→2→3→4로 바뀌고, 번호=값으로 바로 지정할 수 있습니다."),
        new Feature("게임성", "convenience", "extensions", "traits9", 1, "", "", "개성 9칸", "무장 개성을 원작 5칸에서 9칸까지 늘립니다. 세이브에 저장된 개성 값은 보존되며, 끄면 원작 5칸만 표시·적용합니다."),
        new Feature("게임성", "convenience", "extensions", "stats200", 1, "", "", "능력치 상한 200", "무장 능력치 상한을 원작 100에서 200으로 늘립니다. 100을 넘는 성장과 보정이 반영되며, 끄면 새 성장은 원작 100 상한을 따릅니다."),
        new Feature("게임성", "convenience", "extensions", "stats200_growth", 1, "extensions.stats200", "", "능력치 100 이후 자연 성장", "능력치가 100 이상일 때도 경험치로 조금씩 자연 성장하게 합니다. 200에서 멈추며, 위의 「능력치 상한 200」을 켜야 함께 동작합니다. 끄면 100 이후에는 성장하지 않습니다."),
        new Feature("기타(개발용)", "", "groups", "diagnostics", 0, "", "", "개발용 기능 전체", "진단 로그·추적·시간 기록 같은 개발용 기능의 묶음입니다. 끄면 아래 항목이 모두 꺼집니다. 일반 플레이에는 필요 없습니다."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "enabled", 0, "", "", "진단 로그 기록 (SAN14_*.log)", "모드 판단과 오류를 게임 폴더 로그 파일에 남깁니다. 배포용(로그 제외) DLL에서는 효과가 없습니다."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "terrain_diag_mode", 0, "", "", "지형 편집 렉 분리 A/B/C", "0=기준(물살·렌더러 ON), 1=A(물살 OFF), 2=B(렌더러·높이 캡처 OFF), 3=C(둘 다 OFF). A/B/C는 진단 전용입니다. terrain_edit=1에서 줌 렉 비교만 하고 실제 지형 편집·저장에는 사용하지 마세요."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "reuse_handle", 0, "", "", "로그 파일 핸들 재사용", "로그를 쓸 때 파일을 매번 다시 열지 않아 기록 비용을 줄입니다."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "ai_trace", 0, "", "", "원작 AI 행동 추적 (CSV)", "원작 AI 판단 노드가 부대·군세 목표를 바꾸는 과정을 data/ai_trace에 기록합니다. 기록량이 많습니다."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "ai_trace_original", 0, "diagnostics.ai_trace", "", "원작만 추적 (모드 훅 연결 안 함)", "모드 기능을 연결하지 않고 원작 AI만 기록합니다. 이 상태로 기존 세이브에 덮어쓰지 마세요."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "layout_wait", 0, "", "", "화면 레이아웃 대기 기록", "턴 전환 때 화면 레이아웃 로딩을 기다린 시간을 기록합니다."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "day_stage", 0, "", "", "하루 처리 단계 기록", "진행 중 하루 처리 단계가 바뀔 때마다 그 단계에 머문 시간과 실제 계산 시간을 진단 로그(day_stage)에 남깁니다. 전략중 뒤 1일 시작 대기 조사용이며 진행 속도 버튼이 켜져 있어야 기록됩니다."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "cost_profile", 0, "", "", "함수 시간 측정 (CSV)", "모드 함수와 원작 AI 판단 노드마다 호출 수·시간을 data/performance에 기록합니다. 측정 중에는 약간 느려집니다."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "call_trace", 0, "", "", "함수 호출 추적 (개발용)", "진단 빌드에서 게임 함수 호출·그래픽 그리기·경로 탐색을 크래시 기록에 남깁니다. 게임이 느려지므로 평소에는 끕니다."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "area_index_verify", 0, "", "", "지역 칸 캐시 원작 결과 대조", "진단판에서 매 지역 시설 수 검사 결과를 원작 전체 순회와 대조합니다. 불일치 시 원작 결과를 사용하며 측정 중에는 느려집니다."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "path_fast_verify", 0, "", "", "경로 탐색 가속 결과 비교", "0=끔, 1=매번, N=N번에 한 번. 1은 원작 결과를 유지하며 깨끗한 버퍼와 잔존 상태를 구분합니다. N>1은 전체 호출 순번마다 표본 구간을 깨끗하게 비교하고, 같으면 새 결과·다르면 원작 결과를 씁니다. 1일 때만 지우기 생략을 끕니다. 불일치 상세는 하루 16건, 새 구현만 실행한 시간도 기록합니다. N은 기능설정.ini에서 지정합니다."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "path_count", 0, "", "", "경로 탐색 호출 횟수 기록", "세력 AI가 경로 탐색을 몇 번 부르고 같은 출발지·목적지·조건을 되풀이하는지 진단 로그에 남깁니다. 게임 동작은 바뀌지 않습니다."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "water_diag", 0, "", "", "물살 렌더 진단", "지형 렌더의 물살 처리 과정을 진단 로그에 남깁니다. 관찰 전용입니다."),
        new Feature("기타(개발용)", "diagnostics", "diagnostics", "gpu_profile", 0, "", "", "GPU 시간 기록", "그래픽 처리 시간을 data 폴더에 기록합니다. 관찰 전용입니다."),
        new Feature("기타(개발용)", "diagnostics", "progress_native", "profile", 0, "", "", "AI 진행 함수 시간 기록", "원작 AI 진행 함수별 호출 시간을 진단 로그에 남깁니다."),
    };
    public static Feature ById(string id) { return Items.First(f => f.Id == id); }
    // Structured game-menu settings, deliberately outside the integer/checkbox catalogue.
    // Empty/missing = current default art. FeatureIni preserves this raw string verbatim.
    public static readonly string[] RawSettings = { "ccz_appearance.profile", "tactic_priority.profile" };
    // user 2026-10-28: retired, conflicts with mission_v2; independent of its current value.
    public static readonly HashSet<string> Retired = new HashSet<string> {
        "diplomacy.diplomacy_overinvest", "combat.departures", "combat.group_advance", "combat.support",
        "combat.siege_native", "officer_ui.unified", "performance.timer_1ms", "ccz.ccz_hide_model",
        "ccz.ccz_depth", "ccz.ccz_far_keep"
    };
    public const string RetiredNote = "임무 일관성 v2와 충돌해 사용이 중지된 기능입니다.";
    // 함께 쓸 수 없는 기능: 키 기능이 켜져 있으면 값 기능은 설치되지 않는다 (DLL도 같은 규칙으로 끈다).
    public static readonly Dictionary<string, string> ExcludedBy = new Dictionary<string, string> {
        { "combat.departures", "combat.mission_v2" },
        { "combat.group_advance", "combat.mission_v2" },
        { "combat.support", "combat.mission_v2" },
    };
    public static Feature Master(string group) { return Items.First(f => f.IsMaster && f.Key == group); }
    public static readonly Dictionary<string, string[]> Choices = new Dictionary<string, string[]> {
        { "diagnostics.terrain_diag_mode", new[] { "0 — 기준: foam ON / renderer ON", "1 — A: foam OFF / renderer ON", "2 — B: foam ON / renderer OFF", "3 — C: foam OFF / renderer OFF" } },
        { "domestic.population_rate", new[] { "1배 (원작과 같음)", "2배", "3배", "4배" } },
        { "personnel.merit_troops_level", new[] { "무공 1·2단계 병력 1500", "무공 1·2단계 병력 2000", "무공 1·2단계 병력 2500", "무공 1·2단계 병력 3000 (기본)" } }
    };
    public static int ChoiceBase(string id) { return id == "domestic.population_rate" || id == "personnel.merit_troops_level" ? 1 : 0; }
}

// 기능설정.ini (UTF-16). DLL과 FeatureSelect.ps1처럼 [features] → [섹션] → (이전 파일이면
// 원래 묶음 값) → 기본값 순으로 읽는다.
sealed class FeatureIni {
    readonly List<KeyValuePair<string, List<KeyValuePair<string, string>>>> sections =
        new List<KeyValuePair<string, List<KeyValuePair<string, string>>>>();

    public static FeatureIni Load(string path) {
        var ini = new FeatureIni();
        if (!File.Exists(path)) return ini;
        List<KeyValuePair<string, string>> current = null;
        foreach (var raw in File.ReadAllText(path).Split('\n')) {
            var line = raw.Trim();
            if (line.StartsWith("[") && line.EndsWith("]")) { current = ini.Section(line.Substring(1, line.Length - 2).Trim().ToLowerInvariant(), true); continue; }
            if (line.Length == 0 || line.StartsWith(";") || current == null) continue;
            int at = line.IndexOf('=');
            if (at <= 0) continue;
            var key = line.Substring(0, at).Trim().ToLowerInvariant();
            current.RemoveAll(p => p.Key == key);
            current.Add(new KeyValuePair<string, string>(key, line.Substring(at + 1).Trim()));
        }
        return ini;
    }
    List<KeyValuePair<string, string>> Section(string name, bool create) {
        foreach (var s in sections) if (s.Key == name) return s.Value;
        if (!create) return null;
        var list = new List<KeyValuePair<string, string>>();
        sections.Add(new KeyValuePair<string, List<KeyValuePair<string, string>>>(name, list));
        return list;
    }
    string Get(string section, string key) {
        var s = Section(section, false);
        if (s == null) return null;
        foreach (var p in s) if (p.Key == key) return p.Value;
        return null;
    }
    public int Value(Feature f) {
        if (Catalog.Retired.Contains(f.Id)) return 0;
        int value = f.Inherit != "" ? Value(Catalog.ById(f.Inherit)) : f.Default;
        foreach (var source in new[] { f.Id == "performance.fast_menu" ? Get("game_flow", "fast_menu") : null, Get("features", f.Key), Get(f.Section, f.Key) }) {
            int n;
            if (source != null && int.TryParse(source, out n))
                value = f.Id == "diagnostics.path_fast_verify" ? Math.Max(0, n) :
                    f.Id == "diagnostics.terrain_diag_mode" ? Math.Max(0, Math.Min(3, n)) :
                    f.Id == "domestic.population_rate" || f.Id == "personnel.merit_troops_level" ? Math.Max(1, Math.Min(4, n)) : (n != 0 ? 1 : 0);
        }
        return value;
    }
    // 메뉴 소유 키만 갱신하고 나머지 섹션·키는 그대로 둔다. 설명 주석은 관리.ps1이
    // -NonInteractive 실행 때 원래 형식으로 다시 쓴다.
    public void Save(string path, IDictionary<string, int> values) {
        // user 2026-11-03: migrate the old section alias; strategy_batch was
        // removed for screen corruption and must not look like a working switch.
        foreach (var s in sections) s.Value.RemoveAll(p =>
            (s.Key == "game_flow" && p.Key == "fast_menu") ||
            (s.Key == "performance" && p.Key == "strategy_batch"));
        foreach (var f in Catalog.Items) {
            var s = Section(f.Section, true);
            s.RemoveAll(p => p.Key == f.Key);
            s.Add(new KeyValuePair<string, string>(f.Key, Catalog.Retired.Contains(f.Id) ? "0" : values[f.Id].ToString()));
        }
        var text = new StringBuilder();
        text.Append("; AI 모드 기능 켜기/끄기. 1 = 켜기, 0 = 끄기. 게임을 다시 시작하면 적용됩니다.\r\n");
        foreach (var s in sections) {
            if (s.Key == "features") continue;
            text.Append("\r\n[").Append(s.Key).Append("]\r\n");
            foreach (var p in s.Value) text.Append(p.Key).Append('=').Append(p.Value).Append("\r\n");
        }
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        var temp = path + ".aitactics.tmp";
        File.WriteAllText(temp, text.ToString(), Encoding.Unicode);
        try {
            if (File.Exists(path)) File.Replace(temp, path, null); else File.Move(temp, path);
        } finally { if (File.Exists(temp)) File.Delete(temp); }
    }
}

// 게임 폴더 찾기·기억, 패키지 파일 복사.
static class Folders {
    public const string GameExe = "SAN14CE_KO.exe";
    static string Saved { get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), @"AI_TacticsMod\installer_game_folder.txt"); } }
    public static bool IsGame(string dir) { return !string.IsNullOrEmpty(dir) && File.Exists(Path.Combine(dir, GameExe)); }
    public static bool Same(string a, string b) {
        if (a == null || b == null) return false;
        return string.Equals(Path.GetFullPath(a).TrimEnd('\\'), Path.GetFullPath(b).TrimEnd('\\'), StringComparison.OrdinalIgnoreCase);
    }
    public static void Remember(string dir) {
        try { Directory.CreateDirectory(Path.GetDirectoryName(Saved)); File.WriteAllText(Saved, dir, Encoding.UTF8); } catch { }
    }
    public static string Remembered() {
        try { var d = File.Exists(Saved) ? File.ReadAllText(Saved, Encoding.UTF8).Trim() : null; return IsGame(d) ? d : null; } catch { return null; }
    }
    // Steam 라이브러리(libraryfolders.vdf)에서 설치 위치를 찾는다. 읽기만 한다.
    public static string FromSteam() {
        var roots = new List<string>();
        try {
            var steam = Microsoft.Win32.Registry.GetValue(@"HKEY_CURRENT_USER\Software\Valve\Steam", "SteamPath", null) as string;
            if (!string.IsNullOrEmpty(steam)) {
                roots.Add(steam.Replace('/', '\\'));
                var vdf = Path.Combine(steam, @"steamapps\libraryfolders.vdf");
                if (File.Exists(vdf))
                    foreach (Match m in Regex.Matches(File.ReadAllText(vdf), "\"path\"\\s+\"([^\"]+)\""))
                        roots.Add(m.Groups[1].Value.Replace(@"\\", @"\"));
            }
        } catch { }
        roots.Add(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Steam"));
        foreach (var root in roots) {
            var dir = Path.Combine(root, @"steamapps\common\Romance_of_the_Three_Kingdoms_14");
            if (IsGame(dir)) return dir;
        }
        return null;
    }
    public static string Pick(IWin32Window owner, string start) {
        using (var picker = new OpenFileDialog {
            Title = "삼국지14 CE 한국어판 실행 파일(SAN14CE_KO.exe)을 선택해 주세요",
            Filter = "삼국지14 CE 한국어판|" + GameExe, CheckFileExists = true
        }) {
            if (IsGame(start)) picker.InitialDirectory = start;
            if (picker.ShowDialog(owner) != DialogResult.OK) return null;
            if (!string.Equals(Path.GetFileName(picker.FileName), GameExe, StringComparison.OrdinalIgnoreCase)) return null;
            return Path.GetDirectoryName(picker.FileName);
        }
    }
    // 배포 ZIP 목록에 있는 파일만, 게임 폴더 안으로만 복사한다. 게임 루트 DLL은 목록에
    // 없으며 들어 있어도 건너뛴다(DLL 설치는 관리.ps1의 검사를 거친다).
    public static List<string> PackageFiles(string package) {
        var list = Path.Combine(package, @"AI_TacticsMod\package_files.txt");
        var result = new List<string>();
        if (!File.Exists(list)) return null;
        foreach (var raw in File.ReadAllLines(list, Encoding.UTF8)) {
            var rel = raw.Trim().Replace('/', '\\');
            if (rel.Length == 0 || Path.IsPathRooted(rel) || rel.Split('\\').Contains("..")) continue;
            if (!rel.Contains('\\') && rel.EndsWith(".dll", StringComparison.OrdinalIgnoreCase)) continue;
            if (string.Equals(rel, @"AI_TacticsMod\기능설정.ini", StringComparison.OrdinalIgnoreCase)) continue;
            result.Add(rel);
        }
        return result;
    }
    public static int Changed(string package, string game, List<string> files) {
        int changed = 0;
        foreach (var rel in files) {
            var src = Path.Combine(package, rel); var dst = Path.Combine(game, rel);
            if (!File.Exists(src)) continue;
            if (!File.Exists(dst) || !File.ReadAllBytes(src).SequenceEqual(File.ReadAllBytes(dst))) ++changed;
        }
        return changed;
    }
    public static int Copy(string package, string game, List<string> files) {
        int copied = 0;
        var root = Path.GetFullPath(game).TrimEnd('\\') + "\\";
        foreach (var rel in files) {
            var src = Path.Combine(package, rel); var dst = Path.GetFullPath(Path.Combine(game, rel));
            if (!File.Exists(src) || !dst.StartsWith(root, StringComparison.OrdinalIgnoreCase)) continue;
            Directory.CreateDirectory(Path.GetDirectoryName(dst));
            File.Copy(src, dst, true); ++copied;
        }
        return copied;
    }
}

// 사용자 2026-10-30: docs/CREDITS.md의 목록을 설치기에서도 한 페이지에 표시한다.
static class Credits {
    public const string Introduction = "다른 모드에서 가져온 기능입니다. 각 기능 설명 끝에도 출처를 표시합니다.";
    public const string Summary = "출처 표기: 삼갤 통합패치(180.71·175.115 유동) · 삼갤 주딱 · 112.151 유동(삼봉이정도전) · 조조전(KOEI)·신조조전 — 「출처」 탭 참고";
    public static readonly string[] Lines = {
        "- AI 전사 연출 게임 기록 → 삼갤 통합패치(180.71·175.115 유동)",
        "- 포획대사 → 삼갤 통합패치(180.71·175.115 유동)",
        "- 개성전법 툴팁 콜라보 개성 전법 해금 → 삼갤 통합패치(180.71·175.115 유동)",
        "- 교역 무장 소재 고정 → 삼갤 주딱",
        "- 능력치 경험치 → 삼갤 주딱",
        "- 파견일수 → 삼갤 주딱",
        "- 중개 확장 → 삼갤 주딱",
        "- 전법서 다중 → 삼갤 주딱",
        "- 시작전 군주 변경 → 삼갤 주딱",
        "- 컴퓨터 군단 2차 탐색 생략 → 삼갤 주딱 (SAN14_AISearch v1.1)",
        "- 명승지 연출 → 112.151 유동(삼봉이정도전)의 「11종 선택패치」",
        "- 조조전 병종·장수 그림 → 삼국지 조조전(KOEI) 6.6 · 신조조전 7.0",
    };
}

static class Theme {
    public static readonly Color Header = Color.FromArgb(28, 32, 42);
    public static readonly Color HeaderText = Color.FromArgb(240, 236, 226);
    public static readonly Color HeaderMuted = Color.FromArgb(200, 196, 186);
    public static readonly Color Accent = Color.FromArgb(196, 154, 64);
    public static readonly Color AccentDark = Color.FromArgb(150, 112, 36);
    public static readonly Color Page = Color.FromArgb(246, 244, 239);
    public static readonly Color Card = Color.White;
    public static readonly Color MasterCard = Color.FromArgb(252, 246, 232);
    public static readonly Color ChildCard = Color.FromArgb(250, 249, 246);
    public static readonly Color Border = Color.FromArgb(224, 219, 207);
    public static readonly Color Text = Color.FromArgb(36, 38, 44);
    public static readonly Color Muted = Color.FromArgb(118, 116, 110);
    public static readonly Color Disabled = Color.FromArgb(160, 158, 152);
    public static readonly Color Link = Color.FromArgb(40, 98, 170);
    public static readonly Color Bad = Color.FromArgb(178, 52, 46);
    public static readonly Color NewCard = Color.FromArgb(228, 245, 230);
    public static readonly Color ChangedCard = Color.FromArgb(253, 238, 218);
    public static readonly Color NewText = Color.FromArgb(36, 128, 64);
    public static readonly Color ChangedText = Color.FromArgb(196, 110, 20);
    public static Font Ui(float size, FontStyle style = FontStyle.Regular) { return new Font("맑은 고딕", size, style); }
}

sealed class FlatButton : Button {
    public FlatButton(string text, bool primary) {
        Text = text; FlatStyle = FlatStyle.Flat; Cursor = Cursors.Hand;
        Font = Theme.Ui(10f, primary ? FontStyle.Bold : FontStyle.Regular);
        Height = 38; Padding = new Padding(14, 0, 14, 0); AutoSize = true; AutoSizeMode = AutoSizeMode.GrowAndShrink;
        MinimumSize = new Size(110, 38);
        BackColor = primary ? Theme.Accent : Theme.Card;
        ForeColor = primary ? Color.White : Theme.Text;
        FlatAppearance.BorderColor = primary ? Theme.AccentDark : Theme.Border;
        FlatAppearance.MouseOverBackColor = primary ? Theme.AccentDark : Theme.Page;
        Margin = new Padding(8, 0, 0, 0);
    }
}

sealed class FeatureRow : Panel {
    public readonly Feature Item;
    public readonly CheckBox Box;
    public readonly ComboBox Choice;
    public readonly LinkLabel Fix;
    readonly Label tag, note;
    bool hasNote; // Control.Visible은 숨은 탭 안에서 항상 false라 높이 계산에 쓰지 않는다.
    public FeatureRow(Feature item, bool nested) {
        Item = item;
        Dock = DockStyle.Top;
        BackColor = item.IsMaster ? Theme.MasterCard : nested ? Theme.ChildCard : Theme.Card;
        tag = new Label {
            Text = "└ 종속", AutoSize = true, ForeColor = Theme.AccentDark, Font = Theme.Ui(8.5f, FontStyle.Bold),
            Visible = nested, BackColor = Color.Transparent, Cursor = Cursors.Hand
        };
        // 종속 표시의 실제 글자 폭(DPI·글꼴 배율 포함) 뒤에 체크박스를 둔다. 고정 간격이면
        // 125%/150% 배율에서 표시가 체크박스를 덮어 클릭이 잘 안 된다.
        int tagWidth = nested ? TextRenderer.MeasureText(tag.Text, tag.Font).Width : 0;
        Padding = new Padding(item.IsMaster ? 16 : nested ? 28 + tagWidth + 16 : 40, 10, 16, 10);
        tag.Location = new Point(28, Padding.Top + 2);
        Box = new CheckBox {
            Text = item.Label, AutoSize = true, Cursor = Cursors.Hand, ForeColor = Theme.Text,
            Font = Theme.Ui(item.IsMaster ? 11.5f : 10.5f, item.IsMaster ? FontStyle.Bold : FontStyle.Regular),
            Location = new Point(Padding.Left, Padding.Top), BackColor = Color.Transparent
        };
        Choice = new ComboBox {
            DropDownStyle = ComboBoxStyle.DropDownList, Font = Theme.Ui(10f),
            Location = new Point(Padding.Left, Padding.Top), Width = 430, Visible = Catalog.Choices.ContainsKey(item.Id)
        };
        if (Catalog.Choices.ContainsKey(item.Id)) Choice.Items.AddRange(Catalog.Choices[item.Id]);
        if (Choice.Visible) Box.Visible = false;
        Fix = new LinkLabel {
            AutoSize = true, Font = Theme.Ui(9f, FontStyle.Bold), LinkColor = Theme.Link, ActiveLinkColor = Theme.AccentDark,
            Visible = false, BackColor = Color.Transparent
        };
        note = new Label {
            AutoSize = false, ForeColor = Theme.Muted, Font = Theme.Ui(9f),
            Location = new Point(Padding.Left + (Choice.Visible ? 4 : 20), (Choice.Visible ? Choice.Bottom : Box.Bottom) + 2), BackColor = Color.Transparent
        };
        tag.Click += (s, e) => { if (Box.Enabled) Box.Checked = !Box.Checked; }; // 표시를 눌러도 같은 항목이 바뀐다
        Controls.Add(tag); Controls.Add(Box); Controls.Add(Choice); Controls.Add(Fix); Controls.Add(note);
        SetState(true, item.Note, null);
        Paint += (s, e) => {
            using (var pen = new Pen(Theme.Border)) e.Graphics.DrawLine(pen, 0, Height - 1, Width, Height - 1);
            if (Change != 0) using (var brush = new SolidBrush(Change == 1 ? Theme.NewText : Theme.ChangedText)) e.Graphics.FillRectangle(brush, 0, 0, 5, Height);
        };
        Resize += (s, e) => Relayout();
    }
    // 버전 사이 변경 표시: 1 = 새 기능(초록), 2 = 바뀐 기능(주황), 0 = 없음.
    public int Change { get; private set; }
    public void SetChange(int kind) {
        if (kind == Change) return;
        Change = kind;
        Box.Text = (kind == 1 ? "[새 기능] " : kind == 2 ? "[바뀜] " : "") + Item.Label;
        BackColor = kind == 1 ? Theme.NewCard : kind == 2 ? Theme.ChangedCard : Item.IsMaster ? Theme.MasterCard : Padding.Left > 40 ? Theme.ChildCard : Theme.Card;
        Relayout(); Invalidate();
    }
    // effective=false면 회색 글자 + 이유(빨강) + 바로 켜는 링크.
    public void SetState(bool effective, string text, string fix) {
        Box.ForeColor = effective ? Theme.Text : Theme.Disabled;
        note.ForeColor = effective ? Theme.Muted : Theme.Bad;
        note.Text = text ?? "";
        hasNote = note.Text.Length > 0; note.Visible = hasNote;
        Fix.Text = fix ?? ""; Fix.Visible = fix != null;
        Relayout();
    }
    void Relayout() {
        Control primary = Choice.Visible ? (Control)Choice : Box;
        Fix.Location = new Point(primary.Right + 10, primary.Top + 3);
        int width = Math.Max(200, Width - note.Left - 24);
        var size = TextRenderer.MeasureText(note.Text, note.Font, new Size(width - 8, 0), TextFormatFlags.WordBreak | TextFormatFlags.TextBoxControl);
        note.Size = new Size(width, size.Height);
        note.Top = primary.Bottom + 2;
        Height = (hasNote ? note.Bottom : primary.Bottom) + Padding.Bottom;
    }
}

// 사용자 2026-11-01: GitHub 최신 배포 알림(팝업 없이 버튼 표시)·자동 업데이트·버전 되돌리기.
// 내려받기는 <게임>\AI_TacticsMod\updates\<버전>\ 아래에만 하고, 배포 ZIP의 SHA-256을 GitHub가
// 알려 주는 digest와 대조한다. 파일 설치는 지금처럼 그 버전의 관리.ps1(-NonInteractive)이 맡는다.
sealed class Release {
    public string Version, Title, Notes, Published, ZipUrl, ZipName, ZipSha, HistoryUrl;
    public long ZipSize;
}
static class Updates {
    public const string Repository = "t3ferii/san14ce-ai-mod";
    const string Api = "https://api.github.com/repos/" + Repository + "/releases?per_page=40";
    public static readonly int[] Oldest = { 13, 3 };   // 이보다 오래된 배포는 분리 DLL 관리 방식이 달라 되돌리기 목록에서 뺀다
    static WebClient Client() {
        ServicePointManager.SecurityProtocol |= (SecurityProtocolType)3072;   // TLS 1.2
        var web = new WebClient { Encoding = Encoding.UTF8 };
        web.Headers[HttpRequestHeader.UserAgent] = "SAN14-AI-Installer";
        web.Headers[HttpRequestHeader.Accept] = "application/vnd.github+json";
        return web;
    }
    static string Str(Dictionary<string, object> d, string key) { object v; return d.TryGetValue(key, out v) && v != null ? v.ToString() : ""; }
    public static List<Release> Fetch() {
        string text; using (var web = Client()) text = web.DownloadString(Api);
        var result = new List<Release>();
        foreach (var item in (object[])new JavaScriptSerializer { MaxJsonLength = 1 << 26 }.DeserializeObject(text)) {
            var r = (Dictionary<string, object>)item;
            if (Str(r, "draft") == "True") continue;
            var tag = Str(r, "tag_name");
            if (!tag.StartsWith("SAN14_AI_")) continue;
            var rel = new Release { Version = tag.Substring(9), Title = Str(r, "name"), Notes = Str(r, "body").Replace("\r\n", "\n").Replace("\n", "\r\n"), Published = Str(r, "published_at") };
            if (rel.Published.Length >= 10) rel.Published = rel.Published.Substring(0, 10);
            object assets; r.TryGetValue("assets", out assets);
            foreach (var a in (object[])(assets ?? new object[0])) {
                var asset = (Dictionary<string, object>)a; var name = Str(asset, "name");
                if (name.EndsWith("_integrated.zip", StringComparison.OrdinalIgnoreCase)) {
                    rel.ZipName = name; rel.ZipUrl = Str(asset, "browser_download_url");
                    var digest = Str(asset, "digest");
                    rel.ZipSha = digest.StartsWith("sha256:") ? digest.Substring(7).ToLowerInvariant() : "";
                    long size; long.TryParse(Str(asset, "size"), out size); rel.ZipSize = size;
                } else if (name == "release_features.json") rel.HistoryUrl = Str(asset, "browser_download_url");
            }
            if (rel.ZipUrl != null && Regex.IsMatch(rel.ZipSha ?? "", "^[0-9a-f]{64}$")) result.Add(rel);
        }
        result.Sort((a, b) => Compare(b.Version, a.Version));
        return result;
    }
    public static string DownloadText(string url) { using (var web = Client()) return web.DownloadString(url); }
    // User 2026-11-02: a newer local candidate is not the latest public release.
    public static string StatusText(string installed, bool localDiagnostic, string latest) {
        if (latest == null) return localDiagnostic ? "로컬 개발판 · 공개판 확인 불가" : "공개판 확인 불가";
        if (installed == null) return "설치 가능 · 공개 " + latest;
        int compare = Compare(latest, installed);
        if (compare > 0) return "● 업데이트 가능 · 공개 " + latest;
        if (localDiagnostic) return "로컬 개발판 " + installed + " · 공개 " + latest;
        if (compare == 0) return "공개 최신판 " + latest;
        return "설치판 " + installed + " · 공개 " + latest;
    }
    // v15 > v14.2 > v14.2-rc5 > v14.1. 접미사가 있으면 같은 번호의 정식판보다 앞선 버전이다.
    public static int Compare(string a, string b) {
        Func<string, int[]> nums = v => Regex.Match(v ?? "", @"^v?(\d+(?:\.\d+)*)").Groups[1].Value.Split(new[] { '.' }, StringSplitOptions.RemoveEmptyEntries).Select(int.Parse).ToArray();
        int[] x = nums(a), y = nums(b);
        for (int i = 0; i < Math.Max(x.Length, y.Length); ++i) {
            int p = i < x.Length ? x[i] : 0, q = i < y.Length ? y[i] : 0;
            if (p != q) return p.CompareTo(q);
        }
        bool sa = Regex.IsMatch(a ?? "", @"^v?[\d.]+-"), sb = Regex.IsMatch(b ?? "", @"^v?[\d.]+-");
        if (sa != sb) return sa ? -1 : 1;
        if (!sa) return 0;
        Func<string, int> rc = v => { var m = Regex.Match(v, @"^v?[\d.]+-\D*(\d+)"); return m.Success ? int.Parse(m.Groups[1].Value) : 0; };
        return rc(a).CompareTo(rc(b));   // v12.22-rc5 > v12.22-rc4
    }
    public static bool Supported(string version) { return Compare(version, "v" + Oldest[0] + "." + Oldest[1]) >= 0; }
    public static string Sha256(string path) {
        using (var sha = SHA256.Create()) using (var stream = File.OpenRead(path))
            return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
    }
    public static void Download(Release r, string target, Action<string> log) {
        var part = target + ".part";
        var request = (HttpWebRequest)WebRequest.Create(r.ZipUrl);
        ServicePointManager.SecurityProtocol |= (SecurityProtocolType)3072;
        request.UserAgent = "SAN14-AI-Installer";
        using (var response = request.GetResponse()) using (var input = response.GetResponseStream()) using (var output = File.Create(part)) {
            var buffer = new byte[1 << 16]; long total = 0; int step = 0, n;
            while ((n = input.Read(buffer, 0, buffer.Length)) > 0) {
                output.Write(buffer, 0, n); total += n;
                if (r.ZipSize > 0 && total * 10 / r.ZipSize > step) { step = (int)(total * 10 / r.ZipSize); log("  내려받는 중 " + step * 10 + "%"); }
            }
        }
        if (Sha256(part) != r.ZipSha) { File.Delete(part); throw new InvalidDataException("내려받은 파일의 SHA-256이 GitHub 값과 다릅니다. 다시 시도해 주세요."); }
        if (File.Exists(target)) File.Delete(target);
        File.Move(part, target);
    }
    // ZIP 안 경로가 대상 폴더를 벗어나면 풀지 않는다.
    public static void Extract(string zip, string folder) {
        if (Directory.Exists(folder)) Directory.Delete(folder, true);
        var root = Path.GetFullPath(folder).TrimEnd('\\') + "\\";
        using (var archive = ZipFile.OpenRead(zip)) {
            foreach (var entry in archive.Entries) {
                var dst = Path.GetFullPath(Path.Combine(folder, entry.FullName.Replace('/', '\\')));
                if (!dst.StartsWith(root, StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("배포 ZIP의 경로가 올바르지 않습니다: " + entry.FullName);
                if (entry.FullName.EndsWith("/")) { Directory.CreateDirectory(dst); continue; }
                Directory.CreateDirectory(Path.GetDirectoryName(dst));
                entry.ExtractToFile(dst, true);
            }
        }
    }
}
// 배포별 기능 목록(release_features.json: 버전 → 기능 Id → 이름·기본값·설명 지문). 설치된 버전과 대상
// 버전을 비교하므로 v13.3→v15와 v14.1→v15는 표시되는 변경이 다르다.
sealed class History {
    public readonly Dictionary<string, Dictionary<string, string>> Versions = new Dictionary<string, Dictionary<string, string>>();
    public static History Parse(string json) {
        var h = new History();
        var root = (Dictionary<string, object>)new JavaScriptSerializer().DeserializeObject(json);
        foreach (var v in (Dictionary<string, object>)root["versions"]) {
            var map = new Dictionary<string, string>();
            foreach (var f in (Dictionary<string, object>)v.Value) map[f.Key] = f.Value.ToString();
            h.Versions[v.Key] = map;
        }
        return h;
    }
    public static History Load(string path) { try { return File.Exists(path) ? Parse(File.ReadAllText(path, Encoding.UTF8)) : null; } catch { return null; } }
    public void Merge(History other) { if (other != null) foreach (var v in other.Versions) Versions[v.Key] = v.Value; }
    // 로컬 시험판 이름(v14.2-ccz-local15)은 가장 긴 배포 이름(v14.2)으로 본다.
    public string Known(string version) {
        if (version == null) return null;
        if (Versions.ContainsKey(version)) return version;
        return Versions.Keys.Where(k => version.StartsWith(k + "-")).OrderByDescending(k => k.Length).FirstOrDefault();
    }
    // 1 = 새 기능, 2 = 바뀐 기능. 없어지는 기능은 removed에 이름을 모은다.
    public Dictionary<string, int> Diff(string from, string to, List<string> removed) {
        var result = new Dictionary<string, int>();
        string a = Known(from), b = Known(to);
        if (a == null || b == null || a == b) return result;
        Dictionary<string, string> x = Versions[a], y = Versions[b];
        foreach (var f in y) { string old; if (!x.TryGetValue(f.Key, out old)) result[f.Key] = 1; else if (old != f.Value) result[f.Key] = 2; }
        if (removed != null) removed.AddRange(x.Keys.Where(k => !y.ContainsKey(k)));
        return result;
    }
}

sealed class MainForm : Form {
    readonly string package;   // 이 EXE와 배포 파일이 있는 폴더(게임 폴더와 같을 수 있음)
    string game, project, settings, manager;
    bool packageCopyApproved;
    bool versionChanged;   // PW31(사용자 2026-11-09): 이번 실행에서 업데이트·되돌리기를 마쳤으면 배포 폴더를 다시 복사하지 않는다
    readonly Dictionary<string, int> values = new Dictionary<string, int>();
    readonly List<FeatureRow> rows = new List<FeatureRow>();
    readonly Dictionary<string, Panel> pages = new Dictionary<string, Panel>();
    readonly Dictionary<string, RadioButton> tabButtons = new Dictionary<string, RadioButton>();
    readonly Label status = new Label(), folder = new Label(), summary = new Label();
    readonly Panel content = new Panel(), logPage = new Panel();
    readonly TextBox log = new TextBox();
    readonly List<Control> actions = new List<Control>();
    string lastLinked = "";
    bool dirty, busy, syncing;
    readonly List<string> pendingLog = new List<string>();   // game 폴더를 알기 전에 남긴 기록(첫 기록 때 파일로 내보낸다)
    // 사용자 2026-11-09: 설치기와 게임 DLL이 게임 폴더의 이 파일 하나에 함께 남긴다.
    const string CompatLogName = "ai행동교정모드호환성.log";
    const string CompatOldLogName = "ai행동교정모드호환성.old.log";
    static readonly string[] OurModules = { "SAN14_Layout.dll", "SAN14_CombatAI.dll", "SAN14_Convenience.dll", "SAN14_CCZ.dll" };
    // 업데이트·되돌리기 (사용자 2026-11-01)
    Button updateButton, rollbackButton;
    readonly ListBox versionList = new ListBox();
    readonly TextBox notesBox = new TextBox();
    readonly Label versionInfo = new Label(), versionDiff = new Label();
    FlatButton versionAction;
    List<Release> releases = new List<Release>();
    History history;
    Release latest;
    Dictionary<string, int> changes = new Dictionary<string, int>();
    string changeLegend = "";
    public string UpdatedTo;   // 자동 업데이트 뒤 다시 켜졌을 때 (Program --updated=)
    public static bool AutoCheck;   // Program만 켠다(검사용 창은 GitHub에 접속하지 않는다)

    public MainForm(string packageDir, string gameDir) {
        package = packageDir;
        Text = "삼국지14 CE AI 모드 설치 관리자";
        Font = Theme.Ui(10f); BackColor = Theme.Page; StartPosition = FormStartPosition.CenterScreen;
        AutoScaleMode = AutoScaleMode.Dpi; ClientSize = new Size(900, 720); MinimumSize = new Size(800, 580);
        WindowState = FormWindowState.Maximized;   // 2026-11-01 사용자: 최대 화면으로 연다(복원 시 900×720)

        BuildFooter(); BuildBody(); BuildTabs(); BuildHeader();
        content.BringToFront(); // Fill는 마지막에 배치되어야 머리/탭/하단을 침범하지 않는다.
        SetGame(gameDir);
        ShowLastGameRun();   // 작업 기록 탭에만: 지난 게임 실행에서 다른 모드와 겹쳐 꺼진 기능
        ShowTab(Catalog.Tabs[0]);
        FormClosing += (s, e) => {
            if (busy) { e.Cancel = true; return; }
            if (dirty && MessageBox.Show(this, "저장하지 않은 기능 선택이 있습니다. 닫을까요?", Text,
                    MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) e.Cancel = true;
        };
        Activated += (s, e) => { if (!busy) RefreshStatus(); };
        Shown += (s, e) => {
            if (UpdatedTo != null) Append(UpdatedTo + "(으)로 업데이트하고 설치 관리자를 다시 열었습니다.");
            if (AutoCheck) CheckUpdates(false); else LoadHistory();
        };
    }

    // ---- 업데이트·되돌리기 ----
    sealed class ReleaseItem {
        public Release Release; public string Text;
        public override string ToString() { return Text; }
    }
    string UpdateState { get { return Path.Combine(project, @"updates\state.json"); } }
    string InstalledVersion() {
        try {
            var file = Path.Combine(project, @"backup\input_state.json");
            if (!File.Exists(file)) return null;
            var m = Regex.Match(File.ReadAllText(file), "\"release_version\"\\s*:\\s*\"([^\"]+)\"");
            return m.Success ? m.Groups[1].Value : null;
        } catch { return null; }
    }
    string PreviousVersion() {
        try {
            if (!File.Exists(UpdateState)) return null;
            var json = File.ReadAllText(UpdateState, Encoding.UTF8);
            var installed = Regex.Match(json, "\"installed\"\\s*:\\s*\"([^\"]+)\"");
            var previous = Regex.Match(json, "\"previous\"\\s*:\\s*\"([^\"]+)\"");
            return installed.Success && previous.Success && installed.Groups[1].Value == InstalledVersion() ? previous.Groups[1].Value : null;
        } catch { return null; }
    }
    bool InstalledLocalDiagnostic(string installed) {
        if (installed == null) return false;
        try {
            var file = Path.Combine(project, @"build\input_package\manifest.json");
            var manifest = new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(File.ReadAllText(file));
            object version, local;
            return manifest.TryGetValue("release_version", out version) && (version as string) == installed &&
                manifest.TryGetValue("local_diagnostic_only", out local) && local is bool && (bool)local;
        } catch { return false; }
    }
    void LoadHistory() {
        history = History.Load(Path.Combine(project, "release_features.json")) ?? new History();
        if (package != null) history.Merge(History.Load(Path.Combine(package, @"AI_TacticsMod\release_features.json")));
    }

    // 시작할 때 한 번, 그리고 버튼을 누를 때 GitHub 배포 목록을 확인한다. 알림은 버튼 글자·색으로만.
    void CheckUpdates(bool manual) {
        LoadHistory(); ApplyDefaultChanges();
        updateButton.Text = "업데이트 확인 중…"; updateButton.Enabled = false;
        new Thread(() => {
            List<Release> found = null; History remote = null; string error = null;
            try {
                found = Updates.Fetch();
                var newest = found.FirstOrDefault(r => r.HistoryUrl != null);
                if (newest != null) remote = History.Parse(Updates.DownloadText(newest.HistoryUrl));
            } catch (Exception ex) { error = ex.Message; }
            try { BeginInvoke(new Action(() => OnReleases(found, remote, error, manual))); } catch (InvalidOperationException) { }
        }) { IsBackground = true }.Start();
    }
    void OnReleases(List<Release> found, History remote, string error, bool manual) {
        updateButton.Enabled = !busy;
        if (found == null) {
            updateButton.Text = "업데이트 확인";
            versionInfo.Text = "GitHub 배포 목록을 불러오지 못했습니다: " + error;
            if (manual) Append("업데이트 확인 실패: " + error);
            return;
        }
        releases = found; history.Merge(remote);
        var installed = InstalledVersion();
        latest = found.FirstOrDefault(r => !Regex.IsMatch(r.Version, @"-rc\d*$"));
        bool newer = latest != null && (installed == null || Updates.Compare(latest.Version, installed) > 0);
        bool localDiagnostic = InstalledLocalDiagnostic(installed);
        string updateStatus = Updates.StatusText(installed, localDiagnostic, latest != null ? latest.Version : null);
        updateButton.Text = localDiagnostic && !newer ? "로컬 개발판 · 공개 " + (latest != null ? latest.Version : "미확인") : updateStatus;
        updateButton.BackColor = newer ? Theme.Accent : Theme.Header;
        updateButton.ForeColor = newer ? Color.White : Theme.HeaderText;
        versionInfo.Text = "현재 설치: " + (installed ?? "없음") + (localDiagnostic ? " (로컬 개발판)" : "") + "    ·    공개 최신: " + (latest != null ? latest.Version : "-");
        versionList.Items.Clear();
        foreach (var r in found.Where(r => Updates.Supported(r.Version))) {
            string mark = !localDiagnostic && r.Version == installed ? "  (현재 공개판)" : r == latest ? "  (공개 최신)" : "";
            versionList.Items.Add(new ReleaseItem { Release = r, Text = r.Version + "   " + r.Published + mark });
        }
        // 가장 긴 항목(예: "v15   2026-11-01  (현재)")이 잘리지 않게 목록 폭을 맞춘다(2026-11-01 사용자).
        int listWidth = versionList.Width;
        foreach (ReleaseItem item in versionList.Items)
            listWidth = Math.Max(listWidth, TextRenderer.MeasureText(item.Text, versionList.Font).Width + SystemInformation.VerticalScrollBarWidth + 24);
        versionList.Width = listWidth;
        if (versionList.Items.Count > 0) versionList.SelectedIndex = 0;
        if (manual) Append(updateStatus);
        ApplyDefaultChanges();
    }
    void OnUpdateButton() {
        if (busy) return;
        var installed = InstalledVersion();
        if (latest != null && (installed == null || Updates.Compare(latest.Version, installed) > 0)) StartVersionChange(latest);
        else CheckUpdates(true);
    }
    // 기본 색 표시: 받을 업데이트가 있으면 설치본→최신, 없으면 지난 업데이트(이전→현재)로 바뀐 기능.
    void ApplyDefaultChanges() {
        if (history == null) return;
        var installed = InstalledVersion();
        if (latest != null && installed != null && Updates.Compare(latest.Version, installed) > 0)
            ShowChanges(installed, latest.Version, "업데이트하면 바뀌는 기능");
        else {
            var previous = PreviousVersion();
            ShowChanges(previous, installed, previous != null ? "지난 업데이트로 바뀐 기능" : null);
        }
    }
    void ShowChanges(string from, string to, string reason) {
        changes = reason != null && history != null ? history.Diff(from, to, null) : new Dictionary<string, int>();
        changeLegend = changes.Count > 0 ? reason + " (" + history.Known(from) + " → " + history.Known(to) + "): 초록 새 기능 " + changes.Count(c => c.Value == 1) + "개 · 주황 바뀐 기능 " + changes.Count(c => c.Value == 2) + "개" : "";
        foreach (var row in rows) { int kind; row.SetChange(changes.TryGetValue(row.Item.Id, out kind) ? kind : 0); }
        if (!syncing) { SyncRows(); UpdateSummary(); }
    }
    void OnVersionSelected() {
        var item = versionList.SelectedItem as ReleaseItem;
        if (item == null || history == null) { versionAction.Enabled = false; return; }
        var r = item.Release; var installed = InstalledVersion();
        int order = installed == null ? 1 : Updates.Compare(r.Version, history.Known(installed) ?? installed);
        var removed = new List<string>();
        var diff = history.Diff(installed, r.Version, removed);
        var text = new StringBuilder();
        text.Append(r.Title.Length > 0 ? r.Title : r.Version).Append("  (").Append(r.Published).Append(")\r\n\r\n");
        if (diff.Count > 0 || removed.Count > 0) {
            text.Append("현재 설치(").Append(history.Known(installed)).Append(") → ").Append(r.Version).Append("\r\n");
            foreach (var f in Catalog.Items.Where(f => diff.ContainsKey(f.Id)))
                text.Append(diff[f.Id] == 1 ? "  [새 기능] " : "  [바뀜] ").Append(f.Label).Append(" (").Append(f.Tab).Append(" 탭)\r\n");
            foreach (var id in removed) text.Append("  [없어짐] ").Append(id).Append("\r\n");
            text.Append("\r\n");
        } else if (installed != null && history.Known(r.Version) == null) text.Append("(이 배포의 기능 목록이 없어 바뀌는 기능을 표시하지 못합니다)\r\n\r\n");
        text.Append("── 패치 노트 ──\r\n").Append(r.Notes);
        notesBox.Text = text.ToString();
        if (Visible && pages.ContainsKey("업데이트") && pages["업데이트"].Visible)
            ShowChanges(installed, r.Version, order > 0 ? "이 버전으로 업데이트하면 바뀌는 기능" : order < 0 ? "이 버전으로 되돌리면 달라지는 기능" : null);
        versionAction.Text = order > 0 ? r.Version + "(으)로 업데이트" : order < 0 ? r.Version + "(으)로 되돌리기" : "현재 설치된 버전";
        versionAction.Enabled = order != 0 && !busy;
        versionDiff.Text = changeLegend.Length > 0 ? changeLegend : "기능 탭의 색 표시는 이 버전과 현재 설치본의 차이입니다.";
    }

    // 업데이트·되돌리기: 내려받기 → SHA-256 검사 → (되돌리기면 현재 관리.ps1로 해제) → 배포 파일 복사 →
    // 그 버전의 관리.ps1 Apply. 업데이트면 새 설치 관리자로 바꾼 뒤 자동으로 다시 연다.
    void StartVersionChange(Release r) {
        if (busy) return;
        if (GameRunning()) { Append("→ 거부: 게임을 완전히 종료한 뒤 다시 시도해 주세요."); MessageBox.Show(this, "게임을 완전히 종료한 뒤 다시 시도해 주세요.", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
        var installed = InstalledVersion();
        bool rollback = installed != null && Updates.Compare(r.Version, history != null ? history.Known(installed) ?? installed : installed) < 0;
        if (rollback && MessageBox.Show(this, r.Version + "(으)로 되돌립니다. 현재 모드를 해제한 뒤 그 버전을 설치합니다.\n" +
                "기능 설정은 updates 폴더에 백업하며, 설치 관리자는 지금 버전을 그대로 씁니다. 진행할까요?",
                Text, MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) { Append("→ 취소: 버전 되돌리기"); return; }
        try { if (dirty) { FeatureIni.Load(settings).Save(settings, values); dirty = false; UpdateSummary(); } }
        catch (UnauthorizedAccessException) { OfferElevation(); return; }
        ShowTab("작업 기록");
        Append(""); Append("==== " + (rollback ? "버전 되돌리기 → " : "업데이트 → ") + r.Version + " (" + DateTime.Now.ToString("HH:mm:ss") + ") ====");
        AppendHeaderInfo();
        AppendOtherMods();
        SetBusy(true);
        new Thread(() => {
            string relaunch = null, error = null;
            try { relaunch = ChangeVersion(r, installed, rollback); }
            catch (UnauthorizedAccessException) { error = "권한"; }
            catch (Exception ex) { error = ex.Message; }
            try { BeginInvoke(new Action(() => FinishVersionChange(r, rollback, relaunch, error))); } catch (InvalidOperationException) { }
        }) { IsBackground = true }.Start();
    }
    string ChangeVersion(Release r, string installed, bool rollback) {
        var dir = Path.Combine(project, @"updates\" + r.Version);
        Directory.CreateDirectory(dir);
        var zip = Path.Combine(dir, r.ZipName);
        if (File.Exists(zip) && Updates.Sha256(zip) == r.ZipSha) Append("이미 받아 둔 파일을 씁니다: " + zip);
        else { Append("내려받기: " + r.ZipUrl); Updates.Download(r, zip, Append); }
        Append("SHA-256 확인: " + r.ZipSha);
        var files = Path.Combine(dir, "files");
        Updates.Extract(zip, files);
        var list = Folders.PackageFiles(files);
        if (list == null || !File.Exists(Path.Combine(files, @"AI_TacticsMod\관리.ps1"))) throw new InvalidDataException("배포 ZIP에 관리 스크립트·파일 목록이 없습니다.");
        if (File.Exists(settings)) File.Copy(settings, Path.Combine(dir, "기능설정_" + (installed ?? "none") + "_" + DateTime.Now.ToString("yyyyMMdd_HHmmss") + ".ini"), true);
        if (rollback && File.Exists(manager)) {
            Append("현재 모드 해제(원본 복구)…");
            if (RunManagerSync("Restore") != 0) throw new InvalidOperationException("현재 모드를 해제하지 못했습니다. 되돌리기를 중단합니다.");
        }
        var running = Path.GetFullPath(Application.ExecutablePath);
        string installer = null;
        var copy = new List<string>();
        foreach (var rel in list) {
            bool rootExe = !rel.Contains('\\') && rel.EndsWith(".exe", StringComparison.OrdinalIgnoreCase);
            if (!rootExe) { copy.Add(rel); continue; }
            if (rollback) continue;   // 되돌리기는 지금 설치 관리자를 그대로 둔다(다시 업데이트할 수 있게)
            installer = Path.Combine(files, rel);
            if (!string.Equals(Path.GetFullPath(Path.Combine(game, rel)), running, StringComparison.OrdinalIgnoreCase)) copy.Add(rel);
        }
        Append("배포 파일 " + Folders.Copy(files, game, copy) + "개 복사");
        Append(r.Version + " 관리 스크립트로 적용…");
        if (RunManagerSync("Apply") != 0) throw new InvalidOperationException(r.Version + " 적용에 실패했습니다. 위 메시지를 확인해 주세요.");
        Directory.CreateDirectory(Path.GetDirectoryName(UpdateState));
        File.WriteAllText(UpdateState, "{\r\n  \"installed\": \"" + r.Version + "\",\r\n  \"previous\": \"" + (installed ?? "") + "\",\r\n  \"changed_at\": \"" +
            DateTime.Now.ToString("s") + "\",\r\n  \"zip_sha256\": \"" + r.ZipSha + "\"\r\n}\r\n", new UTF8Encoding(false));
        if (installer != null && !File.ReadAllBytes(installer).SequenceEqual(File.ReadAllBytes(running))) return installer;
        return null;
    }
    int RunManagerSync(string mode) {
        var ps = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), @"Sysnative\WindowsPowerShell\v1.0\powershell.exe");
        if (!File.Exists(ps)) ps = Path.Combine(Environment.SystemDirectory, @"WindowsPowerShell\v1.0\powershell.exe");
        var command = "[Console]::OutputEncoding=[Text.Encoding]::UTF8; & '" + manager.Replace("'", "''") + "' -Action " + mode +
                      " -GameDirectory '" + game.Replace("'", "''") + "' -NonInteractive; exit $LASTEXITCODE";
        var info = new ProcessStartInfo(ps, "-NoProfile -STA -ExecutionPolicy Bypass -Command \"" + command.Replace("\"", "\\\"") + "\"") {
            UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8, WorkingDirectory = project
        };
        using (var process = new Process { StartInfo = info }) {
            process.OutputDataReceived += (s, e) => { if (e.Data != null) Append(e.Data); };
            process.ErrorDataReceived += (s, e) => { if (e.Data != null) Append(e.Data); };
            process.Start(); process.BeginOutputReadLine(); process.BeginErrorReadLine(); process.WaitForExit();
            return process.ExitCode;
        }
    }
    void FinishVersionChange(Release r, bool rollback, string relaunch, string error) {
        SetBusy(false); LoadValues(); RefreshStatus();
        if (error == "권한") { Append("→ 실패: 게임 폴더에 쓸 권한이 없습니다."); OfferElevation(); return; }
        if (error != null) { Append("→ 실패: " + error); MessageBox.Show(this, error, Text, MessageBoxButtons.OK, MessageBoxIcon.Error); CheckUpdates(false); return; }
        Append("→ 완료: " + r.Version + (rollback ? "(으)로 되돌렸습니다." : "(으)로 업데이트했습니다."));
        versionChanged = true;
        if (relaunch != null) {
            var running = Path.GetFullPath(Application.ExecutablePath);
            // PW31(사용자 2026-11-09): 내려받은 폴더에서 실행 중이면 그 폴더의 EXE를 바꾸지 않고,
            // 게임 폴더에 새로 들어간 설치 관리자를 같은 인수로 다시 연다(다시 열린 설치 관리자는 package=게임 폴더).
            var gameInstaller = Path.Combine(game, Path.GetFileName(relaunch));
            bool runningInGame = string.Equals(Path.GetFullPath(Path.GetDirectoryName(running)), Path.GetFullPath(game), StringComparison.OrdinalIgnoreCase);
            if (!runningInGame && File.Exists(gameInstaller)) {
                try {
                    Process.Start(new ProcessStartInfo(gameInstaller, "\"" + game + "\" --updated=" + r.Version) { UseShellExecute = false, WorkingDirectory = game });
                    Append("게임 폴더의 새 설치 관리자를 다시 엽니다: " + gameInstaller);
                    dirty = false; Close(); return;
                } catch (Exception ex) {
                    Append("게임 폴더의 설치 관리자를 열지 못해 내려받은 폴더의 설치 관리자를 바꿉니다: " + ex.Message);
                }
            }
            // 실행 중인 EXE는 지울 수 없지만 이름은 바꿀 수 있다: .old로 옮기고 새 EXE를 그 자리에 둔 뒤 다시 연다.
            try {
                var old = running + ".old";
                if (File.Exists(old)) File.Delete(old);
                File.Move(running, old);
                File.Copy(relaunch, running, true);
                Process.Start(new ProcessStartInfo(running, "\"" + game + "\" --updated=" + r.Version) { UseShellExecute = false, WorkingDirectory = Path.GetDirectoryName(running) });
                dirty = false; Close(); return;
            } catch (Exception ex) {
                if (!File.Exists(running) && File.Exists(running + ".old")) try { File.Move(running + ".old", running); } catch { }
                Append("설치 관리자 교체 실패(모드 업데이트는 끝났습니다): " + ex.Message);
            }
        }
        CheckUpdates(false);
    }

    void SetGame(string dir) {
        game = Path.GetFullPath(dir);
        project = Path.Combine(game, "AI_TacticsMod");
        settings = Path.Combine(project, "기능설정.ini");
        manager = Path.Combine(project, "관리.ps1");
        packageCopyApproved = false;
        try { Icon = Icon.ExtractAssociatedIcon(Path.Combine(game, Folders.GameExe)); } catch { }
        LoadValues(); RefreshStatus();
    }

    void BuildHeader() {
        // 앵커는 추가 시점의 부모 폭을 기준으로 하므로 폼 폭을 먼저 준다.
        var header = new Panel { Dock = DockStyle.Top, Height = 118, Width = ClientSize.Width, BackColor = Theme.Header };
        var title = new Label { Text = "AI 모드 설치 관리자", Font = Theme.Ui(17f, FontStyle.Bold), ForeColor = Theme.HeaderText, AutoSize = true, Location = new Point(24, 14) };
        var bar = new Panel { BackColor = Theme.Accent, Location = new Point(24, 54), Size = new Size(48, 3) };
        status.Font = Theme.Ui(9.5f); status.ForeColor = Theme.HeaderMuted; status.AutoSize = false; status.AutoEllipsis = true;
        status.Location = new Point(24, 64); status.Size = new Size(ClientSize.Width - 48, 20); status.Anchor = AnchorStyles.Left | AnchorStyles.Top | AnchorStyles.Right;
        folder.Font = Theme.Ui(9.5f); folder.ForeColor = Theme.HeaderMuted; folder.AutoSize = false; folder.AutoEllipsis = true;
        folder.Location = new Point(24, 88); folder.Size = new Size(ClientSize.Width - 200, 20); folder.Anchor = AnchorStyles.Left | AnchorStyles.Top | AnchorStyles.Right;
        // 사용자 2026-11-01: 버튼 글자가 잘리지 않게 글자 폭(DPI 포함)에 맞춰 늘린다.
        Func<string, Button> headerButton = text => {
            var b = new Button {
                Text = text, FlatStyle = FlatStyle.Flat, ForeColor = Theme.HeaderText, BackColor = Theme.Header,
                Font = Theme.Ui(9.5f, FontStyle.Bold), AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink,
                MinimumSize = new Size(120, 32), Padding = new Padding(10, 2, 10, 2), Cursor = Cursors.Hand, Margin = new Padding(8, 0, 0, 0)
            };
            b.FlatAppearance.BorderColor = Theme.Accent; b.FlatAppearance.MouseOverBackColor = Color.FromArgb(48, 54, 68);
            return b;
        };
        rollbackButton = headerButton("버전 되돌리기…");
        rollbackButton.Click += (s, e) => ShowTab("업데이트");
        updateButton = headerButton("업데이트 확인");
        updateButton.Click += (s, e) => OnUpdateButton();
        var change = headerButton("게임 폴더 변경…");
        change.Click += (s, e) => ChangeFolder();
        actions.Add(change); actions.Add(updateButton); actions.Add(rollbackButton);
        var buttons = new FlowLayoutPanel { AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, WrapContents = false, BackColor = Theme.Header, Location = new Point(0, 16) };
        buttons.Controls.Add(rollbackButton); buttons.Controls.Add(updateButton); buttons.Controls.Add(change);
        Action placeButtons = () => buttons.Left = Math.Max(title.Right + 16, header.ClientSize.Width - buttons.Width - 20);
        buttons.SizeChanged += (s, e) => placeButtons();
        header.Resize += (s, e) => placeButtons();
        header.Controls.Add(title); header.Controls.Add(bar); header.Controls.Add(status); header.Controls.Add(folder); header.Controls.Add(buttons);
        placeButtons();
        var creditsSummary = new Label {
            Name = "creditsSummary", Text = Credits.Summary, Font = Theme.Ui(9f), ForeColor = Theme.HeaderText,
            Location = new Point(24, 114), AutoSize = false
        };
        header.Controls.Add(creditsSummary);
        Action layoutCredits = () => {
            int width = Math.Max(200, header.ClientSize.Width - 48);
            var size = TextRenderer.MeasureText(creditsSummary.Text, creditsSummary.Font, new Size(width, 0), TextFormatFlags.WordBreak | TextFormatFlags.TextBoxControl);
            creditsSummary.Size = new Size(width, size.Height + 4);
            header.Height = creditsSummary.Bottom + 12;
        };
        header.Resize += (s, e) => layoutCredits();
        layoutCredits();
        Controls.Add(header);
    }

    void ChangeFolder() {
        if (busy) return;
        if (dirty && MessageBox.Show(this, "저장하지 않은 기능 선택은 버려집니다. 폴더를 바꿀까요?", Text,
                MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) { Append("→ 취소: 게임 폴더 변경"); return; }
        var dir = Folders.Pick(this, game);
        if (dir == null) return;
        bool hasMod = File.Exists(Path.Combine(dir, @"AI_TacticsMod\관리.ps1"));
        if (!hasMod && (package == null || Folders.PackageFiles(package) == null)) {
            Append("→ 거부: 선택한 폴더에 AI 모드 파일이 없고, 복사할 배포 파일 목록도 찾지 못했습니다. 배포 ZIP을 풀어 둔 폴더의 설치 관리자를 실행해 주세요.");
            MessageBox.Show(this, "선택한 폴더에 AI 모드 파일이 없고, 복사할 배포 파일 목록도 찾지 못했습니다.\n배포 ZIP을 풀어 둔 폴더의 설치 관리자를 실행해 주세요.",
                Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }
        SetGame(dir); Folders.Remember(game);
        Append("설치 대상 폴더: " + game);
    }

    void BuildTabs() {
        var strip = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, BackColor = Theme.Card, Padding = new Padding(18, 8, 18, 2), WrapContents = true };
        foreach (var name in Catalog.Tabs.Concat(new[] { "출처", "업데이트", "작업 기록" })) {
            var tab = name;
            var b = new RadioButton {
                Appearance = Appearance.Button, FlatStyle = FlatStyle.Flat, AutoSize = true, MinimumSize = new Size(128, 42),
                Padding = new Padding(10, 0, 10, 0), TextAlign = ContentAlignment.MiddleCenter, Font = Theme.Ui(10.5f, FontStyle.Bold),
                Cursor = Cursors.Hand, BackColor = Theme.Card, ForeColor = Theme.Muted, Margin = new Padding(0, 0, 4, 0), Text = tab
            };
            b.FlatAppearance.BorderSize = 0; b.FlatAppearance.CheckedBackColor = Theme.Card; b.FlatAppearance.MouseOverBackColor = Theme.Page;
            b.Paint += (s, e) => {
                if (!b.Checked) return;
                using (var brush = new SolidBrush(Theme.Accent)) e.Graphics.FillRectangle(brush, 10, b.Height - 4, b.Width - 20, 4);
            };
            b.CheckedChanged += (s, e) => { b.ForeColor = b.Checked ? Theme.Text : Theme.Muted; if (b.Checked) ShowTab(tab); b.Invalidate(); };
            tabButtons[tab] = b; strip.Controls.Add(b);
        }
        var line = new Panel { Dock = DockStyle.Top, Height = 1, BackColor = Theme.Border };
        Controls.Add(line); Controls.Add(strip);
    }

    // 탭마다 독립 기능을 차례로, 같은 탭의 종속 기능은 상위 바로 아래에 들여 쓴다.
    static List<KeyValuePair<Feature, bool>> TabOrder(string tab) {
        var items = Catalog.Items.Where(f => f.Tab == tab && !Catalog.Retired.Contains(f.Id)).ToList();
        var order = new List<KeyValuePair<Feature, bool>>();
        foreach (var f in items) {
            bool nestedHere = f.IsChild && items.Any(p => p.Id == f.Requires);
            if (nestedHere) continue;
            order.Add(new KeyValuePair<Feature, bool>(f, false));
            foreach (var c in items.Where(c => c.Requires == f.Id)) order.Add(new KeyValuePair<Feature, bool>(c, true));
        }
        return order;
    }

    void BuildBody() {
        content.Dock = DockStyle.Fill; content.Padding = new Padding(24, 16, 24, 8); content.BackColor = Theme.Page;
        foreach (var tab in Catalog.Tabs) {
            var page = new Panel { Dock = DockStyle.Fill, AutoScroll = true, BackColor = Theme.Page, Visible = false };
            var card = new Panel { Dock = DockStyle.Top, AutoSize = true, BackColor = Theme.Border, Padding = new Padding(1) };
            var order = TabOrder(tab);
            // Dock=Top는 뒤에 추가한 것이 위로 가므로 역순으로 넣는다.
            for (int i = order.Count - 1; i >= 0; --i) {
                var row = new FeatureRow(order[i].Key, order[i].Value);
                row.Box.CheckedChanged += (s, e) => OnToggle(row);
                row.Choice.SelectedIndexChanged += (s, e) => OnChoice(row);
                row.Fix.LinkClicked += (s, e) => EnableChain(row.Item, true);
                rows.Add(row); card.Controls.Add(row);
            }
            page.Controls.Add(card);
            pages[tab] = page; content.Controls.Add(page);
        }
        var creditsPage = new Panel {
            Dock = DockStyle.Fill, AutoScroll = true, BackColor = Theme.Card, Visible = false,
            Padding = new Padding(16)
        };
        foreach (var text in new[] { Credits.Introduction }.Concat(Credits.Lines)) {
            creditsPage.Controls.Add(new Label {
                Text = text, AutoSize = false, ForeColor = Theme.Text, Margin = new Padding(0, 0, 0, 12),
                Font = Theme.Ui(10.5f, text == Credits.Introduction ? FontStyle.Bold : FontStyle.Regular)
            });
        }
        bool layingOutCredits = false;
        Action layoutCreditPage = () => {
            if (layingOutCredits) return;
            layingOutCredits = true;
            creditsPage.AutoScrollPosition = Point.Empty;
            int width = Math.Max(200, creditsPage.ClientSize.Width - creditsPage.Padding.Horizontal - SystemInformation.VerticalScrollBarWidth);
            int top = creditsPage.Padding.Top;
            foreach (Control label in creditsPage.Controls) {
                var size = TextRenderer.MeasureText(label.Text, label.Font, new Size(width, 0), TextFormatFlags.WordBreak | TextFormatFlags.TextBoxControl);
                label.Location = new Point(creditsPage.Padding.Left, top);
                label.Size = new Size(width, size.Height + 4);
                top = label.Bottom + label.Margin.Bottom;
            }
            creditsPage.AutoScrollMinSize = new Size(0, top + creditsPage.Padding.Bottom);
            layingOutCredits = false;
        };
        creditsPage.Resize += (s, e) => layoutCreditPage();
        creditsPage.VisibleChanged += (s, e) => { if (creditsPage.Visible) layoutCreditPage(); };
        pages["출처"] = creditsPage; content.Controls.Add(creditsPage);
        // 업데이트 탭: 왼쪽 배포 목록, 오른쪽 패치 노트와 바뀌는 기능, 아래 실행 버튼.
        var updatePage = new Panel { Dock = DockStyle.Fill, BackColor = Theme.Page, Visible = false };
        versionInfo.Dock = DockStyle.Top; versionInfo.Height = 30; versionInfo.Font = Theme.Ui(10.5f, FontStyle.Bold); versionInfo.ForeColor = Theme.Text;
        versionInfo.Text = "배포 목록을 불러오는 중…";
        versionList.Dock = DockStyle.Left; versionList.Width = 340; versionList.Font = Theme.Ui(10.5f); versionList.IntegralHeight = false;
        versionList.SelectedIndexChanged += (s, e) => OnVersionSelected();
        notesBox.Multiline = true; notesBox.ReadOnly = true; notesBox.ScrollBars = ScrollBars.Vertical; notesBox.Dock = DockStyle.Fill;
        notesBox.Font = Theme.Ui(9.5f); notesBox.BackColor = Theme.Card; notesBox.BorderStyle = BorderStyle.FixedSingle;
        var bottom = new Panel { Dock = DockStyle.Bottom, Height = 54, BackColor = Theme.Page, Padding = new Padding(0, 8, 0, 0) };
        versionDiff.Dock = DockStyle.Fill; versionDiff.Font = Theme.Ui(9.5f); versionDiff.ForeColor = Theme.Muted; versionDiff.TextAlign = ContentAlignment.MiddleLeft; versionDiff.AutoEllipsis = true;
        versionAction = new FlatButton("이 버전 설치", true) { Dock = DockStyle.Right, Enabled = false };
        versionAction.Click += (s, e) => { var r = versionList.SelectedItem as ReleaseItem; if (r != null) StartVersionChange(r.Release); };
        bottom.Controls.Add(versionDiff); bottom.Controls.Add(versionAction);
        var notesHolder = new Panel { Dock = DockStyle.Fill, Padding = new Padding(10, 0, 0, 0), BackColor = Theme.Page };
        notesHolder.Controls.Add(notesBox);
        updatePage.Controls.Add(notesHolder); updatePage.Controls.Add(versionList); updatePage.Controls.Add(bottom); updatePage.Controls.Add(versionInfo);
        updatePage.VisibleChanged += (s, e) => { if (!updatePage.Visible) ApplyDefaultChanges(); else OnVersionSelected(); };
        pages["업데이트"] = updatePage; content.Controls.Add(updatePage);
        log.Multiline = true; log.ReadOnly = true; log.ScrollBars = ScrollBars.Vertical; log.Dock = DockStyle.Fill;
        log.Font = new Font("Consolas", 9.5f); log.BackColor = Color.FromArgb(24, 26, 32); log.ForeColor = Color.FromArgb(220, 220, 214);
        log.BorderStyle = BorderStyle.None;
        logPage.Dock = DockStyle.Fill; logPage.Visible = false; logPage.Padding = new Padding(10); logPage.BackColor = Color.FromArgb(24, 26, 32);
        logPage.Controls.Add(log); content.Controls.Add(logPage);
        Controls.Add(content);
    }

    void BuildFooter() {
        var footer = new Panel { Dock = DockStyle.Bottom, Height = 108, BackColor = Theme.Card, Padding = new Padding(24, 10, 24, 14) };
        var line = new Panel { Dock = DockStyle.Top, Height = 1, BackColor = Theme.Border };
        summary.Dock = DockStyle.Top; summary.Height = 28; summary.ForeColor = Theme.Muted; summary.Font = Theme.Ui(9.5f);
        summary.TextAlign = ContentAlignment.MiddleLeft; summary.AutoEllipsis = true;
        var left = new FlowLayoutPanel { Dock = DockStyle.Left, AutoSize = true, WrapContents = false, Padding = new Padding(0, 8, 0, 0) };
        var right = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, WrapContents = false, FlowDirection = FlowDirection.LeftToRight, Padding = new Padding(0, 8, 0, 0) };
        var defaults = new FlatButton("배포 기본값", false); defaults.Margin = new Padding(0);
        var allOn = new FlatButton("모두 켜기", false);
        var restore = new FlatButton("모드 해제 (원본 복구)", false); restore.ForeColor = Theme.Bad;
        var save = new FlatButton("설정만 저장", false);
        var apply = new FlatButton("설치 / 적용", true);
        defaults.Click += (s, e) => SetAll(f => f.Default);
        allOn.Click += (s, e) => SetAll(f => Catalog.Choices.ContainsKey(f.Id) ? f.Default : 1);
        save.Click += (s, e) => Run("Select", "기능 설정 저장");
        apply.Click += (s, e) => Run("Apply", "모드 설치·적용");
        restore.Click += (s, e) => {
            if (MessageBox.Show(this, "AI 모드 DLL을 해제하고 설치 전 상태로 복구합니다.\n원본 EXE·세이브·기능 설정 파일은 그대로 둡니다. 진행할까요?",
                    Text, MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.Yes) Run("Restore", "모드 해제");
        };
        left.Controls.Add(defaults); left.Controls.Add(allOn);
        right.Controls.Add(restore); right.Controls.Add(save); right.Controls.Add(apply);
        actions.AddRange(new Control[] { defaults, allOn, restore, save, apply });
        footer.Controls.Add(left); footer.Controls.Add(right); footer.Controls.Add(summary); footer.Controls.Add(line);
        Controls.Add(footer);
    }

    void ShowTab(string tab) {
        foreach (var p in pages) p.Value.Visible = p.Key == tab;
        logPage.Visible = tab == "작업 기록";
        if (!tabButtons[tab].Checked) tabButtons[tab].Checked = true;
    }

    void LoadValues() {
        FeatureIni ini;
        try { ini = FeatureIni.Load(settings); }
        catch (Exception ex) { ini = new FeatureIni(); Append("기능 설정을 읽지 못해 기본값을 표시합니다: " + ex.Message); }
        foreach (var f in Catalog.Items) values[f.Id] = ini.Value(f);
        lastLinked = ""; SyncRows(); dirty = false; UpdateSummary();
    }

    void SetAll(Func<Feature, int> pick) {
        foreach (var f in Catalog.Items) values[f.Id] = Catalog.Retired.Contains(f.Id) ? 0 : pick(f);
        lastLinked = ""; SyncRows(); dirty = true; UpdateSummary();
    }

    // 실제로 설치·동작하는지: 자신 + 묶음 스위치 + 상위 기능이 모두 켜져야 한다.
    bool Effective(Feature f) {
        if (Catalog.Retired.Contains(f.Id)) return false;
        if (values[f.Id] == 0) return false;
        if (Blocker(f) != null) return false;
        if (!f.IsMaster && values["groups." + f.Group] == 0) return false;
        return !f.IsChild || Effective(Catalog.ById(f.Requires));
    }

    Feature Blocker(Feature f) {
        string by;
        if (!Catalog.ExcludedBy.TryGetValue(f.Id, out by)) return null;
        var b = Catalog.ById(by);
        return values[b.Id] != 0 && (b.IsMaster || values["groups." + b.Group] != 0) ? b : null;
    }

    // 종속 기능을 켜면 묶음 스위치와 상위 기능을 함께 켠다.
    void EnableChain(Feature f, bool self) {
        if (Catalog.Retired.Contains(f.Id)) { values[f.Id] = 0; SyncRows(); UpdateSummary(); return; }
        var turned = new List<string>();
        Action<Feature> on = null;
        on = x => {
            if (!x.IsMaster && values["groups." + x.Group] == 0) { values["groups." + x.Group] = 1; turned.Add(Catalog.Master(x.Group).Label); }
            if (x.IsChild) {
                var parent = Catalog.ById(x.Requires);
                if (values[parent.Id] == 0) { values[parent.Id] = 1; turned.Add(parent.Label); }
                on(parent);
            }
        };
        if (self) values[f.Id] = 1;
        on(f);
        lastLinked = turned.Count > 0 ? "함께 켬: " + string.Join(", ", turned.Select(t => "‘" + t + "’")) : "";
        dirty = true; SyncRows(); UpdateSummary();
    }

    void OnToggle(FeatureRow row) {
        if (syncing) return;
        if (Catalog.Retired.Contains(row.Item.Id)) { values[row.Item.Id] = 0; SyncRows(); UpdateSummary(); return; }
        if (row.Box.Checked) {
            var blocker = Blocker(row.Item);
            if (blocker != null) {
                values[row.Item.Id] = 0; lastLinked = "‘" + blocker.Label + "’과(와) 함께 쓸 수 없습니다.";
                SyncRows(); UpdateSummary(); return;
            }
            EnableChain(row.Item, true);
            // 이 기능과 함께 쓸 수 없는 기능은 끈다.
            var off = Catalog.ExcludedBy.Where(e => e.Value == row.Item.Id && values[e.Key] != 0).Select(e => e.Key).ToList();
            foreach (var id in off) values[id] = 0;
            if (off.Count > 0) { lastLinked = "함께 쓸 수 없어 끔: " + string.Join(", ", off.Select(id => "‘" + Catalog.ById(id).Label + "’")); SyncRows(); UpdateSummary(); }
            return;
        }
        values[row.Item.Id] = 0;
        int dependents = Catalog.Items.Count(c => (c.Requires == row.Item.Id || (row.Item.IsMaster && c.Group == row.Item.Key)) && values[c.Id] != 0);
        lastLinked = dependents > 0 ? "이 항목에 딸린 기능 " + dependents + "개도 함께 동작하지 않습니다(선택은 보존)." : "";
        dirty = true; SyncRows(); UpdateSummary();
    }

    void OnChoice(FeatureRow row) {
        if (syncing || !Catalog.Choices.ContainsKey(row.Item.Id) || row.Choice.SelectedIndex < 0) return;
        int selected = row.Choice.SelectedIndex + Catalog.ChoiceBase(row.Item.Id);
        values[row.Item.Id] = selected;
        if (row.Item.Id == "diagnostics.terrain_diag_mode") {
            if (selected != 0 && values["groups.diagnostics"] == 0) values["groups.diagnostics"] = 1;
            lastLinked = selected == 0 ? "" : "A/B/C 진단 모드 — 지형 편집·저장 금지";
        }
        dirty = true; SyncRows(); UpdateSummary();
    }

    void SyncRows() {
        syncing = true;
        foreach (var row in rows) {
            var f = row.Item;
            if (Catalog.Retired.Contains(f.Id)) {
                values[f.Id] = 0; row.Box.Checked = false; row.Box.Enabled = false;
                row.SetState(false, Catalog.ExcludedBy.ContainsKey(f.Id) ? Catalog.RetiredNote : f.Note, null); continue;
            }
            if (Catalog.Choices.ContainsKey(f.Id)) {
                int choiceBase = Catalog.ChoiceBase(f.Id);
                int selected = Math.Max(0, Math.Min(Catalog.Choices[f.Id].Length - 1, values[f.Id] - choiceBase));
                if (row.Choice.SelectedIndex != selected) row.Choice.SelectedIndex = selected;
                row.Choice.Enabled = !busy;
                row.Box.Checked = f.Id == "diagnostics.terrain_diag_mode" ? selected != 0 : true;
                row.Box.Enabled = false;
            }
            else {
            row.Box.Checked = values[f.Id] != 0;
            row.Box.Enabled = !busy;
            }
            if (f.IsMaster) { row.SetState(values[f.Id] != 0, f.Note, null); continue; }
            var master = Catalog.Master(f.Group);
            var parent = f.IsChild ? Catalog.ById(f.Requires) : null;
            string where = parent != null && parent.Tab != f.Tab ? " (" + parent.Tab + " 탭)" : "";
            var blocked = Blocker(f);
            if (blocked != null) {
                row.Box.Checked = false; row.Box.Enabled = false;
                row.SetState(false, "‘" + blocked.Label + "’과(와) 충돌해 설치되지 않습니다. 쓰려면 그 기능을 먼저 끄세요.", null);
            }
            else if (values[master.Id] == 0)
                row.SetState(false, "‘" + master.Label + "’이(가) 꺼져 있어 설치되지 않습니다 (" + master.Tab + " 탭).", "→ 함께 켜기");
            else if (parent != null && !Effective(parent))
                row.SetState(false, "종속 기능: ‘" + parent.Label + "’" + where + "이(가) 켜져 있어야 설치됩니다.", "→ ‘" + parent.Label + "’ 함께 켜기");
            else if (parent != null)
                row.SetState(values[f.Id] != 0, "종속 기능 · ‘" + parent.Label + "’" + where + "과 함께 설치됩니다." + (f.Note.Length > 0 ? " " + f.Note : ""), null);
            else row.SetState(true, f.Note, null);
        }
        syncing = false;
        foreach (var tab in Catalog.Tabs) {
            var items = Catalog.Items.Where(f => f.Tab == tab && !f.IsMaster && !Catalog.Retired.Contains(f.Id)).ToList();
            int changed = Catalog.Items.Count(f => f.Tab == tab && changes.ContainsKey(f.Id));
            tabButtons[tab].Text = tab + "  " + items.Count(Effective) + "/" + items.Count + (changed > 0 ? "  ●" + changed : "");
            tabButtons[tab].ForeColor = changed > 0 ? Theme.ChangedText : tabButtons[tab].Checked ? Theme.Text : Theme.Muted;
        }
    }

    void UpdateSummary() {
        int active = Catalog.Items.Count(f => !f.IsMaster && Effective(f));
        string text = "설치될 세부 기능 " + active + "개";
        if (changeLegend.Length > 0) text = changeLegend + "  ·  " + text;
        if (lastLinked.Length > 0) text += "  ·  " + lastLinked;
        text += dirty ? "  ·  변경 사항 있음 — ‘설치 / 적용’ 또는 ‘설정만 저장’" : "  ·  저장됨 (게임 재시작 시 적용)";
        summary.Text = text;
        summary.ForeColor = dirty ? Theme.AccentDark : Theme.Muted;
    }

    static bool GameRunning() {
        try { return Process.GetProcessesByName("SAN14CE_KO").Length > 0; } catch { return false; }
    }

    void RefreshStatus() {
        string state = "설치 기록 없음";
        try {
            var file = Path.Combine(project, @"backup\input_state.json");
            if (File.Exists(file)) {
                var json = File.ReadAllText(file);
                bool installed = Regex.IsMatch(json, "\"installed\"\\s*:\\s*true");
                var version = Regex.Match(json, "\"release_version\"\\s*:\\s*\"([^\"]+)\"");
                state = installed ? "설치됨" + (version.Success ? " (" + version.Groups[1].Value + ")" : "") : "해제됨";
            } else if (!File.Exists(manager)) state = "모드 파일 없음 — 설치 시 복사";
        } catch { state = "설치 상태 확인 실패"; }
        bool running = GameRunning();
        status.Text = "상태: " + state + "    ·    게임: " + (running ? "실행 중 — 종료 후 설치/해제하세요" : "종료됨");
        status.ForeColor = running ? Color.FromArgb(240, 150, 120) : Theme.HeaderMuted;
        folder.Text = "게임 폴더: " + game + (package != null && !Folders.Same(package, game) ? "    (배포 파일: " + package + ")" : "");
    }

    void Append(string line) {
        if (InvokeRequired) { BeginInvoke(new Action<string>(Append), line); return; }
        log.AppendText(line + Environment.NewLine);
        // 사용자 2026-11-09: 설치 거부까지 게임 폴더의 ai행동교정모드호환성.log에 남긴다.
        if (game == null) { pendingLog.Add(line); return; }
        if (pendingLog.Count > 0) { pendingLog.Add(line); foreach (var pending in pendingLog) WriteLogLine(pending); pendingLog.Clear(); return; }
        WriteLogLine(line);
    }

    // 화면에만 남긴다(게임이 이미 남긴 [게임] 줄을 되쓰지 않는다).
    void AppendTabOnly(string line) {
        if (InvokeRequired) { BeginInvoke(new Action<string>(AppendTabOnly), line); return; }
        log.AppendText(line + Environment.NewLine);
    }

    // 파일 기록은 실패해도 설치기 동작을 막지 않는다. 게임 DLL과 같은 512 KB 상한·같은 .old.log.
    void WriteLogLine(string line) {
        try {
            var file = Path.Combine(game, CompatLogName);
            if (File.Exists(file) && new FileInfo(file).Length > 512 * 1024) {
                var old = Path.Combine(game, CompatOldLogName);
                try { File.Delete(old); } catch { }
                File.Move(file, old);
            }
            // 게임이 파일을 열어 두어도 되도록 READ|WRITE 공유로 연다.
            using (var fs = new FileStream(file, FileMode.Append, FileAccess.Write, FileShare.ReadWrite))
            using (var writer = new StreamWriter(fs, new UTF8Encoding(false))) {
                writer.NewLine = "\r\n";
                writer.WriteLine(DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss ") + "[설치기] " + line);
            }
        } catch { }
    }

    // 작업 기록 탭 전용: 마지막 "=== 게임 실행" 블록의 [게임] 줄을 보여 준다(파일에는 쓰지 않는다).
    void ShowLastGameRun() {
        try {
            var file = Path.Combine(game, CompatLogName);
            if (!File.Exists(file)) { AppendTabOnly("── 호환성 기록 없음 (아직 게임을 실행하지 않음) ──"); return; }
            var lines = File.ReadAllLines(file, Encoding.UTF8);
            int start = -1;
            for (int i = 0; i < lines.Length; ++i) if (lines[i].Contains("=== 게임 실행")) start = i;
            if (start < 0) { AppendTabOnly("── 호환성 기록 없음 (아직 게임을 실행하지 않음) ──"); return; }
            var body = new List<string>();
            for (int i = start + 1; i < lines.Length; ++i) if (lines[i].Contains("[게임]")) body.Add(lines[i]);
            if (body.Count == 0) { AppendTabOnly("── 지난 게임 실행: 겹쳐 꺼진 기능 없음 ──"); return; }
            AppendTabOnly("── 지난 게임 실행에서 다른 모드와 겹쳐 꺼진 기능 ──");
            foreach (var line in body) AppendTabOnly(line);
        } catch { AppendTabOnly("── 호환성 기록 없음 (아직 게임을 실행하지 않음) ──"); }
    }

    // Apply/Update 시작: 우리 배포 파일 목록에 없는 게임 폴더의 SAN14_*.dll = 다른 모드.
    void AppendOtherMods() {
        try {
            var ours = new HashSet<string>(OurModules, StringComparer.OrdinalIgnoreCase);
            var files = package != null ? Folders.PackageFiles(package) : null;
            if (files != null) foreach (var rel in files) { var name = Path.GetFileName(rel); if (name.Length > 0) ours.Add(name); }
            var other = Directory.GetFiles(game, "SAN14_*.dll")
                .Select(Path.GetFileName)
                .Where(name => !ours.Contains(name))
                .OrderBy(name => name, StringComparer.OrdinalIgnoreCase)
                .ToList();
            Append(other.Count == 0 ? "함께 설치된 다른 모드: 없음" : "함께 설치된 다른 모드: " + string.Join(", ", other));
        } catch (Exception ex) { Append("함께 설치된 다른 모드: 확인 실패 (" + ex.Message + ")"); }
    }

    // 원본 EXE의 PE 타임스탬프(e_lfanew+8). 없거나 못 읽으면 "없음".
    string PeStamp() {
        try {
            var exe = Path.Combine(game, Folders.GameExe);
            if (!File.Exists(exe)) return "없음";
            using (var fs = new FileStream(exe, FileMode.Open, FileAccess.Read, FileShare.ReadWrite)) {
                var br = new BinaryReader(fs);
                fs.Position = 0x3C;
                int lfanew = br.ReadInt32();
                fs.Position = lfanew + 8;
                return "0x" + br.ReadUInt32().ToString("X8");
            }
        } catch { return "없음"; }
    }

    // 동작 머리글 다음 줄: 설치기 버전·게임 폴더·원본 EXE 타임스탬프.
    void AppendHeaderInfo() {
        Append("설치 관리자 " + Application.ProductVersion + " · 게임 폴더 " + game + " · SAN14CE_KO.exe 타임스탬프 " + PeStamp());
    }

    void SetBusy(bool value) {
        busy = value;
        foreach (var c in actions) c.Enabled = !value;
        SyncRows();
        UseWaitCursor = value;
    }

    // PW31(사용자 2026-11-09): 배포 폴더의 배포판 버전(manifest.json). 없거나 읽지 못하면 null(기존 동작).
    string PackageVersion() {
        if (package == null) return null;
        try {
            var file = Path.Combine(package, @"AI_TacticsMod\build\input_package\manifest.json");
            if (!File.Exists(file)) return null;
            var manifest = new JavaScriptSerializer().Deserialize<Dictionary<string, object>>(File.ReadAllText(file));
            object version;
            return manifest.TryGetValue("release_version", out version) ? version as string : null;
        } catch { return null; }
    }

    // 게임 폴더가 배포 폴더와 다르면 배포 파일을 먼저 복사한다(사용자 확인 후).
    // PW31(사용자 2026-11-09): 이번 실행에서 버전을 바꿨거나 배포 폴더가 설치본보다 오래됐으면 복사하지 않는다.
    // (옛 배포 ZIP 폴더에서 실행한 설치 관리자로 업데이트한 뒤 '적용'을 누르면 그 폴더의 옛 파일이 덮어쓰던 문제)
    bool EnsurePackage(bool install) {
        if (package == null || Folders.Same(package, game)) {
            if (File.Exists(manager) || !install) return true;
            Append("→ 거부: 게임 폴더에 AI 모드 파일(관리.ps1)이 없습니다.");
            return false;
        }
        if (versionChanged) {
            Append("이번 실행에서 버전을 바꿨으므로 배포 폴더(" + package + ")의 파일은 복사하지 않고 게임 폴더의 현재 파일로 적용합니다.");
            return File.Exists(manager);
        }
        string pkgVersion = PackageVersion();
        string installedVersion = InstalledVersion();
        if (pkgVersion != null && installedVersion != null && Updates.Compare(pkgVersion, installedVersion) < 0) {
            Append("배포 폴더(" + package + ")의 파일은 " + pkgVersion + "로 설치본 " + installedVersion +
                   "보다 오래되어 복사하지 않습니다. 게임 폴더의 현재 파일로 적용합니다.");
            return File.Exists(manager);
        }
        var files = Folders.PackageFiles(package);
        if (files == null) {
            if (File.Exists(manager)) return true;
            Append("→ 거부: 게임 폴더에 AI 모드 파일이 없고 배포 파일 목록도 없습니다.");
            MessageBox.Show(this, "게임 폴더에 AI 모드 파일이 없고 배포 파일 목록도 없습니다.", Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
            return false;
        }
        int changed = Folders.Changed(package, game, files);
        if (changed == 0) return true;
        if (!packageCopyApproved && MessageBox.Show(this,
                "배포 파일 " + changed + "개를 게임 폴더로 복사합니다.\n" + package + "\n→ " + game +
                "\n\n게임 실행 DLL은 복사 후 관리 스크립트가 검사·백업하며 설치합니다. 진행할까요?",
                Text, MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) { Append("→ 취소: 배포 파일 복사"); return false; }
        packageCopyApproved = true;
        int copied = Folders.Copy(package, game, files);
        Append("배포 파일 " + copied + "개 복사: " + package + " → " + game);
        if (!File.Exists(manager)) { Append("→ 거부: 배포 파일을 복사했지만 관리.ps1이 없습니다."); return false; }
        return true;
    }

    void Run(string mode, string title) {
        if (busy) return;
        if (mode != "Select" && GameRunning()) {
            Append("→ 거부: 게임을 완전히 종료한 뒤 다시 시도해 주세요.");
            MessageBox.Show(this, "게임을 완전히 종료한 뒤 다시 시도해 주세요.", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return;
        }
        try {
            if (mode == "Apply" && !EnsurePackage(true)) return;
            if (mode == "Restore" && !File.Exists(manager)) {
                Append("→ 거부: 이 게임 폴더에는 AI 모드 설치 기록이 없습니다.");
                MessageBox.Show(this, "이 게임 폴더에는 AI 모드 설치 기록이 없습니다.", Text, MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }
            if (mode != "Restore") { FeatureIni.Load(settings).Save(settings, values); dirty = false; UpdateSummary(); }
        }
        catch (UnauthorizedAccessException) { OfferElevation(); return; }
        catch (Exception ex) { Append("→ 거부: " + title + " 준비 중 오류: " + ex.Message); MessageBox.Show(this, title + " 준비 중 오류: " + ex.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error); return; }
        if (mode == "Select" && !File.Exists(manager)) {
            MessageBox.Show(this, "기능 설정을 저장했습니다. 모드 파일은 ‘설치 / 적용’ 때 복사됩니다.", Text, MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }
        ShowTab("작업 기록");
        Append("");
        Append("==== " + title + " (" + DateTime.Now.ToString("HH:mm:ss") + ") ====");
        AppendHeaderInfo();
        AppendOtherMods();
        SetBusy(true);
        var ps = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), @"Sysnative\WindowsPowerShell\v1.0\powershell.exe");
        if (!File.Exists(ps)) ps = Path.Combine(Environment.SystemDirectory, @"WindowsPowerShell\v1.0\powershell.exe");
        var command = "[Console]::OutputEncoding=[Text.Encoding]::UTF8; & '" + manager.Replace("'", "''") + "' -Action " + mode +
                      " -GameDirectory '" + game.Replace("'", "''") + "' -NonInteractive; exit $LASTEXITCODE";
        var info = new ProcessStartInfo(ps, "-NoProfile -STA -ExecutionPolicy Bypass -Command \"" + command.Replace("\"", "\\\"") + "\"") {
            UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true,
            StandardOutputEncoding = Encoding.UTF8, StandardErrorEncoding = Encoding.UTF8, WorkingDirectory = project
        };
        var process = new Process { StartInfo = info, EnableRaisingEvents = true };
        process.OutputDataReceived += (s, e) => { if (e.Data != null) Append(e.Data); };
        process.ErrorDataReceived += (s, e) => { if (e.Data != null) Append(e.Data); };
        process.Exited += (s, e) => BeginInvoke(new Action(() => Finish(process, mode, title)));
        try { process.Start(); process.BeginOutputReadLine(); process.BeginErrorReadLine(); }
        catch (Exception ex) { Append("PowerShell을 시작하지 못했습니다: " + ex.Message); SetBusy(false); }
    }

    void Finish(Process process, string mode, string title) {
        process.WaitForExit();
        int code = process.ExitCode; process.Dispose();
        SetBusy(false); LoadValues(); RefreshStatus();
        bool ok = code == 0;
        Append(ok ? "→ 완료" : "→ 실패 (종료 코드 " + code + ")");
        string text = ok ? title + "을(를) 마쳤습니다." + (mode == "Restore" ? "" : "\n게임을 다시 시작하면 선택한 기능이 적용됩니다.")
                         : title + "에 실패했습니다. ‘작업 기록’ 탭의 메시지를 확인해 주세요.";
        if (!ok && log.Text.Contains("권한")) { OfferElevation(); return; }
        MessageBox.Show(this, text, Text, MessageBoxButtons.OK, ok ? MessageBoxIcon.Information : MessageBoxIcon.Error);
    }

    void OfferElevation() {
        Append("→ 거부: 게임 폴더에 쓸 권한이 없습니다.");
        if (MessageBox.Show(this, "게임 폴더에 쓸 권한이 없습니다. 관리자 권한으로 다시 열까요?", Text,
                MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;
        try {
            Process.Start(new ProcessStartInfo(Application.ExecutablePath, "\"" + game + "\"") { UseShellExecute = true, Verb = "runas" });
            dirty = false; Close();
        } catch (Exception ex) { Append("관리자 권한 실행이 취소되었습니다: " + ex.Message); }
    }
}

static class Program {
    [DllImport("user32.dll")] static extern bool SetProcessDPIAware();

    [STAThread]
    static void Main(string[] args) {
        try { SetProcessDPIAware(); } catch { }
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        var here = Path.GetDirectoryName(Application.ExecutablePath);
        // 자동 업데이트가 남긴 이전 설치 관리자(.old)는 그 프로세스가 끝난 뒤 지운다.
        var leftover = Application.ExecutablePath + ".old";
        if (File.Exists(leftover)) new Thread(() => {
            for (int i = 0; i < 20 && File.Exists(leftover); ++i) { try { File.Delete(leftover); } catch { Thread.Sleep(500); } }
        }) { IsBackground = true }.Start();
        string updated = args.Where(a => a.StartsWith("--updated=")).Select(a => a.Substring(10)).FirstOrDefault();
        args = args.Where(a => !a.StartsWith("--")).ToArray();
        string package = File.Exists(Path.Combine(here, @"AI_TacticsMod\관리.ps1")) ? here : null;
        // 우선순위: 인수 → EXE가 있는 게임 폴더 → 지난번 폴더 → Steam 라이브러리 → 직접 선택.
        string game = args.Length > 0 && Folders.IsGame(args[0]) ? args[0]
                    : Folders.IsGame(here) ? here : Folders.Remembered() ?? Folders.FromSteam();
        if (game == null) {
            MessageBox.Show("게임 폴더를 자동으로 찾지 못했습니다. 다음 창에서 SAN14CE_KO.exe를 선택해 주세요.", "AI 모드 설치 관리자");
            game = Folders.Pick(null, null);
            if (game == null) return;
        }
        if (package == null && !File.Exists(Path.Combine(game, @"AI_TacticsMod\관리.ps1"))) {
            MessageBox.Show("AI_TacticsMod 폴더를 찾지 못했습니다. 배포 ZIP 전체를 푼 폴더에서 실행해 주세요.", "AI 모드 설치 관리자");
            return;
        }
        Folders.Remember(game);
        MainForm.AutoCheck = true;
        Application.Run(new MainForm(package, game) { UpdatedTo = updated });
    }
}
}
