#!/usr/bin/env python
"""Exact compile cache for tools/build_split.ps1 (2026-11-16 user: 검사·설치를 빨리).

    python -B -X utf8 tools/cl_cache.py fresh JOBS.json   -> one JSON line: [true|false, ...] per job
    python -B -X utf8 tools/cl_cache.py store JOBS.json   -> writes the stamp of every job whose 'ok' is true

A job is {"stamp": file, "key": "<compiler + every argument + INCLUDE/LIB>", "deps": the cl /sourceDependencies
directory (one JSON per source), "sources": [compiled sources], "outputs": [files], "ok": bool}. A job is fresh when the stamp has the same key, every output still has its recorded
SHA-256, and every file the last compile read (the source and its whole include closure, system headers included, from
/sourceDependencies) is unchanged: same size and mtime, or else the same SHA-256. Under those conditions the compiler
input is identical, so the recorded output is what a new compile would produce; anything else is stale and rebuilt.
A stamp whose dependency list does not contain its own source is refused (an empty list would make every run fresh).
Since the 2026-11-16 refactor every source is its own job; a link job lists its object files as "inputs" instead of "deps"
and "sources" (build_split.ps1 also treats a link as stale whenever one of its objects is rebuilt in the same run).
Standard library only.
"""
import hashlib, json, os, sys


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def stat(p):
    s = os.stat(p)
    return s.st_size, int(s.st_mtime_ns)


def fresh(job):
    try:
        with open(job['stamp'], encoding='utf-8') as f:
            st = json.load(f)
    except (OSError, ValueError):
        return False
    if st.get('key') != job['key'] or not st.get('deps') or not st.get('outputs'):
        return False
    for p, h in st['outputs'].items():
        if not os.path.exists(p) or sha(p) != h:
            return False
    for d in st['deps']:
        p = d['path']
        if not os.path.exists(p):
            return False
        size, mtime = stat(p)
        if size == d['size'] and mtime == d['mtime']:
            continue
        if size != d['size'] or sha(p) != d['sha256']:
            return False
    return True


def store(job):
    """job['deps']: the /sourceDependencies directory (one JSON per source); job['sources']: the compiled sources.
    A link item has job['inputs'] (its object files) instead: those are the recorded dependencies."""
    if 'inputs' in job:
        files = [os.path.normpath(p) for p in job['inputs']]
        if not files or not all(os.path.exists(p) for p in files):
            raise SystemExit('cl_cache: link inputs of %s missing; not cached' % job['stamp'])
        return write_stamp(job, files)
    files, seen, recorded = [], set(), set()
    for n in sorted(os.listdir(job['deps'])):
        if not n.lower().endswith('.json'):
            continue
        with open(os.path.join(job['deps'], n), encoding='utf-8-sig') as f:
            data = json.load(f).get('Data') or {}
        src = data.get('Source')
        if src:
            recorded.add(os.path.normcase(os.path.normpath(src)))
        for p in ([src] if src else []) + list(data.get('Includes') or []):
            p = os.path.normpath(p)
            if os.path.normcase(p) not in seen:
                seen.add(os.path.normcase(p))
                files.append(p)
    missing = [s for s in job['sources'] if os.path.normcase(os.path.normpath(s)) not in recorded]
    if missing or len(files) <= len(job['sources']):
        raise SystemExit('cl_cache: dependency list of %s lacks %s; not cached' % (job['stamp'], missing or 'includes'))
    write_stamp(job, files)


def write_stamp(job, files):
    deps = []
    for p in files:
        size, mtime = stat(p)
        deps.append({'path': p, 'size': size, 'mtime': mtime, 'sha256': sha(p)})
    st = {'key': job['key'], 'deps': deps, 'outputs': {p: sha(p) for p in job['outputs'] if os.path.exists(p)}}
    if len(st['outputs']) != len(job['outputs']):
        raise SystemExit('cl_cache: output missing for %s' % job['stamp'])
    tmp = job['stamp'] + '.tmp'
    with open(tmp, 'w', encoding='utf-8') as f:
        json.dump(st, f)
    os.replace(tmp, job['stamp'])


def files_key(job):
    """A check run whose result depends only on these files (built binaries + the test script): SHA-256 of each."""
    return {os.path.normcase(os.path.normpath(p)): (sha(p) if os.path.exists(p) else 'missing') for p in job['files']}


def main(argv):
    if len(argv) != 3 or argv[1] not in ('fresh', 'store', 'files-fresh', 'files-store'):
        print(__doc__)
        return 2
    with open(argv[2], encoding='utf-8-sig') as f:
        jobs = json.load(f)
    if isinstance(jobs, dict):
        jobs = [jobs]
    if argv[1] == 'files-fresh':   # run gates (tools/build_split.ps1 release loader fixtures): passed before with identical inputs
        out = []
        for j in jobs:
            try:
                with open(j['stamp'], encoding='utf-8') as f:
                    out.append(json.load(f) == files_key(j))
            except (OSError, ValueError):
                out.append(False)
        print(json.dumps(out))
        return 0
    if argv[1] == 'files-store':
        for j in jobs:
            if j.get('ok'):
                with open(j['stamp'], 'w', encoding='utf-8') as f:
                    json.dump(files_key(j), f)
        return 0
    if argv[1] == 'fresh':
        print(json.dumps([fresh(j) for j in jobs]))
        return 0
    for j in jobs:
        if j.get('ok'):
            store(j)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
