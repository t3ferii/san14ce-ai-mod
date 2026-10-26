"""Install the authorized v11.7 release after backup, source and ZIP verification.

Uses the same packaged PowerShell manager as the user-facing CMD. Never starts
the game or modifies its executable. The preceding v11.6 snapshot is required.
"""
from pathlib import Path
from datetime import datetime, timezone
import hashlib, json, subprocess, zipfile
from package_mod import atomic_write, no_game, ORIGINAL_SHA

PROJECT = Path(__file__).resolve().parents[1]
GAME = PROJECT.parent

def load(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None

def main():
    no_game()
    backup = Path(load(PROJECT/'build/loader_split_backup.json')['backup']).resolve()
    assert backup.is_relative_to((PROJECT/'backup').resolve())
    snapshot = load(backup/'snapshot.json')
    assert snapshot['status'] == 'pre_loader_split_backup_verified'
    original_files = {row['path']: row['sha256'] for row in snapshot['files']}
    for name, expected in original_files.items():
        assert sha(backup/'files'/name) == expected, name
    assert sha(GAME/'SAN14CE_KO.exe') == ORIGINAL_SHA == snapshot['original_exe_sha256']
    prior_state=load(PROJECT/'backup/input_state.json')
    assert prior_state['installed'] and prior_state['release_version']=='v11.6'
    assert sha(PROJECT/'backup/input_state.json') == original_files['AI_TacticsMod/backup/input_state.json']
    for name in ['dinput8.dll', 'SAN14_CombatAI.dll']:
        assert sha(GAME/name)==original_files[name], 'Existing installation changed: '+name
    for name in ['SAN14_Convenience.dll', 'SAN14_AI_PreviousInput.dll']:
        assert not (GAME/name).exists(), 'Unexpected existing file: '+name

    stage = PROJECT/'build/split_package'
    manifest = load(stage/'manifest.json')
    assert manifest['release_version'] == 'v11.7'
    split = manifest['split_modules']
    assert split['loader_only'] and not split['behavior_rules_changed'] and split['baseline_version'] == 'v11.5'
    for name, expected in split['source_sha256'].items():
        assert sha(PROJECT/name) == expected, 'Source changed: '+name
    for name, expected in split['files'].items():
        assert sha(stage/name) == expected == sha(PROJECT/'build/development/loader_split'/name)

    share = load(PROJECT/'build/share_package.json')
    package = Path(share['zip']).resolve()
    assert package.is_relative_to((PROJECT/'dist').resolve())
    zip_hash = sha(package)
    assert zip_hash == share['zip_sha256']
    for record in ['share_verification', 'compatibility_diagnostic_verification']:
        proof = load(PROJECT/'build'/f'{record}.json')
        assert proof['checks'] > 0 and proof['zip_sha256'] == zip_hash, record
    failure=load(PROJECT/'build/split_install_failure_fixture.json')
    assert failure['checks'] > 0 and failure['fresh_and_two_dll_upgrade_rollback'] and failure['zip_sha256']==zip_hash
    binary_proof=load(PROJECT/'build/split_binary_fixture.json')
    assert binary_proof['status']=='split_binary_fixture_passed' and binary_proof['loader_only'] and binary_proof['files']==split['files']

    preserve = {name: sha(GAME/name) for name in ['SAN14CE_KO.exe', 'D3D11.dll', 'dxgi.dll']}
    names = ['AI_TacticsMod/관리.ps1', 'AI_TacticsMod/SplitManage.ps1', 'AI_TacticsMod/진단.ps1',
             'AI_TacticsMod/build/input_package/manifest.json',
             'AI_TacticsMod/build/input_package/dinput8.dll',
             'AI_TacticsMod/build/input_package/SAN14_CombatAI.dll',
             'AI_TacticsMod/build/input_package/SAN14_Convenience.dll']
    with zipfile.ZipFile(package) as archive:
        new_files = {name: archive.read(name) for name in names}
        for name in split['files']:
            assert new_files['AI_TacticsMod/build/input_package/'+name] == (stage/name).read_bytes()
        assert new_files['AI_TacticsMod/build/input_package/manifest.json'] == (stage/'manifest.json').read_bytes()

    promotion = PROJECT/'backup'/('loader_promotion_'+datetime.now().strftime('%Y%m%d_%H%M%S'))
    promotion.mkdir()
    old_files = {name: (GAME/name).read_bytes() if (GAME/name).exists() else None for name in names}
    for name, data in old_files.items():
        if data is not None:
            saved = promotion/name
            saved.parent.mkdir(parents=True, exist_ok=True)
            atomic_write(saved, data)
            assert saved.read_bytes() == data
    no_game()
    try:
        for name, data in new_files.items():
            atomic_write(GAME/name, data)
        run = subprocess.run(['cmd.exe', '/d', '/c', 'call', '.\\AI모드_적용.cmd', '--no-pause'],
                             cwd=GAME, capture_output=True, timeout=40)
        print(run.stdout.decode('utf-8', errors='replace'))
        if run.returncode:
            raise RuntimeError(run.stderr.decode('utf-8', errors='replace') or 'Packaged installer failed')
    except Exception:
        for name, data in old_files.items():
            if data is None:
                (GAME/name).unlink(missing_ok=True)
            else:
                atomic_write(GAME/name, data)
        raise

    state = load(PROJECT/'backup/input_state.json')
    assert state['installed'] and state['release_version'] == 'v11.7'
    assert state['previous_sha256'] is None
    assert state['installed_sha256'] == split['files']['dinput8.dll']
    assert state['modules']['SAN14_CombatAI.dll'] == split['files']['SAN14_CombatAI.dll']
    assert state['modules']['SAN14_Convenience.dll'] == split['files']['SAN14_Convenience.dll']
    assert all(sha(GAME/name) == expected for name, expected in split['files'].items())
    assert all(sha(GAME/name) == expected for name, expected in preserve.items())
    record = dict(status='split_release_installed', release_version='v11.7', loader_only=True,
                  files=split['files'], promotion_backup=str(promotion),
                  install_backup=state['install_backup'], source_backup=str(backup),
                  original_files_unchanged=preserve, zip_sha256=zip_hash,
                  file_verification=True, startup_verified=False, gameplay_verified=False,
                  installed_at=datetime.now(timezone.utc).isoformat())
    atomic_write(PROJECT/'build/dll_split_installation.json',
                 (json.dumps(record, ensure_ascii=False, indent=2)+'\n').encode('utf-8'))
    subprocess.run(['python', '-B', '-X', 'utf8', str(PROJECT/'tools/generate_handoff.py')], check=True)
    print(json.dumps({'status':record['status'], 'files':record['files'], 'gameplay_verified':False}))

if __name__ == '__main__':
    main()
