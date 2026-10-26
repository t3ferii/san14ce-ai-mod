from pathlib import Path
import subprocess, shutil, json, hashlib
PROJECT=Path(__file__).resolve().parents[1]
directory=PROJECT/'build/input_test';directory.mkdir(exist_ok=True)
shutil.copyfile(PROJECT/'build/input_fixture.exe',directory/'input_fixture.exe')
shutil.copyfile(PROJECT/'build/SAN14_AIInput.dll',directory/'dinput8.dll')
log=directory/'SAN14_AITactics.log'
if log.exists():log.unlink()
run=subprocess.run([str(directory/'input_fixture.exe')],cwd=directory,capture_output=True,text=True,timeout=15)
assert run.returncode==0,(run.returncode,run.stdout,run.stderr)
assert 'unsupported_executable' in log.read_text()
report={'status':'input_loader_fixture_passed','original_input_factory_succeeded':True,
        'test_exe_modified':False,'game_startup_verified':False,
        'dll_sha256':hashlib.sha256((directory/'dinput8.dll').read_bytes()).hexdigest()}
(PROJECT/'build/input_loader_verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(run.stdout.strip());print(json.dumps(report,indent=2))
