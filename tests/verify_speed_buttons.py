"""Independent check of speed-button and layout-wait guard constants against the
DRM-free analysis EXE and the captured running .text, plus the ScreenLayout patch."""
from pathlib import Path
import base64,json,re,struct,sys
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
src=(P/'src/native_speed_buttons.inl').read_text(encoding='utf-8')
num=lambda name:int(re.search(name+r'=0x([0-9A-Fa-f]+)',src).group(1),16)
checks=0;fail=[]
def ok(cond,what):
    global checks;checks+=1
    if not cond:fail.append(what)
for fn_,size,h in (('kSetup',0x4C8,'kSetupHash'),('kClick',0xBB,'kClickHash'),('kFrame',0x9C4,'kFrameHash'),('kHover',0xF6,'kHoverHash')):
    rva=num(fn_);want=num(h)
    ok(fnv(rd(rva,size))==want,fn_+' exe fingerprint');ok(fnv(run(rva,size))==want,fn_+' running fingerprint')
for slot,target in (('kSetupSlot','kSetup'),('kFrameSlot','kFrame'),('kHoverSlot','kHover')):
    ok(struct.unpack_from('<Q',rd(num(slot),8))[0]-0x140000000==num(target),slot)
ok(rd(0x898950,16)==bytes.fromhex('488b12488b4908e9c48dffffcccccccc'),'functor tail jump bytes')
ok(0x898957+5+struct.unpack_from('<i',rd(0x898958,4))[0]==num('kClick'),'tail jump target')
ok(struct.unpack_from('<3i',rd(num('kIntervalTable'),12))==(8,2,1),'interval table')
ok(struct.unpack_from('<Q',rd(num('kFunctorVtable')+0x10,8))[0]-0x140000000==0x898950,'functor _Do_call')
lw=(P/'src/native_layout_wait.inl').read_text(encoding='utf-8')
for site in (0x676767,0x86CFA5,0x86DEC9):
    b=rd(site,5);ok(b[0]==0xE8 and site+5+struct.unpack_from('<i',b,1)[0]==0x86C570,'layout load call %X'%site)
wb=bytes(int(x,16) for x in re.search(r'kWrapperBytes\[0x33\]=\{([^}]*)\}',lw).group(1).replace('\n','').split(','))
ok(rd(0x86E0C0,len(wb))==wb and run(0x86E0C0,len(wb))==wb,'86E0C0 wrapper bytes')
patch=json.loads((P/'research/speed_buttons/layout_patch/traits9_layout.json').read_text(encoding='utf-8'))
patch_bank=(P/'research/speed_buttons/layout_patch/traits9_layout.bin').read_bytes()
ok(len(patch['records'])==8 and {r['ktid'] for r in patch['records']}>={0x301526D7,0xFEA5F28C,0x40F993CE,0x454A1BCC},'patch records')
ok(all(patch_bank[r['offset']+r['size']-4:r['offset']+r['size']]==bytes(4) for r in patch['records']),'zero-length terminator chunk')
rep=sorted(int(k) for k in patch['speed_buttons']['repointed'])
code=[int(x) for x in re.search(r'kNormal\[2\]=\{(\d+),(\d+)\},kChecked\[2\]=\{(\d+),(\d+)\}',src).groups()]
ok(rep==sorted(code),'repointed sprites match code')
for r in patch['records'][4:]:
    e=base64.b64decode(r['original_entry']);ok(struct.unpack_from('<I',e,36)[0]==r['ktid'],'original index entry %08X'%r['ktid'])
oc=(P/'src/native_occupy_fast.inl').read_text(encoding='utf-8')
on=lambda name:int(re.search(name+r'=0x([0-9A-Fa-f]+)',oc).group(1),16)
for rva,size,h in ((on('kUpdate'),0xE8,'kUpdateHash'),(0x42B310,0x217,'kCameraPhaseHash'),(0x42C190,0xF7,'kEffectPhaseHash'),(0x42BB90,0x38C,'kTilePhaseHash'),(0x426DA0,0x157,'kLastPhaseHash')):
    ok(fnv(rd(rva,size))==on(h) and fnv(run(rva,size))==on(h),'occupy %X fingerprint'%rva)
ok(struct.unpack_from('<Q',rd(on('kUpdateSlot'),8))[0]-0x140000000==on('kUpdate'),'occupy update slot')
b=rd(on('kCameraCall'),5);ok(b[0]==0xE8 and on('kCameraCall')+5+struct.unpack_from('<i',b,1)[0]==on('kCamera') and on('kCameraCall')%16<=11,'occupy camera call')
ok(rd(0x42C21C,3)==bytes.fromhex('83f946') and rd(0x42C1C6,3)==bytes.fromhex('83f921'),'occupy effect thresholds 0x46/0x21')
# v12.8 counts four per frame: native waits use INC + CMP + JLE (greater-than), so overshoot is safe.
ok(rd(0x42BC28,15)==bytes.fromhex('ff87c005000083bfc0050000190f8e'),'phase 6 counter > 0x19')
ok(rd(0x426E08,15)==bytes.fromhex('ff83c005000083bbc00500001e0f8e'),'phase 7 counter > 0x1E')
print(json.dumps({'status':'speed_buttons_static_verified' if not fail else 'failed','checks':checks,'failures':fail,'gameplay_verified':False}))
sys.exit(1 if fail else 0)
