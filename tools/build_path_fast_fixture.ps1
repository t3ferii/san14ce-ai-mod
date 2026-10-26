param()
# User 2026-10-25: sampled verification, bounded detail logs, stateful helper and tie-order audit.
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
& python -B -X utf8 (Join-Path $taskProject 'tests/verify_path_fast.py')
if($LASTEXITCODE -ne 0){throw 'Path fast static check failed'}
# N must survive both menu catalogs, including a noninteractive UTF-16 rewrite.
$taskIntervalChecks=0
foreach($taskCatalog in @('distribution/FeatureSelect.ps1','FeatureSelect.ps1')) {
    . (Join-Path $taskProject $taskCatalog)
    foreach($taskInterval in @(-1,0,1,16,2147483647)) {
        $taskIni=@{diagnostics=[ordered]@{path_fast_verify=[string]$taskInterval}}
        $taskValues=Get-FeatureValues $taskIni
        if($taskValues['diagnostics.path_fast_verify'] -ne [Math]::Max(0,$taskInterval)){throw 'Path fast interval was normalized to boolean'}
        $taskFixtureIni=Join-Path $taskProject 'build/development/path_fast_settings_fixture.ini'
        Write-FeatureIni $taskFixtureIni $taskValues $taskIni
        $taskRoundtrip=Get-FeatureValues (Read-FeatureIni $taskFixtureIni)
        if($taskRoundtrip['diagnostics.path_fast_verify'] -ne [Math]::Max(0,$taskInterval)){throw 'Path fast interval lost during UTF-16 roundtrip'}
        $taskIntervalChecks+=2
    }
}
Write-Output ('{"status":"path_fast_interval_roundtrip_verified","checks":'+$taskIntervalChecks+'}')
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10';$taskVersion='10.0.26100.0'
$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
    Push-Location -LiteralPath (Join-Path $taskProject 'build/development')
    try {
        & cl.exe /nologo /W4 /WX /utf-8 /O2 /fp:strict /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests/path_fast_fixture.cpp') /Fepath_fast_fixture.exe
        if($LASTEXITCODE -ne 0){throw 'Path fast fixture build failed'}
        $taskResult=& './path_fast_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Path fast fixture failed'}
        [IO.File]::WriteAllText((Join-Path $taskProject 'build/path_fast_fixture.json'),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        $taskResult
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
