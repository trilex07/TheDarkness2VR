# Freeze a DLL-only candidate against the user-accepted ammo baseline.
param([string]$OutputDirectory='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build/lit-mask-projection-20261105'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Choose a fresh package directory'}
if(git -C $root status --porcelain --untracked-files=no){throw 'Commit source before freeze'}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close game first'}
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$before=Hash (Join-Path $game 'd3d9.dll')
if($before -ne 'E8C8015E58045B6F8EA2FE744E142540FEB478608F40D3FB508B8DB4B484C72B'){throw 'Unexpected accepted ammo DLL'}
& (Join-Path $PSScriptRoot 'assert-candidate-profile.ps1') -CandidateDll (Join-Path $root 'd3d9.dll') -RenderIni @((Join-Path $game 'Darkness2VR.ini')) -DiagnosticIni (Join-Path $game 'Darkness2VR-diagnostic.ini') | Out-Host
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$out=(Resolve-Path -LiteralPath $OutputDirectory).Path
Copy-Item -LiteralPath (Join-Path $game 'd3d9.dll') -Destination (Join-Path $out 'baseline-0.bin')
Copy-Item -LiteralPath (Join-Path $root 'd3d9.dll') -Destination (Join-Path $out 'payload-0.bin')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'install-menu-polish.ps1') -Destination (Join-Path $out 'Install.ps1')
$protected=@()
foreach($relative in @('DarknessII.exe','openxr_loader.dll','Darkness2VR.ini','Darkness2VR-diagnostic.ini',
 'Cache.Windows\H.Misc.cache','Cache.Windows\H.Misc.toc',
 'Cache.DLC\PCPatchManifests\Cache.Windows\H.Script.cache','Cache.DLC\PCPatchManifests\Cache.Windows\H.Script.toc')){
 $target=Join-Path $game $relative
 $protected+=@{target=$target;sha256=(Hash $target)}
}
@{kind='lit-mask-projection';source_commit=(git -C $root rev-parse HEAD).Trim();headset='NOT RUN';
 build_flavor='protected-native x86 diagnostic capabilities; recording OFF';
 simulator='PASS effective mask/raster alignment both eyes, rotated poses; contributing crane draw observed; visual headset acceptance pending';
 protected=$protected;files=@(@{relative='d3d9.dll';baseline='baseline-0.bin';payload='payload-0.bin';before=$before;after=(Hash (Join-Path $out 'payload-0.bin'))})} |
 ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $out 'candidate.json') -Encoding UTF8
& (Join-Path $out 'Install.ps1') -VerifyOnly
Write-Output "PREPARED $out; Steam unchanged"
