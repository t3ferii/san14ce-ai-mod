param([switch]$Sortie)
# Unit-test shells of native_adapter.cpp (TACTICS_FIXTURE: every module in one translation unit). Since plan decision D-1 (user 2026-11-07,
# "split build, retire the old way") this is NOT a DLL build: the integrated DLL, the candidate DLLs and the old input-loader/graphics-proxy
# builds are retired. DLLs come only from tools/build_split.ps1.
$ErrorActionPreference = 'Stop'
$taskProject = Split-Path -Parent $PSScriptRoot
$taskBuild = Join-Path $taskProject 'build'
$taskVc = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk = 'C:\Program Files (x86)\Windows Kits\10'
$taskSdkVersion = '10.0.26100.0'
$taskSavedPath = $env:PATH
$taskSavedInclude = $env:INCLUDE
$taskSavedLib = $env:LIB
try {
    $env:PATH = "$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE = "$taskVc\include;$taskSdk\Include\$taskSdkVersion\ucrt;$taskSdk\Include\$taskSdkVersion\shared;$taskSdk\Include\$taskSdkVersion\um"
    $env:LIB = "$taskVc\lib\x64;$taskSdk\Lib\$taskSdkVersion\ucrt\x64;$taskSdk\Lib\$taskSdkVersion\um\x64"
    [void][IO.Directory]::CreateDirectory($taskBuild)
    Push-Location -LiteralPath $taskBuild
    try {
        if ($Sortie) {
            & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc /DTACTICS_FIXTURE /DSORTIE_FORMATION ('/I' + (Join-Path $taskProject 'src')) (Join-Path $taskProject 'tests\native_fixture.cpp') /Fesortie_fixture.exe /link kernel32.lib
        } else {
            & cl.exe /nologo /W4 /utf-8 /O2 /MT /std:c++17 /EHsc /DTACTICS_FIXTURE ('/I' + (Join-Path $taskProject 'src')) (Join-Path $taskProject 'tests\native_fixture.cpp') /Fenative_fixture.exe /link kernel32.lib
        }
        if ($LASTEXITCODE -ne 0) { throw 'Native fixture compilation failed.' }
    } finally { Pop-Location }
} finally {
    $env:PATH = $taskSavedPath; $env:INCLUDE = $taskSavedInclude; $env:LIB = $taskSavedLib
}
