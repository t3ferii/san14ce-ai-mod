"""Compare real CMD apply/restore in isolated folders with the full layout index."""
from pathlib import Path
import base64, hashlib, json, shutil, struct, subprocess, time, zipfile

p = Path(__file__).resolve().parents[1]
scratch = p/'build/installer_speed'
share = json.loads((p/'build/share_package.json').read_text(encoding='utf-8'))
patch = json.loads((p/'build/input_package/traits9_layout.json').read_text(encoding='utf-8'))
raw = (scratch/'layout.snapshot.rdb').read_bytes()
head = struct.unpack_from('<I', raw, 8)[0]
originals = {r['ktid']:base64.b64decode(r['original_entry']) for r in patch['records']}
parts = [raw[:head]]
at = head
while at < len(raw):
    size = struct.unpack_from('<I', raw, at+8)[0]
    ktid = struct.unpack_from('<I', raw, at+36)[0]
    entry = originals.get(ktid, raw[at:at+size])
    parts.extend((entry, bytes((-len(entry)) % 4)))
    at += (size+3) & ~3
original = b''.join(parts)
results = {}
for label in ('before', 'after'):
    game = scratch/('cmd_'+label)
    assert not game.exists(), f'Use a fresh fixture path: {game}'
    game.mkdir()
    with zipfile.ZipFile(share['zip']) as z:
        z.extractall(game)
    shutil.copyfile(p.parent/'SAN14CE_KO.exe', game/'SAN14CE_KO.exe')
    (game/'0002').mkdir()
    index = game/'0002/ScreenLayout.rdb'
    index.write_bytes(original)
    manager = scratch/'SplitManage.before.ps1' if label == 'before' else p/'distribution/SplitManage.ps1'
    (game/'AI_TacticsMod/SplitManage.ps1').write_text(manager.read_text(encoding='utf-8-sig'),encoding='utf-8-sig')
    times = {}
    for action in ('Apply', 'Apply', 'Restore'):
        cmd = 'AI모드_적용.cmd' if action == 'Apply' else 'AI모드_해제_원본복구.cmd'
        started = time.perf_counter()
        run = subprocess.run(['cmd.exe','/d','/c','call',str(game/cmd),'--no-pause'],cwd=game,capture_output=True,timeout=90)
        elapsed = time.perf_counter()-started
        if run.returncode:
            raise AssertionError(run.stdout.decode('utf-8',errors='replace')+run.stderr.decode('utf-8',errors='replace'))
        key = 'repeat_apply_s' if action == 'Apply' and 'apply_s' in times else action.lower()+'_s'
        times[key] = elapsed
        if action == 'Apply':
            manifest = json.loads((game/'AI_TacticsMod/build/input_package/manifest.json').read_text(encoding='utf-8'))
            for name, expected in manifest['split_modules']['files'].items():
                assert hashlib.sha256((game/name).read_bytes()).hexdigest() == expected
            times['patched_index_sha256'] = hashlib.sha256(index.read_bytes()).hexdigest()
        else:
            assert index.read_bytes() == original, 'Restore must reproduce original full layout'
            assert not (game/'dinput8.dll').exists()
            assert not (game/'SAN14_CombatAI.dll').exists()
            assert not (game/'SAN14_Convenience.dll').exists()
            assert not (game/'0002/ScreenLayout.rdb.bin30').exists()
    assert (game/'SAN14CE_KO.exe').read_bytes() == (p.parent/'SAN14CE_KO.exe').read_bytes()
    results[label] = times
assert results['before']['patched_index_sha256'] == results['after']['patched_index_sha256']
results['status'] = 'installer_speed_verified'
results['actual_game_modified'] = False
(scratch/'cmd-timings.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
print(json.dumps(results,indent=2))
