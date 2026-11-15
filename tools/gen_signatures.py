#!/usr/bin/env python
"""Address table generator, single reference column (docs/PATCH_RESILIENCE.md; W2 of docs/MODULARIZATION_PLAN.md).

  python -X utf8 tools/gen_signatures.py analyze --ref current --target <sha8|path> [--out DIR]   # values of the target build, per id
  python -X utf8 tools/gen_signatures.py analyze --ref current --target current                     # self test: every id must stay as it is
  python -X utf8 tools/gen_signatures.py addid <hex rva> [...] [--apply]                           # new ids for the reference table

The reference build is the one the table is named by (ids GA_<RVA of the reference build>, src/game_addresses_data.inl, one column). The
target build is the next Steam build (an unpacked EXE, see tools/pelib.py). For every id the existing resolution method of the table is run
on the target image (a Python mirror of the runtime resolver in src/game_addresses.h); an id the methods cannot find gets a new method
derived from the reference image (a signature / mid pattern / data reference that is unique in BOTH images), and a table-only id is
accepted at its old address when the function bytes there did not change. Every id then gets a status:
  OK         same address, function body identical (modulo relocations)
  AUTO       found at another address, function body identical (modulo relocations and switch jump tables)
  ATTENTION  found, but the function around it changed (or a data id moved): a human decides, the report names file:line
  BLOCKED    not found: ids a source uses are unresolved and the dependent feature stays off (src/game_addresses_gates.inl)
  DENIED     the reference column denies the id (value 0)
Resolution methods (src/game_addresses.h implements the same algorithms):
  rtti    vtable slot address = vtable(RTTI class name, COL offset) + 8*slot
  slotfn  function = pointer stored in another (rtti) slot
  sig     function start: masked byte prefix (rel32 / RIP disp / reloc wildcards), unique among function candidates
  mid     address inside a resolved function: masked pattern, unique inside that function window (+ delta)
  dataref global data = RIP-relative displacement read from a resolved function at a fixed offset
  table   per-build constants only (known builds); unknown builds leave the id unresolved
Standard library only. Writes only below build/development/patch_next (and src/game_addresses_data.inl for `addid --apply`)."""
import sys, os, re, json, struct, bisect, time, pickle, zlib, argparse
from array import array
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pelib as P                                                # noqa: E402
import ga_table as G                                             # noqa: E402

ROOT = P.ROOT
INL = os.path.join(ROOT, 'src', 'game_addresses_data.inl')
CACHE_DIR = os.path.join(ROOT, 'build', 'development', 'patch_next', 'cache')
PREF_BASE = P.BASE
PFX = 128                      # bytes kept per function candidate for signature matching
SIG_LENS = (8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 64, 80, 96, 128)
K_FUNC, K_MID, K_SLOT, K_DATA = 0, 1, 2, 3
M_TABLE, M_SIG, M_MID, M_RTTI, M_SLOTFN, M_DATAREF = 0, 1, 2, 3, 4, 5
SRC_EXT = ('.h', '.cpp', '.inl')


def log(*a):
    print(*a, file=sys.stderr, flush=True)


