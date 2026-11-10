#!/usr/bin/env python
"""Static hook-site extractor for the San14 mod (check redesign package A, 2026-11-06, docs/HISTORY.md section 4).

Reads only source files under <root>/src (+ development/hook_sites.json, optional research/rva_map tables) and produces a
hook map: every place where our code patches the original game (vtable slots, E8/E9 sites, byte patches, COM slots, window
procedure, windows hooks) with owner, DLL, build RVAs, patch length and guard classification. No game, no EXE needed
(the unpacked EXE is only used by tools/hook_map.py rule R13 through ExeReader). Python standard library only.

Heuristics (documented limits, see docs/CHECKS.md / hook_map.py --help):
 - comments are stripped, preprocessor conditions (#if/#ifdef/#elif/#else, defined, !, &&, ||) are evaluated per DLL root with
   the build_split.ps1 flags; release (NO_RUNTIME_LOG) and diagnostic (RUNTIME_COST_PROFILE) configurations are united.
 - address arguments are resolved from GA(GA_X)[+N], file constants (k...=GA(..)), constant arrays and local table rows;
   everything else must be bound in development/hook_sites.json, otherwise it is reported as "unresolved".
"""
import os, re, sys, json, bisect

SRC_EXT = ('.h', '.inl', '.cpp', '.c')
PATCH_CALLS = ('prepare_link', 'prepare_bytes', 'call_prepare', 'patch_pointer', 'ROUTE_VT', 'replace_code16', 'modpack_write',
               'layout_client::vt', 'layout_client::call', 'layout_client::jump', 'layout_client::block',
               'layout_client::span', 'layout_client::bytes', 'add_hook', 'hook_abs_slot')
TYPE_WORDS = {'bool', 'int', 'void', 'unsigned', 'char', 'long', 'short', 'static', 'inline', 'const', 'auto', 'size_t',
              'LONG_PTR', 'HHOOK', 'BOOL', 'DWORD', 'Obj', 'define', 'int32_t', 'uint32_t'}
FLAG_NAMES = ('CHAIN_OK', 'ALL_OR_NONE', 'YIELD_IF_PEER', 'NEED_WHITELIST', 'REFUSE_IF_FOREIGN', 'LATE_INSTALL')
DLL_ROOTS = (   # (dll, root file, extra defines)  - flags per tools/build_split.ps1
    ('convenience', 'native_adapter.cpp', ('SPLIT_BASE',)),
    ('convenience', 'gpu_profile.cpp', ('SPLIT_BASE',)),
    ('combat', 'native_adapter.cpp', ('SPLIT_COMBAT',)),
    ('ccz', 'ccz_module.cpp', ()),
    ('layout', 'layout_main.cpp', ()),
    ('loader', 'module_loader.cpp', ()),
    ('loader', 'input_proxy.cpp', ()),
)
DEFAULT_FLAGS = ('MERIT_CANDIDATE', 'RECRUITMENT_UI_CANDIDATE', 'HIDDEN_LOYALTY', 'INJURY_PROTECTION', 'AWARD_WAREHOUSE',
                 'SORTIE_FORMATION', 'LAYOUT_OWNS_PRESENT')
LAYOUT_FILES_HINT = ('native_layout_core.inl', 'native_layout_ui.inl', 'native_layout_present.inl', 'layout_main.cpp')


# ------------------------------------------------------------------ source loading
_COMMENT = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)


def _blank(m):
    t = m.group(0)
    if t[0] in '"\'':
        return t
    return re.sub(r'[^\n]', ' ', t)


def strip_comments(text):
    return _COMMENT.sub(_blank, text)


class Src:
    """one stripped source file with line lookup."""

    def __init__(self, name, raw):
        self.name = name
        self.text = strip_comments(raw.replace('\r\n', '\n').replace('\r', '\n'))
        self.lines = self.text.split('\n')
        self.nl = [m.start() for m in re.finditer('\n', self.text)]

    def line_of(self, off):
        return bisect.bisect_left(self.nl, off) + 1


class FsTree:
    """files of a working copy (root = folder that contains src/, development/, tools/)."""

    def __init__(self, root):
        self.root = os.path.abspath(root)
        self.label = 'working tree'

    def read(self, rel):
        try:
            with open(os.path.join(self.root, rel), 'rb') as f:
                return f.read()
        except OSError:
            return None

    def list(self, rel):
        d = os.path.join(self.root, rel)
        try:
            return sorted(fn for fn in os.listdir(d) if os.path.isfile(os.path.join(d, fn)))
        except OSError:
            return []


class GitTree:
    """files of a git revision, read in memory with `git archive` (nothing is written to disk)."""

    def __init__(self, root, rev, paths=('src', 'development', 'tools/build_split.ps1'), fallback=None):
        import subprocess
        import tarfile
        import io
        self.root = os.path.abspath(root)
        self.label = 'git ' + rev
        self.files = {}
        self.fallback = fallback        # working-tree files used for development/*.json that the revision does not have yet
        r = subprocess.run(['git', '-C', self.root, 'archive', '--format=tar', rev] + list(paths), capture_output=True)
        if r.returncode != 0:
            raise RuntimeError('git archive %s failed: %s' % (rev, r.stderr.decode('utf-8', 'replace').strip()))
        with tarfile.open(fileobj=io.BytesIO(r.stdout)) as tf:
            for m in tf:
                if m.isfile():
                    self.files[m.name.replace('\\', '/')] = tf.extractfile(m).read()

    def read(self, rel):
        v = self.files.get(rel.replace('\\', '/'))
        if v is None and self.fallback is not None and rel.startswith('development/hook_'):
            v = self.fallback.read(rel)
        return v

    def list(self, rel):
        pre = rel.replace('\\', '/').rstrip('/') + '/'
        return sorted(n[len(pre):] for n in self.files if n.startswith(pre) and '/' not in n[len(pre):])


def as_tree(x):
    return x if hasattr(x, 'read') else FsTree(x)


def load_sources(tree):
    tree = as_tree(tree)
    out = {}
    texts = {}
    for fn in tree.list('src'):
        if fn.endswith(SRC_EXT):              # *.bak_* backups end with another extension and are skipped
            raw = tree.read('src/' + fn)
            if raw is not None:
                texts[fn] = raw.decode('utf-8', 'replace')
    for fn, text in texts.items():
        out[fn] = Src(fn, text)
    return out


def read_json(tree, rel, default=None):
    raw = as_tree(tree).read(rel)
    if raw is None:
        return default
    try:
        return json.loads(raw.decode('utf-8-sig'))
    except ValueError:
        return default


