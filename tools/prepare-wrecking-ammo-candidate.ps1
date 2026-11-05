# Freeze the tested source as a DLL-only candidate. Does not deploy or launch.
param([string]$OutputDirectory='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$OutputDirectory){$OutputDirectory=Join-Path $root 'build/wrecking-light-ammo-reserve-20261105'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Keep frozen packages; choose a fresh output'}
if(git -C $root status --porcelain --untracked-files=no){throw 'Commit source before freeze'}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close game first'}
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$before=Hash (Join-Path $game 'd3d9.dll')
if($before -ne '7DD55BA4CF6E05E8FE460CBF0529FE212E68D4072D12C3D5FE5F93DE1CFE81A1'){throw 'Unexpected accepted DLL'}
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
@{kind='wrecking-light-ammo-reserve';source_commit=(git -C $root rev-parse HEAD).Trim();headset='NOT RUN';
 build_flavor='protected-native x86, diagnostic capabilities compiled; recording OFF in protected installed profile';
 simulator='PASS diagnostic build: exact material eye consistency both eyes and head poses; native ammo fire/reload';
 protected=$protected;files=@(@{relative='d3d9.dll';baseline='baseline-0.bin';payload='payload-0.bin';before=$before;after=(Hash (Join-Path $out 'payload-0.bin'))})} |
 ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $out 'candidate.json') -Encoding UTF8
& (Join-Path $out 'Install.ps1') -VerifyOnly
Write-Output "PREPARED $out; Steam unchanged"
