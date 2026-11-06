param([switch]$Force)
# Address table fixtures (user 2026-11-05 patch resilience): resolver fixture + guard fixtures (convenience / combat). Output: build/development/addr.
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskOut=Join-Path $taskProject 'build\development\addr'
[void][IO.Directory]::CreateDirectory($taskOut)
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10';$taskVersion='10.0.26100.0'
$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
    Push-Location -LiteralPath $taskOut
    try {
        $taskFlags=@('/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc','/DMERIT_CANDIDATE','/DRECRUITMENT_UI_CANDIDATE','/DHIDDEN_LOYALTY','/DINJURY_PROTECTION','/DAWARD_WAREHOUSE','/DSORTIE_FORMATION','/DLAYOUT_OWNS_PRESENT','/DSPLIT_MODULE_FIXTURE')
        $taskSources=@((Join-Path $taskProject 'src\game_addresses.h'),(Join-Path $taskProject 'src\game_addresses_data.inl'),(Join-Path $taskProject 'src\game_addresses_gates.inl'),(Join-Path $taskProject 'tests\address_guard_fixture.cpp'),(Join-Path $taskProject 'tests\addr_resolver_fixture.cpp'))
        $taskNewest=($taskSources+(Get-ChildItem -LiteralPath (Join-Path $taskProject 'src') -Filter '*.inl' | ForEach-Object {$_.FullName})+(Get-ChildItem -LiteralPath (Join-Path $taskProject 'src') -Filter 'native_adapter.cpp' | ForEach-Object {$_.FullName})+(Get-ChildItem -LiteralPath (Join-Path $taskProject 'src') -Filter '*.h' | ForEach-Object {$_.FullName})+(Get-ChildItem -LiteralPath (Join-Path $taskProject 'tools') -Filter 'guard_windows.json' | ForEach-Object {$_.FullName}) | ForEach-Object {(Get-Item $_).LastWriteTimeUtc} | Measure-Object -Maximum).Maximum
        function Need($exe) {$Force -or -not (Test-Path $exe) -or (Get-Item $exe).LastWriteTimeUtc -lt $taskNewest}
        if(Need 'addr_resolver_fixture.exe') {
            & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\addr_resolver_fixture.cpp') /Feaddr_resolver_fixture.exe /link kernel32.lib user32.lib
            if($LASTEXITCODE -ne 0){throw 'Address resolver fixture build failed.'}
        }
        if(Need 'address_guard_convenience.exe') {
            # 2026-11-05 local3: the convenience variant also compiles the imported-code units (modpack, traits9) and the split_activate()
            # installers' guards. The generated payload headers come from the same tools as the production build.
            $taskGen=Join-Path $taskOut 'gen'
            [void][IO.Directory]::CreateDirectory($taskGen)
            if(-not (Test-Path (Join-Path $taskGen 'traits9_payloads.h')) -or $Force) {
                & python -B -X utf8 (Join-Path $taskProject 'tools\prepare_traits9.py') $taskGen
                if($LASTEXITCODE -ne 0){throw 'Traits9 source preparation failed.'}
            }
            & cl.exe @taskFlags /DSPLIT_BASE /DSPLIT_FIXTURE_MODPACK /DSPLIT_FIXTURE_ACTIVATE "/I$taskGen" (Join-Path $taskProject 'tests\address_guard_fixture.cpp') /Feaddress_guard_convenience.exe /link kernel32.lib user32.lib gdi32.lib
            if($LASTEXITCODE -ne 0){throw 'Address guard fixture (convenience) build failed.'}
        }
        if(Need 'address_guard_combat.exe') {
            & cl.exe @taskFlags /DSPLIT_COMBAT (Join-Path $taskProject 'tests\address_guard_fixture.cpp') /Feaddress_guard_combat.exe /link kernel32.lib user32.lib
            if($LASTEXITCODE -ne 0){throw 'Address guard fixture (combat) build failed.'}
        }
    } finally {Pop-Location}
} finally {
    $env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib
}
