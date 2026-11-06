param([switch]$WithLayout)
# trait_hover: pure-logic fixture (src/trait_hover_profile.h only, no game image). -WithLayout also runs the Layout fixtures and the
# registry/blob checks that depend on the 156 "trait_hover" rows (those pass only after the registry work, plan S3).
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build\development\trait_hover'
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
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\trait_hover_fixture.cpp') /Fetrait_hover_fixture.exe /link kernel32.lib user32.lib
        if($LASTEXITCODE -ne 0){throw 'Trait hover fixture build failed.'}
        $taskResult=& '.\trait_hover_fixture.exe'
        if($LASTEXITCODE -ne 0){$taskResult;throw 'Trait hover fixture failed.'}
        [IO.File]::WriteAllText((Join-Path $taskProject 'build\trait_hover_fixture.json'),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        $taskResult
        if($WithLayout) {
            # same commands as build_split.ps1 (Layout fixtures): the real hook manager against the registry, including the trait_hover rows.
            & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\layout_fixture.cpp') /Felayout_fixture.exe /link kernel32.lib user32.lib
            if($LASTEXITCODE -ne 0){throw 'Layout fixture build failed.'}
            $taskLayout=& '.\layout_fixture.exe'
            if($LASTEXITCODE -ne 0){throw 'Layout fixture failed.'}
            $taskLayout
            & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\layout_p3_fixture.cpp') /Felayout_p3_fixture.exe /link kernel32.lib user32.lib
            if($LASTEXITCODE -ne 0){throw 'Layout P3 fixture build failed.'}
            $taskLayoutP3=& '.\layout_p3_fixture.exe'
            if($LASTEXITCODE -ne 0){throw 'Layout P3 fixture failed.'}
            $taskLayoutP3
            & python -B -X utf8 (Join-Path $taskProject 'tests\verify_modpack_static.py')
            if($LASTEXITCODE -ne 0){throw 'Modpack static verification failed.'}
        }
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
