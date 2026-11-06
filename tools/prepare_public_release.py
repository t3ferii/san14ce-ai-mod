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

def enrich_layout(layout):
    # The build machine gathers release history; runtime packages need no dev files.
    current={(int(r['ktid']),int(r['offset']),int(r['size'])) for r in layout['records']}
    hashes=set();records={}
    historical=[P/'build/public_v15_package/traits9_layout.json']
    historical += [P/f'build/public_v15_{n}_package/traits9_layout.json' for n in range(1,5)]
    historical += [P/'updates/v15.3/files/AI_TacticsMod/build/input_package/traits9_layout.json']
    for source in historical:
        if not source.is_file(): continue
        old=read(source)
        if old.get('bank_sha256'): hashes.add(old['bank_sha256'])
        for r in old.get('records',[]):
            key=(int(r['ktid']),int(r['offset']),int(r['size']))
            if key not in current: records[key]={'ktid':key[0],'offset':key[1],'size':key[2]}
    layout['legacy_bank_sha256']=sorted(hashes)
    layout['legacy_records']=[records[k] for k in sorted(records)]
    for r in layout['records']:
        try: locator=__import__('re').search(rb'([0-9a-f]+)@([0-9a-f]+)(?:#([0-9a-f]+))?\x00?$',__import__('base64').b64decode(r['original_entry']))
        except Exception: locator=None
        assert not locator or not (locator.group(3) and 24 <= int(locator.group(3),16) <= 31), 'original_entry points to mod bank 24..31: '+str(r['ktid'])

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
              'SAN14_Convenience.dll':['AIInitialize','SAN14ConvenienceInitialize'],'SAN14_CCZ.dll':['SAN14CczGetApi'],
              # 2026-11-05 user exception: 5th DLL, loaded first by dinput8; without it the Layout-managed features stay off.
              'SAN14_Layout.dll':['SAN14LayoutGetApi','SAN14LayoutInitialize','SAN14LayoutPublish']}
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
    enrich_layout(j)
    old_layout=read(P/'build/input_package/traits9_layout.json')
    assert all(r in j['records'] for r in old_layout['records'])
    stage.mkdir()
    for n in expected:shutil.copyfile(binary/n,stage/n)
    gui='AI모드_설치관리자.exe'
    shutil.copyfile(P/'build/development/installer'/gui,stage/gui)
    shutil.copyfile(layout/'traits9_layout.bin',stage/'traits9_layout.bin')
    save(stage/'traits9_layout.json',j)
    # Installer update/rollback colours (user 2026-11-01): feature catalogue of every published release + this one.
    import subprocess,sys
    subprocess.run([sys.executable,'-B','-X','utf8',str(P/'tools/release_features.py'),args.version,str(stage/'release_features.json')],check=True)
    files={n:sha(stage/n) for n in expected}
    m.update(release_version=args.version,dll_sha256=files['dinput8.dll'],local_diagnostic_only=False,
             code_signed=False,gameplay_verified=False,progress_log={'runtime_logging_compiled_out':True,'enabled':False},
             function_cost_profile={'enabled':False,'compiled_out':True})
    m['split_modules'].update(files=files,source_sha256=inputs,source_sha256_by_module={n:inputs for n in expected},
        runtime_log_writers_compiled_out=True,gameplay_verified=False,startup_verified=False)
    m['gui_installer'].update(sha256=sha(stage/gui))
    # The title editor needs the complete lazy-loaded gallery as well as the battle atlas.
    m['ccz_assets']={n:sha(P/n) for n in ('assets/ccz/ccz_units.bin','assets/ccz/ccz_catalog.bin')}
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
