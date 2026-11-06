import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Static feature switch audit: menu catalog, native config reads, and gates."""
from pathlib import Path
import re, sys, json
P=Path(__file__).resolve().parents[1]
menu=(P/'distribution/FeatureSelect.ps1').read_text(encoding='utf-8-sig')
src=P/'src'
sources={q.name:q.read_text(encoding='utf-8',errors='replace') for q in src.iterdir() if q.is_file() and q.suffix.lower() in {'.cpp','.h','.inl'} and q.name!='native_target_retreat.inl'} # Fixture only: included by TARGET_RETREAT_FIXTURE and never installed.
split=sources['native_split_install.inl']
checks=0; failures=[]
def ok(condition,message):
    global checks
    checks+=1
    if not condition: failures.append(message)
# Parse catalog lines, excluding Retired=$true menu entries.
menu_keys=set(); retired=set(); item_count=0
for line in menu.splitlines():
    if not line.strip().startswith('@{Group='): continue
    sec=re.search(r"Section\s*=\s*'([^']+)'",line)
    key=re.search(r"Key\s*=\s*'([^']+)'",line)
    if not sec or not key: continue
    item_count+=1; pair=(sec.group(1),key.group(1))
    if re.search(r'Retired\s*=\s*\$true',line): retired.add(pair)
    else: menu_keys.add(pair)
# DLL switch reads across every native source translation unit/include.
dll_keys=set(); retired_reads=set(); feature_keys=set()
feature_pattern=r'feature_key\s*\(\s*config\s*,\s*L"([^"]+)"\s*,\s*L"([^"]+)"'
patterns=[feature_pattern,
          r'GetPrivateProfileIntW\s*\(\s*L"([^"]+)"\s*,\s*L"([^"]+)"',
          r'mod_setting\s*\(\s*L"([^"]+)"\s*,\s*L"([^"]+)"']
for name,text in sources.items():
    for pattern in patterns:
        dll_keys.update((a,b) for a,b in re.findall(pattern,text))
    feature_keys.update((a,b) for a,b in re.findall(feature_pattern,text))
    dll_keys.update(('groups',k) for k in re.findall(r'group_enabled\s*\(\s*L"([^"]+)"\s*\)',text))
menu_raw=re.search(r'RawSettings\s*=\s*\{([^}]+)\}',(P/'installer/InstallerGui.cs').read_text(encoding='utf-8'))
raw={tuple(s.split('.',1)) for s in re.findall(r'"([\w]+\.[\w]+)"',menu_raw.group(1) if menu_raw else '')}
dll_keys-=raw
dll_keys.discard(('Mod','Enabled'))  # Layout reads other R1 modules' own ini ([Mod] Enabled in SAN14Mods\modules\*.ini), not 기능설정.ini (native_layout_core.inl)
ok(menu_keys==dll_keys,'menu/DLL key sets differ: menu-only=%s DLL-only=%s'%(sorted(menu_keys-dll_keys),sorted(dll_keys-menu_keys)))
for pair in retired:
    found=pair in dll_keys
    ok(not found,'retired menu key is still read by native code: %s.%s'%pair)
# Keep the prior switch-gates-code audit for each config read (group masters are loader routing).
all_native='\n'.join(sources.values())
for section,key in sorted(feature_keys):
    if section=='groups': continue
    escaped=re.escape(key)
    use=re.search(r'mod_features\.'+escaped+r'\b|\bf\.'+escaped+r'\b',all_native)
    ok(bool(use),'switch gates nothing: %s.%s'%(section,key))
ok('split_core_enabled' in split and 'split_pointer_enabled' in split,'publish gates present')
# User decision 2026-11-03: retired CCZ switches stay OFF; sprites own their paired behavior.
adapter=sources['native_adapter.cpp']
ok('f.population_rate = (std::max)(1u, (std::min)(4u, GetPrivateProfileIntW(L"domestic", L"population_rate", 4, config)))' in adapter and
   'if(mod_features.population_rate<=1)return result;' in sources['native_population.inl'] and
   '(std::min)(60000u,s.before+mod_features.population_rate*(now-s.before))' in sources['native_population.inl'],
   'population_rate clamps to 1..4, 1x uses original growth, and growth keeps the 60000 cap')
# User decision 2026-11-05 (local6): merit stage 1/2 troops are chosen 1500/2000/2500/3000 (default 3000), stage 3 friendship is a switch (default on).
merit_src=sources['native_merit.inl']
ok('f.merit_friendship = f.merit && feature_key(config, L"personnel", L"merit_friendship", true);' in adapter and
   'f.merit_troops_level = (std::max)(1u, (std::min)(4u, GetPrivateProfileIntW(L"personnel", L"merit_troops_level", 4, config)));' in adapter and
   'merit_troops_choices[]={1500,2000,2500,3000}' in merit_src and 'merit_troops_choices[(mod_features.merit_troops_level-1)&3]' in merit_src and
   merit_src.count('mod_features.merit_friendship')>=7 and 'merit_friendship && now.force==old.force' in merit_src,
   'merit troops choice clamps to 1..4 (default 3000) and merit_friendship gates awards, bond effects, profile label and stage-3 row/text')
ok('const bool ccz = GetPrivateProfileIntW(L"groups", L"ccz", 1, config) != 0;' in adapter and
   'f.ccz_sprites = on && ccz && feature_key(config, L"ccz", L"ccz_sprites", false);' in adapter and
   'f.ccz_hide_model = f.ccz_sprites;' in adapter and 'f.ccz_depth = f.ccz_sprites;' in adapter and
   'f.ccz_far_keep = false;' in adapter and
   'f.ccz_block = on && ccz && feature_key(config, L"ccz", L"ccz_block", false);' in adapter and
   'f.ccz_critical = on && ccz && feature_key(config, L"ccz", L"ccz_critical", false);' in adapter,
   'ccz_sprites enables hide/depth, far_keep stays OFF, and groups.ccz gates all three active features')
ok('if(mod_features.distance)install_progress_native();' in adapter,
   'distance=0 prevents progress_native installation')
print(json.dumps({'status':'feature_switches_static_verified' if not failures else 'failed','checks':checks,'menu_items':len(menu_keys),'retired_items':len(retired),'native_keys':len(dll_keys),'failures':failures,'gameplay_verified':False},ensure_ascii=False))
sys.exit(1 if failures else 0)
