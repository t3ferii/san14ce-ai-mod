import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Focused native-site, layout and unchanged save-contract audit."""
from pathlib import Path
import collections,hashlib,json,os,re,struct,sys,zlib
P=Path(__file__).resolve().parents[1];G=P.parent
# User 2026-11-07 W7 / plan D-3: the frozen payload and resource bundle are
# the inputs; never regenerate code by executing Editor_Source or borrowed Python.
sys.path.insert(0,str(P/'tools'))
import traits9_layout as tl
ORIGINAL_INDEX_SHA256='ad512a20793286138014ffe282ac60574f78053a4080c19262748664e213b6f7'
LAYOUT_NODES={0x9EA0EB77:(125,129),0xF1D669AF:(125,129),0x49F89AB3:(108,112),0x7C471B31:(108,112)}
code=(P/'build/ui_native_analysis/fixture/1a4a447d_text.bin').read_bytes();checks=0
def check(v,name):
    global checks
    assert v,name
    checks+=1
def read(r,n):return code[r-4096:r-4096+n]
ranges=[]
# the patch tables are the frozen src/traits9_payloads.h (W1 D-3, RVAs of the reference build 1a4a447d after ga_expand), not the old generator report
pay=(P/'src/traits9_payloads.h').read_text(encoding='utf-8')
def arr(s):return bytes(int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]{2})',s))
core_text=pay[pay.index('traits9_core[]'):]
for rva,n,before in re.findall(r'\{(0x[0-9A-Fa-f]+),(\d+),\{([^}]*)\},\{[^}]*\}\}',core_text):
    rva=int(rva,16);before=arr(before);check(len(before)==int(n),'core size');check(read(rva,len(before))==before,'core before '+hex(rva))
    ranges.append((rva,len(before)))
for rva,n,stub,before in re.findall(r'\{(0x[0-9A-Fa-f]+),(\d+),(\d+),\{([^}]*)\}\}',pay[:pay.index('traits9_core[]')]):
    rva=int(rva,16);before=arr(before);check(read(rva,len(before))==before,'UI '+hex(rva))
    check(5<=len(before)<=32,'complete jump site');ranges.append((rva,len(before)))
check(len(ranges)==13,'5 core patches + 8 UI sites (got %d)'%len(ranges))
for path in (P/'src').glob('*'):
    # 사용자 2026-11-05 Layout 호환 설계 §8: layout_registry.h is the Layout manager's data-only registry of these very patch
    # sites (owner traits9, yield-only); tests/verify_layout_registry.py checks it against this same payload table.
    if path.suffix not in ('.cpp','.h','.inl') or path.name in ('native_traits9.inl','traits9_payloads.h','modpack_tables.h','layout_registry.h','game_addresses_data.inl'):continue
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
SAVE_CONTRACT_PIN={'src/native_merit_save.inl': '090f84424b7a65bbbb973386eb2cf584919454394c9bd594bcfd7562a0baad59', 'src/native_awards_save.inl': '602ace40e42e849bdd304723fd7cd1be65ed5ee55b18878f47f6268697fa684d', 'src/sidecar_cache.h': 'eacc735d3f39fe57698485cc28c3080b283399dd6d6af5e9eb6759b597ede1ec', 'src/merit_bond.h': '77c5320ebdca8da8d1d53e1e98e1485d3eb8afd9bb72d15ddabb66c2d00a1585', 'src/save_binding.h': 'bcb431dc6c4996f8402c8a9e8a1c00cf9776c13001d791386487e451ae4ca1b4'}
for name,expected in SAVE_CONTRACT_PIN.items():
    check(hashlib.sha256((P/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()==expected,
          'save source matches reviewed contract '+name)
new=(P/'src/native_traits9.inl').read_text(encoding='utf-8')
check(not any(s in new for s in ('RemoteWrite','RemoteRead','SaveHost','WriteFile','CreateFile')),'no new save hook or save writer')
original=G/'0002/ScreenLayout.rdb'
if hashlib.sha256(original.read_bytes()).hexdigest()!=ORIGINAL_INDEX_SHA256:
    state=json.loads((P/'backup/input_state.json').read_text(encoding='utf-8-sig'))
    if 'layout' in state:
        # v11.31+: four index entries point at our free bank; undo them in memory.
        sys.path.insert(0,str(P/'tools'));import traits9_layout as tl
        patch=json.loads((P/'research/traits9/layout_patch/traits9_layout.json').read_text(encoding='utf-8-sig'))
        data,reverted,_=tl.revert(original.read_bytes(),patch['records'],state['layout']['bank'])
        check(len(reverted)==4,'installed layout entries revert in memory')
        original=P/'build/traits9_original_index.rdb';original.write_bytes(data)
    else:
        original=Path(state['asset_restore']['0002/ScreenLayout.rdb']['backup']).resolve()
        check(original.is_relative_to((P/'backup').resolve()),'original index backup remains in workspace')
    check(hashlib.sha256(original.read_bytes()).hexdigest()==ORIGINAL_INDEX_SHA256,'original index backup hash')
# User 2026-11-07: origlib is incomplete and is not part of this plan.
# Keep the existing traits9 index parser and the reviewed chunk decoder below;
# the old generator/borrowed module is no longer executed by the test.
def index_bytes(path):
    _header,entries=tl.parse(path.read_bytes())
    return {tl.ktid(e):e for e in entries}
def read_layout(entry,folder):
    off,size,bank=tl.locator(entry)
    check(bank==31,'new layout bank')
    with (folder/('ScreenLayout.rdb.bin%d'%bank)).open('rb') as stream:
        stream.seek(off);raw=stream.read(size)
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
old=index_bytes(original)
asset=P/'research/traits9/assets/0002'
candidate=index_bytes(asset/'ScreenLayout.rdb')
check(set(old)==set(candidate),'all other layout entries preserved')
check({k for k in old if old[k]!=candidate[k]}==set(LAYOUT_NODES),'only four UI layouts change')
for ktid,(before_nodes,after_nodes) in LAYOUT_NODES.items():
    body=read_layout(candidate[ktid],asset)
    section=0x4C
    while struct.unpack_from('<I',body,section)[0]!=1:section+=struct.unpack_from('<I',body,section+4)[0]
    check(after_nodes==before_nodes+4 and struct.unpack_from('<H',body,section+0x12)[0]==after_nodes,'appended four nodes')
result=dict(status='traits9_save_fixture_passed',checks=checks,save_record_layout_unchanged=True,
    existing_nine_slot_person_load=True,save_sidecar_sources_unchanged=True,only_four_ui_layouts_changed=True,
    gameplay_verified=False,live_save_reload_verified=False)
(P/'build/traits9_save_fixture.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result))
