"""Current CE bytes/ABI sites for the display-only forecast feature."""
from pathlib import Path
import hashlib,json,struct,sys
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'data/diagnostics/combat_formula_20261021'))
import inspect_formula as pe
spans=[(0x7FA230,0x7FA763),(0x7D0460,0x7D05DD),(0x7F55D0,0x7F5795),
       (0x16A1D0,0x16A780),(0x2B21A0,0x2B2440),(0x2B2440,0x2B2684),
       (0x2A9E50,0x2AA123),(0x2AA2E0,0x2AA563),(0x18FDA0,0x18FEFD),
       (0x92F810,0x92F886),(0x22C6D0,0x22C723),(0x191760,0x191C28)]
def fnv(b):
    h=0xcbf29ce484222325
    for v in b:h=((h^v)*0x100000001b3)&0xffffffffffffffff
    return h
records=[dict(rva=a,size=b-a,hash=fnv(pe.read(a,b-a)),sha256=hashlib.sha256(pe.read(a,b-a)).hexdigest()) for a,b in spans]
header='// Generated from the verified CE plain copy; do not hand-edit addresses.\n'
header+='struct Guard {unsigned rva,size;unsigned long long hash;};\nstatic const Guard guards[]={\n'
header+=''.join('    {0x%X,0x%X,0x%016Xull},\n'%(r['rva'],r['size'],r['hash']) for r in records)+'};\n'
path=P/'src/damage_preview_profile.h'
if '--generate' in sys.argv:path.write_text(header,encoding='utf-8')
assert path.read_text(encoding='utf-8')==header,'native profile differs'
assert pe.read(0x7FA230,9)==bytes.fromhex('4885d20f8429050000')
assert pe.read(0x7FA653,5)==bytes.fromhex('e878fb96ff')
assert pe.read(0x7FA67C,5)==bytes.fromhex('e84ffb96ff')
assert pe.read(0x7FA762,1)==b'\xc3'
assert hashlib.sha256((P.parent/'SAN14CE_KO.exe').read_bytes()).hexdigest()=='87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a'
out=P/'build/development/damage_preview';out.mkdir(parents=True,exist_ok=True)
(out/'native_evidence.json').write_text(json.dumps(dict(source=str(pe.SOURCE),sha256=hashlib.sha256(pe.data).hexdigest(),ranges=records,
    abi={'refresh':'void(panel RCX, army RDX, target R8); observed native callers ignore return',
         'damage':'void(attacker RCX, attack XMM1, unused XMM2, defender R9, unused stack20, defense stack28, int* stack30)',
         'entry_bridge':'TEST RDX,RDX; JE native RET; absolute JMP entry+9; no relocated RIP data',
         'text':'930D20(table,part,const wchar_t*,1); visibility 92F810(table,part,int)'},live_ui_verified=False),indent=2),encoding='utf-8')
print(json.dumps(dict(status='damage_preview_native_verified',ranges=len(records),sites=4,live_ui_verified=False)))
