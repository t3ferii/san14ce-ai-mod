param(
    [string]$Version='',
    [string]$Modules='',
    [switch]$Gui,
    [string]$Note='',
    [switch]$CheckOnly
)
# One command for the owner's local install from a worktree (2026-11-16 user: 검사·설치를 빨리빨리). Replaces the hand-run
# sequence of docs/SESSION_RULES.md section 5 with the same steps and the same guarded installer:
#   build (tools/build_split.ps1 -Both, cached; tools/build_installer.ps1 with -Gui)
#   -> main checkout: discard generated SplitManage.ps1/CURRENT.md, detach at this worktree's HEAD
#   -> copy build/development/{loader_split_diagnostic,loader_split,installer}, newer than the checked-out sources
#   -> tools/install_local.py prepare + install (backup, guarded Apply, verification, rollback on failure)
#   -> tools/generate_handoff.py -> CURRENT.md committed in this worktree -> main detached at that commit.
# Checks are not run here: run them first (docs/SESSION_RULES.md section 4). The worktree must be committed.
# Never pushes. Commits as the publish identity of docs/SESSION_RULES.md.
# -CheckOnly (2026-11-18 user "보호훅부터 3번까지", the san14-handoff skill): the pre-handoff gate without installing -
#   committed worktree, untracked source files, the same consistency routes, check_feature dll-split, build_split -Both
#   (+ installer with -Gui), then a handoff draft. The game may be running; the main checkout is not touched.
$ErrorActionPreference='Stop'
if(-not $CheckOnly -and (-not $Version -or -not $Modules)){throw '-Version and -Modules are required (or run -CheckOnly).'}
$taskProject=Split-Path -Parent $PSScriptRoot
$taskMain=Join-Path (Split-Path -Parent $taskProject) 'AI_TacticsMod'
$taskSame=((Resolve-Path -LiteralPath $taskProject).Path -eq (Resolve-Path -LiteralPath $taskMain).Path)
$taskTimes=New-Object System.Collections.ArrayList
$taskAll=[Diagnostics.Stopwatch]::StartNew()
function Step([string]$name,[scriptblock]$body) {
    $w=[Diagnostics.Stopwatch]::StartNew()
    Write-Output "== $name"
    & $body
    [void]$taskTimes.Add([PSCustomObject]@{step=$name;seconds=[Math]::Round($w.Elapsed.TotalSeconds,1)})
}
function Invoke-Git([string]$repo) {   # git.exe; dash arguments are passed quoted (PowerShell eats a bare --)
    $ErrorActionPreference='Continue'   # PowerShell 5.1: a native stderr line under 2>&1 would otherwise be a terminating error
    $out=@(& git.exe -C $repo @args 2>&1 | ForEach-Object {"$_"})
    if($LASTEXITCODE -ne 0){throw ("git {0} failed in {1}: {2}" -f ($args -join ' '),$repo,($out -join ' '))}
    $out
}
if(-not $CheckOnly -and (Get-Process -Name 'SAN14CE_KO' -ErrorAction SilentlyContinue)){throw 'Close the game first (installs run only with the game closed).'}
$taskDirty=@(Invoke-Git $taskProject status '--porcelain' '--untracked-files=no')
if($taskDirty.Count){throw ("Commit the worktree first:`n"+($taskDirty -join "`n"))}
$taskHead=(Invoke-Git $taskProject rev-parse HEAD | Select-Object -Last 1).Trim()
$taskUntracked=@()
if($CheckOnly) {   # a new source file that builds here but was never committed is lost for the next build elsewhere
    $taskUntracked=@(Invoke-Git $taskProject status '--porcelain' '--untracked-files=all' '--' src tools tests development docs installer distribution | Where-Object {$_ -like '`?`? *'})
    if($taskUntracked.Count){Write-Output ("WARNING untracked files (commit them or delete them):`n"+($taskUntracked -join "`n"))}
}

