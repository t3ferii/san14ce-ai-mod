#!/usr/bin/env python
"""Self-test of tools/select_checks.py and tools/route_inputs.py (docs/HISTORY.md 7 C).

Builds a throwaway git repository in build/development/select_selftest/ and checks:
propagation, hub (no propagation), layout_registry.h hunk routing, docs-only change,
untracked files, unowned files, --paths, cache hit / invalidation / change during a run,
external stamps, subsumes, and git worktree behaviour. Writes only inside that folder.
"""
import json
import os
import shutil
import stat
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(PROJECT, 'tools'))
import select_checks as sc  # noqa: E402

BASE = os.path.join(PROJECT, 'build', 'development', 'select_selftest')
REPO = os.path.join(BASE, 'repo')
WT = os.path.join(BASE, 'wt')
passed = 0
failed = []


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


def write(root, rel, text):
    p = os.path.join(root, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)


def git(root, *args):
    p = subprocess.run(['git', '-C', root, '-c', 'user.name=t', '-c', 'user.email=t@t', '-c', 'core.autocrlf=false',
                        '-c', 'commit.gpgsign=false'] + list(args), capture_output=True, text=True, encoding='utf-8')
    if p.returncode != 0:
        raise RuntimeError(p.stderr)
    return p.stdout


def names(p, key='routes'):
    return [x['name'] for x in p[key]]


def mods(p):
    return sorted(m['name'] for m in p['modules'])


def actions(p):
    return {r['name']: r['action'] for r in p['routes']}


MANIFEST = {
    'schema': 2,
    'ownership_scope': ['src/', 'tests/', 'tools/'],
    'ownership_ignore': ['**/*.bak*'],
    'always': ['st'],
    'routes': {
        'ra': {'cache': True}, 'rb': {'cache': True}, 'st': {'cache': True},
        'rbig': {'subsumes': ['rb']},
    },
    'router': {'src/layout_registry.h': 'registry-owner'},
    'modules': {
        'a': {'owns': ['src/a.cpp', 'src/a.h'], 'hook_owners': ['oa'], 'tests': ['ra']},
        'b': {'depends': ['a'], 'owns': ['src/b.cpp'], 'tests': ['rb']},
        'hubm': {'hub': True, 'owns': ['src/hub.h'], 'tests': ['rh']},
        'c': {'depends': ['hubm'], 'owns': ['src/c.cpp'], 'tests': ['rc']},
        'layout': {'owns': ['src/layout_main.cpp'], 'tests': ['rl']},
        'lreg': {'hub': True, 'owns': ['src/layout_registry.h'], 'tests': ['rr']},
        'x': {'hook_owners': ['speed_x'], 'owns': ['src/x.cpp'], 'tests': ['rx']},
        'dll-split': {'owns': ['src/split.cpp'], 'tests': ['rs']},
        'big': {'owns': ['src/big.cpp'], 'tests': ['rbig', 'rb']},
    },
}

PS1 = r"""switch($taskRoute) {
    'ra' { & (Join-Path $taskProject 'tests\ra_fixture.cpp') }
    'rb' { & (Join-Path $taskProject 'tests\rb_fixture.cpp'); Get-Content (Join-Path $taskProject 'research\ext.txt') }
    'rh' { & (Join-Path $taskProject 'tests\rh.cpp') }
    'rc' { & (Join-Path $taskProject 'tests\rc.cpp') }
    'rl' { & (Join-Path $taskProject 'tests\rl.cpp') }
    'rr' { & (Join-Path $taskProject 'tests\rr.cpp') }
    'rx' { & (Join-Path $taskProject 'tests\rx.cpp') }
    'rs' { & (Join-Path $taskProject 'tests\rs.cpp') }
    'rbig' { & (Join-Path $taskProject 'tests\rbig.cpp') }
    'st' { & python -B (Join-Path $taskProject 'tests\st.py') }
}
"""

REGISTRY = '''#pragma once
int kMax = 3;
static const Row layout_rows[] = {
    {"oa", LR_X, 1},
    {"speed_x", LR_Y, 2},
};
'''


