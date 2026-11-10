"""Shared guarded LOCAL install for every session (docs/SESSION_RULES.md). No shared ZIP release.

  python -B -X utf8 tools/install_local.py prepare <version> --modules loader,layout,combat,convenience,ccz [--gui] --note "<what>"
  python -B -X utf8 tools/install_local.py install <version>
  python -B -X utf8 tools/install_local.py switch-package <package folder> [--dry-run]

Only the listed modules are replaced from build/development/loader_split_diagnostic (build them first
with tools/build_split.ps1 -RuntimeDiagnostics); every other installed DLL is kept byte-for-byte.
--gui replaces the installer with build/development/installer (tools/build_installer.ps1).
--layout <folder> replaces the ScreenLayout bank (traits9_layout.bin/.json, a superset of the staged records).
Backup -> stage -> manifest -> guarded Apply (관리.ps1) -> verification; any failure restores the backup.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse, configparser, hashlib, json, shutil, subprocess, uuid
from package_mod import atomic_write, no_game, ORIGINAL_SHA, KNOWN_ORIGINAL_SHAS
from install_damage_preview import exports, windows_script

P=Path(__file__).resolve().parents[1]; G=P.parent
NAMES={'loader':'dinput8.dll','combat':'SAN14_CombatAI.dll','convenience':'SAN14_Convenience.dll','ccz':'SAN14_CCZ.dll','layout':'SAN14_Layout.dll'}
# 2026-11-05 user exception: SAN14_Layout.dll (5th DLL) and the dinput8 loader (loads Layout first) are modules too.
# 'loader' = dinput8.dll only; it is bundled with 'layout' in practice (loader + layout + the three feature DLLs = all five).
EXPORTS={'loader':['DirectInput8Create'],'layout':['SAN14LayoutGetApi','SAN14LayoutInitialize','SAN14LayoutPublish'],
         'combat':['AIInitialize','SAN14CombatGetApi','SAN14CombatTraceStep'],
         'convenience':['AIInitialize','SAN14ConvenienceInitialize'],'ccz':['SAN14CczGetApi']}
DIAG_EXPORTS={'ccz':['SAN14CczDiagCrumbs']}   # diagnostic-only crash breadcrumb export (2026-11-04)
def exports_ok(found,key): return found==EXPORTS[key] or sorted(found)==sorted(EXPORTS[key]+DIAG_EXPORTS.get(key,[]))
DIAG=P/'build/development/loader_split_diagnostic'; RELEASE=P/'build/development/loader_split'; GUI_BUILD=P/'build/development/installer'
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
    values={s+'.'+k:v for s in c.sections() for k,v in c.items(s)}
    # user 2026-11-03: compare preservation semantically across the explicit
    # installer migration. The original INI bytes remain in the transaction backup.
    legacy=values.pop('game_flow.fast_menu',None)
    if legacy is not None: values.setdefault('performance.fast_menu',legacy)
    values.pop('performance.strategy_batch',None)
    return values
def set_diagnostics_on():
    """[groups] diagnostics=1 in the settings INI (UTF-16 file kept as is; every other character unchanged)."""
    import re
    ini=P/'기능설정.ini'; raw=ini.read_bytes(); utf16=raw.startswith((b'\xff\xfe',b'\xfe\xff'))
    text=raw.decode('utf-16' if utf16 else 'utf-8-sig')
    start=text.index('[groups]'); end=text.find('\n[',start+1); end=len(text) if end<0 else end
    block,count=re.subn(r'(?m)^diagnostics\s*=.*?(\r?)$',lambda m:'diagnostics=1'+m.group(1),text[start:end])
    assert count==1,'[groups] diagnostics key not found once'
    new=text[:start]+block+text[end:]
    if new!=text:
        atomic_write(ini,new.encode('utf-16') if utf16 else (b'\xef\xbb\xbf' if raw.startswith(b'\xef\xbb\xbf') else b'')+new.encode('utf-8'))
def rdb_entries(data):
    """ScreenLayout.rdb index entries by KTID (same layout as distribution/SplitManage.ps1 Read-LayoutIndex)."""
    import struct
    at=struct.unpack_from('<I',data,8)[0]; out={}
    for _ in range(struct.unpack_from('<I',data,16)[0]):
        size=struct.unpack_from('<I',data,at+8)[0]
        out[struct.unpack_from('<I',data,at+36)[0]]=bytes(data[at:at+size]); at+=(size+3)&~3
    return out
def layout_index_change(before,after,records):
    """2026-11-05 local3: the guarded Apply may change the ScreenLayout index when a feature that is off in the running EXE build
    (traits9 on 1a4a447d, distribution/SplitManage.ps1 Get-GameBuildLayoutOff) gives its records back. Returns the changed KTIDs;
    every one must be a record of the package's own layout bundle, any other index change is a concurrent/foreign edit."""
    old,new=rdb_entries(before),rdb_entries(after)
    assert set(old)==set(new),'ScreenLayout index gained or lost entries'
    changed={k for k in old if old[k]!=new[k]}
    own={int(r['ktid']) for r in records}
    assert changed<=own,'ScreenLayout index changed outside the package records: '+', '.join('%08X'%k for k in sorted(changed-own))
    return changed
