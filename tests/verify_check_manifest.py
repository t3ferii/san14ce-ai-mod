#!/usr/bin/env python3
"""Self-test for tools/check_manifest.py with small synthetic manifests (no game files, no installs).

Writes only under build/development/manifest_selftest/.
"""
import copy
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import check_manifest as cm  # noqa: E402

WORK = os.path.join(ROOT, 'build', 'development', 'manifest_selftest')
shutil.rmtree(WORK, ignore_errors=True)
os.makedirs(os.path.join(WORK, 'src'))
os.makedirs(os.path.join(WORK, 'tests'))
os.makedirs(os.path.join(WORK, 'tools'))

failures = []


def ok(cond, msg):
    print(('ok   ' if cond else 'FAIL ') + msg)
    if not cond:
        failures.append(msg)


def write(rel, text):
    with open(os.path.join(WORK, rel), 'w', encoding='utf-8') as f:
        f.write(text)


write('src/a.inl', 'static const R rows[]={\n  {"alpha",LR_VT,GA(GA_1),GA(GA_2),LF_C},\n};\n'
      'void f(){layout_client::bytes("beta",1,0,0,1,0);add_hook("gamma_x",1);}\n')
write('src/b.h', 'enum : unsigned char {kPointerShared,kPointerHud};\n'
      'static const PeerRule peer_rules[]={\n    {"delta",1,{R1_MOD(L"x.ini")}},\n};\n'
      'void g(){layout_client::yield_to_peer("alpha",1);}\n')
write('src/old.inl.bak_1', 'add_hook("ignored_bak",1);\n')
write('tests/verify_a.py', '')
write('tests/verify_b.py', '')
write('tools/check_feature.ps1', "switch($r) {\n        'x-route' { 1 }\n        'p','q' { 2 }\n        default {}\n}        'tail' {\n }\n")

FILES = ['src/a.inl', 'src/b.h', 'src/old.inl.bak_1', 'tests/verify_a.py', 'tests/verify_b.py', 'tools/check_feature.ps1', 'README.md']
LABELS = {'x-route', 'p', 'q', 'tail'}

BASE = {
    'schema': 2,
    'ownership_scope': ['src/', 'tests/', 'tools/'],
    'ownership_ignore': ['**/*.bak*'],
    'always': ['x-route'],
    'pending_routes': ['future'],
    'routes': {'x-route': {'est_s': 3, 'cache': True}, 'big': {'est_s': 9, 'subsumes': ['x-route']}, 'p': {'est_s': 1}},
    'unrouted_checks': [{'path': 'tests/verify_b.py', 'status': 'manual', 'reason': 'by hand', 'date': '2026-11-06'}],
    'modules': {
        'm1': {'status': 'active', 'dll': 'convenience', 'build': 'split', 'depends': [], 'hook_owners': ['alpha', 'beta', 'gamma_*'],
               'owns': ['src/a.inl', 'tests/verify_a.py'], 'sources': ['src/a.inl'], 'tests': ['x-route', 'future']},
        'm2': {'status': 'active', 'dll': 'layout', 'build': 'split', 'depends': ['m1'], 'hook_owners': ['delta', 'kPointer*'],
               'owns': ['src/b.h', 'tests/verify_b.py'], 'sources': [], 'tests': ['p']},
        'alias1': {'status': 'alias', 'alias_of': 'm1', 'dll': 'convenience', 'build': 'split', 'depends': [], 'hook_owners': [],
                   'owns': [], 'sources': [], 'tests': []},
        'tooling1': {'status': 'tooling', 'dll': 'tool', 'build': 'none', 'depends': [], 'hook_owners': [],
                     'owns': ['tools/*'], 'sources': [], 'tests': []},
    },
}
REACH = {'tests/verify_a.py'}


def run(manifest, files=FILES, reachable=REACH, note=None):
    rep, tracked, unowned, table, scan = cm.check_all(manifest, WORK, files, LABELS, reachable=reachable, reachable_note=note)
    return rep, unowned, table, scan


