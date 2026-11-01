"""CE KO offline evidence. No process attachment, game launch or live writes."""
from pathlib import Path
import hashlib,json,re,struct,os,subprocess
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
# 사용자 2026-10-28 설계 전환 3단계: no entry bypass when result application
# is written by the scene or read back from it. Caller-local application is allowed
# by Claude's corrected instruction (2026-10-28); retain the original byte evidence.
scene_decision_bytes=(
    (0x6C43B0,0x92,0x98ADEEDDF572D31A),
    (0x6C4670,0x92,0x9DF77FEDBF7DE264),
    (0x6873C0,0x1A3,0x3A6D4ADFA6CCD506),
    (0x6A4170,0x80,0xF9ED2E9EF713E2EC),
    (0x1DABF0,0xCDA,0xADAC5F2B9B2D24F1),
    (0x1DB8E0,0x891,0xB962231DACF1C838),
    (0x1DC320,0x1503,0x373216AC06FBEB59),
    (0x26D590,0x2B0E,0xBD74052F2976573B),
    (0x41D550,0xCB,0xB22ECE0F18101E79),
    (0x417250,0x169,0x4F17AB987DAD800A),
    (0x41A7B0,0x109,0x2D9258D747430B0A),
    (0x421990,0x5D,0x2F9204B42302C117),
    (0x4226C0,0xC1,0x7B89F062929E28D1),
    (0x422790,0x23,0x573BC5D4DA0EFB08),
    (0x422930,0x1AB,0x8EC13CEE51C9054),
    (0x2717D0,0x9B3,0xF765655338ECAD99),
    (0x7229C0,0x498,0xDA9A7A57FEA38316),
    (0x75AAE0,0x6BC,0x9A54CAFCD527A722),
    (0x71DC00,0x10F,0xC558E55CE3D92497),
    (0x6EC0D0,0x264,0x1D0D6FE50EF0B75F),
    (0x6EBE70,0x25D,0x75FC1D25E9062A16),
    (0x33AF80,0x68,0xA2331A4958A2604B),
    (0x845E90,0xC,0xD146DC6775CEE2B2),
)
for a,n,h in scene_decision_bytes:
    check(fnv(read(a,n))==h,f'stage3 scene/result decision bytes {a:X}')
    check(read(a,n)==capture[a-0x1000:a-0x1000+n],f'independent stage3 decision capture {a:X}')
for site,target in ((0x1DB340,0x6C43B0),(0x1DB678,0x2717D0),
                    (0x1DBBD0,0x6C43B0),(0x1DBEFC,0x2717D0),
                    (0x1DC6B5,0x6C4670),(0x1DC8D8,0x2717D0),
                    (0x1DCC85,0x4228E0),(0x42291E,0x41D550),
                    (0x1DD7D9,0x2B9440),(0x1DD7EA,0x169530),(0x1DD7F2,0x224670)):
    call=read(site,5)
    check(call[0]==0xE8 and site+5+struct.unpack_from('<i',call,1)[0]==target,f'stage3 scene and subsequent result call {site:X}->{target:X}')
for site,target in ((0x1DCBBE,0x33AF80),(0x1DCBE9,0x33B4B0),(0x1DCC78,0x422AE0),(0x1DCF58,0x323ED0),(0x1DCFCF,0x6C4670)):
    call=read(site,5)
    check(call[0]==0xE8 and site+5+struct.unpack_from('<i',call,1)[0]==target,f'discovery/scenery result applies before presentation {site:X}->{target:X}')
profile=(P/'src/advisor_briefing_profile.h').read_text(encoding='utf-8')
# 사용자 2026-10-28 설계 전환 4단계: exact tail wrappers, no relocated rel32.
for a,n,h in ((0x842A90,0xD5,0xB90A38918F67341E),(0x835230,0x90,0xD63AB33A4A9A5B78),
              (0x863840,0x7F,0x43646F16255C2980),(0x862C10,0x32,0x5F3337F11CA79BA6)):
    check(fnv(read(a,n))==h,f'stage4 complete picture/wait decision bytes {a:X}')
    check(read(a,n)==capture[a-0x1000:a-0x1000+n],f'independent stage4 decision capture {a:X}')