# ------------------------------------------------------------------ preprocessor
def _eval_cond(expr, defs):
    e = re.sub(r'defined\s*\(\s*(\w+)\s*\)|defined\s+(\w+)', lambda m: '1' if (m.group(1) or m.group(2)) in defs else '0', expr)
    e = e.replace('&&', ' and ').replace('||', ' or ')
    e = re.sub(r'!(?!=)', ' not ', e)

    def ident(m):
        w = m.group(0)
        if w in ('and', 'or', 'not'):
            return w
        v = defs.get(w)
        if v is not None and re.fullmatch(r'-?\d+', str(v).strip() or '0'):
            return str(int(str(v).strip() or '0'))
        return '0'
    e = re.sub(r'\b[A-Za-z_]\w*\b', ident, e)
    e = re.sub(r'(\d)[uUlL]+\b', r'\1', e)
    try:
        return bool(eval(e, {'__builtins__': {}}, {}))
    except Exception:
        return False


def active_lines(srcs, root_file, defines):
    """{file: set(active line numbers)} reachable from root_file with the given defines."""
    out = {}
    defs = {d: '1' for d in defines}
    pragma_seen = set()

    def proc(fn, depth):
        s = srcs.get(fn)
        if s is None or depth > 40:
            return
        if fn in pragma_seen:
            return
        act = out.setdefault(fn, set())
        stack = []          # (parent_active, taken, active)
        active = True
        cont = False
        for i, l in enumerate(s.lines, 1):
            t = l.lstrip()
            if cont:
                cont = t.rstrip().endswith('\\')
                continue
            if not t.startswith('#'):
                if active and t:
                    act.add(i)
                continue
            m = re.match(r'#\s*(\w+)\s*(.*)', t)
            if not m:
                continue
            d, rest = m.group(1), m.group(2).strip()
            if d in ('if', 'ifdef', 'ifndef'):
                if d == 'if':
                    c = _eval_cond(rest, defs)
                elif d == 'ifdef':
                    c = rest.split()[0] in defs if rest else False
                else:
                    c = rest.split()[0] not in defs if rest else True
                stack.append((active, c and active, c and active))
                active = active and c
            elif d == 'elif':
                if stack:
                    par, taken, _a = stack[-1]
                    c = (not taken) and par and _eval_cond(rest, defs)
                    stack[-1] = (par, taken or c, c)
                    active = c
            elif d == 'else':
                if stack:
                    par, taken, _a = stack[-1]
                    c = par and not taken
                    stack[-1] = (par, True, c)
                    active = c
            elif d == 'endif':
                if stack:
                    stack.pop()
                    active = stack[-1][2] if stack else True
            elif active:
                if d == 'define':
                    mm = re.match(r'(\w+)(\([^)]*\))?\s*(.*)', rest)
                    if mm:
                        defs[mm.group(1)] = '1' if mm.group(2) else (mm.group(3).strip() or '1')
                elif d == 'undef':
                    defs.pop(rest.split()[0] if rest else '', None)
                elif d == 'pragma' and rest.startswith('once'):
                    pragma_seen.add(fn)
                elif d == 'include':
                    mm = re.match(r'"([^"]+)"', rest)
                    if mm:
                        proc(os.path.basename(mm.group(1)), depth + 1)
            if t.rstrip().endswith('\\') and d == 'define':
                cont = True
    proc(root_file, 0)
    return out


def parse_flags(tree):
    raw = as_tree(tree).read('tools/build_split.ps1')
    if raw:
        m = re.search(r'\$taskFlags=@\(([^)]*)\)', raw.decode('utf-8', 'replace'))
        if m:
            fl = re.findall(r"'/D(\w+)'", m.group(1))
            if fl:
                return tuple(fl)
    return DEFAULT_FLAGS


def compute_variants(srcs, tree):
    """{file: {dll: set(lines)}} united over the release and diagnostic configurations."""
    base = parse_flags(tree)
    res = {}
    for dll, rf, extra in DLL_ROOTS:
        if rf not in srcs:
            continue
        flags = base if dll in ('convenience', 'combat', 'ccz', 'layout') else ()
        for cfg in (('NO_RUNTIME_LOG',), ('RUNTIME_COST_PROFILE',)):
            al = active_lines(srcs, rf, tuple(flags) + tuple(extra) + cfg)
            for fn, ls in al.items():
                res.setdefault(fn, {}).setdefault(dll, set()).update(ls)
    return res


# ------------------------------------------------------------------ generic parsing helpers
_SPECIAL = {}


def match_close(text, i, open_c, close_c):
    """index of the bracket closing text[i]==open_c (strings skipped); -1 if none."""
    pat = _SPECIAL.get((open_c, close_c))
    if pat is None:
        pat = _SPECIAL[(open_c, close_c)] = re.compile('["\'\\\\' + re.escape(open_c) + re.escape(close_c) + ']')
    depth = 0
    n = len(text)
    pos = i
    while True:
        m = pat.search(text, pos)
        if not m:
            return -1
        c = m.group(0)
        j = m.start()
        if c == '"' or c == "'":
            k = j + 1
            while k < n and text[k] != c:
                k += 2 if text[k] == '\\' else 1
            pos = k + 1
            continue
        if c == '\\':
            pos = j + 2
            continue
        if c == open_c:
            depth += 1
        else:
            depth -= 1
            if depth == 0:
                return j
        pos = j + 1


def split_top(s):
    """split on top-level commas ((), {}, [] and strings respected)."""
    out, depth, cur, i, n = [], 0, [], 0, len(s)
    while i < n:
        c = s[i]
        if c == '"':
            j = i + 1
            while j < n and s[j] != '"':
                j += 2 if s[j] == '\\' else 1
            cur.append(s[i:j + 1])
            i = j + 1
            continue
        if c in '({[':
            depth += 1
        elif c in ')}]':
            depth -= 1
        if c == ',' and depth == 0:
            out.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    last = ''.join(cur).strip()
    if last or out:
        out.append(last)
    return out


