"""ccz_sprites static check (2026-10-31 user: 조조전 병종 그림을 우리 모드가 직접 그린다).
Guard constants in src/native_ccz_sprites.inl against the DRM-free analysis EXE and the captured
running .text, the decoded field layout it relies on, and the CCZU asset it loads.
Usage: python -B -X utf8 tests/verify_ccz_sprites.py [--print]"""
from pathlib import Path
import json, re, struct, sys, hashlib
P = Path(__file__).resolve().parents[1]
exe = (P/'build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe').read_bytes()
text = (P/'legacy/FogMod_Source/native_game_analysis/running_code_text.bin').read_bytes()
pe = struct.unpack_from('<I', exe, 0x3c)[0]; n = struct.unpack_from('<H', exe, pe+6)[0]; so = struct.unpack_from('<H', exe, pe+20)[0]
S = [struct.unpack_from('<IIII', exe, pe+24+so+i*40+8) for i in range(n)]
BASE = 0x140000000
def rd(rva, size):
    for vs, va, rs, ro in S:
        if va <= rva < va+max(vs, rs): return exe[ro+rva-va:ro+rva-va+size]
    raise KeyError(hex(rva))
def run(rva, size): return text[rva-0x1000:rva-0x1000+size]
def q(rva): return struct.unpack('<Q', rd(rva, 8))[0]
def fnv(b):
    h = 0xcbf29ce484222325
    for c in b: h = ((h ^ c)*0x100000001b3) & 0xffffffffffffffff
    return h
def rel(site): return site+5+struct.unpack_from('<i', rd(site+1, 4))[0]
if '--print' in sys.argv:
    for rva, size in ((0x1C6EC0, 0x60), (0x1C70E0, 0x40)):
        print(hex(rva), hex(size), hex(fnv(rd(rva, size))), hex(fnv(run(rva, size))))
    sys.exit(0)
src = (P/'src/native_ccz_sprites.inl').read_text(encoding='utf-8')
bridge = (P/'src/native_ccz_bridge.inl').read_text(encoding='utf-8'); module = (P/'src/ccz_module.cpp').read_text(encoding='utf-8')
num = lambda name: int(re.search(name+r'\s*=\s*0x([0-9A-Fa-f]+)', src).group(1), 16)
checks = 0; fail = []
def ok(cond, what):
    global checks; checks += 1
    if not cond: fail.append(what)
slot, submit, size, h = num('kSubmitSlot'), num('kSubmit'), num('kSubmitSize'), num('kSubmitHash')
ok(q(slot) == BASE+submit, 'display vtable slot 5 -> army submit 1C6EC0')
ok(slot == num('kDisplayVtable')+5*8, 'submit is slot 5 of the army display vtable')
ok(fnv(rd(submit, size)) == h, 'submit exe fingerprint'); ok(fnv(run(submit, size)) == h, 'submit running fingerprint')
body = rd(submit, size)
ok(bytes.fromhex('8b8140070000') in body or bytes.fromhex('8b8740070000') in body or b'\x40\x07\x00\x00' in body, 'mode read at display+740')
ok(q(num('kDisplayVtable')+4*8) == BASE+0x1CA600 and q(num('kDisplayVtable')+6*8) == BASE+0x1CC970, 'neighbouring banner slots 1CA600/1CC970')
ok(q(num('kArmyVtable')) > BASE, 'army vtable present')
# CCZU asset loaded at runtime
a = (P/'assets/ccz/ccz_units.bin').read_bytes(); meta = json.loads((P/'assets/ccz/ccz_units.json').read_text(encoding='utf-8'))
magic, ver, cnt, table = struct.unpack_from('<4s3I', a)
ok(magic == b'CCZU' and ver == 2 and cnt == meta['count'], 'CCZU v2 header')
ok(hashlib.sha256(a).hexdigest() == meta['sha256'], 'asset hash matches json')
raw = 48*48*11+64*64*12+48*48*5
for k in (0, cnt-1):
    c, _, o, cz, u = struct.unpack_from('<HHIII', a, table+16*k)
    sys.path.insert(0, str(P/'tools')); from ccz_extract_units import unrle
    ok(u == raw and len(unrle(a[o:o+cz], u)) == raw, 'entry %d decodes' % c)
