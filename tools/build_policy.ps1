param([string]$Compiler = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe')
$ErrorActionPreference = 'Stop'
$taskProject = Split-Path -Parent $PSScriptRoot
$taskBuild = Join-Path $taskProject 'build'
[void][IO.Directory]::CreateDirectory($taskBuild)
$taskSavedPath = $env:PATH
try {
    $env:PATH = (Split-Path -Parent $Compiler) + ';' + $taskSavedPath
    Push-Location -LiteralPath $taskBuild
    try {
        & $Compiler /nologo /W4 /WX /utf-8 /O2 /TC /GS- /LD /Zl /DTACTICS_POLICY_EXPORTS ('/I' + (Join-Path $taskProject 'src')) (Join-Path $taskProject 'src\tactics_policy.c') /FeSAN14_TacticsPolicy.dll /link /NOENTRY /NODEFAULTLIB /MACHINE:X64 /IMPLIB:SAN14_TacticsPolicy.lib
        if ($LASTEXITCODE -ne 0) { throw 'Tactics policy compilation failed.' }
    } finally { Pop-Location }
} finally { $env:PATH = $taskSavedPath }
Write-Output 'Built standalone policy test library. Run build_native.ps1 for the game adapter.'
