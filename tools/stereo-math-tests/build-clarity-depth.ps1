$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'Visual Studio C++ x86 tools were not found.' }
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
$tempRoot = [System.IO.Path]::GetTempPath()
$exe = Join-Path $tempRoot 'd2vr-clarity-depth-test.exe'
$obj = Join-Path $tempRoot 'd2vr-clarity-depth-test.obj'
Push-Location $here
try {
    $source = Join-Path $here 'clarity_depth_test.cpp'
    cmd /c "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /std:c++17 /Fo:`"$obj`" `"$source`" /Fe:`"$exe`""
    if ($LASTEXITCODE -ne 0) { throw 'compile failed' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'test failed' }
} finally { Pop-Location }
