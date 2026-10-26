param()
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10'
$taskSdkVersion='10.0.26100.0'
$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskSdkVersion\ucrt;$taskSdk\Include\$taskSdkVersion\shared;$taskSdk\Include\$taskSdkVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskSdkVersion\ucrt\x64;$taskSdk\Lib\$taskSdkVersion\um\x64"
    Push-Location -LiteralPath (Join-Path $taskProject 'build\development')
    try {
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\target_retreat_fixture.cpp') /Fetarget_retreat_fixture.exe /link kernel32.lib
        if($LASTEXITCODE -ne 0){throw 'Target retreat fixture build failed.'}
        $taskResult=& '.\target_retreat_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Target retreat fixture failed.'}
        [IO.File]::WriteAllText((Join-Path $taskProject 'build\target_retreat_fixture.json'),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        $taskResult
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