def unchanged(p):
    no_game(); assert sha(G/'SAN14CE_KO.exe')==ORIGINAL_SHA
    assert sources()==p['sources'],'Source changed after preparation; rebuild and prepare again'
    for name,h in p['before_hashes'].items(): assert sha(G/name)==h,'Concurrent update detected: '+name
    if p.get('fresh'): assert not any((G/n).exists() for n in NAMES.values() if n not in p['before_hashes']),'Concurrent update detected: mod DLL appeared'
def prepare(a):
    no_game(); proof=proof_path(a.version)
    assert not proof.exists() or read(proof)['status']!='installed_and_verified','Version already installed'
    modules=[m for m in a.modules.split(',') if m]; assert modules or a.gui or a.layout,'Nothing to install'
    assert all(m in EXPORTS for m in modules),'Modules: loader, layout, combat, convenience, ccz'
    assert sha(G/'SAN14CE_KO.exe')==ORIGINAL_SHA
    state=read(P/'backup/input_state.json'); manifest=read(P/'build/input_package/manifest.json')
    # 2026-11-05 user: a fresh install over a NOT installed baseline (a Steam update removed the mod DLLs; the install record says
    # installed:false) is supported. Every module is then new, rollback = remove the mod files, other mods' files are never touched.
    fresh=not state['installed']
    assert state['release_version']==manifest['release_version'],'Install record and Apply package versions differ'
    if fresh:
        assert set(modules)>=set(NAMES),'Fresh install needs every module: --modules loader,layout,combat,convenience,ccz'
        assert not any((G/n).exists() for n in NAMES.values()),'Mod DLL already present although the record says not installed'
    # 2026-11-01: SAN14_CCZ.dll is new; a baseline without it is installed with it once 'ccz' is listed.
    # A user may have manually installed a newer local DLL (2026-11-01 local14 CCZ). Never
    # treat it as the package baseline: only a module explicitly selected for replacement may
    # differ, its expected export surface must still match, and its exact bytes are backed up
    # and recorded for failure/user rollback. Every unselected module remains hash-strict.
    selected_overrides={}
    for key,name in NAMES.items():
        expected=manifest['split_modules']['files'].get(name)
        if expected is None: continue
        if fresh: continue
        actual=sha(G/name)
        if actual==expected: continue
        assert key in modules,'Installed DLL differs from Apply package: '+name
        assert exports_ok(exports(G/name),key),'Modified selected DLL has unexpected exports: '+name
        selected_overrides[name]={'sha256':actual,'package_sha256':expected,'size':(G/name).stat().st_size}
    assert fresh or all(n in manifest['split_modules']['files'] or k in modules for k,n in NAMES.items()),'Add the new module: --modules ...,ccz,layout'
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
        gui=a.gui,release=a.release,fresh=fresh,diagnostics_on=a.diagnostics_on,note=a.note,layout=layout,stage=str(stage),stage_backup=str(backup),before_hashes=hashes,sources=sources(),
        settings_before=settings(),preexisting_selected_overrides=selected_overrides,
        installed=False,code_signed=False,gameplay_verified=False))
    print(json.dumps(dict(status='prepared',version=a.version,modules=modules,gui=a.gui,backup=str(backup),
        preexisting_selected_overrides=selected_overrides),ensure_ascii=False))
