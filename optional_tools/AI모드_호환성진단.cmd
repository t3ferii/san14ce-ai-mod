@echo off
setlocal
chcp 65001 >nul
set "taskPS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe" set "taskPS=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
set "taskOptions="
if /I "%~1"=="--no-pause" set "taskOptions=-NonInteractive"
"%taskPS%" -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0..\진단.ps1" %taskOptions%
set "taskExit=%errorlevel%"
if /I "%~1"=="--no-pause" exit /b %taskExit%
echo.
pause
exit /b %taskExit%
