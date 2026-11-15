#!/usr/bin/env python
"""Installer catalog: one mapping file, generated GUI and PowerShell catalogs (docs/MOD_MANAGER_DESIGN.md W9-2).

development/installer_map.json is the source for the 94 installer items (tab/group/section/key/default/requires/inherit,
label/note, the retired set and the owning module). The C# catalog in installer/InstallerGui.cs and the PowerShell
catalog in distribution/FeatureSelect.ps1 (copied to the game folder) are generated from it, so a new mod adds one item
in one place. Labels/notes may differ between the GUI and the console: the map keeps the GUI text and optional *_ps
overrides.

  python -X utf8 tools/gen_installer_catalog.py --migrate   # build the map from the current catalogs (once)
  python -X utf8 tools/gen_installer_catalog.py --apply     # generate the catalogs from the map
  python -X utf8 tools/gen_installer_catalog.py --check     # catalogs and map agree; every item has a module
  python -X utf8 tools/gen_installer_catalog.py --map       # print the item -> module map

Nothing here is compiled into a DLL. Standard library only.
"""
import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GUI = os.path.join(ROOT, 'installer', 'InstallerGui.cs')
PS = os.path.join(ROOT, 'distribution', 'FeatureSelect.ps1')
PS_COPY = os.path.join(ROOT, 'FeatureSelect.ps1')
FEATURES = os.path.join(ROOT, 'development', 'features.json')
REGISTRY = os.path.join(ROOT, 'development', 'module_registry.json')
MAP = os.path.join(ROOT, 'development', 'installer_map.json')

SECTION_DEFAULT = {
    'real_estate': 'terrain-edit', 'diplomacy': 'diplomacy-intel', 'battle_rules': 'expert-combat',
    'personnel': 'personnel', 'domestic': 'domestic', 'extensions': 'modpack-v16',
    'officer_ui': 'personnel', 'game_flow': 'convenience-misc', 'performance': '',
    'progress_native': 'progress-native', 'stratagem': 'building-destruction',
    'ccz': 'ccz-sprites', 'combat': 'ai', 'groups': '', 'diagnostics': '',
}
OVERRIDE = {
    'personnel.awards': 'awards', 'personnel.awards_ai': 'awards',
    'personnel.recruit': 'recruitment', 'personnel.recruit_ai': 'recruitment',
    'personnel.merit': 'merit', 'personnel.merit_troops_level': 'merit', 'personnel.merit_friendship': 'merit',
    'personnel.injury': 'injury', 'personnel.injury_herbal': 'injury',
    'personnel.ai_search2': 'corps-domestic', 'personnel.officer_first': 'corps-domestic',
    'personnel.officer_rotation': 'officer-rotation',
    'domestic.region_officers': 'region-officers', 'domestic.region_officer_traits': 'region-officers',
    'domestic.population': 'domestic', 'domestic.population_rate': 'domestic',
    'extensions.battle_deaths': 'battle-deaths', 'extensions.tooltip_collab': 'trait-hover',
    'extensions.traits9': 'traits9-captive', 'extensions.stats200': 'modpack-v16',
    'extensions.stats200_growth': 'modpack-v16', 'extensions.scenery_skip': 'modpack-v16',
    'officer_ui.loyalty': 'personnel', 'officer_ui.damage_preview': 'damage-preview',
    'officer_ui.stat_exp': 'stat-exp', 'officer_ui.dispatch_days': 'dispatch-days',
    'officer_ui.mediation_plus': 'mediation-plus', 'officer_ui.drag_learn': 'drag_learn',
    'officer_ui.lord_change': 'lord-change', 'officer_ui.tactic_priority': 'tactic-priority',
    'officer_ui.unified': 'personnel',
    'game_flow.speed_buttons': 'speed-buttons', 'game_flow.occupy_fast': 'convenience-misc',
    'game_flow.advisor_briefing': 'advisor-briefing', 'game_flow.caravan_stay': 'convenience-misc',
    'game_flow.fast_save': 'convenience-misc',
    'performance.name_lookup': 'name-lookup', 'performance.layout_keep': 'layout-wait',
    'performance.fast_menu': 'fast-menu', 'performance.appear_batch': 'appear-batch',
    'performance.area_index': 'area-index', 'performance.path_fast': 'path-fast',
    'performance.face_keep': 'face-keep', 'performance.face_prefetch': 'face-keep',
    'performance.timer_1ms': 'convenience-misc',
    'combat.sortie_formation': 'sortie', 'combat.district_intercept_block': 'district-intercept',
    'ccz.ccz_hide_model': 'ccz-sprites', 'ccz.ccz_depth': 'ccz-sprites', 'ccz.ccz_far_keep': 'ccz-sprites',
}
GLOBAL_SECTIONS = {'groups', 'diagnostics'}