for a,raw,jump,target in (
    (0x451FF0,'488b89b80100004533c0e9910a3f00',10,0x842A90),
    (0x43FF90,'488b89b8010000e994523f00',7,0x835230),
):
    expected=bytes.fromhex(raw)
    check(read(a,len(expected))==expected,f'complete picture wrapper bytes {a:X}')
    check(read(a,len(expected))==capture[a-0x1000:a-0x1000+len(expected)],f'independent picture wrapper capture {a:X}')
    check(a+jump+5+struct.unpack_from('<i',expected,jump+1)[0]==target,f'byte-decoded picture tail target {a:X}->{target:X}')
for site,caller in ((0x298944,0x297E10),(0x2A35B0,0x2A2F50),(0x2A3A44,0x2A2F50),
                    (0x2C237D,0x2C1850),(0x2D1DE5,0x2D1D10),(0x2D5822,0x2D40B0),
                    (0x2DA5B3,0x2DA490),(0x2E01B1,0x2E00C0),(0x2E8E65,0x2E8E30),
                    (0x377A4C,0x3779C0),(0x814854,0x814380),(0x8149BE,0x814380),
                    (0x814E34,0x814AD0),(0x815693,0x814FF0),(0x815864,0x814FF0),
                    (0x815ADD,0x815970),(0x815D71,0x815970),(0x81659A,0x815EA0),(0x816C2D,0x8168A0)):
    call=read(site,5)
    check(call[0]==0xE8 and site+5+struct.unpack_from('<i',call,1)[0]==0x304640,f'research-only wait caller {caller:X} at {site:X}')
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
check(read(0x842490,5)==bytes.fromhex('e89b1b0000'),'shared narrative call preserves the original four-argument 844030 ABI')
check(struct.unpack('<Q',read(0x146E478+0x190,8))[0]==0x140861450,'person notification uses its own original acknowledgement')
check(read(0x861453,7)==bytes.fromhex('488b8990010000'),'person notification animation child is +190, not the ordinary dialog +188')
check(read(0x861472,1)==b'\xe9' and 0x861477+struct.unpack('<i',read(0x861473,4))[0]==0x856840,'person acknowledgement starts its original native closing lifecycle')
for site in (0x733F21,0x734037,0x734139):
    call=read(site,5)
    check(call[0]==0xE8 and site+5+struct.unpack_from('<i',call,1)[0]==0x842380,f'exact recruitment narrative call {site:X}')
    check(call==capture[site-0x1000:site-0x1000+5],f'independent recruitment call bytes {site:X}')
