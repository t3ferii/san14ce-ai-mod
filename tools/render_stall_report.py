"""Summarize data/performance/render_stall_<pid>.csv (render-stall sampler, src/native_face_timing.inl, local203).

Each row is one thread read during one Present gap: gap id, final gap ms, sample number, ms into the gap, thread, roles
(1 render, 2 day frame 44DEE0, 4 window, 8 busy >= 2 ms CPU since the previous reading), CPU us, RIP and return-address
candidates innermost first (module+rva, exe = game). Candidates come from a stack scan, so a frame may be stale; count a
frame only when it repeats across gaps.

Usage: python tools/render_stall_report.py [csv | pid] [--min 100] [--top 12]
Without a path the newest render_stall_*.csv under data/performance (or AI_TacticsMod/data/performance) is used.
System DLL offsets are named from the export table of C:\\Windows\\System32\\<module>.dll (nearest export at or below).
"""
import argparse
import collections
import glob
import os
import struct
import sys

ROLES = ((1, 'render'), (2, 'day-frame'), (4, 'window'), (8, 'busy'))


def newest(pid):
    here = os.path.dirname(os.path.abspath(__file__))
    roots = [os.path.join(here, '..', 'data', 'performance'), os.path.join(here, '..', '..', 'AI_TacticsMod', 'data', 'performance')]
    pattern = 'render_stall_%s.csv' % (pid or '*')
    files = [f for r in roots for f in glob.glob(os.path.join(r, pattern))]
    return max(files, key=os.path.getmtime) if files else None


_exports = {}


def exports(module):
    """Sorted (rva, name) of a System32 DLL's exports; empty when the file is missing or not PE32+."""
    if module in _exports:
        return _exports[module]
    out = []
    path = os.path.join(os.environ.get('SystemRoot', r'C:\Windows'), 'System32', module + '.dll')
    try:
        data = open(path, 'rb').read()
        pe = struct.unpack_from('<I', data, 0x3C)[0]
        sections = struct.unpack_from('<H', data, pe + 6)[0]
        opt = pe + 24
        if struct.unpack_from('<H', data, opt)[0] == 0x20B:
            exp_rva = struct.unpack_from('<I', data, opt + 112)[0]
            table = opt + struct.unpack_from('<H', data, pe + 20)[0]
            spans = [struct.unpack_from('<IIII', data, table + 40 * i + 8) for i in range(sections)]   # vsize, va, rawsize, raw

            def off(rva):
                for vsize, va, _, raw in spans:
                    if va <= rva < va + max(vsize, 1):
                        return rva - va + raw
                return None
            e = off(exp_rva)
            if e is not None:
                n_funcs, n_names, funcs, names, ords = struct.unpack_from('<IIIII', data, e + 20)
                f0, n0, o0 = off(funcs), off(names), off(ords)
                for i in range(n_names):
                    name_rva = struct.unpack_from('<I', data, n0 + 4 * i)[0]
                    ordinal = struct.unpack_from('<H', data, o0 + 2 * i)[0]
                    func = struct.unpack_from('<I', data, f0 + 4 * ordinal)[0]
                    p = off(name_rva)
                    name = data[p:data.index(b'\0', p)].decode('ascii', 'replace')
                    out.append((func, name))
    except (OSError, struct.error, ValueError):
        out = []
    out.sort()
    _exports[module] = out
    return out


def name(label):
    """exe+RVA stays as is; system module offsets get the nearest export name."""
    if '+' not in label or label.startswith('exe+'):
        return label
    module, rva = label.split('+', 1)
    try:
        value = int(rva, 16)
    except ValueError:
        return label
    best = None
    for func, export in exports(module):
        if func > value:
            break
        best = (func, export)
    return '%s!%s+0x%X' % (module, best[1], value - best[0]) if best and value - best[0] < 0x10000 else label


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('source', nargs='?')
    ap.add_argument('--min', type=int, default=100, help='minimum final gap (ms)')
    ap.add_argument('--max', type=int, default=1000, help='maximum final gap (ms); longer gaps are loads, menus or a hidden window')
    ap.add_argument('--top', type=int, default=12)
    a = ap.parse_args()
    path = a.source if a.source and os.path.exists(a.source) else newest(a.source if a.source and a.source.isdigit() else None)
    if not path:
        sys.exit('no render_stall_*.csv found')
    gaps = collections.defaultdict(list)
    for line in open(path, encoding='utf-8', errors='replace'):
        if line.startswith('#') or not line.strip():
            continue
        f = line.rstrip('\r\n').split(',')
        if len(f) < 9:
            continue
        row = {'tick': int(f[0]), 'gap': int(f[1]), 'total': int(f[2]), 'sample': int(f[3]), 'at': int(f[4]), 'thread': int(f[5]),
               'roles': int(f[6]), 'cpu': int(f[7]), 'rip': f[8], 'frames': f[9:]}
        gaps[row['gap']].append(row)
    kept = {g: rows for g, rows in gaps.items() if a.min <= rows[0]['total'] <= a.max}
    totals = sorted(rows[0]['total'] for rows in kept.values())
    print('%s: %d gaps, %d with final gap %d..%d ms%s' % (path, len(gaps), len(kept), a.min, a.max,
          (' (median %d, max %d ms)' % (totals[len(totals) // 2], totals[-1])) if totals else ''))
    for bit, role in ROLES:
        rows = [r for rs in kept.values() for r in rs if r['roles'] & bit and (bit != 8 or r['roles'] == 8)]
        if not rows:
            continue
        in_gaps = len({r['gap'] for r in rows})
        print('\n== %s: %d readings in %d gaps' % (role, len(rows), in_gaps))
        rip = collections.Counter(name(r['rip']) for r in rows)
        print('  where it is (RIP):')
        for k, n in rip.most_common(a.top):
            print('    %4d  %s' % (n, k))
        first_exe = collections.Counter(next((x for x in r['frames'] if x.startswith('exe+')), '(no exe frame)') for r in rows)
        print('  innermost game frame:')
        for k, n in first_exe.most_common(a.top):
            print('    %4d  %s' % (n, k))
        per_gap = collections.Counter()
        for g in {r['gap'] for r in rows}:
            per_gap.update({x for r in rows if r['gap'] == g for x in r['frames']})
        print('  frames present in the most gaps:')
        for k, n in per_gap.most_common(a.top):
            print('    %4d/%d  %s' % (n, in_gaps, name(k)))
    if kept:
        g = sorted(kept, key=lambda k: kept[k][0]['total'])[len(kept) // 2]
        print('\n== median gap %d (%d ms): every reading' % (g, kept[g][0]['total']))
        for r in sorted(kept[g], key=lambda r: (r['sample'], -r['roles'])):
            roles = '+'.join(n for b, n in ROLES if r['roles'] & b)
            print('  s%d @%dms th=%d %s cpu=%dus  %s' % (r['sample'], r['at'], r['thread'], roles, r['cpu'], name(r['rip'])))
            print('      ' + ' < '.join(name(x) for x in r['frames'][:14]))


if __name__ == '__main__':
    main()
