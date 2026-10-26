"""Check the exact CE caller, success roll, raw-damage capture and native writers."""
from pathlib import Path
import hashlib,json,struct,sys
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'data/diagnostics/combat_formula_20261021'))
import inspect_formula as pe
SPANS=[(0x1F91A0,0x1F922A),(0x1F9DE0,0x1FB143),(0x1EB980,0x1EBB9C),
       (0x1F42B0,0x1F4342),(0x294B90,0x295150),(0x229EA0,0x229ECC),
       (0x2CA280,0x2CA2B0),(0x2C5650,0x2C56D6),(0x22CA00,0x22CA75)]
def fnv(data):
    h=0xcbf29ce484222325
    for b in data:h=((h^b)*0x100000001b3)&0xffffffffffffffff
    return h
records=[dict(rva=a,size=b-a,hash=fnv(pe.read(a,b-a)),sha256=hashlib.sha256(pe.read(a,b-a)).hexdigest()) for a,b in SPANS]
header='// Generated from the SHA-256 checked CE plain copy.\nstruct Guard {unsigned rva,size;unsigned long long hash;};\nstatic const Guard guards[]={\n'
header+=''.join('    {0x%X,0x%X,0x%016Xull},\n'%(r['rva'],r['size'],r['hash']) for r in records)+'};\n'
path=P/'src/building_destruction_profile.h'
if '--generate' in sys.argv:path.write_text(header,encoding='utf-8')
assert path.read_text(encoding='utf-8')==header,'building destruction native profile differs'
assert pe.read(0x1F91EC,12)==bytes.fromhex('83f8147507e8ea0b0000eb0a')
assert pe.read(0x1FA607,8)==bytes.fromhex('8955f0448b642478')
assert pe.read(0x1F9E22,4)==bytes.fromhex('48894de0') # command at RBP-20
assert pe.read(0x1F9EA4,4)==bytes.fromhex('488945c0') # caster force at RBP-40
assert pe.read(0x1F9EA8,4)==bytes.fromhex('0fb74010') # Force+10 indexes PERSON table, not force table
# Confirm the decoded 22CA00 result comes from world+DF30 and the native caller
# immediately interprets Force+10 as an index into world+158 (ruler Person).
assert pe.read(0x22CA67,8)==bytes.fromhex('488b84c830df0000')
assert pe.read(0x1F9EBA,8)==bytes.fromhex('488b8cc258010000')

assert pe.read(0x1FA1F2,7)==bytes.fromhex('488b3e48897da0') # primary at RBP-60
assert pe.read(0x1FB080,5)==bytes.fromhex('e82b92ffff')
assert pe.read(0x1FB076,5)==bytes.fromhex('e8159b0900')
assert hashlib.sha256((P.parent/'SAN14CE_KO.exe').read_bytes()).hexdigest()=='87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a'
out=P/'build/development/building_destruction';out.mkdir(parents=True,exist_ok=True)
(out/'native_evidence.json').write_text(json.dumps(dict(source=str(pe.SOURCE),source_sha256=hashlib.sha256(pe.data).hexdigest(),ranges=records,
    abi={'complete':'int(command RCX), native CALL 1F91F1 for type 20 only',
         'capture':'native successful roll EDX before primary durability clamp; command RBP-20, primary RBP-60, region R13, caster force RBP-40',
         'bridge':'CALL + pushes(64) + SUB RSP,98; 16-byte helper alignment, shadow+arg5, volatile GPR/XMM0..5/flags preserved',
         'writes':'after native completion on the same caller thread; 1F42B0(building,remaining) / 294B90(building,0,0)'},live_game_verified=False),indent=2),encoding='utf-8')
print(json.dumps(dict(status='building_destruction_native_verified',ranges=len(records),hook_sites=2,live_game_verified=False)))
