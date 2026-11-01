param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:/Program Files (x86)/Steam/steamapps/common/Darkness II'
$local='C:/Users/Trilex/AppData/Local/Darkness2VR'
$prior=Join-Path $local 'packages/small-tentacles-head-20261101T133218114/rollback'
$candidate=Join-Path $root 'd3d9.dll'
$expected='EFDCCFB0B40F37F716014DF836F5BE127121177B5C88EABC3E3714B828375B9C'
$candidateHash='DDDEF1CD8FB135A7D89E6C515786D809351926914FC1FF1C195ADF1F27B6F5BF'
$exeHash='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
$exe=Join-Path $game 'DarknessII.exe'
$target=Join-Path $game 'd3d9.dll'
$gameIni=Join-Path $game 'Darkness2VR.ini'
$localIni=Join-Path $local 'Darkness2VR.ini'
$diagnosticIni=Join-Path $game 'Darkness2VR-diagnostic.ini'
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II before installation'}
if((Hash $exe) -ne $exeHash -or (Hash $target) -ne $expected){throw 'Installed EXE/DLL identity mismatch'}
if((Hash $candidate) -ne $candidateHash){throw 'Candidate DLL identity mismatch'}
$manifest=Get-Content -LiteralPath (Join-Path $prior 'manifest.json') -Raw | ConvertFrom-Json
if(@($manifest.before).Count -ne 9){throw 'Expected nine protected targets'}
$gate=& (Join-Path $root 'tools/assert-candidate-profile.ps1') -CandidateDll $candidate `
    -RenderIni @($gameIni,$localIni) -DiagnosticIni $diagnosticIni
if($gate.Flavor -ne 'protected-native'){throw 'Protected-native candidate required'}
foreach($ini in @($gameIni,$localIni)){
    if([D2vrProfileGate]::GetPrivateProfileInt('Render','HeadAttachedTentacles',0,$ini) -ne 1){
        throw "HeadAttachedTentacles changed: $ini"
    }
}
if(!$Install){Write-Output "PREFLIGHT PASS DLL=$candidateHash; nine protected targets; no files changed";return}
$package=Join-Path $local ('packages/held-finisher-a-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback'
$candidateDir=Join-Path $package 'candidate'
New-Item -ItemType Directory -Force -Path $rollback,$candidateDir | Out-Null
$before=@();$index=0
foreach($entry in $manifest.before){
    $path=$entry.target
    $saved=Join-Path $rollback ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $path));$index++
    $sha=Hash $path
    Copy-Item -LiteralPath $path -Destination $saved
    if((Hash $saved) -ne $sha){throw "Backup verification failed: $path"}
    $before+=@{target=$path;backup=$saved;sha256=$sha}
}
@{game_dir=$game;exe_sha256=$exeHash;before=$before} | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath (Join-Path $rollback 'manifest.json') -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $prior 'Restore.ps1') -Destination (Join-Path $rollback 'Restore.ps1')
foreach($name in @('d3d9.dll','d3d9.pdb','d3d9.map')){
    Copy-Item -LiteralPath (Join-Path $root $name) -Destination (Join-Path $candidateDir $name)
}
if((Hash (Join-Path $candidateDir 'd3d9.dll')) -ne $candidateHash){throw 'Staged candidate mismatch'}
$sourceCommit=(git -C $root rev-parse HEAD).Trim()
@{dll_sha256=$candidateHash;source_commit=$sourceCommit;change='physical A during alternate native hold';
  headset_validation='NOT RUN'} | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $candidateDir 'source.json') -Encoding UTF8
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Game started during backup'}
foreach($entry in $before){if((Hash $entry.target) -ne $entry.sha256){throw 'Installed state changed during backup'}}
try {
    Copy-Item -LiteralPath (Join-Path $candidateDir 'd3d9.dll') -Destination $target -Force
    if((Hash $target) -ne $candidateHash -or (Hash $exe) -ne $exeHash){throw 'Installed DLL/EXE mismatch'}
    foreach($entry in $before){
        if($entry.target -ne $target -and (Hash $entry.target) -ne $entry.sha256){throw "Protected target changed: $($entry.target)"}
    }
} catch { & (Join-Path $rollback 'Restore.ps1');throw }
@{source_commit=$sourceCommit;dll_sha256=$candidateHash;rollback=$rollback;
  installed_utc=[DateTime]::UtcNow.ToString('o');game_launched=$false} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $package 'deployment.json') -Encoding UTF8
Write-Output "INSTALLED $package DLL=$candidateHash rollback=$rollback; game not launched"
