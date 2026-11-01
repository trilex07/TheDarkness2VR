param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:/Program Files (x86)/Steam/steamapps/common/Darkness II'
$local='C:/Users/Trilex/AppData/Local/Darkness2VR'
$candidate=Join-Path $root 'build/engine-lod-r2/d3d9.dll'
$candidateMap=Join-Path $root 'build/engine-lod-r2/d3d9.map'
$prior=Join-Path $local 'packages/engine-lod-trial-20261101T113328024/rollback'
$expectedDll='9AFA35F2A4B3B74EFA79FBF2C10BCEA296F6A159126B407E1783B8E84F57A358'
$exeHash='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
$exe=Join-Path $game 'DarknessII.exe'
$target=Join-Path $game 'd3d9.dll'
$gameIni=Join-Path $game 'Darkness2VR.ini'
$diagnosticIni=Join-Path $game 'Darkness2VR-diagnostic.ini'
if((Hash $exe) -ne $exeHash){throw 'Stock EXE mismatch'}
if((Hash $target) -ne $expectedDll){throw 'Installed DLL changed'}
if(!(Test-Path -LiteralPath $candidateMap -PathType Leaf)){throw 'Candidate MAP missing'}
$candidateHash=Hash $candidate
$gate=& (Join-Path $root 'tools/assert-candidate-profile.ps1') -CandidateDll $candidate -RenderIni @($gameIni,(Join-Path $local 'Darkness2VR.ini')) -DiagnosticIni $diagnosticIni
if($gate.Flavor -ne 'protected-native'){throw 'Protected-native candidate required'}
$iniText=[IO.File]::ReadAllText($gameIni)
$diagnostic=Get-Content -LiteralPath $diagnosticIni -Raw
if($iniText -notmatch '(?im)^NativeViewBoundary\s*=\s*3\s*$' -or
   $iniText -notmatch '(?im)^EngineLODScale\s*=\s*1\.25\s*$' -or
   $diagnostic -notmatch '(?im)^ImagePreservingPresent\s*=\s*1\s*$'){throw 'Protected LOD view profile mismatch'}
$manifest=Get-Content -LiteralPath (Join-Path $prior 'manifest.json') -Raw | ConvertFrom-Json
if(@($manifest.before).Count -ne 9){throw 'Expected nine protected targets'}
foreach($entry in $manifest.before){if(!(Test-Path -LiteralPath $entry.target -PathType Leaf)){throw "Missing protected target: $($entry.target)"}}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II before installation'}
if(!$Install){Write-Output "PREFLIGHT PASS DLL=$candidateHash; no files changed";return}
$package=Join-Path $local ('packages/engine-lod-r2-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback'
New-Item -ItemType Directory -Path $rollback,(Join-Path $package 'candidate') | Out-Null
$before=@();$index=0
foreach($entry in $manifest.before){
    $saved=Join-Path $rollback ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $entry.target));$index++
    $sha=Hash $entry.target
    Copy-Item -LiteralPath $entry.target -Destination $saved
    if((Hash $saved) -ne $sha){throw "Backup verification failed: $saved"}
    $before+=@{target=$entry.target;backup=$saved;sha256=$sha}
}
@{game_dir=$game;exe_sha256=$exeHash;before=$before} | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath (Join-Path $rollback 'manifest.json') -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $prior 'Restore.ps1') -Destination (Join-Path $rollback 'Restore.ps1')
Copy-Item -LiteralPath $candidate -Destination (Join-Path $package 'candidate/d3d9.dll')
Copy-Item -LiteralPath $candidateMap -Destination (Join-Path $package 'candidate/d3d9.map')
$source=(git -C $root rev-parse HEAD).Trim()
@{dll_sha256=$candidateHash;source_commit=$source;setting='EngineLODScale=1.25';
  scope='Native LOD trial plus current-pause R2 routing and event trace';
  limitation='R2 fix requires headset acceptance; no synthetic FPS claim'} | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $package 'candidate/source.json') -Encoding UTF8
try {
    Copy-Item -LiteralPath $candidate -Destination $target -Force
    if((Hash $target) -ne $candidateHash){throw 'DLL install verification failed'}
    foreach($entry in $before){
        if($entry.target -eq $target){continue}
        if((Hash $entry.target) -ne $entry.sha256){throw "Protected target changed: $($entry.target)"}
    }
    if((Hash $exe) -ne $exeHash){throw 'EXE changed after installation'}
} catch {
    & (Join-Path $rollback 'Restore.ps1')
    throw
}
Write-Output "INSTALLED $package DLL=$candidateHash rollback=$rollback; game not launched"
