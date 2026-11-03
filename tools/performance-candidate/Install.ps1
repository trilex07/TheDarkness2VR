# Frozen MAIN candidate. Run only after closing Darkness II. No launch.
param([switch]$Restore,[ValidateSet('Both','Cache','Worker','Off')][string]$Mode='Cache',[switch]$VerifyOnly)
$ErrorActionPreference='Stop'
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
function Closed{if(Get-Process | Where-Object ProcessName -eq 'DarknessII'){throw 'Close Darkness II before installing/restoring'}}
Closed
$m=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'candidate.json') -Raw | ConvertFrom-Json
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$dll=Join-Path $game 'd3d9.dll';$ini=Join-Path $game 'Darkness2VR.ini'
if($m.before.Count -ne 2 -or $m.before[0].target -ne $dll -or $m.before[1].target -ne $ini){throw 'Unexpected target set'}
foreach($entry in $m.files){if($entry.name -ne [IO.Path]::GetFileName($entry.name) -or (Hash (Join-Path $PSScriptRoot $entry.name)) -ne $entry.sha256){throw 'Frozen package hash mismatch'}}
foreach($entry in $m.protected){if((Hash $entry.target) -ne $entry.sha256){throw 'Protected installation changed'}}
if((Hash (Join-Path $PSScriptRoot 'baseline.dll')) -ne $m.before[0].sha256 -or (Hash (Join-Path $PSScriptRoot 'baseline.ini')) -ne $m.before[1].sha256){throw 'Baseline backup differs'}
if($VerifyOnly){Write-Output 'VERIFIED frozen candidate, rollback backups and protected installation; no writes';return}
$receiptPath=Join-Path $PSScriptRoot 'installed.json'
if($Restore){
 $receipt=Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
 foreach($entry in $receipt.after){if((Hash $entry.target) -ne $entry.sha256){throw 'Installed state differs; refusing overwrite'}}
 Closed
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'baseline.dll') -Destination $dll -Force
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'baseline.ini') -Destination $ini -Force
 if((Hash $dll) -ne $m.before[0].sha256 -or (Hash $ini) -ne $m.before[1].sha256){throw 'Restore verification failed'}
 Write-Output 'RESTORED exact baseline DLL and INI; separate experimental cache retained, old caches unchanged';return
}
$expected=if(Test-Path -LiteralPath $receiptPath){(Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json).after}else{$m.before}
if((Hash $dll) -eq $m.before[0].sha256 -and (Hash $ini) -eq $m.before[1].sha256){$expected=$m.before}
foreach($entry in $expected){if((Hash $entry.target) -ne $entry.sha256){throw 'Installed baseline/candidate differs'}}
$profile=[string]$m.profiles.$Mode
if(!$profile -or $profile -ne [IO.Path]::GetFileName($profile) -or !($m.files|Where-Object name -eq $profile)){throw 'Profile missing from frozen package'}
Closed
try{
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'd3d9.dll') -Destination $dll -Force
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot $profile) -Destination $ini -Force
 if((Hash $dll) -ne ($m.files|Where-Object name -eq 'd3d9.dll').sha256 -or (Hash $ini) -ne ($m.files|Where-Object name -eq $profile).sha256){throw 'Installation hash differs'}
 foreach($entry in $m.protected){if((Hash $entry.target) -ne $entry.sha256){throw 'Protected file changed'}}
 @{source_commit=$m.source_commit;mode=$Mode;headset='NOT RUN';game_launched=$false;after=@(@{target=$dll;sha256=(Hash $dll)},@{target=$ini;sha256=(Hash $ini)})} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $receiptPath -Encoding UTF8
}catch{
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'baseline.dll') -Destination $dll -Force
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'baseline.ini') -Destination $ini -Force
 throw
}
Write-Output "INSTALLED Mode=$Mode; no game launched. Rollback: powershell -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Restore"
