#!/usr/bin/env python
"""Hook map generator and rule checker R1-R14 (check redesign package A, 2026-11-06, docs/HISTORY.md section 4).

    python -X utf8 tools/hook_map.py                      generate build/development/hook_map.json and check R1-R14
    python -X utf8 tools/hook_map.py --no-exe             skip R13 (no unpacked EXE read)
    python -X utf8 tools/hook_map.py --json               machine readable result on stdout
    python -X utf8 tools/hook_map.py --owner X|--feature X  only report findings/sites of one owner (or features.json module)
    python -X utf8 tools/hook_map.py --diff REV           sites added/removed/changed against a git revision (read in memory)
    python -X utf8 tools/hook_map.py --diff REF TARGET [--table candidate.inl]
                                                          new Steam build: the hook map of a two-column candidate table (tools/patch_next.py emit),
                                                          sites unresolved / moved in TARGET and the findings (R12/R13 ...) that exist only there
    python -X utf8 tools/hook_map.py --probe GA_809461 [--kind call] [--len 5]
                                                          who owns / overlaps this address in both builds (before writing a new module)
    python -X utf8 tools/hook_map.py --policy-skeleton    policy entries (empty reason) for every unsuppressed error
    python -X utf8 tools/hook_map.py --root DIR           another tree (the self test uses a synthetic one)

Exit status 1 when an error finding is not covered by development/hook_policy.json. Standard library only.
Writes only below <root>/build/development.

Rules (docs/CHECKS.md has the long form; all static, severity error unless noted):
 R1  vtable/COM slot owned by two owners; split_pointers rows with `next` must match their Layout registry row
 R2  byte ranges of different owners overlap (kind mutex/alias in the policy allows it)
 R3  different owners in one 16-byte block while one of them writes a block snapshot (prepare_link/prepare_bytes/replace_code16)
 R4  block constraints: prepare_link offset<=11, prepare_bytes offset+len<=16, hook_block 5..16, span 5..32, bytes 1..32 (CALL offset>11: info)
 R5  Layout blocked-entry lists agree; no site range contains a blocked entry; no site overlaps a foreign entry fingerprint
 R6  ga_observers displaced prefixes overlap a site (or each other)
 R7  site overlaps a foreign-mod site: needs a relation in foreign_hooks.json, and the Layout flags must fit the relation action
 R8  NEED_WHITELIST rows are registered from convenience/CCZ DLL code only
 R9  every Layout owner has a registering file; every registration has a registry row; no row registered from two files
 R10 the same point written by two DLLs (same source line compiled into two DLLs is info)
 R11 Layout registrations <= kMaxHooks / kMaxHandles (warn from 90 %)
 R12 kind consistency with the kinds of game_addresses_data.inl (vt slot -> slot/data, original -> func, call/jump -> mid/func); ids unresolved in a build (info)
 R13 EXE bytes in both builds: vt qword == base+original, E8/E9 opcode + target, block/span expect bytes
 R14 guards: Layout rows resolve in both builds (warn), direct patches have a guard (baseline in the policy), Present/WndProc only in SAN14_Layout.dll
"""
import os, sys, re, json, time, argparse, collections

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import hookmap_extract as X                     # noqa: E402

IMAGE_BASE = 0x140000000
CODE_KINDS = ('call', 'jump', 'branch', 'block', 'span', 'bytes')
WRITER_DLLS = ('convenience', 'combat', 'ccz', 'layout')
DECIDED = '2026-11-06'


# ------------------------------------------------------------------ data files
def load_policy(tree):
    d = X.read_json(tree, 'development/hook_policy.json', {}) or {}
    return d.get('entries', [])


def load_foreign(tree, ga):
    """foreign sites with per-build RVAs, relations, mods."""
    d = X.read_json(tree, 'development/foreign_hooks.json', {}) or {}
    sites = []
    for s in d.get('sites', []):
        rv = {}
        spec = s.get('rva')
        for b in ga.builds:
            v = None
            if isinstance(spec, dict):
                v = spec.get(b)
            elif isinstance(spec, str):
                gid = 'GA_' + spec.upper().lstrip('0')
                v = ga.rva(gid, b) if gid in ga.index else (int(spec, 16) if b == ga.builds[0] else None)
            elif s.get('ga'):
                v = ga.rva(s['ga'], b)
            rv[b] = int(v, 16) if isinstance(v, str) else v
        key = s.get('key')
        if not key and s.get('kind') == 'com':
            key = 'com:' + s.get('com', '')
        sites.append(dict(s, rvas=rv, key=key))
    return {'mods': d.get('mods', {}), 'sites': sites, 'relations': d.get('relations', [])}


def load_addr_kinds(tree):
    """kind of every table id (func / mid / slot / data) from the data file itself (single build column since the W1-E re-base)."""
    raw = tree.read('src/game_addresses_data.inl')
    if not raw:
        return None
    text = raw.decode('utf-8')
    m = re.search(r'enum\s+GaId[^{]*\{(.*?)\}\s*;', text, re.S)
    rows = re.search(r'ga_entries\[GA_COUNT\]\s*=\s*\{(.*?)\n?\};', text, re.S)
    if not m or not rows:
        return None
    names = [n for n in re.findall(r'(GA_\w+?)\s*=\s*\d+', m.group(1)) if n != 'GA_COUNT']
    kinds = re.findall(r'\{0[xX][0-9A-Fa-f]+,(\d+),\d+,', rows.group(1))
    if len(kinds) != len(names):
        return None
    return {n: {'kind': ('func', 'mid', 'slot', 'data')[int(k)]} for n, k in zip(names, kinds)}


class ExeReader:
    """read-only access to the unpacked EXEs of the builds (R13), by build name (sha8) through tools/pelib.py."""

    def __init__(self):
        import pelib
        self.P = pelib
        self._b = {}

    def raw(self, build, rva, n):
        try:
            if build not in self._b:
                self._b[build] = self.P.open_build(build)
            return bytes(self._b[build].raw(rva, n))
        except (SystemExit, OSError):          # no unpacked image of this build on this machine
            return None


