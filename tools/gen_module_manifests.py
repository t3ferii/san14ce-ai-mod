#!/usr/bin/env python
"""Module manifests of the mod manager (docs/MOD_MANAGER_DESIGN.md, W9-1).

Each module gets development/modules/<id>/module.json. In this stage the central registry (development/features.json and
development/module_registry.json) stays the build input; the manifest is the module-level description used by
tools/mod_compat.py and (next stage) the installer catalog. Nothing here is compiled into a DLL or read at game runtime.

  python -X utf8 tools/gen_module_manifests.py --apply    # write/refresh manifests from the central registry
  python -X utf8 tools/gen_module_manifests.py --check    # every module has a matching manifest (always route)

Standard library only.
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FEATURES = os.path.join(ROOT, 'development', 'features.json')
REGISTRY = os.path.join(ROOT, 'development', 'module_registry.json')
INSTALLER = os.path.join(ROOT, 'development', 'installer_map.json')
OUT_DIR = os.path.join(ROOT, 'development', 'modules')
CATALOG = os.path.join(ROOT, 'development', 'module_catalog.json')
SCHEMA = 1


def read(path):
    with open(path, encoding='utf-8-sig') as f:
        return json.load(f)


def source_text(rel):
    try:
        with open(os.path.join(ROOT, rel), encoding='utf-8-sig', errors='replace') as f:
            return f.read()
    except OSError:
        return ''


FEATURE_KEY = re.compile(r'feature_key\s*\(\s*config\s*,\s*L"([^"]+)"\s*,\s*L"([^"]+)"')
MOD_SETTING = re.compile(r'mod_setting\s*\(\s*L"([^"]+)"\s*,\s*L"([^"]+)"')
PROFILE_INT = re.compile(r'GetPrivateProfileIntW\s*\(\s*L"([^"]+)"\s*,\s*L"([^"]+)"')
UI_SCOPE = re.compile(r'UiScope[^;()]*\(\s*"([^"]+)"')


def ini_keys(module):
    """(section,key) pairs read by the files this module owns. Hub modules are skipped: they read every key."""
    if module.get('hub'):
        return []
    pairs = []
    for rel in module.get('owns', []):
        if not rel.endswith(('.inl', '.h', '.cpp', '.c')):
            continue
        text = source_text(rel)
        for rx in (FEATURE_KEY, MOD_SETTING, PROFILE_INT):
            for a, b in rx.findall(text):
                if (a, b) not in pairs:
                    pairs.append((a, b))
    return [{'section': a, 'key': b} for a, b in sorted(pairs)]


def ui_owners(module):
    if module.get('hub'):
        return []
    names = []
    for rel in module.get('owns', []):
        if not rel.endswith(('.inl', '.h', '.cpp', '.c')):
            continue
        for name in UI_SCOPE.findall(source_text(rel)):
            if name not in names:
                names.append(name)
    return [{'owner': name, 'entry': ''} for name in sorted(names)]


def module_manifest(mid, features, registry):
    m = features['modules'][mid]
    owns = list(m.get('owns', []))
    owns_set = set(owns)
    installer = []
    try:
        imap = read(INSTALLER)
    except OSError:
        imap = {'items': []}
    for item in imap.get('items', []):
        if item.get('module') == mid:
            installer.append({'section': item['section'], 'key': item['key'],
                              **({'retired': True} if item.get('retired') else {})})
    ini = ini_keys(m)
    for item in installer:
        pair = {'section': item['section'], 'key': item['key']}
        if pair not in ini:
            ini.append(pair)
    ini.sort(key=lambda x: (x['section'], x['key']))
    return {
        'schema': SCHEMA,
        'id': mid,
        'title': mid,
        'host': m.get('dll', ''),
        'build': m.get('build', 'split'),
        'status': m.get('status', 'active'),
        'hub': bool(m.get('hub', False)),
        'alias_of': m.get('alias_of', ''),
        'depends': list(m.get('depends', [])),
        'conflicts': [],
        'owns': owns,
        'sources': list(m.get('sources', [])),
        'tests': list(m.get('tests', [])),
        'hook_owners': list(m.get('hook_owners', [])),
        'checks': list(m.get('tests', [])),
        'ini': ini,
        'installer': {'items': installer},
        'ui': ui_owners(m),
        'provides': [],
        'uses': [],
        'hooks': [],
        'registry': {
            'rows': [r for r in registry.get('rows', []) if r.get('module') == mid],
            'subscriptions': [s for s in registry.get('subscriptions', []) if s.get('owner') == mid],
            'unity': [u for u in registry.get('unity', {}).get('entries', []) if u.get('file') in owns_set],
            'flags': [],
        },
        'notes': '',
    }


def manifests(features, registry):
    return {mid: module_manifest(mid, features, registry) for mid in sorted(features['modules'])}


def catalog(data):
    """Machine-readable installer/tool view (no source code or registry rows)."""
    modules = {}
    for mid, m in sorted(data.items()):
        modules[mid] = {k: m[k] for k in ('title', 'host', 'status', 'hub', 'depends', 'conflicts',
                                          'checks', 'ini', 'installer', 'ui')}
    return {'schema': 1, 'generated_by': 'tools/gen_module_manifests.py', 'modules': modules}


def save_catalog(data):
    with open(CATALOG, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(catalog(data), f, ensure_ascii=False, indent=1)
        f.write('\n')


def manifest_path(mid):
    return os.path.join(OUT_DIR, mid, 'module.json')


def save(mid, data):
    path = manifest_path(mid)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
        f.write('\n')


def load_existing():
    out = {}
    if not os.path.isdir(OUT_DIR):
        return out
    for mid in sorted(os.listdir(OUT_DIR)):
        path = os.path.join(OUT_DIR, mid, 'module.json')
        if os.path.isfile(path):
            out[mid] = read(path)
    return out


def check(features, registry, existing):
    errs = []
    expected = manifests(features, registry)
    for mid in sorted(expected):
        if mid not in existing:
            errs.append('missing manifest: %s' % mid)
        elif existing[mid] != expected[mid]:
            errs.append('manifest differs from the registry: %s' % mid)
    for mid in sorted(set(existing) - set(expected)):
        errs.append('manifest has no module in features.json: %s' % mid)
    try:
        cur = read(CATALOG)
    except (OSError, ValueError):
        cur = None
    if cur != catalog(expected):
        errs.append('module catalog is stale or missing: development/module_catalog.json')
    return errs


def main(argv=None):
    ap = argparse.ArgumentParser(description='module manifests (docs/MOD_MANAGER_DESIGN.md)')
    ap.add_argument('--apply', action='store_true', help='write/refresh development/modules/<id>/module.json')
    ap.add_argument('--check', action='store_true', help='verify every manifest matches the central registry')
    a = ap.parse_args(argv)
    features = read(FEATURES)
    registry = read(REGISTRY)
    if a.apply:
        data = manifests(features, registry)
        for mid, manifest in data.items():
            save(mid, manifest)
        save_catalog(data)
        print('wrote %d manifests under development/modules/ and development/module_catalog.json' % len(data))
        return 0
    errs = check(features, registry, load_existing())
    for e in errs[:40]:
        print('ERROR', e)
    print('module manifests: %d modules, %d problem(s)' % (len(features['modules']), len(errs)))
    return 0 if not errs else 1


if __name__ == '__main__':
    sys.exit(main())
