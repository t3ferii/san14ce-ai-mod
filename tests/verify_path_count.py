"""Read-only Steam identity, decoded/captured bytes, ABI, hook and catalog checks.
Steam .text is encrypted on disk; this explicitly distinguishes saved runtime
bytes from a fresh live-game verification. No game launch, live read or write.
"""
from pathlib import Path
import hashlib,json,re,struct,sys
P=Path(__file__).resolve().parents[1]
steam=(P.parent/'SAN14CE_KO.exe').read_bytes()
decoded=(P/'build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe').read_bytes()
captured=(P/'legacy/FogMod_Source/native_game_analysis/running_code_text.bin').read_bytes()
def sections(data):
    pe=struct.unpack_from('<I',data,60)[0];table=pe+24+struct.unpack_from('<H',data,pe+20)[0]
    for i in range(struct.unpack_from('<H',data,pe+6)[0]):
        at=table+40*i
        yield data[at:at+8].rstrip(b'\0'),*struct.unpack_from('<IIII',data,at+8)
def read(at,n):
    for _,vs,va,rs,ro in sections(decoded):
        if va<=at and at+n<=va+rs:return decoded[ro+at-va:ro+at-va+n]
    raise KeyError(hex(at))
checks=0;fail=[]
def ok(value,why):
    global checks
    checks+=1
    if not value:fail.append(why)
def fnv(data):
    h=0xcbf29ce484222325
    for b in data:h=((h^b)*0x100000001b3)&0xffffffffffffffff
    return h
def code(at,hex_bytes,why):
    want=bytes.fromhex(hex_bytes)
    ok(read(at,len(want))==want and captured[at-0x1000:at-0x1000+len(want)]==want,why)
ok(hashlib.sha256(steam).hexdigest()=='87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a','current Steam EXE identity')
ok(hashlib.sha256(decoded).hexdigest()=='1fbe6be7466a66466fe97cf3ab7d49392913346879c7d40de7f4eaf86eef81c6','decoded build identity')
ok(read(0x183A30,0x9A2)==captured[0x183A30-0x1000:0x1843D2-0x1000],'complete decoded/captured function agreement through RET')
ok(fnv(read(0x183A30,0x9A2))==0x11cbf08ff66c9feb,'full function FNV fingerprint')
code(0x183A30,'488bc444894820','two complete nonrelative MOV instructions (7 bytes), leaf unwind')
code(0x183A37,'4c89401889501048894808','R8 goals, EDX start and RCX ctx saved')
code(0x183A72,'48634d77488b7d6f','sixth arg clear bytes (RBP+77) and fifth arg node buffer (RBP+6F)')
code(0x1844B7,'41b901000000','caller passes goal_count in R9D')
code(0x1844BD,'896c2430','seventh arg flags at caller RSP+30')
code(0x1844C1,'498bcd','caller RCX context')
code(0x1844C4,'c744242810bd0000','sixth arg at caller RSP+28 (0xBD10 here)')
code(0x1844CC,'4c89442420','fifth arg buffer at caller RSP+20')
code(0x1844D1,'4c8d8424b0000000','R8 points to goal')
code(0x1844E0,'e84bf5ffff4885c0','E8 to 183A30 and RAX pointer/null consumed')
code(0x1844EF,'488b4010','result parent pointer at +10')
# Scan the entire executable code section for relative CALL/JMP encodings. A
# cached instruction boundary independently confirms the sole E8 is executable.
relative={0xe8:[],0xe9:[]}
for name,vs,va,rs,ro in sections(decoded):
    if name!=b'.text':continue
    text=decoded[ro:ro+rs]
    for i in range(len(text)-4):
        op=text[i]
        if op in relative and va+i+5+struct.unpack_from('<i',text,i+1)[0]==0x183A30:relative[op].append(va+i)
