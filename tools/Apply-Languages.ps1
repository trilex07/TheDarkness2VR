param([Parameter(Mandatory=$true)][int]$OwnerPid,
 [Parameter(Mandatory=$true)][UInt64]$OwnerFileTime,
 [Parameter(Mandatory=$true)][string]$IniPath)
$ErrorActionPreference='Stop'
# Retain the process object; never wait on a recycled numeric PID.
Add-Type @'
using System;using System.Runtime.InteropServices;
public static class VrLanguageIni {
 [DllImport("kernel32",CharSet=CharSet.Unicode)] public static extern uint GetPrivateProfileInt(string s,string k,int d,string p);
 [DllImport("kernel32",CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string s,string k,string v,string p);
}
'@
$root=[IO.Path]::GetFullPath($PSScriptRoot)
$game=Split-Path $root -Parent
$cache=Join-Path $game 'Cache.Windows'
$mutex=New-Object Threading.Mutex($false,'Local\Darkness2VR-Language-67370')
$locked=$false;$handles=@{};$backups=@{};$changed=@();$state=Join-Path $root 'language-state.json'
function Hash([string]$p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Closed {if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'A game process is active'}}
try {
 $owner=Get-Process -Id $OwnerPid -ErrorAction Stop
 if($owner.Path -ne (Join-Path $game 'DarknessII.exe') -or [UInt64]$owner.StartTime.ToUniversalTime().ToFileTimeUtc() -ne $OwnerFileTime){throw 'Owner identity mismatch'}
 $owner.WaitForExit()
 $locked=$mutex.WaitOne(0);if(!$locked){throw 'Another language transaction owns the archives'}
 Closed
 if(!(Test-Path -LiteralPath $IniPath) -or [IO.Path]::GetFullPath($IniPath) -ne $IniPath){throw 'INI identity missing'}
 $voice=if([VrLanguageIni]::GetPrivateProfileInt('VRSettings','PolishVoice',1,$IniPath)){'pl'}else{'en'}
 $text=if([VrLanguageIni]::GetPrivateProfileInt('VRSettings','PolishText',1,$IniPath)){'pl'}else{'en'}
 $m=Get-Content (Join-Path $root 'languages.json') -Raw | ConvertFrom-Json
 $names=@('H.Misc.cache','H.Misc.toc','H.Misc_en.cache','H.Misc_en.toc','B.Misc_en.cache','B.Misc_en.toc','F.Misc_en.cache','F.Misc_en.toc')
 $selected=@{}
 foreach($n in $names){
  $lang=if($n -eq 'H.Misc.cache' -or $n -eq 'H.Misc.toc'){$text}else{$voice}
  $profile=$m.$lang;$h=$profile.$n
  if($h -notmatch '^[0-9a-f]{64}$'){throw 'Manifest set/hash invalid'}
  $source=Join-Path $root "languages/$lang/$n";if((Hash $source) -ne $h){throw "Profile changed: $n"}
  $current=Hash (Join-Path $cache $n)
  if($current -ne $m.en.$n -and $current -ne $m.pl.$n){throw "Unrecognized installed archive: $n"}
  $selected[$n]=@{source=$source;hash=$h;before=$current}
 }
 $backup=Join-Path $root ('transactions/'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffffff'))
 New-Item -ItemType Directory -Path $backup | Out-Null
 # All eight exclusive handles are acquired before any write. A new game
 # cannot read mixed cache/TOC pairs while these handles are held.
 foreach($n in $names){$handles[$n]=[IO.File]::Open((Join-Path $cache $n),[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)}
 Closed
 foreach($n in $names){
  $dest=Join-Path $backup $n;$out=[IO.File]::Create($dest)
  try{$handles[$n].CopyTo($out);$handles[$n].Position=0}finally{$out.Dispose()}
  if((Hash $dest) -ne $selected[$n].before){throw 'Preimage changed before exclusive ownership'}
  $backups[$n]=$dest
 }
 @{status='PREPARED';before=$selected;voice=$voice;text=$text} | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $backup 'receipt.json')
 foreach($n in $names){
  $changed+=$n;$source=[IO.File]::OpenRead($selected[$n].source)
  try{$handles[$n].Position=0;$source.CopyTo($handles[$n]);$handles[$n].SetLength($source.Length);$handles[$n].Flush($true)}finally{$source.Dispose()}
  $handles[$n].Position=0;$sha=[Security.Cryptography.SHA256]::Create()
  try{$got=[BitConverter]::ToString($sha.ComputeHash($handles[$n])).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
  if($got -ne $selected[$n].hash){throw 'Installed readback mismatch'}
 }
 [void][VrLanguageIni]::WritePrivateProfileString('VRSettings','AppliedPolishVoice',$(if($voice -eq 'pl'){'1'}else{'0'}),$IniPath)
 [void][VrLanguageIni]::WritePrivateProfileString('VRSettings','AppliedPolishText',$(if($text -eq 'pl'){'1'}else{'0'}),$IniPath)
 [void][VrLanguageIni]::WritePrivateProfileString('VRSettings','LanguageError','0',$IniPath)
 @{status='APPLIED';voice=$voice;text=$text;backup=$backup} | ConvertTo-Json | Set-Content $state
} catch {
 $reason=$_.Exception.Message
 foreach($n in $changed){if($backups.ContainsKey($n)){
  $source=[IO.File]::OpenRead($backups[$n]);try{$handles[$n].Position=0;$source.CopyTo($handles[$n]);$handles[$n].SetLength($source.Length);$handles[$n].Flush($true)}finally{$source.Dispose()}
 }}
 [void][VrLanguageIni]::WritePrivateProfileString('VRSettings','LanguageError','1',$IniPath)
 @{status='FAILED';reason=$reason} | ConvertTo-Json | Set-Content $state
 exit 1
} finally {
 foreach($h in $handles.Values){$h.Dispose()}
 if($locked){$mutex.ReleaseMutex()};$mutex.Dispose()
}