def read(path):
    with open(path, encoding='utf-8-sig') as f:
        return f.read()


def write(path, text):
    raw = open(path, 'rb').read()
    bom = raw.startswith(b'\xef\xbb\xbf')
    crlf = raw.count(b'\r\n') > 0 and raw.count(b'\r\n') >= raw.count(b'\n') // 2
    with open(path, 'w', encoding='utf-8-sig' if bom else 'utf-8', newline=('\r\n' if crlf else '\n')) as f:
        f.write(text)


def parse_args(s):
    out, cur, quote, esc = [], '', False, False
    for ch in s:
        if quote:
            if esc:
                cur += ch; esc = False
            elif ch == '\\':
                cur += '\\'; esc = True
            elif ch == '"':
                quote = False
            else:
                cur += ch
        else:
            if ch == '"':
                quote = True
            elif ch == ',':
                out.append(cur.strip()); cur = ''
            elif ch == ')':
                out.append(cur.strip()); break
            else:
                cur += ch
    return out


def parse_cs():
    text = read(GUI)
    body = text[text.index('public static readonly Feature[] Items = {'):]
    body = body[:body.index('};')]
    items = []
    for line in body.splitlines():
        if 'new Feature(' not in line:
            continue
        args = parse_args(line[line.index('new Feature(') + len('new Feature('):])
        if len(args) != 9:
            raise SystemExit('bad Feature row: %r' % line)
        items.append({'tab': args[0], 'group': args[1], 'section': args[2], 'key': args[3],
                      'default': int(args[4]), 'requires': args[5], 'inherit': args[6],
                      'label': args[7], 'note': args[8]})
    retired = set(re.findall(r'"([\w.]+)"', re.search(r'HashSet<string> Retired = new HashSet<string> \{(.*?)\};', text, re.S).group(1)))
    return items, retired


def parse_ps():
    text = read(PS)
    body = text[text.index('$taskFeatureCatalog = @('):]
    body = body[:body.index('\n)')]
    items = []
    for line in body.splitlines():
        if not line.strip().startswith('@{'):
            continue

        def field(name):
            m = re.search(name + r"=('(?:[^']|'')*'|\$true|-?\d+)", line)
            if not m:
                return None
            v = m.group(1)
            if v.startswith("'"):
                return v[1:-1].replace("''", "'")
            if v == '$true':
                return True
            return int(v)
        items.append({'tab': field('Tab'), 'group': field('Group'), 'section': field('Section'),
                      'key': field('Key'), 'default': field('Default'), 'requires': field('Requires'),
                      'inherit': field('Inherit'), 'label': field('Label'), 'note': field('Note'),
                      'retired': field('Retired') is True})
    return items


def flag_owners():
    """mod_features.<flag> -> module from the generated activation rows."""
    reg = json.load(open(REGISTRY, encoding='utf-8-sig'))
    out = {}
    for row in reg.get('rows', []):
        for flag in re.findall(r'mod_features\.(\w+)', row.get('wanted', '')):
            out.setdefault(flag, row.get('module'))
    return out


def module_for(item, modules, flags):
    ident = item['section'] + '.' + item['key']
    if ident in OVERRIDE:
        return OVERRIDE[ident]
    if item['section'] in GLOBAL_SECTIONS:
        return ''
    if item['key'] in flags:
        return flags[item['key']] or ''
    norm = item['key'].replace('_', '-')
    if norm in modules:
        return norm
    return SECTION_DEFAULT.get(item['section'], '')


