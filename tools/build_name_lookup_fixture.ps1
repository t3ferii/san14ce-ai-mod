$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build\development\name_lookup'
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
        & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\name_lookup_fixture.cpp') /Fename_lookup_fixture.exe /link kernel32.lib
        if($LASTEXITCODE -ne 0){throw 'Name lookup fixture build failed.'}
        # Original bytes: DRM-free analysis copy and the captured running .text.
        $taskResult=& '.\name_lookup_fixture.exe' (Join-Path $taskProject 'build\ui_native_analysis\editor83_plain\game.exe.unpacked.exe') (Join-Path $taskProject 'legacy\FogMod_Source\native_game_analysis\running_code_text.bin')
        if($LASTEXITCODE -ne 0){$taskResult;throw 'Name lookup equivalence fixture failed.'}
        [IO.File]::WriteAllText((Join-Path $taskProject 'build\name_lookup_fixture.json'),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        $taskResult
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
