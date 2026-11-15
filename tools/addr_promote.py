#!/usr/bin/env python
"""addr_promote: re-base the address table and every source that spells original addresses to a new reference build
(docs/MODULARIZATION_PLAN.md 4 and 5, docs/PATCH_RESILIENCE.md). Record-and-patch: it changes files and writes a record of every change,
so the rewrite is auditable and can be re-run (at every future Steam patch: promote the checked build to the new reference).

  python -X utf8 tools/addr_promote.py plan  --from <build> --to <build> --exe <unpacked exe of --to>      # report only, nothing written
  python -X utf8 tools/addr_promote.py apply --from <build> --to <build> --exe <unpacked exe of --to>     # rewrite + record
  python -X utf8 tools/addr_promote.py guards --exe <unpacked exe> --dump <dir with convenience.txt/combat.txt> [--apply]
        # guard literals: every executed ga::mcmp / ga::mcmp_at whose expected bytes differ from the image bytes at the guarded RVA
        # (tests/ga_equiv_dump.cpp output) -> the source byte list is rewritten to the image bytes; static ga::mcmp(image,GA_x,off,arr,n) sites too
  python -X utf8 tools/addr_promote.py check --exe <unpacked exe> [--dump <dir>]
        # nothing may differ: table values vs image sanity, GH literals vs FNV-1a of the image bytes, guard literals vs image bytes

What `apply` does (every change is one line in <record>/changes.tsv):
  table   src/game_addresses_data.inl is parsed (tools/ga_table.py) and written again with ONE build column: ids are named GA_<RVA of --to>
          (ids that share a new RVA merge, users unioned); an id whose --to value is 0 is DENIED: if no source mentions it and no other id
          uses it as a container it is dropped (listed in dropped.tsv), else it is kept as GA_X<old RVA> with value 0 (denied.tsv);
          ga_wild is recomputed from the --to image, probes/observers follow the new names/RVAs, ga_hash_pairs / exact-byte windows are removed.
  ids     GA_<old> -> GA_<new> in tracked src/tests/tools/development files (one pass; names that are no old id are left, listed in unknown.tsv)
  hash    GH(0x<old FNV>ull) -> GH(0x<FNV of the same range in --to>ull) through ga_hash_pairs (an unpaired literal stays; a pair with 0 = range
          changed becomes 0x0, the guard then never matches exactly as before)
  tests   hex literals that equal an old id RVA in tests/*.cpp|inl|h (and the listed verifier scripts) -> the new RVA (values < 0x10000 only for listed spots)
Never touches a game binary beyond reading the --to unpacked EXE. Standard library only.
"""
import argparse, bisect, collections, json, os, re, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import ga_table as G                                                      # noqa: E402

INL = os.path.join(ROOT, 'src', 'game_addresses_data.inl')
ID_RE = re.compile(r'\bGA_([0-9A-F]{3,8})\b')
GH_RE = re.compile(r'GH\(\s*0[xX]([0-9A-Fa-f]+)([uU][lL]{2})\s*\)')
HEX_RE = re.compile(r'0[xX]([0-9A-Fa-f]{4,9})([uUlL]{0,3})(?![0-9A-Za-z_])')
# files whose GA_ tokens are not ids of the table (generators / self tests / this tool) or are history
ID_EXCLUDE = {'tools/addr_promote.py', 'tools/ga_table.py', 'tools/gen_emit.py', 'tools/gen_signatures.py', 'tools/observer_hooks.json',
              'tests/verify_hook_map.py', 'tests/verify_feature_deps.py', 'tests/ga_expand.py', 'src/game_addresses_data.inl', 'src/game_addresses.h'}
# fixture literal spots below 0x10000 that are real RVAs (file, value)
SMALL_RVA = {('tests/region_officer_fixture.cpp', 0xEDE0), ('tests/terrain_edit_ui_fixture.cpp', 0xF560)}
# files whose byte lists the sweep must not convert (W1-E: the previous-build capture form, deleted by 1-F)
SWEEP_SKIP_FILES = {'src/building_destruction_profile.h'}
# files in which hex literals equal to an old id RVA are converted (C++ fixtures); python verifiers are added by --py
LIT_GLOBS = ('tests/',)
LIT_EXT = ('.cpp', '.inl', '.h')
LIT_SKIP = {'tests/ga_equiv_dump.cpp', 'tests/layout_fixture.cpp', 'tests/layout_loader_mock.cpp'}   # synthetic images, no game RVAs (checked by hand)


def tracked(*dirs):
    out = subprocess.run(['git', '-C', ROOT, 'ls-files'] + list(dirs), capture_output=True, text=True, encoding='utf-8').stdout.split('\n')
    return [f for f in out if f and os.path.isfile(os.path.join(ROOT, f))]


RECORDS = 'development/addr_promote/'          # the records of earlier promotions are history: their GA_ names never move


def rewritable(*dirs):
    return [f for f in tracked(*dirs) if not f.startswith(RECORDS)]


def rd(path):
    return open(os.path.join(ROOT, path), 'rb').read().decode('utf-8', errors='surrogateescape')


def wr(path, text):
    open(os.path.join(ROOT, path), 'wb').write(text.encode('utf-8', errors='surrogateescape'))


