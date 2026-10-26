param([Alias('Action')][ValidateSet('Apply','Restore','Select')][string]$Mode = 'Apply', [string]$GameDirectory, [switch]$NonInteractive)
$ErrorActionPreference = 'Stop'
try {
    if (-not $GameDirectory) {
        $GameDirectory = Split-Path -Parent $PSScriptRoot
        if (-not [IO.File]::Exists((Join-Path $GameDirectory 'SAN14CE_KO.exe')) -and -not $NonInteractive) {
            Add-Type -AssemblyName System.Windows.Forms
            $taskPicker = New-Object Windows.Forms.OpenFileDialog
            $taskPicker.Title = '실제 게임 폴더의 SAN14CE_KO.exe를 선택해 주세요'
            $taskPicker.Filter = '삼국지14 CE 한국어판|SAN14CE_KO.exe'
            try {
                if ($taskPicker.ShowDialog() -ne [Windows.Forms.DialogResult]::OK) { exit 0 }
                if ([IO.Path]::GetFileName($taskPicker.FileName) -ine 'SAN14CE_KO.exe') { throw 'SAN14CE_KO.exe를 선택해 주세요.' }
                $GameDirectory = [IO.Path]::GetDirectoryName($taskPicker.FileName)
            } finally { $taskPicker.Dispose() }
        }
    }
    $taskGame = [IO.Path]::GetFullPath($GameDirectory)
    $taskProject = Join-Path $taskGame 'AI_TacticsMod'
    $taskPayload = Join-Path $PSScriptRoot 'build\input_package'
    $taskStateFile = Join-Path $taskProject 'backup\input_state.json'
    $taskTarget = Join-Path $taskGame 'dinput8.dll'
    $taskChain = Join-Path $taskGame 'SAN14_AI_PreviousInput.dll'
    $taskExe = Join-Path $taskGame 'SAN14CE_KO.exe'
    $taskOriginalHash = '87ed4e46590e0939c9efcaef14fa0f892917604dfda86e0a4be319fccffa2d6a'
    function Get-Digest([string]$Path) {
        if ([IO.File]::Exists($Path)) {
            $taskAlgorithm = [Security.Cryptography.SHA256]::Create()
            $taskStream = [IO.File]::OpenRead($Path)
            try { return [BitConverter]::ToString($taskAlgorithm.ComputeHash($taskStream)).Replace('-','').ToLowerInvariant() }
            finally { $taskStream.Dispose(); $taskAlgorithm.Dispose() }
        }
        return $null
    }
    function Assert-GameClosed {
        if (Get-Process -Name SAN14CE_KO -ErrorAction SilentlyContinue) {
            throw '게임을 완전히 종료한 뒤 다시 실행해 주세요.'
        }
    }
    function Assert-GameBuild([string]$Path) {
        $taskReader = New-Object IO.BinaryReader([IO.File]::OpenRead($Path))
        try {
            if ($taskReader.BaseStream.Length -lt 64 -or $taskReader.ReadUInt16() -ne 0x5A4D) { throw '게임 실행 파일의 DOS 헤더가 올바르지 않습니다.' }
            $taskReader.BaseStream.Position = 0x3C
            $taskPE = $taskReader.ReadUInt32()
            if ($taskPE -lt 64 -or $taskPE -gt 4096 -or $taskPE + 84 -gt $taskReader.BaseStream.Length) { throw '게임 실행 파일의 PE 헤더가 올바르지 않습니다.' }
            $taskReader.BaseStream.Position = $taskPE
            if ($taskReader.ReadUInt32() -ne 0x4550 -or $taskReader.ReadUInt16() -ne 0x8664) { throw '한국어 Complete Edition 64비트 실행 파일이 필요합니다.' }
            $taskReader.BaseStream.Position = $taskPE + 8
            $taskTimestamp = $taskReader.ReadUInt32()
            if ($taskTimestamp -eq 0x6A6D9F72) { throw '롤백 게임 빌드(0x6A6D9F72)입니다. 현재 AI DLL은 0x6A983C25 빌드용입니다. 롤백판 함수 주소를 이식한 별도 DLL이 필요합니다.' }
            if ($taskTimestamp -ne 0x6A983C25) { throw ('지원 대상과 다른 게임 빌드입니다. 현재 0x{0:X8}, 지원 0x6A983C25.' -f $taskTimestamp) }
            $taskReader.BaseStream.Position = $taskPE + 24
            if ($taskReader.ReadUInt16() -ne 0x20B) { throw '64비트 PE32+ 실행 파일이 필요합니다.' }
            $taskReader.BaseStream.Position = $taskPE + 80
            $taskImageSize = $taskReader.ReadUInt32()
            if ($taskImageSize -eq 0x029C5000) {
                # 2026-10-25 user: same build with only the Steam stub section (.bind, 0x29C5000..0x29FF000)
                # removed. Accept it only when the remaining nine sections are exactly the supported ones.
                $taskReader.BaseStream.Position = $taskPE + 6
                $taskSections = $taskReader.ReadUInt16()
                $taskReader.BaseStream.Position = $taskPE + 20
                $taskOptional = $taskReader.ReadUInt16()
                $taskExpected = @(@('.text',0x1000,0x133E9AE),@('.rdata',0x1340000,0x6DF47E),@('.data',0x1A20000,0xE03DCC),
                    @('.pdata',0x2824000,0xDC47C),@('.tls',0x2901000,0x15),@('.gfids',0x2902000,0x9FC),
                    @('_RDATA',0x2903000,0x9950),@('.rsrc',0x290D000,0x530D0),@('.reloc',0x2961000,0x63DC8))
                if ($taskSections -ne 9) { throw ('지원 대상과 다른 이미지 크기입니다. 현재 0x{0:X8}, 지원 0x029FF000 (섹션 {1}개).' -f $taskImageSize,$taskSections) }
                for ($taskIndex = 0; $taskIndex -lt 9; $taskIndex++) {
                    $taskReader.BaseStream.Position = $taskPE + 24 + $taskOptional + 40 * $taskIndex
                    $taskName = [Text.Encoding]::ASCII.GetString($taskReader.ReadBytes(8)).TrimEnd([char]0)
                    $taskVirtualSize = $taskReader.ReadUInt32(); $taskAddress = $taskReader.ReadUInt32()
                    $taskWant = $taskExpected[$taskIndex]
                    if ($taskName -ne $taskWant[0] -or $taskAddress -ne $taskWant[1] -or $taskVirtualSize -ne $taskWant[2]) {
                        throw ('지원 대상과 다른 섹션 구성입니다: {0} (지원 {1}). .bind만 없는 같은 빌드가 아닙니다.' -f $taskName,$taskWant[0])
                    }
                }
                Write-Host 'Steam 보호 섹션(.bind)이 없는 같은 게임 빌드입니다. 원작 코드 확인은 게임 실행 중 모드가 기능별로 다시 합니다.'
            }
            elseif ($taskImageSize -ne 0x029FF000) { throw ('지원 대상과 다른 이미지 크기입니다. 현재 0x{0:X8}, 지원 0x029FF000.' -f $taskImageSize) }
        } finally { $taskReader.Dispose() }
    }
    function Write-CompatibilityDiagnostic {
        $taskDiagnostic = Join-Path $PSScriptRoot '진단.ps1'
        if ([IO.File]::Exists($taskDiagnostic)) {
            try { & $taskDiagnostic -GameDirectory $taskGame -Quiet }
            catch { Write-Host ('진단 기록을 만들지 못했습니다: ' + $_.Exception.Message) }
        }
    }
    function Write-Atomic([string]$Path, [byte[]]$Bytes) {
        # Preserve an unchanged combat DLL (and its file identity/timestamp).
        if ([IO.File]::Exists($Path)) {
            $taskAlgorithm = [Security.Cryptography.SHA256]::Create()
            try { $taskDesiredHash = [BitConverter]::ToString($taskAlgorithm.ComputeHash($Bytes)).Replace('-','').ToLowerInvariant() }
            finally { $taskAlgorithm.Dispose() }
            if ((Get-Digest $Path) -eq $taskDesiredHash) { return }
        }
        $taskTemp = $Path + '.aitactics.tmp'
        [IO.File]::WriteAllBytes($taskTemp, $Bytes)
        try {
            for ($taskAttempt=0; ; $taskAttempt++) {
                try {
                    if ([IO.File]::Exists($Path)) { [IO.File]::Replace($taskTemp, $Path, [NullString]::Value) }
                    else { [IO.File]::Move($taskTemp, $Path) }
                    break
                } catch {
                    $taskCause=$_.Exception
                    while ($taskCause.InnerException) { $taskCause=$taskCause.InnerException }
                    $taskCode=$taskCause.HResult -band 0xFFFF
                    if ($taskAttempt -lt 2 -and $taskCode -in @(32,33,1175)) {
                        Start-Sleep -Milliseconds (50*($taskAttempt+1))
                        continue
                    }
                    throw ('Atomic file update failed: {0} (Win32 {1}): {2}' -f $Path,$taskCode,$taskCause.Message)
                }
            }
        } finally {
            if ([IO.File]::Exists($taskTemp)) { [IO.File]::Delete($taskTemp) }
        }
    }
    function Write-State($State) {
        $taskJson = $State | ConvertTo-Json -Depth 6
        Write-Atomic $taskStateFile ([Text.Encoding]::UTF8.GetBytes($taskJson))
    }
    $taskSplitManifestPath=Join-Path $taskPayload 'manifest.json'
    $taskSplitManifest=if([IO.File]::Exists($taskSplitManifestPath)){Get-Content -LiteralPath $taskSplitManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json}else{$null}
    $taskSplitState=if([IO.File]::Exists($taskStateFile)){Get-Content -LiteralPath $taskStateFile -Raw -Encoding UTF8 | ConvertFrom-Json}else{$null}
    if(($null -ne $taskSplitManifest -and $taskSplitManifest.PSObject.Properties.Name -contains 'split_modules') -or
       ($null -ne $taskSplitState -and $taskSplitState.PSObject.Properties.Name -contains 'modules')) {
        . (Join-Path $PSScriptRoot 'SplitManage.ps1')
        . (Join-Path $PSScriptRoot 'FeatureSelect.ps1')
        if($Mode -eq 'Select') {
            [void](Invoke-FeatureSelection (Join-Path $taskProject '기능설정.ini') (-not $NonInteractive))
            exit 0
        }
        Invoke-SplitPackage
        Write-Host '원본 EXE·세이브·기존 안개 모드는 유지됩니다.'
        exit 0
    }
    Assert-GameClosed
    $taskState = $null
    if (Test-Path -LiteralPath $taskStateFile) {
        $taskState = Get-Content -LiteralPath $taskStateFile -Raw -Encoding UTF8 | ConvertFrom-Json
    }
    $taskCurrent = Get-Digest $taskTarget
    if ($Mode -eq 'Restore') {
        if ($null -eq $taskState) { Write-Host 'AI 모드 설치 기록이 없습니다. 파일을 변경하지 않았습니다.'; exit 0 }
        if (-not $taskState.installed) {
            if ($taskCurrent -ne $taskState.previous_sha256) { throw '해제 이후 입력 DLL이 변경되었습니다. 파일을 덮어쓰지 않습니다.' }
            Write-Host 'AI 모드는 이미 해제되어 있습니다.'; exit 0
        }
        if ($taskCurrent -ne $taskState.installed_sha256) { throw '설치 후 입력 DLL이 변경되었습니다. 자동 복구를 중단합니다.' }
        if ((Get-Digest $taskChain) -ne $taskState.previous_sha256) { throw '원래 입력 DLL의 보존 상태가 다릅니다. 자동 복구를 중단합니다.' }
        $taskOldBytes = [IO.File]::ReadAllBytes($taskTarget)
        $taskOriginalBytes = $null
        if ($null -ne $taskState.previous_sha256) { $taskOriginalBytes = [IO.File]::ReadAllBytes($taskChain) }
        Assert-GameClosed
        try {
            if ($null -eq $taskOriginalBytes) { [IO.File]::Delete($taskTarget) }
            else { Write-Atomic $taskTarget $taskOriginalBytes }
            $taskState.installed = $false
            Write-State $taskState
        } catch {
            Write-Atomic $taskTarget $taskOldBytes
            throw
        }
        if ($null -ne $taskOriginalBytes) { [IO.File]::Delete($taskChain) }
        Write-Host 'AI 모드를 해제하고 설치 전 입력 설정으로 복구했습니다.'
    } else {
        if (-not [IO.File]::Exists($taskExe)) { throw 'SAN14CE_KO.exe가 없습니다. ZIP을 실제 게임 실행 파일과 같은 폴더에 풀어 주세요.' }
        try { Assert-GameBuild $taskExe }
        catch { Write-CompatibilityDiagnostic; throw }
        $taskExeHash = Get-Digest $taskExe
        if ($taskExeHash -ne $taskOriginalHash) {
            Write-Host '지원 PE 빌드 지문이 일치합니다. EXE 전체 해시 차이는 설치 차단 사유가 아닙니다.'
            Write-Host ('현재 EXE SHA256: ' + $taskExeHash)
            Write-Host '게임 실행 시 DLL이 복호화된 코드 CRC와 후킹 원본 바이트를 다시 검사합니다.'
        }
        $taskManifest = Get-Content -LiteralPath (Join-Path $taskPayload 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
        $taskNewPath = Join-Path $taskPayload 'dinput8.dll'
        $taskNewHash = Get-Digest $taskNewPath
        if (-not $taskNewHash -or $taskNewHash -ne $taskManifest.dll_sha256) { throw '배포 DLL의 검사가 실패했습니다. ZIP을 다시 풀어 주세요.' }
        $taskNewBytes = [IO.File]::ReadAllBytes($taskNewPath)
        if ($null -ne $taskState -and $taskState.installed) {
            if ($taskCurrent -ne $taskState.installed_sha256 -or (Get-Digest $taskChain) -ne $taskState.previous_sha256) {
                throw '설치된 AI DLL 또는 기존 입력 DLL이 변경되었습니다. 자동 업데이트를 중단합니다.'
            }
            if ($taskCurrent -eq $taskNewHash) { Write-Host '이미 최신 AI 모드가 적용되어 있습니다.'; exit 0 }
            $taskOldBytes = [IO.File]::ReadAllBytes($taskTarget)
            $taskOldState = [IO.File]::ReadAllBytes($taskStateFile)
            Assert-GameClosed
            try {
                Write-Atomic $taskTarget $taskNewBytes
                $taskState.installed_sha256 = $taskNewHash
                Write-State $taskState
            } catch {
                Write-Atomic $taskTarget $taskOldBytes
                Write-Atomic $taskStateFile $taskOldState
                throw
            }
        } else {
            if (Test-Path -LiteralPath $taskChain) { throw '보존용 DLL 이름이 이미 사용 중입니다. 파일을 덮어쓰지 않습니다.' }
            if ($taskCurrent -eq $taskNewHash) { throw 'AI DLL은 있으나 설치 기록이 없습니다. DLL을 다른 모드로 백업하지 않습니다.' }
            if ($null -ne $taskState -and $taskCurrent -ne $taskState.previous_sha256) { throw '해제 후 입력 DLL이 변경되었습니다. 기존 기록을 덮어쓰지 않습니다.' }
            [void][IO.Directory]::CreateDirectory((Split-Path -Parent $taskStateFile))
            $taskOldBytes = $null
            if ($null -ne $taskCurrent) { $taskOldBytes = [IO.File]::ReadAllBytes($taskTarget) }
            $taskOldState = $null
            if ([IO.File]::Exists($taskStateFile)) { $taskOldState = [IO.File]::ReadAllBytes($taskStateFile) }
            $taskNewState = @{installed=$true; installed_sha256=$taskNewHash; previous_sha256=$taskCurrent; exe_unchanged=$true}
            Assert-GameClosed
            try {
                if ($null -ne $taskOldBytes) { Write-Atomic $taskChain $taskOldBytes }
                Write-Atomic $taskTarget $taskNewBytes
                Write-State $taskNewState
            } catch {
                if ($null -eq $taskOldBytes) { [IO.File]::Delete($taskTarget) }
                else { Write-Atomic $taskTarget $taskOldBytes }
                if ($null -ne $taskOldState) { Write-Atomic $taskStateFile $taskOldState }
                elseif ([IO.File]::Exists($taskStateFile)) { [IO.File]::Delete($taskStateFile) }
                if ((Get-Digest $taskChain) -eq $taskCurrent -and $null -ne $taskCurrent) { [IO.File]::Delete($taskChain) }
                throw
            }
        }
        Write-Host 'AI 모드 적용 완료. 평소 Steam/런처로 실행해 주세요.'
    }
    Write-Host '원본 EXE·세이브·기존 안개 모드는 유지됩니다.'
    exit 0
} catch {
    Write-Host ('실패: ' + $_.Exception.Message) -ForegroundColor Red
    if ($_.Exception -is [UnauthorizedAccessException] -or $_.Exception -is [Security.SecurityException]) {
        Write-Host '파일 접근 권한 오류입니다. CMD 파일을 우클릭하여 관리자 권한으로 실행해 주세요.'
    }
    exit 1
}
