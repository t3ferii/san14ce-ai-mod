"""Runtime smoke check from the diagnostic logs: did one short game run behave like the baseline run?

Reads only the game-root logs written by the diagnostic DLLs (SAN14_AITactics[.old].log = convenience/loader side,
SAN14_CombatAI[.old].log = combat side). Never touches the game, its memory or its files.

    python -X utf8 tools/runtime_smoke.py sessions                  # list logged runs (start time, version, days)
    python -X utf8 tools/runtime_smoke.py snapshot [--session -1]   # save that run as the baseline
    python -X utf8 tools/runtime_smoke.py compare [--days 30]       # latest run vs the baseline

A run = the lines from a `day=-1` block (start-up, before any game day) to the next start-up block. The comparison:
  1. start-up results: every `<feature>_installed`-style event of the baseline must appear again (missing = problem),
     first-number (army) changes are listed for review (timing/profiling counters are skipped);
  2. failure signals: fail/mismatch/disabled/... events that are new or clearly more frequent;
  3. activity: event names the baseline logged at least MIN_ACTIVITY times in its first N game days must appear in
     the new run's first N days.
Exit code 0 = no problem found, 1 = items to check, 2 = usage / missing data. docs/RUNTIME_SMOKE.md says what to play.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import re
import sys
from collections import Counter
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
GAME = PROJECT.parent
LOGS = {'conv': ('SAN14_AITactics.old.log', 'SAN14_AITactics.log'), 'combat': ('SAN14_CombatAI.old.log', 'SAN14_CombatAI.log')}
BASELINE = PROJECT / 'data/diagnostics/smoke_baseline.json'
LINE = re.compile(r'^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d) day=(-?\d+) thread=\d+ (\S+) army=(\S+) value=(\S+)')
START_EVENTS = re.compile(r'(_installed|_registered|_ready|_published|_link|_on|_hook|_hooks_installed|_enabled|_source)$')
EXC = re.compile(r'^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d) day=(-?\d+) thread=\d+ (native_exception_first_chance)')
# timing / profiling counters and per-run identifiers (thread ids, costs) change every run: not compared
PROFILE = re.compile(r'^(terrain|layout_p4)_(wall|cpu|self|calls|cycles|range|peak|peak_thread|valid)_|_(cost|timing|thread|time_thread|render_thread)$|^layout_p4_')
FAIL = re.compile(r'(fail|mismatch|refus|disabled|split_disable|feature_off|unavailable|engine_off|abort|invalid|error|crash|timeout|rollback)')
MIN_ACTIVITY = 3
PAIR_SECONDS = 180
# Known, intended differences: event -> why (keep short; remove once a new baseline is taken after the change).
EXPECTED = {
    'tactic_priority_installed': 'local194: the second value (diagnostic only) was dropped',
    'season_tiles_engine_off': 'local194: logged only when 4계절 tiles are on but the season rules cannot run',
}


def parse(side):
    """Yield (time, day, event, army, value) for every log line of one side, old file first."""
    for name in LOGS[side]:
        p = GAME / name
        if not p.exists():
            continue
        with p.open(encoding='utf-8', errors='replace') as f:
            for line in f:
                m = LINE.match(line)
                if m:
                    yield m.group(1), int(m.group(2)), m.group(3), m.group(4), m.group(5)
                    continue
                m = EXC.match(line)
                if m:   # crash diagnostic lines have their own format (src/native_crash_diagnostic.inl)
                    yield m.group(1), int(m.group(2)), m.group(3), '', ''


def sessions(side):
    out, cur, prev = [], None, None
    for rec in parse(side):
        if rec[1] == -1 and prev != -1:
            cur = {'start': rec[0], 'lines': []}
            out.append(cur)
        if cur is not None:
            cur['lines'].append(rec)
        prev = rec[1]
    return out


def versions():
    try:
        m = json.loads((PROJECT / 'build/input_package/manifest.json').read_text(encoding='utf-8-sig'))
    except (OSError, ValueError):
        return []
    rows = []
    for c in m.get('local_changes', []):
        try:
            when = dt.datetime.fromisoformat(c['date']).astimezone().replace(tzinfo=None)
        except (KeyError, ValueError):
            continue
        rows.append((when, c.get('version', '?')))
    return sorted(rows)


def version_at(start, rows):
    t = dt.datetime.strptime(start, '%Y-%m-%d %H:%M:%S')
    best = '?'
    for when, v in rows:
        if when <= t:
            best = v
    return best


def signature(conv, combat, days):
    sig = {'start': conv['start'], 'combat_start': combat['start'] if combat else None, 'startup': {}, 'activity': {},
           'failures': {}, 'days': [None, None]}
    first_day = None
    for side, s in (('conv', conv), ('combat', combat)):
        if not s:
            continue
        for t, day, ev, army, value in s['lines']:
            key = side + ':' + ev
            if day == -1:
                row = sig['startup'].setdefault(key, [0, army, value])
                row[0] += 1
            else:
                if first_day is None or day < first_day:
                    first_day = day
                lo, hi = sig['days']
                sig['days'] = [day if lo is None else min(lo, day), day if hi is None else max(hi, day)]
    for side, s in (('conv', conv), ('combat', combat)):
        if not s:
            continue
        for t, day, ev, army, value in s['lines']:
            in_window = day == -1 or (first_day is not None and 0 <= day - first_day < days)
            if in_window and (FAIL.search(ev) or ev.startswith('native_exception')) and ev != 'exception_diagnostic_ready':
                sig['failures'][side + ':' + ev] = sig['failures'].get(side + ':' + ev, 0) + 1
    if first_day is not None:
        for side, s in (('conv', conv), ('combat', combat)):
            if not s:
                continue
            c = Counter(side + ':' + ev for t, day, ev, a, v in s['lines'] if day >= 0 and day - first_day < days)
            sig['activity'].update(c)
    sig['window_days'] = days
    return sig


def pick(index, days):
    conv_all, combat_all = sessions('conv'), sessions('combat')
    if not conv_all:
        sys.exit('no convenience-log run found in ' + str(GAME))
    conv = conv_all[index]
    t0 = dt.datetime.strptime(conv['start'], '%Y-%m-%d %H:%M:%S')
    combat = None
    for s in combat_all:
        if abs((dt.datetime.strptime(s['start'], '%Y-%m-%d %H:%M:%S') - t0).total_seconds()) <= PAIR_SECONDS:
            combat = s
    sig = signature(conv, combat, days)
    sig['version'] = version_at(conv['start'], versions())
    return sig


def cmd_sessions(a):
    rows = versions()
    combat_starts = [s['start'] for s in sessions('combat')]
    for i, s in enumerate(sessions('conv')):
        d = [r[1] for r in s['lines'] if r[1] >= 0]
        span = f'day {min(d)}..{max(d)} ({max(d) - min(d)}일)' if d else '게임 날짜 없음(타이틀·로드만)'
        paired = any(abs((dt.datetime.strptime(c, '%Y-%m-%d %H:%M:%S') - dt.datetime.strptime(s['start'], '%Y-%m-%d %H:%M:%S')).total_seconds()) <= PAIR_SECONDS for c in combat_starts)
        print(f"[{i - len(sessions('conv'))}] {s['start']}  {version_at(s['start'], rows):<24} {span}  lines={len(s['lines'])}  combat={'yes' if paired else 'no'}")


def cmd_snapshot(a):
    sig = pick(a.session, a.days)
    out = Path(a.out) if a.out else BASELINE
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(sig, ensure_ascii=False, indent=1) + '\n', encoding='utf-8')
    print(f"baseline saved: {out}  run {sig['start']} ({sig['version']}), start-up events {len(sig['startup'])}, days {sig['days']}")


def cmd_compare(a):
    path = Path(a.baseline) if a.baseline else BASELINE
    if not path.exists():
        print('baseline missing: run `snapshot` on a good run first (' + str(path) + ')'); return 2
    base = json.loads(path.read_text(encoding='utf-8'))
    new = pick(a.session, a.days)
    problems, review = [], []
    print(f"기준: {base['start']} ({base.get('version', '?')}), 날짜 {base['days']}")
    print(f"이번: {new['start']} ({new['version']}), 날짜 {new['days']}")
    if new['start'] == base['start']:
        print('주의: 기준과 같은 실행이다. 게임을 한 번 실행한 뒤 다시 비교한다.')

    # 1. start-up results
    for key, (n, army, value) in sorted(base['startup'].items()):
        ev = key.split(':', 1)[1]
        if not START_EVENTS.search(ev) or PROFILE.search(ev):
            continue
        got = new['startup'].get(key)
        if got is None:
            (review if ev in EXPECTED else problems).append(f'설치 기록 없음: {key} (기준 army={army} value={value})' + (f' — 예상: {EXPECTED[ev]}' if ev in EXPECTED else ''))
        elif got[1] != army or (a.values and got[2] != value):
            review.append(f'설치 값 다름: {key} {army},{value} → {got[1]},{got[2]}' + (f' — 예상: {EXPECTED[ev]}' if ev in EXPECTED else ''))
    for key, (n, army, value) in sorted(new['startup'].items()):
        if key not in base['startup'] and START_EVENTS.search(key.split(':', 1)[1]) and not PROFILE.search(key.split(':', 1)[1]):
            review.append(f'새 설치 기록: {key} army={army} value={value}')

    # 2. failure signals
    for key, n in sorted(new['failures'].items()):
        b = base['failures'].get(key, 0)
        ev = key.split(':', 1)[1]
        if b == 0 or n > 2 * b + 5:
            (review if ev in EXPECTED else problems).append(f'실패 신호: {key} {b}회 → {n}회' + (f' — 예상: {EXPECTED[ev]}' if ev in EXPECTED else ''))

    # 3. activity in the first N days (only when both runs cover the window)
    window = min(a.days, *(r['days'][1] - r['days'][0] + 1 for r in (base, new) if r['days'][0] is not None)) if new['days'][0] is not None and base['days'][0] is not None else 0
    if window <= 0:
        problems.append('이번 실행에 게임 날짜 진행 기록이 없다(타이틀·로드만 했거나 로그 꺼짐). 하루 이상 진행해야 활동을 비교한다.')
    else:
        if window < a.days:
            print(f'활동 비교 창: 두 실행이 공통으로 진행한 {window}일')
        quiet = [k for k, n in base['activity'].items() if n >= MIN_ACTIVITY and k not in new['activity'] and not FAIL.search(k) and not PROFILE.search(k.split(':', 1)[1])]
        for k in sorted(quiet):
            review.append(f"활동 없음: {k} (기준 첫 {base.get('window_days', a.days)}일 {base['activity'][k]}회)")

    for title, rows in (('문제', problems), ('확인할 차이', review)):
        print(f'\n## {title} {len(rows)}건')
        for r in rows[:a.limit]:
            print(' - ' + r)
        if len(rows) > a.limit:
            print(f' ... {len(rows) - a.limit}건 더 (--limit)')
    print('\n결과:', '문제 없음' if not problems else f'문제 {len(problems)}건', f'/ 확인할 차이 {len(review)}건')
    return 1 if problems else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    sub.add_parser('sessions')
    for name in ('snapshot', 'compare'):
        p = sub.add_parser(name)
        p.add_argument('--session', type=int, default=-1, help='run index from `sessions` (default: latest)')
        p.add_argument('--days', type=int, default=30, help='game days of activity to compare (default 30)')
        p.add_argument('--out' if name == 'snapshot' else '--baseline', default='')
        if name == 'compare':
            p.add_argument('--limit', type=int, default=60)
            p.add_argument('--values', action='store_true', help='also list start-up value changes (second number; often thread ids or sizes)')
    a = ap.parse_args(argv)
    return {'sessions': cmd_sessions, 'snapshot': cmd_snapshot, 'compare': cmd_compare}[a.cmd](a) or 0


if __name__ == '__main__':
    sys.exit(main())
