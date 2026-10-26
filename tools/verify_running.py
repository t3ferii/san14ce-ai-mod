"""Read-only confirmation of automatic module loading and installed links.
Does not issue orders or write process memory; gameplay remains a separate check.
"""
import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import argparse, json, sys, struct, hashlib
PROJECT=Path(__file__).resolve().parents[1]; GAME=PROJECT.parent
sys.path.insert(0,str(GAME/'AI_TacticsMod/legacy/FogMod_Source/tools'))
from audit_running_game import MODULEENTRY32W

def main(pid):
    kernel=C.WinDLL('kernel32',use_last_error=True)
    kernel.CreateToolhelp32Snapshot.argtypes=[W.DWORD,W.DWORD]; kernel.CreateToolhelp32Snapshot.restype=W.HANDLE
    kernel.Module32FirstW.argtypes=[W.HANDLE,C.POINTER(MODULEENTRY32W)];kernel.Module32FirstW.restype=W.BOOL
    kernel.Module32NextW.argtypes=kernel.Module32FirstW.argtypes;kernel.Module32NextW.restype=W.BOOL
    kernel.CloseHandle.argtypes=[W.HANDLE];kernel.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];kernel.OpenProcess.restype=W.HANDLE
    kernel.ReadProcessMemory.argtypes=[W.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)];kernel.ReadProcessMemory.restype=W.BOOL
    snapshot=kernel.CreateToolhelp32Snapshot(0x18,pid)
    if snapshot==C.c_void_p(-1).value:raise C.WinError(C.get_last_error())
    modules=[]
    try:
        entry=MODULEENTRY32W();entry.dwSize=C.sizeof(entry)
        if not kernel.Module32FirstW(snapshot,C.byref(entry)):raise C.WinError(C.get_last_error())
        while True:
            modules.append(dict(name=entry.szModule,path=entry.szExePath,base=entry.modBaseAddr,size=entry.modBaseSize))
            if not kernel.Module32NextW(snapshot,C.byref(entry)):break
    finally:kernel.CloseHandle(snapshot)
    exe=next(m for m in modules if m['name'].lower()=='san14ce_ko.exe')
    mod=next(m for m in modules if m['name'].lower()=='dinput8.dll' and Path(m['path']).resolve()==(GAME/'dinput8.dll').resolve())
    assert Path(exe['path']).resolve()==(GAME/'SAN14CE_KO.exe').resolve()
    assert Path(mod['path']).resolve()==(GAME/'dinput8.dll').resolve()
    handle=kernel.OpenProcess(0x410,False,pid)
    if not handle:raise C.WinError(C.get_last_error())
    def read(addr,size):
        buffer=C.create_string_buffer(size);got=C.c_size_t()
        if not kernel.ReadProcessMemory(handle,addr,buffer,size,C.byref(got)):raise C.WinError(C.get_last_error())
        assert got.value==size;return buffer.raw
    def q(addr):return struct.unpack('<Q',read(addr,8))[0]
    try:
        slots=[]
        for rva in [0x1344518,0x1343C98,0x134B4F8]:
            address=q(exe['base']+rva)
            assert mod['base']<=address<mod['base']+mod['size']
            slots.append(dict(slot_rva=hex(rva),dll_callback_rva=hex(address-mod['base'])))
        retreat=q(exe['base']+0x134AF00)
        campaign_hook=mod['base']<=retreat<mod['base']+mod['size']
        if campaign_hook:slots.append(dict(slot_rva='0x134af00',dll_callback_rva=hex(retreat-mod['base'])))
        else:assert retreat==exe['base']+0x61120,'Unexpected retreat node modification'
        dll_sha=hashlib.sha256(Path(mod['path']).read_bytes()).hexdigest()
        manifest=json.loads((PROJECT/'build/input_package/manifest.json').read_text())
        if manifest.get('behavior') in ['campaign_v3','campaign_v4'] and dll_sha==manifest['dll_sha256']:
            assert campaign_hook,'Campaign candidate missing construction return hook'
        call=read(exe['base']+0xD2486,5);assert call[0]==0xE8
        relay=exe['base']+0xD2486+5+struct.unpack_from('<i',call,1)[0]
        jump=read(relay,14);assert jump[:6]==b'\xff\x25\0\0\0\0'
        callback=struct.unpack_from('<Q',jump,6)[0];assert mod['base']<=callback<mod['base']+mod['size']
        live=bytearray(read(exe['base']+0x1000,20179374))
        captured=(GAME/'AI_TacticsMod/legacy/FogMod_Source/native_game_analysis/running_code_text.bin').read_bytes()
        start=0xD2486-0x1000;live[start:start+5]=captured[start:start+5]
        adjacency_links=[]
        if live[0x2B07C0-0x1000]==0xE9:
            for rva,opcode in [(0x169F44,0xE8),(0x2B07C0,0xE9)]:
                patch=read(exe['base']+rva,5);assert patch[0]==opcode
                relay2=exe['base']+rva+5+struct.unpack_from('<i',patch,1)[0]
                jump2=read(relay2,14);assert jump2[:6]==b'\xff\x25\0\0\0\0'
                callback2=struct.unpack_from('<Q',jump2,6)[0];assert mod['base']<=callback2<mod['base']+mod['size']
                adjacency_links.append(dict(site_rva=hex(rva),relay=hex(relay2),callback_rva=hex(callback2-mod['base'])))
                off=rva-0x1000;live[off:off+5]=captured[off:off+5]
            relay_base=int(adjacency_links[0]['relay'],16)
            trampoline=read(relay_base+64,19)
            assert trampoline[:5]==bytes.fromhex('4053554157') and trampoline[5:11]==b'\xff\x25\0\0\0\0'
            assert struct.unpack_from('<Q',trampoline,11)[0]==exe['base']+0x2B07C5
        if manifest.get('behavior')=='campaign_v4' and dll_sha==manifest['dll_sha256']:
            assert len(adjacency_links)==2,'Missing actual combat adjacency links'
        wall_link=None
        if live[0x279190-0x1000]==0xE9:
            wall_patch=read(exe['base']+0x279190,5)
            wall_relay=exe['base']+0x279195+struct.unpack_from('<i',wall_patch,1)[0]
            wall_jump=read(wall_relay,14)
            assert wall_jump[:6]==b'\xff\x25\0\0\0\0'
            wall_callback=struct.unpack_from('<Q',wall_jump,6)[0]
            assert mod['base']<=wall_callback<mod['base']+mod['size']
            wall_trampoline=read(wall_relay+32,19)
            assert wall_trampoline[:5]==bytes.fromhex('48896c2410') and wall_trampoline[5:11]==b'\xff\x25\0\0\0\0'
            assert struct.unpack_from('<Q',wall_trampoline,11)[0]==exe['base']+0x279195
            wall_link=dict(site_rva='0x279190',relay=hex(wall_relay),callback_rva=hex(wall_callback-mod['base']))
            off=0x279190-0x1000;live[off:off+5]=captured[off:off+5]
        if manifest.get('computer_wall_building')=='blocked' and dll_sha==manifest['dll_sha256']:
            assert wall_link,'Candidate missing native wall build prohibition hook'
        domestic_links=[]
        domestic_verified=False
        if live[0x295160-0x1000]==0xE9:
            for rva,original in [(0x1346E70,0x36320),(0x134C8F8,0x7A110)]:
                callback=q(exe['base']+rva)
                assert mod['base']<=callback<mod['base']+mod['size']
                slots.append(dict(slot_rva=hex(rva),dll_callback_rva=hex(callback-mod['base']),original_rva=hex(original)))
            for rva,opcode,size in [(0x295160,0xE9,5),(0x1ED475,0xE8,6)]:
                patch=read(exe['base']+rva,size);assert patch[0]==opcode
                relay3=exe['base']+rva+5+struct.unpack_from('<i',patch,1)[0]
                if size==5:
                    jump3=read(relay3,14);assert jump3[:6]==b'\xff\x25\0\0\0\0'
                    callback3=struct.unpack_from('<Q',jump3,6)[0]
                    assert mod['base']<=callback3<mod['base']+mod['size']
                    trampoline3=read(relay3+32,19)
                    assert trampoline3[:5]==bytes.fromhex('4c89442418')
                    assert trampoline3[5:11]==b'\xff\x25\0\0\0\0'
                    assert struct.unpack_from('<Q',trampoline3,11)[0]==exe['base']+0x295165
                else:
                    assert patch[5]==0x90
                    bridge=read(relay3,19+36+14+36+20)
                    assert bridge[:19]==bytes.fromhex('9c50515241504151415241534881ec88000000')
                    assert bridge[55:57]==b'\x48\xb8'
                    cost_callback=struct.unpack_from('<Q',bridge,57)[0]
                    assert mod['base']<=cost_callback<mod['base']+mod['size']
                    assert bridge[65:69]==bytes.fromhex('ffd089c3')
                    assert bridge[-20:]==bytes.fromhex('4881c488000000415b415a415941585a59589dc3')
                domestic_links.append(dict(site_rva=hex(rva),relay=hex(relay3),bytes=size))
                off=rva-0x1000;live[off:off+size]=captured[off:off+size]
            assert read(exe['base']+0x36423,6)==b'\x90'*6
            off=0x36423-0x1000;live[off:off+6]=captured[off:off+6]
            domestic_links.append(dict(site_rva='0x36423',bytes=6,kind='automatic_node_budget_guard_only'))
            domestic_verified=True
        if manifest.get('automatic_domestic') and dll_sha==manifest['dll_sha256']:
            assert domestic_verified,'Candidate missing automatic domestic links'
        assert bytes(live)==captured,'Unexpected code modification outside the cut callback'
        root=q(exe['base']+0x277DF18)
        report=dict(status='automatic_native_hooks_verified',pid=pid,exe_base=hex(exe['base']),
                    dll_base=hex(mod['base']),links=slots,cut_predicate_relay=hex(relay),
                    dll_file_sha256=dll_sha,campaign_return_hook_verified=campaign_hook,
                    adjacency_links=adjacency_links,contextual_adjacency_hook_verified=len(adjacency_links)==2,
                    unrelated_code_preserved=True,root=hex(root),gameplay_validated=False,
                    wall_build_permission_link=wall_link,computer_wall_build_hook_verified=wall_link is not None,
                    domestic_links=domestic_links,automatic_domestic_hooks_verified=domestic_verified)
        (PROJECT/'build/startup_verification.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
    finally:kernel.CloseHandle(handle)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--pid',type=int,required=True);args=parser.parse_args();main(args.pid)
