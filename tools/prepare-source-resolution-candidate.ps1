# MAIN source supersampling trial; extended Exact bound, exact current rollback.
param([string]$OutputDirectory,[string]$AcceptedPackage)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build/source-resolution-3200-20261103'}
if(!$AcceptedPackage){$AcceptedPackage=Join-Path $root 'build/gaze-fxaa-nis-20261103'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Keep frozen packages; output already exists'}
if(git -C $root status --porcelain --untracked-files=no){throw 'Commit tracked source before freeze'}
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
$accepted=Get-Content -LiteralPath (Join-Path $AcceptedPackage 'installed.json') -Raw|ConvertFrom-Json
$prior=Get-Content -LiteralPath (Join-Path $AcceptedPackage 'candidate.json') -Raw|ConvertFrom-Json
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
if($accepted.mode -ne 'Off' -or $accepted.after.Count -ne 2 -or
 $accepted.after[0].target -ne (Join-Path $game 'd3d9.dll') -or $accepted.after[1].target -ne (Join-Path $game 'Darkness2VR.ini')){throw 'Expected current FXAA-Off targets'}
foreach($e in @($accepted.after)+@($prior.protected)){if((Hash $e.target) -ne $e.sha256){throw "Installed preimage mismatch: $($e.target)"}}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$out=(Resolve-Path -LiteralPath $OutputDirectory).Path
$files=@()
foreach($name in @('d3d9.dll','d3d9.pdb','d3d9.map')){
 Copy-Item -LiteralPath (Join-Path $root $name) -Destination (Join-Path $out $name)
 $files+=@{name=$name;sha256=(Hash (Join-Path $out $name))}
}
Copy-Item -LiteralPath $accepted.after[0].target -Destination (Join-Path $out 'baseline.dll')
Copy-Item -LiteralPath $accepted.after[1].target -Destination (Join-Path $out 'baseline.ini')
if(!('D2vrResolutionProfile' -as [type])){Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class D2vrResolutionProfile {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string s,string k,string v,string p);
}
'@}
$baseline=[IO.File]::ReadAllText((Join-Path $out 'baseline.ini'))
foreach($required in @('(?im)^SourceQuality=4\s*$','(?im)^RenderWidth=2676\s*$','(?im)^RenderHeight=2728\s*$','(?im)^NisMode=2\s*$','(?im)^GazeFXAA=0\s*$')){if($baseline -notmatch $required){throw 'Unexpected source baseline'}}
$remove='(?im)^\s*(?:RenderWidth|RenderHeight)\s*=.*(?:\r?\n|$)'
foreach($profile in @(@('candidate.ini','3200','3264'),@('off.ini','2676','2728'))){
 $p=Join-Path $out $profile[0]
 Copy-Item -LiteralPath (Join-Path $out 'baseline.ini') -Destination $p
 if(![D2vrResolutionProfile]::WritePrivateProfileString('Render','RenderWidth',$profile[1],$p)){throw 'Profile write failed'}
 if(![D2vrResolutionProfile]::WritePrivateProfileString('Render','RenderHeight',$profile[2],$p)){throw 'Profile write failed'}
 $contents=[IO.File]::ReadAllText($p)
 if([regex]::Replace($baseline,$remove,'').TrimEnd() -cne [regex]::Replace($contents,$remove,'').TrimEnd()){throw 'Only RenderWidth/RenderHeight may change'}
 & (Join-Path $PSScriptRoot 'assert-candidate-profile.ps1') -CandidateDll (Join-Path $out 'd3d9.dll') -RenderIni @($p) -DiagnosticIni (Join-Path $game 'Darkness2VR-diagnostic.ini')|Out-Null
 $files+=@{name=$profile[0];sha256=(Hash $p)}
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'performance-candidate/Install.ps1') -Destination (Join-Path $out 'Install.ps1')
$files+=@{name='Install.ps1';sha256=(Hash (Join-Path $out 'Install.ps1'))}
@{source_commit=(git -C $root rev-parse HEAD).Trim();build_flavor='protected-native /O2 x86';default_mode='Combined';
 profiles=@{Combined='candidate.ini';Off='off.ini'};files=$files;before=$accepted.after;protected=$prior.protected;
 previous_package=$AcceptedPackage;installed=$false;headset='NOT RUN';
 limits='Combined requests native scene3200x3264 through SourceQuality4 with explicit Exact pixel limit extended to10444800. Off requests previous2676x2728 on the new DLL. No projection/FOV/XR recommendation/NIS/AF/FXAA change. Restore returns exact pre-trial DLL/INI. Requested sizes are not internal buffer proof or visual/FPS acceptance.'}|
 ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $out 'candidate.json') -Encoding utf8
foreach($e in @($accepted.after)+@($prior.protected)){if((Hash $e.target) -ne $e.sha256){throw 'Installed preimage changed during preparation'}}
Write-Output "PREPARED ONLY $out; Steam unchanged; default Combined source3200x3264; Exact bound extended; exact Restore available"