def codes(items):
    return sorted({c for c, _ in items})


def mutated(fn):
    m = copy.deepcopy(BASE)
    fn(m)
    return m


# helpers
ok(cm.glob_regex('src/**').match('src/a/b.c') and not cm.glob_regex('src/*').match('src/a/b.c'), 'glob: ** crosses directories, * does not')
ok(cm.glob_regex('**/*.bak*').match('src/x.inl.bak_1'), 'glob: **/ matches nested path')
ok(cm.ps1_labels(open(os.path.join(WORK, 'tools/check_feature.ps1')).read()) == LABELS, 'ps1 labels incl. comma list and glued "} \'tail\' {"')

# baseline
rep, unowned, table, scan = run(BASE)
ok(not rep.errors, 'baseline manifest has no errors: %s' % rep.errors)
ok(sorted(scan) == ['alpha', 'beta', 'delta', 'gamma_x', 'kPointerHud', 'kPointerShared'], 'V4 scan finds registry/bytes/add_hook/peer/enum owners, skips .bak: %s' % sorted(scan))
ok(rep.per_check['V1'].startswith('5 tracked'), 'V1 counts scope files minus ignored: %s' % rep.per_check['V1'])

# V1
rep, unowned, _, _ = run(mutated(lambda m: m['modules']['tooling1'].update(owns=[])))
ok(any(c == 'V1' and 'tools/check_feature.ps1' in s for c, s in rep.errors), 'V1 flags an unowned tracked file')
rep, _, _, _ = run(mutated(lambda m: m['modules']['m2']['owns'].append('src/a.inl')))
ok(any(c == 'V1' and 'multiple owners' in s for c, s in rep.errors), 'V1 flags a file owned twice')
rep, _, _, _ = run(mutated(lambda m: m['modules']['m2']['owns'].append('src/*.zzz')))
ok(not rep.errors and any(c == 'V1' for c, _ in rep.warnings), 'V1 warns for dead owns glob only')
rep, _, _, _ = run(BASE, files=FILES + ['src/new_thing.inl'])
ok(any(c == 'V1' and 'new_thing' in s for c, s in rep.errors), 'V1 flags a newly tracked unowned file')
rep, unowned, table, scan = run(BASE, files=FILES + ['src/new_thing.inl'])
ok(any('src/new_thing.inl' in l for l in cm.suggest(BASE, unowned, table, scan)), '--suggest lists the unowned file')

# V2
rep, _, _, _ = run(mutated(lambda m: m['modules']['m2']['tests'].append('nope')))
ok(any(c == 'V2' and 'nope' in s for c, s in rep.errors), 'V2 flags an unknown test name')
rep, _, _, _ = run(mutated(lambda m: (m['modules']['m2']['tests'].append('viarunner'), m['routes'].update(viarunner={'est_s': 1, 'runner': 'tools/x.py'}))))
ok(not rep.errors, 'V2 accepts routes.<n>.runner')
rep, _, _, _ = run(mutated(lambda m: (m['modules']['m2']['tests'].append('viasteps'), m['routes'].update(viasteps={'est_s': 1, 'parallel_safe': True, 'budget_s': 5, 'steps': [{'id': 'a', 'py': 'tools/x.py'}]}))))
ok(not rep.errors and not any('unknown key' in s for _, s in rep.warnings), 'V2 accepts routes.<n>.steps and the W3 keys steps/parallel_safe/budget_s without an unknown-key warning')
rep, _, _, _ = run(mutated(lambda m: m['routes'].update(typo={'est_s': 1, 'stepz': []})))
ok(any(c == 'V7' and 'stepz' in s for c, s in rep.warnings), 'V7 still warns about an unknown route key')
rep, _, _, _ = run(mutated(lambda m: m['pending_routes'].remove('future')))
ok(any(c == 'V2' and 'future' in s for c, s in rep.errors), 'V2 pending_routes list is what allows a branchless name')

