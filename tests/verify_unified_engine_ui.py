"""Check the built native UI boundary and preserve the installed modules."""
from pathlib import Path
import hashlib,json,re,struct

project=Path(__file__).resolve().parents[1]
game=project.parent
candidate=project/'build/development/loader_split'
output=project/'build/development/unified_personnel'
checks=0
def check(ok,label):
    global checks
    assert ok,label
    checks+=1
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def text_digest(path):
    data=path.read_bytes();pe=struct.unpack_from('<I',data,60)[0]
    count=struct.unpack_from('<H',data,pe+6)[0]
    table=pe+24+struct.unpack_from('<H',data,pe+20)[0]
    for i in range(count):
        at=table+40*i
        if data[at:at+8].rstrip(b'\0')==b'.text':
            size,offset=struct.unpack_from('<II',data,at+16)
            return hashlib.sha256(data[offset:offset+size]).hexdigest()
    raise AssertionError(f'Missing .text: {path}')
sources=json.loads((project/'development/features.json').read_text(encoding='utf-8'))['modules']['unified-personnel']['sources']
def executable_pointer(rva):
    data=(game/'SAN14CE_KO.exe').read_bytes();pe=struct.unpack_from('<I',data,60)[0]
    optional=pe+24;base=struct.unpack_from('<Q',data,optional+24)[0]
    count=struct.unpack_from('<H',data,pe+6)[0]
    table=optional+struct.unpack_from('<H',data,pe+20)[0]
    for i in range(count):
        at=table+40*i;virtual,raw_size,raw=struct.unpack_from('<III',data,at+12)
        if virtual<=rva and rva+8<=virtual+raw_size:
            return struct.unpack_from('<Q',data,raw+rva-virtual)[0]-base
    raise AssertionError(f'Unmapped native hook slot: {rva:X}')
# Fixtures seed their own tables. Check the actual EXE too, so a mistaken
# slot cannot disable the entire convenience module while mock tests pass.
pointer_source=(project/'src/native_split_install.inl').read_text(encoding='utf-8').split('struct SplitInstallState')[0]
native_hooks=re.findall(r'\{0x([0-9A-Fa-f]+),0x([0-9A-Fa-f]+),reinterpret_cast',pointer_source)
check(len(native_hooks)>=12,'Both owners native pointer guards discovered')
native_binaries=[(candidate/name).read_bytes() for name in ('SAN14_CombatAI.dll','SAN14_Convenience.dll')]
for slot,original in native_hooks:
    check(executable_pointer(int(slot,16))==int(original,16),f'Actual executable hook slot {slot} -> {original}')
    guard=struct.pack('<II',int(slot,16),int(original,16))
    check(any(guard in data for data in native_binaries),f'Built binary includes exact native hook guard {slot} -> {original}')
for relative in sources:
    if relative.startswith('src/native_unified_'):
        source=(project/relative).read_text(encoding='utf-8-sig')
        if relative=='src/native_unified_overlay.inl':
            check('WS_POPUP' in source and 'ShowWindow(' not in source and 'fn<' not in source and 'open_main(' not in source,'Only a hidden DXGI probe; render/input threads never call the game')
        else:
            check(not re.search(r'\b(?:CreateWindow\w*|MessageBox\w*|HWND|WNDCLASS\w*|GetAsyncKeyState|EnumWindows|WM_COMMAND)\b',source),relative)
names=['dinput8.dll','SAN14_CombatAI.dll','SAN14_Convenience.dll']
current=(project/'development/CURRENT.md').read_text(encoding='utf-8')
installed={name:digest(game/name) for name in names}
for name in names:
    recorded=re.search(re.escape(name)+r': ([0-9a-f]{64})',current)
    check(recorded and installed[name]==recorded[1],f'Installed file unchanged: {name}')
same_text={name:text_digest(game/name)==text_digest(candidate/name) for name in names}
for name in names[:2]:check(same_text[name],f'No loader/combat execution code changes: {name}')
payload=(candidate/'SAN14_Convenience.dll').read_bytes()
for api in [b'CreateWindowExA',b'MessageBoxW',b'RegisterClassW',b'GetMessageW',b'GetAsyncKeyState',b'COMCTL32.dll']:
    check(api not in payload,f'No separate OS UI dependency: {api!r}')
marker='무장 1명당 교역 공물'.encode('utf-16le')
owners={name:marker in (candidate/name).read_bytes() for name in names}
check(owners==dict(zip(names,[False,False,True])),'Native personnel UI is owned only by convenience')
check('SAN14UnifiedPersonnel'.encode('utf-16le') not in payload,'Old Win32 window class removed')
results={}
for key,file in [('policy','result.json'),('native','native_result.json'),('engine','engine_result.json'),('overlay','overlay_result.json'),('relations','relations_result.json'),('relation_trace','sortie_relation_trace.json'),('panes','panes_result.json')]:
    results[key]=json.loads((output/file).read_text(encoding='utf-8-sig'))
    check(results[key].get('checks',0)>0,f'Completed focused fixture: {key}')
check(results['overlay'].get('real_d3d11_pixel_readback'),'Actual Direct3D pixel readback and native window input fixture')
record=dict(status='native_personnel_candidate_verified',checks=checks,installed=False,gameplay_verified=False,
            rendering_verified=False,uses_game_native_ui=True,hud_renderer='d3d11_cached_button',installed_unchanged=True,installed_sha256=installed,
            candidate_sha256={name:digest(candidate/name) for name in names},installed_text_equals_candidate=same_text,
            ui_marker_ownership=owners,focused_checks=results,
            sources_sha256={name:digest(project/name) for name in sources})
(output/'candidate_verification.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:record[k] for k in ['status','checks','uses_game_native_ui','installed_unchanged','gameplay_verified']}))
