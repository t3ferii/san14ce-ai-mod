@echo off
set "SAN14_TRACE_ROOT=%~dp0..\..\"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$taskScript=Join-Path $env:SAN14_TRACE_ROOT 'AI_TacticsMod\tools\capture_progress_cpu.ps1'; Start-Process powershell.exe -Verb RunAs -WindowStyle Hidden -ArgumentList @('-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-File',([string][char]34+$taskScript+[char]34),'-Seconds','120','-SampleOnly')"
