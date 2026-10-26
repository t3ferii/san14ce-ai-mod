$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $taskProject 'distribution\ProgressMetrics.psm1') -Force
$script:taskChecks=0
function Assert-Measure($Value,[string]$Label) { if(-not $Value) { throw $Label }; $script:taskChecks++ }
function Snapshot([int]$Id,[long]$Start,[double]$At,[double]$Cpu,$Threads) {
    [PSCustomObject]@{process_id=$Id;started_ticks=$Start;at_ms=$At;cpu_ms=$Cpu;threads=$Threads;failed_thread_reads=0}
}
$taskA=Snapshot 10 1000 0 100 @{'5:10'=@{id=5;cpu_ms=50};'6:20'=@{id=6;cpu_ms=10}}
$taskB=Snapshot 10 1000 500 1100 @{'5:10'=@{id=5;cpu_ms=550};'6:20'=@{id=6;cpu_ms=260}}
$taskResult=Get-ProgressInterval $taskA $taskB 8
Assert-Measure ($taskResult.process_core_equivalents -eq 2) 'Two core equivalents must not be clamped.'
Assert-Measure ($taskResult.process_machine_percent -eq 25) 'Whole-machine CPU percentage.'
Assert-Measure ($taskResult.hottest_thread_id -eq 5 -and $taskResult.hottest_thread_one_core_percent -eq 100) 'Hottest thread accounting.'
Assert-Measure ($taskResult.matched_threads -eq 2) 'Stable thread matches.'
$taskReuse=Snapshot 10 1000 500 600 @{'5:99'=@{id=5;cpu_ms=10000}}
$taskResult=Get-ProgressInterval $taskA $taskReuse 8
Assert-Measure ($taskResult.matched_threads -eq 0 -and $null -eq $taskResult.hottest_thread_one_core_percent) 'Reused or new thread IDs must not create fake CPU deltas.'
$taskIdle=Snapshot 10 1000 500 100 @{'5:10'=@{id=5;cpu_ms=50}}
Assert-Measure ((Get-ProgressInterval $taskA $taskIdle 8).process_core_equivalents -eq 0) 'Zero CPU is valid.'
foreach($taskInvalid in @((Snapshot 11 1000 500 200 @{}),(Snapshot 10 9999 500 200 @{}),(Snapshot 10 1000 0 200 @{}),(Snapshot 10 1000 500 50 @{}))) {
    $taskRejected=$false
    try { Get-ProgressInterval $taskA $taskInvalid 8 | Out-Null } catch { $taskRejected=$true }
    Assert-Measure $taskRejected 'Reject PID reuse, identity changes, zero duration or backwards CPU.'
}
$taskRejected=$false
try { Get-ProgressInterval $taskA $taskB 0 | Out-Null } catch { $taskRejected=$true }
Assert-Measure $taskRejected 'Reject zero logical processors.'
$taskClock=[Diagnostics.Stopwatch]::StartNew();$taskSelf=[Diagnostics.Process]::GetCurrentProcess()
try {
    $taskLiveA=Get-ProgressSnapshot $taskSelf $taskClock
    Start-Sleep -Milliseconds 250
    $taskLiveB=Get-ProgressSnapshot $taskSelf $taskClock
    $taskLiveResult=Get-ProgressInterval $taskLiveA $taskLiveB ([Environment]::ProcessorCount)
    Assert-Measure ($taskLiveResult.interval_ms -ge 200 -and $taskLiveResult.process_cpu_ms -ge 0) 'Actual Windows accounting read.'
} finally { $taskSelf.Dispose() }
$taskFolder=Join-Path $taskProject ('build\progress_fixture_'+[Guid]::NewGuid().ToString('N')+' 한글 space')
try {
    & powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File (Join-Path $taskProject 'distribution\ProgressMeasure.ps1') -ProcessId $PID -Countdown 0 -Seconds 2 -IntervalMs 250 -OutputDirectory $taskFolder -Label fixture | Out-Host
    Assert-Measure ($LASTEXITCODE -eq 0) 'Standalone Windows PowerShell script succeeds.'
    $taskFiles=@(Get-ChildItem -LiteralPath $taskFolder -Filter '*.json')
    Assert-Measure ($taskFiles.Count -eq 1) 'Unique output JSON in Unicode/space path.'
    $taskJson=Get-Content -LiteralPath $taskFiles[0].FullName -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert-Measure ($taskJson.status -eq 'completed' -and $taskJson.samples -ge 2) 'Actual recorder produces samples.'
    Assert-Measure (-not $taskJson.loaded_dll_verified -and -not $taskJson.exact_progress_button_interval -and -not $taskJson.function_attribution_available) 'Do not claim game DLL loading, exact button timing or function attribution.'
    $taskCsv=@(Import-Csv -LiteralPath ($taskFiles[0].FullName -replace '\.json$','.csv'))
    Assert-Measure ($taskCsv.Count -eq $taskJson.samples -and @($taskJson.rows).Count -eq $taskJson.samples) 'JSON and CSV row counts agree.'
} finally {
    # Only delete this newly generated fixture after checking its absolute scope.
    $taskResolved=[IO.Path]::GetFullPath($taskFolder)
    $taskBuild=[IO.Path]::GetFullPath((Join-Path $taskProject 'build'))+[IO.Path]::DirectorySeparatorChar
    if(-not $taskResolved.StartsWith($taskBuild,[StringComparison]::OrdinalIgnoreCase) -or -not ([IO.Path]::GetFileName($taskResolved).StartsWith('progress_fixture_'))) { throw 'Unsafe cleanup path.' }
    if(Test-Path -LiteralPath $taskResolved) { Remove-Item -LiteralPath $taskResolved -Recurse -Force }
}
$taskReport=@{status='progress_measure_fixture_passed';checks=$script:taskChecks;gameplay_verified=$false;windows_accounting_verified=$true}
[IO.File]::WriteAllText((Join-Path $taskProject 'build\progress_measure_fixture.json'),($taskReport | ConvertTo-Json),(New-Object Text.UTF8Encoding($false)))
$taskReport | ConvertTo-Json
