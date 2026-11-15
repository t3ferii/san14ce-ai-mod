@echo off
rem name_lookup A/B CPU record (administrator). Usage: A or B. Stops when the game exits.
set "SAN14_TRACE_ROOT=%~dp0..\..\"
set "SAN14_TRACE_SIDE=%~1"
if "%SAN14_TRACE_SIDE%"=="" set "SAN14_TRACE_SIDE=A"
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -Command "$taskScript=Join-Path $env:SAN14_TRACE_ROOT 'AI_TacticsMod\tools\name_lookup_trace.ps1'; Start-Process powershell.exe -Verb RunAs -WindowStyle Hidden -ArgumentList @('-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-File',([string][char]34+$taskScript+[char]34),'-Side',$env:SAN14_TRACE_SIDE)"
