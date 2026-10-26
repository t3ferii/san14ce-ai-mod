"""Prove relation ABI/call sites against this build, independently of the mocks."""
from pathlib import Path
import hashlib,json,re,struct,sys

project=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(project/'legacy/FogMod_Source/tools'))
from audit_native_game import PEImage

game=project.parent/'SAN14CE_KO.exe'
plain=project/'build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe'
checks=0
def check(ok,label):
    global checks
    assert ok,label
    checks+=1
check(hashlib.sha256(game.read_bytes()).hexdigest()=='87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a','live EXE is still the original file')
check(hashlib.sha256(plain.read_bytes()).hexdigest()=='1fbe6be7466a66466fe97cf3ab7d49392913346879c7d40de7f4eaf86eef81c6','fresh analysis copy is the Editor83 verified unpacked image')
pe=PEImage(plain)
check(struct.unpack('<Q',pe.read(0x1459678+0x1F8,8))[0]-pe.image_base==0x7D4B90,'sortie person selection initializer')
check(pe.read(0x7D4BEC,6)==bytes.fromhex('41bed4000000'),'sortie allocates relation columns from index212')
check(pe.read(0x7D4CF3,4)==bytes.fromhex('4883fe09'),'sortie creates nine relation columns')
def relative(site,target,opcode=b'\xe8'):
    b=pe.read(site,len(opcode)+4)
    check(b.startswith(opcode) and site+len(b)+struct.unpack('<i',b[len(opcode):])[0]==target,f'native reference {site:X}->{target:X}')
relative(0x7D4F6B,0x7B2430,b'\x48\x8d\x05')
relative(0x7D4F7D,0x7B2550,b'\x48\x8d\x05')
relative(0x7B2467,0x7D6AC0)
relative(0x7B246F,0x30BED0)
relative(0x7B2560,0x7D6AC0)
relative(0x7D6B21,0x2A6AA0)
relative(0x2A6AD6,0x2A6B60)
source=(project/'src/native_unified_relations.inl').read_text(encoding='utf-8')
for name,address in [('compare',0x7D6AC0),('caption',0x30BED0)]:
    initializer=re.search(r'unsigned char '+name+r'\[\]=\{([^}]+)\}',source)[1]
    expected=bytes(int(n,16) for n in re.findall(r'0x([0-9A-Fa-f]+)',initializer))
    check(len(expected)==16 and pe.read(address,16)==expected,f'production {name} guard matches fresh native bytes')
record=dict(checks=checks,game_modified=False,native_game_executed=False,
            source_exe_sha256=hashlib.sha256(game.read_bytes()).hexdigest(),
            analysis_sha256=hashlib.sha256(plain.read_bytes()).hexdigest(),
            call_chain=['7D4B90','7B2430','7D6AC0','2A6AA0','2A6B60'],caption='30BED0')
(project/'build/development/unified_personnel/sortie_relation_trace.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(json.dumps(record))
