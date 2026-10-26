param(
    [ValidateRange(2,300)][int]$Seconds=60,
    [ValidateRange(250,5000)][int]$IntervalMs=500,
    [ValidateRange(0,30)][int]$Countdown=5,
    [int]$ProcessId=0,
    [string]$ProcessName='SAN14CE_KO',
    [string]$Label='manual_progress_capture',
    [ValidateRange(-1,51)][int]$FactionCount=-1,
    [ValidateRange(-1,500)][int]$ArmyCount=-1,
    [string]$OutputDirectory
)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'ProgressMetrics.psm1') -Force
$taskProject=Split-Path -Parent $PSScriptRoot
if (-not $OutputDirectory) { $OutputDirectory=Join-Path $taskProject 'data\performance' }
$taskOutput=[IO.Path]::GetFullPath($OutputDirectory)
$taskRows=New-Object 'System.Collections.Generic.List[object]'
$taskProcess=$null; $taskStatus='completed'; $taskError=$null
try {
    if($ProcessId) { $taskProcess=Get-Process -Id $ProcessId -ErrorAction Stop }
    else {
        $taskCandidates=@(Get-Process -Name $ProcessName -ErrorAction SilentlyContinue)
        if($taskCandidates.Count -eq 0) { throw '게임 프로세스를 찾지 못했습니다. 게임을 실행하고 다시 시도하세요.' }
        if($taskCandidates.Count -ne 1) { throw '프로세스가 여러 개입니다. -ProcessId로 대상 하나를 지정하세요.' }
        $taskProcess=$taskCandidates[0]
    }
    $taskStarted=$taskProcess.StartTime.ToUniversalTime().ToString('o')
    $taskName=$taskProcess.ProcessName
    Write-Host ('측정 대상: {0} PID {1}. {2}초 동안 기록합니다.' -f $taskName,$taskProcess.Id,$Seconds)
    Write-Host '게임으로 돌아가 진행을 누르세요. 측정은 버튼을 감지하지 않으며 대기/화면 처리도 포함합니다.'
    Write-Host '게임을 종료할 필요가 없습니다. 완료 뒤 결과 파일을 확인하세요.'
    for($taskRemaining=$Countdown;$taskRemaining -gt 0;$taskRemaining--) {
        Write-Host ('시작까지 {0}초' -f $taskRemaining); Start-Sleep -Seconds 1
    }
    $taskLogical=[Environment]::ProcessorCount
    $taskClock=[Diagnostics.Stopwatch]::StartNew()
    $taskBefore=Get-ProgressSnapshot $taskProcess $taskClock
    $taskCaptureUtc=[DateTime]::UtcNow.ToString('o')
    $taskObserver=[Diagnostics.Process]::GetCurrentProcess()
    $taskObserverBefore=$taskObserver.TotalProcessorTime.TotalMilliseconds
    try {
        while($taskClock.Elapsed.TotalSeconds -lt $Seconds) {
            $taskWait=[math]::Min($IntervalMs,[math]::Max(1,[int]($Seconds*1000-$taskClock.Elapsed.TotalMilliseconds)))
            Start-Sleep -Milliseconds $taskWait
            try {
                $taskAfter=Get-ProgressSnapshot $taskProcess $taskClock
                $taskRows.Add((Get-ProgressInterval $taskBefore $taskAfter $taskLogical))
                $taskBefore=$taskAfter
            } catch {
                $taskStatus='process_ended_or_accounting_unavailable';$taskError=$_.Exception.Message;break
            }
        }
    } finally {
        $taskClock.Stop();$taskObserver.Refresh()
        $taskObserverCpu=$taskObserver.TotalProcessorTime.TotalMilliseconds-$taskObserverBefore
        [void][IO.Directory]::CreateDirectory($taskOutput)
        $taskStamp=[DateTime]::UtcNow.ToString('yyyyMMdd_HHmmss_fff')+'_'+[Guid]::NewGuid().ToString('N').Substring(0,8)
        $taskBase=Join-Path $taskOutput ('progress_'+$taskStamp)
        if($taskRows.Count) { $taskRows | Export-Csv -LiteralPath ($taskBase+'.csv') -NoTypeInformation -Encoding UTF8 }
        $taskManifest=Join-Path $taskProject 'build\input_package\manifest.json'
        $taskVersion=$null;$taskExpectedHash=$null
        if([IO.File]::Exists($taskManifest)) {
            try { $taskInfo=Get-Content -LiteralPath $taskManifest -Raw -Encoding UTF8 | ConvertFrom-Json
                $taskVersion=$taskInfo.release_version;$taskExpectedHash=$taskInfo.dll_sha256
            } catch { }
        }
        $taskMeta=[ordered]@{
            schema_version=1;status=$taskStatus;error=$taskError;captured_utc=$taskCaptureUtc
            process_name=$taskName;process_id=$taskProcess.Id;process_started_utc=$taskStarted
            requested_seconds=$Seconds;recorded_seconds=$taskClock.Elapsed.TotalSeconds;interval_ms=$IntervalMs
            logical_processors_visible_to_recorder=$taskLogical;label=$Label
            user_reported_faction_count=$FactionCount;user_reported_army_count=$ArmyCount
            package_version=$taskVersion;package_dll_sha256=$taskExpectedHash;loaded_dll_verified=$false
            recorder_cpu_ms=$taskObserverCpu;samples=$taskRows.Count
            exact_progress_button_interval=$false;function_attribution_available=$false
            rows=@($taskRows.ToArray())
        }
        [IO.File]::WriteAllText($taskBase+'.json',($taskMeta | ConvertTo-Json -Depth 8),(New-Object Text.UTF8Encoding($false)))
        Write-Host ('결과: '+$taskBase+'.json')
        if($taskRows.Count) { Write-Host ('CSV: '+$taskBase+'.csv') }
        $taskObserver.Dispose()
    }
    if($taskStatus -ne 'completed') { Write-Warning '기록이 중단되었습니다. JSON의 status/error를 확인하세요.' }
} catch { Write-Error $_; exit 1 }
finally { if($taskProcess) { $taskProcess.Dispose() } }
