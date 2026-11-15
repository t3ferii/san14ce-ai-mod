"""SQLite index over the read-only Ghidra exports (research/ghidra_full/*, research/ghidra_refs/*).

Build / refresh:   python -X utf8 tools/ghidra_index.py [--force] [--only <name-part>] [--check]
Each export folder gets <export>/index.sqlite. It is rebuilt only when the export's TSV/manifest/c-shard
sizes or mtimes changed (meta 'sig'). Decompiled bodies are NOT copied: functions.off/len point into the shard.
ghidra_full.py uses the index when it is fresh and falls back to reading the TSVs otherwise.

Extra tables for the two original-EXE builds (RTTI read from the unpacked analysis EXE, read-only):
  rtti_vtable, rtti_bases, ui_vtable_slots (slot -> function), build_map (old<->new build by RTTI name).
"""
import bisect, csv, json, os, re, sqlite3, sys, time
from pathlib import Path

csv.field_size_limit(sys.maxsize)
BASE = Path(__file__).resolve().parents[1]
RESEARCH = BASE / 'research'
FULL = RESEARCH / 'ghidra_full'
REFS = RESEARCH / 'ghidra_refs'
INDEX_NAME = 'index.sqlite'
SCHEMA = 7
IMAGE_BASE = 0x140000000
BUILD_ORDER = ('1fbe6be7', '1a4a447d')          # older build first (map columns vt_a / vt_b)
EXE_DIRS = [Path.home() / 'Documents' / 'Codex' / 'analysis' / 'SAN14CE_KO']
HEX8 = re.compile(r'(?:[0-9A-F]{8}|[1-9A-F][0-9A-F]{8,})\Z')   # canonical '%08X' text (wider values keep their digits)
DEC = re.compile(r'(?:0|[1-9][0-9]*)\Z')
FUNC_PREFIX = b'// ==== FUNC '
UI_NAME = re.compile(r'UI|Dlg|Dialog|Window|Wnd|Tip|Layout|Panel|Btn|Button|Menu|Cmn|Footer|Balloon|Cutin|Slider|Scroll|Gauge|Cursor|Popup|Screen|Gadget|Widget|Tab(?![a-z])|Caption')
UI_DOC_LINE = re.compile(r'UI|버튼|화면|레이아웃|패널|툴팁|메뉴|다이얼로그|Dlg|확인 창|대화창|(?<![가-힣])창(?![가-힣])')


# ---------------------------------------------------------------- freshness
def source_files(path):
    out = []
    for p in sorted(path.iterdir()):
        if p.is_file() and (p.suffix == '.tsv' or p.name == 'manifest.json'):
            out.append(p)
    c = path / 'c'
    if c.is_dir():
        out += sorted(c.glob('*.c'))
    return out


def source_sig(path):
    parts = ['schema%d' % SCHEMA]
    for p in source_files(path):
        st = p.stat()
        parts.append('%s|%d|%d' % (p.relative_to(path).as_posix(), st.st_size, st.st_mtime_ns))
    return '\n'.join(parts)


def file_uri(p):
    """file: URI without pathlib.as_uri (that imports urllib, ~0.2 s)."""
    s = str(Path(p).resolve()).replace('\\', '/').replace('%', '%25').replace('?', '%3f').replace('#', '%23')
    return 'file:///' + s if not s.startswith('/') else 'file://' + s


def index_path(path):
    return Path(path) / INDEX_NAME


def open_fresh(path, check=True):
    """Read-only connection if the index exists and matches the export, else None."""
    ip = index_path(path)
    if not ip.exists():
        return None
    con = None
    try:
        con = sqlite3.connect(file_uri(ip) + '?mode=ro', uri=True)
        con.execute('PRAGMA query_only=1')
        row = con.execute("SELECT value FROM meta WHERE key='sig'").fetchone()
        if row is None or (check and row[0] != source_sig(path)):
            con.close()
            return None
        return con
    except (sqlite3.Error, OSError):
        if con is not None:
            con.close()
        return None


# ---------------------------------------------------------------- loading
def load_tsv(p):
    with p.open(encoding='utf-8', newline='') as f:
        rd = csv.reader(f, delimiter='\t', quoting=csv.QUOTE_NONE)
        header = None
        rows = []
        bad = 0
        for r in rd:
            if not r:
                continue
            if header is None:
                header = r
                continue
            if len(r) != len(header):
                bad += 1
                r = (r + [''] * len(header))[:len(header)]
            rows.append(r)
    return header, rows, bad


NUM_HEX = {'rva', 'end', 'caller_func_rva', 'callsite_rva', 'callee_rva', 'from_rva', 'from_func_rva', 'to_rva', 'vftable_rva',
           'target_rva', 'first_ref_func', 'iat_rva', 'game_rva', 'loc_rva', 'func_rva'}
NUM_DEC = {'size', 'line', 'parameters', 'slot', 'code_refs'}


