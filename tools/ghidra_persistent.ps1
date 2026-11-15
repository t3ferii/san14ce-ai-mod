param(
    [ValidateSet('Initialize','Verify','Export','FullExport','Status','RefExport')][string]$Action='Status',
    [string[]]$Address=@(),
    [string[]]$Shards=@(),
    [ValidateSet('1fbe6be7','1a4a447d')][string]$Build='1a4a447d',
    [string]$ProjectRoot='',
    [string]$GhidraHome='C:\Users\<user>\.codex\visualizations\2026\10\03\01a100a1-9d57-7913-a999-9ffece87e071\recovery_tools\ghidra_12.1.4_PUBLIC',
    [string]$JavaHome='C:\Users\<user>\AppData\Local\Programs\Eclipse Adoptium\jdk-21.0.12.101-hotspot',
    [ValidatePattern('^\d+[GM]$')][string]$Heap='4G',
    [ValidateRange(1,16)][int]$MaxCpu=4,
    [string]$RefRoot=(Join-Path $env:USERPROFILE 'Documents\Codex\analysis\SAN14_refs'),
    [string]$Only='',
    [ValidateRange(0,16)][int]$Parallel=0,
    [switch]$Force
)
$ErrorActionPreference='Stop'
# -File 실행 때 '86E140,92B0A0'이 문자열 하나로 들어오므로 쉼표로 나눈다(2026-11-05).
$Address=@($Address|ForEach-Object{$_ -split ','}|ForEach-Object{$_.Trim()}|Where-Object{$_})
$Shards=@($Shards|ForEach-Object{$_ -split ','}|ForEach-Object{$_.Trim()}|Where-Object{$_})
$taskProject=Split-Path -Parent $PSScriptRoot
# Build table (2026-11-05): source EXE, SHA-256, project folder, FullExport output folder. Default = current reference build; historical builds require an explicit -Build (W7, 2026-11-07).
$taskBuilds=@{
    '1fbe6be7'=@{source='backup\w7_retired_20261107\editor83_plain\game.exe.unpacked.exe';hash='1fbe6be7466a66466fe97cf3ab7d49392913346879c7d40de7f4eaf86eef81c6';folder='1fbe6be7466a6646'}
    '1a4a447d'=@{source='build\ui_native_analysis\editor83_b25953386\game.exe.unpacked.exe';hash='1a4a447d14e68c40ad2a5107c29c7341c10eee8fc297a7b35032900c341dbcaa';folder='1a4a447d14e68c40'}
}
$taskBuild=$taskBuilds[$Build]
if(-not $PSBoundParameters.ContainsKey('ProjectRoot')){$ProjectRoot=Join-Path $env:USERPROFILE ('Documents\Codex\analysis\SAN14CE_KO\'+$taskBuild.folder)}
$ProjectRoot=[IO.Path]::GetFullPath($ProjectRoot)
$taskSource=Join-Path $taskProject $taskBuild.source
$taskHash=$taskBuild.hash
$taskName='SAN14CE_KO'
$taskGpr=Join-Path $ProjectRoot ($taskName+'.gpr')
$taskRep=Join-Path $ProjectRoot ($taskName+'.rep')
$taskIndex=Join-Path $ProjectRoot 'index'
$taskProof=Join-Path $ProjectRoot 'manifest.json'
$taskHeadless=Join-Path $GhidraHome 'support\analyzeHeadless.bat'
$taskScripts=Join-Path $PSScriptRoot 'ghidra_scripts'
if($Action -eq 'Status') {
    if(Test-Path -LiteralPath $taskProof){Get-Content -LiteralPath $taskProof -Raw}
    else{[pscustomobject]@{status='not_initialized';project=$taskGpr}|ConvertTo-Json}
    return
}
if($Action -eq 'Initialize' -and ((Test-Path -LiteralPath $taskGpr) -or (Test-Path -LiteralPath $taskRep))){throw 'Project exists; use Verify or Export. Existing database will not be overwritten.'}
# ---- RefExport: reference-mod DLLs -> separate persistent project + research/ghidra_refs (2026-11-05) ----
if($Action -eq 'RefExport') {
    $ErrorActionPreference='Continue'
    $rxGame=Split-Path -Parent $taskProject
    $rxOut=Join-Path $taskProject 'research\ghidra_refs'
    $rxName='SAN14_refs'
    $rxScripts=Join-Path $taskScripts 'San14RefExport.java'
    $rxFuncs=Join-Path $taskProject 'research\ghidra_full\1fbe6be7466a6646\functions.tsv'
    if($RefRoot -match '[()!%&^]'){throw 'RefRoot must not contain cmd metacharacters'}
    if(-not (Test-Path -LiteralPath $taskHeadless)){throw 'Ghidra headless launcher missing'}
    [void][IO.Directory]::CreateDirectory($RefRoot);[void][IO.Directory]::CreateDirectory($rxOut)
    $rxUtf8=New-Object Text.UTF8Encoding($false)
    $rxLock=[IO.File]::Open((Join-Path $RefRoot 'san14-job.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    $rxSavedJava=$env:JAVA_HOME;$rxSavedHeap=$env:GHIDRA_HEADLESS_MAXMEM
    try {
        $env:JAVA_HOME=$JavaHome;$env:GHIDRA_HEADLESS_MAXMEM=$Heap
        $rxVer=(Get-FileHash -LiteralPath $rxScripts -Algorithm SHA256).Hash.Substring(0,12).ToLowerInvariant()
        $rxStaged=Join-Path $RefRoot ('script_versions\'+$rxVer);[void][IO.Directory]::CreateDirectory($rxStaged)
        Copy-Item -LiteralPath $rxScripts -Destination (Join-Path $rxStaged 'San14RefExport.java') -Force
        $rxIn=Join-Path $RefRoot 'input';[void][IO.Directory]::CreateDirectory($rxIn)
        $rxFuncCopy=Join-Path $rxIn 'orig_functions.tsv';Copy-Item -LiteralPath $rxFuncs -Destination $rxFuncCopy -Force
        # Post-patch reference packs (refermod_*_YYMMDD) target Steam buildid 25953386 (1a4a447d); 2026-11-05.
        $rxFuncsNew=Join-Path $taskProject 'research\ghidra_full\1a4a447d14e68c40\functions.tsv'
        $rxFuncCopyNew=Join-Path $rxIn 'orig_functions_1a4a447d.tsv';if(Test-Path -LiteralPath $rxFuncsNew){Copy-Item -LiteralPath $rxFuncsNew -Destination $rxFuncCopyNew -Force}
        # target discovery (analysis folder only; no runtimes / installers)
        $rxFiles=@()
        foreach($rxDir in @('분석용\refermod_1\support\payload','분석용\refermod_2\packs','분석용\refermod_2_261105','분석용\refermod_2_261108')) {
            $rxFiles+=Get-ChildItem -LiteralPath (Join-Path $rxGame $rxDir) -Recurse -File -Filter *.dll -ErrorAction SilentlyContinue
        }
        $rxFiles=$rxFiles|Where-Object{$_.FullName -notmatch 'San14BgmExpansion'}
        $rxSeen=@{};$rxJobs=@()
        foreach($f in ($rxFiles|Sort-Object Length,FullName)) {
            $h=(Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            if($rxSeen.ContainsKey($h)){continue};$rxSeen[$h]=$f.FullName
            $rel=$f.FullName.Substring($rxGame.Length+1).Replace('\','/')
            $gb=if($rel -match 'refermod_\d+_\d{6}/'){'1a4a447d'}else{'1fbe6be7'}
            $rxJobs+=[pscustomobject]@{file=$f;sha=$h;rel=$rel;folder=($f.BaseName+'_'+$h.Substring(0,16));game_build=$gb}
        }
        if($Only){$rxJobs=@($rxJobs|Where-Object{$_.folder -like "*$Only*" -or $_.rel -like "*$Only*"})}
        function Write-RefIndex {
            $lines=@("mod`tsource`tsha256`tfolder`tfunctions`tstatus`tseconds`tnote")
            foreach($d in Get-ChildItem -LiteralPath $rxOut -Directory|Sort-Object Name) {
                $mf2=Join-Path $d.FullName 'manifest.json';if(-not(Test-Path -LiteralPath $mf2)){continue}
                $m2=[IO.File]::ReadAllText($mf2)|ConvertFrom-Json
                $fn=if($m2.counts -and $m2.counts.'functions.tsv'){$m2.counts.'functions.tsv'}else{''}
                $lines+=(@(($d.Name -replace '_[0-9a-f]{16}$',''),"$($m2.source)","$($m2.sha256)",$d.Name,"$fn","$($m2.status)","$($m2.seconds)","$($m2.note)") -join "`t")
            }
            [IO.File]::WriteAllText((Join-Path $rxOut 'index.tsv'),(($lines -join "`n")+"`n"),$rxUtf8)
        }
        # ---- parallel scheduling (2026-11-05): one Ghidra project / lock / log / stage folder per DLL ----
        $rxTodo=@()
        foreach($j in $rxJobs) {
            $mf=Join-Path (Join-Path $rxOut $j.folder) 'manifest.json'
            if((Test-Path -LiteralPath $mf) -and -not $Force) {
                if(([IO.File]::ReadAllText($mf)|ConvertFrom-Json).status -eq 'complete'){Write-Output ('skip complete: '+$j.folder);continue}
            }
            $rxTodo+=$j
        }
        $rxTodo=@($rxTodo|Sort-Object @{Expression={$_.file.Length};Descending=$true},@{Expression={$_.folder}})
        $rxCores=[Environment]::ProcessorCount
        $rxFreeGB=(Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1MB
        $rxN=if($Parallel -gt 0){$Parallel}else{[int][math]::Max(1,[math]::Min($rxTodo.Count,[math]::Min([math]::Floor($rxCores/2),[math]::Floor($rxFreeGB/3))))}
        if($rxTodo.Count -gt 0 -and $rxN -gt $rxTodo.Count){$rxN=$rxTodo.Count}
        if($rxN -lt 1){$rxN=1}
        $rxCpu=if($PSBoundParameters.ContainsKey('MaxCpu')){$MaxCpu}else{[int][math]::Max(1,[math]::Floor($rxCores/$rxN))}
        if($PSBoundParameters.ContainsKey('Heap')){$rxHeap=$Heap}else{$rxHeap=''+[int][math]::Min(6144,[math]::Max(2048,[math]::Floor($rxFreeGB*0.75*1024/$rxN)))+'M'}
        $env:GHIDRA_HEADLESS_MAXMEM=$rxHeap
        Write-Output ("RefExport plan: jobs=$($rxTodo.Count) parallel=$rxN max-cpu/job=$rxCpu heap/job=$rxHeap cores=$rxCores freeGB=$([math]::Round($rxFreeGB,1))")
        $rxWall=[DateTime]::UtcNow
        $rxRunning=@();$rxQueue=New-Object 'System.Collections.Generic.Queue[object]';foreach($j in $rxTodo){$rxQueue.Enqueue($j)}
        function Complete-RefJob($r) {
            $j=$r.job;$dest=Join-Path $rxOut $j.folder;$mf=Join-Path $dest 'manifest.json'
            $null=$r.proc.Handle;$r.proc.WaitForExit();$code=$r.proc.ExitCode
            $text=''
            foreach($lf in @($r.log,($r.log+'.err'))){if(Test-Path -LiteralPath $lf){$text+=[IO.File]::ReadAllText($lf)+"`n"}}
            $secs=([DateTime]::UtcNow-$r.t0).TotalSeconds
            [void][IO.Directory]::CreateDirectory($dest)
            $ok=(($null -eq $code) -or ($code -eq 0)) -and ($text -match 'SAN14_REF_EXPORT_FINISHED') -and ($text -notmatch 'SCRIPT ERROR|Error running script') -and (Test-Path -LiteralPath (Join-Path $r.stage 'manifest.json'))
            if($ok) {
                & robocopy $r.stage $dest /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
                $m=[IO.File]::ReadAllText($mf)|ConvertFrom-Json
                $m|Add-Member -Force NoteProperty source $j.rel
                $m|Add-Member -Force NoteProperty game_build $j.game_build
                $m|Add-Member -Force NoteProperty seconds ([math]::Round($secs,1))
                if($text -match '(?i)analysis timed out'){$m|Add-Member -Force NoteProperty note 'analysis_timeout'}
                [IO.File]::WriteAllText($mf,($m|ConvertTo-Json -Depth 6),$rxUtf8)
            } else {
                $why=(($text -split "`n")|Where-Object{$_ -match 'ERROR|Exception|SCRIPT'}|Select-Object -First 3) -join ' | '
                $m=[ordered]@{status='failed';sha256=$j.sha;source=$j.rel;seconds=[math]::Round($secs,1);note="exit=$code $why";log=$r.log}
                [IO.File]::WriteAllText($mf,($m|ConvertTo-Json -Depth 4),$rxUtf8)
            }
            $r.lock.Dispose()
            Write-Output ('  -> '+$j.folder+' '+$(if($ok){'complete'}else{'FAILED'})+' '+[math]::Round($secs,1)+'s')
        }
        while($rxQueue.Count -gt 0 -or $rxRunning.Count -gt 0) {
            $still=@()
            foreach($r in $rxRunning){if($r.proc.HasExited){Complete-RefJob $r}else{$still+=$r}}
            $rxRunning=$still
            if($rxQueue.Count -gt 0 -and $rxRunning.Count -lt $rxN) {
                $j=$rxQueue.Dequeue()
                $pdir=Join-Path $RefRoot ('jobs\'+$j.folder)
                if(Test-Path -LiteralPath $pdir){Remove-Item -LiteralPath $pdir -Recurse -Force}
                [void][IO.Directory]::CreateDirectory($pdir)
                $jl=[IO.File]::Open((Join-Path $pdir 'san14-job.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
                $stage=Join-Path $RefRoot ('out\'+$j.folder)
                if(Test-Path -LiteralPath $stage){Remove-Item -LiteralPath $stage -Recurse -Force}
                $inFile=Join-Path $rxIn ($j.folder+'.dll');Copy-Item -LiteralPath $j.file.FullName -Destination $inFile -Force
                if((Get-FileHash -LiteralPath $inFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $j.sha){throw 'staged hash mismatch'}
                # own script folder per job so concurrent Ghidra script compilation cannot collide
                $jScripts=Join-Path $pdir 'scripts';[void][IO.Directory]::CreateDirectory($jScripts)
                Copy-Item -LiteralPath $rxScripts -Destination (Join-Path $jScripts 'San14RefExport.java') -Force
                $stamp=[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ')
                $log=Join-Path $pdir ('refexport_'+$j.folder+'_'+$stamp+'.log')
                $funcArg=if($j.game_build -eq '1a4a447d'){$rxFuncCopyNew}else{$rxFuncCopy}
                $a=@($pdir,$j.folder,'-max-cpu',"$rxCpu",'-log',(Join-Path $pdir ($stamp+'_ghidra.log')),'-scriptlog',(Join-Path $pdir ($stamp+'_scripts.log')),
                    '-scriptPath',$jScripts,'-import',$inFile,'-overwrite','-analysisTimeoutPerFile','5400',
                    '-postScript','San14RefExport.java',$stage,$j.sha,$funcArg,'ref')
                $argStr=($a|ForEach-Object{if("$_" -match '[\s"]'){'"'+$_+'"'}else{"$_"}}) -join ' '
                Write-Output ('RefExport start '+$j.folder+' ('+$j.file.Length+' bytes) running='+($rxRunning.Count+1)+'/'+$rxN)
                $p=Start-Process -FilePath $taskHeadless -ArgumentList $argStr -NoNewWindow -PassThru -RedirectStandardOutput $log -RedirectStandardError ($log+'.err')
                $null=$p.Handle  # cache the process handle now so ExitCode stays readable
                $rxRunning+=[pscustomobject]@{job=$j;proc=$p;t0=[DateTime]::UtcNow;log=$log;stage=$stage;lock=$jl}
                Start-Sleep -Seconds 8
            } else { Start-Sleep -Seconds 5 }
        }
        Write-RefIndex
        Write-Output ('REF_EXPORT_WALL_SECONDS '+[math]::Round(([DateTime]::UtcNow-$rxWall).TotalSeconds,1))
        Write-Output 'REF_EXPORT_DONE'
    } finally {
        $env:JAVA_HOME=$rxSavedJava;$env:GHIDRA_HEADLESS_MAXMEM=$rxSavedHeap;$rxLock.Dispose()
    }
    return
}
if((Get-FileHash -LiteralPath $taskSource -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskHash){throw 'Analysis source SHA-256 mismatch'}
if(-not (Test-Path -LiteralPath $taskHeadless)){throw 'Ghidra headless launcher missing'}
if(-not (Test-Path -LiteralPath (Join-Path $JavaHome 'bin\java.exe'))){throw 'Configured Java missing'}
if($Action -ne 'Initialize' -and -not (Test-Path -LiteralPath $taskGpr)){throw 'Initialize the project first'}
if($Action -eq 'Export' -and $Address.Count -eq 0){throw 'Export requires -Address RVA[,RVA...]'}
foreach($taskAddress in $Address){if($taskAddress -notmatch '^(0x)?[0-9a-fA-F]{1,16}$'){throw 'Address must be hexadecimal'}}
if($ProjectRoot -match '[()!%&^]'){throw 'Use a project path without cmd.exe metacharacters'}
[void][IO.Directory]::CreateDirectory($ProjectRoot)
$taskLock=[IO.File]::Open((Join-Path $ProjectRoot 'san14-job.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
$taskUtf8=New-Object Text.UTF8Encoding($false)
$taskSavedJava=$env:JAVA_HOME;$taskSavedHeap=$env:GHIDRA_HEADLESS_MAXMEM
$taskStarted=[DateTime]::UtcNow
$taskStamp=$taskStarted.ToString('yyyyMMddTHHmmssfffZ')
$taskLog=Join-Path $ProjectRoot ($Action.ToLowerInvariant()+'_'+$taskStamp+'.log')
try {
    $env:JAVA_HOME=$JavaHome;$env:GHIDRA_HEADLESS_MAXMEM=$Heap
    # The shipped Windows batch launcher mishandles parentheses in arguments.
    # Stage a hash-checked analysis copy and scripts outside Program Files (x86).
    # Ghidra can reuse stale compiled classes when copied source timestamps are
    # older than its compiled bundle. Give every source revision its own folder.
    $taskScriptNames=@('San14AnalysisReport.java','San14ExportFunctions.java')
    if($Action -eq 'FullExport') {$taskScriptNames+= 'San14FullExport.java'}
    $taskScriptVersion=-join @($taskScriptNames|ForEach-Object {(Get-FileHash -LiteralPath (Join-Path $taskScripts $_) -Algorithm SHA256).Hash.Substring(0,12).ToLowerInvariant()})
    $taskStagedScripts=Join-Path $ProjectRoot ('script_versions\'+$taskScriptVersion)
    $taskInputDirectory=Join-Path $ProjectRoot 'input'
    [void][IO.Directory]::CreateDirectory($taskStagedScripts)
    [void][IO.Directory]::CreateDirectory($taskInputDirectory)
    foreach($taskScript in $taskScriptNames) {
        $taskStagedScript=Join-Path $taskStagedScripts $taskScript
        if(-not(Test-Path -LiteralPath $taskStagedScript)) {
            Copy-Item -LiteralPath (Join-Path $taskScripts $taskScript) -Destination $taskStagedScript
        }
        if((Get-FileHash -LiteralPath $taskStagedScript).Hash -ne (Get-FileHash -LiteralPath (Join-Path $taskScripts $taskScript)).Hash){throw 'Staged script fingerprint conflict'}
    }
    $taskStagedSource=Join-Path $taskInputDirectory 'game.exe.unpacked.exe'
    if(-not(Test-Path -LiteralPath $taskStagedSource)) {Copy-Item -LiteralPath $taskSource -Destination $taskStagedSource}
    if((Get-FileHash -LiteralPath $taskStagedSource -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskHash){throw 'Staged input SHA-256 mismatch'}
    $taskArgs=@($ProjectRoot,$taskName,'-max-cpu',"$MaxCpu",'-log',(Join-Path $ProjectRoot ($taskStamp+'_ghidra.log')),
        '-scriptlog',(Join-Path $ProjectRoot ($taskStamp+'_scripts.log')),'-scriptPath',$taskStagedScripts)
    if($Action -eq 'Initialize') {
        $taskState=[ordered]@{status='analyzing';source=$taskSource;sha256=$taskHash;project=$taskGpr;program='game.exe.unpacked.exe';
            started_utc=$taskStarted.ToString('o');heap=$Heap;max_cpu=$MaxCpu;log=$taskLog;analysis_complete=$false;reopen_verified=$false}
        [IO.File]::WriteAllText($taskProof,($taskState|ConvertTo-Json -Depth 6),$taskUtf8)
        # Deliberately no -readOnly or -deleteProject: the analysis must be saved.
        $taskArgs+=@('-import',$taskStagedSource,'-postScript','San14AnalysisReport.java',$taskIndex,$taskHash)
    } else {
        $taskState=Get-Content -LiteralPath $taskProof -Raw|ConvertFrom-Json
        if($taskState.sha256 -ne $taskHash -or -not $taskState.analysis_complete){throw 'Saved analysis is not verified complete'}
        $taskArgs+=@('-process','game.exe.unpacked.exe','-noanalysis','-readOnly')
        if($Action -eq 'Verify') {
            $taskOut=Join-Path $ProjectRoot ('verification\'+$taskStamp)
            $taskArgs+=@('-postScript','San14ExportFunctions.java',$taskOut,$taskHash,'85FCE0','92E430')
        } elseif($Action -eq 'FullExport') {
            foreach($shard in $Shards) {if($shard -notmatch '^[0-9a-fA-F]{4}$'){throw 'Shard must be four hexadecimal digits'}}
            $taskOut='AI_TacticsMod/research/ghidra_full/'+$taskBuild.folder
            $taskArgs+=@('-postScript','San14FullExport.java',$taskOut,$taskHash)+$Shards
        } else {
            $taskOut=Join-Path $ProjectRoot ('exports\'+$taskStamp)
            $taskArgs+=@('-postScript','San14ExportFunctions.java',$taskOut,$taskHash)+$Address
        }
    }
    Write-Output ('Ghidra '+$Action+': '+$taskGpr)
    & $taskHeadless @taskArgs 2>&1 | ForEach-Object {
        $taskLine="$_";[IO.File]::AppendAllText($taskLog,$taskLine+[Environment]::NewLine,$taskUtf8);Write-Output $taskLine
    }
    if($LASTEXITCODE -ne 0){throw "Ghidra exited $LASTEXITCODE; see $taskLog"}
    if((Get-FileHash -LiteralPath $taskSource -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskHash){throw 'Input changed during analysis'}
    $taskLogText=[IO.File]::ReadAllText($taskLog)
    if($taskLogText -match 'SCRIPT ERROR|Error running script|ERROR REPORT:|Exception in thread'){throw "Ghidra reported an error; see $taskLog"}
    if($Action -eq 'Initialize') {
        if(-not(Test-Path -LiteralPath $taskGpr) -or -not(Test-Path -LiteralPath $taskRep)){throw 'Saved project missing'}
        $taskReport=Get-Content -LiteralPath (Join-Path $taskIndex 'analysis_report.json') -Raw|ConvertFrom-Json
        if($taskReport.sha256 -ne $taskHash -or $taskReport.analysis_timeout -or $taskReport.functions -lt 1000){throw 'Analysis report invalid'}
        if($taskLogText -notmatch 'SAN14_INDEX_EXPORTED' -or $taskLogText -notmatch 'Save succeeded'){throw 'Analysis/export/save success not all confirmed'}
        $taskState.status='saved';$taskState.analysis_complete=$true
        $taskState['functions']=$taskReport.functions;$taskState['instructions']=$taskReport.instructions;$taskState['strings']=$taskReport.strings
        $taskState['ghidra_version']=$taskReport.ghidra_version;$taskState['image_base']=$taskReport.image_base
        $taskState['saved_utc']=[DateTime]::UtcNow.ToString('o');$taskState['analysis_seconds']=([DateTime]::UtcNow-$taskStarted).TotalSeconds
    } else {
        if($Action -eq 'Verify') {
            if($taskLogText -notmatch 'SAN14_CODE_BYTES_VERIFIED'){throw 'Saved-program byte verification missing'}
            foreach($taskFile in @('0085FCE0_c.txt','0085FCE0_asm.txt','0092E430_c.txt','0092E430_asm.txt')) {
                if(-not(Test-Path -LiteralPath (Join-Path $taskOut $taskFile))){throw 'Reopen sample export missing'}
            }
            $taskState.status='saved_and_reopened';$taskState.reopen_verified=$true
            $taskState|Add-Member -Force NoteProperty verification_log $taskLog
            $taskState|Add-Member -Force NoteProperty verification_directory $taskOut
            $taskState|Add-Member -Force NoteProperty reopen_seconds ([DateTime]::UtcNow-$taskStarted).TotalSeconds
        } elseif($Action -eq 'FullExport') {
            if($taskLogText -notmatch 'SAN14_FULL_EXPORT_FINISHED'){throw 'Full export did not finish'}
        } else {
            $taskCache=Join-Path $taskProject ('research\decomp_cache\'+$taskBuild.folder)
            [void][IO.Directory]::CreateDirectory($taskCache)
            $taskAdded=0
            foreach($taskExport in Get-ChildItem -LiteralPath $taskOut -Filter '*.txt' -File) {
                if($taskExport.Name -notmatch '^[0-9A-F]{8}_(c|asm)\.txt$'){throw 'Unexpected export filename'}
                if([IO.File]::ReadAllText($taskExport.FullName) -notmatch ('digest='+$taskHash)){throw 'Export identity missing'}
                $taskDestination=Join-Path $taskCache $taskExport.Name
                if(Test-Path -LiteralPath $taskDestination){continue}
                $taskBytes=[IO.File]::ReadAllBytes($taskExport.FullName)
                $taskStream=[IO.File]::Open($taskDestination,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
                try{$taskStream.Write($taskBytes,0,$taskBytes.Length)}finally{$taskStream.Dispose()}
                $taskAdded++
            }
            $taskState|Add-Member -Force NoteProperty last_export_directory $taskOut
            $taskState|Add-Member -Force NoteProperty last_cache_added $taskAdded
        }
    }
    if($Action -ne 'FullExport') {[IO.File]::WriteAllText($taskProof,($taskState|ConvertTo-Json -Depth 6),$taskUtf8)}
    $taskState|ConvertTo-Json -Depth 6
} catch {
    if($Action -eq 'Initialize' -and $null -ne $taskState) {
        $taskState.status='failed';$taskState['error']="$($_.Exception.Message)"
        [IO.File]::WriteAllText($taskProof,($taskState|ConvertTo-Json -Depth 6),$taskUtf8)
    }
    throw
} finally {
    $env:JAVA_HOME=$taskSavedJava;$env:GHIDRA_HEADLESS_MAXMEM=$taskSavedHeap;$taskLock.Dispose()
}
