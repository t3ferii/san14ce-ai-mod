#!/usr/bin/env python
"""Parse and emit src/game_addresses_data.inl (the generated address table) without any game binary.

Used by tools/addr_promote.py (re-base of the table to a new reference build) and usable by any tool that needs the table as data.
Model (Table): ids (name, kind, method, parameters, per-build values, users, wildcard mask) + hash pairs + build rows + probes + observers.
Layouts understood: one build column (current layout, ga_builds[1]) and two build columns + exact-byte windows + hash pairs (pre re-base layout).
Standard library only.
"""
import re

KIND = ('func', 'mid', 'slot', 'data')
METHOD = ('table', 'sig', 'mid', 'rtti', 'slotfn', 'dataref')


class Entry:
    __slots__ = ('name', 'rva', 'kind', 'method', 'len', 'a', 'b', 'c', 'd', 'pat', 'mask', 'rname', 'users', 'wild', 'vals')

    def __init__(self):
        self.users = set()
        self.vals = {}
        self.pat = self.mask = self.rname = None
        self.wild = 0


class Table:
    def __init__(self):
        self.entries = []             # list of Entry, enum order
        self.files = []               # source file names (users bitmap order)
        self.builds = []              # [(name, timestamp, text_size, text_crc, image_size)]
        self.hash_pairs = []          # [(old, new)] (two-column layout only)
        self.probes = []              # [(entry name, 5 bytes)]
        self.observers = []           # [(rvas tuple, len, 16 bytes)]
        self.expb = None              # {build: (pool, off[], post[])} (two-column layout only)

    def by_name(self):
        return {e.name: i for i, e in enumerate(self.entries)}


def _braced(text, start):
    """text between the '{' at/after start and its matching '}' (nested braces counted)."""
    i = text.index('{', start)
    depth = 0
    for j in range(i, len(text)):
        c = text[j]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return text[i + 1:j], j + 1
    raise ValueError('unbalanced braces')


def _ints(s):
    return [int(x, 0) for x in re.findall(r'0[xX][0-9A-Fa-f]+|\d+', s.replace('ULL', '').replace('U', '').replace('u', ''))]


def parse(path):
    return parse_text(open(path, encoding='utf-8').read())


