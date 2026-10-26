"""Current-build caller/ABI checks for the existing dispatcher policy gate."""
from pathlib import Path
import hashlib,json,struct
P=Path(__file__).resolve().parents[1];ROOT=P.parent
raw=(P/'build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe').read_bytes()
sha=lambda b:hashlib.sha256(b).hexdigest()
assert sha(raw)=='1fbe6be7466a66466fe97cf3ab7d49392913346879c7d40de7f4eaf86eef81c6'
assert sha((ROOT/'SAN14CE_KO.exe').read_bytes())=='87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a'
pe=struct.unpack_from('<I',raw,60)[0];at=pe+24+struct.unpack_from('<H',raw,pe+20)[0]
sections=[struct.unpack_from('<IIII',raw,at+i*40+8) for i in range(struct.unpack_from('<H',raw,pe+6)[0])]
def read(r,n):
    vs,va,z,f=next(s for s in sections if s[1]<=r and r+n<=s[1]+s[2])
    return raw[f+r-va:f+r-va+n]
assert read(0x35662,5)==bytes.fromhex('e829721b00')
assert 0x35667+struct.unpack('<i',read(0x35663,4))[0]==0x1ec890
assert read(0x35667,3)==bytes.fromhex('4c8bf8') # command result kept in R15
assert read(0x356a4,5)==bytes.fromhex('bb02000000') # invalid result -> BT failure2
assert struct.unpack('<Q',read(0x1346af8,8))[0]==0x140034f10
policy=(P/'src/native_district_intercept.inl').read_text(encoding='utf-8')
link=(P/'src/native_district_intercept_link.inl').read_text(encoding='utf-8')
assert 'policy==kPolicySupport?(action==2 || action==3?Allow:BlockSupport):(military(action)?BlockDomestic:Allow)' in policy
assert 'kPolicyDomestic=4,kPolicySupport=5' in policy and 'district[0x17]' in policy and 'district[0x15]' not in policy
assert '(0x233750)(district)' in policy
assert 'valid(direct) && district==direct' in policy.replace('!valid(direct) || district==direct','valid(direct) && district==direct')
assert 'combat_module_loaded()' in link and 'prepare_link(link,0x1EC890,0xE9,stub)' in link
assert read(0x1EC890,16)==bytes.fromhex('405556574154415541564157488d6c24'),'dispatcher entry'
# Condition 84 (v12.19): C2610 jump table C38A0[0x54-1] -> stub C2F65 whose CALL at C2F73 is AEE60.
stub=struct.unpack('<I',read(0xC38A0+(0x54-1)*4,4))[0]
assert stub==0xC2F65 and read(0xC2F73,1)==bytes([0xE8]) and 0xC2F78+struct.unpack('<i',read(0xC2F74,4))[0]==0xAEE60
assert read(0xC2F70,16)==bytes.fromhex('498bcae8e8befeff0fb6d04883c4205d')
assert 'prepare_link(link,0xC2F73,0xE8,relay+3392)' in link and 'args[0]==5 && args[3]==15' in link and 'restricted_corps(corps)' in link
sortie=(P/'src/native_sortie.inl').read_text(encoding='utf-8')
i=sortie.index('static Obj __fastcall sortie_dispatch')
assert sortie.index('district_intercept::blocked(command)',i) < sortie.index('original_sortie_dispatch',i)
install=(P/'src/native_split_install.inl').read_text(encoding='utf-8')
assert 'district_intercept::prepare(s.intercept,r)' in install and 'publish_link(s.intercept)' in install
result={'status':'district_intercept_native_verified','native_call_rva':'0x35662',
        'native_return_rva':'0x35667','original_dispatch':'0x1EC890','invalid_result_bt_failure':2,
        'new_hooks':1,'owner':'1EC890 dispatcher: combat sortie_dispatch, else SAN14_Convenience.dll entry hook (never both)',
        'protected_native_departure_slot':'0x1346AF8 remains convenience/personnel owner',
        'policy_rules':{'corps+17=5 지원':'transport (2/3) only','corps+17=4 내정':'no 1/4/5/7/14'},'policy_byte_live_measured':True,'ui_policy_label_order_observed':False,
        'gameplay_verified':False,'build_or_install_performed_by_this_check':False}
out=P/'build/development/district_intercept';out.mkdir(parents=True,exist_ok=True)
(out/'native_verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
