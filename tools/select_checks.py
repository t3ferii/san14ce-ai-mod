#!/usr/bin/env python
"""Change-based check selection and input-hash cache (docs/HISTORY.md 3).

  select_checks.py plan --changed [--base REV] [--paths a,b] [--no-cache] [--json]
  select_checks.py key <route>
  select_checks.py record <route> --key K --seconds S
  select_checks.py explain <file>

Works with the current features.json (no new keys: owns = sources, no cache,
always = []) and with the extended schema (owns, hook_owners, depends, hub,
routes, always, router, ownership_scope/ignore). Standard library only. Reads
git state; writes only build/development/check_cache/<route>.json (record).
"""
import argparse
import datetime
import json
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import route_inputs  # noqa: E402

DEFAULT_SCOPE = ['src/', 'tests/', 'tools/', 'distribution/', 'installer/', 'FeatureSelect.ps1']
DEFAULT_IGNORE = ['**/*.bak*', '**/__pycache__/**']
DEFAULT_ROUTER = {'src/layout_registry.h': 'registry-owner'}
FALLBACK_MODULE = 'dll-split'
CACHE_DIR = os.path.join('build', 'development', 'check_cache')
ROW_RE = re.compile(r'\{\s*"([A-Za-z0-9_]+)"\s*,')


def git(root, *args, check=True):
    p = subprocess.run(['git', '-C', root] + list(args), capture_output=True, text=True, encoding='utf-8', errors='replace')
    if check and p.returncode != 0:
        raise RuntimeError('git %s failed: %s' % (' '.join(args), p.stderr.strip()))
    return p.stdout


def glob_re(pat):
    pat = pat.replace('\\', '/')
    out = ''
    i = 0
    while i < len(pat):
        c = pat[i]
        if pat.startswith('**/', i):
            out += '(?:.*/)?'
            i += 3
            continue
        if pat.startswith('**', i):
            out += '.*'
            i += 2
            continue
        if c == '*':
            out += '[^/]*'
        elif c == '?':
            out += '[^/]'
        else:
            out += re.escape(c)
        i += 1
    return re.compile('^' + out + '$')


def match_any(path, pats):
    for p in pats:
        if p.endswith('/') and path.startswith(p):
            return True
        if glob_re(p).match(path):
            return True
    return False


class Manifest:
    def __init__(self, root):
        self.root = root
        with open(os.path.join(root, 'development', 'features.json'), encoding='utf-8') as f:
            self.data = json.load(f)
        self.modules = self.data.get('modules', {})
        self.routes = self.data.get('routes') or {}
        self.always = list(self.data.get('always') or [])
        self.scope = self.data.get('ownership_scope') or DEFAULT_SCOPE
        self.ignore = self.data.get('ownership_ignore') or DEFAULT_IGNORE
        self.router = self.data.get('router') or DEFAULT_ROUTER
        self.schema = self.data.get('schema', 1)

    def owns(self, name):
        m = self.modules[name]
        return m.get('owns') if 'owns' in m else m.get('sources', [])

    def in_scope(self, path):
        for s in self.scope:
            if path == s or (s.endswith('/') and path.startswith(s)):
                return True
        return False

    def owners_of(self, path):
        return [n for n in self.modules if match_any(path, self.owns(n))]

    def tests_of(self, name):
        t = list(self.modules[name].get('tests') or [])
        if self.schema < 2 and name == 'terrain-edit':
            for extra in ('terrain-foam', 'terrain-diag'):
                if extra not in t:
                    t.append(extra)
        return t


def project_prefix(root):
    return git(root, 'rev-parse', '--show-prefix').strip()


def changed_files(root, base, paths):
    if paths:
        return sorted({route_inputs.norm(p.strip()) for p in paths if p.strip()}), None
    ref = 'HEAD'
    base_note = None
    if base:
        ref = git(root, 'merge-base', 'HEAD', base).strip()
        base_note = ref[:12]
    names = set(git(root, 'diff', '--name-only', '--relative', ref).splitlines())
    names |= set(git(root, 'ls-files', '--others', '--exclude-standard').splitlines())
    return sorted(n for n in names if n), (base_note or 'HEAD'), ref


def registry_owners(root, ref, path):
    """Owners of changed rows in a router file via `git diff -U0`; None = whole file (untracked/non-row change)."""
    tracked = git(root, 'ls-files', '--', path).strip()
    if not tracked:
        return set(), True
    diff = git(root, 'diff', '-U0', '--relative', ref, '--', path)
    owners, other = set(), False
    for line in diff.splitlines():
        if line.startswith(('+++', '---', '@@', 'diff ', 'index ')) or not line[:1] in '+-':
            continue
        body = line[1:].strip()
        if not body or body.startswith('//'):
            continue
        m = ROW_RE.search(body)
        if m:
            owners.add(m.group(1))
        else:
            other = True
    return owners, other


