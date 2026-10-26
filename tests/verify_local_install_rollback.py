"""Shared rollback transaction tests in a fresh fake game folder only."""
from pathlib import Path
import hashlib
import json
import shutil
import sys
import tempfile
from types import SimpleNamespace

P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'tools'))
import install_local as m
sha=lambda b:hashlib.sha256(b).hexdigest()
root=Path(tempfile.mkdtemp(prefix='rollback_fixture_',dir=P/'build/development')).resolve()
assert root.is_relative_to((P/'build/development').resolve())
checks=0
def check(ok):
    global checks
    assert ok
    checks+=1
def save(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v),encoding='utf-8')
def write(p,b):p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
def case(index):
    g=root/str(index);p=g/'AI_TacticsMod';backup=p/'backup/base'
    before={'dinput8.dll':b'loader','SAN14_CombatAI.dll':b'combat','SAN14_Convenience.dll':b'old convenience'}
    after=dict(before);after['SAN14_Convenience.dll']=b'new convenience'
    for n,b in after.items():write(g/n,b);write(p/'build/input_package'/n,b)
    for n,b in before.items():write(backup/'files'/n,b)
    old={'release_version':'old','split_modules':{'files':{n:sha(b) for n,b in before.items()}},'gui_installer':{'file':'gui.exe'}}
    new=dict(old,release_version='new',split_modules={'files':{n:sha(b) for n,b in after.items()}})
    restore={'AI_TacticsMod/build/input_package/'+n:b for n,b in before.items()}
    restore['AI_TacticsMod/build/input_package/manifest.json']=json.dumps(old).encode()
    restore['AI_TacticsMod/backup/input_state.json']=json.dumps({'release_version':'old','installed':True}).encode()
    restore['SAN14_Convenience.dll']=before['SAN14_Convenience.dll']
    for n,b in restore.items():write(backup/'files'/n,b)
    hashes={n:sha(b) for n,b in restore.items()};hashes.update({n:sha(b) for n,b in before.items()})
    save(p/'build/input_package/manifest.json',new);save(p/'backup/input_state.json',{'release_version':'new','installed':True})
    save(p/'build/new_installation.json',{'status':'installed_and_verified','installed':True,'version':'new','baseline_version':'old',
         'modules':['convenience'],'gui':False,'layout':None,'files':{n:sha(b) for n,b in after.items()},
         'before_hashes':hashes,'stage_backup':str(backup)})
    write(g/'SAN14CE_KO.exe',b'fixture original exe')
    write(p/'기능설정.ini',b'[groups]\nconvenience=1\n[diagnostics]\nenabled=1\n')
    m.P=p;m.G=g;m.ORIGINAL_SHA=sha(b'fixture original exe');m.no_game=lambda:None
    return g,p,backup

try:
    g,p,b=case(1);settings=(p/'기능설정.ini').read_bytes()
    m.rollback(SimpleNamespace(version='new'))
    check((g/'SAN14_Convenience.dll').read_bytes()==b'old convenience')
    check((g/'SAN14_CombatAI.dll').read_bytes()==b'combat' and (g/'dinput8.dll').read_bytes()==b'loader')
    check((p/'기능설정.ini').read_bytes()==settings)
    check(m.read(p/'build/input_package/manifest.json')['release_version']=='old')
    check(m.read(p/'build/new_installation.json')['status']=='rolled_back_user_report')
    g,p,b=case(2);write(b/'files/SAN14_Convenience.dll',b'corrupt backup')
    try:m.rollback(SimpleNamespace(version='new'));raise RuntimeError('corrupt backup accepted')
    except AssertionError:pass
    check((g/'SAN14_Convenience.dll').read_bytes()==b'new convenience')
    g,p,b=case(3);write(g/'SAN14_CombatAI.dll',b'other session')
    try:m.rollback(SimpleNamespace(version='new'));raise RuntimeError('concurrent update accepted')
    except AssertionError:pass
    check((g/'SAN14_Convenience.dll').read_bytes()==b'new convenience')
    g,p,b=case(4);snapshot={q.relative_to(g):q.read_bytes() for q in g.rglob('*') if q.is_file()}
    real=m.atomic_write;calls=0
    def failing(path,data):
        global calls
        calls+=1
        if calls==3:raise OSError('injected interrupted write')
        real(path,data)
    m.atomic_write=failing
    try:m.rollback(SimpleNamespace(version='new'));raise RuntimeError('injected failure ignored')
    except OSError:pass
    finally:m.atomic_write=real
    check(all((g/n).read_bytes()==data for n,data in snapshot.items()))
    # User 2026-10-26: a live unchanged installer blocked the old exception
    # rollback, leaving old runtime DLLs paired with new Apply metadata.
    def interrupted(index):
        g,p,b=case(index);stage=p/'build/staged_new';shutil.copytree(p/'build/input_package',stage)
        proof=m.read(p/'build/new_installation.json');proof.update(status='prepared',stage=str(stage))
        write(g/'gui.exe',b'unchanged GUI');write(b/'files/gui.exe',b'unchanged GUI');proof['before_hashes']['gui.exe']=sha(b'unchanged GUI')
        save(p/'build/new_installation.json',proof)
        return g,p,b
    g,p,b=interrupted(5);write(g/'SAN14_Convenience.dll',b'old convenience')
    user_settings=(p/'기능설정.ini').read_bytes();real=m.atomic_write
    def gui_locked(path,data):
        if path==g/'gui.exe':raise PermissionError('unchanged GUI is in use')
        real(path,data)
    m.atomic_write=gui_locked
    try:m.rollback(SimpleNamespace(version='new'))
    finally:m.atomic_write=real
    check(m.read(p/'build/input_package/manifest.json')['release_version']=='old')
    check((p/'build/input_package/SAN14_Convenience.dll').read_bytes()==b'old convenience')
    check((p/'기능설정.ini').read_bytes()==user_settings and (g/'gui.exe').read_bytes()==b'unchanged GUI')
    check(m.read(p/'build/new_installation.json')['status']=='rolled_back_prepared_install')
    g,p,b=interrupted(6);write(g/'SAN14_CombatAI.dll',b'other session')
    try:m.rollback(SimpleNamespace(version='new'));raise RuntimeError('prepared recovery overwrote another session')
    except AssertionError:pass
    check((g/'SAN14_Convenience.dll').read_bytes()==b'new convenience')
    g,p,b=interrupted(7);write(b/'files/SAN14_Convenience.dll',b'bad backup')
    try:m.rollback(SimpleNamespace(version='new'));raise RuntimeError('prepared recovery accepted bad backup')
    except AssertionError:pass
    check((g/'SAN14_Convenience.dll').read_bytes()==b'new convenience')
    g,p,b=interrupted(8);write(p/'build/input_package/SAN14_Convenience.dll',b'unrecognized package')
    try:m.rollback(SimpleNamespace(version='new'));raise RuntimeError('prepared recovery overwrote unknown package')
    except AssertionError:pass
    check((g/'SAN14_Convenience.dll').read_bytes()==b'new convenience')
    print(json.dumps({'status':'local_install_rollback_fixture_passed','checks':checks,'real_game_touched':False}))
finally:
    assert root.is_relative_to((P/'build/development').resolve()) and root.name.startswith('rollback_fixture_')
    shutil.rmtree(root)
