"""Verify installed native pilot, backups and share without touching the game."""
import json,zipfile
from pathlib import Path
from package_input import PROJECT as p,GAME as game,digest
from package_mod import ORIGINAL_SHA
def load(path):return json.loads(path.read_text(encoding='utf-8'))
def main():
    proof=load(p/'build/recruitment_ui_candidate.json');native=proof['progress_native']
    state=load(p/'backup/input_state.json');manifest=load(p/'build/input_package/manifest.json')
    share=load(p/'build/share_package.json');launch=load(p/'build/share_verification.json')
    assert proof['release_version']==manifest['release_version']==share['release_version']=='v11.2'
    assert state['installed'] and proof['candidate_sha256']==state['installed_sha256']==manifest['dll_sha256']==share['dll_sha256']==digest(game/'dinput8.dll')==digest(p/'build/input_package/dinput8.dll')
    assert manifest['progress_native']==native and manifest['small_army_campaign']==proof['small_army_campaign']
    assert digest(game/'SAN14CE_KO.exe')==ORIGINAL_SHA and digest(game/'SAN14_AI_PreviousInput.dll')==state['previous_sha256']
    for group in ['recruitment_source_sha256','obstacle_source_sha256']:
        for name,expected in proof[group].items():assert digest(p/name)==expected,name
    assert launch['status']=='share_windows_launchers_verified' and launch['checks']>=55
    assert digest(Path(share['zip']))==share['zip_sha256']==launch['zip_sha256']
    with zipfile.ZipFile(share['zip']) as z:
        for name in ['dinput8.dll','manifest.json']:
            assert z.read('AI_TacticsMod/build/input_package/'+name)==(p/'build/input_package'/name).read_bytes()
        assert z.read('AI_TacticsMod/progress_native.ini').decode('utf-8').splitlines()==(p/'progress_native.ini').read_text(encoding='utf-8').splitlines()
        assert all(not name.endswith(('.exe','.etl','.csv','.meritbond')) for name in z.namelist())
    folder=p/native['backup_snapshot'];snapshot=load(folder/'snapshot.json')
    for record in snapshot['files']:assert digest(folder/'files'/record['path'])==record['sha256'],record['path']
    backup=p/native['install_backup']
    assert digest(backup/'dinput8.dll')==digest(backup/'package_dinput8.dll')==proof['installed_input_sha256']==snapshot['baseline_dll_sha256']
    saves=load(p/'build/progress_save_checkpoint.json')
    assert saves['status']=='closed_game_save_checkpoint_verified'
    for record in saves['files']:assert digest(Path(saves['folder'])/record['copy'])==record['sha256']
    sidecars=load(p/'build/progress_sidecar_checkpoint.json')
    for record in sidecars['files']:assert digest(Path(sidecars['folder'])/record['copy'])==record['sha256']
    total=sum(proof[k]['checks'] for k in ['native_fixture','sortie_fixture','merit_fixture','fixture','personnel_fixture','awards_fixture'])
    assert total==12477
    result={'status':'progress_native_release_files_verified','release_version':'v11.2','dll_sha256':share['dll_sha256'],
        'zip_sha256':share['zip_sha256'],'installed_apply_zip_match':True,'original_exe_unchanged':True,
        'snapshot_files_verified':len(snapshot['files']),'save_files_verified':len(saves['files']),
        'existing_fixture_checks':total,'native_fixture_checks':native['fixtures']['progress_native_fixture']['checks'],
        'geometry_comparisons':native['fixtures']['progress_geometry_fixture']['comparisons'],
        'native_comparisons':native['fixtures']['progress_native_fixture']['native_comparisons'],
        'rollback_checks':native['rollback_fixture']['checks'],'share_checks':launch['checks'],
        'sidecar_files_verified':len(sidecars['files']),
        'startup_verified':native.get('startup_verified',False),'gameplay_verified':False,'button_latency_improvement_verified':False}
    (p/'build/progress_native_release_verification.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
