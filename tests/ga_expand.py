"""Static verification scripts read the C++ sources as text and match original RVAs. Since 2026-11-05 (user, patch resilience)
the sources spell every original address as an address table id: GA(GA_22C990) (id = reference-build RVA), guard hashes as
GH(0x...ull) and literal-site memcmp guards as ga::mcmp(image,GA_X,off,...). Importing this module makes Path.read_text() and
open() return the text with those forms expanded back to the reference-build literals, so the existing checks keep their meaning
(they verify current reference-build RVA ids; runtime resolution is verified by tools/check_addresses.py)."""
import builtins, io, re
from pathlib import Path

_EXT = ('.h', '.cpp', '.inl')
_GA = re.compile(r'GA\(GA_([0-9A-F]+)\)')
_TMPL = re.compile(r'(?<![\w])GA_([0-9A-F]{4,8})(?![\w(])')
_GH = re.compile(r'GH\((0[xX][0-9A-Fa-f]+[uU][lL]{2})\)')
_MC = re.compile(r'ga::mcmp\(image,GA_([0-9A-F]+),([^,]+),')


def expand(text):
    text = _GH.sub(r'\1', text)
    text = _MC.sub(lambda m: 'memcmp(image+0x%s%s,' % (m.group(1), '' if m.group(2).strip() == '0' else '+' + m.group(2)), text)
    text = _GA.sub(r'0x\1', text)
    return text


def _wants(path):
    s = str(path).replace('\\', '/')
    return s.endswith(_EXT) and '/src/' in s and 'game_addresses' not in s


_read_text = Path.read_text


def read_text(self, *a, **k):
    t = _read_text(self, *a, **k)
    return expand(t) if _wants(self) else t


Path.read_text = read_text
_open = builtins.open


def open_(file, mode='r', *a, **k):
    f = _open(file, mode, *a, **k)
    if 'b' not in mode and 'r' == mode.replace('t', '') and isinstance(file, (str, Path)) and _wants(file):
        try:
            data = f.read()
        finally:
            f.close()
        return io.StringIO(expand(data))
    return f


builtins.open = open_
