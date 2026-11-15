#!/usr/bin/env python
"""patch_next: what a new Steam build does to the mod (docs/PATCH_RESILIENCE.md, W2 of docs/MODULARIZATION_PLAN.md).

  python -X utf8 tools/patch_next.py detect  [--exe <installed EXE>]          # read-only header + Steam buildid -> build.json
  python -X utf8 tools/patch_next.py run     --target <sha8|path|auto>        # resolve + guards + hookmap + report + emit, in that order
  python -X utf8 tools/patch_next.py resolve --target ...                     # id values of the target build (gen_signatures) + C++ resolver cross-check
  python -X utf8 tools/patch_next.py guards  --target ...                     # every executed ga::mcmp / guard hash on the target image, features via the gates
  python -X utf8 tools/patch_next.py hookmap --target ...                     # hook_map.py --diff <ref> <target> on the candidate table
  python -X utf8 tools/patch_next.py report  --target ...                     # report.md / report.json from the artifacts above
  python -X utf8 tools/patch_next.py emit    --target ...                     # candidate game_addresses_data.inl (two columns) + fixtures on the target
  python -X utf8 tools/patch_next.py promote --target ... [--confirmed]       # re-base to the target (tools/addr_promote.py), after in-game confirmation
  python -X utf8 tools/patch_next.py selftest                                 # reference == target: everything must be OK (fast; route patch-next)

The reference build is the table column of src/game_addresses_data.inl (ids GA_<RVA of the reference>). The target is the next build, an unpacked
EXE (Steamless, a user step: build/ui_native_analysis/editor83_b<buildid>/game.exe.unpacked.exe; docs/SESSION_RULES.md 7). Everything is read-only
on the game: the installed EXE header, the Steam appmanifest and the unpacked images are read, never written; the game is never started and
no memory is touched. Artifacts go to build/development/patch_next/<target sha8>/ (build.json, addr_table.json, resolve.json, guards.json,
guards.tsv, hookmap_diff.json, report.md, report.json, emit/, stage/).
Result per id: OK / AUTO / ATTENTION (file:line) / BLOCKED (tools/gen_signatures.py); the report adds the build gate table (features that stay
off on the target). Standard library only."""
import os, sys, re, json, shutil, subprocess, argparse, glob, time, collections, bisect

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
import pelib as P                                                # noqa: E402
import ga_table as G                                             # noqa: E402
import gen_signatures as S                                       # noqa: E402
import gen_emit as E                                             # noqa: E402

ROOT = P.ROOT
GAME_DIR = os.path.dirname(ROOT)
PN_DIR = os.path.join(ROOT, 'build', 'development', 'patch_next')
INSTALLED_EXE = 'SAN14CE_KO.exe'            # the build the table is made for (TimeDateStamp of ga_builds); the other language EXEs are listed only
NOT_COVERED = {'ccz_sprites'}               # the CCZ guards load SAN14_CCZ.dll, which the file-only fixture cannot judge (tools/check_addresses.py)
PS = ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File']
ORDER = ('OK', 'AUTO', 'ATTENTION', 'BLOCKED')


def log(*a):
    print(*a, file=sys.stderr, flush=True)


def write_json(path, doc):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(doc, f, indent=1, ensure_ascii=False)
        f.write('\n')


def last_json(text):
    lines = [l for l in text.splitlines() if l.startswith('{')]
    return json.loads(lines[-1]) if lines else None


# ---------------------------------------------------------------- detect
def steam_manifest(game_dir):
    """appmanifest of the game folder (read only): appid, buildid, last update."""
    steamapps = os.path.dirname(os.path.dirname(game_dir))
    for p in glob.glob(os.path.join(steamapps, 'appmanifest_*.acf')):
        kv = dict(re.findall(r'"(\w+)"\s+"([^"]*)"', open(p, encoding='utf-8', errors='replace').read()))
        if kv.get('installdir') == os.path.basename(game_dir):
            return dict(path=p, appid=kv.get('appid'), name=kv.get('name'), buildid=kv.get('buildid'), last_updated=kv.get('LastUpdated'),
                        size_on_disk=kv.get('SizeOnDisk'))
    return None


