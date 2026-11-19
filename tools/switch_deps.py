#!/usr/bin/env python
"""Hidden switch dependencies (interop S4, docs/MOD_INTEROP_SPEC.md section 3).

A feature flag F (mod_features.F, set from one installer key in load_mod_features: the hub, src/native_core.inl +
src/native_patch_core.inl + src/native_adapter.cpp since the 2026-11-16 multi-unit refactor) may be tested together with a
one-line predicate function P (static bool P(){return ...mod_features.G...;}, or inline bool P() in the shared core) whose
body reads another
flag G. Then F silently needs G. The installer must say so: G's key has to be in the requires closure of F's key
(development/installer_map.json), or G must be a code-level parent of F (f.F = f.G && ...), or the pair is accepted in
development/switch_deps_baseline.json (ratchet: new pairs fail, stale baseline pairs warn).

  python -X utf8 tools/switch_deps.py --check
  python -X utf8 tools/switch_deps.py --selftest
Standard library only.
"""
import argparse
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASELINE = os.path.join('development', 'switch_deps_baseline.json')

ASSIGN = re.compile(r'^\s*f\.(\w+)\s*=\s*(.*?);', re.M)
KEY = re.compile(r'feature_key\s*\(\s*config\s*,\s*L"([^"]+)"\s*,\s*L"([^"]+)"')
PARENT = re.compile(r'\bf\.(\w+)\b')
NS = re.compile(r'\bnamespace\s+(\w+)\s*\{')
# static/inline one-liners, and since the 2026-11-16 multi-unit refactor also a unit's external ones at column 0
# (terrain_render::active, terrain_foam::active: declared by tu_terrain_api.h)
PRED = re.compile(r'(?m)(?:static\s+(?:inline\s+)?|inline\s+|^)bool\s+(\w+)\s*\(\s*\)\s*\{\s*return\s+([^;{}]*);\s*\}')
FLAG = re.compile(r'\bmod_features\.(\w+)\b')


def read(root, rel):
    try:
        with open(os.path.join(root, rel), encoding='utf-8-sig', errors='replace') as f:
            return f.read()
    except OSError:
        return ''


def flags(adapter):
    """flag -> {'key': 'section.key' or None, 'parents': {flag}}"""
    out = {}
    for name, expr in ASSIGN.findall(adapter):
        k = KEY.search(expr)
        out[name] = {'key': '%s.%s' % k.groups() if k else None, 'parents': set(p for p in PARENT.findall(expr) if p != name)}
    return out


def predicates(files):
    """'ns::fn' and bare 'fn' (per file) -> set of flags the body reads."""
    preds = {}
    for rel, text in files.items():
        for m in PRED.finditer(text):
            ns = None
            for n in NS.finditer(text, 0, m.start()):
                ns = n.group(1)
            read_flags = set(FLAG.findall(m.group(2)))
            if not read_flags:
                continue
            name = '%s::%s' % (ns, m.group(1)) if ns else m.group(1)
            preds.setdefault(name, set()).update(read_flags)
    return preds


def uses(files, preds, known):
    """[(F, G, pred, file:line)] where a line tests mod_features.F and calls a predicate reading G."""
    out = []
    qual = {p: re.compile(r'(?<![\w:])' + re.escape(p) + r'\s*\(') for p in preds if '::' in p}
    for rel, text in files.items():
        for i, line in enumerate(text.splitlines(), 1):
            fs = set(FLAG.findall(line)) & known
            if not fs:
                continue
            for p, rx in qual.items():
                if rx.search(line):
                    for g in preds[p]:
                        for f in fs:
                            if g != f:
                                out.append((f, g, p, '%s:%d' % (rel, i)))
    return out


def requires_closure(items):
    req = {}
    for it in items:
        k = '%s.%s' % (it['section'], it['key'])
        req[k] = [r.strip() for r in (it.get('requires') or '').replace('+', ',').split(',') if r.strip()]
    memo = {}

    def close(k, seen=()):
        if k in memo:
            return memo[k]
        s = set()
        for r in req.get(k, []):
            if r not in seen:
                s.add(r)
                s |= close(r, seen + (k,))
        memo[k] = s
        return s
    return close


def parent_closure(fl, f, seen=()):
    s = set()
    for p in fl.get(f, {}).get('parents', ()):
        if p not in seen:
            s.add(p)
            s |= parent_closure(fl, p, seen + (f,))
    return s


