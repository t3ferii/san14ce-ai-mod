"""CE KO offline evidence. No process attachment, game launch or live writes."""
from pathlib import Path
import hashlib,json,re,struct
P=Path(__file__).resolve().parents[1]
b=(P/'build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe').read_bytes()
capture=(P/'legacy/FogMod_Source/native_game_analysis/running_code_text.bin').read_bytes()
checks=0
def check(ok,why):
    global checks
    checks+=1
    assert ok,why
check(hashlib.sha256(b).hexdigest()=='1fbe6be7466a66466fe97cf3ab7d49392913346879c7d40de7f4eaf86eef81c6','decoded image identity')
check(hashlib.sha256((P.parent/'SAN14CE_KO.exe').read_bytes()).hexdigest()=='87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a','original Steam image identity')
pe=struct.unpack_from('<I',b,60)[0];st=pe+24+struct.unpack_from('<H',b,pe+20)[0]
sections=[struct.unpack_from('<IIII',b,st+i*40+8) for i in range(struct.unpack_from('<H',b,pe+6)[0])]
def read(a,n):
    for _,va,size,offset in sections:
        if va<=a and a+n<=va+size:return b[offset+a-va:offset+a-va+n]
    raise ValueError(hex(a))
def fnv(data):
    h=0xcbf29ce484222325
    for x in data:h=((h^x)*0x100000001b3)&0xffffffffffffffff
    return h
profile=(P/'src/advisor_briefing_profile.h').read_text(encoding='utf-8')
for a,n,h in re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+),0x([0-9A-F]+)ull\}',profile):
    a,n,h=int(a,16),int(n,16),int(h,16)
    check(fnv(read(a,n))==h,f'current function bytes {a:X}')
    check(read(a,n)==capture[a-0x1000:a-0x1000+n],f'independent retained capture {a:X}')
slots=[]
for a,t in re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+)\}',profile.split('static const Slot slots[]={')[1]):
    a,t=int(a,16),int(t,16);slots.append(a)
    check(struct.unpack('<Q',read(a,8))[0]==0x140000000+t,f'exact vtable slot {a:X}')
check(len(slots)==14,'only fourteen exact informational/lifecycle slots')
check(read(0x930D20,5)==bytes.fromhex('48895c2408'),'text relay displaces exactly one five-byte MOV; no RSP or RIP change')
check(read(0x86149C,6)==bytes.fromhex('899078010000'),'native +190 stores EDX in dialog+178')
check(read(0x8614A2,1)==b'\xe9' and 0x8614A7+struct.unpack('<i',read(0x8614A3,4))[0]==0x856840,'native acknowledgement starts the original closing animation, beyond storing the result')
check(struct.unpack('<8I',read(0x14311A0,32))==(909,909,910,911,912,913,914,915),'income district headings match native label table')
cache=P/'research/decomp_cache/1fbe6be7466a6646'
for a,fragments in {
    '00862E40':['140862e74 MOV R8,qword ptr [RCX + 0x4b0]','140862e92 CALL 0x140842380','140862ea3 JMP 0x140012540'],
    '0067B420':['0x480','0x470','FUN_14067c990(param_1,0)'],
    '0067C990':['param_1 + 0x488','param_1 + 0x490'],
    '00680650':['FUN_14067d1f0(param_1,lVar3)'],
    '0067D1F0':['FUN_140930d20','param_1 + 0x378'],
    '00677D70':['local_90 = 0x394'],
    '0056FE50':['param_1 + 0x10','param_1 + 0x20','0x70','bool FUN_14056fe50'],
    '00676870':['0x82','0x16280','0x1b00','0x140'],
    '002417A0':['param_1 + 0x8a','0x55'],
    '0067B1E0':['param_1 + 0x48','param_1 + 0x488','param_1 + 0x490','param_1 + 0x480'],
    '004F7170':['param_1 + 0x48','param_1 + 0x470'],
    '008631F0':['+ 400))(param_1,0x7ffffffd)'],
    '00860C90':['param_1 + 0x2f) = 0xffffffff'],
    '0088D5A0':['0x130,5,10','param_1 + 0x448'],
    '0092CB10':['param_1 + 0x10','param_1 + 8'],
    '0092CBA0':['param_1 + 0x10c','param_1 + 0xf4','param_1[0x1c]','param_1[0x1d]','lVar1 + 0x30','lVar1 + 0x34'],
}.items():
    suffix='_asm.txt' if a=='00862E40' else '_c.txt'
    s=(cache/(a+suffix)).read_text(encoding='utf-8')
    for f in fragments:check(f in s,f'caller/ABI evidence {a}: {f}')
classes=[json.loads(l) for l in (P/'research/ui_atlas/classes_all.jsonl').read_text(encoding='utf-8').splitlines()]
for c in classes:
    if 'CCmnDlgMsg' in c['name'] and ('YesNo' in c['name'] or 'Input' in c['name']):
        for v in c['vtables']:check(not any(v['rva']<=s<v['rva']+len(v['methods'])*8 for s in slots),'choice/input table remains native: '+c['name'])
for rel in ['installer/InstallerGui.cs','distribution/FeatureSelect.ps1','FeatureSelect.ps1','기능설정.ini']:
    raw=(P/rel).read_bytes();encoding='utf-16' if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig'
    check('advisor_briefing' in raw.decode(encoding),'single candidate switch '+rel)
code=(P/'src/native_advisor_briefing.inl').read_text(encoding='utf-8')
overlay=(P/'src/native_advisor_overlay.inl').read_text(encoding='utf-8')
check('native_ui::officer_notice' not in overlay and 'fn<' not in overlay,'Present/WndProc never calls the game')
check('ExecuteCommandList(commands,TRUE)' in overlay,'runtime restores the complete immediate pipeline state')
check('if(!ok || batch.empty())' in code,'unrecognized income format retains native report')
print(json.dumps(dict(status='advisor_briefing_native_verified',checks=checks,slots=len(slots),live_game=False)))
