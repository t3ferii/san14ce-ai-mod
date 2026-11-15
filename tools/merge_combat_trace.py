"""Merge an ai_trace CSV (trace_mod_*) with the diagnostic SAN14_CombatAI.log by (game day, army slot).

Read-only analysis of files written by the game. Writes summary.json and report.md to --out.
Army ids: trace `id` and combat-log `army` are both army slots (22DA10, 1..500).
Writer labels: original node name, `mod:<event>` when the combat DLL logged an order event for that
army on that day, `between` for changes outside behaviour-tree nodes (daily processing, combat, player).
"""
import argparse, collections as C, csv, json, os, re, sys
sys.path.insert(0, os.path.dirname(__file__))
from analyze_ai_trace import ACTION, KIND  # noqa: E402

LOG = re.compile(r'^\S+ \S+ day=(-?\d+) thread=\d+ (\w+) army=(\d+) value=(\d+) hex=(\d+)')
ORDER = ('action', 'target_kind', 'target_id')
# Combat-DLL hooks chained over these nodes (native_adapter.cpp: first_army_node, cut_node, city_node,
# retreat_node). A change inside them may be the mod policy or the original node it fell back to.
MOD_NODES = {'CSetRelayHexEvadeFlameHexNode': 'first_army_node', 'CCutLogisticsNode': 'cut_node',
             'CExecuteFootholdOccupyNode': 'city_node', 'CArmySetRetreatNode': 'retreat_node'}
# Combat-log events that are not per-army order decisions (army field is a person/count or no order).
NOT_ORDER = {'dispatch_native_command', 'build_skip_native_check', 'build_skip_present', 'build_skip_no_free_site',
             'build_skip_area_cap', 'build_skip_native_candidates', 'capture_credit', 'ai_wall_build_blocked',
             'cohesive_sortie_departed', 'campaign_reinforcement_unavailable', 'native_retreat_allowed'}


def act(x): return ACTION.get(x, f'코드{x}')
def kind(x): return KIND.get(x, f'종류{x}')
def order_text(o): return f'{act(o[0])}/{kind(o[1])}#{o[2]}' if o else '-'


class Sortie:
    __slots__ = ('slot', 'start', 'end', 'origin', 'commander', 'force', 'order', 'changes', 'events', 'context',
                 'removed')

    def __init__(self, slot, day, origin, commander, force):
        self.slot, self.start, self.end, self.origin = slot, day, day, origin
        self.commander, self.force = commander, force
        self.order = [0, 0, 0]
        self.changes = []   # (day, writer, before, after)
        self.events = []    # (day, event, value, hex)
        self.context = ''
        self.removed = False


def load_log(path):
    events = C.defaultdict(list)  # slot -> [(day, event, value, hex)]
    kinds = C.Counter()
    with open(path, encoding='utf-8', errors='replace') as h:
        for line in h:
            m = LOG.match(line)
            if not m: continue
            day, ev, army, value, hexv = int(m[1]), m[2], int(m[3]), int(m[4]), int(m[5])
            kinds[ev] += 1
            if ev in NOT_ORDER or not 1 <= army <= 500: continue
            events[army].append((day, ev, value, hexv))
    return events, kinds


def load_trace(path):
    sorties, live = [], {}
    pending = None  # (slot, day, writer, seq-block) accumulating one node call's field changes

    def flush():
        nonlocal pending
        if not pending: return
        s, day, writer, before = pending
        after = tuple(s.order)
        if after != before: s.changes.append((day, writer, before, after))
        pending = None

    with open(path, encoding='utf-8', errors='replace') as h:
        for r in csv.DictReader(h):
            if r['kind'] != 'army' or r['conc'] != '0': continue
            try: slot, day = int(r['id']), int(r['day'])
            except ValueError: continue
            field = r['field']
            if field == 'reuse':
                flush()
                old = live.get(slot)
                if old: old.end = day
                s = Sortie(slot, day, r['node'] if r['cause'] == 'node' else 'between', r['commander'], r['force'])
                live[slot] = s; sorties.append(s)
                continue
            s = live.get(slot)
            if s is None: continue
            s.end = day
            if r.get('context'): s.context = r['context']
            if field.startswith('init.'):
                f = field[5:]
                if f in ORDER: s.order[ORDER.index(f)] = int(r['new'] or 0)
                continue
            if field == 'removed' or field == 'commander' and r['new'] == '0':
                flush(); s.removed = True; live.pop(slot, None); continue
            if field not in ORDER: continue
            writer = r['node'] if r['cause'] == 'node' else 'between'
            key = (slot, day, writer, r['seq'] if False else r['cause'] + r['node'])
            if not pending or pending[0] is not s or pending[1] != day or pending[2] != writer:
                flush(); pending = (s, day, writer, tuple(s.order))
            s.order[ORDER.index(field)] = int(r['new'] or 0)
        flush()
    return sorties


def attach(sorties, events):
    by_slot = C.defaultdict(list)
    for s in sorties: by_slot[s.slot].append(s)
    for slot, evs in events.items():
        lst = sorted(by_slot.get(slot, []), key=lambda s: s.start)
        for e in evs:
            for s in reversed(lst):
                if s.start <= e[0] or e[0] < 0:
                    if e[0] <= s.end + 10: s.events.append(e)
                    break


