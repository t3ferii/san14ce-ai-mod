"""Read-only validation of native code in a normally running SAN14CE_KO.exe.

Uses PROCESS_VM_READ and PROCESS_QUERY_INFORMATION. Does not write memory,
inject code, suspend threads, call game functions, or load the old mod.
"""
from __future__ import annotations
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
import os
from pathlib import Path
import struct


class MODULEENTRY32W(C.Structure):
    _fields_ = [('dwSize', W.DWORD), ('th32ModuleID', W.DWORD),
                ('th32ProcessID', W.DWORD), ('GlblcntUsage', W.DWORD),
                ('ProccntUsage', W.DWORD), ('modBaseAddr', C.c_void_p),
                ('modBaseSize', W.DWORD), ('hModule', W.HMODULE),
                ('szModule', W.WCHAR * 256), ('szExePath', W.WCHAR * 260)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--capture-code', action='store_true',
                        help='Save only the main EXE executable sections for native function analysis')
    args = parser.parse_args()
    if os.name != 'nt' or C.sizeof(C.c_void_p) != 8:
        raise RuntimeError('64-bit Windows Python required')
    project = Path(__file__).resolve().parents[1]
    directory = project / 'native_game_analysis'
    disk = json.loads((directory / 'native_sites.json').read_text(encoding='utf-8'))
    kernel = C.WinDLL('kernel32', use_last_error=True)
    kernel.CreateToolhelp32Snapshot.argtypes = [W.DWORD, W.DWORD]
    kernel.CreateToolhelp32Snapshot.restype = W.HANDLE
    kernel.Module32FirstW.argtypes = [W.HANDLE, C.POINTER(MODULEENTRY32W)]
    kernel.Module32FirstW.restype = W.BOOL
    kernel.Module32NextW.argtypes = kernel.Module32FirstW.argtypes
    kernel.Module32NextW.restype = W.BOOL
    kernel.CloseHandle.argtypes = [W.HANDLE]
    kernel.CloseHandle.restype = W.BOOL
    kernel.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    kernel.OpenProcess.restype = W.HANDLE
    kernel.ReadProcessMemory.argtypes = [W.HANDLE, C.c_void_p, C.c_void_p, C.c_size_t, C.POINTER(C.c_size_t)]
    kernel.ReadProcessMemory.restype = W.BOOL
    snapshot = kernel.CreateToolhelp32Snapshot(0x8 | 0x10, args.pid)
    if snapshot == C.c_void_p(-1).value:
        raise C.WinError(C.get_last_error())
    modules = []
    try:
        entry = MODULEENTRY32W()
        entry.dwSize = C.sizeof(entry)
        if not kernel.Module32FirstW(snapshot, C.byref(entry)):
            raise C.WinError(C.get_last_error())
        while True:
            modules.append(dict(name=entry.szModule, path=entry.szExePath,
                                base=entry.modBaseAddr, size=entry.modBaseSize))
            if not kernel.Module32NextW(snapshot, C.byref(entry)):
                error = C.get_last_error()
                if error != 18:  # ERROR_NO_MORE_FILES
                    raise C.WinError(error)
                break
    finally:
        kernel.CloseHandle(snapshot)
    games = [m for m in modules if m['name'].lower() == 'san14ce_ko.exe']
    if len(games) != 1 or Path(games[0]['path']).resolve() != Path(disk['executable']).resolve():
        raise RuntimeError('Target process is not this workspace SAN14CE_KO.exe')
    game = games[0]
    handle = kernel.OpenProcess(0x0400 | 0x0010, False, args.pid)
    if not handle:
        raise C.WinError(C.get_last_error())
    def read(rva, length):
        if rva < 0 or rva + length > game['size']:
            raise ValueError('Read outside the main game module')
        buffer = C.create_string_buffer(length)
        count = C.c_size_t()
        if not kernel.ReadProcessMemory(handle, game['base'] + rva, buffer, length, C.byref(count)):
            raise C.WinError(C.get_last_error())
        if count.value != length:
            raise RuntimeError('Incomplete memory read')
        return buffer.raw
    try:
        header = read(0, 4096)
        if header[:2] != b'MZ':
            raise RuntimeError('Invalid module DOS header')
        pe = struct.unpack_from('<I', header, 0x3C)[0]
        machine = struct.unpack_from('<H', header, pe + 4)[0]
        timestamp = struct.unpack_from('<I', header, pe + 8)[0]
        size = struct.unpack_from('<I', header, pe + 24 + 56)[0]
        if machine != 0x8664 or timestamp != int(disk['timestamp'], 16) or size != int(disk['image_size'], 16):
            raise RuntimeError('Incompatible running game PE profile')
        sites = []
        for site in disk['sites']:
            actual = read(int(site['game_rva'], 16), site['byte_count'])
            sites.append(dict(reference_name=site['reference_name'], game_rva=site['game_rva'],
                              address_source=site['address_source'],
                              byte_count=site['byte_count'],
                              exact_match=actual == bytes.fromhex(site['expected_bytes']),
                              differs_from_disk=actual != bytes.fromhex(site['actual_bytes']),
                              actual_bytes=actual.hex(' '), expected_bytes=site['expected_bytes']))
        report = dict(pid=args.pid, main_module_path=game['path'], main_module_base=f"0x{game['base']:X}",
                      timestamp=f'0x{timestamp:X}', image_size=f'0x{size:X}',
                      candidate_sites=len(sites), matched=sum(s['exact_match'] for s in sites),
                      mismatched=sum(not s['exact_match'] for s in sites),
                      sites_different_from_disk=sum(s['differs_from_disk'] for s in sites),
                      sites_with_table_targets=sum(s['address_source'] == 'signature_table' for s in sites),
                      table_target_matches=sum(s['address_source'] == 'signature_table' and s['exact_match'] for s in sites),
                      old_fog_dll_loaded=any(m['name'].lower() == 'san14_fogtrial.dll' for m in modules),
                      sites=sites,
                      limitations=['Read-only byte comparison; no hook or behavior equivalence tested.',
                                   'Only these candidate native code locations were read.'])
        (directory / 'running_native_sites.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps({k: v for k, v in report.items() if k not in ('sites', 'limitations')}, indent=2))
        if args.capture_code:
            captures = []
            # Capture code sections only, excluding game data and other modules.
            section_count = struct.unpack_from('<H', header, pe + 6)[0]
            optional_size = struct.unpack_from('<H', header, pe + 20)[0]
            section_start = pe + 24 + optional_size
            for i in range(section_count):
                at = section_start + i * 40
                name = header[at:at + 8].rstrip(b'\0').decode('ascii', 'replace')
                virtual_size, rva, raw_size, _ = struct.unpack_from('<IIII', header, at + 8)
                characteristics = struct.unpack_from('<I', header, at + 36)[0]
                if characteristics & 0x20000000 and name != '.bind':
                    payload = read(rva, min(virtual_size, raw_size))
                    filename = 'running_code_' + name.strip('.') + '.bin'
                    (directory / filename).write_bytes(payload)
                    captures.append(dict(section=name, rva=rva, bytes=len(payload), file=filename))
            (directory / 'running_code_map.json').write_text(json.dumps(dict(pid=args.pid,
                module_base=game['base'], executable=game['path'], sections=captures), indent=2), encoding='utf-8')
            print(json.dumps({'code_sections_saved': captures}, indent=2))
    finally:
        kernel.CloseHandle(handle)


if __name__ == '__main__':
    main()
