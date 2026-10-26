# Restore must return the traits9/speed-button ScreenLayout entries to the original index even
# when the install record has no (or a stale) layout entry. Works on copies in build/ only.
param([string]$Report)
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskRoot=Split-Path -Parent $taskProject
if(-not $Report){$Report=Join-Path $taskProject 'build\layout_restore_fixture.json'}
function Get-Digest([string]$Path){if(-not [IO.File]::Exists($Path)){return $null};$a=[Security.Cryptography.SHA256]::Create();try{return ([BitConverter]::ToString($a.ComputeHash([IO.File]::ReadAllBytes($Path)))).Replace('-','').ToLowerInvariant()}finally{$a.Dispose()}}
function Get-BytesDigest([byte[]]$Bytes){$a=[Security.Cryptography.SHA256]::Create();try{return ([BitConverter]::ToString($a.ComputeHash($Bytes))).Replace('-','').ToLowerInvariant()}finally{$a.Dispose()}}
$checks=0
function Check([bool]$Value,[string]$Label){if(-not $Value){throw ('FAIL: '+$Label)};$script:checks++}
$taskGame=Join-Path $taskProject ('build\layout_restore_'+[Guid]::NewGuid().ToString('N').Substring(0,8))
. (Join-Path $taskProject 'distribution\SplitManage.ps1')
$patch=Get-Content (Join-Path $taskProject 'build\input_package\traits9_layout.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$patch | Add-Member -NotePropertyName Bytes -NotePropertyValue ([IO.File]::ReadAllBytes((Join-Path $taskProject 'build\input_package\traits9_layout.bin')))
$patch | Add-Member -NotePropertyName Hash -NotePropertyValue $patch.bank_sha256
function Write-Plan($Plan){foreach($p in $Plan.Writes.Keys){[IO.File]::WriteAllBytes($p,$Plan.Writes[$p])};foreach($p in $Plan.Deletes){[IO.File]::Delete($p)}}
try {
    [void][IO.Directory]::CreateDirectory((Join-Path $taskGame '0002'))
    # Original index: the installed index with every patch record put back to its recorded original entry.
    $live=Read-LayoutIndex ([IO.File]::ReadAllBytes((Join-Path $taskRoot '0002\ScreenLayout.rdb')))
    foreach($r in $patch.records){$k=[uint32]$r.ktid;Check ($live.ByKtid.ContainsKey($k) -and $live.ByKtid[$k] -ge 0) 'patch key exists once';$live.Entries[$live.ByKtid[$k]]=[Convert]::FromBase64String($r.original_entry)}
    $original=Build-LayoutIndex $live;$originalHash=Get-BytesDigest $original
    $index=Join-Path $taskGame '0002\ScreenLayout.rdb'
    $scenarios=@(@{Name='no layout record';State=[pscustomobject]@{installed=$true}},
                 @{Name='stale layout bank';State=[pscustomobject]@{installed=$true;layout=[pscustomobject]@{bank=27;bank_sha256='0'*64}}},
                 @{Name='left over after restore';State=[pscustomobject]@{installed=$false}})
    foreach($s in $scenarios) {
        [IO.File]::WriteAllBytes($index,$original)
        $apply=Get-TraitLayoutPlan $null $patch $true;Write-Plan $apply
        $bank=Join-Path $taskGame ('0002\ScreenLayout.rdb.bin'+$apply.State.bank)
        Check ((Get-Digest $index) -ne $originalHash -and [IO.File]::Exists($bank) -and $apply.State.applied.Count -eq $patch.records.Count) ($s.Name+': applied all records')
        $restore=Get-TraitLayoutPlan $s.State $patch $false;Write-Plan $restore
        Check ((Get-Digest $index) -eq $originalHash) ($s.Name+': index returns to the original bytes')
        Check (-not [IO.File]::Exists($bank)) ($s.Name+': our bank file is removed')
        $again=Get-TraitLayoutPlan $s.State $patch $false
        Check ($again.Writes.Count -eq 0 -and $again.Deletes.Count -eq 0) ($s.Name+': second restore changes nothing')
    }
    # A foreign bank with our KTIDs but other locators (another layout mod) is never touched.
    [IO.File]::WriteAllBytes($index,$original)
    $foreign=Read-LayoutIndex $original;$r=$patch.records[0];$k=[uint32]$r.ktid
    $foreign.Entries[$foreign.ByKtid[$k]]=New-LayoutEntry ([Convert]::FromBase64String($r.original_entry)) 16 32 29 1
    $foreignBytes=Build-LayoutIndex $foreign;[IO.File]::WriteAllBytes($index,$foreignBytes)
    [IO.File]::WriteAllBytes((Join-Path $taskGame '0002\ScreenLayout.rdb.bin29'),[byte[]](1,2,3))
    $plan=Get-TraitLayoutPlan ([pscustomobject]@{installed=$false}) $patch $false;Write-Plan $plan
    Check ((Get-Digest $index) -eq (Get-BytesDigest $foreignBytes) -and [IO.File]::Exists((Join-Path $taskGame '0002\ScreenLayout.rdb.bin29'))) 'foreign layout entries and bank stay untouched'
    $result=[ordered]@{status='layout_restore_fixture_passed';checks=$checks;scenarios=$scenarios.Count;records=$patch.records.Count;actual_game_files_changed=$false}
    $result | ConvertTo-Json | Set-Content $Report -Encoding UTF8
    $result | ConvertTo-Json -Compress
} finally {
    if($taskGame -like (Join-Path $taskProject 'build\layout_restore_*')){Remove-Item -LiteralPath $taskGame -Recurse -Force -ErrorAction SilentlyContinue}
}
