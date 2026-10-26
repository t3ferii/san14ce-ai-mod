param()
# User/Claude decision 2026-10-25: diagnostic-only original path search counts.
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
& python -B -X utf8 (Join-Path $taskProject 'tests/verify_path_count.py')
if($LASTEXITCODE -ne 0){throw 'Path count static check failed'}
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10';$taskVersion='10.0.26100.0'
$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
    Push-Location -LiteralPath (Join-Path $taskProject 'build/development')
    try {
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests/path_count_fixture.cpp') /Fepath_count_fixture.exe /link kernel32.lib
        if($LASTEXITCODE -ne 0){throw 'Path count fixture build failed'}
        $taskResult=& './path_count_fixture.exe' (Join-Path $taskProject 'legacy/FogMod_Source/native_game_analysis/running_code_text.bin')
        if($LASTEXITCODE -ne 0){throw 'Path count fixture failed'}
        [IO.File]::WriteAllText((Join-Path $taskProject 'build/path_count_fixture.json'),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        $taskResult
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