check(struct.unpack('<Q',read(0x1440FF8+0x28,8))[0]==0x140733D30,'employment result class and caller identity')
check(read(0x930D20,5)==bytes.fromhex('48895c2408'),'text relay displaces exactly one five-byte MOV; no RSP or RIP change')
check(read(0x841660,5)==bytes.fromhex('48895c2408'),'speech relay preserves exactly one five-byte MOV with original five-argument ABI')
check(struct.unpack('<Q',read(0x146E290,8))[0]==0x140861480,'diplomacy message uses the same native acknowledgement and close lifecycle')
check(read(0x86149C,6)==bytes.fromhex('899078010000'),'native +190 stores EDX in dialog+178')
check(read(0x8614A2,1)==b'\xe9' and 0x8614A7+struct.unpack('<i',read(0x8614A3,4))[0]==0x856840,'native acknowledgement starts the original closing animation, beyond storing the result')
# Leave 4F7170 cleans an opened dialog before delivering the parent callback.
check(read(0x4F7190,1)==b'\xe8' and 0x4F7195+struct.unpack('<i',read(0x4F7191,4))[0]==0x862D00,'Leave queries dialog state even while opened')
check(read(0x4F71A3,6)==bytes.fromhex('ff9098010000') and read(0x4F71B6,3)==bytes.fromhex('ff5028') and read(0x4F71CD,2)==bytes.fromhex('ff10'),'Leave closes layout and invokes deleting destructor before parent callback')
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
    '008108C0':['param_1 + 0x478','+ 0x1b0','100,&local_50'],
    '00841660':['param_1 + 0x478','param_2 * 8','FUN_140841c30'],
    '0044CAF0':['void FUN_14044caf0(longlong param_1)','FUN_14056fe10()'],
    '0022CA00':['param_1 + 0xe0','+ 0xdf30'],
    '0022ECC0':['FUN_14022ed00(param_1,uVar1)'],
    '00815EA0':['*(undefined4 *)(param_1 + 0x4f8) = 0','*(undefined4 *)(param_1 + 0x4f8) = 1'],
    '00733D30':['FUN_140842380(param_1,param_1[0x96],local_818,1)','FUN_140842380(param_1,param_1[0x97],local_818,1)','3 - (uint)((int)param_1[0x98] != 0)'],
    '00842380':['longlong param_5,undefined4 param_6','EventView::CInputWaitState','FUN_140571ef0(param_1)'],
    '001F16A0':['FUN_140860d50','param_1 + 0x478','lVar1 + 0x188'],
    '00844030':['param_1 + 0x1f8','param_1 + 0x178','FUN_140841c30'],
    '0083BE30':['FUN_140861380(param_1 + 0x470)','FUN_140012540'],
    '00861380':['param_1 + 8','param_1 + 0xc','0x7fffffff'],
    '00863270':['param_1[0x35] + 0x22c','FUN_140863c70','+ 400'],
    '00676870':['0x82','0x16280','0x1b00','0x140'],
    '002417A0':['param_1 + 0x8a','0x55'],
    '0067B1E0':['param_1 + 0x48','param_1 + 0x488','param_1 + 0x490','param_1 + 0x480'],
    '004F7170':['param_1 + 0x48','param_1 + 0x470','0x198'],
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
picture_prepare=code.split('static bool prepare_picture_link(',1)[1].split('static void pop()',1)[0]
check('memcpy' not in picture_prepare and 'absolute_jump(at,hook)' in picture_prepare,'picture replacement never relocates native relative tail jumps')
check('fn<void(__fastcall *)(Obj,int,int)>(0x842A90)(pointer(object,0x1B8),id,0)' in code and
      'fn<void(__fastcall *)(Obj)>(0x835230)(pointer(object,0x1B8))' in code,'original picture route reconstructs both tail-wrapper ABIs')
split=(P/'src/native_split_install.inl').read_text(encoding='utf-8')
check('if(f.advisor_briefing && !advisor_briefing_guards_match())split_disable(f.advisor_briefing,26);' in split,'existing split compatibility gate disables all stage4 hooks on fingerprint mismatch')
# 사용자 2026-10-28 설계 정정 replaces the earlier overly strict no-hook assertion.
for scene in ('0x6C43B0','0x6C4670','0x41D550'):
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
check('if(diplomatic && dialogue_ready(false))' in code and 'if(!diplomatic)return;' in simple,'entry-bypassing ordinary and person message states preserve native handling')
entry=code.split('static bool entry_ready(',1)[1].split('static bool prepare_message_link(',1)[0]+code.split('static bool capture_event(',1)[1].split('static void __fastcall set_text(',1)[0]
entry=re.sub(r'//[^\n]*', '',entry)
for address in ('0x83A450','0x1D3940','0x1D3C20','0x1D3AB0','0x844030','0x571EF0','0x12540'):
    check(address not in entry,'entry capture never calls push/suspend/yield/pop '+address)
for token in ('message_entry_counts','message_pass_reasons'):
    check(token not in release_code,'NO_RUNTIME_LOG excludes entry counters '+token)
