$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools unavailable' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$exe = Join-Path $env:TEMP 'd2vr-motion-vector-test.exe'
$obj = Join-Path $env:TEMP 'd2vr-motion-vector-test.obj'
Push-Location $here
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 motion_vector_projection_test.cpp /Fo:`"$obj`" /Fe:`"$exe`""
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
} finally { Pop-Location }