# ---------------------------------------------------------------- RTTI
class Rtti:
    def __init__(self, b):
        pe, d = b.pe, b.pe.d
        self.td = {}                      # name -> [td_rva]
        self.td_name = {}
        for sec in pe.sections:
            if sec[0] in ('.data', '.rdata'):
                n, va, vs, ro, rs = sec
                blob = d[ro:ro + rs]
                i = 0
                while True:
                    i = blob.find(b'.?A', i)
                    if i < 0:
                        break
                    e = blob.find(b'\0', i)
                    nm = blob[i:e]
                    if len(nm) > 6 and nm.endswith(b'@@') and nm[3:4] in (b'V', b'U') and va + i >= 16:
                        s = nm.decode('latin1')
                        self.td.setdefault(s, []).append(va + i - 16)
                        self.td_name[va + i - 16] = s
                    i = e
        n, va, vs, ro, rs = pe.sec['.rdata']
        arr = array('I')
        blob = d[ro:ro + (rs & ~3)]
        arr.frombytes(blob)
        tdset = self.td_name
        self.cols = {}                    # col_rva -> (td_rva, offset)
        for i, v in enumerate(arr):
            if v in tdset and i >= 3:
                col = va + (i - 3) * 4
                o = col - va
                if o + 24 <= len(blob):
                    sig, off, cd, tdr, cls, selfr = struct.unpack_from('<IIIIII', blob, o)
                    if sig == 1 and tdr == v and selfr == col:
                        self.cols[col] = (tdr, off)
        qarr = array('Q')
        qarr.frombytes(d[ro:ro + (rs & ~7)])
        colva = {PREF_BASE + c: c for c in self.cols}
        self.vt_of_col = defaultdict(list)
        for i, v in enumerate(qarr):
            if v in colva:
                self.vt_of_col[colva[v]].append(va + i * 8 + 8)
        text_lo, text_hi = PREF_BASE + b.text_va, PREF_BASE + b.text_va + b.text_size
        self.vts = []
        for col, vl in self.vt_of_col.items():
            for vt in vl:
                o = vt - va
                k = 0
                while o + 8 * (k + 1) <= len(blob):
                    q, = struct.unpack_from('<Q', blob, o + 8 * k)
                    if not (text_lo <= q < text_hi):
                        break
                    k += 1
                self.vts.append((vt, vt + 8 * k, col))
        self.vts.sort()
        self.vt_starts = [v[0] for v in self.vts]

    def locate(self, v):
        """(class name, col offset, slot) when v is a slot address inside an RTTI vtable (or its start)."""
        k = bisect.bisect_right(self.vt_starts, v) - 1
        if k < 0:
            return None
        s, e, col = self.vts[k]
        if v < e and (v - s) % 8 == 0:
            tdr, off = self.cols[col]
            return self.td_name[tdr], off, (v - s) // 8
        return None

    def resolve(self, name, off, slot):
        tds = self.td.get(name)
        if not tds or len(tds) != 1:
            return None
        tdr = tds[0]
        cols = [c for c, (t, o) in self.cols.items() if t == tdr and o == off]
        if len(cols) != 1:
            return None
        vl = self.vt_of_col.get(cols[0], [])
        if len(vl) != 1:
            return None
        for s, e, col in self.vts:
            if s == vl[0]:
                if slot < 0 or s + 8 * slot >= e:
                    return None
                return s + 8 * slot
        return None


def load_rtti(b):
    """RTTI index of an image, cached under build/development/patch_next/cache (keyed by the EXE sha256)."""
    path = os.path.join(CACHE_DIR, 'rtti_%s.pkl' % b.sha[:16])
    if os.path.exists(path):
        try:
            return pickle.load(open(path, 'rb'))
        except Exception:                                        # noqa: BLE001  stale / truncated cache: rebuild
            pass
    r = Rtti(b)
    os.makedirs(CACHE_DIR, exist_ok=True)
    pickle.dump(r, open(path + '.tmp', 'wb'))
    os.replace(path + '.tmp', path)
    return r


# ---------------------------------------------------------------- per image index
class BI:
    """one image + candidate / prefix indexes + RTTI."""

    def __init__(self, b):
        self.b = b
        self.sha = b.sha
        self.cands = b.candidates()
        self.cset = set(self.cands)
        self.text = b.text
        self.tva = b.text_va
        self.ints = None
        self.bucket = None
        self.rtti = load_rtti(b)

    def index(self):
        if self.ints is not None:
            return
        t, va = self.text, self.tva
        self.bucket = defaultdict(list)
        self.ints = {}
        for c in self.cands:
            o = c - va
            chunk = t[o:o + PFX]
            if len(chunk) < PFX:
                chunk = chunk + bytes(PFX - len(chunk))
            self.ints[c] = int.from_bytes(chunk, 'big')
            self.bucket[(chunk[0], chunk[1])].append(c)

    def sig_matches(self, pat, mk):
        """candidate starts whose raw bytes equal pat at every literal position (len <= PFX)."""
        self.index()
        L = len(pat)
        sh = 8 * (PFX - L)
        Pv = M = 0
        for i in range(L):
            Pv = (Pv << 8) | (0 if mk[i] else pat[i])
            M = (M << 8) | (0 if mk[i] else 0xFF)
        return [c for c in self.bucket.get((pat[0], pat[1]), ()) if ((self.ints[c] >> sh) & M) == Pv]

    def container(self, rva):
        k = bisect.bisect_right(self.cands, rva) - 1
        return self.cands[k] if k >= 0 else None

    def next_cand(self, rva):
        k = bisect.bisect_right(self.cands, rva)
        return self.cands[k] if k < len(self.cands) else self.tva + len(self.text)

    def rd32(self, rva):
        r = self.b.pe.read(rva, 4)
        return None if r is None else struct.unpack('<i', r)[0]

    def rd64(self, rva):
        r = self.b.pe.read(rva, 8)
        return None if r is None else struct.unpack('<Q', r)[0]