def col_kind(vals, name=None):
    """'x' canonical hex address, 'd' canonical decimal, 't' text. Only known numeric columns are ever typed."""
    if name is not None and name not in NUM_HEX and name not in NUM_DEC:
        return 't'
    nonempty = [v for v in vals if v != '']
    if nonempty and all(HEX8.match(v) for v in nonempty):
        return 'x'
    if nonempty and all(DEC.match(v) for v in nonempty):
        return 'd'
    return 't'


def conv(kind, vals):
    if kind == 'x':
        return [int(v, 16) if v != '' else None for v in vals]
    if kind == 'd':
        return [int(v) if v != '' else None for v in vals]
    return vals


def q(name):
    return '"%s"' % name


def create_table(con, fmt, name, header, rows, extra=()):
    cols = list(zip(*rows)) if rows else [[] for _ in header]
    kinds = []
    data = []
    for h, vals in zip(header, cols):
        vals = list(vals)
        k = col_kind(vals, h)
        kinds.append(k)
        data.append(conv(k, vals))
    fmt[name] = dict(zip(header, kinds))
    decl = ', '.join('%s %s' % (q(h), 'TEXT' if k == 't' else 'INTEGER') for h, k in zip(header, kinds))
    for e in extra:
        decl += ', %s INTEGER' % q(e[0])
    con.execute('CREATE TABLE %s (id INTEGER PRIMARY KEY, %s)' % (q(name), decl))
    ncol = len(header) + len(extra)
    allcols = data + [e[1] for e in extra]
    con.executemany('INSERT INTO %s (%s) VALUES (%s)' % (q(name), ','.join(q(h) for h in header + [e[0] for e in extra]), ','.join('?' * ncol)), zip(*allcols) if rows else [])
    return kinds


def scan_shard(data):
    """Return (header line numbers, header offsets) for '// ==== FUNC ' lines."""
    offs = []
    if data.startswith(FUNC_PREFIX):
        offs.append(0)
    pos = 0
    pat = b'\n' + FUNC_PREFIX
    while True:
        i = data.find(pat, pos)
        if i < 0:
            break
        offs.append(i + 1)
        pos = i + 1
    return offs


def function_extents(root, rows, header):
    """Byte offset/length of each function body inside its shard (same cut rule as ghidra_full.Root.body)."""
    ix = {h: i for i, h in enumerate(header)}
    by_shard = {}
    for n, r in enumerate(rows):
        by_shard.setdefault(r[ix['shard']], []).append(n)
    extents = [(None, None)] * len(rows)
    for shard, ids in by_shard.items():
        sp = root / shard
        if not sp.exists():
            continue
        data = sp.read_bytes()
        if data.count(b'\r') != data.count(b'\r\n'):
            raise ValueError('lone CR in %s: legacy path required' % sp)
        hoffs = scan_shard(data)
        hlines = []                       # 1-based line number of each header
        prev_off = 0
        prev_line = 1
        for o in hoffs:
            prev_line += data.count(b'\n', prev_off, o)
            prev_off = o
            hlines.append(prev_line)
        for n in ids:
            r = rows[n]
            L = int(r[ix['line']])
            rva = r[ix['rva']]
            own = FUNC_PREFIX + rva.encode() + b' '
            k = bisect.bisect_left(hlines, L)
            if k < len(hlines) and hlines[k] == L:
                start = hoffs[k]
            else:
                # line L is not a header: advance from the previous header (or file start)
                if k == 0:
                    off, ln = 0, 1
                else:
                    off, ln = hoffs[k - 1], hlines[k - 1]
                while ln < L:
                    j = data.find(b'\n', off)
                    if j < 0:
                        off = len(data)
                        break
                    off = j + 1
                    ln += 1
                start = off
                if start >= len(data):
                    extents[n] = (0, 0)
                    continue
            end = len(data)
            kk = bisect.bisect_left(hoffs, start)
            while kk < len(hoffs):
                if not data.startswith(own, hoffs[kk]):
                    end = hoffs[kk]
                    break
                kk += 1
            extents[n] = (start, end - start)
    return extents


# ---------------------------------------------------------------- PE / RTTI
def find_exe(folder):
    for d in EXE_DIRS:
        p = d / folder / 'input' / 'game.exe.unpacked.exe'
        if p.exists():
            return p
    return None


class Pe:
    def __init__(self, data):
        import struct
        self.d = data
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        n = struct.unpack_from('<H', data, pe + 6)[0]
        opt = struct.unpack_from('<H', data, pe + 20)[0]
        self.secs = []
        o = pe + 24 + opt
        for i in range(n):
            nm, vs, va, rs, ro = struct.unpack_from('<8sIIII', data, o + 40 * i)
            ch = struct.unpack_from('<I', data, o + 40 * i + 36)[0]
            self.secs.append((nm.rstrip(b'\0').decode('latin1'), va, vs, ro, rs, ch))

    def off(self, rva):
        for nm, va, vs, ro, rs, ch in self.secs:
            if va <= rva < va + min(vs, rs):
                return ro + rva - va
        return None

    def is_code(self, rva):
        for nm, va, vs, ro, rs, ch in self.secs:
            if va <= rva < va + vs:
                return bool(ch & 0x20000000)
        return False

    def u32(self, rva):
        import struct
        o = self.off(rva)
        return struct.unpack_from('<I', self.d, o)[0] if o is not None else None

    def cstr(self, rva):
        o = self.off(rva)
        if o is None:
            return None
        return self.d[o:self.d.index(b'\0', o)].decode('latin1')


