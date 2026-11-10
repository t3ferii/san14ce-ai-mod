"""Reader for the decoded 1a4a447d image used by the offline native verifiers (tools/verify_*_native.py).
Replaces data/diagnostics/combat_formula_20261021/inspect_formula.py, which reads the former build. Offline, read-only."""
from pathlib import Path
import hashlib, struct
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'build/ui_native_analysis/editor83_b25953386/game.exe.unpacked.exe'
EXPECTED = '1a4a447d14e68c40ad2a5107c29c7341c10eee8fc297a7b35032900c341dbcaa'
data = SOURCE.read_bytes()
assert hashlib.sha256(data).hexdigest() == EXPECTED
_pe = struct.unpack_from('<I', data, 60)[0]
_st = _pe + 24 + struct.unpack_from('<H', data, _pe + 20)[0]
_sections = [struct.unpack_from('<IIII', data, _st + i * 40 + 8) for i in range(struct.unpack_from('<H', data, _pe + 6)[0])]


def read(rva, size):
    s = next(s for s in _sections if s[1] <= rva and rva + size <= s[1] + s[2])
    p = s[3] + rva - s[1]
    return data[p:p + size]
