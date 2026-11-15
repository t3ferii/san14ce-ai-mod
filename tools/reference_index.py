"""Static reference inventory/search. Never imports or executes reference code."""
from pathlib import Path
import argparse, ast, hashlib, json, re, struct, zipfile

PROJECT = Path(__file__).resolve().parents[1]
GAME = PROJECT.parent
ROOT = GAME / 'reference_mod'
OUT = PROJECT / 'development/reference_inventory.json'
TEXT = {'.py', '.c', '.cpp', '.h', '.hpp', '.inl', '.md', '.txt', '.json', '.ini', '.ps1', '.cmd', '.def', '.sh'}
SKIP = {'_internal', '__pycache__', '.git'}


def digest(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def decode(p):
    raw = p.read_bytes()
    for enc in ('utf-8-sig', 'utf-16', 'cp949'):
        try:
            return raw.decode(enc)
        except UnicodeError:
            pass
    return raw.decode('utf-8', errors='replace')


def pe_metadata(p):
    # Metadata only, no loading DLLs. Machine/timestamp are not compatibility proof.
    try:
        with p.open('rb') as f:
            dos = f.read(64)
            if dos[:2] != b'MZ': return None
            f.seek(struct.unpack_from('<I', dos, 60)[0]); nt = f.read(88)
        if nt[:4] != b'PE\0\0': return None
        return {'machine': hex(struct.unpack_from('<H', nt, 4)[0]),
                'timestamp': hex(struct.unpack_from('<I', nt, 8)[0]),
                'optional_magic': hex(struct.unpack_from('<H', nt, 24)[0]),
                'size_of_image': hex(struct.unpack_from('<I', nt, 80)[0])}
    except (OSError, struct.error):
        return None


def symbols(p, content):
    out = []
    if p.suffix == '.py':
        try: tree = ast.parse(content)
        except (SyntaxError, ValueError): return out
        for node in ast.walk(tree):
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
                out.append({'name': node.name, 'line': node.lineno, 'kind': type(node).__name__})
        for node in tree.body:
            if isinstance(node, ast.Assign):
                for target in node.targets:
                    if isinstance(target, ast.Name) and isinstance(node.value, ast.Constant) and type(node.value.value) is int:
                        out.append({'name': target.id, 'line': node.lineno, 'kind': 'integer_constant', 'value': hex(node.value.value)})
    elif p.suffix in {'.c', '.h', '.cpp', '.hpp', '.inl'}:
        for i, line in enumerate(content.splitlines(), 1):
            m = re.match(r'\s*#define\s+(\w+)\s+(0x[0-9a-fA-F]+)', line)
            if m: out.append({'name': m[1], 'line': i, 'kind': 'integer_macro', 'value': m[2]})
    return sorted(out, key=lambda s: s['line'])


def priority(p):
    s = p.as_posix()
    if 'Modding_DOCS/' in s: return 0
    if 'Editor_Source/' in s: return 1
    if 'SAN14CE_4Mods_v1_3/01_' in s: return 2
    return 3


def build():
    files, archives, seen, skipped = [], [], {}, 0
    packages = {}
    for p in sorted(ROOT.rglob('*'), key=lambda p: (priority(p), p.as_posix())):
        if not p.is_file(): continue
        package = p.relative_to(ROOT).parts[0]
        packages.setdefault(package, {'files': 0, 'indexed': 0, 'runtime_excluded': 0})['files'] += 1
        if any(part in SKIP for part in p.relative_to(ROOT).parts):
            skipped += 1; packages[package]['runtime_excluded'] += 1; continue
        rel = p.relative_to(GAME).as_posix()
        record = {'path': rel, 'bytes': p.stat().st_size, 'sha256': digest(p)}
        record['canonical'] = seen.setdefault(record['sha256'], rel)
        suffix = p.suffix.lower()
        record['kind'] = 'text' if suffix in TEXT else 'binary' if suffix in {'.exe', '.dll'} else 'archive' if suffix == '.zip' else 'data'
        if suffix in {'.exe', '.dll'}: record['pe'] = pe_metadata(p)
        if record['kind'] == 'text' and record['canonical'] == rel:
            record['symbols'] = symbols(p, decode(p))
        if suffix == '.zip':
            try:
                with zipfile.ZipFile(p) as z:
                    members = [{'name': n.filename, 'bytes': n.file_size, 'crc32': hex(n.CRC)} for n in z.infolist() if not n.is_dir()]
                archives.append({'path': rel, 'members': members})
            except zipfile.BadZipFile: record['archive_error'] = 'BadZipFile'
        files.append(record); packages[package]['indexed'] += 1
    data = {'format': 1, 'scope': 'Static files, ZIP central directories, PE headers, AST declarations; no reference code execution or live game validation.',
            'excluded_runtime_files': skipped, 'packages': packages, 'files': files, 'archives': archives}
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'inventory': str(OUT), 'indexed_files': len(files), 'unique_files': len(seen),
                      'runtime_excluded': skipped, 'zip_archives': len(archives)}, ensure_ascii=False))


def search(args):
    if not OUT.exists(): raise SystemExit('Run --build first.')
    data = json.loads(OUT.read_text(encoding='utf-8'))
    records = [r for r in data['files'] if r['kind'] == 'text' and r['path'] == r['canonical']]
    records.sort(key=lambda r: (priority(Path(r['path'])), r['path']))
    terms = [term.casefold() for term in args.query.split('|') if term]
    if not terms: raise SystemExit('Specify a literal term; use | to search alternatives.')
    found = 0
    for record in records:
        path = record['path']
        if args.scope and args.scope.casefold() not in path.casefold(): continue
        if not args.all and any(x in path.casefold() for x in ('/tests', '/audit/', '/history/', '/v26_reference/')): continue
        p = GAME / path
        if not p.is_file(): continue
        hits = []
        if args.symbols:
            for symbol in record.get('symbols', []):
                if any(term in (symbol['name'] + ' ' + symbol.get('value', '')).casefold() for term in terms):
                    hits.append(f"  {symbol['line']}: {symbol['kind']} {symbol['name']} {symbol.get('value', '')}")
                    if len(hits) >= args.per_file: break
        else:
            for line, value in enumerate(decode(p).splitlines(), 1):
                if any(term in value.casefold() for term in terms):
                    hits.append(f'  {line}: {value.strip()[:args.width]}')
                    if len(hits) >= args.per_file: break
        if hits:
            print(path + (' [changed since inventory]' if digest(p) != record['sha256'] else ''))
            print('\n'.join(hits)); found += 1
            if found >= args.limit: break
    print(f'Matched files shown: {found}; limit={args.limit}; duplicates/runtime excluded.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', action='store_true')
    parser.add_argument('--query')
    parser.add_argument('--scope', default='', help='Path substring, e.g. Editor_Source/ai')
    parser.add_argument('--all', action='store_true', help='Also search historical docs/test evidence')
    parser.add_argument('--symbols', action='store_true', help='Search static declarations; integer constants are not automatically RVAs')
    parser.add_argument('--limit', type=int, default=6)
    parser.add_argument('--per-file', type=int, default=3)
    parser.add_argument('--width', type=int, default=180)
    args = parser.parse_args()
    if not (1 <= args.limit <= 40 and 1 <= args.per_file <= 12 and 40 <= args.width <= 400):
        parser.error('Use bounded output: limit 1..40, per-file 1..12, width 40..400.')
    if args.build: build()
    elif args.query: search(args)
    else: parser.error('Use --build or --query.')


if __name__ == '__main__': main()