def build_map():
    cs, retired = parse_cs()
    ps = parse_ps()
    features = json.load(open(FEATURES, encoding='utf-8-sig'))
    modules = set(features['modules'])
    flags = flag_owners()
    psmap = {(x['section'], x['key']): x for x in ps}
    items = []
    for item in cs:
        ident = (item['section'], item['key'])
        p = psmap.get(ident, {})
        row = dict(item)
        row['retired'] = ident[0] + '.' + ident[1] in retired
        row['module'] = module_for(item, modules, flags)
        if p.get('label') and p['label'] != row['label']:
            row['label_ps'] = p['label']
        if p.get('note') and p['note'] != row['note'].replace('\n', ' / '):
            row['note_ps'] = p['note']
        items.append(row)
    return {'schema': 1, 'generated_from': 'installer/InstallerGui.cs + distribution/FeatureSelect.ps1',
            'items': items}


def save_map(data):
    with open(MAP, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(data, f, ensure_ascii=False, indent=1)
        f.write('\n')


def load_map():
    return json.load(open(MAP, encoding='utf-8-sig'))


def cs_quote(s):
    return s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')


def ps_quote(s):
    return s.replace("'", "''")


def render_cs(data):
    lines = ['public static readonly Feature[] Items = {']
    for i in data['items']:
        lines.append('        new Feature("%s", "%s", "%s", "%s", %d, "%s", "%s", "%s", "%s"),' % (
            cs_quote(i['tab']), cs_quote(i['group']), cs_quote(i['section']), cs_quote(i['key']), i['default'],
            cs_quote(i['requires']), cs_quote(i['inherit']), cs_quote(i['label']), cs_quote(i['note'])))
    lines.append('    };')
    retired = ['public static readonly HashSet<string> Retired = new HashSet<string> {']
    rows = [s + '.' + i['key'] for i in data['items'] for s in [i['section']] if i.get('retired')]
    for k in range(0, len(rows), 4):
        retired.append('        ' + ', '.join('"%s"' % x for x in rows[k:k + 4]) + (',' if k + 4 < len(rows) else ''))
    retired.append('    };')
    return '\n'.join(lines), '\n'.join(retired)


def render_ps(data):
    lines = ['$taskFeatureCatalog = @(']
    for i in data['items']:
        parts = ["Group='%s'" % ps_quote(i['group']), "Section='%s'" % ps_quote(i['section']),
                 "Key='%s'" % ps_quote(i['key']), 'Default=%d' % i['default'],
                 "Requires='%s'" % ps_quote(i['requires']), "Inherit='%s'" % ps_quote(i['inherit']),
                 "Tab='%s'" % ps_quote(i['tab'])]
        if i.get('retired'):
            parts.append('Retired=$true')
        parts += ["Label='%s'" % ps_quote(i.get('label_ps', i['label'])),
                  # 2026-11-12: GUI notes are several lines (원작 / 변경 / 효과 / 필요); the console menu keeps one row per item.
                  "Note='%s'" % ps_quote(i.get('note_ps', i['note']).replace('\n', ' / '))]
        lines.append('    @{' + '; '.join(parts) + '}')
    lines.append(')')
    return '\n'.join(lines)


def replace_block(text, start_marker, end_marker, replacement, include_end=False):
    start = text.index(start_marker)
    end = text.index(end_marker, start + len(start_marker))
    if include_end:
        end += len(end_marker)
    return text[:start] + replacement + text[end:]


def apply(data):
    cs_items, cs_retired = render_cs(data)
    text = read(GUI)
    original = text
    text = replace_block(text, 'public static readonly Feature[] Items = {', '\n    };', cs_items, include_end=True)
    text = replace_block(text, 'public static readonly HashSet<string> Retired = new HashSet<string> {', '\n    };', cs_retired, include_end=True)
    if text != original:
        write(GUI, text)
    ps = render_ps(data)
    text = read(PS)
    text = replace_block(text, '$taskFeatureCatalog = @(', '\n)', ps, include_end=True)
    write(PS, text)
    write(PS_COPY, text)
    print('wrote installer/InstallerGui.cs, distribution/FeatureSelect.ps1 and FeatureSelect.ps1')


def check(data):
    errs = []
    features = json.load(open(FEATURES, encoding='utf-8-sig'))
    modules = set(features['modules'])
    for i in data['items']:
        ident = i['section'] + '.' + i['key']
        if i.get('module') and i['module'] not in modules:
            errs.append('unknown module for %s: %s' % (ident, i['module']))
        if not i.get('module') and i['section'] not in GLOBAL_SECTIONS:
            errs.append('no module mapping for %s' % ident)
        if i.get('retired') and i['default'] != 0 and i['section'] not in GLOBAL_SECTIONS:
            pass
    # catalogs must carry the same structural fields
    try:
        cs, retired = parse_cs()
    except Exception as e:  # noqa: BLE001
        return ['cannot parse installer/InstallerGui.cs: %s' % e]
    try:
        ps = parse_ps()
    except Exception as e:  # noqa: BLE001
        return ['cannot parse distribution/FeatureSelect.ps1: %s' % e]
    csmap = {(x['section'], x['key']): x for x in cs}
    psmap = {(x['section'], x['key']): x for x in ps}
    if set(csmap) != set(psmap):
        errs.append('GUI/PS key sets differ: %s' % sorted(set(csmap) ^ set(psmap))[:10])
    for i in data['items']:
        k = (i['section'], i['key'])
        for label, m in (('GUI', csmap), ('PS', psmap)):
            x = m.get(k)
            if not x:
                errs.append('%s is missing %s.%s' % (label, i['section'], i['key']))
                continue
            for f in ('tab', 'group', 'default', 'requires', 'inherit'):
                if x[f] != i[f]:
                    errs.append('%s %s.%s %s differs' % (label, i['section'], i['key'], f))
        if ((k[0] + '.' + k[1]) in retired) != bool(i.get('retired')):
            errs.append('retired flag differs for %s.%s' % k)
    # D9: installer parent feature -> module dependency (direct depends is enough for a warning)
    byid = {(i['section'], i['key']): i for i in data['items']}
    for i in data['items']:
        if not i.get('requires'):
            continue
        parts = i['requires'].split('.', 1)
        if len(parts) != 2:
            continue
        parent = byid.get(tuple(parts))
        if parent and parent.get('module') and i.get('module') and parent['module'] != i['module']:
            deps = features['modules'].get(i['module'], {}).get('depends', [])
            if parent['module'] not in deps:
                errs.append('D9: %s.%s requires %s but module %s does not depend on %s' % (
                    i['section'], i['key'], i['requires'], i['module'], parent['module']))
    return errs


def main(argv=None):
    ap = argparse.ArgumentParser(description='installer catalog from development/installer_map.json')
    ap.add_argument('--migrate', action='store_true', help='build installer_map.json from the current catalogs')
    ap.add_argument('--apply', action='store_true', help='regenerate both installer catalogs from the map')
    ap.add_argument('--check', action='store_true', help='catalogs and map agree, every item has a module')
    ap.add_argument('--map', action='store_true', help='print item -> module')
    a = ap.parse_args(argv)
    if a.migrate:
        data = build_map()
        save_map(data)
        print('wrote development/installer_map.json (%d items)' % len(data['items']))
        return 0
    data = load_map()
    if a.apply:
        apply(data)
    if a.map:
        for i in data['items']:
            print('%s.%s -> %s%s' % (i['section'], i['key'], i['module'] or '(global)', ' [retired]' if i.get('retired') else ''))
    errs = check(data)
    for e in errs[:40]:
        print('ERROR', e)
    print('installer catalog: %d items, %d problem(s)' % (len(data['items']), len(errs)))
    return 0 if not errs else 1


if __name__ == '__main__':
    sys.exit(main())
