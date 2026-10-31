param([switch]$Install)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$game='C:\Program Files (x86)\Steam\steamapps\common\Darkness II'
$local='C:\Users\Trilex\AppData\Local\Darkness2VR'
$exeHash='D46BB6CB762732B72AB9D34428237D2AFA89F5792BDBED957D78793EE8134EC1'
$candidateHash='4B50EAF295135D8BBB06B039D91FA739D114266A189C3572D95D0890DF1693C3'
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}
function Assert-Free {
    $busy=@(Get-CimInstance Win32_Process | Where-Object {$_.Name -eq 'DarknessII.exe' -or
        ($_.ExecutablePath -like '*\steamapps\common\*' -and $_.ExecutablePath -notmatch '\\(SteamVR|fpsVR|PlayStation VR2 App)\\')})
    if($busy){throw ('Game session busy: '+(($busy | ForEach-Object {"$($_.Name) PID=$($_.ProcessId)"}) -join ', '))}
}
function Contract($path) {
    $section='';$values=@()
    foreach($line in Get-Content -LiteralPath $path){
        $v=$line.Trim();if(!$v -or $v.StartsWith(';') -or $v.StartsWith('#')){continue}
        if($v -match '^\[(.+)\]$'){$section=$Matches[1];continue}
        if($section -ieq 'Render' -and $v -match '^AdaptiveFPS\s*='){continue}
        $values+="$section|$v"
    }
    $values -join "`n"
}
Assert-Free
if((Hash "$game\DarknessII.exe") -ne $exeHash){throw 'EXE identity mismatch'}
if((Hash "$game\d3d9.dll") -ne 'BFF8B0A9F5BE83C511762E66C78C5903BD9FB3A48FD3D2F81BF15C035FBD1CFA'){throw 'Installed DLL changed; re-evaluate deployment'}
if((Hash "$root\d3d9.dll") -ne $candidateHash){throw 'Frozen candidate DLL mismatch'}
if((Hash "$game\Darkness2VR.ini") -ne 'C252004214079D174E89D32A1D2F3EF985C109EB0B0FCAC76B103F2A2AF8E692' -or
   (Hash "$local\Darkness2VR.ini") -ne '6083B9E7A3BEE21464A884FD73AD13657A3C89AB2260CCEBDCEE41197E722049'){throw 'Installed profiles changed; re-evaluate deployment'}
& "$PSScriptRoot\assert-candidate-profile.ps1" -CandidateDll "$root\d3d9.dll" -RenderIni @("$game\Darkness2VR.ini","$local\Darkness2VR.ini") -DiagnosticIni "$game\Darkness2VR-diagnostic.ini" | Out-Host
if(!$Install){Write-Output 'PREFLIGHT PASS; no installed files changed';return}
$package=Join-Path $local ('packages\adaptive-half-rate-'+(Get-Date -Format 'yyyyMMddTHHmmssfff'))
$rollback=Join-Path $package 'rollback';$candidate=Join-Path $package 'candidate'
New-Item -ItemType Directory -Path $rollback,$candidate | Out-Null
$old=Get-Content -LiteralPath "$local\packages\four-branch-performance-20261031\rollback\manifest.json" -Raw | ConvertFrom-Json
$before=@();$index=0
foreach($entry in $old.before){
    $target=$entry.target;$sha=Hash $target
    $saved=Join-Path $rollback ('{0:D2}-{1}' -f $index,(Split-Path -Leaf $target));$index++
    Copy-Item -LiteralPath $target -Destination $saved
    if((Hash $saved) -ne $sha){throw 'Backup verification failed'}
    $before+=@{target=$target;backup=$saved;sha256=$sha}
}
@{game_dir=$game;exe_sha256=$exeHash;source='346fac9327493ac6fffc2ebd2bdade07ad52e824';before=$before} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$rollback\manifest.json" -Encoding UTF8
Copy-Item -LiteralPath "$local\packages\four-branch-performance-20261031\rollback\Restore.ps1" -Destination "$rollback\Restore.ps1"
foreach($name in 'd3d9.dll','d3d9.pdb','d3d9.map'){Copy-Item -LiteralPath "$root\$name" -Destination "$candidate\$name"}
Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class D2vrAdaptiveIni {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
 public static extern bool WritePrivateProfileString(string section,string key,string value,string path);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
 public static extern uint GetPrivateProfileString(string section,string key,string fallback,System.Text.StringBuilder value,uint size,string path);
}
'@
$profiles=@("$game\Darkness2VR.ini","$local\Darkness2VR.ini")
$contracts=@{};foreach($profile in $profiles){$contracts[$profile]=Contract $profile}
Assert-Free
foreach($entry in $before){if((Hash $entry.target) -ne $entry.sha256){throw 'Installed state changed during backup'}}
try {
    Copy-Item -LiteralPath "$candidate\d3d9.dll" -Destination "$game\d3d9.dll" -Force
    foreach($profile in $profiles){
        if(![D2vrAdaptiveIni]::WritePrivateProfileString('Render','AdaptiveFPS','AUTO',$profile)){throw 'INI update failed'}
        $value=[Text.StringBuilder]::new(32)
        [void][D2vrAdaptiveIni]::GetPrivateProfileString('Render','AdaptiveFPS','',$value,32,$profile)
        if($value.ToString() -ne 'AUTO' -or (Contract $profile) -cne $contracts[$profile]){throw 'INI contract mismatch'}
    }
    if((Hash "$game\d3d9.dll") -ne $candidateHash -or (Hash "$game\DarknessII.exe") -ne $exeHash){throw 'Deployment identity mismatch'}
    foreach($entry in $before){if($entry.target -notin (@("$game\d3d9.dll")+$profiles) -and (Hash $entry.target) -ne $entry.sha256){throw 'Preserved profile/cache changed'}}
} catch {
    & "$rollback\Restore.ps1"
    throw
}
$after=@();foreach($entry in $before){$after+=@{target=$entry.target;sha256=(Hash $entry.target)}}
@{source='346fac9327493ac6fffc2ebd2bdade07ad52e824';dll_sha256=$candidateHash;exe_sha256=$exeHash;deployed_utc=[DateTime]::UtcNow.ToString('o');after=$after;rollback=$rollback;game_launched=$false;headset_validation='NOT RUN';AdaptiveFPS='AUTO'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$package\deployment.json" -Encoding UTF8
Write-Output "DEPLOYED verified DLL + AdaptiveFPS=AUTO; rollback: $rollback"