def install(a):
    proof=proof_path(a.version); p=read(proof); assert p['status']=='prepared'; unchanged(p)
    stage=Path(p['stage']); manifest=read(stage/'manifest.json'); gui=manifest['gui_installer']['file']
    newest=max((P/n).stat().st_mtime for n in p['sources'])
    release=p.get('release',False); build=RELEASE if release else DIAG
    for m in p['modules']:
        f=build/NAMES[m]; assert f.stat().st_mtime>=newest,'Stale build: '+f.name
        assert (sorted(exports(f))==sorted(x for x in EXPORTS[m] if x!='SAN14CombatTraceStep') if release else exports_ok(exports(f),m)),'Unexpected exports: '+f.name
        # SAN14_CCZ.dll writes through the convenience module's log_event (no day= writer of its own).
        # (the loader and Layout DLLs have no day= writer either: Layout is checked by its own diagnostic exports)
        assert release or m in ('ccz','loader','layout') or b'day=' in f.read_bytes(),'Diagnostic variant required: '+f.name
    if p['gui']: assert (GUI_BUILD/gui).stat().st_mtime>=newest,'Stale installer build'
    for key,name in NAMES.items():   # listed modules from the build, everything else as installed
        if key not in p['modules'] and not (G/name).exists(): continue   # a module never installed (e.g. layout) is not staged
        shutil.copyfile(build/name if key in p['modules'] or (release and key=='loader') else G/name,stage/name)
    shutil.copyfile(GUI_BUILD/gui if p['gui'] else G/gui,stage/gui)
    hashes={n:sha(stage/n) for n in NAMES.values() if (stage/n).exists()}
    manifest.update(release_version=a.version,local_diagnostic_only=not release,dll_sha256=hashes['dinput8.dll'])
    manifest['supported_original_exe_sha256_all']=list(KNOWN_ORIGINAL_SHAS)   # 2026-11-05 build table; the legacy single field is kept
    if release: manifest['build_kind']='release'
    manifest['split_modules'].update(files=hashes)
    if p['modules']: manifest['split_modules']['runtime_log_writers_compiled_out']=release
    by_module=manifest['split_modules'].setdefault('source_sha256_by_module',{})
    for m in p['modules']: by_module[NAMES[m]]=p['sources']
    manifest['gui_installer']['sha256']=sha(stage/gui)
    manifest['feature_switches']['count']=(P/'distribution/FeatureSelect.ps1').read_text(encoding='utf-8-sig').count('@{Group=')
    # Native CCZ editor ships both-source catalog; directory/lazy reads keep game memory bounded.
    if 'ccz' in p['modules'] and (P/'assets/ccz/ccz_catalog.bin').exists():
        catalog=read(P/'assets/ccz/ccz_catalog.json')
        assert sha(P/'assets/ccz/ccz_catalog.bin')==catalog['sha256'],'CCZ catalog hash mismatch'
        manifest.setdefault('ccz_assets',{})['assets/ccz/ccz_catalog.bin']=catalog['sha256']
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
        # Hand a validated manual local override to the guarded manager without relaxing its
        # ownership check. The original state file is already in the transaction backup and is
        # restored by the exception path; only explicitly selected/replaced modules are adopted.
        overrides=p.get('preexisting_selected_overrides',{})
        if overrides:
            state_file=P/'backup/input_state.json'; owned=read(state_file)
            assert owned['release_version']==p['baseline_version'] and owned['installed']
            for name,info in overrides.items():
                assert name in [NAMES[m] for m in p['modules']]
                assert owned['modules'][name]==info['package_sha256'] and sha(G/name)==info['sha256']
                owned['modules'][name]=info['sha256']
            save(state_file,owned)
        no_game()
        r=subprocess.run(['powershell.exe','-NoProfile','-STA','-ExecutionPolicy','Bypass','-File',str(P/'관리.ps1'),
            '-Action','Apply','-NonInteractive'],cwd=G,capture_output=True)
        log=P/('build/development/'+a.version.replace('.','_')+'_install.log'); log.write_bytes(r.stdout+r.stderr)
        assert r.returncode==0,'Guarded Apply failed; see '+str(log)
        if p['gui']: atomic_write(G/gui,(stage/gui).read_bytes())
        assert all(sha(G/n)==sha(P/'build/input_package'/n)==h for n,h in hashes.items())
        if p.get('diagnostics_on'): set_diagnostics_on()   # user 2026-11-05: real-game log collection; no other key changes
        expected=dict(p['settings_before'])
        if p.get('diagnostics_on'): expected['groups.diagnostics']='1'
        after=settings(); assert all(after.get(k)==v for k,v in expected.items()),'Existing setting changed'
        assert sha(G/'SAN14CE_KO.exe')==ORIGINAL_SHA and sha(G/gui)==manifest['gui_installer']['sha256']
        current=read(P/'backup/input_state.json'); assert current['release_version']==a.version and current['installed']
        for n in ('0002/ScreenLayout.rdb',):   # the index changes only with a new layout bank (--layout)
            # user 2026-11-05: a fresh install (mod was removed) re-applies the existing layout bank through the guarded Apply,
            # which records it in input_state['layout']; that index change is expected. Upgrades keep the strict check.
            if p.get('fresh') and current.get('layout'): continue
            if n in p['before_hashes'] and not p.get('layout') and sha(G/n)!=p['before_hashes'][n]:
                # user 2026-11-05 (local3): feature OFF in this EXE build gives the screen records back. Only the package's own
                # records may differ from the backup; the change is recorded so that rollback restores the index.
                changed=layout_index_change((backup/'files'/n).read_bytes(),(G/n).read_bytes(),read(P/'build/input_package/traits9_layout.json')['records'])
                p['layout_index_changed']=sorted(changed)
    except BaseException:
        no_game()
        if p.get('fresh'):   # created mod files only (never any a file this installer does not own, e.g. the San14Sim session's version.dll)
            for n,h2 in hashes.items():
                if n not in p['before_hashes'] and (G/n).exists() and sha(G/n)==h2: (G/n).unlink()
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
    if p.get('release'): owned.add(NAMES['loader'])
    assert all(sha(G/n)==p['before_hashes'][n] for n in NAMES.values() if n not in owned and n in p['before_hashes']),'Another module changed'
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
        overrides=p.get('preexisting_selected_overrides',{})
        assert all(sha(P/'build/input_package'/n)==h and
            (not (G/n).exists() if p.get('fresh') else sha(G/n)==(p['before_hashes'][n] if n in overrides else h))
            for n,h in old['split_modules']['files'].items())
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
    assert all(sha(G/n)==p['files'][n]==current['split_modules']['files'][n] for n in NAMES.values() if n in p['files']),'Concurrent install detected'
    backup=Path(p['stage_backup']).resolve(); assert backup.is_relative_to((P/'backup').resolve())
    owned={NAMES[m] for m in p['modules']}
    if p.get('release'): owned.add(NAMES['loader'])
    # Other modules must already equal the prior version and remain untouched.
    assert all(p['before_hashes'][n]==p['files'][n] for n in NAMES.values() if n not in owned and n in p['before_hashes'])
    restore=[n for n in p['before_hashes'] if n.startswith('AI_TacticsMod/build/input_package/')]
    added=sorted(n for n in owned if n not in p['before_hashes'])   # e.g. SAN14_CCZ.dll over a baseline without it
    restore+=sorted(n for n in owned if n in p['before_hashes'])+['AI_TacticsMod/backup/input_state.json']
    if p.get('layout') or p.get('layout_index_changed'): restore+=['0002/ScreenLayout.rdb']   # local3: also when Apply gave records back
    if p.get('gui'): restore+=[current['gui_installer']['file']]
    contents={}
    for name in restore:
        source=backup/'files'/name
        assert sha(source)==p['before_hashes'][name],'Backup hash mismatch: '+name
        contents[name]=source.read_bytes()
    settings_before=settings()
    protected={n:sha(G/n) for n in NAMES.values() if n not in owned and (G/n).exists()}
    before={n:(G/n).read_bytes() for n in restore+added}
    try:
        for name in restore: atomic_write(G/name,contents[name])
        for name in added: (G/name).unlink()
        old=read(P/'build/input_package/manifest.json'); old_state=read(P/'backup/input_state.json')
        assert old['release_version']==old_state['release_version']==p['baseline_version']
        overrides=p.get('preexisting_selected_overrides',{})
        assert all(sha(P/'build/input_package'/n)==h and
            (not (G/n).exists() if p.get('fresh') else sha(G/n)==(p['before_hashes'][n] if n in overrides else h))
            for n,h in old['split_modules']['files'].items())
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
        for name in reversed(restore+added): atomic_write(G/name,before[name])
        raise
    p.update(status='rolled_back_user_report',installed=False,rolled_back_to=p['baseline_version'],
        rollback_settings_preserved=True,rollback_protected_modules=protected)
    save(proof,p)
    print(json.dumps({'status':p['status'],'restored_version':p['baseline_version'],
        'settings_preserved':True,'other_modules_preserved':True,'layout_restored':bool(p.get('layout')),
        'local_overrides_restored':sorted(p.get('preexisting_selected_overrides',{}))}))

