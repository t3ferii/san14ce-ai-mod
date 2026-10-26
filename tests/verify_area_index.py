"""Offline identity/byte/ABI evidence. Steam .text is encrypted: decoded bytes are
checked against the cached unpacked EXE and the existing running-code capture.
No live process, EXE execution, memory writes or new reverse-engineering session.
"""
from pathlib import Path
import hashlib,json,re,struct,sys
P=Path(__file__).resolve().parents[1]
original=(P.parent/'SAN14CE_KO.exe').read_bytes()
decoded=(P/'build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe').read_bytes()
captured=(P/'legacy/FogMod_Source/native_game_analysis/running_code_text.bin').read_bytes()
def reader(data):
    pe=struct.unpack_from('<I',data,60)[0]
    count=struct.unpack_from('<H',data,pe+6)[0];optional=struct.unpack_from('<H',data,pe+20)[0]
    sections=[struct.unpack_from('<IIII',data,pe+24+optional+i*40+8) for i in range(count)]
    def read(at,size):
        for vs,va,rs,ro in sections:
            if va<=at and at+size<=va+max(vs,rs):return data[ro+at-va:ro+at-va+size]
        raise KeyError(hex(at))
    return read
rd=reader(decoded);raw=reader(original)
checks=0;fail=[]
def ok(value,why):
    global checks
    checks+=1
    if not value:fail.append(why)
def fnv(b):
    h=0xcbf29ce484222325
    for c in b:h=((h^c)*0x100000001b3)&0xffffffffffffffff
    return h
def bytes_at(at,hex_bytes,why):
    b=bytes.fromhex(hex_bytes)
    ok(rd(at,len(b))==b and captured[at-0x1000:at-0x1000+len(b)]==b,why)
def call(at,target,why):
    b=rd(at,5)
    ok(b[0]==0xe8 and at+5+struct.unpack_from('<i',b,1)[0]==target and
       b==captured[at-0x1000:at-0x1000+5],why)
ok(hashlib.sha256(original).hexdigest()=='87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a','current Steam EXE identity')
ok(hashlib.sha256(decoded).hexdigest()=='1fbe6be7466a66466fe97cf3ab7d49392913346879c7d40de7f4eaf86eef81c6','decoded build identity')
src=(P/'src/native_area_index.inl').read_text(encoding='utf-8')
guards=[tuple(int(x,16) for x in m) for m in re.findall(r'\{0x([\dA-F]+),0x([\dA-F]+),0x([\da-f]+)ull\}',src)]
ok([a for a,_,_ in guards]==[0x2C9E30,0x231EB0,0x332F80,0x22FB10,0x229EA0],'all five required guards registered')
for at,size,want in guards:
    ok(fnv(rd(at,size))==want,f'{at:X} full decoded function FNV')
    ok(fnv(captured[at-0x1000:at-0x1000+size])==want,f'{at:X} full captured running function FNV')
    ok(rd(at+size-1,1)==b'\xc3',f'{at:X} complete fingerprint through RET')
sites=[0x279290,0x7E4B8E,0x7F962F]
for site in sites:
    call(site,0x2C9E30,f'{site:X} E8 -> 2C9E30')
    bytes_at(site+5,'85c0',f'{site:X} caller consumes EAX int result')
