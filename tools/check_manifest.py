#!/usr/bin/env python3
"""Validate development/features.json (module manifest, schema 2).

Checks (docs/HISTORY.md section 2):
  V1 every tracked file in ownership_scope is owned (owns globs) by exactly one module
  V2 every module test name is a check_feature.ps1 branch label, a route with a runner, or a pending route
  V3 every check file is reachable from a route or listed in unrouted_checks (needs tools/route_inputs.py)
  V4 every source hook owner is in exactly one module's hook_owners, and no hook_owners entry is stale
  V5 depends exist, no cycles (warning)
  V6 listed files exist (warning)
  V7 enum values and field types
  V8 alias modules carry alias_of
  V9 modules with empty tests are legacy/tooling/alias only

Usage: check_manifest.py [--root DIR] [--manifest FILE] [--strict] [--suggest] [--inventory] [--json]
Exit 0 when there are no errors (warnings do not fail unless --strict).
Standard library only; reads files, writes nothing.
"""
import argparse
import collections
import fnmatch
import json
import os
import re
import subprocess
import sys

STATUS = {'active', 'candidate', 'legacy', 'alias', 'aggregate', 'tooling'}
DLL = {'convenience', 'combat', 'ccz', 'layout', 'loader', 'tool'}
BUILD = {'split', 'none'}   # plan D-1: the integrated ('native') build kind is retired
UNROUTED_STATUS = {'legacy', 'manual', 'orphan-failing', 'candidate'}
EMPTY_TESTS_OK = {'legacy', 'tooling', 'alias'}
MODULE_KEYS = {'status', 'owns', 'hook_owners', 'depends', 'dll', 'build', 'alias_of', 'hub', 'sources', 'tests'}
ROUTE_KEYS = {'est_s', 'cache', 'inputs', 'external', 'subsumes', 'runner', 'args', 'python', 'covered_by', 'steps', 'parallel_safe', 'budget_s'}
CHECK_FILE_GLOBS = ['tests/verify_*', 'tests/*_verify.py', 'tests/*_fixture*', 'tests/test_*.py']


# ---------------------------------------------------------------- helpers
def glob_regex(pattern):
    out, i = '', 0
    while i < len(pattern):
        if pattern.startswith('**/', i):
            out += '(?:.*/)?'
            i += 3
        elif pattern.startswith('**', i):
            out += '.*'
            i += 2
        else:
            c = pattern[i]
            out += '[^/]*' if c == '*' else '[^/]' if c == '?' else re.escape(c)
            i += 1
    return re.compile('^' + out + '$')


def has_glob(p):
    return any(c in p for c in '*?')


def git_tracked(root):
    out = subprocess.check_output(['git', '-c', 'core.quotepath=false', '-C', root, 'ls-files', '-z'], encoding='utf-8')
    # A retirement is checked before it is committed: deleted tracked files are
    # part of the diff, but no longer belong to the current source inventory.
    return [f for f in out.split('\0') if f and os.path.isfile(os.path.join(root, f))]


def in_scope(path, scope):
    return any(path == s or (s.endswith('/') and path.startswith(s)) for s in scope)


def is_ignored(path, ignore_rx):
    return any(rx.match(path) or rx.match(os.path.basename(path)) for rx in ignore_rx)


def ps1_labels(text):
    """Branch labels of the check_feature.ps1 switch: 'name' { ... } and 'a','b' { ... }."""
    labels = set()
    for m in re.finditer(r"(?:^|\})[ \t]*((?:'[\w.-]+'[ \t]*,[ \t]*)*'[\w.-]+')[ \t]*\{", text, re.M):
        labels.update(re.findall(r"'([\w.-]+)'", m.group(1)))
    return labels


