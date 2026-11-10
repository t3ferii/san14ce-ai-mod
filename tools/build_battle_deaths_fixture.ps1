# battle_deaths: relay + table fixture (docs/HISTORY.md 4-1, step S2). Runs bdp::build_branch_relay output in an RWX page with raw-byte
# harness stubs; no game image, no game memory. Registry cross-check runs inside the fixture once the layout_registry.h rows exist (step S3).
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build\development\battle_deaths'
[void][IO.Directory]::CreateDirectory($taskBuild)
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10';$taskVersion='10.0.26100.0'
$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
    Push-Location -LiteralPath $taskBuild
    try {
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\battle_deaths_fixture.cpp') /Febattle_deaths_fixture.exe /link kernel32.lib user32.lib
        if($LASTEXITCODE -ne 0){throw 'Battle deaths fixture build failed.'}
        $taskResult=& '.\battle_deaths_fixture.exe'
        if($LASTEXITCODE -ne 0){$taskResult;throw 'Battle deaths fixture failed.'}
        [IO.File]::WriteAllText((Join-Path $taskProject 'build\battle_deaths_fixture.json'),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        $taskResult
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
