#!/usr/bin/env python
"""Input set and cache key of one check route (docs/HISTORY.md 3).

A route is a branch of tools/check_feature.ps1 (or a routes.<name>.runner of
development/features.json). Its inputs are every repo file the branch can read:
nested scripts, C/C++ include closure, local Python imports. External data
(research/, legacy/, ...) is stamped by size and mtime instead of hashed.
Standard library only. Read-only.
"""
import argparse
import contextlib
import glob as _glob
import hashlib
import json
import os
import re
import stat
import sys

PS1_REL = 'tools/check_feature.ps1'
EXTERNAL_PREFIXES = ('research/', 'legacy/', 'build/ui_native_analysis', 'backup/', 'data/', 'dist/')   # stamped by size/mtime, never content-hashed (W0: hashing ~4000 backup/data/dist files made -List take 65-330 s)
SKIP_PREFIX = ('build/', '.git/')
LEAF_EXT = ('.ps1', '.py', '.cpp', '.c', '.h', '.hpp', '.inl', '.cs', '.json', '.def', '.rc', '.txt', '.tsv', '.csv', '.md', '.bin')
CODE_EXT = ('.cpp', '.c', '.h', '.hpp', '.inl')
BARE_PATH = re.compile(r'(?<![\w.])((?:src|tests|tools|distribution|installer|development)[\\/][\w.\\/*-]*[\w*])')
EXT_PATH = re.compile(r'((?:research|legacy)[\\/][^\s\'"`)\]]*|build[\\/]ui_native_analysis[^\s\'"`)\]]*)')
STR_TOKEN = re.compile(r"'([^'\r\n]*)'|\"([^\"\r\n]*)\"")
JOIN_PATH = re.compile(r"Join-Path\s+(\$\w+|\([^)]*\))\s+['\"]([^'\"]+)['\"]")
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
PY_IMPORT = re.compile(r'^\s*(?:from|import)\s+([A-Za-z_][\w]*)', re.M)
VC_ASSIGN = re.compile(r"\$\w*[Vv][Cc]\w*\s*=\s*'([^']+)'")
TOOL_ASSIGN = re.compile(r"^\s*(?:\$\w*(?:[Vv][Cc]|[Ss][Dd][Kk]|[Vv]ersion)\w*\s*=.*)$", re.M)


# 2026-11-16 (검사 빨리, cache hole): a doc or request-list edit reran the two heaviest dll-split routes. Three over-collections:
# (1) comments were scanned as references (build_native_fixture.ps1 "DLLs come only from tools/build_split.ps1" pulled the whole
# build script closure), (2) cl steps followed build_split.ps1, whose own references are the fixture scripts its -Fixture mode runs,
# (3) Python tools (run_checks.py, unit_equivalence.py, hook_map.py ...) read build_split.ps1 as text, and every data-step route
# reached run_checks.py, which reads features.json only for this route's entry and imports select_checks.py only to record pass
# keys (select_checks -> feature_deps.py names whole top-level folders).
# A .ps1 named by a Python file that never starts powershell is a leaf: its content and toolchain lines are inputs, the files it
# names are not followed. DATA_READ: what one script does with another repo file it names: 'leaf' as above (run_checks.py
# starts powershell for ps1 steps but only reads build_split.ps1), 'entry' = only this route's entry is read (already in the
# key), 'record' = bookkeeping after the run, 'link' = only a link written into generated text (never opened).
DATA_READ = {
    'tools/run_checks.py': {'tools/build_split.ps1': 'leaf', 'development/features.json': 'entry', 'tools/select_checks.py': 'record'},
    'tools/gen_module_table.py': {'development/CURRENT.md': 'link'},   # docs/MODULES.md links to it; rewritten by every install
}
RUNS_PS = re.compile(r'powershell|pwsh', re.I)
RUNS_PY = re.compile(r'''sys\.executable|['"](?:python3?|py)(?:\.exe)?(?:['"]|\s+-)|runpy|import_module|spec_from_file_location|__import__|\bexec\(''', re.I)
STEP_RUNNER = 'tools/run_checks.py'   # executes data steps (args, cl command lines): its own code is an input of every step route
# A folder named as a whole (feature_deps.py 'development/', cpu_trace_fixture.ps1 Join-Path $sandbox 'tools') is walked or
# listed: its files' content stays an input, but they are not followed as if run (DIR_DATA, 2026-11-16).
SCRIPT_EXT = ('.py', '.ps1', '.cmd', '.bat')
BARE_SCRIPT = re.compile(r'^[\w.-]+\.(?:py|ps1)$')
# os.path.join(A.ROOT, 'build', 'ui_native_analysis', 'editor83_b25953386', 'game.exe.unpacked.exe'): the literal parts joined;
# an external path is stamped (check_addresses.py reads the unpacked EXE; only a coincidental chain used to stamp it).
OS_JOIN = re.compile(r'os\.path\.join\(([^()]*)\)')
LITERAL = re.compile(r"'([^'\r\n]*)'|\"([^\"\r\n]*)\"")
# run_checks.py turns the step argument @split_image into the fixture image build_split.ps1 passes to split_module_fixture.exe
SPLIT_IMAGE = re.compile(r"split_module_fixture\.exe'\s+\(Join-Path \$taskProject '([^']+)'\)")
LEAF_ONLY = 'tools/build_split.ps1'   # cl steps read flags/toolchain from it


