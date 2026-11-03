# user 2026-11-03: CCZ OFF/Restore must undo only CCZ resource placement.
# Operates exclusively on newly created fixture files under build/.
param([string]$Payload)
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
if(-not $Payload){$Payload=Join-Path $taskProject 'build\input_package'}
$taskGame=Join-Path $taskProject ('build\ccz_layout_fixture_'+[Guid]::NewGuid().ToString('N'))
function Get-Digest([string]$Path){if(-not [IO.File]::Exists($Path)){return $null};$h=[Security.Cryptography.SHA256]::Create();try{return ([BitConverter]::ToString($h.ComputeHash([IO.File]::ReadAllBytes($Path)))).Replace('-','').ToLowerInvariant()}finally{$h.Dispose()}}
function Check($Condition,$Name){if(-not $Condition){throw ('FAIL '+$Name)};$script:checks++}
function Same($A,$B){return [Convert]::ToBase64String($A) -ceq [Convert]::ToBase64String($B)}
function Commit($Plan){foreach($f in $Plan.Writes.Keys){[IO.File]::WriteAllBytes($f,$Plan.Writes[$f])};foreach($f in $Plan.Deletes){[IO.File]::Delete($f)}}
. (Join-Path $taskProject 'distribution\SplitManage.ps1')
$patch=Get-Content (Join-Path $Payload 'traits9_layout.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$patch | Add-Member -NotePropertyName Bytes -NotePropertyValue ([IO.File]::ReadAllBytes((Join-Path $Payload 'traits9_layout.bin')))
$patch | Add-Member -NotePropertyName Hash -NotePropertyValue $patch.bank_sha256
$keys=@([uint32]0x3DA1641D,[uint32]0x757F1A41,[uint32]0x5C046EA3)
$speedKeys=@([uint32]0x301526D7,[uint32]0xFEA5F28CL,[uint32]0x40F993CE,[uint32]0x454A1BCC)
$on=@{'groups.convenience'=1;'ccz.ccz_sprites'=1}
$off=@{'groups.convenience'=1;'ccz.ccz_sprites'=0}
$checks=0
try {
    [void][IO.Directory]::CreateDirectory((Join-Path $taskGame '0002'))
    $path=Join-Path $taskGame '0002\ScreenLayout.rdb'
    $base=Read-LayoutIndex ([IO.File]::ReadAllBytes((Join-Path (Split-Path -Parent $taskProject) '0002\ScreenLayout.rdb')))
    foreach($r in $patch.records){$base.Entries[$base.ByKtid[[uint32]$r.ktid]]=[Convert]::FromBase64String($r.original_entry)}
    $original=Build-LayoutIndex $base
    Check ((Get-LayoutLocator $base.Entries[$base.ByKtid[[uint32]0x5C046EA3]]) -match ":0$") 'army original has no mod-bank locator'
    $record=@($patch.records | Where-Object {$_.ktid -eq [uint32]0x5C046EA3})[0]
    Check ([Text.Encoding]::ASCII.GetString([Convert]::FromBase64String($record.original_entry)) -match '8d5cc@7d5') 'independent backup army original'
    [IO.File]::WriteAllBytes($path,$original)
    $state=$null
    foreach($features in @($off,$on,$off,$on,@{'groups.convenience'=0;'ccz.ccz_sprites'=1},$on,@{'groups.convenience'=1;'ccz.ccz_sprites'=1;'game_flow.speed_buttons'=0},$on,@{'groups.convenience'=1;'ccz.ccz_sprites'=0;'game_flow.speed_buttons'=0})) {
        $before=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
        $plan=Get-TraitLayoutPlan $state $patch $true $features;Commit $plan
        $state=[pscustomobject]@{installed=$true;layout=[pscustomobject]$plan.State}
        $active=$features['groups.convenience'] -and $features['ccz.ccz_sprites']
        $speed=$features['groups.convenience'] -and (-not $features.Contains('game_flow.speed_buttons') -or $features['game_flow.speed_buttons'])
        $count=14-$(if($active){0}else{3})-$(if($speed){0}else{4})
        Check ($plan.State.applied.Count -eq $count) 'selected layout count follows effective CCZ'
        $after=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
        foreach($r in $patch.records){
            $entry=$after.Entries[$after.ByKtid[[uint32]$r.ktid]]
            if((-not $active -and $keys -contains [uint32]$r.ktid) -or (-not $speed -and $speedKeys -contains [uint32]$r.ktid)){Check (Same $entry ([Convert]::FromBase64String($r.original_entry))) 'CCZ OFF exact original entry'}
            else{Check ((Get-LayoutLocator $entry) -eq (Get-LayoutTarget $r $plan.State.bank)) 'enabled/non-CCZ entry remains applied'}
        }
        $unchanged=$true
        foreach($k in $base.ByKtid.Keys){if(@($patch.records.ktid) -notcontains $k -and -not (Same $before.Entries[$before.ByKtid[$k]] $after.Entries[$after.ByKtid[$k]])){$unchanged=$false;break}}
        Check $unchanged 'every unrelated record preserved'
    }
    $restore=Get-TraitLayoutPlan $state $patch $false;Commit $restore
    Check (Same ([IO.File]::ReadAllBytes($path)) $original) 'uninstall after ON restores full original index'
    # Old uninstall left army pointing to #1c even without a layout state record.
    $stale=Read-LayoutIndex $original
    $stale.Entries[$stale.ByKtid[[uint32]$record.ktid]]=New-LayoutEntry ([Convert]::FromBase64String($record.original_entry)) $record.offset $record.size 28 $record.dsz
    [IO.File]::WriteAllBytes((Get-LayoutBankPath 28),$patch.Bytes)
    [IO.File]::WriteAllBytes($path,(Build-LayoutIndex $stale))
    $plan=Get-TraitLayoutPlan ([pscustomobject]@{installed=$false}) $patch $false;Commit $plan
    Check (Same ([IO.File]::ReadAllBytes($path)) $original) 'old bank28 contamination repaired on Restore'
    [IO.File]::WriteAllBytes((Get-LayoutBankPath 28),$patch.Bytes)
    [IO.File]::WriteAllBytes($path,(Build-LayoutIndex $stale))
    $plan=Get-TraitLayoutPlan ([pscustomobject]@{installed=$false}) $patch $true $off;Commit $plan
    $fixed=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
    Check (Same $fixed.Entries[$fixed.ByKtid[[uint32]$record.ktid]] ([Convert]::FromBase64String($record.original_entry))) 'old bank28 contamination repaired on OFF Apply'
    # Same locator in an externally changed bank is not ownership evidence.
    $foreign=[byte[]]$patch.Bytes.Clone();$foreign[[int]$record.offset]=$foreign[[int]$record.offset] -bxor 1
    [IO.File]::WriteAllBytes((Get-LayoutBankPath 28),$foreign)
    [IO.File]::WriteAllBytes($path,(Build-LayoutIndex $stale))
    $plan=Get-TraitLayoutPlan ([pscustomobject]@{installed=$false}) $patch $false;Commit $plan
    Check (Same ([IO.File]::ReadAllBytes($path)) (Build-LayoutIndex $stale)) 'foreign same-locator payload preserved'
    $result=@{status='ccz_layout_switch_passed';checks=$checks;actual_game_changed=$false}
    $result | ConvertTo-Json | Set-Content (Join-Path $taskProject 'build\development\ccz_layout_switch_results.json') -Encoding UTF8
    $result | ConvertTo-Json -Compress
} finally {
    $resolved=[IO.Path]::GetFullPath($taskGame)
    $allowed=[IO.Path]::GetFullPath((Join-Path $taskProject 'build'))+[IO.Path]::DirectorySeparatorChar
    if($resolved.StartsWith($allowed) -and (Split-Path -Leaf $resolved) -like 'ccz_layout_fixture_*'){Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue}
}
