#!/usr/bin/env python
"""Static address table checks, single reference build (user 2026-11-05 patch resilience; docs/PATCH_RESILIENCE.md). Part of dll-split.

  python -X utf8 tools/check_addresses.py                     # all checks against the reference build 1a4a447d
  python -X utf8 tools/check_addresses.py --scan <unpacked.exe> [out.tsv]   # new patch: resolve every id by signature/RTTI, list the rest

(a) every table id: the runtime resolver (src/game_addresses.h, the real header compiled into tests/addr_resolver_fixture.cpp) run over the
    unpacked EXE returns exactly the table value for every id that has a resolution method and a table value (unique signature / RTTI /
    slot pointer / data reference); ids the table denies (value 0) or that have no method are listed. Scan time must stay below 250 ms.
    The data file must parse (tools/ga_table.py), carry one build column and the timestamp of the unpacked EXE.
(b) feature sources contain no original-range hex RVA constant (>= 0x10000 or a listed 4-digit RVA) outside the explicit allow list (object
    offsets etc., with the reason: tools/addrlib.py NON_RVA / OFFSET_PRE); (b2) every guard-profile row ends in a hash literal wrapped in
    GH().
(c) the real module guards (split_feature_guards incl. address gates / split_guards_match) run against the unpacked image
    (tests/address_guard_fixture.cpp): every feature must stay on except the CCZ ones the file-only fixture cannot judge; the convenience
    fixture also runs the guards of the imported-code units (modpack, traits9) and of the split_activate() installers. Exact-byte tamper
    test (mcmp / mcmp_at see a flipped rel32 byte) and the read-only observer tolerance (c3).
(c4) tools/check_observer_hooks.py (ga_observers table, San14Sim profile, impact rows, ModPack blob fingerprints).
(d)  tools/addr_promote.py check: GH literal rows equal the FNV-1a of the image bytes, guard dump literals equal the image bytes.
Exit code 0 only when all pass.
"""
import os, sys, re, json, subprocess, argparse
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import addrlib as A
import ga_table as G

ADDR_DIR = os.path.join(A.ROOT, 'build', 'development', 'addr')
INL = os.path.join(A.ROOT, 'src', 'game_addresses_data.inl')
EXE = os.path.join(A.ROOT, 'build', 'ui_native_analysis', 'editor83_b25953386', 'game.exe.unpacked.exe')
# the CCZ guards load SAN14_CCZ.dll, which the file-only fixture does not have: reported, never judged
NOT_COVERED = {'ccz_sprites'}
fails = []


def fail(msg):
    fails.append(msg)
    print('FAIL', msg)


