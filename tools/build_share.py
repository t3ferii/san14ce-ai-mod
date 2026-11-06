"""Package only mod-owned files, using a Python-free PowerShell manager."""
from pathlib import Path
import argparse,hashlib,json,zipfile,re,posixpath

PROJECT=Path(__file__).resolve().parents[1]
GAME=PROJECT.parent
parser=argparse.ArgumentParser()
parser.add_argument('--payload',type=Path,default=PROJECT/'build/input_package')
parser.add_argument('--local-diagnostic-candidate',action='store_true',
    help='Package runtime-diagnostic DLLs only under build/local_candidates, never dist.')
# User 2026-11-01: no more _noexe packages; every release carries the installer EXE.
parser.add_argument('--report',type=Path,help='Keep a version-specific packaging receipt under build.')
ARGS=parser.parse_args()
PAYLOAD=ARGS.payload.resolve();WITHOUT_GUI=False
assert PAYLOAD.is_relative_to((PROJECT/'build').resolve())
manifest=json.loads((PAYLOAD/'manifest.json').read_text(encoding='utf-8'))
if manifest.get('local_diagnostic_only') and not ARGS.local_diagnostic_candidate:
    raise RuntimeError('Local runtime diagnostics must never be distributed.')
if ARGS.local_diagnostic_candidate:
    assert manifest.get('local_diagnostic_only'),'Local diagnostic candidate requires local_diagnostic_only manifest.'
if manifest.get('release_version')=='v12.22-rc3' and (PROJECT/'data/diagnostics/v12_22_rc3_crash_rollback.json').exists():
    raise RuntimeError('v12.22-rc3 expert-entry crash reported; this release is quarantined.')
if manifest.get('release_version')=='v11.11' and (PROJECT/'data/diagnostics/v11_11_crash_rollback.json').exists():
    raise RuntimeError('v11.11 demo-start crash confirmed; release packaging is blocked.')
dll=(PAYLOAD/'dinput8.dll').read_bytes()
assert hashlib.sha256(dll).hexdigest()==manifest['dll_sha256']
combat_dll=None
if manifest.get('split_modules'):
    combat_dll=(PAYLOAD/'SAN14_CombatAI.dll').read_bytes()
    assert hashlib.sha256(combat_dll).hexdigest()==manifest['split_modules']['files']['SAN14_CombatAI.dll']
def windows_text(path,bom=False):
    text=path.read_text(encoding='utf-8-sig').replace('\r\n','\n').replace('\n','\r\n')
    return (b'\xef\xbb\xbf' if bom else b'')+text.encode('utf-8')
assert not manifest.get('assets'), 'Current releases must not carry reference text assets.'
files={}
for name in ('AI모드_적용.cmd','AI모드_해제_원본복구.cmd'):
    files[name]=windows_text(GAME/name)
for name,source in (('관리.ps1','Manage.ps1'),('진단.ps1','Diagnose.ps1'),
                    ('SplitManage.ps1','SplitManage.ps1'),('FeatureSelect.ps1','FeatureSelect.ps1')):
    files['AI_TacticsMod/'+name]=windows_text(PROJECT/'distribution'/source,True)
for name in ('AI모드_호환성진단.cmd','AI모드_기능선택.cmd'):
    files['AI_TacticsMod/optional_tools/'+name]=windows_text(PROJECT/'optional_tools'/name)
for name,digest in manifest['split_modules']['files'].items():
    assert name in ('dinput8.dll','SAN14_CombatAI.dll','SAN14_Convenience.dll','SAN14_CCZ.dll','SAN14_Layout.dll')
    data=(PAYLOAD/name).read_bytes();assert hashlib.sha256(data).hexdigest()==digest
    files['AI_TacticsMod/build/input_package/'+name]=data
files['AI_TacticsMod/build/input_package/manifest.json']=(PAYLOAD/'manifest.json').read_bytes()
# 2026-11-01: SAN14_CCZ.dll reads its sprite atlas from AI_TacticsMod/assets (credits: 조조전 6.6, 신조조전 7.0).
for name,digest in manifest.get('ccz_assets',{}).items():
    data=(PROJECT/name).read_bytes();assert hashlib.sha256(data).hexdigest()==digest,name
    files['AI_TacticsMod/'+name]=data
assert 'SAN14_CCZ.dll' not in manifest['split_modules']['files'] or manifest.get('ccz_assets'),'SAN14_CCZ.dll needs its asset'
# Installer update/rollback (v15): per-release feature catalogue for the changed-feature colours.
if (PAYLOAD/'release_features.json').exists():
    files['AI_TacticsMod/release_features.json']=(PAYLOAD/'release_features.json').read_bytes()
for name,key in (('traits9_layout.bin','bank_sha256'),('traits9_layout.json','json_sha256')):
    data=(PAYLOAD/name).read_bytes();assert hashlib.sha256(data).hexdigest()==manifest['layout_patch'][key]
    files['AI_TacticsMod/build/input_package/'+name]=data
# Current releases use 기능설정.ini; old installations retain legacy fallback files.
for document in ('FEATURE_SWITCHES.md','COMBAT_AI.md','DOMESTIC.md','REGION_OFFICERS.md',
                 'SPEED_AND_SAVE.md','EXPERT_SETTINGS.md','EXTENSIONS.md','UNIFIED_PERSONNEL.md',
                 'CHANGES_ALL.md','GITHUB_기능상세.md','DAMAGE_PREVIEW.md','BUILDING_DESTRUCTION.md','CREDITS.md'):
    files['AI_TacticsMod/docs/'+document]=windows_text(PROJECT/'docs'/document,True)
