# MAIN only. Exact accepted baseline, closed game, DLL + game INI only.
param([switch]$Install,[string]$Restore,[string]$PreviousPackage)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$local='C:\Users\Trilex\AppData\Local\Darkness2VR'
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
function Closed {if(Get-CimInstance Win32_Process -Filter "Name='DarknessII.exe'"){throw 'Close Darkness II first'}}
Closed
if($Restore) {
 $m=Get-Content -LiteralPath (Join-Path $Restore 'deployment.json') -Raw | ConvertFrom-Json
 foreach($e in $m.after){if((Hash $e.target) -ne $e.sha256){throw 'Current installation differs from candidate; refusing overwrite'}}
 foreach($e in $m.before){if((Hash $e.backup) -ne $e.sha256){throw 'Backup hash mismatch'}}
 foreach($e in $m.before){Copy-Item -LiteralPath $e.backup -Destination $e.target -Force;if((Hash $e.target) -ne $e.sha256){throw 'Restore mismatch'}}
 Write-Output 'RESTORED accepted DLL and game INI; game not launched';return
}
if(!$Install){throw 'Use -Install or -Restore <package>'}
$dll=Join-Path $game 'd3d9.dll';$ini=Join-Path $game 'Darkness2VR.ini'
$diag=Join-Path $game 'Darkness2VR-diagnostic.ini'
$candidate=Join-Path $root 'd3d9.dll'
if($PreviousPackage) {
 $previous=Get-Content -LiteralPath (Join-Path $PreviousPackage 'deployment.json') -Raw | ConvertFrom-Json
 if(!$previous.after -or $previous.after.Count -ne 2 -or $previous.after[0].target -ne $dll -or $previous.after[1].target -ne $ini){throw 'Previous package target set differs'}
 foreach($e in @($previous.after)+@($previous.protected)){if((Hash $e.target) -ne $e.sha256){throw 'Previous installed candidate differs'}}
} elseif((Hash $dll) -ne '9AF9A51E7E517621C1C667E192D17B158C2F4DBCE1256F98AA9DEFD79C441A5D'){throw 'Accepted DLL baseline differs'}
if((Hash (Join-Path $game 'DarknessII.exe')) -ne 'D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'){throw 'EXE differs'}
& (Join-Path $PSScriptRoot 'assert-candidate-profile.ps1') -CandidateDll $candidate -RenderIni @($ini) -DiagnosticIni $diag | Out-Host
$source=(git -C $root rev-parse HEAD).Trim()
if(git -C $root status --porcelain --untracked-files=no){throw 'Commit tracked source before installation'}
$package=Join-Path $local ('packages/cinematic-theater-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
New-Item -ItemType Directory -Path $package | Out-Null
$before=@();$n=0
foreach($path in @($dll,$ini)) {
 $backup=Join-Path $package ('{0}-{1}' -f $n,(Split-Path -Leaf $path));$n++
 $sha=Hash $path;Copy-Item -LiteralPath $path -Destination $backup
 if((Hash $backup) -ne $sha){throw 'Backup mismatch'}
 $before+=@{target=$path;backup=$backup;sha256=$sha}
}
$protected=@()
foreach($p in @((Join-Path $game 'DarknessII.exe'),$diag,(Join-Path $game 'openxr_loader.dll'),(Join-Path $local 'Darkness2VR.ini'))) {
 $protected+=@{target=$p;sha256=(Hash $p)}
}
Copy-Item -LiteralPath $candidate -Destination (Join-Path $package 'candidate.dll')
$receipt=@{source_commit=$source;dll_sha256=(Hash $candidate);before=$before;protected=$protected;headset='NOT RUN';game_launched=$false}
$receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $package 'deployment.json') -Encoding UTF8
if(!('D2vrCinemaIni' -as [type])){Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;
public static class D2vrCinemaIni {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
 public static extern bool WritePrivateProfileString(string section,string key,string value,string path);
}
"@}
Closed
foreach($e in $before){if((Hash $e.target) -ne $e.sha256){throw 'Preimage changed'}}
try {
 Copy-Item -LiteralPath (Join-Path $package 'candidate.dll') -Destination $dll -Force
 if(![D2vrCinemaIni]::WritePrivateProfileString('Render','CinematicTheater','1',$ini)){throw 'INI write failed'}
 if((Hash $dll) -ne $receipt.dll_sha256 -or [D2vrProfileGate]::GetPrivateProfileInt('Render','CinematicTheater',0,$ini) -ne 1){throw 'Install mismatch'}
 foreach($e in $protected){if((Hash $e.target) -ne $e.sha256){throw 'Protected file changed'}}
 $receipt.after=@(@{target=$dll;sha256=(Hash $dll)},@{target=$ini;sha256=(Hash $ini)})
 $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $package 'deployment.json') -Encoding UTF8
} catch {
 foreach($e in $before){Copy-Item -LiteralPath $e.backup -Destination $e.target -Force}
 throw
}
Write-Output "INSTALLED $package DLL=$($receipt.dll_sha256); CinematicTheater=1; game not launched"
Write-Output "ROLLBACK: powershell -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Restore `"$package`""
