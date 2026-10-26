param()
$ErrorActionPreference='Stop'
$rebuildProject=Split-Path -Parent $PSScriptRoot
$rebuildOutput=Join-Path $rebuildProject 'build\development\combat_v2'
[void][IO.Directory]::CreateDirectory($rebuildOutput)
$rebuildVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$rebuildSdk='C:\Program Files (x86)\Windows Kits\10';$rebuildSdkVersion='10.0.26100.0'
$rebuildSavedPath=$env:PATH;$rebuildSavedInclude=$env:INCLUDE;$rebuildSavedLib=$env:LIB
try {
    $env:PATH="$rebuildVc\bin\Hostx64\x64;$rebuildSavedPath"
    $env:INCLUDE="$rebuildVc\include;$rebuildSdk\Include\$rebuildSdkVersion\ucrt;$rebuildSdk\Include\$rebuildSdkVersion\shared;$rebuildSdk\Include\$rebuildSdkVersion\um"
    $env:LIB="$rebuildVc\lib\x64;$rebuildSdk\Lib\$rebuildSdkVersion\ucrt\x64;$rebuildSdk\Lib\$rebuildSdkVersion\um\x64"
    Push-Location -LiteralPath $rebuildOutput
    try {
        $rebuildFlags=@('/nologo','/W4','/WX','/utf-8','/O2','/MT','/std:c++17','/EHsc')
        & cl.exe @rebuildFlags /c (Join-Path $rebuildProject 'src\combat_v2\sortie.cpp') /Fosortie.obj
        if($LASTEXITCODE -ne 0){throw 'Combat v2 compilation failed.'}
        & cl.exe @rebuildFlags /c (Join-Path $rebuildProject 'src\combat_v2\movement.cpp') /Fomovement.obj
        if($LASTEXITCODE -ne 0){throw 'Combat v2 movement compilation failed.'}
        & cl.exe @rebuildFlags /c (Join-Path $rebuildProject 'src\combat_v2\legacy_features.cpp') /Folegacy_features.obj
        if($LASTEXITCODE -ne 0){throw 'Transferred combat features compilation failed.'}
        & lib.exe /nologo /OUT:SAN14_CombatAI_v2_unlinked.lib sortie.obj movement.obj legacy_features.obj
        if($LASTEXITCODE -ne 0){throw 'Combat v2 library build failed.'}
        & cl.exe @rebuildFlags (Join-Path $rebuildProject 'tests\combat_v2_sortie_fixture.cpp') SAN14_CombatAI_v2_unlinked.lib /Fecombat_v2_sortie_fixture.exe
        if($LASTEXITCODE -ne 0){throw 'Combat v2 fixture compilation failed.'}
        $rebuildResult=& '.\combat_v2_sortie_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Combat v2 fixture failed.'}
        $rebuildResult
        [IO.File]::WriteAllText((Join-Path $rebuildOutput 'fixture.json'),($rebuildResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        & cl.exe @rebuildFlags (Join-Path $rebuildProject 'tests\combat_v2_movement_fixture.cpp') SAN14_CombatAI_v2_unlinked.lib /Fecombat_v2_movement_fixture.exe
        if($LASTEXITCODE -ne 0){throw 'Combat v2 movement fixture compilation failed.'}
        $rebuildMovementResult=& '.\combat_v2_movement_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Combat v2 movement fixture failed.'}
        $rebuildMovementResult
        [IO.File]::WriteAllText((Join-Path $rebuildOutput 'movement_fixture.json'),($rebuildMovementResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        & cl.exe @rebuildFlags (Join-Path $rebuildProject 'tests\combat_v2_features_fixture.cpp') SAN14_CombatAI_v2_unlinked.lib /Fecombat_v2_features_fixture.exe
        if($LASTEXITCODE -ne 0){throw 'Transferred combat feature fixture compilation failed.'}
        $rebuildFeatureResult=& '.\combat_v2_features_fixture.exe'
        if($LASTEXITCODE -ne 0){throw 'Transferred combat feature fixture failed.'}
        $rebuildFeatureResult
        [IO.File]::WriteAllText((Join-Path $rebuildOutput 'features_fixture.json'),($rebuildFeatureResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
    } finally {Pop-Location}
} finally {$env:PATH=$rebuildSavedPath;$env:INCLUDE=$rebuildSavedInclude;$env:LIB=$rebuildSavedLib}
& python -X utf8 (Join-Path $rebuildProject 'tests\verify_combat_rebuild.py')
if($LASTEXITCODE -ne 0){throw 'Combat v2 separation audit failed.'}
Write-Output 'Unlinked static library only. No runtime DLL, installer, hooks or reference binary executed.'
