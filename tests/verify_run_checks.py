#!/usr/bin/env python
"""Self-test of tools/run_checks.py (docs/MODULARIZATION_PLAN.md W3-2/3-3): step graph, CPU budget, locks, exclusivity,
fail-fast, save, cache record, runner/args compatibility and cl command assembly. Writes only build/development/run_checks_selftest/."""
import json
import os
import shutil
import stat
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(PROJECT, 'tools'))
import run_checks as rc  # noqa: E402

BASE = os.path.join(PROJECT, 'build', 'development', 'run_checks_selftest')
passed, failed = 0, []


def check(name, cond, detail=''):
    global passed
    if cond:
        passed += 1
        print('PASS ' + name)
    else:
        failed.append(name)
        print('FAIL %s %s' % (name, detail))


def rmtree(path):
    def onerr(fn, p, exc):
        try:
            os.chmod(p, stat.S_IWRITE)
            fn(p)
        except OSError:
            pass
    if os.path.isdir(path):
        shutil.rmtree(path, onerror=onerr)


def write(rel, text):
    p = os.path.join(BASE, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)


def events():
    out = []
    d = os.path.join(BASE, 'build', 'ev')      # one file per event: concurrent appends to one file may interleave
    if os.path.isdir(d):
        for fn in os.listdir(d):
            kind, name = fn.rsplit('.', 1)[0].split('__')
            out.append((float(open(os.path.join(d, fn)).read()), kind, name))
    return sorted(out)


def max_overlap(group=None):
    cur, best = 0, 0
    for _, kind, name in events():
        if group and name not in group:
            continue
        cur += 1 if kind == 'start' else -1
        best = max(best, cur)
    return best


def spans(name):
    t = {k: tm for tm, k, n in events() if n == name}
    return t.get('start'), t.get('end')


STEP = r'''
import os, sys, time
name, secs, code = sys.argv[1], float(sys.argv[2]), int(sys.argv[3])
ev = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'build', 'ev')
os.makedirs(ev, exist_ok=True)
def log(kind):
    with open(os.path.join(ev, '%s__%s.txt' % (kind, name)), 'w') as f:
        f.write('%.4f' % time.time())
log('start')
time.sleep(secs)
log('end')
print('line-a')
print('result-%s' % name)
sys.exit(code)
'''


def step(name, secs=0.5, code=0, **kw):
    d = {'id': name, 'py': 'tests/step.py', 'args': [name, str(secs), str(code)]}
    d.update(kw)
    return d


def setup(routes, always=()):
    rmtree(BASE)
    write('tests/step.py', STEP)
    write('tools/build_split.ps1', "$taskVc='C:\\VC\\14'\n$taskSdk='C:\\SDK';$taskVersion='10.0.1'\n$taskFlags=@('/nologo','/W4','/O2')\n"
          "& '.\\split_module_fixture.exe' (Join-Path $taskProject 'legacy\\img\\text.bin')\n")
    write('development/features.json', json.dumps({'schema': 2, 'always': list(always), 'routes': routes, 'modules': {}}))


def run(routes, *extra, root=None):
    p = subprocess.run([sys.executable, '-B', '-X', 'utf8', os.path.join(PROJECT, 'tools', 'run_checks.py'), '--root', root or BASE,
                        '--routes', ','.join(routes)] + list(extra), capture_output=True, text=True, encoding='utf-8', errors='replace')
    return p.returncode, p.stdout + p.stderr


# 1. sequential default, needs graph
setup({'seq': {'parallel_safe': True, 'steps': [step('s1', 0.3), step('s2', 0.3)]},
       'dag': {'parallel_safe': True, 'steps': [step('d1', 0.3, needs=[]), step('d2', 0.3, needs=[]), step('d3', 0.1, needs=['d1', 'd2'])]}})
code, out = run(['seq'])
a, b = spans('s1'), spans('s2')
check('default needs = previous step (sequential)', code == 0 and a[1] <= b[0] + 0.001, out[-300:])
code, out = run(['dag'])
check('needs graph: d3 waits for d1 and d2, d1/d2 overlap', code == 0 and spans('d3')[0] >= max(spans('d1')[1], spans('d2')[1]) - 0.001 and max_overlap() >= 2, out[-300:])
check('success output is the last two lines only', 'result-d3' in out and out.count('line-a') <= 3)

