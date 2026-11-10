#!/usr/bin/env python
"""Data-driven check runner with a CPU budget (docs/MODULARIZATION_PLAN.md W3-2/3-3).

  run_checks.py --routes a,b,c [--jobs 4] [--plan plan.json] [--priority belownormal|idle|normal] [--keep-going] [--verbose] [--list]

Reads development/features.json routes.<name>.steps (or the older runner/args pair). A step is one process:
  {"py": "tests/x.py", "args": [..]}                       python -B -X utf8
  {"ps1": "tools/x.ps1", "args": [..]}                     powershell -NoProfile -ExecutionPolicy Bypass -File
  {"exe": "build/x.exe", "args": [..], "cwd": "build", "save": "build/x.json"}   (save = last stdout line, UTF-8 without BOM)
  {"cl": {"flags": "split" | [..], "defs": [..], "inc": [..], "src": [..], "out": "x.dll", "link": [..]}, "cwd": "build/development/x"}
Step keys: id, prio (higher starts first inside a route class, default 0), needs (step ids of the same route; default = the previous step, [] = no wait), lock (steps sharing a lock never run
together, e.g. two compiles of the same object name), cwd (relative to the project).
parallel_safe (route key, default false): only routes marked true share the pool; any other route runs alone (nothing else starts while
one of its steps runs), because its outputs or processes were not verified to be independent.
Scheduling: every step of every selected route is one task in a single pool of --jobs slots (default min(4, cpus)), so the whole run
never uses more than that many processes. Ready tasks are ordered always-routes first, then the longest estimate first. Children run at
BELOW_NORMAL priority (--priority idle for IDLE). A failed step skips the rest of its route; unless --keep-going no new task starts
after the first failure. A route that passes is recorded for the cache through select_checks.py record (keys from --plan).
Standard library only. Writes only what the steps write (build/...).
"""
import argparse
import concurrent.futures as cf
import json
import os
import re
import subprocess
import sys
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
PRIORITY = {'belownormal': 0x00004000, 'idle': 0x00000040, 'normal': 0}
TOOLCHAIN_FILE = 'tools/build_split.ps1'      # single source of the MSVC paths and of the split build flags


def load_routes(root):
    with open(os.path.join(root, 'development', 'features.json'), encoding='utf-8') as f:
        return json.load(f)


def steps_of(route_name, meta):
    """Normalised step list [{id, needs, ...}] of a route entry (steps, else runner/args)."""
    raw = meta.get('steps')
    if raw is None and meta.get('runner'):
        runner = meta['runner']
        key = 'py' if (meta.get('python') or runner.endswith('.py')) else 'ps1'
        raw = [{key: runner, 'args': list(meta.get('args') or [])}]
    if not raw:
        raise ValueError('route %s has no steps or runner' % route_name)
    out, prev = [], None
    for i, s in enumerate(raw):
        s = dict(s)
        s.setdefault('id', 's%d' % i)
        if 'needs' not in s:
            s['needs'] = [prev] if prev else []
        prev = s['id']
        out.append(s)
    ids = [s['id'] for s in out]
    if len(set(ids)) != len(ids):
        raise ValueError('route %s: duplicate step ids' % route_name)
    for s in out:
        for n in s['needs']:
            if n not in ids:
                raise ValueError('route %s: step %s needs unknown step %s' % (route_name, s['id'], n))
    return out


def toolchain(root):
    """MSVC/SDK paths and the split build flags, read from tools/build_split.ps1."""
    with open(os.path.join(root, TOOLCHAIN_FILE), encoding='utf-8-sig') as f:
        text = f.read()
    def one(name):
        m = re.search(r"\$%s\s*=\s*'([^']+)'" % name, text)
        if not m:
            raise ValueError('%s: $%s not found' % (TOOLCHAIN_FILE, name))
        return m.group(1)
    m = re.search(r"\$taskFlags\s*=\s*@\(([^)]*)\)", text)
    if not m:
        raise ValueError('%s: $taskFlags not found' % TOOLCHAIN_FILE)
    flags = re.findall(r"'([^']*)'", m.group(1))
    vc, sdk, ver = one('taskVc'), one('taskSdk'), one('taskVersion')
    env = {
        'PATH': os.path.join(vc, 'bin', 'Hostx64', 'x64') + os.pathsep + os.environ.get('PATH', ''),
        'INCLUDE': ';'.join([os.path.join(vc, 'include')] + [os.path.join(sdk, 'Include', ver, d) for d in ('ucrt', 'shared', 'um')]),
        'LIB': ';'.join([os.path.join(vc, 'lib', 'x64')] + [os.path.join(sdk, 'Lib', ver, d, 'x64') for d in ('ucrt', 'um')]),
    }
    return {'cl': os.path.join(vc, 'bin', 'Hostx64', 'x64', 'cl.exe'), 'env': env, 'flags': flags}


