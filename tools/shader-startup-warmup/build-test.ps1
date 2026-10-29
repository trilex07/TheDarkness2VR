param([string]$Package)
$ErrorActionPreference='Stop'
$here=$PSScriptRoot
$out=Join-Path $env:TEMP 'd2vr-shader-startup-tests'
New-Item -ItemType Directory -Force $out | Out-Null
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vc=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $out
try {
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$here\test.cpp`" /Fe:test.exe"
 if($LASTEXITCODE -ne 0){throw 'compile failed'}
 if($Package){& .\test.exe $Package}else{& .\test.exe}
 if($LASTEXITCODE -ne 0){throw 'test failed'}
 cmd /c "`"$vc`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 `"$here\demand_test.cpp`" /Fe:demand_test.exe"
 if($LASTEXITCODE -ne 0){throw 'demand compile failed'}
 & .\demand_test.exe
 if($LASTEXITCODE -ne 0){throw 'demand test failed'}
} finally {Pop-Location}
