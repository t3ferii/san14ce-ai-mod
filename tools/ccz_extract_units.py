"""조조전·신조조전(Ekd5) 부대 형상을 모드 자산 assets/ccz/ccz_units.bin(CCZU v1)으로 뽑는다. 원본은 읽기만 한다.

사용: python -I -X utf8 tools/ccz_extract_units.py [시작칸=0] [끝칸=240] [--src=신조조전] [--generals]
  --src: 게임 폴더 아래 원본 폴더(기본 조조전). 2026-10-31 사용자 선택은 신조조전(병종 190, 칸 0~569).
  --generals: data/ccz_generals.tsv 의 장수 그림 칸(720 + 형상*3 + 2)도 넣는다(2026-11-01 사용자).

CCZU v2 (리틀엔디언). v2 = v1의 zlib 대신 0-연속 RLE(DLL에 zlib가 없음, 2026-10-31)
  0  'CCZU', u32 버전(2), u32 항목 수 N, u32 표 오프셋
 16  팔레트 768B (Spalet 0번, RGB)
784  팀색 표시 256B (1 = 엔진이 진영색으로 바꾸는 녹색·청록 자리)
 표  N × (u16 칸, u16 0, u32 오프셋, u32 압축 크기, u32 원래 크기)
 RLE: 바이트 b < 0x80 이면 다음 b+1 바이트를 그대로, b >= 0x80 이면 0을 b-0x7F개(1..128)
 각 항목 = RLE(이동 11×48×48 | 공격 12×64×64 | 특수 5×48×48), 8비트 색 번호, 0 = 투명
프레임: 이동 0-1 앞 걸음, 2-3 뒤, 4-5 옆(왼쪽), 6-8 앞·뒤·옆 서기, 9-10 약함.
        공격 0-3 앞, 4-7 뒤, 8-11 옆(왼쪽). 특수 0-2 방어(앞·뒤·옆), 3 피격, 4 승리.
"""
import colorsys, hashlib, json, os, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'research', 'cao_cao_den'))
import ls12  # noqa: E402
from ls12 import load, unpack  # noqa: E402

OUT = os.path.join(ROOT, 'assets', 'ccz')
SIZES = (('Unit_mov.e5', 48, 11), ('Unit_atk.e5', 64, 12), ('Unit_spc.e5', 48, 5))
RAW = sum(s * s * n for _, s, n in SIZES)


def team_mask(pal):
    m = bytearray(256)
    for i in range(1, 256):
        h, s, v = colorsys.rgb_to_hsv(*[x / 255 for x in pal[i * 3:i * 3 + 3]])
        m[i] = 1 if s > 0.25 and 0.22 < h < 0.55 else 0
    return m


def rle(raw):
    out = bytearray(); i = 0; n = len(raw)
    while i < n:
        if raw[i] == 0:
            j = i
            while j < n and raw[j] == 0 and j - i < 128: j += 1
            out.append(0x7F + j - i); i = j
        else:
            j = i
            while j < n and raw[j] != 0 and j - i < 128: j += 1
            out.append(j - i - 1); out += raw[i:j]; i = j
    return bytes(out)


def unrle(z, size):
    out = bytearray(); i = 0
    while i < len(z):
        b = z[i]; i += 1
        if b < 0x80: out += z[i:i + b + 1]; i += b + 1
        else: out += bytes(b - 0x7F)
    assert len(out) == size
    return bytes(out)


def palette_name():
    return 'Spalet.e5' if os.path.exists(os.path.join(ls12.G, 'Spalet.e5')) else r'E5\Spalet.e5'


def general_cells():
    path = os.path.join(ROOT, 'data', 'ccz_generals.tsv')
    rows = [l.split('	') for l in open(path, encoding='utf-8').read().splitlines() if l and not l.startswith('#')]
    return sorted({int(r[1]) for r in rows})


def build(cells):
    pd, pe = load(palette_name()); pal = unpack(pd, *pe[0])
    files = [load(f) for f, _, _ in SIZES]
    blobs = []
    for c in cells:
        raw = b''.join(unpack(d, *e[c]) for d, e in files)
        assert len(raw) == RAW, (c, len(raw))
        blobs.append((c, rle(raw)))
    hdr = 16 + 768 + 256; table = hdr; off = table + 16 * len(blobs)
    out = bytearray(struct.pack('<4s3I', b'CCZU', 2, len(blobs), table) + pal + team_mask(pal))
    body = bytearray()
    for c, z in blobs:
        out += struct.pack('<HHIII', c, 0, off + len(body), len(z), RAW); body += z
    return bytes(out + body)


def read(b):
    magic, ver, n, table = struct.unpack_from('<4s3I', b)
    assert magic == b'CCZU' and ver == 2
    pal = b[16:784]; mask = b[784:1040]; items = {}
    for k in range(n):
        c, _, o, cz, u = struct.unpack_from('<HHIII', b, table + 16 * k)
        items[c] = (o, cz, u)
    return pal, mask, items


def frames(b, items, c):
    o, cz, u = items[c]; raw = unrle(b[o:o + cz], u)
    out = []; p = 0
    for _, s, n in SIZES:
        out.append([raw[p + k * s * s:p + (k + 1) * s * s] for k in range(n)]); p += s * s * n
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    lo = int(args[0]) if args else 0; hi = int(args[1]) if len(args) > 1 else 240
    src = next((a[6:] for a in sys.argv[1:] if a.startswith('--src=')), None)
    if src: ls12.G = os.path.join(os.path.dirname(ROOT), src)
    os.makedirs(OUT, exist_ok=True)
    generals = general_cells() if '--generals' in sys.argv else []
    data = build(sorted(set(range(lo, hi)) | set(generals)))
    path = os.path.join(OUT, 'ccz_units.bin'); open(path, 'wb').write(data)
    # 되읽기 검사: 모든 항목이 원본 해제 결과와 바이트 단위로 같은지
    pal, mask, items = read(data); files = [load(f) for f, _, _ in SIZES]
    for c in items:
        fr = frames(data, items, c)
        ref = b''.join(unpack(d, *e[c]) for d, e in files)
        assert b''.join(b''.join(x) for x in fr) == ref, c
    hashes = {f: hashlib.sha256(open(os.path.join(ls12.G, f), 'rb').read()).hexdigest()
              for f in [f for f, _, _ in SIZES] + [palette_name(), 'Ekd5.exe']}
    meta = {'format': 'CCZU v2', 'cells': [lo, hi], 'general_cells': generals, 'count': len(items), 'bytes': len(data),
            'sha256': hashlib.sha256(data).hexdigest(), 'team_indices': [i for i in range(256) if mask[i]],
            'source': os.path.basename(ls12.G) + ' (Ekd5 엔진) Unit_mov/atk/spc.e5, ' + palette_name(), 'source_sha256': hashes}
    json.dump(meta, open(os.path.join(OUT, 'ccz_units.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print(len(items), 'cells', len(data), 'bytes, roundtrip ok ->', path)


if __name__ == '__main__':
    main()
