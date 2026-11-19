"""Read-only PE32+ reader for static checks (sections, RVA -> file offset, .pdata function ranges).

Moved 2026-11-16 from the untracked legacy/FogMod_Source/tools/audit_native_game.py so tracked tests and tools
(verify_modpack_static, verify_split_exports, install_damage_preview, package_mod) work in a clean checkout or a
worktree without the legacy folder. Standard library only; never changes the input file.
"""
from __future__ import annotations

import struct
from pathlib import Path


class PEImage:
    def __init__(self, path: Path):
        self.path = path
        self.data = Path(path).read_bytes()
        if self.data[:2] != b'MZ':
            raise ValueError('Not a PE image')
        pe = struct.unpack_from('<I', self.data, 0x3C)[0]
        if self.data[pe:pe + 4] != b'PE\0\0':
            raise ValueError('Bad PE signature')
        self.machine, count, self.timestamp = struct.unpack_from('<HHI', self.data, pe + 4)
        optional_size = struct.unpack_from('<H', self.data, pe + 20)[0]
        optional = pe + 24
        if struct.unpack_from('<H', self.data, optional)[0] != 0x20B:
            raise ValueError('Expected PE32+ (64-bit)')
        self.image_base = struct.unpack_from('<Q', self.data, optional + 24)[0]
        self.image_size = struct.unpack_from('<I', self.data, optional + 56)[0]
        self.sections = []
        for i in range(count):
            at = optional + optional_size + i * 40
            name = self.data[at:at + 8].rstrip(b'\0').decode('ascii', 'replace')
            virtual_size, rva, raw_size, raw = struct.unpack_from('<IIII', self.data, at + 8)
            self.sections.append(dict(name=name, rva=rva, virtual_size=virtual_size,
                                      raw_size=raw_size, file_offset=raw))
        # The exception directory identifies actual function boundaries in the
        # EXE. A signature can point inside a function, not just to its entry.
        exception_rva, exception_size = struct.unpack_from('<II', self.data, optional + 112 + 3 * 8)
        self.functions = []
        if exception_rva and exception_size:
            raw = self.read(exception_rva, exception_size)
            for at in range(0, len(raw) - 11, 12):
                start, end, unwind = struct.unpack_from('<III', raw, at)
                if start < end:
                    self.functions.append((start, end, unwind))

    def offset(self, rva: int, count: int) -> tuple[int, str]:
        for s in self.sections:
            delta = rva - s['rva']
            if 0 <= delta and delta + count <= s['raw_size']:
                return s['file_offset'] + delta, s['name']
        raise ValueError(f'RVA 0x{rva:X}, length {count} has no file-backed section')

    def read(self, rva: int, count: int) -> bytes:
        offset, _ = self.offset(rva, count)
        return self.data[offset:offset + count]

    def containing_function(self, rva: int):
        for start, end, unwind in self.functions:
            if start <= rva < end:
                return dict(start_rva=f'0x{start:X}', end_rva=f'0x{end:X}',
                            unwind_rva=f'0x{unwind:X}', offset_into_function=rva - start)
        return None