def cache_path(root, route):
    return os.path.join(root, CACHE_DIR, re.sub(r'[^\w.-]', '_', route) + '.json')


def read_cache(root, route):
    try:
        with open(cache_path(root, route), encoding='utf-8') as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def route_key(root, man, route):
    return route_inputs.key_of(root, route_inputs.compute(root, route, man.routes.get(route)))


def default_impact(root, changed, ref, use_hunks):
    """feature_deps impact of the changed src files: {module: reason}. Needs tools/feature_deps.py and src/ in the repo.
    Raises on a scanner failure (the caller turns that into a warning and keeps the owner + depends selection)."""
    srcs = [f for f in changed if f.startswith('src/') and os.path.isfile(os.path.join(root, f))]
    if not srcs or not os.path.isfile(os.path.join(root, 'tools', 'feature_deps.py')):
        return {}
    import feature_deps as FD  # lazy: feature_deps imports this module
    res = FD.analyse(root)
    out = {}
    for f in srcs:
        info = FD.compute_impact(root, res, f, use_hunks and bool(git(root, 'ls-files', '--', f).strip()), ref if use_hunks else None)
        for it in info.get('impacted') or []:
            out.setdefault(it['module'], 'feature_deps: %s uses %s of %s' % (it['module'], ','.join(it['symbols'][:3]) or '/'.join(it['via']), f))
    return out


def plan(root, base=None, paths=None, no_cache=False, impact_fn=default_impact):
    man = Manifest(root)
    warnings = []
    res = changed_files(root, base, paths)
    changed, base_label = res[0], res[1]
    ref = res[2] if len(res) > 2 else 'HEAD'
    mods = {}      # name -> reason

    def add(name, reason):
        if name in man.modules:
            mods.setdefault(name, reason)
            return True
        return False

    unowned, extra_routes = [], {}
    for f in changed:
        if match_any(f, man.ignore):
            continue
        if f == 'development/features.json':
            if os.path.isfile(os.path.join(root, 'tools', 'check_manifest.py')):
                extra_routes['manifest'] = 'features.json changed'
            continue
        if f in man.router and man.router[f] == 'registry-owner' and paths is None:
            owners, whole = registry_owners(root, ref, f)
            hit = False
            for o in sorted(owners):
                targets = [n for n, m in man.modules.items() if o in (m.get('hook_owners') or [])]
                if targets:
                    for t in targets:
                        add(t, '%s row owner %s' % (f, o))
                    hit = True
                else:
                    whole = True
                    warnings.append('registry owner %s has no module hook_owners; routed to layout' % o)
            if whole or not hit:
                if not add('layout', '%s changed' % f):
                    add(FALLBACK_MODULE, '%s changed (no layout module)' % f)
            continue
        owners = man.owners_of(f)
        if owners:
            for o in owners:
                add(o, f)
        elif man.in_scope(f):
            unowned.append(f)
    if unowned:
        warnings.append('unowned files: ' + ', '.join(unowned[:8]) + (' ...' if len(unowned) > 8 else '') + '; added ' + FALLBACK_MODULE)
        add(FALLBACK_MODULE, 'unowned file')

    # reverse dependency closure; hub modules do not propagate
    changed_set = list(mods)
    queue = list(changed_set)
    while queue:
        cur = queue.pop()
        if man.modules.get(cur, {}).get('hub'):
            continue
        for n, m in man.modules.items():
            if cur in (m.get('depends') or []) and n not in mods:
                mods[n] = 'depends on %s' % cur
                queue.append(n)

    # 3-1 impact expansion: modules whose code uses what the changed files define (symbol, flag and hook-site edges of feature_deps)
    try:
        # hub modules (adapter/installer/core services) reference every feature by #include and install call; their routes are the
        # pre-install gate (-Feature dll-split), so a leaf feature change does not select them here (same as the depends closure)
        for name, reason in sorted((impact_fn(root, changed, ref, paths is None) or {}).items()):
            if not man.modules.get(name, {}).get('hub'):
                add(name, reason)
    except Exception as e:  # noqa: BLE001 - a scanner problem must not hide the owner/depends selection
        warnings.append('impact expansion unavailable: %s' % str(e).splitlines()[0][:160] if str(e) else type(e).__name__)

    # routes
    order = []
    why = {}

    def addroute(r, reason):
        if r not in why:
            order.append(r)
            why[r] = reason

    for n in mods:
        for r in man.tests_of(n):
            addroute(r, 'module ' + n)
    for r in man.always:
        addroute(r, 'always')
    for r, reason in extra_routes.items():
        addroute(r, reason)

    subsumed = {}
    for r in order:
        for s in (man.routes.get(r, {}).get('subsumes') or []):
            if s in why and s != r:
                subsumed[s] = r
    entries = []
    for r in order:
        ent = man.routes.get(r, {})
        last = read_cache(root, r)
        item = {'name': r, 'action': 'run', 'key': None, 'est_s': ent.get('est_s'), 'last_s': last.get('last_s'), 'reason': why[r]}
        if r in subsumed:
            item['action'] = 'subsumed'
            item['reason'] = 'subsumed by ' + subsumed[r]
        elif ent.get('cache') and not no_cache:
            k = route_key(root, man, r)
            item['key'] = k
            if last.get('key') == k:
                item['action'] = 'cached'
        elif ent.get('cache'):
            item['key'] = route_key(root, man, r)
        budget = ent.get('budget_s') or ent.get('est_s')
        if budget and last.get('last_s') and last['last_s'] > 1.5 * budget and item['action'] == 'run':
            warnings.append('route %s last run %.0fs exceeds 1.5 x budget %.0fs' % (r, last['last_s'], budget))
        entries.append(item)

    def sort_key(it):
        est = it['est_s'] if it['est_s'] is not None else (it['last_s'] if it['last_s'] is not None else 60)
        return (0 if it['name'] in man.always else 1, est, it['name'])
    entries.sort(key=sort_key)
    return {'base': base_label, 'changed': changed, 'modules': [{'name': n, 'reason': r} for n, r in mods.items()],
            'routes': entries, 'unowned': unowned, 'warnings': warnings}


