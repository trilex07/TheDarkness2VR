param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:/Program Files (x86)/Steam/steamapps/common/Darkness II'
$local='C:/Users/Trilex/AppData/Local/Darkness2VR'
$candidate=Join-Path $root 'build/gaze-lod-runtime/d3d9.dll'
$candidateMap=Join-Path $root 'build/gaze-lod-runtime/d3d9.map'
$previous=Join-Path $local 'packages/gaze-lod-runtime-20261101T110811047/rollback'
$oldDll='2AB582630B09EF02AD44465582A74B7FCEB563EB1EA5B4D998C03D8FB716A327'
$newDll='C398F251E9BE1556292E7683308D55E49A1D3B61C3A415669DF9BEBEC9EC1CB1'
$exeHash='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
$exe=Join-Path $game 'DarknessII.exe'
$target=Join-Path $game 'd3d9.dll'
$gameIni=Join-Path $game 'Darkness2VR.ini'
$profiles=@($gameIni,(Join-Path $local 'Darkness2VR.ini'))
if((Hash $exe) -ne $exeHash){throw 'Stock EXE mismatch'}
if((Hash $target) -ne $oldDll){throw 'Installed DLL changed'}
if((Hash $candidate) -ne $newDll){throw 'Candidate DLL changed'}
if(!(Test-Path -LiteralPath $candidateMap -PathType Leaf)){throw 'Candidate MAP missing'}
$gate=& (Join-Path $root 'tools/assert-candidate-profile.ps1') -CandidateDll $candidate -RenderIni $profiles -DiagnosticIni (Join-Path $game 'Darkness2VR-diagnostic.ini')
if($gate.Flavor -ne 'protected-native'){throw 'Protected-native candidate required'}
$iniText=[IO.File]::ReadAllText($gameIni)
if($iniText -notmatch '(?im)^GazeLodRuntimeObserver\s*=\s*1\s*$'){throw 'LOD observer setting missing'}
$diagnostic=Get-Content -LiteralPath (Join-Path $game 'Darkness2VR-diagnostic.ini') -Raw
if($iniText -notmatch '(?im)^NativeViewBoundary\s*=\s*3\s*$' -or
   $diagnostic -notmatch '(?im)^ImagePreservingPresent\s*=\s*1\s*$'){throw 'Protected view profile mismatch'}
$oldManifest=Get-Content -LiteralPath (Join-Path $previous 'manifest.json') -Raw | ConvertFrom-Json
if(@($oldManifest.before).Count -ne 9){throw 'Expected nine protected targets'}
foreach($entry in $oldManifest.before){if(!(Test-Path -LiteralPath $entry.target -PathType Leaf)){throw "Missing protected target: $($entry.target)"}}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II before installation'}
if(!$Install){Write-Output "PREFLIGHT PASS DLL=$newDll; no files changed";return}
$package=Join-Path $local ('packages/gaze-lod-debug-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback'
New-Item -ItemType Directory -Path $rollback,(Join-Path $package 'candidate') | Out-Null
$before=@();$index=0
foreach($entry in $oldManifest.before){
    $saved=Join-Path $rollback ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $entry.target));$index++
    $sha=Hash $entry.target
    Copy-Item -LiteralPath $entry.target -Destination $saved
    if((Hash $saved) -ne $sha){throw "Backup verification failed: $saved"}
    $before+=@{target=$entry.target;backup=$saved;sha256=$sha}
}
@{game_dir=$game;exe_sha256=$exeHash;before=$before} | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath (Join-Path $rollback 'manifest.json') -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $previous 'Restore.ps1') -Destination (Join-Path $rollback 'Restore.ps1')
Copy-Item -LiteralPath $candidate -Destination (Join-Path $package 'candidate/d3d9.dll')
Copy-Item -LiteralPath $candidateMap -Destination (Join-Path $package 'candidate/d3d9.map')
$source=(git -C $root rev-parse HEAD).Trim()
@{dll_sha256=$newDll;source_commit=$source;build='x86 /O2 D2VR_MATCHED_CAPTURE; no PDB due MSPDB140 mismatch';
  test='protected x86 build, exact EXE bytes and profile gate PASS';
  limitation='Read-only LOD owner observation; no CPU saving or headset acceptance yet'} | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $package 'candidate/source.json') -Encoding UTF8
try {
    Copy-Item -LiteralPath $candidate -Destination $target -Force
    if((Hash $target) -ne $newDll){throw 'DLL install verification failed'}
    if(([regex]::Matches([IO.File]::ReadAllText($gameIni),'(?m)^GazeLodRuntimeObserver=1\s*$')).Count -ne 1){throw 'Observer setting verification failed'}
    foreach($entry in $before){
        if($entry.target -eq $target){continue}
        if((Hash $entry.target) -ne $entry.sha256){throw "Protected target changed: $($entry.target)"}
    }
    if((Hash $exe) -ne $exeHash){throw 'EXE changed after installation'}
} catch {
    & (Join-Path $rollback 'Restore.ps1')
    throw
}
Write-Output "INSTALLED $package DLL=$newDll rollback=$rollback; game not launched"
