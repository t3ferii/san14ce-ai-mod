#!/usr/bin/env python
"""Fixture image of the current Steam build (docs/MODULARIZATION_PLAN.md 1-B).

  python -X utf8 tools/make_fixture_image.py [--exe <unpacked exe>] [--out <dir>]

Writes <out>/<sha8>_text.bin (the virtual .text of the unpacked EXE, the same layout as the former
old-build text dump: byte k = RVA 0x1000+k) and <out>/<sha8>_text.json
(build, sha256 of the EXE, TimeDateStamp, text rva/size, SizeOfImage). Default: build/ui_native_analysis/editor83_b25953386
-> build/ui_native_analysis/fixture/1a4a447d_text.bin. Reads the unpacked EXE only. Standard library only."""
import sys, os, json, struct, hashlib, argparse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', default=os.path.join(ROOT, 'build', 'ui_native_analysis', 'editor83_b25953386', 'game.exe.unpacked.exe'))
    ap.add_argument('--out', default=os.path.join(ROOT, 'build', 'ui_native_analysis', 'fixture'))
    a = ap.parse_args()
    d = open(a.exe, 'rb').read()
    sha = hashlib.sha256(d).hexdigest()
    e = struct.unpack_from('<I', d, 0x3C)[0]
    n = struct.unpack_from('<H', d, e + 6)[0]
    so = struct.unpack_from('<H', d, e + 20)[0]
    ts = struct.unpack_from('<I', d, e + 8)[0]
    size_of_image = struct.unpack_from('<I', d, e + 24 + 56)[0]
    text = None
    for i in range(n):
        o = e + 24 + so + i * 40
        if d[o:o + 8].rstrip(b'\0') == b'.text':
            vs, va, rs, ro = struct.unpack_from('<IIII', d, o + 8)
            text = (va, vs, ro, rs)
    if not text:
        raise SystemExit('no .text section')
    va, vs, ro, rs = text
    blob = d[ro:ro + min(rs, vs)]
    blob += bytes(vs - len(blob))
    name = sha[:8]
    os.makedirs(a.out, exist_ok=True)
    open(os.path.join(a.out, name + '_text.bin'), 'wb').write(blob)
    json.dump(dict(build=name, exe_sha256=sha, time_date_stamp=ts, text_rva=va, text_size=vs, size_of_image=size_of_image,
                   source=os.path.basename(os.path.dirname(a.exe))), open(os.path.join(a.out, name + '_text.json'), 'w'), indent=1)
    print('wrote', os.path.join(a.out, name + '_text.bin'), len(blob))


if __name__ == '__main__':
    main()
