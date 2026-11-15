"""Mission ledger from an ai_trace CSV: one record per troops (군세) mission with every step and writer.

Read-only. A mission starts when a troops object gets a non-zero mode (+14) and ends when the mode
returns to 0 or the troops object disappears. Member armies are those whose troops id (+58) equals the
mission's troops id. Each army order change is a step; its writer is the original node, or that node plus
the mod decision labels (`next` column of node rows, diagnostic SAN14_CombatAI.dll only).

  python -X utf8 tools/mission_ledger.py <trace.csv> --out <dir>
  python -X utf8 tools/mission_ledger.py <trace.csv> --out <dir> --mission <troops_id> <start_day>
"""
import argparse, collections as C, csv, json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from analyze_ai_trace import ACTION, KIND  # noqa: E402
from mission_patterns import mission, dist  # noqa: E402

ORDER = ('action', 'target_kind', 'target_id', 'target_hex')
# Labels meaning the original node ran, or plain observation; anything else is a mod decision.
NATIVE_LABELS = {'army_node:native', 'cut_node:native_skip', 'retreat_node:native', 'city_node:native_then_members',
                 'dispatch_native_command', 'campaign_remembered', 'native_retreat_allowed', 'troops_mode_change'}
RELAYS = ('relay1', 'relay2', 'relay3', 'relay4', 'relay5')


def step_name(o):
    a, k = o[0], o[1]
    return f'{ACTION.get(a, f"코드{a}")}/{KIND.get(k, f"종류{k}")}'


def writer(r):
    w = r['node'] if r['cause'] == 'node' else 'between'
    nxt = r.get('next') or ''
    if r['cause'] == 'node' and nxt and not nxt.startswith('C'):
        mod = [x for x in nxt.split(';') if x and x not in NATIVE_LABELS]
        if mod: return w + ' [모드 ' + ';'.join(dict.fromkeys(mod)) + ']'
    return w


class Mission:
    def __init__(self, tid, day, mode, goal, w):
        self.tid, self.start, self.end = tid, day, None
        self.mode, self.goal, self.origin = mode, goal, w
        self.modes = [(day, w, mode, goal)]
        self.steps = []      # (day, army, writer, before, after)
        self.relays = []     # (day, army, writer, position, relays tuple, target_hex)
        self.armies = set()

    def kind(self): return mission(self.mode)


def load(path):
    troops, missions, armies = {}, [], {}
    with open(path, encoding='utf-8', errors='replace') as h:
        for r in csv.DictReader(h):
            if r['conc'] != '0': continue
            try: i, day = int(r['id']), int(r['day'])
            except ValueError: continue
            k, f = r['kind'], r['field']
            if k == 'troops':
                t = troops.setdefault(i, {'mode': 0, 'goal': 0, 'm': None})
                if f == 'gone' and t['m']: t['m'].end = day; t['m'] = None; continue
                if f not in ('mode', 'goal'): continue
                t[f] = int(r['new'] or 0)
                m = t['m']
                if t['mode'] == 0:
                    if m: m.end = day; t['m'] = None
                elif m is None:
                    m = Mission(i, day, t['mode'], t['goal'], writer(r)); t['m'] = m; missions.append(m)
                else:
                    if (m.modes[-1][2], m.modes[-1][3]) != (t['mode'], t['goal']):
                        m.modes.append((day, writer(r), t['mode'], t['goal']))
                    m.mode, m.goal = t['mode'], t['goal']
                continue
            if k != 'army': continue
            s = armies.setdefault(i, {'o': [0, 0, 0, 48400], 'relay': [48400] * 5})
            if f in ('reuse', 'spawn'):
                armies[i] = {'o': [0, 0, 0, 48400], 'relay': [48400] * 5}; continue
            g = f[5:] if f.startswith('init.') else f
            try: new = int(r['new'] or 0); tid = int(r['troops'] or 0); pos = int(r['hex'] or 48400)
            except ValueError: continue
            m = troops.get(tid, {}).get('m')
            if g in ORDER:
                before = tuple(s['o']); s['o'][ORDER.index(g)] = new
                if m and not f.startswith('init.') and g in ('action', 'target_kind', 'target_id'):
                    m.armies.add(i)
                    if m.steps and m.steps[-1][0] == day and m.steps[-1][1] == i and m.steps[-1][2] == writer(r):
                        d, a, w, b0, _ = m.steps[-1]; m.steps[-1] = (d, a, w, b0, tuple(s['o']))
                    else:
                        m.steps.append((day, i, writer(r), before, tuple(s['o'])))
            elif g in RELAYS:
                s['relay'][RELAYS.index(g)] = new
                if m and not f.startswith('init.'):
                    m.armies.add(i)
                    m.relays.append((day, i, writer(r), pos, tuple(s['relay']), s['o'][3]))
    return missions


