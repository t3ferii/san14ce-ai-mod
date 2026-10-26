"""Check actual release export ownership and pure loader contents without loading the game."""
from pathlib import Path
import hashlib,json,struct,sys
p=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(p.parent/'AI_TacticsMod/legacy/FogMod_Source/tools'))
from audit_native_game import PEImage
binary=p/'build/development/loader_split'
def exports(path):
    pe=PEImage(path);optional=struct.unpack_from('<I',pe.data,0x3c)[0]+24
    rva=struct.unpack_from('<I',pe.data,optional+112)[0]
    header=pe.read(rva,40);count,names=struct.unpack_from('<II',header,24)[0],struct.unpack_from('<I',header,32)[0]
    result=[]
    for i in range(count):
        address=struct.unpack('<I',pe.read(names+i*4,4))[0]
        result.append(pe.read(address,80).split(b'\0')[0].decode('ascii'))
    return sorted(result)
expected={'dinput8.dll':['DirectInput8Create'],'SAN14_CombatAI.dll':['AIInitialize','SAN14CombatGetApi'],
          'SAN14_Convenience.dll':['AIInitialize','SAN14ConvenienceInitialize']}
checks=0
for name,names in expected.items():
    assert exports(binary/name)==names,(name,exports(binary/name));checks+=1
loader=(binary/'dinput8.dll').read_bytes()
for marker in [b'split_modules_installed',b'campaign_v7',b'injury_protection_hooks_installed',b'award_warehouse_hooks_installed']:
    assert marker not in loader;checks+=1
assert len(loader)<(binary/'SAN14_Convenience.dll').stat().st_size;checks+=1
record=dict(status='split_binary_fixture_passed',checks=checks,exports=expected,loader_only=True,
            files={name:hashlib.sha256((binary/name).read_bytes()).hexdigest() for name in expected},gameplay_verified=False)
(p/'build/split_binary_fixture.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(json.dumps(record))
