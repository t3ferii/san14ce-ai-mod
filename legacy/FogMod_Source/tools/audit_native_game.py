"""Read SAN14CE_KO.exe directly and check candidate native integration sites.

The old mod supplies candidate addresses and expected bytes only. No mod code is
executed or copied into a new executable. This is an analysis tool, not a mod.
Uses Python's standard library and never changes the input EXE or DLL.
"""
from __future__ import annotations

import hashlib
import json
import re
import struct
from pathlib import Path


class PEImage:
    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
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


def array_byte_size(type_id: int, types: dict) -> int:
    node = types[type_id]
    attrs = node['attributes']
    if 'DW_AT_byte_size' in attrs:
        return attrs['DW_AT_byte_size']
    if node['tag'] == 'DW_TAG_array_type':
        count = 1
        for child in node['children']:
            a = child['attributes']
            if 'DW_AT_count' in a:
                count *= a['DW_AT_count']
            elif 'DW_AT_upper_bound' in a:
                count *= a['DW_AT_upper_bound'] - a.get('DW_AT_lower_bound', 0) + 1
            else:
                raise ValueError('Unknown array bound')
        return count * array_byte_size(attrs['DW_AT_type'], types)
    if 'DW_AT_type' in attrs:
        return array_byte_size(attrs['DW_AT_type'], types)
    raise ValueError(f'Unknown size for type {type_id}')


def main():
    project = Path(__file__).resolve().parents[1]
    game = PEImage(project.parent / 'SAN14CE_KO.exe')
    reference = project / 'reference' / 'original_package' / '_files' / 'payload' / 'SAN14_FogTrial.dll'
    mod = reference.read_bytes()
    mod_pe = PEImage(reference)
    symbols = json.loads((project / 'metadata' / 'coff_symbols.json').read_text(encoding='utf-8'))
    variables = {v['name']: v for v in json.loads((project / 'metadata' / 'variables.json').read_text(encoding='utf-8'))
                 if 'DW_AT_location' in v['attributes']}
    variables_by_va = {}
    for variable in variables.values():
        location = variable['attributes']['DW_AT_location']
        if isinstance(location, list) and len(location) == 9 and location[0] == 3:
            variables_by_va[int.from_bytes(bytes(location[1:]), 'little')] = variable
    types = {v['offset']: v for v in json.loads((project / 'metadata' / 'types.json').read_text(encoding='utf-8'))}
    signature_symbols = {mod_pe.image_base + s['rva']: s for s in symbols
                         if re.search(r'_sig_[0-9a-fA-F]+$', s['name'])}
    signature_tables = {}
    for section in mod_pe.sections:
        if section['name'] not in ('.rdata', '.data'):
            continue
        blob = mod[section['file_offset']:section['file_offset'] + section['raw_size']]
        for at in range(0, len(blob) - 15, 8):
            target, length, pointer = struct.unpack_from('<IIQ', blob, at)
            signature = signature_symbols.get(pointer)
            variable = variables_by_va.get(pointer)
            if signature is None or variable is None or not (0 < target < game.image_size):
                continue
            if length != array_byte_size(variable['attributes']['DW_AT_type'], types):
                continue
            signature_tables.setdefault(signature['name'], []).append(dict(
                target_rva=target, table_rva=section['rva'] + at))
    sites = []
    for symbol in symbols:
        match = re.search(r'_sig_([0-9a-fA-F]+)$', symbol['name'])
        if not match:
            continue
        variable = variables_by_va.get(mod_pe.image_base + symbol['rva'])
        if variable is None:
            variable = variables.get(symbol['name'])
        if variable is None:
            raise ValueError(f"Missing type metadata: {symbol['name']}")
        length = array_byte_size(variable['attributes']['DW_AT_type'], types)
        # A suffix can retain an old research address even after a signature
        # table is updated. Use actual table targets where available, not names.
        name_hint = int(match[1], 16)
        table_entries = signature_tables.get(symbol['name'], [])
        targets = sorted({entry['target_rva'] for entry in table_entries})
        rva = targets[0] if len(targets) == 1 else name_hint
        expected = mod[symbol['offset']:symbol['offset'] + length]
        actual = game.read(rva, length)
        offset, section = game.offset(rva, length)
        sites.append(dict(reference_name=symbol['name'], reference_source=variable['source'],
                          game_rva=f'0x{rva:X}', game_file_offset=f'0x{offset:X}',
                          symbol_name_rva_hint=f'0x{name_hint:X}',
                          address_source='signature_table' if len(targets) == 1 else 'symbol_name_hint_only',
                          signature_table_entries=table_entries,
                          section=section, byte_count=length, exact_match=actual == expected,
                          actual_bytes=actual.hex(' '), expected_bytes=expected.hex(' '),
                          native_function=game.containing_function(rva)))
    report = dict(executable=str(game.path), sha256=hashlib.sha256(game.data).hexdigest(),
                  file_size=len(game.data), machine=f'0x{game.machine:X}',
                  timestamp=f'0x{game.timestamp:X}', image_base=f'0x{game.image_base:X}',
                  image_size=f'0x{game.image_size:X}', native_function_entries=len(game.functions),
                  candidate_sites=len(sites), matched=sum(s['exact_match'] for s in sites),
                  mismatched=sum(not s['exact_match'] for s in sites), sections=game.sections,
                  sites_with_table_targets=sum(s['address_source'] == 'signature_table' for s in sites),
                  sites=sites,
                  limitations=['Symbol suffixes are research labels and may not be actual runtime check addresses.',
                               'Only entries marked signature_table have target addresses derived from a table.',
                               'Byte matches identify candidate integration locations only.',
                               'They do not prove function semantics, ABI, hook safety, or feature equivalence.',
                               'This tool implements no fog, weather, UI, or game patching.'])
    output = project / 'native_game_analysis'
    output.mkdir(exist_ok=True)
    (output / 'native_sites.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('sha256', 'native_function_entries', 'candidate_sites', 'matched', 'mismatched')}, indent=2))


if __name__ == '__main__':
    main()
