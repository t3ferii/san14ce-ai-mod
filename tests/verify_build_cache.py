"""Self test of tools/cl_cache.py (compile cache of tools/build_split.ps1) and tools/run_checks.py result_of (2026-11-16).

A cached compile must be skipped only when the key, every recorded dependency (content) and every output are unchanged;
an empty or source-less dependency list must never be cached. Synthetic files only; no compiler needed.
"""
import json, os, shutil, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CC = os.path.join(ROOT, 'tools', 'cl_cache.py')
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import run_checks  # noqa: E402

fails = []


def check(name, cond):
    if not cond:
        fails.append(name)


def cc(cmd, jobs, d):
    p = os.path.join(d, 'jobs.json')
    with open(p, 'w', encoding='utf-8') as f:
        json.dump(jobs, f)
    r = subprocess.run([sys.executable, '-B', '-X', 'utf8', CC, cmd, p], capture_output=True, text=True)
    return r.returncode, r.stdout.strip()


d = tempfile.mkdtemp(prefix='cl_cache_selftest_')
try:
    src, hdr, out, deps = (os.path.join(d, n) for n in ('a.cpp', 'a.h', 'a.dll', 'deps'))
    os.makedirs(deps)
    for p, t in ((src, '#include "a.h"\n'), (hdr, 'int f(){return 1;}\n'), (out, 'binary')):
        with open(p, 'w') as f:
            f.write(t)
    with open(os.path.join(deps, 'a.cpp.json'), 'w') as f:
        json.dump({'Version': '1.2', 'Data': {'Source': src, 'Includes': [hdr]}}, f)
    job = {'stamp': os.path.join(d, 'stamp.json'), 'key': 'cl /O2', 'outputs': [out], 'deps': deps, 'sources': [src], 'ok': True}
    check('no stamp is stale', cc('fresh', [job], d) == (0, '[false]'))
    check('store', cc('store', [job], d)[0] == 0)
    check('stored job is fresh', cc('fresh', [job], d) == (0, '[true]'))
    with open(hdr, 'w') as f:
        f.write('int f(){return 2;}\n')
    check('header content change is stale', cc('fresh', [job], d) == (0, '[false]'))
    time.sleep(0.05)
    with open(hdr, 'w') as f:
        f.write('int f(){return 1;}\n')
    check('same content with a new mtime is fresh', cc('fresh', [job], d) == (0, '[true]'))
    check('other key is stale', cc('fresh', [dict(job, key='cl /Od')], d) == (0, '[false]'))
    with open(out, 'w') as f:
        f.write('tampered')
    check('changed output is stale', cc('fresh', [job], d) == (0, '[false]'))
    os.remove(os.path.join(deps, 'a.cpp.json'))
    with open(os.path.join(deps, 'b.json'), 'w') as f:
        json.dump({'Data': {'Source': os.path.join(d, 'other.cpp'), 'Includes': []}}, f)
    check('dependency list without the source is refused', cc('store', [dict(job, stamp=os.path.join(d, 's2.json'))], d)[0] != 0)
    gate = {'stamp': os.path.join(d, 'gate.json'), 'files': [hdr, out], 'ok': True}
    check('gate without stamp is stale', cc('files-fresh', [gate], d) == (0, '[false]'))
    cc('files-store', [gate], d)
    check('gate with same files is fresh', cc('files-fresh', [gate], d) == (0, '[true]'))
    with open(out, 'w') as f:
        f.write('rebuilt')
    check('gate with a changed binary is stale', cc('files-fresh', [gate], d) == (0, '[false]'))
finally:
    shutil.rmtree(d, ignore_errors=True)

check('result_of last line', run_checks.result_of(['x', '{"checks": 3}', '']) == '{"checks": 3}')
check('result_of multi-line JSON', json.loads(run_checks.result_of(['log', '{', '  "checks": 7,', '  "status": "ok"', '}'])) == {'checks': 7, 'status': 'ok'})
check('result_of empty', run_checks.result_of(['', ' ']) == '')
print(json.dumps({'status': 'build_cache_selftest_passed' if not fails else 'build_cache_selftest_failed', 'checks': 14, 'failures': fails}))
sys.exit(1 if fails else 0)
