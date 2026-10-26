# Headless check of the built GUI installer: loads the EXE assembly and, in an isolated fixture
# folder only, checks old-file inheritance, dependency chaining, package-copy limits, the
# guarded manager's (distribution copy) rewrite of 기능설정.ini, and renders each tab to PNG.
# Never touches the real game folder; no clicks.
param([Parameter(Mandatory)][string]$Exe,[Parameter(Mandatory)][string]$Fixture)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$taskProject=Split-Path -Parent $PSScriptRoot
if(Test-Path -LiteralPath $Fixture){Remove-Item -LiteralPath $Fixture -Recurse -Force}
$taskGame=Join-Path $Fixture 'game';$taskPkg=Join-Path $Fixture 'package'
[void][IO.Directory]::CreateDirectory((Join-Path $taskGame 'AI_TacticsMod'))
[void][IO.Directory]::CreateDirectory((Join-Path $taskPkg 'AI_TacticsMod\build\input_package'))
[IO.File]::WriteAllText((Join-Path $taskGame 'SAN14CE_KO.exe'),'fixture')
# Package = distribution manager scripts as build_share packages them (UTF-8 with BOM).
function Copy-Script([string]$From,[string]$To){[IO.File]::WriteAllText($To,[IO.File]::ReadAllText($From,[Text.Encoding]::UTF8),(New-Object Text.UTF8Encoding($true)))}
Copy-Script (Join-Path $taskProject 'distribution\Manage.ps1') (Join-Path $taskPkg 'AI_TacticsMod\관리.ps1')
Copy-Script (Join-Path $taskProject 'distribution\SplitManage.ps1') (Join-Path $taskPkg 'AI_TacticsMod\SplitManage.ps1')
Copy-Script (Join-Path $taskProject 'distribution\FeatureSelect.ps1') (Join-Path $taskPkg 'AI_TacticsMod\FeatureSelect.ps1')
Copy-Item (Join-Path $taskProject 'build\input_package\manifest.json') (Join-Path $taskPkg 'AI_TacticsMod\build\input_package\manifest.json')
[IO.File]::WriteAllText((Join-Path $taskPkg 'evil.dll'),'x')
[IO.File]::WriteAllLines((Join-Path $taskPkg 'AI_TacticsMod\package_files.txt'),
    [string[]]@('AI_TacticsMod/관리.ps1','AI_TacticsMod/SplitManage.ps1','AI_TacticsMod/FeatureSelect.ps1','AI_TacticsMod/build/input_package/manifest.json','evil.dll','../outside.txt','AI_TacticsMod/기능설정.ini'),
    (New-Object Text.UTF8Encoding($false)))
