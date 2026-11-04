param([string]$OutputDirectory,
 [ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedInstalledDll='4A0320B2C9369EA8696C65786B4A871434D855CB62540859209DC95A4F559EC7',
 [ValidatePattern('^[0-9A-Fa-f]{64}$')][string]$ExpectedInstalledDiagnostic='3A3FD69804D0E62A66E699652AB89C6BCD086CCB271CBE31A65B97F8071E6A6A')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build/combined-scenes-final-20261103'}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close game and restore simulator lab first'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Frozen output already exists'}
if(git -C $root status --porcelain --untracked-files=no){throw 'Commit tracked source first'}
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$dll=Join-Path $game 'd3d9.dll';$diag=Join-Path $game 'Darkness2VR-diagnostic.ini'
if((Hash $dll) -ne $ExpectedInstalledDll -or (Hash $diag) -ne $ExpectedInstalledDiagnostic){
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
foreach($name in 'candidate-diagnostic.ini','shadow-experiment.ini','reflection-experiment.ini','smaa-experiment.ini','material-experiment.ini'){
 $p=Join-Path $out $name;Copy-Item -LiteralPath $diag -Destination $p
 foreach($key in 'MatchedFrameCapture','CommandHistory','CameraShakeTrace','VideoDrawProbe','CpuSyncTiming','CapturePassStages','MatchedCaptureSerial','HookCostSampling'){
  if(![D2vrCombinedProfile]::WritePrivateProfileString('Diagnostics',$key,'0',$p)){throw 'Profile write failed'}
 }
 if(![D2vrCombinedProfile]::WritePrivateProfileString('Diagnostics','ShadowClipCoordinates',$(if($name -eq 'shadow-experiment.ini'){'1'}else{'0'}),$p)){throw 'Profile write failed'}
 if(![D2vrCombinedProfile]::WritePrivateProfileString('Diagnostics','ReflectionEyeExperiment',$(if($name -eq 'reflection-experiment.ini'){'1'}else{'0'}),$p)){throw 'Profile write failed'}
 if(![D2vrCombinedProfile]::WritePrivateProfileString('Diagnostics','SmaaExperiment',$(if($name -eq 'smaa-experiment.ini'){'1'}else{'0'}),$p)){throw 'Profile write failed'}
 foreach($key in 'SpecularFootprintExperiment','SkinMipFilterExperiment'){
  if(![D2vrCombinedProfile]::WritePrivateProfileString('Diagnostics',$key,$(if($name -eq 'material-experiment.ini'){'1'}else{'0'}),$p)){throw 'Profile write failed'}
 }
 foreach($key in 'SharedCaptureRotationExperiment','SharedReaderCompletionEventExperiment','SharedReaderSleepUsExperiment'){
  if(![D2vrCombinedProfile]::WritePrivateProfileString('Diagnostics',$key,'0',$p)){throw 'Profile write failed'}
 }
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
Install.ps1 -Mode SmaaExperiment adds SMAA 1x HIGH AFTER NIS to Combined.
It preserves resolution, FXAA OFF and all scene/input changes. Shadow and
reflection experiments stay OFF. No temporal history or motion-vector guesses.
Offline RTX5070 fixture: both3232x3300 eyes GPU~1.25ms, CPU enqueue~0.001ms;
not a game FPS result. Flat interiors are exact, diagonal edges receive coverage.
Actual headset shimmer/clarity acceptance remains NOT RUN.
Install.ps1 -Mode MaterialExperiment enables the exact native skin shader's
spatial specular footprint and normal-map mip interpolation. It preserves
geometry resolution, NIS, all scene/input fixes and keeps SMAA/FXAA OFF.
Actual GPU fixtures reduce temporal specular variation and mip transitions;
these do not anti-alias mesh silhouettes. Headset effectiveness NOT RUN.
Supersampling remains a separate emulator-only experiment, not in this package.
No new GPU wait, reader polling or capture rotation default is enabled.
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
 'candidate-diagnostic.ini','shadow-experiment.ini','reflection-experiment.ini','smaa-experiment.ini','material-experiment.ini','Install.ps1','README.txt')
$files=@(foreach($name in $names){@{name=$name;sha256=(Hash (Join-Path $out $name))}})
@{source_commit=(git -C $root rev-parse HEAD).Trim();build_flavor='protected-native /O2 x86';
 default_mode='Combined';files=$files;before=$before;protected=$protected;installed=$false;
 headset='NOT RUN';performance='unresolved';floor_reflections='unresolved';shadow_default='OFF';reflection_default='OFF';smaa_default='OFF'}|
 ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $out 'candidate.json') -Encoding UTF8
& (Join-Path $out 'Install.ps1') -VerifyOnly
Write-Output "PREPARED $out; Steam accepted baseline retained"
