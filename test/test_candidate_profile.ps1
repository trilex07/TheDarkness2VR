param(
 [Parameter(Mandatory=$true)][string]$StandardDll,
 [Parameter(Mandatory=$true)][string]$ProtectedDll
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$gate=Join-Path $root 'tools/assert-candidate-profile.ps1'
$out=Join-Path $env:TEMP ('d2vr-profile-gate-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out|Out-Null
$main=Join-Path $out 'main.ini';$diag=Join-Path $out 'diagnostic.ini'
$checks=0
function Check($condition){$script:checks++;if(!$condition){throw 'Profile gate assertion failed'}}
function Reject([scriptblock]$action){$failed=$false;try{& $action|Out-Null}catch{$failed=$true};Check $failed}
'[Render]'|Set-Content -LiteralPath $main
'[Diagnostics]'|Set-Content -LiteralPath $diag
$r=& $gate -CandidateDll $StandardDll -RenderIni @($main) -DiagnosticIni $diag
Check ($r.Flavor -eq 'standard' -and $r.Required.Count -eq 0)
Check ($r.DLLSHA256 -eq (Get-FileHash -LiteralPath $StandardDll).Hash)
$r=& $gate -CandidateDll $ProtectedDll -RenderIni @($main) -DiagnosticIni $diag
Check ($r.Flavor -eq 'protected-native')
foreach($key in 'ImagePreservingPresent','ImagePreservingInput','ProtectedHeadTracking','ProtectedGestureActions',
 'ProtectedLeftAim','ProtectedLeftThrow','ProtectedTrackedHands','NativeCameraCover','MatchedFrameCapture',
 'PresentReadiness','CpuSyncTiming','ProductionResolution') {
 "[Diagnostics]`r`n$key =1"|Set-Content -LiteralPath $diag
 Reject {& $gate -CandidateDll $StandardDll -RenderIni @($main) -DiagnosticIni $diag}
 $r=& $gate -CandidateDll $ProtectedDll -RenderIni @($main) -DiagnosticIni $diag
 Check ($r.Flavor -eq 'protected-native' -and $r.Required.Count -eq 1)
}
'[Diagnostics]'|Set-Content -LiteralPath $diag
foreach($key in 'NativeViewBoundary','NativeOwnedConstantCopy','NativeOwnedMaterialCopy','QueryPreloadCount','DeferStereoTransferReader') {
 "[render]`r`n$key =3"|Set-Content -LiteralPath $main
 Reject {& $gate -CandidateDll $StandardDll -RenderIni @($main) -DiagnosticIni $diag}
 $r=& $gate -CandidateDll $ProtectedDll -RenderIni @($main) -DiagnosticIni $diag
 Check ($r.Flavor -eq 'protected-native' -and $r.Required.Count -eq 1)
}
# A local override also requires the proper binary, even if root INI is clean.
$override=Join-Path $out 'local.ini';Copy-Item -LiteralPath $main -Destination $override
'[Render]'|Set-Content -LiteralPath $main
Reject {& $gate -CandidateDll $StandardDll -RenderIni @($main,$override) -DiagnosticIni $diag}
Reject {& $gate -CandidateDll $ProtectedDll -RenderIni @($main+'.absent') -DiagnosticIni $diag}
# Real PE derivatives are rejected without executing DllMain.
$bad=Join-Path $out 'invalid.dll'
$bytes=[IO.File]::ReadAllBytes($StandardDll);$pe=[BitConverter]::ToInt32($bytes,0x3c)
$bytes[$pe+4]=0x64;$bytes[$pe+5]=0x86;[IO.File]::WriteAllBytes($bad,$bytes)
Reject {& $gate -CandidateDll $bad -RenderIni @($main) -DiagnosticIni $diag}
[IO.File]::WriteAllBytes($bad,[byte[]]@(0x4D,0x5A))
Reject {& $gate -CandidateDll $bad -RenderIni @($main) -DiagnosticIni $diag}
# Reproduce the actual wrong-build install via the existing entrypoint.
$mockRepo=Join-Path $out 'repo';$mockGame=Join-Path $out 'game'
New-Item -ItemType Directory -Path $mockRepo,$mockGame|Out-Null
Copy-Item -LiteralPath $StandardDll -Destination (Join-Path $mockRepo 'd3d9.dll')
Copy-Item -LiteralPath $override -Destination (Join-Path $mockRepo 'Darkness2VR.ini')
Copy-Item -LiteralPath $ProtectedDll -Destination (Join-Path $mockGame 'd3d9.dll')
Copy-Item -LiteralPath $main -Destination (Join-Path $mockGame 'Darkness2VR.ini')
Copy-Item -LiteralPath $diag -Destination (Join-Path $mockGame 'Darkness2VR-diagnostic.ini')
$before=Get-FileHash -LiteralPath (Join-Path $mockGame 'd3d9.dll')
Reject {& (Join-Path $root 'tools/deploy-research-build.ps1') -Install -DllOnly -RepoRoot $mockRepo -GameDir $mockGame}
Check ((Get-FileHash -LiteralPath (Join-Path $mockGame 'd3d9.dll')).Hash -eq $before.Hash)
Check (@(Get-ChildItem -LiteralPath $mockGame -File).Count -eq 3)
"PASS $checks profile/build-flavor checks; real x86 DLLs read without loading; wrong-flavor install refused before target writes; fixtures=$out"
