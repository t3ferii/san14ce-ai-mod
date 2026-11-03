"""Title/native/layout/config evidence, independent of mocked UI calls. No game launch."""
from pathlib import Path
import hashlib,json,re,struct,sys
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'research/ui_atlas'));from build_catalog import PE
from build_layouts import parse_layout
sys.path.insert(0,str(P/'tools'));import prepare_ccz_title_layout as layout
source=(P/'src/native_ccz_title.inl').read_text(encoding='utf-8');pe=PE(P/'build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe')
split=(P/'src/native_split_install.inl').read_text(encoding='utf-8')
running=(P/'legacy/FogMod_Source/native_game_analysis/running_code_text.bin').read_bytes();checks=0
def check(value,label):
    global checks
    assert value,label;checks+=1
def fnv(b):
    h=0xcbf29ce484222325
    for c in b:h=(h^c)*0x100000001b3&0xffffffffffffffff
    return h
guards=re.findall(r'\{0x([0-9A-F]+),(0x[0-9A-F]+|64),0x([0-9A-F]+)ull\}',source)
check(len(guards)==12,'all native guards parsed')
for a,n,h in guards:
    a=int(a,16);n=int(n,0);b=pe.read(a,n)
    check(b==running[a-0x1000:a-0x1000+n] and fnv(b)==int(h,16),'guard vs independent executable + running code '+hex(a))
