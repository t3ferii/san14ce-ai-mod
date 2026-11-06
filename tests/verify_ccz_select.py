"""Integration test of the installed manager Select -> CCZ / speed layout transaction on copies.

2026-11-05 local4 (user: outdated checks are brought up to date, never weakened). It no longer needs an old release zip:
  * the manager scripts are the CURRENT distribution/ files (Manage.ps1 -> 관리.ps1, SplitManage.ps1, FeatureSelect.ps1);
  * the package is the CURRENT staged package (newest build/local_stage_* whose manifest hashes match its files, else
    build/input_package; `--package <dir>` overrides);
  * every known game build of the table is run in its own temporary game folder: Steam buildid 25953386 (1a4a447d,
    TimeDateStamp 0x6AD06F53: the REAL current SAN14CE_KO.exe is copied) and the previous build (1fbe6be7, 0x6A983C25: a header-only
    PE that satisfies the manager's PE gate, its sections are not read for this build).
The checks themselves are unchanged: ON -> OFF -> ON, groups/ccz OFF and the speed switch restore the exact original entries,
Restore returns the full original index, unrelated records and the DLLs stay byte for byte. The only per-build difference is the
count of applied records: a build whose DLL keeps traits9 off gets no nine-slot records (14/11 become 10/7 and the four nine-slot
KTIDs must stay original). local10 (2026-11-06, docs/LOCAL10_PORT_PLAN.md 4.1): traits9 is table-id based and ON in both builds, so
0x6AD06F53 now applies the same 14/11 records as 0x6A983C25 (Get-GameBuildLayoutOff returns nothing).
"""
from pathlib import Path
import base64, hashlib, json, re, shutil, struct, subprocess, tempfile, sys

P=Path(__file__).resolve().parents[1]
G=P.parent
TRAITS9=(0x49F89AB3,0x7C471B31,0x9EA0EB77,0xF1D669AF)
CCZ=(0x3DA1641D,0x757F1A41,0x5C046EA3)
SPEED=(0x301526D7,0xFEA5F28C,0x40F993CE,0x454A1BCC)
# build table: name, EXE TimeDateStamp, ScreenLayout records withheld by the DLL's traits9 gate
BUILDS=(('1a4a447d',0x6AD06F53,False),('1fbe6be7',0x6A983C25,False))   # local10 2026-11-06: traits9 runs on both builds

def records(data):
    at=struct.unpack_from('<I',data,8)[0]; result=[]
    for _ in range(struct.unpack_from('<I',data,16)[0]):
        size=struct.unpack_from('<I',data,at+8)[0]
        result.append((struct.unpack_from('<I',data,at+36)[0],data[at:at+size]))
        at+=(size+3)&~3
    assert at==len(data)
    return result

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def package_ok(d):
    m=d/'manifest.json'
    if not m.exists(): return False
    files=json.loads(m.read_text(encoding='utf-8-sig'))['split_modules']['files']
    return all((d/n).exists() and sha(d/n)==h for n,h in files.items()) and (d/'traits9_layout.json').exists()

def choose_package(arg):
    if arg: return Path(arg)
    stages=sorted((P/'build').glob('local_stage_*'),key=lambda q:q.stat().st_mtime,reverse=True)
    for d in stages+[P/'build/input_package']:
        if d.is_dir() and package_ok(d): return d
    raise SystemExit('no consistent package (build/local_stage_* or build/input_package)')

def pe_stub(stamp):
    """Header-only PE accepted by the manager's PE gate for the previous build (image size 0x29FF000)."""
    b=bytearray(0x400); b[0:2]=b'MZ'; struct.pack_into('<I',b,0x3C,0x80)
    struct.pack_into('<IH',b,0x80,0x4550,0x8664); struct.pack_into('<I',b,0x80+8,stamp)
    struct.pack_into('<H',b,0x80+20,0xF0); struct.pack_into('<H',b,0x80+24,0x20B); struct.pack_into('<I',b,0x80+80,0x029FF000)
    return bytes(b)

