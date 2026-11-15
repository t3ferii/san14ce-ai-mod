"""Offline sampled CPU accounting in an explicit wall-clock window (U35).

Busy one-second bins are not individual date stalls. Inclusive frames overlap;
the first non-system frame can still contain native work without its own stack.
No game access. Times in --begin/--end use an explicit UTC offset.
"""
import argparse
import bisect
from collections import Counter, defaultdict
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import re


def summarize(csv_path, pid, tids, begin, end, boundaries, maps=None):
    starts, ends, labels = zip(*boundaries)
    modules = {}
    symbols = {}
    if maps:
        for name in ('SAN14_Convenience', 'SAN14_CombatAI', 'SAN14_Layout'):
            rows = []
            for line in (maps / (name + '.map')).read_text(encoding='latin-1').splitlines():
                m = re.match(r'\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{16})\s', line)
                if m:
                    rows.append((int(m[2], 16) - 0x180000000, m[1]))
            rows.sort()
            symbols[name.lower() + '.dll'] = tuple(zip(*rows))
    tag = '(' + str(pid) + ')'
    per = defaultdict(lambda: defaultdict(Counter))
    current = None
    frames = []
    trace_start = None
    lo = hi = None
    lost = None

    def resolve(address):
        for name, (base, limit) in modules.items():
            if base <= address < limit:
                rva = address - base
                if name == 'san14ce_ko.exe':
                    i = bisect.bisect_right(starts, rva) - 1
                    return 'EXE:' + (labels[i] if i >= 0 and rva <= ends[i] else hex(rva))
                if name in symbols:
                    addresses, names = symbols[name]
                    i = bisect.bisect_right(addresses, rva) - 1
                    return name + ':' + (names[i] if i >= 0 else hex(rva))
                return name + ':' + hex(rva)
        return None

    def flush():
        if current is None:
            return
        at, tid, count = current
        if not lo <= at < hi:
            return
        row = per[tid][at // 1_000_000]
        row['samples_ms'] += count
        names = [n for a in frames if (n := resolve(a)) is not None]
        for name in set(names):
            row['inc ' + name] += count
        row['leaf ' + (names[0] if names else 'SYS')] += count

    with csv_path.open(encoding='utf-8', errors='replace') as source:
        for line in source:
            line = line.lstrip()
            if 'Trace Start:' in line and trace_start is None:
                ticks = int(re.search(r'Trace Start: (\d+)', line)[1])
                trace_start = datetime(1601, 1, 1, tzinfo=timezone.utc) + timedelta(microseconds=ticks // 10)
                lo = int((begin - trace_start).total_seconds() * 1_000_000)
                hi = int((end - trace_start).total_seconds() * 1_000_000)
                m = re.search(r'Events Lost: (\d+), Buffers lost: (\d+)', line)
                lost = [int(x) for x in m.groups()] if m else None
            elif line.startswith(('I-Start', 'I-DCStart')) and tag in line:
                fields = line.split(',')
                for name in ('san14ce_ko.exe', 'san14_convenience.dll', 'san14_combatai.dll', 'san14_layout.dll'):
                    if ('\\' + name) in line.lower() and name not in modules:
                        modules[name] = (int(fields[3], 16), int(fields[4], 16))
            elif line.startswith('SampledProfile'):
                flush()
                current = None
                frames = []
                fields = line.split(',')
                if tag in fields[2] and fields[3].strip() in tids:
                    current = (int(fields[1]), fields[3].strip(), int(fields[8]))
            elif current and line.startswith('Stack'):
                fields = line.split(',')
                if int(fields[1]) == current[0] and fields[2].strip() == current[1]:
                    frames.append(int(fields[4], 16))
    flush()
    if trace_start is None or 'san14ce_ko.exe' not in modules:
        raise ValueError('Trace header or selected process EXE image missing')
    result = {'pid': pid, 'begin': begin.isoformat(), 'end': end.isoformat(),
              'trace_start': trace_start.isoformat(), 'lost_events_buffers': lost,
              'meaning': 'sampled CPU ms; busy bins are not date counts', 'threads': {}}
    for tid in tids:
        seconds = per[tid]
        total = Counter()
        busy = Counter()
        for sec, row in seconds.items():
            total.update(row)
            if row['samples_ms'] >= 300:
                busy.update(row)
        result['threads'][tid] = {'all': dict(total.most_common()), 'busy': dict(busy.most_common()),
                                  'busy_bins': sum(r['samples_ms'] >= 300 for r in seconds.values()),
                                  'per_second_ms': {s: r['samples_ms'] for s, r in sorted(seconds.items())}}
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('csv', type=Path)
    ap.add_argument('pid', type=int)
    ap.add_argument('--tid', nargs='+', required=True)
    ap.add_argument('--begin', required=True, type=datetime.fromisoformat)
    ap.add_argument('--end', required=True, type=datetime.fromisoformat)
    ap.add_argument('--out', required=True, type=Path)
    ap.add_argument('--maps', type=Path)
    args = ap.parse_args()
    if args.begin.tzinfo is None or args.end.tzinfo is None or args.begin >= args.end:
        ap.error('begin/end must be ordered timestamps with UTC offsets')
    root = Path(__file__).resolve().parents[1]
    ft = root.parent / 'AI_TacticsMod/research/ghidra_full/1a4a447d14e68c40/functions.tsv'
    bounds = []
    for line in ft.read_text(encoding='utf-8').splitlines()[1:]:
        row = line.split('\t')
        bounds.append((int(row[0], 16), int(row[1], 16), row[0].lstrip('0')))
    result = summarize(args.csv, args.pid, set(args.tid), args.begin, args.end, sorted(bounds), args.maps)
    args.out.write_text(json.dumps(result, indent=1), encoding='utf-8')
    for tid, data in result['threads'].items():
        print(tid, 'sampled_ms', data['all'].get('samples_ms', 0), 'busy_bins', data['busy_bins'])
        for key in ('inc EXE:86E920', 'inc EXE:870470', 'inc EXE:8ACF20'):
            print(' ', key, data['all'].get(key, 0), 'busy', data['busy'].get(key, 0))


if __name__ == '__main__':
    main()
