# MAIN only. User approved deployment; never launches a game.
param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$local=Join-Path $env:LOCALAPPDATA 'Darkness2VR'
$previous=Join-Path $local 'packages/unarmed-lefttrigger-20261031T204143841'
$expected='6CA7A1F5A40E29FBD7451CD7051D72C170D9FF33C16CE589ED968A882541AF80'
$exeHash='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
$source='5e70b9db'
function Hash($path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
function Assert-Free {
 if(Get-CimInstance Win32_Process | Where-Object {$_.Name -eq 'DarknessII.exe'}) {throw 'Close Darkness II before installing'}
}
function CandidateIni([string]$text) {
 $match=[regex]::Match($text,'(?ims)^\[Render\][^\r\n]*\r?\n(?<body>.*?)(?=^\[|\z)')
 if(!$match.Success){throw 'Render section missing'}
 $body=$match.Groups['body'].Value
 foreach($key in 'HideNativeHUD','HeadAttachedTentacles') {
  $pattern='(?im)^'+$key+'\s*=.*$'
  if([regex]::IsMatch($body,$pattern)){$body=[regex]::Replace($body,$pattern,"$key=1`r")}
  else {$body="$key=1`r`n"+$body}
 }
 return $text.Substring(0,$match.Groups['body'].Index)+$body+$text.Substring($match.Groups['body'].Index+$match.Groups['body'].Length)
}
Assert-Free
if((Hash "$game/DarknessII.exe") -ne $exeHash){throw 'EXE identity mismatch'}
if((Hash "$game/d3d9.dll") -ne $expected){throw 'Installed DLL changed since preflight; re-evaluate'}
$candidateDll=Join-Path $root 'build/hud-head-tentacles/d3d9.dll'
$gate=& "$root/tools/assert-candidate-profile.ps1" -CandidateDll $candidateDll -RenderIni @("$game/Darkness2VR.ini","$local/Darkness2VR.ini") -DiagnosticIni "$game/Darkness2VR-diagnostic.ini"
if($gate.Flavor -ne 'protected-native'){throw 'Protected-native build required'}
if(!$Install){$gate | Format-List;Write-Output 'PREFLIGHT PASS; no files changed';return}
$package=Join-Path $local ('packages/hud-head-tentacles-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback';$candidate=Join-Path $package 'candidate'
New-Item -ItemType Directory -Path $rollback,$candidate | Out-Null
$before=@();$index=0
$old=Get-Content -LiteralPath "$previous/rollback/manifest.json" -Raw | ConvertFrom-Json
foreach($entry in $old.before) {
 $target=$entry.target;$sha=Hash $target
 $saved=Join-Path $rollback ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $target));$index++
 Copy-Item -LiteralPath $target -Destination $saved
 if((Hash $saved) -ne $sha){throw 'Backup verification failed'}
 $before+=@{target=$target;backup=$saved;sha256=$sha}
}
@{game_dir=$game;source=$source;before=$before;exe_sha256=$exeHash} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$rollback/manifest.json" -Encoding UTF8
Copy-Item -LiteralPath "$previous/rollback/Restore.ps1" -Destination "$rollback/Restore.ps1"
foreach($name in 'd3d9.dll','d3d9.pdb','d3d9.map') {
 Copy-Item -LiteralPath "$root/build/hud-head-tentacles/$name" -Destination "$candidate/$name"
}
$changes=@(@{target="$game/d3d9.dll";staged="$candidate/d3d9.dll"})
foreach($profile in @(@{path="$game/Darkness2VR.ini";name='Steam.ini'},@{path="$local/Darkness2VR.ini";name='Local.ini'})) {
 $staged=Join-Path $candidate $profile.name
 $text=[IO.File]::ReadAllText($profile.path)
 $new=CandidateIni $text
 # A stripped comparison proves that only these two settings changed.
 $strip='(?im)^(HideNativeHUD|HeadAttachedTentacles)\s*=.*\r?\n?'
 if([regex]::Replace($new,$strip,'') -cne [regex]::Replace($text,$strip,'')){throw 'Unrelated profile change'}
 [IO.File]::WriteAllText($staged,$new,[Text.UTF8Encoding]::new($false))
 foreach($key in 'HideNativeHUD','HeadAttachedTentacles') {
  if([D2vrProfileGate]::GetPrivateProfileInt('Render',$key,0,$staged) -ne 1){throw "Staged INI parser did not admit $key"}
 }
 $changes+=@{target=$profile.path;staged=$staged}
}
& "$root/tools/assert-candidate-profile.ps1" -CandidateDll "$candidate/d3d9.dll" -RenderIni @("$candidate/Steam.ini","$candidate/Local.ini") -DiagnosticIni "$game/Darkness2VR-diagnostic.ini" | Out-Host
Assert-Free
foreach($entry in $before){if((Hash $entry.target) -ne $entry.sha256){throw 'Installed state changed during backup'}}
$changedPaths=@($changes | ForEach-Object {[IO.Path]::GetFullPath($_.target)})
try {
 foreach($change in $changes) {
  Copy-Item -LiteralPath $change.staged -Destination $change.target -Force
  if((Hash $change.target) -ne (Hash $change.staged)){throw 'Deployment verification failed'}
 }
 foreach($entry in $before) {
  if([IO.Path]::GetFullPath($entry.target) -notin $changedPaths -and (Hash $entry.target) -ne $entry.sha256){throw 'Preserved file changed'}
 }
 if((Hash "$game/DarknessII.exe") -ne $exeHash){throw 'EXE identity changed'}
 foreach($path in @("$game/Darkness2VR.ini","$local/Darkness2VR.ini")) {
  foreach($key in 'HideNativeHUD','HeadAttachedTentacles') {
   if([D2vrProfileGate]::GetPrivateProfileInt('Render',$key,0,$path) -ne 1){throw 'Installed INI read-back failed'}
  }
 }
} catch { & "$rollback/Restore.ps1";throw }
$after=@();foreach($entry in $before){$after+=@{target=$entry.target;sha256=(Hash $entry.target)}}
@{source=$source;base='cb795d58';deployed_utc=[DateTime]::UtcNow.ToString('o');dll_sha256=(Hash "$game/d3d9.dll");after=$after;rollback=$rollback;game_launched=$false;headset_validation='NOT RUN'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$package/deployment.json" -Encoding UTF8
Write-Output "DEPLOYED: $package"
Write-Output "DLL SHA256: $(Hash "$game/d3d9.dll")"
Write-Output "ROLLBACK: $rollback/Restore.ps1"
