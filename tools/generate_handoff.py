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
        '| 원본 EXE/manifest 기준 해시 | '+str(digest(GAME/'SAN14CE_KO.exe')==manifest.get('supported_original_exe_sha256'))+' |','',
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
                ', 표시 개선 미확인. 추가 배포 보류. [시도·성과·한계](../docs/EXPERT_ADJACENCY_UI_ATTEMPTS.md).')
    briefing=read_json(PROJECT/'development/advisor_briefing_candidate.json')
    if briefing:
        source_match=all(digest(PROJECT/rel)==value for rel,value in briefing.get('sources',{}).items())
        install_note='후보 설치됨' if digest(GAME/'SAN14_Convenience.dll')==briefing.get('diagnostic_sha256') else '현재 설치 미적용'
        from install_local import settings as current_settings
        cfg=current_settings()
        briefing_on=cfg.get('game_flow.advisor_briefing','0')!='0' and cfg.get('groups.convenience','1')!='0'
        live_note='표시 상태 읽기 확인, 턴 진행은 별도 확인 필요' if briefing.get('runtime_probe_verified') else '수정판 실게임 확인 대기'
        if briefing.get('diplomacy_refusal_verified'):
            live_note=briefing.get('version','')+' 보고 ON·외교 금 요구 거절 후 정상 진행 사용자 확인. 소식 '+str(briefing.get('runtime_observation',{}).get('events','?'))+'건 수집 읽기 확인. 다른 상호작용·장기 안정성은 별도 확인 필요'
        entry='- 개발 후보: 군사의 열흘 보고. 좌하단 군사 알림함·10일치 집계·열람/삭제·다음 진행 초기화. 설정 '+('ON' if briefing_on else 'OFF')+', '+install_note+', 미서명. 후보 소스 일치: '+str(source_match)+'. 동작/가드 fixture '+str(briefing.get('behavior_checks'))+'개, WARP 그래픽 '+str(briefing.get('graphics_checks'))+'개, 원작 정적 '+str(briefing.get('native_checks'))+'개. '+live_note+'. [설명](../docs/ADVISOR_BRIEFING.md).'
        at=lines.index('## 미해결·실게임 확인 범위')-1
        lines[at:at]=[entry,'']
    (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(json.dumps({'installed_version':installed,'apply_version':version,'share_version':share_version,
        'installed_files_match':bool(pair and recorded),'release_zip_matches':installed_zip_matches,
        'latest_zip_matches':share_matches}))

def main():
    package=PROJECT/'build/input_package'
    state=read_json(PROJECT/'backup/input_state.json')
    manifest=read_json(package/'manifest.json')
    install=read_json(PROJECT/'build/recruitment_ui_installation.json')
    share=read_json(PROJECT/'build/share_package.json')
    share_check=read_json(PROJECT/'build/share_verification.json')
    registry=read_json(PROJECT/'development/features.json')
    injury=bool(manifest.get('injury_protection'))
    awards=bool(manifest.get('award_warehouse'))
    office_fix=bool(manifest.get('office_picker_fix'))
    sortie=bool(manifest.get('cohesive_sortie'))
    expected=state.get('installed_sha256') if state.get('installed') else None
    actual=digest(GAME/'dinput8.dll');payload=digest(package/'dinput8.dll')
    matches=bool(expected and expected==actual==payload==manifest.get('dll_sha256'))
    if manifest.get('split_modules'):
        split=manifest['split_modules'];combat=digest(GAME/'SAN14_CombatAI.dll')
        pair=matches and all(digest(GAME/name)==digest(package/name)==expected for name,expected in split['files'].items())
        if manifest.get('region_officers'):
            write_current_split(manifest,state,share,share_check,pair)
            return
        if manifest.get('v12'):
            archive=PROJECT/'dist'/('SAN14_AI_'+manifest['release_version']+'_integrated.zip')
            zip_matches=bool(archive.exists() and digest(archive)==share_check.get('zip_sha256')==share.get('zip_sha256'))
            lines=['# '+manifest['release_version']+' 숙련 설정 설명 정리','',
                '설치/Apply/세 DLL 해시 일치: '+str(pair and state.get('release_version')==manifest['release_version'])+'.',
                '배포 ZIP 검증 일치: '+str(zip_matches)+'.',
                'v11.32 기능을 유지하고 숙련 모드 설정2의 자세력/타세력 기본 공방·인접 버프 설명만 수정.',
                '네 툴팁은 각각 3줄: 기본 공방에 계산식·설정 단위, 인접효과에 자세력/타세력 원작 기본값(표준/중/대). 스크롤 없음.',
                '계산·입력·저장·훅 소유권 변경 없음. 편의 DLL이 해당 UI를 단독 소유.',
                '검사: expert-combat(native, split-module), 세 DLL release build, ZIP apply/restore fixture.',
                '실제 게임 화면의 긴 툴팁 배치·입력·저장 재로드는 미확인.',
                '기능설정.ini 유지. 기존 combat=0·unified=0 등 사용자 설정 보존.',
                '배포: '+str(archive), '설치 백업: '+str(state.get('install_backup')), '',
                *[n+': '+str(digest(GAME/n)) for n in split['files']], '',
                '근거: build/v12_work/baseline.json, expert-check.log, split-build.log, release-proof.json.',
                '준비: tools/release_v11_31.py prepare|stage (v12); build_share.py; guarded 관리.ps1 Apply.',
                '이전 상세 기록과 잔여 기능 과제는 docs/START.md 및 기능별 문서 참조.']
            speed=read_json(PROJECT/'build/installer_speed/cmd-timings.json')
            region_candidate=read_json(PROJECT/'development/region_officers_candidate.json')
            if region_candidate:
                lines+=['', '개발 후보: 월드맵 지역담당관 초상화 상시 표시(75%·공석 황색), 임명 기능 없음.',
                    '후보 미설치. 실제 배치·클릭 통과·FPS 비교 미확인: 사용자가 화면 제어를 중지함.',
                    '검사/후보 경로: docs/REGION_OFFICERS.md 및 development/region_officers_candidate.json.',
                    '사용자 지시(2026-10-19): 이 프로젝트에서 Codex Bridge 사용 금지.']
            if speed.get('status')=='installer_speed_verified':
                lines[0]='# '+manifest['release_version']+' 숙련 설정 3줄·설치기 개선'
                lines+=['', '설치기: KTID 위치 색인·직접 바이트 배열 생성으로 반복 검색/할당 비용 제거. 백업·해시·다른 모드 보존 유지.',
                    '별도 복사본 실제 CMD: 설치 16.85→5.02초, 재적용 34.15→3.88초, 복구 22.89→3.04초 (1회 비교).',
                    '근거: docs/INSTALLER_SPEED.md, build/installer_speed/cmd-timings.json 및 final-proof.json.']
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
            print(json.dumps({'handoff':str(PROJECT/'development/CURRENT.md'),'installed_files_match':pair,'zip_matches':zip_matches}))
            return
        crash=read_json(PROJECT/'data/diagnostics/v11_10_crash_rollback.json')
        field_crash=read_json(PROJECT/'data/diagnostics/v11_11_crash_rollback.json')
        if manifest.get('modpack_v16'):
            report=read_json(PROJECT/(manifest['unified_personnel_revision'].get('installation_report','build/unified_personnel_installation.json') if manifest.get('unified_personnel_revision') else 'build/traits9_installation.json' if manifest.get('traits9_revision') else 'build/modpack_installation.json'))
            archive=PROJECT/'dist'/('SAN14_AI_'+manifest['release_version']+'_integrated.zip')
            archive_matches=bool(archive.exists() and digest(archive)==share_check.get('zip_sha256')==report.get('zip_sha256'))
            assets=all(digest(GAME/n)==digest(package/n)==h for n,h in manifest.get('assets',{}).items())
            lines=['# '+manifest['release_version']+' ModPack v1.6 편의성 통합','',
                '설치/Apply/세 DLL 해시 일치: '+str(pair and state.get('release_version')==manifest['release_version'])+'.',
                '전법 설명 파일 일치: '+str(assets)+'. 배포 ZIP 검증 일치: '+str(archive_matches)+'.',
                '개성·전법 툴팁, 콜라보 개성·전법 해금, 능력치 편집/실효/자연 성장 상한200.',
                '100 이후 자연 성장은 원작99→100 필요 경험치 증가폭 유지. UI 콜백에서 기존 콜라보 데이터 해금.',
                'AI 전사 시 원작 일러스트·대사·게임 기록, 명승지 연출 Skip. 명승지Lv6 전용 연출은 참조 모드처럼 유지.',
                '추가 기능 소유 SAN14_Convenience.dll. 로더/전투/편의 세 DLL 유지. FPS/기타 설정 추가 없음.',
                '툴팁/전사 코드는 지문 확인 후 COFF 정적 연결. 추가 DLL/메모리 PE 로더 없음. 진단 파일 작성 함수 비활성화.',
                'v11.20 숙련 설정·기존 전투·편의 기능 유지. 원본 EXE/사용자 저장 파일 직접 수정 없음.',
                '후보 검사 '+str(manifest['modpack_v16']['checks'])+'개; ZIP 적용·복원 '+str(share_check.get('checks'))+'개.',
                '실제 게임 화면·전사·자연 성장·저장 재로드 실행 확인 전.',
                '배포 '+str(archive)+'; 백업 '+str(report.get('backup'))+'.',
                'tools/release_modpack.py prepare|install; docs/MODPACK_v11_21.md; research/modpack_v16/INTEGRATION.md.','']
            lines += [n+': '+str(digest(GAME/n)) for n in split['files']]
            if manifest.get('traits9_revision'):
                lines[0]='# '+manifest['release_version']+' 9개성·포획대사 추가 통합'
                lines += ['', '9개성 판정·선택과 무장 상세 기본/능력·일람 9칸 표시. 기존 Editor_Source 구현 재사용.',
                    '포획 측/포로 측이 플레이어일 때 원작 포획대사 표시. 확률/포획 판단 변경 없음.',
                    '기존 무장 u16×9 저장칸 사용. 세이브 구조·논공/명품 보조 저장 코드 변경 없음. 실 저장 재로드는 미확인.',
                    '0002/ScreenLayout.rdb 네 항목과 신규 bin31 관리. 원본 인덱스 백업, 해제 시 복구.',
                    'tools/release_traits9.py prepare|install; docs/TRAITS9_v11_22.md; research/traits9/INTEGRATION.md.']
            if manifest.get('unified_personnel_revision'):
                lines[0]='# '+manifest['release_version']+' 게임 내부 통합 인사'
                lines += ['', '진행 오른쪽·명령서 아래 통합 인사: 12조건 검색, 7개 일괄 작업, 무력형/지력형 출진 전법 우선순위.',
                    '통합 인사와 전법 자동 선택 모두 SAN14_Convenience.dll 소유. 로더/전투 실행 코드(.text)는 v11.22와 동일.',
                    '통합 인사 정책28·설정/전법13·UI 어댑터'+str(manifest['unified_personnel_revision']['candidate_verification']['focused_checks']['engine']['checks'])+'·소스/바이너리 경계'+str(manifest['unified_personnel_revision']['candidate_verification']['checks'])+' 검사 통과. 새 배치·실제 비용·저장 재로드 미확인.',
                    'tools/release_unified_personnel.py prepare|install; docs/UNIFIED_PERSONNEL.md; '+manifest['unified_personnel_revision'].get('installation_report','build/unified_personnel_installation.json')+'.']
                if manifest['unified_personnel_revision'].get('hud_visibility_fix'):
                    lines += ['v11.23 버튼 미표시 수정: 팝업 관리자와 전략 상태 혼동 제거. 실행 중 게임에서 기존 훅 연결·빈 팝업 상태 확인, 회귀 검사로 실패 재현 후 수정. 실제 수정 화면은 미확인.']
                if manifest['unified_personnel_revision'].get('hud_registration_fix'):
                    lines += ['v11.24 버튼 생성 후 미표시 수정: 원작 추가 컨트롤 목록에 HUD 등록, 부모 leave에서 원작 소유권으로 분리·삭제. 기존 화면 메타데이터 수정 없음.']
                if manifest['unified_personnel_revision'].get('hud_overlay_entry'):
                    lines += ['진입 버튼만 무술대회 방식의 작은 D3D11 텍스처로 표시. 검색·선택·작업·입력은 기존 게임 목록창 재사용.',
                        '실제 D3D11 픽셀/클릭/좌표 검사23 통과. 전체 화면 복사·매 프레임 버튼 이미지 재생성 없음. 게임 함수 호출은 UI 스레드.',
                        'REA 사용 허용(2026-10-18 금지 해제). 현재는 한 단계씩 UI 배치부터 구현; 최적화·함수별 로그는 후속 작업.']
                    observed=read_json(PROJECT/'data/diagnostics/unified_ui_validation.json')
                    if observed.get('version')==manifest['release_version'] and observed.get('files')==split['files']:
                        lines += ['실제 게임 화면 관찰: 버튼 표시='+str(observed.get('entry_button_visible'))+', 원작 통합 목록창 표시='+str(observed.get('native_personnel_dialog_visible'))+'.',
                            '자동 클릭은 사용자 입력 감지로 실행되지 않음. 검색 조건 메뉴·일괄 작업 비용·저장 재로드는 미확인.']
                if manifest['unified_personnel_revision'].get('two_pane_layout'):
                    if manifest['unified_personnel_revision']['two_pane_layout'].get('native_list_panes'):
                        lines += ['v11.30: 전체 화면 호스트 하나 안에 원작 CDataList 두 개. 왼쪽(64,200,1200,690), 오른쪽(1344,200,496,690). 별도 전체 화면 목록창/배경 생성 제거.',
                            '왼쪽 소속열9개·전법/정책/개성 너비 조절, 모든12탭 너비≤1200 검증. 각 패널 스크롤바 독립. 네이티브 클래스·크기 함수 실제 EXE 대조30/수명·입력92검사.',
                            '실제 좌우 배치·게임 입력 확인은 진단 기록 참조. 우측 드롭다운과 결과만 갱신하는 개선은 별도 후속.']
                    else:
                        lines += ['좌측 무장 일람과 우측 검색 조건은 둘 다 원작 목록창. 원작 자동 너비 설정+자식 레이아웃 재사용; 추가 모달 상태/채널/외부 창 없음.',
                        '오른쪽 조건/현재값을 선택한 후 하단 선택 조건 변경. 조건 팝업 중 우측 갱신 중단; 부모 파괴 전에 자식 분리·삭제.',
                        'v11.26에서 진입 버튼과 원작 목록은 실제 관찰됨. 새 좌우 배치/우측 입력은 게임에서 확인 필요.']
                if manifest['unified_personnel_revision'].get('native_slot_fix'):
                    lines += ['v11.27 버튼 소멸 원인: generic dialog frame 슬롯을 +38로 잘못 지정해 편의 DLL 전체 등록 거부. 실제 EXE 대조 검사로 회귀 재현.',
                        'v11.28: frame +10/13AC588→2FB440 수정, controls +38/13AC5B0→258170 별도 연결. 실제 EXE의 모든 split pointer guard 대조 포함.']
                if manifest['unified_personnel_revision'].get('native_sortie_relations'):
                    lines += ['v11.29: 출진 원작7D6AC0/30BED0 연계 판정·기호·정렬 재사용. 실제 무장일람에서 기준 선택 → 원작 등급 선택. 연계 탭에 부친·모친·배우자·의형제 열 포함.',
                        '비교 결과는 기준 무장별 캐시, 일괄 작업 후 폐기. 어댑터16/현재 EXE 호출 경로14검사 통과. 드롭다운 상시 패널·목록만 갱신하는 조작 개선은 아직 남음. 실게임 화면 확인 전.']
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
            print(json.dumps({'handoff':str(PROJECT/'development/CURRENT.md'),'installed_files_match':pair,'assets_match':assets,'archive_matches':archive_matches}));return
        if manifest.get('function_cost_profile'):
            session=read_json(PROJECT/'data/performance/cost_profile_session.json')
            lines=['# '+manifest['release_version']+' 로컬 함수시간 계측','',
                '설치/Apply/세 DLL 해시 일치: '+str(pair and state.get('release_version')==manifest['release_version'])+'.',
                'v11.12 충돌 수정 및 기존 기능 유지. 추행진(7) 공성 군세·단독 여성 지원 대상 제외 수정 포함.',
                'RUNTIME_COST_PROFILE + NO_RUNTIME_LOG: 일반 .log/이벤트 기록 제외, 함수 '+str(manifest['function_cost_profile']['functions'])+'곳의 호출 횟수·포함/자체/최대 시간만 RAM 집계 후 약 1초 간격 CSV 기록.',
                '로컬 진단 전용; 이 설치 패키지의 build_share.py 배포 차단. 새 무로그 v11.13은 별도 build/warehouse_fix_package에서 준비.',
                '설치 전 원본 v11.12 백업: '+str(session.get('baseline_backup')),
                'tools/manage_cost_profile.py prepare|install|restore; 복구/교체 시 게임 종료 필수.',
                '로그 data/performance/function_cost_{Combat|Convenience}_PID.csv; tools/analyze_function_costs.py --pid PID.',
                '실측 재현(사용자 1턴 완료 확인): 명품창고 동기화 1,141,413회/포함 시간 10.37초, 군주 조회 54,524,070회. 실행·불러오기 포함, 정확한 진행 경계 미상.',
                'warehouse_sync_revision 포함 시: 상태가 같은 조회는 전체 동기화·정렬 생략, 변경 즉시 반영. 소유자/세력별 현재 조회 중복 제거. 자체 시간에 계측 안 된 원작 호출 포함.',
                'effect_queries_use_native_owner_iterator=True이면 효과 조회는 해당 무장 명품만 검사; 빈 명품 목록 군주 조회 없음. 기존 저장/수여/AI 전체 동기화 경계 유지.',
                '현재 native_* 시간 분리 및 원작 AI root/force/district/army 계측 훅 포함. 원작 호출 내부 모드 콜백이 있으므로 바닐라 단독 시간과 같지 않음.',
                '사용자 PID 57480 같은 저장 한 턴 완료, 부드러워졌고 렉이 사라진 느낌 확인. 조회 평균 9.461→0.631µs(약93.3% 감소), 군주 조회 54,524,070→94,063회. 전체 게임 속도 비율 아님.',
                '근거 data/performance/warehouse_owner_cache_57480 및 warehouse_user_validation.json. 수정 전 warehouse_before_40964. 원작 AI root/세력/군단/부대 행은 미관측; 원작 AI 전체 시간 계측 완료로 보지 않음.',
                '1차 전체 입력 캐시(PID 38380)는 체감 여전히 느림; 무장별 후보(PID 57480)는 개선 확인. 보고 화면 5초 지연 별도 검증 및 장기 안정성 미확인. 지역담당관 작업 보류.',
                '사용자 세이브·원본 EXE 변경 없음. 로컬 진단판은 현재 CSV 작성 활성 상태; 게임 종료 후 v11.13 install로 로그 없는 판 교체 가능.','']
            prepared=read_json(PROJECT/'build/warehouse_fix_preparation.json')
            if prepared.get('release_version')=='v11.13':
                lines+=['v11.13 무로그 준비 검사 '+str(prepared['checks'])+'개. ZIP 적용/복구 '+str(share_check.get('checks'))+'개.',
                    '배포 dist/SAN14_AI_v11.13_integrated.zip; 일반 .log/이벤트/예외/함수시간 CSV 작성자 모두 컴파일 제외.',
                    'tools/release_warehouse_fix.py prepare|install; 설치 시 종료 확인/백업/해시 검증. 현재 설치는 위 로컬 진단판.','']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8');return
        if manifest.get('expert_combat_settings_revision'):
            report=read_json(PROJECT/'build/expert_settings_installation.json')
            revision=manifest['expert_combat_settings_revision']
            archive=PROJECT/'dist'/('SAN14_AI_'+manifest['release_version']+'_integrated.zip')
            archive_matches=bool(archive.exists() and digest(archive)==share_check.get('zip_sha256')==report.get('zip_sha256'))
            lines=['# '+manifest['release_version']+' 숙련 모드 기본 공방·직접 인접효과','',
                '설치/Apply/상태/세 DLL 해시 일치: '+str(pair and state.get('release_version')==manifest['release_version'])+'.',
                '배포 ZIP 검증 해시 일치: '+str(archive_matches)+'.',
                '시나리오→군주→숙련 모드 진입 후 설정2, 포위점령 다음에 자세력/타세력 기본 공방50~200(표준100) 두 줄 추가.',
                '두 인접지역 효과0~200 직접 퍼센트(8=8%, 표준4). 원작 부대 공방·정보·예측에 배율 반영, 컴퓨터끼리 실제 전투4% 유지.',
                ('v11.20 툴팁은 항목별 직접 퍼센트/기본 배율 설명으로 교체. 숙련 설정·툴팁·계수 훅은 SAN14_Convenience.dll 전용.'
                 if revision.get('distinct_player_and_computer_tooltips') else '숙련 설정은 SAN14_Convenience.dll 전용.'),
                ('v11.19는 v11.18 표시 미반영과875BC0의 XMM1 float 인자 전달 오류 수정. 기존 원작 부대별 공방 유지,1000 고정 없음.'
                 if revision.get('unit_stat_paths_updated') else 'v11.18은 부대 표시 미반영과 목록 위치 인자 전달 오류 신고가 있는 이전 판.'),
                '원작 옵션 RAM·저장/로드 경로 재사용. 부대 공방 계산 중 파일 IO 없음. 기존 미편집 저장은 원작 계산 유지.',
                '기존 v11.17 동작과 세 DLL 고정 역할 유지. 로컬/배포 로그·함수시간 작성자 컴파일 제외.',
                '후보 검사 '+str(revision['checks'])+'개, ZIP 적용/복구 '+str(share_check.get('checks'))+'개 통과.',
                '실제 새 화면 표시/게임 저장 재로드/전투 수치/장기 안정성은 확인 전. 원본 EXE/사용자 저장 파일 직접 수정 없음.',
                '배포 '+str(archive)+'; 백업 '+str(report.get('backup'))+'.',
                'tools/release_expert_settings.py prepare|install; build/expert_settings_installation.json.',
                ('docs/EXPERT_SETTINGS_v11_19.md' if revision.get('unit_stat_paths_updated') else 'docs/EXPERT_SETTINGS_v11_18.md')+'; research/expert_settings/implementation.md.',
                '지역담당관/보고화면 지연 개별 조사 보류.','']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8');return
        if manifest.get('siege_policy_revision'):
            report=read_json(PROJECT/'build/siege_policy_installation.json')
            lines=['# '+manifest['release_version']+' 30일 정지 집단 진군 · 두 가지 명령 잠금','',
                '설치/Apply/상태/세 DLL 해시 일치: '+str(pair and state.get('release_version')==manifest['release_version'])+'.',
                '명령 잠금: 실제 성/관문 피격 목표 고정과 집결한 군세의 실제30일 정지 집단 진군만. 일반 공성 명령은 교체 가능.',
                '30일 진군은 군세 구성·대열·최종 성을 유지. 이동 작성자만 범위 내에서 잠금 통과; 종료 후 잠금 복구. 원작 목표 덮어쓰기와 실패 경로 명령 검증.',
                '건설3회 제한/횟수 추적과 성 앞 공격 우선 모두 제거. 일반 군세 계속 건설 가능. 일반40일 정지 퇴각 유지.',
                '약한 성 군세 우회 제거, 두 병력 조건, 추행진 제한 제거, 모드 부대 요격 제거, 명품 RAM 최적화 유지.',
                '로컬/배포 모두 로그·이벤트·예외·함수시간 CSV 작성자 컴파일 제외. 원본 EXE/세이브/인접표 변경 없음.',
                '전체 빌드·모의·배포 DLL·ZIP 적용/복구 '+str(share_check.get('checks'))+'개 통과. 실제 한 턴/장기 안정성 확인 전.',
                '배포 dist/SAN14_AI_v11.17_integrated.zip; 백업 '+str(report.get('backup'))+'. tools/release_siege_policy.py prepare|install.',
                'docs/SIEGE_POLICY_v11_17.md; 최신 잠금 범위 AGENTS.md 기록. 도시/지역 원본표 research/city_attack_routes/README.md.',
                '지역담당관/보고화면 지연 개별 조사 보류. 기존 target_retreat_fixture는 보관용 검사.','']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            expert=read_json(PROJECT/'build/expert_settings_candidate.json')
            if expert.get('status')=='verified_candidate':
                lines += ['', '숙련 모드 설정2 후보(미설치): 기본 공방50~200/표준100 두 줄, 인접효과0~200 직접퍼센트/표준4.',
                          '플레이어 포함 실제 부대 전투에만 적용, 컴퓨터끼리 기존4% 유지. 원작 옵션 RAM·저장/로드 재사용; 계산 중 파일IO 없음.',
                          '후보 '+str(expert.get('directory'))+'; 검사 '+str(sum(p['checks'] for p in expert['proofs'].values()))+'개 통과.',
                          '증거 build/expert_settings_candidate.json; research/expert_settings/implementation.md.',
                          '실제 새 UI/게임 저장 재로드/전투 수치 미확인. 설치·Apply·공유 ZIP 미변경.']
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8');return
        if manifest.get('siege_commit_revision'):
            report=read_json(PROJECT/'build/siege_commit_installation.json')
            lines=['# '+manifest['release_version']+' 공성 명령 고정 · 선택 건설보다 공격 우선','',
                '설치/Apply/상태/세 DLL 해시 일치: '+str(pair and state.get('release_version')==manifest['release_version'])+'.',
                '실제 성/관문 피해는 공격한 성만 고정; 적 부대 피격은 목표를 변경하지 않음. 도시4/5·관문5/6 대상 형식 유지.',
                '공성 군세 건설 완료 누적3회, 집결 후 실제 전원 정지20일이면 최종 성 강제공격. 횟수 RAM/출진 초기화, 재불러오기 이전 완료 횟수 보존 sidecar 없음.',
                '기존 성 공격 명령 및 성2칸 이내 선택 건설은 공격 우선. 강제군세는40일 퇴각보다 공격 우선, 일반부대40일 유지. 소모<1000 기존 회복.',
                '상위 AI와 건설/병참단절/이동/경로탐색/사기/철거/이전 작전 복원 명령 쓰기 보호. AGENTS.md에 향후 필수 충돌 검토 기록.',
                '건업70000/오3540은가용66000으로 조건 통과. 약한 성의 군세 우회 두 분기 제거. 20000~39999 목표≤가용+5000,40000이상 조건 유지.',
                '추행진 제한/모드 부대 요격/공성 인접지역선 강제 이동 제거 유지. 명품 RAM 최적화 유지.',
                '로컬/배포 로그·이벤트·예외·함수시간 CSV 작성자 컴파일 제외.',
                '전체 빌드/모의·배포 DLL·ZIP 적용복구 '+str(share_check.get('checks'))+'개 통과. 실제 게임 정지 해소·장기 안정성 확인 전.',
                '배포 dist/SAN14_AI_v11.16_integrated.zip; 백업 '+str(report.get('backup'))+'. tools/release_siege_commit.py prepare|install.',
                '원작 EXE/사용자 세이브/공용시나리오 표 변경 없음. docs/SIEGE_COMMIT_v11_16.md.',
                '도시·관문61/지역343 연결 전수 목록과 주소: research/city_attack_routes/README.md 및 도시별_인접거점.md. 고정 성쌍별 순서표는 미확인.',
                '지역담당관/보고화면 지연 개별 조사 보류. 기존 target_retreat_fixture는 보관용 검사.','']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8');return
        if manifest.get('native_ai_restore_revision'):
            report=read_json(PROJECT/'build/native_ai_restore_installation.json')
            lines=['# '+manifest['release_version']+' 원작 요격 · 추행진 제한 제거 · 40일 정지 퇴각','',
                '설치/Apply/상태/세 DLL 해시 일치: '+str(pair and state.get('release_version')==manifest['release_version'])+'.',
                '모드 출진 성 방어 요격/피해 반격 전환/작전 주변 적 추격 제거. 원작 일일 공격/요격 후보 필터 패치도 설치하지 않음.',
                '추행진 편성/지원 제외·강제 진형 교체·진형별 1000명 상한·자동 작업부대 전환/출진 성 대기 제거.',
                '같은 타일 이동 없이 40일(4턴)이면 퇴각. 교전/피해/건설/군악대 사기 회복 시간 연장 없음. 실제 이동/부대 식별 변경만 초기화.',
                '활동 중인 1000명 이상 작업부대/공성 군세의 소병력 구성원이 원작 소모 퇴각으로 넘어가는 누락 보완.',
                'v11.13 명품 소유자별 RAM 캐시와 공성 인접지역선 강제 이동 제거, 일반 출진 목적지 경로 방지 유지.',
                '로컬/배포 모두 NO_RUNTIME_LOG; RUNTIME_COST_PROFILE 제외. 로그/이벤트/예외/함수시간 CSV 작성자 컴파일 제외.',
                'ZIP 적용/복구 '+str(share_check.get('checks'))+'개. 배포 dist/SAN14_AI_v11.15_integrated.zip.',
                '설치 백업 '+str(report.get('backup'))+'. 상세 docs/NATIVE_AI_RESTORE_v11_15.md.',
                '모의 검사/배포 DLL 검사 통과. 실제 반복 귀환 해소·한 턴/장기 안정성 확인 전. 지역담당관/보고 화면 지연 개별 조사 보류.',
                '기존 target_retreat_fixture는 보관용 검사로 v11.15 활성 기능 아님. 원본 EXE/사용자 세이브 변경 없음.',
                'tools/release_native_ai_restore.py prepare|install.','']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8');return
        if manifest.get('interception_revision'):
            report=read_json(PROJECT/'build/interception_fix_installation.json')
            lines=['# '+manifest['release_version']+' 원작 요격 유지 · 모드 요격 범위 제한 · 20일 정지 퇴각','',
                '설치/Apply/상태/세 DLL 해시 일치: '+str(pair and state.get('release_version')==manifest['release_version'])+'.',
                '원작 요격·원군 판단 유지. 모드 추가 요격은 이미 출진한 적대 성 공격 군세만; 부대/방어 성/적 사이 각각 10칸 이내.',
                '추행진/작업/퇴각/지원 부대 제외. 자동 공성 편성은 적대 목표만. 자동 요격 추행진을 보유 야전 진형으로 교체; 직접 플레이어 명령 유지.',
                '같은 타일 비전투 20일(2턴) 정지 시 퇴각. 이동/부대 재사용 이력 초기화, 실제 피해 및 근처 유효 공격/건설/군악대 회복 보호.',
                'v11.13 명품 소유자별 RAM 캐시와 공성 인접지역선 강제 이동 제거, v11.12 출진 충돌 수정 유지.',
                'NO_RUNTIME_LOG; RUNTIME_COST_PROFILE 제외. .log/이벤트/예외/함수시간 CSV 작성자 모두 컴파일 제외.',
                '검사 '+str(sum(r['checks'] for r in split['fixtures'].values()))+'개; ZIP 적용/복구 '+str(share_check.get('checks'))+'개.',
                '배포 dist/SAN14_AI_'+manifest['release_version']+'_integrated.zip; 설치 백업 '+str(report.get('backup')),
                '상세 docs/INTERCEPTION_FIX_v11_14.md. 실전 재현 해소 및 장기 안정성은 확인 전. 지역담당관 작업/보고 화면 지연 개별 검증 보류.',
                'tools/release_interception_fix.py prepare|install; 원본 EXE/사용자 세이브 변경 없음.','']
            ready=read_json(PROJECT/'build/native_ai_restore_ready.json')
            if ready.get('release_version')=='v11.15':
                lines+=['','v11.15 준비/ZIP 검사 완료, 게임 종료 대기. 아직 위 v11.14 설치 상태.',
                    'v11.15는 모드 요격/추행진 제한 제거, 예외 없는 같은 타일 40일 정지 퇴각. 소병력 작업/군세 조기 귀환 분기 보완.',
                    '준비 build/native_ai_restore_package; 배포 dist/SAN14_AI_v11.15_integrated.zip. tools/release_native_ai_restore.py install.',
                    'docs/NATIVE_AI_RESTORE_v11_15.md; 실제 반복 귀환 해소·게임 안정성 확인 전.']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8');return
        if manifest.get('warehouse_sync_revision'):
            report=read_json(PROJECT/'build/warehouse_fix_installation.json')
            lines=['# '+manifest['release_version']+' 명품 조회 최적화 · 추행진 제외 · 로그 없음','',
                '설치/Apply/상태/세 DLL 해시 일치: '+str(pair and state.get('release_version')==manifest['release_version'])+'.',
                '기존 v11.12 기능/출진 경로 충돌 수정 유지. 명품 조회는 소유자 반복자/정확한 RAM 캐시, 변경 즉시 반영. 빈 목록 군주 조회 없음.',
                '같은 저장 진단판 PID 57480 사용자 한 턴 완료 및 체감 개선. 조회 평균 비용 약93.3% 감소. 불러오기/대기 포함, 전체 게임 속도 개선율 아님.',
                '추행진 공성 군세/관문 선택 결과/복구 및 단독 여무장 응원·악주 대상 제외.',
                '마지막 요청으로 공성 인접지역선 강제 이동 제거. 지역 BFS/중심 경유/인접지역만 경로 제한 삭제. 주변 점령/건설과 원작 경로 유지; 멈춤 해소 실전 확인 전.',
                'NO_RUNTIME_LOG; RUNTIME_COST_PROFILE 제외. .log/이벤트/예외/함수시간 CSV 작성 코드 없음. 보조 저장은 유지.',
                '검사 '+str(sum(r['checks'] for r in split['fixtures'].values()))+'개; ZIP 적용/복구 '+str(share_check.get('checks'))+'개.',
                '배포 dist/SAN14_AI_v11.13_integrated.zip; 설치 백업 '+str(report.get('backup')),
                '직접 무로그 게임 진행/장기 안정성 및 보고화면 5초 지연 별도 확인 미완료. 지역담당관 추가 작업 보류.',
                '상세 docs/WAREHOUSE_FIX_v11_13.md. 근거 data/performance/warehouse_user_validation.json.',
                'tools/release_warehouse_fix.py prepare|install; 원본 EXE/사용자 세이브 변경 없음.','']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8');return
        if manifest.get('dispatch_crash_fix'):
            fix_install=read_json(PROJECT/'build/dispatch_fix_installation.json')
            lines=['# '+manifest['release_version']+' 출진 경로 충돌 수정 · 로그 없음','',
                '현재 설치/적용 패키지: '+manifest['release_version']+'; 설치 상태/세 DLL/Apply 해시 일치: '+str(pair and state.get('release_version')==manifest['release_version'])+'.',
                'v11.11 기능 모두 유지. 로컬과 공유판 DLL은 NO_RUNTIME_LOG로 함수 호출 기록·.log·예외 로그 작성 코드 제외.',
                '수정 전 실제 스택: sortie_dispatch → 원작 0x1EC890 → 경로 0x1843E0 → 접근 위반 0x183D32.',
                '점령 진형 출진의 idle/종류 255가 목적지 없음 48,400을 반환했다. 유효한 출진 성 헥스 대기 및 잘못된 단일 출진 경로 요청 차단으로 수정.',
                '사용자 확인(2026-10-14): 로그 ON 수정 후보에서 진행 버튼을 눌러도 충돌 없음. 배포용 NO_RUNTIME_LOG 빌드의 직접 게임 검증 및 장기 캠페인/렉 개선율은 미확인.',
                '점령 후 아군 건설 상한까지 건설; 공성 집단에만 인접 지역선; 군악대 사기 회복; 주변 거점 활동 강화.',
                '가용(성 병력-4,000): 20,000 미만 공성 군세 대기, 20,000~39,999 목표 성 ≤ 가용+5,000, 40,000 이상 목표 병력 무관.',
                '일반 성 공성 진형 최후, 관문 원작 우선, 공성 최대 8,000. 일반 친애 8슬롯/군주별 신규 논공 친애 500/RAM·입출력 최적화.',
                '검사 '+str(sum(r['checks'] for r in split['fixtures'].values()))+'개, ZIP 실제 적용/복구 '+str(share_check.get('checks'))+'개.',
                '설치 백업: '+str(fix_install.get('backup')),
                '배포 dist/SAN14_AI_v11.12_integrated.zip. tools/release_dispatch_fix.py prepare|install; 루트 Apply/Restore CMD.',
                '원본 EXE 변경 없음. 사용자 세이브 수정 없음. 충돌했던 원래 v11.11 ZIP은 계속 격리.',
                '상세 docs/DISPATCH_FIX_v11_12.md, docs/FIELD_IO_v11_11.md; 진단/사용자 확인 data/diagnostics/dispatch_fix_user_validation.json.','']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
            return
        if field_crash and manifest['release_version']=='v11.9':
            lines=['# v11.11 데모 충돌 후 v11.9 복구','',
                '현재 설치/적용 패키지: v11.9; 설치 기록 및 세 DLL/Apply 해시 일치: '+str(pair and state.get('release_version')=='v11.9')+'. 원본 EXE 변경 없음.',
                '사용자 확인: v11.9에서는 해당 충돌 없음. v11.10과 v11.11은 데모 시작 충돌.',
                'Windows 기록(2026-10-14 01:23:54 +09:00): SAN14CE_KO.exe, 접근 위반 0xc0000005, RVA 0x183D32. v11.10과 같은 원작 경로 탐색 지점.',
                'v11.10 후속 출진·중첩 경로 방지 후보를 포함한 v11.11도 실전 실패. 정확한 유발 호출/원인은 미확정; 호출 스택/덤프 없음.',
                '우선 조사: v11.10 추가 경로상 거점 점령 탐색 및 출진/건설 호출 순서. 모의 검사 통과를 충돌 해결로 간주하지 않음.',
                'v11.11 개발 소스는 조사용 보존 상태이며 현재 게임에는 연결하지 않음.',
                '충돌 DLL·소스 보존: '+field_crash['backup'],
                'v11.11 ZIP 격리: dist/quarantined/SAN14_AI_v11.11_integrated_crash.zip. 준비/설치/배포 도구 차단.',
                '복구 도구 tools/rollback_field_io.py. 루트 적용 CMD는 v11.9 패키지를 사용.',
                '상세 docs/FIELD_IO_v11_11.md, data/diagnostics/v11_11_crash_analysis.json.','']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
            return
        if manifest.get('field_io_revision'):
            field_install=read_json(PROJECT/'build/field_io_installation.json')
            lines=['# v11.11 점령·공성·저장 최적화','',
                '현재 설치/적용 패키지: '+manifest['release_version']+'; 설치 기록 및 세 DLL/Apply 해시 일치: '+str(pair and state.get('release_version')==manifest['release_version'])+'.',
                '원본 EXE 변경 없음. 실제 v11.11 게임 실행·턴 안정성·렉 개선율은 미확인.',
                '점령 후 아군 건설 상한까지 반복 건설; 아군 성 및 목표 적 성 주변 거점 활동 강화.',
                '공성 집단만 출진 성부터 목표 성까지 인접 지역선 사용. 군악대 회복·기존 임무 복귀 수정.',
                '가용병력(성 병력-4,000): 2만 미만 공성 군세 대기, 2만~4만 미만은 적 성 ≤ 가용+5,000, 4만 이상 적 병력 무관.',
                '일반 성은 야전 진형 우선/공성 마지막; 관문 공성 우선 유지. 공성 진형 최대 8,000; 약한 성 우회 제거.',
                '일반 친애 8슬롯의 빈자리 활용, 신규 논공 친애 군주별 500명/RAM 관리. 보조 저장·관계 검색·지원 위치 탐색 최적화.',
                '배포 DLL은 NO_RUNTIME_LOG: 설정과 관계없이 로그 작성 코드 제외.',
                'v11.9 같은 저장 1턴 정상은 사용자 확인. v11.10 출진 직후 명령 연기·중첩 경로 방지 후보 포함; 충돌 원인/해결 확정 아님.',
                '검사: '+str(sum(r['checks'] for r in split['fixtures'].values()))+'개 및 ZIP 적용/복구 '+str(share_check.get('checks'))+'개.',
                '설치 백업: '+str(field_install.get('backup')),
                '설치 도구 tools/install_field_io_release.py --install; 적용/해제는 루트 CMD.',
                '배포 dist/SAN14_AI_v11.11_integrated.zip; 상세 docs/FIELD_IO_v11_11.md.','']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
            return
        if crash and manifest['release_version']=='v11.9':
            lines=['# v11.10 충돌 후 v11.9 복구','',
                '현재 설치/적용 패키지: v11.9; 세 DLL 해시 일치: '+str(pair)+'. 원본 EXE 변경 없음.',
                'v11.10은 턴 진행 직후 두 번 접근 위반 0xc0000005 / 원작 RVA 0x183D32 충돌. DLL 연결만 검증됐으며 실전 검증 실패.',
                '사용자 확인(2026-10-14): 같은 저장 게임으로 복구 v11.9에서 1턴 진행해도 충돌하지 않았다. v11.10 원인/해결 확정은 아니다.',
                'v11.10 DLL·소스 보존: backup/crash_v11_10_20261012_175728. release_combat_priority.py 준비/설치 차단.',
                '현재 개발 소스: 출진 디스패처의 조기 지역 이동 연기, 원작 경로 탐색 중 모드 중첩 경로/명령 변경 차단 후보. 미설치; 원인/해결 미확정.',
                '진단 로그 OFF. 로더 dinput8.dll / 전투 SAN14_CombatAI.dll / 편의 SAN14_Convenience.dll.',
                '상세: docs/COMBAT_PRIORITY_v11_10.md, data/diagnostics/v11_10_crash_analysis.json.','']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            candidate=read_json(PROJECT/'build/field_io_preparation.json')
            if candidate.get('release_version')=='v11.11':
                lines+=['','v11.11 점령·건설·공성 지역선·군악대·친애/입출력·로그 제외·출진 조건·공성 진형 수정 배포판 준비.',
                    '로컬 설치는 v11.9 유지; 준비 build/field_io_package, 배포 dist/SAN14_AI_v11.11_integrated.zip.',
                    '검사: '+str(candidate['checks'])+'개 및 ZIP 적용/복구 검사. v11.11 실제 게임 턴 진행·성능 개선율 미확인.',
                    'v11.10 후속 출진·중첩 경로 방지 후보 포함. 상세 docs/FIELD_IO_v11_11.md.',
                    'v11.11 배포 DLL은 NO_RUNTIME_LOG: 설정과 관계없이 로그 작성자 제외.',
                    'v11.11 생성 도구 tools/prepare_field_io_release.py → tools/build_share.py --payload build/field_io_package.']
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
            return
        if manifest.get('combat_priority'):
            startup=read_json(PROJECT/'data/diagnostics/combat_merge_startup.json')
            live=bool(startup.get('startup_verified') and startup.get('release_version')==manifest['release_version'] and startup.get('files')==split['files'])
            lines=['# '+manifest['release_version']+' 점령·건설·공성 연결','',
                '설치 패키지/세 DLL 일치: '+str(pair)+'; 실제 실행 연결 확인: '+str(live)+'; 수정 후 실전 턴 진행: 미확인.',
                'dinput8.dll 순수 로더 / SAN14_CombatAI.dll 전투 / SAN14_Convenience.dll 편의 기능.',
                '추행진7 자동 출진 1,000명. 성·관문 돌격 및 현장 공성 모집에서 제외. 기존 출진 병력을 삭제하지 않음.',
                '아군 성 직접 인접 거점 → 그 거점의 인접 거점 우선. 1,000명 점령 → 건설 → 다음 점령 → 건설 반복 및 저장된 임무 복구.',
                '평온 지역 원본 후보가 없으면 같은 지역 아군 토지 검토. 원본 비용·시설 상한·건설 자격·실제 경로 검사 유지.',
                '원본 이동 경로상 미점령 거점·항구 점령 후 최종 성 목표 복귀. 한 칸 제한 없음. 도시/관문은 별도 공성/자동점령 보호 유지.',
                '목표 적 성 지역에서도 확보한 아군 타일의 투석대·요새·군악대·진·망루·함정 건설 검토. 공사 중 공동 군세 돌격 덮어쓰기 차단.',
                '성 고정 4,000명 보존. 여무장 자동 출진 1,000명. 가용 2만 공동 군세, 약한 성 단독 출진, 실제 피격·요격·50일 정지 대응 유지.',
                '검사: '+str(sum(r['checks'] for r in split['fixtures'].values()))+'개, tools/check_feature.ps1 -Feature dll-split -Build.',
                '백업 v11.9: '+split['backup']+'; 최초 갱신 tools/release_combat_priority.py --install.',
                '설치 패키지 build/input_package; 준비 build/combat_priority_package. 현버전 AI모드_적용.cmd / AI모드_해제_원본복구.cmd.',
                '일반 로그 OFF. 원격 함수 호출·프로세스 메모리 쓰기·원본 EXE/세이브 수정 없음. 이전 공유 ZIP v11.7.',
                '변경표 docs/COMBAT_PRIORITY_v11_10.md. 관우·장비·장씨 당시 장릉항 경로는 실전 원인 확정 미완료.','']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
            return
        if manifest.get('combat_revision'):
            startup=read_json(PROJECT/'data/diagnostics/combat_merge_startup.json')
            live=bool(startup.get('startup_verified') and startup.get('release_version')==manifest['release_version'] and
                startup.get('files')==split['files'])
            lines=['# '+manifest['release_version']+' 실전 피드백 수정','',
                '설치 패키지/세 DLL 일치: '+str(pair)+'; 실제 실행 연결 확인: '+str(live)+'; 수정 후 실전 턴 진행: 미확인.',
                'dinput8.dll 순수 로더 / SAN14_CombatAI.dll 전투 / SAN14_Convenience.dll 편의 기능.',
                '성 고정 4,000명 보존. 원군·점령·지원도 같은 기준. 위협에 따른 추가 병력 공제 없음.',
                '공성 군세: 성 출발부터 목표 인접 거점까지 원본 전체 경로 이동 → 거점 소유 및 전체 구성원 3헥스 집결 → 공유 건설/대열 이동.',
                '개별 공성 편성·경로 보호·병참 이동 임무가 이동 중 공성 군세를 빼가지 않음. 실제 피격·급박한 요격·50일 정지 대응 유지.',
                '아군 성 직접 인접 거점 점령 우선. 10일마다 성별 최대 두 명의 1,000명 점령 부대 출진 시도; 점령/건설 후 연속 확장.',
                '원본 공격 계획이 없어도 남은 가용 병력 2만 이상 공동 군세 편성 시도. 1만 미만 적 성은 단독 공격 및 원본 계획 유지.',
                '여무장 자동 출진은 1,000명. 공동 군세에서는 지원 개성 여무장만 후열. 플레이어 직접 출진 및 이미 출진한 병력 변경 없음.',
                '원본 후보·건강·능력·자금·지휘력·부대 슬롯 검사 유지; 원격 함수 호출/가짜 행동 노드/EXE·세이브 수정 없음.',
                '검사: '+str(sum(r['checks'] for r in split['fixtures'].values()))+'개, tools/check_feature.ps1 -Feature dll-split -Build.',
                '세 DLL 준비: build/combat_revision_package. 현버전 적용: AI모드_적용.cmd. 해제: AI모드_해제_원본복구.cmd.',
                '기준 v11.8 백업: '+split['backup']+'; 최초 갱신 설치: tools/release_combat_revision.py --install.',
                '로그 OFF. 이전 공유 ZIP은 v11.7 보관본. 변경표: docs/COMBAT_REVISION_v11_9.md.',
                '읽기 전용 관찰: 군세 85 대교/정보 짧은 이동·노숙 자기칸 대기. 양평 28,000 시점 원본 공격 계획 누락 사유는 확정하지 않음.','']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
            return
        if manifest.get('combat_merge'):
            startup=read_json(PROJECT/'data/diagnostics/combat_merge_startup.json')
            lines=['# v11.8 기존 공격 루트 병합','',
                '설치: '+str(pair)+'; 버전: '+manifest['release_version'],
                'dinput8.dll은 순수 로더. SAN14_CombatAI.dll 전투 판단 / SAN14_Convenience.dll 편의 기능.',
                '독립 combat_v2 개발 중단·보관, 게임 연결 없음. 기존 공격 루트에 최신 26개 요청 병합.',
                '가용 병력만 사용. 잔류병력 공제·성 2헥스 회피·안전 A*·지역선 강제 제거.',
                '가까운 성 주변 거점 원작 경로, 1,000명 인접 거점 보조 출진, C지역 아무 타일 자동점령 방지.',
                '군세 역할별 한 칸 도착 확인 / 공통 평지 이동력 / 좁은 길 종대 / 여성 후방 3헥스.',
                '원작 병참 회복 재검토 / 성·부대 실제 피격 목표 구분 / 3,000명 초과 퇴각 억제 / 같은 칸 50일 강제 퇴각.',
                '모의 회귀 검사: '+str(sum(r['checks'] for r in split['fixtures'].values()))+'개 통과.',
                '실행 후 연결 확인: '+str(startup.get('startup_verified',False))+'; 실제 저장게임 턴 진행: 미확인.',
                '설치 패키지: build/input_package. 이전 공유 ZIP은 v11.7 보관본이며 이번 설치는 v11.8.',
                '백업: '+split['backup'],'일반 진단 로그 OFF.',
                '변경표: docs/COMBAT_MERGE_v11_8.md',
                '검사: tools/check_feature.ps1 -Feature dll-split -Build',
                '설치: tools/release_combat_merge.py --install (v11.7에서 최초 갱신용), 이후 AI모드_적용.cmd.',
                '해제: AI모드_해제_원본복구.cmd. 원본 EXE와 세이브를 수정하지 않음.','']
            for name in split['files']:lines.append(name+': '+str(digest(GAME/name)))
            (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
            return
        lines=['# 로더·전투 판단·편의 기능 DLL 분리','',
            '생성: '+datetime.now(timezone(timedelta(hours=9))).isoformat(timespec='seconds'),
            '버전: '+manifest['release_version']+'; 설치 상태: '+str(bool(state.get('installed')))+'; 모든 DLL/Apply 일치: '+str(pair),
            '', '`SAN14_CombatAI.dll`: 출진·이동·점령/건설·병참·피격·퇴각 판단.',
            '`SAN14_Convenience.dll`: 인접 교전 보너스·부상 보호·내정·인구·인사·UI·논공·명품·열전·동등 거리 최적화.',
            '`dinput8.dll`: 입력 전달과 위 두 기능 DLL 로드만 수행. 게임 기능 소스는 포함하지 않음.',
            '원래 v11.5 행동 규칙 유지. ABI 1, 두 기능 DLL을 검증/준비 후 연결. 일반 출진 TLS를 DLL 간 원래 호출 스레드에서 전달.',
            '', '각 설치 SHA256:']
        for name in split['files']:lines.append('- '+name+': `'+str(digest(GAME/name))+'`')
        lines+=['','회귀 검사: '+str(sum(r['checks'] for r in split['fixtures'].values()))+'개.',
            '공유 설치·해제 검사: '+str(share_check.get('checks'))+'개. 실전 게임 진행: 미확인.',
            '공유: '+str(share.get('zip')),'백업: '+split['backup'],
            '최신 검사/빌드: tools/check_feature.ps1 -Feature dll-split -Build.',
            '설치/해제: 기존 AI모드_적용.cmd / AI모드_해제_원본복구.cmd. 세 파일 해시/상태를 함께 관리.',
            '원본 EXE·세이브·기존 그래픽 모드 보존. 일반 배포 로그 OFF; 로컬 설정은 diagnostics.ini에서 확인.',
            '근거: research/dll_split/기능분리.md. 이전 단일 통합 빌드는 회귀 비교용.']
        if (PROJECT/'build/folder_cleanup_backup.json').exists():
            lines+=['','게임 폴더 정리: 루트 CMD는 현재 적용·해제 두 개만 유지. 선택 도구는 optional_tools에 이동.',
                '기존 AI_Analysis는 research/AI_Analysis, 이전 개발 소스·ZIP은 legacy로 이동. 개발 도구의 PE/캡처 경로도 수정.',
                '생성 로그 삭제·로컬 진단 OFF. 안내 docs/FOLDER_LAYOUT.md; 기록 build/folder_cleanup_actions.json.']
        if (PROJECT/'src/combat_v2/sortie.cpp').exists():
            results=[read_json(PROJECT/('build/development/combat_v2/'+name)) for name in
                     ('fixture.json','movement_fixture.json','features_fixture.json','installed_verification.json')]
            lines+=['','새 전투: 기존 병참 차단·건설·연속 점령·수정된 피격 대응 부분만 이식. 성 회피·안전 거점·보호 부대·campaign_safe_route 제외.',
                '가용 병력 20,000 이상, 예비병력 공제 없음. 원본 후보 2~7부대 공성 군세·인접 거점 집결까지 이동중시·성까지 대열중시.',
                '공성 임무 잠금: 다른 이동·병참 차단·연속 점령 차단, 건설/피격 대응 후 같은 목표 복귀. 성 목표와 임시 적 부대 ID 분리.',
                '게임 연결 금지: 정적 라이브러리만 빌드. 현재 설치 DLL/Apply/공유는 위 v11.7 그대로.',
                '자체 모의 검사와 분리/파일 검사 '+str(sum(r.get('checks',0) for r in results))+'개; NativePort/MovementPort/FeaturePort의 실제 게임 구현·AI 호출부 등록은 아직 없음.',
                '검사 tools/check_feature.ps1 -Feature combat-rebuild -Build; 근거 research/combat_rebuild/제로베이스_군세출진.md.']
        (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
        return
    paths=[path for entry in registry['modules'].values() for path in entry['sources']]
    for name in set(paths):
        if not (PROJECT/name).is_file():raise RuntimeError('Missing registered source: '+name)
    development=read_json(PROJECT/'build/development/small_army_campaign.json')
    candidate=read_json(PROJECT/'build/recruitment_ui_candidate.json')
    if candidate.get('field_activity'):
        import configparser
        field=candidate['field_activity'];promoted=matches and actual==candidate['candidate_sha256']==share.get('dll_sha256')
        ini=configparser.ConfigParser();ini.read(PROJECT/'diagnostics.ini',encoding='utf-8')
        total=sum(candidate[k]['checks'] for k in field['fixture_checks'])
        saves=read_json(PROJECT/'build/progress_save_checkpoint.json')
        fresh=Path(saves.get('folder','')).resolve().is_relative_to((PROJECT/field['backup_snapshot']).resolve())
        lines=['# 현재 통합 설치와 군세 이동·야전 활동 후보','',
          '생성: '+datetime.now(timezone(timedelta(hours=9))).isoformat(timespec='seconds'),
          '설치 버전: '+manifest.get('release_version','미기록')+'; v11.5 후보 설치/Apply/공유 일치: '+str(promoted),
          '설치 DLL SHA256: `'+str(actual)+'`','후보 DLL SHA256: `'+candidate['candidate_sha256']+'`',
          '공유 후보: `dist/SAN14_AI_v11.5_integrated.zip`; 공유 검사 '+str(share_check.get('checks'))+'개; 최신 ZIP 해시 일치 '+str(share_check.get('zip_sha256')==share.get('zip_sha256')),'',
          '진행 중인 점령·건설을 공성 군세 모집보다 우선. 성8헥스 이내에서 실제 공격 전열과 남은 수비 병력 비율로 전환.',
          '군세: 가장 느린 부대의 한 턴10일 이동량2/3를 진군 끝점으로 하고 중계5개를 경로 전체에 간격 배치. 독립 부대·건설은 실제 목표까지 이동.',
          '성에서 정확한 대열 조립·방향 전환 대기와 중계별 속도0 제거. 선두/중진/후열 유지, 도착 차이 한 턴 허용. 좁은 길은 짧은 종대 시도.',
          '친선 지역 중심 의무 경유 제거. 인접 그래프는 우선 경로이며 원작 통행 가능 지형 우회 허용. 동맹/통행 불가/성 위험 정책 유지.',
          '보호는 적 성/항구 통과에 필요. 실제 도착 전 통과 금지. 가까운 야전 지원이 없으면 원작 자원·예비 병력·지휘력 검사로1000명 지원 요청. 통과 후 또는20일에 대기 해제.',
          '새 훅/AI 스레드/EXE 수정/원격 함수 호출 없음. v11.4 목표 퇴각 수정·소규모 군세와 기존 통합 기능 유지.','',
          '검사: 통합 모의 회귀 '+str(total)+'개; 목표 퇴각 '+str(candidate['target_retreat']['fixture']['checks'])+'개; 원작 거리/로그387376개·append45개·원복 '+str(field['rollback_fixture']['checks'])+'개; 통합 /W4 /WX 빌드.',
          '백업: '+field['backup_snapshot']+'; 설치 v11.4 DLL/Apply/메타/ZIP/소스/설정335파일 SHA 확인.',
          '이번 종료 후 세이브 체크포인트: '+str(fresh)+'; 설치 전용 백업 '+field['install_backup'],
          '직전v11.4 원복: 게임 종료 후 python -X utf8 AI_TacticsMod/tools/rollback_progress_native.py --apply. 세이브/개발 소스 유지.',
          '검사 tools/check_feature.ps1 -Feature field-activity -Build; 준비 tools/prepare_field_activity.py.','',
          '공유/일반 실행 로그OFF·profile0. 현재 로컬 diagnostics enabled='+str(ini.getint('diagnostics','enabled',fallback=0))+'; 사용자 요청 재현 조사용. 변경 시 재시작 필요.',
          '실전 확인: '+str(field.get('gameplay_verified',False))+'; 고순/후성/초선 출발·장료 장거리 점령·유비7명 대열·지원군·평원/남피 퇴각 해소와 장기 안정성 남음.',
          '원작 일일 점수269150·지형2318B0/BE120·이동률2B0290로 예산 예측. 혼잡/전투/날씨에 따라 도착 변화. 원작 중계 구간 재생성은 첫 발령 구간 관찰 검사.',
          '근거·한계: research/field_activity/군세이동_야전활동_수정.md. 기존 관찰 네 부대는 귀환11/12가 아니므로 성 앞 정지를 귀환 충돌로 단정하지 않음.',
          '진행 대기 약30초 감소·세력→군단 병렬화는 아직 검증/구현 없음.']
        (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
        print(json.dumps({'handoff':str(PROJECT/'development/CURRENT.md'),'installed_share_match':promoted,'candidate':'v11.5'}));return
    if candidate.get('target_retreat'):
        import configparser
        retreat=candidate['target_retreat'];promoted=matches and actual==candidate['candidate_sha256']==share.get('dll_sha256')
        ini=configparser.ConfigParser();ini.read(PROJECT/'diagnostics.ini',encoding='utf-8')
        local_logs=ini.getint('diagnostics','enabled',fallback=0)
        saves=read_json(PROJECT/'build/progress_save_checkpoint.json')
        save_folder=Path(saves.get('folder','')).resolve()
        fresh_saves=save_folder.is_relative_to((PROJECT/retreat['backup_snapshot']).resolve())
        live=read_json(PROJECT/'build/target_retreat_live_verification.json')
        live_matches=live.get('dll_sha256')==candidate['candidate_sha256']
        lines=['# 현재 통합 설치와 목표 부대 퇴각 명령 수정','',
            '생성: '+datetime.now(timezone(timedelta(hours=9))).isoformat(timespec='seconds'),
            '설치 버전: '+manifest.get('release_version','미기록')+'; v11.4 후보 설치/Apply/manifest/공유 일치: '+str(promoted),
            '설치 DLL SHA256: `'+str(actual)+'`','후보 DLL SHA256: `'+candidate['candidate_sha256']+'`',
            '공유 후보: `dist/SAN14_AI_v11.4_integrated.zip`',
            '공유 적용·해제·기본 설정 검사: '+str(share_check.get('checks'))+'개; 최신 ZIP 해시 일치: '+str(share_check.get('zip_sha256')==share.get('zip_sha256')),'',
            'v11.4: 가까운 적이 퇴각 명령11/12를 내렸다는 이유만으로 AI 공격 대상을 버리는 원작 경로 수정.',
            '일자272190: AI/위임의 공격7·부대목표27·유효 적·거리6이하·아군1000이상에만 취소 억제 비트8을 호출 동안 사용. finally에서 원상 복구.',
            '별개 보조목표·특수 상태 제외. 원작 자체 병력/사기/위험 판단으로 바뀐 퇴각은 유지. 플레이어 직접 명령 제외.',
            '군세 AI69BD0: 6A26A/6A948의11/12 명령 상태만 후보 제외하는3바이트 조건 수정. 유효성/실제 귀환지역2/3/거리/위험 평가는 원작 유지.',
            '이미 발령된 아군 귀환 명령은 취소하지 않음. 적용 후 재현에는 문제 이전 세이브 권장.',
            '설정 target_retreat.ini enabled=0이면 새3개 훅 발행 안 함; 게임 재시작 필요.',
            'AI 신규 스레드·난수·적 부대 데이터 수정 없음. 외부 메모리 쓰기/원격 게임 호출 없음. 원본 EXE 유지.','',
            '검사: 기존 통합 회귀12,477개; 새 목표 퇴각 '+str(retreat['fixture']['checks'])+'개; 원작 필터 기계어1024조건 비교·완전 가드·실패3단계 원복·SEH/중첩·unwind·실제 gate.',
            '원작 거리/로그 정책387,376개; 로그 append45개; 원복 '+str(retreat['rollback_fixture']['checks'])+'개; 통합 /W4 /WX 빌드.',
            '백업: '+retreat['backup_snapshot']+'; 설치 v11.3 DLL/Apply/메타/ZIP/소스/설정317개 SHA 확인. 당시 로컬 로그ON도 보존.',
            '이번 변경용 종료 후 세이브 백업 완료: '+str(fresh_saves),
            '설치 전용 백업: '+retreat['install_backup'],
            '직전 v11.3 원복: 게임 종료 후 python -X utf8 AI_TacticsMod/tools/rollback_progress_native.py --apply. 세이브/개발 소스 유지.',
            '검사 경로: tools/check_feature.ps1 -Feature target-retreat -Build; 준비 tools/prepare_target_retreat.py.','',
            '로그 기본값: 공유/일반 실행OFF, profile=0. 명시 요청 또는 사전 개발 로그 고지 후에만ON. 저장용 sidecar 유지.',
            '현재 로컬 diagnostics.ini enabled='+str(local_logs)+'; 이번 사용자 요청으로 재현 기록용ON. 조사 종료 후 알리고OFF/재시작.',
            'ON 로그: target_retreat_nearby_attack_kept / other_native_decision; order_before/after의 value=action+(kind<<8), hex=targetID; enemy_action_before의 value=상대action, hex=targetID.',
            '실제 시작 확인: '+str(live_matches and live.get('startup_verified',False))+'; 평원·남피 반복 해소 실전 확인: '+str(live_matches and live.get('gameplay_verified',False)),
            '실전 미확인: 원소/안량/문추와 장각/장보/장량의 연쇄 퇴각 해소·장기 안정성·위임군단/항구 사례.',
            '설계·근거·한계: research/retreat_diagnostics/목표부대_퇴각명령_교전유지.md. 기존 로그만으로 과거2~3회 연쇄 순서 전체는 확인할 수 없었음.',
            'v11.1 소규모 군세/현장 합류/공성/집결 실패 임무 복귀와 기존 통합 기능 유지.',
            '진행 대기: 사용자 이전3턴 약30초·약간 부드러움. 세력→군단 캐시/병렬화·전체 대기 감소 검증 아직 없음.']
        (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
        print(json.dumps({'handoff':str(PROJECT/'development/CURRENT.md'),'installed_share_match':promoted,'candidate':'v11.4'}));return
    if candidate.get('progress_log'):
        logging=candidate['progress_log'];native=candidate['progress_native']
        promoted=matches and actual==candidate['candidate_sha256']==share.get('dll_sha256')
        lines=['# 현재 통합 설치와 로그 정책','',
            '생성: '+datetime.now(timezone(timedelta(hours=9))).isoformat(timespec='seconds'),
            '설치 버전: '+manifest.get('release_version','미기록')+'; 후보 v11.3 설치/Apply/manifest/공유 일치: '+str(promoted),
            '설치 DLL SHA256: `'+str(actual)+'`','후보 DLL SHA256: `'+candidate['candidate_sha256']+'`',
            '공유 ZIP: `dist/SAN14_AI_v11.3_integrated.zip`',
            'ZIP 적용/해제·기본 OFF 검사: '+str(share_check.get('status'))+' / '+str(share_check.get('checks'))+'개; 최신 ZIP 해시 일치: '+str(share_check.get('zip_sha256')==share.get('zip_sha256')),'',
            '일반 실행·공유 배포 기본 로그 OFF. diagnostics.ini가 없어도 OFF.',
            '사용자 “로그를 켜줘” 요청 또는 먼저 “로그를 켜고 개발하자” 사전 고지한 경우에만 ON. 개발 시작만으로 자동 활성화 금지.',
            'diagnostics.ini [diagnostics] enabled=0; progress_native.ini profile=0. 변경은 게임 재시작 후 적용. 측정 후 OFF로 복귀.',
            'OFF: 이벤트 인수 계산/형식화/잠금/쓰기·전투 수치·예외 로그 중단. AI 전체/세력/군단/부대 측정 훅 발행하지 않음.',
            '개발 ON: 일반 이벤트 로그 파일 핸들 재사용, 기존 동기 쓰기/순서 유지. 예외 기록은 독립 경로.',
            '논공 .meritbond·명품창고 .warehouse는 저장 데이터이며 OFF에서도 저장/불러오기 유지. 기존 로그 파일 삭제 없음.',
            'v11.1 소규모 군세·현장 합류·공성·집결 실패 임무 복귀, v11.2 동등 헥스 거리 계산과 기존 통합 기능 유지.',
            'AI 명령/평가 순서/난수 변경 없음. 세력→군단 반복 계산 캐시/병렬화 아직 구현하지 않음.','',
            '검사: 기존 모의 회귀12,477개; 통합 /W4 /WX 빌드.',
            '로그 append 검사: '+str(logging['fixtures']['log_append_fixture']['checks'])+'개; 동시8,000줄·실패 재시도.',
            '원작 훅/기본 OFF/개발 ON 검사: '+str(native['fixtures']['progress_native_fixture']['checks'])+'개; 거리 원본 비교787,200쌍.',
            '별도 거리 검사: 원본 비교2,548,800쌍. 원복 트랜잭션 검사: '+str(logging['rollback_fixture']['checks'])+'개.',
            '백업: '+logging['backup_snapshot']+'; 설치 v11.2 DLL/Apply/상태/ZIP/설정 보존. 개발 소스 백업은 로그 OFF 작업 중 상태.',
            '종료 후 세이브 백업: '+str(read_json(PROJECT/'build/progress_save_checkpoint.json').get('status','미기록')),
            '설치 전용 백업: '+logging['install_backup'],
            '직전 설치 v11.2 원복: 게임 종료 후 python -X utf8 AI_TacticsMod/tools/rollback_progress_native.py --apply. 세이브·개발 소스 유지.',
            '검사 경로: tools/check_feature.ps1 -Feature progress-log -Build. 준비 tools/prepare_progress_log.py.',
            '조사: 기존 WPR 군단 스택7,591표본 중 log_event4,253(약56%); 버튼 시간 비율 아님. 원작 병목 전체가 로그라는 결론 아님.',
            '이전 v11.2 실전: 사용자3턴 약30초, 비슷하지만 조금 부드러운 느낌. 새 v11.3 결과 아님.',
            '새 v11.3 실전 미확인: 로그 미증가·같은 세이브 진행 대기 개선율·장기 안정성·소규모 군세 원소 사례.',
            '설계·한계: research/performance/진행_로그부하개선.md. 원본 EXE 유지·외부 메모리 쓰기/원격 게임 호출 없음.']
        (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
        print(json.dumps({'handoff':str(PROJECT/'development/CURRENT.md'),'installed_share_match':promoted,'candidate':'v11.3'}));return
    if candidate.get('progress_native'):
        native=candidate['progress_native'];promoted=matches and actual==candidate['candidate_sha256']==share.get('dll_sha256')
        live=read_json(PROJECT/'build/progress_native_live_verification.json')
        lines=['# 현재 통합 설치와 원작 진행 최적화 시험','',
            '생성: '+datetime.now(timezone(timedelta(hours=9))).isoformat(timespec='seconds'),
            '설치 버전: '+manifest.get('release_version','미기록')+'; 후보 v11.2 설치/Apply/manifest/공유 일치: '+str(promoted),
            '설치 DLL SHA256: `'+str(actual)+'`',
            '후보 DLL SHA256: `'+candidate['candidate_sha256']+'`',
            '공유 ZIP: `dist/SAN14_AI_v11.2_integrated.zip`',
            'ZIP 적용/해제 검사: '+str(share_check.get('status'))+' / '+str(share_check.get('checks'))+'개; 최신 ZIP 해시 일치: '+str(share_check.get('zip_sha256')==share.get('zip_sha256')),'',
            'v11.1 소규모 군세·현장 합류·공성·집결 실패 임무 복귀와 기존 전체 통합 기능 유지.',
            'v11.2: 원작 공통 헥스 거리2AA850을 불변 좌표표로 정확하게 계산. 범위 밖 인자는 원본 복귀.',
            '원작43E030 전체·세력D3D50·군단D3CE0·부대D3C70 QPC 시간/호출 수 기록. 원래 명령/AI 순서 유지.',
            '군세D3D90은 안전한16바이트 발행 범위를 넘으므로 직접 측정 제외. 잔여 시간은 군세+기타 비용.',
            '세력→군단 반복 구조의 캐시/병렬화는 아직 구현하지 않음. 35초 대기 해결/전체 개선율 미확인.',
            'QPC는 기존 모드/원작 하위 호출·대기도 포함하는 벽시계. 호출 수에는 원작 가드로 건너뛴 대상 포함.',
            '새5개 바이트 가드·원자적 발행·실패 원복·AI gate unwind·재진입/SEH·8스레드 검사. 외부 게임 메모리 쓰기/원격 호출 없음.','',
            '검사: 기존 모의 회귀12,477개; 통합 /W4 /WX 빌드. 실제 게임 검증 아님.']
        for name,result in native['fixtures'].items():lines.append('- '+name+': '+str(result['checks'])+'검사 / '+str(result.get('comparisons',result.get('native_comparisons')))+'쌍 원본 비교')
        lines += ['- 로컬 v11.1 원복 트랜잭션/보호 검사: '+str(native['rollback_fixture']['checks'])+'개.','',
            '백업: '+native['backup_snapshot']+'; snapshot 전체 SHA 검증. EXE·DLL·Apply·상태·ZIP·개발 소스·sidecar.',
            '종료 후 세이브 백업: '+str(read_json(PROJECT/'build/progress_save_checkpoint.json').get('status','아직 미수행')),
            '설치 전용 백업: '+native['install_backup']+'. 과거 백업 유지.',
            '설정: progress_native.ini enabled=0 새 훅 끔; geometry=0 거리만 원본/단계 측정 유지; 재시작 필요.',
            'v11.1 원복: 게임 종료 후 python -X utf8 AI_TacticsMod/tools/rollback_progress_native.py --apply. 개발 소스·세이브 유지.',
            '검사 경로: tools/check_feature.ps1 -Feature progress-native -Build.',
            '배포 준비: tools/prepare_progress_native.py; 설치 tools/install_recruitment_ui_candidate.py.',
            '설계·한계: research/performance/진행_원작최적화_시험.md; 이전 WPR 조사: 진행_처리와개선제안.md.',
            '실전 미확인: 같은 영웅집결 세이브 A/B·전체 대기 감소·장기 안정성·소규모 군세 원소 사례.',
            '새 DLL/훅 시작 확인: '+str(live.get('startup_verified',False))+'; 원작 단계 기록 확인: '+str(live.get('phase_records_verified',False)),
            '실측 기록: '+str(live.get('phase_sample_count',0))+' AI 호출; 군단/전체 호출 경과시간 합계 비율 '+str(live.get('district_fraction','미기록'))+' (버튼 시간 비율 아님; 모드 하위 호출 포함).',
            '사용자 새 진행 관찰: '+str(live.get('user_button_observation','아직 미응답'))+'; 같은 세이브 대조/전체 개선율 미확인.',
            '최신 세이브와 맞는 sidecar 추가 백업: '+str(read_json(PROJECT/'build/progress_sidecar_checkpoint.json').get('status','미기록')),
            '이전 v11.1 게임 시작 확인과15/35/36/35초 연속 턴 관찰은 새 v11.2 검증이 아님. WPR 유실0 기록은 로컬 data/performance.',
            '다음: 군단/세력 비용 집중 구간의 하위 루틴 재조사→순수 반복 계산 재사용→검증 가능한 병렬화.']
        (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
        print(json.dumps({'handoff':str(PROJECT/'development/CURRENT.md'),'installed_share_match':promoted,'candidate':'v11.2'}));return
    if candidate.get('small_army_campaign'):
        promoted=matches and actual==candidate.get('candidate_sha256')==share.get('dll_sha256')
        lines=['# 현재 통합 설치와 다음 조사','',
            '생성: '+datetime.now(timezone(timedelta(hours=9))).isoformat(timespec='seconds'),
            '설치 버전: '+manifest.get('release_version','미기록')+'; 설치/Apply/manifest/공유 DLL 일치: '+str(promoted),
            '설치 DLL SHA256: `'+str(actual)+'`',
            '공유 ZIP: `dist/SAN14_AI_v11.1_integrated.zip`',
            'ZIP 설치/복원 검사: '+str(share_check.get('status'))+' / '+str(share_check.get('checks'))+'개; 현재 ZIP 해시 일치: '+str(share_check.get('zip_sha256')==share.get('zip_sha256')),'',
            '- 소규모 군세 후보를 통합 v11.1에 반영. 고정4만 출진 조건 제거, 가용 병력별2~7부대.',
            '- 완료한 지역 임무/idle 독립 부대 현장 합류. native 병력·진형·Troops 유지.',
            '- 공성 핵심(전열/공성진/5,000↑중열)과 목표8헥스 적+성 주둔1.5배 비교. 작은 중열·지원 후방 유지.',
            '- 목표 소유 인접 일반 거점2개 이하 준비→공성,3개 이상 지역점령. 지원 도착 필수 아님.',
            '- 집결/접근 진행 없으면2턴 뒤 원래 임무 복귀·2턴 유예. 집결 실패 자동퇴각 없음; 기존 개인5턴 정지퇴각 유지.',
            '- 잔류 최대(3,000,주둔20%,성10헥스 적 야전1.5배). 기존 비용/부상/수동 제외·기존 통합 기능 유지.','',
            '검사: native/ABI 모의 회귀 및 통합 /W4 /WX 빌드. 실제 게임 진행 검증 아님.']
        for key in ['native_fixture','sortie_fixture','merit_fixture','fixture','personnel_fixture','awards_fixture']:
            result=candidate[key];lines.append('- '+key+': '+result['status']+' / '+str(result['checks'])+'개')
        lines += ['', '실전 미확인: 원소12,000/국의4,470/견씨1,000 사례·좁은 길/공성/지원/집결 실패/저장 재시작·장기 안정성. DLL 시작 확인은 아래 최신 측정 기록 참고.',
            '설계: research/campaign/소규모군세_현장합류.md; 코드 campaign_policy.h/native_campaign_operations.inl/sortie_policy.h.',
            '검사: tools/check_feature.ps1 -Feature small-army-campaign -Build.',
            '배포 준비: tools/prepare_small_army.py; 전용 백업 backup/recruitment_ui_before_small_army_20261012.',
            '다음: 삼국지14 본체의 진행 처리 최적화. 이번 단계는 조사·제안·측정만; 기존 AI 동작 유지.',
            '기존 build/development/small_army_campaign.json은 dev1 과거 기록. 현재 배포 증거는 build/recruitment_ui_candidate.json.',
            '원격 게임 함수 호출·실행 중 외부 쓰기 없음. 새 배포에는 새 baseline/고유 백업 필요.']
        study=read_json(PROJECT/'build/progress_study.json')
        if study.get('mode')=='read_only_external_accounting':
            lines += ['', '진행 조사/측정 준비: research/performance/진행_처리와개선제안.md.',
                'AI모드_진행성능측정.cmd: Windows 프로세스/스레드 CPU 시간만 기록. DLL/게임 AI 규칙/실행 메모리 변경 없음.',
                '검사: tools/check_feature.ps1 -Feature progress-study (빌드 불필요). 측정/스택 파서 '+str(study.get('fixture_checks'))+'개 통과.',
                '실측: 영웅집결 PID12404·90초174샘플. 사용자가 첫 진행→일자 시작 약15초 후 정지 관찰(기존 약30초 보고와 별개).',
                '추가 새 명령 턴의 사용자 대기 관찰35/36/35초. 동일 세이브 재로드 반복 아님. v11.1 DLL 경로/시작 hooks 로그 확인.',
                '초반20초 약1.8~2.0코어,최다CPU 스레드 약94~98%. 정지 뒤에도 바빠 AI 스레드/함수 병목 확정 불가.',
                '관리자 스택2회 저장·추적 종료. 첫 기록346,965이벤트 유실; 좁은 CPU 기록76.96초·유실0·게임CPU148,945샘플.',
                '복수 스레드와 원작B36460 원자적 비트맵 반복 확인. 작업분배/동기화 가설; 실제 AI 연결·경합 시간 미확인.',
                '정확한 버튼~완료 구간·원작대조·개선율 미확인. 본체 반복조회/후보/경로 재사용→순수 계산 병렬화·순차 명령 적용 제안.',
                '측정 기록은 data/performance; 개인정보/현 세이브 기록은 공유 ZIP에 포함하지 않음. 실측은 소규모 군세 실전 기능 검증을 대신하지 않음.']
        (PROJECT/'development/CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
        print(json.dumps({'handoff':str(PROJECT/'development/CURRENT.md'),'installed_share_match':promoted}))
        return
    if development.get('small_army_campaign'):
        binary=PROJECT/'build/development/SAN14_AIInput_recruitment_ui_candidate.dll'
        lines=['# 현재 설치와 소규모 군세 개발 후보','',
            '생성: '+datetime.now(timezone(timedelta(hours=9))).isoformat(timespec='seconds'),
            '설치 버전: '+manifest.get('release_version','미기록')+'; 설치/Apply/manifest 일치: '+str(matches),
            '설치 DLL SHA256: `'+str(actual)+'`',
            '개발 후보 SHA256: `'+str(digest(binary))+'`; 검증 기록 일치: '+str(digest(binary)==development.get('candidate_sha256')),
            '후보 경로: build/development/SAN14_AIInput_recruitment_ui_candidate.dll. 설치/Apply/공유 패키지는 이번 작업에서 변경하지 않음.','',
            '- 출진 4만 조건 제거. 동원 병력에 따라 2~7부대, 전열 추가 배분2.5배·안행 중열·지원1,000명.',
            '- 신규 출진/현장 원군/추가 지원의 잔류 기준: 최대(3,000,주둔20%,10헥스 적 야전1.5배).',
            '- 완료한 지역 임무/대기 중인 독립 부대는 기존 병력·진형·native Troops를 유지하며 공동 작전 합류. 방어/미완료 점령/퇴각/수동/다른 성 공격 제외.',
            '- 실제 등록된 공성 핵심 병력만 적 성 주둔+주변8헥스 적 야전의1.5배 조건에 포함. 다른 성의 저장 병력 제외. 지원의 성5헥스 집결 요구 제거.',
            '- 목표 소유 인접 일반 거점2개 이하이면 집결→준비→공성. 3개 이상이면 지역 점령; 무작위15% 공성 제거.',
            '- 전열 및5,000이상 중열만 공성. 작은 중열/지원은 성2헥스 밖에서 전열 뒤를 추종. 적 부대 방향으로 대열 갱신.',
            '- 기존 완공 진/요새3헥스 안에서 공성 핵심은 한 턴 준비. 강한 성에는 합법 요새/군악대 건설 시도, 최대2턴 뒤 원래 임무 재개.',
            '- 집결/접근 진행이2턴 없으면 원래 임무 복귀와2턴 재합류 유예. 부족한 독립 부대는 임무 유지. 기존 개인5턴 정지 퇴각과 긴급 회복은 유지.',
            '- 장애물 파괴는 기존 증거·허용 범위와 native 공격만 재사용. 새로운 훅/원격 호출 없음.','',
            '검사(모의 native/ABI, 실제 게임 턴 검증 아님):']
        for name,result in development.get('fixtures',{}).items():
            lines.append('- '+name+': '+str(result.get('status'))+' / '+str(result.get('checks'))+'개')
        lines += ['', '실전 미확인: 새 DLL 로드, 원소/국의/견씨 원 세이브, 좁은 길·교전·버프 지속·장기 진행·저장/재시작.',
            '관련 설계: research/campaign/소규모군세_현장합류.md; 정책 campaign_policy.h; native_campaign_operations.inl; 검사 small-army-campaign 또는 campaign-revision.',
            '빌드: tools/check_feature.ps1 -Feature small-army-campaign -Build. 통합 빌드만 사용; 기존 merit/recruitment/awards 등 유지.',
            '새 출시 시 별도 baseline/백업/증거와 guarded installer 필요. 기존 설치 DLL/공유는 현재 설치 버전을 유지.']
        folder=PROJECT/'development';folder.mkdir(exist_ok=True)
        (folder/'CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
        print(json.dumps({'handoff':str(folder/'CURRENT.md'),'installed_files_match':matches,'development_candidate':True,'registered_sources':len(set(paths))}))
        return
    candidate=read_json(PROJECT/'build/recruitment_ui_candidate.json')
    if candidate.get('campaign_support_revision'):
        promoted=actual==candidate.get('candidate_sha256')==payload==share.get('dll_sha256')
        lines=['# 새 대화용 현재 상태','',
            '생성: '+datetime.now(timezone(timedelta(hours=9))).isoformat(timespec='seconds'),
            '현재 설치 버전: '+manifest.get('release_version','미기록')+'. 최신 후보/공유: '+candidate['release_version']+' 시험판.',
            '설치 DLL SHA256: `'+str(actual)+'`',
            '설치/Apply/manifest 일치: '+str(matches)+'. 최신 후보/공유 DLL도 일치: '+str(promoted),
            '원본 EXE SHA256: `'+str(digest(GAME/'SAN14CE_KO.exe'))+'`',
            '최신 설치일: '+str(install.get('time','미기록')),
            '공유 ZIP: `'+str(share.get('zip'))+'`','',
            'v11: 사기 회복·집결 공성·병참 지원·원군 요청.',
            '- 중산9/고산10/험산19 또는 사기70이하: 경로 군악대, 없으면 합법 군악대 우선 건설. 일반 임무90이하는5헥스 기존 군악대. 실제 효과로100까지 회복 후 원래 임무 복귀.',
            '- 목표 성15헥스 아군 야전 합계, 적 야전+성 주둔 합계. 적*3<=아군*2 병력 조건을 유지.',
            '- 목표 성 세력이 직접 소유한 인접 일반 거점만 계산. 0~1개100% 집결/공성,2개1부대 점령+나머지 공성,3개 이상15%. 공격 측/제3세력 제외; 같은 세력/성/턴 확률 공유, 결정 후 집결 유지.',
            '- 집결 반경3~5, 도착한 가용 부대에 한 AI 콜백에서 성 공격4/kind5 명령. 피격 대응 부대 공격7/kind27 유지.',
            '- 병력/부대 부족 시 출발 성 및60헥스 내 같은 세력 다른 성/군단에 원군 요청. 직접 조작 군단·부상 제외, 지휘/금/명령서 검사·실제 비용. 오는 원군 중복 방지, 한 요청 최대4부대.',
            '- 원군 수비 잔류는 최대(2만, 주둔1/3,10헥스 적 병력1.5배). 현장 원군 요청에는 원본 출진 노드 수비값이 없어 새 보수적 기준 사용.',
            '- A-C-B 중간 성 지역은 성 중심 대신 성에서3헥스 이상 타일.15헥스 내 보호 부대 이동 요청, 실제 C지역 존재 전에는 생략 불가. C의 실제 공성 부대는 공성을 유지하면서 자연 보호.',
            '- 막히면 완공 진/요새3헥스 중첩 범위 타일 우선 대기. 비공성 성2헥스 회피는 부대/턴별50%, 같은 턴 재탐색은 같은 선택.',
            '- 병력 부족 퇴각1,000미만, 개인 같은 칸 연속5턴 무조건 퇴각 유지. 회복/집결/보호 대기도 예외 없음. 임시 상태는 재시작 후 재관찰.','',
            '읽기 전용 PID7852 관찰: 왕광 Army53 3,000·사기92·자중 고산·중계 없음. 방열 Army75 8,000·107·무양 도로·중계 없음, 진223 버프3헥스 안. 정확한 대기 원인은 로그만으로 확정 불가.',
            '기존 기능: AI/내정·군세·논공 친애/저장·자동등용/UI·숨은 충성·AI 등용 제한·부상/본초학·명품창고/배분/외교·열전 스크롤·인구4배 유지.',
            'v10.4 부대 공격 타입 오류 수정은 유지. 수신자 동일 종료 원인 및 장기 안정성은 아직 확정하지 않음.','',
            '검사: campaign-revision 선택 경로 및 통합 /W4 /WX 빌드. 아래 수치는 모의 조건/ABI 검사이며 실제 턴 진행이 아님.']
        for name in ['native_fixture','sortie_fixture','merit_native_fixture','recruitment_ui_fixture','personnel_native_fixture','awards_native_fixture']:
            result=read_json(PROJECT/'build'/f'{name}.json')
            lines.append('- '+name+': '+str(result.get('status'))+' / '+str(result.get('checks'))+'개')
        lines += ['','공유 적용/원복: '+str(share_check.get('checks'))+'개, 최신 ZIP 검사 일치: '+str(share_check.get('zip_sha256')==share.get('zip_sha256')),
            '실전 미확인: 수정 DLL 시작·방열/왕광 도움/중계·담당관 자동점령/병참·사기100 회복·실제 동시 공성·원군 출진/합류·방어 버프 중첩·장기 데모/재시작.',
            '설치 상태: '+('최신 후보 설치·Apply·공유 일치. 실제 시작/턴 진행 미확인.' if promoted else '최신 후보/공유 준비, 게임 종료 뒤 guarded installer로 교체 필요.'),
            '준비 tools/prepare_campaign_support.py; 설치 tools/install_recruitment_ui_candidate.py; 파일 검증 tools/verify_campaign_support.py.',
            'v10.4 baseline/백업: backup/recruitment_ui_before_campaign_support_20261011. 다음 릴리스는 고유 baseline/증거/백업 갱신.',
            '관련 src/native_campaign{,_routes,_support,_logistics,_reinforcements}.inl, native_sortie.inl. 근거 research/campaign/사기_집결_병참지원.md.',
            '전체 변경표 [CHANGES_ALL](../docs/CHANGES_ALL.md). 함수 docs/NATIVE_FUNCTIONS.md; registry development/features.json.',
            '원격 함수 호출·실행 게임 쓰기 금지. 기존 UI/AI 스레드만. 기본v7 빌드/레거시 친애500명 설치 금지.']
        folder=PROJECT/'development';folder.mkdir(exist_ok=True)
        (folder/'CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
        print(json.dumps({'handoff':str(folder/'CURRENT.md'),'installed_files_match':matches,'latest_promoted':promoted,'registered_sources':len(set(paths))}))
        return
    lines=[
        '# 새 대화용 현재 상태',
        '',
        '생성: '+datetime.now(timezone(timedelta(hours=9))).isoformat(timespec='seconds'),
        '게임 폴더: `'+str(GAME)+'`',
        '설치 DLL SHA256: `'+str(actual)+'`',
        '설치 기록/Apply 패키지/manifest 일치: '+str(matches),
        '원본 EXE SHA256: `'+str(digest(GAME/'SAN14CE_KO.exe'))+'`',
        '공유 ZIP: `'+str(share.get('zip','없음'))+'`',
        '',
        '통합 배포 버전: '+manifest.get('release_version','이전 기록')+' — AI·자동내정 + 논공 친애 + 등용 UI·숨은 충성도'+(' + 컴퓨터/위임군단 등용 제한' if manifest.get('ai_recruitment') else '')+(' + 부상 보호·본초학' if injury else '')+(' + 명품창고·6슬롯·AI 배분·외교·활동 중 수여' if awards else '')+(' + 군세·중계1~5·후열지원·공성8,000.' if sortie else '.'),
        '최신 설치일: '+str(install.get('time','미기록')),
        '중립 장애물: '+('v9 10턴 왕복 / 실제 안행·누선 피격; 건설 상한 조건 제거. 실전 미검증.' if manifest.get('neutral_obstacles') else '이전 정책'),
        '관련 자료: research/v9/중립장애물_파괴조건.md; src/native_obstacles.inl, obstacle_policy.h.',
        *(['등용 요인: 상대군주 친애·충성100↑ 불가 / 100미만 친애는 주의. 현 등용 군주 사관금지는 가족 예외 없이 불가.',
           '사관금지 +162 u16 군주번호, 기간 +164 u8. 원본은 기간0도 군주번호 일치 시 차단.',
           '실전 읽기: 허정 충성97·상대군주 친애, 원본 판정 바이트 일치. 이번 저장의 사관금지 사례는 없음.',
           '새 표시 화면·실제 등용·금지 발생/만료·컴퓨터 파견은 미확인. 근거: research/v8/등용가능성_친애와사관금지.md.',
           '설치 준비: '+('tools/prepare_campaign_revision.py.' if manifest.get('campaign_revision') else 'tools/prepare_recruitment_alert.py.' if manifest.get('player_recruitment_alert') else 'tools/prepare_sortie.py.' if sortie else 'tools/prepare_office_picker_fix.py.' if office_fix else 'tools/prepare_awards.py.' if awards else 'tools/prepare_injury.py.' if injury else 'tools/prepare_recruitment_ai.py.' if manifest.get('ai_recruitment') else 'tools/prepare_recruitment_factors.py.')+' 과거 준비 스크립트는 현재 릴리스에 사용하지 말 것.'] if manifest.get('recruitment_factors') else []),
        *(['컴퓨터/위임군단: 후보 7B360·명령 2CC50의 AI 슬롯만 연결. 플레이어 직접 등용/공통 전달 함수는 그대로.',
           'AI 군주 혐오는 가족 실행무장도 불가. 충성100↑·현군주 사관금지 불가, 친애·충성100미만 주의 허용.',
           '타세력 가족은 원본 검사를 통과하는 활동 가능한 가족 실행무장 예외. 파견 직전 다시 검사'+('하며 부상 실행무장도 제외.' if injury else '; 부상 규칙 추가 없음.'),
           '실제 컴퓨터/군단 차단 로그·파견·명령서·재시작 연결·직접등용은 미확인. research/v8/컴퓨터군단_등용시도제한.md.'] if manifest.get('ai_recruitment') else []),
        *(['플레이어 인재 느낌표: 현재 군주 혐오 재야/미발견 무장을 알림 집계에서 제외. 유효 대상·명품 탐색 알림 유지.',
           '표시 전용 CALL 3곳; 실제 탐색/수동 등용·AI·관계 데이터 유지. UI1283·공통374·논공6236·통합 빌드 통과.',
           '설치 전 실행 중 원본 바이트 일치. 당시 군주952 혐오 사례 없음. 새 DLL 시작·혐오만/혼합·턴/재시작 실제 화면 미확인.',
           '근거 research/v8/플레이어_등용느낌표_군주혐오.md; 이전 DLL backup/recruitment_ui_before_alert_20261011.'] if manifest.get('player_recruitment_alert') else []),
        '기존 버튼 첫 표시·취소 후 상태는 사용자 확인. 최신 결과 안내창의 실제 화면은 아직 미확인.',
        '논공 3단계 실제 수여·전투·저장 재시작 검증도 남아 있다.',
        *(['부상 보호: 컴퓨터·위임군단의 건강(+161)=0인 무장만 출진/수송/탐색/등용/내정/군정/계략 후보·명령에 허용. 플레이어 직접 명령 유지.',
           '연결: 검증된 AI 슬롯 14개 + AI 전용 호출 2개. 기존 군정·지역개발·첩보 배치도 도시 처리에서 해제.',
           '본초학41: 자세력 부상 무장이 있으면 인사 시정에 추가. 수군육성39·선종외시50 보호; 건강한 본초학 보유 무장과 자리 필요(군주·군사 제외).',
           '부상이 모두 회복되면 원본 AI의 시정 재편 허용. 이미 발행된 명령·출진 중 부대는 취소하지 않음.',
           '인사1030개·UI1197개·AI공통374개·논공6236개 및 통합 빌드 통과. 호출 브리지 실행·부분 설치 롤백 검사 포함.',
           '게임 종료 확인 후 설치 완료. 새 DLL 시작·부상 세이브 한 턴·본초학 교체·직접 명령은 실전 미확인.',
           '근거: research/injury/부상보호_본초학.md; src/native_injury.inl, native_injury_links.inl.'] if injury else []),
        *(['명품/관직: 활동 중 순수 4능력 명품은 같은 슬롯보다 초과, 빈 슬롯은 양수; 관직 지휘병사는 이상. 정지 무장은 하향 교체 가능.',
           '출진 중 관직 없음 및 출진 무장 관직 이동 차단. 모두 해임도 최종 배치에서 보호.',
           '통솔·무력·지력·정치·수명·명마 각 1개, 충성용 중복 허용. 교체품·탐색·포로 획득·회수품은 세력창고로.',
           '수여창은 창고/착용품 통합 관리 및 기존 군주 대상 회수. 외교 목록은 미배분 창고품만. 군주 창고품의 착용 효과 제외.',
           '컴퓨터/위임군단 하루 1회: 능력별 높은 무장 순, 명마·수명은 통솔 순. 원본 명령서 비용 유지.',
           '창고 저장: data/merit/<native_name>.<archive_hash>.warehouse. 다른 PC로 저장을 옮기면 보조파일도 필요. 누락 시 군주 보유품 창고 초기화.',
           '명품/관직 '+str(read_json(PROJECT/"build/awards_native_fixture.json").get('checks'))+'개·기존 공통/논공/등용 검사 및 통합 빌드 통과. 부분 게시 롤백·실제 trampoline 실행·보조파일 IO 포함.',
           *(['사용자 확인: 이전 설치의 명품은 의도대로 작동. 개별 포로/탐색·외교·AI·저장 재시작 사례는 미확인.',
              '관직 후속 수정: 74B120 실제 선택창 비활성 목록에 활동 중 지휘병사 이상 조건 적용. Post 벡터는 인코딩값이며 +58 DB30으로 복호화(+34 평문 fallback).',
              '읽기 전용 기존 저장: 간옹 관직7000·조인15000; 출진15000/7000→20000, 동급20000→20000 허용·하향 차단을 모의 검사.',
              '새 관직 수정 DLL 시작·조인/간옹 실제 선택/수여·모두 해임은 미확인. 이전 DLL 백업 backup/recruitment_ui_before_office_picker_20261011.'] if office_fix else [
              '새 DLL 시작·실제 수여/회수 화면(군주 대상 원본 회수 문구 포함)·교역/지역 배치·관직 모두 해임·AI 한 턴·포로/탐색·외교·재시작 저장/로드는 미확인.']),
           '근거: research/awards/명품창고_활동중수여.md. src/native_awards*.inl, award_policy.h; 검사 이름 awards.'] if awards else []),
        *(['군세: 컴퓨터·위임 군단만. 원본 수비 병력 제외 4만 이상·지휘한도/명령서/금 확보 시 2~7부대. 어린/방원 선두 추가 배분 2.5배, 가능한 안행 후열.',
           '각 부대 중계1~5에 연속 인접 헥스를 함께 등록. 현 지형 원본 속도 중 최저값·선행 부대 중계 대기. 고정 3칸/턴 제한 없음; 도착 후 같은 날 다음 경로 가능.',
           '공동 지역점령·요새4/군악대7/투석대6 단일 건설+호위. 활성 응원96·경국94·악주23 여성 후열1,000명, 소규모 원본 공격에도 가용 지원 추가.',
           '도시 공성진 우선순위 최하위, 관문 최종 목표는 원본 진형 선택 유지. 컴퓨터/군단 공성8/9/10은 관문도 최대8,000. 직접 출진 제외.',
           '군세 '+str(read_json(PROJECT/"build/sortie_fixture.json").get('checks'))+'개·관련 공통/논공/UI/인사 검사·통합 /WX 통과. 8개 훅/7인자 탐색 ABI 실행·부분 게시 원복 포함.',
           '새 DLL 시작·실전 대열/지형/전투/점령/건설/버프/퇴각/저장 재시작 미확인. 좁은 지형 20일 진행 없으면 원본 복귀. 재시작 시 4만 미만 또는 건설 귀환 그룹은 원본 복귀 가능.',
           '근거 research/sortie/군세_출진과공동행동.md; src/native_sortie*.inl, sortie_policy.h; 검사 sortie. 이전 DLL backup/recruitment_ui_before_sortie_20261011.'] if sortie else []),
        '미설치: '+('' if injury else '전반 부상 보호·본초학, ')+('' if awards else '명품창고/배분/외교, 활동 중 수여, ')+'나머지 전체 인사 후보 기능.',
        '공유 설치/복원: '+str(share_check.get('status','미검사'))+' / '+str(share_check.get('checks','없음'))+'개; 현재 ZIP 검사 일치: '+str(bool(share_check.get('zip_sha256') and share_check.get('zip_sha256')==share.get('zip_sha256'))),
        '',
        '기능 지도: docs/MODULES.md. 함수 재사용: docs/NATIVE_FUNCTIONS.md.',
        '다른 모드 참고: docs/REFERENCE_MODS.md → 관련 요약 하나만. tools/reference_index.py로 원본을 제한 검색.',
        '원하는 기능만 읽고 tools/check_feature.ps1 -Feature <name> -List로 검사 경로 선택.',
        '통합 개발 빌드: 같은 명령에 -Build; build/development 출력. 설치/공유를 바꾸지 않음.',
        '현재 baseline/백업/검사 증거는 새 release에 맞춰 갱신해야 함. 기본 v7 빌드로 되돌리지 말 것.',
        '일반 친애 500명 후보는 사용하지 않음; 기존 8칸 + 별도 논공 관계를 유지.',
        '명령은 기존 UI/AI 스레드에서만. 원격 함수 호출·실행 메모리 쓰기 금지.',
        '',
        '최신 fixture 결과:'
    ]
    for name in ['native_fixture','recruitment_ui_fixture','personnel_native_fixture','merit_native_fixture']+(['awards_native_fixture'] if awards else [])+(['sortie_fixture'] if sortie else []):
        result=read_json(PROJECT/'build'/f'{name}.json')
        lines.append('- '+name+': '+str(result.get('status'))+' / '+str(result.get('checks'))+'개')
    if manifest.get('campaign_revision'):
        lines=lines[:12]+[
            '',
            '최신 변경: 지역 인접선 중계·병참 보호·피격 대응·개별 정지 퇴각·명품 배분·열전 스크롤·인구4배.',
            '- 전략 성 목표 유지. A-C-B 인접 그래프 순차 점령; 중간 적 지역에 실제 보호 부대가 있을 때만 개별 부대 생략 허용.',
            '- 보호 부대는 점령 완료까지 타일 유지. 교전/퇴각/정체성·목표 변경 시 해제; 부재하면 안전 경로 대기/재검토.',
            '- 4만 이상 군세는 출발 성→목표 성의 지역 인접선을 따라 순차 점령·공동 건설. 도로 우회 및 20일 정체 후 성 직행 제거.',
            '- 비공성 점령·건설 접근·지원·대열 배치 중계는 적 성2헥스 이내 회피. 접근할 안전 경로가 없으면 대기.',
            '- 실제 피격(병력 감소) 후 가까운 살아 있는 적 부대 우선. 전투 종료 뒤 기존 거점/성 목표 복귀.',
            '- 병력 부족 퇴각은1,000명 미만. 1,000~2,999명 군세 구성원은 유지. 개인 같은 헥스 연속5턴(50일)은 무조건 퇴각.',
            '- 목표 변경만으로 정지 이력 초기화하지 않음. 이동/부대·지휘관·세력·월드 변경/날짜 역행 시 재관찰.',
            '- 명품은 세력 전체 능력순 배분. 군주 군단 자원으로 다른 군단·활동 중 순수4능력 상향 수여. 더 강한 기존 장비 하향 교체 방지.',
            '- 열전 휠/PageUp/PageDown/Home/End. 원문 전체 보관, UI 스레드에서900자 페이지 갱신. 일반 친애8칸 유지.',
            *(['- v10.1 열전 입력 수정: 실제 게임 창 스레드에 훅 연결, 스레드 간 잠금·포그라운드/앞쪽 창 검사. 반복키/키업도 목록 전달 차단. 9행 페이지·위치 표시·색상/서로게이트 보존. 실제 별도 Win32 메시지 스레드 검사 통과, 새 게임 화면은 미확인.'] if manifest.get('biography_scroll_fix') else ['- 열전 실전 입력 문제: PageDown이 무장 목록으로 전달됨. 근거 research/campaign/열전_실전입력문제.md.'] if (PROJECT/'research/campaign/열전_실전입력문제.md').exists() else []),
            '- 실제 월간 문화/치안/난수 포함 인구 양의 증가분4배. 감소와60,000 상한 유지, 모든 세력 도시 소속 지역.',
            '',
            '기존 통합 기능 유지: 중립 장애물·시설/내정·논공 친애/저장·등용 UI·숨은 충성·AI/위임등용 제한·부상/본초학·명품창고/관직·군세/후열지원/공성8,000·플레이어 등용 느낌표.',
            '전체 변경표: [CHANGES_ALL](../docs/CHANGES_ALL.md). v8 이름의 이전 ZIP에는 당시 v10 DLL이 들어 있었다. 현재 공유 파일명은 manifest의 통합 배포 버전을 따른다.',
            '이번 릴리스 준비: '+('tools/prepare_crash_order_fix.py. 백업: backup/recruitment_ui_before_crash_order_fix_20261011.' if manifest.get('combat_order_fix') else 'tools/prepare_recruitment_button.py. 백업: backup/recruitment_ui_before_button_move_20261011.' if manifest.get('recruitment_button_layout') else 'tools/prepare_distribution_fix.py. 백업: backup/recruitment_ui_before_distribution_fix_20261011.' if manifest.get('distribution_compatibility_revision') else 'tools/prepare_biography_scroll.py. 백업: backup/recruitment_ui_before_biography_scroll_20261011.' if manifest.get('biography_scroll_fix') else 'tools/prepare_campaign_revision.py. 백업: backup/recruitment_ui_before_campaign_20261011.')+' 다음 릴리스에는 baseline·증거·고유 백업을 새로 갱신할 것.',
            '검사 선택: tools/check_feature.ps1 -Feature campaign-revision -List; 실행/통합 개발 빌드는 -Build.',
            '근거/ABI: research/campaign/경로_병참_교전_인구_열전.md. 새 함수/기계어는 이 자료 우선 재사용.',
            '코드 지도: docs/MODULES.md; docs/NATIVE_FUNCTIONS.md; development/features.json.',
            '관련 코드: native_campaign/routes, native_obstacles, native_sortie, native_awards, native_population, native_merit/scroll/links.',
            '',
            '검증: 관련 모의 검사 및 통합 /W4 /WX 빌드 통과. 모의 월드/훅 실패 원복/실제 사설 trampoline ABI 포함.',
            '실전 미확인: 새 DLL 시작, 시상→남성→건안 군세, 담당관 자동점령 억제·병참, 성 사거리 회피, 교전/퇴각, 명품/명령서, 열전 입력/줄바꿈, 월간 인구, 저장 재시작.',
            '임시 부대 경로·교전·정지 이력은 저장 재시작 후 재관찰. 인접 그래프 최단 경로가 막히면 다른 지역 우회보다 안전 대기.',
            '공유 설치/원복: '+str(share_check.get('status'))+' / '+str(share_check.get('checks'))+'개; ZIP 검사 해시 일치: '+str(share_check.get('zip_sha256')==share.get('zip_sha256')),
            '',
            '최신 fixture 결과:'
        ]
        for name in ['native_fixture','sortie_fixture','merit_native_fixture','recruitment_ui_fixture','personnel_native_fixture','awards_native_fixture']:
            result=read_json(PROJECT/'build'/f'{name}.json')
            lines.append('- '+name+': '+str(result.get('status'))+' / '+str(result.get('checks'))+'개')
        lines+=['','명령은 기존 UI/AI 스레드에서만. 원격 함수 호출·실행 게임 메모리 쓰기 금지. 기본v7 빌드로 되돌리지 말 것.']
    compatibility_check=read_json(PROJECT/'build/compatibility_diagnostic_verification.json')
    if compatibility_check.get('zip_sha256')==share.get('zip_sha256') and compatibility_check.get('status'):
        lines += ['', 'v10.2 배포: EXE 전체 SHA는 진단용. 참고 네 설치기의 PE machine/magic/timestamp/ImageSize 검사로 변경. 같은 빌드 파일 차이 허용, 롤백은 별도 이식 필요.',
                  'AI모드_호환성진단.cmd: EXE 해시/PE 지문/Steam buildid/모드 로그/최근 Windows 충돌 기록을 JSON으로 수집. 설치 실패 시에도 자동 진단. 게임 파일·세이브 변경 없음.',
                  '진단 검사: '+str(compatibility_check.get('checks'))+'개 통과. 경로 이동·한글/공백·원본/변조/잘못된/없는 EXE·미지원 차단·기존 DLL 보존. 통합 런타임 fixture 및 /W4 /WX 빌드 재검사 완료.',
                  '참고 모드 근거: research/compatibility/배포_EXE지문과참고모드.md. 새 빌드는 함수/가드/ABI/객체 재검증 후 이식하며 해시만 추가하지 않음.',
                  '월간 실행 relay 캐시 갱신3040→4096 누락 수정. 첫 예외3건 코드/RVA/phase 기록(계속 검색, 예외 복구 없음). 제보된3~6턴 종료 직접 원인·안정성은 미확인. 수신자 로그/이벤트 필요.']
    candidate=read_json(PROJECT/'build/recruitment_ui_candidate.json')
    if candidate.get('distribution_compatibility_revision'):
        promoted=actual==candidate.get('candidate_sha256')==payload==share.get('dll_sha256')
        lines += ['', '최신 준비/공유 버전: '+candidate.get('release_version','미기록')+' 시험판. DLL SHA256: '+str(candidate.get('candidate_sha256')),
                  candidate.get('release_version','후보')+' 설치/Apply/공유 DLL 일치: '+str(promoted)+'. '+('파일 교체 완료. 시작/수 턴 진행 미검증.' if promoted else '현재 실행 게임/설치 DLL은 이전 판. 게임 종료 뒤 tools/install_recruitment_ui_candidate.py 실행 필요. 개발 후보·공유 ZIP 준비 완료.'),
                  'v10.2 준비: tools/prepare_distribution_fix.py; 전용 백업 backup/recruitment_ui_before_distribution_fix_20261011.',
                  'v10.2에서 캐시 갱신 범위·첫 예외 진단 보완. 당시 통합 fixture10,332개 통과. 후속 변경 검사는 아래 최신 항목 참조.',
                  '로컬 C0000005/3CC37 종료 경로와 연결되는 부대 공격 명령 타입 오류 수정. 수신자 동일 원인과 수정 후 안정성은 미확인.' if candidate.get('combat_order_fix') else '반복 종료 원인 미확정. 안정화 완료로 배포하지 않음.']
    if candidate.get('recruitment_button_layout') and not candidate.get('combat_order_fix'):
        lines += ['', 'v10.3 자동등용 버튼: X192→142(왼쪽50 이동), 크기240×48·높이 위치 유지. 군주 조언 겹침 제보 대응.',
                  '선택 검사 recruitment1,283개·통합 /W4 /WX 빌드 통과. 버튼 새 위치 실제 화면은 미확인. 다른 모듈은 기존 검사 근거 유지.',
                  '준비 tools/prepare_recruitment_button.py; 백업 backup/recruitment_ui_before_button_move_20261011.']
    if candidate.get('combat_order_fix'):
        lines += ['', 'v10.4: 부대 공격 action4/kind27 오류를 원본 action7/kind27로 수정. 로컬 fatal C0000005/RVA3CC37 도시 처리 경로와 연결되는 오류.',
                  '수신 로그 두 개는 예외 위치 기록이 없어 동일 원인 확정 불가. 근거 research/campaign/종료_3CC37_부대공격명령.md.',
                  '자동등용: 본문 아래쪽 오른쪽 명령서 행 옆 빈 공간(width-260,height-184),240×48. 조언 위/부모 안쪽.',
                  '통합 fixture10,337개·/W4 /WX 빌드 통과. 수정 후 실제 데모 진행·버튼 배치/클릭 미확인.',
                  '준비 tools/prepare_crash_order_fix.py; 백업 backup/recruitment_ui_before_crash_order_fix_20261011.']
    lines += ['', '현재 로컬 적용 상태: '+('설치됨' if state.get('installed') else '해제됨')+'. 실제 입력 DLL SHA256: '+str(actual)+'.']
    folder=PROJECT/'development';folder.mkdir(exist_ok=True)
    (folder/'CURRENT.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print(json.dumps({'handoff':str(folder/'CURRENT.md'),'installed_files_match':matches,
                      'registered_sources':len(set(paths))},ensure_ascii=True))

if __name__=='__main__':main()
