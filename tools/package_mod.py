"""Replace the existing graphics DLL import name, preserving original headers.
No captured/decrypted code is written to the executable. Exact version only.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import sys
import ctypes

PROJECT = Path(__file__).resolve().parents[1]
GAME = PROJECT.parent
sys.path.insert(0, str(GAME / 'AI_TacticsMod/legacy/FogMod_Source/tools'))
from audit_native_game import PEImage
ORIGINAL_SHA = '87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a'
DLL = 'AI14.dll'

def sha(data): return hashlib.sha256(data).hexdigest()
def align(n, a): return (n + a - 1) // a * a

def imported_dlls(pe):
    header = struct.unpack_from('<I', pe.data, 0x3c)[0] + 24
    rva, size = struct.unpack_from('<II', pe.data, header + 120)
    descriptors, names = [], []
    for offset in range(0, size, 20):
        descriptor = pe.read(rva + offset, 20)
        if descriptor == bytes(20): break
        descriptors.append(descriptor)
        name_rva = struct.unpack_from('<I', descriptor, 12)[0]
        names.append(pe.read(name_rva, 128).split(b'\0')[0].decode('ascii'))
    else: raise ValueError('Import table has no terminator')
    return descriptors, names

def patch_import(source, destination, expected_sha):
    pe = PEImage(source)
    if sha(pe.data) != expected_sha: raise ValueError('Input version/hash mismatch')
    if pe.machine != 0x8664: raise ValueError('Expected AMD64')
    descriptors, names = imported_dlls(pe)
    if DLL.lower() in [name.lower() for name in names]: raise ValueError('DLL already imported')
    data = bytearray(pe.data)
    nt = struct.unpack_from('<I', data, 0x3c)[0]
    optional = nt + 24
    count = struct.unpack_from('<H', data, nt + 6)[0]
    optional_size = struct.unpack_from('<H', data, nt + 20)[0]
    new_header = optional + optional_size + count * 40
    first_raw = min(s['file_offset'] for s in pe.sections if s['raw_size'])
    if new_header + 40 > first_raw or any(data[new_header:new_header+40]):
        raise ValueError('No unused section header space')
    section_alignment, file_alignment = struct.unpack_from('<II', data, optional + 32)
    rva = align(max(s['rva'] + max(s['virtual_size'], s['raw_size']) for s in pe.sections), section_alignment)
    raw = align(len(data), file_alignment)
    import_bytes = (len(descriptors) + 2) * 20
    payload = bytearray(import_bytes)
    for i, descriptor in enumerate(descriptors): payload[i*20:(i+1)*20] = descriptor
    name = len(payload); payload.extend(DLL.encode() + b'\0')
    hint = align(len(payload), 2); payload.extend(bytes(hint-len(payload))); payload.extend(b'\0\0AIInitialize\0')
    ilt = align(len(payload), 8); payload.extend(bytes(ilt-len(payload))); payload.extend(struct.pack('<QQ', rva + hint, 0))
    iat = len(payload); payload.extend(struct.pack('<QQ', rva + hint, 0))
    struct.pack_into('<IIIII', payload, len(descriptors)*20, rva+ilt, 0, 0, rva+name, rva+iat)
    raw_size = align(len(payload), file_alignment)
    data.extend(bytes(raw-len(data))); data.extend(payload); data.extend(bytes(raw_size-len(payload)))
    data[new_header:new_header+40] = struct.pack('<8sIIIIIIHHI', b'.aitmod\0',len(payload),rva,raw_size,raw,0,0,0,0,0xC0000040)
    struct.pack_into('<H', data, nt+6, count+1)
    struct.pack_into('<I', data, optional+56, align(rva+len(payload),section_alignment))
    struct.pack_into('<I', data, optional+60, align(new_header+40,file_alignment))
    struct.pack_into('<II', data, optional+120, rva,import_bytes)
    struct.pack_into('<I', data, optional+8, struct.unpack_from('<I',data,optional+8)[0]+raw_size)
    struct.pack_into('<I', data, optional+64,0)
    # Recompute the normal Windows PE checksum, without changing certificate bytes.
    imagehlp = ctypes.WinDLL('imagehlp')
    imagehlp.CheckSumMappedFile.argtypes = [ctypes.c_void_p,ctypes.c_uint32,ctypes.POINTER(ctypes.c_uint32),ctypes.POINTER(ctypes.c_uint32)]
    imagehlp.CheckSumMappedFile.restype = ctypes.c_void_p
    buffer = (ctypes.c_ubyte*len(data)).from_buffer(data)
    old, checksum = ctypes.c_uint32(),ctypes.c_uint32()
    if not imagehlp.CheckSumMappedFile(buffer,len(data),ctypes.byref(old),ctypes.byref(checksum)): raise OSError('PE checksum failed')
    struct.pack_into('<I',data,optional+64,checksum.value)
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_bytes(data)
    modified = PEImage(destination)
    _, modified_names = imported_dlls(modified)
    assert modified_names == names + [DLL]
    for section in pe.sections:
        start=section['file_offset']; end=start+section['raw_size']
        assert data[start:end] == pe.data[start:end], section['name']
    for directory in [0,2,3,4,5,9,10,12]:
        start=optional+112+directory*8
        assert data[start:start+8] == pe.data[start:start+8]
    assert data[optional+16:optional+20] == pe.data[optional+16:optional+20]
    assert data[first_raw:len(pe.data)] == pe.data[first_raw:]
    return {'source_sha256':expected_sha,'patched_sha256':sha(data),'original_entrypoint_preserved':True,
            'original_section_bytes_preserved':True,'original_overlay_preserved':True,
            'dll_import':DLL,'imports':modified_names,'text_decryption_or_drm_bypass':False}

def patch_existing_graphics_import(source,destination,expected_sha=ORIGINAL_SHA):
    """Change only the existing DLL name, without growing the protected PE.
    AI14 forwards the original D3D11CreateDevice export to D3D11.dll, preserving
    the user's existing graphics proxy and all original import slots.
    """
    pe=PEImage(source)
    if sha(pe.data)!=expected_sha:raise ValueError('Original executable version differs')
    descriptors,names=imported_dlls(pe)
    index=next(i for i,name in enumerate(names) if name.lower()=='d3d11.dll')
    descriptor=descriptors[index];oft,stamp,chain,name_rva,iat=struct.unpack('<IIIII',descriptor)
    imported=[];at=oft or iat
    while True:
        thunk=struct.unpack('<Q',pe.read(at,8))[0]
        if not thunk:break
        if thunk>>63:raise ValueError('Unexpected ordinal graphics import')
        imported.append(pe.read(thunk+2,80).split(b'\0')[0].decode('ascii'));at+=8
    if imported!=['D3D11CreateDevice']:raise ValueError('Unexpected graphics exports; forwarder not compatible')
    offset,_=pe.offset(name_rva,10)
    assert pe.data[offset:offset+10].lower()==b'd3d11.dll\0'
    data=bytearray(pe.data);data[offset:offset+10]=DLL.encode().ljust(10,b'\0')
    destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(data)
    modified=PEImage(destination);_,newnames=imported_dlls(modified)
    expected_names=names.copy();expected_names[index]=DLL
    assert newnames==expected_names
    differences=[i for i,(old,new) in enumerate(zip(pe.data,data)) if old!=new]
    assert len(data)==len(pe.data) and all(offset<=i<offset+10 for i in differences)
    return dict(source_sha256=expected_sha,patched_sha256=sha(data),dll_import=DLL,imports=newnames,
                original_entrypoint_preserved=True,original_headers_preserved=True,
                original_code_preserved=True,original_import_slots_preserved=True,original_overlay_preserved=True,
                patch_offset=offset,patch_original=pe.data[offset:offset+10].hex(),patch_replacement=data[offset:offset+10].hex(),
                changed_bytes=len(differences),original_graphics_provider='D3D11.dll',
                graphics_forwarded_exports=imported,text_decryption_or_drm_bypass=False)

def no_game():
    from ctypes import wintypes as W
    class ProcessEntry(ctypes.Structure):
        _fields_=[('size',W.DWORD),('usage',W.DWORD),('pid',W.DWORD),('heap',ctypes.c_size_t),
                  ('module',W.DWORD),('threads',W.DWORD),('parent',W.DWORD),('priority',W.LONG),
                  ('flags',W.DWORD),('name',W.WCHAR*260)]
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateToolhelp32Snapshot.argtypes=[W.DWORD,W.DWORD];kernel.CreateToolhelp32Snapshot.restype=W.HANDLE
    kernel.Process32FirstW.argtypes=[W.HANDLE,ctypes.POINTER(ProcessEntry)];kernel.Process32FirstW.restype=W.BOOL
    kernel.Process32NextW.argtypes=kernel.Process32FirstW.argtypes;kernel.Process32NextW.restype=W.BOOL
    kernel.CloseHandle.argtypes=[W.HANDLE]
    snapshot=kernel.CreateToolhelp32Snapshot(2,0)
    if snapshot==ctypes.c_void_p(-1).value:raise ctypes.WinError(ctypes.get_last_error())
    try:
        entry=ProcessEntry();entry.size=ctypes.sizeof(entry)
        if not kernel.Process32FirstW(snapshot,ctypes.byref(entry)):raise ctypes.WinError(ctypes.get_last_error())
        while True:
            if entry.name.lower()=='san14ce_ko.exe':raise RuntimeError('Close the game before changing executable files.')
            if not kernel.Process32NextW(snapshot,ctypes.byref(entry)):
                if ctypes.get_last_error()!=18:raise ctypes.WinError(ctypes.get_last_error())
                break
    finally:kernel.CloseHandle(snapshot)

def atomic_write(path,data):
    temp=path.with_name(path.name+'.aitactics.tmp')
    temp.write_bytes(data); temp.replace(path)

def main():
    parser=argparse.ArgumentParser(); parser.add_argument('action',choices=['build','install','restore','verify']); args=parser.parse_args()
    package=PROJECT/'build/package'; manifest_path=package/'manifest.json'
    if args.action=='install':
        raise RuntimeError('This EXE modification failed the real game with error 51. Installation disabled; use package_input.py instead.')
    target=GAME/'SAN14CE_KO.exe'; backup=PROJECT/'backup/SAN14CE_KO.original.exe'
    if args.action=='build':
        source=backup if backup.exists() else target
        report=patch_existing_graphics_import(source,package/target.name)
        dll=(PROJECT/'build/SAN14_AITactics.dll').read_bytes(); (package/DLL).write_bytes(dll)
        report['dll_sha256']=sha(dll); report['scope']='AI priorities; adjacency deferred'
        manifest_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(report,indent=2)); return
    report=json.loads(manifest_path.read_text(encoding='utf-8'))
    if args.action=='verify':
        assert sha(target.read_bytes())==report['patched_sha256'],'Installed EXE differs'
        assert sha((GAME/DLL).read_bytes())==report['dll_sha256'],'Installed DLL differs'
        assert sha(backup.read_bytes())==ORIGINAL_SHA,'Backup differs'
        print('Installed EXE, DLL, and original backup hashes verified.'); return
    no_game()
    if args.action=='install':
        original=target.read_bytes()
        if sha(original)==report['patched_sha256']:
            if sha((GAME/DLL).read_bytes())==report['dll_sha256']: print('Already installed.'); return
            raise RuntimeError('An installed DLL differs; restore first.')
        if sha(original)!=ORIGINAL_SHA: raise RuntimeError('Unexpected current EXE; refusing overwrite.')
        if (GAME/DLL).exists(): raise RuntimeError('Existing DLL would be overwritten; refusing.')
        backup.parent.mkdir(parents=True,exist_ok=True)
        if backup.exists() and sha(backup.read_bytes())!=ORIGINAL_SHA: raise RuntimeError('Backup hash differs')
        if not backup.exists(): backup.write_bytes(original)
        dll=(package/DLL).read_bytes(); patched=(package/target.name).read_bytes()
        assert sha(dll)==report['dll_sha256'] and sha(patched)==report['patched_sha256']
        atomic_write(GAME/DLL,dll)
        try: atomic_write(target,patched)
        except BaseException: (GAME/DLL).unlink(); raise
        print('Installed; exact original backup retained.'); return
    current=sha(target.read_bytes())
    if current not in [ORIGINAL_SHA,report['patched_sha256']]: raise RuntimeError('Current EXE changed; refusing restore overwrite.')
    original=backup.read_bytes()
    if sha(original)!=ORIGINAL_SHA: raise RuntimeError('Backup hash differs')
    if (GAME/DLL).exists() and sha((GAME/DLL).read_bytes())!=report['dll_sha256']: raise RuntimeError('DLL changed; refusing removal')
    atomic_write(target,original)
    if (GAME/DLL).exists(): (GAME/DLL).unlink()
    print('Restored exact original executable. Backup and source retained.')

if __name__=='__main__': main()
