param([string]$Tag='full',[int]$MaxSeconds=2400,[switch]$ValidateOnly)
if($ValidateOnly){
  [void][scriptblock]::Create([IO.File]::ReadAllText($PSCommandPath))
  Write-Output 'CPU trace script syntax valid; no recording started.';return
}
# WPR rejects results paths under Program Files (0xc5600600): save to TEMP, then move.
# User 2026-11-10 (U34 total analysis): sampled-CPU WPR record of the whole game session (original EXE + all SAN14 DLLs, with stacks).
# Records only; never touches the game process. Run from an ADMIN PowerShell before starting the game.
# Stops when the game exits after having run, on the stop file data\performance\cpu_trace.stop, or after MaxSeconds.
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$folder=Join-Path $project 'data\performance'
[void][IO.Directory]::CreateDirectory($folder)
$inst='SAN14Cpu_'+$Tag
$etl=Join-Path $folder ('cpu_'+$Tag+'_'+(Get-Date).ToString('yyyyMMdd_HHmmss')+'.etl')
$state=Join-Path $folder ('cpu_trace_'+$Tag+'.json')
$mainFolder=Join-Path (Split-Path -Parent $project) 'AI_TacticsMod\data\performance'
$states=@($state)
if($mainFolder -ne $folder -and (Test-Path -LiteralPath $mainFolder)){
  $states+=Join-Path $mainFolder ('cpu_trace_'+$Tag+'.json')
}
$stop=Join-Path $folder 'cpu_trace.stop'
Remove-Item -LiteralPath $stop -ErrorAction SilentlyContinue
function St($s,$extra=@{}){$o=@{status=$s;tag=$Tag;etl=$etl;time=(Get-Date).ToString('o');monitor_pid=$PID;sampled_peak_private_bytes=$peakPrivate;peak_working_set_bytes=$peakWorkingSet}+$extra
  foreach($path in $states){[IO.File]::WriteAllText($path,($o|ConvertTo-Json),(New-Object Text.UTF8Encoding($false)))}}
# Use the process API directly and keep the detected process object. A failed
# lookup must never be interpreted as the exit of a previously observed game.
function Find-TraceGame {
  [Diagnostics.Process]::GetProcessesByName('SAN14CE_KO') | Sort-Object Id | Select-Object -First 1
}
$owned=$false;$seen=$null;$game=$null;$monitorError=$null;$reason='waiting_timeout'
[long]$peakPrivate=0;[long]$peakWorkingSet=0
try {
  & wpr.exe -start ((Join-Path $project 'distribution\SAN14CpuSample.wprp')+'!SAN14CPU') -filemode -instancename $inst
  if($LASTEXITCODE -ne 0){St 'start_failed' @{error='wpr_exit_'+$LASTEXITCODE};Write-Host 'WPR start failed (run this window as administrator).';return}
  $owned=$true;St 'recording' @{phase='waiting_for_game';pid=$null};Write-Host 'Recording. Start the game now; the trace stops when the game exits.'
  $clock=[Diagnostics.Stopwatch]::StartNew()
  $lastHeartbeat=-5
  while($clock.Elapsed.TotalSeconds -lt $MaxSeconds -and -not (Test-Path -LiteralPath $stop)) {
    if(-not $seen){
      $game=Find-TraceGame
      if($game){
        $seen=$game.Id;$clock.Restart();$lastHeartbeat=-5;$reason='duration_limit'
        Write-Host ('Game detected: PID '+$seen)
      }
    }
    if($seen){
      $game.Refresh()
      if($game.HasExited){$reason='game_exited';break}
    }
    if($clock.Elapsed.TotalSeconds - $lastHeartbeat -ge 5){
      if($seen){
        # Process counters only; no game memory reads or remote game calls.
        $peakPrivate=[Math]::Max($peakPrivate,[long]$game.PrivateMemorySize64)
        $peakWorkingSet=[Math]::Max($peakWorkingSet,[long]$game.PeakWorkingSet64)
      }
      $phase=if($seen){'game_detected'}else{'waiting_for_game'}
      St 'recording' @{pid=$seen;phase=$phase;elapsed_seconds=[int]$clock.Elapsed.TotalSeconds}
      $lastHeartbeat=$clock.Elapsed.TotalSeconds
    }
    Start-Sleep -Milliseconds 200
  }
  if(Test-Path -LiteralPath $stop){$reason='stop_file'}
} catch {
  $monitorError=$_.Exception.Message;$reason='monitor_error'
  St 'monitor_failed' @{pid=$seen;error=$monitorError}
  throw
} finally {
  if($owned){St 'saving' @{pid=$seen;reason=$reason;error=$monitorError};Write-Host 'Saving trace...'; $tmp=Join-Path $env:TEMP ('san14_cpu_'+$Tag+'.etl'); & wpr.exe -stop $tmp ('SAN14 cpu '+$Tag) -skipPdbGen -instancename $inst
    if($LASTEXITCODE -eq 0){
      Move-Item -LiteralPath $tmp -Destination $etl -Force
      $result=if($monitorError){'monitor_failed'}elseif($seen){'saved'}else{'saved_no_game'}
      St $result @{pid=$seen;reason=$reason;error=$monitorError};Write-Host ($result+' '+$etl)
    }else{St 'save_failed' @{pid=$seen;error='wpr_exit_'+$LASTEXITCODE}}}
  Remove-Item -LiteralPath $stop -ErrorAction SilentlyContinue
}