ok(relative[0xe8]==[0x1844E0] and relative[0xe9]==[],'all .text direct CALL sites: one; tail JMP sites: zero')
asm=(P/'research/decomp_cache/1fbe6be7466a6646/001843E0_asm.txt').read_text(encoding='utf-8')
ok('1401844e0 CALL 0x140183a30' in asm,'cached decoded instruction boundary confirms direct caller')
absolute=struct.pack('<Q',0x140183A30)
ok(absolute not in decoded,'no embedded absolute function-address literal (does not prove absence of computed indirect calls)')
source=(P/'src/native_path_count.inl').read_text(encoding='utf-8')
core=(P/'src/path_count.h').read_text(encoding='utf-8')
for text,name in [(source,'module'),(core,'pure core')]:ok('#ifndef NO_RUNTIME_LOG' in text,name+' omitted in release')
ok('COST_SCOPE' not in source and 'COST_SCOPE' not in core,'no new cost key')
ok('static constexpr unsigned entry=0x183A30,size=0x9A2;' in source and '0x11cbf08ff66c9febull' in source,'runtime full-function guard constants')
ok('if(!guards_match()) {log_event("path_count_guard_rejected");return false;}' in source,'guard failure only skips diagnostic hook')
ok('!dev_group() || !diagnostic_logging || !mod_setting(L"diagnostics",L"path_count",0)' in source,'three runtime switch gates and default0')
ok('if(!root_ready)' in source and 'path_count_root_unavailable' in source,'fail closed without safe force boundary')
ok('RtlAddFunctionTable' in source and 'const unsigned char unwind[]={1,7,0,0}' in source,'copied leaf prologue registered with unwinder')
ok('original(ctx,start,goals,count,buffer,clear_bytes,flags)' in source and 'return output;' in source,'original seven-argument call and pointer return')
ok('thread_local path_count_core::Force force' in source and 'AcquireSRWLockExclusive(&day_lock)' in source,'force TLS and synchronized day totals')
ok('~' not in source,'no loader-lock TLS destructor logger')
progress=(P/'src/native_progress.inl').read_text(encoding='utf-8')
ok(progress.count('PATH_COUNT_FLUSH();')==1 and progress.index('PATH_COUNT_FLUSH();')>progress.index('__finally {progress_sample=previous;}'),'flush after original root return and TLS scope restoration')
adapter=(P/'src/native_adapter.cpp').read_text(encoding='utf-8-sig')
ok(adapter.count('#include "native_path_count.inl"')==1 and adapter.index('#include "native_path_count.inl"')<adapter.index('#include "native_progress.inl"'),'one include before root wrapper')
split=(P/'src/native_split_install.inl').read_text(encoding='utf-8')
# User/Claude 2026-10-25: path_fast and path_count share 183A30; successful fast install owns it.
ok('#ifndef NO_RUNTIME_LOG\n    if(path_fast::active)log_event("path_count_skipped_path_fast");\n    else path_count::install(progress_native_active && progress_native_profile,[]() {return progress_sample!=nullptr;});\n#endif' in split,'diagnostic install preserves root TLS identity and skips occupied 183A30')
label='경로 탐색 호출 횟수 기록';note='세력 AI가 경로 탐색을 몇 번 부르고 같은 출발지·목적지·조건을 되풀이하는지 진단 로그에 남깁니다. 게임 동작은 바뀌지 않습니다.'
for name in ['installer/InstallerGui.cs','distribution/FeatureSelect.ps1','FeatureSelect.ps1']:
    text=(P/name).read_text(encoding='utf-8-sig');lines=[s for s in text.splitlines() if ('"path_count"' if name.endswith('.cs') else "Key='path_count'") in s]
    ok(len(lines)==1 and label in lines[0] and note in lines[0] and ('"기타(개발용)", "diagnostics", "diagnostics", "path_count", 0' in lines[0] if name.endswith('.cs') else "Group='diagnostics'; Section='diagnostics'; Key='path_count'; Default=0" in lines[0]),name+' catalog exact label, note, group, default')
ok(json.loads((P/'development/features.json').read_text(encoding='utf-8-sig'))['modules']['path-count']['tests']==['path-count'],'focused check route registered')
print(json.dumps(dict(status='path_count_static_verified' if not fail else 'failed',checks=checks,failures=fail,direct_call_sites=[hex(x) for x in relative[0xe8]],tail_jump_sites=relative[0xe9],steam_text_encrypted=True,gameplay_verified=False),ensure_ascii=False))
sys.exit(bool(fail))
