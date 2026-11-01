"""사용자 결정 2026-10-30: 통합패치·발탁장수 공존을 위해 첫머리 훅을 호출 자리로 이동."""
import functools
import hashlib
import json
import re
import struct
import zlib
from pathlib import Path
from native_search_callgraph import NativeGraph, derive

P = Path(__file__).resolve().parents[1]
d = (P / 'research/AI_Analysis/native_analysis_only.bin').read_bytes()
pe = struct.unpack_from('<I', d, 60)[0]
sh = pe + 24 + struct.unpack_from('<H', d, pe + 20)[0]
sections = []
for i in range(struct.unpack_from('<H', d, pe + 6)[0]):
    off = sh + 40 * i
    size, va, raw_size, raw = struct.unpack_from('<4I', d, off + 8)
    sections.append((d[off:off+8].rstrip(b'\0'), va, size, raw))
_, va, size, raw = next(s for s in sections if s[0] == b'.text')
text = d[raw:raw+size]
checks = 0


def check(value, label):
    global checks
    checks += 1
    assert value, label


check(struct.unpack_from('<I', d, pe+8)[0] == 0x6A983C25, 'matching timestamp')
check(zlib.crc32(text) == 0x6C7F386E, 'matching complete text CRC')
source = (P/'src/native_sortie_links.inl').read_text(encoding='utf-8')


def sites(name):
    body = re.search(name+r'\[\]=\{(.*?)\};', source, re.S).group(1)
    return [int(s, 16) for s in re.findall(r'0x([0-9A-F]+)', body)]


@functools.cache
def native_calls(target):
    return [va+i for i in range(len(text)-4)
            if text[i] == 0xE8 and va+i+5+struct.unpack_from('<i', text, i+1)[0] == target]


movement = sites('sortie_movement_sites')
search = sites('sortie_search_sites')
plans = sites('sortie_plan_sites')
check(len(movement) == 49 and movement == native_calls(0x2B0290), 'all 49 movement calls, in order')
check(len(native_calls(0x1843E0)) == 100, '100 original search callers')
# 사용자 결정 2026-10-30: 통합패치·발탁장수 공존을 위해 첫머리 훅을 호출 자리로 이동
# Claude review: regenerate the depth-eight graph, retaining all capture users.
derived = derive(P)
check(sites('sortie_search_roots') == list(derived['roots']), 'all planners and audited native trace-scope roots')
check(search == derived['sites'], 'source search table exactly equals independently regenerated .text/.pdata graph')
check(set([0x46425, 0x4658B, 0x484D7, 0x4862B, 0x4A5B6, 0x4A70E,
           0x4B38D, 0x4D426, 0x4D58D, 0x1ECCF6]) <= set(search), 'all former ten search callers retained')
graph = NativeGraph(d)
for site in native_calls(0x1843E0):
    if derived['depths'].get(graph.owner(site), 99) < 8:
        check(site in search, f'byte scan independently confirms every reachable search caller {site:X}')
for site, path in derived['paths'].items():
    check(path[0][0] in {graph.owner(root) for root in derived['roots']} and sum(e[3] for e in path) <= 8,
          f'graph search caller {site:X} has a bounded path from an authorized root')
    for _, at, target, cost in path:
        opcode = text[at-va]
        width = 1 if opcode == 0xEB else 4
        delta = int.from_bytes(text[at-va+1:at-va+1+width], 'little', signed=True)
        check(opcode in ((0xE8,) if cost else (0xE9, 0xEB)) and graph.owner(at+1+width+delta) == target,
              f'graph edge at {at:X} is an actual direct instruction to its unwind owner')


class DepthFixture(NativeGraph):
    def __init__(self):
        self.decode_gaps = []

    def owner(self, address):
        return address

    def edges(self, entry):
        if entry == 1:
            return ((0x10, 2, 0),)
        if entry == 2:
            return ((0x20, 1, 0), (0x21, 10, 1))
        if 10 <= entry < 16:
            return ((entry*16, entry+1, 1),)
        if entry == 16:
            return ((0x160, 0x1843E0, 1), (0x161, 17, 1))
        if entry == 17:
            return ((0x170, 0x1843E0, 1),)
        return ()


boundary_sites, _, boundary_depths = DepthFixture().search_calls([1])
check(boundary_sites == [0x160] and boundary_depths[0x1843E0] == 8,
      'depth eight included, depth nine excluded, zero-cost tail cycle terminates')
for site in search:
    check(site in native_calls(0x1843E0), f'search caller {site:X}')
for site, target in zip(plans, [0x48CE0, 0x46B80, 0x4BA50, 0x4ABE0, 0x44A80]):
    check(native_calls(target) == [site], f'planner unique caller {site:X}')
call_count = len(plans)+len(search)+len(movement)
check(len(set(plans+search+movement)) == call_count, 'all combat callers distinct')
check(text[0x26C962-va] == 0xE9 and
      0x26C967+struct.unpack_from('<i', text, 0x26C963-va)[0] == 0x301AB0,
      'command-update tail jump 26C940 -> 301AB0 confirms restored capture path')
check(0x301BD9 in native_calls(0x1843E0) and 0x301BD9 in search,
      'deep command-update search 301BD9 is hooked for both trace consumers')
check(0x5CC37E in native_calls(0x2C0820), 'info native E8 target')
check('log_event("sortie_call_skip"' in source, 'skipped sortie calls logged')
split = (P/'src/native_split_install.inl').read_text(encoding='utf-8')
check('log_event("info_call_skip"' in split, 'skipped info calls logged')
check('code.links[i],e.rva,0xE9' not in source, 'no former eight-entry publication loop')
check('0x2C0820,0xE9' not in split, 'no info entry publication')
check('relay+4096+k*16' in source and 4096+call_count*16 <= 8192 and
      '4096+kSortieCallCount*16<=8192' in source, 'all disjoint 16-byte tail relays fit existing allocation')
check('relay+2432,gate=stub+32' in source, 'dispatch retains old reserved region')
check('relay+3200' in split and 'relay+3232' in split and 'relay+3264' not in split,
      'info relays retain offsets without relocated entry')
adapter = (P/'src/native_adapter.cpp').read_text(encoding='utf-8-sig')
check('Obj call,unsigned bytes=0x1000' in adapter and 'combat_relay,0x2000' in adapter,
      'only requested relay allocation paths extended')
check('fn<InfoListCreate>(0x2C0820)' in (P/'src/native_officer_info.inl').read_text(),
      'officer fallback keeps native entry')
graph_record = dict(derived, search_call_count=len(search), outside_search_calls=100-len(search),
                    depth_limit=8, tail_jump_cost=0, gameplay_verified=False)
(P/'build/development/sortie_search_callgraph_verified.json').write_text(
    json.dumps(graph_record, indent=2)+'\n', encoding='utf-8')
print(json.dumps(dict(status='callsite_inventory_passed', checks=checks, failures=0,
                      movement_calls=49, search_calls=len(search), outside_search_calls=100-len(search),
                      search_sites=[f'0x{site:X}' for site in search], graph_functions=derived['functions'],
                      relay_end=4096+call_count*16,
                      analysis_sha256=hashlib.sha256(d).hexdigest(), gameplay_verified=False)))
