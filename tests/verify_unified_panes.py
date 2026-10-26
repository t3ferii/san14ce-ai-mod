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
