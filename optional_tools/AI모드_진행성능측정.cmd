@echo off
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\distribution\ProgressMeasure.ps1" %*
if errorlevel 1 exit /b 1
pause
