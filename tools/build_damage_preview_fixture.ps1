$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build\development\damage_preview'
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
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\damage_preview_fixture.cpp') /Fedamage_preview_fixture.exe /link kernel32.lib
        if($LASTEXITCODE -ne 0){throw 'Damage forecast fixture compilation failed.'}
        $taskResult=& '.\damage_preview_fixture.exe' (Join-Path $taskProject 'build\ui_native_analysis\editor83_plain\game.exe.unpacked.exe')
        if($LASTEXITCODE -ne 0){throw 'Damage forecast fixture failed.'}
        $taskResult
        [IO.File]::WriteAllText((Join-Path $taskBuild 'result.json'),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        & python -B -X utf8 (Join-Path $taskProject 'tools\verify_damage_preview_native.py')
        if($LASTEXITCODE -ne 0){throw 'Damage forecast current-EXE check failed.'}
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
