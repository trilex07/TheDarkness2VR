param([switch]$Install,
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$CandidateName='rc15-safe-startup-20261021')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$candidate=Join-Path (Join-Path $root 'dist') $CandidateName
if($Install -and (Get-Process DarknessII -ErrorAction SilentlyContinue)) { throw 'Close Darkness II before installing.' }
New-Item -ItemType Directory -Path $candidate -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'd3d9.dll') -Destination $candidate -Force
Copy-Item -LiteralPath 'C:\Users\Trilex\openxr-sdk\native\Win32\release\bin\openxr_loader.dll' -Destination $candidate -Force
# Keep the user's actual tested setup, including resolution and HeadLookMode.
$config=[IO.File]::ReadAllText((Join-Path $game 'Darkness2VR.ini'))
$config=[regex]::Replace($config,'(?m)^DumpFrames=.*$','DumpFrames=0')
$config=[regex]::Replace($config,'(?m)^HeadGeometry=.*\r?\n?','')
$config=[regex]::Replace($config,'(?m)^EdgeCrop=.*\r?\n?','')
$config=$config.Replace('[Render]',"[Render]`r`nHeadGeometry=0`r`nEdgeCrop=0")
$config=[regex]::Replace($config,'(?m)^GestureGamepadBridge=.*\r?\n?','')
$config=[regex]::Replace($config,'(?m)^TentacleGestureBridge=.*\r?\n?','')
$config=[regex]::Replace($config,'(?m)^TentacleAimMarker=.*\r?\n?','')
$config=[regex]::Replace($config,'(?m)^NativeLeftAim=.*\r?\n?','')
$config=[regex]::Replace($config,'(?m)^NativeWeaponPose=.*\r?\n?','')
$config=[regex]::Replace($config,'(?m)^NativeTrackedHands=.*\r?\n?','')
$config=[regex]::Replace($config,'(?m)^ThrowAimAssist=.*\r?\n?','')
$config=$config.Replace('[Input]',"[Input]`r`n; RC10: AutoFov GetFOV probe + engine cull sync (record pass); RC9 per-eye; RC8d hands.`r`nTentacleGestureBridge=1`r`nTentacleAimMarker=1`r`nNativeLeftAim=1`r`nNativeWeaponPose=1`r`nNativeTrackedHands=1`r`nThrowAimAssist=75`r`nGestureGamepadBridge=0")
[IO.File]::WriteAllText((Join-Path $candidate 'Darkness2VR.ini'),$config,[Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath (Join-Path $root 'RC_TESTS.md') -Destination (Join-Path $candidate 'README.md') -Force
$files=Get-ChildItem -LiteralPath $candidate -File | Where-Object Name -ne 'SHA256.json'
$hashes=@($files | ForEach-Object { @{File=$_.Name;SHA256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash} })
$hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $candidate 'SHA256.json') -Encoding utf8
if($Install) {
    & (Join-Path $PSScriptRoot 'assert-candidate-profile.ps1') -CandidateDll (Join-Path $candidate 'd3d9.dll') `
        -RenderIni @((Join-Path $candidate 'Darkness2VR.ini')) `
        -DiagnosticIni (Join-Path $game 'Darkness2VR-diagnostic.ini') | Out-Host
    $stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
    foreach($name in @('d3d9.dll','Darkness2VR.ini')) {
        $target=Join-Path $game $name
        Copy-Item -LiteralPath $target -Destination "$target.before-rc-$stamp" -Force
        Copy-Item -LiteralPath (Join-Path $candidate $name) -Destination $target -Force
    }
    Write-Output "Installed RC, backups: *.before-rc-$stamp"
}
Write-Output "Candidate: $candidate"
Write-Output "DLL SHA256: $((Get-FileHash -LiteralPath (Join-Path $candidate 'd3d9.dll')).Hash)"