class GaTable:
    """game_addresses_data.inl: GA id -> RVA per build (0 = unresolved), observers."""

    def __init__(self, srcs):
        self.ok = False
        self.index, self.vals, self.builds, self.observers = {}, {}, [], []
        s = srcs.get('game_addresses_data.inl')
        if not s:
            return
        t = s.text
        m = re.search(r'enum\s+GaId\s*:\s*unsigned\s*\{(.*?)\}\s*;', t, re.S)
        if not m:
            m = re.search(r'enum\s+GaId[^{]*\{(.*?)\}\s*;?', t, re.S)
        if m:
            for name, num in re.findall(r'(GA_\w+?)\s*=\s*(\d+)', m.group(1)):
                if name != 'GA_COUNT':
                    self.index[name] = int(num)
        for m in re.finditer(r'ga_val_(\w+)\s*\[[^\]]*\]\s*=\s*\{(.*?)\}\s*;', t, re.S):
            self.builds.append(m.group(1))
            self.vals[m.group(1)] = [int(x, 16) for x in re.findall(r'0[xX]([0-9A-Fa-f]+)', m.group(2))]
        for m in re.finditer(r'\{\{((?:0[xX][0-9A-Fa-f]+,?)+)\},(\d+),\{([^}]*)\}\}', t[t.find('ga_observers'):] if 'ga_observers' in t else ''):
            self.observers.append({'rva': [int(x, 16) for x in re.findall(r'0[xX]([0-9A-Fa-f]+)', m.group(1))], 'len': int(m.group(2)),
                                   'bytes': [int(x, 16) for x in re.findall(r'0[xX]([0-9A-Fa-f]+)', m.group(3))]})
        self.ok = bool(self.index and self.builds)

    def rva(self, ga, build):
        i = self.index.get(ga)
        v = self.vals.get(build)
        if i is None or v is None or i >= len(v):
            return None
        return v[i] or None

    def hexid(self, ga):
        return ga[3:] if ga and ga.startswith('GA_') else None


GA_RE = re.compile(r'^GA\(\s*(GA_\w+)\s*\)\s*(?:\+\s*(0[xX][0-9A-Fa-f]+|\d+))?$')


def block_depths(t):
    """[(offset, function/block depth after the brace)] ignoring namespace braces."""
    out, stack = [], []
    for m in re.finditer(r'[{}]', t):
        if m.group(0) == '{':
            stack.append('ns' if re.search(r'\bnamespace\s*\w*\s*$|extern\s*"C"\s*$', t[max(0, m.start() - 60):m.start()]) else 'blk')
        elif stack:
            stack.pop()
        out.append((m.start(), sum(1 for x in stack if x == 'blk')))
    return out


def braces_at(depths, off):
    lo, hi = 0, len(depths)
    while lo < hi:
        mid = (lo + hi) // 2
        if depths[mid][0] < off:
            lo = mid + 1
        else:
            hi = mid
    return depths[lo - 1][1] if lo else 0


GA_USE = re.compile(r'GA\(\s*GA_\w+\s*\)(?:\s*\+\s*(?:0[xX][0-9A-Fa-f]+|\d+))?\s*[;,)]')
LOOKBACK = re.compile(r'(\w+)\s*=\s*$')


class Resolver:
    """resolves address expressions to (GA id, addend) with file constants, constant arrays and table rows (tables are parsed lazily)."""

    SKIP = ('game_addresses_data.inl', 'game_addresses_gates.inl')

    def __init__(self, srcs):
        self.srcs = srcs
        self.consts = {}     # name -> {file: expr}
        self.raw = {}        # name -> {file: (type, body text)}
        self.structs = {}    # type -> {file: [field names]}
        self._sizes = {}     # lazily: array name -> declared size, '=name' -> integer constant
        self.scoped = set()  # (name, file) of constants defined outside any function body (namespace/global scope)
        self._rows = {}
        self._scan()

    def _scan(self):
        for fn, s in self.srcs.items():
            if fn in self.SKIP:
                continue
            t = s.text
            braces = None
            if 'GA(' in t:
                for m in GA_USE.finditer(t):
                    pm = LOOKBACK.search(t, max(0, m.start() - 70), m.start())
                    if not pm:
                        continue
                    name = pm.group(1)
                    self.consts.setdefault(name, {}).setdefault(fn, []).append((pm.start(1), re.sub(r'\s+', '', m.group(0)[:-1])))
                    if braces is None:
                        braces = block_depths(t)
                    if braces_at(braces, pm.start(1)) == 0:
                        self.scoped.add((name, fn))
            for m in re.finditer(r'\b(\w+)\s*((?:\[[^\]]*\])+)\s*=\s*\{', t):
                name = m.group(1)
                ob = m.end() - 1
                pre = t[max(0, m.start() - 80):m.start()]
                tm = re.search(r'(\w+)\s*$', pre.rstrip())
                self.raw.setdefault(name, {})[fn] = (tm.group(1) if tm else '', ob)       # body is cut lazily (rows_of)
            for m in re.finditer(r'\bstruct\s+(\w+)\s*\{([^{}]*)\}', t):
                names = []
                for decl in m.group(2).split(';'):
                    for part in decl.split(','):
                        mm = re.search(r'(\w+)\s*(?:\[[^\]]*\])?\s*$', part.strip())
                        if mm and part.strip():
                            names.append(mm.group(1))
                if names:
                    self.structs.setdefault(m.group(1), {})[fn] = names

    def size_of(self, key):
        """declared array size of `name` or integer value of `=name` (searched on demand)."""
        if key in self._sizes:
            return self._sizes[key]
        name = key.lstrip('=')
        pat = re.compile(r'\b%s\s*=\s*(\d+)\s*[;,]' % re.escape(name)) if key.startswith('=') else re.compile(r'\b%s\s*\[\s*(\d+)\s*\]' % re.escape(name))
        val = None
        for fn, s in self.srcs.items():
            if fn in self.SKIP or name not in s.text:
                continue
            m = pat.search(s.text)
            if m:
                val = int(m.group(1))
                break
        self._sizes[key] = val
        return val

    def rows_of(self, name, fn):
        key = (name, fn)
        if key not in self._rows:
            typ, ob = self.raw[name][fn]
            t = self.srcs[fn].text
            cb = match_close(t, ob, '{', '}')
            body = t[ob + 1:cb] if cb > 0 else ''
            rows = []
            for p in split_top(body):
                p = p.strip()
                if p.startswith('{') and p.endswith('}'):
                    rows.append([re.sub(r'\s+', ' ', x).strip() for x in split_top(p[1:-1])])
                elif p:
                    rows.append([re.sub(r'\s+', ' ', p)])
            self._rows[key] = (typ, rows)
        return self._rows[key]

    def _pick(self, store, name, fn):
        d = store.get(name)
        if not d:
            return None
        if fn in d:
            return fn
        if len(d) == 1:
            return next(iter(d))
        return None

    def array(self, name, fn):
        k = self._pick(self.raw, name, fn)
        if k is None:
            return None
        typ, rows = self.rows_of(name, k)
        if rows and all(len(r) == 1 for r in rows):
            return [r[0].replace(' ', '') for r in rows], k
        return None

    def const_expr(self, name, fn, off, qualified=False):
        d = self.consts.get(name)
        if not d:
            return None
        if fn in d:
            lst = d[fn]
            best = None
            bo = 0
            for o, e in lst:
                if off is None or o <= off:
                    best, bo = e, o
            if best is None:
                return lst[0][1] if off is None else None
            if off is not None and re.search(r'\b%s\s*=(?!=)' % re.escape(name), self.srcs[fn].text[bo + len(name) + 1:off]):
                return None                 # re-assigned with something else before the use
            return best
        if not qualified:
            d = {f2: lst for f2, lst in d.items() if (name, f2) in self.scoped}   # local variables (site, rva, entry...) never resolve across files
        exprs = {e for lst in d.values() for _o, e in lst}
        return next(iter(exprs)) if len(exprs) == 1 else None

    def eval(self, expr, fn, off=None):
        """-> (ga_id, addend) or None. off: source offset of the use (nearest preceding assignment of a local name wins)."""
        e = re.sub(r'\s+', '', expr or '')
        e = re.sub(r'^\(+(?=GA\()', '', e)
        m = GA_RE.match(e)
        if m:
            return m.group(1), int(m.group(2), 0) if m.group(2) else 0
        m = re.match(r'^(?:\w+::)*(\w+)(?:\[(\d+)\])?(?:\+(0[xX][0-9A-Fa-f]+|\d+))?$', e)
        if m:
            name, idx, add = m.group(1), m.group(2), m.group(3)
            extra = int(add, 0) if add else 0
            if idx is not None:
                arr = self.array(name, fn)
                if arr and int(idx) < len(arr[0]):
                    r = self.eval(arr[0][int(idx)], arr[1])
                    return (r[0], r[1] + extra) if r else None
                return None
            ce = self.const_expr(name, fn, off, '::' in e)
            if ce:
                r = self.eval(ce, fn)
                return (r[0], r[1] + extra) if r else None
            if re.fullmatch(r'GA_\w+', name):
                return name, extra
        return None

    def table(self, name, fn):
        """(type, rows, defining file) of the table, preferring fn."""
        d = self.raw.get(name)
        if not d:
            return None
        k = fn if fn in d else sorted(d)[0]
        typ, rows = self.rows_of(name, k)
        return typ, rows, k

    def field_index(self, typ, field, fn):
        if isinstance(field, int):
            return field
        d = self.structs.get(typ) or {}
        names = d.get(fn) or (list(d.values())[0] if d else None)
        if names and field in names:
            return names.index(field)
        return None


