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
        $taskFlags=@('/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc','/DMERIT_CANDIDATE','/DRECRUITMENT_UI_CANDIDATE','/DHIDDEN_LOYALTY','/DINJURY_PROTECTION','/DAWARD_WAREHOUSE','/DSORTIE_FORMATION','/DLAYOUT_OWNS_PRESENT')   # 2026-11-05 user exception: Present/WndProc owned by SAN14_Layout.dll
        # W5-3 (docs/MODULARIZATION_PLAN.md): in every DLL build an unreferenced local function (C4505) is an error. The split fixture build
        # (SPLIT_MODULE_FIXTURE, flags also read by tools/run_checks.py) keeps /wd4505: it compiles fixture-only seams the production roots do not use.
        if(-not $Fixture){$taskFlags=@($taskFlags | ForEach-Object {if($_ -eq '/wd4505'){'/we4505'}else{$_}})}
        $taskDiagnosticLink=@()
        if($RuntimeDiagnostics){$taskFlags+=@('/Zi','/DRUNTIME_COST_PROFILE');$taskDiagnosticLink=@('/DEBUG:FULL','/OPT:REF','/OPT:ICF','/MAP')}
        if($RuntimeCosts){$taskFlags+=@('/DRUNTIME_COST_PROFILE','/DNO_RUNTIME_LOG')}
        if($Fixture){$taskFlags+='/DSPLIT_MODULE_FIXTURE'}
        elseif(-not $RuntimeDiagnostics) {$taskFlags+='/DNO_RUNTIME_LOG'}
        $taskBase=if($Fixture){'SplitBaseFixture.dll'}else{'SAN14_Convenience.dll'}
        $taskCombat=if($Fixture){'SplitCombatFixture.dll'}else{'SAN14_CombatAI.dll'}
        $taskBaseSources=@((Join-Path $taskProject 'src\native_adapter.cpp'))
        if(-not $Fixture){$taskBaseSources+=(Join-Path $taskProject 'src\gpu_profile.cpp')}
        & cl.exe @taskFlags "/I$taskBuild" /DSPLIT_BASE /LD @taskBaseSources "/Fe$taskBase" /link @taskDiagnosticLink kernel32.lib user32.lib gdi32.lib bcrypt.lib dxguid.lib
        if($LASTEXITCODE -ne 0){throw 'Other-module build failed.'}
        & cl.exe @taskFlags /DSPLIT_COMBAT /LD (Join-Path $taskProject 'src\native_adapter.cpp') "/Fe$taskCombat" /link @taskDiagnosticLink kernel32.lib user32.lib
        if($LASTEXITCODE -ne 0){throw 'Combat-module build failed.'}
        if(-not $Fixture) {   # 2026-11-01 user: 조조전 탭 = SAN14_CCZ.dll, loaded by the convenience module
            & cl.exe @taskFlags "/I$(Join-Path $taskProject 'src')" /LD (Join-Path $taskProject 'src\ccz_module.cpp') /FeSAN14_CCZ.dll /link @taskDiagnosticLink kernel32.lib user32.lib gdi32.lib
            if($LASTEXITCODE -ne 0){throw 'CCZ-module build failed.'}
        }
        if(-not $Fixture) {   # 2026-11-05 user exception: SAN14_Layout.dll = hook manager + Present/WndProc owner, loaded first by dinput8
            & cl.exe @taskFlags "/I$(Join-Path $taskProject 'src')" /LD (Join-Path $taskProject 'src\layout_main.cpp') /FeSAN14_Layout.dll /link @taskDiagnosticLink kernel32.lib user32.lib
            if($LASTEXITCODE -ne 0){throw 'Layout-module build failed.'}
        }
        if(-not $Fixture) {
            & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc /LD (Join-Path $taskProject 'src\module_loader.cpp') (Join-Path $taskProject 'src\input_proxy.cpp') /Fedinput8.dll /link kernel32.lib
            if($LASTEXITCODE -ne 0){throw 'Loader-only build failed.'}
        }
        if(-not $Fixture -and -not $RuntimeDiagnostics) {
            & cl.exe /nologo /W4 /WX /utf-8 /O2 /MT /std:c++17 /EHsc "/I$taskBuild" "/I$(Join-Path $taskProject 'src')" (Join-Path $taskProject 'tests\modpack_fixture.cpp') /Femodpack_fixture.exe /link kernel32.lib user32.lib bcrypt.lib
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
            # 2026-11-05 user exception: Layout loads first, Layout publishes last; Previous-input loop prevention.
            & cl.exe /nologo /W4 /WX /O2 /MT /LD /DMOCK_LAYOUT (Join-Path $taskProject 'tests\layout_loader_mock.cpp') /FeLayoutMock.dll /link kernel32.lib
            if($LASTEXITCODE -ne 0){throw 'Mock layout build failed.'}
            & cl.exe /nologo /W4 /WX /O2 /MT /LD /DMOCK_LAYOUT_FAIL (Join-Path $taskProject 'tests\layout_loader_mock.cpp') /FeLayoutMockFail.dll /link kernel32.lib
            if($LASTEXITCODE -ne 0){throw 'Mock failing layout build failed.'}
            & cl.exe /nologo /W4 /WX /O2 /MT /LD (Join-Path $taskProject 'tests\layout_loader_mock.cpp') /FeLayoutMockConvenience.dll /link kernel32.lib
            if($LASTEXITCODE -ne 0){throw 'Mock layout-order convenience build failed.'}
            & python -B -X utf8 (Join-Path $taskProject 'tests\verify_layout_loader.py')
            if($LASTEXITCODE -ne 0){throw 'Layout loader fixture failed.'}
            & python -B -X utf8 (Join-Path $taskProject 'tests\verify_split_exports.py')
            if($LASTEXITCODE -ne 0){throw 'Split release exports rejected.'}
        }
        if($Fixture) {
            & cl.exe @taskFlags (Join-Path $taskProject 'tests\split_module_fixture.cpp') /Fesplit_module_fixture.exe /link kernel32.lib user32.lib
            if($LASTEXITCODE -ne 0){throw 'Split module fixture build failed.'}
            $taskResult=& '.\split_module_fixture.exe' (Join-Path $taskProject 'build\ui_native_analysis\fixture\1a4a447d_text.bin')
            if($LASTEXITCODE -ne 0){throw 'Split module integration fixture failed.'}
            [IO.File]::WriteAllText((Join-Path $taskProject 'build\split_module_fixture.json'),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            $taskResult
            # 2026-11-05 user exception: SAN14_Layout.dll hook manager / Present chain / window subclass (real sources, synthetic image).
            & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\layout_fixture.cpp') /Felayout_fixture.exe /link kernel32.lib user32.lib
            if($LASTEXITCODE -ne 0){throw 'Layout fixture build failed.'}
            $taskLayout=& '.\layout_fixture.exe'
            if($LASTEXITCODE -ne 0){throw 'Layout fixture failed.'}
            [IO.File]::WriteAllText((Join-Path $taskProject 'build\layout_fixture.json'),($taskLayout | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            $taskLayout
            # 2026-11-05 P3 (user Layout compatibility design 8): every UI hook registry row through the real manager, yields, E9, 70E9C1, spans, whitelist, owner roll-back.
            & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\layout_p3_fixture.cpp') /Felayout_p3_fixture.exe /link kernel32.lib user32.lib
            if($LASTEXITCODE -ne 0){throw 'Layout P3 fixture build failed.'}
            $taskLayoutP3=& '.\layout_p3_fixture.exe'
            if($LASTEXITCODE -ne 0){throw 'Layout P3 fixture failed.'}
            [IO.File]::WriteAllText((Join-Path $taskProject 'build\layout_p3_fixture.json'),($taskLayoutP3 | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            $taskLayoutP3
            # 2026-11-05 P4 (layout built/destroyed events, runtime UI services, diagnostic probes): real Layout sources + FAKE game object model.
            & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\layout_p4_fixture.cpp') /Felayout_p4_fixture.exe /link kernel32.lib user32.lib
            if($LASTEXITCODE -ne 0){throw 'Layout P4 fixture build failed.'}
            $taskLayoutP4=& '.\layout_p4_fixture.exe'
            if($LASTEXITCODE -ne 0){throw 'Layout P4 fixture failed.'}
            [IO.File]::WriteAllText((Join-Path $taskProject 'build\layout_p4_fixture.json'),($taskLayoutP4 | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            $taskLayoutP4
            & python -B -X utf8 (Join-Path $taskProject 'tests\verify_layout_registry.py')
            if($LASTEXITCODE -ne 0){throw 'Layout registry static verification failed.'}
            $taskFailure=& '.\split_module_fixture.exe' (Join-Path $taskProject 'build\ui_native_analysis\fixture\1a4a447d_text.bin') '--failure'
            if($LASTEXITCODE -ne 0){throw 'Partial split publication rollback failed.'}
            [IO.File]::WriteAllText((Join-Path $taskProject 'build\split_module_failure_fixture.json'),($taskFailure | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            $taskFailure
            # 사용자 결정 2026-11-03: 통합 인사 제거. Keep shared CCZ/HUD guard coverage.
            foreach($taskCompat in @('--compat','--ccz','--ccz-guard','--merit','--save','--off','--empty','--combat-guard','--base-failure','--combat-failure','--base-prepare','--both-failure','--recheck','--briefing','--briefing-guard','--briefing-pointer-guard','--opt-merit','--opt-recruit','--opt-recruit-ai','--opt-injury','--opt-awards','--opt-loyalty','--opt-expert','--combat-alone','--base-alone')) {
                $taskCompatResult=& '.\split_module_fixture.exe' (Join-Path $taskProject 'build\ui_native_analysis\fixture\1a4a447d_text.bin') $taskCompat
                if($LASTEXITCODE -ne 0){throw "Split compat fixture failed: $taskCompat"}
                $taskCompatResult
            }
        }
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
