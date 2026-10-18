# Builds and runs the stereo geometry test harness (x86, real D3D9 device).
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $here

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"

Push-Location $here
try {
    $cmd = "`"$vcvars`" x86 && cl /nologo /EHsc /W3 /O2 /MD /std:c++17 stereo_test.cpp /link d3d9.lib d3dcompiler.lib user32.lib /Fe:stereo_test.exe"
    cmd /c $cmd
    if ($LASTEXITCODE -ne 0) { throw "test build failed" }
    Write-Host "--- running stereo_test.exe ---"
    & ".\stereo_test.exe"
    Write-Host "--- exit: $LASTEXITCODE ---"
} finally {
    Pop-Location
}