# ------------------------------------------------------------------ call extraction
CALL_RE = re.compile(r'(?<![\w.>])(' + '|'.join(re.escape(c) for c in sorted(PATCH_CALLS, key=len, reverse=True)) + r')\s*\(')
KIND_OF = {'layout_client::vt': 'vt', 'layout_client::call': 'call', 'layout_client::jump': 'jump', 'layout_client::block': 'block',
           'layout_client::span': 'span', 'layout_client::bytes': 'bytes', 'patch_pointer': 'vt', 'ROUTE_VT': 'vt',
           'call_prepare': 'call', 'replace_code16': 'block', 'modpack_write': 'bytes', 'prepare_bytes': 'bytes',
           'hook_abs_slot': 'com'}


def owner_label(fn, text_before, aliases):
    stem = fn
    stem = re.sub(r'\.(inl|cpp|h|c)$', '', stem)
    stem = re.sub(r'^native_', '', stem)
    stem = re.sub(r'_links?$', '', stem)
    if stem in aliases:
        return aliases[stem]
    if fn in ('native_adapter.cpp', 'native_split_install.inl'):
        return 'adapter'        # shared core of both gameplay DLLs: one owner label
    return stem


def find_calls(srcs, variants, aliases, wrappers):
    """raw patch calls: [dict(file,line,call,args,owner,dlls)]"""
    out = []
    names = list(PATCH_CALLS)
    for w in wrappers:
        names.append(w['name'])
    pat = re.compile(r'(?<![\w.>])(' + '|'.join(re.escape(c) for c in sorted(names, key=len, reverse=True)) + r')\s*\(')
    for fn, s in srcs.items():
        t = s.text
        for m in pat.finditer(t):
            call = m.group(1)
            # skip declarations/definitions/macros: preceded by a type word
            pre = t[max(0, m.start() - 40):m.start()]
            pm = re.search(r'([\w:]+)\s*[*&]*\s*$', pre)
            if pm and pm.group(1) in TYPE_WORDS:
                continue
            if re.search(r'#\s*define\s+$', pre):
                continue
            op = m.end() - 1
            cp = match_close(t, op, '(', ')')
            if cp < 0:
                continue
            args = split_top(t[op + 1:cp])
            # a definition has a '{' right after ')' (function body) - skip
            after = t[cp + 1:cp + 6].lstrip()
            if after.startswith('{') and re.search(r'\b(bool|void|int|static)\b', pre):
                continue
            line = s.line_of(m.start())
            dlls = sorted(d for d, ls in variants.get(fn, {}).items() if line in ls)
            out.append({'file': fn, 'line': line, 'call': call, 'args': [re.sub(r'\s+', ' ', a) for a in args],
                        'owner': owner_label(fn, t[:m.start()], aliases), 'dlls': dlls, 'off': m.start()})
    return out


# ------------------------------------------------------------------ registry & patch rows
def flag_set(expr, symbols):
    out = set()
    for tok in re.split(r'[|\s]+', expr.strip()):
        if not tok:
            continue
        if tok.startswith('LAYOUT_'):
            out.add(tok[7:])
        elif tok in symbols:
            out |= symbols[tok]
    return out