[string]$taskIni=Join-Path $taskGame 'AI_TacticsMod\기능설정.ini'
# Pre-v12.8 file: no split keys; recruit=0 and injury=0 must be inherited by their split parts.
[IO.File]::WriteAllText($taskIni,"[groups]`r`ncombat=0`r`n[personnel]`r`nrecruit=0`r`ninjury=0`r`nawards=0`r`n[features]`r`nunified=1`r`n[diagnostics]`r`nlayout_wait=1`r`n",[Text.Encoding]::Unicode)
$taskAsm=[Reflection.Assembly]::Load([IO.File]::ReadAllBytes($Exe))
$taskType=$taskAsm.GetType('AiTacticsInstaller.FeatureIni');$taskItems=$taskAsm.GetType('AiTacticsInstaller.Catalog').GetField('Items').GetValue($null)
$taskLoad=$taskType.GetMethod('Load');$taskChecks=0
function Check([bool]$Value,[string]$Label){if(-not $Value){throw $Label};$script:taskChecks++}
$taskDoc=$taskLoad.Invoke($null,[object[]]@($taskIni))
$taskValues=New-Object 'System.Collections.Generic.Dictionary[string,int]'
foreach($f in $taskItems){$taskValues[$f.Id]=$taskDoc.Value($f)}
Check ($taskValues['groups.combat'] -eq 0 -and $taskValues['officer_ui.unified'] -eq 1 -and $taskValues['groups.performance'] -eq 1) 'legacy/section/default precedence'
Check ($taskValues['personnel.recruit_ai'] -eq 0 -and $taskValues['personnel.injury_herbal'] -eq 0 -and $taskValues['extensions.captive_dialog'] -eq 1) 'old files inherit split switches'
# Package copy: listed files only, no root DLL, no traversal, never the user's settings.
$taskFolders=$taskAsm.GetType('AiTacticsInstaller.Folders')
$taskList=$taskFolders.GetMethod('PackageFiles').Invoke($null,[object[]]@([string]$taskPkg))
Check ($taskList.Count -eq 4) 'package list filtered to four safe files'
$taskCopied=$taskFolders.GetMethod('Copy').Invoke($null,[object[]]@([string]$taskPkg,[string]$taskGame,$taskList))
Check ($taskCopied -eq 4 -and -not (Test-Path (Join-Path $taskGame 'evil.dll')) -and -not (Test-Path (Join-Path $Fixture 'outside.txt'))) 'copy stays inside the game folder'
Check ($taskFolders.GetMethod('Changed').Invoke($null,[object[]]@([string]$taskPkg,[string]$taskGame,$taskList)) -eq 0) 'copied files compare equal'
# Guarded manager rewrite keeps every value and unowned sections.
$taskValues['domestic.region_officers']=0;$taskValues['groups.combat']=1;$taskValues['officer_ui.unified']=0
$taskDoc.Save($taskIni,$taskValues)
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $taskGame 'AI_TacticsMod\관리.ps1') -Action Select -GameDirectory $taskGame -NonInteractive | Out-Null
Check ($LASTEXITCODE -eq 0) '관리.ps1 Select'
$taskAfter=$taskLoad.Invoke($null,[object[]]@($taskIni))
foreach($f in $taskItems){Check ($taskAfter.Value($f) -eq $taskValues[$f.Id]) ('value kept after manager rewrite: '+$f.Id)}
Check (([IO.File]::ReadAllText($taskIni)) -match 'layout_wait=1') 'unowned section kept'
Check (([IO.File]::ReadAllText($taskIni)) -match 'recruit_ai=0') 'split key written explicitly'
# Form: dependency chaining and rendering.
$taskForm=[Activator]::CreateInstance($taskAsm.GetType('AiTacticsInstaller.MainForm'),[object[]]@([string]$taskPkg,[string]$taskGame))
$taskForm.Show();[Windows.Forms.Application]::DoEvents()
$taskFlags=[Reflection.BindingFlags]'NonPublic,Instance'
$taskState=$taskForm.GetType().GetField('values',$taskFlags).GetValue($taskForm)
$taskState['groups.convenience']=0;$taskState['personnel.awards']=0;$taskState['personnel.awards_ai']=0
$taskChild=$taskAsm.GetType('AiTacticsInstaller.Catalog').GetMethod('ById').Invoke($null,[object[]]@('personnel.awards_ai'))
$taskForm.GetType().GetMethod('EnableChain',$taskFlags).Invoke($taskForm,[object[]]@($taskChild,$true))
Check ($taskState['personnel.awards_ai'] -eq 1 -and $taskState['personnel.awards'] -eq 1 -and $taskState['groups.convenience'] -eq 1) 'child enables parent and group'
$taskEffective=$taskForm.GetType().GetMethod('Effective',$taskFlags)
$taskState['personnel.awards']=0
Check (-not $taskEffective.Invoke($taskForm,[object[]]@($taskChild))) 'child is not effective without parent'
$taskShow=$taskForm.GetType().GetMethod('ShowTab',$taskFlags)
$taskForm.GetType().GetMethod('LoadValues',$taskFlags).Invoke($taskForm,$null)
$taskIndex=0
foreach($tab in @('전투','편의성','성능','게임성','작업 기록')){
    $taskShow.Invoke($taskForm,[object[]]@($tab));[Windows.Forms.Application]::DoEvents()
    $taskBitmap=New-Object Drawing.Bitmap $taskForm.Width,$taskForm.Height
    $taskForm.DrawToBitmap($taskBitmap,(New-Object Drawing.Rectangle 0,0,$taskForm.Width,$taskForm.Height))
    $taskBitmap.Save((Join-Path $Fixture ('tab'+$taskIndex+'.png')));$taskBitmap.Dispose();$taskIndex++;$taskChecks++
}
# New display feature: inspect its actual scrolled row in the hidden form.
$taskShow.Invoke($taskForm,[object[]]@('편의성'));[Windows.Forms.Application]::DoEvents()
$taskRows=$taskForm.GetType().GetField('rows',$taskFlags).GetValue($taskForm)
$taskPreview=@($taskRows | Where-Object {$_.Item.Id -eq 'officer_ui.damage_preview'})
Check ($taskPreview.Count -eq 1) 'damage preview exists once in convenience tab'
Check ($taskPreview[0].Item.Group -eq 'convenience' -and $taskPreview[0].Item.Default -eq 1) 'damage preview group/default'
$taskPage=$taskForm.GetType().GetField('pages',$taskFlags).GetValue($taskForm)['편의성']
$taskPage.ScrollControlIntoView($taskPreview[0]);[Windows.Forms.Application]::DoEvents()
$taskBitmap=New-Object Drawing.Bitmap $taskForm.Width,$taskForm.Height
$taskForm.DrawToBitmap($taskBitmap,(New-Object Drawing.Rectangle 0,0,$taskForm.Width,$taskForm.Height))
$taskBitmap.Save((Join-Path $Fixture 'damage_preview.png'));$taskBitmap.Dispose()
$taskShow.Invoke($taskForm,[object[]]@('게임성'));[Windows.Forms.Application]::DoEvents()
$taskArea=@($taskRows | Where-Object {$_.Item.Id -eq 'stratagem.building_destruction_all'})
Check ($taskArea.Count -eq 1) 'area building destruction exists exactly once'
Check ($taskArea[0].Item.Tab -eq '게임성' -and $taskArea[0].Item.Group -eq 'convenience' -and $taskArea[0].Item.Default -eq 0) 'area destruction gameplay owner and opt-in default'
$taskPage=$taskForm.GetType().GetField('pages',$taskFlags).GetValue($taskForm)['게임성']
$taskPage.ScrollControlIntoView($taskArea[0]);[Windows.Forms.Application]::DoEvents()
$taskBitmap=New-Object Drawing.Bitmap $taskForm.Width,$taskForm.Height
$taskForm.DrawToBitmap($taskBitmap,(New-Object Drawing.Rectangle 0,0,$taskForm.Width,$taskForm.Height))
$taskBitmap.Save((Join-Path $Fixture 'building_destruction.png'));$taskBitmap.Dispose()
$taskForm.Hide();$taskForm.Dispose()
'{"status":"installer_gui_roundtrip_passed","checks":'+$taskChecks+',"ui_clicked":false}'