# ---------------------------------------------------------------- PE image
class Image:
    def __init__(self, path):
        self.path = path
        d = open(path, 'rb').read()
        e = struct.unpack_from('<I', d, 0x3C)[0]
        n = struct.unpack_from('<H', d, e + 6)[0]
        so = struct.unpack_from('<H', d, e + 20)[0]
        self.d = d
        self.secs = []
        for i in range(n):
            o = e + 24 + so + i * 40
            nm = d[o:o + 8].rstrip(b'\0').decode('latin1')
            vs, va, rs, ro = struct.unpack_from('<IIII', d, o + 8)
            self.secs.append((nm, va, vs, rs, ro))
        self.timestamp = struct.unpack_from('<I', d, e + 8)[0]
        self.size = struct.unpack_from('<I', d, e + 24 + 56)[0]

    def read(self, rva, n):
        for nm, va, vs, rs, ro in self.secs:
            if va <= rva < va + max(vs, rs):
                o = rva - va
                if o + n <= rs:
                    return self.d[ro + o:ro + o + n]
                got = self.d[ro + o:ro + rs] if o < rs else b''
                return got + bytes(n - len(got))
        return None


FNV_OFF, FNV_PRIME, M64 = 0xcbf29ce484222325, 0x100000001b3, (1 << 64) - 1


def fnv(b):
    h = FNV_OFF
    for x in b:
        h = ((h ^ x) * FNV_PRIME) & M64
    return h


# ---------------------------------------------------------------- code spans (outside comments / strings), same rules as tools/addrlib.py
def code_spans(lines):
    in_block = False
    for ln, s in enumerate(lines):
        i, n, start = 0, len(s), 0
        spans = []
        while i < n:
            if in_block:
                j = s.find('*/', i)
                if j < 0:
                    i = n
                    start = n
                else:
                    i = j + 2
                    start = i
                    in_block = False
                continue
            c = s[i]
            if c == '/' and i + 1 < n and s[i + 1] == '/':
                spans.append((start, i))
                start = n
                i = n
                break
            if c == '/' and i + 1 < n and s[i + 1] == '*':
                spans.append((start, i))
                in_block = True
                i += 2
                continue
            if c == '"' or c == "'":
                q = c
                spans.append((start, i))
                i += 1
                while i < n and s[i] != q:
                    i += 2 if s[i] == '\\' else 1
                i += 1
                start = i
                continue
            i += 1
        if start < n:
            spans.append((start, n))
        for a, b in spans:
            if b > a:
                yield ln, a, b


# ---------------------------------------------------------------- the promotion of the table
def build_plan(T, frm, to, exe_image, mentions):
    """returns dict(new table, idmap {old name -> new name or None}, dropped [..], denied [..], merged [..], wild_changed n)."""
    assert frm in T.builds[0][0] and len(T.builds) == 2 and T.builds[1][0] == to, 'the table must carry builds [%s, %s]' % (frm, to)
    ents = T.entries
    n = len(ents)
    # containers referenced by other entries (mid a / slotfn a / dataref a)
    referenced = set()
    for e in ents:
        if e.method in (2, 4, 5):
            referenced.add(e.a)
    keep, dropped, denied = [], [], []
    for i, e in enumerate(ents):
        v = e.vals[to]
        if v:
            keep.append(i)
        elif e.name in mentions or e.users or i in referenced:
            denied.append(i)
        else:
            dropped.append(i)
    # merge ids sharing a new RVA (representative = first by preference: has method, func kind)
    by_new = collections.OrderedDict()
    for i in keep:
        by_new.setdefault(ents[i].vals[to], []).append(i)
    merged = []
    rep_of = {}
    for v, idxs in by_new.items():
        rep = sorted(idxs, key=lambda i: (ents[i].method == 0, ents[i].kind != 0, i))[0]
        for i in idxs:
            rep_of[i] = rep
        if len(idxs) > 1:
            merged.append((v, [ents[i].name for i in idxs], ents[rep].name))
    order = sorted(by_new.keys())
    newname = {}
    for v in order:
        for i in by_new[v]:
            newname[i] = 'GA_%X' % v
    for i in denied:
        newname[i] = 'GA_X%X' % ents[i].rva
    # new entries in order: by new rva, denied (old rva order) last
    new_idx_of_rep = {}
    new_entries = []
    for v in order:
        rep = rep_of[by_new[v][0]]
        new_idx_of_rep[rep] = len(new_entries)
        e = G.Entry()
        o = ents[rep]
        e.name = 'GA_%X' % v
        e.rva = v
        e.kind, e.method, e.len, e.a, e.b, e.c, e.d = o.kind, o.method, o.len, o.a, o.b, o.c, o.d
        e.pat, e.mask, e.rname = o.pat, o.mask, o.rname
        users = set()
        for i in by_new[v]:
            users |= ents[i].users
        e.users = users
        e.wild = o.wild
        e.vals = {to: v}
        new_entries.append(e)
    for i in sorted(denied, key=lambda i: ents[i].rva):
        o = ents[i]
        new_idx_of_rep[i] = len(new_entries)
        e = G.Entry()
        e.name = newname[i]
        e.rva, e.kind, e.method, e.len, e.a, e.b, e.c, e.d = o.rva, o.kind, o.method, o.len, o.a, o.b, o.c, o.d
        e.pat, e.mask, e.rname = o.pat, o.mask, o.rname
        e.users = set(o.users)
        e.wild = o.wild
        e.vals = {to: 0}
        new_entries.append(e)
    # entry index (old) -> new index
    remap = {}
    for i in range(n):
        if i in rep_of:
            remap[i] = new_idx_of_rep[rep_of[i]]
        elif i in new_idx_of_rep:
            remap[i] = new_idx_of_rep[i]
    for e in new_entries:
        if e.method in (2, 4, 5):
            if e.a not in remap:
                raise SystemExit('container of %s was dropped' % e.name)
            e.a = remap[e.a]
    # wildcard masks from the --to image (code ids): the first 32 bytes at the id, relocatable fields marked
    wild_changed = 0
    wp = None
    if exe_image is not None:
        import pelib
        nb = pelib.Build(exe_image.path)
        wp = (nb, nb.candidates())
    if wp:
        nb, cands = wp
        for e in new_entries:
            if e.kind in (0, 1) and e.vals[to]:
                k = bisect.bisect_right(cands, e.rva) - 1
                c = cands[k]
                pos = e.rva - c
                _, mk = nb.masked(c, pos + 40)
                w = 0
                for b in range(32):
                    if pos + b < len(mk) and mk[pos + b]:
                        w |= 1 << b
                if w != e.wild:
                    wild_changed += 1
                    e.wild = w
    NT = G.Table()
    NT.entries = new_entries
    NT.files = T.files
    ti = [b for b in T.builds if b[0] == to][0]
    NT.builds = [(to, exe_image.timestamp if exe_image else ti[1], ti[2], ti[3], exe_image.size if exe_image else ti[4])]
    old_by_name = {e.name: i for i, e in enumerate(ents)}
    NT.probes = [(newname[old_by_name[nm]], b) for nm, b in T.probes]
    NT.observers = [((rv[1],), ln, bs) for rv, ln, bs in T.observers]
    NT.hash_pairs = []
    NT.expb = None
    idmap = {ents[i].name: newname.get(i) for i in range(n)}
    return dict(table=NT, idmap=idmap, dropped=[ents[i] for i in dropped], denied=[ents[i] for i in denied], merged=merged, wild_changed=wild_changed,
                newname=newname)


