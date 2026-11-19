#!/usr/bin/env python
"""Proof that the SAN14_MULTI_TU split compiles every module exactly as the unity build does (2026-11-16 refactor,
docs/REFACTOR_EQUIVALENCE.md section 4).

    python -B -X utf8 tools/unit_equivalence.py                  preprocess + audit of the split fixture objects
    python -B -X utf8 tools/unit_equivalence.py preprocess       unity vs unit text, every build variant
    python -B -X utf8 tools/unit_equivalence.py audit OBJDIR...  dynamic initializers read across objects, split static state

preprocess: for each build variant of tools/build_split.ps1 (release, diag, costs, fixture) and each generated unit source
src/tu_<root>_<unit>.cpp, cl /P runs on the unity root (native_adapter.cpp) and on the unit. Every file whose text first
appears in the unit after its interface header (the unit's modules and the headers only they include) must preprocess to
the same lines in both, so no module tests a macro that an earlier main-side module or native_adapter.cpp defines. Names
made from __COUNTER__ are normalized. The one allowed difference: a line the unity text has and the unit lacks, which the
unit's interface header src/tu_<unit>_api.h defines with the same text up to `static` -> `inline` (a load-time value the
main unit reads; the module keeps it under #ifndef SAN14_MULTI_TU). In the unity text, the #pragma pack/warning stacks must be empty where the first
unit module starts, and no source of ours may use #pragma optimize or a bare #pragma pack(n) before it.

audit: in each object directory (one DLL's objects), no object may refer (UNDEF) to a variable that another object
initializes dynamically (??__E): load-time order across objects is unspecified. A variable both objects define (C++17
inline) is initialized in each object before its later definitions, so it is allowed.
Also, no two objects of one directory may hold the same internal-linkage (Static) symbol in a writable section (.data, .bss,
.tls): a namespace-scope `static` variable, or a function-local static of a `static` function, that a shared header gives every
unit would become one copy per unit, and the units would no longer see each other's writes (e.g. layout_client's handle list).
Stateless `static` helpers (native_patch_core.inl) live in .text and are allowed. The default run first proves on two tiny
objects that the audit catches such a copy (and passes a C++17 inline variable), then audits the split fixture objects.
Standard library + tools/run_checks.py (toolchain of tools/build_split.ps1).
"""
import concurrent.futures, difflib, glob, json, os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import run_checks  # noqa: E402

# tools/build_split.ps1 Get-TaskVariant: output folder and extra defines per variant (checked against the script below)
VARIANTS = {
    'release': ('build/development/loader_split', ['/DNO_RUNTIME_LOG']),
    'diag': ('build/development/loader_split_diagnostic', ['/DRUNTIME_COST_PROFILE']),
    'costs': ('build/development/loader_split_cost_profile', ['/DRUNTIME_COST_PROFILE', '/DNO_RUNTIME_LOG']),
    'fixture': ('build/development/loader_split', ['/DSPLIT_MODULE_FIXTURE']),
}
ROOT_DEFS = {'convenience': ['/DSPLIT_BASE'], 'combat': ['/DSPLIT_COMBAT']}
FIXTURE_OBJS = ['build/development/loader_split/obj/SplitBaseFixture', 'build/development/loader_split/obj/SplitCombatFixture']
LINE = re.compile(r'^#line\s+\d+\s+"(.*)"\s*$')
COUNTER = re.compile(r'\b(cost_key_|cost_scope_|hot_scope_|terrain_scope_)\d+\b')
PRAGMA = re.compile(r'^\s*#\s*pragma\s+(pack|warning|optimize)\s*\((.*)\)')


def units():
    """[(root, unit, path, [member files])] from the generated unit sources."""
    out = []
    for p in sorted(glob.glob(os.path.join(ROOT, 'src', 'tu_*_*.cpp'))):
        m = re.match(r'tu_(convenience|combat)_(\w+)\.cpp$', os.path.basename(p))
        if not m:
            continue
        text = open(p, encoding='utf-8-sig').read()
        api = text.find('_api.h"')
        members = re.findall(r'^\s*#include\s+"([^"]+)"', text[api:], re.M)[1:] if api >= 0 else []
        out.append((m.group(1), m.group(2), p, members))
    return out


def check_variants():
    text = open(os.path.join(ROOT, 'tools', 'build_split.ps1'), encoding='utf-8-sig').read()
    for kind, (folder, defs) in VARIANTS.items():
        if folder.replace('/', '\\') not in text or any("'%s'" % d not in text for d in defs):
            raise SystemExit('unit_equivalence: variant %s (%s %s) no longer matches tools/build_split.ps1' % (kind, folder, defs))


