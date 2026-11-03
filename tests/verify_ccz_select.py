"""Integration test of installed manager Select -> CCZ layout transaction on copies."""
from pathlib import Path
import base64, hashlib, json, re, shutil, struct, subprocess, tempfile, zipfile, sys

P=Path(__file__).resolve().parents[1]
G=P.parent

def records(data):
    at=struct.unpack_from('<I',data,8)[0]; result=[]
    for _ in range(struct.unpack_from('<I',data,16)[0]):
        size=struct.unpack_from('<I',data,at+8)[0]
        result.append((struct.unpack_from('<I',data,at+36)[0],data[at:at+size]))
        at+=(size+3)&~3
    assert at==len(data)
    return result

def main():
    report=json.loads((Path(sys.argv[1]) if len(sys.argv)>1 else P/'build/share_package_ccz_layout_local1.json').read_text(encoding='utf-8'))
    root=Path(tempfile.mkdtemp(prefix='ccz_select_',dir=P/'build')).resolve()
    checks=0
    try:
        with zipfile.ZipFile(report['zip']) as z:
            for name in z.namelist():
                target=(root/name).resolve(); assert target.is_relative_to(root)
                target.parent.mkdir(parents=True,exist_ok=True); target.write_bytes(z.read(name))
        shutil.copyfile(G/'SAN14CE_KO.exe',root/'SAN14CE_KO.exe')
        payload=root/'AI_TacticsMod/build/input_package'
        patch=json.loads((payload/'traits9_layout.json').read_text(encoding='utf-8-sig'))
        originals={r['ktid']:base64.b64decode(r['original_entry']) for r in patch['records']}
        live=(G/'0002/ScreenLayout.rdb').read_bytes()
        head=struct.unpack_from('<I',live,8)[0]
        chunks=[live[:head]]
        for k,entry in records(live):
            entry=originals.get(k,entry); chunks.extend((entry,b'\0'*((-len(entry))%4)))
        original=b''.join(chunks)
        (root/'0002').mkdir(); index=root/'0002/ScreenLayout.rdb';index.write_bytes(original)
        def run(mode):
            result=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(root/'AI_TacticsMod/관리.ps1'),'-Action',mode,'-GameDirectory',str(root),'-NonInteractive'],capture_output=True,timeout=60)
            assert result.returncode==0,result.stdout.decode('utf-8',errors='replace')+result.stderr.decode('utf-8',errors='replace')
        run('Apply')
        settings=root/'AI_TacticsMod/기능설정.ini'
        for mode in (1,0,1,0):
            s=settings.read_text(encoding='utf-16')
            s,n=re.subn(r'(?m)^ccz_sprites=\d+',f'ccz_sprites={mode}',s);assert n==1
            settings.write_text(s,encoding='utf-16')
            before={n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in ('dinput8.dll','SAN14_CombatAI.dll','SAN14_Convenience.dll','SAN14_CCZ.dll')}
            run('Select')
            state=json.loads((root/'AI_TacticsMod/backup/input_state.json').read_text(encoding='utf-8-sig'))
            assert len(state['layout']['applied'])==(14 if mode else 11);checks+=1
            assert all(hashlib.sha256((root/n).read_bytes()).hexdigest()==h for n,h in before.items());checks+=1
            current=dict(records(index.read_bytes()))
            if not mode:
                assert all(current[k]==originals[k] for k in (0x3DA1641D,0x757F1A41,0x5C046EA3));checks+=1
        # user 2026-11-03: speed UI must follow its own switch even with CCZ off.
        for mode in (1,0):
            s=settings.read_text(encoding='utf-16')
            s,n=re.subn(r'(?m)^speed_buttons=\d+',f'speed_buttons={mode}',s);assert n==1
            settings.write_text(s,encoding='utf-16')
            run('Select')
            current=dict(records(index.read_bytes()))
            for k in (0x301526D7,0xFEA5F28C,0x40F993CE,0x454A1BCC):
                assert (current[k]==originals[k])==(mode==0);checks+=1
        run('Restore');assert index.read_bytes()==original;checks+=1
        print(json.dumps(dict(status='ccz_select_transaction_passed',checks=checks,actual_game_changed=False)))
    finally:
        assert root.is_relative_to((P/'build').resolve()) and root.name.startswith('ccz_select_')
        shutil.rmtree(root)

if __name__=='__main__':main()