# 2. CPU budget: never more than --jobs, never more than 4
for jobs, expect, count in (('2', 2, 5), ('9', 4, 8)):
    setup({'wide': {'parallel_safe': True, 'steps': [step('w%d' % i, 0.9, needs=[]) for i in range(count)]}})
    code, out = run(['wide'], '--jobs', jobs)
    check('--jobs %s runs at most %d at once and uses all of them' % (jobs, expect), code == 0 and max_overlap() == expect, 'overlap=%d' % max_overlap())

# 3. locks serialise steps; different locks and no lock still overlap
setup({'locks': {'parallel_safe': True, 'steps': [step('l1', 0.4, needs=[], lock='x'), step('l2', 0.4, needs=[], lock='x'), step('l3', 0.4, needs=[], lock='y'),
                                                  step('l4', 0.4, needs=[])]}})
code, out = run(['locks'])
check('lock x never overlaps', code == 0 and max_overlap({'l1', 'l2'}) == 1, out[-200:])
check('other locks and unlocked steps overlap', max_overlap({'l1', 'l3', 'l4'}) >= 2)

# 4. exclusive route (parallel_safe false) runs alone
setup({'solo': {'steps': [step('x1', 0.5)]}, 'par1': {'parallel_safe': True, 'steps': [step('p1', 0.5)]}, 'par2': {'parallel_safe': True, 'steps': [step('p2', 0.5)]}})
code, out = run(['par1', 'solo', 'par2'])
x = spans('x1')
others = [spans('p1'), spans('p2')]
check('route without parallel_safe never overlaps another', code == 0 and all(o[1] <= x[0] + 0.001 or o[0] >= x[1] - 0.001 for o in others), out[-300:])
check('parallel_safe routes still share the pool', max_overlap({'p1', 'p2'}) == 2)

# 5. fail-fast: later steps of the failing route and not-started routes are skipped, exit 1, output shown
setup({'bad': {'parallel_safe': True, 'steps': [step('b1', 0.1, code=3), step('b2', 0.1)]},
       'later': {'parallel_safe': True, 'steps': [step('z1', 0.1)]}})
code, out = run(['bad', 'later'], '--jobs', '1')
names = {n for _, _, n in events()}
check('failing step: exit 1, FAIL line with its output, rest of route skipped', code == 1 and 'FAIL' in out and 'result-b1' in out and 'b2' not in names, out[-300:])
check('fail-fast starts nothing new after the failure (jobs=1)', 'z1' not in names or 'bad' in out)
setup({'bad': {'parallel_safe': True, 'steps': [step('b1', 0.1, code=3), step('b2', 0.1)]},
       'later': {'parallel_safe': True, 'steps': [step('z1', 0.1)]}})
code, out = run(['bad', 'later'], '--jobs', '1', '--keep-going')
check('--keep-going still runs other routes', code == 1 and 'z1' in {n for _, _, n in events()}, out[-300:])

# 6. exe step with save (last stdout line, no BOM), cwd, runner/args compatibility
setup({'ex': {'parallel_safe': True, 'steps': [{'id': 'e', 'exe': sys.executable, 'args': ['-c', 'print("first"); print("{\\"ok\\": 1}")'], 'cwd': 'build/sub', 'save': 'build/out.json'}]},
       'old': {'parallel_safe': True, 'runner': 'tests/step.py', 'args': ['old1', '0.1', '0'], 'python': True}})
code, out = run(['ex', 'old'])
sv = os.path.join(BASE, 'build', 'out.json')
check('exe step: cwd created, last line saved without BOM', code == 0 and os.path.isfile(sv) and open(sv, 'rb').read() == b'{"ok": 1}', out[-300:])
check('runner/args entry runs as one python step', 'old1' in {n for _, _, n in events()})
check('steps_of: runner without .py and no python key becomes a ps1 step',
      rc.steps_of('r', {'runner': 'tools/x.ps1', 'args': ['-A']})[0].get('ps1') == 'tools/x.ps1')