check('restore_link(scene_links[--i]);restore_link(event_link);for(unsigned i=3;i;)restore_link(message_links[--i]);' in code and 'published=publish_link(message_links[i])' in code and 'published && publish_link(event_link)' in code and 'published=publish_link(scene_links[i])' in code,'all seven entries participate in transactional install and rollback')
check(code.index('if(!guards_match())return false;',code.index('static bool install()'))<code.index('publish_link(message_links[i])'),'fingerprint/prologue mismatch publishes neither entry hook')
for address,prologue in ((0x1F0AD0,'488bc455488d68a1'),(0x1F09E0,'488bc4574881ec80000000'),(0x1F0DD0,'488bc455488d68a1'),(0x842380,'405556574154'),(0x6C43B0,'40574883ec70'),(0x6C4670,'40574883ec70'),(0x41D550,'488bc44156')):
    check(read(address,len(bytes.fromhex(prologue)))==bytes.fromhex(prologue),'whole non-RIP-relative entry instructions '+hex(address))
# PE exception-table bounds independently establish whole function fingerprint sizes.
ex_rva,ex_size=struct.unpack_from('<II',b,pe+24+112+3*8)
bounds={a:e-a for a,e,_ in struct.iter_unpack('<III',read(ex_rva,ex_size))}
check(bounds[0x1F0AD0]==0x2FA and bounds[0x1F09E0]==0xE9 and bounds[0x1F0DD0]==0x1D4 and bounds[0x842380]==0x1E6,'entry FNV guards cover full functions through RET, using PE runtime function bounds')
check(bounds[0x6C43B0]==0x92 and bounds[0x6C4670]==0x92 and bounds[0x41D550]==0xCB,'scene entry guards cover all native wrapper instructions through RET')
check(read(0x845E90,12)==bytes.fromhex('488b12488b4908e924a8d2ff'),'both scene functors only load caller+8 and tail-call 5706C0 resume; no params writeback')
for table in (0x144DCE0,0x144F838):
    check(struct.unpack('<Q',read(table+0x10,8))[0]==0x140845E90,'scene completion callback is resume-only '+hex(table))
check(read(0x68744E,0x21)==bytes.fromhex('498b06488983b0040000498b4608488983b8040000418b46108983c0040000eb03'),'recruitment constructor reads params into independent state fields only')
for vt,name in ((0x1447030,'CDiscoveryItemDialog'),(0x1446AC0,'CInfoSummaryGroundAdvantageDiscovery'),(0x1446C60,'CInfoSummarySceneryDiscovery')):
    col=struct.unpack('<Q',read(vt-8,8))[0]-0x140000000;td=struct.unpack('<I',read(col+12,4))[0]
    check(name.encode() in read(td+16,len(name)+10),'independent discovery presentation class '+name)
check(struct.unpack('<Q',read(0x1447030+0x1A8,8))[0]==0x1406EC0D0 and struct.unpack('<Q',read(0x1447030+0x1A0,8))[0]==0x1406EBE70,'discovery type0 item and type2 secondary item use read-only presentation methods')
scene_capture=code.split('static bool scene_ready(',1)[1].split('static bool prepare_message_link(',1)[0]
for formatter in ('0x30A2F0','0x30DBB0','0x75AAE0','0x733D30','0x841660','0x6873C0','0x6863D0','0x417250','0x571EF0'):
    check(formatter not in scene_capture,'summary capture invokes neither original message formatting nor scene construction '+formatter)
check('e.text=e.report=summary' in scene_capture and 'scene_ready(4)' in scene_capture and 'scene_ready(5)' in scene_capture and 'scene_ready(6)' in scene_capture,'one owned summary uses the same native gates on each scene entry')
for site,target in ((0x1F0AF8,0x332E90),(0x1F0C13,0x1D3940),(0x1F0D2E,0x1D3940),(0x1F0DA7,0x571EF0),(0x1F0A00,0x332E90),(0x1F0AA2,0x1D3C20),(0x1F0AAA,0x571EF0)):
    call=read(site,5)
    check(call[0]==0xE8 and site+5+struct.unpack_from('<i',call,1)[0]==target,'independent native entry call '+hex(site))
