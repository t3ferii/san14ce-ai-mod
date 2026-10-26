param([switch]$Fixture,[switch]$RuntimeDiagnostics,[switch]$RuntimeCosts)
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build\development\loader_split'
if($RuntimeCosts){$RuntimeDiagnostics=$true}
if($RuntimeDiagnostics){
    if($Fixture){throw 'Choose fixtures or runtime diagnostics.'}
    $taskBuild=Join-Path $taskProject 'build\development\loader_split_diagnostic'
}
if($RuntimeCosts){$taskBuild=Join-Path $taskProject 'build\development\loader_split_cost_profile'}
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
        $taskFlags=@('/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc','/DMERIT_CANDIDATE','/DRECRUITMENT_UI_CANDIDATE','/DHIDDEN_LOYALTY','/DINJURY_PROTECTION','/DAWARD_WAREHOUSE','/DSORTIE_FORMATION')
        $taskDiagnosticLink=@()
        if($RuntimeDiagnostics){$taskFlags+=@('/Zi','/DRUNTIME_COST_PROFILE');$taskDiagnosticLink=@('/DEBUG:FULL','/OPT:REF','/OPT:ICF','/MAP')}
        if($RuntimeCosts){$taskFlags+=@('/DRUNTIME_COST_PROFILE','/DNO_RUNTIME_LOG')}
        if($Fixture){$taskFlags+='/DSPLIT_MODULE_FIXTURE'}
        elseif(-not $RuntimeDiagnostics) {$taskFlags+='/DNO_RUNTIME_LOG'}
        $taskBase=if($Fixture){'SplitBaseFixture.dll'}else{'SAN14_Convenience.dll'}
        $taskCombat=if($Fixture){'SplitCombatFixture.dll'}else{'SAN14_CombatAI.dll'}
        $taskModpackObjects=@()
        if(-not $Fixture) {
            & python -B -X utf8 (Join-Path $taskProject 'tools\prepare_traits9.py') $taskBuild
            if($LASTEXITCODE -ne 0){throw 'Traits9 source preparation failed.'}
            & python -B -X utf8 (Join-Path $taskProject 'tools\import_modpack_objects.py') $taskBuild
            if($LASTEXITCODE -ne 0){throw 'Modpack static import failed.'}
            $taskModpackObjects=@('hover.obj','deaths.obj')
        }
        $taskBaseSources=@((Join-Path $taskProject 'src\native_adapter.cpp'))
        if(-not $Fixture){$taskBaseSources+=(Join-Path $taskProject 'src\gpu_profile.cpp')}
        & cl.exe @taskFlags "/I$taskBuild" /DSPLIT_BASE /LD @taskBaseSources @taskModpackObjects "/Fe$taskBase" /link @taskDiagnosticLink kernel32.lib user32.lib gdi32.lib bcrypt.lib dxguid.lib
        if($LASTEXITCODE -ne 0){throw 'Other-module build failed.'}
        & cl.exe @taskFlags /DSPLIT_COMBAT /LD (Join-Path $taskProject 'src\native_adapter.cpp') "/Fe$taskCombat" /link @taskDiagnosticLink kernel32.lib user32.lib
        if($LASTEXITCODE -ne 0){throw 'Combat-module build failed.'}
        if(-not $Fixture) {
            & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc /LD (Join-Path $taskProject 'src\module_loader.cpp') (Join-Path $taskProject 'src\input_proxy.cpp') /Fedinput8.dll /link kernel32.lib
            if($LASTEXITCODE -ne 0){throw 'Loader-only build failed.'}
        }
        if(-not $Fixture -and -not $RuntimeDiagnostics) {
            & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc "/I$taskBuild" "/I$(Join-Path $taskProject 'src')" (Join-Path $taskProject 'tests\modpack_fixture.cpp') @taskModpackObjects /Femodpack_fixture.exe /link kernel32.lib user32.lib bcrypt.lib
            if($LASTEXITCODE -ne 0){throw 'Modpack fixture build failed.'}
            $taskModpackResult=& '.\modpack_fixture.exe'
            if($LASTEXITCODE -ne 0){throw 'Modpack fixture failed.'}
            [IO.File]::WriteAllText((Join-Path $taskProject 'build\modpack_fixture.json'),($taskModpackResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            $taskModpackResult
            & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\pure_loader_fixture.cpp') /Feloader_fixture.exe /link kernel32.lib dxguid.lib
            if($LASTEXITCODE -ne 0){throw 'Loader fixture build failed.'}
            & cl.exe /nologo /W4 /WX /O2 /MT /LD /DMOCK_COMBAT (Join-Path $taskProject 'tests\loader_mock.cpp') /FeLoaderMockCombat.dll /link kernel32.lib
            if($LASTEXITCODE -ne 0){throw 'Mock combat build failed.'}
            & cl.exe /nologo /W4 /WX /O2 /MT /LD /DMOCK_COMBAT /DMOCK_BAD_ABI (Join-Path $taskProject 'tests\loader_mock.cpp') /FeLoaderMockBadCombat.dll /link kernel32.lib
            if($LASTEXITCODE -ne 0){throw 'Mock incompatible combat build failed.'}
            & cl.exe /nologo /W4 /WX /O2 /MT /LD (Join-Path $taskProject 'tests\loader_mock.cpp') /FeLoaderMockConvenience.dll /link kernel32.lib
            if($LASTEXITCODE -ne 0){throw 'Mock convenience build failed.'}
            & python -B -X utf8 (Join-Path $taskProject 'tests\verify_loader.py')
            if($LASTEXITCODE -ne 0){throw 'Pure loader fixture failed.'}
            & python -B -X utf8 (Join-Path $taskProject 'tests\verify_split_exports.py')
            if($LASTEXITCODE -ne 0){throw 'Split release exports rejected.'}
        }
        if($Fixture) {
            & cl.exe @taskFlags (Join-Path $taskProject 'tests\split_module_fixture.cpp') /Fesplit_module_fixture.exe /link kernel32.lib user32.lib
            if($LASTEXITCODE -ne 0){throw 'Split module fixture build failed.'}
            $taskResult=& '.\split_module_fixture.exe' (Join-Path $taskProject 'legacy\FogMod_Source\native_game_analysis\running_code_text.bin')
            if($LASTEXITCODE -ne 0){throw 'Split module integration fixture failed.'}
            [IO.File]::WriteAllText((Join-Path $taskProject 'build\split_module_fixture.json'),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            $taskResult
            $taskFailure=& '.\split_module_fixture.exe' (Join-Path $taskProject 'legacy\FogMod_Source\native_game_analysis\running_code_text.bin') '--failure'
            if($LASTEXITCODE -ne 0){throw 'Partial split publication rollback failed.'}
            [IO.File]::WriteAllText((Join-Path $taskProject 'build\split_module_failure_fixture.json'),($taskFailure | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            $taskFailure
            foreach($taskCompat in @('--compat','--unified','--merit','--save','--off','--empty','--combat-guard','--base-failure','--combat-failure','--base-prepare','--both-failure','--recheck','--briefing','--briefing-guard')) {
                $taskCompatResult=& '.\split_module_fixture.exe' (Join-Path $taskProject 'legacy\FogMod_Source\native_game_analysis\running_code_text.bin') $taskCompat
                if($LASTEXITCODE -ne 0){throw "Split compat fixture failed: $taskCompat"}
                $taskCompatResult
            }
        }
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
