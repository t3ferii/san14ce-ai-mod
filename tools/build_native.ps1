param([switch]$Fixture, [switch]$LoaderFixture, [switch]$InputLoader, [switch]$InputFixture, [switch]$PersonnelCandidate, [switch]$FriendshipCandidate, [switch]$MeritCandidate, [switch]$RecruitmentUICandidate, [switch]$Development, [switch]$Sortie)
$ErrorActionPreference = 'Stop'
$taskProject = Split-Path -Parent $PSScriptRoot
$taskBuild = Join-Path $taskProject 'build'
if ($Development) {
    if(-not $InputLoader -or -not $RecruitmentUICandidate -or $Fixture -or $LoaderFixture -or $InputFixture -or $PersonnelCandidate -or $FriendshipCandidate -or $MeritCandidate) {
        throw 'Development uses only the integrated -InputLoader -RecruitmentUICandidate variant.'
    }
    $taskBuild = Join-Path $taskBuild 'development'
}
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
        if ($RecruitmentUICandidate) {
            if(-not $InputLoader -or $PersonnelCandidate -or $FriendshipCandidate -or $MeritCandidate){throw 'Use -InputLoader -RecruitmentUICandidate; includes the existing merit component.'}
            & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc /DMERIT_CANDIDATE /DRECRUITMENT_UI_CANDIDATE /DHIDDEN_LOYALTY /DINJURY_PROTECTION /DAWARD_WAREHOUSE /DSORTIE_FORMATION /LD (Join-Path $taskProject 'src\native_adapter.cpp') (Join-Path $taskProject 'src\input_proxy.cpp') /FeSAN14_AIInput_recruitment_ui_candidate.dll /link kernel32.lib user32.lib
        } elseif ($MeritCandidate) {
            if(-not $InputLoader -or $PersonnelCandidate -or $FriendshipCandidate){throw 'Use -InputLoader -MeritCandidate for the isolated merit candidate.'}
            & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc /DMERIT_CANDIDATE /LD (Join-Path $taskProject 'src\native_adapter.cpp') (Join-Path $taskProject 'src\input_proxy.cpp') /FeSAN14_AIInput_merit_candidate.dll /link kernel32.lib
        } elseif ($FriendshipCandidate) {
            if(-not $InputLoader -or $PersonnelCandidate){throw 'Use -InputLoader -FriendshipCandidate for the isolated friendship candidate.'}
            & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc /DFRIENDSHIP_CANDIDATE /LD (Join-Path $taskProject 'src\native_adapter.cpp') (Join-Path $taskProject 'src\input_proxy.cpp') /FeSAN14_AIInput_friendship_candidate.dll /link kernel32.lib
        } elseif ($PersonnelCandidate) {
            if(-not $InputLoader){throw 'Use -InputLoader -PersonnelCandidate for the isolated, uninstalled candidate.'}
            & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc /DPERSONNEL_CANDIDATE /LD (Join-Path $taskProject 'src\native_adapter.cpp') (Join-Path $taskProject 'src\input_proxy.cpp') /FeSAN14_AIInput_personnel_candidate.dll /link kernel32.lib
        } elseif ($InputFixture) {
            & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT (Join-Path $taskProject 'tests\input_fixture.cpp') /Feinput_fixture.exe /link kernel32.lib dinput8.lib dxguid.lib
        } elseif ($InputLoader) {
            & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc /LD (Join-Path $taskProject 'src\native_adapter.cpp') (Join-Path $taskProject 'src\input_proxy.cpp') /FeSAN14_AIInput.dll /link kernel32.lib
        } elseif ($LoaderFixture) {
            & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT (Join-Path $taskProject 'tests\loader_fixture.cpp') /Feloader_fixture.exe /link kernel32.lib d3d11.lib
        } elseif ($Fixture -and $Sortie) {
            & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc /DTACTICS_FIXTURE /DSORTIE_FORMATION ('/I' + (Join-Path $taskProject 'src')) (Join-Path $taskProject 'tests\native_fixture.cpp') /Fesortie_fixture.exe /link kernel32.lib
        } elseif ($Fixture) {
            & cl.exe /nologo /W4 /utf-8 /O2 /MT /std:c++17 /EHsc /DTACTICS_FIXTURE ('/I' + (Join-Path $taskProject 'src')) (Join-Path $taskProject 'tests\native_fixture.cpp') /Fenative_fixture.exe /link kernel32.lib
        } else {
            & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc /LD (Join-Path $taskProject 'src\native_adapter.cpp') (Join-Path $taskProject 'src\graphics_proxy.cpp') /FeSAN14_AITactics.dll /link kernel32.lib
        }
        if ($LASTEXITCODE -ne 0) { throw 'Native compilation failed.' }
    } finally { Pop-Location }
} finally {
    $env:PATH = $taskSavedPath; $env:INCLUDE = $taskSavedInclude; $env:LIB = $taskSavedLib
}
