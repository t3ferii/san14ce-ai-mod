param([switch]$Bindings,[switch]$Friendship,[switch]$Merit,[switch]$RecruitmentUI,[switch]$Awards)
$ErrorActionPreference = 'Stop'
$taskProject = Split-Path -Parent $PSScriptRoot
$taskVc = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$taskSdk = 'C:\Program Files (x86)\Windows Kits\10'
$taskSdkVersion = '10.0.26100.0'
$taskSavedPath=$env:PATH; $taskSavedInclude=$env:INCLUDE; $taskSavedLib=$env:LIB
try {
    $env:PATH="$taskVc\bin\Hostx64\x64;$taskSavedPath"
    $env:INCLUDE="$taskVc\include;$taskSdk\Include\$taskSdkVersion\ucrt;$taskSdk\Include\$taskSdkVersion\shared;$taskSdk\Include\$taskSdkVersion\um"
    $env:LIB="$taskVc\lib\x64;$taskSdk\Lib\$taskSdkVersion\ucrt\x64;$taskSdk\Lib\$taskSdkVersion\um\x64"
    Push-Location -LiteralPath (Join-Path $taskProject 'build')
    try {
        if(([int]$Bindings.IsPresent+[int]$Friendship.IsPresent+[int]$Merit.IsPresent+[int]$RecruitmentUI.IsPresent+[int]$Awards.IsPresent) -gt 1){throw 'Choose one fixture at a time.'}
        $taskFixtureName=if($Awards){'awards_native_fixture'}elseif($RecruitmentUI){'recruitment_ui_fixture'}elseif($Merit){'merit_native_fixture'}elseif($Friendship){'friendship_fixture'}elseif($Bindings){'personnel_native_fixture'}else{'personnel_policy_fixture'}
        & cl.exe /nologo /W4 /WX /wd4505 /utf-8 /O2 /MT /std:c++17 /EHsc (Join-Path $taskProject "tests\$taskFixtureName.cpp") "/Fe$taskFixtureName.exe" /link kernel32.lib user32.lib
        if($LASTEXITCODE -ne 0){throw 'Personnel rule compilation failed.'}
        $taskResult = & (Join-Path (Get-Location) "$taskFixtureName.exe")
        if($LASTEXITCODE -ne 0){throw 'Personnel rule checks failed.'}
        if($RecruitmentUI -or $Awards){$taskResult=$taskResult | Select-Object -Last 1}
        $taskUtf8=New-Object System.Text.UTF8Encoding($false)
        [IO.File]::WriteAllText((Join-Path $taskProject "build\$taskFixtureName.json"),$taskResult,$taskUtf8)
        $taskResult
    } finally {Pop-Location}
} finally {$env:PATH=$taskSavedPath;$env:INCLUDE=$taskSavedInclude;$env:LIB=$taskSavedLib}
