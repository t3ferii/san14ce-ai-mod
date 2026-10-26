$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build\development\building_destruction\diagnostic'
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
        & python -B -X utf8 (Join-Path $taskProject 'tools\prepare_traits9.py') $taskBuild
        if($LASTEXITCODE -ne 0){throw 'Traits9 preparation failed.'}
        & python -B -X utf8 (Join-Path $taskProject 'tools\import_modpack_objects.py') $taskBuild
        if($LASTEXITCODE -ne 0){throw 'Modpack preparation failed.'}
        $taskFlags=@('/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc','/Zi','/DMERIT_CANDIDATE','/DRECRUITMENT_UI_CANDIDATE','/DHIDDEN_LOYALTY','/DINJURY_PROTECTION','/DAWARD_WAREHOUSE','/DSORTIE_FORMATION','/DSPLIT_BASE','/LD')
        & cl.exe @taskFlags "/I$taskBuild" (Join-Path $taskProject 'src\native_adapter.cpp') (Join-Path $taskProject 'src\gpu_profile.cpp') hover.obj deaths.obj /FeSAN14_Convenience.dll /link /DEBUG:FULL /OPT:REF /OPT:ICF /MAP kernel32.lib user32.lib gdi32.lib bcrypt.lib dxguid.lib
        if($LASTEXITCODE -ne 0){throw 'Convenience diagnostic build failed.'}
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