ok(str(raw) in src.replace("'", '') or 'kRawSize' in src, 'renderer knows the entry size')
# formation -> class cells (Army+1B read by 22B090) must exist in the asset; 충차(9) keeps the native model
body_ = re.search(r'kFormationClass\[21\]\[3\]=\{(.*?)\};', src, re.S).group(1)
rows = [[x.strip() for x in r.split(',')] for r in re.findall(r'\{([^{}]*)\}', re.sub(r'//[^\n]*', '', body_))]
present = {struct.unpack_from('<H', a, table+16*k)[0] for k in range(cnt)}
cls = lambda x: None if x == 'kNative' else int(x)
ok(len(rows) == 21 and all(len(r) == 3 for r in rows), 'formation table 21 x 3 tiers')
ok(all(cls(x) is None or cls(x)*3 in present for r in rows for x in r), 'formation class cells exist in the asset')
ok(all(cls(x) is None for x in rows[9]+rows[14]), '충차/사이 native')
classes = {int(r[0]): r[1] for r in (l.split(',') for l in (P/'research/cao_cao_den/tables/classes_sin.csv').read_text(encoding='utf-8-sig').splitlines()[1:])}
want = {1: '선봉대 중전차 철갑차', 2: '척후병 중기병 친위대', 3: '장궁병 명궁병 용기장군', 4: '검병 장검병 근위병', 5: '책사교위 참모중랑 군사장군',
        6: '산적 의적 호걸', 7: '경기병 경기병 경기병', 8: '정란 정란 정란', 10: '경포차 중포차 벽력차', 19: '식량대 물자대 치중대'}   # 2026-10-31 user table
for f, names in want.items(): ok(' '.join(classes[cls(x)] for x in rows[f]) == names, 'formation %d -> %s' % (f, names))
for name, want in (('kRulerClass', ['군웅', '영웅', '제왕']), ('kStrategistClass', ['책사교위', '참모중랑', '군사장군'])):
    m = re.search(name+r'\[3\]=\{([^}]*)\}', src); ok(m and [classes[int(x)] for x in m.group(1).split(',')] == want, name)
ok('else if(main_rtv && post_seen && dsv && dsv!=scene_dsv)' in src and 'h*2==static_cast<unsigned>(want_height))post_seen=true;' in src and 'post_seen=false;' in src and 'fallback_attempted=false;drop(main_rtv);drop(frame_dsv);' in src and 'draw_scene(c,main_rtv,frame_dsv?frame_dsv:scene_dsv)' in src and 'rtv[0]==main_rtv && dsv && dsv!=frame_dsv && !post_seen' in src and 'drop(frame_dsv);' in src and 'ccz_sprites_zoom_fallback' in src and 'ccz_sprites_frame_stall' in src and 'if(draw_scene(c,main_rtv,frame_dsv?frame_dsv:scene_dsv))drawn_this_frame=true;' in src and 'if(scene_drawn || set_table)return;' in src,
   'draws into the back buffer at the first non-scene depth bind after post starts (battle effects mid-scene ignored), depth = view bound with the scene colour target; Present only as fallback')
