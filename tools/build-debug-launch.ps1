$ErrorActionPreference='Stop'
$vswhere='C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC/Auxiliary/Build/vcvarsall.bat'
cmd /c "`"$vc`" x86 && cl /nologo /EHsc /std:c++17 /O2 /Fe:tools\debug-launch.exe tools\debug-launch.cpp"
if($LASTEXITCODE -ne 0) { throw 'debug observer build failed' }
