#!/usr/bin/env python3
"""Feature dependency scanner for the San14 mod (modularization program W0-2, docs/MODULARIZATION_PLAN.md section 1.2).

Builds a module -> module edge list from src/ so that checks can predict conflict points and the coupling can be reduced
wave by wave.  Standard library + tools/hookmap_extract.py (preprocessor, build flags, hook sites) only; writes only below
build/development/feature_deps/.

Pipeline
  1. build variants (convenience / combat / ccz / layout / loader) with hookmap_extract.compute_variants (build_split.ps1 flags)
  2. per file (cached by content hash): comments, string contents, directive lines and lines inactive in every variant are
     blanked (offsets kept), then a namespace-scope walker collects definitions (functions, statics, types, enumerators,
     using, macros, body-less declarations) and a tokenizer collects references
  3. per DLL root an independent definition table; references resolve through the namespace chain, `using namespace` and
     the global scope; 2+ candidate modules = ambiguous (not counted); generic names only when qualified or uniquely defined
  4. edges = referencing module -> defining module (+ mod_features.<flag> reads, shared hook_map sites, cross-module #include)
  5. rules D1..D10 and a ratchet against development/feature_deps_policy.json

Commands
  feature_deps.py scan [--root DIR] [--no-hooks]      write build/development/feature_deps/edges.{json,tsv,dot}, print a summary
  feature_deps.py --check                             scan + ratchet check (always route, exit 1 on a new violation)
  feature_deps.py explain <module>                    edges, files, SCC, dead includes of one module
  feature_deps.py impact <file> [--hunks] [--base R]  modules affected by a change (for select_checks)
  feature_deps.py tools                               tools/ and tests/ files not reachable from any route (list only)
  feature_deps.py --baseline | --tighten              rewrite the whole baseline (human decision) | only shrink it

Limits (heuristics): single-name local variables that shadow a global are filtered only for functions (a reference counts
when followed by '(' or preceded by '&'); #if inside a definition that is active in two variants at once can confuse the
brace walker; macros count as defined when their file is compiled in that DLL; flag reads are only recognised as
mod_features.<flag>; D9 (depends vs installer parent/child) is evaluated by tools/gen_installer_catalog.py --check
(development/installer_map.json).
"""
import argparse
import bisect
import collections
import datetime
import hashlib
import io
import json
import os
import pickle
import re
import subprocess
import sys
import time
import tokenize

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hookmap_extract as X  # noqa: E402
import select_checks as SC   # noqa: E402

VERSION = 4
_SCAN_STAMP = [None]


def scan_stamp():      # scanner code changes invalidate the per-file cache
    if _SCAN_STAMP[0] is None:
        with open(__file__, 'rb') as f:
            _SCAN_STAMP[0] = hashlib.sha1(f.read()).hexdigest()[:10]
    return _SCAN_STAMP[0]
REAL_DLLS = ('convenience', 'combat', 'ccz', 'layout', 'loader')
WHITELIST_DLLS = {'convenience', 'ccz', 'combat'}
KEYWORDS = set('''if else for while do switch case default break continue return goto sizeof alignof alignas new delete this nullptr
true false static const constexpr volatile inline extern struct class union enum namespace using typedef template typename public
private protected virtual override final explicit operator try catch throw noexcept static_cast reinterpret_cast const_cast
dynamic_cast decltype auto void bool char short int long float double signed unsigned wchar_t __declspec __forceinline __fastcall
__cdecl __stdcall __restrict thread_local mutable register friend asm __asm and or not defined'''.split())
D6_CALLS = ('prepare_link', 'prepare_bytes', 'call_prepare', 'patch_pointer', 'ROUTE_VT', 'replace_code16', 'modpack_write',
            'add_hook', 'hook_abs_slot')
DEFAULT_GENERIC = ['guards', 'profile', 'draw', 'install', 'state', 'init', 'reset', 'load', 'save', 'apply', 'update', 'enabled',
                   'run', 'tick', 'path', 'open', 'close', 'read', 'write']
WAVES = {'D1': 'W6', 'D2': 'W5', 'D4': 'W4', 'D5': 'W4', 'HOOK': 'W4'}
POLICY_REL = 'development/feature_deps_policy.json'
OUT_REL = 'build/development/feature_deps'

_STRIP = re.compile(r'''^[ \t]*\#(?:\\[\s\S]|[^\n\\])*|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*\'''', re.M)
_STOP = re.compile(r'[{};]')
_BRACE = re.compile(r'[{}]')
_TOKEN = re.compile(r'(?<![\w.])(?:::)?[A-Za-z_]\w*(?:\s*::\s*~?[A-Za-z_]\w*)*')
_NS = re.compile(r'^(?:inline\s+)?namespace\s*([\w:]*)$')
_EXTERN_C = re.compile(r'^extern\s*"[^"]*"$')
_TYPEKW = re.compile(r'\b(struct|class|union|enum)\b(?:\s+(?:class|struct))?\s*(?:alignas\s*\([^)]*\)\s*)?(\w+)?')
_ATTR = re.compile(r'__declspec\s*\([^)]*\)|alignas\s*\([^)]*\)|__attribute__\s*\(\([^)]*\)\)')
_FLAG = re.compile(r'\bmod_features\s*(?:\.|->)\s*(\w+)')
_D6 = re.compile(r'\b(%s)\s*\(' % '|'.join(D6_CALLS))
_FNAME = re.compile(r'((?:[A-Za-z_]\w*\s*::\s*)*(?:~?[A-Za-z_]\w*|operator\s*[^\s\w(]+|operator\s+\w+))\s*$')
_INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)
_DEFINE = re.compile(r'^\s*#\s*define\s+(\w+)', re.M)


def now_date():
    return datetime.date.today().isoformat()


def sha1(b):
    return hashlib.sha1(b).hexdigest()


# ------------------------------------------------------------------ per-file scan
def strip_text(text):
    """text (comments already blanked) -> (blanked text, directive texts[(line, text)]); string contents and directives -> spaces."""
    dirs = []
    pos = [0, 1]

    def line_at(off):
        pos[1] += text.count('\n', pos[0], off)
        pos[0] = off
        return pos[1]

    def sub(m):
        t = m.group(0)
        if t[0] in '"\'':
            return t[0] + re.sub(r'[^\n]', ' ', t[1:-1]) + t[-1]
        dirs.append((line_at(m.start()), t))
        return re.sub(r'[^\n]', ' ', t)
    return _STRIP.sub(sub, text), dirs


def match_braces(B):
    stack, out = [], {}
    for m in _BRACE.finditer(B):
        if m.group() == '{':
            stack.append(m.start())
        elif stack:
            out[stack.pop()] = m.start()
    return out


def strip_template(h):
    h = h.lstrip()
    while h.startswith('template'):
        i = h.find('<')
        if i < 0:
            break
        d = 0
        j = i
        while j < len(h):
            if h[j] == '<':
                d += 1
            elif h[j] == '>':
                d -= 1
                if d == 0:
                    break
            j += 1
        h = h[j + 1:].lstrip()
    return h


def ntokens(s):
    return len([w for w in re.findall(r'[A-Za-z_]\w*', s) if w not in ('static', 'inline', 'const', 'extern', 'constexpr', 'volatile', 'unsigned', 'signed')]) + \
        len(re.findall(r'\b(?:unsigned|signed)\b', s))


def fn_name(head):
    """(name, tokens before name) of the function a head declares, or None."""
    h = _ATTR.sub(' ', strip_template(head)).strip()
    p = h.find('(')
    if p <= 0:
        return None
    pre = h[:p]
    m = _FNAME.search(pre)
    if not m:
        return None
    name = re.sub(r'\s+', '', m.group(1))
    if name.split('::')[-1] in KEYWORDS or name.startswith('operator') and False:
        return None
    return name, ntokens(pre[:m.start()]), h


def var_name(head):
    h = _ATTR.sub(' ', strip_template(head)).strip()
    seg = h.split('=')[0]
    seg = re.sub(r'\[[^\]]*\]', ' ', seg).strip()
    seg = re.sub(r'\bextern\s*"[^"]*"', ' ', seg)
    m = re.search(r'([A-Za-z_]\w*)\s*$', seg)
    if not m or m.group(1) in KEYWORDS:
        return None
    if ntokens(seg[:m.start()]) < 1:
        return None
    return m.group(1)