# 7. cache record only after a pass (keys come from --plan), through select_checks.py
setup({'good': {'parallel_safe': True, 'cache': True, 'steps': [step('g1', 0.1)]}, 'broken': {'parallel_safe': True, 'cache': True, 'steps': [step('k1', 0.1, code=1)]}})
plan = os.path.join(BASE, 'plan.json')
json.dump({'routes': [{'name': 'good', 'key': ''}, {'name': 'broken', 'key': ''}]}, open(plan, 'w'))
code, out = run(['good', 'broken'], '--plan', plan, '--keep-going')
cdir = os.path.join(BASE, 'build', 'development', 'check_cache')
check('record: passing route cached, failing route not', os.path.isfile(os.path.join(cdir, 'good.json')) and not os.path.isfile(os.path.join(cdir, 'broken.json')), out[-300:])

# 8. cl command assembly: flags/paths from build_split.ps1, separate object folder, absolute cl.exe
setup({'c': {'parallel_safe': True, 'steps': [{'id': 'c1', 'cwd': 'build/ls', 'cl': {'flags': 'split', 'defs': ['/DX'], 'inc': ['.', 'src'], 'src': ['src/a.cpp'], 'out': 'A.dll', 'dll': True, 'link': ['kernel32.lib']}},
                                              {'id': 'r1', 'exe': 'build/ls/a.exe', 'args': ['@split_image']}]}})
data = rc.load_routes(BASE)
st = rc.steps_of('c', data['routes']['c'])
argv, env, cwd = rc.build_command(BASE, st[0], {})
check('cl: absolute cl.exe from build_split.ps1, split flags, defs, sources',
      argv[0] == 'C:\\VC\\14\\bin\\Hostx64\\x64\\cl.exe' and argv[1:4] == ['/nologo', '/W4', '/O2'] and '/DX' in argv and os.path.join(BASE, 'src', 'a.cpp') in argv, str(argv))
check('cl: own object folder, /Fe, /LD and /link tail', '/Foobj\\A\\' in argv and '/FeA.dll' in argv and '/LD' in argv and argv[-2:] == ['/link', 'kernel32.lib'], str(argv))
check('cl: environment carries INCLUDE/LIB/PATH of the toolchain', 'C:\\VC\\14\\include' in env['INCLUDE'] and 'C:\\SDK\\Lib\\10.0.1\\um\\x64' in env['LIB'] and env['PATH'].startswith('C:\\VC\\14\\bin'))
check('cl: object folder created under the step cwd', os.path.isdir(os.path.join(BASE, 'build', 'ls', 'obj', 'A')))
argv2, _, _ = rc.build_command(BASE, st[1], {})
check('@split_image resolves to the path build_split.ps1 passes to the fixture exe', argv2[-1] == os.path.join(BASE, 'legacy', 'img', 'text.bin'), str(argv2))

# 9. data validation
try:
    rc.steps_of('x', {'steps': [{'id': 'a', 'py': 'p'}, {'id': 'a', 'py': 'q'}]})
    check('duplicate step ids rejected', False)
except ValueError:
    check('duplicate step ids rejected', True)
try:
    rc.steps_of('x', {'steps': [{'id': 'a', 'py': 'p', 'needs': ['nope']}]})
    check('unknown needs rejected', False)
except ValueError:
    check('unknown needs rejected', True)

# 10. the real manifest: every route has steps or a runner, step graphs are valid, cl/py/ps1 sources exist
with open(os.path.join(PROJECT, 'development', 'features.json'), encoding='utf-8') as f:
    real = json.load(f)
bad = []
for name, meta in real['routes'].items():
    try:
        for s in rc.steps_of(name, meta):
            for key in ('py', 'ps1'):
                if s.get(key) and not os.path.isfile(os.path.join(PROJECT, s[key])):
                    bad.append('%s: %s missing' % (name, s[key]))
            for src in (s.get('cl') or {}).get('src', []):
                if not os.path.isfile(os.path.join(PROJECT, src)):
                    bad.append('%s: %s missing' % (name, src))
    except ValueError as e:
        bad.append(str(e))
check('real features.json: all %d routes have valid step graphs and existing scripts' % len(real['routes']), not bad, '; '.join(bad[:5]))
check('real features.json: every route declares parallel_safe', all('parallel_safe' in m for m in real['routes'].values()))
check('real build_split.ps1 yields toolchain and fixture image', rc.toolchain(PROJECT)['flags'][:1] == ['/nologo'] and rc.split_image(PROJECT).endswith('.bin'))

rmtree(BASE)
print(json.dumps({'status': 'run_checks_selftest_passed' if not failed else 'run_checks_selftest_failed', 'checks': passed, 'failed': failed}))
sys.exit(1 if failed else 0)