def demangle(m):
    """.?AVName@ns@@ -> ns::Name ; templates keep their raw text. Returns (name, outer, ns_tail)."""
    s = m[4:]
    if s.startswith('?$'):
        outer = s[2:].split('@', 1)[0]
        tail = s[:-2].rsplit('@', 1)[-1] if s.endswith('@@') else ''
        return s, outer, tail
    parts = s.split('@')
    names = []
    for p in parts:
        if p == '':
            break
        names.append(p)
    return '::'.join(reversed(names)), names[0] if names else s, names[-1] if len(names) > 1 else ''


def rtti_scan(exe):
    """vtables with RTTI: list of dicts (vtable, col, offset, mangled, slots[], bases[])."""
    import array, struct
    pe = Pe(exe)
    data_secs = [s for s in pe.secs if s[0] in ('.rdata', '_RDATA', '.data')]
    cols = {}
    for nm, va, vs, ro, rs, ch in data_secs:
        ln = min(vs, rs) & ~3
        a = array.array('I')
        a.frombytes(exe[ro:ro + ln])
        for i in range(len(a) - 6):
            if a[i] == 1 and a[i + 5] == va + 4 * i:
                cols[va + 4 * i] = (a[i + 1], a[i + 2], a[i + 3], a[i + 4])
    out = []
    for nm, va, vs, ro, rs, ch in data_secs:
        ln = min(vs, rs) & ~7
        a = array.array('Q')
        a.frombytes(exe[ro:ro + ln])
        for i, v in enumerate(a):
            r = v - IMAGE_BASE
            if r in cols:
                vt = va + 8 * i + 8
                off, cd, tdr, chd = cols[r]
                mangled = pe.cstr(tdr + 16)
                if not mangled or not mangled.startswith('.?A'):
                    continue
                slots = []
                o = pe.off(vt)
                k = 0
                while o is not None and o + 8 * (k + 1) <= len(exe):
                    t = struct.unpack_from('<Q', exe, o + 8 * k)[0] - IMAGE_BASE
                    if not (0 < t < 0x10000000 and pe.is_code(t)):
                        break
                    slots.append(t)
                    k += 1
                bases = []
                if chd:
                    nb = pe.u32(chd + 8)
                    arr = pe.u32(chd + 12)
                    if nb is not None and arr is not None and nb < 256:
                        for j in range(1, nb):
                            bcd = pe.u32(arr + 4 * j)
                            btd = pe.u32(bcd) if bcd else None
                            nm2 = pe.cstr(btd + 16) if btd else None
                            if nm2:
                                bases.append(nm2)
                out.append({'vtable': vt, 'col': r, 'offset': off, 'mangled': mangled, 'slots': slots, 'bases': bases})
    out.sort(key=lambda x: x['vtable'])
    return out


def doc_rvas():
    """Hex RVA tokens the project docs discuss in a UI context (old-build addresses)."""
    toks = set()
    tok = re.compile(r'\b[0-9A-F]{5,7}\b')

    def keep(t):
        return re.search('[A-F]', t) or len(t) >= 6
    p = BASE / 'docs' / 'UI_ADD_GUIDE.md'
    if p.exists():
        toks.update(int(t, 16) for t in tok.findall(p.read_text(encoding='utf-8', errors='replace')) if keep(t))
    p = BASE / 'docs' / 'NATIVE_FUNCTIONS.md'
    if p.exists():
        for line in p.read_text(encoding='utf-8', errors='replace').splitlines():
            if UI_DOC_LINE.search(line):
                toks.update(int(t, 16) for t in tok.findall(line) if keep(t))
    return toks


def ui_by_name(rec):
    name, outer, tail = demangle(rec['mangled'])
    if tail == 'std' or name.startswith('std::') or outer.startswith('_Func_') or outer.startswith('_Ref_count'):
        return False
    return bool(UI_NAME.search(outer))


def build_rtti(folder8, build_dirname):
    import hashlib
    exe = find_exe(build_dirname)
    if exe is None:
        return None
    data = exe.read_bytes()
    manifest = json.loads((FULL / build_dirname / 'manifest.json').read_text(encoding='utf-8'))
    if hashlib.sha256(data).hexdigest() != manifest.get('sha256'):
        print('  RTTI skipped: %s hash differs from manifest' % exe, file=sys.stderr)
        return None
    return rtti_scan(data)


def keyed(scan):
    """(mangled, col_offset, ordinal among duplicates) -> record; sets rec['ord']."""
    seen = {}
    res = {}
    for r in scan:
        k = (r['mangled'], r['offset'])
        r['ord'] = seen.get(k, 0)
        seen[k] = r['ord'] + 1
        res[(k[0], k[1], r['ord'])] = r
    return res


