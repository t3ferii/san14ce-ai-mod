#!/usr/bin/env python
"""Compare one module manifest with every other module (docs/MOD_MANAGER_DESIGN.md, W9-1).

  python -X utf8 tools/mod_compat.py <module id>            # manifest in development/modules/<id>/module.json
  python -X utf8 tools/mod_compat.py --manifest <file.json> # an external modder's manifest
  python -X utf8 tools/mod_compat.py --selftest
  python -X utf8 tools/mod_compat.py <id> --json

Compares declared dependencies/conflicts, hook sites (same original address, vtable slot, 16-byte block), ini keys, UI owners
and provided API names. It prints the affected check routes (the target's checks plus every module that depends on it).
Only build/install-time data is read; nothing here runs in the game. Standard library only.
"""
import argparse
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MOD_DIR = os.path.join(ROOT, 'development', 'modules')
HOOK_MAP = os.path.join(ROOT, 'build', 'development', 'hook_map.json')


def read(path):
    with open(path, encoding='utf-8-sig') as f:
        return json.load(f)


def load_manifests():
    out = {}
    for path in sorted(glob.glob(os.path.join(MOD_DIR, '*', 'module.json'))):
        m = read(path)
        out[m['id']] = m
    return out


def load_sites(path=HOOK_MAP):
    if not os.path.isfile(path):
        return []
    return read(path).get('sites', [])


def module_of_owner(manifests):
    out = {}
    for mid, m in manifests.items():
        for owner in m.get('hook_owners', []):
            out.setdefault(owner, mid)
    return out


def target_sites(target, manifests, sites, owner_map, rva_of):
    """Sites of an existing module (by hook owner) or of an explicit manifest `hooks` list."""
    if target.get('hooks'):
        out = []
        for h in target['hooks']:
            key = h.get('key', '')
            rva = h.get('rva') or rva_of.get(key, 0)
            out.append({'key': key, 'add': h.get('add', 0), 'rva': rva, 'len': h.get('len', 5),
                        'owner': h.get('owner', target['id']), 'module': target['id'],
                        'original': h.get('original', {})})
        return out
    owners = set(target.get('hook_owners', []))
    out = []
    for s in sites:
        if s.get('owner') in owners:
            out.append({'key': s.get('key', ''), 'add': s.get('add', 0),
                        'rva': (s.get('rva') or {}).get('1a4a447d', 0), 'len': s.get('len', 5),
                        'owner': s.get('owner'), 'module': owner_map.get(s.get('owner'), target['id']),
                        'original': s.get('original', {})})
    return out