def pat_regex(pat, mk):
    return re.compile(b'(?=' + b''.join(b'.' if mk[i] else re.escape(bytes([bt])) for i, bt in enumerate(pat)) + b')', re.S)


def window_matches(bi, start, size, pat, mk):
    o = start - bi.tva
    w = bi.text[o:o + size]
    return [start + m.start() for m in pat_regex(pat, mk).finditer(w)]


def same_modulo_jumptables(ref, tgt, r0, t0):
    """masked function bodies equal; differing unmasked 4-byte words must be function-relative switch table entries (moved together)."""
    er, et = ref.next_cand(r0), tgt.next_cand(t0)
    size = er - r0
    if et - t0 != size or size > 0x8000:
        return False
    rr, mr = ref.b.masked(r0, size)
    rt, mt = tgt.b.masked(t0, size)
    if mr != mt:
        return False
    covered = set()
    for p in (i for i in range(size) if not mr[i] and rr[i] != rt[i]):
        if p in covered:
            continue
        for w in range(max(0, p - 3), p + 1):
            if w + 4 > size:
                continue
            vo, vn = int.from_bytes(rr[w:w + 4], 'little'), int.from_bytes(rt[w:w + 4], 'little')
            if 0 <= vo - r0 < size and vo - r0 == vn - t0:
                covered.update(range(w, w + 4))
                break
        else:
            return False
    return True


