"""Read-only (PROCESS_VM_READ) probe for unit-detail options of one field army.

  python -I tools/army_option_probe.py snap <무장이름> <label>   save that army's bytes as <label>
  python -I tools/army_option_probe.py diff <labelA> <labelB>    show bytes that differ (army +0..+1FF)

Used to map 부대상세 options (적 접근 시·자주퇴각·추격·사후명령) by changing one option in the game
between two snapshots. Never writes game memory and never calls game functions. Snapshots are kept
in build/development/army_option_probe/."""
import ctypes as C, json, struct, sys
from ctypes import wintypes as WT
from pathlib import Path
P=Path(__file__).resolve().parents[1];OUT=P/'build/development/army_option_probe'
k=C.WinDLL('kernel32',use_last_error=True)
class ME(C.Structure):
    _fields_=[('dwSize',WT.DWORD),('th32ModuleID',WT.DWORD),('th32ProcessID',WT.DWORD),('GlblcntUsage',WT.DWORD),('ProccntUsage',WT.DWORD),
              ('modBaseAddr',C.c_void_p),('modBaseSize',WT.DWORD),('hModule',WT.HMODULE),('szModule',C.c_wchar*256),('szExePath',C.c_wchar*260)]
class PE32(C.Structure):
    _fields_=[('dwSize',WT.DWORD),('cntUsage',WT.DWORD),('th32ProcessID',WT.DWORD),('th32DefaultHeapID',C.c_void_p),('th32ModuleID',WT.DWORD),
              ('cntThreads',WT.DWORD),('th32ParentProcessID',WT.DWORD),('pcPriClassBase',C.c_long),('dwFlags',WT.DWORD),('szExeFile',C.c_wchar*260)]
k.CreateToolhelp32Snapshot.restype=WT.HANDLE;k.OpenProcess.restype=WT.HANDLE
k.ReadProcessMemory.argtypes=[WT.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)]
def game_pid():
    s=k.CreateToolhelp32Snapshot(2,0);e=PE32();e.dwSize=C.sizeof(e);ok=k.Process32FirstW(WT.HANDLE(s),C.byref(e))
    try:
        while ok:
            if e.szExeFile.lower()=='san14ce_ko.exe':return e.th32ProcessID
            ok=k.Process32NextW(WT.HANDLE(s),C.byref(e))
    finally:k.CloseHandle(WT.HANDLE(s))
    raise SystemExit('게임(SAN14CE_KO.exe)이 실행 중이 아닙니다.')
def attach():
    pid=game_pid();s=k.CreateToolhelp32Snapshot(0x18,pid);e=ME();e.dwSize=C.sizeof(e)
    assert k.Module32FirstW(WT.HANDLE(s),C.byref(e))
    while e.szModule.lower()!='san14ce_ko.exe':assert k.Module32NextW(WT.HANDLE(s),C.byref(e))
    base=e.modBaseAddr;k.CloseHandle(WT.HANDLE(s))
    h=k.OpenProcess(0x410,False,pid);assert h,'읽기 권한을 얻지 못했습니다.'
    def rd(a,n):
        b=C.create_string_buffer(n);c=C.c_size_t()
        return b.raw if k.ReadProcessMemory(WT.HANDLE(h),C.c_void_p(a),b,n,C.byref(c)) else None
    return base,rd
def main():
    if sys.argv[1]=='diff':
        a=json.loads((OUT/(sys.argv[2]+'.json')).read_text(encoding='utf-8'));b=json.loads((OUT/(sys.argv[3]+'.json')).read_text(encoding='utf-8'))
        x,y=bytes.fromhex(a['army']),bytes.fromhex(b['army'])
        print(json.dumps({'army':a['army_id'],'name':a['name'],'changed':[{'offset':hex(i),'from':x[i],'to':y[i]} for i in range(len(x)) if x[i]!=y[i]]},ensure_ascii=False))
        return
    name,label=sys.argv[2],sys.argv[3]
    base,rd=attach();q=lambda a:struct.unpack('<Q',rd(a,8))[0];world=q(base+0x277DF18)
    def person(i):
        p=q(world+0x158+i*8)
        if not p:return ''
        f=rd(p+0x34,40).decode('utf-16-le','replace').split('\0')[0];c=rd(p+0x2E,6).decode('utf-16-le','replace').split('\0')[0]
        return f[:len(f)-len(c)] if c and f.endswith(c) and len(c)<len(f) else f
    hits=[]
    for i in range(1,501):
        a=q(world+0x7E838+i*8)
        if a and person(struct.unpack('<H',rd(a+0x12,2))[0])==name:hits.append((i,a))
    if len(hits)!=1:raise SystemExit(f'{name} 부대를 하나로 찾지 못했습니다({len(hits)}개).')
    i,a=hits[0];OUT.mkdir(parents=True,exist_ok=True)
    (OUT/(label+'.json')).write_text(json.dumps({'name':name,'army_id':i,'army':rd(a,0x200).hex()}),encoding='utf-8')
    b=rd(a,0x60)
    print(json.dumps({'saved':label,'army':i,'name':name,'+40':b[0x40],'+41':b[0x41],'+26':b[0x26],'+27':b[0x27]},ensure_ascii=False))
if __name__=='__main__':main()
