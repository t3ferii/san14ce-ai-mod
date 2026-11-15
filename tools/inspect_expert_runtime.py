"""Bounded read-only snapshot of expert UI maps in the matching diagnostic DLL."""
from pathlib import Path
import argparse, ctypes as C, hashlib, json, re, struct, sys
from ctypes import wintypes as W
PROJECT=Path(__file__).resolve().parents[1]; GAME=PROJECT.parent
sys.path.insert(0,str(PROJECT/'legacy/FogMod_Source/tools'))
from audit_running_game import MODULEENTRY32W

def main(pid):
    dll=GAME/'SAN14_Convenience.dll'
    diagnostic=PROJECT/'build/development/loader_split_diagnostic/SAN14_Convenience.dll'
    digest=hashlib.sha256(dll.read_bytes()).hexdigest()
    if digest!=hashlib.sha256(diagnostic.read_bytes()).hexdigest():
        diagnostic=PROJECT/'build/development/expert_presets/local4_symbols/SAN14_Convenience.dll'
    assert digest==hashlib.sha256(diagnostic.read_bytes()).hexdigest(),'Diagnostic map does not match installed DLL'
    symbols={}
    for line in diagnostic.with_suffix('.map').read_text().splitlines():
        m=re.search(r'\s(\?\S+)\s+([0-9a-fA-F]{16})\s',line)
        if m:symbols[m[1]]=int(m[2],16)-0x180000000
    def symbol(name):
        hits=[v for k,v in symbols.items() if k.startswith('?'+name+'@@')]
        assert len(hits)==1,(name,hits)
        return hits[0]
    k=C.WinDLL('kernel32',use_last_error=True)
    k.CreateToolhelp32Snapshot.argtypes=[W.DWORD,W.DWORD];k.CreateToolhelp32Snapshot.restype=W.HANDLE
    k.Module32FirstW.argtypes=[W.HANDLE,C.POINTER(MODULEENTRY32W)];k.Module32FirstW.restype=W.BOOL
    k.Module32NextW.argtypes=k.Module32FirstW.argtypes;k.Module32NextW.restype=W.BOOL
    k.CloseHandle.argtypes=[W.HANDLE]
    k.OpenProcess.argtypes=[W.DWORD,W.BOOL,W.DWORD];k.OpenProcess.restype=W.HANDLE
    k.ReadProcessMemory.argtypes=[W.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)];k.ReadProcessMemory.restype=W.BOOL
    modules=[];snap=k.CreateToolhelp32Snapshot(0x18,pid)
    try:
        e=MODULEENTRY32W();e.dwSize=C.sizeof(e)
        assert k.Module32FirstW(snap,C.byref(e))
        while True:
            modules.append(dict(name=e.szModule,path=e.szExePath,base=e.modBaseAddr,size=e.modBaseSize))
            if not k.Module32NextW(snap,C.byref(e)):break
    finally:k.CloseHandle(snap)
    exe=next(m for m in modules if m['name'].lower()=='san14ce_ko.exe')
    mod=next(m for m in modules if m['name'].lower()=='san14_convenience.dll')
    assert Path(exe['path']).resolve()==(GAME/'SAN14CE_KO.exe').resolve()
    assert Path(mod['path']).resolve()==dll.resolve()
    base=exe['base'];mb=mod['base'];h=k.OpenProcess(0x410,False,pid)
    if not h:raise C.WinError(C.get_last_error())
    def read(at,size,optional=False):
        assert 0<size<=0x3000
        buf=C.create_string_buffer(size);got=C.c_size_t()
        if not k.ReadProcessMemory(h,at,buf,size,C.byref(got)) or got.value!=size:
            if optional:return None
            raise C.WinError(C.get_last_error())
        return buf.raw
    def q(at):return struct.unpack('<Q',read(at,8))[0]
    def u(at):return struct.unpack('<I',read(at,4))[0]
    def tree(name,value_size):
        at=mb+symbol(name);head=q(at);count=q(at+8);assert count<=512
        stack=[q(head+8)];seen=set();items=[]
        while stack:
            node=stack.pop()
            if node==head or node in seen:continue
            assert len(seen)<512
            seen.add(node);raw=read(node,40+value_size)
            if raw[25]:continue
            stack.extend(struct.unpack_from('<Q',raw,o)[0] for o in (0,16))
            items.append((struct.unpack_from('<Q',raw,32)[0],raw[40:40+value_size]))
        assert len(items)==count,(name,count,len(items))
        return items
    try:
        hooks=[]
        for slot,original in [(0x141BD18,0x5F12B0),(0x141BCC8,0x5B9110),(0x1429728,0x65EDD0)]:
            target=q(base+slot)
            hooks.append(dict(slot=hex(slot),target=hex(target),module_rva=hex(target-mb),hooked=mb<=target<mb+mod['size'],original=hex(original)))
        factory_bytes=read(base+0x5F14DA,5);assert factory_bytes[0]==0xE8
        target=base+0x5F14DF+struct.unpack_from('<i',factory_bytes,1)[0]
        factory_call=dict(site='0x5f14da',target=hex(target),hooked=target!=base+0x92E430,bytes=factory_bytes.hex())
        panels=[]
        for row,value in tree('expert_preset_panels',16):
            layout,*buttons=struct.unpack('<2Q',value)
            pane=q(row+0x178);graph=pane+0x130 if pane else 0
            entry=dict(row=hex(row),row_vtable=hex(q(row)-base),id=u(row+0x220),layout=hex(layout),pane=hex(pane),buttons=[hex(b) for b in buttons],row_header=read(row,0x180).hex(),pane_header=read(pane,0x178).hex() if pane else None)
            if graph:
                count=u(graph+0x10);entry['node_count']=count
                records=q(graph);descriptors=q(graph+8)
                entry['nodes']=[]
                for index in (4,12,48):
                    if index>=count:continue
                    desc=q(descriptors+index*8)
                    raw=read(desc,0x100,True) if desc else None
                    entry['nodes'].append(dict(index=index,descriptor=hex(desc),record=read(records+index*0x30,0x30).hex(),descriptor_bytes=raw.hex() if raw else None))
            entry['widgets']=[]
            for label,button in [('standard',q(row+0x1A0)),('native_radio0',q(row+0x1C8))]+[(str(i),b) for i,b in enumerate(buttons)]:
                if button:
                    style=q(button+0x188)
                    parts=q(button+0x130)
                    descriptors=q(button+0x138)
                    entry['widgets'].append(dict(label=label,address=hex(button),raw=read(button,0x1A0).hex(),style=read(style,0x20).hex(),style_vtable=[hex(q(q(style)+o)-base) for o in range(0,0x48,8)],resource=read(q(button+0x90),0x90).hex(),parts=read(parts,17*0x30).hex(),descriptors=[dict(index=i,address=hex(q(descriptors+i*8)),raw=(read(q(descriptors+i*8),0x100,True) or b'').hex()) for i in range(17)]))
                    for desc in entry['widgets'][-1]['descriptors']:
                        at=int(desc['address'],16)
                        desc['transforms']=[struct.unpack('<16f',read(q(at+off),64)) if q(at+off) else None for off in (0xE0,0xE8)]
                        desc['tail']=read(at+0x100,0x50).hex()
                        if desc['index'] in (0,3,16):desc['detail']=read(at,0x300).hex()
            panels.append(entry)
        choices=[dict(layout=hex(at),selected=struct.unpack('<2i',value)) for at,value in tree('expert_choices',8)]
        heads=q(base+0x27D10F0);limit=u(base+0x27D1120)
        def listing(at):
            handle_ptr=q(at+8);handle=u(handle_ptr) if handle_ptr else limit
            values=[];link=q(heads+handle*8) if handle<limit else 0;seen=set()
            while link and link not in seen:
                assert len(seen)<10000
                seen.add(link);values.append(hex(q(link)));link=q(link+8)
            return dict(handle=handle,values=values)
        buckets=[listing(base+0x1B62110+i*16) for i in range(21)]
        registered=[v for bucket in buckets for v in bucket['values']]
        for panel in panels:
            panel['pane_layers']=[listing(q(int(panel['pane'],16)+0x160+i*8)) if q(int(panel['pane'],16)+0x160+i*8) else None for i in range(3)]
            for widget in panel['widgets']:widget['registered']=widget['address'] in registered
        report=dict(pid=pid,memory_access='read only',dll_sha256=digest,modules=modules,hooks=hooks,factory_call=factory_call,panels=panels,choices=choices,registered=registered,buckets=buckets,manager_raw=read(base+0x1B62110,0x200).hex(),pool_raw=read(base+0x27D10B0,0xA0).hex())
        out=PROJECT/'data/diagnostics/expert_preset_crash_20261024'/f'runtime_{pid}.json'
        out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(dict(pid=pid,memory_access='read only',hooks=hooks,factory_call=factory_call,panels=[{k:v for k,v in p.items() if k not in ('nodes','widgets','row_header','pane_header','pane_layers')} for p in panels],choices=choices,report=str(out)),ensure_ascii=False))
    finally:k.CloseHandle(h)

if __name__=='__main__':
    sys.stdout.reconfigure(encoding='utf-8');parser=argparse.ArgumentParser();parser.add_argument('--pid',type=int,required=True)
    main(parser.parse_args().pid)