def build_repo():
    rmtree(BASE)
    os.makedirs(REPO)
    git(REPO, 'init', '-q')
    write(REPO, 'development/features.json', json.dumps(MANIFEST, indent=2) + '\n')
    write(REPO, 'tools/check_feature.ps1', PS1)
    write(REPO, 'tests/ra_fixture.cpp', '#include "a.h"\nint main(){}\n')
    write(REPO, 'tests/rb_fixture.cpp', 'int main(){}\n')
    write(REPO, 'tests/st.py', 'print(1)\n')
    for n in ('rh', 'rc', 'rl', 'rr', 'rx', 'rs', 'rbig'):
        write(REPO, 'tests/%s.cpp' % n, 'int main(){}\n')
    write(REPO, 'src/a.h', '#pragma once\n#include "deep.h"\n')
    write(REPO, 'src/deep.h', '// deep\n')
    for n in ('a.cpp', 'b.cpp', 'hub.h', 'layout_main.cpp', 'x.cpp', 'split.cpp', 'big.cpp'):
        write(REPO, 'src/' + n, '// ' + n + '\n')
    write(REPO, 'src/layout_registry.h', REGISTRY)
    write(REPO, 'research/ext.txt', 'external one\n')
    write(REPO, 'docs/readme.md', 'doc\n')
    git(REPO, 'add', '-A')
    git(REPO, 'commit', '-q', '-m', 'init')


def reset(root):
    git(root, 'checkout', '-q', '--', '.')
    git(root, 'clean', '-fdq')