# ------------------------------------------------------------------ helpers
def rng(site, b):
    r = site['rva'].get(b)
    if r is None or not site['len']:
        return None
    return r, r + site['len']


def overlap(a, b):
    return a and b and a[0] < b[1] and b[0] < a[1]


def cross_dll_shared(a, b):
    """a shared-source line (compiled into both gameplay DLLs) only competes inside its own DLL copy."""
    return a['dll'] != b['dll'] and bool(a.get('shared') or b.get('shared'))


def is_active(s):
    return s['dll'] != 'unreached'


def code_site(s):
    return s['kind'] in CODE_KINDS


class Findings:
    def __init__(self):
        self.items = []
        self.seen = set()

    def add(self, rule, sev, msg, keys=(), owners=(), sites=(), build=None, **kw):
        dk = (rule, sev, build, tuple(sorted(set(k for k in keys if k))), tuple(sorted(set(o for o in owners if o))), re.sub(r'\b(convenience|combat|ccz|layout)\b', 'dll', msg))
        if dk in self.seen:
            return
        self.seen.add(dk)
        self.items.append(dict(rule=rule, severity=sev, msg=msg, keys=sorted(set(k for k in keys if k)), owners=sorted(set(o for o in owners if o)),
                               sites=list(sites), build=build, **kw))


def rows_by_ga(mp):
    return {r['ga']: r for r in mp.reg['rows']}