def split_image(root):
    """Captured text image of the split fixtures: the path build_split.ps1 passes to split_module_fixture.exe (one source of truth)."""
    with open(os.path.join(root, TOOLCHAIN_FILE), encoding='utf-8-sig') as f:
        m = re.search(r"split_module_fixture\.exe'\s+\(Join-Path \$taskProject '([^']+)'\)", f.read())
    if not m:
        raise ValueError('%s: fixture image path not found' % TOOLCHAIN_FILE)
    return os.path.join(root, m.group(1).replace('\\', os.sep))


class Task:
    def __init__(self, route, step, est, always, excl):
        self.route, self.step, self.est, self.always, self.excl = route, step, est, always, excl
        self.id = (route, step['id'])
        self.lock = step.get('lock')
        self.state = 'wait'      # wait run done fail skip
        self.out = ''
        self.secs = 0.0


def build_command(root, step, tc_cache):
    """(argv, env, cwd) of a step."""
    env = None
    cwd = os.path.join(root, step['cwd'].replace('/', os.sep)) if step.get('cwd') else root
    args = [split_image(root) if str(a) == '@split_image' else str(a) for a in step.get('args') or []]
    if 'py' in step:
        return [sys.executable, '-B', '-X', 'utf8', os.path.join(root, step['py'].replace('/', os.sep))] + args, env, cwd
    if 'ps1' in step:
        return ['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', os.path.join(root, step['ps1'].replace('/', os.sep))] + args, env, cwd
    if 'exe' in step:
        return [os.path.join(root, step['exe'].replace('/', os.sep))] + args, env, cwd
    if 'cl' in step:
        if 'tc' not in tc_cache:
            tc_cache['tc'] = toolchain(root)
        tc = tc_cache['tc']
        c = step['cl']
        flags = tc['flags'] if c.get('flags') == 'split' else list(c.get('flags') or [])
        objdir = os.path.join('obj', os.path.splitext(c['out'])[0]) + os.sep
        os.makedirs(os.path.join(cwd, objdir), exist_ok=True)
        argv = [tc['cl']] + flags + list(c.get('defs') or [])
        argv += ['/I' + (cwd if i == '.' else os.path.join(root, i.replace('/', os.sep))) for i in c.get('inc') or []]
        argv += [os.path.join(root, x.replace('/', os.sep)) for x in c['src']]
        argv += ['/Fo' + objdir, '/Fe' + c['out']]
        if c.get('dll'):
            argv.append('/LD')
        argv += ['/link'] + list(c.get('link') or [])
        e = dict(os.environ)
        e.update(tc['env'])
        return argv, e, cwd
    raise ValueError('step %s has no py/ps1/exe/cl' % step['id'])


def run_task(root, t, priority, tc_cache):
    t0 = time.time()
    try:
        argv, env, cwd = build_command(root, t.step, tc_cache)
        os.makedirs(cwd, exist_ok=True)
        p = subprocess.run(argv, cwd=cwd, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           creationflags=PRIORITY[priority] if os.name == 'nt' else 0)
        text = p.stdout.decode('utf-8', 'replace')
        rc = p.returncode
        if rc == 0 and t.step.get('save'):
            lines = [l for l in text.splitlines() if l.strip()]
            path = os.path.join(root, t.step['save'].replace('/', os.sep))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w', encoding='utf-8', newline='') as f:
                f.write(lines[-1] if lines else '')
    except Exception as e:  # noqa: BLE001 - reported as a step failure
        text, rc = 'runner error: %s' % e, 99
    t.out = text
    t.secs = time.time() - t0
    return rc


def make_tasks(data, names):
    routes = data.get('routes') or {}
    always = set(data.get('always') or [])
    tasks = []
    for n in names:
        meta = routes.get(n)
        if meta is None:
            raise ValueError('unknown route: %s' % n)
        est = float(meta.get('est_s') or 30)
        for s in steps_of(n, meta):
            tasks.append(Task(n, s, est, n in always, not meta.get('parallel_safe')))
    return tasks


