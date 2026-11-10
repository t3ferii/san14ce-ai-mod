#!/usr/bin/env python
"""Self test of tools/hook_map.py (check redesign package A, 2026-11-06, docs/HISTORY.md section 7 A).

Builds a small synthetic source tree (a miniature of src/ + development/, two builds) under
build/development/hookmap_selftest/<case>/ and checks:
  - the clean tree gives no error and no warning,
  - each rule R1..R14 fires exactly once (one distinct finding group, no other error) when the matching defect is added,
  - a policy entry suppresses an error (and is counted), a policy entry that matches nothing is reported as stale,
  - a hook_sites.json entry that matches nothing is reported as stale.
Writes only below build/development/hookmap_selftest. No game, no EXE (R13 uses an in-memory reader). Standard library only.
"""
import os, sys, json, shutil, re, collections

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import hookmap_extract as X          # noqa: E402
import hook_map as H                 # noqa: E402

BASE = os.path.join(ROOT, 'build', 'development', 'hookmap_selftest')
OLD, NEW = '1fbe6be7', '1a4a447d'
IDS = ['6000', '9002', 'A000', 'A010', 'B000', 'B002', 'B008', 'C000', 'D00E', 'F000', 'F010', 'F100']     # old-build RVAs
NEW_DELTA = 0x100


def data_inl(kinds=None):
    k = dict(KINDS)
    k.update(kinds or {})
    entries = ','.join('{0x%s,%d,0,0,0u,0u,0u,0}' % (g, ('func', 'mid', 'slot', 'data').index(k[g])) for g in IDS)
    n = len(IDS)
    enum = ','.join('GA_%s=%d' % (g, i) for i, g in enumerate(IDS)) + ',GA_COUNT=%d' % n
    old = ','.join('0x%s' % g for g in IDS)
    new = ','.join('0x%X' % (int(g, 16) + NEW_DELTA) for g in IDS)
    return ('// synthetic game_addresses_data.inl\nenum GaId : unsigned {\n%s};\n'
            'static const unsigned ga_val_%s[GA_COUNT]={\n%s};\nstatic const unsigned ga_val_%s[GA_COUNT]={\n%s};\n'
            'enum : unsigned {GA_OBSERVER_COUNT=1};\nstruct GaObserver {unsigned rva[2];unsigned char len;unsigned char bytes[16];};\n'
            'static const GaObserver ga_observers[GA_OBSERVER_COUNT]={{{0x9000,0x9100},5,{0x40,0x55,0x56,0x57,0x41,0,0,0,0,0,0,0,0,0,0,0}}};\n'
            'static const GaEntry ga_entries[GA_COUNT]={%s};\n') % (enum, OLD, old, NEW, new, entries)


REGISTRY = '''#pragma once
#include "layout_api.h"
enum LayoutRowKind : unsigned char {LR_VT=1,LR_CALL=2,LR_JUMP=3};
struct LayoutRow {const char *owner;unsigned char kind;uint32_t rva,original,flags;};
enum : uint32_t {
    LF_C=LAYOUT_CHAIN_OK|LAYOUT_ALL_OR_NONE,
    LF_CY=LF_C|LAYOUT_YIELD_IF_PEER,
    LF_CW=LF_C|LAYOUT_NEED_WHITELIST
};
static const LayoutRow layout_rows[]={
    {"alpha",LR_VT,GA(GA_A000),GA(GA_F000),LF_CY},
    {"alpha",LR_CALL,GA(GA_B000),GA(GA_F100),LF_C},
    {"beta",LR_VT,GA(GA_A010),GA(GA_F010),LF_CW},
};
static const unsigned layout_row_count=sizeof(layout_rows)/sizeof(layout_rows[0]);
static const uint32_t layout_blocked_entries[]={GA(GA_6000)};
'''
CORE = '''static const unsigned kMaxHooks=384;
static const PeerRule peer_rules[]={
    {"alpha",1,{R1_MOD(L"Alpha.ini")}},
};
static const uint32_t blocked_entries[]={GA(GA_6000)};
'''
CLIENT = 'static const unsigned kMaxHandles=384;\n'
ADAPTER = '''#include "game_addresses.h"
#include "layout_registry.h"
#include "native_layout_client.inl"
#ifdef SPLIT_BASE
#include "native_alpha.inl"
#include "native_beta.inl"
#include "native_c.inl"
#endif
'''
ALPHA = '''static bool register_alpha() {
    if(layout_client::vt(GA(GA_A000),reinterpret_cast<void *>(&alpha_vt),&next_vt)<=0 ||
       layout_client::call(GA(GA_B000),reinterpret_cast<void *>(&alpha_call),&next_call)<=0)return false;
    return true;
}
'''
BETA = '''static bool register_beta() {
    return layout_client::vt(GA(GA_A010),reinterpret_cast<void *>(&beta_vt),&next_beta)>0;
}
'''
C_FILE = '''static const unsigned char kExpect[16]={0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8D,0x6C,0x24};
static bool prepare_c(Obj relay) {
    if(ga::mcmp(image,GA_C000,0,kExpect,16))return false;
    return prepare_link(link,GA(GA_C000),0xE9,relay);
}
'''
FILES = {'src/game_addresses_data.inl': None, 'src/layout_registry.h': REGISTRY, 'src/native_layout_core.inl': CORE,
         'src/native_layout_client.inl': CLIENT, 'src/native_adapter.cpp': ADAPTER, 'src/native_alpha.inl': ALPHA,
         'src/native_beta.inl': BETA, 'src/native_c.inl': C_FILE,
         'src/layout_main.cpp': '#include "native_layout_core.inl"\n',
         'src/ccz_module.cpp': '#include "game_addresses.h"\n',
         'development/hook_sites.json': json.dumps({'schema': 1, 'bindings': [], 'ignore_calls': [], 'com_sites': []}),
         'development/foreign_hooks.json': json.dumps({'schema': 1, 'mods': {}, 'sites': [], 'relations': []}),
         'development/hook_policy.json': json.dumps({'schema': 1, 'entries': []})}
