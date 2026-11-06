"""Layout stage 1 (user exception 2026-11-05): loader order and Previous-input loop prevention, with harmless mocks.

Order: SAN14_Layout.dll is initialized first, then the convenience module starts, then Layout publishes
the registered hooks. A missing or failing Layout never blocks the convenience module (UI features stay off).
Loop: a renamed copy of the loader sitting in the Previous-input slot must not recurse; the loader leaves
through the system DirectInput provider. Never loads the game."""
from pathlib import Path
import hashlib,json,shutil,subprocess,tempfile
p=Path(__file__).resolve().parents[1];binary=p/'build/development/loader_split'
folder=Path(tempfile.mkdtemp(prefix='layout_loader_verify_',dir=p/'build')).resolve()
checks=0
def ini(case):
    (case/'AI_TacticsMod').mkdir()
    (case/'AI_TacticsMod'/'기능설정.ini').write_bytes(('﻿[groups]\r\ncombat=0\r\n').encode('utf-16-le'))
try:
    # (name, layout dll or None, convenience present, expected convenience calls, expected order lines)
    cases=[('layout_first','LayoutMock.dll',True,1,['layout_init','convenience','layout_publish']),
           ('layout_missing',None,True,1,['convenience']),
           ('layout_initialize_fails','LayoutMockFail.dll',True,1,['layout_init','convenience']),
           ('layout_without_convenience','LayoutMock.dll',False,0,['layout_init'])]
    for name,layout,convenience,expected,order in cases:
        case=folder/name;case.mkdir();ini(case)
        shutil.copy2(binary/'dinput8.dll',case/'dinput8.dll')
        if layout:shutil.copy2(binary/layout,case/'SAN14_Layout.dll')
        if convenience:shutil.copy2(binary/'LayoutMockConvenience.dll',case/'SAN14_Convenience.dll')
        run=subprocess.run([str(binary/'loader_fixture.exe'),str(case/'dinput8.dll'),str(expected)],cwd=case,capture_output=True,timeout=20)
        assert run.returncode==0,(name,run.stdout.decode('utf-8',errors='replace'),run.stderr.decode('utf-8',errors='replace'))
        checks+=json.loads(run.stdout)['checks']
        log=case/'layout_order.log'
        lines=log.read_text(encoding='ascii').split() if log.exists() else []
        assert lines==order,(name,lines,order);checks+=1
    # Previous-input loop: the Previous slot holds a copy of this very loader.
    case=folder/'previous_is_self';case.mkdir();ini(case)
    # The Previous provider is looked up next to the host EXE, so the fixture EXE runs from this folder.
    shutil.copy2(binary/'dinput8.dll',case/'dinput8.dll');shutil.copy2(binary/'dinput8.dll',case/'SAN14_AI_PreviousInput.dll')
    shutil.copy2(binary/'loader_fixture.exe',case/'loader_fixture.exe')
    run=subprocess.run([str(case/'loader_fixture.exe'),str(case/'dinput8.dll'),'0'],cwd=case,capture_output=True,timeout=20)
    assert run.returncode==0,('previous_is_self',run.stdout.decode('utf-8',errors='replace'),run.stderr.decode('utf-8',errors='replace'))
    checks+=json.loads(run.stdout)['checks']
    record=dict(status='layout_loader_fixture_passed',checks=checks,cases=[c[0] for c in cases]+['previous_is_self'],
                loader_sha256=hashlib.sha256((binary/'dinput8.dll').read_bytes()).hexdigest(),gameplay_verified=False)
    (p/'build/layout_loader_fixture.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(record))
finally:
    assert folder.is_relative_to((p/'build').resolve()) and folder.name.startswith('layout_loader_verify_')
    shutil.rmtree(folder)