def detect(exe=None, write=True):
    T = S.load_table()
    ref = T.builds[0]
    exe = exe or os.path.join(GAME_DIR, INSTALLED_EXE)
    out = dict(schema=1, installed=None, variants=[], steam=steam_manifest(GAME_DIR),
               table=dict(reference=ref[0], time_date_stamp='%08X' % ref[1], size_of_image=ref[4]))
    if os.path.isfile(exe):
        h = P.header(exe)
        out['installed'] = dict(path=exe, sha256=h['sha256'], time_date_stamp='%08X' % h['time_date_stamp'], size_of_image=h['size_of_image'], size=h['size'])
    for p in sorted(glob.glob(os.path.join(GAME_DIR, 'SAN14*.exe'))):
        if os.path.abspath(p) != os.path.abspath(exe):
            h = P.header(p)
            out['variants'].append(dict(name=os.path.basename(p), sha256=h['sha256'][:16], time_date_stamp='%08X' % h['time_date_stamp'], size_of_image=h['size_of_image']))
    inst = out['installed']
    if not inst:
        out['verdict'] = 'no installed EXE found (%s)' % exe
    else:
        same = int(inst['time_date_stamp'], 16) == ref[1] and inst['size_of_image'] == ref[4]
        out['verdict'] = 'current' if same else 'new'
        unpacked = None
        for name, path in P.known_images().items():
            hh = P.header(path)
            if '%08X' % hh['time_date_stamp'] == inst['time_date_stamp'] and hh['size_of_image'] == inst['size_of_image']:
                unpacked = (name, path)
        out['target_build'] = unpacked[0] if unpacked else None
        out['unpacked_image'] = unpacked[1] if unpacked else None
        bid = (out['steam'] or {}).get('buildid') or '<buildid>'
        if not unpacked:
            out['next'] = ('unpack the installed EXE with Steamless into build/ui_native_analysis/editor83_b%s/game.exe.unpacked.exe '
                           '(user step, docs/SESSION_RULES.md 7), export it with tools/ghidra_full.py, then run: python -X utf8 tools/patch_next.py run --target auto' % bid)
        elif out['verdict'] == 'current':
            out['next'] = 'the installed build is the reference build of the table: nothing to do (python -X utf8 tools/patch_next.py selftest checks the tooling)'
        else:
            out['next'] = 'python -X utf8 tools/patch_next.py run --target auto'
    if write:
        key = (inst['sha256'][:8] if inst else 'none')
        write_json(os.path.join(PN_DIR, key, 'build.json'), out)
        tb = out.get('target_build')
        if tb and tb != key:
            write_json(os.path.join(PN_DIR, tb, 'build.json'), out)
    return out


def cmd_detect(a):
    out = detect(a.exe)
    print(json.dumps(out, indent=1, ensure_ascii=False))
    return 0


