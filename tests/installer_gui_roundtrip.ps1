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
[IO.File]::WriteAllText($taskIni,"[groups]`r`ncombat=0`r`n[personnel]`r`nrecruit=0`r`ninjury=0`r`nawards=0`r`n[features]`r`nunified=1`r`n[diagnostics]`r`nlayout_wait=1`r`n[extensions]`r`ncaptive_dialog=1`r`n",[Text.Encoding]::Unicode)   # local10 2026-11-06: the retired captive_dialog key of an old file must be ignored without an error
$taskAsm=[Reflection.Assembly]::Load([IO.File]::ReadAllBytes($Exe))
$taskType=$taskAsm.GetType('AiTacticsInstaller.FeatureIni');$taskItems=$taskAsm.GetType('AiTacticsInstaller.Catalog').GetField('Items').GetValue($null)
$taskLoad=$taskType.GetMethod('Load');$taskChecks=0
function Check([bool]$Value,[string]$Label){if(-not $Value){throw $Label};$script:taskChecks++}
$taskDoc=$taskLoad.Invoke($null,[object[]]@($taskIni.psobject.BaseObject))
$taskValues=New-Object 'System.Collections.Generic.Dictionary[string,int]'
foreach($f in $taskItems){$taskValues[$f.Id]=$taskDoc.Value($f)}
Check ($taskValues['groups.combat'] -eq 0 -and $taskValues['officer_ui.unified'] -eq 0 -and $taskValues['groups.performance'] -eq 1) 'user decision 2026-11-03: retired unified loads OFF while other legacy/default precedence remains'
Check ($taskValues['personnel.recruit_ai'] -eq 0 -and $taskValues['personnel.injury_herbal'] -eq 0 -and $taskValues['extensions.traits9'] -eq 1) 'old files inherit split switches'
Check (-not $taskValues.ContainsKey('extensions.captive_dialog')) 'captive_dialog was removed from the catalog (an old ini key is ignored)'
# Package copy: listed files only, no root DLL, no traversal, never the user's settings.
$taskFolders=$taskAsm.GetType('AiTacticsInstaller.Folders')
$taskList=$taskFolders.GetMethod('PackageFiles').Invoke($null,[object[]]@([string]$taskPkg.psobject.BaseObject))
Check ($taskList.Count -eq 4) 'package list filtered to four safe files'
$taskCopied=$taskFolders.GetMethod('Copy').Invoke($null,[object[]]@([string]$taskPkg.psobject.BaseObject,[string]$taskGame.psobject.BaseObject,$taskList.psobject.BaseObject))
Check ($taskCopied -eq 4 -and -not (Test-Path (Join-Path $taskGame 'evil.dll')) -and -not (Test-Path (Join-Path $Fixture 'outside.txt'))) 'copy stays inside the game folder'
Check ($taskFolders.GetMethod('Changed').Invoke($null,[object[]]@([string]$taskPkg.psobject.BaseObject,[string]$taskGame.psobject.BaseObject,$taskList.psobject.BaseObject)) -eq 0) 'copied files compare equal'
# Guarded manager rewrite keeps every value and unowned sections.
$taskValues['domestic.region_officers']=0;$taskValues['groups.combat']=1;$taskValues['officer_ui.unified']=0
$taskDoc.Save($taskIni,$taskValues)
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $taskGame 'AI_TacticsMod\관리.ps1') -Action Select -GameDirectory $taskGame -NonInteractive | Out-Null
Check ($LASTEXITCODE -eq 0) '관리.ps1 Select'
$taskAfter=$taskLoad.Invoke($null,[object[]]@($taskIni.psobject.BaseObject))
foreach($f in $taskItems){Check ($taskAfter.Value($f) -eq $taskValues[$f.Id]) ('value kept after manager rewrite: '+$f.Id)}
Check (([IO.File]::ReadAllText($taskIni)) -match 'layout_wait=1') 'unowned section kept'
Check (([IO.File]::ReadAllText($taskIni)) -match 'recruit_ai=0') 'split key written explicitly'
# v15.1: terrain_diag_mode alone is a four-value diagnostic setting; ordinary switches stay binary.
[string]$taskTerrainIni=Join-Path $Fixture 'terrain-diag.ini'
foreach($taskMode in 0..3){
    [IO.File]::WriteAllText($taskTerrainIni,"[diagnostics]`r`nterrain_diag_mode=$taskMode`r`nenabled=7`r`nextra_key=keep`r`n[real_estate]`r`nterrain_edit=-2`r`n",[Text.Encoding]::Unicode)
    $taskTerrainDoc=$taskLoad.Invoke($null,[object[]]@($taskTerrainIni.psobject.BaseObject))
    $taskTerrainValues=New-Object 'System.Collections.Generic.Dictionary[string,int]'
    foreach($f in $taskItems){$taskTerrainValues[$f.Id]=$taskTerrainDoc.Value($f)}
    Check ($taskTerrainValues['diagnostics.terrain_diag_mode'] -eq $taskMode) ('GUI loads terrain mode '+$taskMode)
    Check ($taskTerrainValues['diagnostics.enabled'] -eq 1 -and $taskTerrainValues['real_estate.terrain_edit'] -eq 1) ('GUI keeps binary normalization '+$taskMode)
    $taskTerrainDoc.Save($taskTerrainIni,$taskTerrainValues)
    $taskTerrainRaw=[IO.File]::ReadAllText($taskTerrainIni)
    Check (([regex]::Matches($taskTerrainRaw,'(?m)^\[diagnostics\]\r?$')).Count -eq 1 -and ([regex]::Matches($taskTerrainRaw,'(?m)^terrain_diag_mode=')).Count -eq 1) ('GUI avoids duplicate terrain key/section '+$taskMode)
    Check ($taskTerrainRaw -match ('(?m)^terrain_diag_mode='+$taskMode+'\r?$') -and $taskTerrainRaw -match '(?m)^extra_key=keep\r?$') ('GUI saves mode and preserves user key '+$taskMode)
}
# 사용자 결정 2026-10-28: 충돌 옵션 사용 중지. Legacy 1 and injected values must save as 0.
$taskCatalog=$taskAsm.GetType('AiTacticsInstaller.Catalog')
$taskRetired=@($taskCatalog.GetField('Retired').GetValue($null))
$taskRetiredSet=$taskCatalog.GetField('Retired').GetValue($null)
$taskExpectedRetired=@('combat.departures','combat.group_advance','combat.support','diplomacy.diplomacy_overinvest','officer_ui.unified')
foreach($id in $taskExpectedRetired){Check ($taskRetiredSet.Contains($id)) ('retired catalog preserves prior setting: '+$id)}
# 사용자 결정 2026-11-03: the four newly retired settings remain covered by the catalog-driven check.
foreach($id in @('ccz.ccz_hide_model','ccz.ccz_depth','ccz.ccz_far_keep','performance.timer_1ms')){Check ($taskRetiredSet.Contains($id)) ('user decision 2026-11-03: retired catalog includes: '+$id)}
[string]$taskRetiredIni=Join-Path $Fixture 'retired.ini'
[IO.File]::WriteAllText($taskRetiredIni,"[features]`r`ndepartures=1`r`ngroup_advance=1`r`nsupport=1`r`nunified=1`r`n[combat]`r`nmission_v2=0`r`ndepartures=1`r`ngroup_advance=1`r`nsupport=1`r`n[officer_ui]`r`nunified=1`r`n",[Text.Encoding]::Unicode)
$taskRetiredDoc=$taskLoad.Invoke($null,[object[]]@($taskRetiredIni.psobject.BaseObject))
foreach($f in $taskItems | Where-Object {$_.Id -in $taskRetired}){
    Check ($taskRetiredDoc.Value($f) -eq 0) ('GUI ignores legacy 1: '+$f.Id)
    $taskValues[$f.Id]=1
}
$taskRetiredDoc.Save($taskRetiredIni,$taskValues)
$taskRaw=[IO.File]::ReadAllText($taskRetiredIni)
foreach($key in @('departures','group_advance','support','unified')){Check ($taskRaw -match ('(?m)^'+$key+'=0\r?$')) ('user decision 2026-11-03: retired setting key remains and saves OFF: '+$key)}
# Form: dependency chaining and rendering.
$taskForm=[Activator]::CreateInstance($taskAsm.GetType('AiTacticsInstaller.MainForm'),[object[]]@([string]$taskPkg.psobject.BaseObject,[string]$taskGame.psobject.BaseObject))
$taskForm.Show();[Windows.Forms.Application]::DoEvents()
$taskFlags=[Reflection.BindingFlags]'NonPublic,Instance'
# User 2026-11-02: local/public status comparisons are deterministic and never need GitHub.
$taskUpdates=$taskAsm.GetType('AiTacticsInstaller.Updates').GetMethod('StatusText')
foreach($taskCase in @(
    @('v15.1',$true,'v15','로컬 개발판 v15.1 · 공개 v15'),
    @('v15',$false,'v15','공개 최신판 v15'),
    @('v15.1',$true,'v15.2','● 업데이트 가능 · 공개 v15.2'),
    @('v15.1',$true,'v15.1','로컬 개발판 v15.1 · 공개 v15.1'),
    @('v15.1',$false,'v15','설치판 v15.1 · 공개 v15'),
    @('v15.1',$true,$null,'로컬 개발판 · 공개판 확인 불가')
)){
    $taskStatusInstalled=[string]$taskCase[0];$taskStatusPublic=if($null -eq $taskCase[2]){$null}else{[string]$taskCase[2]}
    Check ($taskUpdates.Invoke($null,[object[]]@($taskStatusInstalled.psobject.BaseObject,[bool]$taskCase[1],$(if($null -eq $taskStatusPublic){$null}else{$taskStatusPublic.psobject.BaseObject}))) -eq $taskCase[3]) ('update status '+$taskCase[3])
}
$taskManifest=Join-Path $taskGame 'AI_TacticsMod\build\input_package\manifest.json'
$taskOriginalManifest=[IO.File]::ReadAllText($taskManifest)
try {
    foreach($taskCase in @(@('v15.1',$true,$true),@('v15',$true,$false),@('v15.1',$false,$false))){
        [IO.File]::WriteAllText($taskManifest,('{"release_version":"'+$taskCase[0]+'","local_diagnostic_only":'+([string]$taskCase[1]).ToLowerInvariant()+'}'))
        Check ($taskForm.GetType().GetMethod('InstalledLocalDiagnostic',$taskFlags).Invoke($taskForm,[object[]]@('v15.1')) -eq $taskCase[2]) 'local diagnostic metadata must match installed version'
    }
} finally {[IO.File]::WriteAllText($taskManifest,$taskOriginalManifest)}
$taskFar=@($taskItems|Where-Object {$_.Id -eq 'ccz.ccz_far_keep'})[0]
Check ($taskFar.Default -eq 0 -and $taskFar.Tab -eq '조조전' -and $taskFar.Requires -eq 'ccz.ccz_sprites') 'far keep opt-in catalog'
# 사용자 결정 2026-11-03: 원거리 그림 유지 제거, 지형 가림은 병종 그림에 통합
foreach($taskFarValue in @(0,1)){
    $taskFarIni=Join-Path $Fixture 'far-keep.ini'
    [IO.File]::WriteAllText($taskFarIni,"[ccz]`r`nccz_far_keep=$taskFarValue`r`nccz_depth=1`r`ncustom=kept`r`n",[Text.Encoding]::Unicode)
    $taskFarDoc=$taskLoad.Invoke($null,[object[]]@($taskFarIni.psobject.BaseObject))
    $taskFarValues=New-Object 'System.Collections.Generic.Dictionary[string,int]'
    foreach($f in $taskItems){$taskFarValues[$f.Id]=$taskFarDoc.Value($f)}
    $taskFarDoc.Save($taskFarIni,$taskFarValues)
    . (Join-Path $taskProject 'distribution\FeatureSelect.ps1')
    $taskFarPS=Get-FeatureValues (Read-FeatureIni $taskFarIni)
    Check ($taskFarPS['ccz.ccz_far_keep'] -eq 0 -and $taskFarPS['ccz.ccz_depth'] -eq 0) 'retired far keep/depth always saved 0 (user 2026-11-03)'
    Write-FeatureIni $taskFarIni $taskFarPS (Read-FeatureIni $taskFarIni)
    $taskFarReload=$taskLoad.Invoke($null,[object[]]@($taskFarIni.psobject.BaseObject))
    Check ($taskFarReload.Value($taskFar) -eq 0 -and [IO.File]::ReadAllText($taskFarIni).Contains('custom=kept')) 'retired far keep PS to GUI reload is 0; custom key preserved (user 2026-11-03)'
}
$taskPopulation=@($taskItems|Where-Object {$_.Id -eq 'domestic.population_rate'})[0]
foreach($taskPopulationInput in @(1,2,3,4,0,9)){
    $taskPopulationExpected=[Math]::Max(1,[Math]::Min(4,$taskPopulationInput))
    $taskPopulationIni=Join-Path $Fixture 'population-rate.ini'
    [IO.File]::WriteAllText($taskPopulationIni,"[domestic]`r`npopulation_rate=$taskPopulationInput`r`n",[Text.Encoding]::Unicode)
    $taskPopulationDoc=$taskLoad.Invoke($null,[object[]]@($taskPopulationIni.psobject.BaseObject))
    Check ($taskPopulationDoc.Value($taskPopulation) -eq $taskPopulationExpected) ('GUI population_rate clamps '+$taskPopulationInput+' to '+$taskPopulationExpected)
    $taskPopulationValues=New-Object 'System.Collections.Generic.Dictionary[string,int]'
    foreach($f in $taskItems){$taskPopulationValues[$f.Id]=$taskPopulationDoc.Value($f)}
    $taskPopulationPS=Get-FeatureValues (Read-FeatureIni $taskPopulationIni)
    Check ($taskPopulationPS['domestic.population_rate'] -eq $taskPopulationExpected) ('PS population_rate clamps '+$taskPopulationInput+' to '+$taskPopulationExpected)
    Write-FeatureIni $taskPopulationIni $taskPopulationPS (Read-FeatureIni $taskPopulationIni)
    Check ((Read-FeatureIni $taskPopulationIni)['domestic']['population_rate'] -eq [string]$taskPopulationExpected) ('population_rate '+$taskPopulationExpected+' survives installer roundtrip')
}
# 2026-11-05 (user): merit stage 1/2 troops choice (1..4 = 1500/2000/2500/3000, default 4) and stage-3 friendship switch (default on).
$taskMeritTroops=@($taskItems|Where-Object {$_.Id -eq 'personnel.merit_troops_level'})[0]
$taskMeritFriend=@($taskItems|Where-Object {$_.Id -eq 'personnel.merit_friendship'})[0]
foreach($taskMeritInput in @(1,2,3,4,0,9)){
    $taskMeritExpected=[Math]::Max(1,[Math]::Min(4,$taskMeritInput))
    $taskMeritIni=Join-Path $Fixture 'merit-troops.ini'
    [IO.File]::WriteAllText($taskMeritIni,"[personnel]`r`nmerit_troops_level=$taskMeritInput`r`nmerit_friendship=0`r`n",[Text.Encoding]::Unicode)
    $taskMeritDoc=$taskLoad.Invoke($null,[object[]]@($taskMeritIni.psobject.BaseObject))
    Check ($taskMeritDoc.Value($taskMeritTroops) -eq $taskMeritExpected -and $taskMeritDoc.Value($taskMeritFriend) -eq 0) ('GUI merit_troops_level clamps '+$taskMeritInput+' to '+$taskMeritExpected+', friendship off kept')
    $taskMeritPS=Get-FeatureValues (Read-FeatureIni $taskMeritIni)
    Check ($taskMeritPS['personnel.merit_troops_level'] -eq $taskMeritExpected -and $taskMeritPS['personnel.merit_friendship'] -eq 0) ('PS merit_troops_level clamps '+$taskMeritInput+' to '+$taskMeritExpected)
    Write-FeatureIni $taskMeritIni $taskMeritPS (Read-FeatureIni $taskMeritIni)
    $taskMeritBack=Read-FeatureIni $taskMeritIni
    Check ($taskMeritBack['personnel']['merit_troops_level'] -eq [string]$taskMeritExpected -and $taskMeritBack['personnel']['merit_friendship'] -eq '0') ('merit_troops_level '+$taskMeritExpected+' and friendship survive installer roundtrip')
}
$taskMeritDefault=Join-Path $Fixture 'merit-default.ini'
[IO.File]::WriteAllText($taskMeritDefault,"[personnel]`r`nmerit=1`r`n",[Text.Encoding]::Unicode)
$taskMeritDefaultDoc=$taskLoad.Invoke($null,[object[]]@($taskMeritDefault.psobject.BaseObject))
Check ($taskMeritDefaultDoc.Value($taskMeritTroops) -eq 4 -and $taskMeritDefaultDoc.Value($taskMeritFriend) -eq 1) 'merit troops default 3000 and friendship default on when keys are absent'
$taskState=$taskForm.GetType().GetField('values',$taskFlags).GetValue($taskForm)
$taskState['groups.convenience']=0;$taskState['personnel.awards']=0;$taskState['personnel.awards_ai']=0
$taskChild=$taskAsm.GetType('AiTacticsInstaller.Catalog').GetMethod('ById').Invoke($null,[object[]]@('personnel.awards_ai'))
$taskForm.GetType().GetMethod('EnableChain',$taskFlags).Invoke($taskForm.psobject.BaseObject,[object[]]@($taskChild.psobject.BaseObject,$true))
Check ($taskState['personnel.awards_ai'] -eq 1 -and $taskState['personnel.awards'] -eq 1 -and $taskState['groups.convenience'] -eq 1) 'child enables parent and group'
$taskEffective=$taskForm.GetType().GetMethod('Effective',$taskFlags)
$taskState['personnel.awards']=0
Check (-not $taskEffective.Invoke($taskForm.psobject.BaseObject,[object[]]@($taskChild.psobject.BaseObject))) 'child is not effective without parent'
$taskShow=$taskForm.GetType().GetMethod('ShowTab',$taskFlags)
$taskForm.GetType().GetMethod('LoadValues',$taskFlags).Invoke($taskForm.psobject.BaseObject,$null)
$taskIndex=0
foreach($tab in @('전투','편의성','성능','게임성','조조전','기타(개발용)','출처','업데이트','작업 기록')){
    $taskShow.Invoke($taskForm.psobject.BaseObject,[object[]]@($tab.psobject.BaseObject));[Windows.Forms.Application]::DoEvents()
    $taskBitmap=New-Object Drawing.Bitmap $taskForm.Width,$taskForm.Height
    $taskForm.DrawToBitmap($taskBitmap,(New-Object Drawing.Rectangle 0,0,$taskForm.Width,$taskForm.Height))
    $taskBitmap.Save((Join-Path $Fixture ('tab'+$taskIndex+'.png')));$taskBitmap.Dispose();$taskIndex++;$taskChecks++
}
# 사용자 결정 2026-10-30: 최소/기본 창에서 출처 탭 접근·문구·줄바꿈·하단 경계를 확인한다.
$taskPages=$taskForm.GetType().GetField('pages',$taskFlags).GetValue($taskForm)
$taskCreditPage=$taskPages['출처']
$taskCreditLines=@([IO.File]::ReadAllLines((Join-Path $taskProject 'docs\CREDITS.md'),[Text.Encoding]::UTF8) | Where-Object {$_.StartsWith('- ')})
Check ($taskCreditPage.Controls.Count -eq $taskCreditLines.Count+1) 'credits introduction and every line'
Check ($taskCreditPage.Controls[0].Text -eq '다른 모드에서 가져온 기능입니다. 각 기능 설명 끝에도 출처를 표시합니다.') 'credits introduction exact text'
for($i=0;$i -lt $taskCreditLines.Count;$i++){Check ($taskCreditPage.Controls[$i+1].Text -eq $taskCreditLines[$i]) ('canonical credits line '+$i)}
foreach($taskSize in @((New-Object Drawing.Size 800,580),(New-Object Drawing.Size 900,720))){
    $taskForm.ClientSize=$taskSize
    $taskShow.Invoke($taskForm.psobject.BaseObject,[object[]]@('출처'));[Windows.Forms.Application]::DoEvents()
    $taskTabMap=$taskForm.GetType().GetField('tabButtons',$taskFlags).GetValue($taskForm)
    foreach($taskTab in $taskTabMap.Values){Check ($taskTab.Visible -and $taskTab.Bounds.Right -le $taskTab.Parent.ClientSize.Width -and $taskTab.Bounds.Bottom -le $taskTab.Parent.ClientSize.Height) ('tab fully inside strip: '+$taskTab.Text+'/'+$taskSize.Width)}
    $taskCreditSummary=$taskForm.Controls.Find('creditsSummary',$true)[0]
    Check ($taskCreditSummary.Visible -and $taskCreditSummary.Bottom -le $taskCreditSummary.Parent.ClientSize.Height) 'credits summary inside header'
    Check ($taskCreditSummary.Text -eq '출처 표기: 삼갤 통합패치(180.71·175.115 유동) · 삼갤 주딱 · 112.151 유동(삼봉이정도전) · 조조전(KOEI)·신조조전 — 「출처」 탭 참고') 'all three sources in header'
    $taskRequired=[Windows.Forms.TextRenderer]::MeasureText($taskCreditSummary.Text,$taskCreditSummary.Font,(New-Object Drawing.Size $taskCreditSummary.Width,0),([Windows.Forms.TextFormatFlags]::WordBreak -bor [Windows.Forms.TextFormatFlags]::TextBoxControl))
    Check ($taskCreditSummary.Height -ge $taskRequired.Height) 'credits summary height fits wrapped text'
    $taskLastBottom=0
    foreach($taskLabel in $taskCreditPage.Controls){
        Check ($taskLabel.Top -ge $taskLastBottom -and $taskLabel.Right -le $taskCreditPage.ClientSize.Width) 'credits rows do not overlap or clip horizontally'
        $taskRequired=[Windows.Forms.TextRenderer]::MeasureText($taskLabel.Text,$taskLabel.Font,(New-Object Drawing.Size $taskLabel.Width,0),([Windows.Forms.TextFormatFlags]::WordBreak -bor [Windows.Forms.TextFormatFlags]::TextBoxControl))
        Check ($taskLabel.Height -ge $taskRequired.Height) 'credits row height fits wrapped text'
        $taskLastBottom=$taskLabel.Bottom
    }
    $taskCreditPage.ScrollControlIntoView($taskCreditPage.Controls[10]);[Windows.Forms.Application]::DoEvents()
    Check ($taskCreditPage.Controls[10].Bottom -le $taskCreditPage.ClientSize.Height) 'last credit reachable by scrolling'
    $taskBitmap=New-Object Drawing.Bitmap $taskForm.Width,$taskForm.Height
    $taskForm.DrawToBitmap($taskBitmap,(New-Object Drawing.Rectangle 0,0,$taskForm.Width,$taskForm.Height))
    $taskBitmap.Save((Join-Path $Fixture ('credits_'+$taskSize.Width+'.png')));$taskBitmap.Dispose()
    $taskCreditPage.AutoScrollPosition=New-Object Drawing.Point 0,0
}
# New display feature: inspect its actual scrolled row in the hidden form.
$taskShow.Invoke($taskForm.psobject.BaseObject,[object[]]@('편의성'));[Windows.Forms.Application]::DoEvents()
$taskRows=$taskForm.GetType().GetField('rows',$taskFlags).GetValue($taskForm)
$taskSync=$taskForm.GetType().GetMethod('SyncRows',$taskFlags)
$taskTerrainRow=@($taskRows | Where-Object {$_.Item.Id -eq 'diagnostics.terrain_diag_mode'})
Check ($taskTerrainRow.Count -eq 1 -and $taskTerrainRow[0].Choice.Items.Count -eq 4) 'GUI terrain diagnostic row has four choices'
foreach($taskMode in 0..3){$taskState['diagnostics.terrain_diag_mode']=$taskMode;$taskSync.Invoke($taskForm.psobject.BaseObject,$null);Check ($taskTerrainRow[0].Choice.SelectedIndex -eq $taskMode) ('GUI terrain choice reload '+$taskMode)}
$taskTabs=$taskForm.GetType().GetField('tabButtons',$taskFlags).GetValue($taskForm)
$taskShow.Invoke($taskForm.psobject.BaseObject,[object[]]@('전투'))
foreach($mission in @(0,1)){foreach($master in @(0,1)){
    $taskState['combat.mission_v2']=$mission;$taskState['groups.combat']=$master
    foreach($id in $taskRetired){$taskState[$id]=1}
    $taskSync.Invoke($taskForm.psobject.BaseObject,$null)
    foreach($id in $taskRetired){
        $taskRow=@($taskRows | Where-Object {$_.Item.Id -eq $id})
        Check ($taskRow.Count -eq 0) ('user 2026-11-03: retired row absent: '+$id)
        $taskItem=@($taskItems | Where-Object {$_.Id -eq $id})[0]
        Check (-not $taskEffective.Invoke($taskForm.psobject.BaseObject,[object[]]@($taskItem.psobject.BaseObject))) ('retired cannot become effective: '+$id)
        $taskForm.GetType().GetMethod('EnableChain',$taskFlags).Invoke($taskForm.psobject.BaseObject,[object[]]@($taskItem.psobject.BaseObject,$true))
        Check ($taskState[$id] -eq 0 -and $taskState['groups.combat'] -eq $master -and $taskState['combat.mission_v2'] -eq $mission) ('retired enable-chain preserves other settings: '+$id)
    }
    $taskCombat=@($taskItems | Where-Object {$_.Tab -eq '전투' -and -not $_.IsMaster -and $_.Id -notin $taskRetired})
    $taskCount=@($taskCombat | Where-Object {$taskEffective.Invoke($taskForm.psobject.BaseObject,[object[]]@($_.psobject.BaseObject))}).Count
    Check ($taskTabs['전투'].Text -eq ('전투  '+$taskCount+'/'+$taskCombat.Count)) 'combat count excludes its three retired rows; officer_ui.unified is retired separately'
    $taskMissionItem=@($taskItems | Where-Object {$_.Id -eq 'combat.mission_v2'})[0]
    Check ([bool]$taskEffective.Invoke($taskForm.psobject.BaseObject,[object[]]@($taskMissionItem.psobject.BaseObject)) -eq [bool]($mission -and $master)) 'mission_v2 keeps its normal effective state'
}}
# Exercise the actual bulk-action event handlers without mouse input.
$taskChoices=$taskCatalog.GetField('Choices').GetValue($null)
$taskActions=$taskForm.GetType().GetField('actions',$taskFlags).GetValue($taskForm)
foreach($label in @('모두 켜기','배포 기본값')){
    $taskAction=@($taskActions | Where-Object {$_.Text -eq $label})[0]
    $taskAction.GetType().GetMethod('OnClick',$taskFlags).Invoke($taskAction.psobject.BaseObject,[object[]]@([EventArgs]::Empty))
    foreach($id in $taskRetired){Check ($taskState[$id] -eq 0) ('GUI bulk action stays OFF: '+$label+'/'+$id)}
    foreach($f in $taskItems | Where-Object {$_.Id -notin $taskRetired}){
        $taskExpected=if($label -eq '모두 켜기' -and -not $taskChoices.ContainsKey($f.Id)){1}else{$f.Default}
        Check ($taskState[$f.Id] -eq $taskExpected) ('GUI bulk action preserves other feature: '+$f.Id)
    }
}
$taskForm.GetType().GetMethod('LoadValues',$taskFlags).Invoke($taskForm.psobject.BaseObject,$null)
$taskShow.Invoke($taskForm.psobject.BaseObject,[object[]]@('편의성'))
$taskPreview=@($taskRows | Where-Object {$_.Item.Id -eq 'officer_ui.damage_preview'})
Check ($taskPreview.Count -eq 1) 'damage preview exists once in convenience tab'
Check ($taskPreview[0].Item.Group -eq 'convenience' -and $taskPreview[0].Item.Default -eq 1) 'damage preview group/default'
$taskPage=$taskForm.GetType().GetField('pages',$taskFlags).GetValue($taskForm)['편의성']
$taskPage.ScrollControlIntoView($taskPreview[0]);[Windows.Forms.Application]::DoEvents()
$taskBitmap=New-Object Drawing.Bitmap $taskForm.Width,$taskForm.Height
$taskForm.DrawToBitmap($taskBitmap,(New-Object Drawing.Rectangle 0,0,$taskForm.Width,$taskForm.Height))
$taskBitmap.Save((Join-Path $Fixture 'damage_preview.png'));$taskBitmap.Dispose()
$taskShow.Invoke($taskForm.psobject.BaseObject,[object[]]@('게임성'));[Windows.Forms.Application]::DoEvents()
$taskArea=@($taskRows | Where-Object {$_.Item.Id -eq 'stratagem.building_destruction_all'})
Check ($taskArea.Count -eq 1) 'area building destruction exists exactly once'
Check ($taskArea[0].Item.Tab -eq '게임성' -and $taskArea[0].Item.Group -eq 'convenience' -and $taskArea[0].Item.Default -eq 0) 'area destruction gameplay owner and opt-in default'
$taskPage=$taskForm.GetType().GetField('pages',$taskFlags).GetValue($taskForm)['게임성']
$taskPage.ScrollControlIntoView($taskArea[0]);[Windows.Forms.Application]::DoEvents()
$taskBitmap=New-Object Drawing.Bitmap $taskForm.Width,$taskForm.Height
$taskForm.DrawToBitmap($taskBitmap,(New-Object Drawing.Rectangle 0,0,$taskForm.Width,$taskForm.Height))
$taskBitmap.Save((Join-Path $Fixture 'building_destruction.png'));$taskBitmap.Dispose()
$taskForm.Hide();$taskForm.Dispose()
# Both PS catalogs: input 1, numeric selection, all/default actions, fixed menu status and raw save.
function Read-Host { param($Prompt) return $script:taskInputs.Dequeue() }
foreach($taskSource in @('distribution\FeatureSelect.ps1','FeatureSelect.ps1')){
    $taskMenuScript=Join-Path $Fixture 'retired-menu.ps1'
    Copy-Script (Join-Path $taskProject $taskSource) $taskMenuScript
    . $taskMenuScript
    foreach($taskMode in 0..3){
        [IO.File]::WriteAllText($taskTerrainIni,"[diagnostics]`r`nterrain_diag_mode=$taskMode`r`nenabled=7`r`nextra_key=keep`r`n[real_estate]`r`nterrain_edit=-2`r`n",[Text.Encoding]::Unicode)
        $taskPSValues=Get-FeatureValues (Read-FeatureIni $taskTerrainIni)
        Check ($taskPSValues['diagnostics.terrain_diag_mode'] -eq $taskMode) ('PS loads terrain mode '+$taskSource+'/'+$taskMode)
        Check ($taskPSValues['diagnostics.enabled'] -eq 1 -and $taskPSValues['real_estate.terrain_edit'] -eq 1) ('PS keeps binary normalization '+$taskSource+'/'+$taskMode)
        Write-FeatureIni $taskTerrainIni $taskPSValues (Read-FeatureIni $taskTerrainIni)
        $taskTerrainRaw=[IO.File]::ReadAllText($taskTerrainIni)
        Check (([regex]::Matches($taskTerrainRaw,'(?m)^\[diagnostics\]\r?$')).Count -eq 1 -and ([regex]::Matches($taskTerrainRaw,'(?m)^terrain_diag_mode=')).Count -eq 1) ('PS avoids duplicate terrain key/section '+$taskSource+'/'+$taskMode)
        Check ($taskTerrainRaw -match ('(?m)^terrain_diag_mode='+$taskMode+'\r?$') -and $taskTerrainRaw -match '(?m)^extra_key=keep\r?$') ('PS saves mode and preserves user key '+$taskSource+'/'+$taskMode)
    }
    foreach($mission in @(0,1)){
        $taskPSValues=Get-FeatureValues (Read-FeatureIni $taskRetiredIni)
        $taskPSValues['combat.mission_v2']=$mission
        foreach($id in $taskRetired){Check ($taskPSValues[$id] -eq 0) ('PS loads retired OFF: '+$taskSource+'/'+$id)}
        foreach($id in $taskRetired){$taskPSValues[$id]=1}
        $taskMenuText=(& {Show-FeatureMenu $taskPSValues} 6>&1 | Out-String)
        Check (([regex]::Matches($taskMenuText,'\[사용 중지\]')).Count -eq 0) 'user 2026-11-03: PS menu hides retired features'
        $taskNumbers=@(for($i=0;$i -lt $taskFeatureCatalog.Count;$i++){if($taskFeatureCatalog[$i].Retired){$i+1}}) -join ' '
        foreach($inputText in @($taskNumbers,'a','d')){
            $script:taskInputs=New-Object 'Collections.Generic.Queue[string]'
            $script:taskInputs.Enqueue($inputText);$script:taskInputs.Enqueue('')
            $taskChosen=Select-Features $taskPSValues 6>$null
            foreach($id in $taskRetired){Check ($taskChosen[$id] -eq 0) ('PS selection cannot enable retired: '+$inputText+'/'+$id)}
            if($inputText -eq $taskNumbers){Check ($taskChosen['combat.mission_v2'] -eq $mission) 'PS retired selection leaves mission_v2 unchanged'}
        }
        foreach($id in $taskRetired){$taskPSValues[$id]=1}
        Write-FeatureIni $taskRetiredIni $taskPSValues @{}
        $taskRaw=[IO.File]::ReadAllText($taskRetiredIni)
        foreach($key in @('departures','group_advance','support','unified')){Check ($taskRaw -match ('(?m)^'+$key+'=0\r?$')) ('user decision 2026-11-03: PS retains retired setting key and forces OFF: '+$key)}
    }
}
# User 2026-11-02: game-menu appearance profile is raw structured data, never a checkbox.
# 사용자 결정 2026-11-03: 원거리 그림 유지 제거, 지형 가림은 병종 그림에 통합
$taskAppearanceIni=Join-Path $Fixture 'ccz-appearance.ini'
$taskAppearanceProfile=(@(2,70,90,8209)+@(0..67)+@(1..92|ForEach-Object {255})) -join ','
[IO.File]::WriteAllText($taskAppearanceIni,"[ccz]`r`nccz_block=0`r`nccz_depth=1`r`nccz_far_keep=1`r`n[ccz_appearance]`r`nprofile=older`r`n[custom]`r`ntoken=keep`r`n[ccz_appearance]`r`nprofile=$taskAppearanceProfile`r`n",[Text.Encoding]::Unicode)
$taskRawCatalog=$taskAsm.GetType('AiTacticsInstaller.Catalog').GetField('RawSettings').GetValue($null)
Check ($taskRawCatalog -contains 'ccz_appearance.profile') 'GUI registers raw appearance schema'
for($taskRound=0;$taskRound -lt 3;$taskRound++){
    $taskAppearanceDoc=$taskLoad.Invoke($null,[object[]]@($taskAppearanceIni.psobject.BaseObject))
    $taskAppearanceValues=New-Object 'System.Collections.Generic.Dictionary[string,int]'
    foreach($f in $taskItems){$taskAppearanceValues[$f.Id]=$taskAppearanceDoc.Value($f)}
    $taskAppearanceDoc.Save($taskAppearanceIni,$taskAppearanceValues)
    $taskAppearanceRead=Read-FeatureIni $taskAppearanceIni
    Check ($taskAppearanceRead['ccz_appearance']['profile'] -ceq $taskAppearanceProfile) 'GUI preserves full raw profile'
    Check ($taskRawSettings -contains 'ccz_appearance.profile') 'PowerShell registers same raw schema'
    $taskAppearancePS=Get-FeatureValues $taskAppearanceRead
    Write-FeatureIni $taskAppearanceIni $taskAppearancePS $taskAppearanceRead
    $taskAppearanceRead=Read-FeatureIni $taskAppearanceIni
    Check ($taskAppearanceRead['ccz_appearance']['profile'] -ceq $taskAppearanceProfile -and $taskAppearanceRead['custom']['token'] -eq 'keep') 'PS preserves appearance and unrelated user keys'
    Check ($taskAppearanceRead['ccz']['ccz_block'] -eq '0') 'appearance save keeps ordinary 0/1 switch'
    Check ($taskAppearanceRead['ccz']['ccz_depth'] -eq '0' -and $taskAppearanceRead['ccz']['ccz_far_keep'] -eq '0') 'retired appearance switches always save 0 (user 2026-11-03)'
    $taskAppearanceRaw=[IO.File]::ReadAllText($taskAppearanceIni)
    Check (([regex]::Matches($taskAppearanceRaw,'(?m)^\[ccz_appearance\]\r?$').Count -eq 1) -and ([regex]::Matches($taskAppearanceRaw,'(?m)^profile=2,70,90,8209,').Count -eq 1)) 'appearance section and key not duplicated; full source-cell code preserved'
}
Remove-Item Function:\Read-Host
'{"status":"installer_gui_roundtrip_passed","checks":'+$taskChecks+',"ui_clicked":false}'
