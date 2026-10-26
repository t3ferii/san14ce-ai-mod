$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskTokens=$null;$taskErrors=$null
$taskSource=[IO.File]::ReadAllText((Join-Path $taskProject 'distribution/Manage.ps1'),[Text.Encoding]::UTF8)
$taskAst=[Management.Automation.Language.Parser]::ParseInput($taskSource,[ref]$taskTokens,[ref]$taskErrors)
if($taskErrors.Count){throw 'Manager parse failed'}
foreach($taskName in @('Get-Digest','Write-Atomic')) {
    $taskFunction=@($taskAst.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $taskName},$true))
    if($taskFunction.Count -ne 1){throw 'Missing atomic helper'}
    . ([ScriptBlock]::Create($taskFunction[0].Extent.Text))
}
$taskFolder=Join-Path $taskProject ('build/development/installer/atomic_'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($taskFolder)
$taskTarget=Join-Path $taskFolder 'unchanged_combat.dll'
$taskOld=[byte[]](1,2,3,4);$taskNew=[byte[]](4,3,2,1)
[IO.File]::WriteAllBytes($taskTarget,$taskOld)
$taskStamp=[IO.File]::GetLastWriteTimeUtc($taskTarget)
$taskStream=[IO.File]::Open($taskTarget,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try {
    Write-Atomic $taskTarget $taskOld
    if([IO.File]::GetLastWriteTimeUtc($taskTarget) -ne $taskStamp){throw 'Unchanged combat was replaced'}
    if([IO.File]::Exists($taskTarget+'.aitactics.tmp')){throw 'Unchanged DLL created a temp file'}
    $taskRejected=$false
    try {Write-Atomic $taskTarget $taskNew} catch {$taskRejected=$_.Exception.Message.Contains($taskTarget)}
    if(-not $taskRejected){throw 'Locked changed file must reject with exact path'}
    if([IO.File]::Exists($taskTarget+'.aitactics.tmp')){throw 'Failed replacement leaked temp file'}
} finally {$taskStream.Dispose()}
if([Convert]::ToBase64String([IO.File]::ReadAllBytes($taskTarget)) -ne [Convert]::ToBase64String($taskOld)){throw 'Locked target changed'}
Write-Atomic $taskTarget $taskNew
if([Convert]::ToBase64String([IO.File]::ReadAllBytes($taskTarget)) -ne [Convert]::ToBase64String($taskNew)){throw 'Unlocked replacement failed'}
$taskNewFile=Join-Path $taskFolder 'new.dll';Write-Atomic $taskNewFile $taskNew
if([Convert]::ToBase64String([IO.File]::ReadAllBytes($taskNewFile)) -ne [Convert]::ToBase64String($taskNew)){throw 'New file failed'}
'{"status":"installer_atomic_fixture_verified","checks":7,"game_files_changed":false}'