def parse_registry(srcs, variants):
    """layout_registry.h -> dict(rows, patch_rows, traits9, blocked, file, symbols)."""
    res = {'rows': [], 'patch_rows': [], 'traits9': [], 'blocked': [], 'present': False}
    s = srcs.get('layout_registry.h')
    if not s:
        return res
    res['present'] = True
    t = s.text
    sym = {}
    m = re.search(r'enum\s*:\s*uint32_t\s*\{(.*?)\};', t, re.S)
    if m:
        for name, expr in re.findall(r'(\bLF_\w+)\s*=\s*([^,\n]+)', m.group(1)):
            sym[name] = flag_set(expr.split('//')[0], sym)
    for m in re.finditer(r'\{\s*"(\w+)"\s*,\s*LR_(VT|CALL|JUMP)\s*,\s*GA\(\s*(GA_\w+)\s*\)\s*,\s*GA\(\s*(GA_\w+)\s*\)\s*,\s*([\w|:\s]+?)\s*\}', t):
        res['rows'].append({'owner': m.group(1), 'kind': {'VT': 'vt', 'CALL': 'call', 'JUMP': 'jump'}[m.group(2)], 'ga': m.group(3),
                            'orig': m.group(4), 'flags': sorted(flag_set(m.group(5), sym)), 'line': s.line_of(m.start())})
    expects = {}
    for m in re.finditer(r'static\s+const\s+uint8_t\s+(\w+)\s*\[[^\]]*\]\s*=\s*\{([^}]*)\}', t):
        expects[m.group(1)] = [int(x, 16) for x in re.findall(r'0[xX]([0-9A-Fa-f]{1,2})', m.group(2))]
    for m in re.finditer(r'LayoutPatchRow\s+(\w+)\s*=\s*\{\s*"(\w+)"\s*,\s*GA\(\s*(GA_\w+)\s*\)\s*,\s*(\d+)\s*,\s*([\w|\s]+?)\s*\}', t):
        name = m.group(1)
        stem = re.sub(r'_(row|span)$', '', name)
        res['patch_rows'].append({'var': name, 'owner': m.group(2), 'ga': m.group(3), 'len': int(m.group(4)),
                                  'flags': sorted(flag_set(m.group(5), sym)), 'expect': expects.get(stem + '_expect'),
                                  'kind': 'span' if name.endswith('span') else 'block', 'line': s.line_of(m.start())})
    m = re.search(r'layout_traits9_sites\s*\[\]\s*=\s*\{(.*?)\};', t, re.S)
    if m:
        for mm in re.finditer(r'\{\s*LP_(SPAN|BYTES)\s*,\s*GA\(\s*(GA_\w+)\s*\)\s*,\s*(\d+)\s*\}', m.group(1)):
            res['traits9'].append({'owner': 'traits9', 'kind': mm.group(1).lower(), 'ga': mm.group(2), 'len': int(mm.group(3)),
                                   'flags': sorted(sym.get('LF_Y', {'YIELD_IF_PEER', 'ALL_OR_NONE'})), 'line': s.line_of(m.start())})
    m = re.search(r'layout_blocked_entries\s*\[\]\s*=\s*\{(.*?)\};', t, re.S)
    if m:
        res['blocked'] = re.findall(r'GA\(\s*(GA_\w+)\s*\)', m.group(1))
    return res


def parse_core(srcs):
    res = {'blocked': [], 'max_hooks': None, 'max_handles': None, 'peer_owners': [], 'present': False}
    s = srcs.get('native_layout_core.inl')
    if s:
        res['present'] = True
        m = re.search(r'blocked_entries\s*\[\]\s*=\s*\{(.*?)\};', s.text, re.S)
        if m:
            res['blocked'] = re.findall(r'GA\(\s*(GA_\w+)\s*\)', m.group(1))
        m = re.search(r'kMaxHooks\s*=\s*(\d+)', s.text)
        res['max_hooks'] = int(m.group(1)) if m else None
        m = re.search(r'peer_rules\s*\[\]\s*=\s*\{(.*?)\n\};', s.text, re.S)
        if m:
            res['peer_owners'] = sorted(set(re.findall(r'\{\s*"(\w+)"\s*,\s*\d+\s*,', m.group(1))))
    c = srcs.get('native_layout_client.inl')
    if c:
        m = re.search(r'kMaxHandles\s*=\s*(\d+)', c.text)
        res['max_handles'] = int(m.group(1)) if m else None
    return res


def parse_split_pointers(srcs, variants, res, ga):
    """split_pointers rows (#ifdef SPLIT_COMBAT aware) -> list of row dicts. The rows are generated (W5): src/module_pointers.inl."""
    s = srcs.get('module_pointers.inl')
    out = []
    if not s:
        return out
    m = re.search(r'split_pointers\s*\[\]\s*=\s*\{', s.text)
    if not m:
        return out
    ob = m.end() - 1
    cb = match_close(s.text, ob, '{', '}')
    base = s.line_of(ob)
    for i in range(base, s.line_of(cb) + 1):
        l = s.lines[i - 1]
        mm = re.match(r'\s*\{\s*(.+?)\s*\}\s*,?\s*$', l)
        if not mm:
            continue
        cols = split_top(mm.group(1))
        if len(cols) < 3:
            continue
        dlls = sorted(d for d, ls in variants.get('module_pointers.inl', {}).items() if i in ls)
        rep = re.search(r'&\s*([\w:]+)', cols[2])
        out.append({'slot': cols[0], 'original': cols[1], 'replacement': rep.group(1) if rep else cols[2],
                    'owner_enum': cols[3] if len(cols) > 3 else 'kPointerShared', 'next': cols[4].lstrip('&') if len(cols) > 4 else None,
                    'line': i, 'dlls': dlls})
    return out


# ------------------------------------------------------------------ site building
ADDR_ARG = {'prepare_link': 1, 'prepare_bytes': 1, 'call_prepare': 1, 'patch_pointer': 0, 'ROUTE_VT': 0, 'replace_code16': 0,
            'modpack_write': 0, 'layout_client::vt': 0, 'layout_client::call': 0, 'layout_client::jump': 0,
            'layout_client::block': 1, 'layout_client::span': 1, 'layout_client::bytes': 1, 'add_hook': 2}
LEN_ARG = {'prepare_bytes': 2, 'modpack_write': 3, 'layout_client::block': 3, 'layout_client::span': 3, 'layout_client::bytes': 4,
           'add_hook': 7}
ADD_HOOK_KIND = {'kVtable': 'vt', 'kCall': 'call', 'kBlock': 'block', 'kAbsSlot': 'com'}
DIRECT_GUARD = {'add_hook': 'layout-original', 'patch_pointer': 'exact', 'ROUTE_VT': 'exact', 'modpack_write': 'exact', 'replace_code16': 'exact', 'call_prepare': 'opcode'}
BIND_KIND = {'call5': 'call', 'jmp5': 'jump', 'slot8': 'vt', 'bytes': 'bytes'}


def norm_stem(fn):
    stem = re.sub(r'\.(inl|cpp|h|c)$', '', fn)
    stem = re.sub(r'^native_', '', stem)
    return re.sub(r'_(links?|save|profile|policy|ui|settings|api)$', '', stem)