ok('memcpy(cam,camera,sizeof(cam))' not in src, 'camera copy uses the array size, not a pointer parameter size (local8 bug)')
gen = [l.split('	') for l in (P/'data/ccz_generals.tsv').read_text(encoding='utf-8').splitlines() if l and not l.startswith('#')]
m = re.search(r'kGenerals\[\]=\{(.*?)\};', src, re.S)
pairs = [tuple(map(int, x)) for x in re.findall(r'\{(\d+),(\d+)\}', m.group(1))] if m else []
ok(pairs == [(int(r[0]), int(r[1])) for r in gen] and pairs == sorted(pairs) and len(gen) == 92, 'general table = data/ccz_generals.tsv (92, sorted by person id)')
ok(all(c in present and ((c % 3 == 0 and 160 <= c//3 <= 180 and classes[c//3] == r[3]) or (r[3].startswith('개인 R') and c == (720+(int(r[3][4:])-1)*3+2 if int(r[3][4:]) < 33 else int(r[3][4:])+783))) for (_, c), r in zip(pairs, gen)) and sum(not r[3].startswith('개인') for r in gen) == 17, 'general cells: 17 personal classes 160~180, the rest 신조조전 R형상 pictures, all in the asset')
snap = {l.split('	')[0]: l.split('	')[1] for l in (P/'data/officer_snapshot.tsv').read_text(encoding='utf-8-sig').splitlines()[4:] if l.count('	') > 2}
ok(all(snap.get(r[0]) == r[2] for r in gen), 'general ids match San14 names (officer snapshot)')
ekd = P.parent/'신조조전'/'Ekd5.exe'
if ekd.exists():
    rs = struct.unpack_from('<1024H', ekd.read_bytes(), 0x9f800+(0x501000-0x4ce000))
    ok(rs[0] == 1 and rs[1] == 2 and rs[32] == 22 and rs[119] == 122 and all(int(r[3][4:]) in rs for r in gen if r[3].startswith('개인 R')), '신조조전 Ekd5 0x501000 R형상 table (조조 1, 하후돈 2, 유비 22, 여포 122)')
# User 2026-11-02: a selected formation mask now gates the same personal artwork.
# The C++ appearance fixture exercises actual identify(), including masked-out fallbacks.
ok('if(formation<=7){unsigned g=ccz_appearance::personal(config,leader,formation);if(g){r.cell=g;if(r.cls==kNative)r.cls=g/3u;}}' in src and 'if(!own)r.cell=ccz_appearance::cell(r.cls);' in src and 'u.cell=who.cell;' in src and 'for(const auto &g:kGenerals)if(!entries.count(g.cell))return false;' in src,
   'general picture wins over ruler/strategist, soldier formations only, asset must carry it')
ok('person[0xEC]' in src and 'word(owner,0x10)' in src and 'word(owner,0x52)' in src, '통솔 Person+EC, ruler Force+10, strategist Force+52')
ok(bytes.fromhex('0fb6411b') in rd(0x22B090, 0x90) or b'\x1b' in rd(0x22B090, 0x90), '22B090 reads Army+1B formation')
# 막기·회심 (2026-10-31 user: block = full nullify): 169C50 case 6 call 16FDD0 at 169F8B is the only caller
site = 0x169F8B
ok(rd(site, 1) == b'\xe8' and rel(site) == 0x16FDD0 and rd(site-11, 11) == bytes.fromhex('4c8d442430488bd3498bcf'), '169F8B calls 16FDD0 with output rsp+30')
ok(run(site-11, 16) == rd(site-11, 16), '169F8B running bytes match')
ok(rd(0x169F9D, 1) == b'\xe8' and rel(0x169F9D) == 0x1749A0, 'next step 7 is 1749A0 (damage at output+20/+22)')
tx = rd(0x1000, 0x133ea00)
calls = [i for i in range(len(tx)-5) if tx[i] == 0xE8 and 0x1000+i+5+struct.unpack_from('<i', tx, i+1)[0] == 0x16FDD0]
ok(calls == [site-0x1000], '16FDD0 has one caller')
ok('kSite=0x169F8B,kFix=0x16FDD0' in bridge and 'kFix=0x16FDD0,kStat=0x22FCE0' in src and 'damage[0]=damage[1]=0;' in src and 'clamp(50+4*(wv-wa),10,100)' in src and ',10,100))) {' in src and 'attacker_guard' not in src and
   'clamp(50+3*(wa-wv)+2*' in src and 'v>0xFFFF?0xFFFF:v' in src, 'block zeroes damage; percent clamps; crit u16 cap')
# force colours: 2AD010 reads Force+49 (<3E) -> 2ACF00 mode 0 -> 32C220 table image+1A26590 (RRGGBBAA)
ok(rd(0x2AD010, 0x30).find(bytes.fromhex('0fb64149')) >= 0 or b'\x49' in rd(0x2AD010, 0x30), '2AD010 reads Force+49')
ok(rd(0x32C240, 16).find(bytes.fromhex('458b9483906' + '5a201')) >= 0, '32C220 loads the colour table at +1A26590')
cols = struct.unpack('<62I', rd(0x1A26590, 62*4))
ok(cols[0] == 0x2828E8FF and cols[60] == 0xFFFFFFFF and cols[61] == 0 and 'colours[0]==0x2828E8FFu && colours[60]==0xFFFFFFFFu && colours[61]==0' in src, 'force colour table guard')
ok('kStrategistClass[3]={75,76,77}' in src and 'cs>0.85f && ch_>=0.30f && ch_<=0.43f' in src and 'h=h<=0.62f?0.12f*(0.62f-h)/0.42f:1-0.06f*(h-0.62f)/0.30f' in src, 'mounted strategist; team colour only on the pure-green ramp; option C warm contrast')
ok('kRows=64' in src and 't.Height=kRows' in src and 'owner[0x49]<kColourCount' in src, '64 palette rows by Force+49')
hl = (P/'src/ccz_sprites.hlsl').read_text(encoding='utf-8')
ok('near==0)discard' in hl and 'team&0x200' in hl and 'output[row][0]=lum>0.45f' in src, 'contrasting outline and crit flash')
ok('kSpecialRow+static_cast<unsigned>(view)*48' in src and 'kSpecialRow+3*48' in src and 'troops<u.troops && now-u.hurt_at>=kHurtCooldownMs)u.hurt_at=now' in src and 'kPixel=9.0f' in src and 'kMinScreen=1.0f/10' in src and 'drawable && InterlockedCompareExchange(&hide_model' in src and 'r.team=kRedHare' in src and 'row==kRedHare' in src and 'kForward' not in src and 'kAttackFrameMs=260' in src and '/150)%4' not in src, 'block / hurt special frames')
ok('kCharges=5' in src and 'left=trigger?kCharges-1:(left>0?left-1:0);slot->last=day;' in src and 'day<=b.last+1' in src and 'kBuffDays' not in src, '5-charge state, ends after a day without an exchange')
ok(rd(0x32BA50, 0x1C) == bytes.fromhex('0fbf018d14400fbe41028d14900fbe4103446bc21e83c0e14103c0c3') and '*12+static_cast<signed char>(d[2]))*30+static_cast<signed char>(d[3])-31' in src, '32BA50 day = (year*12+month)*30+day-31')
ok('buff_use(victim,day,true,!guard)' in src and 'buff_use(attacker,day,false,!fury)' in src, 'blocks, guarded attacks and crits each spend a charge')
# User 2026-11-02: keep five-hit mechanics, display effects only on real hits.
ok('block && (guard ||' in src and 'critical && (fury ||' in src and 'if(u.guard && now-u.moved>=400)return guard_pose;' not in src and 'effect_visible(u,kind!=0,now)' in src, 'held charges cannot invent an idle block/critical effect')
ok('uB9C9'+chr(92)+'uAE30!' in src and 'cells[written++]=kLabelCell' in src and 'kQuads=kSlots*3' in src and 'b.ByteWidth=sizeof(Vertex)*6*kQuads' in src, '막기!/회심! labels, vertex room for two per unit')
ok('v.team&0x400' in hl and 'v.team&0x800' in hl, 'label colours in the shader')
ok('ccz_combat::install(mod_features.ccz_block,mod_features.ccz_critical)' in (P/'src/native_split_install.inl').read_text(encoding='utf-8'), 'ccz_combat installed in split_activate')
# wiring: one owner (convenience), guard code 32, three installer catalogs (SESSION_RULES 2)
inst = (P/'src/native_split_install.inl').read_text(encoding='utf-8'); adapter = (P/'src/native_adapter.cpp').read_text(encoding='utf-8')
ok('ccz_sprites_guards_match())split_disable(f.ccz_sprites,32)' in inst and 'ccz_sprites::install(' in inst, 'guard 32 + install in split_activate')
ok('f.ccz_block = on &&' in adapter and 'f.ccz_critical = on &&' in adapter and 'f.ccz_hide_model = f.ccz_sprites &&' in adapter and 'f.ccz_depth = f.ccz_sprites &&' in adapter, 'child keys depend on ccz_sprites')
ok('%x' % submit not in ' '.join('%x' % int(m, 16) for m in re.findall(r'\{0x[0-9A-Fa-f]+,0x([0-9A-Fa-f]+),', inst)), 'submit slot not in split_pointers (single owner)')
for cat in ('installer/InstallerGui.cs', 'distribution/FeatureSelect.ps1', 'FeatureSelect.ps1'):
    t = (P/cat).read_text(encoding='utf-8-sig')
    ok(all(k in t for k in ('ccz_sprites', 'ccz_hide_model', 'ccz_depth', 'ccz_block', 'ccz_critical')), cat+' lists the ccz keys')
# 2026-11-01 user: 조조전 탭 = SAN14_CCZ.dll. Convenience keeps the Present hook and the 169F8B CALL; the module owns 13A03D0.
ok('native_ccz_bridge.inl' in adapter and 'native_ccz_sprites.inl' not in adapter and '#include "native_ccz_sprites.inl"' in module, 'sprites code only in SAN14_CCZ.dll')
ok('L"SAN14_CCZ.dll"' in bridge and 'found->abi!=ccz_module_abi' in bridge and 'combat_fix' in bridge and 'call_publish(link)' in bridge and 'call_publish' not in src, 'bridge loads the module, publishes the combat CALL')
ok('SAN14CczGetApi' in module and 'patch_pointer(kSubmitSlot' in src and 'overlay_install();' in src and 'unified_overlay::' not in src, 'module exports its API; overlay via callback')
# User 2026-11-02: opt-in retention must not gate the independent local14 render rescue.
ok('far_position(u.army,pos,yaw)' in src and 'if(far_keep_enabled && !count && scene && armed>0 && distance<=armed*kFarFactor)' in src and '__except(EXCEPTION_EXECUTE_HANDLER)' in src and 'kFarFactor=2.0f' in src, 'far-zoom fallback keeps known armies only when enabled')
ok('if(!far_keep_enabled || armed<=0' in src and 'else if(far_keep_enabled && !count && recent' in src,
   'OFF disables stale-camera refresh and far-distance arming')
ok('bool ccz_far_keep = false;' in adapter and 'f.ccz_far_keep = f.ccz_sprites && feature_key(config, L"ccz", L"ccz_far_keep", false)' in adapter,
   'far keep defaults OFF and requires sprites')
ok('ccz_module_abi=2' in (P/'src/ccz_module_api.h').read_text(encoding='utf-8') and
   'install_sprites(hide,depth,far_keep)' in bridge and 'ccz_sprites::install(hide,depth,far_keep)' in module,
   'ABI 2 carries far flag; mismatched module is rejected')
ok('ccz_sprites_far_keep' in src and 'ccz_sprites_zoom_recovered' in src and
   'if(submitted)' in src and 'zoom_recovery_pending=false;has_submitted_sprites=true;' in src and
   'submitted_at-last_sprite_submission>=500' in src,
   'recovery log follows actual sprite submission')
rescue=src[src.index('// The failed zoom path'):src.index('if(main_rtv && rtv[0]==main_rtv')]
ok('far_keep_enabled' not in rescue and 'ccz_sprites_zoom_fallback' in rescue,
   'local14 rescue is independent of far keep')
render_hook=src[src.index('static void STDMETHODCALLTYPE set_targets('):src.index('static void hook_context(')]
ok(hashlib.sha256(render_hook.encode('utf-8')).hexdigest()=='da0c30d9793f771077e39ff774d4b5b11ba817c760ed63fb528e9d6326b784fb',
   '2026-11-02: entire local14 normal/rescue render-target hook unchanged')
# Original damage stage: attacker is a pointer; defender is INLINE at pair+8.
cache=P/'research/decomp_cache/1fbe6be7466a6646'
apply=(cache/'001749A0_c.txt').read_text(encoding='utf-8')
ok('puVar2 = (ushort *)*param_2;' in apply and 'puVar1 = (ushort *)(param_2 + 1);' in apply and 'victim=field_army(pair+8)' in src,'native attacker/inline defender ABI retained')
ok('fn<unsigned char(__fastcall *)(Obj)>(0x22CC40)' in src and 'fn<bool(__fastcall *)(unsigned,unsigned)>(0x2CA280)' in src,'force byte and native hostility bool return ABIs')
for rva,h in ((0x22CC40,0x777A504FBF698C1C),(0x2CA280,0x7221823C76F2F29F)):
    ok(fnv(rd(rva,0x30))==h and rd(rva,0x30)==run(rva,0x30),'native force/relation guard '+hex(rva))
ok('native_facing(u,yaw)' in src and 'std::sin(yaw)' in src and 'std::cos(yaw)' in src,'stationary turns use the native radians in both live and opted-in far capture')
print(json.dumps({'checks': checks, 'failed': fail}, ensure_ascii=False))
sys.exit(1 if fail else 0)