# ------------------------------------------------------------------ rules
def run_rules(mp, foreign, policy_unused, reader, addr_kinds, nexe):
    F = Findings()
    builds = mp.ga.builds
    act = [s for s in mp.sites if is_active(s)]
    owners_of = lambda ss: [s['owner'] for s in ss]
    rowmap = rows_by_ga(mp)
    notes = {'unreached': len(mp.sites) - len(act), 'r12_compared': 0, 'r13_verified': 0}

    # ---- R1
    for b in builds:
        groups = collections.defaultdict(list)
        for s in act:
            if s['kind'] in ('vt', 'com', 'wndproc'):
                k = s['key'] if s['kind'] in ('com', 'wndproc') else s['rva'].get(b)
                if k is not None:
                    groups[(s['kind'] if s['kind'] != 'vt' else 'vt', k)].append(s)
        for (kind, k), ss in groups.items():
            bad = [(x, y) for i, x in enumerate(ss) for y in ss[i + 1:] if x['owner'] != y['owner'] and not cross_dll_shared(x, y)]
            if bad:
                ow = {z['owner'] for pr in bad for z in pr}
                F.add('R1', 'error', '%s %s owned by %s' % (kind, k if kind != 'vt' else '%X' % k, sorted(ow)), [z['key'] for pr in bad for z in pr],
                      ow, [z['id'] for pr in bad for z in pr], b)
    for r in mp.registrations:
        if r.get('via') == 'split_pointers':
            row = rowmap.get(r['ga'])
            if not row:
                F.add('R1', 'error', 'split_pointers row %s has a next pointer but no Layout registry row' % r['ga'], [r['ga']], [],
                      ['module_pointers.inl:%d' % r['line']])
            elif r.get('orig') and r['orig'] != row['orig']:
                F.add('R1', 'error', 'split_pointers row %s original %s differs from registry original %s' % (r['ga'], r['orig'], row['orig']),
                      [r['ga']], [row['owner']], ['module_pointers.inl:%d' % r['line']])

    # ---- R2 / R3 / R10 (pairs)
    pair_seen = set()
    for b in builds:
        items = []
        for s in act:
            if s['kind'] in ('com', 'wndproc', 'winhook'):
                continue
            r = rng(s, b)
            if r:
                items.append((r[0], r[1], s))
        items.sort(key=lambda t: (t[0], t[1]))
        live = []
        for st, en, s in items:
            live = [t for t in live if t[1] > st]
            for st2, en2, s2 in live:
                if s2['owner'] == s['owner'] and s2['dll'] == s['dll']:
                    continue
                if cross_dll_shared(s, s2):
                    continue                                    # shared-source copy in the other DLL: judged inside its own DLL
                if s['kind'] == 'vt' and s2['kind'] == 'vt' and st == st2 and en == en2:
                    continue                                                     # R1
                if s['owner'] == s2['owner']:
                    pass
                pk = (b, s['id'], s2['id'])
                if pk in pair_seen:
                    continue
                pair_seen.add(pk)
                keys = [s['key'], s2['key']]
                if s['owner'] != s2['owner']:
                    F.add('R2', 'error', 'ranges overlap at %X: %s (%s %s, %s) and %s (%s %s, %s)' % (max(st, st2), s['owner'], s['kind'], s['key'], s['dll'],
                          s2['owner'], s2['kind'], s2['key'], s2['dll']), keys, [s['owner'], s2['owner']], [s['id'], s2['id']], b)
                if s['dll'] != s2['dll'] and s['key'] == s2['key']:
                    same_line = s['src'] == s2['src']
                    F.add('R10', 'info' if same_line and s['owner'] == s2['owner'] else 'error',
                          'same point %s written by %s (%s) and %s (%s)' % (s['key'], s['dll'], s['src'], s2['dll'], s2['src']),
                          keys, [s['owner'], s2['owner']], [s['id'], s2['id']], b)
            live.append((st, en, s, ) if False else (st, en, s))
        # R3: one 16-byte block, different owners, one of them writes a snapshot of the block
        blocks = collections.defaultdict(list)
        for st, en, s in items:
            if s['kind'] in ('vt',):
                continue
            blocks[st >> 4].append((st, en, s))
            if (en - 1) >> 4 != st >> 4 and s['kind'] in ('block', 'span', 'bytes'):
                blocks[(en - 1) >> 4].append((st, en, s))
        for blk, ls in blocks.items():
            for i in range(len(ls)):
                for j in range(i + 1, len(ls)):
                    (a0, a1, sa), (b0, b1, sb) = ls[i], ls[j]
                    if sa['owner'] == sb['owner'] or not (sa.get('snap') or sb.get('snap')):
                        continue
                    if a0 < b1 and b0 < a1:
                        continue                                                 # already R2
                    pk = (b, 'R3', sa['id'], sb['id'])
                    if pk in pair_seen:
                        continue
                    pair_seen.add(pk)
                    F.add('R3', 'error', 'block %X holds %s (%s %s, %s) and %s (%s %s, %s); a snapshot writer is involved' % (blk << 4, sa['owner'], sa['kind'],
                          sa['key'], sa.get('method'), sb['owner'], sb['kind'], sb['key'], sb.get('method')), [sa['key'], sb['key']],
                          [sa['owner'], sb['owner']], [sa['id'], sb['id']], b)
    # R10 (identical start, different DLL, no byte overlap seen because ranges are equal handled above; plus compiled-into-both)
    shared = [s for s in act if s.get('shared') and s['via'] == 'direct']
    notes['compiled_in_both_dlls'] = len({(s['src'], s['key']) for s in shared})

    # R2 (info): Layout CALL rows whose original target is an entry that another owner patches directly (no byte overlap; the call lands in that patch)
    for s in act:
        if s['via'] == 'layout' and s['kind'] == 'call' and s.get('original'):
            for o in act:
                if o is not s and o['via'] == 'direct' and o['ga'] == s['original']['ga'] and o['kind'] in ('jump', 'branch', 'bytes') and o['owner'] != s['owner']:
                    F.add('R2', 'info', 'row %s (%s) calls %s, whose entry is patched by %s (%s, %s): no byte overlap, the redirected call passes through that patch' % (
                          s['key'], s['owner'], s['original']['ga'], o['owner'], o['dll'], o['src']), [s['key'], o['key']], [s['owner'], o['owner']], [s['id'], o['id']])

    # ---- R4
    for s in act:
        r0 = s['rva'].get(builds[0])
        for b in builds:
            r = s['rva'].get(b)
            if r is None:
                continue
            off = r & 15
            m = s.get('method')
            if m == 'prepare_link' and off > 11:
                F.add('R4', 'error', 'prepare_link at offset %d of its block (max 11): %s' % (off, s['src']), [s['key']], [s['owner']], [s['id']], b)
            elif m == 'prepare_bytes' and s['len'] and off + s['len'] > 16:
                F.add('R4', 'error', 'prepare_bytes offset %d + len %d > 16: %s' % (off, s['len'], s['src']), [s['key']], [s['owner']], [s['id']], b)
            elif s['via'] == 'layout' and s['kind'] == 'block' and not (5 <= s['len'] <= 16):
                F.add('R4', 'error', 'hook_block len %d outside 5..16: %s' % (s['len'], s['src']), [s['key']], [s['owner']], [s['id']], b)
            elif s['via'] == 'layout' and s['kind'] == 'span' and not (5 <= s['len'] <= 32):
                F.add('R4', 'error', 'hook_span len %d outside 5..32: %s' % (s['len'], s['src']), [s['key']], [s['owner']], [s['id']], b)
            elif s['via'] == 'layout' and s['kind'] == 'bytes' and not (1 <= s['len'] <= 32):
                F.add('R4', 'error', 'hook_bytes len %d outside 1..32: %s' % (s['len'], s['src']), [s['key']], [s['owner']], [s['id']], b)
            elif s['kind'] == 'call' and off > 11:
                F.add('R4', 'info', 'CALL at block offset %d (>11): rel32 crosses the 16-byte block (Layout block_rel_write / raw call write): %s' % (off, s['src']),
                      [s['key']], [s['owner']], [s['id']], b)

    # ---- R5
    rb, cb = set(mp.reg['blocked']), set(mp.core['blocked'])
    if mp.reg['present'] and mp.core['present'] and rb != cb:
        F.add('R5', 'error', 'layout_blocked_entries (registry) and blocked_entries (core) differ: only registry %s, only core %s' % (sorted(rb - cb), sorted(cb - rb)),
              sorted(rb ^ cb), [])
    blocked = sorted(rb | cb)
    for b in builds:
        bl = {g: mp.ga.rva(g, b) for g in blocked}
        for s in act:
            r = rng(s, b)
            if not r or s['kind'] in ('vt',):
                continue
            for g, a in bl.items():
                if a is not None and r[0] <= a < r[1]:
                    F.add('R5', 'error', 'site %s (%s, %s) %s covers blocked entry %s' % (s['key'], s['owner'], s['via'], s['src'], g), [s['key'], g], [s['owner']], [s['id']], b)
        for fs in foreign['sites']:
            if fs.get('kind') != 'entry_fingerprint':
                continue
            fr = fs['rvas'].get(b)
            if fr is None:
                continue
            fr = (fr, fr + fs.get('len', 16))
            for s in act:
                r = rng(s, b)
                if r and s['kind'] != 'vt' and overlap(r, fr):
                    ok = any(rel.get('owner') == s['owner'] and rel.get('action') in ('blocked-entry', 'moved', 'refuse') and rel.get('mod') in fs.get('mods', [])
                             for rel in foreign['relations'])
                    F.add('R5', 'info' if ok else 'error', 'site %s (%s) %s overlaps the entry fingerprint %s kept by %s' % (s['key'], s['owner'], s['src'], fs.get('id'), fs.get('mods')),
                          [s['key'], fs.get('ga') or fs.get('id')], [s['owner']], [s['id']], b)

    # ---- R6
    for b_i, b in enumerate(builds):
        obs = []
        for o in mp.ga.observers:
            r = o['rva'][b_i] if b_i < len(o['rva']) else 0
            if r:
                obs.append((r, r + o['len'], 'obs:%X' % o['rva'][0]))      # label = RVA of the first build column
        for i, o in enumerate(obs):
            for o2 in obs[i + 1:]:
                if overlap(o, o2):
                    F.add('R6', 'error', 'observer prefixes %X and %X overlap' % (o[0], o2[0]), [o[2], o2[2]], [], [], b)
            for s in act:
                r = rng(s, b)
                if r and s['kind'] not in ('vt',) and overlap(r, o):
                    F.add('R6', 'error', 'site %s (%s) %s overlaps the observer prefix %X..%X' % (s['key'], s['owner'], s['src'], o[0], o[1]), [s['key'], o[2]],
                          [s['owner']], [s['id']], b)

    # ---- R7
    for b in builds:
        for fs in foreign['sites']:
            if fs.get('kind') == 'entry_fingerprint':
                continue
            for s in act:
                hit = False
                if fs.get('key') and s['key'] == fs['key'] and s['kind'] in ('com', 'wndproc'):
                    hit = True
                else:
                    fr = fs['rvas'].get(b)
                    r = rng(s, b)
                    if fr is not None and r and overlap(r, (fr, fr + max(fs.get('len', 5), 1))):
                        hit = True
                if not hit:
                    continue
                mods = fs.get('mods') or [fs.get('mod')]
                rels = [r for r in foreign['relations'] if r.get('owner') == s['owner'] and r.get('mod') in mods and
                        (not r.get('sites') or fs.get('id') in r['sites'])]
                rels.sort(key=lambda r: 0 if r.get('sites') else 1)             # a relation bound to this very site wins
                keys = [s['key'], fs.get('ga') or fs.get('id')]
                if not rels:
                    F.add('R7', 'error', 'site %s (%s) overlaps foreign %s [%s] and foreign_hooks.json has no relation' % (s['key'], s['owner'], fs.get('id'), ','.join(m for m in mods if m)),
                          keys, [s['owner']], [s['id']], b)
                    continue
                for rel in rels:
                    act_ = rel.get('action')
                    fl = set(s.get('flags') or [])
                    need = {'chain': 'CHAIN_OK', 'yield': 'YIELD_IF_PEER', 'conditional-yield': 'YIELD_IF_PEER', 'refuse': 'REFUSE_IF_FOREIGN'}.get(act_)
                    problems = []
                    if s['via'] == 'layout' and s['kind'] != 'com':
                        if need and need not in fl:
                            problems.append('action %s needs flag %s (has %s)' % (act_, need, sorted(fl)))
                        if rel.get('requires_flag') and rel['requires_flag'] not in fl:
                            problems.append('relation requires %s' % rel['requires_flag'])
                        if act_ in ('yield', 'conditional-yield'):
                            pr = rel.get('peer_rule')
                            if not pr or (mp.core['peer_owners'] and pr not in mp.core['peer_owners']):
                                problems.append('peer_rule %r not found in peer_rules[]' % pr)
                    if act_ == 'moved':
                        problems.append('relation says the site was moved but it still overlaps')
                    if problems:
                        F.add('R7', 'error', 'site %s (%s) vs foreign %s: %s' % (s['key'], s['owner'], fs.get('id'), '; '.join(problems)), keys, [s['owner']], [s['id']], b)
                    else:
                        F.add('R7', 'info', 'site %s (%s) overlaps foreign %s [%s]: relation %s ok' % (s['key'], s['owner'], fs.get('id'), mods[0], act_), keys, [s['owner']], [s['id']], b)
                    break

    # ---- R8 / R9
    regs_by_ga = collections.defaultdict(list)
    for r in mp.registrations:
        if r.get('key'):
            regs_by_ga[r['key']].append(r)
    loop_owner_files = collections.defaultdict(set)
    for r in mp.registrations:
        for o in r.get('loop_owners', []):
            loop_owner_files[o].add(r['file'])
    layout_sites = [s for s in act if s['via'] == 'layout']
    for s in layout_sites:
        if 'NEED_WHITELIST' in (s.get('flags') or []):
            rs_ = regs_by_ga.get(s['key'], [])
            dl = set(d for r in rs_ for d in r['dlls'])
            if rs_ and not dl <= {'convenience', 'ccz'}:
                F.add('R8', 'error', 'NEED_WHITELIST row %s (%s) is registered from %s code (only convenience/CCZ DLL files may)' % (s['key'], s['owner'], sorted(dl)),
                      [s['key']], [s['owner']], [s['id']])
    hook_owners = None
    fj = X.read_json(mp.tree, 'development/features.json', None)
    if isinstance(fj, dict) and isinstance(fj.get('modules'), (dict, list)):
        mods = fj['modules']
        it = mods.items() if isinstance(mods, dict) else [(m.get('name'), m) for m in mods]
        ho = {}
        it = list(it)
        # Hub files (adapter, split installer, ...) register rows for several modules by design (check redesign 2026-11-06).
        hub_files = set()
        for _n, m in it:
            if isinstance(m, dict) and m.get('hub'):
                hub_files.update(m.get('owns', []) or [])
        for name, m in it:
            if isinstance(m, dict) and m.get('hook_owners'):
                for o in m['hook_owners']:
                    ho[o] = (name, set(m.get('sources', [])) | set(m.get('owns', [])) | hub_files)
        hook_owners = ho or None
    mp.hook_owner_map = hook_owners or {}
    reg_owner_rows = collections.defaultdict(list)
    for s in layout_sites:
        if s['method'] in ('layout_rows', 'layout_patch_row', 'layout_traits9_sites'):
            reg_owner_rows[s['owner']].append(s)
    for owner, rows in reg_owner_rows.items():
        files = set()
        for s in rows:
            fl = {r['file'] for r in regs_by_ga.get(s['key'], [])}
            if not fl:
                fl = {f for f in loop_owner_files.get(owner, [])} | {r['file'] for r in mp.registrations if r.get('owner_lit') == owner}
            files |= fl
            if not fl:
                F.add('R9', 'error', 'registry row %s (%s) has no registering code' % (s['key'], owner), [s['key']], [owner], [s['id']])
            elif len({r['file'] for r in regs_by_ga.get(s['key'], [])}) > 1:
                F.add('R9', 'error', 'registry row %s (%s) is registered from %d files: %s' % (s['key'], owner, len(fl), sorted(fl)), [s['key']], [owner], [s['id']])
        if not files:
            F.add('R9', 'error', 'Layout owner %s has no registering file' % owner, [], [owner])
        if hook_owners and owner in hook_owners:
            mod, srcset = hook_owners[owner]
            bad = sorted(f for f in files if srcset and 'src/' + f not in srcset and f not in srcset)
            if bad:
                F.add('R9', 'error', 'owner %s (module %s) is registered from %s, outside the module sources' % (owner, mod, bad), [], [owner])
    reg_ga = {s['key'] for s in layout_sites}
    for g, rs_ in regs_by_ga.items():
        for r in rs_:
            if r.get('via') in ('layout_client::vt', 'layout_client::call', 'layout_client::jump', 'ROUTE_VT', 'split_pointers') and g not in reg_ga:
                F.add('R9', 'error', 'registration of %s at %s:%d has no registry row (the manager refuses it at run time)' % (g, r['file'], r['line']), [g], [],
                      ['%s:%d' % (r['file'], r['line'])])

    # ---- R11
    nreg = len(layout_sites) + sum(1 for s in act if s['via'] == 'layout_self') + sum(1 for s in act if s['via'] == 'direct' and s['dll'] == 'layout' and s['kind'] != 'wndproc')
    for what, n, cap in (('Layout hook table (kMaxHooks)', nreg, mp.meta.get('max_hooks')),
                         ('client handles (kMaxHandles)', len(layout_sites), mp.meta.get('max_handles'))):
        if cap:
            if n > cap:
                F.add('R11', 'error', '%s: %d registrations exceed capacity %d' % (what, n, cap), [], [])
            elif n >= 0.9 * cap:
                F.add('R11', 'warn', '%s: %d of %d used (>=90%%)' % (what, n, cap), [], [])
    notes['layout_hooks'] = nreg
    notes['layout_hook_capacity'] = mp.meta.get('max_hooks')

    # ---- R12
    if addr_kinds:
        want = {'vt': ('slot', 'data'), 'call': ('mid', 'func'), 'jump': ('mid', 'func'), 'branch': ('mid', 'func'), 'bytes': ('mid', 'func', 'data'),
                'block': ('mid', 'func'), 'span': ('mid', 'func')}
        seen = set()
        for s in act:
            ks = [(s['ga'], s['kind'], want.get(s['kind']))]
            if s.get('original'):
                ks.append((s['original']['ga'], 'original', ('func',)))
            for g, role, ok in ks:
                if not g or not ok or (g, role) in seen:
                    continue
                seen.add((g, role))
                k = (addr_kinds.get(g) or {}).get('kind')
                notes['r12_compared'] += 1 if k else 0
                if k and k not in ok:
                    F.add('R12', 'error', '%s used as %s (%s) but the address table says %s: %s' % (g, role, s['owner'], k, s['src']), [g], [s['owner']], [s['id']])
    for s in act:
        for b in builds:
            if s['ga'] and s['rva'].get(b) is None:
                F.add('R12', 'info', '%s unresolved in build %s (%s %s: that feature is off there)' % (s['ga'], b, s['owner'], s['src']), [s['key']], [s['owner']], [s['id']], b)

    # ---- R13
    if reader and not nexe:
        for b in builds:
            for s in act:
                r = s['rva'].get(b)
                if r is None or s['via'] == 'layout_self':
                    continue
                if s['kind'] == 'vt' and s.get('original'):
                    o = s['original']['rva'].get(b)
                    raw = reader.raw(b, r, 8)
                    notes['r13_verified'] += 1 if raw is not None and any(raw) and o is not None else 0
                    if raw is not None and not any(raw):
                        F.add('R13', 'info', 'slot %X (%s) is zero in the unpacked EXE (filled at run time): not checkable statically' % (r, s['owner']), [s['key']], [s['owner']], [s['id']], b)
                    elif o is not None and raw is not None and int.from_bytes(raw, 'little') != IMAGE_BASE + o:
                        F.add('R13', 'error', 'slot %X (%s) holds %X, expected original %X (%s)' % (r, s['owner'], int.from_bytes(raw, 'little') - IMAGE_BASE, o, s['original']['ga']),
                              [s['key']], [s['owner']], [s['id']], b)
                elif s['kind'] in ('call', 'jump') and s['via'] == 'layout':
                    raw = reader.raw(b, r, 5)
                    o = s['original']['rva'].get(b) if s.get('original') else None
                    if raw is None:
                        continue
                    notes['r13_verified'] += 1
                    op = 0xE8 if s['kind'] == 'call' else 0xE9
                    if raw[0] != op:
                        F.add('R13', 'error', 'site %X (%s): opcode %02X, expected %02X' % (r, s['owner'], raw[0], op), [s['key']], [s['owner']], [s['id']], b)
                    elif o is not None:
                        tgt = (r + 5 + int.from_bytes(raw[1:5], 'little', signed=True)) & 0xFFFFFFFF
                        if tgt != o:
                            F.add('R13', 'error', 'site %X (%s): target %X, expected original %X (%s)' % (r, s['owner'], tgt, o, s['original']['ga']), [s['key']], [s['owner']], [s['id']], b)
                elif s['kind'] == 'call' and s.get('method') == 'call_prepare':
                    raw = reader.raw(b, r, 1)
                    notes['r13_verified'] += 1 if raw is not None else 0
                    if raw is not None and raw[0] != 0xE8:
                        F.add('R13', 'error', 'site %X (%s): not a CALL (%02X)' % (r, s['owner'], raw[0]), [s['key']], [s['owner']], [s['id']], b)
                elif s['kind'] in ('block', 'span') and s.get('expect'):
                    raw = reader.raw(b, r, len(s['expect']))
                    notes['r13_verified'] += 1 if raw is not None else 0
                    if raw is not None and list(raw) != s['expect']:
                        F.add('R13', 'error', 'bytes at %X (%s) differ from expect[]: %s' % (r, s['owner'], raw.hex()), [s['key']], [s['owner']], [s['id']], b)

    # ---- R14
    for s in layout_sites:
        for b in builds:
            if s['rva'].get(b) is None and s['ga']:
                F.add('R14', 'warn', 'Layout row %s (%s) is unresolved in build %s: the registration cannot work there' % (s['key'], s['owner'], b), [s['key']], [s['owner']], [s['id']], b)
            elif s.get('original') and s['original']['rva'].get(b) is None:
                F.add('R14', 'warn', 'original %s of row %s (%s) is unresolved in build %s' % (s['original']['ga'], s['key'], s['owner'], b), [s['key']], [s['owner']], [s['id']], b)
    for s in act:
        if s['via'] == 'direct' and s['guard'] == 'none' and s['kind'] not in ('wndproc', 'winhook'):
            F.add('R14', 'error', 'direct patch %s (%s, %s) has no detectable guard in its module files: %s' % (s['key'], s['owner'], s.get('method'), s['src']),
                  [s['key']], [s['owner']], [s['id']])
        if s['kind'] in ('com', 'wndproc') and s['key'] and s['dll'] != 'layout' and (s['key'].startswith('com:IDXGISwapChain') or s['key'] == 'wndproc:game'):
            F.add('R14', 'error', '%s is reserved for SAN14_Layout.dll (user exception 2026-11-05) but %s (%s) writes it' % (s['key'], s['owner'], s['src']),
                  [s['key']], [s['owner']], [s['id']])
    return F, notes


