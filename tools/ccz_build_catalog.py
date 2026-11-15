"""Read both E5 sources without executing them. Extract every nonempty full animation + standing preview.
The game reads the directory only, then only requested animation/thumbnail bytes. Existing CCZU stays intact.
"""
from pathlib import Path
import hashlib,json,struct,sys
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'tools'));import ccz_extract_units as units
sys.path.insert(0,str(P/'research/cao_cao_den'));import ls12
OUT=P/'assets/ccz/ccz_catalog.bin';META=OUT.with_suffix('.json')
def main():
    if OUT.exists():
        assert len(sys.argv)==2 and sys.argv[1]=='--replace-sha256='+hashlib.sha256(OUT.read_bytes()).hexdigest(),'Existing catalog requires its exact previous hash'
        backup=P/'build/development'/('ccz_catalog_before_'+hashlib.sha256(OUT.read_bytes()).hexdigest()[:12]+'.bin')
        if not backup.exists():backup.write_bytes(OUT.read_bytes())
    palettes=[];blobs=[];sources={};totals={}
    for source_id,name in enumerate(('신조조전','조조전')):
        ls12.G=str(P.parent/name);files=[ls12.load(n) for n,_,_ in units.SIZES]
        counts=[len(entries) for _,entries in files];assert len(set(counts))==1
        pd,entries=ls12.load(units.palette_name());pal=ls12.unpack(pd,*entries[0]);assert len(pal)==768
        palettes.append(pal+units.team_mask(pal));selected=0
        for cell in range(counts[0]):
            raw=b''.join(ls12.unpack(data,*entries[cell]) for data,entries in files);assert len(raw)==units.RAW
            if not any(raw):continue
            packed=units.rle(raw);assert units.unrle(packed,len(raw))==raw
            preview=raw[6*48*48:7*48*48];assert len(preview)==2304
            if not any(preview):
                for frame in range(11):
                    candidate=raw[frame*2304:(frame+1)*2304]
                    if any(candidate):preview=candidate;break
                else:
                    for frame in range(12):
                        at=11*2304+frame*4096;candidate=raw[at:at+4096]
                        if any(candidate):preview=bytes(candidate[(y*64//48)*64+x*64//48] for y in range(48) for x in range(48));break
                    else:
                        at=11*2304+12*4096
                        for frame in range(5):
                            candidate=raw[at+frame*2304:at+(frame+1)*2304]
                            if any(candidate):preview=candidate;break
            assert any(preview),'Every selectable shape must have a visible representative'
            blobs.append((source_id*4096+cell,source_id,packed,preview));selected+=1
        totals[name]={'entries':counts[0],'nonempty':selected}
        sources[name]={n:hashlib.sha256((Path(ls12.G)/n).read_bytes()).hexdigest() for n in [n for n,_,_ in units.SIZES]+[units.palette_name()]}
    table=16+2048;offset=table+24*len(blobs);records=bytearray();body=bytearray()
    for cell,source,packed,preview in blobs:
        at=offset+len(body);body+=packed;thumb=offset+len(body);body+=preview
        records+=struct.pack('<HHIIIII',cell,source,at,len(packed),units.RAW,thumb,len(preview))
    result=struct.pack('<4sIII',b'CCZC',1,len(blobs),table)+b''.join(palettes)+records+body
    OUT.write_bytes(result);meta={'format':'CCZC v1','count':len(blobs),'bytes':len(result),'sha256':hashlib.sha256(result).hexdigest(),'sources':sources,'counts':totals,'runtime_directory_bytes':table+len(records),'runtime_gpu_atlas_cache_limit':256,'preview_bytes_per_cell':2304}
    META.write_text(json.dumps(meta,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');print(json.dumps(meta['counts'],ensure_ascii=False));print(json.dumps({k:meta[k] for k in ('count','bytes','runtime_directory_bytes','sha256')}))
if __name__=='__main__':main()
