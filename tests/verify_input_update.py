"""Exercise installer preservation/rollback in a separate build fixture folder.
Never writes the actual game folder, starts a game, or changes process memory.
"""
from pathlib import Path
import sys, json, hashlib
PROJECT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(PROJECT/'tools'))
import package_input as installer

area=PROJECT/'build/installer_fixture'
area.mkdir(exist_ok=True)
installer.PROJECT=area;installer.GAME=area/'game';installer.GAME.mkdir(exist_ok=True)
installer.PACKAGE=area/'package';installer.PACKAGE.mkdir(exist_ok=True)
installer.STATE=area/'backup/input_state.json'
installer.TARGET=installer.GAME/'dinput8.dll';installer.CHAIN=installer.GAME/'SAN14_AI_PreviousInput.dll'
original=b'fixture original exe';provider=b'fixture original input provider'
installer.ORIGINAL_SHA=hashlib.sha256(original).hexdigest()
(installer.GAME/'SAN14CE_KO.exe').write_bytes(original)
installer.TARGET.write_bytes(provider)
if installer.STATE.exists():
    assert not json.loads(installer.STATE.read_text())['installed'],'Previous fixture run was incomplete'
assert not installer.CHAIN.exists(),'Previous fixture chain remains'
installer.no_game=lambda:None

def candidate(data):
    (installer.PACKAGE/'dinput8.dll').write_bytes(data)
    (installer.PACKAGE/'manifest.json').write_text(json.dumps({'dll_sha256':installer.sha(data)}))
def run(action):
    sys.argv=['fixture',action];installer.main()
def rejects(action):
    try:run(action)
    except (RuntimeError,AssertionError):return True
    return False
checks=0
def check(condition):
    global checks
    assert condition;checks+=1

candidate(b'loader A');run('install')
check(installer.CHAIN.read_bytes()==provider)
candidate(b'loader B')
check(rejects('install') and installer.TARGET.read_bytes()==b'loader A')
run('update')
check(installer.TARGET.read_bytes()==b'loader B' and installer.CHAIN.read_bytes()==provider)
check((installer.GAME/'SAN14CE_KO.exe').read_bytes()==original)
candidate(b'loader C')
installer.TARGET.write_bytes(b'external change')
check(rejects('update') and installer.TARGET.read_bytes()==b'external change')
installer.TARGET.write_bytes(b'loader B')
def running():raise RuntimeError('fixture running game')
installer.no_game=running
check(rejects('update') and installer.TARGET.read_bytes()==b'loader B')
installer.no_game=lambda:None
saved_atomic=installer.atomic_write
def fail_state(path,data):
    if path==installer.STATE:raise OSError('fixture state-write failure')
    saved_atomic(path,data)
installer.atomic_write=fail_state
try:run('update')
except OSError:pass
else:raise AssertionError('Expected state-write failure')
installer.atomic_write=saved_atomic
check(installer.TARGET.read_bytes()==b'loader B' and json.loads(installer.STATE.read_text())['installed_sha256']==installer.sha(b'loader B'))
run('restore')
check(installer.TARGET.read_bytes()==provider and not installer.CHAIN.exists())
print(json.dumps({'status':'input_update_fixture_passed','checks':checks,'real_game_folder_modified':False}))
