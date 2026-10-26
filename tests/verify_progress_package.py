"""Exercise packaged read-only measurement without touching the real game."""
from pathlib import Path
import json, os, subprocess, tempfile, zipfile, shutil

p=Path(__file__).resolve().parents[1]
share=json.loads((p/'build/share_package.json').read_text(encoding='utf-8'))
folder=Path(tempfile.mkdtemp(prefix='progress_package_',dir=p/'build')).resolve()
checks=0
def check(ok,label):
    global checks
    assert ok,label;checks+=1

try:
    with zipfile.ZipFile(share['zip']) as z:
        wanted=['AI_TacticsMod/optional_tools/AI모드_진행성능측정.cmd','AI_TacticsMod/optional_tools/AI모드_CPU호출스택측정_관리자.cmd',
            'AI_TacticsMod/distribution/ProgressMeasure.ps1','AI_TacticsMod/distribution/ProgressMetrics.psm1',
            'AI_TacticsMod/tools/capture_progress_cpu.ps1','AI_TacticsMod/distribution/SAN14CpuSample.wprp',
            'AI_TacticsMod/docs/진행_처리와개선제안.md']
        for name in wanted:
            check(name in z.namelist(),'Missing packaged study file: '+name)
            check((folder/name).resolve().is_relative_to(folder),'Unexpected extraction path')
            z.extract(name,folder)
        check(not any('data/performance/' in n or n.endswith('.etl') for n in z.namelist()),'Private traces must not be shared')
        for name in wanted:
            if name.endswith('.ps1') or name.endswith('.psm1'):
                check(z.read(name).startswith(b'\xef\xbb\xbf'),'Windows PowerShell Unicode BOM')
    out=folder/'한글 출력 space'
    command=['cmd.exe','/d','/c','call',str(folder/'AI_TacticsMod/optional_tools/AI모드_진행성능측정.cmd'),
        '-ProcessId',str(os.getpid()),'-Seconds','2','-Countdown','0','-IntervalMs','250',
        '-OutputDirectory',str(out)]
    run=subprocess.run(command,input=b'\r\n',capture_output=True,timeout=20)
    check(run.returncode==0,run.stdout.decode(errors='replace')+run.stderr.decode(errors='replace'))
    files=list(out.glob('*.json'));check(len(files)==1,'Packaged CMD writes unique JSON')
    data=json.loads(files[0].read_text(encoding='utf-8'))
    check(data['status']=='completed' and data['samples']>=2,'Packaged recorder reads actual Windows counters')
    check(not data['function_attribution_available'] and not data['exact_progress_button_interval'],'Packaged recorder scope flags')
    check(data['package_version'] is None,'Missing package metadata stays unknown')
    # Validate, but do not execute the UAC launcher or start another trace.
    run=subprocess.run(['wpr.exe','-profiles',str(folder/'AI_TacticsMod/distribution/SAN14CpuSample.wprp')],capture_output=True,timeout=20)
    check(run.returncode==0 and b'SAN14CPU' in run.stdout,'Packaged sampled-CPU XML accepted by WPR')
    result={'status':'progress_measure_package_verified','checks':checks,'zip_sha256':share['zip_sha256'],
        'packaged_cmd_windows_accounting_verified':True,'admin_launcher_not_exercised':True,
        'actual_game_modified':False}
    (p/'build/progress_package_verification.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))
finally:
    assert folder.is_relative_to((p/'build').resolve()) and folder.name.startswith('progress_package_')
    shutil.rmtree(folder)
