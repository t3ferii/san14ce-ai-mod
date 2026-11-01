$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build\development\terrain_edit'
[void][IO.Directory]::CreateDirectory($taskBuild)
& python -B -X utf8 (Join-Path $PSScriptRoot 'terrain_edit_verify.py')
if($LASTEXITCODE -ne 0){throw 'Terrain edit static verification failed.'}
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10';$taskVersion='10.0.26100.0'
$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
    Push-Location -LiteralPath $taskBuild
    try {
        foreach($taskName in @('ui','split')) {
            foreach($taskMode in @('diagnostic','release')) {
                $taskFlags=@('/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc')
                if($taskMode -eq 'release'){$taskFlags+='/DNO_RUNTIME_LOG'}
                if($taskName -eq 'split'){$taskFlags+=@('/DSPLIT_MODULE_FIXTURE','/DMERIT_CANDIDATE','/DRECRUITMENT_UI_CANDIDATE','/DHIDDEN_LOYALTY','/DINJURY_PROTECTION','/DAWARD_WAREHOUSE','/DSORTIE_FORMATION')}
                $taskExe="terrain_edit_${taskName}_${taskMode}.exe"
                & cl.exe @taskFlags (Join-Path $PSScriptRoot "terrain_edit_${taskName}_fixture.cpp") "/Fe$taskExe" /link kernel32.lib user32.lib gdi32.lib bcrypt.lib dxguid.lib
                if($LASTEXITCODE -ne 0){throw "Terrain edit $taskName $taskMode compilation failed."}
                $taskResult=& ".\$taskExe"
                if($LASTEXITCODE -ne 0){throw "Terrain edit $taskName $taskMode fixture failed."}
                $taskResult
                [IO.File]::WriteAllText((Join-Path $taskBuild "${taskName}_${taskMode}_result.json"),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            }
        }
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