def annotate(scans):
    """scans: {build8: [records]}. Sets rec['reasons'] (UI evidence: name / base / doc)."""
    for b8, scan in scans.items():
        keyed(scan)
        ui_names = set()
        for r in scan:
            r['name_ui'] = ui_by_name(r)
            if r['name_ui']:
                ui_names.add(r['mangled'])
        share = {}
        for r in scan:
            for t in set(r['slots']):
                share[t] = share.get(t, 0) + 1
        docs = doc_rvas() if b8 == BUILD_ORDER[0] else set()
        for r in scan:
            why = []
            if r['name_ui']:
                why.append('name')
            b = next((x for x in r['bases'] if x in ui_names), None)
            if b:
                why.append('base:' + demangle(b)[0])
            r['doc_ui'] = (r['vtable'] in docs or any(t in docs and share[t] <= 8 for t in r['slots'])) and not demangle(r['mangled'])[1].endswith('Node')   # AI behaviour-tree nodes are not UI
            if r['doc_ui']:
                why.append('doc')
            r['reasons'] = why
    old, new = scans.get(BUILD_ORDER[0]), scans.get(BUILD_ORDER[1])
    if old and new:
        ko = keyed(old)
        for r in new:                      # carry the doc evidence to the new build by RTTI name
            o = ko.get((r['mangled'], r['offset'], r['ord']))
            if o is not None and o['doc_ui'] and 'doc' not in r['reasons']:
                r['reasons'].append('doc')


def ui_tables(con, folder8, rtti, other):
    """Create rtti_vtable / rtti_bases / ui_vtable_slots / build_map for one build (other may be None)."""
    con.execute('CREATE TABLE rtti_vtable (vtable INTEGER PRIMARY KEY, col INTEGER, col_offset INTEGER, ord INTEGER, mangled TEXT, class TEXT, outer TEXT, nslots INTEGER, is_ui INTEGER, ui_reason TEXT, other_vtable INTEGER, other_status TEXT)')
    con.execute('CREATE TABLE rtti_bases (vtable INTEGER, idx INTEGER, base TEXT)')
    con.execute('CREATE TABLE ui_vtable_slots (vtable INTEGER, slot INTEGER, func_rva INTEGER, PRIMARY KEY (vtable, slot))')
    con.execute('CREATE TABLE build_map (mangled TEXT, class TEXT, col_offset INTEGER, ord INTEGER, vt_a INTEGER, vt_b INTEGER, is_ui_a INTEGER, is_ui_b INTEGER, status TEXT)')
    mine = keyed(rtti)
    theirs = keyed(other) if other else {}
    rows = []
    for r in rtti:
        nm, outer, _ = demangle(r['mangled'])
        o = theirs.get((r['mangled'], r['offset'], r['ord']))
        status = ('' if not other else 'only_this' if o is None else 'same' if o['vtable'] == r['vtable'] else 'moved')
        rows.append((r['vtable'], r['col'], r['offset'], r['ord'], r['mangled'], nm, outer, len(r['slots']), 1 if r['reasons'] else 0, ','.join(r['reasons']), o['vtable'] if o else None, status))
    con.executemany('INSERT INTO rtti_vtable VALUES (?,?,?,?,?,?,?,?,?,?,?,?)', rows)
    con.executemany('INSERT INTO rtti_bases VALUES (?,?,?)', [(r['vtable'], i, b) for r in rtti for i, b in enumerate(r['bases'])])
    con.executemany('INSERT INTO ui_vtable_slots VALUES (?,?,?)', [(r['vtable'], i, s) for r in rtti for i, s in enumerate(r['slots'])])
    if other:
        a, b = (mine, theirs) if folder8 == BUILD_ORDER[0] else (theirs, mine)
        mrows = []
        for k in sorted(set(a) | set(b)):
            ra, rb = a.get(k), b.get(k)
            st = 'both_same_rva' if ra and rb and ra['vtable'] == rb['vtable'] else ('both' if ra and rb else ('old_only' if ra else 'new_only'))
            mrows.append((k[0], demangle(k[0])[0], k[1], k[2], ra['vtable'] if ra else None, rb['vtable'] if rb else None,
                          (1 if ra['reasons'] else 0) if ra else None, (1 if rb['reasons'] else 0) if rb else None, st))
        con.executemany('INSERT INTO build_map VALUES (?,?,?,?,?,?,?,?,?)', mrows)
    con.execute('CREATE INDEX ix_rtti_mangled ON rtti_vtable(mangled)')
    con.execute('CREATE INDEX ix_slots_func ON ui_vtable_slots(func_rva)')
    con.execute('CREATE INDEX ix_bases_base ON rtti_bases(base)')
    con.execute('CREATE INDEX ix_map_mangled ON build_map(mangled)')


# ---------------------------------------------------------------- build
def set_idle_priority():
    try:
        import ctypes
        k = ctypes.windll.kernel32
        k.SetPriorityClass(k.GetCurrentProcess(), 0x40)
    except Exception:
        pass


