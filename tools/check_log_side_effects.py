#!/usr/bin/env python
"""Reject game/UI calls passed as log_event() arguments (FX4: empty tactic-priority popup in the release build).

  python -X utf8 tools/check_log_side_effects.py

log_event(...) discards every argument when the logger is compiled out (NO_RUNTIME_LOG, src/native_adapter.cpp) or diagnostics are
off: `#define log_event(...) do {if constexpr(false)runtime_log_discard(__VA_ARGS__);}while(0)`. A call placed inside log_event's
argument list therefore never runs in the release build, which is exactly what hid the tactic-priority popup
(src/native_tactic_priority_grid.inl: layout_client::ui_activate(pane) lived inside the log call).

The scan joins each log_event( call up to its matching closing paren, then flags the arguments when they call a UI/game entry point:
layout_client::ui_, ui_activate, show_object(, set_visible(, fn<, or a GA(GA_xxx)( indirect game call.
Exit code 0 when clean, 1 for every hit. Read-only game getters in PURE_GETTERS are allowed
(they only compute the logged value; skipping them in a release build changes nothing).
"""
import os, sys, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'src')
CALL = re.compile(r'(?<![A-Za-z0-9_])log_event\s*\(')
# Read-only getters (no game state change): 22F830 = tile -> area id.
PURE_GETTERS = {'GA_22F830'}
PURE_CALL = re.compile(r'fn\s*<[^>]*>\s*\(\s*GA\s*\(\s*(GA_[0-9A-Fa-f]+)\s*\)\s*\)')
SIDE_EFFECT = re.compile(r'layout_client::ui_|ui_activate|show_object\s*\(|set_visible\s*\(|fn\s*<|GA\s*\(\s*GA_[A-Za-z0-9_]+\)\s*\(')


def args_after(text, start):
    """Return (raw argument text, code-only argument text) for the call whose '(' is at start-1.

    The scan is string/char/comment aware both for paren depth and for matching: literals and comments are blanked in the
    code-only copy, so a log message that merely contains 'show_object(' is not a hit."""
    depth = 0
    i = start - 1
    n = len(text)
    code = []
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            quote = c
            code.append(' ')
            i += 1
            while i < n and text[i] != quote:
                code.append(' ')
                i += 2 if text[i] == '\\' else 1
            code.append(' ')
            i += 1
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            j = text.find('\n', i)
            j = n if j < 0 else j
            code.append(' ' * (j - i))
            i = j
            continue
        if c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            code.append(' ' * (j - i))
            i = j
            continue
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return text[start:i], ''.join(code)
        code.append(c)
        i += 1
    return text[start:], ''.join(code)


def scan(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        text = f.read()
    hits = []
    for m in CALL.finditer(text):
        line = text.count('\n', 0, m.start()) + 1
        line_text = text[text.rfind('\n', 0, m.start()) + 1:text.find('\n', m.start())].lstrip()
        if line_text.startswith('#define'):     # the macro definitions themselves
            continue
        raw, code = args_after(text, m.end())
        code = PURE_CALL.sub(lambda g: 'pure_getter' if g.group(1) in PURE_GETTERS else g.group(0), code)
        bad = SIDE_EFFECT.search(code)
        if bad:
            hits.append((line, ' '.join(raw.split())[:120], bad.group(0)))
    return hits


def main():
    files = []
    for name in sorted(os.listdir(SRC)):
        if name.endswith(('.inl', '.cpp', '.h')):
            files.append(os.path.join(SRC, name))
    total = 0
    for path in files:
        for line, args, what in scan(path):
            total += 1
            print('%s:%d: log_event argument calls %s -> %s' % (os.path.relpath(path, ROOT).replace('\\', '/'), line, what, args))
    print('%d file(s) scanned, %d hit(s)' % (len(files), total))
    return 1 if total else 0


if __name__ == '__main__':
    sys.exit(main())
