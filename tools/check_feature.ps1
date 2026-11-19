param(
    [string]$Feature,
    [switch]$Changed,
    [string]$Base,
    [string[]]$Paths,
    [switch]$NoCache,
    [switch]$List,
    [switch]$Build,
    [int]$Jobs = 0,
    [string]$Priority = '',
    [string]$Results = ''
)
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskRegistry=Get-Content -LiteralPath (Join-Path $taskProject 'development\features.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if(($Feature -and $Changed) -or (-not $Feature -and -not $Changed)) {throw 'Specify exactly one of -Feature <name> or -Changed.'}
if(-not $Changed -and ($Base -or $Paths)) {throw '-Base and -Paths are only valid with -Changed.'}
$taskHasRoutes=[bool]$taskRegistry.PSObject.Properties['routes']
function Get-TaskBuildKind($taskName,$taskItem) {   # features.json build (split|none); every module declares it. Plan D-1: the integrated build kind is retired.
    if($taskItem.PSObject.Properties.Name -contains 'build') {return [string]$taskItem.build}
    return 'split'
}
$taskBuildKind='none'
if($Changed) {
    $taskSelArgs=@('-B','-X','utf8',(Join-Path $PSScriptRoot 'select_checks.py'),'plan','--changed','--json')
    if($Base) {$taskSelArgs+=@('--base',$Base)}
    if($Paths) {$taskSelArgs+=@('--paths',(($Paths | ForEach-Object {$_ -split ','}) -join ','))}
    if($NoCache) {$taskSelArgs+='--no-cache'}
    $taskSelOut=@(& python @taskSelArgs)
    if($LASTEXITCODE -ne 0) {throw 'select_checks.py plan failed.'}
    $taskPlan=$taskSelOut[-1] | ConvertFrom-Json
    $taskRoutes=@($taskPlan.routes | Where-Object {$_.action -eq 'run'} | ForEach-Object {$_.name})
    foreach($taskMod in @($taskPlan.modules)) {
        $taskKind=Get-TaskBuildKind $taskMod.name $taskRegistry.modules.($taskMod.name)
        if($taskKind -eq 'split') {$taskBuildKind=$taskKind}
    }
    Write-Output ('base={0} changed={1} modules={2}' -f $taskPlan.base,@($taskPlan.changed).Count,((@($taskPlan.modules) | ForEach-Object {$_.name}) -join ','))
    @($taskPlan.routes) | Format-Table name,action,est_s,last_s,reason -AutoSize | Out-String -Width 200 | Write-Output
    foreach($taskWarn in @($taskPlan.warnings)) {Write-Warning $taskWarn}
    if($List) {
        [PSCustomObject]@{mode='changed';base=$taskPlan.base;changed=@($taskPlan.changed);modules=@($taskPlan.modules);routes=@($taskPlan.routes);unowned=@($taskPlan.unowned);warnings=@($taskPlan.warnings);build=$Build.IsPresent;buildKind=$taskBuildKind;installs=$false;output='build/development'} | ConvertTo-Json -Depth 6
        return
    }
    if($taskRoutes.Count -eq 0) {Write-Output 'No checks need to run.'; return}
} else {
if($taskRegistry.modules.PSObject.Properties.Name -notcontains $Feature) {throw "Unknown feature: $Feature"}
$taskEntry=$taskRegistry.modules.$Feature
$taskRoutes=@(@($taskEntry.tests) | Where-Object {$_})
$taskRoutes=@($taskRoutes | Select-Object -Unique)
if($taskRoutes.Count -eq 0) {throw "No checks registered for: $Feature"}
$taskSubsumed=@()
if($taskHasRoutes) {   # routes.<name>.subsumes: drop checks another selected route already covers (-Feature mode never uses the cache)
    foreach($taskR in $taskRoutes) {
        $taskRoute0=$taskRegistry.routes.PSObject.Properties[$taskR]
        if($taskRoute0 -and $taskRoute0.Value.PSObject.Properties['subsumes']) {$taskSubsumed+=@($taskRoute0.Value.subsumes | Where-Object {$_ -in $taskRoutes -and $_ -ne $taskR})}
    }
    $taskRoutes=@($taskRoutes | Where-Object {$_ -notin $taskSubsumed})
}
$taskBuildKind=Get-TaskBuildKind $Feature $taskEntry
if($List) {
    [PSCustomObject]@{feature=$Feature;tests=$taskRoutes;build=$Build.IsPresent;installs=$false;output='build/development';mode='feature';cache=-not $NoCache.IsPresent;subsumed=@($taskSubsumed)} | ConvertTo-Json
    return
}
# 2026-11-16 user (검사 빨리): a cache:true route whose input key equals the key of its last pass is skipped as cached (-NoCache runs all)
$taskSelArgs=@('-B','-X','utf8',(Join-Path $PSScriptRoot 'select_checks.py'),'routes','--names',($taskRoutes -join ','))
if($NoCache) {$taskSelArgs+='--no-cache'}
$taskSelOut=@(& python @taskSelArgs)
if($LASTEXITCODE -ne 0) {throw 'select_checks.py routes failed.'}
$taskPlan=$taskSelOut[-1] | ConvertFrom-Json
foreach($taskC in @($taskPlan.routes | Where-Object {$_.action -eq 'cached'})) {Write-Output ('[cached] {0} (inputs unchanged since its last pass)' -f $taskC.name)}
$taskRoutes=@($taskPlan.routes | Where-Object {$_.action -eq 'run'} | ForEach-Object {$_.name})
if($taskRoutes.Count -eq 0) {Write-Output 'All selected checks are cached.'}
}
# Execution is data-driven: features.json routes.<name>.steps run through tools/run_checks.py (parallel pool, CPU budget, cache record).
foreach($taskRoute in $taskRoutes) {
    $taskData=$null
    if($taskHasRoutes) {$taskData=$taskRegistry.routes.PSObject.Properties[$taskRoute]}
    if(-not $taskData -or -not ($taskData.Value.PSObject.Properties['steps'] -or $taskData.Value.PSObject.Properties['runner'])) {throw "Unknown fixture route: $taskRoute"}
}
if($taskRoutes.Count -eq 0 -and -not $Build) {return}
$taskRunArgs=@('-B','-X','utf8',(Join-Path $PSScriptRoot 'run_checks.py'),'--routes',($taskRoutes -join ','))
if($Jobs -gt 0) {$taskRunArgs+=@('--jobs',"$Jobs")}
if($Priority) {$taskRunArgs+=@('--priority',$Priority)}
if($Results) {$taskRunArgs+=@('--results',$Results)}
$taskPlanFile=$null
if($taskSelOut) {   # the plan carries the cache keys; run_checks records a route only after it passed
    $taskPlanFile=Join-Path ([IO.Path]::GetTempPath()) ('san14_plan_{0}.json' -f $PID)
    [IO.File]::WriteAllText($taskPlanFile,($taskSelOut[-1]),(New-Object System.Text.UTF8Encoding($false)))
    $taskRunArgs+=@('--plan',$taskPlanFile)
}
try {
    if($taskRoutes.Count) {& python @taskRunArgs}
    if($LASTEXITCODE -ne 0) {throw 'Selected checks failed (tools/run_checks.py).'}
} finally {
    if($taskPlanFile -and (Test-Path -LiteralPath $taskPlanFile)) {Remove-Item -LiteralPath $taskPlanFile -Force}
}
if($Build) {
    if($taskBuildKind -eq 'none') {Write-Output 'No development DLL build is needed for the selected checks (or the combat rebuild library is already built by its isolated check route).'}
    else { & (Join-Path $PSScriptRoot 'build_split.ps1') }
}
Write-Output 'Selected checks completed. Installed DLL, Apply package and distribution ZIP were not updated.'