# ---------------------------------------------------------------- the analysis
class Analysis:
    """Table T (single reference column) against a target image. self.res[i] = dict(tv, status, note, derived)."""

    def __init__(self, ref, tgt, T, derive=True):
        self.ref_name = T.builds[0][0]
        self.T = T
        self.same_image = ref.sha == tgt.sha
        t0 = time.time()
        self.rb = BI(ref)
        self.tb = self.rb if self.same_image else BI(tgt)
        log('images: ref %s (%d vtables), target %s (%d vtables) %.1fs' % (ref.name, len(self.rb.rtti.vts), tgt.name, len(self.tb.rtti.vts), time.time() - t0))
        self.derive = derive
        self.rva_idx = {e.rva: i for i, e in enumerate(T.entries)}
        self.memo = {}
        self.res = [dict(tv=0, status='', note='', derived=False) for _ in T.entries]
        self.aux = 0

    # -- mirror of the runtime resolver
    def value(self, bi, i):
        key = (id(bi), i)
        if key in self.memo:
            return self.memo[key]
        self.memo[key] = None                        # cycle guard
        v = self._value(bi, i)
        self.memo[key] = v
        return v

    def _value(self, bi, i):
        e = self.T.entries[i]
        m = e.method
        if m == M_SIG:
            h = bi.sig_matches(e.pat, e.mask)
            return h[0] if len(h) == 1 else None
        if m == M_RTTI:
            return bi.rtti.resolve(e.rname, e.c, e.b)
        if m == M_SLOTFN:
            s = self.value(bi, e.a)
            if s is None:
                return None
            c = bi.rd64(s)
            return c - PREF_BASE if c and bi.tva <= c - PREF_BASE < bi.tva + len(bi.text) else None
        if m == M_MID:
            c = self.value(bi, e.a)
            if c is None:
                return None
            h = window_matches(bi, c, e.c, e.pat, e.mask)
            return h[0] + e.d if len(h) == 1 else None
        if m == M_DATAREF:
            c = self.value(bi, e.a)
            if c is None:
                return None
            d = bi.rd32(c + e.b)
            if d is None:
                return None
            v = c + e.c + d
            return v if 0 <= v < bi.b.image_size else None
        return None

    def invalidate(self, i):
        for key in [k for k in self.memo if k[1] == i]:
            del self.memo[key]

    # -- derivation of new methods (unique in the reference image and, when different, in the target image)
    def pick_sig(self, rva):
        rb, tb = self.rb, self.tb
        raw, mk = rb.b.masked(rva, PFX)
        if mk[0] or mk[1]:
            return None
        for L in SIG_LENS:
            ho = rb.sig_matches(raw[:L], mk[:L])
            if len(ho) == 1 and ho[0] == rva:
                hn = ho if self.same_image else tb.sig_matches(raw[:L], mk[:L])
                if len(hn) == 1:
                    return raw[:L], mk[:L], hn[0]
                if len(hn) == 0:
                    return None
            elif len(ho) == 0:
                return None
        return None

    def set_sig(self, i):
        e = self.T.entries[i]
        r = self.pick_sig(e.rva)
        if not r:
            return False
        raw, mk, tv = r
        e.method, e.pat, e.mask, e.len, e.a, e.b, e.c, e.d = M_SIG, raw, mk, len(raw), 0, 0, 0, 0
        self.invalidate(i)
        return True

    def add_aux(self, rva):
        e = G.Entry()
        e.name = 'GA_%X' % rva
        e.rva, e.kind, e.method, e.len, e.a, e.b, e.c, e.d = rva, K_FUNC, M_TABLE, 0, 0, 0, 0, 0
        e.vals = {self.ref_name: rva}
        e.wild = self.wild_of(rva)
        self.T.entries.append(e)
        self.rva_idx[rva] = len(self.T.entries) - 1
        self.res.append(dict(tv=0, status='', note='container of a derived pattern', derived=True))
        self.aux += 1
        return len(self.T.entries) - 1

    def wild_of(self, rva):
        c = self.rb.container(rva)
        pos = rva - c
        _, mk = self.rb.b.masked(c, pos + 40)
        return sum(1 << k for k in range(32) if pos + k < len(mk) and mk[pos + k])

    def container_entry(self, c_old):
        """index of a func entry for c_old that the runtime resolver finds in the target (signature / slot pointer), creating a container id."""
        i = self.rva_idx.get(c_old)
        if i is None:
            if c_old not in self.rb.cset:
                return None
            i = self.add_aux(c_old)
        e = self.T.entries[i]
        if e.kind != K_FUNC:
            return None
        if e.method in (M_SIG, M_SLOTFN) and self.value(self.tb, i):
            return i
        if e.method == M_TABLE or not self.value(self.tb, i):
            if self.set_sig(i) and self.value(self.tb, i):
                self.res[i]['derived'] = True
                self.res[i]['tv'] = self.value(self.tb, i)
                return i
        return None

    def derive_mid(self, i):
        e = self.T.entries[i]
        rb = self.rb
        c_old = rb.container(e.rva)
        ci = self.container_entry(c_old)
        if ci is None:
            return False
        c_new = self.value(self.tb, ci)
        if c_new is None:
            return False
        size_old = rb.next_cand(c_old) - c_old
        win = min(max(size_old * 2 + 256, 512), 0x8000)
        pos = e.rva - c_old
        raw_all, mk_all = rb.b.masked(c_old, pos + 72)
        for L in (6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48):
            for pre in (0, 2, 4, 6, 8, 12, 16):
                if pre > pos or pre >= L:
                    continue
                a = pos - pre
                pat, mk = raw_all[a:a + L], mk_all[a:a + L]
                if len(pat) < L or sum(1 for x in mk if not x) < min(L - 2, 8) or mk[pre]:
                    continue
                ho = window_matches(rb, c_old, win, pat, mk)
                if len(ho) != 1 or ho[0] + pre != e.rva:
                    continue
                hn = window_matches(self.tb, c_new, win, pat, mk)
                if len(hn) != 1:
                    continue
                e.method, e.pat, e.mask, e.len, e.a, e.b, e.c, e.d = M_MID, pat, mk, L, ci, 0, win, pre
                self.invalidate(i)
                return True
        return False

    def code_refs(self, targets):
        """{data rva: [(function, field offset, end offset)]} of RIP-relative references in the reference code (cached per image)."""
        path = os.path.join(CACHE_DIR, 'refs_%s.pkl' % self.rb.sha[:16])
        refs = None
        if os.path.exists(path):
            try:
                refs = pickle.load(open(path, 'rb'))
            except Exception:                                    # noqa: BLE001
                refs = None
        if refs is None:
            t0 = time.time()
            rb = self.rb
            text, va, rel, cands = rb.text, rb.tva, rb.b.rel(), rb.cands
            refs = defaultdict(list)
            for ci, c in enumerate(cands):
                end = cands[ci + 1] if ci + 1 < len(cands) else va + len(text)
                o = c - va
                mk, rips, br, err = P.decode_mk(bytes(text[o:end - va]), c, rel[o:end - va])
                for tgt, field, endo in rips:
                    refs[tgt].append((c, field, endo))
            os.makedirs(CACHE_DIR, exist_ok=True)
            pickle.dump(dict(refs), open(path + '.tmp', 'wb'))
            os.replace(path + '.tmp', path)
            log('data reference scan %.1fs' % (time.time() - t0))
        return refs

    def derive_data(self, idxs):
        refs = self.code_refs({self.T.entries[i].rva for i in idxs})
        n = 0
        for i in idxs:
            e = self.T.entries[i]
            got = []
            for (c, field, endo) in refs.get(e.rva, [])[:40]:
                ci = self.container_entry(c)
                if ci is None:
                    continue
                cn = self.value(self.tb, ci)
                if cn is None:
                    continue
                ro = self.rb.b.masked(c, endo + 4)
                rn = self.tb.b.masked(cn, endo + 4)
                a = max(0, field - 3)
                if ro[0][a:field] != rn[0][a:field]:
                    continue
                d = self.tb.rd32(cn + field)
                if d is None:
                    continue
                tn = cn + endo + d
                if self.tb.b.sec_of(tn) != self.rb.b.sec_of(e.rva):
                    continue
                got.append((ci, field, endo, tn))
                if len(got) >= 3:
                    break
            if not got:
                continue
            tns = Counter(g[3] for g in got)
            if len(tns) > 1:
                self.res[i]['note'] = 'data references disagree: %s' % ' '.join('%X' % x for x in tns)
                continue
            ci, field, endo, tn = got[0]
            e.method, e.len, e.a, e.b, e.c, e.d = M_DATAREF, 0, ci, field, endo, 0
            self.invalidate(i)
            n += 1
        return n

    # -- table-only ids: accepted at the same address when the bytes there are unchanged
    def same_address(self, i):
        e = self.T.entries[i]
        rb, tb = self.rb, self.tb
        if self.same_image:
            return True
        if e.kind in (K_FUNC, K_MID):
            c = rb.container(e.rva)
            if tb.container(e.rva) != c:
                return False
            return same_modulo_jumptables(rb, tb, c, c)
        # data without a method: only an identical, non-trivial 64-byte neighbourhood counts (zero blocks are equal in every build)
        a, b = rb.b.raw(e.rva, 64), tb.b.raw(e.rva, 64)
        return a is not None and a == b and sum(1 for x in a if x) >= 24 and rb.b.sec_of(e.rva) == tb.b.sec_of(e.rva)

    def order_outliers(self):
        """ids of function/mid kind whose target values are not part of the longest increasing run (by reference RVA): moved out of order."""
        rows = sorted((e.rva, i) for i, e in enumerate(self.T.entries) if e.kind in (K_FUNC, K_MID) and self.res[i]['tv'] and e.vals[self.ref_name])
        tails, idx, prev = [], [], [-1] * len(rows)
        for k, (_, i) in enumerate(rows):
            t = self.res[i]['tv']
            p = bisect.bisect_left(tails, t)
            if p == len(tails):
                tails.append(t)
                idx.append(k)
            else:
                tails[p], idx[p] = t, k
            prev[k] = idx[p - 1] if p else -1
        keep = set()
        k = idx[-1] if idx else -1
        while k >= 0:
            keep.add(rows[k][1])
            k = prev[k]
        return {i for _, i in rows} - keep

    def run(self):
        T, rb, tb = self.T, self.rb, self.tb
        n0 = len(T.entries)
        t0 = time.time()
        for i in range(n0):
            e, r = T.entries[i], self.res[i]
            if not e.vals[self.ref_name]:
                r['status'] = 'DENIED'
                continue
            if e.method != M_TABLE:
                vr = self.value(rb, i)
                if vr != e.rva:
                    r['note'] = 'method does not reproduce the reference value (%s != %X)' % ('none' if vr is None else '%X' % vr, e.rva)
            r['tv'] = (self.value(tb, i) or 0) if e.method != M_TABLE else (e.rva if self.same_address(i) else 0)
        log('existing methods: %d of %d ids found in the target %.1fs' % (sum(1 for k in range(n0) if self.res[k]['tv']), n0, time.time() - t0))
        if self.derive:
            self.derive_missing(n0)
        outliers = self.order_outliers()
        for i in range(len(T.entries)):
            self.classify(i)
            r = self.res[i]
            if r['derived'] and i in outliers and r['status'] in ('OK', 'AUTO'):
                r['status'] = 'ATTENTION'
                r['note'] = (r['note'] + '; ' if r['note'] else '') + 'new pattern matched out of order relative to the other ids (possible twin function)'
        return self.res

    def derive_missing(self, n0):
        T = self.T
        miss = [i for i in range(n0) if not self.res[i]['tv'] and T.entries[i].vals[self.ref_name]]
        if not miss:
            return
        t0 = time.time()
        for i in miss:
            e = T.entries[i]
            if e.kind == K_FUNC:
                self.res[i]['derived'] = self.set_sig(i)
            elif e.kind == K_MID:
                self.res[i]['derived'] = self.derive_mid(i)
        data = [i for i in miss if T.entries[i].kind == K_DATA]
        if data:
            self.derive_data(data)
            for i in data:
                self.res[i]['derived'] = T.entries[i].method == M_DATAREF
        for i in miss:
            if self.res[i]['derived']:
                v = self.value(self.tb, i)
                self.res[i]['tv'] = v or 0
                self.res[i]['note'] = ('%s; ' % self.res[i]['note'] if self.res[i]['note'] else '') + 'new %s derived from the reference image' % G.METHOD[T.entries[i].method]
        log('derived methods: %d of %d missing ids resolved %.1fs (%d container ids added)' % (sum(1 for i in miss if self.res[i]['tv']), len(miss), time.time() - t0, self.aux))

    def classify(self, i):
        e, r = self.T.entries[i], self.res[i]
        if r['status'] == 'DENIED':
            return
        tv = r['tv']
        if not tv:
            r['status'] = 'BLOCKED'
            r['note'] = r['note'] or 'not found in the target image'
            return
        if self.same_image and tv == e.rva:
            r['status'] = 'OK'
            return
        if e.method == M_TABLE:
            # no pattern to prove the location (non-unique function / plain data): the same address with equal bytes may be a twin
            r['status'] = 'ATTENTION'
            r['note'] = (r['note'] + '; ' if r['note'] else '') + 'table-only id kept at its address: bytes there equal, nothing proves it is the same function'
            return
        if e.kind in (K_FUNC, K_MID):
            cr = self.rb.container(e.rva)
            ct = self.tb.container(tv)
            same = same_modulo_jumptables(self.rb, self.tb, cr, ct)
            if not same:
                r['status'] = 'ATTENTION'
                r['note'] = (r['note'] + '; ' if r['note'] else '') + 'function body around %X changed in the target (%X)' % (e.rva, ct)
                return
            if e.kind == K_MID and tv - ct != e.rva - cr:
                r['status'] = 'ATTENTION'
                r['note'] = (r['note'] + '; ' if r['note'] else '') + 'offset inside the function moved (%+d)' % ((tv - ct) - (e.rva - cr))
                return
        r['status'] = 'OK' if tv == e.rva else 'AUTO'


