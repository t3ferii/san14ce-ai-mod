# user 2026-11-03: ?? ?? ? ??. CCZ OFF/Restore A-H fixtures.
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
function Reset-Fixture([byte[]]$IndexBytes,[long[]]$KeepBanks=@()) {
    [IO.File]::WriteAllBytes($path,$IndexBytes)
    foreach($b in 24..31){$bp=Get-LayoutBankPath $b;if(($KeepBanks -notcontains $b) -and [IO.File]::Exists($bp)){[IO.File]::Delete($bp)}}
}
. (Join-Path $taskProject 'distribution\SplitManage.ps1')
$patch=Get-Content (Join-Path $Payload 'traits9_layout.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$patch | Add-Member -NotePropertyName Bytes -NotePropertyValue ([IO.File]::ReadAllBytes((Join-Path $Payload 'traits9_layout.bin')))
$patch | Add-Member -NotePropertyName Hash -NotePropertyValue $patch.bank_sha256
$keys=@([uint32]0x3DA1641D,[uint32]0x757F1A41,[uint32]0x5C046EA3)
$speedKeys=@([uint32]0x301526D7,[uint32]0xFEA5F28CL,[uint32]0x40F993CE,[uint32]0x454A1BCC)
# local17: the two auto-command toggles own the main-screen records 0x24E16FBE/0x036BB544; their keys default off,
# so a missing key keeps those records out. $on/$off turn them on to keep the CCZ/speed cases about CCZ/speed.
$autoKeys=@([uint32]0x24E16FBE,[uint32]0x036BB544)
$autoInBank=@($patch.records | Where-Object {$autoKeys -contains [uint32]$_.ktid}).Count
$total=@($patch.records).Count
$on=@{'groups.convenience'=1;'ccz.ccz_sprites'=1;'game_flow.auto_search'=1;'game_flow.auto_reward'=1}
$off=@{'groups.convenience'=1;'ccz.ccz_sprites'=0;'game_flow.auto_search'=1;'game_flow.auto_reward'=1}
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
    foreach($features in @($off,$on,$off,$on,@{'groups.convenience'=0;'ccz.ccz_sprites'=1},$on,@{'groups.convenience'=1;'ccz.ccz_sprites'=1;'game_flow.speed_buttons'=0},$on,@{'groups.convenience'=1;'ccz.ccz_sprites'=0;'game_flow.speed_buttons'=0},@{'groups.convenience'=1;'ccz.ccz_sprites'=1},$on,@{'groups.convenience'=1;'ccz.ccz_sprites'=1;'game_flow.auto_search'=0;'game_flow.auto_reward'=0},@{'groups.convenience'=1;'ccz.ccz_sprites'=1;'game_flow.auto_reward'=1},@{'groups.convenience'=0;'ccz.ccz_sprites'=1;'game_flow.auto_search'=1},$on)) {
        $before=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
        $plan=Get-TraitLayoutPlan $state $patch $true $features;Commit $plan
        $state=[pscustomobject]@{installed=$true;layout=[pscustomobject]$plan.State}
        $active=$features['groups.convenience'] -and (-not $features.Contains('groups.ccz') -or $features['groups.ccz']) -and $features['ccz.ccz_sprites']
        $speed=$features['groups.convenience'] -and (-not $features.Contains('game_flow.speed_buttons') -or $features['game_flow.speed_buttons'])
        $auto=$features['groups.convenience'] -and (($features.Contains('game_flow.auto_search') -and $features['game_flow.auto_search']) -or ($features.Contains('game_flow.auto_reward') -and $features['game_flow.auto_reward']))
        $count=$total-$(if($active){0}else{3})-$(if($speed){0}else{4})-$(if($auto){0}else{$autoInBank})
        Check ($plan.State.applied.Count -eq $count) 'selected layout count follows effective CCZ'
        $after=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
        foreach($r in $patch.records){
            $entry=$after.Entries[$after.ByKtid[[uint32]$r.ktid]]
            if((-not $active -and $keys -contains [uint32]$r.ktid) -or (-not $speed -and $speedKeys -contains [uint32]$r.ktid) -or (-not $auto -and $autoKeys -contains [uint32]$r.ktid)){Check (Same $entry ([Convert]::FromBase64String($r.original_entry))) 'CCZ OFF exact original entry'}
            else{Check ((Get-LayoutLocator $entry) -eq (Get-LayoutTarget $r $plan.State.bank)) 'enabled/non-CCZ entry remains applied'}
        }
        $unchanged=$true
        foreach($k in $base.ByKtid.Keys){if(@($patch.records.ktid) -notcontains $k -and -not (Same $before.Entries[$before.ByKtid[$k]] $after.Entries[$after.ByKtid[$k]])){$unchanged=$false;break}}
        Check $unchanged 'every unrelated record preserved'
    }
    # 2026-11-05 local3 (user): a build whose DLL keeps traits9 off (EXE TimeDateStamp 0x6AD06F53) gets no nine-slot records; the
    # local2 -> local3 upgrade (records already applied) takes them back to the exact original entries; other builds keep all 14.
    $t9Keys=@([uint32]0x49F89AB3,[uint32]0x7C471B31,[uint32]0x9EA0EB77L,[uint32]0xF1D669AFL)
    $exe=Join-Path $taskGame 'SAN14CE_KO.exe'
    function Write-StubExe([uint32]$Stamp) {
        $stub=[byte[]]::new(0x200);$stub[0]=0x4D;$stub[1]=0x5A
        [BitConverter]::GetBytes([int]0x80).CopyTo($stub,0x3C);[BitConverter]::GetBytes([uint32]0x4550).CopyTo($stub,0x80);[BitConverter]::GetBytes($Stamp).CopyTo($stub,0x88)
        [IO.File]::WriteAllBytes($exe,$stub)
    }
    # local18: since local10 no real build withholds traits9 ($taskBuildLayoutOff is empty), so the gate is exercised with a
    # fixture-only stamp 0x7FFF0001; the current Steam build 0x6AD06F53 must apply every record.
    $taskBuildLayoutOff['7FFF0001']=@('traits9')
    foreach($case in @(@{Stamp=[uint32]0x7FFF0001;Applied=($total-4);Off=$true},@{Stamp=[uint32]0x6AD06F53;Applied=$total;Off=$false},@{Stamp=[uint32]0x6A983C25;Applied=$total;Off=$false})) {
        Reset-Fixture $original
        Write-StubExe $case.Stamp
        $plan=Get-TraitLayoutPlan $null $patch $true $on;Commit $plan
        Check ($plan.State.applied.Count -eq $case.Applied) ('build gate applied count '+$case.Applied)
        $idx=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
        foreach($r in $patch.records | Where-Object {$t9Keys -contains [uint32]$_.ktid}) {
            $e=$idx.Entries[$idx.ByKtid[[uint32]$r.ktid]]
            if($case.Off){Check (Same $e ([Convert]::FromBase64String($r.original_entry))) 'traits9 record withheld on a build that keeps traits9 off'}
            else{Check ((Get-LayoutLocator $e) -eq (Get-LayoutTarget $r $plan.State.bank)) 'traits9 record applied on the reference build'}
        }
        foreach($r in $patch.records | Where-Object {$t9Keys -notcontains [uint32]$_.ktid}) {
            Check ((Get-LayoutLocator $idx.Entries[$idx.ByKtid[[uint32]$r.ktid]]) -eq (Get-LayoutTarget $r $plan.State.bank)) 'other records stay applied whatever the build'
        }
    }
    # upgrade: records applied under the reference build, then the new build is present
    Reset-Fixture $original
    Write-StubExe ([uint32]0x6A983C25)
    $oldPlan=Get-TraitLayoutPlan $null $patch $true $on;Commit $oldPlan
    $oldState=[pscustomobject]@{installed=$true;layout=[pscustomobject]$oldPlan.State}
    Check ($oldPlan.State.applied.Count -eq $total) 'upgrade fixture starts with every record applied'
    Write-StubExe ([uint32]0x7FFF0001)
    $newPlan=Get-TraitLayoutPlan $oldState $patch $true $on;Commit $newPlan
    $up=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
    Check ($newPlan.State.applied.Count -eq ($total-4)) 'upgrade to the traits9-off build re-applies all but the four traits9 records'
    foreach($r in $patch.records | Where-Object {$t9Keys -contains [uint32]$_.ktid}) {
        Check (Same $up.Entries[$up.ByKtid[[uint32]$r.ktid]] ([Convert]::FromBase64String($r.original_entry))) 'upgrade restores the exact original nine-slot entries'
    }
    [IO.File]::Delete($exe)
    [void]$taskBuildLayoutOff.Remove('7FFF0001')
    # Master OFF restores the user's previous entries even while CCZ sprites stay enabled.
    Reset-Fixture $original
    $onPlan=Get-TraitLayoutPlan $null $patch $true $on;Commit $onPlan
    $onState=[pscustomobject]@{installed=$true;layout=[pscustomobject]$onPlan.State}
    $masterOff=@{'groups.convenience'=1;'groups.ccz'=0;'ccz.ccz_sprites'=1}
    $offPlan=Get-TraitLayoutPlan $onState $patch $true $masterOff;Commit $offPlan
    $masterOffIndex=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
    foreach($r in $patch.records | Where-Object {$keys -contains [uint32]$_.ktid}) {
        Check (Same $masterOffIndex.Entries[$masterOffIndex.ByKtid[[uint32]$r.ktid]] ([Convert]::FromBase64String($r.original_entry))) 'groups.ccz OFF restores prior/original CCZ entry'
    }
    # Legacy settings without groups.ccz keep the CCZ layout applied.
    Reset-Fixture $original
    $legacy=@{'groups.convenience'=1;'ccz.ccz_sprites'=1}
    $legacyPlan=Get-TraitLayoutPlan $null $patch $true $legacy;Commit $legacyPlan
    $legacyIndex=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
    foreach($r in $patch.records | Where-Object {$keys -contains [uint32]$_.ktid}) {
        Check ((Get-LayoutLocator $legacyIndex.Entries[$legacyIndex.ByKtid[[uint32]$r.ktid]]) -eq (Get-LayoutTarget $r $legacyPlan.State.bank)) 'legacy missing groups.ccz keeps CCZ layout applied'
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
    # A: clean original -> ON stores the actual prior bytes -> OFF restores them.
    Reset-Fixture $original
    $a=Read-LayoutIndex $original;$aEntry=[byte[]]$a.Entries[$a.ByKtid[[uint32]$record.ktid]].Clone()
    $plan=Get-TraitLayoutPlan $null $patch $true $on;Commit $plan
    $aState=[pscustomobject]@{installed=$true;layout=[pscustomobject]$plan.State}
    Check ($plan.State.previous_entries[[string]([uint32]$record.ktid)] -ceq [Convert]::ToBase64String($aEntry)) 'A previous bytes saved'
    $plan=Get-TraitLayoutPlan $aState $patch $true $off;Commit $plan
    $aAfter=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
    Check (Same $aAfter.Entries[$aAfter.ByKtid[[uint32]$record.ktid]] $aEntry) 'A ON then OFF restores exact prior entry'
    # B: external mode entry and bank survive feature OFF and Restore.
    $b=Read-LayoutIndex $original;$bAt=$b.ByKtid[[uint32]$record.ktid]
    $bForeign=New-LayoutEntry $b.Entries[$bAt] 16 32 29 1;$b.Entries[$bAt]=$bForeign
    Reset-Fixture (Build-LayoutIndex $b);[IO.File]::WriteAllBytes((Get-LayoutBankPath 29),[byte[]](1,2,3))
    $plan=Get-TraitLayoutPlan $null $patch $true $on
    Check (-not $plan.State.previous_entries.ContainsKey([string]([uint32]$record.ktid))) 'B foreign entry does not create previous entry'
    $plan=Get-TraitLayoutPlan ([pscustomobject]@{installed=$false}) $patch $false;Commit $plan
    $bAfter=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
    Check ((Same $bAfter.Entries[$bAfter.ByKtid[[uint32]$record.ktid]] $bForeign) -and [IO.File]::Exists((Get-LayoutBankPath 29))) 'B foreign entry and bank preserved'
    # C: v15.3 locator with absent bin28 restores the package original.
    Reset-Fixture $original;$c=Read-LayoutIndex $original
    $cAt=$c.ByKtid[[uint32]$record.ktid];$c.Entries[$cAt]=New-LayoutEntry $c.Entries[$cAt] 0x2377ab 0x7be 28 1
    [IO.File]::WriteAllBytes($path,(Build-LayoutIndex $c))
    $plan=Get-TraitLayoutPlan ([pscustomobject]@{installed=$false}) $patch $false;Commit $plan
    $cAfter=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
    Check (Same $cAfter.Entries[$cAfter.ByKtid[[uint32]$record.ktid]] ([Convert]::FromBase64String($record.original_entry))) 'C absent bin28 restores original'
    # D: a historical bank hash restores the same locator and is deleted when unused.
    $oldBin=Join-Path $taskProject 'build/public_v15_3_package/traits9_layout.bin'
    if(-not [IO.File]::Exists($oldBin)){$oldBin=Join-Path $taskProject 'updates/v15.3/files/AI_TacticsMod/build/input_package/traits9_layout.bin'}
    # local18: a worktree has no untracked build/updates folders; fall back to the main checkout beside it (read only).
if(-not [IO.File]::Exists($oldBin)){$oldBin=Join-Path (Split-Path -Parent $taskProject) 'AI_TacticsMod/updates/v15.3/files/AI_TacticsMod/build/input_package/traits9_layout.bin'}
if(-not [IO.File]::Exists($oldBin)){throw 'D fixture missing v15.3 bank'}
    Reset-Fixture (Build-LayoutIndex $c);[IO.File]::WriteAllBytes((Get-LayoutBankPath 28),[IO.File]::ReadAllBytes($oldBin))
    $plan=Get-TraitLayoutPlan ([pscustomobject]@{installed=$false}) $patch $false;Commit $plan
    Check (-not [IO.File]::Exists((Get-LayoutBankPath 28))) 'D known historical bank restored and deleted'
    # E: same locator with an unknown bank hash is retained with a message.
    Reset-Fixture (Build-LayoutIndex $c);[IO.File]::WriteAllBytes((Get-LayoutBankPath 28),[byte[]](9,8,7,6))
    $plan=Get-TraitLayoutPlan ([pscustomobject]@{installed=$false}) $patch $false;Commit $plan
    Check ([IO.File]::Exists((Get-LayoutBankPath 28)) -and (($plan.Messages -join ' ') -match 'ScreenLayout.rdb.bin28')) 'E unknown bank retained and reported'
    # F: previous entry points at a missing bank, so package original is used.
    $f=Read-LayoutIndex $original;$fAt=$f.ByKtid[[uint32]$record.ktid]
    $f.Entries[$fAt]=New-LayoutEntry $f.Entries[$fAt] $record.offset $record.size 30 $record.dsz
    Reset-Fixture (Build-LayoutIndex $f)
    $fPrev=@{};$fPrev[[string]([uint32]$record.ktid)]=[Convert]::ToBase64String((New-LayoutEntry $f.Entries[$fAt] 1 2 29 1))
    $fState=[pscustomobject]@{installed=$false;layout=[pscustomobject]@{bank=30;bank_sha256=$patch.bank_sha256;previous_entries=$fPrev}}
    $plan=Get-TraitLayoutPlan $fState $patch $false;Commit $plan;$fAfter=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
    Check (Same $fAfter.Entries[$fAfter.ByKtid[[uint32]$record.ktid]] ([Convert]::FromBase64String($record.original_entry))) 'F missing previous bank falls back to package original'
    # G: old installation records without previous_entries still restore safely.
    Reset-Fixture (Build-LayoutIndex $stale);[IO.File]::WriteAllBytes((Get-LayoutBankPath 28),$patch.Bytes)
    $plan=Get-TraitLayoutPlan ([pscustomobject]@{installed=$false;layout=[pscustomobject]@{bank=28;bank_sha256=$patch.bank_sha256}}) $patch $false;Commit $plan
    $gAfter=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
    Check (Same $gAfter.Entries[$gAfter.ByKtid[[uint32]$record.ktid]] ([Convert]::FromBase64String($record.original_entry))) 'G old record without previous_entries works'
    # H: uninstall/Restore follows the same A-F restoration route.
    Reset-Fixture (Build-LayoutIndex $stale);[IO.File]::WriteAllBytes((Get-LayoutBankPath 28),$patch.Bytes)
    $plan=Get-TraitLayoutPlan ([pscustomobject]@{installed=$false}) $patch $false;Commit $plan
    $hAfter=Read-LayoutIndex ([IO.File]::ReadAllBytes($path))
    Check (Same $hAfter.Entries[$hAfter.ByKtid[[uint32]$record.ktid]] ([Convert]::FromBase64String($record.original_entry))) 'H Restore repairs legacy residue'
    $result=@{status='ccz_layout_switch_passed';checks=$checks;actual_game_changed=$false}
    $result | ConvertTo-Json | Set-Content (Join-Path $taskProject 'build\development\ccz_layout_switch_results.json') -Encoding UTF8
    $result | ConvertTo-Json -Compress
} finally {
    $resolved=[IO.Path]::GetFullPath($taskGame)
    $allowed=[IO.Path]::GetFullPath((Join-Path $taskProject 'build'))+[IO.Path]::DirectorySeparatorChar
    if($resolved.StartsWith($allowed) -and (Split-Path -Leaf $resolved) -like 'ccz_layout_fixture_*'){Remove-Item -LiteralPath $resolved -Recurse -Force -ErrorAction SilentlyContinue}
}
