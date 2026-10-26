# Public source checks: no game files, imported payloads, installation or live process access.
param([switch]$Combat)
$ErrorActionPreference = 'Stop'
$taskProject = Split-Path -Parent $PSScriptRoot
$taskOut = Join-Path $taskProject 'build\development\public_smoke'
$taskVc = $env:VCToolsInstallDir
if (-not $taskVc) {
    $taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $taskVswhere)) { throw 'Install Visual Studio C++ desktop tools or set VCToolsInstallDir.' }
    $taskVs = & $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $taskVs) { throw 'Visual Studio x64 C++ tools not found.' }
    $taskVc = (Get-ChildItem -LiteralPath (Join-Path $taskVs 'VC\Tools\MSVC') -Directory | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1).FullName
}
$taskSdk = $env:WindowsSdkDir
if (-not $taskSdk) { $taskSdk = (Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots').KitsRoot10 }
$taskVersion = (Get-ChildItem -LiteralPath (Join-Path $taskSdk 'Include') -Directory | Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'um\Windows.h') } | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1).Name
if (-not $taskVersion) { throw 'Windows 10/11 SDK with x64 libraries not found.' }
$taskCl = Join-Path $taskVc 'bin\Hostx64\x64\cl.exe'
if (-not (Test-Path -LiteralPath $taskCl)) { throw 'x64 compiler not found.' }
$taskSavedPath = $env:PATH; $taskSavedInclude = $env:INCLUDE; $taskSavedLib = $env:LIB
[void][IO.Directory]::CreateDirectory($taskOut)
try {
    $env:PATH = "$(Split-Path -Parent $taskCl);$taskSavedPath"
    $env:INCLUDE = "$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
    $env:LIB = "$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
    Push-Location -LiteralPath $taskOut
    try {
        & $taskCl /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc /LD (Join-Path $taskProject 'src\module_loader.cpp') (Join-Path $taskProject 'src\input_proxy.cpp') /Fedinput8.dll /link kernel32.lib
        if ($LASTEXITCODE -ne 0) { throw 'Public loader compilation failed.' }
        if ($Combat) {
            # Same release defines/link libraries as the combat arm of build_split.ps1.
            # SPLIT_COMBAT does not consume the omitted convenience payloads.
            $taskCombatFlags = @('/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc','/DMERIT_CANDIDATE','/DRECRUITMENT_UI_CANDIDATE','/DHIDDEN_LOYALTY','/DINJURY_PROTECTION','/DAWARD_WAREHOUSE','/DSORTIE_FORMATION','/DNO_RUNTIME_LOG','/DSPLIT_COMBAT')
            & $taskCl @taskCombatFlags /LD (Join-Path $taskProject 'src\native_adapter.cpp') /FeSAN14_CombatAI.dll /link kernel32.lib user32.lib
            if ($LASTEXITCODE -ne 0) { throw 'Public combat module compilation failed.' }
            Write-Output 'Unsigned combat module built; no installation or gameplay verification.'
        }
        foreach ($taskFixture in @('path_fast_fixture', 'officer_query_fixture')) {
            & $taskCl /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject "tests\$taskFixture.cpp") "/Fe$taskFixture.exe"
            if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $taskFixture" }
            & (Join-Path $taskOut "$taskFixture.exe")
            if ($LASTEXITCODE -ne 0) { throw "Fixture failed: $taskFixture" }
        }
        Write-Output 'Public source smoke checks passed; gameplay_verified=false; no installation.'
    } finally { Pop-Location }
} finally {
    $env:PATH = $taskSavedPath; $env:INCLUDE = $taskSavedInclude; $env:LIB = $taskSavedLib
}