def build_index(path, rtti_cache, force=False, log=print):
    path = Path(path)
    sig = source_sig(path)
    ip = index_path(path)
    if not force:
        con = open_fresh(path)
        if con is not None:
            con.close()
            return 'fresh'
    t0 = time.time()
    tmp = path / (INDEX_NAME + '.new')
    for p in (tmp, Path(str(tmp) + '-journal')):
        if p.exists():
            p.unlink()
    con = sqlite3.connect(str(tmp))
    con.execute('PRAGMA journal_mode=OFF')
    con.execute('PRAGMA synchronous=OFF')
    con.execute('PRAGMA page_size=4096')
    fmt = {}
    anomalies = {}
    manifest = json.loads((path / 'manifest.json').read_text(encoding='utf-8'))
    for p in sorted(path.glob('*.tsv')):
        name = p.stem
        header, rows, bad = load_tsv(p)
        if header is None:
            continue
        if bad:
            anomalies[name] = bad
        extra = ()
        if name == 'functions':
            ext = function_extents(path, rows, header) if (path / 'c').is_dir() else [(None, None)] * len(rows)
            extra = (('off', [e[0] for e in ext]), ('len', [e[1] for e in ext]))
        create_table(con, fmt, name, header, rows, extra)
        if name == 'functions':
            if fmt['functions']['rva'] != 'x' or fmt['functions']['end'] != 'x':
                raise ValueError('functions rva not canonical')
            con.execute('CREATE INDEX ix_functions_rva ON functions(rva)')
        elif name == 'calls':
            con.execute('CREATE INDEX ix_calls_callee ON calls(callee_rva)')
            con.execute('CREATE INDEX ix_calls_caller ON calls(caller_func_rva)')
        elif name == 'datarefs':
            con.execute('CREATE INDEX ix_datarefs_to ON datarefs(to_rva)')
        elif name == 'vtables':
            con.execute('CREATE INDEX ix_vtables_vt ON vtables(vftable_rva)')
            con.execute('CREATE INDEX ix_vtables_target ON vtables(target_rva)')
        elif name == 'game_rvas':
            con.execute('CREATE INDEX ix_game_rvas ON game_rvas(game_rva)')
        elif name == 'symbols':
            con.execute('CREATE INDEX ix_symbols_rva ON symbols(rva)')
        del rows
    # hex literals of reference-mod bodies (gamerefs c-literal search)
    if path.parent.name == 'ghidra_refs' and (path / 'c').is_dir():
        build_hexlit(con, path)
    # RTTI / UI map for original builds
    if path.parent.name == 'ghidra_full' and path.name[:8] in BUILD_ORDER:
        me = path.name[:8]
        other8 = [b for b in BUILD_ORDER if b != me][0]
        mine = rtti_cache(me)
        if mine is not None:
            ui_tables(con, me, mine, rtti_cache(other8))
    con.execute('CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT)')
    meta = {'sig': sig, 'schema': str(SCHEMA), 'fmt': json.dumps(fmt), 'built_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
            'anomalies': json.dumps(anomalies), 'sha256': str(manifest.get('sha256', ''))}
    con.executemany('INSERT INTO meta VALUES (?,?)', meta.items())
    con.commit()
    con.execute('ANALYZE')
    con.commit()
    con.close()
    for attempt in range(10):
        try:
            os.replace(tmp, ip)
            break
        except PermissionError:
            if attempt == 9:
                raise
            time.sleep(0.5)
    log('  built %s %.1f MB in %.1fs' % (path.name, ip.stat().st_size / 1e6, time.time() - t0))
    return 'built'


HEXTOK = re.compile(rb'0[xX]([0-9a-fA-F]{4,})')


def build_hexlit(con, path):
    con.execute('CREATE TABLE hexlit (token TEXT, func_id INTEGER, line_off INTEGER, line_len INTEGER)')
    funcs = con.execute('SELECT id, shard, off, len FROM functions WHERE off IS NOT NULL ORDER BY id').fetchall()
    rows = []
    cache = {}
    for fid, shard, off, ln in funcs:
        data = cache.get(shard)
        if data is None:
            cache.clear()
            data = cache[shard] = (path / shard).read_bytes()
        body_end = off + ln
        seen = set()
        for m in HEXTOK.finditer(data, off, body_end):
            ls = data.rfind(b'\n', off, m.start()) + 1
            if ls < off:
                ls = off
            le = data.find(b'\n', m.start(), body_end)
            if le < 0:
                le = body_end
            key = (m.group(1).lower(), ls)
            if key in seen:
                continue
            seen.add(key)
            rows.append((key[0].decode(), fid, ls, le - ls))
    con.executemany('INSERT INTO hexlit VALUES (?,?,?,?)', rows)
    con.execute('CREATE INDEX ix_hexlit_token ON hexlit(token)')


def export_dirs():
    out = []
    for root in (FULL, REFS):
        if not root.is_dir():
            continue
        for d in sorted(root.iterdir()):
            mf = d / 'manifest.json'
            if d.is_dir() and mf.exists():
                try:
                    m = json.loads(mf.read_text(encoding='utf-8'))
                except ValueError:
                    continue
                if m.get('status') == 'complete':
                    out.append(d)
    return out


