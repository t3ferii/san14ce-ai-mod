$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10';$taskVersion='10.0.26100.0'
$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
    Push-Location -LiteralPath (Join-Path $taskProject 'build/development')
    try {
        foreach($taskMode in @('diagnostic','release')) {
            $taskFlags=@('/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc')
            if($taskMode -eq 'release'){$taskFlags+='/DNO_RUNTIME_LOG'}
            $taskExe="ccz_settings_$taskMode.exe"
            & cl.exe @taskFlags (Join-Path $PSScriptRoot 'ccz_settings_fixture.cpp') "/Fe$taskExe" /link kernel32.lib user32.lib gdi32.lib d3d11.lib dxguid.lib
            if($LASTEXITCODE -ne 0){throw "CCZ $taskMode fixture build failed"}
            & ".\$taskExe" (Join-Path $taskProject 'legacy/FogMod_Source/native_game_analysis/running_code_text.bin') (Join-Path (Get-Location) "ccz_settings_$taskMode.ini") (Join-Path (Get-Location) "ccz_settings_$taskMode.bmp")
            if($LASTEXITCODE -ne 0){throw "CCZ $taskMode fixture failed"}

        }
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
