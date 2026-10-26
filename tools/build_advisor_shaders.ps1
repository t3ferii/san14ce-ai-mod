param()
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskFxc='C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe'
$taskOutput=Join-Path $taskProject 'build\development\advisor_shaders'
[void][IO.Directory]::CreateDirectory($taskOutput)
foreach($taskStage in @('vs','ps')) {
    $taskHeader=Join-Path $taskOutput ('advisor_'+$taskStage+'.h')
    & $taskFxc /nologo /O3 /T ($taskStage+'_4_0') /E ($taskStage+'_main') /Vn ('advisor_'+$taskStage) /Fh $taskHeader (Join-Path $taskProject 'src\advisor_briefing.hlsl')
    if($LASTEXITCODE -ne 0){throw 'Advisor shader compilation failed'}
    if((Get-FileHash -LiteralPath $taskHeader).Hash -ne (Get-FileHash -LiteralPath (Join-Path $taskProject ('src\advisor_'+$taskStage+'.h'))).Hash){throw 'Generated shader differs from checked source header; reconcile source assets before building'}
}
