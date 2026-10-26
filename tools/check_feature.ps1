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
if($taskRoutes.Count -eq 0) {throw "No checks registered for: $Feature"}
if($List) {
    [PSCustomObject]@{feature=$Feature;tests=$taskRoutes;build=$Build.IsPresent;installs=$false;output='build/development'} | ConvertTo-Json
    return
}
foreach($taskRoute in $taskRoutes) {
    switch($taskRoute) {
        'advisor-briefing' { & (Join-Path $PSScriptRoot 'build_advisor_briefing_fixture.ps1') }
        'diplomacy-intel' { & (Join-Path $PSScriptRoot 'build_diplomacy_intel_fixture.ps1') }
        'building-destruction' { & (Join-Path $PSScriptRoot 'build_building_destruction_fixture.ps1') }
        'damage-preview' { & (Join-Path $PSScriptRoot 'build_damage_preview_fixture.ps1') }
        'area-index' { & (Join-Path $PSScriptRoot 'build_area_index_fixture.ps1') }
        'path-count' { & (Join-Path $PSScriptRoot 'build_path_count_fixture.ps1') }
        'path-fast' { & (Join-Path $PSScriptRoot 'build_path_fast_fixture.ps1') }
        'appear-batch' { & (Join-Path $PSScriptRoot 'build_appear_batch_fixture.ps1') }
        'face-keep' { & (Join-Path $PSScriptRoot 'build_face_keep_fixture.ps1') }
        'district-intercept' {
            & python -B -X utf8 (Join-Path $taskProject 'tests\verify_district_intercept.py')
            if($LASTEXITCODE -ne 0){throw 'District interception native checks failed.'}
        }
        'region-officers' { & (Join-Path $PSScriptRoot 'build_region_officer_fixture.ps1') }
        'installer-atomic' { & (Join-Path $taskProject 'tests\installer_atomic_fixture.ps1') }
        'installer-layout' { & (Join-Path $taskProject 'tests\verify_installer_layout.ps1') }
        'layout-restore' { & (Join-Path $taskProject 'tests\verify_layout_restore.ps1') }
        'installer-gui' { & (Join-Path $PSScriptRoot 'build_installer.ps1') }
        'ai-trace' { & (Join-Path $PSScriptRoot 'build_ai_trace.ps1') }
        'combat-rebuild' { & (Join-Path $PSScriptRoot 'build_combat_rebuild.ps1') }
        'unified-personnel' { & (Join-Path $PSScriptRoot 'build_unified_fixture.ps1') }
        'split-module' { & (Join-Path $PSScriptRoot 'build_split.ps1') -Fixture }
        'traits9-save' {
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
    elseif($Feature -in @('dll-split','modpack-v16','traits9-captive','unified-personnel','name-lookup','region-officers','district-intercept','damage-preview','building-destruction','appear-batch','face-keep','diplomacy-intel','advisor-briefing')){ & (Join-Path $PSScriptRoot 'build_split.ps1') }
    else { & (Join-Path $PSScriptRoot 'build_native.ps1') -InputLoader -RecruitmentUICandidate -Development }
    if($Feature -eq 'unified-personnel') {
        & python -X utf8 (Join-Path $taskProject 'tests\verify_unified_engine_ui.py')
        if($LASTEXITCODE -ne 0){throw 'Native personnel UI boundary verification failed.'}
    }
}
Write-Output 'Selected checks completed. Installed DLL, Apply package and distribution ZIP were not updated.'
