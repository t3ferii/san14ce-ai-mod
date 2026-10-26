from pathlib import Path
import sys, hashlib, shutil, subprocess, json, struct
PROJECT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(PROJECT/'tools'))
from package_mod import patch_existing_graphics_import, PEImage
fixture=PROJECT/'build/loader_fixture.exe'
directory=PROJECT/'build/loader_test'; directory.mkdir(exist_ok=True)
source_sha=hashlib.sha256(fixture.read_bytes()).hexdigest()
report=patch_existing_graphics_import(fixture,directory/fixture.name,source_sha)
shutil.copyfile(PROJECT/'build/SAN14_AITactics.dll',directory/'AI14.dll')
log=directory/'SAN14_AITactics.log'
if log.exists(): log.unlink()
run=subprocess.run([str(directory/fixture.name)],cwd=directory,capture_output=True,text=True,timeout=10)
assert run.returncode==0,(run.returncode,run.stdout,run.stderr)
assert 'unsupported_executable' in log.read_text()
original=PEImage(PROJECT.parent/'SAN14CE_KO.exe')
nt=struct.unpack_from('<I',original.data,0x3c)[0]
rva,size=struct.unpack_from('<II',original.data,nt+24+112+5*8)
relocations=original.read(rva,size); text_relocations=[]; pos=0
while pos+8<=len(relocations):
    page,blocksize=struct.unpack_from('<II',relocations,pos)
    if blocksize<8: break
    for at in range(pos+8,pos+blocksize,2):
        entry=struct.unpack_from('<H',relocations,at)[0]
        address=page+(entry&0xfff)
        if entry>>12 and 0x1000<=address<0x1000+20179374: text_relocations.append(hex(address))
    pos+=blocksize
assert not text_relocations, 'Full text guard needs ASLR normalization: '+str(text_relocations[:10])
report['loader_fixture_status']='passed'
report['text_absolute_relocations']=len(text_relocations)
report['game_startup_validated']=False
(PROJECT/'build/package_verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(run.stdout.strip()); print('Original text has no ASLR relocations; full text guard is stable across image bases.')