def label(s):
    """Attribute each change: mod event of the same day on a mod-chained node or between."""
    ev_day = C.defaultdict(list)
    for d, ev, *_ in s.events: ev_day[d].append(ev)
    out = []
    for day, writer, before, after in s.changes:
        evs = ev_day.get(day, [])
        if writer in MOD_NODES or writer == 'between':
            w = ('mod:' + '+'.join(sorted(set(evs)))) if evs else (f'{writer}(원작/모드 미구분)' if writer in MOD_NODES else 'between')
        else:
            w = writer
        out.append((day, w, before, after))
    return out


def analyze(sorties, kinds):
    rep = {'sorties': len(sorties), 'log_event_kinds': dict(kinds.most_common())}
    writer_pairs = C.Counter(); flips = C.Counter(); flip_ex = []
    builds = C.Counter(); build_ex = []; wait = []; writer_count = C.Counter(); per_origin = C.Counter()
    for s in sorties:
        ch = label(s); per_origin[s.origin] += 1
        for c in ch: writer_count[c[1].split('+')[0] if c[1].startswith('mod:') else c[1]] += 1
        # A->B->A within 10 days by different writers: tug of war.
        for i in range(2, len(ch)):
            a, b, c = ch[i - 2], ch[i - 1], ch[i]
            if c[3] == a[3] and b[3] != a[3] and c[0] - a[0] <= 10 and b[1] != c[1]:
                flips[(b[1], c[1])] += 1
                if len(flip_ex) < 40: flip_ex.append((s.slot, s.start, s.context, a, b, c))
        for i in range(1, len(ch)):
            if ch[i][1] != ch[i - 1][1] and ch[i][0] - ch[i - 1][0] <= 1:
                writer_pairs[(ch[i - 1][1], ch[i][1])] += 1
        nb = sum(1 for e in s.events if e[1] == 'build_started')
        builds[min(nb, 10)] += 1
        if nb >= 4 and len(build_ex) < 25: build_ex.append((s.slot, s.start, s.end, nb, s.context, s.origin))
        # Waiting: order 대기(13) held for >=30 days, or stationary retreat event.
        t0 = None
        for day, w, before, after in ch:
            if after and after[0] == 13 and t0 is None: t0 = (day, w)
            elif t0 and (not after or after[0] != 13):
                if day - t0[0] >= 30: wait.append((s.slot, t0[0], day, t0[1], w, s.context))
                t0 = None
    rep['changes_by_writer'] = dict(writer_count.most_common(40))
    rep['sorties_by_origin'] = dict(per_origin.most_common())
    rep['tug_of_war_A_B_A'] = [[f'{k[0]} -> {k[1]}', v] for k, v in flips.most_common(30)]
    rep['same_day_overwrites'] = [[f'{k[0]} -> {k[1]}', v] for k, v in writer_pairs.most_common(30)]
    rep['builds_per_sortie'] = {str(k): v for k, v in sorted(builds.items())}
    rep['examples_flip'] = [{'slot': x[0], 'start': x[1], 'context': x[2],
                             'steps': [f'{c[0]} {c[1]}: {order_text(c[2])} -> {order_text(c[3])}' for c in x[3:]]}
                            for x in flip_ex]
    rep['examples_build_spam'] = [dict(zip(('slot', 'start', 'end', 'builds', 'context', 'origin'), x)) for x in build_ex]
    rep['long_wait_13'] = len(wait)
    rep['long_wait_by_writer'] = [[f'{k[0]} .. {k[1]}', v] for k, v in C.Counter((w[3], w[4]) for w in wait).most_common(20)]
    rep['examples_wait'] = [dict(zip(('slot', 'from', 'to', 'set_by', 'cleared_by', 'context'), w)) for w in wait[:25]]
    return rep


def story(sorties, slot, start):
    items = []  # a slot can be reused more than once on its start day
    for s in sorties:
        if s.slot == slot and s.start == start:
            items += [(d, 0, f'{w}: {order_text(b)} -> {order_text(a)}') for d, w, b, a in label(s)]
            items += [(d, 1, f'  [모드] {ev} value={v} hex={h}') for d, ev, v, h in s.events]
    return [f'{d} {t}' for d, _, t in sorted(items)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('trace'); ap.add_argument('log'); ap.add_argument('--out', required=True)
    ap.add_argument('--story', nargs=2, type=int, metavar=('SLOT', 'START_DAY'))
    a = ap.parse_args()
    events, kinds = load_log(a.log)
    sorties = load_trace(a.trace)
    attach(sorties, events)
    if a.story:
        print('\n'.join(story(sorties, *a.story))); return
    rep = analyze(sorties, kinds)
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, 'summary.json'), 'w', encoding='utf-8') as h:
        json.dump(rep, h, ensure_ascii=False, indent=1)
    print(json.dumps({k: v for k, v in rep.items() if not k.startswith('examples') and k != 'log_event_kinds'},
                     ensure_ascii=False, indent=1))


if __name__ == '__main__':
    main()
