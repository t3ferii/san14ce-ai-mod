# AI trace checks: regenerates the node table from the plain EXE, builds and runs the trace fixture
# (the real native_ai_trace.inl against a fake image) and the offline analyzer. The tracer itself is
# part of SAN14_Convenience.dll ([diagnostics] ai_trace / ai_trace_original). Writes only build\development.
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build\development\ai_trace'
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
        & python -B -X utf8 (Join-Path $taskProject 'tools\gen_ai_trace_nodes.py')
        if($LASTEXITCODE -ne 0){throw 'Node table generation failed.'}
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\ai_trace_fixture.cpp') /Feai_trace_fixture.exe /link kernel32.lib
        if($LASTEXITCODE -ne 0){throw 'Trace fixture build failed.'}
        $taskResult=& '.\ai_trace_fixture.exe' (Join-Path $taskBuild 'fixture_trace.csv')
        if($LASTEXITCODE -ne 0){throw 'Trace fixture failed.'}
        $taskResult
        & python -B -X utf8 (Join-Path $taskProject 'tools\analyze_ai_trace.py') (Join-Path $taskBuild 'fixture_trace.csv') --out (Join-Path $taskBuild 'fixture_report')
        if($LASTEXITCODE -ne 0){throw 'Trace analysis of fixture output failed.'}
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