def scan_hook_owners(root, files):
    """Owner strings used by source hook registrations. Returns {owner: sorted files}."""
    found = collections.defaultdict(set)
    simple = [
        r'\{\s*"(\w+)"\s*,\s*LR_',                                   # layout_rows
        r'LayoutPatchRow\s+\w+\s*=\s*\{\s*"(\w+)"',                  # layout_mediation_row, layout_battle_deaths_span
        r'layout_client::(?:span|bytes|block)\(\s*"(\w+)"',
        r'\badd_hook\(\s*"(\w+)"',
        r'\bhook_abs_slot\(\s*"(\w+)"',
        r'\byield_to_peer\(\s*"(\w+)"',
    ]
    for rel in files:
        if not rel.startswith('src/') or '.bak' in os.path.basename(rel):
            continue
        if not rel.endswith(('.h', '.inl', '.cpp', '.c')):
            continue
        try:
            text = open(os.path.join(root, rel), encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        for rx in simple:
            for m in re.finditer(rx, text):
                found[m.group(1)].add(rel)
        pm = re.search(r'peer_rules\s*\[\s*\]\s*=\s*\{', text)
        if pm:
            end = text.find('\n};', pm.end())
            block = text[pm.end(): end if end > 0 else len(text)]
            for m in re.finditer(r'^\s*\{\s*"(\w+)"\s*,\s*\d+\s*,', block, re.M):
                found[m.group(1)].add(rel)
        for em in re.finditer(r'enum\b[^{;]*\{([^}]*)\}', text):
            for name in re.findall(r'\b(kPointer\w+)\b', em.group(1)):
                found[name].add(rel)
    return {k: sorted(v) for k, v in found.items()}


def route_inputs_reachable(root, manifest):
    """Optional V3 hook: tools/route_inputs.py::reachable_check_files(root, manifest) -> set of repo paths."""
    path = os.path.join(root, 'tools', 'route_inputs.py')
    if not os.path.isfile(path):
        return None, 'tools/route_inputs.py not present'
    import importlib.util
    spec = importlib.util.spec_from_file_location('route_inputs', path)
    mod = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(mod)
    except Exception as e:  # noqa: BLE001
        return None, 'route_inputs import failed: %s' % e
    fn = getattr(mod, 'reachable_check_files', None)
    if fn is None:
        return None, 'route_inputs.reachable_check_files not provided'
    try:
        return set(fn(root, manifest)), None
    except Exception as e:  # noqa: BLE001
        return None, 'route_inputs.reachable_check_files failed: %s' % e


# ---------------------------------------------------------------- checks
class Report:
    def __init__(self):
        self.errors = []
        self.warnings = []
        self.info = []
        self.per_check = collections.OrderedDict()

    def err(self, check, msg):
        self.errors.append((check, msg))

    def warn(self, check, msg):
        self.warnings.append((check, msg))

    def note(self, check, msg):
        self.info.append((check, msg))


def compile_owns(mods):
    table = []
    for name, m in mods.items():
        for pat in m.get('owns', []) or []:
            table.append((pat, name, glob_regex(pat)))
    return table


def owners_of(path, table):
    return sorted({name for _, name, rx in table if rx.match(path)})


def check_all(manifest, root, files, labels, check_files=None, reachable=None, reachable_note=None, scan=None):
    rep = Report()
    mods = manifest.get('modules', {})
    routes = manifest.get('routes', {}) or {}
    pending = set(manifest.get('pending_routes', []) or [])
    scope = manifest.get('ownership_scope', [])
    ignore_rx = [glob_regex(p) for p in manifest.get('ownership_ignore', [])]
    table = compile_owns(mods)

    # V7 structure / enums
    for key in ('ownership_scope', 'ownership_ignore', 'always', 'pending_routes'):
        v = manifest.get(key, [])
        if not isinstance(v, list) or not all(isinstance(x, str) for x in v):
            rep.err('V7', 'top-level %s must be a list of strings' % key)
    if manifest.get('schema') != 2:
        rep.err('V7', 'schema must be 2 (got %r)' % (manifest.get('schema'),))
    for name, m in mods.items():
        for k in m:
            if k not in MODULE_KEYS:
                rep.warn('V7', '%s: unknown key %s' % (name, k))
        if m.get('status') not in STATUS:
            rep.err('V7', '%s: status %r not in %s' % (name, m.get('status'), sorted(STATUS)))
        if m.get('dll') not in DLL:
            rep.err('V7', '%s: dll %r not in %s' % (name, m.get('dll'), sorted(DLL)))
        if m.get('build') not in BUILD:
            rep.err('V7', '%s: build %r not in %s' % (name, m.get('build'), sorted(BUILD)))
        for k in ('owns', 'hook_owners', 'depends', 'sources', 'tests'):
            v = m.get(k, [])
            if not isinstance(v, list) or not all(isinstance(x, str) for x in v):
                rep.err('V7', '%s: %s must be a list of strings' % (name, k))
        if 'hub' in m and not isinstance(m['hub'], bool):
            rep.err('V7', '%s: hub must be boolean' % name)
    for rname, r in routes.items():
        if not isinstance(r, dict):
            rep.err('V7', 'route %s must be an object' % rname)
            continue
        for k in r:
            if k not in ROUTE_KEYS:
                rep.warn('V7', 'route %s: unknown key %s' % (rname, k))
        if not isinstance(r.get('est_s', 0), (int, float)) or isinstance(r.get('est_s', 0), bool):
            rep.err('V7', 'route %s: est_s must be a number' % rname)
        if 'cache' in r and not isinstance(r['cache'], bool):
            rep.err('V7', 'route %s: cache must be boolean' % rname)
        for s in r.get('subsumes', []) or []:
            if s not in routes and s not in labels:
                rep.err('V7', 'route %s: subsumes unknown route %s' % (rname, s))
    for u in manifest.get('unrouted_checks', []) or []:
        if u.get('status') not in UNROUTED_STATUS:
            rep.err('V7', 'unrouted_checks %s: status %r not in %s' % (u.get('path'), u.get('status'), sorted(UNROUTED_STATUS)))
        for k in ('path', 'reason', 'date'):
            if not u.get(k):
                rep.err('V7', 'unrouted_checks entry missing %s: %r' % (k, u))
    for a in manifest.get('always', []) or []:
        if a not in labels and a not in routes and a not in pending:
            rep.err('V7', 'always route %s is unknown' % a)

    # V1 ownership
    tracked = [f for f in files if in_scope(f, scope) and not is_ignored(f, ignore_rx)]
    unowned, multi = [], []
    for f in tracked:
        o = owners_of(f, table)
        if not o:
            unowned.append(f)
        elif len(o) > 1:
            multi.append((f, o))
    for f in unowned:
        rep.err('V1', 'unowned: %s' % f)
    for f, o in multi:
        rep.err('V1', 'multiple owners: %s -> %s' % (f, ', '.join(o)))
    fileset = set(files)
    for pat, name, rx in table:
        if has_glob(pat):
            if not any(rx.match(f) for f in files):
                rep.warn('V1', '%s: owns glob matches no tracked file: %s' % (name, pat))
        elif pat not in fileset:
            rep.warn('V6', '%s: owns path is not tracked/does not exist: %s' % (name, pat))
    rep.per_check['V1'] = '%d tracked in scope, %d unowned, %d multiple' % (len(tracked), len(unowned), len(multi))

    # V2 test names
    ok_names = set(labels) | {n for n, r in routes.items() if isinstance(r, dict) and (r.get('runner') or r.get('steps'))} | pending
    bad = 0
    for name, m in mods.items():
        for t in m.get('tests', []) or []:
            if t not in ok_names:
                rep.err('V2', '%s: test %s is not a routed runner/steps entry, a check_feature.ps1 branch or a pending route' % (name, t))
                bad += 1
    for rname in routes:
        if rname not in ok_names:
            rep.warn('V2', 'route %s has no branch label or runner' % rname)
    rep.per_check['V2'] = '%d bad test names' % bad

    # V3 reachability
    if check_files is None:
        check_files = sorted(f for f in files if any(glob_regex(g).match(f) for g in CHECK_FILE_GLOBS) and not is_ignored(f, ignore_rx))
    unrouted = {u.get('path') for u in manifest.get('unrouted_checks', []) or []}
    for p in unrouted:
        if p not in fileset:
            rep.warn('V3', 'unrouted_checks path is not tracked: %s' % p)
    if reachable is None:
        rep.warn('V3', 'skipped: %s' % (reachable_note or 'no reachability data'))
        rep.per_check['V3'] = 'skipped'
    else:
        miss = [f for f in check_files if f not in reachable and f not in unrouted]
        for f in miss:
            rep.err('V3', 'check file neither reachable from a route nor in unrouted_checks: %s' % f)
        for p in unrouted:
            if p in reachable:
                rep.warn('V3', 'unrouted_checks entry is now reachable from a route: %s' % p)
        rep.per_check['V3'] = '%d check files, %d unreachable' % (len(check_files), len(miss))

    # V4 hook owners
    if scan is None:
        scan = scan_hook_owners(root, files)
    pat_owner = collections.defaultdict(list)
    for name, m in mods.items():
        for p in m.get('hook_owners', []) or []:
            pat_owner[p].append(name)
    for p, ms in pat_owner.items():
        if len(set(ms)) > 1:
            rep.err('V4', 'hook_owners entry %s listed by several modules: %s' % (p, ', '.join(sorted(set(ms)))))
        if not any(fnmatch.fnmatchcase(o, p) for o in scan):
            rep.err('V4', '%s: stale hook_owners entry %s (no source registers it)' % (', '.join(ms), p))
    for o in sorted(scan):
        claimed = sorted({n for p, ms in pat_owner.items() if fnmatch.fnmatchcase(o, p) for n in ms})
        if not claimed:
            rep.err('V4', 'hook owner %s (%s) is in no module hook_owners' % (o, ', '.join(scan[o])))
        elif len(claimed) > 1:
            rep.err('V4', 'hook owner %s is in several modules: %s' % (o, ', '.join(claimed)))
    rep.per_check['V4'] = '%d source owners' % len(scan)

    # V5 depends
    for name, m in mods.items():
        for d in m.get('depends', []) or []:
            if d not in mods:
                rep.err('V5', '%s: depends on unknown module %s' % (name, d))
            elif d == name:
                rep.warn('V5', '%s: depends on itself' % name)
    state = {}

    def visit(n, stack):
        if state.get(n) == 2:
            return
        if state.get(n) == 1:
            cyc = stack[stack.index(n):] + [n]
            rep.warn('V5', 'dependency cycle: %s' % ' -> '.join(cyc))
            return
        state[n] = 1
        for d in mods.get(n, {}).get('depends', []) or []:
            if d in mods and d != n:
                visit(d, stack + [n])
        state[n] = 2

    for n in mods:
        visit(n, [])
    rep.per_check['V5'] = 'ok'

    # V6 sources exist
    missing = 0
    for name, m in mods.items():
        for s in m.get('sources', []) or []:
            if not os.path.exists(os.path.join(root, s)):
                rep.warn('V6', '%s: source missing: %s' % (name, s))
                missing += 1
    rep.per_check['V6'] = '%d missing sources' % missing

    # V8 alias
    for name, m in mods.items():
        if m.get('status') == 'alias':
            t = m.get('alias_of')
            if not t:
                rep.err('V8', '%s: alias without alias_of' % name)
            elif t not in mods:
                rep.err('V8', '%s: alias_of unknown module %s' % (name, t))
            elif mods[t].get('status') == 'alias':
                rep.err('V8', '%s: alias_of points to another alias %s' % (name, t))
        elif 'alias_of' in m:
            rep.err('V8', '%s: alias_of on non-alias module (status %s)' % (name, m.get('status')))
    rep.per_check['V8'] = 'ok'

    # V9 empty tests
    for name, m in mods.items():
        if not (m.get('tests') or []) and m.get('status') not in EMPTY_TESTS_OK:
            rep.err('V9', '%s: empty tests but status %s (only legacy/tooling/alias may have none)' % (name, m.get('status')))
    rep.per_check['V9'] = 'ok'
    return rep, tracked, unowned, table, scan


# ---------------------------------------------------------------- suggest / inventory
def suggest(manifest, unowned, table, scan):
    mods = manifest.get('modules', {})
    owned_by_dir = collections.defaultdict(list)
    for pat, name, _ in table:
        if not has_glob(pat):
            owned_by_dir[os.path.dirname(pat)].append((os.path.basename(pat), name))
    lines = []
    for f in unowned:
        d, b = os.path.dirname(f), os.path.basename(f)
        stem = re.sub(r'^(native_|build_|verify_|prepare_|install_)', '', os.path.splitext(b)[0])
        stem = re.sub(r'(_fixture|_verify|_build|_policy|_profile)$', '', stem)
        best, score = None, 0
        for name in mods:
            n = name.replace('-', '_')
            s = len(os.path.commonprefix([stem, n])) if (stem.startswith(n[:4]) or n.startswith(stem[:4])) else 0
            if stem == n:
                s = 100
            if s > score:
                best, score = name, s
        if best is None or score < 4:
            cand = collections.Counter()
            for ob, name in owned_by_dir.get(d, []):
                cand[name] += len(os.path.commonprefix([b, ob]))
            if cand:
                best = cand.most_common(1)[0][0]
                lines.append('%s -> %s (same directory siblings)' % (f, best))
            else:
                lines.append('%s -> ? (no similar file; add an owns entry to a module or a new module)' % f)
        else:
            lines.append('%s -> %s (name match)' % (f, best))
    claimed = set()
    for m in mods.values():
        for p in m.get('hook_owners', []) or []:
            claimed.update(o for o in scan if fnmatch.fnmatchcase(o, p))
    for o in sorted(set(scan) - claimed):
        guess = [n for n in mods if n.replace('-', '_') == o or o.startswith(n.replace('-', '_'))]
        lines.append('hook owner %s (%s) -> %s' % (o, ', '.join(scan[o]), guess[0] if guess else '? (add to a module hook_owners)'))
    return lines


def inventory(manifest, tracked, table, scan):
    mods = manifest.get('modules', {})
    routes = manifest.get('routes', {}) or {}
    count = collections.Counter()
    for f in tracked:
        for o in owners_of(f, table):
            count[o] += 1
    rows = []
    for name, m in mods.items():
        est = sum(routes.get(t, {}).get('est_s', 0) for t in m.get('tests', []) or [])
        rows.append((name, m.get('status'), m.get('dll'), m.get('build'), count.get(name, 0),
                     len(m.get('hook_owners', []) or []), len(m.get('tests', []) or []), est))
    return rows


# ---------------------------------------------------------------- main
def load_manifest(path):
    with open(path, encoding='utf-8-sig') as f:
        return json.load(f)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    here = os.path.dirname(os.path.abspath(__file__))
    ap.add_argument('--root', default=os.path.dirname(here))
    ap.add_argument('--manifest', default=None)
    ap.add_argument('--strict', action='store_true', help='warnings fail too')
    ap.add_argument('--suggest', action='store_true', help='suggest owners for unowned files and hook owners')
    ap.add_argument('--inventory', action='store_true', help='print the module table')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--reachability', action='store_true', help='V3: compute every route input set (slow, minutes)')
    a = ap.parse_args(argv)
    root = os.path.abspath(a.root)
    mpath = a.manifest or os.path.join(root, 'development', 'features.json')
    try:
        manifest = load_manifest(mpath)
    except (OSError, ValueError) as e:
        print('FAIL cannot read manifest %s: %s' % (mpath, e))
        return 1
    files = git_tracked(root)
    ps1 = os.path.join(root, 'tools', 'check_feature.ps1')
    labels = ps1_labels(open(ps1, encoding='utf-8-sig').read()) if os.path.isfile(ps1) else set()
    reachable, note = route_inputs_reachable(root, manifest) if a.reachability else (None, 'V3 runs only with --reachability (slow)')
    rep, tracked, unowned, table, scan = check_all(manifest, root, files, labels, reachable=reachable, reachable_note=note)
    if a.json:
        print(json.dumps({'errors': rep.errors, 'warnings': rep.warnings, 'checks': rep.per_check,
                          'unowned': unowned}, ensure_ascii=False))
    else:
        for c, v in rep.per_check.items():
            print('%s %s' % (c, v))
        for c, m in rep.errors:
            print('ERROR %s %s' % (c, m))
        for c, m in rep.warnings:
            print('WARN  %s %s' % (c, m))
    if a.suggest:
        print('--- suggest')
        for l in suggest(manifest, unowned, table, scan):
            print(l)
    if a.inventory:
        print('--- inventory')
        print('%-22s %-9s %-11s %-6s %5s %5s %5s %6s' % ('module', 'status', 'dll', 'build', 'files', 'hooks', 'tests', 'est_s'))
        rows = inventory(manifest, tracked, table, scan)
        for r in rows:
            print('%-22s %-9s %-11s %-6s %5d %5d %5d %6d' % r)
        by = collections.Counter(r[1] for r in rows)
        print('modules %d (%s); owned files %d/%d; routes %d; unrouted_checks %d; hook owners %d' % (
            len(rows), ', '.join('%s %d' % kv for kv in sorted(by.items())), sum(r[4] for r in rows), len(tracked),
            len(manifest.get('routes', {})), len(manifest.get('unrouted_checks', [])), len(scan)))
    failed = bool(rep.errors) or (a.strict and bool(rep.warnings))
    if not a.json:
        print('%s: %d errors, %d warnings' % ('FAIL' if failed else 'OK', len(rep.errors), len(rep.warnings)))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
