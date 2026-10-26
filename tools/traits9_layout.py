"""Conflict-free traits9 layout patch for 0002/ScreenLayout.rdb.

The index entry is IDRK0000 + size(+8) ... locator length(+16) ... ktid(+36),
with an ASCII locator 'offset@size#bank' (hex) at its end. We only replace
our four layout entries and keep every other entry byte-for-byte, so another
mod (e.g. a face editor using its own bank) may change the index before or
after us. Our bank number is chosen at install time from free numbers.

  python tools/traits9_layout.py extract   -> research/traits9/layout_patch/
  python tools/traits9_layout.py selftest  (no game files are written)
"""
from pathlib import Path
import base64,hashlib,json,re,struct,sys
P=Path(__file__).resolve().parents[1];G=P.parent
OUT=P/'research/traits9/layout_patch'
BANK_CHOICES=list(range(30,23,-1))  # 30..24; 31 is the face editor default.

def parse(data):
    assert data[:8]==b'_DRK0000','not ScreenLayout.rdb'
    head,=struct.unpack_from('<I',data,8);p=head;entries=[]
    while p<len(data):
        assert data[p:p+8]==b'IDRK0000',hex(p)
        size,=struct.unpack_from('<I',data,p+8);entries.append(data[p:p+size]);p+=size+3&~3
    count,=struct.unpack_from('<I',data,16);assert count==len(entries),(count,len(entries))
    return data[:head],entries
def build(header,entries):
    out=bytearray(header);struct.pack_into('<I',out,16,len(entries))
    for e in entries:out+=e;out+=b'\0'*(-len(e)&3)
    return bytes(out)
def ktid(e):return struct.unpack_from('<I',e,36)[0]
def locator(e):
    n,=struct.unpack_from('<Q',e,16);s=e[len(e)-n:].rstrip(b'\0').decode('ascii')
    m=re.fullmatch('([0-9a-f]+)@([0-9a-f]+)(?:#([0-9a-f]+))?',s)
    if not m:return None  # the index's opaque last entry has no locator
    return int(m[1],16),int(m[2],16),int(m[3],16) if m[3] else 0
def relocate(e,off,size,bank,dsz):
    n,=struct.unpack_from('<Q',e,16);new=bytearray(e[:len(e)-n])
    loc=('%x@%x#%x'%(off,size,bank)).encode('ascii')+b'\0';new+=loc
    struct.pack_into('<I',new,8,len(new));struct.pack_into('<Q',new,16,len(loc));struct.pack_into('<Q',new,24,dsz)
    return bytes(new)
def used_banks(entries):return {l[2] for l in map(locator,entries) if l}
def choose_bank(entries,folder,preferred=None):
    used=used_banks(entries)
    if preferred is not None:return preferred
    for b in BANK_CHOICES:
        if b not in used and not (folder/('ScreenLayout.rdb.bin%d'%b)).exists():return b
    raise RuntimeError('no free ScreenLayout bank')
def apply(index_bytes,records,bank):
    """Return (new index, applied ktids, skipped ktids)."""
    header,entries=parse(index_bytes);applied=[];skipped=[]
    for r in records:
        orig=base64.b64decode(r['original_entry'])
        at=[i for i,e in enumerate(entries) if ktid(e)==r['ktid']]
        if len(at)!=1:skipped.append(r['ktid']);continue
        cur=entries[at[0]];mine=locator(cur)==(r['offset'],r['size'],bank)
        if cur!=orig and not mine:skipped.append(r['ktid']);continue
        entries[at[0]]=relocate(orig,r['offset'],r['size'],bank,r['dsz']);applied.append(r['ktid'])
    return build(header,entries),applied,skipped
def revert(index_bytes,records,bank):
    header,entries=parse(index_bytes);reverted=[]
    for r in records:
        for i,e in enumerate(entries):
            if ktid(e)==r['ktid'] and locator(e)==(r['offset'],r['size'],bank):
                entries[i]=base64.b64decode(r['original_entry']);reverted.append(r['ktid'])
    return build(header,entries),reverted,bank in used_banks(entries)

