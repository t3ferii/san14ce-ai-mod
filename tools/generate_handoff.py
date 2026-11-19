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
        '| 설치/Apply/manifest DLL 해시 | '+str(pair and recorded)+' |',
        '| 해당 Apply 버전 ZIP/고유 release proof 해시 | '+str(installed_zip_matches)+' |',
        '| 최신 공유 ZIP | '+share_version+' |',
        '| 최신 ZIP/share_package/share_verification 해시 | '+str(share_matches)+' |',
        '| 원본 EXE/manifest 기준 해시 | '+str(digest(GAME/'SAN14CE_KO.exe') in set([manifest.get('supported_original_exe_sha256'),*manifest.get('supported_original_exe_sha256_all',[])]))+' |','',
        '설치 기록과 Apply 버전이 다르면 준비/승격 중 상태일 수 있다. 해시가 같아도 설치 완료로 추정하지 않는다.','',
        '## 현재 기능과 검사 기록','',
        '- DLL '+str(len(manifest['split_modules']['files']))+'개(로더·Layout·전투·편의·조조전·ModPack). 설치기 기능 '+str(manifest.get('feature_switches',{}).get('count','미기록'))+'개, 종속 키는 부모와 AND.',
        '- 최신 공유 ZIP 검사 기록: '+str(share_check.get('checks'))+'개 ('+str(share_check.get('status'))+'). 이번 생성에서 재실행하지 않음.',
        '- 사용자 설정 보존(해당 release proof 기록): '+str(proof.get('settings_preserved'))+'.','',
        '## 실게임 확인 범위','',
        '- 위 해시 일치는 파일 확인이다. 기능별 실게임 확인 상태는 development/USER_REQUESTS.md에 있다.',
        '- Computer Use 금지. 게임 실행·입력·화면 관찰은 사용자 직접 수행.','',
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
        lines.insert(lines.index('## 실게임 확인 범위')-1,
            '- GUI 설치기 '+gui['file']+' / manifest 해시 일치: '+str(digest(GAME/gui['file'])==gui.get('sha256'))+'.')
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
