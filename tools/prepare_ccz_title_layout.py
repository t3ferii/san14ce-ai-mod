"""Append a native title-menu row to the current merged layout bank; never install it.

Only E7A28866 changes. Existing records/bank bytes remain byte-for-byte intact.
Native CTitleState 4E81D0 binds 13,20,22,24,26,28,30,32; their IDs stay fixed.
"""
from pathlib import Path
import argparse,base64,hashlib,json,struct,sys,zlib
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'research/speed_buttons'))
import g1t
K=0xE7A28866
ORIGINAL='b2f595e26ad052cd1882319da72f6846a1ae1ddaf068a1cd4860f13df095e216'
sha=lambda b:hashlib.sha256(b).hexdigest()
def split(d):
    assert d[:8]==b'LCSK9500';sections=[];at=76
    while at<len(d):
        kind,size=struct.unpack_from('<II',d,at);assert size>=8 and at+size<=len(d)
        sections.append((kind,at,size));at+=size
    assert at==len(d)
    _,sec,total=next(s for s in sections if s[0]==1)
    nsize,xsize=struct.unpack_from('<II',d,sec+8);count=struct.unpack_from('<H',d,sec+18)[0]
    hsize=struct.unpack_from('<I',d,sec+20)[0];assert hsize+nsize+xsize==total
    start=sec+hsize;end=start+nsize;pos=[];at=start
    for i in range(count):
        at=d.find(struct.pack('<HHH',i,i,1),at,end);assert at>=0;pos.append(at);at+=1
    pos.append(end);recs=[d[a:b] for a,b in zip(pos,pos[1:])]
    return sections,sec,hsize,recs,d[end:sec+total]
def append_names(tabs,names):
    out=b'';at=0
    while at<len(tabs):
        kind,count,size=struct.unpack_from('<III',tabs,at);raw=tabs[at:at+size];at+=size
        if kind!=1:out+=raw;continue
        offsets=list(struct.unpack_from('<%dI'%count,raw,12));body=raw[12+4*count:]
        for name in names:offsets.append(len(body));body+=struct.pack('<I',len(name))+name
        out+=struct.pack('<III',kind,len(offsets),12+4*len(offsets)+len(body))+struct.pack('<%dI'%len(offsets),*offsets)+body
    assert at==len(tabs);return out
def patch(d,*,bottom_right=True):
    assert sha(d)==ORIGINAL,'Unexpected title layout: preserve it instead of overwriting'
    sections,sec,hsize,recs,tabs=split(d);assert len(recs)==37
    original=list(recs);recs=list(map(bytearray,recs))
    assert len(recs[32])==88 and len(recs[33])==736
    assert struct.unpack_from('<HHH',recs[32],50)==(12,0,1)
    # Retain the former variant only to recognize the known installed payload.
    # New payloads preserve all eight original rows byte-for-byte.
    if not bottom_right:
        for i in (13,20,22,24,26,28,30,32):
            y=struct.unpack_from('<f',recs[i],12)[0];struct.pack_into('<f',recs[i],12,y+84.5)
    parent_children=struct.unpack_from('<H',recs[12],54)[0];struct.pack_into('<H',recs[12],54,parent_children+1)
    row=bytearray(original[32]);art=bytearray(original[33]);struct.pack_into('<HH',row,0,37,37);struct.pack_into('<HH',art,0,38,38);struct.pack_into('<H',art,50,37)
    if bottom_right:struct.pack_into('<ff',row,8,400.0,-400.0) # Native position 1360,940; size510x96, margins50/44.
    recs+=[row,art];nrec=b''.join(recs);xtab=append_names(tabs,[b'02_01_menu_btn_ccz',b'base_ccz'])
    head=bytearray(d[sec:sec+hsize]);struct.pack_into('<III',head,4,hsize+len(nrec)+len(xtab),len(nrec),len(xtab));struct.pack_into('<H',head,18,39);struct.pack_into('<I',head,24,hsize+len(nrec))
    _,_,oldtotal=next(s for s in sections if s[0]==1)
    out=bytearray(d[:sec])+head+nrec+xtab+d[sec+oldtotal:]
    # +20 is a uint16 node count. +22 is a separate original field (4 here).
    # A uint32 write erased it; the footer layout happened to have zero there.
    struct.pack_into('<I',out,8,len(out));struct.pack_into('<H',out,20,39)
    # Count declarations for the appended 0x58 and 0x2E0 records.
    for offset in (24,30):struct.pack_into('<H',out,offset,struct.unpack_from('<H',d,offset)[0]+1)
    _,_,_,after,_=split(out);assert len(after)==39
    allowed={12} if bottom_right else {12,13,20,22,24,26,28,30,32}
    assert all(after[i]==original[i] for i in range(37) if i not in allowed)
    assert after[37][4:8]==original[32][4:8] and after[37][16:]==original[32][16:]
    assert after[38][4:50]==original[33][4:50] and after[38][52:]==original[33][52:]
    assert out[22:24]==d[22:24] and out[32:76]==d[32:76]
    return bytes(out)
