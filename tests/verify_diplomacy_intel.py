"""Offline CE KO evidence. Never starts a game or opens a process."""
from pathlib import Path
import hashlib
import json
import re
import struct

P=Path(__file__).resolve().parents[1]
b=(P/'build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe').read_bytes()
capture=(P/'legacy/FogMod_Source/native_game_analysis/running_code_text.bin').read_bytes()
checks=0
def check(ok,label):
    global checks
    checks+=1
    assert ok,label
check(hashlib.sha256(b).hexdigest()=='1fbe6be7466a66466fe97cf3ab7d49392913346879c7d40de7f4eaf86eef81c6','decoded identity')
check(hashlib.sha256((P.parent/'SAN14CE_KO.exe').read_bytes()).hexdigest()=='87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a','original Steam identity')
pe=struct.unpack_from('<I',b,60)[0]
st=pe+24+struct.unpack_from('<H',b,pe+20)[0]
sects=[struct.unpack_from('<IIII',b,st+i*40+8) for i in range(struct.unpack_from('<H',b,pe+6)[0])]
def read(a,n):
    for _,va,rs,ro in sects:
        if va<=a and a+n<=va+rs:return b[ro+a-va:ro+a-va+n]
    raise ValueError(hex(a))
def fnv(data):
    h=0xcbf29ce484222325
    for v in data:h=((h^v)*0x100000001b3)&0xffffffffffffffff
    return h
profile=(P/'src/diplomacy_intel_profile.h').read_text(encoding='utf-8')
code,slots,sites=profile.split('static const Slot slots[]={')[0],profile.split('static const Slot slots[]={')[1].split('};')[0],profile.split('static const Site sites[]={')[1].split('};')[0]
for aa,nn,hh in re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+),0x([0-9A-F]+)ull\}',code):
    a,n,h=int(aa,16),int(nn,16),int(hh,16)
    check(fnv(read(a,n))==h,f'code hash {aa}')
    check(read(a,n)==capture[a-0x1000:a-0x1000+n],f'retained capture {aa}')
for aa,tt in re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+)\}',slots):
    a,t=int(aa,16),int(tt,16)
    check(struct.unpack('<Q',read(a,8))[0]==0x140000000+t,f'original vtable slot {aa}')
for aa,tt in re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+)\}',sites):
    a,t=int(aa,16),int(tt,16)
    check(read(a,1)==b'\xe8' and a+5+struct.unpack('<i',read(a+1,4))[0]==t,f'original call {aa}')
cache=P/'research/decomp_cache/1fbe6be7466a6646'
for address,fragments in {
    '00815EA0':['1408162e1 CALL 0x140804770','140816726 CALL 0x140324e20','1408167ae CALL qword ptr [RAX + 0x190]'],
    '00811210':['14081133d CALL 0x140815ea0'],
    '00322B30':['140322b80 CALL 0x14022fce0','140322bee DIVSD XMM1,qword ptr [0x141342c10]','140322d34 CVTTSD2SI ECX,XMM0'],
}.items():
    asm=(cache/(address+'_asm.txt')).read_text(encoding='utf-8')
    for s in fragments:check(s in asm,'assembly ABI/flow '+s)
check(struct.unpack('<d',read(0x1342c10,8))[0]==100,'native amount divisor')
check(struct.unpack('<d',read(0x13b2ad8,8))[0]==10000,'native rounding scale')
for original,ids in [('0082C960',[0x3c,0x42,0x4d,0x49]),('0082C510',[0x4a,0x50,0x5b,0x57]),('0082CD60',[0x7a,0x80,0x8b,0x87]),('0082D170',[0x4a,0x50,0x5b,0x57])]:
    c=(cache/(original+'_c.txt')).read_text(encoding='utf-8')
    for i in ids:check(hex(i) in c,'screen-specific native control '+original+':'+hex(i))
for key in ['diplomacy_overinvest','diplomacy_counterturn']:
    for rel in ['installer/InstallerGui.cs','distribution/FeatureSelect.ps1','FeatureSelect.ps1']:
        check(key in (P/rel).read_text(encoding='utf-8-sig'),'catalog '+key+' '+rel)
adapter=(P/'src/native_adapter.cpp').read_text(encoding='utf-8-sig')
check('f.diplomacy_overinvest = on && feature_key' in adapter,'parent gate')
check('f.diplomacy_counterturn = on && feature_key' in adapter,'parent gate')
print(json.dumps(dict(status='diplomacy_intel_native_verified',checks=checks,code_guards=30,slots=11,sites=7,live_game=False)))
