# Frozen MAIN package: closed-game install, verification or exact rollback.
param([switch]$Restore,[switch]$VerifyOnly)
$ErrorActionPreference='Stop'
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Closed {if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II first'}}
Closed
$m=Get-Content (Join-Path $PSScriptRoot 'candidate.json') -Raw|ConvertFrom-Json
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
if($m.game -ne $game -or $m.files.Count -ne 33){throw 'Package target contract differs'}
$allow=@('d3d9.dll','Darkness2VR.ini','Darkness2VR-diagnostic.ini')
$allow+=@('H.Font','B.Font'|ForEach-Object {$s=$_;'.cache','.toc'|ForEach-Object {"Cache.DLC/PCPatchManifests/Cache.Windows/$s$_"}})
$voices=@('H.Misc','H.Misc_en','B.Misc_en','F.Misc_en'|ForEach-Object {$s=$_;'.cache','.toc'|ForEach-Object {"$s$_"}})
$allow+=@($voices|ForEach-Object {"Cache.Windows/$_"})
$allow+=@('VRSettings/Apply-Languages.ps1','VRSettings/languages.json')
foreach($lang in 'en','pl'){$allow+=@($voices|ForEach-Object {"VRSettings/languages/$lang/$_"})}
if(@($m.files.relative|Select-Object -Unique).Count -ne $allow.Count){throw 'Duplicate targets'}
foreach($f in $m.files){
 if($f.relative -notin $allow){throw 'Unrecognized package target'}
 $target=Join-Path $game $f.relative
 if((Hash (Join-Path $PSScriptRoot ('payload/'+$f.relative))) -ne $f.after){throw 'Payload mismatch'}
 if($f.before -and (Hash (Join-Path $PSScriptRoot ('rollback/'+$f.relative))) -ne $f.before){throw 'Rollback mismatch'}
 if(Test-Path -LiteralPath $target){
  $h=Hash $target
  # Settings are intentionally mutable. Preserve their current version in the
  # recovery transaction before an explicit install/restore resets them.
  if($f.relative -ne 'Darkness2VR.ini' -and $h -ne $f.before -and $h -ne $f.after -and $h -notin $f.allowed){throw "Unrecognized current file: $($f.relative)"}
 }elseif($f.before){throw "Missing original: $($f.relative)"}
}
foreach($p in $m.protected){if((Hash (Join-Path $game $p.relative)) -ne $p.hash){throw "Protected file changed: $($p.relative)"}}
if($VerifyOnly){'VERIFIED frozen payload, rollback, current preimages and protected files';return}
$recovery=Join-Path $PSScriptRoot ('recovery/'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffffff'))
New-Item -ItemType Directory -Force $recovery|Out-Null
$snapshots=@()
foreach($f in $m.files){
 $target=Join-Path $game $f.relative;$backup=Join-Path $recovery $f.relative
 if(Test-Path -LiteralPath $target){
  New-Item -ItemType Directory -Force (Split-Path $backup -Parent)|Out-Null
  Copy-Item -LiteralPath $target -Destination $backup
  $h=Hash $target;if((Hash $backup) -ne $h){throw 'Recovery backup mismatch'}
  $snapshots+=@{target=$target;backup=$backup;hash=$h}
 }else{$snapshots+=@{target=$target;backup=$null;hash=$null}}
}
Closed
try {
 foreach($f in $m.files){
  $target=Join-Path $game $f.relative
  if($Restore -and !$f.before){if(Test-Path -LiteralPath $target){Remove-Item -LiteralPath $target};continue}
  $folder=if($Restore){'rollback/'}else{'payload/'}
  $expected=if($Restore){$f.before}else{$f.after}
  New-Item -ItemType Directory -Force (Split-Path $target -Parent)|Out-Null
  Copy-Item -LiteralPath (Join-Path $PSScriptRoot ($folder+$f.relative)) -Destination $target -Force
  if((Hash $target) -ne $expected){throw 'Installed readback mismatch'}
 }
 foreach($p in $m.protected){if((Hash (Join-Path $game $p.relative)) -ne $p.hash){throw 'Protected readback mismatch'}}
}catch{
 foreach($s in $snapshots){
  if($s.backup){Copy-Item -LiteralPath $s.backup -Destination $s.target -Force;if((Hash $s.target) -ne $s.hash){throw 'Emergency rollback failed'}}
  elseif(Test-Path -LiteralPath $s.target){Remove-Item -LiteralPath $s.target}
 }
 throw
}
@{status=$(if($Restore){'RESTORED'}else{'INSTALLED'});source=$m.source;recovery=$recovery;headset='NOT RUN';simulator='OFF'}|ConvertTo-Json|Set-Content (Join-Path $PSScriptRoot 'installed.json')
$(if($Restore){'RESTORED complete pre-task baseline'}else{'INSTALLED VR settings + DLC font sync; simulator OFF; SS100; PL voice/text'})