def mentions_in_tree():
    """ids named by any tracked source (tokens GA_<hex>)"""
    seen = set()
    for f in tracked('src', 'tests', 'tools', 'development'):
        if f in ID_EXCLUDE or not f.endswith(('.h', '.cpp', '.inl', '.py', '.json', '.ps1')):
            continue
        for m in ID_RE.finditer(rd(f)):
            seen.add('GA_' + m.group(1))
    return seen


# ---------------------------------------------------------------- source rewrites
class Record:
    def __init__(self):
        self.rows = []

    def add(self, kind, f, line, old, new):
        self.rows.append((kind, f, line, old, new))


def rewrite_ids(idmap, rec, files, apply):
    unknown = collections.Counter()
    n = 0
    for f in files:
        if f in ID_EXCLUDE:
            continue
        t = rd(f)
        lines = t.split('\n')
        changed = False
        for li, line in enumerate(lines):
            if 'GA_' not in line:
                continue

            def sub(m):
                nonlocal n
                name = 'GA_' + m.group(1)
                if name in idmap:
                    new = idmap[name]
                    if new is None:
                        unknown['dropped:' + name] += 1
                        return m.group(0)
                    if new != name:
                        rec.add('id', f, li + 1, name, new)
                        n += 1
                    return new
                unknown[name] += 1
                return m.group(0)
            nl = ID_RE.sub(sub, line)
            if nl != line:
                lines[li] = nl
                changed = True
        if changed and apply:
            wr(f, '\n'.join(lines))
    return n, unknown


def rewrite_hashes(pairs, rec, files, apply):
    pm = dict(pairs)
    n = 0
    for f in files:
        if f in ID_EXCLUDE or f.startswith('tools/addr_') or not f.endswith(('.h', '.cpp', '.inl')):
            continue
        t = rd(f)
        lines = t.split('\n')
        changed = False
        for li, line in enumerate(lines):
            if 'GH(' not in line:
                continue

            def sub(m):
                nonlocal n
                digits = m.group(1)
                v = int(digits, 16)
                if v not in pm or pm[v] == v:
                    return m.group(0)
                nv = pm[v]
                fmt = '%0*x' if (digits.lower() == digits and any(c in 'abcdef' for c in digits)) else '%0*X'      # keep the case of the literal
                txt = ('GH(0x' + fmt % (len(digits), nv) + '%s)') % m.group(2)
                rec.add('hash', f, li + 1, '0x%s' % digits, '0x' + fmt % (len(digits), nv))
                n += 1
                return txt
            nl = GH_RE.sub(sub, line)
            if nl != line:
                lines[li] = nl
                changed = True
        if changed and apply:
            wr(f, '\n'.join(lines))
    return n


INTERIOR_MAX = 0x30
INTERIOR_SKIP = {'tests/personnel_native_fixture.cpp', 'tests/merit_combat_fixture.inl'}   # synthetic page placement / ambiguous distance: by hand


def interior_map(T, frm, to):
    """sorted [(old rva, new rva)] of the code ids with a value in both builds, for literals that are an id plus a small offset."""
    lst = sorted((e.vals[frm], e.vals[to]) for e in T.entries if e.kind in (0, 1) and e.vals[frm] and e.vals[to])
    return lst, {e.vals[to] for e in T.entries if e.vals[to]}     # values that are already RVAs of the new build are never converted again


