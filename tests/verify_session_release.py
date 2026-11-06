import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Inspect actual release binaries; never loads the game or writes its memory.
LEGACY (2026-11-03): pre-split v12 release proof, used only by old prepare_*_release scripts and not part of
check_feature / dll-split. It matches log event names as raw substrings, so functional labels that reuse
the same words (e.g. AI reason tag "ap:build_hold_wait") fail it. Do not run it in the current flow."""
from pathlib import Path
import hashlib,json,re,struct,sys
PROJECT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(PROJECT/'legacy/FogMod_Source/tools'))
from audit_native_game import PEImage
binary=PROJECT/'build/development/loader_split'
def cstring(pe,rva):
    data=bytearray()
    for i in range(512):
        value=pe.read(rva+i,1)
        if value==b'\0':return data.decode('ascii')
        data.extend(value)
    raise AssertionError('Unterminated import name')
def imports(path):
    pe=PEImage(path);optional=struct.unpack_from('<I',pe.data,0x3c)[0]+24
    base=struct.unpack_from('<I',pe.data,optional+120)[0];result=[]
    for n in range(100):
        lookup,stamp,chain,name,address=struct.unpack('<IIIII',pe.read(base+n*20,20))
        if not name:break
        thunk=lookup or address
        for i in range(2000):
            value=struct.unpack('<Q',pe.read(thunk+i*8,8))[0]
            if not value:break
            if not value&(1<<63):result.append(cstring(pe,value+2))
    return result
checks=0;files={}
for name in ('native_campaign_routes.inl','native_campaign_operations.inl','native_sortie.inl'):
    source_text=(PROJECT/'src'/name).read_text(encoding='utf-8')
    for removed in ('campaign_region_chain','campaign_siege_waypoint','campaign_siege_leg'):
        assert removed not in source_text,(name,removed);checks+=1
event_names=set()
timing_names=set()
functional_phase_names=set()
interface_names=set()
for source in (PROJECT/'src').rglob('*'):
    if source.suffix not in ('.cpp','.h','.inl'):continue
    text=source.read_text(encoding='utf-8')
    event_names.update(re.findall(r'log_event\("([^"\n]+)"',text))
    timing_names.update(re.findall(r'COST_SCOPE\("([^"\n]+)"',text))
    functional_phase_names.update(re.findall(r'NativePhase\s+\w+\("([^"\n]+)"',text))
    interface_names.update(re.findall(r'__declspec\(dllexport\)[^(;{]*?\b(\w+)\s*\(',text))
    interface_names.update(re.findall(r'GetProcAddress\([^,]+,\s*"(\w+)"',text))
for name in ('dinput8.dll','SAN14_CombatAI.dll','SAN14_Convenience.dll'):
    data=(binary/name).read_bytes();files[name]=hashlib.sha256(data).hexdigest()
    # Dispatch/search names also drive the reentrancy crash guard; retain those.
    # Split module interface names are export/GetProcAddress strings in every build; a timing scope may reuse one.
    # log_event/COST_SCOPE names are narrow strings; the same words as wide strings are settings keys.
    for marker in sorted((event_names|(timing_names-functional_phase_names))-interface_names):
        assert marker.encode() not in data,(name,marker);checks+=1
    for marker in ('SAN14_AITactics.log','SAN14_CombatAI.log','function_cost_'):
        assert marker.encode() not in data and marker.encode('utf-16le') not in data,(name,marker);checks+=1
    assert 'AddVectoredExceptionHandler' not in imports(binary/name),name;checks+=1
combat_imports=imports(binary/'SAN14_CombatAI.dll')
# The static Windows CRT can retain file API imports for its own error paths.
# Their presence does not identify a mod log writer or a published game hook.
file_imports=[f for f in ('CreateFileW','ReadFile','WriteFile','FlushFileBuffers') if f in combat_imports]
flags=(PROJECT/'tools/build_split.ps1').read_text(encoding='utf-8')
assert "elseif(-not $RuntimeDiagnostics) {$taskFlags+='/DNO_RUNTIME_LOG'}" in flags;checks+=1
report={'status':'session_release_binary_fixture_passed','checks':checks,'files':files,
        'runtime_log_writers_compiled_out':True,'combat_file_api_imports':file_imports,
        'function_timing_writer_compiled_out':True,
        'siege_region_route_functions_absent':True,
        'all_literal_event_names_checked':len(event_names),'literal_timing_names_checked':len(timing_names-functional_phase_names),
        'functional_phase_names_retained_for_guards':sorted(functional_phase_names),
        'gameplay_verified':False}
(PROJECT/'build/session_release_binary_fixture.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report))
