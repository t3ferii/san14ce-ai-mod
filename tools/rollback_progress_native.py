"""Guarded local rollback to v11.1; --apply requires a closed game.

Development work and saves are retained. Default verifies/prints the exact plan.
"""
import json,sys
from pathlib import Path
from package_mod import atomic_write,no_game,ORIGINAL_SHA,sha
from package_input import PROJECT,GAME,STATE,TARGET,CHAIN,digest

RESTORE=['dinput8.dll','AI_TacticsMod/build/input_package/dinput8.dll',
    'AI_TacticsMod/build/input_package/manifest.json','AI_TacticsMod/backup/input_state.json',
    'AI_TacticsMod/build/recruitment_ui_candidate.json','AI_TacticsMod/build/recruitment_ui_installation.json',
    'AI_TacticsMod/build/share_package.json','AI_TacticsMod/build/share_verification.json']
CONFIG='AI_TacticsMod/progress_native.ini'
def load(path):return json.loads(path.read_text(encoding='utf-8'))
def plan(game,folder,expected):
    game=game.resolve();folder=folder.resolve()
    if not folder.is_relative_to(game/'AI_TacticsMod/backup'):raise RuntimeError('Backup must be local')
    snapshot=load(folder/'snapshot.json');records={r['path']:r['sha256'] for r in snapshot['files']}
    for name,expected_sha in records.items():
        source=(folder/'files'/name).resolve()
        if not source.is_relative_to(folder/'files') or digest(source)!=expected_sha:raise RuntimeError('Backup changed: '+name)
    state=load(game/'AI_TacticsMod/backup/input_state.json')
    oldstate=load(folder/'files/AI_TacticsMod/backup/input_state.json')
    if not state['installed'] or not expected==digest(game/'dinput8.dll')==state['installed_sha256']:
        raise RuntimeError('Current DLL changed; refusing rollback')
    if digest(game/'SAN14CE_KO.exe')!=ORIGINAL_SHA:raise RuntimeError('Original EXE changed')
    if digest(game/'SAN14_AI_PreviousInput.dll')!=state['previous_sha256'] or state['previous_sha256']!=oldstate['previous_sha256']:
        raise RuntimeError('Preserved input provider changed')
    if snapshot['baseline_dll_sha256']!=records['dinput8.dll'] or oldstate['installed_sha256']!=records['dinput8.dll']:
        raise RuntimeError('Backup baseline is inconsistent')
    result={game/name:(folder/'files'/name).read_bytes() for name in RESTORE}
    result[game/CONFIG]=(folder/'files'/CONFIG).read_bytes() if CONFIG in records else None
    if 'diagnostics_config_existed' in snapshot:
        name='AI_TacticsMod/diagnostics.ini'
        if bool(name in records)!=snapshot['diagnostics_config_existed']:raise RuntimeError('Diagnostics presence inconsistent')
        result[game/name]=(folder/'files'/name).read_bytes() if name in records else None
    if snapshot.get('status') in {'pre_target_retreat_backup_verified','pre_field_activity_backup_verified'}:
        name='AI_TacticsMod/target_retreat.ini'
        result[game/name]=(folder/'files'/name).read_bytes() if name in records else None
    return result
def apply_plan(changes,write=atomic_write):
    prior={path:path.read_bytes() if path.exists() else None for path in changes};written=[]
    try:
        for path,data in changes.items():
            written.append(path)
            if data is None:
                if path.exists():path.unlink()
            else:write(path,data)
        if any((path.read_bytes() if path.exists() else None)!=data for path,data in changes.items()):raise RuntimeError('Rollback verification failed')
    except BaseException:
        for path in reversed(written):
            if prior[path] is None:
                if path.exists():path.unlink()
            else:atomic_write(path,prior[path])
        raise
def main():
    if sys.argv[1:] not in [[],['--apply']]:raise RuntimeError('Only --apply is supported')
    evidence=load(PROJECT/'build/recruitment_ui_candidate.json')
    native=evidence.get('field_activity',evidence.get('target_retreat',evidence.get('progress_log',evidence['progress_native'])));folder=PROJECT/native['backup_snapshot']
    changes=plan(GAME,folder,evidence['candidate_sha256'])
    if sys.argv[1:]:no_game();apply_plan(changes)
    version=load(folder/'files/AI_TacticsMod/build/input_package/manifest.json')['release_version']
    print(json.dumps({'status':version+'_restored' if sys.argv[1:] else 'rollback_plan_verified','apply':bool(sys.argv[1:]),
        'backup':str(folder),'files':[str(p) for p in changes],'saves_changed':False,'development_source_retained':True},indent=2))
if __name__=='__main__':main()
