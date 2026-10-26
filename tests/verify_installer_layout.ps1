param([string]$Manager, [string]$Report)
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
if(-not $Manager){$Manager=Join-Path $taskProject 'distribution/SplitManage.ps1'}
if(-not $Report){$Report=Join-Path $taskProject 'build/installer_layout_fixture.json'}
. $Manager
$taskPatch=Get-Content (Join-Path $taskProject 'build/input_package/traits9_layout.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$taskSnapshot=Join-Path $taskProject 'build/installer_speed/layout.snapshot.rdb'
if(-not [IO.File]::Exists($taskSnapshot)){$taskSnapshot=Join-Path (Split-Path -Parent $taskProject) '0002/ScreenLayout.rdb'}
$taskRaw=[IO.File]::ReadAllBytes($taskSnapshot)
function Digest-Bytes([byte[]]$Data) {
    $taskAlgorithm=[Security.Cryptography.SHA256]::Create()
    try{return [BitConverter]::ToString($taskAlgorithm.ComputeHash($Data))}finally{$taskAlgorithm.Dispose()}
}
$taskResults=[ordered]@{}
$taskClock=[Diagnostics.Stopwatch]::StartNew()
$taskIndex=Read-LayoutIndex $taskRaw
$taskResults.parse_ms=$taskClock.Elapsed.TotalMilliseconds
$taskClock.Restart()
# Restore all matching fixture entries to the provided original bytes.
[void](Reset-TraitLayout $taskIndex $taskPatch.records 30)
[void](Reset-TraitLayout $taskIndex $taskPatch.records 0)
$taskOriginal=Build-LayoutIndex $taskIndex
$taskResults.reset_ms=$taskClock.Elapsed.TotalMilliseconds
$taskResults.original=Digest-Bytes $taskOriginal
$taskClock.Restart()
$taskSet=Set-TraitLayout $taskIndex $taskPatch.records 29
if($taskSet.Applied.Count -ne $taskPatch.records.Count){throw 'Expected all fixture records to apply.'}
$taskPatched=Build-LayoutIndex $taskIndex
$taskResults.set_ms=$taskClock.Elapsed.TotalMilliseconds
$taskResults.patched=Digest-Bytes $taskPatched
$taskBanks=Get-LayoutBanks $taskIndex
if(-not $taskBanks.Contains([long]29)){throw 'Applied bank is missing.'}
$taskClock.Restart()
[void](Reset-TraitLayout $taskIndex $taskPatch.records 29)
if((Digest-Bytes (Build-LayoutIndex $taskIndex)) -cne $taskResults.original){throw 'Restore must reproduce original index bytes.'}
$taskResults.restore_ms=$taskClock.Elapsed.TotalMilliseconds
# A changed target entry, missing KTID and duplicate KTID must all be skipped.
$taskIndex=Read-LayoutIndex $taskOriginal
$taskKtid=[uint32]$taskPatch.records[0].ktid
$taskPosition=-1
for($i=0;$i -lt $taskIndex.Entries.Count;$i++){
    if([BitConverter]::ToUInt32($taskIndex.Entries[$i],36) -eq $taskKtid){$taskPosition=$i;break}
}
$taskForeign=[byte[]]$taskIndex.Entries[$taskPosition].Clone();$taskForeign[24]=$taskForeign[24] -bxor 1
$taskIndex.Entries[$taskPosition]=$taskForeign
$taskSet=Set-TraitLayout $taskIndex @($taskPatch.records[0]) 29
if($taskSet.Applied.Count -ne 0 -or (Digest-Bytes $taskIndex.Entries[$taskPosition]) -cne (Digest-Bytes $taskForeign)){throw 'Foreign target entry was modified.'}
$taskMissing=[pscustomobject]@{ktid=[uint32]::MaxValue}
$taskSet=Set-TraitLayout $taskIndex @($taskMissing) 29
if($taskSet.Skipped.Count -ne 1){throw 'Missing KTID must be skipped.'}
# Build a duplicate index, then parse again so both implementations see it.
$taskIndex.Entries.Add([Convert]::FromBase64String($taskPatch.records[0].original_entry))
$taskIndex=Read-LayoutIndex (Build-LayoutIndex $taskIndex)
$taskSet=Set-TraitLayout $taskIndex @($taskPatch.records[0]) 29
if($taskSet.Applied.Count -ne 0 -or $taskSet.Skipped.Count -ne 1){throw 'Duplicate KTID must be skipped.'}
$taskResults.foreign_duplicate_missing=Digest-Bytes (Build-LayoutIndex $taskIndex)
$taskResults.checks=7
$taskResults | ConvertTo-Json | Set-Content $Report -Encoding UTF8
$taskResults | ConvertTo-Json
