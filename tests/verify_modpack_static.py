import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Compare imported hook ownership and new stat gates with this game build."""
from pathlib import Path
import hashlib,json,re,struct,sys
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'legacy/FogMod_Source/tools'))
from audit_native_game import PEImage
checks=0
def check(v,label):
    global checks
    assert v,label
    checks+=1
# local10 G1-a/b: undo of our own writes must be plain-memcmp (modpack_revert); modpack_write (mcmp_at) ignores `expected` on a known build
src=(P/'src/native_modpack.inl').read_text(encoding='utf-8')
def body(name):
    i=src.index('static void '+name+'(');j=src.index(chr(10)+'}'+chr(10),i);return src[i:j]
for fn in ('modpack_restore_stats',):   # restore_deaths removed 2026-11-06 (docs/BATTLE_DEATHS_PLAN.md S4): battle_deaths is Layout-registered own source, no byte writes
    check('modpack_write(' not in body(fn),fn+' must not use modpack_write')
    check('modpack_revert(' in body(fn),fn+' must use modpack_revert')
check('static bool modpack_revert(' in src and 'memcmp(image+rva,after,size)' in src,'modpack_revert is plain memcmp')
check('modpack_clamp_long(' in src,'clamp long form judged by plain memcmp')
# S3 2026-11-06: sources spell reference-build (1fbe6be7) RVAs after ga_expand, so compare against the reference unpacked EXE (as verify_layout_registry.py does), not the Steam-patched SAN14CE_KO.exe.
game=PEImage(P/'build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe')
code=(P/'legacy/FogMod_Source/native_game_analysis/running_code_text.bin').read_bytes()
hover=PEImage(P/'research/modpack_v16/extracted/payload/SAN14_TraitHover.dll')
stats=PEImage(P/'research/modpack_v16/extracted/payload/SAN14_Stats200.dll')
def read(rva,n):return code[rva-0x1000:rva-0x1000+n] if rva<0x1000+len(code) else game.read(rva,n)
for i in range(14):
    b=stats.read(0x2000+i*112,112);rva,n,guard,gn=struct.unpack_from('<IIII',b)
    check(read(rva,n)==b[16:16+n],hex(rva))
    check(read(guard,gn)==b[48:48+gn],'surrounding '+hex(rva))
check(read(0x2b9dca,6)==bytes.fromhex('4183fd647507'),'growth unsigned-200 gate')
check(read(0x2b9f8a,9)==bytes.fromhex('418d5d01e84dba0700'),'growth bridge original')
blob=[struct.unpack('<III',hover.read(0x62A0+i*12,12)) for i in range(152)]
new={s for s,_,_ in blob};hover152=set(new)
new.update([0x13a7220,0x13a6de8,0x13d2ee0,0x13d2f00,0x14471f0])
# TRAIT_HOVER_PLAN 4-3 (S3, 2026-11-06): the registry's "trait_hover" rows must equal the blob tables (same slots, same originals, same order),
# and no other owner may use those slots/sites. The overlap scan below skips layout_registry.h because it is the owner of these rows.
reg=(P/'src/layout_registry.h').read_text(encoding='utf-8')
reg_body=reg[reg.index('static const LayoutRow layout_rows[]'):reg.index('static const unsigned layout_row_count')]
reg_rows=[(m.group(1),m.group(2),int(m.group(3),16),int(m.group(4),16)) for m in
    re.finditer(r'\{"(\w+)",(LR_\w+),\s*0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+),\s*[\w|]+\}',reg_body)]
th_vt=[(r,o) for ow,k,r,o in reg_rows if ow=='trait_hover' and k=='LR_VT']
th_call=[(r,o) for ow,k,r,o in reg_rows if ow=='trait_hover' and k=='LR_CALL']
check(len(th_vt)==152 and len(th_call)==4,'trait_hover registry rows: 152 VT + 4 CALL (got %d/%d)'%(len(th_vt),len(th_call)))
check(th_vt==[(s,o) for s,o,_ in blob],'trait_hover VT rows equal blob table 0x62A0 (slot, original, order)')
blob_sites=set(struct.unpack('<II',hover.read(0x8040+i*8,8))[j] for i in range(2) for j in range(2))
check({r for r,_ in th_call}==blob_sites and len(blob_sites)==4,'trait_hover CALL rows equal blob 0x8040 sites')
check(all(o==0x930D20 for _,o in th_call),'trait_hover CALL rows target 930D20')
th_all={r for r,_ in th_vt}|{r for r,_ in th_call}
check(not any(ow!='trait_hover' and r in th_all for ow,_,r,_ in reg_rows),'no other owner uses a trait_hover slot or call site')
check(len(th_all)==156,'trait_hover rows are unique')
# 2026-11-06 S6: modpack_tables.h no longer has hover_slots (the hover blob is not linked); the blob table in the research DLL above stays the evidence for the registry rows.
for path in (P/'src').glob('*'):
    if path.suffix not in ('.cpp','.inl','.h') or path.name in ('modpack_tables.h','native_modpack.inl','layout_registry.h','game_addresses_data.inl','native_trait_hover.inl'):continue  # data tables: registry owns the rows, the address table lists every id; native_trait_hover.inl (S4 2026-11-06) names four class vtable slots only to derive vtable rvas for path classification, it registers hooks through the registry rows and never writes a pointer
    constants={int(x,16) for x in re.findall(r'0[xX][0-9a-fA-F]+',path.read_text(encoding='utf-8'))}
    # native_split_install.inl names the collab/scenery peer slots only as yield_to_peer arguments (no pointer write): the five extra vtables are allowed there, the 152 hover slots never
    scan=new-({0x13a7220,0x13a6de8,0x13d2ee0,0x13d2f00,0x14471f0}-hover152 if path.name=='native_split_install.inl' else set())
    check(not (scan&constants),'possible pointer overlap '+path.name+str(scan&constants))
for slot,original in [(0x13a7220,0x23eb50),(0x13a6de8,0x23fb90),(0x13d2ee0,0x421960),(0x13d2f00,0x421990),(0x14471f0,0x7229c0)]:
    check(struct.unpack('<Q',game.read(slot,8))[0]==game.image_base+original,'vtable '+hex(slot))
for site,target in [(0x259682,0x368360),(0x27d976,0x368360),(0x25974b,0x295aa0),(0x27da09,0x295aa0)]:
    b=read(site,5);check(b[0]==0xe8 and site+5+struct.unpack_from('<i',b,1)[0]==target,'death call '+hex(site))
check(read(0x295c5f,8)==bytes.fromhex('85db0f84ce080000'),'death history gate')
check(read(0x422974,11)==bytes.fromhex('8b8b74050000413bc80f83'),'scenery state 3 unsigned frame gate')
result=dict(status='modpack_static_fixture_passed',checks=checks,pointer_overlap=False,
    game_sha256=hashlib.sha256(game.data).hexdigest(),gameplay_verified=False)
(P/'build/modpack_static_fixture.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps(result))
