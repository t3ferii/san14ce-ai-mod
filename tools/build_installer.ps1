# Builds the GUI installer (WinForms, .NET Framework 4.x) into build\development\installer.
# Writes nothing outside build\development; release tools copy the result into a package.
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskOut=Join-Path $taskProject 'build\development\installer'
[void][IO.Directory]::CreateDirectory($taskOut)
$taskCsc='C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\Roslyn\csc.exe'
if(-not [IO.File]::Exists($taskCsc)){$taskCsc=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'}
$taskFramework=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319'
$taskExe=Join-Path $taskOut 'AI모드_설치관리자.exe'
$taskRefs=@('mscorlib.dll','System.dll','System.Core.dll','System.Drawing.dll','System.Windows.Forms.dll') | ForEach-Object { '/reference:' + (Join-Path $taskFramework $_) }
& $taskCsc /nologo /noconfig /nostdlib+ /target:winexe /platform:anycpu /optimize+ /warnaserror+ /utf8output /codepage:65001 `
    ("/win32manifest:" + (Join-Path $taskProject 'installer\app.manifest')) $taskRefs ("/out:" + $taskExe) (Join-Path $taskProject 'installer\InstallerGui.cs')
if($LASTEXITCODE -ne 0){throw 'Installer compilation failed.'}
& python -B -X utf8 (Join-Path $taskProject 'tests\verify_installer_gui.py') $taskExe
if($LASTEXITCODE -ne 0){throw 'Installer catalog check failed.'}
$taskFixture=Join-Path $taskOut 'fixture'
& powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File (Join-Path $taskProject 'tests\installer_gui_roundtrip.ps1') -Exe $taskExe -Fixture $taskFixture
if($LASTEXITCODE -ne 0){throw 'Installer roundtrip check failed.'}
