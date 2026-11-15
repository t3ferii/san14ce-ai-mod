#!/usr/bin/env python
"""Installer preflight: validate the enabled feature set against the module catalog.

  python -X utf8 tools/installer_preflight.py [--ini AI_TacticsMod/기능설정.ini] [--json]

Reads development/installer_map.json + development/module_catalog.json and the current 기능설정.ini, computes the enabled
modules (the same group/parent rules the installer uses), and reports missing module dependencies, declared conflicts and
item-level Requires that are off. It changes nothing; install/selection code can call it before applying settings.
"""
import argparse
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAP = os.path.join(ROOT, 'development', 'installer_map.json')
CATALOG = os.path.join(ROOT, 'development', 'module_catalog.json')
INI = os.path.join(ROOT, '기능설정.ini')


def read_json(path):
    with open(path, encoding='utf-8-sig') as f:
        return json.load(f)


def read_ini(path):
    raw = open(path, 'rb').read()
    for enc in ('utf-16', 'utf-8-sig', 'cp949'):
        try:
            text = raw.decode(enc)
            break
        except UnicodeError:
            continue
    else:
        raise SystemExit('cannot decode %s' % path)
    out = {}
    section = ''
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith(';') or line.startswith('#'):
            continue
        if line.startswith('[') and line.endswith(']'):
            section = line[1:-1].strip().lower()
            continue
        if '=' in line:
            key, value = line.split('=', 1)
            out[section + '.' + key.strip()] = value.strip()
    return out


def intval(values, key, default=0):
    try:
        return int(values.get(key, default) or default)
    except ValueError:
        return default


def analyse(values, items, modules):
    errors, warnings = [], []
    switchable = {mid for mid, m in modules.items() if m.get('installer', {}).get('items')}
    enabled_items = []
    for item in items:
        if item.get('retired'):
            continue
        ident = item['section'] + '.' + item['key']
        group = item.get('group', '')
        if not group:
            continue
        if intval(values, ident) and intval(values, 'groups.' + group, 1):
            enabled_items.append(item)
    byid = {(i['section'], i['key']): i for i in items}
    for item in enabled_items:
        req = item.get('requires', '')
        if not req:
            continue
        parts = req.split('.', 1)
        if len(parts) == 2:
            parent = byid.get(tuple(parts))
            if parent and intval(values, req) == 0:
                errors.append('%s.%s requires %s, which is off' % (item['section'], item['key'], req))
    mods = []
    for item in enabled_items:
        mid = item.get('module', '')
        if mid and mid not in mods:
            mods.append(mid)
    for mid in mods:
        m = modules.get(mid)
        if not m:
            errors.append('enabled item maps to unknown module %s' % mid)
            continue
        for dep in m.get('depends', []):
            if dep and dep not in mods and dep in switchable:
                errors.append('module %s is enabled but its dependency %s is not' % (mid, dep))
        for other in m.get('conflicts', []):
            if other in mods:
                errors.append('modules %s and %s are declared conflicts' % (mid, other))
    return {'enabled_modules': mods, 'errors': errors, 'warnings': warnings,
            'enabled_items': [i['section'] + '.' + i['key'] for i in enabled_items]}


def main(argv=None):
    ap = argparse.ArgumentParser(description='feature set <-> module catalog preflight')
    ap.add_argument('--ini', default=INI)
    ap.add_argument('--json', action='store_true')
    a = ap.parse_args(argv)
    try:
        values = read_ini(a.ini)
    except OSError as e:
        print('preflight: cannot read %s: %s' % (a.ini, e))
        return 2
    imap = read_json(MAP)
    catalog = read_json(CATALOG)
    res = analyse(values, imap['items'], catalog['modules'])
    if a.json:
        print(json.dumps(res, ensure_ascii=False, indent=1))
    else:
        print('enabled modules:', ', '.join(res['enabled_modules']) or '-')
        for e in res['errors']:
            print('ERROR', e)
        print('installer preflight: %d module(s), %d problem(s)' % (len(res['enabled_modules']), len(res['errors'])))
    return 1 if res['errors'] else 0


if __name__ == '__main__':
    sys.exit(main())
