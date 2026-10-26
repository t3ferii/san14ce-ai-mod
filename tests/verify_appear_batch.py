"""Independent check of appear_batch guard constants against the DRM-free analysis EXE and the
captured running .text (2026-10-25 user: several armies per frame), plus the decoded behaviour the
module relies on: one army per call, +480 rest, 22/11 ms budget, list +478, finish via 332E90."""
from pathlib import Path
import json,re,struct,sys
P=Path(__file__).resolve().parents[1]
exe=(P/'build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe').read_bytes()
text=(P/'legacy/FogMod_Source/native_game_analysis/running_code_text.bin').read_bytes()
pe=struct.unpack_from('<I',exe,0x3c)[0];n=struct.unpack_from('<H',exe,pe+6)[0];so=struct.unpack_from('<H',exe,pe+20)[0]
S=[struct.unpack_from('<IIII',exe,pe+24+so+i*40+8) for i in range(n)]
def rd(rva,size):
    for vs,va,rs,ro in S:
        if va<=rva<va+max(vs,rs):return exe[ro+rva-va:ro+rva-va+size]
    raise KeyError(hex(rva))
def run(rva,size):return text[rva-0x1000:rva-0x1000+size]
def fnv(b):
    h=0xcbf29ce484222325
    for c in b:h=((h^c)*0x100000001b3)&0xffffffffffffffff
    return h
src=(P/'src/native_appear_batch.inl').read_text(encoding='utf-8')
num=lambda name:int(re.search(name+r'=0x([0-9A-Fa-f]+)',src).group(1),16)
checks=0;fail=[]
def ok(cond,what):
    global checks;checks+=1
    if not cond:fail.append(what)
for fn_,size,h in (('kUpdate',0xEF,'kUpdateHash'),('kStop',0x39,'kStopHash')):
    rva=num(fn_);want=num(h)
    ok(fnv(rd(rva,size))==want,fn_+' exe fingerprint');ok(fnv(run(rva,size))==want,fn_+' running fingerprint')
ok(struct.unpack_from('<Q',rd(num('kSlot'),8))[0]-0x140000000==num('kUpdate'),'slot 5 -> 44AC80')
# RTTI of the vtable holding the slot: CAppearArmyState.
vt=num('kSlot')-0x28;col=struct.unpack_from('<Q',rd(vt-8,8))[0]-0x140000000
td=struct.unpack_from('<I',rd(col+12,4))[0];ok(rd(td+16,24).startswith(b'.?AVCAppearArmyState@@'),'vtable RTTI CAppearArmyState')
ok(rd(0x44AC96,7)==bytes.fromhex('83bb8004000000'),'rest check cmp [rbx+480],0')
ok(rd(0x44AC9F,10)==bytes.fromhex('c7838004000000000000'),'rest clear')
ok(rd(0x44ACB4,5)[0]==0xE8 and 0x44ACB4+5+struct.unpack_from('<i',rd(0x44ACB5,4))[0]==num('kStop'),'finish condition call 332E90')
ok(rd(0x44ACC7,8)==bytes.fromhex('4883bb7804000000'),'list check [rbx+478]')
ok(rd(0x44AD1C,5)[0]==0xE8 and 0x44AD1C+5+struct.unpack_from('<i',rd(0x44AD1D,4))[0]==0x25C070,'one army: 25C070')
ok(rd(0x44AD14,5)==bytes.fromhex('ba10bd0000'),'army argument 0xBD10')
ok(rd(0x44AD2A,5)[0]==0xE8 and 0x44AD2A+5+struct.unpack_from('<i',rd(0x44AD2B,4))[0]==0x332E40,'budget check 332E40')
ok(rd(0x44AD3D,6)==bytes.fromhex('89ab80040000'),'rest set +480')
ok(rd(0x44AD57,18)==bytes.fromhex('488b8378040000488b480848898b78040000'),'list advance +478 = node+8')
ok(rd(0x332E53,5)==bytes.fromhex('bf16000000') and rd(0x332E70,5)==bytes.fromhex('bf0b000000'),'budget 22/11 ms')
ok('kBudgetUs=50000' in src,'mod budget 50 ms (2026-10-25 user decision: 12 -> 100 -> 50 ms, 100 ms felt choppy)')
print(json.dumps({'status':'appear_batch_static_verified' if not fail else 'failed','checks':checks,'failures':fail,'gameplay_verified':False}))
sys.exit(1 if fail else 0)
