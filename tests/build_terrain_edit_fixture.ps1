$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build\development\terrain_edit'
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
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\terrain_edit_fixture.cpp') /Feterrain_edit_fixture.exe /link kernel32.lib
        if($LASTEXITCODE -ne 0){throw 'Terrain edit fixture compilation failed.'}
        $taskResult=& '.\terrain_edit_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Terrain edit fixture failed.'}
        $taskResult
        [IO.File]::WriteAllText((Join-Path $taskBuild 'result.json'),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc /DNO_RUNTIME_LOG (Join-Path $taskProject 'tests\terrain_edit_fixture.cpp') /Feterrain_edit_release_fixture.exe /link kernel32.lib
        if($LASTEXITCODE -ne 0){throw 'Terrain edit release fixture compilation failed.'}
        $taskRelease=& '.\terrain_edit_release_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Terrain edit release fixture failed.'}
        $taskRelease
        [IO.File]::WriteAllText((Join-Path $taskBuild 'release_result.json'),($taskRelease | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
