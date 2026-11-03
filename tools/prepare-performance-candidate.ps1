# MAIN package preparation only. No game launch, registry or installed-file writes.
param([string]$OutputDirectory,[string]$PreviousPackage='C:\Users\Trilex\AppData\Local\Darkness2VR\packages\cinematic-theater-20261102T193328001',[ValidateSet('Cache','Packets','Combined')][string]$DefaultMode='Cache')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(!$OutputDirectory){$OutputDirectory=Join-Path $root ('build/performance-candidate-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Output already exists; preserve frozen candidates'}
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
$previous=Get-Content -LiteralPath (Join-Path $PreviousPackage 'deployment.json') -Raw | ConvertFrom-Json
$dll=Join-Path $game 'd3d9.dll';$ini=Join-Path $game 'Darkness2VR.ini'
if($previous.after.Count -ne 2 -or $previous.after[0].target -ne $dll -or $previous.after[1].target -ne $ini){throw 'Unexpected baseline targets'}
foreach($entry in @($previous.after)+@($previous.protected)){if((Hash $entry.target) -ne $entry.sha256){throw "Baseline mismatch: $($entry.target)"}}
if(git -C $root status --porcelain --untracked-files=no){throw 'Commit tracked source before freezing package'}
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$out=(Resolve-Path -LiteralPath $OutputDirectory).Path
$files=@()
foreach($name in @('d3d9.dll','d3d9.map','d3d9.pdb')){
 $from=Join-Path $root $name;$to=Join-Path $out $name
 Copy-Item -LiteralPath $from -Destination $to
 if((Hash $from) -ne (Hash $to)){throw 'Candidate copy mismatch'}
 $files+=@{name=$name;sha256=(Hash $to)}
}
Copy-Item -LiteralPath $ini -Destination (Join-Path $out 'baseline.ini')
Copy-Item -LiteralPath $dll -Destination (Join-Path $out 'baseline.dll')
Copy-Item -LiteralPath $ini -Destination (Join-Path $out 'candidate.ini')
if(!('D2vrPerfProfile' -as [type])){Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;
public static class D2vrPerfProfile {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string s,string k,string v,string p);
}
"@}
foreach($key in @('ShaderDemandCache','ReplayPrepareWorker','FullPacketPrepareWorker','NativeMaterialFilter','EarlyPacketPreparation','OverlapNativePresent')){
 $value=if($key -eq 'ShaderDemandCache'){'1'}else{'0'}
 if(![D2vrPerfProfile]::WritePrivateProfileString('Render',$key,$value,(Join-Path $out 'candidate.ini'))){throw 'Candidate INI preparation failed'}
}
$original=[IO.File]::ReadAllText((Join-Path $out 'baseline.ini'))
$prepared=[IO.File]::ReadAllText((Join-Path $out 'candidate.ini'))
$remove='(?im)^\s*(ShaderDemandCache|ReplayPrepareWorker|FullPacketPrepareWorker|NativeMaterialFilter|EarlyPacketPreparation|OverlapNativePresent)\s*=.*(?:\r?\n|$)'
if([regex]::Replace($original,$remove,'').TrimEnd() -cne [regex]::Replace($prepared,$remove,'').TrimEnd()){throw 'Candidate changes settings beyond the six performance switches'}
& (Join-Path $PSScriptRoot 'assert-candidate-profile.ps1') -CandidateDll (Join-Path $out 'd3d9.dll') -RenderIni @((Join-Path $out 'candidate.ini')) -DiagnosticIni (Join-Path $game 'Darkness2VR-diagnostic.ini') | Out-Host
$files+=@{name='candidate.ini';sha256=(Hash (Join-Path $out 'candidate.ini'))}
$profiles=@{Both='both.ini';Cache='candidate.ini';Worker='worker.ini';Packets='packets.ini';Material='material.ini';Overlap='overlap.ini';Combined='combined.ini';Off='off.ini'}
foreach($mode in @('Both','Worker','Packets','Material','Overlap','Combined','Off')){
 $path=Join-Path $out $profiles[$mode]
 Copy-Item -LiteralPath (Join-Path $out 'candidate.ini') -Destination $path
 $cache=if($mode -in @('Both','Combined')){'1'}else{'0'};$worker=if($mode -in @('Worker','Both')){'1'}else{'0'}
 if(![D2vrPerfProfile]::WritePrivateProfileString('Render','ShaderDemandCache',$cache,$path) -or ![D2vrPerfProfile]::WritePrivateProfileString('Render','ReplayPrepareWorker',$worker,$path)){throw 'A/B profile write failed'}
 $packets=if($mode -in @('Packets','Combined')){'1'}else{'0'}
 if(![D2vrPerfProfile]::WritePrivateProfileString('Render','FullPacketPrepareWorker',$packets,$path)){throw 'Packet profile write failed'}
 foreach($key in @('NativeMaterialFilter','EarlyPacketPreparation','OverlapNativePresent')){
  $enabled=($mode -eq 'Combined') -or ($mode -eq 'Material' -and $key -eq 'NativeMaterialFilter') -or
      ($mode -eq 'Packets' -and $key -eq 'EarlyPacketPreparation') -or ($mode -eq 'Overlap' -and $key -eq 'OverlapNativePresent')
  if(![D2vrPerfProfile]::WritePrivateProfileString('Render',$key,$(if($enabled){'1'}else{'0'}),$path)){throw 'Combined profile write failed'}
 }
 $contents=[IO.File]::ReadAllText($path)
 if([regex]::Replace($original,$remove,'').TrimEnd() -cne [regex]::Replace($contents,$remove,'').TrimEnd()){throw "Unexpected profile changes: $mode"}
 & (Join-Path $PSScriptRoot 'assert-candidate-profile.ps1') -CandidateDll (Join-Path $out 'd3d9.dll') -RenderIni @($path) -DiagnosticIni (Join-Path $game 'Darkness2VR-diagnostic.ini') | Out-Null
 $files+=@{name=$profiles[$mode];sha256=(Hash $path)}
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'performance-candidate/Install.ps1') -Destination (Join-Path $out 'Install.ps1')
$files+=@{name='Install.ps1';sha256=(Hash (Join-Path $out 'Install.ps1'))}
$manifest=@{source_commit=(git -C $root rev-parse HEAD).Trim();build_flavor='protected-native /O2 x86';headset='NOT RUN';installed=$false;default_mode=$DefaultMode;profiles=$profiles;files=$files;before=$previous.after;protected=$previous.protected;previous_package=$PreviousPackage;limits='Parallel work covers owned replay VS packets, not native object/material-list production. Worker ceiling remains small; runtime FPS NOT RUN.'}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $out 'candidate.json') -Encoding UTF8
# Preimages remain unchanged throughout preparation. No live cache is modified.
foreach($entry in @($previous.after)+@($previous.protected)){if((Hash $entry.target) -ne $entry.sha256){throw 'Baseline changed during preparation'}}
Write-Output "PREPARED ONLY: $out"
Write-Output "Install with game closed: powershell -ExecutionPolicy Bypass -File `"$out\Install.ps1`""
