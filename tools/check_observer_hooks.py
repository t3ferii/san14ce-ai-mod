#!/usr/bin/env python
"""Static checks of the read-only observer table (local11, 2026-11-06; docs/HISTORY.md section 18). Called from
tools/check_addresses.py as (c4); also runnable alone:  python -X utf8 tools/check_observer_hooks.py

San14Sim's version.dll (the user's other project, read only here) replaces the first bytes of 23 traced original functions and of the daily-progress
entry by `E9 rel32` + NOP. src/game_addresses_data.inl carries them as ga_observers[] (tools/observer_hooks.json + the unpacked EXE of the
reference build, re-based by tools/addr_promote.py). Checks (standard library + pelib / addrlib only):

 (c4a) the ga_observers[] rows equal what observer_hooks.json ("new" = RVA in the reference build 1a4a447d) and the unpacked EXE say (table not
       stale; prefix bytes as in the EXE, no relocation inside).
 (c4b) observer_hooks.json equals San14Sim/dump/profile.h (kProfiles trace[] of both builds, kTraceBase prefix lengths, Profile::entry) and
       dump.cpp's 6-byte entry patch, when those files exist (skipped, not failed, when the folder is absent).
 (c4c) impact: the source guard rows `{GA(GA_x),size,GH(..)}` (and any id + offset reference) that overlap an observer prefix are exactly the
       three known ones (building_destruction GA_1F5680, advisor_briefing GA_44B090 / GA_44C340), all judged through ga::hash_ok; no source patches
       or links an id that lies inside a prefix (prepare_link / prepare_bytes / publish).
 (c4d) removed 2026-11-06 (docs/HISTORY.md S5): the ModPack deaths blob is gone; its replacement guard rows (battle_deaths_profile.h,
       {GA(..),size,GH(..)}) are scanned by (c4c) like every other source row and must not overlap an observer prefix.
Exit status 0 only when all pass."""
import os, re, sys, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import addrlib as A
import pelib

HOOKS = os.path.join(A.ROOT, 'tools', 'observer_hooks.json')
INL = os.path.join(A.ROOT, 'src', 'game_addresses_data.inl')
SAN14SIM = os.path.join(os.path.dirname(A.ROOT), 'San14Sim', 'dump')
KNOWN_ROW_OVERLAPS = {('building_destruction_profile.h', '1F5680'), ('advisor_briefing_profile.h', '44B090'), ('advisor_briefing_profile.h', '44C340'),
                      # 2026-11-17 종군 중립군 stage 0: native_ccz_bands.inl links the 186E60 / 278290 entries. With the San14Sim observer
                      # loaded, prepare_link's observer_conflict refuses and the whole band feature stays off (docs/NEUTRAL_BANDITS.md 11).
                      ('native_ccz_bands.inl', '186E60'), ('native_ccz_bands.inl', '278290')}
fails = []


def fail(msg):
    fails.append(msg)
    print('FAIL', msg)


def entries():
    return json.load(open(HOOKS, encoding='utf-8'))['entries']


def ranges(b):
    """[(rva, len)] per observer entry of build index b (0 = 1fbe6be7, 1 = 1a4a447d)."""
    return [(int(e['old' if b == 0 else 'new'], 16), int(e['len'])) for e in entries()]


