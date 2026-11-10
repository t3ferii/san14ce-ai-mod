#!/usr/bin/env python3
"""Self test of tools/feature_deps.py on a synthetic tree in build/development/feature_deps_selftest/ (W0-2, 2026-11-07).

Covers: namespace/static resolution, comments and strings ignored, member access ignored, ambiguous references, generic names,
declared vs undeclared edges, hub targets, forward declarations (D4), foreign flag reads (D5), dead includes (D3, nested-#if
variant), SCC (D10), D6 direct patch calls, D7 reporting, D8 counters, tools reachability, ratchet (new edge fails, --tighten
shrinks), impact, explain, compat.
"""
import io
import json
import os
import shutil
import sys
import contextlib
import re
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import feature_deps as F  # noqa: E402

BASE = os.path.join(ROOT, 'build', 'development', 'feature_deps_selftest')

FILES = {
    'src/native_adapter.cpp': '''#include "core.h"
#ifdef SPLIT_BASE
#include "native_a.inl"
#include "native_b.inl"
#endif
#include "native_c.inl"
#ifdef SPLIT_BASE
#include "native_e.inl"
#include "native_f.inl"
#endif
#ifdef SPLIT_COMBAT
#if 0
#endif
#include "native_d.inl"
#endif
''',
    'src/core.h': 'static int core_log(int x) { return x; }\n',
    'src/native_a.inl': '''namespace a {
int ping(int x) { return core_log(x); }
}
static int a_helper(int x) { return x + 1; }
static int dup(int x) { return x; }
static int reset_all() { return dup(1) + reset(); }
static int use_state() { return state(); }
''',
    'src/native_b.inl': '''static int b_work(int x) { return a::ping(x); }
static int dup(int x) { return x + 2; }
static int state() { return 1; }
static int reset() { return 0; }
static int uses_a_var() { int a_helper = 3; return a_helper; }
''',
    'src/native_c.inl': '''static int b_work(int x);
static const char *s = "a_helper(";   // a_helper(1); b_work(2);
static int c_fn() { struct { int a_helper; } o = {1}; return o.a_helper + a::ping(2) + (mod_features.b_flag ? 1 : 0); }
static int reset() { return 1; }
static int c_patch() { return patch_pointer(1, 2, 3); }
''',
    'src/native_d.inl': 'static int d_fn() { return core_log(2); }\n',
    'src/native_e.inl': 'static int e_fn() { return f_fn(); }\n',
    'src/native_f.inl': 'static int f_fn() { return e_fn(); }\n',
    'tools/check_feature.ps1': "switch($r) {\n        'r1' { & python -B -X utf8 (Join-Path $taskProject 'tests\\t1.py') }\n}\n",
    'tools/helper.py': 'X = 1\n',
    'tools/orphan.py': 'Y = 2\n',
    'tests/t1.py': 'import helper\n',
    'docs/NOTES.md': 'old build 1fbe6be7 see docs/MISSING.md and docs/NOTES.md\n',
}
MODULES = {
    'core': {'status': 'active', 'dll': 'convenience', 'hub': True, 'depends': [], 'owns': ['src/core.h', 'src/native_adapter.cpp']},
    'a': {'status': 'active', 'dll': 'convenience', 'depends': [], 'owns': ['src/native_a.inl'], 'tests': ['r1']},
    'b': {'status': 'active', 'dll': 'convenience', 'depends': [], 'owns': ['src/native_b.inl'], 'tests': ['r1']},
    'c': {'status': 'active', 'dll': 'convenience', 'depends': ['a'], 'owns': ['src/native_c.inl'], 'tests': ['r1']},
    'd': {'status': 'active', 'dll': 'combat', 'depends': [], 'owns': ['src/native_d.inl']},
    'e': {'status': 'active', 'dll': 'convenience', 'depends': [], 'owns': ['src/native_e.inl']},
    'f': {'status': 'active', 'dll': 'convenience', 'depends': [], 'owns': ['src/native_f.inl']},
}
FEATURES = {'schema': 2, 'always': [], 'routes': {}, 'modules': MODULES}


def make_tree(extra=None, drop=()):
    if os.path.isdir(BASE):
        shutil.rmtree(BASE)
    files = dict(FILES)
    files.update(extra or {})
    for d in drop:
        files.pop(d, None)
    files['development/features.json'] = json.dumps(FEATURES)
    for path, text in files.items():
        p = os.path.join(BASE, path)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
    return BASE


def run(root, policy=None):
    return F.analyse(root, hooks=False, use_cache=False, policy=policy)


