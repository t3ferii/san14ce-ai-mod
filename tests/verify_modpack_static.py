import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Compare imported hook ownership and new stat gates with this game build."""
from pathlib import Path
import hashlib,json,re,struct,sys
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'legacy/FogMod_Source/tools'))
from audit_native_game import PEImage
checks=0
# the ModPack v1.6 payload DLLs (research/modpack_v16) carry RVAs of the former reference build 1fbe6be7: m() moves them to 1a4a447d through the
# id map of the re-base (development/addr_promote/1fbe6be7_to_1a4a447d/idmap.tsv, ids are named by their RVA)
IDMAP={int(a[3:],16):int(b[3:],16) for a,b in (l.split('\t') for l in (P/'development/addr_promote/1fbe6be7_to_1a4a447d/idmap.tsv').read_text(encoding='utf-8').split(chr(10))[1:] if l.strip()) if b.startswith('GA_') and not b.startswith('GA_X')}
_KEYS=sorted(IDMAP)
def m(x):   # an id, or an id plus a small offset inside the same function
    import bisect
    if x==0:return 0
    k=_KEYS[bisect.bisect_right(_KEYS,x)-1]
    assert 0<=x-k<=0x40,'blob RVA %X is neither a table id nor close behind one'%x
    return IDMAP[k]+(x-k)
def check(v,label):
    global checks
    assert v,label
    checks+=1
# F-03 (2026-11-07, docs/MODULARIZATION_PLAN.md section 6): every stats200 write goes through the Layout manager as owner "stats200" (one all-or-none
# transaction); the unit writes no image bytes itself and has no revert of its own (the manager's unhook only puts back bytes that are still ours).
src=(P/'src/native_modpack.inl').read_text(encoding='utf-8')
for gone in ('modpack_write','modpack_revert','prepare_link(','publish_link(','restore_link(','VirtualProtect(image'):
    check(gone not in src,'native_modpack.inl no longer contains '+gone)
check('layout_client::Transaction transaction;' in src and src.count('layout_client::bytes("stats200"')==2 and src.count('layout_client::span("stats200"')==2,
      'stats200 rows (code, data) and the two relay spans are registered in one transaction')
check('LF_Y' in src and 'modpack_copy_before' in src,'rows carry the runtime image bytes as `before`, flags LF_Y')
check('modpack_clamp_long(' not in src,'clamp long form (previous build) removed at the 1a4a447d re-base')
# S3 2026-11-06: sources spell reference-build (1fbe6be7) RVAs after ga_expand, so compare against the reference unpacked EXE (as verify_layout_registry.py does), not the Steam-patched SAN14CE_KO.exe.
game=PEImage(P/'build/ui_native_analysis/editor83_b25953386/game.exe.unpacked.exe')
code=(P/'build/ui_native_analysis/fixture/1a4a447d_text.bin').read_bytes()
hover=PEImage(P/'research/modpack_v16/extracted/payload/SAN14_TraitHover.dll')
stats=PEImage(P/'research/modpack_v16/extracted/payload/SAN14_Stats200.dll')
def read(rva,n):return code[rva-0x1000:rva-0x1000+n] if rva<0x1000+len(code) else game.read(rva,n)
for i in range(14):
    b=stats.read(0x2000+i*112,112);rva,n,guard,gn=struct.unpack_from('<IIII',b)
    if m(rva)==0x231AFB:continue   # the long-form clamp row of the previous build was removed (1-F)
    check(read(m(rva),n)==b[16:16+n],hex(rva))
check(read(0x2bc14a,6)==bytes.fromhex('4183fd647507'),'growth unsigned-200 gate')
check(read(0x2bc30a,9)==bytes.fromhex('418d5d01e82db90700'),'growth bridge original')
blob=[(m(a),m(b),c) for a,b,c in (struct.unpack('<III',hover.read(0x62A0+i*12,12)) for i in range(152))]
new={s for s,_,_ in blob};hover152=set(new)
new.update([0x13aa260,0x13a9e28,0x13d5f70,0x13d5f90,0x144a278])
# docs/HISTORY.md 4-3 (S3, 2026-11-06): the registry's "trait_hover" rows must equal the blob tables (same slots, same originals, same order),
# and no other owner may use those slots/sites. The overlap scan below skips layout_registry.h because it is the owner of these rows.
reg=(P/'src/layout_registry.h').read_text(encoding='utf-8')
reg_body=reg[reg.index('static const LayoutRow layout_rows[]'):reg.index('static const unsigned layout_row_count')]
reg_rows=[(m.group(1),m.group(2),int(m.group(3),16),int(m.group(4),16)) for m in
    re.finditer(r'\{"(\w+)",(LR_\w+),\s*0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+),\s*[\w|]+\}',reg_body)]
th_vt=[(r,o) for ow,k,r,o in reg_rows if ow=='trait_hover' and k=='LR_VT']
th_call=[(r,o) for ow,k,r,o in reg_rows if ow=='trait_hover' and k=='LR_CALL']
check(len(th_vt)==152 and len(th_call)==4,'trait_hover registry rows: 152 VT + 4 CALL (got %d/%d)'%(len(th_vt),len(th_call)))
check(th_vt==[(s,o) for s,o,_ in blob],'trait_hover VT rows equal blob table 0x62A0 (slot, original, order)')
blob_sites=set(m(struct.unpack('<II',hover.read(0x8040+i*8,8))[j]) for i in range(2) for j in range(2))
check({r for r,_ in th_call}==blob_sites and len(blob_sites)==4,'trait_hover CALL rows equal blob 0x8040 sites')
check(all(o==0x9332B0 for _,o in th_call),'trait_hover CALL rows target 930D20')
th_all={r for r,_ in th_vt}|{r for r,_ in th_call}
check(not any(ow!='trait_hover' and r in th_all for ow,_,r,_ in reg_rows),'no other owner uses a trait_hover slot or call site')
check(len(th_all)==156,'trait_hover rows are unique')
# 2026-11-06 S6: modpack_tables.h no longer has hover_slots (the hover blob is not linked); the blob table in the research DLL above stays the evidence for the registry rows.
# pointer-overlap scan retired 2026-11-07 (F-03): hook_map.py R1/R2 judge overlap of slots and sites between owners, and stats200/traits9 are manager registrations.
for slot,original in [(0x13aa260,0x2404f0),(0x13a9e28,0x241530),(0x13d5f70,0x423cd0),(0x13d5f90,0x423D00),(0x144a278,0x724e40)]:
    check(struct.unpack('<Q',game.read(slot,8))[0]==game.image_base+original,'vtable '+hex(slot))
for site,target in [(0x25B022,0x36A850),(0x27f716,0x36A850),(0x25b0eb,0x2979b0),(0x27f7a9,0x2979b0)]:
    b=read(site,5);check(b[0]==0xe8 and site+5+struct.unpack_from('<i',b,1)[0]==target,'death call '+hex(site))
check(read(0x297b6f,8)==bytes.fromhex('85db0f84ce080000'),'death history gate')
check(read(0x424CE4,11)==bytes.fromhex('8b8b74050000413bc80f83'),'scenery state 3 unsigned frame gate')
result=dict(status='modpack_static_fixture_passed',checks=checks,
    game_sha256=hashlib.sha256(game.data).hexdigest(),gameplay_verified=False)
(P/'build/modpack_static_fixture.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result))