KINDS = {'A000': 'slot', 'A010': 'slot', 'B000': 'mid', 'B002': 'mid', 'B008': 'mid', 'C000': 'func', 'D00E': 'func', 'F000': 'func',
         'F010': 'func', 'F100': 'func', '6000': 'func', '9002': 'mid'}


def write_tree(case, edits=None, extra=None, kinds=None):
    d = os.path.join(BASE, case)
    if os.path.isdir(d):
        shutil.rmtree(d)
    files = dict(FILES)
    files['src/game_addresses_data.inl'] = data_inl(kinds)
    for path, fn in (edits or {}).items():
        files[path] = fn(files[path]) if callable(fn) else fn
    files.update(extra or {})
    for path, text in files.items():
        p = os.path.join(d, path)
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
    return d


class FakeReader:
    """in-memory EXE of both builds, filled from the registry rows themselves (correct bytes), then corrupted on request."""

    def __init__(self, root, corrupt=None):
        self.mem = {OLD: bytearray(0x20000), NEW: bytearray(0x20000)}
        mp = X.build_map(root)
        for s in mp.sites:
            if s['via'] != 'layout' or not s.get('original'):
                continue
            for b in (OLD, NEW):
                r, o = s['rva'].get(b), s['original']['rva'].get(b)
                if r is None or o is None:
                    continue
                if s['kind'] == 'vt':
                    self.mem[b][r:r + 8] = (H.IMAGE_BASE + o).to_bytes(8, 'little')
                elif s['kind'] == 'call':
                    self.mem[b][r] = 0xE8
                    self.mem[b][r + 1:r + 5] = ((o - (r + 5)) & 0xFFFFFFFF).to_bytes(4, 'little')
        for b in (OLD, NEW):                                   # direct CALL sites must start with E8 (none here)
            pass
        for (b, r, data) in (corrupt or []):
            self.mem[b][r:r + len(data)] = data

    def raw(self, build, rva, n):
        return bytes(self.mem[build][rva:rva + n])


def run_case(case, edits=None, extra=None, kinds=None, corrupt=None):
    root = write_tree(case, edits, extra, kinds)
    reader = FakeReader(root, corrupt) if corrupt is not None else None
    res = H.analyse(root, nexe=reader is None, reader=reader)
    return res


def groups(res, rule, sev):
    g = set()
    for f in res['findings']:
        if f['rule'] == rule and f['severity'] == sev:
            g.add((tuple(f['keys']), tuple(f['owners'])))
    return g


def other_errors(res, rule):
    return sorted({(f['rule'], f['msg'][:90]) for f in res['findings'] if f['severity'] == 'error' and f['rule'] != rule})


CASES = []


def case(rule, sev='error'):
    def deco(fn):
        CASES.append((rule, sev, fn))
        return fn
    return deco


def add_include(name):
    return lambda o: o.replace('#include "native_c.inl"', '#include "native_c.inl"\n#include "%s"' % name)


def append(path, text):
    return lambda old: old + text


# ---------------------------------------------------------------- the defects (one per rule)
@case('R1')
def c_r1():
    return run_case('r1', edits={'src/native_adapter.cpp': add_include('native_d.inl')},
                    extra={'src/native_d.inl': 'static bool prepare_d() {\n    return patch_pointer(GA(GA_A000),GA(GA_F000),x);\n}\n'})