# Exact whitelist provenance: independently decode RTTI COL/type descriptor and frame slots.
for vt,name,frame in [(0x14471E8,'CSearchDiscoveryState',0x731EA0),(0x1440FF8,'CStrategyEmployTalkState',0x733D30),(0x143BF50,'CStrategyAdviceState',0x731F70),(0x14653E8,'CEventModalState',0x83A880),(0x1447C70,'CStrategyCreditRankupState',0x7334A0)]:
    col=struct.unpack('<Q',read(vt-8,8))[0]-0x140000000
    td=struct.unpack('<I',read(col+12,4))[0]
    check(name.encode() in read(td+16,len(name)+10),'independent PE RTTI owner '+name)
    check(struct.unpack('<Q',read(vt+0x28,8))[0]==0x140000000+frame,'independent PE frame '+name)
    check(hex(vt)[2:].upper() in profile,'profile records exact owner '+name)
# 사용자 2026-10-28 입력·대기창 보정: result-only wait bypass with timeout=0.
check(read(0x862C24,7)==bytes.fromhex('488b4208448900'),'CCmnWaitTimeDialog init loads b and writes a DWORD result through it')
check(bounds[0x304640]==0xB9 and read(0x304640,11)==bytes.fromhex('488bc4574881ec80000000'),'time wait complete PE function bound and eleven non-relative entry bytes')
check(read(0x304640,0xB9)==capture[0x304640-0x1000:0x304640-0x1000+0xB9],'independent current wait entry capture')
check('prepare_message_link(time_wait_link,0x304640,narration_prologue' in code and 'publish_link(time_wait_link)' in code and 'restore_link(time_wait_link)' in code,'guarded wait entry participates in transactional prepare/publication/rollback')
time_body=code.split('static void __fastcall time_wait(',1)[1].split('static void name_speaker(',1)[0]
check('result && !duel_running() && defer()' in time_body and 'Obj m=pointer(image,0x2783730);return m && dword(m,0x10)!=0;' in code and 'return !duel_running();   // stored; a duel line is also shown natively' in code and '0x332E90' in time_body and '0x56FC20' in time_body and '*result=0;return;' in time_body and 'original_time_wait(milliseconds,result,allow_skip)' in time_body,'wait uses defer/interaction, native mode/caller, zero timeout, original fallback arguments')
for address in ('0x243BD0','0x571EF0','0x12540'):
    check(address not in time_body,'time entry never pushes/yields/pops '+address)
check('Obj top=top_state();bool player=top && pointer(top,0)==image+0x13D63A0 && !interaction();' in code and 'if(!player){open=false;rendered_open=false;display_cache.open=false;}' in code,'native log frame closes all cached open flags on covered player state')
col=struct.unpack('<Q',read(0x13D63A0-8,8))[0]-0x140000000;td=struct.unpack('<I',read(col+12,4))[0]
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
                   'advisor_portrait_sample','portrait_sample_logs','portrait_accept_logs')
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
run=subprocess.run([str(release),str(P/'legacy/FogMod_Source/native_game_analysis/running_code_text.bin')],cwd=out,capture_output=True)
check(run.returncode==0,'NO_RUNTIME_LOG behavior parity: '+(run.stdout+run.stderr).decode('utf-8','replace')[-3000:])
print(run.stdout.decode('utf-8','replace').strip())
for token in (*diagnostic_tokens,'advisor_briefing_uncollected_top','advisor_briefing_interactive_after_passive'):
    check(token.encode() not in release.read_bytes() and token.encode('utf-16-le') not in release.read_bytes(),'NO_RUNTIME_LOG binary excludes diagnostic strings: '+token)
print(json.dumps(dict(status='advisor_briefing_native_verified',checks=checks,slots=len(slots),live_game=False)))
