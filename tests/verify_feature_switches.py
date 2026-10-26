"""Static check: installer menu keys (distribution/FeatureSelect.ps1) and the keys the
DLLs read (native_adapter.cpp, module_loader.cpp) are the same set, and every switch
gates code. Not a game check."""
from pathlib import Path
import json,re,sys
P=Path(__file__).resolve().parents[1]
menu=(P/'distribution/FeatureSelect.ps1').read_text(encoding='utf-8-sig')
adapter=(P/'src/native_adapter.cpp').read_text(encoding='utf-8')
loader=(P/'src/module_loader.cpp').read_text(encoding='utf-8')
split=(P/'src/native_split_install.inl').read_text(encoding='utf-8')
checks=0;fail=[]
def ok(cond,what):
    global checks;checks+=1
    if not cond:fail.append(what)
items=re.findall(r"Section='(\w+)';\s*Key='(\w+)';\s*Default=(\d)",menu)
menu_keys={(s,k) for s,k,_ in items}
dll_keys=set(re.findall(r'feature_key\(config, L"(\w+)", L"(\w+)"',adapter))
dll_keys|={('groups',k) for k in re.findall(r'GetPrivateProfileIntW\(L"groups", L"(\w+)"',adapter)}
dll_keys|={('groups',k) for k in re.findall(r'group_enabled\(L"(\w+)"\)',loader)}
hidden={('performance','timer_1ms')}
ok(menu_keys==dll_keys-hidden,'menu/dll key sets differ: menu-only %s, dll-only %s'%(sorted(menu_keys-dll_keys),sorted(dll_keys-hidden-menu_keys)))
ok(len(items)==len(menu_keys),'duplicate menu keys')
for s,k in dll_keys-{('groups','combat')}:
    if s=='groups':continue
    used=re.search(r'mod_features\.%s\b|\bf\.%s\b'%(k,k),split+adapter.split('static void load_mod_features')[1].split('\n}\n',1)[1]) or \
         any(re.search(r'mod_features\.%s\b'%k,q.read_text(encoding='utf-8',errors='replace')) for q in (P/'src').glob('*.inl'))
    ok(bool(used),'switch %s.%s gates nothing'%(s,k))
ok('split_core_enabled' in split and 'split_pointer_enabled' in split,'publish gates present')
print(json.dumps({'status':'feature_switches_static_verified' if not fail else 'failed','checks':checks,'menu_items':len(items),'failures':fail,'gameplay_verified':False},ensure_ascii=False))
sys.exit(1 if fail else 0)
