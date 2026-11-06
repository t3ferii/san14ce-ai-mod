import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""User/Claude 2026-10-29: independent decoded EXE ABI + tree wiring checks."""
from pathlib import Path
import hashlib
import json
import re
import struct

P = Path(__file__).resolve().parents[1]
exe = Path(r'C:\Users\<user>\Documents\Codex\analysis\SAN14CE_KO\1fbe6be7466a6646\input\game.exe.unpacked.exe')
if not exe.exists():
    exe = P / 'build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe'
data = exe.read_bytes()
checks = 0


def ok(value, label):
    global checks
    checks += 1
    assert value, label


pe = struct.unpack_from('<I', data, 60)[0]
optional = pe + 24
base = struct.unpack_from('<Q', data, optional + 24)[0]
table = optional + struct.unpack_from('<H', data, pe + 20)[0]
sections = [struct.unpack_from('<IIII', data, table + i * 40 + 8)
            for i in range(struct.unpack_from('<H', data, pe + 6)[0])]


def read(rva, length):
    for _, va, size, raw in sections:
        if va <= rva and rva + length <= va + size:
            return data[raw + rva - va:raw + rva - va + length]
    raise ValueError(hex(rva))


def source(name):
    return (P / name).read_text(encoding='utf-8-sig')


native = source('src/native_terrain_trees.inl')
split = source('src/native_split_install.inl')
render = source('src/native_terrain_render.inl')
edit = source('src/native_terrain_edit.inl')
ok(hashlib.sha256(data).hexdigest() == '1fbe6be7466a66466fe97cf3ab7d49392913346879c7d40de7f4eaf86eef81c6', 'decoded EXE identity')
ok(struct.unpack('<Q', read(0x1694958, 8))[0] == base + 0xBB10F0, 'placement vtable +30 slot')
for name, rva in [('update_bytes', 0xBB10F0), ('setter_bytes', 0xB91330)]:
    body = re.search(rf'{name}\[16\]=\{{([^}}]+)\}}', native)[1]
    fingerprint = bytes(int(part, 16) for part in body.split(','))
    ok(len(fingerprint) == 16 and read(rva, 16) == fingerprint, f'{name} matches independent native bytes')

# push 5 registers, rbp = entry_rsp - 0x28 - 0x18F0 = entry_rsp - 0x1918.
# Thus +1940/+1948/+1950/+1958 are entry_rsp+28/+30/+38/+40, args 5..8.
body = read(0xBB10F0, 0x323)
for load in ['48 8b 85 40 19 00 00', '8b 85 48 19 00 00', '0b 85 50 19 00 00', '85 9d 58 19 00 00']:
    ok(bytes.fromhex(load) in body, 'callback reads pointer/mask stack argument: ' + load)
ok(read(0xBB9F24, 4) == bytes.fromhex('89 4c 24 38') and
   read(0xBB9F2E, 4) == bytes.fromhex('89 4c 24 30') and
   read(0xBB9F38, 4) == bytes.fromhex('89 4c 24 28') and
   read(0xBB9F43, 11) == bytes.fromhex('48 89 4c 24 20 48 8b cb ff 50 30'),
   'BB9F4B supplies pointer arg5 and three u32 masks before virtual +30')
ok(read(0xD67C34, 21) == bytes.fromhex('89 5c 24 38 89 7c 24 30 89 74 24 28 4c 89 74 24 20 41 ff 52 30'),
   'common engine dispatch D67C45 has the same eight-argument ABI')
ok('using Update=uint64_t(__fastcall *)(Obj,Obj,Obj,Obj,Obj,unsigned,unsigned,unsigned);' in native,
   'opaque return and exact native argument widths')
wrapper = native.split('static uint64_t __fastcall update(', 1)[1]
ok(wrapper.index('apply_pending(s);') < wrapper.index('return fn<Update>'), 'queued work precedes original update')
ok('(s,second,context,view,fifth,sixth,seventh,eighth);' in wrapper, 'all eight arguments forwarded unchanged')
mark = native.split('static void mark(', 1)[1].split('static void before_world_change()', 1)[0]
ok('AcquireSRWLockExclusive(&lock)' in mark and 'fn<' not in mark and 'pointer(' not in mark,
   'UI mark only holds numeric goals under SRW lock')
ok('terrain_trees::mark(hex,terrain,original_terrain);' in render, 'all terrain render marks also notify trees')
ok('terrain_trees::take_ui_work(resync)' in edit and 'if(resync)for(unsigned i=0;i<changes.size();++i)' in edit,
   'tree capture and renderer resync both replay current changes')
ok(edit.count('terrain_trees::before_world_change();') == 2, 'world identity and explicit load boundaries connected')
ok('terrain_render::mark(c.hex,current,c.original);' in edit and
   'terrain_render::mark(c.hex,c.next,c.original);' in source('src/native_terrain_edit_save.inl'),
   'reconcile and sidecar load enter the same tree mark path')
ok('{terrain_trees::Slot,terrain_trees::UpdateRva,reinterpret_cast<void *>(&terrain_trees::update),kPointerTerrain}' in split,
   'sole convenience owner in shared pointer publication/rollback table')
ok('split_disable(terrain_trees::permitted,29)' in split and
   'if(p.slot==terrain_trees::Slot)return mod_features.terrain_edit && terrain_trees::permitted;' in split,
   'foreign tree slot/start disables tree publication independently')
ok(split.index('!terrain_trees_guards_match()') < split.index('for(const auto &p:split_pointers)if(split_pointer_enabled(p)'),
   'tree slot is isolated before generic terrain pointer validation')
for forbidden in ['WriteProcessMemory', 'CreateRemoteThread', 'CreateThread', 'BC55F0', 'BC7110', 'world()']:
    ok(forbidden not in native, 'no remote/direct scan/worker/world access: ' + forbidden)
ok('#ifndef NO_RUNTIME_LOG\n    if(captured)log_event("terrain_trees_capture"' in native and
   'if(cells)log_event("terrain_trees_apply",cells,writes,0)' in native, 'bounded shared diagnostics omitted in release')
entry = json.loads(source('development/features.json'))['modules']['terrain-edit']
ok('terrain-trees' in entry['tests'] and 'src/native_terrain_trees.inl' in entry['sources'] and
   "'terrain-trees'" in source('tools/check_feature.ps1'), 'feature checker registration')
print(json.dumps({'terrain_trees_source_checks': checks, 'failed': 0, 'abi_arguments': 8}))