def is_var_head(h):
    if '=' not in h:
        return '(' not in h
    return '(' not in h or h.index('=') < h.index('(')


def walk(B, nl, braces):
    """namespace-scope walk of the blanked text -> (defs, usings, chain events, imbalance)."""
    defs, usings, events = [], [], [(0, ())]
    chain, stack = [], []
    pos, n, imb = 0, len(B), 0

    def L(off):
        return bisect.bisect_left(nl, off) + 1

    def q(name):
        return '::'.join(chain + [name]) if chain else name

    def add(name, kind, off, end_off):
        defs.append((q(name.lstrip(':')), kind, L(off), L(end_off)))

    while pos < n:
        m = _STOP.search(B, pos)
        if not m:
            break
        c, i = m.group(), m.start()
        if c == '}':
            if stack:
                for _ in range(stack.pop()):
                    chain.pop()
                events.append((i, tuple(chain)))
            else:
                imb += 1
            pos = i + 1
            continue
        head = B[pos:i]
        hs = head.strip()
        off = pos + (len(head) - len(head.lstrip()))
        if c == ';':
            pos = i + 1
            if not hs:
                continue
            h = _ATTR.sub(' ', strip_template(hs)).strip()
            if h.startswith('using'):
                mm = re.match(r'using\s+namespace\s+([\w:]+)', h)
                if mm:
                    usings.append(mm.group(1).lstrip(':'))
                    continue
                mm = re.match(r'using\s+(\w+)\s*=', h)
                if mm:
                    add(mm.group(1), 'type', off, i)
                    continue
                mm = re.match(r'using\s+([\w:]+)\s*$', h)
                if mm:                                  # using-declaration: the name becomes visible here
                    usings.append('@' + mm.group(1).lstrip(':'))
                continue
            if h.startswith('typedef'):
                mm = re.search(r'\(\s*\*\s*(\w+)\s*\)', h) or re.search(r'(\w+)\s*(?:\[[^\]]*\])?\s*$', h)
                if mm:
                    add(mm.group(1), 'type', off, i)
                continue
            if h.startswith(('static_assert', 'friend', 'struct ', 'class ', 'enum ', 'union ')) and '(' not in h:
                continue
            if is_var_head(h):
                v = var_name(h)
                if v:
                    add(v, 'var', off, i)
                continue
            fnm = fn_name(h)
            if fnm and (fnm[1] >= 1 or '::' in fnm[0]) and '=' not in fnm[2][:fnm[2].find('(')]:
                add(fnm[0], 'decl', off, i)
            continue
        # '{'
        nm = _NS.match(hs)
        if nm:
            parts = [p for p in nm.group(1).split('::') if p]
            if not parts:
                parts = ['(anon)']          # unnamed namespace: internal linkage, never visible to other files
            chain.extend(parts)
            stack.append(len(parts))
            events.append((i, tuple(chain)))
            pos = i + 1
            continue
        if _EXTERN_C.match(hs):
            stack.append(0)
            pos = i + 1
            continue
        j = braces.get(i)
        if j is None:
            imb += 1
            break
        h = _ATTR.sub(' ', strip_template(hs)).strip()
        pos = j + 1
        if not h:
            continue
        tm = _TYPEKW.search(h) if '(' not in h and '=' not in h else None
        if tm and not h.startswith('typedef'):
            kw, nm_ = tm.group(1), tm.group(2)
            if nm_ and nm_ not in KEYWORDS:
                add(nm_, 'type', off, j)
            if kw == 'enum':
                scoped = bool(re.match(r'enum\s+(?:class|struct)\b', h))
                for part in X.split_top(B[i + 1:j]):
                    em = re.match(r'\s*([A-Za-z_]\w*)', part)
                    if em:
                        add(('%s::%s' % (nm_, em.group(1))) if scoped and nm_ else em.group(1), 'enumerator', i, j)
            continue
        if h.startswith('typedef'):
            mm = re.search(r'\}\s*(\w+)', B[j:j + 80]) or None
            if mm:
                add(mm.group(1), 'type', off, j)
            continue
        if is_var_head(h) and '=' in h:
            v = var_name(h)
            if v:
                add(v, 'var', off, j)
            continue
        fnm = fn_name(h)
        if fnm and (fnm[1] >= 1 or '::' in fnm[0]):
            add(fnm[0], 'func', off, j)
    return defs, usings, events, imb


def scan_file(text, union):
    """text = comment-stripped source, union = set of lines active in at least one variant."""
    B, dirs = strip_text(text)
    lines = B.split('\n')
    lines = [l if (i + 1 in union or not l.strip()) else ' ' * len(l) for i, l in enumerate(lines)]
    B = '\n'.join(lines)
    nl = [m.start() for m in re.finditer('\n', B)]
    braces = match_braces(B)
    defs, usings, events, imb = walk(B, nl, braces)
    for ln, t in dirs:
        mm = _DEFINE.match(t)
        if mm:
            defs.append((mm.group(1), 'macro', ln, ln))
    incs = []
    for ln, t in dirs:
        mm = _INCLUDE.match(t)
        if mm:
            incs.append((os.path.basename(mm.group(1).replace('\\', '/')), ln))
    ev_off = [e[0] for e in events]
    chains = {}
    refs = {}
    flags, d6 = [], []
    off = 0
    multi = len(events) > 1
    for ln, lt in enumerate(lines, 1):
        lstart = off
        off += len(lt) + 1
        if not lt.strip():
            continue
        for mm in _FLAG.finditer(lt):
            flags.append((mm.group(1), ln))
        for mm in _D6.finditer(lt):
            d6.append((mm.group(1), ln))
        for m in _TOKEN.finditer(lt):
            s = m.start()
            tok = m.group()
            if lt[s - 2:s] == '->':
                continue
            first = tok.split('::')[0].strip()
            if first in KEYWORDS and '::' not in tok:
                continue
            name = re.sub(r'\s+', '', tok)
            if name.startswith('::'):
                name = name[2:]
            ch = events[bisect.bisect_right(ev_off, lstart + s) - 1][1] if multi else ()
            ci = chains.setdefault(ch, len(chains))
            after = lt[m.end():m.end() + 6].lstrip()
            pre = lt[max(0, s - 3):s].rstrip()
            fl = 1 if (after[:1] == '(' or (pre[-1:] == '&' and pre[-2:] != '&&')) else 0
            refs.setdefault((name, ci, fl), []).append(ln)
    return {'defs': defs, 'usings': usings, 'chains': sorted(chains, key=chains.get), 'refs': refs, 'flags': flags, 'd6': d6,
            'incs': incs, 'imbalance': imb}


# ------------------------------------------------------------------ manifest / policy
class Manifest:
    def __init__(self, tree):
        d = X.read_json(tree, 'development/features.json', {}) or {}
        mods = d.get('modules') or {}
        if isinstance(mods, list):
            mods = {m.get('name'): m for m in mods}
        self.mods = mods
        self.data = d
        self.globs = []
        for n, m in mods.items():
            for g in m.get('owns') or []:
                if g.startswith('src/') or g.startswith('src\\'):
                    self.globs.append((SC.glob_re(g), n))
        self.cache = {}

    def owner(self, rel):
        v = self.cache.get(rel)
        if v is None:
            v = '?'
            for rx, n in self.globs:
                if rx.match(rel):
                    v = n
                    break
            self.cache[rel] = v
        return v

    def dll(self, m):
        return (self.mods.get(m) or {}).get('dll', '')

    def depends(self, m):
        return list((self.mods.get(m) or {}).get('depends') or [])

    def closure(self, m):
        seen, todo = set(), self.depends(m)
        while todo:
            x = todo.pop()
            if x not in seen:
                seen.add(x)
                todo += self.depends(x)
        return seen


def load_policy(root):
    p = os.path.join(root, POLICY_REL)
    try:
        with open(p, 'rb') as f:
            d = json.loads(f.read().decode('utf-8-sig'))
    except (OSError, ValueError):
        d = {}
    d.setdefault('schema', 1)
    d.setdefault('generic_names', list(DEFAULT_GENERIC))
    d.setdefault('services', [])
    d.setdefault('flag_owners', {})
    d.setdefault('edges', [])
    d.setdefault('sccs', [])
    d.setdefault('counters', {})
    d.setdefault('tools_baseline', [])
    d.setdefault('entry_tools', {})
    return d


