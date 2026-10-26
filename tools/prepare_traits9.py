"""Reuse current-build editor manifests and four-node layout expansion.

Only pure generation/index/layout definitions are copied. No live-memory,
process-control, installer, apply or revert function is invoked.
"""
from pathlib import Path
import ast,collections,hashlib,json,os,re,struct,sys,types,zlib
P=Path(__file__).resolve().parents[1];G=P.parent
S=G/'reference_mod/Editor_Source/san14ce'
OUT=P/'research/traits9';OUT.mkdir(exist_ok=True)
class Manifest:
    def __init__(self,id,code,data=b'',sites=(),relocs=(),code_size=None,data_size=None):
        self.id=id;self.code=bytes(code);self.data=bytes(data);self.sites=sites;self.relocs=relocs
        self.code_size=((code_size or len(code))+4095)&~4095
        self.data_size=((data_size or max(len(data),1))+4095)&~4095
CC=types.SimpleNamespace(Manifest=Manifest,Site=collections.namedtuple('Site','name rva original stub'),
                        Reloc=collections.namedtuple('Reloc','region offset kind target addend'))
def borrow(name,only=None,extra=None):
    text=(S/(name+'.py')).read_text(encoding='utf-8-sig');selected=[]
    for n in ast.parse(text).body:
        if isinstance(n,(ast.Assign,ast.AnnAssign)) and only is None:selected.append(n)
        elif isinstance(n,(ast.FunctionDef,ast.ClassDef)) and (only is None or n.name in only):
            if n.name not in ('apply','revert','install','uninstall','state','status'):selected.append(n)
    owned=OUT/(name+'_borrowed.py');owned.write_text(ast.unparse(ast.Module(body=selected,type_ignores=[]))+'\n',encoding='utf-8')
    ns=dict(struct=struct,collections=collections,os=os,re=re,zlib=zlib,CC=CC,CHUNK=0x4000)
    ns.update(extra or {});exec(compile(owned.read_text(encoding='utf-8'),str(owned),'exec'),ns)
    return types.SimpleNamespace(**ns)
def array(b):return '{'+','.join('0x%02X'%v for v in b)+'}'
manifests=[borrow('traits9ui').manifest(),borrow('traits9abil').manifest(),borrow('traits9list').file_manifest()]
text=(S/'codepatch.py').read_text(encoding='utf-8-sig')
section=text.split('"traits9": (',1)[1].split('\n    ),',1)[0]
patches=[]
for r,b,a,gr,gb in re.findall(r'_p\((0x[\dA-Fa-f]+), "([\da-f]+)", "([\da-f]+)",\s*(0x[\dA-Fa-f]+), "([\da-f]+)"\)',section):
    patches.append(dict(rva=int(r,16),before=b,after=a,guard_rva=int(gr,16),guard=gb))
assert len(patches)==5
index=borrow('senpotex',{'TexError','Index','bank_path','read_entry','build_entry'})
layout=borrow('traitlayout',extra={'S':index})
folder=G/'0002';original=folder/'ScreenLayout.rdb'
if (folder/'ScreenLayout.rdb.bin31').exists():
    state=json.loads((P/'backup/input_state.json').read_text(encoding='utf-8-sig'))
    for name in ('0002/ScreenLayout.rdb','0002/ScreenLayout.rdb.bin31'):
        assert hashlib.sha256((G/name).read_bytes()).hexdigest()==state['modules'][name]
    saved=state['asset_restore']['0002/ScreenLayout.rdb'];original=Path(saved['backup']).resolve()
    assert original.is_relative_to((P/'backup').resolve()) and hashlib.sha256(original.read_bytes()).hexdigest()==saved['sha256']
else:
    state=json.loads((P/'backup/input_state.json').read_text(encoding='utf-8-sig')) if (P/'backup/input_state.json').exists() else {}
    if 'layout' in state:
        # v11.31+ merged layout: undo our four entries in memory, never touch the game file.
        sys.path.insert(0,str(P/'tools'));import traits9_layout as tl
        patch=json.loads((P/'research/traits9/layout_patch/traits9_layout.json').read_text(encoding='utf-8-sig'))
        data,reverted,_=tl.revert(original.read_bytes(),patch['records'],state['layout']['bank'])
        assert len(reverted)==4,reverted
        original=P/'build/traits9_original_index.rdb';original.write_bytes(data)
idx=index.Index(str(original));bank=bytearray()
assets=OUT/'assets';(assets/'0002').mkdir(parents=True,exist_ok=True)
changes=[]
for spec in layout.SPECS:
    for k in spec.ktids:
        off,size,b=index.Index.locator(idx.entry(k));header,body=index.read_entry(str(folder),off,size,b)
        new=layout.expand(body,spec);entry=index.build_entry(header,new)
        idx.relocate(k,len(bank),len(entry),31,len(new));bank+=entry
        changes.append(dict(ktid=hex(k),old_nodes=spec.count,new_nodes=spec.count+4))