def build_fixtures():
    r = subprocess.run(['powershell', '-NoProfile', '-File', os.path.join(A.TOOLS, 'build_addr_fixtures.ps1')], capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0:
        print(r.stdout[-3000:], r.stderr[-3000:])
        fail('address fixtures failed to build')
        return False
    return True


def run_json(exe, *args):
    r = subprocess.run([os.path.join(ADDR_DIR, exe)] + list(args), capture_output=True, text=True, encoding='utf-8', errors='replace')
    last = [l for l in r.stdout.splitlines() if l.startswith('{')]
    return r.returncode, (json.loads(last[-1]) if last else None), r.stderr


def check_a(rep):
    T = G.parse(INL)
    if len(T.builds) != 1:
        fail('(a) src/game_addresses_data.inl carries %d build columns (expected the single reference build)' % len(T.builds))
    elif T.builds[0][0] != '1a4a447d':
        fail('(a) the table column is %s, expected 1a4a447d' % T.builds[0][0])
    rc, res, err = run_json('addr_resolver_fixture.exe', EXE)
    if rc != 0 or not res:
        fail('(a) resolver fixture exit %s: %s' % (rc, err[-500:]))
        return
    rep['a'] = res
    if res['mismatch']:
        fail('(a) %d ids resolve to a value different from the table' % res['mismatch'])
    if res['scan_ms'] > 250:
        fail('(a) signature scan took %.0f ms (limit 250, goal 100)' % res['scan_ms'])
    print('(a) %s: %d ids, scan %d resolved in %.0f ms, %d match the table, %d mismatch, %d denied by policy, %d unresolved' % (
        res['build'], res['ids'], res['scan_resolved'], res['scan_ms'], res['matched_by_scan'], res['mismatch'], res['denied_by_policy'], res['unresolved']))


def check_b(rep):
    srcdir = os.path.join(A.ROOT, 'src')
    allowed, bad = {}, []
    for fn in sorted(os.listdir(srcdir)):
        p = os.path.join(srcdir, fn)
        if not os.path.isfile(p) or os.path.splitext(fn)[1] not in A.SRC_EXT or fn.startswith('game_addresses'):
            continue
        lits, lines = A.literals(p)
        extra = []
        for ln, a, b in A.code_spans(lines):
            seg = lines[ln][a:b]
            for m in A.HEX4.finditer(seg):
                v = int(m.group(1), 16)
                if v >= 0x1000:
                    extra.append((ln + 1, a + m.start(), m.group(0), v))
        for ln, col, text, val in lits + [(e[0], e[1], e[2], e[3]) for e in extra]:
            if not (A.IMG_LO <= val < A.IMG_HI):
                continue
            pre = lines[ln - 1][:col]
            post = lines[ln - 1][col + len(text):]
            if len(text) == 6:        # 4-digit literal: an RVA only inside call / list context (same rules as the scanner)
                prev = re.search(r'(0[xX][0-9A-Fa-f]{5,9}),\s*$', pre)
                listed = prev is not None and A.IMG_LO <= int(prev.group(1), 16) < A.IMG_HI and not re.match(r'\s*,\s*0[xX][0-9A-Fa-f]{8,}', post)
                profile_row = pre.rstrip().endswith('{') and re.match(A.PROFILE_ROW_POST, post) is not None
                if not (re.search(r'>\(\s*$', pre) or re.search(r'image\s*\+\s*$', pre) or listed or profile_row or re.match(r'\s*,\s*0[xX][0-9A-Fa-f]{4,9}\b', post)) or 'GUID' in lines[ln - 1]:
                    continue
            reason = None
            if (fn, val) in A.NON_RVA:
                reason = A.NON_RVA[(fn, val)]
            elif any(r.search(pre) for r in A.OFFSET_PRE):
                reason = 'object field/table offset or comparison context'
            if reason:
                allowed.setdefault(reason, 0)
                allowed[reason] += 1
            else:
                bad.append('%s:%d %s' % (fn, ln, text))
    # (b2) a guard-profile row ends in a hash literal that must go through GH() (the marker the tools recognise)
    raw_hash = []
    for fn in sorted(os.listdir(srcdir)):
        p = os.path.join(srcdir, fn)
        if not os.path.isfile(p) or os.path.splitext(fn)[1] not in A.SRC_EXT or fn.startswith('game_addresses'):
            continue
        for ln, line in enumerate(open(p, encoding='utf-8', errors='replace'), 1):
            if re.search(r'^\s*\{[^{}]*,\s*0[xX][0-9A-Fa-f]{12,16}[uU][lL]{2}\s*\}', line) and 'GH(' not in line:
                raw_hash.append('%s:%d' % (fn, ln))
    rep['b_raw_hash_rows'] = len(raw_hash)
    if raw_hash:
        fail('(b2) %d guard-profile rows with a raw hash (wrap it in GH()): %s' % (len(raw_hash), ', '.join(raw_hash[:12])))
    rep['b_allowed'] = allowed
    rep['b_allowed_total'] = sum(allowed.values())
    rep['b_violations'] = len(bad)
    if bad:
        fail('(b) %d original-range hex constants in feature sources: %s%s' % (len(bad), ', '.join(bad[:12]), ' ...' if len(bad) > 12 else ''))
    print('(b) feature sources: %d RVA-range constants left, all allowed non-addresses: %s' % (rep['b_allowed_total'], json.dumps(allowed, ensure_ascii=False)))


def check_c(rep):
    out = {}
    for exe in ('address_guard_convenience.exe', 'address_guard_combat.exe'):
        rc, res, err = run_json(exe, EXE)
        if rc != 0 or not res:
            fail('(c) %s exit %s %s' % (exe, rc, err[-300:]))
            continue
        out[res['module']] = res
        if res['module'] == 'convenience' and res.get('adjacency_push') != 0x56:
            fail('(c) 2B07C0 displaced push byte is %s (expected 0x56)' % res.get('adjacency_push'))
        if res['module'] == 'convenience':
            # known-build guards compare exact bytes (a flipped rel32 byte must be detected, the pristine image must pass)
            if res.get('exact_pristine') != 0 or res.get('exact_at_pristine') != 0 or res.get('exact_tampered') != 1 or res.get('exact_at_tampered') != 1:
                fail('(c) exact-byte guard test failed: %s' % {k: res.get(k) for k in ('exact_pristine', 'exact_tampered', 'exact_at_pristine', 'exact_at_tampered')})
            else:
                print('(c) exact-byte guards: pristine passes, flipped rel32 byte detected (mcmp and mcmp_at)')
        # (c3) shared read-only observer tolerance (ga::observers / hash_ok / the observed whole-text CRC): with the 24 entries of San14Sim's
        # version.dll hooked (E9 rel32 + NOP) every guard gives the pristine result; an E9 into the game, other padding, a changed byte behind
        # the prefix and a longer prefix are rejected. A patch of an observed entry is a conflict.
        ob = res.get('observer') or {}
        common = ('table_ok', 'pristine_raw', 'pristine_ready', 'all_raw_differs', 'all_text_ok', 'all_ready', 'inside_fails', 'tail_fails', 'pad_fails',
                  'longer_fails', 'int3_ok', 'conflict_blocks', 'pristine_link', 'restored')
        conv = ('deaths_pristine', 'deaths_all', 'all_bd', 'all_adv', 'all_singles_unchanged', 'all_features_unchanged', 'each_ok', 'adv_inside_fails', 'adv_pristine')
        need = common + (conv if res['module'] == 'convenience' else ())
        if not ob or ob.get('count') != 24 or not all(ob.get(k) is True for k in need):
            fail('(c3) %s: observer-hook tolerance test failed: %s' % (res['module'], {k: ob.get(k) for k in need if ob.get(k) is not True} or ob))
        else:
            print('(c3) %s: 24 observer entries hooked: text CRC exact, guards unchanged; inside-image / padding / tail / longer prefix rejected' % res['module'])
        real = [f for f in res['disabled'] if f not in NOT_COVERED]
        if real or not res['guards_ok']:
            fail('(c) %s: guards switch features off or fail: %s ok=%s' % (res['module'], real, res['guards_ok']))
        bad_single = sorted(k for k, v in res.get('guard_single', {}).items() if v is False)
        if bad_single:
            fail('(c) %s: single guards fail: %s' % (res['module'], bad_single))
        if res['module'] == 'convenience':
            if res.get('hover_off') is not False or 'battle_deaths' in res['disabled']:
                fail('(c) hover / battle_deaths must stay on (hover_off=%s, disabled=%s)' % (res.get('hover_off'), res['disabled']))
            for needed in ('trait_hover', 'battle_deaths', 'battle_deaths_rows', 'traits9_slots', 'traits9_before_runtime', 'traits9_emit', 'modpack_collab', 'modpack_scenery',
                           'activate_speed_buttons', 'activate_lord_change', 'activate_tactic_priority', 'activate_name_lookup', 'activate_occupy_fast',
                           'activate_caravan_stay', 'activate_drag_learn', 'advisor_briefing',
                           'traits9_edit_list', 'traits9_screens_sites', 'traits9_screens_geometry'):   # local236 개성 9칸 범위 B
                if needed not in res.get('guard_single', {}):
                    fail('(c) single guard %s is not covered by the fixture' % needed)
        print('(c) %-11s guards_ok=%s disabled(%d): %s' % (res['module'], res['guards_ok'], len(res['disabled']), ','.join(res['disabled'])))
    rep['c'] = out


def check_d(rep):
    r = subprocess.run([sys.executable, '-X', 'utf8', os.path.join(A.TOOLS, 'addr_promote.py'), 'check', '--exe', EXE], capture_output=True, text=True, encoding='utf-8', errors='replace')
    print(r.stdout.strip())
    if r.returncode:
        fail('(d) addr_promote check: GH literals or guard literals differ from the image')


def scan_new(exe, out):
    if not build_fixtures():
        return 1
    rc, res, err = run_json('addr_resolver_fixture.exe', exe, 'scan', out)
    print(res or err)
    ids = [l.split('\t') for l in open(out, encoding='utf-8')]
    un = [(i[0], i[2].strip()) for i in ids if i[1] == '0' and i[2].strip() != '0']
    print('%d ids with a resolution method are unresolved in this build: %s' % (len(un), ' '.join('GA_%s' % u[0] for u in un[:40])))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--scan', nargs='+')
    ap.add_argument('--skip-cpp', action='store_true')
    a = ap.parse_args()
    if a.scan:
        return scan_new(a.scan[0], a.scan[1] if len(a.scan) > 1 else os.path.join(ADDR_DIR, 'scan.tsv'))
    rep = {}
    if not a.skip_cpp and build_fixtures():
        check_a(rep)
        check_c(rep)
    check_b(rep)
    check_d(rep)
    import check_observer_hooks            # (c4) observer table, San14Sim profile, impact rows, ModPack blob fingerprints
    if check_observer_hooks.run():
        fail('(c4) observer hook checks failed (see above)')
    os.makedirs(ADDR_DIR, exist_ok=True)
    json.dump(rep, open(os.path.join(ADDR_DIR, 'check_addresses_report.json'), 'w', encoding='utf-8'), indent=1)
    print(json.dumps({'status': 'check_addresses_failed' if fails else 'check_addresses_passed', 'failures': len(fails)}))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
