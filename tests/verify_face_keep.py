"""Independent check of face_keep guard constants against the DRM-free analysis EXE and the captured
running .text (2026-10-25 user: army flag faces stay loaded), plus the decoded behaviour it relies on."""
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
src=(P/'src/native_face_keep.inl').read_text(encoding='utf-8')
num=lambda name:int(re.search(name+r'=0x([0-9A-Fa-f]+)',src).group(1),16)
checks=0;fail=[]
def ok(cond,what):
    global checks;checks+=1
    if not cond:fail.append(what)
for fn_,size,h in (('kFace',0x58,'kFaceHash'),('kAdd',0x5A,'kAddHash'),('kId',0xE0,'kIdHash'),('kRelease',0x4A,'kReleaseHash')):
    rva=num(fn_);want=num(h)
    ok(fnv(rd(rva,size))==want,fn_+' exe fingerprint');ok(fnv(run(rva,size))==want,fn_+' running fingerprint')
site=num('kSite');b=rd(site,5);ok(b[0]==0xE8 and site+5+struct.unpack_from('<i',b,1)[0]==num('kFace'),'army flag call 1B3523 -> 1B91E0')
ok(rd(0x1B91FA,5)[0]==0xE8 and 0x1B91FA+5+struct.unpack_from('<i',rd(0x1B91FB,4))[0]==0x32F500,'1B91E0 checks the person (32F500)')
ok(rd(0x1B9207,7)==bytes.fromhex('0fb79ba6000000'),'face = word person+A6')
ok(rd(0x1B9219,4)==bytes.fromhex('448d460a'),'kind 10 (lea r8d,[rsi+0A])')
ok(rd(0x8A7EC7,3)==bytes.fromhex('ff0c88') and rd(0x8A7EE4,1)==b'\xe9' and 0x8A7EE4+5+struct.unpack_from('<i',rd(0x8A7EE5,4))[0]==0x8ACB40,'release: count-1, unload at 0')
ok(rd(0x8A7EA0,7)==bytes.fromhex('4c8b8180010000'),'counts at manager+180')
ok(rd(0x8A83A0,12)==bytes.fromhex('4881c198010000e904000000'),'8A83A0 = table lookup (rcx+198)')
ok(struct.unpack_from('<I',rd(0x8A7EDD+3,4))[0]+0x8A7EDD+7==num('kManager'),'manager global 27D4390')
ok('kMax=256' in (P/'src/face_keep.h').read_text(encoding='utf-8'),'cap 256 (2026-10-25 user decision, was 512)')
# face_prefetch (2026-10-25 user)
ok(rd(0x8AB11B,13)==bytes.fromhex('e8709c76ff84c0751933c9ff15'),'8AB11B call 14D90; test al; jne 8AB13D; Sleep(0) loop')
ok(0x8AB11B+5+struct.unpack_from('<i',rd(0x8AB11C,4))[0]==num('kPump') and 0x8AB134+5+struct.unpack_from('<i',rd(0x8AB135,4))[0]==num('kPump'),'both loop calls go to 14D90')
ok(rd(0x8AB0FA,1)==bytes([0xE8]) and 0x8AB0FA+5+struct.unpack_from('<i',rd(0x8AB0FB,4))[0]==0xB393B0,'job created by B393B0 just before the wait')
ok(rd(0x8AB13D,10)==bytes.fromhex('8bd5488bcee819cdffff'),'after the wait: 8A7E60(manager,id)')
ok(rd(0x8AB180,7)==bytes.fromhex('488b8678010000') and rd(0x8AB199,4)==bytes.fromhex('48897a08'),'entry at manager+178[id], job at +8')
lo=num('kLoad');ok(fnv(rd(lo,0x8AB216-lo))==num('kLoadHash') and fnv(run(lo,0x8AB216-lo))==num('kLoadHash'),'8AAD70 fingerprint exe/running')
body=rd(0x25C070,0x400);hit=[i for i in range(len(body)-7) if body[i:i+3] in (bytes.fromhex('488b05'),bytes.fromhex('488b0d'),bytes.fromhex('488b15'),bytes.fromhex('4c8b05'),bytes.fromhex('4c8b0d')) and 0x25C070+i+7+struct.unpack_from('<i',body,i+3)[0]==num('kPersons')]
ok(len(hit)>=1,'25C070 reads the person table global 277DF18')
print(json.dumps({'status':'face_keep_static_verified' if not fail else 'failed','checks':checks,'failures':fail,'gameplay_verified':False}))
sys.exit(1 if fail else 0)
