import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Independent current-EXE bytes and split ownership; offline, never executes the EXE.

User/Claude 2026-10-29: one regional domestic entry, kind/occupancy/membership dispatch and six guarded slots including highlight exit.
"""
from pathlib import Path
import hashlib
import json
import re
import struct
import registry_text as R   # W5: generated include lists / SplitPointer rows

P = Path(__file__).resolve().parents[1]
EXE = Path((Path(__file__).resolve().parents[1]/'build/ui_native_analysis/editor83_b25953386/game.exe.unpacked.exe'))
if not EXE.exists():
    EXE = P / 'build/ui_native_analysis/editor83_b25953386/game.exe.unpacked.exe'
data = EXE.read_bytes()
checks = 0


def ok(condition, label):
    global checks
    checks += 1
    assert condition, label


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


digest = hashlib.sha256(data).hexdigest()
ok(digest == '1a4a447d14e68c40ad2a5107c29c7341c10eee8fc297a7b35032900c341dbcaa', 'decoded EXE identity')
ok(base == 0x140000000, 'image base')
slots = [(0x14503D8, 0x7A9D40), (0x143F950, 0x6ED8A0), (0x1444140, 0x726AE0), (0x1444148, 0x720530), (0x1450DB0, 0x7A9F40), (0x1450DE8, 0x7AB260)]
removed_slots = [(0x143F528, 0x732C60), (0x1453938, 0x7AA610)]
for slot, original in slots:
    ok(struct.unpack('<Q', read(slot, 8))[0] == base + original, f'original slot {slot:X}')
fingerprints = {
    # User/Claude 2026-10-29: independent decoded-byte evidence for native yellow highlight.
    'highlight_select_bytes': (0x73E654, '48 8b cb e8 d4 43 af ff 48 8b c8 48 8b d8 e8 19 31 bf ff 85 c0 74 2b f6 43 5f 01 74 25 e8 fa a7 8d ff 48 39 98 10 01 00 00 74 3a e8 ec a7 8d ff 83 b8 18 01 00 00 00 75 2c 48 89 98 10 01 00 00 eb 23 e8 d5 a7 8d ff 48 39 a8 10 01 00 00 74 15 e8 c7 a7 8d ff 83 b8 18 01 00 00 00 75 07 48 89 a8 10 01 00 00 48'),
    'highlight_lock_bytes': (0x488959, 'e8 12 05 b9 ff 83 b8 18 01 00 00 00 75 07 48 89 a8 10 01 00 00 e8 fd 04 b9 ff c7 80 18 01 00 00 01 00 00 00 e8 ee 04 b9 ff 48 8b c8 e8 06 eb 49 00'),
    'highlight_unlock_bytes': (0x488B58, 'e8 13 03 b9 ff 89 b0 18 01 00 00 e8 08 03 b9 ff 39 b0 18 01 00 00 75 07 48 89 b0 10 01 00 00 e8 f4 02 b9 ff 48 8b c8 e8 0c e9 49 00'),
    'hex_exit_bytes': (0x720530, '48 89 5c 24 10 57 48 83 ec 20 48 8b d9 48 8b fa'),
    'highlight_update_bytes': (0x927490, '48 8b c4 55 41 54 41 55 41 56 41 57 48 8d 68 98 48 81 ec 40 01 00 00 48 c7 44 24 58 fe ff ff ff'),
    'domestic_input_bytes': (0x73E615, 'e8 d6 2b 8d ff ba 01 00 00 00 48 8b c8 48 8b f8 e8 56 7a cb ff 85 c0 74 26 48 8b cf e8 3a 7f cb ff 85 c0 75 1a 48 8b d5 48 8b ce'),
    'hex_close_bytes': (0x73455B, 'c7 87 a8 04 00 00 01 00 00 00'),
    'hex_update_bytes': (0x7345D1, 'e8 4a c9 8d ff 48 8b c8 ba 05 00 00 00 e8 8d c9 13 00 85 c0 75 08 39 87 a8 04 00 00 74 17 e8 4c ca 8d ff 48 8b c8 e8 a4 de 8d ff'),
    'hex_hint_bytes': (0x7AB260, '48 8b 12 48 8b 49 08 e9 a4 fd f0 ff'),
    'hex_hint_call_bytes': (0x6BB1EE, '49 8b 8e a0 04 00 00 48 8b d6 e8 13 e4 19 00'),
    'hex_hint_set_bytes': (0x859610, '48 89 5c 24 08 57 48 83 ec 20 48 8b f9 48 8b da 48 8b 89 68 04 00 00 48 85 c9 74 22 48 85 d2 74 05'),

    'hex_destroy_bytes': (0x4354C9, '48 8b 4e 38 48 85 c9 74 10 48 8b 01 48 3b ce 0f 95 c2 ff 50 20 4c 89 76 38'),
    'hex_click_bytes': (0x7A9F40, '48 8b 12 48 8b 49 08 e9 e4 59 fc ff'),
    'hex_return_bytes': (0x726D5C, '48 8d 05 3d a0 d2 00'),
    'hex_open_bytes': (0x4598AB,
        'c7 87 78 04 00 00 01 00 00 00 48 89 5d c7 0f 28 05 70 c4 ee 00 0f 29 45 b7 44 8d 48 01 4c 8d 45 b7 48 8d 55 77 48 8b cb e8 e8 5b bb ff 85 c0 75 13 e8 0f 79 bb ff 48 8b 40 34 89 45 77 48 c1 e8 20 89 45 7b 48 8b 45 77 48 89 45 cf 33 db 89 5d d7 48 8d 45 df 48 89 45 77 48 8d 05 4d 21 f8 00 48 89 45 df 48 89 7d e7 48 8d 45 df 48 89 45 17 e8 20 77 bb ff 48 8b c8 4c 8d 4d df 4c 8d 45 c7 48 8d 15 76 11 f8 00 e8 59 ba fd ff'),
    'menu_call_bytes': (0x726D56, 'ff 90 88 01 00 00'),
    'domestic_click_bytes': (0x7A9D40, '48 8b 12 48 8b 49 08 e9 94 48 f9 ff'),
    'domestic_state_bytes': (0x73E5E0, '48 89 5c 24 18 56 48 83 ec 20 48 8b f1 48 8b da 48 8b ca e8 88 31 bf ff 85 c0 0f 84 de 01 00 00 48 89 6c 24 30 48 8b cb 48 89 7c 24 38 e8 ce d2 ae ff'),
    'domestic_area_bytes': (0x710C50, '48 8b 81 a0 04 00 00 48 85 c0 74 26 48 83 3d d4 34 0c 02 00 74 1c 48 8b 0d d3 34 0c 02 8b 00 48 85 c9 74 0e 3b 05 f6 34 0c 02 73 06 48 8b 04 c1 eb 02 33 c0 48 85 c0 74 15 0f 1f 80 00 00 00 00 48 3b 10 74 0c 48 8b 40 08 48 85 c0 75 f2 33 c0 c3 b8 01 00 00 00 c3'),
    'hex_complete_bytes': (0x4590A0, '48 8b 41 08 c7 80 78 04 00 00 00 00 00 00 c3'),
}
ui = (P / 'src/native_terrain_edit_ui.inl').read_text(encoding='utf-8-sig')
for name, (rva, expected) in fingerprints.items():
    expected = bytes.fromhex(expected)
    ok(read(rva, len(expected)) == expected, f'independent EXE fingerprint {rva:X}')
    literal = re.search(rf'{name}\[\]=\{{(.*?)\}};', ui, re.S).group(1)
    actual = bytes(int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]+)', literal))
    ok(actual == expected, f'runtime guard matches independent EXE {name}')
# Independent instruction decoding: signed RIP/call targets and native stack offsets.
def rel32(rva, size, offset):
    return rva + size + struct.unpack('<i', read(rva + offset, 4))[0]
# Native RCX object getter, predicate and flag; separate area was already in RBP.
ok(rel32(0x73E657, 5, 1) == 0x232A30, 'highlight tile object getter')
ok(rel32(0x73E662, 5, 1) == 0x331780, 'highlight object predicate')
ok(read(0x73E66B, 4) == bytes.fromhex('f6 43 5f 01'), 'highlight object low-bit flag')
for call in [0x73E671, 0x73E67F, 0x73E696, 0x73E6A4,
             0x488959, 0x48896E, 0x48897D, 0x488B58, 0x488B63, 0x488B77]:
    ok(rel32(call, 5, 1) == 0x18E70, f'highlight manager getter {call:X}')
ok(rel32(0x488985, 5, 1) == 0x927490 and rel32(0x488B7F, 5, 1) == 0x927490,
   'native lock and unlock call the same updater with RCX=manager')
for name in ['highlight_select_bytes', 'highlight_lock_bytes', 'highlight_unlock_bytes', 'hex_exit_bytes', 'highlight_update_bytes']:
    rva = fingerprints[name][0]
    ok(f'!memcmp(image+0x{rva:X},{name},sizeof({name}))' in ui, f'active runtime fingerprint {name}')
ok('pointer(image,0x1444148)==image+0x720530' in ui, 'exit original pointer runtime guard')
ok(rel32(0x7A9D47, 5, 1) == 0x73E5E0, 'regional functor tail target')
ok(rel32(0x73E5F3, 5, 1) == 0x331780, 'regional tile predicate call')
ok(rel32(0x73E60D, 5, 1) == 0x22B8E0, 'regional tile-to-area call')
ok(read(0x14503C8 + 2*8, 8) == struct.pack('<Q', base+0x7A9D40), 'regional functor vtable slot 2')
ok(read(0x726D56, 6) == bytes.fromhex('ff 90 88 01 00 00'), 'hex calls menu vt+188')
ok(rel32(0x7A9F47, 5, 1) == 0x76F930, 'hex callback native tail target')
ok(read(0x4598AB, 14) == bytes.fromhex('c7 87 78 04 00 00 01 00 00 00 48 89 5d c7'), 'ms+478=1 and params[0]=tile RBX')
ok(rel32(0x4598B9, 7, 3) == 0x1345D30, 'a8 copies native 16-byte constant')
ok(read(0x4598C4, 15) == bytes.fromhex('44 8d 48 01 4c 8d 45 b7 48 8d 55 77 48 8b cb'), 'F560 RCX=tile RDX=&res18 R8=&a8 R9D=1')
ok(rel32(0x4598D3, 5, 1) == 0xF4C0 and rel32(0x4598DC, 5, 1) == 0x111F0, 'position and fallback getters')
ok(read(0x4598EF, 13) == bytes.fromhex('48 8b 45 77 48 89 45 cf 33 db 89 5d d7'), 'params +8 position and +10 zero dword')
ok(rel32(0x459904, 7, 3) == 0x13DBA58, 'functor exact vtable')
ok(read(0x45990B, 16) == bytes.fromhex('48 89 45 df 48 89 7d e7 48 8d 45 df 48 89 45 17'), 'functor [0]=vtable [1]=ms [7]=self (+38)')
ok(rel32(0x45991B, 5, 1) == 0x11040, 'native manager getter')
ok(read(0x459920, 11) == bytes.fromhex('48 8b c8 4c 8d 4d df 4c 8d 45 c7'), '433020 RCX=manager R9=&functor R8=&params')
ok(rel32(0x45992B, 7, 3) == 0x13DAAA8 and read(0x13DAAA8, 22) == b'CStrategyAreaHexState\0', 'RDX exact native state name')
ok(rel32(0x459932, 5, 1) == 0x435390, 'native state open CALL bytes and target')
ok(read(0x4354C9, 25) == bytes.fromhex('48 8b 4e 38 48 85 c9 74 10 48 8b 01 48 3b ce 0f 95 c2 ff 50 20 4c 89 76 38'), '433020 destroys self functor with vt+20(f,0) and clears [7]')
for i, target in enumerate([0x458DC0, 0x458DC0, 0x4590A0, 0x45AFF0, 0x90DE00]):
    ok(struct.unpack('<Q', read(0x13DBA58 + 8*i, 8))[0] == base+target, f'functor vtable slot {i}')
# User/Claude 2026-10-29: identical-build decoded bytes authorized; runtime guards check live bytes.
ok(rel32(0x73E615, 5, 1) == 0x111F0, 'native click input getter')
ok(rel32(0x73E625, 5, 1) == 0x3F6080 and rel32(0x73E631, 5, 1) == 0x3F6570, 'native click input predicates')
ok(rel32(0x7AB267, 5, 1) == 0x6BB010, 'hint thunk tail target with state RCX and *arg RDX')
ok(read(0x726D18, 7) == bytes.fromhex('48 89 8f a0 04 00 00'), 'builder RCX saved as state+4A0')
ok(read(0x726DA2, 39) == bytes.fromhex('48 8d 05 2f a0 d2 00 48 89 45 80 48 89 7d 88 48 8d 45 80 48 89 45 b8 48 8d 55 80 48 8b 8f a0 04 00 00 e8 27 28 13 00'), 'hint functor captures same state and registers on same menu')
ok(rel32(0x726DA2, 7, 3) == 0x1450DD8 and rel32(0x726DC4, 5, 1) == 0x8595F0, 'hint vtable and registration targets')
ok(read(0x6BB037, 7) == bytes.fromhex('4c 63 ba 80 00 00 00'), 'hint uses passed menu+80 index')
ok(rel32(0x6BB1F8, 5, 1) == 0x859610, 'tooltip call target')
ok(read(0x6BB02D, 2) == bytes.fromhex('33 f6') and read(0x6BB1C8, 4) == bytes.fromhex('85 db 74 22'), 'zero message preserves null RSI through tooltip call')
ok(read(0x85961A, 6) == bytes.fromhex('48 8b f9 48 8b da') and read(0x85962C, 5) == bytes.fromhex('48 85 d2 74 05'), '857320 takes menu RCX and nullable string RDX')
source = R.split_install_text()   # W5: install code plus the generated SplitPointer rows
registration = source.split('static const SplitPointer split_pointers[]={', 1)[1].split('\n};', 1)[0]
combat, convenience = registration.split('#else', 1)
for slot, original in slots:
    entry = f'{{0x{slot:X},0x{original:X},'
    ok(entry not in combat, f'combat owns no terrain slot {slot:X}')
    ok(convenience.count(entry) == 1, f'one convenience owner {slot:X}')
    ok(any(entry in line and 'kPointerTerrain' in line for line in convenience.splitlines()), 'terrain switch owns slot')
for slot, original in removed_slots:
    ok(f'{{0x{slot:X},' not in registration, f'removed city menu slot {slot:X} has no publisher')
    ok(struct.unpack('<Q', read(slot, 8))[0] == base+original, f'original unowned city slot {slot:X}')
ok(read(0x710C50, 7) == bytes.fromhex('48 8b 81 a0 04 00 00'), 'native membership reads state area list')
ok(read(0x710C90, 9) == bytes.fromhex('48 3b 10 74 0c 48 8b 40 08'), 'native membership compares area and follows next node')
ok('459730' not in source and 'prepare_terrain' not in source and 'TerrainCode' not in source,
   'unused map entry hook, trampoline and publisher removed')
ok('map_click' not in ui and 'map_entry_bytes' not in ui and 'map_call_bytes' not in ui,
   'unused map callback and fingerprints removed')
ok('alignas(16) static unsigned char hex_completion[0x500]={};' in ui and
   'functor.slots[1]=reinterpret_cast<uint64_t>(hex_completion)' in ui, 'DLL static aligned zero completion buffer')
ok('ms+0x478' not in ui and 'static void open_hex_menu(Obj tile)' in ui, 'opener never writes native state completion field')
native = (P / 'src/native_terrain_edit.inl').read_text(encoding='utf-8-sig')
save = (P / 'src/native_terrain_edit_save.inl').read_text(encoding='utf-8-sig')
for removed in ['begin_development', 'end_development', 'terrain_dev_mode', 'picker_city', 'expanded_city']:
    ok(removed not in native+ui+save+source, f'removed mode and city extension: {removed}')
ok('0x732FC9' not in ui and '0x732C60' not in ui and '0x7AA610' not in ui, 'city wrapper, caller and fingerprints removed')
ok('state && tile && valid(tile) && allowed(tile[0x10])' in ui and '!occupied(hex)' in ui,
   'regional gate requires allowed valid unoccupied tile')
ok('fn<int(__fastcall *)(Obj,Obj)>(0x710C50)(state,area)!=0' in ui and
   'fn<int(__fastcall *)(int)>(0x17B390)(0x3A)==0' in ui, 'native membership and input gate')
ok('target && target==opening_tile' in ui, 'only recorded opening tile expands native hex state')
ok('reset_ui_context();' in save, 'load cleanup retains only transient picker state')
ok('#ifndef NO_RUNTIME_LOG\n    if(hex==HexCount && tile)hex=tile_hex(tile);\n    log_event("terrain_domestic_click"' in ui, 'regional diagnostic removed in release')
ok('if(f.terrain_edit && !terrain_edit_guards_match())split_disable(f.terrain_edit,27);' in source, 'isolated guard disable')
adapter = (P / 'src/native_adapter.cpp').read_text(encoding='utf-8-sig')
import registry_text as R   # W5: includes come from the generated per-DLL lists
ok(R.hosts('native_terrain_edit_ui.inl')==['convenience'], 'UI excluded from combat compilation')
ok('feature_key(config, L"real_estate", L"terrain_edit", false)' in adapter, 'runtime config section')
# User/Claude 2026-10-29: two-step picker within native 8/3 row limits.
picker = re.search(r'picker_entries\[\]=\{(.*?)\n\};', ui, re.S).group(1)
rows = [(int(g), int(c), label, int(t)) for g, c, label, t in
        re.findall(r'\{(\d+),(\d+),L"([^"]+)",(\d+)\}', picker)]
ok(len(rows) == 26, 'picker has 8 top rows and 18 leaves')
ok([(g, c) for g, c, _, _ in rows[:8]] == [(g, 0) for g in range(8)], 'eight top rows first')
ok([label for _, _, label, _ in rows[:8]] == ['평지류', '습지', '숲', '산', '험지', '물', '급류', '닫기'], 'exact top group names')
ok(all(0 <= g < 8 and 0 <= c <= 3 for g, c, _, _ in rows), 'all rows within native 8/3 limits')
for g in range(8):
    children = [c for group, c, _, _ in rows if group == g and c]
    ok(len(children) <= 3 and children == list(range(1, len(children) + 1)), f'group {g} has at most 3 contiguous children')
ok([t for _, _, _, t in rows[8:]] == [1, 2, 3, 4, 7, 5, 6, 8, 9, 10, 11, 19, 20, 12, 13, 14, 21, 24], 'exact leaf terrain order')
ok('out[13]' not in ui and 'return 14;' not in ui, 'no city menu appended row')
ok('static_assert(' in ui and 'picker_rows_valid()' in ui, 'compile-time picker row guard retained')
ok('지형개발' not in ui and 'action_stop' not in ui and 'actions[i]=static_cast<int>(e.terrain)' in ui, 'groups and Close have no terrain action without a mode')
ok('dword(state,0x4B8)==0x54454431' in ui, 'terrain picker opens only in the advisor-opened city state (2026-11-08 user)')
note = '군사의 열흘 보고 창에 「부동산 개발」 버튼을 추가합니다. 보유한 성 목록에서 성을 고르면 그 성의 지역내정으로 들어가고, 땅을 클릭해 바닥·높이·나무·절벽·수면을 무료로 바꿀 수 있습니다. 일반 지역내정의 땅클릭은 원작 그대로이며, 이 버튼으로 들어왔을 때만 지형 변경이 열립니다. 끄면 버튼과 지형 변경이 모두 사라집니다.'
for rel in ['installer/InstallerGui.cs', 'distribution/FeatureSelect.ps1', 'FeatureSelect.ps1']:
    text = (P / rel).read_text(encoding='utf-8-sig')
    line = next(line for line in text.splitlines() if 'terrain_edit' in line)
    ok('real_estate' in line and '게임성' in line and note in line, rel + ' catalog')
contract = next(json.loads(line) for line in (P / 'research/ui_atlas/contracts.jsonl').read_text(encoding='utf-8').splitlines()
                if json.loads(line)['id'] == 'city.menu')
ok(contract['facts']['record_stride'] == 32 and contract['facts']['create_slot'] == 0x188, 'retained menu contract')
# The installed Steam EXE has encrypted .text. Verify its identity and unencrypted
# slots independently; instruction checks above use the matching offline decoded image.
installed_exe = P.parent / 'SAN14CE_KO.exe'
installed_data = installed_exe.read_bytes()
installed_sha256 = hashlib.sha256(installed_data).hexdigest()
ok(installed_sha256 in ('87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a', '782cf229a56cdde2b95399e0e7536740b3f5e96bea702679365e4dad061abdce'), 'current installed EXE identity')
pe2 = struct.unpack_from('<I', installed_data, 60)[0]
table2 = pe2 + 24 + struct.unpack_from('<H', installed_data, pe2 + 20)[0]
sections2 = [struct.unpack_from('<IIII', installed_data, table2 + i*40 + 8)
             for i in range(struct.unpack_from('<H', installed_data, pe2+6)[0])]
def installed_read(rva, length):
    for _, va, size, raw in sections2:
        if va <= rva and rva + length <= va + size:
            return installed_data[raw+rva-va:raw+rva-va+length]
    raise ValueError(hex(rva))
for slot, original in slots + removed_slots:
    ok(struct.unpack('<Q', installed_read(slot, 8))[0] == base+original, f'installed EXE slot {slot:X}')
ok(installed_read(0x13DBA58, 40) == read(0x13DBA58, 40), 'installed EXE functor vtable agrees with decoded image')
record = dict(terrain_edit_static_checks=checks, failed=0, exe=str(EXE), sha256=digest,
              original_slots=[dict(slot=hex(s), original=hex(o)) for s, o in slots],
              installed_exe=str(installed_exe), installed_sha256=installed_sha256,
              instruction_evidence="matching offline decoded EXE; installed .text is encrypted", live_verified=False)
out = P / 'build/development/terrain_edit/static_result.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(record, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(json.dumps(record, ensure_ascii=False))
