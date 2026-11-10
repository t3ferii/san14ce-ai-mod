import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Static check: the GUI installer catalog (installer/InstallerGui.cs) equals the
distribution/FeatureSelect.ps1 catalog (section/key/default/group/requires/inherit/tab), the
DLL applies every Requires as an AND, and the GUI delegates DLL installation to the guarded
관리.ps1 (-NonInteractive). Not a UI/game check."""
from pathlib import Path
import ast,json,re,sys
P=Path(__file__).resolve().parents[1]
gui=(P/'installer/InstallerGui.cs').read_text(encoding='utf-8')
menu=(P/'distribution/FeatureSelect.ps1').read_text(encoding='utf-8-sig')
adapter=(P/'src/native_adapter.cpp').read_text(encoding='utf-8')
split_manage=(P/'distribution/SplitManage.ps1').read_text(encoding='utf-8-sig')
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
# 사용자 결정 2026-10-28: 충돌 옵션 사용 중지 (ExcludedBy보다 우선, mission_v2 OFF도 동일).
# ??? ?? 2026-11-03: ?? ?? ?? ??? ?? ?? ?? ?? ???? ??.
# 사용자 결정: 사용 중지 키 읽기 제거 2026-11-03.
retired={'combat.departures','combat.group_advance','combat.support','diplomacy.diplomacy_overinvest','officer_ui.unified',
         'ccz.ccz_hide_model','ccz.ccz_depth','ccz.ccz_far_keep','performance.timer_1ms'}
retired_body=re.search(r'HashSet<string> Retired = new HashSet<string> \{(.*?)\};',gui,re.S)
ok(retired_body is not None and set(re.findall(r'"([\w.]+)"',retired_body[1]))==retired,'GUI retired set')
ps_retired={s+'.'+k for s,k in re.findall(r"Section='(\w+)'; Key='(\w+)';[^\r\n]*Retired=\$true",menu)}
ok(ps_retired==retired,'PS retired set equals GUI')
ok((P/'FeatureSelect.ps1').read_text(encoding='utf-8-sig')==menu,'game-folder PS catalog equals distribution')
ok((P/'SplitManage.ps1').read_text(encoding='utf-8-sig')==split_manage,'game-folder guarded manager equals distribution')
ok("$taskInstalledHash -eq '553ae296eabe73cc911cb79de0bf9bf11e594dbb4f2a63515ca8e8eb5fecb937'" in split_manage and
   "$taskState.release_version -like 'v15*'" in split_manage and "$taskManifest.release_version -eq 'v15.1'" in split_manage,
   'v15.1 accepts only the exact known local14 CCZ override')
ok('f.departures = f.group_advance = f.support = false;' in adapter and
   'if (f.mission_v2) { f.departures = false;' not in adapter,'DLL retired switches unconditionally OFF')
# 사용자 결정: 사용 중지 키 읽기 제거 2026-11-03.
for key in ('departures','group_advance','support'):
    ok(not re.search(r'feature_key\(config, L"combat", L"%s"'%key,adapter),'retired combat key is not read: '+key)
ok('retired 2026-10-28 (mission_v2 conflict); keys no longer read' in adapter,'DLL combat retirement decision recorded')
ok('f.diplomacy_overinvest = false; // retired 2026-11-03: native cap/marker restored; key no longer read' in adapter and
   not re.search(r'feature_key\(config, L"diplomacy", L"diplomacy_overinvest"',adapter),'retired diplomacy key is not read')
ok('!f.IsMaster && !Catalog.Retired.Contains(f.Id)' in gui,'tab denominator excludes retired rows')
ok(menu.count("Retired=$true;")==9 and
   menu.count("Note='\uc784\ubb34 \uc77c\uad00\uc131 v2\uc640 \ucda9\ub3cc\ud574 \uc0ac\uc6a9\uc774 \uc911\uc9c0\ub41c \uae30\ub2a5\uc785\ub2c8\ub2e4.'")==3 and
   menu.count("Note='\uac1c\ubc1c \uc911\uc9c0\ub85c \uc81c\uac70\ub41c \uae30\ub2a5\uc785\ub2c8\ub2e4.'")==1 and
   '\uac1c\ubc1c \uc911\uc9c0\ub85c \uc81c\uac70\ub41c \uae30\ub2a5' in gui,'user decision 2026-11-03: retirement explanations')
ok(cs.get(('diagnostics','terrain_diag_mode'))==('diagnostics',0,'','','기타(개발용)'),'terrain diagnostic mode catalog/default/group/tab')
ok('Math.Max(0, Math.Min(3, n))' in gui and "[Math]::Max(0, [Math]::Min(3, $taskNumber))" in menu,'terrain diagnostic mode keeps 0..3')
ok('0 — 기준: foam ON / renderer ON' in gui and '[0 기준]' in menu and '번호=값' in menu,'GUI/PS expose four terrain diagnostic choices')
ok('A/B/C는 진단 전용입니다.' in gui and '실제 지형 편집·저장에는 사용하지 마세요.' in menu,'terrain diagnostic warning')
ok('AssemblyVersion("15.4.0.0")' in gui,'installer assembly version 15.4.0.0')
tabs=('게임렉제거(성능)','전투','편의성','게임성','조조전','기타(개발용)')
for (s,k),(g,d,r,i,t) in cs.items():
    ok(t in tabs,'unknown tab %s'%t)
    ok(g=='' or ('groups.'+g) in ids,'unknown group for %s.%s'%(s,k))
    ok(r=='' or r in ids,'unknown parent for %s.%s'%(s,k))
    ok(i=='' or i in ids,'unknown inherit for %s.%s'%(s,k))
    if (s,k)==('diagnostics','ai_trace_original'):
        ok('f.trace_original = f.ai_trace && GetPrivateProfileIntW(L"diagnostics", L"ai_trace_original"' in adapter,'DLL does not AND ai_trace_original with ai_trace')
    elif r and (s,k) not in {('progress_native','geometry'),('ccz','ccz_hide_model'),('ccz','ccz_depth'),('ccz','ccz_far_keep'),('domestic','population_rate'),('personnel','merit_troops_level')}:
        # The DLL must AND the parent so a child can never run alone.
        parent=r.split('.')[1]
        ok(re.search(r'f\.%s = f\.%s && feature_key\(config, L"%s", L"%s"'%(k,parent,s,k),adapter) is not None,'DLL does not AND %s with %s'%(k,parent))
    if i:
        ok(re.search(r'f\.%s = on && feature_key\(config, L"%s", L"%s", f\.%s\)'%(k,s,k,i.split('.')[1]),adapter) is not None,'DLL inherit differs for %s'%k)
masters={k for (s,k),(g,*_) in cs.items() if g==''}
# W9 dependency audit: every feature-level AND / split inheritance in the DLL must be a child row in the installer.
expected_requires={
 ('combat','builder_ruler_fill'):'combat.mission_v2',
 ('domestic','region_officer_traits'):'domestic.region_officers',
 ('personnel','awards_ai'):'personnel.awards',
 ('personnel','merit_friendship'):'personnel.merit',
 ('domestic','domestic_free'):'domestic.domestic',
 ('domestic','domestic_direct'):'domestic.domestic',
 ('extensions','stats200_growth'):'extensions.stats200',
 ('performance','face_prefetch'):'performance.face_keep',
 ('diagnostics','ai_trace_original'):'diagnostics.ai_trace',
 ('real_estate','terrain_edit'):'game_flow.advisor_briefing',
}
for (s,k),parent in expected_requires.items():
    ok(cs.get((s,k),(None,None,None,None,None))[2]==parent,'installer Requires missing for %s.%s -> %s'%(s,k,parent))
expected_inherit={('personnel','recruit_ai'):'personnel.recruit',('personnel','injury_herbal'):'personnel.injury'}
for (s,k),parent in expected_inherit.items():
    ok(cs.get((s,k),(None,None,None,None,None))[3]==parent,'installer Inherit missing for %s.%s -> %s'%(s,k,parent))
# Player-facing descriptions: every active non-development item explains itself and keeps internal evidence out.
imap_data=json.loads((P/'development/installer_map.json').read_text(encoding='utf-8-sig'))
for item in imap_data['items']:
    if item.get('retired') or item['section'] in ('groups','diagnostics'):
        continue
    note=item.get('note','')
    ok(len(note)>=30 and not re.search(r'GA_|0x[0-9A-Fa-f]|RVA|훅|hook',note),
       'installer description is too short or technical: %s.%s'%(item['section'],item['key']))
ok(cs.get(('combat','district_intercept_block'))==('convenience',1,'','','게임성'),'district intercept policy must be an independent gameplay option owned by the convenience module (no combat AI needed)')
ok(cs.get(('stratagem','building_destruction_all'))==('convenience',0,'','','게임성'),'area building destruction must be an opt-in convenience gameplay option')
# Dependent rows: the checkbox starts after the measured width of the "└ 종속" tag (any DPI), and the tag toggles it too.
ok('int tagWidth = nested ? TextRenderer.MeasureText(tag.Text, tag.Font).Width : 0;' in gui and 'nested ? 28 + tagWidth + 16' in gui,'dependent tag must not overlap its checkbox')
ok('tag.Click += (s, e) => { if (Box.Enabled) Box.Checked = !Box.Checked; };' in gui,'dependent tag click toggles its checkbox')
ok(masters=={'combat','convenience','performance','diagnostics','ccz'},'master switches %s'%sorted(masters))
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
# 사용자 결정 2026-10-30: 기능 설명뿐 아니라 출처 탭 전체 목록과 배포 차단을 검증한다.
credits=[line for line in (P/'docs/CREDITS.md').read_text(encoding='utf-8-sig').splitlines() if line.startswith('- ')]
credits_body=re.search(r'static class Credits \{.*?string\[\] Lines = \{(.*?)\};',gui,re.S)
ok(len(credits)==11 and credits_body is not None and re.findall(r'"([^"]+)"',credits_body[1])==credits,'credits tab matches all eleven canonical lines in order')
ok('pages["출처"] = creditsPage;' in gui and 'new[] { "출처", "업데이트", "작업 기록" }' in gui,'credits page registered as a peer tab')
for key,source in re.findall(r'^\* `(\w+)` = (.+)$',(P/'docs/CREDITS.md').read_text(encoding='utf-8-sig'),re.M):
    ok(any('"'+key+'",' in row and '출처: '+source in row for row in gui.splitlines()),'GUI credit note: '+key)
    ok(any("Key='"+key+"';" in row and '출처: '+source in row for row in menu.splitlines()),'PS credit note: '+key)
if exe:
    data=exe.read_bytes();ok(data[:2]==b'MZ' and b'BSJB' in data,'managed PE produced')
    for line in credits:ok(line.encode('utf-16-le') in data,'EXE credits tab line: '+line)
    # Run only build_share's real credits preflight block; never execute ZIP/package writes.
    share_tree=ast.parse((P/'tools/build_share.py').read_text(encoding='utf-8-sig'))
    credit_blocks=[node for node in share_tree.body if isinstance(node,ast.If) and any(isinstance(child,ast.Name) and child.id=='gui_bytes' for child in ast.walk(node))]
    ok(len(credit_blocks)==1,'share credits-only preflight found once')
    if len(credit_blocks)==1:
        preflight=compile(ast.Module(body=credit_blocks,type_ignores=[]),'build_share.py credits preflight','exec')
        class Payload:
            def __init__(self,content):self.content=content
            def __truediv__(self,name):return self
            def read_bytes(self):return self.content
        sources=re.findall(r'^\* `(\w+)` = (.+)$',(P/'docs/CREDITS.md').read_text(encoding='utf-8-sig'),re.M)
        def run_preflight(content,without_gui=False):
            exec(preflight,{'manifest':{'gui_installer':{'file':'fixture.exe'}},'WITHOUT_GUI':without_gui,
                            'PAYLOAD':Payload(content),'credits':credits,'installer_credits':sources})
        run_preflight(data);ok(True,'share accepts rebuilt EXE credits')
        for line in credits:
            encoded=line.encode('utf-16-le')
            try:
                run_preflight(data.replace(encoded,b'\0'*len(encoded)))
            except RuntimeError as error:
                ok(str(error)=='Installer EXE lacks credits tab line: '+line+'; rebuild it.','share rejects missing line with rebuild guidance: '+line)
            else:ok(False,'share accepted missing credits line: '+line)
        run_preflight(b'',without_gui=True);ok(True,'share credits preflight preserves without-GUI packages')
print(json.dumps({'status':'installer_gui_static_verified' if not fail else 'failed','checks':checks,'items':len(rows),
                  'failures':fail,'ui_verified':False},ensure_ascii=False))
sys.exit(1 if fail else 0)
