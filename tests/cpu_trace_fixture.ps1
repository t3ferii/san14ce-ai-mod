# User 2026-11-10 U35: exercise capture lifecycle without WPR or a game.
$ErrorActionPreference='Stop'
$source=Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '..\tools\cpu_trace.ps1')
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseInput($source,[ref]$tokens,[ref]$errors)
if($errors.Count){throw ($errors | Out-String)}
$finder=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Find-TraceGame'},$true)
$sandbox=Join-Path $env:TEMP ('san14_trace_fixture_'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory((Join-Path $sandbox 'tools'))
function wpr.exe {
  if($args[0] -eq '-start'){$global:LASTEXITCODE=$script:startCode}
  else{[IO.File]::WriteAllText($args[1],'mock ETL');$global:LASTEXITCODE=0}
}
function Start-Sleep { param($Milliseconds) }
try {
  foreach($case in @('detected_exit','no_game','lookup_error','start_failure')){
    $script:startCode=if($case -eq 'start_failure'){5}else{0}
    $mock=if($case -eq 'lookup_error'){'function Find-TraceGame { throw "fixture lookup failed" }'}else{@'
function Find-TraceGame {
  $p=[pscustomobject]@{Id=12345;HasExited=$false;Polls=0;PrivateMemorySize64=8589934592L;PeakWorkingSet64=4294967296L}
  $p | Add-Member ScriptMethod Refresh { $this.Polls++;$this.HasExited=($this.Polls -ge 2) }
  $p
}
'@}
    $body=$source.Remove($finder.Extent.StartOffset,$finder.Extent.EndOffset-$finder.Extent.StartOffset).Insert($finder.Extent.StartOffset,$mock)
    $body=$body.Replace('$PSScriptRoot',("'"+(Join-Path $sandbox 'tools').Replace("'","''")+"'"))
    $limit=if($case -eq 'no_game'){0}else{10}
    try{& ([scriptblock]::Create($body)) -Tag $case -MaxSeconds $limit}
    catch{if($case -ne 'lookup_error'){throw}}
    $state=Get-Content -Raw -LiteralPath (Join-Path $sandbox ('data\performance\cpu_trace_'+$case+'.json')) | ConvertFrom-Json
    $expected=@{detected_exit='saved';no_game='saved_no_game';lookup_error='monitor_failed';start_failure='start_failed'}[$case]
    if($state.status -ne $expected){throw "$case status=$($state.status), expected=$expected"}
    if($case -eq 'detected_exit' -and ($state.pid -ne 12345 -or $state.reason -ne 'game_exited')){throw 'Detected PID/exit reason lost'}
    if($case -eq 'detected_exit' -and ($state.sampled_peak_private_bytes -ne 8589934592L -or $state.peak_working_set_bytes -ne 4294967296L)){throw '64-bit process memory counters lost'}
    if($case -ne 'start_failure' -and -not (Test-Path -LiteralPath $state.etl)){throw 'Mock ETL not saved'}
    Write-Host ('[PASS] '+$case)
  }
} finally {
  # Only remove the unique fixture directory created above, beneath TEMP.
  if((Split-Path -Parent ([IO.Path]::GetFullPath($sandbox))) -ne [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\')){throw 'Unexpected fixture path'}
  Remove-Item -LiteralPath $sandbox -Recurse -Force
}
