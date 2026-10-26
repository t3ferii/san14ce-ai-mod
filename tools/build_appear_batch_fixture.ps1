param()
# appear_batch: static guard check against the analysis EXE/running .text + mock of the per-frame loop.
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
& python -B -X utf8 (Join-Path $taskProject 'tests/verify_appear_batch.py')
if($LASTEXITCODE -ne 0){throw 'Appear batch static check failed'}
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10';$taskVersion='10.0.26100.0'
$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
    Push-Location -LiteralPath (Join-Path $taskProject 'build/development')
    try {
        & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests/appear_batch_fixture.cpp') /Feappear_batch_fixture.exe /link kernel32.lib
        if($LASTEXITCODE -ne 0){throw 'Appear batch fixture build failed'}
        $taskResult=& './appear_batch_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Appear batch fixture failed'}
        [IO.File]::WriteAllText((Join-Path $taskProject 'build/appear_batch_fixture.json'),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        $taskResult
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
