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
        # local5: on the 1a4a447d install SplitManage.ps1 Get-TraitLayoutPlan does not apply the four traits9 records (the 9-slot code is off on
        # that build), so the installed index already holds the original entries and nothing is reverted; any other count is an error.
        applied=set(state['layout'].get('applied',[]))
        expected=4 if any(r['ktid'] in applied for r in patch['records']) else 0
        assert len(reverted)==expected,(reverted,expected)
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
# local10 (2026-11-06, docs/LOCAL10_PORT_PLAN.md 4.1): the manifests come from the reference build (1fbe6be7).  Every original RVA of
# that build is written as a table id (GA(GA_<oldRVA>), resolved per running build, the same convention as the rest of the mod); a
# target inside a function is written GA(GA_<site>)+offset.  Anything else must be a plain offset and is refused here.
SITE_IDS=[0x8DF198,0x8DF1F5,0x907294,0x8CDD40,0x8E0148,0x8E01D0,0x907D9C,0x2BF033]
CORE_IDS=[0x4A6CF3,0x4A7313,0x22558F,0x23064A,0x232D97]
FUNC_IDS=[0x353810,0x2305D0,0x32F500]
DATA_IDS=[0x1A8A078,0x1A8A140,0x1A30830,0x1A47B98,0x1A2B850,0x277DF18]
INNER={0x8DF19F:(0x8DF198,7),0x8DF1A0:(0x8DF198,8),0x8DF1FB:(0x8DF1F5,6),0x9072A5:(0x907294,0x11),
       0x8E0152:(0x8E0148,0xA),0x8E0160:(0x8E0148,0x18),0x8E01D6:(0x8E01D0,6),0x907DAC:(0x907D9C,0x10),0x2BF042:(0x2BF033,0xF)}
KNOWN=set(SITE_IDS+CORE_IDS+FUNC_IDS+DATA_IDS)
def ga(v):
    assert v in KNOWN,'original RVA 0x%X is not a registered table id (tools/gen_signatures.py EXTRA_IDS)'%v
    return 'GA(GA_%X)'%v
def ga_expr(v):
    if v in KNOWN:return ga(v)
    assert v in INNER,'module-relative addend 0x%X is neither a table id nor a registered in-function offset'%v
    site,off=INNER[v];return 'GA(GA_%X)+0x%X'%(site,off)
for m in manifests:
    for s in m.sites:assert s.rva in KNOWN,hex(s.rva)
for p_ in patches:assert p_['rva'] in KNOWN,hex(p_['rva'])
assert {s.rva for m in manifests for s in m.sites}=={*SITE_IDS},'site list drifted'
assert {p_['rva'] for p_ in patches}=={*CORE_IDS},'core list drifted'
lines=['// Borrowed current-build Editor_Source manifests (original RVAs are table ids, local10).','#include "game_addresses.h"']
for i,m in enumerate(manifests):
    lines += ['static const unsigned char t9_code%d[]=%s;'%(i,array(m.code)),
              'static const unsigned char t9_data%d[]=%s;'%(i,array(m.data))]
    lines.append('static const Trait9Site t9_sites%d[]={%s};'%(i,','.join('{%s,%d,%d,%s}'%(ga(s.rva),len(s.original),s.stub,array(s.original)) for s in m.sites)))
    target={'module':0,'code':1,'data':2}
    lines.append('static const Trait9Reloc t9_relocs%d[]={%s};'%(i,','.join('{%d,%d,%d,%d,%s}'%(r.region=='data',r.offset,r.kind=='abs64',target[r.target],ga_expr(r.addend) if r.target=='module' else str(r.addend)) for r in m.relocs)))
lines.append('static const Trait9Manifest trait9_manifests[]={'+','.join('{t9_code%d,sizeof(t9_code%d),t9_data%d,sizeof(t9_data%d),%d,%d,t9_sites%d,sizeof(t9_sites%d)/sizeof(Trait9Site),t9_relocs%d,sizeof(t9_relocs%d)/sizeof(Trait9Reloc)}'%(i,i,i,i,m.code_size,m.data_size,i,i,i,i) for i,m in enumerate(manifests))+'};')
lines.append('static const ModpackBytes traits9_core[]={'+','.join('{%s,%d,%s,%s}'%(ga(p['rva']),len(bytes.fromhex(p['before'])),array(bytes.fromhex(p['before'])),array(bytes.fromhex(p['after']))) for p in patches)+'};')
(out/'traits9_payloads.h').write_text(chr(10).join(lines)+chr(10),encoding='utf-8')
# The captive_dialog feature (either-force capture scene) was removed in local10 (user 2026-11-06): no CaptiveDialog audience
# predicate or capture-site search any more.
report=dict(status='traits9_sources_prepared',patches=patches,layout_changes=changes,
    asset_originals={'0002/ScreenLayout.rdb':hashlib.sha256(original.read_bytes()).hexdigest()},
    assets={p.relative_to(assets).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in assets.rglob('*') if p.is_file()},
    source_sha256={n:hashlib.sha256((S/n).read_bytes()).hexdigest() for n in ('codepatch.py','traits9ui.py','traits9abil.py','traits9list.py','traitlayout.py','senpotex.py')},
    manifests=[dict(id=m.id,sites=[dict(rva=s.rva,before=s.original.hex(),stub=s.stub) for s in m.sites]) for m in manifests])
(P/'build/traits9_preparation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('Prepared 9-trait logic,',sum(len(m.sites) for m in manifests),'UI hooks and four layouts; originals unchanged')