def extract():
    state=json.loads((P/'backup/input_state.json').read_text(encoding='utf-8-sig'))
    pristine=Path(state['asset_restore']['0002/ScreenLayout.rdb']['backup'].replace('\\','/').split('Romance_of_the_Three_Kingdoms_14/',1)[1])
    pristine=(G/pristine).read_bytes()
    assert hashlib.sha256(pristine).hexdigest()=='5983c6538223d63a1b2da915de7d4521bd74bf169d8fda19c1f00a88900a9c71'
    asset=P/'research/traits9/assets/0002';modified=(asset/'ScreenLayout.rdb').read_bytes();bank=(asset/'ScreenLayout.rdb.bin31').read_bytes()
    _,orig=parse(pristine);_,mod=parse(modified);by={ktid(e):e for e in orig};records=[]
    for e in mod:
        l=locator(e)
        if not l or l[2]!=31:continue
        off,size,b=l
        k=ktid(e);dsz,=struct.unpack_from('<Q',e,24)
        assert bank[off:off+8]==b'IDRK0000' and off+size<=len(bank)
        records.append(dict(ktid=k,offset=off,size=size,dsz=dsz,original_entry=base64.b64encode(by[k]).decode()))
        assert relocate(by[k],off,size,31,dsz)==e
    assert len(records)==4
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'traits9_layout.bin').write_bytes(bank)
    manifest=dict(format=1,bank_sha256=hashlib.sha256(bank).hexdigest(),bank_size=len(bank),
                  legacy_bank=31,legacy_index_sha256=hashlib.sha256(modified).hexdigest(),records=records)
    (OUT/'traits9_layout.json').write_text(json.dumps(manifest,indent=1)+'\n',encoding='utf-8')
    print('extracted',len(records),'records')

def selftest():
    m=json.loads((OUT/'traits9_layout.json').read_text(encoding='utf-8'));recs=m['records']
    state=json.loads((P/'backup/input_state.json').read_text(encoding='utf-8-sig'))
    rel=state['asset_restore']['0002/ScreenLayout.rdb']['backup'].replace('\\','/').split('Romance_of_the_Three_Kingdoms_14/',1)[1]
    pristine=(G/rel).read_bytes();legacy=(P/'research/traits9/assets/0002/ScreenLayout.rdb').read_bytes()
    # 1. pristine + bank31 reproduces the legacy installed index exactly.
    out,a,s=apply(pristine,recs,31);assert out==legacy and len(a)==4 and not s
    # 2. pristine + bank30, then revert, is byte-identical to pristine.
    out,a,s=apply(pristine,recs,30);assert len(a)==4;back,r,still=revert(out,recs,30);assert back==pristine and len(r)==4 and not still
    # 3. a foreign mod's change elsewhere survives apply and revert.
    h,e=parse(pristine);i=next(i for i,x in enumerate(e) if locator(x) and locator(x)[2]==0 and ktid(x) not in {r['ktid'] for r in recs})
    foreign=relocate(e[i],0x10,locator(e[i])[1],31,struct.unpack_from('<Q',e[i],24)[0]);e[i]=foreign;face=build(h,e)
    out,a,s=apply(face,recs,30);assert len(a)==4 and foreign in parse(out)[1]
    back,_,_=revert(out,recs,30);assert back==face
    # 4. legacy bank31 install migrates: revert 31 then apply 30 keeps the foreign edit.
    h,e=parse(legacy);e[i]=foreign;mixed=build(h,e)
    back,r,_=revert(mixed,recs,31);assert len(r)==4 and back==face
    # 5. another mod owning one of our layouts is left alone.
    h,e=parse(pristine);j=next(j for j,x in enumerate(e) if ktid(x)==recs[0]['ktid']);e[j]=relocate(e[j],0,8,29,1);other=build(h,e)
    out,a,s=apply(other,recs,30);assert s==[recs[0]['ktid']] and len(a)==3
    # 6. re-apply is idempotent.
    once,_,_=apply(pristine,recs,30);twice,a,_=apply(once,recs,30);assert once==twice and len(a)==4
    assert choose_bank(parse(pristine)[1],Path('/nonexistent'))==30
    print('selftest ok')

if __name__=='__main__':
    {'extract':extract,'selftest':selftest}[sys.argv[1]]()
