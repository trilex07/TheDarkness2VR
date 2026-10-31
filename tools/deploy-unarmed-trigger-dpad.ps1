# MAIN deployment of side/unarmed-trigger-dpad-on-half-rate (protected-native build).
# Changes ONLY d3d9.dll. All nine DLL/INI/cache targets are backed up with SHA256 and Restore.ps1.
param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$local='C:\Users\Trilex\AppData\Local\Darkness2VR'
$exeHash='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
$installedHash='1D24CC777D4F9001A0437E71696BC84E77A96351BEEFB0D40D14FB253967E442'
$prevPackage="$local\packages\unarmed-trigger-dpad-20261031T193931147"
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
function Assert-Free {
    $busy=@(Get-CimInstance Win32_Process | Where-Object {$_.Name -eq 'DarknessII.exe' -or
        ($_.ExecutablePath -like '*\steamapps\common\*' -and $_.ExecutablePath -notmatch '\\(SteamVR|fpsVR|PlayStation VR2 App)\\')})
    if($busy){throw ('Game session busy: '+(($busy | ForEach-Object {"$($_.Name) PID=$($_.ProcessId)"}) -join ', '))}
}
Assert-Free
if((Hash "$game\DarknessII.exe") -ne $exeHash){throw 'EXE identity mismatch'}
$cur=Hash "$game\d3d9.dll"
if($cur -ne $installedHash){throw "Installed DLL is $cur, expected previous unarmed-trigger build $installedHash; re-evaluate"}
$cand="$root\d3d9.dll"
if(!(Test-Path -LiteralPath $cand)){throw 'Candidate not built; run build.ps1 -DiagnosticCapture first'}
$gate=& "$root\tools\assert-candidate-profile.ps1" -CandidateDll $cand -RenderIni @("$game\Darkness2VR.ini","$local\Darkness2VR.ini") -DiagnosticIni "$game\Darkness2VR-diagnostic.ini"
$gate | Out-Host
if($gate.Flavor -ne 'protected-native'){throw 'Candidate must be protected-native (build.ps1 -DiagnosticCapture)'}
$candHash=Hash $cand
$text=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($cand))
if(!$text.Contains("[DoubleTapDpad]")){throw 'Candidate does not contain the UnarmedTriggerDpad feature'}
if(!$Install){Write-Output "PREFLIGHT PASS candidate=$candHash; no installed files changed";return}

$package=Join-Path $local ('packages\unarmed-doubletap-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback';$candidate=Join-Path $package 'candidate'
New-Item -ItemType Directory -Path $rollback,$candidate | Out-Null
$old=Get-Content -LiteralPath "$prevPackage\rollback\manifest.json" -Raw | ConvertFrom-Json
$before=@();$index=0
foreach($entry in $old.before){
    $target=$entry.target;$sha=Hash $target
    $saved=Join-Path $rollback ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $target));$index++
    Copy-Item -LiteralPath $target -Destination $saved
    if((Hash $saved) -ne $sha){throw 'Backup verification failed'}
    $before+=@{target=$target;backup=$saved;sha256=$sha}
}
if(!($before | Where-Object {$_.target -eq "$game\d3d9.dll"})){throw 'Previous manifest does not cover the DLL'}
@{game_dir=$game;exe_sha256=$exeHash;source='side/unarmed-trigger-dpad-on-installed d3d77785';before=$before} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$rollback\manifest.json" -Encoding UTF8
Copy-Item -LiteralPath "$prevPackage\rollback\Restore.ps1" -Destination "$rollback\Restore.ps1"
foreach($name in 'd3d9.dll','d3d9.pdb','d3d9.map'){Copy-Item -LiteralPath "$root\$name" -Destination "$candidate\$name"}
Assert-Free
foreach($entry in $before){if((Hash $entry.target) -ne $entry.sha256){throw 'Installed state changed during backup'}}
try {
    Copy-Item -LiteralPath "$candidate\d3d9.dll" -Destination "$game\d3d9.dll" -Force
    if((Hash "$game\d3d9.dll") -ne $candHash -or (Hash "$game\DarknessII.exe") -ne $exeHash){throw 'Deployment identity mismatch'}
    foreach($entry in $before){if($entry.target -ne "$game\d3d9.dll" -and (Hash $entry.target) -ne $entry.sha256){throw "Preserved file changed: $($entry.target)"}}
} catch {
    & "$rollback\Restore.ps1"
    throw
}
$after=@();foreach($entry in $before){$after+=@{target=$entry.target;sha256=(Hash $entry.target)}}
@{source='side/unarmed-trigger-dpad-on-installed d3d77785';base='side/shouldrender-fix-snapshot 2ae798d3';dll_sha256=$candHash;exe_sha256=$exeHash;deployed_utc=[DateTime]::UtcNow.ToString('o');after=$after;rollback=$rollback;game_launched=$false;headset_validation='NOT RUN';ini_changed=$false} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$package\deployment.json" -Encoding UTF8
Write-Output "DEPLOYED $candHash (DLL only, INIs untouched); rollback: $rollback\Restore.ps1"
