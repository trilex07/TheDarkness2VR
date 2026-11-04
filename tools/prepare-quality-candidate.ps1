param([Parameter(Mandatory=$true)][string]$OutputDirectory,
 [string]$MaterialPackage='build/combined-material-20261104')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close game first'}
if(git -C $root status --porcelain --untracked-files=no){throw 'Commit tracked source first'}
if(Test-Path $OutputDirectory){throw 'Frozen output exists'}
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$source=Get-Content (Join-Path $MaterialPackage 'candidate.json') -Raw|ConvertFrom-Json
foreach($f in $source.files){if($f.name -ne [IO.Path]::GetFileName($f.name) -or (Hash (Join-Path $MaterialPackage $f.name)) -ne $f.sha256){throw 'Material package changed'}}
foreach($f in @($source.before)+@($source.protected)){if((Hash $f.target) -ne $f.sha256){throw 'Restore exact user preimage first'}}
# Only this measured diagnostic DLL was exercised with resource-only Mode5.
if((Hash (Join-Path $MaterialPackage 'd3d9.dll')) -ne 'E50496EBFC598A198A6B012C93765B3B076B9DA78D4DFBD0E538B0F31F737B47'){throw 'Untested coverage build'}
New-Item -ItemType Directory $OutputDirectory|Out-Null
$out=(Resolve-Path $OutputDirectory).Path
foreach($name in 'd3d9.dll','d3d9.pdb','d3d9.map','baseline.dll','baseline-diagnostic.ini','candidate-diagnostic.ini','material-experiment.ini'){
 Copy-Item (Join-Path $MaterialPackage $name) (Join-Path $out $name)
}
Copy-Item (Join-Path $game 'Darkness2VR.ini') (Join-Path $out 'baseline-render.ini')
Copy-Item (Join-Path $game 'Darkness2VR.ini') (Join-Path $out 'coverage-render.ini')
if(!( 'D2QualityProfile' -as [type])){Add-Type 'using System.Runtime.InteropServices; public static class D2QualityProfile { [DllImport("kernel32",CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string s,string k,string v,string p); }'}
$profile=Join-Path $out 'coverage-render.ini'
foreach($kv in @(@('RenderWidth','3345'),@('RenderHeight','3410'),@('SourceQuality','5'),@('MatchEyeResolutionToSource','0'))){
 if(![D2QualityProfile]::WritePrivateProfileString('Render',$kv[0],$kv[1],$profile)){throw 'Profile write failed'}
}
$remove='(?im)^\s*(?:RenderWidth|RenderHeight|SourceQuality|MatchEyeResolutionToSource)\s*=.*(?:\r?\n|$)'
if([regex]::Replace([IO.File]::ReadAllText((Join-Path $out 'baseline-render.ini')),$remove,'').TrimEnd() -cne [regex]::Replace([IO.File]::ReadAllText($profile),$remove,'').TrimEnd()){throw 'Unexpected render profile mutation'}
Copy-Item (Join-Path $PSScriptRoot 'quality-candidate/Install.ps1') (Join-Path $out 'Install.ps1')
$readme=@'
Three reversible modes on one measured x86 diagnostic DLL. Close game first.
Install.ps1 -Mode Combined: new DLL, original render INI, experiments OFF.
Install.ps1 -Mode Material: exact skin specular footprint + normal-map mip
interpolation; original render INI. No FXAA/SMAA/global AF16, no resolution change.
Install.ps1 -Mode Coverage: Material plus native geometry/depth3345x3410,
final XR eyes2676x2728. Resource-only sizing; physical window RECT preserved.
Install.ps1 -Restore: exact user4A DLL,3A diagnostic and68EE render INI.
Install.ps1 -VerifyOnly: hash/ownership checks without writes.

All scene/input/isolated-eye optimizations retained. Simulator and collectors
OFF. No runtime switch, priority change or game launch. New waits/backoff/slot
rotation OFF. EXE/loader protected. Default Combined leaves experiments OFF.

Actual GPU fixtures: better synthetic subpixel edge coverage, smoother skin
normal mip transitions, reduced specular variation. Both eyes/hands present
in emulator Villa. Clean Villa: native~74-76 and Coverage~75 world-bearing
layers/s. Not simulation FPS, headset acceptance or worst-case combat proof.
Same native session fell36-43 under concurrent audio-generation CPU load and
returned~75 after it ended. Raising game priority did not help.

November4 restaurant continuation: resource-only native control and Coverage
both reached90 world-bearing layers/s without the audio Python jobs. Strict
normal-3D-only windows: native8 windows69.6-90.0 (mean85.9), Coverage10 windows
89.0-90.0 (mean89.68). Independent trials, different warmup/capture history;
NOT evidence that Coverage improves performance. Earlier loaded restaurant
Coverage had62.8/66.2 while audio jobs used~7 logical cores. Two tracked hands
and pistol were visible and moved in the earlier Coverage trial. Actual firing
and worst-case outdoor combat are NOT verified. Automatic-mode transitions,
video and mixed windows excluded by tools/perf/summarize_world_cadence.py.

OPEN: headset shimmer, large-combat FPS and floor/shadow visual acceptance.
Material does not anti-alias mesh silhouettes. Coverage is experimental and
costs56.25% more source pixels; do not assume free performance in all scenes.
The material fixture reduces mean specular brightness~9%; judge skin highlights
in the headset. Spatial coverage tests cannot certify temporal headset quality.
No private captures, saved games or external tool binaries in this package.
'@
[IO.File]::WriteAllText((Join-Path $out 'README.txt'),$readme,[Text.Encoding]::UTF8)
$before=@(foreach($pair in @(@('d3d9.dll','baseline.dll'),@('Darkness2VR-diagnostic.ini','baseline-diagnostic.ini'),@('Darkness2VR.ini','baseline-render.ini'))){
 $target=Join-Path $game $pair[0];$hash=Hash $target
 if((Hash (Join-Path $out $pair[1])) -ne $hash){throw 'Backup mismatch'}
 @{target=$target;sha256=$hash}
})
$protected=@($source.protected|Where-Object {$_.target -ne (Join-Path $game 'Darkness2VR.ini')})
if($protected.Count -ne 2){throw 'Unexpected protected set'}
$files=@(Get-ChildItem $out -File|ForEach-Object {@{name=$_.Name;sha256=(Hash $_.FullName)}})
@{source_commit=(git -C $root rev-parse HEAD).Trim();runtime_source=$source.source_commit;before=$before;protected=$protected;files=$files;
 profiles=@{Combined=@('d3d9.dll','candidate-diagnostic.ini','baseline-render.ini');Material=@('d3d9.dll','material-experiment.ini','baseline-render.ini');Coverage=@('d3d9.dll','material-experiment.ini','coverage-render.ini')};
 headset='NOT RUN';combat='BOUNDED RESTAURANT EMULATOR ONLY; outdoor/firing NOT VERIFIED';installed=$false;default_mode='Combined'}|ConvertTo-Json -Depth 8|Set-Content (Join-Path $out 'candidate.json')
& (Join-Path $out 'Install.ps1') -VerifyOnly
Write-Output "PREPARED $out; Steam unchanged"
