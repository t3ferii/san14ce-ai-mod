"""Source text of the hub for the static verifiers (2026-11-16 multi-unit refactor).

The shared definitions that used to open native_adapter.cpp (settings, ModFeatures, logging, GA, patch services) now live in
src/native_core.inl and src/native_patch_core.inl, which native_adapter.cpp and every src/tu_<root>_<unit>.cpp include first.
A verifier that asserted hub text reads all of it, in include order, so a fact keeps counting wherever the hub keeps it.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = ('native_core.inl', 'native_patch_core.inl', 'native_adapter.cpp')   # include order


def hub_files(root=ROOT):
    return [Path(root) / 'src' / f for f in FILES if (Path(root) / 'src' / f).exists()]


def hub_text(root=ROOT):
    return '\n'.join(p.read_text(encoding='utf-8-sig') for p in hub_files(root))
