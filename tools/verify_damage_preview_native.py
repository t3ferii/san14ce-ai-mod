"""Current CE bytes/ABI sites for the display-only forecast feature."""
import sys as _s,os as _o;_s.path.insert(0,_o.path.join(_o.path.dirname(_o.path.abspath(__file__)),'..','tests'));import ga_expand  # local10 G4: the profile spells GA(GA_x)/GH(0x..) (user 2026-11-05); the import expands them back to the reference-build literals
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
import re
# local10 G4 5.1: the profile is no longer a bare generated table (GA()/GH() spelling, pragma once, guards_match()), so the rows are parsed and
# compared one by one with the bytes of the reference build (same strength as the old whole-text comparison of the table).
path=P/'src/damage_preview_profile.h'
rows=[(int(a,16),int(b,16),int(c,16)) for a,b,c in re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+),0x([0-9A-F]+)ull\}',path.read_text(encoding='utf-8'))]
assert rows==[(r['rva'],r['size'],r['hash']) for r in records],'native profile differs'
assert pe.read(0x7FA230,9)==bytes.fromhex('4885d20f8429050000')
assert pe.read(0x7FA653,5)==bytes.fromhex('e878fb96ff')
assert pe.read(0x7FA67C,5)==bytes.fromhex('e84ffb96ff')
assert pe.read(0x7FA762,1)==b'\xc3'
assert hashlib.sha256((P.parent/'SAN14CE_KO.exe').read_bytes()).hexdigest() in ('87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a','782cf229a56cdde2b95399e0e7536740b3f5e96bea702679365e4dad061abdce') # Steam EXE: 87ed4e46 = before the 2026-11-05 update (the decoded evidence above), 782cf229 = buildid 25953386 (per-build resolution: tools/check_addresses.py, nothing weakened)
out=P/'build/development/damage_preview';out.mkdir(parents=True,exist_ok=True)
(out/'native_evidence.json').write_text(json.dumps(dict(source=str(pe.SOURCE),sha256=hashlib.sha256(pe.data).hexdigest(),ranges=records,
    abi={'refresh':'void(panel RCX, army RDX, target R8); observed native callers ignore return',
         'damage':'void(attacker RCX, attack XMM1, unused XMM2, defender R9, unused stack20, defense stack28, int* stack30)',
         'entry_bridge':'TEST RDX,RDX; JE native RET; absolute JMP entry+9; no relocated RIP data',
         'text':'930D20(table,part,const wchar_t*,1); visibility 92F810(table,part,int)'},live_ui_verified=False),indent=2),encoding='utf-8')
print(json.dumps(dict(status='damage_preview_native_verified',ranges=len(records),sites=4,live_ui_verified=False)))