def analyse(root):
    # hub in include order (2026-11-16: load_mod_features moved to native_core.inl; reading the adapter alone found no flag)
    adapter = '\n'.join(read(root, 'src/' + n) for n in ('native_core.inl', 'native_patch_core.inl', 'native_adapter.cpp'))
    fl = flags(adapter)
    files = {}
    for path in sorted(glob.glob(os.path.join(root, 'src', '*.inl')) + glob.glob(os.path.join(root, 'src', '*.h')) +
                       glob.glob(os.path.join(root, 'src', '*.cpp'))):
        rel = os.path.relpath(path, root).replace('\\', '/')
        files[rel] = read(root, rel)
    preds = predicates(files)
    try:
        items = json.loads(read(root, 'development/installer_map.json') or '{}').get('items', [])
    except ValueError:
        items = []
    close = requires_closure(items)
    found = {}
    for f, g, p, at in uses(files, preds, set(fl)):
        kf, kg = fl[f]['key'], fl.get(g, {}).get('key')
        if not kf or not kg or kf == kg:
            continue                        # group flags and derived flags have no installer key of their own
        if g in parent_closure(fl, f) or kg in close(kf):
            continue
        found.setdefault((kf, kg), {'needs': kg, 'feature': kf, 'via': p, 'at': at})
    return sorted(found.values(), key=lambda x: (x['feature'], x['needs']))


def check(root):
    found = analyse(root)
    try:
        base = json.loads(read(root, BASELINE) or '{}').get('accepted', [])
    except ValueError:
        base = []
    acc = {(b['feature'], b['needs']) for b in base}
    errs = [x for x in found if (x['feature'], x['needs']) not in acc]
    stale = sorted(acc - {(x['feature'], x['needs']) for x in found})
    for x in errs:
        print('ERROR hidden switch dependency: %s needs %s (via %s at %s); add requires=%s in installer_map or accept it in %s'
              % (x['feature'], x['needs'], x['via'], x['at'], x['needs'], BASELINE.replace('\\', '/')))
    for f, n in stale:
        print('WARN  stale baseline entry: %s -> %s' % (f, n))
    print('switch deps: %d hidden, %d accepted, %d new, %d stale' % (len(found), len(acc & {(x['feature'], x['needs']) for x in found}), len(errs), len(stale)))
    return 1 if errs else 0


def selftest():
    import tempfile
    ok = True
    with tempfile.TemporaryDirectory() as t:
        os.makedirs(os.path.join(t, 'src'))
        os.makedirs(os.path.join(t, 'development'))
        w = lambda rel, s: open(os.path.join(t, rel), 'w', encoding='utf-8').write(s)
        w('src/native_adapter.cpp', 'void load(){\n    f.edit = on && feature_key(config, L"real_estate", L"terrain_edit", false);\n'
          '    f.weather = on && feature_key(config, L"season", L"season_weather", false);\n'
          '    f.tiles = f.weather && feature_key(config, L"season", L"season_tiles", true);\n}\n')
        w('src/render.inl', 'namespace terrain_render {\nstatic bool active() {return enabled && mod_features.edit;}\n}\n')
        w('src/bridge.inl', 'static void tiles() {\n    const bool on=mod_features.tiles && terrain_render::active();\n'
          '    const bool w=mod_features.tiles && mod_features.weather;\n}\n')
        items = [{'section': 'real_estate', 'key': 'terrain_edit'}, {'section': 'season', 'key': 'season_weather'},
                 {'section': 'season', 'key': 'season_tiles', 'requires': 'season.season_weather'}]
        w('development/installer_map.json', json.dumps({'items': items}))
        r = analyse(t)
        good = len(r) == 1 and r[0]['feature'] == 'season.season_tiles' and r[0]['needs'] == 'real_estate.terrain_edit'
        print('%s hidden need through a predicate is found (%s)' % ('PASS' if good else 'FAIL', r))
        ok &= good
        items[2]['requires'] = 'season.season_weather, real_estate.terrain_edit'
        w('development/installer_map.json', json.dumps({'items': items}))
        r = analyse(t)
        print('%s declared requires clears it' % ('PASS' if not r else 'FAIL'))
        ok &= not r
        # multi-unit layout (2026-11-16): flags assigned in the shared core, predicate exported by a unit (no static)
        items[2]['requires'] = 'season.season_weather'
        w('development/installer_map.json', json.dumps({'items': items}))
        os.replace(os.path.join(t, 'src/native_adapter.cpp'), os.path.join(t, 'src/native_core.inl'))
        w('src/native_adapter.cpp', '#include "native_core.inl"\n')
        w('src/render.inl', 'namespace terrain_render {\nbool active() {return enabled && mod_features.edit;}\n}\n')
        r = analyse(t)
        good = len(r) == 1 and r[0]['feature'] == 'season.season_tiles' and r[0]['needs'] == 'real_estate.terrain_edit'
        print('%s flags in the shared core and an external unit predicate are found (%s)' % ('PASS' if good else 'FAIL', r))
        ok &= good
    print('switch_deps self test %s' % ('passed' if ok else 'FAILED'))
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--root', default=ROOT)
    a = ap.parse_args(argv)
    if a.selftest:
        return selftest()
    return check(a.root)


if __name__ == '__main__':
    sys.exit(main())