SWITCH_SKIP_DIRS=('AI_TacticsMod/docs/',)
def switch_files(root):
    """Package files the switch copies: everything except the project documents (README/사용안내/docs), which stay as they are."""
    out=[]
    for q in sorted(root.rglob('*')):
        if not q.is_file(): continue
        rel=q.relative_to(root).as_posix()
        if rel.startswith(SWITCH_SKIP_DIRS): continue
        if rel.startswith('AI_TacticsMod/') and rel.count('/')==1 and rel.lower().endswith('.md'): continue
        out.append(rel)
    return out
def run_manager(action,tag):
    no_game()
    r=subprocess.run(['powershell.exe','-NoProfile','-STA','-ExecutionPolicy','Bypass','-File',str(P/'관리.ps1'),
        '-Action',action,'-GameDirectory',str(G),'-NonInteractive'],cwd=G,capture_output=True)
    log=P/('build/development/switch_'+tag+'_'+action.lower()+'.log'); log.parent.mkdir(parents=True,exist_ok=True)
    log.write_bytes(r.stdout+r.stderr)
    return r.returncode,log
def switch_package(a):
    """user 2026-11-10 (F4): install another released package (e.g. updates/v15.7.1/files) the way the installer's version
    change does it: the CURRENT 관리.ps1 Restore (original index/DLLs back), copy the package files (project documents are
    never overwritten), the PACKAGE's 관리.ps1 Apply, then verify the five DLLs, the installer and the state against the
    package manifest. Everything it overwrites is copied first to backup/switch_<version>_<time>/files."""
    src=Path(a.version).resolve()   # relative to the current directory
    assert (src/'AI_TacticsMod/관리.ps1').is_file() and (src/'AI_TacticsMod/build/input_package/manifest.json').is_file(),\
        'Not a release package folder (needs AI_TacticsMod/관리.ps1 and build/input_package/manifest.json): '+str(src)
    assert not src.is_relative_to(G.resolve()) or {'updates','build'} & set(src.relative_to(G.resolve()).parts),\
        'Package folder inside the game folder must be under an updates or build folder'
    target=read(src/'AI_TacticsMod/build/input_package/manifest.json'); version=target['release_version']
    files=switch_files(src)
    tag=version.replace('.','_')+'_'+datetime.now().strftime('%Y%m%d_%H%M%S')
    backup=P/'backup'/('switch_'+tag)
    keep=sorted(set(files)|{'AI_TacticsMod/기능설정.ini','AI_TacticsMod/backup/input_state.json','0002/ScreenLayout.rdb',*NAMES.values()})
    keep=[rel for rel in keep if (G/rel).is_file()]
    current=read(P/'backup/input_state.json').get('release_version') if (P/'backup/input_state.json').is_file() else None
    plan=dict(status='switch_dry_run' if a.dry_run else 'switching',from_version=current,to_version=version,package=str(src),
        files=len(files),skipped_documents=sum(1 for q in src.rglob('*') if q.is_file())-len(files),backup=str(backup),backed_up=len(keep))
    if a.dry_run:
        print(json.dumps(plan,ensure_ascii=False)); return
    no_game(); assert sha(G/'SAN14CE_KO.exe')==ORIGINAL_SHA,'Original EXE changed'
    for rel in keep:
        dest=backup/'files'/rel; dest.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(G/rel,dest)
    save(backup/'switch.json',plan)
    settings_before=settings()
    if (P/'관리.ps1').is_file():
        rc,log=run_manager('Restore',tag)
        assert rc==0,'Current 관리.ps1 Restore failed (nothing copied yet); see '+str(log)
    for rel in files: atomic_write(G/rel,(src/rel).read_bytes())
    rc,log=run_manager('Apply',tag)
    assert rc==0,version+' 관리.ps1 Apply failed; files before the switch are in '+str(backup/'files')+'; see '+str(log)
    m=read(P/'build/input_package/manifest.json'); assert m['release_version']==version
    bad=[n for n,h in m['split_modules']['files'].items() if not (G/n).is_file() or sha(G/n)!=h]
    gui=m.get('gui_installer',{})
    if gui.get('file') and (G/gui['file']).is_file() and sha(G/gui['file'])!=gui.get('sha256'): bad.append(gui['file'])
    state=read(P/'backup/input_state.json')
    assert not bad,'Installed files differ from the package manifest: '+', '.join(bad)
    assert state['release_version']==version and state['installed'],'State does not record '+version
    assert sha(G/'SAN14CE_KO.exe')==ORIGINAL_SHA
    after=settings(); changed=sorted(k for k,v in settings_before.items() if after.get(k)!=v)
    plan.update(status='switched_and_verified',settings_changed=changed,installed_at=datetime.now(timezone.utc).isoformat())
    save(backup/'switch.json',plan); print(json.dumps(plan,ensure_ascii=False))

if __name__=='__main__':
    ap=argparse.ArgumentParser(); ap.add_argument('action',choices=('prepare','install','rollback','switch-package')); ap.add_argument('version',help='version, or the package folder for switch-package')
    ap.add_argument('--modules',default=''); ap.add_argument('--gui',action='store_true'); ap.add_argument('--note',default='')
    ap.add_argument('--release',action='store_true')
    ap.add_argument('--diagnostics-on',action='store_true',help='install sets [groups] diagnostics=1 (nothing else in 기능설정.ini)')
    ap.add_argument('--layout',default='',help='folder with a rebuilt traits9_layout.bin/.json (keeps every staged record)')
    ap.add_argument('--dry-run',action='store_true',help='switch-package: print what would be copied and backed up, change nothing')
    a=ap.parse_args(); {'prepare':prepare,'install':install,'rollback':rollback,'switch-package':switch_package}[a.action](a)
