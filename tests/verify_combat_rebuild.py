import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Read-only separation audit. Does not load DLLs or access a game process."""
from pathlib import Path
import hashlib,json,re

project=Path(__file__).resolve().parents[1]
game=project.parent
expected=json.loads((project/'build/development/combat_v2/before_build_hashes.json').read_text(encoding='utf-8-sig'))
checks=0
def check(value,message):
    global checks
    checks+=1
    if not value:raise AssertionError(message)

for path in (project/'src/combat_v2').iterdir():
    if path.suffix not in ('.cpp','.h'):continue
    code=re.sub(r'/\*.*?\*/|//[^\n]*','',path.read_text(encoding='utf-8'),flags=re.S)
    for forbidden in ('campaign_safe_route','campaign_city_danger','campaign_fire_avoid',
                      'campaign_guard','campaign_request_guard','campaign_defensive_wait','try_other',
                      'DllMain','install_hooks','WriteProcessMemory','CreateRemoteThread','LoadLibraryW'):
        check(not re.search(r'\b'+forbidden+r'\s*\(',code),f'Excluded behavior/connection: {path.name} {forbidden}')
    for include in re.findall(r'#include\s+"([^"]+)"',code):
        check('/' not in include and '\\' not in include and (path.parent/include).is_file(),f'Legacy include dependency: {path.name}')
for name in ('native_adapter.cpp','input_proxy.cpp','module_loader.cpp','native_split_install.inl'):
    check('combat_v2' not in (project/'src'/name).read_text(encoding='utf-8'),f'Runtime connected new combat code: {name}')
registry=json.loads((project/'development/features.json').read_text(encoding='utf-8'))
for name in registry['modules']['combat-rebuild']['sources']:
    check((project/name).is_file(),f'Missing selected source: {name}')
actual={}
check(set(expected)=={'dinput8.dll','SAN14_CombatAI.dll','SAN14_Convenience.dll','SAN14CE_KO.exe',
                      'AI_TacticsMod/dist/SAN14_AI_v11.7_integrated.zip'},'Protected artifact inventory differs')
for name,digest in expected.items():
    actual[name]=hashlib.sha256((game/name).read_bytes()).hexdigest()
    check(actual[name]==digest,f'Installed/original/shared artifact changed: {name}')
out=project/'build/development/combat_v2'
check((out/'SAN14_CombatAI_v2_unlinked.lib').is_file(),'Missing unlinked library')
check(not list(out.glob('*.dll')),'Unexpected runtime DLL in unlinked build')
proof={'status':'combat_v2_separation_verified','checks':checks,'installed_files_unchanged':True,
       'game_connected':False,'gameplay_verified':False,'files':actual}
(out/'installed_verification.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({key:value for key,value in proof.items() if key!='files'}))
