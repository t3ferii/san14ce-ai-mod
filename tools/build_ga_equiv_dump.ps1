param([string]$Out='',[string]$Exe='',[switch]$Force,[string]$Project='')
# Address-table equivalence dump (docs/MODULARIZATION_PLAN.md 1-C): builds tests/ga_equiv_dump.cpp in the two guard-fixture variants
# (convenience, combat) and runs both on the unpacked game EXE. Output: <Out>/convenience.txt, combat.txt (default build/development/ga_equiv/current).
# Compare two output folders with tools/ga_equiv_compare.py. Reads files only; never touches the game or an installation.
# -Project <tree>: build from another tree (tools/patch_next.py stages a candidate table; env GA_FIXTURE_BUILD picks its column).
# Since the W1-E re-base the address table has one build column; the checked-in pre re-base dump is development/ga_equiv/baseline_b1.
$ErrorActionPreference='Stop'
$taskProject=if($Project){$Project}else{Split-Path -Parent $PSScriptRoot}
$taskWork=Join-Path $taskProject 'build\development\ga_equiv'
if(-not $Out){$Out=Join-Path $taskWork 'current'}
if(-not $Exe){$Exe=Join-Path $taskProject 'build\ui_native_analysis\editor83_b25953386\game.exe.unpacked.exe'}
if(-not [IO.Path]::IsPathRooted($Out)){$Out=Join-Path $taskProject $Out}
if(-not [IO.Path]::IsPathRooted($Exe)){$Exe=Join-Path $taskProject $Exe}
[void][IO.Directory]::CreateDirectory($taskWork);[void][IO.Directory]::CreateDirectory($Out)
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10';$taskVersion='10.0.26100.0'
$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
    Push-Location -LiteralPath $taskWork
    try {
        $taskFlags=@('/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc','/DMERIT_CANDIDATE','/DRECRUITMENT_UI_CANDIDATE','/DHIDDEN_LOYALTY','/DINJURY_PROTECTION','/DAWARD_WAREHOUSE','/DSORTIE_FORMATION','/DLAYOUT_OWNS_PRESENT','/DSPLIT_MODULE_FIXTURE')
        $taskDump=Join-Path $taskProject 'tests\ga_equiv_dump.cpp'
        if($Force -or -not (Test-Path 'ga_equiv_convenience.exe') -or (Get-Item 'ga_equiv_convenience.exe').LastWriteTimeUtc -lt (Get-Item (Join-Path $taskProject 'src\game_addresses.h')).LastWriteTimeUtc) {
            & cl.exe @taskFlags /DSPLIT_BASE /DSPLIT_FIXTURE_MODPACK /DSPLIT_FIXTURE_ACTIVATE $taskDump /Fega_equiv_convenience.exe /link kernel32.lib user32.lib gdi32.lib
            if($LASTEXITCODE -ne 0){throw 'ga_equiv_dump (convenience) build failed.'}
        }
        if($Force -or -not (Test-Path 'ga_equiv_combat.exe') -or (Get-Item 'ga_equiv_combat.exe').LastWriteTimeUtc -lt (Get-Item (Join-Path $taskProject 'src\game_addresses.h')).LastWriteTimeUtc) {
            & cl.exe @taskFlags /DSPLIT_COMBAT $taskDump /Fega_equiv_combat.exe /link kernel32.lib user32.lib
            if($LASTEXITCODE -ne 0){throw 'ga_equiv_dump (combat) build failed.'}
        }
        foreach($v in 'convenience','combat'){
            $env:GA_EQUIV_OUT=(Join-Path $Out ($v+'.txt'))
            $taskJson=& ".\ga_equiv_$v.exe" $Exe
            if($LASTEXITCODE -ne 0){throw "ga_equiv_$v failed."}
            Set-Content -LiteralPath (Join-Path $Out ($v+'.fixture.json')) -Value $taskJson -Encoding utf8
            Write-Output ("{0}: {1} lines" -f $v,(Get-Content -LiteralPath $env:GA_EQUIV_OUT | Measure-Object -Line).Lines)
        }
        Remove-Item Env:\GA_EQUIV_OUT -ErrorAction SilentlyContinue
    } finally {Pop-Location}
} finally {
    $env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib
}
