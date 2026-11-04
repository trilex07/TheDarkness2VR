# Exercise the actual three-target installer against private disposable files.
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$work=Join-Path $root ('test-output/quality-installer-'+[guid]::NewGuid().ToString('N'))
$game=Join-Path $work 'game';$package=Join-Path $work 'package'
New-Item -ItemType Directory $game,$package|Out-Null
function Hash([string]$p){(Get-FileHash $p -Algorithm SHA256).Hash}
function WriteFixture([string]$p,[string]$text){[IO.File]::WriteAllText($p,$text,[Text.Encoding]::ASCII)}
function Check([bool]$condition,[string]$why){if(!$condition){throw $why}}
$source=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'Install.ps1'))
$literal='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
Check ($source.Split(@($literal),[StringSplitOptions]::None).Count -eq 2) 'Expected one fixed game directory'
WriteFixture (Join-Path $package 'Install.ps1') ($source.Replace($literal,$game))
foreach($name in 'baseline.dll','baseline-diagnostic.ini','baseline-render.ini','d3d9.dll','candidate-diagnostic.ini','material-experiment.ini','coverage-render.ini'){WriteFixture (Join-Path $package $name) $name}
$names=@('d3d9.dll','Darkness2VR-diagnostic.ini','Darkness2VR.ini')
$backups=@('baseline.dll','baseline-diagnostic.ini','baseline-render.ini')
$before=@(for($i=0;$i -lt 3;$i++){
 $target=Join-Path $game $names[$i];Copy-Item (Join-Path $package $backups[$i]) $target
 @{target=$target;sha256=(Hash $target)}
})
$protected=@(foreach($name in 'DarknessII.exe','openxr_loader.dll'){$p=Join-Path $game $name;WriteFixture $p $name;@{target=$p;sha256=(Hash $p)}})
$profiles=@{Combined=@('d3d9.dll','candidate-diagnostic.ini','baseline-render.ini');Material=@('d3d9.dll','material-experiment.ini','baseline-render.ini');Coverage=@('d3d9.dll','material-experiment.ini','coverage-render.ini')}
$files=@(Get-ChildItem $package -File|ForEach-Object {@{name=$_.Name;sha256=(Hash $_.FullName)}})
@{source_commit='fixture';before=$before;protected=$protected;files=$files;profiles=$profiles}|ConvertTo-Json -Depth 7|Set-Content (Join-Path $package 'candidate.json')
$installer=Join-Path $package 'Install.ps1'
& $installer -VerifyOnly
foreach($mode in 'Combined','Material','Coverage','Material','Combined'){
 & $installer -Mode $mode
 for($i=0;$i -lt 3;$i++){Check ((Hash $before[$i].target) -eq (Hash (Join-Path $package $profiles[$mode][$i]))) "Wrong target $i in $mode"}
 & $installer -VerifyOnly
}
& $installer -Restore
foreach($f in $before){Check ((Hash $f.target) -eq $f.sha256) 'Restore mismatch'}
foreach($target in @((Join-Path $package 'd3d9.dll'),$protected[0].target,$before[2].target)){
 $original=[IO.File]::ReadAllBytes($target);WriteFixture $target 'foreign';$rejected=$false
 try{& $installer -VerifyOnly}catch{$rejected=$true}
 Check $rejected 'VerifyOnly accepted tamper/foreign render preimage'
 $rejected=$false;try{& $installer -Mode Coverage}catch{$rejected=$true}
 Check $rejected 'Install accepted tamper/foreign render preimage'
 Check ([IO.File]::ReadAllText($target) -eq 'foreign') 'Foreign bytes overwritten'
 [IO.File]::WriteAllBytes($target,$original)
}
& $installer -Mode Coverage
WriteFixture $before[2].target 'foreign-after-coverage';$rejected=$false
try{& $installer -Restore}catch{$rejected=$true}
Check $rejected 'Restore accepted foreign candidate render INI'
Check ([IO.File]::ReadAllText($before[2].target) -eq 'foreign-after-coverage') 'Restore overwrote foreign bytes'
Copy-Item (Join-Path $package 'coverage-render.ini') $before[2].target -Force
& $installer -Restore
foreach($f in $before){Check ((Hash $f.target) -eq $f.sha256) 'Final restore mismatch'}
Write-Output "PASS actual three-target installer: all modes, reverse transitions, verify-only, exact rollback, tamper/foreign preimage rejection; $work"
