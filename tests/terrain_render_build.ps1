$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build\development\terrain_render'
[void][IO.Directory]::CreateDirectory($taskBuild)
& python -B -X utf8 (Join-Path $PSScriptRoot 'terrain_render_verify.py')
if($LASTEXITCODE -ne 0){throw 'Terrain render source checks failed.'}
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10';$taskVersion='10.0.26100.0'
$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
    Push-Location -LiteralPath $taskBuild
    try {
        foreach($taskMode in @('diagnostic','release')) {
            $taskFlags=@('/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc')
            if($taskMode -eq 'release'){$taskFlags+='/DNO_RUNTIME_LOG'}
            foreach($taskR2 in @('r2','r2b','overlay','cliff','water_mask')) {
                $taskR2Exe="terrain_render_${taskR2}_${taskMode}.exe"
                $taskR2Libs=@('kernel32.lib')
                if($taskR2 -eq 'overlay'){$taskR2Libs+=@('user32.lib','gdi32.lib','d3d11.lib','dxgi.lib')}
                if($taskR2 -in @('r2b','cliff','water_mask')){$taskR2Libs+=@('d3d11.lib')}
                & cl.exe @taskFlags (Join-Path $PSScriptRoot "terrain_render_${taskR2}_fixture.cpp") "/Fe$taskR2Exe" /link @taskR2Libs
                if($LASTEXITCODE -ne 0){throw "Terrain render $taskR2 $taskMode compilation failed."}
                # User/Claude 2026-10-29 local55: dump code must be compiled out of release.
                if($taskR2 -eq 'water_mask') {
                    $taskImageText=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes((Join-Path $taskBuild $taskR2Exe)))
                    $taskHasDump=$taskImageText.Contains('water_mask_dump')
                    if($taskHasDump -ne ($taskMode -eq 'diagnostic')){throw 'Mask dump diagnostic/release exclusion failed.'}
                }
                $taskR2Result=& ".\$taskR2Exe"
                if($LASTEXITCODE -ne 0){throw "Terrain render $taskR2 $taskMode fixture failed."}
                $taskR2Result
                [IO.File]::WriteAllText((Join-Path $taskBuild "${taskR2}_${taskMode}_result.json"),($taskR2Result | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            }
            $taskExe="terrain_render_${taskMode}.exe"
            & cl.exe @taskFlags (Join-Path $PSScriptRoot 'terrain_render_fixture.cpp') "/Fe$taskExe" /link kernel32.lib
            if($LASTEXITCODE -ne 0){throw "Terrain render $taskMode compilation failed."}
            $taskResult=& ".\$taskExe"
            if($LASTEXITCODE -ne 0){throw "Terrain render $taskMode fixture failed."}
            $taskResult
            [IO.File]::WriteAllText((Join-Path $taskBuild "${taskMode}_result.json"),($taskResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            # User 2026-10-29: forwarding proxy coexistence exercises production original_site.
            $taskSiteExe="terrain_render_site_${taskMode}.exe"
            & cl.exe @taskFlags (Join-Path $PSScriptRoot 'terrain_render_site_fixture.cpp') "/Fe$taskSiteExe" /link kernel32.lib
            if($LASTEXITCODE -ne 0){throw "Terrain render site $taskMode compilation failed."}
            $taskSiteResult=& ".\$taskSiteExe"
            if($LASTEXITCODE -ne 0){throw "Terrain render site $taskMode fixture failed."}
            $taskSiteResult
            [IO.File]::WriteAllText((Join-Path $taskBuild "site_${taskMode}_result.json"),($taskSiteResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
            $taskEarlyExe="terrain_render_early_${taskMode}.exe"
            & cl.exe @taskFlags (Join-Path $PSScriptRoot 'terrain_render_early_fixture.cpp') "/Fe$taskEarlyExe" /link kernel32.lib
            if($LASTEXITCODE -ne 0){throw "Terrain render early $taskMode compilation failed."}
            $taskEarlyResult=& ".\$taskEarlyExe"
            if($LASTEXITCODE -ne 0){throw "Terrain render early $taskMode fixture failed."}
            $taskEarlyResult
            [IO.File]::WriteAllText((Join-Path $taskBuild "early_${taskMode}_result.json"),($taskEarlyResult | Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)))
        }
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
