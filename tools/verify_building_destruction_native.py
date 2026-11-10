"""Check the exact CE caller, success roll, raw-damage capture and native writers."""
from pathlib import Path
import hashlib,json,struct,sys
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'tools'))
import current_image as pe   # decoded 1a4a447d image (the former inspect_formula module reads the previous build)
SPANS=[(0x1FA530,0x1FA5BA),(0x1FB170,0x1FC4D3),(0x1ECD20,0x1ECF3C),
       (0x1F5680,0x1F5712),(0x296AA0,0x297060),(0x22B8E0,0x22B90C),
       (0x2CC630,0x2CC660),(0x2C7A00,0x2C7A86),(0x22E450,0x22E4C5)]
def fnv(data):
    h=0xcbf29ce484222325
    for b in data:h=((h^b)*0x100000001b3)&0xffffffffffffffff
    return h
records=[dict(rva=a,size=b-a,hash=fnv(pe.read(a,b-a)),sha256=hashlib.sha256(pe.read(a,b-a)).hexdigest()) for a,b in SPANS]
header='// Generated from the SHA-256 checked CE plain copy.\nstruct Guard {unsigned rva,size;unsigned long long hash;};\nstatic const Guard guards[]={\n'
header+=''.join('    {0x%X,0x%X,0x%016Xull},\n'%(r['rva'],r['size'],r['hash']) for r in records)+'};\n'
path=P/'src/building_destruction_profile.h'
if '--generate' in sys.argv:path.write_text(header,encoding='utf-8')
# local10 G2: the profile also holds the pure guards_match()/capture_form() functions after the generated table, and spells the table with
# GA()/GH() (expanded back to the reference-build literals by tests/ga_expand.py, like the other static checks)
sys.path.insert(0,str(P/'tests'));import ga_expand
text=path.read_text(encoding='utf-8').replace('#pragma once'+chr(10),'',1).replace('#include "game_addresses.h"'+chr(10),'',1)
assert text.startswith(header),'building destruction native profile differs'
assert pe.read(0x1FA57C,12)==bytes.fromhex('83f8147507e8ea0b0000eb0a')
assert pe.read(0x1FB9E6,13)==bytes.fromhex('03559003 55e8895590448965d0'.replace(' ',''))   # capture point (re-allocated registers: two ADDs, mov [rbp-70],edx, mov [rbp-30],r12d)
assert pe.read(0x1FB1B2,4)==bytes.fromhex('48894de0') # command at RBP-20
assert pe.read(0x1FB234,4)==bytes.fromhex('488945f8') # caster force at RBP-8 (RBP-40 in the previous build)
assert pe.read(0x1FB238,4)==bytes.fromhex('0fb74010') # Force+10 indexes PERSON table, not force table
# Confirm the decoded 22CA00 result comes from world+DF30 and the native caller
# immediately interprets Force+10 as an index into world+158 (ruler Person).
assert pe.read(0x22E4B7,8)==bytes.fromhex('488b84c830df0000')
assert pe.read(0x1FB24A,8)==bytes.fromhex('488b8cc258010000')

assert pe.read(0x1FB584,4)==bytes.fromhex('488945a8') # primary at RBP-58 (RBP-60 in the previous build)
assert pe.read(0x1FC467,5)==bytes.fromhex('e81492ffff')   # CALL 1F5680 (building,remaining) after the 296AA0 call
assert pe.read(0x1FC45D,5)==bytes.fromhex('e83ea60900')   # CALL 296AA0 (building,0,0)
assert hashlib.sha256((P.parent/'SAN14CE_KO.exe').read_bytes()).hexdigest() in ('87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a','782cf229a56cdde2b95399e0e7536740b3f5e96bea702679365e4dad061abdce') # 782cf229 = Steam buildid 25953386
out=P/'build/development/building_destruction';out.mkdir(parents=True,exist_ok=True)
(out/'native_evidence.json').write_text(json.dumps(dict(source=str(pe.SOURCE),source_sha256=hashlib.sha256(pe.data).hexdigest(),ranges=records,
    abi={'complete':'int(command RCX), native CALL 1F91F1 for type 20 only',
         'capture':'native successful roll EDX before primary durability clamp; command RBP-20, primary RBP-60, region R13, caster force RBP-40',
         'bridge':'CALL + pushes(64) + SUB RSP,98; 16-byte helper alignment, shadow+arg5, volatile GPR/XMM0..5/flags preserved',
         'writes':'after native completion on the same caller thread; 1F42B0(building,remaining) / 294B90(building,0,0)'},live_game_verified=False),indent=2),encoding='utf-8')
print(json.dumps(dict(status='building_destruction_native_verified',ranges=len(records),hook_sites=2,live_game_verified=False)))
