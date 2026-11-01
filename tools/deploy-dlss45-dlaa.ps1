param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:/Program Files (x86)/Steam/steamapps/common/Darkness II'
$local='C:/Users/Trilex/AppData/Local/Darkness2VR'
$candidate=Join-Path $root 'd3d9.dll'
$candidateMap=Join-Path $root 'd3d9.map'
$hostSource=Join-Path $root 'build/dlss_host'
$hostDir=Join-Path $game 'd2vr_dlss'
$prior=Join-Path $local 'packages/r2-postload-lod125-20261101T121504038/rollback'
$expectedDll='CC7DD6C7B048158255D607649841BDE6305D284414CAD790A4CE99B14A941248'
$exeHash='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
$files=@('d2vr_dlss_host64.exe','nvngx_dlss.dll','amd_fidelityfx_loader_dx12.dll',
         'amd_fidelityfx_upscaler_dx12.dll','NOTICE.md','NVIDIA-DLSS-LICENSE.txt',
         'AMD-FIDELITYFX-LICENSE.md')
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
$exe=Join-Path $game 'DarknessII.exe'
$target=Join-Path $game 'd3d9.dll'
$gameIni=Join-Path $game 'Darkness2VR.ini'
$diagnosticIni=Join-Path $game 'Darkness2VR-diagnostic.ini'
if((Hash $exe) -ne $exeHash){throw 'Stock EXE mismatch'}
if((Hash $target) -ne $expectedDll){throw 'Installed DLL changed'}
if(!(Test-Path -LiteralPath $candidateMap -PathType Leaf)){throw 'Candidate MAP missing'}
$candidateHash=Hash $candidate
$gate=& (Join-Path $root 'tools/assert-candidate-profile.ps1') -CandidateDll $candidate `
    -RenderIni @($gameIni,(Join-Path $local 'Darkness2VR.ini')) -DiagnosticIni $diagnosticIni
if($gate.Flavor -ne 'protected-native'){throw 'Protected-native candidate required'}
$iniText=[IO.File]::ReadAllText($gameIni)
$diagnostic=[IO.File]::ReadAllText($diagnosticIni)
if($iniText -notmatch '(?im)^NativeViewBoundary\s*=\s*3\s*$' -or
   $iniText -notmatch '(?im)^EngineLODScale\s*=\s*1\.25\s*$' -or
   $iniText -notmatch '(?im)^NisMode\s*=\s*2\s*$' -or
   $diagnostic -notmatch '(?im)^ImagePreservingPresent\s*=\s*1\s*$'){
    throw 'Protected current profile mismatch'
}
$manifest=Get-Content -LiteralPath (Join-Path $prior 'manifest.json') -Raw | ConvertFrom-Json
if(@($manifest.before).Count -ne 9){throw 'Expected nine protected targets'}
foreach($entry in $manifest.before){if(!(Test-Path -LiteralPath $entry.target -PathType Leaf)){throw "Missing protected target: $($entry.target)"}}
foreach($file in $files){if(!(Test-Path -LiteralPath (Join-Path $hostSource $file) -PathType Leaf)){throw "Missing helper file: $file"}}
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II before installation'}
if(!$Install){Write-Output "PREFLIGHT PASS DLL=$candidateHash hostFiles=$($files.Count); no files changed";return}
$package=Join-Path $local ('packages/dlss45-dlaa-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback'
$candidateDir=Join-Path $package 'candidate'
New-Item -ItemType Directory -Force -Path $rollback,$candidateDir,(Join-Path $candidateDir 'd2vr_dlss') | Out-Null
$before=@();$index=0
foreach($entry in $manifest.before){
    $saved=Join-Path $rollback ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $entry.target));$index++
    $sha=Hash $entry.target
    Copy-Item -LiteralPath $entry.target -Destination $saved
    if((Hash $saved) -ne $sha){throw "Backup verification failed: $saved"}
    $before+=@{target=$entry.target;backup=$saved;sha256=$sha}
}
$hostBefore=@()
foreach($file in $files){
    $source=Join-Path $hostSource $file
    $dest=Join-Path $hostDir $file
    $saved=Join-Path $rollback ('host-'+$file)
    $existed=Test-Path -LiteralPath $dest -PathType Leaf
    $sha=if($existed){Hash $dest}else{''}
    if($existed){Copy-Item -LiteralPath $dest -Destination $saved;if((Hash $saved) -ne $sha){throw "Host backup failed: $file"}}
    $installedSha=Hash $source
    Copy-Item -LiteralPath $source -Destination (Join-Path $candidateDir ('d2vr_dlss/'+$file))
    if((Hash (Join-Path $candidateDir ('d2vr_dlss/'+$file))) -ne $installedSha){throw "Candidate copy failed: $file"}
    $hostBefore+=@{target=$dest;backup=$saved;sha256=$sha;existed=$existed;installed_sha256=$installedSha}
}
@{game_dir=$game;exe_sha256=$exeHash;before=$before;host=$hostBefore} | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath (Join-Path $rollback 'manifest.json') -Encoding UTF8
Copy-Item -LiteralPath (Join-Path $root 'tools/restore-dlss45-dlaa.ps1') -Destination (Join-Path $rollback 'Restore.ps1')
Copy-Item -LiteralPath $candidate -Destination (Join-Path $candidateDir 'd3d9.dll')
Copy-Item -LiteralPath $candidateMap -Destination (Join-Path $candidateDir 'd3d9.map')
$sourceCommit=(git -C $root rev-parse HEAD).Trim()
@{dll_sha256=$candidateHash;source_commit=$sourceCommit;profile='DLAA preset K; NIS off; object motion off';
  limitation='No HMD acceptance; synthetic depth and rotation-only camera motion; no CPU/FPS gain claim'} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $candidateDir 'source.json') -Encoding UTF8
if(!('D2vrDlssIni' -as [type])){
    Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;
public static class D2vrDlssIni {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
 public static extern bool WritePrivateProfileString(string section,string key,string value,string path);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
 public static extern uint GetPrivateProfileInt(string section,string key,int fallback,string path);
}
"@
}
try {
    New-Item -ItemType Directory -Force -Path $hostDir | Out-Null
    foreach($file in $files){Copy-Item -LiteralPath (Join-Path $candidateDir ('d2vr_dlss/'+$file)) -Destination (Join-Path $hostDir $file) -Force}
    Copy-Item -LiteralPath $candidate -Destination $target -Force
    foreach($kv in @(@('DlssMode','1'),@('DlssPreset','11'),@('DlssBackend','0'),
                    @('DlssObjectMotion','0'),@('DlssBiasMask','0'),@('NisMode','0'))){
        if(![D2vrDlssIni]::WritePrivateProfileString('Render',$kv[0],$kv[1],$gameIni)){
            throw "INI write failed: $($kv[0])"
        }
    }
    foreach($kv in @(@('DlssMode',1),@('DlssPreset',11),@('DlssBackend',0),
                    @('DlssObjectMotion',0),@('DlssBiasMask',0),@('NisMode',0))){
        if([D2vrDlssIni]::GetPrivateProfileInt('Render',$kv[0],-1,$gameIni) -ne $kv[1]){
            throw "INI verify failed: $($kv[0])"
        }
    }
    if((Hash $target) -ne $candidateHash -or (Hash $exe) -ne $exeHash){throw 'Installed DLL/EXE verify failed'}
    foreach($entry in $hostBefore){if((Hash $entry.target) -ne $entry.installed_sha256){throw "Host install verify failed: $($entry.target)"}}
    foreach($entry in $before){
        if($entry.target -ne $target -and $entry.target -ne $gameIni -and (Hash $entry.target) -ne $entry.sha256){
            throw "Protected target changed: $($entry.target)"
        }
    }
} catch {
    & (Join-Path $rollback 'Restore.ps1')
    throw
}
Write-Output "INSTALLED $package DLL=$candidateHash rollback=$rollback; game not launched"