def strip_ps1_comments(text):
    """PowerShell text with comments blanked (# outside strings at a token start, <# ... #> blocks); newlines are kept."""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '<' and text.startswith('<#', i):
            j = text.find('#>', i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r'[^\n]', ' ', text[i:j]))
            i = j
            continue
        if c == '@' and text[i + 1:i + 2] in ("'", '"') and text[i + 2:i + 3] in ('\r', '\n'):   # here-string
            q = text[i + 1]
            m = re.compile(r'^\s*' + q + '@', re.M).search(text, i + 2)
            j = n if not m else m.end()
            out.append(text[i:j])
            i = j
            continue
        if c in '\'"':
            j = i + 1
            while j < n and text[j] != c and text[j] != '\n':
                j += 2 if (c == '"' and text[j] == '`') else 1
            out.append(text[i:j + 1])
            i = j + 1
            continue
        if c == '#' and (i == 0 or text[i - 1] in ' \t\r\n;({}|'):
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def strip_py_comments(text):
    """Python text with # comments and docstrings (string-only statements) blanked, newlines kept (tokenize); the text as is
    when it does not tokenize. Strings inside code (paths a script opens) stay."""
    import io
    import tokenize
    try:
        toks = list(tokenize.generate_tokens(io.StringIO(text).readline))
    except (tokenize.TokenError, SyntaxError, IndentationError):
        return text
    skip = (tokenize.NL, tokenize.COMMENT)
    sig = [t for t in toks if t.type not in skip]
    blank = [t for t in toks if t.type == tokenize.COMMENT]
    i = 0
    while i < len(sig):
        t = sig[i]
        if t.type == tokenize.STRING and (i == 0 or sig[i - 1].type in (tokenize.NEWLINE, tokenize.INDENT, tokenize.DEDENT, tokenize.ENCODING)):
            j = i
            while j < len(sig) and sig[j].type == tokenize.STRING:
                j += 1
            if j >= len(sig) or sig[j].type in (tokenize.NEWLINE, tokenize.ENDMARKER):
                blank += sig[i:j]
            i = j
            continue
        i += 1
    if not blank:
        return text
    lines = text.splitlines(True)
    for t in blank:
        (r0, c0), (r1, c1) = t.start, t.end
        for r in range(r0, r1 + 1):
            ln = lines[r - 1]
            a = c0 if r == r0 else 0
            b = c1 if r == r1 else len(ln.rstrip('\r\n'))
            lines[r - 1] = ln[:a] + ' ' * (b - a) + ln[b:]
    return ''.join(lines)


def norm(p):
    p = p.replace('\\', '/')
    while p.startswith('./'):
        p = p[2:]
    return p


def read_text(path):
    try:
        with open(path, 'rb') as f:
            data = f.read()
    except OSError:
        return ''
    for enc in ('utf-8-sig', 'cp949', 'latin-1'):
        try:
            return data.decode(enc)
        except UnicodeDecodeError:
            continue
    return ''


# 2026-11-16 (검사 빨리): one process computes the keys of many routes over the same files; paths, directory walks and hashes
# are memoized for the life of the process (the tree does not change while keys are computed). Keys are unchanged.
_REL, _WALK, _EXPAND, _SHA = {}, {}, {}, {}
_NODE, _EXT = {}, {}   # per-file scan results (checked against the file's size/mtime); external stamps (only while frozen)
_FROZEN = [0]


