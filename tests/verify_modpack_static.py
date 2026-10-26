"""Compare imported hook ownership and new stat gates with this game build."""
from pathlib import Path
import hashlib,json,re,struct,sys
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'legacy/FogMod_Source/tools'))
from audit_native_game import PEImage
game=PEImage(P.parent/'SAN14CE_KO.exe')
code=(P/'legacy/FogMod_Source/native_game_analysis/running_code_text.bin').read_bytes()
hover=PEImage(P/'research/modpack_v16/extracted/payload/SAN14_TraitHover.dll')
stats=PEImage(P/'research/modpack_v16/extracted/payload/SAN14_Stats200.dll')
checks=0
def check(v,label):
    global checks
    assert v,label
    checks+=1
def read(rva,n):return code[rva-0x1000:rva-0x1000+n] if rva<0x1000+len(code) else game.read(rva,n)
for i in range(14):
    b=stats.read(0x2000+i*112,112);rva,n,guard,gn=struct.unpack_from('<IIII',b)
    check(read(rva,n)==b[16:16+n],hex(rva))
    check(read(guard,gn)==b[48:48+gn],'surrounding '+hex(rva))
check(read(0x2b9dca,6)==bytes.fromhex('4183fd647507'),'growth unsigned-200 gate')
check(read(0x2b9f8a,9)==bytes.fromhex('418d5d01e84dba0700'),'growth bridge original')
new={struct.unpack('<III',hover.read(0x62A0+i*12,12))[0] for i in range(152)}
new.update([0x13a7220,0x13a6de8,0x13d2ee0,0x13d2f00,0x14471f0])
for path in (P/'src').glob('*'):
    if path.suffix not in ('.cpp','.inl','.h') or path.name in ('modpack_tables.h','native_modpack.inl'):continue
    constants={int(x,16) for x in re.findall(r'0[xX][0-9a-fA-F]+',path.read_text(encoding='utf-8'))}
    check(not (new&constants),'possible pointer overlap '+path.name+str(new&constants))
for slot,original in [(0x13a7220,0x23eb50),(0x13a6de8,0x23fb90),(0x13d2ee0,0x421960),(0x13d2f00,0x421990),(0x14471f0,0x7229c0)]:
    check(struct.unpack('<Q',game.read(slot,8))[0]==game.image_base+original,'vtable '+hex(slot))
for site,target in [(0x259682,0x368360),(0x27d976,0x368360),(0x25974b,0x295aa0),(0x27da09,0x295aa0)]:
    b=read(site,5);check(b[0]==0xe8 and site+5+struct.unpack_from('<i',b,1)[0]==target,'death call '+hex(site))
check(read(0x295c5f,8)==bytes.fromhex('85db0f84ce080000'),'death history gate')
check(read(0x422974,11)==bytes.fromhex('8b8b74050000413bc80f83'),'scenery state 3 unsigned frame gate')
result=dict(status='modpack_static_fixture_passed',checks=checks,pointer_overlap=False,
    game_sha256=hashlib.sha256(game.data).hexdigest(),gameplay_verified=False)
(P/'build/modpack_static_fixture.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result))
