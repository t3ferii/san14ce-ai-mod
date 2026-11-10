import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""CE KO offline evidence. No process attachment, game launch or live writes."""
from pathlib import Path
import hashlib,json,re,struct,os,subprocess
P=Path(__file__).resolve().parents[1]
b=(P/'build/ui_native_analysis/editor83_b25953386/game.exe.unpacked.exe').read_bytes()
capture=(P/'build/ui_native_analysis/fixture/1a4a447d_text.bin').read_bytes()
checks=0
def check(ok,why):
    global checks
    checks+=1
    assert ok,why
check(hashlib.sha256(b).hexdigest()=='1a4a447d14e68c40ad2a5107c29c7341c10eee8fc297a7b35032900c341dbcaa','decoded image identity')
check(hashlib.sha256((P.parent/'SAN14CE_KO.exe').read_bytes()).hexdigest() in ('87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a','782cf229a56cdde2b95399e0e7536740b3f5e96bea702679365e4dad061abdce'),'original Steam image identity') # Steam EXE: 87ed4e46 = before the 2026-11-05 update, 782cf229 = buildid 25953386 (build table, nothing weakened)
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
# 사용자 2026-10-28 설계 전환 3단계: no entry bypass when result application
# is written by the scene or read back from it. Caller-local application is allowed
# by Claude's corrected instruction (2026-10-28); retain the original byte evidence.
scene_decision_bytes=(
    (0x6C6B10,0x92,0x78D0D5A298A64342),
    (0x6C6DD0,0x92,0x82A091CE73565A9F),
    (0x689B00,0x1A3,0x7A06BC7D56BBA418),
    (0x6A66F0,0x80,0x769A1AF026389299),
    (0x1DBD80,0xCDA,0x83E17EA357BAFC55),
    (0x1DCA70,0x891,0x55F7240275B4F147),
    (0x1DD4B0,0x1503,0x96EEC0F39110992D),
    (0x26EEB0,0x2B0E,0x979A41140358A35E),
    (0x41F8C0,0xCB,0xB70122A792C1219E),
    (0x4195D0,0x169,0x161F7A80F36B5E64),
    (0x41CB30,0x109,0xE465205E84D2C593),
    (0x423D00,0x5D,0x5E8B1F5ECC158262),
    (0x424A30,0xC1,0x76B448DB4A610380),
    (0x424B00,0x23,0x573BC5D4DA0EFB08),
    (0x424CA0,0x1AB,0x6B2E58304C5487),
    (0x273100,0x9B3,0x2D730E4587730006),
    (0x724E40,0x498,0x2174EB3F7BA26F35),
    (0x75CA50,0x6BC,0x12EBCFA67044AD88),
    (0x71FFB0,0x10F,0xF9E03AB046FF6964),
    (0x6EE3B0,0x264,0x223BDB6412080245),
    (0x6EE150,0x25D,0xF4636CD17E8B4894),
    (0x33D1C0,0x68,0xA2331A4958A2604B),
    (0x848340,0xC,0x517FD736F7F12ADC),
)
for a,n,h in scene_decision_bytes:
    check(fnv(read(a,n))==h,f'stage3 scene/result decision bytes {a:X}')
    check(read(a,n)==capture[a-0x1000:a-0x1000+n],f'independent stage3 decision capture {a:X}')
for site,target in ((0x1DC4D0,0x6C6B10),(0x1DC808,0x273100),
                    (0x1DCD60,0x6C6B10),(0x1DD08C,0x273100),
                    (0x1DD845,0x6C6DD0),(0x1DDA68,0x273100),
                    (0x1DDE15,0x424C50),(0x424C8E,0x41F8C0),
                    (0x1DE969,0x2BB7C0),(0x1DE97A,0x16A4E0),(0x1DE982,0x2260A0)):
    call=read(site,5)
    check(call[0]==0xE8 and site+5+struct.unpack_from('<i',call,1)[0]==target,f'stage3 scene and subsequent result call {site:X}->{target:X}')
for site,target in ((0x1DDD4E,0x33D1C0),(0x1DDD79,0x33D6F0),(0x1DDE08,0x424E50),(0x1DE0E8,0x325F80),(0x1DE15F,0x6C6DD0)):
    call=read(site,5)
    check(call[0]==0xE8 and site+5+struct.unpack_from('<i',call,1)[0]==target,f'discovery/scenery result applies before presentation {site:X}->{target:X}')
profile=(P/'src/advisor_briefing_profile.h').read_text(encoding='utf-8')
# 사용자 2026-10-28 설계 전환 4단계: exact tail wrappers, no relocated rel32.
for a,n,h in ((0x844F70,0xD5,0x1DB1F690873FB1AE),(0x837710,0x90,0x750C3604570D5AB6),
              (0x865BD0,0x7F,0x9489A7D252761B52),(0x864F40,0x32,0x5F3337F11CA79BA6)):
    check(fnv(read(a,n))==h,f'stage4 complete picture/wait decision bytes {a:X}')
    check(read(a,n)==capture[a-0x1000:a-0x1000+n],f'independent stage4 decision capture {a:X}')
for a,raw,jump,target in (
    (0x4541E0,'488b89b80100004533c0e9810d3f00',10,0x844F70),
    (0x4421F0,'488b89b8010000e914553f00',7,0x837710),
):
    expected=bytes.fromhex(raw)
    check(read(a,len(expected))==expected,f'complete picture wrapper bytes {a:X}')
    check(read(a,len(expected))==capture[a-0x1000:a-0x1000+len(expected)],f'independent picture wrapper capture {a:X}')
    check(a+jump+5+struct.unpack_from('<i',expected,jump+1)[0]==target,f'byte-decoded picture tail target {a:X}->{target:X}')
for site,caller in ((0x29A854,0x299D20),(0x2A57A0,0x2A5140),(0x2A5C34,0x2A5140),
                    (0x2C46FD,0x2C3BD0),(0x2D4195,0x2D40C0),(0x2D7BD2,0x2D6460),
                    (0x2DC963,0x2DC840),(0x2E24E1,0x2E23F0),(0x2EB225,0x2EB1F0),
                    (0x379DAC,0x379D20),(0x816F84,0x816AB0),(0x8170EE,0x816AB0),
                    (0x81756D,0x817200),(0x817DD7,0x817730),(0x817FB4,0x817730),
                    (0x81822F,0x8180C0),(0x8184C7,0x8180C0),(0x818D06,0x818600),(0x8193A0,0x819010)):
    call=read(site,5)
    check(call[0]==0xE8 and site+5+struct.unpack_from('<i',call,1)[0]==0x306A30,f'research-only wait caller {caller:X} at {site:X}')
    check(call==capture[site-0x1000:site-0x1000+5],f'independent wait caller bytes {site:X}')
for a,n,h in re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+),0x([0-9A-F]+)ull\}',profile):
    a,n,h=int(a,16),int(n,16),int(h,16)
    check(fnv(read(a,n))==h,f'current function bytes {a:X}')
    check(read(a,n)==capture[a-0x1000:a-0x1000+n],f'independent retained capture {a:X}')
slots=[]
for a,t in re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+)\}',profile.split('static const Slot slots[]={')[1]):
    a,t=int(a,16),int(t,16);slots.append(a)
    check(struct.unpack('<Q',read(a,8))[0]==0x140000000+t,f'exact vtable slot {a:X}')
check(len(slots)==20,'twenty exact information/lifecycle slots including person/messenger notices')
check(read(0x844970,5)==bytes.fromhex('e89b1b0000'),'shared narrative call preserves the original four-argument 844030 ABI')
check(struct.unpack('<Q',read(0x1471508+0x190,8))[0]==0x140863780,'person notification uses its own original acknowledgement')
check(read(0x863783,7)==bytes.fromhex('488b8990010000'),'person notification animation child is +190, not the ordinary dialog +188')
check(read(0x8637A2,1)==b'\xe9' and 0x8637A7+struct.unpack('<i',read(0x8637A3,4))[0]==0x858B30,'person acknowledgement starts its original native closing lifecycle')
for site in (0x7362B1,0x7363C7,0x7364C9):
    call=read(site,5)
    check(call[0]==0xE8 and site+5+struct.unpack_from('<i',call,1)[0]==0x844860,f'exact recruitment narrative call {site:X}')
    check(call==capture[site-0x1000:site-0x1000+5],f'independent recruitment call bytes {site:X}')
check(struct.unpack('<Q',read(0x1444080+0x28,8))[0]==0x1407360C0,'employment result class and caller identity')
check(read(0x9332B0,5)==bytes.fromhex('48895c2408'),'text relay displaces exactly one five-byte MOV; no RSP or RIP change')
check(read(0x843B60,5)==bytes.fromhex('48895c2408'),'speech relay preserves exactly one five-byte MOV with original five-argument ABI')
check(struct.unpack('<Q',read(0x1471320,8))[0]==0x1408637B0,'diplomacy message uses the same native acknowledgement and close lifecycle')
check(read(0x8637CC,6)==bytes.fromhex('899078010000'),'native +190 stores EDX in dialog+178')
check(read(0x8637D2,1)==b'\xe9' and 0x8637D7+struct.unpack('<i',read(0x8637D3,4))[0]==0x858B30,'native acknowledgement starts the original closing animation, beyond storing the result')
# Leave 4F7170 cleans an opened dialog before delivering the parent callback.
check(read(0x4F99F0,1)==b'\xe8' and 0x4F99F5+struct.unpack('<i',read(0x4F99F1,4))[0]==0x865030,'Leave queries dialog state even while opened')
check(read(0x4F9A03,6)==bytes.fromhex('ff9098010000') and read(0x4F9A16,3)==bytes.fromhex('ff5028') and read(0x4F9A2D,2)==bytes.fromhex('ff10'),'Leave closes layout and invokes deleting destructor before parent callback')
check(struct.unpack('<8I',read(0x1434230,32))==(909,909,910,911,912,913,914,915),'income district headings match native label table')
G=P/'research/ghidra_full/1a4a447d14e68c40'
def ghidra_c(rva):   # decompiled body of FUN_140<rva> from the current build's ghidra export (2026-11-07: replaces the old decomp_cache texts)
    pat=re.compile(r'\n[A-Za-z_][^\n(]*FUN_140%06x\('%rva)
    for f in sorted((G/'c').glob('*.c')):
        t=f.read_text(encoding='utf-8',errors='replace');m=pat.search(t)
        if m:
            e=t.find('\n}\n',m.start());return t[m.start():e]
    raise KeyError(hex(rva))
code=(P/'src/native_advisor_briefing.inl').read_text(encoding='utf-8')
# User 2026-11-02: skip final report UI, retain result writers and parent Leave.
check(read(0x6793CD,5)==bytes.fromhex('e86e410000'),'headless replay owns exactly the native result-generation caller')
check(read(0x6793CD,5)==capture[0x6793CD-0x1000:0x6793CD-0x1000+5],'independent result caller capture')
ctor=read(0x674020,0xB5)
check(bytes.fromhex('48c7837004000004000000') in ctor and bytes.fromhex('4889b380040000') in ctor,'fresh report constructor initializes phase four and null background')
check(bytes.fromhex('88040000') not in ctor and bytes.fromhex('90040000') not in ctor,'skipped UI phase pointers are not initialized by the native constructor')
formatter=ghidra_c(0x67F910)
empty_string=struct.unpack('<Q',read(0x1A4AB88,8))[0]-0x140000000
check(read(empty_string,2)==b'\0\0','native PTR_DAT_141a47b98 really points to an empty UTF-16 string')
check('uVar7 = 0x26;' in formatter and 'puVar10 = PTR_DAT_141a4ab88;' in formatter,'native non-applicable military branch explicitly clears delta 38')
check(set(re.findall(r'param_1 \+ (0x[0-9a-f]+)',formatter))=={'0x378'},'income formatter accesses only the supplied text layout pointer')
lookup=ghidra_c(0x92F390)
check('*(int *)(param_1 + 0x10) <= param_2' in lookup and 'return 0;' in lookup,'zero-node layout rejects lookup before accessing nodes')
visibility=ghidra_c(0x9314A0)
check('param_2 < (int)param_1[2]' in visibility and '(lVar1 != 0)' in visibility,'optional visibility update handles an empty layout without dereferencing nodes')
check('call_restore(report_results_call)' in code and 'scene_call_publish(report_results_call)' in code,'result replay caller participates in guarded publication and rollback')
headless=code.split('static bool try_headless_report(',1)[1].split('static unsigned long long __fastcall report_enter(',1)[0]
check(headless.index('consumed.push_back')<headless.index('0x67D540')<headless.index('headless_income(batch,districts)'), 'native consumption, result writers and income calculation retain original order')
check('replay_results=action' in headless and 'r.second&6' in headless,'fallback restores pending bits and reuses applied results')
for token in ('ReportTiming','advisor_report_cost','advisor_report_cpu_valid'):
    # Added below to the existing release preprocessing/binary checks as well.
    check(token in code,'headless diagnostic wall/CPU timing exists '+token)
# 2026-11-07: caller/ABI evidence re-derived from the current build's ghidra export (each function located by its decompiled-text equivalence to the former build's;
# FUN_ names inside the fragments mapped to the current build's functions).
for a,fragments in {
    '0067DB40':['0x480', '0x470', 'FUN_14067f0b0(param_1,0)'],
    '0067F0B0':['param_1 + 0x488', 'param_1 + 0x490'],
    '00682D70':['FUN_14067f910(param_1,lVar3)'],
    '0067F910':['FUN_1409332b0', 'param_1 + 0x378'],
    '0067A430':['local_90 = 0x394'],
    '005726B0':['param_1 + 0x10', 'param_1 + 0x20', '0x70', 'bool FUN_1405726b0'],
    '00812FF0':['param_1 + 0x478', '+ 0x1b0', '100,&local_50'],
    '00843B60':['param_1 + 0x478', 'param_2 * 8', 'FUN_140844110'],
    '0044ED30':['void FUN_14044ed30(longlong param_1)', 'FUN_140572670()'],
    '0022E450':['param_1 + 0xe0', '+ 0xdf30'],
    '00230750':['FUN_140230790(param_1,uVar1)'],
    '00818600':['*(undefined4 *)(param_1 + 0x4f8) = 0', '*(undefined4 *)(param_1 + 0x4f8) = 1'],
    '007360C0':['FUN_140844860(param_1,param_1[0x96],local_818,1)', 'FUN_140844860(param_1,param_1[0x97],local_818,1)', '3 - (uint)((int)param_1[0x98] != 0)'],
    '00844860':['longlong param_5,undefined4 param_6', 'EventView::CInputWaitState', 'FUN_140574750(param_1)'],
    '001F2A60':['FUN_140863080', 'param_1 + 0x478', 'lVar1 + 0x188'],
    '00846510':['param_1 + 0x1f8', 'param_1 + 0x178', 'FUN_140844110'],
    '0083E300':['FUN_1408636b0(param_1 + 0x470)', 'FUN_1400124a0'],
    '008636B0':['param_1 + 8', 'param_1 + 0xc', '0x7fffffff'],
    '008655A0':['param_1[0x35] + 0x22c', 'FUN_140865fc0', '+ 400'],
    '00678F30':['0x82', '0x16280', '0x1b00', '0x140'],
    '00243140':['param_1 + 0x8a', '0x55'],
    '0067D8A0':['param_1 + 0x48', 'param_1 + 0x488', 'param_1 + 0x490', 'param_1 + 0x480'],
    '004F99D0':['param_1 + 0x48', 'param_1 + 0x470', '0x198'],
    '00865520':['+ 400))(param_1,0x7ffffffd)'],
    '00862FC0':['param_1 + 0x2f) = 0xffffffff'],
    '0088F890':['0x130,5,10', 'param_1 + 0x448'],
    '0092EFC0':['param_1 + 0x10', 'param_1 + 8'],
    '0092F050':['param_1 + 0x10c', 'param_1 + 0xf4', 'param_1[0x1c]', 'param_1[0x1d]', 'lVar1 + 0x30', 'lVar1 + 0x34'],
}.items():
    s=ghidra_c(int(a,16))
    for f in fragments:check(f in s,f'caller/ABI evidence {a}: {f}')
gcalls=(G/'calls.tsv').read_text(encoding='utf-8')   # former 00862E40 asm fragments: same instructions at the same offsets of the current function 865170
check(read(0x8651A4,7)==bytes.fromhex('4c8b81b0040000') and '00865170\t008651C2\t00844860\tdirect' in gcalls and '00865170\t008651D3\t000124A0\tdirect' in gcalls,'caller/ABI evidence 00865170: MOV R8,[RCX+0x4b0]; CALL 844860; JMP 124A0')
classes=[json.loads(l) for l in (P/'research/ui_atlas/classes_all.jsonl').read_text(encoding='utf-8').splitlines()]
for c in classes:
    if 'CCmnDlgMsg' in c['name'] and ('YesNo' in c['name'] or 'Input' in c['name']):
        for v in c['vtables']:check(not any(v['rva']<=s<v['rva']+len(v['methods'])*8 for s in slots),'choice/input table remains native: '+c['name'])
for rel in ['installer/InstallerGui.cs','distribution/FeatureSelect.ps1','FeatureSelect.ps1','기능설정.ini']:
    if rel=='기능설정.ini' and not (P/rel).exists():continue   # install-generated local file, untracked: absent in a clean checkout/worktree
    raw=(P/rel).read_bytes();encoding='utf-16' if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig'
    check('advisor_briefing' in raw.decode(encoding),'single candidate switch '+rel)
picture_prepare=code.split('static bool prepare_picture_link(',1)[1].split('static void pop()',1)[0]
check('memcpy' not in picture_prepare and 'absolute_jump(at,hook)' in picture_prepare,'picture replacement never relocates native relative tail jumps')
check('fn<void(__fastcall *)(Obj,int,int)>(0x844F70)(pointer(object,0x1B8),id,0)' in code and
      'fn<void(__fastcall *)(Obj)>(0x837710)(pointer(object,0x1B8))' in code,'original picture route reconstructs both tail-wrapper ABIs')
split=(P/'src/native_split_install.inl').read_text(encoding='utf-8')
check('if(f.advisor_briefing && !advisor_briefing_guards_match())split_disable(f.advisor_briefing,26);' in split,'existing split compatibility gate disables all stage4 hooks on fingerprint mismatch')
# 사용자 2026-10-28 설계 정정 replaces the earlier overly strict no-hook assertion.
for scene in ('0x6C6B10','0x6C6DD0','0x41F8C0'):
    check('prepare_message_link(scene_links[' in code and scene in code,'corrected stage3 decision installs guarded scene entry '+scene)
overlay=(P/'src/native_advisor_overlay.inl').read_text(encoding='utf-8')
check('native_ui::officer_notice' not in overlay and 'fn<' not in overlay,'Present/WndProc never calls the game')
# 사용자 결정 2026-10-28: immediate render replaces full command-list save/restore.
check('ExecuteCommandList' not in overlay and 'CreateDeferredContext' not in overlay and 'PipelineState saved_pipeline(c)' in overlay,'immediate drawing saves/restores only touched bindings; WARP verifies each binding')
check('if(!ok || batch.empty())' in code,'unrecognized income format retains native report')
core=(P/'src/advisor_briefing.h').read_text(encoding='utf-8')
notice=core.split('inline bool passive_notice(',1)[1].split('inline bool interactive_state(',1)[0]
check('!has(text,' not in notice and 'return !text.empty();' in notice,'사용자 결정 2026-10-28: 클릭형은 모두 수집; no punctuation-based question filter')
# 사용자 2026-10-28 설계 전환 2단계: removed shared display/timer collectors.
check('passive_speech(' not in code and 'recruitment_speech(' not in code and 'recruitment_links' not in code and 'passive_link' not in code,'사용자 2026-10-28 설계 전환 2단계: obsolete call-site collectors are removed')
release_code=re.sub(r'#ifndef NO_RUNTIME_LOG\n.*?#endif', '',code,flags=re.S)
for token in ('diagnose_uncollected_top','diagnostic_cycle','collected_state','collected_top_rvas','last_top_rva','state_rva','advisor_briefing_uncollected_top'):
    check(token not in release_code,'NO_RUNTIME_LOG excludes diagnostic-only code: '+token)
check('static CodeLink message_links[3];' in code and 'static CodeLink event_link;' in code and 'static CodeLink scene_links[3];' in code,'seven notification/EventView/scene entries have dedicated transactional CodeLinks')
check('message==WM_CAPTURECHANGED && reinterpret_cast<HWND>(lparam)==hwnd' in overlay,'same-window recapture is ignored before cancelling drag state')
check('if(closing_now && !head.pressed && !scrolling.pressed)' in overlay and 'return hit!=0;' in overlay,'closing animation consumes only bubble/head hits or active presses')
simple=code.split('static void __fastcall simple_frame(',1)[1].split('static bool record(',1)[0]
# 사용자 2026-10-28 설계 전환: 직접 pop으로 CProgressState 소실, 원작 유지.
# Replace the old immediate-close assertions; only report_wait retains a direct pop.
plain=re.sub(r'//[^\n]*|/\*.*?\*/','',code,flags=re.S)
narration=code.split('static void __fastcall narration(',1)[1].split('static unsigned long long message_enter(',1)[0]
check('pop();' not in simple and 'dialog+0x178' not in simple and 'pop();' not in narration and 'store(' not in narration,'general message/narration hooks never collect, write close result or directly pop')
check(len(re.findall(r'\bpop\(\);',plain))==1 and 'pop();' in plain.split('static void __fastcall report_wait(',1)[1].split('#ifndef',1)[0],'only the explicitly preserved report_wait path can directly pop a state')
check('const bool skip=diplomatic && dialogue_ready(false),watch=diplomatic && !skip && dialogue_observed(false);' in code and 'if(!diplomatic)return;' in simple,'entry-bypassing ordinary and person message states preserve native handling')
entry=code.split('static bool entry_ready(',1)[1].split('static bool prepare_message_link(',1)[0]+code.split('static bool capture_event(',1)[1].split('static void __fastcall set_text(',1)[0]
entry=re.sub(r'//[^\n]*', '',entry)
for address in ('0x83C920','0x1D4AE0','0x1D4DC0','0x1D4C50','0x846510','0x574750','0x124A0'):
    check(address not in entry,'entry capture never calls push/suspend/yield/pop '+address)
for token in ('message_entry_counts','message_pass_reasons'):
    check(token not in release_code,'NO_RUNTIME_LOG excludes entry counters '+token)
check('restore_link(scene_links[--i]);restore_link(event_link);for(unsigned i=3;i;)restore_link(message_links[--i]);' in code and 'published=publish_link(message_links[i])' in code and 'published && publish_link(event_link)' in code and 'published=publish_link(scene_links[i])' in code,'all seven entries participate in transactional install and rollback')
check(code.index('if(!guards_match())return false;',code.index('static bool install()'))<code.index('publish_link(message_links[i])'),'fingerprint/prologue mismatch publishes neither entry hook')
for address,prologue in ((0x1F1E90,'488bc455488d68a1'),(0x1F1DA0,'488bc4574881ec80000000'),(0x1F2190,'488bc455488d68a1'),(0x844860,'405556574154'),(0x6C6B10,'40574883ec70'),(0x6C6DD0,'40574883ec70'),(0x41F8C0,'488bc44156')):
    check(read(address,len(bytes.fromhex(prologue)))==bytes.fromhex(prologue),'whole non-RIP-relative entry instructions '+hex(address))
# PE exception-table bounds independently establish whole function fingerprint sizes.
ex_rva,ex_size=struct.unpack_from('<II',b,pe+24+112+3*8)
bounds={a:e-a for a,e,_ in struct.iter_unpack('<III',read(ex_rva,ex_size))}
check(bounds[0x1F1E90]==0x2FA and bounds[0x1F1DA0]==0xE9 and bounds[0x1F2190]==0x1D4 and bounds[0x844860]==0x1E6,'entry FNV guards cover full functions through RET, using PE runtime function bounds')
check(bounds[0x6C6B10]==0x92 and bounds[0x6C6DD0]==0x92 and bounds[0x41F8C0]==0xCB,'scene entry guards cover all native wrapper instructions through RET')
check(read(0x830EC0,12)==bytes.fromhex('488b12488b4908e92438d4ff') and 0x830ECC+struct.unpack('<i',read(0x830EC8,4))[0]==0x5746F0,'both scene functors only load caller+8 and tail-call 5706C0 resume; no params writeback')
for table in (0x1450D68,0x14528C0):
    check(struct.unpack('<Q',read(table+0x10,8))[0]==0x140830EC0,'scene completion callback is resume-only '+hex(table))
check(read(0x689B8E,0x21)==bytes.fromhex('498b06488983b0040000498b4608488983b8040000418b46108983c0040000eb03'),'recruitment constructor reads params into independent state fields only')
for vt,name in ((0x144A0B8,'CDiscoveryItemDialog'),(0x1449B48,'CInfoSummaryGroundAdvantageDiscovery'),(0x1449CE8,'CInfoSummarySceneryDiscovery')):
    col=struct.unpack('<Q',read(vt-8,8))[0]-0x140000000;td=struct.unpack('<I',read(col+12,4))[0]
    check(name.encode() in read(td+16,len(name)+10),'independent discovery presentation class '+name)
check(struct.unpack('<Q',read(0x144A0B8+0x1A8,8))[0]==0x1406EE3B0 and struct.unpack('<Q',read(0x144A0B8+0x1A0,8))[0]==0x1406EE150,'discovery type0 item and type2 secondary item use read-only presentation methods')
scene_capture=code.split('static bool scene_ready(',1)[1].split('static bool prepare_message_link(',1)[0]
for formatter in ('0x30C380','0x30FC40','0x75CA50','0x7360C0','0x843B60','0x689B00','0x688B10','0x4195D0','0x574750'):
    check(formatter not in scene_capture,'summary capture invokes neither original message formatting nor scene construction '+formatter)
check('e.text=e.report=summary' in scene_capture and 'scene_ready(4)' in scene_capture and 'scene_ready(5)' in scene_capture and 'scene_ready(6)' in scene_capture,'one owned summary uses the same native gates on each scene entry')
for site,target in ((0x1F1EB8,0x335110),(0x1F1FD3,0x1D4AE0),(0x1F20EE,0x1D4AE0),(0x1F2167,0x574750),(0x1F1DC0,0x335110),(0x1F1E62,0x1D4DC0),(0x1F1E6A,0x574750)):
    call=read(site,5)
    check(call[0]==0xE8 and site+5+struct.unpack_from('<i',call,1)[0]==target,'independent native entry call '+hex(site))
# Exact whitelist provenance: independently decode RTTI COL/type descriptor and frame slots.
for vt,name,frame in [(0x144A270,'CSearchDiscoveryState',0x734230),(0x1444080,'CStrategyEmployTalkState',0x7360C0),(0x143EFD8,'CStrategyAdviceState',0x734300),(0x1468478,'CEventModalState',0x83CD50),(0x144ACF8,'CStrategyCreditRankupState',0x735830)]:
    col=struct.unpack('<Q',read(vt-8,8))[0]-0x140000000
    td=struct.unpack('<I',read(col+12,4))[0]
    check(name.encode() in read(td+16,len(name)+10),'independent PE RTTI owner '+name)
    check(struct.unpack('<Q',read(vt+0x28,8))[0]==0x140000000+frame,'independent PE frame '+name)
    check(hex(vt)[2:].upper() in profile,'profile records exact owner '+name)
# 사용자 2026-10-28 입력·대기창 보정: result-only wait bypass with timeout=0.
check(read(0x864F54,7)==bytes.fromhex('488b4208448900'),'CCmnWaitTimeDialog init loads b and writes a DWORD result through it')
check(bounds[0x306A30]==0xB9 and read(0x306A30,11)==bytes.fromhex('488bc4574881ec80000000'),'time wait complete PE function bound and eleven non-relative entry bytes')
check(read(0x306A30,0xB9)==capture[0x306A30-0x1000:0x306A30-0x1000+0xB9],'independent current wait entry capture')
check('prepare_message_link(time_wait_link,0x306A30,narration_prologue' in code and 'publish_link(time_wait_link)' in code and 'restore_link(time_wait_link)' in code,'guarded wait entry participates in transactional prepare/publication/rollback')
time_body=code.split('static void __fastcall time_wait(',1)[1].split('static void name_speaker(',1)[0]
check('result && !duel_running() && defer()' in time_body and 'Obj m=pointer(image,0x2786780);return m && dword(m,0x10)!=0;' in code and 'return !duel_running() && !capture_scene;   // stored; a duel or capture line is also shown natively' in code and '0x335110' in time_body and '0x572480' in time_body and '*result=0;return;' in time_body and 'original_time_wait(milliseconds,result,allow_skip)' in time_body,'wait uses defer/interaction, native mode/caller, zero timeout, original fallback arguments')
for address in ('0x245630','0x574750','0x124A0'):
    check(address not in time_body,'time entry never pushes/yields/pops '+address)
check('Obj top=top_state();bool player=top && pointer(top,0)==image+0x13D9430 && !interaction();' in code and 'if(!player){open=false;rendered_open=false;display_cache.open=false;}' in code,'native log frame closes all cached open flags on covered player state')
col=struct.unpack('<Q',read(0x13D9430-8,8))[0]-0x140000000;td=struct.unpack('<I',read(col+12,4))[0]
check(b'CUserStrategyState' in read(td+16,100),'independent RTTI identifies exact player top vtable')
for body in (code.split('static void __fastcall log_frame(',1)[1].split('static void __fastcall log_leave(',1)[0],code.split('static void __fastcall advice_frame(',1)[1].split('static void __fastcall advice_leave(',1)[0]):
    check('move(old_log,false)' not in body and 'move(old_advice,false);fn<' not in body,'사용자 2026-10-28 설계 전환 2단계: no per-frame restore/native/hide round trip')
check('l.moved==hide' in code and 'needs_portrait' in code and '!briefing_overlay::captured(portrait_serial)' in code,'position changes are transition guarded; only missing/new portrait requests expose advice')
wait_body=code.split('static void __fastcall diplomatic_wait(',1)[1].split('static unsigned long long __fastcall report_enter(',1)[0]
check('advance_recruit' not in wait_body and 'advance_passive' not in wait_body and 'speech_waiting' in wait_body,'only original diplomatic confirmation advances native wait')
check('install_mouse_table' in overlay and 'GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT' in overlay and 'GetModuleFileNameW(owner,path,MAX_PATH)' in overlay,'DirectInput resolves each real function owner, preserving module lifetime reference count')
check('module_contains' not in overlay and 'allowed_mouse_path(path,system,game,have_steam?steam:nullptr)' in overlay and 'GetModuleHandleW(L"dinput8.dll")' in overlay,'사용자 2026-10-28 local22: proxy export unchanged; exact system/PreviousInput/installed Steam paths only')
check('RegGetValueW(HKEY_CURRENT_USER' in overlay and 'RRF_SUBKEY_WOW6432KEY' in overlay and 'GetFullPathNameW(candidate.c_str()' in overlay and 'GetFileAttributesW(path)' in overlay and 'gameoverlayrenderer64.dll' in overlay,'Steam overlay trust comes from registered install directory and actual file, not module basename')
check('reinterpret_cast<DeviceState>(table[9]),reinterpret_cast<DeviceData>(table[10])' in overlay and 'HRESULT result=original(device,bytes,data)' in overlay and 'HRESULT result=original(device,bytes,data,count,flags)' in overlay,'saved current provider pointers are called with original state/data arguments before filtering')
check('InterlockedCompareExchangePointer(t.table+9' in overlay and 'InterlockedCompareExchangePointer(t.table+10' in overlay and 'state_before==' in overlay and 'data_before==' in overlay,'removal compares each slot with our wrapper and diagnoses intervening ownership changes')
check('outside_click=true;post_input(2,model.generation);InterlockedExchange(&block_buttons,1)' in overlay and 'if(outside_click)' in overlay and '!scrolling.pressed && !scrolling.active()' in overlay,'outside click pair closes, blocks DI and respects scrolling exceptions')
check('try_mouse_interface<IDirectInput8A,IDirectInputDevice8A>' in overlay and 'try_mouse_interface<IDirectInput8W,IDirectInputDevice8W>' in overlay,'사용자 2026-10-28: both A/W interface/device ABIs are attempted independently')
check('GetCapabilities(&info)' in overlay and 'GetDeviceInfo(&info)' not in overlay,'device identification uses the shared A/W capabilities ABI')
check('mouse_interface_success==3 || mouse_attempts>=3' in overlay and 'now+1000' in overlay,'initial provider failure has bounded delayed retries, with no per-frame COM loop after success')
# 사용자 지시 2026-10-28: prove diagnostic exclusion with the real preprocessor,
# and run the same behavior fixture in a NO_RUNTIME_LOG build as well.
vc=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231')
sdk=Path(r'C:\Program Files (x86)\Windows Kits\10');version='10.0.26100.0'
env=os.environ.copy();env['PATH']=str(vc/'bin/Hostx64/x64')+os.pathsep+env['PATH']
env['INCLUDE']=';'.join(map(str,[vc/'include',sdk/'Include'/version/'ucrt',sdk/'Include'/version/'shared',sdk/'Include'/version/'um']))
env['LIB']=';'.join(map(str,[vc/'lib/x64',sdk/'Lib'/version/'ucrt/x64',sdk/'Lib'/version/'um/x64']))
out=P/'build/development';out.mkdir(exist_ok=True)
diagnostic_tokens=('frame_metrics','layout_move_calls','FrameTiming','measured_frames','message_entry_counts','message_pass_reasons','message_entry_paths','message_reason_columns','hook_activity','hook_called','wait_failures','wait_false','wait_last_state','wait_last_rva','wait_last_top_equal',
                   'picture_count_slots','picture_last_id','picture_counts','picture_id_counts','picture_close_counts',
                   'wait_zero_writes','wait_native_returns','wait_top_changed','wait_ready_diagnostic','wait_owner_matches','wait_condition',
                   'present_thread_id','present_tick_ms','wndproc_thread_id','wndproc_tick_ms','lock_thread_id','lock_tick_ms',
                   'di_install_attempted','di_install_success','di_patched_table','di_interface_kind','di_install_details','di_provider_stage','di_interface_mask',
                   'di_owner_paths','di_owner_checks','di_restore_counts','outside_click_counts','context_resets','time_wait_counts','player_context_counts',
                   'advisor_portrait_sample','portrait_sample_logs','portrait_accept_logs',
                   'ReportTiming','advisor_report_cost','advisor_report_cpu_valid','advisor_report_headless',
                   'advisor_report_income_missing','advisor_report_collect_fail')
for fixture in ('advisor_briefing_fixture','advisor_overlay_fixture'):
    pp=subprocess.run([str(vc/'bin/Hostx64/x64/cl.exe'),'/nologo','/utf-8','/std:c++17','/EP','/DNO_RUNTIME_LOG',str(P/'tests'/f'{fixture}.cpp')],cwd=out,env=env,capture_output=True)
    check(pp.returncode==0,'NO_RUNTIME_LOG preprocess '+fixture+': '+pp.stderr.decode('utf-8','replace')[-2000:])
    for token in diagnostic_tokens:
        check(token.encode() not in pp.stdout,'NO_RUNTIME_LOG excludes diagnostic symbol/string '+fixture+': '+token)
    check(b'advisor_briefing_uncollected_top' not in pp.stdout and b'advisor_briefing_interactive_after_passive' not in pp.stdout,'NO_RUNTIME_LOG excludes runtime diagnostic log strings '+fixture)
release=out/'advisor_briefing_nolog_fixture.exe'
compiled=subprocess.run([str(vc/'bin/Hostx64/x64/cl.exe'),'/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc','/DNO_RUNTIME_LOG',
                         str(P/'tests/advisor_briefing_fixture.cpp'),'/Fe'+str(release),'/link','kernel32.lib','user32.lib'],cwd=out,env=env,capture_output=True)
check(compiled.returncode==0,'NO_RUNTIME_LOG fixture build: '+(compiled.stdout+compiled.stderr).decode('utf-8','replace')[-3000:])
run=subprocess.run([str(release),str(P/'build/ui_native_analysis/fixture/1a4a447d_text.bin')],cwd=out,capture_output=True)
check(run.returncode==0,'NO_RUNTIME_LOG behavior parity: '+(run.stdout+run.stderr).decode('utf-8','replace')[-3000:])
print(run.stdout.decode('utf-8','replace').strip())
for token in (*diagnostic_tokens,'advisor_briefing_uncollected_top','advisor_briefing_interactive_after_passive'):
    check(token.encode() not in release.read_bytes() and token.encode('utf-16-le') not in release.read_bytes(),'NO_RUNTIME_LOG binary excludes diagnostic strings: '+token)
print(json.dumps(dict(status='advisor_briefing_native_verified',checks=checks,slots=len(slots),live_game=False)))
