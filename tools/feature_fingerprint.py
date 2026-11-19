#!/usr/bin/env python
"""Feature fingerprint: the behaviour surface of the built mod, for refactor equivalence (user 2026-11-16:
"검사는 리팩토링 효율화 후에도 현재 우리 모드 기능을 유지하는걸 반드시 증명할 수 있어야해").

    python -B -X utf8 tools/feature_fingerprint.py capture --name NAME --results build/development/<run>.jsonl
           [--diag DIR] [--release DIR] [--hook-map FILE]      -> development/fingerprint/NAME.json
    python -B -X utf8 tools/feature_fingerprint.py compare BASE.json NEW.json [--allow FILE] [--same-code]
    python -B -X utf8 tools/feature_fingerprint.py selftest

Surface (docs/REFACTOR_EQUIVALENCE.md):
  dlls      both variants (loader_split_diagnostic, loader_split): exports, the string set of the data sections
            (ini sections/keys, log event names, messages, shader/asset names; build paths dropped), .text hash
  hooks     build/development/hook_map.json sites without source position (kind, address, length, owner, feature, DLL,
            original, flags, guard, method): every patched game address and who owns it
  settings  development/installer_map.json rows (section, key, default, module, retired)
  features  development/features.json modules -> check routes
  routes    last output line of every passed check step (tools/run_checks.py --results): status, check counts, hashes;
            timing fields dropped
compare fails on any lost or changed item; a check count may grow, never shrink. Added strings/steps are listed
(new surface must be intended). --same-code also requires identical .text bytes (tooling-only changes).
Standard library only. Writes only development/fingerprint/.
"""
import argparse, copy, hashlib, json, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from pe_image import PEImage  # noqa: E402

DLLS = ('dinput8.dll', 'SAN14_Layout.dll', 'SAN14_Convenience.dll', 'SAN14_CombatAI.dll', 'SAN14_CCZ.dll', 'SAN14_ModPack.dll')
VARIANTS = {'diag': 'build/development/loader_split_diagnostic', 'release': 'build/development/loader_split'}
SITE_KEYS = ('kind', 'ga', 'add', 'key', 'rva', 'len', 'owner', 'feature', 'dll', 'via', 'original', 'flags', 'guard', 'method', 'snap', 'variant')
TIMING = re.compile(r'(^|_)(ms|s|secs?|seconds|ratio|wall|elapsed|time|bench\w*|benchmarks?|us|per_sec|speedup)$|_ms$|_ratio$|benchmark', re.I)
PATHY = re.compile(r'[A-Za-z]:[\\/]|\.pdb\b|AI_TacticsMod|\\Users\\|/Users/', re.I)
NARROW = re.compile(rb'(?:[\x20-\x7e]|[\xea-\xed][\x80-\xbf][\x80-\xbf]){5,}')   # ASCII or UTF-8 Hangul (/utf-8 literals)
WIDE = re.compile(rb'(?:[\x20-\x7e]\x00|[\x00-\xff][\xac-\xd7]){4,}')   # ASCII or Hangul syllables (U+AC00-D7A3) in UTF-16LE
HANGUL = re.compile('[가-힣]')
HANGUL2 = re.compile('[가-힣]{2}')


def texty(v):
    """Real text, not a relocated RVA/jump table that happens to decode (those move with every code layout change)."""
    if HANGUL.search(v):
        return bool(HANGUL2.search(v))
    return any(c.isalpha() for c in v)


# --- source-literal string rule (2026-11-16 multi-unit refactor) -------------------------------------------------------
# The string set is the maximal printable runs of the data sections, so a string's run also takes in printable bytes of
# whatever the linker put in front of it (float constants, the "0001..99" digit table, a stray wide char). A code change
# moves those neighbours: '# %.1f %s %p %p' becomes '?# %.1f %s %p %p', 'W:9ranges' becomes 'W:ranges', and runs that are
# only float constants ('8@HPXb', '?ff&?333?') appear or vanish. A C string ends at its NUL, so the neighbour can only be
# in front. Without --same-code, compare therefore checks the mod's own strings: every piece of every string literal of
# both source trees (git archive of each fingerprint's results commit) that some run ENDS with. A piece the base DLL has
# and the new DLL lacks fails, and so does a piece fewer runs end with. Runs no literal explains (toolchain strings,
# constants) are listed, not enforced.
def _narrow_ok(c):   # mirrors NARROW: printable ASCII or a 3-byte UTF-8 sequence with lead byte EA-ED
    o = ord(c)
    return 0x20 <= o <= 0x7e or 0xA000 <= o <= 0xDFFF


