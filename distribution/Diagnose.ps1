param([string]$GameDirectory, [switch]$Quiet, [switch]$Trace, [switch]$NonInteractive)
$ErrorActionPreference = 'Stop'
if (-not $GameDirectory) {
    $GameDirectory = Split-Path -Parent $PSScriptRoot
    if (-not [IO.File]::Exists((Join-Path $GameDirectory 'SAN14CE_KO.exe')) -and -not $NonInteractive) {
        Add-Type -AssemblyName System.Windows.Forms
        $taskPicker = New-Object Windows.Forms.OpenFileDialog
        $taskPicker.Title = '진단할 실제 게임 폴더의 SAN14CE_KO.exe를 선택해 주세요'
        $taskPicker.Filter = '삼국지14 CE 한국어판|SAN14CE_KO.exe'
        try {
            if ($taskPicker.ShowDialog() -ne [Windows.Forms.DialogResult]::OK) { exit 0 }
            $GameDirectory = [IO.Path]::GetDirectoryName($taskPicker.FileName)
        } finally { $taskPicker.Dispose() }
    }
}
$taskGame = [IO.Path]::GetFullPath($GameDirectory)
$taskExpected = '87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a'
function Get-FileRecord([string]$Path) {
    if (-not [IO.File]::Exists($Path)) { return @{present=$false} }
    $taskAlgorithm = [Security.Cryptography.SHA256]::Create()
    $taskStream = [IO.File]::OpenRead($Path)
    try {
        return @{present=$true; bytes=$taskStream.Length; sha256=[BitConverter]::ToString($taskAlgorithm.ComputeHash($taskStream)).Replace('-','').ToLowerInvariant()}
    } finally { $taskStream.Dispose(); $taskAlgorithm.Dispose() }
}
function Get-PERecord([string]$Path) {
    $taskStream = [IO.File]::OpenRead($Path)
    $taskReader = New-Object IO.BinaryReader($taskStream)
    try {
        if ($taskReader.ReadUInt16() -ne 0x5A4D) { throw 'DOS header missing' }
        $taskStream.Position = 0x3C
        $taskPE = $taskReader.ReadUInt32()
        if ($taskPE -gt $taskStream.Length - 88) { throw 'PE header outside file' }
        $taskStream.Position = $taskPE
        if ($taskReader.ReadUInt32() -ne 0x4550) { throw 'PE signature missing' }
        $taskMachine = $taskReader.ReadUInt16()
        $taskSections = $taskReader.ReadUInt16()
        $taskTimestamp = $taskReader.ReadUInt32()
        $taskStream.Position = $taskPE + 20
        $taskOptionalSize = $taskReader.ReadUInt16()
        $taskStream.Position = $taskPE + 24
        $taskMagic = $taskReader.ReadUInt16()
        if ($taskMagic -ne 0x20B -and $taskMagic -ne 0x10B) { throw 'Unsupported PE optional header' }
        $taskStream.Position = $taskPE + 40
        $taskEntry = $taskReader.ReadUInt32()
        $taskStream.Position = $taskPE + 80
        $taskImageSize = $taskReader.ReadUInt32()
        $taskResult = @{machine=('0x{0:X4}' -f $taskMachine); optional_magic=('0x{0:X4}' -f $taskMagic); timestamp=('0x{0:X8}' -f $taskTimestamp); image_size=('0x{0:X8}' -f $taskImageSize); entry_rva=('0x{0:X8}' -f $taskEntry); section_count=$taskSections}
        if ($taskSections -gt 96 -or $taskOptionalSize -lt 64) { throw 'Invalid PE section table' }
        $taskSectionTable = [long]$taskPE + 24 + $taskOptionalSize
        if ($taskSectionTable + 40 * $taskSections -gt $taskStream.Length) { throw 'Section table outside file' }
        for ($taskIndex=0; $taskIndex -lt $taskSections; $taskIndex++) {
            $taskStream.Position = $taskSectionTable + 40 * $taskIndex
            $taskName = [Text.Encoding]::ASCII.GetString($taskReader.ReadBytes(8)).TrimEnd([char]0)
            $taskVirtual = $taskReader.ReadUInt32()
            $taskRva = $taskReader.ReadUInt32()
            $taskRawSize = $taskReader.ReadUInt32()
            $taskRawOffset = $taskReader.ReadUInt32()
            if ($taskName -eq '.text') {
                $taskResult.text = @{rva=('0x{0:X8}' -f $taskRva); virtual_size=$taskVirtual; raw_size=$taskRawSize; raw_offset=$taskRawOffset}
            }
        }
        return $taskResult
    } finally { $taskReader.Dispose(); $taskStream.Dispose() }
}
$taskReport = [ordered]@{
    schema=1; collected_at=[DateTimeOffset]::Now.ToString('o'); diagnostic_only=$true
    expected_exe_sha256=$taskExpected
    expected_runtime=@{machine='0x8664'; timestamp='0x6A983C25'; decoded_text_crc32='0x6C7F386E'; decoded_text_bytes=20179374}
    runtime_fingerprint_checked=$false
    files=[ordered]@{}; steam=$null; mod_log_tail=@(); application_errors=@(); notes=@()
}
foreach ($taskName in @('SAN14CE_KO.exe','dinput8.dll','SAN14_CombatAI.dll','SAN14_Convenience.dll','SAN14_AI_PreviousInput.dll','D3D11.dll','dxgi.dll')) {
    if ($Trace) { Write-Host ('Reading: ' + $taskName) }
    try { $taskReport.files[$taskName] = Get-FileRecord (Join-Path $taskGame $taskName) }
    catch { $taskReport.files[$taskName] = @{present=[IO.File]::Exists((Join-Path $taskGame $taskName)); read_error=$_.Exception.Message} }
}
$taskExe = Join-Path $taskGame 'SAN14CE_KO.exe'
if ($taskReport.files['SAN14CE_KO.exe'].present) {
    if ($Trace) { Write-Host 'Reading PE/version' }
    try {
        $taskReport.exe_pe = Get-PERecord $taskExe
        $taskVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($taskExe)
        $taskReport.exe_version = @{file=$taskVersion.FileVersion; product=$taskVersion.ProductVersion}
    } catch { $taskReport.notes += 'EXE PE/version: ' + $_.Exception.Message }
}
$taskReport.exact_reference_exe = $taskReport.files['SAN14CE_KO.exe'].sha256 -eq $taskExpected
$taskReport.supported_pe_build = $false
if ($taskReport.exe_pe) {
    $taskReport.supported_pe_build = $taskReport.exe_pe.machine -eq '0x8664' -and $taskReport.exe_pe.optional_magic -eq '0x020B' -and $taskReport.exe_pe.timestamp -eq '0x6A983C25' -and $taskReport.exe_pe.image_size -eq '0x029FF000'
}
# Read only the matching game's library manifest; never include Steam account data.
if ($Trace) { Write-Host 'Reading Steam build' }
try {
    $taskSteamApps = Split-Path -Parent (Split-Path -Parent $taskGame)
    $taskInstallDir = Split-Path -Leaf $taskGame
    foreach ($taskManifest in Get-ChildItem -LiteralPath $taskSteamApps -Filter 'appmanifest_*.acf' -File -ErrorAction SilentlyContinue) {
        $taskText = [IO.File]::ReadAllText($taskManifest.FullName)
        $taskDirMatch = [regex]::Match($taskText, '"installdir"\s+"([^"\r\n]+)"')
        if ($taskDirMatch.Success -and $taskDirMatch.Groups[1].Value -eq $taskInstallDir) {
            $taskAppMatch = [regex]::Match($taskText, '"appid"\s+"(\d+)"')
            $taskBuildMatch = [regex]::Match($taskText, '"buildid"\s+"(\d+)"')
            $taskReport.steam = @{appid=$taskAppMatch.Groups[1].Value; buildid=$taskBuildMatch.Groups[1].Value}
            break
        }
    }
} catch { $taskReport.notes += 'Steam build: ' + $_.Exception.Message }
try {
    $taskLog = Join-Path $taskGame 'SAN14_AITactics.log'
    if ([IO.File]::Exists($taskLog)) { $taskReport.mod_log_tail = @(Get-Content -LiteralPath $taskLog -Tail 80 -Encoding UTF8 | ForEach-Object { $_.ToString() }) }
    $taskCombatLog=Join-Path $taskGame 'SAN14_CombatAI.log'
    if([IO.File]::Exists($taskCombatLog)){$taskReport.mod_log_tail+=@('--- SAN14_CombatAI.log ---')+@(Get-Content -LiteralPath $taskCombatLog -Tail 80 -Encoding UTF8 | ForEach-Object {$_.ToString()})}
} catch { $taskReport.notes += 'Mod log: ' + $_.Exception.Message }
try {
    if ($Trace) { Write-Host 'Reading recent Windows events' }
    # Some hosts stall even on a bounded event query. Isolate this read and enforce a deadline.
    $taskEventScript = @'
$ErrorActionPreference = 'Stop'
$results = @()
foreach ($entry in (Get-WinEvent -LogName Application -MaxEvents 200)) {
    if ($entry.Id -ne 1000 -or $entry.TimeCreated -lt (Get-Date).AddDays(-7)) { continue }
    [xml]$xml = $entry.ToXml()
    $fields = @{}
    foreach ($data in $xml.Event.EventData.Data) { if ($data.Name) { $fields[[string]$data.Name] = [string]$data.InnerText } }
    if ($fields.AppName -eq 'SAN14CE_KO.exe') {
        $results += @{time=$entry.TimeCreated.ToString('o'); app_version=$fields.AppVersion; module=$fields.ModuleName; module_version=$fields.ModuleVersion; exception_code=$fields.ExceptionCode; fault_offset=$fields.FaultingOffset}
        if ($results.Count -ge 3) { break }
    }
}
ConvertTo-Json -InputObject $results -Compress
'@
    $taskEventStart = New-Object Diagnostics.ProcessStartInfo
    $taskEventStart.FileName = Join-Path $PSHOME 'powershell.exe'
    $taskEventStart.Arguments = '-NoProfile -NonInteractive -EncodedCommand ' + [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($taskEventScript))
    $taskEventStart.UseShellExecute = $false
    $taskEventStart.CreateNoWindow = $true
    $taskEventStart.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $taskEventStart.RedirectStandardOutput = $true
    $taskEventStart.RedirectStandardError = $true
    $taskEventProcess = New-Object Diagnostics.Process
    $taskEventProcess.StartInfo = $taskEventStart
    try {
        [void]$taskEventProcess.Start()
        $taskEventOutput = $taskEventProcess.StandardOutput.ReadToEndAsync()
        $taskEventErrors = $taskEventProcess.StandardError.ReadToEndAsync()
        if (-not $taskEventProcess.WaitForExit(4000)) {
            $taskEventProcess.Kill()
            [void]$taskEventProcess.WaitForExit(1000)
            $taskReport.notes += 'Windows crash event query timed out after 4 seconds'
        } elseif ($taskEventProcess.ExitCode -eq 0 -and $taskEventOutput.Wait(1000)) {
            $taskReport.application_errors = @($taskEventOutput.Result | ConvertFrom-Json)
        } else { $taskReport.notes += 'Windows crash events unavailable or absent' }
    } finally {
        if ($taskEventProcess.Id -and -not $taskEventProcess.HasExited) { $taskEventProcess.Kill() }
        $taskEventProcess.Dispose()
    }
} catch { $taskReport.notes += 'Windows crash events unavailable or absent' }
if ($Trace) { Write-Host 'Serializing diagnostic JSON' }
$taskJson = $taskReport | ConvertTo-Json -Depth 8
if ($Trace) { Write-Host 'Writing diagnostic JSON' }
$taskOutput = Join-Path $taskGame 'AI_TacticsMod\data\diagnostics\AI모드_호환성진단.json'
try {
    [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($taskOutput))
    [IO.File]::WriteAllText($taskOutput, $taskJson, (New-Object Text.UTF8Encoding($false)))
}
catch {
    $taskOutput = Join-Path ([IO.Path]::GetTempPath()) 'SAN14_AI_compatibility.json'
    try { [IO.File]::WriteAllText($taskOutput, $taskJson, (New-Object Text.UTF8Encoding($false))) }
    catch { $taskOutput = $null; Write-Host $taskJson }
}
Write-Host ('실행 파일 SHA256: ' + $taskReport.files['SAN14CE_KO.exe'].sha256)
if ($taskReport.exe_pe) { Write-Host ('PE 빌드: ' + $taskReport.exe_pe.timestamp + ' / ImageSize ' + $taskReport.exe_pe.image_size) }
if ($taskReport.steam) { Write-Host ('Steam buildid: ' + $taskReport.steam.buildid) }
if ($taskOutput) { Write-Host ('진단 기록: ' + $taskOutput) }
if (-not $Quiet) {
    Write-Host 'EXE·DLL·세이브를 변경하지 않았습니다. 게임 실행이나 해시 검사 우회도 하지 않습니다.'
    Write-Host '진단 JSON을 제작자에게 전달해 주세요. 지원 여부와 실행 중 안정성은 별도 확인이 필요합니다.'
}
