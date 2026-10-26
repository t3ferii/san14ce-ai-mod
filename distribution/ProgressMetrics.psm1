# Read Windows accounting only; no game memory, handles for writes, or native calls.
function Get-ProgressSnapshot {
    param([Diagnostics.Process]$Process, [Diagnostics.Stopwatch]$Clock)
    $Process.Refresh()
    $taskCpu = $Process.TotalProcessorTime.TotalMilliseconds
    $taskAt = $Clock.Elapsed.TotalMilliseconds
    $taskThreads = @{}; $taskFailed = 0
    try {
        foreach ($taskThread in $Process.Threads) {
            try {
                $taskIdentity = '{0}:{1}' -f $taskThread.Id, $taskThread.StartTime.ToUniversalTime().Ticks
                $taskThreads[$taskIdentity] = @{ id=$taskThread.Id; cpu_ms=$taskThread.TotalProcessorTime.TotalMilliseconds }
            } catch { $taskFailed++ }
        }
    } catch { $taskFailed++ }
    [PSCustomObject]@{
        process_id=$Process.Id; started_ticks=$Process.StartTime.ToUniversalTime().Ticks
        at_ms=$taskAt; cpu_ms=$taskCpu; threads=$taskThreads; failed_thread_reads=$taskFailed
    }
}
function Get-ProgressInterval {
    param($Before, $After, [int]$LogicalProcessors)
    if ($LogicalProcessors -lt 1) { throw 'Logical processor count must be positive.' }
    if ($Before.process_id -ne $After.process_id -or $Before.started_ticks -ne $After.started_ticks) { throw 'Process identity changed.' }
    $taskWall = [double]$After.at_ms - [double]$Before.at_ms
    $taskCpu = [double]$After.cpu_ms - [double]$Before.cpu_ms
    if ($taskWall -le 0 -or $taskCpu -lt 0) { throw 'Invalid accounting interval.' }
    $taskHotId=$null; $taskHotCpu=0.0; $taskMatched=0
    foreach ($taskKey in $After.threads.Keys) {
        if (-not $Before.threads.ContainsKey($taskKey)) { continue }
        $taskDelta = [double]$After.threads[$taskKey].cpu_ms - [double]$Before.threads[$taskKey].cpu_ms
        if ($taskDelta -lt 0) { continue }
        $taskMatched++
        if ($taskDelta -gt $taskHotCpu) { $taskHotCpu=$taskDelta; $taskHotId=$After.threads[$taskKey].id }
    }
    # One-core-equivalent usage can exceed 100% when multiple threads work.
    [PSCustomObject]@{
        elapsed_ms=[math]::Round($After.at_ms,3); interval_ms=[math]::Round($taskWall,3)
        process_cpu_ms=[math]::Round($taskCpu,3)
        process_core_equivalents=[math]::Round($taskCpu/$taskWall,4)
        process_machine_percent=[math]::Round(100*$taskCpu/$taskWall/$LogicalProcessors,3)
        hottest_thread_id=$taskHotId; hottest_thread_cpu_ms=[math]::Round($taskHotCpu,3)
        hottest_thread_one_core_percent=$(if($taskMatched){[math]::Round(100*$taskHotCpu/$taskWall,3)}else{$null})
        matched_threads=$taskMatched; current_threads=$After.threads.Count
        failed_thread_reads=$After.failed_thread_reads
    }
}
Export-ModuleMember -Function Get-ProgressSnapshot,Get-ProgressInterval