def main():
    build_repo()
    key_st = lambda: sc.route_key(REPO, sc.Manifest(REPO), 'st')  # noqa: E731

    # clean tree: only always
    p = sc.plan(REPO)
    check('clean tree: no modules, always-only', mods(p) == [] and names(p) == ['st'], p)

    # docs only
    write(REPO, 'docs/readme.md', 'doc changed\n')
    p = sc.plan(REPO)
    check('docs-only change selects only always', mods(p) == [] and names(p) == ['st'] and not p['warnings'], p)
    reset(REPO)

    # propagation a -> b
    write(REPO, 'src/a.cpp', '// changed\n')
    p = sc.plan(REPO)
    check('propagation: a changes b too', mods(p) == ['a', 'b'] and set(names(p)) == {'ra', 'rb', 'st'}, p)
    reset(REPO)

    # hub does not propagate
    write(REPO, 'src/hub.h', '// changed\n')
    p = sc.plan(REPO)
    check('hub change does not propagate', mods(p) == ['hubm'], p)
    reset(REPO)

    # hunk routing
    reg = open(os.path.join(REPO, 'src/layout_registry.h'), encoding='utf-8').read()
    write(REPO, 'src/layout_registry.h', reg.replace('{"speed_x", LR_Y, 2}', '{"speed_x", LR_Y, 3}'))
    p = sc.plan(REPO)
    check('hunk routing: speed_x row -> x only', mods(p) == ['x'], p)
    reset(REPO)
    write(REPO, 'src/layout_registry.h', reg.replace('{"oa", LR_X, 1}', '{"oa", LR_X, 9}'))
    p = sc.plan(REPO)
    check('hunk routing: oa row -> a (+b by depends)', mods(p) == ['a', 'b'], p)
    reset(REPO)
    write(REPO, 'src/layout_registry.h', reg.replace('kMax = 3', 'kMax = 4'))
    p = sc.plan(REPO)
    check('hunk routing: non-row line -> layout', mods(p) == ['layout'], p)
    reset(REPO)
    write(REPO, 'src/layout_registry.h', reg.replace('{"oa", LR_X, 1}', '{"oa", LR_X, 9}').replace('kMax = 3', 'kMax = 4'))
    p = sc.plan(REPO)
    check('hunk routing: row + non-row -> a,b,layout', mods(p) == ['a', 'b', 'layout'], p)
    reset(REPO)

    # untracked owned file
    write(REPO, 'src/c.cpp', '// new\n')
    p = sc.plan(REPO)
    check('untracked owned file maps to its module', mods(p) == ['c'], p)
    reset(REPO)

    # unowned file in scope -> fallback + warning; outside scope ignored
    write(REPO, 'src/nobody.cpp', '// new\n')
    p = sc.plan(REPO)
    check('unowned file: dll-split fallback + warning', mods(p) == ['dll-split'] and p['unowned'] == ['src/nobody.cpp'] and p['warnings'], p)
    reset(REPO)
    write(REPO, 'misc/other.txt', 'x\n')
    p = sc.plan(REPO)
    check('file outside scope ignored', mods(p) == [] and not p['unowned'], p)
    reset(REPO)

    # --paths
    p = sc.plan(REPO, paths=['src/x.cpp'])
    check('--paths ignores git state', mods(p) == ['x'] and p['changed'] == ['src/x.cpp'], p)

    # subsumes
    write(REPO, 'src/big.cpp', '// changed\n')
    p = sc.plan(REPO)
    check('subsumes: rb subsumed by rbig', actions(p).get('rb') == 'subsumed' and actions(p).get('rbig') == 'run', p)
    reset(REPO)

    # base: committed change visible against base, not against HEAD
    git(REPO, 'checkout', '-q', '-b', 'feat')
    write(REPO, 'src/x.cpp', '// committed on feat\n')
    git(REPO, 'commit', '-qam', 'feat')
    base = git(REPO, 'rev-parse', 'HEAD~1').strip()
    p0 = sc.plan(REPO)
    p1 = sc.plan(REPO, base=base)
    check('--base sees committed branch changes', mods(p0) == [] and mods(p1) == ['x'], (p0, p1))

    # cache
    p = sc.plan(REPO)
    check('cache: first plan runs st', actions(p).get('st') == 'run' and p['routes'][0]['key'])
    k1 = key_st()
    check('cache: record stores pass', sc.cmd_record(REPO, 'st', k1, 1.5) == 0)
    p = sc.plan(REPO)
    check('cache: same input -> cached', actions(p).get('st') == 'cached', p)
    p = sc.plan(REPO, no_cache=True)
    check('cache: --no-cache forces run', actions(p).get('st') == 'run', p)
    write(REPO, 'tests/st.py', 'print(2)\n')
    p = sc.plan(REPO)
    check('cache: changed input invalidates', actions(p).get('st') == 'run', p)
    rc = sc.cmd_record(REPO, 'st', k1, 1.0)   # key from before the edit: inputs changed during the run
    p = sc.plan(REPO)
    check('cache: change during run is not recorded', rc == 3 and actions(p).get('st') == 'run', (rc, p))
    k2 = key_st()
    sc.cmd_record(REPO, 'st', k2, 1.0)
    check('cache: record after stable run', actions(sc.plan(REPO)).get('st') == 'cached')
    reset(REPO)

    # include closure + external stamp
    write(REPO, 'src/a.cpp', '// touch\n')
    man = sc.Manifest(REPO)
    ka, kb = sc.route_key(REPO, man, 'ra'), sc.route_key(REPO, man, 'rb')
    write(REPO, 'src/deep.h', '// deep changed\n')
    check('include closure: nested header changes ra key',
          sc.route_key(REPO, man, 'ra') != ka and sc.route_key(REPO, man, 'rb') == kb)
    write(REPO, 'research/ext.txt', 'external two, longer\n')
    check('external stamp changes rb key', sc.route_key(REPO, man, 'rb') != kb)
    reset(REPO)

    # 3-1 impact expansion (feature_deps edges): injected impact function; selection = owners + depends closure + impacted modules + always
    reset(REPO)
    imp = lambda root, changed, ref, hunks: {'c': 'feature_deps: c uses X of src/hub.h'}   # noqa: E731
    p = sc.plan(REPO, paths=['src/hub.h'], impact_fn=imp)
    check('impact: hub change reaches the module that uses its symbols (depends closure alone stops at a hub)',
          mods(p) == ['c', 'hubm'] and 'rc' in names(p) and 'rh' in names(p), p['modules'])
    check('impact: reason names feature_deps', any(m['name'] == 'c' and m['reason'].startswith('feature_deps:') for m in p['modules']))
    p0 = sc.plan(REPO, paths=['src/hub.h'], impact_fn=lambda *a: {})
    check('impact: empty impact leaves the old selection (hub does not propagate)', mods(p0) == ['hubm'], p0['modules'])
    p = sc.plan(REPO, paths=['src/a.cpp'], impact_fn=lambda *a: {'x': 'feature_deps: x uses Y of src/a.cpp', 'b': 'feature_deps: dup'})
    check('impact: union with depends closure, no duplicate module', mods(p) == ['a', 'b', 'x'] and [m['name'] for m in p['modules']].count('b') == 1, p['modules'])
    check('impact: owner/depends reason wins over the impact reason', [m['reason'] for m in p['modules'] if m['name'] == 'b'] == ['depends on a'], p['modules'])

    p = sc.plan(REPO, paths=['src/a.cpp'], impact_fn=lambda *a: {'hubm': 'feature_deps: hub includes a.cpp', 'x': 'feature_deps: x'})
    check('impact: hub modules are not selected by impact (they are the pre-install gate)', mods(p) == ['a', 'b', 'x'], p['modules'])

    def boom(*a):
        raise RuntimeError('scanner exploded\nsecond line')
    p = sc.plan(REPO, paths=['src/a.cpp'], impact_fn=boom)
    check('impact: scanner failure becomes a warning and keeps owner + depends selection',
          mods(p) == ['a', 'b'] and any('impact expansion unavailable: scanner exploded' in w for w in p['warnings']), p)
    p = sc.plan(REPO, paths=['src/a.cpp'])
    check('impact: default impact is skipped silently when the repo has no tools/feature_deps.py', mods(p) == ['a', 'b'] and not p['warnings'], p)
    seen = {}
    sc.plan(REPO, paths=['src/a.cpp', 'docs/readme.md'], impact_fn=lambda root, changed, ref, hunks: seen.update(changed=list(changed), hunks=hunks) or {})
    check('impact: function gets the changed list; --paths disables hunk narrowing', seen.get('changed') == ['docs/readme.md', 'src/a.cpp'] and seen.get('hunks') is False, seen)

    # budget warning: last run longer than 1.5 x budget_s (or est_s)
    man_b = json.loads(json.dumps(MANIFEST))
    man_b['routes']['ra']['budget_s'] = 10
    write(REPO, 'development/features.json', json.dumps(man_b, indent=2) + '\n')
    os.makedirs(os.path.join(REPO, sc.CACHE_DIR), exist_ok=True)
    json.dump({'route': 'ra', 'key': None, 'last_s': 16.0}, open(sc.cache_path(REPO, 'ra'), 'w'))
    p = sc.plan(REPO, paths=['src/a.cpp'], no_cache=True, impact_fn=lambda *a: {})
    check('budget: last_s 16 > 1.5 x budget 10 warns', any('route ra last run 16s exceeds 1.5 x budget 10s' in w for w in p['warnings']), p['warnings'])
    json.dump({'route': 'ra', 'key': None, 'last_s': 14.0}, open(sc.cache_path(REPO, 'ra'), 'w'))
    p = sc.plan(REPO, paths=['src/a.cpp'], no_cache=True, impact_fn=lambda *a: {})
    check('budget: last_s 14 within 1.5 x budget stays quiet', not any('budget' in w for w in p['warnings']), p['warnings'])
    reset(REPO)
    shutil.rmtree(os.path.join(REPO, 'build'), ignore_errors=True)

    # worktree
    git(REPO, 'worktree', 'add', '-q', WT, '-b', 'wtbranch')
    write(WT, 'src/x.cpp', '// only in worktree\n')
    pw = sc.plan(WT)
    pr = sc.plan(REPO)
    check('worktree: change seen in worktree only', mods(pw) == ['x'] and mods(pr) == [], (pw, pr))
    kw = sc.route_key(WT, sc.Manifest(WT), 'st')
    sc.cmd_record(WT, 'st', kw, 0.5)
    check('worktree: cache is per working copy',
          actions(sc.plan(WT)).get('st') == 'cached' and os.path.isfile(os.path.join(WT, sc.CACHE_DIR, 'st.json')))

    # CLI: last stdout line is JSON
    out = subprocess.run([sys.executable, '-B', '-X', 'utf8', os.path.join(PROJECT, 'tools', 'select_checks.py'),
                          '--root', WT, 'plan', '--changed'], capture_output=True, text=True, encoding='utf-8').stdout
    try:
        last = json.loads(out.strip().splitlines()[-1])
        check('CLI: table then JSON last line', 'routes' in last and 'modules' in last)
    except ValueError:
        check('CLI: table then JSON last line', False, out)

    rmtree(WT)
    git(REPO, 'worktree', 'prune')
    print('verify_select_checks: %d passed, %d failed' % (passed, len(failed)))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
