# MAIN: freeze only the verified DLL and owned language helper; never live caches.
param([Parameter(Mandatory=$true)][string]$CandidateDll,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Choose a fresh package directory'}
if(git -C $root status --porcelain --untracked-files=no){throw 'Commit source before freeze'}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close game first'}
$targets=@('d3d9.dll','VRSettings\Apply-Languages.ps1')
$expected=@('642704E5B885B53078C918B70D833C1B87655C0C3A9139F07F30700E60D84D4C',
 '5937C0B7405C2ADA36FD79261560AEC62CA994B9400474B8EF39BF92BB531D56')
for($i=0;$i -lt 2;$i++){if((Hash (Join-Path $game $targets[$i])) -ne $expected[$i]){throw 'Inspected baseline differs'}}
& (Join-Path $PSScriptRoot 'assert-candidate-profile.ps1') -CandidateDll $CandidateDll `
 -RenderIni @((Join-Path $game 'Darkness2VR.ini')) -DiagnosticIni (Join-Path $game 'Darkness2VR-diagnostic.ini') | Out-Host
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$out=(Resolve-Path -LiteralPath $OutputDirectory).Path
$payload=@($CandidateDll,(Join-Path $PSScriptRoot 'Apply-Languages.ps1'));$files=@()
for($i=0;$i -lt 2;$i++){
 Copy-Item -LiteralPath (Join-Path $game $targets[$i]) -Destination (Join-Path $out "baseline-$i.bin")
 Copy-Item -LiteralPath $payload[$i] -Destination (Join-Path $out "payload-$i.bin")
 $files+=@{relative=$targets[$i];baseline="baseline-$i.bin";payload="payload-$i.bin";before=$expected[$i];after=(Hash $payload[$i])}
}
$installer=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'install-menu-polish.ps1'))
$installer=$installer.Replace('if($m.files.Count -ne $allowed.Count)',
 'if($m.kind -eq ''vr-language-fix''){$allowed=@(''d3d9.dll'',''VRSettings\Apply-Languages.ps1'')}' + "`r`n" + 'if($m.files.Count -ne $allowed.Count)')
[IO.File]::WriteAllText((Join-Path $out 'Install.ps1'),$installer)
$protected=@()
foreach($relative in @('DarknessII.exe','openxr_loader.dll','Darkness2VR.ini','Darkness2VR-diagnostic.ini',
 'Cache.Windows\H.Misc.cache','Cache.Windows\H.Misc.toc','VRSettings\languages.json',
 'Cache.DLC\PCPatchManifests\Cache.Windows\H.Script.cache','Cache.DLC\PCPatchManifests\Cache.Windows\H.Script.toc')){
 $target=Join-Path $game $relative;$protected+=@{target=$target;sha256=(Hash $target)}
}
@{kind='vr-language-fix';source_commit=(git -C $root rev-parse HEAD).Trim();
 headset='NOT RUN';runtime='NOT VERIFIED; language source only; ladder UNRESOLVED';protected=$protected;files=$files} |
 ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $out 'candidate.json') -Encoding UTF8
& (Join-Path $out 'Install.ps1') -VerifyOnly
Write-Output "PREPARED $out"
