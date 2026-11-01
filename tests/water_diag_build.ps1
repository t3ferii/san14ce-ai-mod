$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build\development\water_diag'
[void][IO.Directory]::CreateDirectory($taskBuild)
& python -B -X utf8 (Join-Path $PSScriptRoot 'water_diag_verify.py')
if($LASTEXITCODE -ne 0){throw 'Water diagnostic source checks failed.'}
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
            $taskExe="water_diag_${taskMode}.exe"
            & cl.exe @taskFlags (Join-Path $PSScriptRoot 'water_diag_fixture.cpp') "/Fe$taskExe" /link kernel32.lib d3d11.lib d3dcompiler.lib bcrypt.lib dxguid.lib
            if($LASTEXITCODE -ne 0){throw "Water $taskMode fixture compilation failed."}
            $taskSavedError=$ErrorActionPreference
            $ErrorActionPreference='Continue'
            $taskResult=& ".\$taskExe" 2>&1
            $taskExit=$LASTEXITCODE
            $ErrorActionPreference=$taskSavedError
            $taskResult
            if($taskExit -ne 0){throw "Water $taskMode fixture failed (exit $taskExit)."}
            [IO.File]::WriteAllText((Join-Path $taskBuild "${taskMode}_result.json"),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        }
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
