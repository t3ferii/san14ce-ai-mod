"""Assemble our leaf function and require relocation-free COFF text."""
from pathlib import Path
import struct,subprocess
P=Path(__file__).resolve().parents[1]
source=P/'src/progress_distance_kernel.asm';out=P/'build/development/progress_distance_kernel.obj'
assembler=Path(r'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\ml64.exe')
subprocess.run([str(assembler),'/nologo','/c','/Fo'+str(out),str(source)],check=True)
data=out.read_bytes();machine,count,_,_,_,optional,_=struct.unpack_from('<HHIIIHH',data,0)
assert machine==0x8664 and optional==0
text=None
for index in range(count):
    at=20+index*40;name=data[at:at+8].rstrip(b'\0')
    if name.startswith(b'.text'):
        size,raw=struct.unpack_from('<II',data,at+16);relocations=struct.unpack_from('<H',data,at+32)[0]
        assert relocations==0,'Kernel needs unsupported relocation'
        text=data[raw:raw+size]
assert text and len(text)<=128
coords=struct.pack('<Q',0x1122334455667788);gate=struct.pack('<Q',0x8877665544332211)
assert text.count(coords)==1 and text.count(gate)==1
header=['#pragma once','// Generated from progress_distance_kernel.asm; no relocations.',
        'static const unsigned char progress_kernel_bytes[]={']
header+=['    '+','.join('0x%02X'%b for b in text[i:i+16])+',' for i in range(0,len(text),16)]
header+=['};','static const unsigned progress_kernel_coordinates_offset=%u;'%text.index(coords),
        'static const unsigned progress_kernel_original_offset=%u;'%text.index(gate)]
destination=P/'src/progress_distance_kernel.h';content='\n'.join(header)+'\n'
if not destination.exists() or destination.read_text()!=content:destination.write_text(content)
print('Relocation-free progress kernel: %d bytes'%len(text))