@contextlib.contextmanager
def frozen():
    """Callers that key many routes at one moment (select_checks plan/routes, run_checks record) reuse external stamps;
    outside this block every key re-stamps them, so a long-lived process still sees later changes."""
    _FROZEN[0] += 1
    try:
        yield
    finally:
        _FROZEN[0] -= 1
        if not _FROZEN[0]:
            _EXT.clear()


def sha_file(path):
    try:
        st = os.stat(path)
    except OSError:
        return 'unreadable'
    k = (path, st.st_size, st.st_mtime_ns)
    v = _SHA.get(k)
    if v is None:
        v = _SHA[k] = _sha_file(path)
    return v


def _sha_file(path):
    h = hashlib.sha256()
    try:
        with open(path, 'rb') as f:
            for chunk in iter(lambda: f.read(1 << 20), b''):
                h.update(chunk)
    except OSError:
        return 'unreadable'
    return h.hexdigest()


def rel_to(root, path):
    k = (root, path)
    v = _REL.get(k)
    if v is None:
        v = _REL[k] = os.path.relpath(path, root).replace('\\', '/')
    return v


def strip_comment_and_find_block(text, route):
    """Return the text of the switch branch `'route' { ... }` (brace matched), or ''."""
    m = re.search(r"^[ \t]*['\"]%s['\"][ \t]*\{" % re.escape(route), text, re.M)
    if not m:
        return ''
    i = m.end() - 1
    depth = 0
    n = len(text)
    j = i
    while j < n:
        c = text[j]
        if c in '\'"':
            q = c
            j += 1
            while j < n and text[j] != q:
                if q == '"' and text[j] == '`':
                    j += 1
                j += 1
        elif c == '#':
            while j < n and text[j] != '\n':
                j += 1
            continue
        elif c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return text[m.start():j + 1]
        j += 1
    return text[m.start():]


def wildcard(s):
    return re.sub(r'\$\{[^}]*\}|\$\w+', '*', s)


