from pathlib import Path
import json,struct,sys
P=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(P/'legacy/FogMod_Source/tools'))
from audit_native_game import PEImage
data=P/'research/modpack_v16/extracted/payload'
stats=PEImage(data/'SAN14_Stats200.dll');collab=PEImage(data/'SAN14_Collab.dll')
rows=[]
def array(b):return '{'+','.join('0x%02X'%v for v in b)+'}'
for n in range(14):
    b=stats.read(0x2000+n*112,112);rva,size,_,_=struct.unpack_from('<IIII',b)
    if n==12:continue
    rows.append('{0x%X,%d,%s,%s}'%(rva,size,array(b[16:16+size]),array(b[32:32+size])))
rows.append('{0x2B9DCA,6,{0x41,0x83,0xFD,0x64,0x75,0x07},{0x41,0x80,0xFD,0xC8,0x72,0x07}}')
header='// Extracted from the supported v1.6 DLL; growth gate extends its original policy.\n'
header+='static const ModpackBytes modpack_bytes[]={\n'+',\n'.join(rows)+'\n};\n'
for name,rva,count in [('traits',0x2020,39),('tactics',0x2070,43)]:
    values=struct.unpack('<'+'H'*count,collab.read(rva,count*2))
    header+='static const unsigned short collab_'+name+'[]={'+','.join(map(str,values))+'};\n'
hover=PEImage(data/'SAN14_TraitHover.dll')
slots=[struct.unpack('<III',hover.read(0x62A0+i*12,12)) for i in range(152)]
header+='static const unsigned hover_slots[]={'+','.join('0x%X'%s[0] for s in slots)+'};\n'
(P/'src/modpack_tables.h').write_text(header,encoding='utf-8')
print('prepared',len(rows),'stat patches; collab ID tables;',len(slots),'hover callback slots')