def blocks(rva, length):
    if not rva:
        return set()
    return set(range(rva // 16, (rva + max(1, length) - 1) // 16 + 1))


def cycle_nodes(dep_of, start):
    """nodes involved in a cycle reachable from start (empty when acyclic)."""
    seen, stack, out = set(), [], set()

    def walk(n):
        if n in stack:
            out.update(stack[stack.index(n):])
            return
        if n in seen:
            return
        seen.add(n)
        stack.append(n)
        for d in dep_of.get(n, []):
            if d in dep_of:
                walk(d)
        stack.pop()

    walk(start)
    return sorted(out)


def analyse(target, manifests, sites, rva_of=None):
    rva_of = rva_of or {}
    errs, warns = [], []
    affected = set(target.get('checks') or [])
    tid = target['id']
    owner_map = module_of_owner(manifests)
    others = {k: v for k, v in manifests.items() if k != tid}

    # dependencies
    for d in target.get('depends', []):
        if d not in manifests:
            errs.append('unknown dependency: %s' % d)
    dep = {k: list(v.get('depends', [])) for k, v in manifests.items()}
    dep[tid] = list(target.get('depends', []))
    cyc = cycle_nodes(dep, tid)
    if cyc:
        errs.append('dependency cycle: %s' % ' -> '.join(cyc + [tid]))
        for k in cyc:
            if k in manifests:
                affected.update(manifests[k].get('checks', []))

    # declared conflicts
    declared = set(target.get('conflicts', []))
    for other, m in others.items():
        if other in declared or tid in set(m.get('conflicts', [])):
            errs.append('declared conflict: %s' % other)
            affected.update(m.get('checks', []))

    # hooks
    tsites = target_sites(target, manifests, sites, owner_map, rva_of)
    osites = []
    for other, m in others.items():
        osites.extend(target_sites(m, manifests, sites, owner_map, rva_of))
    for ts in tsites:
        for os_ in osites:
            if ts['key'] and ts['key'] == os_['key'] and ts['add'] == os_['add']:
                errs.append('same hook slot: %s %+d (%s)' % (ts['key'], ts['add'], os_['owner']))
            trva, orva = ts.get('rva', 0), os_.get('rva', 0)
            if trva and orva and trva == orva:
                errs.append('same original address: %s (0x%X, %s)' % (ts['key'], trva, os_['owner']))
            if trva and orva and blocks(trva, ts['len']) & blocks(orva, os_['len']):
                if ts['key'] != os_['key']:
                    warns.append('16-byte block shared: %s @0x%X vs %s @0x%X (%s)' % (ts['key'], trva, os_['key'], orva, os_['owner']))

    # ini keys
    tini = {(i.get('section'), i.get('key')) for i in target.get('ini', [])}
    for other, m in others.items():
        if m.get('hub'):
            continue
        oini = {(i.get('section'), i.get('key')) for i in m.get('ini', [])}
        for pair in sorted(tini & oini):
            warns.append('ini key read by both: [%s] %s (%s)' % (pair[0], pair[1], other))
            affected.update(m.get('checks', []))

    # UI owners
    tui = {u.get('owner') for u in target.get('ui', []) if u.get('owner')}
    for other, m in others.items():
        oui = {u.get('owner') for u in m.get('ui', []) if u.get('owner')}
        for name in sorted(tui & oui):
            errs.append('same UI owner: %s (%s)' % (name, other))
            affected.update(m.get('checks', []))

    # provided APIs
    tprov = set(target.get('provides', []))
    for other, m in others.items():
        for name in sorted(tprov & set(m.get('provides', []))):
            errs.append('same provided API: %s (%s)' % (name, other))
            affected.update(m.get('checks', []))

    # modules that depend on the target
    for other, m in others.items():
        if tid in m.get('depends', []):
            affected.update(m.get('checks', []))

    return {
        'target': tid,
        'errors': errs,
        'warnings': warns,
        'affected_checks': sorted({c for group in affected for c in (group if isinstance(group, list) else [group]) if c}),        'hook_sites': len(tsites),
    }


def rva_table():
    """GA_<RVA> -> RVA from the committed address table (external manifests may name a current-build id)."""
    path = os.path.join(ROOT, 'src', 'game_addresses_data.inl')
    try:
        text = open(path, encoding='utf-8-sig').read()
    except OSError:
        return {}
    out = {}
    for name, value in re.findall(r'\b(GA_[0-9A-F]+)=(\d+)', text.split('GA_COUNT=')[0]):
        out[name] = 0
    # the value array is the authority; names are the enum order
    m = re.search(r'ga_val_1a4a447d\[GA_COUNT\]=\{(.*?)\};', text, re.S)
    if m:
        vals = [int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]+', m.group(1))]
        names = list(out)
        for i, name in enumerate(names[:len(vals)]):
            out[name] = vals[i]
    return out


def selftest():
    ok, total = True, 0

    def check(cond, msg):
        nonlocal ok, total
        total += 1
        if not cond:
            ok = False
            print('FAIL', msg)
        else:
            print('PASS', msg)

    base = {'schema': 1, 'checks': ['r1'], 'depends': [], 'hook_owners': [], 'owns': [], 'sources': [],
            'conflicts': [], 'ini': [], 'ui': [], 'provides': [], 'uses': [], 'hooks': [], 'registry': {}}
    a = dict(base, id='a', hook_owners=['ha'], ini=[{'section': 's', 'key': 'k'}], ui=[{'owner': 'u'}], provides=['p'])
    b = dict(base, id='b', hook_owners=['hb'], ini=[{'section': 's', 'key': 'k'}], ui=[{'owner': 'u'}], provides=['p'])
    c = dict(base, id='c', depends=['a'])
    d = dict(base, id='d', depends=['e'])
    e = dict(base, id='e', depends=['d'])
    sites = [
        {'owner': 'ha', 'key': 'GA_100', 'add': 0, 'rva': {'1a4a447d': 0x1000}, 'len': 5, 'original': {'ga': 'GA_900'}},
        {'owner': 'hb', 'key': 'GA_100', 'add': 0, 'rva': {'1a4a447d': 0x1000}, 'len': 5, 'original': {'ga': 'GA_900'}},
        {'owner': 'hb', 'key': 'GA_200', 'add': 0, 'rva': {'1a4a447d': 0x1004}, 'len': 5},
    ]
    r = analyse(a, {'a': a, 'b': b, 'c': c}, sites)
    check(any('same hook slot' in x for x in r['errors']), 'same hook slot is an error')
    check(any('same original address' in x for x in r['errors']), 'same address is an error')
    check(any('ini key read by both' in x for x in r['warnings']), 'shared ini key is a warning')
    check(any('same UI owner' in x for x in r['errors']), 'same UI owner is an error')
    check(any('same provided API' in x for x in r['errors']), 'same API name is an error')
    check('r1' in r['affected_checks'], 'target checks are listed')
    r2 = analyse(a, {'a': a, 'b': dict(b, hook_owners=[], ini=[], ui=[], provides=[]), 'c': c}, sites)
    check(not r2['errors'], 'clean pair has no errors')
    r3 = analyse(d, {'d': d, 'e': e}, [])
    check(any('dependency cycle' in x for x in r3['errors']), 'dependency cycle is reported')
    r4 = analyse(dict(base, id='x', hooks=[{'key': 'GA_100', 'len': 5}]), {'a': a}, sites, {'GA_100': 0x1000})
    check(any('same original address' in x for x in r4['errors']), 'external manifest hook is compared by rva')
    print('%s: %d checks' % ('mod_compat self test passed' if ok else 'mod_compat self test FAILED', total))
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description='module compatibility check (docs/MOD_MANAGER_DESIGN.md)')
    ap.add_argument('id', nargs='?', help='module id in development/modules/<id>/module.json')
    ap.add_argument('--manifest', help='external module manifest')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--sites', default=HOOK_MAP)
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest()
    manifests = load_manifests()
    if a.manifest:
        target = read(a.manifest)
    elif a.id:
        target = manifests.get(a.id)
        if not target:
            print('unknown module: %s' % a.id)
            return 2
    else:
        ap.error('give a module id, --manifest or --selftest')
    sites = load_sites(a.sites)
    res = analyse(target, manifests, sites, rva_table())
    if a.json:
        print(json.dumps(res, ensure_ascii=False, indent=1))
    else:
        print('target: %s (%d hook sites)' % (res['target'], res['hook_sites']))
        for e in res['errors']:
            print('ERROR', e)
        for w in res['warnings']:
            print('WARN ', w)
        print('affected checks:', ', '.join(res['affected_checks']) or '-')
    return 1 if res['errors'] else 0


if __name__ == '__main__':
    sys.exit(main())