class Collector:
    def __init__(self, root):
        self.root = root
        self.files = set()
        self.external = {}
        self.toolchain = []
        self.visited = set()

    def expand(self, candidate):
        """candidate: path relative to root (may hold *). Returns existing repo-relative files."""
        candidate = norm(candidate)
        k = (self.root, candidate)
        v = _EXPAND.get(k)
        if v is None:
            v = _EXPAND[k] = self._expand(candidate)
        return list(v)

    def _expand(self, candidate):
        if candidate.startswith('/') or re.match(r'^[A-Za-z]:', candidate) or '..' in candidate.split('/'):
            return []
        full = os.path.join(self.root, candidate)
        out = []
        if '*' in candidate:
            for hit in _glob.glob(full, recursive=True):
                out.extend(self.walk(hit))
        elif os.path.isdir(full):
            out.extend(self.walk(full))
        elif os.path.isfile(full):
            out.append(full)
        return out

    def walk(self, path):
        v = _WALK.get(path)
        if v is None:
            v = _WALK[path] = self._walk(path)
        return list(v)

    def _walk(self, path):
        if os.path.isfile(path):
            return [path]
        res = []
        for dp, dn, fn in os.walk(path):
            dn[:] = [d for d in dn if d not in ('__pycache__', '.git')]
            for f in fn:
                if '.bak' in f:
                    continue
                res.append(os.path.join(dp, f))
                if len(res) > 4000:
                    return res
        return res

    def stamp_external(self, rel):
        rel = norm(rel).rstrip('.,;:')
        if rel in self.external:
            return
        if not _FROZEN[0]:
            self.external[rel] = self._stamp(rel)
            return
        k = (self.root, rel)
        if k not in _EXT:
            _EXT[k] = self._stamp(rel)
        self.external[rel] = _EXT[k]

    def _stamp(self, rel):
        hits = []
        full = os.path.join(self.root, rel)
        if '*' in rel:
            hits = _glob.glob(full, recursive=True)
        elif os.path.exists(full):
            hits = [full]
        parts = []
        for h in sorted(hits)[:500]:
            if os.path.isdir(h):
                try:
                    names = sorted(os.listdir(h))[:500]
                except OSError:
                    names = []
                for nm in names:
                    p = os.path.join(h, nm)
                    try:
                        st = os.stat(p)
                        parts.append('%s:%d:%d' % (nm, st.st_size, int(st.st_mtime)))
                    except OSError:
                        pass
            else:
                try:
                    st = os.stat(h)
                    parts.append('%s:%d:%d' % (os.path.basename(h), st.st_size, int(st.st_mtime)))
                except OSError:
                    pass
        return hashlib.sha256('|'.join(parts).encode()).hexdigest() if parts else 'missing'

    def add_path(self, rel_full):
        rel = rel_to(self.root, rel_full)
        if rel.startswith('..'):
            return None
        if rel.startswith(EXTERNAL_PREFIXES):
            self.stamp_external(rel)
            return None
        if rel.startswith(SKIP_PREFIX) or '.bak' in rel or '__pycache__' in rel:
            return None
        if rel in self.files:
            return rel
        self.files.add(rel)
        return rel

    def scan_text(self, text, base_dir, label):
        """Find referenced paths in a script/branch text. base_dir = dir meaning $PSScriptRoot."""
        found = []
        listed = []   # files reached only through a whole folder: read or listed, never run (DIR_DATA)

        def take(cand):
            cand = norm(cand)
            (listed if '*' not in cand and os.path.isdir(os.path.join(self.root, cand)) else found).extend(self.expand(cand))
        # Join-Path $x 'rel'
        for m in JOIN_PATH.finditer(text):
            var, rel = m.group(1), m.group(2)
            rel = wildcard(rel)
            base = base_dir if var == '$PSScriptRoot' else self.root
            take(rel_to(self.root, os.path.join(base, norm(rel))))
        # quoted strings with root-relative or script-relative shape
        for m in STR_TOKEN.finditer(text):
            s = m.group(1) if m.group(1) is not None else m.group(2)
            if not s or len(s) > 260 or ('/' not in s and '\\' not in s):
                continue
            s2 = norm(wildcard(s))
            s2 = re.sub(r'^\*/', '', s2)
            if re.match(r'^(src|tests|tools|distribution|installer|development)/', s2):
                take(s2)
        for m in BARE_PATH.finditer(text):
            take(m.group(1))
        found_set = set(found)
        for f in listed:
            if f not in found_set:
                self.add_path(f)
        for m in EXT_PATH.finditer(text):
            self.stamp_external(m.group(1))
        if '_payloads.h' in text or 'loader_split' in text:
            self.stamp_external('build/development/loader_split/*_payloads.h')
        out = []
        for f in found:
            r = self.add_path(f)
            if r:
                out.append(f)
        return out

    def follow(self, full):
        full = os.path.normpath(full)
        if full in self.visited:
            return
        try:
            st = os.stat(full)
        except OSError:
            return
        if not stat.S_ISREG(st.st_mode):
            return
        self.visited.add(full)
        # 2026-11-16: what one file contributes (paths, externals, toolchain, files to follow) depends on that file, so it is
        # scanned once and replayed for every route that reaches it while its size/mtime stay (dll-split plan 14 s -> 2 s).
        # External stamps are names here and stamped per key (stamp_external).
        k = (self.root, full)
        node = _NODE.get(k)
        if node is None or node[0] != (st.st_size, st.st_mtime_ns):
            rec = Collector(self.root)
            children = rec._scan_file(full)
            node = _NODE[k] = ((st.st_size, st.st_mtime_ns), frozenset(rec.files), list(rec.external), list(rec.toolchain), children)
        _, files, external, toolchain, children = node
        self.files |= files
        for rel in external:
            self.stamp_external(rel)
        self.toolchain.extend(toolchain)
        for f in children:
            self.follow(f)

    def leaf(self, full):
        """A file read as data: its content and toolchain lines are inputs, the files it names are not."""
        if not os.path.isfile(full):
            return
        self.add_path(full)
        if full.lower().endswith('.ps1'):
            self.toolchain.extend(self._ps1_toolchain(strip_ps1_comments(read_text(full))))

    @staticmethod
    def _ps1_toolchain(text):
        out = []
        for m in VC_ASSIGN.finditer(text):
            vc = m.group(1)
            cl = os.path.join(vc, 'bin', 'Hostx64', 'x64', 'cl.exe')
            try:
                st = os.stat(cl)
                out.append('cl:%s:%d:%d' % (vc, st.st_size, int(st.st_mtime)))
            except OSError:
                out.append('cl:%s:missing' % vc)
        for m in TOOL_ASSIGN.finditer(text):
            out.append(m.group(0).strip())
        return out

    def _scan_file(self, full):
        """One file's own contribution; returns the files it leads to, in order (follow() recurses)."""
        self._imports = set()
        children = self._scan_refs(full)
        if full.lower().endswith('.py'):
            # A Python file runs a .ps1 only through powershell and another .py only through an interpreter (sys.executable,
            # python ...) or runpy/importlib; without that, a script it names is read as data (flags, toolchain, rewritten text).
            code = strip_py_comments(read_text(full))
            runs_ps, runs_py = RUNS_PS.search(code), RUNS_PY.search(code)
            keep = []
            for c in children:
                low = c.lower()
                if (low.endswith('.ps1') and not runs_ps) or (low.endswith('.py') and not runs_py and c not in self._imports):
                    self.leaf(c)
                else:
                    keep.append(c)
            children = keep
        rules = DATA_READ.get(rel_to(self.root, full))
        if rules:
            keep = []
            for c in children:
                how = rules.get(rel_to(self.root, c))
                if how is None:
                    keep.append(c)
                elif how == 'leaf':
                    self.leaf(c)
            children = keep
            for rel, how in rules.items():
                if how != 'leaf':
                    self.files.discard(rel)
        return children

    def _scan_refs(self, full):
        children = []
        self.add_path(full)
        ext = os.path.splitext(full)[1].lower()
        raw = read_text(full)
        text = strip_ps1_comments(raw) if ext == '.ps1' else strip_py_comments(raw) if ext == '.py' else raw
        d = os.path.dirname(full)
        if ext == '.ps1':
            self.toolchain.extend(self._ps1_toolchain(text))
            for f in self.scan_text(text, d, full):
                children.append(f)
        elif ext == '.py':
            for m in PY_IMPORT.finditer(text):
                name = m.group(1)
                for base in (d, os.path.join(self.root, 'tools'), os.path.join(self.root, 'tests')):
                    p = os.path.join(base, name + '.py')
                    if os.path.isfile(p):
                        children.append(p)
                        self._imports.add(p)
                        break
            # a script started by its bare name next to a folder variable: os.path.join(A.TOOLS, 'build_addr_fixtures.ps1')
            for m in STR_TOKEN.finditer(text):
                s = m.group(1) if m.group(1) is not None else m.group(2)
                if s and BARE_SCRIPT.match(s):
                    for base in (d, os.path.join(self.root, 'tools'), os.path.join(self.root, 'tests')):
                        p = os.path.join(base, s)
                        if os.path.isfile(p):
                            if self.add_path(p):
                                children.append(p)
                            break
            for m in OS_JOIN.finditer(text):
                parts = [a if a is not None else b for a, b in LITERAL.findall(m.group(1))]
                if len(parts) >= 2:
                    joined = '/'.join(p.strip('/\\') for p in parts)
                    if joined.startswith(EXTERNAL_PREFIXES):
                        self.stamp_external(joined)
            if re.search(r"""['"]src['"/\\]|['"]src/""", raw) or "'src" in raw or '"src' in raw:
                for f in self.expand('src'):
                    self.add_path(f)
            for f in self.scan_text(text, d, full):
                children.append(f)
        elif ext in CODE_EXT:
            for m in INCLUDE.finditer(text):
                inc = m.group(1)
                for base in (d, os.path.join(self.root, 'src'), os.path.join(self.root, 'tests'), self.root):
                    p = os.path.normpath(os.path.join(base, inc))
                    if os.path.isfile(p):
                        children.append(p)
                        break
        if text is not raw:
            # Comments and docstrings often name the data a script opens with os.path.join(ROOT, 'development', 'x.json'):
            # such data files stay inputs. A script named only there is not run by this one, so it is neither an input nor
            # followed (build_native_fixture.ps1 "DLLs come only from tools/build_split.ps1").
            tmp = Collector(self.root)
            tmp.scan_text(raw, d, full)
            for rel in tmp.files:
                if not rel.lower().endswith(SCRIPT_EXT):
                    self.files.add(rel)
            for rel in tmp.external:
                self.stamp_external(rel)
        # other leaf types: content only
        return children