def rewrite_literals(rvamap, rec, files, apply, suffixed_only=False, interior=None):
    n = 0
    for f in files:
        if f in LIT_SKIP:
            continue
        t = rd(f)
        lines = t.split('\n')
        changed = False
        if f.endswith('.py'):
            spans = [(i, 0, len(l)) for i, l in enumerate(lines)]
        else:
            spans = list(code_spans(lines))
        edits = collections.defaultdict(list)
        for li, a, b in spans:
            seg = lines[li][a:b]
            for m in HEX_RE.finditer(seg):
                v = int(m.group(1), 16)
                if interior is not None:
                    lst, newset = interior
                    if f in INTERIOR_SKIP or v in rvamap or v in newset or v < 0x10000 or (v & 0xFFFF) == 0:
                        continue
                    k = bisect.bisect_right([x[0] for x in lst], v) - 1
                    if k < 0 or not 0 < v - lst[k][0] <= INTERIOR_MAX:
                        continue
                    rvamap = dict(rvamap)
                    rvamap[v] = lst[k][1] + (v - lst[k][0])
                if v not in rvamap or rvamap[v] == v:
                    continue
                if v < 0x10000 and (f, v) not in SMALL_RVA:
                    continue
                if suffixed_only and not m.group(2):
                    continue
                edits[li].append((a + m.start(), a + m.end(), v, m.group(0)))
        for li, es in edits.items():
            line = lines[li]
            for s, e, v, txt in sorted(es, reverse=True):
                mm = HEX_RE.fullmatch(txt)
                digits, suffix = mm.group(1), mm.group(2)
                nv = ('0x%0*X' % (len(digits), rvamap[v]) if digits.isupper() or digits.isdigit() else '0x%0*x' % (len(digits), rvamap[v])) + suffix
                line = line[:s] + nv + line[e:]
                rec.add('literal', f, li + 1, txt, nv)
                n += 1
            lines[li] = line
            changed = True
        if changed and apply:
            wr(f, '\n'.join(lines))
    return n


