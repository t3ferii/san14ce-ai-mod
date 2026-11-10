#!/usr/bin/env python
"""Input set and cache key of one check route (docs/HISTORY.md 3).

A route is a branch of tools/check_feature.ps1 (or a routes.<name>.runner of
development/features.json). Its inputs are every repo file the branch can read:
nested scripts, C/C++ include closure, local Python imports. External data
(research/, legacy/, ...) is stamped by size and mtime instead of hashed.
Standard library only. Read-only.
"""
import argparse
import glob as _glob
import hashlib
import json
import os
import re
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


def sha_file(path):
    h = hashlib.sha256()
    try:
        with open(path, 'rb') as f:
            for chunk in iter(lambda: f.read(1 << 20), b''):
                h.update(chunk)
    except OSError:
        return 'unreadable'
    return h.hexdigest()


def rel_to(root, path):
    return os.path.relpath(path, root).replace('\\', '/')


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
        self.external[rel] = hashlib.sha256('|'.join(parts).encode()).hexdigest() if parts else 'missing'

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
        # Join-Path $x 'rel'
        for m in JOIN_PATH.finditer(text):
            var, rel = m.group(1), m.group(2)
            rel = wildcard(rel)
            base = base_dir if var == '$PSScriptRoot' else self.root
            cand = rel_to(self.root, os.path.join(base, norm(rel)))
            found.extend(self.expand(cand))
        # quoted strings with root-relative or script-relative shape
        for m in STR_TOKEN.finditer(text):
            s = m.group(1) if m.group(1) is not None else m.group(2)
            if not s or len(s) > 260 or ('/' not in s and '\\' not in s):
                continue
            s2 = norm(wildcard(s))
            s2 = re.sub(r'^\*/', '', s2)
            if re.match(r'^(src|tests|tools|distribution|installer|development)/', s2):
                found.extend(self.expand(s2))
        for m in BARE_PATH.finditer(text):
            found.extend(self.expand(norm(m.group(1))))
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
        if full in self.visited or not os.path.isfile(full):
            return
        self.visited.add(full)
        self.add_path(full)
        ext = os.path.splitext(full)[1].lower()
        text = read_text(full)
        d = os.path.dirname(full)
        if ext == '.ps1':
            for m in VC_ASSIGN.finditer(text):
                vc = m.group(1)
                cl = os.path.join(vc, 'bin', 'Hostx64', 'x64', 'cl.exe')
                try:
                    st = os.stat(cl)
                    self.toolchain.append('cl:%s:%d:%d' % (vc, st.st_size, int(st.st_mtime)))
                except OSError:
                    self.toolchain.append('cl:%s:missing' % vc)
            for m in TOOL_ASSIGN.finditer(text):
                self.toolchain.append(m.group(0).strip())
            for f in self.scan_text(text, d, full):
                self.follow(f)
        elif ext == '.py':
            for m in PY_IMPORT.finditer(text):
                name = m.group(1)
                for base in (d, os.path.join(self.root, 'tools'), os.path.join(self.root, 'tests')):
                    p = os.path.join(base, name + '.py')
                    if os.path.isfile(p):
                        self.follow(p)
                        break
            if re.search(r"""['"]src['"/\\]|['"]src/""", text) or "'src" in text or '"src' in text:
                for f in self.expand('src'):
                    self.add_path(f)
            for f in self.scan_text(text, d, full):
                self.follow(f)
        elif ext in CODE_EXT:
            for m in INCLUDE.finditer(text):
                inc = m.group(1)
                for base in (d, os.path.join(self.root, 'src'), os.path.join(self.root, 'tests'), self.root):
                    p = os.path.normpath(os.path.join(base, inc))
                    if os.path.isfile(p):
                        self.follow(p)
                        break
        # other leaf types: content only


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
        for st in steps:
            for key in ('py', 'ps1'):
                if st.get(key):
                    p = os.path.join(root, norm(st[key]))
                    col.add_path(p)
                    col.follow(p)
            if st.get('cl'):
                for rel in list(st['cl'].get('src') or []) + ['tools/build_split.ps1']:
                    p = os.path.join(root, norm(rel))
                    col.add_path(p)
                    col.follow(p)
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
