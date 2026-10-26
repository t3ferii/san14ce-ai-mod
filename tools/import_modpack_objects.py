"""Statically link two fingerprinted, no-CRT native implementations as COFF.

Preserves original RVA distances, resolves imports at link time, and converts PE
DIR64 fixups to COFF ADDR64 relocations. No DLL loading or PE entrypoint execution.
Original files remain untouched. Runtime code seals text/data pages separately.
"""
from pathlib import Path
import hashlib,json,struct,sys
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'legacy/FogMod_Source/tools'))
from audit_native_game import PEImage
SOURCES={
 'hover':('SAN14_TraitHover.dll','8cfad1a6ada78ecab898bef9d166167874d730a757a94d3f707b60cca1ef91bc',0x2dd0,[0x3030,0x3290]),
 'deaths':('SAN14_BattleDeaths.dll','1f354aec9b1e2ee29d0f07e552e32f87606ca1bf8e21738c0db631cff56cf696',0x1050,[0x1a00,0x20a0])}
def build(key,out):
    name,digest,entry,disabled=SOURCES[key]
    path=P/'research/modpack_v16/extracted/payload'/name
    pe=PEImage(path);assert hashlib.sha256(pe.data).hexdigest()==digest
    nt=struct.unpack_from('<I',pe.data,0x3c)[0]; opt=nt+24
    directories=[struct.unpack_from('<II',pe.data,opt+112+i*8) for i in range(16)]
    start=0x1000; blob=bytearray(pe.image_size-start)
    sections=[]
    for s in pe.sections:
        if s['raw_size']:
            blob[s['rva']-start:s['rva']-start+s['raw_size']]=pe.data[s['file_offset']:s['file_offset']+s['raw_size']]
        sections.append((s['rva']-start,s['virtual_size'],0x20 if s['name']=='.text' else 4 if s['name']=='.data' else 2))
    for rva in disabled:blob[rva-start]=0xc3
    names=['san14_modpack_'+key]; reloc=[]
    def symbol(name):
        if name not in names:names.append(name)
        return names.index(name)
    if key=='hover':
        # Its InitOnce installer is reached by native UI callbacks, after native
        # fixdata initialization. Seed preloaded collab records on that thread.
        site=0x3720-start; gate=pe.sections[-1]['rva']+0x100-start
        original=bytes(blob[site:site+6]);assert original==bytes.fromhex('415741564155')
        bridge=bytearray(bytes.fromhex('4883ec2848b8')+bytes(8)+bytes.fromhex('ffd04883c428'))
        bridge+=original+b'\xe9'+struct.pack('<i',(site+6)-(gate+len(bridge)+len(original)+5))
        blob[gate:gate+len(bridge)]=bridge
        blob[site:site+6]=b'\xe9'+struct.pack('<i',gate-site-5)+b'\x90'
        reloc.append((gate+6,symbol('SAN14ModpackCollabBootstrap'),1))
        sections[-1]=(sections[-1][0],0x100+len(bridge),0x20)
    at,size=directories[5];end=at+size
    while at<end:
        page,n=struct.unpack('<II',pe.read(at,8));assert n>=8
        for i in range((n-8)//2):
            item=struct.unpack('<H',pe.read(at+8+i*2,2))[0];typ=item>>12
            if not typ:continue
            assert typ==10
            offset=page+(item&4095)-start
            value=struct.unpack_from('<Q',blob,offset)[0]-pe.image_base-start
            assert 0<=value<len(blob)
            struct.pack_into('<Q',blob,offset,value);reloc.append((offset,0,1))
        at+=n
    imports=[]; at=directories[1][0]
    while at:
        original,_,_,dll,iat=struct.unpack('<IIIII',pe.read(at,20))
        if not any((original,dll,iat)):break
        k=0
        while True:
            value=struct.unpack('<Q',pe.read((original or iat)+k*8,8))[0]
            if not value:break
            assert not value>>63,'Ordinal imports are not admitted'
            offset,_=pe.offset(value+2,1);api=pe.data[offset:pe.data.index(0,offset)].decode('ascii')
            dest=iat+k*8-start;struct.pack_into('<Q',blob,dest,0)
            reloc.append((dest,symbol(api),1));imports.append(api);k+=1
        at+=20
    assert len(reloc)<65535
    strings=bytearray(b'\0\0\0\0');symbols=bytearray()
    for i,name in enumerate(names):
        label=name.encode();at=len(strings);strings+=label+b'\0'
        symbols+=struct.pack('<IIIhHBB',0,at,0,1 if i==0 else 0,0,2,0)
    struct.pack_into('<I',strings,0,len(strings))
    raw=60;relat=raw+len(blob);symat=relat+len(reloc)*10
    header=struct.pack('<HHIIIHH',0x8664,1,0,symat,len(names),0,0)
    section=struct.pack('<8sIIIIIIHHI',('.m'+key).encode(),0,0,len(blob),raw,relat,0,len(reloc),0,0xe0d00020)
    obj=header+section+blob+b''.join(struct.pack('<IIH',*r) for r in reloc)+symbols+strings
    (out/(key+'.obj')).write_bytes(obj)
    meta=dict(source_sha256=digest,entry=entry-start,size=len(blob),sections=sections,
              exception_rva=directories[3][0]-start,exception_count=directories[3][1]//12,
              imports=imports,relocations=len(reloc),disabled_diagnostic_writers=disabled)
    (out/(key+'.json')).write_text(json.dumps(meta,indent=2))
    return meta
def main():
    out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
    header=['// Generated from fingerprinted ModPack v1.6 native implementations.']
    for key in SOURCES:
        m=build(key,out);print(key,'COFF',m['relocations'],'relocations;',len(m['imports']),'imports')
        header.append('extern "C" unsigned char san14_modpack_'+key+'[];')
        header.append('static const ImportedSection '+key+'_sections[]={' + ','.join('{%d,%d,%d}'%tuple(s) for s in m['sections'])+'};')
        header.append('static const ImportedCode '+key+'_code={san14_modpack_'+key+',%d,%d,%d,%d,%s_sections,sizeof(%s_sections)/sizeof(ImportedSection)};'%(m['size'],m['entry'],m['exception_rva'],m['exception_count'],key,key))
    (out/'modpack_payloads.h').write_text('\n'.join(header)+'\n',encoding='utf-8')
if __name__=='__main__':main()