def main():
    import argparse
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--force', action='store_true')
    ap.add_argument('--check', action='store_true', help='only report which indexes are stale/missing')
    ap.add_argument('--only', default=None, help='name substring filter')
    a = ap.parse_args()
    set_idle_priority()
    dirs = [d for d in export_dirs() if not a.only or a.only.lower() in d.name.lower()]
    if a.check:
        for d in dirs:
            con = open_fresh(d)
            print(('fresh  ' if con else 'STALE  ') + d.name)
            if con:
                con.close()
        return 0
    cache = {}

    def rtti_cache(b8):
        if not cache:
            scans = {}
            for x in BUILD_ORDER:
                folder = next((d.name for d in FULL.iterdir() if d.name.startswith(x)), None)
                t = time.time()
                scan = build_rtti(x, folder) if folder else None
                if scan is not None:
                    scans[x] = scan
                    print('  rtti %s: %d vtables (%.1fs)' % (x, len(scan), time.time() - t))
            annotate(scans)
            cache.update(scans)
            cache['_done'] = True
        return cache.get(b8)
    # original builds first (they need both RTTI scans), then refs
    dirs.sort(key=lambda d: (d.parent.name != 'ghidra_full', d.name))
    rc = 0
    for d in dirs:
        print(d.name, flush=True)
        try:
            print(' ', build_index(d, rtti_cache, a.force))
        except Exception as e:  # keep going; fall back to TSV path for this export
            rc = 1
            print('  FAILED:', type(e).__name__, e, file=sys.stderr)
    return rc


# ---------------------------------------------------------------- query side
def addr(s, base=IMAGE_BASE):
    n = int(s, 16)
    return n - base if n >= base else n


LIKE_LIT = re.compile(r'[A-Za-z0-9_ :,/\-]+\Z')