(assets/'0002/ScreenLayout.rdb').write_bytes(idx.build())
(assets/'0002/ScreenLayout.rdb.bin31').write_bytes(bank)
out=Path(sys.argv[1]) if len(sys.argv)>1 else P/'build/development/loader_split';out.mkdir(parents=True,exist_ok=True)
lines=['// Borrowed current-build Editor_Source manifests.']
for i,m in enumerate(manifests):
    lines += ['static const unsigned char t9_code%d[]=%s;'%(i,array(m.code)),
              'static const unsigned char t9_data%d[]=%s;'%(i,array(m.data))]
    lines.append('static const Trait9Site t9_sites%d[]={%s};'%(i,','.join('{0x%X,%d,%d,%s}'%(s.rva,len(s.original),s.stub,array(s.original)) for s in m.sites)))
    target={'module':0,'code':1,'data':2}
    lines.append('static const Trait9Reloc t9_relocs%d[]={%s};'%(i,','.join('{%d,%d,%d,%d,%d}'%(r.region=='data',r.offset,r.kind=='abs64',target[r.target],r.addend) for r in m.relocs)))
lines.append('static const Trait9Manifest trait9_manifests[]={'+','.join('{t9_code%d,sizeof(t9_code%d),t9_data%d,sizeof(t9_data%d),%d,%d,t9_sites%d,sizeof(t9_sites%d)/sizeof(Trait9Site),t9_relocs%d,sizeof(t9_relocs%d)/sizeof(Trait9Reloc)}'%(i,i,i,i,m.code_size,m.data_size,i,i,i,i) for i,m in enumerate(manifests))+'};')
lines.append('static const ModpackBytes traits9_core[]={'+','.join('{0x%X,%d,%s,%s}'%(p['rva'],len(bytes.fromhex(p['before'])),array(bytes.fromhex(p['before'])),array(bytes.fromhex(p['after']))) for p in patches)+'};')
(out/'traits9_payloads.h').write_text('\n'.join(lines)+'\n',encoding='utf-8')
# CaptiveDialog's original audience predicate and R15 capturing-force contract.
sys.path.insert(0,str(P/'legacy/FogMod_Source/tools'))
# Public snapshot: install capstone in the active Python environment.
from capstone import Cs,CS_ARCH_X86,CS_MODE_64,CS_GRP_CALL,CS_GRP_JUMP
from audit_native_game import PEImage
pe=PEImage(G/'SAN14CE_KO.exe');plain=(P/'legacy/FogMod_Source/native_game_analysis/running_code_text.bin').read_bytes()
b=bytes.fromhex('4439a424d80000000f84d8000000498bcee89e28f5ff85c00f84c8000000e8616b11');mask=bytearray([1]*len(b))
md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
for ins in md.disasm(b,0):
    if (ins.group(CS_GRP_CALL) or ins.group(CS_GRP_JUMP)) and ins.imm_size:mask[ins.address+ins.imm_offset:ins.address+ins.imm_offset+ins.imm_size]=bytes(ins.imm_size)
mask[-4:]=bytes(4)
hits=[]
for at in range(0x2E041C-0x4000,0x2E041C+0x4000):
    if all(not mask[i] or b[i]==plain[at-4096+i] for i in range(len(b))):hits.append(at)
assert len(hits)==1,hits
capture=hits[0]+17;before=plain[capture-4096:capture-4096+5]
target=capture+5+struct.unpack_from('<i',before,1)[0]
assert target==0x2337B0,(hex(capture),hex(target))
with (out/'traits9_payloads.h').open('a',encoding='utf-8') as f:
    f.write('static const unsigned captive_site=0x%X,captive_original=0x%X;\nstatic const unsigned char captive_before[]=%s;\n'%(capture,target,array(before)))
report=dict(status='traits9_sources_prepared',patches=patches,layout_changes=changes,
    asset_originals={'0002/ScreenLayout.rdb':hashlib.sha256(original.read_bytes()).hexdigest()},
    assets={p.relative_to(assets).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in assets.rglob('*') if p.is_file()},
    source_sha256={n:hashlib.sha256((S/n).read_bytes()).hexdigest() for n in ('codepatch.py','traits9ui.py','traits9abil.py','traits9list.py','traitlayout.py','senpotex.py')},
    manifests=[dict(id=m.id,sites=[dict(rva=s.rva,before=s.original.hex(),stub=s.stub) for s in m.sites]) for m in manifests])
report['capture']={'site':capture,'original':target,'before':before.hex(),'scope':'prisoner force or capturing force is player'}
(P/'build/traits9_preparation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('Prepared 9-trait logic,',sum(len(m.sites) for m in manifests),'UI hooks and four layouts; originals unchanged')
