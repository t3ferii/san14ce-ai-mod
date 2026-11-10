#!/usr/bin/env python
"""PE reader, masking x86-64 decoder and build registry for the patch tools (docs/PATCH_RESILIENCE.md, W2).

 - PE: sections, .pdata functions, relocations of an unpacked game EXE (read only)
 - decode_mk(): length decoder that returns the wildcard map of a code range (rel32 branches, RIP displacements, absolute disp32 in the
   image range, relocation entries) plus the RIP-relative data references; the same masks as the table column ga_wild
 - Build: one image (.text bytes, function-start candidates in the definition of src/game_addresses.h, masked prefixes)
 - image registry: a build is named by the first 8 hex digits of its EXE sha256 (e.g. 1a4a447d) or given as a path
Replaces the decode part of the retired tools/rva_remap.py. Standard library only."""
import os, sys, struct, bisect, hashlib, glob
from array import array

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)                                   # AI_TacticsMod (or a worktree of it)
BASE = 0x140000000
CURRENT_BUILD = '1a4a447d'                                      # reference build of the address table (docs/PATCH_RESILIENCE.md)


# ---------------------------------------------------------------- PE
class PE:
    def __init__(self, path):
        self.path = path
        d = self.d = open(path, 'rb').read()
        e = struct.unpack_from('<I', d, 0x3c)[0]
        ns, = struct.unpack_from('<H', d, e + 6)
        so, = struct.unpack_from('<H', d, e + 20)
        ob = e + 24
        self.size_of_image, = struct.unpack_from('<I', d, ob + 56)
        dd = ob + 112
        self.dirs = [struct.unpack_from('<II', d, dd + 8 * i) for i in range(16)]
        self.sections = []
        for i in range(ns):
            s = ob + so + 40 * i
            name = d[s:s + 8].rstrip(b'\0').decode('latin1')
            vs, va, rs, ro = struct.unpack_from('<IIII', d, s + 8)
            self.sections.append((name, va, vs, ro, rs))
        self.sec = {s[0]: s for s in self.sections}
        self.sha256 = hashlib.sha256(d).hexdigest()

    def off(self, rva):
        for n, va, vs, ro, rs in self.sections:
            if va <= rva < va + max(vs, rs):
                o = rva - va
                return ro + o if o < rs else None
        return None

    def read(self, rva, n):
        """raw bytes at rva (zero for the bss part); None when the range leaves the mapped sections."""
        out = bytearray()
        while n > 0:
            sec = None
            for s in self.sections:
                if s[1] <= rva < s[1] + max(s[2], s[4]):
                    sec = s
                    break
            if sec is None:
                return None
            n_, va, vs, ro, rs = sec
            o = rva - va
            take = min(n, va + max(vs, rs) - rva)
            if o < rs:
                t2 = min(take, rs - o)
                out += self.d[ro + o:ro + o + t2]
                out += bytes(take - t2)
            else:
                out += bytes(take)
            rva += take
            n -= take
        return bytes(out)

    def sec_of(self, rva):
        for s in self.sections:
            if s[1] <= rva < s[1] + max(s[2], s[4]):
                return s[0]
        return None

    def functions(self):
        n, va, vs, ro, rs = self.sec['.pdata']
        cnt = min(vs, rs) // 12
        out = []
        for i in range(cnt):
            b, e, u = struct.unpack_from('<III', self.d, ro + 12 * i)
            if b:
                out.append((b, e))
        out.sort()
        res = []
        for f in out:
            if not res or res[-1][0] != f[0]:
                res.append(f)
        return res

    def relocs(self):
        rva0, sz = self.dirs[5]
        p = self.off(rva0)
        d = self.d
        end = p + sz
        out = []
        while p + 8 <= end:
            page, bsz = struct.unpack_from('<II', d, p)
            if bsz < 8:
                break
            for k in range((bsz - 8) // 2):
                w, = struct.unpack_from('<H', d, p + 8 + 2 * k)
                t = w >> 12
                if t == 10:
                    out.append((page + (w & 0xFFF), 8))
                elif t == 3:
                    out.append((page + (w & 0xFFF), 4))
            p += bsz
        return out



# ---------------------------------------------------------------- x86-64 length decoder
M = [0] * 256
I = [0] * 256        # 1 imm8, 2 immz, 3 imm16, 4 rel8, 5 rel32, 6 moffs, 7 mov imm, 8 enter
BAD = set()
for b in range(0x40):
    lo = b & 7
    if lo < 4:
        M[b] = 1
    elif lo == 4:
        I[b] = 1
    elif lo == 5:
        I[b] = 2
    else:
        BAD.add(b)
BAD.discard(0x0F)
M[0x63] = 1
I[0x68] = 2; M[0x69] = 1; I[0x69] = 2; I[0x6A] = 1; M[0x6B] = 1; I[0x6B] = 1
for b in range(0x70, 0x80): I[b] = 4
M[0x80] = 1; I[0x80] = 1; M[0x81] = 1; I[0x81] = 2; M[0x83] = 1; I[0x83] = 1
for b in range(0x84, 0x90): M[b] = 1
for b in range(0xA0, 0xA4): I[b] = 6
I[0xA8] = 1; I[0xA9] = 2
for b in range(0xB0, 0xB8): I[b] = 1
for b in range(0xB8, 0xC0): I[b] = 7
M[0xC0] = 1; I[0xC0] = 1; M[0xC1] = 1; I[0xC1] = 1; I[0xC2] = 3
M[0xC6] = 1; I[0xC6] = 1; M[0xC7] = 1; I[0xC7] = 2; I[0xC8] = 8; I[0xCA] = 3; I[0xCD] = 1
for b in range(0xD0, 0xD4): M[b] = 1
for b in range(0xD8, 0xE0): M[b] = 1
for b in range(0xE0, 0xE4): I[b] = 4
for b in range(0xE4, 0xE8): I[b] = 1
I[0xE8] = 5; I[0xE9] = 5; I[0xEB] = 4
M[0xF6] = 1; M[0xF7] = 1; M[0xFE] = 1; M[0xFF] = 1
for b in (0x60, 0x61, 0x82, 0x9A, 0xCE, 0xD4, 0xD5, 0xD6, 0xEA):
    BAD.add(b)
M0 = [1] * 256
I0 = [0] * 256
for b in (0x05, 0x06, 0x07, 0x08, 0x09, 0x0B, 0x0E, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x37, 0x77, 0xA0, 0xA1, 0xA2, 0xA8, 0xA9, 0xAA):
    M0[b] = 0
for b in range(0xC8, 0xD0): M0[b] = 0
for b in range(0x80, 0x90): M0[b] = 0; I0[b] = 5
for b in (0x70, 0x71, 0x72, 0x73, 0xA4, 0xAC, 0xBA, 0xC2, 0xC4, 0xC5, 0xC6, 0x0F): I0[b] = 1
VEXIMM = {0x70, 0x71, 0x72, 0x73, 0xC2, 0xC4, 0xC5, 0xC6}
PFX = {0x66, 0x67, 0xF2, 0xF3, 0xF0, 0x2E, 0x36, 0x3E, 0x26, 0x64, 0x65}
ABS_LO, ABS_HI = 0x1000000, 0x2A00000




def decode_mk(buf, base, rm=None):
    """(wildcard map bytearray, [(rip target, field offset, end offset)], rel32 branch targets, error count) of linear code at rva `base`.
    `rm` = optional relocation map (1 = byte belongs to a relocation entry)."""
    n = len(buf)
    mk = bytearray(n)
    rips = []
    br = array('l')
    err = 0
    i = 0
    frm = int.from_bytes
    while i < n:
        opsz = False
        b = buf[i]
        while b in PFX:
            if b == 0x66:
                opsz = True
            i += 1
            if i >= n:
                break
            b = buf[i]
        if i >= n:
            break
        rex = 0
        if 0x40 <= b <= 0x4F:
            rex = b
            i += 1
            if i >= n:
                break
            b = buf[i]
        modrm = 0
        imm = 0
        grp3 = False
        if b == 0x0F:
            i += 1
            if i >= n:
                break
            b2 = buf[i]
            i += 1
            if b2 == 0x38:
                i += 1; modrm = 1
            elif b2 == 0x3A:
                i += 1; modrm = 1; imm = 1
            else:
                modrm = M0[b2]; imm = I0[b2]
        elif b == 0xC5:
            if i + 2 >= n:
                break
            op = buf[i + 2]
            i += 3; modrm = 1
            if op in VEXIMM:
                imm = 1
        elif b == 0xC4:
            if i + 3 >= n:
                break
            mm = buf[i + 1] & 0x1F
            op = buf[i + 3]
            i += 4; modrm = 1
            if mm == 3 or (mm == 1 and op in VEXIMM):
                imm = 1
        elif b == 0x62:
            if i + 4 >= n:
                break
            mm = buf[i + 1] & 7
            op = buf[i + 4]
            i += 5; modrm = 1
            if mm == 3 or (mm == 1 and op in VEXIMM):
                imm = 1
        else:
            i += 1
            modrm = M[b]; imm = I[b]
            if b in BAD:
                err += 1
            if b == 0xF6 or b == 0xF7:
                grp3 = True
        rfield = -1
        afield = -1
        if modrm:
            if i >= n:
                break
            mr = buf[i]
            i += 1
            mod = mr >> 6
            r = mr & 7
            if mod != 3:
                sibbase = -1
                if r == 4:
                    if i >= n:
                        break
                    sibbase = buf[i] & 7
                    i += 1
                if mod == 0:
                    if r == 5:
                        rfield = i; i += 4
                    elif sibbase == 5:
                        afield = i; i += 4
                elif mod == 1:
                    i += 1
                else:
                    afield = i; i += 4
            if grp3 and ((mr >> 3) & 7) < 2:
                imm = 1 if b == 0xF6 else 2
        efield = -1
        if imm:
            if imm == 1 or imm == 4:
                i += 1
            elif imm == 2:
                i += 2 if opsz else 4
            elif imm == 3:
                i += 2
            elif imm == 5:
                efield = i; i += 4
            elif imm == 6:
                i += 8
            elif imm == 7:
                i += 8 if rex & 8 else (2 if opsz else 4)
            elif imm == 8:
                i += 3
        if i > n:
            err += 1
            i = n
        if rfield >= 0 and rfield + 4 <= n:
            disp = frm(buf[rfield:rfield + 4], 'little', signed=True)
            rips.append((base + i + disp, rfield, i))
            mk[rfield:rfield + 4] = b'\1\1\1\1'
        if afield >= 0 and afield + 4 <= n:
            v = frm(buf[afield:afield + 4], 'little')
            if ABS_LO <= v <= ABS_HI:
                mk[afield:afield + 4] = b'\1\1\1\1'
        if efield >= 0 and efield + 4 <= n:
            disp = frm(buf[efield:efield + 4], 'little', signed=True)
            br.append(base + i + disp)
            mk[efield:efield + 4] = b'\1\1\1\1'
    if rm is not None:
        k = rm.find(1)
        while k >= 0:
            mk[k] = 1
            k = rm.find(1, k + 1)
    return mk, rips, br, err


# ---------------------------------------------------------------- one image
class Build:
    """one unpacked build: sections, function-start candidates and masked prefixes."""

    def __init__(self, path):
        self.path = path
        self.pe = PE(path)
        pe = self.pe
        self.sha = pe.sha256
        self.name = self.sha[:8]
        n, va, vs, ro, rs = pe.sec['.text']
        self.text_va, self.text_size = va, rs
        self.text = pe.d[ro:ro + rs]
        self.relmask = None
        e = struct.unpack_from('<I', pe.d, 0x3c)[0]
        self.timestamp, = struct.unpack_from('<I', pe.d, e + 8)
        self.image_size = pe.size_of_image
        self._cands = None

    def rel(self):
        if self.relmask is None:
            rm = bytearray(self.text_size)
            for r, ln in self.pe.relocs():
                o = r - self.text_va
                if 0 <= o < self.text_size:
                    for k in range(ln):
                        if o + k < self.text_size:
                            rm[o + k] = 1
            self.relmask = rm
        return self.relmask

    def raw(self, rva, n):
        return self.pe.read(rva, n)

    def candidates(self):
        """function-start candidates: .pdata begin addresses + 16-aligned starts after 0xCC padding (src/game_addresses.h candidates())."""
        if self._cands is None:
            pd = set(b for b, e in self.pe.functions())
            t = self.text
            va = self.text_va
            c = set(pd)
            i = 0
            n = len(t)
            while True:
                i = t.find(b'\xcc', i)
                if i < 0:
                    break
                j = i
                while j < n and t[j] == 0xCC:
                    j += 1
                if j < n and ((va + j) & 15) == 0 and j - i >= 1:
                    c.add(va + j)
                i = j + 1
            self._cands = sorted(x for x in c if va <= x < va + n)
        return self._cands

    def masked(self, rva, n):
        """(raw bytes, wildcard map) of the n bytes at a code rva (decoded as linear code)."""
        o = rva - self.text_va
        buf = self.text[o:o + n + 24]
        rm = self.rel()[o:o + n + 24]
        mk, rips, br, err = decode_mk(bytes(buf), rva, rm)
        return bytes(buf[:n]), bytes(mk[:n])

    def func_end(self, rva):
        c = self.candidates()
        k = bisect.bisect_right(c, rva)
        return c[k] if k < len(c) else self.text_va + self.text_size

    def sec_of(self, rva):
        return self.pe.sec_of(rva)


# ---------------------------------------------------------------- registry
def header(path):
    """read-only PE header summary of an EXE: sha256, TimeDateStamp, SizeOfImage."""
    d = open(path, 'rb').read()
    e = struct.unpack_from('<I', d, 0x3C)[0]
    return dict(path=path, size=len(d), sha256=hashlib.sha256(d).hexdigest(), time_date_stamp=struct.unpack_from('<I', d, e + 8)[0],
                size_of_image=struct.unpack_from('<I', d, e + 24 + 56)[0])


def known_images():
    """{sha8: path} of every unpacked image below build/ui_native_analysis (the analysis copies the user unpacks with Steamless)."""
    out = {}
    for p in sorted(glob.glob(os.path.join(ROOT, 'build', 'ui_native_analysis', '*', '*.unpacked.exe'))):
        out[hashlib.sha256(open(p, 'rb').read()).hexdigest()[:8]] = p
    return out


def image_path(name_or_path):
    """a build name (sha8, or 'current') or a path to an unpacked EXE -> path."""
    if os.path.isfile(name_or_path):
        return name_or_path
    imgs = known_images()
    key = CURRENT_BUILD if name_or_path == 'current' else name_or_path.lower()
    if key in imgs:
        return imgs[key]
    raise SystemExit('no unpacked image for build %r (known: %s); pass a path to the unpacked EXE' % (name_or_path, ', '.join(sorted(imgs)) or 'none'))


_builds = {}


def open_build(name_or_path):
    p = image_path(name_or_path)
    if p not in _builds:
        _builds[p] = Build(p)
    return _builds[p]
