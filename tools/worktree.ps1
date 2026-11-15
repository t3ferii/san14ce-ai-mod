# Local git worktrees for parallel work (user 2026-11-06: "로컬 깃 기능으로 작업을 병렬화").
# A worktree lives next to AI_TacticsMod inside the game folder (so '..' is still the game root) as
# AI_TacticsMod_wt_<name>. Heavy untracked folders are junctions to the main checkout (read them, do not
# write them); build/ and dist/ are the worktree's own, with build/input_package junctioned for reference.
# Never install, publish or run the game from a worktree: merge into the main checkout first.
#   powershell -File tools/worktree.ps1 -New <name> [-Base main]
#   powershell -File tools/worktree.ps1 -Remove <name>
#   powershell -File tools/worktree.ps1 -List
param([string]$New,[string]$Remove,[switch]$List,[string]$Base='main')
$ErrorActionPreference='Stop'
$taskMain=Split-Path -Parent $PSScriptRoot
$taskGame=Split-Path -Parent $taskMain
$taskShared=@('assets','data','legacy','research','backup','updates','readme_screenshot')
# Read-only reference inputs under build/ (installed package, unpacked EXEs for static guards).
$taskBuildRefs=@('input_package','ui_native_analysis')
$taskRootFiles=@('기능설정.ini','diagnostics.ini','progress_native.ini','release_features.json')
function Path-Of([string]$n){ if($n -notmatch '^[a-z0-9][a-z0-9-]{0,30}$'){throw 'name: lowercase letters, digits, dash'}; Join-Path $taskGame ('AI_TacticsMod_wt_'+$n) }
if($List){ git -C $taskMain worktree list; return }
if($New){
    $taskPath=Path-Of $New
    if(Test-Path -LiteralPath $taskPath){throw "exists: $taskPath"}
    git -C $taskMain worktree add -b "wt/$New" $taskPath $Base
    if($LASTEXITCODE -ne 0){throw 'git worktree add failed'}
    foreach($d in $taskShared){
        $src=Join-Path $taskMain $d
        if(Test-Path -LiteralPath $src){ New-Item -ItemType Junction -Path (Join-Path $taskPath $d) -Target $src | Out-Null }
    }
    foreach($f in $taskRootFiles){ $src=Join-Path $taskMain $f; if(Test-Path -LiteralPath $src){Copy-Item -LiteralPath $src -Destination $taskPath} }
    $build=Join-Path $taskPath 'build'; [void][IO.Directory]::CreateDirectory($build); [void][IO.Directory]::CreateDirectory((Join-Path $taskPath 'dist'))
    foreach($d in $taskBuildRefs){ $src=Join-Path (Join-Path $taskMain 'build') $d; if(Test-Path -LiteralPath $src){ New-Item -ItemType Junction -Path (Join-Path $build $d) -Target $src | Out-Null } }
    git -C $taskPath config user.name t3ferii; git -C $taskPath config user.email 188655834+t3ferii@users.noreply.github.com
    Write-Output ("worktree ready: {0} (branch wt/{1})" -f $taskPath,$New)
    return
}
if($Remove){
    $taskPath=Path-Of $Remove
    if(Test-Path -LiteralPath $taskPath){
        # Remove every junction first, non-recursively, and refuse to continue while any remains: a forced
        # worktree removal must never be able to reach the shared folders behind them.
        foreach($j in Get-ChildItem -LiteralPath $taskPath -Recurse -Force -Attributes ReparsePoint -ErrorAction SilentlyContinue){ [IO.Directory]::Delete($j.FullName,$false) }
        if(Get-ChildItem -LiteralPath $taskPath -Recurse -Force -Attributes ReparsePoint -ErrorAction SilentlyContinue){throw 'junction still present; not removing'}
        git -C $taskMain worktree remove --force $taskPath
        # git leaves untracked content (build/, dist/, copied ini files) behind: delete the leftover folder too. Safe here: every junction
        # was unlinked above and the re-check proved none remains, so nothing can reach a shared folder.
        if(Test-Path -LiteralPath $taskPath){
            if(Get-ChildItem -LiteralPath $taskPath -Recurse -Force -Attributes ReparsePoint -ErrorAction SilentlyContinue){throw 'junction still present; not deleting the leftover folder'}
            Remove-Item -LiteralPath $taskPath -Recurse -Force
        }
    }
    git -C $taskMain worktree prune
    Write-Output "worktree removed: $taskPath (branch wt/$Remove kept until merged/deleted)"
    return
}
Get-Help $PSCommandPath