def write_record(dirpath, plan, rec, summary, unknown):
    os.makedirs(dirpath, exist_ok=True)
    with open(os.path.join(dirpath, 'idmap.tsv'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('old_id\tnew_id\n')
        for k, v in plan['idmap'].items():
            f.write('%s\t%s\n' % (k, v or '(dropped)'))
    with open(os.path.join(dirpath, 'dropped.tsv'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('old_id\told_rva\tkind\tmethod\treason\n')
        for e in plan['dropped']:
            f.write('%s\t%X\t%s\t%s\tvalue 0 in the target build, no source names it, no other id uses it as container\n' % (e.name, e.rva, G.KIND[e.kind], G.METHOD[e.method]))
        for e in plan['denied']:
            f.write('%s\t%X\t%s\t%s\tkept as %s (value 0 = denied)\n' % (e.name, e.rva, G.KIND[e.kind], G.METHOD[e.method], plan['newname'].get(plan['idmap_index'][e.name], '?')))
    with open(os.path.join(dirpath, 'changes.tsv'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('kind\tfile\tline\told\tnew\n')
        for r in rec.rows:
            f.write('%s\t%s\t%d\t%s\t%s\n' % r)
    with open(os.path.join(dirpath, 'unknown.tsv'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('token\tcount\n')
        for k, v in sorted(unknown.items()):
            f.write('%s\t%d\n' % (k, v))
    json.dump(summary, open(os.path.join(dirpath, 'summary.json'), 'w', encoding='utf-8', newline='\n'), indent=1, sort_keys=True)


def cmd_plan_apply(a, apply):
    if apply and a.table:
        raise SystemExit('apply rewrites src/game_addresses_data.inl: put the candidate there first (tools/patch_next.py promote)')
    T = G.parse(a.table or INL)
    img = Image(a.exe) if a.exe else None
    if img:
        tb = [b for b in T.builds if b[0] == a.to][0]
        if img.timestamp != tb[1]:
            raise SystemExit('--exe TimeDateStamp %08X is not build %s (%08X)' % (img.timestamp, a.to, tb[1]))
    ment = mentions_in_tree()
    plan = build_plan(T, a.frm, a.to, img, ment)
    plan['idmap_index'] = {e.name: i for i, e in enumerate(T.entries)}
    idmap = plan['idmap']
    old_rva = {e.name: e.rva for e in T.entries}
    new_rva = {e.name: e.vals[a.to] for e in T.entries}
    rvamap = {old_rva[k]: new_rva[k] for k in idmap if new_rva[k]}
    rec = Record()
    files = rewritable('src', 'tests', 'tools', 'development')
    nid, unknown = rewrite_ids(idmap, rec, files, apply)
    nh = rewrite_hashes(T.hash_pairs, rec, files, apply)
    lit_files = [f for f in rewritable('tests') if f.endswith(LIT_EXT)]
    lit_files += [f for f in (a.py or []) if f not in lit_files]
    nl = rewrite_literals(rvamap, rec, lit_files, apply)
    summary = dict(frm=a.frm, to=a.to, ids_before=len(T.entries), ids_after=len(plan['table'].entries), dropped=len(plan['dropped']),
                   denied_kept=len(plan['denied']), merged_groups=len(plan['merged']), wild_changed=plan['wild_changed'],
                   id_token_rewrites=nid, hash_rewrites=nh, literal_rewrites=nl, hash_pairs=len(T.hash_pairs),
                   unknown_tokens=len(unknown), exe_timestamp='%08X' % (img.timestamp if img else 0))
    print(json.dumps(summary, indent=1))
    if plan['dropped']:
        print('dropped (value 0, unreferenced): ' + ' '.join(e.name for e in plan['dropped']))
    if plan['denied']:
        print('denied kept: ' + ' '.join(e.name for e in plan['denied']))
    if apply:
        hdr = ['// Generated by tools/addr_promote.py from the previous table (reference build %s -> %s). Do not edit by hand.' % (a.frm, a.to),
               '// id GA_<HEX> = RVA of the reference build %s (GA_X<HEX> = denied id, value 0); value = dense index. Single build column ga_val_%s[id].' % (a.to, a.to)]
        wr('src/game_addresses_data.inl', G.emit(plan['table'], hdr))
        rd_dir = os.path.join(ROOT, 'development', 'addr_promote', '%s_to_%s' % (a.frm, a.to))
        write_record(rd_dir, plan, rec, summary, unknown)
        print('record:', rd_dir)
    return 0


# ---------------------------------------------------------------- guard literals
class ByteLists:
    """every brace-enclosed list of integer literals (byte arrays) of the sources."""
    INT = re.compile(r'\s*(0[xX][0-9A-Fa-f]+|\d+)[uUlL]*\s*')

    def __init__(self, files):
        self.lists = []                 # (file, [(start, end, value, text)])
        self.text = {}
        for f in files:
            t = rd(f)
            self.text[f] = t
            blank = self.strip(t)
            for m in re.finditer(r'\{([^{}]*)\}', blank):
                body = m.group(1)
                toks = []
                pos = m.start(1)
                ok = True
                for part in body.split(','):
                    if not part.strip():
                        pos += len(part) + 1
                        continue
                    mm = self.INT.fullmatch(part)
                    if not mm:
                        ok = False
                        break
                    s = pos + part.index(mm.group(1))
                    toks.append((s, s + len(mm.group(1)), int(mm.group(1), 0), mm.group(1)))
                    pos += len(part) + 1
                if ok and toks and all(0 <= x[2] <= 255 for x in toks):
                    self.lists.append((f, toks))
        self.index = collections.defaultdict(list)
        for li, (f, toks) in enumerate(self.lists):
            for k, tk in enumerate(toks):
                self.index[tk[2]].append((li, k))

    @staticmethod
    def strip(t):
        out = list(t)
        i, n = 0, len(t)
        while i < n:
            c = t[i]
            if c == '/' and i + 1 < n and t[i + 1] == '/':
                while i < n and t[i] != '\n':
                    out[i] = ' '
                    i += 1
            elif c == '/' and i + 1 < n and t[i + 1] == '*':
                j = t.find('*/', i + 2)
                j = n if j < 0 else j + 2
                for k in range(i, j):
                    if t[k] != '\n':
                        out[k] = ' '
                i = j
            elif c == '"' or c == "'":
                q = c
                j = i + 1
                while j < n and t[j] != q:
                    j += 2 if t[j] == '\\' else 1
                for k in range(i + 1, min(j, n)):
                    if t[k] != '\n':
                        out[k] = ' '
                i = j + 1
            else:
                i += 1
        return ''.join(out)

    def find(self, run):
        hits = []
        for li, k in self.index.get(run[0], ()):
            f, toks = self.lists[li]
            if k + len(run) <= len(toks) and all(toks[k + j][2] == run[j] for j in range(len(run))):
                hits.append((li, k))
        return hits


def parse_dump(dirpath):
    recs = {}
    for v in ('convenience', 'combat'):
        p = os.path.join(dirpath, v + '.txt')
        if not os.path.exists(p):
            continue
        for ln in open(p, encoding='utf-8'):
            ps = ln.split()
            if len(ps) == 5 and ps[0] in ('M', 'A'):
                recs[(int(ps[1], 16), int(ps[2]), ps[3])] = ps[4]
    return recs


def static_mcmp_sites(files):
    """ga::mcmp(image,GA_x,off,ARR,n) with plain identifier array and literal / sizeof count -> (file, line, id, off, arr, n)"""
    out = []
    for f in files:
        if not f.startswith('src/') or not f.endswith(('.cpp', '.inl', '.h')):
            continue
        t = rd(f)
        for m in re.finditer(r'ga::mcmp\(\s*image\s*,\s*(GA_\w+)\s*,\s*([^,()]+?)\s*,\s*(\w+)\s*,\s*(sizeof\(\s*\w+\s*\)|\d+|0x[0-9A-Fa-f]+)\s*\)', t):
            out.append((f, t.count('\n', 0, m.start()) + 1, m.group(1), m.group(2), m.group(3), m.group(4)))
    return out


def cmd_guards(a):
    img = Image(a.exe)
    files = [f for f in rewritable('src') if f.endswith(('.h', '.cpp', '.inl')) and not f.startswith('src/game_addresses')]
    BL = ByteLists(files)
    todo = []                                            # (expected run, image bytes, source)
    if a.dump:
        for (rva, n, hx), res in parse_dump(a.dump).items():
            exp = bytes.fromhex(hx)
            im = img.read(rva, n)
            if im is not None and im != exp:
                todo.append((exp, im, 'dump M/A %X+%d' % (rva, n)))
    t = G.parse(INL)
    rv = {e.name: e.vals[t.builds[0][0]] for e in t.entries}
    unresolved_static = []
    for f, line, gid, off, arr, cnt in static_mcmp_sites(files):
        base = rv.get(gid)
        if not base:
            continue
        try:
            o = int(off, 0)
        except ValueError:
            unresolved_static.append((f, line, 'offset ' + off))
            continue
        mm = re.match(r'sizeof\(\s*(\w+)\s*\)', cnt)
        # the array definition: the same file first, then any other source
        found = None
        for f2 in [f] + [x for x in BL.text if x != f]:
            m2 = re.search(r'\b%s\s*\[[^\]]*\]\s*=\s*\{([^{}]*)\}' % re.escape(arr), BL.strip(BL.text[f2]))
            if m2:
                found = (f2, m2)
                break
        if not found:
            unresolved_static.append((f, line, 'array ' + arr))
            continue
        f2, m2 = found
        try:
            vals = [int(x.strip(), 0) for x in re.split(r',', m2.group(1)) if x.strip()]
        except ValueError:
            unresolved_static.append((f, line, 'array %s is not a plain byte list' % arr))
            continue
        n = len(vals) if mm else int(cnt, 0)
        exp = bytes(vals[:n])
        im = img.read(base + o, n)
        if im is not None and im != exp:
            todo.append((exp, im, 'static %s:%d %s+%d %s' % (f, line, gid, o, arr)))
    # group by expected run
    byrun = collections.OrderedDict()
    for exp, im, src in todo:
        byrun.setdefault(exp, []).append((im, src))
    edits, conflicts, nohit, multi = [], [], [], []
    for exp, lst in byrun.items():
        news = {x[0] for x in lst}
        if len(news) > 1:
            conflicts.append((exp.hex(), [(x[0].hex(), x[1]) for x in lst]))
            continue
        hits = BL.find(list(exp))
        if not hits:
            nohit.append((exp.hex(), lst[0][1]))
            continue
        if len(hits) > 1:
            multi.append((exp.hex(), len(hits), [BL.lists[h[0]][0] for h in hits][:6], lst[0][1]))
        new = lst[0][0]
        for li, k in hits:
            edits.append((li, k, new, lst[0][1]))
    print('guard literals: %d stale expectation(s), %d source run(s) to rewrite, %d conflict(s), %d without a source list, %d with several matches, %d static sites not resolvable' % (
        len(todo), len(edits), len(conflicts), len(nohit), len(multi), len(unresolved_static)))
    for c in conflicts:
        print('CONFLICT', c)
    for c in nohit:
        print('NOHIT', c)
    for c in multi:
        print('MULTI', c)
    for c in unresolved_static:
        print('STATIC-UNRESOLVED', c)
    a.stale, a.rewrote = len(todo), 0
    if not a.apply:
        return 1 if (conflicts or nohit) else 0
    # apply: token replacement per file, back to front
    perfile = collections.defaultdict(list)
    seen = set()
    for li, k, new, src in edits:
        f, toks = BL.lists[li]
        for j in range(len(new)):
            key = (f, li, k + j)
            if key in seen:
                continue
            seen.add(key)
            s, e, v, txt = toks[k + j]
            if v == new[j]:
                continue
            nt = ('0x%02X' % new[j]) if txt.lower().startswith('0x') else str(new[j])
            perfile[f].append((s, e, nt, txt, src))
    rec = Record()
    for f, es in perfile.items():
        t = BL.text[f]
        for s, e, nt, txt, src in sorted(es, reverse=True):
            line = t.count('\n', 0, s) + 1
            t = t[:s] + nt + t[e:]
            rec.add('guard-byte', f, line, txt, nt)
        wr(f, t)
    rd_dir = os.path.join(ROOT, 'development', 'addr_promote', a.record or 'guards')
    os.makedirs(rd_dir, exist_ok=True)
    mode = 'a' if os.path.exists(os.path.join(rd_dir, 'guard_bytes.tsv')) else 'w'
    with open(os.path.join(rd_dir, 'guard_bytes.tsv'), mode, encoding='utf-8', newline='\n') as fo:
        if mode == 'w':
            fo.write('kind\tfile\tline\told\tnew\n')
        for r in rec.rows:
            fo.write('%s\t%s\t%d\t%s\t%s\n' % r)
    print('rewrote %d byte tokens in %d files' % (len(rec.rows), len(perfile)))
    a.rewrote = len(rec.rows)
    return 1 if (conflicts or nohit) else 0


def cmd_converge(a):
    """guards until a dump run shows no stale expectation (each pass can unmask guards that an earlier failing guard short-circuited)."""
    rd_dir = os.path.join(ROOT, 'development', 'addr_promote', a.record or 'guards')
    gb = os.path.join(rd_dir, 'guard_bytes.tsv')
    if os.path.exists(gb):
        os.remove(gb)
    ps = os.path.join(HERE, 'build_ga_equiv_dump.ps1')
    for i in range(a.max):
        out = os.path.join(ROOT, 'build', 'development', 'ga_equiv', 'promote_%d' % i)
        r = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', ps, '-Out', out, '-Exe', a.exe, '-Force'],
                           capture_output=True, text=True, encoding='utf-8', errors='replace')
        if r.returncode:
            print(r.stdout[-3000:], r.stderr[-3000:])
            return 1
        ns = argparse.Namespace(exe=a.exe, dump=out, apply=True, record=a.record)
        rc = cmd_guards(ns)
        print('pass %d: %d stale expectation(s), %d byte token(s) rewritten' % (i, ns.stale, ns.rewrote))
        if rc:
            return rc
        if not ns.stale:
            print('converged; dump of the last pass: %s' % out)
            return 0
    print('no convergence after %d passes' % a.max)
    return 1


def cmd_sweep(a):
    """Byte lists of the sources that match no code of the --exe image (stale guard literals) but do occur once in the --from-exe image:
    mapped through the previous table (--old-table, two build columns) to the new location and rewritten to the new bytes. Independent of
    which guards a fixture runs (install-time prepare guards, tables behind a feature that no fixture enables)."""
    new, old = Image(a.exe), Image(a.from_exe)
    OT = G.parse(a.old_table)
    names = [b[0] for b in OT.builds]
    ci, ni = 0, len(names) - 1
    ids = sorted((e.vals[names[ci]], e.vals[names[ni]]) for e in OT.entries if e.kind in (0, 1) and e.vals[names[ci]] and e.vals[names[ni]])
    keys = [i[0] for i in ids]

    def text_of(img):
        for nm, va, vs, rs, ro in img.secs:
            if nm == '.text':
                return va, img.d[ro:ro + rs]

    nva, ntext = text_of(new)
    ova, otext = text_of(old)
    files = [f for f in rewritable('src') if f.endswith(('.h', '.cpp', '.inl')) and not f.startswith('src/game_addresses')]
    BL = ByteLists(files)
    stale, ambiguous, unmatched = [], [], 0
    seen = set()
    for li, (f, toks) in enumerate(BL.lists):
        vals = bytes(t[2] for t in toks)
        if len(vals) < 5 or vals in seen:
            continue
        seen.add(vals)
        if ntext.find(vals) >= 0:
            continue
        found = None
        for L in sorted({min(len(vals), 64), 48, 32, 24, 20, 16, 15, 14, 12, 11, 10, 9, 8, 7, 6, 5}, reverse=True):
            if L > len(vals):
                continue
            pat = vals[:L]
            p = otext.find(pat)
            if p < 0:
                continue
            if otext.find(pat, p + 1) >= 0:
                found = ('ambiguous', L)
                break
            found = ('ok', L, ova + p)
            break
        if not found:
            unmatched += 1
            continue
        if found[0] == 'ambiguous':
            ambiguous.append((f, vals[:found[1]].hex()))
            continue
        L, r_old = found[1], found[2]
        k = bisect.bisect_right(keys, r_old) - 1
        if k < 0 or r_old - ids[k][0] > 0x2000:
            ambiguous.append((f, 'no id near %X' % r_old))
            continue
        r_new = ids[k][1] + (r_old - ids[k][0])
        span = len(vals) if otext[r_old - ova:r_old - ova + len(vals)] == vals else L      # verbatim copy of old code: all bytes, else the matched prefix
        nb = ntext[r_new - nva:r_new - nva + span]
        stale.append((f, li, vals, nb, r_old, r_new, L))
    print('byte lists (>=5) matching no new code: %d stale (found once in the old image), %d ambiguous, %d unmatched' % (len(stale), len(ambiguous), unmatched))
    for s in stale:
        f, li, vals, nb, ro, rn, L = s
        print('%s %s old %X -> new %X len %d(%d): %s -> %s' % ('STALE' if len(nb) == len(vals) else 'PARTIAL(not a verbatim copy, left alone)', f, ro, rn, len(vals), L, vals[:20].hex(), nb[:20].hex()))
    stale = [s for s in stale if len(s[3]) == len(s[2]) and s[0] not in SWEEP_SKIP_FILES]
    for x in ambiguous[:30]:
        print('AMBIGUOUS', x)
    if not a.apply:
        return 0
    perfile = collections.defaultdict(list)
    for f, li, vals, nb, ro, rn, L in stale:
        toks = BL.lists[li][1]
        for j in range(min(len(vals), len(nb))):
            if vals[j] == nb[j]:
                continue
            s_, e_, v_, txt = toks[j]
            perfile[BL.lists[li][0]].append((s_, e_, ('0x%02X' % nb[j]) if txt.lower().startswith('0x') else str(nb[j]), txt))
    rec = Record()
    for f, es in perfile.items():
        t = BL.text[f]
        for s_, e_, nt, txt in sorted(set(es), reverse=True):
            rec.add('sweep-byte', f, t.count('\n', 0, s_) + 1, txt, nt)
            t = t[:s_] + nt + t[e_:]
        wr(f, t)
    rd_dir = os.path.join(ROOT, 'development', 'addr_promote', a.record or 'guards')
    os.makedirs(rd_dir, exist_ok=True)
    with open(os.path.join(rd_dir, 'sweep_bytes.tsv'), 'w', encoding='utf-8', newline='\n') as fo:
        fo.write('kind\tfile\tline\told\tnew\n')
        for r in rec.rows:
            fo.write('%s\t%s\t%d\t%s\t%s\n' % r)
    print('rewrote %d byte tokens in %d files' % (len(rec.rows), len(perfile)))
    return 0


def cmd_check(a):
    img = Image(a.exe)
    T = G.parse(INL)
    bad = 0
    nm = T.builds[0][0]
    if len(T.builds) != 1:
        print('FAIL table carries %d build columns' % len(T.builds))
        bad += 1
    if img.timestamp != T.builds[0][1]:
        print('FAIL exe timestamp %08X != table %08X' % (img.timestamp, T.builds[0][1]))
        bad += 1
    # hash literals of the sources against the image: rows {GA(GA_x),size,GH(h)}
    rv = {e.name: e.vals[nm] for e in T.entries}
    rows = 0
    mism = []
    for f in tracked('src'):
        if not f.endswith(('.h', '.cpp', '.inl')):
            continue
        for m in re.finditer(r'\{\s*GA\((GA_\w+)\)\s*,\s*(0[xX][0-9A-Fa-f]+|\d+)\s*,\s*GH\(\s*0[xX]([0-9A-Fa-f]+)[uU][lL]{2}\s*\)\s*\}', rd(f)):
            rows += 1
            base = rv.get(m.group(1))
            n = int(m.group(2), 0)
            h = int(m.group(3), 16)
            b = img.read(base, n) if base else None
            if b is None or fnv(b) != h:
                mism.append((f, m.group(1), n, '%X' % h, '%X' % fnv(b) if b else '-'))
    print('GH profile rows: %d, differ from FNV of the image: %d' % (rows, len(mism)))
    for x in mism[:40]:
        print('  ', x)
    if a.dump:
        recs = parse_dump(a.dump)
        stale = [(r, n) for (r, n, hx), res in recs.items() if img.read(r, n) != bytes.fromhex(hx)]
        print('guard dump lines: %d, expected bytes differing from the image: %d' % (len(recs), len(stale)))
        hs = 0
        hm = []
        for v in ('convenience', 'combat'):
            p = os.path.join(a.dump, v + '.txt')
            if not os.path.exists(p):
                continue
            for ln in open(p, encoding='utf-8'):
                ps = ln.split()
                if len(ps) == 4 and ps[0] == 'H':
                    hs += 1
                    r, n, h = int(ps[1], 16), int(ps[2]), int(ps[3], 16)
                    b = img.read(r, n)
                    if b is None or fnv(b) != h:
                        hm.append((v, '%X' % r, n, '%X' % h))
        print('hash dump lines: %d, differing from FNV of the image: %d' % (hs, len(hm)))
        for x in hm[:20]:
            print('  ', x)
        bad += len(stale)
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    for name in ('plan', 'apply'):
        p = sub.add_parser(name)
        p.add_argument('--from', dest='frm', required=True)
        p.add_argument('--to', required=True)
        p.add_argument('--exe', required=True)
        p.add_argument('--py', nargs='*', help='extra python verifier files whose hex literals are converted')
        p.add_argument('--table', help='two-column candidate table instead of src/game_addresses_data.inl (plan only: tools/patch_next.py promote)')
    p = sub.add_parser('guards')
    p.add_argument('--exe', required=True)
    p.add_argument('--dump')
    p.add_argument('--apply', action='store_true')
    p.add_argument('--record')
    p = sub.add_parser('literals')
    p.add_argument('--old-table', required=True, help='previous data file with both build columns')
    p.add_argument('--record')
    p.add_argument('--interior', action='store_true', help='literals that are a code id plus up to 0x30 (second pass; run after the exact pass)')
    p.add_argument('--suffixed-only', action='store_true', help='only literals with a u/l suffix (the first pass of an older tool version missed them)')
    p.add_argument('--apply', action='store_true')
    p = sub.add_parser('converge')
    p.add_argument('--exe', required=True)
    p.add_argument('--record')
    p.add_argument('--max', type=int, default=8)
    p = sub.add_parser('sweep')
    p.add_argument('--exe', required=True, help='unpacked EXE of the new reference build')
    p.add_argument('--from-exe', required=True, help='unpacked EXE of the previous reference build')
    p.add_argument('--old-table', required=True, help='previous data file with both build columns (git show <rev>:src/game_addresses_data.inl)')
    p.add_argument('--apply', action='store_true')
    p.add_argument('--record')
    p = sub.add_parser('check')
    p.add_argument('--exe', required=True)
    p.add_argument('--dump')
    a = ap.parse_args()
    if a.cmd in ('plan', 'apply'):
        return cmd_plan_apply(a, a.cmd == 'apply')
    if a.cmd == 'guards':
        return cmd_guards(a)
    if a.cmd == 'sweep':
        return cmd_sweep(a)
    if a.cmd == 'converge':
        return cmd_converge(a)
    if a.cmd == 'literals':
        OT = G.parse(a.old_table)
        names = [b[0] for b in OT.builds]
        rvamap = {e.vals[names[0]]: e.vals[names[-1]] for e in OT.entries if e.vals[names[-1]]}
        rec = Record()
        lit_files = [f for f in rewritable('tests') if f.endswith(LIT_EXT)]
        n = rewrite_literals(rvamap, rec, lit_files, a.apply, a.suffixed_only, interior_map(OT, names[0], names[-1]) if a.interior else None)
        print('%d literal(s) %s' % (n, 'rewritten' if a.apply else 'would change'))
        if a.apply:
            rd_dir = os.path.join(ROOT, 'development', 'addr_promote', a.record or 'literals')
            with open(os.path.join(rd_dir, 'literals_%s.tsv' % ('interior' if a.interior else 'suffixed') + ''), 'w', encoding='utf-8', newline='\n') as fo:
                fo.write('kind\tfile\tline\told\tnew\n')
                for r in rec.rows:
                    fo.write('%s\t%s\t%d\t%s\t%s\n' % r)
        return 0
    return cmd_check(a)


if __name__ == '__main__':
    sys.exit(main())
