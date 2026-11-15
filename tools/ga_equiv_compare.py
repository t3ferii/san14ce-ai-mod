#!/usr/bin/env python
"""Compare two address-table equivalence dumps (tools/build_ga_equiv_dump.ps1, docs/MODULARIZATION_PLAN.md 1-C).

  python -X utf8 tools/ga_equiv_compare.py <before dir> <after dir> [--nonaux <file>] [--ignore-single name,name] [--ignore-line 'V 231AFB' ...]

Each dir holds convenience.txt, combat.txt (+ .fixture.json). Lines carry no ids, only RVAs and bytes:
  V <rva>                      every table value that code read          -> must be equal
  M/A <rva> <n> <bytes> r0|r1  effective expectation of a guard + result -> must be equal
  H/G ...                      guard hash rows / effective hash values   -> must be equal
  D <file>                     source files denied by the address gates  -> must be equal
  T <rva>                      table values                              -> after must contain every non-auxiliary value of before
The fixture json is compared without the keys that name the build (build, unresolved_ids, ...).
--ignore-single: guard_single entries of the fixture json that exist on one side only because the guard itself was deleted (1-F: the
  previous-build clamp form guard modpack_clamp_restore_safe); never used to hide a changed value.
--ignore-line: dump lines that exist on one side only because the code that read/guarded them was deleted (1-F: V 231AFB = the read of the
  previous-build clamp row of modpack_tables.h); each use is named in the commit message.
--nonaux: text file with one hex RVA per line = the table values of before that are not auxiliary containers (default: every T line).
Exit 0 = equal, 1 = difference (listed)."""
import sys, os, json, argparse

DROP = {'build', 'unresolved_ids', 'uncovered'}


def lines(path):
    with open(path, encoding='utf-8') as f:
        return [x.rstrip('\n') for x in f if x.strip()]


def split(ls):
    d = {}
    for x in ls:
        d.setdefault(x[0], set()).add(x)
    return d


def strip(o):
    if isinstance(o, dict):
        return {k: strip(v) for k, v in o.items() if k not in DROP}
    if isinstance(o, list):
        return [strip(v) for v in o]
    return o


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('before')
    ap.add_argument('after')
    ap.add_argument('--nonaux')
    ap.add_argument('--ignore-single', default='')
    ap.add_argument('--ignore-line', action='append', default=[])
    a = ap.parse_args()
    nonaux = None
    if a.nonaux:
        nonaux = {('T ' + x.strip().upper()) for x in open(a.nonaux, encoding='utf-8') if x.strip()}
    ignore = [x for x in a.ignore_single.split(',') if x]
    bad = 0
    for v in ('convenience', 'combat'):
        skip = set(a.ignore_line)
        b, c = split([x for x in lines(os.path.join(a.before, v + '.txt')) if x not in skip]), split([x for x in lines(os.path.join(a.after, v + '.txt')) if x not in skip])
        for k in sorted(set(b) | set(c)):
            sb, sc = b.get(k, set()), c.get(k, set())
            if k == 'T':
                need = sb if nonaux is None else (sb & nonaux)
                miss = need - sc
                print('%s T: before %d (required %d) after %d missing %d' % (v, len(sb), len(need), len(sc), len(miss)))
                if miss:
                    bad += 1
                    for x in sorted(miss)[:20]:
                        print('   missing', x)
                continue
            if sb != sc:
                bad += 1
                print('%s %s: before %d after %d  only-before %d only-after %d' % (v, k, len(sb), len(sc), len(sb - sc), len(sc - sb)))
                for x in sorted(sb - sc)[:10]:
                    print('   -', x[:160])
                for x in sorted(sc - sb)[:10]:
                    print('   +', x[:160])
            else:
                print('%s %s: %d equal' % (v, k, len(sb)))
        jb = os.path.join(a.before, v + '.fixture.json')
        ja = os.path.join(a.after, v + '.fixture.json')
        if os.path.exists(jb) and os.path.exists(ja):
            def load(p):
                t = open(p, encoding='utf-8-sig').read().strip().splitlines()
                out = [strip(json.loads(x)) for x in t if x.strip().startswith('{')]
                for o in out:
                    for k in ignore:
                        if isinstance(o.get('guard_single'), dict):
                            o['guard_single'].pop(k, None)
                return out
            if load(jb) != load(ja):
                bad += 1
                print('%s fixture json differs' % v)
                for x, y in zip(load(jb), load(ja)):
                    if x != y:
                        for key in sorted(set(x) | set(y)):
                            if x.get(key) != y.get(key):
                                print('   key', key, str(x.get(key))[:200], '|', str(y.get(key))[:200])
            else:
                print('%s fixture json: equal' % v)
    print('EQUIVALENT' if not bad else 'DIFFERENT (%d)' % bad)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