def compute(root, route, routes_entry=None, ps1=None):
    """Return dict(block, files, external, toolchain, entry)."""
    routes_entry = routes_entry or {}
    ps1 = ps1 or os.path.join(root, PS1_REL)
    col = Collector(root)
    ps1_text = read_text(ps1)
    block = strip_comment_and_find_block(ps1_text, route)
    runner = routes_entry.get('runner')
    if block:
        for f in col.scan_text(block, os.path.dirname(ps1), 'branch'):
            col.follow(f)
    steps = routes_entry.get('steps')
    if steps:   # data steps (tools/run_checks.py): every quoted path is an input; cl steps read the toolchain and flags from build_split.ps1
        text = json.dumps(steps, sort_keys=True, ensure_ascii=True)
        block = block + '\nsteps=' + text
        for f in col.scan_text(text, os.path.dirname(ps1), 'steps'):
            col.follow(f)
        col.leaf(os.path.join(root, STEP_RUNNER))   # imports only select_checks, to record pass keys after the run
        if '@split_image' in text:
            m = SPLIT_IMAGE.search(read_text(os.path.join(root, LEAF_ONLY)))
            col.stamp_external(norm(m.group(1)) if m else 'build/ui_native_analysis/fixture')
        for st in steps:
            for key in ('py', 'ps1'):
                if st.get(key):
                    p = os.path.join(root, norm(st[key]))
                    col.add_path(p)
                    col.follow(p)
            if st.get('cl'):
                for rel in list(st['cl'].get('src') or []):
                    p = os.path.join(root, norm(rel))
                    col.add_path(p)
                    col.follow(p)
                col.leaf(os.path.join(root, LEAF_ONLY))
    if runner:
        r = os.path.join(root, norm(runner))
        col.add_path(r)
        col.follow(r)
        block = block + '\nrunner=%s args=%s' % (runner, json.dumps(routes_entry.get('args', [])))
    for pat in routes_entry.get('inputs', []) or []:
        for f in col.expand(pat):
            col.add_path(f)
            if os.path.splitext(f)[1].lower() in ('.ps1', '.py') + CODE_EXT:
                col.follow(f)
    for pat in routes_entry.get('external', []) or []:
        col.stamp_external(pat)
    return {
        'block': block,
        'files': sorted(col.files),
        'external': dict(sorted(col.external.items())),
        'toolchain': sorted(set(col.toolchain)),
        'entry': routes_entry,
    }


