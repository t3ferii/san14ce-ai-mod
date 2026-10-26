"""Verify the final ZIP and candidate, with installation status reported separately."""
import hashlib,json,zipfile
from pathlib import Path
project=Path(__file__).resolve().parents[1];game=project.parent
def load(path):return json.loads(path.read_text(encoding='utf-8'))
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None
proof=load(project/'build/recruitment_ui_candidate.json');share=load(project/'build/share_package.json')
launch=load(project/'build/share_verification.json');compat=load(project/'build/compatibility_diagnostic_verification.json')
assert proof['release_version']==share['release_version']=='v10.2'
assert proof['candidate_sha256']==share['dll_sha256']==digest(project/'build/SAN14_AIInput_recruitment_ui_candidate.dll')
assert digest(Path(share['zip']))==share['zip_sha256']==launch['zip_sha256']==compat['zip_sha256']
assert launch['checks']==55 and compat['checks']==41
assert compat['same_build_different_hash_installs_and_restores'] and compat['unsupported_pe_still_blocked'] and compat['external_source_selected_game_tested']
for group in ['recruitment_source_sha256','obstacle_source_sha256']:
    for name,expected in proof[group].items():assert digest(project/name)==expected,name
with zipfile.ZipFile(share['zip']) as archive:
    manifest=json.loads(archive.read('AI_TacticsMod/build/input_package/manifest.json'))
    assert manifest['release_version']=='v10.2'
    assert hashlib.sha256(archive.read('AI_TacticsMod/build/input_package/dinput8.dll')).hexdigest()==share['dll_sha256']==manifest['dll_sha256']
    for name,source in [('AI_TacticsMod/관리.ps1','distribution/Manage.ps1'),('AI_TacticsMod/진단.ps1','distribution/Diagnose.ps1')]:
        packed=archive.read(name)
        assert packed.decode('utf-8-sig').replace('\r\n','\n')==(project/source).read_text(encoding='utf-8-sig')
        (project/Path(name).name).write_bytes(packed)
    readme=archive.read('읽어주세요.txt').decode('utf-8-sig')
    assert '통합 v10.2' in readme and '반복 종료 실전 검증 전 시험판' in readme
state=load(project/'backup/input_state.json')
actual=digest(game/'dinput8.dll');payload=digest(project/'build/input_package/dinput8.dll')
installed=state['installed'] and actual==state['installed_sha256']==payload==share['dll_sha256']
assert actual==(state['installed_sha256'] if state['installed'] else state['previous_sha256'])
assert digest(game/'SAN14_AI_PreviousInput.dll')==state['previous_sha256']
assert digest(game/'SAN14CE_KO.exe')==proof['original_exe_sha256']
if installed:
    with zipfile.ZipFile(share['zip']) as archive:
        assert archive.read('AI_TacticsMod/build/input_package/manifest.json')==(project/'build/input_package/manifest.json').read_bytes()
    backup=project/'backup/recruitment_ui_before_distribution_fix_20261011'
    assert digest(backup/'dinput8.dll')==proof['installed_input_sha256']
record={'status':'distribution_v10_2_files_verified','release_version':'v10.2','dll_sha256':share['dll_sha256'],
        'zip_sha256':share['zip_sha256'],'installed_apply_zip_match':bool(installed),'actual_local_dll_sha256':actual,
        'launcher_checks':55,'compatibility_checks':41,'runtime_fixture_checks':10332,
        'original_exe_unchanged':True,'previous_input_preserved':True,'recipient_gameplay_verified':False,
        'crash_cause_identified':False,'stability_verified':False}
(project/'build/distribution_fix_verification.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(json.dumps(record,indent=2))