def entry(raw,body):
    csz=struct.unpack_from('<Q',raw,16)[0];head=bytearray(raw[:len(raw)-csz]);compressed=b''
    for at in range(0,len(body),16384):
        chunk=zlib.compress(body[at:at+16384],6);compressed+=struct.pack('<I',len(chunk))+chunk
    compressed+=struct.pack('<I',0);struct.pack_into('<QQQ',head,8,len(head)+len(compressed),len(compressed),len(body));struct.pack_into('<I',head,48,zlib.crc32(compressed)&0xffffffff)
    out=bytes(head)+compressed;assert g1t.decompress(out)==body;return out
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',default='build/development/ccz_title_layout');args=ap.parse_args()
    source=P/'build/input_package';out=P/args.output
    j=json.loads((source/'traits9_layout.json').read_text(encoding='utf-8-sig'));before=(source/'traits9_layout.bin').read_bytes();assert sha(before)==j['bank_sha256']
    existing=[r for r in j['records'] if r['ktid']==K];assert len(existing)<=1
    base=(P/'research/ui_atlas/layout_bodies/E7A28866.lcsk').read_bytes();body=patch(base)
    if existing:
        old=existing[0];raw=before[old['offset']:old['offset']+old['size']]
        previous=patch(base,bottom_right=False);former=bytearray(previous);struct.pack_into('<I',former,20,39)
        assert g1t.decompress(raw) in (body,previous,bytes(former)),'Unknown local title layout; do not overwrite'
        index=base64.b64decode(old['original_entry'])
    else:
        raw=g1t.raw_entry(K);assert sha(g1t.decompress(raw))==ORIGINAL
        r=g1t.rows()[K];index=g1t.RDB.read_bytes()[r['index_offset']:r['index_offset']+r['index_size']]
    new=entry(raw,body)
    assert struct.unpack_from('<I',index,36)[0]==K
    record=dict(ktid=K,offset=len(before),size=len(new),dsz=len(body),original_entry=base64.b64encode(index).decode())
    kept=[r for r in j['records'] if r['ktid']!=K]
    bank=before+new;result=dict(j);result.update(bank_sha256=sha(bank),bank_size=len(bank),records=kept+[record],ccz_title=dict(layout=K,node=37,art=38,nodes=39,original_sha256=ORIGINAL,native_position=[1360,940],original_menu_preserved=True))
    if existing:result['replaces_ktids']=[K]
    out.mkdir(parents=True,exist_ok=False)
    (out/'traits9_layout.bin').write_bytes(bank);(out/'traits9_layout.json').write_text(json.dumps(result,ensure_ascii=False,indent=1)+'\n',encoding='utf-8')
    assert bank[:len(before)]==before and result['records'][:-1]==kept
    print(json.dumps(dict(status='ccz_title_layout_prepared',records=len(result['records']),original_records_preserved=True,sha256=sha(bank))))
if __name__=='__main__':main()
