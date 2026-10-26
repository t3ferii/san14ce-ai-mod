"""Focused native-site, layout and unchanged save-contract audit."""
from pathlib import Path
import collections,hashlib,json,os,re,struct,sys,zlib
P=Path(__file__).resolve().parents[1];G=P.parent
report=json.loads((P/'build/traits9_preparation.json').read_text())
baseline=json.loads((P/'build/input_package/manifest.json').read_text(encoding='utf-8'))
code=(P/'legacy/FogMod_Source/native_game_analysis/running_code_text.bin').read_bytes();checks=0
def check(v,name):
    global checks
    assert v,name
    checks+=1
def read(r,n):return code[r-4096:r-4096+n]
ranges=[]
for p in report['patches']:
    before=bytes.fromhex(p['before']);guard=bytes.fromhex(p['guard'])
    check(read(p['rva'],len(before))==before,'core before '+hex(p['rva']))
    check(read(p['guard_rva'],len(guard))==guard,'core surrounding '+hex(p['rva']))
    ranges.append((p['rva'],len(before)))
for m in report['manifests']:
    for p in m['sites']:
        before=bytes.fromhex(p['before']);check(read(p['rva'],len(before))==before,'UI '+hex(p['rva']))
        check(5<=len(before)<=32,'complete jump site');ranges.append((p['rva'],len(before)))
p=report['capture'];check(read(p['site'],5)==bytes.fromhex(p['before']),'capture call original')
check(p['site']+5+struct.unpack_from('<i',read(p['site'],5),1)[0]==p['original'],'capture native player predicate')
ranges.append((p['site'],5))
for path in (P/'src').glob('*'):
    if path.suffix not in ('.cpp','.h','.inl') or path.name in ('native_traits9.inl','modpack_tables.h'):continue
    values={int(x,16) for x in re.findall(r'0[xX][0-9a-fA-F]+',path.read_text(encoding='utf-8'))}
    hits=[hex(v) for a,n in ranges for v in values if a<=v<a+n]
    check(not hits,'existing hook candidate '+path.name+str(hits))
# Original native person load already copies all nine u16 trait IDs at +118.
check(read(0x31FF44,3)==bytes.fromhex('83ff09'),'native person copy count remains nine')
check(0x118+9*2==0x12A and 0x12A<0x12C,'nine traits fit existing record before next field')
save_sources=('src/native_merit_save.inl','src/native_awards_save.inl','src/sidecar_cache.h','src/merit_bond.h')
# v11.31 reviewed change: the merit feature switch only gates re-adding saved
# merit friends to native slots after load. Any other edit still fails here.
reviewed={'src/native_merit_save.inl':[(
    "        // Off: keep saved records unchanged for a later re-enable, add no friends.\r\n        if(mod_features.merit)merit_bonds.each(",
    "        merit_bonds.each(")]}
for name in save_sources:
    data=(P/name).read_bytes();expected=baseline['split_modules']['source_sha256'][name]
    if hashlib.sha256(data).hexdigest()==expected:check(True,'save binding unchanged '+name);continue
    for new_text,old_text in reviewed.get(name,()):
        text=data.decode('utf-8');check(text.count(new_text)==1,'reviewed save edit present once '+name)
        data=text.replace(new_text,old_text).encode('utf-8')
    check(hashlib.sha256(data).hexdigest()==expected,'save binding unchanged '+name)
new=(P/'src/native_traits9.inl').read_text(encoding='utf-8')
check(not any(s in new for s in ('RemoteWrite','RemoteRead','SaveHost','WriteFile','CreateFile')),'no new save hook or save writer')
ns=dict(struct=struct,re=re,os=os,zlib=zlib,CHUNK=16384)
owned=P/'research/traits9/senpotex_borrowed.py';exec(compile(owned.read_text(encoding='utf-8'),str(owned),'exec'),ns)
original=G/'0002/ScreenLayout.rdb'
if hashlib.sha256(original.read_bytes()).hexdigest()!=report['asset_originals']['0002/ScreenLayout.rdb']:
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
    check(hashlib.sha256(original.read_bytes()).hexdigest()==report['asset_originals']['0002/ScreenLayout.rdb'],'original index backup hash')
old=ns['Index'](str(original))
asset=P/'research/traits9/assets/0002';candidate=ns['Index'](str(asset/'ScreenLayout.rdb'))
changed={k for k in old.by_ktid if old.entry(k)!=candidate.entry(k)}
check(changed=={int(x['ktid'],16) for x in report['layout_changes']},'only four UI layouts change')
check(set(old.by_ktid)==set(candidate.by_ktid),'all other layout entries preserved')
for x in report['layout_changes']:
    raw=candidate.entry(int(x['ktid'],16));off,size,bank=ns['Index'].locator(raw)
    header,body=ns['read_entry'](str(asset),off,size,bank)
    check(bank==31,'new layout bank')
    section=0x4C
    while struct.unpack_from('<I',body,section)[0]!=1:section+=struct.unpack_from('<I',body,section+4)[0]
    check(struct.unpack_from('<H',body,section+0x12)[0]==x['new_nodes'],'appended four nodes')
result=dict(status='traits9_save_fixture_passed',checks=checks,save_record_layout_unchanged=True,
    existing_nine_slot_person_load=True,save_sidecar_sources_unchanged=True,only_four_ui_layouts_changed=True,
    gameplay_verified=False,live_save_reload_verified=False)
(P/'build/traits9_save_fixture.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result))