def apply_policy(F, policy):
    hits = collections.Counter()
    out = []
    for f in F.items:
        f['policy'] = None
        if f['severity'] in ('error', 'warn'):
            for i, p in enumerate(policy):
                rules = p.get('rule')
                rules = rules if isinstance(rules, list) else [rules]
                if '*' not in rules and f['rule'] not in rules:
                    continue
                ps = set(p.get('sites') or [])
                if ps and not (ps & set(f['keys'])):
                    continue
                po = set(p.get('owners') or [])
                if po and not set(f['owners']) <= po:
                    continue
                if not ps and not po:
                    continue
                hits[i] += 1
                f['policy'] = i
                if p.get('downgrade') == 'warn':
                    f['severity'] = 'warn'
                else:
                    f['severity'] = 'suppressed'
                break
        out.append(f)
    stale = [i for i in range(len(policy)) if not hits[i]]
    return out, stale, hits


# ------------------------------------------------------------------ runner
def analyse(root, nexe=True, reader=None, tree=None):
    t0 = time.time()
    tree = tree or X.FsTree(root)
    mp = X.build_map(tree)
    foreign = load_foreign(tree, mp.ga)
    policy = load_policy(tree)
    addr_kinds = load_addr_kinds(tree)
    if reader is None and not nexe:
        try:
            reader = ExeReader()
        except Exception:
            reader = None
    F, notes = run_rules(mp, foreign, None, reader, addr_kinds, nexe)
    for st in mp.sites:                                    # feature = features.json module that lists the owner in hook_owners (empty until package B)
        st['feature'] = mp.hook_owner_map.get(st['owner'], ('',))[0]
    # hook_sites / foreign / policy staleness
    hs = mp.hook_sites
    extra = []
    for key in ('bindings', 'ignore_calls', 'com_sites', 'len_overrides'):
        for e in hs.get(key, []):
            if not e.get('_hit'):
                extra.append({'rule': 'sites', 'severity': 'warn', 'msg': 'hook_sites.json %s entry matched nothing (stale): %s' % (key, json.dumps({k: v for k, v in e.items() if not k.startswith('_')}, ensure_ascii=False)[:160]),
                              'keys': [], 'owners': [], 'sites': [], 'build': None, 'policy': None})
            if key == 'ignore_calls' and not e.get('reason'):
                extra.append({'rule': 'sites', 'severity': 'error', 'msg': 'hook_sites.json ignore_calls entry without a reason', 'keys': [], 'owners': [], 'sites': [], 'build': None, 'policy': None})
    for u in mp.unresolved:
        extra.append({'rule': 'sites', 'severity': 'warn', 'msg': 'address argument not resolvable (add a hook_sites.json binding): %s:%d %s(%s)' % (u['file'], u['line'], u['call'], u['arg']),
                      'keys': [], 'owners': [], 'sites': [], 'build': None, 'policy': None})
    for p in policy:
        for fld in ('reason', 'decided', 'evidence'):
            if not p.get(fld):
                extra.append({'rule': 'policy', 'severity': 'error', 'msg': 'policy entry %s lacks %s' % (p.get('rule'), fld), 'keys': [], 'owners': [], 'sites': [], 'build': None, 'policy': None})
    items, stale, hits = apply_policy(F, policy)
    for i in stale:
        p = policy[i]
        extra.append({'rule': 'policy', 'severity': 'warn', 'msg': 'stale policy entry (matches no finding): rule %s sites %s owners %s' % (p.get('rule'), p.get('sites'), p.get('owners')),
                      'keys': [], 'owners': [], 'sites': [], 'build': None, 'policy': None})
    items = items + extra
    res = {'mp': mp, 'findings': items, 'notes': notes, 'policy': policy, 'foreign': foreign, 'elapsed_ms': int((time.time() - t0) * 1000), 'stale': stale}
    return res


