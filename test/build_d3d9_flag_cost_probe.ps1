$ErrorActionPreference='Stop'
if(Get-Process DarknessII -ErrorAction SilentlyContinue){throw 'Game active; probe deferred'}
$vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'd3d9_flag_cost_probe.cpp'
$exe=Join-Path $env:TEMP 'd2vr-d3d9-flag-cost.exe'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /WX /O2 /MD /std:c++17 `"$source`" /Fo:`"$env:TEMP\d2vr-d3d9-flag-cost.obj`" /Fe:`"$exe`" /link user32.lib"
if($LASTEXITCODE -ne 0){throw 'Probe build failed'}
& $exe | Tee-Object -FilePath (Join-Path $env:TEMP 'd2vr-d3d9-flag-cost.txt')
if($LASTEXITCODE -ne 0){throw 'Probe failed'}
