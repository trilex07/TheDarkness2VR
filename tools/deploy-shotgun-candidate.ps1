param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:/Program Files (x86)/Steam/steamapps/common/Darkness II'
$local='C:/Users/Trilex/AppData/Local/Darkness2VR'
$prior=Join-Path $local 'packages/shotgun-pose-observer-20261101T140317414/rollback'
$candidate=Join-Path $root 'd3d9.dll'
$expected='867E3DE9E96920836F9B3CE761AF53875029F6A89A26EB43AFC6E39118C3EE0C'
$candidateHash='00F659DF51A7D3ED43084A837D190F2E288BB05861A464E31721FA32DDA99139'
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
if([D2vrProfileGate]::GetPrivateProfileInt('Diagnostics','ShotgunCandidate',0,$diagnosticIni) -ne 0){
    throw 'ShotgunCandidate must be disabled in the installed profile before this trial'
}
foreach($ini in @($gameIni,$localIni)){
    if([D2vrProfileGate]::GetPrivateProfileInt('Render','HeadAttachedTentacles',0,$ini) -ne 1){
        throw "HeadAttachedTentacles changed: $ini"
    }
}
if(!$Install){Write-Output "PREFLIGHT PASS DLL=$candidateHash; nine protected targets; no files changed";return}
$package=Join-Path $local ('packages/shotgun-aim-support-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
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
$diagnosticRaw=[IO.File]::ReadAllText($diagnosticIni)
if(!$diagnosticRaw.StartsWith('[Diagnostics]') -or $diagnosticRaw -match '(?im)^\s*ShotgunCandidate\s*='){
    throw 'Unexpected diagnostic INI format or duplicate ShotgunCandidate key'
}
$stagedDiagnostic=Join-Path $candidateDir 'Darkness2VR-diagnostic.ini'
[IO.File]::WriteAllText($stagedDiagnostic,
    $diagnosticRaw.Replace('[Diagnostics]','[Diagnostics]'+"`r`n"+'ShotgunCandidate=1'),
    [Text.Encoding]::ASCII)
$diagnosticHash=Hash $stagedDiagnostic
$sourceCommit=(git -C $root rev-parse HEAD).Trim()
@{dll_sha256=$candidateHash;diagnostic_ini_sha256=$diagnosticHash;source_commit=$sourceCommit;change='gated shotgun final-shot/model alignment and stock animated left support grip; camera shake unchanged';
  headset_validation='NOT RUN'} | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $candidateDir 'source.json') -Encoding UTF8
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Game started during backup'}
foreach($entry in $before){if((Hash $entry.target) -ne $entry.sha256){throw 'Installed state changed during backup'}}
try {
    Copy-Item -LiteralPath (Join-Path $candidateDir 'd3d9.dll') -Destination $target -Force
    Copy-Item -LiteralPath $stagedDiagnostic -Destination $diagnosticIni -Force
    if((Hash $target) -ne $candidateHash -or (Hash $diagnosticIni) -ne $diagnosticHash -or
       (Hash $exe) -ne $exeHash){throw 'Installed DLL/INI/EXE mismatch'}
    foreach($entry in $before){
        if($entry.target -ne $target -and $entry.target -ne $diagnosticIni -and
           (Hash $entry.target) -ne $entry.sha256){throw "Protected target changed: $($entry.target)"}
    }
} catch { & (Join-Path $rollback 'Restore.ps1');throw }
@{source_commit=$sourceCommit;dll_sha256=$candidateHash;diagnostic_ini_sha256=$diagnosticHash;rollback=$rollback;
  installed_utc=[DateTime]::UtcNow.ToString('o');game_launched=$false} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $package 'deployment.json') -Encoding UTF8
Write-Output "INSTALLED $package DLL=$candidateHash rollback=$rollback; game not launched"