def site_json(s):
    d = {k: v for k, v in s.items() if k not in ('expect',)}
    return d


def write_map(res, root):
    mp = res['mp']
    out_dir = os.path.join(root, 'build', 'development')
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, 'hook_map.json')
    doc = {'schema': 1, 'generated_by': 'tools/hook_map.py', 'builds': mp.ga.builds, 'tree': mp.meta['tree'], 'notes': res['notes'],
           'sites': [site_json(s) for s in mp.sites], 'registrations': mp.registrations, 'unresolved': mp.unresolved,
           'findings': [{k: v for k, v in f.items()} for f in res['findings']]}
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(doc, f, indent=1, ensure_ascii=False)
        f.write('\n')
    return path


def summarize(res, args):
    items = res['findings']
    sev = collections.Counter(f['severity'] for f in items)
    byrule = collections.defaultdict(collections.Counter)
    for f in items:
        byrule[f['rule']][f['severity']] += 1
    mp = res['mp']
    act = [s for s in mp.sites if is_active(s)]
    summary = {
        'status': 'hook_map_failed' if sev['error'] else 'hook_map_passed',
        'errors': sev['error'], 'warnings': sev['warn'], 'suppressed_by_policy': sev['suppressed'], 'info': sev['info'],
        'sites': len(mp.sites), 'active_sites': len(act), 'unreached_sites': len(mp.sites) - len(act),
        'by_dll': dict(collections.Counter(s['dll'] for s in act)),
        'by_via': dict(collections.Counter(s['via'] for s in act)),
        'owners': len({s['owner'] for s in act}),
        'registrations': len(mp.registrations),
        'layout_hooks': '%s/%s' % (res['notes'].get('layout_hooks'), res['notes'].get('layout_hook_capacity')),
        'unresolved_arguments': len(mp.unresolved),
        'policy_entries': len(res['policy']), 'stale_policy_entries': len(res['stale']),
        'foreign_sites': len(res['foreign']['sites']), 'foreign_relations': len(res['foreign']['relations']),
        'exe_checked': not args.no_exe, 'r12_compared': res['notes'].get('r12_compared'), 'r13_verified': res['notes'].get('r13_verified'),
        'elapsed_ms': res['elapsed_ms'],
        'rules': {r: dict(c) for r, c in sorted(byrule.items())},
    }
    return summary


