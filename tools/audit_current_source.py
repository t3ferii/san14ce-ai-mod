"""Local source inventory for documentation; never execute game code.

Definitions are lexical candidates, not a compiler/LSP call graph. The CSV retains
conditional/fixture/legacy definitions so a missing compiler database cannot hide
them. Behavioral claims belong to the reviewed feature documents.
"""
from pathlib import Path
import csv,hashlib,json,re

P=Path(__file__).resolve().parents[1]
TOPICS={
 'native_campaign':'지역 점령·집결·원군',
 'native_sortie':'출진 명령·편성·대열·역할·이동속도',
 'native_district_intercept':'플레이어 위임 군단 방침별 출진·수송 출발지',
 'native_merit':'무공 병력·논공 친애·열전·보조 저장',
 'native_awards':'명품 6슬롯·창고·수여·관직·보조 저장',
 'native_recruit':'등용 화면·알림·컴퓨터 등용 제한',
 'native_personnel':'인물 관계·건강 검사·자동등용 실행',
 'native_injury':'부상 임무 제한·담당관 해제·본초학',
 'native_domestic':'균형 개발·담당관 임명·명령서 비용',
 'native_population':'원작 월간 양의 인구 증가분 4배',
 'native_region_officer':'담당관·관련개성 표시·분산 생성',
 'native_expert':'숙련 설정 UI·입력·저장 값·계수 연결',
 'native_adjacency':'공방·인접 지역 배율',
 'native_modpack':'능력200·성장·툴팁·콜라보·전사·명승지',
 'native_traits9':'개성9칸·포획 대사 연결',
 'native_speed_buttons':'×4/×8·호버·진행 프레임',
 'native_occupy_fast':'자동점령 카메라·효과 대기',
 'native_save_codec':'저장 압축 연결·검증·원작 대체',
 'native_progress':'거리 계산·AI 진입 가속',
 'native_name_lookup':'이름 탐색 가속',
 'native_layout_wait':'화면자료 유지·선택 진단',
 'native_obstacles':'피해 관찰·중립 방벽 자격',
 'native_facilities':'시설 건설·파괴·병참 임무 복원',
 'native_unified':'시험 통합 인사·검색·일괄 작업',
 'native_officer':'시험 무장 목록·검색·스냅샷',
 'native_tactic_priority':'시험 플레이어 출진 전법 우선순위',
 'native_split_install':'DLL별 연결 소유권·검사·게시·복구',
 'native_adapter':'설정·원작 자료 접근·전투 우선순위·스레드·연결 공통부',
 'module_loader':'설정별 DLL 로드·ABI·입력 전달',
 'input_proxy':'원래 입력 DLL 전달',
}
def topic(name):
    return next((v for k,v in TOPICS.items() if name.startswith(k)),
                '정책·자료구조·저장 형식·공통 보조 함수')
def status(path):
    if 'combat_v2' in path.parts:return '미연결 전투 재구축 후보'
    if path.name.startswith(('native_friendship','native_target_retreat','graphics_proxy')):
        return '이전 설계/독립 후보; 최신 split 등록 대상 아님'
    if path.name.startswith(('native_ai_trace','gpu_profile','native_cost_profile','native_crash_diagnostic')):
        return '진단/조건부 컴파일; 일반 기능과 구분'
    if path.name.startswith(('native_unified','native_officer','native_tactic_priority')):
        return '시험 기능; 설치기 기본 꺼짐'
    return '현재 소스 또는 그 공통 정책/보조부; 연결은 매크로·등록표로 별도 판단'
def masked(text):
    return re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|L?"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda m:''.join('\n' if c=='\n' else ' ' for c in m[0]),text)