def key_of(root, info):
    files = {}
    for rel in info['files']:
        files[rel] = sha_file(os.path.join(root, rel))
    payload = json.dumps({
        'block': info['block'], 'files': files, 'external': info['external'],
        'toolchain': info['toolchain'], 'entry': info['entry'],
    }, sort_keys=True, ensure_ascii=True)
    return hashlib.sha256(payload.encode('utf-8')).hexdigest()


def load_routes(root):
    try:
        with open(os.path.join(root, 'development', 'features.json'), encoding='utf-8') as f:
            return (json.load(f).get('routes') or {})
    except (OSError, ValueError):
        return {}


def reachable_check_files(root, manifest):
    """V3 of tools/check_manifest.py: repo paths read by any route named in the manifest (tests lists + routes)."""
    routes = dict(manifest.get('routes') or {})
    names = set(routes)
    mods = manifest.get('modules') or []
    for m in (mods.values() if isinstance(mods, dict) else mods):
        names.update(m.get('tests') or [])
    out = set()
    for r in sorted(names):
        try:
            info = compute(root, r, routes.get(r))
        except Exception:  # a route this collector cannot parse just contributes nothing
            continue
        out.update(norm(f) for f in info.get('files') or [])
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('route')
    ap.add_argument('--root', default=os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    ap.add_argument('--key', action='store_true', help='print only the cache key')
    a = ap.parse_args(argv)
    root = os.path.abspath(a.root)
    info = compute(root, a.route, load_routes(root).get(a.route))
    k = key_of(root, info)
    if a.key:
        print(k)
        return 0
    print(json.dumps({'route': a.route, 'key': k, 'files': info['files'], 'external': info['external'],
                      'toolchain': info['toolchain']}, indent=1, ensure_ascii=True))
    return 0


if __name__ == '__main__':
    sys.exit(main())