def save_policy(root, pol):
    pol['edges'] = sorted(pol['edges'], key=lambda e: (e['rule'], e['from'], e['to']))
    pol['counters'] = dict(sorted(pol['counters'].items()))
    p = os.path.join(root, POLICY_REL)
    with open(p, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(pol, f, ensure_ascii=False, indent=1)
        f.write('\n')


# ------------------------------------------------------------------ tracked files / legacy / reachability
def tracked(root):
    try:
        out = subprocess.run(['git', '-c', 'core.quotepath=false', '-C', root, 'ls-files', '-z'], capture_output=True, timeout=60)
        if out.returncode == 0 and out.stdout:
            return sorted(x for x in out.stdout.decode('utf-8', 'replace').split('\0')
                          if x and os.path.isfile(os.path.join(root, x)))
    except (OSError, subprocess.SubprocessError):
        pass
    res = []
    for dp, dn, fn in os.walk(root):
        dn[:] = [d for d in dn if d not in ('build', '.git', '__pycache__')]
        for f in fn:
            res.append(os.path.relpath(os.path.join(dp, f), root).replace('\\', '/'))
    return sorted(res)


def read_text(root, rel):
    try:
        with open(os.path.join(root, rel), 'rb') as f:
            b = f.read()
    except OSError:
        return None
    if b'\0' in b[:4096]:
        return None
    return b.decode('utf-8', 'replace')


LEGACY_SKIP = ('legacy/', 'research/', 'build/', 'reference_mod/', 'REA_Analysis/', 'graphify-out/', 'backup/', 'development/modules/')
TEXT_EXT = ('.py', '.ps1', '.psm1', '.cpp', '.inl', '.h', '.c', '.json', '.txt', '.md', '.cs', '.ini', '.cmd', '.asm', '.hlsl', '.csv')


def legacy_counters(root, files):
    c = collections.Counter()
    detail = collections.defaultdict(list)
    names = set(files)
    for rel in files:
        if rel.startswith(LEGACY_SKIP) or rel in ('docs/HISTORY.md', POLICY_REL, 'tools/feature_deps.py', 'tests/verify_feature_deps.py') or not rel.endswith(TEXT_EXT):
            continue
        t = read_text(root, rel)
        if not t:
            continue
        n = t.count('1fbe6be7')
        if n:
            c['old_build_1fbe6be7_files'] += 1
            c['old_build_1fbe6be7_mentions'] += n
            detail['1fbe6be7'].append(rel)
        if re.search(r'running_code_text\.bin|native_analysis_only\.bin', t):
            c['old_image_path_files'] += 1
            detail['old_image'].append(rel)
        if 'ga_expand' in t and not os.path.basename(rel).startswith('ga_expand'):
            c['ga_expand_files'] += 1
            detail['ga_expand'].append(rel)
        if rel.endswith(('.md', '.ps1', '.py', '.json')):
            missing = set()
            for m in re.finditer(r'(?<![\w/])docs/([\w\-\.]+\.md)\b', t):
                ref = 'docs/' + m.group(1)
                if ref not in names and not os.path.exists(os.path.join(root, ref)):
                    missing.add(ref)
            if rel.endswith('.md'):
                base = os.path.dirname(rel)
                for m in re.finditer(r'\]\(([\w\-\.\/]+\.md)(?:#[^)]*)?\)', t):
                    ref = os.path.normpath(os.path.join(base, m.group(1))).replace('\\', '/')
                    if ref not in names and not os.path.exists(os.path.join(root, ref)):
                        missing.add(ref)
            if missing:
                c['missing_doc_refs'] += len(missing)
                for ref in sorted(missing):
                    detail['missing_doc'].append('%s -> %s' % (rel, ref))
    return dict(c), dict(detail)


_NAME_RE = re.compile(r'[\w\-]+\.(?:py|ps1|psm1|cpp|inl|h|c|json|asm|hlsl|bat|cmd|txt|ini|cs|exe|dll)\b')
_IMPORT_RE = re.compile(r'^\s*(?:from|import)\s+([A-Za-z_]\w*)', re.M)


def route_blocks(ps1text):
    """{route name: text of its switch branch} in check_feature.ps1."""
    out = {}
    for m in re.finditer(r"^[ \t]*'([\w\-]+)'[ \t]*\{", ps1text, re.M):
        i = m.end() - 1
        d, j = 0, i
        while j < len(ps1text):
            ch = ps1text[j]
            if ch == '{':
                d += 1
            elif ch == '}':
                d -= 1
                if d == 0:
                    break
            j += 1
        out[m.group(1)] = ps1text[i:j + 1]
    return out


# W7: reachability follows only executable references. src/ (comments name tools), development/ (the manifest names every tool in owns
# globs) and docs/ are never a caller; a file nothing executes is unreachable unless the policy lists it under entry_tools (manual entry
# points a person runs: install_local.py, ghidra queries, release gates) with a reason.
NO_TRAVERSE = ('src/', 'development/', 'docs/')   # (C++ files are skipped too: a comment naming a tool does not run it)
_INCLUDE_RE = re.compile(r'^[ \t]*#[ \t]*include[ \t]+"([^"]+)"', re.M)
_TOKEN_RE = re.compile(r'[A-Za-z_]\w{9,}')
_TMPL_RE = re.compile(r'([\w\-]*(?:\$\{\w+\}|\$\w+)[\w\-\$\{\}]*)\.(py|ps1|cpp|inl|h)\b')   # "terrain_render_${name}_fixture.cpp"


def executable_refs_text(rel, text):
    """Discard comments/docstrings without losing literal paths used by scripts.

    This is a conservative reference scan, not an interpreter: dynamic references
    still need an entry_tools declaration. User 2026-11-07 W7 retirement.
    """
    if rel.endswith('.py'):
        try:
            import ast
            text = text.lstrip('\ufeff')
            tree = ast.parse(text)
            # AST columns are UTF-8 byte offsets. Preserve statements after a
            # one-line docstring (including Korean text and a semicolon).
            lines = text.encode('utf-8').splitlines(keepends=True)
            for node in ast.walk(tree):
                body = getattr(node, 'body', None)
                if isinstance(body, list) and body and isinstance(body[0], ast.Expr) \
                        and isinstance(body[0].value, ast.Constant) and isinstance(body[0].value.value, str):
                    doc = body[0]
                    for row in range(doc.lineno - 1, doc.end_lineno):
                        lo = doc.col_offset if row == doc.lineno - 1 else 0
                        hi = doc.end_col_offset if row == doc.end_lineno - 1 else len(lines[row])
                        lines[row] = lines[row][:lo] + re.sub(rb'[^\r\n]', b' ', lines[row][lo:hi]) + lines[row][hi:]
            text = b''.join(lines).decode('utf-8')
            return tokenize.untokenize(t for t in tokenize.generate_tokens(io.StringIO(text).readline)
                                       if t.type != tokenize.COMMENT)
        except (SyntaxError, tokenize.TokenError, IndentationError):
            return text  # never delete a tool merely because its caller could not be parsed
    if rel.endswith(('.ps1', '.psm1')):
        text = re.sub(r'<#.*?#>', '', text, flags=re.S)
        return re.sub(r'(?m)^[ \t]*#.*$', '', text)
    return text


def reachability(root, files, man, entry_tools=None):
    scope = [f for f in files if f.startswith(('tools/', 'tests/'))]
    bystem = collections.defaultdict(list)           # fixtures and verifiers are often named by a bare stem built into a path in a script
    for f in scope:
        stem = os.path.splitext(os.path.basename(f))[0]
        if '_' in stem and len(stem) >= 10:
            bystem[stem].append(f)
    allf = [f for f in files if not f.startswith(LEGACY_SKIP)]
    byname = collections.defaultdict(list)
    for f in allf:
        byname[os.path.basename(f)].append(f)
        if f.endswith(('.py', '.psm1')):
            byname[os.path.basename(f).rsplit('.', 1)[0] + '.py'].append(f)
    ps1 = read_text(root, 'tools/check_feature.ps1') or ''
    blocks = route_blocks(ps1)
    data = man.data
    routes = set(data.get('always') or [])
    for m in man.mods.values():
        routes.update(m.get('tests') or [])
    texts = []
    rootfiles = set()
    for r in sorted(routes):
        meta = (data.get('routes') or {}).get(r) or {}
        if meta.get('runner'):
            rootfiles.add(meta['runner'].replace('\\', '/'))
            texts.append(' '.join(str(a) for a in meta.get('args') or []))
        if meta.get('steps'):
            texts.append(json.dumps(meta['steps']))
        if r in blocks:
            texts.append(blocks[r])
        for i in meta.get('inputs') or []:
            texts.append(str(i))
    for rel in allf:
        if rel.startswith(('distribution/', 'installer/')) or '/' not in rel and rel.endswith(('.ps1', '.cmd', '.txt')) \
                or rel in ('tools/check_feature.ps1', 'tools/select_checks.py', 'tools/route_inputs.py', 'tools/run_checks.py'):
            rootfiles.add(rel)
    seen = set()

    def refs_of(text):
        res = set()
        for nm in _NAME_RE.findall(text):
            for f in byname.get(nm, ()):
                res.add(f)
        for im in _IMPORT_RE.findall(text):
            for f in byname.get(im + '.py', ()):
                res.add(f)
        for tok in _TOKEN_RE.findall(text):
            for f in bystem.get(tok, ()):
                res.add(f)
        for stem, ext in _TMPL_RE.findall(text):          # a path built from a variable: every scope file the pattern can name
            pat = re.compile(''.join('.+' if part.startswith('$') else re.escape(part)
                                     for part in re.split(r'(\$\{\w+\}|\$\w+)', stem) if part) + re.escape('.' + ext))
            for f in scope:
                if pat.fullmatch(os.path.basename(f)):
                    res.add(f)
        return res
    rootfiles.update(f for f in (entry_tools or {}) if f in set(allf))
    rootfiles.update(u['path'] for u in (data.get('unrouted_checks') or []) if u.get('path') in set(allf))   # hand-run checks the registry records
    todo = list(rootfiles)
    for t in texts:
        todo += list(refs_of(t))
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.add(f)
        if f == POLICY_REL:
            continue
        if f.endswith(('.cpp', '.inl', '.h', '.c')):                 # C++ comments do not run tools: follow #include lines only
            t = read_text(root, f)
            if t:
                for nm in _INCLUDE_RE.findall(t):
                    todo += [x for x in byname.get(os.path.basename(nm), ()) if x not in seen]
            continue
        if f.startswith(NO_TRAVERSE):
            continue
        if not f.endswith(('.py', '.ps1', '.psm1', '.cs', '.cmd', '.bat', '.json', '.txt')):
            continue
        t = read_text(root, f)
        if t:
            todo += [x for x in refs_of(executable_refs_text(f, t)) if x not in seen]
    doc_text = ''
    for d in [f for f in files if f.endswith('.md') and not f.startswith(LEGACY_SKIP)] + ['AGENTS.md']:
        t = read_text(root, d)
        if t:
            doc_text += t
    doc_names = set(_NAME_RE.findall(doc_text))
    unreachable = [f for f in scope if f not in seen]
    doc_only = [f for f in unreachable if os.path.basename(f) in doc_names]
    return {'unreachable': sorted(unreachable), 'doc_only': sorted(doc_only), 'reachable': len(seen & set(scope)),
            'scope': len(scope)}


# ------------------------------------------------------------------ analysis
def load_cache(path):
    try:
        with open(path, 'rb') as f:
            d = pickle.load(f)
        if d.get('version') == VERSION:
            return d
    except Exception:
        pass
    return {'version': VERSION, 'files': {}}


def write_cache(path, obj):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + '.tmp%d' % os.getpid()
    with open(tmp, 'wb') as f:
        pickle.dump(obj, f, protocol=4)
    os.replace(tmp, path)


def flag_owner(flag, man, pol):
    fo = pol.get('flag_owners') or {}
    if flag in fo:
        return fo[flag] or None
    names = {n.replace('-', '_'): n for n, m in man.mods.items() if m.get('status') in ('active', 'candidate')}
    if flag in names:
        return names[flag]
    best = None
    for k, n in names.items():
        if flag.startswith(k + '_') and (best is None or len(k) > len(best[0])):
            best = (k, n)
    return best[1] if best else None


def is_service(m, man, pol):
    return bool((man.mods.get(m) or {}).get('hub')) or m in (pol.get('services') or [])


def shared_files(tree):
    """Files that the registry lists under unity.shared: stateless code compiled into several DLL roots by design (W5-1).
    A reference to them is not a cross-module edge and compiling them in a second DLL is not a dead include."""
    raw = tree.read('development/module_registry.json')
    try:
        return set(json.loads(raw.decode('utf-8-sig')).get('unity', {}).get('shared', {})) if raw else set()
    except ValueError:
        return set()


def analyse(root, hooks=True, use_cache=True, policy=None):
    t0 = time.time()
    root = os.path.abspath(root)
    tree = X.FsTree(root)
    man = Manifest(tree)
    pol = policy or load_policy(root)
    srcs = X.load_sources(tree)
    shared = shared_files(tree)
    outdir = os.path.join(root, OUT_REL)
    files_all = tracked(root)
    h = hashlib.sha1()
    h.update(('v%d' % VERSION).encode())
    for me in (__file__, X.__file__):
        with open(me, 'rb') as mf:
            h.update(sha1(mf.read()).encode())
    for fn in sorted(srcs):
        h.update(fn.encode())
        h.update(sha1(srcs[fn].text.encode('utf-8', 'replace')).encode())
    for rel in ('development/features.json', 'development/module_registry.json', 'development/hook_sites.json', 'development/hook_policy.json', 'tools/build_split.ps1', POLICY_REL,
                'tools/check_feature.ps1', 'src/game_addresses_data.inl'):
        b = tree.read(rel)
        h.update(rel.encode())
        h.update(sha1(b).encode() if b else b'-')
    for rel in files_all:                                  # D8 / reachability inputs: size + mtime only
        try:
            st = os.stat(os.path.join(root, rel))
            h.update(('%s|%d|%d' % (rel, st.st_size, st.st_mtime_ns)).encode())
        except OSError:
            h.update(rel.encode())
    h.update(b'hooks' if hooks else b'nohooks')
    key = h.hexdigest()
    rpath = os.path.join(outdir, 'result_cache.pkl')
    if use_cache:
        try:
            with open(rpath, 'rb') as f:
                rc = pickle.load(f)
            if rc.get('key') == key:
                rc['result']['timing'] = {'total_s': round(time.time() - t0, 2), 'cache': 'hit'}
                return rc['result']
        except Exception:
            pass
    variants = X.compute_variants(srcs, tree)
    t_var = time.time() - t0
    cpath = os.path.join(outdir, 'scan_cache.pkl')
    cache = load_cache(cpath) if use_cache else {'version': VERSION, 'files': {}}
    scans, hit, miss = {}, 0, 0
    for fn, s in srcs.items():
        if fn not in variants:
            continue
        union = set()
        for ls in variants[fn].values():
            union |= ls
        k = scan_stamp() + sha1(s.text.encode('utf-8', 'replace')) + ':' + sha1(','.join(map(str, sorted(union))).encode())
        ent = cache['files'].get(fn)
        if ent and ent[0] == k:
            scans[fn] = ent[1]
            hit += 1
        else:
            scans[fn] = scan_file(s.text, union)
            cache['files'][fn] = (k, scans[fn])
            miss += 1
    if miss and use_cache:
        cache['files'] = {k2: v for k2, v in cache['files'].items() if k2 in srcs}
        write_cache(cpath, cache)
    t_scan = time.time() - t0

    generic = set(pol.get('generic_names') or [])
    fmod = {fn: man.owner('src/' + fn) for fn in scans}
    allnames = set()
    for fn, sc in scans.items():
        for d in sc['defs']:
            allnames.add(d[0].rsplit('::', 1)[-1])
    edges = collections.defaultdict(lambda: {'syms': {}, 'dlls': set(), 'kinds': set()})
    ambiguous = 0
    decl_hits = []

    def add_edge(a, b, kind, dll, sym, fn, line, cnt=1):
        e = edges[(a, b)]
        e['kinds'].add(kind)
        e['dlls'].add(dll)
        s = e['syms'].setdefault(sym, [0, fn, line])
        s[0] += cnt

    imbalance = {fn: sc['imbalance'] for fn, sc in scans.items() if sc['imbalance']}
    for dll in REAL_DLLS:
        reached = [fn for fn in scans if dll in variants.get(fn, {})]
        if not reached:
            continue
        table = collections.defaultdict(list)
        for fn in reached:
            act = variants[fn][dll]
            # a .cpp that includes no .inl is its own translation unit (gpu_profile.cpp, graphics_proxy.cpp): its file-scope definitions are
            # internal (anonymous namespace) or declared in a header, so only the header declarations resolve a reference elsewhere
            private_tu = fn.endswith('.cpp') and not any(t.endswith('.inl') for t, _l in scans[fn]['incs'])
            for name, kind, line, end in scans[fn]['defs']:
                if private_tu and kind != 'macro':
                    continue
                if kind == 'macro' or line in act or end in act:
                    table[name].append((fn, line, kind, end))
        for fn in reached:
            sc = scans[fn]
            act = variants[fn][dll]
            me = fmod[fn]
            usings = [u for u in sc['usings'] if not u.startswith('@')]
            using_decl = {u[1:].rsplit('::', 1)[-1]: u[1:] for u in sc['usings'] if u.startswith('@')}
            for (name, ci, fl), lines in sc['refs'].items():
                last = name.rsplit('::', 1)[-1]
                if last not in allnames:
                    continue
                n_act = [l for l in lines if l in act]
                if not n_act:
                    continue
                chain = sc['chains'][ci]
                cands = None
                for k in range(len(chain), -1, -1):
                    c = '::'.join(chain[:k]) + ('::' if k else '') + name
                    if c in table:
                        cands = (c, table[c])
                        break
                if cands is None and name in using_decl and using_decl[name] in table:
                    cands = (using_decl[name], table[using_decl[name]])
                if cands is None:
                    for u in usings:
                        c = u + '::' + name
                        if c in table:
                            cands = (c, table[c])
                            break
                if cands is None:
                    continue
                cname, ents = cands
                real = [e for e in ents if e[2] != 'decl']
                if not real:
                    real = ents
                usable = [e for e in real if fl or e[2] not in ('func', 'decl')]
                if not usable:
                    continue
                if all(e[0] in shared for e in usable):
                    continue
                if last in generic and '::' not in name and len(usable) > 1:
                    continue
                mods = {fmod.get(e[0], '?') for e in usable}
                if len(mods) > 1:
                    ambiguous += 1
                    continue
                to = mods.pop()
                if to == me:
                    continue
                add_edge(me, to, 'symbol', dll, cname, fn, n_act[0], len(n_act))
            for tgt, line in sc['incs']:
                if tgt in scans and tgt not in shared and line >= 0 and dll in variants.get(tgt, {}):
                    to = fmod[tgt]
                    if to != me:
                        add_edge(me, to, 'include', dll, '#include ' + tgt, fn, line)
            for flag, line in sc['flags']:
                if line in act:
                    ow = flag_owner(flag, man, pol)
                    if ow and ow != me:
                        add_edge(me, ow, 'flag', dll, 'mod_features.' + flag, fn, line)
            for name, kind, line, end in sc['defs']:
                if kind == 'decl' and line in act:
                    ents = [e for e in table.get(name, []) if e[2] in ('func',)]
                    if ents:
                        to = fmod.get(ents[0][0], '?')
                        if to != me:
                            decl_hits.append((me, to, name, fn, line, dll))
    # --- hook_map site sharing
    hook_note = ''
    sites_by_feature = {}
    ownmap = {}
    for n_, m_ in man.mods.items():
        for o_ in m_.get('hook_owners') or []:
            ownmap.setdefault(o_, n_)
    for n_ in man.mods:                      # owner label == module id with '_' for '-' (sortie, path_fast, face_keep ...)
        ownmap.setdefault(n_.replace('-', '_'), n_)
    if hooks and tree.read('src/game_addresses_data.inl'):
        try:
            mp = X.build_map(tree)
            builds = mp.ga.builds
            grp = collections.defaultdict(set)
            for st in mp.sites:
                if st.get('dll') == 'unreached':
                    continue
                feat = ownmap.get(st['owner'], '')
                if not feat:
                    continue
                keys = []
                if st.get('key'):
                    keys.append(('slot' if st.get('kind') == 'vt' else 'site') + ' ' + st['key'])
                rv = (st.get('rva') or {}).get(builds[-1] if builds else '')
                if rv is not None and st.get('kind') != 'vt' and st.get('len'):
                    for b in range(rv // 16, (rv + max(1, st['len']) - 1) // 16 + 1):
                        keys.append('block 0x%X' % (b * 16))
                for k2 in keys:
                    grp[k2].add((feat, st['src']))
            for k2, items in grp.items():
                fs = sorted({f for f, _ in items})
                if len(fs) > 1:
                    for i in range(len(fs)):
                        for j in range(len(fs)):
                            if i != j:
                                src = sorted(s for f, s in items if f == fs[i])[0]
                                fn_, _, ln_ = src.partition(':')
                                add_edge(fs[i], fs[j], 'hook', 'hook', k2, os.path.basename(fn_), int(ln_ or 0))
            sites_by_feature = {'sites': len(mp.sites)}
        except Exception as e:                      # noqa: BLE001
            hook_note = 'hook edges skipped: %s' % e
    elif hooks:
        hook_note = 'hook edges skipped: no src/game_addresses_data.inl'

    # --- classification
    modules = {}
    for n, m in man.mods.items():
        modules[n] = {'dll': m.get('dll', ''), 'status': m.get('status', ''), 'service': is_service(n, man, pol), 'depends': list(m.get('depends') or [])}
    modules.setdefault('?', {'dll': '', 'status': '', 'service': False, 'depends': []})
    edge_list = []
    findings = []
    unowned_files = sorted(fn for fn in scans if fmod[fn] == '?')
    for (a, b), e in sorted(edges.items()):
        if a == '?' or b == '?':
            edge_list.append({'from': a, 'to': b, 'kinds': sorted(e['kinds']), 'dlls': sorted(e['dlls']), 'declared': None, 'class': 'unowned',
                              'symbols': {k: v for k, v in e['syms'].items()}})
            continue
        sa, sb = is_service(a, man, pol), is_service(b, man, pol)
        declared = b in man.closure(a)
        cls = 'ok'
        kinds = e['kinds'] - {'hook'}
        if sa:
            cls = 'hub-out'
        elif sb:
            cls = 'service'
        elif kinds:
            if not declared:
                cls = 'D1'
            if man.dll(a) != man.dll(b) and man.dll(a) in REAL_DLLS and man.dll(b) in REAL_DLLS:
                findings.append({'rule': 'D2', 'from': a, 'to': b, 'dlls': (man.dll(a), man.dll(b)),
                                 'symbols': {k: v for k, v in e['syms'].items() if not k.startswith(('slot ', 'site ', 'block '))}})
            if 'flag' in kinds and not declared:
                findings.append({'rule': 'D5', 'from': a, 'to': b, 'symbols': {k: v for k, v in e['syms'].items() if k.startswith('mod_features.')}})
        if 'hook' in e['kinds'] and not sa and not sb:
            findings.append({'rule': 'HOOK', 'from': a, 'to': b, 'symbols': {k: v for k, v in e['syms'].items() if k.startswith(('slot ', 'site ', 'block '))}})
        edge_list.append({'from': a, 'to': b, 'kinds': sorted(e['kinds']), 'dlls': sorted(e['dlls']), 'declared': declared, 'class': cls,
                          'symbols': {k: v for k, v in e['syms'].items()}})
    for ed in edge_list:
        if ed['class'] == 'D1':
            sy = {k: v for k, v in ed['symbols'].items() if not k.startswith(('slot ', 'site ', 'block '))}
            if sy:
                findings.append({'rule': 'D1', 'from': ed['from'], 'to': ed['to'], 'symbols': sy})
    # D4: forward declarations of another module's function
    d4 = collections.defaultdict(dict)
    for me, to, name, fn, line, dll in decl_hits:
        if to == '?' or me == '?' or is_service(to, man, pol) or is_service(me, man, pol):
            continue
        d4[(me, to)].setdefault(name, [1, fn, line])
    for (a, b), sy in d4.items():
        findings.append({'rule': 'D4', 'from': a, 'to': b, 'symbols': sy})
    # D3: files compiled into another DLL than their module's
    d3 = []
    for fn in sorted(scans):
        m = fmod[fn]
        if m == '?' or is_service(m, man, pol) or fn in shared:
            continue
        od = man.dll(m)
        if od in ('convenience', 'ccz', 'layout'):
            for d in variants.get(fn, {}):
                if d == 'combat':
                    d3.append({'file': fn, 'module': m, 'owner_dll': od, 'compiled_in': d})
    d3b = [fn for fn in sorted(scans) if fmod[fn] not in ('?',) and not is_service(fmod[fn], man, pol) and man.dll(fmod[fn]) == 'combat'
           and 'convenience' in variants.get(fn, {})]
    # D5 note: D1 for flag-only edges is reported separately
    # D6: direct patch calls outside services
    d6 = collections.Counter()
    for fn, sc in scans.items():
        m = fmod[fn]
        if m == '?' or is_service(m, man, pol):
            continue
        acts = set()
        for d, ls in variants.get(fn, {}).items():
            acts |= ls
        n = len([1 for _c, ln in sc['d6'] if ln in acts])
        if n:
            d6[m] += n
    # D7: NEED_WHITELIST rows only from convenience / ccz / combat
    d7 = []
    if hooks and not hook_note.startswith('hook edges skipped'):
        regs = collections.defaultdict(list)
        for r in mp.registrations:
            if r.get('key'):
                regs[r['key']].append(r)
        for st in mp.sites:
            if 'NEED_WHITELIST' in (st.get('flags') or []) and st.get('dll') != 'unreached':
                feat = ownmap.get(st['owner'], '')
                od = man.dll(feat) if feat else ''
                dl = {d for r in regs.get(st['key'], []) for d in r['dlls']}
                if (od and od not in WHITELIST_DLLS) or (dl and not dl <= WHITELIST_DLLS):
                    d7.append({'site': st['key'], 'owner': st['owner'], 'feature': feat, 'owner_dll': od, 'registered_in': sorted(dl)})
    # D10: SCCs over feature modules
    graph = collections.defaultdict(set)
    undecl = collections.defaultdict(set)
    for ed in edge_list:
        if ed['class'] in ('unowned', 'service', 'hub-out') or not (set(ed['kinds']) - {'hook'}):
            continue
        graph[ed['from']].add(ed['to'])
        graph.setdefault(ed['to'], set())
        if not ed['declared']:
            undecl[ed['from']].add(ed['to'])
    sccs = [sorted(s) for s in tarjan(graph) if len(s) > 1]
    sccs = [s for s in sccs if any(undecl[a] & set(s) for a in s)]
    sccs.sort()
    legacy, legacy_detail = legacy_counters(root, files_all)
    reach = reachability(root, files_all, man, pol.get('entry_tools'))
    counters = {
        'd1_edges': len({(f['from'], f['to']) for f in findings if f['rule'] == 'D1'}),
        'd1_symbols': sum(len(f['symbols']) for f in findings if f['rule'] == 'D1'),
        'd2_edges': len([f for f in findings if f['rule'] == 'D2']),
        'd3_dead_include_files': len(d3),
        'd4_decl_pairs': len([f for f in findings if f['rule'] == 'D4']),
        'd4_decl_symbols': sum(len(f['symbols']) for f in findings if f['rule'] == 'D4'),
        'd5_flag_edges': len([f for f in findings if f['rule'] == 'D5']),
        'd6_direct_patch_calls': sum(d6.values()),
        'd10_undeclared_sccs': len(sccs),
        'hook_shared_pairs': len([f for f in findings if f['rule'] == 'HOOK']),
        'unowned_src_files': len(unowned_files),
        'tools_unreachable': len([f for f in reach['unreachable'] if f.startswith('tools/')]),
        'tests_unreachable': len([f for f in reach['unreachable'] if f.startswith('tests/')]),
    }
    for m, n in d6.items():
        counters['d6:' + m] = n
    counters.update(legacy)
    for k2 in ('old_build_1fbe6be7_files', 'old_build_1fbe6be7_mentions', 'old_image_path_files', 'ga_expand_files', 'missing_doc_refs'):
        counters.setdefault(k2, 0)
    res = {'schema': 1, 'generated': time.strftime('%Y-%m-%d %H:%M:%S'), 'root': root, 'modules': modules, 'edges': edge_list,
           'findings': findings, 'd3': d3, 'd3_combat_in_convenience': d3b, 'd7': d7, 'sccs': sccs, 'counters': counters,
           'legacy_detail': legacy_detail, 'reach': reach, 'unowned_files': unowned_files, 'ambiguous_refs': ambiguous,
           'imbalance': imbalance, 'hook_note': hook_note, 'hook_info': sites_by_feature,
           'defs': {fn: [(d[0], d[1], d[2], d[3]) for d in sc['defs']] for fn, sc in scans.items()},
           'file_module': fmod, 'variants': {fn: sorted(v) for fn, v in variants.items()},
           'files': {'scanned': len(scans), 'cache_hit': hit, 'cache_miss': miss},
           'timing': {'variants_s': round(t_var, 2), 'scan_s': round(t_scan - t_var, 2), 'total_s': round(time.time() - t0, 2), 'cache': 'miss'}}
    res['stamp'] = key
    if use_cache:
        try:
            write_cache(rpath, {'key': key, 'result': res})
        except OSError:
            pass
    return res


def tarjan(graph):
    idx, low, on, st, out, c = {}, {}, set(), [], [], [0]
    sys.setrecursionlimit(10000)

    def sc(v):
        idx[v] = low[v] = c[0]
        c[0] += 1
        st.append(v)
        on.add(v)
        for w in sorted(graph.get(v, ())):
            if w not in idx:
                sc(w)
                low[v] = min(low[v], low[w])
            elif w in on:
                low[v] = min(low[v], idx[w])
        if low[v] == idx[v]:
            comp = []
            while True:
                w = st.pop()
                on.discard(w)
                comp.append(w)
                if w == v:
                    break
            out.append(comp)
    for v in sorted(graph):
        if v not in idx:
            sc(v)
    return out


# ------------------------------------------------------------------ outputs
def write_outputs(root, res):
    d = os.path.join(root, OUT_REL)
    os.makedirs(d, exist_ok=True)
    slim = {k: v for k, v in res.items() if k not in ('defs', 'variants', 'file_module', 'stamp')}
    with open(os.path.join(d, 'edges.json'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(slim, f, ensure_ascii=False, indent=1)
    with open(os.path.join(d, 'edges.tsv'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('from\tto\tkinds\tdlls\tdeclared\tclass\tsymbols\ttop_symbols\n')
        for e in res['edges']:
            top = sorted(e['symbols'].items(), key=lambda kv: -kv[1][0])[:5]
            f.write('%s\t%s\t%s\t%s\t%s\t%s\t%d\t%s\n' % (e['from'], e['to'], ','.join(e['kinds']), ','.join(e['dlls']), e['declared'], e['class'],
                                                         len(e['symbols']), ' '.join(k for k, _ in top)))
    with open(os.path.join(d, 'edges.dot'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('digraph feature_deps {\n  rankdir=LR; node [shape=box,fontsize=10];\n')
        for n, m in sorted(res['modules'].items()):
            if m['service']:
                f.write('  "%s" [style=dashed];\n' % n)
        for e in res['edges']:
            if e['class'] == 'unowned' or e['class'] == 'hub-out' or e['to'] == '?':
                continue
            style = ' [color=red]' if e['class'] == 'D1' else ' [color=gray]' if e['class'] == 'service' else ''
            if e['kinds'] == ['hook']:
                style = ' [style=dotted,color=blue]'
            f.write('  "%s" -> "%s"%s;\n' % (e['from'], e['to'], style))
        f.write('}\n')


def violations(res, pol):
    """compare with the ratchet baseline -> (errors, warnings)."""
    errs, warns = [], []
    for d in res['d7']:
        errs.append('D7 NEED_WHITELIST row %s (%s/%s) registered outside convenience/ccz/combat: %s' % (d['site'], d['owner'], d['owner_dll'], d['registered_in']))
    # W8 strict mode: these rules must be empty, even if an old baseline still lists the edge (the W6/W4 waves ended).
    for rule in pol.get('strict_rules') or []:
        if rule == 'D10':
            for s in res['sccs']:
                errs.append('strict D10 undeclared SCC: %s' % ', '.join(s))
            continue
        for f in res['findings']:
            if f['rule'] == rule:
                sy = sorted(f['symbols'], key=lambda s: -f['symbols'][s][0])[:4]
                errs.append('strict %s edge %s -> %s (%d symbols: %s)' % (rule, f['from'], f['to'], len(f['symbols']), ', '.join(sy)))
    base = {(e['rule'], e['from'], e['to']): e for e in pol['edges']}
    cur = {}
    for f in res['findings']:
        cur[(f['rule'], f['from'], f['to'])] = f
    for k, f in sorted(cur.items()):
        if k not in base:
            sy = sorted(f['symbols'], key=lambda s: -f['symbols'][s][0])[:4]
            errs.append('%s new edge %s -> %s (%d symbols: %s)' % (k[0], k[1], k[2], len(f['symbols']), ', '.join(sy)))
        else:
            old = set(base[k].get('symbols') or [])
            if base[k].get('symbol_count', len(old)) < len(f['symbols']) and len(old) < len(f['symbols']):
                warns.append('%s %s -> %s grew: %d -> %d symbols' % (k[0], k[1], k[2], base[k].get('symbol_count', len(old)), len(f['symbols'])))
    for k in sorted(set(base) - set(cur)):
        warns.append('stale baseline edge %s %s -> %s (run --tighten)' % k)
    for name, v in sorted(res['counters'].items()):
        b = pol['counters'].get(name)
        if b is None:
            if v:
                errs.append('counter %s=%d has no baseline entry (run --baseline after review)' % (name, v))
        elif v > b:
            errs.append('counter %s increased %d -> %d' % (name, b, v))
        elif v < b:
            warns.append('counter %s decreased %d -> %d (run --tighten)' % (name, b, v))
    bs = [set(s['modules']) for s in pol['sccs']]
    for s in res['sccs']:
        if not any(set(s) <= b for b in bs):
            errs.append('D10 new undeclared SCC: %s' % ', '.join(s))
    known = set(pol.get('tools_baseline') or [])
    for f in res['reach']['unreachable']:
        if f not in known:
            errs.append('new unreachable file %s (not referenced from any route or entry script)' % f)
    if res['imbalance']:
        warns.append('brace walker imbalance in %s' % ', '.join(sorted(res['imbalance'])))
    if res['hook_note']:
        warns.append(res['hook_note'])
    return errs, warns


def make_baseline(res, pol, shrink_only=False):
    new = dict(pol)
    cur = {(f['rule'], f['from'], f['to']): f for f in res['findings']}
    old = {(e['rule'], e['from'], e['to']): e for e in pol['edges']}
    edges = []
    for k, f in cur.items():
        if shrink_only and k not in old:
            continue
        syms = sorted(f['symbols'], key=lambda s: -f['symbols'][s][0])
        if k in old:
            e = dict(old[k])
            if shrink_only:
                e['symbols'] = [s for s in e.get('symbols', []) if s in f['symbols']]
                e['symbol_count'] = min(e.get('symbol_count', len(f['symbols'])), len(f['symbols']))
            else:
                e['symbols'] = syms[:10]
                e['symbol_count'] = len(syms)
        else:
            e = {'rule': k[0], 'from': k[1], 'to': k[2], 'symbols': syms[:10], 'symbol_count': len(syms),
                 'reason': 'W0 baseline (%s): existing coupling listed in docs/MODULARIZATION_PLAN.md section 1.3' % now_date(),
                 'date': now_date(), 'until_wave': WAVES.get(k[0], 'W6')}
        edges.append(e)
    new['edges'] = edges
    if shrink_only:
        new['counters'] = {k: min(v, pol['counters'].get(k, v)) for k, v in res['counters'].items() if k in pol['counters']}
        new['sccs'] = [s for s in pol['sccs'] if any(set(x) <= set(s['modules']) for x in res['sccs'])]
        new['tools_baseline'] = [f for f in pol.get('tools_baseline', []) if f in set(res['reach']['unreachable'])]
    else:
        new['counters'] = dict(res['counters'])
        new['sccs'] = [{'modules': s, 'reason': 'W0 baseline (%s)' % now_date(), 'until_wave': 'W6'} for s in res['sccs']]
        new['tools_baseline'] = list(res['reach']['unreachable'])
        new['baseline_date'] = now_date()
    return new


def summary_lines(res):
    c = res['counters']
    by = collections.Counter(f['rule'] for f in res['findings'])
    lines = ['feature_deps: %d modules, %d edges, %d files scanned (cache hit %d miss %d), %s in %.1fs' % (
        len(res['modules']), len(res['edges']), res['files']['scanned'], res['files']['cache_hit'], res['files']['cache_miss'],
        res['timing']['cache'], res['timing']['total_s'])]
    lines.append('  rules: D1 %d edges/%d symbols, D2 %d, D3 %d files, D4 %d pairs/%d decls, D5 %d, D6 %d calls, D7 %d, D10 %d SCC, hook-shared %d pairs' % (
        c['d1_edges'], c['d1_symbols'], c['d2_edges'], c['d3_dead_include_files'], c['d4_decl_pairs'], c['d4_decl_symbols'], c['d5_flag_edges'],
        c['d6_direct_patch_calls'], len(res['d7']), c['d10_undeclared_sccs'], c['hook_shared_pairs']))
    lines.append('  D8: 1fbe6be7 %d files/%d mentions, old image paths %d files, ga_expand %d files, missing doc refs %d' % (
        c.get('old_build_1fbe6be7_files', 0), c.get('old_build_1fbe6be7_mentions', 0), c.get('old_image_path_files', 0), c.get('ga_expand_files', 0),
        c.get('missing_doc_refs', 0)))
    lines.append('  reach: %d/%d tools+tests reachable, %d unreachable (tools %d, tests %d), %d doc-only; unowned src %d; ambiguous refs %d' % (
        res['reach']['reachable'], res['reach']['scope'], len(res['reach']['unreachable']), c['tools_unreachable'], c['tests_unreachable'],
        len(res['reach']['doc_only']), c['unowned_src_files'], res['ambiguous_refs']))
    return lines


# ------------------------------------------------------------------ mod compatibility (reused by tools/mod_compat.py later)
def compat(root, mod_id, res=None):
    """Compare one module (a future independent mod; same id as features.json / modules/<id>/module.json) with all the others.
    Returns {'ok', 'module', 'conflicts': [{kind, other, symbols, ...}], 'affected_modules', 'affected_checks'}; read-only."""
    res = res or analyse(root)
    mods = res['modules']
    if mod_id not in mods:
        return {'ok': False, 'module': mod_id, 'error': 'unknown module id', 'conflicts': [], 'affected_modules': [], 'affected_checks': []}
    man = Manifest(X.FsTree(root))
    pol = load_policy(root)
    base = {(e['rule'], e['from'], e['to']) for e in pol['edges']}
    kindname = {'symbol': 'symbol', 'include': 'foreign-include', 'flag': 'foreign-flag', 'hook': 'hook-site-overlap'}
    conflicts = []
    for e in res['edges']:
        if mod_id not in (e['from'], e['to']) or '?' in (e['from'], e['to']):
            continue
        other = e['to'] if e['from'] == mod_id else e['from']
        for kind in e['kinds']:
            if kind == 'hook':
                sy = [k for k in e['symbols'] if k.startswith(('slot ', 'site ', 'block '))]
            elif kind == 'include':
                sy = [k for k in e['symbols'] if k.startswith('#include')]
            elif kind == 'flag':
                sy = [k for k in e['symbols'] if k.startswith('mod_features.')]
            else:
                sy = [k for k in e['symbols'] if not k.startswith(('slot ', 'site ', 'block ', '#include', 'mod_features.'))]
            if not sy:
                continue
            conflicts.append({'kind': kindname[kind], 'direction': 'uses' if e['from'] == mod_id else 'used_by', 'other': other,
                              'declared': e['declared'], 'class': e['class'], 'symbols': sorted(sy)[:12], 'symbol_count': len(sy),
                              'baselined': any((r, e['from'], e['to']) in base for r in ('D1', 'D2', 'D4', 'D5', 'HOOK'))})
    for f in res['d3']:
        if f['module'] == mod_id:
            conflicts.append({'kind': 'dead-include', 'other': f['compiled_in'], 'symbols': [f['file']]})
    for s in res['sccs']:
        if mod_id in s:
            conflicts.append({'kind': 'dependency-cycle', 'other': ','.join(x for x in s if x != mod_id), 'symbols': []})
    others = sorted({c['other'] for c in conflicts if c['other'] in mods} | {m for m in mods if mod_id in man.closure(m)})
    checks = []
    for m in [mod_id] + others:
        for t in (man.mods.get(m) or {}).get('tests') or []:
            if t not in checks:
                checks.append(t)
    bad = [c for c in conflicts if c['kind'] in ('hook-site-overlap', 'dependency-cycle') and not c.get('baselined', False)]
    return {'ok': not bad, 'module': mod_id, 'conflicts': conflicts, 'affected_modules': others,
            'affected_checks': checks + list(man.data.get('always') or [])}


# ------------------------------------------------------------------ commands
def cmd_explain(res, mod):
    if mod not in res['modules']:
        print('unknown module', mod)
        return 2
    m = res['modules'][mod]
    files = sorted(fn for fn, o in res['file_module'].items() if o == mod)
    print('%s  dll=%s status=%s service=%s depends=%s' % (mod, m['dll'], m['status'], m['service'], m['depends']))
    print('files (%d): %s' % (len(files), ' '.join(files)))
    for s in res['sccs']:
        if mod in s:
            print('SCC:', ', '.join(s))
    d3 = [d['file'] + '->' + d['compiled_in'] for d in res['d3'] if d['module'] == mod]
    if d3:
        print('dead includes:', ' '.join(d3))
    for title, sel in (('out', lambda e: e['from'] == mod), ('in', lambda e: e['to'] == mod)):
        print('--', title)
        for e in res['edges']:
            if sel(e):
                top = sorted(e['symbols'].items(), key=lambda kv: -kv[1][0])[:6]
                other = e['to'] if title == 'out' else e['from']
                print('  %s %-22s %-7s %-9s declared=%s %d sym: %s' % ('->' if title == 'out' else '<-', other, ','.join(e['kinds']), e['class'], e['declared'],
                                                                    len(e['symbols']), ' '.join(k for k, _ in top)))
    return 0


def changed_lines(root, rel, base):
    args = ['git', '-C', root, 'diff', '-U0']
    if base:
        args.append(base)
    args += ['--', rel]
    out = subprocess.run(args, capture_output=True, text=True, encoding='utf-8', errors='replace').stdout
    lines = set()
    for m in re.finditer(r'^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@', out, re.M):
        s, n = int(m.group(1)), int(m.group(2) if m.group(2) is not None else 1)
        lines.update(range(s, s + max(n, 1)))
    return lines


def cmd_impact(root, res, rel, hunks, base):
    out = compute_impact(root, res, rel, hunks, base)
    print(json.dumps(out, ensure_ascii=False))
    return 0


def compute_impact(root, res, rel, hunks, base):
    """Modules affected by a change of one file: {'file','owner','impacted':[{module,via,symbols}],'mode',...} (select_checks 3-1)."""
    rel = rel.replace('\\', '/')
    fn = os.path.basename(rel) if rel.startswith('src/') else rel
    owner = res['file_module'].get(fn) if rel.startswith('src/') else None
    out = {'file': rel, 'owner': owner, 'impacted': [], 'mode': 'unknown file'}
    if owner is None:
        return out
    svc = res['modules'].get(owner, {}).get('service')
    syms = None
    if hunks or svc:
        defs = res['defs'].get(fn, [])
        if hunks:
            ch = changed_lines(root, rel, base)
            inside, outside = set(), False
            for ln in ch:
                hit = [d for d in defs if d[2] <= ln <= d[3] and d[1] != 'macro']
                if hit:
                    inside.update(d[0] for d in hit)
                else:
                    outside = True
            syms = None if outside or not ch else inside
            out['mode'] = 'hunks' + ('' if syms is not None else ' (outside definitions: whole file)')
            out['changed_symbols'] = sorted(syms) if syms is not None else None
        else:
            syms = {d[0] for d in defs if d[1] != 'macro'}
            out['mode'] = 'service file: referencing modules of its definitions'
    else:
        out['mode'] = 'feature file: all modules with edges into the owner'
    seen = {}
    for e in res['edges']:
        if e['to'] != owner or e['from'] in ('?', owner):
            continue
        use = {k: v for k, v in e['symbols'].items() if syms is None or k in syms or k.startswith(('slot ', 'site ', 'block ', 'mod_features.'))}
        if syms is not None and not use:
            continue
        seen[e['from']] = {'module': e['from'], 'via': sorted(e['kinds']), 'symbols': sorted(use)[:8]}
    for e in res['edges']:                               # hook sharing is symmetric: both sides are reported by the extractor
        pass
    out['impacted'] = [seen[k] for k in sorted(seen)]
    return out


def cmd_tools(res):
    r = res['reach']
    print('tools/tests files: %d, reachable from routes/entry scripts/entry_tools: %d, unreachable: %d (of which mentioned in docs: %d)' % (
        r['scope'], r['reachable'], len(r['unreachable']), len(r['doc_only'])))
    print('-- unreachable (no route, no executable reference, not an entry_tools item)')
    for f in r['unreachable']:
        print(f + ('   [mentioned in docs]' if f in set(r['doc_only']) else ''))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('cmd', nargs='?', default='scan', choices=['scan', 'explain', 'impact', 'tools', 'compat'])
    ap.add_argument('arg', nargs='?')
    ap.add_argument('--root', default=os.path.dirname(HERE))
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--baseline', action='store_true')
    ap.add_argument('--tighten', action='store_true')
    ap.add_argument('--no-hooks', action='store_true')
    ap.add_argument('--no-cache', action='store_true')
    ap.add_argument('--hunks', action='store_true')
    ap.add_argument('--base')
    ap.add_argument('--json', action='store_true')
    a = ap.parse_args(argv)
    root = os.path.abspath(a.root)
    pol = load_policy(root)
    res = analyse(root, hooks=not a.no_hooks, use_cache=not a.no_cache, policy=pol)
    if a.cmd == 'explain':
        return cmd_explain(res, a.arg or '')
    if a.cmd == 'impact':
        return cmd_impact(root, res, a.arg or '', a.hunks, a.base)
    if a.cmd == 'tools':
        return cmd_tools(res)
    if a.cmd == 'compat':
        r = compat(root, a.arg or '', res)
        print(json.dumps(r, ensure_ascii=False, indent=1))
        return 0 if r.get('ok') else 1
    write_outputs(root, res)
    if a.baseline or a.tighten:
        save_policy(root, make_baseline(res, pol, shrink_only=a.tighten and not a.baseline))
        print('policy written: %s (%s)' % (POLICY_REL, 'baseline' if a.baseline else 'tighten'))
        pol = load_policy(root)
    for l in summary_lines(res):
        print(l)
    if a.check or a.baseline or a.tighten:
        errs, warns = violations(res, pol)
        for w in warns[:12]:
            print('  warn:', w)
        if len(warns) > 12:
            print('  warn: ... %d more' % (len(warns) - 12))
        for e in errs[:40]:
            print('  FAIL:', e)
        if len(errs) > 40:
            print('  FAIL: ... %d more' % (len(errs) - 40))
        print('feature_deps %s (%d errors, %d warnings), outputs in %s' % ('PASSED' if not errs else 'FAILED', len(errs), len(warns), OUT_REL))
        return 1 if errs and a.check else 0
    return 0


if __name__ == '__main__':
    sys.exit(main())
