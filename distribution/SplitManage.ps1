# Helpers/paths are supplied by Manage.ps1. Only these mod-owned names are allowed.
# Traits9 layout: our four ScreenLayout index entries are merged into the
# current index and point to a bank number chosen at install time. Every other
# index entry stays byte-for-byte, so a face editor or another layout mod may
# change ScreenLayout.rdb before or after this mod without colliding.
$taskLegacyLayout=@('0002/ScreenLayout.rdb','0002/ScreenLayout.rdb.bin31')
function Read-LayoutIndex([byte[]]$Data) {
    if($Data.Length -lt 20 -or [Text.Encoding]::ASCII.GetString($Data,0,8) -cne '_DRK0000'){throw 'ScreenLayout.rdb 형식이 올바르지 않습니다.'}
    $taskHead=[int][BitConverter]::ToUInt32($Data,8)
    $taskEntries=New-Object 'System.Collections.Generic.List[byte[]]'
    $taskByKtid=@{}
    $taskAt=$taskHead
    while($taskAt -lt $Data.Length) {
        if($taskAt+12 -gt $Data.Length -or [Text.Encoding]::ASCII.GetString($Data,$taskAt,8) -cne 'IDRK0000'){throw 'ScreenLayout.rdb 항목 경계가 올바르지 않습니다.'}
        $taskSize=[int][BitConverter]::ToUInt32($Data,$taskAt+8)
        if($taskSize -lt 40 -or $taskAt+$taskSize -gt $Data.Length){throw 'ScreenLayout.rdb 항목 크기가 올바르지 않습니다.'}
        $taskEntry=[byte[]]::new($taskSize);[Array]::Copy($Data,$taskAt,$taskEntry,0,$taskSize)
        $taskKtid=[BitConverter]::ToUInt32($taskEntry,36)
        # Preserve duplicate detection: a duplicate key can never be patched.
        if($taskByKtid.ContainsKey($taskKtid)){$taskByKtid[$taskKtid]=-1}
        else{$taskByKtid[$taskKtid]=$taskEntries.Count}
        $taskEntries.Add($taskEntry)
        $taskAt+=($taskSize+3) -band -4
    }
    if([BitConverter]::ToUInt32($Data,16) -ne $taskEntries.Count){throw 'ScreenLayout.rdb 항목 수가 올바르지 않습니다.'}
    $taskHeader=[byte[]]::new($taskHead);[Array]::Copy($Data,0,$taskHeader,0,$taskHead)
    return @{Header=$taskHeader;Entries=$taskEntries;ByKtid=$taskByKtid}
}
function Build-LayoutIndex($Index) {
    $taskStream=New-Object IO.MemoryStream
    $taskHeader=[byte[]]$Index.Header.Clone()
    [Array]::Copy([BitConverter]::GetBytes([uint32]$Index.Entries.Count),0,$taskHeader,16,4)
    $taskStream.Write($taskHeader,0,$taskHeader.Length)
    foreach($taskEntry in $Index.Entries) {
        $taskStream.Write($taskEntry,0,$taskEntry.Length)
        $taskPad=(4-($taskEntry.Length%4))%4
        if($taskPad){$taskStream.Write(([byte[]]::new($taskPad)),0,$taskPad)}
    }
    return ,$taskStream.ToArray()
}
function Get-LayoutKtid([byte[]]$Entry){return [BitConverter]::ToUInt32($Entry,36)}
function Get-LayoutLocator([byte[]]$Entry) {
    $taskLength=[BitConverter]::ToUInt64($Entry,16)
    if($taskLength -eq 0 -or $taskLength -gt [uint64]($Entry.Length-40)){return $null}
    $taskText=[Text.Encoding]::ASCII.GetString($Entry,$Entry.Length-[int]$taskLength,[int]$taskLength).TrimEnd([char]0)
    $taskMatch=[regex]::Match($taskText,'^([0-9a-f]+)@([0-9a-f]+)(?:#([0-9a-f]+))?$')
    if(-not $taskMatch.Success){return $null}
    $taskBank=if($taskMatch.Groups[3].Success){[Convert]::ToInt64($taskMatch.Groups[3].Value,16)}else{0}
    return ('{0}:{1}:{2}' -f [Convert]::ToInt64($taskMatch.Groups[1].Value,16),[Convert]::ToInt64($taskMatch.Groups[2].Value,16),$taskBank)
}
function Get-LayoutBanks($Index) {
    $taskBanks=New-Object 'System.Collections.Generic.HashSet[long]'
    foreach($taskEntry in $Index.Entries) {
        $taskLocator=Get-LayoutLocator $taskEntry
        if($taskLocator){[void]$taskBanks.Add([long]($taskLocator.Split(':')[2]))}
    }
    return ,$taskBanks
}
function New-LayoutEntry([byte[]]$Original,[long]$Offset,[long]$Size,[long]$Bank,[uint64]$Data) {
    $taskOld=[int][BitConverter]::ToUInt64($Original,16)
    $taskLocator=[Text.Encoding]::ASCII.GetBytes(('{0:x}@{1:x}#{2:x}' -f $Offset,$Size,$Bank)+[char]0)
    $taskKeep=$Original.Length-$taskOld;$taskLength=$taskKeep+$taskLocator.Length
    $taskEntry=[byte[]]::new($taskLength)
    [Array]::Copy($Original,0,$taskEntry,0,$taskKeep);[Array]::Copy($taskLocator,0,$taskEntry,$taskKeep,$taskLocator.Length)
    [Array]::Copy([BitConverter]::GetBytes([uint32]$taskLength),0,$taskEntry,8,4)
    [Array]::Copy([BitConverter]::GetBytes([uint64]$taskLocator.Length),0,$taskEntry,16,8)
    [Array]::Copy([BitConverter]::GetBytes([uint64]$Data),0,$taskEntry,24,8)
    return ,$taskEntry
}
function Get-LayoutTarget($Record,[long]$Bank){return ('{0}:{1}:{2}' -f [long]$Record.offset,[long]$Record.size,$Bank)}
function Reset-TraitLayout($Index,$Records,[long]$Bank) {
    # Only our entries that still point into our previous bank are returned to the
    # original entry. Any offset in that bank counts: a newer patch may resize records.
    $taskCount=0
    foreach($taskRecord in $Records) {
        $taskKtid=[uint32]$taskRecord.ktid
        # Reset every matching entry, as before, including duplicate KTIDs.
        if(-not $Index.ByKtid.ContainsKey($taskKtid)){continue}
        $taskPositions=if($Index.ByKtid[$taskKtid] -ge 0){@($Index.ByKtid[$taskKtid])}else{
            for($i=0;$i -lt $Index.Entries.Count;$i++){
                if([BitConverter]::ToUInt32($Index.Entries[$i],36) -eq $taskKtid){$i}
            }
        }
        foreach($i in $taskPositions) {
            $taskEntry=$Index.Entries[$i];$taskLocator=Get-LayoutLocator $taskEntry
            if($taskLocator -and [long]($taskLocator.Split(':')[2]) -eq $Bank) {
                $Index.Entries[$i]=[Convert]::FromBase64String($taskRecord.original_entry);$taskCount++
            }
        }
    }
    return $taskCount
}
function Set-TraitLayout($Index,$Records,[long]$Bank) {
    $taskApplied=@();$taskSkipped=@()
    foreach($taskRecord in $Records) {
        $taskKtid=[uint32]$taskRecord.ktid
        if(-not $Index.ByKtid.ContainsKey($taskKtid) -or $Index.ByKtid[$taskKtid] -lt 0){$taskSkipped+=$taskRecord.ktid;continue}
        $taskAt=$Index.ByKtid[$taskKtid]
        $taskCurrent=$Index.Entries[$taskAt]
        $taskMine=(Get-LayoutLocator $taskCurrent) -eq (Get-LayoutTarget $taskRecord $Bank)
        if(-not $taskMine -and [Convert]::ToBase64String($taskCurrent) -cne $taskRecord.original_entry){$taskSkipped+=$taskRecord.ktid;continue}
        $taskOriginal=[Convert]::FromBase64String($taskRecord.original_entry)
        $Index.Entries[$taskAt]=New-LayoutEntry $taskOriginal ([long]$taskRecord.offset) ([long]$taskRecord.size) $Bank ([uint64]$taskRecord.dsz)
        $taskApplied+=$taskRecord.ktid
    }
    return @{Applied=$taskApplied;Skipped=$taskSkipped}
}
function Get-LayoutBankPath([long]$Bank){return Join-Path $taskGame ('0002\ScreenLayout.rdb.bin'+$Bank)}
function Get-TraitLayoutPlan($State,$Patch,[bool]$Install) {
    # Returns the index/bank writes. Nothing is written here.
    $taskIndexPath=Join-Path $taskGame '0002\ScreenLayout.rdb'
    $taskPlan=@{Writes=@{};Deletes=@();State=$null;Changed=$false;Messages=@()}
    if(-not [IO.File]::Exists($taskIndexPath)){
        if($Install){$taskPlan.Messages+='0002\ScreenLayout.rdb가 없어 개성 9칸 화면 확장을 건너뜁니다.'}
        return $taskPlan
    }
    $taskOldBytes=[IO.File]::ReadAllBytes($taskIndexPath)
    $taskIndex=Read-LayoutIndex $taskOldBytes
    $taskRecords=if($null -ne $Patch){$Patch.records}else{$null}
    $taskReleased=@()
    $taskLegacy=$null -ne $State -and $State.PSObject.Properties.Name -contains 'modules' -and $null -ne $State.modules -and
        $State.modules.PSObject.Properties.Name -contains '0002/ScreenLayout.rdb.bin31'
    if($taskLegacy -and $null -ne $taskRecords) {
        # v11.22-v11.30 used a fixed bank 31, the same name as a face editor.
        [void](Reset-TraitLayout $taskIndex $taskRecords 31)
        $taskReleased+=@{Bank=31;Hash=$State.modules.'0002/ScreenLayout.rdb.bin31'}
        $taskPlan.Changed=$true
    }
    $taskPrevious=$null
    if($null -ne $State -and $State.PSObject.Properties.Name -contains 'layout' -and $null -ne $State.layout -and $null -ne $taskRecords) {
        $taskPrevious=$State.layout
        [void](Reset-TraitLayout $taskIndex $taskRecords ([long]$taskPrevious.bank))
        $taskReleased+=@{Bank=[long]$taskPrevious.bank;Hash=$taskPrevious.bank_sha256}
    }
    if(-not $Install -and $null -ne $taskRecords) {
        # Restore without (or with a stale) layout record: our entries are recognised by
        # their exact patch locator (offset:size) in any mod bank 24..31. Other mods'
        # entries never carry these locators, so they stay byte-for-byte.
        foreach($taskCandidate in 24..31) {
            $taskHit=$false
            foreach($taskRecord in $taskRecords) {
                $taskKtid=[uint32]$taskRecord.ktid
                if(-not $taskIndex.ByKtid.ContainsKey($taskKtid) -or $taskIndex.ByKtid[$taskKtid] -lt 0){continue}
                $taskAt=$taskIndex.ByKtid[$taskKtid]
                if((Get-LayoutLocator $taskIndex.Entries[$taskAt]) -eq (Get-LayoutTarget $taskRecord ([long]$taskCandidate))) {
                    $taskIndex.Entries[$taskAt]=[Convert]::FromBase64String($taskRecord.original_entry);$taskHit=$true
                }
            }
            if($taskHit -and -not (@($taskReleased | ForEach-Object { [long]$_.Bank }) -contains [long]$taskCandidate)) {
                $taskReleased+=@{Bank=[long]$taskCandidate;Hash=$Patch.Hash}
            }
        }
    }
    $taskBanks=Get-LayoutBanks $taskIndex
    $taskKeep=$null
    foreach($taskOld in $taskReleased) {
        $taskPath=Get-LayoutBankPath $taskOld.Bank
        if((Get-Digest $taskPath) -eq $taskOld.Hash -and -not $taskBanks.Contains($taskOld.Bank)) {
            if($Install -and $taskOld.Bank -ne 31 -and $null -eq $taskKeep){$taskKeep=$taskOld.Bank}
            else{$taskPlan.Deletes+=$taskPath}
        } elseif((Get-Digest $taskPath) -ne $taskOld.Hash -and [IO.File]::Exists($taskPath)) {
            $taskPlan.Messages+=('ScreenLayout.rdb.bin{0}은 다른 모드가 사용 중이라 지우지 않았습니다.' -f $taskOld.Bank)
        }
    }
    if($Install -and $null -ne $taskRecords) {
        $taskBank=$taskKeep
        if($null -eq $taskBank) {
            foreach($taskCandidate in 30..24) {
                if(-not $taskBanks.Contains([long]$taskCandidate) -and -not [IO.File]::Exists((Get-LayoutBankPath $taskCandidate)) -and
                   -not ($taskPlan.Deletes -contains (Get-LayoutBankPath $taskCandidate))){$taskBank=[long]$taskCandidate;break}
            }
        }
        if($null -eq $taskBank) {
            $taskPlan.Messages+='비어 있는 ScreenLayout 번호(24~30)가 없어 개성 9칸 화면 확장을 건너뜁니다.'
        } else {
            $taskResult=Set-TraitLayout $taskIndex $taskRecords $taskBank
            if($taskResult.Skipped.Count){$taskPlan.Messages+=('다른 모드가 바꾼 무장 화면 {0}개는 그대로 두었습니다. 해당 화면의 개성은 5칸으로 표시됩니다.' -f $taskResult.Skipped.Count)}
            if($taskResult.Applied.Count) {
                $taskBankPath=Get-LayoutBankPath $taskBank
                if((Get-Digest $taskBankPath) -ne $Patch.Hash){$taskPlan.Writes[$taskBankPath]=$Patch.Bytes}
                $taskPlan.State=@{format=1;bank=$taskBank;bank_sha256=$Patch.Hash;applied=@($taskResult.Applied)}
                if($null -eq $taskPrevious -or [long]$taskPrevious.bank -ne $taskBank){$taskPlan.Changed=$true}
                $taskPlan.Messages+=('개성 9칸 화면을 ScreenLayout.rdb.bin{0}에 연결했습니다.' -f $taskBank)
            }
        }
    }
    $taskNewBytes=Build-LayoutIndex $taskIndex
    if([Convert]::ToBase64String($taskNewBytes) -cne [Convert]::ToBase64String($taskOldBytes)){$taskPlan.Writes[$taskIndexPath]=$taskNewBytes;$taskPlan.Changed=$true}
    if($taskPlan.Deletes.Count){$taskPlan.Changed=$true}
    return $taskPlan
}
function Invoke-SplitPackage {
    Assert-GameClosed
    $taskAllowed=@('SAN14_CombatAI.dll','SAN14_Convenience.dll','SAN14_전법설명.txt')+$taskLegacyLayout
    # Reference text only; no DLL reads it. A same-named file from another mod or an
    # older copy never blocks install/update/restore: it is left as it is, not owned.
    $taskOptionalAssets=@('SAN14_전법설명.txt')
    $taskState=$null
    if([IO.File]::Exists($taskStateFile)){$taskState=Get-Content -LiteralPath $taskStateFile -Raw -Encoding UTF8 | ConvertFrom-Json}
    $taskOwned=@{}
    if($null -ne $taskState -and $taskState.PSObject.Properties.Name -contains 'modules' -and $null -ne $taskState.modules) {
        foreach($taskProperty in $taskState.modules.PSObject.Properties) {
            if($taskAllowed -notcontains $taskProperty.Name){throw '설치 기록에 지원하지 않는 DLL 이름이 있습니다.'}
            # Old whole-index layout records are migrated by Get-TraitLayoutPlan.
            if($taskLegacyLayout -notcontains $taskProperty.Name){$taskOwned[$taskProperty.Name]=$taskProperty.Value}
        }
    }
    $taskPatch=$null
    $taskPatchJson=Join-Path $taskPayload 'traits9_layout.json';$taskPatchBin=Join-Path $taskPayload 'traits9_layout.bin'
    if([IO.File]::Exists($taskPatchJson)) {
        $taskPatch=Get-Content -LiteralPath $taskPatchJson -Raw -Encoding UTF8 | ConvertFrom-Json
        if($taskPatch.format -ne 1 -or (Get-Digest $taskPatchBin) -ne $taskPatch.bank_sha256){throw '개성 9칸 화면 자료의 검사가 실패했습니다.'}
        $taskPatch | Add-Member -NotePropertyName Bytes -NotePropertyValue ([IO.File]::ReadAllBytes($taskPatchBin))
        $taskPatch | Add-Member -NotePropertyName Hash -NotePropertyValue $taskPatch.bank_sha256
    }
    $taskCurrent=Get-Digest $taskTarget
    $taskWasInstalled=$null -ne $taskState -and $taskState.installed
    if($Mode -eq 'Restore') {
        if($null -eq $taskState){Write-Host 'AI 모드 설치 기록이 없습니다. 파일을 변경하지 않았습니다.';return}
        if(-not $taskWasInstalled) {
            if($taskCurrent -ne $taskState.previous_sha256){throw '해제 이후 입력 DLL이 변경되었습니다.'}
            # An earlier restore may have left our layout entries (missing layout record):
            # clean only entries that still carry this mod's exact patch locators.
            $taskLeft=Get-TraitLayoutPlan $taskState $taskPatch $false
            if($taskLeft.Writes.Count -or $taskLeft.Deletes.Count) {
                Assert-GameClosed
                $taskBackup=@{}
                foreach($taskPath in @($taskLeft.Writes.Keys)+$taskLeft.Deletes){if([IO.File]::Exists($taskPath)){$taskBackup[$taskPath]=[IO.File]::ReadAllBytes($taskPath)}}
                try {
                    foreach($taskPath in $taskLeft.Writes.Keys){Write-Atomic $taskPath $taskLeft.Writes[$taskPath]}
                    foreach($taskPath in $taskLeft.Deletes){[IO.File]::Delete($taskPath)}
                } catch {
                    foreach($taskPath in $taskBackup.Keys){Write-Atomic $taskPath $taskBackup[$taskPath]}
                    throw
                }
                foreach($taskMessage in $taskLeft.Messages){Write-Host $taskMessage}
                Write-Host '남아 있던 개성 9칸·진행 속도 화면을 원본으로 되돌렸습니다.'
            }
            Write-Host 'AI 모드는 이미 해제되어 있습니다.';return
        }
        if($taskCurrent -ne $taskState.installed_sha256 -or (Get-Digest $taskChain) -ne $taskState.previous_sha256){throw '설치된 입력 DLL 또는 보존 DLL이 변경되었습니다.'}
        $taskOldFiles=@{$taskTarget=[IO.File]::ReadAllBytes($taskTarget);$taskStateFile=[IO.File]::ReadAllBytes($taskStateFile)}
        foreach($taskName in @($taskOwned.Keys)) {
            $taskPath=Join-Path $taskGame $taskName
            if($taskOptionalAssets -contains $taskName -and (Get-Digest $taskPath) -ne $taskOwned[$taskName]) {
                $taskOwned.Remove($taskName);Write-Host ($taskName+'은(는) 바뀌었거나 없어서 그대로 둡니다.');continue
            }
            if((Get-Digest $taskPath) -ne $taskOwned[$taskName]){throw ('설치된 '+$taskName+'이 변경되었습니다. 자동 삭제하지 않습니다.')}
            $taskOldFiles[$taskPath]=[IO.File]::ReadAllBytes($taskPath)
        }
        $taskLayout=Get-TraitLayoutPlan $taskState $taskPatch $false
        foreach($taskPath in @($taskLayout.Writes.Keys)+$taskLayout.Deletes){if([IO.File]::Exists($taskPath)){$taskOldFiles[$taskPath]=[IO.File]::ReadAllBytes($taskPath)}}
        $taskOriginal=if($null -ne $taskState.previous_sha256){[IO.File]::ReadAllBytes($taskChain)}else{$null}
        Assert-GameClosed
        try {
            if($null -eq $taskOriginal){[IO.File]::Delete($taskTarget)}else{Write-Atomic $taskTarget $taskOriginal}
            foreach($taskName in $taskOwned.Keys){[IO.File]::Delete((Join-Path $taskGame $taskName))}
            foreach($taskPath in $taskLayout.Writes.Keys){Write-Atomic $taskPath $taskLayout.Writes[$taskPath]}
            foreach($taskPath in $taskLayout.Deletes){[IO.File]::Delete($taskPath)}
            $taskNewState=@{installed=$false;installed_sha256=$taskState.installed_sha256;previous_sha256=$taskState.previous_sha256;exe_unchanged=$true;
                modules=$taskOwned;release_version=$taskState.release_version;install_backup=$taskState.install_backup}
            Write-State $taskNewState
        } catch {
            foreach($taskPath in $taskOldFiles.Keys){Write-Atomic $taskPath $taskOldFiles[$taskPath]}
            throw
        }
        if($null -ne $taskOriginal){[IO.File]::Delete($taskChain)}
        foreach($taskMessage in $taskLayout.Messages){Write-Host $taskMessage}
        Write-Host '로더·전투 판단·편의 기능 DLL을 해제하고 설치 전 구성을 복구했습니다.'
        return
    }
    if(-not [IO.File]::Exists($taskExe)){throw 'SAN14CE_KO.exe가 없습니다. ZIP을 실제 게임 실행 파일과 같은 폴더에 풀어 주세요.'}
    try { Assert-GameBuild $taskExe }
    catch { Write-CompatibilityDiagnostic; throw }
    $taskExeHash=Get-Digest $taskExe
    if($taskExeHash -ne $taskOriginalHash) {
        Write-Host '지원 PE 빌드 지문이 일치합니다. EXE 전체 해시 차이는 설치 차단 사유가 아닙니다.'
        Write-Host ('현재 EXE SHA256: ' + $taskExeHash)
        Write-Host '게임 실행 시 DLL이 복호화된 코드 CRC와 후킹 원본 바이트를 다시 검사합니다.'
    }
    $taskManifest=Get-Content -LiteralPath (Join-Path $taskPayload 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($taskManifest.split_modules.abi -ne 1){throw '지원하지 않는 분리 모드 형식입니다.'}
    $taskNames=@($taskManifest.split_modules.files.PSObject.Properties.Name)
    $taskRequired=@('dinput8.dll','SAN14_CombatAI.dll')
    if($taskManifest.split_modules.loader_only){$taskRequired+='SAN14_Convenience.dll'}
    if($taskNames.Count -ne $taskRequired.Count){throw '분리 모드의 DLL 목록이 올바르지 않습니다.'}
    foreach($taskName in $taskRequired){if($taskNames -notcontains $taskName){throw '분리 모드의 필수 DLL이 없습니다.'}}
    $taskExpected=@{}
    foreach($taskName in $taskNames){$taskExpected[$taskName]=$taskManifest.split_modules.files.$taskName}
    if($taskManifest.PSObject.Properties.Name -contains 'assets') {
        foreach($taskAsset in $taskManifest.assets.PSObject.Properties) {
            if($taskAsset.Name -ne 'SAN14_전법설명.txt' -or $taskAsset.Value -notmatch '^[a-f0-9]{64}$'){throw '지원하지 않는 보조 파일입니다.'}
            $taskNames+=$taskAsset.Name;$taskExpected[$taskAsset.Name]=$taskAsset.Value
        }
    }
    # Current packages no longer carry the old reference text. Accept its old
    # ownership record on upgrade, but leave any existing file untouched. This
    # also handles an edited or missing copy without weakening DLL validation.
    foreach($taskName in $taskOptionalAssets) {
        if($taskNames -notcontains $taskName){$taskOwned.Remove($taskName)}
    }
    if($taskManifest.PSObject.Properties.Name -contains 'layout_patch') {
        if($null -eq $taskPatch -or $taskPatch.bank_sha256 -ne $taskManifest.layout_patch.bank_sha256 -or
           (Get-Digest $taskPatchJson) -ne $taskManifest.layout_patch.json_sha256){throw '개성 9칸 화면 자료가 배포 목록과 다릅니다.'}
    } else {$taskPatch=$null}
    $taskBytes=@{};$taskHashes=@{};$taskOldFiles=@{}
    foreach($taskName in $taskNames) {
        $taskSource=Join-Path $taskPayload $taskName
        $taskHash=Get-Digest $taskSource
        if(-not $taskHash -or $taskHash -ne $taskExpected[$taskName]){throw ('배포 '+$taskName+'의 해시 검사가 실패했습니다.')}
        $taskBytes[$taskName]=[IO.File]::ReadAllBytes($taskSource);$taskHashes[$taskName]=$taskHash
    }
    if($taskHashes['dinput8.dll'] -ne $taskManifest.dll_sha256){throw '배포 입력 DLL의 검사가 실패했습니다.'}
    foreach($taskName in @($taskNames | Where-Object { $taskOptionalAssets -contains $_ })) {
        $taskHash=Get-Digest (Join-Path $taskGame $taskName)
        $taskMine=$taskWasInstalled -and $taskOwned.ContainsKey($taskName) -and $taskHash -eq $taskOwned[$taskName]
        if($null -ne $taskHash -and -not $taskMine) {
            $taskNames=@($taskNames | Where-Object { $_ -ne $taskName });$taskOwned.Remove($taskName)
            Write-Host ('기존 '+$taskName+'은(는) 그대로 두고 설치합니다(모드 동작에 필요 없는 안내 파일).')
        } elseif($null -eq $taskHash -and $taskOwned.ContainsKey($taskName)) {$taskOwned.Remove($taskName)}
    }
    if($taskWasInstalled) {
        if($taskCurrent -ne $taskState.installed_sha256 -or (Get-Digest $taskChain) -ne $taskState.previous_sha256){throw '설치된 입력 DLL 또는 보존 DLL이 변경되었습니다.'}
        foreach($taskName in $taskOwned.Keys) {
            if((Get-Digest (Join-Path $taskGame $taskName)) -ne $taskOwned[$taskName]){throw ('설치된 '+$taskName+'이 변경되었습니다.')}
            if($taskNames -notcontains $taskName){throw '현재 모듈을 누락한 패키지로 업데이트할 수 없습니다.'}
        }
        $taskPrevious=$taskState.previous_sha256
    } else {
        if([IO.File]::Exists($taskChain)){throw '보존용 DLL 이름이 이미 사용 중입니다.'}
        if($taskCurrent -eq $taskHashes['dinput8.dll']){throw 'AI DLL은 있으나 설치 기록이 없습니다.'}
        if($null -ne $taskState -and $taskCurrent -ne $taskState.previous_sha256){throw '해제 후 입력 DLL이 변경되었습니다.'}
        $taskPrevious=$taskCurrent
    }
    $taskAllCurrent=$taskWasInstalled
    foreach($taskName in $taskNames) {
        $taskPath=Join-Path $taskGame $taskName;$taskHash=Get-Digest $taskPath
        if($taskName -ne 'dinput8.dll' -and (-not $taskWasInstalled -or -not $taskOwned.ContainsKey($taskName)) -and $null -ne $taskHash){throw ('기존 '+$taskName+' 이름이 사용 중입니다.')}
        if($taskHash -ne $taskHashes[$taskName]){$taskAllCurrent=$false}
        $taskOldFiles[$taskPath]=if($null -ne $taskHash){[IO.File]::ReadAllBytes($taskPath)}else{$null}
    }
    $taskLayout=Get-TraitLayoutPlan $taskState $taskPatch $true
    if($taskLayout.Changed -or $taskLayout.Writes.Count){$taskAllCurrent=$false}
    # Feature switches are chosen before any file changes (menu unless -NonInteractive).
    $taskSettings=Join-Path $taskProject '기능설정.ini'
    [void](Invoke-FeatureSelection $taskSettings (-not $NonInteractive))
    if($taskAllCurrent) {
        # Files are current; still record a new release number or a released optional file.
        $taskRecorded=@{};foreach($taskName in $taskNames){if($taskName -ne 'dinput8.dll'){$taskRecorded[$taskName]=$taskHashes[$taskName]}}
        $taskStateModules=@{};foreach($taskProperty in $taskState.modules.PSObject.Properties){$taskStateModules[$taskProperty.Name]=$taskProperty.Value}
        $taskStateOwn=@($taskStateModules.Keys | Where-Object { $taskLegacyLayout -notcontains $_ })
        $taskSame=$taskStateOwn.Count -eq $taskRecorded.Count -and @($taskStateOwn | Where-Object { $taskStateModules[$_] -ne $taskRecorded[$_] }).Count -eq 0
        if($taskState.release_version -ne $taskManifest.release_version -or -not $taskSame) {
            foreach($taskName in @($taskStateModules.Keys)){if($taskLegacyLayout -notcontains $taskName){$taskStateModules.Remove($taskName)}}
            foreach($taskName in $taskRecorded.Keys){$taskStateModules[$taskName]=$taskRecorded[$taskName]}
            $taskState | Add-Member -NotePropertyName modules -NotePropertyValue ([pscustomobject]$taskStateModules) -Force
            $taskState | Add-Member -NotePropertyName release_version -NotePropertyValue $taskManifest.release_version -Force
            Assert-GameClosed
            Write-State $taskState
            Write-Host ('설치 기록을 '+$taskManifest.release_version+'(으)로 갱신했습니다.')
        }
        Write-Host '모든 DLL이 최신 버전입니다.';return
    }
    foreach($taskPath in @($taskLayout.Writes.Keys)+$taskLayout.Deletes) {
        if(-not $taskOldFiles.ContainsKey($taskPath)){$taskOldFiles[$taskPath]=if([IO.File]::Exists($taskPath)){[IO.File]::ReadAllBytes($taskPath)}else{$null}}
    }
    $taskOldFiles[$taskStateFile]=if([IO.File]::Exists($taskStateFile)){[IO.File]::ReadAllBytes($taskStateFile)}else{$null}
    $taskBackupRoot=[IO.Path]::GetFullPath((Join-Path $taskProject 'backup'))
    $taskBackup=[IO.Path]::GetFullPath((Join-Path $taskBackupRoot ('split_install_'+[Guid]::NewGuid().ToString('N'))))
    if(-not $taskBackup.StartsWith($taskBackupRoot+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw '백업 경로 오류입니다.'}
    [void][IO.Directory]::CreateDirectory($taskBackup)
    foreach($taskPath in $taskOldFiles.Keys) {
        if($null -ne $taskOldFiles[$taskPath]){[IO.File]::WriteAllBytes((Join-Path $taskBackup ([IO.Path]::GetFileName($taskPath))),$taskOldFiles[$taskPath])}
    }
    $taskNewModules=@{}
    foreach($taskName in $taskNames){if($taskName -ne 'dinput8.dll'){$taskNewModules[$taskName]=$taskHashes[$taskName]}}
    $taskNewState=@{installed=$true;installed_sha256=$taskHashes['dinput8.dll'];previous_sha256=$taskPrevious;exe_unchanged=$true;modules=$taskNewModules;release_version=$taskManifest.release_version;install_backup=$taskBackup}
    if($null -ne $taskLayout.State){$taskNewState.layout=$taskLayout.State}
    Assert-GameClosed
    try {
        if(-not $taskWasInstalled -and $null -ne $taskOldFiles[$taskTarget]){Write-Atomic $taskChain $taskOldFiles[$taskTarget]}
        foreach($taskName in $taskNames){if($taskName -ne 'dinput8.dll'){Write-Atomic (Join-Path $taskGame $taskName) $taskBytes[$taskName]}}
        foreach($taskPath in $taskLayout.Writes.Keys){Write-Atomic $taskPath $taskLayout.Writes[$taskPath]}
        foreach($taskPath in $taskLayout.Deletes){[IO.File]::Delete($taskPath)}
        Write-Atomic $taskTarget $taskBytes['dinput8.dll']
        Write-State $taskNewState
    } catch {
        foreach($taskPath in $taskOldFiles.Keys) {
            if($null -eq $taskOldFiles[$taskPath]){[IO.File]::Delete($taskPath)}else{Write-Atomic $taskPath $taskOldFiles[$taskPath]}
        }
        if(-not $taskWasInstalled -and $null -ne $taskOldFiles[$taskTarget] -and (Get-Digest $taskChain) -eq $taskPrevious){[IO.File]::Delete($taskChain)}
        throw
    }
    foreach($taskMessage in $taskLayout.Messages){Write-Host $taskMessage}
    Write-Host '로더·전투 판단·편의 기능 DLL을 설치했습니다. 평소 Steam/런처로 실행해 주세요.'
    Write-Host '기능을 나중에 바꾸려면 AI_TacticsMod\optional_tools\AI모드_기능선택.cmd를 실행하세요.'
}
