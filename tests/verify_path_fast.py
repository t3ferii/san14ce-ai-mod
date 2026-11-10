import sys as _s, os as _o; _s.path.insert(0, _o.path.dirname(_o.path.abspath(__file__))); import ga_expand  # user 2026-11-05 patch resilience: sources spell addresses as GA(GA_x)
"""Offline path_fast evidence. No process access, game execution or live writes.

Steam's on-disk text is encrypted: check its identity, then the matching decoded
image and retained runtime capture. This is explicitly not fresh runtime proof.
2026-10-25 user/Claude: 183A30 per-segment replacement; 184485 clear elision except N=1; N>1 clean sampled comparisons.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import sys

P = Path(__file__).resolve().parents[1]
BASE = 0x140000000
GHIDRA = P / 'research/ghidra_full/1a4a447d14e68c40'   # function table of the current build (replaces the former decomp_cache listings)
pass  # Public snapshot: install capstone in the active Python environment.
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

decoded = (P / 'build/ui_native_analysis/editor83_b25953386/game.exe.unpacked.exe').read_bytes()
capture = (P / 'build/ui_native_analysis/fixture/1a4a447d_text.bin').read_bytes()
checks = 0
failures = []


def ok(condition, label):
    global checks
    checks += 1
    if not condition:
        failures.append(label)


def sections():
    pe = struct.unpack_from('<I', decoded, 60)[0]
    table = pe + 24 + struct.unpack_from('<H', decoded, pe + 20)[0]
    for i in range(struct.unpack_from('<H', decoded, pe + 6)[0]):
        at = table + i * 40
        yield decoded[at:at + 8].rstrip(b'\0'), *struct.unpack_from('<IIII', decoded, at + 8)


def read(at, size):
    for _, _, va, rs, ro in sections():
        if va <= at and at + size <= va + rs:
            return decoded[ro + at - va:ro + at - va + size]
    raise ValueError(hex(at))


def fnv(data):
    value = 0xcbf29ce484222325
    for b in data:
        value = ((value ^ b) * 0x100000001b3) & 0xffffffffffffffff
    return value


FUNCTIONS = {}
for _row in (GHIDRA / 'functions.tsv').read_text(encoding='utf-8').splitlines()[1:]:
    _f = _row.split('\t')
    FUNCTIONS[int(_f[0], 16)] = int(_f[1], 16) - int(_f[0], 16) + 1   # inclusive end: the size column omits one byte of some bodies


def function_size(address):
    return FUNCTIONS[address]


def norm(text):
    return re.sub(r'0x[0-9a-f]+|\b[0-9]+\b', lambda m: hex(int(m.group(0), 0)), text.lower().replace(' ', ''))


def cached(address):
    """address -> normalized 'mnemonic operands' of every instruction of the current-build function (capstone; was the decomp_cache asm text)."""
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    return {a: norm(op + ' ' + args) for a, _, op, args in md.disasm_lite(read(address, function_size(address)), address)}


def has(listing, address, text):
    return listing.get(address) == norm(text)


def audit():
    # local10 (2026-11-06): the on-disk Steam EXE is now buildid 25953386 (782cf229...); 87ed4e46... is the build the decoded evidence below belongs to.
    ok(hashlib.sha256((P.parent / 'SAN14CE_KO.exe').read_bytes()).hexdigest() in
       ('87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a',
        '782cf229a56cdde2b95399e0e7536740b3f5e96bea702679365e4dad061abdce'), 'Steam identity')
    ok(hashlib.sha256(decoded).hexdigest() ==
       '1a4a447d14e68c40ad2a5107c29c7341c10eee8fc297a7b35032900c341dbcaa', 'decoded identity')
    # Audit all .text address operands, not merely decompiler cross references.
    # At this build the only address producers in the contiguous lock/result/node
    # allocation are two lock lifecycle functions and the three path wrappers.
    # MOV [rip+1BB3D00] loads a heap singleton value, NOT &1BB3D00; it cannot
    # designate the neighbouring static arrays. No LEA of that base exists.
    low, high = 0x1bb6d50, 0x1bbab50 + 3 * 0x1d8a80
    producers, singleton = [], 0
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.skipdata = True
    callsites = []
    constant_one = []
    pending_count = None
    for name, _, va, rs, ro in sections():
        if name != b'.text':
            continue
        for addr, size, op, args in md.disasm_lite(decoded[ro:ro + rs], BASE + va):
            site = addr - BASE
            # Conservative fifth-argument classification: only a direct immediate
            # store after the preceding CALL/stack adjustment is conclusive here.
            if op == 'mov' and args.startswith('dword ptr [rsp + 0x20], '):
                value = args.rsplit(', ',1)[1]
                pending_count = int(value,0) if re.fullmatch(r'(0x[0-9a-f]+|[0-9]+)',value) else None
            if op == 'call' and args == '0x140185710':
                callsites.append(site)
                if pending_count == 1:
                    constant_one.append(site)
            if op == 'call' or (op in ('add','sub') and args.startswith('rsp,')) or op in ('ret','int3'):
                pending_count = None
            if 'rip ' in args:
                disp = int(args.split('rip ', 1)[1].split(']')[0].replace(' ', ''), 16)
                target = site + size + disp
                if low <= target < high:
                    if target == low and op in ('mov', 'cmp'):
                        singleton += 1
                    else:
                        producers.append(site)
            elif '[' in args:
                # Include indexed module-base+RVA addressing, including nearby
                # lock and result bases, as well as the node base itself.
                memory = args.split('[', 1)[1].split(']', 1)[0]
                if any(low <= int(x, 16) < high or BASE + low <= int(x, 16) < BASE + high
                       for x in re.findall(r'0x[0-9a-f]+', memory)):
                    producers.append(site)
            # Absolute immediate image addresses would introduce another root.
            elif any(BASE + low <= int(x, 16) < BASE + high
                     for x in re.findall(r'0x[0-9a-f]+', args)):
                producers.append(site)
    # Linear decoding also covers embedded jump-table DWORDs. Check these
    # exact candidates against their native indexed DWORD table loads rather
    # than treating enclosing function/pdata extents as instruction boundaries.
    # (Some functions have noncontiguous bodies.) The two pool bounds are RCX+98h
    # heap pool values plus a 32MiB SIZE, never image-base-relative addresses.
    # 2026-11-07 (1a4a447d): same five path functions at the same offsets, pool bounds in the allocator, and nine jump-table DWORD
    # candidates (the former build had eight; each is a code RVA consumed by the switch load listed below).
    pool_bounds = [0x14d815, 0x14d96b]
    table_data = [0x1f294b, 0x1f2987, 0x3f2a37, 0x4c9cc6, 0x4c9f16,
                  0x4d43e9, 0x93dfc5, 0xa637cf, 0xd5c5ff]
    expected = [0x14afd5, 0x1544ba, 0x1544c1,
                0x185746, 0x185781, 0x18579e, 0x185878,
                0x186059, 0x18609c, 0x1860b9, 0x1861a0,
                0x186989, 0x1869cc, 0x1869e9, 0x186ad0]
    ok(producers == sorted(expected + pool_bounds + table_data),
       'exhaustive text address candidates: ' + str([hex(x) for x in producers]))
    for site in pool_bounds:
        ok(read(site, 7) == bytes.fromhex('488d8200000002') and
           read(site - 28, 60).find(bytes.fromhex('488b9198000000')) >= 0,
           f'{site:X} allocator pool-end comparison based on [RCX+98h]')
    table_loads = [(0x1f2766,'8b8c8244291f00'), (0x1f2766,'8b8c8244291f00'),
                   (0x3f2603,'418b948194293f00'), (0x4c9a7b,'8b8c82bc9c4c00'),
                   (0x4c9dbd,'8b8c820c9f4c00'), (0x4d361a,'8b8c82c0434d00'),
                   (0x93dcc7,'418b8c8094df9300'), (0xa62e86,'418b8c80c437a600'),
                   (0xd5aeed,'8b8c8724c5d500')]
    for site, (load, hex_bytes) in zip(table_data, table_loads):
        expected_bytes = bytes.fromhex(hex_bytes)
        table = struct.unpack('<I', expected_bytes[-4:])[0]
        # These bytes are parts of consecutive 32-bit code RVAs, not an x86
        # ADD using a node-buffer displacement. Require the actual switch load.
        ok(read(load,len(expected_bytes)) == expected_bytes and table <= site and
           0x1000 <= struct.unpack('<I',read(site & ~3,4))[0] < 0x1342000,
           f'{site:X} DWORD switch data consumed by native load {load:X}')
    ok(singleton == 327, 'singleton value references classified separately')
    # Scan every DIR64 relocation, including tables outside .text. Unaligned raw
    # byte coincidences are not pointers (e.g. CMP byte [RBX+141h],0; JE ...).
    pe = struct.unpack_from('<I', decoded, 60)[0]
    rva, size = struct.unpack_from('<II', decoded, pe + 24 + 112 + 5 * 8)
    blob = read(rva, size)
    at, pointer_roots, relocations = 0, [], 0
    while at + 8 <= len(blob):
        page, length = struct.unpack_from('<II', blob, at)
        if length < 8:
            break
        for offset in range(at + 8, at + length, 2):
            entry = struct.unpack_from('<H', blob, offset)[0]
            if entry >> 12 == 10:
                relocations += 1
                target = struct.unpack('<Q', read(page + (entry & 4095), 8))[0]
                if BASE + low <= target < BASE + high:
                    pointer_roots.append((page + (entry & 4095), target))
        at += length
    ok(not pointer_roots, 'no relocated array pointers or aliases in image tables')
    # Lifecycle: 1534D0 iterates exactly three lock pointer slots, stopping at
    # the result-array address. It never dereferences the end sentinel.
    init, destroy = cached(0x1544b0), cached(0x14afc0)
    ok(has(init, 0x1544e3, 'cmp rbx,rdi') and has(init, 0x1544e6, 'jl 0x1544d0'),
       '1544B0 result base is only the exclusive lock-array end sentinel')
    ok(has(destroy, 0x14afe1, 'mov ebp,3') and has(destroy, 0x14b036, 'add rsi,8'),
       '14AFC0 only destroys three locks')
    # All three buffer roots are killed by a complete clear before any search;
    # returned node pointers are consumed locally into DWORD result arrays and
    # never returned/stored globally. 183A30 passes node pointers only to local
    # open-list helpers 18AFA0/18AE60, not to the game-world cost helpers.
    for entry in (0x185710, 0x186020, 0x186950):
        asm = cached(entry)
        ok(norm('mov ecx,0x1d8a80') in asm.values() and sum(v.startswith('repstosb') for v in asm.values()) == 2,
           f'{entry:X} clears result and full nodes before search')
        size = function_size(entry)
        ok(read(entry, size) == capture[entry - 0x1000:entry - 0x1000 + size],
           f'{entry:X} complete decoded/captured agreement')
    setup = cached(0x18c330)
    ok(has(setup, 0x18c405, 'mov dword ptr [rbx + 0x24],eax') and
       has(setup, 0x18c408, 'mov byte ptr [rbx + 0x1c],1') and
       has(setup, 0x18c414, 'mov qword ptr [r14],rbx'),
       'single clean goal seeds one empty-root tree, g/parent/children stay zero')
    ok(cached(0x185710).get(0x1857b5, '').startswith('repstosb') and
       has(cached(0x185710), 0x1857ed, 'mov dword ptr [rsp + 0x30],ebp'),
       'first search follows full clear; seventh original search argument flags')
    for site,code,label in [
        (0x1857B5,'f3aa','only node REP STOSB is patched'),
        (0x185790,'f3aa','result clear remains original'),
        (0x1857BE,'4d85e4','flags killed by TEST R12,R12'),
        (0x1857F1,'498bcd','RCX killed before first call'),
        (0x1857E3,'418b04b6','RAX killed by waypoint load'),
        (0x185833,'48637b08','RDI killed before chain destination index'),
        (0x18581F,'488b4010','later segments skip first chain node'),
        (0x185823,'8b4820014b0c','cost from consumed node +20'),
        (0x185830,'8b4818','cell from consumed node +18'),
        (0x185840,'488b4010','follow chain +10')]:
        want=bytes.fromhex(code)
        ok(read(site,len(want))==want and capture[site-0x1000:site-0x1000+len(want)]==want,label)
    # Enumerate every actual instruction between REP and the first original
    # search call. No skipped-register read occurs before its defining write.
    md=Cs(CS_ARCH_X86,CS_MODE_64)
    sequence=list(md.disasm_lite(read(0x1857B7,0x185810-0x1857B7),0x1857B7))
    ok([hex(a) for a,_,op,args in sequence if re.search(r'\b(rdi|edi|di|dil)\b',args)]==[],
       'RDI not read at all before first search call (nonvolatile restored or overwritten later)')
    ok([hex(a) for a,_,op,args in sequence if re.search(r'\b(rcx|ecx|cx|cl)\b',args)]==['0x1857f1'],
       'RCX only written before first search call; empty-path branch writes RCX or never reads it')
    ok([hex(a) for a,_,op,args in sequence if re.search(r'\b(rax|eax|ax|al)\b',args)]==['0x1857e3','0x185809'],
       'RAX defined before its only subsequent read (empty-path branch defines EAX before return)')
    return dict(address_roots=[hex(x) for x in producers], singleton_value_refs=singleton,
                dir64_relocations=relocations, embedded_array_pointers=pointer_roots,
                caller_count=len(callsites), direct_immediate_one=len(constant_one),
                caller_classification='conservative direct immediate stores only; not runtime frequency',
                callers=[hex(x) for x in callsites], gameplay_verified=False)


def implementation():
    native = (P/'src/native_path_fast.inl').read_text(encoding='utf-8')
    core = (P/'src/path_fast.h').read_text(encoding='utf-8')
    guards = re.findall(r'\{0x([0-9A-F]+),0x([0-9A-F]+),0x([0-9a-f]+)ull\}',native)
    ok(len(guards)==37,'37 fingerprints: 34 complete functions plus 3 entry-excluded ranges')
    tails={0x185720:0x185710,0x2ACB90:0x2ACB80,0x2B255F:0x2B2550}
    for address,size,h in guards:
        a,n,value=int(address,16),int(size,16),int(h,16)
        owner=tails.get(a,a)
        ok(n==function_size(owner)-(a-owner) and fnv(read(a,n))==value,'complete native or entry-excluded fingerprint '+address)
        ok(read(a,n)==capture[a-0x1000:a-0x1000+n],'decoded/captured helper bytes '+address)
    ok(not any(int(a,16) in (0x185710,0x2ACB80,0x2B2550) for a,_,_ in guards),
       'other owners entry 16-byte publication blocks excluded')
    sortie=(P/'src/native_sortie_links.inl').read_text(encoding='utf-8')
    # 사용자 결정 2026-10-30: 통합패치·발탁장수 공존을 위해 첫머리 훅을 호출 자리로 이동
    patches=[(0x1EDC30,16,'dispatch entry')]
    normalized={0x184FF9,0x18793A}
    for name in ('sortie_plan_sites','sortie_search_sites','sortie_movement_sites'):
        body=re.search(name+r'\[\]=\{(.*?)\};',sortie,re.S).group(1)
        for raw in re.findall(r'0x([0-9A-F]+)',body):
            site=int(raw,16)
            if site not in normalized:patches.append((site,5,'sortie caller '+raw))
    ok('movement_calls[]={0x184FF9,0x18793A}' in native and 'movement_call_match(site)' in native,
       'both overlapping movement CALLs validate native or external next before hash normalization')
    import registry_text as R
    split=R.split_install_text()   # W5: install code plus the generated SplitPointer rows
    combat=split.split('static const SplitPointer split_pointers[]={',1)[1].split('#else',1)[0]
    patches += [(int(a,16),8,'combat pointer '+a) for a in re.findall(r'\{0x([0-9A-F]+),0x',combat)]
    patches += [(0x2ACB80,16,'progress distance')]
    ok(len(patches)==82,'2026-10-30 reviewed graph: dispatch block, 73 disjoint sortie callers, 7 combat pointer slots and progress distance block enumerated')
    for address,size,_ in guards:
        a,n=int(address,16),int(size,16)
        ok(all(a+n<=b or b+size<=a for b,size,_ in patches),'fingerprint excludes all existing owner patches '+address)
    ok(all(0x1857B0+16<=b or b+size<=0x1857B0 for b,size,_ in patches),
       'clear-elision whole atomic publication block is disjoint from combat/progress patches')
    ok(read(0x184D60,7)==bytes.fromhex('488bc444894820'),'seven-byte complete nonrelative home-space MOV prologue')
    ok(read(0x185098,7)==bytes.fromhex('833d35fafb0100'),'debug global RIP address 142141A84')
    # the source spells the entry as GA(GA_184D60) (expanded to the old-build RVA by ga_expand); const since the 2026-11-05 table change
    ok(any(f'static {q} unsigned entry=0x184D60,debug_rva=0x2144AD4;' in native for q in ('constexpr','const')),'sole convenience entry is 183A30')
    ok('count!=1 || !buffer || clear_bytes!=0xBD10' in native and
       'original(ctx,start,goals,count,buffer,clear_bytes,flags)' in native,'seven original arguments and mandatory delegation')
    ok('dword(image,debug_rva)' in native and 'busy)' in native,'debug and recursion delegate')
    ok('RtlAddFunctionTable' in native and 'const unsigned char unwind[]={1,7,0,0}' in native,'registered leaf unwind')
    ok(native.count('COST_SCOPE("path_fast")')==1,'one cost key')
    keys=set()
    for f in (P/'src').iterdir():
        if f.suffix in ('.inl','.h','.cpp'):
            keys.update(re.findall(r'COST_SCOPE\("([^"]+)"\)',f.read_text(encoding='utf-8-sig')))
    maximum=int(re.search(r'maximum=(\d+)',(P/'src/native_cost_profile.h').read_text(encoding='utf-8')).group(1))
    # 사용자 결정 2026-10-30: 통합패치·발탁장수 공존을 위해 첫머리 훅을 호출 자리로 이동; 현행 native_cost_profile.h의 1024 한도 보존
    ok(len(keys)<=maximum and maximum==1024,'current 1024-key limit retained; all cost keys fit')
    # User 2026-10-25 changes verification from boolean to global Nth sampling.
    for token in ['path_fast_day','path_fast_verify_day','path_fast_mismatch','totals.lines<64',
                  'path_fast_time_day','totals.only_fast,microseconds(totals.only_ticks),totals.all_compared',
                  'if(fast && !compared)','if(compared)++totals.all_compared',
                  'totals.details<16','++totals.details','totals={};totals.day=today',
                  'record(false,false,false,false,call)','record(true,compared,mismatch,dirty,call',
                  'if(verify==1)dirty=!path_fast_core::clean(buffer)','if(compared && !dirty)',
                  'if(mismatch && dirty)++totals.multi','InterlockedIncrement64(&calls)',
                  'path_fast_core::compare(output,actual)','detail.valid=native.valid;detail.force=native.force']:
        ok(token in native,'sampled diagnostics/timing: '+token)
    verify_body=native.split('        if(path_fast_core::verify_call(verify,call)) {',1)[1].split('        } else',1)[0]
    ok(verify_body.index('memset(buffer,0,path_fast_core::node_bytes)')<verify_body.index('output=original(')<
       verify_body.index('path_fast_core::search(')<verify_body.index('path_fast_core::compare(')<
       verify_body.index('path_fast_core::verified_result('),
       'sample order: full clean, original in native buffer, private new, compare, select return')
    ok('mod_setting(L"diagnostics",L"path_fast_verify",0)' in native and
       'verify=interval>0?static_cast<unsigned>(interval):0' in native and '#ifndef NO_RUNTIME_LOG' in native,
       'diagnostic integer interval default0, nonpositive disabled')
    ok('ordinal%interval==0' in core and 'return interval!=1' in core and
       'interval>1 && !mismatch?materialize(buffer,actual):original' in core,
       'N=1 original identity; sampled match publishes new chain, mismatch retains original')
    ok('path_fast_core::materialize(buffer,result)' in native and 'buffer[node->cell]=value' in core and
       'generations[i]!=generation' in core,'publish only full chain records from clean generation storage')
    # User report 2026-10-26: per-thread `new Workspace` leaked ~2.9MB per force-AI thread (65.9GB crash).
    ok('new Workspace' not in native and 'thread_local Workspace' not in native and
       'slot_storage[3]' in native and 'acquire_storage(buffer)' in native and
       'if(slot<0) {storage=nullptr;return false;}' in native,'storage bounded to the 3 locked native slots, no per-thread allocation')
    ok('clear_delegate(buffer,known_slot(buffer),skip_clear!=0)' in native and
       'if(known_slot && skipped_clear)std::memset(buffer,0,node_bytes)' in core,'delegation restores full known-slot clear only after elision')
    ok('prepare_bytes(clear_link,0x1857B5,nop,sizeof(nop))' in native and
       'if(path_fast_core::elide_clear(verify))' in native and 'publish_link(clear_link)' in native and
       'clear_bytes_expected[]={0xF3,0xAA}' in native,'guarded two-byte clear elision active for 0 and N>1')
    for token in ['path_fast_mm','path_fast_mm_flags','path_fast_mm_ctx','path_fast_mm_cost',
                  'path_fast_mm_diff','path_fast_mm_orig','path_fast_mm_new','path_fast_mm_nb',
                  'index<64','direction<=6','storage.generations[id]==storage.generation',
                  'original->g!=actual->g','difference.original_length<<16','n.g)&0xFFFFFF']:
        ok(token in core,'bounded mismatch detail: '+token)
    diag=core.split('inline bool verify_call',1)[1].split('template<class S>',1)[0]
    ok('terrain(' not in diag and 'adjust(' not in diag and 'touch(' not in diag,
       'details only read existing nodes, never call cost helpers or mutate generations')
    pure=cached(0x2b11b0)
    ok(not any(v.startswith('call') for v in pure.values()) and norm('mov dword ptr [r9],ecx') in pure.values() and norm('mov dword ptr [r8],edx') in pure.values(),
       '2AEE80 only writes supplied coordinate outputs, no global/helper side effects')
    main=cached(0x184d60)
    for address, token in [(0x185277,'mov r13d,1'),(0x185280,'mov edx,r13d'),(0x185613,'inc r13d'),
                           (0x185616,'cmp r13d,7'),(0x185572,'cmp dword ptr [rax + 0x24],ebx'),
                           (0x185575,'jle 0x185584'),(0x185584,'mov rcx,qword ptr [rax + 8]'),
                           (0x1855e3,'jle 0x1855f2'),(0x1855f2,'mov rcx,qword ptr [rax + 8]'),
                           (0x185528,'call 0x18c1f0'),(0x185650,'or byte ptr [rax + 0x1c],4')]:
        ok(has(main, address, token),'original direction / reinsertion / state timing: %X %s' % (address, token))
    ok(list(struct.unpack('<7I',read(0x2a9a58,28)))==
       [0x2a9973,0x2a997f,0x2a99a6,0x2a99be,0x2a99e5,0x2a9a15,0x2a9a2d],
       '2A9910 actual switch maps direction values 0..6 to decoded geometry cases (same case offsets as the former 2A7720)')
    state_access=[(a,v) for a,v in main.items() if re.search(r'\+0x1c\]',v)]
    ok(not any('0x4' in v.split(',')[-1:] [0] or v.endswith(',0x4') for a,v in state_access if a!=0x185650),
       'path bit4 is written only at success; no later bit4 decision in 184D60')
    ok('destination' not in native and 'Result *out' not in native and '0x1BB3D20' not in native,
       'no 1843E0 hook or result-buffer writes remain')
    movement=(P/'src/native_sortie.inl').read_text(encoding='utf-8').split('static float sortie_movement_next',1)[1].split('static void sortie_move',1)[0]
    ok('native=next(army,formation,preview,modifier,tile)' in movement and
       'return slowest>0?slowest:native;' in movement,
       '사용자 결정 2026-10-30: 통합패치·발탁장수 공존을 위해 첫머리 훅을 호출 자리로 이동; movement uses site next and common wrapper')
    progress=(P/'src/native_progress.inl').read_text(encoding='utf-8')
    ok('memcpy(relay,progress_kernel_bytes' in progress and 'original_progress_distance=' in progress,
       'progress valid-cell distance is equivalent kernel; fallback original gate')
    adapter=(P/'src/native_adapter.cpp').read_text(encoding='utf-8-sig')
    ok('f.path_fast = fast && feature_key(config, L"performance", L"path_fast", false);' in adapter,
       'performance parent AND, code default0')
    split=(P/'src/native_split_install.inl').read_text(encoding='utf-8')
    ok('if(f.path_fast && !path_fast_guards_match())split_disable(f.path_fast,23);' in split,
       'independent compatibility disable')
    import registry_text as R
    ok('path_fast::install()' in R.row('path_fast') and R.index('path_fast')<R.index('path_count') and
       'if(path_fast::active)log_event("path_count_skipped_path_fast");' in split and 'else path_count::install(' in split,
       'path_count skips installation only after path_fast successfully owns entry')
    ok('path-fast' in json.loads((P/'development/features.json').read_text(encoding='utf-8-sig'))['modules']['path-fast']['tests'],
       'focused route registered')
    for rel in ('installer/InstallerGui.cs','distribution/FeatureSelect.ps1','FeatureSelect.ps1'):
        text=(P/rel).read_text(encoding='utf-8-sig')
        one=[s for s in text.splitlines() if ('"path_fast",' if rel.endswith('.cs') else "Key='path_fast';") in s]
        v=[s for s in text.splitlines() if ('"path_fast_verify",' if rel.endswith('.cs') else "Key='path_fast_verify';") in s]
        ok(len(one)==1 and '경로 탐색 가속' in one[0] and '경유지가 여럿인 경로' in one[0] and
           ('"path_fast", 1' in one[0] if rel.endswith('.cs') else 'Default=1' in one[0]),rel+' fast catalog default1')
        ok(len(v)==1 and '경로 탐색 가속 결과 비교' in v[0] and '0=끔, 1=매번, N=N번에 한 번' in v[0] and
           ('"기타(개발용)", "diagnostics", "diagnostics"' in v[0] if rel.endswith('.cs') else "Group='diagnostics'" in v[0]),rel+' verify diagnostic group')
    ok('f.Id == "diagnostics.path_fast_verify" ? Math.Max(0, n)' in (P/'installer/InstallerGui.cs').read_text(encoding='utf-8-sig'),
       'GUI preserves integer verification interval on save')
    for rel in ('distribution/FeatureSelect.ps1','FeatureSelect.ps1'):
        ok("$taskItem.Key -eq 'path_fast_verify'" in (P/rel).read_text(encoding='utf-8-sig') and
           '[Math]::Max(0, $taskNumber)' in (P/rel).read_text(encoding='utf-8-sig'),rel+' preserves interval during manager roundtrip')
    return dict(cost_keys=len(keys),cost_limit=maximum,guards=len(guards))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--audit-only', action='store_true')
    args = parser.parse_args()
    evidence = audit()
    if not args.audit_only:
        evidence.update(implementation())
    print(json.dumps(dict(status='path_fast_audit_verified' if not failures else 'failed',
                         checks=checks, failures=failures, evidence=evidence), ensure_ascii=False))
    return bool(failures)


if __name__ == '__main__':
    sys.exit(main())