def classify(m, before, after):
    a, k, t = after[0], after[1], after[2]
    goal = m.goal
    if a in (11, 12): return '퇴각'
    if a == 13: return '대기'
    if a in (8, 9): return '건설'
    if k in (5, 6): return '임무 목표 공격' if t == goal else '다른 성/관문'
    if k == 27: return '적 부대'
    if k == 9: return '임무 목표 칸' if t == goal else '경유/점령 지역'
    if k == 11: return '시설'
    return step_name(after)


def summarize(missions):
    by = C.defaultdict(list)
    for m in missions: by[m.kind()].append(m)
    out = {}
    for kind, ms in sorted(by.items(), key=lambda x: -len(x[1])):
        trans = C.Counter(); writers = C.Counter(); dev = C.Counter(); mode_w = C.Counter()
        first = C.Counter(); last = C.Counter(); dur = []
        for m in ms:
            if m.end is not None: dur.append(m.end - m.start)
            for d, w, mo, go in m.modes[1:]: mode_w[(w, mission(mo))] += 1
            prev = C.defaultdict(lambda: '출발')
            for d, a, w, b, af in m.steps:
                c = classify(m, b, af)
                trans[(prev[a], c)] += 1; writers[(c, w)] += 1; prev[a] = c
                if c in ('다른 성/관문', '대기', '퇴각') or '[모드' in w: dev[(c, w)] += 1
            if m.steps: first[classify(m, *m.steps[0][3:])] += 1; last[classify(m, *m.steps[-1][3:])] += 1
        dur.sort()
        out[kind] = {
            'missions': len(ms), 'duration_median': dur[len(dur) // 2] if dur else None,
            'origin': C.Counter(m.origin for m in ms).most_common(6),
            'first_step': first.most_common(8), 'last_step': last.most_common(8),
            'step_transitions': [[f'{a} -> {b}', n] for (a, b), n in trans.most_common(25)],
            'step_writers': [[c, w, n] for (c, w), n in writers.most_common(30)],
            'mission_changed_by': [[w, mo, n] for (w, mo), n in mode_w.most_common(15)],
            'deviation_writers': [[c, w, n] for (c, w), n in dev.most_common(20)],
        }
    return out


def story(m):
    L = [f'임무 군세{m.tid} {m.kind()} 목표{m.goal} 시작{m.start} 끝{m.end} 출발 {m.origin} 부대 {sorted(m.armies)}']
    ev = [(d, 0, f'임무 {mission(mo)} 목표{go} ({w})') for d, w, mo, go in m.modes[1:]]
    ev += [(d, 1, f'부대{a} {classify(m, b, af)}: {step_name(b)}#{b[2]} -> {step_name(af)}#{af[2]} 칸{af[3]} ({w})')
           for d, a, w, b, af in m.steps]
    for d, a, w, pos, rel, final in m.relays:
        pts = [x for x in rel if x < 48400]
        hops = [dist(pos, pts[0]) if pts else None] + [dist(pts[j], pts[j + 1]) for j in range(len(pts) - 1)]
        ev.append((d, 2, f'  부대{a} 중계 {pts} 최종칸{final} 위치{pos} 간격{hops} ({w})'))
    return L + [f'{d} {t}' for d, _, t in sorted(ev)]


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('trace'); ap.add_argument('--out', required=True)
    ap.add_argument('--mission', nargs=2, type=int, metavar=('TROOPS', 'START_DAY'))
    a = ap.parse_args()
    missions = load(a.trace)
    if a.mission:
        for m in missions:
            if m.tid == a.mission[0] and m.start == a.mission[1]: print('\n'.join(story(m)))
        return
    rep = summarize(missions)
    os.makedirs(a.out, exist_ok=True)
    json.dump(rep, open(os.path.join(a.out, 'mission_ledger.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    for kind, r in rep.items():
        print(f"\n## {kind}: {r['missions']}건, 기간 중앙값 {r['duration_median']}일, 출발 {r['origin'][:3]}")
        print('  첫 단계', r['first_step'][:5]); print('  끝 단계', r['last_step'][:5])
        for t in r['step_transitions'][:8]: print('   ', t)
        print('  임무 변경', r['mission_changed_by'][:5])


if __name__ == '__main__':
    main()