files['AI_TacticsMod/README.md']=windows_text(PROJECT/'README.md',True)
files['AI_TacticsMod/사용안내.md']=windows_text(PROJECT/'사용안내.md',True)
files['AI_TacticsMod/docs/읽어주세요.txt']=windows_text(PROJECT/'distribution/읽어주세요.txt',True)
files['AI_TacticsMod/docs/README.md']=windows_text(PROJECT/'docs/PACKAGE_INDEX.md',True)
# User 2026-10-29: every release names the other mods it borrowed from (docs/CREDITS.md).
credits=[l for l in (PROJECT/'docs/CREDITS.md').read_text(encoding='utf-8-sig').splitlines() if l.startswith('- ')]
assert credits,'docs/CREDITS.md lists no credited features.'
for name in ('AI_TacticsMod/README.md','AI_TacticsMod/docs/읽어주세요.txt'):
    missing=[l for l in credits if l not in files[name].decode('utf-8-sig').splitlines()]
    if missing:raise RuntimeError(name+' lacks credits from docs/CREDITS.md: '+'; '.join(missing))
installer_credits=re.findall(r'^\* `(\w+)` = (.+)$',(PROJECT/'docs/CREDITS.md').read_text(encoding='utf-8-sig'),re.M)
assert installer_credits,'docs/CREDITS.md lists no installer credits.'
for path,pattern in (('installer/InstallerGui.cs','"{}",'),('distribution/FeatureSelect.ps1',"Key='{}';")):
    rows=(PROJECT/path).read_text(encoding='utf-8-sig').splitlines()
    for key,source in installer_credits:
        if not any(pattern.format(key) in row and '출처: '+source in row for row in rows):
            raise RuntimeError(path+' feature '+key+' lacks "출처: '+source+'".')
if manifest.get('gui_installer') and not WITHOUT_GUI:
    gui_bytes=(PAYLOAD/manifest['gui_installer']['file']).read_bytes()
    # User 2026-10-30: feature notes alone do not establish a complete credits tab.
    for line in credits:
        if line.encode('utf-16-le') not in gui_bytes:
            raise RuntimeError('Installer EXE lacks credits tab line: '+line+'; rebuild it.')
    for source in sorted({source for _,source in installer_credits}):
        if ('출처: '+source).encode('utf-16-le') not in gui_bytes:
            raise RuntimeError('Installer EXE predates docs/CREDITS.md ("출처: '+source+'"); rebuild it.')
if manifest.get('gui_installer') and not WITHOUT_GUI:
    gui=(PAYLOAD/manifest['gui_installer']['file']).read_bytes()
    assert hashlib.sha256(gui).hexdigest()==manifest['gui_installer']['sha256']
    files[manifest['gui_installer']['file']]=gui
ROOT={'AI모드_적용.cmd','AI모드_해제_원본복구.cmd'}|({manifest['gui_installer']['file']} if manifest.get('gui_installer') and not WITHOUT_GUI else set())
# Extracting the package also keeps the root limited to the current CMDs and installer EXE.
for launcher in ('AI모드_적용.cmd','AI모드_해제_원본복구.cmd'):
    files[launcher]=re.sub(rb'Current release: [^\r\n]+',
        b'Current release: '+manifest['release_version'].encode('ascii'),files[launcher])
files={('AI_TacticsMod/docs/'+name if '/' not in name and name not in
        ROOT else name):data for name,data in files.items()}
if manifest.get('feature_switches'):
    # Workspace-only evidence links remain plain labels in the recipient's copy.
    for name in list(files):
        if name.endswith('.md'):
            def packaged_link(match):
                target=match[2].strip('<>')
                if re.match(r'^[A-Za-z][\w+.-]*:',target) or target.startswith('#'):
                    return match[0]
                resolved=posixpath.normpath(posixpath.join(posixpath.dirname(name),target.split('#',1)[0]))
                return match[0] if resolved in files else match[1]
            content=files[name].decode('utf-8-sig')
            files[name]=b'\xef\xbb\xbf'+re.sub(r'\[([^\]\n]+)\]\(([^\n]*?)\)',packaged_link,content).encode('utf-8')
if manifest.get('gui_installer') and not WITHOUT_GUI:
    # v12.8: the GUI installer copies exactly these files when the game lives elsewhere.
    files['AI_TacticsMod/package_files.txt']=('\r\n'.join(sorted(files))+'\r\n').encode('utf-8')
target=(PROJECT/'build/local_candidates' if ARGS.local_diagnostic_candidate else PROJECT/'dist')/('SAN14_AI_'+manifest['release_version']+'_integrated.zip')
target.parent.mkdir(parents=True,exist_ok=True)
if target.exists():raise FileExistsError('Existing dist archives are protected: '+str(target))
with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as archive:
    for name,data in files.items():archive.writestr(name,data)
with zipfile.ZipFile(target) as archive:
    assert set(archive.namelist())==set(files)
    for name,data in files.items():assert archive.read(name)==data
report={'release_version':manifest['release_version'],'zip':str(target),'zip_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),
        'files':list(files),'dll_sha256':manifest['dll_sha256'],
        'python_required':False,'game_exe_included':False,'user_backups_included':False,
        'candidate_gameplay_verified':False,'gui_installer_included':bool(manifest.get('gui_installer')) and not WITHOUT_GUI}
report['local_diagnostic_only']=bool(ARGS.local_diagnostic_candidate)
if combat_dll is not None:report['split_modules']=manifest['split_modules']
receipt=ARGS.report.resolve() if ARGS.report else PROJECT/'build/share_package.json'
assert receipt.is_relative_to((PROJECT/'build').resolve())
receipt.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({k:report[k] for k in ['release_version','zip','zip_sha256','game_exe_included','user_backups_included']},ensure_ascii=False))
