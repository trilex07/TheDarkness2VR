# MAIN only; v7: gaze menu cursor in pause menus (native pause / Flash cursor gate), A confirms in pause. No game launch, profile or cache edits.
param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:/Program Files (x86)/Steam/steamapps/common/Darkness II'
$local='C:/Users/Trilex/AppData/Local/Darkness2VR'
$previous=Join-Path $local 'packages/eye-tracking-v6-20261101T004123205/rollback'
$candidate=Join-Path $root 'build/eye-tracking-v7'
New-Item -ItemType Directory -Force -Path $candidate | Out-Null
foreach($name in 'd3d9.dll','d3d9.pdb','d3d9.map'){Copy-Item -LiteralPath "$root/$name" -Destination "$candidate/$name" -Force}
$source=(git -C $root rev-parse --short HEAD 2>$null)
@{dll_sha256=(Get-FileHash -LiteralPath "$candidate/d3d9.dll" -Algorithm SHA256).Hash;built_utc=(Get-Item "$root/d3d9.dll").LastWriteTimeUtc.ToString('o');source="main-weapon-wheels v7 (worktree HEAD $source + uncommitted)";tests='dual_weapon_wheel MSVC /W4 /WX'} | ConvertTo-Json | Set-Content -LiteralPath "$candidate/source.json" -Encoding UTF8
$provenance=Get-Content -LiteralPath "$candidate/source.json" -Raw | ConvertFrom-Json
function Hash($path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
function Assert-Free {
 if(Get-CimInstance Win32_Process | Where-Object {$_.Name -eq 'DarknessII.exe'}){throw 'Close Darkness II before installing'}
}
$exeHash='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
$oldDll='FE5D648056A6BF012FD36BBE367A7D4F8FE942BFBF0028C23C0184FF4D357169'
if((Hash "$game/DarknessII.exe") -ne $exeHash){throw 'EXE identity mismatch'}
if((Hash "$game/d3d9.dll") -ne $oldDll){throw 'Installed DLL changed; re-evaluate'}
if((Hash "$candidate/d3d9.dll") -ne $provenance.dll_sha256){throw 'Candidate provenance mismatch'}
$gate=& "$root/tools/assert-candidate-profile.ps1" -CandidateDll "$candidate/d3d9.dll" -RenderIni @("$game/Darkness2VR.ini","$local/Darkness2VR.ini") -DiagnosticIni "$game/Darkness2VR-diagnostic.ini"
if($gate.Flavor -ne 'protected-native'){throw 'Protected-native candidate required'}
foreach($ini in @("$game/Darkness2VR.ini","$local/Darkness2VR.ini")) {
 foreach($key in 'HideNativeHUD','HeadAttachedTentacles') {
  if([D2vrProfileGate]::GetPrivateProfileInt('Render',$key,0,$ini) -ne 1){throw "Missing existing setting: $key"}
 }
 if([IO.File]::ReadAllText($ini) -notmatch '(?im)^AdaptiveFPS\s*=\s*OFF\s*$'){throw 'Explicit AdaptiveFPS=OFF required'}
}
if(!$Install){$gate | Format-List;Write-Output 'PREFLIGHT PASS; no files changed';return}
Assert-Free
$package=Join-Path $local ('packages/eye-tracking-v7-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback'
New-Item -ItemType Directory -Path $rollback,"$package/candidate" | Out-Null
$old=Get-Content -LiteralPath "$previous/manifest.json" -Raw | ConvertFrom-Json
if(@($old.before).Count -ne 9){throw 'Expected DLL, four profiles and four caches'}
$before=@();$index=0
foreach($entry in $old.before) {
 $target=$entry.target;$sha=Hash $target
 $saved=Join-Path $rollback ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $target));$index++
 Copy-Item -LiteralPath $target -Destination $saved
 if((Hash $saved) -ne $sha){throw 'Backup verification failed'}
 $before+=@{target=$target;backup=$saved;sha256=$sha}
}
@{game_dir=$game;source=$provenance.source;before=$before;exe_sha256=$exeHash} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$rollback/manifest.json" -Encoding UTF8
Copy-Item -LiteralPath "$previous/Restore.ps1" -Destination "$rollback/Restore.ps1"
foreach($name in 'd3d9.dll','d3d9.pdb','d3d9.map','source.json') {
 Copy-Item -LiteralPath "$candidate/$name" -Destination "$package/candidate/$name"
}
Assert-Free
foreach($entry in $before){if((Hash $entry.target) -ne $entry.sha256){throw 'Installed files changed during backup'}}
try {
 Copy-Item -LiteralPath "$package/candidate/d3d9.dll" -Destination "$game/d3d9.dll" -Force
 if((Hash "$game/d3d9.dll") -ne $provenance.dll_sha256){throw 'DLL copy verification failed'}
 foreach($entry in $before) {
  if([IO.Path]::GetFullPath($entry.target) -ne [IO.Path]::GetFullPath("$game/d3d9.dll") -and (Hash $entry.target) -ne $entry.sha256){throw 'Preserved profile/cache changed'}
 }
 if((Hash "$game/DarknessII.exe") -ne $exeHash){throw 'EXE identity changed'}
} catch {& "$rollback/Restore.ps1";throw}
@{source=$provenance.source;deployed_utc=[DateTime]::UtcNow.ToString('o');dll_sha256=(Hash "$game/d3d9.dll");rollback=$rollback;game_launched=$false;runtime_validation='PENDING'} | ConvertTo-Json | Set-Content -LiteralPath "$package/deployment.json" -Encoding UTF8
$bridgeDir=Join-Path $local 'runtime/psvr2'
$bridgeNew=Join-Path $root 'build/psvr2-bridge/Darkness2Psvr2Bridge.exe'
if(Test-Path -LiteralPath $bridgeNew) {
 if(Get-Process -Name Darkness2Psvr2Bridge -ErrorAction SilentlyContinue){throw 'PSVR2 bridge still running; close the game first'}
 $bridgeOld=Join-Path $bridgeDir 'Darkness2Psvr2Bridge.exe'
 if(Test-Path -LiteralPath $bridgeOld){Copy-Item -LiteralPath $bridgeOld -Destination (Join-Path $rollback 'Darkness2Psvr2Bridge.exe.previous')}
 Copy-Item -LiteralPath $bridgeNew -Destination $bridgeOld -Force
 if((Hash $bridgeOld) -ne (Hash $bridgeNew)){throw 'Bridge copy verification failed'}
 Write-Output "BRIDGE: $(Hash $bridgeOld) (previous saved as rollback/Darkness2Psvr2Bridge.exe.previous; Restore.ps1 does not restore it)"
}
Write-Output "DEPLOYED: $package"
Write-Output "ROLLBACK: $rollback/Restore.ps1"
