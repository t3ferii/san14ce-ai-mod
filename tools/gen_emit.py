#!/usr/bin/env python
"""Emit the candidate address table of a patch (called by tools/patch_next.py emit; docs/PATCH_RESILIENCE.md).

The table of src/game_addresses_data.inl has one build column, the reference build. For a new Steam build the candidate carries two
columns, [reference, target], in the layout tools/addr_promote.py expects as its input (`plan` / `apply` re-base the table and the sources to the
target and write the single column again). The target column comes from tools/gen_signatures.py (Analysis); ids it could not find are 0.
Besides the values the candidate holds what a re-base needs from the target image: the guard-hash pairs (GH literal of the reference build ->
hash of the same range in the target), the read-only observer entries and the readiness probes with bytes that exist in both builds.
With ref == target the candidate is the committed table again (the self test of patch_next: `emit --check` compares it byte for byte).
Standard library only."""
import os, sys, json, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ga_table as G
import gen_signatures as S

ROOT = S.ROOT
OBSERVERS = os.path.join(ROOT, 'tools', 'observer_hooks.json')
PROBES = 12
LAYOUT_BUFFER_LIMIT = 0x8F0000        # the layout fixture buffers stop at 0x901000: probes live below


def build_row(b):
    """(name, TimeDateStamp, .text size without trailing zeros, CRC32 of it, SizeOfImage) like ga_builds[]."""
    t = b.text
    size = len(t.rstrip(b'\0'))
    return (b.name, b.timestamp, size, zlib.crc32(t[:size]) & 0xFFFFFFFF, b.image_size)


def observer_rvas(A, ent):
    """target RVA of every observer entry (function start in the reference build): the id's target value, else a unique masked prefix."""
    out = []
    rb, tb = A.rb, A.tb
    for o in ent:
        ref_rva, ln = int(o['new'], 16), int(o['len'])
        i = A.rva_idx.get(ref_rva)
        tv = A.res[i]['tv'] if i is not None else 0
        if not tv:
            raw, mk = rb.b.masked(ref_rva, 32)
            if not (mk[0] or mk[1]):
                hits = tb.sig_matches(raw[:16], mk[:16]) if len(raw) >= 16 else []
                tv = hits[0] if len(hits) == 1 else 0
        if tv and tb.b.raw(tv, ln) != rb.b.raw(ref_rva, ln):
            tv = 0                                                  # the displaced prefix must be the same bytes in both builds
        out.append(tv)
    return out


def probes(A, T_old):
    """12 readiness probes: the old ones when their 5 prologue bytes are identical in both builds, else evenly chosen replacements."""
    rb, tb = A.rb, A.tb
    ok = []
    for i, e in enumerate(A.T.entries):
        tv = A.res[i]['tv']
        if e.kind != S.K_FUNC or e.method != S.M_SIG or not tv or not e.vals[A.ref_name] or e.rva >= LAYOUT_BUFFER_LIMIT or tv >= LAYOUT_BUFFER_LIMIT + 0x8000:
            continue
        ro, mo = rb.b.masked(e.rva, 8)
        rn, mn = tb.b.masked(tv, 8)
        if ro[:5] == rn[:5] and not any(mo[:5]) and not any(mn[:5]) and ro[0] not in (0x00, 0xCC):
            ok.append((e.name, tuple(ro[:5])))
    keep = [p for p in T_old.probes if p in ok]
    rest = [p for p in ok if p not in keep]
    step = max(1, len(rest) // max(1, PROBES - len(keep)))
    out = keep + rest[::step][:PROBES - len(keep)]
    if len(out) != PROBES:
        raise SystemExit('only %d readiness probes with identical prologue bytes in both builds' % len(out))
    return out[:PROBES]


def candidate(A, ref, tgt, pairs):
    """G.Table with the build columns [reference, target] (a single column when target is the reference) from an Analysis."""
    T0 = A.T
    N = G.Table()
    two = ref.sha != tgt.sha
    for i, e in enumerate(T0.entries):
        n = G.Entry()
        for f in ('name', 'rva', 'kind', 'method', 'len', 'a', 'b', 'c', 'd', 'pat', 'mask', 'rname', 'wild'):
            setattr(n, f, getattr(e, f))
        n.users = set(e.users)
        n.vals = {ref.name: e.vals[A.ref_name]}
        if two:
            n.vals[tgt.name] = A.res[i]['tv']
        N.entries.append(n)
    N.files = T0.files
    N.builds = [build_row(ref)] + ([build_row(tgt)] if two else [])
    ent = json.load(open(OBSERVERS, encoding='utf-8'))['entries']
    if two:
        N.observers = [((int(o['new'], 16), tv), int(o['len']), bytes(ref.raw(int(o['new'], 16), int(o['len']))) + bytes(16 - int(o['len'])))
                       for o, tv in zip(ent, observer_rvas(A, ent))]
        N.probes = probes(A, T0)
        N.hash_pairs = [tuple(p) for p in pairs]
    else:
        N.observers = list(T0.observers)
        N.probes = list(T0.probes)
    return N


def header(ref, tgt):
    two = ref.sha != tgt.sha
    h = ['// Candidate table of tools/patch_next.py (reference build %s, target build %s). Do not edit by hand.' % (ref.name, tgt.name if two else ref.name),
         '// id GA_<HEX> = RVA of the reference build %s; value columns ga_val_<build>[id]%s.' % (ref.name, ' (second column = target, 0 = not found)' if two else '')]
    return h


def emit_text(A, ref, tgt, pairs):
    return G.emit(candidate(A, ref, tgt, pairs), header(ref, tgt))


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(description='candidate address table of a patch')
    ap.add_argument('--ref', default='current')
    ap.add_argument('--target', required=True)
    ap.add_argument('--out', required=True, help='file for the candidate game_addresses_data.inl')
    ap.add_argument('--check', action='store_true', help='ref == target: the candidate must equal src/game_addresses_data.inl')
    a = ap.parse_args(argv)
    ref, tgt = S.P.open_build(a.ref), S.P.open_build(a.target)
    A = S.Analysis(ref, tgt, S.load_table())
    A.run()
    pairs = S.guard_hash_pairs(A) if ref.sha != tgt.sha else []
    text = emit_text(A, ref, tgt, pairs)
    open(a.out, 'w', encoding='utf-8', newline='\n').write(text)
    print('wrote', a.out, len(text), 'bytes')
    if a.check:
        cur = open(S.INL, encoding='utf-8').read()
        same = cur.split('\n', 2)[2] == text.split('\n', 2)[2]
        print('candidate equals the committed table body:', same)
        return 0 if same else 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