def main():
    ok, total = True, 0

    def check(cond, msg):
        nonlocal ok, total
        total += 1
        print('%s %s' % ('PASS' if cond else 'FAIL', msg))
        if not cond:
            ok = False

    root = make_tree()
    res = run(root)
    E = {(e['from'], e['to']): e for e in res['edges']}
    check(('b', 'a') in E and 'a::ping' in E[('b', 'a')]['symbols'], 'namespace-qualified call b -> a::ping resolved')
    check(E.get(('b', 'a'), {}).get('class') == 'D1', 'undeclared b -> a classified D1')
    check(('c', 'a') in E and E[('c', 'a')]['declared'] is True and E[('c', 'a')]['class'] == 'ok', 'declared c -> a is not a violation')
    check(E.get(('a', 'core'), {}).get('class') == 'service', 'edge to a hub module is allowed (service)')
    check('a_helper' not in (E.get(('b', 'a'), {}).get('symbols') or {}) and 'a_helper' not in (E.get(('c', 'a'), {}).get('symbols') or {}),
          'local variable, struct member, string and comment named like a function are not references')
    check(not any(k.startswith('dup') for e in res['edges'] for k in e['symbols']) and res['ambiguous_refs'] >= 1, 'ambiguous references (dup, 2 modules) add no edge')
    check(('a', 'b') in E and 'state' in E[('a', 'b')]['symbols'] and 'reset' not in E[('a', 'b')]['symbols'],
          'generic name counts only when uniquely defined (state yes, reset no)')
    d4 = [f for f in res['findings'] if f['rule'] == 'D4']
    check(len(d4) == 1 and (d4[0]['from'], d4[0]['to']) == ('c', 'b') and 'b_work' in d4[0]['symbols'], 'D4: forward declaration of another module function (c -> b)')
    d5 = [f for f in res['findings'] if f['rule'] == 'D5']
    check(len(d5) == 1 and (d5[0]['from'], d5[0]['to']) == ('c', 'b'), 'D5: foreign mod_features flag read (c reads b_flag)')
    check([d['file'] for d in res['d3']] == ['native_c.inl'], 'D3: convenience module file compiled into combat (%s)' % [d['file'] for d in res['d3']])
    check(res['variants']['native_d.inl'] == ['combat'], 'nested #if in an inactive group does not leak native_d.inl into convenience (%s)' % res['variants']['native_d.inl'])
    check(res['sccs'] == [['a', 'b'], ['e', 'f']], 'D10: SCCs a <-> b (via state) and e <-> f found (%s)' % res['sccs'])
    check(res['counters']['d6_direct_patch_calls'] == 1 and res['counters'].get('d6:c') == 1, 'D6: direct patch_pointer call counted for module c')
    check(res['counters']['old_build_1fbe6be7_files'] == 1 and res['counters']['missing_doc_refs'] == 1, 'D8: old build mention and missing docs reference counted')
    check('tools/orphan.py' in res['reach']['unreachable'] and 'tools/helper.py' not in res['reach']['unreachable'] and 'tests/t1.py' not in res['reach']['unreachable'],
          'reachability: route -> tests/t1.py -> tools/helper.py reachable, tools/orphan.py not (%s)' % res['reach']['unreachable'])
    # User 2026-11-07 W7: metadata and explanatory comments must not save a retired tool.
    reach_root = make_tree(extra={
        'tests/t1.py': '"""옛 사용법: ghost_docstring.py"""; import helper\n# ghost_comment.py\n',
        'src/core.h': FILES['src/core.h'] + '// ghost_source.py\n',
        'docs/NOTES.md': 'manual example ghost_document.py\n',
        'tools/ghost_docstring.py': '', 'tools/ghost_comment.py': '',
        'tools/ghost_source.py': '', 'tools/ghost_document.py': '',
        'tools/manual_entry.py': 'import manual_helper\n', 'tools/manual_helper.py': '',
        'tools/terrain_render_demo_fixture.cpp': '#include "fixture_support.h"\n// ghost_source.py\n',
        'tests/fixture_support.h': '',
        'tools/template.ps1': '& cl "terrain_render_${name}_fixture.cpp"\n# ghost_comment.py\n',
    })
    reach = run(reach_root, {'entry_tools': {'tools/manual_entry.py': 'manual query', 'tools/template.ps1': 'fixture family'}})['reach']
    check(all('tools/ghost_%s.py' % n in reach['unreachable'] for n in ('docstring', 'comment', 'source', 'document')),
          'W7: script docstrings/comments, C++ comments and documentation are not executable references')
    check(not any(f in reach['unreachable'] for f in ('tools/manual_entry.py', 'tools/manual_helper.py',
              'tools/terrain_render_demo_fixture.cpp', 'tests/fixture_support.h')),
          'W7: explicit manual entry, its import, templated fixture and C++ include stay reachable')
    root = make_tree()
    check('import helper' in F.executable_refs_text('example.py', '"""옛 도구 설명"""; import helper\n'),
          'W7: removing a Korean docstring preserves executable statements on the same line')
    # ratchet
    pol = F.load_policy(root)
    base = F.make_baseline(res, pol)
    errs, _w = F.violations(res, base)
    check(not errs, 'baseline passes its own check (%s)' % errs[:2])
    # W8 strict mode: D1/D10 must fail even when the baseline lists them.
    strict_base = dict(base, strict_rules=['D1', 'D10'])
    strict_errs = F.violations(res, strict_base)[0]
    check(any(e.startswith('strict D1 edge b -> a') for e in strict_errs) and any(e.startswith('strict D10 undeclared SCC') for e in strict_errs),
          'W8 strict mode: a baselined D1 edge and D10 SCC still fail (%s)' % [e[:34] for e in strict_errs][:2])
    fake = dict(res)
    fake['d7'] = [{'site': 'GA_X', 'owner': 'o', 'feature': 'f', 'owner_dll': 'layout', 'registered_in': ['layout']}]
    check(any(e.startswith('D7') for e in F.violations(fake, base)[0]), 'D7 fails immediately regardless of baseline')
    FEATURES['modules']['g'] = {'status': 'active', 'dll': 'convenience', 'depends': [], 'owns': ['src/native_g.inl']}
    root2 = make_tree(extra={'src/native_g.inl': 'static int g_fn() { return a::ping(1); }\n',
                             'src/native_adapter.cpp': FILES['src/native_adapter.cpp'] + '#include "native_g.inl"\n',
                             'tools/new_orphan.py': 'Z = 3\n'})
    res2 = run(root2)
    errs2, _w2 = F.violations(res2, base)
    check(any('D1 new edge g -> a' in e for e in errs2), 'ratchet: a new undeclared edge fails (%s)' % [e[:40] for e in errs2][:3])
    check(any('counter d1_edges increased' in e for e in errs2) and any('new unreachable file tools/new_orphan.py' in e for e in errs2),
          'ratchet: counter increase and a new unreachable file fail')
    del FEATURES['modules']['g']
    root3 = make_tree()
    res3 = run(root3)
    tight = F.make_baseline(res3, F.make_baseline(res2, pol), shrink_only=True)
    check(not [e for e in tight['edges'] if e['from'] == 'g'] and tight['counters']['d1_edges'] == res3['counters']['d1_edges'],
          '--tighten drops vanished edges and lowers counters')
    buf = io.StringIO()
    F.save_policy(root3, base)
    with contextlib.redirect_stdout(buf):
        rc = F.main(['--root', root3, '--check', '--no-hooks', '--no-cache'])
    check(rc == 0 and 'feature_deps PASSED' in buf.getvalue() or print(buf.getvalue()), 'CLI --check passes on the baselined synthetic tree')
    # impact / explain / compat
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        F.cmd_impact(root3, res3, 'src/native_a.inl', False, None)
    imp = json.loads(buf.getvalue())
    check({i['module'] for i in imp['impacted']} == {'b', 'c', 'core'}, 'impact(native_a.inl) = referencing modules b, c and the including hub (%s)' % [i['module'] for i in imp['impacted']])
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        rc = F.cmd_explain(res3, 'b')
    check(rc == 0 and 'a::ping' in buf.getvalue(), 'explain b lists its edges')
    cp = F.compat(root3, 'b', res3)
    check(any(c['other'] == 'a' and c['kind'] == 'symbol' for c in cp['conflicts']) and 'r1' in cp['affected_checks'], 'compat(b): conflicts with a, affected checks include r1')
    check(F.compat(root3, 'zzz', res3).get('ok') is False, 'compat of an unknown module id reports an error')
    check(os.path.isdir(BASE) and BASE.replace('\\', '/').endswith('build/development/feature_deps_selftest'), 'synthetic trees live under build/development/feature_deps_selftest')
    # W7 must not leave a deleted source behind an inactive fixture-only #ifdef.
    missing = []
    for source in (Path(ROOT) / 'src').rglob('*'):
        if source.suffix not in ('.h', '.cpp', '.inl'):
            continue
        for inc in re.findall(r'^\s*#\s*include\s+"([^"]+)"', source.read_text(encoding='utf-8-sig'), re.M):
            if not (source.parent / inc).is_file() and not (Path(ROOT) / 'src' / inc).is_file():
                missing.append('%s -> %s' % (source.name, inc))
    check(not missing, 'W7: all local source includes exist, including inactive fixture branches (%s)' % missing)
    print('%s: %d checks' % ('feature_deps self test passed' if ok else 'feature_deps self test FAILED', total))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
