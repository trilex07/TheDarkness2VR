param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build/combined-scenes-final-20261103'}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close game and restore simulator lab first'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Frozen output already exists'}
if(git -C $root status --porcelain --untracked-files=no){throw 'Commit tracked source first'}
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$dll=Join-Path $game 'd3d9.dll';$diag=Join-Path $game 'Darkness2VR-diagnostic.ini'
if((Hash $dll) -ne 'E56BF027875A6FF3997E0B6A51A0DC43F52DD86C2EB39AE79D4C5EAB073FA5CA' -or
 (Hash $diag) -ne 'AC8290E48D6CD8CA34ED935734E7293572E2F9227B0AA27D4CA03507FA619299'){
 throw 'Exact accepted lab rollback must be installed before freeze'
}
$before=@(@{target=$dll;sha256=(Hash $dll)},@{target=$diag;sha256=(Hash $diag)})
$protected=@(foreach($name in 'DarknessII.exe','Darkness2VR.ini','openxr_loader.dll'){
 $p=Join-Path $game $name;@{target=$p;sha256=(Hash $p)}
})
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$out=(Resolve-Path -LiteralPath $OutputDirectory).Path
foreach($name in 'd3d9.dll','d3d9.pdb','d3d9.map'){
 Copy-Item -LiteralPath (Join-Path $root $name) -Destination (Join-Path $out $name)
}
Copy-Item -LiteralPath $dll -Destination (Join-Path $out 'baseline.dll')
Copy-Item -LiteralPath $diag -Destination (Join-Path $out 'baseline-diagnostic.ini')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'combined-scenes-candidate/Install.ps1') -Destination (Join-Path $out 'Install.ps1')
if(!('D2vrCombinedProfile' -as [type])){Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class D2vrCombinedProfile {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string section,string key,string value,string path);
}
'@}
foreach($name in 'candidate-diagnostic.ini','shadow-experiment.ini','reflection-experiment.ini'){
 $p=Join-Path $out $name;Copy-Item -LiteralPath $diag -Destination $p
 foreach($key in 'MatchedFrameCapture','CommandHistory','CameraShakeTrace','VideoDrawProbe','CpuSyncTiming','CapturePassStages','MatchedCaptureSerial'){
  if(![D2vrCombinedProfile]::WritePrivateProfileString('Diagnostics',$key,'0',$p)){throw 'Profile write failed'}
 }
 if(![D2vrCombinedProfile]::WritePrivateProfileString('Diagnostics','ShadowClipCoordinates',$(if($name -eq 'shadow-experiment.ini'){'1'}else{'0'}),$p)){throw 'Profile write failed'}
 if(![D2vrCombinedProfile]::WritePrivateProfileString('Diagnostics','ReflectionEyeExperiment',$(if($name -eq 'reflection-experiment.ini'){'1'}else{'0'}),$p)){throw 'Profile write failed'}
 if(![D2vrCombinedProfile]::WritePrivateProfileString('Simulator','Enabled','0',$p)){throw 'Profile write failed'}
 & (Join-Path $PSScriptRoot 'assert-candidate-profile.ps1') -CandidateDll (Join-Path $out 'd3d9.dll') -RenderIni @((Join-Path $game 'Darkness2VR.ini')) -DiagnosticIni $p|Out-Null
}
$readme=@'
One combined candidate: exact native scene flags, final-target movie detection,
protected contextual buttons and selected-player fallback; accepted rendering
optimizations retained. Triangle: first press sends native Y immediately;
second short press within350ms pauses in ordinary play/dialogue. Protected
QTE/execution/story choices keep rapid buttons. L3+R3 manual60-second override
remains available. Resolution and main rendering INI are unchanged.

Install.ps1 -VerifyOnly checks without writes. Install.ps1 installs Combined.
Install.ps1 -Restore restores the exact accepted DLL/diagnostic INI.
Install.ps1 -Mode ShadowExperiment enables ONLY the pinned shadow experiment.
Install.ps1 -Mode ReflectionExperiment enables ONLY the exact shader-pair eye
coordinate experiment. Both experiments are OFF in default Combined mode.
Close the game before each operation. No automatic launch or runtime switch.

PASS: source/build/fixture checks, simulator intro transitions, villa hands and
non-cinema walking, native passive/interactive restaurant transition, double-Y
pause. Actual native QTE/execution/story/Darkling/carnival scenes NOT RUN.
Headset acceptance NOT RUN. No demonstrated recovery of user25ms/40FPS.
Floor reflections remain unresolved. Shadow reconstruction algebra is improved
but no demonstrated OFF/ON visual improvement; its default remains OFF.
Reflection experiment: actual two-eye draw constants agree with rendered eye
after rotation and15cm head translation; visual floor-artifact acceptance is
NOT established. Its extra driver queries/setters are not an FPS optimization.
This package is a reviewable partial candidate, not a declaration all requested
rendering/performance issues are solved. Private captures/saves are excluded.
'@
[IO.File]::WriteAllText((Join-Path $out 'README.txt'),$readme,[Text.Encoding]::UTF8)
$names=@('d3d9.dll','d3d9.pdb','d3d9.map','baseline.dll','baseline-diagnostic.ini',
 'candidate-diagnostic.ini','shadow-experiment.ini','reflection-experiment.ini','Install.ps1','README.txt')
$files=@(foreach($name in $names){@{name=$name;sha256=(Hash (Join-Path $out $name))}})
@{source_commit=(git -C $root rev-parse HEAD).Trim();build_flavor='protected-native /O2 x86';
 default_mode='Combined';files=$files;before=$before;protected=$protected;installed=$false;
 headset='NOT RUN';performance='unresolved';floor_reflections='unresolved';shadow_default='OFF';reflection_default='OFF'}|
 ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $out 'candidate.json') -Encoding UTF8
& (Join-Path $out 'Install.ps1') -VerifyOnly
Write-Output "PREPARED $out; Steam accepted baseline retained"
