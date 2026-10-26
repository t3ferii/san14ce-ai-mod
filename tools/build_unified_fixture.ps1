$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build\development\unified_personnel'
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
        & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\officer_query_fixture.cpp') /Feofficer_query_fixture.exe
        if($LASTEXITCODE -ne 0){throw 'Officer search engine fixture compilation failed.'}
        $taskQueryResult=& '.\officer_query_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Officer search engine fixture failed.'}
        $taskQueryResult
        & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\unified_personnel_fixture.cpp') /Feunified_personnel_fixture.exe
        if($LASTEXITCODE -ne 0){throw 'Unified personnel fixture compilation failed.'}
        $taskResult=& '.\unified_personnel_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Unified personnel fixture failed.'}
        $taskResult | Set-Content -Encoding UTF8 result.json
        $taskResult
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\unified_native_fixture.cpp') /Feunified_native_fixture.exe /link kernel32.lib
        if($LASTEXITCODE -ne 0){throw 'Unified native fixture compilation failed.'}
        $taskNativeResult=& '.\unified_native_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Unified native fixture failed.'}
        $taskNativeResult | Set-Content -Encoding UTF8 native_result.json
        $taskNativeResult
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\unified_panes_fixture.cpp') /Feunified_engine_fixture.exe /link kernel32.lib
        if($LASTEXITCODE -ne 0){throw 'Unified game UI fixture compilation failed.'}
        $taskEngineResult=& '.\unified_engine_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Unified game UI fixture failed.'}
        $taskEngineResult | Set-Content -Encoding UTF8 engine_result.json
        $taskEngineResult
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\unified_relations_fixture.cpp') /Feunified_relations_fixture.exe /link kernel32.lib
        if($LASTEXITCODE -ne 0){throw 'Native sortie relation fixture compilation failed.'}
        $taskRelationResult=& '.\unified_relations_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Native sortie relation fixture failed.'}
        $taskRelationResult | Set-Content -Encoding UTF8 relations_result.json
        $taskRelationResult
        & python -X utf8 (Join-Path $taskProject 'tests\verify_sortie_relation.py')
        if($LASTEXITCODE -ne 0){throw 'Current executable sortie relation trace verification failed.'}
        & python -X utf8 (Join-Path $taskProject 'tests\verify_unified_panes.py')
        if($LASTEXITCODE -ne 0){throw 'Native two-pane geometry and class verification failed.'}
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\unified_overlay_fixture.cpp') /Feunified_overlay_fixture.exe /link kernel32.lib user32.lib gdi32.lib d3d11.lib dxgi.lib
        if($LASTEXITCODE -ne 0){throw 'In-game button fixture compilation failed.'}
        $taskOverlayResult=& '.\unified_overlay_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'In-game button pixel/input fixture failed.'}
        $taskOverlayResult | Set-Content -Encoding UTF8 overlay_result.json
        $taskOverlayResult
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
