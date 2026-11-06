param(
    [Parameter(Mandatory=$true)]
    [string]$Feature,
    [switch]$List,
    [switch]$Build
)
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskRegistry=Get-Content -LiteralPath (Join-Path $taskProject 'development\features.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if($taskRegistry.modules.PSObject.Properties.Name -notcontains $Feature) {throw "Unknown feature: $Feature"}
$taskEntry=$taskRegistry.modules.$Feature
$taskRoutes=@($taskEntry.tests)
if($Feature -eq 'terrain-edit'){$taskRoutes+=@('terrain-foam','terrain-diag')}
if($taskRoutes.Count -eq 0) {throw "No checks registered for: $Feature"}
if($List) {
    [PSCustomObject]@{feature=$Feature;tests=$taskRoutes;build=$Build.IsPresent;installs=$false;output='build/development'} | ConvertTo-Json
    return
}
foreach($taskRoute in $taskRoutes) {
    switch($taskRoute) {
        'ai-trace' { & (Join-Path $PSScriptRoot 'build_ai_trace.ps1') }
        'combat-rebuild' { & (Join-Path $PSScriptRoot 'build_combat_rebuild.ps1') }
        'region-officers' { & (Join-Path $PSScriptRoot 'build_region_officer_fixture.ps1') }
        'district-intercept' {
            & python -B -X utf8 (Join-Path $taskProject 'tests\verify_district_intercept.py')
            if($LASTEXITCODE -ne 0){throw 'District intercept verification failed.'}
        }
        'layout-registry' {   # 2026-11-05 Layout hook registry vs the original EXE, blocklist, whitelist rows and the converted modules (needs a prior build for traits9_payloads.h)
            & python -B -X utf8 (Join-Path $taskProject 'tests/verify_layout_registry.py')
            if($LASTEXITCODE -ne 0){throw 'Layout registry verification failed.'}
        }
        'address-table' {   # 2026-11-05 user (patch resilience): address table ids resolve identically in every known build, no RVA literals in feature sources, module guards per build
            & python -B -X utf8 (Join-Path $taskProject 'tools\check_addresses.py')
            if($LASTEXITCODE -ne 0){throw 'Address table checks failed.'}
        }
        'installer-gui' {
            & python -B -X utf8 (Join-Path $taskProject 'tests\verify_installer_gui.py')
            if($LASTEXITCODE -ne 0){throw 'Installer GUI verification failed.'}
        }
        'installer-atomic' { & (Join-Path $taskProject 'tests\installer_atomic_fixture.ps1') }
        'installer-layout' { & (Join-Path $taskProject 'tests\verify_installer_layout.ps1') }
        'layout-restore' { & (Join-Path $taskProject 'tests\verify_layout_restore.ps1') }
        'terrain-profile' { & (Join-Path $taskProject 'tests\terrain_call_profile_build.ps1') }
        'terrain-edit' { & (Join-Path $taskProject 'tests\build_terrain_edit_fixture.ps1') }
        'terrain-foam' { & (Join-Path $taskProject 'tests\terrain_foam_build.ps1') }
        'terrain-diag' { & python -B -X utf8 (Join-Path $taskProject 'tests\verify_terrain_diag.py') }
        'terrain-edit-ui' { & (Join-Path $taskProject 'tests\terrain_edit_build.ps1') }
        'terrain-render' { & (Join-Path $taskProject 'tests\terrain_render_build.ps1') }
        'water-diag' { & (Join-Path $taskProject 'tests\water_diag_build.ps1') }
        'terrain-trees' { & (Join-Path $taskProject 'tests\terrain_trees_build.ps1') }
        'advisor-briefing' { & (Join-Path $PSScriptRoot 'build_advisor_briefing_fixture.ps1') }
        'shared-overlay' { & (Join-Path $PSScriptRoot 'build_shared_overlay_fixture.ps1') }
        'diplomacy-intel' { & (Join-Path $PSScriptRoot 'build_diplomacy_intel_fixture.ps1') }
        'building-destruction' { & (Join-Path $PSScriptRoot 'build_building_destruction_fixture.ps1') }
        'damage-preview' { & (Join-Path $PSScriptRoot 'build_damage_preview_fixture.ps1') }
        'area-index' { & (Join-Path $PSScriptRoot 'build_area_index_fixture.ps1') }
        'path-count' { & (Join-Path $PSScriptRoot 'build_path_count_fixture.ps1') }
        'path-fast' { & (Join-Path $PSScriptRoot 'build_path_fast_fixture.ps1') }
        'appear-batch' { & (Join-Path $PSScriptRoot 'build_appear_batch_fixture.ps1') }
        'face-keep' { & (Join-Path $PSScriptRoot 'build_face_keep_fixture.ps1') }
        'trait-hover' { & (Join-Path $PSScriptRoot 'build_trait_hover_fixture.ps1') -WithLayout }   # 2026-11-06 pure-logic fixture first, then Layout fixtures + modpack static check (those need the S3 registry rows)
        'battle-deaths' { & (Join-Path $PSScriptRoot 'build_battle_deaths_fixture.ps1') }   # 2026-11-06 relay executed in an RWX page + guard/site/span table checks (docs/BATTLE_DEATHS_PLAN.md 4-1)
        'split-module' {
            & (Join-Path $PSScriptRoot 'build_split.ps1') -Fixture
            # 사용자 결정 2026-11-03: 통합 인사 제거. Keep independent sortie call inventory.
            & python -B -X utf8 (Join-Path $taskProject 'tests\verify_callsite_compat.py')
            if($LASTEXITCODE -ne 0){throw 'Call-site byte inventory failed.'}
        }
        'ccz-sprites' {
            & (Join-Path $taskProject 'tests\ccz_battle_build.ps1')
            & (Join-Path $taskProject 'tests\ccz_settings_build.ps1')
            & (Join-Path $taskProject 'tests\ccz_native_editor_build.ps1')
            & python -B -X utf8 (Join-Path $taskProject 'tests\verify_ccz_sprites.py')
            if($LASTEXITCODE -ne 0){throw 'CCZ sprite verification failed.'}
            & python -B -X utf8 (Join-Path $taskProject 'tests\verify_ccz_settings.py')
            if($LASTEXITCODE -ne 0){throw 'CCZ settings verification failed.'}
        }        'traits9-save' {
            & python -X utf8 (Join-Path $taskProject 'tests\verify_traits9_save.py')
            if($LASTEXITCODE -ne 0){throw 'Traits9 save-contract fixture failed.'}
        }
        'target-retreat' { & (Join-Path $PSScriptRoot 'build_target_retreat_fixture.ps1') }
        'log' { & (Join-Path $PSScriptRoot 'build_log_fixture.ps1') }
        'progress-native' { & (Join-Path $PSScriptRoot 'build_progress_fixture.ps1') }
        'name-lookup' { & (Join-Path $PSScriptRoot 'build_name_lookup_fixture.ps1') }
        'progress-rollback' {
            & python -X utf8 (Join-Path $taskProject 'tests\verify_progress_rollback.py')
            if($LASTEXITCODE -ne 0){throw 'Progress rollback fixture failed.'}
        }
        'progress' { & (Join-Path $taskProject 'tests\progress_measure_fixture.ps1') }
        'progress-samples' {
            & python -X utf8 (Join-Path $taskProject 'tests\verify_progress_samples.py')
            if($LASTEXITCODE -ne 0){throw 'Sampled CPU parser fixture failed.'}
        }
        'awards' { & (Join-Path $PSScriptRoot 'build_personnel_fixture.ps1') -Awards }
        'recruitment' { & (Join-Path $PSScriptRoot 'build_personnel_fixture.ps1') -RecruitmentUI }
        'merit' { & (Join-Path $PSScriptRoot 'build_personnel_fixture.ps1') -Merit }
        'personnel' { & (Join-Path $PSScriptRoot 'build_personnel_fixture.ps1') -Bindings }
        'sortie' {
            & (Join-Path $PSScriptRoot 'build_native.ps1') -Fixture -Sortie
            Push-Location -LiteralPath (Join-Path $taskProject 'build')
            try {$taskResult=& '.\sortie_fixture.exe'} finally {Pop-Location}
            if($LASTEXITCODE -ne 0){throw 'Sortie fixture failed.'}
            [IO.File]::WriteAllText((Join-Path $taskProject 'build\sortie_fixture.json'),($taskResult | Select-Object -Last 1),(New-Object System.Text.UTF8Encoding($false)))
            $taskResult
        }
        'native' {
            & (Join-Path $PSScriptRoot 'build_native.ps1') -Fixture
            Push-Location -LiteralPath (Join-Path $taskProject 'build')
            try {$taskResult=& '.\native_fixture.exe'} finally {Pop-Location}
            if($LASTEXITCODE -ne 0){throw 'Native fixture failed.'}
            [IO.File]::WriteAllText((Join-Path $taskProject "build\native_fixture.json"),($taskResult | Select-Object -Last 1),(New-Object System.Text.UTF8Encoding($false)))
            $taskResult
        }
        default { throw "Unknown fixture route: $taskRoute" }
    }
}
if($Build) {
    if($Feature -eq 'combat-rebuild') {Write-Output 'Combat rebuild library already built by its isolated check route.'}
    elseif($Feature -in @('terrain-edit','dll-split','modpack-v16','traits9-captive','name-lookup','region-officers','district-intercept','damage-preview','building-destruction','appear-batch','face-keep','trait-hover','battle-deaths','diplomacy-intel','advisor-briefing','advisor_briefing','drag_learn','layout')){ & (Join-Path $PSScriptRoot 'build_split.ps1') }   # 2026-11-05 user exception: layout = SAN14_Layout.dll (fifth DLL), built by build_split.ps1
    else { & (Join-Path $PSScriptRoot 'build_native.ps1') -InputLoader -RecruitmentUICandidate -Development }
}
Write-Output 'Selected checks completed. Installed DLL, Apply package and distribution ZIP were not updated.'