def resolve_len(expr, fn, rs):
    if expr is None:
        return None
    e = expr.strip()
    if re.fullmatch(r'\d+|0[xX][0-9A-Fa-f]+', e):
        return int(e, 0)
    m = re.fullmatch(r'sizeof\s*\(\s*(?:\w+::)*(\w+)\s*\)', e)
    if m:
        arr = rs.array(m.group(1), fn)
        if arr:
            return len(arr[0])
        return rs.size_of(m.group(1))
    m = re.fullmatch(r'(?:\w+::)*(\w+)', e)
    if m:
        return rs.size_of('=' + m.group(1))
    return None


class Map:
    def __init__(self):
        self.sites = []
        self.registrations = []     # layout_client registrations
        self.unresolved = []
        self.meta = {}

    def add(self, **kw):
        kw['id'] = 'S%04d' % (len(self.sites) + 1)
        self.sites.append(kw)
        return kw


def canon_key(ga, ga_id, add):  # noqa
    """GA id of (id + addend) when the table has it (GA ids are old-build RVAs), else 'GA_X+N'."""
    if not ga_id or not add:
        return ga_id
    c = 'GA_%X' % (int(ga_id[3:], 16) + add)
    return c if c in ga.index else '%s+%d' % (ga_id, add)


def rvas_for(ga, ga_id, add):
    d = {}
    for b in ga.builds:
        r = ga.rva(ga_id, b) if ga_id else None
        d[b] = (r + add) if r is not None else None
    return d


