# Read-only deployment gate. Never loads the candidate DLL or starts the game.
param(
 [Parameter(Mandatory=$true)][string]$CandidateDll,
 [Parameter(Mandatory=$true)][string[]]$RenderIni,
 [Parameter(Mandatory=$true)][string]$DiagnosticIni
)
$ErrorActionPreference='Stop'
$bytes=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $CandidateDll).Path)
if($bytes.Length -lt 256 -or [BitConverter]::ToUInt16($bytes,0) -ne 0x5A4D){throw 'Candidate is not a PE'}
$pe=[BitConverter]::ToInt32($bytes,0x3c)
if($pe -lt 0 -or $pe -gt $bytes.Length-24 -or [BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550 -or
 [BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x14c -or !([BitConverter]::ToUInt16($bytes,$pe+22) -band 0x2000)){
 throw 'Candidate must be an x86 PE DLL'
}
$text=[Text.Encoding]::ASCII.GetString($bytes)
$protected=$text.Contains('Darkness2VR-build:protected-native-v1')
$standard=$text.Contains('Darkness2VR-build:standard-v1')
if($protected -eq $standard){throw 'Missing or ambiguous build flavor; rebuild with current build.ps1'}
if(!('D2vrProfileGate' -as [type])) {
 Add-Type -TypeDefinition @"
using System.Runtime.InteropServices;
public static class D2vrProfileGate {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]
 public static extern uint GetPrivateProfileInt(string section,string key,int fallback,string path);
}
"@
}
$required=[Collections.Generic.List[string]]::new()
if(Test-Path -LiteralPath $DiagnosticIni -PathType Leaf) {
 $diagnostic=(Resolve-Path -LiteralPath $DiagnosticIni).Path
 foreach($key in 'ImagePreservingPresent','ImagePreservingInput','ProtectedHeadTracking','ProtectedGestureActions',
  'ProtectedLeftAim','ProtectedLeftThrow','ProtectedTrackedHands','NativeCameraCover','MatchedFrameCapture',
  'PresentReadiness','CpuSyncTiming','ProductionResolution') {
  if([D2vrProfileGate]::GetPrivateProfileInt('Diagnostics',$key,0,$diagnostic)){$required.Add("Diagnostics/$key")}
 }
}
foreach($path in $RenderIni) {
 if(!(Test-Path -LiteralPath $path -PathType Leaf)){throw "Missing profile: $path"}
 $full=(Resolve-Path -LiteralPath $path).Path
 foreach($key in 'NativeViewBoundary','NativeOwnedConstantCopy','NativeOwnedMaterialCopy','QueryPreloadCount','DeferStereoTransferReader') {
  if([D2vrProfileGate]::GetPrivateProfileInt('Render',$key,0,$full)){$required.Add("Render/$key")}
 }
}
if($required.Count -and !$protected){throw ('Standard DLL omits required protected-native features: '+($required -join ', '))}
[pscustomobject]@{Flavor=$(if($protected){'protected-native'}else{'standard'});DLLSHA256=(Get-FileHash -LiteralPath $CandidateDll -Algorithm SHA256).Hash;Required=@($required)}
