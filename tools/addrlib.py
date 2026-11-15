#!/usr/bin/env python
"""Source scanning helpers of the address table tools (user 2026-11-05 patch resilience; docs/PATCH_RESILIENCE.md).

Finds original-range hex literals in src/ (outside comments, strings and char literals) and classifies them (image RVA vs object offset).
Used by tools/check_addresses.py (b). The PE reader / decoder / build registry live in tools/pelib.py (W2 of docs/MODULARIZATION_PLAN.md).
Standard library only.
"""
import os, re, sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)                                  # AI_TacticsMod
IMG_LO = 0x1000
IMG_HI = 0x29FE248 + 0x1000                                    # upper bound of the original image range for a literal to count as an RVA

# ---------------------------------------------------------------- source literals
HEX = re.compile(r'0[xX]([0-9A-Fa-f]{5,9})[uUlL]{0,3}(?![0-9A-Za-z_])')
SRC_EXT = {'.h', '.cpp', '.inl', '.c'}


def code_spans(lines):
    """yield (line_index, start, end) spans of code (outside comments, string and char literals)."""
    in_block = False
    for ln, s in enumerate(lines):
        i, n, start = 0, len(s), 0
        spans = []
        while i < n:
            if in_block:
                j = s.find('*/', i)
                if j < 0:
                    i = n
                    start = n
                else:
                    i = j + 2
                    start = i
                    in_block = False
                continue
            c = s[i]
            if c == '/' and i + 1 < n and s[i + 1] == '/':
                spans.append((start, i))
                start = n
                i = n
                break
            if c == '/' and i + 1 < n and s[i + 1] == '*':
                spans.append((start, i))
                in_block = True
                i += 2
                continue
            if c == '"' or c == "'":
                q = c
                spans.append((start, i))
                i += 1
                while i < n and s[i] != q:
                    i += 2 if s[i] == '\\' else 1
                i += 1
                start = i
                continue
            i += 1
        if not in_block and start < n:
            spans.append((start, n))
        elif in_block and start < n:
            spans.append((start, n))
        for a, b in spans:
            if b > a:
                yield ln, a, b


def literals(path):
    """[(line_no(1 based), col, text, value)] of 5-9 digit hex literals in code."""
    try:
        lines = open(path, encoding='utf-8', errors='replace').read().split('\n')
    except OSError:
        return [], []
    out = []
    for ln, a, b in code_spans(lines):
        seg = lines[ln][a:b]
        for m in HEX.finditer(seg):
            out.append((ln + 1, a + m.start(), m.group(0), int(m.group(1), 16)))
    return out, lines


# 4-digit RVAs (0x1000..0xFFFF) exist too; only taken in call/list contexts (not size/flag constants)
PROFILE_ROW_POST = r'\s*,\s*0[xX][0-9A-Fa-f]{1,4}\s*,\s*(?:GH\()?0[xX][0-9A-Fa-f]{10,16}[uU][lL]{2}'
HEX4 = re.compile(r'0[xX]([0-9A-Fa-f]{4})[uUlL]{0,3}(?![0-9A-Za-z_])')

# offset-style contexts: the literal is a field/table offset of a game object, not an image RVA
OFFSET_PRE = [
    re.compile(r'\btable\(\s*$'), re.compile(r'\btable\(\s*[\w.\[\]>-]+\s*,\s*$'),
    re.compile(r'(?<![\w])(?:word|dword|byte|pointer)\(\s*(?!image\b)[\w.\[\]>()+*-]+\s*,\s*$'),
    re.compile(r'(?:action|results|replay_results|offset|base|row|record)\s*\+\s*$'),
    re.compile(r'k(?:Army|Troops)Table=$'), re.compile(r'\bat\s*[=<]\s*$'), 
    re.compile(r'officer\*8\s*:\s*$'), re.compile(r'key,0\)\s*:\s*$'), re.compile(r'i\*0x[0-9A-Fa-f]+\s*:\s*$'),
    re.compile(r'\bbytes\(\s*$'), re.compile(r'uintptr_t offset=$'), re.compile(r'offset<$'), re.compile(r'(?<!&)&\s*$'),
    re.compile(r'(?:table|base)\s*(?:>=|-)\s*$'), re.compile(r'[<>]=?\s*$'),
]
# literals that are no addresses although they sit in the old image range (file, value) -> reason; kept explicit for review
NON_RVA = {
    ('native_layout_ui.inl', 0xFFFF): '16-bit mask', ('native_ccz_sprites.inl', 0x10000): 'size constant',
    ('native_terrain_foam.inl', 0xEC810E): 'foam record key (not an address)',
    ('native_ai_trace.inl', 0x7E838): 'army table offset in the game world object', ('native_ai_trace.inl', 0x7F8D8): 'troops table offset',
    ('native_drag_learn.inl', 0x77428): 'tactic table offset in world', ('native_drag_learn.inl', 0x774D0): 'tactic table end offset',
    ('native_modpack.inl', 0x7DD18): 'world table offset', ('native_modpack.inl', 0x774D0): 'world table offset',
    ('native_path_fast.inl', 0x73A50): 'world person fallback slot offset', ('native_path_fast.inl', 0x14000): 'id key constant',
    ('native_advisor_briefing.inl', 0x16140): 'report record buffer offset', ('native_advisor_briefing.inl', 0x16290): 'report record size',
    ('native_advisor_briefing.inl', 0x16280): 'report record buffer offset', ('native_advisor_briefing.inl', 0x16284): 'report record buffer offset',
    ('path_fast.h', 0xFFFFFF): '24-bit mask', ('native_traits9.inl', 0x10000): 'address-space scan stride', ('native_ai_trace.inl', 0x10000): 'address-space scan stride (fn_trace near page)', ('trait_hover_profile.h', 0x10000): 'kEditRowMin: tooltip key-space boundary (keys >= this are edit-row pointers), no address', ('native_advisor_overlay.inl', 0xD5A0): 'GUID field', ('native_advisor_overlay.inl', 0x11CF): 'GUID field', ('path_fast.h', 0x1D8A80): 'node buffer byte count (cells*sizeof(Node))', ('native_adapter.cpp', 0x1000): 'PE .text base / byte count (same in every build, verified by check_addresses)', ('native_adapter.cpp', 0x2000): 'allocation size', ('native_modpack.inl', 0x1000): 'argument constant', ('native_split_install.inl', 0x2000): 'allocation size', ('native_traits9.inl', 0x3000000): 'address-space scan bound', ('native_traits9.inl', 0x70000000): 'address-space scan bound',
}
