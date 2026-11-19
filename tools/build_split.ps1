param([switch]$Fixture,[switch]$RuntimeDiagnostics,[switch]$RuntimeCosts,[switch]$Both,[switch]$NoCache,[int]$Jobs=0)
# 2026-11-16 user (검사·설치를 빨리): every compile of a run goes into one parallel pool (own obj/PDB/dependency folder per
# target, below-normal priority) and is skipped when tools/cl_cache.py proves its input unchanged (-NoCache rebuilds all).
# -Both builds the diagnostic variant (installed by tools/install_local.py) and the release variant (+ loader fixtures) in
# one pool. Flags, defines, link inputs and outputs are those of the former serial script (proved with tools/feature_fingerprint.py
# compare --same-code before the units existed). 2026-11-16 multi-unit refactor: SAN14_MULTI_TU compiles native_adapter.cpp and
# each src/tu_<root>_<unit>.cpp as its own cached job and links them in a separate step; equivalence with the unity build is
# tools/unit_equivalence.py plus the fingerprint compare (all but code bytes). Pool size: -Jobs, else 3 while the game runs and 8 otherwise.
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
if($RuntimeCosts){$RuntimeDiagnostics=$true}
if($Both -and ($Fixture -or $RuntimeDiagnostics)){throw '-Both builds the diagnostic and the release variant; combine it with no other variant switch.'}
if($RuntimeDiagnostics -and $Fixture){throw 'Choose fixtures or runtime diagnostics.'}
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10';$taskVersion='10.0.26100.0'
$taskFlags=@('/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc','/DMERIT_CANDIDATE','/DRECRUITMENT_UI_CANDIDATE','/DHIDDEN_LOYALTY','/DINJURY_PROTECTION','/DAWARD_WAREHOUSE','/DSORTIE_FORMATION','/DLAYOUT_OWNS_PRESENT')   # 2026-11-05 user exception: Present/WndProc owned by SAN14_Layout.dll
# W5-3 (docs/MODULARIZATION_PLAN.md): in every DLL build an unreferenced local function (C4505) is an error. The split fixture build
# (SPLIT_MODULE_FIXTURE, flags also read by tools/run_checks.py) keeps /wd4505: it compiles fixture-only seams the production roots do not use.
$taskPlain=@('/nologo','/W4','/WX','/utf-8','/O2','/MT','/std:c++17','/EHsc')   # loader and its fixtures
$taskMock=@('/nologo','/W4','/WX','/O2','/MT')
$taskLayoutFixtureFlags=@('/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc')
$taskCl=Join-Path $taskVc 'bin\Hostx64\x64\cl.exe'
$taskSrc=Join-Path $taskProject 'src';$taskTests=Join-Path $taskProject 'tests'
$taskPython=(Get-Command python).Source