def run_build(name,stamp,traits9_off,package,report):
    root=Path(tempfile.mkdtemp(prefix='ccz_select_',dir=P/'build')).resolve()
    checks=0
    try:
        mod=root/'AI_TacticsMod'; payload=mod/'build/input_package'; payload.mkdir(parents=True)
        for q in package.iterdir():
            if q.is_file(): shutil.copyfile(q,payload/q.name)
        shutil.copyfile(package/'release_features.json',mod/'release_features.json')
        for src,dst in (('Manage.ps1','관리.ps1'),('SplitManage.ps1','SplitManage.ps1'),('FeatureSelect.ps1','FeatureSelect.ps1')):
            shutil.copyfile(P/'distribution'/src,mod/dst)
        diag=P/'distribution/Diagnose.ps1'
        if diag.exists(): shutil.copyfile(diag,mod/'진단.ps1')
        if stamp==0x6AD06F53:
            shutil.copyfile(G/'SAN14CE_KO.exe',root/'SAN14CE_KO.exe')
            head=(root/'SAN14CE_KO.exe').read_bytes()[:0x400]; pe=struct.unpack_from('<I',head,0x3C)[0]
            assert struct.unpack_from('<I',head,pe+8)[0]==stamp,'the installed game EXE is not the build table entry '+name
        else:
            (root/'SAN14CE_KO.exe').write_bytes(pe_stub(stamp))
        patch=json.loads((payload/'traits9_layout.json').read_text(encoding='utf-8-sig'))
        originals={r['ktid']:base64.b64decode(r['original_entry']) for r in patch['records']}
        live=(G/'0002/ScreenLayout.rdb').read_bytes()
        head=struct.unpack_from('<I',live,8)[0]
        chunks=[live[:head]]
        for k,entry in records(live):
            entry=originals.get(k,entry); chunks.extend((entry,b'\0'*((-len(entry))%4)))
        original=b''.join(chunks)
        (root/'0002').mkdir(); index=root/'0002/ScreenLayout.rdb';index.write_bytes(original)
        full=len(patch['records']); t9=len(TRAITS9) if traits9_off else 0
        def run(mode):
            result=subprocess.run(['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(mod/'관리.ps1'),'-Action',mode,'-GameDirectory',str(root),'-NonInteractive'],capture_output=True,timeout=120)
            assert result.returncode==0,name+' '+mode+': '+result.stdout.decode('utf-8',errors='replace')+result.stderr.decode('utf-8',errors='replace')
        run('Apply')
        settings=mod/'기능설정.ini'
        for mode in (1,0,1,0):
            s=settings.read_text(encoding='utf-16')
            s,n=re.subn(r'(?m)^ccz_sprites=\d+',f'ccz_sprites={mode}',s);assert n==1
            settings.write_text(s,encoding='utf-16')
            before={n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in ('dinput8.dll','SAN14_CombatAI.dll','SAN14_Convenience.dll','SAN14_CCZ.dll')}
            run('Select')
            state=json.loads((mod/'backup/input_state.json').read_text(encoding='utf-8-sig'))
            assert len(state['layout']['applied'])==((full if mode else full-len(CCZ))-t9),name+' applied count';checks+=1
            assert all(hashlib.sha256((root/n).read_bytes()).hexdigest()==h for n,h in before.items());checks+=1
            current=dict(records(index.read_bytes()))
            if not mode:
                assert all(current[k]==originals[k] for k in CCZ);checks+=1
            if traits9_off:
                assert all(current[k]==originals[k] for k in TRAITS9),name+' nine-slot records must stay original';checks+=1
            else:
                assert all(current[k]!=originals[k] for k in TRAITS9);checks+=1
        # user 2026-11-03: speed UI must follow its own switch even with CCZ off.
        for mode in (1,0):
            s=settings.read_text(encoding='utf-16')
            s,n=re.subn(r'(?m)^speed_buttons=\d+',f'speed_buttons={mode}',s);assert n==1
            settings.write_text(s,encoding='utf-16')
            run('Select')
            current=dict(records(index.read_bytes()))
            for k in SPEED:
                assert (current[k]==originals[k])==(mode==0);checks+=1
        run('Restore');assert index.read_bytes()==original;checks+=1
        report[name]=checks
    finally:
        assert root.is_relative_to((P/'build').resolve()) and root.name.startswith('ccz_select_')
        shutil.rmtree(root)
    return checks

def main():
    package=choose_package(sys.argv[1] if len(sys.argv)>1 else '')
    report={}; total=0
    for name,stamp,off in BUILDS:
        total+=run_build(name,stamp,off,package,report)
    print(json.dumps(dict(status='ccz_select_transaction_passed',checks=total,per_build=report,package=str(package.relative_to(P)),actual_game_changed=False)))

if __name__=='__main__':main()
