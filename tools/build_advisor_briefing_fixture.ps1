param()
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'build_advisor_shaders.ps1')
& python -B -X utf8 (Join-Path $taskProject 'tests/verify_advisor_briefing.py')
if($LASTEXITCODE -ne 0){throw 'Advisor native evidence rejected'}
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10';$taskVersion='10.0.26100.0'
$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
    Push-Location -LiteralPath (Join-Path $taskProject 'build/development')
    try {
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests/advisor_briefing_fixture.cpp') /Feadvisor_briefing_fixture.exe /link kernel32.lib user32.lib
        if($LASTEXITCODE -ne 0){throw 'Advisor behavior fixture build failed'}
        $taskResult=& './advisor_briefing_fixture.exe' (Join-Path $taskProject 'legacy/FogMod_Source/native_game_analysis/running_code_text.bin')
        if($LASTEXITCODE -ne 0){throw 'Advisor behavior fixture failed'}
        $taskResult
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests/advisor_overlay_fixture.cpp') /Feadvisor_overlay_fixture.exe /link kernel32.lib user32.lib gdi32.lib d3d11.lib dxguid.lib
        if($LASTEXITCODE -ne 0){throw 'Advisor renderer fixture build failed'}
        $taskGraphics=& './advisor_overlay_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Advisor renderer fixture failed'}
        $taskGraphics
        [IO.File]::WriteAllText((Join-Path $taskProject 'build/advisor_briefing_fixture.json'),($taskResult|Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        [IO.File]::WriteAllText((Join-Path $taskProject 'build/advisor_overlay_fixture.json'),($taskGraphics|Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
