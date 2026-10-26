"""Read module identities and published hooks only, with QUERY/VM_READ rights."""
import argparse,ctypes as C,json,struct,sys
import hashlib
from ctypes import wintypes as W
from pathlib import Path
from datetime import datetime,timezone
from package_mod import atomic_write
PROJECT=Path(__file__).resolve().parents[1];GAME=PROJECT.parent
sys.path.insert(0,str(PROJECT/'legacy/FogMod_Source/tools'))
from audit_running_game import MODULEENTRY32W

def main():
    p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);args=p.parse_args()
    k=C.WinDLL('kernel32',use_last_error=True)
    k.CreateToolhelp32Snapshot.argtypes=[W.DWORD,W.DWORD];k.CreateToolhelp32Snapshot.restype=W.HANDLE
    k.CloseHandle.argtypes=[W.HANDLE]
    for name in ('Module32FirstW','Module32NextW'):
        f=getattr(k,name);f.argtypes=[W.HANDLE,C.POINTER(MODULEENTRY32W)];f.restype=W.BOOL
    k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];k.OpenProcess.restype=W.HANDLE
    k.ReadProcessMemory.argtypes=[W.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)];k.ReadProcessMemory.restype=W.BOOL
    snapshot=k.CreateToolhelp32Snapshot(0x18,args.pid);modules=[]
    if snapshot==C.c_void_p(-1).value:raise C.WinError(C.get_last_error())
    try:
        entry=MODULEENTRY32W();entry.dwSize=C.sizeof(entry)
        if not k.Module32FirstW(snapshot,C.byref(entry)):raise C.WinError(C.get_last_error())
        while True:
            modules.append({'name':entry.szModule,'path':entry.szExePath,'base':entry.modBaseAddr,'size':entry.modBaseSize})
            if not k.Module32NextW(snapshot,C.byref(entry)):break
    finally:k.CloseHandle(snapshot)
    names=('SAN14CE_KO.exe','dinput8.dll','SAN14_CombatAI.dll','SAN14_Convenience.dll')
    owned={}
    for name in names:
        found=[m for m in modules if Path(m['path']).resolve()==(GAME/name).resolve()]
        if len(found)==1:owned[name]=found[0]
    checks=[];handle=k.OpenProcess(0x0400|0x0010,False,args.pid)
    if not handle:raise C.WinError(C.get_last_error())
    def read(address,size):
        assert 0x10000<=address<0x800000000000 and 0<size<=64
        buffer=C.create_string_buffer(size);count=C.c_size_t()
        if not k.ReadProcessMemory(handle,address,buffer,size,C.byref(count)) or count.value!=size:raise C.WinError(C.get_last_error())
        return buffer.raw
    def owner(address):
        return next((m['name'] for m in owned.values() if m['base']<=address<m['base']+m['size']),None)
    try:
        if len(owned)==4:
            base=owned['SAN14CE_KO.exe']['base']
            for slot in (0x1344518,0x1343C98,0x134B4F8,0x134AF00,0x1346E70,0x134C8F8):
                target=struct.unpack('<Q',read(base+slot,8))[0]
                expected='SAN14_CombatAI.dll' if slot in (0x1344518,0x1343C98,0x134B4F8,0x134AF00) else 'SAN14_Convenience.dll'
                checks.append({'slot':hex(slot),'owner':owner(target),'expected':expected,'passed':owner(target)==expected})
            for rva in (0x48CE0,0x46B80,0x4BA50,0x4ABE0,0x44A80,0x1EC890,0x2B0290,0x1843E0,0x1749A0,0x279190):
                code=read(base+rva,5);target_owner=None
                if code[0]==0xE9:
                    relay=base+rva+5+struct.unpack_from('<i',code,1)[0];jump=read(relay,14)
                    if jump[:6]==b'\xff\x25\0\0\0\0':target_owner=owner(struct.unpack_from('<Q',jump,6)[0])
                checks.append({'entry':hex(rva),'owner':target_owner,'passed':target_owner=='SAN14_CombatAI.dll'})
    finally:k.CloseHandle(handle)
    verified=len(owned)==4 and len(checks)==16 and all(c['passed'] for c in checks)
    result={'status':'combat_merge_startup_verified' if verified else 'combat_merge_startup_pending',
            'time':datetime.now(timezone.utc).isoformat(),'pid':args.pid,'modules':owned,
            'checks':checks,'startup_verified':verified,'gameplay_verified':False,'memory_writes':False}
    state=json.loads((PROJECT/'backup/input_state.json').read_text(encoding='utf-8-sig'))
    result['release_version']=state.get('release_version')
    result['files']={name:hashlib.sha256((GAME/name).read_bytes()).hexdigest() for name in names[1:] if (GAME/name).exists()}
    destination=PROJECT/'data/diagnostics/combat_merge_startup.json';destination.parent.mkdir(parents=True,exist_ok=True)
    atomic_write(destination,(json.dumps(result,indent=2)+'\n').encode('utf-8'))
    print(json.dumps({'status':result['status'],'modules':list(owned),'checks':len(checks),'startup_verified':verified}))
if __name__=='__main__':main()