def _wide_ok(c):   # mirrors WIDE: printable ASCII or a UTF-16 unit whose high byte is AC-D7
    o = ord(c)
    return 0x20 <= o <= 0x7e or 0xAC00 <= o <= 0xD7FF


_ESC = {'n': '\n', 't': '\t', 'r': '\r', '0': '\0', 'a': '\a', 'b': '\b', 'f': '\f', 'v': '\v', '\\': '\\', '"': '"', "'": "'", '?': '?'}


def _unescape(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c != '\\' or i + 1 >= len(s):
            out.append(c)
            i += 1
            continue
        n = s[i + 1]
        if n == 'x':
            m = re.match(r'[0-9A-Fa-f]+', s[i + 2:])
            out.append(chr(int(m.group(), 16) % 0x110000) if m else 'x')
            i += 2 + (len(m.group()) if m else 0)
        elif n in 'uU':
            k = 4 if n == 'u' else 8
            h = s[i + 2:i + 2 + k]
            out.append(chr(int(h, 16)) if re.fullmatch(r'[0-9A-Fa-f]{%d}' % k, h) and int(h, 16) < 0x110000 else '\x01')
            i += 2 + k
        elif n in '01234567':
            m = re.match(r'[0-7]{1,3}', s[i + 1:])
            out.append(chr(int(m.group(), 8)))
            i += 1 + len(m.group())
        else:
            out.append(_ESC.get(n, '\x01'))
            i += 2
    return ''.join(out)


def literals_of_text(text):
    """[(kind 'n'|'w', literal text)] of one C/C++ source; adjacent literals are also joined (over-extraction only adds
    candidates; a missed literal would be checked in neither DLL)."""
    out, i, n = [], 0, len(text)
    run = []   # (kind, text) of the current sequence of adjacent literals
    while i < n:
        c = text[i]
        if c == '/' and text.startswith('//', i):
            i = text.find('\n', i)
            i = n if i < 0 else i
            continue
        if c == '/' and text.startswith('/*', i):
            j = text.find('*/', i + 2)
            i = n if j < 0 else j + 2
            continue
        if c == "'":
            k = i - 1
            while k >= 0 and (text[k].isalnum() or text[k] == '_'):
                k -= 1
            if i - k > 1 and text[k + 1].isdigit():   # digit separator (1'000)
                i += 1
                continue
            j = i + 1
            while j < n and text[j] not in "'\n":
                j += 2 if text[j] == '\\' else 1
            i = j + 1
            if run:
                out.extend(_joined(run))
                run = []
            continue
        if c == '"':
            m = re.search(r'(?:\b(u8|u|U|L)?(R)?)$', text[max(0, i - 3):i])
            prefix, raw = (m.group(1) or '', m.group(2)) if m else ('', None)
            if raw:
                d = re.match(r'([^()\\\s]{0,16})\(', text[i + 1:])
                if not d:
                    i += 1
                    continue
                end = text.find(')' + d.group(1) + '"', i + 2 + len(d.group(1)))
                end = n if end < 0 else end
                body = text[i + 2 + len(d.group(1)):end]
                i = end + len(d.group(1)) + 2
            else:
                j = i + 1
                while j < n and text[j] not in '"\n':
                    j += 2 if text[j] == '\\' else 1
                body = _unescape(text[i + 1:j])
                i = j + 1
            if prefix != 'U':
                run.append(('w' if prefix in ('L', 'u') else 'n', body))
            continue
        if not c.isspace() and run:
            out.extend(_joined(run))
            run = []
        i += 1
    if run:
        out.extend(_joined(run))
    return out


def _joined(run):
    out = list(run)
    if len(run) > 1 and len({k for k, _ in run}) == 1:
        out.append((run[0][0], ''.join(t for _, t in run)))
    return out


_FLAG_LIST = re.compile(r'#\s*define\s+(\w+)\s*\(\s*(\w+)\s*\)((?:[^\n]*\\\n)*[^\n]*)')


def stringized(text):
    """Names the sources turn into strings with #x: X-macro lists (#define LIST(X) X(a) X(b)) applied to a one-parameter
    macro that stringizes its parameter (module_closure.inl MODULE_FLAG_ROW over module_flags.h MODULE_FLAG_LIST)."""
    lists, stringize = {}, set()
    for m in _FLAG_LIST.finditer(text):
        name, param, body = m.group(1), m.group(2), m.group(3).replace('\\\n', ' ')
        if re.search(r'(?<!#)#\s*%s\b' % re.escape(param), body):
            stringize.add(name)
        items = re.findall(r'\b%s\s*\(\s*(\w+)\s*\)' % re.escape(param), body)
        if items:
            lists[name] = items
    out = []
    for lst, items in lists.items():
        for s in stringize:
            if re.search(r'\b%s\s*\(\s*%s\s*\)' % (re.escape(lst), re.escape(s)), text):
                out += [('n', x) for x in items]
    return out


def pieces(kind, s):
    """Maximal runs of the extractor's alphabet in a literal, stripped and filtered as dll_surface filters its runs."""
    ok, least = (_wide_ok, 4) if kind == 'w' else (_narrow_ok, 5)
    out, cur = [], []
    for c in s + '\0':
        if ok(c):
            cur.append(c)
            continue
        p = ''.join(cur).strip()
        cur = []
        if len(p) >= least and not PATHY.search(p) and texty(p):
            out.append(p)
    return out


# Every C/C++ text the DLL compiles may include (src/season/*.inc carries the season engine's log text).
SOURCE_EXT = ('.cpp', '.cc', '.cxx', '.c', '.h', '.hh', '.hpp', '.hxx', '.inl', '.inc', '.ipp', '.hlsl')


def source_literals(commit):
    """{'n': set, 'w': set} of literal pieces of src/ at a commit, or None when it cannot be read exactly."""
    import io, subprocess, tarfile
    if not commit or not re.fullmatch(r'[0-9a-f]{7,40}', commit):
        return None
    p = subprocess.run(['git', '-C', ROOT, 'archive', commit, 'src'], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if p.returncode:
        return None
    lits = {'n': set(), 'w': set()}
    whole = []
    with tarfile.open(fileobj=io.BytesIO(p.stdout)) as t:
        for m in t.getmembers():
            if m.isfile() and m.name.lower().endswith(SOURCE_EXT):
                text = t.extractfile(m).read().decode('utf-8-sig', 'replace').replace('\r\n', '\n')
                whole.append(text)
                for kind, s in literals_of_text(text):
                    lits[kind].update(pieces(kind, s))
    for kind, s in stringized('\n'.join(whole)):
        lits[kind].update(pieces(kind, s))
    return lits


class Tails:
    """Runs of one DLL by kind, reversed and sorted: how many distinct runs end with a piece."""
    def __init__(self, strings):
        import bisect
        self.bisect = bisect
        self.rev = {'n': sorted(s[::-1] for s in strings if not s.startswith('W:')),
                    'w': sorted(s[2:][::-1] for s in strings if s.startswith('W:'))}

    def count(self, kind, piece):
        r, a = piece[::-1], self.rev[kind]
        return self.bisect.bisect_left(a, r + '\U0010ffff') - self.bisect.bisect_left(a, r)


def explained(strings, lits):
    """Runs that end with some literal piece (the rest: toolchain strings, constants, glue-only runs)."""
    lengths = {k: sorted({len(p) for p in v}) for k, v in lits.items()}
    out = set()
    for s in strings:
        kind, t = ('w', s[2:]) if s.startswith('W:') else ('n', s)
        if any(t[len(t) - k:] in lits[kind] for k in lengths[kind] if k <= len(t)):
            out.add(s)
    return out


def string_rule(base, new, same_code, literals=None):
    """(literal pieces or None, rule name). None = the exact run comparison (fails closed)."""
    if same_code:
        return None, 'runs (--same-code)'
    if literals is not None:
        return literals, 'source literals'
    bc, nc = base.get('results_commits') or [], new.get('results_commits') or []
    if len(bc) != 1 or len(nc) != 1:
        return None, 'runs (a fingerprint has no single clean results commit)'
    lb, ln = source_literals(bc[0]), source_literals(nc[0])
    if lb is None or ln is None:
        return None, 'runs (source of %s or %s not readable)' % (bc[0], nc[0])
    return {k: lb[k] | ln[k] for k in lb}, 'source literals'


def sha(b):
    return hashlib.sha256(b).hexdigest()


def exports(pe):
    opt = struct.unpack_from('<I', pe.data, 0x3c)[0] + 24
    rva = struct.unpack_from('<I', pe.data, opt + 112)[0]
    if not rva:
        return []
    header = pe.read(rva, 40)
    count, table = struct.unpack_from('<I', header, 24)[0], struct.unpack_from('<I', header, 32)[0]
    return sorted(pe.read(struct.unpack('<I', pe.read(table + i * 4, 4))[0], 80).split(b'\0')[0].decode() for i in range(count))


def dll_surface(path):
    pe = PEImage(path)
    strings, text = set(), b''
    for s in pe.sections:
        raw = pe.data[s['file_offset']:s['file_offset'] + s['raw_size']]
        if s['name'] == '.text':
            text += raw[:s['virtual_size']] if s['virtual_size'] else raw
            continue
        if s['name'] in ('.reloc', '.pdata', '.rsrc'):
            continue
        raw = re.sub(rb'RSDS[\s\S]{20}', b'\0' * 24, raw)   # CodeView record: 'RSDS' + PDB GUID + age differ in every build
        for m in NARROW.finditer(raw):
            v = m.group().decode('utf-8', 'replace').strip()
            if len(v) >= 5 and not PATHY.search(v) and texty(v):
                strings.add(v)
        for m in WIDE.finditer(raw):
            try:
                v = m.group().decode('utf-16-le').strip()
            except UnicodeDecodeError:
                continue
            if len(v) >= 4 and not PATHY.search(v) and texty(v):
                strings.add('W:' + v)
    return {'exports': exports(pe), 'strings': sorted(strings), 'text_sha256': sha(text), 'text_bytes': len(text)}


def strip_timing(v):
    if isinstance(v, dict):
        return {k: strip_timing(x) for k, x in v.items() if not TIMING.search(k)}
    if isinstance(v, list):
        return [strip_timing(x) for x in v]
    return v


COMPILE_MSG = re.compile(r'^[\w.-]+\.(cpp|c)$|\.lib .*\.exp|Creating library', re.I)


def unroot(v):
    """Worktree-independent: the project path becomes <root> in every string."""
    if isinstance(v, dict):
        return {k: unroot(x) for k, x in v.items()}
    if isinstance(v, list):
        return [unroot(x) for x in v]
    if isinstance(v, str):
        for r in (ROOT, ROOT.replace('\\', '/'), ROOT.replace('\\', '\\\\')):
            v = v.replace(r, '<root>')
        return v
    return v


def norm_line(line, cl=False):
    """A compile step proves only that it compiled; its last line is the toolchain's file/library message."""
    if cl or COMPILE_MSG.search(line):
        return '<compiled>'
    try:
        return unroot(strip_timing(json.loads(line)))
    except ValueError:
        return unroot(re.sub(r'\d+\.\d+\s*(ms|s)\b', '<t>', line))


def capture(a):
    fp = {'schema': 1, 'dlls': {}, 'hooks': [], 'settings': [], 'features': {}, 'routes': {}}
    dirs = {'diag': a.diag or VARIANTS['diag'], 'release': a.release or VARIANTS['release']}
    for var, d in dirs.items():
        for n in DLLS:
            p = os.path.join(ROOT, d, n) if not os.path.isabs(d) else os.path.join(d, n)
            if os.path.exists(p):
                fp['dlls'][var + '/' + n] = dll_surface(p)
    hm = a.hook_map or os.path.join(ROOT, 'build', 'development', 'hook_map.json')
    with open(hm, encoding='utf-8') as f:
        sites = json.load(f)['sites']
    fp['hooks'] = sorted(json.dumps({k: s.get(k) for k in SITE_KEYS}, sort_keys=True, ensure_ascii=False) for s in sites)
    with open(os.path.join(ROOT, 'development', 'installer_map.json'), encoding='utf-8') as f:
        items = json.load(f)
    items = items if isinstance(items, list) else next(v for v in items.values() if isinstance(v, list))
    fp['settings'] = sorted('%s.%s=%s@%s%s' % (it.get('section'), it.get('key'), it.get('default'), it.get('module'), ' retired' if it.get('retired') else '') for it in items)
    with open(os.path.join(ROOT, 'development', 'features.json'), encoding='utf-8') as f:
        feats = json.load(f)
    fp['features'] = {n: sorted(set(m.get('tests') or [])) for n, m in feats['modules'].items()}
    if a.results:   # the last entry of a step wins (a later failure removes an earlier pass); one source state only
        last, commits = {}, set()
        with open(a.results, encoding='utf-8') as f:
            for line in f:
                if line.strip():
                    r = json.loads(line)
                    last[r['route'] + '/' + r['step']] = r
                    commits.add(r.get('commit') or 'unrecorded')
        if len(commits) > 1 and not a.mixed:
            raise SystemExit('results come from several source states %s; rerun all routes into a fresh file (or --mixed with a reason)' % sorted(commits))
        fp['results_commits'] = sorted(commits)
        for k, r in last.items():
            if r.get('ok', True):
                fp['routes'][k] = norm_line(r['line'], r.get('cl', False))
    if a.note:
        fp['note'] = a.note
    out = a.out or os.path.join(ROOT, 'development', 'fingerprint', a.name + '.json')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(fp, f, ensure_ascii=False, indent=1, sort_keys=True)
        f.write('\n')
    print(json.dumps({'status': 'fingerprint_captured', 'file': os.path.relpath(out, ROOT).replace('\\', '/'), 'dlls': len(fp['dlls']),
                      'strings': sum(len(v['strings']) for v in fp['dlls'].values()), 'hooks': len(fp['hooks']),
                      'settings': len(fp['settings']), 'features': len(fp['features']), 'route_steps': len(fp['routes'])}))
    return 0


def checks_of(v):
    return v.get('checks') if isinstance(v, dict) and isinstance(v.get('checks'), int) else None


def diff(base, new, allow=(), same_code=False, literals=None, info=None):
    """(failures, notes) as lists of strings; info (dict) receives the string rule and its coverage."""
    fails, notes = [], []
    allowed = set(allow)
    lits, rule = string_rule(base, new, same_code, literals)
    unexplained_lost = unexplained_added = 0
    coverage = {}

    def fail(msg, key):
        (notes if key in allowed else fails).append(msg + (' (allowed)' if key in allowed else ''))
    for k, b in base['dlls'].items():
        n = new['dlls'].get(k)
        if n is None:
            fail('dll missing: ' + k, 'dll:' + k)
            continue
        if b['exports'] != n['exports']:
            fail('%s exports %s -> %s' % (k, b['exports'], n['exports']), 'exports:' + k)
        if lits is None:
            lost = sorted(set(b['strings']) - set(n['strings']))
            added = sorted(set(n['strings']) - set(b['strings']))
            for s in lost:
                fail('%s lost string %r' % (k, s), 'string:' + s)
            for s in added:
                notes.append('%s added string %r' % (k, s))
        else:
            tb, tn = Tails(b['strings']), Tails(n['strings'])
            for kind in ('n', 'w'):
                for p in sorted(lits[kind]):
                    cb, cn = tb.count(kind, p), tn.count(kind, p)
                    s = ('W:' if kind == 'w' else '') + p
                    if cb and not cn:
                        fail('%s lost string %r' % (k, s), 'string:' + s)
                    elif cn < cb:
                        fail('%s fewer strings end with %r (%d -> %d)' % (k, s, cb, cn), 'string:' + s)
                    elif cn and not cb:
                        notes.append('%s added string %r' % (k, s))
            ex = explained(b['strings'], lits)
            gone = sorted(set(b['strings']) - set(n['strings']) - ex)
            appeared = sorted(set(n['strings']) - set(b['strings']) - explained(n['strings'], lits))   # new imports, RTTI, CRT, constants
            unexplained_lost += len(gone)
            unexplained_added += len(appeared)
            coverage[k] = '%d/%d' % (len(ex), len(b['strings']))
            notes.append('%s: %d of %d strings end with a source literal; %d other strings gone and %d appeared with the layout' % (
                k, len(ex), len(b['strings']), len(gone), len(appeared)))
            for s in gone:
                notes.append('%s layout-only string gone (no source literal ends it): %r' % (k, s))
            for s in appeared:
                notes.append('%s layout-only string added (no source literal ends it; a new import or toolchain name shows here): %r' % (k, s))
        if b['text_sha256'] != n['text_sha256']:
            (fails if same_code else notes).append('%s code bytes changed (%d -> %d)' % (k, b['text_bytes'], n['text_bytes']))
    for k in sorted(set(new['dlls']) - set(base['dlls'])):
        notes.append('dll added: ' + k)
    bh, nh = set(base['hooks']), set(new['hooks'])
    for s in sorted(bh - nh):
        fail('hook site lost or changed: ' + s, 'hook:' + s)
    for s in sorted(nh - bh):
        fail('hook site added or changed: ' + s, 'hook:' + s)
    for s in sorted(set(base['settings']) - set(new['settings'])):
        fail('setting lost or changed: ' + s, 'setting:' + s)
    for s in sorted(set(new['settings']) - set(base['settings'])):
        notes.append('setting added: ' + s)
    for m, tests in base['features'].items():
        if m not in new['features']:
            fail('feature module lost: ' + m, 'feature:' + m)
        elif set(tests) - set(new['features'][m]):
            fail('feature %s lost checks %s' % (m, sorted(set(tests) - set(new['features'][m]))), 'feature:' + m)
    for k, b in base['routes'].items():
        if k not in new['routes']:
            fail('check step missing or failed: ' + k, 'route:' + k)
            continue
        n = new['routes'][k]
        b, n = strip_timing(b), strip_timing(n)   # again here: a timing key learned later also clears stored fingerprints
        cb, cn = checks_of(b), checks_of(n)
        if cb is not None and cn is not None:
            if cn < cb:
                fail('%s check count %d -> %d' % (k, cb, cn), 'route:' + k)
            elif cn > cb:
                notes.append('%s check count %d -> %d' % (k, cb, cn))
            b2, n2 = dict(b), dict(n)
            b2.pop('checks'), n2.pop('checks')
            if b2 != n2:
                fail('%s result changed: %s -> %s' % (k, json.dumps(b2, ensure_ascii=False)[:300], json.dumps(n2, ensure_ascii=False)[:300]), 'route:' + k)
        elif b != n:
            fail('%s result changed: %s -> %s' % (k, json.dumps(b, ensure_ascii=False)[:300], json.dumps(n, ensure_ascii=False)[:300]), 'route:' + k)
    for k in sorted(set(new['routes']) - set(base['routes'])):
        notes.append('check step added: ' + k)
    if info is not None:
        info.update(string_rule=rule, layout_only_strings_gone=unexplained_lost, layout_only_strings_added=unexplained_added,
                    literal_coverage=coverage)
    return fails, notes


def load(p):
    with open(p, encoding='utf-8') as f:
        return json.load(f)


def compare(a):
    allow = []
    if a.allow:
        allow = load(a.allow).get('allow', [])
    info = {}
    fails, notes = diff(load(a.base), load(a.new), allow, a.same_code, info=info)
    for n in notes[:400]:
        print('note: ' + n)
    if len(notes) > 400:
        print('note: ... %d more' % (len(notes) - 400))
    for f in fails[:300]:
        print('FAIL: ' + f)
    print(json.dumps(dict({'status': 'fingerprint_equal' if not fails else 'fingerprint_differs', 'failures': len(fails), 'notes': len(notes),
                           'same_code': a.same_code}, **info), ensure_ascii=False))
    return 1 if fails else 0


def selftest(_a):
    """Each mutation class of the surface must fail compare; the identity must pass."""
    base = {'schema': 1,
            'dlls': {'diag/SAN14_CCZ.dll': {'exports': ['SAN14CczGetApi'], 'strings': ['ccz_draw_probe', 'ccz_probe_ctx', 'W:sprite_fallback'],
                                            'text_sha256': 'a', 'text_bytes': 10}},
            'hooks': ['{"ga": "GA_1", "owner": "x"}', '{"ga": "GA_2", "owner": "y"}'],
            'settings': ['ccz.ccz_sprites=1@ccz-sprites', 'diagnostics.ccz_draw_probe=0@ccz-sprites'],
            'features': {'ccz-sprites': ['ccz-sprites', 'split-module']},
            'routes': {'ccz-sprites/verify': {'status': 'ok', 'checks': 91}, 'manifest/s0': 'manifest PASSED (41 rows)'}}
    muts = {
        'export removed': lambda f: f['dlls']['diag/SAN14_CCZ.dll'].update(exports=[]),
        'ini key renamed': lambda f: f['dlls']['diag/SAN14_CCZ.dll']['strings'].__setitem__(0, 'ccz_draw_probX'),
        'log event removed': lambda f: f['dlls']['diag/SAN14_CCZ.dll']['strings'].remove('ccz_probe_ctx'),
        'dll missing': lambda f: f['dlls'].clear(),
        'hook row dropped': lambda f: f['hooks'].pop(),
        'hook owner changed': lambda f: f['hooks'].__setitem__(0, '{"ga": "GA_1", "owner": "z"}'),
        'setting default changed': lambda f: f['settings'].__setitem__(0, 'ccz.ccz_sprites=0@ccz-sprites'),
        'feature check dropped': lambda f: f['features']['ccz-sprites'].remove('split-module'),
        'check count dropped': lambda f: f['routes']['ccz-sprites/verify'].update(checks=90),
        'check status changed': lambda f: f['routes']['ccz-sprites/verify'].update(status='bad'),
        'check step failed': lambda f: f['routes'].pop('manifest/s0'),
        'text result changed': lambda f: f['routes'].__setitem__('manifest/s0', 'manifest PASSED (40 rows)'),
    }
    bad = []
    if diff(base, copy.deepcopy(base))[0]:
        bad.append('identity')
    for name, m in muts.items():
        f = copy.deepcopy(base)
        m(f)
        if not diff(base, f)[0]:
            bad.append(name)
    grow = copy.deepcopy(base)
    grow['routes']['ccz-sprites/verify']['checks'] = 95
    grow['dlls']['diag/SAN14_CCZ.dll']['text_sha256'] = 'b'
    if diff(base, grow)[0]:
        bad.append('check count growth / code change must not fail')
    if not diff(base, grow, same_code=True)[0]:
        bad.append('--same-code must fail on a code change')
    print(json.dumps({'status': 'fingerprint_selftest_passed' if not bad else 'fingerprint_selftest_failed', 'mutations': len(muts), 'failures': bad}))
    return 1 if bad else 0


def selftest_strings(_a):
    """The source-literal string rule: the lexer finds the literals, layout glue passes, a lost literal fails."""
    bad = []
    src = '\n'.join([
        '// "comment_text_xx" is not a literal',
        '/* "block_comment_xx" */',
        "int big = 1'000'000; char q = '\"'; char e = '\\'';",
        'log_event("ccz_draw_probe");',
        'const wchar_t *w = L"sprite_fallback";',
        'const char16_t *h = u"utf16_word";',
        'const char32_t *x = U"skipped_u32";',
        'const char *r = R"tag(raw "inner" text)tag";',
        'const char *j = "joined_" "pieces_tail";',
        'const char *k = u8"\\x41scaped\\tafter_tab";',
        'const wchar_t *g = L"승리했습니다";',
        '#define FLAG_LIST(X) X(alpha_flag) \\',
        '    X(beta_flag)',
        '#define FLAG_ROW(name) {#name, 0},',
        'static const Row rows[] = { FLAG_LIST(FLAG_ROW) };',
    ])
    lits = {'n': set(), 'w': set()}
    for kind, s in literals_of_text(src) + stringized(src):
        lits[kind].update(pieces(kind, s))
    want_n = {'ccz_draw_probe', 'raw "inner" text', 'joined_', 'pieces_tail', 'joined_pieces_tail', 'Ascaped', 'after_tab',
              'alpha_flag', 'beta_flag'}
    want_w = {'sprite_fallback', 'utf16_word', '승리했습니다'}
    if not want_n <= lits['n']:
        bad.append('lexer missed narrow %s' % sorted(want_n - lits['n']))
    if not want_w <= lits['w']:
        bad.append('lexer missed wide %s' % sorted(want_w - lits['w']))
    leaked = {p for p in lits['n'] | lits['w'] if 'comment' in p or 'skipped' in p or '000' in p}
    if leaked:
        bad.append('lexer took comments/U"" literals/digit separators: %s' % sorted(leaked))
    if '.inc' not in SOURCE_EXT or source_literals('not-a-commit') is not None:
        bad.append('source reader: .inc not scanned or a non-commit accepted')

    def fp(strings, commits=('a' * 40,)):
        return {'schema': 1, 'results_commits': list(commits),
                'dlls': {'diag/SAN14_CCZ.dll': {'exports': ['SAN14CczGetApi'], 'strings': list(strings), 'text_sha256': 'a', 'text_bytes': 10}},
                'hooks': [], 'settings': [], 'features': {}, 'routes': {}}
    base = fp(['?ccz_draw_probe', 'ccz_draw_probe_ctx', 'Q@pieces_tail', 'pieces_tail', 'W:9sprite_fallback', '8@HPXb'])
    cases = {
        # name: (new strings, must fail)
        'identity passes': (base['dlls']['diag/SAN14_CCZ.dll']['strings'], False),
        'glue moved, float run gone passes': (['ccz_draw_probe', 'ccz_draw_probe_ctx', 'Z#pieces_tail', 'pieces_tail', 'W:sprite_fallback'], False),
        'literal lost fails': (['ccz_draw_probe_ctx', 'Q@pieces_tail', 'pieces_tail', 'W:9sprite_fallback'], True),
        'wide literal lost fails': (['?ccz_draw_probe', 'ccz_draw_probe_ctx', 'Q@pieces_tail', 'pieces_tail'], True),
        'literal renamed fails': (['?ccz_draw_probX', 'ccz_draw_probe_ctx', 'Q@pieces_tail', 'pieces_tail', 'W:9sprite_fallback'], True),
        'fewer runs end with a literal fails': (['?ccz_draw_probe', 'ccz_draw_probe_ctx', 'pieces_tail', 'W:9sprite_fallback'], True),
    }
    for name, (strings, must_fail) in cases.items():
        info = {}
        fails = diff(base, fp(strings), literals=lits, info=info)[0]
        if bool(fails) != must_fail or info.get('string_rule') != 'source literals':
            bad.append('%s: %s' % (name, fails or 'passed'))
    info = {}
    fails, notes = diff(base, fp(base['dlls']['diag/SAN14_CCZ.dll']['strings'] + ['BCryptHashData']), literals=lits, info=info)
    if fails or info.get('layout_only_strings_added') != 1 or not any('layout-only string added' in x and 'BCryptHashData' in x for x in notes):
        bad.append('a new unexplained run (import name) must be listed, not failed: %s' % (fails or info))
    glue = fp(cases['glue moved, float run gone passes'][0])
    for name, b2, n2, same in (('two results commits -> exact runs', fp(base['dlls']['diag/SAN14_CCZ.dll']['strings'], ('a' * 40, 'b' * 40)), glue, False),
                               ('dirty/non-commit -> exact runs', fp(base['dlls']['diag/SAN14_CCZ.dll']['strings'], ('a' * 40 + '+dirty',)), glue, False),
                               ('--same-code -> exact runs', base, glue, True)):
        info = {}
        if not diff(b2, n2, same_code=same, info=info)[0] or not info.get('string_rule', '').startswith('runs'):
            bad.append(name + ': glue change was not checked run by run')
    print(json.dumps({'status': 'fingerprint_strings_selftest_passed' if not bad else 'fingerprint_strings_selftest_failed',
                      'cases': len(cases) + 4, 'failures': bad}, ensure_ascii=False))
    return 1 if bad else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    c = sub.add_parser('capture')
    c.add_argument('--name', default='current')
    c.add_argument('--out')
    c.add_argument('--results')
    c.add_argument('--diag')
    c.add_argument('--release')
    c.add_argument('--hook-map')
    c.add_argument('--note', help='source state of the capture (e.g. the commit the DLLs were built from)')
    c.add_argument('--mixed', action='store_true', help='accept results from several source states (state the reason in --note)')
    k = sub.add_parser('compare')
    k.add_argument('base')
    k.add_argument('new')
    k.add_argument('--allow')
    k.add_argument('--same-code', action='store_true')
    sub.add_parser('selftest')
    sub.add_parser('selftest-strings')
    a = ap.parse_args(argv)
    return {'capture': capture, 'compare': compare, 'selftest': selftest, 'selftest-strings': selftest_strings}[a.cmd](a)


if __name__ == '__main__':
    sys.exit(main())
