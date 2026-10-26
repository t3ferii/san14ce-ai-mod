param([switch]$Fixture,[string]$Source)
$ErrorActionPreference='Stop'
$taskProject=Split-Path -Parent $PSScriptRoot
$taskBuild=Join-Path $taskProject 'build\development\lag_analysis'
$taskVc='C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk='C:\Program Files (x86)\Windows Kits\10';$taskVersion='10.0.26100.0'
$taskSavedPath=$env:PATH;$taskSavedInclude=$env:INCLUDE;$taskSavedLib=$env:LIB
try {
 $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
 $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskVersion\ucrt;$taskSdk\Include\$taskVersion\shared;$taskSdk\Include\$taskVersion\um"
 $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskVersion\ucrt\x64;$taskSdk\Lib\$taskVersion\um\x64"
 [void][IO.Directory]::CreateDirectory($taskBuild);Push-Location -LiteralPath $taskBuild
 try {
  if($Fixture) {
   & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject 'tests\cost_profile_fixture.cpp') /Fecost_profile_fixture.exe /link kernel32.lib
   if($LASTEXITCODE -ne 0){throw 'Cost fixture build failed'}
   $taskResult=& '.\cost_profile_fixture.exe';if($LASTEXITCODE -ne 0){throw 'Cost fixture failed'}
   [IO.File]::WriteAllText((Join-Path $taskProject 'build\cost_profile_fixture.json'),($taskResult|Select-Object -Last 1),(New-Object Text.UTF8Encoding($false)));$taskResult
  } else {
   $taskResolved=[IO.Path]::GetFullPath($Source);if(-not $taskResolved.StartsWith((Join-Path $taskProject 'backup\'),[StringComparison]::OrdinalIgnoreCase)){throw 'Use preserved source under backup'}
   $taskFlags=@('/nologo','/W4','/WX','/wd4505','/utf-8','/O2','/MT','/std:c++17','/EHsc','/DMERIT_CANDIDATE','/DRECRUITMENT_UI_CANDIDATE','/DHIDDEN_LOYALTY','/DINJURY_PROTECTION','/DAWARD_WAREHOUSE','/DSORTIE_FORMATION','/DNO_RUNTIME_LOG')
   foreach($taskModule in @('Convenience','CombatAI')) {
    $taskDefine=if($taskModule -eq 'Convenience'){'/DSPLIT_BASE'}else{'/DSPLIT_COMBAT'}
    & cl.exe @taskFlags $taskDefine /LD (Join-Path $taskResolved 'native_adapter.cpp') "/FeSAN14_$taskModule.dll" /link kernel32.lib user32.lib "/MAP:SAN14_$taskModule.map"
    if($LASTEXITCODE -ne 0){throw 'Preserved source map build failed'}
   }
  }
 } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