# V3
rep, _, _, _ = run(mutated(lambda m: m['unrouted_checks'].clear()))
ok(any(c == 'V3' and 'verify_b' in s for c, s in rep.errors), 'V3 flags an unreachable check not in unrouted_checks')
rep, _, _, _ = run(BASE, reachable=None, note='no route_inputs')
ok(not rep.errors and any(c == 'V3' and 'skipped' in s for c, s in rep.warnings), 'V3 skips with a warning without route_inputs')
rep, _, _, _ = run(BASE, reachable=REACH | {'tests/verify_b.py'})
ok(any(c == 'V3' and 'now reachable' in s for c, s in rep.warnings), 'V3 warns when an unrouted entry became reachable')

# V4
write('src/c.inl', 'void h(){add_hook("newowner",1);}\n')
rep, _, _, _ = run(BASE, files=FILES + ['src/c.inl'])
ok(any(c == 'V4' and 'newowner' in s for c, s in rep.errors), 'V4 flags an unregistered source owner')
os.remove(os.path.join(WORK, 'src/c.inl'))
rep, _, _, _ = run(mutated(lambda m: m['modules']['m1']['hook_owners'].append('ghost')))
ok(any(c == 'V4' and 'stale' in s and 'ghost' in s for c, s in rep.errors), 'V4 flags a stale hook_owners entry')
rep, _, _, _ = run(mutated(lambda m: m['modules']['m2']['hook_owners'].append('alpha')))
ok(any(c == 'V4' and 'alpha' in s and 'several' in s for c, s in rep.errors), 'V4 flags an owner claimed by two modules')

# V5
rep, _, _, _ = run(mutated(lambda m: m['modules']['m2']['depends'].append('missing')))
ok(any(c == 'V5' and 'missing' in s for c, s in rep.errors), 'V5 flags unknown depends')
rep, _, _, _ = run(mutated(lambda m: m['modules']['m1']['depends'].append('m2')))
ok(not rep.errors and any(c == 'V5' and 'cycle' in s for c, s in rep.warnings), 'V5 warns on a cycle')

# V6
rep, _, _, _ = run(mutated(lambda m: m['modules']['m1']['sources'].append('src/absent.inl')))
ok(not rep.errors and any(c == 'V6' and 'absent' in s for c, s in rep.warnings), 'V6 warns on a missing source')

# V7
rep, _, _, _ = run(mutated(lambda m: m['modules']['m1'].update(status='weird')))
ok(any(c == 'V7' and 'status' in s for c, s in rep.errors), 'V7 flags a bad status')
rep, _, _, _ = run(mutated(lambda m: m['modules']['m1'].update(dll='x')))
ok(any(c == 'V7' and 'dll' in s for c, s in rep.errors), 'V7 flags a bad dll')
rep, _, _, _ = run(mutated(lambda m: m['modules']['m1'].update(build='x')))
ok(any(c == 'V7' and 'build' in s for c, s in rep.errors), 'V7 flags a bad build')
rep, _, _, _ = run(mutated(lambda m: m['unrouted_checks'][0].update(status='bogus')))
ok(any(c == 'V7' and 'unrouted' in s for c, s in rep.errors), 'V7 flags a bad unrouted status')
rep, _, _, _ = run(mutated(lambda m: m['routes']['p'].update(cache='yes')))
ok(any(c == 'V7' and 'cache' in s for c, s in rep.errors), 'V7 flags a non-boolean cache')

# V8
rep, _, _, _ = run(mutated(lambda m: m['modules']['alias1'].pop('alias_of')))
ok(any(c == 'V8' for c, _ in rep.errors), 'V8 flags an alias without alias_of')
rep, _, _, _ = run(mutated(lambda m: m['modules']['alias1'].update(alias_of='nowhere')))
ok(any(c == 'V8' and 'nowhere' in s for c, s in rep.errors), 'V8 flags alias_of to an unknown module')
rep, _, _, _ = run(mutated(lambda m: m['modules']['m2'].update(alias_of='m1')))
ok(any(c == 'V8' and 'non-alias' in s for c, s in rep.errors), 'V8 flags alias_of on a non-alias')

