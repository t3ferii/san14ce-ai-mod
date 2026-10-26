"""Run the packaged CMD/Windows PowerShell installers in isolated game folders."""
from pathlib import Path
import hashlib,json,shutil,subprocess,tempfile,zipfile
import configparser

PROJECT=Path(__file__).resolve().parents[1]
GAME=PROJECT.parent
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None
baseline={p.name:sha(p) for p in [GAME/'SAN14CE_KO.exe',GAME/'dinput8.dll',GAME/'SAN14_CombatAI.dll',GAME/'SAN14_Convenience.dll',GAME/'D3D11.dll']}
import sys
REPORT=Path(sys.argv[1]) if len(sys.argv)>1 else PROJECT/'build/share_package.json'
SHARE=json.loads(REPORT.read_text(encoding='utf-8'))
package=Path(SHARE['zip']).resolve()
assert package.is_relative_to((PROJECT/'dist').resolve()) and package.suffix=='.zip'
fixture=Path(tempfile.mkdtemp(prefix='share_verify_',dir=PROJECT/'build')).resolve()
assert fixture.is_relative_to((PROJECT/'build').resolve()) and fixture.name.startswith('share_verify_')
checks=0
def check(value,label):
    global checks
    assert value,label
    checks+=1
def invoke(game,mode,success=True):
    script='AI모드_적용.cmd' if mode=='Apply' else 'AI모드_해제_원본복구.cmd'
    run=subprocess.run(f'cmd.exe /d /c call ".\\{script}" --no-pause',cwd=game,capture_output=True,timeout=30)
    check((run.returncode==0)==success,run.stdout.decode('utf-8',errors='replace')+run.stderr.decode('utf-8',errors='replace'))