function Get-TaskVariant([string]$kind) {   # release | diag | costs | fixture -> @{Dir; Jobs}
    $rel=@{release='build\development\loader_split';fixture='build\development\loader_split';diag='build\development\loader_split_diagnostic';costs='build\development\loader_split_cost_profile'}[$kind]
    $dir=Join-Path $taskProject $rel;[void][IO.Directory]::CreateDirectory($dir)
    $f=@($taskFlags)
    if($kind -ne 'fixture'){$f=@($f | ForEach-Object {if($_ -eq '/wd4505'){'/we4505'}else{$_}})}
    $link=@()
    if($kind -in 'diag','costs'){$f+=@('/Zi','/DRUNTIME_COST_PROFILE');$link=@('/DEBUG:FULL','/OPT:REF','/OPT:ICF','/MAP')}
    if($kind -eq 'costs'){$f+=@('/DRUNTIME_COST_PROFILE','/DNO_RUNTIME_LOG')}
    if($kind -eq 'fixture'){$f+='/DSPLIT_MODULE_FIXTURE'} elseif($kind -eq 'release'){$f+='/DNO_RUNTIME_LOG'}
    $j=New-Object System.Collections.ArrayList
    function Add-Job($out,$flags,$pre,$srcs,$libs,[switch]$Dll) {
        [void]$j.Add(@{Out=$out;Dir=$dir;Kind=$kind;Flags=@($flags)+@($pre);Dll=[bool]$Dll;Src=@($srcs);Link=@($libs)})
    }
    $base=if($kind -eq 'fixture'){'SplitBaseFixture.dll'}else{'SAN14_Convenience.dll'}
    $combat=if($kind -eq 'fixture'){'SplitCombatFixture.dll'}else{'SAN14_CombatAI.dll'}
    # 2026-11-16 refactor (docs/REFACTOR_EQUIVALENCE.md section 4): each root DLL = native_adapter.cpp + its generated unit sources
    # (src/tu_<root>_<unit>.cpp, tools/gen_module_table.py), compiled with SAN14_MULTI_TU, one pool item per source. The split fixture DLLs
    # build the same way, so the split-module fixture exercises the multi-unit DLL.
    $baseUnits=@(Get-ChildItem -LiteralPath $taskSrc -Filter 'tu_convenience_*.cpp' | Sort-Object Name | ForEach-Object {$_.FullName})
    $combatUnits=@(Get-ChildItem -LiteralPath $taskSrc -Filter 'tu_combat_*.cpp' | Sort-Object Name | ForEach-Object {$_.FullName})
    $baseSrc=@((Join-Path $taskSrc 'native_adapter.cpp'))+$baseUnits;if($kind -ne 'fixture'){$baseSrc+=(Join-Path $taskSrc 'gpu_profile.cpp')}
    Add-Job $base $f @("/I$dir",'/DSPLIT_BASE','/DSAN14_MULTI_TU') $baseSrc (@($link)+@('kernel32.lib','user32.lib','gdi32.lib','bcrypt.lib','dxguid.lib')) -Dll
    Add-Job $combat $f @('/DSPLIT_COMBAT','/DSAN14_MULTI_TU') (@((Join-Path $taskSrc 'native_adapter.cpp'))+$combatUnits) (@($link)+@('kernel32.lib','user32.lib')) -Dll
    if($kind -ne 'fixture') {
        # 2026-11-13 user: other authors' patches = SAN14_ModPack.dll (4계절 규칙 first), loaded by the convenience module
        Add-Job 'SAN14_ModPack.dll' $f @("/I$taskSrc") @((Join-Path $taskSrc 'modpack_dll.cpp'),(Join-Path $taskSrc 'season\season_engine.c')) (@($link)+@('kernel32.lib','user32.lib')) -Dll
        # 2026-11-01 user: 조조전 탭 = SAN14_CCZ.dll, loaded by the convenience module
        Add-Job 'SAN14_CCZ.dll' $f @("/I$taskSrc") @((Join-Path $taskSrc 'ccz_module.cpp')) (@($link)+@('kernel32.lib','user32.lib','gdi32.lib')) -Dll
        # 2026-11-05 user exception: SAN14_Layout.dll = hook manager + Present/WndProc owner, loaded first by dinput8
        Add-Job 'SAN14_Layout.dll' $f @("/I$taskSrc") @((Join-Path $taskSrc 'layout_main.cpp')) (@($link)+@('kernel32.lib','user32.lib')) -Dll
        Add-Job 'dinput8.dll' $taskPlain @() @((Join-Path $taskSrc 'module_loader.cpp'),(Join-Path $taskSrc 'input_proxy.cpp')) @('kernel32.lib') -Dll
    }
    if($kind -eq 'release') {
        Add-Job 'modpack_fixture.exe' $taskPlain @("/I$dir","/I$taskSrc") @((Join-Path $taskTests 'modpack_fixture.cpp')) @('kernel32.lib','user32.lib','bcrypt.lib')
        Add-Job 'loader_fixture.exe' $taskPlain @() @((Join-Path $taskTests 'pure_loader_fixture.cpp')) @('kernel32.lib','dxguid.lib')
        Add-Job 'LoaderMockCombat.dll' $taskMock @('/DMOCK_COMBAT') @((Join-Path $taskTests 'loader_mock.cpp')) @('kernel32.lib') -Dll
        Add-Job 'LoaderMockBadCombat.dll' $taskMock @('/DMOCK_COMBAT','/DMOCK_BAD_ABI') @((Join-Path $taskTests 'loader_mock.cpp')) @('kernel32.lib') -Dll
        Add-Job 'LoaderMockConvenience.dll' $taskMock @() @((Join-Path $taskTests 'loader_mock.cpp')) @('kernel32.lib') -Dll
        # 2026-11-05 user exception: Layout loads first, Layout publishes last; Previous-input loop prevention.
        Add-Job 'LayoutMock.dll' $taskMock @('/DMOCK_LAYOUT') @((Join-Path $taskTests 'layout_loader_mock.cpp')) @('kernel32.lib') -Dll
        Add-Job 'LayoutMockFail.dll' $taskMock @('/DMOCK_LAYOUT_FAIL') @((Join-Path $taskTests 'layout_loader_mock.cpp')) @('kernel32.lib') -Dll
        Add-Job 'LayoutMockConvenience.dll' $taskMock @() @((Join-Path $taskTests 'layout_loader_mock.cpp')) @('kernel32.lib') -Dll
    }
    if($kind -eq 'fixture') {
        Add-Job 'split_module_fixture.exe' $f @() @((Join-Path $taskTests 'split_module_fixture.cpp')) @('kernel32.lib','user32.lib')
        foreach($n in 'layout_fixture','layout_p3_fixture','layout_p4_fixture'){Add-Job "$n.exe" $taskLayoutFixtureFlags @() @((Join-Path $taskTests "$n.cpp")) @('kernel32.lib','user32.lib')}
    }
    return @{Dir=$dir;Jobs=$j}
}