def build_map(root):
    tree = as_tree(root)
    root = tree.root
    srcs = load_sources(tree)
    variants = compute_variants(srcs, tree)
    ga = GaTable(srcs)
    reg = parse_registry(srcs, variants)
    core = parse_core(srcs)
    hs = read_json(tree, 'development/hook_sites.json', {}) or {}
    aliases = hs.get('owner_aliases', {})
    wrappers = hs.get('wrappers', [])
    rs = Resolver(srcs)
    mp = Map()
    mp.meta = {'root': root, 'tree': tree.label, 'builds': ga.builds, 'src_files': len(srcs), 'ga_ok': ga.ok, 'registry': reg['present'],
               'max_hooks': core['max_hooks'], 'max_handles': core['max_handles'], 'peer_owners': core['peer_owners'],
               'blocked_registry': reg['blocked'], 'blocked_core': core['blocked']}
    mp.ga, mp.reg, mp.core, mp.srcs, mp.variants, mp.resolver, mp.hook_sites, mp.tree = ga, reg, core, srcs, variants, rs, hs, tree
    rowmap = {r['ga']: r for r in reg['rows']}
    rowvar = {p['var']: p for p in reg['patch_rows']}

    def new_site(kind, ga_id, add, length, owner, dll, via, src, **kw):
        key = canon_key(ga, ga_id, add) if ga_id else kw.pop('key', None)
        return mp.add(kind=kind, ga=ga_id, add=add, key=key, rva=rvas_for(ga, ga_id, add), len=length, owner=owner, feature='',
                      dll=dll, via=via, src=src, **kw)

    # --- registry rows (the Layout manager writes them)
    for r in reg['rows']:
        new_site(r['kind'], r['ga'], 0, 8 if r['kind'] == 'vt' else 5, r['owner'], 'layout', 'layout', 'src/layout_registry.h:%d' % r['line'],
                 original={'ga': r['orig'], 'rva': rvas_for(ga, r['orig'], 0)}, flags=r['flags'], guard='layout-original',
                 method='layout_rows', snap=False, variant='layout')
    for p in reg['patch_rows']:
        new_site(p['kind'], p['ga'], 0, p['len'], p['owner'], 'layout', 'layout', 'src/layout_registry.h:%d' % p['line'],
                 flags=p['flags'], guard='exact', expect=p['expect'], method='layout_patch_row', snap=False, variant='layout')
    for p in reg['traits9']:
        new_site(p['kind'], p['ga'], 0, p['len'], p['owner'], 'layout', 'layout', 'src/layout_registry.h:%d' % p['line'],
                 flags=p['flags'], guard='layout-original', method='layout_traits9_sites', snap=False, variant='layout')

    # --- split_pointers table
    for r in parse_split_pointers(srcs, variants, reg, ga):
        sl = rs.eval(r['slot'], 'native_split_install.inl')
        og = rs.eval(r['original'], 'native_split_install.inl')
        if not sl:
            mp.unresolved.append({'file': 'module_pointers.inl', 'line': r['line'], 'call': 'split_pointers', 'arg': r['slot']})
            continue
        row = rowmap.get(sl[0])
        owner = row['owner'] if row else None
        if owner is None:
            ns = r['replacement'].split('::')
            owner = ns[0] if len(ns) > 1 else ('combat_ai:' if 'combat' in r['dlls'] else 'shared:') + r['replacement']
        if r['next']:
            mp.registrations.append({'file': 'module_pointers.inl', 'line': r['line'], 'ga': sl[0], 'add': sl[1], 'key': canon_key(ga, sl[0], sl[1]), 'dlls': ['convenience'],
                                     'kind': 'vt', 'via': 'split_pointers', 'orig': og[0] if og else None})
        else:
            for d in (r['dlls'] or ['unreached']):
                new_site('vt', sl[0], sl[1], 8, owner, d, 'direct', 'src/module_pointers.inl:%d' % r['line'],
                         original={'ga': og[0], 'rva': rvas_for(ga, og[0], 0)} if og else None, guard='exact', method='split_pointers',
                         snap=False, variant=d, flags=[])

    # --- raw patch calls
    calls = find_calls(srcs, variants, aliases, wrappers)
    ignores = hs.get('ignore_calls', [])
    bindings = hs.get('bindings', [])
    wrap_by_name = {w['name']: w for w in wrappers}

    def ignored(c, argtext):
        for ig in ignores:
            if ig.get('file') not in (None, c['file']) or ig.get('call') not in (None, c['call']) or ig.get('arg') not in (None, argtext):
                continue
            if ig.get('line_contains') and ig['line_contains'] not in srcs[c['file']].lines[c['line'] - 1]:
                continue
            ig['_hit'] = ig.get('_hit', 0) + 1
            return ig
        return None

    for c in calls:
        call, args, fn = c['call'], c['args'], c['file']
        active = bool(c['dlls'])
        dlls = c['dlls'] or ['unreached']
        src = 'src/%s:%d' % (fn, c['line'])
        if fn == 'native_layout_core.inl' and call in ('add_hook', 'hook_abs_slot'):
            continue                                   # manager implementation (wrappers with variable arguments)
        w = wrap_by_name.get(call)
        ai = w['site_arg'] if w else ADDR_ARG.get(call)
        li = w.get('len_arg') if w else LEN_ARG.get(call)

        def unresolved(arg):
            if active:
                mp.unresolved.append({'file': fn, 'line': c['line'], 'call': call, 'arg': arg})
        if call == 'hook_abs_slot':
            owner_lit = re.match(r'"(\w+)"', args[0] if args else '')
            ol = owner_lit.group(1) if owner_lit else None
            com = None
            for cs in hs.get('com_sites', []):
                if cs.get('file') == fn and cs.get('owner') == ol:
                    com = cs
                    cs['_hit'] = cs.get('_hit', 0) + 1
            if com:
                for d in dlls:
                    new_site('com', None, 0, 8, ol, d, 'layout_self', src, key='com:' + com['com'], guard='layout-original',
                             flags=['CHAIN_OK'], method=call, snap=False, variant=d)
            else:
                unresolved(args[1] if len(args) > 1 else '')
            continue
        if ai is None or ai >= len(args):
            unresolved('')
            continue
        aexpr = args[ai]
        if call in ('layout_client::block', 'layout_client::span', 'layout_client::bytes', 'add_hook'):
            mm = re.match(r'(\w+)\.rva$', aexpr)
            if mm and mm.group(1) in rowvar:
                aexpr = 'GA(%s)' % rowvar[mm.group(1)]['ga']
        if call == 'replace_code16' and re.search(r'\bblock\b', aexpr):
            continue                                   # undo/publish of an already prepared link
        ig = ignored(c, aexpr)
        if ig:
            if ig.get('loop_owner'):
                mp.registrations.append({'file': fn, 'line': c['line'], 'ga': None, 'add': 0, 'dlls': dlls, 'kind': KIND_OF.get(call),
                                         'via': call, 'loop_owners': [ig['loop_owner']]})
            continue
        kind = KIND_OF.get(call)
        if call == 'add_hook':
            kind = ADD_HOOK_KIND.get(args[1].strip(), 'block') if len(args) > 1 else 'block'
        if call == 'prepare_link':
            op = args[2].strip().lower() if len(args) > 2 else ''
            kind = 'call' if op == '0xe8' else 'jump' if op == '0xe9' else 'branch'
        if w:
            kind = BIND_KIND.get(w.get('kind'), 'call')
        ln = resolve_len(args[li], fn, rs) if li is not None and li < len(args) else None
        owner = c['owner']
        if call in ('layout_client::block', 'layout_client::span', 'layout_client::bytes'):
            mo = re.match(r'"(\w+)"', args[0])
            if mo:
                owner = mo.group(1)
            else:
                mo = re.match(r'(\w+)\.owner$', args[0])
                if mo and mo.group(1) in rowvar:
                    owner = rowvar[mo.group(1)]['owner']
        elif call == 'add_hook':
            mo = re.match(r'"(\w+)"', args[0])
            owner = mo.group(1) if mo else owner
        # --- address list: one resolved expression, or the rows of a bound table
        addrs = []
        r = rs.eval(aexpr, fn, c['off'])
        if r:
            addrs.append({'r': r, 'kind': None, 'len': None, 'owner': None, 'rows': None})
        else:
            addrs.extend(bound_addrs(c, aexpr, bindings, rs))
        if not addrs:
            lo = set()
            if call.startswith('layout_client::') or call == 'ROUTE_VT':
                if 'layout_rows' in srcs[fn].text:
                    lo |= set(re.findall(r'"(\w+)"', srcs[fn].text)) & {x['owner'] for x in reg['rows']}
                if call in ('layout_client::block', 'layout_client::span', 'layout_client::bytes') and re.match(r'"(\w+)"', args[0]):
                    lo.add(owner)
            if lo:
                mp.registrations.append({'file': fn, 'line': c['line'], 'ga': None, 'add': 0, 'dlls': dlls, 'kind': kind, 'via': call,
                                         'loop_owners': sorted(lo)})
            else:
                unresolved(aexpr)
            continue
        for a in addrs:
            r = a['r']
            k = a['kind'] or kind
            lng = a['len'] if a['len'] else ln
            if w and w.get('len_arg') is not None:
                lng = ln or 5                           # wrapper writes E9 + NOP padding over `len` bytes
            elif k in ('call', 'jump', 'branch'):
                lng = 5
            elif k == 'vt':
                lng = 8
            elif call == 'replace_code16':
                lng = 16
            own = a['owner'] or owner
            if call.startswith('layout_client::') or call == 'ROUTE_VT':
                kinds = {'layout_client::vt': 'vt', 'layout_client::call': 'call', 'layout_client::jump': 'jump', 'ROUTE_VT': 'vt'}
                rd = [d for d in dlls if call != 'ROUTE_VT' or d == 'convenience']
                if rd:
                    ent = {'file': fn, 'line': c['line'], 'ga': r[0], 'add': r[1], 'key': canon_key(ga, r[0], r[1]), 'dlls': rd, 'kind': kinds.get(call, k), 'via': call}
                    if call in ('layout_client::block', 'layout_client::span', 'layout_client::bytes'):
                        ent['owner_lit'] = own
                    mp.registrations.append(ent)
                if call != 'ROUTE_VT':
                    continue
                dd = [d for d in dlls if d != 'convenience']    # outside the routed convenience build ROUTE_VT is patch_pointer
                if not dd or canon_key(ga, r[0], r[1]) in rowmap:
                    continue                                     # the production (routed) path owns slots that have a registry row
            else:
                dd = dlls
            orig = None
            if call in ('patch_pointer', 'ROUTE_VT') and len(args) > 1:
                o2 = rs.eval(args[1], fn)
                if o2:
                    orig = {'ga': o2[0], 'rva': rvas_for(ga, o2[0], 0)}
            for d in dd:
                ln_eff = lng
                for lo in hs.get('len_overrides', []):
                    if lo.get('file') == fn and lo.get('ga') == r[0]:
                        ln_eff = lo['len']
                        lo['_hit'] = lo.get('_hit', 0) + 1
                new_site(k, r[0], r[1], ln_eff or 1, own, d, 'direct', src, original=orig, guard=a.get('guard') or DIRECT_GUARD.get(call),
                         method=call, flags=[], variant=d,
                         snap=call in ('prepare_link', 'prepare_bytes', 'replace_code16', 'call_prepare'), shared=len(dlls) > 1,
                         bound=a['rows'] is not None, table=a.get('table'), _off=c['off'])

    # --- window procedure / windows hooks / COM slots by symbol
    for fn, s in srcs.items():
        for m in re.finditer(r'\bSetWindowLongPtrW\s*\([^;]*?GWLP_WNDPROC', s.text):
            line = s.line_of(m.start())
            for d in (sorted(dd for dd, ls in variants.get(fn, {}).items() if line in ls) or ['unreached']):
                new_site('wndproc', None, 0, 8, owner_label(fn, s.text[:m.start()], aliases), d, 'direct', 'src/%s:%d' % (fn, line),
                         key='wndproc:game', guard='none', method='SetWindowLongPtrW', flags=[], variant=d, snap=False)
        for m in re.finditer(r'\bSetWindowsHookExW\s*\(\s*(\w+)', s.text):
            line = s.line_of(m.start())
            for d in (sorted(dd for dd, ls in variants.get(fn, {}).items() if line in ls) or ['unreached']):
                new_site('winhook', None, 0, 0, owner_label(fn, s.text[:m.start()], aliases), d, 'direct', 'src/%s:%d' % (fn, line),
                         key='winhook:' + m.group(1), guard='none', method='SetWindowsHookExW', flags=[], variant=d, snap=False)
    for cs in hs.get('com_sites', []):
        if 'symbol' not in cs or cs.get('file') not in srcs:
            continue
        s = srcs[cs['file']]
        m = re.search(r'\b%s\b' % re.escape(cs['symbol']), s.text)
        if not m:
            continue
        cs['_hit'] = cs.get('_hit', 0) + 1
        line = s.line_of(m.start())
        for d in (sorted(dd for dd, ls in variants.get(cs['file'], {}).items() if line in ls) or ['unreached']):
            new_site('com', None, 0, 8, cs.get('owner') or norm_stem(cs['file']), d, 'direct', 'src/%s:%d' % (cs['file'], line),
                     key='com:' + cs['com'], guard='exact', method='com_site', flags=[], variant=d, snap=False)
    for st in mp.sites:
        if st['via'] == 'direct' and st['guard'] is None:
            st['guard'] = guard_for(mp, st)
    return mp


