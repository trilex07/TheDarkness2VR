$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$fxc=Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x86\fxc.exe"|Sort-Object FullName|Select-Object -Last 1 -ExpandProperty FullName
if(!$fxc){throw 'Windows SDK fxc missing'}
& $fxc /nologo /WX /O3 /T cs_5_0 /E main /Vn kGazeFxaaCS /Fh (Join-Path $root 'src/gaze_fxaa_cs.h') (Join-Path $root 'src/gaze_fxaa.hlsl')
if($LASTEXITCODE -ne 0){throw 'FXAA shader compile failed'}
