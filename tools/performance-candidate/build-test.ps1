param([string]$Archive)
$ErrorActionPreference='Stop'
$out=Join-Path $env:TEMP 'd2vr-performance-candidate-tests'
New-Item -ItemType Directory -Force $out | Out-Null
$source=Get-Content (Join-Path $PSScriptRoot '../../src/stereo_replay.inl') -Raw
$start=$source.IndexOf('static thread_local d2vr_prepare::Job* g_prepareCurrent')
$end=$source.IndexOf('// ------------------------------------------------------------- frame recording',$start)
if($start -lt 0 -or $end -lt 0){throw 'Actual production wrapper missing'}
[IO.File]::WriteAllText((Join-Path $out 'd2vr_prepare_wrapper.inl'),$source.Substring($start,$end-$start))
$start=$source.IndexOf('struct ReplayPreparationState {')
$end=$source.IndexOf('#include "replay_packet_prepare.inl"',$start)
if($start -lt 0 -or $end -lt 0){throw 'Actual preparation lifecycle missing'}
[IO.File]::WriteAllText((Join-Path $out 'd2vr_prepare_lifecycle.inl'),$source.Substring($start,$end-$start))
$start=$source.IndexOf('static bool ApplyShiftToUpload(')
$end=$source.IndexOf('// ------------------------------------------------------------- recording hooks',$start)
if($start -lt 0 -or $end -lt 0){throw 'Actual packet transform oracle missing'}
[IO.File]::WriteAllText((Join-Path $out 'd2vr_packet_oracle.inl'),$source.Substring($start,$end-$start))
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $out
try {
 foreach($test in @('worker','cache','packet')){
  cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /I`"$out`" `"$PSScriptRoot\${test}_test.cpp`" /Fe:${test}_test.exe"
  if($LASTEXITCODE -ne 0){throw "$test compile failed"}
  & ".\${test}_test.exe" $Archive
  if($LASTEXITCODE -ne 0){throw "$test regression failed"}
  if($test -eq 'packet'){
   cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 /DD2VR_MATCHED_CAPTURE=1 /I`"$out`" `"$PSScriptRoot\packet_test.cpp`" /Fe:packet_diagnostic_test.exe"
   if($LASTEXITCODE -ne 0){throw 'Diagnostic packet compile failed'}
   & .\packet_diagnostic_test.exe
   if($LASTEXITCODE -ne 0){throw 'Diagnostic packet regression failed'}
  }
 }
} finally {Pop-Location}