def parse_text(t):
    T = Table()
    m = re.search(r'enum GaId : unsigned \{(.*?)GA_COUNT=(\d+)\};', t, re.S)
    names = re.findall(r'(GA_\w+)=(\d+)', m.group(1))
    n = int(m.group(2))
    assert len(names) == n and [int(x[1]) for x in names] == list(range(n))
    for nm, _ in names:
        e = Entry()
        e.name = nm
        T.entries.append(e)
    m = re.search(r'ga_file_names\[GA_FILE_COUNT\]=\{(.*?)\};', t, re.S)
    T.files = re.findall(r'"([^"]*)"', m.group(1))
    rows = re.findall(r'\{(0x[0-9A-Fa-f]+),(\d+),(\d+),(\d+),(\d+)u,(\d+)u,(\d+)u,(-?\d+)\}', t[t.index('ga_entries[GA_COUNT]'):t.index('static const unsigned char ga_pool')])
    assert len(rows) == n, (len(rows), n)
    pool = bytes(_ints(_braced(t, t.index('ga_pool['))[0]))
    names_blob = bytes(_ints(_braced(t, t.index('ga_names['))[0]))
    for e, r in zip(T.entries, rows):
        e.rva = int(r[0], 16)
        e.kind, e.method, e.len = int(r[1]), int(r[2]), int(r[3])
        e.a, e.b, e.c, e.d = int(r[4]), int(r[5]), int(r[6]), int(r[7])
        if e.method == 1:
            e.pat, e.mask = pool[e.a:e.a + e.len], pool[e.a + e.len:e.a + 2 * e.len]
        elif e.method == 2:
            e.pat, e.mask = pool[e.b:e.b + e.len], pool[e.b + e.len:e.b + 2 * e.len]
        elif e.method == 3:
            end = names_blob.index(b'\0', e.a)
            e.rname = names_blob[e.a:end].decode('latin1')
    body = _braced(t, t.index('ga_users[GA_COUNT]'))[0]
    words = re.findall(r'\{(0x[0-9A-Fa-f]+)ULL,(0x[0-9A-Fa-f]+)ULL\}', body)
    assert len(words) == n
    for e, w in zip(T.entries, words):
        v = int(w[0], 16) | (int(w[1], 16) << 64)
        for k, f in enumerate(T.files):
            if (v >> k) & 1:
                e.users.add(f)
    wild = _ints(_braced(t, t.index('ga_wild[GA_COUNT]'))[0])
    assert len(wild) == n
    for e, w in zip(T.entries, wild):
        e.wild = w
    if 'ga_hash_pairs' in t:
        hp = re.findall(r'\{(0x[0-9A-Fa-f]+)ULL,(0x[0-9A-Fa-f]+)ULL\}', _braced(t, t.index('ga_hash_pairs['))[0])
        T.hash_pairs = [(int(a, 16), int(b, 16)) for a, b in hp]
    bm = re.search(r'ga_builds\[(\d)\]=\{(.*?)\};\n', t, re.S)
    nb = int(bm.group(1))
    brow = re.findall(r'\{"(\w+)",(0x[0-9A-Fa-f]+)U,(\d+)u,(0x[0-9A-Fa-f]+)U,(\d+)u,ga_val_(\w+)\}', bm.group(2))
    assert len(brow) == nb
    for name, ts, ts_size, crc, isz, vname in brow:
        T.builds.append((name, int(ts, 16), int(ts_size), int(crc, 16), int(isz)))
        vals = _ints(_braced(t, t.index('ga_val_%s[GA_COUNT]' % vname))[0])
        assert len(vals) == n
        for e, v in zip(T.entries, vals):
            e.vals[name] = v
    if 'ga_expb_pool_' in t:
        T.expb = {}
        for mm in re.finditer(r'ga_expb_pool_(\w+)\[', t):
            b = mm.group(1)
            T.expb[b] = (bytes(_ints(_braced(t, mm.start())[0])), _ints(_braced(t, t.index('ga_expb_off_%s[' % b))[0]),
                         _ints(_braced(t, t.index('ga_expb_post_%s[' % b))[0]))
    pm = _braced(t, t.index('ga_probes['))[0]
    T.probes = [(a, tuple(int(x, 16) for x in b.split(','))) for a, b in re.findall(r'\{(GA_\w+),\{([^}]*)\}\}', pm)]
    om = t[t.index('ga_observers[GA_OBSERVER_COUNT]'):]
    om = _braced(om, 0)[0]
    for mm in re.finditer(r'\{\{([^}]*)\},(\d+),\{([^}]*)\}\}', om):
        T.observers.append((tuple(int(x, 16) for x in mm.group(1).split(',')), int(mm.group(2)), bytes(int(x, 16) for x in mm.group(3).split(','))))
    return T


