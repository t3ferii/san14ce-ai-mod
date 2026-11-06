import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
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
check(hashlib.sha256((P.parent/'SAN14CE_KO.exe').read_bytes()).hexdigest() in ('87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a','782cf229a56cdde2b95399e0e7536740b3f5e96bea702679365e4dad061abdce'),'original Steam identity') # Steam EXE: 87ed4e46 = before the 2026-11-05 update (the decoded evidence below), 782cf229 = buildid 25953386 (its decoded copy is checked at the end; nothing weakened)
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
# User 2026-10-28 live PID9808: modpack 0x230098 and advisor 930D20 relay made
# guards fail and disabled both settings. Known same-mod windows hash as original.
inl=(P/'src/native_diplomacy_intel.inl').read_text(encoding='utf-8-sig')
modpack=(P/'src/modpack_tables.h').read_text(encoding='utf-8-sig')
check('{0x230098,8,{0x74,0x48,0x66,0x0F,0x1F,0x44,0x00,0x00},{0x75,0x06,0x83,0xFE,0x64,0x7C,0x43,0x90}}' in modpack,'modpack window source')
check('{0x230098,8,{0x74,0x48,0x66,0x0F,0x1F,0x44,0x00,0x00},{0x75,0x06,0x83,0xFE,0x64,0x7C,0x43,0x90},false}' in inl,'modpack window accepted')
check('{0x930D20,5,{0x48,0x89,0x5C,0x24,0x08},{0xE9},true}' in inl,'advisor relay window accepted')
check(read(0x230098,8)==bytes([0x74,0x48,0x66,0x0F,0x1F,0x44,0,0]) and read(0x930D20,5)==bytes([0x48,0x89,0x5C,0x24,0x08]),'known windows original bytes')
advisor=(P/'src/native_advisor_briefing.inl').read_text(encoding='utf-8-sig')
check('prepare_link(text_link,0x930D20,0xE9,relay)' in advisor,'advisor 930D20 uses JMP relay')
def masked(a,n,patches):
    data=bytearray(read(a,n))
    for pa,pb in patches:
        if a<=pa and pa+len(pb)<=a+n:data[pa-a:pa-a+len(pb)]=pb
    return bytes(data)
def emulate(a,n,patches):
    data=bytearray(masked(a,n,patches))
    for ka,orig,alt,anyjmp in [(0x230098,bytes([0x74,0x48,0x66,0x0F,0x1F,0x44,0,0]),bytes([0x75,0x06,0x83,0xFE,0x64,0x7C,0x43,0x90]),False),(0x930D20,bytes([0x48,0x89,0x5C,0x24,0x08]),b'\xe9',True)]:
        if a<=ka<a+n:
            w=bytes(data[ka-a:ka-a+len(orig)])
            if w==orig or (w[0]==0xE9 if anyjmp else w==alt):data[ka-a:ka-a+len(orig)]=orig
            # local10 G4 (G1-d): the source no longer rejects a foreign change in a known window by itself (the window moves with the build);
            # the bytes stay as found and the guard hash must then differ from the original's.
    return fnv(data)
guards={int(x,16):(int(y,16),int(z,16)) for x,y,z in re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+),0x([0-9A-F]+)ull\}',code)}
patched=[(0x230098,bytes([0x75,0x06,0x83,0xFE,0x64,0x7C,0x43,0x90])),(0x930D20,b'\xe9\xdb\xf2\x58\xff')]
check(emulate(0x22FCE0,*[guards[0x22FCE0][0]],patched)==guards[0x22FCE0][1],'22FCE0 guard passes with modpack ability cap')
check(emulate(0x930D20,*[guards[0x930D20][0]],patched)==guards[0x930D20][1],'930D20 guard passes with advisor relay')
check(emulate(0x22FCE0,guards[0x22FCE0][0],[(0x230098,b'\x90'*8)])!=guards[0x22FCE0][1],'unknown change in known window still rejected (hash differs)')
check(emulate(0x930D20,guards[0x930D20][0],[(0x930D20,b'\xcc\x90\x90\x90\x90')])!=guards[0x930D20][1],'non-jump change at 930D20 still rejected (hash differs)')
check(fnv(masked(0x22FCE0,guards[0x22FCE0][0],[(0x22FD00,b'\xcc')]))!=guards[0x22FCE0][1],'change outside known window still rejected')
# User 2026-11-03: verify the native calls and the hidden-mode exclusion independently.
for at in (0x814884,0x8149EE):
    check(read(at,1)==b'\xe8' and at+5+struct.unpack('<i',read(at+1,4))[0]==0x856620,'original phase-seven resumes the footer with 856620')
check(read(0x85652C,7)==bytes.fromhex('ffc883f8010f86'),'generic footer opener excludes modes one and two')
check(struct.unpack('<Q',read(0x145DBE0,8))[0]==0x140811210,'frame hook retains native four-argument state handler')