try:
    for index,provider in enumerate([None,b'fixture original input provider']):
        game=fixture/str(index);game.mkdir()
        with zipfile.ZipFile(package) as archive:
            for name in archive.namelist():
                dest=(game/name).resolve();assert dest.is_relative_to(game.resolve())
                dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(archive.read(name))
        shutil.copyfile(GAME/'SAN14CE_KO.exe',game/'SAN14CE_KO.exe')
        packaged_manifest=json.loads((game/'AI_TacticsMod/build/input_package/manifest.json').read_text(encoding='utf-8'))
        if packaged_manifest.get('progress_log') and not (isinstance(packaged_manifest['progress_log'],dict) and packaged_manifest['progress_log'].get('runtime_logging_compiled_out')):
            ini=configparser.ConfigParser();ini.read(game/'AI_TacticsMod/diagnostics.ini',encoding='utf-8')
            check(ini.getint('diagnostics','enabled')==0,'shared diagnostics default off')
            ini.read(game/'AI_TacticsMod/progress_native.ini',encoding='utf-8')
            check(ini.getint('progress_native','profile')==0,'shared phase instrumentation default off')
        if packaged_manifest.get('target_retreat'):
            ini=configparser.ConfigParser();ini.read(game/'AI_TacticsMod/target_retreat.ini',encoding='utf-8')
            check(ini.getint('target_retreat','enabled')==1,'target-retreat fix enabled in shared package')
        if packaged_manifest.get('region_officers'):
            check(not packaged_manifest['region_officers'].get('one_shot_profile'),'regional profiler absent from manifest')
            check(not (game/'AI_TacticsMod/region_officers_profile.ini').exists(),'regional profiler config absent from package')
            regional_binary=(game/'AI_TacticsMod/build/input_package/SAN14_Convenience.dll').read_bytes()
            markers=['region_officers_profile.ini','region_officers_once.json','warm_adapter_cpu_us','trait_native_reads']
            check(all(marker.encode(encoding) not in regional_binary for marker in markers for encoding in ('ascii','utf-16le')),'regional profiler writer and counters absent from DLL')
        if packaged_manifest.get('gui_installer') and SHARE.get('gui_installer_included',True):
            gui=game/packaged_manifest['gui_installer']['file']
            check(gui.exists() and sha(gui)==packaged_manifest['gui_installer']['sha256'],'GUI installer EXE at root matches manifest')
        target=game/'dinput8.dll';chain=game/'SAN14_AI_PreviousInput.dll'
        state=game/'AI_TacticsMod/backup/input_state.json'
        payload=game/'AI_TacticsMod/build/input_package/dinput8.dll'
        new=payload.read_bytes();new_hash=sha(payload)
        split=packaged_manifest.get('split_modules')
        combat=game/'SAN14_CombatAI.dll';combat_payload=game/'AI_TacticsMod/build/input_package/SAN14_CombatAI.dll'
        combat_bytes=combat_payload.read_bytes() if split else None
        combat_hash=sha(combat_payload) if split else None
        previous_hash=hashlib.sha256(provider).hexdigest() if provider is not None else None
        if provider is not None:target.write_bytes(provider)
        for name,digest in packaged_manifest.get('asset_originals',{}).items():
            source=GAME/name
            if sha(source)!=digest:
                local_state=json.loads((PROJECT/'backup/input_state.json').read_text(encoding='utf-8-sig'))
                original=local_state.get('asset_restore',{}).get(name,{})
                check(original.get('sha256')==digest,'recorded original asset matches package')
                source=Path(original['backup']).resolve()
                check(source.is_relative_to((PROJECT/'backup').resolve()) and sha(source)==digest,'verified original asset backup')
            dest=game/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,dest)
            check(sha(dest)==digest,'layout fixture starts with original index')
        invoke(game,'Restore');check(sha(target)==previous_hash,'fresh restore leaves input unchanged')
        optional=[n for n in packaged_manifest.get('assets',{}) if n.endswith('.txt')]
        if index==1:
            # Reported case: a same-named reference text from another mod/older copy must not block a fresh install.
            for name in optional:(game/name).write_bytes(b'other mod text')
        invoke(game,'Apply');check(sha(target)==new_hash and sha(chain)==previous_hash,'initial apply and provider preservation')
        if index==1:
            for name in optional:
                owned=json.loads((game/'AI_TacticsMod/backup/input_state.json').read_text(encoding='utf-8'))['modules']
                check((game/name).read_bytes()==b'other mod text' and name not in owned,'existing reference text kept and not owned')
                (game/name).unlink()
            invoke(game,'Apply');check(all(sha(game/n)==packaged_manifest['assets'][n] for n in optional),'missing reference text installed on the next apply')
        for name,digest in packaged_manifest.get('assets',{}).items():
            asset=game/name;asset_payload=game/'AI_TacticsMod/build/input_package'/name
            data=asset_payload.read_bytes();check(sha(asset)==digest,'hover text installed')
            if name.endswith('.txt'):
                # Optional reference text: an edited copy is kept and released from ownership, never blocking.
                asset.write_bytes(b'foreign text');invoke(game,'Apply')
                check(asset.read_bytes()==b'foreign text','changed reference text preserved without blocking')
                asset.unlink();invoke(game,'Apply');check(sha(asset)==digest,'reference text reinstalled when absent')
            else:
                asset.write_bytes(b'foreign text');invoke(game,'Apply',False);invoke(game,'Restore',False)
                check(asset.read_bytes()==b'foreign text','changed hover text preserved');asset.write_bytes(data)
            asset_payload.write_bytes(b'bad text');invoke(game,'Apply',False)
            check(sha(asset)==digest,'bad hover text payload preserves installation');asset_payload.write_bytes(data)
        if split:
            check(sha(combat)==combat_hash,'both DLLs installed')
            combat.unlink();invoke(game,'Apply',False);invoke(game,'Restore',False)
            check(sha(target)==new_hash,'missing combat module does not remove input DLL');combat.write_bytes(combat_bytes)
            combat.write_bytes(b'changed combat');invoke(game,'Apply',False);invoke(game,'Restore',False)
            check(combat.read_bytes()==b'changed combat' and sha(target)==new_hash,'foreign combat module preserved');combat.write_bytes(combat_bytes)
            combat_payload.write_bytes(b'bad combat payload');invoke(game,'Apply',False)
            check(sha(target)==new_hash and sha(combat)==combat_hash,'bad combat payload leaves installed pair intact');combat_payload.write_bytes(combat_bytes)
            if split.get('loader_only'):
                convenience=game/'SAN14_Convenience.dll';convenience_payload=game/'AI_TacticsMod/build/input_package/SAN14_Convenience.dll'
                convenience_bytes=convenience_payload.read_bytes();convenience_hash=sha(convenience_payload)
                check(sha(convenience)==convenience_hash,'convenience DLL installed separately')
                convenience.unlink();invoke(game,'Apply',False);invoke(game,'Restore',False)
                check(sha(target)==new_hash and sha(combat)==combat_hash,'missing convenience leaves other modules intact');convenience.write_bytes(convenience_bytes)
                convenience.write_bytes(b'foreign convenience');invoke(game,'Apply',False);invoke(game,'Restore',False)
                check(convenience.read_bytes()==b'foreign convenience','changed convenience preserved');convenience.write_bytes(convenience_bytes)
                convenience_payload.write_bytes(b'bad convenience payload');invoke(game,'Apply',False)
                check(sha(convenience)==convenience_hash,'bad convenience payload preserves installed module');convenience_payload.write_bytes(convenience_bytes)
        invoke(game,'Apply');check(sha(target)==new_hash and sha(chain)==previous_hash,'repeat apply cannot chain itself')
        if not packaged_manifest.get('assets'):
            # Upgrade from pre-cleanup packages with an owned, edited or missing
            # optional text. It must leave DLL checks intact and release ownership.
            retired=game/'SAN14_전법설명.txt'
            for contents in (b'old owned text',b'user edited text',None):
                saved=json.loads(state.read_text(encoding='utf-8'))
                saved['modules'][retired.name]=hashlib.sha256(b'old owned text').hexdigest()
                state.write_text(json.dumps(saved),encoding='utf-8')
                if contents is None:
                    retired.unlink(missing_ok=True)
                else:retired.write_bytes(contents)
                invoke(game,'Apply')
                migrated=json.loads(state.read_text(encoding='utf-8'))
                check(retired.name not in migrated['modules'],'retired text ownership released')
                check((retired.read_bytes() if retired.exists() else None)==contents,'retired text left untouched')
            check(not (game/'AI_TacticsMod/build/input_package'/retired.name).exists(),'retired text absent from package')
        target.write_bytes(b'external change')
        invoke(game,'Restore',False);check(target.read_bytes()==b'external change','restore refuses external input change')
        invoke(game,'Apply',False);check(target.read_bytes()==b'external change','apply refuses external input change')
        target.write_bytes(new);payload.write_bytes(b'corrupt payload')
        invoke(game,'Apply',False);check(sha(target)==new_hash,'payload failure leaves installed mod intact');payload.write_bytes(new)
        invoke(game,'Restore');check(sha(target)==previous_hash and not chain.exists(),'restore original provider or absence')
        for name in packaged_manifest.get('assets',{}):
            original=packaged_manifest.get('asset_originals',{}).get(name)
            check(sha(game/name)==original,'restore original asset or remove new asset')
        if split:check(not combat.exists(),'restore removes owned combat module')
        if split and split.get('loader_only'):check(not convenience.exists(),'restore removes owned convenience module')
        check(not json.loads(state.read_text(encoding='utf-8'))['installed'],'restore state recorded')
        invoke(game,'Restore');invoke(game,'Apply')
        # Existing local installation state uses the same schema: exercise update.
        old=b'fixture previous AI version';target.write_bytes(old)
        saved=json.loads(state.read_text(encoding='utf-8'));saved['installed_sha256']=hashlib.sha256(old).hexdigest()
        state.write_text(json.dumps(saved),encoding='utf-8')
        invoke(game,'Apply');check(sha(target)==new_hash and sha(chain)==previous_hash,'previous AI update preserves original provider')
        invoke(game,'Restore');check(sha(target)==previous_hash,'updated version can restore')
        if split and split.get('loader_only'):
            # Upgrade the actual previous two-DLL schema without retaining its base DLL as a provider.
            target.write_bytes(old);combat.write_bytes(combat_bytes)
            saved={'installed':True,'installed_sha256':hashlib.sha256(old).hexdigest(),'previous_sha256':previous_hash,'modules':{'SAN14_CombatAI.dll':combat_hash}}
            state.write_text(json.dumps(saved),encoding='utf-8')
            if provider is not None:chain.write_bytes(provider)
            invoke(game,'Apply');check(sha(target)==new_hash and sha(convenience)==convenience_hash and sha(chain)==previous_hash,'two-DLL to loader plus two feature DLLs upgrade')
            invoke(game,'Restore');check(sha(target)==previous_hash and not combat.exists() and not convenience.exists(),'upgraded three-DLL release restores original state')
        state.unlink();target.write_bytes(new)
        invoke(game,'Apply',False);check(not chain.exists() and sha(target)==new_hash,'orphan AI loader cannot be chained into itself')
        target.unlink();chain.write_bytes(b'existing reserved name')
        invoke(game,'Apply',False);check(not target.exists() and chain.read_bytes()==b'existing reserved name','reserved chain name is preserved')
        (game/'SAN14CE_KO.exe').write_bytes(b'unsupported exe')
        invoke(game,'Apply',False);check(not target.exists(),'unsupported executable receives no mod')
    check(all(sha(GAME/name)==digest for name,digest in baseline.items()),'actual local installation unchanged')
    report={'status':'share_windows_launchers_verified','checks':checks,'python_required_by_recipient':False,
            'existing_provider_and_no_provider_tested':True,'actual_local_installation_unchanged':True,
            'zip_sha256':sha(package),'candidate_gameplay_verified':False}
    (PROJECT/('build/share_verification.json' if REPORT.name=='share_package.json' else 'build/share_verification_'+REPORT.stem.split('share_package_')[-1]+'.json')).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,indent=2))
finally:
    # Only remove this freshly created fixture, after checking its absolute scope.
    assert fixture.is_relative_to((PROJECT/'build').resolve()) and fixture.name.startswith('share_verify_')
    shutil.rmtree(fixture)