def main():
    files=[];rows=[]
    for path in sorted((P/'src').rglob('*')):
        if not path.is_file():continue
        data=path.read_bytes();text=data.decode('utf-8-sig',errors='replace');clean=masked(text)
        functions=[]
        # Line-anchored declarations with a body; no call-expression inventory.
        pattern=r'(?m)^[ \t]*(?:\[\[[^\n]*\]\][ \t]*)?(?:(?:static|inline|constexpr|virtual|extern|__declspec\([^)]*\))[ \t]+)*(?:[\w:<>,*&]+[ \t]+)+(?:__\w+[ \t]+)?([\w:~]+)\s*\([^;{}]*\)\s*(?:const\s*)?(?:noexcept\s*)?\{'
        for m in re.finditer(pattern,clean):
            name=m[1]
            if name in ('if','for','while','switch','return','catch'):continue
            start=m.start();line=clean.count('\n',0,start)+1
            opening=m.end()-1;depth=1;end=opening+1
            while end<len(clean) and depth:
                depth+=(clean[end]=='{')-(clean[end]=='}');end+=1
            body=text[start:end]
            row={'file':path.relative_to(P).as_posix(),'line':line,'function':name,
                 'topic':topic(path.stem),'scope':status(path),
                 'body_sha256':hashlib.sha256(body.encode()).hexdigest()}
            rows.append(row);functions.append({'name':name,'line':line})
        files.append({'file':path.relative_to(P).as_posix(),'sha256':hashlib.sha256(data).hexdigest(),
                      'scope':status(path),'topic':topic(path.stem),'definitions':functions})
    out=P/'development';out.mkdir(exist_ok=True)
    with (out/'FUNCTION_INDEX.csv').open('w',encoding='utf-8-sig',newline='') as f:
        w=csv.DictWriter(f,fieldnames=['file','line','function','topic','scope','body_sha256']);w.writeheader();w.writerows(rows)
    (out/'source_audit.json').write_text(json.dumps({'basis':'2026-10-26 public development source snapshot; no installed/release identity claim',
        'method':'lexical definition candidates; not preprocessed; not full semantic review',
        'files':files},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    lines=['# 현재 소스 조사 범위와 함수 색인','',
      '기준: 2026-10-26 공개 개발 소스. 이 실행은 정의 위치와 지문만 자동 추출하며 설치 버전·정식 릴리스 일치·원작 ABI·실게임 동작을 검증하지 않는다.',
      '일반 설명은 [README](../README.md), 원작 연결 함수와 변경 조건은 [기술 설명](../docs/GITHUB_기능상세.md)에 있다.','',
      '## 근거와 한계','',
      '- `FUNCTION_INDEX.csv`: 파일·행·함수 정의 후보·기능 주제·범위·본문 지문. `source_audit.json`: 모든 src 파일 지문과 정의 후보.',
      '- 정의 후보는 정규식과 괄호 대응으로 수집했다. 조건부 컴파일과 fixture 분기를 포함하며 템플릿·연산자·생성자 등을 완전히 파싱하는 컴파일러 색인이 아니다.',
      '- 핵심 실행 함수와 정책 조건을 수동 대조한 설명과, 위치를 자동 추출한 보조함수 색인을 구분한다. 모든 보조함수의 의미·ABI·도달성을 개별 검증 완료했다는 뜻은 아니다.',
      '- 실제 compile_commands.json이 없어 C++ LSP의 교차 참조 완전성을 주장하지 않는다. 원작 전체 EXE를 새로 역분석하거나 게임을 실행한 검사가 아니다.',
      '- 툴팁·전사 코드는 지문이 고정된 참고 DLL의 COFF 변환물을 정적으로 포함한다. `tools/import_modpack_objects.py`와 `docs/SOURCE_DEPENDENCIES.md`가 근거이며 모든 내부 기계어 함수를 이번에 재분석한 것은 아니다.','',
      '## 기존 프로젝트 문서의 경로 설명 (이번 공개 작업에서 재검증하지 않음)','',
      '| 경로 | 확인한 판단·변경 |','|---|---|',
      '| first_army_node / cut_node / city_node / retreat_node | 개별 우선순위·병참·원작 군세 재판정·10일 회군 억제·소규모 임무 유지 |',
      '| sortie_plan* / sortie_dispatch / sortie_departure_from / sortie_movement / sortie_step | 편성·예산·공성8000·지원1000·최저 중립지형 속도·대열과 지역 작업 |',
      '| campaign_proactive_departures / campaign_request_reinforcements / campaign_request_companion | 10일 출진·4000 유보·지역 일꾼·원군·동행 |',
      '| campaign_individual_priority / campaign_forced_siege_priority / campaign_morale_priority | 40일 정지 귀환·실제 성 피해와30일 집단 진군 잠금·사기 회복 |',
      '| district_intercept::judge / restricted_corps / transport_safety | 방침+17(4내정/5지원)·행동별 제한·직할 제외·거점15칸 수송 예외 |',
      '| balanced_development / synchronize_domestic / domestic_command_cost / population_growth | 균형 개발·1군단 옵션·자동 임명 비용·양의 월증가4배 |',
      '| personnel_recruitment_reason / recruit_reason_label / personnel_auto_employ / recruit_ai_* | 관계·충성·금지·가족 예외·사자·명령서·메뉴 알림 |',
      '| injury_* / personnel_healthy_command | 부상 임무·담당관·컴퓨터 시정 본초학 |',
      '| merit_effects / merit_batch / merit_has / merit_list / merit_profile / merit_normalize | 무공3000/3000/0·논공 상호친애·원작 슬롯 추적·열전 스크롤 |',
      '| award_full_synchronize / award_transfer / award_ai_city / award_post_* | 명품 중복·활동 제한·수여·세력단위 배분·관직 한도 |',
      '| native_split_install / native_modpack / native_traits9 | 기능별 게시·모듈 경계·개성/능력·포획·전사·명승지·콜라보 |','',
      '## 파일별 탐색표','',
      f'현재 src {len(files)}개 파일, 정의 후보 {len(rows)}개. 이 수는 검증 통과 수가 아니다.','',
      '| 파일 | 정의 후보 | 기능 주제 | 조사 범위 |','|---|---:|---|---|']
    lines += [f"| [{x['file']}](../{x['file']}) | {len(x['definitions'])} | {x['topic']} | {x['scope']} |" for x in files]
    (out/'FUNCTION_AUDIT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(json.dumps({'source_files':len(files),'definition_candidates':len(rows)},ensure_ascii=False))
if __name__=='__main__':main()