def enclosing_function(text_before):
    ms = list(re.finditer(r'^(?:\[\[maybe_unused\]\]\s*)?(?:static|template[^\n]*\n\s*static)\s[^\n(=;]*?\b(\w+)\s*\([^;{)]*\)\s*\{', text_before, re.M))
    return ms[-1].group(1) if ms else None


def bound_addrs(c, aexpr, bindings, rs):
    """address list for an argument that is not a plain GA expression: rows of a table named by a hook_sites.json binding."""
    fn = c['file']
    out = []
    for b in bindings:
        if b.get('file') not in (None, fn) or b.get('call') not in (None, c['call']):
            continue
        if b.get('arg') is not None and b['arg'] != aexpr:
            continue
        if b.get('in_function') and enclosing_function(rs.srcs[fn].text[:c['off']]) != b['in_function']:
            continue
        tables = b.get('tables') or ([b['table']] if b.get('table') else [])
        made = 0
        for tn in tables:
            tf = b.get('table_file') or fn
            t = rs.table(tn, tf)
            if not t:
                continue
            typ, rows, dfile = t
            fi = rs.field_index(typ, b.get('field', 0), dfile)
            if fi is None:
                continue
            for idx, r in enumerate(rows):
                if b.get('rows') is not None and idx not in b['rows']:
                    continue
                v = rs.eval(r[fi], dfile) if fi < len(r) else None
                if not v:
                    continue
                g = b.get('guard') or table_guard(rs, fn, tn)
                out.append({'r': v, 'kind': BIND_KIND.get(b.get('kind')), 'len': b.get('len'), 'owner': b.get('owner'), 'rows': True,
                            'table': tn, 'guard': g})
                made += 1
        if made:
            b['_hit'] = b.get('_hit', 0) + 1
            return out
    return out


def table_guard(rs, fn, table):
    """a loop over the same table that compares bytes / slot pointers / opcodes counts as the guard of every row."""
    s = rs.srcs.get(fn)
    pat = r'for\s*\([^)]*\b%s\b[^)]*\)[\s\S]{0,400}?(mcmp|pointer\(|!=\s*0x[Ee][89]|memcmp)' % re.escape(table)
    if s and re.search(pat, s.text):
        return 'exact'
    return None


GUARD_PATTERNS = (r'ga::mcmp\w*\s*\(\s*image\s*,\s*(?:GA\(\s*)?%s\b', r'mcmp\w*\([^;]*?\b%s\b', r'\{\s*GA\(\s*%s\s*\)\s*,\s*\d+\s*,\s*GH\(',
                  r'image\s*\[\s*(?:GA\(\s*)?%s\b[^\]]*\]\s*!=\s*0x[Ee][89]', r'pointer\(image\s*,\s*(?:GA\(\s*)?%s\b', r'GH\([^;]*%s\b')


FUNC_GUARD = re.compile(r'mcmp\w*\s*\(|\bmemcmp\s*\(|\bGH\s*\(|\bguards\w*\s*\(|_guards_match\s*\(|\bpointer\s*\(\s*image|image\s*\[[^\]]*\]\s*[!=]=\s*0x[0-9A-Fa-f]{2}')


def guard_for(mp, st):
    """heuristic guard class of a direct patch: exact (bytes/hash compare earlier in the same function, or a compare of the id), none."""
    ga_id = st.get('ga')
    if not ga_id:
        return 'none'
    fn = st['src'].split(':')[0][4:]
    off = st.get('_off')
    if off is not None:
        text = mp.srcs[fn].text[:off]
        ms = list(re.finditer(r'^(?:\[\[maybe_unused\]\]\s*)?(?:static|template[^\n]*\n\s*static)\s[^\n(=;]*?\b(\w+)\s*\([^;{)]*\)\s*\{', text, re.M))
        body = text[ms[-1].start():] if ms else ''
        if FUNC_GUARD.search(body):
            return 'exact'
    stem = norm_stem(fn).split('_')[0]
    names = {ga_id}
    for name, d in mp.resolver.consts.items():
        if any(e.startswith('GA(%s)' % ga_id) for _o, e in d.get(fn, [])):
            names.add(name)
    for f in mp.srcs:
        if not (f == fn or norm_stem(f).split('_')[0] == stem or f.endswith('_profile.h')):
            continue
        t = mp.srcs[f].text
        for nm in names:
            if nm not in t:
                continue
            q = re.escape(nm)
            if any(re.search(p % q, t) for p in GUARD_PATTERNS):
                return 'exact'
    return 'none'
