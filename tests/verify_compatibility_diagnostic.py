"""Exercise the packaged read-only diagnostic in relocated, isolated folders."""
from pathlib import Path
import hashlib, json, shutil, struct, subprocess, tempfile, zipfile

project = Path(__file__).resolve().parents[1]
game = project.parent
package = Path(json.loads((project/'build/share_package.json').read_text(encoding='utf-8'))['zip'])
fixture = Path(tempfile.mkdtemp(prefix='compatibility_verify_', dir=project/'build')).resolve()
checks = 0
def check(value, label):
    global checks
    assert value, label
    checks += 1
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None
def invoke(folder, script):
    return subprocess.run(['cmd.exe', '/d', '/c', 'call', '.\\'+script, '--no-pause'], cwd=folder, capture_output=True, timeout=45)
baseline = {name: digest(game/name) for name in ['SAN14CE_KO.exe', 'dinput8.dll', 'D3D11.dll']}
try:
    relocated = fixture/'Steam library 한글 공백'/'steamapps'/'common'/'Romance_of_the_Three_Kingdoms_14'
    relocated.mkdir(parents=True)
    with zipfile.ZipFile(package) as archive:
        for name in archive.namelist():
            dest = (relocated/name).resolve()
            assert dest.is_relative_to(relocated.resolve())
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(archive.read(name))
    steamapps = relocated.parent.parent
    (steamapps/'appmanifest_872410.acf').write_text('"AppState" { "appid" "872410" "installdir" "Romance_of_the_Three_Kingdoms_14" "buildid" "25094285" "LastOwner" "never-export-this" }', encoding='utf-8')
    exe = relocated/'SAN14CE_KO.exe'
    target = relocated/'dinput8.dll'
    target.write_bytes(b'preserved other input provider')
    original_provider = digest(target)
    shutil.copyfile(game/'SAN14CE_KO.exe', exe)
    before = digest(exe)
    log = relocated/'SAN14_AITactics.log'
    log.write_text('installation_rejected\nnative_code_timeout\n', encoding='utf-8')
    result = invoke(relocated, 'AI_TacticsMod/optional_tools/AI모드_호환성진단.cmd')
    check(result.returncode == 0, result.stdout.decode('utf-8', errors='replace'))
    report_path = relocated/'AI_TacticsMod/data/diagnostics/AI모드_호환성진단.json'
    report = json.loads(report_path.read_text(encoding='utf-8'))
    check(report['diagnostic_only'] and not report['runtime_fingerprint_checked'], 'never claim live code checked')
    check(report['exact_reference_exe'] and report['supported_pe_build'] and report['files']['SAN14CE_KO.exe']['sha256'] == before, 'reference file and build identified')
    check(report['exe_pe']['timestamp'] == '0x6A983C25' and report['exe_pe']['machine'] == '0x8664', 'actual PE headers read')
    check(report['steam']['buildid'] == '25094285' and report['steam']['appid'] == '872410', 'matching library game only')
    check('never-export-this' not in report_path.read_text(encoding='utf-8'), 'Steam owner excluded')
    check(report['mod_log_tail'] == ['installation_rejected', 'native_code_timeout'], 'mod log included')
    check(digest(target) == original_provider and digest(exe) == before, 'diagnostic preserves EXE and input')
    # A file overlay changes whole-file SHA while leaving this build's game code intact.
    original_exe = exe.read_bytes()
    with exe.open('ab') as handle:
        handle.write(b'fixture changed file contents')
    altered = digest(exe)
    result = invoke(relocated, 'AI모드_적용.cmd')
    output = result.stdout.decode('utf-8', errors='replace')
    check(result.returncode == 0 and digest(target) != original_provider, 'same-build different file SHA installs')
    check(altered in output, 'different file SHA displayed')
    decoded_output = [result.stdout.decode(encoding, errors='replace') for encoding in ['utf-8', 'cp949']]
    check(any('전체 해시 차이는 설치 차단 사유가 아닙니다' in text for text in decoded_output), 'same-build policy explained')
    check(digest(exe) == altered and digest(relocated/'SAN14_AI_PreviousInput.dll') == original_provider, 'EXE unchanged and previous input preserved')
    result = invoke(relocated, 'AI모드_해제_원본복구.cmd')
    check(result.returncode == 0 and digest(target) == original_provider and digest(exe) == altered, 'same-build variant restores')
    result = invoke(relocated, 'AI_TacticsMod/optional_tools/AI모드_호환성진단.cmd')
    report = json.loads(report_path.read_text(encoding='utf-8'))
    check(not report['exact_reference_exe'] and report['supported_pe_build'], 'file identity and build compatibility are distinct')
    pe = struct.unpack_from('<I', original_exe, 0x3C)[0]
    cases = [('rollback', pe+8, '<I', 0x6A6D9F72), ('other build', pe+8, '<I', 0x12345678),
             ('other image', pe+80, '<I', 0x029FF000+0x1000), ('32-bit machine', pe+4, '<H', 0x014C),
             ('PE32 header', pe+24, '<H', 0x010B), ('invalid PE location', 0x3C, '<I', 0xFFFFFFFF)]
    for label, offset, fmt, value in cases:
        modified = bytearray(original_exe)
        struct.pack_into(fmt, modified, offset, value)
        exe.write_bytes(modified)
        changed = digest(exe)
        result = invoke(relocated, 'AI모드_적용.cmd')
        check(result.returncode != 0, label+' blocked')
        report = json.loads(report_path.read_text(encoding='utf-8'))
        check(not report['supported_pe_build'], label+' diagnostic distinguishes unsupported build')
        check(digest(target) == original_provider and digest(exe) == changed and not (relocated/'SAN14_AI_PreviousInput.dll').exists(), label+' leaves game and provider intact')
        if label == 'rollback':
            check(any('롤백 게임 빌드' in result.stdout.decode(encoding, errors='replace') for encoding in ['utf-8','cp949']), 'rollback needs a separate native profile')
    # The source ZIP may live outside the game directory, including another Steam library.
    selected = fixture/'Selected game 한글'
    selected.mkdir()
    (selected/'SAN14CE_KO.exe').write_bytes(original_exe)
    manager = relocated/'AI_TacticsMod/관리.ps1'
    def selected_mode(mode):
        return subprocess.run(['powershell.exe','-NoProfile','-STA','-ExecutionPolicy','Bypass','-File',str(manager),'-Mode',mode,'-GameDirectory',str(selected),'-NonInteractive'], capture_output=True, timeout=30)
    result = selected_mode('Apply')
    check(result.returncode == 0 and digest(selected/'dinput8.dll') == digest(relocated/'AI_TacticsMod/build/input_package/dinput8.dll'), 'external ZIP installs into selected game')
    check(not (selected/'AI_TacticsMod/build/input_package').exists(), 'source payload need not already exist inside selected game')
    result = selected_mode('Restore')
    check(result.returncode == 0 and not (selected/'dinput8.dll').exists(), 'external ZIP restores selected game')
    exe.write_bytes(b'invalid executable')
    result = invoke(relocated, 'AI_TacticsMod/optional_tools/AI모드_호환성진단.cmd')
    report = json.loads(report_path.read_text(encoding='utf-8'))
    check(result.returncode == 0 and not report['supported_pe_build'], 'invalid PE produces diagnostic')
    check('exe_pe' not in report and report['notes'], 'invalid PE is reported without guessing')
    exe.unlink()
    result = invoke(relocated, 'AI모드_적용.cmd')
    check(result.returncode != 0 and any('같은 폴더' in result.stdout.decode(encoding, errors='replace') for encoding in ['utf-8', 'cp949']), 'missing EXE distinguished from different version')
    result = invoke(relocated, 'AI_TacticsMod/optional_tools/AI모드_호환성진단.cmd')
    report = json.loads(report_path.read_text(encoding='utf-8'))
    check(result.returncode == 0 and not report['files']['SAN14CE_KO.exe']['present'], 'missing file diagnostic')
    check(all(digest(game/name) == expected for name, expected in baseline.items()), 'actual installation unchanged')
    record = {'status': 'compatibility_diagnostic_verified', 'checks': checks, 'zip_sha256': digest(package), 'diagnostic_does_not_modify_game_files': True, 'relocated_unicode_path_tested': True, 'same_build_different_hash_installs_and_restores': True, 'unsupported_pe_still_blocked': True, 'external_source_selected_game_tested': True, 'real_recipient_exe_tested': False, 'game_function_or_process_invoked': False}
    (project/'build/compatibility_diagnostic_verification.json').write_text(json.dumps(record, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(record, indent=2))
finally:
    assert fixture.is_relative_to((project/'build').resolve()) and fixture.name.startswith('compatibility_verify_')
    shutil.rmtree(fixture)
