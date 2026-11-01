param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:/Program Files (x86)/Steam/steamapps/common/Darkness II'
$local='C:/Users/Trilex/AppData/Local/Darkness2VR'
$candidate=Join-Path $root 'build/gaze-lod-diagnostic-final/d3d9.dll'
$expectedExe='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
$expectedOld='148538F92CB37B2AF2D166282972B29FF1C23E46E7D70BDB1787D700719889CF'
$expectedNew='AC6D3568AF0D670F69A8330D08A57B9F3EA4E602783F499A4BEC449848F47E1A'
$previous=Join-Path $local 'packages/gaze-lod-diagnostic-20261101T015029208/rollback'
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
if((Hash (Join-Path $game 'DarknessII.exe')) -ne $expectedExe){throw 'EXE mismatch'}
if((Hash (Join-Path $game 'd3d9.dll')) -ne $expectedOld){throw 'Installed DLL changed'}
if((Hash $candidate) -ne $expectedNew){throw 'Candidate changed'}
$gate=& (Join-Path $root 'tools/assert-candidate-profile.ps1') -CandidateDll $candidate `
    -RenderIni @((Join-Path $game 'Darkness2VR.ini'),(Join-Path $local 'Darkness2VR.ini')) `
    -DiagnosticIni (Join-Path $game 'Darkness2VR-diagnostic.ini')
if($gate.Flavor -ne 'protected-native'){throw 'Protected-native candidate required'}
$oldManifest=Get-Content -LiteralPath (Join-Path $previous 'manifest.json') -Raw | ConvertFrom-Json
if(@($oldManifest.before).Count -ne 9){throw 'Expected nine protected targets'}
foreach($entry in $oldManifest.before){if(!(Test-Path -LiteralPath $entry.target)){throw "Missing $($entry.target)"}}
if(!$Install){Write-Output "PREFLIGHT PASS $expectedNew; no files changed";return}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II before installation'}
$package=Join-Path $local ('packages/gaze-lod-owner-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback'
New-Item -ItemType Directory -Path $rollback,(Join-Path $package 'candidate') | Out-Null
$before=@();$index=0
foreach($entry in $oldManifest.before){
    $saved=Join-Path $rollback ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $entry.target));$index++
    $sha=Hash $entry.target
    Copy-Item -LiteralPath $entry.target -Destination $saved
    if((Hash $saved) -ne $sha){throw 'Backup verification failed'}
    $before+=@{target=$entry.target;backup=$saved;sha256=$sha}
}
@{game_dir=$game;exe_sha256=$expectedExe;before=$before} | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath (Join-Path $rollback 'manifest.json') -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $previous 'Restore.ps1') -Destination (Join-Path $rollback 'Restore.ps1')
foreach($name in @('d3d9.dll','d3d9.pdb','d3d9.map')){
    Copy-Item -LiteralPath (Join-Path $root "build/gaze-lod-diagnostic-final/$name") -Destination (Join-Path $package "candidate/$name")
}
foreach($name in @('d3d9.cpp','gaze_lod_diagnostic.inl','gaze_lod_owner_diagnostic.inl','native_aim.inl')){
    Copy-Item -LiteralPath (Join-Path $root "build/gaze-lod-diagnostic-final/src/$name") -Destination (Join-Path $package "candidate/$name")
}
try {
    Copy-Item -LiteralPath $candidate -Destination (Join-Path $game 'd3d9.dll') -Force
    if((Hash (Join-Path $game 'd3d9.dll')) -ne $expectedNew){throw 'DLL install verification failed'}
    foreach($entry in $before){
        if($entry.target -ne (Join-Path $game 'd3d9.dll') -and (Hash $entry.target) -ne $entry.sha256){
            throw "Protected target changed: $($entry.target)"
        }
    }
    if((Hash (Join-Path $game 'DarknessII.exe')) -ne $expectedExe){throw 'EXE changed'}
} catch {
    & (Join-Path $rollback 'Restore.ps1')
    throw
}
Write-Output "INSTALLED $package DLL=$expectedNew rollback=$rollback; game not launched"