def emit(T, header):
    """text of the data file for Table T (single or multi build). header = first comment lines."""
    n = len(T.entries)
    idx = T.by_name()
    pool = bytearray()
    names = bytearray(b'\0')
    name_off = {}
    rows = []
    for e in T.entries:
        a = b = c = d = ln = 0
        if e.method == 1:
            ln, a = len(e.pat), len(pool)
            pool += e.pat + e.mask
        elif e.method == 2:
            ln, a, b, c, d = len(e.pat), e.a, len(pool), e.c, e.d
            pool += e.pat + e.mask
        elif e.method == 3:
            if e.rname not in name_off:
                name_off[e.rname] = len(names)
                names += e.rname.encode('latin1') + b'\0'
            a, b, c = name_off[e.rname], e.b, e.c
        elif e.method == 4:
            a = e.a
        elif e.method == 5:
            a, b, c = e.a, e.b, e.c
        rows.append('{0x%X,%d,%d,%d,%du,%du,%du,%d}' % (e.rva, e.kind, e.method, ln, a, b, c, d))
    fidx = {f: i for i, f in enumerate(T.files)}
    out = list(header)
    w = out.append
    w('enum GaId : unsigned {')
    line = []
    for i, e in enumerate(T.entries):
        line.append('%s=%d' % (e.name, i))
        if len(line) == 8:
            w(','.join(line) + ',')
            line = []
    if line:
        w(','.join(line) + ',')
    w('GA_COUNT=%d};' % n)
    w('enum : unsigned {GA_FILE_COUNT=%d,GA_FILE_WORDS=2};' % len(T.files))
    w('static const char *const ga_file_names[GA_FILE_COUNT]={' + ','.join('"%s"' % f for f in T.files) + '};')
    w('enum GaFile : unsigned {' + ','.join('GF_%s=%d' % (f.replace('.', '_'), i) for i, f in enumerate(T.files)) + '};')
    w('struct GaEntry {unsigned rva;unsigned char kind,method;unsigned short len;unsigned a,b,c;int d;};')
    w('static const GaEntry ga_entries[GA_COUNT]={')
    for i in range(0, n, 4):
        w(','.join(rows[i:i + 4]) + ',')
    w('};')
    w('static const unsigned char ga_pool[%d]={' % len(pool) + ','.join(str(x) for x in pool) + '};')
    w('static const char ga_names[%d]={' % len(names) + ','.join(str(x) for x in names) + '};')
    w('static const unsigned long long ga_users[GA_COUNT][GA_FILE_WORDS]={')
    for e in T.entries:
        words = [0, 0]
        for u in e.users:
            k = fidx[u]
            words[k // 64] |= 1 << (k % 64)
        w('{' + ','.join('0x%XULL' % x for x in words) + '},')
    w('};')
    w('static const unsigned ga_wild[GA_COUNT]={')
    for i in range(0, n, 12):
        w(','.join('0x%X' % T.entries[j].wild for j in range(i, min(n, i + 12))) + ',')
    w('};')
    if T.hash_pairs:
        w('static const unsigned long long ga_hash_pairs[%d][2]={' % len(T.hash_pairs) + ','.join('{0x%XULL,0x%XULL}' % p for p in T.hash_pairs) + '};')
        w('static const unsigned ga_hash_count=%d;' % len(T.hash_pairs))
    brow = []
    for name, ts, tsize, crc, isz in T.builds:
        w('static const unsigned ga_val_%s[GA_COUNT]={' % name)
        for i in range(0, n, 12):
            w(','.join('0x%X' % T.entries[j].vals[name] for j in range(i, min(n, i + 12))) + ',')
        w('};')
        brow.append('{"%s",0x%XU,%du,0x%XU,%du,ga_val_%s}' % (name, ts, tsize, crc, isz, name))
    if T.expb:
        for b, (pl, off, post) in T.expb.items():
            w('static const unsigned char ga_expb_pool_%s[%d]={' % (b, max(1, len(pl))) + ','.join(str(x) for x in pl) + '};')
            w('static const unsigned ga_expb_off_%s[GA_COUNT]={' % b + ','.join(str(x) for x in off) + '};')
            w('static const unsigned short ga_expb_post_%s[GA_COUNT]={' % b + ','.join(str(x) for x in post) + '};')
    w('struct GaBuild {const char *name;unsigned timestamp,text_size,text_crc,image_size;const unsigned *vals;};')
    w('static const GaBuild ga_builds[%d]={' % len(T.builds) + ','.join(brow) + '};')
    w('struct GaProbe {unsigned id;unsigned char bytes[5];};')
    w('static const GaProbe ga_probes[%d]={' % len(T.probes) + ','.join('{%s,{%s}}' % (nm, ','.join('0x%02X' % x for x in b)) for nm, b in T.probes) + '};')
    nbuild = len(T.builds)
    w('// read-only observer entries (tools/observer_hooks.json): rva[build index], displaced prefix length, original prefix bytes')
    w('enum : unsigned {GA_OBSERVER_COUNT=%d};' % len(T.observers))
    w('struct GaObserver {unsigned rva[%d];unsigned char len;unsigned char bytes[16];};' % nbuild)
    w('static const GaObserver ga_observers[GA_OBSERVER_COUNT]={' + ','.join(
        '{{%s},%d,{%s}}' % (','.join('0x%X' % r for r in rv), ln, ','.join('0x%02X' % x for x in bs)) for rv, ln, bs in T.observers) + '};')
    return '\n'.join(out) + '\n'