@case('R2')
def c_r2():
    return run_case('r2', edits={'src/native_adapter.cpp': lambda o: o.replace('#include "native_c.inl"', '#include "native_c.inl"\n#include "native_d.inl"')},
                    extra={'src/native_d.inl': 'static bool prepare_d(Obj relay) {\n    if(ga::mcmp(image,GA_B002,0,k,16))return false;\n    return prepare_link(l,GA(GA_B002),0xE9,relay);\n}\n'})


@case('R3')
def c_r3():
    return run_case('r3', edits={'src/native_adapter.cpp': lambda o: o.replace('#include "native_c.inl"', '#include "native_c.inl"\n#include "native_d.inl"')},
                    extra={'src/native_d.inl': 'static bool prepare_d(Obj relay) {\n    if(ga::mcmp(image,GA_B008,0,k,16))return false;\n    return prepare_link(l,GA(GA_B008),0xE9,relay);\n}\n'})


@case('R4')
def c_r4():
    return run_case('r4', edits={'src/native_adapter.cpp': lambda o: o.replace('#include "native_c.inl"', '#include "native_c.inl"\n#include "native_d.inl"')},
                    extra={'src/native_d.inl': 'static bool prepare_d(Obj relay) {\n    if(ga::mcmp(image,GA_D00E,0,k,16))return false;\n    return prepare_link(l,GA(GA_D00E),0xE9,relay);\n}\n'})


@case('R5')
def c_r5():
    return run_case('r5', edits={'src/native_adapter.cpp': lambda o: o.replace('#include "native_c.inl"', '#include "native_c.inl"\n#include "native_d.inl"')},
                    extra={'src/native_d.inl': 'static bool prepare_d(Obj relay) {\n    if(ga::mcmp(image,GA_6000,0,k,16))return false;\n    return prepare_link(l,GA(GA_6000),0xE9,relay);\n}\n'})


@case('R6')
def c_r6():
    return run_case('r6', edits={'src/native_adapter.cpp': lambda o: o.replace('#include "native_c.inl"', '#include "native_c.inl"\n#include "native_d.inl"')},
                    extra={'src/native_d.inl': 'static bool prepare_d(Obj relay) {\n    if(ga::mcmp(image,GA_9002,0,k,16))return false;\n    return prepare_link(l,GA(GA_9002),0xE9,relay);\n}\n'})


@case('R7')
def c_r7():
    fh = json.dumps({'schema': 1, 'mods': {'m1': {'label': 'foreign', 'complete': False}},
                     'sites': [{'id': 'm1_vt_A000', 'mod': 'm1', 'kind': 'vt', 'rva': 'A000', 'len': 8}], 'relations': []})
    return run_case('r7', edits={'development/foreign_hooks.json': fh})


@case('R8')
def c_r8():
    # beta's NEED_WHITELIST row is registered from code that only the combat DLL compiles
    return run_case('r8', edits={'src/native_adapter.cpp': lambda o: o.replace('#include "native_beta.inl"', '#endif\n#ifdef SPLIT_COMBAT\n#include "native_beta.inl"\n#endif\n#ifdef SPLIT_BASE')})


@case('R9')
def c_r9():
    return run_case('r9', edits={'src/native_alpha.inl': lambda o: o.replace('layout_client::call(GA(GA_B000),reinterpret_cast<void *>(&alpha_call),&next_call)<=0)', 'false)')})


@case('R10')
def c_r10():
    return run_case('r10', edits={'src/ccz_module.cpp': '#include "game_addresses.h"\n#include "native_c_links.inl"\n'},
                    extra={'src/native_c_links.inl': C_FILE.replace('prepare_c', 'prepare_c_ccz')})


@case('R11')
def c_r11():
    return run_case('r11', edits={'src/native_layout_core.inl': lambda o: o.replace('kMaxHooks=384', 'kMaxHooks=2')})


@case('R12')
def c_r12():
    return run_case('r12', kinds={'A000': 'func'})


@case('R13')
def c_r13():
    return run_case('r13', corrupt=[(OLD, 0xA000, (0x1234).to_bytes(8, 'little'))])


@case('R14')
def c_r14():
    return run_case('r14', edits={'src/native_c.inl': lambda o: o.replace('    if(ga::mcmp(image,GA_C000,0,kExpect,16))return false;\n', '')})


