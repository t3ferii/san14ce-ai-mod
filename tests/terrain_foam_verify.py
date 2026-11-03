"""Independent file evidence and diagnostic-only integration checks."""
import argparse
import hashlib
import json
import os
import re
import struct
from pathlib import Path
from terrain_foam_extract import body_fingerprint, cpp_table, extract

ROOT = Path(__file__).resolve().parents[1]


def evidence_dir():
    override = os.environ.get("TERRAIN_FOAM_EVIDENCE_DIR")
    if override:
        return Path(override)
    base = Path(os.environ["TEMP"]) / "claude" / "C--Program-Files--x86--Steam-steamapps-common-Romance-of-the-Three-Kingdoms-14"
    matches = [p.parent for p in base.glob("*/scratchpad/kids_39d346.bin") if (p.parent / "terr.bin").is_file()]
    assert len(matches) == 1, "set TERRAIN_FOAM_EVIDENCE_DIR to the supplied scratchpad"
    return matches[0]


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--paths", action="store_true")
    parser.add_argument("--release-dll", type=Path)
    args = parser.parse_args()
    evidence = evidence_dir()
    if args.paths:
        print(evidence / "kids_39d346.bin")
        return
    source = (ROOT / "src/native_terrain_foam.inl").read_text(encoding="utf-8")
    records = extract(evidence / "kids_39d346.bin")
    assert cpp_table(records) in source, "all 35 position/quaternion/scale constants match original float bits"
    assert f"BodyFingerprint=0x{body_fingerprint(evidence / 'kids_39d346.bin'):08X}u" in source
    terrain = (evidence / "terr.bin").read_bytes()
    assert len(terrain) == 48400
    rapid = sum(terrain[r["owner"]] == 24 for r in records)
    # Independent nearest-center implementation, including exact ties and map edges.
    for record in records:
        best = min(((record["pos"][0] - (37594 + c * 702)) ** 2 +
                    (record["pos"][2] - (13335.3 + (r + (0.5 if c % 2 == 0 else 0)) * 810.5997779422346)) ** 2,
                    r * 220 + c) for r in range(220) for c in range(220))[1]
        assert best == record["owner"]
    # User 2026-10-30: release feature (terrain_edit); no diagnostic-only guard, no reserve pool.
    assert "#pragma once\n#ifndef SPLIT_COMBAT\n#define SAN14_TERRAIN_FOAM_INCLUDED" in source and "NO_RUNTIME_LOG" not in source
    assert not any(word in source for word in ("B30F70", "B34350", "B31250", "early_parser", "foam_test", "PoolCount", "bcrypt"))
    # User 2026-11-02: eliminate the 222-call zoom probe burst, preserving fault/identity guards.
    assert "VirtualQuery" not in re.sub(r"//[^\n]*", "", source)
    assert "capture_fields(handle,fields)" in source and "__except(EXCEPTION_EXECUTE_HANDLER)" in source
    assert "pointer(p,0)!=image+0x16DA140" in source
    for name in ("time_update", "render"):
        body = re.search(r"static uint64_t __fastcall " + name + r"\([^;]+?\) \{([^}]+)\}", source).group(1)
        # User 2026-11-02: timing scopes may surround the unchanged observe -> native call.
        # Strip only the explicit accounting statements; retain ABI/order/one-call assertions.
        body = re.sub(r"\s*TERRAIN_SCOPE\(\w+\);", "", body)
        assert body.strip().startswith("if(watch)observe(a,")
        assert body.count("fn<Callback>") == 1
        assert "find_locked" not in body and "readable" not in body
    mark = source.split("static void mark(", 1)[1].split("static void before_world_change", 1)[0]
    assert not any(word in mark for word in ("effects[", "pointer(", "fn<", "find_locked", "readable("))
    # User/Claude 2026-10-29: startup effects survive save loads; E18280 alone
    # invalidates their identities. World boundaries may change numeric jobs only.
    boundary = source.split("static void before_world_change()", 1)[1].split("static int find_locked", 1)[0]
    assert not any(word in boundary for word in ("memset(effects", "e.object=", "e.handle=", "e.ready=", "e.time_seen=", "e.render_seen=", "any_time=false", "pointer(", "fn<"))
    assert "update_watch_locked();" in boundary and "e.job=e.restore_job" in boundary
    assert "!transition && !restoration_pending_locked()" in source
    adapter = (ROOT / "src/native_adapter.cpp").read_text(encoding="utf-8")
    assert '#include "native_terrain_foam.inl"\n#if !defined(SPLIT_COMBAT)\n#include "native_terrain_trees.inl"' in adapter
    split = (ROOT / "src/native_split_install.inl").read_text(encoding="utf-8")
    for call in ("terrain_foam::install();", "terrain_foam::restore();"):
        assert "#ifdef SAN14_TERRAIN_FOAM_INCLUDED\n    " + call + "\n#endif" in split
    edit = (ROOT / "src/native_terrain_edit.inl").read_text(encoding="utf-8")
    assert edit.count("terrain_foam::mark(") == edit.count("terrain_render::mark(") == 4
    assert edit.count("terrain_foam::before_world_change();") == 2
    # Current unpacked EXE is read only. PE section translation is independent
    # of the static fixture and checks both pointer qwords and instruction bytes.
    exe = Path(r"C:/Users/<user>/Documents/Codex/analysis/SAN14CE_KO/1fbe6be7466a6646/input/game.exe.unpacked.exe").read_bytes()
    assert hashlib.sha256(exe).hexdigest() == "1fbe6be7466a66466fe97cf3ab7d49392913346879c7d40de7f4eaf86eef81c6"
    pe = struct.unpack_from("<I", exe, 0x3C)[0]
    n, optional = struct.unpack_from("<H", exe, pe + 6)[0], struct.unpack_from("<H", exe, pe + 20)[0]
    sections = [struct.unpack_from("<8sIIII", exe, pe + 24 + optional + i * 40) for i in range(n)]

    def read(rva, length):
        for _, virtual_size, virtual_address, raw_size, raw_offset in sections:
            if virtual_address <= rva and rva + length <= virtual_address + min(virtual_size, raw_size):
                start = raw_offset + rva - virtual_address
                return exe[start:start + length]
        raise AssertionError(hex(rva))

    hook_entries = re.findall(r"\{(0x[0-9A-F]+),(0x[0-9A-F]+),\{([^}]+)\},reinterpret_cast<void \*>\(&\w+\),false\}", source)
    assert len(hook_entries) == 5
    for slot, rva, fingerprint in hook_entries:
        assert struct.unpack("<Q", read(int(slot, 16), 8))[0] == 0x140000000 + int(rva, 16)
        assert read(int(rva, 16), 16) == bytes(int(x, 16) for x in fingerprint.split(","))
    setter_block = source.split("SetterBytes[][16]={", 1)[1].split("};", 1)[0]
    setter_bytes = re.findall(r"\{([^}]+)\}", setter_block)
    for rva, fingerprint in zip((0xE1AAE0, 0xE1ADD0, 0xE1B160), setter_bytes, strict=True):
        assert read(rva, 16) == bytes(int(x, 16) for x in fingerprint.split(","))
    print(json.dumps(dict(ok=True, hook_qwords=5, function_fingerprints=8, source_transforms=35,
                          rapid_owners=rapid, total_owners=35, release_dll_checked=bool(args.release_dll))))


if __name__ == "__main__":
    main()
