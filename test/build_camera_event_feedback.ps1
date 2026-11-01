param([string]$Replay='')
$ErrorActionPreference='Stop'
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'camera_event_feedback_test.cpp'
$out=Join-Path $env:TEMP 'd2vr-camera-event-feedback.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$source`" /Fo:`"$env:TEMP\d2vr-camera-event-feedback.obj`" /Fe:`"$out`""
if($LASTEXITCODE -ne 0){throw 'Camera event fixture build failed'}
if($Replay){& $out $Replay}else{& $out}
if($LASTEXITCODE -ne 0){throw 'Camera event fixture failed'}
