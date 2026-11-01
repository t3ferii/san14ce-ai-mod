"""Read the original RDOK descriptors; never execute scratchpad scripts.

Usage: python -B -X utf8 terrain_foam_extract.py <kids_39d346.bin> <terr.bin>
The caller supplies evidence paths so a different session's scratchpad is usable.
"""
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

KIDS_SHA256 = "022dc7c2dd45203b432533c3a7da8dbcc0c2da7251578e4ed6950649d5bde5a9"
ROW_HEIGHT = 810.5997779422346


def body_fingerprint(path):
    value = 2166136261
    for byte in Path(path).read_bytes()[28:]:
        value = ((value ^ byte) * 16777619) & 0xFFFFFFFF
    return value


def cpp_table(records):
    def floats(values):
        return "{" + ",".join(f"{x:.17e}f" for x in values) + "}"
    return "\n".join("    {0x%08Xu,%uu,%s,%s,%s}," %
                     (r["id"], r["owner"], floats(r["pos"]), floats(r["quat"]), floats(r["scale"])) for r in records)


def pool_bytes(path):
    """User/Claude 2026-10-29: retain 171 originals and append 64 checked clones."""
    data = Path(path).read_bytes()
    assert hashlib.sha256(data).hexdigest() == KIDS_SHA256
    ids, offset = set(), 28
    for _ in range(171):
        size, key = struct.unpack_from("<II", data, offset + 8)
        assert key not in ids and key < 0xFFFFFF00
        ids.add(key)
        offset += (size + 3) & ~3
    assert offset == len(data) == 20820
    clone = data[0x94:0x94 + 120]
    assert struct.unpack_from("<II", clone, 8) == (120, 0x00EC810E)
    output = bytearray(data)
    struct.pack_into("<I", output, 16, 235)
    struct.pack_into("<I", output, 24, 28500)
    for i in range(64):
        key = 0xFFFFFF00 + i
        assert key not in ids
        ids.add(key)
        record = bytearray(clone)
        struct.pack_into("<I", record, 12, key)
        struct.pack_into("<3f", record, 0x68, 0.001, 0.001, 0.001)
        output.extend(record)
    assert len(output) == 28500
    return bytes(output)


def center(hex_id):
    row, col = divmod(hex_id, 220)
    return 37594.0 + col * 702.0, 13335.3 + row * ROW_HEIGHT + (ROW_HEIGHT / 2 if col % 2 == 0 else 0)


def extract(path):
    data = Path(path).read_bytes()
    assert hashlib.sha256(data).hexdigest() == KIDS_SHA256, "KIDS fingerprint mismatch"
    assert data[:8] == b"_DOK0000"
    header, version, count, _, total = struct.unpack_from("<5I", data, 8)
    assert (header, version, count, total) == (28, 10, 171, len(data))
    offset, records, ids = header, [], set()
    for _ in range(count):
        assert data[offset:offset + 8] == b"RDOK0000"
        size, object_id, cls, resource, properties = struct.unpack_from("<5I", data, offset + 8)
        assert object_id not in ids
        ids.add(object_id)
        assert offset + size <= total
        if resource == 0x05641EE9:
            assert cls == 0xB7EFA6B7 and size in (104, 120)
            values, payload = {}, offset + 28 + properties * 12
            for index in range(properties):
                kind, n, key = struct.unpack_from("<3I", data, offset + 28 + index * 12)
                assert kind in (4, 8), (kind, hex(key))
                values[key] = struct.unpack_from(f"<{n}{'f' if kind == 8 else 'I'}", data, payload)
                payload += 4 * n
            assert payload == offset + size
            pos = values[0x27D889E5]
            quat = values.get(0xF52EA910, (0.0, 0.0, 0.0, 1.0))
            scale = values.get(0xFEA9E4F8, (1.0, 1.0, 1.0))
            assert len(pos) == 3 and len(quat) == 4 and len(scale) == 3
            assert all(math.isfinite(x) for x in (*pos, *quat, *scale))
            assert all(x > 0 for x in scale)
            owner = min(range(48400), key=lambda h: ((center(h)[0] - pos[0]) ** 2 + (center(h)[1] - pos[2]) ** 2, h))
            records.append(dict(id=object_id, offset=offset, size=size, pos=pos, quat=quat, scale=scale, owner=owner))
        offset += (size + 3) & ~3
    assert offset == len(data) and len(records) == 35
    assert sum(r["size"] == 120 for r in records) == 28
    return records


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("kids", type=Path)
    parser.add_argument("terrain", type=Path)
    parser.add_argument("--cpp", action="store_true", help="emit the reproducible C++ source table")
    args = parser.parse_args()
    records = extract(args.kids)
    if args.cpp:
        print(cpp_table(records))
        print(f"// BodyFingerprint=0x{body_fingerprint(args.kids):08X}u")
        return
    terrain = args.terrain.read_bytes()
    assert len(terrain) == 48400, "terrain byte count"
    for record in records:
        record["terrain"] = terrain[record["owner"]]
    print(json.dumps(dict(foam_count=len(records), rapid_owners=sum(r["terrain"] == 24 for r in records),
                         terrain_sha256=hashlib.sha256(terrain).hexdigest(), records=records), ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
