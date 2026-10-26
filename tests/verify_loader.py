"""Load the actual pure loader with harmless mock modules, never the game."""
from pathlib import Path
import hashlib,json,shutil,subprocess,tempfile
p=Path(__file__).resolve().parents[1];binary=p/'build/development/loader_split'
folder=Path(tempfile.mkdtemp(prefix='loader_verify_',dir=p/'build')).resolve()
checks=0
try:
    # Optional [groups] in AI_TacticsMod\기능설정.ini (UTF-16, as installed).
    cases=[('complete','LoaderMockCombat.dll',True,1,None),('missing_combat',None,True,0,None),
           ('missing_convenience','LoaderMockCombat.dll',False,0,None),('bad_abi','LoaderMockBadCombat.dll',True,0,None),
           ('combat_group_off_without_dll',None,True,1,'combat=0'),
           ('combat_group_off_ignores_bad_dll','LoaderMockBadCombat.dll',True,1,'combat=0'),
           ('convenience_group_off_still_coordinates','LoaderMockCombat.dll',True,1,'convenience=0'),
           ('performance_only','LoaderMockCombat.dll',True,1,'combat=0\r\nconvenience=0'),
           ('all_groups_off','LoaderMockCombat.dll',True,0,'combat=0\r\nconvenience=0\r\nperformance=0')]
    for name,combat,convenience,expected,groups in cases:
        case=folder/name;case.mkdir()
        if groups:
            (case/'AI_TacticsMod').mkdir()
            (case/'AI_TacticsMod'/'기능설정.ini').write_bytes(('﻿[groups]\r\n'+groups+'\r\n').encode('utf-16-le'))
        shutil.copy2(binary/'dinput8.dll',case/'dinput8.dll')
        if combat:shutil.copy2(binary/combat,case/'SAN14_CombatAI.dll')
        if convenience:shutil.copy2(binary/'LoaderMockConvenience.dll',case/'SAN14_Convenience.dll')
        run=subprocess.run([str(binary/'loader_fixture.exe'),str(case/'dinput8.dll'),str(expected)],cwd=case,capture_output=True,timeout=15)
        assert run.returncode==0,(name,run.stdout.decode('utf-8',errors='replace'),run.stderr.decode('utf-8',errors='replace'))
        checks+=json.loads(run.stdout)['checks']
    record=dict(status='pure_loader_fixture_passed',checks=checks,cases=[c[0] for c in cases],
                loader_sha256=hashlib.sha256((binary/'dinput8.dll').read_bytes()).hexdigest(),gameplay_verified=False)
    (p/'build/pure_loader_fixture.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(record))
finally:
    assert folder.is_relative_to((p/'build').resolve()) and folder.name.startswith('loader_verify_')
    shutil.rmtree(folder)
