# Offline transaction fixture. Only the fixed game-directory literal is replaced
# in a private copy of the production installer. No installed file is accessed.
$ErrorActionPreference='Stop'
$out=Join-Path $env:TEMP ('d2vr-perf-package-test-'+[guid]::NewGuid().ToString('N'))
$game=Join-Path $out 'fake-game';$package=Join-Path $out 'package'
New-Item -ItemType Directory -Path $game,$package | Out-Null
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
function Check([bool]$ok,[string]$message){if(!$ok){throw $message}}
$global:D2vrPackageFixtureBusy=$false
function Get-CimInstance{param($ClassName,$Filter)if($global:D2vrPackageFixtureBusy){[pscustomobject]@{Name='DarknessII.exe'}}}
$source=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Install.ps1') -Raw
$literal='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
Check ($source.Contains($literal)) 'Production target literal missing'
[IO.File]::WriteAllText((Join-Path $package 'Install.ps1'),$source.Replace($literal,$game))
Set-Content -LiteralPath (Join-Path $game 'd3d9.dll') -Value 'baseline DLL'
Set-Content -LiteralPath (Join-Path $game 'Darkness2VR.ini') -Value "[Render]`r`nSourceRenderWidth=2676`r`nSourceRenderHeight=2728"
Set-Content -LiteralPath (Join-Path $game 'protected.exe') -Value 'protected'
Copy-Item -LiteralPath (Join-Path $game 'd3d9.dll') -Destination (Join-Path $package 'baseline.dll')
Copy-Item -LiteralPath (Join-Path $game 'Darkness2VR.ini') -Destination (Join-Path $package 'baseline.ini')
Set-Content -LiteralPath (Join-Path $package 'd3d9.dll') -Value 'candidate DLL'
$profiles=@{Both='candidate.ini';Cache='cache.ini';Worker='worker.ini';Off='off.ini'}
foreach($mode in $profiles.Keys){Set-Content -LiteralPath (Join-Path $package $profiles[$mode]) -Value "fixture profile $mode"}
$files=@('Install.ps1','d3d9.dll')+@($profiles.Values)
$m=@{source_commit='fixture';profiles=$profiles;files=@($files|ForEach-Object {@{name=$_;sha256=(Hash (Join-Path $package $_))}});before=@(@{target=(Join-Path $game 'd3d9.dll');sha256=(Hash (Join-Path $game 'd3d9.dll'))},@{target=(Join-Path $game 'Darkness2VR.ini');sha256=(Hash (Join-Path $game 'Darkness2VR.ini'))});protected=@(@{target=(Join-Path $game 'protected.exe');sha256=(Hash (Join-Path $game 'protected.exe'))})}
$m|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $package 'candidate.json')
$install=Join-Path $package 'Install.ps1'
& $install -VerifyOnly
foreach($mode in @('Both','Cache','Worker','Off')){
 & $install -Mode $mode
 Check ((Hash (Join-Path $game 'Darkness2VR.ini')) -eq (Hash (Join-Path $package $profiles[$mode]))) 'Wrong A/B profile'
}
& $install -Restore
foreach($e in $m.before){Check ((Hash $e.target) -eq $e.sha256) 'Restore differs'}
$global:D2vrPackageFixtureBusy=$true;$refused=$false
try{& $install}catch{$refused=$_.Exception.Message -like '*Close Darkness II*'}
Check $refused 'Active-game gate bypassed';$global:D2vrPackageFixtureBusy=$false
Add-Content -LiteralPath (Join-Path $package 'd3d9.dll') -Value 'corrupt'
$refused=$false;try{& $install}catch{$refused=$_.Exception.Message -like '*hash mismatch*'}
Check $refused 'Corrupt candidate accepted'
foreach($e in $m.before){Check ((Hash $e.target) -eq $e.sha256) 'Rejected installation changed preimage'}
Write-Output 'PASS offline production-installer fixture: four A/B modes, exact rollback, busy-game rejection, corrupt-payload rejection; real game untouched'
