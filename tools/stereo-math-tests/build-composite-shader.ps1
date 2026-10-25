param([string[]]$PrivateShaders = @())
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools unavailable' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$testExe = Join-Path $env:TEMP 'd2vr-composite-shader-test.exe'
$testObj = Join-Path $env:TEMP 'd2vr-composite-shader-test.obj'
Push-Location $here
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 composite_shader_test.cpp /Fo:`"$testObj`" /Fe:`"$testExe`" /link d3d9.lib user32.lib"
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
    & $testExe @PrivateShaders
    if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
} finally { Pop-Location }