class Index:
    """Query layer producing exactly the lines ghidra_full.query() prints."""

    def __init__(self, path, con):
        self.path = Path(path)
        self.con = con
        self.fmt = json.loads(con.execute("SELECT value FROM meta WHERE key='fmt'").fetchone()[0])
        self._names = None
        self._shard = None
        try:
            self.base = int(json.loads((self.path / 'manifest.json').read_text(encoding='utf-8')).get('image_base', '140000000'), 16)
        except (OSError, ValueError):
            self.base = IMAGE_BASE

    # formatting ---------------------------------------------------------
    def _fmt(self, table):
        f = self.fmt[table]
        cols = list(f)
        kinds = [f[c] for c in cols]
        return cols, kinds

    def _join(self, kinds, row):
        out = []
        for k, v in zip(kinds, row):
            if v is None:
                out.append('')
            elif k == 'x':
                out.append('%08X' % v)
            elif k == 'd':
                out.append(str(v))
            else:
                out.append(v)
        return '\t'.join(out)

    def _has(self, table):
        return table in self.fmt

    def _key(self, table, col, n):
        return n if self.fmt[table][col] != 't' else '%08X' % n

    def names(self):
        if self._names is None:
            self._names = {r[0]: r[1] for r in self.con.execute('SELECT rva, name FROM functions ORDER BY id')}
        return self._names

    def name_of(self, rva_text):
        if HEX8.match(rva_text or ''):
            return self.names().get(int(rva_text, 16), '')
        return ''

    # functions ---------------------------------------------------------
    def find_func(self, address):
        """-> dict(rva:str, name, shard, off, len) or None, same selection rule as ghidra_full.Root.find_func."""
        n = addr(address, self.base)
        r = self.con.execute('SELECT id, rva, "end", name, shard, off, len FROM functions WHERE rva<=? ORDER BY rva DESC, id DESC LIMIT 1', (n,)).fetchone()
        if r is None or not (r[1] <= n <= r[2]):
            r = self.con.execute('SELECT id, rva, "end", name, shard, off, len FROM functions WHERE rva<=? AND "end">=? ORDER BY id LIMIT 1', (n, n)).fetchone()
        if r is None:
            return None
        return {'id': r[0], 'rva': '%08X' % r[1], 'rva_n': r[1], 'name': r[3], 'shard': r[4], 'off': r[5], 'len': r[6]}

    def _read(self, shard, off, ln):
        with (self.path / shard).open('rb') as f:
            f.seek(off)
            raw = f.read(ln)
        return raw.decode('utf-8').replace('\r\n', '\n')

    def body(self, f):
        if f['off'] is None:
            return
        text = self._read(f['shard'], f['off'], f['len'])
        if text == '':
            return
        lines = text.split('\n')
        if text.endswith('\n'):
            lines.pop()
        yield from lines

    # grep ---------------------------------------------------------------
    def grep_c(self, value):
        pat = re.compile(value, re.I)
        pre = None
        if not re.search(r'\\A|\\Z|\(\?', value):
            try:
                pre = re.compile(value, re.I | re.M)
            except re.error:
                pre = None
        cur = self.con.execute('SELECT rva, name, shard, off, len FROM functions WHERE off IS NOT NULL ORDER BY id')
        hits = {}
        cache_shard, raw = None, None
        for rva, name, shard, off, ln in cur:
            if shard != cache_shard:
                cache_shard = shard
                raw = (self.path / shard).read_bytes()
                if shard not in hits:
                    hits[shard] = pre is None or pre.search(raw.decode('utf-8').replace('\r\n', '\n')) is not None
            if not hits[shard]:
                continue
            seg = raw[off:off + ln].decode('utf-8').replace('\r\n', '\n')
            if seg == '':
                continue
            lines = seg.split('\n')
            if seg.endswith('\n'):
                lines.pop()
            for line in lines:
                if pat.search(line):
                    yield '%08X' % rva + '\t' + name + '\t' + line

    def grep_table(self, table, value):
        pat = re.compile(value, re.I)
        cols, kinds = self._fmt(table)
        sel = ','.join(q(c) for c in cols)
        if table == 'symbols' and LIKE_LIT.match(value):
            rv = "printf('%08X',rva)" if self.fmt['symbols']['rva'] == 'x' else 'rva'
            hay = rv + "||char(9)||name||char(9)||kind"
            cur = self.con.execute("SELECT %s FROM symbols WHERE %s LIKE ? ESCAPE '\\' ORDER BY id" % (sel, hay), ('%' + value.replace('_', '\\_') + '%',))
        else:
            cur = self.con.execute('SELECT %s FROM %s ORDER BY id' % (sel, q(table)))
        for row in cur:
            line = self._join(kinds, row)
            if not pat.search(line):
                continue
            if table == 'strings':
                refs = self._get(cols, row, 'ref_functions') or ''
                refs = refs.split(',')
            else:
                refs = [self._join(['x' if kinds[cols.index('rva')] == 'x' else 't'], [self._get(cols, row, 'rva')])]
            for ref in refs:
                yield ref + '\t' + self.name_of(ref) + '\t' + line

    @staticmethod
    def _get(cols, row, col):
        return row[cols.index(col)]

    def list_table(self, command, value):
        table = {'str': 'strings', 'imports': 'imports', 'exports': 'exports'}[command]
        if not self._has(table):
            raise ValueError(table + '.tsv not available for this export')
        pat = re.compile(value or '.', re.I)
        cols, kinds = self._fmt(table)
        sel = ','.join(q(c) for c in cols)
        for row in self.con.execute('SELECT %s FROM %s ORDER BY id' % (sel, q(table))):
            line = self._join(kinds, row)
            if pat.search(line):
                if command == 'str':
                    refs = self._get(cols, row, 'ref_functions') or ''
                    line += '\t' + ','.join(r + ':' + self.name_of(r) for r in refs.split(',') if r)
                yield line

    # address queries -----------------------------------------------------
    def addr_query(self, command, value):
        n = addr(value, self.base)
        if command in ('callers', 'callees'):
            f = self.find_func(value)
            tn = f['rva_n'] if f else n
            col = 'callee_rva' if command == 'callers' else 'caller_func_rva'
            table = 'calls'
            ids = self.con.execute('SELECT * FROM calls WHERE "%s"=? ORDER BY id' % col, (self._key('calls', col, tn),))
        elif command == 'xref':
            table = 'datarefs'
            ids = self.con.execute('SELECT * FROM datarefs WHERE to_rva=? ORDER BY id', (self._key('datarefs', 'to_rva', n),))
        else:
            table = 'vtables'
            ids = self.con.execute('SELECT * FROM vtables WHERE id IN (SELECT id FROM vtables WHERE vftable_rva=? UNION SELECT id FROM vtables WHERE target_rva=?) ORDER BY id',
                                   (self._key('vtables', 'vftable_rva', n), self._key('vtables', 'target_rva', n)))
        _, kinds = self._fmt(table)
        for row in ids:
            yield self._join(kinds, row[1:])

    def query(self, command, value=None, scope='c'):
        if command == 'func':
            f = self.find_func(value)
            if f is None:
                raise ValueError('function not found in completed shards')
            yield from self.body(f)
        elif command == 'grep':
            if scope == 'c':
                yield from self.grep_c(value)
            else:
                yield from self.grep_table(scope, value)
        elif command in ('str', 'imports', 'exports'):
            yield from self.list_table(command, value)
        else:
            yield from self.addr_query(command, value)

    # gamerefs ----------------------------------------------------------
    def game_rvas(self, label, starts, target):
        if not self._has('game_rvas'):
            return
        cols, kinds = self._fmt('game_rvas')
        marks = ','.join('?' * len(starts))
        for row in self.con.execute('SELECT %s FROM game_rvas WHERE game_rva IN (%s) ORDER BY id' % (','.join(q(c) for c in cols), marks), tuple(self._key('game_rvas', 'game_rva', x) for x in starts)):
            d = {c: self._join([k], [v]) for c, k, v in zip(cols, kinds, row)}
            fr = d['func_rva']
            yield '[' + label + ']\t' + d['form'] + '\tloc=' + d['loc_rva'] + '\tfunc=' + (fr + ' ' + self.name_of(fr) if fr else '-') + '\t' + d['value'] + ('\t(function start)' if row[cols.index('game_rva')] != self._key('game_rvas', 'game_rva', target) else '')

    def c_literals(self, label, n):
        lit = re.compile(r'0x(?:14)?0*%x\b' % n, re.I)
        h = '%x' % n
        if len(h) < 4 or not self.con.execute("SELECT 1 FROM sqlite_master WHERE name='hexlit'").fetchone():
            for f in self._all_funcs():
                for line in self.body(f):
                    if lit.search(line):
                        yield '[' + label + ']\tc-literal\tfunc=' + f['rva'] + ' ' + f['name'] + '\t' + line.strip()[:160]
            return
        toks = [pre + '0' * z + h for pre in ('', '14') for z in range(0, 17)]
        cand = set(self.con.execute('SELECT func_id, line_off, line_len FROM hexlit WHERE token IN (%s)' % ','.join('?' * len(toks)), toks))
        for fid, lo, ll in sorted(cand):
            f = self.con.execute('SELECT rva, name, shard FROM functions WHERE id=?', (fid,)).fetchone()
            line = self._read(f[2], lo, ll).rstrip('\n')
            if lit.search(line):
                yield '[' + label + ']\tc-literal\tfunc=' + '%08X' % f[0] + ' ' + f[1] + '\t' + line.strip()[:160]

    def _all_funcs(self):
        for fid, rva, name, shard, off, ln in self.con.execute('SELECT id, rva, name, shard, off, len FROM functions ORDER BY id').fetchall():
            yield {'rva': '%08X' % rva, 'name': name, 'shard': shard, 'off': off, 'len': ln}

    # UI map ---------------------------------------------------------------
    def has_ui(self):
        return bool(self.con.execute("SELECT 1 FROM sqlite_master WHERE name='rtti_vtable'").fetchone())

    def uiclass(self, value):
        if not self.has_ui():
            raise ValueError('no UI map in this export (needs the analysis EXE; run ghidra_index.py)')
        pat = re.compile(value, re.I)
        for r in self.con.execute('SELECT vtable, col_offset, class, mangled, nslots, is_ui, ui_reason, other_vtable, other_status FROM rtti_vtable ORDER BY vtable'):
            if pat.search(r[2]) or pat.search(r[3]):
                yield '\t'.join(['%08X' % r[0], 'off=%d' % r[1], r[2], 'slots=%d' % r[4], 'ui=' + (r[6] if r[5] else '-'), 'other=' + ('%08X' % r[7] if r[7] is not None else '-') + ('(%s)' % r[8] if r[8] else '')])

    def uimap(self, value):
        """Old build (a) <-> new build (b) vtable correspondence by RTTI name."""
        if not self.has_ui() or not self.con.execute('SELECT 1 FROM build_map LIMIT 1').fetchone():
            raise ValueError('no build map in this export (needs both analysis EXEs; run ghidra_index.py)')
        pat = re.compile(value, re.I)
        yield '# class\toffset\told(%s)\tnew(%s)\tui_old\tui_new\tstatus' % BUILD_ORDER
        for r in self.con.execute('SELECT mangled, class, col_offset, vt_a, vt_b, is_ui_a, is_ui_b, status FROM build_map ORDER BY class, col_offset, ord'):
            if pat.search(r[1]) or pat.search(r[0]):
                f = lambda v: '%08X' % v if v is not None else '-'
                yield '\t'.join([r[1], 'off=%d' % r[2], f(r[3]), f(r[4]), '-' if r[5] is None else str(r[5]), '-' if r[6] is None else str(r[6]), r[7]])

    def uislot(self, value):
        if not self.has_ui():
            raise ValueError('no UI map in this export (needs the analysis EXE; run ghidra_index.py)')
        n = addr(value, self.base)
        v = self.con.execute('SELECT vtable, class, is_ui, ui_reason FROM rtti_vtable WHERE vtable=?', (n,)).fetchone()
        if v:
            yield '# vtable %08X %s ui=%s' % (v[0], v[1], v[3] if v[2] else '-')
            for slot, fr in self.con.execute('SELECT slot, func_rva FROM ui_vtable_slots WHERE vtable=? ORDER BY slot', (n,)):
                yield '%d\t%08X\t%s' % (slot, fr, self.name_of('%08X' % fr))
            return
        rows = self.con.execute('SELECT s.vtable, s.slot, v.class, v.is_ui, v.ui_reason FROM ui_vtable_slots s JOIN rtti_vtable v ON v.vtable=s.vtable WHERE s.func_rva=? ORDER BY s.vtable, s.slot', (n,)).fetchall()
        if not rows:
            raise ValueError('%08X is neither an RTTI vtable nor a vtable slot function' % n)
        yield '# function %08X %s' % (n, self.name_of('%08X' % n))
        for vt, slot, cls, ui, why in rows:
            yield '%08X\tslot=%d\t%s\tui=%s' % (vt, slot, cls, why if ui else '-')


def open_index(path):
    con = open_fresh(path)
    return Index(path, con) if con is not None else None


if __name__ == '__main__':
    sys.exit(main())