# ------------------------------------------------------------------ probe / diff
def probe(res, args):
    mp = res['mp']
    ga = mp.ga
    target = args.probe
    kind = args.kind or 'call'
    ln = args.len or {'vt': 8, 'call': 5, 'jump': 5}.get(kind, 5)
    gid = target if target.upper().startswith('GA_') else 'GA_' + target.upper().lstrip('0')
    gid = 'GA_' + gid[3:].upper()
    lines = []
    out = {'target': gid, 'kind': kind, 'len': ln, 'builds': {}}
    for b in ga.builds:
        r = ga.rva(gid, b) if gid in ga.index else (int(target, 16) if re.fullmatch(r'[0-9A-Fa-f]+', target) else None)
        info = {'rva': None if r is None else '%X' % r, 'ours': [], 'foreign': [], 'observers': [], 'blocked': [], 'same_block': []}
        out['builds'][b] = info
        if r is None:
            info['note'] = 'unresolved in this build'
            continue
        rg = (r, r + ln)
        for s in mp.sites:
            if not is_active(s):
                continue
            sr = rng(s, b)
            if not sr:
                continue
            row = {'owner': s['owner'], 'kind': s['kind'], 'key': s['key'], 'dll': s['dll'], 'via': s['via'], 'src': s['src'], 'range': '%X..%X' % sr}
            if overlap(sr, rg):
                info['ours'].append(row)
            elif kind not in ('vt',) and s['kind'] not in ('vt', 'com') and (sr[0] >> 4) == (r >> 4):
                info['same_block'].append(dict(row, snapshot=bool(s.get('snap'))))
        for fs in res['foreign']['sites']:
            fr = fs['rvas'].get(b)
            if fr is not None and overlap((fr, fr + fs.get('len', 5)), rg):
                info['foreign'].append({'id': fs.get('id'), 'mods': fs.get('mods') or [fs.get('mod')], 'kind': fs.get('kind'), 'behavior': fs.get('behavior')})
        for i, o in enumerate(ga.observers):
            ob = o['rva'][list(ga.builds).index(b)] if list(ga.builds).index(b) < len(o['rva']) else 0
            if ob and overlap((ob, ob + o['len']), rg):
                info['observers'].append('%X (+%d bytes)' % (ob, o['len']))
        for g in sorted(set(mp.reg['blocked']) | set(mp.core['blocked'])):
            a = ga.rva(g, b)
            if a is not None and rg[0] <= a < rg[1]:
                info['blocked'].append(g)
    lines.append('probe %s (kind %s, len %d)' % (gid, kind, ln))
    for b, info in out['builds'].items():
        lines.append('  build %s rva %s' % (b, info['rva']))
        for k in ('ours', 'foreign', 'observers', 'blocked', 'same_block'):
            for row in info[k]:
                lines.append('    %-10s %s' % (k, json.dumps(row, ensure_ascii=False) if isinstance(row, dict) else row))
        if not any(info[k] for k in ('ours', 'foreign', 'observers', 'blocked', 'same_block')):
            lines.append('    free: no owner, foreign hook, observer, blocked entry or block neighbour')
    return out, lines


