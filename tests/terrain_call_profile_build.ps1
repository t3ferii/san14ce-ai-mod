$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
& python -B -X utf8 (Join-Path $PSScriptRoot 'test_analyze_terrain_profile.py')
if($LASTEXITCODE -ne 0){throw 'Terrain CPU log parser tests failed.'}
$taskBuild=Join-Path $taskProject 'build\development\terrain_call_profile'
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
        foreach($taskMode in @('diagnostic','release')) {
            $taskFlags=@('/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc')
            if($taskMode -eq 'release'){$taskFlags+='/DNO_RUNTIME_LOG'}
            $taskExe="terrain_call_profile_${taskMode}.exe"
            & cl.exe @taskFlags (Join-Path $PSScriptRoot 'terrain_call_profile_fixture.cpp') "/Fe$taskExe" /link kernel32.lib
            if($LASTEXITCODE -ne 0){throw "Terrain call profile $taskMode compilation failed."}
            & ".\$taskExe"
            if($LASTEXITCODE -ne 0){throw "Terrain call profile $taskMode fixture failed."}
            if($taskMode -eq 'release'){
                $taskText=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes((Join-Path $taskBuild $taskExe)))
                if($taskText.Contains('terrain_profile_on') -or $taskText.Contains('terrain_frame_cpu')){throw 'Terrain profiler must be absent from release.'}
            }
        }
    }finally{Pop-Location}
}finally{$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
