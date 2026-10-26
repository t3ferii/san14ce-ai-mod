"""Simulate file-write failures in isolated copies of the real manager."""
from pathlib import Path
import hashlib,json,shutil,subprocess,tempfile,zipfile
p=Path(__file__).resolve().parents[1];game_root=p.parent
package=Path(json.loads((p/'build/share_package.json').read_text(encoding='utf-8'))['zip'])
folder=Path(tempfile.mkdtemp(prefix='split_failure_',dir=p/'build')).resolve()
checks=0
try:
    stages=['combat','convenience','input','state']
    for label in [f'{mode}_{stage}' for mode in ['fresh','upgrade'] for stage in stages]:
        mode,stage=label.split('_');game=folder/label;game.mkdir()
        with zipfile.ZipFile(package) as z:
            for name in z.namelist():
                f=(game/name).resolve();assert f.is_relative_to(game);f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(z.read(name))
        shutil.copy2(game_root/'SAN14CE_KO.exe',game/'SAN14CE_KO.exe')
        manager=game/'AI_TacticsMod/관리.ps1'
        text=manager.read_text(encoding='utf-8-sig')
        variable={'input':'$taskTarget','state':'$taskStateFile','combat':"(Join-Path $taskGame 'SAN14_CombatAI.dll')",'convenience':"(Join-Path $taskGame 'SAN14_Convenience.dll')"}[stage]
        fault=f"if ($Path -eq {variable} -and -not $script:taskInjectedOnce) {{$script:taskInjectedOnce=$true;throw 'fixture file-write failure'}}\n        "
        text=text.replace('[IO.File]::WriteAllBytes($taskTemp, $Bytes)',fault+'[IO.File]::WriteAllBytes($taskTemp, $Bytes)')
        manager.write_text(text,encoding='utf-8-sig')
        if mode=='upgrade':
            (game/'dinput8.dll').write_bytes(b'previous base module');(game/'SAN14_CombatAI.dll').write_bytes(b'previous combat module')
            state=game/'AI_TacticsMod/backup/input_state.json';state.parent.mkdir(exist_ok=True)
            digest=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
            state.write_text(json.dumps(dict(installed=True,previous_sha256=None,installed_sha256=digest(game/'dinput8.dll'),modules={'SAN14_CombatAI.dll':digest(game/'SAN14_CombatAI.dll')})),encoding='utf-8')
        names=['dinput8.dll','SAN14_CombatAI.dll','SAN14_Convenience.dll','AI_TacticsMod/backup/input_state.json']
        old={name:(game/name).read_bytes() if (game/name).exists() else None for name in names}
        run=subprocess.run('cmd.exe /d /c call ".\\AI모드_적용.cmd" --no-pause',cwd=game,capture_output=True,timeout=30)
        assert run.returncode!=0;checks+=1
        for name,data in old.items():
            assert ((game/name).read_bytes() if (game/name).exists() else None)==data;checks+=1
        assert hashlib.sha256((game/'SAN14CE_KO.exe').read_bytes()).digest()==hashlib.sha256((game_root/'SAN14CE_KO.exe').read_bytes()).digest();checks+=1
    report={'status':'split_install_failure_fixture_passed','checks':checks,'actual_game_modified':False,'fresh_and_two_dll_upgrade_rollback':True,'zip_sha256':hashlib.sha256(package.read_bytes()).hexdigest()}
    (p/'build/split_install_failure_fixture.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report))
finally:
    assert folder.is_relative_to((p/'build').resolve()) and folder.name.startswith('split_failure_')
    shutil.rmtree(folder)
