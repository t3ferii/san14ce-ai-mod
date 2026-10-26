"""Verify v10.4 files, selected regressions and unchanged installer evidence."""
import json,hashlib,zipfile
from pathlib import Path
p=Path(__file__).resolve().parents[1];game=p.parent
def load(f):return json.loads(f.read_text(encoding='utf-8'))
def sha(f):return hashlib.sha256(f.read_bytes()).hexdigest() if f.exists() else None
proof=load(p/'build/recruitment_ui_candidate.json');share=load(p/'build/share_package.json')
state=load(p/'backup/input_state.json');manifest=load(p/'build/input_package/manifest.json')
launch=load(p/'build/share_verification.json');compat=load(p/'build/compatibility_diagnostic_verification.json')
assert proof['release_version']==share['release_version']==manifest['release_version']=='v10.4'
assert state['installed'] and state['installed_sha256']==proof['candidate_sha256']==manifest['dll_sha256']==share['dll_sha256']==sha(game/'dinput8.dll')==sha(p/'build/input_package/dinput8.dll')
assert sha(Path(share['zip']))==share['zip_sha256']==launch['zip_sha256'] and launch['checks']==55
assert manifest['combat_order_fix']['army_attack_action']==7 and manifest['combat_order_fix']['army_target_kind']==27
assert manifest['recruitment_button_layout']['y']=='layout_height-184'
assert sha(game/'SAN14CE_KO.exe')==proof['original_exe_sha256']
assert sha(game/'SAN14_AI_PreviousInput.dll')==state['previous_sha256']
assert sha(p/'backup/recruitment_ui_before_crash_order_fix_20261011/dinput8.dll')==proof['installed_input_sha256']
for group in ['recruitment_source_sha256','obstacle_source_sha256']:
    for name,expected in proof[group].items():assert sha(p/name)==expected,name
previous=p/'dist/SAN14_AI_v10.2_integrated.zip'
assert sha(previous)==compat['zip_sha256'] and compat['checks']==41
with zipfile.ZipFile(share['zip']) as z,zipfile.ZipFile(previous) as old:
    for name in ['AI모드_적용.cmd','AI모드_해제_원본복구.cmd','AI모드_호환성진단.cmd','AI_TacticsMod/관리.ps1','AI_TacticsMod/진단.ps1']:
        assert z.read(name)==old.read(name),'Installer changed: '+name
    for name in ['dinput8.dll','manifest.json']:
        assert z.read('AI_TacticsMod/build/input_package/'+name)==(p/'build/input_package'/name).read_bytes()
    assert json.loads(old.read('AI_TacticsMod/build/input_package/manifest.json'))['distribution_compatibility_revision']==manifest['distribution_compatibility_revision']
total=sum(proof[k]['checks'] for k in ['native_fixture','sortie_fixture','merit_fixture','fixture','personnel_fixture','awards_fixture'])
assert total==10337
record={'status':'crash_order_fix_release_verified','release_version':'v10.4','dll_sha256':share['dll_sha256'],
    'zip_sha256':share['zip_sha256'],'installed_apply_zip_match':True,'fixture_checks':total,'launcher_checks':55,
    'unchanged_compatibility_checks_reused':41,'compatibility_evidence_zip_sha256':compat['zip_sha256'],
    'post_fix_gameplay_verified':False,'live_ui_verified':False,'recipient_same_crash_confirmed':False}
(p/'build/crash_order_fix_verification.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(json.dumps(record,indent=2))