def preprocess(tc, args, src, out):
    env = dict(os.environ)
    env.update(tc['env'])
    p = subprocess.run([tc['cl']] + args + ['/P', '/Fi' + out, src], cwd=os.path.dirname(out), env=env,
                       stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if p.returncode:
        raise SystemExit('unit_equivalence: cl /P failed for %s %s\n%s' % (src, ' '.join(args), p.stdout.decode('utf-8', 'replace')[-3000:]))
    return out


def by_file(path):
    """{normcase file: [normalized lines]} in order, plus the order in which files first appear."""
    files, order, cur = {}, [], None
    with open(path, encoding='utf-8', errors='replace') as f:
        for raw in f:
            m = LINE.match(raw)
            if m:
                cur = os.path.normcase(os.path.normpath(m.group(1).replace('\\\\', '\\')))
                if cur not in files:
                    files[cur] = []
                    order.append(cur)
                continue
            s = raw.strip()
            if s and cur is not None:
                files[cur].append(COUNTER.sub(r'\1#', s))
    return files, order


def pragma_state(path, stop_files):
    """#pragma pack/warning depth and offending pragmas of our sources before the first line of any stop file."""
    pack = warn = 0
    bad, cur = [], None
    src = os.path.normcase(os.path.join(ROOT, 'src'))
    with open(path, encoding='utf-8', errors='replace') as f:
        for raw in f:
            m = LINE.match(raw)
            if m:
                cur = os.path.normcase(os.path.normpath(m.group(1).replace('\\\\', '\\')))
                if cur in stop_files:
                    break
                continue
            m = PRAGMA.match(raw)
            if not m:
                continue
            kind, arg = m.group(1), m.group(2).replace(' ', '')
            ours = cur is not None and cur.startswith(src)
            if kind == 'pack':
                if arg.startswith('push'):
                    pack += 1
                elif arg.startswith('pop'):
                    pack -= 1
                elif ours:
                    bad.append('%s: #pragma pack(%s)' % (cur, arg))
            elif kind == 'warning':
                if arg.startswith('push'):
                    warn += 1
                elif arg.startswith('pop'):
                    warn -= 1
            elif ours:
                bad.append('%s: #pragma optimize(%s)' % (cur, arg))
    return pack, warn, bad


STATIC = re.compile(r'^static\s+')
INLINE = re.compile(r'^inline\s+')


def moved_only(a, b, api):
    """(None, n) when the unit text b is the unity text a minus n lines that the unit's interface header defines instead
    (#ifndef SAN14_MULTI_TU in the module: a load-time value the main unit reads, `static` there and `inline` in the header,
    same initializer); otherwise (index of the first other difference, 0)."""
    n = 0
    for tag, i1, i2, j1, _ in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes():
        if tag == 'equal':
            continue
        if tag != 'delete':
            return i1, 0
        for i in range(i1, i2):
            if STATIC.sub('', a[i]) not in api:
                return i, 0
        n += i2 - i1
    return None, n


def cmd_preprocess(tc):
    check_variants()
    us = units()
    if not us:
        print(json.dumps({'status': 'unit_equivalence_preprocess_passed', 'units': 0}))
        return 0
    base = [f for f in tc['flags'] if not f.startswith('/wd') and not f.startswith('/we')]
    tmp = tempfile.mkdtemp(prefix='san14_unit_eq_')
    jobs = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(8, os.cpu_count() or 4)) as ex:
        for kind, (folder, defs) in VARIANTS.items():
            inc = ['/I' + os.path.join(ROOT, folder.replace('/', os.sep))]
            for r in sorted({u[0] for u in us}):
                args = base + defs + ROOT_DEFS[r] + (inc if r == 'convenience' else [])
                jobs[(kind, r, None)] = ex.submit(preprocess, tc, args, os.path.join(ROOT, 'src', 'native_adapter.cpp'),
                                                  os.path.join(tmp, '%s_%s_unity.i' % (kind, r)))
                jobs[(kind, r, '')] = ex.submit(preprocess, tc, args + ['/DSAN14_MULTI_TU'], os.path.join(ROOT, 'src', 'native_adapter.cpp'),
                                                os.path.join(tmp, '%s_%s_main.i' % (kind, r)))
                for root, unit, path, _ in us:
                    if root == r:
                        jobs[(kind, r, unit)] = ex.submit(preprocess, tc, args + ['/DSAN14_MULTI_TU'], path,
                                                          os.path.join(tmp, '%s_%s_%s.i' % (kind, r, unit)))
        done = {k: v.result() for k, v in jobs.items()}
    failures, compared, moved = [], 0, 0
    for kind in VARIANTS:
        for root, unit, path, members in us:
            unity, uorder = by_file(done[(kind, root, None)])
            mine, morder = by_file(done[(kind, root, unit)])
            member_paths = {os.path.normcase(os.path.join(ROOT, 'src', m)) for m in members}
            first = next((i for i, f in enumerate(morder) if f in member_paths), None)
            if first is None:
                continue   # every member is switched off in this variant
            own = morder[first:]
            api = {INLINE.sub('', s) for s in mine.get(os.path.normcase(os.path.join(ROOT, 'src', 'tu_%s_api.h' % unit)), [])}
            for f in own:
                compared += 1
                if f not in unity:
                    failures.append('%s %s: %s is compiled by the unit but not by the unity build' % (kind, unit, f))
                elif unity[f] != mine[f]:
                    a, b = unity[f], mine[f]
                    bad, n = moved_only(a, b, api)
                    if bad is None:
                        moved += n
                        continue
                    failures.append('%s %s: %s differs at line %d of its text\n  unity: %s\n  unit:  %s' % (
                        kind, unit, f, bad + 1, a[bad] if bad < len(a) else '<end>', b[bad] if bad < len(b) else '<end>'))
            # A member the variant leaves out of the unit may still be compiled by the main unit (SPLIT_MODULE_FIXTURE keeps the
            # guard-only modules in native_adapter.cpp); then the main unit's text of it must equal the unity text.
            main, _ = by_file(done[(kind, root, '')])
            for f in member_paths:
                if f in unity and f not in mine:
                    if f not in main:
                        failures.append('%s %s: %s is compiled by the unity build but by neither the unit nor the main unit' % (kind, unit, f))
                    elif unity[f] != main[f]:
                        a, b = unity[f], main[f]
                        bad = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
                        failures.append('%s main: %s differs at line %d of its text\n  unity: %s\n  main:  %s' % (
                            kind, f, bad + 1, a[bad] if bad < len(a) else '<end>', b[bad] if bad < len(b) else '<end>'))
                    else:
                        compared += 1
            pack, warn, bad = pragma_state(done[(kind, root, None)], member_paths)
            if pack or warn or bad:
                failures.append('%s %s: before the first unit module the unity build has pack depth %d, warning depth %d%s' % (
                    kind, unit, pack, warn, ''.join('\n  ' + b for b in bad)))
    for p in glob.glob(os.path.join(tmp, '*.i')):
        os.remove(p)
    os.rmdir(tmp)
    if failures:
        print('\n'.join(failures))
        print(json.dumps({'status': 'unit_equivalence_preprocess_failed', 'failures': len(failures)}))
        return 1
    print(json.dumps({'status': 'unit_equivalence_preprocess_passed', 'variants': list(VARIANTS), 'units': [u[1] for u in us],
                      'files_compared': compared, 'definitions_moved_to_interface': moved}))
    return 0


