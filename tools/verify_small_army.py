"""Verify small-army release files; never launch or modify the game."""
import json, hashlib, zipfile
from pathlib import Path
from prepare_small_army import BASELINE, ROUTES
from package_mod import ORIGINAL_SHA
from package_input import PROJECT as p, GAME as game, digest

def load(path):return json.loads(path.read_text(encoding='utf-8'))

def main():
    proof=load(p/'build/recruitment_ui_candidate.json');manifest=load(p/'build/input_package/manifest.json')
    state=load(p/'backup/input_state.json');share=load(p/'build/share_package.json');launch=load(p/'build/share_verification.json')
    assert proof['release_version']==manifest['release_version']==share['release_version']=='v11.1'
    assert state['installed'] and state['installed_sha256']==proof['candidate_sha256']==manifest['dll_sha256']==share['dll_sha256']==digest(game/'dinput8.dll')==digest(p/'build/input_package/dinput8.dll')
    assert launch['status']=='share_windows_launchers_verified' and launch['checks']>=55
    assert digest(Path(share['zip']))==share['zip_sha256']==launch['zip_sha256']
    assert manifest['small_army_campaign']==proof['small_army_campaign']
    assert manifest['cohesive_sortie']['minimum_mobilizable'] is None
    assert manifest['campaign_support_revision']['target_owned_adjacent_bases_policy']['3_or_more']=='regional_capture'
    assert manifest['combat_order_fix']['army_attack_action']==7
    assert digest(game/'SAN14CE_KO.exe')==ORIGINAL_SHA
    assert digest(game/'SAN14_AI_PreviousInput.dll')==state['previous_sha256']
    backup=p/'backup/recruitment_ui_before_small_army_20261012'
    assert digest(backup/'dinput8.dll')==digest(backup/'package_dinput8.dll')==BASELINE==proof['installed_input_sha256']
    for group in ['recruitment_source_sha256','obstacle_source_sha256']:
        for name,expected in proof[group].items():assert digest(p/name)==expected,name
    total=0
    for key,name,status,minimum in ROUTES:
        result=load(p/'build'/f'{name}.json');assert result==proof[key]
        assert result['status']==status and result['checks']>=minimum
        total+=result['checks']
    assert total>=12477
    with zipfile.ZipFile(share['zip']) as z:
        for name in ['dinput8.dll','manifest.json']:
            assert z.read('AI_TacticsMod/build/input_package/'+name)==(p/'build/input_package'/name).read_bytes()
        assert all(not name.endswith('.exe') for name in z.namelist())
    result={'status':'small_army_release_files_verified','release_version':'v11.1',
        'dll_sha256':share['dll_sha256'],'zip_sha256':share['zip_sha256'],
        'fixture_checks':total,'launcher_checks':launch['checks'],'installed_apply_zip_match':True,
        'baseline_backup_verified':True,'original_exe_and_input_provider_preserved':True,
        'startup_verified':bool(manifest['small_army_campaign'].get('startup_verified')),'gameplay_verified':False}
    (p/'build/small_army_release_verification.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