# ---------------------------------------------------------------- sources: ids -> file:line
def source_lines(root=ROOT):
    """{'GA_<hex>': [(file, line)]} of every mention in src/ (comments included: a mention is a review hint)."""
    out = defaultdict(list)
    sd = os.path.join(root, 'src')
    for fn in sorted(os.listdir(sd)):
        if fn.startswith('game_addresses') or os.path.splitext(fn)[1] not in SRC_EXT:
            continue
        for ln, line in enumerate(open(os.path.join(sd, fn), encoding='utf-8', errors='replace'), 1):
            for m in re.finditer(r'\bGA_([0-9A-F]{3,8})\b', line):
                out['GA_' + m.group(1)].append((fn, ln))
    return out


# ---------------------------------------------------------------- guard hashes
FNV_OFF, FNV_PRIME, M64 = 0xcbf29ce484222325, 0x100000001b3, (1 << 64) - 1
GH_RE = re.compile(r'GH\(\s*0[xX]([0-9A-Fa-f]+)[uU][lL]{2}\s*\)')


def fnv(data):
    h = FNV_OFF
    for x in data:
        h = ((h ^ x) * FNV_PRIME) & M64
    return h


def guard_hash_pairs(A, root=ROOT):
    """[(reference hash, hash of the same range at the target value or 0)] for every GH(...) literal of src/ that is the FNV-1a of a code range at a code id."""
    lits = set()
    sd = os.path.join(root, 'src')
    for fn in os.listdir(sd):
        if os.path.splitext(fn)[1] in SRC_EXT and not fn.startswith('game_addresses'):
            for m in GH_RE.finditer(open(os.path.join(sd, fn), encoding='utf-8', errors='replace').read()):
                lits.add(int(m.group(1), 16))
    found = defaultdict(set)
    ot, nt = A.rb.text, A.tb.text
    for i, e in enumerate(A.T.entries):
        if e.kind not in (K_FUNC, K_MID) or not e.vals[A.ref_name]:
            continue
        o = e.rva - A.rb.tva
        h = FNV_OFF
        nv = A.res[i]['tv']
        for size in range(1, 0x3200):
            if o + size > len(ot):
                break
            h = ((h ^ ot[o + size - 1]) * FNV_PRIME) & M64
            if h in lits:
                nh = fnv(nt[nv - A.tb.tva:nv - A.tb.tva + size]) if nv and (nv - A.tb.tva) + size <= len(nt) else 0
                found[h].add(nh)
    return sorted([h, next(iter(s)) if len(s) == 1 else 0] for h, s in found.items())


