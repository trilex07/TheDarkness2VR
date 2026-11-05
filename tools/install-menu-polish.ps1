# MAIN frozen local DLL/asset package; exact target set selected by kind.
param([switch]$Restore,[switch]$VerifyOnly)
$ErrorActionPreference='Stop'
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
function Closed {if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II first'}}
Closed
$m=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'candidate.json') -Raw|ConvertFrom-Json
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$allowed=@('d3d9.dll','Darkness2VR-diagnostic.ini',
 'Cache.DLC\PCPatchManifests\Cache.Windows\H.Script.cache',
 'Cache.DLC\PCPatchManifests\Cache.Windows\H.Script.toc')
if($m.kind -in @('wrecking-light-ammo-reserve','lit-mask-projection')){$allowed=@('d3d9.dll')}
if($m.kind -eq 'menu-hands-shrine-symbols'){
 $allowed+=@('Cache.Windows\H.Misc.cache','Cache.Windows\H.Misc.toc')
}
if($m.files.Count -ne $allowed.Count){throw 'Invalid target count'}
$before=$true;$after=$true
for($i=0;$i -lt $allowed.Count;$i++){
 $f=$m.files[$i]
 if($f.relative -ne $allowed[$i]){throw 'Unexpected target'}
 foreach($n in @($f.payload,$f.baseline)){
  if($n -ne [IO.Path]::GetFileName($n)){throw 'Unsafe package path'}
 }
 if((Hash (Join-Path $PSScriptRoot $f.payload)) -ne $f.after -or
    (Hash (Join-Path $PSScriptRoot $f.baseline)) -ne $f.before){throw 'Package or rollback identity differs'}
 $h=Hash (Join-Path $game $f.relative)
 $before=$before -and ($h -eq $f.before);$after=$after -and ($h -eq $f.after)
}
if(!$before -and !$after){throw 'Unrecognized or mixed installed payload; refusing overwrite'}
foreach($e in $m.protected){if((Hash $e.target) -ne $e.sha256){throw 'Protected file changed'}}
if($VerifyOnly){Write-Output "VERIFIED $($allowed.Count) payloads, exact rollback, installed identity and protected files";return}
$sources=@();$wanted=@();$recovery=@()
foreach($f in $m.files){
 $sources+=if($Restore){$f.baseline}else{$f.payload}
 $wanted+=if($Restore){$f.before}else{$f.after}
 $recovery+=if($before){$f.baseline}else{$f.payload}
}
Closed
try {
 for($i=0;$i -lt $allowed.Count;$i++){
  Copy-Item -LiteralPath (Join-Path $PSScriptRoot $sources[$i]) -Destination (Join-Path $game $allowed[$i]) -Force
  if((Hash (Join-Path $game $allowed[$i])) -ne $wanted[$i]){throw 'Installation readback differs'}
 }
 foreach($e in $m.protected){if((Hash $e.target) -ne $e.sha256){throw 'Protected file changed'}}
 @{source_commit=$m.source_commit;restored=[bool]$Restore;simulator='OFF';headset='NOT RUN';
   after=@(for($i=0;$i -lt $allowed.Count;$i++){@{relative=$allowed[$i];sha256=$wanted[$i]}})}|
   ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $PSScriptRoot 'installed.json') -Encoding UTF8
}catch{
 Closed
 for($i=0;$i -lt $allowed.Count;$i++){
  Copy-Item -LiteralPath (Join-Path $PSScriptRoot $recovery[$i]) -Destination (Join-Path $game $allowed[$i]) -Force
  if((Hash (Join-Path $game $allowed[$i])) -ne (Hash (Join-Path $PSScriptRoot $recovery[$i]))){throw 'Recovery readback differs'}
 }
 throw
}
Write-Output $(if($Restore){'RESTORED exact package baseline'}else{'INSTALLED combined candidate; simulator OFF; resolution preserved'})
