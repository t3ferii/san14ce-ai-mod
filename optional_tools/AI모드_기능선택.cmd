@echo off
setlocal
chcp 65001 >nul
set "taskPS=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if exist "%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe" set "taskPS=%SystemRoot%\Sysnative\WindowsPowerShell\v1.0\powershell.exe"
rem Changes AI_TacticsMod\기능설정.ini only. DLL files are not touched; restart the game to apply.
"%taskPS%" -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0..\관리.ps1" -Action Select
set "taskExit=%errorlevel%"
echo.
pause
exit /b %taskExit%