# ---------------------------------------------------------------- driver
def load_table():
    T = G.parse(INL)
    if len(T.builds) != 1:
        raise SystemExit('%s must carry one build column (found %d)' % (INL, len(T.builds)))
    return T


def to_json(A, ref, tgt):
    rows = []
    src = source_lines()
    for i, e in enumerate(A.T.entries):
        r = A.res[i]
        rows.append(dict(id=e.name, rva='%X' % e.rva, kind=G.KIND[e.kind], method=G.METHOD[e.method], target='%X' % r['tv'], status=r['status'],
                         derived=r['derived'], note=r['note'], users=sorted(e.users), uses=['%s:%d' % x for x in src.get(e.name, [])[:6]]))
    return dict(meta=dict(ref=dict(build=ref.name, sha256=ref.sha, timestamp=ref.timestamp, image_size=ref.image_size),
                          target=dict(build=tgt.name, sha256=tgt.sha, timestamp=tgt.timestamp, image_size=tgt.image_size),
                          counts=dict(Counter(r['status'] for r in A.res)), ids=len(rows), aux_ids=A.aux), entries=rows)


def cmd_analyze(a):
    ref, tgt = P.open_build(a.ref), P.open_build(a.target)
    T = load_table()
    if T.builds[0][0] != ref.name:
        raise SystemExit('the table column is %s but --ref is %s' % (T.builds[0][0], ref.name))
    A = Analysis(ref, tgt, T, derive=not a.no_derive)
    A.run()
    doc = to_json(A, ref, tgt)
    out = a.out or os.path.join(ROOT, 'build', 'development', 'patch_next', tgt.name)
    os.makedirs(out, exist_ok=True)
    json.dump(doc, open(os.path.join(out, 'addr_table.json'), 'w', encoding='utf-8'), indent=0)
    print(json.dumps(doc['meta']['counts']), 'ids', doc['meta']['ids'], '->', os.path.join(out, 'addr_table.json'))
    return A