# ---------------------------------------------------------------- context shared by the stages
class Ctx:
    """reference + target images, the analysis (once), the candidate table and the tree the fixtures are built from."""

    def __init__(self, target, ref='current'):
        if target in (None, 'auto'):
            d = detect(write=False)
            if not d.get('unpacked_image'):
                raise SystemExit(d.get('next') or d['verdict'])
            target = d['unpacked_image']
        self.ref, self.tgt = P.open_build(ref), P.open_build(target)
        self.same = self.ref.sha == self.tgt.sha
        self.out = os.path.join(PN_DIR, self.tgt.name)
        os.makedirs(self.out, exist_ok=True)
        self.col = 0 if self.same else 1
        self._A = None
        self.pairs = []

    @property
    def A(self):
        if self._A is None:
            T = S.load_table()
            if T.builds[0][0] != self.ref.name:
                raise SystemExit('the table column is %s but the reference image is %s' % (T.builds[0][0], self.ref.name))
            self._A = S.Analysis(self.ref, self.tgt, T)
            self._A.run()
            doc = S.to_json(self._A, self.ref, self.tgt)
            write_json(os.path.join(self.out, 'addr_table.json'), doc)
            if not self.same:
                self.pairs = S.guard_hash_pairs(self._A)
        return self._A

    # -- candidate + staged tree
    @property
    def candidate(self):
        return os.path.join(self.out, 'emit', 'game_addresses_data.inl')

    def write_candidate(self):
        text = E.emit_text(self.A, self.ref, self.tgt, self.pairs)
        os.makedirs(os.path.dirname(self.candidate), exist_ok=True)
        with open(self.candidate, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
        return text

    @property
    def stage(self):
        return os.path.join(self.out, 'stage')

    @property
    def root(self):
        """tree the fixtures are compiled from: this tree (the committed table) when ref == target, else a staged copy with the candidate table."""
        return ROOT if self.same else self.stage

    @property
    def addr_dir(self):
        return os.path.join(self.root, 'build', 'development', 'addr')

    def make_stage(self):
        if self.same:
            return ROOT
        self.write_candidate()
        if os.path.isdir(self.stage):
            shutil.rmtree(self.stage)
        ign = shutil.ignore_patterns('__pycache__')
        shutil.copytree(os.path.join(ROOT, 'src'), os.path.join(self.stage, 'src'), ignore=ign)
        shutil.copytree(os.path.join(ROOT, 'tests'), os.path.join(self.stage, 'tests'), ignore=ign)
        os.makedirs(os.path.join(self.stage, 'tools'))
        for fn in ('guard_windows.json',):
            if os.path.exists(os.path.join(TOOLS, fn)):
                shutil.copy2(os.path.join(TOOLS, fn), os.path.join(self.stage, 'tools', fn))
        shutil.copy2(self.candidate, os.path.join(self.stage, 'src', 'game_addresses_data.inl'))
        return self.stage

    def build_fixtures(self):
        if not self.same and not os.path.isdir(self.stage):
            self.make_stage()
        r = subprocess.run(PS + [os.path.join(TOOLS, 'build_addr_fixtures.ps1'), '-Project', self.root, '-Out', self.addr_dir],
                           capture_output=True, text=True, encoding='utf-8', errors='replace')
        if r.returncode != 0:
            raise SystemExit('address fixtures failed to build:\n' + (r.stdout + r.stderr)[-3000:])

    def run_fixture(self, exe, *args):
        env = dict(os.environ, GA_FIXTURE_BUILD=str(self.col))
        r = subprocess.run([os.path.join(self.addr_dir, exe)] + list(args), capture_output=True, text=True, encoding='utf-8', errors='replace', env=env)
        return r.returncode, last_json(r.stdout), r.stderr


# ---------------------------------------------------------------- resolve: Python analysis against the C++ runtime resolver
def stage_resolve(ctx, with_cpp=True):
    A = ctx.A
    counts = collections.Counter(r['status'] for r in A.res)
    out = dict(ref=ctx.ref.name, target=ctx.tgt.name, ids=len(A.res), counts=dict(counts), derived=sum(1 for r in A.res if r['derived']),
               container_ids_added=A.aux, cpp=None)
    if with_cpp:
        ctx.build_fixtures()
        scan = os.path.join(ctx.out, 'scan.tsv')
        rc, res, err = ctx.run_fixture('addr_resolver_fixture.exe', ctx.tgt.path, 'scan', scan)
        if rc != 0 or not res:
            raise SystemExit('resolver fixture (scan) failed: %s' % err[-500:])
        rows = [l.rstrip('\n').split('\t') for l in open(scan, encoding='utf-8')]
        if len(rows) != len(A.T.entries):
            raise SystemExit('the C++ resolver knows %d ids, the analysis %d (stale fixture build?)' % (len(rows), len(A.T.entries)))
        mism = []
        compared = 0
        for i, (rv, sv, meth) in enumerate(rows):
            e = A.T.entries[i]
            if int(meth) != e.method or int(rv, 16) != e.rva:
                mism.append(dict(id=e.name, why='C++ table row differs from the analysis (method/rva)'))
                continue
            if e.method == S.M_TABLE or not e.vals[A.ref_name]:
                continue
            compared += 1
            if int(sv, 16) != A.res[i]['tv']:
                mism.append(dict(id=e.name, python='%X' % A.res[i]['tv'], cpp=sv, why='Python mirror and C++ resolver disagree'))
        out['cpp'] = dict(compared=compared, resolved=res['resolved'], mismatch=len(mism), mismatches=mism[:50])
    write_json(os.path.join(ctx.out, 'resolve.json'), out)
    return out


def cmd_resolve(a):
    ctx = Ctx(a.target)
    out = stage_resolve(ctx, not a.no_cpp)
    print(json.dumps({k: v for k, v in out.items() if k != 'cpp'}), json.dumps(out['cpp'] and {k: v for k, v in out['cpp'].items() if k != 'mismatches'}))
    return 1 if out['cpp'] and out['cpp']['mismatch'] else 0


# ---------------------------------------------------------------- guards
def parse_dump(path):
    """(M/A lines [(kind, rva, n, expected bytes)], H lines [(rva, n, hash)]) of a tests/ga_equiv_dump.cpp output."""
    mc, hs = [], []
    for ln in open(path, encoding='utf-8'):
        ps = ln.split()
        if len(ps) == 5 and ps[0] in ('M', 'A'):
            mc.append((ps[0], int(ps[1], 16), int(ps[2]), bytes.fromhex(ps[3])))
        elif len(ps) == 4 and ps[0] == 'H':
            hs.append((int(ps[1], 16), int(ps[2]), int(ps[3], 16)))
    return mc, hs


def reference_dump(ctx):
    """guard expectations of the reference build (tests/ga_equiv_dump.cpp on the reference image)."""
    out = os.path.join(ctx.out, 'equiv_ref')
    r = subprocess.run(PS + [os.path.join(TOOLS, 'build_ga_equiv_dump.ps1'), '-Out', out, '-Exe', ctx.ref.path], capture_output=True, text=True,
                       encoding='utf-8', errors='replace')
    if r.returncode != 0:
        raise SystemExit('guard dump failed:\n' + (r.stdout + r.stderr)[-3000:])
    return out


def stage_guards(ctx, dump_dir=None, with_dump=True):
    A = ctx.A
    rb, tb = A.rb, A.tb
    src = S.source_lines()
    ents = A.T.entries
    by_container = collections.defaultdict(list)
    for i, e in enumerate(ents):
        if e.kind in (S.K_FUNC, S.K_MID) and e.vals[A.ref_name] and A.res[i]['tv']:
            by_container[rb.container(e.rva)].append((e.rva, i))
    for v in by_container.values():
        v.sort()
    tv_sorted = sorted((A.res[i]['tv'], i) for i, e in enumerate(ents) if e.kind in (S.K_FUNC, S.K_MID) and A.res[i]['tv'])
    tv_keys = [x[0] for x in tv_sorted]
    rows = []

    def locate(rva):
        c = rb.container(rva)
        cand = [x for x in by_container.get(c, ()) if x[0] <= rva]
        if not cand:
            return None
        base, i = cand[-1]
        return i, rva - base

    def judge(kind, rva, n, expected, h):
        loc = locate(rva)
        if loc is None:
            return dict(kind=kind, ref_rva='%X' % rva, n=n, id='', verdict='UNMAPPED', note='no resolved id in the function around the guard')
        i, off = loc
        e = ents[i]
        trv = A.res[i]['tv'] + off
        got = tb.b.raw(trv, n)
        c = rb.container(rva)
        _, mask = rb.b.masked(c, (rva - c) + n)
        mask = mask[rva - c:rva - c + n]
        if h is None:
            # runtime rule of ga::mcmp_at on an unknown build: wildcard bits only within 32 bytes of the nearest id value at or below
            k = bisect.bisect_right(tv_keys, trv) - 1
            wild, woff = 0, 0
            if k >= 0 and trv - tv_keys[k] < 32:
                wild, woff = ents[tv_sorted[k][1]].wild, trv - tv_keys[k]
            run_ok = got is not None and all(got[q] == expected[q] or (woff + q < 32 and (wild >> (woff + q)) & 1) for q in range(n))
            mask_ok = got is not None and all(got[q] == expected[q] or mask[q] for q in range(n))
        else:
            run_ok = got is not None and S.fnv(got) == h
            refb = rb.b.raw(rva, n)
            mask_ok = got is not None and refb is not None and all(got[q] == refb[q] or mask[q] for q in range(n))
        verdict = 'OK' if run_ok else ('RELOC' if mask_ok else 'CHANGED')
        return dict(kind=kind, ref_rva='%X' % rva, n=n, id=e.name, id_offset=off, target_rva='%X' % trv, verdict=verdict,
                    uses=['%s:%d' % x for x in src.get(e.name, [])[:3]],
                    note={'OK': '', 'RELOC': 'bytes equal modulo relocation fields: the runtime guard does not tolerate them here',
                          'CHANGED': 'guarded bytes changed in the target'}[verdict])

    if with_dump:
        dump_dir = dump_dir or reference_dump(ctx)
        seen = set()
        for v in ('convenience', 'combat'):
            p = os.path.join(dump_dir, v + '.txt')
            if not os.path.exists(p):
                continue
            mc, hs = parse_dump(p)
            for kind, rva, n, exp in mc:
                if (kind, rva, n, exp) not in seen:
                    seen.add((kind, rva, n, exp))
                    rows.append(judge(kind, rva, n, exp, None))
            for rva, n, h in hs:
                if ('H', rva, n, h) not in seen:
                    seen.add(('H', rva, n, h))
                    rows.append(judge('H', rva, n, None, h))
    # features: the address gates and guards of the real modules run on the target image (tests/address_guard_fixture.cpp)
    ctx.build_fixtures()
    fx = {}
    for exe in ('address_guard_convenience.exe', 'address_guard_combat.exe'):
        rc, res, err = ctx.run_fixture(exe, ctx.tgt.path)
        if rc != 0 or not res:
            raise SystemExit('%s failed (exit %s): %s' % (exe, rc, err[-300:]))
        fx[res['module']] = dict(disabled=res['disabled'], not_covered=[f for f in res['disabled'] if f in NOT_COVERED], guards_ok=res['guards_ok'],
                                 unresolved_ids=res['unresolved_ids'], guard_single_failed=sorted(k for k, v in res.get('guard_single', {}).items() if v is False))
    bad = [r for r in rows if r['verdict'] != 'OK']
    out = dict(ref=ctx.ref.name, target=ctx.tgt.name, guards=len(rows), counts=dict(collections.Counter(r['verdict'] for r in rows)),
               failing=bad[:200], fixtures=fx)
    write_json(os.path.join(ctx.out, 'guards.json'), out)
    with open(os.path.join(ctx.out, 'guards.tsv'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('kind\tref_rva\tn\tid\tid_offset\ttarget_rva\tverdict\tuses\n')
        for r in rows:
            f.write('\t'.join(str(r.get(k, '')) for k in ('kind', 'ref_rva', 'n', 'id', 'id_offset', 'target_rva', 'verdict')) + '\t' + ' '.join(r.get('uses', [])) + '\n')
    return out


def cmd_guards(a):
    ctx = Ctx(a.target)
    out = stage_guards(ctx, with_dump=not a.no_dump)
    print(json.dumps({k: v for k, v in out.items() if k != 'failing'}))
    return 0


# ---------------------------------------------------------------- hookmap
def stage_hookmap(ctx):
    import hook_map
    if not ctx.same:
        ctx.write_candidate()
    out = hook_map.build_diff(ROOT, ctx.ref.name, ctx.tgt.name, None if ctx.same else ctx.candidate)
    write_json(os.path.join(ctx.out, 'hookmap_diff.json'), out)
    return out


def cmd_hookmap(a):
    ctx = Ctx(a.target)
    out = stage_hookmap(ctx)
    print(json.dumps({k: v for k, v in out.items() if k not in ('findings', 'unresolved')}), 'unresolved', len(out['unresolved']), 'findings', len(out['findings']))
    return 1 if out['errors'] else 0


# ---------------------------------------------------------------- emit
def stage_emit(ctx):
    A = ctx.A
    text = ctx.write_candidate()
    rows = []
    for i, e in enumerate(A.T.entries):
        r = A.res[i]
        rows.append('\t'.join((e.name, '%X' % e.rva, '%X' % r['tv'], r['status'], G.METHOD[e.method], 'derived' if r['derived'] else '', r['note'])))
    with open(os.path.join(ctx.out, 'emit', 'ids.tsv'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('id\tref_rva\ttarget_rva\tstatus\tmethod\tderived\tnote\n' + '\n'.join(rows) + '\n')
    out = dict(candidate=ctx.candidate, bytes=len(text), columns=1 if ctx.same else 2, hash_pairs=len(ctx.pairs), static=None)
    # address-static on the target image: the resolver of the real header over the target EXE returns the candidate column for every id
    ctx.build_fixtures()
    rc, res, err = ctx.run_fixture('addr_resolver_fixture.exe', ctx.tgt.path)
    out['static'] = res or {'status': 'failed', 'stderr': err[-500:]}
    if ctx.same:
        cur = open(S.INL, encoding='utf-8').read()
        out['equals_committed_table'] = cur.split('\n', 2)[2] == text.split('\n', 2)[2]
    write_json(os.path.join(ctx.out, 'emit', 'emit.json'), out)
    return out


def cmd_emit(a):
    ctx = Ctx(a.target)
    out = stage_emit(ctx)
    print(json.dumps(out, ensure_ascii=False))
    return 0 if out['static'] and out['static'].get('status') == 'address_resolver_fixture_passed' else 1


# ---------------------------------------------------------------- report
def module_map():
    """src file name -> [features.json module]"""
    j = json.load(open(os.path.join(ROOT, 'development', 'features.json'), encoding='utf-8'))
    out = collections.defaultdict(list)
    for name, m in j['modules'].items():
        for p in m.get('owns', []):
            if p.startswith('src/'):
                out[p[4:]].append(name)
    return out


def worst(a, b):
    return a if ORDER.index(a) >= ORDER.index(b) else b


def build_report(ctx, resolve, guards, hook):
    A = ctx.A
    mods = module_map()
    ids = collections.Counter(r['status'] for r in A.res)
    src = S.source_lines()
    notes = {'ATTENTION': [], 'BLOCKED': [], 'AUTO': []}
    mod_status = collections.defaultdict(lambda: dict(status='OK', ids=collections.Counter()))

    def touch(files, status):
        for f in files:
            for m in mods.get(f, ()):
                ms = mod_status[m]
                ms['status'] = worst(ms['status'], status)
                ms['ids'][status] += 1

    for i, e in enumerate(A.T.entries):
        r = A.res[i]
        if r['status'] in notes:
            notes[r['status']].append(dict(id=e.name, ref='%X' % e.rva, target='%X' % r['tv'], kind=G.KIND[e.kind], method=G.METHOD[e.method], note=r['note'],
                                           derived=r['derived'], uses=['%s:%d' % x for x in src.get(e.name, [])[:4]]))
        if r['status'] in ORDER:
            touch(sorted(e.users), r['status'])
    if guards:
        for g in guards['failing']:
            if g.get('id'):
                e = A.T.entries[A.rva_idx[int(g['id'][3:], 16)]]
                touch(sorted(e.users), 'ATTENTION')
    gates = {}
    gate_off = set()
    if guards:
        for mod, fx in guards['fixtures'].items():
            gates[mod] = dict(disabled=[f for f in fx['disabled'] if f not in NOT_COVERED], not_covered=fx['not_covered'], guards_ok=fx['guards_ok'],
                              guard_single_failed=fx['guard_single_failed'])
            gate_off |= set(gates[mod]['disabled'])
    cpp_bad = bool(resolve and resolve.get('cpp') and resolve['cpp']['mismatch'])
    guard_bad = bool(guards and (guards['counts'].get('CHANGED', 0) or guards['counts'].get('RELOC', 0) or guards['counts'].get('UNMAPPED', 0)))
    if not (ids['ATTENTION'] or ids['BLOCKED'] or ids['AUTO'] or gate_off or guard_bad or cpp_bad or (hook and (hook['errors'] or hook['unresolved']))):
        verdict = 'ALL OK'
    elif ids['BLOCKED'] or gate_off or cpp_bad or (hook and hook['errors']):
        verdict = 'BLOCKED'
    elif ids['ATTENTION'] or guard_bad or (hook and hook['unresolved']):
        verdict = 'ATTENTION'
    else:
        verdict = 'AUTO'
    return dict(ref=ctx.ref.name, target=ctx.tgt.name, same_image=ctx.same, verdict=verdict, ids=dict(ids), total_ids=len(A.res), derived=sum(1 for r in A.res if r['derived']),
                resolve_cpp=resolve and resolve.get('cpp') and {k: v for k, v in resolve['cpp'].items() if k != 'mismatches'},
                guards=guards and {k: v for k, v in guards.items() if k != 'failing'},
                hookmap=hook and dict({k: v for k, v in hook.items() if k not in ('findings', 'unresolved')}, unresolved=len(hook['unresolved']), findings=len(hook['findings'])),
                modules={m: dict(status=v['status'], ids=dict(v['ids'])) for m, v in sorted(mod_status.items())},
                gates=gates, attention=notes['ATTENTION'], blocked=notes['BLOCKED'], auto=notes['AUTO'][:50], auto_total=len(notes['AUTO']),
                guard_failures=(guards or {}).get('failing', [])[:100], hook_unresolved=(hook or {}).get('unresolved', []), hook_findings=(hook or {}).get('findings', []))


def report_md(R):
    L = []
    w = L.append
    w('# patch_next report: %s -> %s' % (R['ref'], R['target']))
    w('')
    w('**Verdict: %s**%s' % (R['verdict'], ' (reference == target: self test of the tooling)' if R['same_image'] else ''))
    w('')
    w('## Ids (%d)' % R['total_ids'])
    w('')
    w('| status | ids |')
    w('|---|---|')
    for s in ('OK', 'AUTO', 'ATTENTION', 'BLOCKED', 'DENIED'):
        w('| %s | %d |' % (s, R['ids'].get(s, 0)))
    w('')
    w('%d ids needed a new method derived from the reference image; C++ resolver cross-check: %s.' % (
        R['derived'], 'not run' if not R['resolve_cpp'] else '%d compared, %d mismatches' % (R['resolve_cpp']['compared'], R['resolve_cpp']['mismatch'])))
    if R['attention']:
        w('')
        w('## ATTENTION: code around the id changed or moved (a person decides)')
        w('')
        w('| id | ref | target | note | uses |')
        w('|---|---|---|---|---|')
        for x in R['attention']:
            w('| %s | %s | %s | %s | %s |' % (x['id'], x['ref'], x['target'], x['note'], ', '.join(x['uses'])))
    if R['blocked']:
        w('')
        w('## BLOCKED: not found, the dependent feature stays off')
        w('')
        w('| id | ref | kind/method | note | uses |')
        w('|---|---|---|---|---|')
        for x in R['blocked']:
            w('| %s | %s | %s/%s | %s | %s |' % (x['id'], x['ref'], x['kind'], x['method'], x['note'], ', '.join(x['uses'])))
    if R['auto_total']:
        w('')
        w('## AUTO: moved, function body identical (%d, first %d listed)' % (R['auto_total'], len(R['auto'])))
        w('')
        w(', '.join('%s->%s' % (x['id'], x['target']) for x in R['auto']))
    g = R['guards']
    if g:
        w('')
        w('## Guards on the target image')
        w('')
        w('%d guards (ga::mcmp / mcmp_at / guard hashes executed by the fixtures): %s. OK = passes at run time on the unknown-build rules, RELOC = bytes equal '
          'modulo relocation fields (the guard needs a wildcard), CHANGED = the guarded bytes changed.' % (g['guards'], json.dumps(g['counts'])))
        if R['guard_failures']:
            w('')
            w('| kind | ref rva | id+off | target rva | verdict | uses |')
            w('|---|---|---|---|---|---|')
            for x in R['guard_failures']:
                w('| %s | %s | %s+%s | %s | %s | %s |' % (x['kind'], x['ref_rva'], x.get('id', ''), x.get('id_offset', ''), x.get('target_rva', ''), x['verdict'], ', '.join(x.get('uses', []))))
    h = R['hookmap']
    if h:
        w('')
        w('## Hook map (hook_map.py --diff %s %s)' % (R['ref'], R['target']))
        w('')
        w('%d active sites with an address: %d same, %d moved, %d unresolved in the target, %d findings only in the target (%d errors).' % (
            h['active_sites'], h['same'], h['moved'], h['unresolved'], h['findings'], h['errors']))
        for u in R['hook_unresolved']:
            w('- unresolved %s (%s %s): %s' % (u['ga'], u['owner'], u['kind'], u['src']))
        for f in R['hook_findings']:
            w('- %s %s %s' % (f['severity'].upper(), f['rule'], f['msg']))
    w('')
    w('## Build gate table (features switched off on the target)')
    w('')
    if R['gates']:
        w('| module DLL | switched off | not judged by the file-only fixture | guards_ok | single guards failing |')
        w('|---|---|---|---|---|')
        for m, x in sorted(R['gates'].items()):
            w('| %s | %s | %s | %s | %s |' % (m, ', '.join(x['disabled']) or '-', ', '.join(x['not_covered']) or '-', x['guards_ok'], ', '.join(x['guard_single_failed']) or '-'))
    else:
        w('not run (resolve/guards only)')
    w('')
    w('## Modules (features.json) by worst status of the ids / guards their sources use')
    w('')
    w('| module | status | ids by status |')
    w('|---|---|---|')
    for m, x in R['modules'].items():
        if x['status'] != 'OK' or R['same_image'] is False:
            w('| %s | %s | %s |' % (m, x['status'], json.dumps(x['ids'])))
    if not any(x['status'] != 'OK' for x in R['modules'].values()):
        w('| (all %d modules that use address ids) | OK | |' % len(R['modules']))
    w('')
    return '\n'.join(L) + '\n'


def stage_report(ctx, resolve=None, guards=None, hook=None):
    def load(name):
        p = os.path.join(ctx.out, name)
        return json.load(open(p, encoding='utf-8')) if os.path.exists(p) else None
    resolve = resolve or load('resolve.json')
    guards = guards or load('guards.json')
    hook = hook or load('hookmap_diff.json')
    R = build_report(ctx, resolve, guards, hook)
    write_json(os.path.join(ctx.out, 'report.json'), R)
    with open(os.path.join(ctx.out, 'report.md'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(report_md(R))
    return R


def cmd_report(a):
    ctx = Ctx(a.target)
    R = stage_report(ctx)
    print('%s: verdict %s, ids %s -> %s' % (ctx.tgt.name, R['verdict'], json.dumps(R['ids']), os.path.join(ctx.out, 'report.md')))
    return 0


def cmd_run(a):
    ctx = Ctx(a.target)
    d = detect()
    log('detect:', d['verdict'])
    t0 = time.time()
    resolve = stage_resolve(ctx)
    log('resolve: %s (%.0fs)' % (json.dumps(resolve['counts']), time.time() - t0))
    guards = stage_guards(ctx, with_dump=not a.no_dump)
    log('guards: %s' % json.dumps(guards['counts']))
    hook = stage_hookmap(ctx)
    log('hookmap: %d unresolved, %d findings only in the target' % (len(hook['unresolved']), len(hook['findings'])))
    em = stage_emit(ctx)
    R = stage_report(ctx, resolve, guards, hook)
    print('verdict %s | ids %s | emit %s | report %s' % (R['verdict'], json.dumps(R['ids']), em['candidate'], os.path.join(ctx.out, 'report.md')))
    return 0


# ---------------------------------------------------------------- promote
def cmd_promote(a):
    ctx = Ctx(a.target)
    if ctx.same:
        raise SystemExit('the target is the reference build: nothing to promote')
    if not os.path.exists(ctx.candidate):
        raise SystemExit('run `patch_next.py emit` first')
    ap = [sys.executable, '-X', 'utf8', os.path.join(TOOLS, 'addr_promote.py')]
    common = ['--from', ctx.ref.name, '--to', ctx.tgt.name, '--exe', ctx.tgt.path]
    if not a.confirmed:
        r = subprocess.run(ap + ['plan'] + common + ['--table', ctx.candidate], capture_output=True, text=True, encoding='utf-8', errors='replace')
        print(r.stdout[-6000:], r.stderr[-1500:])
        print('plan only: nothing was written. After the in-game check (addr_build = %s, addr_unresolved 0 or the expected features off) run again with --confirmed.' % ctx.tgt.name)
        return r.returncode
    tracked = subprocess.run(['git', '-C', ROOT, 'status', '--porcelain', '--', 'src/game_addresses_data.inl'], capture_output=True, text=True).stdout.strip()
    if tracked:
        raise SystemExit('src/game_addresses_data.inl has uncommitted changes: commit or discard them first (promote rewrites it)')
    shutil.copy2(ctx.candidate, S.INL)
    r = subprocess.run(ap + ['apply'] + common)
    if r.returncode:
        subprocess.run(['git', '-C', ROOT, 'checkout', '--', 'src/game_addresses_data.inl'])
        raise SystemExit('addr_promote apply failed; src/game_addresses_data.inl restored')
    print('promoted to %s. Next (docs/PATCH_RESILIENCE.md, new patch step 3-5): addr_promote sweep --from-exe <old unpacked> --old-table <candidate> --apply; '
          'converge --exe; build_ga_equiv_dump + ga_equiv_compare; check_feature.ps1 -Feature dll-split -Build; in-game check; commit.' % ctx.tgt.name)
    return 0


# ---------------------------------------------------------------- self test (reference == target)
def cmd_selftest(a):
    t0 = time.time()
    fails = []

    def check(name, ok, msg=''):
        print('%s %s %s' % ('PASS' if ok else 'FAIL', name, msg))
        if not ok:
            fails.append(name)

    ctx = Ctx('current')
    A = ctx.A
    counts = collections.Counter(r['status'] for r in A.res)
    check('ref=target ids', set(counts) <= {'OK', 'DENIED'} and counts['OK'] > 2000, json.dumps(dict(counts)))
    check('no derived method needed', not any(r['derived'] for r in A.res) and A.aux == 0)
    check('methods reproduce the reference values', not any(r['note'] for r in A.res), '%d notes' % sum(1 for r in A.res if r['note']))
    text = ctx.write_candidate()
    cur = open(S.INL, encoding='utf-8').read()
    check('candidate == committed table', cur.split('\n', 2)[2] == text.split('\n', 2)[2], '%d bytes' % len(text))
    d = detect(write=False)
    check('detect', d.get('verdict') in ('current', 'new') or d['installed'] is None, '%s unpacked=%s' % (d['verdict'], d.get('target_build')))
    if d.get('installed'):
        check('installed EXE is the reference build', d['verdict'] == 'current', d['verdict'])
    import hook_map
    hm = hook_map.build_diff(ROOT, ctx.ref.name, ctx.tgt.name)
    check('hookmap diff ref=target', hm['errors'] == 0 and not hm['unresolved'] and not hm['findings'] and hm['moved'] == 0,
          '%d sites, %d same' % (hm['active_sites'], hm['same']))
    ctx.build_fixtures()
    resolve = stage_resolve(ctx)
    check('C++ resolver agrees with the analysis', resolve['cpp']['mismatch'] == 0, '%d compared' % resolve['cpp']['compared'])
    g = stage_guards(ctx, with_dump=False)
    off = {m: [f for f in x['disabled'] if f not in NOT_COVERED] for m, x in g['fixtures'].items()}
    check('feature gates on the target image', not any(off.values()) and all(x['guards_ok'] for x in g['fixtures'].values()), json.dumps(off))
    R = stage_report(ctx, resolve, g, hm)
    check('report verdict', R['verdict'] == 'ALL OK', R['verdict'])
    print(json.dumps({'status': 'patch_next_selftest_failed' if fails else 'patch_next_selftest_passed', 'failures': fails, 'seconds': round(time.time() - t0)}))
    return 1 if fails else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('detect')
    p.add_argument('--exe')
    for name in ('run', 'resolve', 'guards', 'hookmap', 'report', 'emit', 'promote'):
        p = sub.add_parser(name)
        p.add_argument('--target', default='auto', help='sha8 of an unpacked image below build/ui_native_analysis, a path to the unpacked EXE, or auto (the image of the installed EXE)')
        if name == 'resolve':
            p.add_argument('--no-cpp', action='store_true', help='skip the C++ resolver cross-check (no fixture build)')
        if name in ('run', 'guards'):
            p.add_argument('--no-dump', action='store_true', help='skip the per-guard evaluation (needs the guard dump of the reference build)')
        if name == 'promote':
            p.add_argument('--confirmed', action='store_true', help='the target build was checked in the game: really rewrite the table and the sources')
    sub.add_parser('selftest')
    a = ap.parse_args()
    return {'detect': cmd_detect, 'run': cmd_run, 'resolve': cmd_resolve, 'guards': cmd_guards, 'hookmap': cmd_hookmap, 'report': cmd_report, 'emit': cmd_emit,
            'promote': cmd_promote, 'selftest': cmd_selftest}[a.cmd](a)


if __name__ == '__main__':
    sys.exit(main())
