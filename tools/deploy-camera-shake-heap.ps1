# Build, commit and protected install of the heap-only camera-shake candidate.
# Usage (game CLOSED):  powershell -ExecutionPolicy Bypass -File tools\deploy-camera-shake-heap.ps1 -Install
param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:/Program Files (x86)/Steam/steamapps/common/Darkness II'
$local='C:/Users/Trilex/AppData/Local/Darkness2VR'
$prior=Join-Path $local 'packages/camera-shake-owner-trace-20261101T181538632/rollback'
$expected='7414984FF998FABDF154AEB5EE443D75805378DDB2359F7EC6F20E83E8A6FA55'
$diagnosticHash='30EDBFD53636856E57312375983749D9FB5D8ABB8548E43EE837F0D4782988FD'
$exeHash='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
$exe=Join-Path $game 'DarknessII.exe'
$target=Join-Path $game 'd3d9.dll'
$gameIni=Join-Path $game 'Darkness2VR.ini'
$localIni=Join-Path $local 'Darkness2VR.ini'
$diagnosticIni=Join-Path $game 'Darkness2VR-diagnostic.ini'
$candidate=Join-Path $root 'd3d9.dll'
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II before installation'}
if((Hash $exe) -ne $exeHash -or (Hash $target) -ne $expected){throw 'Installed EXE/DLL identity mismatch'}
if((Hash $diagnosticIni) -ne $diagnosticHash){throw 'Installed diagnostic INI identity mismatch'}
$manifest=Get-Content -LiteralPath (Join-Path $prior 'manifest.json') -Raw | ConvertFrom-Json
if(@($manifest.before).Count -ne 9){throw 'Expected nine protected targets'}

# 1. Commit the source on the MAIN integration branch.
$branch=(git -C $root rev-parse --abbrev-ref HEAD).Trim()
if($branch -ne 'codex/gaze-nis-main-integration'){throw "Unexpected branch $branch"}
git -C $root add -- src/d3d9.cpp src/native_aim.inl tools/deploy-camera-shake-heap.ps1 docs/research/darkness2-camera-shake-20261101.md docs/agent-workflow/handoffs/HANDOFF-shotgun-twohand-shake-20261101.md
if(git -C $root diff --cached --name-only){
    git -C $root commit -q -m "Heap-only camera shake suppression candidate (default OFF)" -m "Zero mShakeFactorRot/mShakeFactorPos of the four first-person procedural shake blocks on the verified 01001630+5E0 -> 00FDF170 owner. No EXE writes; impact springs measured only. Headset result NOT RUN." -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>" -m "Claude-Session: https://claude.ai/code/session_01FyWRDxP6vih9TJ4UkrV5Bn"
    if($LASTEXITCODE){throw 'git commit failed'}
}
$sourceCommit=(git -C $root rev-parse HEAD).Trim()

# 2. Build protected-native x86 and gate it.
& (Join-Path $root 'build.ps1') -DiagnosticCapture
if(!(Test-Path -LiteralPath $candidate)){throw 'Build produced no d3d9.dll'}
$candidateHash=Hash $candidate
$text=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($candidate))
if(!$text.Contains('CameraShakeSuppressHeap')){throw 'Candidate lacks CameraShakeSuppressHeap'}
$gate=& (Join-Path $root 'tools/assert-candidate-profile.ps1') -CandidateDll $candidate `
    -RenderIni @($gameIni,$localIni) -DiagnosticIni $diagnosticIni
if($gate.Flavor -ne 'protected-native'){throw 'Protected-native candidate required'}
if([D2vrProfileGate]::GetPrivateProfileInt('Diagnostics','ShotgunCandidate',0,$diagnosticIni) -ne 1){throw 'ShotgunCandidate=1 must be retained'}
if(!$Install){Write-Output "PREFLIGHT PASS commit=$sourceCommit DLL=$candidateHash; nothing installed";return}

# 3. Back up the nine protected targets, then install DLL + one INI key.
$package=Join-Path $local ('packages/camera-shake-heap-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
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
    $p=Join-Path $root $name; if(Test-Path -LiteralPath $p){Copy-Item -LiteralPath $p -Destination (Join-Path $candidateDir $name)}
}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Game started during backup'}
if(!('D2vrIniWrite' -as [type])){Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;
public static class D2vrIniWrite {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
 public static extern bool WritePrivateProfileString(string section,string key,string value,string path);
}
"@}
try {
    Copy-Item -LiteralPath (Join-Path $candidateDir 'd3d9.dll') -Destination $target -Force
    if(![D2vrIniWrite]::WritePrivateProfileString('Diagnostics','CameraShakeSuppressHeap','1',$diagnosticIni)){throw 'INI write failed'}
    if((Hash $target) -ne $candidateHash -or (Hash $exe) -ne $exeHash){throw 'Installed DLL/EXE mismatch'}
    if([D2vrProfileGate]::GetPrivateProfileInt('Diagnostics','CameraShakeSuppressHeap',0,$diagnosticIni) -ne 1){throw 'INI key not readable'}
    foreach($entry in $before){
        if($entry.target -ne $target -and $entry.target -ne $diagnosticIni -and (Hash $entry.target) -ne $entry.sha256){throw "Protected target changed: $($entry.target)"}
    }
} catch { & (Join-Path $rollback 'Restore.ps1');throw }
$iniHash=Hash $diagnosticIni
@{source_commit=$sourceCommit;dll_sha256=$candidateHash;diagnostic_ini_sha256=$iniHash;rollback=$rollback;
  change='heap-only zero of procedural shake factors; shotgun candidate and trace retained';
  installed_utc=[DateTime]::UtcNow.ToString('o');game_launched=$false;headset_validation='NOT RUN'} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $package 'deployment.json') -Encoding UTF8
Write-Output "INSTALLED commit=$sourceCommit DLL=$candidateHash INI=$iniHash"
Write-Output "ROLLBACK: powershell -ExecutionPolicy Bypass -File `"$rollback\Restore.ps1`""