def check_table():
    txt = open(INL, encoding='utf-8').read()
    m = re.search(r'GA_OBSERVER_COUNT=(\d+)', txt)
    ent = entries()
    if not m or int(m.group(1)) != len(ent):
        fail('(c4a) src/game_addresses_data.inl GA_OBSERVER_COUNT (%s) differs from observer_hooks.json (%d): re-base the table (tools/addr_promote.py)' % (m.group(1) if m else '?', len(ent)))
        return
    rows = re.search(r'ga_observers\[GA_OBSERVER_COUNT\]=\{(.*?)\};', txt, re.S).group(1)
    got = re.findall(r'\{\{0x([0-9A-F]+)\},(\d+),\{([^}]*)\}\}', rows)
    if len(got) != len(ent):
        fail('(c4a) generated ga_observers has %d rows, observer_hooks.json %d' % (len(got), len(ent)))
        return
    new = pelib.open_build('current')
    bad = []
    for e, (rn, ln, by) in zip(ent, got):
        rn, ln = int(rn, 16), int(ln)
        want_n, want_l = int(e['new'], 16), int(e['len'])
        data = bytes(int(x, 16) for x in by.split(','))
        bn = new.raw(want_n, want_l)
        if (rn, ln) != (want_n, want_l) or data != bn + bytes(16 - want_l):
            bad.append(e['new'])
        lo = want_n - new.text_va
        if any(new.rel()[lo:lo + want_l]):
            bad.append(e['new'] + ' (relocation)')
    if bad:
        fail('(c4a) ga_observers rows stale or wrong for %s: re-base the table (tools/addr_promote.py)' % ' '.join(bad))
    else:
        print('(c4a) ga_observers: %d entries, original prefix bytes equal the unpacked EXE, no relocation inside, data.inl current' % len(ent))


def check_san14sim():
    prof = os.path.join(SAN14SIM, 'profile.h')
    if not os.path.exists(prof):
        print('(c4b) San14Sim/dump/profile.h not found: comparison skipped')
        return
    src = open(prof, encoding='utf-8').read()
    # Profile initialiser: {"name", timestamp, entry, world_global, {trace RVAs...}, settings}
    trace, prof_entry = {}, {}
    for name, ent, body in re.findall(r'\{"(new|old) [^"]*",\s*0x[0-9A-Fa-f]+u,\s*0x([0-9A-Fa-f]+)u,\s*0x[0-9A-Fa-f]+u,\s*\{([^}]*)\}', src):
        trace[name] = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]+)', body)]
        prof_entry[name] = int(ent, 16)
    tb = re.search(r'kTraceBase\[kTraceCount\]=\{(.*?)\n\};', src, re.S).group(1)
    lens = [int(m.group(1)) for m in re.finditer(r'^\s*\{0,\s*(\d+),\s*\{', tb, re.M)]
    problems = []
    for key, b in (('old', 0), ('new', 1)):
        if key not in trace:
            problems.append('cannot parse the %s profile of profile.h' % key)
            continue
        want = ranges(b)
        if trace[key] != [w[0] for w in want[:23]]:
            problems.append('%s: traced RVAs differ from observer_hooks.json (first mismatch %s)' % (key, next((('%X' % a, '%X' % w[0]) for a, w in zip(trace[key], want) if a != w[0]), len(trace[key]))))
        if prof_entry[key] != want[23][0]:
            problems.append('%s: Profile::entry %X != observer_hooks.json entry hook %X' % (key, prof_entry[key], want[23][0]))
    if lens != [w[1] for w in ranges(0)[:23]]:
        problems.append('kTraceBase prefix lengths %s differ from observer_hooks.json' % lens)
    dump = os.path.join(SAN14SIM, 'dump.cpp')
    if os.path.exists(dump):
        d = open(dump, encoding='utf-8', errors='replace').read()
        if 'unsigned char patch[6]={0xe9,0,0,0,0,0x90}' not in d or 'kEntryGuard' not in d:
            problems.append('dump.cpp entry patch is no longer the 6-byte `E9 rel32 NOP` (observer_hooks.json entry hook len 6)')
    if problems:
        for p in problems:
            fail('(c4b) San14Sim profile vs observer_hooks.json: ' + p)
    else:
        print('(c4b) observer_hooks.json equals San14Sim profile.h (23 traced functions of both builds, prefix lengths, Profile::entry) and dump.cpp 6-byte entry patch')


def table_values():
    import ga_table
    T = ga_table.parse(INL)
    out = {}
    for e in T.entries:
        v = e.vals[T.builds[0][0]]
        out[e.name[3:].upper()] = (v, sorted(e.users))
    return out


