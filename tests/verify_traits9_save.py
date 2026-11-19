import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Focused native-site, layout and unchanged save-contract audit."""
from pathlib import Path
import base64,collections,hashlib,json,os,re,struct,sys,zlib
P=Path(__file__).resolve().parents[1];G=P.parent
# User 2026-11-07 W7 / plan D-3: the frozen payload and resource bundle are
# the inputs; never regenerate code by executing Editor_Source or borrowed Python.
sys.path.insert(0,str(P/'tools'))
import traits9_layout as tl
LAYOUT_NODES={0x9EA0EB77:(125,129),0xF1D669AF:(125,129),0x49F89AB3:(108,112),0x7C471B31:(108,112)}
code=(P/'build/ui_native_analysis/fixture/1a4a447d_text.bin').read_bytes();checks=0
def check(v,name):
    global checks
    assert v,name
    checks+=1
def read(r,n):return code[r-4096:r-4096+n]
ranges=[]
# the patch tables are the hand-written src/traits9_sites.h (2026-11-18 local235, replaces the frozen traits9_payloads.h;
# RVAs of the reference build 1a4a447d after ga_expand), not the old generator report
pay=ga_expand.expand((P/'src/traits9_sites.h').read_text(encoding='utf-8'))
def arr(s):return bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',s))
core_text=pay[pay.index('traits9_core[]'):]
for rva,n,before in re.findall(r'\{(0x[0-9A-Fa-f]+),(\d+),\{([^}]*)\},\{[^}]*\}\}',core_text):
    rva=int(rva,16);before=arr(before);check(len(before)==int(n),'core size');check(read(rva,len(before))==before,'core before '+hex(rva))
    ranges.append((rva,len(before)))
site_text=pay[pay.index('traits9_sites[T9_SITE_COUNT]'):pay.index('traits9_core[]')]
for rva,n,before in re.findall(r'\{(0x[0-9A-Fa-f]+),(\d+),\{([^}]*)\}\}',site_text):
    rva=int(rva,16);before=arr(before);check(len(before)==int(n),'site size');check(read(rva,len(before))==before,'UI '+hex(rva))
    check(5<=len(before)<=32,'complete jump site');ranges.append((rva,len(before)))
check(len(ranges)==14,'5 core patches + 9 UI sites (got %d)'%len(ranges))   # local236 +1: edit officer list 4CCBE3
for path in (P/'src').glob('*'):
    # 사용자 2026-11-05 Layout 호환 설계 §8: layout_registry.h is the Layout manager's data-only registry of these very patch
    # sites (owner traits9, yield-only); tests/verify_layout_registry.py checks it against this same payload table.
    if path.suffix not in ('.cpp','.h','.inl') or path.name in ('native_traits9.inl','traits9_sites.h','modpack_tables.h','layout_registry.h','game_addresses_data.inl'):continue
    # game_addresses_data.inl (generated, 2026-11-05) is the address table: its RVA/id integer literals are table data, never a hook site.
    values={int(x,16) for x in re.findall(r'0[xX][0-9a-fA-F]+',path.read_text(encoding='utf-8'))}
    # trait_hover (2026-11-06) only reads the traits9-patched count compare 23064A (83 FB 05|09) to detect nine traits; no write.
    reads={0x2320AA} if path.name=='trait_hover_profile.h' else set()
    hits=[hex(v) for a,n in ranges for v in values if a<=v<a+n and v not in reads]
    check(not hits,'existing hook candidate '+path.name+str(hits))
# Original native person load already copies all nine u16 trait IDs at +118.
check(read(0x321FB4,3)==bytes.fromhex('83ff09'),'native person copy count remains nine')
check(0x118+9*2==0x12A and 0x12A<0x12C,'nine traits fit existing record before next field')
# User 2026-11-07 W7: pin the reviewed CURRENT save sources independently of
# an installed release manifest or a previous-build idmap. W4-5 host pins stay
# identical; W7 changes only the generic binding include (merit serialization
# remains checked by merit/save-host fixtures). Future edits require review.
# Reviewed 2026-11-16: native_merit_save.inl changed after the W7 pin only by the local161 load-host phase log (diagnostic build,
# reads only) and the local166/183 game-start holder guard (start_state_guard, a crash fix on the start state); neither touches the
# merit sidecar format, its read/write path or the save binding. native_awards_save.inl: local23 COST_SCOPE markers only (profiling, no
# code in release). merit_bond.h: local17 contains_ext/list_ext, a live family extension with no stored state (format unchanged).
SAVE_CONTRACT_PIN={'src/native_merit_save.inl': '8224b0fc10cd703c9c55df589244b2e43d79b6ee520dca6a7454f80606d322f5', 'src/native_awards_save.inl': 'd52436cf9a654ca13d1f24d00b0a5a58c162c6b9664875a3faa5024265876b5f', 'src/sidecar_cache.h': 'eacc735d3f39fe57698485cc28c3080b283399dd6d6af5e9eb6759b597ede1ec', 'src/merit_bond.h': '7d8cdaeb50571796bede1da56ed45ed7f0018b154c75cc3d2fe0008875f3966c', 'src/save_binding.h': 'bcb431dc6c4996f8402c8a9e8a1c00cf9776c13001d791386487e451ae4ca1b4'}
for name,expected in SAVE_CONTRACT_PIN.items():
    check(hashlib.sha256((P/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==expected,
          'save source matches reviewed contract '+name)
new=(P/'src/native_traits9.inl').read_text(encoding='utf-8')
check(not any(s in new for s in ('RemoteWrite','RemoteRead','SaveHost','WriteFile','CreateFile')),'no new save hook or save writer')
# 2026-11-16: the layout audit no longer pins the whole game index (ad512a20, the index before the 2026-11-05 game update) nor the
# research candidate index (built 11-07 from an index that already held other features' bank-26 entries). The game index now also
# carries a foreign mod's eight bank-24 layouts, and every feature shares one install bank. What is audited instead, independent of
# what else is installed: the frozen traits9 patch names exactly the four UI layouts, the shipped bundle carries those four records
# byte-for-byte, they were built against this game's index, and applying them changes nothing else.
patch=json.loads((P/'research/traits9/layout_patch/traits9_layout.json').read_text(encoding='utf-8-sig'))
bank=(P/'research/traits9/layout_patch/traits9_layout.bin').read_bytes()
check(hashlib.sha256(bank).hexdigest()==patch['bank_sha256'],'frozen traits9 bank hash')
records={r['ktid']:r for r in patch['records']}
check(len(records)==len(patch['records']) and set(records)==set(LAYOUT_NODES),'traits9 patch names exactly the four UI layouts')
ship=json.loads((P/'build/input_package/traits9_layout.json').read_text(encoding='utf-8-sig'))
ship_bank=(P/'build/input_package/traits9_layout.bin').read_bytes()
check(hashlib.sha256(ship_bank).hexdigest()==ship['bank_sha256'],'shipped layout bank hash')
for k,r in records.items():
    same=[x for x in ship['records'] if x['ktid']==k]
    check(len(same)==1,'shipped bundle carries traits9 layout %08X'%k)
    x=same[0]
    check((x['original_entry'],x['offset'],x['size'],x['dsz'])==(r['original_entry'],r['offset'],r['size'],r['dsz']),'shipped record unchanged %08X'%k)
    check(ship_bank[x['offset']:x['offset']+x['size']]==bank[r['offset']:r['offset']+r['size']],'shipped layout bytes unchanged %08X'%k)
# The game index as it was before our install: every entry our installer applied (one shared bank) gets its saved previous bytes
# back. The install record lives in the install host (game root/AI_TacticsMod): P itself in the main checkout, the game's checkout
# when this runs from a sibling worktree.
header,entries=tl.parse((G/'0002/ScreenLayout.rdb').read_bytes())
state_file=G/'AI_TacticsMod/backup/input_state.json'
state=json.loads(state_file.read_text(encoding='utf-8-sig')) if state_file.is_file() else {}
installed=state.get('layout') if state.get('installed') else None
if installed:
    previous=installed.get('previous_entries') or {}
    originals={x['ktid']:x['original_entry'] for x in ship['records']}
    for i,e in enumerate(entries):
        k=tl.ktid(e);at=tl.locator(e)
        if k in installed['applied'] and at and at[2]==installed['bank']:
            # records installed before previous_entries existed (2026-11-03) go back to the bundle's original entry
            entries[i]=base64.b64decode(previous.get(str(k),originals.get(k,'')))
    check(installed['bank'] not in tl.used_banks(entries),'installed layout entries revert in memory')
old={tl.ktid(e):e for e in entries}
for k,r in records.items():
    check(old.get(k)==base64.b64decode(r['original_entry']),'traits9 record built against this game index %08X'%k)
data,applied,skipped=tl.apply(tl.build(header,entries),patch['records'],patch['legacy_bank'])
check(sorted(applied)==sorted(records) and not skipped,'all four traits9 records apply')
candidate={tl.ktid(e):e for e in tl.parse(data)[1]}
check(set(old)==set(candidate),'all other layout entries preserved')
check({k for k in old if old[k]!=candidate[k]}==set(LAYOUT_NODES),'only four UI layouts change')
# User 2026-11-07: origlib is incomplete and is not part of this plan.
# Keep the existing traits9 index parser and the reviewed chunk decoder below;
# the old generator/borrowed module is no longer executed by the test.
def read_layout(entry):
    off,size,where=tl.locator(entry)
    check(where==31,'new layout bank')
    raw=bank[off:off+size]
    check(len(raw)==size and raw[:8]==b'IDRK0000','complete layout bank entry')
    compressed,decoded=struct.unpack_from('<QQ',raw,16)
    pos=size-compressed;body=bytearray()
    while pos<size:
        length,=struct.unpack_from('<I',raw,pos);pos+=4
        if length==0:break
        check(pos+length<=size,'compressed chunk bounds')
        body+=zlib.decompressobj().decompress(raw[pos:pos+length]);pos+=length
    check(len(body)==decoded,'decoded layout length')
    return bytes(body)
for ktid,(before_nodes,after_nodes) in LAYOUT_NODES.items():
    body=read_layout(candidate[ktid])
    section=0x4C
    while struct.unpack_from('<I',body,section)[0]!=1:section+=struct.unpack_from('<I',body,section+4)[0]
    check(after_nodes==before_nodes+4 and struct.unpack_from('<H',body,section+0x12)[0]==after_nodes,'appended four nodes')
result=dict(status='traits9_save_fixture_passed',checks=checks,save_record_layout_unchanged=True,
    existing_nine_slot_person_load=True,save_sidecar_sources_unchanged=True,only_four_ui_layouts_changed=True,
    gameplay_verified=False,live_save_reload_verified=False)
(P/'build/traits9_save_fixture.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result))
