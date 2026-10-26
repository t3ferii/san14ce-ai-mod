"""Shared guarded LOCAL install for every session (docs/SESSION_RULES.md). No shared ZIP release.

  python -B -X utf8 tools/install_local.py prepare <version> --modules combat[,convenience] [--gui] --note "<what>"
  python -B -X utf8 tools/install_local.py install <version>

Only the listed modules are replaced from build/development/loader_split_diagnostic (build them first
with tools/build_split.ps1 -RuntimeDiagnostics); every other installed DLL is kept byte-for-byte.
--gui replaces the installer with build/development/installer (tools/build_installer.ps1).
--layout <folder> replaces the ScreenLayout bank (traits9_layout.bin/.json, a superset of the staged records).
Backup -> stage -> manifest -> guarded Apply (관리.ps1) -> verification; any failure restores the backup.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse, configparser, hashlib, json, shutil, subprocess, uuid
from package_mod import atomic_write, no_game, ORIGINAL_SHA
from install_damage_preview import exports, windows_script

P=Path(__file__).resolve().parents[1]; G=P.parent
NAMES={'loader':'dinput8.dll','combat':'SAN14_CombatAI.dll','convenience':'SAN14_Convenience.dll'}
EXPORTS={'combat':['AIInitialize','SAN14CombatGetApi','SAN14CombatTraceStep'],
         'convenience':['AIInitialize','SAN14ConvenienceInitialize']}
DIAG=P/'build/development/loader_split_diagnostic'; GUI_BUILD=P/'build/development/installer'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p): return json.loads(p.read_text(encoding='utf-8-sig'))
def save(p,v): atomic_write(p,(json.dumps(v,ensure_ascii=False,indent=2)+'\n').encode('utf-8'))
def proof_path(version): return P/('build/'+version.replace('.','_')+'_installation.json')
def stage_path(version): return P/('build/local_stage_'+version.replace('.','_'))
def sources():
    paths=[q for q in (P/'src').rglob('*') if q.suffix in ('.cpp','.h','.inl','.asm')]
    paths+=[P/n for n in ('installer/InstallerGui.cs','distribution/FeatureSelect.ps1','distribution/Manage.ps1','distribution/SplitManage.ps1')]
    return {q.relative_to(P).as_posix():sha(q) for q in paths}
def settings():
    raw=(P/'기능설정.ini').read_bytes()
    text=raw.decode('utf-16' if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig')
    for sec in ('combat','diagnostics','groups'):
        assert text.count('['+sec+']')<=1,'Duplicate ['+sec+'] section in 기능설정.ini'
    c=configparser.ConfigParser(interpolation=None,strict=False);c.read_string(text)
    return {s+'.'+k:v for s in c.sections() for k,v in c.items(s)}
def unchanged(p):
    no_game(); assert sha(G/'SAN14CE_KO.exe')==ORIGINAL_SHA
    assert sources()==p['sources'],'Source changed after preparation; rebuild and prepare again'
    for name,h in p['before_hashes'].items(): assert sha(G/name)==h,'Concurrent update detected: '+name
def prepare(a):
    no_game(); proof=proof_path(a.version)
    assert not proof.exists() or read(proof)['status']!='installed_and_verified','Version already installed'
    modules=[m for m in a.modules.split(',') if m]; assert modules or a.gui or a.layout,'Nothing to install'
    assert all(m in EXPORTS for m in modules),'Modules: combat, convenience'
    assert sha(G/'SAN14CE_KO.exe')==ORIGINAL_SHA
    state=read(P/'backup/input_state.json'); manifest=read(P/'build/input_package/manifest.json')
    assert state['installed'] and state['release_version']==manifest['release_version']
    assert all(sha(G/n)==manifest['split_modules']['files'][n] for n in NAMES.values()),'Installed DLLs differ from Apply package'
    settings()
    backup=P/'backup'/('local_install_'+uuid.uuid4().hex); backup.mkdir()
    paths=[*(G/n for n in NAMES.values()),G/manifest['gui_installer']['file'],G/'0002/ScreenLayout.rdb',
        P/'backup/input_state.json',P/'기능설정.ini',P/'diagnostics.ini',P/'progress_native.ini',
        P/'관리.ps1',P/'SplitManage.ps1',P/'FeatureSelect.ps1']
    paths+=[q for q in (P/'build/input_package').iterdir() if q.is_file()]
    hashes={}
    for q in paths:
        if not q.exists(): continue
        rel=q.relative_to(G).as_posix(); dst=backup/'files'/rel; dst.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(q,dst); assert sha(dst)==sha(q); hashes[rel]=sha(q)
    stage=stage_path(a.version)
    if stage.exists(): shutil.rmtree(stage)
    shutil.copytree(P/'build/input_package',stage)
    layout=None
    if a.layout:   # 2026-10-23: a rebuilt ScreenLayout bank (e.g. research/lord_change/layout_patch) replaces the staged one
        src=P/a.layout; j=read(src/'traits9_layout.json'); assert sha(src/'traits9_layout.bin')==j['bank_sha256'],'Layout bank hash mismatch'
        old=read(stage/'traits9_layout.json')
        # 2026-10-24 expert buttons: a bank may rebuild only the KTIDs it names in replaces_ktids.
        replaced=set(j.get('replaces_ktids',[])); assert replaced<={r['ktid'] for r in j['records']}
        assert all(r in j['records'] or r['ktid'] in replaced for r in old['records']),'Layout bank must keep every staged record'
        for n in ('traits9_layout.bin','traits9_layout.json'): shutil.copyfile(src/n,stage/n)
        m=read(stage/'manifest.json'); m['layout_patch'].update(bank_sha256=j['bank_sha256'],json_sha256=sha(stage/'traits9_layout.json'),
            merge=str(len(j['records']))+' ktid entries'); save(stage/'manifest.json',m)
        layout=dict(source=a.layout,bank_sha256=j['bank_sha256'],records=len(j['records']))
    save(proof,dict(status='prepared',version=a.version,baseline_version=manifest['release_version'],modules=modules,
        gui=a.gui,note=a.note,layout=layout,stage=str(stage),stage_backup=str(backup),before_hashes=hashes,sources=sources(),
        settings_before=settings(),installed=False,code_signed=False,gameplay_verified=False))
    print(json.dumps(dict(status='prepared',version=a.version,modules=modules,gui=a.gui,backup=str(backup)),ensure_ascii=False))
def install(a):
    proof=proof_path(a.version); p=read(proof); assert p['status']=='prepared'; unchanged(p)
    stage=Path(p['stage']); manifest=read(stage/'manifest.json'); gui=manifest['gui_installer']['file']
    newest=max((P/n).stat().st_mtime for n in p['sources'])
    for m in p['modules']:
        f=DIAG/NAMES[m]; assert f.stat().st_mtime>=newest,'Stale build: '+f.name
        assert exports(f)==EXPORTS[m],'Unexpected exports: '+f.name
        assert b'day=' in f.read_bytes(),'Diagnostic variant required: '+f.name
    if p['gui']: assert (GUI_BUILD/gui).stat().st_mtime>=newest,'Stale installer build'
    for key,name in NAMES.items():   # listed modules from the build, everything else as installed
        shutil.copyfile(DIAG/name if key in p['modules'] else G/name,stage/name)
    shutil.copyfile(GUI_BUILD/gui if p['gui'] else G/gui,stage/gui)
    hashes={n:sha(stage/n) for n in NAMES.values()}
    manifest.update(release_version=a.version,local_diagnostic_only=True,dll_sha256=hashes['dinput8.dll'])
    manifest['split_modules'].update(files=hashes,runtime_log_writers_compiled_out=False)
    by_module=manifest['split_modules'].setdefault('source_sha256_by_module',{})
    for m in p['modules']: by_module[NAMES[m]]=p['sources']
    manifest['gui_installer']['sha256']=sha(stage/gui)
    manifest['feature_switches']['count']=(P/'distribution/FeatureSelect.ps1').read_text(encoding='utf-8-sig').count('@{Group=')
    manifest.setdefault('local_changes',[]).append(dict(version=a.version,modules=p['modules'],gui=p['gui'],note=p['note'],
        date=datetime.now(timezone.utc).isoformat()))
    manifest['region_officers']['release_proof']=proof.relative_to(P).as_posix()
    save(stage/'manifest.json',manifest)
    backup=Path(p['stage_backup']); unchanged(p)
    try:
        for q in stage.iterdir():
            if q.is_file(): atomic_write(P/'build/input_package'/q.name,q.read_bytes())
        for target,source in (('관리.ps1','Manage.ps1'),('SplitManage.ps1','SplitManage.ps1'),('FeatureSelect.ps1','FeatureSelect.ps1')):
            windows_script(P/'distribution'/source,P/target)
        no_game()
        r=subprocess.run(['powershell.exe','-NoProfile','-STA','-ExecutionPolicy','Bypass','-File',str(P/'관리.ps1'),
            '-Action','Apply','-NonInteractive'],cwd=G,capture_output=True)
        log=P/('build/development/'+a.version.replace('.','_')+'_install.log'); log.write_bytes(r.stdout+r.stderr)
        assert r.returncode==0,'Guarded Apply failed; see '+str(log)
        if p['gui']: atomic_write(G/gui,(stage/gui).read_bytes())
        assert all(sha(G/n)==sha(P/'build/input_package'/n)==h for n,h in hashes.items())
        after=settings(); assert all(after.get(k)==v for k,v in p['settings_before'].items()),'Existing setting changed'
        assert sha(G/'SAN14CE_KO.exe')==ORIGINAL_SHA and sha(G/gui)==manifest['gui_installer']['sha256']
        for n in ('0002/ScreenLayout.rdb',):   # the index changes only with a new layout bank (--layout)
            if n in p['before_hashes'] and not p.get('layout'): assert sha(G/n)==p['before_hashes'][n]
        current=read(P/'backup/input_state.json'); assert current['release_version']==a.version and current['installed']
    except BaseException:
        no_game()
        for name,h in p['before_hashes'].items():
            # A running, unchanged GUI must not prevent the remaining rollback.
            if (G/name).exists() and sha(G/name)==h: continue
            atomic_write(G/name,(backup/'files'/name).read_bytes())
        p.update(status='rolled_back',installed=False); save(proof,p); raise
    p.update(status='installed_and_verified',installed=True,files=hashes,install_backup=current['install_backup'],
        installed_at=datetime.now(timezone.utc).isoformat(),settings_preserved=True,original_exe_unchanged=True,
        local_only=True,shared_zip_created=False,installed_apply_match=True)
    save(proof,p); print(json.dumps({k:p[k] for k in ('status','version','modules','gui','settings_preserved')},ensure_ascii=False))
def recover_prepared(proof,p):
    """Recover a known interrupted DLL/GUI transaction, including partial rollback.

    User 2026-10-26: a locked, unmodified GUI interrupted rollback after the DLLs
    were restored but before Apply metadata. Validate old/staged ownership first.
    """
    assert p['status']=='prepared' and not p.get('layout'),'Prepared layout recovery requires its layout-specific evidence'
    no_game(); assert sha(G/'SAN14CE_KO.exe')==ORIGINAL_SHA
    backup=Path(p['stage_backup']).resolve();stage=Path(p['stage']).resolve()
    assert backup.is_relative_to((P/'backup').resolve()) and stage.is_relative_to((P/'build').resolve())
    versions={p['version'],p['baseline_version']}
    manifest=read(P/'build/input_package/manifest.json');state=read(P/'backup/input_state.json')
    assert manifest['release_version'] in versions and state['release_version'] in versions,'Another install is current'
    owned={NAMES[m] for m in p['modules']}
    assert all(sha(G/n)==p['before_hashes'][n] for n in NAMES.values() if n not in owned),'Another module changed'
    config={'AI_TacticsMod/기능설정.ini','AI_TacticsMod/diagnostics.ini','AI_TacticsMod/progress_native.ini'}
    contents={};before={};saved_settings=settings()
    for name,h in p['before_hashes'].items():
        target=(G/name).resolve();source=(backup/'files'/name).resolve()
        assert target.is_relative_to(G.resolve()) and source.is_relative_to(backup)
        assert sha(source)==h,'Backup hash mismatch: '+name
        current=sha(target)
        if current==h or name in config: continue
        allowed={h}
        if name in owned or (p.get('gui') and name==manifest['gui_installer']['file']):
            allowed.add(sha(stage/name))
        elif name.startswith('AI_TacticsMod/build/input_package/'):
            allowed.add(sha(stage/Path(name).name))
        elif name=='AI_TacticsMod/backup/input_state.json':
            assert read(target)['release_version'] in versions
            allowed.add(current)
        assert current in allowed,'Unrecognized concurrent update: '+name
        contents[name]=source.read_bytes();before[name]=target.read_bytes()
    written=[]
    try:
        for name,data in contents.items():atomic_write(G/name,data);written.append(name)
        old=read(P/'build/input_package/manifest.json');old_state=read(P/'backup/input_state.json')
        assert old['release_version']==old_state['release_version']==p['baseline_version']
        assert all(sha(G/n)==sha(P/'build/input_package'/n)==h for n,h in old['split_modules']['files'].items())
        assert settings()==saved_settings
    except BaseException:
        for name in reversed(written):atomic_write(G/name,before[name])
        raise
    p.update(status='rolled_back_prepared_install',installed=False,recovered_to=p['baseline_version'],recovery_settings_preserved=True)
    save(proof,p)
    print(json.dumps({'status':p['status'],'restored_version':p['baseline_version'],'settings_preserved':True}))

def rollback(a):
    """Undo the current last install from its verified backup, preserving settings.

    2026-10-24: expert layout local3 crashed before the added button factory.
    This uses the shared installer, never a session-specific copying script.
    """
    no_game(); proof=proof_path(a.version); p=read(proof)
    if p['status']=='prepared':return recover_prepared(proof,p)
    assert p['status']=='installed_and_verified' and p.get('installed')
    current=read(P/'build/input_package/manifest.json'); state=read(P/'backup/input_state.json')
    assert current['release_version']==state['release_version']==a.version
    assert all(sha(G/n)==p['files'][n]==current['split_modules']['files'][n] for n in NAMES.values()),'Concurrent install detected'
    backup=Path(p['stage_backup']).resolve(); assert backup.is_relative_to((P/'backup').resolve())
    owned={NAMES[m] for m in p['modules']}
    # Other modules must already equal the prior version and remain untouched.
    assert all(p['before_hashes'][n]==p['files'][n] for n in NAMES.values() if n not in owned)
    restore=[n for n in p['before_hashes'] if n.startswith('AI_TacticsMod/build/input_package/')]
    restore+=sorted(owned)+['AI_TacticsMod/backup/input_state.json']
    if p.get('layout'): restore+=['0002/ScreenLayout.rdb']
    if p.get('gui'): restore+=[current['gui_installer']['file']]
    contents={}
    for name in restore:
        source=backup/'files'/name
        assert sha(source)==p['before_hashes'][name],'Backup hash mismatch: '+name
        contents[name]=source.read_bytes()
    settings_before=settings()
    protected={n:sha(G/n) for n in NAMES.values() if n not in owned}
    before={n:(G/n).read_bytes() for n in restore}
    try:
        for name in restore: atomic_write(G/name,contents[name])
        old=read(P/'build/input_package/manifest.json'); old_state=read(P/'backup/input_state.json')
        assert old['release_version']==old_state['release_version']==p['baseline_version']
        assert all(sha(G/n)==sha(P/'build/input_package'/n)==h for n,h in old['split_modules']['files'].items())
        assert settings()==settings_before
        assert all(sha(G/n)==h for n,h in protected.items())
        assert sha(G/'SAN14CE_KO.exe')==ORIGINAL_SHA
        if p.get('layout'):
            # Prior screen records are a byte-for-byte prefix of the additive
            # bank. Validate every indexed record against the restored bundle.
            import sys
            sys.path.insert(0,str(P/'research/speed_buttons'))
            import g1t
            g1t._rows=None
            layout=read(P/'build/input_package/traits9_layout.json')
            bank=(P/'build/input_package/traits9_layout.bin').read_bytes()
            assert hashlib.sha256(bank).hexdigest()==layout['bank_sha256']
            for r in layout['records']:assert g1t.raw_entry(r['ktid'])==bank[r['offset']:r['offset']+r['size']]
    except BaseException:
        for name in reversed(restore): atomic_write(G/name,before[name])
        raise
    p.update(status='rolled_back_user_report',installed=False,rolled_back_to=p['baseline_version'],
        rollback_settings_preserved=True,rollback_protected_modules=protected)
    save(proof,p)
    print(json.dumps({'status':p['status'],'restored_version':p['baseline_version'],
        'settings_preserved':True,'other_modules_preserved':True,'layout_restored':bool(p.get('layout'))}))

if __name__=='__main__':
    ap=argparse.ArgumentParser(); ap.add_argument('action',choices=('prepare','install','rollback')); ap.add_argument('version')
    ap.add_argument('--modules',default=''); ap.add_argument('--gui',action='store_true'); ap.add_argument('--note',default='')
    ap.add_argument('--layout',default='',help='folder with a rebuilt traits9_layout.bin/.json (keeps every staged record)')
    a=ap.parse_args(); {'prepare':prepare,'install':install,'rollback':rollback}[a.action](a)