function ConvertTo-TaskArg([string]$a) {   # Windows command-line quoting (MSVCRT rules: backslashes before a quote or the closing quote doubled)
    if($a -ne '' -and $a -notmatch '[\s"]'){return $a}
    $b=[regex]::Replace($a,'(\\*)"','$1$1\"');$b=[regex]::Replace($b,'(\\+)$','$1$1')
    return '"'+$b+'"'
}

function Invoke-TaskPool($items,[int]$max) {   # items: hashtables with File, Args, Cwd and optional After (items that must end with Code 0 first); sets Code, Output, Secs
    $queue=New-Object System.Collections.ArrayList;foreach($i in $items){if(-not $i.ContainsKey('Code')){$i.Code=$null};[void]$queue.Add($i)}
    $run=New-Object System.Collections.ArrayList
    while($queue.Count -or $run.Count) {
        foreach($i in @($queue)) {   # a prerequisite failed: skipped, reported as failed
            $pre=if($i.After){@($i.After)}else{@()}
            if(@($pre | Where-Object {$null -ne $_.Code -and $_.Code -ne 0}).Count){$i.Code=-1;$i.Output='skipped: a prerequisite failed';$i.Secs=0;$queue.Remove($i)}
        }
        while($run.Count -lt $max) {
            $i=$null
            foreach($q in $queue){$pre=if($q.After){@($q.After)}else{@()};if(-not @($pre | Where-Object {$_.Code -ne 0}).Count){$i=$q;break}}
            if($null -eq $i){break}
            $queue.Remove($i)
            $psi=New-Object Diagnostics.ProcessStartInfo
            $psi.FileName=$i.File;$psi.Arguments=(@($i.Args | ForEach-Object {ConvertTo-TaskArg ([string]$_)}) -join ' ')
            $psi.WorkingDirectory=$i.Cwd;$psi.UseShellExecute=$false;$psi.CreateNoWindow=$true
            $psi.RedirectStandardOutput=$true;$psi.RedirectStandardError=$true
            $i.Watch=[Diagnostics.Stopwatch]::StartNew()
            $p=[Diagnostics.Process]::Start($psi)
            try {$p.PriorityClass='BelowNormal'} catch {}
            $i.Proc=$p;$i.OutTask=$p.StandardOutput.ReadToEndAsync();$i.ErrTask=$p.StandardError.ReadToEndAsync()
            [void]$run.Add($i)
        }
        Start-Sleep -Milliseconds 40
        foreach($i in @($run)) {
            if($i.Proc.HasExited) {
                $i.Proc.WaitForExit();$i.Code=$i.Proc.ExitCode;$i.Output=($i.OutTask.Result+$i.ErrTask.Result).TrimEnd()
                $i.Secs=$i.Watch.Elapsed.TotalSeconds;$i.Proc.Dispose();$run.Remove($i)
            }
        }
    }
}

