# Frozen MAIN candidate. Never launches the game or changes the OpenXR runtime.
param([switch]$Restore,[switch]$VerifyOnly,
 [ValidateSet('Combined','ShadowExperiment','ReflectionExperiment','SmaaExperiment','MaterialExperiment')][string]$Mode='Combined')
$ErrorActionPreference='Stop'
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
function Closed {if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II first'}}
Closed
$m=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'candidate.json') -Raw|ConvertFrom-Json
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$targets=@((Join-Path $game 'd3d9.dll'),(Join-Path $game 'Darkness2VR-diagnostic.ini'))
$backups=@('baseline.dll','baseline-diagnostic.ini')
if($m.before.Count -ne 2){throw 'Unexpected target count'}
for($i=0;$i -lt 2;$i++){
 if($m.before[$i].target -ne $targets[$i] -or (Hash (Join-Path $PSScriptRoot $backups[$i])) -ne $m.before[$i].sha256){throw 'Baseline identity mismatch'}
}
foreach($e in $m.files){
 if($e.name -ne [IO.Path]::GetFileName($e.name) -or (Hash (Join-Path $PSScriptRoot $e.name)) -ne $e.sha256){throw 'Frozen file identity mismatch'}
}
foreach($e in $m.protected){if((Hash $e.target) -ne $e.sha256){throw 'Protected installation changed'}}
$receipt=Join-Path $PSScriptRoot 'installed.json'
$expected=$m.before
$atBaseline=$true
for($i=0;$i -lt 2;$i++){if((Hash $targets[$i]) -ne $m.before[$i].sha256){$atBaseline=$false}}
if(!$atBaseline){
 if(!(Test-Path -LiteralPath $receipt)){throw 'Unrecognized installed payload'}
 $r=Get-Content -LiteralPath $receipt -Raw|ConvertFrom-Json
 if($r.after.Count -ne 2){throw 'Invalid installation receipt'}
 $expected=$r.after
}
for($i=0;$i -lt 2;$i++){
 if($expected[$i].target -ne $targets[$i] -or (Hash $targets[$i]) -ne $expected[$i].sha256){throw 'Installed preimage changed'}
}
if($VerifyOnly){Write-Output 'VERIFIED package, current preimage, exact rollback and protected files; no writes';return}
$profile=switch($Mode){
 'ShadowExperiment' {'shadow-experiment.ini'}
 'ReflectionExperiment' {'reflection-experiment.ini'}
 'SmaaExperiment' {'smaa-experiment.ini'}
 'MaterialExperiment' {'material-experiment.ini'}
 default {'candidate-diagnostic.ini'}
}
$sources=if($Restore){$backups}else{@('d3d9.dll',$profile)}
$wanted=if($Restore){@($m.before|ForEach-Object sha256)}else{@(
 ($m.files|Where-Object name -eq 'd3d9.dll').sha256,
 ($m.files|Where-Object name -eq $profile).sha256)}
Closed
try {
 for($i=0;$i -lt 2;$i++){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $sources[$i]) -Destination $targets[$i] -Force}
 for($i=0;$i -lt 2;$i++){if((Hash $targets[$i]) -ne $wanted[$i]){throw 'Installed hash mismatch'}}
 foreach($e in $m.protected){if((Hash $e.target) -ne $e.sha256){throw 'Protected file changed'}}
 if(!$Restore){
  @{source_commit=$m.source_commit;mode=$Mode;headset='NOT RUN';game_launched=$false;
    after=@(for($i=0;$i -lt 2;$i++){@{target=$targets[$i];sha256=(Hash $targets[$i])}})}|
    ConvertTo-Json -Depth 5|Set-Content -LiteralPath $receipt -Encoding UTF8
 }
}catch{
 for($i=0;$i -lt 2;$i++){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $backups[$i]) -Destination $targets[$i] -Force}
 throw
}
Write-Output $(if($Restore){'RESTORED exact accepted DLL and diagnostic INI'}else{"INSTALLED $Mode; simulator OFF, collectors OFF; resolution preserved; no game launched"})