for slot,target in [(8,0x504670),(16,0x4F9310),(40,0x50B800)]:check(struct.unpack('<Q',pe.read(0x13E4E08+slot,8))[0]==0x140000000+target,'CTitleState native slot '+str(slot))
asm=(P/'research/decomp_cache/1fbe6be7466a6646/004E81D0_asm.txt').read_text(encoding='utf-8')
for site in ('1404e831d CALL 0x14086e0c0','1404e8341 CALL 0x14092e430','1404e836e CALL 0x14092e0b0','1404e8461 CALL 0x140867300'):check(site in asm,'retained title builder ABI call '+site)
stage=P/'build/development/ccz_title_right_layout';j=json.loads((stage/'traits9_layout.json').read_text(encoding='utf-8'));bank=(stage/'traits9_layout.bin').read_bytes()
check(hashlib.sha256(bank).hexdigest()==j['bank_sha256'],'merged layout bank hash')
original=(P/'research/ui_atlas/layout_bodies/E7A28866.lcsk').read_bytes();check(hashlib.sha256(original).hexdigest()==layout.ORIGINAL,'retained original title layout identity')
body=layout.patch(original);record=next(r for r in j['records'] if r['ktid']==layout.K)
check(layout.g1t.decompress(bank[record['offset']:record['offset']+record['size']])==body,'staged title matches deterministic patch')
parsed,nodes=parse_layout(body,layout.K)
check(parsed['node_count']==39 and parsed['parent_tree_status']=='all_indices_and_child_counts_consistent','all title parent/child links and counts')
check(nodes[37]['parent_id']==12 and nodes[38]['parent_id']==37 and nodes[37]['x']==400 and nodes[37]['y']==-400,'CCZ row independent bottom-right placement')
_,_,_,original_records,_=layout.split(original);_,_,_,new_records,_=layout.split(body)
check(all(new_records[i]==original_records[i] for i in range(37) if i!=12),'all original title records preserved except parent child count (user 2026-11-03)')
check(nodes[0]['x']+nodes[12]['x']==0 and nodes[0]['y']+nodes[12]['y']==0 and nodes[37]['x']-nodes[0]['x']==1360 and nodes[0]['y']-nodes[37]['y']==940,'native bottom-right coordinates have 50/44 viewport margins')
counts=tuple(sum(r['size']==s for r in nodes) for s in (88,144,224,736))
check(struct.unpack_from('<4H',body,24)==counts,'title header record counts match all actual node types')
check(struct.unpack_from('<H',body,20)[0]==39 and body[22:24]==original[22:24]==b'\x04\x00','16-bit node count preserves adjacent nonzero original header field (user 2026-11-02)')
check(body[12:20]==original[12:20] and body[32:76]==original[32:76],'unrelated title header and animation declarations preserved')
current=json.loads((P/'build/input_package/traits9_layout.json').read_text(encoding='utf-8-sig'));old=[r for r in current['records'] if r['ktid']!=layout.K]
check(j['records'][:-1]==old and bank[:record['offset']]==(P/'build/input_package/traits9_layout.bin').read_bytes()[:record['offset']],'all previous layout records and bank prefix preserved')
appearance=(P/'src/ccz_appearance.h').read_text(encoding='utf-8')
check('low=70,high=90' in appearance and 'forms.fill(255)' in appearance,'unchanged default leadership and personal masks')
for n in ['installer/InstallerGui.cs','distribution/FeatureSelect.ps1','FeatureSelect.ps1']:check('ccz_appearance.profile' in (P/n).read_text(encoding='utf-8-sig'),'registered raw profile '+n)
asset=(P/'assets/ccz/ccz_units.bin').read_bytes();_,_,count,table=struct.unpack_from('<4s3I',asset)
cells={struct.unpack_from('<H',asset,table+i*16)[0] for i in range(count)}
check(all(c*3 in cells for c in range(190)),'every selectable common class exists in current sprite atlas')
check('ccz_module_abi=2' in (P/'src/ccz_module_api.h').read_text(encoding='utf-8').replace(' ',''),'existing CCZ API ABI unchanged')
check(source.index('current=pane=button=nullptr')<source.index('fn<Leave>(0x4F9310)'),'clear title references before native destruction')
check('ccz_settings::show();' in source and 'GetForegroundWindow' not in source,'native asynchronous title entry without Windows popup')
check('kAttachSite=0x4E83F3' in source and 'kAttachSite+5+*reinterpret_cast<int *>(image+kAttachSite+1)!=0x866230' in source,'last native title-row attach callsite and target guard')
callsite_bytes=b'\xE8\x38\xDE\x37\x00';check(pe.read(0x4E83F3,5)==callsite_bytes,'plain executable title attach callsite bytes')
check(running[0x4E83F3-0x1000:0x4E83F3-0x1000+5]==callsite_bytes,'retained running code title attach callsite bytes')
check(0x4E83F3+5+struct.unpack('<i',callsite_bytes[1:])[0]==0x866230,'independent callsite target calculation')
check('fn<void(__fastcall *)(Obj,int,Obj,int)>(0x866230)(root,id,object,order);' in source,'native row attach executes before mod insertion')
check('if(id!=32 || !initializing || initializing_button_added)return;' in source and 'if(add_button(initializing))initializing_button_added=true;' in source,'only last native row triggers one in-constructor CCZ insertion')
check('initializing=nullptr;return result;' in source and 'fn<Initialize>(0x504670)(state);add_button(state)' not in source,'late post-constructor insertion removed')
check(split.index('if(mod_features.ccz_sprites)ccz_title::install();') < split.index('layout_wait::install();'),'title hook is installed before layout_wait patches 86E0C0')
editor=(P/'src/native_ccz_editor.inl').read_text(encoding='utf-8')
check('unified_dialogs::choices' in editor and 'unified_dialogs::number' in editor and 'DialogBox' not in editor and 'CreateWindow' not in editor,'game-native list and numeric editors')
check('ccz_preview::picture' in editor and 'session->catalog.entries' not in editor and 's.catalog.entries' in editor,'all source shapes and actual representative images')
adapter=(P/'src/native_adapter.cpp').read_text(encoding='utf-8')
check('#include "native_ccz_editor.inl"' in adapter and '#include "native_ccz_settings.inl"' not in adapter,'old Windows dialog no longer compiled into the game module')
meta=json.loads((P/'assets/ccz/ccz_catalog.json').read_text(encoding='utf-8'))
catalog=(P/'assets/ccz/ccz_catalog.bin').read_bytes()
check(hashlib.sha256(catalog).hexdigest()==meta['sha256'] and meta['count']==1078,'full catalog hash and nonempty source count')
check(meta['counts']['신조조전']['nonempty']==686 and meta['counts']['조조전']['nonempty']==392,'both source folders included')
check(catalog[16:1040]==asset[16:1040],'existing default-source palette and team mask unchanged')
check('ccz_module_abi=2' in (P/'src/ccz_module_api.h').read_text(encoding='utf-8').replace(' ',''),'new cell settings do not change DLL API')
preview=(P/'src/native_ccz_preview.inl').read_text(encoding='utf-8')
check('release_pending' in preview and 'std::vector<unsigned>().swap(pixels)' in preview,'preview resources released after leaving editor')
print(json.dumps(dict(status='ccz_settings_static_passed',checks=checks,live_game=False)))
