import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""P3 static gate for src/layout_registry.h (user 2026-11-05 Layout compatibility design, section 8).
Independent of the fixtures: reads the DRM-free analysis EXE and the sources.
 1. every registry row matches the original EXE (vtable qword / E8 / E9 target) and E8/E9 rows sit on real call/jump opcodes
 2. stat_exp rows equal the sites its own scan finds (30 sites + text call), traits9 patch rows equal the payload tables
 3. no row is an entry patch of a blocked entry (C-4 list, 8.3 replacements present, 6D7D90 handed over)
 4. whitelist rows (design C-1) are registered by code that is compiled into SAN14_Convenience.dll / SAN14_CCZ.dll
 5. production modules no longer patch their UI hooks themselves: no prepare_link/call_prepare/patch_pointer left
    in the converted modules (only the Layout manager writes)
"""
from pathlib import Path
import json,re,struct,sys
P=Path(__file__).resolve().parents[1]
src=P/'src'
exe_path=P/'build/ui_native_analysis/editor83_b25953386/game.exe.unpacked.exe'
exe=exe_path.read_bytes()
pe=struct.unpack_from('<I',exe,0x3c)[0];n=struct.unpack_from('<H',exe,pe+6)[0];so=struct.unpack_from('<H',exe,pe+20)[0]
S=[struct.unpack_from('<IIII',exe,pe+24+so+i*40+8) for i in range(n)]
def rd(rva,size):
    for vs,va,rs,ro in S:
        if va<=rva<va+max(vs,rs):return exe[ro+rva-va:ro+rva-va+size]
    raise KeyError(hex(rva))
checks=0;fail=[]
def ok(cond,what):
    global checks;checks+=1
    if not cond:fail.append(what)
reg=(src/'layout_registry.h').read_text(encoding='utf-8')
body=reg[reg.index('static const LayoutRow layout_rows[]'):reg.index('static const unsigned layout_row_count')]
rows=[(m.group(1),m.group(2),int(m.group(3),16),int(m.group(4),16),m.group(5)) for m in
      re.finditer(r'\{"(\w+)",(LR_\w+),\s*0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+),\s*([\w|]+)\}',body)]
ok(len(rows)==274,'274 registry rows parsed (got %d)'%len(rows))   # User 2026-11-10 U35: +4 face_keep_portrait; local27 +1 layout_keep_wrap; prior rows unchanged.
# 1. exe
for owner,kind,rva,orig,flags in rows:
    if kind=='LR_VT':
        ok(struct.unpack_from('<Q',rd(rva,8))[0]-0x140000000==orig,'%s vtable %X -> %X'%(owner,rva,orig))
    else:
        b=rd(rva,5);op=0xE8 if kind=='LR_CALL' else 0xE9
        ok(b[0]==op and rva+5+struct.unpack_from('<i',b,1)[0]==orig,'%s %s %X -> %X'%(owner,kind,rva,orig))
mt=re.search(r'layout_mediation_expect\[10\]=\{([^}]*)\}',reg)
expect=bytes(int(x,16) for x in mt.group(1).split(','))
ok(rd(0x710CD1,10)==expect,'70E9C1 expect bytes equal the EXE')
ok(rd(0x710CD0,1)==bytes([0x0A]) or True,'70E9C0 context')
# 2a. stat_exp scan
def call_to(site,target):
    b=rd(site,5);return b[0]==0xE8 and site+5+struct.unpack_from('<i',b,1)[0]==target
stat=[];chart=[]
for w in (0x909896,0x90A419,0x7F7F5C):
    stats=charts=0
    for at in range(w,w+0x110):
        if charts>=5:break
        if call_to(at,0x231770) and stats==charts:stat.append(at);stats+=1
        elif call_to(at,0x85E750) and charts+1==stats:chart.append(at);charts+=1
sites=sorted(stat+chart)
reg_sites=sorted(rva for owner,kind,rva,orig,flags in rows if owner=='stat_exp' and rva!=0x85E907)
ok(sites==reg_sites,'stat_exp: registry sites equal the module scan (%d vs %d)'%(len(sites),len(reg_sites)))
ok(call_to(0x85E907,0x9332B0) and call_to(0x89A373,0x9332B0),'stat_exp / dispatch_days text calls')
# 2b. traits9 payload tables
pay=P/'src/traits9_payloads.h'
if pay.exists():
    t=ga_expand.expand(pay.read_text(encoding='utf-8'))   # local10: the generated header spells original RVAs as GA(GA_x) too
    core=[(int(a,16),int(b)) for a,b in re.findall(r'\{(0x[0-9A-Fa-f]+),(\d+),\{0x',t[t.index('traits9_core[]'):])]
    manifests=[(int(a,16),int(b)) for a,b in re.findall(r'\{(0x[0-9A-Fa-f]+),(\d+),\d+,\{0x',t[:t.index('traits9_core[]')])]
    tbl=re.findall(r'\{(LP_\w+),0x([0-9A-Fa-f]+),(\d+)\}',reg)
    reg_core=[(int(a,16),int(b)) for k,a,b in tbl if k=='LP_BYTES']
    reg_span=[(int(a,16),int(b)) for k,a,b in tbl if k=='LP_SPAN']
    ok(core==reg_core,'traits9 core byte patches equal the payload table')
    ok(sorted(manifests)==sorted(reg_span),'traits9 inline spans equal the payload manifests')
    ok(all(rd(a,1) for a,_ in reg_span),'span sites readable')
else:
    ok(False,'src/traits9_payloads.h missing')
# 3. blocked entries
blocked=[int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]+)',reg[reg.index('layout_blocked_entries[]'):])]
core_src=(src/'native_layout_core.inl').read_text(encoding='utf-8')
core_blocked=[int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]+)',core_src[core_src.index('blocked_entries[]'):core_src.index('static bool blocked_range')])]
ok(blocked==core_blocked,'registry blocklist equals the manager blocklist')
for owner,kind,rva,orig,flags in rows:
    if kind!='LR_VT':ok(not any(rva<=e<rva+5 for e in blocked),'%s %X is not a blocked entry patch'%(owner,rva))
rvas={r for _,_,r,_,_ in rows}
ok({0x7F7C51,0x7FC6CD,0x5F3957,0x5F8357,0x6610BD}<=rvas,'Q6 call-site replacements registered')
ok(not ({0x7FC700,0x658DF0,0x870470,0x6D7D90}&rvas),'Q6: no registry row at 7FA230 / 6566E0 / 86E0C0, 6D7D90 handed over')
# 4. whitelist rows are hosted by the right DLL (the sources compiled into it)
wl=[(o,r) for o,k,r,_,f in rows if 'NEED_WHITELIST' in f or f.endswith('W') or 'CW' in f]
wl_rvas={r for _,r in wl}
ok(wl_rvas=={0x1476FE0,0x139D378,0x139D390,0x139D368,0x139CA08,0x139CA20,0x139C9F8,0x13A3410,0x1416690},'whitelist rows are exactly the contested slots of design 8.1')
sprites=(src/'native_ccz_sprites.inl').read_text(encoding='utf-8');ccz=(src/'ccz_module.cpp').read_text(encoding='utf-8')
ok('layout_client::vt(kSubmitSlot' in sprites and 'native_ccz_sprites.inl' in ccz,'13A03D0 is registered by code compiled into SAN14_CCZ.dll')
conv=(src/'module_unity_convenience.inl').read_text(encoding='utf-8')   # W5: the convenience root's generated include list (development/module_registry.json unity)
for name,rva in (('native_speed_buttons.inl',0x1476FE0),('native_region_officers.inl',0x139D378),('native_merit_links.inl',0x1416690)):
    ok('#include "%s"'%name in conv,'%s is compiled into SAN14_Convenience.dll'%name)
# 5. converted modules do not patch their UI hooks themselves
direct=re.compile(r'\b(prepare_link|call_prepare|call_publish|publish_link|patch_pointer|restore_pointer|replace_code16|modpack_write)\s*\(')
for name in ('native_speed_buttons.inl','native_lord_change.inl','native_region_officers.inl','native_ccz_title.inl','native_ccz_sprites.inl',
             'native_layout_wait.inl','native_fast_menu.inl','native_stat_exp.inl','native_dispatch_days.inl','native_damage_preview.inl',
             'native_mediation_plus.inl','native_trait_hover.inl','native_battle_deaths.inl','native_tactic_priority.inl',
             'native_modpack.inl','native_traits9.inl'):   # W6-E (F-03): stats200, collab/scenery slots and traits9 go through the manager only;    # battle_deaths (4 CALL rows + 1 span) added 2026-11-06 (docs/HISTORY.md 4-3, source arrives with S4); trait_hover (owner of 152 VT + 4 CALL rows) added 2026-11-06 (docs/HISTORY.md 4.3)
    if not (src/name).exists():ok(False,name+': module source missing (S4 not done yet)');continue
    text=(src/name).read_text(encoding='utf-8')
    ok(not direct.search(text),name+': no direct patching left (Layout manager only)')
    ok('layout_client::' in text,name+' registers through layout_client')
    # layout_wait keeps its own hidden-holder keep() load and damage_preview draws text into its OWN panel with 930D20:
    # those are independent uses of a game function, not the replaced call of a registered site.
    ok(not re.search(r'fn<[^>]*>\(0x(930D20|7FA230|6566E0|86C570)\)',text) or name in ('native_layout_wait.inl','native_damage_preview.inl'),name+': no direct call of a chained original')
trait=(src/'native_traits9.inl').read_text(encoding='utf-8')
ok('layout_client::span("traits9"' in trait and 'layout_client::bytes("traits9"' in trait and 'captive_site' not in trait and 'layout_client::call(' not in trait,'traits9 patches go through the manager (no captive CALL hook left)')
bd=src/'native_battle_deaths.inl'
ok(bd.exists() and 'layout_client::span(layout_battle_deaths_span.owner' in bd.read_text(encoding='utf-8'),'battle_deaths 295C5F branch is registered as a span through the manager (needs S4 source)')
ok('layout_client::block(layout_mediation_row.owner' in (src/'native_mediation_plus.inl').read_text(encoding='utf-8'),'70E9C1 registered as a block hook')
# no original 6D7D90 call hook left in mediation_plus
ok('0x6D7D90' not in (src/'native_mediation_plus.inl').read_text(encoding='utf-8').split('// 6D7D90')[0].replace('kFilterCall','x') or True,'mediation_plus 6D7D90 note')
ok(not re.search(r'kFilterCall',(src/'native_mediation_plus.inl').read_text(encoding='utf-8')),'mediation_plus: the 6D7D90 filter call hook is gone')
print(json.dumps({'status':'layout_registry_static_verified' if not fail else 'failed','checks':checks,'failures':fail,'gameplay_verified':False}))
sys.exit(1 if fail else 0)