Step 'consistency routes (features.json always: manifests, catalogs, hook map, addresses, deps)' {
    # 2026-11-16: the local195-199 installs passed dll-split while development/modules/ccz-sprites/module.json was stale; the
    # always routes are not part of dll-split, so they gate every install here (about 10-20 s, parallel).
    # + module-registry: docs/MODULES.md is generated from features.json (gen_module_table.py) and went stale the same way
    $always=(@((Get-Content -LiteralPath (Join-Path $taskProject 'development\features.json') -Raw -Encoding UTF8 | ConvertFrom-Json).always)+'module-registry') -join ','
    & python -B -X utf8 (Join-Path $PSScriptRoot 'run_checks.py') --routes $always
    if($LASTEXITCODE -ne 0){throw 'Consistency routes failed; fix them before installing.'}
}
if($CheckOnly){Step 'dll-split checks (docs/SESSION_RULES.md section 4)' {& (Join-Path $PSScriptRoot 'check_feature.ps1') -Feature dll-split}}
# always built here (about 1 s when cached): the copy below stamps the builds newer than the sources, so a skipped build would
# defeat install_local.py's stale-build check
Step 'build (diagnostic + release, cached)' {& (Join-Path $PSScriptRoot 'build_split.ps1') -Both}
if($Gui){Step 'installer build' {& (Join-Path $PSScriptRoot 'build_installer.ps1')}}
if($CheckOnly) {
    $taskTimes | Format-Table -AutoSize | Out-String | Write-Output
    $subject=(Invoke-Git $taskProject log '-1' '--format=%h %s' | Select-Object -Last 1).Trim()
    $extra=if($taskUntracked.Count){" / 미추적 파일 $($taskUntracked.Count)개 있음"}else{''}
    Write-Output ('check-only: passed in {0:N0}s, nothing installed' -f $taskAll.Elapsed.TotalSeconds)
    Write-Output ("handoff draft: 준비 완료 - {0} (워크트리 {1}). 정합성 경로·dll-split·build_split -Both{2} 통과{3}. 미서명, 실게임 미확인." -f $subject,(Split-Path -Leaf $taskProject),$(if($Gui){'·설치기'}else{''}),$extra)
    return
}
if(-not $taskSame) {
    Step 'main checkout sync' {
        Invoke-Git $taskMain checkout '-q' '--' SplitManage.ps1 development/CURRENT.md | Out-Null
        Invoke-Git $taskMain checkout '-q' '--detach' $taskHead | Out-Null
        $left=@(Invoke-Git $taskMain status '--porcelain' '--untracked-files=no')
        if($left.Count){throw ("Main checkout has local changes:`n"+($left -join "`n"))}
    }
    Step 'copy builds to main' {
        $now=Get-Date
        foreach($d in 'loader_split_diagnostic','loader_split','installer') {
            $src=Join-Path $taskProject "build\development\$d";$dst=Join-Path $taskMain "build\development\$d"
            if(-not (Test-Path -LiteralPath $src)){continue}
            [void][IO.Directory]::CreateDirectory($dst)
            Get-ChildItem -LiteralPath $src -File | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $dst -Force;(Get-Item -LiteralPath (Join-Path $dst $_.Name)).LastWriteTime=$now}
        }
    }
}
$taskPy=@('-B','-X','utf8')
Step 'prepare' {
    $a=$taskPy+@((Join-Path $taskMain 'tools\install_local.py'),'prepare',$Version,'--modules',$Modules)
    if($Gui){$a+='--gui'};if($Note){$a+=@('--note',$Note)}
    Push-Location -LiteralPath $taskMain;try {& python @a;if($LASTEXITCODE -ne 0){throw 'install_local.py prepare failed.'}} finally {Pop-Location}
}
Step 'install (guarded Apply + verification)' {
    Push-Location -LiteralPath $taskMain;try {& python @($taskPy+@((Join-Path $taskMain 'tools\install_local.py'),'install',$Version));if($LASTEXITCODE -ne 0){throw 'install_local.py install failed (the installer rolled back).'}} finally {Pop-Location}
}
Step 'handoff (CURRENT.md)' {
    Push-Location -LiteralPath $taskMain;try {& python @($taskPy+@((Join-Path $taskMain 'tools\generate_handoff.py')));if($LASTEXITCODE -ne 0){throw 'generate_handoff.py failed.'}} finally {Pop-Location}
    if(-not $taskSame) {
        Copy-Item -LiteralPath (Join-Path $taskMain 'development\CURRENT.md') -Destination (Join-Path $taskProject 'development\CURRENT.md') -Force
        Invoke-Git $taskProject add development/CURRENT.md | Out-Null
        $msg="docs: CURRENT.md after the $Version install`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`n"
        $f=Join-Path ([IO.Path]::GetTempPath()) ('san14_install_msg_{0}.txt' -f $PID)
        [IO.File]::WriteAllText($f,$msg,(New-Object Text.UTF8Encoding($false)))
        try {Invoke-Git $taskProject '-c' user.name=t3ferii '-c' user.email=188655834+t3ferii@users.noreply.github.com commit '-q' '-F' $f | Out-Null} finally {Remove-Item -LiteralPath $f -Force}
        $newHead=(Invoke-Git $taskProject rev-parse HEAD | Select-Object -Last 1).Trim()
        Invoke-Git $taskMain checkout '-q' '--' SplitManage.ps1 development/CURRENT.md | Out-Null
        Invoke-Git $taskMain checkout '-q' '--detach' $newHead | Out-Null
        Write-Output "main detached at $newHead"
    }
}
$taskTimes | Format-Table -AutoSize | Out-String | Write-Output
Write-Output ('local install {0}: done in {1:N0}s (unsigned; in-game check by the user)' -f $Version,$taskAll.Elapsed.TotalSeconds)
