# MAIN correction: explicit OFF in both primary INIs; no DLL/game launch.
param([switch]$Apply)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$local=Join-Path $env:LOCALAPPDATA 'Darkness2VR'
$previous=Join-Path $local 'packages/hud-head-tentacles-20261031T210354632'
$expected='155D6A427E4D53AD5EBEAE206E17C4A7F755ABBD4FEBC2C8D5629FAFF48D1AE1'
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
function Assert-Free {if(Get-CimInstance Win32_Process | Where-Object {$_.Name -eq 'DarknessII.exe'}){throw 'Close Darkness II before profile correction'}}
Assert-Free
if((Hash "$game/d3d9.dll") -ne $expected){throw 'Installed DLL changed; re-evaluate'}
if((Hash "$game/DarknessII.exe") -ne 'D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'){throw 'EXE identity mismatch'}
& "$root/tools/assert-candidate-profile.ps1" -CandidateDll "$game/d3d9.dll" -RenderIni @("$game/Darkness2VR.ini","$local/Darkness2VR.ini") -DiagnosticIni "$game/Darkness2VR-diagnostic.ini" | Out-Host
if(!$Apply){Write-Output 'PREFLIGHT PASS; no files changed';return}
$package=Join-Path $local ('packages/hud-fps-off-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback';$candidate=Join-Path $package 'candidate'
New-Item -ItemType Directory -Path $rollback,$candidate | Out-Null
$old=Get-Content "$previous/rollback/manifest.json" -Raw | ConvertFrom-Json
$before=@();$index=0
foreach($entry in $old.before) {
 $saved=Join-Path $rollback ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $entry.target));$index++
 $sha=Hash $entry.target;Copy-Item -LiteralPath $entry.target -Destination $saved
 if((Hash $saved) -ne $sha){throw 'Backup verification failed'}
 $before+=@{target=$entry.target;backup=$saved;sha256=$sha}
}
@{game_dir=$game;before=$before;source='codex/hud-head-tentacles AdaptiveFPS OFF profile correction'} | ConvertTo-Json -Depth 6 | Set-Content "$rollback/manifest.json" -Encoding UTF8
Copy-Item "$previous/rollback/Restore.ps1" "$rollback/Restore.ps1"
Copy-Item "$local/d3d9.log" "$package/before-d3d9.log"
$changes=@();$index=0
foreach($path in @("$game/Darkness2VR.ini","$local/Darkness2VR.ini")) {
 $text=[IO.File]::ReadAllText($path)
 if([regex]::IsMatch($text,'(?im)^AdaptiveFPS\s*=')){throw 'Explicit AdaptiveFPS already present; re-evaluate section ownership'}
 $newline=if($text.Contains("`r`n")){"`r`n"}else{"`n"}
 $new=[regex]::Replace($text,'(?im)^\[Render\]',[Text.RegularExpressions.MatchEvaluator]{param($m)$m.Value+$newline+'AdaptiveFPS=OFF'})
 if($new -eq $text){throw 'No Render section'}
 $staged=Join-Path $candidate ("$index.ini");$index++
 [IO.File]::WriteAllText($staged,$new,[Text.UTF8Encoding]::new($false))
 $strip='(?im)^AdaptiveFPS\s*=.*\r?\n'
 if([regex]::Replace($new,$strip,'') -cne $text){throw 'Unrelated INI change'}
 # Int parser cannot distinguish OFF from zero: confirm the literal as well.
 if(![regex]::IsMatch($new,'(?im)^AdaptiveFPS=OFF\r?$') -or [D2vrProfileGate]::GetPrivateProfileInt('Render','AdaptiveFPS',99,$staged) -ne 0){throw 'Staged INI parser failed'}
 $changes+=@{target=$path;staged=$staged}
}
Assert-Free
foreach($entry in $before){if((Hash $entry.target) -ne $entry.sha256){throw 'Installed state changed during backup'}}
$changedPaths=@($changes | ForEach-Object {[IO.Path]::GetFullPath($_.target)})
try {
 foreach($change in $changes) {
  Copy-Item -LiteralPath $change.staged -Destination $change.target -Force
  if((Hash $change.target) -ne (Hash $change.staged)){throw 'Installed profile hash mismatch'}
 }
 foreach($entry in $before){if([IO.Path]::GetFullPath($entry.target) -notin $changedPaths -and (Hash $entry.target) -ne $entry.sha256){throw 'Preserved target changed'}}
} catch {& "$rollback/Restore.ps1";throw}
$after=@();foreach($entry in $before){$after+=@{target=$entry.target;sha256=(Hash $entry.target)}}
@{deployed_utc=[DateTime]::UtcNow.ToString('o');dll_changed=$false;after=$after;rollback=$rollback;game_launched=$false;purpose='Disable inherited AdaptiveFPS AUTO; shadows unresolved'} | ConvertTo-Json -Depth 6 | Set-Content "$package/deployment.json" -Encoding UTF8
Write-Output "APPLIED AdaptiveFPS=OFF in both primary INIs: $package"
Write-Output "DLL unchanged: $(Hash "$game/d3d9.dll")"