# ---------------------------------------------------------------- driver
def main():
    ok = True
    total = 0

    def check(cond, msg):
        nonlocal ok, total
        total += 1
        print('%s %s' % ('PASS' if cond else 'FAIL', msg))
        if not cond:
            ok = False

    base = run_case('clean')
    nonclean = [f for f in base['findings'] if f['severity'] in ('error', 'warn')]
    check(not nonclean, 'clean synthetic tree: no error, no warning (%s)' % ([(f['rule'], f['msg'][:70]) for f in nonclean][:3] or 'none'))
    check(len([s for s in base['mp'].sites if s['dll'] != 'unreached']) >= 4, 'clean tree: sites extracted (%d)' % len(base['mp'].sites))
    for rule, sev, fn in CASES:
        res = fn()
        g = groups(res, rule, sev)
        oe = other_errors(res, rule)
        check(len(g) == 1 and (sev != 'error' or not oe), '%s fires once (%s findings group%s, other errors: %s)' % (rule, len(g), '' if len(g) == 1 else 's', oe[:2] or 'none'))
    # policy suppression: the R1 defect becomes suppressed, the entry is counted, exit status would be 0
    pol = {'schema': 1, 'entries': [{'rule': 'R1', 'sites': ['GA_A000'], 'owners': ['alpha', 'd'], 'kind': 'accepted', 'reason': 'self test', 'decided': '2026-11-06', 'evidence': 'tests/verify_hook_map.py'}]}
    res = run_case('policy', edits={'src/native_adapter.cpp': lambda o: o.replace('#include "native_c.inl"', '#include "native_c.inl"\n#include "native_d.inl"'),
                                    'development/hook_policy.json': json.dumps(pol)},
                   extra={'src/native_d.inl': 'static bool prepare_d() {\n    return patch_pointer(GA(GA_A000),GA(GA_F000),x);\n}\n'})
    errs = [f for f in res['findings'] if f['severity'] == 'error']
    sup = [f for f in res['findings'] if f['severity'] == 'suppressed']
    check(not errs and sup and not res['stale'], 'policy suppresses the R1 error (%d suppressed, %d errors, %d stale)' % (len(sup), len(errs), len(res['stale'])))
    # stale policy entry and stale hook_sites entry
    pol2 = {'schema': 1, 'entries': [{'rule': 'R2', 'sites': ['GA_F010'], 'owners': ['x'], 'kind': 'mutex', 'reason': 'self test', 'decided': '2026-11-06', 'evidence': 'tests/verify_hook_map.py'}]}
    hs = {'schema': 1, 'bindings': [{'file': 'native_none.inl', 'call': 'patch_pointer', 'arg': 'q.slot', 'table': 'nothing', 'field': 0}], 'ignore_calls': [], 'com_sites': []}
    res = run_case('stale', edits={'development/hook_policy.json': json.dumps(pol2), 'development/hook_sites.json': json.dumps(hs)})
    msgs = [f['msg'] for f in res['findings'] if f['severity'] == 'warn']
    check(any('stale policy entry' in m for m in msgs), 'stale policy entry is reported as a warning')
    check(any('hook_sites.json' in m and 'stale' in m for m in msgs), 'stale hook_sites.json entry is reported as a warning')
    check(not [f for f in res['findings'] if f['severity'] == 'error'], 'stale entries are warnings, not errors')
    # policy entry without reason is an error
    pol3 = {'schema': 1, 'entries': [{'rule': 'R2', 'sites': ['GA_F010'], 'kind': 'mutex'}]}
    res = run_case('noreason', edits={'development/hook_policy.json': json.dumps(pol3)})
    check(any(f['rule'] == 'policy' and f['severity'] == 'error' for f in res['findings']), 'policy entry without reason/decided/evidence is an error')
    # active_lines: a nested #if..#endif inside an inactive group must not re-activate the rest of that group (W0-1, 2026-11-07)
    nested = {'root.cpp': X.Src('root.cpp', '#ifdef SPLIT_COMBAT\n#if 0\n#endif\nint combat_only;\n#endif\nint always_line;\n#ifdef SPLIT_BASE\n#if 1\n#endif\nint base_line;\n#endif\n')}
    al_base = X.active_lines(nested, 'root.cpp', ('SPLIT_BASE',)).get('root.cpp', set())
    al_comb = X.active_lines(nested, 'root.cpp', ('SPLIT_COMBAT',)).get('root.cpp', set())
    check(4 not in al_base and 6 in al_base and 10 in al_base, 'active_lines: nested #if in an inactive group stays inactive (lines %s)' % sorted(al_base))
    check(4 in al_comb and 6 in al_comb and 10 not in al_comb, 'active_lines: nested #if in an active group keeps working (lines %s)' % sorted(al_comb))
    # the self test must have written only below build/development/hookmap_selftest
    check(os.path.isdir(BASE), 'synthetic trees live under build/development/hookmap_selftest')
    print('%s: %d checks' % ('hook_map self test passed' if ok else 'hook_map self test FAILED', total))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