def diff_maps(cur, old):
    def k(s):
        return (s['owner'], s['kind'], s['key'], s['dll'], s['via'])
    a = collections.Counter(k(s) for s in old.sites if is_active(s))
    b = collections.Counter(k(s) for s in cur.sites if is_active(s))
    return sorted((b - a).elements()), sorted((a - b).elements())


def stage_tree(root, dest, table=None):
    """a minimal copy of the tree the map reads (src/, development/*.json, tools/build_split.ps1); `table` replaces src/game_addresses_data.inl."""
    import shutil
    if os.path.isdir(dest):
        shutil.rmtree(dest)
    shutil.copytree(os.path.join(root, 'src'), os.path.join(dest, 'src'), ignore=shutil.ignore_patterns('__pycache__'))
    os.makedirs(os.path.join(dest, 'development'))
    for fn in os.listdir(os.path.join(root, 'development')):
        if fn.endswith('.json'):
            shutil.copy2(os.path.join(root, 'development', fn), os.path.join(dest, 'development', fn))
    os.makedirs(os.path.join(dest, 'tools'))
    shutil.copy2(os.path.join(root, 'tools', 'build_split.ps1'), os.path.join(dest, 'tools', 'build_split.ps1'))
    if table:
        shutil.copy2(table, os.path.join(dest, 'src', 'game_addresses_data.inl'))
    return dest


