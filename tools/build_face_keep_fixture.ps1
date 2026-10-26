param()
# face_keep: static guard check against the analysis EXE/running .text + mock of the pin decision.
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
& python -B -X utf8 (Join-Path $taskProject 'tests/verify_face_keep.py')
if($LASTEXITCODE -ne 0){throw 'Face keep static check failed'}
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10';$taskVersion='10.0.26100.0'
$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
    Push-Location -LiteralPath (Join-Path $taskProject 'build/development')
    try {
        & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests/face_keep_fixture.cpp') /Feface_keep_fixture.exe /link kernel32.lib
        if($LASTEXITCODE -ne 0){throw 'Face keep fixture build failed'}
        $taskResult=& './face_keep_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Face keep fixture failed'}
        [IO.File]::WriteAllText((Join-Path $taskProject 'build/face_keep_fixture.json'),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        $taskResult
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
