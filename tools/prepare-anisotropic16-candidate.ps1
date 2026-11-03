# MAIN configuration-only trial of existing world diffuse AF16; exact current rollback.
param([string]$OutputDirectory,[string]$AcceptedPackage)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build/anisotropic-16x-20261103'}
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
if((Hash (Join-Path $root 'd3d9.dll')) -ne $accepted.after[0].sha256){throw 'Configuration-only trial requires identical DLL'}
$files=@()
foreach($name in @('d3d9.dll','d3d9.pdb','d3d9.map')){
 Copy-Item -LiteralPath (Join-Path $root $name) -Destination (Join-Path $out $name)
 $files+=@{name=$name;sha256=(Hash (Join-Path $out $name))}
}
Copy-Item -LiteralPath $accepted.after[0].target -Destination (Join-Path $out 'baseline.dll')
Copy-Item -LiteralPath $accepted.after[1].target -Destination (Join-Path $out 'baseline.ini')
if(!('D2vrAnisoProfile' -as [type])){Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class D2vrAnisoProfile {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string s,string k,string v,string p);
}
'@}
$baseline=[IO.File]::ReadAllText((Join-Path $out 'baseline.ini'))
$remove='(?im)^\s*(?:WorldDiffuseAnisotropy|TextureAnisotropy)\s*=.*(?:\r?\n|$)'
foreach($profile in @(@('candidate.ini','1'),@('off.ini','0'))){
 $p=Join-Path $out $profile[0]
 Copy-Item -LiteralPath (Join-Path $out 'baseline.ini') -Destination $p
 if(![D2vrAnisoProfile]::WritePrivateProfileString('Render','WorldDiffuseAnisotropy',$profile[1],$p)){throw 'Profile write failed'}
 if(![D2vrAnisoProfile]::WritePrivateProfileString('Render','TextureAnisotropy','16',$p)){throw 'Profile write failed'}
 $contents=[IO.File]::ReadAllText($p)
 if([regex]::Replace($baseline,$remove,'').TrimEnd() -cne [regex]::Replace($contents,$remove,'').TrimEnd()){throw 'Only WorldDiffuseAnisotropy/TextureAnisotropy may change'}
 & (Join-Path $PSScriptRoot 'assert-candidate-profile.ps1') -CandidateDll (Join-Path $out 'd3d9.dll') -RenderIni @($p) -DiagnosticIni (Join-Path $game 'Darkness2VR-diagnostic.ini')|Out-Null
 $files+=@{name=$profile[0];sha256=(Hash $p)}
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'performance-candidate/Install.ps1') -Destination (Join-Path $out 'Install.ps1')
$files+=@{name='Install.ps1';sha256=(Hash (Join-Path $out 'Install.ps1'))}
@{source_commit=(git -C $root rev-parse HEAD).Trim();build_flavor='protected-native /O2 x86';default_mode='Combined';
 profiles=@{Combined='candidate.ini';Off='off.ini'};files=$files;before=$accepted.after;protected=$prior.protected;
 previous_package=$AcceptedPackage;installed=$false;headset='NOT RUN';
 limits='Combined enables existing WorldDiffuseAnisotropy=1 with TextureAnisotropy=16 for exact VS293F456B/PSCA08BDC4 s0 material only. Off disables the scoped promotion. DLL is byte-identical to installed FXAA-Off. Restore returns exact prior DLL/INI. Resolution and FXAA/NIS unchanged; no shimmer or coverage guarantee.'}|
 ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $out 'candidate.json') -Encoding utf8
foreach($e in @($accepted.after)+@($prior.protected)){if((Hash $e.target) -ne $e.sha256){throw 'Installed preimage changed during preparation'}}
Write-Output "PREPARED ONLY $out; Steam unchanged; default Combined scoped AF16; unchanged DLL; exact Restore available"