def check_impact():
    vals = table_values()
    obs = [(int(e['old'], 16), int(e['new'], 16), int(e['len'])) for e in entries()]
    srcdir = os.path.join(A.ROOT, 'src')
    row = re.compile(r'GA\(GA_([0-9A-Fa-f]+)\)\s*(\+\s*0[xX][0-9A-Fa-f]+|\+\s*\d+)?\s*,\s*(0[xX][0-9A-Fa-f]+|\d+)\b')
    off_ref = [re.compile(r'GA\(GA_([0-9A-Fa-f]+)\)\s*([+-])\s*(0[xX][0-9A-Fa-f]+|\d+)'), re.compile(r'mcmp\(image,\s*GA_([0-9A-Fa-f]+)\s*,\s*(0[xX][0-9A-Fa-f]+|\d+)\s*,')]
    found = set()
    n_rows = 0
    for fn in sorted(os.listdir(srcdir)):
        if os.path.splitext(fn)[1] not in ('.h', '.inl', '.cpp') or fn.startswith('game_addresses'):
            continue
        for ln, line in enumerate(open(os.path.join(srcdir, fn), encoding='utf-8', errors='replace'), 1):
            spans = []
            for m in row.finditer(line):
                idn = m.group(1).upper()
                if idn in vals:
                    spans.append((idn, int(m.group(2).replace('+', '').strip(), 0) if m.group(2) else 0, int(m.group(3), 0)))
            for pi, p in enumerate(off_ref):
                for m in p.finditer(line):
                    idn = m.group(1).upper()
                    if idn in vals:
                        spans.append((idn, int(m.group(3), 0) * (1 if m.group(2) == '+' else -1) if pi == 0 else int(m.group(2), 0), 1))
            for idn, off, size in spans:
                n_rows += 1
                v = vals[idn][0]
                if not v:
                    continue
                lo, hi = v + off, v + off + size
                for (ro, rn, ln) in obs:
                    if lo < rn + ln and hi > rn:
                        found.add((fn, idn if off == 0 else '%s+%X' % (idn, off), rn))
    unexpected = sorted(x for x in found if (x[0], x[1]) not in KNOWN_ROW_OVERLAPS)
    if unexpected:
        fail('(c4c) guard rows / offset references overlap an observer prefix without a tolerance decision: %s' % ', '.join('%s GA_%s (entry %X)' % x for x in unexpected))
    missing = sorted(KNOWN_ROW_OVERLAPS - {(f, i) for f, i, _ in found})
    if missing:
        fail('(c4c) the known observer overlaps are gone from the sources (update KNOWN_ROW_OVERLAPS and the doc): %s' % missing)
    # ids lying inside a prefix: nobody may patch or link them
    inside = set()
    for (ro, rn, ln) in obs:
        for idn, (n, users) in vals.items():
            if n and rn <= n < rn + ln:
                inside.add(idn)
    patchers = []
    for fn in sorted(os.listdir(srcdir)):
        if os.path.splitext(fn)[1] not in ('.h', '.inl', '.cpp') or fn.startswith('game_addresses'):
            continue
        for ln, line in enumerate(open(os.path.join(srcdir, fn), encoding='utf-8', errors='replace'), 1):
            if re.search(r'prepare_link|prepare_bytes|publish_link|modpack_write|ROUTE_|hook_call|hook_jump', line):
                for idn in inside:
                    if re.search(r'GA_%s\b' % idn, line):
                        patchers.append('%s:%d GA_%s' % (fn, ln, idn))
    if patchers:
        fail('(c4c) a source patches/links an observed entry (real conflict, decide before shipping): %s' % ', '.join(patchers))
    print('(c4c) impact: %d id-based guard ranges scanned, overlaps with observer prefixes = %s; ids inside a prefix: %s; patched/linked by sources: none' % (
        n_rows, sorted(found and {(f, i) for f, i, _ in found}), ' '.join('GA_' + i for i in sorted(inside))))


def run():
    check_table()
    check_san14sim()
    check_impact()
    return len(fails)


if __name__ == '__main__':
    n = run()
    print(json.dumps({'status': 'check_observer_hooks_failed' if n else 'check_observer_hooks_passed', 'failures': n}))
    sys.exit(1 if n else 0)