cache=P/'research/decomp_cache/1fbe6be7466a6646'
check(read(0x324E98,4)==bytes.fromhex('3bc77d70'),'native resolver uses strict draw < chance after the original draw')
check(read(0x816756,5)==bytes.fromhex('ba01000000'),'tail gate replaces one whole MOV EDX,1 after the resolver result is consumed')
check(read(0x816756,5)==capture[0x816756-0x1000:0x81675B-0x1000],'independent retained tail instruction')
check(read(0x81675B,3)==bytes.fromhex('488bce') and read(0x816807,4)==bytes.fromhex('488b4590'),'normal resume restores RSI argument; skipped UI path resumes at native container cleanup')
source=(P/'src/native_diplomacy_intel.inl').read_text(encoding='utf-8')
check('number(button,0x40)|=2;' in source and 'number(child,0x40)|=2;' in source,'decision visibility matches native phase-seven OR, not an inverted clear')
check('absolute_jump(at+21,cleanup)' in source and 'absolute_jump(at+40,resume)' in source and 'restore_link(final_tail_link)' in source,'deferred-only tail gate retains both native continuations and publication rollback')
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
# 사용자 결정: 사용 중지 키 읽기 제거 2026-11-03.
check('f.diplomacy_overinvest = false; // retired 2026-11-03: native cap/marker restored; key no longer read' in adapter and
      not re.search(r'feature_key\(config, L"diplomacy", L"diplomacy_overinvest"',adapter),'retired key is not read')
check('f.diplomacy_counterturn = on && feature_key' in adapter,'parent gate')
# ---- local10 G4 5.2 (2026-11-06): the same feature on the decoded 1a4a447d copy (static evidence, not the live game) ----
nb=(P/'build/ui_native_analysis/editor83_b25953386/game.exe.unpacked.exe').read_bytes()
check(hashlib.sha256(nb).hexdigest()=='1a4a447d14e68c40ad2a5107c29c7341c10eee8fc297a7b35032900c341dbcaa','1a4a447d decoded identity')
npe=struct.unpack_from('<I',nb,60)[0]
nst=npe+24+struct.unpack_from('<H',nb,npe+20)[0]
nsects=[struct.unpack_from('<IIII',nb,nst+i*40+8) for i in range(struct.unpack_from('<H',nb,npe+6)[0])]
def nread(a,n):
    for _,va,rs,ro in nsects:
        if va<=a and a+n<=va+rs:return nb[ro+a-va:ro+a-va+n]
    raise ValueError(hex(a))
def nrip(at,ln):return at+ln+struct.unpack('<i',nread(at+ln-4,4))[0]
RATE=0x324BC0
check(nread(RATE+0xA9,5)==bytes.fromhex('837c246800') and nread(RATE+0xC0,4)==bytes.fromhex('660f6e05'),'324BC0 has the 8th-argument compare at +0xA9 and the movd at +0xC0')
check(nrip(RATE+0xC0,8)==0x1A4B884 and struct.unpack('<i',nread(0x1A4B884,4))[0]==70,'8th argument scales by the global 1A4B884 = 70')
# four coefficient blocks in code order k9,k8,k5,k4 (dispatch sub ebx,4/1/3, cmp ebx,1 as in the original)
check(nread(0x324CA5,3)==bytes.fromhex('83eb04') and nread(0x324CAA,3)==bytes.fromhex('83eb01') and nread(0x324CAF,3)==bytes.fromhex('83eb03') and nread(0x324CB4,3)==bytes.fromhex('83fb01'),'dispatch sub ebx,4/1/3, cmp ebx,1')
blk=[(0x324CBD,[0x1A4B700,0x1A4B708,0x1A4B714,0x27D55C4,0x1A4B728]),(0x324CE0,[0x1A4B59C,0x1A4B5A8,0x1A4B5B4,0x27D55BC,0x1A4B5C8]),
     (0x324D03,[0x1A4B79C,0x1A4B7A4,0x1A4B7B0,0x27D55D0,0x1A4B7C4]),(0x324D26,[0x1A4B534,0x1A4B540,0x1A4B54C,0x27D55B8,0x1A4B560])]
for start,want in blk:
    ops=[(b'\x8b\x0d',6),(b'\x44\x8b\x05',7),(b'\x44\x8b\x0d',7),(b'\x8b\x15',6),(b'\x44\x8b\x15',7)]
    got=[];at=start
    for op,ln in ops:
        check(nread(at,len(op))==op,'coefficient block opcode %X'%at);got.append(nrip(at,ln));at+=ln
    check(got==want,'coefficient block %X decodes to the table ids'%start)
for at,tgt in ((0x830583,RATE),(0x82FEC3,RATE),(0x830223,RATE),(0x818A0E,RATE),(0x818A4D,0x806DA0),(0x818E96,0x326ED0)):
    check(nread(at,1)==b'\xe8' and at+5+struct.unpack('<i',nread(at+1,4))[0]==tgt,'new call %X -> %X'%(at,tgt))
check(nread(0x818EC6,5)==bytes.fromhex('ba01000000') and nread(0x818ECB,3)==bytes.fromhex('488bce'),'new tail MOV EDX,1 / MOV RCX,RSI')
check(nread(0x818F76,5)==bytes.fromhex('90488b4590'),'new container cleanup: NOP then mov rax,[rbp-0x70]')
tab={e['old']:e['new'].get('1a4a447d') for e in json.loads((P/'research/rva_map/1fbe6be7_to_1a4a447d/addr_table.json').read_text(encoding='utf-8'))['entries']}
def newrva(old):
    v=tab[old]
    check(v not in (None,'0','') ,'table resolves '+old)
    return int(v,16)
for aa,tt in re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+)\}',slots):
    s,o=newrva(aa),newrva(tt)
    check(struct.unpack('<Q',nread(s,8))[0]==0x140000000+o,'new vtable slot %X -> %X'%(s,o))
for aa,tt in re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+)\}',sites):
    s,o=newrva(aa),newrva(tt)
    check(nread(s,1)==b'\xe8' and s+5+struct.unpack('<i',nread(s+1,4))[0]==o,'new call site %X -> %X'%(s,o))
print(json.dumps(dict(status='diplomacy_intel_native_verified',checks=checks,code_guards=32,slots=12,sites=8,inline_gates=1,live_game=False)))
