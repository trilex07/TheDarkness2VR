# Deploy only after offline acceptance; never starts a game or alters SteamVR.
param(
    [Parameter(Mandatory=$true)][string]$CandidateDll,
    [Parameter(Mandatory=$true)][string]$BridgeExe,
    [Parameter(Mandatory=$true)][string]$ToolkitLoader,
    [Parameter(Mandatory=$true)][string]$SourceCommit,
    [string]$GameDir='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
)
$ErrorActionPreference='Stop'
function Assert-Free {
    $busy=@(Get-Process | Where-Object {$_.ProcessName -eq 'DarknessII' -or
        ($_.Path -like '*\steamapps\common\*' -and $_.Path -notlike '*\SteamVR\*' -and
         $_.Path -notlike '*\fpsVR\*' -and $_.Path -notlike '*\PlayStation VR2 App\*')})
    if($busy){throw ('Steam/game busy: '+(($busy | ForEach-Object {"$($_.ProcessName) PID $($_.Id)"}) -join ', '))}
}
function Hash([string]$path){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
function IniContract([string]$path) {
    $section='';$lines=@()
    foreach($line in (Get-Content -LiteralPath $path)) {
        $text=$line.Trim()
        if(!$text -or $text.StartsWith(';') -or $text.StartsWith('#')){continue}
        if($text -match '^\[(.+)\]$'){$section=$Matches[1];continue}
        if($section -ieq 'Input' -and $text -match '^AdaptiveTriggers\s*='){continue}
        $lines+="$section|$text"
    }
    $lines -join "`n"
}
function Assert-PE([string]$path,[int]$machine) {
    $bytes=[IO.File]::ReadAllBytes($path)
    if($bytes.Length -lt 256 -or [BitConverter]::ToUInt16($bytes,0) -ne 0x5A4D){throw "Not a PE: $path"}
    $pe=[BitConverter]::ToInt32($bytes,0x3c)
    if($pe -lt 0 -or $pe+6 -gt $bytes.Length -or [BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550 -or
        [BitConverter]::ToUInt16($bytes,$pe+4) -ne $machine){throw "Wrong PE architecture: $path"}
    if([Text.Encoding]::ASCII.GetString($bytes).Contains('fake_psvr2_')){throw "Test fake refused: $path"}
}
Assert-Free
if($SourceCommit -notmatch '^[0-9a-f]{40}$'){throw 'Require full committed source identity'}
if((Hash (Join-Path $GameDir 'DarknessII.exe')) -ne 'D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'){throw 'Native EXE identity mismatch'}
Assert-PE $CandidateDll 0x14c
Assert-PE $BridgeExe 0x8664
Assert-PE $ToolkitLoader 0x8664
$root=(Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$runtime=Join-Path $env:LOCALAPPDATA 'Darkness2VR\runtime\psvr2'
$backup=Join-Path $env:LOCALAPPDATA ('Darkness2VR\backups\psvr2-haptics-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$ini=@((Join-Path $GameDir 'Darkness2VR.ini'),(Join-Path $GameDir 'Darkness2VR-diagnostic.ini'),
    (Join-Path $env:LOCALAPPDATA 'Darkness2VR\Darkness2VR.ini'),(Join-Path $env:LOCALAPPDATA 'Darkness2VR\last-lighting.ini'))
foreach($path in $ini){if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing expected preserved INI: $path"}}
$iniBefore=@{};foreach($path in $ini){$iniBefore[$path]=IniContract $path}
$copies=@(
    @{Source=$CandidateDll;Target=(Join-Path $GameDir 'd3d9.dll')},
    @{Source=$BridgeExe;Target=(Join-Path $runtime 'Darkness2Psvr2Bridge.exe')},
    @{Source=$ToolkitLoader;Target=(Join-Path $runtime 'psvr2_toolkit_capi_loader.dll')},
    @{Source=(Join-Path $root 'THIRD-PARTY-PSVR2.md');Target=(Join-Path $runtime 'THIRD-PARTY-PSVR2.md')})
New-Item -ItemType Directory -Path $backup | Out-Null
$before=@();$index=0
foreach($path in @($copies | ForEach-Object {$_.Target})+$ini){
    $exists=Test-Path -LiteralPath $path -PathType Leaf
    $saved=Join-Path $backup ('{0:D2}-{1}' -f $index,(Split-Path $path -Leaf));++$index
    $sha=$null
    if($exists){$sha=Hash $path;Copy-Item -LiteralPath $path -Destination $saved; if((Hash $saved) -ne $sha){throw 'Backup hash mismatch'}}
    $before+=@{Path=$path;Existed=$exists;Backup=$saved;SHA256=$sha}
}
$manifest=@{SourceCommit=$SourceCommit;BuiltDllSHA256=(Hash $CandidateDll);Before=$before;GameDir=$GameDir;DirectRecordCopy=0;AdaptiveTriggers=1}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $backup 'manifest.json') -Encoding UTF8
$rollback=@'
$ErrorActionPreference='Stop'
if(Get-Process -Name DarknessII -ErrorAction SilentlyContinue){throw 'Close Darkness II before rollback'}
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw | ConvertFrom-Json
foreach($entry in $manifest.Before){
    if($entry.Existed -and (Get-FileHash -LiteralPath $entry.Backup -Algorithm SHA256).Hash -ne $entry.SHA256){throw "Backup corrupt: $($entry.Backup)"}
}
foreach($entry in $manifest.Before){
    if($entry.Existed){Copy-Item -LiteralPath $entry.Backup -Destination $entry.Path -Force}
    elseif(Test-Path -LiteralPath $entry.Path -PathType Leaf){Remove-Item -LiteralPath $entry.Path -Force}
}
foreach($entry in $manifest.Before){
    if($entry.Existed -and (Get-FileHash -LiteralPath $entry.Path -Algorithm SHA256).Hash -ne $entry.SHA256){throw "Restore mismatch: $($entry.Path)"}
    if(!$entry.Existed -and (Test-Path -LiteralPath $entry.Path)){throw "New file remains: $($entry.Path)"}
}
'RESTORED_ALL_HASHES_MATCH'
'@
[IO.File]::WriteAllText((Join-Path $backup 'rollback.ps1'),$rollback,[Text.Encoding]::UTF8)
Add-Type @'
using System.Runtime.InteropServices;
using System.Text;
public static class Psvr2Ini {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool WritePrivateProfileString(string s,string k,string v,string f);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] public static extern uint GetPrivateProfileInt(string s,string k,int d,string f);
}
'@
if([Psvr2Ini]::GetPrivateProfileInt('Render','DirectRecordCopy',0,$ini[1]) -ne 0){throw 'DirectRecordCopy must remain OFF'}
Assert-Free
# Refuse a stale preimage rather than overwriting another agent's deployment.
foreach($entry in $before){
    if($entry.Existed -and (Hash $entry.Path) -ne $entry.SHA256){throw "Target changed since backup: $($entry.Path)"}
    if(!$entry.Existed -and (Test-Path -LiteralPath $entry.Path)){throw "New target appeared since backup: $($entry.Path)"}
}
try {
    New-Item -ItemType Directory -Force -Path $runtime | Out-Null
    foreach($item in $copies){Copy-Item -LiteralPath $item.Source -Destination $item.Target -Force;if((Hash $item.Source) -ne (Hash $item.Target)){throw 'Deployment hash mismatch'}}
    foreach($path in $ini){
        if(![Psvr2Ini]::WritePrivateProfileString('Input','AdaptiveTriggers','1',$path)){throw "INI update failed: $path"}
        if([Psvr2Ini]::GetPrivateProfileInt('Input','AdaptiveTriggers',0,$path) -ne 1){throw 'Haptics not enabled'}
        if((IniContract $path) -cne $iniBefore[$path]){throw "Unrelated INI setting changed: $path"}
    }
    if([Psvr2Ini]::GetPrivateProfileInt('Render','DirectRecordCopy',0,$ini[1]) -ne 0){throw 'DirectRecordCopy changed'}
    if((Hash (Join-Path $GameDir 'DarknessII.exe')) -ne 'D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'){throw 'EXE changed'}
    $after=@(@($copies | ForEach-Object {$_.Target})+$ini | ForEach-Object {@{Path=$_;SHA256=(Hash $_)}})
    @{SourceCommit=$SourceCommit;Files=$after;Backup=$backup;ManualHeadsetValidation='NOT RUN'} | ConvertTo-Json -Depth 6 |
        Set-Content -LiteralPath (Join-Path $backup 'deployed.json') -Encoding UTF8
} catch {
    & (Join-Path $backup 'rollback.ps1')
    throw
}
[pscustomobject]@{Commit=$SourceCommit;DllSHA256=(Hash (Join-Path $GameDir 'd3d9.dll'));AdaptiveTriggers=1;DirectRecordCopy=0;Backup=$backup;Ready='manual PSVR2 test; game not launched'} | Format-List
