param([Parameter(Mandatory=$true)][ValidateSet('A','B','H')][string]$Side,[int]$MaxSeconds=1800)
# Sampled-CPU WPR record for the name_lookup A/B. Records only; never touches
# the game process. Stops when the game exits after having run, on the stop
# file data\performance\name_lookup_trace.stop, or after MaxSeconds.
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$folder=Join-Path $project 'data\performance'
$inst='SAN14NameLookup_'+$Side
$etl=Join-Path $folder ('name_lookup_'+$Side+'_'+(Get-Date).ToString('yyyyMMdd_HHmmss')+'.etl')
$state=Join-Path $folder ('name_lookup_trace_'+$Side+'.json')
$stop=Join-Path $folder 'name_lookup_trace.stop'
Remove-Item -LiteralPath $stop -ErrorAction SilentlyContinue
function St($s,$extra=@{}){$o=@{status=$s;side=$Side;etl=$etl;time=(Get-Date).ToString('o')}+$extra
  [IO.File]::WriteAllText($state,($o|ConvertTo-Json),(New-Object Text.UTF8Encoding($false)))}
$owned=$false;$seen=$null
try {
  & wpr.exe -start ((Join-Path $project 'distribution\SAN14CpuSample.wprp')+'!SAN14CPU') -filemode -recordtempto $folder -instancename $inst
  if($LASTEXITCODE -ne 0){St 'start_failed' @{error='wpr_exit_'+$LASTEXITCODE};return}
  $owned=$true;St 'recording'
  $clock=[Diagnostics.Stopwatch]::StartNew()
  while($clock.Elapsed.TotalSeconds -lt $MaxSeconds -and -not (Test-Path -LiteralPath $stop)) {
    $game=Get-Process SAN14CE_KO -ErrorAction SilentlyContinue | Select-Object -First 1
    if($game -and -not $seen){$seen=$game.Id;St 'recording' @{pid=$seen}}
    if($seen -and -not $game){break}
    Start-Sleep -Milliseconds 500
  }
} finally {
  if($owned){St 'saving' @{pid=$seen}; & wpr.exe -stop $etl ('SAN14 name_lookup '+$Side) -skipPdbGen -instancename $inst
    if($LASTEXITCODE -eq 0){St 'saved' @{pid=$seen}}else{St 'save_failed' @{pid=$seen;error='wpr_exit_'+$LASTEXITCODE}}}
  Remove-Item -LiteralPath $stop -ErrorAction SilentlyContinue
}