function Write-TaskJson($path,$value) {[IO.File]::WriteAllText($path,($value | ConvertTo-Json -Depth 5 -Compress),(New-Object Text.UTF8Encoding($false)))}

$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
    $taskKinds=if($Both){@('diag','release')}elseif($Fixture){@('fixture')}elseif($RuntimeCosts){@('costs')}elseif($RuntimeDiagnostics){@('diag')}else{@('release')}
    $taskVariants=@{};foreach($k in $taskKinds){$taskVariants[$k]=Get-TaskVariant $k}
    # biggest translation units first (the two native_adapter.cpp roots of every variant), then the rest in script order
    $taskAll=@();foreach($k in $taskKinds){$taskAll+=@($taskVariants[$k].Jobs | Select-Object -First 2)}
    foreach($k in $taskKinds){$taskAll+=@($taskVariants[$k].Jobs | Select-Object -Skip 2)}
    # 2026-11-16 refactor: one compile item per source (own stamp, so an edit recompiles only the units that read the file) and
    # one link item per output: cl as the link driver with the job's own flags, so the linker gets the options of the former
    # single command. A link waits for its compiles and is fresh only when its stamp and all of its compiles are.
    $taskCompiles=New-Object System.Collections.ArrayList;$taskLinks=New-Object System.Collections.ArrayList
    foreach($t in $taskAll) {
        $obj=Join-Path $t.Dir ('obj\'+[IO.Path]::GetFileNameWithoutExtension($t.Out));[void][IO.Directory]::CreateDirectory($obj)
        $mine=@()
        foreach($s in $t.Src) {
            $ss=[IO.Path]::GetFileNameWithoutExtension($s);$deps=Join-Path $obj "deps\$ss";[void][IO.Directory]::CreateDirectory($deps)
            $a=@($t.Flags)+@('/c',"/Fo$obj\$ss.obj",'/sourceDependencies',$deps)
            $outs=@("$obj\$ss.obj")
            if($t.Flags -contains '/Zi'){$a+="/Fd$obj\$ss.pdb";$outs+="$obj\$ss.pdb"}
            $a+=$s
            $c=@{Name=('{0}:{1}' -f $t.Out,$ss);Kind=$t.Kind;File=$taskCl;Args=$a;Cwd=$t.Dir;Deps=$deps;Stamp="$obj\$ss.stamp.json";Src=@($s);Outputs=$outs;Obj="$obj\$ss.obj"}
            $c.Key=(@($taskCl)+$a+@($env:INCLUDE,$env:LIB)) -join "`n"
            $mine+=$c;[void]$taskCompiles.Add($c)
        }
        $a=@($t.Flags);if($t.Dll){$a+='/LD'}
        $a+=@($mine | ForEach-Object {$_.Obj})+@("/Fe$($t.Out)",'/link')+@($t.Link)
        $outs=@((Join-Path $t.Dir $t.Out))
        if($t.Flags -contains '/Zi' -and $t.Dll){$stem=Join-Path $t.Dir ([IO.Path]::GetFileNameWithoutExtension($t.Out));$outs+=@("$stem.pdb","$stem.map")}
        $l=@{Name=$t.Out;Kind=$t.Kind;File=$taskCl;Args=$a;Cwd=$t.Dir;Stamp="$obj\link.json";Inputs=@($mine | ForEach-Object {$_.Obj});Outputs=$outs;After=$mine}
        $l.Key=(@($taskCl)+$a+@($env:LIB)) -join "`n"
        [void]$taskLinks.Add($l)
    }
    $taskItems=@($taskCompiles)+@($taskLinks)
    $taskJobsFile=Join-Path ([IO.Path]::GetTempPath()) ('san14_cl_jobs_{0}.json' -f $PID)
    $taskFresh=@($taskItems | ForEach-Object {$false})
    if(-not $NoCache) {
        Write-TaskJson $taskJobsFile @($taskItems | ForEach-Object {@{stamp=$_.Stamp;key=$_.Key;outputs=$_.Outputs}})
        $taskFreshText=& $taskPython -B -X utf8 (Join-Path $PSScriptRoot 'cl_cache.py') fresh $taskJobsFile
        if($LASTEXITCODE -ne 0){throw 'cl_cache.py fresh failed.'}
        $taskFresh=@($taskFreshText | ConvertFrom-Json | ForEach-Object {$_})   # PowerShell 5.1 emits a JSON array as ONE object: enumerate it
        if($taskFresh.Count -ne $taskItems.Count -or @($taskFresh | Where-Object {$_ -isnot [bool]}).Count){throw "cl_cache.py fresh returned $($taskFresh.Count) answers for $($taskItems.Count) items."}
    }
    for($n=0;$n -lt $taskItems.Count;++$n){$taskItems[$n].Fresh=[bool]$taskFresh[$n]}
    foreach($l in $taskLinks){if(@($l.After | Where-Object {-not $_.Fresh}).Count){$l.Fresh=$false}}   # an object being rebuilt makes its link stale
    $taskTodo=@();foreach($i in $taskItems){if($i.Fresh){$i.Code=0}else{$taskTodo+=$i}}
    foreach($l in $taskLinks){if($l.Fresh){Write-Output ('[cached] {0} ({1})' -f $l.Name,$l.Kind)}}
    foreach($c in $taskCompiles){if($c.Fresh -and @($taskLinks | Where-Object {-not $_.Fresh -and $_.After -contains $c}).Count){Write-Output ('[cached] {0} ({1})' -f $c.Name,$c.Kind)}}
    foreach($c in $taskTodo){if($c.Deps){Get-ChildItem -LiteralPath $c.Deps -Filter *.json | Remove-Item -Force}}
    $taskMax=if($Jobs -gt 0){$Jobs}elseif(Get-Process -Name 'SAN14CE_KO' -ErrorAction SilentlyContinue){3}else{8}
    $taskWatch=[Diagnostics.Stopwatch]::StartNew()
    if($taskTodo.Count){Invoke-TaskPool $taskTodo $taskMax}
    $taskBad=@($taskTodo | Where-Object {$_.Code -ne 0})
    foreach($t in $taskTodo) {
        if($t.Code -eq 0){Write-Output ('[ok {0,5:N1}s] {1} ({2})' -f $t.Secs,$t.Name,$t.Kind);$warn=@(($t.Output -split "`r?`n") | Where-Object {$_ -match 'warning '});if($warn){$warn}}
        else {Write-Output ('[FAIL rc={0}] {1} ({2})' -f $t.Code,$t.Name,$t.Kind);Write-Output $t.Output}
    }
    if($taskTodo | Where-Object {$_.Code -eq 0}) {   # -NoCache skips only the freshness check; what it built is recorded for the next run
        Write-TaskJson $taskJobsFile @($taskTodo | ForEach-Object {if($_.Deps){@{stamp=$_.Stamp;key=$_.Key;outputs=$_.Outputs;deps=$_.Deps;sources=@($_.Src);ok=($_.Code -eq 0)}}else{@{stamp=$_.Stamp;key=$_.Key;outputs=$_.Outputs;inputs=@($_.Inputs);ok=($_.Code -eq 0)}}})
        & $taskPython -B -X utf8 (Join-Path $PSScriptRoot 'cl_cache.py') store $taskJobsFile
        if($LASTEXITCODE -ne 0){throw 'cl_cache.py store failed.'}
    }
    if(Test-Path -LiteralPath $taskJobsFile){Remove-Item -LiteralPath $taskJobsFile -Force}
    if($taskBad){throw ('Build failed: '+(($taskBad | ForEach-Object {'{0} ({1})' -f $_.Name,$_.Kind}) -join ', '))}
    Write-Output ('compile: {0} built, {1} cached, wall {2:N1}s, pool {3}' -f $taskTodo.Count,($taskItems.Count-$taskTodo.Count),$taskWatch.Elapsed.TotalSeconds,$taskMax)
    # 2026-11-16 unit split (docs/REFACTOR_EQUIVALENCE.md section 4): a multi-unit convenience DLL must not give each unit its own copy of
    # static state (tools/unit_equivalence.py audit: same internal-linkage symbol in .data/.bss/.tls of two objects). Every built variant is
    # audited here (diag adds COST_SCOPE keys); the split fixture objects are audited by split-module r_units. Unchanged objects skip it.
    foreach($k in @($taskVariants.Keys | Where-Object {$_ -ne 'fixture'} | Sort-Object)) {
        $taskOd=Join-Path $taskVariants[$k].Dir 'obj\SAN14_Convenience'
        $taskObjs=@(Get-ChildItem -LiteralPath $taskOd -Filter '*.obj' -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object {$_.FullName})
        if($taskObjs.Count -lt 2){continue}
        $taskAuditIn=@((Join-Path $PSScriptRoot 'unit_equivalence.py'))+$taskObjs
        $taskAuditGate=@{stamp=(Join-Path $taskOd 'gate_unit_state.json');files=$taskAuditIn}
        $taskAuditFile=Join-Path ([IO.Path]::GetTempPath()) ('san14_unit_audit_{0}.json' -f $PID)
        $taskAuditFresh=$false
        if(-not $NoCache) {
            Write-TaskJson $taskAuditFile @($taskAuditGate)
            $taskAuditFresh=@(& $taskPython -B -X utf8 (Join-Path $PSScriptRoot 'cl_cache.py') files-fresh $taskAuditFile | ConvertFrom-Json | ForEach-Object {$_})[0]
            if($LASTEXITCODE -ne 0){throw 'cl_cache.py files-fresh failed.'}
        }
        if($taskAuditFresh){Write-Output ('[cached] unit state audit ({0}) (objects and script unchanged since its last pass)' -f $k)}
        else {
            $taskAuditOut=& $taskPython -B -X utf8 (Join-Path $PSScriptRoot 'unit_equivalence.py') audit $taskOd 2>&1
            $taskAuditCode=$LASTEXITCODE
            Write-Output ('[{0}] unit state audit ({1})' -f $(if($taskAuditCode -eq 0){'ok'}else{'FAIL'}),$k)
            Write-Output $taskAuditOut
            if($taskAuditCode -ne 0){if(Test-Path -LiteralPath $taskAuditFile){Remove-Item -LiteralPath $taskAuditFile -Force};throw ('Unit state audit failed ({0}).' -f $k)}
            $taskAuditGate.ok=$true;Write-TaskJson $taskAuditFile @($taskAuditGate)
            & $taskPython -B -X utf8 (Join-Path $PSScriptRoot 'cl_cache.py') files-store $taskAuditFile
            if($LASTEXITCODE -ne 0){throw 'cl_cache.py files-store failed.'}
        }
        if(Test-Path -LiteralPath $taskAuditFile){Remove-Item -LiteralPath $taskAuditFile -Force}
    }
    if($taskVariants.ContainsKey('release')) {   # release-only gates: modpack fixture, pure loader + Layout loader order, exported surface
        $taskBuild=$taskVariants['release'].Dir
        $b={param($n) Join-Path $taskBuild $n}
        $taskDlls=@('dinput8.dll','SAN14_Layout.dll','SAN14_Convenience.dll','SAN14_CombatAI.dll','SAN14_CCZ.dll','SAN14_ModPack.dll' | ForEach-Object {& $b $_})
        $taskRuns=@(
            @{Name='modpack fixture';File=(& $b 'modpack_fixture.exe');Args=@();Cwd=$taskBuild;Save=(Join-Path $taskProject 'build\modpack_fixture.json');In=@((& $b 'modpack_fixture.exe'))},
            @{Name='pure loader fixture';File=$taskPython;Args=@('-B','-X','utf8',(Join-Path $taskTests 'verify_loader.py'));Cwd=$taskBuild;In=@((Join-Path $taskTests 'verify_loader.py'),(& $b 'dinput8.dll'),(& $b 'loader_fixture.exe'),(& $b 'LoaderMockCombat.dll'),(& $b 'LoaderMockBadCombat.dll'),(& $b 'LoaderMockConvenience.dll'))},
            @{Name='layout loader fixture';File=$taskPython;Args=@('-B','-X','utf8',(Join-Path $taskTests 'verify_layout_loader.py'));Cwd=$taskBuild;In=@((Join-Path $taskTests 'verify_layout_loader.py'),(& $b 'dinput8.dll'),(& $b 'loader_fixture.exe'),(& $b 'LayoutMock.dll'),(& $b 'LayoutMockFail.dll'),(& $b 'LayoutMockConvenience.dll'))},
            @{Name='split release exports';File=$taskPython;Args=@('-B','-X','utf8',(Join-Path $taskTests 'verify_split_exports.py'));Cwd=$taskBuild;In=@((Join-Path $taskTests 'verify_split_exports.py'),(Join-Path $PSScriptRoot 'pe_image.py'))+$taskDlls})
        for($n=0;$n -lt $taskRuns.Count;++$n){$taskRuns[$n].Stamp=Join-Path $taskBuild ('obj\gate_{0}.json' -f $n)}
        $taskGateFile=Join-Path ([IO.Path]::GetTempPath()) ('san14_gate_jobs_{0}.json' -f $PID)
        $taskGateFresh=@($taskRuns | ForEach-Object {$false})
        if(-not $NoCache) {   # a gate that passed with byte-identical binaries and script is not run again
            Write-TaskJson $taskGateFile @($taskRuns | ForEach-Object {@{stamp=$_.Stamp;files=$_.In}})
            $taskGateText=& $taskPython -B -X utf8 (Join-Path $PSScriptRoot 'cl_cache.py') files-fresh $taskGateFile
            if($LASTEXITCODE -ne 0){throw 'cl_cache.py files-fresh failed.'}
            $taskGateFresh=@($taskGateText | ConvertFrom-Json | ForEach-Object {$_})
            if($taskGateFresh.Count -ne $taskRuns.Count){throw 'cl_cache.py files-fresh answer count mismatch.'}
        }
        $taskGateTodo=@();for($n=0;$n -lt $taskRuns.Count;++$n){if($taskGateFresh[$n]){Write-Output ('[cached] {0} (binaries and script unchanged since its last pass)' -f $taskRuns[$n].Name);$taskRuns[$n].Code=0;$taskRuns[$n].Cached=$true}else{$taskGateTodo+=$taskRuns[$n]}}
        if($taskGateTodo.Count){Invoke-TaskPool $taskGateTodo 4}
        if($taskGateTodo.Count) {
            Write-TaskJson $taskGateFile @($taskGateTodo | ForEach-Object {@{stamp=$_.Stamp;files=$_.In;ok=($_.Code -eq 0)}})
            & $taskPython -B -X utf8 (Join-Path $PSScriptRoot 'cl_cache.py') files-store $taskGateFile
            if($LASTEXITCODE -ne 0){throw 'cl_cache.py files-store failed.'}
        }
        if(Test-Path -LiteralPath $taskGateFile){Remove-Item -LiteralPath $taskGateFile -Force}
        foreach($r in $taskGateTodo) {
            if($r.Code -eq 0 -and $r.Save){[IO.File]::WriteAllText($r.Save,((($r.Output -split "`r?`n") | Where-Object {$_.Trim()} | Select-Object -Last 1)),(New-Object Text.UTF8Encoding($false)))}
            Write-Output ('[{0} {1,5:N1}s] {2}' -f $(if($r.Code -eq 0){'ok'}else{'FAIL'}),$r.Secs,$r.Name)
            Write-Output $r.Output
        }
        $taskRunBad=@($taskRuns | Where-Object {$_.Code -ne 0})
        if($taskRunBad){throw ('Release gate failed: '+(($taskRunBad | ForEach-Object {$_.Name}) -join ', '))}
    }
    if($Fixture) {
        $taskBuild=$taskVariants['fixture'].Dir
        Push-Location -LiteralPath $taskBuild
        try {
            $taskResult=& '.\split_module_fixture.exe' (Join-Path $taskProject 'build\ui_native_analysis\fixture\1a4a447d_text.bin')
            if($LASTEXITCODE -ne 0){throw 'Split module integration fixture failed.'}
            [IO.File]::WriteAllText((Join-Path $taskProject 'build\split_module_fixture.json'),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            $taskResult
            # 2026-11-05 user exception: SAN14_Layout.dll hook manager / Present chain / window subclass (real sources, synthetic image).
            foreach($n in 'layout_fixture','layout_p3_fixture','layout_p4_fixture') {
                $taskLayout=& ".\$n.exe"
                if($LASTEXITCODE -ne 0){throw "$n failed."}
                [IO.File]::WriteAllText((Join-Path $taskProject "build\$n.json"),($taskLayout | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
                $taskLayout
            }
            & $taskPython -B -X utf8 (Join-Path $taskTests 'verify_layout_registry.py')
            if($LASTEXITCODE -ne 0){throw 'Layout registry static verification failed.'}
            $taskFailure=& '.\split_module_fixture.exe' (Join-Path $taskProject 'build\ui_native_analysis\fixture\1a4a447d_text.bin') '--failure'
            if($LASTEXITCODE -ne 0){throw 'Partial split publication rollback failed.'}
            [IO.File]::WriteAllText((Join-Path $taskProject 'build\split_module_failure_fixture.json'),($taskFailure | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            $taskFailure
            # 사용자 결정 2026-11-03: 통합 인사 제거. Keep shared CCZ/HUD guard coverage.
            foreach($taskCompat in @('--compat','--ccz','--ccz-guard','--merit','--save','--off','--empty','--combat-guard','--base-failure','--combat-failure','--base-prepare','--both-failure','--recheck','--briefing','--briefing-guard','--briefing-pointer-guard','--opt-merit','--opt-recruit','--opt-recruit-ai','--opt-injury','--opt-awards','--opt-loyalty','--opt-expert','--combat-alone','--base-alone')) {
                $taskCompatResult=& '.\split_module_fixture.exe' (Join-Path $taskProject 'build\ui_native_analysis\fixture\1a4a447d_text.bin') $taskCompat
                if($LASTEXITCODE -ne 0){throw "Split compat fixture failed: $taskCompat"}
                $taskCompatResult
            }
        } finally {Pop-Location}
    }
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