# V9
rep, _, _, _ = run(mutated(lambda m: m['modules']['m2'].update(tests=[])))
ok(any(c == 'V9' and 'm2' in s for c, s in rep.errors), 'V9 flags an active module without tests')
ok(not any(c == 'V9' for c, _ in run(BASE)[0].errors), 'V9 allows empty tests for alias/tooling')

# inventory
rows = cm.inventory(BASE, FILES, run(BASE)[2], run(BASE)[3])
ok(len(rows) == 4 and rows[0][0] == 'm1', 'inventory returns one row per module')

# CLI against a real throwaway git repo
repo = os.path.join(WORK, 'repo')
os.makedirs(os.path.join(repo, 'src'))
os.makedirs(os.path.join(repo, 'tools'))
os.makedirs(os.path.join(repo, 'development'))
open(os.path.join(repo, 'src', 'a.inl'), 'w').write('add_hook("alpha",1);\n')
open(os.path.join(repo, 'tools', 'check_feature.ps1'), 'w').write("switch($r) {\n        'x-route' { 1 }\n}\n")
cli = copy.deepcopy(BASE)
cli['ownership_scope'] = ['src/', 'tools/']
cli['modules'] = {'m1': {'status': 'active', 'dll': 'convenience', 'build': 'split', 'depends': [], 'hook_owners': ['alpha'],
                         'owns': ['src/a.inl', 'tools/check_feature.ps1'], 'sources': [], 'tests': ['x-route']}}
cli['unrouted_checks'] = []
cli['pending_routes'] = []
cli['always'] = ['x-route']
cli['routes'] = {'x-route': {'est_s': 1}}
json.dump(cli, open(os.path.join(repo, 'development', 'features.json'), 'w'))
git = ['git', '-C', repo]
try:
    subprocess.run(git + ['init', '-q'], check=True)
    subprocess.run(git + ['add', '-A'], check=True)
    r = subprocess.run([sys.executable, '-B', os.path.join(ROOT, 'tools', 'check_manifest.py'), '--root', repo, '--inventory'],
                       capture_output=True, text=True, encoding='utf-8')
    ok(r.returncode == 0 and 'inventory' in r.stdout, 'CLI exit 0 on a clean synthetic repo: %s' % r.stdout[-200:])
    open(os.path.join(repo, 'src', 'extra.inl'), 'w').write('x\n')
    subprocess.run(git + ['add', '-A'], check=True)
    r = subprocess.run([sys.executable, '-B', os.path.join(ROOT, 'tools', 'check_manifest.py'), '--root', repo, '--suggest'],
                       capture_output=True, text=True, encoding='utf-8')
    ok(r.returncode == 1 and 'unowned: src/extra.inl' in r.stdout and 'src/extra.inl ->' in r.stdout, 'CLI exit 1 with --suggest on an unowned file')
    # User 2026-11-07 W7: deleting a tracked source before commit must not
    # leave a phantom unowned entry in the current inventory.
    os.remove(os.path.join(repo, 'src', 'extra.inl'))
    r = subprocess.run([sys.executable, '-B', os.path.join(ROOT, 'tools', 'check_manifest.py'), '--root', repo],
                       capture_output=True, text=True, encoding='utf-8')
    ok(r.returncode == 0, 'CLI inventories the working tree after a tracked file is retired')
except (OSError, subprocess.CalledProcessError) as e:
    ok(False, 'CLI test needs git: %s' % e)

shutil.rmtree(WORK, ignore_errors=True)
print('%s: %d failures' % ('FAIL' if failures else 'OK', len(failures)))
sys.exit(1 if failures else 0)
