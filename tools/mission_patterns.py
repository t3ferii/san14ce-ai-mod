"""Mission patterns from an ai_trace CSV: troops (군세) mission mode/goal vs. army orders and relays.

Read-only. A troops object (world+7F8D8, +14 mode, +16 goal) is the native mission; armies (+58 troops id)
carry the current step: action(+37)/target_kind(+38)/target_id(+3A), target_hex(+42), relay1..5(+2C..+34).
Output: summary.json + printed tables (mission x step, writers of mission changes, relay structure).
"""
import argparse, collections as C, csv, json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from analyze_ai_trace import ACTION, KIND  # noqa: E402

MISSION = {0: '임무없음(귀환)', 1: '요격', 5: '건설', 7: '부대행동', 8: '성공략', 9: '성공략', 10: '성공략', 22: '성공략',
           11: '성공략(직접)', 12: '아군거점이동', 13: '지역점령', 14: '지역점령', 15: '지역점령', 3: '지역점령',
           17: '목표유지', 18: '목표유지'}
N = 220


def mission(m): return MISSION.get(m, f'mode{m}')
def act(a): return ACTION.get(a, f'코드{a}')


def cube(h):
    x, y = h % N, h // N  # columns alternate (odd-q); approximate grid distance
    z = y - (x - (x & 1)) // 2
    return x, z, -x - z


def dist(a, b):
    if a >= 48400 or b >= 48400: return None
    p, q = cube(a), cube(b)
    return max(abs(p[0] - q[0]), abs(p[1] - q[1]), abs(p[2] - q[2]))


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('trace'); ap.add_argument('--out', required=True)
    a = ap.parse_args()
    troops = {}                       # id -> [mode, goal]
    troop_writer = C.Counter()        # (writer, before mission, after mission)
    army = {}                         # slot -> dict state
    step = C.Counter()                # (mission, action) on every army order change
    step_writer = C.Counter()         # (mission, action, writer)
    goal_link = C.Counter()           # army target relation to troops goal per mission
    relay = C.Counter(); relay_gap = []; relay_final = C.Counter()
    with open(a.trace, encoding='utf-8', errors='replace') as h:
        for r in csv.DictReader(h):
            if r['conc'] != '0': continue
            k, f = r['kind'], r['field']
            writer = r['node'] if r['cause'] == 'node' else 'between'
            try: i = int(r['id'])
            except ValueError: continue
            if k == 'troops' and f in ('mode', 'goal'):
                t = troops.setdefault(i, [0, 0]); before = t[0]
                t[0 if f == 'mode' else 1] = int(r['new'] or 0)
                if f == 'mode': troop_writer[(writer, mission(before), mission(t[0]))] += 1
                continue
            if k != 'army': continue
            s = army.setdefault(i, {'action': 0, 'target_kind': 0, 'target_id': 0, 'target_hex': 48400,
                                    'relay': [48400] * 5})
            if f == 'reuse':
                army[i] = {'action': 0, 'target_kind': 0, 'target_id': 0, 'target_hex': 48400, 'relay': [48400] * 5}
                continue
            f2 = f[5:] if f.startswith('init.') else f
            try: new = int(r['new'] or 0)
            except ValueError: continue
            tid = int(r['troops'] or 0)
            t = troops.get(tid, [None, None])
            if f2 in ('action', 'target_kind', 'target_id', 'target_hex'):
                s[f2] = new
                if f2 == 'action' and not f.startswith('init.'):
                    m = mission(t[0]) if t[0] is not None else '?'
                    step[(m, act(new))] += 1; step_writer[(m, act(new), writer)] += 1
                if f2 == 'target_id' and t[0] is not None:
                    m = mission(t[0]); goal = t[1]
                    kind = s['target_kind']
                    if kind in (5, 6) and new == goal: rel = '군세 목표와 같음'
                    elif kind == 9 and new == goal: rel = '군세 목표 칸과 같음'
                    elif kind in (5, 6): rel = '다른 성/관문'
                    elif kind == 9: rel = '다른 지역(경유·점령)'
                    elif kind == 27: rel = '적 부대'
                    elif kind == 11: rel = '시설'
                    else: rel = KIND.get(kind, f'종류{kind}')
                    goal_link[(m, rel)] += 1
            elif f2.startswith('relay') and f2[5:].isdigit():
                n = int(f2[5:]) - 1
                if 0 <= n < 5:
                    s['relay'][n] = new
                    if n == 0 and new < 48400:
                        pos = int(r['hex'] or 48400)
                        d1 = dist(pos, new); dt = dist(pos, s['target_hex'])
                        if d1 is not None: relay_gap.append((d1, dt))
                        relay['relay1 갱신'] += 1
                    last = [x for x in s['relay'] if x < 48400]
                    if last and s['target_hex'] < 48400:
                        d = dist(last[-1], s['target_hex'])
                        relay_final['마지막 중계=최종칸' if d == 0 else '최종칸 인접(≤2)' if d is not None and d <= 2
                                    else '최종칸과 떨어짐'] += 1
    gaps = sorted(d for d, _ in relay_gap)
    rep = {
        'mission_x_step': [[m, s, n] for (m, s), n in step.most_common(60)],
        'mission_x_step_writer': [[m, s, w, n] for (m, s, w), n in step_writer.most_common(80)],
        'mission_changes_by_writer': [[w, b, f, n] for (w, b, f), n in troop_writer.most_common(60)],
        'army_target_vs_mission_goal': [[m, r, n] for (m, r), n in goal_link.most_common(60)],
        'relay1_updates': relay['relay1 갱신'],
        'relay1_distance_from_position': {'median': gaps[len(gaps) // 2] if gaps else None,
                                          'p90': gaps[int(len(gaps) * .9)] if gaps else None},
        'last_relay_vs_final_hex': dict(relay_final),
    }
    os.makedirs(a.out, exist_ok=True)
    json.dump(rep, open(os.path.join(a.out, 'mission_patterns.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
    print(json.dumps(rep, ensure_ascii=False, indent=0)[:12000])


if __name__ == '__main__':
    main()
