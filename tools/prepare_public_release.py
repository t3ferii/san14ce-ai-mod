"""Stage a fresh unsigned, log-free release without touching the installed game."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
from install_local import sources
from install_damage_preview import exports

P=Path(__file__).resolve().parents[1]
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def main():
    a=argparse.ArgumentParser()
    a.add_argument('version');a.add_argument('--layout',required=True)
    args=a.parse_args()
    assert args.version.replace('.','').replace('-','').isalnum()
    stage=P/'build'/('public_'+args.version.replace('.','_').replace('-','_')+'_package')
    assert not stage.exists(),'Existing stage is protected'
    binary=P/'build/development/loader_split'
    original=read(P/'build/input_package/manifest.json')
    m=read(P/'build/input_package/manifest.json')
    inputs=sources()
    newest=max((P/n).stat().st_mtime for n in inputs if n.startswith('src/'))
    expected={'dinput8.dll':['DirectInput8Create'],'SAN14_CombatAI.dll':['AIInitialize','SAN14CombatGetApi'],
              'SAN14_Convenience.dll':['AIInitialize','SAN14ConvenienceInitialize']}
    for n,e in expected.items():
        assert exports(binary/n)==e,n
        if n!='dinput8.dll':
            assert (binary/n).stat().st_mtime>=newest,'Stale release build: '+n
            b=(binary/n).read_bytes()
            assert b'day=' not in b and 'function_cost_'.encode('utf-16le') not in b,'Diagnostic binary: '+n
    layout=(P/args.layout).resolve()
    assert layout.is_relative_to(P.resolve())
    j=read(layout/'traits9_layout.json')
    assert sha(layout/'traits9_layout.bin')==j['bank_sha256']
    old_layout=read(P/'build/input_package/traits9_layout.json')
    assert all(r in j['records'] for r in old_layout['records'])
    stage.mkdir()
    for n in expected:shutil.copyfile(binary/n,stage/n)
    gui='AI모드_설치관리자.exe'
    shutil.copyfile(P/'build/development/installer'/gui,stage/gui)
    for n in ('traits9_layout.bin','traits9_layout.json'):shutil.copyfile(layout/n,stage/n)
    files={n:sha(stage/n) for n in expected}
    m.update(release_version=args.version,dll_sha256=files['dinput8.dll'],local_diagnostic_only=False,
             code_signed=False,gameplay_verified=False,progress_log={'runtime_logging_compiled_out':True,'enabled':False},
             function_cost_profile={'enabled':False,'compiled_out':True})
    m['split_modules'].update(files=files,source_sha256=inputs,source_sha256_by_module={n:inputs for n in expected},
        runtime_log_writers_compiled_out=True,gameplay_verified=False,startup_verified=False)
    m['gui_installer'].update(sha256=sha(stage/gui))
    m['feature_switches']['count']=(P/'distribution/FeatureSelect.ps1').read_text(encoding='utf-8-sig').count('@{Group=')
    m['layout_patch'].update(bank_sha256=j['bank_sha256'],json_sha256=sha(stage/'traits9_layout.json'),merge=str(len(j['records']))+' ktid entries')
    m['progress_native']['profile_logging_compiled_out']=True
    m['expert_adjacency_presets']={'owner':'SAN14_Convenience.dll','layout':0xDA74DA9F,'nodes':[48],
        'create_during_native_row_init':True,'exclusive_per_force':True,'apply_on_standard':True,
        'decimal_percent_saved':True,'gameplay_verified':False,'screen_verified':False}
    save(stage/'manifest.json',m)
    report={'status':'public_release_staged','version':args.version,'installed_baseline':original['release_version'],
        'files':files,'layout_records':len(j['records']),'settings_count':m['feature_switches']['count'],
        'code_signed':False,'gameplay_verified':False,'stage':str(stage),'sources':inputs}
    save(P/'build'/('public_'+args.version.replace('.','_').replace('-','_')+'_preparation.json'),report)
    print(json.dumps({k:report[k] for k in ('status','version','stage','layout_records','settings_count')}))

if __name__=='__main__':main()
