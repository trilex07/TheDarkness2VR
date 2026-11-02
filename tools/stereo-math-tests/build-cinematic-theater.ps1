$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$vswhere="${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
Push-Location $root
try {
 New-Item -ItemType Directory -Force build | Out-Null
 cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 tools\stereo-math-tests\cinematic_theater_test.cpp /Fo:build\cinematic-test.obj /Fe:build\cinematic-test.exe"
 if($LASTEXITCODE -ne 0){throw 'Compile failed'}
 & .\build\cinematic-test.exe
 if($LASTEXITCODE -ne 0){throw 'Regression failed'}
} finally {Pop-Location}
