"""Exercise rollback guards/transaction in a disposable local fixture."""
from pathlib import Path
import json,sys,tempfile,shutil
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from rollback_progress_native import plan,apply_plan,RESTORE,CONFIG
from package_input import PROJECT,GAME,digest
checks=0
def check(value):
    global checks
    assert value
    checks+=1
def reject(call):
    try:call()
    except RuntimeError:check(True)
    else:check(False)
root=Path(tempfile.mkdtemp(prefix='progress_rollback_',dir=PROJECT/'build')).resolve()
try:
    folder=root/'AI_TacticsMod/backup/pre';files=folder/'files';files.mkdir(parents=True)
    state={'installed':True,'installed_sha256':__import__('hashlib').sha256(b'old').hexdigest(),'previous_sha256':None}
    for name in RESTORE:
        source=files/name;source.parent.mkdir(parents=True,exist_ok=True)
        source.write_bytes(json.dumps(state).encode() if name.endswith('input_state.json') else b'old')
        target=root/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(b'current')
    shutil.copyfile(GAME/'SAN14CE_KO.exe',root/'SAN14CE_KO.exe')
    current=__import__('hashlib').sha256(b'current').hexdigest()
    current_state=dict(state,installed_sha256=current)
    (root/'AI_TacticsMod/backup/input_state.json').write_text(json.dumps(current_state))
    records=[{'path':name,'sha256':digest(files/name)} for name in RESTORE]
    snapshot={'files':records,'baseline_dll_sha256':state['installed_sha256'],'diagnostics_config_existed':False}
    (folder/'snapshot.json').write_text(json.dumps(snapshot))
    changes=plan(root,folder,current);check(changes[root/CONFIG] is None)
    diagnostic=root/'AI_TacticsMod/diagnostics.ini';check(changes[diagnostic] is None);diagnostic.write_bytes(b'enabled=1')
    (root/CONFIG).write_bytes(b'enabled=1')
    before={p:p.read_bytes() if p.exists() else None for p in changes};count=0
    def fail_write(path,data):
        global count
        count+=1
        if count==3:raise RuntimeError('injected transaction failure')
        path.write_bytes(data)
    reject(lambda:apply_plan(changes,fail_write))
    check(all((p.read_bytes() if p.exists() else None)==data for p,data in before.items()))
    reject(lambda:plan(root,folder,'0'*64))
    reject(lambda:plan(root,root,current))
    (root/'SAN14_AI_PreviousInput.dll').write_bytes(b'foreign')
    reject(lambda:plan(root,folder,current));(root/'SAN14_AI_PreviousInput.dll').unlink()
    source=files/RESTORE[0];source.write_bytes(b'corrupt');reject(lambda:plan(root,folder,current));source.write_bytes(b'old')
    apply_plan(changes);check((root/'dinput8.dll').read_bytes()==b'old' and not (root/CONFIG).exists())
    check(not diagnostic.exists())
    check(all(digest(root/name)==digest(files/name) for name in RESTORE))
    snapshot['status']='pre_target_retreat_backup_verified'
    (folder/'snapshot.json').write_text(json.dumps(snapshot))
    target_config=root/'AI_TacticsMod/target_retreat.ini';target_config.write_bytes(b'enabled=1')
    target_changes=plan(root,folder,state['installed_sha256'])
    check(target_changes[target_config] is None)
    apply_plan(target_changes);check(not target_config.exists())
    # The old runtime had user-authorized diagnostics ON: restore exact bytes.
    diagnostic_copy=files/'AI_TacticsMod/diagnostics.ini';diagnostic_copy.write_bytes(b'enabled=1')
    records.append({'path':'AI_TacticsMod/diagnostics.ini','sha256':digest(diagnostic_copy)})
    snapshot['diagnostics_config_existed']=True
    (folder/'snapshot.json').write_text(json.dumps(snapshot))
    diagnostic.write_bytes(b'enabled=0');target_changes=plan(root,folder,state['installed_sha256'])
    check(target_changes[diagnostic]==b'enabled=1')
    apply_plan(target_changes);check(diagnostic.read_bytes()==b'enabled=1')
    snapshot['status']='pre_field_activity_backup_verified'
    target_copy=files/'AI_TacticsMod/target_retreat.ini';target_copy.write_bytes(b'enabled=1')
    records.append({'path':'AI_TacticsMod/target_retreat.ini','sha256':digest(target_copy)})
    (folder/'snapshot.json').write_text(json.dumps(snapshot))
    target_config.write_bytes(b'enabled=0');field_changes=plan(root,folder,state['installed_sha256'])
    check(field_changes[target_config]==b'enabled=1')
    apply_plan(field_changes);check(target_config.read_bytes()==b'enabled=1')
    records.append({'path':'../../outside','sha256':None});(folder/'snapshot.json').write_text(json.dumps(snapshot))
    reject(lambda:plan(root,folder,state['installed_sha256']))
    result={'status':'progress_rollback_fixture_passed','checks':checks,'actual_installation_changed':False}
    (PROJECT/'build/progress_rollback_fixture.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result))
finally:
    assert root.is_relative_to((PROJECT/'build').resolve()) and root.name.startswith('progress_rollback_')
    shutil.rmtree(root)
