import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Offline CE KO evidence. Never starts a game or opens a process."""
from pathlib import Path
import hashlib
import json
import re
import struct

P=Path(__file__).resolve().parents[1]
b=(P/'build/ui_native_analysis/editor83_b25953386/game.exe.unpacked.exe').read_bytes()
capture=(P/'build/ui_native_analysis/fixture/1a4a447d_text.bin').read_bytes()
checks=0
def check(ok,label):
    global checks
    checks+=1
    assert ok,label
check(hashlib.sha256(b).hexdigest()=='1a4a447d14e68c40ad2a5107c29c7341c10eee8fc297a7b35032900c341dbcaa','decoded identity')
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
# User 2026-10-28 live PID9808: modpack 0x231AFB and advisor 930D20 relay made
# guards fail and disabled both settings. Known same-mod windows hash as original.
inl=(P/'src/native_diplomacy_intel.inl').read_text(encoding='utf-8-sig')
modpack_src=(P/'src/native_modpack.inl').read_text(encoding='utf-8-sig')
# 2026-11-06 G1-d: the modpack ability-cap window is no longer a fixed table row; both modules find it at run time (JE rel8, NOP3,
# target MOV ESI,100) and the diplomacy guard treats the 5-byte target as original when it holds the MOV or an E9 relay jump.
R=0x231AFB
check('unsigned r=0x231AFB;' in inl and 'unsigned r=0x231AFB;' in modpack_src,'both modules anchor the short-form window at the same address')
CLAMP=R+2+struct.unpack('<b',read(R+1,1))[0]
check(read(R,1)==b'\x74' and read(R+2,3)==bytes([0x0F,0x1F,0x00]) and R+5<CLAMP<R+0x80 and read(CLAMP,5)==bytes([0xBE,0x64,0,0,0]),'modpack short-form window found by pattern in the current image (JE rel8, NOP3, target MOV ESI,100)')
check('image[t]==0xE9 || !ga::mcmp_at(image,t,mov,5)?t:0' in inl and 'return mov[rva-t]' in inl,'modpack window accepted (original MOV or E9 relay)')
check(read(0x231770,0x4B5)[CLAMP-0x231770:CLAMP-0x231770+5]==bytes([0xBE,0x64,0,0,0]),'the window lies inside the 231770 guard range')
check('{0x9332B0,5,{0x48,0x89,0x5C,0x24,0x08},{0xE9},true}' in inl,'advisor relay window accepted')
check(read(0x9332B0,5)==bytes([0x48,0x89,0x5C,0x24,0x08]),'known advisor window original bytes')
advisor=(P/'src/native_advisor_briefing.inl').read_text(encoding='utf-8-sig')
check('prepare_link(text_link,0x9332B0,0xE9,relay)' in advisor,'advisor 930D20 uses JMP relay')
def masked(a,n,patches):
    data=bytearray(read(a,n))
    for pa,pb in patches:
        if a<=pa and pa+len(pb)<=a+n:data[pa-a:pa-a+len(pb)]=pb
    return bytes(data)
def emulate(a,n,patches):
    data=bytearray(masked(a,n,patches))
    for ka,orig,alt,anyjmp in [(CLAMP,bytes([0xBE,0x64,0,0,0]),b'\xe9',True),(0x9332B0,bytes([0x48,0x89,0x5C,0x24,0x08]),b'\xe9',True)]:
        if a<=ka<a+n:
            w=bytes(data[ka-a:ka-a+len(orig)])
            if w==orig or (w[0]==0xE9 if anyjmp else w==alt):data[ka-a:ka-a+len(orig)]=orig
            # local10 G4 (G1-d): the source no longer rejects a foreign change in a known window by itself (the window moves with the build);
            # the bytes stay as found and the guard hash must then differ from the original's.
    return fnv(data)
guards={int(x,16):(int(y,16),int(z,16)) for x,y,z in re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+),0x([0-9A-F]+)ull\}',code)}
patched=[(CLAMP,b'\xe9\x11\x22\x33\x44'),(0x9332B0,b'\xe9\xdb\xf2\x58\xff')]
check(emulate(0x231770,*[guards[0x231770][0]],patched)==guards[0x231770][1],'22FCE0 guard passes with modpack ability cap')
check(emulate(0x9332B0,*[guards[0x9332B0][0]],patched)==guards[0x9332B0][1],'930D20 guard passes with advisor relay')
check(emulate(0x231770,guards[0x231770][0],[(CLAMP,b'\x90'*5)])!=guards[0x231770][1],'unknown change in known window still rejected (hash differs)')
check(emulate(0x9332B0,guards[0x9332B0][0],[(0x9332B0,b'\xcc\x90\x90\x90\x90')])!=guards[0x9332B0][1],'non-jump change at 930D20 still rejected (hash differs)')
check(fnv(masked(0x231770,guards[0x231770][0],[(0x231780,b'\xcc')]))!=guards[0x231770][1],'change outside known window still rejected')
# User 2026-11-03: verify the native calls and the hidden-mode exclusion independently.
for at in (0x816FB4,0x81711E):
    check(read(at,1)==b'\xe8' and at+5+struct.unpack('<i',read(at+1,4))[0]==0x858910,'original phase-seven resumes the footer with 858910 (former 856620)')
check(read(0x85881C,7)==bytes.fromhex('ffc883f8010f86'),'generic footer opener excludes modes one and two')
check(struct.unpack('<Q',read(0x1460C70,8))[0]==0x140813940,'frame hook retains native four-argument state handler')

check(read(0x326F48,4)==bytes.fromhex('3bc77d70'),'native resolver uses strict draw < chance after the original draw')
check(read(0x818EC6,5)==bytes.fromhex('ba01000000'),'tail gate replaces one whole MOV EDX,1 after the resolver result is consumed')
check(read(0x818EC6,5)==capture[0x818EC6-0x1000:0x818ECB-0x1000],'independent retained tail instruction')
check(read(0x818ECB,3)==bytes.fromhex('488bce') and read(0x818F77,4)==bytes.fromhex('488b4590'),'normal resume restores RSI argument; skipped UI path resumes at native container cleanup')
source=(P/'src/native_diplomacy_intel.inl').read_text(encoding='utf-8')
check('number(button,0x40)|=2;' in source and 'number(child,0x40)|=2;' in source,'decision visibility matches native phase-seven OR, not an inverted clear')
check('absolute_jump(at+21,cleanup)' in source and 'absolute_jump(at+40,resume)' in source and 'restore_link(final_tail_link)' in source,'deferred-only tail gate retains both native continuations and publication rollback')
# 2026-11-07: former decomp_cache asm checks re-derived from the current build's ghidra export (calls.tsv/datarefs.tsv) and image bytes.
G=P/'research/ghidra_full/1a4a447d14e68c40'
gcalls=(G/'calls.tsv').read_text(encoding='utf-8');gdata=(G/'datarefs.tsv').read_text(encoding='utf-8')
for row in ('00818600\t00818A4D\t00806DA0\tdirect','00818600\t00818E96\t00326ED0\tdirect','00818600\t00818F1E\t\tindirect','00813940\t00813A6D\t00818600\tdirect','00324BC0\t00324C10\t00231770\tdirect'):
    check(row in gcalls,'assembly ABI/flow (ghidra calls) '+row.replace('\t',' '))
check(read(0x818F1E,6)==bytes.fromhex('ff9090010000'),'phase-seven virtual call [rax+0x190]')
for at in (0x324C76,0x324C90):
    check(read(at,4)==bytes.fromhex('f20f5e15') and ('%08X\t00324BC0\t01345C10\tREAD'%at) in gdata,'rate function divides by the amount divisor (ghidra datarefs) %X'%at)
check(read(0x324DE3,4)==bytes.fromhex('f20f2cc8'),'rate function converts with CVTTSD2SI ECX,XMM0')
check(struct.unpack('<d',read(0x1345c10,8))[0]==100,'native amount divisor')
check(struct.unpack('<d',read(0x13b5b48,8))[0]==10000,'native rounding scale')
def ghidra_c(rva):   # decompiled body of FUN_140<rva> from the current build's ghidra export
    pat=re.compile(r'\n[A-Za-z_][^\n(]*FUN_140%06x\('%rva)
    for f in sorted((G/'c').glob('*.c')):
        t=f.read_text(encoding='utf-8',errors='replace');m=pat.search(t)
        if m:return t[m.start():t.find('\n}\n',m.start())]
    raise KeyError(hex(rva))
# the four per-screen builders (former 82C960/82C510/82CD60/82D170) in the current build, found by their control-id sets
for original,ids in [('0082F030',[0x3c,0x42,0x4d,0x49]),('0082EB90',[0x4a,0x50,0x5b,0x57]),('0082F490',[0x7a,0x80,0x8b,0x87]),('0082F8F0',[0x4a,0x50,0x5b,0x57])]:
    c=ghidra_c(int(original,16))
    check('param_1 + 0x1d8' in c,'screen builder reads the owner state '+original)
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
# the 12 vtable slots and 8 call sites of the profile were checked above directly against this image (source ids are the current build's addresses since the 2026-11-07 re-base).
print(json.dumps(dict(status='diplomacy_intel_native_verified',checks=checks,code_guards=32,slots=12,sites=8,inline_gates=1,live_game=False)))