def build_diff(root, ref, target, table=None, reader=None):
    """Hook map of a candidate table with the columns [ref, target] (builds named by sha8): what a new Steam build does to our hook sites.
    Returns dict(ref, target, active_sites, same, moved, unresolved[], findings[] (target only, error/warn/info), errors)."""
    dest = stage_tree(root, os.path.join(root, 'build', 'development', 'hookmap_diff', target, 'tree'), table)
    res = analyse(dest, nexe=False, reader=reader)
    mp = res['mp']
    if ref not in mp.ga.builds or target not in mp.ga.builds:
        raise SystemExit('the table carries the builds %s, not %s and %s' % (mp.ga.builds, ref, target))
    act = [s for s in mp.sites if is_active(s)]
    same = moved = 0
    unresolved = []
    for s in act:
        a, b = s['rva'].get(ref), s['rva'].get(target)
        if not s['ga'] or a is None:
            continue
        if b is None:
            unresolved.append({'owner': s['owner'], 'kind': s['kind'], 'ga': s['ga'], 'src': s['src']})
        elif a == b:
            same += 1
        else:
            moved += 1
    seen_ref = {(f['rule'], tuple(f['keys']), tuple(f['owners'])) for f in res['findings'] if f.get('build') == ref}
    new = [f for f in res['findings'] if f.get('build') == target and (f['rule'], tuple(f['keys']), tuple(f['owners'])) not in seen_ref]
    return {'ref': ref, 'target': target, 'active_sites': len(act), 'same': same, 'moved': moved, 'unresolved': unresolved,
            'findings': [{k: f[k] for k in ('rule', 'severity', 'msg', 'owners', 'sites')} for f in new],
            'errors': sum(1 for f in new if f['severity'] == 'error')}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--root', default=ROOT)
    ap.add_argument('--no-exe', action='store_true')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--owner')
    ap.add_argument('--feature')
    ap.add_argument('--diff', nargs='+', metavar='REV|REF TARGET', help='one git revision, or two build names (REF TARGET) with --table')
    ap.add_argument('--table', help='--diff REF TARGET: candidate game_addresses_data.inl with both columns (default: the tree table)')
    ap.add_argument('--probe')
    ap.add_argument('--kind', choices=['vt', 'call', 'jump', 'bytes', 'block'])
    ap.add_argument('--len', type=int)
    ap.add_argument('--policy-skeleton', action='store_true')
    ap.add_argument('--no-write', action='store_true')
    ap.add_argument('-v', '--verbose', action='store_true', help='list info findings too')
    args = ap.parse_args(argv)
    root = os.path.abspath(args.root)
    if args.diff and len(args.diff) == 2:
        out = build_diff(root, args.diff[0], args.diff[1], args.table)
        if args.json:
            print(json.dumps(out, indent=1, ensure_ascii=False))
        else:
            print('hook map diff %s -> %s: %d active sites with an address: %d same, %d moved, %d unresolved in the target, %d findings only in the target (%d errors)' % (
                out['ref'], out['target'], out['active_sites'], out['same'], out['moved'], len(out['unresolved']), len(out['findings']), out['errors']))
            for u in out['unresolved']:
                print('  unresolved %s (%s %s): %s' % (u['ga'], u['owner'], u['kind'], u['src']))
            for f in out['findings']:
                print('  %-5s %-4s %s' % (f['severity'].upper(), f['rule'], f['msg']))
        return 1 if out['errors'] else 0
    if args.diff and len(args.diff) > 2:
        ap.error('--diff takes one git revision or two build names')
    res = analyse(root, nexe=args.no_exe or bool(args.probe) or bool(args.diff))
    if args.probe:
        out, lines = probe(res, args)
        print(json.dumps(out, indent=1, ensure_ascii=False) if args.json else '\n'.join(lines))
        return 0
    path = None if args.no_write else write_map(res, root)
    if args.diff:
        old = X.build_map(X.GitTree(root, args.diff[0], fallback=X.FsTree(root)))
        added, removed = diff_maps(res['mp'], old)
        print('hook map diff against %s: %d added, %d removed (owner, kind, key, dll, via); hook_sites.json bindings come from the working tree' % (args.diff[0], len(added), len(removed)))
        for x in added:
            print('  + %s' % (x,))
        for x in removed:
            print('  - %s' % (x,))
        return 0
    if args.policy_skeleton:
        ents = collections.OrderedDict()
        for f in res['findings']:
            if f['severity'] == 'error' and f['rule'] not in ('policy', 'sites'):
                kk = (f['rule'], tuple(f['keys']), tuple(f['owners']))
                ents.setdefault(kk, {'rule': f['rule'], 'sites': f['keys'], 'owners': f['owners'], 'kind': 'accepted', 'reason': '', 'decided': DECIDED, 'evidence': '', '_msg': f['msg']})
        print(json.dumps({'entries': list(ents.values())}, indent=2, ensure_ascii=False))
        return 0
    items = res['findings']
    fown = {o for o, v in res['mp'].hook_owner_map.items() if args.feature and v[0] == args.feature}      # --feature X: owners of that features.json module
    sel = [f for f in items if (not args.owner or args.owner in f['owners']) and
           (not args.feature or args.feature in f['owners'] or (fown & set(f['owners'])))]
    summary = summarize(res, args)
    if args.json:
        print(json.dumps({'summary': summary, 'findings': [f for f in sel if f['severity'] != 'info' or args.verbose]}, indent=1, ensure_ascii=False))
    else:
        for sv in ('error', 'warn') + (('info',) if args.verbose else ()):
            for f in sel:
                if f['severity'] == sv:
                    print('%-5s %-4s %s%s' % (sv.upper(), f['rule'], ('[%s] ' % f['build'][:8]) if f['build'] else '', f['msg']))
        print(json.dumps(summary, ensure_ascii=False))
        if path:
            print('map: %s' % os.path.relpath(path, root))
    return 1 if summary['errors'] else 0


if __name__ == '__main__':
    sys.exit(main())
