"""Current-file ABI checks; optional bounded PROCESS_VM_READ observation only."""
from pathlib import Path
import argparse, ctypes as C, hashlib, json, re, struct, sys
from ctypes import wintypes as W
P=Path(__file__).resolve().parents[1]; GAME=P.parent
sys.path.insert(0,str(P/'research/ui_atlas'))
from build_catalog import PE, SOURCE, EXPECTED

def digest(data): return hashlib.sha256(data).hexdigest()

def verify_live(pid,pe,profile):
    sys.path.insert(0,str(P/'legacy/FogMod_Source/tools'))
    from audit_running_game import MODULEENTRY32W
    k=C.WinDLL('kernel32',use_last_error=True)
    k.CreateToolhelp32Snapshot.argtypes=[W.DWORD,W.DWORD];k.CreateToolhelp32Snapshot.restype=W.HANDLE
    k.Module32FirstW.argtypes=[W.HANDLE,C.POINTER(MODULEENTRY32W)];k.Module32FirstW.restype=W.BOOL
    k.Module32NextW.argtypes=k.Module32FirstW.argtypes;k.Module32NextW.restype=W.BOOL
    k.CloseHandle.argtypes=[W.HANDLE]
    k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];k.OpenProcess.restype=W.HANDLE
    k.ReadProcessMemory.argtypes=[W.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)];k.ReadProcessMemory.restype=W.BOOL
    snap=k.CreateToolhelp32Snapshot(0x18,pid);assert snap!=C.c_void_p(-1).value
    entry=MODULEENTRY32W();entry.dwSize=C.sizeof(entry)
    try:
        assert k.Module32FirstW(snap,C.byref(entry))
        while entry.szModule.lower()!='san14ce_ko.exe':
            assert k.Module32NextW(snap,C.byref(entry)),'Game module not found'
        assert Path(entry.szExePath).resolve()==(GAME/'SAN14CE_KO.exe').resolve()
        base=entry.modBaseAddr
    finally:k.CloseHandle(snap)
    handle=k.OpenProcess(0x410,False,pid);assert handle,'Read-only game access unavailable'
    def read(addr,n):
        b=C.create_string_buffer(n);count=C.c_size_t()
        assert k.ReadProcessMemory(handle,addr,b,n,C.byref(count)) and count.value==n,C.get_last_error()
        return b.raw
    try:
        for row in profile:
            assert read(base+row['rva'],row['size'])==pe.read(row['rva'],row['size']),hex(row['rva'])
        for slot,expected in [(0x139A338,0x1A7F50),(0x139A350,0x1A74D0),(0x139A328,0x19B1D0),(0x13999C8,0x1A7EA0),(0x13999E0,0x1A75A0),(0x13999B8,0x19B350)]:
            assert struct.unpack('<Q',read(base+slot,8))[0]==base+expected,hex(slot)
        mode=struct.unpack('<I',read(base+0x21431FC,4))[0]
        return dict(pid=pid,function_ranges=len(profile),original_slots=6,map_mode=mode,
                    access='PROCESS_VM_READ|PROCESS_QUERY_INFORMATION',memory_written=False,game_functions_invoked=False)
    finally:k.CloseHandle(handle)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--pid',type=int);args=parser.parse_args()
    pe=PE(SOURCE);assert digest(pe.data)==EXPECTED
    assert digest((GAME/'SAN14CE_KO.exe').read_bytes())=='87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a'
    profile=json.loads((P/'research/area_officer_overlay/profile.json').read_text())['functions']
    header=(P/'src/region_officer_profile.h').read_text()
    for row in profile:
        raw=pe.read(row['rva'],row['size']);assert digest(raw)==row['sha256']
        h=0xCBF29CE484222325
        for byte in raw:h=((h^byte)*0x100000001B3)&((1<<64)-1)
        assert h==row['hash']
        assert f'{{0x{row["rva"]:X},0x{row["size"]:X},0x{h:016X}ull}}' in header
    # Native CCapitalInfo data getter and area ownership, separately from fixtures.
    assert bytes.fromhex('488B89F0010000') in pe.read(0x19E140,32)
    assert pe.read(0x22CC1C,5)==bytes.fromhex('0FB64114C3')
    # City label (CFootholdInfoLayout): +1F0 object +4E base id -> building +10 centre (22D500), tile +12 area (22A7B0).
    assert pe.read(0x22D506,4)==bytes.fromhex('0FB7414E') and bytes.fromhex('98DA0600') in pe.read(0x22D500,0x30)
    assert pe.read(0x1A3507,7)==bytes.fromhex('488D1DE2FCF901')
    assert 0x1A3507+7+struct.unpack('<i',pe.read(0x1A350A,4))[0]==0x21431F0
    for slot,rva in [(0x139A338,0x1A7F50),(0x139A350,0x1A74D0),(0x139A328,0x19B1D0),(0x13999C8,0x1A7EA0),(0x13999E0,0x1A75A0),(0x13999B8,0x19B350)]:
        assert struct.unpack('<Q',pe.read(slot,8))[0]==pe.base+rva
    source='\n'.join((P/n).read_text(encoding='utf-8') for n in ['src/native_region_officers.inl','src/native_region_officer_traits.inl'])
    calls={int(x,16) for x in re.findall(r'\(0x([A-Fa-f0-9]+)\)\(',source)}
    assert not calls & {0x29BF00,0x27A660,0x199950,0x198FA0,0x36320,0x7A110},'Domestic mutation/button route is forbidden'
    assert 'CDomesticAreaBtnBase' not in source.split('namespace region_officers',1)[1]
    assert pe.read(0x34143D,7)==bytes.fromhex('0FB681BA000000')
    assert pe.read(0x341445,7)==bytes.fromhex('0FB681B6000000')
    assert struct.unpack('<4I',pe.read(0x13B3A40,16))==(33,34,36,37)
    assert struct.unpack('<Q',pe.read(0x1A2B850+38*96+24,8))[0]==pe.base+0x361590
    # v12.19: QPC only for the heavy-step cooldown (no timing output); still no writer.
    assert 'heavy_resume=end+2*(end-begin)' in source and 'if(heavy_clock()<heavy_resume)return false;' in source
    for removed in ('metrics::','observe_text','region_officers_once.json','region_officers_profile.ini','WriteFile','CreateFileW'):
        assert removed not in source,removed
    assert not (P/'src/native_region_officer_metrics.inl').exists()
    adapter=(P/'src/native_adapter.cpp').read_text()
    section=adapter.split('#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE)',1)[1].split('#endif',1)[0]
    assert '#include "native_region_officers.inl"' in section
    report=dict(status='region_officer_current_exe_passed',function_ranges=len(profile),original_slots=6,
                original_exe_unchanged=True,convenience_only=True,installed=False,gameplay_verified=False,fps_measured=False)
    if args.pid:report['live_byte_observation']=verify_live(args.pid,pe,profile)
    output=P/'build/development/region_officers';output.mkdir(parents=True,exist_ok=True)
    (output/('live-bytes.json' if args.pid else 'native-bytes.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))
if __name__=='__main__':main()
