"""Verify v11.3 candidate or installed package, default-off configs and immutable backups."""
import json,sys,zipfile,configparser
from pathlib import Path
from package_input import PROJECT as p,GAME as game,digest
from package_mod import ORIGINAL_SHA

def load(path):return json.loads(path.read_text(encoding='utf-8'))
def main():
    if sys.argv[1:] not in [[],['--staged']]:raise RuntimeError('Only --staged is supported')
    staged=bool(sys.argv[1:]);package=p/('build/distribution_candidate/input_package' if staged else 'build/input_package')
    proof=load(p/'build/recruitment_ui_candidate.json');logging=proof['progress_log'];native=proof['progress_native']
    state=load(p/'backup/input_state.json');manifest=load(package/'manifest.json');share=load(p/'build/share_package.json');launch=load(p/'build/share_verification.json')
    assert proof['release_version']==manifest['release_version']==share['release_version']=='v11.3'
    assert proof['candidate_sha256']==manifest['dll_sha256']==share['dll_sha256']==digest(package/'dinput8.dll')
    assert digest(game/'SAN14CE_KO.exe')==ORIGINAL_SHA and digest(game/'SAN14_AI_PreviousInput.dll')==state['previous_sha256']
    assert state['installed'] and state['installed_sha256']==digest(game/'dinput8.dll')==(proof['installed_input_sha256'] if staged else proof['candidate_sha256'])
    assert manifest['progress_log']==logging and manifest['small_army_campaign']==proof['small_army_campaign']
    for group in ['recruitment_source_sha256','obstacle_source_sha256']:
        for name,expected in proof[group].items():assert digest(p/name)==expected,name
    assert launch['status']=='share_windows_launchers_verified' and launch['checks']>=59
    assert digest(Path(share['zip']))==share['zip_sha256']==launch['zip_sha256']
    with zipfile.ZipFile(share['zip']) as archive:
        for name in ['dinput8.dll','manifest.json']:assert archive.read('AI_TacticsMod/build/input_package/'+name)==(package/name).read_bytes()
        for name,section,key in [('diagnostics.ini','diagnostics','enabled'),('progress_native.ini','progress_native','profile')]:
            data=archive.read('AI_TacticsMod/'+name).decode('utf-8');ini=configparser.ConfigParser();ini.read_string(data);assert ini.getint(section,key)==0
            assert data.splitlines()==(p/'distribution'/name).read_text(encoding='utf-8').splitlines()
            assert data.splitlines()==(package/name if staged else p/name).read_text(encoding='utf-8').splitlines()
        assert all(not name.endswith(('.exe','.etl','.csv','.meritbond','.warehouse')) and '/backup/' not in name for name in archive.namelist())
    folder=p/logging['backup_snapshot'];snapshot=load(folder/'snapshot.json')
    for row in snapshot['files']:assert digest(folder/'files'/row['path'])==row['sha256'],row['path']
    assert snapshot['baseline_dll_sha256']==proof['installed_input_sha256']
    if not staged:
        backup=p/logging['install_backup'];assert digest(backup/'dinput8.dll')==digest(backup/'package_dinput8.dll')==proof['installed_input_sha256']
        saves=load(p/'build/progress_save_checkpoint.json');assert saves['status']=='closed_game_save_checkpoint_verified' and Path(saves['folder']).is_relative_to(folder)
        for row in saves['files']:assert digest(Path(saves['folder'])/row['copy'])==row['sha256']
        sidecars=load(p/'build/progress_sidecar_checkpoint.json');assert Path(sidecars['folder']).is_relative_to(folder)
        for row in sidecars['files']:assert digest(Path(sidecars['folder'])/row['copy'])==row['sha256']
    total=sum(proof[key]['checks'] for key in ['native_fixture','sortie_fixture','merit_fixture','fixture','personnel_fixture','awards_fixture']);assert total==12477
    result={'status':'progress_log_candidate_files_verified' if staged else 'progress_log_release_files_verified','version':'v11.3','installed':not staged,
        'dll_sha256':share['dll_sha256'],'zip_sha256':share['zip_sha256'],'snapshot_files_verified':len(snapshot['files']),
        'existing_fixture_checks':total,'hook_and_policy_checks':native['fixtures']['progress_native_fixture']['checks'],'log_checks':logging['fixtures']['log_append_fixture']['checks'],
        'share_checks':launch['checks'],'rollback_checks':logging['rollback_fixture']['checks'],'normal_logging_enabled':False,
        'startup_verified':False,'button_latency_improvement_verified':False}
    (p/'build'/('progress_log_stage_verification.json' if staged else 'progress_log_release_verification.json')).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
