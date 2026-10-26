"""Static check: the GUI installer catalog (installer/InstallerGui.cs) equals the
distribution/FeatureSelect.ps1 catalog (section/key/default/group/requires/inherit/tab), the
DLL applies every Requires as an AND, and the GUI delegates DLL installation to the guarded
관리.ps1 (-NonInteractive). Not a UI/game check."""
from pathlib import Path
import json,re,sys
P=Path(__file__).resolve().parents[1]
gui=(P/'installer/InstallerGui.cs').read_text(encoding='utf-8')
menu=(P/'distribution/FeatureSelect.ps1').read_text(encoding='utf-8-sig')
adapter=(P/'src/native_adapter.cpp').read_text(encoding='utf-8')
fail=[];checks=0
def ok(cond,what):
    global checks;checks+=1
    if not cond:fail.append(what)
rows=re.findall(r'new Feature\("([^"]+)", "(\w*)", "(\w+)", "(\w+)", (\d), "([\w.]*)", "([\w.]*)"',gui)
ps=re.findall(r"Group='(\w*)'; Section='(\w+)'; Key='(\w+)'; Default=(\d); Requires='([\w.]*)'; Inherit='([\w.]*)'; Tab='([^']+)'",menu)
cs={(s,k):(g,int(d),r,i,t) for t,g,s,k,d,r,i in rows}
pm={(s,k):(g,int(d),r,i,t) for g,s,k,d,r,i,t in ps}
ok(len(rows)==len(cs) and len(ps)==len(pm),'duplicate keys')
ok(cs==pm,'GUI/PS catalogs differ: %s'%sorted(k for k in set(cs)|set(pm) if cs.get(k)!=pm.get(k)))
ids={s+'.'+k for s,k in cs}
tabs=('전투','편의성','성능','게임성','기타(개발용)')
for (s,k),(g,d,r,i,t) in cs.items():
    ok(t in tabs,'unknown tab %s'%t)
    ok(g=='' or ('groups.'+g) in ids,'unknown group for %s.%s'%(s,k))
    ok(r=='' or r in ids,'unknown parent for %s.%s'%(s,k))
    ok(i=='' or i in ids,'unknown inherit for %s.%s'%(s,k))
    if r:
        # The DLL must AND the parent so a child can never run alone.
        parent=r.split('.')[1]
        ok(re.search(r'f\.%s = f\.%s && feature_key\(config, L"%s", L"%s"'%(k,parent,s,k),adapter) is not None,'DLL does not AND %s with %s'%(k,parent))
    if i:
        ok(re.search(r'f\.%s = on && feature_key\(config, L"%s", L"%s", f\.%s\)'%(k,s,k,i.split('.')[1]),adapter) is not None,'DLL inherit differs for %s'%k)
masters={k for (s,k),(g,*_) in cs.items() if g==''}
ok(cs.get(('combat','district_intercept_block'))==('convenience',1,'','','게임성'),'district intercept policy must be an independent gameplay option owned by the convenience module (no combat AI needed)')
ok(cs.get(('stratagem','building_destruction_all'))==('convenience',0,'','','게임성'),'area building destruction must be an opt-in convenience gameplay option')
# Dependent rows: the checkbox starts after the measured width of the "└ 종속" tag (any DPI), and the tag toggles it too.
ok('int tagWidth = nested ? TextRenderer.MeasureText(tag.Text, tag.Font).Width : 0;' in gui and 'nested ? 28 + tagWidth + 16' in gui,'dependent tag must not overlap its checkbox')
ok('tag.Click += (s, e) => { if (Box.Enabled) Box.Checked = !Box.Checked; };' in gui,'dependent tag click toggles its checkbox')
ok(masters=={'combat','convenience','performance','diagnostics'},'master switches %s'%sorted(masters))
for t in tabs:ok(any(v[4]==t for v in cs.values()),'empty tab %s'%t)
# User classification (2026-10-19): combat tab = AI behaviour correction only.
ok(all(g=='combat' or (s,k)==('groups','combat') for (s,k),(g,d,r,i,t) in cs.items() if t=='전투'),'non-combat item in 전투 tab')
for key,tab in (('battle_rules','편의성'),('battle_deaths','편의성'),('awards','편의성'),('recruit','편의성'),
                ('merit','게임성'),('injury','게임성'),('domestic','게임성')):
    ok(any(k==key and v[4]==tab for (s,k),v in cs.items()),'%s must be in %s'%(key,tab))
ok('1군단' in gui[gui.index('"domestic", "domestic"'):][:400] and '"domestic_direct", 0' in gui,'auto domestic must state the ruler direct-corps exclusion')
ok("-NonInteractive" in gui and '관리.ps1' in gui,'installer must delegate to guarded 관리.ps1 non-interactively')
ok('WriteAllBytes' not in gui and 'dinput8.dll' not in gui,'GUI must not write mod DLLs directly')
ok('package_files.txt' in gui and 'EndsWith(".dll"' in gui and 'StartsWith(root' in gui,'package copy limited to listed files inside the game folder')
ok('게임 폴더 변경' in gui and 'libraryfolders.vdf' in gui,'folder picker and Steam library lookup present')
exe=Path(sys.argv[1]) if len(sys.argv)>1 else None
if exe:
    data=exe.read_bytes();ok(data[:2]==b'MZ' and b'BSJB' in data,'managed PE produced')
print(json.dumps({'status':'installer_gui_static_verified' if not fail else 'failed','checks':checks,'items':len(rows),
                  'failures':fail,'ui_verified':False},ensure_ascii=False))
sys.exit(1 if fail else 0)
