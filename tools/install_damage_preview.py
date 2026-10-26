"""Guarded local UI update, preserving the user's current diagnostic combat DLL.

prepare -> build_split.ps1 -RuntimeDiagnostics -> install. No shared ZIP release.
"""
from pathlib import Path
from datetime import datetime, timezone
import configparser, hashlib, json, shutil, struct, subprocess, sys, uuid
from package_mod import atomic_write, no_game, ORIGINAL_SHA

P=Path(__file__).resolve().parents[1]; G=P.parent
STAGE=P/'build/damage_preview_local_package'; PROOF=P/'build/damage_preview_installation.json'
VERSION='v12.21-damage-preview-local1'
NAMES=('dinput8.dll','SAN14_CombatAI.dll','SAN14_Convenience.dll')
EXPECTED=dict(zip(NAMES,('292b12e63f5d30157c12eb454e300a9d4c41f7dbd84736fa50019c011bb54017',
    '6eb8993917d07a2d9d8fe5bac06e7ca686f4c3efe23b80c4e7212741c655c346',
    'a0d0ebfc2141ab2ea96b754429b51587843e65f9ecd27c9509a7fc88217b103b')))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p): return json.loads(p.read_text(encoding='utf-8-sig'))
def save(p,value): atomic_write(p,(json.dumps(value,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
def sources():
    paths=[q for q in (P/'src').rglob('*') if q.suffix in ('.cpp','.h','.inl','.asm')]
    paths += [P/n for n in ('installer/InstallerGui.cs','distribution/FeatureSelect.ps1','distribution/Manage.ps1','distribution/SplitManage.ps1')]
    return {q.relative_to(P).as_posix():sha(q) for q in paths}
def settings():
    c=configparser.ConfigParser(interpolation=None,strict=False)
    data=(P/'기능설정.ini').read_bytes()
    c.read_string(data.decode('utf-16' if data.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig'))
    return {s+'.'+k:v for s in c.sections() for k,v in c.items(s)}
def check_current(p):
    no_game(); assert sha(G/'SAN14CE_KO.exe')==ORIGINAL_SHA
    assert sources()==p['sources'], 'Source changed after preparation'
    for name,h in p['before_hashes'].items():
        assert sha(G/name)==h, 'Baseline changed: '+name
def windows_script(source,target):
    text=source.read_text(encoding='utf-8-sig').replace('\r\n','\n').replace('\n','\r\n')
    atomic_write(target,b'\xef\xbb\xbf'+text.encode('utf-8'))
def prepare():
    no_game(); assert not PROOF.exists()
    if STAGE.exists():
        assert all(q.is_file() and (P/'build/input_package'/q.name).exists() and
            sha(q)==sha(P/'build/input_package'/q.name) for q in STAGE.iterdir()), 'Incomplete stage differs'
    assert sha(G/'SAN14CE_KO.exe')==ORIGINAL_SHA
    assert all(sha(G/n)==h for n,h in EXPECTED.items()), 'Installed baseline changed'
    diag=P/'build/development/loader_split_diagnostic'
    assert all(sha(diag/n)==EXPECTED[n] for n in NAMES[1:]), 'Unknown diagnostic baseline'
    candidate=read(P/'build/development/damage_preview/verification.json')
    assert candidate['status']=='development_candidate_verified' and candidate['fixture']['checks']==70
    assert all(sha(P/n)==h for n,h in candidate['sources'].items()), 'Recheck changed candidate'
    state=read(P/'backup/input_state.json'); manifest=read(P/'build/input_package/manifest.json')
    assert state['installed'] and state['release_version']==manifest['release_version']=='v12.21'
    backup=P/'backup'/('damage_preview_install_'+uuid.uuid4().hex); backup.mkdir()
    paths=[*(G/n for n in NAMES),G/manifest['gui_installer']['file'],G/'0002/ScreenLayout.rdb',
        P/'backup/input_state.json',P/'기능설정.ini',P/'diagnostics.ini',
        P/'관리.ps1',P/'SplitManage.ps1',P/'FeatureSelect.ps1']
    paths += [q for q in (P/'build/input_package').iterdir() if q.is_file()]
    hashes={}
    for q in paths:
        if not q.exists(): continue
        rel=q.relative_to(G).as_posix(); dst=backup/'files'/rel; dst.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(q,dst); assert sha(dst)==sha(q); hashes[rel]=sha(q)
    shutil.copytree(P/'build/input_package',STAGE,dirs_exist_ok=True)
    p=dict(status='prepared',version=VERSION,baseline_version='v12.21',stage=str(STAGE),stage_backup=str(backup),
        before_hashes=hashes,sources=sources(),settings_before=settings(),baseline_dlls=EXPECTED,
        existing_diagnostic_baseline_verified=True,installed=False,code_signed=False,gameplay_verified=False)
    save(PROOF,p); print(json.dumps(dict(status=p['status'],backup=str(backup))))
def exports(path):
    from audit_native_game import PEImage
    pe=PEImage(path); opt=struct.unpack_from('<I',pe.data,0x3c)[0]+24
    header=pe.read(struct.unpack_from('<I',pe.data,opt+112)[0],40)
    count,table=struct.unpack_from('<I',header,24)[0],struct.unpack_from('<I',header,32)[0]
    return sorted(pe.read(struct.unpack('<I',pe.read(table+i*4,4))[0],80).split(b'\0')[0].decode() for i in range(count))
def install():
    p=read(PROOF); assert p['status']=='prepared'; check_current(p)
    dll=P/'build/development/loader_split_diagnostic/SAN14_Convenience.dll'
    assert dll.stat().st_mtime>=max((P/n).stat().st_mtime for n in p['sources'] if n.startswith('src/'))
    assert exports(dll)==['AIInitialize','SAN14ConvenienceInitialize']
    data=dll.read_bytes(); assert '예상 피해(1회)'.encode('utf-16le') in data and b'damage_preview_installed' in data
    assert b'other_module_hooks_installed' in data and b'day=' in data, 'Diagnostic variant required'
    shutil.copyfile(dll,STAGE/NAMES[2])
    # Keep the actual combat module, including the other session's changes.
    for n in NAMES[:2]: shutil.copyfile(G/n,STAGE/n)
    manifest=read(STAGE/'manifest.json'); gui=manifest['gui_installer']['file']
    shutil.copyfile(P/'build/development/installer'/gui,STAGE/gui)
    hashes={n:sha(STAGE/n) for n in NAMES}
    manifest.update(release_version=VERSION,local_diagnostic_only=True,dll_sha256=hashes[NAMES[0]])
    manifest['split_modules'].update(files=hashes,runtime_log_writers_compiled_out=False)
    manifest['split_modules']['source_sha256']=p['sources']
    manifest['gui_installer']['sha256']=sha(STAGE/gui)
    manifest['feature_switches']['count']=45
    manifest['damage_preview']={'key':'officer_ui.damage_preview','default':1,'owner':NAMES[2],
        'fixture_checks':70,'native_ranges':12,'gameplay_verified':False,'local_install':True}
    manifest['region_officers']['release_proof']='build/damage_preview_installation.json'
    manifest['progress_log']={'runtime_logging_compiled_out':False,'enabled':True}
    save(STAGE/'manifest.json',manifest)
    backup=Path(p['stage_backup']); check_current(p)
    try:
        # The user-installed diagnostic DLLs predate this install and match the
        # existing diagnostic build exactly. Reconcile only their stale ownership
        # hashes, after backing up both actual bytes and the untouched old record.
        state=read(P/'backup/input_state.json')
        for n in NAMES[1:]: state['modules'][n]=EXPECTED[n]
        save(P/'backup/input_state.json',state)
        for q in STAGE.iterdir():
            if q.is_file(): atomic_write(P/'build/input_package'/q.name,q.read_bytes())
        for target,source in (('관리.ps1','Manage.ps1'),('SplitManage.ps1','SplitManage.ps1'),('FeatureSelect.ps1','FeatureSelect.ps1')):
            windows_script(P/'distribution'/source,P/target)
        no_game()
        r=subprocess.run(['powershell.exe','-NoProfile','-STA','-ExecutionPolicy','Bypass','-File',str(P/'관리.ps1'),
            '-Action','Apply','-NonInteractive'],cwd=G,capture_output=True)
        (P/'build/development/damage_preview/install.log').write_bytes(r.stdout+r.stderr)
        assert r.returncode==0, 'Guarded Apply failed; see install.log'
        atomic_write(G/gui,(STAGE/gui).read_bytes())
        assert all(sha(G/n)==sha(P/'build/input_package'/n)==h for n,h in hashes.items())
        after=settings(); assert all(after.get(k)==v for k,v in p['settings_before'].items()), 'Existing setting changed'
        assert after['officer_ui.damage_preview']=='1' and after['groups.convenience']=='1'
        assert sha(G/'SAN14CE_KO.exe')==ORIGINAL_SHA
        for n in ('0002/ScreenLayout.rdb','AI_TacticsMod/diagnostics.ini'):
            if n in p['before_hashes']: assert sha(G/n)==p['before_hashes'][n]
        assert sha(G/NAMES[1])==EXPECTED[NAMES[1]], 'Combat DLL changed'
        assert sha(G/gui)==manifest['gui_installer']['sha256']
        current=read(P/'backup/input_state.json'); assert current['release_version']==VERSION and current['installed']
    except BaseException:
        no_game()
        for name in p['before_hashes']: atomic_write(G/name,(backup/'files'/name).read_bytes())
        p.update(status='rolled_back',installed=False); save(PROOF,p); raise
    p.update(status='installed_and_verified',installed=True,files=hashes,install_backup=current['install_backup'],
        installed_at=datetime.now(timezone.utc).isoformat(),settings_preserved=True,damage_preview_enabled=True,
        original_exe_unchanged=True,combat_dll_unchanged=True,diagnostics_preserved=True,
        gui_sha256=sha(G/gui),local_only=True,shared_zip_created=False,installed_apply_match=True)
    save(PROOF,p); print(json.dumps({k:p[k] for k in ('status','version','combat_dll_unchanged','settings_preserved','damage_preview_enabled')}))
if __name__=='__main__': {'prepare':prepare,'install':install}[sys.argv[1]]()
