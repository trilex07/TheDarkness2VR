# Selected-owner feedback, optionally combined with native camera composition.
# Never enables the rejected render-time camera stabilizers.
param([switch]$Install,[switch]$NativeNoShake,[switch]$ModelSnapshotFix,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Fa-f0-9]{64}$')][string]$CandidateHash)
$CandidateHash=$CandidateHash.ToUpperInvariant()
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:/Program Files (x86)/Steam/steamapps/common/Darkness II'
$local='C:/Users/Trilex/AppData/Local/Darkness2VR'
$prior=Join-Path $local 'packages/camera-anim-stabilize-20261101T212812295/rollback'
$buildDir=Join-Path $root 'build/camera-event'
$candidate=Join-Path $buildDir 'd3d9.dll'
$expected='7414984FF998FABDF154AEB5EE443D75805378DDB2359F7EC6F20E83E8A6FA55'
$diagnosticHash='30EDBFD53636856E57312375983749D9FB5D8ABB8548E43EE837F0D4782988FD'
$exeHash='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
if($NativeNoShake){
    $expected='8DB11F12A5032BCAC7FA4806C22E1439DDE63BE5035B244159FDD043EF1A8966'
    $diagnosticHash='D218DB73AF6452218E9AE2829767869304F05EE294A3A88354EC00385FE963F3'
}
if($ModelSnapshotFix){
    if(!$NativeNoShake){throw 'Model snapshot fix requires NativeNoShake'}
    $expected='61C4270D5C9DF9475A8A53C55AA4FD5C122408F51626CDDDC7AB3F305F74CB37'
    $diagnosticHash='AC8290E48D6CD8CA34ED935734E7293572E2F9227B0AA27D4CA03507FA619299'
}
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
$exe=Join-Path $game 'DarknessII.exe'
$target=Join-Path $game 'd3d9.dll'
$gameIni=Join-Path $game 'Darkness2VR.ini'
$localIni=Join-Path $local 'Darkness2VR.ini'
$diagnosticIni=Join-Path $game 'Darkness2VR-diagnostic.ini'
if($Install -and (Get-CimInstance Win32_Process -Filter "Name='DarknessII.exe'")){throw 'Close Darkness II before installation'}
if((Hash $exe) -ne $exeHash -or (Hash $target) -ne $expected){throw 'Installed EXE/DLL identity mismatch'}
if((Hash $diagnosticIni) -ne $diagnosticHash){throw 'Installed diagnostic INI identity mismatch'}
if((Hash $candidate) -ne $candidateHash){throw 'Candidate DLL identity mismatch'}
$manifest=Get-Content -LiteralPath (Join-Path $prior 'manifest.json') -Raw | ConvertFrom-Json
if(@($manifest.before).Count -ne 9){throw 'Expected nine protected targets'}
$gate=& (Join-Path $root 'tools/assert-candidate-profile.ps1') -CandidateDll $candidate `
    -RenderIni @($gameIni,$localIni) -DiagnosticIni $diagnosticIni
if($gate.Flavor -ne 'protected-native'){throw 'Protected-native candidate required'}
if([D2vrProfileGate]::GetPrivateProfileInt('Diagnostics','ShotgunCandidate',0,$diagnosticIni) -ne 1 -or
   [D2vrProfileGate]::GetPrivateProfileInt('Diagnostics','CameraShakeTrace',0,$diagnosticIni) -ne 1){
    throw 'Installed profile must retain ShotgunCandidate=1 and CameraShakeTrace=1'
}
foreach($ini in @($gameIni,$localIni)){
    if([D2vrProfileGate]::GetPrivateProfileInt('Render','HeadAttachedTentacles',0,$ini) -ne 1){
        throw "HeadAttachedTentacles changed: $ini"
    }
}
foreach($key in @('CameraAnimStabilize','CameraShakeSourceSuppress','CameraShakeSuppressHeap','CameraShakeSuppress')) {
    if([D2vrProfileGate]::GetPrivateProfileInt('Diagnostics',$key,0,$diagnosticIni) -ne 0){throw "Rejected camera path enabled: $key"}
}
if(!$Install){Write-Output "PREFLIGHT PASS DLL=$candidateHash; nine protected targets; no files changed";return}
$packageKind=if($ModelSnapshotFix){'camera-model-snapshot-'}elseif($NativeNoShake){'camera-native-no-shake-'}else{'camera-event-rumble-'}
$package=Join-Path $local ('packages/'+$packageKind+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
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
    Copy-Item -LiteralPath (Join-Path $buildDir $name) -Destination (Join-Path $candidateDir $name)
}
if((Hash (Join-Path $candidateDir 'd3d9.dll')) -ne $candidateHash){throw 'Staged candidate mismatch'}
$sourceCommit=(git -C $root rev-parse HEAD).Trim()
@{dll_sha256=$candidateHash;diagnostic_before_sha256=$diagnosticHash;source_commit=$sourceCommit;enable="Diagnostics/CameraEventRumble=1; CameraNativeNoShake=$([int][bool]$NativeNoShake)";change='selected-owner native feedback; optional synchronous native FP composition; shotgun retained';
  headset_validation='NOT RUN'} | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $candidateDir 'source.json') -Encoding UTF8
if(Get-CimInstance Win32_Process -Filter "Name='DarknessII.exe'"){throw 'Game started during backup'}
foreach($entry in $before){if((Hash $entry.target) -ne $entry.sha256){throw 'Installed state changed during backup'}}
if(!('D2vrEventIni' -as [type])){Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;
public static class D2vrEventIni {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
 public static extern bool WritePrivateProfileString(string section,string key,string value,string path);
}
"@}
try {
    Copy-Item -LiteralPath (Join-Path $candidateDir 'd3d9.dll') -Destination $target -Force
    if(![D2vrEventIni]::WritePrivateProfileString('Diagnostics','CameraEventRumble','1',$diagnosticIni)){throw 'Event INI write failed'}
    if([D2vrProfileGate]::GetPrivateProfileInt('Diagnostics','CameraEventRumble',0,$diagnosticIni) -ne 1){throw 'Event INI not readable'}
    if($NativeNoShake){
        if(![D2vrEventIni]::WritePrivateProfileString('Diagnostics','CameraNativeNoShake','1',$diagnosticIni)){throw 'Source INI write failed'}
        if([D2vrProfileGate]::GetPrivateProfileInt('Diagnostics','CameraNativeNoShake',0,$diagnosticIni) -ne 1){throw 'Source INI not readable'}
    }
    $diagnosticHash=Hash $diagnosticIni
    if((Hash $target) -ne $candidateHash -or
       (Hash $exe) -ne $exeHash){throw 'Installed DLL/INI/EXE mismatch'}
    foreach($entry in $before){
        if($entry.target -ne $target -and $entry.target -ne $diagnosticIni -and (Hash $entry.target) -ne $entry.sha256){throw "Protected target changed: $($entry.target)"}
    }
} catch { & (Join-Path $rollback 'Restore.ps1');throw }
@{source_commit=$sourceCommit;dll_sha256=$candidateHash;diagnostic_ini_sha256=$diagnosticHash;rollback=$rollback;
  installed_utc=[DateTime]::UtcNow.ToString('o');game_launched=$false} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $package 'deployment.json') -Encoding UTF8
Write-Output "INSTALLED $package DLL=$candidateHash rollback=$rollback; game not launched"