def cmd_addid(a):
    """new ids for the reference table: kind and method derived from the reference image only (no target)."""
    ref = P.open_build(a.ref)
    T = load_table()
    A = Analysis(ref, ref, T, derive=False)
    A.derive = True
    have = {e.rva for e in T.entries}
    new = []
    for s in a.rvas:
        rva = int(s, 16)
        if rva in have:
            print('GA_%X exists' % rva)
            continue
        sec = ref.sec_of(rva)
        e = G.Entry()
        e.name, e.rva, e.method, e.len, e.a, e.b, e.c, e.d = 'GA_%X' % rva, rva, M_TABLE, 0, 0, 0, 0, 0
        e.vals = {A.ref_name: rva}
        if sec == '.text':
            e.kind = K_FUNC if rva in A.rb.cset else K_MID
            e.wild = A.wild_of(rva)
        else:
            e.kind = K_SLOT if A.rb.rtti.locate(rva) else K_DATA
        T.entries.append(e)
        A.rva_idx[rva] = len(T.entries) - 1
        A.res.append(dict(tv=rva, status='', note='', derived=True))
        i = len(T.entries) - 1
        if e.kind == K_SLOT:
            nm, off, slot = A.rb.rtti.locate(rva)
            e.method, e.rname, e.b, e.c = M_RTTI, nm, slot, off
            if A.value(A.rb, i) != rva:
                e.method = M_TABLE
        elif e.kind == K_FUNC:
            A.set_sig(i)
        elif e.kind == K_MID:
            A.derive_mid(i)
        elif e.kind == K_DATA and A.derive_data([i]) and A.value(A.rb, i) != rva:
            e.method = M_TABLE
        new.append(i)
        print('GA_%X: %s via %s' % (rva, G.KIND[e.kind], G.METHOD[e.method]))
    src = source_lines()
    for i in new:
        e = T.entries[i]
        e.users = {f for f, _ in src.get(e.name, [])}
        for f in sorted(e.users):   # 2026-11-12: a source file that uses address ids for the first time joins the file table (appended, indices kept)
            if f not in T.files:
                T.files.append(f)
    if a.apply and new:
        hdr = open(INL, encoding='utf-8').read().split('enum GaId')[0].rstrip('\n').split('\n')
        open(INL, 'w', encoding='utf-8', newline='\n').write(G.emit(T, hdr))
        print('wrote', INL, '(%d ids added; rebuild the fixtures and run tools/check_addresses.py)' % len(new))
    elif new:
        print('dry run: pass --apply to write src/game_addresses_data.inl')
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('analyze')
    p.add_argument('--ref', default='current', help='reference build (sha8, "current" or an unpacked EXE path)')
    p.add_argument('--target', required=True, help='target build (sha8 or an unpacked EXE path)')
    p.add_argument('--out')
    p.add_argument('--no-derive', action='store_true', help='only run the existing methods (no new patterns)')
    p = sub.add_parser('addid')
    p.add_argument('rvas', nargs='+')
    p.add_argument('--ref', default='current')
    p.add_argument('--apply', action='store_true')
    a = ap.parse_args()
    if a.cmd == 'analyze':
        cmd_analyze(a)
        return 0
    return cmd_addid(a)


if __name__ == '__main__':
    sys.exit(main())
