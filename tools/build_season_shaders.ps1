# 4계절 패치: compile the bake/compression shaders offline (fxc, ~3 min for the bake PS) into build\season.
# The DLL loads them from AI_TacticsMod\data\season at runtime (the bake PS is too slow to compile in game).
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$fxc=Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin' -Recurse -Filter fxc.exe | Where-Object {$_.FullName -match '\\x64\\'} | Select-Object -Last 1
if(-not $fxc){throw 'fxc.exe not found (Windows SDK).'}
$out=Join-Path $root 'build\season';[void][IO.Directory]::CreateDirectory($out)
$src=Join-Path $root 'src\season'
$jobs=@(
    @('vs_5_0','bake_vs','season_bake.hlsl','season_bake_vs.cso','/O1'),
    @('ps_5_0','bake_map','season_bake.hlsl','season_bake_ps.cso','/O3'),
    @('cs_5_0','bc1_main','season_bc.hlsl','season_bc1_cs.cso','/O3'),
    @('cs_5_0','bc3_main','season_bc.hlsl','season_bc3_cs.cso','/O3'),
    # R1c rain layer (season patch SeasonRain*/SeasonRainDrops*.hlsl, drawn after the game's lighting pass)
    @('ps_5_0','main','season_rain.hlsl','season_rain_ps.cso','/O3'),
    @('ps_5_0','main','season_rainb.hlsl','season_rainb_ps.cso','/O3'),
    @('vs_5_0','main','season_rain_drops.hlsl','season_drops_vs.cso','/O3','VERTEX_SHADER'),
    @('ps_5_0','main','season_rain_drops.hlsl','season_drops_ps.cso','/O3','PIXEL_SHADER'),
    @('vs_5_0','main','season_rain_dropsb.hlsl','season_dropsb_vs.cso','/O3','VERTEX_SHADER'),
    @('ps_5_0','main','season_rain_dropsb.hlsl','season_dropsb_ps.cso','/O3','PIXEL_SHADER'),
    # local152 cloud layer: the rain shaders' cloud body alone, drawn with our own full-screen vertex shader
    @('ps_5_0','main','season_rain.hlsl','season_cloud_ps.cso','/O3','CLOUD_ONLY'),
    @('ps_5_0','main','season_rainb.hlsl','season_cloudb_ps.cso','/O3','CLOUD_ONLY'),
    @('vs_5_0','main','season_cloud_vs.hlsl','season_cloud_vs.cso','/O3'))
foreach($j in $jobs) {
    $dst=Join-Path $out $j[3];$in=Join-Path $src $j[2]
    $newest=(Get-ChildItem $src -Filter *.hlsl | Sort-Object LastWriteTime | Select-Object -Last 1).LastWriteTime
    if((Test-Path $dst) -and (Get-Item $dst).LastWriteTime -gt $newest){Write-Output "up to date $($j[3])";continue}
    $define=@();if($j.Count -gt 5){$define=@('/D',"$($j[5])=1")}
    & $fxc.FullName /nologo /T $j[0] /E $j[1] $j[4] @define /Fo $dst $in | Out-Null
    if($LASTEXITCODE -ne 0){throw "fxc failed: $($j[3])"}
    Write-Output "built $($j[3]) $((Get-Item $dst).Length)"
}
