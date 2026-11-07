# Freeze only the tested gaze DLL; preserve all installed profiles/assets.
param([string]$OutputDirectory='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build/tentacle-gaze-steam-20261107'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Choose a fresh package directory'}
if(git -C $root status --porcelain --untracked-files=no){throw 'Commit source before freeze'}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close game first'}
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$before=Hash (Join-Path $game 'd3d9.dll')
if($before -ne 'A9D487B1C5D3B73930A9514C28C9B2CC643D35C1E6A997BBFA65C077E462CE72'){throw 'Unexpected installed baseline'}
$candidate=Join-Path $root 'd3d9.dll'
$tested='D72F8E1CBED5CE41D7F90B5775C6CC89A55F39CA0F69933D3A01021C4CA498EE'
if((Hash $candidate) -ne $tested){throw 'Candidate differs from simulator-tested gaze build'}
& (Join-Path $PSScriptRoot 'assert-candidate-profile.ps1') -CandidateDll $candidate -RenderIni @((Join-Path $game 'Darkness2VR.ini')) -DiagnosticIni (Join-Path $game 'Darkness2VR-diagnostic.ini') | Out-Host
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$out=(Resolve-Path -LiteralPath $OutputDirectory).Path
Copy-Item -LiteralPath (Join-Path $game 'd3d9.dll') -Destination (Join-Path $out 'baseline-0.bin')
Copy-Item -LiteralPath $candidate -Destination (Join-Path $out 'payload-0.bin')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'install-menu-polish.ps1') -Destination (Join-Path $out 'Install.ps1')
$protected=@()
foreach($relative in @('DarknessII.exe','openxr_loader.dll','Darkness2VR.ini','Darkness2VR-diagnostic.ini',
 'Cache.Windows\H.Misc.cache','Cache.Windows\H.Misc.toc',
 'Cache.DLC\PCPatchManifests\Cache.Windows\H.Script.cache','Cache.DLC\PCPatchManifests\Cache.Windows\H.Script.toc')){
 $target=Join-Path $game $relative;$protected+=@{target=$target;sha256=(Hash $target)}
}
@{kind='tentacle-gaze-stability';source_commit=(git -C $root rev-parse HEAD).Trim();headset='NOT RUN';
 simulator='PASS synthetic blink, spike, fixation changes and acquisition consumer; actual held-object flight NOT VERIFIED';
 protected=$protected;files=@(@{relative='d3d9.dll';baseline='baseline-0.bin';payload='payload-0.bin';before=$before;after=$tested})} |
 ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $out 'candidate.json') -Encoding UTF8
& (Join-Path $out 'Install.ps1') -VerifyOnly
Write-Output "PREPARED $out"
