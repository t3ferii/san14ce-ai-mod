# user 2026-11-03: GUI/PS migrate known old keys without losing raw/custom data.
param([string]$Exe)
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
if(-not $Exe){$Exe=Join-Path $project 'build\development\installer\AI모드_설치관리자.exe'}
$asm=[Reflection.Assembly]::LoadFile([IO.Path]::GetFullPath($Exe))
$flags=[Reflection.BindingFlags]'Public,NonPublic,Static,Instance'
$catalog=$asm.GetType('AiTacticsInstaller.Catalog').GetField('Items',$flags).GetValue($null)
$load=$asm.GetType('AiTacticsInstaller.FeatureIni').GetMethod('Load',$flags)
. (Join-Path $project 'distribution\FeatureSelect.ps1')
$root=Join-Path $project ('build\feature_migration_'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($root)
$checks=0
function Check($Value,$Label){if(-not $Value){throw ('FAIL '+$Label)};$script:checks++}
try {
    foreach($canonical in @('',"[performance]`r`nfast_menu=0`r`n")) {
        $text="[game_flow]`r`nfast_menu=1`r`n[performance]`r`nstrategy_batch=1`r`n"+$canonical+"[ccz_appearance]`r`nprofile=2,70,90,3,4,5`r`n[custom]`r`nuser_key=한글 보존`r`n"
        $expected=if($canonical){0}else{1}
        $gui=Join-Path $root 'gui.ini';$ps=Join-Path $root 'ps.ini'
        foreach($f in @($gui,$ps)){[IO.File]::WriteAllText($f,$text,[Text.Encoding]::Unicode)}
        $doc=$load.Invoke($null,[object[]]@($gui.psobject.BaseObject));$values=New-Object 'Collections.Generic.Dictionary[string,int]'
        foreach($item in $catalog){$values[$item.Id]=$doc.Value($item)}
        Check ($values['performance.fast_menu'] -eq $expected) 'GUI alias/canonical precedence'
        $doc.Save($gui,$values)
        $ini=Read-FeatureIni $ps;$psvalues=Get-FeatureValues $ini
        Check ($psvalues['performance.fast_menu'] -eq $expected) 'PS alias/canonical precedence'
        Write-FeatureIni $ps $psvalues $ini
        foreach($file in @($gui,$ps)) {
            $saved=Read-FeatureIni $file
            Check (-not $saved['game_flow'].Contains('fast_menu')) 'old section alias removed'
            Check (-not $saved['performance'].Contains('strategy_batch')) 'removed feature is not revived'
            Check ($saved['ccz_appearance']['profile'] -eq '2,70,90,3,4,5') 'raw appearance preserved'
            Check ($saved['custom']['user_key'] -eq '한글 보존') 'unknown user key preserved'
            Check (([regex]::Matches([IO.File]::ReadAllText($file),'(?m)^\[performance\]\r?$')).Count -eq 1) 'one canonical section'
        }
        foreach($item in $catalog){Check ((Get-FeatureValues (Read-FeatureIni $gui))[$item.Id] -eq (Get-FeatureValues (Read-FeatureIni $ps))[$item.Id]) ('GUI/PS same '+$item.Id)}
    }
    @{status='feature_migration_passed';checks=$checks;actual_settings_changed=$false} | ConvertTo-Json -Compress
} finally {
    $resolved=[IO.Path]::GetFullPath($root);$allowed=[IO.Path]::GetFullPath((Join-Path $project 'build'))+[IO.Path]::DirectorySeparatorChar
    if($resolved.StartsWith($allowed) -and (Split-Path -Leaf $resolved) -like 'feature_migration_*'){Remove-Item -LiteralPath $resolved -Recurse -Force}
}
