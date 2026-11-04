# Exercise the actual installer body against an isolated fake installation.
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$work=Join-Path $root ('test-output/combined-installer-'+[guid]::NewGuid().ToString('N'))
$game=Join-Path $work 'game';$package=Join-Path $work 'package'
New-Item -ItemType Directory -Path $game,$package|Out-Null
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
function WriteFixture([string]$p,[string]$text){[IO.File]::WriteAllText($p,$text,[Text.Encoding]::ASCII)}
function Check([bool]$value,[string]$message){if(!$value){throw $message}}
$source=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'Install.ps1'))
$literal="C:\Program Files (x86)\Steam\steamapps\common\Darkness II"
Check ($source.Split(@($literal),[StringSplitOptions]::None).Count -eq 2) 'Expected exactly one fixed game literal'
WriteFixture (Join-Path $package 'Install.ps1') ($source.Replace($literal,$game))
foreach($pair in @(@('baseline.dll','accepted-dll'),@('baseline-diagnostic.ini','accepted-diag'),
 @('d3d9.dll','candidate-dll'),@('candidate-diagnostic.ini','collectors0-simulator0-shadow0'),
 @('shadow-experiment.ini','collectors0-simulator0-shadow1'),
 @('reflection-experiment.ini','collectors0-simulator0-shadow0-reflection1'),
 @('smaa-experiment.ini','collectors0-simulator0-shadow0-reflection0-smaa1'),
 @('material-experiment.ini','collectors0-simulator0-shadow0-reflection0-smaa0-material1'))){WriteFixture (Join-Path $package $pair[0]) $pair[1]}
Copy-Item (Join-Path $package 'baseline.dll') (Join-Path $game 'd3d9.dll')
Copy-Item (Join-Path $package 'baseline-diagnostic.ini') (Join-Path $game 'Darkness2VR-diagnostic.ini')
$before=@(foreach($name in 'd3d9.dll','Darkness2VR-diagnostic.ini'){@{target=(Join-Path $game $name);sha256=(Hash (Join-Path $game $name))}})
$protected=@(foreach($name in 'DarknessII.exe','Darkness2VR.ini','openxr_loader.dll'){
 $p=Join-Path $game $name;WriteFixture $p ('protected-'+$name);@{target=$p;sha256=(Hash $p)}
})
$files=@(Get-ChildItem $package -File|ForEach-Object {@{name=$_.Name;sha256=(Hash $_.FullName)}})
@{source_commit='fixture';before=$before;protected=$protected;files=$files}|ConvertTo-Json -Depth 6|Set-Content (Join-Path $package 'candidate.json')
$installer=Join-Path $package 'Install.ps1'
& $installer -VerifyOnly
Check ((Hash $before[0].target) -eq $before[0].sha256) 'VerifyOnly mutated DLL'
& $installer
Check ((Hash $before[0].target) -eq (Hash (Join-Path $package 'd3d9.dll'))) 'Candidate DLL not installed'
Check ((Hash $before[1].target) -eq (Hash (Join-Path $package 'candidate-diagnostic.ini'))) 'Combined profile not installed'
& $installer -Mode ShadowExperiment
Check ((Hash $before[1].target) -eq (Hash (Join-Path $package 'shadow-experiment.ini'))) 'Shadow profile not installed'
& $installer -Mode ReflectionExperiment
Check ((Hash $before[1].target) -eq (Hash (Join-Path $package 'reflection-experiment.ini'))) 'Reflection profile not installed'
& $installer -Mode SmaaExperiment
Check ((Hash $before[1].target) -eq (Hash (Join-Path $package 'smaa-experiment.ini'))) 'SMAA profile not installed'
& $installer -Mode MaterialExperiment
Check ((Hash $before[1].target) -eq (Hash (Join-Path $package 'material-experiment.ini'))) 'Material profile not installed'
& $installer -Mode Combined
Check ((Hash $before[1].target) -eq (Hash (Join-Path $package 'candidate-diagnostic.ini'))) 'Combined did not disable experiment'
& $installer -Restore
foreach($e in $before){Check ((Hash $e.target) -eq $e.sha256) 'Restore not exact'}
$frozen=Join-Path $package 'd3d9.dll';$original=[IO.File]::ReadAllBytes($frozen)
WriteFixture $frozen 'tampered';$rejected=$false
try{& $installer}catch{$rejected=$true}
Check $rejected 'Tampered package accepted'
foreach($e in $before){Check ((Hash $e.target) -eq $e.sha256) 'Tampered package wrote targets'}
[IO.File]::WriteAllBytes($frozen,$original)
$protectedTarget=$protected[1].target;$original=[IO.File]::ReadAllBytes($protectedTarget)
WriteFixture $protectedTarget 'changed';$rejected=$false
try{& $installer}catch{$rejected=$true}
Check $rejected 'Changed protected installation accepted'
foreach($e in $before){Check ((Hash $e.target) -eq $e.sha256) 'Protected rejection wrote targets'}
[IO.File]::WriteAllBytes($protectedTarget,$original)
WriteFixture $before[0].target 'foreign';$rejected=$false
try{& $installer}catch{$rejected=$true}
Check $rejected 'Foreign preimage accepted'
Check ([IO.File]::ReadAllText($before[0].target) -eq 'foreign') 'Foreign preimage overwritten'
Write-Output "PASS actual installer: verify-only, five modes, return to Combined, exact rollback, frozen tamper, protected tamper, foreign preimage; private fixture $work"
