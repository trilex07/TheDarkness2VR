param([switch]$Run,[string]$Package)
$ErrorActionPreference='Stop'
$here=$PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-shader-driver-probe'
New-Item -ItemType Directory -Force $out | Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $out
try {
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$here\driver_probe.cpp`" /Fe:probe.exe /link user32.lib"
 if($LASTEXITCODE -ne 0){throw 'compile failed'}
 if($Run){
  if(-not $Package){throw 'Explicit private package required'}
  if(Get-Process DarknessII,Singularity -ErrorAction SilentlyContinue){throw 'Known game active; do not run GPU probe'}
  & .\probe.exe $Package
  if($LASTEXITCODE -ne 0){throw 'driver probe failed'}
 }
}finally{Pop-Location}
