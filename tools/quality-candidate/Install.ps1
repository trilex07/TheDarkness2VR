# Frozen MAIN quality candidate: three exact targets, no launch/runtime change.
param([switch]$Restore,[switch]$VerifyOnly,
 [ValidateSet('Combined','Material','Coverage')][string]$Mode='Combined')
$ErrorActionPreference='Stop'
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
function Closed{if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II first'}}
Closed
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$targets=@('d3d9.dll','Darkness2VR-diagnostic.ini','Darkness2VR.ini')|ForEach-Object {Join-Path $game $_}
$backups=@('baseline.dll','baseline-diagnostic.ini','baseline-render.ini')
$m=Get-Content (Join-Path $PSScriptRoot 'candidate.json') -Raw|ConvertFrom-Json
if($m.before.Count -ne 3){throw 'Unexpected target count'}
foreach($f in $m.files){
 if($f.name -ne [IO.Path]::GetFileName($f.name) -or (Hash (Join-Path $PSScriptRoot $f.name)) -ne $f.sha256){throw 'Frozen package changed'}
}
for($i=0;$i -lt 3;$i++){
 if($m.before[$i].target -ne $targets[$i] -or (Hash (Join-Path $PSScriptRoot $backups[$i])) -ne $m.before[$i].sha256){throw 'Rollback identity changed'}
}
foreach($f in $m.protected){if((Hash $f.target) -ne $f.sha256){throw 'Protected installation changed'}}
function Profile([string]$name){
 $names=@($m.profiles.$name)
 if($names.Count -ne 3){throw 'Profile target count differs'}
 foreach($n in $names){if($n -ne [IO.Path]::GetFileName($n) -or @($m.files|Where-Object name -eq $n).Count -ne 1){throw 'Unknown profile file'}}
 return $names
}
function Identities($names){@(foreach($name in $names){($m.files|Where-Object name -eq $name).sha256})}
$receiptPath=Join-Path $PSScriptRoot 'installed.json'
$atBaseline=$true
for($i=0;$i -lt 3;$i++){if((Hash $targets[$i]) -ne $m.before[$i].sha256){$atBaseline=$false}}
$expected=if($atBaseline){@($m.before|ForEach-Object sha256)}else{
 if(!(Test-Path $receiptPath)){throw 'Unknown installed preimage'}
 $r=Get-Content $receiptPath -Raw|ConvertFrom-Json
 if($r.source_commit -ne $m.source_commit -or $r.mode -notin @('Combined','Material','Coverage')){throw 'Invalid install receipt'}
 # Recompute expected bytes from frozen mode, never trust arbitrary after paths.
 Identities (Profile $r.mode)
}
for($i=0;$i -lt 3;$i++){if((Hash $targets[$i]) -ne $expected[$i]){throw 'Installed preimage changed'}}
$sources=if($Restore){$backups}else{Profile $Mode}
$wanted=if($Restore){@($m.before|ForEach-Object sha256)}else{Identities $sources}
if($VerifyOnly){Write-Output 'VERIFIED three targets, frozen profiles, rollback and protected files; no writes';return}
Closed
try{
 for($i=0;$i -lt 3;$i++){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $sources[$i]) -Destination $targets[$i] -Force}
 for($i=0;$i -lt 3;$i++){if((Hash $targets[$i]) -ne $wanted[$i]){throw 'Install verification failed'}}
 foreach($f in $m.protected){if((Hash $f.target) -ne $f.sha256){throw 'Protected file changed'}}
 if(!$Restore){@{source_commit=$m.source_commit;mode=$Mode;headset='NOT RUN';game_launched=$false}|ConvertTo-Json|Set-Content $receiptPath}
}catch{
 for($i=0;$i -lt 3;$i++){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $backups[$i]) -Destination $targets[$i] -Force}
 throw
}
Write-Output $(if($Restore){'RESTORED exact DLL, diagnostic INI and render INI'}else{"INSTALLED $Mode; no game launched; runtime unchanged"})
