$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ tools unavailable' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$output = Join-Path $env:TEMP 'd2vr-exception-witness-test.exe'
$obj = Join-Path $env:TEMP 'd2vr-exception-witness-test.obj'
Push-Location $here
try {
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W4 /O2 /MD /std:c++17 exception_record_witness_test.cpp /Fo:`"$obj`" /Fe:`"$output`""
    if ($LASTEXITCODE -ne 0) { throw 'build failed' }
    & $output
    if ($LASTEXITCODE -ne 0) { throw 'tests failed' }
} finally { Pop-Location }
