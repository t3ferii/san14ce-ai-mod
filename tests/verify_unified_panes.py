"""Compare pane geometry and native classes against the actual analysis image."""
from pathlib import Path
import hashlib,json,re,struct,sys

project=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(project/'legacy/FogMod_Source/tools'))
from audit_native_game import PEImage
path=project/'build/ui_native_analysis/editor83_plain/game.exe.unpacked.exe'
assert hashlib.sha256(path.read_bytes()).hexdigest()=='1fbe6be7466a66466fe97cf3ab7d49392913346879c7d40de7f4eaf86eef81c6'
pe=PEImage(path);checks=0
def check(ok,label):
    global checks
    assert ok,label
    checks+=1
def virtual(table,slot,target):
    check(struct.unpack('<Q',pe.read(table+slot,8))[0]-pe.image_base==target,f'actual native virtual {table:X}+{slot:X}->{target:X}')
for slot,target in [(0,0x48C190),(0x10,0x878810),(0x28,0x8FBD40),(0x38,0x257C50),(0x1E0,0x2BEFE0)]:virtual(0x13ACF30,slot,target)
for slot,target in [(0,0x84C3B0),(0x10,0x85E750),(0x28,0x85E030),(0x38,0x84C9A0),(0x188,0x851930)]:virtual(0x146C440,slot,target)
for at,table in [(0x2504E8,0x13ACF30),(0x84B338,0x146C440)]:
    b=pe.read(at,7);check(b[:3]==bytes.fromhex('488d05') and at+7+struct.unpack_from('<i',b,3)[0]==table,'native constructor chooses the verified pane class')
check(pe.read(0x875F08,3)==bytes.fromhex('895128'),'width setter writes width+28')
check(pe.read(0x8740F8,3)==bytes.fromhex('89512c'),'height setter writes height+2C; not absolute-position vcall+88')
source=(project/'src/native_unified_dialogs.inl').read_text(encoding='utf-8')
# User 2026-11-03: verify the missing RDX against real instructions, not the mock.
check(pe.read(0x867930,5)==bytes.fromhex('4885d27456'),'scrollbar attachment tests its second argument RDX')
check(pe.read(0x86794A,7)==bytes.fromhex('48899178010000'),'scrollbar attachment stores RDX at body+178')
call=pe.read(0x867979,5)
check(call[0]==0xE8 and 0x867979+5+struct.unpack_from('<i',call,1)[0]==0x1A3CB0,'scrollbar argument receives the original event delegate')
check('ui_call_trace::BarBind,0x867930,fn<void(__fastcall *)(Obj,Obj)>(0x867930),body,object' in source,'production wrapper forwards body and its actual scrollbar')
# User 2026-11-03 second crash: the old mock accepted a null native vtable.
delegate=int(re.search(r'kChangeDelegateRva=0x([0-9A-F]+)',source)[1],16)
lea=pe.read(0x2C1381,7)
check(lea[:3]==bytes.fromhex('488d05') and 0x2C1388+struct.unpack_from('<i',lea,3)[0]==delegate,'delegate address derives independently from original CDataSelectDlg constructor LEA')
for slot,target in enumerate((0x3056E0,0x3056E0,0x65E490,0x306A40,0x90B5D0,0x7FEB90)):virtual(delegate,slot*8,target)
copy=pe.read(0x3056E5,7)
check(copy[:3]==bytes.fromhex('488d05') and 0x3056EC+struct.unpack_from('<i',copy,3)[0]==delegate,'native delegate copy constructor publishes the same complete vtable')
check(struct.unpack('<Q',pe.read(0x13AEFA0,8))[0]==0,'old delegate address is null in actual executable; negative regression control')
editor=(project/'src/native_ccz_editor.inl').read_text(encoding='utf-8')
check('image+kChangeDelegateRva' in source,'shared panes use the verified native delegate address')
layout=editor.split('static bool layout_existing_list(',1)[1].split('static void queue(',1)[0]
check('pointer(r.object,0x180)' in layout and 'pointer(r.object,0x198)' in layout,'CCZ uses controls already owned by CDataSelectDlg')
check(not any(t in layout for t in ('make_data_list','make_scrollbar','dispose_control','0x1A3CB0','3F6BC0')),'CCZ frame neither replaces/deletes native controls nor rebinds their delegates')
# User 2026-11-03: separate the actual list viewport and require explicit gallery confirmation.
check(pe.read(0x2C0ED6,7)==bytes.fromhex('418b8aa4000000'),'native constructor reads viewport column count from config+A4')
check(pe.read(0x2F0B25,9)==bytes.fromhex('83b8a000000000741c'),'native single-choice auto-close is skipped when config+A0 is zero')
pick=editor.split('static bool pick(',1)[1].split('static void ',1)[0]
check('r->config+0xA4)=1' in pick and 'r->column+0x10)=1000' in pick,'CCZ configures internal column viewport before native open')
check('if(session->step==Gallery)' in pick and 'r->config+0xA0)=0' in pick and 'L"결정"' in pick and 'unified_dialogs::close(*dialog,true)' in pick,'gallery uses original deferred acceptance plus an explicit confirmation command')
check(80+1048<1232 and 80+1048+16+24<1232,'list and scrollbar including checkbox gutter end before preview starts')
bounds={name:tuple(map(int,re.search(name+r'_bounds\{([^}]+)\}',source)[1].split(','))) for name in ('roster','condition')}
for name,(x,y,w,h) in bounds.items():check(x>=0 and y>=0 and x+w+32<=1920 and y+h<=1080 and w>0 and h>80,name+' and its scrollbar stay inside the shared viewport')
left,right=bounds['roster'],bounds['condition']
check(left[0]+left[2]+32<right[0] and left[1]==right[1] and left[3]==right[3],'two visible sibling panes with a gap and aligned edges')
body=source.split('static bool attach_sidebar(Dialog &r)',1)[1].split('static void process_control',1)[0]
check('0x250760' not in body and '0x2C0820' not in body and 'make_data_list(side,r.object,condition_bounds)' in body,'right pane never creates another fullscreen dialog/backdrop')
personnel=(project/'src/native_unified_personnel.inl').read_text(encoding='utf-8')
# v11.31: one native roster (no side pane). Tabs must fit the native list.
tabs=re.search(r'search_tabs\[12\]\[14\]=\{(.*?)\n\};',personnel,re.S)[1]
check('r->sidebar' not in personnel and 'make_sidebar' not in personnel,'roster no longer attaches the two-pane sidebar')
widths=[]
for row in re.findall(r'\{([^}]+)\}',tabs):
    ids=[int(n) for n in row.split(',') if int(n)>=0]
    total=48
    for n in ids:
        width=108 if 213<=n<=221 else 132 if n==222 else 600 if n==101 else struct.unpack('<I',pe.read(0x1A2B850+n*96+16,4))[0]
        total+=width
    widths.append(total)
    check(total<=1400,f'roster tab {len(widths)} content {total}px fits inside the native 1400px roster')
record=dict(checks=checks,bounds=bounds,tab_widths=widths,fullscreen_hosts=1,visible_panes=2,native_game_executed=False,rendering_verified=False)
(project/'build/development/unified_personnel/panes_result.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
print(json.dumps(record))
