"""Create a short, read-only inventory for a new feature chat. No game access."""
from pathlib import Path
from datetime import datetime,timezone,timedelta
import hashlib,json

PROJECT=Path(__file__).resolve().parents[1]
GAME=PROJECT.parent
def read_json(path):
    return json.loads(path.read_text(encoding='utf-8')) if path.exists() else {}
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None

def write_current_split(manifest,state,share,share_check,pair):
    """Summarize installed, Apply and shared versions independently."""
    version=manifest['release_version']
    installed=state.get('release_version') if state.get('installed') else '미설치'
    region=manifest['region_officers']
    proof_path=region.get('release_proof','build/region_officers_release.json')
    proof=read_json(PROJECT/proof_path)
    archive=PROJECT/'dist'/('SAN14_AI_'+version+'_integrated.zip')
    # The installed release has its own proof; share_verification can refer to a newer candidate.
    installed_zip_matches=bool(archive.exists() and proof.get('zip_sha256') and digest(archive)==proof['zip_sha256'])
    share_version=share.get('release_version','없음')
    shared=PROJECT/'dist'/('SAN14_AI_'+share_version+'_integrated.zip')
    share_matches=bool(shared.exists() and share.get('zip_sha256') and
        digest(shared)==share['zip_sha256']==share_check.get('zip_sha256'))
    recorded=bool(state.get('installed') and installed==version)
    lines=['# 현재 설치·배포 상태','',
        '이 문서는 tools/generate_handoff.py가 파일/기존 검사 기록에서 생성한다. 게임 실행 검증이 아니다.','',
        '| 항목 | 확인 결과 |','|---|---|',
        '| 설치 상태 기록 | '+str(installed)+' |',
        '| Apply 패키지 | '+version+' |',
        '| 설치/Apply/manifest 세 DLL 해시 | '+str(pair and recorded)+' |',
        '| 해당 Apply 버전 ZIP/고유 release proof 해시 | '+str(installed_zip_matches)+' |',
        '| 최신 공유 ZIP | '+share_version+' |',
        '| 최신 ZIP/share_package/share_verification 해시 | '+str(share_matches)+' |',
        '| 원본 EXE/manifest 기준 해시 | '+str(digest(GAME/'SAN14CE_KO.exe') in set([manifest.get('supported_original_exe_sha256'),*manifest.get('supported_original_exe_sha256_all',[])]))+' |','',
        '설치 기록과 Apply 버전이 다르면 준비/승격 중 상태일 수 있다. 해시가 같아도 설치 완료로 추정하지 않는다.','',
        '## 현재 기능과 검사 기록','',
        '- 세 DLL 경계 유지. 설치기 기능 '+str(manifest.get('feature_switches',{}).get('count','미기록'))+'개, 종속 키는 부모와 AND.',
        '- 자동 내정: 2군단~·컴퓨터 기본, 1군단은 domestic_direct(기본0)일 때만. 1군단 임명 비용0·명품 AI 배분 제외.',
        '- 속도 ×4/×8(60fps 기준, 프레임당 최대2회 시작), 자동점령 연출 약1/4. 현재 안정성의 실게임 확인과 구별한다.',
        '- 지역담당관: 75%, 이름 왼쪽, 공석 회색, 줌0~4, 관련개성 이름 끝+4. 표시 전용.',
        '- Apply의 지역담당관 검사 기록: adapter '+str(region.get('adapter_checks'))+'개 / 현재 EXE '+str(region.get('native_ranges'))+'범위 / 원래 슬롯3개.',
        '- 최신 공유 ZIP 검사 기록: '+str(share_check.get('checks'))+'개 ('+str(share_check.get('status'))+'). 이번 생성에서 재실행하지 않음.',
        '- 사용자 설정 보존(해당 release proof 기록): '+str(proof.get('settings_preserved'))+'. 일반 로그·함수시간 작성자 제외, 논공/명품 보조 저장 유지.','',
        '## 미해결·실게임 확인 범위','',
        '- v12.9 실행 중 종료 사용자 보고. WER/덤프 없음, 원인 미확인. 속도 ×8 두 번째 호출·자동점령 효과 건너뛰기는 격리 후보이며 확정 원인이 아님.',
        '- v12.5 담당관 간격 사용자 정상 확인. 이후 분산 처리의 FPS 효과·클릭 통과·전체 스위치 조합·통합 인사 배치/입력/저장 재로드·장기 안정성 미확인.',
        '- Computer Use 및 Codex Bridge 금지. 게임 실행·입력·화면 관찰은 사용자 직접 수행.','',
        '## 파일과 근거','',
        '- Apply: build/input_package/manifest.json; 설치 기록: backup/input_state.json.',
        '- Release proof: '+proof_path+' ('+str(proof.get('status'))+').',
        '- 해당 Apply 버전 ZIP: '+archive.name+'. 최신 공유 ZIP: '+shared.name+'.',
        '- 설치 백업: '+str(state.get('install_backup'))+'.',
        '- 업데이트 전 백업: '+str(proof.get('stage_backup'))+'.','',
        *['- '+name+': `'+str(digest(GAME/name))+'`' for name in manifest['split_modules']['files']],'',
        '[문서 목차](../docs/README.md) · [시작](../docs/START.md) · [기능 스위치](../docs/FEATURE_SWITCHES.md)']
    gui=manifest.get('gui_installer')
    if gui:
        lines.insert(lines.index('## 미해결·실게임 확인 범위')-1,
            '- GUI 설치기 '+gui['file']+' / manifest 해시 일치: '+str(digest(GAME/gui['file'])==gui.get('sha256'))+'.')
    if 'SAN14_전법설명.txt' in manifest.get('optional_assets',[]):
        lines.insert(lines.index('## 미해결·실게임 확인 범위')-1,
            '- 선택 참고문 SAN14_전법설명.txt는 기존/수정 파일을 보존하며 설치·업데이트·복구 차단에서 제외.')
    candidate=read_json(PROJECT/'build/development/district_intercept/verification.json')
    if manifest.get('district_intercept_block'):
        lines.insert(lines.index('## 미해결·실게임 확인 범위')-1,
            '- 지원·내정 군단 출진 제한: 게임성 탭, 기본ON, 편의 묶음 필요(전투 DLL 불필요). 지원=수송만, 내정=군사 출진 금지. 해당 군단 거점의 적 근접 수송 출발지 제외 조건도 보완. 컴퓨터·직할·기존 야전부대 제외. 미서명 배포, 전체 경로 실게임 미확인.')
    elif candidate:
        current=all(digest(PROJECT/name)==value for name,value in candidate.get('sources',{}).items())
        lines.insert(lines.index('## 미해결·실게임 확인 범위')-1,
            '- 미설치 개발 후보: 지원·내정 군단 신규 요격 출진 차단(게임성 탭). 개발 메뉴44개; 후보 소스 일치: '+str(current)+
            '. 검사·분리 빌드 완료, 미서명 개발 후보. 배포 패키지 준비 전. [근거](../research/ai_trace/지원내정_요격출진차단_구현.md).')
    preview=read_json(PROJECT/'build/development/damage_preview/verification.json')
    preview_proof=manifest.get('damage_preview',{}).get('release_proof','build/damage_preview_installation.json')
    preview_install=read_json(PROJECT/preview_proof)
    # A later shared install may rebuild either DLL. Use its current owner/source
    # provenance instead of requiring every historical feature DLL to survive.
    def current_owner_proof(feature, historical):
        owner=manifest.get(feature,{}).get('owner','SAN14_Convenience.dll')
        source={'damage_preview':'src/native_damage_preview.inl',
                'building_destruction_all':'src/native_building_destruction.inl'}[feature]
        if (manifest.get(feature) and recorded and pair and proof.get('installed')
                and proof.get('files',{}).get(owner)==digest(GAME/owner)
                and proof.get('sources',{}).get(source)
                and proof['sources'][source]==digest(PROJECT/source)):
            return proof
        return historical
    preview_install=current_owner_proof('damage_preview',preview_install)
    if (manifest.get('damage_preview') and preview_install.get('installed') and
        all(digest(GAME/name)==value for name,value in preview_install.get('files',{}).items())):
        current=all(digest(PROJECT/name)==value for name,value in preview_install.get('sources',{}).items())
        screen=read_json(PROJECT/'data/diagnostics/combat_formula_20261021/user_screens_verification.json')
        observed=('사용자 화면: 조인↔한당·장의·강주 표시 확인, 부대 피해4방향은 정수 절삭 범위와 양립. 실제 피해 적용은 미확인.'
            if screen.get('status')=='screenshot_arithmetic_verified' else '실제 게임 화면/피해 대조 미확인.')
        lines.insert(lines.index('## 미해결·실게임 확인 범위')-1,
            '- 설치 완료: 편의성 「전투력대체 예상 피해량」ON. 칼 대신 1회 병사 피해·구조물 내구 피해 표시. 빌드 소스 일치: '+str(current)+
            '. 전투 DLL·기존 설정값·진단 설정 보존. fixture70개·12범위·분리 빌드/설치기 검사 통과. '+observed+' [설명](../docs/DAMAGE_PREVIEW.md).')
        lines.insert(lines.index('## 미해결·실게임 확인 범위')-1,
            '- 현재 Apply는 로컬 진단 빌드이며 이 로컬 버전 자체의 공유 ZIP은 생성하지 않았다. 별도 공개 빌드는 위 최신 공유 ZIP과 구별한다. 해당 Apply 버전 ZIP 해시 False는 미생성 상태다.')
    elif preview:
        current=bool(preview.get('sources')) and all(digest(PROJECT/name)==value for name,value in preview['sources'].items())
        lines.insert(lines.index('## 미해결·실게임 확인 범위')-1,
            '- 미설치 개발 후보: 편의성 「전투력대체 예상 피해량」(개발 설정45개). 칼 대신 1회 병사 피해·구조물 내구 피해 표시. 후보 소스 일치: '+str(current)+
            '. 피해 표시 fixture70개·원작 지문12범위·세 DLL 분리 빌드·설치기 검사 통과. 실제 게임 화면/피해 대조 미확인. [설명](../docs/DAMAGE_PREVIEW.md).')
    destruction_proof=manifest.get('building_destruction_all',{}).get('release_proof','build/building_destruction_installation.json')
    destruction=read_json(PROJECT/destruction_proof)
    destruction=current_owner_proof('building_destruction_all',destruction)
    if manifest.get('building_destruction_all') and destruction.get('installed') and all(
            digest(GAME/name)==value for name,value in destruction.get('files',{}).items()):
        current=all(digest(PROJECT/name)==value for name,value in destruction.get('sources',{}).items())
        lines.insert(lines.index('## 미해결·실게임 확인 범위')-1,
            '- 설치 완료: 게임성 「건물파괴 지역 전체 피해」ON. 성공 시 해당 지역 적 군사시설·장애물·함정 각각에 원래 피해량 적용. 실패는 원작. 전투 DLL·기존 설정·진단 보존. 소스 일치: '+str(current)+
            '. 기능 fixture'+str(manifest['building_destruction_all'].get('fixture_checks','미기록'))+'개·원작'+str(manifest['building_destruction_all'].get('native_ranges','미기록'))+'범위·분리 빌드·설치기 검사 통과. 군주 Person 번호를 세력 번호로 읽던 오류 교정, 수정 후 실게임 미확인. [설명](../docs/BUILDING_DESTRUCTION.md).')
    presets=read_json(PROJECT/'build/development/expert_presets/verification.json')
    if presets:
        current=bool(presets.get('sources')) and all(digest(PROJECT/n)==h for n,h in presets['sources'].items())
        installed_presets=bool(recorded and pair and proof.get('installed') and current and
            proof.get('files',{}).get('SAN14_Convenience.dll')==digest(GAME/'SAN14_Convenience.dll') and
            all(proof.get('sources',{}).get(n)==h for n,h in presets['sources'].items() if n.startswith('src/')))
        lines.insert(lines.index('## 미해결·실게임 확인 범위')-1,
            '- '+('설치 완료' if installed_presets else '개발 후보')+': 숙련 설정2 두 인접효과의 소·중·대 단일 선택 → 표준 적용. '
            '미선택 시 표준 비활성, 현재 난이도 사용(초급·중급=상급), 39.2/62.5% 소수 저장·계산. '
            '소스 일치: '+str(current)+'. 필수 dll-split -Build 검사 통과, 미서명. 실게임 배치·클릭·저장 재로드 미확인. '
            '[근거](../research/expert_settings/implementation.md).')
    recovery=read_json(PROJECT/'data/diagnostics/expert_preset_crash_20261024/recovery.json')
    if recovery:
        lines.insert(lines.index('## 미해결·실게임 확인 범위')-1,
            '- 숙련 설정 긴급 복구: local3/rc3 진입 충돌 사용자 보고. 원작 숫자 행 로딩 중 AEAF49(+30 읽기), 호출5F13A8. '
            '화면 자료의 인접 헤더값2→0 및 블록 순서 변경 확인. local2로 원복 후 사용자 정상 진입 확인. rc3는 사용자가 GitHub에서 삭제, 재배포 차단.')
        if recovery.get('fixed_candidate_installed') and installed=='v12.21-adjpreset-local4':
            lines.insert(lines.index('## 미해결·실게임 확인 범위')-1,
                '- local4 후보: 헤더 인접 필드와 원작 블록 순서 보존. 두 충돌 회귀 각각 거부 검사·복구 트랜잭션8개·설치기 검사 통과. '
                '현재 새 화면 실게임 확인: '+str(recovery.get('fixed_screen_verified',False))+'. 추가 배포 보류.')
        if recovery.get('local4_entry_user_confirmed'):
            lines.insert(lines.index('## 미해결·실게임 확인 범위')-1,
                '- local4 실게임: 정상 진입, 소·중·대 배경/글자 미표시. 읽기 전용 PID7664에서51노드·행마다3버튼·등록·선택[2,2] 확인. '
                '입력 영역 표준849..961/선택969..1113 비중첩. local5 생성 시점 이동 후에도 사용자 미표시 확인. '
                'local6는 표준과 같은 스킨3을 쓰며 선택 객체/좌표 유지. 후보 설치: '+str(recovery.get('next_candidate_installed',False))+
                ', 표시 개선 미확인. 추가 배포 보류. [시도·성과·한계](../docs/HISTORY.md).')
    briefing=read_json(PROJECT/'development/advisor_briefing_candidate.json')
    if briefing:
        source_match=all(digest(PROJECT/rel)==value for rel,value in briefing.get('sources',{}).items())
        install_note='후보 설치됨' if digest(GAME/'SAN14_Convenience.dll')==briefing.get('diagnostic_sha256') else '현재 설치 미적용'
        from install_local import settings as current_settings
        cfg=current_settings()
        briefing_on=cfg.get('game_flow.advisor_briefing','0')!='0' and cfg.get('groups.convenience','1')!='0'
        live_note='표시 상태 읽기 확인, 턴 진행은 별도 확인 필요' if briefing.get('runtime_probe_verified') else '수정판 실게임 확인 대기'
        if briefing.get('runtime_observation',{}).get('diplomatic_events',0):
            observed=briefing['runtime_observation']
            live_note=briefing.get('version','')+' 읽기 전용 관찰: 외교 후속 대사 '+str(observed['diplomatic_events'])+'건/총 '+str(observed['events'])+'건 수집·플레이어 대기 복귀. 시각적 표시·다른 상호작용은 별도 확인 필요'
        if briefing.get('diplomacy_refusal_verified'):
            live_note=briefing.get('version','')+' 보고 ON·외교 금 요구 거절 후 정상 진행 사용자 확인. 소식 '+str(briefing.get('runtime_observation',{}).get('events','?'))+'건 수집 읽기 확인. 다른 상호작용·장기 안정성은 별도 확인 필요'
        if briefing.get('diplomatic_followup'):
            live_note+='; 외교 확인 전용 후속 대사 수집·명조/먹색 UI 적용'
        if briefing.get('paper_report_theme'):
            live_note+='; 창1160×1000·기존 글자 크기 유지·종이색 본문·정렬 개선·플레이어 프레임 입력·군사 보고 문체 적용'
        if briefing.get('click_visibility_repaired'):
            live_note+='; 명령 메뉴 자체 숨김과 버블 표시 분리·등용 결과 대화 수집/원작 자동 이어가기 추가'
        if briefing.get('updating_state_guard_repaired'):
            live_note+='; 갱신 중인 상태(manager+48)를 모달로 오인하던 입력 차단 제거·실제 맨 위 화면으로 판단'
        if briefing.get('runtime_observation',{}).get('input_dispatches',0):
            click=briefing['runtime_observation']
            live_note+='; 실게임 입력 '+str(click['input_dispatches'])+'회·보고함 열림 읽기 확인(입력 큐 최대 '+str(click.get('input_max_delay_ms','?'))+'ms, 전체 표시 지연 아님)'
        if briefing.get('readable_income_table'):
            live_note+='; 본문 굵기/대비·무장6색·금/군량 고정 열·군사 결과 요약·전령형 진행 통지·보고 전환 마스크 적용'
        if briefing.get('top_right_navigation'):
            live_note+='; 우측 상단 이전/다음·페이지 복사 축소·입력부터 그리기 제출까지 진단 추가'
        entry='- 개발 후보: 군사의 열흘 보고. 좌하단 군사 알림함·10일치 집계·열람/삭제·다음 진행 초기화. 설정 '+('ON' if briefing_on else 'OFF')+', '+install_note+', 미서명. 후보 소스 일치: '+str(source_match)+'. 동작/가드 fixture '+str(briefing.get('behavior_checks'))+'개, WARP 그래픽 '+str(briefing.get('graphics_checks'))+'개, 원작 정적 '+str(briefing.get('native_checks'))+'개. '+live_note+'. [설명](../docs/ADVISOR_BRIEFING.md).'
        at=lines.index('## 미해결·실게임 확인 범위')-1
        lines[at:at]=[entry,'']
    (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(json.dumps({'installed_version':installed,'apply_version':version,'share_version':share_version,
        'installed_files_match':bool(pair and recorded),'release_zip_matches':installed_zip_matches,
        'latest_zip_matches':share_matches}))

def main():
    """Current release layout only (split modules + region_officers manifest). The per-version branches of v7..v12 handoffs were dead
    after the split manifests and were removed with the W7 tool retirement (docs/MODULARIZATION_PLAN.md)."""
    package=PROJECT/'build/input_package'
    state=read_json(PROJECT/'backup/input_state.json')
    manifest=read_json(package/'manifest.json')
    share=read_json(PROJECT/'build/share_package.json')
    share_check=read_json(PROJECT/'build/share_verification.json')
    expected=state.get('installed_sha256') if state.get('installed') else None
    actual=digest(GAME/'dinput8.dll');payload=digest(package/'dinput8.dll')
    matches=bool(expected and expected==actual==payload==manifest.get('dll_sha256'))
    split=manifest.get('split_modules')
    if not split or not manifest.get('region_officers'):
        raise RuntimeError('manifest has no split_modules/region_officers: not a current release package')
    pair=matches and all(digest(GAME/name)==digest(package/name)==expected for name,expected in split['files'].items())
    write_current_split(manifest,state,share,share_check,pair)

if __name__=='__main__':main()