ok([int(x,16) for x in re.search(r'callers\[\]=\{([^}]+)',src).group(1).split(',')]==sites,'runtime guards cover three native callers')
bytes_at(0x2C9E30,'488bc4574883ec70','eight displaced bytes are complete MOV/PUSH/SUB, no relative operand')
bytes_at(0x2C9E4C,'488bf1','only area argument RCX retained in RSI')
call(0x2C9E4F,0x32F500,'area validity uses original 32F500')
bytes_at(0x2C9E54,'85c00f84e2000000','invalid area returns 32F500 EAX unchanged')
bytes_at(0x32F500,'4883ec284885c97414488b01ff501885c0740ab8010000004883c428c333c04883c428c3','native validity returns exactly EAX 0/1 via vtable+18')
call(0x2C9E68,0x231EB0,'native region iterator start')
call(0x2C9E76,0x232000,'native region iterator end')
call(0x2C9E93,0x22FB10,'native cell facility lookup')
call(0x2C9E9E,0x32F500,'native building validity')
call(0x2C9ED4,0x32F500,'native template validity')
bytes_at(0x2C9EA7,'0fb6431283f81e','building type id is byte+12, table last id30')
bytes_at(0x2C9EB2,'488b055f404b02488b9cc858390700','live global type table g+73958')
bytes_at(0x2C9EDD,'0fb6432c83f802740883c0fd83f8017702ffc7','byte type+2C matches 2 or unsigned(type-3)<=1')
bytes_at(0x2C9F32,'0fb64e6e3bf9400f9dc58bc5','unsigned byte area+6E <= count, int EAX0/1')
bytes_at(0x231ECE,'488b0543c054024c8d88f0ca06004c8d8070e20000492b09','iterator g+E270..6CAF0 and area0 subtraction')
bytes_at(0x231EE6,'48b81bca6b28afa1bc0648f7e948c1fa02488bc248c1e83f4803d0','signed native area offset / 0x98')
bytes_at(0x332F80,'448b4118418d40ff3df3010000760333c0c3488b020fb7481233c0413bc80f94c0c3','predicate unsigned idx1..500 and ushort cell+12')
bytes_at(0x22FB10,'0fb7411a3db80b00007712','facility ushort cell+1A, ids0..3000 else0')
bytes_at(0x229EA0,'0fb741123df40100007712','region ushort cell+12, ids0..500 else0')
ok((0x6CAF0-0xE270)//8==48400 and 0x6CAE8==0xE270+48399*8,'48,400 pointer slots and last slot identity')
ok(raw(0x2C9E30,8)!=rd(0x2C9E30,8),'Steam text encrypted; no false claim of on-disk decoded instructions')
for at,size in [(0x232000,0x68)]:
    ok(rd(at,size)==captured[at-0x1000:at-0x1000+size],'end iterator decoded/capture bytes agree')
adapter=(P/'src/native_adapter.cpp').read_text(encoding='utf-8-sig')
split=(P/'src/native_split_install.inl').read_text(encoding='utf-8-sig')
ok('f.area_index = fast && feature_key(config, L"performance", L"area_index", false);' in adapter,'feature guarded by performance group and safe missing-key default')
ok('if(f.area_index && !area_index_guards_match())split_disable(f.area_index,22)' in split,'per-feature incompatibility disables only area_index')
ok('if(mod_features.area_index)area_index::install();' in split,'convenience feature install registered')
ok(src.count('COST_SCOPE("area_index_check")')==1,'exactly one cost scope')
ok('#ifndef NO_RUNTIME_LOG\n    COST_SCOPE("area_index_check")' in src and '#ifndef NO_RUNTIME_LOG\n    if(verify)' in src,'diagnostic scope and native verification compiled out of release')
ok('mod_setting(L"diagnostics",L"area_index_verify",0)' in src and 'verify=dev_group()' in src,'verify switch defaults0 under diagnostics group')
ok('TryAcquireSRWLockShared' in src and 'TryAcquireSRWLockExclusive' in src,'shared read, nonblocking exclusive rebuild/fallback')
keys=set()
for path in (P/'src').iterdir():
    if path.suffix in ('.h','.cpp','.inl'):keys.update(re.findall(r'COST_SCOPE\("([^"\n]+)"',path.read_text(encoding='utf-8-sig')))
maximum=int(re.search(r'maximum=(\d+)',(P/'src/native_cost_profile.h').read_text(encoding='utf-8')).group(1))
ok(len(keys)<=maximum,'cost profile registry capacity')
for path in [P/'installer/InstallerGui.cs',P/'distribution/FeatureSelect.ps1',P/'FeatureSelect.ps1']:
    text=path.read_text(encoding='utf-8-sig')
    ok(text.count('"area_index"' if path.suffix=='.cs' else "Key='area_index'")==1,str(path.name)+' performance key catalog')
    ok(text.count('"area_index_verify"' if path.suffix=='.cs' else "Key='area_index_verify'")==1,str(path.name)+' verify key catalog')
print(json.dumps({'status':'area_index_static_verified' if not fail else 'failed','checks':checks,'failures':fail,
                  'steam_text_encrypted':True,'cost_scope_literals':len(keys),'cost_scope_capacity':maximum,'gameplay_verified':False}))
sys.exit(1 if fail else 0)
