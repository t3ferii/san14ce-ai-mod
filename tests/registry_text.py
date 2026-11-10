"""Source-text access to the generated activation table for the static verifiers (W4-0).

The install statements that used to be spelled out in split_activate (native_split_install.inl) are rows of
src/module_table_convenience.inl (generated from development/module_registry.json). Verifiers that asserted the old text
ask this module for the row of a feature instead.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TABLE = ROOT / 'src' / 'module_table_convenience.inl'


def table():
    return TABLE.read_text(encoding='utf-8')


def index(name):
    """Offset of the row in the table text (activation order = offset order)."""
    return table().index('    {"%s",' % name)


def row(name):
    text = table()
    start = text.index('    {"%s",' % name)
    return text[start:text.index('\n', start)]


def cond(name):
    """The #if line guarding the row ('' when unconditional)."""
    text = table()
    start = text.index('    {"%s",' % name)
    before = text[:start].rstrip('\n').split('\n')[-1]
    return before[4:] if before.startswith('#if ') else ''


# ---- W5-1: the generated unity include lists (src/module_unity_{combat,convenience,fixture}.inl)
def unity(which='fixture'):
    """Text of one root's include list (the fixture list holds every entry in file order)."""
    return (ROOT / 'src' / ('module_unity_%s.inl' % which)).read_text(encoding='utf-8')


def hosts(name):
    """DLL roots whose generated include list includes the file (subset of ['combat', 'convenience'])."""
    return [w for w in ('combat', 'convenience') if '#include "%s"' % name in unity(w)]


def unity_index(name):
    """Offset of the include in the fixture list (= include order); -1 when absent."""
    return unity().find('#include "%s"' % name)


def split_install_text():
    """native_split_install.inl plus the generated SplitPointer owners/rows (src/module_pointers.inl) it includes."""
    base = (ROOT / 'src' / 'native_split_install.inl').read_text(encoding='utf-8-sig')
    return base + '\n' + (ROOT / 'src' / 'module_pointers.inl').read_text(encoding='utf-8')


def adapter_text():
    """native_adapter.cpp plus the lists its unity slots pull: what the former hand-written include lines said."""
    return (ROOT / 'src' / 'native_adapter.cpp').read_text(encoding='utf-8-sig') + '\n' + unity()