SYM = re.compile(r'^\w+\s+\w+\s+(\S+)\s+\S+\s+(?:\(\)\s+)?(External|Static)\s+\|\s+(\S+)')
WRITABLE = {'.data', '.bss', '.tls', '.tls$'}
ANON = re.compile(r'\?A0x[0-9a-f]+', re.I)   # anonymous-namespace names carry a per-file hash


def symbols(tc, obj):
    dumpbin = os.path.join(os.path.dirname(tc['cl']), 'dumpbin.exe')
    env = dict(os.environ)
    env.update(tc['env'])
    p = subprocess.run([dumpbin, '/nologo', '/symbols', obj], env=env, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if p.returncode:
        raise SystemExit('unit_equivalence: dumpbin failed for %s' % obj)
    # ??__E<qualified name>@@YAXXZ initializes the variable ?<qualified name>@@3<type>: compare the qualified names
    rows = [m.groups() for m in (SYM.match(l.strip()) for l in p.stdout.decode('utf-8', 'replace').splitlines()) if m]
    sections = {}
    for sect, cls, name in rows:
        if name.startswith('.'):
            sections.setdefault(sect, name)
    dyn, undef, state = set(), set(), set()
    for sect, cls, name in rows:
        if name.startswith('.'):
            continue
        if name.startswith('??__E') and name.endswith('@@YAXXZ'):
            dyn.add(name[5:-7])
        elif cls == 'External' and sect == 'UNDEF':
            v = re.match(r'\?([^?@][^?]*?)@@3', name)
            if v:
                undef.add(v.group(1))
        elif cls == 'Static' and sections.get(sect) in WRITABLE and not name.startswith(('$', '??_C@')):
            state.add(ANON.sub('?A0x', name))
    return dyn, undef, state


def cmd_audit(tc, dirs, expect=None):
    """expect: {dir: object count} that must be present (the split fixture: native_adapter.obj + one per unit of the root)."""
    failures, objs_seen = [], 0
    for d in dirs:
        objs = sorted(glob.glob(os.path.join(d, '*.obj')))
        if expect and len(objs) != expect.get(d, len(objs)):
            failures.append('%s holds %d objects, expected %d (native_adapter.obj + one per unit)' % (d, len(objs), expect[d]))
        if len(objs) < 2:
            continue
        objs_seen += len(objs)
        info = {o: symbols(tc, o) for o in objs}
        for a in objs:
            for b in objs:
                if a == b:
                    continue
                cross = info[b][0] & info[a][1]
                for v in sorted(cross):
                    failures.append('%s reads %s, which %s initializes dynamically' % (os.path.basename(a), v, os.path.basename(b)))
                if a < b:
                    for v in sorted(info[a][2] & info[b][2]):
                        failures.append('%s and %s each hold their own static %s (state split across units: make it inline)' % (
                            os.path.basename(a), os.path.basename(b), v))
    if failures:
        print('\n'.join(failures))
        print(json.dumps({'status': 'unit_equivalence_audit_failed', 'failures': len(failures)}))
        return 1
    print(json.dumps({'status': 'unit_equivalence_audit_passed', 'objects': objs_seen}))
    return 0


SELFTEST = {
    'a.cpp': 'static int split_state;\ninline int shared_state;\nstatic int counter() {static int local; return ++local;}\n'
             'int touch_a() {return ++split_state + ++shared_state + counter();}\n',
    'b.cpp': 'static int split_state;\ninline int shared_state;\nstatic int counter() {static int local; return ++local;}\n'
             'int touch_b() {return ++split_state + ++shared_state + counter();}\n',
}


def cmd_selftest(tc):
    """The audit must report the namespace static and the local static of a static function that two objects each hold,
    and must not report the C++17 inline variable they share."""
    tmp = tempfile.mkdtemp(prefix='san14_unit_audit_')
    env = dict(os.environ)
    env.update(tc['env'])
    try:
        for name, text in SELFTEST.items():
            open(os.path.join(tmp, name), 'w', encoding='utf-8').write(text)
            p = subprocess.run([tc['cl'], '/nologo', '/c', '/O2', '/std:c++17', name], cwd=tmp, env=env, stdin=subprocess.DEVNULL,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if p.returncode:
                raise SystemExit('unit_equivalence: self-test compile failed\n' + p.stdout.decode('utf-8', 'replace'))
        a, b = (symbols(tc, os.path.join(tmp, n)) for n in ('a.obj', 'b.obj'))
        dup = a[2] & b[2]
        want = any('split_state' in v for v in dup) and any('local' in v and 'counter' in v for v in dup)
        if not want or any('shared_state' in v for v in dup):
            print(json.dumps({'status': 'unit_equivalence_selftest_failed', 'duplicates': sorted(dup)}))
            return 1
        print(json.dumps({'status': 'unit_equivalence_selftest_passed', 'caught': sorted(dup)}))
        return 0
    finally:
        for f in glob.glob(os.path.join(tmp, '*')):
            os.remove(f)
        os.rmdir(tmp)


def main(argv):
    tc = run_checks.toolchain(ROOT)
    if len(argv) == 1:
        rc = cmd_selftest(tc) or cmd_preprocess(tc)
        dirs = [os.path.join(ROOT, d.replace('/', os.sep)) for d in FIXTURE_OBJS]
        per_root = {r: sum(1 for u in units() if u[0] == r) for r in ROOT_DEFS}
        return rc or cmd_audit(tc, dirs, {dirs[0]: 1 + per_root['convenience'], dirs[1]: 1 + per_root['combat']})
    if argv[1] == 'preprocess':
        return cmd_preprocess(tc)
    if argv[1] == 'selftest':
        return cmd_selftest(tc)
    if argv[1] == 'audit' and len(argv) > 2:
        return cmd_audit(tc, argv[2:])
    print(__doc__)
    return 2


if __name__ == '__main__':
    sys.exit(main(sys.argv))