def print_table(p):
    print('base: %s   changed files: %d' % (p['base'] or 'paths', len(p['changed'])))
    print('modules: ' + (', '.join(m['name'] for m in p['modules']) or '(none)'))
    print('%-22s %-9s %6s %6s  %s' % ('route', 'action', 'est_s', 'last_s', 'reason'))
    for r in p['routes']:
        print('%-22s %-9s %6s %6s  %s' % (r['name'], r['action'], r['est_s'] if r['est_s'] is not None else '-',
                                         ('%.0f' % r['last_s']) if r['last_s'] is not None else '-', r['reason']))
    for w in p['warnings']:
        print('warning: ' + w)


def cmd_record(root, route, key, seconds):
    man = Manifest(root)
    ent = man.routes.get(route, {})
    stored = None
    if ent.get('cache'):
        now = route_key(root, man, route)
        if key and now != key:
            print('inputs changed while %s ran; result not cached' % route)
            return 3
        stored = now
    os.makedirs(os.path.dirname(cache_path(root, route)), exist_ok=True)
    rec = {'route': route, 'key': stored, 'last_s': round(float(seconds), 1),
           'at': datetime.datetime.now().isoformat(timespec='seconds')}
    with open(cache_path(root, route), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(rec, f, indent=1)
        f.write('\n')
    print('recorded %s (%s s)%s' % (route, rec['last_s'], ' with key' if stored else ''))
    return 0


def cmd_explain(root, f):
    man = Manifest(root)
    f = route_inputs.norm(f)
    owners = man.owners_of(f)
    print('file: %s  in_scope: %s' % (f, man.in_scope(f)))
    print('owners: ' + (', '.join(owners) or '(none)'))
    for o in owners:
        m = man.modules[o]
        print('  %s hub=%s depends=%s tests=%s' % (o, bool(m.get('hub')), m.get('depends', []), man.tests_of(o)))
    if f in man.router:
        print('router: %s -> %s' % (f, man.router[f]))
    for r, ent in man.routes.items():
        if ent.get('cache') and f in route_inputs.compute(root, r, ent)['files']:
            print('cached route input of: ' + r)
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--root', default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    sub = ap.add_subparsers(dest='cmd', required=True)
    pl = sub.add_parser('plan')
    pl.add_argument('--changed', action='store_true')
    pl.add_argument('--base')
    pl.add_argument('--paths')
    pl.add_argument('--no-cache', action='store_true')
    pl.add_argument('--json', action='store_true')
    k = sub.add_parser('key')
    k.add_argument('route')
    r = sub.add_parser('record')
    r.add_argument('route')
    r.add_argument('--key', default='')
    r.add_argument('--seconds', type=float, default=0)
    e = sub.add_parser('explain')
    e.add_argument('file')
    a = ap.parse_args(argv)
    root = os.path.abspath(a.root)
    if a.cmd == 'plan':
        paths = a.paths.split(',') if a.paths else None
        p = plan(root, a.base, paths, a.no_cache)
        if not a.json:
            print_table(p)
        print(json.dumps(p, ensure_ascii=True))
        return 0
    if a.cmd == 'key':
        print(route_key(root, Manifest(root), a.route))
        return 0
    if a.cmd == 'record':
        return cmd_record(root, a.route, a.key, a.seconds)
    return cmd_explain(root, a.file)


if __name__ == '__main__':
    sys.exit(main())