def schedule(root, tasks, jobs, priority, keep_going, verbose, on_route_done):
    """Run the task graph; returns (failed routes list, per-route seconds)."""
    by_id = {t.id: t for t in tasks}
    route_tasks = {}
    for t in tasks:
        route_tasks.setdefault(t.route, []).append(t)
    route_t0, route_secs, failed = {}, {}, []
    locks, tc_cache, stop = set(), {}, [False]
    cv = threading.Lock()
    running = {}

    def ready(t):
        return t.state == 'wait' and all(by_id[(t.route, n)].state == 'done' for n in t.step['needs'])

    def pick():
        busy = list(running.values())
        if any(x.excl for x in busy):
            return None
        cand = [t for t in tasks if ready(t) and (not t.lock or t.lock not in locks) and not (t.excl and busy)]
        cand.sort(key=lambda t: (0 if t.always else 1, -t.step.get('prio', 0), -t.est, t.route, t.step['id']))
        return cand[0] if cand else None

    def finish_route(r):
        ts = route_tasks[r]
        if all(x.state in ('done', 'fail', 'skip') for x in ts) and r not in route_secs:
            end = max((x.secs for x in ts), default=0)
            route_secs[r] = round(time.time() - route_t0.get(r, time.time()), 1)
            if all(x.state == 'done' for x in ts):
                on_route_done(r, route_secs[r])
            return end

    with cf.ThreadPoolExecutor(max_workers=jobs) as ex:
        while True:
            while len(running) < jobs and not stop[0]:
                t = pick()
                if t is None:
                    break
                t.state = 'run'
                if t.lock:
                    locks.add(t.lock)
                route_t0.setdefault(t.route, time.time())
                running[ex.submit(run_task, root, t, priority, tc_cache)] = t
            if not running:
                break
            done, _ = cf.wait(list(running), return_when=cf.FIRST_COMPLETED)
            for fu in done:
                t = running.pop(fu)
                rc = fu.result()
                if t.lock:
                    locks.discard(t.lock)
                label = '%s/%s' % t.id
                lines = t.out.splitlines()
                if rc == 0:
                    t.state = 'done'
                    shown = lines if verbose else [l for l in lines if l.strip()][-2:]
                    print('[ok %5.1fs] %s' % (t.secs, label))
                    for l in shown:
                        print('    ' + l[:300])
                else:
                    t.state = 'fail'
                    print('[FAIL %5.1fs rc=%s] %s' % (t.secs, rc, label))
                    for l in lines[-120:]:
                        print('    ' + l[:400])
                    if t.route not in failed:
                        failed.append(t.route)
                    for x in route_tasks[t.route]:
                        if x.state == 'wait':
                            x.state = 'skip'
                    if not keep_going:
                        stop[0] = True
                sys.stdout.flush()
                finish_route(t.route)
    for r in route_tasks:
        for x in route_tasks[r]:
            if x.state == 'wait':
                x.state = 'skip'
        finish_route(r)
    return failed, route_secs


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--root', default=os.path.dirname(HERE))
    ap.add_argument('--routes', required=True)
    ap.add_argument('--jobs', type=int, default=0)
    ap.add_argument('--plan')
    ap.add_argument('--priority', default=os.environ.get('SAN14_CHECK_PRIORITY', 'belownormal'), choices=sorted(PRIORITY))
    ap.add_argument('--keep-going', action='store_true')
    ap.add_argument('--verbose', action='store_true')
    ap.add_argument('--list', action='store_true')
    a = ap.parse_args(argv)
    root = os.path.abspath(a.root)
    jobs = a.jobs if a.jobs > 0 else min(4, os.cpu_count() or 2)
    jobs = max(1, min(jobs, 4 if not os.environ.get('SAN14_CHECK_JOBS_MAX') else int(os.environ['SAN14_CHECK_JOBS_MAX'])))
    data = load_routes(root)
    names = []
    for n in a.routes.split(','):
        n = n.strip()
        if n and n not in names:
            names.append(n)
    tasks = make_tasks(data, names)
    if a.list:
        for t in tasks:
            print('%s/%s needs=%s lock=%s' % (t.route, t.step['id'], ','.join(t.step['needs']) or '-', t.lock or '-'))
        return 0
    keys = {}
    if a.plan:
        with open(a.plan, encoding='utf-8-sig') as f:
            keys = {r['name']: r.get('key') or '' for r in json.load(f).get('routes', [])}
    record = a.plan is not None
    sel = os.path.join(HERE, 'select_checks.py')

    def on_done(route, secs):
        if record:
            p = subprocess.run([sys.executable, '-B', '-X', 'utf8', sel, '--root', root, 'record', route, '--key', keys.get(route, ''), '--seconds', str(secs)],
                               capture_output=True, text=True, encoding='utf-8', errors='replace')
            print((p.stdout or p.stderr).strip())

    t0 = time.time()
    print('run_checks: %d routes, %d steps, jobs=%d priority=%s' % (len(names), len(tasks), jobs, a.priority))
    failed, secs = schedule(root, tasks, jobs, a.priority, a.keep_going, a.verbose, on_done)
    print('run_checks: wall %.1fs; routes: %s' % (time.time() - t0, ', '.join('%s=%.0fs' % (r, s) for r, s in sorted(secs.items(), key=lambda kv: -kv[1]))))
    skipped = [t for t in tasks if t.state == 'skip']
    if failed:
        print('run_checks FAILED: %s (%d steps skipped)' % (', '.join(failed), len(skipped)))
        return 1
    print('run_checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
