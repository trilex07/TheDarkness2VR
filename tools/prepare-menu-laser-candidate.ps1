# MAIN: DLL-only controller-menu-laser candidate with exact rollback and protected inputs.
param([Parameter(Mandatory=$true)][string]$CandidateDll,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Choose a fresh package directory'}
if(git -C $root status --porcelain --untracked-files=no){throw 'Commit source before freeze'}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close game first'}
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$before=Hash (Join-Path $game 'd3d9.dll')
if($before -ne 'F11472BE86A02DBB166F4A3E0D9F05A958B3C5DA332FEA9F74454CA3737405DF'){
 throw 'Installed baseline differs from inspected heart-pull candidate'
}
& (Join-Path $PSScriptRoot 'assert-candidate-profile.ps1') -CandidateDll $CandidateDll `
 -RenderIni @((Join-Path $game 'Darkness2VR.ini')) -DiagnosticIni (Join-Path $game 'Darkness2VR-diagnostic.ini') | Out-Host
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$out=(Resolve-Path -LiteralPath $OutputDirectory).Path
Copy-Item -LiteralPath (Join-Path $game 'd3d9.dll') -Destination (Join-Path $out 'baseline-0.bin')
Copy-Item -LiteralPath $CandidateDll -Destination (Join-Path $out 'payload-0.bin')
$installer=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'install-menu-polish.ps1'))
$installer=$installer.Replace("'tentacle-gaze-stability'","'tentacle-gaze-stability','controller-menu-laser'")
[IO.File]::WriteAllText((Join-Path $out 'Install.ps1'),$installer)
$protected=@()
foreach($relative in @('DarknessII.exe','openxr_loader.dll','Darkness2VR.ini','Darkness2VR-diagnostic.ini',
 'VRSettings\Apply-Languages.ps1','VRSettings\languages.json',
 'Cache.Windows\H.Misc.cache','Cache.Windows\H.Misc.toc',
 'Cache.DLC\PCPatchManifests\Cache.Windows\H.Script.cache','Cache.DLC\PCPatchManifests\Cache.Windows\H.Script.toc')){
 $target=Join-Path $game $relative;$protected+=@{target=$target;sha256=(Hash $target)}
}
@{kind='controller-menu-laser';source_commit=(git -C $root rev-parse HEAD).Trim();
 headset='NOT RUN';runtime='NOT VERIFIED; offline candidate';protected=$protected;
 files=@(@{relative='d3d9.dll';baseline='baseline-0.bin';payload='payload-0.bin';before=$before;after=(Hash $CandidateDll)})} |
 ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $out 'candidate.json') -Encoding UTF8
& (Join-Path $out 'Install.ps1') -VerifyOnly
Write-Output "PREPARED $out"
