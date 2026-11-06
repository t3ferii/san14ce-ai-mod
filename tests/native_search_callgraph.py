import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Offline .text/.pdata graph for the 2026-10-30 search-capture revision.

Root depth is zero. E8 edges cost one; direct tail jumps cost zero. The
maximum of eight includes the E8 into 1843E0. Indirect calls are not inferred.
"""
import bisect
import collections
import functools
import re
import struct
import sys
from pathlib import Path

try:
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64
except ModuleNotFoundError:
    # Reuse the dependency already used by verify_path_fast.py; no installation.
    pass  # Public snapshot: install capstone in the active Python environment.
    from capstone import Cs, CS_ARCH_X86, CS_MODE_64

PLANNERS = (0x48CE0, 0x46B80, 0x4BA50, 0x4ABE0, 0x44A80, 0x1EC890)


def source_capture_roots(project):
    """Audit every active capture scope and its local helper closure."""
    names = ('native_sortie.inl', 'native_campaign_routes.inl', 'native_adapter.cpp')
    sources = {name: (project/'src'/name).read_text(encoding='utf-8-sig') for name in names}
    # Mask comments and quoted strings without changing source offsets.
    mask = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
    clean = {name: mask.sub(lambda m: ' ' * len(m[0]), text) for name, text in sources.items()}
    scopes = (
        ('native_sortie.inl', 'sortie_trace_output=&trace;', 'sortie_trace_output=previous;'),
        ('native_sortie.inl', 'campaign_native_trace=&trace;', 'campaign_native_trace=prior;'),
        ('native_campaign_routes.inl', 'campaign_native_trace=&result.cells;', 'campaign_native_trace=previous;'),
    )
    # New setters must be added to the audit instead of being silently ignored.
    setter_files = {}
    for path in (project/'src').iterdir():
        if path.suffix not in ('.cpp', '.inl', '.h'):
            continue
        text = mask.sub(lambda m: ' ' * len(m[0]), path.read_text(encoding='utf-8-sig'))
        count = len(re.findall(r'\b(?:sortie_trace_output|campaign_native_trace)\s*=\s*&', text))
        if count:
            setter_files[path.name] = count
    assert setter_files == {'native_sortie.inl': 2, 'native_campaign_routes.inl': 1}, setter_files

    def body(name):
        for text in clean.values():
            match = re.search(r'^static[^\n;{}]*\b'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{', text, re.M)
            if not match:
                continue
            start = match.end(); depth = 1
            for end in range(start, len(text)):
                depth += (text[end] == '{') - (text[end] == '}')
                if not depth:
                    return text[start:end]
        return None

    queue = collections.deque()
    for name, start, end in scopes:
        text = clean[name]
        assert text.count(start) == 1 and text.count(end) == 1
        queue.append(text[text.index(start)+len(start):text.index(end, text.index(start))])
    roots = set(); helpers = set()
    while queue:
        text = queue.popleft()
        roots.update(int(rva, 16) for rva in re.findall(r'\bfn<[^;\n]*?>\s*\(\s*(0x[0-9A-Fa-f]+)\s*\)', text))
        for name in re.findall(r'\b([A-Za-z_]\w*)\s*\(', text):
            if name in helpers or name in ('if', 'for', 'while', 'switch', 'sizeof', 'decltype'):
                continue
            found = body(name)
            if found is not None:
                helpers.add(name); queue.append(found)
    expected = {0xA2410, 0x2EAAE0, 0x2EAB40, 0x2EE1E0, 0x26C940,
                0x32F500, 0x22CC40, 0x22DA10}
    assert roots == expected, ([hex(rva) for rva in sorted(roots)], sorted(helpers))
    return tuple(sorted(set(PLANNERS) | roots)), sorted(helpers)


class NativeGraph:
    def __init__(self, data):
        self.data = data
        pe = struct.unpack_from('<I', data, 60)[0]
        sh = pe+24+struct.unpack_from('<H', data, pe+20)[0]
        self.sections = []
        for i in range(struct.unpack_from('<H', data, pe+6)[0]):
            off = sh+40*i
            size, va, _, raw = struct.unpack_from('<4I', data, off+8)
            self.sections.append((data[off:off+8].rstrip(b'\0'), va, size, raw))
        _, self.text_start, size, _ = next(s for s in self.sections if s[0] == b'.text')
        self.text_end = self.text_start+size
        _, _, size, raw = next(s for s in self.sections if s[0] == b'.pdata')
        self.ranges = sorted(struct.unpack_from('<III', data, raw+i)
                             for i in range(0, size-11, 12)
                             if struct.unpack_from('<I', data, raw+i)[0])
        self.starts = [a for a, _, _ in self.ranges]
        self.groups = collections.defaultdict(list)
        self.owners = {}
        for entry in self.ranges:
            owner = self.unwind_owner(entry)
            self.groups[owner].append(entry[:2]); self.owners[entry[0]] = owner
        self.md = Cs(CS_ARCH_X86, CS_MODE_64)
        # Unwind spans may end in embedded jump tables/alignment data. Continue
        # decoding subsequent bytes instead of truncating that function's edges.
        self.md.skipdata = True
        self.decode_gaps = []

    def read(self, address, size):
        for _, va, length, raw in self.sections:
            if va <= address and address+size <= va+length:
                return self.data[raw+address-va:raw+address-va+size]
        raise ValueError(f'RVA outside file sections: {address:X}+{size:X}')

    @functools.cache
    def unwind_owner(self, entry):
        start, _, unwind = entry
        header = self.read(unwind, 4)
        if header[0] & 0x20:  # UNW_FLAG_CHAININFO, padded array of UNWIND_CODE
            at = unwind+4+((header[2]+1) & ~1)*2
            return self.unwind_owner(struct.unpack('<III', self.read(at, 12)))
        return start

    def owner(self, address):
        i = bisect.bisect_right(self.starts, address)-1
        if i >= 0 and address < self.ranges[i][1]:
            return self.owners[self.ranges[i][0]]
        return address

    def in_text(self, address):
        return self.text_start <= address < self.text_end

    @functools.cache
    def edges(self, entry):
        """Return decoded (call site, target function, E8 depth cost) edges."""
        edges = set()
        ranges = self.groups.get(entry)
        if ranges:
            for start, end in ranges:
                consumed = start
                for at, size, mnemonic, operands in self.md.disasm_lite(self.read(start, end-start), start):
                    consumed = at+size
                    self.edge(entry, at, size, mnemonic, operands, edges)
                if consumed != end:
                    self.decode_gaps.append((start, end, consumed))
        else:
            # Leaf functions have no .pdata entry. Decode their reachable basic
            # blocks, bounded by the next unwind entry, stopping at RET/JMP.
            i = bisect.bisect_right(self.starts, entry)
            end = self.starts[i] if i < len(self.starts) else self.text_end
            blocks = [entry]; decoded = set()
            while blocks:
                start = blocks.pop()
                if not entry <= start < end or start in decoded:
                    continue
                for at, size, mnemonic, operands in self.md.disasm_lite(self.read(start, end-start), start):
                    if at in decoded:
                        break
                    decoded.add(at)
                    self.edge(entry, at, size, mnemonic, operands, edges)
                    if mnemonic.startswith('j') and operands.startswith('0x'):
                        target = int(operands, 16)
                        if entry <= target < end:
                            blocks.append(target)
                    if mnemonic in ('ret', 'retf', 'jmp', 'ud2', 'int3'):
                        break
        return tuple(sorted(edges))

    def edge(self, entry, at, size, mnemonic, operands, edges):
        opcode = self.read(at, 1)[0]
        if opcode == 0xE8 and size == 5:
            target = at+5+struct.unpack('<i', self.read(at+1, 4))[0]
            if self.in_text(target):
                edges.add((at, self.owner(target), 1))
        elif mnemonic == 'jmp' and operands.startswith('0x'):
            target = int(operands, 16)
            if self.in_text(target) and self.owner(target) != entry:
                edges.add((at, self.owner(target), 0))

    def search_calls(self, roots, maximum=8):
        depth = {}; paths = {}; queue = collections.deque()
        for root in roots:
            owner = self.owner(root)
            depth[owner] = 0; paths[owner] = []; queue.append(owner)
        sites = {}; searched = set()
        while queue:
            node = queue.popleft(); current = depth[node]
            searched.add(node)
            for site, target, cost in self.edges(node):
                next_depth = current+cost
                if next_depth > maximum:
                    continue
                path = paths[node]+[(node, site, target, cost)]
                if cost and target == self.owner(0x1843E0):
                    sites[site] = path
                if next_depth < depth.get(target, maximum+1):
                    depth[target] = next_depth; paths[target] = path
                    if cost:
                        queue.append(target)
                    else:
                        queue.appendleft(target)
        assert not self.decode_gaps, self.decode_gaps
        return sorted(sites), sites, depth


def derive(project):
    project = Path(project)
    roots, helpers = source_capture_roots(project)
    graph = NativeGraph((project/'research/AI_Analysis/native_analysis_only.bin').read_bytes())
    sites, paths, depths = graph.search_calls(roots)
    return dict(roots=roots, helpers=helpers, sites=sites, paths=paths,
                depths=depths, functions=len(depths), pdata_entries=len(graph.ranges))


if __name__ == '__main__':
    import json
    result = derive(Path(__file__).resolve().parents[1])
    print(json.dumps(result, indent=2))
